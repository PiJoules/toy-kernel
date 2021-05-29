#include <_syscalls.h>
#include <elf.h>
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

// Split up the string into argv. This function populates argv by pointing to
// strings into a argv_buffer, which will contain  a series of null terminated
// strings copied from argstr. The ending of argv will be indicated with a null
// pointer.
void ArgvFromArgString(const char *argstr, size_t argv_buffer_len,
                       char *argv[ARG_MAX], char argv_buffer[argv_buffer_len]) {
  char c;
  bool parsing_arg = false;
  while ((c = *(argstr++)) != 0) {
    if (isspace(c)) {
      if (parsing_arg) {
        // We found whitespace after parsing an argument. Terminate this
        // argument.
        *(argv_buffer++) = 0;
      }
      parsing_arg = false;
    } else {
      if (!parsing_arg) {
        // This is the first character in an argument we're parsing.
        assert(*argv_buffer && "Should not be pointing to a null terminator.");
        *(argv++) = argv_buffer;
      }
      *(argv_buffer++) = c;
      parsing_arg = true;
    }
  }
  *argv_buffer = 0;
  *argv = nullptr;
}

}  // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  char buffer[kCmdBufferSize];
  while (1) {
    printf("> ");
    DebugRead(buffer);

    size_t bufferlen = strlen(buffer) + 1;
    char *argv[ARG_MAX];
    char argv_buffer[bufferlen];
    ArgvFromArgString(buffer, bufferlen, argv, argv_buffer);
    size_t argc = 0;
    while (argv[argc]) ++argc;
    if (!argc) continue;

    // If it happens to be an executable file, execute it.
    // Note that we can pass `cmd` here because the std::string ctor will parse
    // it as a null-terminated string up to the first argument.
    if (const vfs::File *file = GetRootDir().getFile(argv[0])) {
      LoadElfProgram(file->getContents().data(), GetGlobalEnvInfo(), argc,
                     const_cast<const char **>(argv));
    } else {
      printf("Unknown command '%s'\n", argv[0]);
    }
  }

  return 0;
}
