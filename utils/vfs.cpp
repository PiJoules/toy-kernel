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

std::string SimplifyName(std::string name) {
  std::string path(name);
  InplaceLStrip(path);
  InplaceRStrip(path);
  if (path.empty()) return path;

  auto found_sep = path.find(kPathSeparator);
  if (found_sep == path.end()) {
    // "dirname"
    return path;
  }

  if (found_sep == path.end() - 1) {
    // "dirname/"
    return std::string(path.c_str(), path.size() - 1);
  }

  return name;
}

Node *GetNodeImpl(const Directory &dir, const std::string &path) {
  std::string name = SimplifyName(path);
  if (name.empty()) return nullptr;

  for (const std::unique_ptr<Node> &node : dir.getNodes()) {
    auto simplified = SimplifyName(node->getName());
    if (name == simplified) return node.get();
  }
  return nullptr;
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

}  // namespace

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

  std::unique_ptr<Node> dir(new Directory(normalized_name));
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

  std::unique_ptr<Node> file(new File(path));
  current_dir->nodes_.push_back(std::move(file));
  return *rtti::cast<File>(current_dir->nodes_.back().get());
}

std::unique_ptr<Directory> ParseUSTAR(const uint8_t *archive, size_t tarsize) {
  assert(tarsize % kTarBlockSize == 0 &&
         "Tar file is not a multiple of 512 bytes.");

  std::unique_ptr<Directory> root(new Directory);

  const TarBlock *tar = reinterpret_cast<const TarBlock *>(archive);
  while (tarsize && memcmp(tar->ustar, "ustar", 5) == 0) {
    std::string path(tar->prefix);
    path += tar->name;

    size_t filesize = oct2bin(tar->size, sizeof(TarBlock::size) - 1);
    if (tar->type == kDirectory) {
      assert(!filesize && "A directory should have no file size.");
      ++tar;
      tarsize -= kTarBlockSize;
      root->mkdir(path);
      continue;
    }

    assert(filesize);

    // Skip past the header.
    ++tar;
    tarsize -= kTarBlockSize;

    // Round up to the nearest number of 512 byte chunks.
    size_t num_chunks = (filesize % kTarBlockSize)
                            ? (filesize / kTarBlockSize) + 1
                            : (filesize / kTarBlockSize);

    File &file = root->mkfile(path);
    file.Write(reinterpret_cast<const char *>(tar), filesize);

    tar += num_chunks;
    tarsize -= num_chunks * kTarBlockSize;
  }

  return root;
}

}  // namespace vfs
