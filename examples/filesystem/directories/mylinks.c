#include <unistd.h>

main() {

  /* Create a hard link */
  if (link("path1", "path2")) {
    perror ("link");
  }

  /* Create a symbolic link */
  if (symlink("path1", "path3")) {
    perror ("symlink");
  }
}
