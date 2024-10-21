// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#ifndef MMQOS_MTK_C
#define MMQOS_MTK_C

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <linux/clk.h>
//#include <linux/interconnect-provider.h>
#include <linux/interconnect.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_mmdvfs.h>
#include <linux/sched/clock.h>
#include <soc/mediatek/smi.h>
#include <soc/mediatek/dramc.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk_iommu.h"
#include "mmqos-global.h"
#include "mmqos-mtk.h"
#include "mtk_qos_bound.h"
#include "mmqos-vcp.h"
#include "mmqos-vcp-memory.h"
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/emi.h>
#endif /* CONFIG_MTK_EMI */

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <mt-plat/mrdump.h>
#endif

#define CREATE_TRACE_POINTS
#include "mmqos_events.h"

#define SHIFT_ROUND(a, b)	((((a) - 1) >> (b)) + 1)
#define icc_to_MBps(x)		((x) / 1000)
#define MASK_8(a)		((a) & 0xff)
#define MASK_16(a)		((a) & 0xffff)
#define COMM_PORT_COMM_ID(a)	((a >> 8) & 0xff)
#define MULTIPLY_RATIO(value)	((value)*1000)

#define RSH_16(a)		(a >> 16)
#define NODE_TYPE(a)		RSH_16(a)
#define LARB_ID(a)		(MASK_8(a))

#define MAX_RECORD_COMM_NUM	(2)
#define MAX_RECORD_PORT_NUM	(9)
#define VIRT_COMM_PORT_ID	(8)
#define MAX_RECORD_LARB_NUM	(50)

#define MAX_BW_VALUE_NUM	(24)
#define MAX_REG_VALUE_NUM	(8)
#define MAX_BW_UNIT		(1023)

#define APMCU_MASK_OFFSET	(gmmqos->apmcu_mask_offset)
#define APMCU_ON_BW_OFFSET(i)	(gmmqos->apmcu_on_bw_offset + 4 * i)
#define APMCU_OFF_BW_OFFSET(i)	(gmmqos->apmcu_off_bw_offset + 4 * i)

#define MMINFRA_DUMMY		(0x400)

#define IS_ON_TABLE		(true)

enum mmqos_rw_type {
	path_no_type = 0,
	path_write,		//1
	path_read,		//2
};

static u32 ftrace_ena;

struct last_record {
	u64 larb_port_update_time;
	u32 larb_port_node_id;
	u32 larb_port_avg_bw;
	u32 larb_port_peak_bw;
	u64 larb_update_time;
	u32 larb_node_id;
	u32 larb_avg_bw;
	u32 larb_peak_bw;
};

struct last_record *last_rec;

struct comm_port_bw_record {
	u8 idx[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM];
	u64 time[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 larb_id[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 avg_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 peak_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 l_avg_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
	u32 l_peak_bw[MAX_RECORD_COMM_NUM][MAX_RECORD_PORT_NUM][RECORD_NUM];
};

struct chn_bw_record {
	u8 idx[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM];
	u64 time[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 srt_r_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 srt_w_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 hrt_r_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
	u32 hrt_w_bw[MAX_RECORD_COMM_NUM][MMQOS_COMM_CHANNEL_NUM][RECORD_NUM];
};

struct larb_port_bw_record {
	u8 idx[MAX_RECORD_LARB_NUM];
	u64 time[MAX_RECORD_LARB_NUM][RECORD_NUM];
	u32 port_id[MAX_RECORD_LARB_NUM][RECORD_NUM];
	u32 avg_bw[MAX_RECORD_LARB_NUM][RECORD_NUM];
	u32 peak_bw[MAX_RECORD_LARB_NUM][RECORD_NUM];
	u32 mix_bw[MAX_RECORD_LARB_NUM][RECORD_NUM];
	u8 ostdl[MAX_RECORD_LARB_NUM][RECORD_NUM];
};

struct comm_port_bw_record *comm_port_bw_rec;
struct chn_bw_record *chn_bw_rec;
struct larb_port_bw_record *larb_port_bw_rec;

struct larb_port_node {
	struct mmqos_base_node *base;
	u32 old_avg_bw;
	u32 old_peak_bw;
	u16 bw_ratio;
	u8 channel;
	bool is_max_ostd;
	bool is_write;
};

struct mtk_mmqos {
	struct device *dev;
	struct icc_provider prov;
	struct notifier_block nb;
	struct list_head comm_list;
	u32 max_ratio;
	u32 max_disp_ostdl;
	bool dual_pipe_enable;
	bool qos_bound;
	u32 disp_virt_larbs[MMQOS_MAX_DISP_VIRT_LARB_NUM];
	struct proc_dir_entry *proc;
	struct proc_dir_entry *last_proc;
	void __iomem *vmmrc_base;
	u32 apmcu_mask_offset;
	u32 apmcu_mask_bit;
	u32 apmcu_on_bw_offset;
	u32 apmcu_off_bw_offset;
	void __iomem *mminfra_base;
	struct mutex bw_lock;
};

u32 mmqos_state;
u32 log_level;

static struct mtk_mmqos *gmmqos;
static struct mmqos_hrt *g_hrt;

static u32 chn_hrt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 chn_srt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 chn_hrt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 chn_srt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
#ifdef ENABLE_INTERCONNECT_V2
static u32 v2_chn_hrt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 v2_chn_srt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 v2_chn_hrt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 v2_chn_srt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
#endif

static u32 disp_srt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 disp_srt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 disp_hrt_r_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 disp_hrt_w_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};

static u32 emi_chn_hrt_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 emi_chn_srt_bw[MMQOS_MAX_COMM_NUM][MMQOS_COMM_CHANNEL_NUM] = {};
static u32 dramc_freq_mapping_table[MMQOS_MAX_DVFS_STEP_NUM][MMQOS_DVFS_VALUE_NUM];

u32 on_bw_value[MAX_BW_VALUE_NUM];
u32 old_on_bw_value[MAX_BW_VALUE_NUM];
u32 off_bw_value[MAX_BW_VALUE_NUM];
u32 old_off_bw_value[MAX_BW_VALUE_NUM];
u32 on_reg_value[MAX_REG_VALUE_NUM];
u32 off_reg_value[MAX_REG_VALUE_NUM];

u32 freq_mode = BY_REGULATOR;

static void mmqos_update_comm_bw(struct device *dev,
	u32 comm_port, u32 freq, u64 mix_bw, u64 bw_peak, bool qos_bound, bool max_bwl)
{
	u32 comm_bw = 0;
	u32 value;

	if (!freq || !dev)
		return;
	if (mix_bw)
		comm_bw = (mix_bw << 8) / freq;
	if (max_bwl)
		comm_bw = 0xfff;
	if (comm_bw)
		value = ((comm_bw > 0xfff) ? 0xfff :
			((comm_bw < 0x200) ? 0x200 : comm_bw)) |
			((bw_peak > 0 || !qos_bound) ? 0x1000 : 0x3000);
	else
		value = 0x1200;
	mtk_smi_common_bw_set(dev, comm_port, value);
	if (log_level & 1 << log_bw)
		dev_notice(dev, "comm port=%d bw=%d freq=%d qos_bound=%d value=%#x\n",
			comm_port, comm_bw, freq, qos_bound, value);
}

static void mmqos_update_comm_ostdl(struct device *dev, u32 comm_port,
		u16 max_ratio, struct icc_node *larb)
{
	struct larb_node *larb_node = (struct larb_node *)larb->data;
	u32 value;
	u16 bw_ratio;

	bw_ratio = larb_node->bw_ratio;
	if (larb->avg_bw) {
		value = SHIFT_ROUND(icc_to_MBps(larb->avg_bw), bw_ratio);
		if (value > max_ratio)
			value = max_ratio;
	} else
		value = 0;

	mtk_smi_common_ostdl_set(dev, comm_port, larb_node->is_write, value);
	if (log_level & 1 << log_bw)
		dev_notice(dev, "%s larb_id=%d comm port=%d is_write=%d bw_ratio=%d avg_bw=%d ostdl=%d\n",
			__func__, LARB_ID(larb->id), comm_port, larb_node->is_write,
			bw_ratio, larb->avg_bw, value);
}

static void mmqos_update_setting(struct mtk_mmqos *mmqos)
{
	struct common_node *comm_node;
	struct common_port_node *comm_port;

	list_for_each_entry(comm_node, &mmqos->comm_list, list) {
		comm_node->freq = clk_get_rate(comm_node->clk)/1000000;
		if (mmqos_state & BWL_ENABLE) {
			list_for_each_entry(comm_port,
						&comm_node->comm_port_list, list) {
				mutex_lock(&comm_port->bw_lock);
				if (comm_port->latest_mix_bw
					|| comm_port->latest_peak_bw) {
					mmqos_update_comm_bw(comm_port->larb_dev,
						MASK_8(comm_port->base->icc_node->id),
						comm_port->common->freq,
						icc_to_MBps(comm_port->latest_mix_bw),
						icc_to_MBps(comm_port->latest_peak_bw),
						mmqos->qos_bound,
						comm_port->hrt_type == HRT_MAX_BWL);
				}
				mutex_unlock(&comm_port->bw_lock);
			}
		}
	}
}


static int update_mm_clk(struct notifier_block *nb,
		unsigned long value, void *v)
{
	struct mtk_mmqos *mmqos =
		container_of(nb, struct mtk_mmqos, nb);

	mmqos_update_setting(mmqos);
	return 0;
}

void mtk_mmqos_is_dualpipe_enable(bool is_enable)
{
	gmmqos->dual_pipe_enable = is_enable;
	pr_notice("%s: %d\n", __func__, gmmqos->dual_pipe_enable);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_is_dualpipe_enable);

s32 mtk_mmqos_system_qos_update(unsigned short qos_status)
{
	struct mtk_mmqos *mmqos = gmmqos;

	if (IS_ERR_OR_NULL(mmqos)) {
		pr_notice("%s is not ready\n", __func__);
		return 0;
	}
	mmqos->qos_bound = (qos_status > QOS_BOUND_BW_FREE);
	mmqos_update_setting(mmqos);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_system_qos_update);

static unsigned long get_volt_by_freq(struct device *dev, unsigned long freq)
{
	struct dev_pm_opp *opp;
	unsigned long ret;

	opp = dev_pm_opp_find_freq_ceil(dev, &freq);

	/* It means freq is over the highest available frequency */
	if (opp == ERR_PTR(-ERANGE))
		opp = dev_pm_opp_find_freq_floor(dev, &freq);

	if (IS_ERR(opp)) {
		dev_notice(dev, "%s failed(%ld) freq=%lu\n",
			__func__, PTR_ERR(opp), freq);
		return 0;
	}

	ret = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	return ret;
}

static bool is_disp_comm_port(u8 hrt_type)
{
	if (hrt_type == HRT_MAX_BWL)
		return true;
	if (hrt_type == HRT_DISP)
		return true;
	if (hrt_type == HRT_DISP_BY_LARB)
		return true;

	return false;
}

static void set_total_bw_to_emi(struct common_node *comm_node, u32 peak_emi_bw)
{
	u32 avg_bw = 0, peak_bw = 0, total_bw_to_vcp = 0;
	u64 normalize_peak_bw;
	struct common_port_node *comm_port_node;
	u32 comm_id;

	list_for_each_entry(comm_port_node, &comm_node->comm_port_list, list) {
		mutex_lock(&comm_port_node->bw_lock);
		total_bw_to_vcp +=
			comm_port_node->latest_avg_bw + comm_port_node->latest_peak_bw;
		if (mmqos_state & DPC_ENABLE
			&& is_disp_comm_port(comm_port_node->hrt_type)) {
			if (log_level & 1 << log_debug)
				MMQOS_DBG("ignore disp comm port bw");
		} else {
			if (mmqos_state & VCODEC_BW_BYPASS
				&& comm_port_node->hrt_type == SRT_VDEC) {
				if (log_level & 1 << log_debug)
					pr_notice("ignore SRT_VDEC comm port bw\n");
			} else if (mmqos_state & VCODEC_BW_BYPASS
				&& comm_port_node->hrt_type == SRT_VENC) {
				if (log_level & 1 << log_debug)
					pr_notice("ignore SRT_VENC comm port bw\n");
			} else {
				avg_bw += comm_port_node->latest_avg_bw;
			}
			if (comm_port_node->hrt_type < HRT_TYPE_NUM) {
				normalize_peak_bw = MULTIPLY_RATIO(comm_port_node->latest_peak_bw)
							/ mtk_mmqos_get_hrt_ratio(
							comm_port_node->hrt_type);
				peak_bw += normalize_peak_bw;
			}
		}
		mutex_unlock(&comm_port_node->bw_lock);
	}

	comm_id = MASK_8(comm_node->base->icc_node->id);
	if (mmqos_met_enabled())
		trace_mmqos__bw_to_emi(comm_id,
			icc_to_MBps(avg_bw), icc_to_MBps(peak_bw));

	if (log_level & 1 << log_bw)
		MMQOS_DBG("comm%d avg %d peak_emi_bw %u peak %d",
			comm_id, icc_to_MBps(avg_bw), peak_emi_bw, icc_to_MBps(peak_bw));
	if (MEM_BASE != NULL) {
		writel(total_bw_to_vcp, MEM_APMCU_TOTAL_BW);
		if (log_level & 1 << log_bw)
			MMQOS_DBG("total_bw_to_vcp:%d", total_bw_to_vcp);
	}

	MMQOS_SYSTRACE_BEGIN("to EMI avg %d peak_emi_bw %u peak %d\n",
		icc_to_MBps(avg_bw), peak_emi_bw, icc_to_MBps(peak_bw));
	icc_set_bw(comm_node->icc_path, avg_bw, MBps_to_icc(peak_emi_bw));
	icc_set_bw(comm_node->icc_hrt_path, peak_bw, 0);
	MMQOS_SYSTRACE_END();
}

static u32 get_dyna_urate_from_chnn_diff(u32 chn0_bw, u32 chn1_bw)
{
	u32 urate = 70;

	if (chn0_bw > chn1_bw * 2 || chn1_bw > chn0_bw * 2)
		urate = 60;
	else if (chn0_bw > chn1_bw * 15 / 10 || chn1_bw > chn0_bw * 15 / 10)
		urate = 65;

	return urate;
}

static u32 get_max_channel_bw_in_common(u32 comm_id)
{
	u32 max_bw = 0, i;
	u32 final_h_r_bw, final_h_w_bw, final_s_r_bw, final_s_w_bw;

	for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++) {
		max_bw = max_t(u32, max_bw, chn_hrt_r_bw[comm_id][i] * 10 / 7);
		max_bw = max_t(u32, max_bw, chn_srt_r_bw[comm_id][i]);
		max_bw = max_t(u32, max_bw, chn_hrt_w_bw[comm_id][i] * 10 / 7);
		max_bw = max_t(u32, max_bw, chn_srt_w_bw[comm_id][i]);

		if (log_level & 1 << log_comm_freq)
			MMQOS_DBG("comm(%d) chn=%d without urate max_bw=%u s_r=%u h_r=%u, s_w=%u h_w=%u\n",
				comm_id, i, max_bw, chn_srt_r_bw[comm_id][i], chn_hrt_r_bw[comm_id][i] * 10 / 7,
				chn_srt_w_bw[comm_id][i], chn_hrt_w_bw[comm_id][i] * 10 / 7);

		if (mmqos_met_enabled())
			trace_mmqos__chn_bw(comm_id, i,
				icc_to_MBps(chn_srt_r_bw[comm_id][i] * 10 / 7),
				icc_to_MBps(chn_srt_w_bw[comm_id][i]),
				icc_to_MBps(chn_hrt_r_bw[comm_id][i] * 10 / 7),
				icc_to_MBps(chn_hrt_w_bw[comm_id][i]),
				icc_to_MBps(max_bw),
				TYPE_IS_ON,
				TYPE_IS_MAX);
	}

	if (mmqos_state & DYNA_URATE_ENABLE) {
		u32 hrt_r_urate, hrt_w_urate, srt_r_urate, srt_w_urate;

		max_bw = 0;

		hrt_r_urate = get_dyna_urate_from_chnn_diff(
			chn_hrt_r_bw[comm_id][0], chn_hrt_r_bw[comm_id][1]);
		hrt_w_urate = get_dyna_urate_from_chnn_diff(
			chn_hrt_w_bw[comm_id][0], chn_hrt_w_bw[comm_id][1]);
		// change srt from occupied bw to data bw (/1.33)
		chn_srt_r_bw[comm_id][0] = chn_srt_r_bw[comm_id][0] * 75 / 100;
		chn_srt_r_bw[comm_id][1] = chn_srt_r_bw[comm_id][1] * 75 / 100;
		chn_srt_w_bw[comm_id][0] = chn_srt_w_bw[comm_id][0] * 75 / 100;
		chn_srt_w_bw[comm_id][1] = chn_srt_w_bw[comm_id][1] * 75 / 100;

		srt_r_urate = get_dyna_urate_from_chnn_diff(
			chn_srt_r_bw[comm_id][0], chn_srt_r_bw[comm_id][1]);
		srt_w_urate = get_dyna_urate_from_chnn_diff(
			chn_srt_w_bw[comm_id][0], chn_srt_w_bw[comm_id][1]);

		if (log_level & 1 << log_comm_freq) {
			MMQOS_DBG("comm(%d) original data bw s_r[chn0]=%u s_r[chn1]=%u, s_w[chn0]=%u s_w[chn1]=%u\n",
				comm_id, chn_srt_r_bw[comm_id][0], chn_srt_r_bw[comm_id][1],
				chn_srt_w_bw[comm_id][0], chn_srt_w_bw[comm_id][1]);
			MMQOS_DBG("comm(%d) h_r[chn0]=%u h_r[chn1]=%u, h_w[chn0]=%u h_w[chn1]=%u\n",
				comm_id, chn_hrt_r_bw[comm_id][0], chn_hrt_r_bw[comm_id][1],
				chn_hrt_w_bw[comm_id][0], chn_hrt_w_bw[comm_id][1]);
			MMQOS_DBG("comm(%d) srt_r_urate=%d srt_w_urate=%d hrt_r_urate=%d hrt_w_urate=%d\n",
				comm_id, srt_r_urate, srt_w_urate, hrt_r_urate, hrt_w_urate);
		}
		for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++) {
			final_h_r_bw = chn_hrt_r_bw[comm_id][i] * 100 / hrt_r_urate;
			final_h_w_bw = chn_hrt_w_bw[comm_id][i] * 100 / hrt_w_urate;
			final_s_r_bw = chn_srt_r_bw[comm_id][i] * 100 / srt_r_urate;
			final_s_w_bw = chn_srt_w_bw[comm_id][i] * 100 / srt_w_urate;

			emi_chn_hrt_bw[comm_id][i] = chn_hrt_r_bw[comm_id][i] + chn_hrt_w_bw[comm_id][i];
			emi_chn_srt_bw[comm_id][i] = chn_srt_r_bw[comm_id][i] + chn_srt_w_bw[comm_id][i];

			max_bw = max_t(u32, max_bw, final_h_r_bw);
			max_bw = max_t(u32, max_bw, final_s_r_bw);
			max_bw = max_t(u32, max_bw, final_h_w_bw);
			max_bw = max_t(u32, max_bw, final_s_w_bw);
			if (log_level & 1 << log_comm_freq)
				MMQOS_DBG("comm(%d) chn=%d max_bw=%u apply urate s_r=%u h_r=%u, s_w=%u h_w=%u\n",
					comm_id, i, max_bw, final_s_r_bw, final_h_r_bw,
					final_s_w_bw, final_h_w_bw);
			if (mmqos_met_enabled())
				trace_mmqos__chn_bw(comm_id, i,
					icc_to_MBps(final_s_r_bw),
					icc_to_MBps(final_s_w_bw),
					icc_to_MBps(final_h_r_bw),
					icc_to_MBps(final_h_w_bw),
					icc_to_MBps(max_bw),
					TYPE_IS_URATE,
					TYPE_IS_MAX_URATE);
		}
	}

