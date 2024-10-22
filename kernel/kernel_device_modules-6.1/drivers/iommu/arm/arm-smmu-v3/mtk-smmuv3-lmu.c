// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

/*
 * This driver adds support for Mediatek SMMU Latancy Monitor Unit.
 *
 * SMMU LMU named as smmuv3_lmu_<phys_addr_page> where
 * <phys_addr_page> is the physical page address of the SMMU LMU wrapped
 * to 4K boundary. For example, the LMU at 0xff88840000 is named
 * smmuv3_lmu_ff88840
 *
 * Filtering by AXI id is done by specifying filtering parameters
 * with the event. options are:
 *   filter_txu_id - the smmu txu id, to count specified txu transactions
 *   filter_axid   - the AxID to count the event
 *   filter_mask   - the AxID mask, to count a group of AxID match
 *		     (input ID & filter_mask) == (filter_axid & filter_mask)
 *   filter_lat_spec - to filter the transactions latency exceed the spec
 *
 * SMMU LMU events are not attributable to a CPU, so task mode and sampling
 * are not supported.
 */

#include <linux/acpi.h>
#include <linux/acpi_iort.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "arm-smmu-v3.h"
#include "mtk-smmu-v3.h"
#include "smmu_secure.h"

#define SMMU_LME_MAX_COUNTERS		16

#define EVENT_R_LAT_MAX			0
#define EVENT_R_LAT_MAX_AXID		1
#define EVENT_R_LAT_MAX_PEND_BY_EMI	2
#define EVENT_R_LAT_TOT			3
#define EVENT_R_TRANS_TOT		4
#define EVENT_R_LAT_AVG			5
#define EVENT_R_OOS_TRANS_TOT		6
#define EVENT_W_LAT_MAX			7
#define EVENT_W_LAT_MAX_AXID		8
#define EVENT_W_LAT_MAX_PEND_BY_EMI	9
#define EVENT_W_LAT_TOT			10
#define EVENT_W_TRNAS_TOT		11
#define EVENT_W_LAT_AVG			12
#define EVENT_W_OOS_TRANS_TOT		13
#define EVENT_TBUS_TRANS_TOT		14
#define EVENT_TBUS_LAT_AVG		15

#define FILTER_TXU_ID_TCU		0
#define FILTER_TXU_ID_TBU0		1
#define FILTER_TXU_ID_TBU1		2
#define FILTER_TXU_ID_TBU2		3
#define FILTER_TXU_ID_TBU3		4

static const char *PMU_SMMU_PROP_NAME = "mtk,smmu";
static int cpuhp_state_num;
/* 1 TCU + at most 4 TBU, each has SMMU_LME_MAX_COUNTERS counters */
static u64 lmu_stats[SMMU_TBU_CNT_MAX + 1][SMMU_LME_MAX_COUNTERS];
static u64 lmu_tbus_lat_avg;

struct smmu_lmu {
	struct hlist_node node;
	struct perf_event *events[SMMU_LME_MAX_COUNTERS];
	unsigned int on_cpu;
	struct pmu pmu;
	unsigned int num_counters;
	unsigned int used_counters;
	struct device *dev;
	struct arm_smmu_device *smmu;
	enum mtk_smmu_type smmu_type;
	raw_spinlock_t counter_lock;
	ktime_t last_read;
	bool take_power;
};

#define to_smmu_lmu(p) (container_of(p, struct smmu_lmu, pmu))

#define SMMU_LMU_EVENT_ATTR_EXTRACTOR(_name, _config, _start, _end)        \
	static inline u32 get_##_name(struct perf_event *event)            \
	{                                                                  \
		return FIELD_GET(GENMASK_ULL(_end, _start),                \
				 event->attr._config);                     \
	}                                                                  \

SMMU_LMU_EVENT_ATTR_EXTRACTOR(event, config, 0, 15);
SMMU_LMU_EVENT_ATTR_EXTRACTOR(filter_axid, config1, 0, 15);
SMMU_LMU_EVENT_ATTR_EXTRACTOR(filter_mask, config1, 16, 31);
SMMU_LMU_EVENT_ATTR_EXTRACTOR(filter_lat_spec, config1, 32, 47);
SMMU_LMU_EVENT_ATTR_EXTRACTOR(filter_txu_id, config1, 48, 51);

