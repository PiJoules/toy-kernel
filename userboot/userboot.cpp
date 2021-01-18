#include <_syscalls.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern bool __use_debug_log;

namespace {

constexpr char CR = 13;  // Carriage return

char GetChar() {
  char c;
  while (!sys::DebugRead(c)) {}
  return c;
}

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
// FIXME: Getting characters works by making a syscall to the kernel which waits
// for a new character from serial.
void DebugRead(char *buffer) {
  while (1) {
    char c = GetChar();
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

void DumpCommands() {
  printf(
      "help - Dump commands.\n"
      "shutdown - Exit userboot.\n");
}

}  // namespace

extern "C" int __user_main(void *arg) {
  __use_debug_log = true;

  // This is the dummy argument we expect from the kernel.
  printf("arg: %p\n", arg);
  assert((uint32_t)arg == 0xfeed);

  printf("\nWelcome! Type \"help\" for commands\n");

  char buffer[kCmdBufferSize];
  while (1) {
    printf("shell> ");
    DebugRead(buffer);

    if (strcmp(buffer, "help") == 0) {
      DumpCommands();
    } else if (strcmp(buffer, "shutdown") == 0) {
      printf("Shutting down...\n");
      break;
    } else {
      printf("Unknown command: %s\n", buffer);
    }
  }

  return 0;
}
