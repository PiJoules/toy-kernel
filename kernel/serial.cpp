#include <io.h>
#include <stdint.h>
#include <string.h>

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

bool TryRead(char &c) {
  if (Received()) {
    c = static_cast<char>(Read8(kCOM1));
    return true;
  }
  return false;
}

bool TryWrite(char c) {
  if (IsTransmitEmpty()) {
    Write8(kCOM1, static_cast<uint8_t>(c));
    return true;
  }
  return false;
}

char AtomicRead() {
  while (!Received()) {}
  return static_cast<char>(Read8(kCOM1));
}

void AtomicPut(char c) {
  while (!IsTransmitEmpty()) {}
  Write8(kCOM1, static_cast<uint8_t>(c));
}

void AtomicWrite(const char *str) {
  for (size_t size = strlen(str), i = 0; i < size; ++i) AtomicPut(str[i]);
}

}  // namespace serial
