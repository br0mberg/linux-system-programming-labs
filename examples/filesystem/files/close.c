#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
  char str[]="Hello\n";
    
  int fd = open("testfile1.txt", O_WRONLY | O_CREAT, 0777);
  write(fd, str, strlen(str));
    
  close(fd);
  
  return 0; 
}
