/*                                                                                     
 * log.h                                                                               
 *                                                                                     
 *  Created on: Jun 25, 2010                                                           
 *      Author: klabadi & dlopo                                                        
 */                                                                                    
                                                                                       
#ifndef LOG_H_                                                                         
#define LOG_H_                                                                         

#define __FUNCTION__ (__func__)

typedef enum                                                                           
{                                                                                      
    VERBOSE_ERROR = 0,                                                                 
    VERBOSE_WARN,                                                                      
    VERBOSE_NOTICE,                                                                    
    VERBOSE_DEBUG,                                                                     
    VERBOSE_TRACE                                                                      
} log_verbose_t;                                                                       
                                                                                       
typedef enum                                                                           
{                                                                                      
    NUMERIC_DEC = 0, NUMERIC_HEX                                                       
} log_numeric_t;                                                                       
                                                                                       
void log_set_verbose(log_verbose_t verbose);                                           
void log_set_numeric(log_numeric_t numeric);                                           
void log_set_verbose_write(unsigned state);                                            
                                                                                       
void log_print_write(unsigned a, unsigned b);                                          
                                                                                       
void log_print0(log_verbose_t verbose, const char* function_name);                     
                                                                                       
void log_print1(log_verbose_t verbose, const char* function_name, const char* a);      
                                                                                       
void log_print2(log_verbose_t verbose, const char* function_name, const char* a,       
        unsigned b);                                                                   
                                                                                       
void log_print3(log_verbose_t verbose, const char* function_name, const char* a,       
        unsigned b, unsigned c);                                                       
                                                                                       
void log_print_int(log_verbose_t verbose, const char* a, unsigned b);                  
                                                                                       
void log_print_int2(log_verbose_t verbose, const char* a, unsigned b,                  
        unsigned c);                                                                   
                                                                                       
void log_print_int3(log_verbose_t verbose, const char* a, unsigned b,                  
        unsigned c, unsigned d);                                                       
                                                                                       
#define LOG_ASSERT(x, a)        while (!(x)) { LOG_ERROR(a); exit(-1); }               
#define LOG_EXIT(a)             do { LOG_ERROR(a); exit(-1); } while (0)               
#define LOG_WRITE(a, b)         log_print_write((a), (b))                              
                                                                                       
#define LOG_ERROR(a)            log_print1(VERBOSE_ERROR,  __FUNCTION__, (a))          
#define LOG_ERROR2(a, b)        log_print2(VERBOSE_ERROR,  __FUNCTION__, (a), (b))     
#define LOG_ERROR3(a, b, c)     log_print3(VERBOSE_ERROR,  __FUNCTION__, (a), (b), (c))
#define LOG_WARNING(a)          log_print1(VERBOSE_WARN,   __FUNCTION__, (a))          
#define LOG_WARNING2(a, b)      log_print2(VERBOSE_WARN,   __FUNCTION__, (a), (b))     
#define LOG_WARNING3(a, b, c)   log_print3(VERBOSE_WARN,   __FUNCTION__, (a), (b), (c))
#define LOG_NOTICE(a)           log_print1(VERBOSE_NOTICE, __FUNCTION__, (a))          
#define LOG_NOTICE2(a, b)       log_print2(VERBOSE_NOTICE, __FUNCTION__, (a), (b))     
#define LOG_NOTICE3(a, b, c)    log_print3(VERBOSE_NOTICE, __FUNCTION__, (a), (b), (c))
#define LOG_DEBUG(a)            log_print1(VERBOSE_DEBUG,  __FUNCTION__, (a))          
#define LOG_DEBUG2(a, b)        log_print2(VERBOSE_DEBUG,  __FUNCTION__, (a), (b))     
#define LOG_DEBUG3(a, b, c)     log_print3(VERBOSE_DEBUG,  __FUNCTION__, (a), (b), (c))
#define LOG_TRACE()             log_print0(VERBOSE_TRACE,  __FUNCTION__)               
#define LOG_TRACE1(a)           log_print_int(VERBOSE_TRACE,  __FUNCTION__, (a))       
#define LOG_TRACE2(a, b)        log_print_int2(VERBOSE_TRACE,  __FUNCTION__, (a), (b)) 
#define LOG_TRACE3(a, b, c)     log_print3(VERBOSE_TRACE,  __FUNCTION__, (a), (b), (c))
                                                                                       
#define LOG_MENU(a)                 log_print0(VERBOSE_NOTICE, (a))                    
#define LOG_MENUSTR(a, b)           log_print1(VERBOSE_NOTICE, (a), (b))               
#define LOG_MENUINT(a, b)           log_print_int(VERBOSE_NOTICE, (a),(b))             
#define LOG_MENUINT2(a, b, c)       log_print_int2(VERBOSE_NOTICE, (a),(b), (c))       
#define LOG_MENUINT3(a, b, c, d)    log_print_int3(VERBOSE_NOTICE, (a),(b), (c), (d))  
                                                                                       
#endif /* LOG_H_ */                                                                    
                                                                                             