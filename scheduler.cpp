#include <DescriptorTables.h>
#include <Terminal.h>
#include <kassert.h>
#include <kmalloc.h>
#include <paging.h>
#include <panic.h>
#include <scheduler.h>

thread_t *CurrentThread = nullptr;
uint32_t next_tid = 0;

namespace {

struct ThreadNode {
  thread_t *thread;
  ThreadNode *next;
};

ThreadNode *ReadyQueue = nullptr;
const thread_t *kMainKernelThread = nullptr;

// FIXME: If we plan to task switch between multiple use threads, we should
// have one esp0 stack per user thread. If all use threads shared the same
// esp0 stack, switching from one to another would clobber values written by
// the first user thread.
uint8_t *SharedEsp0StackAllocation;

}  // namespace

const thread_t *GetMainKernelThread() { return kMainKernelThread; }

const thread_t *GetCurrentThread() { return CurrentThread; }

extern "C" void switch_kernel_thread_run(thread_t *);
extern "C" void switch_first_kernel_thread_run(thread_t *);
extern "C" void switch_first_user_thread_run(thread_t *);
extern "C" void switch_user_thread_run(thread_t *);

void init_scheduler() {
  assert(!ReadyQueue && !CurrentThread && !kMainKernelThread &&
         "This function should not be called twice.");
  CurrentThread = toy::kmalloc<thread_t>(1);
  memset(CurrentThread, 0, sizeof(thread_t));
  CurrentThread->id = next_tid++;
  CurrentThread->state = RUNNING;
  CurrentThread->first_run = false;

  // The main thread does not need to allocate a stack.
  CurrentThread->stack_allocation = nullptr;

  kMainKernelThread = CurrentThread;

  ReadyQueue = toy::kmalloc<ThreadNode>(1);
  ReadyQueue->thread = CurrentThread;
  ReadyQueue->next = nullptr;

  SharedEsp0StackAllocation = toy::kmalloc<uint8_t>(DEFAULT_THREAD_STACK_SIZE);
  set_kernel_stack(reinterpret_cast<uint32_t>(SharedEsp0StackAllocation +
                                              DEFAULT_THREAD_STACK_SIZE));
}

void thread_is_ready(thread_t *t) {
  assert(ReadyQueue);

  // Create a new list item for the new thread.
  ThreadNode *item = toy::kmalloc<ThreadNode>(1);
  item->thread = t;
  item->next = ReadyQueue;
  ReadyQueue = item;
}

void schedule(const registers_t *regs) {
  // The queue does not have any items and is not ready yet.
  if (!ReadyQueue) return;

  // Only one thread is on the queue, so we don't need to change (fast path).
  if (!ReadyQueue->next) return;

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

  thread_t *thread = new_thread->thread;

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
    CurrentThread->regs.cs = regs->cs;
    CurrentThread->regs.eflags = regs->eflags;

    CurrentThread->regs.esp = adjusted_esp;

    CurrentThread->regs.ebx = regs->ebx;
    CurrentThread->regs.esi = regs->esi;
    CurrentThread->regs.edi = regs->edi;
    CurrentThread->regs.ds = regs->ds;
    CurrentThread->regs.es = regs->ds;
    CurrentThread->regs.fs = regs->ds;
    CurrentThread->regs.gs = regs->ds;
  }

  bool first_thread_run = thread->first_run;
  thread->first_run = false;
  bool ring_switch = thread->isUserThread();

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

    kfree(CurrentThread->stack_allocation);
    kfree(node);
    CurrentThread = nullptr;
  }

  // Switch to the new thread.
  if (first_thread_run && !ring_switch) {
    switch_first_kernel_thread_run(thread);
  } else if (first_thread_run && ring_switch) {
    switch_first_user_thread_run(thread);
  } else if (!first_thread_run && ring_switch) {
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
  kfree(SharedEsp0StackAllocation);
}
