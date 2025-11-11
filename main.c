/* See LICENSE for license details. */
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

int gl_attributes[4] = {GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None};

/* config.h for applying patches and the configuration. */
#include "config.h"

static inline ushort sixd_to_16bit(int);

static int xgeommasktogravity(int);
static void xinit(int, int);
static void xsetenv(void);

static void run(void);
static void usage(void);

static void (*handler[LASTEvent])(XEvent *) = {
    [KeyPress] = kpress,
    [ClientMessage] = cmessage,
    [ConfigureNotify] = resize,
    [VisibilityNotify] = visibility,
    [UnmapNotify] = unmap,
    [Expose] = expose,
    [FocusIn] = focus,
    [FocusOut] = focus,
    [MotionNotify] = bmotion,
    [ButtonPress] = bpress,
    [ButtonRelease] = brelease,
    /*
     * Uncomment if you want the selection to disappear when you select
     * something different in another window.
     */
    /*	[SelectionClear] = selclear_, */
    [SelectionNotify] = selnotify,
    /*
     * PropertyNotify is only turned on when there is some INCR transfer
     * happening for the selection retrieval.
     */
    [PropertyNotify] = propnotify,
    [SelectionRequest] = selrequest,
};

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
  sizeh->base_height = 2 * borderpx;
  sizeh->base_width = 2 * borderpx;
  sizeh->min_height = terminal_window.character_height + 2 * borderpx;
  sizeh->min_width = terminal_window.character_width + 2 * borderpx;
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

void xinit(int cols, int rows) {
  XGCValues gcvalues;
  Cursor cursor;
  Window parent, root;
  pid_t thispid = getpid();
  XColor xmousefg, xmousebg;

  if (!(xw.display = XOpenDisplay(NULL)))
    die("can't open display\n");
  xw.screen = XDefaultScreen(xw.display);
  xw.vis = XDefaultVisual(xw.display, xw.screen);

  xw.visual_info = glXChooseVisual(xw.display, xw.screen, gl_attributes);
  if (!xw.visual_info)
    die("can't create glx visual\n");

  xw.cmap = XCreateColormap(xw.display,
                            RootWindow(xw.display, xw.visual_info->screen),
                            xw.visual_info->visual, AllocNone);

  xloadcols();

  terminal_window.character_height = 24;
  terminal_window.character_gl_width = 32;
  terminal_window.character_gl_height = 32;
  terminal_window.character_width = 9;

  /* adjust fixed window geometry */
  terminal_window.width = cols * terminal_window.character_width;
  terminal_window.height = rows * terminal_window.character_height;

  if (xw.gm & XNegative)
    xw.left_offset +=
        DisplayWidth(xw.display, xw.screen) - terminal_window.width - 2;
  if (xw.gm & YNegative)
    xw.top_offset +=
        DisplayHeight(xw.display, xw.screen) - terminal_window.height - 2;

  /* Events */
  xw.attrs.background_pixel = drawing_context.colors[defaultbg].pixel;
  xw.attrs.border_pixel = drawing_context.colors[defaultbg].pixel;
  xw.attrs.bit_gravity = NorthWestGravity;
  xw.attrs.event_mask = FocusChangeMask | KeyPressMask | KeyReleaseMask |
                        ExposureMask | VisibilityChangeMask |
                        StructureNotifyMask | ButtonMotionMask |
                        ButtonPressMask | ButtonReleaseMask;

  xw.attrs.colormap = xw.cmap;

  root = XRootWindow(xw.display, xw.screen);
  if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0))))
    parent = root;

  xw.vis = xw.visual_info->visual;

  printf("Teminal window width %i\n", terminal_window.width);
  printf("Teminal window height %i\n", terminal_window.height);

  xw.win = XCreateWindow(
      xw.display, root, xw.left_offset, xw.top_offset, terminal_window.width,
      terminal_window.height, 0, XDefaultDepth(xw.display, xw.screen),
      InputOutput, xw.vis,
      CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
      &xw.attrs);

  // Open GL
  xw.gl_context = glXCreateContext(xw.display, xw.visual_info, None, GL_TRUE);
  if (!xw.gl_context) {
    die("Can't create GL context");
  }

  glXMakeCurrent(xw.display, xw.win, xw.gl_context);
  set_ortho_projection(terminal_window.width, terminal_window.height);
  glViewport(0, 0, terminal_window.width, terminal_window.height);
  load_texture(&font_texture_id, "/root/pterminal/font1.png");

  if (parent != root)
    XReparentWindow(xw.display, xw.win, parent, xw.left_offset, xw.top_offset);


  xw.xembed = XInternAtom(xw.display, "_XEMBED", False);
  xw.wmdeletewin = XInternAtom(xw.display, "WM_DELETE_WINDOW", False);
  xw.netwmname = XInternAtom(xw.display, "_NET_WM_NAME", False);
  xw.netwmiconname = XInternAtom(xw.display, "_NET_WM_ICON_NAME", False);
  XSetWMProtocols(xw.display, xw.win, &xw.wmdeletewin, 1);

  xw.netwmpid = XInternAtom(xw.display, "_NET_WM_PID", False);
  XChangeProperty(xw.display, xw.win, xw.netwmpid, XA_CARDINAL, 32,
                  PropModeReplace, (uchar *)&thispid, 1);

  terminal_window.mode = MODE_NUMLOCK;
  resettitle();
  xhints();
  XMapWindow(xw.display, xw.win);
  XSync(xw.display, False);

  clock_gettime(CLOCK_MONOTONIC, &xsel.tclick1);
  clock_gettime(CLOCK_MONOTONIC, &xsel.tclick2);
  xsel.primary = NULL;
  xsel.clipboard = NULL;
  xsel.xtarget = XInternAtom(xw.display, "UTF8_STRING", 0);
  if (xsel.xtarget == None)
    xsel.xtarget = XA_STRING;
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

