/* See LICENSE for license details. */
#define _XOPEN_SOURCE 700
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

#include "window.h"

#include "utf8.h"

#include "selection.h"
#include "tty.h"
#include "ansi_escapes.h"

#include "color.h"
#include <pthread.h>

pthread_mutex_t draw_mutex = PTHREAD_MUTEX_INITIALIZER;


enum cursor_state {
  CURSOR_DEFAULT = 0,
  CURSOR_WRAPNEXT = 1,
  CURSOR_ORIGIN = 2
};

enum charset {
  CS_GRAPHIC0,
  CS_GRAPHIC1,
  CS_UK,
  CS_USA,
  CS_MULTI,
  CS_GER,
  CS_FIN
};



/* Globals */
Term term;

ssize_t xwrite(int fd, const char *s, size_t len) {
  size_t aux = len;
  ssize_t r;

  while (len > 0) {
    r = write(fd, s, len);
    if (r < 0)
      return r;
    len -= r;
    s += r;
  }

  return aux;
}

void *xmalloc(size_t len) {
  void *p;

  if (!(p = malloc(len)))
    die("malloc: %s\n", strerror(errno));

  return p;
}

void *xrealloc(void *p, size_t len) {
  if ((p = realloc(p, len)) == NULL)
    die("realloc: %s\n", strerror(errno));

  return p;
}

char base64dec_getc(const char **src) {
  while (**src && !isprint((unsigned char)**src))
    (*src)++;
  return **src ? *((*src)++) : '='; /* emulate padding if string ends */
}

