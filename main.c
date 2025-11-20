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
#include "arg.h"
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
  if (xw.gm & (XValue | YValue)) {
    sizeh->flags |= USPosition | PWinGravity;
    sizeh->x = xw.left_offset;
    sizeh->y = xw.top_offset;
    sizeh->win_gravity = xgeommasktogravity(xw.gm);
  }

  XSetWMProperties(xw.display, xw.win, NULL, NULL, NULL, 0, sizeh, &wm, &class);
  XFree(sizeh);
}

int xgeommasktogravity(int mask) {
  switch (mask & (XNegative | YNegative)) {
  case 0:
    return NorthWestGravity;
  case XNegative:
    return NorthEastGravity;
  case YNegative:
    return SouthWestGravity;
  }

  return SouthEastGravity;
}

void xsetenv(void) {
  char buf[sizeof(long) * 8 + 1];

  snprintf(buf, sizeof(buf), "%lu", xw.win);
  setenv("WINDOWID", buf, 1);
}

void xseticontitle(char *p) {
  XTextProperty prop;
  DEFAULT(p, opt_title);

  if (p[0] == '\0')
    p = opt_title;

  if (Xutf8TextListToTextProperty(xw.display, &p, 1, XUTF8StringStyle, &prop) !=
      Success)
    return;
  XSetWMIconName(xw.display, xw.win, &prop);
  XSetTextProperty(xw.display, xw.win, &prop, xw.netwmiconname);
  XFree(prop.value);
}

void xsettitle(char *p) {
  XTextProperty prop;
  DEFAULT(p, opt_title);

  if (p[0] == '\0')
    p = opt_title;

  if (Xutf8TextListToTextProperty(xw.display, &p, 1, XUTF8StringStyle, &prop) !=
      Success)
    return;
  XSetWMName(xw.display, xw.win, &prop);
  XSetTextProperty(xw.display, xw.win, &prop, xw.netwmname);
  XFree(prop.value);
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

void xbell(void) {
  if (!(IS_WINDOSET(MODE_FOCUSED)))
    xseturgency(1);
  if (bellvolume)
    XkbBell(xw.display, xw.win, bellvolume, (Atom)NULL);
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


void usage(void) {
  die("usage: %s [-aiv] [-c class] [-f font] [-g geometry]"
      " [-n name] [-o file]\n"
      "          [-T title] [-t title] [-w windowid]"
      " [[-e] command [args ...]]\n"
      "       %s [-aiv] [-c class] [-f font] [-g geometry]"
      " [-n name] [-o file]\n"
      "          [-T title] [-t title] [-w windowid] -l line"
      " [stty_args ...]\n",
      argv0, argv0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, handle_interrupt);

  is_opengl = true;

  xw.left_offset = xw.top_offset = 0;
  xw.isfixed = False;
  xsetcursor(cursorshape);

  ARGBEGIN {
  case 'a':
    allowaltscreen = 0;
    break;
  case 'c':
    opt_class = EARGF(usage());
    break;
  case 'e':
    if (argc > 0)
      --argc, ++argv;
    goto run;
  case 'f':
    opt_font = EARGF(usage());
    break;
  case 'g':
    xw.gm = XParseGeometry(EARGF(usage()), &xw.left_offset, &xw.top_offset,
                           &cols, &rows);
    break;
  case 'i':
    xw.isfixed = 1;
    break;
  case 'o':
    opt_io = EARGF(usage());
    break;
  case 'l':
    opt_line = EARGF(usage());
    break;
  case 'n':
    opt_name = EARGF(usage());
    break;
  case 't':
  case 'T':
    opt_title = EARGF(usage());
    break;
  case 'w':
    opt_embed = EARGF(usage());
    break;
  case 'v':
    die("%s\n", argv0);
    break;
  default:
    usage();
  }
  ARGEND;

run:
  if (argc > 0) /* eat all remaining arguments */
    opt_cmd = argv;

  if (!opt_title)
    opt_title = (opt_line || !opt_cmd) ? "pterminal" : opt_cmd[0];

  setlocale(LC_CTYPE, "");
  XSetLocaleModifiers("");
  cols = MAX(cols, 1);
  printf("Columns %i\n", cols);
  rows = MAX(rows, 1);
  printf("Rows %i\n", rows);
  tnew(cols, rows);
  printf("now terminal col %i\n", term.col);
  create_window(cols, rows);
  init_draw_method();
  xsetenv();
  selinit();

  if (terminal_window.type == XORG) {
    window_manager_file_descriptor = XConnectionNumber(xw.display);
    wait_for_mapping();
  }else{

    while(!is_window_configured){}
    MODBIT(terminal_window.mode, 1 , MODE_VISIBLE);
  }

  cresize(terminal_window.width, terminal_window.height);

  pthread_t tty_id;
  pthread_create(&tty_id, NULL, handle_tty, NULL);

  while (1) {
    
    pthread_mutex_lock(&draw_mutex);
    if (can_draw) {
      printf("draw\n");
      draw();
      can_draw = false;
    }
    pthread_mutex_unlock(&draw_mutex);
    //sleep(1);
  }
  return 0;
}
