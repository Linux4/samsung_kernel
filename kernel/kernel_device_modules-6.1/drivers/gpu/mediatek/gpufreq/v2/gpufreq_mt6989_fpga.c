// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_mt6989_fpga.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/timekeeping.h>

#include <gpufreq_v2.h>
#include <gpufreq_mssv.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6989.h>
#include <gpufreq_reg_mt6989.h>
#include <mtk_gpu_utility.h>

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
/* power control function */
static void __gpufreq_footprint_mfg_timeout(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name, unsigned int footprint);
static void __gpufreq_mfg1_bus_prot(enum gpufreq_power_state power);
static void __gpufreq_mfgx_rpc_ctrl(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name);
static void __gpufreq_mfg0_bus_prot(enum gpufreq_power_state power);
static void __gpufreq_mfg0_spm_shutdown_ctrl(enum gpufreq_power_state power);
static void __gpufreq_mtcmos_ctrl(enum gpufreq_power_state power);
static void __gpufreq_buck_iso_config(enum gpufreq_power_state power);
/* init function */
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __gpufreq_pdrv_probe,
	.remove = __gpufreq_pdrv_remove,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_ifrbus_ao_base;
static void __iomem *g_gpueb_sram_base;
static void __iomem *g_gpueb_cfgreg_base;
static void __iomem *g_mali_base;
static struct gpufreq_status g_gpu;
static struct gpufreq_status g_stack;
static unsigned int g_gpueb_support;
static unsigned int g_shader_present;

static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_ap_fp = {};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get power state (on/off) */
unsigned int __gpufreq_get_power_state(void)
{
	if (g_gpu.power_count > 0)
		return GPU_PWR_ON;
	else
		return GPU_PWR_OFF;
}

/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return -1;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return 0;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return 0;
}

/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return -1;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);
	return -1;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return -1;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);
	return -1;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);
	return -1;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);
	return -1;
}

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{
	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("MFG0=0x%08x, MFG1=0x%08x, MFG37=0x%08x, MFG2=0x%08x, MFG3=0x%08x",
		DRV_Reg32(SPM_MFG0_PWR_CON), DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG37_PWR_CON), DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG3_PWR_CON));
	GPUFREQ_LOGI("MFG9=0x%08x, MFG_RPC_SLP_PROT_EN_STA=0x%08x",
		DRV_Reg32(MFG_RPC_MFG9_PWR_CON), DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA));
	GPUFREQ_LOGI("IFR_EMISYS_PROTECT_EN_STA_0=0x%08x, (0x1002C120)=0x%08x",
		DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0), DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1));
	GPUFREQ_LOGI("SPM_SOC_BUCK_ISO_CON=0x%08x", DRV_Reg32(SPM_SOC_BUCK_ISO_CON));
}

int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx_gpu);
	GPUFREQ_UNREFERENCED(target_oppidx_stack);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	int ret = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGI("+ PWR_STATUS: 0x%08lx", MFG_0_22_37_PWR_STATUS & MFG_0_1_2_3_9_37_PWR_MASK);
	GPUFREQ_LOGI("switch power: %s (Power: %d, Active: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off", g_gpu.power_count,
		g_gpu.active_count, g_gpu.buck_count,
		g_gpu.mtcmos_count, g_gpu.cg_count);

	if (power == GPU_PWR_ON)
		g_gpu.power_count++;
	else
		g_gpu.power_count--;
	__gpufreq_footprint_power_count(g_gpu.power_count);

	if (power == GPU_PWR_ON && g_gpu.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* clear Buck ISO after Buck on */
		__gpufreq_buck_iso_config(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x02);

		/* enable MTCMOS */
		__gpufreq_mtcmos_ctrl(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x03);
	} else if (power == GPU_PWR_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(0x04);

		/* disable MTCMOS */
		__gpufreq_mtcmos_ctrl(GPU_PWR_OFF);
		__gpufreq_footprint_power_step(0x05);

		/* set Buck ISO before Buck off */
		__gpufreq_buck_iso_config(GPU_PWR_OFF);
		__gpufreq_footprint_power_step(0x06);
	}

	/* return power count if successfully control power */
	ret = g_gpu.power_count;

	if (power == GPU_PWR_ON)
		__gpufreq_footprint_power_step(0x07);
	else if (power == GPU_PWR_OFF)
		__gpufreq_footprint_power_step(0x08);

	GPUFREQ_LOGI("- PWR_STATUS: 0x%08lx", MFG_0_22_37_PWR_STATUS & MFG_0_1_2_3_9_37_PWR_MASK);

	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static void __gpufreq_buck_iso_config(enum gpufreq_power_state power)
{
	if (power == GPU_PWR_ON) {
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [8] VGPU_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(8));
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [16] VSTACK0_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(16));
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [28] VSTACK1_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(28));
	} else {
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [28] VSTACK1_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(28));
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [16] VSTACK0_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(16));
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [8] VGPU_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(8));
	}

	GPUFREQ_LOGI("power: %d, SPM_SOC_BUCK_ISO_CON: 0x%08x",
		power, DRV_Reg32(SPM_SOC_BUCK_ISO_CON));
}

