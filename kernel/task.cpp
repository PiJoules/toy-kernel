#include <assert.h>
#include <descriptortables.h>
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
Task *kMainKernelTask = nullptr;

enum Direction {
  // Copy from the current task to another task.
  CurrentToOther,

  // Copy from another task to the current task.
  OtherToCurrent,
};

template <Direction Dir>
void TaskMemcpy(Task &task, Task &other_task, void *dst, const void *src,
                size_t size) {
  DisableInterruptsRAII raii;

  if (&other_task == &task) {
    memcpy(dst, src, size);
    return;
  }

  const void *vaddr_page;
  size_t vaddr_offset;
  const void *task_vaddr = Dir == CurrentToOther ? dst : src;
  if (reinterpret_cast<uint32_t>(task_vaddr) % kPageSize4M == 0) {
    vaddr_page = task_vaddr;
    vaddr_offset = 0;
  } else {
    vaddr_page = PageAddr4M(PageIndex4M(task_vaddr));
    assert(task_vaddr > vaddr_page);
    vaddr_offset =
        static_cast<size_t>(reinterpret_cast<const uint8_t *>(task_vaddr) -
                            reinterpret_cast<const uint8_t *>(vaddr_page));
  }

  void *paddr = task.getPageDirectory().GetPhysicalAddr(vaddr_page);
  uint8_t *shared_mem = (uint8_t *)TMP_SHARED_TASK_MEM_START;
  other_task.getPageDirectory().AddPage(shared_mem, paddr, /*flags=*/0,
                                        /*allow_physical_reuse=*/true);

  const void *adj_src =
      Dir == CurrentToOther ? src : (shared_mem + vaddr_offset);
  void *adj_dst = Dir == CurrentToOther ? (shared_mem + vaddr_offset) : dst;
  memcpy(adj_dst, adj_src, size);

  other_task.getPageDirectory().RemovePage(shared_mem);
}

}  // namespace

// This is used for constructing the main kernel task.
Task::Task()
    : id_(next_tid++),
      state_(RUNNING),
      pd_allocation_(GetKernelPageDirectory()),
      parent_task_(nullptr) {
  memset(&regs_, 0, sizeof(regs_));
}

KernelTask::KernelTask() : Task() {}

Task::Task(PageDirectory &pd_allocation)
    : id_(next_tid++),
      state_(READY),
      pd_allocation_(pd_allocation),
      parent_task_(GetCurrentTask()) {
  memset(&regs_, 0, sizeof(regs_));
  assert(ReadyQueue && "Scheduling has not yet been initialized.");

  parent_task_->AddChildTask(*this);
}

void Task::AddChildTask(Task &task) { child_tasks_.push_back(&task); }

void Task::RemoveChildTask(Task &task) {
  auto task_iter = child_tasks_.find(&task);
  assert(task_iter != child_tasks_.end() && "Child task does not exist.");
  child_tasks_.erase(task_iter);
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
  getPageDirectory().AddPage(user_shared, paddr, PG_USER);

  // Temporarily use the same physical address for this page directory. Writes
  // to this will also be written to the shared space in the user PD.
  GetKernelPageDirectory().AddPage(user_shared, paddr, /*flags=*/0,
                                   /*allow_physical_reuse=*/true);
  void *stack_arg = copyfunc(arg, user_shared, (void *)USER_SHARED_SPACE_END);

  // Setup the initial stack which will be used when jumping into this task for
  // the first time.
  uint32_t *stack_bottom = getStackPointer();
  Write(--stack_bottom, &stack_arg, sizeof(stack_arg));

  auto current_stack_bottom = reinterpret_cast<uint32_t>(stack_bottom);
  Write(--stack_bottom, &kUserDataSegment, sizeof(uint32_t));
  Write(--stack_bottom, &current_stack_bottom, sizeof(uint32_t));
  getRegs().ds = kUserDataSegment;

  auto src = UINT32_C(0x202);  // Interrupts enabled.
  Write(--stack_bottom, &src, sizeof(src));

  Write(--stack_bottom, &kUserCodeSegment, sizeof(uint32_t));
  getRegs().cs = kUserCodeSegment;

  src = USER_START;
  Write(--stack_bottom, &src, sizeof(src));

  getRegs().esp = reinterpret_cast<uint32_t>(stack_bottom);
  // End setting up the stack.

  // We don't need to access the stack anymore after this.
  GetKernelPageDirectory().RemovePage(user_shared);

  AddToQueue();

  // Copy the function code from the parent (current) task into this task's
  // address space.
  void *userstart_paddr = GetPhysicalBitmap4M().NextFreePhysicalPage();
  getPageDirectory().AddPage((void *)USER_START, userstart_paddr, PG_USER);
  assert(getPageDirectory().GetPhysicalAddr((void *)USER_START) ==
         userstart_paddr);
  Write((void *)USER_START, (void *)userfunc_, usercode_size_);
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
  Join();
  getPageDirectory().ReclaimPageDirRegion();
  kfree(esp0_allocation_);
}

__attribute__((always_inline)) inline void DumpRegs() {
  DisableInterruptsRAII raii;

  uint32_t esp, ebp, eax, ebx, ecx, edx, esi, edi;
  uint32_t eip = (uint32_t)__builtin_return_address(0);
  asm volatile("mov %%esp, %0" : "=r"(esp));
  asm volatile("mov %%ebp, %0" : "=r"(ebp));
  asm volatile("mov %%eax, %0" : "=r"(eax));
  asm volatile("mov %%ebx, %0" : "=r"(ebx));
  asm volatile("mov %%ecx, %0" : "=r"(ecx));
  asm volatile("mov %%edx, %0" : "=r"(edx));
  asm volatile("mov %%esi, %0" : "=r"(esi));
  asm volatile("mov %%edi, %0" : "=r"(edi));
  DebugPrint(
      "esp: {}\n"
      "ebp: {}\n"
      "eax: {}\n"
      "ebx: {}\n"
      "ecx: {}\n"
      "edx: {}\n"
      "esi: {}\n"
      "edi: {}\n"
      "eip: {}\n",
      print::Hex(esp), print::Hex(ebp), print::Hex(eax), print::Hex(ebx),
      print::Hex(ecx), print::Hex(edx), print::Hex(esi), print::Hex(edi),
      print::Hex(eip));
}

