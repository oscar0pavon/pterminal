#ifndef TTY_H
#define TTY_H

#include <stdlib.h>

void new_serial_tty(char **);
void ttywriteraw(const char *, size_t);
void ttywrite(const char *s, size_t n, int may_echo);

void *handle_tty(void *none);

extern int iofd;
extern int cmdfd;
extern pid_t pid;

#endif
