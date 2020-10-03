#include <Terminal.h>
#include <bitvector.h>
#include <function_traits.h>
#include <initializer_list.h>
#include <isr.h>
#include <iterable.h>
#include <kassert.h>
#include <kmalloc.h>
#include <knew.h>
#include <kstring.h>
#include <ktask.h>
#include <string.h>
#include <tests.h>
#include <unique.h>
#include <vector.h>

namespace {

using toy::BitVector;
using toy::MakeUnique;
using toy::String;
using toy::Unique;
using toy::Vector;

TEST(IntegerPower) {
  for (uint32_t p = 0, expected = 1; p < 31; ++p, expected <<= 1) {
    ASSERT_EQ(ipow2<uint32_t>(p), expected);
  }
}

TEST_SUITE(KernelFunctions) { RUN_TEST(IntegerPower); }

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
// I ran into one when allocating tasks.
TEST(TaskAllocation) {
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

TEST(Realloc) {
  size_t heap_used = GetKernelHeapUsed();
  size_t init_size = 10;
  void *ptr = kmalloc(init_size);
  ASSERT_EQ(GetKernelHeapUsed(), heap_used + 10 + sizeof(MallocHeader));

  // Re-usable pointer for size 0 realloc.
  ASSERT_EQ(krealloc(ptr, 0), nullptr);
  auto *chunk = MallocHeader::FromPointer(ptr);
  ASSERT_EQ(chunk->used, 1);
  ASSERT_EQ(chunk->size, init_size + sizeof(MallocHeader));

  // Same size allocation.
  ASSERT_EQ(krealloc(ptr, init_size), ptr);

  // Increased size. It may or may not be the same pointer though.
  size_t newsize = 1024;
  void *newptr = krealloc(ptr, newsize);
  ASSERT_NE(newptr, ptr);
  chunk = MallocHeader::FromPointer(ptr);
  auto *chunk2 = MallocHeader::FromPointer(newptr);
  ASSERT_EQ(chunk->used, 0);
  ASSERT_EQ(chunk2->used, 1);
  ASSERT_EQ(chunk2->size, newsize + sizeof(MallocHeader));

  // Decrease size. This is a large enough decrease that the pointer may not
  // change.
  ptr = krealloc(newptr, init_size);
  ASSERT_EQ(ptr, newptr);
  ASSERT_EQ(chunk2->used, 1);
  ASSERT_EQ(chunk2->size, init_size + sizeof(MallocHeader));

  // Decrease enough such that it can't be split into two chunks. This will lead
  // to a new pointer.
  newptr = krealloc(ptr, init_size - 1);
  ASSERT_NE(newptr, ptr);
  chunk = MallocHeader::FromPointer(ptr);
  ASSERT_EQ(chunk->used, 0);
  chunk2 = MallocHeader::FromPointer(newptr);
  ASSERT_EQ(chunk2->used, 1);
  ASSERT_EQ(chunk2->size, init_size - 1 + sizeof(MallocHeader));

  kfree(newptr);
  ASSERT_EQ(heap_used, GetKernelHeapUsed());
}

TEST(ReallocDataCopied) {
  size_t s = 10;
  size_t s2 = 1000;
  char *ptr = toy::kmalloc<char>(s);
  memset(ptr, 'a', s);
  char *ptr2 = toy::krealloc<char>(ptr, s2);
  ASSERT_NE(ptr2, nullptr);
  ASSERT_NE(ptr, ptr2);
  for (int i = 0; i < s; ++i) ASSERT_EQ(ptr2[i], 'a');
  kfree(ptr2);
}

TEST_SUITE(Malloc) {
  RUN_TEST(MinAllocation);
  RUN_TEST(MoreThanMinAllocation);
  RUN_TEST(MultipleAllocations);
  RUN_TEST(TaskAllocation);
  RUN_TEST(Alignment);
  RUN_TEST(Realloc);
  RUN_TEST(ReallocDataCopied);
}

TEST(CallocTest) {
  auto *ptr = reinterpret_cast<int *>(kcalloc(4, sizeof(int)));
  for (int i = 0; i < 4; ++i) ASSERT_EQ(ptr[i], 0);
  kfree(ptr);
  ptr = toy::kcalloc<int>(4);
  for (int i = 0; i < 4; ++i) ASSERT_EQ(ptr[i], 0);
  kfree(ptr);
}

TEST_SUITE(Calloc) { RUN_TEST(CallocTest); }

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
  size_t stack_size = 256;
  uint32_t *stack = toy::kmalloc<uint32_t>(stack_size);
  uint32_t *stack2 = toy::kmalloc<uint32_t>(stack_size);

  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;
  Task t(func, &val, stack);
  Task t2(func2, &val2, stack2);

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
  size_t stack_size = 256;
  uint32_t *stack = toy::kmalloc<uint32_t>(stack_size);

  uint32_t x = 10;
  Task t(func3, &x, stack);

  t.Join();
  ASSERT_EQ(x, 11);
}

TEST(DefaultStackAllocation) {
  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;
  Task t(func, &val);
  Task t2(func2, &val2);

  for (int i = 0; i < 300; ++i) ++val3;

  t.Join();
  t2.Join();

  ASSERT_EQ(val, 100);
  ASSERT_EQ(val2, 200);
  ASSERT_EQ(val3, 300);
}

TEST(JoinOnDestructor) {
  uint32_t val = 0;
  uint32_t val2 = 0;
  volatile uint32_t val3 = 0;

  {
    Task t(func, &val);
    Task t2(func2, &val2);
    for (int i = 0; i < 300; ++i) ++val3;
  }

  ASSERT_EQ(val, 100);
  ASSERT_EQ(val2, 200);
  ASSERT_EQ(val3, 300);
}

TEST(UserTasks) {
  uint32_t val = 0;
  uint32_t val2 = 0;
  Task t = Task::CreateUserTask(func, &val);
  Task t2 = Task::CreateUserTask(func2, &val2);

  ASSERT_NE(t.getPageDirectory().get(), t2.getPageDirectory().get());
}

TEST_SUITE(Tasking) {
  RUN_TEST(TaskIDs);
  RUN_TEST(SimpleTasks);
  RUN_TEST(TaskExit);
  RUN_TEST(DefaultStackAllocation);
  RUN_TEST(JoinOnDestructor);
  RUN_TEST(UserTasks);
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

  pd1.AddPage(virt_addr, phys_addr, PG_USER);
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

void PageFaultHandler(registers_t *regs) {
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

    Task t(PageFaultTaskFunc, &x);
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

    Task t(PageFaultTaskFunc, &x);
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

TEST(PushBack) {
  Vector<int> v;
  ASSERT_TRUE(v.empty());
  v.push_back(1);
  ASSERT_EQ(v.size(), 1);
  ASSERT_EQ(v[0], 1);
  v.push_back(2);
  v.push_back(3);
  ASSERT_EQ(v.size(), 3);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);

  Vector<int, /*InitCapacity=*/1> v2;
  v2.push_back(1);
  v2.push_back(10);  // Triggers reallocation.
  ASSERT_EQ(v2.size(), 2);
  ASSERT_EQ(v2[0], 1);
  ASSERT_EQ(v2[1], 10);
}

struct S {
  static int copy_ctor_calls;
  S() {}
  S(const S &) { ++copy_ctor_calls; }
};
int S::copy_ctor_calls = 0;

TEST_SUITE(VectorRangeCtor) {
  // Under capacity
  int x[] = {1, 2, 3};
  Vector<int, /*InitCapacity=*/8> v(x, x + 3);
  ASSERT_EQ(v.size(), 3);
  ASSERT_EQ(v[0], 1);
  ASSERT_EQ(v[1], 2);
  ASSERT_EQ(v[2], 3);

  // More than capacity
  Vector<int, 1> v2(x, x + 3);
  ASSERT_EQ(v2.size(), 3);
  ASSERT_EQ(v2[0], 1);
  ASSERT_EQ(v2[1], 2);
  ASSERT_EQ(v2[2], 3);

  // Ensure constructors are called
  S s[] = {S(), S(), S()};
  Vector<S> v3(s, s + 3);
  ASSERT_EQ(v2.size(), 3);
  ASSERT_EQ(S::copy_ctor_calls, 3);
}

TEST(VectorElemDtors) {
  int dtor_calls = 0;
  struct S {
    int &x;
    S(int &x) : x(x) {}
    S(const S &other) : x(other.x) {}
    ~S() { ++x; }
  };

  S s(dtor_calls);
  {
    Vector<S> v;
    v.push_back(s);
  }
  ASSERT_EQ(dtor_calls, 1);
}

TEST(VectorRange) {
  int x[] = {0, 1, 2};
  bool found[] = {false, false, false};
  Vector<int> v(x, x + 3);
  for (auto i : v) { found[i] = true; }
  ASSERT_TRUE(found[0]);
  ASSERT_TRUE(found[1]);
  ASSERT_TRUE(found[2]);
}

TEST(VectorMove) {
  size_t heap_used = GetKernelHeapUsed();

  {
    // Empty vector move.
    Vector<Unique<int>> v;
    Vector<Unique<int>> v2(toy::move(v));
  }
  ASSERT_EQ(heap_used, GetKernelHeapUsed());

  {
    // Handful of elements.
    Vector<Unique<int>> v;
    v.push_back(MakeUnique<int>(1));
    v.push_back(MakeUnique<int>(2));
    v.push_back(MakeUnique<int>(3));

    Vector<Unique<int>> v2(toy::move(v));
    ASSERT_TRUE(v.empty());
    ASSERT_EQ(v2.size(), 3);

    // Accesses to `v` are undefined.
    ASSERT_EQ(*v2[0], 1);
    ASSERT_EQ(*v2[1], 2);
    ASSERT_EQ(*v2[2], 3);
  }
  ASSERT_EQ(heap_used, GetKernelHeapUsed());

  {
    // From a moved reference
    Vector<Unique<int>> v;
    v.push_back(MakeUnique<int>(1));
    v.push_back(MakeUnique<int>(2));
    v.push_back(MakeUnique<int>(3));

    auto &vref = v;

    Vector<Unique<int>> v2(toy::move(vref));
    ASSERT_TRUE(v.empty());
    ASSERT_TRUE(vref.empty());
    ASSERT_EQ(v2.size(), 3);

    // Accesses to `v` are undefined.
    ASSERT_EQ(*v2[0], 1);
    ASSERT_EQ(*v2[1], 2);
    ASSERT_EQ(*v2[2], 3);
  }
  ASSERT_EQ(heap_used, GetKernelHeapUsed());
}

TEST_SUITE(VectorSuite) {
  RUN_TEST(PushBack);
  RUN_TEST(VectorRangeCtor);
  RUN_TEST(VectorElemDtors);
  RUN_TEST(VectorRange);
  RUN_TEST(VectorMove);
}

[[maybe_unused]] float FreeFunc(int a, char b) { return a + b; }

TEST(FunctionReturnType) {
  {
    using Traits = toy::FunctionTraits<decltype(FreeFunc)>;
    static_assert(toy::is_same<Traits::return_type, float>::value, "");
    static_assert(Traits::num_args == 2, "");
    static_assert(toy::is_same<Traits::argument<0>::type, int>::value, "");
    static_assert(toy::is_same<Traits::argument<1>::type, char>::value, "");
  }

  struct A {
    int func(char) { return 0; }
  };
  {
    using Traits = toy::FunctionTraits<decltype(&A::func)>;
    static_assert(toy::is_same<Traits::return_type, int>::value, "");
    static_assert(Traits::num_args == 2, "");
    static_assert(toy::is_same<Traits::argument<0>::type, A &>::value, "");
    static_assert(toy::is_same<Traits::argument<1>::type, char>::value, "");
  }
}

TEST_SUITE(TypeTraits) { RUN_TEST(FunctionReturnType); }

TEST(EnumerateIterator) {
  Vector<int> v({1, 2, 3});
  int i = 0;
  for (auto p : toy::Iterable(v)) {
    ++i;
    ASSERT_EQ(p, i);
  }
  ASSERT_EQ(i, 3);

  i = 0;
  for (auto p : toy::Enumerate(v)) {
    ASSERT_EQ(p.first, i);
    ++i;
    ASSERT_EQ(p.second, i);
  }
  ASSERT_EQ(i, 3);
}

TEST_SUITE(Iterators) { RUN_TEST(EnumerateIterator); }

template <typename T>
void CheckInitializerList(const T &l) {
  ASSERT_EQ(l[0], 1);
  ASSERT_EQ(l[1], 2);
  ASSERT_EQ(l[2], 3);
}

TEST(InitializerListTest) {
  // We can only have an implicit list-initialization to initializer_list
  // conversion if the type is explicitly "std::initializer_list".
  auto fn = [](std::initializer_list<int> l) { CheckInitializerList(l); };
  fn({1, 2, 3});

  // Check using directive.
  using std::initializer_list;
  auto fn2 = [](initializer_list<int> l) { CheckInitializerList(l); };
  fn2({1, 2, 3});

  // Check alias.
  auto fn3 = [](toy::InitializerList<int> l) { CheckInitializerList(l); };
  fn3({1, 2, 3});

  toy::InitializerList<int> l({1, 2, 3});
  CheckInitializerList(l);

  toy::InitializerList<int> l2 = {1, 2, 3};
  CheckInitializerList(l2);
}

TEST_SUITE(InitializerListSuite) { RUN_TEST(InitializerListTest); }

TEST(UniqueTest) {
  size_t heapused = GetKernelHeapUsed();
  {
    Unique<int> u;
    ASSERT_FALSE(u);

    Unique<int> u2(new int(1));
    ASSERT_TRUE(u2);
    ASSERT_EQ(*u2, 1);

    // Assignment
    u = toy::move(u2);
    ASSERT_TRUE(u);
    ASSERT_FALSE(u2);

    int *i = u.release();
    ASSERT_FALSE(u);
    ASSERT_EQ(*i, 1);

    // Swapping
    Unique<int> u3(i);
    ASSERT_EQ(*u3, 1);
    Unique<int> u4(new int(4));
    u3.swap(u4);
    ASSERT_EQ(*u3, 4);
    ASSERT_EQ(*u4, 1);

    // Copy construction.
    Unique<int> u5(toy::move(u4));
    ASSERT_TRUE(u5);
    ASSERT_FALSE(u4);
    ASSERT_EQ(*u5, 1);
  }

  // Everything should be free'd afterwards.
  ASSERT_EQ(GetKernelHeapUsed(), heapused);
}

TEST(DestructorCalled) {
  bool called = false;
  struct A {
    bool &b_;
    A(bool &b) : b_(b) {}
    ~A() { b_ = true; }
  };

  {
    // At normal scope exit.
    Unique<A> a(new A(called));
  }
  ASSERT_TRUE(called);

  called = false;
  {
    Unique<A> a(new A(called));
    {
      // Swapping.
      bool junk;
      Unique<A> b(new A(junk));

      // Swap the pointers, so once `b` goes out of scope, `a`'s destructor will
      // be called.
      a.swap(b);
    }
    ASSERT_TRUE(called);
  }

  called = false;
  {
    Unique<A> a(new A(called));
    {
      // Moving.
      bool junk;
      Unique<A> b(new A(junk));

      // Destructor for `b` is called after this move.
      a = toy::move(b);
      ASSERT_TRUE(called);
    }
  }

  called = false;
  {
    Unique<A> a(new A(called));
    {
      // Copy constructing.
      // Destructor for `a` is called after b exist scope.
      Unique<A> b(toy::move(a));
    }
    ASSERT_TRUE(called);
  }
}

TEST_SUITE(UniqueSuite) {
  RUN_TEST(UniqueTest);
  RUN_TEST(DestructorCalled);
}

TEST(StringTest) {
  String s;
  ASSERT_TRUE(s.empty());
  s.push_back('c');
  ASSERT_EQ(s[0], 'c');

  s.push_back('a');
  ASSERT_STREQ(s.c_str(), "ca");
}

TEST(StringConstruction) {
  String s("abc");
  ASSERT_EQ(s.size(), 3);
  ASSERT_STREQ(s.c_str(), "abc");

  String s2("abc", /*init_capacity=*/1);
  ASSERT_EQ(s2.size(), 3);
  ASSERT_STREQ(s2.c_str(), "abc");
}

TEST_SUITE(StringSuite) {
  RUN_TEST(StringTest);
  RUN_TEST(StringConstruction);
}

TEST(BitVectorTest) {
  BitVector v;
  ASSERT_TRUE(v.empty());

  for (auto i = 0; i < 32; ++i) {
    v.push_back(1);
    ASSERT_EQ(v.size(), i + 1);
    ASSERT_TRUE(v.get(i));
    ASSERT_EQ((UINT64_C(1) << (i + 1)) - 1, v.getAs<uint64_t>());
  }

  ASSERT_EQ(v.size(), 32);
  ASSERT_EQ(v.getAs<uint32_t>(), UINT32_MAX);

  v.push_back(0);
  ASSERT_EQ(v.size(), 33);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT32_MAX);
  v.set(32, 0);
  ASSERT_EQ(v.size(), 33);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT32_MAX);
  v.set(32, 1);
  ASSERT_EQ(v.getAs<uint64_t>(), (static_cast<uint64_t>(UINT32_MAX) << 1) + 1);

