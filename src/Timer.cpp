#include <Terminal.h>
#include <Timer.h>
#include <io.h>
#include <isr.h>
#include <kernel.h>
#include <kstdint.h>
#include <ktask.h>

uint32_t tick = 0;

// Ensure a task runs for at least this many ticks before switching.
constexpr uint32_t kQuanta = 10;

namespace {

void TimerCallback(registers_t *regs) {
  ++tick;

  // NOTE: If it turns out the schedule() function takes longer than it does for
  // the PIT to tick once more, then it's possible for us to be stuck at a given
  // address because we will hit this callback again after handling the previous
  // timer callback. To prevent taking a *potentially* slow schedule(), we can
  // insteak take the fast path of not scheduling and allow the current task to
  // keep running in hopes that it can at least progress. Note that it is also
  // possible, but less likely, that if this callback in general is slow, then
  // we could also be stuck at the same instruction no matter which path we
  // take. (Let's just hope it doesn't come down to this ever.)
  if (tick % kQuanta == 0) schedule(regs);
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
