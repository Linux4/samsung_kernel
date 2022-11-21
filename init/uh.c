#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/cacheflush.h>
#include <asm/irqflags.h>
#include <linux/fs.h>
#include <asm/tlbflush.h>
#include <linux/init.h>
#include <asm/io.h>

#include <linux/uh.h>
#include "ld.h"

#define UH_32BIT_SMC_CALL_MAGIC 0x82000400
#define UH_64BIT_SMC_CALL_MAGIC 0xC2000400

#define UH_STACK_OFFSET 0x2000

#define UH_MODE_AARCH32 0
#define UH_MODE_AARCH64 1


char * __initdata uh;
size_t __initdata uh_size;

extern char _start_uh;
extern char _end_uh;
extern char _uh_disable;

int __init uh_disable(void)
{
	_uh_goto_EL2(UH_64BIT_SMC_CALL_MAGIC, (void *)virt_to_phys(&_uh_disable), UH_STACK_OFFSET, UH_MODE_AARCH64, NULL, 0);

	printk(KERN_ALERT "%s\n", __FUNCTION__);

	return 0;
}

static int __init uh_entry(void *head_text_base)
{
	typedef int (*_main_)(void);

	int status;
	_main_ entry_point;

	entry_point = (_main_)head_text_base;

	printk(KERN_ALERT "uh entry point: %p\n", entry_point);

	uh = (char *)virt_to_phys(uh);

	flush_cache_all();

	status = _uh_goto_EL2(UH_64BIT_SMC_CALL_MAGIC, entry_point, UH_STACK_OFFSET, UH_MODE_AARCH64, uh, uh_size);

	printk(KERN_ALERT "uh add: %p, size: 0x%x, status: %x\n", uh, (int)uh_size, status);

	return 0;
}

int __init uh_init(void)
{
	size_t bss_size = 0,head_text_size = 0;
	u64 bss_base = 0,head_text_base = 0;
	void *base;

	if(smp_processor_id() != 0) { return 0; }

	printk(KERN_ALERT "%s: bin 0x%p, 0x%x\n", __func__, &_start_uh, (int)(&_end_uh - &_start_uh));
	memcpy((void *)phys_to_virt(UH_START),  &_start_uh, (size_t)(&_end_uh - &_start_uh));

	uh = (void *)phys_to_virt(UH_START);
	uh_size = UH_SIZE;

	if(ld_get_sect(uh, ".bss", &base, &bss_size)) { return -1; }
	printk(KERN_ALERT "%s: BSS base = 0x%p, 0x%x\n", __func__, base, (int)bss_size);
	bss_base = (u64)virt_to_phys(base);
	
	if(ld_get_sect(uh, ".text.head", &base, &head_text_size)) { return -1; }
	printk(KERN_ALERT "%s: Head Text base = 0x%p, 0x%x\n", __func__, base, (int)head_text_size);
	head_text_base = (u64)virt_to_phys(base);	
	

	memset((void *)phys_to_virt(bss_base), 0, (size_t)bss_size);
	//Now uh will be freed by free_initmem, no need to zero out
	//memset(&_start_uh, 0, (size_t)(&_end_uh - &_start_uh));

	uh_entry((void *)head_text_base);

	return 0;
}