static inline const char *smmu_name(enum mtk_smmu_type type)
{
	switch (type) {
	case MM_SMMU:
		return "mm";
	case APU_SMMU:
		return "apu";
	case SOC_SMMU:
		return "socsys";
	case GPU_SMMU:
		return "gpu";
	default:
		return "unknown";
	}
}

static inline unsigned int smmu_read_reg(void __iomem *base,
					 unsigned int offset)
{
	return readl_relaxed(base + offset);
}

static inline void smmu_write_field(void __iomem *base,
				    unsigned int reg,
				    unsigned int mask,
				    unsigned int val)
{
	unsigned int regval;

	regval = readl_relaxed(base + reg);
	regval = (regval & (~mask))|val;
	writel_relaxed(regval, base + reg);
}

static inline void smmu_write_reg(void __iomem *base,
				  unsigned int offset,
				  unsigned int val)
{
	writel_relaxed(val, base + offset);
}

static u64 smmu_lmu_counter_get_value(struct smmu_lmu *smmu_lmu,
					     struct perf_event *event)
{
	unsigned int tcu_lat_max, tcu_pend_max, tcu_lat_tot;
	unsigned int tcu_trans_tot, tcu_oos_trans_tot, tcu_avg_lat;
	unsigned int tbu_rlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_id_rlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_id_wlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_rpend_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wpend_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_rlat_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wlat_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_rtrans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wtrans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_roos_trans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_woos_trans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_avg_rlat[SMMU_TBU_CNT_MAX];
	unsigned int tbu_avg_wlat[SMMU_TBU_CNT_MAX];
	unsigned int tbus_trans_tot = 0;
	unsigned int tbus_trans_tot_accu = 0;
	unsigned int tbus_lat_tot = 0;
	void __iomem *wp_base = smmu_lmu->smmu->wp_base;
	unsigned long irq_flags;
	unsigned int regval;
	u32 txu_id, event_id, tbu_cnt;
	ktime_t cur;
	int i;

	cur = ktime_get();
	txu_id = get_filter_txu_id(event);
	event_id = get_event(event);
	tbu_cnt = SMMU_TBU_CNT(smmu_lmu->smmu_type);

	if (txu_id > tbu_cnt || event_id >= SMMU_LME_MAX_COUNTERS)
		return 0;

	raw_spin_lock_irqsave(&smmu_lmu->counter_lock, irq_flags);
	/*
	 * skip continuous read in the following 1ms.
	 * Since all events share a global reset register
	 */
	if (ktime_ms_delta(cur, smmu_lmu->last_read) < 1) {
		raw_spin_unlock_irqrestore(&smmu_lmu->counter_lock, irq_flags);
		goto out_get_val;
	}