void run(void) {
  XEvent event;

  int width = terminal_window.width;
  int height = terminal_window.height;

  fd_set rfd;

  int xorg_file_descriptor = XConnectionNumber(xw.display);

  int tty_file_descriptor, xev, drawing;

  struct timespec seltv, *tv, now, lastblink, trigger;
  double timeout;

  /* Waiting for window mapping */
  do {
    XNextEvent(xw.display, &event);

    if (event.type == ConfigureNotify) {
      width = event.xconfigure.width;
      height = event.xconfigure.height;
    }

  } while (event.type != MapNotify);

  tty_file_descriptor = ttynew(opt_line, shell, opt_io, opt_cmd);
  cresize(width, height);

  for (timeout = -1, drawing = 0, lastblink = (struct timespec){0};;) {
    FD_ZERO(&rfd);
    FD_SET(tty_file_descriptor, &rfd);
    FD_SET(xorg_file_descriptor, &rfd);

    if (XPending(xw.display))
      timeout = 0; /* existing events might not set xfd */

    seltv.tv_sec = timeout / 1E3;
    seltv.tv_nsec = 1E6 * (timeout - 1E3 * seltv.tv_sec);
    tv = timeout >= 0 ? &seltv : NULL;

    if (pselect(MAX(xorg_file_descriptor, tty_file_descriptor) + 1, &rfd, NULL,
                NULL, tv, NULL) < 0) {
      if (errno == EINTR)
        continue;
      die("select failed: %s\n", strerror(errno));
    }
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (FD_ISSET(tty_file_descriptor, &rfd))
      ttyread();

    xev = 0;
    while (XPending(xw.display)) {
      xev = 1;
      XNextEvent(xw.display, &event);
      if (XFilterEvent(&event, None))
        continue;
      if (handler[event.type])
        (handler[event.type])(&event);
    }

    /*
     * To reduce flicker and tearing, when new content or event
     * triggers drawing, we first wait a bit to ensure we got
     * everything, and if nothing new arrives - we draw.
     * We start with trying to wait minlatency ms. If more content
     * arrives sooner, we retry with shorter and shorter periods,
     * and eventually draw even without idle after maxlatency ms.
     * Typically this results in low latency while interacting,
     * maximum latency intervals during `cat huge.txt`, and perfect
     * sync with periodic updates from animations/key-repeats/etc.
     */
    if (FD_ISSET(tty_file_descriptor, &rfd) || xev) {
      if (!drawing) {
        trigger = now;
        drawing = 1;
      }
      timeout = (maxlatency - TIMEDIFF(now, trigger)) / maxlatency * minlatency;
      if (timeout > 0)
        continue; /* we have time, try to find idle */
    }

    /* idle detected or maxlatency exhausted -> draw */
    timeout = -1;
    if (blinktimeout && tattrset(ATTR_BLINK)) {
      timeout = blinktimeout - TIMEDIFF(now, lastblink);
      if (timeout <= 0) {
        if (-timeout > blinktimeout) /* start visible */
          terminal_window.mode |= MODE_BLINK;
        terminal_window.mode ^= MODE_BLINK;
        tsetdirtattr(ATTR_BLINK);
        lastblink = now;
        timeout = blinktimeout;
      }
    }

    draw();

    XFlush(xw.display);

    drawing = 0;
  }
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
  xinit(cols, rows);
  xsetenv();
  selinit();
  run();

  return 0;
}
