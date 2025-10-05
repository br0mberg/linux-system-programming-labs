#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) 
{
  int fd = open("/tmp/foo", O_CREAT|O_WRONLY|O_TRUNC, 0660);

  if (fd == -1) { 
    perror("open"); 
    exit(1);
  }

  if (dup2(fd, STDOUT_FILENO) == -1) { 
    perror("dup2"); 
    exit(1); 
  }

  close(fd);    

  printf("Hello!\n");
  
  return 0;
}