	return max_bw;
}

static u32 get_peak_emi_freq(u32 emi_bw)
{
	u32 step, emi_freq = 0;

	if (emi_bw == 0)
		return 0;

	emi_freq = emi_bw * 8 / 128 / 1000; //MHZ

	for (step = 0; step < MMQOS_MAX_DVFS_STEP_NUM; step++) {
		if (emi_freq > dramc_freq_mapping_table[step][1] || !dramc_freq_mapping_table[step][1])
			break;
	}

	if (step > 0)
		return dramc_freq_mapping_table[step - 1][0];
	return dramc_freq_mapping_table[step][0];
}

static u32 get_max_channel_bw_in_emi(u32 comm_id)
{
	u32 i, hrt_urate, srt_urate, final_h_bw, final_s_bw, max_bw = 0;

	hrt_urate = get_dyna_urate_from_chnn_diff(
		emi_chn_hrt_bw[comm_id][0], emi_chn_hrt_bw[comm_id][1]);
	srt_urate = get_dyna_urate_from_chnn_diff(
		emi_chn_srt_bw[comm_id][0], emi_chn_srt_bw[comm_id][1]);

	if (log_level & 1 << log_comm_freq) {
		pr_notice("%s comm(%d) emi_h[chn0]=%u emi_h[chn1]=%u, emi_s[chn0]=%u emi_s[chn1]=%u\n",
			__func__, comm_id, emi_chn_hrt_bw[comm_id][0], emi_chn_hrt_bw[comm_id][1],
			emi_chn_srt_bw[comm_id][0], emi_chn_srt_bw[comm_id][1]);
		pr_notice("%s comm(%d) hrt_urate=%d srt_urate=%d\n",
			__func__, comm_id, hrt_urate, srt_urate);
	}
	for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++) {
		final_h_bw = emi_chn_hrt_bw[comm_id][i] * 100 / hrt_urate;
		final_s_bw = emi_chn_srt_bw[comm_id][i] * 100 / srt_urate;

		if (log_level & 1 << log_comm_freq)
			pr_notice("%s comm(%d) chn=%d apply urate h_bw=%u s_bw=%u (KB/s)\n",
				__func__, comm_id, i, final_h_bw, final_s_bw);

		max_bw = max_t(u32, max_bw, final_h_bw);
		max_bw = max_t(u32, max_bw, final_s_bw);
	}

	return max_bw;
}

static u32 get_max_channel_bw(u32 comm_id, u32 freq_mode)
{
	u32 max_bw = 0, comm_bw, i;

	if (freq_mode == BY_REGULATOR)
		return get_max_channel_bw_in_common(comm_id);

	// BY_MMDVFS
	for (i = 0; i < MMQOS_MAX_COMM_NUM; i++) {
		comm_bw = get_max_channel_bw_in_common(i);
		max_bw = max_t(u32, max_bw, comm_bw);
	}

	return max_bw;
}

static void set_freq_by_regulator(struct common_node *comm_node, unsigned long smi_clk)
{
	u32 volt;

	volt = get_volt_by_freq(comm_node->comm_dev, smi_clk);
	if (volt > 0 && volt != comm_node->volt) {
		if (IS_ERR_OR_NULL(comm_node->comm_reg)) {
			dev_notice(comm_node->comm_dev, "comm_reg not existed\n");
		} else if (regulator_set_voltage(comm_node->comm_reg,
				volt, INT_MAX))
			dev_notice(comm_node->comm_dev,
				"regulator_set_voltage failed volt=%u\n", volt);
		comm_node->volt = volt;
	}
}

/*
static void set_freq_by_mmdvfs(struct common_node *comm_node, unsigned long smi_clk)
{
	if (log_level & 1 << log_comm_freq)
		dev_notice(comm_node->comm_dev, "set freq_rate:%lu\n", smi_clk);

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMQOS);
	mmdvfs_mux_set_opp("user-smi", smi_clk);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMQOS);
}
*/

static u32 change_to_unit(u32 bw)
{
	/* bw unit is 16MB/s */
	u32 bw_unit;

	bw_unit = (icc_to_MBps(bw)) >> 4;
	if (bw_unit >= MAX_BW_UNIT) {
		MMQOS_ERR("bw is over %d*16MB/s", MAX_BW_UNIT);
		bw_unit = MAX_BW_UNIT;
	}
	if (log_level & 1 << log_comm_freq && bw > 0)
		MMQOS_DBG("bw:%d, bw_unit:%d", bw, bw_unit);

	return bw_unit;
}

static void store_bw_value(const u32 comm_id, const u32 chnn_id,
	bool is_srt, bool is_write, bool is_on, u32 bw)
{
	int bw_value_idx;

	bw_value_idx = (comm_id << 2) + (chnn_id << 1) + (is_srt ? 0 : 12)
		+ (is_write ? 1 : 0);

	if (!is_srt)
		bw = bw * 10 / 7;

	bw = change_to_unit(bw);

	if (is_on)
		on_bw_value[bw_value_idx] = bw;
	else
		off_bw_value[bw_value_idx] = bw;

	if (log_level & 1 << log_comm_freq)
		if (bw != 0)
			MMQOS_DBG("bw_value_idx:%d, bw:%d", bw_value_idx, bw);
}

bool is_bw_value_changed(bool is_on)
{
	u32 *old_bw;
	u32 *bw_value;
	int i = 0;

	if (is_on) {
		old_bw = old_on_bw_value;
		bw_value = on_bw_value;
	} else {
		old_bw = old_off_bw_value;
		bw_value = off_bw_value;
	}

	for (i = 0 ; i < MAX_BW_VALUE_NUM ; i++) {
		if (bw_value[i] == old_bw[i])
			continue;
		else
			return true;
	}
	return false;
}

