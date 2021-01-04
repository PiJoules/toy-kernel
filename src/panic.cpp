#include <kernel.h>
#include <panic.h>

void Panic(const char *msg, const char *file, int line) {
  // We encountered a massive problem and have to stop.
  asm volatile("cli");  // Disable interrupts.

  DebugPrint("PANIC(");
  DebugPrint(msg);
  DebugPrint(") at ");
  DebugPrint(file);
  DebugPrint(":");
  DebugPrint("{}", line);
  DebugPrint("\n");
  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
}
