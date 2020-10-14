#include <DescriptorTables.h>
#include <Terminal.h>
#include <isr.h>
#include <kassert.h>
#include <kernel.h>
#include <kmalloc.h>
#include <kstring.h>
#include <ktask.h>
#include <paging.h>
#include <panic.h>
#include <syscall.h>

// FIXME: This is extern so functions in task.s can access it, but it should
// instead probably just be accesswd through GetCurrentTask().
Task *CurrentTask = nullptr;

namespace {

uint32_t next_tid = 0;

struct TaskNode {
  Task *task;
  TaskNode *next;
};

TaskNode *ReadyQueue = nullptr;
const Task *kMainKernelTask = nullptr;

}  // namespace

// TODO: Rename this to distinguish it more from `exit_this_task`.
void task_exit();

Task::Task(TaskFunc func, void *arg, bool user)
    : Task(func, arg,
           reinterpret_cast<uint32_t *>(kmalloc(DEFAULT_THREAD_STACK_SIZE)),
           user) {}

// The main task does not need to allocate a stack.
Task::Task()
    : id_(next_tid++),
      state_(RUNNING),
      first_run_(false),
      pd_allocation(GetKernelPageDirectory()) {
  memset(&regs_, 0, sizeof(regs_));
}

Task::Task(TaskFunc func, void *arg, uint32_t *stack_allocation, bool user)
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

  // When we are inside the task function, the top of the stack will have the
  // task_exit (representing the return address), followed by the first
  // argument passed to the task. Once we hit the end of the task function,
  // we will `ret` into the task_exit function.
  *(--stack_bottom) = reinterpret_cast<uint32_t>(arg);
  if (user)
    *(--stack_bottom) = reinterpret_cast<uint32_t>(syscall_exit_user_task);
  else
    *(--stack_bottom) = reinterpret_cast<uint32_t>(task_exit);

  // When this as loaded as the new stack pointer, the `ret` instruction at the
  // end of `switch_task` will instead jump to this function.
  if (user) {
    auto current_stack_bottom = reinterpret_cast<uint32_t>(stack_bottom);
    *(--stack_bottom) =
        UINT32_C(kUserDataSegment);  // User data segment | ring 3
    *(--stack_bottom) = current_stack_bottom;
    getRegs().ds = kUserDataSegment;
  } else {
    getRegs().ds = kKernelDataSegment;
  }

  *(--stack_bottom) = UINT32_C(0x202);  // Interrupts enabled

  if (user) {
    *(--stack_bottom) =
        UINT32_C(kUserCodeSegment);  // User code segment | ring 3
    getRegs().cs = kUserCodeSegment;
  } else {
    *(--stack_bottom) = UINT32_C(kKernelCodeSegment);  // Kernel code segment
    getRegs().cs = kKernelCodeSegment;
  }

  // FIXME: This is a hack for detecting if the function we are calling is in
  // kernel or user space.
  {
    if (user &&
        (KERN_HEAP_BEGIN <= (uint32_t)func && (uint32_t)func < KERN_HEAP_END)) {
      *(--stack_bottom) = USER_START;
    } else {
      // This is kernel mode or kernel code.
      *(--stack_bottom) = reinterpret_cast<uint32_t>(func);
    }
  }

  getRegs().esp = reinterpret_cast<uint32_t>(stack_bottom);

  if (user) esp0_allocation = toy::kmalloc<uint8_t>(DEFAULT_THREAD_STACK_SIZE);

  assert(ReadyQueue);

  // Create a new list item for the new task.
  TaskNode *item = toy::kmalloc<TaskNode>(1);
  item->task = this;
  item->next = ReadyQueue;
  ReadyQueue = item;
}

// RVO guarantees that the destructor will not be called multiple times for
// these functions that return and create tasks.
Task Task::CreateUserTask(TaskFunc func, void *arg) {
  return Task(func, arg, /*user=*/true);
}

Task Task::CreateUserTask(TaskFunc func, size_t codesize, void *arg) {
  Task t = CreateUserTask(func, arg);
  assert(codesize && "Expected a non-zero size for user code");
  t.userfunc_ = func;
  t.usercode_size_ = codesize;
  return t;
}

Task::~Task() {
  if (this != GetMainKernelTask()) Join();
  if (isUserTask()) pd_allocation.ReclaimPageDirRegion();
  kfree(stack_allocation);
  kfree(esp0_allocation);
}

