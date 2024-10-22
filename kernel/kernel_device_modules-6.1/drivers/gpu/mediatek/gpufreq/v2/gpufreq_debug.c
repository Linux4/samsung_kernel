// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpufreq_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#include <gpufreq_v2.h>
#include <gpufreq_mssv.h>
#include <gpufreq_debug.h>
#include <gpufreq_history.h>

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static unsigned int g_dual_buck;
static unsigned int g_gpueb_support;
static unsigned int g_debug_power_state;
static unsigned int g_debug_margin_mode;
static int g_fixed_oppidx_gpu;
static int g_fixed_oppidx_stack;
static unsigned int g_fixed_fgpu;
static unsigned int g_fixed_vgpu;
static unsigned int g_fixed_fstack;
static unsigned int g_fixed_vstack;
static const struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_debug_lock);

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
#if defined(CONFIG_PROC_FS)
static int gpufreq_status_proc_show(struct seq_file *m, void *v)
{
	unsigned long long power_time = 0;
	struct gpufreq_ptp3_shared_status ptp3_status = {};

	mutex_lock(&gpufreq_debug_lock);

	gpufreq_update_debug_opp_info();

	seq_printf(m,
		"[GPUFREQ-DEBUG] Current Status of GPUFREQ\n");
	seq_printf(m,
		"%-16s Index: %2d, Freq: %7d, Volt: %6d, Vsram: %6d\n",
		"[GPU   OPP]",
		g_shared_status->cur_oppidx_gpu,
		g_shared_status->cur_fgpu,
		g_shared_status->cur_vgpu,
		g_shared_status->cur_vsram_gpu);
	if (g_dual_buck)
		seq_printf(m,
			"%-16s Index: %2d, Freq: %7d, Volt: %6d, Vsram: %6d\n",
			"[STACK OPP]",
			g_shared_status->cur_oppidx_stack,
			g_shared_status->cur_fstack,
			g_shared_status->cur_vstack,
			g_shared_status->cur_vsram_stack);

	seq_printf(m,
		"%-16s FmeterFreq: %7d, Con1Freq: %7d, ReguVolt: %6d, ReguVsram: %6d\n",
		"[GPU   REALOPP]",
		g_shared_status->cur_fmeter_fgpu,
		g_shared_status->cur_con1_fgpu,
		g_shared_status->cur_regulator_vgpu,
		g_shared_status->cur_regulator_vsram_gpu);
	if (g_dual_buck)
		seq_printf(m,
			"%-16s FmeterFreq: %7d, Con1Freq: %7d, ReguVolt: %6d, ReguVsram: %6d\n",
			"[STACK REALOPP]",
			g_shared_status->cur_fmeter_fstack,
			g_shared_status->cur_con1_fstack,
			g_shared_status->cur_regulator_vstack,
			g_shared_status->cur_regulator_vsram_stack);

	seq_printf(m,
		"%-16s SegmentID: %d, WorkingOPPNum: %2d, SignedOPPNum: %2d\n",
		"[GPU   Segment]",
		g_shared_status->segment_id,
		g_shared_status->opp_num_gpu,
		g_shared_status->signed_opp_num_gpu);
	if (g_dual_buck)
		seq_printf(m,
			"%-16s SegmentID: %d, WorkingOPPNum: %2d, SignedOPPNum: %2d\n",
			"[STACK Segment]",
			g_shared_status->segment_id,
			g_shared_status->opp_num_stack,
			g_shared_status->signed_opp_num_stack);

	seq_printf(m,
		"%-16s LimitIndex: %2d, Limiter: %2d, Priority: %2d\n",
		"[PPM Ceiling]",
		g_shared_status->cur_ceiling,
		g_shared_status->cur_c_limiter,
		g_shared_status->cur_c_priority);
	seq_printf(m,
		"%-16s LimitIndex: %2d, Limiter: %2d, Priority: %2d\n",
		"[PPM Floor]",
		g_shared_status->cur_floor,
		g_shared_status->cur_f_limiter,
		g_shared_status->cur_f_priority);

	power_time = g_shared_status->power_time_h;
	power_time = (power_time << 32) | g_shared_status->power_time_l;
	seq_printf(m,
		"%-16s PowerCount: %d, Active: %d, CG: %d, MTCMOS: %d, BUCK: %d\n",
		"[Power State]",
		g_shared_status->power_count,
		g_shared_status->active_count,
		g_shared_status->cg_count,
		g_shared_status->mtcmos_count,
		g_shared_status->buck_count);
	seq_printf(m,
		"%-16s Timestamp: %lld, DVFSState: 0x%04x\n",
		"[Power State]",
		power_time,
		g_shared_status->dvfs_state);
	seq_printf(m,
		"%-16s PowerStatus: 0x%08x, ShaderPresent: 0x%08x\n",
		"[Power State]",
		g_shared_status->mfg_pwr_status,
		g_shared_status->shader_present);

	seq_printf(m,
		"%-16s DualBuck: %s, GPUEBSupport: %s, StressTest: %s\n",
		"[MFGSYS Config]",
		g_dual_buck ? "True" : "False",
		g_gpueb_support ? "On" : "Off",
		g_shared_status->stress_test == STRESS_RANDOM ? "Random" :
		g_shared_status->stress_test == STRESS_TRAVERSE ? "Traverse" :
		g_shared_status->stress_test == STRESS_MAX_MIN ? "Max_Min" : "Off");
	seq_printf(m,
		"%-16s AgingMargin: %s, AVSMargin: %s, GPM1.0: %s, GPM3.0: %s\n",
		"[MFGSYS Config]",
		g_shared_status->aging_margin ? "On" : "Off",
		g_shared_status->avs_margin ? "On" : "Off",
		g_shared_status->gpm1_mode ? "On" : "Off",
		g_shared_status->gpm3_mode ? "On" : "Off");
	seq_printf(m,
		"%-16s Temperature: %d'C, GPUTemperComp: %d/%d, STACKTemperComp: %d/%d\n",
		"[MFGSYS Config]",
		g_shared_status->temperature,
		g_shared_status->temper_comp_norm_gpu,
		g_shared_status->temper_comp_high_gpu,
		g_shared_status->temper_comp_norm_stack,
		g_shared_status->temper_comp_high_stack);
	seq_printf(m,
		"%-16s DBGVer: 0x%08x, PTPVer: 0x%04x, SBVer: 0x%04x\n",
		"[MFGSYS Config]",
		g_shared_status->dbg_version,
		g_shared_status->ptp_version,
		g_shared_status->sb_version);

	ptp3_status = g_shared_status->ptp3_status;
	seq_printf(m,
		"%-16s PTP3: %s, DVFSMode: %s, ImaxProt: %s, HBVCFreq: %s, HBVCVolt: %s\n",
		"[PTP3 Config]",
		ptp3_status.ptp3_mode ? "On" : "Off",
		(ptp3_status.dvfs_mode == HW_DUAL_LOOP_DVFS ? "HW_LOOP" :
		(ptp3_status.dvfs_mode == SW_DUAL_LOOP_DVFS ? "SW_LOOP" : "LEGACY")),
		(ptp3_status.gpm3_prot_mode == GPM3_5_IMAX_PROT ? "3.5" :
		(ptp3_status.gpm3_prot_mode == GPM3_0_IMAX_PROT ? "3.0" : "Off")),
		ptp3_status.hbvc_freq_ctrl_support ? "On" : "Off",
		ptp3_status.hbvc_volt_ctrl_support ? "On" : "Off");
	seq_printf(m,
		"%-16s BRCAST: %s, DELSEL: %s, PreOC: %s\n",
		"[PTP3 Config]",
		ptp3_status.brcast_mode == BRCAST_WITH_AUTO_DMA ? "AutoDMA" : "SW",
		ptp3_status.delsel_mode == HW_DELSEL ? "HW" : "SW",
		ptp3_status.hbvc_preoc_mode ? "On" : "Off");
	seq_printf(m,
		"%-16s FLL: %s, ATMC: %s, Vmeter: %s, Tmeter: %s, CPmeter: %s, CTT: %s\n",
		"[PTP3 Config]",
		ptp3_status.brisket_fll_mode ? "On" : "Off",
		(ptp3_status.brisket_atmc_mode == BRISKET_ENABLE ? "On" :
		(ptp3_status.brisket_atmc_mode == BRISKET_UNSUPPORTED ? "NSup" : "Off")),
		(ptp3_status.brisket_vmeter_mode == BRISKET_ENABLE ? "On" :
		(ptp3_status.brisket_vmeter_mode == BRISKET_UNSUPPORTED ? "NSup" : "Off")),
		(ptp3_status.brisket_tmeter_mode == BRISKET_ENABLE ? "On" :
		(ptp3_status.brisket_tmeter_mode == BRISKET_UNSUPPORTED ? "NSup" : "Off")),
		ptp3_status.brisket_cpmeter_mode ? "On" : "Off",
		ptp3_status.brisket_ctt_mode ? "On" : "Off");
	seq_printf(m,
		"%-16s InFreq: %d/%d, OutFreq: %d/%d, CC: %d/%d, FC: %d/%d\n",
		"[PTP3 Config]",
		g_shared_status->ptp3_info.infreq0, g_shared_status->ptp3_info.infreq1,
		g_shared_status->ptp3_info.outfreq0, g_shared_status->ptp3_info.outfreq1,
		g_shared_status->ptp3_info.hw_cc, g_shared_status->ptp3_info.sw_cc,
		g_shared_status->ptp3_info.hw_fc, g_shared_status->ptp3_info.sw_fc);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static int gpu_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_num = g_shared_status->opp_num_gpu;
	opp_table = g_shared_status->working_table_gpu;
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU working OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		if (g_shared_status->test_mode)
			seq_printf(m,
				"[%02d] freq: %7d, volt: %6d, vsram: %6d, posdiv: %1d, margin: %5d, power: %5d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram, opp_table[i].posdiv,
				opp_table[i].margin, opp_table[i].power);
		else
			seq_printf(m,
				"[%02d] freq: %7d, volt: %6d, vsram: %6d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int stack_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_num = g_shared_status->opp_num_stack;
	opp_table = g_shared_status->working_table_stack;
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK working OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		if (g_shared_status->test_mode)
			seq_printf(m,
				"[%02d] freq: %7d, volt: %6d, vsram: %6d, posdiv: %1d, margin: %5d, power: %5d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram, opp_table[i].posdiv,
				opp_table[i].margin, opp_table[i].power);
		else
			seq_printf(m,
				"[%02d] freq: %7d, volt: %6d, vsram: %6d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int gpu_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_shared_status->test_mode) {
		opp_num = g_shared_status->signed_opp_num_gpu;
		opp_table = g_shared_status->signed_table_gpu;
	} else {
		opp_num = g_shared_status->opp_num_gpu;
		opp_table = g_shared_status->working_table_gpu;
	}
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU signed OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		if (g_shared_status->test_mode)
			seq_printf(m,
				"[%02d*] freq: %7d, volt: %6d, vsram: %6d, posdiv: %1d, margin: %5d, power: %5d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram, opp_table[i].posdiv,
				opp_table[i].margin, opp_table[i].power);
		else
			seq_printf(m,
				"[%02d*] freq: %7d, volt: %6d, vsram: %6d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int stack_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_shared_status->test_mode) {
		opp_num = g_shared_status->signed_opp_num_stack;
		opp_table = g_shared_status->signed_table_stack;
	} else {
		opp_num = g_shared_status->opp_num_stack;
		opp_table = g_shared_status->working_table_stack;
	}
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK signed OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		if (g_shared_status->test_mode)
			seq_printf(m,
				"[%02d*] freq: %7d, volt: %6d, vsram: %6d, posdiv: %1d, margin: %5d, power: %5d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram, opp_table[i].posdiv,
				opp_table[i].margin, opp_table[i].power);
		else
			seq_printf(m,
				"[%02d*] freq: %7d, volt: %6d, vsram: %6d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int limit_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpuppm_limit_info *limit_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	limit_table = g_shared_status->limit_table;
	if (!limit_table) {
		GPUFREQ_LOGE("fail to get limit table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	seq_printf(m, "%4s %15s %11s %10s %8s %11s %11s\n",
		"[id]", "[name]", "[priority]",
		"[ceiling]", "[floor]", "[c_enable]", "[f_enable]");

	for (i = 0; i < LIMIT_NUM; i++) {
		seq_printf(m, "%4d %15s %11d %10d %8d %11d %11d\n",
			i, limit_table[i].name,
			limit_table[i].priority,
			limit_table[i].ceiling,
			limit_table[i].floor,
			limit_table[i].c_enable,
			limit_table[i].f_enable);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static ssize_t limit_table_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64], cmd[64];
	unsigned int len = 0;
	int limiter = 0, ceiling = 0, floor = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%6s %2d %2d %2d", cmd, &limiter, &ceiling, &floor) == 4) {
		if (sysfs_streq(cmd, "set")) {
			ret = gpufreq_set_limit(TARGET_DEFAULT, limiter, ceiling, floor);
			if (ret)
				GPUFREQ_LOGE("fail to set debug limit index (%d)", ret);
		}
		else if (sysfs_streq(cmd, "switch")) {
			ret = gpufreq_switch_limit(TARGET_DEFAULT, limiter, ceiling, floor);
			if (ret)
				GPUFREQ_LOGE("fail to switch debug limit option (%d)", ret);
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept OPP index */
static int fix_target_opp_index_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_dual_buck) {
		if ((g_fixed_oppidx_gpu != -1) && (g_fixed_oppidx_stack != -1))
			seq_printf(m, "[GPUFREQ-DEBUG] fix GPU/STACK OPP index: %d/%d\n",
				g_fixed_oppidx_gpu, g_fixed_oppidx_stack);
		else
			seq_puts(m, "[GPUFREQ-DEBUG] fix GPU/STACK OPP index is disabled\n");
	} else {
		if (g_fixed_oppidx_gpu != -1)
			seq_printf(m, "[GPUFREQ-DEBUG] fix GPU OPP index: %d\n", g_fixed_oppidx_gpu);
		else
			seq_puts(m, "[GPUFREQ-DEBUG] fix GPU OPP index is disabled\n");
	}

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: keep OPP index
 * -1: free run
 * others: OPP index to be kept
 */
static ssize_t fix_target_opp_index_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;
	int oppidx = 0, opp_num = -1;
	int oppidx_gpu = 0, oppidx_stack = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "min")) {
		if (g_dual_buck)
			opp_num = g_shared_status->opp_num_stack;
		else
			opp_num = g_shared_status->opp_num_gpu;
		if (opp_num > 0) {
			oppidx = opp_num - 1;
			ret = gpufreq_fix_target_oppidx(TARGET_DEFAULT, oppidx);
			if (ret)
				GPUFREQ_LOGE("fail to fix OPP index: %d (%d)", oppidx, ret);
			else {
				g_fixed_oppidx_gpu = oppidx;
				g_fixed_oppidx_stack = oppidx;
			}
		}
	} else if (sscanf(buf, "%2d %2d", &oppidx_gpu, &oppidx_stack) == 2) {
		ret = gpufreq_fix_dual_target_oppidx(oppidx_gpu, oppidx_stack);
		if (ret)
			GPUFREQ_LOGE("fail to fix GPU/STACK OPP index: %d/%d (%d)",
				oppidx_gpu, oppidx_stack, ret);
		else {
			g_fixed_oppidx_gpu = oppidx_gpu;
			g_fixed_oppidx_stack = oppidx_stack;
		}
	} else if (sscanf(buf, "%2d", &oppidx) == 1) {
		ret = gpufreq_fix_target_oppidx(TARGET_DEFAULT, oppidx);
		if (ret)
			GPUFREQ_LOGE("fail to fix OPP index: %d (%d)", oppidx, ret);
		else {
			g_fixed_oppidx_gpu = oppidx;
			g_fixed_oppidx_stack = oppidx;
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept freq and volt */
static int fix_custom_freq_volt_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_dual_buck) {
		if (g_fixed_fgpu && g_fixed_vgpu && g_fixed_fstack && g_fixed_vstack)
			seq_printf(m, "[GPUFREQ-DEBUG] fix GPU F(%d)/V(%d), STACK F(%d)/V(%d)\n",
				g_fixed_fgpu, g_fixed_vgpu, g_fixed_fstack, g_fixed_vstack);
		else
			seq_puts(m, "[GPUFREQ-DEBUG] fix GPU F/V, STACK F/V is disabled\n");
	} else {
		if (g_fixed_fgpu && g_fixed_vgpu)
			seq_printf(m, "[GPUFREQ-DEBUG] fix GPU F(%d)/V(%d)\n",
				g_fixed_fgpu, g_fixed_vgpu);
		else
			seq_puts(m, "[GPUFREQ-DEBUG] fix GPU F/V is disabled\n");
	}

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: keep freq and volt
 * 0 0: free run
 * others others: freq and volt to be kept
 */
static ssize_t fix_custom_freq_volt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;
	unsigned int freq = 0, volt = 0;
	unsigned int fgpu = 0, vgpu = 0, fstack = 0, vstack = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%7d %7d %7d %7d", &fgpu, &vgpu, &fstack, &vstack) == 4) {
		ret = gpufreq_fix_dual_custom_freq_volt(fgpu, vgpu, fstack, vstack);
		if (ret)
			GPUFREQ_LOGE("fail to fix GPU F(%d)/V(%d), STACK F(%d)/V(%d) (%d)",
				fgpu, vgpu, fstack, vstack, ret);
		else {
			g_fixed_fgpu = fgpu;
			g_fixed_vgpu = vgpu;
			g_fixed_fstack = fstack;
			g_fixed_vstack = vstack;
		}
	} else if (sscanf(buf, "%7d %7d", &freq, &volt) == 2) {
		ret = gpufreq_fix_custom_freq_volt(TARGET_DEFAULT, freq, volt);
		if (ret)
			GPUFREQ_LOGE("fail to fix F(%d)/V(%d) (%d)", freq, volt, ret);
		else {
			if (g_dual_buck) {
				g_fixed_fgpu = g_shared_status->cur_fgpu;
				g_fixed_vgpu = g_shared_status->cur_vgpu;
				g_fixed_fstack = freq;
				g_fixed_vstack = volt;
			} else {
				g_fixed_fgpu = freq;
				g_fixed_vgpu = volt;
			}
		}
	}

	mutex_unlock(&gpufreq_debug_lock);
done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current power state */
static int mfgsys_power_control_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_debug_power_state)
		seq_puts(m, "[GPUFREQ-DEBUG] MFGSYS is DBG_POWER_ON\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] MFGSYS is DBG_POWER_OFF\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: additionally control power of MFGSYS once to achieve AO state
 * power_on: power on once
 * power_off: power off once
 */
static ssize_t mfgsys_power_control_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "power_on") && g_debug_power_state == GPU_PWR_OFF) {
		ret = gpufreq_power_control(GPU_PWR_ON);
		if (ret < 0)
			GPUFREQ_LOGE("fail to power on MFGSYS (%d)", ret);
		else
			g_debug_power_state = GPU_PWR_ON;
	} else if (sysfs_streq(buf, "power_off") && g_debug_power_state == GPU_PWR_ON) {
		ret = gpufreq_power_control(GPU_PWR_OFF);
		if (ret < 0)
			GPUFREQ_LOGE("fail to power off MFGSYS (%d)", ret);
		else
			g_debug_power_state = GPU_PWR_OFF;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current info of aging*/
static int asensor_info_proc_show(struct seq_file *m, void *v)
{
	struct gpufreq_asensor_info asensor_info = {};

	mutex_lock(&gpufreq_debug_lock);

	asensor_info = g_shared_status->asensor_info;

	seq_printf(m,
		"Aging table index: %d, MOST_AGRRESIVE_AGING_TABLE_ID: %d\n",
		asensor_info.aging_table_idx, asensor_info.aging_table_idx_agrresive);
	seq_printf(m,
		"efuse1(0x%08x): 0x%08x, efuse2(0x%08x): 0x%08x\n",
		asensor_info.efuse1_addr, asensor_info.efuse1,
		asensor_info.efuse2_addr, asensor_info.efuse2);
	seq_printf(m,
		"efuse3(0x%08x): 0x%08x, efuse4(0x%08x): 0x%08x\n",
		asensor_info.efuse3_addr, asensor_info.efuse3,
		asensor_info.efuse4_addr, asensor_info.efuse4);
	seq_printf(m,
		"a_t0_efuse1: %d, a_t0_efuse2: %d, a_t0_efuse3: %d, a_t0_efuse4: %d\n",
		asensor_info.a_t0_efuse1, asensor_info.a_t0_efuse2,
		asensor_info.a_t0_efuse3, asensor_info.a_t0_efuse4);
	seq_printf(m,
		"a_tn_sensor1: %d, a_tn_sensor2: %d, a_tn_sensor3: %d, a_tn_sensor4: %d\n",
		asensor_info.a_tn_sensor1, asensor_info.a_tn_sensor2,
		asensor_info.a_tn_sensor3, asensor_info.a_tn_sensor4);
	seq_printf(m,
		"a_diff1: %d, a_diff2: %d, a_diff3: %d, a_diff4: %d\n",
		asensor_info.a_diff1, asensor_info.a_diff2,
		asensor_info.a_diff3, asensor_info.a_diff4);
	seq_printf(m,
		"tj_max: %d, lvts5_0_y_temperature: %d, leakage_power: %d\n",
		asensor_info.tj_max, asensor_info.lvts5_0_y_temperature,
		asensor_info.leakage_power);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static int mfgsys_config_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_adj_info *aging_table_gpu = NULL;
	const struct gpufreq_adj_info *aging_table_stack = NULL;
	const struct gpufreq_adj_info *avs_table_gpu = NULL;
	const struct gpufreq_adj_info *avs_table_stack = NULL;
	const struct gpufreq_gpm3_info *gpm3_table = NULL;
	int i = 0;
	int adj_num = GPUFREQ_MAX_ADJ_NUM;
	int gpm3_num = GPUFREQ_MAX_GPM3_NUM;

	mutex_lock(&gpufreq_debug_lock);

	aging_table_gpu = g_shared_status->aging_table_gpu;
	aging_table_stack = g_shared_status->aging_table_stack;
	avs_table_gpu = g_shared_status->avs_table_gpu;
	avs_table_stack = g_shared_status->avs_table_stack;
	gpm3_table = g_shared_status->gpm3_table;

	seq_printf(m, "%-8s AgingSensor: %s, AgingLoad: %s, AgingMargin: %s\n",
		"[AGING]",
		g_shared_status->asensor_enable ? "On" : "Off",
		g_shared_status->aging_load ? "On" : "Off",
		g_shared_status->aging_margin ? "On" : "Off");
	seq_printf(m, "%-8s AVS: %s, AVSMargin: %s\n",
		"[AVS]",
		g_shared_status->avs_enable ? "On" : "Off",
		g_shared_status->avs_margin ? "On" : "Off");
	seq_printf(m, "%-8s GPM1.0: %s, GPM3.0: %s\n",
		"[GPM]",
		g_shared_status->gpm1_mode ? "On" : "Off",
		g_shared_status->gpm3_mode ? "On" : "Off");
	seq_printf(m, "%-8s TemperComp: %s, HTTemperComp: %s\n",
		"[TEMPER]",
		g_shared_status->temper_comp_mode ? "On" : "Off",
		g_shared_status->ht_temper_comp_mode ? "On" : "Off");
	seq_printf(m, "%-8s IPS: %s, Vmin: %d (0x%x)\n",
		"[IPS]",
		g_shared_status->ips_mode ? "On" : "Off",
		g_shared_status->ips_info.vmin_val,
		g_shared_status->ips_info.vmin_reg_val);
	seq_printf(m, "%-8s AutoKResult: %s (Trim: 0x%x, 0x%x, 0x%x)\n",
		"[IPS]",
		g_shared_status->ips_info.autok_result ? "Pass" : "Fail",
		g_shared_status->ips_info.autok_trim0,
		g_shared_status->ips_info.autok_trim1,
		g_shared_status->ips_info.autok_trim2);
	seq_printf(m, "%-8s StressTest: %s, TestMode: %s\n",
		"[Misc]",
		g_shared_status->stress_test == STRESS_RANDOM ? "Random" :
		g_shared_status->stress_test == STRESS_TRAVERSE ? "Traverse" :
		g_shared_status->stress_test == STRESS_MAX_MIN ? "Max_Min" : "Disable",
		g_shared_status->test_mode ? "On" : "Off");

	seq_puts(m, "\n[##*] [TOP Vaging] [##*] [TOP Vavs] [##*] [STK Vaging] [##*] [STK Vavs]\n");
	for (i = 0; i < adj_num; i++) {
		seq_printf(m, "[%02d*] %12d [%02d*] %10d [%02d*] %12d [%02d*] %10d\n",
			aging_table_gpu[i].oppidx, aging_table_gpu[i].volt,
			avs_table_gpu[i].oppidx, avs_table_gpu[i].volt,
			aging_table_stack[i].oppidx, aging_table_stack[i].volt,
			avs_table_stack[i].oppidx, avs_table_stack[i].volt);
	}

	seq_puts(m, "\n[Temper] [Ceiling] [I_STACK] [I_SRAM] [P_STACK]\n");
	for (i = 0; i < gpm3_num; i++) {
		seq_printf(m, "[%6d] %9d %9d %8d %9d\n",
			gpm3_table[i].temper, gpm3_table[i].ceiling,
			gpm3_table[i].i_stack, gpm3_table[i].i_sram,
			gpm3_table[i].p_stack);
	}

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static ssize_t mfgsys_config_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64], input_target[32], input_val[32];
	unsigned int len = 0;
	enum gpufreq_config_target target = CONFIG_TARGET_INVALID;
	enum gpufreq_config_value val = CONFIG_VAL_INVALID;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%14s %10s", input_target, input_val) == 2) {
		/* parsing */
		if (sysfs_streq(input_target, "test_mode")) {
			target = CONFIG_TEST_MODE;
			ret = kstrtouint(input_val, 16, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "stress_test")) {
			target = CONFIG_STRESS_TEST;
			if (sysfs_streq(input_val, "enable"))
				val = STRESS_RANDOM;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
			else if (sysfs_streq(input_val, "random"))
				val = STRESS_RANDOM;
			else if (sysfs_streq(input_val, "traverse"))
				val = STRESS_TRAVERSE;
			else if (sysfs_streq(input_val, "maxmin"))
				val = STRESS_MAX_MIN;
		} else if (sysfs_streq(input_target, "margin")) {
			target = CONFIG_MARGIN;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
			/* only allow enable/disable once */
			if (g_debug_margin_mode ^ val)
				g_debug_margin_mode = val;
			else
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "gpm1")) {
			target = CONFIG_GPM1;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
		} else if (sysfs_streq(input_target, "gpm3")) {
			target = CONFIG_GPM3;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
		} else if (sysfs_streq(input_target, "dfd")) {
			target = CONFIG_DFD;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
			else if (sysfs_streq(input_val, "force_dump"))
				val = DFD_FORCE_DUMP;
		} else if (sysfs_streq(input_target, "imax_gpu")) {
			target = CONFIG_IMAX_GPU;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "imax_stack")) {
			target = CONFIG_IMAX_STACK;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "imax_sram")) {
			target = CONFIG_IMAX_SRAM;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "pmax_stack")) {
			target = CONFIG_PMAX_STACK;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "dyn_gpu")) {
			target = CONFIG_DYN_GPU;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "dyn_stack")) {
			target = CONFIG_DYN_STACK;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "dyn_sram_gpu")) {
			target = CONFIG_DYN_SRAM_GPU;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "dyn_sram_stack")) {
			target = CONFIG_DYN_SRAM_STACK;
			ret = kstrtouint(input_val, 10, &val);
			if (ret)
				val = CONFIG_VAL_INVALID;
		} else if (sysfs_streq(input_target, "ips")) {
			target = CONFIG_IPS;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
			else if (sysfs_streq(input_val, "get"))
				val = IPS_VMIN_GET;
		} else if (sysfs_streq(input_target, "mcuetm_clk")) {
			target = CONFIG_MCUETM_CLK;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
		} else if (sysfs_streq(input_target, "ptp3")) {
			target = CONFIG_PTP3;
			if (sysfs_streq(input_val, "enable"))
				val = FEAT_ENABLE;
			else if (sysfs_streq(input_val, "disable"))
				val = FEAT_DISABLE;
		}

		/* set to mfgsys if valid */
		if (target != CONFIG_TARGET_INVALID && val != CONFIG_VAL_INVALID)
			gpufreq_set_mfgsys_config(target, val);
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

