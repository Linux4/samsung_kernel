/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *		http://www.samsung.com
 *
 * Chip Abstraction Layer for System power down support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../pwrcal-env.h"
#include "../pwrcal.h"
#include "../pwrcal-pmu.h"
#include "../pwrcal-rae.h"
#include "S5E7570-cmusfr.h"
#include "S5E7570-pmusfr.h"
#include "S5E7570-cmu.h"

enum sys_powerdown {
	SYS_SICD,
	SYS_AFTR,
	SYS_STOP,
	SYS_LPD,
	SYS_LPA,
	SYS_DSTOP,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};

struct exynos_pmu_conf {
	unsigned int *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

/* init_pmu_l2_option */
#define MANUAL_ACINACTM_VALUE		(0x1 << 3)
#define MANUAL_ACINACTM_CONTROL		(0x1 << 2)
#define MANUAL_AINACTS_VALUE		(0x1 << 1)
#define MANUAL_AINACTS_CONTROL		(0x1 << 0)
#define USE_AUTOMATIC_L2FLUSHREQ	(0x1 << 17)
#define USE_STANDBYWFIL2			(0x1 << 16)
#define USE_RETENTION				(0x1 << 4)

/* CPU option */
#define USE_SMPEN					(0x1 << 28)
#define USE_STANDBYWFE				(0x1 << 24)
#define USE_STANDBYWFI				(0x1 << 16)
#define USE_IRQCPU_FOR_PWR			(0x3 << 4)
#define USE_MEMPWRDOWN_FEEDBACK		(0x1 << 3)
#define USE_MEMPWRDOWN_COUNTER		(0x1 << 2)
#define USE_SC_FEEDBACK				(0x1 << 1)
#define USE_SC_COUNTER				(0x1 << 0)
#define DUR_WAIT_RESET				(0xF << 20)
#define DUR_SCALL					(0xF << 4)
#define DUR_SCALL_VALUE				(0x1 << 4)

/* init_pmu_up_scheduler */
#define ENABLE_CPUCL0_CPU			(0x1 << 0)

/* init_set_duration */
#define DUR_STABLE_MASK_AT_RESET	0xFFFFF
#define DUR_STABLE_MASK				0x3FF
#define TCXO_DUR_STABLE							0x140		/* 10ms in 32Khz */
#define EXTREG_SHARED_DUR_STABLE				0x140		/* 10ms in 32Khz */
#define EXTREG_SHARED_DUR_STABLE_AT_RESET		0x3F7A0		/* 10ms in 26Mhz */

/* init_pshold_setting_value*/
#define ENABLE_HW_TRIP				(0x1 << 31)
#define PS_HOLD_OUTPUT_HIGH		(0x3 << 8)

#define PAD_INITIATE_WAKEUP			(0x1 << 28)

struct system_power_backup_reg {
	void *reg;
	unsigned int backup;
	void *reg_stat;
	unsigned int backup_stat;
	void *pwr_stat_reg;	/* if pwr_check true, this reg should be set */
	unsigned int valid;
};

#define SYS_PWR_BACK_REG(_reg, _reg_stat, _pwr_stat_reg)	\
{						\
	.reg		= _reg,			\
	.backup		= 0,			\
	.reg_stat		= _reg_stat,		\
	.backup_stat	= 0,			\
	.pwr_stat_reg	= _pwr_stat_reg,	\
	.valid		= 0,			\
}

static unsigned int *pmu_cpuoption_sfrlist[] = {
	CPUCL0_CPU0_OPTION,
	CPUCL0_CPU1_OPTION,
	CPUCL0_CPU2_OPTION,
	CPUCL0_CPU3_OPTION,
};

static void init_pmu_cpu_option(void)
{
	int cpu;
	unsigned int tmp;

	/* use both sc_counter and sc_feedback */
	/* enable to wait for low SMP-bit at sys power down */
	for (cpu = 0; cpu < sizeof(pmu_cpuoption_sfrlist) / sizeof(pmu_cpuoption_sfrlist[0]); cpu++) {
		tmp = pwrcal_readl(pmu_cpuoption_sfrlist[cpu]);
		tmp |= USE_SC_FEEDBACK;
		tmp |= USE_SMPEN;
		tmp &= ~USE_SC_COUNTER;
		tmp |= USE_STANDBYWFI;
		tmp |= USE_MEMPWRDOWN_FEEDBACK;
		tmp &= ~USE_STANDBYWFE;
		pwrcal_writel(pmu_cpuoption_sfrlist[cpu], tmp);
	}
}

static void set_pmu_enable_reset_lpi_timeout(void)
{
	pwrcal_setbit(RESET_LPI_TIMEOUT, 0, 1);
}

static void init_pmu_apm_option(void)
{
	pwrcal_setbit(CENTRAL_SEQ_APM_OPTION, 1, 1);	/*USE STANDBYWFI*/
	pwrcal_setbit(CORTEXM0_APM_OPTION, 16, 1);	/*USE StandbyWFI*/
}

static void init_pmu_l2_option(void)
{
	unsigned int tmp;

	/* disable automatic L2 flush */
	/* disable L2 retention */

	tmp = pwrcal_readl(CPUCL0_L2_OPTION);
	tmp &= ~(USE_AUTOMATIC_L2FLUSHREQ | USE_RETENTION);
	tmp |= USE_STANDBYWFIL2;
	pwrcal_writel(CPUCL0_L2_OPTION, tmp);
}

static void init_pmu_cpuseq_option(void)
{
}

static void init_pmu_up_scheduler(void)
{
	unsigned int tmp;

	/* limit in-rush current for CPUCL0/1 local power up */
	tmp = pwrcal_readl(UP_SCHEDULER);
	tmp |= ENABLE_CPUCL0_CPU;
	pwrcal_writel(UP_SCHEDULER, tmp);
}

/* init_pmu_feedback */
static unsigned int *pmu_feedback_sfrlist[] = {
	CPUCL0_NONCPU_OPTION,
	TOP_PWR_OPTION,
	TOP_PWR_MIF_OPTION,
	DISPAUD_OPTION,
	MFCMSCL_OPTION,
	ISP_OPTION,
};

static void init_set_duration(void)
{
	unsigned int tmp;

	tmp = pwrcal_readl(TCXO_SHARED_DURATION3);
	tmp &= ~DUR_STABLE_MASK;
	tmp |= TCXO_DUR_STABLE;
	pwrcal_writel(TCXO_SHARED_DURATION3, tmp);

	tmp = pwrcal_readl(EXT_REGULATOR_SHARED_DURATION1);
	tmp &= ~DUR_STABLE_MASK_AT_RESET;
	tmp |= EXTREG_SHARED_DUR_STABLE_AT_RESET;
	pwrcal_writel(EXT_REGULATOR_SHARED_DURATION1, tmp);

	tmp = pwrcal_readl(EXT_REGULATOR_SHARED_DURATION3);
	tmp &= ~DUR_STABLE_MASK;
	tmp |= EXTREG_SHARED_DUR_STABLE;
	pwrcal_writel(EXT_REGULATOR_SHARED_DURATION3, tmp);
}

static void init_pmu_feedback(void)
{
	int i;
	unsigned int tmp;

	for (i = 0; i < sizeof(pmu_feedback_sfrlist) / sizeof(pmu_feedback_sfrlist[0]); i++) {
		tmp = pwrcal_readl(pmu_feedback_sfrlist[i]);
		tmp &= ~USE_SC_COUNTER;
		tmp |= USE_SC_FEEDBACK;
		pwrcal_writel(pmu_feedback_sfrlist[i], tmp);
	}
}
static void init_ps_hold_setting(void)
{
	unsigned int tmp;

	tmp = pwrcal_readl(PS_HOLD_CONTROL);
	tmp |= (ENABLE_HW_TRIP | PS_HOLD_OUTPUT_HIGH);
	pwrcal_writel(PS_HOLD_CONTROL, tmp);
}


static void enable_armidleclockdown(void);
static void syspwr_init(void)
{
	init_pmu_feedback();
	init_pmu_l2_option();
	init_pmu_cpu_option();
	init_pmu_cpuseq_option();
	init_pmu_up_scheduler();
	init_ps_hold_setting();
	init_set_duration();
	init_pmu_apm_option();
	set_pmu_enable_reset_lpi_timeout();
	enable_armidleclockdown();
}

static struct exynos_pmu_conf exynos_syspwr_pmu_config[] = {
	/* { .addr = address, .val = { SICD, AFTR, STOP, LPD, LPA, DSTOP, SLEEP } } */
	{ CPUCL0_CPU0_SYS_PWR_REG,			{ 0xF, 0x0, 0xF, 0x0, 0x0, 0x0, 0x8} },
	{ DIS_IRQ_CPUCL0_CPU0_LOCAL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU0_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU0_CPUSEQUENCER_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CPUCL0_CPU1_SYS_PWR_REG,			{ 0xF, 0x0, 0xF, 0x0, 0x0, 0x0, 0x8} },
	{ DIS_IRQ_CPUCL0_CPU1_LOCAL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU1_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU1_CPUSEQUENCER_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CPUCL0_CPU2_SYS_PWR_REG,			{ 0xF, 0x0, 0xF, 0x0, 0x0, 0x0, 0x8} },
	{ DIS_IRQ_CPUCL0_CPU2_LOCAL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU2_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU2_CPUSEQUENCER_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CPUCL0_CPU3_SYS_PWR_REG,			{ 0xF, 0x0, 0xF, 0x0, 0x0, 0x0, 0x8} },
	{ DIS_IRQ_CPUCL0_CPU3_LOCAL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU3_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DIS_IRQ_CPUCL0_CPU3_CPUSEQUENCER_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CPUCL1_CPU0_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF} },
	{ DIS_IRQ_CPUCL1_CPU0_LOCAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU0_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU0_CPUSEQUENCER_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ CPUCL1_CPU1_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF} },
	{ DIS_IRQ_CPUCL1_CPU1_LOCAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU1_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU1_CPUSEQUENCER_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ CPUCL1_CPU2_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF} },
	{ DIS_IRQ_CPUCL1_CPU2_LOCAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU2_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU2_CPUSEQUENCER_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ CPUCL1_CPU3_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF} },
	{ DIS_IRQ_CPUCL1_CPU3_LOCAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU3_CENTRAL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DIS_IRQ_CPUCL1_CPU3_CPUSEQUENCER_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ CPUCL0_NONCPU_SYS_PWR_REG,			{ 0xF, 0x0, 0xF, 0x0, 0x0, 0x0, 0x8} },
	{ CPUCL1_NONCPU_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF} },
	{ CPUCL0_L2_SYS_PWR_REG,			{ 0x7, 0x0, 0x7, 0x0, 0x0, 0x0, 0x7} },
	{ CPUCL1_L2_SYS_PWR_REG,			{ 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7} },
	{ CLKSTOP_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ CLKRUN_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ RETENTION_CMU_TOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x3} },
	{ RESET_CMU_TOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x0} },
	{ RESET_CPUCLKSTOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x0} },
	{ CLKSTOP_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ CLKRUN_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ RETENTION_CMU_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x0, 0x0, 0x3} },
	{ RESET_CMU_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x0} },
	{ DDRPHY_CLKSTOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x1} },
	{ DDRPHY_ISO_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x1} },
	{ DDRPHY_DLL_CLK_SYS_PWR_REG,			{ 0x0, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_CMU_TOP_SYS_PWR_REG,			{ 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_AUD_PLL_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ DISABLE_PLL_CMU_MIF_SYS_PWR_REG,			{ 0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_APM_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_AHEAD_CP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ RESET_AHEAD_GNSS_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ TOP_BUS_SYS_PWR_REG,			{ 0x7, 0x7, 0x7, 0x0, 0x0, 0x0, 0x0} },
	{ TOP_RETENTION_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x3} },
	{ TOP_PWR_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x3} },
	{ TOP_BUS_MIF_SYS_PWR_REG,			{ 0x0, 0x7, 0x7, 0x7, 0x0, 0x0, 0x0} },
	{ TOP_RETENTION_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x0, 0x0, 0x3} },
	{ TOP_PWR_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x0, 0x0, 0x3} },
	{ LOGIC_RESET_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ OSCCLK_GATE_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x1} },
	{ SLEEP_RESET_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x0} },
	{ LOGIC_RESET_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x0, 0x0, 0x0} },
	{ OSCCLK_GATE_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x1, 0x0, 0x0, 0x1} },
	{ SLEEP_RESET_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x0} },
	{ RESET_ASB_MIF_GNSS_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ MEMORY_TOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x3} },
	{ TCXO_GATE_GNSS_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ RESET_ASB_GNSS_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ CLEANY_BUS_CP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ LOGIC_RESET_CP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ TCXO_GATE_CP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ RESET_ASB_CP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ RESET_ASB_MIF_CP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ MEMORY_MIF_ALIVEIRAM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ MEMORY_MIF_TOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x0, 0x0, 0x3} },
	{ CLEANY_BUS_GNSS_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ LOGIC_RESET_GNSS_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ PAD_RETENTION_LPDDR3_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_AUD_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_PEDOMETER_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ PAD_RETENTION_MMC2_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_UART_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_MMC0_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_MMC1_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_SPI_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_RETENTION_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x1} },
	{ PAD_RETENTION_BOOTLDO_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ PAD_ISOLATION_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x1} },
	{ EXT_REGULATOR_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0} },
	{ GPIO_MODE_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ GPIO_MODE_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ GPIO_MODE_DISPAUD_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0} },
	{ G3D_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0x0, 0x0, 0x0, 0x0} },
	{ DISPAUD_SYS_PWR_REG,			{ 0xF, 0xF, 0x0, 0xF, 0xF, 0x0, 0x0} },
	{ ISP_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0x0, 0x0, 0x0, 0x0} },
	{ MFCMSCL_SYS_PWR_REG,			{ 0xF, 0xF, 0xF, 0x0, 0x0, 0x0, 0x0} },
	{ CLKRUN_CMU_G3D_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CLKRUN_CMU_DISPAUD_SYS_PWR_REG,			{ 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ CLKRUN_CMU_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CLKRUN_CMU_MFCMSCL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CLKSTOP_CMU_G3D_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CLKSTOP_CMU_DISPAUD_SYS_PWR_REG,			{ 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ CLKSTOP_CMU_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ CLKSTOP_CMU_MFCMSCL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_CMU_G3D_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_CMU_DISPAUD_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_CMU_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_CMU_MFCMSCL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_LOGIC_G3D_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_LOGIC_DISPAUD_SYS_PWR_REG,			{ 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_LOGIC_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_LOGIC_MFCMSCL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ MEMORY_G3D_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ MEMORY_DISPAUD_SYS_PWR_REG,			{ 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ MEMORY_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ MEMORY_MFCMSCL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_CMU_G3D_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_CMU_DISPAUD_SYS_PWR_REG,			{ 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_CMU_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_CMU_MFCMSCL_SYS_PWR_REG,			{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0} },

	{ RESET_AHEAD_WIFI_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ CLEANY_BUS_WIFI_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ LOGIC_RESET_WIFI_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ TCXO_GATE_WIFI_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ RESET_ASB_WIFI_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ RESET_ASB_MIF_WIFI_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ CORTEXM0_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ CLKRUN_CMU_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ TOP_BUS_APM_SYS_PWR_REG,			{ 0x7, 0x7, 0x7, 0x0, 0x0, 0x0, 0x0} },
	{ CLKSTOP_CMU_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ CLKSTOP_OPEN_CMU_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ DISABLE_PLL_CMU_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },
	{ TOP_RETENTION_APM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ OSCCLK_GATE_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0} },
	{ MEMORY_APM_TOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ LOGIC_RESET_APM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ SLEEP_RESET_APM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ RETENTION_CMU_APM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ RESET_CMU_APM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3} },
	{ TOP_PWR_APM_SYS_PWR_REG,			{ 0x3, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0} },
	{ TCXO_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x1, 0x1, 0x0, 0x0} },
	{ PAD_RETENTION_PEDOMETER_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },

	{ PAD_RETENTION_PEDOMETER_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1} },
	{ PAD_RETENTION_PEDOMETER_APM_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0} },

	{ 0, },
};

