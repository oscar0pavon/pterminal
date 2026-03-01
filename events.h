#ifndef EVENTS_H
#define EVENTS_H

#include "window.h"

void xseturgency(int);
void visibility(XEvent *);
void unmap(XEvent *);
void cmessage(XEvent *);
void focus(XEvent *);


void focus_window(bool is_focuses);

extern void (*event_handler[LASTEvent])(XEvent *);

#endif
