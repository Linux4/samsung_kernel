// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

/*
 * This driver adds support for perf events to use MPAM resource
 * monitor associated with an SMMUv3 node to monitor that node.
 *
 * Currently only Cache Storage Usage is supported to monitor.
 *
 * Filtering by partid is done by specifying filtering parameters
 * with the event. options are:
 *   filter_partid     - The partition id to count for
 *   filter_pmg        - The performance monitoring group to count for
 *   filter_ris        - The resource instance selector to count for
 *   filter_enable_pmg - Whether measures only storage with PMG matching
 *
 * SMMU MPAM resource monitor events are not attributable to a CPU,
 * so task mode and sampling are not supported.
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

#define SMMU_MPAM_MAX_COUNTERS		4

#define SMMU_MPAM_MAX_EVENT		1

#define FILTER_TXU_ID_TCU		0
#define FILTER_TXU_ID_TBU0		1
#define FILTER_TXU_ID_TBU1		2
#define FILTER_TXU_ID_TBU2		3
#define FILTER_TXU_ID_TBU3		4

static const char *PMU_SMMU_PROP_NAME = "mtk,smmu";
static int cpuhp_state_num;

struct smmu_mpam_mon {
	struct hlist_node node;
	struct perf_event *events[SMMU_MPAM_MAX_COUNTERS];
	DECLARE_BITMAP(used_counters, SMMU_MPAM_MAX_COUNTERS);
	unsigned int on_cpu;
	struct pmu pmu;
	unsigned int num_counters;
	struct device *dev;
	struct arm_smmu_device *smmu;
	enum mtk_smmu_type smmu_type;
	raw_spinlock_t counter_lock;
	char *name;
	u32 txu_id;
	bool take_power;
};

struct smmu_mpam_mon_list {
	struct smmu_mpam_mon *mpam_mons;
	u32 mpam_mon_cnt;
};

#define to_smmu_mpam_mon(p) (container_of(p, struct smmu_mpam_mon, pmu))

#define SMMU_MPAM_EVENT_ATTR_EXTRACTOR(_name, _config, _start, _end)       \
	static inline u32 get_##_name(struct perf_event *event)            \
	{                                                                  \
		return FIELD_GET(GENMASK_ULL(_end, _start),                \
				 event->attr._config);                     \
	}                                                                  \

SMMU_MPAM_EVENT_ATTR_EXTRACTOR(event, config, 0, 0);
SMMU_MPAM_EVENT_ATTR_EXTRACTOR(filter_partid, config1, 0, 15);
SMMU_MPAM_EVENT_ATTR_EXTRACTOR(filter_pmg, config1, 16, 31);
SMMU_MPAM_EVENT_ATTR_EXTRACTOR(filter_ris, config1, 32, 47);
SMMU_MPAM_EVENT_ATTR_EXTRACTOR(filter_enable_pmg, config1, 48, 48);

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

static inline u64 smmu_mpam_counter_get_value(struct smmu_mpam_mon *mpam_mon,
					      struct perf_event *event)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(mpam_mon->smmu);
	struct hw_perf_event *hwc = &event->hw;
	void __iomem *mpam_base;
	unsigned long irq_flags;
	int idx = hwc->idx;
	u32 txu_id, ris, reg, now;
	bool nrdy = false;

	txu_id = mpam_mon->txu_id;
	mpam_base = data->txu_mpam_data[txu_id].mpam_base;

	raw_spin_lock_irqsave(&mpam_mon->counter_lock, irq_flags);
	ris = get_filter_ris(event);
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_MON_SEL,
			 FIELD_PREP(CFG_MON_SEL_MON_SEL, idx));
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_RIS,
			 FIELD_PREP(CFG_MON_SEL_RIS, ris));
	reg = smmu_read_reg(mpam_base, MSMON_CSU);
	nrdy = reg & MSMON_NRDY;
	now = reg & MSMON_CSU_VALUE;
	raw_spin_unlock_irqrestore(&mpam_mon->counter_lock, irq_flags);

	return nrdy ? 0 : now;
}

static inline void smmu_mpam_counter_enable(struct smmu_mpam_mon *mpam_mon,
					    struct perf_event *event)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(mpam_mon->smmu);
	struct hw_perf_event *hwc = &event->hw;
	void __iomem *mpam_base;
	unsigned long irq_flags;
	int idx = hwc->idx;
	u32 txu_id, ris;

	txu_id = mpam_mon->txu_id;
	mpam_base = data->txu_mpam_data[txu_id].mpam_base;

	raw_spin_lock_irqsave(&mpam_mon->counter_lock, irq_flags);
	ris = get_filter_ris(event);
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_MON_SEL,
			 FIELD_PREP(CFG_MON_SEL_MON_SEL, idx));
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_RIS,
			 FIELD_PREP(CFG_MON_SEL_RIS, ris));

	smmu_write_reg(mpam_base,
		       MSMON_CSU,
		       0);

	smmu_write_field(mpam_base,
			 MSMON_CFG_CSU_CTL,
			 CSU_CTL_EN,
			 FIELD_PREP(CSU_CTL_EN, 1));
	raw_spin_unlock_irqrestore(&mpam_mon->counter_lock, irq_flags);
}

static inline void smmu_mpam_counter_disable(struct smmu_mpam_mon *mpam_mon,
					     struct perf_event *event)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(mpam_mon->smmu);
	struct hw_perf_event *hwc = &event->hw;
	void __iomem *mpam_base;
	unsigned long irq_flags;
	int idx = hwc->idx;
	u32 txu_id, ris;

	txu_id = mpam_mon->txu_id;
	mpam_base = data->txu_mpam_data[txu_id].mpam_base;

	raw_spin_lock_irqsave(&mpam_mon->counter_lock, irq_flags);
	ris = get_filter_ris(event);
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_MON_SEL,
			 FIELD_PREP(CFG_MON_SEL_MON_SEL, idx));
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_RIS,
			 FIELD_PREP(CFG_MON_SEL_RIS, ris));
	smmu_write_field(mpam_base,
			 MSMON_CFG_CSU_CTL,
			 CSU_CTL_EN,
			 FIELD_PREP(CSU_CTL_EN, 0));
	raw_spin_unlock_irqrestore(&mpam_mon->counter_lock, irq_flags);
}

static void smmu_mpam_event_update(struct perf_event *event)
{
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(event->pmu);
	u64 now;

	now = smmu_mpam_counter_get_value(mpam_mon, event);
	local64_add(now, &event->count);
}

static int smmu_mpam_apply_event_filter(struct smmu_mpam_mon *mpam_mon,
					struct perf_event *event, int idx)
{
	u32 partid, pmg, ris, txu_id;
	struct mtk_smmu_data *data = to_mtk_smmu_data(mpam_mon->smmu);
	struct txu_mpam_data *txu_mpam_data;
	void __iomem *mpam_base;
	unsigned long irq_flags;
	u32 enable_pmg;

	txu_id = mpam_mon->txu_id;
	partid = get_filter_partid(event);
	pmg = get_filter_pmg(event);
	ris = get_filter_ris(event);
	enable_pmg = get_filter_enable_pmg(event) ? 1 : 0;

	if (txu_id >= data->txu_mpam_data_cnt)
		return -EAGAIN;

	txu_mpam_data = &data->txu_mpam_data[txu_id];
	mpam_base = data->txu_mpam_data[txu_id].mpam_base;

	if (partid > txu_mpam_data->partid_max)
		return -EAGAIN;

	if (pmg > txu_mpam_data->pmg_max)
		return -EAGAIN;

	if (ris > txu_mpam_data->ris_max)
		return -EAGAIN;

	raw_spin_lock_irqsave(&mpam_mon->counter_lock, irq_flags);
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_MON_SEL,
			 FIELD_PREP(CFG_MON_SEL_MON_SEL, idx));
	smmu_write_field(mpam_base,
			 MSMON_CFG_MON_SEL,
			 CFG_MON_SEL_RIS,
			 FIELD_PREP(CFG_MON_SEL_RIS, ris));
	smmu_write_field(mpam_base,
			 MSMON_CFG_CSU_FLT,
			 CSU_FLT_PARTID,
			 FIELD_PREP(CSU_FLT_PARTID, partid));
	smmu_write_field(mpam_base,
			 MSMON_CFG_CSU_FLT,
			 CSU_FLT_PMG,
			 FIELD_PREP(CSU_FLT_PMG, pmg));
	smmu_write_field(mpam_base,
			 MSMON_CFG_CSU_CTL,
			 CSU_CTL_MATCH_PARTID,
			 FIELD_PREP(CSU_CTL_MATCH_PARTID, 1));
	smmu_write_field(mpam_base,
			 MSMON_CFG_CSU_CTL,
			 CSU_CTL_MATCH_PMG,
			 FIELD_PREP(CSU_CTL_MATCH_PMG, enable_pmg));
	raw_spin_unlock_irqrestore(&mpam_mon->counter_lock, irq_flags);

	return 0;
}

static bool smmu_mpam_events_compatible(struct perf_event *curr,
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
static int smmu_mpam_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(event->pmu);
	struct device *dev = mpam_mon->dev;
	struct perf_event *sibling;
	int group_num_events = 1;
	u16 event_id;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	if (hwc->sample_period) {
		dev_dbg(dev, "Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->cpu < 0) {
		dev_dbg(dev, "Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	/* Verify specified event is supported on this PMU */
	event_id = get_event(event);
	if (event_id >= SMMU_MPAM_MAX_COUNTERS) {
		dev_dbg(dev, "Invalid event %d for this PMU\n", event_id);
		return -EINVAL;
	}

	/* Don't allow groups with mixed PMUs, except for s/w events */
	if (!is_software_event(event->group_leader)) {
		if (!smmu_mpam_events_compatible(event->group_leader, event))
			return -EINVAL;

		if (++group_num_events > mpam_mon->num_counters)
			return -EINVAL;
	}

	for_each_sibling_event(sibling, event->group_leader) {
		if (is_software_event(sibling))
			continue;

		if (!smmu_mpam_events_compatible(sibling, event))
			return -EINVAL;

		if (++group_num_events > mpam_mon->num_counters)
			return -EINVAL;
	}

	hwc->idx = -1;

	/*
	 * Ensure all events are on the same cpu so all events are in the
	 * same cpu context, to avoid races on pmu_enable etc.
	 */
	event->cpu = mpam_mon->on_cpu;

	return 0;
}

