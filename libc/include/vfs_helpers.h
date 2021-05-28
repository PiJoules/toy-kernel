#ifndef VFS_HELPERS_H
#define VFS_HELPERS_H

#include <vfs.h>

#include <memory>

std::unique_ptr<vfs::Directory> ParseUSTARFromRawData();

#endif
