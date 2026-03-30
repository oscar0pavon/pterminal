#include "mouse.h"

#include <errno.h>
#include <pway/mouse.h>
#include <pway/pway.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pterminal.h"
#include "window.h"
#include "macros.h"
#include "types.h"
#include "input.h"
#include "terminal.h"
#include "selection.h"

Mouse main_mouse;

static MouseShortcut mshortcuts[] = {
	/* mask                 button   function        argument       release */
	{ ShiftMask,            Button4, kscrollup,      {.f = -0.1} },
	{ ShiftMask,            Button5, kscrolldown,    {.f = -0.1} },
	//{ XK_ANY_MOD,           Button2, selpaste,       {.i = 0},      1 },
	{ ShiftMask,            Button4, ttysend,        {.s = "\033[5;2~"} },
	{ XK_ANY_MOD,           Button4, ttysend,        {.s = "\031"} },
	{ ShiftMask,            Button5, ttysend,        {.s = "\033[6;2~"} },
	{ XK_ANY_MOD,           Button5, ttysend,        {.s = "\005"} },
};

/* selection timeouts (in milliseconds) */
static unsigned int doubleclicktimeout = 300;
static unsigned int tripleclicktimeout = 600;


int mouse_to_col() {
  int x = pway->mouse.x;
  LIMIT(x, 0, terminal_window.tty_width - 1);
  return x / terminal_window.character_width;
}


int mouse_to_row() {
  int y = pway->mouse.y;
  LIMIT(y, 0, terminal_window.tty_height - 1);
  return y / terminal_window.character_height;
}

void mouse_click(){

  struct timespec now;
  int snap;

  int btn;

  if(pway->mouse.current_button)
    btn = pway->mouse.current_button->id;


  if ( IS_WINDOSET(MODE_MOUSE) ) {
    send_mouse_info_to_tty();
    return;
  }
  

  if(!pway->mouse.left_button.pressed)
    return;

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
  pway_set_text_cursor();
  // printf("Mouse col %i row %i\n", mouse_to_col(), mouse_to_row());
  // printf("Mouse x %i, mouse y %i\n", main_mouse.x, main_mouse.y);
  
}

void release_button(){


  if ( IS_WINDOSET(MODE_MOUSE) ) {
    send_mouse_info_to_tty();
    return;
  }

  printf("release button\n");

  if(pway->mouse.wheel_down.released){
     Arg new_arg;
     new_arg.f = -0.1f;
     kscrollup(&new_arg);
     can_draw = true;
     return;
  }
  
  if(pway->mouse.wheel_up.released){
     Arg new_arg;
     new_arg.f = -0.1f;
     kscrolldown(&new_arg);
     can_draw = true;
     return;
  }

  if(pway->mouse.middle_button.released){
    paste_from_clipboard(true);
    printf("paste\n");
    can_draw = true;
    return;
  }

  if(pway->mouse.left_button.released){
    printf("released left click\n");
    select_with_mouse(true);
  }

}

void select_with_mouse(bool done) {

  int type, seltype = SEL_REGULAR;

  selextend(mouse_to_col(), mouse_to_row(), seltype, done);

  if (done){
    printf("seletion: %s\n", get_selection());
    pway_primary_copy();
    pway_set_default_cursor();
  }

  can_draw = true;

}


void update_mouse(){

  main_mouse.col = mouse_to_col();
  main_mouse.row = mouse_to_row();

  if ( IS_WINDOSET(MODE_MOUSE) ) {
    send_mouse_info_to_tty();
    return;
  }
 
  if(pway->mouse.left_button.pressed)
    select_with_mouse(false);
}
#define DRAG 32
#define NO_BUTTON 35

void send_mouse_scape(int code, int col, int row, char release){

  int len;
  char buf[40];

  len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", 
      code, col, row,
      release);

  write_to_tty(buf, len, 0);

  //printf("Code: %i %c\n",code, release);
}

bool drag = false;

void send_mouse_info_to_tty() {

  if (!IS_WINDOSET(MODE_MOUSESGR))
    return;

  main_mouse.old_col = main_mouse.col;
  main_mouse.old_row = main_mouse.row;

  char is_released;

  is_released = 'M';

  if(pway->mouse.current_button){

   int code = pway->mouse.current_button->id;

    if(pway->mouse.current_button->released){
      is_released = 'm';

      if(pway->mouse.wheel_down.released || pway->mouse.wheel_up.released){
        is_released = 'M';
      }

      send_mouse_scape(code, main_mouse.col + 1, main_mouse.row + 1, is_released);
      drag = false;
      return;
    }
    if(pway->mouse.current_button->pressed){
      is_released = 'M';

      if(!drag)
        send_mouse_scape(code, main_mouse.col + 1, main_mouse.row + 1, is_released);

      drag = true;

      if(IS_WINDOSET(MODE_MOUSEMOTION)){
        code += DRAG;
      }
      send_mouse_scape(code, main_mouse.col + 1, main_mouse.row + 1, is_released);
      return;
    }

  }else {
    if(IS_WINDOSET(MODE_MOUSEMANY)){
      is_released = 'M';
      send_mouse_scape(NO_BUTTON, main_mouse.col + 1, main_mouse.row + 1, is_released);
      return;
    }
  }

}

bool is_on_mouse_mode(){
  return IS_WINDOSET(MODE_MOUSE);
}


void selclear(void) {
  if (selection.original_beginning.x == -1)
    return;
  selection.mode = SEL_IDLE;
  selection.original_beginning.x = -1;
  tsetdirt(selection.beginning_normalized.y, selection.end_normalized.y);
}


