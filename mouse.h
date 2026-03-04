#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"



void mouse_click();
void release_button();
void select_with_mouse(bool done);
void handle_mouse_motion(bool has_motion);
void update_mouse_terminal_position();


bool is_on_mouse_mode();

extern uint buttons; /* bit field of pressed buttons */
extern uint current_button_number;

#endif
