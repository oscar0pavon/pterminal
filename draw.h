#ifndef DRAW_H
#define DRAW_H

#include <X11/Xlib.h>
#include "color.h"


/* Drawing Context */
typedef struct {
  Color *colors;
  size_t collen;
  Font font, bfont, ifont, ibfont;
  GC gc;
} DC;

extern DC drawing_context;

extern int borderpx;

extern unsigned int defaultrcs;

void draw(void);

void xdrawglyph(PGlyph glyph, int x, int y);

/*
 * thickness of underline and bar cursors
 */
extern unsigned int cursorthickness;

#endif
