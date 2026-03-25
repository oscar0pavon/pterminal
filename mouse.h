#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>


typedef struct Mouse {
  uint32_t old_col;
  uint32_t old_row;
  uint32_t col;
  uint32_t row;
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
