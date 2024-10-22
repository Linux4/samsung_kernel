// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    gpufreq_mt6989.c
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
#include <gpufreq_history_common.h>
#include <gpufreq_history_mt6989.h>
#include <gpu_smmu_common.h>
#if IS_ENABLED(CONFIG_THERMAL)
#include <linux/thermal.h>
#endif
/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state);
static void __gpufreq_devapc_vio_handler(void);
static void __gpufreq_set_margin_mode(unsigned int mode);
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int mode);
static void __gpufreq_set_mcu_etm_clock(unsigned int mode);
static void __gpufreq_apply_restore_margin(enum gpufreq_target target, unsigned int mode);
static void __gpufreq_set_temper_compensation(void);
static void __gpufreq_set_gpm3_0_limit(void);
static void __gpufreq_measure_power(void);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static void __gpufreq_update_shared_status_opp_table(void);
static void __gpufreq_update_shared_status_adj_table(void);
static void __gpufreq_update_shared_status_gpm3_table(void);
/* dvfs function */
static void __gpufreq_restore_last_oppidx(void);
static int __gpufreq_generic_scale(
	unsigned int fgpu_old, unsigned int fgpu_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int fstack_old, unsigned int fstack_new,
	unsigned int vstack_old, unsigned int vstack_new,
	unsigned int vsram_gpu, unsigned int vsram_stack);
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_custom_commit_dual(unsigned int target_fgpu, unsigned int target_vgpu,
	unsigned int target_fstack, unsigned int target_vstack, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(unsigned int volt_old, unsigned int volt_new);
static int __gpufreq_volt_scale_stack(unsigned int volt_old, unsigned int volt_new);
static void __gpufreq_volt_check_sram(unsigned int volt_old, unsigned int volt_new);
static void __gpufreq_sw_vmeter_config(enum gpufreq_opp_direct direct,
	unsigned int cur_vgpu, unsigned int cur_vstack);
static void __gpufreq_get_parking_volt(enum gpufreq_opp_direct direct,
	unsigned int cur_vgpu, unsigned int cur_vstack,
	unsigned int *park_vgpu, unsigned int *park_vstack);
static int __gpufreq_volt_scale(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vstack_old, unsigned int vstack_new,
	unsigned int vsram_gpu, unsigned int vsram_stack);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu(enum gpufreq_opp_direct direct, int deltaV);
static unsigned int __gpufreq_settle_time_vstack(enum gpufreq_opp_direct direct, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target);
static unsigned int __gpufreq_get_fmeter_fgpu(void);
static unsigned int __gpufreq_get_fmeter_fstack0(void);
static unsigned int __gpufreq_get_fmeter_fstack1(void);
static unsigned int __gpufreq_get_sub_fgpu(void);
static unsigned int __gpufreq_get_sub_fstack(void);
static unsigned int __gpufreq_get_pll_fgpu(void);
static unsigned int __gpufreq_get_pll_fstack0(void);
static unsigned int __gpufreq_get_pll_fstack1(void);
static unsigned int __gpufreq_get_pmic_vgpu(void);
static unsigned int __gpufreq_get_pmic_vstack(void);
static unsigned int __gpufreq_get_pmic_vsram(void);
static unsigned int __gpufreq_get_dreq_vsram_gpu(void);
static unsigned int __gpufreq_get_dreq_vsram_stack(void);
static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt);
static unsigned int __gpufreq_lkg_formula_rt_gpu(unsigned int v, int t, unsigned int lkg_info);
static unsigned int __gpufreq_lkg_formula_ht_gpu(unsigned int v, int t, unsigned int lkg_info);
static unsigned int __gpufreq_lkg_formula_rt_stack(unsigned int v, int t, unsigned int lkg_info);
static unsigned int __gpufreq_lkg_formula_ht_stack(unsigned int v, int t, unsigned int lkg_info);
#if GPUFREQ_GPM3_0_ENABLE
static unsigned int __gpufreq_get_lkg_istack(unsigned int volt, int temper);
static unsigned int __gpufreq_get_dyn_istack(unsigned int freq, unsigned int volt);
#endif /* GPUFREQ_GPM3_0_ENABLE */
static enum gpufreq_posdiv __gpufreq_get_pll_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_pll_posdiv_stack0(void);
static enum gpufreq_posdiv __gpufreq_get_pll_posdiv_stack1(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq);
/* power control function */
static int __gpufreq_buck_ctrl(enum gpufreq_power_state power);
static int __gpufreq_vstack_ctrl(enum gpufreq_power_state power);
static void __gpufreq_mtcmos_ctrl(enum gpufreq_power_state power);
static void __gpufreq_mfgx_rpc_ctrl(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name);
static void __gpufreq_mfg1_bus_prot(enum gpufreq_power_state power);
static void __gpufreq_footprint_mfg_timeout(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name, unsigned int footprint);
static int __gpufreq_clock_ctrl(enum gpufreq_power_state power);
static void __gpufreq_clksrc_ctrl(enum gpufreq_target target, enum gpufreq_clk_src clksrc);
static void __gpufreq_bus_clk_div2_config(void);
static void __gpufreq_top_hw_delsel_config(void);
static void __gpufreq_pdca_irq_config(void);
static void __gpufreq_top_hwdcm_config(void);
static void __gpufreq_stack_hwdcm_config(void);
static void __gpufreq_acp_config(void);
static void __gpufreq_axi_2to1_config(void);
static void __gpufreq_axi_merger_config(void);
static void __gpufreq_axuser_config(void);
static void __gpufreq_ocl_timestamp_config(void);
static void __gpufreq_gpm1_2_config(void);
static void __gpufreq_dfd2_0_config(void);
static void __gpufreq_broadcaster_config(void);
static void __gpufreq_check_bus_idle(void);
static void __gpufreq_check_devapc_vio(void);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* init function */
static void __gpufreq_update_springboard(void);
static void __gpufreq_interpolate_volt(enum gpufreq_target target);
static void __gpufreq_apply_adjustment(void);
static unsigned int __gpufreq_compute_avs_freq(unsigned int val);
static unsigned int __gpufreq_compute_avs_volt(unsigned int val);
static void __gpufreq_compute_avs(void);
static void __gpufreq_init_gpm3_0_table(void);
static int __gpufreq_init_opp_idx(void);
static void __gpufreq_init_opp_table(void);
static void __gpufreq_init_brisket(void);
static void __gpufreq_init_hbvc(void);
static void __gpufreq_init_leakage_info(void);
static void __gpufreq_init_shader_present(void);
static void __gpufreq_init_segment_id(void);
static int __gpufreq_init_clk(struct platform_device *pdev);
static int __gpufreq_init_pmic(struct platform_device *pdev);
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

static void __iomem *g_mali_base;
static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_cg_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfg_pll_sc0_base;
static void __iomem *g_mfg_pll_sc1_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_axuser_base;
static void __iomem *g_mfg_brcast_base;
static void __iomem *g_mfg_vgpu_devapc_ao_base;
static void __iomem *g_mfg_vgpu_devapc_base;
static void __iomem *g_mfg_smmu_base;
static void __iomem *g_mfg_hbvc_base;
static void __iomem *g_brisket_top_base;
static void __iomem *g_brisket_st0_base;
static void __iomem *g_brisket_st1_base;
static void __iomem *g_brisket_st3_base;
static void __iomem *g_brisket_st4_base;
static void __iomem *g_brisket_st5_base;
static void __iomem *g_brisket_st6_base;
static void __iomem *g_sleep;
static void __iomem *g_topckgen_base;
static void __iomem *g_emi_base;
static void __iomem *g_sub_emi_base;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_ifrbus_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_sth_emi_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_secure_base;
static void __iomem *g_drm_debug_base;
static void __iomem *g_nemi_mi32_mi33_smi;
static void __iomem *g_semi_mi32_mi33_smi;
static void __iomem *g_gpueb_sram_base;
static void __iomem *g_gpueb_cfgreg_base;
static void __iomem *g_mcdi_mbox_base;
static void __iomem *g_mcusys_par_wrap_base;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_status g_gpu;
static struct gpufreq_status g_stack;
static struct gpufreq_volt_sb g_springboard[NUM_PARKING_IDX];
static int g_temperature;
static int g_temper_comp_vgpu;
static int g_temper_comp_vstack;
static unsigned int g_vmeter_gpu_val;
static unsigned int g_vmeter_stack_val;
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_load;
static unsigned int g_aging_margin;
static unsigned int g_avs_enable;
static unsigned int g_avs_margin;
static unsigned int g_overdrive_ic;
static unsigned int g_gpm1_2_mode;
static unsigned int g_gpm3_0_mode;
static unsigned int g_custom_stack_imax;
static unsigned int g_custom_ref_istack;
static unsigned int g_mcuetm_clk_enable;
static unsigned int g_ptp_version;
static unsigned int g_dfd2_0_mode;
static unsigned int g_dfd3_6_mode;
static unsigned int g_test_mode;
static unsigned int g_gpueb_support;
static unsigned int g_hwdvfs_support;
static unsigned int g_gpufreq_ready;
static enum gpufreq_dvfs_state g_dvfs_state;
static struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_lock);

static struct gpufreq_platform_fp platform_ap_fp = {
	/* Common */
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.active_sleep_ctrl_enable = __gpufreq_active_sleep_ctrl_enable,
	.get_power_state = __gpufreq_get_power_state,
	.get_dvfs_state = __gpufreq_get_dvfs_state,
	.get_shader_present = __gpufreq_get_shader_present,
	.power_control = __gpufreq_power_control,
	.active_sleep_control = __gpufreq_active_sleep_control,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.update_debug_opp_info = __gpufreq_update_debug_opp_info,
	.set_shared_status = __gpufreq_set_shared_status,
	.fix_target_oppidx_dual = __gpufreq_fix_target_oppidx_dual,
	.fix_custom_freq_volt_dual = __gpufreq_fix_custom_freq_volt_dual,
	.update_temperature = __gpufreq_update_temperature,
	.mssv_commit = __gpufreq_mssv_commit,
	/* GPU */
	.get_cur_fgpu = __gpufreq_get_cur_fgpu,
	.get_cur_vgpu = __gpufreq_get_cur_vgpu,
	.get_cur_vsram_gpu = __gpufreq_get_cur_vsram_gpu,
	.get_cur_pgpu = __gpufreq_get_cur_pgpu,
	.get_max_pgpu = __gpufreq_get_max_pgpu,
	.get_min_pgpu = __gpufreq_get_min_pgpu,
	.get_cur_idx_gpu = __gpufreq_get_cur_idx_gpu,
	.get_opp_num_gpu = __gpufreq_get_opp_num_gpu,
	.get_signed_opp_num_gpu = __gpufreq_get_signed_opp_num_gpu,
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	/* STACK */
	.get_cur_fstack = __gpufreq_get_cur_fstack,
	.get_cur_vstack = __gpufreq_get_cur_vstack,
	.get_cur_vsram_stack = __gpufreq_get_cur_vsram_stack,
	.get_cur_pstack = __gpufreq_get_cur_pstack,
	.get_max_pstack = __gpufreq_get_max_pstack,
	.get_min_pstack = __gpufreq_get_min_pstack,
	.get_cur_idx_stack = __gpufreq_get_cur_idx_stack,
	.get_opp_num_stack = __gpufreq_get_opp_num_stack,
	.get_signed_opp_num_stack = __gpufreq_get_signed_opp_num_stack,
	.get_fstack_by_idx = __gpufreq_get_fstack_by_idx,
	.get_pstack_by_idx = __gpufreq_get_pstack_by_idx,
	.get_idx_by_fstack = __gpufreq_get_idx_by_fstack,
	.get_lkg_pstack = __gpufreq_get_lkg_pstack,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.fix_target_oppidx_stack = __gpufreq_fix_target_oppidx_stack,
	.fix_custom_freq_volt_stack = __gpufreq_fix_custom_freq_volt_stack,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_dyn_pstack = __gpufreq_get_dyn_pstack,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get POWER_CTRL enable status */
unsigned int __gpufreq_power_ctrl_enable(void)
{
	return GPUFREQ_POWER_CTRL_ENABLE;
}

/* API: get ACTIVE_SLEEP_CTRL status */
unsigned int __gpufreq_active_sleep_ctrl_enable(void)
{
	return GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE && GPUFREQ_POWER_CTRL_ENABLE;
}

/* API: get power state (on/off) */
unsigned int __gpufreq_get_power_state(void)
{
	if (g_stack.power_count > 0)
		return GPU_PWR_ON;
	else
		return GPU_PWR_OFF;
}

/* API: get DVFS state (free/disable/keep) */
unsigned int __gpufreq_get_dvfs_state(void)
{
	return g_dvfs_state;
}

/* API: get GPU shader core setting */
unsigned int __gpufreq_get_shader_present(void)
{
	return g_shader_present;
}

/* API: get type of current IC */
unsigned int __gpufreq_get_chip_type(void)
{
	if (g_overdrive_ic)
		return GPUIC_OVERDRIVE;
	else
		return GPUIC_NORMAL;
}

/* API: get current Freq of GPU */
unsigned int __gpufreq_get_cur_fgpu(void)
{
	return g_gpu.cur_freq;
}

/* API: get current Freq of STACK */
unsigned int __gpufreq_get_cur_fstack(void)
{
	return g_stack.cur_freq;
}

/* API: get current Volt of GPU */
unsigned int __gpufreq_get_cur_vgpu(void)
{
	return g_gpu.cur_volt;
}

/* API: get current Volt of STACK */
unsigned int __gpufreq_get_cur_vstack(void)
{
	return g_stack.cur_volt;
}

/* API: get current Vsram of GPU */
unsigned int __gpufreq_get_cur_vsram_gpu(void)
{
	return g_gpu.cur_vsram;
}

/* API: get current Vsram of STACK */
unsigned int __gpufreq_get_cur_vsram_stack(void)
{
	return g_stack.cur_vsram;
}

/* API: get current Power of GPU */
unsigned int __gpufreq_get_cur_pgpu(void)
{
	return g_gpu.working_table[g_gpu.cur_oppidx].power;
}

/* API: get current Power of STACK */
unsigned int __gpufreq_get_cur_pstack(void)
{
	return g_stack.working_table[g_stack.cur_oppidx].power;
}

/* API: get max Power of GPU */
unsigned int __gpufreq_get_max_pgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].power;
}

unsigned int __gpufreq_get_seg_max_opp_index(void)
{
	return g_gpu.segment_upbound;
}
EXPORT_SYMBOL(__gpufreq_get_seg_max_opp_index);

/* API: get max Power of STACK */
unsigned int __gpufreq_get_max_pstack(void)
{
	return g_stack.working_table[g_stack.max_oppidx].power;
}

/* API: get min Power of GPU */
unsigned int __gpufreq_get_min_pgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].power;
}

/* API: get min Power of STACK */
unsigned int __gpufreq_get_min_pstack(void)
{
	return g_stack.working_table[g_stack.min_oppidx].power;
}

/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return g_gpu.cur_oppidx;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return g_stack.cur_oppidx;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return g_gpu.opp_num;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return g_stack.opp_num;
}

/* API: get number of signed OPP of GPU */
int __gpufreq_get_signed_opp_num_gpu(void)
{
	return g_gpu.signed_opp_num;
}

/* API: get number of signed OPP of STACK */
int __gpufreq_get_signed_opp_num_stack(void)
{
	return g_stack.signed_opp_num;
}

/* API: get pointer of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}

/* API: get pointer of working OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void)
{
	return g_stack.working_table;
}

/* API: get pointer of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}

/* API: get pointer of signed OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void)
{
	return g_stack.signed_table;
}

/* API: get Freq of GPU via OPP index */
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].freq;
	else
		return 0;
}

/* API : get current GPU temperature */
int __gpufreq_get_gpu_temp(void)
{
#if IS_ENABLED(CONFIG_THERMAL)
	struct thermal_zone_device *zone;
	int ret, temp = 0;

	zone = thermal_zone_get_zone_by_name("gpu2");
	if (IS_ERR(zone))
		return 0;

	ret = thermal_zone_get_temp(zone, &temp);
	if (ret != 0)
		return 0;

	temp /= 1000;

	return temp;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(__gpufreq_get_gpu_temp);

/* API: get Freq of STACK via OPP index */
unsigned int __gpufreq_get_fstack_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_stack.opp_num)
		return g_stack.working_table[oppidx].freq;
	else
		return 0;
}

/* API: get Power of GPU via OPP index */
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].power;
	else
		return 0;
}

/* API: get Power of STACK via OPP index */
unsigned int __gpufreq_get_pstack_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_stack.opp_num)
		return g_stack.working_table[oppidx].power;
	else
		return 0;
}

/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given freq */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].freq >= freq)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given freq */
	for (i = g_stack.min_oppidx; i >= g_stack.max_oppidx; i--) {
		if (g_stack.working_table[i].freq >= freq)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_stack.max_oppidx) ? i : g_stack.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given volt */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].volt >= volt)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given volt */
	for (i = g_stack.min_oppidx; i >= g_stack.max_oppidx; i--) {
		if (g_stack.working_table[i].volt >= volt)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_stack.max_oppidx) ? i : g_stack.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given power */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].power >= power)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given power */
	for (i = g_stack.min_oppidx; i >= g_stack.max_oppidx; i--) {
		if (g_stack.working_table[i].power >= power)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_stack.max_oppidx) ? i : g_stack.max_oppidx;

	return oppidx;
}

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt, int temper)
{
	unsigned int p_leakage = 0;

	if ((temper >= LKG_HT_GPU_EFUSE_TEMPER) && g_gpu.lkg_ht_info)
		p_leakage = __gpufreq_lkg_formula_ht_gpu(volt / 100,
			temper, g_gpu.lkg_ht_info) * volt / 100000;
	else
		p_leakage = __gpufreq_lkg_formula_rt_gpu(volt / 100,
			temper, g_gpu.lkg_rt_info) * volt / 100000;

	return p_leakage;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = GPU_DYN_REF_POWER;
	unsigned int ref_freq = GPU_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = GPU_DYN_REF_POWER_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000 * 100000);

	return (unsigned int)p_dynamic;
}