char *base64dec(const char *src) {
  size_t in_len = strlen(src);
  char *result, *dst;
  static const char base64_digits[256] = {
      [43] = 62, 0,  0,  0,  63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,
      0,         0,  -1, 0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10,        11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
      0,         0,  0,  0,  0,  0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
      36,        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

  if (in_len % 4)
    in_len += 4 - (in_len % 4);
  result = dst = xmalloc(in_len / 4 * 3 + 1);
  while (*src) {
    int a = base64_digits[(unsigned char)base64dec_getc(&src)];
    int b = base64_digits[(unsigned char)base64dec_getc(&src)];
    int c = base64_digits[(unsigned char)base64dec_getc(&src)];
    int d = base64_digits[(unsigned char)base64dec_getc(&src)];

    /* invalid input. 'a' can be -1, e.g. if src is "\n" (c-str) */
    if (a == -1 || b == -1)
      break;

    *dst++ = (a << 2) | ((b & 0x30) >> 4);
    if (c == -1)
      break;
    *dst++ = ((b & 0x0f) << 4) | ((c & 0x3c) >> 2);
    if (d == -1)
      break;
    *dst++ = ((c & 0x03) << 6) | d;
  }
  *dst = '\0';
  return result;
}

int tlinelen(int y) {
  int i = term.col;
  Line line = TLINE(y);

  if (line[i - 1].mode & ATTR_WRAP)
    return i;

  while (i > 0 && line[i - 1].u == ' ')
    --i;

  return i;
}

void die(const char *errstr, ...) {
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  exit(1);
}

void execute_shell(char *cmd, char **args) {
  char *sh, *prog, *arg;
  const struct passwd *pw;

  errno = 0;
  if ((pw = getpwuid(getuid())) == NULL) {
    if (errno)
      die("getpwuid: %s\n", strerror(errno));
    else
      die("who are you?\n");
  }

  if ((sh = getenv("SHELL")) == NULL)
    sh = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

  if (args) {
    prog = args[0];
    arg = NULL;
  } else if (scroll) {
    prog = scroll;
    arg = utmp ? utmp : sh;
  } else if (utmp) {
    prog = utmp;
    arg = NULL;
  } else {
    prog = sh;
    arg = NULL;
  }
  DEFAULT(args, ((char *[]){prog, arg, NULL}));

  unsetenv("COLUMNS");
  unsetenv("LINES");
  unsetenv("TERMCAP");
  setenv("LOGNAME", pw->pw_name, 1);
  setenv("USER", pw->pw_name, 1);
  setenv("SHELL", sh, 1);
  setenv("HOME", pw->pw_dir, 1);
  setenv("TERM", termname, 1);

  signal(SIGCHLD, SIG_DFL);
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  signal(SIGALRM, SIG_DFL);

  execvp(prog, args);
  printf("Exit from shell\n");
  _exit(1);
}

void sigchld(int a) {
  int stat;
  pid_t p;

  if ((p = waitpid(pid, &stat, WNOHANG)) < 0)
    die("waiting for pid %hd failed: %s\n", pid, strerror(errno));

  if (pid != p)
    return;

  if (WIFEXITED(stat) && WEXITSTATUS(stat))
    die("child exited with status %d\n", WEXITSTATUS(stat));
  else if (WIFSIGNALED(stat))
    die("child terminated due to signal %d\n", WTERMSIG(stat));
  _exit(0);
}



int tattrset(int attr) {
  int i, j;
  int y = TLINEOFFSET(0);

  for (i = 0; i < term.row - 1; i++) {
    Line line = TSCREEN.buffer[y];
    for (j = 0; j < term.col - 1; j++) {
      if (line[j].mode & attr)
        return 1;
    }
    y = (y + 1) % TSCREEN.size;
  }

  return 0;
}

void tsetdirt(int top, int bot) {
  int i;

  LIMIT(top, 0, term.row - 1);
  LIMIT(bot, 0, term.row - 1);

  for (i = top; i <= bot; i++)
    term.dirty[i] = 1;
}

void tsetdirtattr(int attr) {
  int i, j;
  int y = TLINEOFFSET(0);

  for (i = 0; i < term.row - 1; i++) {
    Line line = TSCREEN.buffer[y];
    for (j = 0; j < term.col - 1; j++) {
      if (line[j].mode & attr) {
        tsetdirt(i, i);
        break;
      }
    }
    y = (y + 1) % TSCREEN.size;
  }
}

void tfulldirt(void) { tsetdirt(0, term.row - 1); }

void tcursor(int mode) {
  if (mode == CURSOR_SAVE) {
    TSCREEN.sc = term.cursor;
  } else if (mode == CURSOR_LOAD) {
    term.cursor = TSCREEN.sc;
    tmoveto(term.cursor.x, term.cursor.y);
  }
}

void treset(void) {
  int i, j;
  PGlyph g = (PGlyph){.fg = defaultfg, .bg = defaultbg};

  memset(term.tabs, 0, term.col * sizeof(*term.tabs));
  for (i = tabspaces; i < term.col; i += tabspaces)
    term.tabs[i] = 1;
  term.top = 0;
  term.bot = term.row - 1;
  term.mode = MODE_WRAP | MODE_UTF8;
  memset(term.trantbl, CS_USA, sizeof(term.trantbl));
  term.charset = 0;

  for (i = 0; i < 2; i++) {
    term.screen[i].sc = (TCursor){{.fg = defaultfg, .bg = defaultbg}};
    term.screen[i].cur = 0;
    term.screen[i].off = 0;
    for (j = 0; j < term.row; ++j) {
      if (term.col != term.linelen)
        term.screen[i].buffer[j] =
            xrealloc(term.screen[i].buffer[j], term.col * sizeof(PGlyph));
      clearline(term.screen[i].buffer[j], g, 0, term.col);
    }
    for (j = term.row; j < term.screen[i].size; ++j) {
      free(term.screen[i].buffer[j]);
      term.screen[i].buffer[j] = NULL;
    }
  }
  tcursor(CURSOR_LOAD);
  term.linelen = term.col;
  tfulldirt();
}

void tnew(int col, int row) {
  int i;
  term = (Term){};
  term.screen[0].buffer = xmalloc(HISTSIZE * sizeof(Line));
  term.screen[0].size = HISTSIZE;
  term.screen[1].buffer = NULL;
  for (i = 0; i < HISTSIZE; ++i)
    term.screen[0].buffer[i] = NULL;

  tresize(col, row);
  treset();
}

void tswapscreen(void) {
  term.mode ^= MODE_ALTSCREEN;
  tfulldirt();
}

void kscrollup(const Arg *a) {
  float n = a->f;

  if (IS_SET(MODE_ALTSCREEN))
    return;

  if (n < 0)
    n = MAX((-n) * term.row, 1);
  if (n > TSCREEN.size - term.row - TSCREEN.off)
    n = TSCREEN.size - term.row - TSCREEN.off;
  while (!TLINE((int)-n))
    --n;
  TSCREEN.off += n;
  selscroll(0, n);
  tfulldirt();
}

void kscrolldown(const Arg *a) {

  float n = a->f;

  if (IS_SET(MODE_ALTSCREEN))
    return;

  if (n < 0)
    n = MAX((-n) * term.row, 1);
  if (n > TSCREEN.off)
    n = TSCREEN.off;
  TSCREEN.off -= n;
  selscroll(0, -n);
  tfulldirt();
}

void tscrolldown(int orig, int n) {
  int i;
  Line temp;

  LIMIT(n, 0, term.bot - orig + 1);

  /* Ensure that lines are allocated */
  for (i = -n; i < 0; i++) {
    TLINE(i) = ensureline(TLINE(i));
  }

  /* Shift non-scrolling areas in ring buffer */
  for (i = term.bot + 1; i < term.row; i++) {
    temp = TLINE(i);
    TLINE(i) = TLINE(i - n);
    TLINE(i - n) = temp;
  }
  for (i = 0; i < orig; i++) {
    temp = TLINE(i);
    TLINE(i) = TLINE(i - n);
    TLINE(i - n) = temp;
  }

  /* Scroll buffer */
  TSCREEN.cur = (TSCREEN.cur + TSCREEN.size - n) % TSCREEN.size;
  /* Clear lines that have entered the view */
  tclearregion(0, orig, term.linelen - 1, orig + n - 1);
  /* Redraw portion of the screen that has scrolled */
  tsetdirt(orig + n - 1, term.bot);
  selscroll(orig, n);
}

void tscrollup(int orig, int n) {
  int i;
  Line temp;

  LIMIT(n, 0, term.bot - orig + 1);

  /* Ensure that lines are allocated */
  for (i = term.row; i < term.row + n; i++) {
    TLINE(i) = ensureline(TLINE(i));
  }

  /* Shift non-scrolling areas in ring buffer */
  for (i = orig - 1; i >= 0; i--) {
    temp = TLINE(i);
    TLINE(i) = TLINE(i + n);
    TLINE(i + n) = temp;
  }
  for (i = term.row - 1; i > term.bot; i--) {
    temp = TLINE(i);
    TLINE(i) = TLINE(i + n);
    TLINE(i + n) = temp;
  }

  /* Scroll buffer */
  TSCREEN.cur = (TSCREEN.cur + n) % TSCREEN.size;
  /* Clear lines that have entered the view */
  tclearregion(0, term.bot - n + 1, term.linelen - 1, term.bot);
  /* Redraw portion of the screen that has scrolled */
  tsetdirt(orig, term.bot - n + 1);
  selscroll(orig, -n);
}

void selscroll(int orig, int n) {
  if (selection.original_beginning.x == -1 ||
      selection.alt != IS_SET(MODE_ALTSCREEN))
    return;

  if (BETWEEN(selection.beginning_normalized.y, orig, term.bot) !=
      BETWEEN(selection.end_normalized.y, orig, term.bot)) {
    selclear();
  } else if (BETWEEN(selection.beginning_normalized.y, orig, term.bot)) {
    selection.original_beginning.y += n;
    selection.original_end.y += n;
    if (selection.original_beginning.y < term.top ||
        selection.original_beginning.y > term.bot ||
        selection.original_end.y < term.top ||
        selection.original_end.y > term.bot) {
      selclear();
    } else {
      selnormalize();
    }
  }
}

void tnewline(int first_col) {
  int y = term.cursor.y;

  if (y == term.bot) {
    tscrollup(term.top, 1);
  } else {
    y++;
  }
  tmoveto(first_col ? 0 : term.cursor.x, y);
}


/* for absolute user moves, when decom is set */
void tmoveato(int x, int y) {
  tmoveto(x, y + ((term.cursor.state & CURSOR_ORIGIN) ? term.top : 0));
}

void tmoveto(int x, int y) {
  int miny, maxy;

  if (term.cursor.state & CURSOR_ORIGIN) {
    miny = term.top;
    maxy = term.bot;
  } else {
    miny = 0;
    maxy = term.row - 1;
  }
  term.cursor.state &= ~CURSOR_WRAPNEXT;
  term.cursor.x = LIMIT(x, 0, term.col - 1);
  term.cursor.y = LIMIT(y, miny, maxy);
}

void tsetchar(Rune u, PGlyph *attr, int x, int y) {
  attr->utf8_value = (int)u;
  static const char *vt100_0[62] = {
      /* 0x41 - 0x7e */
      "↑", "↓", "→", "←", "█", "▚", "☃",      /* A - G */
      0,   0,   0,   0,   0,   0,   0,   0,   /* H - O */
      0,   0,   0,   0,   0,   0,   0,   0,   /* P - W */
      0,   0,   0,   0,   0,   0,   0,   " ", /* X - _ */
      "◆", "▒", "␉", "␌", "␍", "␊", "°", "±", /* ` - g */
      "␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺", /* h - o */
      "⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬", /* p - w */
      "│", "≤", "≥", "π", "≠", "£", "·",      /* x - ~ */
  };
  Line line = TLINE(y);
  // line[x].utf8_value = u;

  /*
   * The table is proudly stolen from rxvt.
   */
  if (term.trantbl[term.charset] == CS_GRAPHIC0 && BETWEEN(u, 0x41, 0x7e) &&
      vt100_0[u - 0x41]) {
    utf8decode(vt100_0[u - 0x41], &u, UTF_SIZ);
  }

  if (line[x].mode & ATTR_WIDE) {
    if (x + 1 < term.col) {
      line[x + 1].u = ' ';
      line[x + 1].mode &= ~ATTR_WDUMMY;
    }
  } else if (line[x].mode & ATTR_WDUMMY) {
    line[x - 1].u = ' ';
    line[x - 1].mode &= ~ATTR_WIDE;
  }

  term.dirty[y] = 1;
  line[x] = *attr;
  line[x].u = u;
}

void tclearregion(int x1, int y1, int x2, int y2) {
  int x, y, L, S, temp;
  PGlyph *gp;

  if (x1 > x2)
    temp = x1, x1 = x2, x2 = temp;
  if (y1 > y2)
    temp = y1, y1 = y2, y2 = temp;

  LIMIT(x1, 0, term.linelen - 1);
  LIMIT(x2, 0, term.linelen - 1);
  LIMIT(y1, 0, term.row - 1);
  LIMIT(y2, 0, term.row - 1);

  L = TLINEOFFSET(y1);
  for (y = y1; y <= y2; y++) {
    term.dirty[y] = 1;
    for (x = x1; x <= x2; x++) {
      gp = &TSCREEN.buffer[L][x];
      if (selected(x, y))
        selclear();
      gp->fg = term.cursor.attr.fg;
      gp->bg = term.cursor.attr.bg;
      gp->mode = 0;
      gp->u = ' ';
    }
    L = (L + 1) % TSCREEN.size;
  }
}

void tdeletechar(int n) {
  int dst, src, size;
  PGlyph *line;

  LIMIT(n, 0, term.col - term.cursor.x);

  dst = term.cursor.x;
  src = term.cursor.x + n;
  size = term.col - src;
  line = TLINE(term.cursor.y);

  memmove(&line[dst], &line[src], size * sizeof(PGlyph));
  tclearregion(term.col - n, term.cursor.y, term.col - 1, term.cursor.y);
}

void tinsertblank(int n) {
  int dst, src, size;
  PGlyph *line;

  LIMIT(n, 0, term.col - term.cursor.x);

  dst = term.cursor.x + n;
  src = term.cursor.x;
  size = term.col - dst;
  line = TLINE(term.cursor.y);

  memmove(&line[dst], &line[src], size * sizeof(PGlyph));
  tclearregion(src, term.cursor.y, dst - 1, term.cursor.y);
}

void tinsertblankline(int n) {
  if (BETWEEN(term.cursor.y, term.top, term.bot))
    tscrolldown(term.cursor.y, n);
}

void tdeleteline(int n) {
  if (BETWEEN(term.cursor.y, term.top, term.bot))
    tscrollup(term.cursor.y, n);
}

int32_t tdefcolor(const int *attr, int *npar, int l) {
  int32_t idx = -1;
  uint r, g, b;

  switch (attr[*npar + 1]) {
  case 2: /* direct color in RGB space */
    if (*npar + 4 >= l) {
      fprintf(stderr, "erresc(38): Incorrect number of parameters (%d)\n",
              *npar);
      break;
    }
    r = attr[*npar + 2];
    g = attr[*npar + 3];
    b = attr[*npar + 4];
    *npar += 4;
    if (!BETWEEN(r, 0, 255) || !BETWEEN(g, 0, 255) || !BETWEEN(b, 0, 255))
      fprintf(stderr, "erresc: bad rgb color (%u,%u,%u)\n", r, g, b);
    else
      idx = TRUECOLOR(r, g, b);
    break;
  case 5: /* indexed color */
    if (*npar + 2 >= l) {
      fprintf(stderr, "erresc(38): Incorrect number of parameters (%d)\n",
              *npar);
      break;
    }
    *npar += 2;
    if (!BETWEEN(attr[*npar], 0, 255))
      fprintf(stderr, "erresc: bad fgcolor %d\n", attr[*npar]);
    else
      idx = attr[*npar];
    break;
  case 0: /* implemented defined (only foreground) */
  case 1: /* transparent */
  case 3: /* direct color in CMY space */
  case 4: /* direct color in CMYK space */
  default:
    fprintf(stderr, "erresc(38): gfx attr %d unknown\n", attr[*npar]);
    break;
  }

  return idx;
}

void tsetattr(const int *attr, int l) {
  int i;
  int32_t idx;

  for (i = 0; i < l; i++) {
    switch (attr[i]) {
    case 0:
      term.cursor.attr.mode &=
          ~(ATTR_BOLD | ATTR_FAINT | ATTR_ITALIC | ATTR_UNDERLINE | ATTR_BLINK |
            ATTR_REVERSE | ATTR_INVISIBLE | ATTR_STRUCK);
      term.cursor.attr.fg = defaultfg;
      term.cursor.attr.bg = defaultbg;
      break;
    case 1:
      term.cursor.attr.mode |= ATTR_BOLD;
      break;
    case 2:
      term.cursor.attr.mode |= ATTR_FAINT;
      break;
    case 3:
      term.cursor.attr.mode |= ATTR_ITALIC;
      break;
    case 4:
      term.cursor.attr.mode |= ATTR_UNDERLINE;
      break;
    case 5: /* slow blink */
            /* FALLTHROUGH */
    case 6: /* rapid blink */
      term.cursor.attr.mode |= ATTR_BLINK;
      break;
    case 7:
      term.cursor.attr.mode |= ATTR_REVERSE;
      break;
    case 8:
      term.cursor.attr.mode |= ATTR_INVISIBLE;
      break;
    case 9:
      term.cursor.attr.mode |= ATTR_STRUCK;
      break;
    case 22:
      term.cursor.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT);
      break;
    case 23:
      term.cursor.attr.mode &= ~ATTR_ITALIC;
      break;
    case 24:
      term.cursor.attr.mode &= ~ATTR_UNDERLINE;
      break;
    case 25:
      term.cursor.attr.mode &= ~ATTR_BLINK;
      break;
    case 27:
      term.cursor.attr.mode &= ~ATTR_REVERSE;
      break;
    case 28:
      term.cursor.attr.mode &= ~ATTR_INVISIBLE;
      break;
    case 29:
      term.cursor.attr.mode &= ~ATTR_STRUCK;
      break;
    case 38:
      if ((idx = tdefcolor(attr, &i, l)) >= 0)
        term.cursor.attr.fg = idx;
      break;
    case 39:
      term.cursor.attr.fg = defaultfg;
      break;
    case 48:
      if ((idx = tdefcolor(attr, &i, l)) >= 0)
        term.cursor.attr.bg = idx;
      break;
    case 49:
      term.cursor.attr.bg = defaultbg;
      break;
    default:
      if (BETWEEN(attr[i], 30, 37)) {
        term.cursor.attr.fg = attr[i] - 30;
      } else if (BETWEEN(attr[i], 40, 47)) {
        term.cursor.attr.bg = attr[i] - 40;
      } else if (BETWEEN(attr[i], 90, 97)) {
        term.cursor.attr.fg = attr[i] - 90 + 8;
      } else if (BETWEEN(attr[i], 100, 107)) {
        term.cursor.attr.bg = attr[i] - 100 + 8;
      } else {
        fprintf(stderr, "erresc(default): gfx attr %d unknown\n", attr[i]);
        csidump();
      }
      break;
    }
  }
}

