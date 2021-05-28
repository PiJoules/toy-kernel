#include <_syscalls.h>
#include <allocator.h>
#include <dirent.h>
#include <elf.h>
#include <stdio.h>
#include <vfs_helpers.h>

extern const void *__raw_vfs_data;
extern "C" Handle GetRawVFSDataOwner();

// This assumes the heap is setup to support unique_ptr.
std::unique_ptr<vfs::Directory> ParseUSTARFromRawData() {
  Handle vfs_owner = GetRawVFSDataOwner();

  // The first argument will be a pointer to vfs data in the owner task's
  // address space. Map some virtual address in this task's address space to
  // the page with the vfs so we can read from it and construct a vfs object.
  auto vfs_loc_int = reinterpret_cast<uintptr_t>(__raw_vfs_data);
  uint32_t loc_offset = vfs_loc_int % kPageSize4M;
  auto vfs_loc_page = vfs_loc_int - loc_offset;
  uint8_t *this_vfs_page;
  sys_share_page(vfs_owner, reinterpret_cast<void **>(&this_vfs_page),
                 reinterpret_cast<void *>(vfs_loc_page));
  uint8_t *this_vfs = this_vfs_page + loc_offset;

  // First get the size and do a santity check that the remainder of the vfs
  // data is still on the page we mapped. Otherwise, we'll need to map another
  // page.
  size_t initrd_size;
  memcpy(&initrd_size, this_vfs, sizeof(initrd_size));
  assert(loc_offset + initrd_size <= kPageSize4M &&
         "FIXME: Will need to allocate more pages. Initrd does not fit in one "
         "page.");

  // Everything is mapped up. We can safely use this vfs pointer.
  uint8_t *initrd_data = this_vfs + sizeof(initrd_size);

  std::unique_ptr<vfs::Directory> vfs =
      vfs::ParseUSTAR(initrd_data, initrd_size);

  return vfs;
}

struct DIR {
  dirent entry;
  size_t entry_num;

  char *dirname;
};

// FIXME: We currently represent a DIR* as just a char*, but we shouldn't keep
// this permanently. Because DIR is an opaque type, it doesn't really matter
// what its underlying representation is. Many of these implementations are
// slow, but their only purpose is to work for now.
DIR *opendir(const char *dirname) {
  auto vfs = ParseUSTARFromRawData();
  if (const vfs::Directory *dir = vfs->getDir(dirname)) {
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
  auto vfs = ParseUSTARFromRawData();
  const vfs::Directory &dir = *(vfs->getDir(dirp->dirname));

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
