#ifndef WMOUSE_H
#define WMOUSE_H

#include <stdint.h>
#include <stdbool.h>

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

}Mouse;


void configure_mouse(void);

extern Mouse main_mouse;

#endif
