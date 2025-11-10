#ifndef ANSI_ESCAPES_H
#define ANSI_ESCAPES_H

#include <stdlib.h>
#include <uchar.h>
#include "types.h"
#include "utf8.h"

#define ESC_BUF_SIZ (128 * UTF_SIZ)
#define ESC_ARG_SIZ 16
#define STR_BUF_SIZ ESC_BUF_SIZ
#define STR_ARG_SIZ ESC_ARG_SIZ

enum escape_state {
  ESC_START = 1,
  ESC_CSI = 2,
  ESC_STR = 4, /* DCS, OSC, PM, APC */
  ESC_ALTCHARSET = 8,
  ESC_STR_END = 16, /* a final string was encountered */
  ESC_TEST = 32,    /* Enter in test mode */
  ESC_UTF8 = 64,
};

/* CSI Escape sequence structs */
/* ESC '[' [[ [<priv>] <arg> [;]] <mode> [<mode>]] */
typedef struct {
  char buf[ESC_BUF_SIZ]; /* raw string */
  size_t len;            /* raw string length */
  char priv;
  int arg[ESC_ARG_SIZ];
  int narg; /* nb of args */
  char mode[2];
} CSIEscape;

/* STR Escape sequence structs */
/* ESC type [[ [<priv>] <arg> [;]] <mode>] ESC '\' */
typedef struct {
  char type;  /* ESC type ... */
  char *buf;  /* allocated raw string */
  size_t siz; /* allocation size */
  size_t len; /* raw string length */
  char *args[STR_ARG_SIZ];
  int narg; /* nb of args */

} STREscape;

void csidump(void);
void csihandle(void);
void csiparse(void);
void csireset(void);
void osc_color_response(int, int, int);
int eschandle(uchar);
void strdump(void);
void strhandle(void);
void strparse(void);
void strreset(void);

extern CSIEscape csiescseq;
extern STREscape strescseq;

#endif
