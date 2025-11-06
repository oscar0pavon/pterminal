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
#include "win.h"
#include "draw.h"
#include "terminal.h"

//OpenGL
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdbool.h>
#include "opengl.h"


int gl_attributes[4] = {GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None};


/* X modifiers */
#define XK_ANY_MOD UINT_MAX
#define XK_NO_MOD 0
#define XK_SWITCH_MOD (1 << 13 | 1 << 14)

/* function definitions used in config.h */
static void clipcopy(const Arg *);
static void clippaste(const Arg *);
static void numlock(const Arg *);
static void selpaste(const Arg *);
static void zoom(const Arg *);
static void zoomabs(const Arg *);
static void zoomreset(const Arg *);
static void ttysend(const Arg *);
void kscrollup(const Arg *);
void kscrolldown(const Arg *);

/* config.h for applying patches and the configuration. */
#include "config.h"

/* XEMBED messages */
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5




static inline ushort sixd_to_16bit(int);
static void xdrawglyph(Glyph, int, int);
static void xclear(int, int, int, int);
static int xgeommasktogravity(int);
static int ximopen(Display *);
static void ximinstantiate(Display *, XPointer, XPointer);
static void ximdestroy(XIM, XPointer, XPointer);
static int xicdestroy(XIC, XPointer, XPointer);
static void xinit(int, int);
static void cresize(int, int);
static void xresize(int, int);
static void xhints(void);
static int xloadcolor(int, const char *, Color *);
static int xloadfont(Font *, FcPattern *);
static void xsetenv(void);
static void xseturgency(int);
static int evcol(XEvent *);
static int evrow(XEvent *);

static void expose(XEvent *);
static void visibility(XEvent *);
static void unmap(XEvent *);
static void kpress(XEvent *);
static void cmessage(XEvent *);
static void resize(XEvent *);
static void focus(XEvent *);
static uint buttonmask(uint);
static int mouseaction(XEvent *, uint);
static void brelease(XEvent *);
static void bpress(XEvent *);
static void bmotion(XEvent *);
static void propnotify(XEvent *);
static void selnotify(XEvent *);
static void selclear_(XEvent *);
static void selrequest(XEvent *);
static void setsel(char *, Time);
static void mousesel(XEvent *, int);
static void mousereport(XEvent *);
static char *kmap(KeySym, uint);
static int match(uint, uint);

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

/* Globals */
XWindow xw;
XSelection xsel;
TermWindow win;

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

static uint buttons; /* bit field of pressed buttons */

void clipcopy(const Arg *dummy) {
  Atom clipboard;

  free(xsel.clipboard);
  xsel.clipboard = NULL;

  if (xsel.primary != NULL) {
    xsel.clipboard = xstrdup(xsel.primary);
    clipboard = XInternAtom(xw.display, "CLIPBOARD", 0);
    XSetSelectionOwner(xw.display, clipboard, xw.win, CurrentTime);
  }
}

void clippaste(const Arg *dummy) {
  Atom clipboard;

  clipboard = XInternAtom(xw.display, "CLIPBOARD", 0);
  XConvertSelection(xw.display, clipboard, xsel.xtarget, clipboard, xw.win,
                    CurrentTime);
}

void selpaste(const Arg *dummy) {
  XConvertSelection(xw.display, XA_PRIMARY, xsel.xtarget, XA_PRIMARY, xw.win,
                    CurrentTime);
}

void numlock(const Arg *dummy) { win.mode ^= MODE_NUMLOCK; }

void zoom(const Arg *arg) {
  Arg larg;

//  larg.f = usedfontsize + arg->f;
  zoomabs(&larg);
}

void zoomabs(const Arg *arg) {
  //xunloadfonts();
  //xloadfonts(usedfont, arg->f);
  cresize(0, 0);
  redraw();
  xhints();
}

