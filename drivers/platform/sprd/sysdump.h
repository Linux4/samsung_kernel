#ifndef __SPRD_PLATFORM_SYSDUMP_H
#define __SPRD_PLATFORM_SYSDUMP_H

#define SPRD_SYSDUMP_MAGIC      (0x80000000 + 0x20000000 - (1024<<10))

struct sysdump_mem {
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long soff;
	size_t size;
	int type;
};

enum sysdump_type {
	SYSDUMP_RAM,
	SYSDUMP_MODEM,
	SYSDUMP_IOMEM,
};

#ifdef CONFIG_ARM
#include <soc/sprd/sysdump32.h>
#endif

#ifdef CONFIG_ARM64
#include <soc/sprd/sysdump64.h>
#endif

#endif
