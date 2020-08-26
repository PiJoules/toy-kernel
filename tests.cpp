#include <Terminal.h>
#include <isr.h>
#include <kassert.h>
#include <kmalloc.h>
#include <knew.h>
#include <kstring.h>
#include <kthread.h>
#include <tests.h>

namespace {

TEST(Memset) {
  size_t size = 10;
  char buffer[size];
  buffer[9] = 9;
  char expected = 'a';
  memset(buffer, expected, size);
  for (unsigned i = 0; i < size; ++i) ASSERT_EQ(buffer[i], expected);
}

TEST(Memcpy) {
  size_t size = 10;
  char buffer[size], buffer2[size];
  memset(buffer, 1, size);
  memset(buffer2, 2, size);
  memcpy(buffer2, buffer, size);
  for (unsigned i = 0; i < size; ++i) ASSERT_EQ(buffer2[i], 1);
}

TEST(Strlen) {
  char str[] = "buffer\0\0";
  ASSERT_EQ(strlen(str), 6u);
}

TEST(Strcmp) {
  size_t size = 10;
  char str1[size], str2[size];
  str1[size - 1] = 0;
  str2[size - 1] = 0;
  memset(str1, 'a', size - 1);
  memset(str2, 'b', size - 1);

  str2[0] = 'a';
  ASSERT_EQ(strcmp(str1, str2), -1);

  str1[0] = 'c';
  ASSERT_EQ(strcmp(str1, str2), 2);

  memset(str1, 'b', size - 1);
  memset(str2, 'b', size - 1);
  ASSERT_EQ(strcmp(str1, str2), 0);
}

TEST_SUITE(KernelString) {
  RUN_TEST(Memset);
  RUN_TEST(Memcpy);
  RUN_TEST(Strlen);
  RUN_TEST(Strcmp);
}

char Buffer[1024];
char *BufferPtr = Buffer;

void PutBuffer(char c) {
  *BufferPtr = c;
  ++BufferPtr;
}

void PrintingSetup() {
  BufferPtr = Buffer;
  memset(Buffer, 0, sizeof(Buffer) * sizeof(char));
}

TEST(StringFormatting) {
  terminal::WriteF(PutBuffer, "hello {}", "world");
  ASSERT_STREQ(Buffer, "hello world");
}

TEST(IntFormatting) {
  terminal::WriteF(PutBuffer, "12{}4", 3);
  ASSERT_STREQ(Buffer, "1234");
}

TEST(UnsignedFormatting) {
  terminal::WriteF(PutBuffer, "12{}4", 3u);
  ASSERT_STREQ(Buffer, "1234");
}

TEST_SUITE(Printing) {
  SETUP(PrintingSetup);
  RUN_TEST(StringFormatting);
  RUN_TEST(IntFormatting);
  RUN_TEST(UnsignedFormatting);
}

uint32_t RegNum;
void InterruptHandler(registers_t *regs) { RegNum = regs->int_no; }

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

TEST(MinAllocation) {
  size_t heap_used = GetKernelHeapUsed();
  size_t size = sizeof(char) * 4;
  auto *str = static_cast<char *>(kmalloc(size));

  // Check allocation size.
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + size + sizeof(MallocHeader));

  // Make sure we can read and write expectedly.
  str[0] = 'a';
  str[1] = 'b';
  str[2] = 'c';
  str[3] = '\0';
  ASSERT_STREQ(str, "abc");

  // Should be freed.
  kfree(str);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);
}

TEST(MoreThanMinAllocation) {
  size_t heap_used = GetKernelHeapUsed();
  size_t size = kMallocMinSize * 2;
  auto *str = static_cast<char *>(kmalloc(sizeof(char) * size));

  // We get the amount we request + the malloc header.
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + size + sizeof(MallocHeader));

  // Make sure we can write to all of it.
  memset(str, 'a', size);

  kfree(str);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);
}

TEST(MultipleAllocations) {
  size_t size = kMallocMinSize;
  size_t heap_used = GetKernelHeapUsed();
  void *buf1 = kmalloc(size);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + (size + sizeof(MallocHeader)));
  void *buf2 = kmalloc(size);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + (size + sizeof(MallocHeader)) * 2);
  void *buf3 = kmalloc(size);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + (size + sizeof(MallocHeader)) * 3);

  memset(buf1, 'a', size);
  memset(buf2, 'a', size);
  memset(buf3, 'a', size);

  kfree(buf1);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + (size + sizeof(MallocHeader)) * 2);
  kfree(buf2);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + (size + sizeof(MallocHeader)));
  kfree(buf3);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);
}

// This is a regression test to make sure I don't run into a bug in kmalloc that
// I ran into one when allocating threads.
TEST(ThreadAllocation) {
  size_t size = 32;

  auto heap_used = GetKernelHeapUsed();
  auto *alloc1 = toy::kmalloc<uint8_t>(size);

  // Size of allocation + size of malloc header
  ASSERT_EQ(MallocHeader::FromPointer(alloc1)->size,
            size + sizeof(MallocHeader));
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + size + sizeof(MallocHeader));

  uint8_t size2 = 1;
  auto *alloc2 = toy::kmalloc<uint8_t>(size2);

  ASSERT_EQ(MallocHeader::FromPointer(alloc2)->size,
            size2 + sizeof(MallocHeader));
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + (size + sizeof(MallocHeader)) +
                                     (size2 + sizeof(MallocHeader)));

  kfree(alloc1);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + size2 + sizeof(MallocHeader));
  kfree(alloc2);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);
}

