#ifndef KTASK_H_
#define KTASK_H_

#include <allocator.h>
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
Task *GetCurrentTask();

// Accept any argument and return void.
using TaskFunc = void (*)(void *);

constexpr const uint32_t kKernelDataSegment = 0x10;
constexpr const uint32_t kUserDataSegment = 0x23;
constexpr const uint32_t kKernelCodeSegment = 0x08;
constexpr const uint32_t kUserCodeSegment = 0x1b;

class KernelTask;
class UserTask;

// A task represents all the info necessary about the current context that we
// are runnning in. This includes saved registers (when switching from another
// task), the current page directory/address space, what ring we're in, etc.
//
// Note that because we can create other tasks in whatever the current task is,
// we should take care to limit accesses to that new task's address space only
// during creation.
class Task {
 public:
  virtual ~Task();

  // NOTE: These values are assigned in task.s. Be sure to update that file if
  // these are ever changed.
  struct X86TaskRegs {
    uint32_t esp, ebp, eax, ebx, ecx, edx, esi, edi, eflags, eip;
    uint16_t ds, es, fs, gs, cs;  // These must be 16-bit aligned.

    // 2-byte Padding

    void Dump() const;
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
  bool Finished() const { return state_ == COMPLETED; }

  // This will be run right before the context switch into the next task.
  virtual void SetupBeforeTaskRun() {}

  // Copy data from a virtual address in the current task's address space to a
  // virtual address in this tasks's address space. If the current task is the
  // same as this task, this just performs a memcpy().
  void Write(void *this_dst, const void *current_src, size_t size);

  // Copy data from a virtual address in this task's address space to a virtual
  // address in the current task's address space. If the current task is the
  // same as this task, this just performs a memcpy().
  void Read(void *current_dst, const void *this_dst, size_t size);

  Task *getParent() const {
    assert(this != GetMainKernelTask() &&
           "Attempting to get non-existant parent of the main kernel task.");
    return parent_task_;
  }

 protected:
  virtual uint32_t *getStackPointerImpl() const = 0;

  // This is used specifically by InitScheduler() for initializing the default
  // kernel task without needing to set anything on the default stack.
  Task();
  Task(PageDirectory &pd_allocation);

  void AddToQueue();

  void AddChildTask(Task &task);
  void RemoveChildTask(Task &task);

 private:
  friend void exit_this_task();
  friend void schedule(const X86Registers *);

  const uint32_t id_;  // Task ID.

  // This is volatile so we can access it each time in Join().
  volatile TaskState state_;

  X86TaskRegs regs_;
  PageDirectory &pd_allocation_;

  // FIXME: This is only meaningful for user tasks, not all tasks in general.
  // This should be removed.
  bool user_in_kernel_space_;

  Task *parent_task_;  // This will be null for the main kernel task.
  std::vector<Task *> child_tasks_;
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

    auto *header = utils::MallocHeader::FromPointer(stack_allocation_);
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
  // When copying an argument to the shared user region, we can provide this
  // function to the UserTask constructor to control how to copy the argument
  // into the shared region. `arg` is the argument, `dst_start` is the start of
  // the region the argument can be copied to (USER_SHARED_SPACE_START),
  // and `dst_end` is the end of the region (USER_SHARED_SPACE_END).
  //
  // Also return a pointer representing the value to be added to the stack.
  //
  // FIXME: It's very possible for arguments here to be overwritten by stuff we
  // add to the stack. We should make it so that `dst_end` actually points
  // bellow the lowest point this stack can get during setup.
  using CopyArgFunc = void *(*)(void *arg, void *dst_start, void *dst_end);

  static void *CopyArgDefault(void *arg, [[maybe_unused]] void *dst_start,
                              void *dst_end) {
    memcpy((uint8_t *)dst_end - sizeof(arg), &arg, sizeof(arg));
    return arg;
  }

  UserTask(TaskFunc func, size_t codesize, void *arg = nullptr,
           CopyArgFunc copyfunc = CopyArgDefault, size_t entry_offset = 0);
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
    auto *header = utils::MallocHeader::FromPointer(esp0_allocation_);
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
  uint32_t entry_offset_;
};

void exit_this_task();

void InitScheduler();
void schedule(const X86Registers *regs);
void DestroyScheduler();

#endif
