#include <stdio.h>
#include <unistd.h>
#include <vfs_helpers.h>

char *getcwd(char *buf, size_t size) {
  return strncpy(buf, GetCWD().getName().c_str(), size);
}
