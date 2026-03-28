#include "pterminal.h"

#include <pway/pway.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/poll.h>
#include <unistd.h>
#include "window.h"

#include "tty.h"

#include "draw.h"

bool can_draw;

static char *shell = "/bin/sh";

int set_terminal_cursor(int cursor) {
  if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
    return 1;
  terminal_window.cursor = cursor;
  return 0;
}

void *run_pterminal(void *none) {

  fd_set read_file_descriptor;

  int tty_fd;


  tty_fd = ttynew(NULL, shell, NULL, NULL);

  pway_set_app_fd(tty_fd);

  printf("running pterminal\n");

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

    clean_mouse_buttons();

  }
}
