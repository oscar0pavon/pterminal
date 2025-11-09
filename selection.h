#ifndef SELECTION_H
#define SELECTION_H



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