/* API: get leakage Power of STACK */
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt, int temper)
{
	unsigned int p_leakage = 0;

	if ((temper >= LKG_HT_STACK_EFUSE_TEMPER) && g_stack.lkg_ht_info)
		p_leakage = __gpufreq_lkg_formula_ht_stack(volt / 100,
			temper, g_stack.lkg_ht_info) * volt / 100000;
	else
		p_leakage = __gpufreq_lkg_formula_rt_stack(volt / 100,
			temper, g_stack.lkg_rt_info) * volt / 100000;

	return p_leakage;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = STACK_DYN_REF_POWER;
	unsigned int ref_freq = STACK_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = STACK_DYN_REF_POWER_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000 * 100000);

	return (unsigned int)p_dynamic;
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	int ret = 0;
	u64 power_time = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("+ PWR_STATUS: 0x%08lx", MFG_0_22_37_PWR_STATUS);
	GPUFREQ_LOGD("switch power: %s (Power: %d, Active: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off", g_stack.power_count, g_stack.active_count,
		g_stack.buck_count, g_stack.mtcmos_count, g_stack.cg_count);

	if (power == GPU_PWR_ON) {
		g_gpu.power_count++;
		g_stack.power_count++;
#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		g_gpu.active_count++;
		g_stack.active_count++;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
	} else if (power == GPU_PWR_OFF) {
		g_gpu.power_count--;
		g_stack.power_count--;
#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		g_gpu.active_count--;
		g_stack.active_count--;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
	}
	__gpufreq_footprint_power_count(g_stack.power_count);

	if (power == GPU_PWR_ON && g_stack.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* config bus clock divide 2 in necessary */
		__gpufreq_bus_clk_div2_config();
		__gpufreq_footprint_power_step(0x02);

		/* enable Buck */
		ret = __gpufreq_buck_ctrl(GPU_PWR_ON);
		if (ret) {
			GPUFREQ_LOGE("fail to enable Buck (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x03);

		/* enable MTCMOS */
		__gpufreq_mtcmos_ctrl(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x04);

		/* enable Clock */
		ret = __gpufreq_clock_ctrl(GPU_PWR_ON);
		if (ret) {
			GPUFREQ_LOGE("fail to enable Clock (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x05);

#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		/* restore smmu after enable clock */
		__gpu_smmu_config(GPU_PWR_ON);
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
		__gpufreq_footprint_power_step(0x06);

		/* config TOP HW_DELSEL */
		__gpufreq_top_hw_delsel_config();
		__gpufreq_footprint_power_step(0x07);

		/* config PDCAv3 */
		__gpufreq_pdca_config(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x08);

		/* config PDCA IRQ */
		__gpufreq_pdca_irq_config();
		__gpufreq_footprint_power_step(0x09);

		/* config TOP HWDCM */
		__gpufreq_top_hwdcm_config();
		__gpufreq_footprint_power_step(0x0A);

#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		/* config STACK HWDCM */
		__gpufreq_stack_hwdcm_config();
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
		__gpufreq_footprint_power_step(0x0B);

		/* config ACP */
		__gpufreq_acp_config();
		__gpufreq_footprint_power_step(0x0C);

		/* config AXI 2to1 */
		__gpufreq_axi_2to1_config();
		__gpufreq_footprint_power_step(0x0D);

		/* config AXI transaction */
		__gpufreq_axi_merger_config();
		__gpufreq_footprint_power_step(0x0E);

#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		/* config AxUSER */
		__gpufreq_axuser_config();
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
		__gpufreq_footprint_power_step(0x0F);

		/* config OpenCL timestamp */
		__gpufreq_ocl_timestamp_config();
		__gpufreq_footprint_power_step(0x10);

		/* config GPM1.2 */
		__gpufreq_gpm1_2_config();
		__gpufreq_footprint_power_step(0x11);

		/* config DFD */
		__gpufreq_dfd2_0_config();
		__gpufreq_footprint_power_step(0x12);

		/* config Broadcaster if AutoDMA disable */
		__gpufreq_broadcaster_config();
		__gpufreq_footprint_power_step(0x13);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x14);
	} else if (power == GPU_PWR_OFF && g_stack.power_count == 0) {
		__gpufreq_footprint_power_step(0x15);

		/* check DEVAPC status before power off */
		__gpufreq_check_devapc_vio();
		__gpufreq_footprint_power_step(0x16);

		/* check all transaction complete before power off */
		__gpufreq_check_bus_idle();
		__gpufreq_footprint_power_step(0x17);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x18);

#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		/* backup smmu before disable clock */
		__gpu_smmu_config(GPU_PWR_OFF);
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
		__gpufreq_footprint_power_step(0x19);

		/* disable Clock */
		ret = __gpufreq_clock_ctrl(GPU_PWR_OFF);
		if (ret) {
			GPUFREQ_LOGE("fail to disable Clock (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x1A);

		/* disable MTCMOS */
		__gpufreq_mtcmos_ctrl(GPU_PWR_OFF);
		__gpufreq_footprint_power_step(0x1B);

		/* disable Buck */
		ret = __gpufreq_buck_ctrl(GPU_PWR_OFF);
		if (ret) {
			GPUFREQ_LOGE("fail to disable Buck (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x1C);
	}

	/* return power count if successfully control power */
	ret = g_stack.power_count;
	/* record time of successful power control */
	power_time = ktime_get_ns();

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->power_count = g_stack.power_count;
		g_shared_status->active_count = g_stack.active_count;
		g_shared_status->buck_count = g_stack.buck_count;
		g_shared_status->mtcmos_count = g_stack.mtcmos_count;
		g_shared_status->cg_count = g_stack.cg_count;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->power_time_h = (power_time >> 32) & GENMASK(31, 0);
		g_shared_status->power_time_l = power_time & GENMASK(31, 0);
	}

	if (power == GPU_PWR_ON && g_stack.power_count == 1)
		__gpufreq_footprint_power_step(0x1D);
	else if (power == GPU_PWR_OFF && g_stack.power_count == 0)
		__gpufreq_footprint_power_step(0x1E);

done_unlock:
	GPUFREQ_LOGD("- PWR_STATUS: 0x%08lx", MFG_0_22_37_PWR_STATUS);

	mutex_unlock(&gpufreq_lock);

#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	/* do DVFS after successful power-on and outside mutex lock */
	if (power == GPU_PWR_ON && ret == 1)
		__gpufreq_restore_last_oppidx();
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: control runtime active-idle state of GPU
 * return active_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_active_sleep_control(enum gpufreq_power_state power)
{
	int ret = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("+ PWR_STATUS: 0x%08lx", MFG_0_22_37_PWR_STATUS);
	GPUFREQ_LOGD("switch runtime state: %s (Active: %d, Buck: %d)",
		power ? "Active" : "Idle", g_stack.active_count, g_stack.buck_count);

	/* active-idle control is only available at power-on */
	if (__gpufreq_get_power_state() == GPU_PWR_OFF)
		__gpufreq_abort("switch active-idle at power-off (%d)", g_stack.power_count);

	if (power == GPU_PWR_ON) {
		g_gpu.active_count++;
		g_stack.active_count++;
	} else {
		g_gpu.active_count--;
		g_stack.active_count--;
	}

	if (power == GPU_PWR_ON && g_stack.active_count == 1) {
		/* power on VSTACK */
		ret = __gpufreq_vstack_ctrl(GPU_PWR_ON);
		if (ret)
			goto done;
		/* switch GPU MUX to PLL */
		__gpufreq_clksrc_ctrl(TARGET_GPU, CLOCK_MAIN);
		/* switch STACK MUX to PLL */
		__gpufreq_clksrc_ctrl(TARGET_STACK, CLOCK_MAIN);
		/* config SMMU */
		__gpu_smmu_config(GPU_PWR_ON);
		/* free DVFS when active */
		g_dvfs_state &= ~DVFS_SLEEP;
	} else if (power == GPU_PWR_OFF && g_stack.active_count == 0) {
		/* freeze DVFS when idle */
		g_dvfs_state |= DVFS_SLEEP;
		/* config SMMU */
		__gpu_smmu_config(GPU_PWR_OFF);
		/* switch STACK MUX to REF_SEL */
		__gpufreq_clksrc_ctrl(TARGET_STACK, CLOCK_SUB);
		/* switch GPU MUX to REF_SEL */
		__gpufreq_clksrc_ctrl(TARGET_GPU, CLOCK_SUB);
		/* power off VSTACK */
		ret = __gpufreq_vstack_ctrl(GPU_PWR_OFF);
		if (ret)
			goto done;
	} else if (g_stack.active_count < 0)
		__gpufreq_abort("incorrect active count: %d", g_stack.active_count);

	/* return active count if successfully control runtime state */
	ret = g_stack.active_count;

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->active_count = g_stack.active_count;
		g_shared_status->buck_count = g_stack.buck_count;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_fstack = g_stack.cur_freq;
	}

done:
	GPUFREQ_LOGD("state %s, Fgpu: %d, Fstack: %d, Vstack: %d",
		power ? "Active" : "Idle", __gpufreq_get_fmeter_freq(TARGET_GPU),
		__gpufreq_get_fmeter_freq(TARGET_STACK), __gpufreq_get_pmic_vstack());
	GPUFREQ_LOGD("- PWR_STATUS: 0x%08lx", MFG_0_22_37_PWR_STATUS);

	mutex_unlock(&gpufreq_lock);

	/* do DVFS after successful active and outside mutex lock */
	if (power == GPU_PWR_ON && g_stack.active_count == 1)
		__gpufreq_restore_last_oppidx();

	GPUFREQ_TRACE_END();

	return ret;
}

/* API: commit DVFS with only GPU OPP index */
int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/* API: commit DVFS with only STACK OPP index */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	return __gpufreq_generic_commit_dual(target_oppidx, target_oppidx, key);
}

/* API: commit DVFS by given both GPU and STACK OPP index */
int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	int ret = GPUFREQ_SUCCESS;
	/* GPU */
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	int cur_oppidx_gpu = 0, opp_num_gpu = g_gpu.opp_num;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	unsigned int cur_vsram_gpu = 0, target_vsram_gpu = 0;
	/* STACK */
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	int cur_oppidx_stack = 0, opp_num_stack = g_stack.opp_num;
	unsigned int cur_fstack = 0, cur_vstack = 0, target_fstack = 0, target_vstack = 0;
	unsigned int cur_vsram_stack = 0, target_vsram_stack = 0;

	GPUFREQ_TRACE_START("target_oppidx_gpu=%d, target_oppidx_stack=%d, key=%d",
		target_oppidx_gpu, target_oppidx_stack, key);

	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx_gpu < 0 || target_oppidx_gpu >= opp_num_gpu ||
		target_oppidx_stack < 0 || target_oppidx_stack >= opp_num_stack) {
		GPUFREQ_LOGE("invalid target GPU/STACK idx: %d/%d (num: %d/%d)",
			target_oppidx_gpu, target_oppidx_stack, opp_num_gpu, opp_num_stack);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpufreq_lock);
	__gpufreq_footprint_dvfs_step(0x01);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		/* still update Volt when DVFS is fixed by fix OPP cmd */
		if (g_dvfs_state == DVFS_FIX_OPP) {
			target_oppidx_gpu = g_gpu.cur_oppidx;
			target_oppidx_stack = g_stack.cur_oppidx;
		/* otherwise skip */
		} else {
			ret = GPUFREQ_SUCCESS;
			goto done_unlock;
		}
	}

	/* prepare OPP setting */
	cur_oppidx_gpu = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	cur_oppidx_stack = g_stack.cur_oppidx;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram_stack = g_stack.cur_vsram;

	target_fgpu = working_gpu[target_oppidx_gpu].freq;
	target_vgpu = working_gpu[target_oppidx_gpu].volt;
	target_fstack = working_stack[target_oppidx_stack].freq;
	target_vstack = working_stack[target_oppidx_stack].volt;

	/* temperature compensation check */
	target_vgpu = (int)target_vgpu + g_temper_comp_vgpu;
	target_vstack = (int)target_vstack + g_temper_comp_vstack;

	/* clamp to signoff */
	if (target_vgpu > GPU_MAX_SIGNOFF_VOLT)
		target_vgpu = GPU_MAX_SIGNOFF_VOLT;
	if (target_vstack > STACK_MAX_SIGNOFF_VOLT)
		target_vstack = STACK_MAX_SIGNOFF_VOLT;
	target_vsram_gpu = __gpufreq_get_vsram_by_vlogic(target_vgpu);
	target_vsram_stack = __gpufreq_get_vsram_by_vlogic(target_vstack);

	__gpufreq_footprint_oppidx(target_oppidx_stack);
	GPUFREQ_LOGD("commit GPU OPP(%d->%d), STACK OPP(%d->%d)",
		cur_oppidx_gpu, target_oppidx_gpu, cur_oppidx_stack, target_oppidx_stack);

	ret = __gpufreq_generic_scale(cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_fstack, target_fstack, cur_vstack, target_vstack,
		target_vsram_gpu, target_vsram_stack);
	if (ret) {
		GPUFREQ_LOGE(
			"fail to commit GPU F(%d)/V(%d)/VSRAM(%d), STACK F(%d)/V(%d)/VSRAM(%d)",
			target_fgpu, target_vgpu, target_vsram_gpu,
			target_fstack, target_vstack, target_vsram_stack);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;
	g_stack.cur_oppidx = target_oppidx_stack;

	__gpufreq_footprint_dvfs_step(0x02);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_oppidx_stack = g_stack.cur_oppidx;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->cur_vstack = g_stack.cur_volt;
		g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
		g_shared_status->cur_power_stack = g_stack.working_table[g_stack.cur_oppidx].power;
	}

done_unlock:
	__gpufreq_footprint_dvfs_step(0x03);
	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: fix OPP of GPU via given OPP index */
int __gpufreq_fix_target_oppidx_gpu(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);

	return GPUFREQ_EINVAL;
}

/* API: fix OPP of STACK via given OPP index */
int __gpufreq_fix_target_oppidx_stack(int oppidx)
{
	return __gpufreq_fix_target_oppidx_dual(oppidx, oppidx);
}

/* API: fix OPP via given GPU and STACK OPP index */
int __gpufreq_fix_target_oppidx_dual(int oppidx_gpu, int oppidx_stack)
{
	int ret = GPUFREQ_SUCCESS;
	int opp_num_gpu = g_gpu.opp_num;
	int opp_num_stack = g_stack.opp_num;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	ret = __gpufreq_active_sleep_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	if (oppidx_gpu == -1 && oppidx_stack == -1) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx_gpu >= 0 && oppidx_gpu < opp_num_gpu &&
		oppidx_stack >= 0 && oppidx_stack < opp_num_stack) {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_OPP);
		ret = __gpufreq_generic_commit_dual(oppidx_gpu, oppidx_stack, DVFS_FIX_OPP);
		if (ret)
			__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
	} else
		ret = GPUFREQ_EINVAL;

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	__gpufreq_active_sleep_control(GPU_PWR_OFF);
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	if (ret)
		GPUFREQ_LOGE("fail to commit GPU/STACK idx: %d/%d", oppidx_gpu, oppidx_stack);

	return ret;
}

/* API: fix Freq and Volt via given GPU Freq and Volt */
int __gpufreq_fix_custom_freq_volt_gpu(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return GPUFREQ_EINVAL;
}

/* API: fix Freq and Volt via given STACK Freq and Volt */
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt)
{
	unsigned int max_freq = 0, min_freq = 0;
	unsigned int max_volt = 0, min_volt = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	ret = __gpufreq_active_sleep_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	if (g_test_mode) {
		max_freq = POSDIV_2_MAX_FREQ;
		max_volt = VSTACK_MAX_VOLT;
	} else {
		max_freq = g_stack.working_table[0].freq;
		max_volt = g_stack.working_table[0].volt;
	}

	min_freq = POSDIV_16_MIN_FREQ;
	min_volt = VSTACK_MIN_VOLT;

	if (freq == 0 && volt == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > max_freq || freq < min_freq) {
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_FREQ_VOLT);
		ret = __gpufreq_custom_commit_stack(freq, volt, DVFS_FIX_FREQ_VOLT);
		if (ret)
			__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
	}

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	__gpufreq_active_sleep_control(GPU_PWR_OFF);
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	if (ret)
		GPUFREQ_LOGE("fail to commit STACK F(%d)/V(%d)", freq, volt);

	return ret;
}

/* API: fix Freq and Volt via given GPU and STACK Freq and Volt */
int __gpufreq_fix_custom_freq_volt_dual(unsigned int fgpu, unsigned int vgpu,
	unsigned int fstack, unsigned int vstack)
{
	unsigned int max_fgpu = 0, min_fgpu = 0, max_vgpu = 0, min_vgpu = 0;
	unsigned int max_fstack = 0, min_fstack = 0, max_vstack = 0, min_vstack = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	ret = __gpufreq_active_sleep_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	if (g_test_mode) {
		max_fgpu = POSDIV_2_MAX_FREQ;
		max_vgpu = VGPU_MAX_VOLT;
		max_fstack = POSDIV_2_MAX_FREQ;
		max_vstack = VSTACK_MAX_VOLT;
	} else {
		max_fgpu = g_gpu.working_table[0].freq;
		max_vgpu = g_gpu.working_table[0].volt;
		max_fstack = g_stack.working_table[0].freq;
		max_vstack = g_stack.working_table[0].volt;
	}

	min_fgpu = POSDIV_16_MIN_FREQ;
	min_vgpu = VGPU_MIN_VOLT;
	min_fstack = POSDIV_16_MIN_FREQ;
	min_vstack = VSTACK_MIN_VOLT;

	if (fgpu == 0 && vgpu == 0 && fstack == 0 && vstack == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
		ret = GPUFREQ_SUCCESS;
	} else if (fgpu > max_fgpu || fgpu < min_fgpu ||
		fstack > max_fstack || fstack < min_fstack) {
		ret = GPUFREQ_EINVAL;
	} else if (vgpu > max_vgpu || vgpu < min_vgpu ||
		vstack > max_vstack || vstack < min_vstack) {
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_FREQ_VOLT);
		ret = __gpufreq_custom_commit_dual(fgpu, vgpu,
			fstack, vstack, DVFS_FIX_FREQ_VOLT);
		if (ret)
			__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
	}

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	__gpufreq_active_sleep_control(GPU_PWR_OFF);
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	if (ret)
		GPUFREQ_LOGE("fail to commit GPU F(%d)/V(%d), STACK F(%d)/V(%d)",
			fgpu, vgpu, fstack, vstack);

	return ret;
}

void __gpufreq_dump_infra_status(char *log_buf, int *log_len, int log_size)
{
	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s MFG_PLL: %d, MFG_SC0_PLL: %d, MFG_SC1_PLL: %d, MFG_SEL: 0x%08x",
		"[CLK]", __gpufreq_get_pll_fgpu(), __gpufreq_get_pll_fstack0(),
		__gpufreq_get_pll_fstack1(), DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL));

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	DRV_WriteReg32(MFG_QCHANNEL_CON, (DRV_Reg32(MFG_QCHANNEL_CON) | BIT(0)));
	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	DRV_WriteReg32(MFG_DEBUG_SEL, (DRV_Reg32(MFG_DEBUG_SEL) | GENMASK(1, 0)));

	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MALI]",
		"MALI_SHADER", DRV_Reg32(MALI_SHADER_READY_LO),
		"MALI_TILER", DRV_Reg32(MALI_TILER_READY_LO),
		"MALI_L2", DRV_Reg32(MALI_L2_READY_LO),
		"MALI_GPU_IRQ", DRV_Reg32(MALI_GPU_IRQ_STATUS));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MFG_DBG]",
		"MFG_DEBUG_SEL", DRV_Reg32(MFG_DEBUG_SEL),
		"MFG_DEBUG_TOP", DRV_Reg32(MFG_DEBUG_TOP),
		"RPC_AO_CLK_CFG", DRV_Reg32(MFG_RPC_AO_CLK_CFG),
		"MFG_RPC_SLP_PROT", DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[BUS_PROT]",
		"EMISYS_PROT_EN_0", DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0),
		"EMISYS_PROT_RDY_0", DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_0),
		"EMISYS_PROT_EN_1", DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1),
		"EMISYS_PROT_RDY_1", DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_1));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(SUB_EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(SUB_EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(SUB_EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(SUB_EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(SUB_EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(SUB_EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(SUB_EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(SUB_EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(SUB_EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(SUB_EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(SUB_EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"NTH_MFG_EMI1_GALS_SLV", DRV_Reg32(NTH_MFG_EMI1_GALS_SLV_DBG),
		"NTH_MFG_EMI0_GALS_SLV", DRV_Reg32(NTH_MFG_EMI0_GALS_SLV_DBG),
		"STH_MFG_EMI1_GALS_SLV", DRV_Reg32(STH_MFG_EMI1_GALS_SLV_DBG),
		"STH_MFG_EMI0_GALS_SLV", DRV_Reg32(STH_MFG_EMI0_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_GALS]",
		"NTH_APU_EMI1_GALS_SLV", DRV_Reg32(NTH_APU_EMI1_GALS_SLV_DBG),
		"NTH_APU_EMI0_GALS_SLV", DRV_Reg32(NTH_APU_EMI0_GALS_SLV_DBG),
		"STH_APU_EMI1_GALS_SLV", DRV_Reg32(STH_APU_EMI1_GALS_SLV_DBG),
		"STH_APU_EMI0_GALS_SLV", DRV_Reg32(STH_APU_EMI0_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_M6M7]",
		"NTH_M6M7_IDLE_1", DRV_Reg32(NTH_AO_M6M7_IDLE_BIT_EN_1),
		"NTH_M6M7_IDLE_0", DRV_Reg32(NTH_AO_M6M7_IDLE_BIT_EN_0),
		"STH_M6M7_IDLE_1", DRV_Reg32(STH_AO_M6M7_IDLE_BIT_EN_1),
		"STH_M6M7_IDLE_0", DRV_Reg32(STH_AO_M6M7_IDLE_BIT_EN_0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_PROT]",
		"NTH_SLEEP_PROT", DRV_Reg32(NTH_AO_SLEEP_PROT_START),
		"NTH_GLITCH_PROT", DRV_Reg32(NTH_AO_GLITCH_PROT_START),
		"STH_SLEEP_PROT", DRV_Reg32(STH_AO_SLEEP_PROT_START),
		"STH_GLITCH_PROT", DRV_Reg32(STH_AO_GLITCH_PROT_START));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI32_SMI_S0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S0),
		"NEMI_MI32_SMI_S1", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S1),
		"NEMI_MI32_SMI_S2", DRV_Reg32(NEMI_MI32_SMI_DEBUG_S2),
		"NEMI_MI32_SMI_M0", DRV_Reg32(NEMI_MI32_SMI_DEBUG_M0),
		"NEMI_MI32_SMI_MISC", DRV_Reg32(NEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"NEMI_MI33_SMI_S0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S0),
		"NEMI_MI33_SMI_S1", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S1),
		"NEMI_MI33_SMI_S2", DRV_Reg32(NEMI_MI33_SMI_DEBUG_S2),
		"NEMI_MI33_SMI_M0", DRV_Reg32(NEMI_MI33_SMI_DEBUG_M0),
		"NEMI_MI33_SMI_MISC", DRV_Reg32(NEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"SEMI_MI32_SMI_S0", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S0),
		"SEMI_MI32_SMI_S1", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S1),
		"SEMI_MI32_SMI_S2", DRV_Reg32(SEMI_MI32_SMI_DEBUG_S2),
		"SEMI_MI32_SMI_M0", DRV_Reg32(SEMI_MI32_SMI_DEBUG_M0),
		"SEMI_MI32_SMI_MISC", DRV_Reg32(SEMI_MI32_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_SMI]",
		"SEMI_MI33_SMI_S0", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S0),
		"SEMI_MI33_SMI_S1", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S1),
		"SEMI_MI33_SMI_S2", DRV_Reg32(SEMI_MI33_SMI_DEBUG_S2),
		"SEMI_MI33_SMI_M0", DRV_Reg32(SEMI_MI33_SMI_DEBUG_M0),
		"SEMI_MI33_SMI_MISC", DRV_Reg32(SEMI_MI33_SMI_DEBUG_MISC));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[EMI_DBG]",
		"INFRA_BUS0_DBG_CTRL", DRV_Reg32(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		"INFRA_BUS1_DBG_CTRL", DRV_Reg32(INFRA_AO1_BUS1_U_DEBUG_CTRL0),
		"NTH_EMI_BUS_DBG_CTRL", DRV_Reg32(NTH_EMI_AO_BUS_U_DEBUG_CTRL0),
		"STH_EMI_BUS_DBG_CTRL", DRV_Reg32(STH_EMI_AO_BUS_U_DEBUG_CTRL0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MFG_ACP]",
		"MCUSYS_ACP_GALS", DRV_Reg32(MCUSYS_PAR_WRAP_ACP_GALS_DBG),
		"NTH_DVM_GALS_MST", DRV_Reg32(NTH_MFG_ACP_DVM_GALS_MST_DBG),
		"NTH_GALS_SLV", DRV_Reg32(NTH_MFG_ACP_GALS_SLV_DBG),
		"STH_DVM_GALS_MST", DRV_Reg32(STH_MFG_ACP_DVM_GALS_MST_DBG),
		"STH_GALS_SLV", DRV_Reg32(STH_MFG_ACP_GALS_SLV_DBG));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08lx",
		"[MISC]",
		"SPM_SRC_REQ", DRV_Reg32(SPM_SRC_REQ),
		"SPM_SOC_BUCK_ISO", DRV_Reg32(SPM_SOC_BUCK_ISO_CON),
		"PWR_STATUS", MFG_0_22_37_PWR_STATUS);
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[HBVC]",
		"CFG", DRV_Reg32(MFG_HBVC_CFG),
		"SAMPLE_EN", DRV_Reg32(MFG_HBVC_SAMPLE_EN),
		"GRP0_BACKEND0", DRV_Reg32(MFG_HBVC_GRP0_DBG_BACKEND0),
		"GRP1_BACKEND0", DRV_Reg32(MFG_HBVC_GRP1_DBG_BACKEND0),
		"FLL0_FRONTEND0", DRV_Reg32(MFG_HBVC_FLL0_DBG_FRONTEND0),
		"FLL1_FRONTEND0", DRV_Reg32(MFG_HBVC_FLL1_DBG_FRONTEND0));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[BRCAST]",
		"ERROR_IRQ", DRV_Reg32(MFG_BRCAST_ERROR_IRQ),
		"DEBUG_INFO_9", DRV_Reg32(MFG_BRCAST_DEBUG_INFO_9),
		"DEBUG_INFO_10", DRV_Reg32(MFG_BRCAST_DEBUG_INFO_10),
		"DEBUG_INFO_11", DRV_Reg32(MFG_BRCAST_DEBUG_INFO_11),
		"CONFIG_0", DRV_Reg32(MFG_BRCAST_CONFIG_0),
		"CONFIG_1", DRV_Reg32(MFG_BRCAST_CONFIG_1));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[AUTO_DMA0]",
		"STATUS", DRV_Reg32(GPUEB_AUTO_DMA_0_STATUS),
		"CUR_PC", DRV_Reg32(GPUEB_AUTO_DMA_0_CUR_PC),
		"TRIGGER_STATUS", DRV_Reg32(GPUEB_AUTO_DMA_0_TRIGGER_STATUS),
		"R0", DRV_Reg32(GPUEB_AUTO_DMA_0_R0),
		"R1", DRV_Reg32(GPUEB_AUTO_DMA_0_R1),
		"R2", DRV_Reg32(GPUEB_AUTO_DMA_0_R2));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[AUTO_DMA0]",
		"R3", DRV_Reg32(GPUEB_AUTO_DMA_0_R3),
		"R4", DRV_Reg32(GPUEB_AUTO_DMA_0_R4),
		"R5", DRV_Reg32(GPUEB_AUTO_DMA_0_R5),
		"R6", DRV_Reg32(GPUEB_AUTO_DMA_0_R6),
		"R7", DRV_Reg32(GPUEB_AUTO_DMA_0_R7));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[AUTO_DMA1]",
		"STATUS", DRV_Reg32(GPUEB_AUTO_DMA_1_STATUS),
		"CUR_PC", DRV_Reg32(GPUEB_AUTO_DMA_1_CUR_PC),
		"TRIGGER_STATUS", DRV_Reg32(GPUEB_AUTO_DMA_1_TRIGGER_STATUS),
		"R0", DRV_Reg32(GPUEB_AUTO_DMA_1_R0),
		"R1", DRV_Reg32(GPUEB_AUTO_DMA_1_R1),
		"R2", DRV_Reg32(GPUEB_AUTO_DMA_1_R2));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[AUTO_DMA1]",
		"R3", DRV_Reg32(GPUEB_AUTO_DMA_1_R3),
		"R4", DRV_Reg32(GPUEB_AUTO_DMA_1_R4),
		"R5", DRV_Reg32(GPUEB_AUTO_DMA_1_R5),
		"R6", DRV_Reg32(GPUEB_AUTO_DMA_1_R6),
		"R7", DRV_Reg32(GPUEB_AUTO_DMA_1_R7));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG0", DRV_Reg32(SPM_MFG0_PWR_CON),
		"MFG1", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MFG37", DRV_Reg32(MFG_RPC_MFG37_PWR_CON),
		"MFG2", DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		"MFG3", DRV_Reg32(MFG_RPC_MFG3_PWR_CON),
		"MFG4", DRV_Reg32(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG22", DRV_Reg32(MFG_RPC_MFG22_PWR_CON),
		"MFG6", DRV_Reg32(MFG_RPC_MFG6_PWR_CON),
		"MFG7", DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		"MFG8", DRV_Reg32(MFG_RPC_MFG8_PWR_CON),
		"MFG9", DRV_Reg32(MFG_RPC_MFG9_PWR_CON),
		"MFG10", DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG11", DRV_Reg32(MFG_RPC_MFG11_PWR_CON),
		"MFG12", DRV_Reg32(MFG_RPC_MFG12_PWR_CON),
		"MFG13", DRV_Reg32(MFG_RPC_MFG13_PWR_CON),
		"MFG14", DRV_Reg32(MFG_RPC_MFG14_PWR_CON),
		"MFG15", DRV_Reg32(MFG_RPC_MFG15_PWR_CON),
		"MFG16", DRV_Reg32(MFG_RPC_MFG16_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[MTCMOS]",
		"MFG17", DRV_Reg32(MFG_RPC_MFG17_PWR_CON),
		"MFG18", DRV_Reg32(MFG_RPC_MFG18_PWR_CON),
		"MFG19", DRV_Reg32(MFG_RPC_MFG19_PWR_CON),
		"MFG20", DRV_Reg32(MFG_RPC_MFG20_PWR_CON));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[NEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"URGENT_CNT", DRV_Reg32(SUB_EMI_URGENT_CNT),
		"MD_LAT", DRV_Reg32(SUB_EMI_MD_LAT_HRT_URGENT_CNT),
		"MD", DRV_Reg32(SUB_EMI_MD_HRT_URGENT_CNT),
		"DISP", DRV_Reg32(SUB_EMI_DISP_HRT_URGENT_CNT),
		"CAM", DRV_Reg32(SUB_EMI_CAM_HRT_URGENT_CNT),
		"MDMCU", DRV_Reg32(SUB_EMI_MDMCU_HRT_URGENT_CNT));
	GPUFREQ_LOGB(log_buf, log_len, log_size,
		"%-11s %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"[SEMI_THRO]",
		"MD_WR_LAT", DRV_Reg32(SUB_EMI_MD_WR_LAT_HRT_URGENT_CNT),
		"MDMCU_LOW", DRV_Reg32(SUB_EMI_MDMCU_LOW_LAT_URGENT_CNT),
		"MDMCU_HIGH", DRV_Reg32(SUB_EMI_MDMCU_HIGH_LAT_URGENT_CNT),
		"MDMCU_LOW_WR", DRV_Reg32(SUB_EMI_MDMCU_LOW_WR_LAT_URGENT_CNT),
		"MDMCU_HIGH_WR", DRV_Reg32(SUB_EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT));
}

