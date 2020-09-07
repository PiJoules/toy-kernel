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

}  // namespace

// TODO: Rename this to distinguish it more from `exit_this_thread`.
void thread_exit();

Thread::Thread(ThreadFunc func, void *arg, bool user)
    : Thread(func, arg,
             reinterpret_cast<uint32_t *>(kmalloc(DEFAULT_THREAD_STACK_SIZE)),
             user) {}

// The main thread does not need to allocate a stack.
Thread::Thread()
    : id_(next_tid++),
      state_(RUNNING),
      first_run_(false),
      pd_allocation(GetKernelPageDirectory()) {
  memset(&regs_, 0, sizeof(regs_));
}

Thread::Thread(ThreadFunc func, void *arg, uint32_t *stack_allocation,
               bool user)
    : id_(next_tid++),
      state_(RUNNING),
      first_run_(true),
      stack_allocation(stack_allocation),
      pd_allocation(user ? *GetKernelPageDirectory().Clone()
                         : GetKernelPageDirectory()) {
  memset(&regs_, 0, sizeof(regs_));

  assert(reinterpret_cast<uintptr_t>(stack_allocation) % 4 == 0 &&
         "The stack allocation must be 4 byte aligned.");
  uint32_t *stack_bottom = getStackPointer();

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
    getRegs().ds = 0x23;
  } else {
    getRegs().ds = 0x10;
  }

  *(--stack_bottom) = UINT32_C(0x202);  // Interrupts enabled

  if (user) {
    *(--stack_bottom) = UINT32_C(0x1B);  // User code segment | ring 3
    getRegs().cs = 0x1b;
  } else {
    *(--stack_bottom) = UINT32_C(0x08);  // Kernel code segment
    getRegs().cs = 0x08;
  }

  *(--stack_bottom) = reinterpret_cast<uint32_t>(func);

  getRegs().esp = reinterpret_cast<uint32_t>(stack_bottom);

  if (user) esp0_allocation = toy::kmalloc<uint8_t>(DEFAULT_THREAD_STACK_SIZE);

  assert(ReadyQueue);

  // Create a new list item for the new thread.
  ThreadNode *item = toy::kmalloc<ThreadNode>(1);
  item->thread = this;
  item->next = ReadyQueue;
  ReadyQueue = item;
}

// RVO guarantees that the destructor will not be called multiple times for
// these functions that return and create threads.
Thread Thread::CreateUserProcess(ThreadFunc func, void *arg) {
  return Thread(func, arg, /*user=*/true);
}

Thread Thread::CreateUserProcess(ThreadFunc func, size_t codesize, void *arg) {
  Thread t = CreateUserProcess(func, arg);
  assert(codesize && "Expected a non-zero size for user code");
  t.userfunc_ = func;
  t.usercode_size_ = codesize;
  return t;
}

Thread::~Thread() {
  if (this != GetMainKernelThread()) Join();
  if (isUserThread()) pd_allocation.ReclaimPageDirRegion();
  kfree(stack_allocation);
  kfree(esp0_allocation);
}

void Thread::Join() {
  while (this->state_ != COMPLETED) {}
}

void thread_exit() {
  DisableInterrupts();

  CurrentThread->state_ = COMPLETED;

  // Remove this thread then switch to another.
  schedule(nullptr);
  PANIC("Should have jumped to the next thread");
}

void exit_this_thread() { thread_exit(); }

const Thread *GetMainKernelThread() { return kMainKernelThread; }

const Thread *GetCurrentThread() { return CurrentThread; }

extern "C" void switch_kernel_thread_run(Thread::regs_t *);
extern "C" void switch_first_kernel_thread_run(Thread::regs_t *);
extern "C" void switch_first_user_thread_run(Thread::regs_t *);
extern "C" void switch_user_thread_run(Thread::regs_t *);

void InitScheduler() {
  assert(!ReadyQueue && !CurrentThread && !kMainKernelThread &&
         "This function should not be called twice.");
  CurrentThread = new Thread;
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

    CurrentThread->getRegs().ebp = regs->ebp;
    CurrentThread->getRegs().eip = regs->eip;
    CurrentThread->getRegs().cs = static_cast<uint16_t>(regs->cs);
    CurrentThread->getRegs().eflags = regs->eflags;

    CurrentThread->getRegs().esp = adjusted_esp;

    CurrentThread->getRegs().ebx = regs->ebx;
    CurrentThread->getRegs().esi = regs->esi;
    CurrentThread->getRegs().edi = regs->edi;
    CurrentThread->getRegs().ds = static_cast<uint16_t>(regs->ds);
    CurrentThread->getRegs().es = static_cast<uint16_t>(regs->ds);
    CurrentThread->getRegs().fs = static_cast<uint16_t>(regs->ds);
    CurrentThread->getRegs().gs = static_cast<uint16_t>(regs->ds);
  }

  bool first_thread_run = thread->first_run_;
  thread->first_run_ = false;
  bool jump_to_user = thread->isUserThread();

  if (!first_thread_run) assert(thread->getRegs().eip);

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

  SwitchPageDirectory(thread->getPageDirectory());

  if (first_thread_run && jump_to_user && thread->ShouldCopyUsercode()) {
    // FIXME: We skip the first 4MB because we could still need to read
    // stuff that multiboot inserted in the first 4MB page. Starting from 0
    // here could lead to overwriting that multiboot data. We should
    // probably copy that data somewhere else after paging is enabled.
    thread->getPageDirectory().AddPage(
        USER_START, GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1),
        PG_USER);
    memcpy(reinterpret_cast<void *>(USER_START),
           reinterpret_cast<void *>(thread->userfunc_), thread->usercode_size_);
  }

  Thread::regs_t *thread_regs = &thread->getRegs();

  // Switch to the new thread.
  CurrentThread = thread;
  if (first_thread_run && !jump_to_user) {
    switch_first_kernel_thread_run(thread_regs);
  } else if (first_thread_run && jump_to_user) {
    switch_first_user_thread_run(thread_regs);
  } else if (!first_thread_run && jump_to_user) {
    switch_user_thread_run(thread_regs);
  } else {
    switch_kernel_thread_run(thread_regs);
  }
  PANIC("Should've switched to a different thread");
}

void DestroyScheduler() {
  assert(ReadyQueue && !ReadyQueue->next &&
         "Expected only the main thread to be left.");
  delete ReadyQueue->thread;
  kfree(ReadyQueue);
}
