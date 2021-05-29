#ifndef VFS_H_
#define VFS_H_

/**
 * This contains code for manipulating a virtual filesystem.
 */

#include <BitVector.h>
#include <rtti.h>

#include <cstring>
#include <memory>
#include <string>

namespace vfs {

class Node {
 protected:
  enum NodeKind {
    kFileKind,
    kDirectoryKind,
  };

 public:
  Node(NodeKind kind, const std::string &name) : kind_(kind), name_(name) {}
  virtual ~Node() {}

  void Dump() const;

  const std::string &getName() const { return name_; }

  NodeKind getKind() const { return kind_; }

 private:
  void DumpImpl(utils::BitVector &last) const;

  const NodeKind kind_;
  std::string name_;
};

class File : public Node {
 public:
  File(const std::string &name, const std::vector<uint8_t> &contents)
      : Node(kFileKind, name), contents_(contents) {}
  File(const std::string &name) : Node(kFileKind, name) {}
  const auto &getContents() const { return contents_; }
  bool empty() const { return contents_.empty(); }
  size_t getSize() const { return contents_.size(); }

  static bool classof(const Node *node) { return node->getKind() == kFileKind; }

  // Copy data into the start of the file.
  void Write(const char *data, size_t size) {
    contents_.resize(size);
    for (size_t i = 0; i < size; ++i)
      contents_[i] = static_cast<uint8_t>(data[i]);
  }

 private:
  std::vector<uint8_t> contents_;
};

class Directory : public Node {
 public:
  // Constructor for a root directory.
  Directory() : Directory("") {}

  // Directory with no files.
  Directory(const std::string &name) : Node(kDirectoryKind, name) {}

  Directory(const std::string &name, std::vector<std::unique_ptr<Node>> &files)
      : Node(kDirectoryKind, name), nodes_(std::move(files)) {}

  static bool classof(const Node *node) {
    return node->getKind() == kDirectoryKind;
  }

  bool hasNode(const std::string &name) const { return getNode(name); }
  bool hasFile(const std::string &name) const { return getFile(name); }
  bool hasDir(const std::string &name) const { return getDir(name); }

  // Returns the following node in this directory if it exists, otherwise return
  // nullptr.
  const Node *getNode(const std::string &name) const;
  Node *getNode(const std::string &name);

  // Returns the following file/directory in this directory if it exists,
  // otherwise returns nullptr if it doesn't exist or isn't a file or directory.
  const File *getFile(const std::string &name) const;
  File *getFile(const std::string &name);
  const Directory *getDir(const std::string &name) const;
  Directory *getDir(const std::string &name);

  const std::vector<std::unique_ptr<Node>> &getNodes() const { return nodes_; }
  std::vector<std::unique_ptr<Node>> &getNodes() { return nodes_; }

  size_t NumFiles() const { return nodes_.size(); }
  bool empty() const { return nodes_.empty(); }

  // Make the directory and return a refrence to it.
  Directory &mkdir(const std::string &path);

  // Make the file and return a reference to it.
  File &mkfile(const std::string &path);

 private:
  // Create the upper most directory in a potentially nested directory path and
  // return it. For example, given "a/b/c", then a directory for "a" will be
  // created in this one and a reference to it will be returned.
  Directory &mkdir_once(const std::string &path);

  std::vector<std::unique_ptr<Node>> nodes_;
};

union TarBlock {
  union {
    // Pre-POSIX.1-1988 format
    struct {
      char name[100];  // file name
      char mode[8];    // permissions
      char uid[8];     // user id (octal)
      char gid[8];     // group id (octal)
      char size[12];   // size (octal)
      char mtime[12];  // modification time (octal)
      char check[8];  // sum of unsigned characters in block, with spaces in the
                      // check field while calculation is done (octal)
      char link;      // link indicator
      char link_name[100];  // name of linked file
    };

    // UStar format (POSIX IEEE P1003.1)
    struct {
      char old[156];             // first 156 octets of Pre-POSIX.1-1988 format
      char type;                 // file type
      char also_link_name[100];  // name of linked file
      char ustar[8];             // ustar\000
      char owner[32];            // user name (string)
      char group[32];            // group name (string)
      char major[8];             // device major number
      char minor[8];             // device minor number
      char prefix[155];
    };
  };

  char block[512];  // raw memory (500 octets of actual data, padded to 1 block)
};
constexpr const size_t kTarBlockSize = 512;
static_assert(sizeof(TarBlock) == kTarBlockSize);

std::unique_ptr<Directory> ParseUSTAR(const uint8_t *archive);
std::string SimplifyName(std::string name);

struct DirInfo {
  std::string prefix;
  std::string name;

  std::string getFullPath() const {
    return prefix + name;
  }
};

struct FileInfo {
  std::string prefix;
  std::string name;
  size_t size;
  const char *data;

  std::string getFullPath() const {
    return prefix + name;
  }
};

// These callbacks return false if we wish to exit the iteration early and return
// true if it should continue to the next iteration.
using OnDirCallback = bool (*)(const DirInfo &, void *arg);
using OnFileCallback = bool (*)(const FileInfo &, void *arg);

void IterateUSTAR(const uint8_t *archive,
                  OnDirCallback dircallback,
                  OnFileCallback filecallback,
                  void *arg = nullptr);

}  // namespace vfs

#endif