void tsetscroll(int t, int b) {
  int temp;

  LIMIT(t, 0, term.row - 1);
  LIMIT(b, 0, term.row - 1);
  if (t > b) {
    temp = t;
    t = b;
    b = temp;
  }
  term.top = t;
  term.bot = b;
}

void tsetmode(int priv, int set, const int *args, int narg) {
  int alt;
  const int *lim;

  for (lim = args + narg; args < lim; ++args) {
    if (priv) {
      switch (*args) {
      case 1: /* DECCKM -- Cursor key */
        xsetmode(set, MODE_APPCURSOR);
        break;
      case 5: /* DECSCNM -- Reverse video */
        xsetmode(set, MODE_REVERSE);
        break;
      case 6: /* DECOM -- Origin */
        MODBIT(term.cursor.state, set, CURSOR_ORIGIN);
        tmoveato(0, 0);
        break;
      case 7: /* DECAWM -- Auto wrap */
        MODBIT(term.mode, set, MODE_WRAP);
        break;
      case 0:  /* Error (IGNORED) */
      case 2:  /* DECANM -- ANSI/VT52 (IGNORED) */
      case 3:  /* DECCOLM -- Column  (IGNORED) */
      case 4:  /* DECSCLM -- Scroll (IGNORED) */
      case 8:  /* DECARM -- Auto repeat (IGNORED) */
      case 18: /* DECPFF -- Printer feed (IGNORED) */
      case 19: /* DECPEX -- Printer extent (IGNORED) */
      case 42: /* DECNRCM -- National characters (IGNORED) */
      case 12: /* att610 -- Start blinking cursor (IGNORED) */
        break;
      case 25: /* DECTCEM -- Text Cursor Enable Mode */
        xsetmode(!set, MODE_HIDE);
        break;
      case 9: /* X10 mouse compatibility mode */
        xsetpointermotion(0);
        xsetmode(0, MODE_MOUSE);
        xsetmode(set, MODE_MOUSEX10);
        break;
      case 1000: /* 1000: report button press */
        xsetpointermotion(0);
        xsetmode(0, MODE_MOUSE);
        xsetmode(set, MODE_MOUSEBTN);
        break;
      case 1002: /* 1002: report motion on button press */
        xsetpointermotion(0);
        xsetmode(0, MODE_MOUSE);
        xsetmode(set, MODE_MOUSEMOTION);
        break;
      case 1003: /* 1003: enable all mouse motions */
        xsetpointermotion(set);
        xsetmode(0, MODE_MOUSE);
        xsetmode(set, MODE_MOUSEMANY);
        break;
      case 1004: /* 1004: send focus events to tty */
        xsetmode(set, MODE_FOCUS);
        break;
      case 1006: /* 1006: extended reporting mode */
        xsetmode(set, MODE_MOUSESGR);
        break;
      case 1034:
        xsetmode(set, MODE_8BIT);
        break;
      case 1049: /* swap screen & set/restore cursor as xterm */
        if (!allowaltscreen)
          break;
        tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
        /* FALLTHROUGH */
      case 47: /* swap screen */
      case 1047:
        if (!allowaltscreen)
          break;
        alt = IS_SET(MODE_ALTSCREEN);
        if (alt) {
          tclearregion(0, 0, term.col - 1, term.row - 1);
        }
        if (set ^ alt) /* set is always 1 or 0 */
          tswapscreen();
        if (*args != 1049)
          break;
        /* FALLTHROUGH */
      case 1048:
        tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
        break;
      case 2004: /* 2004: bracketed paste mode */
        xsetmode(set, MODE_BRCKTPASTE);
        break;
      /* Not implemented mouse modes. See comments there. */
      case 1001: /* mouse highlight mode; can hang the
                    terminal by design when implemented. */
      case 1005: /* UTF-8 mouse mode; will confuse
                    applications not supporting UTF-8
                    and luit. */
      case 1015: /* urxvt mangled mouse mode; incompatible
                    and can be mistaken for other control
                    codes. */
        break;
      default:
        fprintf(stderr, "erresc: unknown private set/reset mode %d\n", *args);
        break;
      }
    } else {
      switch (*args) {
      case 0: /* Error (IGNORED) */
        break;
      case 2:
        xsetmode(set, MODE_KBDLOCK);
        break;
      case 4: /* IRM -- Insertion-replacement */
        MODBIT(term.mode, set, MODE_INSERT);
        break;
      case 12: /* SRM -- Send/Receive */
        MODBIT(term.mode, !set, MODE_ECHO);
        break;
      case 20: /* LNM -- Linefeed/new line */
        MODBIT(term.mode, set, MODE_CRLF);
        break;
      default:
        fprintf(stderr, "erresc: unknown set/reset mode %d\n", *args);
        break;
      }
    }
  }
}


