/*
 *  linux/arch/arm/mach-mmp/reset.c
 *
 *  Copyright (C) 2009-2011 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-timers.h>
#include <mach/cputype.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>

#ifdef CONFIG_MFD_D2199
#include <linux/d2199/core.h>
#include <linux/d2199/pmic.h>
#include <linux/d2199/d2199_reg.h>
#include <linux/d2199/d2199_battery.h>
#endif

#ifdef CONFIG_MFD_88PM822
#include <linux/mfd/88pm822.h>
#endif

#define REG_RTC_BR0	(APB_VIRT_BASE + 0x010014)

#define MPMU_APRR_WDTR	(1<<4)
#define MPMU_APRR_CPR	(1<<0)
#define MPMU_CPRR_DSPR	(1<<2)
#define MPMU_CPRR_BBR	(1<<3)

int is_panic = 1;

/* Using watchdog reset */
static void do_wdt_reset(const char *cmd)
{
	u32 reg, backup;
	void __iomem *watchdog_virt_base;
	int i;
	int match = 0, count = 0;

	if (cpu_is_pxa910() || cpu_is_pxa988() || cpu_is_pxa986() ||
			cpu_is_pxa1088())
		watchdog_virt_base = CP_TIMERS2_VIRT_BASE;
	else if (cpu_is_pxa168())
		watchdog_virt_base = TIMERS1_VIRT_BASE;
	else
		return;

	/*Hold cp to avoid reset watchdog*/
	if (cpu_is_pxa910() || cpu_is_pxa988() || cpu_is_pxa986() ||
			cpu_is_pxa1088()) {
		/*hold CP first */
		reg = readl(MPMU_APRR) | MPMU_APRR_CPR;
		writel(reg, MPMU_APRR);
		udelay(10);
		/*CP reset MSA */
		reg = readl(MPMU_CPRR) | MPMU_CPRR_DSPR | MPMU_CPRR_BBR;
		writel(reg, MPMU_CPRR);
		udelay(10);
	}

	/*If reboot by recovery, store info for uboot*/
	if (cpu_is_pxa910() || cpu_is_pxa988() || cpu_is_pxa986() ||
			cpu_is_pxa1088()) {
		if (cmd && !strcmp(cmd, "recovery")) {
			for (i = 0, backup = 0; i < 4; i++) {
				backup <<= 8;
				backup |= *(cmd + i);
			}
			do {
				writel(backup, REG_RTC_BR0);
			} while (readl(REG_RTC_BR0) != backup);
		}
	}

	/* reset/enable WDT clock */
	writel(0x7, MPMU_WDTPCR);
	readl(MPMU_WDTPCR);
	writel(0x3, MPMU_WDTPCR);
	readl(MPMU_WDTPCR);

	/* enable WDT reset */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0x3, watchdog_virt_base + TMR_WMER);

	/* negate hardware reset to the WDT after system reset */
	reg = readl(MPMU_APRR) | MPMU_APRR_WDTR;
	writel(reg, MPMU_APRR);

	/* clear previous WDT status */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0, watchdog_virt_base + TMR_WSR);

	match = readl(watchdog_virt_base + TMR_WMR);
	count = readl(watchdog_virt_base + TMR_WVR);

	/* set match counter */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel((0x20 + count) & 0xFFFF, watchdog_virt_base + TMR_WMR);
}

int pxa_board_reset(char mode, const char *cmd)
{
	struct membank *bank;
	int i;

	flush_cache_all();

	for (i = 0; i < meminfo.nr_banks; i ++) {
		bank = &meminfo.bank[i];
		if (bank->size)
			outer_flush_range(bank->start, bank->size);
	}

	return 0;
}

int (*board_reset)(char mode, const char *cmd) = pxa_board_reset;
void mmp_arch_reset(char mode, const char *cmd)
{
	int count = 10;
#if defined(CONFIG_MFD_D2199)
	static unsigned char data;
#endif

	if ((!cpu_is_pxa910()) && (!cpu_is_pxa168()) &&
	    (!cpu_is_pxa988()) && (!cpu_is_pxa986()) &&
	    (!cpu_is_pxa1088()))
		return;
#if defined(CONFIG_MFD_D2199)
	if (is_panic) {
		/* dump buck1 voltage */
		d2199_extern_reg_read(D2199_BUCK2PH_BUCK1_REG, &data);
		pr_info("buck1 voltage: 0x%x\n", data);

		d2199_extern_reg_write(D2199_BUCK2PH_BUCK1_REG, 0xd8);

		/* double check */
		d2199_extern_reg_read(D2199_BUCK2PH_BUCK1_REG, &data);
		pr_info("buck1 voltage: 0x%x\n", data);
	}
#endif

	printk("%s (%c)\n", __func__, mode);

	if (board_reset(mode, cmd))
		return;

	switch (mode) {
	case 's':
		/* Jump into ROM at address 0 */
		cpu_reset(0);
		break;
	case 'w':
	default:
		while(count--) {
			do_wdt_reset(cmd);
			mdelay(1000);
			printk("Watchdog fail...retry\n");
		}
		break;
	}

}
