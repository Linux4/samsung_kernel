/*
 * IO performance mode with UFS
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#include <linux/of.h>
#include "ufs-exynos-perf.h"

#include <scsi/scsi_cmnd.h>

#define CREATE_TRACE_POINTS
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <trace/events/ufs_exynos_perf.h>
#include <ufs/ufs_exynos_boost.h>

#if IS_ENABLED(CONFIG_ARM_EXYNOS_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
static struct ufs_perf *_perf;

static int ufs_perf_cpufreq_nb(struct notifier_block *nb,
		unsigned long event, void *arg)
{
	struct cpufreq_policy *policy = (struct cpufreq_policy *)arg;
	struct ufs_perf *perf = _perf;

	if (event != CPUFREQ_CREATE_POLICY)
		return NOTIFY_OK;

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	freq_qos_tracer_add_request(&policy->constraints,
			&perf->pm_qos_cluster[policy->cpu], FREQ_QOS_MIN, 0);
#else
	freq_qos_add_request(&policy->constraints,
			&perf->pm_qos_cluster[policy->cpu], FREQ_QOS_MIN, 0);
#endif
	return NOTIFY_OK;
}
#endif

void ufs_perf_wakeup(struct ufs_perf *perf)
{
	struct ufs_hba *hba = perf->hba;

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)
		return;

	wake_up_process(perf->handler);
}

static int ufs_perf_handler(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	unsigned long flags;
	u32 ctrl_handle[__CTRL_REQ_MAX];
	int i;
	int idle;

	while (true) {
		if (kthread_should_stop())
			break;

		__set_current_state(TASK_RUNNING);
		/* get requests */
		spin_lock_irqsave(&perf->lock_handle, flags);
		for (i = 0; i < __CTRL_REQ_MAX; i++) {
			ctrl_handle[i] = perf->ctrl_handle[i];
			perf->ctrl_handle[i] = CTRL_OP_NONE;
		}
		spin_unlock_irqrestore(&perf->lock_handle, flags);

		/* execute */
		idle = 0;
		for (i = 0; i < __CTRL_REQ_MAX; i++) {
			if (ctrl_handle[i] == CTRL_OP_NONE) {
				idle++;
			} else if (perf->ctrl[i]) {
				/* TODO: implement */
				perf->ctrl[i](perf, ctrl_handle[i]);
			}
		}
		if (idle == __CTRL_REQ_MAX) {
			trace_ufs_perf_lock("sleep", ctrl_handle[0]);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			trace_ufs_perf_lock("wake-up", ctrl_handle[0]);
		}
	}

	return 0;
}

/* EXTERNAL FUNCTIONS */
void ufs_perf_update(void *data, u32 qd, struct ufshcd_lrb *lrbp,
		ufs_perf_op op)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	policy_res res = R_OK;
	unsigned long stat_bits = (unsigned long)perf->stat_bits;
	struct scsi_cmnd *scmd = lrbp->cmd;
	ktime_t time = ktime_get();
	int index;
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;
	struct ufs_perf_stat_v2 *stat_v2 = &perf->stat_v2;
	unsigned long len;

	if (perf->perf_suspend == PERF_SUSPEND_PREPARE)
		return;

	switch(op) {
	case UFS_PERF_OP_R:
	case UFS_PERF_OP_W:
		len = be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		stat_v2->count += len;
		break;

	default:
		len = scsi_cmd_to_rq(scmd)->__data_len;
		stat_v2->count += len;
		break;
	}

	if (len >= SZ_256K)
		stat->seq_count++;

	for_each_set_bit(index, &stat_bits, __UPDATE_MAX) {
		if (!(BIT(index) & perf->stat_bits))
			continue;
		res = perf->update[index](perf, qd, op, UFS_PERF_ENTRY_QUEUED);

		/* wake-up thread */
		if (res == R_CTRL)
			ufs_perf_wakeup(perf);
	}

	trace_ufs_perf("update", op, qd, ktime_to_us(ktime_sub(ktime_get(),
					time)), res, len);
}

