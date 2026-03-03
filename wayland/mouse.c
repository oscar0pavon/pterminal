#include "mouse.h"

#include "wayland.h"
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
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

  update_mouse_terminal_position();
 
  handle_mouse_motion(true);

}

void press_button(){

  if(main_mouse.current_button){

    main_mouse.current_button->pressed = true;
    main_mouse.current_button->released = false;

  }
}

void clean_mouse_buttons(){

  if(main_mouse.current_button){
    if(main_mouse.current_button->released){
      main_mouse.current_button->released = false;
      main_mouse.current_button = NULL;
    }
  }
}

void release_mouse_button(){

  if(main_mouse.current_button){
    
    main_mouse.current_button->pressed = false;
    main_mouse.current_button->released = true;

  }
}

static void pointer_handle_button(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time, 
                                  uint32_t button, uint32_t state) {

  main_mouse.current_button = NULL;

  switch (button) {
      case BTN_LEFT:
                   main_mouse.current_button = &main_mouse.left_button; 
                   main_mouse.current_button->id = 0;
                   break;
      case BTN_MIDDLE:  
                   main_mouse.current_button = &main_mouse.middle_button; 
                   main_mouse.current_button->id = 1;
                   break;
      case BTN_RIGHT:
                   main_mouse.current_button = &main_mouse.right_button; 
                   main_mouse.current_button->id = 2;
                   break;
      default:
                   main_mouse.current_button = NULL;
  }

  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {

    press_button();

    mouse_click();

  }else{

    release_mouse_button();

    release_button();

    
  }
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value) {

  double scroll_delta = wl_fixed_to_double(value);

  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {

    int button_number = (scroll_delta < 0) ? 4 : 5;

    buttons = button_number;

  }

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

  main_mouse.wheel_up.id = 64;
  main_mouse.wheel_down.id = 65;
  
}
