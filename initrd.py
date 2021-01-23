#!/usr/bin/env python3

# Script for making the initial ram disk in a virtual filesystem format
# recognized by the kernel. This is a custom initrd format. The format is a
# flat sequence of "files". Each file has the following layout:
#
#   Bits        Size  Description
#
#   [0, 32)     32    Offset to the user entry point in this file. When jumping
#                     from kernel to userspace, we need some address that
#                     indicates where we should jump to. This can be indicated
#                     by this offset which is always at the start of the initrd
#                     file.
#
#                     This must point to a file's contents.
#
#                     FIXME: It's technically possible for the vfs component to
#                     contain no files and be empty. We should handle this case.
#
#     File descriptor bits (header/start of the vfs):
#
#   [32, 64)    32    FileID; This can be any random unique 4-byte integer; Also
#                     implies there can be no more than 2^32 files in the vfs
#   [64, 65)    1     0 => Directory, 1 => File
#   [65, 96)    31    Reserved (for other flags that we may want to add)
#   [96, 608)   512   Filename (max 64 1-byte characters). If the name doesn't
#                     fill the whole length, it is padded with zeros.
#
#       If this node is a file:
#
#   [608, 640)  32    Content size; Also implies the maximum file size is 4GB
#   [640, ...)  0+    Contents
#
#       Otherwise if this node is a directory:
#
#   [608, 640)  32    Number of files; Also implies the maximum number of files in
#                     a directory is 2^32
#   [640, ...)  0+    Sequence of files/directories that are in this directory.
#
# NOTE: This layout is subject to change and not set in stone.
# TODO: Add some sort of magic number field for error checking?
#
# The vfs file itself represents a root directory. When this is loaded into the
# kernel, each file in the vfs is treated as under the root directory.

import sys
import os
import struct


class Node:
  UNIQUE_ID = 0
  FILENAME_LEN = 64

  # We have a test to check this. If the header ever changes in length, this
  # should also be modified.
  HEADER_LEN = 72

  def __init__(self, isdir, name):
    self.id = Node.UNIQUE_ID
    Node.UNIQUE_ID += 1
    assert Node.UNIQUE_ID < 2**32, "Reached maximum number of files"

    self.isdir = isdir
    self.name = name
    assert len(
        name
    ) <= self.FILENAME_LEN, "A filename must be at most 64 characters ({})".format(
        name)

  def _header_bytes(self):
    # FIXME: Packing depends on the host endianness. We should just exclusively
    # use little-endian.
    bytes = b""

    # FileID
    bytes += struct.pack("I", self.id)

    # Flags
    if self.isdir:
      bytes += struct.pack("I", 0)
    else:
      bytes += struct.pack("I", 1)

    # Filename
    bytes += struct.pack(str(self.FILENAME_LEN) + "s", self.name.encode())

    return bytes

  def bytes(self):
    raise NotImplementedError

  @classmethod
  def _parse_one_file(cls, bytes):
    assert len(bytes) >= 4
    fileid = struct.unpack("I", bytes[:4])[0]
    bytes = bytes[4:]

    assert len(bytes) >= 4
    flags = struct.unpack("I", bytes[:4])[0]
    bytes = bytes[4:]
    isfile = bool(flags & 1)

    assert len(bytes) >= Node.FILENAME_LEN
    name = struct.unpack(
        str(Node.FILENAME_LEN) + "s", bytes[:Node.FILENAME_LEN])[0].decode()
    bytes = bytes[Node.FILENAME_LEN:]

    assert len(bytes) >= 4
    size = struct.unpack("I", bytes[:4])[0]
    bytes = bytes[4:]

    if not isfile:
      files = []
      for i in range(size):
        file, bytes = cls._parse_one_file(bytes)
        files.append(file)
      return Directory(name, files), bytes
    else:
      contents = struct.unpack(str(size) + "s", bytes[:size])[0]
      bytes = bytes[size:]
      return File(name, contents), bytes

  @classmethod
  def from_bytes(cls, bytes):
    files = []

    while bytes:
      file, bytes = cls._parse_one_file(bytes)
      files.append(file)

    return Directory("<root>", files)

  def __eq__(self, other):
    return isinstance(other, Node) and self.id == other.id

  def __ne__(self, other):
    return not (self == other)

  def __hash__(self):
    return hash(self.id)


