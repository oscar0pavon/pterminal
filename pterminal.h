#ifndef PTERMINAL_H
#define PTERMINAL_H

#include <poll.h>
#include <pway/pway.h>
extern struct pollfd pterminal_fds[3];

extern PWay* pway;
void *run_pterminal(void *none);

#endif
