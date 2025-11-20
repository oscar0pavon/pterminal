#include "wayland.h"
#include "protocol.h"
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <pthread.h>
#include "input.h"

#include "../window.h"

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
void configure_surface(void* data, DesktopSurface *surface, uint32_t serial){

  WaylandTerminal* terminal = (WaylandTerminal*)data;

  xdg_surface_ack_configure(surface, serial);

}

void configure_window(void* data, struct xdg_toplevel* window, int width, 
    int height, struct wl_array *state){

  WaylandTerminal* terminal = data;

  printf("Compositor suggested size %i %i\n",width, height); 

  is_window_configured = true;
  if(width == 0 && height == 0){
    can_draw = true;
    return;
  }


  terminal_window.width = width;
  terminal_window.height = height;
  cresize(terminal_window.width, terminal_window.height);
  can_draw = true;
  printf("resized\n");

}

SurfaceListener surface_listener = {
  .configure = configure_surface
};

WindowListener window_listener = {
  .configure = configure_window
};

void register_global(void *data, Registry *registry, uint32_t name_id,
    const char *interface_name, uint32_t version) {

  WaylandTerminal *terminal = (WaylandTerminal*)data;

  if (strcmp(interface_name, wl_compositor_interface.name) == 0) {

    initialization.compositor = true;
    terminal->compositor =
        wl_registry_bind(registry, name_id, &wl_compositor_interface, 1);
    printf("Got compositor\n");

  } else if (strcmp(interface_name, xdg_wm_base_interface.name) == 0) {

    initialization.desktop = true;

    terminal->desktop =
        wl_registry_bind(registry, name_id, &xdg_wm_base_interface, 1);

  } else if (strcmp(interface_name, wl_shm_interface.name) == 0) {

    wl_registry_bind(registry, name_id, &wl_shm_interface, 1);

    printf("found shared memory\n");

  } else if (strcmp(interface_name, wl_seat_interface.name) == 0) {

    terminal->seat = wl_registry_bind(registry, name_id, &wl_seat_interface, 1);

    configure_input(&wayland_terminal);

    printf("found seat\n");
  }
}

void remove_global(void *data, Registry *registry, uint32_t name) {}

RegistryListener registry_listener = {
  .global = register_global,
  .global_remove = remove_global
};

void* run_wayland_loop(void*none){
  
  while(wl_display_dispatch(wayland_terminal.display)){
   //events 
  }

  return NULL;
}

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

  wayland_terminal.wayland_surface =
      wl_compositor_create_surface(wayland_terminal.compositor);

  wayland_terminal.desktop_surface = xdg_wm_base_get_xdg_surface(
      wayland_terminal.desktop, wayland_terminal.wayland_surface);

  xdg_surface_add_listener(wayland_terminal.desktop_surface, &surface_listener,
                           &wayland_terminal);

  wayland_terminal.window = xdg_surface_get_toplevel(wayland_terminal.desktop_surface);
  
  xdg_toplevel_add_listener(wayland_terminal.window, &window_listener, &wayland_terminal);

  xdg_toplevel_set_title(wayland_terminal.window, "pterminal");

 
  wl_surface_commit(wayland_terminal.wayland_surface);//TODO

  wl_display_roundtrip(wayland_terminal.display);

  pthread_t wayland_loop_id;
  pthread_create(&wayland_loop_id, NULL, run_wayland_loop, NULL);


  return true;
}
