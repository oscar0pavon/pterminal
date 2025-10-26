/* See LICENSE for license details. */
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>

enum term_mode {
  MODE_WRAP = 1 << 0,
  MODE_INSERT = 1 << 1,
  MODE_ALTSCREEN = 1 << 2,
  MODE_CRLF = 1 << 3,
  MODE_ECHO = 1 << 4,
  MODE_PRINT = 1 << 5,
  MODE_UTF8 = 1 << 6,
};

/* macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b) ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d) (((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b) (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b) (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define ATTRCMP(a, b)                                                          \
  ((a).mode != (b).mode || (a).fg != (b).fg || (a).bg != (b).bg)
#define TIMEDIFF(t1, t2)                                                       \
  ((t1.tv_sec - t2.tv_sec) * 1000 + (t1.tv_nsec - t2.tv_nsec) / 1E6)
#define MODBIT(x, set, bit) ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r, g, b) (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x) (1 << 24 & (x))
#define IS_SET(flag) ((term.mode & (flag)) != 0)
#define ISCONTROLC0(c) (BETWEEN(c, 0, 0x1f) || (c) == 0x7f)
#define ISCONTROLC1(c) (BETWEEN(c, 0x80, 0x9f))
#define ISCONTROL(c) (ISCONTROLC0(c) || ISCONTROLC1(c))
#define ISDELIM(u) (u && wcschr(worddelimiters, u))

#define TSCREEN term.screen[IS_SET(MODE_ALTSCREEN)]
#define TLINEOFFSET(y)                                                         \
  (((y) + TSCREEN.cur - TSCREEN.off + TSCREEN.size) % TSCREEN.size)
#define TLINE(y) (TSCREEN.buffer[TLINEOFFSET(y)])

#define HISTSIZE 2000

enum glyph_attribute {
  ATTR_NULL = 0,
  ATTR_BOLD = 1 << 0,
  ATTR_FAINT = 1 << 1,
  ATTR_ITALIC = 1 << 2,
  ATTR_UNDERLINE = 1 << 3,
  ATTR_BLINK = 1 << 4,
  ATTR_REVERSE = 1 << 5,
  ATTR_INVISIBLE = 1 << 6,
  ATTR_STRUCK = 1 << 7,
  ATTR_WRAP = 1 << 8,
  ATTR_WIDE = 1 << 9,
  ATTR_WDUMMY = 1 << 10,
  ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
};

enum selection_mode { SEL_IDLE = 0, SEL_EMPTY = 1, SEL_READY = 2 };

enum selection_type { SEL_REGULAR = 1, SEL_RECTANGULAR = 2 };

enum selection_snap { SNAP_WORD = 1, SNAP_LINE = 2 };

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef uint_least32_t Rune;

#define Glyph Glyph_
typedef struct {
  Rune u;      /* character code */
  ushort mode; /* attribute flags */
  uint32_t fg; /* foreground  */
  uint32_t bg; /* background  */
} Glyph;

typedef Glyph *Line;

typedef union {
  int i;
  uint ui;
  float f;
  const void *v;
  const char *s;
} Arg;


typedef struct {
  Glyph attr; /* current char attributes */
  int x;
  int y;
  char state;
} TCursor;

/* Screen lines */
typedef struct {
  Line *buffer; /* ring buffer */
  int size;     /* size of buffer */
  int cur;      /* start of active screen */
  int off;      /* scrollback line offset */
  TCursor sc;   /* saved cursor */
} LineBuffer;

/* Internal representation of the screen */
typedef struct {
  int row;              /* nb row */
  int col;              /* nb col */
  LineBuffer screen[2]; /* screen and alternate screen */
  int linelen;          /* allocated line length */
  int *dirty;           /* dirtyness of lines */
  TCursor c;            /* cursor */
  int ocx;              /* old cursor col */
  int ocy;              /* old cursor row */
  int top;              /* top    scroll limit */
  int bot;              /* bottom scroll limit */
  int mode;             /* terminal mode flags */
  int esc;              /* escape state flags */
  char trantbl[4];      /* charset table translation */
  int charset;          /* current charset */
  int icharset;         /* selected charset for sequence */
  int *tabs;
  Rune lastc; /* last printed char outside of sequence, 0 if control */
} Term;

void die(const char *, ...);

void printscreen(const Arg *);
void printsel(const Arg *);
void sendbreak(const Arg *);
void toggleprinter(const Arg *);

int tattrset(int);
void tnew(int, int);
void tresize(int, int);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);

void resettitle(void);

void redraw(void);

void selclear(void);
void selinit(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
int selected(int, int);
char *getsel(void);

size_t utf8encode(Rune, char *);

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);

/* config.h globals */
extern Term term;
extern char *utmp;
extern char *scroll;
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern int allowaltscreen;
extern int allowwindowops;
extern char *termname;
extern unsigned int tabspaces;
extern unsigned int defaultfg;
extern unsigned int defaultbg;
extern unsigned int defaultcs;

#endif