static struct exynos_pmu_conf exynos_syspwr_pmu_option[] = {
	/* { .addr = address, .val =				{ SICD,		AFTR,		STOP,		LPD,			LPA,			DSTOP,		SLEEP		} } */
	{CENTRAL_SEQ_OPTION,				{0x000F0000,	0x000F0000,	0x000F0000,	0x000F0000,	0x000F0000,	0x000F0000,	0x000F0000	} },
	{CENTRAL_SEQ_OPTION1,			{0x10000000,	0x10000000,	0x10000000,	0x10000000,	0x10000000,	0x10000000,	0x10000000	} },
	{CENTRAL_SEQ_MIF_OPTION,			{0x18,		0x18,		0x18,		0x18,		0x18,		0x18,		0x18			} },
	{TOP_BUS_MIF_OPTION,				{0x3,		0x0,			0x0,			0x0,			0x0,			0x0,			0x0			} },
	{0, },
};

static void set_pmu_sys_pwr_reg(enum sys_powerdown mode)
{
	int i;

	for (i = 0; exynos_syspwr_pmu_config[i].reg != 0; i++)
		pwrcal_writel(exynos_syspwr_pmu_config[i].reg, exynos_syspwr_pmu_config[i].val[mode]);

	for (i = 0; exynos_syspwr_pmu_option[i].reg != 0; i++)
		pwrcal_writel(exynos_syspwr_pmu_option[i].reg, exynos_syspwr_pmu_option[i].val[mode]);
}

