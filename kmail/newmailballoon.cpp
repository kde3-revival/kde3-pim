/*
    This file is part of KMail.

    A balloon at the system tray icon for new mail.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "newmailballoon.h"

#include "accountmanager.h"
#include "globalsettings.h"
#include "kmfolder.h"
#include "kmfoldermgr.h"
#include "kmfoldertree.h"
#include "folderstorage.h"
#include "kmheaders.h"
#include "kmkernel.h"
#include "kmmainwidget.h"
#include "kmmessage.h"
#include "kmmsgbase.h"
#include "kmmsgdict.h"
#include "kmmsgpart.h"
#include "kmsystemtray.h"

#include <libemailfunctions/email.h>

#include <kabc/addresseelist.h>
#include <kabc/stdaddressbook.h>

#include <mimelib/body.h>
#include <mimelib/bodypart.h>
#include <mimelib/enum.h>
#include <mimelib/headers.h>
#include <mimelib/message.h>

#include <dcopclient.h>
#include <kapplication.h>
#include <kballoon.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <kinstance.h>
#include <klocale.h>
#include <knotifyclient.h>
#include <kwin.h>

#include <qapplication.h>
#include <qtimer.h>

#include <time.h>

using namespace KMail;

namespace {

const char * const notifyEvent = "new-mail-arrived";

// The direction controls of Unicode: those of the text would reach the
// punctuation and the text around it in the balloon.
bool isBidiControl( ushort c )
{
  return c == 0x200e || c == 0x200f || c == 0x061c ||
         ( c >= 0x202a && c <= 0x202e ) || ( c >= 0x2066 && c <= 0x2069 );
}

// Neither control characters (but line breaks with @p lines) nor
// direction controls; other line and paragraph separators are breaks.
QString clean( const QString & text, bool lines = false )
{
  QString result;
  result.reserve( text.length() );
  for ( uint i = 0; i < text.length(); ++i ) {
    const QChar c = text[i];
    const ushort u = c.unicode();
    if ( u == '\n' || u == 0x2028 || u == 0x2029 || u == 0x85 ) {
      result += lines ? QChar( '\n' ) : QChar( ' ' );
      continue;
    }
    if ( u == '\t' ) {
      result += QChar( ' ' );
      continue;
    }
    if ( c.category() == QChar::Other_Control || isBidiControl( u ) || u == 0xfeff )
      continue;
    result += c;
  }
  return result;
}

// Whether text can be cut before @p i: not inside a surrogate pair, not
// before a combining mark, a variation selector, an emoji modifier or a
// joiner, not after a joiner.
bool isBoundary( const QString & text, uint i )
{
  if ( i == 0 || i >= text.length() )
    return true;
  const ushort c = text[i].unicode();
  const ushort before = text[i - 1].unicode();
  if ( c >= 0xdc00 && c <= 0xdfff )
    return false;
  if ( c == 0x200d || before == 0x200d )
    return false;
  if ( ( c >= 0xfe00 && c <= 0xfe0f ) || ( c >= 0x20d0 && c <= 0x20ff ) )
    return false;
  if ( c == 0xd83c && i + 1 < text.length() &&
       text[i + 1].unicode() >= 0xdffb && text[i + 1].unicode() <= 0xdfff )
    return false;   // U+1F3FB..U+1F3FF, the skin tones
  if ( c == 0xdb40 )
    return false;   // the tags of a flag (U+E0020..U+E007F)
  const QChar::Category category = text[i].category();
  return category != QChar::Mark_NonSpacing && category != QChar::Mark_SpacingCombining &&
         category != QChar::Mark_Enclosing;
}

// The first @p length characters of @p text at most, cut where a
// character ends.
QString left( const QString & text, uint length )
{
  if ( text.length() <= length )
    return text;
  uint n = length;
  while ( n > 0 && !isBoundary( text, n ) )
    --n;
  return text.left( n );
}

// Words longer than this are cut: the balloon wraps lines between words.
const uint maxWord = 30;

// One line, at most @p length characters (" ..." then), no word longer
// than maxWord characters ("..." then).
QString squash( const QString & text, uint length )
{
  const QString line = clean( text ).simplifyWhiteSpace();
  QString result;
  int start = 0;
  while ( start < (int)line.length() ) {
    int end = line.find( ' ', start );
    if ( end < 0 )
      end = line.length();
    const QString word = line.mid( start, end - start );
    if ( !result.isEmpty() )
      result += ' ';
    result += word.length() > maxWord ? left( word, maxWord ) + QString::fromLatin1( "..." ) : word;
    if ( result.length() > length )
      break;
    start = end + 1;
  }
  if ( result.length() <= length )
    return result;
  return left( result, length ) + QString::fromLatin1( " ..." );
}

// Text from the message in the balloon's sentence: embedded in its own
// direction (its first strong character) and closed with the mark of
// ours, so it reorders only itself.
QString isolated( const QString & text )
{
  bool rightToLeft = false;
  for ( uint i = 0; i < text.length(); ++i ) {
    const QChar::Direction direction = text[i].direction();
    if ( direction == QChar::DirL )
      break;
    if ( direction == QChar::DirR || direction == QChar::DirAL ) {
      rightToLeft = true;
      break;
    }
  }
  return QChar( rightToLeft ? 0x202b : 0x202a ) + text + QChar( 0x202c ) +
         QChar( QApplication::reverseLayout() ? 0x200f : 0x200e );
}

// Larger text/plain parts are not decoded for the start of their text.
const uint maxPreviewSource = 256 * 1024;

// Whether @p headers say "Content-Disposition: attachment".
bool isAttachment( DwHeaders & headers )
{
  return headers.HasContentDisposition() &&
         headers.ContentDisposition().DispositionType() == DwMime::kDispTypeAttachment;
}

// The first text/plain part among @p part and the parts after it,
// looking into multiparts (@p depth levels at most), never into an
// attachment (a multipart one neither) or an attached message.
DwBodyPart * plainPart( DwBodyPart * part, int depth )
{
  for ( ; part; part = part->Next() ) {
    if ( !part->hasHeaders() || !part->Headers().HasContentType() )
      continue;   // (text/plain by default, but in a digest: no guess)
    DwHeaders & headers = part->Headers();
    if ( isAttachment( headers ) )
      continue;
    const int type = headers.ContentType().Type();
    if ( type == DwMime::kTypeMultipart ) {
      if ( depth > 0 ) {
        if ( DwBodyPart * found = plainPart( part->Body().FirstBodyPart(), depth - 1 ) )
          return found;
      }
      continue;
    }
    if ( type == DwMime::kTypeText && headers.ContentType().Subtype() == DwMime::kSubtypePlain )
      return part;
  }
  return 0;
}

// The start of the text of @p msg: from its text/plain part (the body of
// a message that is text/plain, or the first text/plain part of its
// multiparts that is no attachment and in none), without quoted lines
// and signature; never HTML, never an encrypted text.  Empty when there
// is none.
QString previewOf( const KMMessage * msg )
{
  QString text;
  DwHeaders & headers = msg->getTopLevelPart()->Headers();
  if ( isAttachment( headers ) )
    return QString::null;
  if ( msg->type() == DwMime::kTypeMultipart ) {
    DwBodyPart * part = plainPart( msg->getFirstDwBodyPart(), 8 );
    if ( !part || part->Body().AsString().length() > maxPreviewSource )
      return QString::null;
    KMMessagePart msgPart;
    KMMessage::bodyPart( part, &msgPart );
    text = msgPart.bodyToUnicode();
  } else if ( ( msg->type() == DwMime::kTypeText && msg->subtype() == DwMime::kSubtypePlain ) ||
              msg->type() == DwMime::kTypeNull ) {
    if ( msg->getTopLevelPart()->Body().AsString().length() > maxPreviewSource )
      return QString::null;
    text = msg->bodyToUnicode();
  } else {
    return QString::null;
  }
  if ( text.find( "-----BEGIN PGP MESSAGE-----" ) >= 0 )
    return QString::null;
  QString preview;
  const QStringList lines = QStringList::split( '\n', text.left( 4096 ), false );
  for ( QStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it ) {
    if ( *it == "-- " || *it == "-- \r" )
      break;   // the signature
    const QString line = ( *it ).stripWhiteSpace();
    if ( line.startsWith( ">" ) || line.startsWith( "-----BEGIN PGP" ) || line.startsWith( "Hash:" ) )
      continue;
    preview += line + ' ';
    if ( preview.length() > 400 )
      break;
  }
  return preview;
}

// The picture of the sender of @p msg in the address book, if it has
// one in it (never one at a URL), at most 48 pixels.
QImage pictureOf( const KMMessage * msg )
{
  const QString email = KPIM::getFirstEmailAddress( msg->from() );
  if ( email.isEmpty() )
    return QImage();
  KABC::AddressBook * addressBook = KABC::StdAddressBook::self( true );
  const KABC::AddresseeList addressees = addressBook->findByEmail( email );
  for ( KABC::AddresseeList::ConstIterator it = addressees.begin(); it != addressees.end(); ++it ) {
    const KABC::Picture photo = ( *it ).photo();
    if ( !photo.isIntern() )
      continue;
    const QImage image = photo.data();
    if ( image.isNull() || image.width() > 4096 || image.height() > 4096 )
      continue;
    if ( image.width() > 48 || image.height() > 48 )
      return image.smoothScale( 48, 48, QImage::ScaleMin );
    return image;
  }
  return QImage();
}

// KNotify's event without the given presentation (what the user chose in
// Configure Notifications, but @p present), as KNotifyClient::event()
// sends it.
void notify( int present, const QString & text, WId window )
{
  if ( present <= 0 )
    return;
  DCOPClient * client = kapp->dcopClient();
  if ( !client->isAttached() )
    client->attach();
  if ( !client->isAttached() || !KNotifyClient::startDaemon() )
    return;
  QByteArray data;
  QDataStream ds( data, IO_WriteOnly );
  ds << QString::fromLatin1( notifyEvent )
     << QString::fromLatin1( KNotifyClient::instance()->instanceName() ) << text
     << QString::null << QString::null << present << int( KNotifyClient::Default )
     << int( window ) << kMax( 1, kapp->random() );
  client->send( "knotify", "Notify", "notify(QString,QString,QString,QString,QString,int,int,int,int)", data );
}

// The folders of the messages the balloon looks at, each opened once
// (a closed one reads its whole index) and closed again at the end.
class FolderSet
{
public:
  ~FolderSet()
  {
    for ( QValueList<QGuardedPtr<KMFolder> >::Iterator it = mOpen.begin(); it != mOpen.end(); ++it ) {
      if ( *it )
        ( *it )->close( owner );
    }
  }

  bool open( KMFolder * folder )
  {
    for ( QValueList<QGuardedPtr<KMFolder> >::ConstIterator it = mOpen.begin(); it != mOpen.end(); ++it ) {
      if ( static_cast<KMFolder*>( *it ) == folder )
        return true;
    }
    if ( mFailed.contains( folder ) )
      return false;
    if ( folder->open( owner ) != 0 ) {
      // (a maildir folder counts the open that failed: it was not
      // open before, an open fails only for the first opener)
      if ( folder->isOpened() )
        folder->close( owner );
      mFailed.append( folder );
      return false;
    }
    mOpen.append( folder );
    return true;
  }

  /** The message @p serNum where it is now, its folder open: in
      @p folder at @p idx (the index once the folder is open). */
  KMMsgBase * locate( Q_UINT32 serNum, KMFolder ** folder, int * idx )
  {
    *folder = 0;
    *idx = -1;
    KMMsgDict::instance()->getLocation( serNum, folder, idx );
    if ( !*folder || *idx < 0 || !open( *folder ) )
      return 0;
    KMMsgDict::instance()->getLocation( serNum, folder, idx );
    if ( !*folder || *idx < 0 )
      return 0;
    return ( *folder )->getMsgBase( *idx );
  }

