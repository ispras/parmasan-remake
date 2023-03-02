
#include "parmasan.h"
#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

static char parmasan_socket_buffer[PATH_MAX * 4];
static int parmasan_socket = -1;

#define MAKE_MESSAGE_PREFIX "MAKE   "
#define TARGET_PID_MESSAGE   "TARGET_PID   "
#define DEPENDENCY_MESSAGE   "DEPENDENCY   "
#define FINISH_MESSAGE       "FINISH       "

typedef enum
{
  CONNECTION_STATE_UNINITIALIZED = 0,
  CONNECTION_STATE_TRACER_PROCESS = 1,
  CONNECTION_STATE_MAKE_PROCESS = 2,
  CONNECTION_STATE_DONE = 3
} connection_state_t;

int
open_parmasan_socket (void)
{
  // Read the parmasan fd from the environment.
  const char *parmasan_fd_env = getenv ("PARMASAN_DAEMON_FD");
  if (parmasan_fd_env == NULL)
    {
      perror ("PARMASAN_DAEMON_FD environment variable not set");
      return -1;
    }
  parmasan_socket = atoi (parmasan_fd_env);
  if (parmasan_socket < 0)
    {
      perror ("PARMASAN_DAEMON_FD environment variable is invalid");
      return -1;
    }

  return 1;
}

int
parmasan_wait_for_acknowledge_packet (void)
{
  char buffer[8] = {};
  ssize_t length = 0;

  if (!parmasan_socket)
    return -1;
  while (1)
    {
      length = read (parmasan_socket, buffer, sizeof (buffer));
      if (length < 0)
        return -1;
      if (strcmp (buffer, "ACK") == 0)
        return 0;
    }
}

void
parmasan_socket_deinitialize (void)
{
  if (parmasan_socket)
    {
      parmasan_socket_send_done_packet ();
      close (parmasan_socket);
      parmasan_socket = -1;
    }
}

void
parmasan_socket_report_dependency (const char *target, const char *dependency)
{
  int len = 0;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d %s %zu %s %zu %s", getpid (),
                   DEPENDENCY_MESSAGE, strlen (target),
                   target, strlen (dependency), dependency);

  send (parmasan_socket, parmasan_socket_buffer, len, 0);
}

void
parmasan_socket_report_target_pid (pid_t pid, const char *name)
{

  int len = 0;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d %s %zu %s %d", getpid (),
                   TARGET_PID_MESSAGE, strlen (name),
                   name, pid);

  send (parmasan_socket, parmasan_socket_buffer, len, 0);
  parmasan_wait_for_acknowledge_packet ();
}

void
parmasan_socket_send_done_packet (void)
{

  int len = 0;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d %s", getpid (),
                   FINISH_MESSAGE);

  send (parmasan_socket, parmasan_socket_buffer, len, 0);
}

void
parmasan_socket_send_init_packet (void)
{
  int len = 0;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d INIT", getpid ());

  send (parmasan_socket, parmasan_socket_buffer, len, 0);
  parmasan_wait_for_acknowledge_packet ();
}