class File(Node):

  def __init__(self, name, contents, fileid=None):
    super().__init__(False, name)
    self.contents = contents

  def bytes(self):
    size = len(self.contents)
    return (self._header_bytes() + struct.pack("I", size) +
            struct.pack(str(size) + "s", self.contents))

  def offset_to_file_contents(self):
    # The offset to the contents is just the size of the header + the size of
    # the length for the contents.
    return Node.HEADER_LEN + 4

  @staticmethod
  def from_file(fullpath):
    assert os.path.isfile(fullpath)
    with open(fullpath, "rb") as f:
      bytes = f.read()
    return File(os.path.basename(fullpath), bytes)


def split_path(filepath):
  # Split a path into "parts".
  # "a/b/c.txt" -> ["a", "b", "c.txt"]
  # "a/" -> ["a"]
  # "a" -> ["a"]
  filepath = filepath.strip()
  assert filepath, "File path is an empty string"
  assert not os.path.isabs(filepath)

  parts = []
  head = filepath
  while True:
    head, tail = os.path.split(head)

    # `tail` can be an empty string if `head` ends with a path separator.
    if tail:
      parts.append(tail)

    if not head:
      break

  assert parts, "Found no pathlets?"
  return parts[::-1]


class Directory(Node):

  def __init__(self, name, files=None):
    super().__init__(True, name)
    self.files = files or []

  def bytes(self):
    b = b""

    b += self._header_bytes()
    b += struct.pack("I", len(self.files))
    for file in self.files:
      b += file.bytes()

    return b

  def vfs_bytes(self):
    """Return the bytes this directory represents as if it were the root vfs directory."""
    b = b""

    for file in self.files:
      b += file.bytes()

    return b

  def has(self, filename):
    """Check if this directory contains a file by a given name."""
    return any(f.name == filename for f in self.files)

  def get(self, filename):
    """Get a file in this directory. Raises an exception otherwise."""
    for f in self.files:
      if f.name == filename:
        return f
    raise Exception(
        "File '{}' does not exist in this directory".format(filename))

  def vfs_offset_to_file_contents(self, filepath):
    """Get the offset to the file contents of a given file path, relative to the start of the vfs directory."""

    return self.offset_to_file_contents(filepath) - Node.HEADER_LEN - 4

  def offset_to_file_contents(self, filepath):
    """Get the offset to the file contents of a given file path, relative to the start of this current directory."""
    # Add current header size.
    offset = Node.HEADER_LEN

    # Add 4 bytes for the number of files.
    offset += 4

    pathlets = split_path(filepath)
    filename = pathlets[0]
    assert self.has(filename), "This directory does not have file '{}'".format(
        filename)

    for file in self.files:
      if file.name == filename:
        if len(pathlets) == 1:
          # Found the file!
          offset += file.offset_to_file_contents()
        else:
          # This is a directory. We will recursively add more to the offset.
          # FIXME: We don't need to constantly split and join here. We can just
          # get the highest directory.
          offset += file.offset_to_file_contents(os.path.join(pathlets[1:]))
        break

      # Add size of this mismatching file or directory.
      offset += len(file.bytes())

    return offset

  @staticmethod
  def from_dir(fullpath):
    assert os.path.isdir(fullpath)
    assert os.path.exists(fullpath)

    files = []
    for file in os.listdir(fullpath):
      file = os.path.join(fullpath, file)
      if os.path.isfile(file):
        node = File.from_file(file)
      else:
        node = Directory.from_dir(file)
      files.append(node)

    return Directory(os.path.basename(fullpath), files)


class Dumper:
  """Dump files in a `tree` like fasion."""

  def __init__(self):
    pass

  def dump(self, node, last=None):
    last = last or []
    if last:
      for i in range(len(last) - 1):
        if last[i]:
          print("   ", end="")
        else:
          print("|  ", end="")

      if last[-1]:
        print("`--", end="")
      else:
        print("|--", end="")
    print(node.name)

    if not node.isdir or not node.files:
      return

    dircontents = list(node.files)
    for i in range(len(dircontents) - 1):
      self.dump(dircontents[i], last + [False])

    self.dump(dircontents[-1], last + [True])


def dir_to_initrd(dirname, entry, out_file):
  assert os.path.isdir(dirname), "Directory doesn't exist"
  assert os.path.isfile(os.path.join(
      dirname, entry)), "File does not exists relative to directory"

  vfs = Directory.from_dir(dirname)
  offset = vfs.vfs_offset_to_file_contents(entry)

  # Add the size of this offset.
  offset += 4

  b = struct.pack("I", offset)

  out_file.write(b + vfs.vfs_bytes())


