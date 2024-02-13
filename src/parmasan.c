
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

#define PARMASAN_SYNC        "SYNC  "
#define PARMASAN_ASYNC       "ASYNC "
#define PARMASAN_ENV_VAR     "PARMASAN_DAEMON_SOCK"
#define TARGET_PID_MESSAGE   "TARGET_PID   "
#define DEPENDENCY_MESSAGE   "DEPENDENCY   "
#define GOAL_MESSAGE         "GOAL         "
#define FINISH_MESSAGE       "FINISH       "

static char parmasan_socket_buffer[PATH_MAX * 4];
static char *parmasan_message_buffer = parmasan_socket_buffer + sizeof (PARMASAN_SYNC) - 1;
#define PARMASAN_MAX_MSG_LEN (sizeof(parmasan_socket_buffer) - sizeof(PARMASAN_SYNC) + 1)
static int parmasan_socket = -1;

const char *parmasan_strategy;

static void parmasan_perror_abort (const char *error)
{
  perror (error);
  exit (EXIT_FAILURE);
}

static void parmasan_error_abort (const char *error)
{
  fprintf (stderr, "Parmasan error: %s\n", error);
  exit (EXIT_FAILURE);
}

static void
open_parmasan_socket (void)
{
  // Read the socket path
  char *sock_str = getenv (PARMASAN_ENV_VAR);
  if (sock_str == NULL)
	parmasan_error_abort (PARMASAN_ENV_VAR " environment variable not set\n");

  if (*sock_str == '\0')
	parmasan_error_abort (PARMASAN_ENV_VAR " environment variable must not be empty\n");

  struct sockaddr_un server_address = {};
  server_address.sun_family = AF_UNIX;

  size_t socket_length = strlen (sock_str);

  if (socket_length >= sizeof (server_address.sun_path))
	socket_length = sizeof (server_address.sun_path) - 1;

  memcpy (server_address.sun_path, sock_str, socket_length);

  if (sock_str[0] == '$')
	server_address.sun_path[0] = '\0';

  parmasan_socket = socket (AF_UNIX, SOCK_SEQPACKET, 0);
  if (parmasan_socket < 0)
	parmasan_perror_abort ("Failed to create a socket for parmasan\n");

  int connection_result = connect (parmasan_socket, (const struct sockaddr *) &server_address,
								   sizeof (server_address.sun_family) + socket_length);

  if (connection_result < 0)
	parmasan_perror_abort ("Failed to connect to parmasan daemon\n");
}

void
init_parmasan (void)
{
  bool parmasan_enabled = false;

  if (parmasan_strategy == NULL)
	parmasan_strategy = "env";

  if (strcmp (parmasan_strategy, "env") == 0)
	parmasan_enabled = getenv (PARMASAN_ENV_VAR) != NULL;
  else if (strcmp (parmasan_strategy, "disable") == 0)
	parmasan_enabled = false;
  else if (strcmp (parmasan_strategy, "require") == 0)
	parmasan_enabled = true;
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
		parmasan_perror_abort ("Failed to read from parmasan fd");

	  if (length < 4 || strcmp (buffer, "ACK") != 0)
		parmasan_error_abort ("Malformed acknowledge packet received "
							  "from parmasan daemon");
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

static void parmasan_socket_send (int len, bool sync)
{
  if (sync)
	memcpy (parmasan_socket_buffer, PARMASAN_SYNC, sizeof (PARMASAN_SYNC) - 1);
  else
	memcpy (parmasan_socket_buffer, PARMASAN_ASYNC, sizeof (PARMASAN_ASYNC) - 1);

  if (send (parmasan_socket, parmasan_socket_buffer, len + sizeof (PARMASAN_SYNC) - 1, 0) < 0)
	parmasan_perror_abort ("Failed to send data to parmasan socket");

  if (sync)
	parmasan_wait_for_acknowledge_packet ();
}

void
parmasan_socket_report_dependency (const char *target, const char *dependency)
{
  int len = 0;
  if (parmasan_socket == -1)
	return;

  len += snprintf (parmasan_message_buffer, PARMASAN_MAX_MSG_LEN, "%s %zu %s %zu %s",
				   DEPENDENCY_MESSAGE, strlen (target),
				   target, strlen (dependency), dependency);

  parmasan_socket_send (len, false);
}

void
parmasan_socket_report_target_pid (pid_t pid, const char *name)
{
  int len = 0;
  if (parmasan_socket == -1)
	return;

  len += snprintf (parmasan_message_buffer, PARMASAN_MAX_MSG_LEN, "%s %zu %s %d",
				   TARGET_PID_MESSAGE, strlen (name),
				   name, pid);

  parmasan_socket_send (len, true);
}

void
parmasan_socket_report_goal (const char *name)
{
  int len = 0;
  if (parmasan_socket == -1)
	return;

  len += snprintf (parmasan_message_buffer, PARMASAN_MAX_MSG_LEN, "%s %zu %s",
				   GOAL_MESSAGE, strlen (name),
				   name);

  parmasan_socket_send (len, true);
}

void
parmasan_socket_send_init_packet (void)
{
  int len = 0;
  if (parmasan_socket == -1)
	return;

  len = snprintf (parmasan_message_buffer, PARMASAN_MAX_MSG_LEN, "INIT MAKE");
  parmasan_socket_send (len, true);
}