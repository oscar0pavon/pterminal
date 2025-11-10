#include "ansi_escapes.h"
#include <stdio.h>

#include "color.h"
#include <ctype.h>
#include "terminal.h"
#include "window.h"

CSIEscape csiescseq;
STREscape strescseq;

void csihandle(void) {
  char buf[40];
  int len;

  switch (csiescseq.mode[0]) {
  default:
  unknown:
    fprintf(stderr, "erresc: unknown csi ");
    csidump();
    /* die(""); */
    break;
  case '@': /* ICH -- Insert <n> blank char */
    DEFAULT(csiescseq.arg[0], 1);
    tinsertblank(csiescseq.arg[0]);
    break;
  case 'A': /* CUU -- Cursor <n> Up */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(term.cursor.x, term.cursor.y - csiescseq.arg[0]);
    break;
  case 'B': /* CUD -- Cursor <n> Down */
  case 'e': /* VPR --Cursor <n> Down */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(term.cursor.x, term.cursor.y + csiescseq.arg[0]);
    break;
  case 'i': /* MC -- Media Copy */
    switch (csiescseq.arg[0]) {
    case 0:
      tdump();
      break;
    case 1:
      tdumpline(term.cursor.y);
      break;
    case 2:
      tdumpsel();
      break;
    case 4:
      term.mode &= ~MODE_PRINT;
      break;
    case 5:
      term.mode |= MODE_PRINT;
      break;
    }
    break;
  case 'c': /* DA -- Device Attributes */
    if (csiescseq.arg[0] == 0)
      ttywrite(vtiden, strlen(vtiden), 0);
    break;
  case 'b': /* REP -- if last char is printable print it <n> more times */
    LIMIT(csiescseq.arg[0], 1, 65535);
    if (term.lastc)
      while (csiescseq.arg[0]-- > 0)
        tputc(term.lastc);
    break;
  case 'C': /* CUF -- Cursor <n> Forward */
  case 'a': /* HPR -- Cursor <n> Forward */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(term.cursor.x + csiescseq.arg[0], term.cursor.y);
    break;
  case 'D': /* CUB -- Cursor <n> Backward */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(term.cursor.x - csiescseq.arg[0], term.cursor.y);
    break;
  case 'E': /* CNL -- Cursor <n> Down and first col */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(0, term.cursor.y + csiescseq.arg[0]);
    break;
  case 'F': /* CPL -- Cursor <n> Up and first col */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(0, term.cursor.y - csiescseq.arg[0]);
    break;
  case 'g': /* TBC -- Tabulation clear */
    switch (csiescseq.arg[0]) {
    case 0: /* clear current tab stop */
      term.tabs[term.cursor.x] = 0;
      break;
    case 3: /* clear all the tabs */
      memset(term.tabs, 0, term.col * sizeof(*term.tabs));
      break;
    default:
      goto unknown;
    }
    break;
  case 'G': /* CHA -- Move to <col> */
  case '`': /* HPA */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveto(csiescseq.arg[0] - 1, term.cursor.y);
    break;
  case 'H': /* CUP -- Move to <row> <col> */
  case 'f': /* HVP */
    DEFAULT(csiescseq.arg[0], 1);
    DEFAULT(csiescseq.arg[1], 1);
    tmoveato(csiescseq.arg[1] - 1, csiescseq.arg[0] - 1);
    break;
  case 'I': /* CHT -- Cursor Forward Tabulation <n> tab stops */
    DEFAULT(csiescseq.arg[0], 1);
    tputtab(csiescseq.arg[0]);
    break;
  case 'J': /* ED -- Clear screen */
    switch (csiescseq.arg[0]) {
    case 0: /* below */
      tclearregion(term.cursor.x, term.cursor.y, term.col - 1, term.cursor.y);
      if (term.cursor.y < term.row - 1) {
        tclearregion(0, term.cursor.y + 1, term.col - 1, term.row - 1);
      }
      break;
    case 1: /* above */
      if (term.cursor.y > 1)
        tclearregion(0, 0, term.col - 1, term.cursor.y - 1);
      tclearregion(0, term.cursor.y, term.cursor.x, term.cursor.y);
      break;
    case 2: /* all */
      tclearregion(0, 0, term.col - 1, term.row - 1);
      break;
    default:
      goto unknown;
    }
    break;
  case 'K': /* EL -- Clear line */
    switch (csiescseq.arg[0]) {
    case 0: /* right */
      tclearregion(term.cursor.x, term.cursor.y, term.col - 1, term.cursor.y);
      break;
    case 1: /* left */
      tclearregion(0, term.cursor.y, term.cursor.x, term.cursor.y);
      break;
    case 2: /* all */
      tclearregion(0, term.cursor.y, term.col - 1, term.cursor.y);
      break;
    }
    break;
  case 'S': /* SU -- Scroll <n> line up */
    if (csiescseq.priv)
      break;
    DEFAULT(csiescseq.arg[0], 1);
    tscrollup(term.top, csiescseq.arg[0]);
    break;
  case 'T': /* SD -- Scroll <n> line down */
    DEFAULT(csiescseq.arg[0], 1);
    tscrolldown(term.top, csiescseq.arg[0]);
    break;
  case 'L': /* IL -- Insert <n> blank lines */
    DEFAULT(csiescseq.arg[0], 1);
    tinsertblankline(csiescseq.arg[0]);
    break;
  case 'l': /* RM -- Reset Mode */
    tsetmode(csiescseq.priv, 0, csiescseq.arg, csiescseq.narg);
    break;
  case 'M': /* DL -- Delete <n> lines */
    DEFAULT(csiescseq.arg[0], 1);
    tdeleteline(csiescseq.arg[0]);
    break;
  case 'X': /* ECH -- Erase <n> char */
    DEFAULT(csiescseq.arg[0], 1);
    tclearregion(term.cursor.x, term.cursor.y,
                 term.cursor.x + csiescseq.arg[0] - 1, term.cursor.y);
    break;
  case 'P': /* DCH -- Delete <n> char */
    DEFAULT(csiescseq.arg[0], 1);
    tdeletechar(csiescseq.arg[0]);
    break;
  case 'Z': /* CBT -- Cursor Backward Tabulation <n> tab stops */
    DEFAULT(csiescseq.arg[0], 1);
    tputtab(-csiescseq.arg[0]);
    break;
  case 'd': /* VPA -- Move to <row> */
    DEFAULT(csiescseq.arg[0], 1);
    tmoveato(term.cursor.x, csiescseq.arg[0] - 1);
    break;
  case 'h': /* SM -- Set terminal mode */
    tsetmode(csiescseq.priv, 1, csiescseq.arg, csiescseq.narg);
    break;
  case 'm': /* SGR -- Terminal attribute (color) */
    tsetattr(csiescseq.arg, csiescseq.narg);
    break;
  case 'n': /* DSR -- Device Status Report */
    switch (csiescseq.arg[0]) {
    case 5: /* Status Report "OK" `0n` */
      ttywrite("\033[0n", sizeof("\033[0n") - 1, 0);
      break;
    case 6: /* Report Cursor Position (CPR) "<row>;<column>R" */
      len = snprintf(buf, sizeof(buf), "\033[%i;%iR", term.cursor.y + 1,
                     term.cursor.x + 1);
      ttywrite(buf, len, 0);
      break;
    default:
      goto unknown;
    }
    break;
  case 'r': /* DECSTBM -- Set Scrolling Region */
    if (csiescseq.priv) {
      goto unknown;
    } else {
      DEFAULT(csiescseq.arg[0], 1);
      DEFAULT(csiescseq.arg[1], term.row);
      tsetscroll(csiescseq.arg[0] - 1, csiescseq.arg[1] - 1);
      tmoveato(0, 0);
    }
    break;
  case 's': /* DECSC -- Save cursor position (ANSI.SYS) */
    tcursor(CURSOR_SAVE);
    break;
  case 'u': /* DECRC -- Restore cursor position (ANSI.SYS) */
    tcursor(CURSOR_LOAD);
    break;
  case ' ':
    switch (csiescseq.mode[1]) {
    case 'q': /* DECSCUSR -- Set Cursor Style */
      if (xsetcursor(csiescseq.arg[0]))
        goto unknown;
      break;
    default:
      goto unknown;
    }
    break;
  }
}


