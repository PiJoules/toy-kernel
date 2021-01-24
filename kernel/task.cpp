#include <DescriptorTables.h>
#include <assert.h>
#include <isr.h>
#include <kernel.h>
#include <kmalloc.h>
#include <ktask.h>
#include <paging.h>
#include <panic.h>
#include <string.h>
#include <syscall.h>

namespace {

Task *CurrentTask = nullptr;

uint32_t next_tid = 0;

struct TaskNode {
  Task *task;
  TaskNode *next;
};

TaskNode *ReadyQueue = nullptr;
const Task *kMainKernelTask = nullptr;

}  // namespace

// This is used for constructing the main kernel task.
Task::Task()
    : id_(next_tid++),
      state_(RUNNING),
      pd_allocation_(GetKernelPageDirectory()) {
  memset(&regs_, 0, sizeof(regs_));
}

KernelTask::KernelTask() : Task() {}

Task::Task(PageDirectory &pd_allocation)
    : id_(next_tid++), state_(READY), pd_allocation_(pd_allocation) {
  memset(&regs_, 0, sizeof(regs_));
  assert(ReadyQueue && "Scheduling has not yet been initialized.");
}

KernelTask::KernelTask(TaskFunc func, void *arg)
    : Task(GetKernelPageDirectory()),
      stack_allocation_(
          reinterpret_cast<uint32_t *>(kmalloc(DEFAULT_THREAD_STACK_SIZE))) {
  // Setup the initial stack which will be used when jumping into this task for
  // the first time.
  uint32_t *stack_bottom = getStackPointer();
  *(--stack_bottom) = reinterpret_cast<uint32_t>(arg);
  *(--stack_bottom) = reinterpret_cast<uint32_t>(exit_this_task);

  getRegs().ds = kKernelDataSegment;

  *(--stack_bottom) = UINT32_C(0x202);  // Interrupts enabled

  *(--stack_bottom) = UINT32_C(kKernelCodeSegment);  // Kernel code segment
  getRegs().cs = kKernelCodeSegment;

  *(--stack_bottom) = reinterpret_cast<uint32_t>(func);

  getRegs().esp = reinterpret_cast<uint32_t>(stack_bottom);
  // End setting up the stack.

  AddToQueue();
}

UserTask::UserTask(TaskFunc func, size_t codesize, void *arg,
                   CopyArgFunc copyfunc)
    : Task(*GetKernelPageDirectory().Clone()),
      esp0_allocation_(toy::kmalloc<uint8_t>(DEFAULT_THREAD_STACK_SIZE)),
      userfunc_(func),
      usercode_size_(codesize) {
  void *paddr = GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1);

  void *user_shared = (void *)USER_SHARED_SPACE_START;

  // Allocate the shared user space.
  assert(!getPageDirectory().isVirtualMapped(user_shared) &&
         "The page directory for this user task should not have previously "
         "reserves the shared user space page.");
  getPageDirectory().AddPage((void *)USER_SHARED_SPACE_START, paddr, PG_USER);

  // Temporarily use the same physical address for this page directory. Writes
  // to this will also be written to the shared space in the user PD.
  GetKernelPageDirectory().AddPage(user_shared, paddr, /*flags=*/0,
                                   /*allow_physical_reuse=*/true);
  void *stack_arg = copyfunc(arg, (void *)USER_SHARED_SPACE_START,
                             (void *)USER_SHARED_SPACE_END);

  // Setup the initial stack which will be used when jumping into this task for
  // the first time.
  uint32_t *stack_bottom = getStackPointer();
  *(--stack_bottom) = reinterpret_cast<uint32_t>(stack_arg);

  // FIXME: This might not be needed if user tasks should just end up calling
  // the task exit syscall.
  *(--stack_bottom) = reinterpret_cast<uint32_t>(syscall_exit_user_task);

  auto current_stack_bottom = reinterpret_cast<uint32_t>(stack_bottom);
  *(--stack_bottom) = UINT32_C(kUserDataSegment);  // User data segment | ring 3
  *(--stack_bottom) = current_stack_bottom;
  getRegs().ds = kUserDataSegment;

  *(--stack_bottom) = UINT32_C(0x202);  // Interrupts enabled

  *(--stack_bottom) = UINT32_C(kUserCodeSegment);  // User code segment | ring 3
  getRegs().cs = kUserCodeSegment;

  *(--stack_bottom) = USER_START;

  getRegs().esp = reinterpret_cast<uint32_t>(stack_bottom);
  // End setting up the stack.

  // We don't need to access the stack anymore after this.
  GetKernelPageDirectory().RemovePage((void *)USER_SHARED_SPACE_START);

  AddToQueue();
}

void Task::AddToQueue() {
  TaskNode *item = toy::kmalloc<TaskNode>(1);
  item->task = this;
  item->next = ReadyQueue;
  ReadyQueue = item;
}

KernelTask::~KernelTask() {
  if (this != GetMainKernelTask()) Join();
  kfree(stack_allocation_);
}

UserTask::~UserTask() {
  if (this != GetMainKernelTask()) Join();
  getPageDirectory().ReclaimPageDirRegion();
  kfree(esp0_allocation_);
}

void Task::Join() {
  while (this->state_ != COMPLETED) {}
}

void exit_this_task() {
  DisableInterrupts();

  CurrentTask->state_ = COMPLETED;

  // Remove this task then switch to another.
  schedule(nullptr);
  PANIC("Should have jumped to the next task");
}

const Task *GetMainKernelTask() { return kMainKernelTask; }

const Task *GetCurrentTask() { return CurrentTask; }

