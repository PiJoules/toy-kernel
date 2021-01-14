#ifndef ASSERT_H_
#define ASSERT_H_

void __assert(bool expr, const char *msg, const char *filename, int line,
              const char *pretty_func);

#ifndef STR
#define __STR(s) STR(s)
#define STR(s) #s
#endif

#ifdef NDEBUG
#define assert(condition) ((void)(condition))
#else
#define assert(condition) \
  (__assert(condition, STR(condition), __FILE__, __LINE__, __PRETTY_FUNCTION__))
#endif

#endif
