/* See LICENSE for license details. */

#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <pway/pway.h>

#include "pterminal.h"

#include "draw.h"
#include "terminal.h"
#include "window.h"
#include "selection.h"
#include "tty.h"
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

void handle_interrupt(int signal_number){

  exit_pterminal();

}

void exit_pterminal(){
  ttyhangup();

  pway_finish();

  printf("Exit pterminal\n");

 
  exit(EXIT_SUCCESS);

}


int main(int argc, char *argv[]) {

  signal(SIGINT, handle_interrupt);

  set_terminal_cursor(cursorshape);

  setlocale(LC_CTYPE, "");

  //if you provide arguments for size this will change
  cols = MAX(cols, 1);
  rows = MAX(rows, 1);

  new_terminal(cols, rows);

  printf("Terminal size cols: %i rows: %i \n", term.col, term.row);

  create_window(cols, rows);

  init_draw_method();

  selinit();

  terminal_window.mode = MODE_NUMLOCK;

  MODBIT(terminal_window.mode, 1 , MODE_VISIBLE);

  resize_pterminal(terminal_window.width, terminal_window.height);


  int tty_fd = new_tty("/bin/sh", NULL, NULL);

  pway_set_app_fd(tty_fd);

  //Main loop
  while (terminal_window.is_running) {
    
    pway_handle_events();

    if( pway_app_has_event() ){
      read_tty();
      can_draw = true;
    }

    if(can_draw){
      draw();
      can_draw = false;
    }

  }

  exit_pterminal();

  return 0;
}