static void smmu_mpam_event_start(struct perf_event *event, int flags)
{
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	hwc->state = 0;

	smmu_mpam_counter_enable(mpam_mon, event);
}

static void smmu_mpam_event_stop(struct perf_event *event, int flags)
{
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	smmu_mpam_counter_disable(mpam_mon, event);
	smmu_mpam_event_update(event);
	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int smmu_mpam_get_event_idx(struct smmu_mpam_mon *smmu_mpam_mon,
				   struct perf_event *event)
{
	int idx, err;
	unsigned int num_ctrs = smmu_mpam_mon->num_counters;
	unsigned long flags;

	raw_spin_lock_irqsave(&smmu_mpam_mon->counter_lock, flags);
	idx = find_first_zero_bit(smmu_mpam_mon->used_counters, num_ctrs);
	if (idx == num_ctrs) {
		raw_spin_unlock_irqrestore(&smmu_mpam_mon->counter_lock,
					   flags);
		/* The counters are all in use. */
		return -EAGAIN;
	}

	if (idx == 0) {
		err = smmu_mpam_mon->smmu_type == SOC_SMMU ? 0 :
		      mtk_smmu_pm_get(smmu_mpam_mon->smmu_type);
		if (err) {
			raw_spin_unlock_irqrestore(&smmu_mpam_mon->counter_lock,
						   flags);
			pr_info("%s, pm get fail, smmu:%d.\n",
				__func__, smmu_mpam_mon->smmu_type);
			return -EPERM;
		}
		smmu_mpam_mon->take_power = true;
		pr_info("%s, pm get ok, smmu:%d.\n",
			__func__, smmu_mpam_mon->smmu_type);
	}
	raw_spin_unlock_irqrestore(&smmu_mpam_mon->counter_lock, flags);

	err = smmu_mpam_apply_event_filter(smmu_mpam_mon, event, idx);
	if (err)
		return err;

	set_bit(idx, smmu_mpam_mon->used_counters);

	return idx;

}

static int smmu_mpam_event_add(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(event->pmu);

	idx = smmu_mpam_get_event_idx(mpam_mon, event);
	if (idx < 0)
		return idx;

	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	mpam_mon->events[idx] = event;
	local64_set(&hwc->prev_count, 0);

	if (flags & PERF_EF_START)
		smmu_mpam_event_start(event, flags);

	/* Propagate changes to the userspace mapping. */
	perf_event_update_userpage(event);

	return 0;
}

static void smmu_mpam_event_del(struct perf_event *event, int flags)
{
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	unsigned long irq_flags;
	unsigned int cur_idx, num_ctrs = mpam_mon->num_counters;
	int err;

	if (idx < 0 || idx >= SMMU_MPAM_MAX_COUNTERS) {
		dev_dbg(mpam_mon->dev, "ignore non-mpam event while delete\n");
		return;
	}

	smmu_mpam_event_stop(event, flags | PERF_EF_UPDATE);

	mpam_mon->events[idx] = NULL;

	raw_spin_lock_irqsave(&mpam_mon->counter_lock, irq_flags);
	clear_bit(idx, mpam_mon->used_counters);

	cur_idx = find_first_bit(mpam_mon->used_counters, num_ctrs);
	if (cur_idx == num_ctrs && mpam_mon->smmu_type != SOC_SMMU &&
	    mpam_mon->take_power) {
		err = mtk_smmu_pm_put(mpam_mon->smmu_type);
		if (err)
			pr_info("%s, pm put fail, smmu:%d, err:%d\n",
				__func__, mpam_mon->smmu_type, err);
		else
			pr_info("%s, pm put ok, smmu:%d\n",
				__func__, mpam_mon->smmu_type);
		mpam_mon->take_power = false;
	}
	raw_spin_unlock_irqrestore(&mpam_mon->counter_lock, irq_flags);

	perf_event_update_userpage(event);
}

static void smmu_mpam_event_read(struct perf_event *event)
{
	smmu_mpam_event_update(event);
}

/* cpumask */

static ssize_t smmu_mpam_cpumask_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct smmu_mpam_mon *mpam_mon = to_smmu_mpam_mon(dev_get_drvdata(dev));

	return cpumap_print_to_pagebuf(true, buf,
				       cpumask_of(mpam_mon->on_cpu));
}

