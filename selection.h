#ifndef SELECTION_H
#define SELECTION_H

typedef struct {
  int mode;
  int type;
  int snap;
  /*
   * Selection variables:
   * nb – normalized coordinates of the beginning of the selection
   * ne – normalized coordinates of the end of the selection
   * ob – original coordinates of the beginning of the selection
   * oe – original coordinates of the end of the selection
   */
  struct {
    int x, y;
  } beginning_normalized, end_normalized, original_beginning, original_end;

  int alt;
} Selection;

extern Selection selection;

void selnormalize(void);
void selscroll(int, int);
void selsnap(int *, int *, int);

void selclear(void);
void selinit(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
int selected(int, int);
char *getsel(void);

#endif
