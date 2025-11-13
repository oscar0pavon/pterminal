#include "wayland.h"
#include <stdbool.h>
#include <stdio.h>


WaylandTerminal wayland_terminal = {};

bool init_wayland() {
  wayland_terminal.display = wl_display_connect(NULL);
  if(wayland_terminal.display == NULL){
    printf("Can't get Wayland display, trying Xorg\n");
    return false;
  }

  wayland_terminal.registry = wl_display_get_registry(wayland_terminal.display);

  return true;
}
