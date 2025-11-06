/* See LICENSE for license details. */
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

char *argv0;
#include "arg.h"
#include "window.h"
#include "draw.h"
#include "terminal.h"

//OpenGL
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdbool.h>
#include "opengl.h"

#include "mouse.h"
#include "input.h"

int gl_attributes[4] = {GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None};




/* config.h for applying patches and the configuration. */
#include "config.h"

/* XEMBED messages */
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5




static inline ushort sixd_to_16bit(int);
static void xdrawglyph(Glyph, int, int);
static int xgeommasktogravity(int);
static int ximopen(Display *);
static void ximinstantiate(Display *, XPointer, XPointer);
static void ximdestroy(XIM, XPointer, XPointer);
static int xicdestroy(XIC, XPointer, XPointer);
static void xinit(int, int);
static int xloadcolor(int, const char *, Color *);
static int xloadfont(Font *, FcPattern *);
static void xsetenv(void);
static void xseturgency(int);
static int evcol(XEvent *);
static int evrow(XEvent *);

static void visibility(XEvent *);
static void unmap(XEvent *);
static void cmessage(XEvent *);
static void focus(XEvent *);
static uint buttonmask(uint);
static int mouseaction(XEvent *, uint);

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


static char *opt_class = NULL;
static char **opt_cmd = NULL;
static char *opt_embed = NULL;
static char *opt_font = NULL;
static char *opt_io = NULL;
static char *opt_line = NULL;
static char *opt_name = NULL;
static char *opt_title = NULL;





void ttysend(const Arg *arg) { ttywrite(arg->s, strlen(arg->s), 1); }




ushort sixd_to_16bit(int x) { return x == 0 ? 0 : 0x3737 + 0x2828 * x; }

int xloadcolor(int i, const char *name, Color *ncolor) {
  XRenderColor color = {.alpha = 0xffff};

  if (!name) {
    if (BETWEEN(i, 16, 255)) {  /* 256 color */
      if (i < 6 * 6 * 6 + 16) { /* same colors as xterm */
        color.red = sixd_to_16bit(((i - 16) / 36) % 6);
        color.green = sixd_to_16bit(((i - 16) / 6) % 6);
        color.blue = sixd_to_16bit(((i - 16) / 1) % 6);
      } else { /* greyscale */
        color.red = 0x0808 + 0x0a0a * (i - (6 * 6 * 6 + 16));
        color.green = color.blue = color.red;
      }
      return XftColorAllocValue(xw.display, xw.vis, xw.cmap, &color, ncolor);
    } else
      name = colorname[i];
  }

  return XftColorAllocName(xw.display, xw.vis, xw.cmap, name, ncolor);
}

void xloadcols(void) {
  int i;
  static int loaded;
  Color *cp;

  if (loaded) {
    for (cp = drawing_context.colors; cp < &drawing_context.colors[drawing_context.collen]; ++cp)
      XftColorFree(xw.display, xw.vis, xw.cmap, cp);
  } else {
    drawing_context.collen = MAX(LEN(colorname), 256);
    drawing_context.colors = xmalloc(drawing_context.collen * sizeof(Color));
  }

  for (i = 0; i < drawing_context.collen; i++)
    if (!xloadcolor(i, NULL, &drawing_context.colors[i])) {
      if (colorname[i])
        die("could not allocate color '%s'\n", colorname[i]);
      else
        die("could not allocate color %d\n", i);
    }
  loaded = 1;
}

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b) {
  if (!BETWEEN(x, 0, drawing_context.collen - 1))
    return 1;

  *r = drawing_context.colors[x].color.red >> 8;
  *g = drawing_context.colors[x].color.green >> 8;
  *b = drawing_context.colors[x].color.blue >> 8;

  return 0;
}

int xsetcolorname(int x, const char *name) {
  Color ncolor;

  if (!BETWEEN(x, 0, drawing_context.collen - 1))
    return 1;

  if (!xloadcolor(x, name, &ncolor))
    return 1;

  XftColorFree(xw.display, xw.vis, xw.cmap, &drawing_context.colors[x]);
  drawing_context.colors[x] = ncolor;

  return 0;
}


