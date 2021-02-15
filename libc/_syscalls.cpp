#include <_syscalls.h>

#define INTERRUPT "$0x80"
#define RET_TYPE int32_t

uint32_t sys_debug_print(const char *str) {
  RET_TYPE a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

void sys_debug_put(char c) {
  char str[2] = {c, 0};
  sys_debug_print(str);
}

void sys_exit_task() { asm volatile("int " INTERRUPT ::"a"(1)); }

bool sys_debug_read(char *c) {
  RET_TYPE a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(2), "b"((uint32_t)c));
  return a;
}

Handle sys_create_task(const void *entry, uint32_t codesize, void *arg) {
  Handle handle;
  asm volatile("int " INTERRUPT ::"a"(3), "b"((uint32_t)entry),
               "c"((uint32_t)codesize), "d"((uint32_t)arg),
               "S"((uint32_t)&handle));
  return handle;
}

void sys_destroy_task(Handle handle) {
  asm volatile("int " INTERRUPT ::"a"(4), "b"((uint32_t)handle));
}