def parse_args():
  from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter

  parser = ArgumentParser(description="Script for making the initial ram disk",
                          formatter_class=ArgumentDefaultsHelpFormatter)

  parser.add_argument(
      "root",
      help="Directory that will act as the root of the virtual filesystem.")
  parser.add_argument(
      "-e",
      "--entry",
      help="Path relative to root that indicates the entry file point.")
  parser.add_argument("-o",
                      "--output",
                      default="initrd.vfs",
                      help="Output vfs file.")
  parser.add_argument("-d",
                      "--dump",
                      action="store_true",
                      help="Dump a vfs file passed as `root`")

  return parser.parse_args()


def main():
  args = parse_args()

  if args.dump:
    with open(args.root, "rb") as f:
      bytes = f.read()

      assert len(bytes) >= 4
      offset = struct.unpack("I", bytes[:4])[0]

      # The name of the file is 68 bytes before the content start.
      name = bytes[offset - 68:offset - 4].decode()

      bytes = bytes[4:]
      dir = Node.from_bytes(bytes)

    print("offset to entry {} ({})".format(offset, name))
    Dumper().dump(dir)
    return 0

  with open(args.output, "wb") as out:
    dir_to_initrd(args.root, args.entry, out)

  return 0


import unittest


class TestVFS(unittest.TestCase):
  """Test the VFS format."""

  def test_split_path(self):
    """Test that paths are split into parts as expected."""
    self.assertEqual(split_path("a/b/c"), ["a", "b", "c"])
    self.assertEqual(split_path("a/b"), ["a", "b"])
    self.assertEqual(split_path("a/b/"), ["a", "b"])
    self.assertEqual(split_path("a//"), ["a"])
    self.assertEqual(split_path("a/"), ["a"])
    self.assertEqual(split_path("a"), ["a"])

  def test_header_bytes_length(self):
    d = Directory.from_dir("tests/emptydir")
    self.assertEqual(len(d._header_bytes()), Node.HEADER_LEN)

  def test_empty_dir(self):
    """Test an empty directory."""
    d = Directory.from_dir("tests/emptydir")
    self.assertEqual(d.files, [])
    self.assertTrue(d.isdir)
    self.assertEqual(d.vfs_bytes(), b"")

    d2 = Node.from_bytes(d.vfs_bytes())
    self.assertTrue(d2.isdir)
    self.assertEqual(d2.files, [])
    self.assertEqual(d2.vfs_bytes(), b"")

  def test_file(self):
    """Test a file."""
    f = File.from_file("tests/onefile/file.txt")
    self.assertFalse(f.isdir)
    self.assertEqual(f.name, "file.txt")
    self.assertEqual(f.contents, b"abcd\n")

  def test_one_file_dir(self):
    """Test a directory with only 1 file."""
    d = Directory.from_dir("tests/onefile")
    self.assertEqual(len(d.files), 1)
    self.assertEqual(d.name, "onefile")

    f = d.files[0]
    self.assertFalse(f.isdir)
    self.assertEqual(f.name, "file.txt")
    self.assertEqual(f.contents, b"abcd\n")

    offset = d.offset_to_file_contents("file.txt")
    self.assertEqual(d.bytes()[offset:offset + 5], b"abcd\n")

    vfs_offset = d.vfs_offset_to_file_contents("file.txt")
    self.assertEqual(d.vfs_bytes()[vfs_offset:vfs_offset + 5], b"abcd\n")

  def test_nested_files(self):
    """Test a slightly more complex directory structure."""
    d = Directory.from_dir("tests/nested")
    self.assertEqual(len(d.files), 3)

    self.assertEqual(d.get("file").contents, b"abcd\n")
    self.assertEqual(d.get("nested2").files, [])

    vfs_offset = d.vfs_offset_to_file_contents("file")
    self.assertEqual(d.vfs_bytes()[vfs_offset:vfs_offset + 5], b"abcd\n")

    d2 = d.get("nested")
    self.assertEqual(len(d2.files), 2)
    self.assertEqual(d2.get("file").contents, b"efgh\n")
    self.assertNotEqual(d2.get("file"), d.get("file"))

    vfs_offset = d2.vfs_offset_to_file_contents("file")
    self.assertEqual(d2.vfs_bytes()[vfs_offset:vfs_offset + 5], b"efgh\n")

    d3 = d2.get("nested3")
    self.assertEqual(len(d3.files), 1)
    self.assertEqual(d3.get("file").contents, b"")
    self.assertNotEqual(d3.get("file"), d2.get("file"))
    self.assertNotEqual(d3.get("file"), d.get("file"))


if __name__ == "__main__":
  if len(sys.argv) == 2 and sys.argv[1] == "test":
    argv = sys.argv[:1] + sys.argv[2:]
    unittest.main(argv=argv)
  else:
    sys.exit(main())