void write_register(u32 offset, u32 value)
{
	if (log_level & (1U << log_bw))
		if (value != 0)
			MMQOS_DBG("offset:0x%x, value:0x%x", offset, value);

	if (mmqos_state & VMMRC_ENABLE) {
		if (gmmqos->vmmrc_base == NULL)
			MMQOS_ERR("write vmmrc fail, vmmrc_base is NULL");
		else
			writel_relaxed(value, gmmqos->vmmrc_base + offset);
	}
}

u32 read_register(u32 offset)
{
	if (mmqos_state & VMMRC_ENABLE) {
		if (gmmqos->vmmrc_base == NULL)
			MMQOS_ERR("read vmmrc fail, vmmrc_base is NULL");
		else
			return readl_relaxed(gmmqos->vmmrc_base + offset);
	}
	return 0;
}

static void start_write_bw(void)
{
	u32 orig = 0;

	// for hfrp timeout debug
	readl_relaxed(gmmqos->mminfra_base + MMINFRA_DUMMY);
	//enable vcp
	if (mmqos_state & VMMRC_VCP_ENABLE)
		mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMQOS);
	orig = read_register(APMCU_MASK_OFFSET);
	write_register(APMCU_MASK_OFFSET, orig | BIT(gmmqos->apmcu_mask_bit));
}

static void stop_write_bw(void)
{
	u32 orig = 0;

	orig = read_register(APMCU_MASK_OFFSET);
	write_register(APMCU_MASK_OFFSET, orig & ~BIT(gmmqos->apmcu_mask_bit));
	//disable vcp
	if (mmqos_state & VMMRC_VCP_ENABLE)
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMQOS);
}

void clear_reg_value(bool is_on)
{
	int i;

	for (i = 0; i < MAX_REG_VALUE_NUM; i++) {
		if (is_on)
			on_reg_value[i] = 0;
		else
			off_reg_value[i] = 0;
	}
}

void set_channel_bw_reg_value(bool is_on)
{
	int i, reg_value_idx;
	bool is_test = log_level & (1 << log_debug);
	u32 *reg_value;
	u32 *bw_value;
	u32 *old_bw_value;

	clear_reg_value(is_on);
	if (is_on) {
		reg_value = on_reg_value;
		bw_value = on_bw_value;
		old_bw_value = old_on_bw_value;
	} else {
		reg_value = off_reg_value;
		bw_value = off_bw_value;
		old_bw_value = old_off_bw_value;
	}

	for (i = 0 ; i < MAX_BW_VALUE_NUM ; i++) {
		if (bw_value[i] != 0 && is_test)
			MMQOS_DBG("is_on:%d, i:%d, bw_value:%d",
				is_on, i, bw_value[i]);
		reg_value_idx = i/3;
		reg_value[reg_value_idx] += (bw_value[i] & 0x3FF) << ((i % 3) * 10);
		if (reg_value[reg_value_idx] != 0 && is_test)
			MMQOS_DBG("is_on:%d, idx:%d, reg_value:%#x",
				is_on, reg_value_idx, reg_value[reg_value_idx]);
		old_bw_value[i] = bw_value[i];
	}
}

static void set_channel_bw_to_hw(void)
{
	int i;

	start_write_bw();
	for (i = 0 ; i < MAX_REG_VALUE_NUM; i++)
		write_register(APMCU_ON_BW_OFFSET(i), on_reg_value[i]);

	for (i = 0 ; i < MAX_REG_VALUE_NUM; i++)
		write_register(APMCU_OFF_BW_OFFSET(i), off_reg_value[i]);
	stop_write_bw();
}

static void set_freq_by_vmmrc(const u32 comm_id)
{
	int i, j;
	bool is_reg_value_changed = false;
	u32 off_s_r_bw;
	u32 off_s_w_bw;
	u32 off_h_r_bw;
	u32 off_h_w_bw;

	if (log_level & 1 << log_comm_freq) {
		for (i = 0; i < MMQOS_MAX_COMM_NUM; i++) {
			for (j = 0; j < MMQOS_COMM_CHANNEL_NUM; j++) {
				MMQOS_DBG("comm(%d) chn=%d disp s_r=%u h_r=%u s_w=%u h_w=%u",
				i, j, disp_srt_r_bw[i][j], disp_hrt_r_bw[i][j],
				disp_srt_w_bw[i][j], disp_hrt_w_bw[i][j]);
			}
		}
	}

	for (i = 0; i < MMQOS_COMM_CHANNEL_NUM; i++) {
		bool is_srt = true;
		bool is_write = true;

		store_bw_value(comm_id, i, is_srt, !is_write, IS_ON_TABLE,
			chn_srt_r_bw[comm_id][i]);
		store_bw_value(comm_id, i, is_srt, is_write, IS_ON_TABLE,
			chn_srt_w_bw[comm_id][i]);
		store_bw_value(comm_id, i, !is_srt, !is_write, IS_ON_TABLE,
			chn_hrt_r_bw[comm_id][i]);
		store_bw_value(comm_id, i, !is_srt, is_write, IS_ON_TABLE,
			chn_hrt_w_bw[comm_id][i]);

		off_s_r_bw = chn_srt_r_bw[comm_id][i] - disp_srt_r_bw[comm_id][i];
		off_s_w_bw = chn_srt_w_bw[comm_id][i] - disp_srt_w_bw[comm_id][i];
		off_h_r_bw = chn_hrt_r_bw[comm_id][i] - disp_hrt_r_bw[comm_id][i];
		off_h_w_bw = chn_hrt_w_bw[comm_id][i] - disp_hrt_w_bw[comm_id][i];
		store_bw_value(comm_id, i, is_srt, !is_write, !IS_ON_TABLE,
			off_s_r_bw);
		store_bw_value(comm_id, i, is_srt, is_write, !IS_ON_TABLE,
			off_s_w_bw);
		store_bw_value(comm_id, i, !is_srt, !is_write, !IS_ON_TABLE,
			off_h_r_bw);
		store_bw_value(comm_id, i, !is_srt, is_write, !IS_ON_TABLE,
			off_h_w_bw);

		if (mmqos_met_enabled())
			trace_mmqos__chn_bw(comm_id, i,
				icc_to_MBps(off_s_r_bw),
				icc_to_MBps(off_s_w_bw),
				icc_to_MBps(off_h_r_bw),
				icc_to_MBps(off_h_w_bw),
				0,
				TYPE_IS_OFF,
				TYPE_IS_MAX);
	}

	if (is_bw_value_changed(IS_ON_TABLE)) {
		set_channel_bw_reg_value(IS_ON_TABLE);
		is_reg_value_changed = true;
	}
	if (is_bw_value_changed(!IS_ON_TABLE)) {
		set_channel_bw_reg_value(!IS_ON_TABLE);
		is_reg_value_changed = true;
	}
	if (is_reg_value_changed)
		set_channel_bw_to_hw();
}

static void set_comm_icc_bw(struct common_node *comm_node)
{
	u32 max_bw = 0, emi_freq = 0, peak_emi_freq = 0, emi_bw = 0;
	unsigned long smi_clk = 0;
	u32 comm_id, i, j;

	MMQOS_SYSTRACE_BEGIN("%s %s\n", __func__, comm_node->base->icc_node->name);

	comm_id = MASK_8(comm_node->base->icc_node->id);

	if (log_level & 1 << log_comm_freq) {
		for (i = 0; i < MMQOS_MAX_COMM_NUM; i++) {
			for (j = 0; j < MMQOS_COMM_CHANNEL_NUM; j++) {
				MMQOS_DBG("comm(%d) chn=%d s_r=%u h_r=%u s_w=%u h_w=%u",
				i, j, chn_srt_r_bw[i][j], chn_hrt_r_bw[i][j],
				chn_srt_w_bw[i][j], chn_hrt_w_bw[i][j]);
			}
		}
	}

	if (freq_mode == BY_REGULATOR || freq_mode == BY_MMDVFS) {
		max_bw = get_max_channel_bw(comm_id, freq_mode);
		if (mmqos_state & DYNA_URATE_ENABLE) {
			emi_bw = get_max_channel_bw_in_emi(comm_id);
			peak_emi_freq = get_peak_emi_freq(emi_bw);
		}

		if (max_bw)
			smi_clk = SHIFT_ROUND(max_bw, 4) * 1000;
		else
			smi_clk = 0;

		if (log_level & 1 << log_comm_freq) {
			dev_notice(comm_node->comm_dev,
				"comm(%d) max_bw=%u smi_clk=%lu freq_mode=%d emi_freq=%u peak_emi_bw=%u\n",
				comm_id, max_bw, smi_clk, freq_mode, emi_freq, peak_emi_freq);
		}

		if (comm_node->comm_dev && smi_clk != comm_node->smi_clk) {
			if (freq_mode == BY_REGULATOR)
				set_freq_by_regulator(comm_node, smi_clk);
			else
				MMQOS_ERR("freq_mode:%d is wrong", freq_mode);

			comm_node->smi_clk = smi_clk;
		}
	} else if (freq_mode == BY_VMMRC) {
		set_freq_by_vmmrc(comm_id);
	}

	set_total_bw_to_emi(comm_node, peak_emi_freq);

	MMQOS_SYSTRACE_END();
}

static void update_hrt_bw(struct mtk_mmqos *mmqos)
{
	struct common_node *comm_node;
	struct common_port_node *comm_port;
	u32 hrt_bw[HRT_TYPE_NUM] = {0};
	u32 i;

	list_for_each_entry(comm_node, &mmqos->comm_list, list) {
		list_for_each_entry(comm_port,
				    &comm_node->comm_port_list, list) {
			if (comm_port->hrt_type < HRT_TYPE_NUM) {
				mutex_lock(&comm_port->bw_lock);
				hrt_bw[comm_port->hrt_type] +=
					icc_to_MBps(comm_port->latest_peak_bw);
				mutex_unlock(&comm_port->bw_lock);
			}
		}
	}

	for (i = 0; i < HRT_TYPE_NUM; i++)
		if (i != HRT_MD)
			mtk_mmqos_set_hrt_bw(i, hrt_bw[i]);

}

static void record_last_larb(u32 node_id, u32 avg_bw, u32 peak_bw)
{
	last_rec->larb_update_time = sched_clock();
	last_rec->larb_node_id = node_id;
	last_rec->larb_avg_bw = avg_bw;
	last_rec->larb_peak_bw = peak_bw;
}

static void record_last_larb_port(u32 node_id, u32 avg_bw, u32 peak_bw)
{
	last_rec->larb_port_update_time = sched_clock();
	last_rec->larb_port_node_id = node_id;
	last_rec->larb_port_avg_bw = avg_bw;
	last_rec->larb_port_peak_bw = peak_bw;
}

static void record_comm_port_bw(u32 comm_id, u32 port_id, u32 larb_id,
	u32 avg_bw, u32 peak_bw, u32 l_avg, u32 l_peak)
{
	u32 idx;

	if (log_level & 1 << log_bw)
		pr_notice("%s comm%d port%d larb%d %d %d %d %d\n", __func__,
			comm_id, port_id, larb_id,
			icc_to_MBps(avg_bw), icc_to_MBps(peak_bw),
			icc_to_MBps(l_avg), icc_to_MBps(l_peak));
	idx = comm_port_bw_rec->idx[comm_id][port_id];
	comm_port_bw_rec->time[comm_id][port_id][idx] = sched_clock();
	comm_port_bw_rec->larb_id[comm_id][port_id][idx] = larb_id;
	comm_port_bw_rec->avg_bw[comm_id][port_id][idx] = avg_bw;
	comm_port_bw_rec->peak_bw[comm_id][port_id][idx] = peak_bw;
	comm_port_bw_rec->l_avg_bw[comm_id][port_id][idx] = l_avg;
	comm_port_bw_rec->l_peak_bw[comm_id][port_id][idx] = l_peak;
	comm_port_bw_rec->idx[comm_id][port_id] = (idx + 1) % RECORD_NUM;
}

static void record_chn_bw(u32 comm_id, u32 chnn_id, u32 srt_r, u32 srt_w, u32 hrt_r, u32 hrt_w)
{
	u32 idx;

	idx = chn_bw_rec->idx[comm_id][chnn_id];
	chn_bw_rec->time[comm_id][chnn_id][idx] = sched_clock();
	chn_bw_rec->srt_r_bw[comm_id][chnn_id][idx] = srt_r;
	chn_bw_rec->srt_w_bw[comm_id][chnn_id][idx] = srt_w;
	chn_bw_rec->hrt_r_bw[comm_id][chnn_id][idx] = hrt_r;
	chn_bw_rec->hrt_w_bw[comm_id][chnn_id][idx] = hrt_w;
	chn_bw_rec->idx[comm_id][chnn_id] = (idx + 1) % RECORD_NUM;
}