void sendbreak(const Arg *arg) {
  if (tcsendbreak(cmdfd, 0))
    perror("Error sending break");
}

void tprinter(char *s, size_t len) {
  if (iofd != -1 && xwrite(iofd, s, len) < 0) {
    perror("Error writing to output file");
    close(iofd);
    iofd = -1;
  }
}

void toggleprinter(const Arg *arg) { term.mode ^= MODE_PRINT; }

void printscreen(const Arg *arg) { tdump(); }

void printsel(const Arg *arg) { tdumpsel(); }

void tdumpsel(void) {
  char *ptr;

  if ((ptr = getsel())) {
    tprinter(ptr, strlen(ptr));
    free(ptr);
  }
}

void tdumpline(int n) {
  char buf[UTF_SIZ];
  const PGlyph *bp, *end;

  bp = &TLINE(n)[0];
  end = &bp[MIN(tlinelen(n), term.col) - 1];
  if (bp != end || bp->u != ' ') {
    for (; bp <= end; ++bp)
      tprinter(buf, utf8encode(bp->u, buf));
  }
  tprinter("\n", 1);
}

void tdump(void) {
  int i;

  for (i = 0; i < term.row; ++i)
    tdumpline(i);
}

void tputtab(int n) {
  uint x = term.cursor.x;

  if (n > 0) {
    while (x < term.col && n--)
      for (++x; x < term.col && !term.tabs[x]; ++x)
        /* nothing */;
  } else if (n < 0) {
    while (x > 0 && n++)
      for (--x; x > 0 && !term.tabs[x]; --x)
        /* nothing */;
  }
  term.cursor.x = LIMIT(x, 0, term.col - 1);
}

