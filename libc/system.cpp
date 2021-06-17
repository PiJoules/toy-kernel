#include <ctype.h>
#include <elf.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <vfs.h>
#include <vfs_helpers.h>

namespace {

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

int system(const char *cmd) {
  size_t cmdlen = strlen(cmd) + 1;
  char *argv[ARG_MAX];
  char argv_buffer[cmdlen];
  ArgvFromArgString(cmd, cmdlen, argv, argv_buffer);
  size_t argc = 0;
  while (argv[argc]) ++argc;
  if (!argc) return 0;

  // If it happens to be an executable file, execute it.
  // Note that we can pass `cmd` here because the std::string ctor will parse
  // it as a null-terminated string up to the first argument.
  if (const vfs::File *file = GetRootDir().getFile(argv[0])) {
    LoadElfProgram(file->getContents().data(), GetGlobalEnvInfo(), argc,
                   const_cast<const char **>(argv));
  } else {
    printf("Unknown command '%s'\n", argv[0]);
    return -1;
  }

  return 0;
}