void zoomreset(const Arg *arg) {
  Arg larg;

  // if (defaultfontsize > 0) {
  //   larg.f = defaultfontsize;
  //   zoomabs(&larg);
  // }
}

void ttysend(const Arg *arg) { ttywrite(arg->s, strlen(arg->s), 1); }

int evcol(XEvent *e) {
  int x = e->xbutton.x - borderpx;
  LIMIT(x, 0, win.tw - 1);
  return x / win.character_width;
}

int evrow(XEvent *e) {
  int y = e->xbutton.y - borderpx;
  LIMIT(y, 0, win.th - 1);
  return y / win.character_height;
}

void mousesel(XEvent *e, int done) {
  int type, seltype = SEL_REGULAR;
  uint state = e->xbutton.state & ~(Button1Mask | forcemousemod);

  for (type = 1; type < LEN(selmasks); ++type) {
    if (match(selmasks[type], state)) {
      seltype = type;
      break;
    }
  }
  selextend(evcol(e), evrow(e), seltype, done);
  if (done)
    setsel(getsel(), e->xbutton.time);
}

void mousereport(XEvent *e) {
  int len, btn, code;
  int x = evcol(e), y = evrow(e);
  int state = e->xbutton.state;
  char buf[40];
  static int ox, oy;

  if (e->type == MotionNotify) {
    if (x == ox && y == oy)
      return;
    if (!IS_WINDOSET(MODE_MOUSEMOTION) && !IS_WINDOSET(MODE_MOUSEMANY))
      return;
    /* MODE_MOUSEMOTION: no reporting if no button is pressed */
    if (IS_WINDOSET(MODE_MOUSEMOTION) && buttons == 0)
      return;
    /* Set btn to lowest-numbered pressed button, or 12 if no
     * buttons are pressed. */
    for (btn = 1; btn <= 11 && !(buttons & (1 << (btn - 1))); btn++)
      ;
    code = 32;
  } else {
    btn = e->xbutton.button;
    /* Only buttons 1 through 11 can be encoded */
    if (btn < 1 || btn > 11)
      return;
    if (e->type == ButtonRelease) {
      /* MODE_MOUSEX10: no button release reporting */
      if (IS_WINDOSET(MODE_MOUSEX10))
        return;
      /* Don't send release events for the scroll wheel */
      if (btn == 4 || btn == 5)
        return;
    }
    code = 0;
  }

  ox = x;
  oy = y;

  /* Encode btn into code. If no button is pressed for a motion event in
   * MODE_MOUSEMANY, then encode it as a release. */
  if ((!IS_WINDOSET(MODE_MOUSESGR) && e->type == ButtonRelease) || btn == 12)
    code += 3;
  else if (btn >= 8)
    code += 128 + btn - 8;
  else if (btn >= 4)
    code += 64 + btn - 4;
  else
    code += btn - 1;

  if (!IS_WINDOSET(MODE_MOUSEX10)) {
    code += ((state & ShiftMask) ? 4 : 0) +
            ((state & Mod1Mask) ? 8 : 0) /* meta key: alt */
            + ((state & ControlMask) ? 16 : 0);
  }

  if (IS_WINDOSET(MODE_MOUSESGR)) {
    len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", code, x + 1, y + 1,
                   e->type == ButtonRelease ? 'm' : 'M');
  } else if (x < 223 && y < 223) {
    len = snprintf(buf, sizeof(buf), "\033[M%c%c%c", 32 + code, 32 + x + 1,
                   32 + y + 1);
  } else {
    return;
  }

  ttywrite(buf, len, 0);
}

uint buttonmask(uint button) {
  return button == Button1   ? Button1Mask
         : button == Button2 ? Button2Mask
         : button == Button3 ? Button3Mask
         : button == Button4 ? Button4Mask
         : button == Button5 ? Button5Mask
                             : 0;
}

