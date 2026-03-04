#include "window.h"
#include "opengl.h"
#include "terminal.h"
#include "types.h"
#include "draw.h"
#include "wayland/wayland.h"

#include "egl.h"

#include <stdbool.h>
#include <unistd.h>

/* Globals */
XWindow xw;
XSelection xsel;
TerminalWindow terminal_window;

bool is_window_configured = false;
bool can_draw = false;

char *opt_class = NULL;
char **opt_cmd = NULL;
char *opt_embed = NULL;
char *opt_font = NULL;
char *opt_io = NULL;
char *opt_line = NULL;
char *opt_name = NULL;
char *opt_title = NULL;

void create_window(int cols, int rows){
  terminal_window.character_height = 24;
  terminal_window.character_gl_width = 32;
  terminal_window.character_gl_height = 32;
  terminal_window.character_width = 9;

  terminal_window.width = cols * terminal_window.character_width;
  terminal_window.height = rows * terminal_window.character_height;

  xloadcols();

  if(init_wayland() == false){

  }else{
    terminal_window.type = WAYLAND;
    printf("Running Wayland window\n");
  }

  terminal_window.is_ready = true;
  terminal_window.is_running = true;

}

void expose(XEvent *ev) { redraw(); }


void clear_window() {

  glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);//TODO get colors
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}


void resize_pterminal(int width, int height) {

  if(terminal_window.width == width && terminal_window.height == height)
    return;

  int col, row;

  if (width != 0)
    terminal_window.width = width;
  if (height != 0)
    terminal_window.height = height;

  wl_egl_window_resize(egl_window, width, height, 0 , 0);

  col = terminal_window.width / terminal_window.character_width;

  row = terminal_window.height / terminal_window.character_height;

  col = MAX(1, col);
  row = MAX(1, row);

  resize_terminal(col, row);

  terminal_window.tty_width = col * terminal_window.character_width;
  terminal_window.tty_height = row * terminal_window.character_height;

  clear_window();

  resize_tty(terminal_window.tty_width, terminal_window.tty_height);


  can_update_size = true;

  printf("resized\n");
}

void resize(XEvent *e) {
  if (e->xconfigure.width == terminal_window.width &&
      e->xconfigure.height == terminal_window.height)
    return;

  resize_pterminal(e->xconfigure.width, e->xconfigure.height);

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
  resize_pterminal(0, 0);
  redraw();
  //xhints();
}

void zoomreset(const Arg *arg) {
  Arg larg;

  // if (defaultfontsize > 0) {
  //   larg.f = defaultfontsize;
  //   zoomabs(&larg);
  // }
}
