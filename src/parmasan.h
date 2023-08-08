
#ifndef REMAKE_PARMASAN_H
#define REMAKE_PARMASAN_H

#include "types.h"

extern const char* parmasan_strategy;

void init_parmasan (void);

void parmasan_socket_deinitialize (void);
void parmasan_socket_report_dependency (const char *target,
                                        const char *dependency);
void parmasan_socket_report_target_pid (pid_t pid, const char *name);
void parmasan_socket_send_init_packet (void);

#endif // REMAKE_PARMASAN_H
