#include "tty.h"
#include "terminal.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <pty.h>
#include <poll.h>

#include <stdbool.h>
#include <pthread.h>
#include "window.h"

static char *shell = "/bin/sh";

int iofd = 1;
int cmdfd;
pid_t pid;

void new_serial_tty(char **args) {
  char cmd[_POSIX_ARG_MAX], **argument_pointer, *queue, *string;
  size_t arguments_lenght, current_argument_size;

  if ((arguments_lenght = strlen(stty_args)) > sizeof(cmd) - 1)
    die("incorrect stty parameters\n");

  memcpy(cmd, stty_args, arguments_lenght);

  queue = cmd + arguments_lenght;

  current_argument_size = sizeof(cmd) - arguments_lenght;

  for (argument_pointer = args;
       argument_pointer && (string = *argument_pointer); ++argument_pointer) {

    if ((arguments_lenght = strlen(string)) > current_argument_size - 1)
      die("stty parameter length too long\n");

    *queue++ = ' ';

    memcpy(queue, string, arguments_lenght);

    queue += arguments_lenght;

    current_argument_size -= arguments_lenght + 1;
  }

  *queue = '\0';

  if (system(cmd) != 0)
    perror("Couldn't call stty");
}

int ttynew(const char *line, char *cmd, const char *out, char **args) {
  int master, slave;
  
  //for using like serial terminal
  if (out) {
    term.mode |= MODE_PRINT;
    iofd = (!strcmp(out, "-")) ? 1 : open(out, O_WRONLY | O_CREAT, 0666);
    if (iofd < 0) {
      fprintf(stderr, "Error opening %s:%s\n", out, strerror(errno));
    }
  }

  if (line) {
    if ((cmdfd = open(line, O_RDWR)) < 0)
      die("open line '%s' failed: %s\n", line, strerror(errno));
    dup2(cmdfd, 0);
    new_serial_tty(args);
    return cmdfd;
  }
  //end serial terminal

  /* seems to work fine on linux, openbsd and freebsd */
  if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
    die("openpty failed: %s\n", strerror(errno));

  switch (pid = fork()) {
  case -1:
    die("fork failed: %s\n", strerror(errno));
    break;
  case 0:
    close(iofd);
    close(master);
    setsid(); /* create a new process group */
    dup2(slave, 0);
    dup2(slave, 1);
    dup2(slave, 2);
    if (ioctl(slave, TIOCSCTTY, NULL) < 0)
      die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
    if (slave > 2)
      close(slave);
    execute_shell(cmd, args);
    break;
  default:
    close(slave);
    cmdfd = master;
    signal(SIGCHLD, sigchld);
    break;
  }
  return cmdfd;
}

size_t ttyread(void) {
  static char buf[BUFSIZ];
  static int buflen = 0;
  int ret, written;

  /* append read bytes to unprocessed bytes */
  ret = read(cmdfd, buf + buflen, LEN(buf) - buflen);

  switch (ret) {
  case 0:
    exit(0);
  case -1:
    die("couldn't read from shell: %s\n", strerror(errno));
  default:
    buflen += ret;
    written = twrite(buf, buflen, 0);
    buflen -= written;
    /* keep any incomplete UTF-8 byte sequence for the next call */
    if (buflen > 0)
      memmove(buf, buf + written, buflen);
    return ret;
  }
}

void ttywrite(const char *s, size_t n, int may_echo) {
  const char *next;

  if (may_echo && IS_SET(MODE_ECHO))
    twrite(s, n, 1);

  if (!IS_SET(MODE_CRLF)) {
    ttywriteraw(s, n);
    return;
  }

  /* This is similar to how the kernel handles ONLCR for ttys */
  while (n > 0) {
    if (*s == '\r') {
      next = s + 1;
      ttywriteraw("\r\n", 2);
    } else {
      next = memchr(s, '\r', n);
      DEFAULT(next, s + n);
      ttywriteraw(s, next - s);
    }
    n -= next - s;
    s = next;
  }
}

void ttywriteraw(const char *s, size_t n) {
  fd_set wfd, rfd;
  ssize_t r;
  size_t lim = 256;

  /*
   * Remember that we are using a pty, which might be a modem line.
   * Writing too much will clog the line. That's why we are doing this
   * dance.
   * FIXME: Migrate the world to Plan 9.
   */
  while (n > 0) {
    FD_ZERO(&wfd);
    FD_ZERO(&rfd);
    FD_SET(cmdfd, &wfd);
    FD_SET(cmdfd, &rfd);

    /* Check if we can write. */
    if (pselect(cmdfd + 1, &rfd, &wfd, NULL, NULL, NULL) < 0) {
      if (errno == EINTR)
        continue;
      die("select failed: %s\n", strerror(errno));
    }
    if (FD_ISSET(cmdfd, &wfd)) {
      /*
       * Only write the bytes written by ttywrite() or the
       * default of 256. This seems to be a reasonable value
       * for a serial line. Bigger values might clog the I/O.
       */
      if ((r = write(cmdfd, s, (n < lim) ? n : lim)) < 0)
        goto write_error;
      if (r < n) {
        /*
         * We weren't able to write out everything.
         * This means the buffer is getting full
         * again. Empty it.
         */
        if (n < lim)
          lim = ttyread();
        n -= r;
        s += r;
      } else {
        /* All bytes have been written. */
        break;
      }
    }
    if (FD_ISSET(cmdfd, &rfd))
      lim = ttyread();
  }
  return;

write_error:
  die("write error on tty: %s\n", strerror(errno));
}

void ttyresize(int tw, int th) {
  struct winsize w;

  w.ws_row = term.row;
  w.ws_col = term.col;
  w.ws_xpixel = tw;
  w.ws_ypixel = th;
  if (ioctl(cmdfd, TIOCSWINSZ, &w) < 0)
    fprintf(stderr, "Couldn't set window size: %s\n", strerror(errno));
}

void ttyhangup(void) {
  /* Send SIGHUP to shell */
  kill(pid, SIGHUP);
}

void *handle_tty(void *none) {

  fd_set read_file_descriptor;

  int tty_file_descriptor;

  bool have_event, drawing;

  struct timespec seltv, *wait_time, now, lastblink, trigger;
  double timeout;

  tty_file_descriptor = ttynew(opt_line, shell, opt_io, opt_cmd);

  struct pollfd fds[] = {{tty_file_descriptor, POLLIN, 0}};
  while (1) {
    printf("tty loop\n");
    if (poll(fds, 1, -1) == -1) {
    }
    ttyread();

    pthread_mutex_lock(&draw_mutex);
    can_draw = true;
    printf("Can draw tty\n");
    pthread_mutex_unlock(&draw_mutex);
  }
}
