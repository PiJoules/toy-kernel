#include <DescriptorTables.h>
#include <Terminal.h>
#include <kassert.h>
#include <kernel.h>
#include <kmalloc.h>
#include <kstring.h>
#include <kthread.h>
#include <panic.h>
#include <scheduler.h>
#include <syscall.h>

extern uint32_t next_tid;

// TODO: Rename this to distinguish it more from `exit_this_thread`.
void thread_exit();

thread_t *create_thread(ThreadFunc func, void *arg, bool user) {
  return create_thread(
      func, arg,
      reinterpret_cast<uint32_t *>(kmalloc(DEFAULT_THREAD_STACK_SIZE)), user);
}

thread_t *create_thread(ThreadFunc func, void *arg, uint32_t *stack_allocation,
                        bool user) {
  auto *thread = toy::kmalloc<thread_t>(1);
  memset(thread, 0, sizeof(thread_t));
  thread->id = next_tid++;

  thread->stack_allocation = stack_allocation;
  uint32_t *stack_bottom = thread->getStackPointer();

  // When we are inside the thread function, the top of the stack will have the
  // thread_exit (representing the return address), followed by the first
  // argument passed to the thread. Once we hit the end of the thread function,
  // we will `ret` into the thread_exit function.
  *(--stack_bottom) = reinterpret_cast<uint32_t>(arg);
  if (user)
    *(--stack_bottom) = reinterpret_cast<uint32_t>(syscall_exit_user_thread);
  else
    *(--stack_bottom) = reinterpret_cast<uint32_t>(thread_exit);

  // When this as loaded as the new stack pointer, the `ret` instruction at the
  // end of `switch_thread` will instead jump to this function.
  if (user) {
    auto current_stack_bottom = reinterpret_cast<uint32_t>(stack_bottom);
    *(--stack_bottom) = UINT32_C(0x23);  // User data segment | ring 3
    *(--stack_bottom) = current_stack_bottom;
    thread->regs.ss = 0x23;
    thread->regs.ds = 0x23;
  } else {
    thread->regs.ss = 0x10;
    thread->regs.ds = 0x10;
  }

  *(--stack_bottom) = UINT32_C(0x202);  // Interrupts enabled

  if (user) {
    *(--stack_bottom) = UINT32_C(0x1B);  // User code segment | ring 3
    thread->regs.cs = 0x1b;
  } else {
    *(--stack_bottom) = UINT32_C(0x08);  // Kernel code segment
    thread->regs.cs = 0x08;
  }

  *(--stack_bottom) = reinterpret_cast<uint32_t>(func);

  thread->regs.esp = reinterpret_cast<uint32_t>(stack_bottom);

  thread->regs.eflags = 0x202;  // Interrupts enabled.
  thread_is_ready(thread);

  thread->state = RUNNING;
  thread->first_run = true;
  return thread;
}

thread_t *create_user_thread(ThreadFunc func, void *arg) {
  auto *thread = create_thread(func, arg, /*user=*/true);
  return thread;
}

void thread_join(volatile thread_t *thread) {
  while (thread->state != COMPLETED) {}
}

extern thread_t *CurrentThread;

void thread_exit() {
  bool interrupts = InteruptsAreEnabled();
  if (interrupts) DisableInterrupts();

  CurrentThread->state = COMPLETED;

  // Remove this thread then switch to another.
  schedule(nullptr);
  PANIC("Should have jumped to the next thread");
}

void exit_this_thread() { thread_exit(); }

void thread_t::DumpRegs() const {
  terminal::WriteF("edi: {}\n", terminal::Hex(regs.edi));
  terminal::WriteF("esi: {}\n", terminal::Hex(regs.esi));
  terminal::WriteF("eip: {}\n", terminal::Hex(regs.eip));
  terminal::WriteF("ebp: {}\n", terminal::Hex(regs.ebp));
  terminal::WriteF("ds: {}\n", terminal::Hex(regs.ds));
}
