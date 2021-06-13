#ifndef UNISTD_H
#define UNISTD_H

#include <_internals.h>
#include <stdint.h>

__BEGIN_CDECLS

// The getcwd() function shall place an absolute pathname of the current working
// directory in the array pointed to by buf, and return buf. The pathname copied
// to the array shall contain no components that are symbolic links. The size
// argument is the size in bytes of the character array pointed to by the buf
// argument. If buf is a null pointer, the behavior of getcwd() is unspecified.
//
// Upon successful completion, getcwd() shall return the buf argument.
// Otherwise, getcwd() shall return a null pointer and set errno to indicate the
// error. The contents of the array pointed to by buf are then undefined.
char *getcwd(char *buf, size_t size);

__END_CDECLS

#endif