int mouseaction(XEvent *e, uint release) {
  MouseShortcut *ms;

  /* ignore Button<N>mask for Button<N> - it's set on release */
  uint state = e->xbutton.state & ~buttonmask(e->xbutton.button);

  for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
    if (ms->release == release && ms->button == e->xbutton.button &&
        (match(ms->mod, state) || /* exact or forced */
         match(ms->mod, state & ~forcemousemod))) {
      ms->func(&(ms->arg));
      return 1;
    }
  }

  return 0;
}

void bpress(XEvent *e) {
  int btn = e->xbutton.button;
  struct timespec now;
  int snap;

  if (1 <= btn && btn <= 11)
    buttons |= 1 << (btn - 1);

  if (IS_WINDOSET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
    mousereport(e);
    return;
  }

  if (mouseaction(e, 0))
    return;

  if (btn == Button1) {
    /*
     * If the user clicks below predefined timeouts specific
     * snapping behaviour is exposed.
     */
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (TIMEDIFF(now, xsel.tclick2) <= tripleclicktimeout) {
      snap = SNAP_LINE;
    } else if (TIMEDIFF(now, xsel.tclick1) <= doubleclicktimeout) {
      snap = SNAP_WORD;
    } else {
      snap = 0;
    }
    xsel.tclick2 = xsel.tclick1;
    xsel.tclick1 = now;

    selstart(evcol(e), evrow(e), snap);
  }
}

void propnotify(XEvent *e) {
  XPropertyEvent *xpev;
  Atom clipboard = XInternAtom(xw.display, "CLIPBOARD", 0);

  xpev = &e->xproperty;
  if (xpev->state == PropertyNewValue &&
      (xpev->atom == XA_PRIMARY || xpev->atom == clipboard)) {
    selnotify(e);
  }
}

void selnotify(XEvent *e) {
  ulong nitems, ofs, rem;
  int format;
  uchar *data, *last, *repl;
  Atom type, incratom, property = None;

  incratom = XInternAtom(xw.display, "INCR", 0);

  ofs = 0;
  if (e->type == SelectionNotify)
    property = e->xselection.property;
  else if (e->type == PropertyNotify)
    property = e->xproperty.atom;

  if (property == None)
    return;

  do {
    if (XGetWindowProperty(xw.display, xw.win, property, ofs, BUFSIZ / 4, False,
                           AnyPropertyType, &type, &format, &nitems, &rem,
                           &data)) {
      fprintf(stderr, "Clipboard allocation failed\n");
      return;
    }

    if (e->type == PropertyNotify && nitems == 0 && rem == 0) {
      /*
       * If there is some PropertyNotify with no data, then
       * this is the signal of the selection owner that all
       * data has been transferred. We won't need to receive
       * PropertyNotify events anymore.
       */
      MODBIT(xw.attrs.event_mask, 0, PropertyChangeMask);
      XChangeWindowAttributes(xw.display, xw.win, CWEventMask, &xw.attrs);
    }

    if (type == incratom) {
      /*
       * Activate the PropertyNotify events so we receive
       * when the selection owner does send us the next
       * chunk of data.
       */
      MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
      XChangeWindowAttributes(xw.display, xw.win, CWEventMask, &xw.attrs);

      /*
       * Deleting the property is the transfer start signal.
       */
      XDeleteProperty(xw.display, xw.win, (int)property);
      continue;
    }

    /*
     * As seen in getsel:
     * Line endings are inconsistent in the terminal and GUI world
     * copy and pasting. When receiving some selection data,
     * replace all '\n' with '\r'.
     * FIXME: Fix the computer world.
     */
    repl = data;
    last = data + nitems * format / 8;
    while ((repl = memchr(repl, '\n', last - repl))) {
      *repl++ = '\r';
    }

    if (IS_WINDOSET(MODE_BRCKTPASTE) && ofs == 0)
      ttywrite("\033[200~", 6, 0);
    ttywrite((char *)data, nitems * format / 8, 1);
    if (IS_WINDOSET(MODE_BRCKTPASTE) && rem == 0)
      ttywrite("\033[201~", 6, 0);
    XFree(data);
    /* number of 32-bit chunks returned */
    ofs += nitems * format / 32;
  } while (rem > 0);

  /*
   * Deleting the property again tells the selection owner to send the
   * next data chunk in the property.
   */
  XDeleteProperty(xw.display, xw.win, (int)property);
}

