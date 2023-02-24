/* performance mode with UFS
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
#include "ufs-perf.h"
#define CREATE_TRACE_POINTS
#include <trace/events/ufs_perf.h>

static void ufs_perf_cp_and_trg(struct ufs_perf_control *perf,
		bool is_big, bool is_cp_time, s64 time, u32 ctrl_flag)
{
	unsigned long flags;

	spin_lock_irqsave(&perf->lock, flags);
	perf->cur_time = time;
	perf->ctrl_flag = ctrl_flag;
	if (is_cp_time) {
		perf->count_b = perf->count_l = 0;
		perf->cp_time = time;
	} else {
		if (is_big)
			perf->count_b++;
		else
			perf->count_l++;
	}

	/*
	 * In increasing traffic cases after idle,
	 * make it do not update stat because we assume
	 * this is IO centric scenarios.
	 */
	if (perf->is_locked && !perf->is_held &&
			ctrl_flag == UFS_PERF_LOCK)
		perf->is_held = true;

	if (!perf->is_active) {
		if (!perf->is_locked &&
				ctrl_flag == UFS_PERF_LOCK) {
			trace_ufs_perf_lock(49);
			perf->wq_qos_cond = true;
			wake_up(&perf->wq_qos_hd);
		}
		if (perf->is_locked &&
				ctrl_flag == UFS_PERF_RELEASE) {
			trace_ufs_perf_lock(99);
			perf->wq_qos_cond = true;
			wake_up(&perf->wq_qos_hd);
		}
	}
	spin_unlock_irqrestore(&perf->lock, flags);
}

static int ufs_perf_handler(void *data)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	u32 ctrl_flag;
	unsigned long flags;
	bool is_locked;
	bool is_held;
	int ret = 0;

	perf->is_active = true;
	perf->is_locked = false;
	perf->is_held = false;
	init_waitqueue_head(&perf->wq_qos_hd);

	while (ret == 0) {
		if (kthread_should_stop())
			break;

		/* ctrl_flag should be reset to get incoming request */
		spin_lock_irqsave(&perf->lock, flags);
		ctrl_flag = perf->ctrl_flag;
		perf->ctrl_flag = UFS_PERF_NONE;
		is_locked = perf->is_locked;
		is_held = perf->is_held;	//
		spin_unlock_irqrestore(&perf->lock, flags);

		if (ctrl_flag == UFS_PERF_LOCK) {
			if (!is_locked) {
				if (perf->pm_qos_int_value)
					pm_qos_update_request(&perf->pm_qos_int, perf->pm_qos_int_value);
				if (perf->pm_qos_mif_value)
					pm_qos_update_request(&perf->pm_qos_mif, perf->pm_qos_mif_value);
				if (perf->pm_qos_cluster2_value)
					pm_qos_update_request(&perf->pm_qos_cluster2, perf->pm_qos_cluster2_value);
				if (perf->pm_qos_cluster1_value)
					pm_qos_update_request(&perf->pm_qos_cluster1, perf->pm_qos_cluster1_value);
				if (perf->pm_qos_cluster0_value)
					pm_qos_update_request(&perf->pm_qos_cluster0, perf->pm_qos_cluster0_value);

				spin_lock_irqsave(&perf->lock, flags);
				trace_ufs_perf_lock(1);
				spin_unlock_irqrestore(&perf->lock, flags);
			}

			spin_lock_irqsave(&perf->lock, flags);
			perf->is_locked = true;
			perf->is_held = true;
			spin_unlock_irqrestore(&perf->lock, flags);
		} else if (ctrl_flag == UFS_PERF_RELEASE) {
			if (is_locked) {
				spin_lock_irqsave(&perf->lock, flags);
				trace_ufs_perf_lock(3);
				spin_unlock_irqrestore(&perf->lock, flags);

				if (perf->pm_qos_int_value)
					pm_qos_update_request(&perf->pm_qos_int, 0);
				if (perf->pm_qos_mif_value)
					pm_qos_update_request(&perf->pm_qos_mif, 0);
				if (perf->pm_qos_cluster2_value)
					pm_qos_update_request(&perf->pm_qos_cluster2, 0);
				if (perf->pm_qos_cluster1_value)
					pm_qos_update_request(&perf->pm_qos_cluster1, 0);
				if (perf->pm_qos_cluster0_value)
					pm_qos_update_request(&perf->pm_qos_cluster0, 0);
			}

			spin_lock_irqsave(&perf->lock, flags);
			perf->is_locked = false;
			trace_ufs_perf_lock(8);
			spin_unlock_irqrestore(&perf->lock, flags);
		} else {
			spin_lock_irqsave(&perf->lock, flags);
			perf->is_active = false;
			trace_ufs_perf_lock(8);
			perf->wq_qos_cond = false;
			spin_unlock_irqrestore(&perf->lock, flags);

			ret = wait_event_interruptible(perf->wq_qos_hd, perf->wq_qos_cond);

			trace_ufs_perf_lock(9);
			spin_lock_irqsave(&perf->lock, flags);
			perf->is_active = true;
			spin_unlock_irqrestore(&perf->lock, flags);
		}

	}

	if (ret)
		WARN(1, "ufs_perf_handler might receive signals, so terminated = %d\n", ret);
	spin_lock_irqsave(&perf->lock, flags);
	perf->handler = NULL;
	spin_unlock_irqrestore(&perf->lock, flags);
	perf->is_active = false;

	return 0;
}

