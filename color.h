#ifndef COLOR_H
#define COLOR_H

#include <GL/gl.h>
#include <X11/Xft/Xft.h>

#define TRUERED(x) (((x) & 0xff0000) >> 8)
#define TRUEGREEN(x) (((x) & 0xff00))
#define TRUEBLUE(x) (((x) & 0xff) << 8)

typedef struct PColor{
  GLfloat r;
  GLfloat g;
  GLfloat b;
} PColor;

typedef XftColor Color;
typedef struct RenderColor{
  Color* background;
  Color* foreground;
  Color truefg, truebg;
  Color revfg, revbg;
  PColor gl_background_color;
  PColor gl_foreground_color;

}RenderColor;

extern const char *colorname[];

#endif
