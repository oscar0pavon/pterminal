#include "mouse.h"

#include "wayland.h"
#include <linux/input.h>
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

  if(main_mouse.left_click){
    select_with_mouse(false);
  }

}

static void pointer_handle_button(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time, 
                                  uint32_t button, uint32_t state) {
  WaylandTerminal *term = data;
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
      if (button == BTN_LEFT) {
        main_mouse.left_click = true;
        mouse_click();

      }
      if (button == BTN_RIGHT) {

        printf("right click\n");
      }
  }else{

    if (button == BTN_LEFT) {
      main_mouse.left_click = false;    

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
