/*
    This file is part of KMail.

    Watches systemd-logind for the machine going to sleep and waking up.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef KMAIL_LOGINDSLEEP_H
#define KMAIL_LOGINDSLEEP_H

// No Qt and no GLib here: logindsleep.cpp has to include <gio/gio.h>
// before any Qt header (Qt defines "signals"), its users include Qt.

namespace KMail {

class LogindSleepListener
{
public:
  virtual ~LogindSleepListener() {}
  /** The machine is about to suspend or hibernate.  Call
      releaseLogindSleepDelay() once ready, logind waits for that (at
      most InhibitDelayMaxSec, 5 s by default). */
  virtual void prepareForSleep() = 0;
  /** The machine woke up (or the suspend failed, or the connection to
      the bus was lost: the wakeup may have gone unnoticed). */
  virtual void resumedFromSleep() = 0;
};

/** Subscribes to logind's PrepareForSleep signal on the system bus and
    takes a "delay" sleep inhibitor lock.  The callbacks are delivered by
    the GLib main loop, i.e. only when Qt runs on it (QT_GLIB_EVENTLOOP).
    Returns false when this is not available; the work is asynchronous,
    a missing system bus or logind is only logged.

    For testing, KMAIL_LOGIND_TEST_BUS=<D-Bus address> uses that bus
    instead of the system bus. */
bool watchLogindSleep( LogindSleepListener *listener );

/** Lets the pending suspend proceed (drops the delay lock until the
    machine is awake again). */
void releaseLogindSleepDelay();

void unwatchLogindSleep();

}

#endif
