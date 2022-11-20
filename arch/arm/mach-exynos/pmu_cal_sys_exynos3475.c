/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *		http://www.samsung.com
 *
 * Chip Abstraction Layer for System power down support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_CAL_SYS_PWRDOWN
/* for OS */
#include <linux/types.h>
#include <linux/io.h>
#include <mach/pmu_cal_sys.h>
#else
/* for firmware */
#include "util.h"
#include "pmu_cal_sys_exynos3475.h"
struct exynos_pmu_conf {
	unsigned long reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};
#endif
#define REG_INFORM1	(S5P_VA_SYSRAM_NS + 0xC)
#define USE_SC_FEEDBACK		(0x1 << 1)
#define USE_SC_COUNTER		(0x1 << 0)
#define NUM_ALL_CPU		4
#define NUM_ALL_CLUSTER		1

#define	writebits(addr, base, mask, val) \
	__raw_writel((__raw_readl(addr) & ~((mask) << (base))) | \
		(((mask) & (val)) << (base)), addr)

#define readbits(addr, base, mask) \
	((__raw_readl(addr) >> (base)) & (mask))

static struct exynos_pmu_conf exynos3475_pmu_config[] = {
	/* { .addr = address, .val = { SICD, AFTR, LPD, DSTOP } } */
	{ CPU_CPU0_SYS_PWR_REG,				{ 0xF , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU0_LOCAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU0_CENTRAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU0_CPUSEQUENCER_SYS_PWR_REG,	{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CPU_CPU1_SYS_PWR_REG,				{ 0xF , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU1_LOCAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU1_CENTRAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU1_CPUSEQUENCER_SYS_PWR_REG,	{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CPU_CPU2_SYS_PWR_REG,				{ 0xF , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU2_LOCAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU2_CENTRAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU2_CPUSEQUENCER_SYS_PWR_REG,	{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CPU_CPU3_SYS_PWR_REG,				{ 0xF , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU3_LOCAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU3_CENTRAL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DIS_IRQ_CPU_CPU3_CPUSEQUENCER_SYS_PWR_REG,	{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CPU_NONCPU_SYS_PWR_REG,			{ 0xF , 0x0 , 0x0 , 0x0 } },
	{ CPU_L2_SYS_PWR_REG,				{ 0x7 , 0x0 , 0x0 , 0x0 } },
	{ CLKSTOP_CMU_TOP_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ CLKRUN_CMU_TOP_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ RETENTION_CMU_TOP_SYS_PWR_REG,		{ 0x3 , 0x3 , 0x0 , 0x0 } },
	{ RESET_CMU_TOP_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x1 } },
	{ RESET_CPUCLKSTOP_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x1 } },
	{ CLKSTOP_CMU_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ CLKRUN_CMU_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ RETENTION_CMU_MIF_SYS_PWR_REG,		{ 0x3 , 0x3 , 0x3 , 0x0 } },
	{ RESET_CMU_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x1 } },
	{ DDRPHY_CLKSTOP_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ DDRPHY_ISO_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ DDRPHY_DLL_CLK_SYS_PWR_REG,			{ 0x0 , 0x1 , 0x1 , 0x0 } },
	{ DISABLE_PLL_CMU_TOP_SYS_PWR_REG,		{ 0x0 , 0x1 , 0x0 , 0x0 } },
	{ DISABLE_PLL_CMU_MIF_SYS_PWR_REG,		{ 0x0 , 0x1 , 0x1 , 0x0 } },
	{ TOP_BUS_SYS_PWR_REG,				{ 0x7 , 0x7 , 0x0 , 0x0 } },
	{ TOP_RETENTION_SYS_PWR_REG,			{ 0x3 , 0x3 , 0x0 , 0x0 } },
	{ TOP_PWR_SYS_PWR_REG,				{ 0x3 , 0x3 , 0x0 , 0x0 } },
	{ TOP_BUS_MIF_SYS_PWR_REG,			{ 0x0 , 0x7 , 0x7 , 0x0 } },
	{ TOP_RETENTION_MIF_SYS_PWR_REG,		{ 0x3 , 0x3 , 0x3 , 0x0 } },
	{ TOP_PWR_MIF_SYS_PWR_REG,			{ 0x3 , 0x3 , 0x3 , 0x0 } },
	{ LOGIC_RESET_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ OSCCLK_GATE_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ SLEEP_RESET_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x1 } },
	{ LOGIC_RESET_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ OSCCLK_GATE_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ SLEEP_RESET_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x1 } },
	{ MEMORY_TOP_SYS_PWR_REG,			{ 0x3 , 0x3 , 0x0 , 0x0 } },
	{ MEMORY_IMEM_ALIVEIRAM_SYS_PWR_REG,		{ 0x3 , 0x3 , 0x0 , 0x0 } },
	{ RESET_ASB_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x1 } },
	{ PAD_RETENTION_LPDDR3_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ PAD_RETENTION_AUD_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ PAD_RETENTION_JTAG_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_MMC2_SYS_PWR_REG,		{ 0x1 , 0x0 , 0x0 , 0x0 } },
	{ PAD_RETENTION_TOP_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_UART_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_MMC0_SYS_PWR_REG,		{ 0x1 , 0x0 , 0x0 , 0x0 } },
	{ PAD_RETENTION_MMC1_SYS_PWR_REG,		{ 0x1 , 0x0 , 0x0 , 0x0 } },
	{ PAD_RETENTION_EBIA_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_EBIB_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_SPI_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_MIF_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ PAD_RETENTION_USBXTI_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_RETENTION_BOOTLDO_SYS_PWR_REG,		{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ PAD_ALV_SEL_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ TCXO_SYS_PWR_REG,				{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ GPIO_MODE_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x0 , 0x0 } },
	{ GPIO_MODE_MIF_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ GPIO_MODE_AUD_SYS_PWR_REG,			{ 0x1 , 0x1 , 0x1 , 0x0 } },
	{ G3D_SYS_PWR_REG,				{ 0xF , 0xF , 0x0 , 0x0 } },
	{ DISPAUD_SYS_PWR_REG,				{ 0xF , 0xF , 0xF , 0x0 } },
	{ ISP_SYS_PWR_REG,				{ 0xF , 0xF , 0x0 , 0x0 } },
	{ MFCMSCL_SYS_PWR_REG,				{ 0xF , 0xF , 0x0 , 0x0 } },
	{ CLKRUN_CMU_G3D_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKRUN_CMU_DISPAUD_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKRUN_CMU_ISP_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKRUN_CMU_MFCMSCL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKSTOP_CMU_G3D_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKSTOP_CMU_DISPAUD_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKSTOP_CMU_ISP_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ CLKSTOP_CMU_MFCMSCL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DISABLE_PLL_CMU_G3D_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DISABLE_PLL_CMU_DISPAUD_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DISABLE_PLL_CMU_ISP_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ DISABLE_PLL_CMU_MFCMSCL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_LOGIC_G3D_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_LOGIC_DISPAUD_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_LOGIC_ISP_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_LOGIC_MFCMSCL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_CMU_G3D_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_CMU_DISPAUD_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_CMU_ISP_SYS_PWR_REG,			{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ RESET_CMU_MFCMSCL_SYS_PWR_REG,		{ 0x0 , 0x0 , 0x0 , 0x0 } },
	{ NULL, },
};

static struct exynos_pmu_conf exynos3475_pmu_option[] = {
		/* { .addr = address, .val = { SICD, AFTR, LPD, DSTOP } } */
	{ PMU_SYNC_CTRL,		{		 0x1,		 0x1,		 0x1,		 0x1 } },
	{ MEMORY_TOP_OPTION,		{		0x12,		0x12,		0x12,		0x12 } },
	{ MEMORY_IMEM_ALIVEIRAM_OPTION,	{		0x12,		0x12,		0x12,		0x12 } },
	{ PAD_RETENTION_MIF_OPTION,	{		 0x0,		 0x0,	  0x20000000,	  0x20000000 } },
	{ NULL, },
};

static void set_pmu_sys_pwr_reg(enum sys_powerdown mode)
{
	int i;

	for (i = 0; exynos3475_pmu_config[i].reg != NULL; i++)
		__raw_writel(exynos3475_pmu_config[i].val[mode],
				exynos3475_pmu_config[i].reg);

	for (i = 0; exynos3475_pmu_option[i].reg != NULL; i++)
		__raw_writel(exynos3475_pmu_option[i].val[mode],
				exynos3475_pmu_option[i].reg);
}

static void set_pmu_central_seq(bool enable)
{
	unsigned int tmp;

#define	CENTRAL_SEQ_CFG 0x10000
	/* central sequencer */
	tmp = __raw_readl(CENTRAL_SEQ_CONFIGURATION);
	if (enable)
		tmp &= ~CENTRAL_SEQ_CFG;
	else
		tmp |= CENTRAL_SEQ_CFG;
	__raw_writel(tmp, CENTRAL_SEQ_CONFIGURATION);
}

static void set_pmu_central_seq_mif(bool enable)
{
	unsigned int tmp;

#define	CENTRAL_SEQ_CFG_MIF	0x10000
	/* central sequencer MIF */
	tmp = __raw_readl(CENTRAL_SEQ_MIF_CONFIGURATION);
	if (enable)
		tmp &= ~CENTRAL_SEQ_CFG_MIF;
	else
		tmp |= CENTRAL_SEQ_CFG_MIF;
	__raw_writel(tmp, CENTRAL_SEQ_MIF_CONFIGURATION);
}

static void set_pmu_pad_retention_release(void)
{
#define PAD_INITIATE_WAKEUP	(0x1 << 28)
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_AUD_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MMC2_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_TOP_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_UART_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MMC0_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MMC1_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_EBIA_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_EBIB_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_SPI_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MIF_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_BOOTLDO_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_USBXTI_OPTION);
}

static void init_pmu_l2_option(void)
{
	int cluster;
	unsigned int tmp;

#define L2_OPTION(_nr)			(CPU_L2_OPTION + (_nr) * 0x20)
#define USE_STANDBYWFIL2		(0x1 << 16)
#define USE_RETENTION			(0x1 << 4)
	/* disable L2 retention */
	/* eanble STANDBYWFIL2 */
	for (cluster = 0; cluster < NUM_ALL_CLUSTER; cluster++) {
		tmp = __raw_readl(L2_OPTION(cluster));
		tmp &= ~(USE_RETENTION);
		tmp |= USE_STANDBYWFIL2;
		__raw_writel(tmp, L2_OPTION(cluster));
	}
}

static void init_pmu_cpu_option(void)
{
	int cpu;
	unsigned int tmp;

#define CPU_OPTION(_nr)			(CPU_CPU0_OPTION + (_nr) * 0x80)
#define USE_STANDBYWFE			(0x1 << 24)
#define USE_STANDBYWFI			(0x1 << 16)
#define USE_DELAYED_RESET_ASSERTION	(0x1 << 12)
#define USE_IRQCPU_FORPWRUP		(0x1 << 5)
#define USE_IRQCPU_FORPWRDOWN		(0x1 << 4)

	/* use only sc_feedback */
	for (cpu = 0; cpu < NUM_ALL_CPU; cpu++) {
		tmp = __raw_readl(CPU_OPTION(cpu));
		tmp &= ~USE_SC_COUNTER;
		tmp |= USE_SC_FEEDBACK;
		tmp |= USE_DELAYED_RESET_ASSERTION;
		tmp |= USE_IRQCPU_FORPWRUP;
		tmp |= USE_IRQCPU_FORPWRDOWN;
		tmp |= USE_STANDBYWFI;
		tmp &= ~USE_STANDBYWFE;
		__raw_writel(tmp, CPU_OPTION(cpu));
	}

#define CPU_DURATION(_nr)	(CPU_CPU0_DURATION0 + (_nr) * 0x80)
#define DUR_WAIT_RESET	(0xF << 20)
	for (cpu = 0; cpu < NUM_ALL_CPU; cpu++) {
		tmp = __raw_readl(CPU_DURATION(cpu));
		tmp |= DUR_WAIT_RESET;
		__raw_writel(tmp, CPU_DURATION(cpu));
	}
}

static void init_pmu_up_scheduler(void)
{
	unsigned int tmp;

#define ENABLE_CPU_CPU	(0x1 << 0)

	tmp = __raw_readl(UP_SCHEDULER);
	tmp |= ENABLE_CPU_CPU;
	__raw_writel(tmp, UP_SCHEDULER);
}

static void __iomem *feed_list[] = {
	CPU_NONCPU_OPTION,
	TOP_PWR_OPTION,
	TOP_PWR_MIF_OPTION,
	DISPAUD_OPTION,
	ISP_OPTION,
	MFCMSCL_OPTION,
};

static void init_pmu_feedback(void)
{
	int i;
	unsigned int tmp;

#define ARR_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
	for (i = 0; i < ARR_SIZE(feed_list); i++) {
		tmp = __raw_readl(feed_list[i]);
		tmp &= ~USE_SC_COUNTER;
		tmp |= USE_SC_FEEDBACK;
		__raw_writel(tmp, feed_list[i]);
	}
}

static void init_pmu_G3D_option(void)
{
	unsigned int tmp1;
	unsigned int tmp2;

#define DUR_SCPRE_G3D		(0xF << 8)
#define DUR_SCPRE_VALUE_G3D	(0x4 << 8)
#define DUR_SCALL_G3D		(0xF << 4)
#define DUR_SCALL_VALUE_G3D	(0x4 << 4)

	tmp1 = __raw_readl(G3D_OPTION);
	tmp1 &= ~USE_SC_FEEDBACK;
	tmp1 |= USE_SC_COUNTER;
	__raw_writel(tmp1, G3D_OPTION);

	tmp2 = __raw_readl(G3D_DURATION0);
	tmp2 &= ~DUR_SCPRE_G3D;
	tmp2 |= DUR_SCPRE_VALUE_G3D;
	tmp2 &= ~DUR_SCALL_G3D;
	tmp2 |= DUR_SCALL_VALUE_G3D;
	__raw_writel(tmp2, G3D_DURATION0);
}

static void init_pmu_stable_duration(void)
{
#define TCXO_DUR3_VALUE		(0x7F)

	writebits(TCXO_DURATION3, 0, 0xFFFFF, TCXO_DUR3_VALUE);
}
static void init_pmu_island_workaround(void)
{
#define CPU_CPUSEQUENCER_OPTION_VALUE	(0x31000)
#define IRQ_SELECTION_VALUE	(0x1)
#define CPU_SEQ_TRANSITION0_VALUE (0x80920094)
#define CPU_SEQ_TRANSITION1_VALUE (0x80930096)
#define CPU_SEQ_TRANSITION2_VALUE (0x80950093)
#define CPU_SEQ_MIF_TRANSITION0_VALUE (0x80920094)
#define CPU_SEQ_MIF_TRANSITION1_VALUE (0x80930096)
#define CPU_SEQ_MIF_TRANSITION2_VALUE (0x80950093)

	writebits(CPU_CPUSEQUENCER_OPTION, 0, 0xFFFFF, CPU_CPUSEQUENCER_OPTION_VALUE);
	writebits(IRQ_SELECTION, 0, 0xF, IRQ_SELECTION_VALUE);
	writebits(SEQ_TRANSITION0, 0, 0xFFFFFFFF, CPU_SEQ_TRANSITION0_VALUE);
	writebits(SEQ_TRANSITION1, 0, 0xFFFFFFFF, CPU_SEQ_TRANSITION1_VALUE);
	writebits(SEQ_TRANSITION2, 0, 0xFFFFFFFF, CPU_SEQ_TRANSITION2_VALUE);
	writebits(SEQ_MIF_TRANSITION0, 0, 0xFFFFFFFF, CPU_SEQ_MIF_TRANSITION0_VALUE);
	writebits(SEQ_MIF_TRANSITION1, 0, 0xFFFFFFFF, CPU_SEQ_MIF_TRANSITION1_VALUE);
	writebits(SEQ_MIF_TRANSITION2, 0, 0xFFFFFFFF, CPU_SEQ_MIF_TRANSITION2_VALUE);
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_init
 * description: default settings at boot time
 */
void pmu_cal_sys_init(void)
{
	init_pmu_feedback();
	init_pmu_up_scheduler();
	init_pmu_cpu_option();
	init_pmu_l2_option();
	init_pmu_G3D_option();
	init_pmu_stable_duration();
	init_pmu_island_workaround();
	pr_info("EXYNOS3475 PMU CAL Initialize\n");
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_prepare
 * description: settings before going to system power down
 */
void pmu_cal_sys_prepare(enum sys_powerdown mode)
{
	/* CAUTION: sys_pwr_reg doesn't include EXT_REGULATOR_OPTION */
	set_pmu_sys_pwr_reg(mode);
	set_pmu_central_seq(true);
	set_pmu_central_seq_mif(true);
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_post
 * description: settings after normal wakeup
 */
void pmu_cal_sys_post(enum sys_powerdown mode)
{
	set_pmu_pad_retention_release();
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_post
 * description: settings after early wakeup
 */
void pmu_cal_sys_earlywake(enum sys_powerdown mode)
{
	set_pmu_central_seq(false);
}

#ifdef CONFIG_CAL_SYS_PWRDOWN
static const struct pmu_cal_sys_ops sys_ops = {
	.sys_init	= pmu_cal_sys_init,
	.sys_prepare	= pmu_cal_sys_prepare,
	.sys_post	= pmu_cal_sys_post,
	.sys_earlywake	= pmu_cal_sys_earlywake,
};

void register_pmu_cal_sys_ops(const struct pmu_cal_sys_ops **cal)
{
	*cal = &sys_ops;
}
#endif
