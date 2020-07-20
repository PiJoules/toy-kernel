#ifndef TESTS_H_
#define TESTS_H_

#include <Terminal.h>

/**
 * Small testing framework for this kernel.
 */

namespace {

using SetupFunc = void (*)();
using TeardownFunc = void (*)();

SetupFunc TestingSetup = nullptr;
TeardownFunc TestingTeardown = nullptr;
int32_t NumFailures;
bool TestFailed = false;

#define TEST(Test) static void Test()
#define TEST_SUITE(Suite) static void Suite()
#define SETUP(Setup) \
  { TestingSetup = Setup; }
#define RUN_TEST(TestName)                                      \
  {                                                             \
    if (TestingSetup) TestingSetup();                           \
    TestFailed = false;                                         \
    terminal::Write(STR(TestName) " ... ");                     \
    TestName();                                                 \
    terminal::WriteF("{}\n", TestFailed ? "FAILED" : "PASSED"); \
    if (TestingTeardown) TestingTeardown();                     \
  }

#define ASSERT_STREQ(s1, s2)                                                   \
  {                                                                            \
    if (!AssertStrEqual(s1, s2, STR(s1), STR(s2), __FILE__, __LINE__)) return; \
  }

#define ASSERT_EQ(val1, val2)                                               \
  {                                                                         \
    if (!AssertEqual(val1, val2, STR(val1), STR(val2), __FILE__, __LINE__)) \
      return;                                                               \
  }

class TestingFramework {
 public:
  TestingFramework() {
    NumFailures = 0;
    terminal::Write("\nRunning tests...\n");
  }

  ~TestingFramework() {
    if (NumFailures)
      terminal::WriteF("{} tests failed\n\n", NumFailures);
    else
      terminal::Write("All tests passed!\n\n");
    while (NumFailures) {}
  }

  using TestSuite = void (*)();

  void RunSuite(TestSuite suite) {
    TestingSetup = nullptr;
    TestingTeardown = nullptr;
    suite();
  }
};

inline bool AssertStrEqual(const char *found, const char *expected,
                           const char *found_expr, const char *expected_expr,
                           const char *file, int line) {
  if (strcmp(found, expected) == 0) return true;
  terminal::WriteF("Strings are not equal {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is:\n", found_expr);
  terminal::WriteF("  {}\n\n", found);
  terminal::WriteF("Expected `{}` which is:\n", expected_expr);
  terminal::WriteF("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

template <typename T1, typename T2>
bool AssertEqual(T1 found, T2 expected, const char *found_expr,
                 const char *expected_expr, const char *file, int line) {
  if (found == expected) return true;
  terminal::WriteF("Values are not equal {}:{}\n", file, line);
  terminal::WriteF("Found `{}` which is:\n", found_expr);
  terminal::WriteF("  {}\n\n", found);
  terminal::WriteF("Expected `{}` which is:\n", expected_expr);
  terminal::WriteF("  {}\n\n", expected);
  ++NumFailures;
  TestFailed = true;
  return false;
}

}  // namespace

#endif
