#include "window.h"
#include "opengl.h"
#include "types.h"
#include "terminal.h"

/* Globals */
XWindow xw;
XSelection xsel;
TermWindow win;

void expose(XEvent *ev) { redraw(); }

/*
 * Absolute coordinates.
 */
void xclear(int x1, int y1, int x2, int y2) {

	// XftDrawRect(xw.draw,
	// 		&dc.col[IS_SET(MODE_REVERSE)? defaultfg : defaultbg],
	// 		x1, y1, x2-x1, y2-y1);
}

void xresize(int col, int row)
{
	win.tw = col * win.character_width;
	win.th = row * win.character_height;

	xclear(0, 0, win.width, win.hight);

}

void cresize(int width, int height)
{
	int col, row;

	if (width != 0)
		win.width = width;
	if (height != 0)
		win.hight = height;

	col = (win.width - 2 * borderpx) / win.character_width;
	row = (win.hight - 2 * borderpx) / win.character_height;
	col = MAX(1, col);
	row = MAX(1, row);

	tresize(col, row);
	xresize(col, row);
	ttyresize(win.tw, win.th);
  
  set_ortho_projection(win.width, win.hight);
  glViewport(0,0,win.width,win.hight);
}

void resize(XEvent *e) {
  if (e->xconfigure.width == win.width && e->xconfigure.height == win.hight)
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
  //xunloadfonts();
  //xloadfonts(usedfont, arg->f);
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
