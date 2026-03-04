#ifndef INPUT_H
#define INPUT_H

#include <limits.h>

#include "types.h"
#include <xkbcommon/xkbcommon.h>

/* X modifiers */
#define XK_ANY_MOD UINT_MAX
#define XK_NO_MOD 0
#define XK_SWITCH_MOD (1 << 13 | 1 << 14)

#define ShiftMask		(1<<0)
#define LockMask		(1<<1)
#define ControlMask		(1<<2)
#define Mod1Mask		(1<<3)
#define Mod2Mask		(1<<4)
#define Mod3Mask		(1<<5)
#define Mod4Mask		(1<<6)
#define Mod5Mask		(1<<7)

char *get_esc_from_special_keys(xkb_keysym_t key_sym, uint state);
int match(uint mask, uint state);
void numlock(const Arg *);

#endif
