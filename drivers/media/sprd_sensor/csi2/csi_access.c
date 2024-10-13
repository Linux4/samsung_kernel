/*                                   
 *  access.c                         
 *                                   
 *  Created on: Jun 25, 2010         
 *      Author: klabadi & dlopo      
 */
#include "csi_types.h"
#include "csi_access.h"                  
#include "csi_log.h"                     
                                     
static u32 * access_base_addr = 0;   
int access_init(u32 * base_addr)     
{                                    
    access_base_addr = base_addr;    
    return 0;                        
}                                    
                                     
u32 access_read(u16 addr)            
{                                    
    return access_base_addr[addr];   
}                                    
                                     
void access_write(u32 data, u16 addr)
{                                    
    access_base_addr[addr] = data;   
}                                    