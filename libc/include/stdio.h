#ifndef STDIO_H_
#define STDIO_H_

#include <_internals.h>

__BEGIN_CDECLS

int printf(const char *s, ...);
void put(char c);  // FIXME: This should be putc.
int puts(const char *);

__END_CDECLS

#endif
