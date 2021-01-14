#include <_syscalls.h>
#include <print.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

bool __use_debug_log = false;

// FIXME: We will be printing characters with serial for now, but should really
// implement a filesystem-like structure for more "standard" printing.

namespace {

void __system_print(const char *str) {
  if (__use_debug_log)
    sys::DebugPrint(str);
  else
    __builtin_trap();
}

void __system_put(char c) {
  if (__use_debug_log)
    sys::DebugPut(c);
  else
    __builtin_trap();
}

}  // namespace

// FIXME: Have a more graceful way of indicating a crash rather than just using
// __builtin_trap().
extern "C" int printf(const char *fmt, ...) {
  // FIXME: Actually record the number of characters written.
  int num_written = 0;
  va_list ap;
  va_start(ap, fmt);

  if (__use_debug_log) {
    char c;
    while ((c = *(fmt++))) {
      if (c == '%') {
        c = *(fmt++);
        switch (c) {
          case 'c': {
            char other_char = static_cast<char>(va_arg(ap, int));
            __system_put(other_char);
            break;
          }
          case 'd': {
            int i = va_arg(ap, int);
            print::Print(__system_put, "{}", i);
            break;
          }
          case 'u': {
            unsigned i = va_arg(ap, unsigned);
            print::Print(__system_put, "{}", i);
            break;
          }
          case 's': {
            char *s = va_arg(ap, char *);
            __system_print(s);
            break;
          }
          // case 'p': {
          //  void *s = va_arg(ap, void *);
          //  __system_print(s);
          //  break;
          //}
          default:
            __builtin_trap();
        }
      } else {
        __system_put(c);
      }
    }
  } else {
    // No other way of printing rn.
    __builtin_trap();
  }

  va_end(ap);
  return num_written;
}

extern "C" void put(char c) { __system_put(c); }
