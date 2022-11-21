#ifndef __SYSDEP_H__
#define __SYSDEP_H__

#include <linux/idr.h>
#include <linux/version.h>

#include <asm/cacheflush.h>

#if defined(CONFIG_ARM64)
#define outer_inv_range(s, e)
#else
#define __flush_dcache_area(s, e)	__cpuc_flush_dcache_area(s, e)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
#define IDR_REMOVE_ALL(id)
#else
#define IDR_REMOVE_ALL(id)	idr_remove_all(id)
#endif

int sysdep_idr_alloc(struct idr *idr, void *mem);

#endif /* __SYSDEP_H__ */