void xclipcopy(void) { clipcopy(NULL); }

void selclear_(XEvent *e) { selclear(); }

void selrequest(XEvent *e) {
  XSelectionRequestEvent *xsre;
  XSelectionEvent xev;
  Atom xa_targets, string, clipboard;
  char *seltext;

  xsre = (XSelectionRequestEvent *)e;
  xev.type = SelectionNotify;
  xev.requestor = xsre->requestor;
  xev.selection = xsre->selection;
  xev.target = xsre->target;
  xev.time = xsre->time;
  if (xsre->property == None)
    xsre->property = xsre->target;

  /* reject */
  xev.property = None;

  xa_targets = XInternAtom(xw.display, "TARGETS", 0);
  if (xsre->target == xa_targets) {
    /* respond with the supported type */
    string = xsel.xtarget;
    XChangeProperty(xsre->display, xsre->requestor, xsre->property, XA_ATOM, 32,
                    PropModeReplace, (uchar *)&string, 1);
    xev.property = xsre->property;
  } else if (xsre->target == xsel.xtarget || xsre->target == XA_STRING) {
    /*
     * xith XA_STRING non ascii characters may be incorrect in the
     * requestor. It is not our problem, use utf8.
     */
    clipboard = XInternAtom(xw.display, "CLIPBOARD", 0);
    if (xsre->selection == XA_PRIMARY) {
      seltext = xsel.primary;
    } else if (xsre->selection == clipboard) {
      seltext = xsel.clipboard;
    } else {
      fprintf(stderr, "Unhandled clipboard selection 0x%lx\n", xsre->selection);
      return;
    }
    if (seltext != NULL) {
      XChangeProperty(xsre->display, xsre->requestor, xsre->property,
                      xsre->target, 8, PropModeReplace, (uchar *)seltext,
                      strlen(seltext));
      xev.property = xsre->property;
    }
  }

  /* all done, send a notification to the listener */
  if (!XSendEvent(xsre->display, xsre->requestor, 1, 0, (XEvent *)&xev))
    fprintf(stderr, "Error sending SelectionNotify event\n");
}

void setsel(char *str, Time t) {
  if (!str)
    return;

  free(xsel.primary);
  xsel.primary = str;

  XSetSelectionOwner(xw.display, XA_PRIMARY, xw.win, t);
  if (XGetSelectionOwner(xw.display, XA_PRIMARY) != xw.win)
    selclear();
}

void xsetsel(char *str) { setsel(str, CurrentTime); }

void brelease(XEvent *e) {
  int btn = e->xbutton.button;

  if (1 <= btn && btn <= 11)
    buttons &= ~(1 << (btn - 1));

  if (IS_WINDOSET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
    mousereport(e);
    return;
  }

  if (mouseaction(e, 1))
    return;
  if (btn == Button1)
    mousesel(e, 1);
}

void bmotion(XEvent *e) {
  if (IS_WINDOSET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
    mousereport(e);
    return;
  }

  mousesel(e, 0);
}

void cresize(int width, int height) {
  int col, row;

  if (width != 0)
    win.width = width;
  if (height != 0)
    win.hight = height;

  col = (win.width - 2 * borderpx) / win.character_width;
  row = (win.hight - 2 * borderpx) / win.character_height;
  col = MAX(1, col);
  row = MAX(1, row);

  tresize(col, row);
  xresize(col, row);
  ttyresize(win.tw, win.th);
  set_ortho_projection(win.width, win.hight); 
  glViewport(0, 0, win.width, win.hight);
}