#define	PWRCAL_CENTRALSEQ_PWR_CFG	0x10000

static void set_pmu_central_seq(int mode, bool enable)
{
	unsigned int tmp;

	/* central sequencer */
	tmp = pwrcal_readl(CENTRAL_SEQ_CONFIGURATION);
	if (enable)
		tmp &= ~PWRCAL_CENTRALSEQ_PWR_CFG;
	else
		tmp |= PWRCAL_CENTRALSEQ_PWR_CFG;
	pwrcal_writel(CENTRAL_SEQ_CONFIGURATION, tmp);

	/* central sequencer MIF */
	if (mode == SYS_SICD) {
		tmp = pwrcal_readl(CENTRAL_SEQ_MIF_CONFIGURATION);
		if (enable)
			tmp &= ~PWRCAL_CENTRALSEQ_PWR_CFG;
		else
			tmp |= PWRCAL_CENTRALSEQ_PWR_CFG;
		pwrcal_writel(CENTRAL_SEQ_MIF_CONFIGURATION, tmp);
	}
}

static struct system_power_backup_reg backup_reg[] = {
};

static int syspwr_clkpwr_backup(unsigned int power_mode)
{
	int i;

	for (i = 0; i < sizeof(backup_reg) / sizeof(backup_reg[0]); i++) {
		backup_reg[i].valid = 0;
		if (backup_reg[i].pwr_stat_reg && pwrcal_getf(backup_reg[i].pwr_stat_reg, 0, 0xF) != 0xF)
			continue;

		if (backup_reg[i].reg) {
			backup_reg[i].backup = pwrcal_readl(backup_reg[i].reg);
			if (backup_reg[i].reg_stat)
				backup_reg[i].backup_stat = pwrcal_readl(backup_reg[i].reg_stat);
			backup_reg[i].valid = 1;
		}
	}

	return 0;
}

