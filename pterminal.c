#include "pterminal.h"

#include <stdbool.h>
#include <sys/poll.h>
#include <unistd.h>
#include "window.h"


#include "draw.h"

bool can_draw;

static char *shell = "/bin/sh";

int set_terminal_cursor(int cursor) {
  if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
    return 1;
  terminal_window.cursor = cursor;
  return 0;
}