void ufs_perf_reset(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	policy_res res = R_OK;
	policy_res res_t = R_OK;
	int index;

	for (index = 0; index < __UPDATE_MAX; index++) {
		if (!(BIT(index) & perf->stat_bits))
			continue;
		res_t = perf->update[index](perf, 0, UFS_PERF_OP_NONE,
				UFS_PERF_ENTRY_RESET);
		if (res_t == R_CTRL)
			res = res_t;
	}

	/* wake-up thread */
	if (res_t == R_CTRL)
		ufs_perf_wakeup(perf);
}

void ufs_perf_resume(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	int index;

	perf->perf_suspend = PERF_NORMAL;

	for (index = 0; index < __UPDATE_MAX; index++) {
		if (!(BIT(index) & perf->stat_bits))
			continue;
		if (!perf->resume[index])
			continue;

		perf->resume[index](perf);
	}
}

int ufs_perf_parse_cpu_clusters(struct ufs_perf *perf)
{
	struct device_node *cpus, *map, *cluster, *cpu_node;
	unsigned int *clusters = perf->clusters;
	struct device *dev = perf->hba->dev;
	int num_clusters = 0;
	char name[20];

	cpus = of_find_node_by_path("/cpus");
	if (!cpus) {
		dev_err(dev, "No CPU information found in DT\n");
		return -ENOENT;
	}

	map = of_get_child_by_name(cpus, "cpu-map");
	if (!map) {
		dev_err(dev, "No CPU MAP node found in DT\n");
		return -ENOENT;
	}

	do {
		snprintf(name, sizeof(name), "cluster%d", num_clusters);
		cluster = of_get_child_by_name(map, name);
		if (cluster) {
			cpu_node = of_get_child_by_name(cluster, "core0");
			if (cpu_node) {
				cpu_node = of_parse_phandle(cpu_node, "cpu", 0);
				clusters[num_clusters] =
					of_cpu_node_to_id(cpu_node);
			}
			num_clusters++;
		} else {
			break;
		}
	} while (cluster);
	perf->num_clusters = num_clusters;

	return 0;
}

void ufs_init_cpufreq_request(struct ufs_perf *perf, bool add_knob)
{
	unsigned int wished[MAX_CLUSTERS] = {0,};
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos, *table;
	struct ufs_hba *hba = perf->hba;
	struct device *dev = perf->hba->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	int table_len = 0;
	int i, ret;

	ret = ufs_perf_parse_cpu_clusters(perf);
	if (ret) {
		dev_err(dev, "%s: failed to parse CPU DT\n", __func__);
		return;
	}

	child_np = of_get_child_by_name(np, "ufs-pm-qos");
	if (!child_np)
		dev_info(dev, "%s: No ufs-pm-qos node, not quarantee pm qos\n",
				__func__);
	else {
		ret = of_property_count_u32_elems(child_np,
						"cpufreq-qos-levels");
		if (ret < 0) {
			dev_info(dev, "%s: No ufs-pm-qos node, not quarantee pm qos\n", __func__);
			return;
		}
		of_property_read_u32_array(child_np,
						"cpufreq-qos-levels", wished,
						ret);
	}

	pr_info("%s: clusters num: %d\n", __func__, perf->num_clusters);
	policy = cpufreq_cpu_get(perf->clusters[perf->num_clusters - 1]);
	if (!policy)
		return;

	perf->pm_qos_cluster = devm_kzalloc(hba->dev,
			sizeof(struct freq_qos_request) * (policy->cpu + 1),
			GFP_KERNEL);
	if (!perf->pm_qos_cluster) {
		pr_info("%s: pm_qos_cluster alloc fail!!\n", __func__);
		return;
	}

	perf->cluster_qos_value = devm_kzalloc(hba->dev,
			sizeof(s32) * (policy->cpu + 1), GFP_KERNEL);
	if (!perf->cluster_qos_value) {
		pr_info("%s: cluster_qos_value alloc fail!!\n", __func__);
		return;
	}

	for (i = 0; i < perf->num_clusters; i++) {
		policy = cpufreq_cpu_get(perf->clusters[i]);
		if (!policy)
			continue;

#if IS_ENABLED(CONFIG_ARM_EXYNOS_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
		if (add_knob)
			ufs_perf_cpufreq_nb(NULL, CPUFREQ_CREATE_POLICY, policy);
#endif
		table = policy->freq_table;
		cpufreq_for_each_valid_entry(pos, table) {
			table_len++;
		}

		if (table_len > wished[i])
			pos = table + wished[i];
		else
			pos = table + (table_len - 1);

		perf->cluster_qos_value[i] = pos->frequency;
	}
}

