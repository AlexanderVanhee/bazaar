/* bazaar-daemon.c
 *
 * Copyright 2026 Alexander Vanhee
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <dbus/dbus.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <systemd/sd-event.h>
#include <unistd.h>

#include "search-index.h"

#define BUS_NAME            "io.github.kolunmi.Bazaar.SearchProvider"
#define OBJECT_PATH         "/io/github/kolunmi/Bazaar/SearchProvider"
#define DAEMON_IFACE        "io.github.kolunmi.Bazaar.Daemon"
#define SEARCH_IFACE        "org.gnome.Shell.SearchProvider2"
#define MAX_SEARCH_RESULTS  25
#define ACTIVATE_TIMEOUT_MS 2000

#define UPDATE_CHECK_INTERVAL_USEC (60ULL * 60ULL * 1000000ULL) /* 1 hour */
#define IDLE_EXIT_TIMEOUT_USEC     (5ULL * 1000000ULL)

#define _cleanup_(x) __attribute__ ((cleanup (x)))

static sd_event        *event               = NULL;
static DBusConnection  *bus                 = NULL;
static pid_t            child_pid           = -1;
static SearchIndex     *g_index             = NULL;
static char            *g_index_path        = NULL;
static sd_event_source *update_timer_source = NULL;
static sd_event_source *idle_timer_source   = NULL;
static sd_event_source *dispatch_source     = NULL;

