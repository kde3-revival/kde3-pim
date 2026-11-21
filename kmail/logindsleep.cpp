/*
    This file is part of KMail.

    Watches systemd-logind for the machine going to sleep and waking up.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// GIO first: Qt defines "signals", which GLib uses as an identifier
#ifdef HAVE_GIO_UNIX
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <unistd.h>
#include <stdlib.h>
#endif

#include "logindsleep.h"

#include <qglobal.h>
#include <kdebug.h>

// The callbacks need a GLib main loop, Qt has to run on one
#if defined(HAVE_GIO_UNIX) && defined(QT_GLIB_EVENTLOOP)

namespace {

const char *const loginService = "org.freedesktop.login1";
const char *const loginPath = "/org/freedesktop/login1";
const char *const loginManager = "org.freedesktop.login1.Manager";

struct State
{
  KMail::LogindSleepListener *listener;
  GCancellable *cancellable;
  GDBusConnection *connection;
  guint subscription;
  gulong closedHandler;
  int delayLock;       // fd of the inhibitor lock, -1 if not held
  bool inhibiting;     // an Inhibit() call is on its way
};

State *s = 0;

void takeDelayLock();

void inhibitDone( GObject *source, GAsyncResult *result, gpointer )
{
  GUnixFDList *fds = 0;
  GError *error = 0;
  GVariant *reply = g_dbus_connection_call_with_unix_fd_list_finish(
      G_DBUS_CONNECTION( source ), &fds, result, &error );
  if ( !reply ) {
    // when cancelled, s is gone already
    if ( !g_error_matches( error, G_IO_ERROR, G_IO_ERROR_CANCELLED ) ) {
      kdWarning(5006) << "logind: no sleep delay lock: " << error->message << endl;
      s->inhibiting = false;
    }
    g_error_free( error );
    return;
  }
  s->inhibiting = false;

  gint32 index = -1;
  g_variant_get( reply, "(h)", &index );
  g_variant_unref( reply );
  int fd = fds ? g_unix_fd_list_get( fds, index, &error ) : -1;
  if ( fds )
    g_object_unref( fds );
  if ( fd < 0 ) {
    kdWarning(5006) << "logind: Inhibit() returned no file descriptor" << endl;
    if ( error )
      g_error_free( error );
    return;
  }
  if ( s->delayLock >= 0 )
    close( s->delayLock );
  s->delayLock = fd;
  kdDebug(5006) << "logind: holding a sleep delay lock" << endl;
}

void takeDelayLock()
{
  if ( !s->connection || s->delayLock >= 0 || s->inhibiting )
    return;
  s->inhibiting = true;
  g_dbus_connection_call_with_unix_fd_list(
      s->connection, loginService, loginPath, loginManager, "Inhibit",
      g_variant_new( "(ssss)", "sleep", "KMail",
                     "Disconnect from the mail servers", "delay" ),
      G_VARIANT_TYPE( "(h)" ), G_DBUS_CALL_FLAGS_NONE, -1, 0,
      s->cancellable, inhibitDone, 0 );
}

void prepareForSleep( GDBusConnection *, const gchar *, const gchar *,
                      const gchar *, const gchar *, GVariant *parameters,
                      gpointer )
{
  if ( !g_variant_is_of_type( parameters, G_VARIANT_TYPE( "(b)" ) ) )
    return;
  gboolean sleeping = FALSE;
  g_variant_get( parameters, "(b)", &sleeping );
  kdDebug(5006) << "logind: PrepareForSleep(" << ( sleeping ? "true" : "false" ) << ")" << endl;
  if ( sleeping ) {
    s->listener->prepareForSleep();
  } else {
    takeDelayLock();
    s->listener->resumedFromSleep();
  }
}

void connectionClosed( GDBusConnection *, gboolean, GError *, gpointer )
{
  kdWarning(5006) << "logind: lost the connection to the bus" << endl;
  // the subscription and the pending calls die with the connection
  g_signal_handler_disconnect( s->connection, s->closedHandler );
  g_object_unref( s->connection );
  s->connection = 0;
  s->subscription = 0;
  s->closedHandler = 0;
  s->inhibiting = false;
  if ( s->delayLock >= 0 ) {
    close( s->delayLock );
    s->delayLock = -1;
  }
}

void connected( GObject *, GAsyncResult *result, gpointer testBus )
{
  GError *error = 0;
  GDBusConnection *connection = testBus
      ? g_dbus_connection_new_for_address_finish( result, &error )
      : g_bus_get_finish( result, &error );
  if ( !connection ) {
    if ( !g_error_matches( error, G_IO_ERROR, G_IO_ERROR_CANCELLED ) )
      kdWarning(5006) << "logind: cannot connect to the system bus: " << error->message << endl;
    g_error_free( error );
    return;
  }
  // g_bus_get() connections call exit() when the bus goes away
  g_dbus_connection_set_exit_on_close( connection, FALSE );
  s->connection = connection;
  s->closedHandler = g_signal_connect( connection, "closed",
                                       G_CALLBACK( connectionClosed ), 0 );
  s->subscription = g_dbus_connection_signal_subscribe(
      connection, loginService, loginManager, "PrepareForSleep", loginPath,
      0, G_DBUS_SIGNAL_FLAGS_NONE, prepareForSleep, 0, 0 );
  takeDelayLock();
}

}

bool KMail::watchLogindSleep( LogindSleepListener *listener )
{
  if ( s )
    return true;
  s = new State;
  s->listener = listener;
  s->cancellable = g_cancellable_new();
  s->connection = 0;
  s->subscription = 0;
  s->closedHandler = 0;
  s->delayLock = -1;
  s->inhibiting = false;

  const char *testBus = getenv( "KMAIL_LOGIND_TEST_BUS" );
  if ( testBus && *testBus ) {
    kdDebug(5006) << "logind: using the test bus " << testBus << endl;
    g_dbus_connection_new_for_address(
        testBus, GDBusConnectionFlags( G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                       G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION ),
        0, s->cancellable, connected, GINT_TO_POINTER( 1 ) );
  } else {
    g_bus_get( G_BUS_TYPE_SYSTEM, s->cancellable, connected, 0 );
  }
  return true;
}

void KMail::releaseLogindSleepDelay()
{
  if ( s && s->delayLock >= 0 ) {
    close( s->delayLock );
    s->delayLock = -1;
    kdDebug(5006) << "logind: released the sleep delay lock" << endl;
  }
}

void KMail::unwatchLogindSleep()
{
  if ( !s )
    return;
  // the callbacks of cancelled calls still run, from the main loop, and
  // must not touch s
  g_cancellable_cancel( s->cancellable );
  g_object_unref( s->cancellable );
  if ( s->connection ) {
    g_signal_handler_disconnect( s->connection, s->closedHandler );
    if ( s->subscription )
      g_dbus_connection_signal_unsubscribe( s->connection, s->subscription );
    g_object_unref( s->connection );
  }
  if ( s->delayLock >= 0 )
    close( s->delayLock );
  delete s;
  s = 0;
}

#else // no GIO or no GLib main loop

bool KMail::watchLogindSleep( LogindSleepListener * )
{
  return false;
}

void KMail::releaseLogindSleepDelay()
{
}

void KMail::unwatchLogindSleep()
{
}

#endif
