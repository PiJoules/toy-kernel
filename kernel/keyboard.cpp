/**
 * FIXME: This should be moved into user code and not remain in kernel code.
 */

#include <assert.h>
#include <io.h>
#include <isr.h>
#include <kernel.h>
#include <keyboard.h>
#include <panic.h>
#include <print.h>
#include <serial.h>
#include <type.h>

namespace {

// These do not correspond to any ascii character.
enum KeyAction {
  NOACTION = 0,
#define KEY_ACTION(name, val) name = val,
#include "KeyActions.def"
};
KeyAction PreviousAction = NOACTION;

#define NOCHAR "\0"

// Scan code set 1 (https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Set_1)
// NOTE: Only add characters that have ascii equivalents here. Scancodes that
// don't have ascii equivalents should instead be added as a stringified macro
// and a slot in the KeyActions enum.
constexpr char kKeyPresses[] =
    // 0x00
    NOCHAR /*Escape*/
    "\x1b"
    "1234567890"

    // 0x0C
    "-="   /*Backspace*/
    "\x08" /*tab*/
    "\t"

    // 0x10
    "qwertyuiop[]" NOCHAR NOCHAR

    // 0x1E
    "asdfghjkl;'`" NOCHAR
    "\\"

    // 0x2C
    "zxcvbnm,./" NOCHAR "*"

    // 0x38
    ;
constexpr size_t kNumKeyPresses = sizeof(kKeyPresses) / sizeof(char);

char ShiftedKey(char pressed_key) {
  if (islower(pressed_key)) return static_cast<char>(toupper(pressed_key));

  switch (pressed_key) {
    case '`':
      return '~';
    case '1':
      return '!';
    case '2':
      return '@';
    case '3':
      return '#';
    case '4':
      return '$';
    case '5':
      return '%';
    case '6':
      return '^';
    case '7':
      return '&';
    case '8':
      return '*';
    case '9':
      return '(';
    case '0':
      return ')';
    case '-':
      return '_';
    case '=':
      return '+';
    case '[':
      return '{';
    case ']':
      return '}';
    case '\\':
      return '|';
    case ';':
      return ':';
    case '\'':
      return '"';
    case ',':
      return '<';
    case '.':
      return '>';
    case '/':
      return '?';
  }
  return pressed_key;
}

void KeyboardCallback([[maybe_unused]] X86Registers *regs) {
  uint8_t scancode = Read8(0x60);

  // TODO: Handle other pressed codes >= 0xE0.
  if (scancode < kNumKeyPresses) {
    // Key was pressed.
    switch (scancode) {
      case ENTER:
        serial::Put('\n');
        PreviousAction = NOACTION;
        return;
      case LCTRL:
      case LSHIFT:
      case RSHIFT:
        // We will handle this action in the followup key.
        PreviousAction = static_cast<KeyAction>(scancode);
        return;
    }

    char pressed_key = kKeyPresses[scancode];
    if (pressed_key == NOCHAR[0])
      // TODO: Once we actually implement mappings for all scancodes, this
      // should be replaced with an assert.
      return DebugPrint(
          "WARNING: Found an unmapped scancode that doesn't have an ascii "
          "character: {}\n",
          print::Hex(scancode));

    switch (PreviousAction) {
      case NOACTION:
      case LCTRL:
        break;
      case ENTER:
        PANIC("Should've already handled ENTER key.");
        break;
      case LSHIFT:
      case RSHIFT:
        pressed_key = ShiftedKey(pressed_key);
        break;
    }
    PreviousAction = NOACTION;

    return serial::Put(pressed_key);
  } else if (scancode < 0xE0) {
    // Key was released.
    return;
  }

  // TODO: Once we actually implement mappings for all scancodes, this
  // should be replaced with an assert.
  DebugPrint("WARNING: Unhandled scancode {}\n", print::Hex(scancode));
}

}  // namespace

void InitializeKeyboard() { RegisterInterruptHandler(IRQ1, &KeyboardCallback); }