static struct device_attribute smmu_mpam_cpumask_attr =
		__ATTR(cpumask, 0444, smmu_mpam_cpumask_show, NULL);

static struct attribute *smmu_mpam_cpumask_attrs[] = {
	&smmu_mpam_cpumask_attr.attr,
	NULL
};

static const struct attribute_group smmu_mpam_cpumask_group = {
	.attrs = smmu_mpam_cpumask_attrs,
};

/* Events */

static ssize_t smmu_mpam_event_show(struct device *dev,
				    struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);

	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

#define SMMU_EVENT_ATTR(name, config)			\
	PMU_EVENT_ATTR_ID(name, smmu_mpam_event_show, config)

static struct attribute *smmu_mpam_events[] = {
	SMMU_EVENT_ATTR(cmax, 0),
	NULL
};

static umode_t smmu_mpam_event_is_visible(struct kobject *kobj,
					  struct attribute *attr, int unused)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr.attr);

	if (pmu_attr->id >= SMMU_MPAM_MAX_EVENT)
		return 0;

	return attr->mode;
}

static const struct attribute_group smmu_mpam_events_group = {
	.name = "events",
	.attrs = smmu_mpam_events,
	.is_visible = smmu_mpam_event_is_visible,
};

/* Formats */
PMU_FORMAT_ATTR(event,			"config:0-15");
PMU_FORMAT_ATTR(filter_partid,		"config1:0-15");
PMU_FORMAT_ATTR(filter_pmg,		"config1:16-31");
PMU_FORMAT_ATTR(filter_ris,		"config1:32-47");
PMU_FORMAT_ATTR(filter_enable_pmg,	"config1:48-48");

