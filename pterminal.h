#ifndef PTERMINAL_H
#define PTERMINAL_H

#include <poll.h>
#include <pway/pway.h>

extern PWay* pway;

extern bool can_draw;

void *run_pterminal(void *none);

int set_terminal_cursor(int cursor);

#endif
