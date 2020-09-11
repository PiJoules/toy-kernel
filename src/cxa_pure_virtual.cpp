#include <panic.h>

extern "C" void __cxa_pure_virtual() { PANIC("__cxa_pure_virtual called!"); }