static void record_larb_port_bw_ostdl(u32 larb_id, u32 port_id, u32 avg_bw, u32 peak_bw, u32 mix_bw, u8 ostdl)
{
	u32 idx;

	idx = larb_port_bw_rec->idx[larb_id];
	larb_port_bw_rec->time[larb_id][idx] = sched_clock();
	larb_port_bw_rec->port_id[larb_id][idx] = port_id;
	larb_port_bw_rec->avg_bw[larb_id][idx] = avg_bw;
	larb_port_bw_rec->peak_bw[larb_id][idx] = peak_bw;
	larb_port_bw_rec->mix_bw[larb_id][idx] = mix_bw;
	larb_port_bw_rec->ostdl[larb_id][idx] = ostdl;
	larb_port_bw_rec->idx[larb_id] = (idx + 1) % RECORD_NUM;
}

void update_channel_bw(const u32 comm_id, const u32 chnn_id,
	struct icc_node *src)
{
	struct common_port_node *comm_port_node;
	u32 half_hrt_r;

	comm_port_node = (struct common_port_node *)src->data;
	if (comm_port_node == NULL) {
		MMQOS_ERR("fail, comm_port_node is NULL");
		return;
	}
	if (mmqos_state & DISP_BY_LARB_ENABLE
		&& comm_port_node->hrt_type == HRT_DISP) {
		if (log_level & 1 << log_bw)
			pr_notice("ignore HRT_DISP comm port:%#x\n", src->id);
		return;
	} else if (!(mmqos_state & DISP_BY_LARB_ENABLE)
		&& comm_port_node->hrt_type == HRT_DISP_BY_LARB) {
		if (log_level & 1 << log_bw)
			pr_notice("ignore HRT_DISP_BY_LARB comm port:%#x\n",
					src->id);
		return;
	}

	if ((mmqos_state & DPC_ENABLE)
		&& is_disp_comm_port(comm_port_node->hrt_type)) {
		// display bw should be only write on the on table
		// other mm users need to write on & off table.
		// so we need to record display chnn bw
		disp_srt_r_bw[comm_id][chnn_id] -= comm_port_node->old_avg_r_bw;
		disp_srt_w_bw[comm_id][chnn_id] -= comm_port_node->old_avg_w_bw;
		disp_hrt_r_bw[comm_id][chnn_id] -= comm_port_node->old_peak_r_bw;
		disp_hrt_w_bw[comm_id][chnn_id] -= comm_port_node->old_peak_w_bw;
		disp_srt_r_bw[comm_id][chnn_id] += src->v2_avg_r_bw;
		disp_srt_w_bw[comm_id][chnn_id] += src->v2_avg_w_bw;
		disp_hrt_r_bw[comm_id][chnn_id] += src->v2_peak_r_bw;
		disp_hrt_w_bw[comm_id][chnn_id] += src->v2_peak_w_bw;
	}
	v2_chn_hrt_w_bw[comm_id][chnn_id] -= comm_port_node->old_peak_w_bw;
	v2_chn_hrt_r_bw[comm_id][chnn_id] -= comm_port_node->old_peak_r_bw;
	v2_chn_srt_w_bw[comm_id][chnn_id] -= comm_port_node->old_avg_w_bw;
	v2_chn_srt_r_bw[comm_id][chnn_id] -= comm_port_node->old_avg_r_bw;
	v2_chn_hrt_w_bw[comm_id][chnn_id] += src->v2_peak_w_bw;
	v2_chn_hrt_r_bw[comm_id][chnn_id] += src->v2_peak_r_bw;
	v2_chn_srt_w_bw[comm_id][chnn_id] += src->v2_avg_w_bw;
	v2_chn_srt_r_bw[comm_id][chnn_id] += src->v2_avg_r_bw;
	if (comm_port_node->hrt_type == HRT_DISP
		&& gmmqos->dual_pipe_enable) {
		half_hrt_r = (src->v2_peak_r_bw / 2);
		v2_chn_hrt_r_bw[comm_id][chnn_id] -= half_hrt_r;
		if (mmqos_state & DPC_ENABLE)
			disp_hrt_r_bw[comm_id][chnn_id] -= half_hrt_r;
	} else {
		half_hrt_r = (src->v2_peak_r_bw);
	}
	comm_port_node->old_peak_w_bw = src->v2_peak_w_bw;
	comm_port_node->old_peak_r_bw = half_hrt_r;
	comm_port_node->old_avg_w_bw = src->v2_avg_w_bw;
	comm_port_node->old_avg_r_bw = src->v2_avg_r_bw;
#ifdef ENABLE_INTERCONNECT_V1
	if ((chn_hrt_w_bw[comm_id][chnn_id]
		!= v2_chn_hrt_w_bw[comm_id][chnn_id]) ||
	(chn_hrt_r_bw[comm_id][chnn_id]
		!= v2_chn_hrt_r_bw[comm_id][chnn_id]) ||
	(chn_srt_w_bw[comm_id][chnn_id]
		!= v2_chn_srt_w_bw[comm_id][chnn_id]) ||
	(chn_srt_r_bw[comm_id][chnn_id]
		!= v2_chn_srt_r_bw[comm_id][chnn_id])) {
		pr_notice("[mmqos][dbg][new]v1_hrt_w_bw:%d  v1_hrt_r_bw:%d v1_srt_w_bw:%d v1_srt_r_bw:%d\n",
			 chn_hrt_w_bw[comm_id][chnn_id],
			 chn_hrt_r_bw[comm_id][chnn_id],
			 chn_srt_w_bw[comm_id][chnn_id],
			 chn_srt_r_bw[comm_id][chnn_id]);
		pr_notice("[mmqos][dbg][new]v2_hrt_w_bw:%d  v2_hrt_r_bw:%d v2_srt_w_bw:%d v2_srt_r_bw:%d\n",
			 v2_chn_hrt_w_bw[comm_id][chnn_id],
			 v2_chn_hrt_r_bw[comm_id][chnn_id],
			 v2_chn_srt_w_bw[comm_id][chnn_id],
			 v2_chn_srt_r_bw[comm_id][chnn_id]);
	}
#endif

	chn_hrt_w_bw[comm_id][chnn_id] = v2_chn_hrt_w_bw[comm_id][chnn_id];
	chn_srt_w_bw[comm_id][chnn_id] = v2_chn_srt_w_bw[comm_id][chnn_id];
	chn_hrt_r_bw[comm_id][chnn_id] = v2_chn_hrt_r_bw[comm_id][chnn_id];
	chn_srt_r_bw[comm_id][chnn_id] = v2_chn_srt_r_bw[comm_id][chnn_id];
	if (log_level & 1 << log_v2_dbg)
		pr_notice("[mmqos][dbg][new] hrt_w_bw:%d  hrt_r_bw:%d srt_w_bw:%d srt_r_bw:%d\n",
			chn_hrt_w_bw[comm_id][chnn_id],
			chn_hrt_r_bw[comm_id][chnn_id],
			chn_srt_w_bw[comm_id][chnn_id],
			chn_srt_r_bw[comm_id][chnn_id]);
}
EXPORT_SYMBOL_GPL(update_channel_bw);

static u32 is_path_write = path_no_type;

static inline bool is_max_bw_to_max_ostdl_policy(void)
{
	return (!(mmqos_state & DPC_ENABLE) && !(mmqos_state & CAM_NO_MAX_OSTDL)) || (g_hrt->cam_occu_bw == 0);
}

static int mtk_mmqos_set(struct icc_node *src, struct icc_node *dst)
{
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct common_port_node *comm_port_node;
	struct common_node *comm_node;
	struct mtk_mmqos *mmqos = container_of(dst->provider,
					struct mtk_mmqos, prov);
	u32 value = 1;
	u32 comm_id, chnn_id, port_id, trace_comm_id, trace_chnn_id;
	const char *r_w_type = "w";

	MMQOS_SYSTRACE_BEGIN("%s %s->%s\n", __func__, src->name, dst->name);
	switch (NODE_TYPE(dst->id)) {
	case MTK_MMQOS_NODE_COMMON:
		comm_node = (struct common_node *)dst->data;
#ifdef ENABLE_INTERCONNECT_V2
		comm_port_node = (struct common_port_node *)src->data;
		comm_id = (comm_port_node->channel_v2 >> 4) & 0xf;
		chnn_id = comm_port_node->channel_v2 & 0xf;
		if (chnn_id) {
			chnn_id -= 1;
			update_channel_bw(comm_id, chnn_id, src);

			record_chn_bw(comm_id, chnn_id,
				chn_srt_r_bw[comm_id][chnn_id],
				chn_srt_w_bw[comm_id][chnn_id],
				chn_hrt_r_bw[comm_id][chnn_id],
				chn_hrt_w_bw[comm_id][chnn_id]);
			if (mmqos_met_enabled())
				trace_mmqos__chn_bw(comm_id, chnn_id,
					icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_srt_w_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_hrt_w_bw[comm_id][chnn_id]),
					0,
					TYPE_IS_ON,
					TYPE_IS_MAX);
		}
#endif
		if (!comm_node)
			break;
		if (mmqos_state & DVFSRC_ENABLE) {
			set_comm_icc_bw(comm_node);
			update_hrt_bw(mmqos);
		}
		//queue_work(mmqos->wq, &comm_node->work);
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		comm_port_node = (struct common_port_node *)dst->data;
		larb_node = (struct larb_node *)src->data;
		if (!comm_port_node || !larb_node)
			break;
		comm_id = (larb_node->channel >> 4) & 0xf;
		chnn_id = larb_node->channel_v2 & 0xf;
		if ((mmqos_state & DISP_BY_LARB_ENABLE) == 0
			&& (src->id == gmmqos->disp_virt_larbs[1] ||
			src->id == gmmqos->disp_virt_larbs[2])) {
			if (log_level & 1 << log_bw)
				pr_notice("ignore larb%d\n", src->id);
			break;
		}

		record_last_larb(src->id, src->avg_bw, src->peak_bw);

		if (chnn_id) {
#ifdef ENABLE_INTERCONNECT_V1
			chnn_id -= 1;
			if (mmqos_state & DISP_BY_LARB_ENABLE
				&& src->id == gmmqos->disp_virt_larbs[0]) {
				if (log_level & 1 << log_bw)
					pr_notice("ignore larb%d\n", src->id);
			} else if (larb_node->is_write) {
				chn_hrt_w_bw[comm_id][chnn_id] -= larb_node->old_peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] -= larb_node->old_avg_bw;
				chn_hrt_w_bw[comm_id][chnn_id] += src->peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] += src->avg_bw;
				larb_node->old_peak_bw = src->peak_bw;
				larb_node->old_avg_bw = src->avg_bw;
			} else {
				if (comm_port_node->hrt_type == HRT_DISP
					&& gmmqos->dual_pipe_enable) {
					chn_hrt_r_bw[comm_id][chnn_id] -= larb_node->old_peak_bw;
					chn_hrt_r_bw[comm_id][chnn_id] += (src->peak_bw / 2);
					larb_node->old_peak_bw = (src->peak_bw / 2);
				} else {
					chn_hrt_r_bw[comm_id][chnn_id] -= larb_node->old_peak_bw;
					chn_hrt_r_bw[comm_id][chnn_id] += src->peak_bw;
					larb_node->old_peak_bw = src->peak_bw;
				}

				chn_srt_r_bw[comm_id][chnn_id] -= larb_node->old_avg_bw;
				chn_srt_r_bw[comm_id][chnn_id] += src->avg_bw;
				larb_node->old_avg_bw = src->avg_bw;

				if (log_level & 1 << log_bw)
					pr_notice("comm=%d chnn=%d larb=%d avg_bw:%d peak_bw:%d s_r:%d h_r:%d\n",
						comm_id, chnn_id, LARB_ID(src->id),
						icc_to_MBps(src->avg_bw),
						icc_to_MBps(src->peak_bw),
						icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
						icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]));
			}
#endif
			if (mmqos_met_enabled()) {
				if (is_path_write == path_no_type) {
					if (!larb_node->is_write)
						r_w_type = "r";
					else
						r_w_type = "w";
					trace_comm_id = (larb_node->channel_v2 >> 4) & 0xf;
					trace_chnn_id = larb_node->channel_v2 & 0xf;
					trace_chnn_id -= 1;
					trace_mmqos__larb_avg_bw(
						r_w_type,
						src->name,
						icc_to_MBps(src->avg_bw),
						trace_comm_id,
						trace_chnn_id);
					trace_mmqos__larb_peak_bw(
						r_w_type,
						src->name,
						icc_to_MBps(src->peak_bw),
						trace_comm_id,
						trace_chnn_id);
				}
				is_path_write = path_no_type;
#ifdef ENABLE_INTERCONNECT_V1
				trace_mmqos__chn_bw(comm_id, chnn_id,
					icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_srt_w_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]),
					icc_to_MBps(chn_hrt_w_bw[comm_id][chnn_id]));
#endif
			}
		}

		mutex_lock(&comm_port_node->bw_lock);
