#ifndef WMOUSE_H
#define WMOUSE_H

#include <stdint.h>
#include <stdbool.h>

#include "wayland.h"

#define Button1			1
#define Button2			2
#define Button3			3
#define Button4			4
#define Button5			5

typedef struct MouseButton{
  uint32_t id;
  bool pressed;
  bool released;
}MouseButton;

typedef struct Mouse {
  uint32_t x;
  uint32_t y;
  uint32_t old_col;
  uint32_t old_row;
  uint32_t col;
  uint32_t row;

  MouseButton* current_button;

  MouseButton left_button;
  MouseButton right_button;
  MouseButton middle_button;

  MouseButton wheel_up;
  MouseButton wheel_down;
  
  uint32_t last_input_serial;

}Mouse;


void configure_mouse(void);
void release_mouse_button();

void clean_mouse_buttons();

extern Mouse main_mouse;

#endif