void __gpufreq_dump_power_tracker_status(void)
{
	int i = 0;
	unsigned int val = 0, cur_point = 0, read_point = 0;

	if (!g_gpufreq_ready)
		return;

	cur_point = (DRV_Reg32(MFG_POWER_TRACKER_SETTING) >> 10) & GENMASK(5, 0);
	GPUFREQ_LOGI("== [PDC POWER TRAKER STATUS: %d] ==", cur_point);
	for (i = 1; i <= 16; i++) {
		/* only dump last 16 record */
		read_point = (cur_point + ~i + 1) & GENMASK(5, 0);
		val = (DRV_Reg32(MFG_POWER_TRACKER_SETTING) & ~GENMASK(9, 4)) | (read_point << 4);
		DRV_WriteReg32(MFG_POWER_TRACKER_SETTING, val);
		udelay(1);

		GPUFREQ_LOGI("[%d][%d] STA 1=0x%08x, 2=0x%08x, 3=0x%08x, 4=0x%08x, 5=0x%08x",
			read_point, DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS0),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS1),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS2),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS3),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS4),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS5));
	}
}

/* API: update debug info to shared memory */
void __gpufreq_update_debug_opp_info(void)
{
	if (!g_shared_status)
		return;

	mutex_lock(&gpufreq_lock);

	/* update current status to shared memory */
	if (__gpufreq_get_power_state()) {
		g_shared_status->cur_con1_fgpu = __gpufreq_get_pll_fgpu();
		g_shared_status->cur_con1_fstack = __gpufreq_get_pll_fstack0();
		g_shared_status->cur_fmeter_fgpu = __gpufreq_get_fmeter_freq(TARGET_GPU);
		g_shared_status->cur_fmeter_fstack = __gpufreq_get_fmeter_freq(TARGET_STACK);
		g_shared_status->cur_regulator_vsram_gpu = __gpufreq_get_dreq_vsram_gpu();
		g_shared_status->cur_regulator_vsram_stack = __gpufreq_get_dreq_vsram_stack();
		g_shared_status->temperature = g_temperature;
		g_shared_status->temper_comp_norm_gpu = g_temper_comp_vgpu;
		g_shared_status->temper_comp_norm_stack = g_temper_comp_vstack;
	} else {
		g_shared_status->cur_con1_fgpu = 0;
		g_shared_status->cur_con1_fstack = 0;
		g_shared_status->cur_fmeter_fgpu = 0;
		g_shared_status->cur_fmeter_fstack = 0;
		g_shared_status->cur_regulator_vsram_gpu = 0;
		g_shared_status->cur_regulator_vsram_stack = 0;
		g_shared_status->temperature = TEMPERATURE_DEFAULT;
		g_shared_status->temper_comp_norm_gpu = TEMPER_COMP_DEFAULT_VOLT;
		g_shared_status->temper_comp_norm_stack = TEMPER_COMP_DEFAULT_VOLT;
	}
	g_shared_status->cur_regulator_vgpu = __gpufreq_get_pmic_vgpu();
	g_shared_status->cur_regulator_vstack = __gpufreq_get_pmic_vstack();
	g_shared_status->mfg_pwr_status = MFG_0_22_37_PWR_STATUS;

	mutex_unlock(&gpufreq_lock);
}

/* API: general interface to set MFGSYS config */
void __gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val)
{
	mutex_lock(&gpufreq_lock);

	switch (target) {
	case CONFIG_STRESS_TEST:
		gpuppm_set_stress_test(val);
		break;
	case CONFIG_MARGIN:
		__gpufreq_set_margin_mode(val);
		break;
	case CONFIG_GPM1:
		__gpufreq_set_gpm_mode(1, val);
		break;
	case CONFIG_GPM3:
		__gpufreq_set_gpm_mode(3, val);
		break;
	case CONFIG_DFD:
		g_dfd2_0_mode = val;
		break;
	case CONFIG_IMAX_STACK:
		g_custom_stack_imax = val;
		break;
	case CONFIG_DYN_STACK:
		g_custom_ref_istack = val;
		break;
	case CONFIG_MCUETM_CLK:
		__gpufreq_set_mcu_etm_clock(val);
		break;
	case CONFIG_DEVAPC_HANDLE:
		__gpufreq_devapc_vio_handler();
		break;
	default:
		GPUFREQ_LOGE("invalid config target: %d", target);
		break;
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: update GPU temper and temper related features */
void __gpufreq_update_temperature(void)
{
	int cur_temper = 0;

	mutex_lock(&gpufreq_lock);

	// todo: get temper
	cur_temper = 30;
	g_temperature = cur_temper;

	/* compensate Volt according to temper */
	__gpufreq_set_temper_compensation();
	/* set ceiling according to current */
	__gpufreq_set_gpm3_0_limit();

	if (g_shared_status) {
		g_shared_status->temperature = g_temperature;
		g_shared_status->temper_comp_norm_gpu = g_temper_comp_vgpu;
		g_shared_status->temper_comp_norm_stack = g_temper_comp_vstack;
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: get core_mask table */
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void)
{
	return g_core_mask_table;
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

/* PDCA: GPU DDK automatically control GPU shader MTCMOS */
void __gpufreq_pdca_config(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCA_ENABLE
	if (power == GPU_PWR_ON) {
		/* MFG_APM_CG_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_CG_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_CG_06) | BIT(0)));
		/* MFG_APM_ST0_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST0_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST0_06) | BIT(0)));
		/* MFG_APM_ST1_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST1_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST1_06) | BIT(0)));
		/* MFG_APM_ST3_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST3_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST3_06) | BIT(0)));
		/* MFG_APM_ST4_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST4_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST4_06) | BIT(0)));
		/* MFG_APM_ST5_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST5_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST5_06) | BIT(0)));
		/* MFG_APM_ST6_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST6_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST6_06) | BIT(0)));
		/* MFG_APM_SC0_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_00,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_00) | BIT(0)));
		/* MFG_APM_SC1_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_06) | BIT(0)));
		/* MFG_APM_SC2_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_12,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_12) | BIT(0)));
		/* MFG_APM_SC3_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_18,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_18) | BIT(0)));
		/* MFG_APM_SC4_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_24,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_24) | BIT(0)));
		/* MFG_APM_SC5_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_30,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_30) | BIT(0)));
		/* MFG_APM_SC6_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_36,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_36) | BIT(0)));
		/* MFG_APM_SC7_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_42,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_42) | BIT(0)));
		/* MFG_APM_SC8_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_48,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_48) | BIT(0)));
		/* MFG_APM_SC9_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_54,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_54) | BIT(0)));
		/* MFG_APM_SC10_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_60,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_60) | BIT(0)));
		/* MFG_APM_SC11_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_66,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_66) | BIT(0)));
		/* MFG_APM_RTU0_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_00,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_00) | BIT(0)));
		/* MFG_APM_RTU1_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_06,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_06) | BIT(0)));
		/* MFG_APM_RTU2_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_12,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_12) | BIT(0)));
		/* MFG_APM_RTU3_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_18,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_18) | BIT(0)));
		/* MFG_APM_RTU4_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_24,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_24) | BIT(0)));
		/* MFG_APM_RTU5_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_30,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_30) | BIT(0)));
		/* MFG_APM_RTU6_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_36,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_36) | BIT(0)));
		/* MFG_APM_RTU7_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_42,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_42) | BIT(0)));
		/* MFG_APM_RTU8_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_48,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_48) | BIT(0)));
		/* MFG_APM_RTU9_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_54,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_54) | BIT(0)));
		/* MFG_APM_RTU10_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_60,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_60) | BIT(0)));
		/* MFG_APM_RTU11_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_66,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_66) | BIT(0)));
	} else if (power == GPU_PWR_OFF) {
		/* MFG_APM_CG_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_CG_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_CG_06) & ~BIT(0)));
		/* MFG_APM_ST0_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST0_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST0_06) & ~BIT(0)));
		/* MFG_APM_ST1_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST1_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST1_06) & ~BIT(0)));
		/* MFG_APM_ST3_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST3_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST3_06) & ~BIT(0)));
		/* MFG_APM_ST4_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST4_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST4_06) & ~BIT(0)));
		/* MFG_APM_ST5_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST5_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST5_06) & ~BIT(0)));
		/* MFG_APM_ST6_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST6_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST6_06) & ~BIT(0)));
		/* MFG_APM_SC0_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_00,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_00) & ~BIT(0)));
		/* MFG_APM_SC1_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_06) & ~BIT(0)));
		/* MFG_APM_SC2_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_12,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_12) & ~BIT(0)));
		/* MFG_APM_SC3_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_18,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_18) & ~BIT(0)));
		/* MFG_APM_SC4_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_24,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_24) & ~BIT(0)));
		/* MFG_APM_SC5_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_30,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_30) & ~BIT(0)));
		/* MFG_APM_SC6_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_36,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_36) & ~BIT(0)));
		/* MFG_APM_SC7_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_42,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_42) & ~BIT(0)));
		/* MFG_APM_SC8_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_48,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_48) & ~BIT(0)));
		/* MFG_APM_SC9_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_54,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_54) & ~BIT(0)));
		/* MFG_APM_SC10_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_60,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_60) & ~BIT(0)));
		/* MFG_APM_SC11_0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_66,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_66) & ~BIT(0)));
		/* MFG_APM_RTU0_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_00,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_00) & ~BIT(0)));
		/* MFG_APM_RTU1_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_06,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_06) & ~BIT(0)));
		/* MFG_APM_RTU2_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_12,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_12) & ~BIT(0)));
		/* MFG_APM_RTU3_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_18,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_18) & ~BIT(0)));
		/* MFG_APM_RTU4_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_24,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_24) & ~BIT(0)));
		/* MFG_APM_RTU5_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_30,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_30) & ~BIT(0)));
		/* MFG_APM_RTU6_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_36,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_36) & ~BIT(0)));
		/* MFG_APM_RTU7_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_42,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_42) & ~BIT(0)));
		/* MFG_APM_RTU8_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_48,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_48) & ~BIT(0)));
		/* MFG_APM_RTU9_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_54,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_54) & ~BIT(0)));
		/* MFG_APM_RTU10_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_60,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_60) & ~BIT(0)));
		/* MFG_APM_RTU11_0 */
		DRV_WriteReg32(MFG_RTU_ACTIVE_POWER_CON_66,
			(DRV_Reg32(MFG_RTU_ACTIVE_POWER_CON_66) & ~BIT(0)));
	}
#else
	GPUFREQ_UNREFERENCED(power);
#endif /* GPUFREQ_PDCA_ENABLE */
}

/* API: init first time shared status */
void __gpufreq_set_shared_status(struct gpufreq_shared_status *shared_status)
{
	mutex_lock(&gpufreq_lock);

	if (shared_status)
		g_shared_status = shared_status;
	else
		__gpufreq_abort("null gpufreq shared status: 0x%llx", shared_status);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->magic = GPUFREQ_MAGIC_NUMBER;
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->opp_num_gpu = g_gpu.opp_num;
		g_shared_status->signed_opp_num_gpu = g_gpu.signed_opp_num;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = g_gpu.working_table[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = g_gpu.working_table[g_gpu.min_oppidx].power;
		g_shared_status->cur_oppidx_stack = g_stack.cur_oppidx;
		g_shared_status->opp_num_stack = g_stack.opp_num;
		g_shared_status->signed_opp_num_stack = g_stack.signed_opp_num;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->cur_vstack = g_stack.cur_volt;
		g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
		g_shared_status->cur_power_stack = g_stack.working_table[g_stack.cur_oppidx].power;
		g_shared_status->max_power_stack = g_stack.working_table[g_stack.max_oppidx].power;
		g_shared_status->min_power_stack = g_stack.working_table[g_stack.min_oppidx].power;
		g_shared_status->lkg_rt_info_gpu = g_gpu.lkg_rt_info;
		g_shared_status->lkg_rt_info_stack = g_stack.lkg_rt_info;
		g_shared_status->lkg_rt_info_sram = g_stack.lkg_rt_info_sram;
		g_shared_status->lkg_ht_info_gpu = g_gpu.lkg_ht_info;
		g_shared_status->lkg_ht_info_stack = g_stack.lkg_ht_info;
		g_shared_status->lkg_ht_info_sram = g_stack.lkg_ht_info_sram;
		g_shared_status->power_count = g_stack.power_count;
		g_shared_status->buck_count = g_stack.buck_count;
		g_shared_status->mtcmos_count = g_stack.mtcmos_count;
		g_shared_status->cg_count = g_stack.cg_count;
		g_shared_status->active_count = g_stack.active_count;
		g_shared_status->power_control = __gpufreq_power_ctrl_enable();
		g_shared_status->active_sleep_control = __gpufreq_active_sleep_ctrl_enable();
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->shader_present = g_shader_present;
		g_shared_status->aging_load = g_aging_load;
		g_shared_status->aging_margin = g_aging_margin;
		g_shared_status->avs_enable = g_avs_enable;
		g_shared_status->avs_margin = g_avs_margin;
		g_shared_status->ptp_version = g_ptp_version;
		g_shared_status->gpm1_mode = g_gpm1_2_mode;
		g_shared_status->gpm3_mode = g_gpm3_0_mode;
		g_shared_status->test_mode = g_test_mode;
		g_shared_status->temper_comp_mode = GPUFREQ_TEMPER_COMP_ENABLE;
		g_shared_status->temperature = g_temperature;
		g_shared_status->temper_comp_norm_gpu = g_temper_comp_vgpu;
		g_shared_status->temper_comp_norm_stack = g_temper_comp_vstack;
		g_shared_status->dual_buck = true;
		g_shared_status->segment_id = g_stack.segment_id;
		g_shared_status->reg_top_delsel.addr = 0x13FBF084;
		g_shared_status->reg_stack_delsel.addr = 0x13E90080;
		g_shared_status->dbg_version = GPUFREQ_DEBUG_VERSION;
		__gpufreq_update_shared_status_opp_table();
		__gpufreq_update_shared_status_adj_table();
		__gpufreq_update_shared_status_gpm3_table();
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: MSSV test function */
int __gpufreq_mssv_commit(unsigned int target, unsigned int val)
{
#if GPUFREQ_MSSV_TEST_MODE
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	ret = __gpufreq_active_sleep_control(GPU_PWR_ON);
	if (ret < 0)
		goto done;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	mutex_lock(&gpufreq_lock);

	switch (target) {
	case TARGET_MSSV_FGPU:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_16_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_freq_scale_gpu(g_gpu.cur_freq, val);
		break;
	case TARGET_MSSV_VGPU:
		if (val > VGPU_MAX_VOLT || val < VGPU_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else {
			ret = __gpufreq_volt_scale_gpu(g_gpu.cur_volt, val);
			g_gpu.cur_vsram = __gpufreq_get_dreq_vsram_gpu();
		}
		break;
	case TARGET_MSSV_FSTACK:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_16_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_freq_scale_stack(g_stack.cur_freq, val);
		break;
	case TARGET_MSSV_VSTACK:
		if (val > VSTACK_MAX_VOLT || val < VSTACK_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else {
			ret = __gpufreq_volt_scale_stack(g_stack.cur_volt, val);
			g_stack.cur_vsram = __gpufreq_get_dreq_vsram_stack();
		}
		break;
	case TARGET_MSSV_TOP_DELSEL:
		if (val == 1 || val == 0) {
			DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV_TOP, val);
			ret = GPUFREQ_SUCCESS;
		} else
			ret = GPUFREQ_EINVAL;
		break;
	case TARGET_MSSV_STACK_DELSEL:
		if (val == 1 || val == 0) {
			DRV_WriteReg32(MFG_CG_SRAM_FUL_SEL_ULV, val);
			ret = GPUFREQ_SUCCESS;
		} else
			ret = GPUFREQ_EINVAL;
		break;
	default:
		ret = GPUFREQ_EINVAL;
		break;
	}

	if (ret)
		GPUFREQ_LOGE("invalid MSSV cmd, target: %d, val: %d", target, val);
	else {
		if (g_shared_status) {
			g_shared_status->cur_fgpu = g_gpu.cur_freq;
			g_shared_status->cur_vgpu = g_gpu.cur_volt;
			g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
			g_shared_status->cur_fstack = g_stack.cur_freq;
			g_shared_status->cur_vstack = g_stack.cur_volt;
			g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
			g_shared_status->reg_top_delsel.addr = 0x13FBF084;
			g_shared_status->reg_top_delsel.val = DRV_Reg32(MFG_SRAM_FUL_SEL_ULV_TOP);
			g_shared_status->reg_stack_delsel.addr = 0x13E90080;
			g_shared_status->reg_stack_delsel.val = DRV_Reg32(MFG_CG_SRAM_FUL_SEL_ULV);
		}
	}

	mutex_unlock(&gpufreq_lock);

#if GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
	__gpufreq_active_sleep_control(GPU_PWR_OFF);
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	return ret;
#else
	GPUFREQ_UNREFERENCED(target);
	GPUFREQ_UNREFERENCED(val);

	return GPUFREQ_EINVAL;
#endif /* GPUFREQ_MSSV_TEST_MODE */
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (!gpufreq_dev) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		return;
	}
	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}
	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		return;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		return;
	}
	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		return;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		return;
	}
	/* 0x13FA0400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc0");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC0");
		return;
	}
	g_mfg_pll_sc0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc0_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC0: 0x%llx", res->start);
		return;
	}
	/* 0x13FA0800 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc1");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC1");
		return;
	}
	g_mfg_pll_sc1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc1_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC1: 0x%llx", res->start);
		return;
	}
	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		return;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		return;
	}
	/* 0x13A00000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SMMU");
		return;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		return;
	}
	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		return;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		return;
	}
	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		return;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_topckgen_base) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		return;
	}

	GPUFREQ_LOGI("[SPM] %s=0x%08x, %s=0x%08x",
		"SPM2GPUPM_CON", DRV_Reg32(SPM_SPM2GPUPM_CON),
		"MFG0_PWR_CON", DRV_Reg32(SPM_MFG0_PWR_CON));
	GPUFREQ_LOGI("[MFG] %s=0x%08lx, %s=0x%08x, %s=0x%08x",
		"MFG_0_22_37_PWR_STATUS", MFG_0_22_37_PWR_STATUS,
		"MFG1_PWR_CON", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MFG_DEFAULT_DELSEL", DRV_Reg32(MFG_DEFAULT_DELSEL_00));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"GTOP_DREQ", DRV_Reg32(MFG_RPC_GTOP_DREQ_CFG),
		"SMMU_CR0", DRV_Reg32(MFG_SMMU_CR0),
		"SMMU_GBPA", DRV_Reg32(MFG_SMMU_GBPA));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_TOP", DRV_Reg32(MFG_RPC_BRISKET_TOP_AO_CFG_0),
		"BRISKET_ST0", DRV_Reg32(MFG_RPC_BRISKET_ST0_AO_CFG_0),
		"BRISKET_ST1", DRV_Reg32(MFG_RPC_BRISKET_ST1_AO_CFG_0),
		"BRISKET_ST3", DRV_Reg32(MFG_RPC_BRISKET_ST3_AO_CFG_0));
	GPUFREQ_LOGI("[MFG] %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"BRISKET_ST4", DRV_Reg32(MFG_RPC_BRISKET_ST4_AO_CFG_0),
		"BRISKET_ST5", DRV_Reg32(MFG_RPC_BRISKET_ST5_AO_CFG_0),
		"BRISKET_ST6", DRV_Reg32(MFG_RPC_BRISKET_ST6_AO_CFG_0));
	GPUFREQ_LOGI("[TOP] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_CON0),
		"CON1", __gpufreq_get_pll_fgpu(),
		"FMETER", __gpufreq_get_fmeter_fgpu(),
		"SEL", DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & MFG_TOP_SEL_BIT,
		"REF_SEL", DRV_Reg32(TOPCK_CLK_CFG_5_STA) & MFG_REF_SEL_BIT);
	GPUFREQ_LOGI("[SC0] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON0", DRV_Reg32(MFG_PLL_SC0_CON0),
		"CON1", __gpufreq_get_pll_fstack0(),
		"FMETER", __gpufreq_get_fmeter_fstack0(),
		"SEL", DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & MFG_SC0_SEL_BIT);
	GPUFREQ_LOGI("[SC1] %s=0x%08x, %s=%d, %s=%d, %s=0x%08lx",
		"PLL_CON1", DRV_Reg32(MFG_PLL_SC1_CON0),
		"CON1", __gpufreq_get_pll_fstack1(),
		"FMETER", __gpufreq_get_fmeter_fstack1(),
		"SEL", DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & MFG_SC1_SEL_BIT);

	GPUFREQ_LOGI("[GPU] %s=0x%08x",
		"MALI_GPU_ID", DRV_Reg32(MALI_GPU_ID));
}

static void __gpufreq_update_shared_status_opp_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* working table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_gpu.opp_num;
	memcpy(g_shared_status->working_table_gpu, g_gpu.working_table, copy_size);
	/* signed table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_gpu.signed_opp_num;
	memcpy(g_shared_status->signed_table_gpu, g_gpu.signed_table, copy_size);

	/* STACK */
	/* working table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_stack.opp_num;
	memcpy(g_shared_status->working_table_stack, g_stack.working_table, copy_size);
	/* signed table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_stack.signed_opp_num;
	memcpy(g_shared_status->signed_table_stack, g_stack.signed_table, copy_size);
}

static void __gpufreq_update_shared_status_adj_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* aging table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->aging_table_gpu, g_gpu_aging_table, copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->avs_table_gpu, g_gpu_avs_table, copy_size);

	/* STACK */
	/* aging table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_STACK_SIGNED_IDX;
	memcpy(g_shared_status->aging_table_stack, g_stack_aging_table, copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_STACK_SIGNED_IDX;
	memcpy(g_shared_status->avs_table_stack, g_stack_avs_table, copy_size);
}

static void __gpufreq_update_shared_status_gpm3_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	copy_size = sizeof(struct gpufreq_gpm3_info) * NUM_GPM3_LIMIT;
	memcpy(g_shared_status->gpm3_table, g_gpm3_table, copy_size);
}

/* API: set/reset DVFS state with lock */
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state)
{
	mutex_lock(&gpufreq_lock);

	if (set)
		g_dvfs_state |= state;
	else
		g_dvfs_state &= ~state;

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->dvfs_state = g_dvfs_state;

	mutex_unlock(&gpufreq_lock);
}

/* API: handle DEVAPC violation */
static void __gpufreq_devapc_vio_handler(void)
{
#if GPUFREQ_DEVAPC_CHECK_ENABLE
#if GPUFREQ_HWDCM_ENABLE
	/* disable HWDCM */
	GPUFREQ_LOGE("disable HWDCM for MFG DEVAPC VIO");
	/* MFG_GLOBAL_CON 0x13FBF0B0 [8] GPU_SOCIF_MST_FREE_RUN = 1'b1 */
	DRV_WriteReg32(MFG_GLOBAL_CON, DRV_Reg32(MFG_GLOBAL_CON) | BIT(8));
	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b1 */
	DRV_WriteReg32(MFG_RPC_AO_CLK_CFG, DRV_Reg32(MFG_RPC_AO_CLK_CFG) | BIT(0));
#endif /* GPUFREQ_HWDCM_ENABLE */
#endif /* GPUFREQ_DEVAPC_CHECK_ENABLE */
}

/* API: apply/restore Vaging to working table of GPU */
static void __gpufreq_set_margin_mode(unsigned int mode)
{
	/* update volt margin */
	__gpufreq_apply_restore_margin(TARGET_GPU, mode);
	__gpufreq_apply_restore_margin(TARGET_STACK, mode);

	/* update power info to working table */
	__gpufreq_measure_power();

	/* update DVFS constraint */
	__gpufreq_update_springboard();

	/* update GPM3.0 setting */
	__gpufreq_init_gpm3_0_table();

	/* update current status to shared memory */
	if (g_shared_status)
		__gpufreq_update_shared_status_opp_table();
}

/* API: enable/disable GPM */
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int mode)
{
	if (version == 1)
		g_gpm1_2_mode = mode;
	if (version == 3) {
		g_gpm3_0_mode = mode;
		/* re-calculate GPM3.0 setting */
		__gpufreq_init_gpm3_0_table();
	}

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->gpm1_mode = g_gpm1_2_mode;
		g_shared_status->gpm3_mode = g_gpm3_0_mode;
	}
}

/* API: set MCU ETM clock every time MFG1 power-on to enable MCU trace */
static void __gpufreq_set_mcu_etm_clock(unsigned int mode)
{
	/* ETM: Embedded Trace Macrocell */
	if (g_test_mode) {
		if (mode == FEAT_ENABLE)
			g_mcuetm_clk_enable = true;
		else if (mode == FEAT_DISABLE)
			g_mcuetm_clk_enable = false;
	}
}

/* API: apply (enable) / restore (disable) margin */
static void __gpufreq_apply_restore_margin(enum gpufreq_target target, unsigned int mode)
{
	struct gpufreq_opp_info *working_table = NULL;
	struct gpufreq_opp_info *signed_table = NULL;
	int working_opp_num = 0, signed_opp_num = 0, segment_upbound = 0, i = 0;

	if (target == TARGET_STACK) {
		working_table = g_stack.working_table;
		signed_table = g_stack.signed_table;
		working_opp_num = g_stack.opp_num;
		signed_opp_num = g_stack.signed_opp_num;
		segment_upbound = g_stack.segment_upbound;
	} else {
		working_table = g_gpu.working_table;
		signed_table = g_gpu.signed_table;
		working_opp_num = g_gpu.opp_num;
		signed_opp_num = g_gpu.signed_opp_num;
		segment_upbound = g_gpu.segment_upbound;
	}

	/* update margin to signed table */
	for (i = 0; i < signed_opp_num; i++) {
		if (mode == FEAT_DISABLE)
			signed_table[i].volt += signed_table[i].margin;
		else if (mode == FEAT_ENABLE)
			signed_table[i].volt -= signed_table[i].margin;
		signed_table[i].vsram = __gpufreq_get_vsram_by_vlogic(signed_table[i].volt);
	}

	for (i = 0; i < working_opp_num; i++) {
		working_table[i].volt = signed_table[segment_upbound + i].volt;
		working_table[i].vsram = signed_table[segment_upbound + i].vsram;

		GPUFREQ_LOGD("Margin mode: %d, %s[%d] Volt: %d, Vsram: %d",
			mode, target == TARGET_STACK ? "STACK" : "GPU",
			i, working_table[i].volt, working_table[i].vsram);
	}
}

static void __gpufreq_set_temper_compensation(void)
{
#if GPUFREQ_TEMPER_COMP_ENABLE
	if (g_temperature != TEMPERATURE_DEFAULT) {
		if (g_avs_margin && g_temperature >= 10 && g_temperature < 25) {
			g_temper_comp_vgpu = TEMPER_COMP_10_25_VOLT;
			g_temper_comp_vstack = TEMPER_COMP_10_25_VOLT;
		} else if (g_avs_margin && g_temperature < 10) {
			g_temper_comp_vgpu = TEMPER_COMP_10_VOLT;
			g_temper_comp_vstack = TEMPER_COMP_10_VOLT;
		} else {
			g_temper_comp_vgpu = TEMPER_COMP_DEFAULT_VOLT;
			g_temper_comp_vstack = TEMPER_COMP_DEFAULT_VOLT;
		}
	}

	GPUFREQ_LOGD("current temper: %d, vgpu_comp: %d, vstack_comp: %d",
		g_temperature, g_temper_comp_vgpu, g_temper_comp_vstack);
#endif /* GPUFREQ_TEMPER_COMP_ENABLE */
}

static void __gpufreq_set_gpm3_0_limit(void)
{
#if GPUFREQ_GPM3_0_ENABLE
	int i = 0, gpm3_num = NUM_GPM3_LIMIT;
	int ceiling = 0;

	if (g_gpm3_0_mode) {
		for (i = 0; i < gpm3_num; i++) {
			if ((g_temperature + GPM3_TEMPER_OFFSET) <= g_gpm3_table[i].temper)
				break;
		}
		/* check if temepr is out-of-bound */
		i = i < gpm3_num ? i : (gpm3_num - 1);
		ceiling = g_gpm3_table[i].ceiling;
	} else
		ceiling = GPUPPM_RESET_IDX;

	gpuppm_set_limit(TARGET_STACK, LIMIT_GPM3, ceiling, GPUPPM_KEEP_IDX, false);

	GPUFREQ_LOGD("current temper: %d, ceiling: %d", g_temperature, ceiling);
#endif /* GPUFREQ_GPM3_0_ENABLE */
}

/* API: let OPP state ready before GPU start to work */
static void __gpufreq_restore_last_oppidx(void)
{
	int oppidx_gpu = -1, oppidx_stack = -1;

	if (g_gpufreq_ready) {
		if (ged_get_last_commit_top_idx_fp && ged_get_last_commit_stack_idx_fp) {
			oppidx_gpu = (int)ged_get_last_commit_top_idx_fp();
			oppidx_stack = (int)ged_get_last_commit_stack_idx_fp();
		} else {
			oppidx_gpu = g_gpu.cur_oppidx;
			oppidx_stack = g_stack.cur_oppidx;
		}
		gpufreq_dual_commit(oppidx_gpu, oppidx_stack);
	}
}

/* API: decide DVFS order of freq and volt */
static int __gpufreq_generic_scale(
	unsigned int fgpu_old, unsigned int fgpu_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int fstack_old, unsigned int fstack_new,
	unsigned int vstack_old, unsigned int vstack_new,
	unsigned int vsram_gpu, unsigned int vsram_stack)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START(
		"fgpu(%d->%d), vgpu(%d->%d), vsram_gpu(%d), fstack(%d->%d), vstack(%d->%d), vsram_stack(%d)",
		fgpu_old, fgpu_new, vgpu_old, vgpu_new,
		fstack_old, fstack_new, vstack_old, vstack_new,
		vsram_gpu, vsram_stack);

	/* Fgpu scale-up and Fstack scale-up */
	if (fgpu_new > fgpu_old && fstack_new > fstack_old) {
		__gpufreq_footprint_dvfs_step(0x10);
		/* Vgpu + Vstack + Vsram */
		ret = __gpufreq_volt_scale(vgpu_old, vgpu_new,
			vstack_old, vstack_new, vsram_gpu, vsram_stack);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x11);
		/* Fstack */
		ret = __gpufreq_freq_scale_stack(fstack_old, fstack_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x12);
		/* Fgpu */
		ret = __gpufreq_freq_scale_gpu(fgpu_old, fgpu_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x13);
	/* Fgpu scale-down and Fstack scale-up */
	} else if (fgpu_new < fgpu_old && fstack_new > fstack_old) {
		__gpufreq_footprint_dvfs_step(0x14);
		/* Fgpu */
		ret = __gpufreq_freq_scale_gpu(fgpu_old, fgpu_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x15);
		/* Vgpu + Vstack + Vsram */
		ret = __gpufreq_volt_scale(vgpu_old, vgpu_new,
			vstack_old, vstack_new, vsram_gpu, vsram_stack);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x16);
		/* Fstack */
		ret = __gpufreq_freq_scale_stack(fstack_old, fstack_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x17);
	/* Fgpu scale-up and Fstack scale-down */
	} else if (fgpu_new > fgpu_old && fstack_new < fstack_old) {
		__gpufreq_footprint_dvfs_step(0x18);
		/* Fstack */
		ret = __gpufreq_freq_scale_stack(fstack_old, fstack_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x19);
		/* Vgpu + Vstack + Vsram */
		ret = __gpufreq_volt_scale(vgpu_old, vgpu_new,
			vstack_old, vstack_new, vsram_gpu, vsram_stack);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x1A);
		/* Fgpu */
		ret = __gpufreq_freq_scale_gpu(fgpu_old, fgpu_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x1B);
	/* Fgpu scale-down and Fstack scale-down */
	} else {
		__gpufreq_footprint_dvfs_step(0x1C);
		/* Fgpu */
		ret = __gpufreq_freq_scale_gpu(fgpu_old, fgpu_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x1D);
		/* Fstack */
		ret = __gpufreq_freq_scale_stack(fstack_old, fstack_new);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x1E);
		/* Vgpu + Vstack + Vsram */
		ret = __gpufreq_volt_scale(vgpu_old, vgpu_new,
			vstack_old, vstack_new, vsram_gpu, vsram_stack);
		if (ret)
			goto done;
		__gpufreq_footprint_dvfs_step(0x1F);
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: fix DVFS by given STACK freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	int target_oppidx = 0;
	unsigned int target_fgpu = 0, target_vgpu = 0;
	unsigned int target_fstack = 0, target_vstack = 0;

	/* get Fgpu and Vgpu by Vstack */
	target_oppidx = __gpufreq_get_idx_by_vstack(target_volt);
	target_fgpu = g_gpu.working_table[target_oppidx].freq;
	target_vgpu = g_gpu.working_table[target_oppidx].volt;
	target_fstack = target_freq;
	target_vstack = target_volt;

	return __gpufreq_custom_commit_dual(target_fgpu, target_vgpu,
		target_fstack, target_vstack, key);
}


