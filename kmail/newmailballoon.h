/*
    This file is part of KMail.

    A balloon at the system tray icon for new mail.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KMAIL_NEWMAILBALLOON_H
#define KMAIL_NEWMAILBALLOON_H

#include <qobject.h>
#include <qguardedptr.h>
#include <qimage.h>
#include <qmap.h>
#include <qstringlist.h>
#include <qvaluelist.h>

class KBalloon;
class KMFolder;
class KMMainWidget;

namespace KMail {

/**
 * The new mail notification of a main widget: KNotify's event
 * "new-mail-arrived" (sound, log file, command, ... as the user chose in
 * Configure Notifications) and, for its passive popup, a balloon
 * (KBalloon) at KMail's system tray icon with the sender, the subject
 * and the start of the text of the new message, or a summary of several
 * ones.  Click or "Read" shows the message in the main window, "Mark as
 * Read" marks the messages of the balloon as read, "Ignore" closes it.
 *
 * One balloon at a time: the new mail of later checks waits (a bounded
 * queue).  All the text in it comes from the messages: it is plain text
 * (never markup), cut short and kept in its own writing direction.
 * Only what is already here is shown: no message is downloaded for it.
 */
class NewMailBalloon : public QObject
{
  Q_OBJECT

public:
  NewMailBalloon( KMMainWidget * mainWidget, const char * name = 0 );
  ~NewMailBalloon();

  /** New mail was found: @p newInFolder the number of new messages per
      folder id (AccountManager::checkedMail()), @p text KNotify's text
      for them.  Notifies with KNotify; the balloon (when it is KNotify's
      passive popup and switched on) for the folders that count for new
      mail.  Returns whether any folder counted. */
  bool newMail( const QMap<QString, int> & newInFolder, const QString & text );

  /** The settings changed: switched off, no balloon. */
  void readConfig();

private slots:
  void showBalloon();
  void slotClicked();
  void slotAction( int action );
  void slotTimedOut();
  void slotBalloonDestroyed();
  void slotHeadersUpdated();

private:
  // A new message the balloon can tell about.
  struct Item {
    Q_UINT32 serNum;
    QString from;
    QString subject;
  };
  // What one balloon tells about: the new mail of one check.
  struct Entry {
    Entry() : count( 0 ) {}
    int count;                  // new messages in all
    QValueList<Item> items;     // the first ones known here (at most maxItems)
    QValueList<Q_UINT32> serNums; // all the ones known here
    QStringList folderIds;      // their folders, the first first
    QMap<QString, int> unknown; // folder id: new messages not known here
    QString preview;            // the start of the text (one message)
    QImage picture;             // the sender's (one message)
  };
  enum { ReadAction = 1, MarkAsReadAction = 2, IgnoreAction = 3 };
  enum { maxEntries = 5, maxItems = 4 };

  bool counts( KMFolder * folder ) const;
  bool isShownInFront( KMFolder * folder ) const;
  bool countsNow( const QString & folderId, QMap<QString, bool> & cache ) const;
  void collect( const QMap<QString, int> & newInFolder, Entry & entry );
  bool stillNew( Entry & entry ) const;
  void closeBalloon( bool byItself = false );
  void next();
  void showMessage( const Entry & entry );
  void selectMessage();

  KMMainWidget * mMainWidget;
  QValueList<Entry> mEntries;       // the first is in the balloon (if shown)
  QGuardedPtr<KBalloon> mBalloon;
  // The balloon of the first entry until we close it (closeBalloon()):
  // one that goes otherwise (a right or middle click hides it, without
  // a signal) was dismissed.
  const KBalloon * mShown;
  Q_UINT32 mPendingSerNum;          // to select once the headers are there
  QGuardedPtr<KMFolder> mPendingFolder;
};

}

#endif
