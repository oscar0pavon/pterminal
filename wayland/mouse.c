#include "mouse.h"

#include "wayland.h"
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include "../mouse.h"

Mouse main_mouse = {0};

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy) {
    WaylandTerminal *term = data;
    // Set the cursor image every time we enter the surface
    // wl_pointer_set_cursor(pointer, serial, term->cursor_surface, 
    //                       term->cursor_image->hotspot_x, 
    //                       term->cursor_image->hotspot_y);
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  WaylandTerminal *term = data;
  // Convert fixed-point coordinates to doubles/integers
  main_mouse.x = wl_fixed_to_double(sx);
  main_mouse.y = wl_fixed_to_double(sy);
 
  main_mouse.motion = true;

  handle_mouse_motion();

}

static void pointer_handle_button(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time, 
                                  uint32_t button, uint32_t state) {
  WaylandTerminal *term = data;
  main_mouse.button_pressed = false;
  main_mouse.button_release = false;

  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {

    main_mouse.button_pressed = true;
    main_mouse.button_release = false;
    if (button == BTN_LEFT) {
      main_mouse.left_click = true;

    }
    if (button == BTN_RIGHT) {

      main_mouse.right_click = true;

    }

    mouse_click();

  }else{

    release_button();

    main_mouse.button_pressed = false;
    main_mouse.button_release = true;

    if (button == BTN_LEFT) {
      main_mouse.left_click = false;    

    }
    if( button == BTN_RIGHT ){
      main_mouse.right_click = false;
    }
    
  }
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value) {
    // value > 0 is scroll down/right; value < 0 is scroll up/left
    printf("wheel mouse\n");
}

static void pointer_handle_frame(void *data, struct wl_pointer *pointer) {
    // Grouped events complete; trigger redraw if needed
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface) {}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
    .frame = pointer_handle_frame,
};

void configure_mouse(void){

  wl_pointer_add_listener(wayland_terminal.pointer, &pointer_listener, &wayland_terminal);
  
}
