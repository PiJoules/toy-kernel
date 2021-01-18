#include <assert.h>
#include <stacktrace.h>

#ifdef KERNEL
#include <kernel.h>
#else
#include <_syscalls.h>
#include <print.h>
#include <stdio.h>
#endif

void __assert(bool condition, const char *msg, const char *filename, int line,
              const char *pretty_func) {
  if (condition) return;

#ifdef KERNEL
  DisableInterrupts();
  DebugPrint("\n{}:{}: {}: Assertion `{}` failed.\nAborted", filename, line,
             pretty_func, msg);

  stacktrace::PrintStackTrace();

  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
#else
  print::Print(put, "\n{}:{}: {}: Assertion `{}` failed.\nAborted", filename,
               line, pretty_func, msg);

  stacktrace::PrintStackTrace();

  // Exit this user task.
  sys::ExitTask();
#endif
}
