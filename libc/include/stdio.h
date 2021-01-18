#ifndef STDIO_H_
#define STDIO_H_

extern "C" {

int printf(const char *s, ...);
void put(char c);  // FIXME: This should be putc.
int puts(const char *);

}  // extern "C"

#endif
