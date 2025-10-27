#include "draw.h"
#include "terminal.h"
#include "win.h"
#include "opengl.h"

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
  int cx = term.cursor.x, ocx = term.old_cursor_x, ocy = term.old_cursor_y;

  if (!xstartdraw())
    return;

  /* adjust cursor position */
  LIMIT(term.old_cursor_x, 0, term.col - 1);
  LIMIT(term.old_cursor_y, 0, term.row - 1);
  if (TLINE(term.old_cursor_y)[term.old_cursor_x].mode & ATTR_WDUMMY)
    term.old_cursor_x--;
  if (TLINE(term.cursor.y)[cx].mode & ATTR_WDUMMY)
    cx--;

  drawregion(0, 0, term.col, term.row);
  if (TSCREEN.off == 0)
    xdrawcursor(cx, term.cursor.y, TLINE(term.cursor.y)[cx], term.old_cursor_x, term.old_cursor_y,
                TLINE(term.old_cursor_y)[term.old_cursor_x]);
  term.old_cursor_x = cx;
  term.old_cursor_y = term.cursor.y;
  xfinishdraw();
  if (ocx != term.old_cursor_x || ocy != term.old_cursor_y)
    xximspot(term.old_cursor_x, term.old_cursor_y);



}


