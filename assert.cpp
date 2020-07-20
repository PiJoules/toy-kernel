#include <Terminal.h>
#include <kassert.h>
#include <kernel.h>

// /tmp/test.cc:4: int main(): Assertion `0' failed.
// Aborted
void __assert(bool condition, const char *msg, const char *filename, int line,
              const char *pretty_func) {
  if (condition) return;

  asm volatile("cli");  // Disable interrupts.
  terminal::WriteF("{}:{}: {}: Assertion `{}` failed.\nAborted", filename, line,
                   pretty_func, msg);

  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
}
