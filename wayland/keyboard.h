#ifndef WKEYBOARD_H
#define WKEYBOARD_H


#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

typedef struct Keyboard {
  struct wl_keyboard *keyboard;
  struct xkb_context *context;
  struct xkb_keymap *keymap;
  struct xkb_state *state;
} Keyboard;

void configure_keyboard(void);

#endif
