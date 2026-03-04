#include "keyboard.h"
#include "copy_paste.h"
#include "wayland.h"
#include <bits/time.h>
#include <bits/types/struct_itimerspec.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <wayland-client-protocol.h>
#include <unistd.h>
#include <mman.h>
#include <poll.h>
#include "../pterminal.h"
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#include "../terminal.h"
#include "../events.h"
#include "data_copy.h"
#include "mouse.h"

Keyboard main_keyboard;

void stop_repeat_time(){
  struct itimerspec stop = {0};
  timerfd_settime(main_keyboard.timer_fd, 0, &stop, NULL);
  main_keyboard.last_key_sym = 0;
}

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

    focus_window(true);

}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface) {

    focus_window(false);
    stop_repeat_time();

}

void handle_key_sym(xkb_keysym_t sym){

  char buf[32] = {0};
  int len;


  bool ctrl_pressed = xkb_state_mod_name_is_active(main_keyboard.state,
        XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE);
  
  bool shift_pressed = xkb_state_mod_name_is_active(main_keyboard.state,
        XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE);

  if(ctrl_pressed && shift_pressed){
    if(sym == XK_V){
      printf("Paste from clipboard\n");
      paste_from_clipboard(false);
      return;
    }else if(sym == XK_C) {
      perform_copy(main_keyboard.last_input_serial);
      printf("Copy to clipboard\n");
      return;
    }
  }


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
      write_to_tty(buf, len - 1, 1);
    else
      write_to_tty(buf, len, 1);
  }

}

void keyboard_update_timer(){
  if(main_keyboard.rate > 0){
    long time_number = 1000000000L;
    struct itimerspec timer_specs = {0};
    timer_specs.it_value.tv_sec = main_keyboard.delay / 1000;
    timer_specs.it_value.tv_nsec = (main_keyboard.delay % 1000) * 1000000;

    long interval_ns = time_number / main_keyboard.rate;

    timer_specs.it_interval.tv_sec = interval_ns / time_number;
    timer_specs.it_interval.tv_nsec = interval_ns % time_number;

    timerfd_settime(main_keyboard.timer_fd, 0, &timer_specs, NULL);
  }
}
void init_keyboard_reapeat_handler(){
  main_keyboard.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
}


void handle_repeat_keys(){

  if( pterminal_fds[2].revents & POLLIN ){
    uint64_t expirations;
    
    if(read(main_keyboard.timer_fd, &expirations, sizeof(expirations) > 0)) {

      handle_key_sym(main_keyboard.last_key_sym);

    }

  }

}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state) {


  uint32_t usable_key = key + 8; //linux eudev code start plus 8
                                 //
  xkb_keysym_t sym = xkb_state_key_get_one_sym(main_keyboard.state, usable_key);

  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {

    main_keyboard.last_input_serial = serial;

    if(xkb_keymap_key_repeats(main_keyboard.keymap, usable_key)){
      main_keyboard.last_key_sym = sym;
      main_keyboard.last_key_code = usable_key;
      handle_key_sym(sym);
      keyboard_update_timer();
    }else{
      handle_key_sym(sym);
    }

  }else if( state == WL_KEYBOARD_KEY_STATE_RELEASED){

    if(usable_key == main_keyboard.last_key_code){
      stop_repeat_time();
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

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *keyboard,
                                        int32_t rate, int32_t delay) {
  main_keyboard.delay = delay;
  main_keyboard.rate= rate;

  printf("got repeat info\n");
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
