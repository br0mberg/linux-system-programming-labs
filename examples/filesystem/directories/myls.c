#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

int main(int argc, char *argv[]) {

  struct stat buf;
  DIR   *dirp;
  struct dirent *dent;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s directoryname\n",argv[0]);
    exit(1);
  }

  if (stat(argv[1], &buf) == -1) {
    perror("stat");
    exit(1);
  }

  if (S_ISDIR(buf.st_mode)) {
    printf("%s is a directory.\n", argv[1]);
  } else {
    fprintf(stderr, "%s is not a directory.\n", argv[1]);
    exit(1);
  }

  if ((dirp = opendir(argv[1])) == NULL) {
    perror("opendir");
    exit(1);
  }
  printf("Contents\n========\n");

  while (dent = readdir(dirp)) {
    printf("%s\n", dent->d_name);
  }
  (void)closedir(dirp);
}
