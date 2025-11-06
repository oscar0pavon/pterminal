#ifndef DRAW_H
#define DRAW_H

#include <X11/Xft/Xft.h>
#include "opengl.h"

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

void draw(void);


#endif