void tdefutf8(char ascii) {
  if (ascii == 'G')
    term.mode |= MODE_UTF8;
  else if (ascii == '@')
    term.mode &= ~MODE_UTF8;
}

void tdeftran(char ascii) {
  static char cs[] = "0B";
  static int vcs[] = {CS_GRAPHIC0, CS_USA};
  char *p;

  if ((p = strchr(cs, ascii)) == NULL) {
    fprintf(stderr, "esc unhandled charset: ESC ( %c\n", ascii);
  } else {
    term.trantbl[term.icharset] = vcs[p - cs];
  }
}

void tdectest(char c) {
  int x, y;

  if (c == '8') { /* DEC screen alignment test. */
    for (x = 0; x < term.col; ++x) {
      for (y = 0; y < term.row; ++y)
        tsetchar('E', &term.cursor.attr, x, y);
    }
  }
}

void tstrsequence(uchar c) {
  switch (c) {
  case 0x90: /* DCS -- Device Control String */
    c = 'P';
    break;
  case 0x9f: /* APC -- Application Program Command */
    c = '_';
    break;
  case 0x9e: /* PM -- Privacy Message */
    c = '^';
    break;
  case 0x9d: /* OSC -- Operating System Command */
    c = ']';
    break;
  }
  strreset();
  strescseq.type = c;
  term.esc |= ESC_STR;
}

