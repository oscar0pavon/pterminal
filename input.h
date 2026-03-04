#ifndef INPUT_H
#define INPUT_H

#include <limits.h>

#include "types.h"
#include <X11/Xlib.h>

/* X modifiers */
#define XK_ANY_MOD UINT_MAX
#define XK_NO_MOD 0
#define XK_SWITCH_MOD (1 << 13 | 1 << 14)

char *get_esc_from_special_keys(KeySym key_sym, uint state);
int match(uint mask, uint state);
void numlock(const Arg *);

#endif
