#ifndef TASK_H_
#define TASK_H_

#include <isr.h>
#include <kassert.h>
#include <kmalloc.h>
#include <kstddef.h>
#include <kstdint.h>
#include <paging.h>

#define DEFAULT_THREAD_STACK_SIZE 2048  // Use a 2kB kernel stack.

enum TaskState {
  RUNNING,    // The task is running.
  COMPLETED,  // The task finished running.
};

class Task;

const Task *GetMainKernelTask();
const Task *GetCurrentTask();

// Accept any argument and return void.
using TaskFunc = void (*)(void *);

constexpr uint32_t kKernelDataSegment = 0x10;
constexpr uint32_t kUserDataSegment = 0x23;
constexpr uint32_t kKernelCodeSegment = 0x08;
constexpr uint32_t kUserCodeSegment = 0x1b;

class Task {
 private:
  Task(TaskFunc func, void *arg, bool user = false);

  /**
   * Create a task from a heap-allocated stack. This allocation must be a
   * raw pointer eventually returned by kmalloc(). The this function takes
   * ownership of the stack allocation and is in charge of freeing it when the
   * task is finished.
   */
  Task(TaskFunc func, void *arg, uint32_t *stack_allocation, bool user = false);

 public:
  static Task CreateKernelTask(TaskFunc func, void *arg);
  static Task CreateKernelTask(TaskFunc func, void *arg,
                               uint32_t *stack_allocation);
  static Task CreateUserTask(TaskFunc func, size_t codesize, void *arg);

  ~Task();

  // NOTE: These values are assigned in task.s. Be sure to update that file if
  // these are ever changed.
  struct X86TaskRegs {
    uint32_t esp, ebp, eax, ebx, ecx, edx, esi, edi, eflags, eip;
    uint16_t ds, es, fs, gs, cs;  // These must be 16-bit aligned.

    // 2-byte Padding
  };
  static_assert(sizeof(X86TaskRegs) == 52,
                "Size of regs changed! If this is intended, be sure to also "
                "update task.s");

  // These are here to provide easy-to-lookup offsets for task.s.
  static_assert(offsetof(X86TaskRegs, esp) == 0);
  static_assert(offsetof(X86TaskRegs, ebp) == 4);
  static_assert(offsetof(X86TaskRegs, eax) == 8);
  static_assert(offsetof(X86TaskRegs, ebx) == 12);
  static_assert(offsetof(X86TaskRegs, ecx) == 16);
  static_assert(offsetof(X86TaskRegs, edx) == 20);
  static_assert(offsetof(X86TaskRegs, esi) == 24);
  static_assert(offsetof(X86TaskRegs, edi) == 28);
  static_assert(offsetof(X86TaskRegs, eflags) == 32);
  static_assert(offsetof(X86TaskRegs, eip) == 36);
  static_assert(offsetof(X86TaskRegs, ds) == 40);
  static_assert(offsetof(X86TaskRegs, es) == 42);
  static_assert(offsetof(X86TaskRegs, fs) == 44);
  static_assert(offsetof(X86TaskRegs, gs) == 46);
  static_assert(offsetof(X86TaskRegs, cs) == 48);

  const X86TaskRegs &getRegs() const { return regs_; }
  X86TaskRegs &getRegs() { return regs_; }
  uint32_t getID() const { return id_; }

  PageDirectory &getPageDirectory() const { return pd_allocation; }

  bool isUserTask() const { return getRegs().ds == kUserDataSegment; }
  bool isKernelTask() const { return !isUserTask(); }

  uint32_t *getStackPointer() const {
    assert(this != GetMainKernelTask() &&
           "Should not need to call this method on the main task since we do "
           "not allocate a stack for it.");
    assert(stack_allocation);
    auto *header = MallocHeader::FromPointer(stack_allocation);
    auto *stack_bottom = reinterpret_cast<uint32_t *>(header->getEnd());
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The user stack is not 4 byte aligned.");
    return stack_bottom;
  }

  /**
   * This serves as the stack that the kernel will jump to on a syscall
   * interrupt. Each task must have its own allocated esp0 stack.
   */
  uint32_t *getEsp0StackPointer() const {
    assert(isUserTask() && "Should not need esp0 for a kernel task");
    assert(esp0_allocation);
    auto *header = MallocHeader::FromPointer(esp0_allocation);
    uint32_t *stack_bottom = reinterpret_cast<uint32_t *>(header->getEnd());
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The esp0 stack is not 4 byte aligned.");
    return stack_bottom;
  }

  void Join();

 private:
  friend void InitScheduler();
  friend void task_exit();
  friend void schedule(const X86Registers *);

  // This is used specifically by InitScheduler() for initializing the default
  // kernel task without needing to set anything on the default stack.
  Task();

  const uint32_t id_;  // Task ID.

  // This is volatile so we can access it each time in Join().
  volatile TaskState state_;
  bool first_run_;

  X86TaskRegs regs_;

  // These should be null for the main kernel task.
  uint32_t *stack_allocation = nullptr;
  uint8_t *esp0_allocation = nullptr;
  PageDirectory &pd_allocation;

  // Used for loading external user programs.
  TaskFunc userfunc_ = nullptr;
  size_t usercode_size_;
};

void exit_this_task();

void InitScheduler();
void schedule(const X86Registers *regs);
void DestroyScheduler();

#endif
