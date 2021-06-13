#include <vfs.h>

#include <cctype>
#include <memory>

#ifndef KERNEL
#include <cstdio>
#endif

namespace vfs {

namespace {

constexpr const char kDirectory = '5';
constexpr const char kPathSeparator = '/';

void InplaceLStrip(std::string &path) {
  if (path.empty()) return;

  while (isspace(path[0])) path.erase(path.begin());
}

void InplaceRStrip(std::string &path) {
  if (path.empty()) return;

  while (1) {
    auto it = path.end() - 1;
    if (!isspace(*it)) return;
    path.erase(it);
  }
}

// Split the uppermost directory from the path.
// Example:
//
//   "a/b/c" -> ("a", "b/c")
//   "a/" -> ("a", "")
//   "a" -> ("a", "")
//   "/a" -> ("/", "a")
//   "/a/b/c" -> ("/", "a/b/c")
//
std::pair<std::string, std::string> SplitHead(const std::string &path) {
  if (path[0] == kPathSeparator)
    return std::pair(std::string(1, kPathSeparator),
                     std::string(path.begin() + 1, path.end()));

  std::string::iter_t sep(nullptr);
  if ((sep = path.find(kPathSeparator)) != path.end()) {
    // Do not include the path separator in either the head or tail.
    std::string head(path.begin(), sep);
    std::string rest(sep + 1, path.end());
    return std::pair(head, rest);
  }
  return std::pair(path, std::string());
}

Directory *GetRootDir(Node *node) {
  Directory *parent;
  while ((parent = node->getParentDir()) != nullptr) { node = parent; }
  return rtti::cast<Directory>(node);
}

const Node *GetNodeImpl(const Directory &dir, const std::string &path);

Node *GetNodeImpl(Directory &dir, const std::string &path) {
  std::string name = SimplifyName(path);
  if (name.empty()) return nullptr;

  // Current directory.
  if (name == ".") return &dir;

  auto head_tail = SplitHead(name);

  if (head_tail.first == std::string(1, kPathSeparator))
    return GetNodeImpl(*GetRootDir(&dir), head_tail.second);

  // Base case: only one node.
  if (head_tail.second.empty()) {
    for (const std::unique_ptr<Node> &node : dir.getNodes()) {
      auto simplified = SimplifyName(node->getName());
      if (head_tail.first == simplified) return node.get();
    }
    return nullptr;
  }

  // Seach for the head in any of the nodes in this directory.
  if (auto *subdir = dir.getDir(head_tail.first))
    return GetNodeImpl(*subdir, head_tail.second);

  return nullptr;
}

const Node *GetNodeImpl(const Directory &dir, const std::string &path) {
  return GetNodeImpl(const_cast<Directory &>(dir), path);
}

size_t oct2bin(const char *str, size_t size) {
  size_t n = 0;
  const char *c = str;
  while (size-- > 0) {
    n *= 8;
    assert(*c >= '0');
    n += static_cast<size_t>(*c - '0');
    c++;
  }
  return n;
}

bool IsZeroPage(const uint8_t *tar_page) {
  for (size_t i = 0; i < kTarBlockSize; ++i) {
    if (*(tar_page++)) return false;
  }
  return true;
}

}  // namespace

std::string SimplifyName(std::string name) {
  InplaceLStrip(name);
  InplaceRStrip(name);
  if (name.empty()) return name;

  if (name.size() == 1 && name[0] == '.') return name;

  if (name.size() >= 2 && name[0] == '.' && name[1] == kPathSeparator) {
    // "./..."
    name.erase(name.begin(), name.begin() + 2);
  }

  auto found_sep = name.find(kPathSeparator);
  if (found_sep == name.end()) {
    // "dirname"
    return name;
  }

  if (found_sep == name.end() - 1) {
    // "dirname/"
    name.erase(found_sep, name.end());
  }

  return name;
}

#ifndef KERNEL
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

  printf("%s\n", getName().c_str());

  if (rtti::isa<File>(this)) return;

  const Directory *dir;
  if ((dir = rtti::dyn_cast<Directory>(this))) {
    if (dir->getNodes().empty()) return;
  }

  for (size_t i = 0; i < dir->getNodes().size() - 1; ++i) {
    last.push_back(0);
    dir->getNodes()[i]->DumpImpl(last);
    last.pop_back();
  }

  last.push_back(1);
  dir->getNodes().back()->DumpImpl(last);
  last.pop_back();
}
#endif

Node::Node(NodeKind kind, const std::string &name, Directory *parent)
    : kind_(kind), name_(name), parent_(parent) {}

const Node *Directory::getNode(const std::string &path) const {
  return GetNodeImpl(*this, path);
}

