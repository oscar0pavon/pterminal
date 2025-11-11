#ifndef COLOR_H
#define COLOR_H

#include <GL/gl.h>
#include <X11/Xft/Xft.h>
#include "terminal.h"

#define TRUERED(x) (((x) & 0xff0000) >> 8)
#define TRUEGREEN(x) (((x) & 0xff00))
#define TRUEBLUE(x) (((x) & 0xff) << 8)

#define TRUECOLOR(r, g, b) (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x) (1 << 24 & (x))

typedef struct PColor{
  GLfloat r;
  GLfloat g;
  GLfloat b;
} PColor;

typedef XftColor Color;
typedef struct RenderColor{
  Color truefg, truebg;
  Color revfg, revbg;
  PColor gl_background_color;
  PColor gl_foreground_color;

}RenderColor;

void get_color_from_glyph(PGlyph* base, RenderColor* out);

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b);

int xloadcolor(int i, const char *name, Color *ncolor);

void xloadcols(void);

int xsetcolorname(int x, const char *name);


#endif