static void              log_msg (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void             *malloc_or_bail (size_t n_bytes);
static char             *build_index_path (void);
static void              ensure_index_loaded (void);
static int               strv_count_local (char **strv);
static void              strv_free_local (char **strv);
static void              strv_freep (char ***strv);
static void              generic_freep (void *p);
static int               auto_update_enabled (void);
static void              cancel_idle_timer (void);
static void              arm_idle_timer (void);
static int               on_idle_timeout (sd_event_source *s, uint64_t usec, void *userdata);
static int               on_child_exit (sd_event_source *s, const siginfo_t *si, void *userdata);
static void              launch_app (char **extra_args, int n_extra_args);
static void              run_update_worker_once (void);
static int               on_update_worker_exit (sd_event_source *s, const siginfo_t *si, void *userdata);
static int               on_update_timer (sd_event_source *s, uint64_t usec, void *userdata);
static int               schedule_next_update_check (void);
static void              build_and_send_search_reply (DBusConnection *connection, DBusMessage *call, char **terms);
static DBusHandlerResult handle_get_result_set (DBusConnection *connection, DBusMessage *message);
static DBusHandlerResult handle_get_result_metas (DBusConnection *connection, DBusMessage *message);
static DBusHandlerResult handle_activate_result (DBusConnection *connection, DBusMessage *message);
static DBusHandlerResult handle_launch_search (DBusConnection *connection, DBusMessage *message);
static DBusHandlerResult handle_relaunch (DBusConnection *connection, DBusMessage *message);
static DBusHandlerResult handle_quit (DBusConnection *connection, DBusMessage *message);
static int               on_signal (sd_event_source *s, const struct signalfd_siginfo *si, void *userdata);
static int               forward_to_running_daemon (DBusConnection *forward_bus, int argc, char *argv[]);
static char            **read_strv_arg (DBusMessageIter *iter);
static void              append_meta (DBusMessageIter *dict_iter, const char *key, const char *value);
static void              send_empty_reply (DBusConnection *connection, DBusMessage *message);
static DBusHandlerResult daemon_message_handler (DBusConnection *connection, DBusMessage *message, void *user_data);
static int               on_watch_io (sd_event_source *s, int fd, uint32_t revents, void *userdata);
static dbus_bool_t       add_watch (DBusWatch *watch, void *data);
static void              remove_watch (DBusWatch *watch, void *data);
static void              watch_toggled (DBusWatch *watch, void *data);
static int               on_dbus_timeout (sd_event_source *s, uint64_t usec, void *userdata);
static dbus_bool_t       add_timeout (DBusTimeout *timeout, void *data);
static void              remove_timeout (DBusTimeout *timeout, void *data);
static void              timeout_toggled (DBusTimeout *timeout, void *data);
static int               on_dispatch (sd_event_source *s, void *userdata);
static void              dispatch_status_cb (DBusConnection *connection, DBusDispatchStatus new_status, void *data);

static const DBusObjectPathVTable daemon_vtable = {
  .message_function = daemon_message_handler,
};

int
main (int argc, char *argv[])
{
  sigset_t                         mask       = { 0 };
  DBusError                        err        = DBUS_ERROR_INIT;
  int                              r          = 0;
  int                              no_window  = 0;
  int                              name_reply = 0;
  _cleanup_ (generic_freep) char **fwd_args   = NULL;
  int                              n_fwd_args = 0;
  int                              i          = 0;

  sigemptyset (&mask);
  sigaddset (&mask, SIGTERM);
  sigaddset (&mask, SIGINT);
  sigaddset (&mask, SIGCHLD);
  sigprocmask (SIG_BLOCK, &mask, NULL);

  r = sd_event_default (&event);
  if (r < 0)
    return 1;

  sd_event_add_signal (event, NULL, SIGTERM, on_signal, NULL);
  sd_event_add_signal (event, NULL, SIGINT, on_signal, NULL);

  bus = dbus_bus_get_private (DBUS_BUS_SESSION, &err);
  if (bus == NULL)
    {
      log_msg ("Failed to connect to session bus: %s", err.message);
      dbus_error_free (&err);
      return 1;
    }

  dbus_connection_set_exit_on_disconnect (bus, FALSE);
  dbus_connection_set_watch_functions (bus, add_watch, remove_watch, watch_toggled, NULL, NULL);
  dbus_connection_set_timeout_functions (bus, add_timeout, remove_timeout, timeout_toggled, NULL, NULL);

  sd_event_add_defer (event, &dispatch_source, on_dispatch, NULL);
  sd_event_source_set_enabled (dispatch_source, SD_EVENT_OFF);
  dbus_connection_set_dispatch_status_function (bus, dispatch_status_cb, NULL, NULL);

  name_reply = dbus_bus_request_name (bus, BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
  if (dbus_error_is_set (&err))
    {
      log_msg ("Failed to acquire bus name %s: %s", BUS_NAME, err.message);
      dbus_error_free (&err);
      return 1;
    }

  if (name_reply == DBUS_REQUEST_NAME_REPLY_EXISTS)
    {
      log_msg ("App already running, forwarding");
      forward_to_running_daemon (bus, argc, argv);
      dbus_connection_close (bus);
      dbus_connection_unref (bus);
      sd_event_unref (event);
      return 0;
    }

  if (name_reply != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      log_msg ("Failed to acquire bus name %s: unexpected reply %d", BUS_NAME, name_reply);
      return 1;
    }

  if (!dbus_connection_register_object_path (bus, OBJECT_PATH, &daemon_vtable, NULL))
    log_msg ("Failed to register daemon object");

  ensure_index_loaded ();
  schedule_next_update_check ();

  fwd_args = malloc_or_bail (sizeof (char *) * (size_t) (argc > 0 ? argc : 1));
  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--no-window") == 0)
        {
          no_window = 1;
          continue;
        }

      fwd_args[n_fwd_args] = argv[i];
      n_fwd_args++;
    }

  if (no_window)
    log_msg ("staying headless");
  else
    launch_app (fwd_args, n_fwd_args);

  unsetenv ("XDG_ACTIVATION_TOKEN");
  unsetenv ("DESKTOP_STARTUP_ID");

  arm_idle_timer ();

  while (dbus_connection_get_dispatch_status (bus) == DBUS_DISPATCH_DATA_REMAINS)
    dbus_connection_dispatch (bus);

  sd_event_loop (event);
  cancel_idle_timer ();

  dbus_connection_flush (bus);
  dbus_connection_close (bus);
  dbus_connection_unref (bus);

  if (update_timer_source != NULL)
    sd_event_source_unref (update_timer_source);
  if (dispatch_source != NULL)
    sd_event_source_unref (dispatch_source);

  search_index_close (g_index);
  free (g_index_path);

  sd_event_unref (event);

  return 0;
}

static DBusHandlerResult
daemon_message_handler (DBusConnection *connection, DBusMessage *message, void *user_data)
{
  if (dbus_message_is_method_call (message, SEARCH_IFACE, "GetInitialResultSet"))
    return handle_get_result_set (connection, message);
  if (dbus_message_is_method_call (message, SEARCH_IFACE, "GetSubsearchResultSet"))
    return handle_get_result_set (connection, message);
  if (dbus_message_is_method_call (message, SEARCH_IFACE, "GetResultMetas"))
    return handle_get_result_metas (connection, message);
  if (dbus_message_is_method_call (message, SEARCH_IFACE, "ActivateResult"))
    return handle_activate_result (connection, message);
  if (dbus_message_is_method_call (message, SEARCH_IFACE, "LaunchSearch"))
    return handle_launch_search (connection, message);
  if (dbus_message_is_method_call (message, DAEMON_IFACE, "Relaunch"))
    return handle_relaunch (connection, message);
  if (dbus_message_is_method_call (message, DAEMON_IFACE, "Quit"))
    return handle_quit (connection, message);

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int
on_watch_io (sd_event_source *s, int fd, uint32_t revents, void *userdata)
{
  DBusWatch   *watch = userdata;
  unsigned int flags = 0;

  if (revents & EPOLLIN)
    flags |= DBUS_WATCH_READABLE;
  if (revents & EPOLLOUT)
    flags |= DBUS_WATCH_WRITABLE;
  if (revents & EPOLLHUP)
    flags |= DBUS_WATCH_HANGUP;
  if (revents & EPOLLERR)
    flags |= DBUS_WATCH_ERROR;

  dbus_watch_handle (watch, flags);

  if (dbus_connection_get_dispatch_status (bus) == DBUS_DISPATCH_DATA_REMAINS
      && dispatch_source != NULL)
    sd_event_source_set_enabled (dispatch_source, SD_EVENT_ON);

  return 0;
}

static dbus_bool_t
add_watch (DBusWatch *watch, void *data)
{
  sd_event_source *s      = NULL;
  unsigned int     flags  = 0;
  uint32_t         events = 0;
  int              r      = 0;

  if (!dbus_watch_get_enabled (watch))
    return TRUE;

  flags = dbus_watch_get_flags (watch);
  if (flags & DBUS_WATCH_READABLE)
    events |= EPOLLIN;
  if (flags & DBUS_WATCH_WRITABLE)
    events |= EPOLLOUT;

  r = sd_event_add_io (event, &s, dbus_watch_get_unix_fd (watch), events, on_watch_io, watch);
  if (r < 0)
    return FALSE;

  dbus_watch_set_data (watch, s, NULL);
  return TRUE;
}

static void
remove_watch (DBusWatch *watch, void *data)
{
  sd_event_source *s = NULL;

  s = dbus_watch_get_data (watch);
  if (s != NULL)
    sd_event_source_unref (s);

  dbus_watch_set_data (watch, NULL, NULL);
}

static void
watch_toggled (DBusWatch *watch, void *data)
{
  sd_event_source *s = NULL;

  s = dbus_watch_get_data (watch);
  if (s == NULL)
    {
      if (dbus_watch_get_enabled (watch))
        add_watch (watch, data);
      return;
    }

  sd_event_source_set_enabled (s, dbus_watch_get_enabled (watch) ? SD_EVENT_ON : SD_EVENT_OFF);
}

static int
on_dbus_timeout (sd_event_source *s, uint64_t usec, void *userdata)
{
  dbus_timeout_handle ((DBusTimeout *) userdata);
  return 0;
}

static dbus_bool_t
add_timeout (DBusTimeout *timeout, void *data)
{
  sd_event_source *s   = NULL;
  uint64_t         now = 0;
  int              r   = 0;

  if (!dbus_timeout_get_enabled (timeout))
    return TRUE;

  sd_event_now (event, CLOCK_MONOTONIC, &now);

  r = sd_event_add_time (event, &s, CLOCK_MONOTONIC,
                         now + (uint64_t) dbus_timeout_get_interval (timeout) * 1000,
                         0, on_dbus_timeout, timeout);
  if (r < 0)
    return FALSE;

  dbus_timeout_set_data (timeout, s, NULL);
  return TRUE;
}

static void
remove_timeout (DBusTimeout *timeout, void *data)
{
  sd_event_source *s = NULL;

  s = dbus_timeout_get_data (timeout);
  if (s != NULL)
    sd_event_source_unref (s);

  dbus_timeout_set_data (timeout, NULL, NULL);
}

static void
timeout_toggled (DBusTimeout *timeout, void *data)
{
  remove_timeout (timeout, data);
  if (dbus_timeout_get_enabled (timeout))
    add_timeout (timeout, data);
}

static int
on_dispatch (sd_event_source *s, void *userdata)
{
  while (dbus_connection_dispatch (bus) == DBUS_DISPATCH_DATA_REMAINS)
    ;

  sd_event_source_set_enabled (s, SD_EVENT_OFF);
  return 0;
}

static void
dispatch_status_cb (DBusConnection *connection, DBusDispatchStatus new_status, void *data)
{
  if (dispatch_source == NULL)
    return;

  sd_event_source_set_enabled (dispatch_source,
                               new_status == DBUS_DISPATCH_DATA_REMAINS ? SD_EVENT_ON : SD_EVENT_OFF);
}

static int
auto_update_enabled (void)
{
  FILE *fp      = NULL;
  char  buf[8]  = { 0 };
  int   enabled = 0;

  fp = popen ("gsettings get io.github.kolunmi.Bazaar auto-update", "r");
  if (fp == NULL)
    return 0;

  if (fgets (buf, sizeof (buf), fp) != NULL)
    enabled = (strncmp (buf, "true", 4) == 0);

  pclose (fp);

  return enabled;
}

static void
cancel_idle_timer (void)
{
  if (idle_timer_source != NULL)
    {
      sd_event_source_unref (idle_timer_source);
      idle_timer_source = NULL;
    }
}

static void
arm_idle_timer (void)
{
  uint64_t now = 0;

  cancel_idle_timer ();

  if (child_pid > 0)
    return;

  if (sd_event_now (event, CLOCK_BOOTTIME, &now) < 0)
    return;

  sd_event_add_time (event, &idle_timer_source, CLOCK_BOOTTIME,
                     now + IDLE_EXIT_TIMEOUT_USEC, 0,
                     on_idle_timeout, NULL);
}

static int
on_idle_timeout (sd_event_source *s,
                 uint64_t         usec,
                 void            *userdata)
{
  idle_timer_source = NULL;

  if (child_pid > 0)
    return 0;

  if (auto_update_enabled ())
    {
      log_msg ("Idle, but auto update is on so staying alive");
      return 0;
    }

  log_msg ("Idle timeout reached, closing down");
  sd_event_exit (event, 0);
  return 0;
}

static int
forward_to_running_daemon (DBusConnection *forward_bus, int argc, char *argv[])
{
  DBusMessage    *m     = NULL;
  DBusMessage    *reply = NULL;
  DBusMessageIter iter;
  DBusMessageIter sub;
  DBusError       err   = DBUS_ERROR_INIT;
  int             i     = 0;
  int             start = argc > 0 ? 1 : 0;
  const char     *token = getenv ("XDG_ACTIVATION_TOKEN");

  m = dbus_message_new_method_call (BUS_NAME, OBJECT_PATH, DAEMON_IFACE, "Relaunch");
  if (m == NULL)
    return -1;

  dbus_message_iter_init_append (m, &iter);
  dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, "s", &sub);
  for (i = start; i < argc; i++)
    dbus_message_iter_append_basic (&sub, DBUS_TYPE_STRING, &argv[i]);
  dbus_message_iter_close_container (&iter, &sub);

  if (token != NULL)
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &token);

  reply = dbus_connection_send_with_reply_and_block (forward_bus, m, ACTIVATE_TIMEOUT_MS, &err);
  dbus_message_unref (m);

  if (reply == NULL)
    {
      log_msg ("Failed to forward relaunch request: %s", err.message);
      dbus_error_free (&err);
      return -1;
    }

  dbus_message_unref (reply);
  return 0;
}

