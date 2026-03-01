#include "keyboard.h"
#include "wayland.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <wayland-client-protocol.h>
#include <unistd.h>
#include <mman.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#include "../terminal.h"
#include "../events.h"

Keyboard main_keyboard;

static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int32_t fd, uint32_t size) {

  printf("Handling xkbcommon init\n");

  if(format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1){
    close(fd);
  }

  char *memory = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  if(!memory){
    printf("Can't map keyboard memory");
    return;
  }

  main_keyboard.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  main_keyboard.keymap = xkb_keymap_new_from_buffer(
      main_keyboard.context, memory, size, XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);

  main_keyboard.state = xkb_state_new(main_keyboard.keymap);

  munmap(memory, size);
  close(fd);
}

// Handle Enter Event (Focus Gained)
static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys) {
    printf("Keyboard focus gained.\n");
    // Change window appearance to "active"
    focus_window(true);
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface) {
    printf("Keyboard focus lost.\n");
    // Change window appearance to "inactive"
    focus_window(false);
}

// Handle Key Event (Key Press/Release)
static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state) {

  xkb_keysym_t sym = xkb_state_key_get_one_sym(main_keyboard.state, key + 8);

  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    char buf[32] = {0}; // Always initialize to zero to prevent the space bug
    int len;


    bool ctrl_pressed = xkb_state_mod_name_is_active(main_keyboard.state,
          XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE);

    if(ctrl_pressed){
        // Handle Ctrl
        if (sym >= XKB_KEY_a && sym <= XKB_KEY_z) {
            buf[0] = sym - XKB_KEY_a + 1;
            len = 1;
        } else {
            len = xkb_keysym_to_utf8(sym, buf, sizeof(buf));
        }
    } else {

        len = xkb_keysym_to_utf8(sym, buf, sizeof(buf));
    }

    if (len > 0) {
      if (!ctrl_pressed)
        ttywrite(buf, len - 1, 1);
      else
        ttywrite(buf, len, 1);
    }
  }


}

// Handle Modifiers Event
static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group) {

  xkb_state_update_mask(main_keyboard.state, mods_depressed, mods_latched,
                          mods_locked, group, group, group);

}

// Handle Repeat Info
static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *keyboard,
                                        int32_t rate, int32_t delay) {
    // Store repeat rate/delay for your application logic
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info,
};

void configure_keyboard(void){

  wl_keyboard_add_listener(wayland_terminal.keyboard, &keyboard_listener, &wayland_terminal);

}