private:
  static const char * const owner;
  QValueList<QGuardedPtr<KMFolder> > mOpen;
  QValueList<KMFolder*> mFailed;
};

const char * const FolderSet::owner = "newmailballoon";

} // namespace

NewMailBalloon::NewMailBalloon( KMMainWidget * mainWidget, const char * name )
  : QObject( mainWidget, name ),
    mMainWidget( mainWidget ),
    mShown( 0 ),
    mPoll( new QTimer( this, "newMailBalloonPoll" ) ),
    mPendingSerNum( 0 )
{
  connect( mPoll, SIGNAL( timeout() ), SLOT( showBalloon() ) );
}

// The balloon is a window of its own: it goes with us.
NewMailBalloon::~NewMailBalloon()
{
  mEntries.clear();
  mShown = 0;
  delete static_cast<KBalloon*>( mBalloon );
}

void NewMailBalloon::readConfig()
{
  if ( GlobalSettings::self()->showNewMailBalloon() )
    return;
  mEntries.clear();
  closeBalloon();
}

// The folders whose new mail gets a balloon: those of the tray icon's
// count (not trash, sent mail, drafts, outbox, templates, search or
// groupware folders), not those excluded from the new mail check.
bool NewMailBalloon::counts( KMFolder * folder ) const
{
  if ( !folder || folder->ignoreNewMail() || folder->noContent() )
    return false;
  if ( folder->folderType() == KMFolderTypeSearch )
    return false;
  if ( folder->storage() && folder->storage()->contentsType() != KMail::ContentsTypeMail )
    return false;   // calendar, contacts, ... (groupware)
  if ( kmkernel->folderIsTrash( folder ) || kmkernel->folderIsSentMailFolder( folder ) ||
       kmkernel->folderIsDraftOrOutbox( folder ) || kmkernel->folderIsTemplates( folder ) )
    return false;
  return !folder->isSystemFolder() || folder->name().lower() == "inbox" ||
         folder->folderType() == KMFolderTypeImap || folder->folderType() == KMFolderTypeCachedImap;
}