	/* reset counter, restart recording */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START, 1);

	/* tcu max read latency and max latency pended by emi */
	regval = smmu_read_reg(wp_base, SMMUWP_TCU_MON1);
	tcu_lat_max = FIELD_GET(TCU_LAT_MAX, regval);
	tcu_pend_max = FIELD_GET(TCU_PEND_MAX, regval);

	/* tcu sum of latency of total read cmds */
	tcu_lat_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON2);

	/* tcu total read cmd count */
	tcu_trans_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON3);

	/* tcu totoal cmd count whose latency exceed latency spec */
	tcu_oos_trans_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON4);

	/* tcu average read cmd latency */
	tcu_avg_lat = tcu_trans_tot > 0 ? (tcu_lat_tot / tcu_trans_tot) : 0;

	/* tbu monitor */
	for (i = 0; i < tbu_cnt; i++) {
		/* max latency */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON1(i));
		tbu_rlat_max[i] = FIELD_GET(MON1_RLAT_MAX, regval);
		tbu_wlat_max[i] = FIELD_GET(MON1_WLAT_MAX, regval);

		/* axid of max read latency */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON2(i));
		tbu_id_rlat_max[i] = FIELD_GET(MON2_ID_RLAT_MAX, regval);

		/* axid of max write latency */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON3(i));
		tbu_id_wlat_max[i] = FIELD_GET(MON3_ID_WLAT_MAX, regval);

		/* read or write max pended latency by emi */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON4(i));
		tbu_rpend_max[i] = FIELD_GET(MON4_RPEND_MAX, regval);
		tbu_wpend_max[i] = FIELD_GET(MON4_WPEND_MAX, regval);

		/* sum of latency of read total cmds */
		tbu_rlat_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON5(i));

		/* sum of latency of write total cmds */
		tbu_wlat_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON6(i));

		/* total read cmds count */
		tbu_rtrans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON7(i));

		/* total write cmds count */
		tbu_wtrans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON8(i));

		/* totoal read cmds whose latecny exceed latency spec */
		tbu_roos_trans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON9(i));

		/* totoal write cmds whose latecny exceed latency spec */
		tbu_woos_trans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON10(i));

		/* average read cmd latency */
		tbu_avg_rlat[i] = tbu_rtrans_tot[i] > 0 ?
				  (tbu_rlat_tot[i] / tbu_rtrans_tot[i]) : 0;

		/* average write cmd latency */
		tbu_avg_wlat[i] = tbu_wtrans_tot[i] > 0 ?
				  (tbu_wlat_tot[i] / tbu_wtrans_tot[i]) : 0;

		tbus_trans_tot += tbu_rtrans_tot[i];
		tbus_trans_tot += tbu_wtrans_tot[i];
		tbus_lat_tot += tbu_rlat_tot[i];
		tbus_lat_tot += tbu_wlat_tot[i];
	}

	/* update tcu counter */
	lmu_stats[0][EVENT_R_LAT_MAX] += tcu_lat_max;
	lmu_stats[0][EVENT_R_LAT_MAX_PEND_BY_EMI] += tcu_pend_max;
	lmu_stats[0][EVENT_R_LAT_TOT] += tcu_lat_tot;
	lmu_stats[0][EVENT_R_TRANS_TOT] += tcu_trans_tot;
	lmu_stats[0][EVENT_R_LAT_AVG] += tcu_avg_lat;
	lmu_stats[0][EVENT_R_OOS_TRANS_TOT] += tcu_oos_trans_tot;

	/* update all tbu counters */
	for (i = 1; i < tbu_cnt + 1; i++) {
		lmu_stats[i][EVENT_R_LAT_MAX] += tbu_rlat_max[i - 1];
		lmu_stats[i][EVENT_R_LAT_MAX_AXID] += tbu_id_rlat_max[i - 1];
		lmu_stats[i][EVENT_R_LAT_MAX_PEND_BY_EMI] += tbu_rpend_max[i - 1];
		lmu_stats[i][EVENT_R_LAT_TOT] += tbu_rlat_tot[i - 1];
		lmu_stats[i][EVENT_R_TRANS_TOT] += tbu_rtrans_tot[i - 1];
		lmu_stats[i][EVENT_R_LAT_AVG] += tbu_avg_rlat[i - 1];
		lmu_stats[i][EVENT_R_OOS_TRANS_TOT] += tbu_roos_trans_tot[i - 1];
		lmu_stats[i][EVENT_W_LAT_MAX] += tbu_wlat_max[i - 1];
		lmu_stats[i][EVENT_W_LAT_MAX_AXID] += tbu_id_wlat_max[i - 1];
		lmu_stats[i][EVENT_W_LAT_MAX_PEND_BY_EMI] += tbu_wpend_max[i - 1];
		lmu_stats[i][EVENT_W_LAT_TOT] += tbu_wlat_tot[i - 1];
		lmu_stats[i][EVENT_W_TRNAS_TOT] += tbu_wtrans_tot[i - 1];
		lmu_stats[i][EVENT_W_LAT_AVG] += tbu_avg_wlat[i - 1];
		lmu_stats[i][EVENT_W_OOS_TRANS_TOT] += tbu_woos_trans_tot[i - 1];
	}

	/* update tbus latency */
	lmu_tbus_lat_avg += tbus_trans_tot > 0 ? tbus_lat_tot / tbus_trans_tot : 0;
	smmu_lmu->last_read = cur;

	raw_spin_unlock_irqrestore(&smmu_lmu->counter_lock, irq_flags);

