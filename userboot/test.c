#include <_syscalls.h>
#include <stdio.h>

extern bool __use_debug_log;

int main() {
  __use_debug_log = true;
  printf("Hello from an elf program.\n");

  sys_exit_task();
  return 0;
}
