#include "color.h"
#include "types.h"
#include "window.h"
#include "macros.h"
#include "draw.h"
#include "terminal.h"

/* Terminal colors (16 first used in escape sequence) */
const char *colorname[] = {
	/* 8 normal colors */
	"black",
	"#cc241d",//red
	"green3",
	"yellow3",
	"#0c5576",//blue
	"magenta3",
	"cyan3",
	"gray90",

	/* 8 bright colors */
	"gray50",
	"#cc241d",//red2
	"#98971a",//green
	"yellow",
	"#458588",//blue
	"magenta",
	"cyan",
	"white",

	[255] = 0,

	/* more colors can be added after 255 to use with DefaultXX */
	"#cccccc",
	"#555555",
	"gray90", /* default foreground colour */
	"#282C34", /* default background colour */
};


/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
unsigned int defaultfg = 258;
unsigned int defaultbg = 259;
unsigned int defaultcs = 256;


ushort sixd_to_16bit(int x) { return x == 0 ? 0 : 0x3737 + 0x2828 * x; }

int xloadcolor(int i, const char *name, Color *ncolor) {
  XRenderColor color = {.alpha = 0xffff};

  if (!name) {
    if (BETWEEN(i, 16, 255)) {  /* 256 color */
      if (i < 6 * 6 * 6 + 16) { /* same colors as xterm */
        color.red = sixd_to_16bit(((i - 16) / 36) % 6);
        color.green = sixd_to_16bit(((i - 16) / 6) % 6);
        color.blue = sixd_to_16bit(((i - 16) / 1) % 6);
      } else { /* greyscale */
        color.red = 0x0808 + 0x0a0a * (i - (6 * 6 * 6 + 16));
        color.green = color.blue = color.red;
      }
      return XftColorAllocValue(xw.display, xw.vis, xw.cmap, &color, ncolor);
    } else
      name = colorname[i];
  }

  return XftColorAllocName(xw.display, xw.vis, xw.cmap, name, ncolor);
}

void xloadcols(void) {
  int i;
  static int loaded;
  Color *cp;

  if (loaded) {
    for (cp = drawing_context.colors; cp < &drawing_context.colors[drawing_context.collen]; ++cp)
      XftColorFree(xw.display, xw.vis, xw.cmap, cp);
  } else {
    drawing_context.collen = MAX(LEN(colorname), 256);
    drawing_context.colors = xmalloc(drawing_context.collen * sizeof(Color));
  }

  for (i = 0; i < drawing_context.collen; i++)
    if (!xloadcolor(i, NULL, &drawing_context.colors[i])) {
      if (colorname[i])
        die("could not allocate color '%s'\n", colorname[i]);
      else
        die("could not allocate color %d\n", i);
    }
  loaded = 1;
}

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b) {
  if (!BETWEEN(x, 0, drawing_context.collen - 1))
    return 1;

  *r = drawing_context.colors[x].color.red >> 8;
  *g = drawing_context.colors[x].color.green >> 8;
  *b = drawing_context.colors[x].color.blue >> 8;

  return 0;
}

int xsetcolorname(int x, const char *name) {
  Color ncolor;

  if (!BETWEEN(x, 0, drawing_context.collen - 1))
    return 1;

  if (!xloadcolor(x, name, &ncolor))
    return 1;

  XftColorFree(xw.display, xw.vis, xw.cmap, &drawing_context.colors[x]);
  drawing_context.colors[x] = ncolor;

  return 0;
}