// The main window is in front (active, on this desktop) and shows
// @p folder: its new mail is seen there.  (In Kontact the mail view
// itself is shown, not another component's.)
bool NewMailBalloon::isShownInFront( KMFolder * folder ) const
{
  if ( mMainWidget->folder() != folder || !mMainWidget->isVisible() )
    return false;
  QWidget * window = mMainWidget->topLevelWidget();
  if ( !window->isVisible() || window->isMinimized() || !window->isActiveWindow() )
    return false;
  return KWin::windowInfo( window->winId(), NET::WMDesktop ).isOnCurrentDesktop();
}

// Whether @p folderId names a folder whose new mail counts for the
// balloon now: it is there, counts (counts()) and is not shown in front.
// (@p cache: the answers so far)
bool NewMailBalloon::countsNow( const QString & folderId, QMap<QString, bool> & cache ) const
{
  QMap<QString, bool>::ConstIterator it = cache.find( folderId );
  if ( it != cache.end() )
    return it.data();
  KMFolder * folder = kmkernel->findFolderById( folderId );
  const bool result = folder && counts( folder ) && !isShownInFront( folder );
  cache.insert( folderId, result );
  return result;
}

// The new mail of a check (@p newInFolder: how many messages in which
// folder) in an entry: the new messages the check received in those
// folders (AccountManager::newMessages(), each with the folder it is
// counted in) where they are now, unless they are read, spam or ignored
// or their folders do not count; the rest of the counted messages (IMAP
// folders the check did not list) only counted.
void NewMailBalloon::collect( const QMap<QString, int> & newInFolder, Entry & entry )
{
  QMap<QString, bool> countsCache;
  // the new messages of each counted folder, found or left out
  QMap<QString, int> found;
  FolderSet folders;
  const QValueList<AccountManager::NewMessage> & messages = kmkernel->acctMgr()->newMessages();
  for ( QValueList<AccountManager::NewMessage>::ConstIterator it = messages.begin(); it != messages.end(); ++it ) {
    if ( !newInFolder.contains( ( *it ).folderId ) )
      continue;   // not counted by this check
    found[ ( *it ).folderId ] += 1;
    if ( !countsNow( ( *it ).folderId, countsCache ) )
      continue;
    KMFolder * folder = 0;
    int idx = -1;
    const KMMsgBase * msgBase = folders.locate( ( *it ).serNum, &folder, &idx );
    if ( !msgBase )
      continue;   // gone meanwhile
    if ( folder->idString() != ( *it ).folderId && !countsNow( folder->idString(), countsCache ) )
      continue;   // moved by a filter into a folder that does not count
    if ( !( msgBase->isNew() || msgBase->isUnread() ) || msgBase->isSpam() || msgBase->isIgnored() )
      continue;
    entry.serNums.append( ( *it ).serNum );
    if ( !entry.folderIds.contains( folder->idString() ) )
      entry.folderIds.append( folder->idString() );
    if ( entry.items.count() < maxItems ) {
      Item item;
      item.serNum = ( *it ).serNum;
      item.from = squash( msgBase->fromStrip(), 200 );
      item.subject = squash( msgBase->subject(), 200 );
      entry.items.append( item );
    }
  }

  QStringList ids = newInFolder.keys();
  ids.sort();
  for ( QStringList::ConstIterator it = ids.begin(); it != ids.end(); ++it ) {
    if ( !countsNow( *it, countsCache ) )
      continue;
    const int rest = newInFolder[ *it ] - found[ *it ];
    if ( rest <= 0 )
      continue;
    entry.unknown[ *it ] += rest;
    if ( !entry.folderIds.contains( *it ) )
      entry.folderIds.append( *it );
  }
  entry.count = entry.serNums.count();
  for ( QMap<QString, int>::ConstIterator it = entry.unknown.begin(); it != entry.unknown.end(); ++it )
    entry.count += it.data();

  // One message: the start of its text and the sender's picture, when
  // the message is here already (not only its headers).
  if ( entry.count == 1 && entry.serNums.count() == 1 ) {
    KMFolder * folder = 0;
    int idx = -1;
    const KMMsgBase * msgBase = folders.locate( entry.serNums.first(), &folder, &idx );
    const bool unGet = msgBase && !msgBase->isMessage();
    KMMessage * msg = msgBase ? folder->getMsg( idx ) : 0;
    if ( msg ) {
      if ( msg->isComplete() )
        entry.preview = squash( previewOf( msg ), 200 );
      entry.picture = pictureOf( msg );
      if ( unGet )
        folder->unGetMsg( idx );
    }
  }
}

