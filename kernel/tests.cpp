#include <ktask.h>
#include <ktests.h>

#include <cassert>

#define PRINT DebugPrint
#include <tests.h>

namespace {

uint32_t RegNum;
void InterruptHandler(X86Registers *regs) { RegNum = regs->int_no; }

TEST(HandleInterrupt) {
  RegNum = 0;
  uint8_t interrupt = 3;
  isr_t old_handler = GetInterruptHandler(interrupt);
  RegisterInterruptHandler(interrupt, InterruptHandler);
  asm volatile("int $0x3");
  ASSERT_EQ(RegNum, 3u);
  RegisterInterruptHandler(interrupt, old_handler);
}

TEST_SUITE(Interrupts) { RUN_TEST(HandleInterrupt); }

void func(void *arg) {
  // Ensure that we increment x 100 times instead of just adding 100.
  volatile auto *x = static_cast<uint32_t *>(arg);
  assert(reinterpret_cast<uintptr_t>(x) % 4 == 0 &&
         "Received misaligned pointer");
  for (int i = 0; i < 100; ++i) ++(*x);
}

void func2(void *arg) {
  volatile auto *x = static_cast<uint32_t *>(arg);
  assert(reinterpret_cast<uintptr_t>(x) % 4 == 0 &&
         "Received misaligned pointer");
  for (int i = 0; i < 200; ++i) ++(*x);
}

TEST(TaskIDs) {
  ASSERT_EQ(GetMainKernelTask()->getID(), 0);

  // We are already in the main kernel task.
  ASSERT_EQ(GetCurrentTask()->getID(), 0);
}

TEST(SimpleTasks) {
  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;
  KernelTask t(func, &val);
  KernelTask t2(func2, &val2);

  for (int i = 0; i < 300; ++i) ++val3;

  t.Join();
  t2.Join();

  ASSERT_EQ(val, 100);
  ASSERT_EQ(val2, 200);
  ASSERT_EQ(val3, 300);
}

void func3(void *arg) {
  volatile auto *x = static_cast<uint32_t *>(arg);
  ++(*x);
  exit_this_task();
  ++(*x);
}

TEST(TaskExit) {
  uint32_t x = 10;
  KernelTask t(func3, &x);

  t.Join();
  ASSERT_EQ(x, 11);
}

TEST(JoinOnDestructor) {
  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;

  {
    KernelTask t(func, &val);
    KernelTask t2(func2, &val2);
    for (int i = 0; i < 300; ++i) ++val3;
  }

  ASSERT_EQ(val, 100);
  ASSERT_EQ(val2, 200);
  ASSERT_EQ(val3, 300);
}

TEST_SUITE(Tasking) {
  RUN_TEST(TaskIDs);
  RUN_TEST(SimpleTasks);
  RUN_TEST(TaskExit);
  RUN_TEST(JoinOnDestructor);

  // TODO: Add test to assert user tasks get different address spaces.
}

TEST(PageFunctions) {
  ASSERT_EQ(kPageSize4M & kPageMask4M, kPageSize4M);
  ASSERT_EQ((kPageSize4M + 1) & kPageMask4M, kPageSize4M);
  ASSERT_EQ((kPageSize4M * 2) & kPageMask4M, kPageSize4M * 2);
  ASSERT_EQ((kPageSize4M - 1) & kPageMask4M, 0);
}

TEST(PagingTest) {
  // We can just write to user here since it is not being used.
  void *phys_addr = GetPhysicalBitmap4M().NextFreePhysicalPage();
  uint32_t page_index = PageIndex4M(phys_addr);
  void *virt_addr = (void *)0xA0000000;
  ASSERT_TRUE(PageDirectory::isPhysicalFree(page_index));

  auto &pd1 = *GetKernelPageDirectory().Clone();
  auto &pd2 = *GetKernelPageDirectory().Clone();

  ASSERT_NE(pd1.get(), pd2.get());
  ASSERT_TRUE(PageDirectory::isPhysicalFree(page_index));

  pd1.AddPage(virt_addr, phys_addr, /*flags=*/0);
  ASSERT_FALSE(PageDirectory::isPhysicalFree(page_index));

  SwitchPageDirectory(pd1);

  // Write enough data such that we can be confident that it doesn not match any
  // uninitialized data that may exist at this address.
  size_t n = 4;
  uint8_t expected = 10;
  memset(virt_addr, expected, n);
  for (size_t i = 0; i < n; ++i) {
    uint8_t val;
    memcpy(&val, (uint8_t *)virt_addr, sizeof(uint8_t));
    ASSERT_EQ(val, expected);
  }

  pd1.ReclaimPageDirRegion();
  pd2.ReclaimPageDirRegion();
  SwitchPageDirectory(GetKernelPageDirectory());

  ASSERT_TRUE(PageDirectory::isPhysicalFree(page_index));
}

void PageFaultHandler(X86Registers *regs) {
  RegNum = regs->int_no;

  // Exit here or we will go back to the instruction that caused the page fault,
  // infinitely jumping back to this function.
  exit_this_task();
}

struct PageFaultData {
  uint32_t val;
  uint32_t addr;
};

void PageFaultTaskFunc(void *arg) {
  auto *x = static_cast<PageFaultData *>(arg);
  x->val = 9;

  uint32_t *ptr = (uint32_t *)x->addr;
  uint32_t do_page_fault = *ptr;
  (void)do_page_fault;

  // Should not reach this.
  x->val = 10;
}

TEST(PageFault) {
  isr_t old_handler = GetInterruptHandler(kPageFaultInterrupt);
  RegisterInterruptHandler(kPageFaultInterrupt, PageFaultHandler);

  PageFaultData x;

  {
    // Test a page fault.
    x.addr = 0xA0000000;
    x.val = 0;
    RegNum = 0;

    KernelTask t(PageFaultTaskFunc, &x);
    t.Join();

    ASSERT_EQ(RegNum, kPageFaultInterrupt);
    ASSERT_EQ(x.val, 9);
  }

  if (!GetKernelPageDirectory().isVirtualMapped(nullptr)) {
    // We should page fault at 0, unless we're using text mode. In this case, we
    // identity map the first page.
    x.addr = 0;
    x.val = 0;
    RegNum = 0;

    KernelTask t(PageFaultTaskFunc, &x);
    t.Join();

    ASSERT_EQ(RegNum, kPageFaultInterrupt);
    ASSERT_EQ(x.val, 9);
  }

  RegisterInterruptHandler(kPageFaultInterrupt, old_handler);
}

TEST_SUITE(Paging) {
  RUN_TEST(PageFunctions);
  RUN_TEST(PagingTest);
  RUN_TEST(PageFault);
}

}  // namespace

void RunTests() {
  test::TestingFramework tests;
  tests.RunSuite(Interrupts);
  tests.RunSuite(Tasking);
  tests.RunSuite(Paging);
}
