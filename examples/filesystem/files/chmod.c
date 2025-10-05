#include <sys/types.h> 
#include <sys/stat.h> 
#include <stdio.h> 

int main(int argc, char** argv)
{
  struct stat buf; 

  if(stat("output", &buf)) {
    perror ("Couldn't stat file");
  } else {
    /* chmod u+s,u+x,g+x */ 
    buf.st_mode |= S_ISUID | S_IXUSR | S_IXGRP ; 
    /* chmod g-w,o-w */ 
    buf.st_mode &= ~(S_IWGRP | S_IWOTH); 

    if (chmod("output", buf.st_mode) == -1) {
      perror("chmod : filename"); 
    }
  }
  
  return 0;
}
