#include <sys/stat.h>

int main(int argc, char** argv)
{
  struct stat buf;

  if (stat("/etc", &buf)) {
    perror("Couldn't stat file");
  } 
  else {
    /* S_ISDIR defined in <sys/stat.h> */
    if (S_ISDIR(buf.st_mode)) {
      printf("It is a directory!\n");
    }
  }
  
  return 0;
}
