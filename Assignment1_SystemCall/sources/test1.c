#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "arch/x86/include/generated/uapi/asm/unistd_64.h"
#include <stdlib.h>

int main(int argc, char ** arv){
		char* str = "This is a stupid message";

			printf("%li\n", syscall(__NR_msg_put, str, strlen(str)));
				printf("Message is put\n");

					printf("Getting the message...\n");
						char* buffer = malloc(sizeof(char)*32);
							printf("%li\n", syscall(__NR_msg_get, buffer, 32));

								for(int i = 0; i<32 && buffer[i]!='\0'; i++){
							            printf("%c", buffer[i]);
													                                                                                                }
									printf("\n");
			char* str1 = "My name is Esra.";
			printf("%li\n", syscall(__NR_msg_put, str1, strlen(str1)));
			printf("Message is put\n");
			printf("Getting the message...\n");
			char* buffer1 = malloc(sizeof(char)*32);
			printf("Message is :\n");			
			printf("%li\n", syscall(__NR_msg_get, buffer1, 32));
			for(int i = 0; i<32 && buffer1[i]!='\0'; i++){
			printf("%c", buffer1[i]);																													                                                                                                }

	 printf("\n");
}