void tcontrolcode(uchar ascii) {
  switch (ascii) {
  case '\t': /* HT */
    tputtab(1);
    return;
  case '\b': /* BS */
    tmoveto(term.cursor.x - 1, term.cursor.y);
    return;
  case '\r': /* CR */
    tmoveto(0, term.cursor.y);
    return;
  case '\f': /* LF */
  case '\v': /* VT */
  case '\n': /* LF */
    /* go to first col if the mode is set */
    tnewline(IS_SET(MODE_CRLF));
    return;
  case '\a': /* BEL */
    if (term.esc & ESC_STR_END) {
      /* backwards compatibility to xterm */
      strhandle();
    } else {
      xbell();
    }
    break;
  case '\033': /* ESC */
    csireset();
    term.esc &= ~(ESC_CSI | ESC_ALTCHARSET | ESC_TEST);
    term.esc |= ESC_START;
    return;
  case '\016': /* SO (LS1 -- Locking shift 1) */
  case '\017': /* SI (LS0 -- Locking shift 0) */
    term.charset = 1 - (ascii - '\016');
    return;
  case '\032': /* SUB */
    tsetchar('?', &term.cursor.attr, term.cursor.x, term.cursor.y);
    /* FALLTHROUGH */
  case '\030': /* CAN */
    csireset();
    break;
  case '\005': /* ENQ (IGNORED) */
  case '\000': /* NUL (IGNORED) */
  case '\021': /* XON (IGNORED) */
  case '\023': /* XOFF (IGNORED) */
  case 0177:   /* DEL (IGNORED) */
    return;
  case 0x80: /* TODO: PAD */
  case 0x81: /* TODO: HOP */
  case 0x82: /* TODO: BPH */
  case 0x83: /* TODO: NBH */
  case 0x84: /* TODO: IND */
    break;
  case 0x85:     /* NEL -- Next line */
    tnewline(1); /* always go to first col */
    break;
  case 0x86: /* TODO: SSA */
  case 0x87: /* TODO: ESA */
    break;
  case 0x88: /* HTS -- Horizontal tab stop */
    term.tabs[term.cursor.x] = 1;
    break;
  case 0x89: /* TODO: HTJ */
  case 0x8a: /* TODO: VTS */
  case 0x8b: /* TODO: PLD */
  case 0x8c: /* TODO: PLU */
  case 0x8d: /* TODO: RI */
  case 0x8e: /* TODO: SS2 */
  case 0x8f: /* TODO: SS3 */
  case 0x91: /* TODO: PU1 */
  case 0x92: /* TODO: PU2 */
  case 0x93: /* TODO: STS */
  case 0x94: /* TODO: CCH */
  case 0x95: /* TODO: MW */
  case 0x96: /* TODO: SPA */
  case 0x97: /* TODO: EPA */
  case 0x98: /* TODO: SOS */
  case 0x99: /* TODO: SGCI */
    break;
  case 0x9a: /* DECID -- Identify Terminal */
    ttywrite(vtiden, strlen(vtiden), 0);
    break;
  case 0x9b: /* TODO: CSI */
  case 0x9c: /* TODO: ST */
    break;
  case 0x90: /* DCS -- Device Control String */
  case 0x9d: /* OSC -- Operating System Command */
  case 0x9e: /* PM -- Privacy Message */
  case 0x9f: /* APC -- Application Program Command */
    tstrsequence(ascii);
    return;
  }
  /* only CAN, SUB, \a and C1 chars interrupt a sequence */
  term.esc &= ~(ESC_STR_END | ESC_STR);
}

/*
 * returns 1 when the sequence is finished and it hasn't to read
 * more characters for this sequence, otherwise 0
 */
