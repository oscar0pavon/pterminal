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
void select_with_mouse(bool done);

uint buttonmask(uint);
int mouseaction(XEvent *, uint);

#endif
