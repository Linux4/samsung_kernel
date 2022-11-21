#ifndef CONFIG_SYSDUMP64_H
#define CONFIG_SYSDUMP64_H

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

extern int sprd_dump_mem_num;
extern struct sysdump_mem sprd_dump_mem[];

#endif
