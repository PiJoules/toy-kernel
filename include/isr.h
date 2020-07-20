#ifndef ISR_H_
#define ISR_H_

#include <kstdint.h>

struct registers_t {
  uint32_t ds;                                      // Data segment selector
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Pushed by pusha.
  uint32_t int_no, err_code;  // Interrupt number and error code (if applicable)

  // Pushed by the processor automatically.
  // If the iret is inter-privilege, then iret only pops EIP, CS, and EFLAGS
  // from the stack. Otherwise for intra-privilege, iret additionally pops the
  // stack pointer and SS.
  uint32_t eip, cs, eflags, useresp, ss;
};
static_assert(sizeof(registers_t) == 64, "Size of registers_t changed");

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

// Enables registration of callbacks for interrupts or IRQs.
// For IRQs, to ease confusion, use the #defines above as the
// first parameter.
using isr_t = void (*)(registers_t *);
void RegisterInterruptHandler(uint8_t interrupt, isr_t handler);
void UnregisterInterruptHandler(uint8_t interrupt);
isr_t GetInterruptHandler(uint8_t interrupt);
void DumpRegisters(registers_t *regs);

#endif
