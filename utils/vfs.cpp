#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits.h>
#include <vfs.h>

namespace vfs {

namespace {

template <typename T>
T ReadAndAdvance(uint8_t *&ptr) {
  // NOTE: This assumes the endianness matches that of the host system.
  T x;
  memcpy(&x, ptr, sizeof(T));
  ptr += sizeof(T);
  return x;
}

std::unique_ptr<Node> ParseOneNode(uint8_t *&bytes) {
  uint32_t fileid = ReadAndAdvance<uint32_t>(bytes);
  uint32_t flags = ReadAndAdvance<uint32_t>(bytes);
  bool isfile = flags & 1;

  char name[kFilenameSize];
  memset(name, 0, kFilenameSize);
  memcpy(name, bytes, kFilenameSize);
  bytes += kFilenameSize;

  size_t size = ReadAndAdvance<uint32_t>(bytes);

  if (isfile) {
    return std::unique_ptr<Node>(new File(fileid, name, size, bytes));
  } else {
    std::vector<std::unique_ptr<Node>> nodes;
    for (size_t i = 0; i < size; ++i) { nodes.push_back(ParseOneNode(bytes)); }
    return std::unique_ptr<Node>(new Directory(fileid, name, nodes));
  }
}

}  // namespace

Header::Header(uint32_t id, bool isfile, const char name[kFilenameSize])
    : id(id), flags(0) {
  flags |= static_cast<uint32_t>(isfile);
  memcpy(this->name, name, std::min(kFilenameSize, strlen(name)));
}

Node::Node(uint32_t id, bool isfile, const char name[kFilenameSize])
    : header(id, isfile, name) {}

File &Node::AsFile() {
  assert(header.isFile());
  return *(static_cast<File *>(this));
}

const File &Node::AsFile() const {
  assert(header.isFile());
  return *(static_cast<const File *>(this));
}

Directory &Node::AsDir() {
  assert(header.isDir());
  return *(static_cast<Directory *>(this));
}

const Directory &Node::AsDir() const {
  assert(header.isDir());
  return *(static_cast<const Directory *>(this));
}

void Node::Dump() const {
  utils::BitVector last;
  DumpImpl(last);
}

void Node::DumpImpl(utils::BitVector &last) const {
  if (!last.empty()) {
    for (size_t i = 0; i < last.size() - 1; ++i) {
      if (last.get(i))
        printf("   ");
      else
        printf("|  ");
    }

    if (last.getBack())
      printf("`--");
    else
      printf("|--");
  }

  printf("%s\n", header.name);

  if (header.isFile() || AsDir().files.empty()) return;

  for (size_t i = 0; i < AsDir().files.size() - 1; ++i) {
    last.push_back(0);
    AsDir().files[i]->DumpImpl(last);
    last.pop_back();
  }

  last.push_back(1);
  AsDir().files.back()->DumpImpl(last);
  last.pop_back();
}

File::File(uint32_t id, const char name[kFilenameSize], size_t size,
           uint8_t *&contents)
    : Node(id, /*isfile=*/true, name), contents(contents, contents + size) {
  contents += size;
}

Directory::Directory(uint32_t id, const char name[kFilenameSize],
                     std::vector<std::unique_ptr<Node>> &files)
    : Node(id, /*isfile=*/false, name), files(std::move(files)) {}

std::unique_ptr<Directory> ParseVFS(uint8_t *bytes_begin, uint8_t *bytes_end,
                                    uint32_t &entry_offset) {
  entry_offset = ReadAndAdvance<uint32_t>(bytes_begin);

  std::vector<std::unique_ptr<Node>> nodes;
  while (bytes_begin < bytes_end) {
    nodes.push_back(ParseOneNode(bytes_begin));
  }
  assert(bytes_begin == bytes_end);
  return std::make_unique<Directory>(/*id=*/-1, "root", nodes);
}

bool Directory::hasFile(const std::string &name) const { return getFile(name); }

const Node *Directory::getFile(const std::string &name) const {
  for (const std::unique_ptr<Node> &node : files) {
    if (name == node->header.name) return node.get();
  }
  return nullptr;
}

}  // namespace vfs
