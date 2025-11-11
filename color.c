#include <X11/extensions/Xrender.h>
#include <stdbool.h>
#include <stdio.h>
#include "color.h"
#include "types.h"
#include "window.h"
#include "macros.h"
#include "draw.h"
#include "terminal.h"

/* Terminal colors (16 first used in escape sequence) */
//in HTML notation #cc24cd
const uint32_t my_colors[] = {
	/* 8 normal colors */
  0x000000,//black
  0xcc241d,//red
	0x00cd00,//green
	0xcdcd00,//yellow
	0x0c5576,//blue
	0xcd00cd,//magenta
	0x00cdcd,//cyan
	0xe5e5e5,//gray

	/* 8 bright colors */
  0x7f7f7f, //gray
	0xcc241d,//red2
	0x98971a,//green
	0xffff00,//"yellow",
	0x458588,//blue
	0xff00ff, //"magenta",
	0x00ffff, //"cyan",
	0xffffff, //"white",

	[255] = 0, 

	/* more colors can be added after 255 to use with DefaultXX */
	0xcccccc,
	0x555555,
	0xdfdfdf, /* default foreground colour */
	0x282C34, /* default background colour */

};

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

// Function to convert an 0xRRGGBB integer to an XftColor
int hexToXftColor(unsigned int hexValue, XftColor *out_color) {

    // Extract R, G, B components (8-bit values)
    unsigned int r8 = (hexValue >> 16) & 0xFF;
    unsigned int g8 = (hexValue >> 8) & 0xFF;
    unsigned int b8 = hexValue & 0xFF;

    XftColor xft_color;
    // Convert to 16-bit values expected by XftColorAllocValue (0 to 65535)
    XRenderColor new_color;
    new_color.red = (unsigned short)(r8 * 257);   // or (r8 << 8) | r8
    new_color.green = (unsigned short)(g8 * 257); // 255 * 257 = 65535
    new_color.blue = (unsigned short)(b8 * 257);
    new_color.alpha = 0xFFFF; // Full opacity
    xft_color.color = new_color;

    memcpy(out_color,&xft_color,sizeof(XftColor));


    return 1; // Success
}

int xloadcolor(int i, const char *name, Color *out_color) {
  XRenderColor color = {.alpha = 0xffff};
  Color new_color;

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
      new_color.color = color;

      memcpy(out_color, &new_color, sizeof(XftColor));
      // return XftColorAllocValue(xw.display, xw.vis, xw.cmap, &color, ncolor);
      return 1;
    } else {
      name = colorname[i];
      printf("Color name: %s\n", name);
      hexToXftColor(my_colors[i], &new_color);
      memcpy(out_color, &new_color, sizeof(XftColor));
    }
  }

  // return XftColorAllocName(xw.display, xw.vis, xw.cmap, name, out_color);
  return 1;
}

void xloadcols(void) {
  int i;
  static int loaded;
  Color *cp;

  if (loaded) {
    // for (cp = drawing_context.colors; cp < &drawing_context.colors[drawing_context.collen]; ++cp)
    //   XftColorFree(xw.display, xw.vis, xw.cmap, cp);
    printf("Reset colors\n");
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

  printf("Color name: %s\n",name);

  if (!xloadcolor(x, name, &ncolor))
    return 1;

  XftColorFree(xw.display, xw.vis, xw.cmap, &drawing_context.colors[x]);
  drawing_context.colors[x] = ncolor;

  return 0;
}

void get_color_from_glyph(PGlyph* base, RenderColor* out){

  Color *fg, *bg, *temp;
  XRenderColor colfg, colbg;


  if (IS_TRUECOL(base->fg)) {
    colfg.alpha = 0xffff;
    colfg.red = TRUERED(base->fg);
    colfg.green = TRUEGREEN(base->fg);
    colfg.blue = TRUEBLUE(base->fg);
    memcpy(&out->truefg.color,&colfg,sizeof(XRenderColor));
    fg = &out->truefg;
  } else {
    fg = &drawing_context.colors[base->fg];
  }

  if (IS_TRUECOL(base->bg)) {
    colbg.alpha = 0xffff;
    colbg.green = TRUEGREEN(base->bg);
    colbg.red = TRUERED(base->bg);
    colbg.blue = TRUEBLUE(base->bg);
    memcpy(&out->truebg.color,&colbg,sizeof(XRenderColor));
    bg = &out->truebg;
  } else {
    bg = &drawing_context.colors[base->bg];
  }

  /* Change basic system colors [0-7] to bright system colors [8-15] */
  if ((base->mode & ATTR_BOLD_FAINT) == ATTR_BOLD && BETWEEN(base->fg, 0, 7))
    fg = &drawing_context.colors[base->fg + 8];

  if (IS_WINDOSET(MODE_REVERSE)) {
    if (fg == &drawing_context.colors[defaultfg]) {
      fg = &drawing_context.colors[defaultbg];
    } else {
      colfg.red = ~fg->color.red;
      colfg.green = ~fg->color.green;
      colfg.blue = ~fg->color.blue;
      colfg.alpha = fg->color.alpha;
      memcpy(&out->revfg.color,&colbg,sizeof(XRenderColor));
      fg = &out->revfg;
    }

    if (bg == &drawing_context.colors[defaultbg]) {
      bg = &drawing_context.colors[defaultfg];
    } else {
      colbg.red = ~bg->color.red;
      colbg.green = ~bg->color.green;
      colbg.blue = ~bg->color.blue;
      colbg.alpha = bg->color.alpha;
      memcpy(&out->revbg.color,&colbg,sizeof(XRenderColor));
      bg = &out->revbg;
    }
  }

  if ((base->mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
    colfg.red = fg->color.red / 2;
    colfg.green = fg->color.green / 2;
    colfg.blue = fg->color.blue / 2;
    colfg.alpha = fg->color.alpha;
    memcpy(&out->revfg.color,&colfg,sizeof(XRenderColor));
    fg = &out->revfg;
  }

  if (base->mode & ATTR_REVERSE) {
    temp = fg;
    fg = bg;
    bg = temp;
  }

  if (base->mode & ATTR_BLINK && terminal_window.mode & MODE_BLINK)
    fg = bg;

  if (base->mode & ATTR_INVISIBLE)
    fg = bg;

  // opengl convertion
  float div;

  if (bg->color.red > 255 || bg->color.blue > 255 || bg->color.green > 255) {
    div = 65525.f;
  } else {
    div = 255.f;
  }

  PColor background = {.r = bg->color.red / div,
                       .g = bg->color.green / div,
                       .b = bg->color.blue / div};

  if (fg->color.red > 255 || fg->color.blue > 255 || fg->color.green > 255) {
    div = 65525.f;
  } else {
    div = 255.f;
  }

  PColor foreground = {.r = fg->color.red / div,
                       .g = fg->color.green / div,
                       .b = fg->color.blue / div};

  
  out->gl_background_color = background;
  out->gl_foreground_color = foreground;

}
