#include "pterminal.h"

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/poll.h>
#include <unistd.h>
#include "window.h"
#include "wayland/keyboard.h"
#include "wayland/wayland.h"

#include "tty.h"

#include "draw.h"

#include "wayland/mouse.h"
#include "mouse.h"


static char *shell = "/bin/sh";

struct pollfd pterminal_fds[3];

void *run_pterminal(void *none) {

  fd_set read_file_descriptor;

  int tty_fd;

  bool have_event, drawing;

  struct timespec seltv, *wait_time, now, lastblink, trigger;
  double timeout;

  tty_fd = ttynew(opt_line, shell, opt_io, opt_cmd);

  struct pollfd tty_poll = {tty_fd, POLLIN, 0};
  struct pollfd wayland_poll = {wayland_fd, POLLIN, 0};
  struct pollfd keyboard_timer_poll= {main_keyboard.timer_fd, POLLIN, 0};

  pterminal_fds[0] = tty_poll;
  pterminal_fds[1] = wayland_poll;
  pterminal_fds[2] = keyboard_timer_poll;

  printf("running pterminal\n");

  while (terminal_window.is_running) {

    prepate_to_read_events();

    if (poll(pterminal_fds, 3, -1) == -1) {
      perror("Poll in fds, TTY or Wayland, Keyboard timer");
    }

    if( pterminal_fds[0].revents & POLLIN){
      read_tty();
    }


    handle_events();

  
    draw();

    clean_mouse_buttons();
  }
}