#if GPUFREQ_MSSV_TEST_MODE
static int mssv_test_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	seq_printf(m, "%-8s DVFSState: 0x%04x\n",
		"[Common]",
		g_shared_status->dvfs_state);
	seq_printf(m, "%-8s Freq: %7d, Volt: %7d, Vsram: %7d\n",
		"[GPU]",
		g_shared_status->cur_fgpu,
		g_shared_status->cur_vgpu,
		g_shared_status->cur_vsram_gpu);
	seq_printf(m, "%-8s Freq: %7d, Volt: %7d, Vsram: %7d\n",
		"[STACK]",
		g_shared_status->cur_fstack,
		g_shared_status->cur_vstack,
		g_shared_status->cur_vsram_stack);
	seq_printf(m, "%-8s %s(0x%08x): %d, %s(0x%08x): %d, %s(0x%08x): %d\n",
		"[SEL]",
		"STACK_SEL", g_shared_status->reg_stack_sel.addr,
		g_shared_status->reg_stack_sel.val,
		"TOP_DELSEL", g_shared_status->reg_top_delsel.addr,
		g_shared_status->reg_top_delsel.val,
		"STACK_DELSEL", g_shared_status->reg_stack_delsel.addr,
		g_shared_status->reg_stack_delsel.val);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static ssize_t mssv_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64], cmd[32];
	unsigned int len = 0, val = 0;
	unsigned int target = TARGET_MSSV_INVALID;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%11s %7d", cmd, &val) == 2) {
		if (sysfs_streq(cmd, "fgpu"))
			target = TARGET_MSSV_FGPU;
		else if (sysfs_streq(cmd, "vgpu"))
			target = TARGET_MSSV_VGPU;
		else if (sysfs_streq(cmd, "fstack"))
			target = TARGET_MSSV_FSTACK;
		else if (sysfs_streq(cmd, "vstack"))
			target = TARGET_MSSV_VSTACK;
		else if (sysfs_streq(cmd, "vsram"))
			target = TARGET_MSSV_VSRAM;
		else if (sysfs_streq(cmd, "stacksel"))
			target = TARGET_MSSV_STACK_SEL;
		else if (sysfs_streq(cmd, "topdelsel"))
			target = TARGET_MSSV_TOP_DELSEL;
		else if (sysfs_streq(cmd, "stackdelsel"))
			target = TARGET_MSSV_STACK_DELSEL;
		else {
			GPUFREQ_LOGE("invalid MSSV cmd: %s", cmd);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		ret = gpufreq_mssv_commit(target, val);
		if (ret)
			GPUFREQ_LOGE("fail to commit: %s, val: %d (%d)",
				cmd, val, ret);
	}

