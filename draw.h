#ifndef DRAW_H
#define DRAW_H

#include <X11/Xlib.h>
#include "color.h"
#include <stdbool.h>


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

extern bool updating_window;

void draw(void);

void drawregion(int, int, int, int);

void xdrawglyph(PGlyph glyph, int x, int y);

void xdrawcursor(int, int, PGlyph, int, int, PGlyph);
void xdrawline(Line, int, int, int);

void update_draw_window();

/*
 * thickness of underline and bar cursors
 */
extern unsigned int cursorthickness;

#endif
