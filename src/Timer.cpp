#include <Terminal.h>
#include <Timer.h>
#include <io.h>
#include <isr.h>
#include <kernel.h>
#include <kstdint.h>
#include <kthread.h>

uint32_t tick = 0;

namespace {

void TimerCallback(registers_t *regs) {
  ++tick;
  schedule(regs);
}

}  // namespace

void InitTimer(uint32_t frequency) {
  // Register our timer callback.
  RegisterInterruptHandler(IRQ0, &TimerCallback);

  // The value we send to the PIT is the value to divide it's input clock
  // (1193180 Hz) by, to get our required frequency. Important to note is
  // that the divisor must be small enough to fit into 16-bits.
  uint32_t divisor = 1193180 / frequency;

  // Send the command byte.
  Write8(0x43, 0x36);

  // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
  uint8_t l = static_cast<uint8_t>(divisor & 0xFF);
  uint8_t h = static_cast<uint8_t>((divisor >> 8) & 0xFF);

  // Send the frequency divisor.
  Write8(0x40, l);
  Write8(0x40, h);

  EnableInterrupts();
}
