#include "mouse.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "wayland/data_copy.h"
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

  int type, seltype = SEL_REGULAR;

  selextend(mouse_to_col(), mouse_to_row(), seltype, done);

  if (done)
    perform_copy_primary();

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

  if (!IS_WINDOSET(MODE_MOUSESGR))
    return;

  if(!has_motion && !main_mouse.current_button)
    return;

  int len, button;
  char buf[40];
  char is_released;

  if(has_motion)
    is_released = 'M';

  if(main_mouse.current_button){

    button = main_mouse.current_button->id;

    if(main_mouse.current_button->released){
      is_released = 'm';
    }else{
      is_released = 'M';
    }

  }else {
    button = 0;
  }
  

  main_mouse.old_col = main_mouse.col;
  main_mouse.old_row = main_mouse.row;

  if(has_motion){

    uint32_t movement_code = report_mouse_movement();

    mouse_code = button + movement_code;
  }
  else{

    mouse_code = button;

  }

  //printf("Code: %i %c \n", mouse_code, is_released);
  len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", 
      mouse_code, main_mouse.col + 1, main_mouse.row + 1,
      is_released);

  write_to_tty(buf, len, 0);
  
  mouse_code = 0;
}


void release_button(){


  if ( IS_WINDOSET(MODE_MOUSE) ) {
    report_mouse(false);
    return;
  }

  printf("release button\n");

  if(main_mouse.left_button.released){
      printf("released left click\n");
    select_with_mouse(true);
  }

}

bool is_on_mouse_mode(){
  return IS_WINDOSET(MODE_MOUSE);
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


void selclear(void) {
  if (selection.original_beginning.x == -1)
    return;
  selection.mode = SEL_IDLE;
  selection.original_beginning.x = -1;
  tsetdirt(selection.beginning_normalized.y, selection.end_normalized.y);
}




void handle_mouse_motion(bool has_motion){

  if ( IS_WINDOSET(MODE_MOUSE) ) {
    report_mouse(has_motion);
    return;
  }
 
  if(main_mouse.left_button.pressed)
    select_with_mouse(false);
}

