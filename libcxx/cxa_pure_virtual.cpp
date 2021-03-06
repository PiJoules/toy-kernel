#ifdef KERNEL
#include <panic.h>

extern "C" void __cxa_pure_virtual() {
  PANIC(
      "__cxa_pure_virtual called! This probably happenned from calling an "
      "abstract virtual function for an object before the object was set up.");
}

#else

extern "C" void __cxa_pure_virtual() { __builtin_trap(); }

#endif