done_unlock:
	mutex_unlock(&gpufreq_debug_lock);
done:
	return (ret < 0) ? ret : count;
}
#endif /* GPUFREQ_MSSV_TEST_MODE */

/* PROCFS : initialization */
PROC_FOPS_RO(gpufreq_status);
PROC_FOPS_RO(gpu_working_opp_table);
PROC_FOPS_RO(gpu_signed_opp_table);
PROC_FOPS_RO(stack_working_opp_table);
PROC_FOPS_RO(stack_signed_opp_table);
PROC_FOPS_RO(asensor_info);
PROC_FOPS_RW(limit_table);
PROC_FOPS_RW(fix_target_opp_index);
PROC_FOPS_RW(fix_custom_freq_volt);
PROC_FOPS_RW(mfgsys_power_control);
PROC_FOPS_RW(mfgsys_config);
#if GPUFREQ_MSSV_TEST_MODE
PROC_FOPS_RW(mssv_test);
#endif /* GPUFREQ_MSSV_TEST_MODE */

static int gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct procfs_entry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct procfs_entry default_entries[] = {
		PROC_ENTRY(gpufreq_status),
		PROC_ENTRY(gpu_working_opp_table),
		PROC_ENTRY(gpu_signed_opp_table),
		PROC_ENTRY(asensor_info),
		PROC_ENTRY(limit_table),
		PROC_ENTRY(fix_target_opp_index),
		PROC_ENTRY(fix_custom_freq_volt),
		PROC_ENTRY(mfgsys_power_control),
		PROC_ENTRY(mfgsys_config),
#if GPUFREQ_MSSV_TEST_MODE
		PROC_ENTRY(mssv_test),
#endif /* GPUFREQ_MSSV_TEST_MODE */
	};

	const struct procfs_entry dualbuck_entries[] = {
		PROC_ENTRY(stack_working_opp_table),
		PROC_ENTRY(stack_signed_opp_table),
	};

	dir = proc_mkdir(GPUFREQ_DIR_NAME, NULL);
	if (!dir) {
		GPUFREQ_LOGE("fail to create /proc/%s (ENOMEM)", GPUFREQ_DIR_NAME);
		return GPUFREQ_ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			GPUFREQ_LOGE("fail to create /proc/%s/%s",
				GPUFREQ_DIR_NAME, default_entries[i].name);
	}

	if (g_dual_buck){
		for (i = 0; i < ARRAY_SIZE(dualbuck_entries); i++) {
			if (!proc_create(dualbuck_entries[i].name, 0660,
				dir, dualbuck_entries[i].fops))
				GPUFREQ_LOGE("fail to create /proc/gpufreqv2/%s",
					dualbuck_entries[i].name);
		}
	}

	return GPUFREQ_SUCCESS;
}
#endif /* CONFIG_PROC_FS */

