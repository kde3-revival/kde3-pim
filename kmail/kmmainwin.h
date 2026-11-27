/*
 * kmail: KDE mail client
 * Copyright (c) 1996-1998 Stefan Taferner <taferner@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#ifndef __KMMAINWIN
#define __KMMAINWIN

#include <kmainwindow.h>
#include "kdeversion.h"
#include "qstring.h"

class KMMainWidget;
namespace KPIM {
   class StatusbarProgressWidget;
   class ProgressDialog;
}
using KPIM::StatusbarProgressWidget;
using KPIM::ProgressDialog;

class KMMainWin : public KMainWindow
{
  Q_OBJECT

public:
  // the main window needs to have a name since else restoring the window
  // settings by kwin doesn't work
  KMMainWin(QWidget *parent = 0);
  virtual ~KMMainWin();
  KMMainWidget *mainKMWidget() const { return mKMMainWidget; };
  StatusbarProgressWidget* progressWidget() const { return mLittleProgress; }
  ProgressDialog* progressDialog() const { return mProgressDialog; }


  /** Read configuration options after widgets are created. */
  virtual void readConfig(void);

  /** Write configuration options. */
  virtual void writeConfig(void);

  /** For a new window that is not shown yet: keeps it hidden behind the
      system tray icon, if the icon is always shown (else the window would
      be lost), and returns true; false when the caller has to show it. */
  bool stayInTray();

  /** Whether the session saved the window hidden in the tray. */
  bool restoredInTray() const { return mRestoredInTray; }

public slots:
  void displayStatusMsg(const QString&);
  void slotEditToolbars();
  void slotUpdateToolbars();
  void setupStatusBar();

protected:
  virtual bool queryClose ();
  virtual void saveProperties( KConfig *config );
  virtual void readProperties( KConfig *config );

protected slots:
  void slotQuit();
  void slotConfigChanged();
  void slotShowTipOnStart();

private slots:
  void slotNewMailReader();

private:
  KMMainWidget *mKMMainWidget;
  StatusbarProgressWidget *mLittleProgress;
  ProgressDialog *mProgressDialog;
  int mMessageStatusId;
  bool mReallyClose;
  bool mRestoredInTray;
};

#endif
