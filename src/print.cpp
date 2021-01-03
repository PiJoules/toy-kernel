#include <print.h>

namespace print {

namespace {

// NOTE: The PrintXXX functions do not print any prefixes like '0x' for hex.
// Assumes 0 <= val < 16
void PrintNibble(PutFunc put, uint8_t val) {
  if (val < 10)
    put(static_cast<char>('0' + val));
  else
    put(static_cast<char>('a' + val - 10));
}

// The PrintHex functions print the 2s complement representation of signed
// integers.
void PrintHex(PutFunc put, uint8_t val) {
  PrintNibble(put, val >> 4);
  PrintNibble(put, val & 0xf);
}

void PrintHex(PutFunc put, uint16_t val) {
  PrintHex(put, static_cast<uint8_t>(val >> 8));
  PrintHex(put, static_cast<uint8_t>(val));
}

void PrintHex(PutFunc put, uint32_t val) {
  PrintHex(put, static_cast<uint16_t>(val >> 16));
  PrintHex(put, static_cast<uint16_t>(val));
}

void PrintHex(PutFunc put, uint64_t val) {
  PrintHex(put, static_cast<uint32_t>(val >> 32));
  PrintHex(put, static_cast<uint32_t>(val));
}

void PrintHex(PutFunc put, int32_t val) {
  PrintHex(put, static_cast<uint32_t>(val));
}

template <typename IntTy, size_t NumDigits>
void PrintDecimalImpl(PutFunc put, IntTy val) {
  constexpr uint32_t kBase = 10;
  char buffer[NumDigits + 1];  // 10 + 1 for the null terminator
  buffer[NumDigits] = '\0';
  char *buffer_ptr = &buffer[NumDigits];
  do {
    --buffer_ptr;
    *buffer_ptr = (val % kBase) + '0';
    val /= kBase;
  } while (val);
  Print(put, buffer_ptr);
}

void PrintDecimal(PutFunc put, uint32_t val) {
  // The largest value for this type is 4294967295 which will require 10
  // characters to print.
  return PrintDecimalImpl<uint32_t, 10>(put, val);
}

void PrintDecimal(PutFunc put, uint64_t val) {
  // The largest value for this type is 18446744073709551615 which will require
  // 20 characters to print.
  return PrintDecimalImpl<uint64_t, 20>(put, val);
}

void PrintDecimal(PutFunc put, int32_t val) {
  if (val >= 0) return PrintDecimal(put, static_cast<uint32_t>(val));
  if (val == INT32_MIN) return Print(put, "-2147483648");

  // We can safely negate without overflow.
  put('-');
  PrintDecimal(put, static_cast<uint32_t>(-val));
}

}  // namespace

void Print(print::PutFunc put, const char *str) {
  while (*str) {
    put(*str);
    ++str;
  }
}

template <>
void PrintFormatter(PutFunc put, char c) {
  put(c);
}

template <>
void PrintFormatter(PutFunc put, uint8_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, uint16_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, uint32_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, uint64_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, unsigned long val) {
  static_assert(sizeof(unsigned long) == 4, "");
  PrintDecimal(put, static_cast<uint32_t>(val));
}

template <>
void PrintFormatter(PutFunc put, int8_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, int16_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, int32_t val) {
  PrintDecimal(put, val);
}

template <>
void PrintFormatter(PutFunc put, Hex<uint8_t> hex) {
  put('0');
  put('x');
  PrintHex(put, hex.val);
}

template <>
void PrintFormatter(PutFunc put, Hex<uint16_t> hex) {
  put('0');
  put('x');
  PrintHex(put, hex.val);
}

template <>
void PrintFormatter(PutFunc put, Hex<uint32_t> hex) {
  put('0');
  put('x');
  PrintHex(put, hex.val);
}

template <>
void PrintFormatter(PutFunc put, Hex<uint64_t> hex) {
  put('0');
  put('x');
  PrintHex(put, hex.val);
}

template <>
void PrintFormatter(PutFunc put, Hex<int32_t> hex) {
  put('0');
  put('x');
  PrintHex(put, hex.val);
}

template <>
void PrintFormatter(PutFunc put, const char *str) {
  while (*str != '\0') {
    put(*str);
    ++str;
  }
}

template <>
void PrintFormatter(PutFunc put, char *str) {
  while (*str != '\0') {
    put(*str);
    ++str;
  }
}

template <>
void PrintFormatter(PutFunc put, decltype(nullptr)) {
  Print(put, "<nullptr>");
}

template <>
void PrintFormatter(PutFunc put, bool b) {
  Print(put, b ? "true" : "false");
}

}  // namespace print