void Task::Join() {
  while (this->state_ != COMPLETED) {}
}

void task_exit() {
  DisableInterrupts();

  CurrentTask->state_ = COMPLETED;

  // Remove this task then switch to another.
  schedule(nullptr);
  PANIC("Should have jumped to the next task");
}

void exit_this_task() { task_exit(); }

const Task *GetMainKernelTask() { return kMainKernelTask; }

const Task *GetCurrentTask() { return CurrentTask; }

extern "C" void switch_kernel_task_run(Task::regs_t *);
extern "C" void switch_first_kernel_task_run(Task::regs_t *);
extern "C" void switch_first_user_task_run(Task::regs_t *);
extern "C" void switch_user_task_run(Task::regs_t *);

void InitScheduler() {
  assert(!ReadyQueue && !CurrentTask && !kMainKernelTask &&
         "This function should not be called twice.");
  CurrentTask = new Task;
  kMainKernelTask = CurrentTask;

  ReadyQueue = toy::kmalloc<TaskNode>(1);
  ReadyQueue->task = CurrentTask;
  ReadyQueue->next = nullptr;
}

void schedule(const registers_t *regs) {
  // The queue does not have any items and is not ready yet.
  if (!ReadyQueue) return;

  // Only one task is on the queue, so we don't need to change (fast path).
  if (!ReadyQueue->next) return;
  assert(!InteruptsAreEnabled());

  // Iterate through the ready queue to the end.
  TaskNode *last_node = ReadyQueue;
  while (last_node->next) last_node = last_node->next;

  // Get the next task and move its node to the end of the queue.
  TaskNode *new_task = ReadyQueue;
  if (last_node != ReadyQueue) {
    // Only cycle through nodes if there is more than 1 node in the queue.
    ReadyQueue = ReadyQueue->next;
    last_node->next = new_task;
    new_task->next = nullptr;
  }

  Task *task = new_task->task;

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

    CurrentTask->getRegs().eax = regs->eax;
    CurrentTask->getRegs().ebp = regs->ebp;
    CurrentTask->getRegs().eip = regs->eip;
    CurrentTask->getRegs().cs = static_cast<uint16_t>(regs->cs);
    CurrentTask->getRegs().eflags = regs->eflags;

    CurrentTask->getRegs().esp = adjusted_esp;

    CurrentTask->getRegs().ebx = regs->ebx;
    CurrentTask->getRegs().ecx = regs->ecx;
    CurrentTask->getRegs().edx = regs->edx;
    CurrentTask->getRegs().esi = regs->esi;
    CurrentTask->getRegs().edi = regs->edi;
    CurrentTask->getRegs().ds = static_cast<uint16_t>(regs->ds);
    CurrentTask->getRegs().es = static_cast<uint16_t>(regs->ds);
    CurrentTask->getRegs().fs = static_cast<uint16_t>(regs->ds);
    CurrentTask->getRegs().gs = static_cast<uint16_t>(regs->ds);
  }

  bool first_task_run = task->first_run_;
  task->first_run_ = false;
  bool jump_to_user = task->isUserTask();

  assert(first_task_run || task->getRegs().eip);

  if (!regs) {
    assert(CurrentTask != kMainKernelTask);

    // Delete the current task since we got here from a task_exit.
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

  if (jump_to_user)
    set_kernel_stack(reinterpret_cast<uint32_t>(task->getEsp0StackPointer()));

  SwitchPageDirectory(task->getPageDirectory());

  if (first_task_run && jump_to_user && task->ShouldCopyUsercode()) {
    // FIXME: We skip the first 4MB because we could still need to read
    // stuff that multiboot inserted in the first 4MB page. Starting from 0
    // here could lead to overwriting that multiboot data. We should
    // probably copy that data somewhere else after paging is enabled.
    task->getPageDirectory().AddPage(
        (void *)USER_START,
        GetPhysicalBitmap4M().NextFreePhysicalPage(/*start=*/1), PG_USER);
    memcpy(reinterpret_cast<void *>(USER_START),
           reinterpret_cast<void *>(task->userfunc_), task->usercode_size_);

    uint32_t *stack = (uint32_t *)task->getRegs().esp;
    assert(*stack == USER_START);
  }

  Task::regs_t *task_regs = &task->getRegs();

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

void DestroyScheduler() {
  assert(ReadyQueue && !ReadyQueue->next &&
         "Expected only the main task to be left.");
  delete ReadyQueue->task;
  kfree(ReadyQueue);
}