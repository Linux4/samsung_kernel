#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <linux/printk.h>

#define mcps_loge(fmt, ...) printk(KERN_ERR "TESTE[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)
#define mcps_logd(fmt, ...) printk(KERN_DEBUG "TESTD[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)

#if defined(CONFIG_KUNIT)

#ifndef __visible_for_testing 
#define __visible_for_testing
#endif //#ifndef __visible_for_testing 

#define mcps_bug()

#else // #if defined(CONFIG_KUNIT)

#ifndef __visible_for_testing
#define __visible_for_testing static
#endif //#ifndef __visible_for_testing

#define mcps_bug() BUG()

#endif // #if defined(CONFIG_KUNIT)

#endif
