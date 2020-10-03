#include <Multiboot.h>
#include <Terminal.h>
#include <io.h>
#include <kassert.h>
#include <kctype.h>
#include <kstdint.h>
#include <paging.h>
#include <serial.h>

extern "C" char _binary_font_psf_start;

namespace terminal {

namespace {

struct Terminal {
 public:
  /**
   * The PutAt function will move the cursor to a given row and column, then
   * write the character there, then advance the cursor.
   */
  using PutAtFunc = void (*)(char c, uint16_t row, uint16_t col);
  using MoveCursorFunc = void (*)(uint16_t row, uint16_t col);

  void Init(PutAtFunc putat, MoveCursorFunc movecursor, uint16_t numrows,
            uint16_t numcols, bool serial) {
    assert(!isInitialized() && "Already set up the terminal");
    putat_ = putat;
    movecursor_ = movecursor;
    numrows_ = numrows;
    numcols_ = numcols;
    serial_ = serial;

    if (serial) serial::Initialize();
  }

  bool isInitialized() const { return putat_ && movecursor_; }

  void Clear() {
    for (uint8_t row = 0; row < numrows_; ++row) {
      for (uint8_t col = 0; col < numcols_; ++col) { putat_(' ', row, col); }
    }

    // Move back to the top left.
    movecursor_(0, 0);
  }

  void Put(char c) {
    putat_(c, row_, col_);
    if (serial_) serial::Write(c);
  }

  uint16_t getNumRows() const { return numrows_; }
  uint16_t getNumCols() const { return numcols_; }
  uint16_t &getRow() { return row_; }
  uint16_t &getCol() { return col_; }

 private:
  PutAtFunc putat_ = nullptr;
  MoveCursorFunc movecursor_ = nullptr;
  uint16_t numrows_, numcols_;
  uint16_t row_ = 0, col_ = 0;
  bool serial_ = false;
};

Terminal kTerminal;

uint16_t &GetRow() { return kTerminal.getRow(); }
uint16_t &GetCol() { return kTerminal.getCol(); }

namespace text {

constexpr uint8_t kVgaWidth = 80;
constexpr uint8_t kVgaHeight = 25;

constexpr uint8_t VgaEntryColor(vga_color fg, vga_color bg) {
  return static_cast<uint8_t>(fg | (bg << 4));
}

constexpr uint16_t VgaEntry(char c, uint8_t color) {
  return static_cast<uint16_t>(static_cast<unsigned char>(c) | (color << 8));
}

uint8_t Color = VgaEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
uint16_t *Buffer = reinterpret_cast<uint16_t *>(0xB8000);

uint16_t GetCursorLoc(uint16_t row, uint16_t col) {
  return row * GetNumCols() + col;
}

uint16_t GetCursorLoc() { return GetCursorLoc(GetRow(), GetCol()); }

void Scroll() {
  for (unsigned row = 0; row < (kVgaHeight - 1) * kVgaWidth; row += kVgaWidth)
    memcpy(&Buffer[row], &Buffer[row + kVgaWidth], sizeof(*Buffer) * kVgaWidth);

  uint8_t attribute = VgaEntryColor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
  uint16_t blank = VgaEntry(' ', attribute);
  for (int i = (kVgaHeight - 1) * kVgaWidth; i < kVgaHeight * kVgaWidth; ++i)
    Buffer[i] = blank;

  GetRow() = kVgaHeight - 1;
}

void HandleNewline() {
  GetCol() = 0;
  if (++GetRow() == GetNumRows()) Scroll();
}

void MoveCursor(uint16_t row, uint16_t col) {
  GetRow() = row;
  GetCol() = col;

  uint16_t cursor_loc = GetCursorLoc(GetRow(), GetCol());
  Write8(0x3D4, 14);  // Set the high cursor byte.
  Write8(0x3D5, static_cast<uint8_t>(cursor_loc >> 8));
  Write8(0x3D4, 15);  // Set the low cursor byte.
  Write8(0x3D5, static_cast<uint8_t>(cursor_loc));
}

void PutAt(char c, uint16_t row, uint16_t col) {
  GetRow() = row;
  GetCol() = col;

  uint16_t index = GetCursorLoc();
  if (c != '\n') {
    Buffer[index] = VgaEntry(c, Color);
    if (++GetCol() == kVgaWidth) HandleNewline();
  } else {
    HandleNewline();
  }
  MoveCursor(static_cast<uint8_t>(GetRow()), static_cast<uint8_t>(GetCol()));
}

}  // namespace text

namespace graphics {

// Pitch - # of bytes of VRAM that should be skipped to move down 1 pixel.
uint32_t PixelWidth, PixelHeight, Pitch;
uint32_t *GFXBuffer;
bool UsingGraphics = false;

// FIXME: We probably shouldn't hardcode these here. This could change
// if we want different fonts.
constexpr uint16_t kLinePixelHeight = 16;
constexpr uint16_t kLinePixelWidth = 8;
constexpr uint32_t kWhite = UINT32_MAX;
constexpr uint32_t kBlack = 0;

struct PSF_font {
  uint32_t magic;         /* magic bytes to identify PSF */
  uint32_t version;       /* zero */
  uint32_t headersize;    /* offset of bitmaps in file, 32 */
  uint32_t flags;         /* 0 if there's no unicode table */
  uint32_t numglyph;      /* number of glyphs */
  uint32_t bytesperglyph; /* size of each glyph */
  uint32_t height;        /* height in pixels */
  uint32_t width;         /* width in pixels */

