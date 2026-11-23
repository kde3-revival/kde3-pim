/*  -*- c++ -*-
    webkitparthtmlwriter.cpp

    This file is part of KMail, the KDE mail client.
    Copyright (c) 2026 The kde3-revival authors


    KMail is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    KMail is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config.h>

#include "webkitparthtmlwriter.h"

#include <kdebug.h>
#include <kglobal.h>
#include <kmdcodec.h>
#include <kmimetype.h>
#include <kparts/part.h>
#include <kstandarddirs.h>
#include <kurl.h>

#include <qapplication.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qregexp.h>
#include <qtextstream.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static long long nowMs()
{
  struct timeval tv;
  gettimeofday( &tv, 0 );
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static bool writeFile( const QString & path, const QString & text )
{
  QFile f( path );
  if ( !f.open( IO_WriteOnly | IO_Truncate ) )
    return false;
  QCString utf8 = text.utf8();
  bool ok = f.writeBlock( utf8.data(), utf8.length() ) == (int)utf8.length();
  f.close();
  return ok;
}

static void removeDirectory( const QString & dir )
{
  QDir d( dir );
  QStringList files = d.entryList( QDir::Files | QDir::Hidden | QDir::System );
  for ( QStringList::ConstIterator it = files.begin(); it != files.end(); ++it )
    ::unlink( QFile::encodeName( dir + "/" + *it ).data() );
  ::rmdir( QFile::encodeName( dir ).data() );
}

// A regular file in KDE's directory of temporary files (KMail's
// KTempFile files), not one of our message directories.
static bool isTemporaryFile( const QString & path )
{
  static QString tmp;
  if ( tmp.isEmpty() ) {
    char * real = realpath( QFile::encodeName( KGlobal::dirs()->saveLocation( "tmp" ) ).data(), 0 );
    if ( !real )
      return false;
    tmp = QFile::decodeName( real ) + "/";
    free( real );
  }
  char * real = realpath( QFile::encodeName( path ).data(), 0 );
  if ( !real )
    return false;
  QString file = QFile::decodeName( real );
  free( real );
  struct stat st;
  return file.startsWith( tmp ) && !file.mid( tmp.length() ).startsWith( "kmail-webkit-" )
         && ::stat( QFile::encodeName( file ).data(), &st ) == 0 && S_ISREG( st.st_mode );
}

namespace KMail {

  WebKitPartHtmlWriter::WebKitPartHtmlWriter( KParts::ReadOnlyPart * part, QObject * parent, const char * name )
    : QObject( parent, name ), HtmlWriter(), mPart( part ), mBlank( false ),
      mLoadTimer( 0, "mLoadTimer" ), mSerial( 0 ),
      mCovered( false ), mBlankLoading( false ), mDeferredLoad( false ),
      mUncoverTimer( 0, "mUncoverTimer" ), mBlankTimer( 0, "mBlankTimer" ),
      mStats( getenv( "KMAIL_WEBKIT_STATS" ) != 0 ), mEndTime( 0 )
  {
    connect( &mLoadTimer, SIGNAL(timeout()), SLOT(slotLoad()) );
    connect( &mUncoverTimer, SIGNAL(timeout()), SLOT(slotUncover()) );
    connect( &mBlankTimer, SIGNAL(timeout()), SLOT(slotBlankDone()) );
    if ( part )
      connect( part, SIGNAL(completed()), SLOT(slotCompleted()) );
  }

  WebKitPartHtmlWriter::~WebKitPartHtmlWriter() {
    for ( QStringList::ConstIterator it = mDirs.begin(); it != mDirs.end(); ++it )
      removeDirectory( *it );
    if ( !mRoot.isEmpty() )
      ::rmdir( QFile::encodeName( mRoot ).data() );
  }

  void WebKitPartHtmlWriter::begin( const QString & css ) {
    mCss = css;
    mHtml = QString::null;
    mBodies.clear();
    mAttachmentList = QString::null;
    mBlank = false;
    forgetLinks();
  }

  void WebKitPartHtmlWriter::end() {
    // Shown when the event loop runs, unless another document replaces
    // this one first.
    mPendingCss = mCss;
    mPendingHtml = mHtml;
    mPendingBodies = mBodies;
    mPendingAttachmentList = mAttachmentList;
    mHtml = QString::null;
    mBodies.clear();
    mEndTime = nowMs();
    mLoadTimer.start( mBlank ? 400 : 0, true );
    mBlank = false;
  }

  void WebKitPartHtmlWriter::reset() {
    mHtml = QString::null;
    mBodies.clear();
    forgetLinks();
  }

  // The links of the document shown are valid until KMail writes the
  // next one (its part ids are those of the message being shown).
  void WebKitPartHtmlWriter::forgetLinks() {
    mLinks.clear();
    mLinkSecret = QString::null;
    // A document still waiting for its load (the previous one) is never
    // shown now: its links would be minted against the next message's
    // parts (an event loop can run while KMail writes it).
    mLoadTimer.stop();
    mPendingHtml = QString::null;
    mPendingBodies.clear();
    mPendingAttachmentList = QString::null;
  }

  // 128 bits of the system's random source, hexadecimal; QString::null if
  // there is none.
  static QString randomSecret() {
    QFile f( "/dev/urandom" );
    if ( !f.open( IO_ReadOnly | IO_Raw ) )
      return QString::null;
    unsigned char bytes[16];
    if ( f.readBlock( (char *)bytes, sizeof bytes ) != (Q_LONG)sizeof bytes )
      return QString::null;
    QString hex;
    for ( unsigned i = 0; i < sizeof bytes; ++i )
      hex += QString().sprintf( "%02x", bytes[i] );
    return hex;
  }

  void WebKitPartHtmlWriter::write( const QString & str ) {
    mHtml += str;
  }

  void WebKitPartHtmlWriter::queue( const QString & str ) {
    mHtml += str;
  }

  void WebKitPartHtmlWriter::flush() {
    end();
  }

  // cid: URLs are resolved by ObjectTreeParser (embedRelatedResources():
  // data: URLs), within the scope of the HTML part: no document-wide map.
  void WebKitPartHtmlWriter::embedPart( const QCString &, const QString & ) {
  }


  void WebKitPartHtmlWriter::queueIsolatedHtml( const QString & html ) {
    // The frame starts 1 pixel high; the part's script gives it the height
    // of its document.  The sandbox: no scripts, no forms, no plugins; it
    // is same-origin with KMail's document only so that the part can
    // measure it (the part's response headers sandbox it too).
    mHtml += QString::fromLatin1( "<iframe class=\"kmail-htmlpart\" src=\"body-%1.html\" "
                                  "sandbox=\"allow-same-origin allow-popups allow-popups-to-escape-sandbox "
                                  "allow-top-navigation-by-user-activation\" "
                                  "style=\"display: block; width: 100%; height: 1px; border: 0; margin: 0;\">"
                                  "</iframe>" ).arg( mBodies.count() );
    mBodies.append( html );
  }

  QString WebKitPartHtmlWriter::messageDirectory() {
    if ( mRoot.isEmpty() ) {
      QCString templ = QFile::encodeName( locateLocal( "tmp", "kmail-webkit-XXXXXX" ) );
      if ( !mkdtemp( templ.data() ) ) {   // 0700
        kdWarning( 5006 ) << "WebKitPartHtmlWriter: cannot create " << templ << ": " << strerror( errno ) << endl;
        return QString::null;
      }
      mRoot = QFile::decodeName( templ );
    }
    QString dir = mRoot + "/" + QString::number( ++mSerial );
    if ( ::mkdir( QFile::encodeName( dir ).data(), 0700 ) != 0 )
      return QString::null;
    mDirs.append( dir );
    // The message shown and the one loading: older ones go.
    while ( mDirs.count() > 3 ) {
      removeDirectory( mDirs.first() );
      mDirs.remove( mDirs.begin() );
    }
    return dir;
  }

  // An image of KMail's document (an inline image, an attachment shown
  // inline, a contact's photo): a link (or a copy) in the message's
  // directory, which is all the part serves.
  QString WebKitPartHtmlWriter::linkIntoMessage( const QString & dir, const QString & path, int index ) {
    QString name = QFileInfo( path ).extension( false ).lower();
    name = QString::fromLatin1( "file-%1" ).arg( index ) + ( name.isEmpty() || name.contains( QRegExp( "[^a-z0-9]" ) ) ? QString::null : "." + name );
    QString target = dir + "/" + name;
    if ( ::link( QFile::encodeName( path ).data(), QFile::encodeName( target ).data() ) != 0 ) {
      QFile in( path ), out( target );
      if ( !in.open( IO_ReadOnly ) || !out.open( IO_WriteOnly ) )
        return QString::null;
      QByteArray data = in.readAll();
      out.writeBlock( data );
    }
    return target;
  }

  void WebKitPartHtmlWriter::slotLoad() {
    if ( !mPart )
      return;
    if ( mBlankLoading ) {
      mDeferredLoad = true;   // after the blank page (slotBlankDone())
      return;
    }
    long long start = nowMs();
    QString dir = messageDirectory();
    if ( dir.isEmpty() )
      return;

    for ( unsigned i = 0; i < mPendingBodies.count(); ++i )
      writeFile( dir + QString::fromLatin1( "/body-%1.html" ).arg( i ), mPendingBodies[i] );

    QString html = mPendingHtml;
    if ( !mPendingAttachmentList.isEmpty() )
      html.replace( "<div id=\"attachmentInjectionPoint\"></div>",
                    "<div id=\"attachmentInjectionPoint\">" + mPendingAttachmentList + "</div>" );
    // src="file:..." of KMail's document: KMail's temporary files (inline
    // images, attachments shown as images) into the directory; anything
    // else becomes a path the part serves or refuses (KDE's icons are
    // served).  No file of the user's gets into the message: the message's
    // own HTML (also an HTML attachment viewed) is never in this document,
    // it is a document of its own (queueIsolatedHtml()).
    QRegExp fileSrc( "src=\"file:([^\"]*)\"" );
    int index = 0;
    for ( int pos = fileSrc.search( html ); pos >= 0; pos = fileSrc.search( html, pos ) ) {
      QString path = KURL::decode_string( fileSrc.cap( 1 ) );
      QString target = isTemporaryFile( path ) ? linkIntoMessage( dir, path, index++ ) : path;
      QString replacement = "src=\"" + KURL::encode_string( target ) + "\"";
      html.replace( pos, fileSrc.matchedLength(), replacement );
      pos += replacement.length();
    }

    // href="file:..." of KMail's document (an attachment's temporary file:
    // a part icon, an image shown inline): WebKit does not navigate to
    // file: from our scheme, so the click never reached KMail; KHTML gave
    // it to KMail, which opens the attachment (or asks, per the user's
    // setting).  They become links of a scheme of their own, which the
    // part gives to KMail on a click, and KMail turns back into the file:
    // URL (originalURL()).  With a secret of this document (128 random
    // bits): a message cannot name them.  Every message-derived string of
    // KMail's document is quoted (labels, descriptions, header fields): the
    // only href attributes are KMail's own.
    forgetLinks();
    mLinkSecret = randomSecret();
    QRegExp link( "(\\s)href=\"(file:[^\"]*)\"" );
    for ( int pos = mLinkSecret.isEmpty() ? -1 : link.search( html ); pos >= 0; pos = link.search( html, pos ) ) {
      const QString key = QString::fromLatin1( "kmail-link:%1-%2" ).arg( mLinkSecret ).arg( mLinks.count() );
      mLinks[key] = link.cap( 2 );
      const QString replacement = link.cap( 1 ) + "href=\"" + key + "\"";
      html.replace( pos, link.matchedLength(), replacement );
      pos += replacement.length();
    }

    QString index_html = dir + "/index.html";
    // KMail's style sheet (KHTML's user style sheet) in the head of
    // KMail's document only.
    // KMail's document is in quirks mode (its HTML 4.01 doctype has no
    // system identifier).  Quirks mode resets the font size, weight, line
    // height and white-space of tables (WebKit's and KHTML's quirks style
    // sheets both do); the fancy header's fields and the attachment table
    // are table cells, the subject is not.  With WebKit the fields were
    // reported larger than the subject: tables inherit them here, as in
    // standards mode; KMail's own rules come after.
    QString style = "<style type=\"text/css\">\n"
                    "table { font-size: inherit; font-weight: inherit; font-style: inherit; "
                    "font-variant: inherit; line-height: inherit; white-space: inherit; }\n"
                    + mPendingCss + "</style>\n";
    int head = html.find( "<head>", 0, false );
    if ( head >= 0 )
      html.insert( head + 6, style );
    else
      html.prepend( style );
    writeFile( index_html, html );
    mPendingHtml = QString::null;
    mPendingBodies.clear();
    if ( mStats )
      fprintf( stderr, "kmail-webkit: document %u: end() -> files written %lld ms (%lld ms writing)\n",
               mSerial, nowMs() - mEndTime, nowMs() - start );
    mPart->openURL( KURL::fromPathOrURL( index_html ) );
  }

  QString WebKitPartHtmlWriter::originalURL( const QString & url ) const {
    QMap<QString, QString>::ConstIterator it = mLinks.find( url );
    return it == mLinks.end() ? QString::null : it.data();
  }

  void WebKitPartHtmlWriter::slotCompleted() {
    if ( mStats )
      fprintf( stderr, "kmail-webkit: document %u: end() -> loaded %lld ms\n", mSerial, nowMs() - mEndTime );
    if ( mBlankLoading )
      slotBlankDone();
  }

  // The blank page of a message switch is loaded (or did not come in
  // time): the next message's document, if it is ready already, is
  // loaded; WebKit keeps showing the blank page until that one is ready,
  // never the previous message again.  The cover goes once WebKit has
  // presented the blank page, which it does not say: 250 ms (measured,
  // four KMails at once: up to ~110 ms; 60 ms let the previous message
  // through for a frame).
  void WebKitPartHtmlWriter::slotBlankDone() {
    if ( !mBlankLoading )
      return;
    mBlankLoading = false;
    mBlankTimer.stop();
    mUncoverTimer.start( 250, true );
    if ( mDeferredLoad ) {
      mDeferredLoad = false;
      mLoadTimer.start( 0, true );
    }
  }

  void WebKitPartHtmlWriter::clearView( const QColor & background ) {
    if ( !mPart || !mPart->widget() )
      return;
    QWidget * view = mPart->widget();
    if ( !mCover ) {
      mCover = new QWidget( view, "kmail-webkit-cover", WRepaintNoErase );
      mCover->setFocusPolicy( QWidget::NoFocus );
      view->installEventFilter( this );
    }
    mUncoverTimer.stop();
    // A document of the previous message not loaded yet: never.
    mLoadTimer.stop();
    mPendingHtml = QString::null;
    mPendingBodies.clear();
    // The X server fills the window with its background colour when it
    // is mapped: blank at once, before KMail prepares the next message.
    mCover->setPaletteBackgroundColor( background );
    mCover->setGeometry( view->rect() );
    mCover->raise();
    mCover->show();
    mCovered = true;
    QApplication::flushX();
    if ( mStats )
      fprintf( stderr, "kmail-webkit: cleared\n" );

    // The part keeps the previous document until the next one is ready
    // (laid out, its frames sized, painted), which takes a while for a
    // large message: the cover cannot know when that is.  A blank page
    // (the reader's background) replaces the previous message in the
    // part first; the cover goes once it is shown, and the next message
    // replaces the blank page whenever it is ready.  Its document waits
    // for the blank page (slotLoad()).
    mDeferredLoad = false;
    if ( mBlankLoading )
      return;   // a blank page is on its way already (KMail selects twice)
    const QString dir = messageDirectory();
    // WebKit replaces a page only by one that is "visually non-empty":
    // text in the background colour makes it so (an empty page left the
    // previous message on screen).
    if ( dir.isEmpty() || !writeFile( dir + "/index.html", QString::fromLatin1(
           "<html><head><style>html, body { background: %1; color: %1; margin: 0; overflow: hidden; }"
           "p { margin: 0; -webkit-user-select: none; }</style></head><body><p>%2</p></body></html>" )
           .arg( background.name() ).arg( QString().fill( 'x', 400 ) ) ) ) {
      mBlankLoading = false;
      mUncoverTimer.start( 250, true );
      return;
    }
    mBlankLoading = true;
    mBlankTimer.start( 1000, true );   // shown or not: go on
    mPart->openURL( KURL::fromPathOrURL( dir + "/index.html" ) );
  }

  void WebKitPartHtmlWriter::slotUncover() {
    if ( mCover )
      mCover->hide();
    mCovered = false;
    if ( mStats )
      fprintf( stderr, "kmail-webkit: document %u: end() -> uncovered %lld ms\n", mSerial, nowMs() - mEndTime );
  }

  bool WebKitPartHtmlWriter::eventFilter( QObject * o, QEvent * e ) {
    if ( mCover && mPart && o == mPart->widget() && e->type() == QEvent::Resize )
      mCover->setGeometry( mPart->widget()->rect() );
    return false;
  }

} // namespace KMail

#include "webkitparthtmlwriter.moc"
