
#include "parmasan.h"
#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

static const char *PARMASAN_SOCKET_PATH = "\0parmasan-socket";
static char *parmasan_socket_buffer = NULL;
static int parmasan_socket_buffer_size = 0;
static int parmasan_socket_buffer_capacity = 0;
static int parmasan_socket = 0;

typedef enum
{
  CONNECTION_STATE_UNINITIALIZED = 0,
  CONNECTION_STATE_TRACER_PROCESS = 1,
  CONNECTION_STATE_MAKE_PROCESS = 2,
  CONNECTION_STATE_DONE = 3
} connection_state_t;

typedef enum
{
  MAKE_EVENT_TARGET_PID = 0,
  MAKE_EVENT_DEPENDENCY = 1,
  MAKE_EVENT_DONE = 2
} make_event_t;

int
open_parmasan_socket (void)
{
  struct sockaddr_un server_address = {};
  int connection_result;
  parmasan_socket = socket (AF_UNIX, SOCK_SEQPACKET, 0);
  if (parmasan_socket < 0)
    {
      perror ("Failed to create socket");
      return -1;
    }

  server_address.sun_family = AF_UNIX;
  size_t path_length = strlen (PARMASAN_SOCKET_PATH + 1) + 1;

  if (path_length >= sizeof (server_address.sun_path))
    path_length = sizeof (server_address.sun_path) - 1;

  memcpy (server_address.sun_path, PARMASAN_SOCKET_PATH, path_length);

  connection_result
      = connect (parmasan_socket, (struct sockaddr *)&server_address,
                 sizeof (server_address.sun_family) + path_length);
  if (connection_result < 0)
    {
      perror ("Failed to connect to parmasan daemon");
      close (parmasan_socket);
      parmasan_socket = 0;
      return -1;
    }

  return 1;
}

static int
parmasan_socket_buffer_grow (void)
{
  char *new_buffer;
  int new_capacity = parmasan_socket_buffer_capacity * 2;

  if (new_capacity == 0)
    {
      new_capacity = 16;
    }

  new_buffer = realloc (parmasan_socket_buffer, new_capacity);

  if (!new_buffer)
    {
      return -1;
    }

  parmasan_socket_buffer_capacity = new_capacity;
  parmasan_socket_buffer = new_buffer;
  return 0;
}

static int
parmasan_socket_buffer_ensure_capacity (size_t required_size)
{
  int new_capacity = parmasan_socket_buffer_capacity;
  char *new_buffer;

  if (new_capacity == 0)
    {
      new_capacity = 16;
    }

  while (new_capacity < required_size)
    {
      new_capacity *= 2;
    }

  if (new_capacity != parmasan_socket_buffer_capacity)
    {
      new_buffer = realloc (parmasan_socket_buffer, new_capacity);
      if (!new_buffer)
        {
          return -1;
        }
      parmasan_socket_buffer_capacity = new_capacity;
      parmasan_socket_buffer = new_buffer;
    }
  return 0;
}

static int
parmasan_socket_write_byte (char byte)
{
  return parmasan_socket_write (&byte, sizeof (byte));
}

int
parmasan_socket_write (const void *ptr, size_t size)
{
  size_t required_size = parmasan_socket_buffer_size + size;
  if (parmasan_socket_buffer_ensure_capacity (required_size) < 0)
    {
      return -1;
    }
  memcpy (parmasan_socket_buffer + parmasan_socket_buffer_size, ptr, size);
  parmasan_socket_buffer_size += size;
  return 0;
}

int
parmasan_socket_write_string (const char *string)
{
  char c = '\0';
  do
    {
      c = *string++;
      if (parmasan_socket_buffer_size >= parmasan_socket_buffer_capacity)
        {
          if (parmasan_socket_buffer_grow () < 0)
            return -1;
        }
      parmasan_socket_buffer[parmasan_socket_buffer_size++] = c;
    }
  while (c != '\0');
  return 0;
}

int
parmasan_wait_for_acknowledge_packet (void)
{
  char buffer[8] = {};
  ssize_t length = 0;

  if (!parmasan_socket)
    return;
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
parmasan_socket_flush (void)
{
  send (parmasan_socket, parmasan_socket_buffer, parmasan_socket_buffer_size,
        0);
  parmasan_socket_buffer_size = 0;
}

void
parmasan_socket_deinitialize (void)
{
  free (parmasan_socket_buffer);
  parmasan_socket_buffer = NULL;
  parmasan_socket_buffer_size = 0;
  parmasan_socket_buffer_capacity = 0;
  if (parmasan_socket)
    {
      parmasan_socket_send_done_packet ();
      close (parmasan_socket);
      parmasan_socket = 0;
    }
}

void
parmasan_socket_report_dependency (const char *target, const char *dependency)
{
  parmasan_socket_write_byte (MAKE_EVENT_DEPENDENCY);
  parmasan_socket_write_string (target);
  parmasan_socket_write_string (dependency);
  parmasan_socket_flush ();
}

void
parmasan_socket_report_target_pid (pid_t pid, const char *name)
{
  parmasan_socket_write_byte (MAKE_EVENT_TARGET_PID);
  parmasan_socket_write (&pid, sizeof (pid_t));
  parmasan_socket_write_string (name);
  parmasan_socket_flush ();
  parmasan_wait_for_acknowledge_packet ();
}

void
parmasan_socket_send_done_packet (void)
{
  parmasan_socket_write_byte (MAKE_EVENT_DONE);
  parmasan_socket_flush ();
}

void
parmasan_socket_send_init_packet (void)
{
  pid_t pid = getpid ();
  parmasan_socket_write_byte (CONNECTION_STATE_MAKE_PROCESS);
  parmasan_socket_write (&pid, sizeof (pid_t));
  parmasan_socket_flush ();
  parmasan_wait_for_acknowledge_packet ();
}