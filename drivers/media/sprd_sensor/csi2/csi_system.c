/*                                                                                    
 * system.c                                                                           
 *                                                                                    
 *  Created on: Jun 25, 2010                                                          
 *      Author: klabadi & dlopo                                                       
 *                                                                                    
 *                                                                                    
 *  @note: this file should be re-written to match the environment the                
 *  API is running on                                                                 
 */                                                                                   
#ifdef __cplusplus                                                                    
extern "C"                                                                            
{                                                                                     
#endif                                                                                
#include "csi_system.h"                                                                   
#include "csi_log.h"                                                                      
                                                                            
void system_sleep_ms(unsigned ms)                                                     
{                                                                                     
                                                                            
}                                                                                     
                                                                                      
int system_thread_create(void* handle, thread_t p_func, void* param)                  
{                                                                                     
    return 0;                                                                     
}                                                                                     
int system_thread_destruct(void* handle)                                              
{                                                                                     
    return 0;                                                                     
}                                                                                     
                                                                                      
int system_start(thread_t thread)                                                     
{                                                                                     
    return 1;                                                                      
}                                                                                     
                                                                                      
static unsigned system_interrupt_map(interrupt_id_t id)                               
{                                                                                     
    return ~0;                                                                        
}                                                                                     
                                                                                      
int system_interrupt_disable(interrupt_id_t id)                                       
{                                                                                     
    if (system_interrupt_map(id) != (unsigned) (~0))                                  
    {                                                                                 
        return 0;                                                                 
    }                                                                                 
    LOG_ERROR("Interrupt not mapped");                                                
    return 0;                                                                     
}                                                                                     
                                                                                      
int system_interrupt_enable(interrupt_id_t id)                                        
{                                                                                     
    if (system_interrupt_map(id) != (unsigned) (~0))                                  
    {                                                                                 
        return 0;                                                                 
    }                                                                                 
    LOG_ERROR("Interrupt not mapped");                                                
    return 0;                                                                     
}                                                                                     
                                                                                      
int system_interrupt_acknowledge(interrupt_id_t id)                                   
{                                                                                     
    if (system_interrupt_map(id) != (unsigned) (~0))                                  
    {                                                                                 
        return 0;                                                                 
    }                                                                                 
    LOG_ERROR("Interrupt not mapped");                                                
    return 0;                                                                     
}                                                                                     
int system_interrupt_handler_register(interrupt_id_t id, handler_t handler,           
        void * param)                                                                 
{                                                                                     
    return 0;                                                                     
}                                                                                     
                                                                                      
int system_interrupt_handler_unregister(interrupt_id_t id)                            
{                                                                                     
    return 0;                                                                     
}                                                                                     
                                                                                      
#ifdef __cplusplus                                                                    
}                                                                                     
#endif                                                                                
                                                                                            