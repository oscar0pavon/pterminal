#include "window.h"
#include "opengl.h"
#include "terminal.h"
#include "types.h"
#include "draw.h"

#include <unistd.h>

/* Globals */
XWindow xw;
XSelection xsel;
TermWindow terminal_window;

char *opt_class = NULL;
char **opt_cmd = NULL;
char *opt_embed = NULL;
char *opt_font = NULL;
char *opt_io = NULL;
char *opt_line = NULL;
char *opt_name = NULL;
char *opt_title = NULL;

void create_window(int cols, int rows){
  Window parent, root;
  pid_t thispid = getpid();

  if (!(xw.display = XOpenDisplay(NULL)))
    die("can't open display\n");
  xw.screen = XDefaultScreen(xw.display);

  xw.visual_info = glXChooseVisual(xw.display, xw.screen, gl_attributes);
  if (!xw.visual_info)
    die("can't create glx visual\n");

  xw.cmap = XCreateColormap(xw.display,
                            RootWindow(xw.display, xw.visual_info->screen),
                            xw.visual_info->visual, AllocNone);

  xloadcols();

  terminal_window.character_height = 24;
  terminal_window.character_gl_width = 32;
  terminal_window.character_gl_height = 32;
  terminal_window.character_width = 9;

  /* adjust fixed window geometry */
  terminal_window.width = cols * terminal_window.character_width;
  terminal_window.height = rows * terminal_window.character_height;

  if (xw.gm & XNegative)
    xw.left_offset +=
        DisplayWidth(xw.display, xw.screen) - terminal_window.width - 2;
  if (xw.gm & YNegative)
    xw.top_offset +=
        DisplayHeight(xw.display, xw.screen) - terminal_window.height - 2;

  /* Events */
  xw.attrs.bit_gravity = NorthWestGravity;
  xw.attrs.event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask |
                        ExposureMask | VisibilityChangeMask |
                        StructureNotifyMask | ButtonMotionMask |
                        ButtonPressMask | ButtonReleaseMask;

  xw.attrs.colormap = xw.cmap;

  root = XRootWindow(xw.display, xw.screen);
  if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0))))
    parent = root;

  xw.vis = xw.visual_info->visual;

  printf("Teminal window width %i\n", terminal_window.width);
  printf("Teminal window height %i\n", terminal_window.height);

  xw.win = XCreateWindow(
      xw.display, root, xw.left_offset, xw.top_offset, terminal_window.width,
      terminal_window.height, 0, XDefaultDepth(xw.display, xw.screen),
      InputOutput, xw.vis,
      CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
      &xw.attrs);


  if (parent != root)
    XReparentWindow(xw.display, xw.win, parent, xw.left_offset, xw.top_offset);

  xw.xembed = XInternAtom(xw.display, "_XEMBED", False);
  xw.wmdeletewin = XInternAtom(xw.display, "WM_DELETE_WINDOW", False);
  xw.netwmname = XInternAtom(xw.display, "_NET_WM_NAME", False);
  xw.netwmiconname = XInternAtom(xw.display, "_NET_WM_ICON_NAME", False);
  XSetWMProtocols(xw.display, xw.win, &xw.wmdeletewin, 1);

  xw.netwmpid = XInternAtom(xw.display, "_NET_WM_PID", False);
  XChangeProperty(xw.display, xw.win, xw.netwmpid, XA_CARDINAL, 32,
                  PropModeReplace, (uchar *)&thispid, 1);

  terminal_window.mode = MODE_NUMLOCK;

  resettitle();

  xhints();

  XMapWindow(xw.display, xw.win);

  XSync(xw.display, False);

  clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
  clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
  xsel.primary = NULL;
  xsel.clipboard = NULL;

  xsel.xtarget = XInternAtom(xw.display, "UTF8_STRING", 0);
  if (xsel.xtarget == None)
    xsel.xtarget = XA_STRING;
}

void expose(XEvent *ev) { redraw(); }

/*
 * Absolute coordinates.
 */
void xclear(int x1, int y1, int x2, int y2) {

  // XftDrawRect(xw.draw,
  // 		&dc.col[IS_SET(MODE_REVERSE)? defaultfg : defaultbg],
  // 		x1, y1, x2-x1, y2-y1);
  glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);//TODO get colors
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void xresize(int col, int row) {
  terminal_window.tty_width = col * terminal_window.character_width;
  terminal_window.tty_height = row * terminal_window.character_height;

  xclear(0, 0, terminal_window.width, terminal_window.height);
}

void cresize(int width, int height) {
  int col, row;

  if (width != 0)
    terminal_window.width = width;
  if (height != 0)
    terminal_window.height = height;

  col = terminal_window.width / terminal_window.character_width;
  row = terminal_window.height / terminal_window.character_height;

  col = MAX(1, col);
  row = MAX(1, row);

  tresize(col, row);
  xresize(col, row);
  ttyresize(terminal_window.tty_width, terminal_window.tty_height);

  set_ortho_projection(terminal_window.width, terminal_window.height);
  glViewport(0, 0, terminal_window.width, terminal_window.height);

}

void resize(XEvent *e) {
  if (e->xconfigure.width == terminal_window.width &&
      e->xconfigure.height == terminal_window.height)
    return;

  cresize(e->xconfigure.width, e->xconfigure.height);
  glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);//TODO get colors
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}

void redraw(void) {
  tfulldirt();
  draw();
}

void zoom(const Arg *arg) {
  Arg larg;

  //  larg.f = usedfontsize + arg->f;
  zoomabs(&larg);
}

void zoomabs(const Arg *arg) {
  // xunloadfonts();
  // xloadfonts(usedfont, arg->f);
  cresize(0, 0);
  redraw();
  xhints();
}

void zoomreset(const Arg *arg) {
  Arg larg;

  // if (defaultfontsize > 0) {
  //   larg.f = defaultfontsize;
  //   zoomabs(&larg);
  // }
}