void strhandle(void) {
  char *p = NULL, *dec;
  int j, narg, par;
  const struct {
    int idx;
    char *str;
  } osc_table[] = {{defaultfg, "foreground"},
                   {defaultbg, "background"},
                   {defaultcs, "cursor"}};

  term.esc &= ~(ESC_STR_END | ESC_STR);
  strparse();
  par = (narg = strescseq.narg) ? atoi(strescseq.args[0]) : 0;

  switch (strescseq.type) {
  case ']': /* OSC -- Operating System Command */
    switch (par) {
    case 0:
      if (narg > 1) {
        xsettitle(strescseq.args[1]);
        xseticontitle(strescseq.args[1]);
      }
      return;
    case 1:
      if (narg > 1)
        xseticontitle(strescseq.args[1]);
      return;
    case 2:
      if (narg > 1)
        xsettitle(strescseq.args[1]);
      return;
    case 52:
      if (narg > 2 && allowwindowops) {
        dec = base64dec(strescseq.args[2]);
        if (dec) {
          xsetsel(dec);
          xclipcopy();
        } else {
          fprintf(stderr, "erresc: invalid base64\n");
        }
      }
      return;
    case 10:
    case 11:
    case 12:
      if (narg < 2)
        break;
      p = strescseq.args[1];
      if ((j = par - 10) < 0 || j >= LEN(osc_table))
        break; /* shouldn't be possible */

      if (!strcmp(p, "?")) {
        osc_color_response(par, osc_table[j].idx, 0);
      } else if (xsetcolorname(osc_table[j].idx, p)) {
        fprintf(stderr, "erresc: invalid %s color: %s\n", osc_table[j].str, p);
      } else {
        tfulldirt();
      }
      return;
    case 4: /* color set */
      if (narg < 3)
        break;
      p = strescseq.args[2];
      /* FALLTHROUGH */
    case 104: /* color reset */
      j = (narg > 1) ? atoi(strescseq.args[1]) : -1;

      if (p && !strcmp(p, "?")) {
        osc_color_response(j, 0, 1);
      } else if (xsetcolorname(j, p)) {
        if (par == 104 && narg <= 1) {
          xloadcols();
          return; /* color reset without parameter */
        }
        fprintf(stderr, "erresc: invalid color j=%d, p=%s\n", j,
                p ? p : "(null)");
      } else {
        /*
         * TODO if defaultbg color is changed, borders
         * are dirty
         */
        tfulldirt();
      }
      return;
    }
    break;
  case 'k': /* old title set compatibility */
    xsettitle(strescseq.args[0]);
    return;
  case 'P': /* DCS -- Device Control String */
  case '_': /* APC -- Application Program Command */
  case '^': /* PM -- Privacy Message */
    return;
  }

  fprintf(stderr, "erresc: unknown str ");
  strdump();
}

