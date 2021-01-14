typedef unsigned int uint32_t;

int i = 0;

uint32_t syscall_terminal_write(const char *str) {
  uint32_t a;
  asm volatile("int $0x80" : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

int __user_main() {
  i = 0;
  syscall_terminal_write("  Hello from\n");
  syscall_terminal_write("  userspace program!\n");

  // FIXME: If we have two user processes running this program, one will print
  // "i == 1" and the other will print "i == 2". This is because user programs
  // are mapped to the same physical memory.
  ++i;
  if (i == 1) {
    syscall_terminal_write(
        "  i == 1: [SUCCESS] Each process has its own address space\n");
  } else if (i == 2) {
    syscall_terminal_write(
        "  i == 2: [ERROR] Processes are sharing address spaces\n");
  } else {
    syscall_terminal_write(
        "  i != 1 && i != 2: [ERROR] Something unexpected happened\n");
  }

  syscall_terminal_write("\n");
  return 0;
}
