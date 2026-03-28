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
void update_mouse();
void send_mouse_info_to_tty();

bool is_on_mouse_mode();

extern Mouse main_mouse;

#endif