  for (auto i = 33; i < 63; ++i) {
    v.push_back(1);
    ASSERT_EQ(v.size(), i + 1);
    ASSERT_TRUE(v.get(i));
    ASSERT_EQ((UINT64_C(1) << (i + 1)) - 1, v.getAs<uint64_t>());
  }

  ASSERT_EQ(v.size(), 63);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT64_MAX >> 1);

  v.push_back(1);
  ASSERT_EQ(v.size(), 64);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT64_MAX);

  // Cannot fit into 64 bits anymore.
  v.push_back(1);
  ASSERT_EQ(v.size(), 65);

  for (int i = 64; i >= 0; --i) {
    ASSERT_TRUE(v.get(i));
    v.set(i, 0);
    ASSERT_FALSE(v.get(i));
  }

  ASSERT_EQ(v.size(), 65);

  v.pop_back();
  ASSERT_EQ(v.size(), 64);
  ASSERT_EQ(v.getAs<uint64_t>(), 0);
  v.pop_back();
  v.push_back(1);
  ASSERT_EQ(v.getAs<uint64_t>(), UINT64_C(1) << 63);
}

TEST_SUITE(BitVectorSuite) { RUN_TEST(BitVectorTest); }

}  // namespace

void RunTests() {
  TestingFramework tests;
  tests.RunSuite(KernelFunctions);
  tests.RunSuite(KernelString);
  tests.RunSuite(Printing);
  tests.RunSuite(Interrupts);
  tests.RunSuite(Malloc);
  tests.RunSuite(Calloc);
  tests.RunSuite(Tasking);
  tests.RunSuite(Paging);
  tests.RunSuite(CppLangFeatures);
  tests.RunSuite(VectorSuite);
  tests.RunSuite(UniqueSuite);
  tests.RunSuite(StringSuite);
  tests.RunSuite(InitializerListSuite);
  tests.RunSuite(Iterators);
  tests.RunSuite(TypeTraits);
  tests.RunSuite(BitVectorSuite);
}
