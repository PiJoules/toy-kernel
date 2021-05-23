#include <_syscalls.h>

#define INTERRUPT "$0x80"
#define RET_TYPE int32_t

int32_t sys_debug_print(const char *str) {
  RET_TYPE a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

bool sys_debug_put(char c) {
  char str[2] = {c, 0};
  return sys_debug_print(str) == 0;
}

void sys_exit_task() { asm volatile("int " INTERRUPT ::"a"(1)); }

bool sys_debug_read(char *c) {
  RET_TYPE a;
  asm volatile("int " INTERRUPT : "=a"(a) : "0"(2), "b"((uint32_t)c));
  return a == 0;
}

Handle sys_create_task(const void *entry, uint32_t codesize, void *arg,
                       size_t entry_offset) {
  Handle handle;
  asm volatile("int " INTERRUPT ::"a"(3), "b"((uint32_t)entry),
               "c"((uint32_t)codesize), "d"((uint32_t)arg),
               "S"((uint32_t)&handle), "D"((uint32_t)entry_offset));
  return handle;
}

void sys_destroy_task(Handle handle) {
  asm volatile("int " INTERRUPT ::"a"(4), "b"((uint32_t)handle));
}

void sys_copy_from_task(Handle handle, void *dst, const void *src,
                        size_t size) {
  asm volatile("int " INTERRUPT ::"a"(5), "b"(handle), "c"((uint32_t)dst),
               "d"((uint32_t)src), "S"((uint32_t)size));
}

Handle sys_get_parent_task() {
  Handle handle;
  asm volatile("int " INTERRUPT ::"a"(6), "b"((uint32_t)&handle));
  return handle;
}

uint32_t sys_get_parent_task_id() {
  uint32_t id;
  asm volatile("int " INTERRUPT ::"a"(7), "b"((uint32_t)&id));
  return id;
}

int32_t sys_map_page(void *addr) {
  RET_TYPE ret;
  asm volatile("int " INTERRUPT : "=a"(ret) : "0"(8), "b"((uint32_t)addr));
  return ret;
}

void sys_share_page(Handle handle, void **dst, const void *src) {
  asm volatile("int " INTERRUPT ::"a"(9), "b"(handle), "c"((uint32_t)dst),
               "d"((uint32_t)src));
}

void sys_unmap_page(void *dst) {
  asm volatile("int " INTERRUPT ::"a"(10), "b"((uint32_t)dst));
}
