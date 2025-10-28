#include "draw.h"
#include "terminal.h"
#include "win.h"
#include "opengl.h"

void drawregion(int position_x, int position_y, int column, int row) {
  int i, line_number;

  line_number = TLINEOFFSET(position_y);
  for (i = position_y; i < row; i++) {
    if (!is_opengl) {
      if (term.dirty[i]) {
        term.dirty[i] = 0;
        xdrawline(TSCREEN.buffer[line_number], position_x, i, column);
      }
    }else{//always draw the buffer
      xdrawline(TSCREEN.buffer[line_number], position_x, i, column);
    }
    line_number = (line_number + 1) % TSCREEN.size;
  }
}

void draw(void) {
  int cursor_x = term.cursor.x;
  int old_cursor_x = term.old_cursor_x;
  int old_cursor_y = term.old_cursor_y;

  if (!xstartdraw())
    return;

  /* adjust cursor position */
  LIMIT(term.old_cursor_x, 0, term.col - 1);
  LIMIT(term.old_cursor_y, 0, term.row - 1);

  if (TLINE(term.old_cursor_y)[term.old_cursor_x].mode & ATTR_WDUMMY)
    term.old_cursor_x--;

  if (TLINE(term.cursor.y)[cursor_x].mode & ATTR_WDUMMY)
    cursor_x--;

  drawregion(0, 0, term.col, term.row);

  if (TSCREEN.off == 0)
    xdrawcursor(cursor_x, term.cursor.y, TLINE(term.cursor.y)[cursor_x],
                term.old_cursor_x, term.old_cursor_y,
                TLINE(term.old_cursor_y)[term.old_cursor_x]);

  term.old_cursor_x = cursor_x;
  term.old_cursor_y = term.cursor.y;

  //xfinishdraw(); //only call when with draw with Xlib

  if (old_cursor_x != term.old_cursor_x || old_cursor_y != term.old_cursor_y)
    xximspot(term.old_cursor_x, term.old_cursor_y);
}
