#ifndef WKEYBOARD_H
#define WKEYBOARD_H


#include <stdint.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/timerfd.h>

typedef struct Keyboard {
  struct wl_keyboard *keyboard;
  struct xkb_context *context;
  struct xkb_keymap *keymap;
  struct xkb_state *state;
  uint32_t rate;
  uint32_t delay;
  xkb_keysym_t last_key_sym;
  uint32_t last_key_code;
  int timer_fd;

} Keyboard;

void configure_keyboard(void);

void init_keyboard_reapeat_handler();

void handle_repeat_keys();

void handle_key_sym(xkb_keysym_t sym);

extern Keyboard main_keyboard;

#endif