static void __gpufreq_footprint_mfg_timeout(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name, unsigned int footprint)
{
	__gpufreq_footprint_power_step(footprint);

	GPUFREQ_LOGE("power: %d, footprint: 0x%02x, (%s)=0x%08x",
		power, footprint, pwr_con_name, DRV_Reg32(pwr_con));
	GPUFREQ_LOGE("(0x1C001F04)=0x%08x, (0x13F91070)=0x%08x, (0x13F9162C)=0x%08x",
		DRV_Reg32(SPM_MFG0_PWR_CON), DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG37_PWR_CON));
	GPUFREQ_LOGE("(0x13F910A0)=0x%08x, (0x13F910A4)=0x%08x, (0x13F910BC)=0x%08x",
		DRV_Reg32(MFG_RPC_MFG2_PWR_CON), DRV_Reg32(MFG_RPC_MFG3_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG9_PWR_CON));
	GPUFREQ_LOGE("(0x13F91048)=0x%08x, (0x1002C100)=0x%08x, (0x1002C120)=0x%08x",
		DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA), DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0),
		DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1));

	__gpufreq_abort("timeout");
}

static void __gpufreq_mfg1_bus_prot(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* EMI */
		/* IFR_EMISYS_PROTECT_EN_W1C_0 0x1002C108 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1C_0, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xC0);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xC1);
		} while (DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_0) & GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xC2);
		/* IFR_EMISYS_PROTECT_EN_W1C_1 0x1002C128 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1C_1, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xC3);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xC4);
		} while (DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_1) & GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xC5);

		/* Rx */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [19:16] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xC6);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xC7);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xC8);

		/* Tx */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [3:0] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xC9);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xCA);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xCB);

		/* GPU to EMI ACP Tx Rx */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [6] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [22] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, (BIT(6) | BIT(22)));
		__gpufreq_footprint_power_step(0xCC);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xCD);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & (BIT(6) | BIT(22)));
		__gpufreq_footprint_power_step(0xCE);

		/* GPUEB to EMI ACP DVM */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [23] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, BIT(23));
		__gpufreq_footprint_power_step(0xCF);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xD0);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & BIT(23));
		__gpufreq_footprint_power_step(0xD1);

		/* GPU to MCU ACP Tx Rx */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [5] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [21] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, (BIT(5) | BIT(21)));
		__gpufreq_footprint_power_step(0xD2);

		/* GPUEB to MCU ACP */
		/* MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT 0x13F90554 [0] = 1'b0 */
		DRV_WriteReg32(MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT,
			(DRV_Reg32(MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT) & ~BIT(0)));
		__gpufreq_footprint_power_step(0xD3);
	} else {
		/* GPUEB to MCU ACP */
		/* MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT 0x13F90554 [0] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT,
			(DRV_Reg32(MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT) | BIT(0)));
		__gpufreq_footprint_power_step(0xD4);

		/* GPU to MCU ACP Tx Rx */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [5] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [21] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, (BIT(5) | BIT(21)));
		__gpufreq_footprint_power_step(0xD5);

		/* GPUEB to EMI ACP DVM */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [23] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, BIT(23));
		__gpufreq_footprint_power_step(0xD6);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xD7);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & BIT(23)) != BIT(23));
		__gpufreq_footprint_power_step(0xD8);

		/* GPU to EMI ACP Tx Rx */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [6] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [22] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, (BIT(6) | BIT(22)));
		__gpufreq_footprint_power_step(0xD9);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xDA);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & (BIT(6) | BIT(22))) !=
			(BIT(6) | BIT(22)));
		__gpufreq_footprint_power_step(0xDB);

		/* Tx */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [3:0] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xDC);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xDD);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(3, 0)) != GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xDE);

		/* Rx */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [19:16] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xDF);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xE0);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16)) != GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xE1);

		/* EMI */
		/* IFR_EMISYS_PROTECT_EN_W1S_0 0x1002C104 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1S_0, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xE2);
		/* IFR_EMISYS_PROTECT_EN_W1S_1 0x1002C124 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1S_1, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xE3);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xE4);
		} while ((DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_0) & GENMASK(20, 19)) !=
			GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xE5);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG_RPC_MFG1_PWR_CON", 0xE6);
		} while ((DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_1) & GENMASK(20, 19)) !=
			GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xE7);
	}
}

static void __gpufreq_mfgx_rpc_ctrl(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* PWR_ON = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(2)));
		__gpufreq_footprint_power_step(0xA0);
		/* PWR_ACK = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 10) {
				__gpufreq_footprint_power_step(0xA1);
				break;
			}
		} while ((DRV_Reg32(pwr_con) & BIT(30)) != BIT(30));
		/* PWR_ON_2ND = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(3)));
		__gpufreq_footprint_power_step(0xA2);
		/* PWR_ACK_2ND = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 10) {
				__gpufreq_footprint_power_step(0xA3);
				break;
			}
		} while ((DRV_Reg32(pwr_con) & BIT(31)) != BIT(31));
		/* PWR_ACK = 1'b1 */
		/* PWR_ACK_2ND = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, pwr_con, pwr_con_name, 0xA4);
		} while ((DRV_Reg32(pwr_con) & GENMASK(31, 30)) != GENMASK(31, 30));
		/* PWR_CLK_DIS = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(4)));
		__gpufreq_footprint_power_step(0xA5);
		/* PWR_ISO = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(1)));
		__gpufreq_footprint_power_step(0xA6);
		/* PWR_RST_B = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(0)));
		__gpufreq_footprint_power_step(0xA7);
		/* PWR_SRAM_PDN = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(8)));
		__gpufreq_footprint_power_step(0xA8);
		/* PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, pwr_con, pwr_con_name, 0xA9);
		} while (DRV_Reg32(pwr_con) & BIT(12));
		__gpufreq_footprint_power_step(0xAA);

		/* release bus protect */
		if (pwr_con == MFG_RPC_MFG1_PWR_CON)
			__gpufreq_mfg1_bus_prot(GPU_PWR_ON);
	} else {
		/* set bus protect */
		if (pwr_con == MFG_RPC_MFG1_PWR_CON)
			__gpufreq_mfg1_bus_prot(GPU_PWR_OFF);

		/* PWR_SRAM_PDN = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(8)));
		__gpufreq_footprint_power_step(0xAB);
		/* PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, pwr_con, pwr_con_name, 0xAC);
		} while ((DRV_Reg32(pwr_con) & BIT(12)) != BIT(12));
		__gpufreq_footprint_power_step(0xAD);
		/* PWR_ISO = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(1)));
		__gpufreq_footprint_power_step(0xAE);
		/* PWR_CLK_DIS = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(4)));
		__gpufreq_footprint_power_step(0xAF);
		/* PWR_RST_B = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(0)));
		__gpufreq_footprint_power_step(0xB0);
		/* PWR_ON = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(2)));
		__gpufreq_footprint_power_step(0xB1);
		/* PWR_ON_2ND = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(3)));
		__gpufreq_footprint_power_step(0xB2);
		/* PWR_ACK = 1'b0 */
		/* PWR_ACK_2ND = 1'b0 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, pwr_con, pwr_con_name, 0xB3);
		} while (DRV_Reg32(pwr_con) & GENMASK(31, 30));
		__gpufreq_footprint_power_step(0xB4);
	}
}

static void __gpufreq_mfg0_bus_prot(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* IFR_INFRASYS_PROTECT_EN_W1C_0 0x1002C008 [17] = 1'b1 */
		DRV_WriteReg32(IFR_INFRASYS_PROTECT_EN_W1C_0, BIT(17));
		__gpufreq_footprint_power_step(0xC0);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xC1);
		} while (DRV_Reg32(IFR_INFRASYS_PROTECT_RDY_STA_0) & BIT(17));
		__gpufreq_footprint_power_step(0xC2);

		/* IFR_MFGSYS_PROTECT_EN_W1C_0 0x1002C1A8 [4] = 1'b1 */
		DRV_WriteReg32(IFR_MFGSYS_PROTECT_EN_W1C_0, BIT(4));
		__gpufreq_footprint_power_step(0xC3);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xC4);
		} while (DRV_Reg32(IFR_MFGSYS_PROTECT_RDY_STA_0) & BIT(4));
		__gpufreq_footprint_power_step(0xC5);
	} else {
		/* IFR_MFGSYS_PROTECT_EN_W1S_0 0x1002C1A4 [4] = 1'b1 */
		DRV_WriteReg32(IFR_MFGSYS_PROTECT_EN_W1S_0, BIT(4));
		__gpufreq_footprint_power_step(0xC6);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xC7);
		} while ((DRV_Reg32(IFR_MFGSYS_PROTECT_RDY_STA_0) & BIT(4)) != BIT(4));
		__gpufreq_footprint_power_step(0xC8);

		/* IFR_INFRASYS_PROTECT_EN_W1S_0 0x1002C004 [4] = 1'b1 */
		DRV_WriteReg32(IFR_INFRASYS_PROTECT_EN_W1S_0, BIT(17));
		__gpufreq_footprint_power_step(0xC9);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xCA);
		} while ((DRV_Reg32(IFR_INFRASYS_PROTECT_RDY_STA_0) & BIT(17)) != BIT(17));
		__gpufreq_footprint_power_step(0xCB);
	}
}