void Task::Join() {
  assert(InterruptsAreEnabled());
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
Task *GetCurrentTask() { return CurrentTask; }

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
  bool jump_to_user = task->isUserTask();
  CurrentTask->user_in_kernel_space_ = false;

  if (task->user_in_kernel_space_) {
    // Although we are switching to a user task, the interrupt for the switch
    // originally occurred while this user task was in kernel space (potentially
    // from an interrupt), so we should take the appropriate jump to kernel
    // space.
    jump_to_user = false;
  }

  if (regs) {
    // This holds the correct esp value we want to save for the current task.
    uint32_t adjusted_esp;

    // The stack can contain these values (each unsigned 32-bit ints):
    //
    //   // These are added by our interrupt handlers.
    //   esp[0]: interrupt num
    //   esp[1]: error code
    //
    //   // These are added when the interrupt is triggerred.
    //   esp[2]: eip
    //   esp[3]: cs
    //   esp[4]: eflags
    //
    //   // These are only added if an interrupt is triggerred from ring 3.
    //   esp[5]: esp0
    //   esp[6]: ds/ss
    //
    uint32_t *esp = reinterpret_cast<uint32_t *>(regs->esp);
    assert(esp[0] == IRQ0 &&
           "Expected this to only be called from a timer interrupt.");
    assert(
        esp[1] == 0 &&
        "No error code should be provided from the timer interrupt handler.");
    if (CurrentTask->isKernelTask()) {
      // If we came from a kernel task, we can just discard the values added to
      // the stack by the IRQ handler and the interrupt.
      adjusted_esp = regs->esp + 20;
      assert(esp[3] == 0x08 &&
             "Expected this interrupt to be triggerred from a kernel task "
             "while in kernel space.");
    } else {
      if (esp[3] == kUserCodeSegment) {
        // The interrupt was triggerred from a user task while in userspace.
        assert(esp[6] == kUserDataSegment &&
               "Expected this task to come from userspace.");

        // When returning back to this task, we will want to jump back to the
        // saved userspace stack.
        adjusted_esp = regs->useresp;
      } else {
        // This is currently a user task, but we tripped the scheduler while in
        // kernel mode, probably from enabling interrupts while in a syscall.
        assert(esp[3] == kKernelCodeSegment &&
               "Expected this task to be triggerred from kernel code.");

        // Although we are in a user task, we triggerred this while in kernel
        // space, so we will want to jump back to the proper kernel esp. This is
        // just the esp value before pushing all interrupt-related values,
        // similar to the handling of a kernel task.
        adjusted_esp = regs->esp + 20;
        CurrentTask->user_in_kernel_space_ = true;
      }
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
    assert(CurrentTask != kMainKernelTask &&
           "We should not manually be quitting the main kernel task.");

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
  }

  task->SetupBeforeTaskRun();
  SwitchPageDirectory(task->getPageDirectory());

  bool first_task_run = task->OnFirstRun();
  task->state_ = RUNNING;

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

void Task::X86TaskRegs::Dump() const {
  DebugPrint(
      "esp: {}\n"
      "ebp: {}\n"
      "eax: {}\n"
      "ebx: {}\n"
      "ecx: {}\n"
      "edx: {}\n"
      "esi: {}\n"
      "edi: {}\n"
      "eflags: {}\n"
      "eip: {}\n"
      "ds: {}\n"
      "es: {}\n"
      "fs: {}\n"
      "gs: {}\n"
      "cs: {}\n",
      print::Hex(esp), print::Hex(ebp), print::Hex(eax), print::Hex(ebx),
      print::Hex(ecx), print::Hex(edx), print::Hex(esi), print::Hex(edi),
      print::Hex(eflags), print::Hex(eip), print::Hex(ds), print::Hex(es),
      print::Hex(fs), print::Hex(gs), print::Hex(cs));
}

void UserTask::SetupBeforeTaskRun() {
  set_kernel_stack(reinterpret_cast<uint32_t>(getEsp0StackPointer()));

  if (OnFirstRun()) {
    // Just perform a check to make sure we will jump to the right place.
    uint32_t stack_val;
    Read(&stack_val, (void *)getRegs().esp, sizeof(stack_val));
    assert(stack_val == USER_START);
  }
}

void DestroyScheduler() {
  assert(ReadyQueue && !ReadyQueue->next &&
         "Expected only the main task to be left.");
  delete ReadyQueue->task;
  kfree(ReadyQueue);
}

void Task::Write(void *this_dst, const void *current_src, size_t size) {
  return TaskMemcpy<CurrentToOther>(*this, *GetCurrentTask(), this_dst,
                                    current_src, size);
}

void Task::Read(void *current_dst, const void *task_src, size_t size) {
  return TaskMemcpy<OtherToCurrent>(*this, *GetCurrentTask(), current_dst,
                                    task_src, size);
}

Task::~Task() {
  assert(child_tasks_.empty());

  // This will only be false for the main kernel task.
  // TODO: Wrap this with an `unlikely`.
  if (parent_task_) parent_task_->RemoveChildTask(*this);
}
