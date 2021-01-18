#include <_syscalls.h>

#define INTERRUPT "$0x80"

namespace sys {

uint32_t DebugPrint(const char *str) {
  uint32_t a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

void DebugPut(char c) {
  char str[2] = {c, 0};
  DebugPrint(str);
}

void ExitTask() { asm volatile("int " INTERRUPT ::"a"(1)); }

bool DebugRead(char &c) {
  int32_t a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(2), "b"((int32_t)&c));
  return a;
}

}  // namespace sys