static void __gpufreq_mfg0_spm_shutdown_ctrl(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* PWR_ON = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(2)));
		__gpufreq_footprint_power_step(0xA0);
		/* PWR_ACK = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 10) {
				__gpufreq_footprint_power_step(0xA1);
				break;
			}
		} while ((DRV_Reg32(SPM_MFG0_PWR_CON) & BIT(30)) != BIT(30));
		/* PWR_ON_2ND = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(3)));
		__gpufreq_footprint_power_step(0xA2);
		/* PWR_ACK_2ND = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 10) {
				__gpufreq_footprint_power_step(0xA3);
				break;
			}
		} while ((DRV_Reg32(SPM_MFG0_PWR_CON) & BIT(31)) != BIT(31));
		/* PWR_ACK = 1'b1 */
		/* PWR_ACK_2ND = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xA4);
		} while ((DRV_Reg32(SPM_MFG0_PWR_CON) & GENMASK(31, 30)) != GENMASK(31, 30));
		/* PWR_CLK_DIS = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(4)));
		__gpufreq_footprint_power_step(0xA5);
		/* PWR_ISO = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(1)));
		__gpufreq_footprint_power_step(0xA6);
		/* PWR_RST_B = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(0)));
		__gpufreq_footprint_power_step(0xA7);
		/* PWR_SRAM_PDN = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(8)));
		__gpufreq_footprint_power_step(0xA8);
		/* PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xA9);
		} while (DRV_Reg32(SPM_MFG0_PWR_CON) & BIT(12));
		__gpufreq_footprint_power_step(0xAA);
		/* PWR_SRAM_ISOINT_B = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(6)));
		__gpufreq_footprint_power_step(0xAB);
		/* PWR_SRAM_CKISO = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(5)));
		__gpufreq_footprint_power_step(0xAC);

		/* release bus protect */
		__gpufreq_mfg0_bus_prot(GPU_PWR_ON);
	} else {
		/* set bus protect */
		__gpufreq_mfg0_bus_prot(GPU_PWR_OFF);

		/* PWR_SRAM_CKISO = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(5)));
		__gpufreq_footprint_power_step(0xAD);
		/* PWR_SRAM_ISOINT_B = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(6)));
		__gpufreq_footprint_power_step(0xAE);
		/* PWR_SRAM_PDN = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(8)));
		__gpufreq_footprint_power_step(0xAF);
		/* PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xB0);
		} while ((DRV_Reg32(SPM_MFG0_PWR_CON) & BIT(12)) != BIT(12));
		__gpufreq_footprint_power_step(0xB1);
		/* PWR_ISO = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(1)));
		__gpufreq_footprint_power_step(0xB2);
		/* PWR_CLK_DIS = 1'b1 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) | BIT(4)));
		__gpufreq_footprint_power_step(0xB3);
		/* PWR_RST_B = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(0)));
		__gpufreq_footprint_power_step(0xB4);
		/* PWR_ON = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(2)));
		__gpufreq_footprint_power_step(0xB5);
		/* PWR_ON_2ND = 1'b0 */
		DRV_WriteReg32(SPM_MFG0_PWR_CON, (DRV_Reg32(SPM_MFG0_PWR_CON) & ~BIT(3)));
		__gpufreq_footprint_power_step(0xB6);
		/* PWR_ACK = 1'b0 */
		/* PWR_ACK_2ND = 1'b0 */
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, SPM_MFG0_PWR_CON,
					"SPM_MFG0_PWR_CON", 0xB7);
		} while (DRV_Reg32(SPM_MFG0_PWR_CON) & GENMASK(31, 30));
		__gpufreq_footprint_power_step(0xB8);
	}
}