out_get_val:
	if (event_id == EVENT_TBUS_TRANS_TOT) {
		for (i = 1; i < tbu_cnt + 1; i++) {
			tbus_trans_tot_accu += lmu_stats[i][EVENT_R_TRANS_TOT];
			tbus_trans_tot_accu += lmu_stats[i][EVENT_W_TRNAS_TOT];
		}
		return tbus_trans_tot_accu;
	} else if (event_id == EVENT_TBUS_LAT_AVG) {
		return lmu_tbus_lat_avg;
	}

	return lmu_stats[txu_id][event_id];
}

static inline void smmu_lmu_counter_enable(struct smmu_lmu *smmu_lmu)
{
	smmu_write_field(smmu_lmu->smmu->wp_base, SMMUWP_LMU_CTL0,
			 CTL0_LAT_MON_START,
			 FIELD_PREP(CTL0_LAT_MON_START, 1));
}

static inline void smmu_lmu_counter_disable(struct smmu_lmu *smmu_lmu)
{
	smmu_write_field(smmu_lmu->smmu->wp_base, SMMUWP_LMU_CTL0,
			 CTL0_LAT_MON_START,
			 FIELD_PREP(CTL0_LAT_MON_START, 0));
}

static void smmu_lmu_event_update(struct perf_event *event)
{
	struct smmu_lmu *smmu_lmu = to_smmu_lmu(event->pmu);
	u64 now;

	now = smmu_lmu_counter_get_value(smmu_lmu, event);
	local64_set(&event->count, now);
}

static void smmu_lmu_apply_event_filter(struct smmu_lmu *smmu_lmu,
				       struct perf_event *event)
{
	void __iomem *wp_base = smmu_lmu->smmu->wp_base;
	u32 axid, mask, lat_spec;
	int i;

	axid = get_filter_axid(event);
	mask = get_filter_mask(event);
	lat_spec = get_filter_lat_spec(event);

	if (lat_spec == 0)
		lat_spec = LAT_SPEC_DEF_VAL;

	smmu_write_field(wp_base, SMMUWP_GLB_CTL4, CTL4_LAT_SPEC,
			 FIELD_PREP(CTL4_LAT_SPEC, lat_spec));

	/* always enable tcu monitor */
	smmu_write_field(wp_base, SMMUWP_TCU_CTL8, TCU_MON_ID,
			 FIELD_PREP(TCU_MON_ID, axid));
	smmu_write_field(wp_base, SMMUWP_TCU_CTL8, TCU_MON_ID_MASK,
			 FIELD_PREP(TCU_MON_ID_MASK, mask));

	/* always enable all tbu monitor */
	for (i = 0; i < SMMU_TBU_CNT(smmu_lmu->smmu_type); i++) {
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL4(i),
				 CTL4_RLAT_MON_ID,
				 FIELD_PREP(CTL4_RLAT_MON_ID, axid));
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL6(i),
				 CTL6_RLAT_MON_ID_MASK,
				 FIELD_PREP(CTL6_RLAT_MON_ID_MASK, mask));
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL5(i),
				 CTL5_WLAT_MON_ID,
				 FIELD_PREP(CTL5_WLAT_MON_ID, axid));
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL7(i),
				 CTL7_WLAT_MON_ID_MASK,
				 FIELD_PREP(CTL7_WLAT_MON_ID_MASK, mask));
	}
}

static void smmu_lmu_reset_event_filter(struct smmu_lmu *smmu_lmu)
{
	void __iomem *wp_base = smmu_lmu->smmu->wp_base;
	int i;

	smmu_write_field(wp_base, SMMUWP_GLB_CTL4, CTL4_LAT_SPEC,
			 FIELD_PREP(CTL4_LAT_SPEC, 0));

	/* disable tcu monitor and clear axid/mask filter */
	smmu_write_reg(wp_base, SMMUWP_TCU_CTL8, 0);

	/* disable all tbu monitor and clear axid/mask filter */
	for (i = 0; i < SMMU_TBU_CNT(smmu_lmu->smmu_type); i++) {
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL4(i), 0);
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL5(i), 0);
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL6(i), 0);
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL7(i), 0);
	}
}