bool NewMailBalloon::newMail( const QMap<QString, int> & newInFolder, const QString & text )
{
  bool any = false;
  for ( QMap<QString, int>::ConstIterator it = newInFolder.begin(); it != newInFolder.end() && !any; ++it ) {
    KMFolder * folder = kmkernel->findFolderById( it.key() );
    any = folder && !folder->ignoreNewMail();
  }
  if ( !any )
    return false;

  // KNotify: the user chooses in Configure Notifications (a passive
  // popup by default).  Its passive popup is the balloon, unless that
  // is switched off and the user chose KNotify's own.
  KInstance * instance = kmkernel->xmlGuiInstance() ? kmkernel->xmlGuiInstance() : KGlobal::instance();
  KNotifyClient::Instance notifyInstance( instance );
  const int configured = KNotifyClient::getPresentation( notifyEvent );
  int present = configured;
  if ( present == -1 )
    present = KNotifyClient::getDefaultPresentation( notifyEvent );
  if ( present == -1 )
    present = KNotifyClient::PassivePopup;
  const WId window = mMainWidget->topLevelWidget()->winId();
  const bool balloon = GlobalSettings::self()->showNewMailBalloon() &&
                       ( present & KNotifyClient::PassivePopup );
  if ( !balloon && ( configured != -1 || !( present & KNotifyClient::PassivePopup ) ) ) {
    // as before
    KNotifyClient::event( window, notifyEvent, text );
    return true;
  }
  // (KNotify shows no message box with its passive popup either.)
  notify( present & ~( KNotifyClient::PassivePopup | KNotifyClient::Messagebox ), text, window );
  if ( !balloon )
    return true;

  Entry entry;
  collect( newInFolder, entry );
  if ( entry.count == 0 )
    return true;

  mEntries.append( entry );
  // At most maxEntries waiting: the oldest one not in the balloon goes.
  while ( mEntries.count() > maxEntries ) {
    QValueList<Entry>::Iterator oldest = mEntries.begin();
    if ( mBalloon )
      ++oldest;
    mEntries.remove( oldest );
  }
  // (a tray icon that shows only with unread mail is shown first)
  if ( !mBalloon )
    schedule( 300 );
  return true;
}

