#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

main(int argc, char *argv[]) {

  char *buf;
  struct stat statbuf;
  int n;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s linkname\n", argv[0]);
    exit(1);
  }

  if (lstat(argv[1], &statbuf) == -1) { 
    perror("lstat"); 
    exit(1);
  }

  if (!S_ISLNK(statbuf.st_mode)) {
    fprintf(stderr, "%s is not a symbolic link.\n", argv[1]);
    exit(1);
  }

  buf = (char *)malloc(statbuf.st_size + 1);
  if (buf == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }

  n = readlink(argv[1], buf, statbuf.st_size + 1);
  if (n == -1) {
    perror("readlink"); 
    exit(1);
  }
  buf[n] = '\0';
  printf("%s\n", buf);
  exit(0);
}
