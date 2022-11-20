/* linux/arch/arm/mach-sc8830/platsmp.c
 *
 * Copyright (c) 2010-2012 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Cloned from linux/arch/arm/mach-vexpress/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>
#include <asm/unified.h>

#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/sci.h>
#include <asm/atomic.h>

extern void sci_secondary_startup(void);

#if (defined CONFIG_ARCH_SC8825)
static int __cpuinit boot_secondary_cpus(int cpu_id, u32 paddr)
{
	if (cpu_id != 1)
		return -1;
	writel(paddr,CPU_JUMP_VADDR);
	writel(1 << (cpu_id * 4),HOLDING_PEN_VADDR);
	return 0;
}

#elif (defined CONFIG_ARCH_SCX35)

#define CA7_CORE1_PD (BIT(8)|BIT(9)|BIT(10))
#define CA7_CORE2_PD (BIT(12)|BIT(13)|BIT(14))
#define CA7_CORE3_PD (BIT(16)|BIT(17)|BIT(18))
int scxx30_all_nonboot_cpus_died(void)
{
	u32 val;
	u32 core1_pd , core2_pd , core3_pd;

	val = sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG, -1UL);

	core1_pd = val & CA7_CORE1_PD;
	core2_pd = val & CA7_CORE2_PD;
	core3_pd = val & CA7_CORE3_PD;

	if(core1_pd && core2_pd && core3_pd){
		pr_debug("*** %s, REG_PMU_APB_PWR_STATUS0_DBG:0x%x ***\n", __func__, val);
		return 1;
	}
	else
		return 0;
}

int poweron_cpus(int cpu)
{
	u32 poweron, val;

	if (cpu == 1)
		poweron = REG_PMU_APB_PD_CA7_C1_CFG;
	else if (cpu == 2)
		poweron = REG_PMU_APB_PD_CA7_C2_CFG;
	else if (cpu == 3)
		poweron = REG_PMU_APB_PD_CA7_C3_CFG;
	else
		return -1;


	val = sci_glb_read(REG_AP_AHB_CA7_RST_SET, -1UL);
	val |= (1 << cpu);
	sci_glb_write(REG_AP_AHB_CA7_RST_SET, val, -1UL);

	val = BITS_PD_CA7_C3_PWR_ON_DLY(4) | BITS_PD_CA7_C3_PWR_ON_SEQ_DLY(4) | BITS_PD_CA7_C3_ISO_ON_DLY(4);
	sci_glb_write(poweron, val, -1UL);

	val = (BIT_PD_CA7_C3_AUTO_SHUTDOWN_EN | __raw_readl(poweron)) &~(BIT_PD_CA7_C3_FORCE_SHUTDOWN);
	sci_glb_write(poweron, val, -1UL);
	dmb();
	udelay(1000);

	while((__raw_readl(poweron)&BIT_PD_CA7_C3_FORCE_SHUTDOWN) ||
		!(__raw_readl(REG_AP_AHB_CA7_RST_SET)&(1 << cpu)) ){
		pr_debug("??? %s set regs failed ???\n", __func__ );
		writel((__raw_readl(REG_AP_AHB_CA7_RST_SET) | (1 << cpu)), REG_AP_AHB_CA7_RST_SET);
		val = (BIT_PD_CA7_C3_AUTO_SHUTDOWN_EN | __raw_readl(poweron)) &~(BIT_PD_CA7_C3_FORCE_SHUTDOWN);
		writel(val,poweron);
		dmb();
		udelay(500);
	}
	writel((__raw_readl(REG_AP_AHB_CA7_RST_SET) & ~(1 << cpu)), REG_AP_AHB_CA7_RST_SET);
	return 0;
}

int powerdown_cpus(int cpu)
{
	u32 poweron, val, i = 0;
	if (cpu == 1)
		poweron = REG_PMU_APB_PD_CA7_C1_CFG;
	else if (cpu == 2)
		poweron = REG_PMU_APB_PD_CA7_C2_CFG;
	else if (cpu == 3)
		poweron = REG_PMU_APB_PD_CA7_C3_CFG;
	else
		return -1;

	val = (BIT_PD_CA7_C3_FORCE_SHUTDOWN | __raw_readl(poweron)) &~(BIT_PD_CA7_C3_AUTO_SHUTDOWN_EN);
	writel(val, poweron);


	while (i < 20) {
		//check power down?
		if (((__raw_readl(REG_PMU_APB_PWR_STATUS0_DBG) >> (4 * (cpu_logical_map(cpu) + 1))) & 0x0f) == 0x07) {
			break;
		}
		udelay(60);
		i++;
	}
	printk("powerdown_cpus i=%d !!\n", i);
	BUG_ON(i >= 20);
	udelay(60);

	return 0;
}

static int __cpuinit boot_secondary_cpus(int cpu_id, u32 paddr)
{
	if (cpu_id < 1 || cpu_id > 3)
		return -1;

	writel(paddr,(CPU_JUMP_VADDR + (cpu_id << 2)));
	writel(readl(HOLDING_PEN_VADDR) | (1 << cpu_id),HOLDING_PEN_VADDR);
	poweron_cpus(cpu_id);

	return 0;
}

#endif

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
extern volatile int pen_release;

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static DEFINE_SPINLOCK(boot_lock);


unsigned int  sprd_boot_magnum =	0xbadf1a90;

#define SPRD_UP_FLAG0	(0x63507530)
#define SPRD_UP_FLAG1	(0x63507531)
#define SPRD_UP_FLAG2	(0x63507532)
#define SPRD_UP_FLAG3	(0x63507533)

unsigned int g_sprd_boot_flag_1 = 0;
unsigned int g_sprd_boot_flag_2 = 0;
char * ga_boot_flag1[4] =
	{
		"bf10",
		"bf11",
		"bf12",
		"bf13"
	};
char * ga_boot_flag2[4] =
	{
		"bf20",
		"bf21",
		"bf22",
		"bf23"
	};

unsigned int g_sprd_up_flag[4] =
	{
		SPRD_UP_FLAG0,
		SPRD_UP_FLAG1,
		SPRD_UP_FLAG2,
		SPRD_UP_FLAG3
	};

atomic_t boot_lock_cnt  = ATOMIC_INIT(0);
atomic_t boot_unlock_cnt = ATOMIC_INIT(0);

void __cpuinit sprd_secondary_init(unsigned int cpu)
{
	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	//gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */

	write_pen_release(g_sprd_up_flag[cpu]);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
	atomic_inc(&boot_unlock_cnt);
	g_sprd_boot_flag_1 = 0;
	g_sprd_boot_flag_2 = 0;

}