extern "C" void switch_kernel_task_run(Task::X86TaskRegs *);
extern "C" void switch_first_kernel_task_run(Task::X86TaskRegs *);
extern "C" void switch_first_user_task_run(Task::X86TaskRegs *);
extern "C" void switch_user_task_run(Task::X86TaskRegs *);

void InitScheduler() {
  assert(!ReadyQueue && !CurrentTask && !kMainKernelTask &&
         "This function should not be called twice.");
  CurrentTask = new KernelTask();
  kMainKernelTask = CurrentTask;

  ReadyQueue = toy::kmalloc<TaskNode>(1);
  ReadyQueue->task = CurrentTask;
  ReadyQueue->next = nullptr;
}

void schedule(const X86Registers *regs) {
  // The queue does not have any items and is not ready yet.
  if (!ReadyQueue) return;

  // Only one task is on the queue, so we don't need to change (fast path).
  if (!ReadyQueue->next) return;
  assert(!InterruptsAreEnabled() &&
         "Interupts should not be enabled at this point.");

  // Iterate through the ready queue to the end.
  TaskNode *last_node = ReadyQueue;
  while (last_node->next) last_node = last_node->next;

  // Get the next task and move its node to the end of the queue.
  TaskNode *task_node = ReadyQueue;
  if (last_node != ReadyQueue) {
    // Only cycle through nodes if there is more than 1 node in the queue.
    ReadyQueue = ReadyQueue->next;
    last_node->next = task_node;
    task_node->next = nullptr;
  }

  Task *task = task_node->task;

  if (regs) {
    uint32_t adjusted_esp;

    uint32_t *esp = reinterpret_cast<uint32_t *>(regs->esp);
    assert(esp[0] == IRQ0);  // Came frome Timer
    assert(esp[1] == 0);
    if (CurrentTask->isKernelTask()) {
      // interrupt num + error code + eip + cs + eflags
      adjusted_esp = regs->esp + 20;
      assert(esp[3] == 0x08);
    } else {
      adjusted_esp = regs->useresp;
      assert(esp[3] == 0x1b);
      assert(esp[6] == 0x23);
    }

    CurrentTask->getRegs().esp = adjusted_esp;
    CurrentTask->getRegs().ebp = regs->ebp;

    CurrentTask->getRegs().eax = regs->eax;
    CurrentTask->getRegs().ebx = regs->ebx;
    CurrentTask->getRegs().ecx = regs->ecx;
    CurrentTask->getRegs().edx = regs->edx;

    CurrentTask->getRegs().esi = regs->esi;
    CurrentTask->getRegs().edi = regs->edi;

    CurrentTask->getRegs().eip = regs->eip;
    CurrentTask->getRegs().eflags = regs->eflags;

    CurrentTask->getRegs().cs = static_cast<uint16_t>(regs->cs);
    CurrentTask->getRegs().ds = static_cast<uint16_t>(regs->ds);
    CurrentTask->getRegs().es = static_cast<uint16_t>(regs->ds);
    CurrentTask->getRegs().fs = static_cast<uint16_t>(regs->ds);
    CurrentTask->getRegs().gs = static_cast<uint16_t>(regs->ds);
  } else {
    assert(CurrentTask != kMainKernelTask);

    // Delete the current task since we got here from a task exit.
    // Find the node.
    TaskNode *node = ReadyQueue;
    TaskNode *prev = nullptr;
    while (node && node->task != CurrentTask) {
      prev = node;
      node = node->next;
    }
    assert(node && "Could not find this task.");

    if (prev) {
      prev->next = node->next;
    } else {
      // This is the front of the queue.
      assert(node == ReadyQueue);
      ReadyQueue = ReadyQueue->next;
    }

    kfree(node);
    CurrentTask = nullptr;
  }

  SwitchPageDirectory(task->getPageDirectory());

  task->SetupBeforeTaskRun();

  bool first_task_run = task->OnFirstRun();
  task->state_ = RUNNING;
  bool jump_to_user = task->isUserTask();

  assert(
      (first_task_run || task->getRegs().eip) &&
      "Expected either for this to be the first time this task is run or to "
      "have been switched from prior, and eip would point to a valid address.");

  Task::X86TaskRegs *task_regs = &task->getRegs();

  // Switch to the new task.
  CurrentTask = task;
  if (first_task_run && !jump_to_user) {
    switch_first_kernel_task_run(task_regs);
  } else if (first_task_run && jump_to_user) {
    switch_first_user_task_run(task_regs);
  } else if (!first_task_run && jump_to_user) {
    switch_user_task_run(task_regs);
  } else {
    switch_kernel_task_run(task_regs);
  }
  PANIC("Should've switched to a different task");
}

void UserTask::SetupBeforeTaskRun() {
  set_kernel_stack(reinterpret_cast<uint32_t>(getEsp0StackPointer()));

  if (OnFirstRun()) {
    // This will be the first instance this user task should run. In this case,
    // we should copy the code into its own address space.
    // FIXME: We skip the first 4MB because we could still need to read
    // stuff that multiboot inserted in the first 4MB page. Starting from 0
    // here could lead to overwriting that multiboot data. We should
    // probably copy that data somewhere else after paging is enabled.
    getPageDirectory().AddPage(
        (void *)USER_START,
        GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1), PG_USER);
    memcpy(reinterpret_cast<void *>(USER_START),
           reinterpret_cast<void *>(userfunc_), usercode_size_);

    uint32_t *stack = reinterpret_cast<uint32_t *>(getRegs().esp);
    assert(*stack == USER_START);
  }
}

void DestroyScheduler() {
  assert(ReadyQueue && !ReadyQueue->next &&
         "Expected only the main task to be left.");
  delete ReadyQueue->task;
  kfree(ReadyQueue);
}
