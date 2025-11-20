#include "input.h"
#include "keyboard.h"

#include <stdio.h>

// announced/removed
static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     uint32_t capabilities) {
  WaylandTerminal *app = data;

  printf("Getting seat capabilities\n");

  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {

    printf("Compositor announced pointer capability. Binding pointer.\n");
    app->pointer = wl_seat_get_pointer(seat);
    //add a listener to app->pointer here

  } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {

    printf("Compositor removed pointer capability.\n");
    wl_pointer_destroy(app->pointer);
    app->pointer = NULL;

  }

  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {

    printf("Compositor announced keyboard capability. Binding keyboard.\n");
    app->keyboard = wl_seat_get_keyboard(seat);
    configure_keyboard();

  } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) {

    printf("Compositor removed keyboard capability.\n");
    wl_keyboard_destroy(app->keyboard);
    app->keyboard = NULL;

  }
}

static void seat_handle_name(void *data, struct wl_seat *seat,
                             const char *name) {
  printf("Seat name is: %s\n", name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
};

void configure_input(WaylandTerminal* wayland){

  wl_seat_add_listener(wayland->seat, &seat_listener, wayland);

}
