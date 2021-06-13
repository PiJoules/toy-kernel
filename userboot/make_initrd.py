#!/usr/bin/env python3

import os
import shutil
import sys


def mkdir(dirname):
  if not os.path.exists(dirname):
    os.makedirs(dirname)


def parse_args():
  from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter

  parser = ArgumentParser(
      description=
      "Parse arguments directly from cmake to create the initial ramdisk once targets are done building.",
      formatter_class=ArgumentDefaultsHelpFormatter)

  parser.add_argument(
      "--files",
      required=True,
      type=lambda s: s.split(),
      help="String of space-separated paths to copy into the initial ramdisk.")

  parser.add_argument(
      "--dests",
      required=True,
      type=lambda s: s.split(),
      help="String of space-sparated destination paths in the vfs.")

  parser.add_argument("--initrd-dir",
                      required=True,
                      help="Path to initial ramdisk directory.")

  return parser.parse_args()


def main():
  args = parse_args()

  assert len(args.files) == len(
      args.dests
  ), "Mismatching number of target files ({}) and destination paths ({}).".format(
      len(args.files), len(args.dests))

  for i in range(len(args.files)):
    dst = os.path.join(args.initrd_dir, args.dests[i])
    mkdir(os.path.dirname(dst))
    shutil.copy(args.files[i], dst)
    print("Copied {} to {}".format(args.files[i], dst))

  return 0


if __name__ == "__main__":
  sys.exit(main())
