#ifndef XORG_H
#define XORG_H

#include <stdbool.h>

void create_x_window(void);

void wait_for_mapping();

bool handle_xorg_events();

#endif
