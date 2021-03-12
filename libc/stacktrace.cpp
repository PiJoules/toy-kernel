#include <print.h>
#include <stacktrace.h>

#ifdef KERNEL

#include <kernel.h>
#define PRINT DebugPrint

#else

#include <stdio.h>
#define PRINT PrintStdout

namespace {

void PrintStdout(const char *data) { print::Print(put, data); }

template <typename T1>
void PrintStdout(const char *data, T1 val) {
  print::Print(put, data, val);
}

template <typename T1, typename... Rest>
void PrintStdout(const char *data, T1 val, Rest... rest) {
  print::Print(put, data, val, rest...);
}

}  // namespace
#endif

namespace stacktrace {

namespace {

struct StackFrame {
  struct StackFrame *ebp;
  uintptr_t eip;
};

}  // namespace

void PrintStackTrace() {
  StackFrame *stack;
  asm volatile("mov %%ebp, %0" : "=r"(stack));
  PRINT("Stack trace:\n");
  for (size_t frame = 0; stack; ++frame) {
    PRINT("{}) {}\n", frame, print::Hex(stack->eip));
    stack = stack->ebp;
  }
}

}  // namespace stacktrace
