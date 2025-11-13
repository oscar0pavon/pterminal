#include "xorg.h"
#include "window.h"
#include <stdlib.h>
#include <unistd.h>
#include "terminal.h"
#include "opengl.h"

void wait_for_mapping(){

  int width = terminal_window.width;
  int height = terminal_window.height;

  XEvent event;
  /* Waiting for window mapping */
  do {
    XNextEvent(xw.display, &event);

    if (event.type == ConfigureNotify) {
      width = event.xconfigure.width;
      height = event.xconfigure.height;
    }

  } while (event.type != MapNotify);
  
  cresize(width, height);
}

void create_x_window(){

  Window parent, root;
  pid_t thispid = getpid();

  if (!(xw.display = XOpenDisplay(NULL)))
    die("can't open display\n");
  xw.screen = XDefaultScreen(xw.display);

  xw.visual_info = glXChooseVisual(xw.display, xw.screen, gl_attributes);
  if (!xw.visual_info)
    die("can't create glx visual\n");

  xw.cmap = XCreateColormap(xw.display,
                            RootWindow(xw.display, xw.visual_info->screen),
                            xw.visual_info->visual, AllocNone);
  if (xw.gm & XNegative)
    xw.left_offset +=
        DisplayWidth(xw.display, xw.screen) - terminal_window.width - 2;
  if (xw.gm & YNegative)
    xw.top_offset +=
        DisplayHeight(xw.display, xw.screen) - terminal_window.height - 2;

  /* Events */
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
