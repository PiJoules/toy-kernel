#include <Terminal.h>
#include <print.h>
#include <stacktrace.h>

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
  terminal::Write("Stack trace:\n");
  for (size_t frame = 0; stack; ++frame) {
    terminal::WriteF("{}) {}\n", frame, print::Hex(stack->eip));
    stack = stack->ebp;
  }
}

}  // namespace stacktrace