#ifdef ENABLE_INTERCONNECT_V1
		if (log_level & 1 << log_v2_dbg)
			pr_notice("[mmqos][MTK_MMQOS_NODE_COMMON_PORT] v1_mix:%d v2_mix:%d node_name:%s\n",
					comm_port_node->base->mix_bw, dst->v2_mix_bw, dst->name);
		if (comm_port_node->latest_mix_bw == comm_port_node->base->mix_bw
			&& comm_port_node->latest_peak_bw == dst->peak_bw
			&& comm_port_node->latest_avg_bw == dst->avg_bw) {
			mutex_unlock(&comm_port_node->bw_lock);
			break;
		}
		comm_port_node->latest_mix_bw = comm_port_node->base->mix_bw;
#endif
#ifdef ENABLE_INTERCONNECT_V2
		if (comm_port_node->latest_mix_bw == dst->v2_mix_bw
			&& comm_port_node->latest_peak_bw == dst->peak_bw
			&& comm_port_node->latest_avg_bw == dst->avg_bw) {
			mutex_unlock(&comm_port_node->bw_lock);
			break;
		}
		comm_port_node->latest_mix_bw = dst->v2_mix_bw;
#endif
		comm_port_node->latest_peak_bw = dst->peak_bw;
		comm_port_node->latest_avg_bw = dst->avg_bw;
		port_id = MASK_8(dst->id);
		if (mmqos_state & BWL_ENABLE)
			mmqos_update_comm_bw(comm_port_node->larb_dev,
				port_id, comm_port_node->common->freq,
				icc_to_MBps(comm_port_node->latest_mix_bw),
				icc_to_MBps(comm_port_node->latest_peak_bw),
				mmqos->qos_bound, comm_port_node->hrt_type == HRT_MAX_BWL);

		if ((mmqos_state & COMM_OSTDL_ENABLE)
			&& larb_node->is_report_bw_larbs) {
			if (mmqos_state & VCODEC_BW_BYPASS
				&& comm_port_node->hrt_type == SRT_VDEC) {
				if (log_level & 1 << log_bw)
					pr_notice("ignore SRT_VDEC comm port:%#x OSTDL\n", dst->id);
			} else if (mmqos_state & VCODEC_BW_BYPASS
				&& comm_port_node->hrt_type == SRT_VENC) {
				if (log_level & 1 << log_bw)
					pr_notice("ignore SRT_VENC comm port:%#x OSTDL\n", dst->id);
			} else {
				if (log_level & 1 << log_bw)
					pr_notice("comm=%d port=%d larb=%d avg_bw:%d peak_bw:%d\n",
						comm_id, MASK_8(dst->id), LARB_ID(src->id),
						icc_to_MBps(src->avg_bw),
						icc_to_MBps(src->peak_bw));
				mmqos_update_comm_ostdl(comm_port_node->larb_dev,
					port_id, mmqos->max_ratio, src);
			}
		}

		comm_id = COMM_PORT_COMM_ID(dst->id);
		if (port_id != VIRT_COMM_PORT_ID)
			chnn_id = comm_port_node->channel - 1;
#ifdef ENABLE_INTERCONNECT_V1
		if (log_level & 1 << log_bw)
			pr_notice("%s comm %d port %d chnn %d larb %d %d %d latest %d %llu chn %d %d %d %d\n",
				__func__,
				comm_id, port_id, chnn_id, LARB_ID(src->id),
				icc_to_MBps(src->avg_bw),
				icc_to_MBps(src->peak_bw),
				icc_to_MBps(comm_port_node->latest_avg_bw),
				icc_to_MBps(comm_port_node->latest_peak_bw),
				icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_srt_w_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_hrt_w_bw[comm_id][chnn_id]));
#endif
		record_comm_port_bw(comm_id, port_id, LARB_ID(src->id),
			src->avg_bw, src->peak_bw,
			comm_port_node->latest_avg_bw,
			comm_port_node->latest_peak_bw);
#ifdef ENABLE_INTERCONNECT_V1
		record_chn_bw(comm_id, chnn_id,
			chn_srt_r_bw[comm_id][chnn_id],
			chn_srt_w_bw[comm_id][chnn_id],
			chn_hrt_r_bw[comm_id][chnn_id],
			chn_hrt_w_bw[comm_id][chnn_id]);
#endif
		mutex_unlock(&comm_port_node->bw_lock);
		break;
	case MTK_MMQOS_NODE_LARB:
		larb_port_node = (struct larb_port_node *)src->data;
		larb_node = (struct larb_node *)dst->data;
		if (!larb_port_node || !larb_node || !larb_node->larb_dev)
			break;
		/* update channel BW */
		comm_id = (larb_port_node->channel >> 4) & 0xf;
		chnn_id = larb_port_node->channel & 0xf;
#ifdef ENABLE_INTERCONNECT_V1
		if (chnn_id) {
			chnn_id -= 1;
			if (larb_port_node->is_write) {
				chn_hrt_w_bw[comm_id][chnn_id] -= larb_port_node->old_peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] -= larb_port_node->old_avg_bw;
				chn_hrt_w_bw[comm_id][chnn_id] += src->peak_bw;
				chn_srt_w_bw[comm_id][chnn_id] += src->avg_bw;
				larb_port_node->old_peak_bw = src->peak_bw;
				larb_port_node->old_avg_bw = src->avg_bw;
			} else {
				chn_hrt_r_bw[comm_id][chnn_id] -= larb_port_node->old_peak_bw;
				chn_srt_r_bw[comm_id][chnn_id] -= larb_port_node->old_avg_bw;
				chn_hrt_r_bw[comm_id][chnn_id] += src->peak_bw;
				chn_srt_r_bw[comm_id][chnn_id] += src->avg_bw;
				larb_port_node->old_peak_bw = src->peak_bw;
				larb_port_node->old_avg_bw = src->avg_bw;
			}
		}
		if (log_level & 1 << log_v2_dbg)
			pr_notice("[mmqos][MTK_MMQOS_NODE_LARB] v1_mix:%d v2_mix:%d node_name:%s\n",
				larb_port_node->base->mix_bw, src->v2_mix_bw, src->name);

		if (larb_port_node->base->mix_bw) {
			value = SHIFT_ROUND(
				icc_to_MBps(larb_port_node->base->mix_bw),
				larb_port_node->bw_ratio);
			if (src->peak_bw)
				value = SHIFT_ROUND(value * 3, 1);
		} else {
			larb_port_node->is_max_ostd = false;
		}
		if (value > mmqos->max_ratio || larb_port_node->is_max_ostd)
			value = mmqos->max_ratio;
#endif
#ifdef ENABLE_INTERCONNECT_V2
		if (icc_to_MBps(src->v2_mix_bw)) {
			value = SHIFT_ROUND(
				icc_to_MBps(src->v2_mix_bw),
				larb_port_node->bw_ratio);
			if (src->peak_bw ||
				(src->v2_max_ostd && !is_max_bw_to_max_ostdl_policy()))
				value = SHIFT_ROUND(value * 3, 1);
		} else {
			src->v2_max_ostd = false;
		}
		if (value > mmqos->max_ratio) {
			if (value > mmqos->max_ratio)
				dev_notice(larb_node->larb_dev,
					"larb=%d port=%d avg_bw:%d peak_bw:%d mix_bw:%d ostd=%#x\n",
					MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
					icc_to_MBps(larb_port_node->base->icc_node->avg_bw),
					icc_to_MBps(larb_port_node->base->icc_node->peak_bw),
					icc_to_MBps(src->v2_mix_bw),
					value);
			value = mmqos->max_ratio;
		}
		if (src->v2_max_ostd && is_max_bw_to_max_ostdl_policy()) {
			if (log_level & 1 << log_bw)
				MMQOS_DBG("larb=%d port=%d use max_disp_ostdl=%#x",
					MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
					mmqos->max_disp_ostdl);
			value = mmqos->max_disp_ostdl;
		}
#endif
		if (mmqos_state & OSTD_ENABLE) {
			record_larb_port_bw_ostdl(MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->avg_bw),
				icc_to_MBps(larb_port_node->base->icc_node->peak_bw),
				icc_to_MBps(src->v2_mix_bw),
				value);
			mtk_smi_larb_bw_set(
				larb_node->larb_dev,
				MTK_M4U_TO_PORT(src->id), value);
		}

		if (log_level & 1 << log_bw)
			dev_notice(larb_node->larb_dev,
				"larb=%d port=%d avg_bw:%d peak_bw:%d mix_bw:%d ostd=%#x\n",
				MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->avg_bw),
				icc_to_MBps(larb_port_node->base->icc_node->peak_bw),
				icc_to_MBps(src->v2_mix_bw),
				value);

		record_last_larb_port(src->id,
			larb_port_node->base->icc_node->avg_bw,
			larb_port_node->base->icc_node->peak_bw);

		if (mmqos_met_enabled()) {
			if (!larb_port_node->is_write) {
				r_w_type = "r";
				is_path_write = path_read;
			} else {
				r_w_type = "w";
				is_path_write = path_write;
			}
			trace_comm_id = (larb_node->channel_v2 >> 4) & 0xf;
			trace_chnn_id = larb_node->channel_v2 & 0xf;
			trace_chnn_id -= 1;
			trace_mmqos__larb_port_avg_bw(
				r_w_type,
				dst->name,
				MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->avg_bw),
				trace_comm_id,
				trace_chnn_id);
			trace_mmqos__larb_port_peak_bw(
				r_w_type,
				dst->name,
				MTK_M4U_TO_LARB(src->id), MTK_M4U_TO_PORT(src->id),
				icc_to_MBps(larb_port_node->base->icc_node->peak_bw),
				trace_comm_id,
				trace_chnn_id);
#ifdef ENABLE_INTERCONNECT_V1
			trace_mmqos__chn_bw(comm_id, chnn_id,
				icc_to_MBps(chn_srt_r_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_srt_w_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_hrt_r_bw[comm_id][chnn_id]),
				icc_to_MBps(chn_hrt_w_bw[comm_id][chnn_id]));
#endif
		}
		//queue_work(mmqos->wq, &larb_node->work);
		break;
	default:
		break;
	}
	MMQOS_SYSTRACE_END();
	return 0;
}

static int mtk_mmqos_aggregate(struct icc_node *node,
	u32 tag, u32 avg_bw, u32 peak_bw, u32 *agg_avg,
	u32 *agg_peak)
{
	struct mmqos_base_node *base_node = NULL;
	struct larb_port_node *larb_port_node;
	u32 mix_bw = peak_bw;

	if (!node || !node->data)
		return 0;

	MMQOS_SYSTRACE_BEGIN("%s %s\n", __func__, node->name);
	switch (NODE_TYPE(node->id)) {
	case MTK_MMQOS_NODE_LARB_PORT:
		larb_port_node = (struct larb_port_node *)node->data;
		base_node = larb_port_node->base;
		if (peak_bw) {
			if (peak_bw == MTK_MMQOS_MAX_BW) {
				larb_port_node->is_max_ostd = true;
				mix_bw = max_t(u32, avg_bw, 1000);
			} else {
				mix_bw = peak_bw;
			}
		}
		break;
	case MTK_MMQOS_NODE_COMMON_PORT:
		base_node = ((struct common_port_node *)node->data)->base;
		break;
	//default:
	//	return 0;
	}
	if (base_node) {
		if (*agg_avg == 0 && *agg_peak == 0)
			base_node->mix_bw = 0;
		base_node->mix_bw += peak_bw ? mix_bw : avg_bw;
	}

	*agg_avg += avg_bw;

	if (peak_bw == MTK_MMQOS_MAX_BW)
		*agg_peak += 1000; /* for BWL soft mode */
	else
		*agg_peak += peak_bw;

	MMQOS_SYSTRACE_END();
	return 0;
}

static bool mtk_mmqos_path_is_write(struct icc_node *node)
{
	struct larb_port_node *larb_port_node = NULL;
	struct larb_node *larb_node = NULL;

	switch (NODE_TYPE(node->id)) {
	case MTK_MMQOS_NODE_LARB_PORT:
		larb_port_node = (struct larb_port_node *)node->data;
		if (larb_port_node->is_write) {
			if (log_level & 1 << log_v2_dbg)
				pr_notice("[mmqos][port] node_id:0x%x is_write:%d\n",
						 node->id, larb_port_node->is_write);
			return true;
		}
		if (log_level & 1 << log_v2_dbg)
			pr_notice("[mmqos][port] node_id:0x%x is_write:%d\n",
						 node->id, larb_port_node->is_write);

		break;
	case MTK_MMQOS_NODE_LARB:
		larb_node = (struct larb_node *)node->data;
		if (larb_node->is_write) {
			if (log_level & 1 << log_v2_dbg)
				pr_notice("[mmqos][larb] node_id:0x%x is_write:%d\n",
						 node->id, larb_node->is_write);
			return true;
		}
		if (log_level & 1 << log_v2_dbg)
			pr_notice("[mmqos][larb] node_id:0x%x is_write:%d\n",
						node->id, larb_node->is_write);
		break;
	}
	return false;
}

