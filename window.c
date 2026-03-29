#include "window.h"
#include "mouse.h"
#include "pterminal.h"
#include "selection.h"
#include "terminal.h"
#include "types.h"
#include "draw.h"
#include <stdio.h>

#include <stdbool.h>
#include <unistd.h>

#include <pway/pway.h>

XSelection xsel;
TerminalWindow terminal_window;

void input_keys(const char* text, int len){
  write_to_tty(text, len, 1);
}


void focus_window(bool is_focuses){
  if(!terminal_window.is_ready)
    return;
  
  if(is_focuses){

    terminal_window.mode |= MODE_FOCUSED;

    if (IS_WINDOSET(MODE_FOCUS))
      write_to_tty("\033[I", 3, 0);

  }else{

    terminal_window.mode &= ~MODE_FOCUSED;

    if (IS_WINDOSET(MODE_FOCUS))
      write_to_tty("\033[O", 3, 0);
  }
  can_draw = true;
}

void end_window(){
  terminal_window.is_running = false;  
}

void create_window(int cols, int rows){
  terminal_window.character_height = 24;
  terminal_window.character_gl_width = 32;
  terminal_window.character_gl_height = 32;
  terminal_window.character_width = 9;

  terminal_window.width = cols * terminal_window.character_width;
  terminal_window.height = rows * terminal_window.character_height;

  xloadcols();

  pway = pway_init();
  pway->resize = resize_pterminal;
  pway->exit = end_window;
  pway->focus = focus_window;
  pway->input = input_keys;
  pway->click = mouse_click;
  pway->click_release = release_button;
  pway->update_mouse = update_mouse;
  pway->get_text = get_selection;


  if(pway_create_window("pterminal0.2",terminal_window.width, terminal_window.height) == false){
    die("Can't create Wayland window");
  }


  terminal_window.is_ready = true;
  terminal_window.is_running = true;

}


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

  pway_egl_resize(width, height);

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
  can_draw = true;

  printf("resized\n");
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