// Whether @p entry is still worth a balloon: one of its messages is
// still there and unread, or one of the folders of its messages only
// counted is still there.  The messages that are gone, or that the
// filters moved into a folder that does not count (online IMAP filters
// after the check), or that became read, spam or ignored, leave it (and its
// count), as do the folders that are gone.
bool NewMailBalloon::stillNew( Entry & entry ) const
{
  FolderSet folders;
  for ( QMap<QString, int>::Iterator it = entry.unknown.begin(); it != entry.unknown.end(); ) {
    QMap<QString, int>::Iterator unknown = it++;
    if ( !kmkernel->findFolderById( unknown.key() ) ) {
      entry.count -= unknown.data();
      entry.unknown.remove( unknown );
    }
  }
  QMap<QString, bool> countsCache;
  QValueList<Q_UINT32> serNums;
  QStringList folderIds;
  QValueList<Item> items;
  for ( QValueList<Q_UINT32>::ConstIterator it = entry.serNums.begin(); it != entry.serNums.end(); ++it ) {
    KMFolder * folder = 0;
    int idx = -1;
    const KMMsgBase * msgBase = folders.locate( *it, &folder, &idx );
    if ( !msgBase || !countsNow( folder->idString(), countsCache ) ||
         !( msgBase->isNew() || msgBase->isUnread() ) || msgBase->isSpam() || msgBase->isIgnored() ) {
      --entry.count;
      continue;
    }
    serNums.append( *it );
    if ( !folderIds.contains( folder->idString() ) )
      folderIds.append( folder->idString() );
    if ( items.count() < maxItems ) {
      Item item;
      item.serNum = *it;
      item.from = squash( msgBase->fromStrip(), 200 );
      item.subject = squash( msgBase->subject(), 200 );
      items.append( item );
    }
  }
  for ( QMap<QString, int>::ConstIterator it = entry.unknown.begin(); it != entry.unknown.end(); ++it ) {
    if ( !folderIds.contains( it.key() ) )
      folderIds.append( it.key() );
  }
  if ( serNums.isEmpty() ) {
    // (the preview and picture were those of a message that is gone)
    entry.preview = QString::null;
    entry.picture = QImage();
  }
  entry.serNums = serNums;
  entry.folderIds = folderIds;
  entry.items = items;
  return !entry.serNums.isEmpty() || !entry.unknown.isEmpty();
}

