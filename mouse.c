#include "mouse.h"
#include "window.h"
#include "macros.h"
#include "draw.h"
#include "types.h"
#include "input.h"
#include "terminal.h"
#include <errno.h>



/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
static MouseShortcut mshortcuts[] = {
	/* mask                 button   function        argument       release */
	{ ShiftMask,            Button4, kscrollup,      {.f = -0.1} },
	{ ShiftMask,            Button5, kscrolldown,    {.f = -0.1} },
	{ XK_ANY_MOD,           Button2, selpaste,       {.i = 0},      1 },
	{ ShiftMask,            Button4, ttysend,        {.s = "\033[5;2~"} },
	{ XK_ANY_MOD,           Button4, ttysend,        {.s = "\031"} },
	{ ShiftMask,            Button5, ttysend,        {.s = "\033[6;2~"} },
	{ XK_ANY_MOD,           Button5, ttysend,        {.s = "\005"} },
};

/* selection timeouts (in milliseconds) */
static unsigned int doubleclicktimeout = 300;
static unsigned int tripleclicktimeout = 600;

static uint buttons; /* bit field of pressed buttons */
/*
 * Force mouse select/shortcuts while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use ShiftMask with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static uint forcemousemod = ShiftMask;
/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static uint selmasks[] = {
	[SEL_RECTANGULAR] = Mod1Mask,
};


static void selclear_(XEvent *);
static void setsel(char *, Time);
static void mousesel(XEvent *, int);
static void mousereport(XEvent *);

char *xstrdup(const char *s) {
  char *p;

  if ((p = strdup(s)) == NULL)
    die("strdup: %s\n", strerror(errno));

  return p;
}

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

void selclear(void) {
  if (sel.ob.x == -1)
    return;
  sel.mode = SEL_IDLE;
  sel.ob.x = -1;
  tsetdirt(sel.nb.y, sel.ne.y);
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
