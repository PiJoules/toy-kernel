#include <kernel.h>
#include <panic.h>
#include <stacktrace.h>

void Panic(const char *msg, const char *file, int line) {
  // We encountered a massive problem and have to stop.
  asm volatile("cli");  // Disable interrupts.

  DebugPrint("PANIC({}) at {}:{}\n", msg, file, line);

  stacktrace::PrintStackTrace();

  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
}