// Whether one of the messages of @p entry is being fetched (for the
// online IMAP filters, which move it after that).
bool NewMailBalloon::inTransfer( const Entry & entry ) const
{
  FolderSet folders;
  for ( QValueList<Q_UINT32>::ConstIterator it = entry.serNums.begin(); it != entry.serNums.end(); ++it ) {
    KMFolder * folder = 0;
    int idx = -1;
    const KMMsgBase * msgBase = folders.locate( *it, &folder, &idx );
    if ( msgBase && msgBase->isMessage() &&
         static_cast<const KMMessage*>( msgBase )->transferInProgress() )
      return true;
  }
  return false;
}

// showBalloon() in @p msec (one pending call: an earlier one stays)
void NewMailBalloon::schedule( int msec )
{
  if ( !mPoll->isActive() || msec == 0 )
    mPoll->start( msec, true );
}

// The first entry in a balloon at the tray icon (in the corner of the
// screen without one).  Title and text are plain text, what comes from
// the messages is cut short and kept in its own direction.
void NewMailBalloon::showBalloon()
{
  if ( mBalloon )
    return;
  while ( !mEntries.isEmpty() ) {
    // A message being fetched holds the first entry back, at most
    // maxWait seconds: a fetch that does not end does not hold it
    // longer (its messages are taken as they are then).
    Entry & first = mEntries.first();
    if ( inTransfer( first ) ) {
      const time_t now = time( 0 );
      if ( !first.waitSince )
        first.waitSince = now;
      if ( now - first.waitSince < maxWait ) {
        schedule( 500 );
        return;
      }
    }
    if ( stillNew( first ) )
      break;
    mEntries.remove( mEntries.begin() );
  }
  if ( mEntries.isEmpty() || !GlobalSettings::self()->showNewMailBalloon() )
    return;
  const Entry & entry = mEntries.first();

  QString title;
  QStringList lines;
  QPixmap picture;
  if ( entry.count == 1 && entry.items.count() == 1 ) {
    const Item & item = entry.items.first();
    title = i18n( "New mail from %1" ).arg( isolated( squash( item.from, 40 ) ) );
    lines.append( item.subject.isEmpty() ? i18n( "(no subject)" ) : isolated( squash( item.subject, 80 ) ) );
    if ( !entry.preview.isEmpty() )
      lines.append( isolated( squash( entry.preview, 120 ) ) );
    if ( !entry.picture.isNull() )
      picture.convertFromImage( entry.picture );
  } else {
    if ( entry.folderIds.count() == 1 ) {
      KMFolder * folder = kmkernel->findFolderById( entry.folderIds.first() );
      const QString label = folder ? isolated( squash( folder->label(), 40 ) ) : QString::null;
      title = i18n( "New mail in %1", "%n new messages in %1", entry.count ).arg( label );
    } else {
      title = i18n( "New mail", "%n new messages", entry.count );
    }
    for ( QValueList<Item>::ConstIterator it = entry.items.begin(); it != entry.items.end(); ++it ) {
      const QString subject = ( *it ).subject.isEmpty() ? i18n( "(no subject)" )
                                                        : isolated( squash( ( *it ).subject, 50 ) );
      lines.append( i18n( "sender: subject", "%1: %2" ).arg( isolated( squash( ( *it ).from, 25 ) ) )
                    .arg( subject ) );
    }
    const int more = entry.count - entry.items.count();
    if ( more > 0 && !entry.items.isEmpty() ) {
      lines.append( i18n( "and 1 more", "and %n more", more ) );
    } else if ( more > 0 && entry.folderIds.count() > 1 ) {
      for ( QMap<QString, int>::ConstIterator it = entry.unknown.begin(); it != entry.unknown.end(); ++it ) {
        KMFolder * folder = kmkernel->findFolderById( it.key() );
        if ( folder )
          lines.append( i18n( "1 in %1", "%n in %1", it.data() ).arg( isolated( squash( folder->label(), 40 ) ) ) );
      }
    }
  }
  if ( picture.isNull() )
    picture = DesktopIcon( "kmail", 48 );

  KBalloon * balloon = new KBalloon( mMainWidget, "newmailballoon" );
  mBalloon = balloon;
  mShown = balloon;
  // (for tools that look for it: no frame shows it)
  balloon->setCaption( i18n( "New Mail" ) );
  balloon->setTitle( title );
  balloon->setText( lines.join( "\n" ) );
  balloon->setPixmap( picture );
  balloon->insertAction( i18n( "to read a message", "Read" ), ReadAction );
  // (only the messages known here: all, or as many as it says)
  if ( !entry.serNums.isEmpty() )
    balloon->insertAction( (int)entry.serNums.count() == entry.count
                           ? i18n( "Mark as Read" )
                           : i18n( "Mark 1 as Read", "Mark %n as Read", entry.serNums.count() ),
                           MarkAsReadAction );
  balloon->insertAction( i18n( "Ignore" ), IgnoreAction );
  balloon->setTimeout( QMAX( 0, GlobalSettings::self()->newMailBalloonTimeout() ) * 1000 );   // 0: none
  connect( balloon, SIGNAL( clicked() ), SLOT( slotClicked() ) );
  connect( balloon, SIGNAL( actionActivated( int ) ), SLOT( slotAction( int ) ) );
  connect( balloon, SIGNAL( timedOut() ), SLOT( slotTimedOut() ) );
  connect( balloon, SIGNAL( destroyed() ), SLOT( slotBalloonDestroyed() ) );
  KMSystemTray * tray = mMainWidget->systray();
  if ( tray && tray->isVisible() )
    balloon->setAnchor( tray );
  balloon->show();
  // (its window is made when it is first shown)
  balloon->setCaption( i18n( "New Mail" ) );
}