static void __gpufreq_mtcmos_ctrl(enum gpufreq_power_state power)
{
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		__gpufreq_mfg0_spm_shutdown_ctrl(GPU_PWR_ON);
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG1_PWR_CON, "MFG_RPC_MFG1_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG37_PWR_CON, "MFG_RPC_MFG37_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG2_PWR_CON, "MFG_RPC_MFG2_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG3_PWR_CON, "MFG_RPC_MFG3_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG9_PWR_CON, "MFG_RPC_MFG9_PWR_CON");

		val = MFG_0_22_37_PWR_STATUS & MFG_0_1_2_3_9_37_PWR_MASK;
		if (unlikely(val != MFG_0_1_2_3_9_37_PWR_MASK))
			GPUFREQ_LOGE("incorrect MFG0/1/2/3/9/37 power on status: 0x%08x", val);

		g_gpu.mtcmos_count++;
		g_stack.mtcmos_count++;
	} else {
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG9_PWR_CON, "MFG_RPC_MFG9_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG3_PWR_CON, "MFG_RPC_MFG3_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG2_PWR_CON, "MFG_RPC_MFG2_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG37_PWR_CON,
			"MFG_RPC_MFG37_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG1_PWR_CON, "MFG_RPC_MFG1_PWR_CON");
		__gpufreq_mfg0_spm_shutdown_ctrl(GPU_PWR_OFF);

		val = MFG_0_22_37_PWR_STATUS & MFG_1_2_3_9_37_PWR_MASK;
		if (unlikely(val))
			GPUFREQ_LOGE("incorrect MFG1/2/3/9/37 power off status: 0x%08x", val);

		g_stack.mtcmos_count--;
		g_gpu.mtcmos_count--;
	}

	GPUFREQ_TRACE_END();
}

