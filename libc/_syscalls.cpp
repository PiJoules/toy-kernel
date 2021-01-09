#include <_syscalls.h>

#define INTERRUPT "$0x80"

namespace sys {

uint32_t DebugPrint(const char *str) {
  uint32_t a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

uint32_t DebugPut(char c) {
  char str[2] = {c, 0};
  return DebugPrint(str);
}

char DebugRead() {
  uint32_t a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(2));
  return a;
}

}  // namespace sys