static struct attribute *smmu_mpam_formats[] = {
	&format_attr_event.attr,
	&format_attr_filter_partid.attr,
	&format_attr_filter_pmg.attr,
	&format_attr_filter_ris.attr,
	&format_attr_filter_enable_pmg.attr,
	NULL
};

static const struct attribute_group smmu_mpam_format_group = {
	.name = "format",
	.attrs = smmu_mpam_formats,
};

static const struct attribute_group *smmu_mpam_attr_grps[] = {
	&smmu_mpam_cpumask_group,
	&smmu_mpam_events_group,
	&smmu_mpam_format_group,
	NULL
};

/*
 * Generic device handlers
 */

static int smmu_mpam_mon_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct smmu_mpam_mon *mpam_mon;
	unsigned int target;

	mpam_mon = hlist_entry_safe(node, struct smmu_mpam_mon, node);
	if (!mpam_mon || cpu != mpam_mon->on_cpu)
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	perf_pmu_migrate_context(&mpam_mon->pmu, cpu, target);
	mpam_mon->on_cpu = target;

	return 0;
}

static int smmu_mpam_mon_init(struct smmu_mpam_mon *mpam_mon)
{
	int smmu_type;
	int err;

	mpam_mon->pmu = (struct pmu) {
		.module		= THIS_MODULE,
		.task_ctx_nr    = perf_invalid_context,
		.event_init	= smmu_mpam_event_init,
		.add		= smmu_mpam_event_add,
		.del		= smmu_mpam_event_del,
		.start		= smmu_mpam_event_start,
		.stop		= smmu_mpam_event_stop,
		.read		= smmu_mpam_event_read,
		.attr_groups	= smmu_mpam_attr_grps,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE,
	};
	smmu_type = mpam_mon->smmu_type;

	if (mpam_mon->txu_id == 0)
		mpam_mon->name = devm_kasprintf(mpam_mon->dev, GFP_KERNEL,
						"smmuv3_%s_mpam_tcu",
						smmu_name(smmu_type));
	else
		mpam_mon->name = devm_kasprintf(mpam_mon->dev, GFP_KERNEL,
						"smmuv3_%s_mpam_tbu_%d",
						smmu_name(smmu_type),
						mpam_mon->txu_id - 1);
	if (!mpam_mon->name) {
		dev_err(mpam_mon->dev,
			"Create name failed, txu_id:%d\n", mpam_mon->txu_id );
		return -EINVAL;
	}

	raw_spin_lock_init(&mpam_mon->counter_lock);

	/* smmuv3 only support 4 monitor counters */
	mpam_mon->num_counters = 4;

	/* Pick one CPU to be the preferred one to use */
	mpam_mon->on_cpu = raw_smp_processor_id();
	err = cpuhp_state_add_instance_nocalls(cpuhp_state_num,
					       &mpam_mon->node);
	if (err) {
		dev_err(mpam_mon->dev,
			"Error %d registering hotplug fail\n", err);
		return err;
	}

	err = perf_pmu_register(&mpam_mon->pmu, mpam_mon->name, -1);
	if (err) {
		dev_err(mpam_mon->dev, "Error %d registering PMU\n", err);
		goto out_unregister;
	}

	return 0;

out_unregister:
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &mpam_mon->node);
	return -EINVAL;
}

