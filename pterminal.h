#ifndef PTERMINAL_H
#define PTERMINAL_H

#include <poll.h>

extern struct pollfd pterminal_fds[3];

void *run_pterminal(void *none);

#endif