void xhints(void) {
  XClassHint class = {opt_name ? opt_name : termname,
                      opt_class ? opt_class : termname};
  XWMHints wm = {.flags = InputHint, .input = 1};
  XSizeHints *sizeh;

  sizeh = XAllocSizeHints();

  sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
  sizeh->height = terminal_window.hight;
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
    sizeh->min_height = sizeh->max_height = terminal_window.hight;
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


int ximopen(Display *dpy) {
  XIMCallback imdestroy = {.client_data = NULL, .callback = ximdestroy};
  XICCallback icdestroy = {.client_data = NULL, .callback = xicdestroy};

  xw.ime.xim = XOpenIM(xw.display, NULL, NULL, NULL);
  if (xw.ime.xim == NULL)
    return 0;

  if (XSetIMValues(xw.ime.xim, XNDestroyCallback, &imdestroy, NULL))
    fprintf(stderr, "XSetIMValues: "
                    "Could not set XNDestroyCallback.\n");

  xw.ime.spotlist = XVaCreateNestedList(0, XNSpotLocation, &xw.ime.spot, NULL);

  if (xw.ime.xic == NULL) {
    xw.ime.xic = XCreateIC(xw.ime.xim, XNInputStyle,
                           XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                           xw.win, XNDestroyCallback, &icdestroy, NULL);
  }
  if (xw.ime.xic == NULL)
    fprintf(stderr, "XCreateIC: Could not create input context.\n");

  return 1;
}

void ximinstantiate(Display *dpy, XPointer client, XPointer call) {
  if (ximopen(dpy))
    XUnregisterIMInstantiateCallback(xw.display, NULL, NULL, NULL, ximinstantiate,
                                     NULL);
}

void ximdestroy(XIM xim, XPointer client, XPointer call) {
  xw.ime.xim = NULL;
  XRegisterIMInstantiateCallback(xw.display, NULL, NULL, NULL, ximinstantiate,
                                 NULL);
  XFree(xw.ime.spotlist);
}

int xicdestroy(XIC xim, XPointer client, XPointer call) {
  xw.ime.xic = NULL;
  return 1;
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

  //OpenGL
  if (is_opengl) {

    xw.visual_info = glXChooseVisual(xw.display, xw.screen, gl_attributes);
    if (!xw.visual_info)
      die("can't create glx visual\n");
  }

  /* font */
  if (!FcInit())
    die("could not init fontconfig.\n");


  xw.cmap = XCreateColormap(xw.display,
                            RootWindow(xw.display, xw.visual_info->screen),
                            xw.visual_info->visual, AllocNone);

  xloadcols();

  /* adjust fixed window geometry */
  terminal_window.width = 2 * borderpx + cols * terminal_window.character_width;
  terminal_window.hight = 2 * borderpx + rows * terminal_window.character_height;
  if (xw.gm & XNegative)
    xw.left_offset += DisplayWidth(xw.display, xw.screen) - terminal_window.width - 2;
  if (xw.gm & YNegative)
    xw.top_offset += DisplayHeight(xw.display, xw.screen) - terminal_window.hight - 2;

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

  xw.win = XCreateWindow(
      xw.display, root, xw.left_offset, xw.top_offset, terminal_window.width, terminal_window.hight, 0,
      XDefaultDepth(xw.display, xw.screen), InputOutput, xw.vis,
      CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
      &xw.attrs);

  //Open GL
  xw.gl_context = glXCreateContext(xw.display, xw.visual_info, None, GL_TRUE);
  if (!xw.gl_context) {
    die("Can't create GL context");
  }

  glXMakeCurrent(xw.display, xw.win, xw.gl_context);
  set_ortho_projection(terminal_window.width, terminal_window.hight);
  glViewport(0, 0, terminal_window.width, terminal_window.hight);
  load_texture(&font_texture_id, "/root/pterminal/font1.png");

  if (parent != root)
    XReparentWindow(xw.display, xw.win, parent, xw.left_offset, xw.top_offset);



  /* input methods */
  if (!ximopen(xw.display)) {
    XRegisterIMInstantiateCallback(xw.display, NULL, NULL, NULL, ximinstantiate,
                                   NULL);
  }

  /* white cursor, black outline */
  cursor = XCreateFontCursor(xw.display, mouseshape);
  XDefineCursor(xw.display, xw.win, cursor);

  if (XParseColor(xw.display, xw.cmap, colorname[mousefg], &xmousefg) == 0) {
    xmousefg.red = 0xffff;
    xmousefg.green = 0xffff;
    xmousefg.blue = 0xffff;
  }

  if (XParseColor(xw.display, xw.cmap, colorname[mousebg], &xmousebg) == 0) {
    xmousebg.red = 0x0000;
    xmousebg.green = 0x0000;
    xmousebg.blue = 0x0000;
  }

  XRecolorCursor(xw.display, cursor, &xmousefg, &xmousebg);

  xw.xembed = XInternAtom(xw.display, "_XEMBED", False);
  xw.wmdeletewin = XInternAtom(xw.display, "WM_DELETE_WINDOW", False);
  xw.netwmname = XInternAtom(xw.display, "_NET_WM_NAME", False);
  xw.netwmiconname = XInternAtom(xw.display, "_NET_WM_ICON_NAME", False);
  XSetWMProtocols(xw.display, xw.win, &xw.wmdeletewin, 1);

  xw.netwmpid = XInternAtom(xw.display, "_NET_WM_PID", False);
  XChangeProperty(xw.display, xw.win, xw.netwmpid, XA_CARDINAL, 32, PropModeReplace,
                  (uchar *)&thispid, 1);

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

  terminal_window.character_height = 32;
  terminal_window.character_width = 16;
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


void xximspot(int x, int y) {
  if (xw.ime.xic == NULL)
    return;

  xw.ime.spot.x = borderpx + x * terminal_window.character_width;
  xw.ime.spot.y = borderpx + (y + 1) * terminal_window.character_height;

  XSetICValues(xw.ime.xic, XNPreeditAttributes, xw.ime.spotlist, NULL);
}


void visibility(XEvent *ev) {
  XVisibilityEvent *e = &ev->xvisibility;

  MODBIT(terminal_window.mode, e->state != VisibilityFullyObscured, MODE_VISIBLE);
}

void unmap(XEvent *ev) { terminal_window.mode &= ~MODE_VISIBLE; }

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

void xseturgency(int add) {
  XWMHints *h = XGetWMHints(xw.display, xw.win);

  MODBIT(h->flags, add, XUrgencyHint);
  XSetWMHints(xw.display, xw.win, h);
  XFree(h);
}

void xbell(void) {
  if (!(IS_WINDOSET(MODE_FOCUSED)))
    xseturgency(1);
  if (bellvolume)
    XkbBell(xw.display, xw.win, bellvolume, (Atom)NULL);
}

void focus(XEvent *ev) {
  XFocusChangeEvent *e = &ev->xfocus;

  if (e->mode == NotifyGrab)
    return;

  if (ev->type == FocusIn) {
    if (xw.ime.xic)
      XSetICFocus(xw.ime.xic);
    terminal_window.mode |= MODE_FOCUSED;
    xseturgency(0);
    if (IS_WINDOSET(MODE_FOCUS))
      ttywrite("\033[I", 3, 0);
  } else {
    if (xw.ime.xic)
      XUnsetICFocus(xw.ime.xic);
    terminal_window.mode &= ~MODE_FOCUSED;
    if (IS_WINDOSET(MODE_FOCUS))
      ttywrite("\033[O", 3, 0);
  }
}



void cmessage(XEvent *e) {
  /*
   * See xembed specs
   *  http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
   */
  if (e->xclient.message_type == xw.xembed && e->xclient.format == 32) {
    if (e->xclient.data.l[1] == XEMBED_FOCUS_IN) {
      terminal_window.mode |= MODE_FOCUSED;
      xseturgency(0);
    } else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
      terminal_window.mode &= ~MODE_FOCUSED;
    }
  } else if (e->xclient.data.l[0] == xw.wmdeletewin) {
    ttyhangup();
    exit(0);
  }
}


void run(void) {
  XEvent ev;
  int w = terminal_window.width, h = terminal_window.hight;
  fd_set rfd;
  int xfd = XConnectionNumber(xw.display), ttyfd, xev, drawing;
  struct timespec seltv, *tv, now, lastblink, trigger;
  double timeout;

  /* Waiting for window mapping */
  do {
    XNextEvent(xw.display, &ev);
    /*
     * This XFilterEvent call is required because of XOpenIM. It
     * does filter out the key event and some client message for
     * the input method too.
     */
    if (XFilterEvent(&ev, None))
      continue;
    if (ev.type == ConfigureNotify) {
      w = ev.xconfigure.width;
      h = ev.xconfigure.height;
    }
  } while (ev.type != MapNotify);

  ttyfd = ttynew(opt_line, shell, opt_io, opt_cmd);
  cresize(w, h);

  for (timeout = -1, drawing = 0, lastblink = (struct timespec){0};;) {
    FD_ZERO(&rfd);
    FD_SET(ttyfd, &rfd);
    FD_SET(xfd, &rfd);

    if (XPending(xw.display))
      timeout = 0; /* existing events might not set xfd */

    seltv.tv_sec = timeout / 1E3;
    seltv.tv_nsec = 1E6 * (timeout - 1E3 * seltv.tv_sec);
    tv = timeout >= 0 ? &seltv : NULL;

    if (pselect(MAX(xfd, ttyfd) + 1, &rfd, NULL, NULL, tv, NULL) < 0) {
      if (errno == EINTR)
        continue;
      die("select failed: %s\n", strerror(errno));
    }
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (FD_ISSET(ttyfd, &rfd))
      ttyread();

    xev = 0;
    while (XPending(xw.display)) {
      xev = 1;
      XNextEvent(xw.display, &ev);
      if (XFilterEvent(&ev, None))
        continue;
      if (handler[ev.type])
        (handler[ev.type])(&ev);
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
    if (FD_ISSET(ttyfd, &rfd) || xev) {
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

    //glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glClear(GL_COLOR_BUFFER_BIT);

    draw();

    //XFlush(xw.display);

    glXSwapBuffers(xw.display, xw.win);

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
    xw.gm = XParseGeometry(EARGF(usage()), &xw.left_offset, &xw.top_offset, &cols, &rows);
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
    die("%s " VERSION "\n", argv0);
    break;
  default:
    usage();
  }
  ARGEND;

run:
  if (argc > 0) /* eat all remaining arguments */
    opt_cmd = argv;

  if (!opt_title)
    opt_title = (opt_line || !opt_cmd) ? "st" : opt_cmd[0];

  setlocale(LC_CTYPE, "");
  XSetLocaleModifiers("");
  cols = MAX(cols, 1);
  rows = MAX(rows, 1);
  tnew(cols, rows);
  xinit(cols, rows);
  xsetenv();
  selinit();
  run();

  return 0;
}
