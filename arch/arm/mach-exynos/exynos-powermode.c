/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS Power mode
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/device.h>
#include <linux/tick.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/exynos-ss.h>

#include <asm/smp_plat.h>
#include <asm/cputype.h>

#include <mach/pmu.h>
#include <mach/exynos-pm.h>
#include <mach/exynos-powermode.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu_cal_sys.h>

#include <sound/exynos.h>
#include "common.h"

/***************************************************************************
 *                         Functions for low power                         *
 ***************************************************************************/
#ifdef CONFIG_EXYNOS_IDLE_CLOCK_DOWN
static void exynos_idle_clock_down(bool on)
{
	void __iomem *reg_pwr_ctrl1, *reg_pwr_ctrl2;
	unsigned int tmp;

	reg_pwr_ctrl1 = EXYNOS_PWR_CTRL;
	reg_pwr_ctrl2 = EXYNOS_PWR_CTRL2;

	if (on) {
		tmp = __raw_readl(reg_pwr_ctrl1);
		tmp &= ~(EXYNOS_USE_CPU_CLAMPCOREOUT |
			EXYNOS_USE_STANDBYWFIL2_CPU);
		tmp |= (EXYNOS_CPU2_RATIO |
			EXYNOS_CPU1_RATIO |
			EXYNOS_DIVCPU2_DOWN_ENABLE |
			EXYNOS_DIVCPU1_DOWN_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl1);
		tmp = __raw_readl(reg_pwr_ctrl2);
		tmp &= ~(EXYNOS_DUR_STANDBY2 |
			EXYNOS_DUR_STANDBY1);
		tmp |= (EXYNOS_UP_CPU2_RATIO |
			EXYNOS_UP_CPU1_RATIO |
			EXYNOS_DIVCPU2_UP_ENABLE |
			EXYNOS_DIVCPU1_UP_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl2);
	} else {
		tmp = __raw_readl(reg_pwr_ctrl1);
		tmp &= ~(EXYNOS_DIVCPU2_DOWN_ENABLE |
			EXYNOS_DIVCPU1_DOWN_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl1);

		tmp = __raw_readl(reg_pwr_ctrl2);
		tmp &= ~(EXYNOS_DIVCPU2_UP_ENABLE |
			EXYNOS_DIVCPU1_UP_ENABLE);
		__raw_writel(tmp, reg_pwr_ctrl2);
	}

	pr_debug("cpu idle clock down is %s\n", (on ? "enabled" : "disabled"));
};
#else
static void exynos_idle_clock_down(bool on) { };
#endif

/***************************************************************************
 *                        Local power gating(C2)                           *
 ***************************************************************************/
/*
 * If cpu is powered down, set c2_state_mask. Otherwise, clear the mask. To
 * keep coherency of c2_state_mask, use the spinlock, c2_lock. According to
 * CPU local power gating, support subordinate power mode, CPD.
 *
 */
static struct cpumask c2_state_mask;
static void init_c2_state_mask(void)
{
	cpumask_clear(&c2_state_mask);
}

static void update_c2_state(bool down, unsigned int cpu)
{
	if (down)
		cpumask_set_cpu(cpu, &c2_state_mask);
	else
		cpumask_clear_cpu(cpu, &c2_state_mask);
}

static unsigned int cpd_residency = UINT_MAX;

void cpu_local_power_down(unsigned int cpu)
{
	exynos_cpu.power_down_noinit(cpu);
}

void cpu_local_power_up(unsigned int cpu)
{
	exynos_cpu.power_up_noinit(cpu);
}

static DEFINE_SPINLOCK(c2_lock);

int enter_c2(unsigned int cpu, int index)
{
	cpu_local_power_down(cpu);

	spin_lock(&c2_lock);
	update_c2_state(true, cpu);
	spin_unlock(&c2_lock);

	return index;
}

void wakeup_from_c2(unsigned int cpu)
{
	cpu_local_power_up(cpu);

	update_c2_state(false, cpu);
}
/***************************************************************************
 *                              IDLE IP API                                *
 ***************************************************************************/
