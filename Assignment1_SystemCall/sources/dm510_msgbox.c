#include "linux/kernel.h"
#include "linux/unistd.h"
#include "linux/slab.h"
#include "linux/uaccess.h"

typedef struct _msg_t msg_t;

struct _msg_t{
		msg_t* previous;
			int length;
				char* message;
};

static msg_t *top = NULL;
static msg_t *bottom = NULL;


asmlinkage
int dm510_msgbox_put( char *buffer, int length ) {

		unsigned long flags;

			if(!access_ok(buffer, length)) {
						return -EACCES;
							}
			if(buffer==NULL){
			 
			return -EINVAL;
			}	
			if(length<0){

				return -EINVAL;
			}	
				msg_t* msg = kzalloc(sizeof(msg_t), GFP_KERNEL);
					
					if(msg == NULL){	return -EINVAL; }
							
						
						msg->previous = NULL;
							msg->length = length;
								msg->message = kmalloc(length, GFP_KERNEL);

									if(msg->message == NULL){ 
										        kfree(msg);
											        return -EINVAL; }
										
											if(copy_from_user(msg->message, buffer, length)) { return -EFAULT; }
												local_irq_save(flags);
														if (bottom == NULL) {
																		bottom = msg;
																					top = msg;
																							} else {
																											
																											msg->previous = top;
																														top = msg;
																																}
																local_irq_restore(flags);
																		return 0;
																			
}

asmlinkage
int dm510_msgbox_get( char* buffer, int length ) {
	unsigned long flags;
		if (top != NULL) {
					
					
					int msglength = top->length;
					if (msglength == 0 ){
						return 0;
					}
							if (length < msglength){ return -EMSGSIZE;  }

										if (!access_ok(buffer, msglength)) {return -EACCES; }
													msg_t* msg = top;
																top = msg->previous;
																			local_irq_save(flags);
																						
																						if(copy_to_user(buffer, msg->message, msglength)){ return -EFAULT; }
																									
																									
																									kfree(msg->message);
																												kfree(msg);
																															
																															local_irq_restore(flags);

																																		return msglength;
																																			}
			return -ENOMSG;
}
