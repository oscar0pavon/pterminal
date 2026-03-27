#ifndef PTERMINAL_H
#define PTERMINAL_H

#include <poll.h>
#include <pway/pway.h>

extern PWay* pway;
void *run_pterminal(void *none);

int set_terminal_cursor(int cursor);

#endif