static int exynos_idle_ip_mask[NUM_SYS_POWERDOWN][NUM_IDLE_IP_INDEX];

static DEFINE_SPINLOCK(idle_ip_index_lock);
static DEFINE_SPINLOCK(idle_ip_clk_lock);
static DEFINE_SPINLOCK(idle_ip_pd_lock);

int exynos_get_idle_ip_index(const char *name)
{
	struct device_node *np = of_find_node_by_name(NULL, "power-mode");
	int index;

	spin_lock(&idle_ip_index_lock);

	index = of_property_match_string(np, "idle-ip0", name);
	if (index >= 0)
		goto found;

	index = of_property_match_string(np, "idle-ip1", name);
	if (index >= 0)
		goto found;

	index = of_property_match_string(np, "idle-ip2", name);
	if (index >= 0)
		goto found;

	index = of_property_match_string(np, "idle-ip3", name);
	if (index >= 0)
		goto found;

	pr_err("[ERROR] Could not get cpuidle idle-ip index\n");
found:
	spin_unlock(&idle_ip_index_lock);

	return index;
}

void exynos_update_pd_idle_status(int index, int idle)
{
	unsigned int val;
	unsigned long flag;

	if (index < 0)
		return;

	spin_lock_irqsave(&idle_ip_pd_lock, flag);

	val = __raw_readl(EXYNOS_PMU_IDLE_IP1);

	if (idle)
		val |= (1 << index);
	else
		val &= ~(1 << index);

	__raw_writel(val, EXYNOS_PMU_IDLE_IP1);

	spin_unlock_irqrestore(&idle_ip_pd_lock, flag);

	return;
}

void exynos_update_ip_idle_status(int index, int idle)
{
	unsigned int val;
	unsigned long flag;

	if (index < 0)
		return;

	spin_lock_irqsave(&idle_ip_clk_lock, flag);

	val = __raw_readl(EXYNOS_PMU_IDLE_IP0);

	if (idle)
		val |= (1 << index);
	else
		val &= ~(1 << index);

	__raw_writel(val, EXYNOS_PMU_IDLE_IP0);

	spin_unlock_irqrestore(&idle_ip_clk_lock, flag);

	return;
}

static void exynos_set_idle_ip_mask(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_LPD:
		__raw_writel(exynos_idle_ip_mask[mode][IDLE_IP0_INDEX], EXYNOS_PMU_IDLE_IP0_MASK);
		__raw_writel(exynos_idle_ip_mask[mode][IDLE_IP1_INDEX], EXYNOS_PMU_IDLE_IP1_MASK);
		break;
	default :
		break;
	}

	return;
}
static int exynos_check_idle_ip_stat(int index)
{
         unsigned int val = __raw_readl(EXYNOS_PMU_IDLE_IP(index));
         unsigned int mask = exynos_idle_ip_mask[SYS_LPD][index];

         return (val & ~mask) == ~mask ? 0 : -EBUSY;
}

/***************************************************************************
 *                              Low power mode                             *
 ***************************************************************************/
/* power domain */
#define PD_STATUS	0xf
#define PD_ENABLE	0xf

static struct sfr_save save_clk_regs[] = {
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_FREECLK_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_PHYCLOCK_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_CLK48MOHCI_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBOTG_OTG20_PHYCLOCK_USER),
};

static struct sfr_bit_ctrl set_clk_regs[] = {
	SFR_CTRL(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_FREECLK_USER,	BIT(12)|BIT(27), BIT(27)),
	SFR_CTRL(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_PHYCLOCK_USER,	BIT(12)|BIT(27), BIT(27)),
	SFR_CTRL(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_CLK48MOHCI_USER,	BIT(12)|BIT(27), BIT(27)),
	SFR_CTRL(EXYNOS3475_CLK_CON_MUX_PHYCLK_USBOTG_OTG20_PHYCLOCK_USER,	BIT(12)|BIT(27), BIT(27)),
};

