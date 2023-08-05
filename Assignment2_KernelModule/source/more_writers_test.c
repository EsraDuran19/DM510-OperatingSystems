#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#define DEVICE_COUNT 2
int main(int argc, char const *argv[]) {
  size_t i;
  for ( i = 0; i < DEVICE_COUNT; i++) {
    int w_point = open("/dev/dm510-0", O_RDWR);

    printf("Pointer to write %lu -> %d ",i,  w_point);
    if(0 > w_point){
      printf("Could not write!\n");
      return 0;
    }
    printf("\n");
  }
  return 0;
}
