#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>
#include <sys/types.h>

__BEGIN_CDECLS

#define DNAME_SIZE 256

struct dirent {
  ino_t d_ino;             /* Inode number */
  off_t d_off;             /* Not an offset; an opaque value */
  unsigned short d_reclen; /* Length of this record */
  unsigned char d_type;    /* Type of file; not supported
                              by all filesystem types */
  char d_name[DNAME_SIZE]; /* Null-terminated filename */
};

// DIR will be an opaque type.
struct DIR;

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void closedir(DIR *);

__END_CDECLS

#endif
