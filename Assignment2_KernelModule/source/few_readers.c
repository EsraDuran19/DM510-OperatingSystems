#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

int main(int argc, char const *argv[]) {
 	int device_0, device_1;
	int i;
 	if(1 >= argc) return 0;

 	device_0 = open("/dev/dm510-0", O_RDWR);

	char * msg = argv[1];
 	size_t size = strlen(msg);
 	printf("Writing : %s\n", msg);
	

	int pid = fork();
	write(device_0, msg, size);
	device_1 = open("/dev/dm510-1", O_RDONLY);

  	char * buffer = malloc(size*sizeof(*read));
	ssize_t b = read(device_1, buffer, size);

	printf("Data read from p%d -> %s\n", pid, buffer);
	

	exit(pid);
  	return 0;
}

