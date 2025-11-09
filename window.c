#include "window.h"
#include "opengl.h"
#include "terminal.h"
#include "types.h"
#include "draw.h"

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
