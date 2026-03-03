#include "mouse.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "window.h"
#include "macros.h"
#include "draw.h"
#include "types.h"
#include "input.h"
#include "terminal.h"
#include "selection.h"

#include "wayland/mouse.h"

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


int mouse_code = 0;

uint buttons;

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


int mouse_to_col() {
  int x = main_mouse.x;
  LIMIT(x, 0, terminal_window.tty_width - 1);
  return x / terminal_window.character_width;
}


int mouse_to_row() {
  int y = main_mouse.y;
  LIMIT(y, 0, terminal_window.tty_height - 1);
  return y / terminal_window.character_height;
}


void select_with_mouse(bool done) {
  if( !main_mouse.left_button.pressed )
    return;

  int type, seltype = SEL_REGULAR;

  selextend(mouse_to_col(), mouse_to_row(), seltype, done);

  // if (done)
  //   setsel(getsel(), e->xbutton.time);
  //   //TODO create a clipboard for wayland
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
  // selextend(evcol(e), evrow(e), seltype, done);
  if (done)
    setsel(getsel(), e->xbutton.time);
}

void update_mouse_terminal_position(){

  main_mouse.col = mouse_to_col();
  main_mouse.row = mouse_to_row();
}

uint32_t report_mouse_movement(void){

  uint32_t movement_code = 35;

  if (IS_WINDOSET(MODE_MOUSEMOTION)){
    if(main_mouse.current_button){
      if (main_mouse.current_button->pressed) {
        return 32;
      
      }else {
        return 35; 
      }

    }else if( IS_WINDOSET(MODE_MOUSEMANY) ){
      return 35;
    }
  }
  if( IS_WINDOSET(MODE_MOUSEMANY) )
    return 35;



}

void report_mouse(bool has_motion) {

  int len, btn;
  char buf[40];

  if(main_mouse.current_button){
    btn = main_mouse.current_button->id;
  }

  main_mouse.old_col = main_mouse.col;
  main_mouse.old_row = main_mouse.row;

  char is_released;
  if(has_motion){

    uint32_t movement_code = report_mouse_movement();

    if(!main_mouse.current_button)
      btn = 0;

    mouse_code = btn + movement_code;
  }
  else{
    mouse_code = btn;
  }

  if(!has_motion && !main_mouse.current_button)
    return;

  // if (!IS_WINDOSET(MODE_MOUSEX10)) {
  //   code += ((state & ShiftMask) ? 4 : 0) +
  //           ((state & Mod1Mask) ? 8 : 0) /* meta key: alt */
  //           + ((state & ControlMask) ? 16 : 0);
  // }

  if(main_mouse.current_button){
    if(main_mouse.current_button->released){
      is_released = 'm';
    }else{
      is_released = 'M';
    }
  }

  if(has_motion)
    is_released = 'M';

  if (IS_WINDOSET(MODE_MOUSESGR)) {
    //printf("Code: %i %c \n", mouse_code, is_released);
    len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", 
        mouse_code, main_mouse.col + 1, main_mouse.row + 1,
        is_released);
  } else {
    if( IS_WINDOSET(MODE_MOUSEMANY) ){
      printf("mouse many mode\n");
    }
    return;
  }

  ttywrite(buf, len, 0);
  
  mouse_code = 0;
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

void release_button(){


  if ( IS_WINDOSET(MODE_MOUSE) ) {
    report_mouse(false);
    return;
  }

}

void mouse_click(){

  struct timespec now;
  int snap;

  int btn;

  if(main_mouse.current_button)
    btn = main_mouse.current_button->id;


  if ( IS_WINDOSET(MODE_MOUSE) ) {
    report_mouse(false);
    return;
  }

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

  selstart(mouse_to_col(),mouse_to_row(), snap);
  // printf("Mouse col %i row %i\n", mouse_to_col(), mouse_to_row());
  // printf("Mouse x %i, mouse y %i\n", main_mouse.x, main_mouse.y);
}

void bpress(XEvent *e) {
  int btn = e->xbutton.button;
  struct timespec now;
  int snap;

  if (1 <= btn && btn <= 11)
    buttons |= 1 << (btn - 1);

  if (IS_WINDOSET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
    report_mouse(false);
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

    // selstart(evcol(e), evrow(e), snap);
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
  if (selection.original_beginning.x == -1)
    return;
  selection.mode = SEL_IDLE;
  selection.original_beginning.x = -1;
  tsetdirt(selection.beginning_normalized.y, selection.end_normalized.y);
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
    report_mouse(false);
    return;
  }

  if (mouseaction(e, 1))
    return;
  if (btn == Button1)
    mousesel(e, 1);
}

void handle_mouse_motion(bool has_motion){

  if ( IS_WINDOSET(MODE_MOUSE) ) {
    report_mouse(has_motion);
    return;
  }
  
  select_with_mouse(false);
}

void bmotion(XEvent *e) {
  if (IS_WINDOSET(MODE_MOUSE) && !(e->xbutton.state & forcemousemod)) {
    report_mouse(true);
    return;
  }

  mousesel(e, 0);
}