/*
 * API: fix DVFS by given GPU and STACK freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_dual(unsigned int target_fgpu, unsigned int target_vgpu,
	unsigned int target_fstack, unsigned int target_vstack, enum gpufreq_dvfs_state key)
{
	int ret = GPUFREQ_SUCCESS;
	int target_oppidx_gpu = 0, target_oppidx_stack = 0;
	unsigned int cur_fgpu = 0, cur_vgpu = 0;
	unsigned int cur_fstack = 0, cur_vstack = 0;
	unsigned int cur_vsram_gpu = 0, target_vsram_gpu = 0;
	unsigned int cur_vsram_stack = 0, target_vsram_stack = 0;

	GPUFREQ_TRACE_START(
		"target_fgpu=%d, target_vgpu=%d, target_fstack=%d, target_vstack=%d, key=%d",
		target_fgpu, target_vgpu, target_fstack, target_vstack, key);

	mutex_lock(&gpufreq_lock);
	__gpufreq_footprint_dvfs_step(0x04);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* prepare OPP setting */
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram_gpu = g_gpu.cur_vsram;
	cur_fstack = g_stack.cur_freq;
	cur_vstack = g_stack.cur_volt;
	cur_vsram_stack = g_stack.cur_vsram;

	target_vsram_gpu = __gpufreq_get_vsram_by_vlogic(target_vgpu);
	target_vsram_stack = __gpufreq_get_vsram_by_vlogic(target_vstack);
	target_oppidx_gpu = __gpufreq_get_idx_by_vgpu(target_vgpu);
	target_oppidx_stack = __gpufreq_get_idx_by_vstack(target_vstack);

	__gpufreq_footprint_oppidx(target_oppidx_stack);
	GPUFREQ_LOGD(
		"commit GPU F(%d->%d)/V(%d->%d)/VSRAM(%d), STACK F(%d->%d)/V(%d->%d)/VSRAM(%d)",
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu, target_vsram_gpu,
		cur_fstack, target_fstack, cur_vstack, target_vstack, target_vsram_stack);

	ret = __gpufreq_generic_scale(cur_fgpu, target_fgpu, cur_vgpu, target_vgpu,
		cur_fstack, target_fstack, cur_vstack, target_vstack,
		target_vsram_gpu, target_vsram_stack);
	if (ret) {
		GPUFREQ_LOGE(
			"fail to commit GPU F(%d)/V(%d)/VSRAM(%d), STACK F(%d)/V(%d)/VSRAM(%d)",
			target_fgpu, target_vgpu, target_vsram_gpu,
			target_fstack, target_vstack, target_vsram_stack);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx_gpu;
	g_stack.cur_oppidx = target_oppidx_stack;

	__gpufreq_footprint_dvfs_step(0x05);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_oppidx_stack = g_stack.cur_oppidx;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->cur_vstack = g_stack.cur_volt;
		g_shared_status->cur_vsram_stack = g_stack.cur_vsram;
		g_shared_status->cur_power_stack = g_stack.working_table[g_stack.cur_oppidx].power;
	}

done_unlock:
	__gpufreq_footprint_dvfs_step(0x06);
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_clksrc_ctrl(enum gpufreq_target target, enum gpufreq_clk_src clksrc)
{
	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	if (target == TARGET_STACK) {
		if (clksrc == CLOCK_MAIN) {
			DRV_WriteReg32(MFG_RPC_MFG_CK_FAST_REF_SEL,
				DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) | MFG_SC0_SEL_BIT);
			DRV_WriteReg32(MFG_RPC_MFG_CK_FAST_REF_SEL,
				DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) | MFG_SC1_SEL_BIT);
			g_stack.cur_freq = __gpufreq_get_pll_fstack0();
		} else if (clksrc == CLOCK_SUB) {
			DRV_WriteReg32(MFG_RPC_MFG_CK_FAST_REF_SEL,
				DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & ~MFG_SC0_SEL_BIT);
			DRV_WriteReg32(MFG_RPC_MFG_CK_FAST_REF_SEL,
				DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & ~MFG_SC1_SEL_BIT);
			g_stack.cur_freq = __gpufreq_get_sub_fstack();
		}
	} else if (target == TARGET_GPU) {
		if (clksrc == CLOCK_MAIN) {
			DRV_WriteReg32(MFG_RPC_MFG_CK_FAST_REF_SEL,
				DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) | MFG_TOP_SEL_BIT);
			g_gpu.cur_freq = __gpufreq_get_pll_fgpu();
		} else if (clksrc == CLOCK_SUB) {
			DRV_WriteReg32(MFG_RPC_MFG_CK_FAST_REF_SEL,
				DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & ~MFG_TOP_SEL_BIT);
			g_gpu.cur_freq = __gpufreq_get_sub_fgpu();
		}
	}

	GPUFREQ_LOGD("MFG_RPC_MFG_CK_FAST_REF_SEL: 0x%08x", DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL));

	GPUFREQ_TRACE_END();
}

/*
 * API: calculate pcw for setting CON1
 * Fin is 26 MHz
 * VCO Frequency = Fin * N_INFO
 * MFGPLL output Frequency = VCO Frequency / POSDIV
 * N_INFO = MFGPLL output Frequency * POSDIV / FIN
 * N_INFO[21:14] = FLOOR(N_INFO, 8)
 */
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv)
{
	/*
	 * MFGPLL VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 * MFGPLL range: 125MHz - 3.8GHz,
	 * | VCO MAX | VCO MIN | POSDIV | PLL OUT MAX | PLL OUT MIN |
	 * |  3800   |  1500   |    1   |   3800MHz   |   1500MHz   |
	 * |  3800   |  1500   |    2   |   1900MHz   |    750MHz   |
	 * |  3800   |  1500   |    4   |    950MHz   |    375MHz   |
	 * |  3800   |  1500   |    8   |    475MHz   |  187.5MHz   |
	 * |  3800   |  2000   |   16   |  237.5MHz   |    125MHz   |
	 */
	unsigned long long pcw = 0;

	if ((freq >= POSDIV_16_MAX_FREQ) && (freq <= POSDIV_2_MAX_FREQ))
		pcw = (((unsigned long long)freq * (1 << posdiv)) << DDS_SHIFT) / MFGPLL_FIN / 1000;
	else
		__gpufreq_abort("out of range Freq: %d", freq);

	GPUFREQ_LOGD("target freq: %d, posdiv: %d, pcw: 0x%llx", freq, posdiv, pcw);

	return (unsigned int)pcw;
}

static enum gpufreq_posdiv __gpufreq_get_pll_posdiv_gpu(void)
{
	unsigned int con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = DRV_Reg32(MFG_PLL_CON1);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_pll_posdiv_stack0(void)
{
	unsigned int con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = DRV_Reg32(MFG_PLL_SC0_CON1);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_pll_posdiv_stack1(void)
{
	unsigned int con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = DRV_Reg32(MFG_PLL_SC1_CON1);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq)
{
	if (freq > POSDIV_4_MAX_FREQ)
		return POSDIV_POWER_2;
	else if (freq > POSDIV_8_MAX_FREQ)
		return POSDIV_POWER_4;
	else if (freq > POSDIV_16_MAX_FREQ)
		return POSDIV_POWER_8;
	else if (freq >= POSDIV_16_MIN_FREQ)
		return POSDIV_POWER_16;

	/* not found */
	__gpufreq_abort("invalid freq: %d", freq);
	return POSDIV_POWER_16;
}

/* API: scale Freq of GPU via PLL CON1 or FHCTL */
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int con1 = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);
	GPUFREQ_LOGD("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);
	__gpufreq_footprint_dvfs_step(0x20);

	if (freq_new == freq_old)
		goto done;

	/*
	 * MFG_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFG_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFG_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_pll_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		__gpufreq_abort("invalid PCW: 0x%x", pcw);
		goto done;
	}

	__gpufreq_footprint_dvfs_step(0x21);
	/* 1. switch to parking clk source */
	__gpufreq_clksrc_ctrl(TARGET_GPU, CLOCK_SUB);
	/* 2. compute CON1 with PCW and POSDIV */
	con1 = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	DRV_WriteReg32(MFG_PLL_CON1, con1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	__gpufreq_clksrc_ctrl(TARGET_GPU, CLOCK_MAIN);

	__gpufreq_footprint_dvfs_step(0x22);
	g_gpu.cur_freq = __gpufreq_get_pll_fgpu();

	if (g_gpu.cur_freq != freq_new)
		__gpufreq_abort("inconsistent cur_freq: %d, target_freq: %d",
			g_gpu.cur_freq, freq_new);

	GPUFREQ_LOGD("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, con1);

	/* notify gpu freq change to DDK  */
	mtk_notify_gpu_freq_change(0, freq_new);

done:
	__gpufreq_footprint_dvfs_step(0x23);
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Freq of STACK via PLL CON1 or FHCTL */
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv cur_sc1_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int con1 = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);
	GPUFREQ_LOGD("begin to scale Fstack: (%d->%d)", freq_old, freq_new);
	__gpufreq_footprint_dvfs_step(0x24);

	if (freq_new == freq_old)
		goto done;

	/*
	 * MFG_PLL_SC0_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFG_PLL_SC0_CON1[26:24]: MFGPLL_POSDIV
	 * MFG_PLL_SC0_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	if (__gpufreq_get_pll_posdiv_stack0() != __gpufreq_get_pll_posdiv_stack1())
		__gpufreq_abort("inconsistent MFG_PLL_SC0_CON1: 0x%08x, MFG_PLL_SC1_CON1: 0x%08x",
			DRV_Reg32(MFG_PLL_SC0_CON1), DRV_Reg32(MFG_PLL_SC1_CON1));
	else
		cur_posdiv = __gpufreq_get_pll_posdiv_stack0();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (!pcw) {
		__gpufreq_abort("invalid PCW: 0x%x", pcw);
		goto done;
	}

	__gpufreq_footprint_dvfs_step(0x25);
	/* 1. switch to parking clk source */
	__gpufreq_clksrc_ctrl(TARGET_STACK, CLOCK_SUB);
	/* 2. compute CON1 with PCW and POSDIV */
	con1 = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3-1. change PCW and POSDIV by writing CON1 */
	DRV_WriteReg32(MFG_PLL_SC0_CON1, con1);
	/* 3-2. change PCW and POSDIV by writing CON1 */
	DRV_WriteReg32(MFG_PLL_SC1_CON1, con1);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	__gpufreq_clksrc_ctrl(TARGET_STACK, CLOCK_MAIN);

	__gpufreq_footprint_dvfs_step(0x26);
	if (__gpufreq_get_pll_fstack0() != __gpufreq_get_pll_fstack1())
		__gpufreq_abort("inconsistent MFG_PLL_SC0_CON1: 0x%08x, MFG_PLL_SC1_CON1: 0x%08x",
			DRV_Reg32(MFG_PLL_SC0_CON1), DRV_Reg32(MFG_PLL_SC1_CON1));
	else
		g_stack.cur_freq = __gpufreq_get_pll_fstack0();

	if (g_stack.cur_freq != freq_new)
		__gpufreq_abort("inconsistent cur_freq: %d, target_freq: %d",
			g_stack.cur_freq, freq_new);

	GPUFREQ_LOGD("Fstack: %d, PCW: 0x%x, CON1: 0x%08x", g_stack.cur_freq, pcw, con1);

	/* notify stack freq change to DDK */
	mtk_notify_gpu_freq_change(1, freq_new);

done:
	__gpufreq_footprint_dvfs_step(0x27);
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_settle_time_vgpu(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6373_VBUCK3][VGPU]
	 * DVFS Rising : (deltaV / 12.5mV) * 1.1 + 3.85us + 2us
	 * DVFS Falling: (deltaV / 12.5mV) * 1.1 + 3.85us + 2us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP)
		/* rising */
		t_settle = (deltaV * 11 / 1250 / 10) + 4 + 2;
	else if (direct == SCALE_DOWN)
		/* falling */
		t_settle = (deltaV * 11 / 1250 / 10) + 4 + 2;

	return t_settle; /* us */
}

static unsigned int __gpufreq_settle_time_vstack(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6316_VBUCK1_2_3_4][VGPUSTACK]
	 * DVFS Rising : (deltaV / 25mV) * 1.1 + 3.85us + 2us
	 * DVFS Falling: (deltaV / 6.25mV) * 1.1 + 3.85us + 2us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP)
		/* rising */
		t_settle = (deltaV * 11 / 2500 / 10) + 4 + 2;
	else if (direct == SCALE_DOWN)
		/* falling */
		t_settle = (deltaV * 11 / 625 / 10) + 4 + 2;

	return t_settle; /* us */
}

