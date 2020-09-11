#include <kstdlib.h>
#include <panic.h>

void abort() { PANIC("abort"); }
