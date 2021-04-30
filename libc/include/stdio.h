#ifndef STDIO_H_
#define STDIO_H_

#include <_internals.h>

#define EOF (-1)

__BEGIN_CDECLS

int printf(const char *s, ...);
void put(char c);  // FIXME: This should be putchar.
int putchar(int c);
int puts(const char *);
int getchar();

__END_CDECLS

#endif