Node *Directory::getNode(const std::string &path) {
  return GetNodeImpl(*this, path);
}

const File *Directory::getFile(const std::string &path) const {
  if (const auto *file = rtti::dyn_cast_or_null<File>(GetNodeImpl(*this, path)))
    return file;
  return nullptr;
}

File *Directory::getFile(const std::string &path) {
  if (auto *file = rtti::dyn_cast_or_null<File>(GetNodeImpl(*this, path)))
    return file;
  return nullptr;
}

const Directory *Directory::getDir(const std::string &path) const {
  if (const auto *dir =
          rtti::dyn_cast_or_null<Directory>(GetNodeImpl(*this, path)))
    return dir;
  return nullptr;
}

Directory *Directory::getDir(const std::string &path) {
  if (auto *dir = rtti::dyn_cast_or_null<Directory>(GetNodeImpl(*this, path)))
    return dir;
  return nullptr;
}

Directory &Directory::mkdir_once(const std::string &path) {
  std::string normalized_name = SimplifyName(path);
  if (Node *node = getNode(normalized_name))
    return *rtti::cast<Directory>(node);

  std::unique_ptr<Node> dir(new Directory(normalized_name, this));
  nodes_.push_back(std::move(dir));
  return *rtti::cast<Directory>(nodes_.back().get());
}

Directory &Directory::mkdir(const std::string &name) {
  std::string path = SimplifyName(name);
  Directory *current_dir = this;
  while (!path.empty()) {
    auto sep = path.find(kPathSeparator);
    std::string substr(path.begin(), sep + 1);
    current_dir = &(current_dir->mkdir_once(substr));
    path.erase(path.begin(), sep + 1);
  }
  return *current_dir;
}

File &Directory::mkfile(const std::string &name) {
  std::string path = SimplifyName(name);
  assert(path[0] != kPathSeparator);
  Directory *current_dir = this;
  std::string::iter_t sep(nullptr);
  while ((sep = path.find(kPathSeparator)) != path.end()) {
    // Keep going until we don't have another path separator.
    std::string substr(path.begin(), sep + 1);
    current_dir = &(current_dir->mkdir_once(substr));
    path.erase(path.begin(), sep + 1);
  }
  assert(!path.empty() && "Missing filename");

  std::unique_ptr<Node> file(new File(path, this));
  current_dir->nodes_.push_back(std::move(file));
  return *rtti::cast<File>(current_dir->nodes_.back().get());
}

void IterateUSTAR(const uint8_t *archive, OnDirCallback dircallback,
                  OnFileCallback filecallback, void *arg) {
  const TarBlock *tar = reinterpret_cast<const TarBlock *>(archive);
  while (!(IsZeroPage(reinterpret_cast<const uint8_t *>(tar)) &&
           IsZeroPage(reinterpret_cast<const uint8_t *>(tar + 1)))) {
    assert(memcmp(tar->ustar, "ustar", 5) == 0 && "Expected UStar format");
    const char *prefix = tar->prefix;
    const char *name = tar->name;

    size_t filesize = oct2bin(tar->size, sizeof(TarBlock::size) - 1);
    if (tar->type == kDirectory) {
      assert(!filesize && "A directory should have no file size.");
      DirInfo dirinfo = {
          .prefix = prefix,
          .name = name,
      };
      if (!dircallback(dirinfo, arg)) return;
      ++tar;
      continue;
    }

    assert(filesize);

    // Skip past the header.
    ++tar;

    // Round up to the nearest number of 512 byte chunks.
    size_t num_chunks = (filesize % kTarBlockSize)
                            ? (filesize / kTarBlockSize) + 1
                            : (filesize / kTarBlockSize);

    FileInfo fileinfo = {
        .prefix = prefix,
        .name = name,
        .size = filesize,
        .data = reinterpret_cast<const char *>(tar),
    };
    if (!filecallback(fileinfo, arg)) return;

    tar += num_chunks;
  }
}

std::unique_ptr<Directory> ParseUSTAR(const uint8_t *archive) {
  std::unique_ptr<Directory> root(new Directory);

  auto dircallback = [](const DirInfo &dirinfo, void *arg) {
    auto *root = reinterpret_cast<Directory *>(arg);
    root->mkdir(dirinfo.getFullPath());
    return true;
  };

  auto filecallback = [](const FileInfo &fileinfo, void *arg) {
    auto *root = reinterpret_cast<Directory *>(arg);
    File &file = root->mkfile(fileinfo.getFullPath());
    file.Write(fileinfo.data, fileinfo.size);
    return true;
  };

  IterateUSTAR(archive, dircallback, filecallback, root.get());

  return root;
}

}  // namespace vfs
