#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#define GET_MAX_NR_PROC 2
int main(int argc, char const *argv[]) {
int size, r_point;
  r_point = open("/dev/dm510-0", O_RDONLY);
 size = ioctl(r_point, GET_MAX_NR_PROC, 0);
  size_t i;

  for (i = 1; i < size << 1; i++) {
    int result = open("/dev/dm510-0", O_RDONLY);
    printf("Success on %lu read pointer -> %d ",i,  result);

    if(result < 0){
      printf("Counld not read!\n");
      return 0;
    }
    printf("\n");
  }
  return 0;
}