static int syspwr_clkpwr_restore(unsigned int power_mode)
{
	int i;
	int timeout;

	for (i = 0; i < sizeof(backup_reg) / sizeof(backup_reg[0]); i++) {
		if (backup_reg[i].valid == 1) {
			if (backup_reg[i].pwr_stat_reg && pwrcal_getf(backup_reg[i].pwr_stat_reg, 0, 0xF) != 0xF)
				continue;
			if (backup_reg[i].reg) {
				pwrcal_writel(backup_reg[i].reg, backup_reg[i].backup);

				if (backup_reg[i].reg_stat) {
					for (timeout = 0; timeout < CLK_WAIT_CNT; timeout++) {
						timeout = pwrcal_readl(backup_reg[i].reg_stat);
						if (timeout == backup_reg[i].backup_stat)
							break;
					}
					if (timeout == CLK_WAIT_CNT)
						pr_warn("[%s] timeout wait for (0x%08X)\n",
							__func__, backup_reg[i].backup_stat);
				}
			}
		}
	}

	return 0;
}

static void enable_armidleclockdown(void)
{
	/*Use L2QACTIVE*/
	pwrcal_setbit(PWR_CTRL3, 0, 1);
	pwrcal_setbit(PWR_CTRL3, 1, 1);
}

inline void disable_armidleclockdown(void)
{
	pwrcal_setbit(PWR_CTRL3, 0, 0);
}