  // Return how many bytes encode one row.
  uint32_t BytesPerLine() const { return (width + 7) / 8; }

  // Get the glyph for the character. If there's no
  // glyph for a given character, we'll return the first glyph.
  const uint8_t *getGlyph(char c) const {
    return reinterpret_cast<const uint8_t *>(this) + headersize +
           (c > 0 && c < numglyph ? c : 0) * bytesperglyph;
  }
};
static_assert(
    sizeof(PSF_font) == 32,
    "PSF_font struct size changed! Make sure this is what's actually desired.");

const auto *kFont = reinterpret_cast<PSF_font *>(&_binary_font_psf_start);

uint32_t GFXCharOffset(uint32_t row, uint32_t col) {
  return (row * kFont->height * Pitch) + (col * (kFont->width + 1) * 4);
}

/**
 * Get the address corresponding to a character row on the frame buffer.
 */
uint8_t *GFXGetPixelRow(uint16_t row) {
  return reinterpret_cast<uint8_t *>(GFXBuffer) + GFXCharOffset(row, 0);
}

/**
 * This is a copy of code from the PC Screen Font example on OSDev. See
 * https://wiki.osdev.org/PC_Screen_Font.
 * Cursor position on screen, is in characters not in pixels.
 */
void GFXPutCharAt(uint32_t *framebuffer, char c, uint16_t row, uint16_t col,
                  uint32_t fg, uint32_t bg) {
  assert(isprint(c) && "Non-prontable character");
  const uint8_t *glyph = kFont->getGlyph(c);
  /* calculate the upper left corner on screen where we want to display.
     we only do this once, and adjust the offset later. This is faster. */
  auto offs = GFXCharOffset(row, col);
  /* finally display pixels according to the bitmap */
  for (uint32_t row = 0; row < kFont->height; ++row) {
    // save the starting position of the line
    uint32_t line = offs;
    uint32_t mask = UINT32_C(1) << (kFont->width - 1);

    // display a row
    for (uint32_t col = 0; col < kFont->width; ++col) {
      auto *pos = reinterpret_cast<uint32_t *>(
          reinterpret_cast<uint8_t *>(framebuffer) + line);
      if (c == 0) {
        *pos = bg;
      } else {
        *pos = (*glyph & mask) ? fg : bg;
      }

      /* adjust to the next pixel */
      mask >>= 1;
      line += 4;
    }

    /* adjust to the next line */
    glyph += kFont->BytesPerLine();
    offs += Pitch;
  }
}

void GFXFill(uint32_t color) {
  for (int row = 0; row < PixelHeight; ++row) {
    for (int col = 0; col < PixelWidth; ++col) {
      // TODO: We explicitly set the bits per pixel to 32 in grub and the
      // bootloader, but we should probably have some way to check the BPP or
      // assert after printing is setup that the BPP is 32.
      GFXBuffer[col + row * PixelWidth] = color;
    }
  }
}

void MoveCursor(uint16_t row, uint16_t col) {
  GetRow() = row;
  GetCol() = col;
}

void Scroll() {
  uint16_t height = GetNumRows();
  uint16_t width = GetNumCols();

  // Copy every nth row onto the (n-1)th row.
  for (uint16_t row = 0; row < (height - 1); ++row) {
    auto *line = GFXGetPixelRow(row);
    auto *nextline = GFXGetPixelRow(row + 1);
    memcpy(line, nextline, nextline - line);
  }

  // Clear the last line.
  for (uint16_t col = 0; col < width; ++col) {
    GFXPutCharAt(GFXBuffer, ' ', height - 1, col, kBlack, kWhite);
  }

  GetRow() = static_cast<uint16_t>(height - 1);
}

void HandleNewline() {
  GetCol() = 0;
  if (++GetRow() >= GetNumRows()) Scroll();
}

void PutAt(char c, uint16_t row, uint16_t col) {
  GetRow() = row;
  GetCol() = col;

  if (c != '\n') {
    GFXPutCharAt(GFXBuffer, c, GetRow(), GetCol(), kBlack, kWhite);
    if (++GetCol() >= GetNumCols()) HandleNewline();
  } else {
    HandleNewline();
  }
  // TODO: Maybe add a blinking cursor?
}

}  // namespace graphics

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
  Write(put, buffer_ptr);
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
  if (val == INT32_MIN) return Write(put, "-2147483648");

