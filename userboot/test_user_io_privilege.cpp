#include <stdint.h>

constexpr uint16_t kCOM1 = 0x3f8;

void Write8(uint16_t port, uint8_t value) {
  asm volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

extern "C" int main() {
  // This should cause a general protection fault.
  Write8(kCOM1, 0);
  return 0;
}
