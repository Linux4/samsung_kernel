/*                                                                              
 * log.c                                                                        
 *                                                                              
 *  Created on: Jun 25, 2010                                                    
 *      Author: klabadi & dlopo                                                 
 */                                                                             
#include <linux/kernel.h>                                                                                
#include "csi_log.h"                                                                
#ifdef __cplusplus                                                              
extern "C"                                                                      
{                                                                               
#endif                                                                          
                                                                                
static log_verbose_t log_verbose = VERBOSE_NOTICE;                              
static log_numeric_t log_numeric = NUMERIC_HEX;                                 
static unsigned log_verbose_write = 0;                                          
                                                                                
void log_set_verbose(log_verbose_t verbose)                                     
{                                                                               
    log_verbose = verbose;                                                      
}                                                                               
                                                                                
void log_set_numeric(log_numeric_t numeric)                                     
{                                                                               
    log_numeric = numeric;                                                      
}                                                                               
                                                                                
void log_set_verbose_write(unsigned state)                                      
{                                                                               
    log_verbose_write = state;                                                  
}                                                                               
                                                                                
void log_print_write(unsigned a, unsigned b)                                    
{                                                                               
    if (log_verbose_write == 1)                                                 
    {                                                                           
        if (log_numeric == NUMERIC_DEC)                                         
        {                                                                       
            printk("%d, %d\n", a, b);                                           
        }                                                                       
        else                                                                    
        {                                                                       
            printk("0x%x, 0x%x\n", a, b);                                       
        }                                                                       
    }                                                                           
}                                                                               
                                                                                
void log_print0(log_verbose_t verbose, const char* function_name)               
{                                                                               
    if (verbose == VERBOSE_ERROR)                                               
    {                                                                           
        printk("ERROR: ");                                                      
    }                                                                           
    if (verbose == VERBOSE_WARN)                                                
    {                                                                           
        printk("WARNING: ");                                                    
    }                                                                           
    if (verbose <= log_verbose)                                                 
    {                                                                           
        printk("%s\n", function_name);                                          
    }                                                                           
}                                                                               
                                                                                
void log_print1(log_verbose_t verbose, const char* function_name, const char* a)
{                                                                               
    if (verbose == VERBOSE_ERROR)                                               
    {                                                                           
        printk("ERROR: ");                                                      
    }                                                                           
    if (verbose == VERBOSE_WARN)                                                
    {                                                                           
        printk("WARNING: ");                                                    
    }                                                                           
    if (verbose <= log_verbose)                                                 
    {                                                                           
        printk("%s: %s\n", function_name, a);                                   
    }                                                                           
}                                                                               
                                                                                
void log_print2(log_verbose_t verbose, const char* function_name, const char* a,
        unsigned b)                                                             
{                                                                               
    if (verbose == VERBOSE_ERROR)                                               
    {                                                                           
        printk("ERROR: ");                                                      
    }                                                                           
    if (verbose == VERBOSE_WARN)                                                
    {                                                                           
        printk("WARNING: ");                                                    
    }                                                                           
    if (verbose <= log_verbose)                                                 
    {                                                                           
        if (log_numeric == NUMERIC_DEC)                                         
        {                                                                       
            printk("%s: %s, %d\n", function_name, a, b);                        
        }                                                                       
        else                                                                    
        {                                                                       
            printk("%s: %s, 0x%x\n", function_name, a, b);                      
        }                                                                       
    }                                                                           
}                                                                               
                                                                                
void log_print3(log_verbose_t verbose, const char* function_name, const char* a,
        unsigned b, unsigned c)                                                 
{                                                                               
    if (verbose == VERBOSE_ERROR)                                               
    {                                                                           
        printk("ERROR: ");                                                      
    }                                                                           
    if (verbose == VERBOSE_WARN)                                                
    {                                                                           
        printk("WARNING: ");                                                    
    }                                                                           
    if (verbose <= log_verbose)                                                 
    {                                                                           
        if (log_numeric == NUMERIC_DEC)                                         
        {                                                                       
            printk("%s: %s, %d, %d\n", function_name, a, b, c);                 
        }                                                                       
        else                                                                    
        {                                                                       
            printk("%s: %s, 0x%x, 0x%x\n", function_name, a, b, c);             
        }                                                                       
    }                                                                           
}                                                                               
                                                                                
void log_print_int(log_verbose_t verbose, const char* a, unsigned b)            
{                                                                               
    if (verbose <= log_verbose)                                                 
    {                                                                           
        if (log_numeric == NUMERIC_DEC)                                         
        {                                                                       
            printk("%s: %d\n", a, b);                                           
        }                                                                       
        else                                                                    
        {                                                                       
            printk("%s: 0x%x\n", a, b);                                         
        }                                                                       
    }                                                                           
}                                                                               
                                                                                
void log_print_int2(log_verbose_t verbose, const char* a, unsigned b,           
        unsigned c)                                                             
{                                                                               
    if (verbose <= log_verbose)                                                 
    {                                                                           
        if (log_numeric == NUMERIC_DEC)                                         
        {                                                                       
            printk("%s: %d, %d\n", a, b, c);                                    
        }                                                                       
        else                                                                    
        {                                                                       
            printk("%s: 0x%x, 0x%x\n", a, b, c);                                
        }                                                                       
    }                                                                           
}                                                                               
                                                                                
void log_print_int3(log_verbose_t verbose, const char* a, unsigned b,           
        unsigned c, unsigned d)                                                 
{                                                                               
    if (verbose <= log_verbose)                                                 
    {                                                                           
        if (log_numeric == NUMERIC_DEC)                                         
        {                                                                       
            printk("%s: %d, %d, %d\n", a, b, c, d);                             
        }                                                                       
        else                                                                    
        {                                                                       
            printk("%s: 0x%x, 0x%x, 0x%x\n", a, b, c, d);                       
        }                                                                       
    }                                                                           
}                                                                               
                                                                                
#ifdef __cplusplus                                                              
}                                                                               
#endif                                                                          
                                                                                      