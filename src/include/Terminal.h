#ifndef TERMINAL_H_
#define TERMINAL_H_

#include <Multiboot.h>
#include <kstdint.h>
#include <kstring.h>
#include <ktype_traits.h>

// A terminal that operates undef VGA text mode 3 (see
// https://wiki.osdev.org/Text_UI#Video_Mode).

namespace terminal {

/**
 * If selected, printing from the kernel will show on a 80x25 textual display.
 *
 * If `serial` is true, characters that normally would be printed on the
 * terminal will also be written to COM1, which can be stored in a file on QEMU.
 */
void UseTextTerminal(bool serial);

void UseTextTerminalVirtual();

/**
 * If selected, printing from the kernel will show on a graphical display. The
 * dimensions and other attributes of this display are determined from the
 * multiboot struct.
 *
 * This allows writes to the physical address specified by
 * Multiboot::framebuffer_addr. When paging is enabled,
 * UseGraphicsTerminalVirtual() should be called to redirect writes to a virtual
 * address rather than the physical address which could be outside the range
 * that we allocated pages for.
 */
void UseGraphicsTerminalPhysical(const Multiboot*, bool serial);

/**
 * If using graphics mode, this should be called after initializing paging to
 * redirect writes to a virtual address rather than the physical address
 * returned by multiboot.
 */
void UseGraphicsTerminalVirtual();

bool UsingGraphics();

/* Hardware text mode color constants. */
enum vga_color : uint8_t {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};

void Clear();
void EnableCursor(uint8_t cursor_start, uint8_t cursor_end);
void DisableCursor();
void SetColor(uint8_t color);
void Write(const char* data, size_t size);
void Write(const char* data);
void Put(char c);
uint16_t GetNumRows();
uint16_t GetNumCols();

/**
 * This type represents a callback that is ultimately used when printing a
 * character. This callback exists for the purpose of handling newlines and
 * scrolling when appropriate.
 */
using PutFunc = void (*)(char c);

/**
 * Call `put` on each character in `str`.
 */
void Write(PutFunc put, const char* str);

/**
 * PrintFormatter is a helper class used to specify how a type should be
 * printed. For a custom type, T, just define a new PrintForatter:
 *
 *   template <>
 *   void PrintFormatter(PutFunc put, T val) {...}
 */
template <typename T1, toy::enable_if_t<!toy::is_pointer<T1>::value, int> = 0>
void PrintFormatter(PutFunc put, T1 val);

/**
 * Hex is a wrapper class for integral values that would like to be printed in
 * hexadecimal notation.
 *
 *   using terminal::Hex;
 *   terminal::WriteF("Hex value: {}\n", Hex(1));
 */
template <typename T, toy::enable_if_t<toy::is_integral<T>::value, int> = 0>
struct Hex {
  Hex(T val) : val(val) {}
  T val;
};

template <typename T>
struct is_string
    : public toy::disjunction<
          toy::is_same<char*, typename toy::decay<T>::type>,
          toy::is_same<const char*, typename toy::decay<T>::type> > {};

template <typename T1,
          toy::enable_if_t<toy::is_pointer<T1>::value && !is_string<T1>::value,
                           int> = 0>
void PrintFormatter(PutFunc put, T1 val) {
  PrintFormatter(put, Hex(reinterpret_cast<uintptr_t>(val)));
}

template <typename T1, toy::enable_if_t<is_string<T1>::value, int> = 0>
void PrintFormatter(PutFunc put, T1 val);

template <typename T1>
void WriteF(PutFunc put, const char* data, T1 val) {
  char c;
  while ((c = *data) != '\0') {
    switch (c) {
      case '{':
        PrintFormatter(put, val);
        ++data;
        assert(*data == '}' && "Missing closing '}'");
        break;
      default:
        put(c);
    }
    ++data;
  }
}

template <typename T1, typename... Rest>
void WriteF(PutFunc put, const char* data, T1 val, Rest... rest) {
  char c;
  while ((c = *data) != '\0') {
    switch (c) {
      case '{':
        PrintFormatter(put, val);
        ++data;
        assert(*data == '}' && "Missing closing '}'");
        return WriteF(put, ++data, rest...);
      default:
        put(c);
    }
    ++data;
  }
}

template <typename T1>
void WriteF(const char* data, T1 val) {
  WriteF(Put, data, val);
}

template <typename T1, typename... Rest>
void WriteF(const char* data, T1 val, Rest... rest) {
  WriteF(Put, data, val, rest...);
}

}  // namespace terminal

#endif
