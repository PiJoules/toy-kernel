#ifndef VFS_H_
#define VFS_H_

// FIXME: We should eventually move this into a separate directory so that both
// the kernel and user applications can share this for reading off the initial
// ramdisk.

#include <BitVector.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

namespace vfs {

constexpr size_t kFilenameSize = 64;

struct Header {
  uint32_t id;
  uint32_t flags;
  char name[kFilenameSize];

  Header(uint32_t id, bool isfile, const char name[kFilenameSize]);

  bool isFile() const { return flags & 1; }
  bool isDir() const { return !isFile(); }
};
static_assert(sizeof(Header) == 72,
              "VFS header size changed! Make sure this is expected.");

struct File;
struct Directory;

struct Node {
  Header header;

  Node(uint32_t id, bool isdir, const char name[kFilenameSize]);
  virtual ~Node() {}

  File &AsFile();
  const File &AsFile() const;
  Directory &AsDir();
  const Directory &AsDir() const;
  void Dump() const;

 private:
  void DumpImpl(utils::BitVector &last) const;
};

struct File : public Node {
  // Use a vector instead of a string because some of the contents could contain
  // '0' which would otherwise be read as a null terminator.
  std::vector<uint8_t> contents;

  File(uint32_t id, const char name[kFilenameSize], size_t size,
       uint8_t *&contents);
};

struct Directory : public Node {
  std::vector<std::unique_ptr<Node>> files;

  Directory(uint32_t id, const char name[kFilenameSize],
            std::vector<std::unique_ptr<Node>> &files);

  bool hasFile(const std::string &name) const;
  const Node *getFile(const std::string &name) const;
};

/**
 * Parse a Directory from a raw pointer.
 */
std::unique_ptr<Directory> ParseVFS(uint8_t *bytes, uint8_t *bytes_end,
                                    uint32_t &entry_offset);

/**
 * Get the filename and size of the entry point file from raw vfs data.
 */
inline uint8_t *GetEntryFilenameSize(uint8_t *vfs_data, char *&filename,
                                     uint32_t &size) {
  // The entry point filename is 68 bytes before the start of the entry point
  // file (64 bytes for the name length + 4 bytes for size of file).
  uint32_t entry_offset;
  memcpy(&entry_offset, vfs_data, sizeof(entry_offset));

  // The entry point size is 4 bytes before the start of the entry point file.
  memcpy(&size, vfs_data + entry_offset - 4, sizeof(uint32_t));

  filename = reinterpret_cast<char *>(vfs_data) + entry_offset - 68;
  return vfs_data + entry_offset;
}

}  // namespace vfs

#endif
