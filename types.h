#ifndef TYPES_H
#define TYPES_H

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef union {
  int i;
  uint ui;
  float f;
  const void *v;
  const char *s;
} Arg;

#endif