static void syspwr_clock_config(int mode)
{
	pwrcal_setbit(CLK_CON_MUX_CLKPHY_FSYS_USB20DRD_PHYCLOCK_USER,	12,	0);
	pwrcal_setbit(CLK_CON_MUX_CLKPHY_FSYS_USB20DRD_PHYCLOCK_USER,	27,	1);
	if (pwrcal_getf(ISP_STATUS,	0,	0xf) == 0xf) {
		pwrcal_setbit(CLK_CON_MUX_CLKPHY_ISP_S_RXBYTECLKHS0_S4_USER, 12, 0);
		pwrcal_setbit(CLK_CON_MUX_CLKPHY_ISP_S_RXBYTECLKHS0_S4_USER, 27, 1);
	}
	if ((pwrcal_getf(DISPAUD_STATUS, 0, 0xf) == 0xf) && (mode != SYS_LPD)) {
		pwrcal_setbit(CLK_CON_MUX_CLKPHY_DISPAUD_MIPIPHY_TXBYTECLKHS_USER,	12,	0); /* MIPIDPHY */
		pwrcal_setbit(CLK_CON_MUX_CLKPHY_DISPAUD_MIPIPHY_RXCLKESC0_USER,	12,	0); /* MIPIDPHY */
		pwrcal_setbit(CLK_CON_MUX_CLKPHY_DISPAUD_MIPIPHY_TXBYTECLKHS_USER,	27, 1); /* MIPIDPHY */
		pwrcal_setbit(CLK_CON_MUX_CLKPHY_DISPAUD_MIPIPHY_RXCLKESC0_USER,	27, 1); /* MIPIDPHY */
	}

	if (mode == SYS_LPD) {
		/* PLL sharing is need to discuss, CP needs all PLL */
		pwrcal_setbit(MIF_ROOTCLKEN,	0,	1);
		pwrcal_setbit(CLK_ENABLE_CLKCMU_PERI_SPI_REARFROM,	0,	0);
		pwrcal_setbit(CLK_ENABLE_CLKCMU_PERI_SPI_ESE,	0,	0);
		pwrcal_setbit(CLK_ENABLE_CLKCMU_PERI_USI_0,	0,	0);
		pwrcal_setbit(CLK_ENABLE_CLKCMU_PERI_USI_1,	0,	0);
	}
	/*LPI disable conditionally, when CP/GNSS/WIFI is disabled*/
}