static int smmu_mpam_mon_probe(struct platform_device *pdev)
{
	const struct mtk_smmu_plat_data *plat_data;
	struct device *dev = &pdev->dev;
	struct platform_device *smmudev;
	struct smmu_mpam_mon_list *mpam_mon_list;
	struct smmu_mpam_mon *mpam_mon;
	struct arm_smmu_device *smmu;
	struct device_node *smmu_node;
	struct mtk_smmu_data *data;
	int i;
	u32 txu_cnt;

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
	txu_cnt = SMMU_TBU_CNT(plat_data->smmu_type) + 1;

	mpam_mon = devm_kcalloc(dev, txu_cnt, sizeof(*mpam_mon), GFP_KERNEL);
	if (!mpam_mon)
		return -ENOMEM;

	mpam_mon_list = devm_kzalloc(dev, sizeof(*mpam_mon_list), GFP_KERNEL);
	if (!mpam_mon_list)
		return -ENOMEM;
	mpam_mon_list->mpam_mons = mpam_mon;
	mpam_mon_list->mpam_mon_cnt = txu_cnt;

	platform_set_drvdata(pdev, mpam_mon_list);

	for (i = 0; i < txu_cnt; i++) {
		mpam_mon[i].smmu = smmu;
		mpam_mon[i].txu_id = i;
		mpam_mon[i].dev = dev;
		mpam_mon[i].smmu_type = plat_data->smmu_type;
		mpam_mon[i].take_power = false;
		smmu_mpam_mon_init(&mpam_mon[i]);
	}

	return 0;
}

