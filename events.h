#ifndef EVENTS_H
#define EVENTS_H

#include "window.h"

void xseturgency(int);
void visibility(XEvent *);
void unmap(XEvent *);
void cmessage(XEvent *);
void focus(XEvent *);

#endif
