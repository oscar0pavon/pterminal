#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct Mouse {
  uint32_t x;
  uint32_t y;

}Mouse;

extern Mouse main_mouse;


void configure_mouse(void);

#endif
