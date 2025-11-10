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

int iofd = 1;
int cmdfd;
pid_t pid;

void stty(char **args) {
  char cmd[_POSIX_ARG_MAX], **p, *q, *s;
  size_t n, siz;

  if ((n = strlen(stty_args)) > sizeof(cmd) - 1)
    die("incorrect stty parameters\n");
  memcpy(cmd, stty_args, n);
  q = cmd + n;
  siz = sizeof(cmd) - n;
  for (p = args; p && (s = *p); ++p) {
    if ((n = strlen(s)) > siz - 1)
      die("stty parameter length too long\n");
    *q++ = ' ';
    memcpy(q, s, n);
    q += n;
    siz -= n + 1;
  }
  *q = '\0';
  if (system(cmd) != 0)
    perror("Couldn't call stty");
}

int ttynew(const char *line, char *cmd, const char *out, char **args) {
  int m, s;

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
    stty(args);
    return cmdfd;
  }

  /* seems to work fine on linux, openbsd and freebsd */
  if (openpty(&m, &s, NULL, NULL, NULL) < 0)
    die("openpty failed: %s\n", strerror(errno));

  switch (pid = fork()) {
  case -1:
    die("fork failed: %s\n", strerror(errno));
    break;
  case 0:
    close(iofd);
    close(m);
    setsid(); /* create a new process group */
    dup2(s, 0);
    dup2(s, 1);
    dup2(s, 2);
    if (ioctl(s, TIOCSCTTY, NULL) < 0)
      die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
    if (s > 2)
      close(s);
    execsh(cmd, args);
    break;
  default:
    close(s);
    cmdfd = m;
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
