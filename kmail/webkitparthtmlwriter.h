/*  -*- c++ -*-
    webkitparthtmlwriter.h

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

#ifndef __KMAIL_WEBKITPARTHTMLWRITER_H__
#define __KMAIL_WEBKITPARTHTMLWRITER_H__

#include "interfaces/htmlwriter.h"

#include <qcolor.h>
#include <qguardedptr.h>
#include <qmap.h>
#include <qobject.h>
#include <qstringlist.h>
#include <qtimer.h>

namespace KParts { class ReadOnlyPart; }

namespace KMail {

  /**
   * An HtmlWriter for the WPE WebKit part in its mail mode
   * (libkwpepart, "mode=mail"): it collects the document and shows it
   * in one go, as files in a private directory (0700).
   *
   * The HTML parts of a message (queueIsolatedHtml()) become documents
   * of their own, shown in sandboxed frames of KMail's document: their
   * styles cannot change KMail's headers and attachment list, nor the
   * other way round.  Their cid: references are data: URLs already
   * (ObjectTreeParser).  Images KMail's document takes from KMail's
   * temporary files (file: URLs) are linked into the message's
   * directory; the part serves nothing else, apart from KDE's icons.
   *
   * Several documents in a row (KMail updates the reader for every part
   * of an IMAP message it downloads) are shown once: a document that is
   * followed by another before the event loop runs is never loaded.  An
   * empty document (no message) waits a little: the next message often
   * follows at once (a folder change), and the old one stays meanwhile.
   */
  class WebKitPartHtmlWriter : public QObject, public HtmlWriter {
    Q_OBJECT
  public:
    WebKitPartHtmlWriter( KParts::ReadOnlyPart * part, QObject * parent = 0, const char * name = 0 );
    virtual ~WebKitPartHtmlWriter();

    void begin( const QString & cssDefs );
    void end();
    void reset();
    void write( const QString & str );
    void queue( const QString & str );
    void flush();
    void embedPart( const QCString & contentId, const QString & url );
    bool isolatesHtml() const { return true; }
    void queueIsolatedHtml( const QString & html );

    /** The document being written shows no message (see class doc). */
    void setBlank() { mBlank = true; }
    /** Another message (or none) is going to be shown: the part is
        covered at once with @p background, so that the previous message
        is not shown while the next one is prepared, downloaded or
        loaded (the part shows a new document only once it is ready).
        A blank page replaces the previous message in the part, the cover
        goes once it is shown (see the implementation). */
    void clearView( const QColor & background );

    /** HTML for KMail's \<div id="attachmentInjectionPoint"\>. */
    void setAttachmentList( const QString & html ) { mAttachmentList = html; }

    /** The URL of KMail's document a kmail-link: URL of the document
        shown stands for (see slotLoad()); QString::null for any other. */
    QString originalURL( const QString & url ) const;

  private slots:
    void slotLoad();
    void slotCompleted();
    void slotUncover();
    void slotBlankDone();

  private:
    void forgetLinks();
    bool eventFilter( QObject * o, QEvent * e );
    QString messageDirectory();
    QString linkIntoMessage( const QString & dir, const QString & path, int index );

    QGuardedPtr<KParts::ReadOnlyPart> mPart;
    QString mCss;
    QString mHtml;
    QStringList mBodies;
    QString mAttachmentList;
    bool mBlank;

    // the document waiting to be shown
    QString mPendingCss, mPendingHtml, mPendingAttachmentList;
    QStringList mPendingBodies;
    QTimer mLoadTimer;

    QString mRoot;          // our private directory
    unsigned mSerial;
    QStringList mDirs;      // message directories, oldest first
    QString mLinkSecret;
    QMap<QString, QString> mLinks;   // kmail-link: URL -> original href
    QGuardedPtr<QWidget> mCover;   // over the part's widget, see clearView()
    bool mCovered;          // the cover is up (whatever the widgets' visibility)
    bool mBlankLoading;     // the blank page of a message switch is loading
    bool mDeferredLoad;     // a document waits for it
    QTimer mUncoverTimer;
    QTimer mBlankTimer;
    bool mStats;
    long long mEndTime;     // statistics: end() of the document shown last
  };

} // namespace KMail

#endif // __KMAIL_WEBKITPARTHTMLWRITER_H__
