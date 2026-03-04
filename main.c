/* See LICENSE for license details. */
#include "wayland/wayland.h"
#include <X11/X.h>
#include <bits/pthreadtypes.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <pthread.h>

#include <wayland-client-core.h>

char *argv0;

#include "draw.h"
#include "terminal.h"
#include "window.h"

// OpenGL
#include "opengl.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdbool.h>

#include "selection.h"
#include <pthread.h>

#include "tty.h"


#include "pterminal.h"

#include "config.h"

static inline ushort sixd_to_16bit(int);


void ttysend(const Arg *arg) { 
  write_to_tty(arg->s, strlen(arg->s), 1); 
}



void xsetmode(int set, unsigned int flags) {
  int mode = terminal_window.mode;
  MODBIT(terminal_window.mode, set, flags);
  if ((terminal_window.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
    redraw();
}

int xsetcursor(int cursor) {
  if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
    return 1;
  terminal_window.cursor = cursor;
  return 0;
}


void handle_interrupt(int signal_number){

  exit_pterminal();

}

void exit_pterminal(){
  ttyhangup();

  wl_display_disconnect(wayland_terminal.display);

  printf("Exit pterminal\n");

 
  exit(EXIT_SUCCESS);

}


int main(int argc, char *argv[]) {

  signal(SIGINT, handle_interrupt);

  is_opengl = true;

  xw.left_offset = xw.top_offset = 0;
  xw.isfixed = False;
  xsetcursor(cursorshape);


  setlocale(LC_CTYPE, "");

  //if you provide arguments for size this will change
  cols = MAX(cols, 1);
  rows = MAX(rows, 1);

  new_terminal(cols, rows);

  printf("Terminal size cols: %i rows: %i \n", term.col, term.row);

  create_window(cols, rows);

  init_draw_method();

  selinit();


  //while(!is_window_configured){}
  MODBIT(terminal_window.mode, 1 , MODE_VISIBLE);


  resize_pterminal(terminal_window.width, terminal_window.height);

  printf("pterminal init..\n");

  run_pterminal(NULL);

  exit_pterminal();

  return 0;
}