static void smmu_lmu_reset_counters(struct smmu_lmu *smmu_lmu)
{
	unsigned int i = 0;

	/* update tcu counter */
	lmu_stats[0][EVENT_R_LAT_MAX] = 0;
	lmu_stats[0][EVENT_R_LAT_MAX_PEND_BY_EMI] = 0;
	lmu_stats[0][EVENT_R_LAT_TOT] = 0;
	lmu_stats[0][EVENT_R_TRANS_TOT] = 0;
	lmu_stats[0][EVENT_R_LAT_AVG] = 0;
	lmu_stats[0][EVENT_R_OOS_TRANS_TOT] = 0;

	/* update all tbu counters */
	for (i = 1; i < SMMU_TBU_CNT(smmu_lmu->smmu_type) + 1; i++) {
		lmu_stats[i][EVENT_R_LAT_MAX] = 0;
		lmu_stats[i][EVENT_R_LAT_MAX_AXID] = 0;
		lmu_stats[i][EVENT_R_LAT_MAX_PEND_BY_EMI] = 0;
		lmu_stats[i][EVENT_R_LAT_TOT] = 0;
		lmu_stats[i][EVENT_R_TRANS_TOT] = 0;
		lmu_stats[i][EVENT_R_LAT_AVG] = 0;
		lmu_stats[i][EVENT_R_OOS_TRANS_TOT] = 0;
		lmu_stats[i][EVENT_W_LAT_MAX] = 0;
		lmu_stats[i][EVENT_W_LAT_MAX_AXID] = 0;
		lmu_stats[i][EVENT_W_LAT_MAX_PEND_BY_EMI] = 0;
		lmu_stats[i][EVENT_W_LAT_TOT] = 0;
		lmu_stats[i][EVENT_W_TRNAS_TOT] = 0;
		lmu_stats[i][EVENT_W_LAT_AVG] = 0;
		lmu_stats[i][EVENT_W_OOS_TRANS_TOT] = 0;
	}
	lmu_tbus_lat_avg = 0;

}

static bool smmu_lmu_events_compatible(struct perf_event *curr,
				       struct perf_event *new)
{
	if (new->pmu != curr->pmu)
		return false;

	return true;
}

/*
 * Implementation of abstract pmu functionality required by
 * the core perf events code.
 */
