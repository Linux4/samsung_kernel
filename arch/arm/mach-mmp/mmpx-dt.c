/*
 *  linux/arch/arm64/mach/mmpx-dt.c
 *
 *  Copyright (C) 2012 Marvell Technology Group Ltd.
 *  Author: Haojian Zhuang <haojian.zhuang@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/irqchip.h>
#include <linux/of_platform.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/memblock.h>

#include <asm/mach/arch.h>

#include <linux/cputype.h>

#include "common.h"
#include "regs-addr.h"
#include "mmpx-dt.h"

#ifdef CONFIG_SD8XXX_RFKILL
#include <linux/sd8x_rfkill.h>
#endif

static const struct of_dev_auxdata mmpx_auxdata_lookup[] __initconst = {
	OF_DEV_AUXDATA("mrvl,mmp-sspa-dai", 0xD128dc00, "mmp-sspa-dai.0", NULL),
	OF_DEV_AUXDATA("mrvl,mmp-sspa-dai", 0xD128dd00, "mmp-sspa-dai.1", NULL),
	OF_DEV_AUXDATA("mrvl,pxa910-ssp", 0xd42a0c00, "pxa988-ssp.1", NULL),
	OF_DEV_AUXDATA("mrvl,pxa910-ssp", 0xd4039000, "pxa988-ssp.4", NULL),
	OF_DEV_AUXDATA("mrvl,pxa-ssp-dai", 1, "pxa-ssp-dai.1", NULL),
	OF_DEV_AUXDATA("mrvl,pxa-ssp-dai", 4, "pxa-ssp-dai.2", NULL),
	OF_DEV_AUXDATA("marvell,pxa-88pm805-snd-card", 0, "sound", NULL),
#ifdef CONFIG_SD8XXX_RFKILL
	OF_DEV_AUXDATA("mrvl,sd8x-rfkill", 0, "sd8x-rfkill", NULL),
#endif
	OF_DEV_AUXDATA("marvell,mmp-disp", 0xd420b000, "mmp-disp", NULL),
	{}
};

#define MPMU_PHYS_BASE		0xd4050000
#define MPMU_APRR		(0x1020)
#define MPMU_WDTPCR		(0x0200)

/* wdt and cp use the clock */
static __init void enable_pxawdt_clock(void)
{
	void __iomem *mpmu_base;

	mpmu_base = ioremap(MPMU_PHYS_BASE, SZ_8K);
	if (!mpmu_base) {
		pr_err("ioremap mpmu_base failed\n");
		return;
	}

	/* reset/enable WDT clock */
	writel(0x7, mpmu_base + MPMU_WDTPCR);
	readl(mpmu_base + MPMU_WDTPCR);
	writel(0x3, mpmu_base + MPMU_WDTPCR);

	iounmap(mpmu_base);
}

#define GENERIC_COUNTER_PHYS_BASE	0xd4101000
#define CNTCR				0x00 /* Counter Control Register */
#define CNTCR_EN			(1 << 0) /* The counter is enabled */
#define CNTCR_HDBG			(1 << 1) /* Halt on debug */

#define APBC_PHY_BASE			0xd4015000
#define APBC_COUNTER_CLK_SEL		0x64
#define FREQ_HW_CTRL			0x1
static __init void enable_arch_timer(void)
{
	void __iomem *apbc_base, *tmr_base;
	u32 tmp;

	apbc_base = ioremap(APBC_PHY_BASE, SZ_4K);
	if (!apbc_base) {
		pr_err("ioremap apbc_base failed\n");
		return;
	}

	tmr_base = ioremap(GENERIC_COUNTER_PHYS_BASE, SZ_4K);
	if (!tmr_base) {
		pr_err("opremap tmr_base failed\n");
		iounmap(apbc_base);
		return;
	}

	tmp = readl(apbc_base + APBC_COUNTER_CLK_SEL);

	/* Default is 26M/32768 = 0x319 */
	if ((tmp >> 16) != 0x319) {
		pr_warn("Generic Counter step of Low Freq is not right\n");
		iounmap(apbc_base);
		iounmap(tmr_base);
		return;
	}
	/*
	 * bit0 = 1: Generic Counter Frequency control by hardware VCTCXO_EN
	 * VCTCXO_EN = 1, Generic Counter Frequency is 26Mhz;
	 * VCTCXO_EN = 0, Generic Counter Frequency is 32KHz.
	 */
	writel(tmp | FREQ_HW_CTRL, apbc_base + APBC_COUNTER_CLK_SEL);

	/*
	 * NOTE: can not read CNTCR before write, otherwise write will fail
	 * Halt on debug;
	 * start the counter
	 */
	writel(CNTCR_HDBG | CNTCR_EN, tmr_base + CNTCR);

	iounmap(tmr_base);
	iounmap(apbc_base);
}

/* Common APB clock register bit definitions */
#define APBC_APBCLK	(1 << 0)  /* APB Bus Clock Enable */
#define APBC_FNCLK	(1 << 1)  /* Functional Clock Enable */
#define APBC_RST	(1 << 2)  /* Reset Generation */