/* API: scale Volt of GPU via Regulator */
static int __gpufreq_volt_scale_gpu(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d", volt_old, volt_new);
	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d)", volt_old, volt_new);
	__gpufreq_footprint_dvfs_step(0x28);

	if (volt_new == volt_old)
		goto done;
	else if (volt_new > volt_old)
		t_settle = __gpufreq_settle_time_vgpu(SCALE_UP, (volt_new - volt_old));
	else if (volt_new < volt_old)
		t_settle = __gpufreq_settle_time_vgpu(SCALE_DOWN, (volt_old - volt_new));

	ret = regulator_set_voltage(g_pmic->reg_vgpu, volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
	if (ret) {
		__gpufreq_abort("fail to set regulator volt: %d (%d)", volt_new, ret);
		goto done;
	}
	udelay(t_settle);

	__gpufreq_footprint_dvfs_step(0x29);
	g_gpu.cur_volt = __gpufreq_get_pmic_vgpu();

	if (g_gpu.cur_volt != volt_new)
		__gpufreq_abort("inconsistent cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, volt_new);

	GPUFREQ_LOGD("Vgpu: %d, udelay: %d", g_gpu.cur_volt, t_settle);

done:
	__gpufreq_footprint_dvfs_step(0x2A);
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Volt of STACK via Regulator */
static int __gpufreq_volt_scale_stack(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int t_settle = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("volt_old=%d, volt_new=%d", volt_old, volt_new);
	GPUFREQ_LOGD("begin to scale Vstack: (%d->%d)", volt_old, volt_new);
	__gpufreq_footprint_dvfs_step(0x2B);

	if (volt_new == volt_old)
		goto done;
	else if (volt_new > volt_old)
		t_settle = __gpufreq_settle_time_vstack(SCALE_UP, (volt_new - volt_old));
	else if (volt_new < volt_old)
		t_settle = __gpufreq_settle_time_vstack(SCALE_DOWN, (volt_old - volt_new));

	ret = regulator_set_voltage(g_pmic->reg_vstack, volt_new * 10, VSTACK_MAX_VOLT * 10 + 125);
	if (ret) {
		__gpufreq_abort("fail to set regulator volt: %d (%d)", volt_new, ret);
		goto done;
	}
	udelay(t_settle);

	__gpufreq_footprint_dvfs_step(0x2C);
	g_stack.cur_volt = __gpufreq_get_pmic_vstack();

	if (g_stack.cur_volt != volt_new)
		__gpufreq_abort("inconsistent cur_volt: %d, target_volt: %d",
			g_stack.cur_volt, volt_new);

	/* additionally update current Vstack due to PTP3 */
	if (g_shared_status)
		g_shared_status->cur_vstack = g_stack.cur_volt;

	GPUFREQ_LOGD("Vstack: %d, udelay: %d", g_stack.cur_volt, t_settle);

done:
	__gpufreq_footprint_dvfs_step(0x2D);
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: check CVCC is set to correct Buck source */
static void __gpufreq_volt_check_sram(unsigned int vsram_gpu, unsigned int vsram_stack)
{
#if GPUFREQ_DREQ_AUTO_ENABLE
	unsigned int dreq_ack = 0, l2_status = 0;

	/* GPU */
	dreq_ack = DRV_Reg32(MFG_DREQ_TOP_DBG_CON_0);
	if (dreq_ack & DREQ_TOP_VLOG_ACK)
		g_gpu.cur_vsram = __gpufreq_get_pmic_vgpu();
	else if (dreq_ack & DREQ_TOP_VSRAM_ACK)
		g_gpu.cur_vsram = __gpufreq_get_pmic_vsram();
	else
		__gpufreq_abort("incorrect DREQ_TOP: 0x%08x", dreq_ack);

	/* STACK, only available when L2 is power-on */
	l2_status = DRV_Reg32(MALI_L2_READY_LO);
	if (l2_status) {
		/* only check STACK0 */
		dreq_ack = DRV_Reg32(MFG_CG_DREQ_CG_DBG_CON_0);
		if (dreq_ack & DREQ_ST0_VLOG_ACK)
			g_stack.cur_vsram = __gpufreq_get_pmic_vstack();
		else if (dreq_ack & DREQ_ST0_VSRAM_ACK)
			g_stack.cur_vsram = __gpufreq_get_pmic_vsram();
		else
			g_stack.cur_vsram = vsram_stack;
	} else
		g_stack.cur_vsram = vsram_stack;

	GPUFREQ_LOGD("vsram_gpu: %d, vsram_stack: %d, DREQ_TOP: 0x%x, DREQ_STACK: 0x%x, L2: 0x%x",
		g_gpu.cur_vsram, g_stack.cur_vsram, DRV_Reg32(MFG_DREQ_TOP_DBG_CON_0),
		DRV_Reg32(MFG_CG_DREQ_CG_DBG_CON_0), DRV_Reg32(MALI_L2_READY_LO));
#else
	GPUFREQ_UNREFERENCED(vsram_gpu);
	GPUFREQ_UNREFERENCED(vsram_stack);
	g_gpu.cur_vsram = __gpufreq_get_pmic_vsram();
	g_stack.cur_vsram = __gpufreq_get_pmic_vsram();
#endif /* GPUFREQ_DREQ_AUTO_ENABLE */
}

/* SW manually set Vmeter volt to config HW_DELSEL */
static void __gpufreq_sw_vmeter_config(enum gpufreq_opp_direct direct,
	unsigned int cur_vgpu, unsigned int cur_vstack)
{
#if GPUFREQ_SW_VMETER_ENABLE
	unsigned int delsel_vgpu = 0, delsel_vstack = 0;

	delsel_vgpu = g_gpu.signed_table[PARKING_TOP_DELSEL].volt;
	delsel_vstack = g_stack.signed_table[PARKING_BINNING].volt;

	/* high volt use DELSEL=0 */
	if (direct == SCALE_UP) {
		if (cur_vgpu == delsel_vgpu) {
			g_vmeter_gpu_val = ((SW_VMETER_DELSEL_0_VOLT / 100) << 2) | BIT(0);
			DRV_WriteReg32(BRISKET_TOP_VOLTAGEEXT, g_vmeter_gpu_val);
		}
		if (cur_vstack == delsel_vstack) {
			g_vmeter_stack_val = ((SW_VMETER_DELSEL_0_VOLT / 100) << 2) | BIT(0);
			DRV_WriteReg32(MFG_BRCAST_PROG_DATA_114, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST0_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST1_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST3_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST4_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST5_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST6_VOLTAGEEXT, g_vmeter_stack_val);
		}
	/* low volt use DELSEL=1 */
	} else if (direct == SCALE_DOWN) {
		if (cur_vgpu == delsel_vgpu) {
			g_vmeter_gpu_val = ((SW_VMETER_DELSEL_1_VOLT / 100) << 2) | BIT(0);
			DRV_WriteReg32(BRISKET_TOP_VOLTAGEEXT, g_vmeter_gpu_val);
		}
		if (cur_vstack == delsel_vstack) {
			g_vmeter_stack_val = ((SW_VMETER_DELSEL_1_VOLT / 100) << 2) | BIT(0);
			DRV_WriteReg32(MFG_BRCAST_PROG_DATA_114, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST0_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST1_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST3_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST4_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST5_VOLTAGEEXT, g_vmeter_stack_val);
			DRV_WriteReg32(BRISKET_ST6_VOLTAGEEXT, g_vmeter_stack_val);
		}
	}

	GPUFREQ_LOGD(
		"[%s] Vgpu: %d, TOP_DELSEL[17]: 0x%08lx, Vstack: %d, STACK_DELSEL[5:0]: 0x%08lx",
		direct == SCALE_DOWN ? "DOWN" : (direct == SCALE_UP ? "UP" : "STAY"),
		cur_vgpu, DRV_Reg32(MFG_DUMMY_REG) & TOP_HW_DELSEL_MASK,
		cur_vstack, DRV_Reg32(MFG_CG_DUMMY_REG) & STACK_HW_DELSEL_MASK);
#else
	GPUFREQ_UNREFERENCED(direct);
	GPUFREQ_UNREFERENCED(cur_vgpu);
	GPUFREQ_UNREFERENCED(cur_vstack);
#endif /* GPUFREQ_SW_VMETER_ENABLE */
}

/* API: use Vstack to find constraint range */
static void __gpufreq_get_parking_volt(enum gpufreq_opp_direct direct,
	unsigned int cur_vgpu, unsigned int cur_vstack,
	unsigned int *park_vgpu, unsigned int *park_vstack)
{
	int i = 0, parking_num = NUM_PARKING_IDX;

	/*
	 * [Springboard Guide]
	 * [Scale-up]  : find springboard volt that SMALLER than cur_volt
	 * [Scale-down]: find springboard volt that LARGER than cur_volt
	 *
	 * e.g. sb[2].vstack = 0.625V < cur_vstack = 0.65V < sb[1].vstack = 0.675V
	 * [Scale-up]   use sb[2].vstack_up
	 * [Scale-down] use sb[1].vstack_down
	 */
	if (direct == SCALE_UP) {
		/* find largest volt which is smaller than cur_volt */
		for (i = 0; i < parking_num ; i++)
			if ((cur_vgpu >= g_springboard[i].vgpu) &&
				(cur_vstack >= g_springboard[i].vstack))
				break;
		/* boundary check */
		i = i < parking_num  ? i : (parking_num  - 1);
		*park_vgpu = g_springboard[i].vgpu_up;
		*park_vstack = g_springboard[i].vstack_up;
	} else if (direct == SCALE_DOWN) {
		/* find smallest volt which is larger than cur_volt */
		for (i = parking_num  - 1; i >= 0; i--)
			if ((cur_vgpu <= g_springboard[i].vgpu) &&
				(cur_vstack <= g_springboard[i].vstack))
				break;
		/* boundary check */
		i = i < 0 ? 0 : i;
		*park_vgpu = g_springboard[i].vgpu_down;
		*park_vstack = g_springboard[i].vstack_down;
	}

	GPUFREQ_LOGD("[%s] parking Vgpu(%d->%d), Vstack(%d->%d)",
		direct == SCALE_DOWN ? "DOWN" : (direct == SCALE_UP ? "UP" : "STAY"),
		cur_vgpu, *park_vgpu, cur_vstack, *park_vstack);
}

/* API: decide DVS order of GPU and STACK */
static int __gpufreq_volt_scale(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vstack_old, unsigned int vstack_new,
	unsigned int vsram_gpu, unsigned int vsram_stack)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int park_vgpu = 0, park_vstack = 0;
	unsigned int target_vgpu = 0, target_vstack = 0;

	GPUFREQ_TRACE_START("vgpu=(%d->%d), vstack=(%d->%d), vsram_gpu=(%d), vsram_stack=(%d)",
		vgpu_old, vgpu_new, vstack_old, vstack_new, vsram_gpu, vsram_stack);

	/* scale-up: Vstack -> Vgpu */
	if (vstack_new > vstack_old) {
		while ((vstack_new != vstack_old) || (vgpu_new != vgpu_old)) {
			/* config SW Vmeter */
			__gpufreq_sw_vmeter_config(SCALE_UP, vgpu_old, vstack_old);
			/* find reachable volt fitting DVFS constraint via Vstack */
			if (vstack_new != vstack_old) {
				__gpufreq_get_parking_volt(SCALE_UP,
					vgpu_old, vstack_old, &park_vgpu, &park_vstack);
				/* only accept parking with scaling up */
				if (park_vstack > vstack_old)
					target_vstack = park_vstack < vstack_new ?
						park_vstack : vstack_new;
				else
					target_vstack = vstack_old;
				if (park_vgpu > vgpu_old)
					target_vgpu = park_vgpu < vgpu_new ?
						park_vgpu : vgpu_new;
				else
					target_vgpu = vgpu_old;
			/* if only left Vgpu not ready, directly scale Vgpu to target */
			} else {
				target_vstack = vstack_new;
				target_vgpu = vgpu_new;
			}
			/* VSTACK */
			ret = __gpufreq_volt_scale_stack(vstack_old, target_vstack);
			if (ret)
				goto done;
			vstack_old = target_vstack;
			/* VGPU */
			ret = __gpufreq_volt_scale_gpu(vgpu_old, target_vgpu);
			if (ret)
				goto done;
			vgpu_old = target_vgpu;
		}
	/* else: Vgpu -> Vstack */
	} else {
		while ((vstack_new != vstack_old) || (vgpu_new != vgpu_old)) {
			/* config SW Vmeter */
			__gpufreq_sw_vmeter_config(SCALE_DOWN, vgpu_old, vstack_old);
			/* find reachable volt fitting DVFS constraint via Vstack */
			if (vstack_new != vstack_old) {
				__gpufreq_get_parking_volt(SCALE_DOWN,
					vgpu_old, vstack_old, &park_vgpu, &park_vstack);
				/* only accept parking with scaling down */
				if (park_vstack < vstack_old)
					target_vstack = park_vstack > vstack_new ?
						park_vstack : vstack_new;
				else
					target_vstack = vstack_old;
				if (park_vgpu < vgpu_old)
					target_vgpu = park_vgpu > vgpu_new ?
						park_vgpu : vgpu_new;
				else
					target_vgpu = vgpu_old;
			/* if only left Vgpu not ready, directly scale Vgpu to target */
			} else {
				target_vstack = vstack_new;
				target_vgpu = vgpu_new;
			}
			/* VGPU */
			ret = __gpufreq_volt_scale_gpu(vgpu_old, target_vgpu);
			if (ret)
				goto done;
			vgpu_old = target_vgpu;
			/* VSTACK */
			ret = __gpufreq_volt_scale_stack(vstack_old, target_vstack);
			if (ret)
				goto done;
			vstack_old = target_vstack;
		}
	}

	/* VSRAM is controlled by DREQ */
	__gpufreq_volt_check_sram(vsram_gpu, vsram_stack);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target)
{
	unsigned int mux_src = 0;
	unsigned int freq = 0;

	if (target == TARGET_STACK) {
		mux_src = DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & MFG_SC0_SEL_BIT;
		if (mux_src == MFG_SC0_SEL_BIT)
			freq = __gpufreq_get_fmeter_fstack0();
		else if (mux_src == 0x0)
			freq = __gpufreq_get_sub_fstack();
	} else if (target == TARGET_GPU) {
		mux_src = DRV_Reg32(MFG_RPC_MFG_CK_FAST_REF_SEL) & MFG_TOP_SEL_BIT;
		if (mux_src == MFG_TOP_SEL_BIT)
			freq = __gpufreq_get_fmeter_fgpu();
		else if (mux_src == 0x0)
			freq = __gpufreq_get_sub_fgpu();
	}

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fgpu(void)
{
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_CON0);
	DRV_WriteReg32(MFG_PLL_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFG_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	/* MFG_PLL_FQMTR_CON0 0x13FA0040 [1:0] = 2'b10, select brisket_out_ck */
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (BIT(1) & ~BIT(0) | BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_TOP_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fstack0(void)
{
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_SC0_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC0_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_SC0_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_SC0_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_SC0_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_fstack1(void)
{
	unsigned int val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_SC1_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_SC1_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_SC1_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFG_SC0_PLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_SC1_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_sub_fgpu(void)
{
	/* parking clock use CCF API directly */
	return clk_get_rate(g_clk->clk_mfg_ref_sel) / 1000; /* Hz */
}

static unsigned int __gpufreq_get_sub_fstack(void)
{
	/* parking clock is fixed 26MHz */
	return 26000; /* KHz */
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fgpu(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get current frequency from PLL CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_pll_fstack0(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_SC0_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

static unsigned int __gpufreq_get_pll_fstack1(void)
{
	unsigned int con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_SC1_CON1);
	pcw = con1 & GENMASK(21, 0);
	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;
	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/* API: get current Vgpu from PMIC (mV * 100) */
static unsigned int __gpufreq_get_pmic_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;

	return volt;
}

/* API: get current Vstack from PMIC (mV * 100) */
static unsigned int __gpufreq_get_pmic_vstack(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vstack))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vstack) / 10;

	return volt;
}

/* API: get current Vsram from PMIC (mV * 100) */
static unsigned int __gpufreq_get_pmic_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram) / 10;

	return volt;
}

/* API: get current Vsram_gpu from DREQ (mV * 100) */
static unsigned int __gpufreq_get_dreq_vsram_gpu(void)
{
#if GPUFREQ_DREQ_AUTO_ENABLE
	unsigned int volt = 0, dreq_ack = 0;

	dreq_ack = DRV_Reg32(MFG_DREQ_TOP_DBG_CON_0);
	if (dreq_ack & DREQ_TOP_VLOG_ACK)
		volt = __gpufreq_get_pmic_vgpu();
	else if (dreq_ack & DREQ_TOP_VSRAM_ACK)
		volt = __gpufreq_get_pmic_vsram();

	return volt;
#else
	return __gpufreq_get_pmic_vsram();
#endif /* GPUFREQ_DREQ_AUTO_ENABLE */
}

/* API: get current Vsram_stack from DREQ (mV * 100) */
static unsigned int __gpufreq_get_dreq_vsram_stack(void)
{
#if GPUFREQ_DREQ_AUTO_ENABLE
	unsigned int volt = 0, dreq_ack = 0, l2_status = 0;

	l2_status = DRV_Reg32(MALI_L2_READY_LO);
	if (l2_status) {
		dreq_ack = DRV_Reg32(MFG_CG_DREQ_CG_DBG_CON_0);
		if (dreq_ack & DREQ_ST0_VLOG_ACK)
			volt = __gpufreq_get_pmic_vstack();
		else if (dreq_ack & DREQ_ST0_VSRAM_ACK)
			volt = __gpufreq_get_pmic_vsram();
	}

	return volt;
#else
	return __gpufreq_get_pmic_vsram();
#endif /* GPUFREQ_DREQ_AUTO_ENABLE */
}

static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt)
{
	unsigned int vsram = 0;

	if (volt <= VSRAM_THRESH)
		vsram = VSRAM_THRESH;
	else
		vsram = volt;

	return vsram;
}

/* API: compute RT GPU leakage via formula, unit: mV, 'C */
static unsigned int __gpufreq_lkg_formula_rt_gpu(unsigned int v, int t, unsigned int lkg_info)
{
	unsigned long long lkg = 0;
	/*
	 * LEAK(efuse @ 30'C) *
	 * (63 * V^3 - 109210 * V^2 + 69857000 * V - 10612000000) *
	 * (30 * T^3 - 2000 * T^2 + 120000 * T - 740000) /
	 * (1870000 * 10^10)
	 */
	lkg = lkg_info;
	lkg = lkg * ((63ULL * v * v * v) + (69857000ULL * v) - (109210ULL * v * v)
		- 10612000000ULL) / 100000;
	lkg = lkg * ((30 * t * t * t) + (120000 * t) - (2000 * t * t) - 740000);
	lkg = lkg / 1870000 / 100000;

	return (unsigned int)lkg;
}

/* API: compute HT GPU leakage via formula, unit: mV, 'C */
static unsigned int __gpufreq_lkg_formula_ht_gpu(unsigned int v, int t, unsigned int lkg_info)
{
	unsigned long long lkg = 0;
	/*
	 * LEAK(efuse @ 65'C) *
	 * (29 * V^3 - 48010 * V^2 + 34305000 * V - 2891000000) *
	 * (30 * T^3 - 2000 * T^2 + 120000 * T - 740000) /
	 * (6848750 * 10^10)
	 */
	lkg = lkg_info;
	lkg = lkg * ((29ULL * v * v * v) + (34305000ULL * v) - (48010ULL * v * v)
		- 2891000000ULL) / 100000;
	lkg = lkg * ((30 * t * t * t) + (120000 * t) - (2000 * t * t) - 740000);
	lkg = lkg / 6848750 / 100000;

	return (unsigned int)lkg;
}

/* API: compute RT STACK leakage via formula, unit: mV, 'C */
static unsigned int __gpufreq_lkg_formula_rt_stack(unsigned int v, int t, unsigned int lkg_info)
{
	unsigned long long lkg = 0;
	/*
	 * LEAK(efuse @ 30'C) *
	 * (66 * V^3 - 111280 * V^2 + 70744000 * V - 9776000000) *
	 * (4 * T^3 - 300 * T^2 + 19800 * T - 159400) /
	 * (210600 * 10^10)
	 */
	lkg = lkg_info;
	lkg = lkg * ((66ULL * v * v * v) + (70744000ULL * v) - (111280ULL * v * v)
		- 9776000000ULL) / 100000;
	lkg = lkg * ((4 * t * t * t) + (19800 * t) - (300 * t * t) - 159400);
	lkg = lkg / 210600 / 100000;

	return (unsigned int)lkg;
}

/* API: compute HT STACK leakage via formula, unit: mV, 'C */
static unsigned int __gpufreq_lkg_formula_ht_stack(unsigned int v, int t, unsigned int lkg_info)
{
	unsigned long long lkg = 0;
	/*
	 * LEAK(efuse @ 65'C) *
	 * (29 * V^3 - 47870 * V^2 + 34811000 * V - 2510000000) *
	 * (4 * T^3 - 300 * T^2 + 19800 * T - 159400) /
	 * (958600 * 10^10)
	 */
	lkg = lkg_info;
	lkg = lkg * ((29ULL * v * v * v) + (34811000ULL * v) - (47870ULL * v * v)
		- 2510000000ULL) / 100000;
	lkg = lkg * ((4 * t * t * t) + (19800 * t) - (300 * t * t) - 159400);
	lkg = lkg / 958600 / 100000;

	return (unsigned int)lkg;
}

#if GPUFREQ_GPM3_0_ENABLE
/* API: get leakage Current of STACK */
static unsigned int __gpufreq_get_lkg_istack(unsigned int volt, int temper)
{
	unsigned int i_leakage = 0;

	if ((temper >= LKG_HT_STACK_EFUSE_TEMPER) && g_stack.lkg_ht_info)
		i_leakage = __gpufreq_lkg_formula_ht_stack(volt / 100, temper, g_stack.lkg_ht_info);
	else
		i_leakage = __gpufreq_lkg_formula_rt_stack(volt / 100, temper, g_stack.lkg_rt_info);

	return i_leakage;
}

/* API: get dynamic Current of STACK */
static unsigned int __gpufreq_get_dyn_istack(unsigned int freq, unsigned int volt)
{
	unsigned long long i_dynamic = STACK_DYN_REF_CURRENT;
	unsigned int ref_freq = STACK_DYN_REF_CURRENT_FREQ;
	unsigned int ref_volt = STACK_DYN_REF_CURRENT_VOLT;

	/* use custom setting if set by cmd */
	i_dynamic = g_custom_ref_istack ? g_custom_ref_istack : i_dynamic;

	i_dynamic = i_dynamic *
		((freq * 100000ULL) / ref_freq) *
		((volt * 100000ULL) / ref_volt) /
		(100000ULL * 100000);

	return (unsigned int)i_dynamic;
}
#endif /* GPUFREQ_GPM3_0_ENABLE */

/* BUS_CLK_DIV2: use half of AXI_CLK for GPU bus if timing issue happen due to Brisket*/
static void __gpufreq_bus_clk_div2_config(void)
{
#if GPUFREQ_BUS_CLK_DIV2_ENABLE
	/* MFG_RPC_DUMMY_REG 0x13F90568 [2] = 1'b1 */
	DRV_WriteReg32(MFG_RPC_DUMMY_REG, (DRV_Reg32(MFG_RPC_DUMMY_REG) | BIT(2)));
#endif /* GPUFREQ_BUS_CLK_DIV2_ENABLE */
}

/* HW_DELSEL: let HW auto config DELSEL according to operating volt */
static void __gpufreq_top_hw_delsel_config(void)
{
#if GPUFREQ_SW_VMETER_ENABLE
	DRV_WriteReg32(BRISKET_TOP_VOLTAGEEXT, g_vmeter_gpu_val);
#endif /* GPUFREQ_SW_VMETER_ENABLE */
#if GPUFREQ_HW_DELSEL_ENABLE
	/* MFG_DUMMY_REG 0x13FBF500 (FECO), 550mV/hystereis 10mV */
	DRV_WriteReg32(MFG_DUMMY_REG, 0x0001160A);
#endif /* GPUFREQ_HW_DELSEL_ENABLE */

	/* MFG_DEFAULT_DELSEL_00 0x13FBFC80 = 0x0, choose FECO DELSEL input */
	DRV_WriteReg32(MFG_DEFAULT_DELSEL_00, 0x0);
}

/* API: config PDCA to EB MFG2 power change IRQ interface */
static void __gpufreq_pdca_irq_config(void)
{
#if GPUFREQ_PDCA_IRQ_ENABLE
	if (g_gpufreq_ready)
		/* MFG_PDCA_BACKDOOR 0x13FBF210 [0] rg_cg_pdca_pwrup_backdoor_en = 1'b1 */
		/* MFG_PDCA_BACKDOOR 0x13FBF210 [1] rg_cg_pdca_pwrdown_backdoor_en = 1'b1 */
		DRV_WriteReg32(MFG_PDCA_BACKDOOR, GENMASK(1, 0));
#endif /* GPUFREQ_PDCA_IRQ_ENABLE */
}

/* HWDCM: mask clock when GPU idle (dynamic clock mask) */
static void __gpufreq_top_hwdcm_config(void)
{
#if GPUFREQ_HWDCM_ENABLE
	/* (A) pclk DCM */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [8] GPU_SOCIF_MST_FREE_RUN = 1'b0 */
	DRV_WriteReg32(MFG_GLOBAL_CON, (DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(8)));

	/* (B) fmem GALS DCM */
	/* MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b1 */
	/* MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b1 */
	DRV_WriteReg32(MFG_ASYNC_CON, (DRV_Reg32(MFG_ASYNC_CON) | BIT(23) | BIT(25)));
	/* MFG_ASYNC_CON3 0x13FBF02C [13] chip_mfg_axi0_1_out_idle_enable = 1'b1 */
	/* MFG_ASYNC_CON3 0x13FBF02C [15] chip_mfg_axi1_1_out_idle_enable = 1'b1 */
	DRV_WriteReg32(MFG_ASYNC_CON3, (DRV_Reg32(MFG_ASYNC_CON3) | BIT(13) | BIT(15)));
	/* MFG_ASYNC_CON4 0x13FBF1B0 [11] mfg_acp_axi_in_idle_enable = 1'b1 */
	/* MFG_ASYNC_CON4 0x13FBF1B0 [22] mfg_tcu_acp_GALS_slpprot_idle_sel = 1'b1 */
	DRV_WriteReg32(MFG_ASYNC_CON4, (DRV_Reg32(MFG_ASYNC_CON4) | BIT(11) | BIT(22)));

	/* (C) faxi DCM */
	/* MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
	DRV_WriteReg32(MFG_RPC_AO_CLK_CFG, (DRV_Reg32(MFG_RPC_AO_CLK_CFG) & ~BIT(0)));

	/* (D) core slow down DCM */
	/* MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b1 */
	/* MFG_DCM_CON_0 0x13FBF010 [6:0] BG3D_DBC_CNT = 7'b0111111 */
	DRV_WriteReg32(MFG_DCM_CON_0,
		(DRV_Reg32(MFG_DCM_CON_0) & ~BIT(6)) | GENMASK(5, 0) | BIT(15));

	/* (E) dvfs hint DCM */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b0 */
	DRV_WriteReg32(MFG_GLOBAL_CON, (DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(21)));

	/* (F) core Qchannel DCM */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b0 */
	DRV_WriteReg32(MFG_GLOBAL_CON, (DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(10)));

	/* Qchannel enable */
	/* MFG_QCHANNEL_CON 0x13FBF0B4 [4] QCHANNEL_ENABLE = 1'b1 */
	DRV_WriteReg32(MFG_QCHANNEL_CON, (DRV_Reg32(MFG_QCHANNEL_CON) | BIT(4)));

	/* (G) freq bridge DCM */
	/* MFG_ASYNC_CON 0x13FBF020 [22] MEM0_MST_CG_ENABLE = 1'b1 */
	/* MFG_ASYNC_CON 0x13FBF020 [24] MEM1_MST_CG_ENABLE = 1'b1 */
	DRV_WriteReg32(MFG_ASYNC_CON, (DRV_Reg32(MFG_ASYNC_CON) | BIT(22) | BIT(24)));
	/* MFG_ASYNC_CON3 0x13FBF02C [12] chip_mfg_axi0_1_in_idle_enable = 1'b1 */
	/* MFG_ASYNC_CON3 0x13FBF02C [14] chip_mfg_axi1_1_in_idle_enable = 1'b1 */
	DRV_WriteReg32(MFG_ASYNC_CON3, (DRV_Reg32(MFG_ASYNC_CON3) | BIT(12) | BIT(14)));
	/* MFG_ASYNC_CON4 0x13FBF1B0 [12] mfg_acp_GALS_slpprot_idle_sel = 1'b1 */
	/* MFG_ASYNC_CON4 0x13FBF1B0 [23] mfg_tcu_acp_mem_gals_mst_sync_sel = 1'b1 */
	DRV_WriteReg32(MFG_ASYNC_CON4, (DRV_Reg32(MFG_ASYNC_CON4) | BIT(12) | BIT(23)));

	if (g_mcuetm_clk_enable)
		/* MFG_CG_CLR 0x13FBF008 [1] = 1'b1 */
		DRV_WriteReg32(MFG_CG_CLR, BIT(1));
#endif /* GPUFREQ_HWDCM_ENABLE */
}

/* HWDCM: mask clock when STACK idle (dynamic clock mask) */
static void __gpufreq_stack_hwdcm_config(void)
{
#if GPUFREQ_HWDCM_ENABLE
	/* (G) (H) CKgen DCM */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [24] stack_hd_bg3d_cg_free_run = 1'b0 */
	/* MFG_GLOBAL_CON 0x13FBF0B0 [25] stack_hd_bg3d_gpu_cg_free_run = 1'b0 */
	DRV_WriteReg32(MFG_GLOBAL_CON, (DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(24 & ~BIT(25))));
#endif /* GPUFREQ_HWDCM_ENABLE */
}

/* ACP: GPU can access CPU cache directly */
static void __gpufreq_acp_config(void)
{
#if GPUFREQ_ACP_ENABLE
	/* ACP Enable */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [0] M0_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [1] M1_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [2] M2_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [3] M3_coherence_enable = 1'b1 */
	DRV_WriteReg32(MFG_AXCOHERENCE_CON, (DRV_Reg32(MFG_AXCOHERENCE_CON) | GENMASK(3, 0)));

	/* Secure register rule 2/3 */
	/* MFG_SECURE_REG 0x13FBCFE0 [31] acp_mpu_rule3_disable = 1'b1 */
	DRV_WriteReg32(MFG_SECURE_REG, (DRV_Reg32(MFG_SECURE_REG) | BIT(31)));
	/* MFG_SECURE_SECU_ACP_FLT_CON 0x13FBC000 = 2'b11 */
	DRV_WriteReg32(MFG_SECURE_SECU_ACP_FLT_CON, GENMASK(1, 0));
	/* MFG_SECURE_NORM_ACP_FLT_CON 0x13FBC004 = 2'b11 */
	DRV_WriteReg32(MFG_SECURE_NORM_ACP_FLT_CON, GENMASK(1, 0));
#endif /* GPUFREQ_ACP_ENABLE */
}

/* force transaction get in to AXI2to1 to increase latency and fix timing issue */
static void __gpufreq_axi_2to1_config(void)
{
	/* disable 2to1 bypass mode and selection mode */
	DRV_WriteReg32(MFG_DISPATCH_DRAM_ACP_TCM_CON_6, 0x0);
	DRV_WriteReg32(MFG_2TO1AXI_CON_0, GENMASK(30, 16));
	DRV_WriteReg32(MFG_OUT_2TO1AXI_CON_0, GENMASK(30, 16));
	DRV_WriteReg32(MFG_TCU_DRAM_2TO1AXI_CON, GENMASK(30, 16));

	/* 2to1 bypass pipe */
	DRV_WriteReg32(MFG_2TO1AXI_BYPASS_PIPE_1, 0x0F35565F);
	DRV_WriteReg32(MFG_2TO1AXI_BYPASS_PIPE_2, 0x000FFEFB);
	DRV_WriteReg32(MFG_2TO1AXI_BYPASS_PIPE_ENABLE, GENMASK(4, 0));

	/* 2to1 priority */
	DRV_WriteReg32(MFG_2TO1AXI_PRIORITY, 0x00AA0000);
}

/* merge GPU transaction to maximize DRAM efficiency */
static void __gpufreq_axi_merger_config(void)
{
#if GPUFREQ_AXI_MERGER_ENABLE
	/* merge AXI READ to window size 8T/24T */
	DRV_WriteReg32(MFG_MERGE_R_CON_00, 0x1808FF81);
	DRV_WriteReg32(MFG_MERGE_R_CON_02, 0x1808FF81);
	DRV_WriteReg32(MFG_MERGE_R_CON_04, 0x1808FF81);
	DRV_WriteReg32(MFG_MERGE_R_CON_06, 0x1808FF81);

	/* merge AXI WRITE to window size 64T/64T */
	DRV_WriteReg32(MFG_MERGE_W_CON_00, 0x4040FF81);
	DRV_WriteReg32(MFG_MERGE_W_CON_02, 0x4040FF81);
	DRV_WriteReg32(MFG_MERGE_W_CON_04, 0x4040FF81);
	DRV_WriteReg32(MFG_MERGE_W_CON_06, 0x4040FF81);

	/* enable bypass mode, extend merge and hybrid mode */
	DRV_WriteReg32(MFG_MERGE_R_CON_08, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_R_CON_09, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_R_CON_10, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_R_CON_11, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_W_CON_08, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_W_CON_09, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_W_CON_10, 0x0000000E);
	DRV_WriteReg32(MFG_MERGE_W_CON_11, 0x0000000E);
#endif /* GPUFREQ_AXI_MERGER_ENABLE */
}

/* set AxUSER config, AxUSER reg is in MFG37 */
static void __gpufreq_axuser_config(void)
{
#if GPUFREQ_AXUSER_SLC_ENABLE

#endif /* GPUFREQ_AXUSER_SLC_ENABLE */
#if GPUFREQ_AXI_MERGER_ENABLE
	/* set AxUSER bypass sideband */
	DRV_WriteReg32(MFG_AXUSER_R_BYPASS_MRG_BY_HINT_CFG, 0xEEFFFFFF);
#endif /* GPUFREQ_AXI_MERGER_ENABLE */
}

static void __gpufreq_ocl_timestamp_config(void)
{
	/* MFG_TIMESTAMP 0x13FBF130 [0] top_tsvalueb_en = 1'b1 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer_sel = 1'b1 */
	DRV_WriteReg32(MFG_TIMESTAMP, GENMASK(1, 0));
}

/* GPM1.2: di/dt reduction by slowing down the speed of DFS */
/* GPM1.0 use CG and GPM1.2 use Brisket HW dual loop to slow down DFS */
static void __gpufreq_gpm1_2_config(void)
{
#if GPUFREQ_GPM1_2_ENABLE
	if (g_gpm1_2_mode) {
		/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300316 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_00, 0x20300316);
		/* MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 = 0x1800000C */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_01, 0x1800000C);
		/* MFG_I2M_PROTECTOR_CFG_02 0x13FBFF68 = 0x01010802 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_02, 0x01010802);
		/* wait 1us */
		udelay(1);
		/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300317 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_00, 0x20300317);
	}
#endif /* GPUFREQ_GPM1_2_ENABLE */
}

