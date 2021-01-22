#ifndef KERNEL_H_
#define KERNEL_H_

/**
 * This header contains miscellanious things that can be used anywhere in the
 * kernel.
 */

#include <print.h>
#include <serial.h>
#include <stdint.h>

#define __STR(s) STR(s)
#define STR(s) #s

inline void DisableInterrupts() { asm volatile("cli"); }
inline void EnableInterrupts() { asm volatile("sti"); }
inline bool InterruptsAreEnabled() {
  uint32_t eflags;
  asm volatile(
      "\
    pushf;\
    pop %0"
      : "=r"(eflags));
  return eflags & 0x200;
}

/**
 * RAII for disabling interrupts in a scope, then re-enabling them after exiting
 * the scope only if they were already enabled at the start.
 */
class DisableInterruptsRAII {
 public:
  DisableInterruptsRAII() : interrupts_enabled_(InterruptsAreEnabled()) {
    DisableInterrupts();
  }
  ~DisableInterruptsRAII() {
    if (interrupts_enabled_) EnableInterrupts();
  }

 private:
  bool interrupts_enabled_;
};

// We must clear interrupt flags, otherwise we could hit an interrupt that
// triggers on the infinite loop if interrupts were previously enabled.
#define LOOP_INDEFINITELY() \
  DisableInterrupts();      \
  while (1) {}

#ifdef __clang__
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE()
#endif

/**
 * Printing to serial.
 */
template <typename... Rest>
void DebugPrint(const char *str, Rest... rest) {
  print::Print(serial::AtomicPut, str, rest...);
}

#endif
