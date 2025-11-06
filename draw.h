#ifndef DRAW_H
#define DRAW_H

#include <X11/Xft/Xft.h>
#include "opengl.h"
#include <X11/Xlib.h>

#define TRUERED(x) (((x) & 0xff0000) >> 8)
#define TRUEGREEN(x) (((x) & 0xff00))
#define TRUEBLUE(x) (((x) & 0xff) << 8)

typedef XftColor Color;
typedef struct RenderColor{
  Color* background;
  Color* foreground;
  Color truefg, truebg;
  Color revfg, revbg;
  PColor gl_background_color;
  PColor gl_foreground_color;

}RenderColor;

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


#endif