static void __gpufreq_dfd2_0_config(void)
{
#if GPUFREQ_DFD2_0_ENABLE
	if (g_dfd2_0_mode) {
		if (g_dfd2_0_mode == DFD_FORCE_DUMP)
			DRV_WriteReg32(MFG_DEBUGMON_CON_00, 0xFFFFFFFF);

		DRV_WriteReg32(MFG_DFD_CON_0, 0x0F101100);
		DRV_WriteReg32(MFG_DFD_CON_1, 0x00001B68);
		DRV_WriteReg32(MFG_DFD_CON_2, 0x0000C60B);
		DRV_WriteReg32(MFG_DFD_CON_3, 0x00814344);
		DRV_WriteReg32(MFG_DFD_CON_4, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_5, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_6, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_7, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_8, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_9, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_10, 0x00000000);
		DRV_WriteReg32(MFG_DFD_CON_11, 0x00000000);
		/* 8phase config */
		DRV_WriteReg32(MFG_DFD_CON_20, 0x00011353);
		DRV_WriteReg32(MFG_DFD_CON_21, 0x0001FE8C);
		DRV_WriteReg32(MFG_DFD_CON_22, 0x0002483A);
		DRV_WriteReg32(MFG_DFD_CON_23, 0x0002AF8F);
		DRV_WriteReg32(MFG_DFD_CON_24, 0x0002C32C);
		DRV_WriteReg32(MFG_DFD_CON_25, 0x00031526);
		DRV_WriteReg32(MFG_DFD_CON_26, 0x16193A3D);
		DRV_WriteReg32(MFG_DFD_CON_27, 0x0000050B);

		if ((DRV_Reg32(DRM_DEBUG_MFG_REG) & BIT(0)) != BIT(0)) {
			DRV_WriteReg32(DRM_DEBUG_MFG_REG, 0x77000000);
			udelay(10);
			DRV_WriteReg32(DRM_DEBUG_MFG_REG, 0x77000001);
		}
	}
#endif /* GPUFREQ_DFD2_0_ENABLE */
}

/* SW refill Broadcaster FW when AutoDMA not ready */
static void __gpufreq_broadcaster_config(void)
{
#if GPUFREQ_SW_BRCAST_ENABLE
	int i = 0;

	/* enable internal clock */
	/* MFG_BRCAST_CONFIG_4 0x13FB1FF8 [0] brcast_en = 1'b1 */
	DRV_WriteReg32(MFG_BRCAST_CONFIG_4, BIT(0));
	/* set blocking ack */
	/* power_turn_on_pause_en[8:3] = 6'b111111, ST0 ~ ST6 */
	DRV_WriteReg32(MFG_BRCAST_TEST_MODE_2, GENMASK(8, 3));
	/* reset Broadcaster */
	/* MFG_BRCAST_CONFIG_4 0x13FB1FF8 [0] brcast_clr = 1'b1 */
	DRV_WriteReg32(MFG_BRCAST_CONFIG_1, BIT(0));
	/* wait reset done */
	/* MFG_BRCAST_CONFIG_4 0x13FB1FF8 [0] brcast_clr = 1'b0 */
	i = 0;
	do {
		udelay(1);
		if (++i == 10)
			__gpufreq_abort("timeout reset CFG_1: 0x%08x, CFG_4: 0x%08x, CFG_5: 0x%08x",
				DRV_Reg32(MFG_BRCAST_CONFIG_1), DRV_Reg32(MFG_BRCAST_CONFIG_4),
				DRV_Reg32(MFG_BRCAST_CONFIG_5));
	} while (DRV_Reg32(MFG_BRCAST_CONFIG_1) & BIT(0));

	/***** Broadcaster FW START *****/

#if GPUFREQ_SW_VMETER_ENABLE
	/* unlock Broadcaster to Brisket permission */
	DRV_WriteReg32(MFG_VGPU_DEVAPC_AO_APC_CON, 0x0);
	DRV_WriteReg32(MFG_VGPU_DEVAPC_AO_MAS_SEC_0, 0x1);
	/* set Broadcaster target address */
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_3, BRISKET_ST0_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_3, BRISKET_ST0_BASE);
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_4, BRISKET_ST1_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_4, BRISKET_ST1_BASE);
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_5, BRISKET_ST3_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_5, BRISKET_ST3_BASE);
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_6, BRISKET_ST4_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_6, BRISKET_ST4_BASE);
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_7, BRISKET_ST5_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_7, BRISKET_ST5_BASE);
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_8, BRISKET_ST6_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_8, BRISKET_ST6_BASE);
	/* set Broadcaster program data content */
	DRV_WriteReg32(MFG_BRCAST_PROG_DATA_114, g_vmeter_stack_val);
	/* set Broadcaster write program data and register offset */
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_1_0_LSB, 0x00002072); /* prog_data : 114 */
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_1_0_MSB, 0x98002900); /* reg offset: 0x148 */
#if GPUFREQ_BRCAST_PARITY_CHECK
	/* set Broadcaster parity check */
	DRV_WriteReg32(MFG_BRCAST_START_END_ADDR_PTY_0_16,
		(DRV_Reg32(MFG_BRCAST_START_END_ADDR_PTY_0_16) | GENMASK(17, 6)));
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_0AND1_PTY,
		(DRV_Reg32(MFG_BRCAST_CMD_SEQ_0AND1_PTY) | BIT(16)));
#endif /* GPUFREQ_BRCAST_PARITY_CHECK */
#endif /* GPUFREQ_SW_VMETER_ENABLE */

#if GPUFREQ_HW_DELSEL_ENABLE
	/* enable SC0, SC1 HW_DELSEL */
	/* set Broadcaster target address */
	DRV_WriteReg32(MFG_BRCAST_START_ADDR_42, MFG_CG_CFG_BASE);
	DRV_WriteReg32(MFG_BRCAST_END_ADDR_42, MFG_CG_CFG_BASE);
	/* set Broadcaster program data content */
	DRV_WriteReg32(MFG_BRCAST_PROG_DATA_141, 0x0001160A);
	DRV_WriteReg32(MFG_BRCAST_PROG_DATA_142, 0x0001160A);
	/* set Broadcaster write program data and register offset */
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_0_0_LSB, 0x0000828D); /* prog_data : 141 */
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_0_0_MSB, 0x9C001080); /* reg offset: 0x084 */
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_0_1_LSB, 0x0000828E); /* prog_data : 142 */
	DRV_WriteReg32(MFG_BRCAST_CMD_SEQ_0_1_MSB, 0x9C001100); /* reg offset: 0x088 */
#endif /* GPUFREQ_HW_DELSEL_ENABLE */

	/***** Broadcaster FW END *****/

	/* start Broadcaster */
	/* MFG_BRCAST_CONFIG_5 0x13FB1FFC [0] boot_set = 1'b1 */
	DRV_WriteReg32(MFG_BRCAST_CONFIG_5, BIT(0));
	/* wait reset done */
	/* MFG_BRCAST_CONFIG_5 0x13FB1FFC [8] boot_done = 1'b1 */
	i = 0;
	do {
		udelay(1);
		if (++i == 10)
			__gpufreq_abort("timeout start CFG_1: 0x%08x, CFG_4: 0x%08x, CFG_5: 0x%08x",
				DRV_Reg32(MFG_BRCAST_CONFIG_1), DRV_Reg32(MFG_BRCAST_CONFIG_4),
				DRV_Reg32(MFG_BRCAST_CONFIG_5));
	} while ((DRV_Reg32(MFG_BRCAST_CONFIG_5) & BIT(8)) != BIT(8));
#endif /* GPUFREQ_SW_BRCAST_ENABLE */
}

static void __gpufreq_check_bus_idle(void)
{
	unsigned int val[10] = {};
	int i = -1;

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	DRV_WriteReg32(MFG_QCHANNEL_CON, (DRV_Reg32(MFG_QCHANNEL_CON) | BIT(0)));
	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	DRV_WriteReg32(MFG_DEBUG_SEL, (DRV_Reg32(MFG_DEBUG_SEL) | GENMASK(1, 0)));

	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [0] MFG_DEBUG_TOP
	 * 0x0: bus idle
	 * 0x1: bus busy
	 */
	do {
		if (++i < 50000) {
			udelay(1);
			val[i % 10] = DRV_Reg32(MFG_DEBUG_TOP);
		} else {
			GPUFREQ_LOGE("(0x13FBF178)=(0x%x, 0x%x, 0x%x, 0x%x, 0x%x)",
				val[(i - 1) % 10], val[(i - 2) % 10], val[(i - 3) % 10],
				val[(i - 4) % 10], val[(i - 5) % 10]);
			GPUFREQ_LOGE("(0x13FBF178)=(0x%x, 0x%x, 0x%x, 0x%x, 0x%x)",
				val[(i - 6) % 10], val[(i - 7) % 10], val[(i - 8) % 10],
				val[(i - 9) % 10], val[(i - 10) % 10]);
			GPUFREQ_LOGE("PWR_STA: 0x%08lx, SHADER: 0x%08x, TILER: 0x%08x, L2: 0x%08x",
				MFG_0_22_37_PWR_STATUS, DRV_Reg32(MALI_SHADER_READY_LO),
				DRV_Reg32(MALI_TILER_READY_LO), DRV_Reg32(MALI_L2_READY_LO));
			__gpufreq_abort("timeout");
			break;
		}
	} while (val[i % 10] & BIT(0));
}

/* API: if violation happened, keep MFG1 power-on and wait until DEVAPC KE triggered */
static void __gpufreq_check_devapc_vio(void)
{
#if GPUFREQ_DEVAPC_CHECK_ENABLE
	unsigned int val0, val1 = 0;

	val0 = DRV_Reg32(MFG_VGPU_DEVAPC_D0_VIO_STA_0);
	val1 = DRV_Reg32(MFG_VGPU_DEVAPC_D0_VIO_STA_1);
	if (val0 || val1) {
		GPUFREQ_LOGE("MFG DEVAPC STA0=0x%08x, STA1=0x%08x", val0, val1);
		__gpufreq_devapc_vio_handler();
		/* wait forever */
		while (1)
			udelay(1);
	}
#endif /* GPUFREQ_DEVAPC_CHECK_ENABLE */
}

static int __gpufreq_clock_ctrl(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		/* enable MFG_REF_SEL */
		ret = clk_prepare_enable(g_clk->clk_mfg_ref_sel);
		if (ret) {
			__gpufreq_abort("fail to enable clk_mfg_ref_sel (%d)", ret);
			goto done;
		}
		/* prepare MFG_TOP_PLL */
		ret = clk_prepare(g_clk->clk_mfg_top_pll);
		if (ret) {
			__gpufreq_abort("fail to prepare clk_mfg_top_pll (%d)", ret);
			goto done;
		}
		g_gpu.cg_count++;

		/* prepare MFG_SC0_PLL */
		ret = clk_prepare(g_clk->clk_mfg_sc0_pll);
		if (ret) {
			__gpufreq_abort("fail to prepare clk_mfg_sc0_pll (%d)", ret);
			goto done;
		}
		/* prepare MFG_SC1_PLL */
		ret = clk_prepare(g_clk->clk_mfg_sc1_pll);
		if (ret) {
			__gpufreq_abort("fail to prepare clk_mfg_sc1_pll (%d)", ret);
			goto done;
		}
		g_stack.cg_count++;

		/* switch GPU MUX to PLL */
		__gpufreq_clksrc_ctrl(TARGET_GPU, CLOCK_MAIN);
		/* switch STACK MUX to PLL */
		__gpufreq_clksrc_ctrl(TARGET_STACK, CLOCK_MAIN);
	} else if (power == GPU_PWR_OFF) {
		/* switch STACK MUX to REF_SEL */
		__gpufreq_clksrc_ctrl(TARGET_STACK, CLOCK_SUB);
		/* switch GPU MUX to REF_SEL */
		__gpufreq_clksrc_ctrl(TARGET_GPU, CLOCK_SUB);

		/* unprepare MFG_SC1_PLL */
		clk_unprepare(g_clk->clk_mfg_sc1_pll);
		/* unprepare MFG_SC0_PLL */
		clk_unprepare(g_clk->clk_mfg_sc0_pll);
		g_stack.cg_count--;

		/* unprepare MFG_TOP_PLL */
		clk_unprepare(g_clk->clk_mfg_top_pll);
		/* disable MFG_REF_SEL */
		clk_disable_unprepare(g_clk->clk_mfg_ref_sel);
		g_gpu.cg_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_footprint_mfg_timeout(enum gpufreq_power_state power,
	void __iomem *pwr_con, char *pwr_con_name, unsigned int footprint)
{
	__gpufreq_footprint_power_step(footprint);
	GPUFREQ_LOGE("power: %d, footprint: 0x%02x, (%s)=0x%08x",
		power, footprint, pwr_con_name, DRV_Reg32(pwr_con));
	GPUFREQ_LOGE("%s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG0", DRV_Reg32(SPM_MFG0_PWR_CON),
		"MFG1", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MFG37", DRV_Reg32(MFG_RPC_MFG37_PWR_CON),
		"MFG2", DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		"MFG3", DRV_Reg32(MFG_RPC_MFG3_PWR_CON),
		"MFG4", DRV_Reg32(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGE("%s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG22", DRV_Reg32(MFG_RPC_MFG22_PWR_CON),
		"MFG6", DRV_Reg32(MFG_RPC_MFG6_PWR_CON),
		"MFG7", DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		"MFG8", DRV_Reg32(MFG_RPC_MFG8_PWR_CON),
		"MFG9", DRV_Reg32(MFG_RPC_MFG9_PWR_CON),
		"MFG10", DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
	GPUFREQ_LOGE("%s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG11", DRV_Reg32(MFG_RPC_MFG11_PWR_CON),
		"MFG12", DRV_Reg32(MFG_RPC_MFG12_PWR_CON),
		"MFG13", DRV_Reg32(MFG_RPC_MFG13_PWR_CON),
		"MFG14", DRV_Reg32(MFG_RPC_MFG14_PWR_CON),
		"MFG15", DRV_Reg32(MFG_RPC_MFG15_PWR_CON),
		"MFG16", DRV_Reg32(MFG_RPC_MFG16_PWR_CON));
	GPUFREQ_LOGE("%s=0x%08x, %s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG17", DRV_Reg32(MFG_RPC_MFG17_PWR_CON),
		"MFG18", DRV_Reg32(MFG_RPC_MFG18_PWR_CON),
		"MFG19", DRV_Reg32(MFG_RPC_MFG19_PWR_CON),
		"MFG20", DRV_Reg32(MFG_RPC_MFG20_PWR_CON));
	GPUFREQ_LOGE("%s=0x%08x, %s=0x%08x, %s=0x%08x",
		"MFG_RPC_SLP_PROT", DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA),
		"IFR_EMISYS_PROT_0", DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0),
		"IFR_EMISYS_PROT_1", DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1));
	__gpufreq_abort("timeout");
}

static void __gpufreq_mfg1_bus_prot(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* NEMI/SEMI */
		/* IFR_EMISYS_PROTECT_EN_W1C_0 0x1002C108 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1C_0, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xC0);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xC1);
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
					"MFG1_PWR_CON", 0xC4);
		} while (DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_1) & GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xC5);

		/* RX */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [19:16] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xC6);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xC7);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xC8);

		/* TX */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [3:0] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xC9);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xCA);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xCB);

		/* ACP1 TX/RX/DVM */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [6] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [22] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [23] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, (BIT(6) | BIT(22) | BIT(23)));
		__gpufreq_footprint_power_step(0xCC);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xCD);
		} while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & (BIT(6) | BIT(22) | BIT(23)));
		__gpufreq_footprint_power_step(0xCE);

		/* ACP0 TX/RX */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [5] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [21] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, (BIT(5) | BIT(21)));
		__gpufreq_footprint_power_step(0xCF);
	} else {
		/* ACP0 TX/RX */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [5] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [21] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, (BIT(5) | BIT(21)));
		__gpufreq_footprint_power_step(0xD0);

		/* ACP1 TX/RX/DVM */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [6] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [22] = 1'b1 */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [23] = 1'b1 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, (BIT(6) | BIT(22) | BIT(23)));
		__gpufreq_footprint_power_step(0xD1);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xD2);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & (BIT(6) | BIT(22) | BIT(23))) !=
			(BIT(6) | BIT(22) | BIT(23)));
		__gpufreq_footprint_power_step(0xD3);

		/* TX */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [3:0] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xD4);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xD5);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(3, 0)) != GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xD6);

		/* Rx */
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [19:16] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xD7);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xD8);
		} while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16)) != GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xD9);

		/* EMI */
		/* IFR_EMISYS_PROTECT_EN_W1S_0 0x1002C104 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1S_0, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xDA);
		/* IFR_EMISYS_PROTECT_EN_W1S_1 0x1002C124 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1S_1, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xDB);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xDC);
		} while ((DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_0) & GENMASK(20, 19)) !=
			GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xDD);
		i = 0;
		do {
			udelay(10);
			if (++i == 500)
				__gpufreq_footprint_mfg_timeout(power, MFG_RPC_MFG1_PWR_CON,
					"MFG1_PWR_CON", 0xDE);
		} while ((DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_1) & GENMASK(20, 19)) !=
			GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xDF);
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

static void __gpufreq_mtcmos_ctrl(enum gpufreq_power_state power)
{
	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG1_PWR_CON, "MFG1_PWR_CON");
		g_gpu.mtcmos_count++;
		g_stack.mtcmos_count++;
#if !GPUFREQ_PDCA_ENABLE
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG37_PWR_CON, "MFG37_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG2_PWR_CON, "MFG2_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG3_PWR_CON, "MFG3_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG4_PWR_CON, "MFG4_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG22_PWR_CON, "MFG22_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG6_PWR_CON, "MFG6_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG7_PWR_CON, "MFG7_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG8_PWR_CON, "MFG8_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG9_PWR_CON, "MFG9_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG10_PWR_CON, "MFG10_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG11_PWR_CON, "MFG11_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG12_PWR_CON, "MFG12_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG13_PWR_CON, "MFG13_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG14_PWR_CON, "MFG14_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG15_PWR_CON, "MFG15_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG16_PWR_CON, "MFG16_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG17_PWR_CON, "MFG17_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG18_PWR_CON, "MFG18_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG19_PWR_CON, "MFG19_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG20_PWR_CON, "MFG20_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG25_PWR_CON, "MFG25_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG26_PWR_CON, "MFG26_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG27_PWR_CON, "MFG27_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG28_PWR_CON, "MFG28_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG29_PWR_CON, "MFG29_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG30_PWR_CON, "MFG30_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG31_PWR_CON, "MFG31_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG32_PWR_CON, "MFG32_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG33_PWR_CON, "MFG33_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG34_PWR_CON, "MFG34_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG35_PWR_CON, "MFG35_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_ON, MFG_RPC_MFG36_PWR_CON, "MFG36_PWR_CON");
#endif /* GPUFREQ_PDCA_ENABLE */
	} else if (power == GPU_PWR_OFF) {
#if !GPUFREQ_PDCA_ENABLE
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG36_PWR_CON, "MFG36_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG35_PWR_CON, "MFG35_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG34_PWR_CON, "MFG34_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG33_PWR_CON, "MFG33_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG32_PWR_CON, "MFG32_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG31_PWR_CON, "MFG31_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG30_PWR_CON, "MFG30_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG29_PWR_CON, "MFG29_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG28_PWR_CON, "MFG28_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG27_PWR_CON, "MFG27_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG26_PWR_CON, "MFG26_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG25_PWR_CON, "MFG25_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG20_PWR_CON, "MFG20_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG19_PWR_CON, "MFG19_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG18_PWR_CON, "MFG18_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG17_PWR_CON, "MFG17_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG16_PWR_CON, "MFG16_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG15_PWR_CON, "MFG15_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG14_PWR_CON, "MFG14_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG13_PWR_CON, "MFG13_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG12_PWR_CON, "MFG12_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG11_PWR_CON, "MFG11_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG10_PWR_CON, "MFG10_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG9_PWR_CON, "MFG9_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG8_PWR_CON, "MFG8_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG7_PWR_CON, "MFG7_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG6_PWR_CON, "MFG6_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG22_PWR_CON, "MFG22_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG4_PWR_CON, "MFG4_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG3_PWR_CON, "MFG3_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG2_PWR_CON, "MFG2_PWR_CON");
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG37_PWR_CON, "MFG37_PWR_CON");
#endif /* GPUFREQ_PDCA_ENABLE */
		__gpufreq_mfgx_rpc_ctrl(GPU_PWR_OFF, MFG_RPC_MFG1_PWR_CON, "MFG1_PWR_CON");
		g_gpu.mtcmos_count--;
		g_stack.mtcmos_count--;
	}

	GPUFREQ_LOGD("power: %d, MFG_0_31_PWR_STA: 0x%08lx, MFG_32_37_PWR_STA: 0x%08lx",
		power, MFG_0_31_PWR_STATUS, MFG_32_37_PWR_STATUS);

	GPUFREQ_TRACE_END();
}

