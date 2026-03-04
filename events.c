#include "events.h"
#include "macros.h"
#include "terminal.h"
#include "input.h"
#include "mouse.h"
#include "window.h"
#include <stdbool.h>


void focus_window(bool is_focuses){
  if(!terminal_window.is_ready)
    return;
  
  if(is_focuses){

    terminal_window.mode |= MODE_FOCUSED;

    if (IS_WINDOSET(MODE_FOCUS))
      write_to_tty("\033[I", 3, 0);

  }else{

    terminal_window.mode &= ~MODE_FOCUSED;

    if (IS_WINDOSET(MODE_FOCUS))
      write_to_tty("\033[O", 3, 0);
  }
}

