typedef unsigned int uint32_t;

uint32_t syscall_terminal_write(const char *str) {
  uint32_t a;
  asm volatile("int $0x80" : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

int main() { syscall_terminal_write("shell> \n"); }