static struct icc_node *mtk_mmqos_xlate(
	struct of_phandle_args *spec, void *data)
{
	struct icc_onecell_data *icc_data;
	s32 i;

	if (!spec || !data)
		return ERR_PTR(-EPROBE_DEFER);
	icc_data = (struct icc_onecell_data *)data;
	for (i = 0; i < icc_data->num_nodes; i++)
		if (icc_data->nodes[i]->id == spec->args[0])
			return icc_data->nodes[i];
	pr_notice("%s: invalid index %u\n", __func__, spec->args[0]);
	return ERR_PTR(-EINVAL);
}

static void larb_port_ostdl_dump(struct seq_file *file, u32 larb_id, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = larb_port_bw_rec->time[larb_id][i];
	rem_nsec = do_div(ts, 1000000000);
	if (ts == 0 &&
		larb_port_bw_rec->port_id[larb_id][i] == 0 &&
		larb_port_bw_rec->avg_bw[larb_id][i] == 0 &&
		larb_port_bw_rec->peak_bw[larb_id][i] == 0 &&
		larb_port_bw_rec->ostdl[larb_id][i] == 0)
		return;

	seq_printf(file, "[%5llu.%06llu] larb%2d port%2d %8d %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		larb_id,
		larb_port_bw_rec->port_id[larb_id][i],
		larb_port_bw_rec->avg_bw[larb_id][i],
		larb_port_bw_rec->peak_bw[larb_id][i],
		larb_port_bw_rec->mix_bw[larb_id][i],
		larb_port_bw_rec->ostdl[larb_id][i]);
}

static void comm_port_bw_dump(struct seq_file *file, u32 comm_id, u32 port_id, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = comm_port_bw_rec->time[comm_id][port_id][i];
	rem_nsec = do_div(ts, 1000000000);
	if (ts == 0 &&
		comm_port_bw_rec->avg_bw[comm_id][port_id][i] == 0 &&
		comm_port_bw_rec->peak_bw[comm_id][port_id][i] == 0 &&
		comm_port_bw_rec->l_avg_bw[comm_id][port_id][i] == 0 &&
		comm_port_bw_rec->l_peak_bw[comm_id][port_id][i] == 0)
		return;

	seq_printf(file, "[%5llu.%06llu] comm%d port%d larb%2d %8d %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		comm_id, port_id,
		comm_port_bw_rec->larb_id[comm_id][port_id][i],
		icc_to_MBps(comm_port_bw_rec->avg_bw[comm_id][port_id][i]),
		icc_to_MBps(comm_port_bw_rec->peak_bw[comm_id][port_id][i]),
		icc_to_MBps(comm_port_bw_rec->l_avg_bw[comm_id][port_id][i]),
		icc_to_MBps(comm_port_bw_rec->l_peak_bw[comm_id][port_id][i]));
}

static void chn_bw_dump(struct seq_file *file, u32 comm_id, u32 chnn_id, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = chn_bw_rec->time[comm_id][chnn_id][i];
	rem_nsec = do_div(ts, 1000000000);
	seq_printf(file, "[%5llu.%06llu] comm%d_%d %8d %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		comm_id, chnn_id,
		icc_to_MBps(chn_bw_rec->srt_r_bw[comm_id][chnn_id][i]),
		icc_to_MBps(chn_bw_rec->srt_w_bw[comm_id][chnn_id][i]),
		icc_to_MBps(chn_bw_rec->hrt_r_bw[comm_id][chnn_id][i]),
		icc_to_MBps(chn_bw_rec->hrt_w_bw[comm_id][chnn_id][i]));
}

static void hrt_bw_dump(struct seq_file *file, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = g_hrt->hrt_rec.time[i];
	rem_nsec = do_div(ts, 1000000000);
	seq_printf(file, "[%5llu.%06llu]     %8d %8d %8d\n",
		(u64)ts, rem_nsec / 1000,
		g_hrt->hrt_rec.avail_hrt[i],
		g_hrt->hrt_rec.cam_max_hrt[i],
		g_hrt->hrt_rec.cam_hrt[i]);
}

static void cam_hrt_bw_dump(struct seq_file *file, u32 i)
{
	u64 ts;
	u64 rem_nsec;

	ts = g_hrt->cam_hrt_rec.time[i];
	rem_nsec = do_div(ts, 1000000000);
	seq_printf(file, "[%5llu.%06llu]     %17d\n",
		(u64)ts, rem_nsec / 1000,
		g_hrt->cam_hrt_rec.cam_max_hrt[i]);
}

static void larb_port_ostdl_full_dump(struct seq_file *file, u32 larb_id)
{
	u32 i, start;

	start = larb_port_bw_rec->idx[larb_id];
	for (i = start; i < RECORD_NUM; i++)
		larb_port_ostdl_dump(file, larb_id, i);

	for (i = 0; i < start; i++)
		larb_port_ostdl_dump(file, larb_id, i);

}

static void comm_port_bw_full_dump(struct seq_file *file, u32 comm_id, u32 port_id)
{
	u32 i, start;

	start = comm_port_bw_rec->idx[comm_id][port_id];
	for (i = start; i < RECORD_NUM; i++)
		comm_port_bw_dump(file, comm_id, port_id, i);

	for (i = 0; i < start; i++)
		comm_port_bw_dump(file, comm_id, port_id, i);

}

static void chn_bw_full_dump(struct seq_file *file, u32 comm_id, u32 chnn_id)
{
	u32 i, start;

	start = chn_bw_rec->idx[comm_id][chnn_id];
	for (i = start; i < RECORD_NUM; i++)
		chn_bw_dump(file, comm_id, chnn_id, i);

	for (i = 0; i < start; i++)
		chn_bw_dump(file, comm_id, chnn_id, i);

}

static void hrt_bw_full_dump(struct seq_file *file)
{
	u32 i, start;
	struct hrt_record *rec = &g_hrt->hrt_rec;

	start = rec->idx;
	for (i = start; i < RECORD_NUM; i++)
		hrt_bw_dump(file, i);

	for (i = 0; i < start; i++)
		hrt_bw_dump(file, i);

}

static void cam_hrt_bw_full_dump(struct seq_file *file)
{
	u32 i, start;
	struct cam_hrt_record *rec = &g_hrt->cam_hrt_rec;

	start = rec->idx;
	for (i = start; i < RECORD_NUM; i++)
		cam_hrt_bw_dump(file, i);

	for (i = 0; i < start; i++)
		cam_hrt_bw_dump(file, i);

}

static int mmqos_last_dump(struct seq_file *file, void *data)
{
	u64 ts;
	u64 ns;

	seq_puts(file, "last larb port:\n");
	ts = last_rec->larb_port_update_time;
	ns = do_div(ts, 1000000000);
	seq_printf(file, "[%5llu.%06llu] %#x %u %u\n",
		(u64)ts, ns / 1000,
		last_rec->larb_port_node_id,
		last_rec->larb_port_avg_bw,
		last_rec->larb_port_peak_bw);
	seq_puts(file, "last larb:\n");
	ts = last_rec->larb_update_time;
	ns = do_div(ts, 1000000000);
	seq_printf(file, "[%5llu.%06llu] %#x %u %u",
		(u64)ts, ns / 1000,
		last_rec->larb_node_id,
		last_rec->larb_avg_bw,
		last_rec->larb_peak_bw);
	return 0;
}

static int mmqos_last_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmqos_last_dump, inode->i_private);
}

static const struct proc_ops mmqos_last_debug_fops = {
	.proc_open = mmqos_last_debug_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int mmqos_bw_dump(struct seq_file *file, void *data)
{
	u32 comm_id = 0, chnn_id = 0, port_id = 0, larb_id = 0;

	seq_printf(file, "MMQoS HRT BW Dump: %8s %8s %8s\n",
		"avail", "cam_max", "cam_hrt");
	hrt_bw_full_dump(file);

	seq_printf(file, "MMQoS CAM HRT BW Dump:      %8s\n", "cam_max");
	cam_hrt_bw_full_dump(file);

	seq_printf(file, "MMQoS Channel BW Dump: %8s %8s %8s %8s\n",
		"s_r", "s_w", "h_r", "h_w");
	for (comm_id = 0; comm_id < MAX_RECORD_COMM_NUM; comm_id++) {
		for (chnn_id = 0; chnn_id < MMQOS_COMM_CHANNEL_NUM; chnn_id++)
			chn_bw_full_dump(file, comm_id, chnn_id);
	}

	seq_printf(file, "MMQoS Common Port BW Dump:        %8s %8s %8s %8s\n",
		"avg", "peak", "l_avg", "l_peak");
	for (comm_id = 0; comm_id < MAX_RECORD_COMM_NUM; comm_id++) {
		for (port_id = 0; port_id < MAX_RECORD_PORT_NUM; port_id++)
			comm_port_bw_full_dump(file, comm_id, port_id);
	}

	seq_printf(file, "MMQoS OSTDL Dump:            %8s %8s %8s %8s\n",
		"avg_bw", "peak_bw", "mix_bw", "ostdl");
	for (larb_id = 0; larb_id < MAX_RECORD_LARB_NUM; larb_id++)
		larb_port_ostdl_full_dump(file, larb_id);

	return 0;
}

static int mmqos_debug_opp_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmqos_bw_dump, inode->i_private);
}