void ufs_init_devfreq_request(struct ufs_perf *perf, struct ufs_hba *hba) {
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;

	child_np = of_get_child_by_name(np, "ufs-pm-qos");
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	perf->val_pm_qos_int = 0;
	perf->val_pm_qos_mif = 0;

	if (!child_np)
		dev_info(dev, "%s: No ufs-pm-qos node, not guarantee pm qos\n",
				__func__);
	else {
		of_property_read_u32(child_np, "perf-int",
				&perf->val_pm_qos_int);
		of_property_read_u32(child_np, "perf-mif",
				&perf->val_pm_qos_mif);
	}

	exynos_pm_qos_add_request(&perf->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&perf->pm_qos_mif, PM_QOS_BUS_THROUGHPUT, 0);
#endif
}

static int exynos_ufs_perf_pm_notifier(struct notifier_block *nb,
		unsigned long pm_event, void *v)
{
	struct ufs_perf *perf = container_of(nb, struct ufs_perf, pm_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s: suspend prepare\n", __func__);
		perf->perf_suspend = PERF_SUSPEND_PREPARE;
		break;

	}

	return NOTIFY_OK;
}
static void boost_timeout(struct timer_list *t)
{
	struct ufs_perf *perf = from_timer(perf, t, boost_timer);

	if (perf->active_cnt) {
		pr_info("%s: timeout! disable boost_mode!\n", __func__);
		ufs_perf_request_boost_mode(&perf->gear_scale_rq, REQUEST_NORMAL);
		perf->active_cnt = 0;
	}
}

bool ufs_perf_init(void **data, struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct ufs_perf *perf;
	bool ret = false;

#if IS_ENABLED(CONFIG_ARM_EXYNOS_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	struct cpufreq_policy pol;
#endif

	/* perf and perf->handler is used to check using performance mode */
	*data = devm_kzalloc(hba->dev, sizeof(struct ufs_perf), GFP_KERNEL);
	if (*data == NULL)
		goto out;

	perf = (struct ufs_perf *)(*data);

	spin_lock_init(&perf->lock_handle);

	perf->hba = hba;
	perf->handler = kthread_run(ufs_perf_handler, perf,
				"ufs_perf_%d", 0);
	if (IS_ERR(perf->handler))
		goto out;

	perf->gear_scale_sup = 0;
	perf->exynos_gear_scale = 0;
	if (of_find_property(np, "samsung,ufs-gear-scale", NULL)) {
		dev_info(dev, "%s: enable ufs-gear-scale\n", __func__);
		perf->gear_scale_sup = 1;
		perf->exynos_gear_scale = 1;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	ufs_init_devfreq_request(perf, hba);
#endif

#if IS_ENABLED(CONFIG_ARM_EXYNOS_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	_perf = perf;
	if (cpufreq_get_policy(&pol, 0) != 0) {
		perf->cpufreq_nb.notifier_call = ufs_perf_cpufreq_nb;
		cpufreq_register_notifier(&perf->cpufreq_nb,
				CPUFREQ_POLICY_NOTIFIER);

		ufs_init_cpufreq_request(perf, false);

	} else {
		ufs_init_cpufreq_request(perf, true);
	}
#endif

	perf->pm_nb.notifier_call = exynos_ufs_perf_pm_notifier;
	register_pm_notifier(&perf->pm_nb);

	/* initial values, TODO: */
	perf->stat_bits = UPDATE_V1;

	/* register updates and ctrls */
	ufs_perf_init_v1(perf);
	if (perf->gear_scale_sup) {
		perf->stat_bits |= UPDATE_GEAR;
		ufs_gear_scale_init(perf);

		/* boost mode request support */
		INIT_LIST_HEAD(&perf->hs_list);
		spin_lock_init(&perf->hs_lock);
		ufs_perf_add_boost_mode_request(&perf->gear_scale_rq);
		perf->active_cnt = 0;

		timer_setup(&perf->boost_timer, boost_timeout, 0);
	}

	perf->perf_suspend = PERF_NORMAL;
	ret = true;
out:
	return ret;
}

void ufs_perf_exit(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	struct ufs_hba *hba = perf->hba;
	int i;

	ufs_perf_exit_v1(perf);

	if (perf->gear_scale_sup)
		ufs_gear_scale_exit(perf);

	unregister_pm_notifier(&perf->pm_nb);

	if (perf && !IS_ERR(perf->handler)) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_remove_request(&perf->pm_qos_int);
		exynos_pm_qos_remove_request(&perf->pm_qos_mif);
#endif

		for (i = 0; i < perf->num_clusters; i++) {
			struct cpufreq_policy *policy;

			policy = cpufreq_cpu_get(perf->clusters[i]);
			if (!policy)
				continue;

			if (!perf->pm_qos_cluster)
				return;

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
			freq_qos_tracer_remove_request(&perf->pm_qos_cluster[policy->cpu]);
#elif IS_ENABLED(CONFIG_ARM_EXYNOS_CPU_FREQ)
			freq_qos_remove_request(&perf->pm_qos_cluster[policy->cpu]);
#endif
		}

		kthread_stop(perf->handler);
	}

	devm_kfree(hba->dev, perf->pm_qos_cluster);
	devm_kfree(hba->dev, perf->cluster_qos_value);
}