static int smmu_mpam_mon_remove(struct platform_device *pdev)
{
	struct smmu_mpam_mon_list *mpam_mon_list;
	struct smmu_mpam_mon *mpam_mon;
	u32 txu_cnt;
	int i;

	mpam_mon_list = platform_get_drvdata(pdev);
	txu_cnt = mpam_mon_list->mpam_mon_cnt;
	for (i = 0; i < txu_cnt; i++) {
		mpam_mon = &mpam_mon_list->mpam_mons[i];
		perf_pmu_unregister(&mpam_mon->pmu);
		cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &mpam_mon->node);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id smmu_mpam_mon_of_match[] = {
	{ .compatible = "mtk,smmu-v3-mpam-mon" },
	{}
};
MODULE_DEVICE_TABLE(of, smmu_mpam_mon_of_match);
#endif

static struct platform_driver smmu_mpam_mon_driver = {
	.driver = {
		.name = "mtk-smmu-v3-mpam-mon",
		.of_match_table = of_match_ptr(smmu_mpam_mon_of_match),
		.suppress_bind_attrs = true,
	},
	.probe = smmu_mpam_mon_probe,
	.remove = smmu_mpam_mon_remove,
};

static int __init mtk_smmu_mpam_mon_init(void)
{
	cpuhp_state_num = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
						  "perf/mtk/smmu_mpam:online",
						  NULL,
						  smmu_mpam_mon_offline_cpu);
	if (cpuhp_state_num < 0)
		return cpuhp_state_num;

	return platform_driver_register(&smmu_mpam_mon_driver);
}
module_init(mtk_smmu_mpam_mon_init);

static void __exit mtk_smmu_mpam_mon_exit(void)
{
	platform_driver_unregister(&smmu_mpam_mon_driver);
	cpuhp_remove_multi_state(cpuhp_state_num);
}

module_exit(mtk_smmu_mpam_mon_exit);

MODULE_DESCRIPTION("MPAM performance monitor driver for MTK SMMUv3");
MODULE_LICENSE("GPL");