static const struct proc_ops mmqos_debug_fops = {
	.proc_open = mmqos_debug_opp_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int mtk_mmqos_probe(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos;
	struct of_phandle_iterator it;
	struct icc_onecell_data *data;
	struct icc_node *node, *temp;
	struct mmqos_base_node *base_node;
	struct common_node *comm_node;
	struct common_port_node *comm_port_node;
	struct larb_node *larb_node;
	struct larb_port_node *larb_port_node;
	struct mtk_iommu_data *smi_imu;
	int i, j, id, num_larbs = 0, ret, ddr_type, max_freq, chn_cnt;
	int step, dram_freq;
	const struct mtk_mmqos_desc *mmqos_desc;
	const struct mtk_node_desc *node_desc;
	struct device *larb_dev;
	struct mmqos_hrt *hrt;
	struct device_node *np;
	struct platform_device *comm_pdev, *larb_pdev;
	struct proc_dir_entry *dir, *proc, *last_proc;
	phys_addr_t pa = 0ULL;
	unsigned long va;

	mmqos = devm_kzalloc(&pdev->dev, sizeof(*mmqos), GFP_KERNEL);
	if (!mmqos)
		return -ENOMEM;
	gmmqos = mmqos;

	mmqos->dev = &pdev->dev;
	smi_imu = devm_kzalloc(&pdev->dev, sizeof(*smi_imu), GFP_KERNEL);
	if (!smi_imu)
		return -ENOMEM;

	last_rec = kzalloc(sizeof(*last_rec), GFP_KERNEL);
	if (!last_rec)
		return -ENOMEM;

	va = (unsigned long) last_rec;
	pa = __pa_nodebug(va);
	if (va && pa) {
		ret = mrdump_mini_add_extra_file(va, pa, PAGE_SIZE, "LAST_MMQOS");
		if (ret)
			MMQOS_DBG("failed:%d va:%#lx pa:%pa", ret, va, &pa);
	}

	chn_bw_rec = devm_kzalloc(&pdev->dev,
		sizeof(*chn_bw_rec), GFP_KERNEL);
	if (!chn_bw_rec)
		return -ENOMEM;

	comm_port_bw_rec = devm_kzalloc(&pdev->dev,
		sizeof(*comm_port_bw_rec), GFP_KERNEL);
	if (!comm_port_bw_rec)
		return -ENOMEM;

	larb_port_bw_rec = devm_kzalloc(&pdev->dev,
		sizeof(*larb_port_bw_rec), GFP_KERNEL);
	if (!larb_port_bw_rec)
		return -ENOMEM;

	of_for_each_phandle(
		&it, ret, pdev->dev.of_node, "mediatek,larbs-supply", NULL, 0) {
		np = of_node_get(it.node);
		if (!of_device_is_available(np))
			continue;
		larb_pdev = of_find_device_by_node(np);
		if (!larb_pdev) {
			larb_pdev = of_platform_device_create(
				np, NULL, platform_bus_type.dev_root);
			if (!larb_pdev || !larb_pdev->dev.driver) {
				of_node_put(np);
				return -EPROBE_DEFER;
			}
		}
		if (of_property_read_u32(np, "mediatek,larb-id", &id))
			id = num_larbs;
		smi_imu->larb_imu[id].dev = &larb_pdev->dev;
		num_larbs += 1;
	}
	INIT_LIST_HEAD(&mmqos->comm_list);
	INIT_LIST_HEAD(&mmqos->prov.nodes);
	mmqos->prov.set = mtk_mmqos_set;
	mmqos->prov.aggregate = mtk_mmqos_aggregate;
	mmqos->prov.path_is_write = mtk_mmqos_path_is_write;
	mmqos->prov.xlate = mtk_mmqos_xlate;
	mmqos->prov.dev = &pdev->dev;
	ret = mtk_icc_provider_add(&mmqos->prov);
	if (ret) {
		dev_notice(&pdev->dev, "mtk_icc_provider_add failed:%d\n", ret);
		return ret;
	}
	mmqos_desc = (struct mtk_mmqos_desc *)
		of_device_get_match_data(&pdev->dev);
	if (!mmqos_desc) {
		ret = -EINVAL;
		goto err;
	}

	freq_mode = mmqos_desc->freq_mode ? mmqos_desc->freq_mode : freq_mode;
	MMQOS_DBG("freq_mode:%d", freq_mode);

	data = devm_kzalloc(&pdev->dev,
		sizeof(*data) + mmqos_desc->num_nodes * sizeof(node),
		GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	for (i = 0; i < mmqos_desc->num_nodes; i++) {
		node_desc = &mmqos_desc->nodes[i];
		node = mtk_icc_node_create(node_desc->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}
		mtk_icc_node_add(node, &mmqos->prov);
		if (node_desc->link != MMQOS_NO_LINK) {
			ret = mtk_icc_link_create(node, node_desc->link);
			if (ret)
				goto err;
		}
		node->name = node_desc->name;
		base_node = devm_kzalloc(
			&pdev->dev, sizeof(*base_node), GFP_KERNEL);
		if (!base_node) {
			ret = -ENOMEM;
			goto err;
		}
		base_node->icc_node = node;
		switch (NODE_TYPE(node->id)) {
		case MTK_MMQOS_NODE_COMMON:
			comm_node = devm_kzalloc(
				&pdev->dev, sizeof(*comm_node), GFP_KERNEL);
			if (!comm_node) {
				ret = -ENOMEM;
				goto err;
			}
			//INIT_WORK(&comm_node->work, set_comm_icc_bw_handler);
			comm_node->clk = devm_clk_get(&pdev->dev,
				mmqos_desc->comm_muxes[MASK_8(node->id)]);
			if (IS_ERR(comm_node->clk)) {
				dev_notice(&pdev->dev, "get clk fail:%s\n",
					mmqos_desc->comm_muxes[
						MASK_8(node->id)]);
				ret = -EINVAL;
				goto err;
			}

			comm_node->freq = clk_get_rate(comm_node->clk)/1000000;
			INIT_LIST_HEAD(&comm_node->list);
			list_add_tail(&comm_node->list, &mmqos->comm_list);
			INIT_LIST_HEAD(&comm_node->comm_port_list);
			comm_node->icc_path = of_icc_get(&pdev->dev,
				mmqos_desc->comm_icc_path_names[
						MASK_8(node->id)]);
			if (IS_ERR_OR_NULL(comm_node->icc_path)) {
				dev_notice(&pdev->dev,
					"get icc_path fail:%s\n",
					mmqos_desc->comm_icc_path_names[
						MASK_8(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			comm_node->icc_hrt_path = of_icc_get(&pdev->dev,
				mmqos_desc->comm_icc_hrt_path_names[
						MASK_8(node->id)]);
			if (IS_ERR_OR_NULL(comm_node->icc_hrt_path)) {
				dev_notice(&pdev->dev,
					"get icc_hrt_path fail:%s\n",
					mmqos_desc->comm_icc_hrt_path_names[
						MASK_8(node->id)]);
				ret = -EINVAL;
				goto err;
			}
			np = of_parse_phandle(pdev->dev.of_node,
					      "mediatek,commons-supply",
					      MASK_8(node->id));
			if (!of_device_is_available(np)) {
				pr_notice("get common(%d) dev fail\n",
					  MASK_8(node->id));
				break;
			}
			comm_pdev = of_find_device_by_node(np);
			if (comm_pdev)
				comm_node->comm_dev = &comm_pdev->dev;
			else
				pr_notice("comm(%d) pdev is null\n",
					  MASK_8(node->id));
			comm_node->comm_reg =
				devm_regulator_get_optional(comm_node->comm_dev,
						   "mmdvfs-dvfsrc-vcore");
			if (freq_mode == BY_REGULATOR)
				if (IS_ERR_OR_NULL(comm_node->comm_reg))
					pr_notice("get common(%d) reg fail\n",
					  MASK_8(node->id));

			dev_pm_opp_of_add_table(comm_node->comm_dev);
			comm_node->base = base_node;
			node->data = (void *)comm_node;
			break;
		case MTK_MMQOS_NODE_COMMON_PORT:
			comm_port_node = devm_kzalloc(&pdev->dev,
				sizeof(*comm_port_node), GFP_KERNEL);
			if (!comm_port_node) {
				ret = -ENOMEM;
				goto err;
			}
			comm_port_node->channel =
				mmqos_desc->comm_port_channels[
				MASK_8((node->id >> 8))][MASK_8(node->id)];
			comm_port_node->channel_v2 = node_desc->channel;
			comm_port_node->hrt_type =
				mmqos_desc->comm_port_hrt_types[
				MASK_8((node->id >> 8))][MASK_8(node->id)];
			mutex_init(&comm_port_node->bw_lock);
			comm_port_node->common = node->links[0]->data;
			INIT_LIST_HEAD(&comm_port_node->list);
			list_add_tail(&comm_port_node->list,
				      &comm_port_node->common->comm_port_list);
			comm_port_node->base = base_node;
			node->data = (void *)comm_port_node;
			break;
		case MTK_MMQOS_NODE_LARB:
			larb_node = devm_kzalloc(
				&pdev->dev, sizeof(*larb_node), GFP_KERNEL);
			if (!larb_node) {
				ret = -ENOMEM;
				goto err;
			}
			comm_port_node = node->links[0]->data;
			larb_dev = smi_imu->larb_imu[node->id &
					(MTK_LARB_NR_MAX-1)].dev;
			if (larb_dev) {
				comm_port_node->larb_dev = larb_dev;
				larb_node->larb_dev = larb_dev;
			}
			//INIT_WORK(&larb_node->work, set_larb_icc_bw_handler);

			larb_node->channel = node_desc->channel;
			larb_node->channel_v2 = comm_port_node->channel_v2;
			larb_node->is_write = node_desc->is_write;
			larb_node->bw_ratio = node_desc->bw_ratio;
			/* init disable dualpipe */
			gmmqos->dual_pipe_enable = false;

			for (j = 0; j < MMQOS_MAX_REPORT_LARB_NUM; j++) {
				if (node->id == mmqos_desc->report_bw_larbs[j]) {
					larb_node->is_report_bw_larbs = true;
					id = mmqos_desc->report_bw_real_larbs[j];
					larb_dev = smi_imu->larb_imu[id & (MTK_LARB_NR_MAX-1)].dev;
					larb_node->larb_dev = larb_dev;
				}
			}
			larb_node->base = base_node;
			node->data = (void *)larb_node;
			break;
		case MTK_MMQOS_NODE_LARB_PORT:
			larb_port_node = devm_kzalloc(&pdev->dev,
				sizeof(*larb_port_node), GFP_KERNEL);
			if (!larb_port_node) {
				ret = -ENOMEM;
				goto err;
			}
			larb_port_node->channel = node_desc->channel;
			larb_port_node->is_write = node_desc->is_write;
			larb_port_node->bw_ratio = node_desc->bw_ratio;
			larb_port_node->base = base_node;
			node->data = (void *)larb_port_node;
			break;
		default:
			dev_notice(&pdev->dev,
				"invalid node id:%#x\n", node->id);
			ret = -EINVAL;
			goto err;
		}
		data->nodes[i] = node;
	}
	data->num_nodes = mmqos_desc->num_nodes;
	mmqos->prov.data = data;


	mmqos->max_ratio = mmqos_desc->max_ratio;
	if (mmqos_desc->max_disp_ostdl == 0)
		mmqos->max_disp_ostdl = mmqos_desc->max_ratio;
	else
		mmqos->max_disp_ostdl = mmqos_desc->max_disp_ostdl;

	of_property_read_u32(pdev->dev.of_node, "mmqos-state", &mmqos_state);
	pr_notice("[mmqos] mmqos probe state: %#x\n", mmqos_state);

	of_property_read_u32(pdev->dev.of_node, "mmqos-log-level", &log_level);
	pr_notice("[mmqos] mmqos log level: %#x\n", log_level);

	for (i = 0 ; i < MMQOS_MAX_DISP_VIRT_LARB_NUM ; i++)
		mmqos->disp_virt_larbs[i] = mmqos_desc->disp_virt_larbs[i];

	/*
	mmqos->wq = create_singlethread_workqueue("mmqos_work_queue");
	if (!mmqos->wq) {
		dev_notice(&pdev->dev, "work queue create fail\n");
		ret = -ENOMEM;
		goto err;
	}
	*/
	hrt = devm_kzalloc(&pdev->dev, sizeof(*hrt), GFP_KERNEL);
	if (!hrt) {
		ret = -ENOMEM;
		goto err;
	}

	ddr_type = mtk_dramc_get_ddr_type();
	if (ddr_type == TYPE_LPDDR4 ||
		ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4P)
		memcpy(hrt, &mmqos_desc->hrt_LPDDR4, sizeof(mmqos_desc->hrt_LPDDR4));
	else
		memcpy(hrt, &mmqos_desc->hrt, sizeof(mmqos_desc->hrt));
	pr_notice("[mmqos] ddr type: %d\n", mtk_dramc_get_ddr_type());

#if IS_ENABLED(CONFIG_MTK_DRAMC) && IS_ENABLED(CONFIG_MTK_EMI)
	max_freq = mtk_dramc_get_steps_freq(0);
	chn_cnt = mtk_emicen_get_ch_cnt();
	if (max_freq > 0 && chn_cnt > 0)
		hrt->hrt_total_bw = max_freq * chn_cnt * 2;
	else
		MMQOS_ERR("dramc or emi not ready, cannot get max frequency or channel count");
	MMQOS_DBG("max_freq:%d, channel count:%d", max_freq, chn_cnt);

	for (step = 0; step < MMQOS_MAX_DVFS_STEP_NUM; step++) {
		dram_freq = mtk_dramc_get_steps_freq(step);
		dramc_freq_mapping_table[step][0] = dram_freq;
		if (dram_freq < 0) {
			dramc_freq_mapping_table[step - 1][1] *= 2;
			dramc_freq_mapping_table[step][1] = 0;
		} else if (dram_freq > 6000)
			dramc_freq_mapping_table[step][1] = 728;
		else
			dramc_freq_mapping_table[step][1] = dram_freq / 8;
		MMQOS_DBG("dram_opp_table step %d: DRAM freq:%d emiclk:%d",
			step, dramc_freq_mapping_table[step][0], dramc_freq_mapping_table[step][1]);
	}
#endif
	MMQOS_DBG("hrt_total_bw: %d", hrt->hrt_total_bw);

	hrt->md_scen = mmqos_desc->md_scen;
	mtk_mmqos_init_hrt(hrt);
	g_hrt = hrt;
	mmqos->nb.notifier_call = update_mm_clk;
	register_mmdvfs_notifier(&mmqos->nb);
	ret = mtk_mmqos_register_hrt_sysfs(&pdev->dev);
	if (ret)
		dev_notice(&pdev->dev, "sysfs create fail\n");
	platform_set_drvdata(pdev, mmqos);
	devm_kfree(&pdev->dev, smi_imu);

	mutex_init(&gmmqos->bw_lock);

	/* create proc file */
	dir = proc_mkdir("mmqos", NULL);
	if (IS_ERR_OR_NULL(dir))
		pr_notice("proc_mkdir failed:%ld\n", PTR_ERR(dir));

	proc = proc_create("mmqos_bw", 0444, dir, &mmqos_debug_fops);
	if (IS_ERR_OR_NULL(proc))
		pr_notice("proc_create failed:%ld\n", PTR_ERR(proc));
	else
		mmqos->proc = proc;

	last_proc = proc_create("last_mmqos", 0444, dir, &mmqos_last_debug_fops);
	if (IS_ERR_OR_NULL(last_proc))
		pr_notice("last proc_create failed:%ld\n", PTR_ERR(last_proc));
	else
		mmqos->last_proc = last_proc;

	return 0;
err:
	list_for_each_entry_safe(node, temp, &mmqos->prov.nodes, node_list) {
		mtk_icc_node_del(node);
		mtk_icc_node_destroy(node->id);
	}
	mtk_icc_provider_del(&mmqos->prov);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_probe);

int mtk_mmqos_v2_probe(struct platform_device *pdev)
{
	struct task_struct *kthr_vcp;
	u32 base_tmp, mminfra_base_tmp;
	int ret, probe_ret;

	probe_ret = mtk_mmqos_probe(pdev);

	ret = of_property_read_u32(pdev->dev.of_node, "vmmrc-base", &base_tmp);
	if (ret >= 0) {
		gmmqos->vmmrc_base = ioremap(base_tmp, 0x40000);
		of_property_read_u32(pdev->dev.of_node, "vmmrc-mask", &gmmqos->apmcu_mask_offset);
		of_property_read_u32(pdev->dev.of_node, "vmmrc-mask-bit", &gmmqos->apmcu_mask_bit);
		of_property_read_u32(pdev->dev.of_node,
			"vmmrc-on-table", &gmmqos->apmcu_on_bw_offset);
		of_property_read_u32(pdev->dev.of_node,
			"vmmrc-off-table", &gmmqos->apmcu_off_bw_offset);
		of_property_read_u32(pdev->dev.of_node, "mminfra-base", &mminfra_base_tmp);
		gmmqos->mminfra_base = ioremap(mminfra_base_tmp, 0x1000);

		MMQOS_DBG("vmmrc base=%#x, mask=%#x, bit=%#x, on table=%#x, off table=%#x, mminfra_base=%#x",
			base_tmp, gmmqos->apmcu_mask_offset, gmmqos->apmcu_mask_bit,
			gmmqos->apmcu_on_bw_offset, gmmqos->apmcu_off_bw_offset,
			mminfra_base_tmp);
	} else {
		MMQOS_DBG("no vmmrc related property");
		gmmqos->vmmrc_base = NULL;
	}

	if (mmqos_state & VCP_ENABLE)
		kthr_vcp = kthread_run(mmqos_vcp_init_thread, NULL, "mmqos-vcp");
	else
		MMQOS_DBG("VCP not enable");

	return probe_ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_v2_probe);

int mtk_mmqos_remove(struct platform_device *pdev)
{
	struct mtk_mmqos *mmqos = platform_get_drvdata(pdev);
	struct icc_node *node, *temp;

	list_for_each_entry_safe(node, temp, &mmqos->prov.nodes, node_list) {
		mtk_icc_node_del(node);
		mtk_icc_node_destroy(node->id);
	}
	mtk_icc_provider_del(&mmqos->prov);
	unregister_mmdvfs_notifier(&mmqos->nb);
	//destroy_workqueue(mmqos->wq);
	mtk_mmqos_unregister_hrt_sysfs(&pdev->dev);
	return 0;
}

void clear_chnn_bw(void)
{
	int i, j;

	for (i = 0; i < MMQOS_MAX_COMM_NUM; i++) {
		for (j = 0; j < MMQOS_COMM_CHANNEL_NUM; j++) {
			chn_srt_r_bw[i][j] = 0;
			chn_srt_w_bw[i][j] = 0;
			chn_hrt_r_bw[i][j] = 0;
			chn_hrt_w_bw[i][j] = 0;
			v2_chn_srt_r_bw[i][j] = 0;
			v2_chn_srt_w_bw[i][j] = 0;
			v2_chn_hrt_r_bw[i][j] = 0;
			v2_chn_hrt_w_bw[i][j] = 0;
			disp_srt_r_bw[i][j] = 0;
			disp_srt_w_bw[i][j] = 0;
			disp_hrt_r_bw[i][j] = 0;
			disp_hrt_w_bw[i][j] = 0;
			emi_chn_hrt_bw[i][j] = 0;
			emi_chn_srt_bw[i][j] = 0;
		}
	}
}
EXPORT_SYMBOL_GPL(clear_chnn_bw);

void check_disp_chnn_bw(int i, int j, const int *ans)
{
	if (i >= 0 && j >= 0) {
		_assert_eq(disp_srt_r_bw[i][j], ans[0], "disp_srt_r");
		_assert_eq(disp_srt_w_bw[i][j], ans[1], "disp_srt_w");
		_assert_eq(disp_hrt_r_bw[i][j], ans[2], "disp_hrt_r");
		_assert_eq(disp_hrt_w_bw[i][j], ans[3], "disp_hrt_w");
	}
}
EXPORT_SYMBOL_GPL(check_disp_chnn_bw);

void check_chnn_bw(int i, int j, int srt_r, int srt_w, int hrt_r, int hrt_w)
{
	if (i >= 0 && j >= 0) {
		_assert_eq(chn_srt_r_bw[i][j], srt_r, "chn_srt_r");
		_assert_eq(chn_srt_w_bw[i][j], srt_w, "chn_srt_w");
		_assert_eq(chn_hrt_r_bw[i][j], hrt_r, "chn_hrt_r");
		_assert_eq(chn_hrt_w_bw[i][j], hrt_w, "chn_hrt_w");
	}
}
EXPORT_SYMBOL_GPL(check_chnn_bw);

struct common_port_node *create_fake_comm_port_node(int hrt_type,
	int srt_r, int srt_w, int hrt_r, int hrt_w)
{
	struct common_port_node *cpn;

	cpn = devm_kzalloc(gmmqos->dev, sizeof(*cpn), GFP_KERNEL);
	if (cpn != NULL) {
		cpn->hrt_type = hrt_type;
		cpn->old_avg_r_bw = srt_r;
		cpn->old_avg_w_bw = srt_w;
		cpn->old_peak_r_bw = hrt_r;
		cpn->old_peak_w_bw = hrt_w;
	}

	return cpn;
}
EXPORT_SYMBOL_GPL(create_fake_comm_port_node);

bool mmqos_met_enabled(void)
{
	return ftrace_ena & (1 << MMQOS_PROFILE_MET);
}

bool mmqos_systrace_enabled(void)
{
	return ftrace_ena & (1 << MMQOS_PROFILE_SYSTRACE);
}

noinline int tracing_mark_write(char *fmt, ...)
{
#if IS_ENABLED(CONFIG_MTK_FTRACER)
	char buf[TRACE_MSG_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= TRACE_MSG_LEN) {
		pr_notice("%s trace size %u exceed limit\n", __func__, len);
		return -1;
	}

	trace_puts(buf);
#endif
	return 0;
}

static int mmqos_dbg_ftrace_thread(void *data)
{
	int retry = 0;

	while (!mmqos_is_init_done()) {
		MMQOS_DBG("start");
		if (++retry > 20) {
			MMQOS_DBG("mmqos vcp init not ready");
			return 0;
		}
		ssleep(2);
	}
	while (!kthread_should_stop()) {
		trace_mmqos__bw_to_emi(TYPE_IS_VCP,
			readl(MEM_VCP_TOTAL_BW), 0);
		trace_mmqos__chn_bw(0, 0,
				RSH_16(readl(MEM_SMI_COMM0_CHN0_BW)),
				MASK_16(readl(MEM_SMI_COMM0_CHN0_BW)),
				0,
				0,
				0,
				TYPE_IS_VCP,
				TYPE_IS_MAX);
		trace_mmqos__chn_bw(0, 1,
				RSH_16(readl(MEM_SMI_COMM0_CHN1_BW)),
				MASK_16(readl(MEM_SMI_COMM0_CHN1_BW)),
				0,
				0,
				0,
				TYPE_IS_VCP,
				TYPE_IS_MAX);
		trace_mmqos__chn_bw(1, 0,
				RSH_16(readl(MEM_SMI_COMM1_CHN0_BW)),
				MASK_16(readl(MEM_SMI_COMM1_CHN0_BW)),
				0,
				0,
				0,
				TYPE_IS_VCP,
				TYPE_IS_MAX);
		trace_mmqos__chn_bw(1, 1,
				RSH_16(readl(MEM_SMI_COMM1_CHN1_BW)),
				MASK_16(readl(MEM_SMI_COMM1_CHN1_BW)),
				0,
				0,
				0,
				TYPE_IS_VCP,
				TYPE_IS_MAX);
		trace_mmqos__larb_avg_bw(
			"r", "vdec", RSH_16(readl(MEM_SMI_VDEC_COMM0_CHN0_BW)), 0, 0);
		trace_mmqos__larb_avg_bw(
			"w", "vdec", MASK_16(readl(MEM_SMI_VDEC_COMM0_CHN0_BW)), 0, 0);
		trace_mmqos__larb_avg_bw(
			"r", "vdec", RSH_16(readl(MEM_SMI_VDEC_COMM0_CHN1_BW)), 0, 1);
		trace_mmqos__larb_avg_bw(
			"w", "vdec", MASK_16(readl(MEM_SMI_VDEC_COMM0_CHN1_BW)), 0, 1);
		trace_mmqos__larb_avg_bw(
			"r", "vdec", RSH_16(readl(MEM_SMI_VDEC_COMM1_CHN0_BW)), 1, 0);
		trace_mmqos__larb_avg_bw(
			"w", "vdec", MASK_16(readl(MEM_SMI_VDEC_COMM1_CHN0_BW)), 1, 0);
		trace_mmqos__larb_avg_bw(
			"r", "vdec", RSH_16(readl(MEM_SMI_VDEC_COMM1_CHN1_BW)), 1, 1);
		trace_mmqos__larb_avg_bw(
			"w", "vdec", MASK_16(readl(MEM_SMI_VDEC_COMM1_CHN1_BW)), 1, 1);
		trace_mmqos__larb_avg_bw(
			"r", "venc", RSH_16(readl(MEM_SMI_VENC_COMM0_CHN0_BW)), 0, 0);
		trace_mmqos__larb_avg_bw(
			"w", "venc", MASK_16(readl(MEM_SMI_VENC_COMM0_CHN0_BW)), 0, 0);
		trace_mmqos__larb_avg_bw(
			"r", "venc", RSH_16(readl(MEM_SMI_VENC_COMM0_CHN1_BW)), 0, 1);
		trace_mmqos__larb_avg_bw(
			"w", "venc", MASK_16(readl(MEM_SMI_VENC_COMM0_CHN1_BW)), 0, 1);
		trace_mmqos__larb_avg_bw(
			"r", "venc", RSH_16(readl(MEM_SMI_VENC_COMM1_CHN0_BW)), 1, 0);
		trace_mmqos__larb_avg_bw(
			"w", "venc", MASK_16(readl(MEM_SMI_VENC_COMM1_CHN0_BW)), 1, 0);
		trace_mmqos__larb_avg_bw(
			"r", "venc", RSH_16(readl(MEM_SMI_VENC_COMM1_CHN1_BW)), 1, 1);
		trace_mmqos__larb_avg_bw(
			"w", "venc", MASK_16(readl(MEM_SMI_VENC_COMM1_CHN1_BW)), 1, 1);
		msleep(20);
	}
	return 0;
}

static int mmqos_debug_set_ftrace(const char *val,
	const struct kernel_param *kp)
{
	static struct task_struct *kthr;
	u32 ena = 0;
	int ret;

	if (!gmmqos) {
		ftrace_ena = false;
		return 0;
	}

	mutex_lock(&gmmqos->bw_lock);
	ret = kstrtou32(val, 0, &ena);

	ftrace_ena = ena;
	if (mmqos_state & VCP_ENABLE) {
		if (ena) {
			kthr = kthread_run(
				mmqos_dbg_ftrace_thread, NULL, "mmqos-dbg-ftrace");
		} else {
			MMQOS_DBG("disable");
			if (kthr) {
				ret = kthread_stop(kthr);
				kthr = NULL;
				if (!ret)
					MMQOS_DBG("stop kthread mmqos-dbg-ftrace");
			}
		}
	}
	mutex_unlock(&gmmqos->bw_lock);
	return 0;
}

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmqos log level");

void set_mmqos_state(const u32 new_state)
{
	mmqos_state = new_state;
}
EXPORT_SYMBOL_GPL(set_mmqos_state);

int get_mmqos_state(void)
{
	return mmqos_state;
}
EXPORT_SYMBOL_GPL(get_mmqos_state);

static int mmqos_set_state(const char *val, const struct kernel_param *kp)
{
	u32 state = 0;
	int ret;

	ret = kstrtou32(val, 0, &state);
	if (ret) {
		MMQOS_ERR("failed:%d state:%#x", ret, state);
		return ret;
	}

	MMQOS_DBG("sync mmqos_state: %d -> %d", mmqos_state, state);
	mmqos_state = state;

	if (mmqos_state & VCP_ENABLE) {
		mtk_mmqos_enable_vcp(true);
		ret = mmqos_vcp_ipi_send(FUNC_SYNC_STATE, mmqos_state, NULL);
		mtk_mmqos_enable_vcp(false);
	}
	return ret;
}

static const struct kernel_param_ops mmqos_state_ops = {
	.set = mmqos_set_state,
	.get = param_get_uint,
};
module_param_cb(mmqos_state, &mmqos_state_ops, &mmqos_state, 0644);
MODULE_PARM_DESC(mmqos_state, "mmqos_state");

module_param(freq_mode, uint, 0644);
MODULE_PARM_DESC(freq_mode, "mminfra change frequency mode");

static const struct kernel_param_ops mmqos_debug_set_ftrace_ops = {
	.set = mmqos_debug_set_ftrace,
	.get = param_get_uint,
};
module_param_cb(ftrace_ena, &mmqos_debug_set_ftrace_ops, &ftrace_ena, 0644);
MODULE_PARM_DESC(ftrace_ena, "mmqos ftrace log");

EXPORT_SYMBOL_GPL(mtk_mmqos_remove);
MODULE_LICENSE("GPL v2");
#endif /* MMQOS_MTK_C */
