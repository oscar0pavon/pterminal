#include "draw.h"
#include "terminal.h"
#include "window.h"
#include "opengl.h"

DC drawing_context;

int borderpx = 2;
unsigned int defaultrcs = 257;

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

  if(!is_opengl)
    xfinishdraw(); //only call when with draw with Xlib

  if (old_cursor_x != term.old_cursor_x || old_cursor_y != term.old_cursor_y)
    xximspot(term.old_cursor_x, term.old_cursor_y);
}

void get_color_from_glyph(Glyph* base, RenderColor* out){

  Color *fg, *bg, *temp;
  XRenderColor colfg, colbg;
  bool is_foreground_true_color = false;
  bool is_background_true_color = false;


  if (IS_TRUECOL(base->fg)) {
    colfg.alpha = 0xffff;
    colfg.red = TRUERED(base->fg);
    colfg.green = TRUEGREEN(base->fg);
    colfg.blue = TRUEBLUE(base->fg);
    XftColorAllocValue(xw.display, xw.vis, xw.cmap, &colfg, &out->truefg);
    fg = &out->truefg;
    is_foreground_true_color = true;
  } else {
    fg = &drawing_context.colors[base->fg];
  }

  if (IS_TRUECOL(base->bg)) {
    colbg.alpha = 0xffff;
    colbg.green = TRUEGREEN(base->bg);
    colbg.red = TRUERED(base->bg);
    colbg.blue = TRUEBLUE(base->bg);
    XftColorAllocValue(xw.display, xw.vis, xw.cmap, &colbg, &out->truebg);
    bg = &out->truebg;
    is_background_true_color = true;
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
      XftColorAllocValue(xw.display, xw.vis, xw.cmap, &colfg, &out->revfg);
      fg = &out->revfg;
    }

    if (bg == &drawing_context.colors[defaultbg]) {
      bg = &drawing_context.colors[defaultfg];
    } else {
      colbg.red = ~bg->color.red;
      colbg.green = ~bg->color.green;
      colbg.blue = ~bg->color.blue;
      colbg.alpha = bg->color.alpha;
      XftColorAllocValue(xw.display, xw.vis, xw.cmap, &colbg, &out->revbg);
      bg = &out->revbg;
    }
  }

  if ((base->mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
    colfg.red = fg->color.red / 2;
    colfg.green = fg->color.green / 2;
    colfg.blue = fg->color.blue / 2;
    colfg.alpha = fg->color.alpha;
    XftColorAllocValue(xw.display, xw.vis, xw.cmap, &colfg, &out->revfg);
    fg = &out->revfg;
  }

  if (base->mode & ATTR_REVERSE) {
    temp = fg;
    fg = bg;
    bg = temp;
  }

  if (base->mode & ATTR_BLINK && win.mode & MODE_BLINK)
    fg = bg;

  if (base->mode & ATTR_INVISIBLE)
    fg = bg;


  //opengl convertion
  float div;
  if(is_background_true_color){

    div = 65535.f;
    PColor mycolor = {.r = bg->color.red/div,
                      .g = bg->color.green/div,
                      .b = bg->color.blue/div};
    out->gl_background_color = mycolor;
  }else{

    if(bg->color.red > 255 || bg->color.blue > 255 || bg->color.green > 255){
      div = 65525.f;
    }else{
      div = 255.f;
    }

    PColor mycolor = {.r = bg->color.red/div,
                      .g = bg->color.green/div,
                      .b = bg->color.blue/div};
    out->gl_background_color = mycolor;
  }
  if(is_foreground_true_color){

    div = 65535.f;
    PColor mycolor = {.r = fg->color.red/div,
                      .g = fg->color.green/div,
                      .b = fg->color.blue/div};
    out->gl_foreground_color = mycolor;
  }else{
    div = 255.f;

    PColor mycolor = {.r = fg->color.red/div,
                      .g = fg->color.green/div,
                      .b = fg->color.blue/div};
    out->gl_foreground_color = mycolor;
  }

  out->foreground = fg;
  out->background = bg;

}


void xdrawglyph(Glyph g, int x, int y) {
  //Background
  RenderColor color;
  get_color_from_glyph(&g, &color);

  int winx_char = ((x * 9.1))-6;
  int winx = ((x * 9.1)+4);
  int winy = borderpx + y * win.character_height;

  gl_draw_rect(color.gl_background_color, winx, winy, 7, 26);


  gl_draw_char(g.u, color.gl_foreground_color, winx_char, winy, 24, 26);
}

void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og) {
  Color drawcol;

  /* remove the old cursor */
  if (selected(ox, oy))
    og.mode ^= ATTR_REVERSE;
  xdrawglyph(og, ox, oy);

  if (IS_WINDOSET(MODE_HIDE))
    return;

  /*
   * Select the right color for the right mode.
   */
  g.mode &= ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_STRUCK | ATTR_WIDE;

  if (IS_WINDOSET(MODE_REVERSE)) {
    g.mode |= ATTR_REVERSE;
    g.bg = defaultfg;
    if (selected(cx, cy)) {
      drawcol = drawing_context.colors[defaultcs];
      g.fg = defaultrcs;
    } else {
      drawcol = drawing_context.colors[defaultrcs];
      g.fg = defaultcs;
    }
  } else {
    if (selected(cx, cy)) {
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
    switch (win.cursor) {
    case 7:         /* st extension */
      g.u = 0x2603; /* snowman (U+2603) */
                    /* FALLTHROUGH */
    case 0:         /* Blinking Block */
    case 1:         /* Blinking Block (Default) */
    case 2:         /* Steady Block */
      xdrawglyph(g, cx, cy);
      break;
    case 3: /* Blinking Underline */
    case 4: /* Steady Underline */
      // XftDrawRect(xw.draw, &drawcol, borderpx + cx * win.cw,
      //             borderpx + (cy + 1) * win.ch - cursorthickness, win.cw,
      //             cursorthickness);
      int winx = ((cx * 9.1));
      int winy = borderpx + cy * win.character_height;
      PColor new_color = {.r = 1, .g = 1, .b =1};
      gl_draw_rect(new_color, winx, winy-(win.character_height-43), 10, 4);
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

void xdrawline(Line line, int position_x, int position_y, int column) {
  int i, current_x_position, old_x, numspecs;
  Glyph base, new;

  for(int i = 0; i < column - position_x; i++){

    RenderColor color;
    get_color_from_glyph(&line[i],&color);

    int winx = ((i * 9.1));
    int winy = borderpx + position_y * win.character_height;
  

    gl_draw_rect(color.gl_background_color, winx, winy, 24,26);
  }
  for(int i = 0; i < column - position_x; i++){

    RenderColor color;
    get_color_from_glyph(&line[i],&color);

    int winx = ((i * 9.1)-position_x)-6;
    int winy = borderpx + position_y * win.character_height;
  
    gl_draw_char(line[i].u, color.gl_foreground_color,  winx, winy, 24,26);
  }

}

void xfinishdraw(void) {

  // XSetForeground(xw.display, drawing_context.gc,
  //                drawing_context.colors[IS_WINDOSET(MODE_REVERSE) ? defaultfg : defaultbg].pixel);
}
