#ifndef __TZ_MMAP_TEST_H__
#define __TZ_MMAP_TEST_H__

#include <linux/types.h>

#define TZ_MMAP_IOC_MAGIC		'c'

#define TZ_MMAP_GET_PADDR_STATIC	_IOR(TZ_MMAP_IOC_MAGIC, 0, unsigned long)
#define TZ_MMAP_GET_PADDR_DYNAMIC	_IOR(TZ_MMAP_IOC_MAGIC, 1, unsigned long)
#define TZ_MMAP_FILL_STATIC		_IOW(TZ_MMAP_IOC_MAGIC, 2, unsigned long)
#define TZ_MMAP_FILL_DYNAMIC		_IOW(TZ_MMAP_IOC_MAGIC, 3, unsigned long)

#endif /*!__TZ_MMAP_TEST_H__*/
