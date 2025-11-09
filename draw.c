#include "draw.h"
#include "terminal.h"
#include "window.h"
#include "opengl.h"
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include "utf8.h"

DC drawing_context;

int borderpx = 2;
unsigned int defaultrcs = 257;
unsigned int cursorthickness = 2;

void drawregion(int position_x, int position_y, int column, int row) {
  int i, line_number;

  line_number = TLINEOFFSET(position_y);

  for (i = position_y; i < row; i++) {

    xdrawline(TSCREEN.buffer[line_number], position_x, i, column);

    line_number = (line_number + 1) % TSCREEN.size;

  }
}

void draw(void) {
  // glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

  if (!is_opengl)
    xfinishdraw(); // only call when with draw with Xlib

  if (old_cursor_x != term.old_cursor_x || old_cursor_y != term.old_cursor_y)
    xximspot(term.old_cursor_x, term.old_cursor_y);

  glXSwapBuffers(xw.display, xw.win);
}



void xdrawcursor(int cursor_x, int cursor_y, PGlyph g, int old_x, int old_y,
                 PGlyph og) {
  Color drawcol;

  /* remove the old cursor */
  if (selected(old_x, old_y))
    og.mode ^= ATTR_REVERSE;
  xdrawglyph(og, old_x, old_y);

  if (IS_WINDOSET(MODE_HIDE))
    return;

  /*
   * Select the right color for the right mode.
   */
  g.mode &= ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_STRUCK | ATTR_WIDE;

  if (IS_WINDOSET(MODE_REVERSE)) {
    g.mode |= ATTR_REVERSE;
    g.bg = defaultfg;
    if (selected(cursor_x, cursor_y)) {
      drawcol = drawing_context.colors[defaultcs];
      g.fg = defaultrcs;
    } else {
      drawcol = drawing_context.colors[defaultrcs];
      g.fg = defaultcs;
    }
  } else {
    if (selected(cursor_x, cursor_y)) {
      g.fg = defaultfg;
      g.bg = defaultrcs;
    } else {
      g.fg = defaultbg;
      g.bg = defaultcs;
    }
    drawcol = drawing_context.colors[g.bg];
  }

  /* draw the new one */
  if (IS_WINDOSET(MODE_FOCUSED)) {
    switch (terminal_window.cursor) {
    case 0:         /* Blinking Block */
    case 1:         /* Blinking Block (Default) */
    case 2:         /* Steady Block */
      xdrawglyph(g, cursor_x, cursor_y);
      break;
    case 3: /* Blinking Underline */
    case 4: /* Steady Underline */

      int winx = cursor_x * terminal_window.character_width;
      int winy = (cursor_y + 1) * terminal_window.character_height;
      PColor new_color = {.r = 1, .g = 1, .b = 1};
      gl_draw_rect(new_color, winx,
                   winy, terminal_window.character_width, cursorthickness);

      break;
    case 5: /* Blinking bar */
    case 6: /* Steady bar */
      // XftDrawRect(xw.draw, &drawcol, borderpx + cx * win.cw,
      //             borderpx + cy * win.ch, cursorthickness, win.ch);
      break;
    }
  } else {
    // XftDrawRect(xw.draw, &drawcol, borderpx + cx * win.cw,
    //             borderpx + cy * win.ch, win.cw - 1, 1);
    // XftDrawRect(xw.draw, &drawcol, borderpx + cx * win.cw,
    //             borderpx + cy * win.ch, 1, win.ch - 1);
    // XftDrawRect(xw.draw, &drawcol, borderpx + (cx + 1) * win.cw - 1,
    //             borderpx + cy * win.ch, 1, win.ch - 1);
    // XftDrawRect(xw.draw, &drawcol, borderpx + cx * win.cw,
    //             borderpx + (cy + 1) * win.ch - 1, win.cw, 1);
  }
}

int xstartdraw(void) { return IS_WINDOSET(MODE_VISIBLE); }

void xdrawglyph(PGlyph glyph, int x, int y) {
  
  RenderColor color;
  get_color_from_glyph(&glyph, &color);

  int winy = y * terminal_window.character_height;
  int background_x = x * terminal_window.character_width;

  gl_draw_rect(color.gl_background_color, background_x, winy,
               terminal_window.character_width,
               terminal_window.character_height);

  float offset = (terminal_window.character_gl_width / 2) -
                 terminal_window.character_width;

  float char_center = terminal_window.character_width / 2;

  float winx = ((x * terminal_window.character_width) - char_center) - offset;

  uint8_t ascii_value;
  if (glyph.u > 127) {
    ascii_value = get_texture_atlas_index(glyph.u);
  } else {
    ascii_value = glyph.u;
  }

  gl_draw_char(ascii_value, color.gl_foreground_color, winx, winy,
               terminal_window.character_gl_width,
               terminal_window.character_height);
}

void xdrawline(Line line, int position_x, int position_y, int column) {

  for (int i = 0; i < column; i++) {
    xdrawglyph(line[i],i,position_y);
  }
}

void xfinishdraw(void) {

  // XSetForeground(xw.display, drawing_context.gc,
  //                drawing_context.colors[IS_WINDOSET(MODE_REVERSE) ? defaultfg : defaultbg].pixel);
}
