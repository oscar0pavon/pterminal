#ifndef TTY_H
#define TTY_H

#include <stdlib.h>

void stty(char **);
void ttywriteraw(const char *, size_t);
void ttywrite(const char *s, size_t n, int may_echo);

extern int iofd;
extern int cmdfd;
extern pid_t pid;

#endif
