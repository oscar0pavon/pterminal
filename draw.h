#ifndef DRAW_H
#define DRAW_H

#include "color.h"
#include <stdbool.h>

/* Drawing Context */
typedef struct {
  Color *colors;
  size_t colors_count;
} DC;

extern DC drawing_context;

extern int borderpx;

extern unsigned int defaultrcs;

extern bool can_update_size;

void draw(void);

void drawregion(int, int, int, int);

void draw_glyph(Glyph glyph, int x, int y);

void xdrawcursor(int, int, Glyph, int, int, Glyph);
void xdrawline(Line, int, int, int);

void init_draw_method(void);
void swap_draw_buffers(void);

void update_size();

/*
 * thickness of underline and bar cursors
 */
extern unsigned int cursorthickness;


#endif