int eschandle(uchar ascii) {
  switch (ascii) {
  case '[':
    term.esc |= ESC_CSI;
    return 0;
  case '#':
    term.esc |= ESC_TEST;
    return 0;
  case '%':
    term.esc |= ESC_UTF8;
    return 0;
  case 'P': /* DCS -- Device Control String */
  case '_': /* APC -- Application Program Command */
  case '^': /* PM -- Privacy Message */
  case ']': /* OSC -- Operating System Command */
  case 'k': /* old title set compatibility */
    tstrsequence(ascii);
    return 0;
  case 'n': /* LS2 -- Locking shift 2 */
  case 'o': /* LS3 -- Locking shift 3 */
    term.charset = 2 + (ascii - 'n');
    break;
  case '(': /* GZD4 -- set primary charset G0 */
  case ')': /* G1D4 -- set secondary charset G1 */
  case '*': /* G2D4 -- set tertiary charset G2 */
  case '+': /* G3D4 -- set quaternary charset G3 */
    term.icharset = ascii - '(';
    term.esc |= ESC_ALTCHARSET;
    return 0;
  case 'D': /* IND -- Linefeed */
    if (term.cursor.y == term.bot) {
      tscrollup(term.top, 1);
    } else {
      tmoveto(term.cursor.x, term.cursor.y + 1);
    }
    break;
  case 'E':      /* NEL -- Next line */
    tnewline(1); /* always go to first col */
    break;
  case 'H': /* HTS -- Horizontal tab stop */
    term.tabs[term.cursor.x] = 1;
    break;
  case 'M': /* RI -- Reverse index */
    if (term.cursor.y == term.top) {
      tscrolldown(term.top, 1);
    } else {
      tmoveto(term.cursor.x, term.cursor.y - 1);
    }
    break;
  case 'Z': /* DECID -- Identify Terminal */
    ttywrite(vtiden, strlen(vtiden), 0);
    break;
  case 'c': /* RIS -- Reset to initial state */
    treset();
    resettitle();
    xloadcols();
    xsetmode(0, MODE_HIDE);
    break;
  case '=': /* DECPAM -- Application keypad */
    xsetmode(1, MODE_APPKEYPAD);
    break;
  case '>': /* DECPNM -- Normal keypad */
    xsetmode(0, MODE_APPKEYPAD);
    break;
  case '7': /* DECSC -- Save Cursor */
    tcursor(CURSOR_SAVE);
    break;
  case '8': /* DECRC -- Restore Cursor */
    tcursor(CURSOR_LOAD);
    break;
  case '\\': /* ST -- String Terminator */
    if (term.esc & ESC_STR_END)
      strhandle();
    break;
  default:
    fprintf(stderr, "erresc: unknown sequence ESC 0x%02X '%c'\n", (uchar)ascii,
            isprint(ascii) ? ascii : '.');
    break;
  }
  return 1;
}

void tputc(Rune u) {
  char c[UTF_SIZ];
  int control;
  int width, len;
  PGlyph *glyph_pointer;

  int original_rune = u;

  control = ISCONTROL(u);
  if (u < 127 || !IS_SET(MODE_UTF8)) {
    c[0] = u;
    width = len = 1;
  } else {
    len = utf8encode(u, c);
    if (!control && (width = wcwidth(u)) == -1)
      width = 1;
  }

  if (IS_SET(MODE_PRINT))
    tprinter(c, len);

  /*
   * STR sequence must be checked before anything else
   * because it uses all following characters until it
   * receives a ESC, a SUB, a ST or any other C1 control
   * character.
   */
  if (term.esc & ESC_STR) {
    if (u == '\a' || u == 030 || u == 032 || u == 033 || ISCONTROLC1(u)) {
      term.esc &= ~(ESC_START | ESC_STR);
      term.esc |= ESC_STR_END;
      goto check_control_code;
    }

    if (strescseq.len + len >= strescseq.siz) {
      /*
       * Here is a bug in terminals. If the user never sends
       * some code to stop the str or esc command, then st
       * will stop responding. But this is better than
       * silently failing with unknown characters. At least
       * then users will report back.
       *
       * In the case users ever get fixed, here is the code:
       */
      /*
       * term.esc = 0;
       * strhandle();
       */
      if (strescseq.siz > (SIZE_MAX - UTF_SIZ) / 2)
        return;
      strescseq.siz *= 2;
      strescseq.buf = xrealloc(strescseq.buf, strescseq.siz);
    }

    memmove(&strescseq.buf[strescseq.len], c, len);
    strescseq.len += len;
    return;
  }

check_control_code:
  /*
   * Actions of control codes must be performed as soon they arrive
   * because they can be embedded inside a control sequence, and
   * they must not cause conflicts with sequences.
   */
  if (control) {
    /* in UTF-8 mode ignore handling C1 control characters */
    if (IS_SET(MODE_UTF8) && ISCONTROLC1(u))
      return;
    tcontrolcode(u);
    /*
     * control codes are not shown ever
     */
    if (!term.esc)
      term.lastc = 0;
    return;
  } else if (term.esc & ESC_START) {
    if (term.esc & ESC_CSI) {
      csiescseq.buf[csiescseq.len++] = u;
      if (BETWEEN(u, 0x40, 0x7E) ||
          csiescseq.len >= sizeof(csiescseq.buf) - 1) {
        term.esc = 0;
        csiparse();
        csihandle();
      }
      return;
    } else if (term.esc & ESC_UTF8) {
      tdefutf8(u);
    } else if (term.esc & ESC_ALTCHARSET) {
      tdeftran(u);
    } else if (term.esc & ESC_TEST) {
      tdectest(u);
    } else {
      if (!eschandle(u))
        return;
      /* sequence already finished */
    }
    term.esc = 0;
    /*
     * All characters which form part of a sequence are not
     * printed
     */
    return;
  }

  if (selected(term.cursor.x, term.cursor.y))
    selclear();

  glyph_pointer = &TLINE(term.cursor.y)[term.cursor.x];
  if (IS_SET(MODE_WRAP) && (term.cursor.state & CURSOR_WRAPNEXT)) {
    glyph_pointer->mode |= ATTR_WRAP;
    tnewline(1);
    glyph_pointer = &TLINE(term.cursor.y)[term.cursor.x];
  }

  if (IS_SET(MODE_INSERT) && term.cursor.x + width < term.col) {
    memmove(glyph_pointer + width, glyph_pointer,
            (term.col - term.cursor.x - width) * sizeof(PGlyph));
    glyph_pointer->mode &= ~ATTR_WIDE;
  }

  if (term.cursor.x + width > term.col) {
    if (IS_SET(MODE_WRAP))
      tnewline(1);
    else
      tmoveto(term.col - width, term.cursor.y);
    glyph_pointer = &TLINE(term.cursor.y)[term.cursor.x];
  }

  tsetchar(u, &term.cursor.attr, term.cursor.x, term.cursor.y);
  term.lastc = u;

  if (width == 2) {
    glyph_pointer->mode |= ATTR_WIDE;
    if (term.cursor.x + 1 < term.col) {
      if (glyph_pointer[1].mode == ATTR_WIDE && term.cursor.x + 2 < term.col) {
        glyph_pointer[2].u = ' ';
        glyph_pointer[2].mode &= ~ATTR_WDUMMY;
      }
      glyph_pointer[1].u = '\0';
      glyph_pointer[1].mode = ATTR_WDUMMY;
    }
  }
  if (term.cursor.x + width < term.col) {
    tmoveto(term.cursor.x + width, term.cursor.y);
  } else {
    term.cursor.state |= CURSOR_WRAPNEXT;
  }
}