static int __gpufreq_vstack_ctrl(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		ret = regulator_enable(g_pmic->reg_vstack);
		if (ret) {
			__gpufreq_abort("fail to enable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count++;
		/* release VSTACK Buck ISO */
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [16] VSTACK0_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(16));
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [28] VSTACK1_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(28));
		/* release VSTACK AO reset */
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [10] SC_MFG1_CT0_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(10));
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [14] SC_MFG1_CT1_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(14));
	} else if (power == GPU_PWR_OFF) {
		/* set VSTACK AO reset */
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [14] SC_MFG1_CT1_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(14));
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [10] SC_MFG1_CT0_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(10));
		/* set VSTACK Buck ISO */
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [28] VSTACK1_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(28));
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [16] VSTACK0_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(16));
		ret = regulator_disable(g_pmic->reg_vstack);
		if (ret) {
			__gpufreq_abort("fail to disable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count--;
	}

	GPUFREQ_LOGD("power: %d, SPM_SOC_BUCK_ISO_CON: 0x%08x, MFG_RPC_MFG1_PWR_CON: 0x%08x",
		power, DRV_Reg32(SPM_SOC_BUCK_ISO_CON), DRV_Reg32(MFG_RPC_MFG1_PWR_CON));

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_buck_ctrl(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on: VSRAM -> VGPU -> VSTACK */
	if (power == GPU_PWR_ON) {
#if GPUFREQ_COVSRAM_CTRL_ENABLE
		/* notify MCDI task GPU is power-on to keep co-VSRAM state */
		DRV_WriteReg32(MCDI_MBOX_GPU_STA, 0x1);
#endif /* GPUFREQ_COVSRAM_CTRL_ENABLE */

		ret = regulator_enable(g_pmic->reg_vgpu);
		if (ret) {
			__gpufreq_abort("fail to enable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count++;
		/* release VGPU Buck ISO */
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [8] VGPU_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(8));
		/* release VGPU AO reset */
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [16] TOP_TOP_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(16));

#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		ret = regulator_enable(g_pmic->reg_vstack);
		if (ret) {
			__gpufreq_abort("fail to enable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count++;
		/* release VSTACK Buck ISO */
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [16] VSTACK0_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(16));
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001F6C [28] VSTACK1_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(28));
		/* release VSTACK AO reset */
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [10] SC_MFG1_CT0_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(10));
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [14] SC_MFG1_CT1_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(14));
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */
	/* power off: VSTACK-> VGPU -> VSRAM */
	} else if (power == GPU_PWR_OFF) {
#if !GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE
		/* set VSTACK AO reset */
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [14] SC_MFG1_CT1_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(14));
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [10] SC_MFG1_CT0_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(10));
		/* set VSTACK Buck ISO */
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [28] VSTACK1_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(28));
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [16] VSTACK0_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(16));
		ret = regulator_disable(g_pmic->reg_vstack);
		if (ret) {
			__gpufreq_abort("fail to disable VSTACK (%d)", ret);
			goto done;
		}
		g_stack.buck_count--;
#endif /* GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE */

		/* set VGPU AO reset */
		/* MFG_RPC_MFG1_PWR_CON 0x13F91070 [16] TOP_TOP_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(16));
		/* set VGPU Buck ISO */
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001F68 [8] VGPU_BUCK_ISO = 1'b1 */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(8));
		ret = regulator_disable(g_pmic->reg_vgpu);
		if (ret) {
			__gpufreq_abort("fail to disable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;

#if GPUFREQ_COVSRAM_CTRL_ENABLE
		/* notify MCDI task GPU is power-off to release co-VSRAM state */
		DRV_WriteReg32(MCDI_MBOX_GPU_STA, 0x2);
#endif /* GPUFREQ_COVSRAM_CTRL_ENABLE */
	}

	GPUFREQ_LOGD("power: %d, %s: 0x%08x, %s: 0x%08x, %s: 0x%08x",
		power, "SOC_BUCK_ISO", DRV_Reg32(SPM_SOC_BUCK_ISO_CON),
		"MFG1_PWR_CON", DRV_Reg32(MFG_RPC_MFG1_PWR_CON),
		"MCDI_MBOX_GPU_STA", DRV_Reg32(MCDI_MBOX_GPU_STA));

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_init_gpm3_0_table(void)
{
#if GPUFREQ_GPM3_0_ENABLE
	int i = 0, j = 0, temper = 0;
	int gpm3_num = NUM_GPM3_LIMIT, opp_num = g_stack.opp_num;
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	unsigned int i_stack = 0, i_dyn_stack = 0, i_lkg_stack = 0;
	unsigned int imax_stack = GPM3_STACK_IMAX;
	unsigned int fstack = 0, vstack = 0;

	/* use custom setting if set by cmd */
	imax_stack = g_custom_stack_imax ? g_custom_stack_imax : imax_stack;

	/* find valid OPP fit Imax spec at different temperature */
	for (i = 0; i < gpm3_num; i++) {
		temper = g_gpm3_table[i].temper;
		for (j = 0; j < opp_num; j++) {
			fstack = working_stack[j].freq;
			vstack = working_stack[j].volt;
			/* compute dyn/lkg current */
			i_dyn_stack = __gpufreq_get_dyn_istack(fstack, vstack);
			i_lkg_stack = __gpufreq_get_lkg_istack(vstack, temper);
			/* total STACK current */
			i_stack = i_dyn_stack + i_lkg_stack;
			if (i_stack <= imax_stack)
				break;
		}
		/* fail to find valid OPP fit Imax spec */
		if (j == opp_num)
			__gpufreq_abort("invalid Imax_STACK(%d) < OPP[%02d](%d)",
				imax_stack, j - 1, i_stack);
		g_gpm3_table[i].ceiling = j;
		g_gpm3_table[i].i_stack = i_stack;
	}

	GPUFREQ_LOGD("[GPM3.0] Imax_STACK: %d", imax_stack);
	for (i = 0; i < gpm3_num; i++)
		GPUFREQ_LOGD("[%02d] Temper: %d, CeilingOPP: %d, I_STACK: %d",
			i, g_gpm3_table[i].temper, g_gpm3_table[i].ceiling,
			g_gpm3_table[i].i_stack);

	/* update current status to shared memory */
	if (g_shared_status)
		__gpufreq_update_shared_status_gpm3_table();
#endif /* GPUFREQ_GPM3_0_ENABLE */
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	int target_oppidx = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(g_gpu.cur_freq);
	/* get current STACK OPP idx by freq set in preloader */
	g_stack.cur_oppidx = __gpufreq_get_idx_by_fstack(g_stack.cur_freq);

#if GPUFREQ_CUST_INIT_ENABLE
	/* decide first OPP idx by custom setting */
	target_oppidx = GPUFREQ_CUST_INIT_OPPIDX;
#else
	/* decide first OPP idx by preloader setting */
	target_oppidx = g_stack.cur_oppidx;
#endif /* GPUFREQ_CUST_INIT_ENABLE */

	GPUFREQ_LOGI(
		"init [%02d] GPU F(%d->%d) V(%d->%d), STACK F(%d->%d) V(%d->%d), VSRAM(%d->%d)",
		target_oppidx,
		g_gpu.cur_freq, working_gpu[target_oppidx].freq,
		g_gpu.cur_volt, working_gpu[target_oppidx].volt,
		g_stack.cur_freq, working_stack[target_oppidx].freq,
		g_stack.cur_volt, working_stack[target_oppidx].volt,
		g_stack.cur_vsram,
		MAX(working_stack[target_oppidx].vsram, working_gpu[target_oppidx].vsram));

	/* init first OPP index */
#if GPUFREQ_DVFS_ENABLE
	g_dvfs_state = DVFS_FREE;
	GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);
	ret = __gpufreq_generic_commit_stack(target_oppidx, DVFS_FREE);
#else
	g_dvfs_state = DVFS_DISABLE;
	GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);
#if GPUFREQ_CUST_INIT_ENABLE
	/* set OPP one time if DVFS is disabled but custom init is enabled */
	ret = __gpufreq_generic_commit_stack(target_oppidx, DVFS_DISABLE);
#endif /* GPUFREQ_CUST_INIT_ENABLE */
#endif /* GPUFREQ_DVFS_ENABLE */

#if GPUFREQ_MSSV_TEST_MODE
	/* disable DVFS when MSSV test */
	g_dvfs_state = DVFS_MSSV_TEST;
#endif /* GPUFREQ_MSSV_TEST_MODE */

	GPUFREQ_TRACE_END();

	return ret;
}

/* API: update DVFS constraint springboard */
static void __gpufreq_update_springboard(void)
{
	struct gpufreq_opp_info *signed_gpu = NULL;
	struct gpufreq_opp_info *signed_stack = NULL;
	int i = 0, parking_num = 0, oppidx = 0;

	signed_gpu = g_gpu.signed_table;
	signed_stack = g_stack.signed_table;
	parking_num = NUM_PARKING_IDX;

	/* update parking idx and volt to springboard */
	for (i = 0; i < parking_num; i++) {
		oppidx = g_parking_idx[i];
		g_springboard[i].oppidx = oppidx;
		g_springboard[i].vgpu = signed_gpu[oppidx].volt;
		g_springboard[i].vstack = signed_stack[oppidx].volt;
	}

	/* update scale-up volt springboard back to front */
	for (i = parking_num - 1; i > 0; i--) {
		g_springboard[i].vgpu_up = g_springboard[i - 1].vgpu;
		g_springboard[i].vstack_up = g_springboard[i - 1].vstack;
	}
	g_springboard[0].vgpu_up = VGPU_MAX_VOLT;
	g_springboard[0].vstack_up = VSTACK_MAX_VOLT;

	/* update scale-down volt springboard front to back */
	for (i = 0; i < parking_num - 1; i++) {
		g_springboard[i].vgpu_down = g_springboard[i + 1].vgpu;
		g_springboard[i].vstack_down = g_springboard[i + 1].vstack;
	}
	g_springboard[parking_num - 1].vgpu_down = VGPU_MIN_VOLT;
	g_springboard[parking_num - 1].vstack_down = VSTACK_MIN_VOLT;

	for (i = 0; i < parking_num; i++)
		GPUFREQ_LOGI("[%02d*] Vgpu: %d (Up: %d, Down: %d), Vstack: %d (Up: %d, Down: %d)",
			g_springboard[i].oppidx, g_springboard[i].vgpu,
			g_springboard[i].vgpu_up, g_springboard[i].vgpu_down,
			g_springboard[i].vstack, g_springboard[i].vstack_up,
			g_springboard[i].vstack_down);
};

/* API: calculate power of every OPP in working table */
static void __gpufreq_measure_power(void)
{
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	struct gpufreq_opp_info *working_stack = g_stack.working_table;
	unsigned int freq = 0, volt = 0;
	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0;
	int opp_num_gpu = g_gpu.opp_num;
	int opp_num_stack = g_stack.opp_num;
	int i = 0;

	for (i = 0; i < opp_num_gpu; i++) {
		freq = working_gpu[i].freq;
		volt = working_gpu[i].volt;

		p_leakage = __gpufreq_get_lkg_pgpu(volt, 30);
		p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_gpu[i].power = p_total;

		GPUFREQ_LOGD("GPU[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}

	for (i = 0; i < opp_num_stack; i++) {
		freq = working_stack[i].freq;
		volt = working_stack[i].volt;

		p_leakage = __gpufreq_get_lkg_pstack(volt, 30);
		p_dynamic = __gpufreq_get_dyn_pstack(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_stack[i].power = p_total;

		GPUFREQ_LOGD("STACK[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_power_gpu = working_gpu[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = working_gpu[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = working_gpu[g_gpu.min_oppidx].power;
		g_shared_status->cur_power_stack = working_stack[g_stack.cur_oppidx].power;
		g_shared_status->max_power_stack = working_stack[g_stack.max_oppidx].power;
		g_shared_status->min_power_stack = working_stack[g_stack.min_oppidx].power;
	}
}

/*
 * API: interpolate OPP from signoff idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(enum gpufreq_target target)
{
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0, pmic_step = 0;
	int adj_num = 0, i = 0, j = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	int range = 0, slope = 0;
	const int *signed_idx = NULL;
	struct gpufreq_opp_info *signed_table = NULL;

	if (target == TARGET_STACK) {
		adj_num = NUM_STACK_SIGNED_IDX;
		signed_idx = g_stack_signed_idx;
		signed_table = g_stack.signed_table;
		pmic_step = VSTACK_PMIC_STEP;
	} else {
		adj_num = NUM_GPU_SIGNED_IDX;
		signed_idx = g_gpu_signed_idx;
		signed_table = g_gpu.signed_table;
		pmic_step = VGPU_PMIC_STEP;
	}

	mutex_lock(&gpufreq_lock);

	for (i = 1; i < adj_num; i++) {
		front_idx = signed_idx[i - 1];
		rear_idx = signed_idx[i];
		range = rear_idx - front_idx;

		/* freq division to amplify slope */
		large_volt = signed_table[front_idx].volt * 100;
		large_freq = signed_table[front_idx].freq / 1000;

		small_volt = signed_table[rear_idx].volt * 100;
		small_freq = signed_table[rear_idx].freq / 1000;

		/* slope = volt / freq */
		slope = (int)(large_volt - small_volt) / (int)(large_freq - small_freq);

		if (slope < 0)
			__gpufreq_abort("invalid slope: %d", slope);

		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, slope: %d",
			target == TARGET_STACK ? "STACK" : "GPU",
			rear_idx, small_freq * 1000, small_volt / 100, slope);

		/* start from small V and F, and use (+) instead of (-) */
		for (j = 1; j < range; j++) {
			inner_idx = rear_idx - j;
			inner_freq = signed_table[inner_idx].freq / 1000;
			inner_volt = (small_volt + slope * (inner_freq - small_freq)) / 100;
			inner_volt = PMIC_STEP_ROUNDUP(inner_volt, pmic_step);

			/* compare interpolated volt with volt of previous OPP idx */
			previous_volt = signed_table[inner_idx + 1].volt;
			if (inner_volt < previous_volt)
				__gpufreq_abort("invalid %s[%d*] Volt: %d < [%d*] Volt: %d",
					target == TARGET_STACK ? "STACK" : "GPU",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);

			/* record margin */
			signed_table[inner_idx].margin += signed_table[inner_idx].volt - inner_volt;
			/* update to signed table */
			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vlogic(inner_volt);

			GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d, Vsram: %d",
				target == TARGET_STACK ? "STACK" : "GPU",
				inner_idx, inner_freq * 1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("%s[%02d*] Freq: %d, Volt: %d",
			target == TARGET_STACK ? "STACK" : "GPU",
			front_idx, large_freq * 1000, large_volt / 100);
	}

	mutex_unlock(&gpufreq_lock);
}

static unsigned int __gpufreq_compute_avs_freq(unsigned int val)
{
	unsigned int freq = 0;

	freq |= (val & BIT(20)) >> 10;         /* Get freq[10]  from efuse[20]    */
	freq |= (val & GENMASK(11, 10)) >> 2;  /* Get freq[9:8] from efuse[11:10] */
	freq |= (val & GENMASK(1, 0)) << 6;    /* Get freq[7:6] from efuse[1:0]   */
	freq |= (val & GENMASK(7, 6)) >> 2;    /* Get freq[5:4] from efuse[7:6]   */
	freq |= (val & GENMASK(19, 18)) >> 16; /* Get freq[3:2] from efuse[19:18] */
	freq |= (val & GENMASK(13, 12)) >> 12; /* Get freq[1:0] from efuse[13:12] */
	/* Freq is stored in efuse with MHz unit */
	freq *= 1000;

	return freq;
}

static unsigned int __gpufreq_compute_avs_volt(unsigned int val)
{
	unsigned int volt = 0;

	volt |= (val & GENMASK(17, 14)) >> 14; /* Get volt[3:0] from efuse[17:14] */
	volt |= (val & GENMASK(5, 4));         /* Get volt[5:4] from efuse[5:4]   */
	volt |= (val & GENMASK(3, 2)) << 4;    /* Get volt[7:6] from efuse[3:2]   */
	/* Volt is stored in efuse with 6.25mV unit */
	volt *= 625;

	return volt;
}

static void __gpufreq_compute_avs(void)
{
	unsigned int val = 0;
	unsigned int temp_volt = 0, temp_freq = 0, volt_ofs = 0, temp_volt_sn = 0;
	int i = 0, oppidx = 0, adj_num_gpu = 0, adj_num_stack = 0;
	struct gpufreq_avs_info avs_info[NUM_STACK_SIGNED_IDX] = {
		{EFUSE_PTPOD31, EFUSE_PTPOD27, 0x00D2208B, 0x00DCE0C9, 0, 0}, /* P1 */
		{EFUSE_PTPOD32, EFUSE_PTPOD28, 0x00C88CB7, 0x00CCE866, 0, 0}, /* P2 */
		{EFUSE_PTPOD32, EFUSE_PTPOD29, 0x00CC08E6, 0x00C4E416, 0, 0}, /* P3 */
		{EFUSE_PTPOD33, EFUSE_PTPOD30, 0x00C09416, 0x00C28087, 0, 0}, /* P4 */
	};

	adj_num_gpu = NUM_GPU_SIGNED_IDX;
	adj_num_stack = NUM_STACK_SIGNED_IDX;

	/*
	 * Compute GPU AVS
	 * Freq (MHz) | Signoff Volt (V) | EFUSE Name
	 * ============================================
	 * 1612       | 1.0              | PTPOD31
	 * 1352       | 0.9              | PTPOD32
	 * 1352       | 0.9              | PTPOD32
	 * 260        | 0.5              | PTPOD33
	 */
	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num_gpu; i++) {
		oppidx = g_gpu_avs_table[i].oppidx;
#ifdef GPUFREQ_MCL50_LOAD
		val = avs_info[i].reg_gpu_mcl50;
#else
		if (g_avs_enable)
			val = avs_info[i].reg_gpu_avs ? DRV_Reg32(avs_info[i].reg_gpu_avs) : 0;
		else
			val = 0;
#endif /* GPUFREQ_MCL50_LOAD */

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = __gpufreq_compute_avs_freq(val);
		/* verify with signoff Freq */
		if (temp_freq != g_gpu.signed_table[oppidx].freq) {
			__gpufreq_abort("GPU[%02d*]: efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_gpu.signed_table[oppidx].freq);
			return;
		}
		g_gpu_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		temp_volt = __gpufreq_compute_avs_volt(val);
		g_gpu_avs_table[i].volt = PMIC_STEP_ROUNDUP(temp_volt, VGPU_PMIC_STEP);

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;
	}

	/* check AVS Volt and update Vsram */
	for (i = adj_num_gpu - 1; i >= 0; i--) {
		oppidx = g_gpu_avs_table[i].oppidx;
		/* add 2 steps if reverse happen */
		volt_ofs = VGPU_PMIC_STEP * 2;

		/* if AVS Volt is not set */
		if (!g_gpu_avs_table[i].volt)
			continue;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i + 1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num_gpu - 1)) {
			if (g_gpu_avs_table[i].volt <= g_gpu_avs_table[i + 1].volt) {
				GPUFREQ_LOGW("GPU efuse[%d].volt(%d) <= efuse[%d].volt(%d)",
					i, g_gpu_avs_table[i].volt,
					i + 1, g_gpu_avs_table[i + 1].volt);
				g_gpu_avs_table[i].volt = g_gpu_avs_table[i + 1].volt + volt_ofs;
			}
		}

		/* check whether AVS Volt is larger than signoff Volt */
		if (g_gpu_avs_table[i].volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("GPU[%02d*]: efuse[%d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_gpu_avs_table[i].volt,
				g_gpu.signed_table[oppidx].volt);
			g_overdrive_ic = true;
		}

		/* update Vsram */
		g_gpu_avs_table[i].vsram = __gpufreq_get_vsram_by_vlogic(g_gpu_avs_table[i].volt);
	}

	/*
	 * Compute STACK AVS
	 * Freq (MHz) | Signoff Volt (V) | EFUSE Name
	 * ============================================
	 * 1612       | 1.0              | PTPOD27
	 * 1300       | 0.85             | PTPOD28
	 * 676        | 0.65             | PTPOD29
	 * 260        | 0.5              | PTPOD30
	 */
	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num_stack; i++) {
		oppidx = g_stack_avs_table[i].oppidx;
#ifdef GPUFREQ_MCL50_LOAD
		val = avs_info[i].reg_stack_mcl50;
#else
		if (g_avs_enable)
			val = avs_info[i].reg_stack_avs ? DRV_Reg32(avs_info[i].reg_stack_avs) : 0;
		else
			val = 0;
#endif /* GPUFREQ_MCL50_LOAD */

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = __gpufreq_compute_avs_freq(val);
		/* verify with signoff Freq */
		if (temp_freq != g_stack.signed_table[oppidx].freq) {
			__gpufreq_abort("STACK[%02d*]: efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_stack.signed_table[oppidx].freq);
			return;
		}
		g_stack_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		temp_volt = __gpufreq_compute_avs_volt(val);
		g_stack_avs_table[i].volt = PMIC_STEP_ROUNDUP(temp_volt, VSTACK_PMIC_STEP);

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;
	}

	/* check AVS Volt and update Vsram */
	for (i = adj_num_stack - 1; i >= 0; i--) {
		oppidx = g_stack_avs_table[i].oppidx;
		/* add 3 steps if reverse happen */
		volt_ofs = VSTACK_PMIC_STEP * 3;

		/* if AVS Volt is not set */
		if (!g_stack_avs_table[i].volt)
			continue;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i + 1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num_stack - 1)) {
			if (g_stack_avs_table[i].volt <= g_stack_avs_table[i + 1].volt) {
				GPUFREQ_LOGW("STACK efuse[%d].volt(%d) <= efuse[%d].volt(%d)",
					i, g_stack_avs_table[i].volt,
					i + 1, g_stack_avs_table[i + 1].volt);
				g_stack_avs_table[i].volt =
					g_stack_avs_table[i + 1].volt + volt_ofs;
			}
		}

		/* check whether AVS Volt is larger than signoff Volt */
		if (g_stack_avs_table[i].volt > g_stack.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("STACK[%d*]: efuse[%d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_stack_avs_table[i].volt,
				g_stack.signed_table[oppidx].volt);
			g_overdrive_ic = true;
		}

		/* update Vsram */
		g_stack_avs_table[i].vsram =
			__gpufreq_get_vsram_by_vlogic(g_stack_avs_table[i].volt);
	}

	for (i = 0; i < adj_num_gpu; i++)
		GPUFREQ_LOGI("GPU[%02d*]: efuse[%d] freq(%d), volt(%d)",
			g_gpu_avs_table[i].oppidx, i, g_gpu_avs_table[i].freq,
			g_gpu_avs_table[i].volt);

	for (i = 0; i < adj_num_stack; i++)
		GPUFREQ_LOGI("STACK[%02d*]: efuse[%d] freq(%d), volt(%d)",
			g_stack_avs_table[i].oppidx, i, g_stack_avs_table[i].freq,
			g_stack_avs_table[i].volt);
}

