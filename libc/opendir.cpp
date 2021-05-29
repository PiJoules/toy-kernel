#include <_syscalls.h>
#include <allocator.h>
#include <dirent.h>
#include <elf.h>
#include <stdio.h>
#include <vfs_helpers.h>

struct DIR {
  // This `dirent` will be updated on calls to `readdir`.
  dirent entry;
  size_t entry_num;

  char *dirname;
};

// FIXME: We currently represent a DIR* as just a char*, but we shouldn't keep
// this permanently. Because DIR is an opaque type, it doesn't really matter
// what its underlying representation is. Many of these implementations are
// slow, but their only purpose is to work for now.
DIR *opendir(const char *dirname) {
  if (const vfs::Directory *dir = GetRootDir().getDir(dirname)) {
    DIR *dirp = (DIR *)malloc(sizeof(DIR));

    // `entry` will be set in `readdir`.
    dirp->entry_num = 0;
    size_t dirname_len = strlen(dirname);
    dirp->dirname = (char *)malloc(dirname_len + 1);
    memcpy(dirp->dirname, dirname, dirname_len);
    dirp->dirname[dirname_len] = 0;

    return dirp;
  }
  return nullptr;
}

// FIXME: Constantly allocating and deallocating is terribly slow. We should
// have some sort of global state that we can access within libc.
dirent *readdir(DIR *dirp) {
  const vfs::Directory &dir = *(GetRootDir().getDir(dirp->dirname));

  // We reached the end.
  if (dirp->entry_num >= dir.getNodes().size()) return NULL;

  const std::unique_ptr<vfs::Node> &node = dir.getNodes()[dirp->entry_num];
  dirp->entry_num++;

  // Get direntry on this node.
  size_t name_len = node->getName().size();
  dirent *entry = &dirp->entry;
  strncpy(entry->d_name, node->getName().c_str(), sizeof(entry->d_name));
  if (name_len >= sizeof(entry->d_name))
    entry->d_name[sizeof(entry->d_name) - 1] = 0;
  else
    entry->d_name[name_len] = 0;
  return entry;
}

void closedir(DIR *dirp) {
  if (dirp) {
    free(dirp->dirname);
    free(dirp);
  }
}
