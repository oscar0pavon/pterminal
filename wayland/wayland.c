#include "wayland.h"
#include "protocol.h"
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client-core.h>


WaylandTerminal wayland_terminal = {};

typedef struct WaylandInitialization{
  bool compositor;
  bool desktop;
}WaylandInitialization;

WaylandInitialization initialization;

bool handle_wayland_initialization(){
  if(!initialization.compositor){
    printf("Can't load compositor\n");
    return false;
  }
  if(!initialization.desktop){
    printf("Can't load desktop\n");
    return false;
  }

  return true;
}

void register_global(void *data, Registry *registry, uint32_t name,
    const char *interface_name, uint32_t version) {

  WaylandTerminal *terminal = (WaylandTerminal*)data;

  if(strcmp(interface_name, wl_compositor_interface.name) == 0){
    initialization.compositor = true;
  }else if (strcmp(interface_name, xdg_wm_base_interface.name) == 0){
    initialization.desktop = true;
  }
}

void remove_global(void *data, Registry *registry, uint32_t name) {}

RegistryListener registry_listener = {
  .global = register_global,
  .global_remove = remove_global
};

bool init_wayland() {
  wayland_terminal.display = wl_display_connect(NULL);
  if(wayland_terminal.display == NULL){
    printf("Can't get Wayland display, trying Xorg\n");
    return false;
  }

  wayland_terminal.registry = wl_display_get_registry(wayland_terminal.display);

  wl_registry_add_listener(wayland_terminal.registry, &registry_listener, 
      &wayland_terminal);

  wl_display_roundtrip(wayland_terminal.display);


  bool init = handle_wayland_initialization();
  if(!init)
    return false;

  
  

  return true;
}
