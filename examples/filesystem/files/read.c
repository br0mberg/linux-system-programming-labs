#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define  MAXSIZE   256

int main(int argc, char** argv)
{
  int   fd, n;
  char  array[MAXSIZE];

  fd = open("/etc/passwd", O_RDONLY);

  if( fd == -1 ) {
    perror("open"); 
    exit(1);
  }

  while ((n = read(fd, array, MAXSIZE)) > 0) {
    if (write(STDOUT_FILENO, array, n) != n) {
      perror("write");
    }
  }

  if (n == -1) {
    perror("read");
  }
  close(fd);
  
  return 0;
}