static void __gpufreq_apply_adjustment(void)
{
	struct gpufreq_opp_info *signed_gpu = NULL;
	struct gpufreq_opp_info *signed_stack = NULL;
	int adj_num_gpu = 0, adj_num_stack = 0, oppidx = 0, i = 0;
	unsigned int avs_volt = 0, avs_vsram = 0, aging_volt = 0;

	signed_gpu = g_gpu.signed_table;
	signed_stack = g_stack.signed_table;
	adj_num_gpu = NUM_GPU_SIGNED_IDX;
	adj_num_stack = NUM_STACK_SIGNED_IDX;

	/* apply AVS margin */
	if (g_avs_margin) {
		/* GPU AVS */
		for (i = 0; i < adj_num_gpu; i++) {
			oppidx = g_gpu_avs_table[i].oppidx;
			avs_volt = g_gpu_avs_table[i].volt ?
				g_gpu_avs_table[i].volt : signed_gpu[oppidx].volt;
			avs_vsram = g_gpu_avs_table[i].vsram ?
				g_gpu_avs_table[i].vsram : signed_gpu[oppidx].vsram;
			/* record margin */
			signed_gpu[oppidx].margin += signed_gpu[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt = avs_volt;
			signed_gpu[oppidx].vsram = avs_vsram;
		}
		/* STACK AVS */
		for (i = 0; i < adj_num_stack; i++) {
			oppidx = g_stack_avs_table[i].oppidx;
			avs_volt = g_stack_avs_table[i].volt ?
				g_stack_avs_table[i].volt : signed_stack[oppidx].volt;
			avs_vsram = g_stack_avs_table[i].vsram ?
				g_stack_avs_table[i].vsram : signed_stack[oppidx].vsram;
			/* record margin */
			signed_stack[oppidx].margin += signed_stack[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_stack[oppidx].volt = avs_volt;
			signed_stack[oppidx].vsram = avs_vsram;
		}
	} else
		GPUFREQ_LOGI("AVS margin is not set");

	/* apply Aging margin */
	if (g_aging_load) {
		/* GPU Aging */
		for (i = 0; i < adj_num_gpu; i++) {
			oppidx = g_gpu_aging_table[i].oppidx;
			aging_volt = g_gpu_aging_table[i].volt;
			/* record margin */
			signed_gpu[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt -= aging_volt;
			signed_gpu[oppidx].vsram =
				__gpufreq_get_vsram_by_vlogic(signed_gpu[oppidx].volt);
		}
		/* STACK Aging */
		for (i = 0; i < adj_num_stack; i++) {
			oppidx = g_stack_aging_table[i].oppidx;
			aging_volt = g_stack_aging_table[i].volt;
			/* record margin */
			signed_stack[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_stack[oppidx].volt -= aging_volt;
			signed_stack[oppidx].vsram =
				__gpufreq_get_vsram_by_vlogic(signed_stack[oppidx].volt);
		}
		g_aging_margin = true;
	} else
		GPUFREQ_LOGI("Aging margin is not set");

	/* compute others OPP exclude signoff idx */
	__gpufreq_interpolate_volt(TARGET_GPU);
	__gpufreq_interpolate_volt(TARGET_STACK);
}

/*
 * 1. init OPP segment range
 * 2. init segment/working OPP table
 * 3. init power measurement
 */
static void __gpufreq_init_opp_table(void)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;

	/* init current GPU/STACK freq and volt set by preloader */
	g_gpu.cur_freq = __gpufreq_get_pll_fgpu();
	g_gpu.cur_volt = __gpufreq_get_pmic_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_pmic_vsram();

	g_stack.cur_freq = __gpufreq_get_pll_fstack0();
	g_stack.cur_volt = __gpufreq_get_pmic_vstack();
	g_stack.cur_vsram = __gpufreq_get_pmic_vsram();

	/* init SW Vmeter */
	g_vmeter_gpu_val = ((SW_VMETER_DELSEL_0_VOLT / 100) << 2) | BIT(0);
	g_vmeter_stack_val = ((SW_VMETER_DELSEL_0_VOLT / 100) << 2) | BIT(0);

	GPUFREQ_LOGI(
		"preloader init [GPU] Freq: %d, Volt: %d [STACK] Freq: %d, Volt: %d, Vsram: %d",
		g_gpu.cur_freq, g_gpu.cur_volt,
		g_stack.cur_freq, g_stack.cur_volt, g_stack.cur_vsram);

	/* init GPU OPP table */
	segment_id = g_gpu.segment_id;
	if (segment_id == MT6989_SEGMENT)
		g_gpu.segment_upbound = MT6989_SEGMENT_UPBOUND;
	else
		g_gpu.segment_upbound = 0;
	g_gpu.segment_lowbound = NUM_GPU_SIGNED_OPP - 1;
	g_gpu.signed_opp_num = NUM_GPU_SIGNED_OPP;
	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;
	g_gpu.signed_table = g_gpu_default_opp_table;

	g_gpu.working_table = kcalloc(g_gpu.opp_num, sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_gpu.working_table) {
		__gpufreq_abort("fail to alloc g_gpu.working_table (%dB)",
			g_gpu.opp_num * sizeof(struct gpufreq_opp_info));
		return;
	}
	GPUFREQ_LOGD("num of signed GPU OPP: %d, segment bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("num of working GPU OPP: %d", g_gpu.opp_num);

	/* init STACK OPP table */
	segment_id = g_stack.segment_id;
	if (segment_id == MT6989_SEGMENT)
		g_stack.segment_upbound = MT6989_SEGMENT_UPBOUND;
	else
		g_stack.segment_upbound = 0;
	g_stack.segment_lowbound = NUM_STACK_SIGNED_OPP - 1;
	g_stack.signed_opp_num = NUM_STACK_SIGNED_OPP;
	g_stack.max_oppidx = 0;
	g_stack.min_oppidx = g_stack.segment_lowbound - g_stack.segment_upbound;
	g_stack.opp_num = g_stack.min_oppidx + 1;
	g_stack.signed_table = g_stack_default_opp_table;

	g_stack.working_table = kcalloc(g_stack.opp_num,
		sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_stack.working_table) {
		__gpufreq_abort("fail to alloc g_stack.working_table (%dB)",
			g_stack.opp_num * sizeof(struct gpufreq_opp_info));
		return;
	}
	GPUFREQ_LOGD("num of signed STACK OPP: %d, segment bound: [%d, %d]",
		g_stack.signed_opp_num, g_stack.segment_upbound, g_stack.segment_lowbound);
	GPUFREQ_LOGI("num of working STACK OPP: %d", g_stack.opp_num);

	/* update signed OPP table from MFGSYS features */

	/* compute AVS table based on EFUSE */
	__gpufreq_compute_avs();
	/* apply Segment/Aging/AVS/... adjustment to signed OPP table  */
	__gpufreq_apply_adjustment();

	/* after these, GPU signed table are settled down */

	/* init working table, based on signed table */
	for (i = 0; i < g_gpu.opp_num; i++) {
		j = i + g_gpu.segment_upbound;
		g_gpu.working_table[i] = g_gpu.signed_table[j];
		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].margin);
	}
	for (i = 0; i < g_stack.opp_num; i++) {
		j = i + g_stack.segment_upbound;
		g_stack.working_table[i] = g_stack.signed_table[j];
		GPUFREQ_LOGD("STACK[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_stack.working_table[i].freq, g_stack.working_table[i].volt,
			g_stack.working_table[i].vsram, g_stack.working_table[i].margin);
	}

	/* set power info to working table */
	__gpufreq_measure_power();

	/* update DVFS constraint */
	__gpufreq_update_springboard();
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

/* Brisket: auto voltage frequency correlation */
static void __gpufreq_init_brisket(void)
{
#if GPUFREQ_BRISKET_BYPASS_MODE
	DRV_WriteReg32(MFG_RPC_BRISKET_TOP_AO_CFG_0, 0x00000003);
	DRV_WriteReg32(MFG_RPC_BRISKET_ST0_AO_CFG_0, 0x00000003);
	DRV_WriteReg32(MFG_RPC_BRISKET_ST1_AO_CFG_0, 0x00000003);
	DRV_WriteReg32(MFG_RPC_BRISKET_ST3_AO_CFG_0, 0x00000003);
	DRV_WriteReg32(MFG_RPC_BRISKET_ST4_AO_CFG_0, 0x00000003);
	DRV_WriteReg32(MFG_RPC_BRISKET_ST5_AO_CFG_0, 0x00000003);
	DRV_WriteReg32(MFG_RPC_BRISKET_ST6_AO_CFG_0, 0x00000003);
#endif /* GPUFREQ_BRISKET_BYPASS_MODE */

	GPUFREQ_LOGI("%s=0x%x, %s=0x%x, %s=0x%x, %s=0x%x",
		"BRISKET_TOP", DRV_Reg32(MFG_RPC_BRISKET_TOP_AO_CFG_0),
		"BRISKET_ST0", DRV_Reg32(MFG_RPC_BRISKET_ST0_AO_CFG_0),
		"BRISKET_ST1", DRV_Reg32(MFG_RPC_BRISKET_ST1_AO_CFG_0),
		"BRISKET_ST3", DRV_Reg32(MFG_RPC_BRISKET_ST3_AO_CFG_0));
	GPUFREQ_LOGI("%s=0x%x, %s=0x%x, %s=0x%x",
		"BRISKET_ST4", DRV_Reg32(MFG_RPC_BRISKET_ST4_AO_CFG_0),
		"BRISKET_ST5", DRV_Reg32(MFG_RPC_BRISKET_ST5_AO_CFG_0),
		"BRISKET_ST6", DRV_Reg32(MFG_RPC_BRISKET_ST6_AO_CFG_0));
}

/* HBVC: high bandwidth voltage controller, let HW control voltage directly */
static void __gpufreq_init_hbvc(void)
{
#if GPUFREQ_HBVC_ENABLE
	// todo: HBVC control reg
#endif /* GPUFREQ_HBVC_ENABLE */
}

static void __gpufreq_init_leakage_info(void)
{
	/* GPU RT */
	g_gpu.lkg_rt_info = 0;

	/* STACK RT */
	g_stack.lkg_rt_info = LKG_RT_STACK_FAKE_EFUSE;

	/* SRAM RT */
	g_gpu.lkg_rt_info_sram = 0;
	g_stack.lkg_rt_info_sram = 0;

	GPUFREQ_LOGI("[RT LKG] GPU: %d, STACK: %d, SRAM: %d (mA)",
		g_gpu.lkg_rt_info, g_stack.lkg_rt_info, g_stack.lkg_rt_info_sram);

	/* GPU HT */
	g_gpu.lkg_ht_info = 0;

	/* STACK HT */
	g_stack.lkg_ht_info = LKG_HT_STACK_FAKE_EFUSE;

	/* SRAM HT */
	g_gpu.lkg_ht_info_sram = 0;
	g_stack.lkg_ht_info_sram = 0;

	GPUFREQ_LOGI("[HT LKG] GPU: %d, STACK: %d, SRAM: %d (mA)",
		g_gpu.lkg_ht_info, g_stack.lkg_ht_info, g_stack.lkg_ht_info_sram);
}

static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_stack.segment_id;

	switch (segment_id) {
	default:
		g_shader_present = GPU_SHADER_PRESENT_12;
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: 0x%08x", segment_id, g_shader_present);
}

static void __gpufreq_init_segment_id(void)
{
	unsigned int efuse_id = 0x0;
	unsigned int segment_id = 0;

	switch (efuse_id) {
	default:
		segment_id = MT6989_SEGMENT;
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_gpu.segment_id = segment_id;
	g_stack.segment_id = segment_id;
}

static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%lx", (unsigned long)pdev);

	g_clk = kzalloc(sizeof(struct gpufreq_clk_info), GFP_KERNEL);
	if (!g_clk) {
		__gpufreq_abort("fail to alloc g_clk (%dB)",
			sizeof(struct gpufreq_clk_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_clk->clk_mfg_top_pll = devm_clk_get(&pdev->dev, "clk_mfg_top_pll");
	if (IS_ERR(g_clk->clk_mfg_top_pll)) {
		ret = PTR_ERR(g_clk->clk_mfg_top_pll);
		__gpufreq_abort("fail to get clk_mfg_top_pll (%ld)", ret);
		goto done;
	}

	g_clk->clk_mfg_sc0_pll = devm_clk_get(&pdev->dev, "clk_mfg_sc0_pll");
	if (IS_ERR(g_clk->clk_mfg_sc0_pll)) {
		ret = PTR_ERR(g_clk->clk_mfg_sc0_pll);
		__gpufreq_abort("fail to get clk_mfg_sc0_pll (%ld)", ret);
		goto done;
	}

	g_clk->clk_mfg_sc1_pll = devm_clk_get(&pdev->dev, "clk_mfg_sc1_pll");
	if (IS_ERR(g_clk->clk_mfg_sc1_pll)) {
		ret = PTR_ERR(g_clk->clk_mfg_sc1_pll);
		__gpufreq_abort("fail to get clk_mfg_sc1_pll (%ld)", ret);
		goto done;
	}

	g_clk->clk_mfg_ref_sel = devm_clk_get(&pdev->dev, "clk_mfg_ref_sel");
	if (IS_ERR(g_clk->clk_mfg_ref_sel)) {
		ret = PTR_ERR(g_clk->clk_mfg_ref_sel);
		__gpufreq_abort("fail to get clk_mfg_ref_sel (%ld)", ret);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%lx", (unsigned long)pdev);

	g_pmic = kzalloc(sizeof(struct gpufreq_pmic_info), GFP_KERNEL);
	if (!g_pmic) {
		__gpufreq_abort("fail to alloc g_pmic (%dB)", sizeof(struct gpufreq_pmic_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_pmic->reg_vgpu = devm_regulator_get_optional(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vstack = devm_regulator_get_optional(&pdev->dev, "vstack");
	if (IS_ERR(g_pmic->reg_vstack)) {
		ret = PTR_ERR(g_pmic->reg_vstack);
		__gpufreq_abort("fail to get VSATCK (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vsram = devm_regulator_get_optional(&pdev->dev, "vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		g_pmic->reg_vsram = devm_regulator_get_optional(&pdev->dev, "vsram-plus");
		if (IS_ERR(g_pmic->reg_vsram)) {
			ret = PTR_ERR(g_pmic->reg_vsram);
			__gpufreq_abort("fail to get VSRAM_PLUS (%ld)", ret);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	if (!gpufreq_dev) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (!of_wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* feature config */
	g_avs_enable = GPUFREQ_AVS_ENABLE;
	g_gpm1_2_mode = GPUFREQ_GPM1_2_ENABLE;
	g_gpm3_0_mode = GPUFREQ_GPM3_0_ENABLE;
	g_dfd2_0_mode = GPUFREQ_DFD2_0_ENABLE;
	g_dfd3_6_mode = GPUFREQ_DFD3_6_ENABLE;
	g_test_mode = true;
	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (!g_mali_base) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_cg_config");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_CG_CONFIG");
		goto done;
	}
	g_mfg_cg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_cg_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_CG_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0400 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc0");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC0");
		goto done;
	}
	g_mfg_pll_sc0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc0_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC0: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0800 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll_sc1");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL_SC1");
		goto done;
	}
	g_mfg_pll_sc1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_pll_sc1_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL_SC1: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_rpc_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FC0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_axuser");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_AXUSER");
		goto done;
	}
	g_mfg_axuser_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_axuser_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_AXUSER: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FB1000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_brcast");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_BRCAST");
		goto done;
	}
	g_mfg_brcast_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_brcast_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_BRCAST: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA1000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vgpu_devapc_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_VGPU_DEVAPC_AO");
		goto done;
	}
	g_mfg_vgpu_devapc_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_vgpu_devapc_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_VGPU_DEVAPC_AO: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA2000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vgpu_devapc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_VGPU_DEVAPC");
		goto done;
	}
	g_mfg_vgpu_devapc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_vgpu_devapc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_VGPU_DEVAPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x13A00000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_smmu");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SMMU");
		goto done;
	}
	g_mfg_smmu_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_smmu_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SMMU: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F50000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_hbvc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_HBVC");
		goto done;
	}
	g_mfg_hbvc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_hbvc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_HBVC: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FB0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_top");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_TOP");
		goto done;
	}
	g_brisket_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_top_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_TOP: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E1C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_st0");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_ST0");
		goto done;
	}
	g_brisket_st0_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_st0_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_ST0: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E2C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_st1");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_ST1");
		goto done;
	}
	g_brisket_st1_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_st1_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_ST1: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E4C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_st3");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_ST3");
		goto done;
	}
	g_brisket_st3_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_st3_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_ST3: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E5C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_st4");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_ST4");
		goto done;
	}
	g_brisket_st4_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_st4_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_ST4: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E6C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_st5");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_ST5");
		goto done;
	}
	g_brisket_st5_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_st5_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_ST5: 0x%llx", res->start);
		goto done;
	}

	/* 0x13E7C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "brisket_st6");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource BRISKET_ST6");
		goto done;
	}
	g_brisket_st6_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_brisket_st6_base)) {
		GPUFREQ_LOGE("fail to ioremap BRISKET_ST6: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sleep) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_topckgen_base) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ifrbus_ao");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource IFRBUS_AO");
		goto done;
	}
	g_ifrbus_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_ifrbus_ao_base) {
		GPUFREQ_LOGE("fail to ioremap IFRBUS_AO: 0x%llx", res->start);
		goto done;
	}

	/* 0x10219000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EMI");
		goto done;
	}
	g_emi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_emi_base) {
		GPUFREQ_LOGE("fail to ioremap EMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021D000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sub_emi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SUB_EMI");
		goto done;
	}
	g_sub_emi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sub_emi_base) {
		GPUFREQ_LOGE("fail to ioremap SUB_EMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emicfg_base) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG");
		goto done;
	}
	g_sth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emicfg_base) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emicfg_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x1030E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emicfg_ao_mem_base) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x10023000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_infra_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002B000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao1_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO1_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao1_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_infra_ao1_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO1_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nth_emi_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10028000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emi_ao_debug_ctrl");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource STH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_sth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_sth_emi_ao_debug_ctrl) {
		GPUFREQ_LOGE("fail to ioremap STH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x11F10000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource EFUSE");
		goto done;
	}
	g_efuse_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_efuse_base) {
		GPUFREQ_LOGE("fail to ioremap EFUSE: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FBC000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_secure");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource MFG_SECURE");
		goto done;
	}
	g_mfg_secure_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_mfg_secure_base) {
		GPUFREQ_LOGE("fail to ioremap MFG_SECURE: 0x%llx", res->start);
		goto done;
	}

	/* 0x1000D000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "drm_debug");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource DRM_DEBUG");
		goto done;
	}
	g_drm_debug_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_drm_debug_base) {
		GPUFREQ_LOGE("fail to ioremap DRM_DEBUG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi32_mi33_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI32_MI33_SMI");
		goto done;
	}
	g_nemi_mi32_mi33_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_nemi_mi32_mi33_smi) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI32_MI33_SMI: 0x%llx", res->start);
		goto done;
	}

	/* 0x10309000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi32_mi33_smi");
	if (!res) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI32_MI33_SMI");
		goto done;
	}
	g_semi_mi32_mi33_smi = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (!g_semi_mi32_mi33_smi) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI32_MI33_SMI: 0x%llx", res->start);
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

	/* 0x0C0DF7E0 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mcdi_mbox");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MCDI_MBOX");
		goto done;
	}
	g_mcdi_mbox_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mcdi_mbox_base)) {
		GPUFREQ_LOGE("fail to ioremap MCDI_MBOX: 0x%llx", res->start);
		goto done;
	}

	/* 0x0C000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mcusys_par_wrap");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MCUSYS_PAR_WRAP");
		goto done;
	}
	g_mcusys_par_wrap_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mcusys_par_wrap_base)) {
		GPUFREQ_LOGE("fail to ioremap MCUSYS_PAR_WRAP: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

/* API: skip gpufreq driver probe if in bringup state */
static unsigned int __gpufreq_bringup(void)
{
	struct device_node *of_wrapper = NULL;
	unsigned int bringup_state = false;

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (!of_wrapper) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node, treat as bringup");
		return true;
	}

	/* check bringup state by dts */
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &bringup_state);

	return bringup_state;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		__gpufreq_dump_bringup_status(pdev);
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}

	/* skip most of probe in EB mode */
	if (g_gpueb_support) {
		GPUFREQ_LOGI("gpufreq platform probe only init reg/pmic/fp in EB mode");
		goto register_fp;
	}

	/* init clock source */
	ret = __gpufreq_init_clk(pdev);
	if (ret) {
		GPUFREQ_LOGE("fail to init clk (%d)", ret);
		goto done;
	}

	/* init segment id */
	__gpufreq_init_segment_id();

	/* init shader present */
	__gpufreq_init_shader_present();

	/* init leakage power info */
	__gpufreq_init_leakage_info();

	/* init HBVC */
	__gpufreq_init_hbvc();

	/* init Brisket */
	__gpufreq_init_brisket();

	/* power on to init first OPP index */
	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (ret < 0) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
		goto done;
	}

	/* init OPP table */
	__gpufreq_init_opp_table();

	/* init first OPP index by current freq and volt */
	ret = __gpufreq_init_opp_idx();
	if (ret) {
		GPUFREQ_LOGE("fail to init OPP index (%d)", ret);
		goto done;
	}

	/* power off after init first OPP index */
	if (__gpufreq_power_ctrl_enable())
		__gpufreq_power_control(GPU_PWR_OFF);
	/* never power off if power control is disabled */
	else
		GPUFREQ_LOGI("power control always on");

	/* init GPM 3.0 */
	__gpufreq_init_gpm3_0_table();

register_fp:
	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	if (g_gpueb_support)
		gpufreq_register_gpufreq_fp(&platform_eb_fp);
	else
		gpufreq_register_gpufreq_fp(&platform_ap_fp);

	/* init gpuppm */
	ret = gpuppm_init(TARGET_STACK, g_gpueb_support);
	if (ret) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	if (!g_gpueb_support) {
#if !GPUFREQ_HW_DELSEL_ENABLE
		gpuppm_set_limit(TARGET_STACK, LIMIT_SRAMRC,
			GPUPPM_DEFAULT_IDX, NO_DELSEL_FLOOR_VSTACK, false);
#endif /* GPUFREQ_HW_DELSEL_ENABLE */
#if !GPUFREQ_GPM3_0_ENABLE
		gpuppm_set_limit(TARGET_STACK, LIMIT_GPM3,
			NO_GPM3_CEILING_OPP, GPUPPM_DEFAULT_IDX, false);
#endif /* GPUFREQ_GPM3_0_ENABLE */
	}

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	kfree(g_gpu.working_table);
	kfree(g_stack.working_table);
	kfree(g_clk);
	kfree(g_pmic);

	return GPUFREQ_SUCCESS;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (ret) {
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