// The balloon goes: by itself (it was clicked, an action chosen, its
// time is out: it closes after its signal), or hidden here.  It deletes
// itself.
void NewMailBalloon::closeBalloon( bool byItself )
{
  KBalloon * balloon = mBalloon;
  mBalloon = 0;
  mShown = 0;
  if ( !balloon )
    return;
  balloon->disconnect( this );
  connect( balloon, SIGNAL( destroyed() ), SLOT( slotBalloonDestroyed() ) );
  if ( !byItself )
    balloon->hide();
}

// The first entry is done: the next one.
void NewMailBalloon::next()
{
  if ( !mEntries.isEmpty() )
    mEntries.remove( mEntries.begin() );
  schedule( 0 );
}

void NewMailBalloon::slotBalloonDestroyed()
{
  if ( !mShown || sender() != mShown )
    return;
  // Dismissed (a right or middle click): as Ignore.
  mShown = 0;
  mBalloon = 0;
  next();
}

void NewMailBalloon::slotClicked()
{
  slotAction( ReadAction );
}

void NewMailBalloon::slotTimedOut()
{
  slotAction( IgnoreAction );
}

void NewMailBalloon::slotAction( int action )
{
  closeBalloon( true );
  if ( mEntries.isEmpty() )
    return;
  const Entry entry = mEntries.first();
  next();
  if ( action == ReadAction ) {
    showMessage( entry );
  } else if ( action == MarkAsReadAction ) {
    // (each folder open meanwhile, the indexes those of the open
    // folder: KMSetStatusCommand would not open those that are closed,
    // a folder the filters moved mail into)
    FolderSet folders;
    QMap<KMFolder*, QValueList<int> > ids;
    for ( QValueList<Q_UINT32>::ConstIterator it = entry.serNums.begin(); it != entry.serNums.end(); ++it ) {
      KMFolder * folder = 0;
      int idx = -1;
      if ( folders.locate( *it, &folder, &idx ) )
        ids[ folder ].append( idx );
    }
    for ( QMap<KMFolder*, QValueList<int> >::Iterator it = ids.begin(); it != ids.end(); ++it )
      it.key()->setStatus( it.data(), KMMsgStatusRead );
  }
}