/* EXTERNAL FUNCTIONS */

static void ufs_perf_reset_timer(struct timer_list *t)
{
	struct ufs_perf_control *perf = from_timer(perf, t, reset_timer);
	unsigned long flags;
	u32 ctrl_flag;

	spin_lock_irqsave(&perf->lock, flags);
	ctrl_flag = perf->ctrl_flag_in_transit;
	perf->ctrl_flag_in_transit = UFS_PERF_NONE;
	spin_unlock_irqrestore(&perf->lock, flags);

	ufs_perf_cp_and_trg(perf, false, true, -1LL, ctrl_flag);
}

/* check point to re-initiate perf stat */
void ufs_perf_reset(void *data, bool boot)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	unsigned long flags;
	u32 ctrl_flag = (boot) ? UFS_PERF_NONE : UFS_PERF_RELEASE;

	if (!perf || !(perf->mode) || IS_ERR(perf->handler))
		return;

	spin_lock_irqsave(&perf->lock, flags);
	perf->ctrl_flag_in_transit = ctrl_flag;
	perf->is_held = false;
	spin_unlock_irqrestore(&perf->lock, flags);
	mod_timer(&perf->reset_timer, jiffies + msecs_to_jiffies(perf->th_reset_in_ms) + 1);
}

void ufs_perf_update_stat(void *data, unsigned int len, enum ufs_perf_op op)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	unsigned long flags;
	s64 time;
	bool is_big;
	bool is_cp_time = false;
	u64 time_diff_in_ns;
	u32 ctrl_flag = UFS_PERF_NONE;

	u32 count;
	s64 cp_time;
	s64 cur_time;

	u32 th_count;
	u32 th_period_in_ms;	/* period for check point */

	BUG_ON(op >= UFS_PERF_OP_MAX);

	/* perf->mpde is not enable */
	if (!perf || !(perf->mode) || IS_ERR(perf->handler))
		return;

	del_timer_sync(&perf->reset_timer);
	is_big = (len >= perf->th_chunk_in_kb * 1024);
	trace_ufs_perf_issue((int)is_big, (int)op, (int)len);

	/* Once triggered, lock state is hold right befor idle */
	if (perf->is_held)
		return;

	th_count = (is_big) ? perf->th_count_b : perf->th_count_l;
	th_period_in_ms = (is_big) ? perf->th_period_in_ms_b :
		perf->th_period_in_ms_l;
	time = cpu_clock(raw_smp_processor_id());

	spin_lock_irqsave(&perf->lock, flags);
	count = (is_big) ? perf->count_b : perf->count_l;
	cur_time = perf->cur_time;

	/* in first case, just update time */
	cp_time = perf->cp_time;
	if (cp_time  == -1LL) {
		perf->cur_time = perf->cp_time = time;
		spin_unlock_irqrestore(&perf->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&perf->lock, flags);

	/* check if check point is needed */
	time_diff_in_ns = (cp_time > cur_time) ? (u64)(cp_time - cur_time) :
		(u64)(cur_time - cp_time);
	if (time_diff_in_ns >= (th_period_in_ms * (1000 * 1000))) {
		is_cp_time = true;

		/* check heavy load */
		ctrl_flag = (count + 1 >= th_count) ?
			UFS_PERF_LOCK : UFS_PERF_RELEASE;
	}

	spin_lock_irqsave(&perf->lock, flags);
	trace_ufs_perf_stat(is_cp_time, time_diff_in_ns, th_period_in_ms,
			cp_time, cur_time, count, th_count,
			(int)perf->is_locked, (int)perf->is_active);
	spin_unlock_irqrestore(&perf->lock, flags);
	ufs_perf_cp_and_trg(perf, is_big, is_cp_time, time, ctrl_flag);
}

