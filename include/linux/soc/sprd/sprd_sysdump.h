#ifndef __SPRD_PLATFORM_SYSDUMP_H
#define __SPRD_PLATFORM_SYSDUMP_H

/**
 * save extend debug information of modules in minidump, such as: cm4, iram...
 *
 * @name:	the name of the modules, and the string will be a part
 *		of the file name.
 *		note: special characters can't be included in the file name,
 *		such as:'?','*','/','\','<','>',':','"','|'.
 *
 * @paddr_start:the start paddr in memory of the modules debug information
 * @paddr_end:	the end paddr in memory of the modules debug information
 *
 * Return: 0 means success, -1 means fail.
 */
extern int minidump_save_extend_information(const char *name,
						unsigned long paddr_start,
						unsigned long paddr_end);

/*
 * save per-cpu's stack and regs in sysdump.
 *
 * @cpu:	the cpu number;
 *
 * @pregs:	pt_regs.
 */
extern void sprd_dump_stack_reg(int cpu, struct pt_regs *pregs);


#endif
