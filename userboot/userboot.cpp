#include <_syscalls.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern bool __use_debug_log;

constexpr char CR = 13;  // Carriage return

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
void DebugRead(char *buffer) {
  while (1) {
    char c = sys::DebugRead();
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

static void DumpCommands() {
  printf(
      "help - Dump commands.\n"
      "shutdown - Exit userboot.\n");
}

extern "C" int main() {
  __use_debug_log = true;

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
