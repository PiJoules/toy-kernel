#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
  DIR *mydir;
  char *dirname;
  char cwd[256] = {};
  if (argc < 2) {
    if (!getcwd(cwd, sizeof(cwd))) {
      printf("getcwd() failed\n");
      return 1;
    }
    dirname = cwd;
  } else {
    dirname = argv[1];
  }
  if ((mydir = opendir(dirname)) == NULL) {
    printf("No directory named '%s'\n", dirname);
    return 1;
  }
  struct dirent *myfile;
  while ((myfile = readdir(mydir)) != NULL) { printf("%s\n", myfile->d_name); }
  closedir(mydir);
  return 0;
}
