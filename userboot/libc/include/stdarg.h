#ifndef STDARG_H_
#define STDARG_H_

extern "C" {

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v, l)
#define va_copy(v, l)  __builtin_va_copy(v, l)

}

#endif