  // We can safely negate without overflow.
  put('-');
  PrintDecimal(put, static_cast<uint32_t>(-val));
}

}  // namespace

void UseTextTerminal(bool serial) {
  kTerminal.Init(text::PutAt, text::MoveCursor, text::kVgaHeight,
                 text::kVgaWidth, serial);
}

bool UsingGraphics() { return graphics::UsingGraphics; }

void UseGraphicsTerminalPhysical(const Multiboot *multiboot, bool serial) {
  graphics::GFXBuffer =
      reinterpret_cast<uint32_t *>(multiboot->framebuffer_addr);

  graphics::PixelWidth = multiboot->framebuffer_width;
  graphics::PixelHeight = multiboot->framebuffer_height;
  graphics::Pitch = multiboot->framebuffer_pitch;

  // Make entire background white.
  graphics::GFXFill(graphics::kWhite);

  kTerminal.Init(
      graphics::PutAt, graphics::MoveCursor,
      static_cast<uint16_t>(graphics::PixelHeight / graphics::kLinePixelHeight),
      static_cast<uint16_t>(graphics::PixelWidth / graphics::kLinePixelWidth),
      serial);

  graphics::UsingGraphics = true;

  // TODO: Right now, we only support this type of font, but if we want to
  // support other fonts, these should not be hardcoded in.
  assert(graphics::kFont->height == 16 && "Font height changed!");
  assert(graphics::kFont->width == 8 && "Font width changed!");
}

// NOTE: We should be very careful to not print here because we just enabled
// paging, but still have not mapped the virtual frame buffer to a physical
// address. Printing before adding the page and reassigning the buffer will lead
// to a page fault, BUT we will not be able to normally see a dump and freeze.
// Instead, it is likely the computer will just crash.
void UseGraphicsTerminalVirtual() {
  GetKernelPageDirectory().AddPage((char *)GFX_MEMORY_START,
                                   (char *)graphics::GFXBuffer,
                                   /*flags=*/0);
  graphics::GFXBuffer = reinterpret_cast<uint32_t *>(GFX_MEMORY_START);
}

void UseTextTerminalVirtual() {
  // Just identity map the first page.
  GetKernelPageDirectory().AddPage(
      (char *)nullptr, (char *)PageAddr4M(PageIndex4M(text::Buffer)),
      /*flags=*/0);
}

uint16_t GetNumRows() { return kTerminal.getNumRows(); }
uint16_t GetNumCols() { return kTerminal.getNumCols(); }

void Put(char c) { kTerminal.Put(c); }

void Write(PutFunc put, const char *str) {
  while (*str) {
    put(*str);
    ++str;
  }
}

void Clear() { kTerminal.Clear(); }

void Write(const char *data, size_t size) {
  for (size_t i = 0; i < size; i++) Put(data[i]);
}

void Write(const char *data) { Write(data, strlen(data)); }

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
  Write(put, "<nullptr>");
}

}  // namespace terminal
