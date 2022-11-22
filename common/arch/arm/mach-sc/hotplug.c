/* linux arch/arm/mach-sc8830/hotplug.c
 *
 *  Cloned from linux/arch/arm/mach-realview/hotplug.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <asm/smp_plat.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

extern volatile int pen_release;
#ifdef CONFIG_ARCH_SCX35
int powerdown_cpus(int cpu);
int sci_shark_enter_lowpower(void);
#endif
static inline void cpu_enter_lowpower(void)
{
	unsigned int v;

	flush_cache_all();
	asm volatile(
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	mcr	p15, 0, %1, c7, c10, 4\n"
	/*
	 * Turn off coherency
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, #0x20\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "r" (0), "Ir" (CR_C)
	  : "cc");
}

static inline void cpu_leave_lowpower(void)
{
	unsigned int v;

	asm volatile(	"mrc	p15, 0, %0, c1, c0, 0\n"
	"	orr	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	orr	%0, %0, #0x20\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	  : "=&r" (v)
	  : "Ir" (CR_C)
	  : "cc");
}

extern unsigned int  sprd_boot_magnum;

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	unsigned int val = sprd_boot_magnum;

	/*
	 * there is no power-control hardware on this platform, so all
	 * we can do is put the core into WFI; this is safe as the calling
	 * code will have already disabled interrupts
	 */

	val |= (cpu_logical_map(cpu) & 0x0000000f);
	for (;;) {
		/*
		 * here's the WFI
		 */
		asm(".word	0xe320f003\n"
		    :
		    :
		    : "memory", "cc");

		if (pen_release == val) {
			/*
			 * OK, proper wakeup, we're done
			 */
			 pr_info("platform_do_lowpower %x %x\n",pen_release,val);
			break;
		}

		/*
		 * Getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * Just note it happening - when we're woken, we can report
		 * its occurrence.
		 */
		(*spurious)++;
	}
}

int sprd_cpu_kill(unsigned int cpu)
{
#ifdef CONFIG_ARCH_SCX35
	int i = 0;
	printk("!! %d  platform_cpu_kill %d !!\n", smp_processor_id(), cpu);
	while (i < 20) {
		//check wfi?
		if (sci_glb_read(REG_AP_AHB_CA7_STANDBY_STATUS, -1UL) & (1 << cpu_logical_map(cpu)))
		{
			powerdown_cpus(cpu);
			break;
		}
		udelay(100);
		i++;
	}
	printk("platform_cpu_kill finished i=%d !!\n", i);
	return (i >= 20? 0 : 1);
#endif
	return 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void __cpuinit sprd_cpu_die(unsigned int cpu)
{
	int spurious = 0;

#ifdef CONFIG_ARCH_SCX35
	sci_shark_enter_lowpower();
	panic("shouldn't be here \n");
#endif
	/*
	 * we're ready for shutdown now, so do it
	 */
	cpu_enter_lowpower();
	platform_do_lowpower(cpu, &spurious);

	/*
	 * bring this CPU back into the world of cache
	 * coherency, and then restore interrupts
	 */
	cpu_leave_lowpower();

	if (spurious)
		pr_warn("CPU%u: %u spurious wakeup calls\n", cpu, spurious);
}

int sprd_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
