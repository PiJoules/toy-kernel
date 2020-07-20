#ifndef KASSERT_H_
#define KASSERT_H_

#include <kernel.h>

void __assert(bool expr, const char *msg, const char *filename, int line,
              const char *pretty_func);

#ifdef NDEBUG
#define assert(condition) ((void)(condition))
#else
#define assert(condition) \
  (__assert(condition, STR(condition), __FILE__, __LINE__, __PRETTY_FUNCTION__))
#endif

#endif