int twrite(const char *buf, int buflen, int show_ctrl) {
  int charsize;
  Rune u;
  int n;

  if (TSCREEN.off) {
    TSCREEN.off = 0;
    tfulldirt();
  }

  for (n = 0; n < buflen; n += charsize) {
    if (IS_SET(MODE_UTF8)) {
      /* process a complete utf8 char */
      charsize = utf8decode(buf + n, &u, buflen - n);
      if (charsize == 0)
        break;
    } else {
      u = buf[n] & 0xFF;
      charsize = 1;
    }
    if (show_ctrl && ISCONTROL(u)) {
      if (u & 0x80) {
        u &= 0x7f;
        tputc('^');
        tputc('[');
      } else if (u != '\n' && u != '\r' && u != '\t') {
        u ^= 0x40;
        tputc('^');
      }
    }
    tputc(u);
  }
  return n;
}

void clearline(Line line, PGlyph g, int x, int xend) {
  int i;
  g.mode = 0;
  g.u = ' ';
  for (i = x; i < xend; ++i) {
    line[i] = g;
  }
}

Line ensureline(Line line) {
  if (!line) {
    line = xmalloc(term.linelen * sizeof(PGlyph));
  }
  return line;
}

void tresize(int col, int row) {
  int i, j;
  int minrow = MIN(row, term.row);
  int mincol = MIN(col, term.col);
  int linelen = MAX(col, term.linelen);
  int *bp;

  if (col < 1 || row < 1 || row > HISTSIZE) {
    fprintf(stderr, "tresize: error resizing to %dx%d\n", col, row);
    return;
  }

  /* Shift buffer to keep the cursor where we expect it */
  if (row <= term.cursor.y) {
    term.screen[0].cur =
        (term.screen[0].cur - row + term.cursor.y + 1) % term.screen[0].size;
  }

  /* Resize and clear line buffers as needed */
  if (linelen > term.linelen) {
    for (i = 0; i < term.screen[0].size; ++i) {
      if (term.screen[0].buffer[i]) {
        term.screen[0].buffer[i] =
            xrealloc(term.screen[0].buffer[i], linelen * sizeof(PGlyph));
        clearline(term.screen[0].buffer[i], term.cursor.attr, term.linelen,
                  linelen);
      }
    }
    for (i = 0; i < minrow; ++i) {
      term.screen[1].buffer[i] =
          xrealloc(term.screen[1].buffer[i], linelen * sizeof(PGlyph));
      clearline(term.screen[1].buffer[i], term.cursor.attr, term.linelen,
                linelen);
    }
  }
  /* Allocate all visible lines for regular line buffer */
  for (j = term.screen[0].cur, i = 0; i < row;
       ++i, j = (j + 1) % term.screen[0].size) {
    if (!term.screen[0].buffer[j]) {
      term.screen[0].buffer[j] = xmalloc(linelen * sizeof(PGlyph));
    }
    if (i >= term.row) {
      clearline(term.screen[0].buffer[j], term.cursor.attr, 0, linelen);
    }
  }
  /* Resize alt screen */
  term.screen[1].cur = 0;
  term.screen[1].size = row;
  for (i = row; i < term.row; ++i) {
    free(term.screen[1].buffer[i]);
  }
  term.screen[1].buffer = xrealloc(term.screen[1].buffer, row * sizeof(Line));
  for (i = term.row; i < row; ++i) {
    term.screen[1].buffer[i] = xmalloc(linelen * sizeof(PGlyph));
    clearline(term.screen[1].buffer[i], term.cursor.attr, 0, linelen);
  }

  /* resize to new height */
  term.dirty = xrealloc(term.dirty, row * sizeof(*term.dirty));
  term.tabs = xrealloc(term.tabs, col * sizeof(*term.tabs));

  /* fix tabstops */
  if (col > term.col) {
    bp = term.tabs + term.col;

    memset(bp, 0, sizeof(*term.tabs) * (col - term.col));
    while (--bp > term.tabs && !*bp)
      /* nothing */;
    for (bp += tabspaces; bp < term.tabs + col; bp += tabspaces)
      *bp = 1;
  }

  /* update terminal size */
  term.col = col;
  term.row = row;
  term.linelen = linelen;
  /* reset scrolling region */
  tsetscroll(0, row - 1);
  /* make use of the LIMIT in tmoveto */
  tmoveto(term.cursor.x, term.cursor.y);
  tfulldirt();
}

void resettitle(void) { xsettitle(NULL); }