static struct sfr_save save_dispaud_clk_regs[] = {
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_ACLK_DISPAUD_133_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_VCLK_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD_USER),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK),
	SFR_SAVE(EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD),
	SFR_SAVE(EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_VCLK),
	SFR_SAVE(EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_ECLK),
	SFR_SAVE(EXYNOS3475_CLK_CON_DIV_SCLK_MI2S_AUD),
	SFR_SAVE(EXYNOS3475_CLK_CON_DIV_SCLK_MIXER_AUD),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133_SECURE_CFW),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_SCLK_DECON_INT_VCLK),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_SCLK_DECON_INT_ECLK),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_SCLK_MI2S_AUD),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_SCLK_MIXER_AUD),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_SCLK_AP_IN),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_BT_IN),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_CP_IN),
	SFR_SAVE(EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_FM_IN),
	SFR_SAVE(EXYNOS3475_CLKOUT_CMU_DISPAUD),
	SFR_SAVE(EXYNOS3475_CLKOUT_CMU_DISPAUD_DIV_STAT),
};

static struct sfr_save save_clk_lpd_regs[] = {
       SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4_USER),
       SFR_SAVE(EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4_USER),
};

static struct sfr_bit_ctrl set_clk_lpd_regs[] = {
       SFR_CTRL(EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4_USER, BIT(12)|BIT(27), BIT(27)),
       SFR_CTRL(EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4_USER, BIT(12)|BIT(27), BIT(27)),
};

static void exynos_sys_powerdown_set_clk(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_AFTR:
		break;
	case SYS_LPD:
               /*
                * This condition is only support to 3475 SOC.
                */
#define DISPAUD_INDEX 0x4
               if (!(__raw_readl(EXYNOS_PMU_IDLE_IP1) & DISPAUD_INDEX)) {
			exynos_save_sfr(save_clk_lpd_regs, ARRAY_SIZE(save_clk_lpd_regs));
			exynos_set_sfr(set_clk_lpd_regs, ARRAY_SIZE(set_clk_lpd_regs));
		}
	       if (exynos_check_idle_ip_stat(IDLE_IP0_INDEX)) {
		       exynos_save_sfr(save_clk_regs, ARRAY_SIZE(save_clk_regs));
		       exynos_set_sfr(set_clk_regs, ARRAY_SIZE(set_clk_regs));
	       }
	       break;
	case SYS_DSTOP:
		if ((__raw_readl(EXYNOS_PMU_DISPAUD_STATUS) & PD_STATUS) == PD_ENABLE)
			exynos_save_sfr(save_dispaud_clk_regs, ARRAY_SIZE(save_dispaud_clk_regs));
		/* thru */
	case SYS_CP_CALL:
		exynos_save_sfr(save_clk_regs, ARRAY_SIZE(save_clk_regs));
		exynos_set_sfr(set_clk_regs, ARRAY_SIZE(set_clk_regs));
		break;
	default:
		break;
	}
}

static void exynos_sys_powerdown_restore_clk(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_AFTR:
		break;
	case SYS_LPD:
               /*
                * This condition is only support to 3475 SOC.
                */
               if (!(__raw_readl(EXYNOS_PMU_IDLE_IP1) & DISPAUD_INDEX))
			exynos_restore_sfr(save_clk_lpd_regs, ARRAY_SIZE(save_clk_lpd_regs));

	       if (exynos_check_idle_ip_stat(IDLE_IP0_INDEX))
		       exynos_restore_sfr(save_clk_regs, ARRAY_SIZE(save_clk_regs));
	       break;
	case SYS_DSTOP:
		if ((__raw_readl(EXYNOS_PMU_DISPAUD_STATUS) & PD_STATUS) == PD_ENABLE)
			exynos_restore_sfr(save_dispaud_clk_regs, ARRAY_SIZE(save_dispaud_clk_regs));
		/* thru */
	case SYS_CP_CALL:
		exynos_restore_sfr(save_clk_regs, ARRAY_SIZE(save_clk_regs));
		break;
	default:
		break;
	}
}

