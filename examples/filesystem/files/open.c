#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv)
{
  int fd = open("/etc/passwd", O_RDONLY);

  if (fd == -1) {
    perror("open");
    exit(1);
  }

  int outfd = open("output", 
      O_WRONLY | O_CREAT | O_TRUNC, 0777);

  if (outfd == -1) {
    perror("open"); 
    exit(1);
  }

  close(fd);
  close(outfd);
}