/* Functional Clock Selection Mask */
#define APBC_FNCLKSEL(x)	(((x) & 0xf) << 4)

static u32 timer_clkreg[] = {0x34, 0x44, 0x68, 0x0};

static __init void enable_soc_timer(void)
{
	void __iomem *apbc_base;
	int i;

	apbc_base = ioremap(APBC_PHY_BASE, SZ_4K);
	if (!apbc_base) {
		pr_err("ioremap apbc_base failed\n");
		return;
	}

	for (i = 0; timer_clkreg[i]; i++) {
		/* Select the configurable clock rate to be 3.25MHz */
		writel(APBC_APBCLK | APBC_RST, apbc_base + timer_clkreg[i]);
		writel(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3),
			apbc_base + timer_clkreg[i]);
	}

	iounmap(apbc_base);
}

#define APMU_SDH0      0x54
static void __init pxa988_sdhc_reset_all(void)
{
	unsigned int reg_tmp;
	void __iomem *apmu_base;

	apmu_base = regs_addr_get_va(REGS_ADDR_APMU);
	if (!apmu_base) {
		pr_err("failed to get apmu_base va\n");
		return;
	}

	/* use bit0 to reset all 3 sdh controls */
	reg_tmp = __raw_readl(apmu_base + APMU_SDH0);
	__raw_writel(reg_tmp & (~1), apmu_base + APMU_SDH0);
	udelay(10);
	__raw_writel(reg_tmp | (1), apmu_base + APMU_SDH0);
}

static void __init pxa1U88_dt_irq_init(void)
{
	irqchip_init();
	/* only for wake up */
	mmp_of_wakeup_init();
}

static __init void pxa1U88_timer_init(void)
{
	void __iomem *chip_id;

	regs_addr_iomap();

	/* this is early, initialize mmp_chip_id here */
	chip_id = regs_addr_get_va(REGS_ADDR_CIU);
	mmp_chip_id = readl_relaxed(chip_id);

	enable_pxawdt_clock();

#ifdef CONFIG_ARM_ARCH_TIMER
	enable_arch_timer();
#endif

	enable_soc_timer();

	of_clk_init(NULL);

	clocksource_of_init();

	pxa988_sdhc_reset_all();

}

/* For HELANLTE CP memeory reservation, 32MB by default */
static u32 cp_area_size = 0x02000000;
static u32 cp_area_addr = 0x06000000;
static int __init early_cpmem(char *p)
{
	char *endp;

	cp_area_size = memparse(p, &endp);
	if (*endp == '@')
		cp_area_addr = memparse(endp + 1, NULL);

	return 0;
}
early_param("cpmem", early_cpmem);

static void pxa_reserve_cp_memblock(void)
{
	/* Reserve memory for CP */
	BUG_ON(memblock_reserve(cp_area_addr, cp_area_size) != 0);
	memblock_free(cp_area_addr, cp_area_size);
	memblock_remove(cp_area_addr, cp_area_size);
	pr_info("Reserved CP memory: 0x%x@0x%x\n", cp_area_size, cp_area_addr);
}

static void __init pxa1U88_init_machine(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			     mmpx_auxdata_lookup, &platform_bus);
}

static void pxa_reserve_obmmem(void)
{
	/* Reserve 1MB memory for obm */
	BUG_ON(memblock_reserve(PLAT_PHYS_OFFSET, 0x100000) != 0);
	memblock_free(PLAT_PHYS_OFFSET, 0x100000);
	memblock_remove(PLAT_PHYS_OFFSET, 0x100000);
	pr_info("Reserved OBM memory: 0x%x@0x%lx\n", 0x100000, PLAT_PHYS_OFFSET);
}

static void __init pxa1x88_reserve(void)
{
	pxa_reserve_obmmem();

	pxa_reserve_cp_memblock();
}

static const char *pxa1U88_dt_board_compat[] __initdata = {
	"marvell,pxa1U88",
	NULL,
};

DT_MACHINE_START(PXA1U88_DT, "PXA1U88")
	.smp_init	= smp_init_ops(mmp_smp_init_ops),
	.init_irq	= pxa1U88_dt_irq_init,
	.init_time      = pxa1U88_timer_init,
	.init_machine	= pxa1U88_init_machine,
	.dt_compat      = pxa1U88_dt_board_compat,
	.reserve	= pxa1x88_reserve,
	.restart	= mmp_arch_restart,
MACHINE_END

static const char *pxa1L88_dt_board_compat[] __initdata = {
	"marvell,pxa1L88",
	NULL,
};

DT_MACHINE_START(PXA1L88_DT, "PXA1L88")
	.smp_init	= smp_init_ops(mmp_smp_init_ops),
	.init_time      = pxa1U88_timer_init,
	.init_machine	= pxa1U88_init_machine,
	.dt_compat      = pxa1L88_dt_board_compat,
	.reserve	= pxa1x88_reserve,
	.restart	= mmp_arch_restart,
MACHINE_END
