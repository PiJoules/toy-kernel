#ifndef THREAD_H_
#define THREAD_H_

#include <isr.h>
#include <kassert.h>
#include <kmalloc.h>
#include <kstddef.h>
#include <kstdint.h>
#include <paging.h>

#define DEFAULT_THREAD_STACK_SIZE 2048  // Use a 2kB kernel stack.

enum ThreadState {
  RUNNING,    // The thread is running.
  COMPLETED,  // The thread finished running.
};

struct Thread;

const Thread *GetMainKernelThread();
const Thread *GetCurrentThread();

// Accept any argument and return void.
using ThreadFunc = void (*)(void *);

constexpr uint32_t kKernelDataSegment = 0x10;
constexpr uint32_t kUserDataSegment = 0x23;

struct Thread {
  Thread(ThreadFunc func, void *arg, bool user = false);

  /**
   * Create a thread from a heap-allocated stack. This allocation must be a
   * raw pointer eventually returned by kmalloc(). The this function takes
   * ownership of the stack allocation and is in charge of freeing it when the
   * thread is finished.
   */
  Thread(ThreadFunc func, void *arg, uint32_t *stack_allocation,
         bool user = false);

  static Thread CreateUserProcess(ThreadFunc func, void *arg);

  ~Thread();

  // NOTE: These values are assigned in thread.s. Be sure to update that file if
  // these are ever changed.
  struct regs_t {
    uint32_t esp, ebp, ebx, esi, edi, eflags;
    uint16_t ds, es, fs, gs;  // These must be 16-bit aligned.
    uint32_t eip;
    uint16_t cs;  // 16-bit padding after this.
  } regs;
  static_assert(sizeof(regs) == 40,
                "Size of regs changed! If this is intended, be sure to also "
                "update thread.s");
  static_assert(offsetof(regs_t, ds) == 24);
  static_assert(offsetof(regs_t, eip) == 32);
  static_assert(offsetof(regs_t, cs) == 36);

  void DumpRegs() const;

  uint32_t id;  // Thread ID.
  volatile ThreadState state;
  bool first_run;

  // These should be null for the main kernel thread.
  uint32_t *stack_allocation = nullptr;
  uint8_t *esp0_allocation = nullptr;

  PageDirectory *pd_allocation = nullptr;

  PageDirectory &getPageDirectory() const { return *pd_allocation; }

  bool isUserThread() const { return regs.ds == kUserDataSegment; }
  bool isKernelThread() const { return !isUserThread(); }

  uint32_t *getStackPointer() const {
    assert(this != GetMainKernelThread() &&
           "Should not need to call this method on the main thread since we do "
           "not allocate a stack for it.");
    assert(stack_allocation);
    auto *header = MallocHeader::FromPointer(stack_allocation);
    auto *stack_bottom = reinterpret_cast<uint32_t *>(header->getEnd());
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The user stack is not 4 byte aligned.");
    return stack_bottom;
  }

  uint32_t *getEsp0StackPointer() const {
    assert(isUserThread() && "Should not need esp0 for a kernel thread");
    assert(esp0_allocation);
    auto *header = MallocHeader::FromPointer(esp0_allocation);
    uint32_t *stack_bottom = reinterpret_cast<uint32_t *>(header->getEnd());
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The esp0 stack is not 4 byte aligned.");
    return stack_bottom;
  }

  void Join();
};

void exit_this_thread();

void InitScheduler();
void schedule(const registers_t *regs);
void DestroyScheduler();

#endif