static int smmu_lmu_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_lmu *smmu_lmu = to_smmu_lmu(event->pmu);
	struct device *dev = smmu_lmu->dev;
	struct perf_event *sibling;
	int group_num_events = 1;
	u16 event_id;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	if (hwc->sample_period) {
		dev_info(dev, "Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->cpu < 0) {
		dev_info(dev, "Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	/* Verify specified event is supported on this PMU */
	event_id = get_event(event);
	if (event_id >= SMMU_LME_MAX_COUNTERS) {
		dev_info(dev, "Invalid event %d for this PMU\n", event_id);
		return -EINVAL;
	}

	/* Don't allow groups with mixed PMUs, except for s/w events */
	if (!is_software_event(event->group_leader)) {
		if (!smmu_lmu_events_compatible(event->group_leader, event))
			return -EINVAL;

		if (++group_num_events > smmu_lmu->num_counters)
			return -EINVAL;
	}

	for_each_sibling_event(sibling, event->group_leader) {
		if (is_software_event(sibling))
			continue;

		if (!smmu_lmu_events_compatible(sibling, event))
			return -EINVAL;

		if (++group_num_events > smmu_lmu->num_counters)
			return -EINVAL;
	}

	hwc->idx = -1;

	/*
	 * Ensure all events are on the same cpu so all events are in the
	 * same cpu context, to avoid races on pmu_enable etc.
	 */
	event->cpu = smmu_lmu->on_cpu;

	return 0;
}

static void smmu_lmu_event_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	hwc->state = 0;
}

static void smmu_lmu_event_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	smmu_lmu_event_update(event);
	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int smmu_lmu_get_event_idx(struct smmu_lmu *smmu_lmu,
				  struct perf_event *event)
{
	int i, idx;

	idx = -1;

	for (i = 0; i < SMMU_LME_MAX_COUNTERS; i++) {
		if (i == get_event(event)) {
			idx = smmu_lmu->events[i] == NULL ? i : -ENOENT;
			break;
		}
	}
	return idx;
}

static int smmu_lmu_event_add(struct perf_event *event, int flags)
{
	struct smmu_lmu *smmu_lmu = to_smmu_lmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	unsigned long irq_flags;
	int idx;
	int err;

	raw_spin_lock_irqsave(&smmu_lmu->counter_lock, irq_flags);
	idx = smmu_lmu_get_event_idx(smmu_lmu, event);
	if (idx < 0) {
		raw_spin_unlock_irqrestore(&smmu_lmu->counter_lock, irq_flags);
		pr_info("%s called with error, event id:%d, idx:%d\n",
			__func__, get_event(event), idx);
		return -EAGAIN;
	}

	if (smmu_lmu->used_counters == 0) {
		err = smmu_lmu->smmu_type == SOC_SMMU ? 0 :
		      mtk_smmu_pm_get(smmu_lmu->smmu_type);
		if (err) {
			raw_spin_unlock_irqrestore(&smmu_lmu->counter_lock,
						   irq_flags);
			pr_info("%s, pm get fail, smmu:%d.\n",
				__func__, smmu_lmu->smmu_type);
			return -EPERM;
		}
		smmu_lmu->take_power = true;
	}

	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	smmu_lmu->events[idx] = event;
	local64_set(&hwc->prev_count, 0);
	if (smmu_lmu->used_counters == 0) {
		/* apply filter only for the fisrt event */
		smmu_lmu_apply_event_filter(smmu_lmu, event);
		smmu_lmu_counter_enable(smmu_lmu);
	}
	smmu_lmu->used_counters++;
	raw_spin_unlock_irqrestore(&smmu_lmu->counter_lock, irq_flags);

	if (flags & PERF_EF_START)
		smmu_lmu_event_start(event, flags);

	/* Propagate changes to the userspace mapping. */
	perf_event_update_userpage(event);

	return 0;
}

static void smmu_lmu_event_del(struct perf_event *event, int flags)
{
	struct smmu_lmu *smmu_lmu = to_smmu_lmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	unsigned long irq_flags;
	int idx = hwc->idx;
	int err = 0;

	if (idx < 0 || idx >= SMMU_LME_MAX_COUNTERS) {
		dev_dbg(smmu_lmu->dev, "ignore non-lmu event while delete\n");
		return;
	}

	smmu_lmu_event_stop(event, flags | PERF_EF_UPDATE);

	raw_spin_lock_irqsave(&smmu_lmu->counter_lock, irq_flags);
	smmu_lmu->events[idx] = NULL;
	smmu_lmu->used_counters--;
	if (smmu_lmu->used_counters == 0) {
		smmu_lmu_counter_disable(smmu_lmu);
		smmu_lmu_reset_event_filter(smmu_lmu);
		smmu_lmu_reset_counters(smmu_lmu);

		if (smmu_lmu->smmu_type != SOC_SMMU && smmu_lmu->take_power) {
			err = mtk_smmu_pm_put(smmu_lmu->smmu_type);
			smmu_lmu->take_power = false;
			if (err)
				pr_info("%s, pm put fail, smmu:%d, err:%d\n",
					__func__, smmu_lmu->smmu_type, err);
		}
	}
	raw_spin_unlock_irqrestore(&smmu_lmu->counter_lock, irq_flags);

	perf_event_update_userpage(event);
}

static void smmu_lmu_event_read(struct perf_event *event)
{
	smmu_lmu_event_update(event);
}

/* cpumask */

static ssize_t smmu_lmu_cpumask_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct smmu_lmu *smmu_lmu = to_smmu_lmu(dev_get_drvdata(dev));

	return cpumap_print_to_pagebuf(true, buf,
				       cpumask_of(smmu_lmu->on_cpu));
}

