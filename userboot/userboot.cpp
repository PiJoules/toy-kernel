typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

const uint16_t kCOM1 = 0x3f8;

// Functions for reading from and writing to ports.
void Write8(uint16_t port, uint8_t value) {
  asm volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

uint8_t Read8(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

static bool Received() { return Read8(kCOM1 + 5) & 1; }

static bool IsTransmitEmpty() { return Read8(kCOM1 + 5) & 0x20; }

uint32_t syscall_terminal_write(const char *str) {
  uint32_t a;
  asm volatile("int $0x80" : "=a"(a) : "0"(0), "b"((uint32_t)str));
  return a;
}

char Read() {
  while (!Received()) {}
  return Read8(kCOM1);
}

void Put(char c) {
  while (!IsTransmitEmpty()) {}
  Write8(kCOM1, c);
}

#define kCmdBufferSize 1024

extern "C" int main() {
  syscall_terminal_write("shell> ");
  while (1) {
    char c = Read();
    Put(c);
  }
}
