#include <stdlib.h>

#ifdef KERNEL
#include <panic.h>

void abort() { PANIC("abort"); }
#endif
