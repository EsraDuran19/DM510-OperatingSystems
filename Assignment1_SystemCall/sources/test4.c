#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "arch/x86/include/generated/uapi/asm/unistd_64.h"
#include <stdlib.h>

int main(int argc, char ** arv){


		char* str2 = "Hello World";
			printf("%li\n", syscall(__NR_msg_put, str2, -1));


				printf("Message is put\n");

					printf("Getting the message...\n");
						char* buffer2 = malloc(sizeof(char)*32);
							printf("%li\n", syscall(__NR_msg_get, buffer2, 32));

								for(int i = 0; i<32 && buffer2[i]!='\0'; i++){
									         printf("%c", buffer2[i]);
										                }
								 printf("\n");
}
