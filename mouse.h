#ifndef MOUSE_H
#define MOUSE_H

#include <X11/Xlib.h>
#include "types.h"

void brelease(XEvent *);

void bpress(XEvent *);

void bmotion(XEvent *);

void propnotify(XEvent *);

void selrequest(XEvent *e);

void selnotify(XEvent *);

void selpaste(const Arg *);

void clipcopy(const Arg *);

void clippaste(const Arg *);

int evcol(XEvent *);
int evrow(XEvent *);

void mouse_click();
void release_button();
void select_with_mouse(bool done);
void handle_mouse_motion();

uint buttonmask(uint);
int mouseaction(XEvent *, uint);

extern uint buttons; /* bit field of pressed buttons */
extern uint current_button_number;

#endif
