#ifndef WMOUSE_H
#define WMOUSE_H

#include <stdint.h>

typedef struct Mouse {
  uint32_t x;
  uint32_t y;
  bool left_click;
  bool right_click;

}Mouse;

void configure_mouse(void);

extern Mouse main_mouse;

#endif
