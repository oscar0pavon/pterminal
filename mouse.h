#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

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


void mouse_click();
void release_button();
void select_with_mouse(bool done);
void handle_mouse_motion(bool has_motion);
void update_mouse_terminal_position();

void init_mouse();

bool is_on_mouse_mode();

extern Mouse main_mouse;

#endif
