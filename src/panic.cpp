#include <Terminal.h>
#include <kernel.h>
#include <panic.h>

void Panic(const char *msg, const char *file, int line) {
  // We encountered a massive problem and have to stop.
  asm volatile("cli");  // Disable interrupts.

  terminal::Write("PANIC(");
  terminal::Write(msg);
  terminal::Write(") at ");
  terminal::Write(file);
  terminal::Write(":");
  terminal::WriteF("{}", line);
  terminal::Write("\n");
  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
}
