#include <_syscalls.h>
#include <print.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

// FIXME: We will be printing characters with serial for now, but should really
// implement a filesystem-like structure for more "standard" printing.

namespace {

void __system_print(const char *str) { sys::DebugPrint(str); }

void __system_put(char c) { sys::DebugPut(c); }

}  // namespace

// FIXME: Have a more graceful way of indicating a crash rather than just using
// __builtin_trap().
extern "C" int printf(const char *fmt, ...) {
  // FIXME: Actually record the number of characters written.
  int num_written = 0;
  va_list ap;
  va_start(ap, fmt);

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
        case 'p': {
          void *p = va_arg(ap, void *);
          print::Print(__system_put, "{}", p);
          break;
        }
        case 'x': {
          unsigned u = va_arg(ap, unsigned);
          print::Print(__system_put, "{}", print::Hex(u));
          break;
        }
        default:
          __builtin_trap();
      }
    } else {
      __system_put(c);
    }
  }

  va_end(ap);
  return num_written;
}

extern "C" void put(char c) { __system_put(c); }

extern "C" int putchar(int c) {
  if (sys::DebugPut(static_cast<char>(c))) return c;
  return EOF;
}

extern "C" int getchar() {
  char c;
  while (!sys::DebugRead(c)) {}
  return c;
}
