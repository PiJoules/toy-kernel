#include <DescriptorTables.h>
#include <Terminal.h>
#include <isr.h>
#include <kassert.h>
#include <kernel.h>
#include <kmalloc.h>
#include <kstring.h>
#include <kthread.h>
#include <paging.h>
#include <panic.h>
#include <syscall.h>

// FIXME: This is extern so functions in thread.s can access it, but it should
// instead probably just be accesswd through GetCurrentThread().
Thread *CurrentThread = nullptr;

namespace {

uint32_t next_tid = 0;

struct ThreadNode {
  Thread *thread;
  ThreadNode *next;
};

ThreadNode *ReadyQueue = nullptr;
const Thread *kMainKernelThread = nullptr;

void thread_is_ready(Thread &t) {
  assert(ReadyQueue);

  // Create a new list item for the new thread.
  ThreadNode *item = toy::kmalloc<ThreadNode>(1);
  item->thread = &t;
  item->next = ReadyQueue;
  ReadyQueue = item;
}

}  // namespace

// TODO: Rename this to distinguish it more from `exit_this_thread`.
void thread_exit();

Thread::Thread(ThreadFunc func, void *arg, bool user)
    : Thread(func, arg,
             reinterpret_cast<uint32_t *>(kmalloc(DEFAULT_THREAD_STACK_SIZE)),
             user) {}

Thread::Thread(ThreadFunc func, void *arg, uint32_t *stack_allocation,
               bool user)
    : id(next_tid++),
      state(RUNNING),
      first_run(true),
      stack_allocation(stack_allocation) {
  assert(reinterpret_cast<uintptr_t>(stack_allocation) % 4 == 0 &&
         "The stack allocation must be 4 byte aligned.");
  uint32_t *stack_bottom = getStackPointer();

  if (user) {
    pd_allocation = GetKernelPageDirectory().Clone();
  } else {
    pd_allocation = &GetKernelPageDirectory();
  }

  memset(&regs, 0, sizeof(regs));

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
    regs.ds = 0x23;
  } else {
    regs.ds = 0x10;
  }

  *(--stack_bottom) = UINT32_C(0x202);  // Interrupts enabled

  if (user) {
    *(--stack_bottom) = UINT32_C(0x1B);  // User code segment | ring 3
    regs.cs = 0x1b;
  } else {
    *(--stack_bottom) = UINT32_C(0x08);  // Kernel code segment
    regs.cs = 0x08;
  }

  *(--stack_bottom) = reinterpret_cast<uint32_t>(func);

  regs.esp = reinterpret_cast<uint32_t>(stack_bottom);
  regs.eflags = 0x202;  // Interrupts enabled.

  if (user) esp0_allocation = toy::kmalloc<uint8_t>(DEFAULT_THREAD_STACK_SIZE);

  thread_is_ready(*this);
}

Thread Thread::CreateUserProcess(ThreadFunc func, void *arg) {
  return Thread(func, arg, /*user=*/true);
}

Thread::~Thread() {
  if (isUserThread()) pd_allocation->ReclaimPageDirRegion();
  kfree(stack_allocation);
  kfree(esp0_allocation);
}

void Thread::Join() {
  while (this->state != COMPLETED) {}
}

void thread_exit() {
  DisableInterrupts();

  CurrentThread->state = COMPLETED;

  // Remove this thread then switch to another.
  schedule(nullptr);
  PANIC("Should have jumped to the next thread");
}

void exit_this_thread() { thread_exit(); }

void Thread::DumpRegs() const {
  terminal::WriteF("edi: {}\n", terminal::Hex(regs.edi));
  terminal::WriteF("esi: {}\n", terminal::Hex(regs.esi));
  terminal::WriteF("eip: {}\n", terminal::Hex(regs.eip));
  terminal::WriteF("ebp: {}\n", terminal::Hex(regs.ebp));
  terminal::WriteF("ds: {}\n", terminal::Hex(regs.ds));
}

const Thread *GetMainKernelThread() { return kMainKernelThread; }

const Thread *GetCurrentThread() { return CurrentThread; }

extern "C" void switch_kernel_thread_run(Thread *);
extern "C" void switch_first_kernel_thread_run(Thread *);
extern "C" void switch_first_user_thread_run(Thread *);
extern "C" void switch_user_thread_run(Thread *);