void exynos_prepare_sys_powerdown(enum sys_powerdown mode)
{
	unsigned int cpu = smp_processor_id();

	exynos_set_wakeupmask(mode);
	exynos_pmu_cal_sys_prepare(mode == SYS_CP_CALL? SYS_LPD : mode);

	exynos_idle_clock_down(false);

	switch (mode) {
	case SYS_AFTR:
		break;
	case SYS_LPD:
		cpu_local_power_down(cpu);
		exynos_lpd_enter();
		break;
	case SYS_DSTOP:
	case SYS_CP_CALL:
		break;
	default:
		break;
	}

	exynos_sys_powerdown_set_clk(mode);
	exynos_set_idle_ip_mask(mode);
}

void exynos_wakeup_sys_powerdown(enum sys_powerdown mode, bool early_wakeup)
{
	unsigned int cpu = smp_processor_id();

	if (early_wakeup)
		exynos_pmu_cal_sys_earlywake(mode == SYS_CP_CALL? SYS_LPD : mode);
	else
		exynos_pmu_cal_sys_post(mode == SYS_CP_CALL? SYS_LPD: mode);

	exynos_idle_clock_down(true);

	exynos_sys_powerdown_restore_clk(mode);

	switch (mode) {
	case SYS_AFTR:
		break;
	case SYS_LPD:
		if (early_wakeup)
			cpu_local_power_up(cpu);
		else
			exynos_lpd_exit();
		break;
	case SYS_DSTOP:
	case SYS_CP_CALL:
		break;
	default:
		break;
	}
}

bool exynos_sys_powerdown_enabled(void)
{
	/*
	 * When system enters low power mode,
	 * this bit changes automatically to high
	 */
	return !((__raw_readl(EXYNOS_PMU_CENTRAL_SEQ_CONFIGURATION) &
			CENTRALSEQ_PWR_CFG));
}

/*
 * To determine which power mode system enter, check clock or power
 * registers and other devices by notifier.
 */

int determine_lpm(void)
{
	/*
	 * Exynos 3475 is only support SYS_LPD power mode.
	 * But, Another System Power Mode is needed,
	 * condition check will be added.
	 */

	return SYS_LPD;
}

/***************************************************************************
 *                   Initialize exynos-powermode driver                    *
 ***************************************************************************/
static void exynos_lpm_dt_init(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "power-mode");
	struct device_node *child_np = of_find_node_by_path("/power-mode/sys_powermode");
	struct device_node *sys_pwr_node;

	for_each_child_of_node(child_np, sys_pwr_node) {
		int index;
		const __be32 *mask_val;

		if (of_property_read_u32(sys_pwr_node, "mode-index", &index)) {
			pr_info(" * %s missing desc property\n",
					sys_pwr_node->full_name);
			continue;
		};

		mask_val = of_get_property(sys_pwr_node, "idle-ip0-mask", NULL);
		if (mask_val)
			exynos_idle_ip_mask[index][IDLE_IP0_INDEX] = be32_to_cpup(mask_val);

		mask_val = of_get_property(sys_pwr_node, "idle-ip1-mask", NULL);
		if (mask_val)
			exynos_idle_ip_mask[index][IDLE_IP1_INDEX] = be32_to_cpup(mask_val);

		mask_val = of_get_property(sys_pwr_node, "idle-ip2-mask", NULL);
		if (mask_val)
			exynos_idle_ip_mask[index][IDLE_IP2_INDEX] = be32_to_cpup(mask_val);

		mask_val = of_get_property(sys_pwr_node, "idle-ip3-mask", NULL);
		if (mask_val)
			exynos_idle_ip_mask[index][IDLE_IP3_INDEX] = be32_to_cpup(mask_val);
	}

	if (of_property_read_u32(np, "cpd_residency", &cpd_residency))
		pr_warn("No matching property: cpd_residency\n");
}

int __init exynos_powermode_init(void)
{
	exynos_idle_clock_down(true);

	init_c2_state_mask();

	exynos_pmu_cal_sys_init();

	exynos_lpm_dt_init();

	return 0;
}
arch_initcall(exynos_powermode_init);
