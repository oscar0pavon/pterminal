#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>
#include "terminal.h"

#define UTF_INVALID 0xFFFD
#define UTF_SIZ 4

size_t utf8decode(const char *, Rune *, size_t);
Rune utf8decodebyte(char, size_t *);
char utf8encodebyte(Rune, size_t);
size_t utf8validate(Rune *, size_t);

int get_texture_atlas_index(unsigned int unicode_char);

size_t utf8encode(Rune, char *);

#endif