void InitScheduler() {
  assert(!ReadyQueue && !CurrentThread && !kMainKernelThread &&
         "This function should not be called twice.");
  CurrentThread = toy::kmalloc<Thread>(1);
  memset(CurrentThread, 0, sizeof(Thread));
  CurrentThread->id = next_tid++;
  CurrentThread->state = RUNNING;
  CurrentThread->first_run = false;

  // The main thread does not need to allocate a stack.
  CurrentThread->stack_allocation = nullptr;

  CurrentThread->pd_allocation = &GetKernelPageDirectory();

  kMainKernelThread = CurrentThread;

  ReadyQueue = toy::kmalloc<ThreadNode>(1);
  ReadyQueue->thread = CurrentThread;
  ReadyQueue->next = nullptr;
}

void schedule(const registers_t *regs) {
  // The queue does not have any items and is not ready yet.
  if (!ReadyQueue) return;

  // Only one thread is on the queue, so we don't need to change (fast path).
  if (!ReadyQueue->next) return;
  assert(!InteruptsAreEnabled());

  // Iterate through the ready queue to the end.
  ThreadNode *last_node = ReadyQueue;
  while (last_node->next) last_node = last_node->next;

  // Get the next thread and move its node to the end of the queue.
  ThreadNode *new_thread = ReadyQueue;
  if (last_node != ReadyQueue) {
    // Only cycle through nodes if there is more than 1 node in the queue.
    ReadyQueue = ReadyQueue->next;
    last_node->next = new_thread;
    new_thread->next = nullptr;
  }

  Thread *thread = new_thread->thread;

  if (regs) {
    uint32_t adjusted_esp;

    uint32_t *esp = reinterpret_cast<uint32_t *>(regs->esp);
    assert(esp[0] == IRQ0);  // Came frome Timer
    assert(esp[1] == 0);
    if (CurrentThread->isKernelThread()) {
      // interrupt num + error code + eip + cs + eflags
      adjusted_esp = regs->esp + 20;
      assert(esp[3] == 0x08);
    } else {
      adjusted_esp = regs->useresp;
      assert(esp[3] == 0x1b);
      assert(esp[6] == 0x23);
    }

    CurrentThread->regs.ebp = regs->ebp;
    CurrentThread->regs.eip = regs->eip;
    CurrentThread->regs.cs = static_cast<uint16_t>(regs->cs);
    CurrentThread->regs.eflags = regs->eflags;

    CurrentThread->regs.esp = adjusted_esp;

    CurrentThread->regs.ebx = regs->ebx;
    CurrentThread->regs.esi = regs->esi;
    CurrentThread->regs.edi = regs->edi;
    CurrentThread->regs.ds = static_cast<uint16_t>(regs->ds);
    CurrentThread->regs.es = static_cast<uint16_t>(regs->ds);
    CurrentThread->regs.fs = static_cast<uint16_t>(regs->ds);
    CurrentThread->regs.gs = static_cast<uint16_t>(regs->ds);
  }

  bool first_thread_run = thread->first_run;
  thread->first_run = false;
  bool jump_to_user = thread->isUserThread();

  if (!first_thread_run) assert(thread->regs.eip);

  if (!regs) {
    assert(CurrentThread != kMainKernelThread);

    // Delete the current thread since we got here from a thread_exit.
    // Find the node.
    ThreadNode *node = ReadyQueue;
    ThreadNode *prev = nullptr;
    while (node && node->thread != CurrentThread) {
      prev = node;
      node = node->next;
    }
    assert(node && "Could not find this thread.");

    if (prev) {
      prev->next = node->next;
    } else {
      // This is the front of the queue.
      assert(node == ReadyQueue);
      ReadyQueue = ReadyQueue->next;
    }

    kfree(node);
    CurrentThread = nullptr;
  }

  if (jump_to_user)
    set_kernel_stack(reinterpret_cast<uint32_t>(thread->getEsp0StackPointer()));

  // TODO: cr3 can only accept a physical address, but since we have paging
  // enabled, we need to have a separate area where we can identity map page
  // directories so we can refer to them here.
  SwitchPageDirectory(thread->getPageDirectory());

  // Switch to the new thread.
  CurrentThread = thread;
  if (first_thread_run && !jump_to_user) {
    switch_first_kernel_thread_run(thread);
  } else if (first_thread_run && jump_to_user) {
    switch_first_user_thread_run(thread);
  } else if (!first_thread_run && jump_to_user) {
    switch_user_thread_run(thread);
  } else {
    switch_kernel_thread_run(thread);
  }
  PANIC("Should've switched to a different thread");
}

void DestroyScheduler() {
  assert(ReadyQueue && !ReadyQueue->next &&
         "Expected only the main thread to be left.");
  kfree(ReadyQueue->thread);
  kfree(ReadyQueue);
}
