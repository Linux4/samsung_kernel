//============================================================ cust log ====
#ifndef __TYHX_LOG__
#define __TYHX_LOG__

static int tyhx_log_level = 3;

#define filename(x) strrchr(x,'/')?strrchr(x,'/')+1:x
#define ENTER \
do{ if(tyhx_log_level >= 3) printk(KERN_INFO "[TYHX_DBG][%s][%04d][%s]\n", filename(__FILE__), __LINE__, __func__); }while(0)

#define PRINT_DBG(format,x...) \
do{ if(tyhx_log_level >= 2) printk(KERN_INFO "[TYHX_DBG][%s][%04d][%s] " format, filename(__FILE__), __LINE__, __func__, ## x); }while(0)

#define PRINT_INF(format,x...) \
do{ if(tyhx_log_level >= 1) printk(KERN_INFO "[TYHX_INF][%s][%04d][%s] " format, filename(__FILE__), __LINE__, __func__, ## x); }while(0)

#define PRINT_ERR(format,x...) \
do{ if(tyhx_log_level >= 0) printk(KERN_ERR "[TYHX_ERR][%s][%04d][%s] " format, filename(__FILE__), __LINE__, __func__, ## x); }while(0)

#endif
//============================================================ cust log ====
