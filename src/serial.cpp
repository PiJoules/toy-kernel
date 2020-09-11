#include <io.h>
#include <kstdint.h>

namespace serial {

constexpr uint16_t kCOM1 = 0x3f8;

namespace {

bool Received() { return Read8(kCOM1 + 5) & 1; }

bool IsTransmitEmpty() { return Read8(kCOM1 + 5) & 0x20; }

}  // namespace

void Initialize() {
  Write8(kCOM1 + 1, 0x00);  // Disable all interrupts
  Write8(kCOM1 + 3, 0x80);  // Enable DLAB (set baud rate divisor)
  Write8(kCOM1 + 0, 0x03);  // Set divisor to 3 (lo byte) 38400 baud
  Write8(kCOM1 + 1, 0x00);  //                  (hi byte)
  Write8(kCOM1 + 3, 0x03);  // 8 bits, no parity, one stop bit
  Write8(kCOM1 + 2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
  Write8(kCOM1 + 4, 0x0B);  // IRQs enabled, RTS/DSR set
}

char Read() {
  while (!Received()) {}
  return Read8(kCOM1);
}

void Write(char c) {
  while (!IsTransmitEmpty()) {}
  Write8(kCOM1, c);
}

}  // namespace serial