void xresize(int col, int row) {
  win.tw = col * win.character_width;
  win.th = row * win.character_height;

  xclear(0, 0, win.width, win.hight);

}

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

/*
 * Absolute coordinates.
 */
void xclear(int x1, int y1, int x2, int y2) {

}

void xhints(void) {
  XClassHint class = {opt_name ? opt_name : termname,
                      opt_class ? opt_class : termname};
  XWMHints wm = {.flags = InputHint, .input = 1};
  XSizeHints *sizeh;

  sizeh = XAllocSizeHints();

  sizeh->flags = PSize | PResizeInc | PBaseSize | PMinSize;
  sizeh->height = win.hight;
  sizeh->width = win.width;
  sizeh->height_inc = win.character_height;
  sizeh->width_inc = win.character_width;
  sizeh->base_height = 2 * borderpx;
  sizeh->base_width = 2 * borderpx;
  sizeh->min_height = win.character_height + 2 * borderpx;
  sizeh->min_width = win.character_width + 2 * borderpx;
  if (xw.isfixed) {
    sizeh->flags |= PMaxSize;
    sizeh->min_width = sizeh->max_width = win.width;
    sizeh->min_height = sizeh->max_height = win.hight;
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
  win.width = 2 * borderpx + cols * win.character_width;
  win.hight = 2 * borderpx + rows * win.character_height;
  if (xw.gm & XNegative)
    xw.left_offset += DisplayWidth(xw.display, xw.screen) - win.width - 2;
  if (xw.gm & YNegative)
    xw.top_offset += DisplayHeight(xw.display, xw.screen) - win.hight - 2;

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
      xw.display, root, xw.left_offset, xw.top_offset, win.width, win.hight, 0,
      XDefaultDepth(xw.display, xw.screen), InputOutput, xw.vis,
      CWBackPixel | CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
      &xw.attrs);

  //Open GL
  xw.gl_context = glXCreateContext(xw.display, xw.visual_info, None, GL_TRUE);
  if (!xw.gl_context) {
    die("Can't create GL context");
  }

  glXMakeCurrent(xw.display, xw.win, xw.gl_context);
  set_ortho_projection(win.width, win.hight);
  glViewport(0, 0, win.width, win.hight);
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

  win.mode = MODE_NUMLOCK;
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

  win.character_height = 32;
  win.character_width = 16;
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


  XSetForeground(xw.display, drawing_context.gc,
                 drawing_context.colors[IS_WINDOSET(MODE_REVERSE) ? defaultfg : defaultbg].pixel);
}

void xximspot(int x, int y) {
  if (xw.ime.xic == NULL)
    return;

  xw.ime.spot.x = borderpx + x * win.character_width;
  xw.ime.spot.y = borderpx + (y + 1) * win.character_height;

  XSetICValues(xw.ime.xic, XNPreeditAttributes, xw.ime.spotlist, NULL);
}

void expose(XEvent *ev) { redraw(); }

void visibility(XEvent *ev) {
  XVisibilityEvent *e = &ev->xvisibility;

  MODBIT(win.mode, e->state != VisibilityFullyObscured, MODE_VISIBLE);
}

void unmap(XEvent *ev) { win.mode &= ~MODE_VISIBLE; }

void xsetpointermotion(int set) {
  MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
  XChangeWindowAttributes(xw.display, xw.win, CWEventMask, &xw.attrs);
}

void xsetmode(int set, unsigned int flags) {
  int mode = win.mode;
  MODBIT(win.mode, set, flags);
  if ((win.mode & MODE_REVERSE) != (mode & MODE_REVERSE))
    redraw();
}

int xsetcursor(int cursor) {
  if (!BETWEEN(cursor, 0, 7)) /* 7: st extension */
    return 1;
  win.cursor = cursor;
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
    win.mode |= MODE_FOCUSED;
    xseturgency(0);
    if (IS_WINDOSET(MODE_FOCUS))
      ttywrite("\033[I", 3, 0);
  } else {
    if (xw.ime.xic)
      XUnsetICFocus(xw.ime.xic);
    win.mode &= ~MODE_FOCUSED;
    if (IS_WINDOSET(MODE_FOCUS))
      ttywrite("\033[O", 3, 0);
  }
}