static int syspwr_clkpwr_optimize(unsigned int power_mode)
{
	disable_armidleclockdown();

	return 0;
}

static void syspwr_set_additional_config(const enum sys_powerdown eMode)
{
	/* USE_LEVEL_TRIGGER */
	pwrcal_setf(WAKEUP_MASK, 30, 0x1, 0x1);
	pwrcal_setf(WAKEUP_MASK_MIF, 30, 0x1, 0x1);

	if (eMode == SYS_STOP) {
		pwrcal_setf(DISPAUD_OPTION, 31, 0x1, 0x1);
		pwrcal_setf(DISPAUD_OPTION, 21, 0x1, 0x1);
	} else {
		pwrcal_setf(DISPAUD_OPTION, 31, 0x1, 0x0);
		pwrcal_setf(DISPAUD_OPTION, 21, 0x1, 0x0);
	}
}

static void syspwr_prepare(int mode)
{
	syspwr_clkpwr_backup(mode);
	syspwr_clock_config(mode);
	syspwr_clkpwr_optimize(mode);

	set_pmu_sys_pwr_reg(mode);
	syspwr_set_additional_config(mode);
	set_pmu_central_seq(mode, true);
}

static void set_pmu_pad_retention_release(void)
{
	pwrcal_writel(PAD_RETENTION_AUD_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_TOP_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_UART_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_MMC0_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_MMC2_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_SPI_OPTION, PAD_INITIATE_WAKEUP);
	pwrcal_writel(PAD_RETENTION_BOOTLDO_OPTION, PAD_INITIATE_WAKEUP);
}

static void syspwr_post(int mode)
{
	if (mode != SYS_SICD)
		set_pmu_pad_retention_release();

	switch (mode) {
	case SYS_SICD:
	case SYS_AFTR:
	case SYS_DSTOP:
	case SYS_STOP:
	case SYS_LPD:
	case SYS_LPA:
	case SYS_SLEEP:
		set_pmu_central_seq(mode, false);
		break;
	default:
		return;
		break;
	}

	enable_armidleclockdown();

	syspwr_clkpwr_restore(mode);
}

static void syspwr_earlywakeup(int mode)
{
	set_pmu_central_seq(mode, false);

	enable_armidleclockdown();

	syspwr_clkpwr_restore(mode);
}

struct cal_pm_ops cal_pm_ops = {
	.pm_enter = syspwr_prepare,
	.pm_exit = syspwr_post,
	.pm_earlywakeup = syspwr_earlywakeup,
	.pm_init = syspwr_init,
};
