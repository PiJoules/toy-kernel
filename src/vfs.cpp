#include <kmalloc.h>
#include <kstring.h>
#include <ktype_traits.h>
#include <vfs.h>

namespace vfs {

namespace {

template <typename T>
T ReadAndAdvance(uint8_t *&ptr) {
  T x;
  memcpy(&x, ptr, sizeof(T));
  ptr += sizeof(T);
  return x;
}

toy::Unique<Node> ParseOneNode(uint8_t *&bytes) {
  uint32_t fileid = ReadAndAdvance<uint32_t>(bytes);
  uint32_t flags = ReadAndAdvance<uint32_t>(bytes);
  bool isfile = flags & 1;

  char name[kFilenameSize];
  memset(name, 0, kFilenameSize);
  memcpy(name, bytes, kFilenameSize);
  bytes += kFilenameSize;

  size_t size = ReadAndAdvance<uint32_t>(bytes);

  if (isfile) {
    return toy::Unique<Node>(new File(fileid, name, size, bytes));
  } else {
    toy::Vector<toy::Unique<Node>> nodes;
    for (size_t i = 0; i < size; ++i) { nodes.push_back(ParseOneNode(bytes)); }
    return toy::Unique<Node>(new Directory(fileid, name, nodes));
  }
}

}  // namespace

Header::Header(uint32_t id, bool isfile, const char name[kFilenameSize])
    : id(id), flags(0) {
  flags |= static_cast<uint32_t>(isfile);
  memcpy(this->name, name, toy::min(kFilenameSize, strlen(name)));
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
  toy::BitVector last;
  DumpImpl(last);
}

void Node::DumpImpl(toy::BitVector &last) const {
  if (!last.empty()) {
    for (size_t i = 0; i < last.size() - 1; ++i) {
      if (last.get(i))
        DebugPrint("   ");
      else
        DebugPrint("|  ");
    }

    if (last.getBack())
      DebugPrint("`--");
    else
      DebugPrint("|--");
  }

  DebugPrint("{}\n", header.name);

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
                     toy::Vector<toy::Unique<Node>> &files)
    : Node(id, /*isfile=*/false, name), files(toy::move(files)) {}

toy::Unique<Directory> ParseVFS(uint8_t *bytes_begin, uint8_t *bytes_end) {
  toy::Vector<toy::Unique<Node>> nodes;
  while (bytes_begin < bytes_end) {
    nodes.push_back(ParseOneNode(bytes_begin));
  }
  assert(bytes_begin == bytes_end);
  return toy::MakeUnique<Directory>(/*id=*/-1, "root", nodes);
}

bool Directory::hasFile(const toy::String &name) const { return getFile(name); }

const Node *Directory::getFile(const toy::String &name) const {
  for (const toy::Unique<Node> &node : files) {
    if (name == node->header.name) return node.get();
  }
  return nullptr;
}

}  // namespace vfs
