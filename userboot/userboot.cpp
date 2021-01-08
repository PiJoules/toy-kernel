#include <stdint.h>
#include <stdio.h>

extern bool __use_debug_log;

const uint16_t kCOM1 = 0x3f8;

// Functions for reading from and writing to ports.
uint8_t Read8(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

static bool Received() { return Read8(kCOM1 + 5) & 1; }

char Read() {
  while (!Received()) {}
  return Read8(kCOM1);
}

constexpr char CR = 13;  // Carriage return

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
void DebugRead(char *buffer) {
  while (1) {
    char c = Read();
    if (c == CR) {
      *buffer = 0;
      put('\n');
      return;
    }

    *(buffer++) = c;
    put(c);
  }
}

constexpr size_t kCmdBufferSize = 1024;

extern "C" int main() {
  __use_debug_log = true;

  char buffer[kCmdBufferSize];
  while (1) {
    printf("shell> ");
    DebugRead(buffer);
    printf("%s\n", buffer);
  }
}