#if defined(CONFIG_DEBUG_FS)
static int history_debug_show(struct seq_file *m, void *v)
{
	int ret = GPUFREQ_SUCCESS;
	int i = 0;
	const unsigned int *history_log = NULL;

	mutex_lock(&gpufreq_debug_lock);

	history_log = gpufreq_history_get_log();

	if (!history_log) {
		GPUFREQ_LOGE("fail to get history log (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < GPUFREQ_HISTORY_SIZE; i++)
		seq_printf(m, "%08x", history_log[i]);

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

/* DEBUGFS : initialization */
DEBUG_FOPS_RO(history);

static int gpufreq_create_debugfs(void)
{
	struct dentry *dir = NULL;
	int i = 0;

	struct debugfs_entry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct debugfs_entry default_entries[] = {
		DEBUG_ENTRY(history),
	};

	dir = debugfs_create_dir(GPUFREQ_DIR_NAME, NULL);
	if (!dir) {
		GPUFREQ_LOGE("fail to create /sys/kernel/debug/%s", GPUFREQ_DIR_NAME);
		return GPUFREQ_ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!debugfs_create_file(default_entries[i].name, 0444,
			dir, NULL, default_entries[i].fops))
			GPUFREQ_LOGE("fail to create /sys/kernel/debug/%s/%s",
				GPUFREQ_DIR_NAME, default_entries[i].name);
	}

	return GPUFREQ_SUCCESS;
}
#endif /* CONFIG_DEBUG_FS */

void gpufreq_debug_init(unsigned int dual_buck, unsigned int gpueb_support,
	const struct gpufreq_shared_status *shared_status)
{
	int ret = GPUFREQ_SUCCESS;

	g_dual_buck = dual_buck;
	g_gpueb_support = gpueb_support;
	g_debug_power_state = GPU_PWR_OFF;
	g_debug_margin_mode = FEAT_ENABLE;
	g_fixed_oppidx_gpu = -1;
	g_fixed_oppidx_stack = -1;
	g_fixed_fgpu = 0;
	g_fixed_vgpu = 0;
	g_fixed_fstack = 0;
	g_fixed_vstack = 0;
	/* take every info of mfgsys from shared status */
	if (shared_status)
		g_shared_status = shared_status;
	else {
		GPUFREQ_LOGE("null gpufreq shared status: 0x%llx", (unsigned long long)shared_status);
		BUG_ON(1);
	}

	ret = gpufreq_history_init();
	if (ret)
		GPUFREQ_LOGE("fail to init gpufreq history");

#if defined(CONFIG_PROC_FS)
	ret = gpufreq_create_procfs();
	if (ret)
		GPUFREQ_LOGE("fail to create procfs (%d)", ret);
#endif /* CONFIG_PROC_FS */

#if defined(CONFIG_DEBUG_FS)
	ret = gpufreq_create_debugfs();
	if (ret)
		GPUFREQ_LOGE("fail to create debugfs (%d)", ret);
#endif /* CONFIG_DEBUG_FS */
}
