// High level interrupt service routines and interrupt request handlers.
// Part of this code is modified from Bran's kernel development tutorials.
// Rewritten for JamesM's kernel development tutorials.

#include <Terminal.h>
#include <io.h>
#include <isr.h>
#include <kthread.h>
#include <paging.h>
#include <panic.h>

using terminal::Hex;

isr_t interrupt_handlers[256];

void DumpRegisters(const registers_t *regs) {
  if (regs->int_no == 0xd) {
    terminal::Write("General protection fault\n");
    if (auto err = regs->err_code) {
      if (err & 1)
        terminal::Write("Exception originated externally to the processor\n");

      auto table = (err >> 1) & 0x3;
      if (table == 0)
        terminal::Write("Occurred within GDT segment ");
      else if (table == 1 || table == 3)
        terminal::Write("Occurred within IDT segment ");
      else if (table == 2)
        terminal::Write("Occurred within LDT segment ");

      auto index = (err >> 3) & 0x1FFF;
      terminal::WriteF("{}\n", terminal::Hex(index));
    }
  }

  bool user = GetCurrentThread()->isUserThread();

  terminal::Write("received interrupt in ");
  if (GetCurrentThread() == GetMainKernelThread()) {
    terminal::Write("main kernel thread");
  } else if (user) {
    terminal::WriteF("user thread {}", GetCurrentThread()->id);
  } else {
    terminal::WriteF("kernel thread {}", GetCurrentThread()->id);
  }
  terminal::WriteF(": {}\n", Hex(regs->int_no));

  terminal::WriteF("ds:  {} edi: {} esi: {}\n", Hex(regs->ds), Hex(regs->edi),
                   Hex(regs->esi));
  terminal::WriteF("ebp: {} esp: {} ebx: {}\n", Hex(regs->ebp), Hex(regs->esp),
                   Hex(regs->ebx));
  terminal::WriteF("edx: {} ecx: {} eax: {}\n", Hex(regs->edx), Hex(regs->ecx),
                   Hex(regs->eax));
  terminal::WriteF("error code: {}\n", Hex(regs->err_code));
  terminal::WriteF("eip: {}\n", Hex(regs->eip));
  terminal::WriteF("cs: {}\n", Hex(regs->cs));
  terminal::WriteF("eflags: {}\n", Hex(regs->eflags));
  terminal::WriteF("useresp: {}\n", Hex(regs->useresp));
  terminal::WriteF("ss: {}\n", Hex(regs->ss));

  // Dump the stack
  uint32_t *esp = (uint32_t *)regs->esp;
  terminal::Write("Stack dump:\n");
  constexpr int dump_size = 28;
  assert(dump_size % 4 == 0 && "The stack dump size must be a multiple of 4");
  for (int i = dump_size; i >= 0; i -= 4) {
    uint32_t *ptr = esp + i;
    terminal::WriteF("{}: {} {} {} {}\n", ptr, Hex(*ptr), Hex(ptr[1]),
                     Hex(ptr[2]), Hex(ptr[3]));
  }
}

// TODO: To make sure we don't accidentally use an interrupt we previously
// assigned, add an assert here to check the interrupt isn't taken. Then make a
// separate function for replacing interrupts.
void RegisterInterruptHandler(uint8_t n, isr_t handler) {
  interrupt_handlers[n] = handler;
}

void UnregisterInterruptHandler(uint8_t n) { interrupt_handlers[n] = nullptr; }

isr_t GetInterruptHandler(uint8_t interrupt) {
  return interrupt_handlers[interrupt];
}

extern "C" {

// This gets called from our ASM interrupt handler stub.
void isr_handler(registers_t *regs) {
  if (isr_t handler = interrupt_handlers[regs->int_no]) {
    return handler(regs);
  }
  DumpRegisters(regs);
  PANIC("Unhandled interrupt!");
}

void irq_handler(registers_t *regs) {
  // Send an EOI (end of interrupt) signal to the PICs.
  // If this interrupt involved the slave.
  if (regs->int_no >= 40) {
    // Send reset signal to slave.
    Write8(0xA0, 0x20);
  }
  // Send reset signal to master. (As well as slave, if necessary).
  Write8(0x20, 0x20);

  if (interrupt_handlers[regs->int_no]) {
    isr_t handler = interrupt_handlers[regs->int_no];
    return handler(regs);
  }
  DumpRegisters(regs);
  PANIC("Unhandled interrupt!");
}
}
