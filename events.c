#include "events.h"
#include "macros.h"
#include "terminal.h"
#include "input.h"
#include "mouse.h"

/* XEMBED messages */
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5

void (*event_handler[LASTEvent])(XEvent *) = {
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

void xseturgency(int add) {
  XWMHints *h = XGetWMHints(xw.display, xw.win);

  MODBIT(h->flags, add, XUrgencyHint);
  XSetWMHints(xw.display, xw.win, h);
  XFree(h);
}

void visibility(XEvent *ev) {
  XVisibilityEvent *e = &ev->xvisibility;

  MODBIT(terminal_window.mode, e->state != VisibilityFullyObscured, MODE_VISIBLE);
}

void unmap(XEvent *ev) { terminal_window.mode &= ~MODE_VISIBLE; }

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

  if (e->xclient.data.l[0] == xw.wmdeletewin) {
    exit_pterminal();
  }

}