// Read: the main window in front, the folder of the first message
// still there selected and the message in it (its folder only: count
// only).
void NewMailBalloon::showMessage( const Entry & entry )
{
  KMSystemTray * tray = mMainWidget->systray();
  if ( tray ) {
    tray->showKMail();
  } else {
    QWidget * window = mMainWidget->topLevelWidget();
    window->show();
    KWin::deIconifyWindow( window->winId() );
    KWin::activateWindow( window->winId() );
    kmkernel->raise();
  }

  // the first of its messages that is still there, else its first
  // folder that is
  KMFolder * folder = 0;
  mPendingSerNum = 0;
  for ( QValueList<Q_UINT32>::ConstIterator it = entry.serNums.begin(); it != entry.serNums.end(); ++it ) {
    KMFolder * where = 0;
    int idx = -1;
    KMMsgDict::instance()->getLocation( *it, &where, &idx );
    if ( where && idx >= 0 ) {
      folder = where;
      mPendingSerNum = *it;
      break;
    }
  }
  for ( QStringList::ConstIterator it = entry.folderIds.begin(); !folder && it != entry.folderIds.end(); ++it )
    folder = kmkernel->findFolderById( *it );
  if ( !folder )
    return;
  mPendingFolder = folder;
  mMainWidget->folderTree()->showFolder( folder );
  if ( !mPendingSerNum )
    return;
  // Selecting a folder builds its message list (later for IMAP).
  KMHeaders * headers = mMainWidget->headers();
  disconnect( headers, SIGNAL( messageListUpdated() ), this, SLOT( slotHeadersUpdated() ) );
  connect( headers, SIGNAL( messageListUpdated() ), SLOT( slotHeadersUpdated() ) );
  QTimer::singleShot( 0, this, SLOT( slotHeadersUpdated() ) );
}

void NewMailBalloon::slotHeadersUpdated()
{
  selectMessage();
}

// The message of Read, once its folder is the one shown.
void NewMailBalloon::selectMessage()
{
  KMHeaders * headers = mMainWidget->headers();
  if ( !mPendingSerNum || !mPendingFolder || headers->folder() != mPendingFolder )
    return;
  KMFolder * folder = 0;
  int idx = -1;
  KMMsgDict::instance()->getLocation( mPendingSerNum, &folder, &idx );
  if ( folder != mPendingFolder || idx < 0 )
    return;
  headers->clearSelection();   // (only this message selected)
  headers->setCurrentItemBySerialNum( mPendingSerNum );
  if ( headers->currentItemIndex() == idx ) {
    mPendingSerNum = 0;
    mPendingFolder = 0;
    disconnect( headers, SIGNAL( messageListUpdated() ), this, SLOT( slotHeadersUpdated() ) );
  }
}

#include "newmailballoon.moc"