void strparse(void) {
  int c;
  char *p = strescseq.buf;

  strescseq.narg = 0;
  strescseq.buf[strescseq.len] = '\0';

  if (*p == '\0')
    return;

  while (strescseq.narg < STR_ARG_SIZ) {
    strescseq.args[strescseq.narg++] = p;
    while ((c = *p) != ';' && c != '\0')
      ++p;
    if (c == '\0')
      return;
    *p++ = '\0';
  }
}

void strdump(void) {
  size_t i;
  uint c;

  fprintf(stderr, "ESC%c", strescseq.type);
  for (i = 0; i < strescseq.len; i++) {
    c = strescseq.buf[i] & 0xff;
    if (c == '\0') {
      putc('\n', stderr);
      return;
    } else if (isprint(c)) {
      putc(c, stderr);
    } else if (c == '\n') {
      fprintf(stderr, "(\\n)");
    } else if (c == '\r') {
      fprintf(stderr, "(\\r)");
    } else if (c == 0x1b) {
      fprintf(stderr, "(\\e)");
    } else {
      fprintf(stderr, "(%02x)", c);
    }
  }
  fprintf(stderr, "ESC\\\n");
}

void strreset(void) {
  strescseq = (STREscape){
      .buf = xrealloc(strescseq.buf, STR_BUF_SIZ),
      .siz = STR_BUF_SIZ,
  };
}
void csiparse(void) {
  char *p = csiescseq.buf, *np;
  long int v;
  int sep = ';'; /* colon or semi-colon, but not both */

  csiescseq.narg = 0;
  if (*p == '?') {
    csiescseq.priv = 1;
    p++;
  }

  csiescseq.buf[csiescseq.len] = '\0';
  while (p < csiescseq.buf + csiescseq.len) {
    np = NULL;
    v = strtol(p, &np, 10);
    if (np == p)
      v = 0;
    if (v == LONG_MAX || v == LONG_MIN)
      v = -1;
    csiescseq.arg[csiescseq.narg++] = v;
    p = np;
    if (sep == ';' && *p == ':')
      sep = ':'; /* allow override to colon once */
    if (*p != sep || csiescseq.narg == ESC_ARG_SIZ)
      break;
    p++;
  }
  csiescseq.mode[0] = *p++;
  csiescseq.mode[1] = (p < csiescseq.buf + csiescseq.len) ? *p : '\0';
}

void csidump(void) {
  size_t i;
  uint c;

  fprintf(stderr, "ESC[");
  for (i = 0; i < csiescseq.len; i++) {
    c = csiescseq.buf[i] & 0xff;
    if (isprint(c)) {
      putc(c, stderr);
    } else if (c == '\n') {
      fprintf(stderr, "(\\n)");
    } else if (c == '\r') {
      fprintf(stderr, "(\\r)");
    } else if (c == 0x1b) {
      fprintf(stderr, "(\\e)");
    } else {
      fprintf(stderr, "(%02x)", c);
    }
  }
  putc('\n', stderr);
}

void csireset(void) { memset(&csiescseq, 0, sizeof(csiescseq)); }

void osc_color_response(int num, int index, int is_osc4) {
  int n;
  char buf[32];
  unsigned char r, g, b;

  if (xgetcolor(is_osc4 ? num : index, &r, &g, &b)) {
    fprintf(stderr, "erresc: failed to fetch %s color %d\n",
            is_osc4 ? "osc4" : "osc", is_osc4 ? num : index);
    return;
  }

  n = snprintf(buf, sizeof buf, "\033]%s%d;rgb:%02x%02x/%02x%02x/%02x%02x\007",
               is_osc4 ? "4;" : "", num, r, r, g, g, b, b);
  if (n < 0 || n >= sizeof(buf)) {
    fprintf(stderr, "error: %s while printing %s response\n",
            n < 0 ? "snprintf failed" : "truncation occurred",
            is_osc4 ? "osc4" : "osc");
  } else {
    ttywrite(buf, n, 1);
  }
}
