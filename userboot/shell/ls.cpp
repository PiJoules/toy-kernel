#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("TODO: `ls` must be provided a directory as an argument\n");
    return 0;
  }

  DIR *mydir = opendir(argv[1]);
  if (!mydir) {
    printf("No directory named '%s'\n", argv[1]);
    return 1;
  }
  struct dirent *myfile;
  while ((myfile = readdir(mydir)) != NULL) { printf("%s\n", myfile->d_name); }
  closedir(mydir);
  return 0;
}