void __ufs_perf_add_boost_mode_request(char *func, unsigned int line,
						struct ufs_boost_request *rq)
{
	struct ufs_perf *perf = _perf;

	if (!perf || !perf->gear_scale_sup) {
		pr_info("Gear scale not supported\n");
		return;
	}

	pr_info("%s: Add boost mode request\n", __func__);
	if (!rq)
		return;

	INIT_LIST_HEAD(&rq->list);
	rq->func = func;
	rq->line = line;
}
EXPORT_SYMBOL_GPL(__ufs_perf_add_boost_mode_request);

int ufs_perf_request_boost_mode(struct ufs_boost_request *rq,
						enum ufs_perf_request_mode mode)
{
	struct ufs_perf *perf = _perf;
	struct ufs_boost_request *iter, *tmp = NULL;
	u8 prev;
	int ret = 0;

	if (!perf || !rq || !rq->func || !rq->line) {
		ret = -ENOMEM;
		goto out;
	}

	if (!perf->gear_scale_sup) {
		ret = -ENOTSUPP;
		goto out;
	}

	prev = perf->exynos_gear_scale;

	switch (mode) {
	case REQUEST_NORMAL:
		/* Check the request is in the list or not */
		spin_lock(&perf->hs_lock);
		list_for_each_entry(iter, &perf->hs_list, list) {
			if (!strcmp(iter->func, rq->func) &&
					(iter->line == rq->line)) {
				tmp = iter;
				break;
			}
		}

		if (tmp) {
			/* remove the request from the hs_list */
			list_del(&tmp->list);
			tmp->e_time = ktime_to_ms(ktime_get());
			spin_unlock(&perf->hs_lock);

			/* Check whether hs_list is empty or not */
			if (list_empty(&perf->hs_list))
				perf->exynos_gear_scale = 1;
		} else {
			spin_unlock(&perf->hs_lock);
			goto out;
		}
		break;

	case REQUEST_BOOST:
		/* Check the request is already in the list or not */
		spin_lock(&perf->hs_lock);
		list_for_each_entry(iter, &perf->hs_list, list) {
			if (!strcmp(iter->func, rq->func) &&
					(iter->line == rq->line)) {
				spin_unlock(&perf->hs_lock);
				goto out;
			}
		}

		/* queue the request into the hs_list */
		rq->s_time = ktime_to_ms(ktime_get());
		list_add(&rq->list, &perf->hs_list);
		spin_unlock(&perf->hs_lock);

		/* If gear scale was enabled, disable gear scale */
		if (perf->exynos_gear_scale)
			perf->exynos_gear_scale = 0;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (prev != perf->exynos_gear_scale) {
		pr_info("ufs: gear scale %s by boost request from %s\n",
			(perf->exynos_gear_scale) ? "enabled" : "disabled",
			rq->func);
		ufs_gear_scale_update(perf);
	}

out:
	return ret;
}
EXPORT_SYMBOL_GPL(ufs_perf_request_boost_mode);

MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos UFS performance booster");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
