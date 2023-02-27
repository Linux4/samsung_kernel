/*                                    
 * access.h                           
 *                                    
 *  Created on: Jun 25, 2010          
 *      Author: klabadi & dlopo       
 */                                   
                                      
#ifndef ACCESS_H_                     
#define ACCESS_H_                     
                                      
#include <linux/types.h>                 
int access_init(u32 * base_addr);     
u32 access_read(u16 addr);            
                                      
void access_write(u32 data, u16 addr);
                                      
#endif /* ACCESS_H_ */                