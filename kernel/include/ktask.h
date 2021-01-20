#ifndef KTASK_H_
#define KTASK_H_

#include <assert.h>
#include <isr.h>
#include <kmalloc.h>
#include <paging.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_THREAD_STACK_SIZE 2048  // Use a 2kB kernel stack.

enum TaskState {
  READY,      // The task has not started yet, but is on the queue and can run.
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

class KernelTask;
class UserTask;

class Task {
 public:
  virtual ~Task() {}

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

  PageDirectory &getPageDirectory() const { return pd_allocation_; }

  // The child class should define this.
  virtual bool isUserTask() const = 0;
  bool isKernelTask() const { return !isUserTask(); }

  uint32_t *getStackPointer() const {
    uint32_t *stack_bottom = getStackPointerImpl();
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The user stack is not 4 byte aligned.");
    return stack_bottom;
  }

  void Join();

  // Indicates to the scheduler that when this task is about to run, that it
  // will be the first time this task runs ever.
  bool OnFirstRun() const { return state_ == READY; }

  // This will be run right before the context switch into the next task.
  virtual void SetupBeforeTaskRun() {}

 protected:
  virtual uint32_t *getStackPointerImpl() const = 0;

  // This is used specifically by InitScheduler() for initializing the default
  // kernel task without needing to set anything on the default stack.
  Task();
  Task(PageDirectory &pd_allocation);

  void AddToQueue();

 private:
  friend void exit_this_task();
  friend void schedule(const X86Registers *);

  const uint32_t id_;  // Task ID.

  // This is volatile so we can access it each time in Join().
  volatile TaskState state_;

  X86TaskRegs regs_;

  PageDirectory &pd_allocation_;
};

class KernelTask : public Task {
 public:
  KernelTask(TaskFunc func, void *arg = nullptr);
  ~KernelTask();

  bool isUserTask() const override { return false; }

 protected:
  uint32_t *getStackPointerImpl() const override {
    assert(this != GetMainKernelTask() &&
           "Should not need to call this method on the main task since we do "
           "not allocate a stack for it.");
    assert(stack_allocation_);

    auto *header = MallocHeader::FromPointer(stack_allocation_);
    return reinterpret_cast<uint32_t *>(header->getEnd());
  }

 private:
  friend void InitScheduler();

  // This is used for making the main kernel task.
  KernelTask();

  // These should be null for the main kernel task.
  uint32_t *stack_allocation_;
};

class UserTask : public Task {
 public:
  UserTask(TaskFunc func, size_t codesize, void *arg = nullptr);
  ~UserTask();

  bool isUserTask() const override { return true; }

  void SetupBeforeTaskRun() override;

  TaskFunc getUserFunc() const {
    assert(userfunc_);
    return userfunc_;
  }

  size_t getCodeSize() const {
    assert(usercode_size_);
    return usercode_size_;
  }

  /**
   * This serves as the stack that the kernel will jump to on a syscall
   * interrupt. Each task must have its own allocated esp0 stack.
   */
  uint32_t *getEsp0StackPointer() const {
    assert(esp0_allocation_);
    auto *header = MallocHeader::FromPointer(esp0_allocation_);
    uint32_t *stack_bottom = reinterpret_cast<uint32_t *>(header->getEnd());
    assert(reinterpret_cast<uintptr_t>(stack_bottom) % 4 == 0 &&
           "The esp0 stack is not 4 byte aligned.");
    return stack_bottom;
  }

 protected:
  uint32_t *getStackPointerImpl() const override {
    return reinterpret_cast<uint32_t *>(USER_SHARED_SPACE_END);
  }

 private:
  uint8_t *esp0_allocation_;

  // Used for loading external user programs.
  TaskFunc userfunc_;
  size_t usercode_size_;
};

void exit_this_task();

void InitScheduler();
void schedule(const X86Registers *regs);
void DestroyScheduler();

#endif
