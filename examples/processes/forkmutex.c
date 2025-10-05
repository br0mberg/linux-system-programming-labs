#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>


int main() {
   pthread_mutex_t lock_;
   pthread_mutexattr_t ma;
   pthread_mutexattr_init( &ma );
   pthread_mutexattr_setpshared( &ma, PTHREAD_PROCESS_SHARED );
   pthread_mutexattr_settype( &ma, PTHREAD_MUTEX_ERRORCHECK );
   pthread_mutex_init( &lock_, &ma );

   pthread_mutex_lock( &lock_ );

   if(fork()==0) {
      std::cout << "child" << std::endl;
      pthread_mutex_lock( &lock_ );
      std::cout << "finish" << std::endl;
   } else {
      std::cout << "parent" << std::endl;
      sleep(1);
      pthread_mutex_lock( &lock_ );
      std::cout << "parent done" << std::endl;
   }

}