int __cpuinit sprd_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;
	int ret;
	unsigned int val = sprd_boot_magnum;

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);
	atomic_inc(&boot_lock_cnt);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 */

	memcpy(&g_sprd_boot_flag_1,ga_boot_flag1[cpu],4);

	val |= (cpu_logical_map(cpu) & 0x0000000f);

	write_pen_release((int)val);

	memcpy(&g_sprd_boot_flag_2,ga_boot_flag2[cpu],4);

	ret = boot_secondary_cpus(cpu, virt_to_phys(sci_secondary_startup));
	if (ret < 0)
		pr_warn("SMP: boot_secondary(%u) error\n", cpu);

	dsb_sev();


	timeout = jiffies + (1 * HZ);

	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == g_sprd_up_flag[cpu])
			break;

		udelay(10);
		dsb_sev();
	}

	spin_unlock(&boot_lock);

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	if(pen_release != g_sprd_up_flag[cpu]){
		printk("*** %s, pen_release:%x ***\n ", __func__, pen_release);
		printk("*** %s, REG_PMU_APB_PWR_STATUS0_DBG:0x%x \n", __func__, sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG, -1UL));
		printk("*** %s, REG_AP_AHB_CA7_RST_SET:0x%x \n", __func__, sci_glb_read(REG_AP_AHB_CA7_RST_SET, -1UL));
		printk("*** %s, REG_AP_AHB_CA7_STANDBY_STATUS:0x%x \n", __func__, sci_glb_read(REG_AP_AHB_CA7_STANDBY_STATUS, -1UL));
		printk("*** %s, REG_AON_APB_MPLL_CFG:0x%x \n", __func__, sci_glb_read(REG_AON_APB_MPLL_CFG, -1UL));
	}

	return pen_release !=  g_sprd_up_flag[cpu] ? -ENOSYS : 0;
}

#ifdef CONFIG_HAVE_ARM_SCU
static unsigned int __get_core_num(void)
{
	void __iomem *scu_base = (void __iomem *)(SPRD_CORE_BASE);
	return (scu_base ? scu_get_core_count(scu_base) : 1);
}
#else
static unsigned int __get_core_num(void)
{
	return 4;
}
#endif

static unsigned int sci_get_core_num(void)
{
	return __get_core_num();
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init sprd_smp_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = sci_get_core_num();

	/* sanity check */
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

}
#ifdef CONFIG_FIX_V7TAGRAM_BUG
unsigned long physical_from_idle = 0;
#endif
void __init sprd_smp_prepare_cpus(unsigned int max_cpus)
{
#ifdef CONFIG_HAVE_ARM_SCU
	scu_enable((void __iomem *)(SPRD_CORE_BASE));
#endif
#ifdef CONFIG_FIX_V7TAGRAM_BUG
	extern unsigned long other_cpu_fix_phys;
	extern void fix_tag_ram_bug(void);
	extern void other_cpu_fix(void);

	if(!other_cpu_fix_phys)
		other_cpu_fix_phys = virt_to_phys(other_cpu_fix);
	local_irq_disable();
	fix_tag_ram_bug();
	local_irq_enable();
#endif
}

extern int sprd_cpu_kill(unsigned int cpu);
extern int sprd_cpu_die(unsigned int cpu);
extern int sprd_cpu_disable(unsigned int cpu);

struct smp_operations sprd_smp_ops __initdata = {
	.smp_init_cpus      = sprd_smp_init_cpus,
	.smp_prepare_cpus   = sprd_smp_prepare_cpus,
	.smp_secondary_init = sprd_secondary_init,
	.smp_boot_secondary = sprd_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_kill		= sprd_cpu_kill,
	.cpu_disable	= sprd_cpu_disable,
	.cpu_die        = sprd_cpu_die,
#endif
};
