#ifndef TTY_H
#define TTY_H

#include <stdlib.h>

void new_serial_tty(char **);
void ttywriteraw(const char *, size_t);
void ttywrite(const char *s, size_t n, int may_echo);

int ttynew(const char *line, char *cmd, const char *out, char **args);

size_t read_tty(void);


extern int iofd;
extern int cmdfd;
extern pid_t pid;

#endif
