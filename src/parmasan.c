
#include "parmasan.h"
#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

static char parmasan_socket_buffer[PATH_MAX * 4];
static int parmasan_socket = -1;

#define MAKE_MESSAGE_PREFIX "MAKE   "
#define TARGET_PID_MESSAGE   "TARGET_PID   "
#define DEPENDENCY_MESSAGE   "DEPENDENCY   "
#define FINISH_MESSAGE       "FINISH       "

const char* parmasan_env_var = "PARMASAN_DAEMON_FD";
const char* parmasan_strategy;

typedef enum
{
  CONNECTION_STATE_UNINITIALIZED = 0,
  CONNECTION_STATE_TRACER_PROCESS = 1,
  CONNECTION_STATE_MAKE_PROCESS = 2,
  CONNECTION_STATE_DONE = 3
} connection_state_t;

static void parmasan_perror_abort (const char* error)
{
  perror (error);
  exit (EXIT_FAILURE);
}

static void parmasan_error_abort (const char* error)
{
  fprintf (stderr, "Parmasan error: %s\n", error);
  exit (EXIT_FAILURE);
}

static void
open_parmasan_socket (void)
{
  // Read the parmasan fd from the environment.
  const char *parmasan_fd_env = getenv (parmasan_env_var);
  if (parmasan_fd_env == NULL)
  {
    parmasan_error_abort ("PARMASAN_DAEMON_FD environment variable not set");
  }
  parmasan_socket = atoi (parmasan_fd_env);
  if (parmasan_socket < 0)
  {
    parmasan_error_abort ("PARMASAN_DAEMON_FD environment variable is invalid");
  }
}

void
init_parmasan (void)
{
  bool parmasan_enabled = false;

  if (parmasan_strategy == NULL)
    {
      parmasan_strategy = "env";
    }

  if (strcmp (parmasan_strategy, "env") == 0)
    {
      parmasan_enabled = getenv (parmasan_env_var) != NULL;
    }
  else if (strcmp (parmasan_strategy, "disable") == 0)
    {
      parmasan_enabled = false;
    }
  else if (strcmp (parmasan_strategy, "require") == 0)
    {
      parmasan_enabled = true;
    }
  else
    {
      fprintf (stderr, "Unknown parmasan strategy: %s\n", parmasan_strategy);
      exit (EXIT_FAILURE);
    }

  if (parmasan_enabled)
    {
      open_parmasan_socket ();
      parmasan_socket_send_init_packet ();
    }
}

int
parmasan_wait_for_acknowledge_packet (void)
{
  char buffer[8] = {};
  ssize_t length = 0;

  if (parmasan_socket == -1)
    return -1;

  while (1)
    {
      length = read (parmasan_socket, buffer, sizeof (buffer));
      if (length < 0)
	{
	  parmasan_perror_abort ("Failed to read from parmasan fd");
	}

      if (length < 4 || strcmp (buffer, "ACK") != 0)
	{
	  parmasan_error_abort ("Malformed acknowledge packet received "
				"from parmasan daemon");
	}
      break;
    }

  return 0;
}

void
parmasan_socket_deinitialize (void)
{
  if (parmasan_socket == -1)
      return;

  close (parmasan_socket);
  parmasan_socket = -1;
}

static void parmasan_socket_send (const char* data, int len)
{
  if (send (parmasan_socket, data, len, 0) < 0)
    {
      parmasan_perror_abort ("Failed to write to parmasan fd");
    }
}

void
parmasan_socket_report_dependency (const char *target, const char *dependency)
{
  int len = 0;
  if (parmasan_socket == -1)
    return;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d %s %zu %s %zu %s", getpid (),
                   DEPENDENCY_MESSAGE, strlen (target),
                   target, strlen (dependency), dependency);

  parmasan_socket_send (parmasan_socket_buffer, len);
}

void
parmasan_socket_report_target_pid (pid_t pid, const char *name)
{
  int len = 0;
  if (parmasan_socket == -1)
    return;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d %s %zu %s %d", getpid (),
                   TARGET_PID_MESSAGE, strlen (name),
                   name, pid);

  parmasan_socket_send (parmasan_socket_buffer, len);
  parmasan_wait_for_acknowledge_packet ();
}

void
parmasan_socket_send_init_packet (void)
{
  int len = 0;
  if (parmasan_socket == -1)
    return;

  len += snprintf (parmasan_socket_buffer, sizeof (parmasan_socket_buffer),
                   MAKE_MESSAGE_PREFIX "%7d INIT", getpid ());

  parmasan_socket_send (parmasan_socket_buffer, len);
  parmasan_wait_for_acknowledge_packet ();
}