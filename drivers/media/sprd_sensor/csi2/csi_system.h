/*                                                                                        
 * system.h                                                                               
 *                                                                                        
 *  Created on: Jun 25, 2010                                                              
 *      Author: klabadi & dlopo                                                           
 */                                                                                       
                                                                                          
#ifndef SYSTEM_H_                                                                         
#define SYSTEM_H_                                                                         
                                                                                          
#include "csi_types.h"                                                                        
                                                                                          
typedef enum                                                                              
{                                                                                         
    CSI_INT_ERR1 = 1, CSI_INT_ERR2                                                        
} interrupt_id_t;                                                                         
void system_sleep_ms(unsigned ms);                                                        
int system_thread_create(void* handle, thread_t p_func, void* param);                     
int system_thread_destruct(void* handle);                                                 
int system_start(thread_t thread);                                                        
int system_interrupt_disable(interrupt_id_t id);                                          
int system_interrupt_enable(interrupt_id_t id);                                           
int system_interrupt_acknowledge(interrupt_id_t id);                                      
int system_interrupt_handler_register(interrupt_id_t id, handler_t handler, void * param);
int system_interrupt_handler_unregister(interrupt_id_t id);                               
                                                                                          
                                                                                          
#endif /* SYSTEM_H_ */                                                                    