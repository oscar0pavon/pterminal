#include "draw.h"
#include "terminal.h"
#include "window.h"
#include "opengl.h"
#include <X11/extensions/Xrender.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "utf8.h"
#include "selection.h"
#include "egl.h"

DC drawing_context;

int borderpx = 2;
unsigned int defaultrcs = 257;
unsigned int cursorthickness = 2;


bool can_update_size = false;


// if we don't use EGL
void create_xgl_context() {

  xw.gl_context = glXCreateContext(xw.display, xw.visual_info, None, GL_TRUE);
  if (!xw.gl_context) {
    die("Can't create GLX context");
  }

  glXMakeCurrent(xw.display, xw.win, xw.gl_context);
}

void swap_draw_buffers(){

  eglSwapBuffers(egl_display, egl_surface);
}

void update_size() {
  if (can_update_size) {

    glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);//TODO get colors
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    set_ortho_projection(terminal_window.width, terminal_window.height);
    glViewport(0, 0, terminal_window.width, terminal_window.height);

    can_update_size = false;
  }
}

void init_draw_method(){

  init_egl();


  set_ortho_projection(terminal_window.width, terminal_window.height);
  glViewport(0, 0, terminal_window.width, terminal_window.height);
  load_texture(&font_texture_id, "/root/pterminal/font1.png");

}

void xdrawline(Line line, int position_x, int position_y, int column) {

  for (int i = 0; i < column; i++) {

    xdrawglyph(line[i],i,position_y);
  }
}

void drawregion(int position_x, int position_y, int column, int row) {
  int i, line_number;

  line_number = TLINEOFFSET(position_y);

  for (i = position_y; i < row; i++) {

    xdrawline(TSCREEN.buffer[line_number], position_x, i, column);

    line_number = (line_number + 1) % TSCREEN.size;

  }
}

void draw(void) {
  update_size();
  glViewport(0, 0, terminal_window.width, terminal_window.height);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  int cursor_x = term.cursor.x;
  int old_cursor_x = term.old_cursor_x;
  int old_cursor_y = term.old_cursor_y;

  if(!IS_WINDOSET(MODE_VISIBLE))
    return;


  /* adjust cursor position */
  LIMIT(term.old_cursor_x, 0, term.col - 1);
  LIMIT(term.old_cursor_y, 0, term.row - 1);

  if (TLINE(term.old_cursor_y)[term.old_cursor_x].mode & ATTR_WDUMMY)
    term.old_cursor_x--;

  if (TLINE(term.cursor.y)[cursor_x].mode & ATTR_WDUMMY)
    cursor_x--;

  drawregion(0, 0, term.col, term.row);

  xdrawcursor(cursor_x, term.cursor.y, TLINE(term.cursor.y)[cursor_x],
               term.old_cursor_x, term.old_cursor_y,
               TLINE(term.old_cursor_y)[term.old_cursor_x]);

  term.old_cursor_x = cursor_x;
  term.old_cursor_y = term.cursor.y;

  swap_draw_buffers();
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
  int winx, winy;
  PColor cursor_color = {.r = 1, .g = 1, .b = 1};
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

      winx = cursor_x * terminal_window.character_width;
      winy = (cursor_y + 1) * terminal_window.character_height;
      gl_draw_rect(cursor_color, winx,
                   winy, terminal_window.character_width, cursorthickness);

      break;
    case 5: /* Blinking bar */
    case 6: /* Steady bar */
      winx = cursor_x * terminal_window.character_width;
      winy = (cursor_y) * terminal_window.character_height;
      gl_draw_rect(cursor_color, winx, winy, cursorthickness,
                   terminal_window.character_height);
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

void xdrawglyph(PGlyph glyph, int x, int y) {

  if (selected(x, y))
    glyph.mode ^= ATTR_REVERSE;
  else if (glyph.mode & ATTR_REVERSE) {
    glyph.mode ^= ATTR_REVERSE;
  }

  RenderColor color;
  get_color_from_glyph(&glyph, &color);

  int winy = y * terminal_window.character_height;
  int background_x = x * terminal_window.character_width;


  gl_draw_rect(color.gl_background_color, background_x, winy,
               terminal_window.character_width,
               terminal_window.character_height);

  float offset_x = (terminal_window.character_gl_width / 2) -
                 terminal_window.character_width;

  float char_center_x = terminal_window.character_width / 2;
  
  float offset_y = (terminal_window.character_gl_height / 2) -
                 terminal_window.character_height;

  float char_center_y = terminal_window.character_height / 2;

  float draw_x = ((x * terminal_window.character_width) - char_center_x) - offset_x;

  float draw_y = ((y * terminal_window.character_height) - char_center_y) - offset_y;
  draw_y = draw_y + 1;//move one pixel down for botton line

  uint8_t ascii_value;
  if (glyph.u > 127) {
    ascii_value = get_texture_atlas_index(glyph.u);
  } else {
    ascii_value = glyph.u;
  }

  gl_draw_char(ascii_value, color.gl_foreground_color, draw_x, draw_y,
               terminal_window.character_gl_width,
               terminal_window.character_gl_height);
}


