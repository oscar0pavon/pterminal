/* See LICENSE for license details. */
#include "wayland/wayland.h"
#include <X11/X.h>
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
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
#include "events.h"
#include "opengl.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdbool.h>

#include "color.h"
#include "input.h"
#include "mouse.h"
#include "selection.h"
#include <pthread.h>

#include "tty.h"

#include "xorg.h"

#include "pterminal.h"

/* config.h for applying patches and the configuration. */
#include "config.h"

int window_manager_file_descriptor;

static inline ushort sixd_to_16bit(int);

static int xgeommasktogravity(int);
static void xsetenv(void);

static void usage(void);

/* Font Ring Cache */
enum { FRC_NORMAL, FRC_ITALIC, FRC_BOLD, FRC_ITALICBOLD };

void ttysend(const Arg *arg) { ttywrite(arg->s, strlen(arg->s), 1); }

void xhints(void) {
  XClassHint class = {opt_name ? opt_name : termname,
                      opt_class ? opt_class : termname};
  XWMHints wm = {.flags = InputHint, .input = 1};
  XSizeHints *sizeh;

  sizeh = XAllocSizeHints();

  sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
  sizeh->height = terminal_window.height;
  sizeh->width = terminal_window.width;
  sizeh->height_inc = terminal_window.character_height;
  sizeh->width_inc = terminal_window.character_width;
  sizeh->base_height = 2;
  sizeh->base_width = 2;
  sizeh->min_height = terminal_window.character_height;
  sizeh->min_width = terminal_window.character_width;
  if (xw.isfixed) {
    sizeh->flags |= PMaxSize;
    sizeh->min_width = sizeh->max_width = terminal_window.width;
    sizeh->min_height = sizeh->max_height = terminal_window.height;
  }


  XSetWMProperties(xw.display, xw.win, NULL, NULL, NULL, 0, sizeh, &wm, &class);
  XFree(sizeh);
}


void xsetpointermotion(int set) {
  MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
  if(terminal_window.type == XORG)
    XChangeWindowAttributes(xw.display, xw.win, CWEventMask, &xw.attrs);
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

  if(terminal_window.type == WAYLAND){
    wl_display_disconnect(wayland_terminal.display);
  }
  printf("Exit pterminal\n");
  exit(0);
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

  if (terminal_window.type == XORG) {
    window_manager_file_descriptor = XConnectionNumber(xw.display);
    wait_for_mapping();
  }else{

    //while(!is_window_configured){}
    MODBIT(terminal_window.mode, 1 , MODE_VISIBLE);
  }

  cresize(terminal_window.width, terminal_window.height);

  printf("pterminal init..\n");

  pthread_t tty_id;
  pthread_create(&tty_id, NULL, run_pterminal, NULL);

  while (terminal_window.is_running) {
    
    pthread_mutex_lock(&draw_mutex);
    if (can_draw) {
      draw();
      can_draw = false;
    }
    pthread_mutex_unlock(&draw_mutex);
    //sleep(1);
  }

  printf("pterminal closed\n");

  return 0;
}