static void __gpufreq_init_shader_present(void)
{
	g_shader_present = GPU_SHADER_PRESENT_1;
}

static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ifrbus_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource IFRBUS_AO");
		goto done;
	}
	g_ifrbus_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_ifrbus_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap IFRBUS_AO: 0x%llx", res->start);
		goto done;
	}

	/* 0x13C00000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_sram");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource GPUEB_SRAM");
		goto done;
	}
	g_gpueb_sram_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_sram_base)) {
		GPUFREQ_LOGE("fail to ioremap GPUEB_SRAM: 0x%llx", res->start);
		goto done;
	}

	/* 0x13C60000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_cfgreg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource GPUEB_CFGREG");
		goto done;
	}
	g_gpueb_cfgreg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_cfgreg_base)) {
		GPUFREQ_LOGE("fail to ioremap GPUEB_CFGREG: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;
	int i = 0;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* init shader present */
	__gpufreq_init_shader_present();

	if (g_gpueb_support) {
		GPUFREQ_LOGI("gpufreq EB mode");
		/* kick GPUEB reset to trigger GPUEB init and poll ready */
		DRV_WriteReg32(GPUEB_CFGREG_SW_RSTN, 0x3);
		do {
			udelay(1);
			/* 1s timeout */
			if (++i == 1000000) {
				GPUFREQ_LOGE("SPM_MFG0_PWR_CON=0x%08x, MFG_RPC_GPUEB_CFG=0x%08x",
					DRV_Reg32(SPM_MFG0_PWR_CON), DRV_Reg32(MFG_RPC_GPUEB_CFG));
				GPUFREQ_LOGE("GPUEB_FOOTPRINT=0x%08x, GPUFREQ_FOOTPRINT=0x%08x",
					DRV_Reg32(GPUEB_SRAM_GPUEB_INIT_FOOTPRINT),
					DRV_Reg32(GPUEB_SRAM_GPUFREQ_FOOTPRINT_GPR));
				__gpufreq_abort("GPUEB init timeout");
			}
		} while (DRV_Reg32(GPUEB_SRAM_GPUEB_INIT_FOOTPRINT) != 0x55667788);
	} else {
		GPUFREQ_LOGI("gpufreq AP mode");
		/* power on to init first OPP index */
		ret = __gpufreq_power_control(GPU_PWR_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
			goto done;
		}

		/* overwrite power count */
		ret = GPUFREQ_SUCCESS;

		GPUFREQ_LOGI("power control always on");
	}

	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	return GPUFREQ_SUCCESS;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