static struct device_attribute smmu_lmu_cpumask_attr =
		__ATTR(cpumask, 0444, smmu_lmu_cpumask_show, NULL);

static struct attribute *smmu_lmu_cpumask_attrs[] = {
	&smmu_lmu_cpumask_attr.attr,
	NULL
};

static const struct attribute_group smmu_lmu_cpumask_group = {
	.attrs = smmu_lmu_cpumask_attrs,
};

/* Events */

static ssize_t smmu_lmu_event_show(struct device *dev,
				   struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);

	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

#define SMMU_EVENT_ATTR(name, config)			\
	PMU_EVENT_ATTR_ID(name, smmu_lmu_event_show, config)

static struct attribute *smmu_lmu_events[] = {
	SMMU_EVENT_ATTR(r_lat_max, 0),
	SMMU_EVENT_ATTR(r_lat_max_axid, 1),
	SMMU_EVENT_ATTR(r_lat_max_pend_by_emi, 2),
	SMMU_EVENT_ATTR(r_lat_tot, 3),
	SMMU_EVENT_ATTR(r_trans_tot, 4),
	SMMU_EVENT_ATTR(r_lat_avg, 5),
	SMMU_EVENT_ATTR(r_oos_trans_tot, 6),
	SMMU_EVENT_ATTR(w_lat_max, 7),
	SMMU_EVENT_ATTR(w_lat_max_axid, 8),
	SMMU_EVENT_ATTR(w_lat_max_pend_by_emi, 9),
	SMMU_EVENT_ATTR(w_lat_tot, 10),
	SMMU_EVENT_ATTR(w_trans_tot, 11),
	SMMU_EVENT_ATTR(w_lat_avg, 12),
	SMMU_EVENT_ATTR(w_oos_trans_tot, 13),
	SMMU_EVENT_ATTR(tbus_trans_tot, 14),
	SMMU_EVENT_ATTR(tbus_lat_avg, 15),
	NULL
};

static umode_t smmu_lmu_event_is_visible(struct kobject *kobj,
					 struct attribute *attr, int unused)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr.attr);

	if (pmu_attr->id >= SMMU_LME_MAX_COUNTERS)
		return 0;

	return attr->mode;
}

static const struct attribute_group smmu_lmu_events_group = {
	.name = "events",
	.attrs = smmu_lmu_events,
	.is_visible = smmu_lmu_event_is_visible,
};

/* Formats */
PMU_FORMAT_ATTR(event,			"config:0-15");
PMU_FORMAT_ATTR(filter_axid,		"config1:0-15");
PMU_FORMAT_ATTR(filter_mask,		"config1:16-31");
PMU_FORMAT_ATTR(filter_lat_spec,	"config1:32-47");
PMU_FORMAT_ATTR(filter_txu_id,		"config1:48-51");

static struct attribute *smmu_lmu_formats[] = {
	&format_attr_event.attr,
	&format_attr_filter_axid.attr,
	&format_attr_filter_mask.attr,
	&format_attr_filter_lat_spec.attr,
	&format_attr_filter_txu_id.attr,
	NULL
};

static const struct attribute_group smmu_lmu_format_group = {
	.name = "format",
	.attrs = smmu_lmu_formats,
};

static const struct attribute_group *smmu_lmu_attr_grps[] = {
	&smmu_lmu_cpumask_group,
	&smmu_lmu_events_group,
	&smmu_lmu_format_group,
	NULL
};

/*
 * Generic device handlers
 */

static int smmu_lmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct smmu_lmu *smmu_lmu;
	unsigned int target;

	smmu_lmu = hlist_entry_safe(node, struct smmu_lmu, node);
	if (!smmu_lmu || cpu != smmu_lmu->on_cpu)
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	perf_pmu_migrate_context(&smmu_lmu->pmu, cpu, target);
	smmu_lmu->on_cpu = target;

	return 0;
}