static DBusHandlerResult
handle_relaunch (DBusConnection *connection, DBusMessage *message)
{
  DBusMessageIter               iter;
  _cleanup_ (strv_freep) char **terms = NULL;
  const char                   *token = NULL;

  dbus_message_iter_init (message, &iter);
  terms = read_strv_arg (&iter);
  if (dbus_message_iter_next (&iter) &&
      dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_STRING)
    dbus_message_iter_get_basic (&iter, &token);

  log_msg ("Relaunch requested via D-Bus");

  if (strv_count_local (terms) == 0 && child_pid > 0 &&
      !dbus_bus_name_has_owner (connection, "io.github.kolunmi.Bazaar", NULL))
    {
      send_empty_reply (connection, message);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

  arm_idle_timer ();
  if (token != NULL)
    setenv ("XDG_ACTIVATION_TOKEN", token, 1);
  launch_app (terms, strv_count_local (terms));
  unsetenv ("XDG_ACTIVATION_TOKEN");

  send_empty_reply (connection, message);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_quit (DBusConnection *connection, DBusMessage *message)
{
  log_msg ("Quit requested");

  send_empty_reply (connection, message);

  if (child_pid > 0)
    kill (child_pid, SIGTERM);

  sd_event_exit (event, 0);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static void
log_msg (const char *fmt, ...)
{
  va_list args;

  printf ("(bazaar-daemon): ");

  va_start (args, fmt);
  vprintf (fmt, args);
  va_end (args);

  printf ("\n");
  fflush (stdout);
}

static void *
malloc_or_bail (size_t n_bytes)
{
  void *p = NULL;

  p = malloc (n_bytes);
  if (p == NULL)
    {
      perror ("malloc");
      _exit (127);
    }

  return p;
}

static char *
build_index_path (void)
{
  const char *cache_home = NULL;
  const char *home       = NULL;
  char        buf[4096]  = { 0 };

  cache_home = getenv ("XDG_CACHE_HOME");
  if (cache_home != NULL && *cache_home != '\0')
    {
      snprintf (buf, sizeof (buf),
                "%s/io.github.kolunmi.Bazaar/core/search-index",
                cache_home);
      return strdup (buf);
    }

  home = getenv ("HOME");
  if (home != NULL && *home != '\0')
    {
      snprintf (buf, sizeof (buf),
                "%s/.cache/io.github.kolunmi.Bazaar/core/search-index",
                home);
      return strdup (buf);
    }

  return strdup ("/tmp/search-index");
}

static void
ensure_index_loaded (void)
{
  if (g_index_path == NULL)
    g_index_path = build_index_path ();

  if (g_index == NULL)
    {
      g_index = search_index_open (g_index_path);
      if (g_index == NULL)
        log_msg ("Search index not yet available at %s", g_index_path);
      return;
    }

  if (search_index_reload_if_stale (&g_index))
    log_msg ("Search index reloaded ");
}

static int
strv_count_local (char **strv)
{
  int n = 0;

  if (strv == NULL)
    return 0;

  while (strv[n] != NULL)
    n++;

  return n;
}

static void
strv_free_local (char **strv)
{
  char **p = NULL;

  if (strv == NULL)
    return;

  for (p = strv; *p != NULL; p++)
    free (*p);
  free (strv);
}

static void
strv_freep (char ***strv)
{
  strv_free_local (*strv);
}

static void
generic_freep (void *p)
{
  free (*(void **) p);
}

static int
on_child_exit (sd_event_source *s,
               const siginfo_t *si,
               void            *userdata)
{
  log_msg ("Application exited (status %d)", si->si_status);

  if (si->si_pid == (pid_t) child_pid)
    child_pid = -1;

  sd_event_source_unref (s);
  ensure_index_loaded ();

  arm_idle_timer ();

  return 0;
}

static void
launch_app (char **extra_args,
            int    n_extra_args)
{
  pid_t                            pid  = -1;
  _cleanup_ (generic_freep) char **argv = NULL;
  int                              i    = 0;
  sigset_t                         mask = { 0 };

  argv    = malloc_or_bail (sizeof (char *) * (size_t) (n_extra_args + 2));
  argv[0] = (char *) BAZAAR_BIN_PATH;
  for (i = 0; i < n_extra_args; i++)
    argv[i + 1] = extra_args[i];
  argv[n_extra_args + 1] = NULL;

  log_msg ("Application starting");

  cancel_idle_timer ();

  pid = fork ();
  if (pid == 0)
    {
      sigemptyset (&mask);
      sigprocmask (SIG_SETMASK, &mask, NULL);

      execvp (argv[0], argv);
      perror ("execvp");
      _exit (127);
    }

  if (pid < 0)
    {
      log_msg ("Failed to spawn application: %s", strerror (errno));
      arm_idle_timer ();
      return;
    }

  if (child_pid <= 0)
    child_pid = pid;

  sd_event_add_child (event, NULL, pid, WEXITED, on_child_exit, NULL);
}

static void
run_update_worker_once (void)
{
  pid_t    pid     = -1;
  char    *argv[3] = { NULL };
  sigset_t mask    = { 0 };

  argv[0] = (char *) BAZAAR_BIN_PATH;
  argv[1] = (char *) UPDATE_WORKER_CLI_OPTION;
  argv[2] = NULL;

  pid = fork ();
  if (pid == 0)
    {
      sigemptyset (&mask);
      sigprocmask (SIG_SETMASK, &mask, NULL);

      execvp (argv[0], argv);
      perror ("execvp");
      _exit (127);
    }

  if (pid < 0)
    {
      log_msg ("Failed to spawn update worker: %s", strerror (errno));
      return;
    }

  sd_event_add_child (event, NULL, pid, WEXITED, on_update_worker_exit, NULL);
}

static int
on_update_worker_exit (sd_event_source *s,
                       const siginfo_t *si,
                       void            *userdata)
{
  sd_event_source_unref (s);
  return 0;
}

static int
on_update_timer (sd_event_source *s,
                 uint64_t         usec,
                 void            *userdata)
{
  run_update_worker_once ();
  schedule_next_update_check ();
  return 0;
}

static int
schedule_next_update_check (void)
{
  uint64_t now = 0;
  int      r   = 0;

  if (update_timer_source != NULL)
    {
      sd_event_source_unref (update_timer_source);
      update_timer_source = NULL;
    }

  r = sd_event_now (event, CLOCK_BOOTTIME, &now);
  if (r < 0)
    return r;

  r = sd_event_add_time (event, &update_timer_source, CLOCK_BOOTTIME,
                         now + UPDATE_CHECK_INTERVAL_USEC, 0,
                         on_update_timer, NULL);
  if (r < 0)
    log_msg ("failed to schedule update: %s", strerror (-r));

  return r;
}

static char **
read_strv_arg (DBusMessageIter *iter)
{
  DBusMessageIter sub;
  char          **result = NULL;
  int             n      = 0;
  int             cap    = 8;

  if (dbus_message_iter_get_arg_type (iter) != DBUS_TYPE_ARRAY)
    return NULL;

  result = malloc_or_bail (sizeof (char *) * (size_t) cap);

  dbus_message_iter_recurse (iter, &sub);
  while (dbus_message_iter_get_arg_type (&sub) == DBUS_TYPE_STRING)
    {
      const char *s = NULL;

      dbus_message_iter_get_basic (&sub, &s);

      if (n + 1 >= cap)
        {
          cap *= 2;
          result = realloc (result, sizeof (char *) * (size_t) cap);
        }

      result[n++] = strdup (s);
      dbus_message_iter_next (&sub);
    }

  result[n] = NULL;
  return result;
}

static void
append_meta (DBusMessageIter *dict_iter, const char *key, const char *value)
{
  DBusMessageIter entry_iter;
  DBusMessageIter variant_iter;

  dbus_message_iter_open_container (dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
  dbus_message_iter_append_basic (&entry_iter, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container (&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
  dbus_message_iter_append_basic (&variant_iter, DBUS_TYPE_STRING, &value);
  dbus_message_iter_close_container (&entry_iter, &variant_iter);
  dbus_message_iter_close_container (dict_iter, &entry_iter);
}

static void
send_empty_reply (DBusConnection *connection, DBusMessage *message)
{
  DBusMessage *reply = NULL;

  reply = dbus_message_new_method_return (message);
  if (reply == NULL)
    return;

  dbus_connection_send (connection, reply, NULL);
  dbus_message_unref (reply);
}

static void
build_and_send_search_reply (DBusConnection *connection,
                             DBusMessage    *call,
                             char          **terms)
{
  DBusMessage     *reply = NULL;
  DBusMessageIter  iter;
  DBusMessageIter  sub;
  SearchIndexMatch matches[MAX_SEARCH_RESULTS] = { 0 };
  size_t           n_matches                   = 0;
  size_t           i                           = 0;

  ensure_index_loaded ();

  n_matches = search_index_query (
      g_index, (const char *const *) terms,
      strv_count_local (terms),
      matches, MAX_SEARCH_RESULTS);

  reply = dbus_message_new_method_return (call);
  if (reply == NULL)
    return;

  dbus_message_iter_init_append (reply, &iter);
  dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, "s", &sub);

  for (i = 0; i < n_matches; i++)
    dbus_message_iter_append_basic (&sub, DBUS_TYPE_STRING, &matches[i].entry->id);

  dbus_message_iter_close_container (&iter, &sub);

  dbus_connection_send (connection, reply, NULL);
  dbus_message_unref (reply);
}

static DBusHandlerResult
handle_get_result_set (DBusConnection *connection, DBusMessage *message)
{
  DBusMessageIter               iter;
  _cleanup_ (strv_freep) char **previous = NULL;
  _cleanup_ (strv_freep) char **terms    = NULL;
  const char                   *sig      = NULL;

  arm_idle_timer ();

  sig = dbus_message_get_signature (message);
  dbus_message_iter_init (message, &iter);

  if (sig != NULL && strcmp (sig, "asas") == 0)
    {
      previous = read_strv_arg (&iter);
      dbus_message_iter_next (&iter);
    }

  terms = read_strv_arg (&iter);
  build_and_send_search_reply (connection, message, terms);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_get_result_metas (DBusConnection *connection, DBusMessage *message)
{
  DBusMessageIter               iter;
  DBusMessage                  *reply = NULL;
  DBusMessageIter               reply_iter;
  DBusMessageIter               array_iter;
  _cleanup_ (strv_freep) char **ids = NULL;
  char                        **p   = NULL;

  arm_idle_timer ();

  dbus_message_iter_init (message, &iter);
  ids = read_strv_arg (&iter);

  ensure_index_loaded ();

  reply = dbus_message_new_method_return (message);
  if (reply == NULL)
    return DBUS_HANDLER_RESULT_HANDLED;

  dbus_message_iter_init_append (reply, &reply_iter);
  dbus_message_iter_open_container (&reply_iter, DBUS_TYPE_ARRAY, "a{sv}", &array_iter);

  for (p = ids; p != NULL && *p != NULL; p++)
    {
      const SearchIndexEntry *e = NULL;
      DBusMessageIter         dict_iter;

      e = search_index_find (g_index, *p);

      if (e == NULL)
        continue;

      dbus_message_iter_open_container (&array_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

      append_meta (&dict_iter, "id", e->id);
      append_meta (&dict_iter, "name", e->title);

      if (e->description != NULL)
        append_meta (&dict_iter, "description", e->description);

      if (e->icon_path != NULL)
        append_meta (&dict_iter, "gicon", e->icon_path);

      dbus_message_iter_close_container (&array_iter, &dict_iter);
    }

  dbus_message_iter_close_container (&reply_iter, &array_iter);

  dbus_connection_send (connection, reply, NULL);
  dbus_message_unref (reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_activate_result (DBusConnection *connection, DBusMessage *message)
{
  DBusMessageIter                 iter;
  const char                     *id      = NULL;
  _cleanup_ (generic_freep) char *uri_arg = NULL;
  char                           *args[1] = { NULL };

  arm_idle_timer ();

  dbus_message_iter_init (message, &iter);
  dbus_message_iter_get_basic (&iter, &id);

  uri_arg = malloc_or_bail (strlen ("appstream:") + strlen (id) + 1);
  sprintf (uri_arg, "appstream:%s", id);

  args[0] = uri_arg;
  launch_app (args, 1);

  send_empty_reply (connection, message);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_launch_search (DBusConnection *connection, DBusMessage *message)
{
  DBusMessageIter                 iter;
  _cleanup_ (strv_freep) char   **terms     = NULL;
  _cleanup_ (generic_freep) char *joined    = NULL;
  _cleanup_ (generic_freep) char *arg       = NULL;
  char                           *args[1]   = { NULL };
  dbus_uint32_t                   timestamp = 0;
  size_t                          len       = 0;
  int                             n_terms   = 0;
  int                             i         = 0;

  arm_idle_timer ();

  dbus_message_iter_init (message, &iter);
  terms = read_strv_arg (&iter);
  dbus_message_iter_next (&iter);
  dbus_message_iter_get_basic (&iter, &timestamp);

  n_terms = strv_count_local (terms);
  if (n_terms == 0)
    {
      launch_app (NULL, 0);
      send_empty_reply (connection, message);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

  for (i = 0; i < n_terms; i++)
    len += strlen (terms[i]) + 1;

  joined    = malloc_or_bail (len);
  joined[0] = '\0';
  for (i = 0; i < n_terms; i++)
    {
      strcat (joined, terms[i]);
      if (i + 1 < n_terms)
        strcat (joined, " ");
    }

  arg = malloc_or_bail (strlen ("--search-for=") + strlen (joined) + 1);
  sprintf (arg, "--search-for=%s", joined);

  args[0] = arg;
  launch_app (args, 1);

  send_empty_reply (connection, message);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static int
on_signal (sd_event_source               *s,
           const struct signalfd_siginfo *si,
           void                          *userdata)
{
  log_msg ("Daemon shutting down");

  if (child_pid > 0)
    kill (child_pid, SIGTERM);

  sd_event_exit (event, 0);

  return 0;
}
