#ifndef DRAW_H
#define DRAW_H

#include <X11/Xlib.h>
#include "color.h"


/* Drawing Context */
typedef struct {
  Color *colors;
  size_t colors_count;
  Font font, bfont, ifont, ibfont;
  GC gc;
} DC;

extern DC drawing_context;

extern int borderpx;

extern unsigned int defaultrcs;

void draw(void);

void drawregion(int, int, int, int);

void xdrawglyph(PGlyph glyph, int x, int y);

void xdrawcursor(int, int, PGlyph, int, int, PGlyph);
void xdrawline(Line, int, int, int);

void init_draw_method(void);
void swap_draw_buffers(void);

/*
 * thickness of underline and bar cursors
 */
extern unsigned int cursorthickness;

#endif