static int smmu_lmu_probe(struct platform_device *pdev)
{
	const struct mtk_smmu_plat_data *plat_data;
	struct device *dev = &pdev->dev;
	struct platform_device *smmudev;
	struct smmu_lmu *smmu_lmu;
	struct arm_smmu_device *smmu;
	struct device_node *smmu_node;
	struct mtk_smmu_data *data;
	char *name;
	int err;

	smmu_lmu = devm_kzalloc(dev, sizeof(*smmu_lmu), GFP_KERNEL);
	if (!smmu_lmu)
		return -ENOMEM;

	smmu_lmu->dev = dev;
	platform_set_drvdata(pdev, smmu_lmu);

	smmu_lmu->pmu = (struct pmu) {
		.module		= THIS_MODULE,
		.task_ctx_nr    = perf_invalid_context,
		.event_init	= smmu_lmu_event_init,
		.add		= smmu_lmu_event_add,
		.del		= smmu_lmu_event_del,
		.start		= smmu_lmu_event_start,
		.stop		= smmu_lmu_event_stop,
		.read		= smmu_lmu_event_read,
		.attr_groups	= smmu_lmu_attr_grps,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE,
	};

	smmu_node = of_parse_phandle(dev->of_node, PMU_SMMU_PROP_NAME, 0);
	if (!smmu_node) {
		dev_err(dev, "Can't find smmu node\n");
		return -EINVAL;
	}

	smmudev = of_find_device_by_node(smmu_node);
	if (!smmudev) {
		dev_err(dev, "Can't find smmu dev\n");
		of_node_put(smmu_node);
		return -EINVAL;
	}

	smmu = platform_get_drvdata(smmudev);
	data = to_mtk_smmu_data(smmu);
	plat_data = data->plat_data;
	smmu_lmu->smmu = smmu;
	smmu_lmu->smmu_type = plat_data->smmu_type;
	smmu_lmu->take_power = false;

	/* assmue only support 16 counter */
	smmu_lmu->num_counters = 14;

	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "smmuv3_lmu_%s",
			      smmu_name(plat_data->smmu_type));
	if (!name) {
		dev_err(dev, "Create name failed\n");
		return -EINVAL;
	}

	/* Pick one CPU to be the preferred one to use */
	smmu_lmu->on_cpu = raw_smp_processor_id();
	err = cpuhp_state_add_instance_nocalls(cpuhp_state_num,
					       &smmu_lmu->node);
	if (err) {
		dev_err(dev, "Error %d registering hotplug fail\n", err);
		return err;
	}

	raw_spin_lock_init(&smmu_lmu->counter_lock);
	err = perf_pmu_register(&smmu_lmu->pmu, name, -1);
	if (err) {
		dev_err(dev, "Error %d registering PMU\n", err);
		goto out_unregister;
	}

	return 0;

out_unregister:
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &smmu_lmu->node);
	return err;
}

static int smmu_lmu_remove(struct platform_device *pdev)
{
	struct smmu_lmu *smmu_lmu = platform_get_drvdata(pdev);

	perf_pmu_unregister(&smmu_lmu->pmu);
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &smmu_lmu->node);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id smmu_lmu_of_match[] = {
	{ .compatible = "mtk,smmu-v3-lmu" },
	{}
};
MODULE_DEVICE_TABLE(of, smmu_lmu_of_match);
#endif

static struct platform_driver smmu_lmu_driver = {
	.driver = {
		.name = "mtk-smmu-v3-lmu",
		.of_match_table = of_match_ptr(smmu_lmu_of_match),
		.suppress_bind_attrs = true,
	},
	.probe = smmu_lmu_probe,
	.remove = smmu_lmu_remove,
};

static int __init mtk_smmu_lmu_init(void)
{
	cpuhp_state_num = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
						  "perf/mtk/lmu:online",
						  NULL,
						  smmu_lmu_offline_cpu);
	if (cpuhp_state_num < 0)
		return cpuhp_state_num;

	return platform_driver_register(&smmu_lmu_driver);
}
module_init(mtk_smmu_lmu_init);

static void __exit mtk_smmu_lmu_exit(void)
{
	platform_driver_unregister(&smmu_lmu_driver);
	cpuhp_remove_multi_state(cpuhp_state_num);
}

module_exit(mtk_smmu_lmu_exit);

MODULE_DESCRIPTION("Latency Meter Unit driver for MTK SMMUv3");
MODULE_LICENSE("GPL");
