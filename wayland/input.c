#include "input.h"
#include "keyboard.h"
#include "mouse.h"

#include <stdio.h>

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     uint32_t capabilities) {
  WaylandTerminal *app = data;

  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {

    app->pointer = wl_seat_get_pointer(seat);
    configure_mouse();

  } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {

    wl_pointer_destroy(app->pointer);
    app->pointer = NULL;

  }

  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {

    app->keyboard = wl_seat_get_keyboard(seat);
    configure_keyboard();

  } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) {

    wl_keyboard_destroy(app->keyboard);
    app->keyboard = NULL;

  }
}

static void seat_handle_name(void *data, struct wl_seat *seat,
                             const char *name) {
  //printf("Seat name is: %s\n", name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
};

void configure_input(WaylandTerminal* wayland){

  wl_seat_add_listener(wayland->seat, &seat_listener, wayland);

}
