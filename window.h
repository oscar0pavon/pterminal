/* See LICENSE for license details. */
#ifndef XWINDOW_H
#define XWINDOW_H

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xft//Xft.h>
#include <GL/glx.h>
#include <time.h>

#include "types.h"

/* macros */
#define IS_WINDOSET(flag) ((terminal_window.mode & (flag)) != 0)

enum win_mode {
	MODE_VISIBLE     = 1 << 0,
	MODE_FOCUSED     = 1 << 1,
	MODE_APPKEYPAD   = 1 << 2,
	MODE_MOUSEBTN    = 1 << 3,
	MODE_MOUSEMOTION = 1 << 4,
	MODE_REVERSE     = 1 << 5,
	MODE_KBDLOCK     = 1 << 6,
	MODE_HIDE        = 1 << 7,
	MODE_APPCURSOR   = 1 << 8,
	MODE_MOUSESGR    = 1 << 9,
	MODE_8BIT        = 1 << 10,
	MODE_BLINK       = 1 << 11,
	MODE_FBLINK      = 1 << 12,
	MODE_FOCUS       = 1 << 13,
	MODE_MOUSEX10    = 1 << 14,
	MODE_MOUSEMANY   = 1 << 15,
	MODE_BRCKTPASTE  = 1 << 16,
	MODE_NUMLOCK     = 1 << 17,
	MODE_MOUSE       = MODE_MOUSEBTN|MODE_MOUSEMOTION|MODE_MOUSEX10\
	                  |MODE_MOUSEMANY,
};

/* types used in config.h */
typedef struct {
  uint mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Shortcut;

typedef struct {
  uint mod;
  uint button;
  void (*func)(const Arg *);
  const Arg arg;
  uint release;
} MouseShortcut;

typedef struct {
  KeySym k;
  uint mask;
  char *s;
  /* three-valued logic variables: 0 indifferent, 1 on, -1 off */
  signed char appkey;    /* application keypad */
  signed char appcursor; /* application cursor */
} Key;

typedef enum window_type {WAYLAND, XORG} WindowType;

/* Purely graphic info */
typedef struct {
  int tty_width, tty_height; /* tty width and height */
  int width, height;   /* window width and height */
  int character_height;     /* char height */
  float character_width;     /* char width  */
  float character_gl_width;
  float character_gl_height;
  int mode;   /* window state/mode flags */
  int cursor; /* cursor style */
  WindowType type;

} TermWindow;

typedef struct {
  Atom xtarget;
  char *primary, *clipboard;
  struct timespec tclick1;
  struct timespec tclick2;
} XSelection;

typedef XftDraw *Draw;


typedef struct {
  Display *display;
  Colormap cmap;
  Window win;
  //OpenGL
  GLXContext gl_context;
  XVisualInfo* visual_info;

  Atom wmdeletewin, netwmname, netwmiconname, netwmpid;
  struct {
    XIM xim;
    XIC xic;
    XPoint spot;
    XVaNestedList spotlist;
  } ime;

  Draw draw;
  Visual *vis;
  XSetWindowAttributes attrs;
  int screen;
  int isfixed; /* is fixed geometry? */
  int left_offset, top_offset;    /* left and top offset */
  int gm;      /* geometry mask */
} XWindow;

extern XWindow xw;
extern XSelection xsel;
extern TermWindow terminal_window;

extern char *opt_class;
extern char **opt_cmd;
extern char *opt_embed;
extern char *opt_font;
extern char *opt_io;
extern char *opt_line;
extern char *opt_name;
extern char *opt_title;

void xbell(void);
void xclipcopy(void);
void xfinishdraw(void);
void xloadcols(void);
int xsetcolorname(int, const char *);
int xgetcolor(int, unsigned char *, unsigned char *, unsigned char *);
void xseticontitle(char *);
void xsettitle(char *);
int xsetcursor(int);
void xsetmode(int, unsigned int);
void xsetpointermotion(int);
void xsetsel(char *);
int xstartdraw(void);
void xximspot(int, int);

void cresize(int width, int height);

void xhints(void);
void resize(XEvent *e);
void expose(XEvent *ev);

/* function definitions used in config.h */
void zoom(const Arg *);
void zoomabs(const Arg *);
void zoomreset(const Arg *);

void create_window(int columns, int rows);

#endif
