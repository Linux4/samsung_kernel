#ifndef __SPRD_PLATFORM_SYSDUMP_H
#define __SPRD_PLATFORM_SYSDUMP_H

#define SPRD_SYSDUMP_MAGIC      (0x80000000 + 0x20000000 - (1024<<10))

enum crash_type {
	FRAMEWORK_CRASH,
	KERNEL_CRASH,
	CRASH_TYPE_END,
};

struct crash_info {
	char magic[16];
	char time[32];
	int size;
	void *payload;
};

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

int sysdump_fill_crash_content(int type, char *magic, char *payload, int size);
int dumpinfo_init(void);
void dumpinfo_exit(void);
void add_kernel_log(const char *format, ...);
void dump_kernel_crash(const char *reason, struct pt_regs *regs);

#endif
