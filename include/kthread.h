#ifndef THREAD_H_
#define THREAD_H_

#include <kassert.h>
#include <kmalloc.h>
#include <kstdint.h>

#define DEFAULT_THREAD_STACK_SIZE 2048  // Use a 2kB kernel stack.

enum ThreadState {
  RUNNING,    // The thread is running.
  COMPLETED,  // The thread finished running.
};

struct thread_t;

const thread_t *GetMainKernelThread();
const thread_t *GetCurrentThread();

// Accept any argument and return void.
using ThreadFunc = void (*)(void *);

constexpr uint32_t kKernelDataSegment = 0x10;
constexpr uint32_t kUserDataSegment = 0x23;

struct thread_t {
  // NOTE: These values are assigned in thread.s. Be sure to update that file if
  // these are ever changed.
  // TODO: Remove any padding.
  struct {
    uint32_t esp, ebp, ebx, esi, edi, eflags;
    uint16_t ds, es, fs, gs;  // These must be 16-bit aligned.
    uint32_t __padding__, eip;
    uint16_t cs, __padding2__;
    uint32_t ss;
  } regs;
  static_assert(sizeof(regs) == 48,
                "Size of regs changed! If this is intended, be sure to also "
                "update thread.s");

  void DumpRegs() const;

  uint32_t id;  // Thread ID.
  ThreadState state;
  bool first_run;

  // This is 4 byte aligned so we can store addresses on here.
  uint32_t *stack_allocation;

  bool isUserThread() const { return regs.ss == kUserDataSegment; }
  bool isKernelThread() const { return !isUserThread(); }

  uint32_t *getStackPointer() const {
    assert(this != GetMainKernelThread() &&
           "Should not need to call this method on the main thread since we do "
           "not allocate a stack for it.");
    assert(stack_allocation);
    auto *header = MallocHeader::FromPointer(stack_allocation);
    uint32_t *stack_bottom = stack_allocation + header->size;
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The stack is not 4 byte aligned.");
    return stack_bottom;
  }
};

thread_t *init_threading();

/**
 * Create a thread. The thread must be freed manually by the user.
 *
 *   auto *thread = create_thread(func, arg);
 *   ...
 *   kfree(thread);
 */
thread_t *create_thread(ThreadFunc func, void *arg, bool user = false);

/**
 * Create a thread from a heap-allocated stack. This allocation must be a
 * raw pointer eventually returned by kmalloc(). The this function takes
 * ownership of the stack allocation and is in charge of freeing it when the
 * thread is finished.
 *
 * The thread must be freed manually by the user.
 *
 * TODO: Replace this with a unique_ptr so we can remove the 'ownership' part of
 * this comment.
 */
thread_t *create_thread(ThreadFunc func, void *arg, uint32_t *stack_allocation,
                        bool user = false);

/**
 * This operates exactly like create_thread() but the function operates in user
 * space.
 */
thread_t *create_user_thread(ThreadFunc func, void *arg);

void thread_join(volatile thread_t *thread);
void exit_this_thread();

#endif
