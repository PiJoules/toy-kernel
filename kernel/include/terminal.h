#ifndef TERMINAL_H_
#define TERMINAL_H_

/**
 * FIXME: This should be moved into user code and not remain in kernel code.
 */

#include <Multiboot.h>
#include <kstring.h>
#include <print.h>
#include <stdint.h>
#include <type_traits.h>

// A terminal that operates undef VGA text mode 3 (see
// https://wiki.osdev.org/Text_UI#Video_Mode).

namespace terminal {

/**
 * If selected, printing from the kernel will show on a 80x25 textual display.
 *
 * If `serial` is true, characters that normally would be printed on the
 * terminal will also be written to COM1, which can be stored in a file on QEMU.
 */
void UseTextTerminal();

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
void UseGraphicsTerminalPhysical(const Multiboot*);

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

template <typename T1>
void WriteF(const char* data, T1 val) {
  print::Print(Put, data, val);
}

template <typename T1, typename... Rest>
void WriteF(const char* data, T1 val, Rest... rest) {
  print::Print(Put, data, val, rest...);
}

}  // namespace terminal

#endif
