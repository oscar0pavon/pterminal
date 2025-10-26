#include "draw.h"
#include "terminal.h"
#include "win.h"

void drawregion(int x1, int y1, int x2, int y2) {
  int y, L;

  L = TLINEOFFSET(y1);
  for (y = y1; y < y2; y++) {
    if (term.dirty[y]) {
      term.dirty[y] = 0;
      xdrawline(TSCREEN.buffer[L], x1, y, x2);
    }
    L = (L + 1) % TSCREEN.size;
  }
}

void draw(void) {
  int cx = term.c.x, ocx = term.ocx, ocy = term.ocy;

  if (!xstartdraw())
    return;

  /* adjust cursor position */
  LIMIT(term.ocx, 0, term.col - 1);
  LIMIT(term.ocy, 0, term.row - 1);
  if (TLINE(term.ocy)[term.ocx].mode & ATTR_WDUMMY)
    term.ocx--;
  if (TLINE(term.c.y)[cx].mode & ATTR_WDUMMY)
    cx--;

  drawregion(0, 0, term.col, term.row);
  if (TSCREEN.off == 0)
    xdrawcursor(cx, term.c.y, TLINE(term.c.y)[cx], term.ocx, term.ocy,
                TLINE(term.ocy)[term.ocx]);
  term.ocx = cx;
  term.ocy = term.c.y;
  xfinishdraw();
  if (ocx != term.ocx || ocy != term.ocy)
    xximspot(term.ocx, term.ocy);
}


