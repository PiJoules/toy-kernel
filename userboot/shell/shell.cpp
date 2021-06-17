#include <_syscalls.h>
#include <elf.h>
#include <unistd.h>
#include <vfs.h>
#include <vfs_helpers.h>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

// FIXME: We would at some point like to accept commands longer than 1024
// characters.
constexpr const size_t kCmdBufferSize = 1024;
constexpr const char CR = 13;  // Carriage return

// Store typed characters into a buffer while also saving the command until the
// next ENTER.
void DebugRead(char *buffer) {
  while (1) {
    int c = getchar();
    assert(c != EOF);
    if (c == CR) {
      *buffer = 0;
      putchar('\n');
      return;
    }

    *(buffer++) = static_cast<char>(c);
    putchar(c);
  }
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  char buffer[kCmdBufferSize];

  GetRootDir().Dump();

  while (1) {
    // FIXME: Remove this magic number. Since USTAR only supports paths up to
    // 256 characters long, this will be the buffer.
    char cwd_buf[256];
    char *cwd = getcwd(cwd_buf, sizeof(cwd));
    assert(cwd && "Could not get current working directory.");
    cwd[sizeof(cwd_buf) - 1] = 0;
    printf("%s$ ", cwd_buf);
    DebugRead(buffer);

    system(buffer);
  }

  return 0;
}
