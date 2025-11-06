#ifndef INPUT_H
#define INPUT_H

#include <limits.h>

#include "types.h"

/* X modifiers */
#define XK_ANY_MOD UINT_MAX
#define XK_NO_MOD 0
#define XK_SWITCH_MOD (1 << 13 | 1 << 14)

int match(uint mask, uint state);
#endif