int ufs_perf_populate_dt(void *data, struct device_node *np)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;
	int ret = 0;

	if (!perf || IS_ERR(perf->handler)) {
		ret = -1;
		goto out;
	}

	/* perf mode enable : 1, disable 0 */
	if (of_property_read_u32(np, "perf-mode", &perf->mode)) {
		perf->mode = 0;
		ret = -1;
		goto out;
	}

	/* Default, not to throttle device tp */
	if (of_property_read_u32(np, "perf-int", &perf->pm_qos_int_value))
		perf->pm_qos_int_value = 533000;

	if (of_property_read_u32(np, "perf-mif", &perf->pm_qos_mif_value))
		perf->pm_qos_mif_value = 2288000;

	/* Default, to issue request fast */
	if (of_property_read_u32(np, "perf-cluster2", &perf->pm_qos_cluster2_value))
		perf->pm_qos_cluster1_value = 2314000;

	if (of_property_read_u32(np, "perf-cluster1", &perf->pm_qos_cluster1_value))
		perf->pm_qos_cluster1_value = 2314000;

	if (of_property_read_u32(np, "perf-cluster0", &perf->pm_qos_cluster0_value))
		perf->pm_qos_cluster0_value = 1950000;

	if (of_property_read_u32(np, "perf-chunk", &perf->th_chunk_in_kb))
		perf->th_chunk_in_kb = 128;

	if (of_property_read_u32(np, "perf-count-b", &perf->th_count_b))
		perf->th_count_b = 40;

	if (of_property_read_u32(np, "perf-count-l", &perf->th_count_l))
		perf->th_count_l = 48;

	/* Default, to escape scenarios that requires power consumption */
	if (of_property_read_u32(np, "perf-period-in-ms-b", &perf->th_period_in_ms_b))
		perf->th_period_in_ms_b = 160;

	/* Default, to escape scenarios that requires power consumption */
	if (of_property_read_u32(np, "perf-period-in-ms-l", &perf->th_period_in_ms_l))
		perf->th_period_in_ms_l = 3;

	/* Default, to escape idle case during IO centric cases */
	if (of_property_read_u32(np, "perf-reset-delay-in-ms", &perf->th_reset_in_ms))
		perf->th_reset_in_ms = 30;
out:
       return ret;
}

bool ufs_perf_init(void **data, struct device *dev)
{
	struct ufs_perf_control *perf;
	bool ret = false;

	/* perf and perf->handler is used to check using performance mode */
	*data = devm_kzalloc(dev, sizeof(struct ufs_perf_control), GFP_KERNEL);
	if (*data == NULL)
		goto out;

	perf = (struct ufs_perf_control *)(*data);

	perf->handler = kthread_run(ufs_perf_handler, perf,
			"ufs_perf_%d", 0);
	if (IS_ERR(perf->handler))
		goto out;

	timer_setup(&perf->reset_timer, ufs_perf_reset_timer, 0);

	pm_qos_add_request(&perf->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
	pm_qos_add_request(&perf->pm_qos_mif, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&perf->pm_qos_cluster0, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&perf->pm_qos_cluster1, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&perf->pm_qos_cluster2, PM_QOS_CLUSTER2_FREQ_MIN, 0);

	ret = true;
out:
	return ret;
}

void ufs_perf_exit(void *data)
{
	struct ufs_perf_control *perf = (struct ufs_perf_control *)data;

	if (perf && !IS_ERR(perf->handler)) {
		pm_qos_remove_request(&perf->pm_qos_int);
		pm_qos_remove_request(&perf->pm_qos_mif);
		pm_qos_remove_request(&perf->pm_qos_cluster2);
		pm_qos_remove_request(&perf->pm_qos_cluster1);
		pm_qos_remove_request(&perf->pm_qos_cluster0);

		kthread_stop(perf->handler);
	}
}

