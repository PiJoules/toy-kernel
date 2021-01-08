#ifndef VFS_H_
#define VFS_H_

#include <bitvector.h>
#include <kassert.h>
#include <kstdint.h>
#include <kstring.h>
#include <string.h>
#include <unique.h>
#include <vector.h>

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
  void DumpImpl(toy::BitVector &last) const;
};

struct File : public Node {
  // Use a vector instead of a string because some of the contents could contain
  // '0' which would otherwise be read as a null terminator.
  toy::Vector<uint8_t> contents;

  File(uint32_t id, const char name[kFilenameSize], size_t size,
       uint8_t *&contents);
};

struct Directory : public Node {
  toy::Vector<toy::Unique<Node>> files;

  Directory(uint32_t id, const char name[kFilenameSize],
            toy::Vector<toy::Unique<Node>> &files);

  bool hasFile(const toy::String &name) const;
  const Node *getFile(const toy::String &name) const;
};

/**
 * Parse a Directory from a raw pointer.
 */
toy::Unique<Directory> ParseVFS(uint8_t *bytes, uint8_t *bytes_end);

}  // namespace vfs

#endif