int match(uint mask, uint state) {
  return mask == XK_ANY_MOD || mask == (state & ~ignoremod);
}

char *kmap(KeySym k, uint state) {
  Key *kp;
  int i;

  /* Check for mapped keys out of X11 function keys. */
  for (i = 0; i < LEN(mappedkeys); i++) {
    if (mappedkeys[i] == k)
      break;
  }
  if (i == LEN(mappedkeys)) {
    if ((k & 0xFFFF) < 0xFD00)
      return NULL;
  }

  for (kp = key; kp < key + LEN(key); kp++) {
    if (kp->k != k)
      continue;

    if (!match(kp->mask, state))
      continue;

    if (IS_WINDOSET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0)
      continue;
    if (IS_WINDOSET(MODE_NUMLOCK) && kp->appkey == 2)
      continue;

    if (IS_WINDOSET(MODE_APPCURSOR) ? kp->appcursor < 0 : kp->appcursor > 0)
      continue;

    return kp->s;
  }

  return NULL;
}

void kpress(XEvent *ev) {
  XKeyEvent *e = &ev->xkey;
  KeySym ksym = NoSymbol;
  char buf[64], *customkey;
  int len;
  Rune c;
  Status status;
  Shortcut *bp;

  if (IS_WINDOSET(MODE_KBDLOCK))
    return;

  if (xw.ime.xic) {
    len = XmbLookupString(xw.ime.xic, e, buf, sizeof buf, &ksym, &status);
    if (status == XBufferOverflow)
      return;
  } else {
    len = XLookupString(e, buf, sizeof buf, &ksym, NULL);
  }
  /* 1. shortcuts */
  for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
    if (ksym == bp->keysym && match(bp->mod, e->state)) {
      bp->func(&(bp->arg));
      return;
    }
  }

  /* 2. custom keys from config.h */
  if ((customkey = kmap(ksym, e->state))) {
    ttywrite(customkey, strlen(customkey), 1);
    return;
  }

  /* 3. composed string from input method */
  if (len == 0)
    return;
  if (len == 1 && e->state & Mod1Mask) {
    if (IS_WINDOSET(MODE_8BIT)) {
      if (*buf < 0177) {
        c = *buf | 0x80;
        len = utf8encode(c, buf);
      }
    } else {
      buf[1] = buf[0];
      buf[0] = '\033';
      len = 2;
    }
  }
  ttywrite(buf, len, 1);
}

void cmessage(XEvent *e) {
  /*
   * See xembed specs
   *  http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
   */
  if (e->xclient.message_type == xw.xembed && e->xclient.format == 32) {
    if (e->xclient.data.l[1] == XEMBED_FOCUS_IN) {
      win.mode |= MODE_FOCUSED;
      xseturgency(0);
    } else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
      win.mode &= ~MODE_FOCUSED;
    }
  } else if (e->xclient.data.l[0] == xw.wmdeletewin) {
    ttyhangup();
    exit(0);
  }
}

void resize(XEvent *e) {
  if (e->xconfigure.width == win.width && e->xconfigure.height == win.hight)
    return;

  cresize(e->xconfigure.width, e->xconfigure.height);
}

void run(void) {
  XEvent ev;
  int w = win.width, h = win.hight;
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
          win.mode |= MODE_BLINK;
        win.mode ^= MODE_BLINK;
        tsetdirtattr(ATTR_BLINK);
        lastblink = now;
        timeout = blinktimeout;
      }
    }

    //glClearColor(40 / 255.f, 44 / 255.f, 52 / 255.f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glClear(GL_COLOR_BUFFER_BIT);

    draw();

    XFlush(xw.display);

    if(is_opengl)
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