TEST(Alignment) {
  void *x = kmalloc(1000);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(x) % kMaxAlignment, 0);
  kfree(x);

  uint32_t alignment = 4096;
  x = kmalloc(alignment * 2, alignment);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(x) % alignment, 0);
  kfree(x);
}

TEST_SUITE(Malloc) {
  RUN_TEST(MinAllocation);
  RUN_TEST(MoreThanMinAllocation);
  RUN_TEST(MultipleAllocations);
  RUN_TEST(ThreadAllocation);
  RUN_TEST(Alignment);
}

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

TEST(SimpleThreads) {
  size_t stack_size = 256;
  uint32_t *stack = toy::kmalloc<uint32_t>(stack_size);
  uint32_t *stack2 = toy::kmalloc<uint32_t>(stack_size);

  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;
  Thread t(func, &val, stack);
  Thread t2(func2, &val2, stack2);

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
  exit_this_thread();
  ++(*x);
}

TEST(ThreadExit) {
  size_t stack_size = 256;
  uint32_t *stack = toy::kmalloc<uint32_t>(stack_size);

  uint32_t x = 10;
  Thread t(func3, &x, stack);

  t.Join();
  ASSERT_EQ(x, 11);
}

TEST(DefaultStackAllocation) {
  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;
  Thread t(func, &val);
  Thread t2(func2, &val2);

  for (int i = 0; i < 300; ++i) ++val3;

  t.Join();
  t2.Join();

  ASSERT_EQ(val, 100);
  ASSERT_EQ(val2, 200);
  ASSERT_EQ(val3, 300);
}

TEST_SUITE(Threading) {
  RUN_TEST(SimpleThreads);
  RUN_TEST(ThreadExit);
  RUN_TEST(DefaultStackAllocation);
}

void PageFaultHandler(registers_t *regs) {
  RegNum = regs->int_no;

  // Exit here or we will go back to the instruction that caused the page fault,
  // infinitely jumping back to this function.
  exit_this_thread();
}

void PageFaultThreadFunc(void *arg) {
  auto *x = static_cast<uint32_t *>(arg);
  *x = 9;

  uint32_t *ptr = (uint32_t *)0xA0000000;
  uint32_t do_page_fault = *ptr;
  (void)do_page_fault;

  // Should not reach this.
  *x = 10;
}

TEST(PageFault) {
  RegNum = 0;
  uint8_t interrupt = 14;
  isr_t old_handler = GetInterruptHandler(interrupt);
  RegisterInterruptHandler(interrupt, PageFaultHandler);

  uint32_t *stack = toy::kmalloc<uint32_t>(256);

  uint32_t x = 0;
  Thread t(PageFaultThreadFunc, &x, stack);

  t.Join();

  ASSERT_EQ(RegNum, 14u);
  ASSERT_EQ(x, 9);
  RegisterInterruptHandler(interrupt, old_handler);
}

TEST_SUITE(PageFaultSuite) { RUN_TEST(PageFault); }

TEST(VirtualMethods) {
  struct A {
    virtual int get() const { return 1; }
  };
  struct B : public A {
    int get() const override { return 2; }
  };
  struct C : public A {
    int get() const override { return 3; }
  };
  A a;
  B b;
  C c;
  A *aptr = &a;
  ASSERT_EQ(aptr->get(), 1);

  aptr = &b;
  ASSERT_EQ(aptr->get(), 2);

  aptr = &c;
  ASSERT_EQ(aptr->get(), 3);

  struct D {
    // __cxa_pure_virtual will need to be defined.
    virtual int get() const = 0;
  };
  struct E : public D {
    int get() const override { return 1; }
  };
  E e;
  D *dptr = &e;
  ASSERT_EQ(dptr->get(), 1);
}

TEST(New) {
  struct A {
    int32_t x, y;
  };

  size_t heap_used = GetKernelHeapUsed();
  A *a = new A;
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + sizeof(A) + sizeof(MallocHeader));
  delete a;
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);

  struct B {
    int32_t x;
    alignas(1024) int32_t arr[2];
  };
  B *b = new B;  // void *operator new(size_t size, std::align_val_t alignment)
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + sizeof(B) + sizeof(MallocHeader));
  delete b;
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);

  struct C {
    uint8_t x, y;
  };
  void *buffer = kmalloc(sizeof(C));
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + sizeof(C) + sizeof(MallocHeader));

  C *c = new (buffer) C;
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + sizeof(C) + sizeof(MallocHeader));

  c->x = 1;
  c->y = 2;
  C *casted = static_cast<C *>(buffer);
  ASSERT_EQ(casted->x, 1);
  ASSERT_EQ(casted->y, 2);

  // Freed after a normal delete.
  delete c;
  ASSERT_EQ(GetKernelHeapUsed(), heap_used);
}

TEST_SUITE(CppLangFeatures) {
  RUN_TEST(VirtualMethods);
  RUN_TEST(New);
}

}  // namespace

void RunTests() {
  TestingFramework tests;
  tests.RunSuite(KernelString);
  tests.RunSuite(Printing);
  tests.RunSuite(Interrupts);
  tests.RunSuite(Malloc);
  tests.RunSuite(Threading);
  tests.RunSuite(PageFaultSuite);
  tests.RunSuite(CppLangFeatures);
}
