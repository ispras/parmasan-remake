
#ifndef REMAKE_PARMASAN_H
#define REMAKE_PARMASAN_H

#include "types.h"


int open_parmasan_socket (void);
void parmasan_socket_deinitialize (void);
void parmasan_socket_report_dependency (const char *target,
                                        const char *dependency);
void parmasan_socket_report_target_pid (pid_t pid, const char *name);
void parmasan_socket_send_init_packet (void);
void parmasan_socket_send_done_packet (void);

#endif // REMAKE_PARMASAN_H
