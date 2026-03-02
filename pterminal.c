#include "pterminal.h"

#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "window.h"
#include "wayland/keyboard.h"
#include "tty.h"

#include "draw.h"

#include <poll.h>

static char *shell = "/bin/sh";

void *run_pterminal(void *none) {

  fd_set read_file_descriptor;

  int tty_file_descriptor;

  bool have_event, drawing;

  struct timespec seltv, *wait_time, now, lastblink, trigger;
  double timeout;

  tty_file_descriptor = ttynew(opt_line, shell, opt_io, opt_cmd);

  struct pollfd fds[] = {
    { tty_file_descriptor, POLLIN, 0 },
    { main_keyboard.timer_fd, POLLIN, 0 }
  };

  while (1) {

    if (poll(fds, 2, -1) == -1) {
      perror("Poll in fds, TTY or Keyboard timer");
    }


    //keyboard key repeat
    if( fds[1].revents & POLLIN ){
      uint64_t expirations;
      read(main_keyboard.timer_fd, &expirations, sizeof(expirations));

      for( uint64_t i = 0; i < expirations; i++){
        handle_key_sym(main_keyboard.last_key_sym);
      }
    }

    read_tty();

    pthread_mutex_lock(&draw_mutex);
    can_draw = true;
    pthread_mutex_unlock(&draw_mutex);
  }
}
