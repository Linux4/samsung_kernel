/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <soc/samsung/bts.h>

#include "npu-scheduler-governor.h"
#include "npu-device.h"
#include "npu-system-soc.h"
#include "npu-llc.h"

static struct npu_scheduler_info *g_npu_scheduler_info;

struct npu_scheduler_info *npu_scheduler_get_info(void)
{
	if (unlikely(!g_npu_scheduler_info)) {
		npu_err("g_npu_scheduler_info pointer is NULL\n");
		return NULL;
	} else {
		return g_npu_scheduler_info;
	}
}

int npu_scheduler_enable(struct npu_scheduler_info *info)
{
	if (!info) {
		npu_err("npu_scheduler_info is NULL!\n");
		return -EINVAL;
	}

	info->enable = 1;

	/* re-schedule work */
	if (info->activated) {
		cancel_delayed_work_sync(&info->sched_work);
		queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(0));
	}

	npu_info("done\n");
	return 1;
}

int npu_scheduler_disable(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;

	if (!info) {
		npu_err("npu_scheduler_info is NULL!\n");
		return -EINVAL;
	}

	info->enable = 0;
	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		npu_pm_qos_update_request(d, &d->qos_req_min, d->min_freq);
	}
	mutex_unlock(&info->exec_lock);

	npu_info("done\n");
	return 1;
}

void npu_scheduler_activate_peripheral_dvfs(unsigned long freq)
{
	bool dvfs_active = false;
	bool is_core = false;
	int i;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = g_npu_scheduler_info;

	if (!info->activated)
		return;

	mutex_lock(&info->exec_lock);
	/* get NPU max freq */
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp("NPU", d->name))
			break;
	}
	npu_info("NPU maxlock at %ld\n", freq);
	if (freq >= d->max_freq)
		/* maxlock deactivated, DVFS should be active */
		dvfs_active = true;
	else
		/* maxlock activated, DVFS should be inactive */
		dvfs_active = false;

	list_for_each_entry(d, &info->ip_list, ip_list) {
		is_core = false;
		/* check for core DVFS */
		for (i = 0; i < (int)ARRAY_SIZE(npu_scheduler_core_name); i++)
			if (!strcmp(npu_scheduler_core_name[i], d->name)) {
				is_core = true;
				break;
			}

		/* only for peripheral DVFS */
		if (!is_core) {
			npu_pm_qos_update_request(d, &d->qos_req_min, d->min_freq);
			if (dvfs_active)
				d->activated = 1;
			else
				d->activated = 0;
			npu_info("%s %sactivated\n", d->name,
					d->activated ? "" : "de");
		}
	}
	mutex_unlock(&info->exec_lock);
}

s32 npu_scheduler_get_freq_ceil(struct npu_scheduler_dvfs_info *d, s32 freq) {
	struct dev_pm_opp *opp;
	unsigned long f;

	/* Calculate target frequency */
	f = (unsigned long) freq;
	opp = dev_pm_opp_find_freq_ceil(&d->dvfs_dev->dev, &f);

	if (IS_ERR(opp))
		return 0;
	else {
		dev_pm_opp_put(opp);
		return (s32)f;
	}
}

void npu_scheduler_set_bts(struct npu_scheduler_info *info)
{
	int ret = 0;

	if (info->mode == NPU_PERF_MODE_NORMAL) {
		info->bts_scenindex = bts_get_scenindex("npu_normal");
		if (info->bts_scenindex < 0) {
			npu_err("bts_get_scenindex failed : %d\n", info->bts_scenindex);
			ret = info->bts_scenindex;
			goto p_err;
		}

		ret = bts_add_scenario(info->bts_scenindex);
		if (ret)
			npu_err("bts_add_scenario failed : %d\n", ret);
	} else if (info->mode == NPU_PERF_MODE_NPU_BOOST ||
			info->mode == NPU_PERF_MODE_NPU_DN) {
		info->bts_scenindex = bts_get_scenindex("npu_performance");
		if (info->bts_scenindex < 0) {
			npu_err("bts_get_scenindex failed : %d\n", info->bts_scenindex);
			ret = info->bts_scenindex;
			goto p_err;
		}

		ret = bts_add_scenario(info->bts_scenindex);
		if (ret)
			npu_err("bts_add_scenario failed : %d\n", ret);
	} else {
		if (info->bts_scenindex > 0) {
			ret = bts_del_scenario(info->bts_scenindex);
			if (ret)
				npu_err("bts_del_scenario failed : %d\n", ret);
		} else
			npu_warn("BTS scen index[%d] is not initialized. "
					"Del scenario will be called.\n", info->bts_scenindex);
	}
p_err:
	return;
}

void npu_pm_qos_update_request(
		struct npu_scheduler_dvfs_info *d,
		struct pm_qos_request *req, s32 new_value)
{
	pm_qos_update_request(req, new_value);
	d->cur_freq = (s32)pm_qos_read_req_value(
			req->pm_qos_class, req);
	npu_trace("freq for %s : %d (%d)\n",
			d->name, d->cur_freq,
			pm_qos_request(req->pm_qos_class));
}

static int npu_scheduler_create_attrs(
		struct device *dev,
		struct device_attribute attrs[],
		int device_attr_num)
{
	unsigned long i;
	int rc;

	npu_info("creates scheduler attributes %d sysfs\n",
			device_attr_num);
	for (i = 0; i < device_attr_num; i++) {
		npu_info("sysfs info %s %p %p\n",
				attrs[i].attr.name, attrs[i].show, attrs[i].store);
		rc = device_create_file(dev, &(attrs[i]));
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &(attrs[i]));
create_attrs_succeed:
	return rc;
}

static int npu_scheduler_init_dt(struct npu_scheduler_info *info)
{
	int i, count, ret = 0;
	unsigned long f = 0;
	struct dev_pm_opp *opp;
	char *tmp_name;
	struct npu_scheduler_dvfs_info *dinfo;
	struct npu_scheduler_governor *g;
	struct of_phandle_args pa;

	BUG_ON(!info);

	probe_info("scheduler init by devicetree\n");

	count = of_property_count_strings(info->dev->of_node, "samsung,npusched-names");
	if (IS_ERR_VALUE((unsigned long)count)) {
		probe_err("invalid npusched list in %s node\n", info->dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}

	for (i = 0; i < count; i += 2) {
		/* get dvfs info */
		dinfo = (struct npu_scheduler_dvfs_info *)devm_kzalloc(info->dev,
				sizeof(struct npu_scheduler_dvfs_info), GFP_KERNEL);
		if (!dinfo) {
			probe_err("failed to alloc dvfs info\n");
			ret = -ENOMEM;
			goto err_exit;
		}

		/* get dvfs name (same as IP name) */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i,
				(const char **)&dinfo->name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node\n",
					i, info->dev->of_node->name);
			goto err_dinfo;
		}
		/* get governor name  */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i + 1,
				(const char **)&tmp_name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node\n",
					i + 1, info->dev->of_node->name);
			goto err_dinfo;
		}

		probe_info("set up %s with %s governor\n", dinfo->name, tmp_name);
		/* get and set governor */
		g = npu_scheduler_governor_get(info, tmp_name);
		if (!g) {
			probe_err("failed to find registered governor %s\n",
					tmp_name);
			ret = -EINVAL;
			goto err_dinfo;
		}
		dinfo->gov = g;

		/* get dvfs and pm-qos info */
		ret = of_parse_phandle_with_fixed_args(info->dev->of_node,
				"samsung,npusched-dvfs",
				NPU_SCHEDULER_DVFS_TOTAL_ARG_NUM, i / 2, &pa);
		if (ret) {
			probe_err("failed to read dvfs args %d from %s node\n",
					i / 2, info->dev->of_node->name);
			goto err_dinfo;
		}

		dinfo->dvfs_dev = of_find_device_by_node(pa.np);
		if (!dinfo->dvfs_dev) {
			probe_err("invalid dt node for %s devfreq device with %d args\n",
					dinfo->name, pa.args_count);
			ret = -EINVAL;
			goto err_dinfo;
		}
		f = ULONG_MAX;
		opp = dev_pm_opp_find_freq_floor(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid max freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->max_freq = f;
			dev_pm_opp_put(opp);
		}
		f = 0;
		opp = dev_pm_opp_find_freq_ceil(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid min freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->min_freq = f;
			dev_pm_opp_put(opp);
		}

		pm_qos_add_request(&dinfo->qos_req_min,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		npu_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				dinfo->qos_req_min.pm_qos_class,
				dinfo->min_freq);
		pm_qos_add_request(&dinfo->qos_req_max,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		npu_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				dinfo->qos_req_max.pm_qos_class,
				dinfo->max_freq);
		pm_qos_add_request(&dinfo->qos_req_min_nw_boost,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request for boosting %s %d as %d\n",
				dinfo->name,
				dinfo->qos_req_min_nw_boost.pm_qos_class,
				dinfo->min_freq);

		probe_info("%s %d %d %d %d %d %d\n", dinfo->name,
				pa.args[0], pa.args[1], pa.args[2],
				pa.args[3], pa.args[4], pa.args[5]);

		/* reset values */
		dinfo->cur_freq = dinfo->min_freq;
		dinfo->delay = 0;
		dinfo->limit_min = 0;
		dinfo->limit_max = INT_MAX;
		dinfo->curr_up_delay = 0;
		dinfo->curr_down_delay = 0;

		/* get governor property slot */
		dinfo->gov_prop = (void *)g->ops->init_prop(dinfo, info->dev, pa.args);
		if (!dinfo->gov_prop) {
			probe_err("failed to init governor property for %s\n",
					dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		}

		/* add device in governor */
		list_add_tail(&dinfo->dev_list, &dinfo->gov->dev_list);

		/* add device in scheduler */
		list_add_tail(&dinfo->ip_list, &info->ip_list);

		probe_info("add %s in list\n", dinfo->name);
	}

	return ret;
err_dinfo:
	devm_kfree(info->dev, dinfo);
err_exit:
	return ret;
}

static void npu_scheduler_work(struct work_struct *work);
static void npu_scheduler_boost_off_work(struct work_struct *work);
static int npu_scheduler_init_info(s64 now, struct npu_scheduler_info *info)
{
	int i, ret = 0;
	const char *mode_name;
	struct npu_qos_setting *qos_setting;

	npu_info("scheduler info init\n");
	qos_setting = &(info->device->system.qos_setting);

	info->enable = 1;	/* default enable */
	ret = of_property_read_string(info->dev->of_node,
			"samsung,npusched-mode", &mode_name);
	if (ret)
		info->mode = NPU_PERF_MODE_NONE;
	else {
		for (i = 0; i < ARRAY_SIZE(npu_perf_mode_name); i++)
			if (!strcmp(npu_perf_mode_name[i], mode_name))
				break;
		if (i == ARRAY_SIZE(npu_perf_mode_name)) {
			npu_err("Fail on npu_scheduler_init_info, number out of bounds in array=[%lu]\n", ARRAY_SIZE(npu_perf_mode_name));
			return -1;
		}
		info->mode = i;
	}
	npu_info("NPU mode : %s\n", npu_perf_mode_name[info->mode]);
	info->bts_scenindex = -1;
	info->llc_status = 0;

	info->time_stamp = now;
	info->time_diff = 0;
	info->freq_interval = 1;
	info->tfi = 0;
	info->boost_count = 0;

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-period", &info->period);
	if (ret)
		info->period = NPU_SCHEDULER_DEFAULT_PERIOD;

	npu_info("NPU period %d ms\n", info->period);

	/* initialize idle information */
	info->idle_load = 0;	/* 0.01 unit */

	/* initialize FPS information */
	info->fps_policy = NPU_SCHEDULER_FPS_MAX;
	mutex_init(&info->fps_lock);
	INIT_LIST_HEAD(&info->fps_frame_list);
	INIT_LIST_HEAD(&info->fps_load_list);
	info->fps_load = 0;	/* 0.01 unit */
	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-tpf-others", &info->tpf_others);
	if (ret)
		info->tpf_others = 0;

	/* initialize RQ information */
	mutex_init(&info->rq_lock);
	info->rq_start_time = now;
	info->rq_idle_start_time = now;
	info->rq_idle_time = 0;
	info->rq_load = 0;	/* 0.01 unit */

	/* initialize common information */
	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-load-policy", &info->load_policy);
	if (ret)
		info->load_policy = NPU_SCHEDULER_LOAD_FPS_RQ;
	/* load window : only for RQ or IDLE load */
	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-loadwin", &info->load_window_size);
	if (ret)
		info->load_window_size = NPU_SCHEDULER_DEFAULT_LOAD_WIN_SIZE;
	info->load_window_index = 0;
	for (i = 0; i < NPU_SCHEDULER_LOAD_WIN_MAX; i++)
		info->load_window[i] = 0;
	info->load = 0;		/* 0.01 unit */
	info->load_idle_time = 0;

	INIT_LIST_HEAD(&info->gov_list);
	INIT_LIST_HEAD(&info->ip_list);

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-dsp-type", &qos_setting->dsp_type);
	if (ret)
		qos_setting->dsp_type = 0;

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-dsp-max-freq", &qos_setting->dsp_max_freq);
	if (ret)
		qos_setting->dsp_max_freq = 0;

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-npu-max-freq", &qos_setting->npu_max_freq);
	if (ret)
		qos_setting->npu_max_freq = 0;

	/* de-activated scheduler */
	info->activated = 0;
	mutex_init(&info->exec_lock);
	wake_lock_init(&info->sched_wake_lock, WAKE_LOCK_SUSPEND,
			"npu-scheduler");
	INIT_DELAYED_WORK(&info->sched_work, npu_scheduler_work);
	INIT_DELAYED_WORK(&info->boost_off_work, npu_scheduler_boost_off_work);

	npu_info("scheduler info init done\n");

	return 0;
}

void npu_scheduler_gate(
		struct npu_device *device, struct npu_frame *frame, bool idle)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	tl = NULL;
	/* find load entry */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == frame->uid) {
			tl = l;
			break;
		}
	}
	/* if not, error !! */
	if (!tl) {
		npu_err("fps load data for uid %d NOT found\n", frame->uid);
		mutex_unlock(&info->fps_lock);
		return;
	}

	npu_trace("try to gate %s for UID %d\n", idle ? "on" : "off", frame->uid);
	if (idle) {
		if (tl->bound_id == NPU_BOUND_UNBOUND) {
			info->used_count--;
			if (!info->used_count) {
				/* gating activated */
				/* HWACG */
				//npu_hwacg(&device->system, true);
				/* clock OSC switch */
				ret = npu_core_clock_off(&device->system);
				if (ret)
					npu_err("fail(%d) in npu_core_clock_off\n", ret);
			}
		}
	} else {
		if (tl->bound_id == NPU_BOUND_UNBOUND) {
			if (!info->used_count) {
				/* gating deactivated */
				/* clock OSC switch */
				ret = npu_core_clock_on(&device->system);
				if (ret)
					npu_err("fail(%d) in npu_core_clock_on\n", ret);
				/* HWACG */
				//npu_hwacg(&device->system, false);
			}
			info->used_count++;
		}
	}
	mutex_unlock(&info->fps_lock);
}

void npu_scheduler_fps_update_idle(
		struct npu_device *device, struct npu_frame *frame, bool idle)
{
	s64 now, frame_time;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_frame *f;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	struct list_head *p;
	s64 new_init_freq, old_init_freq;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!device);
	info = device->sched;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return;
	}

	now = npu_get_time_us();

	npu_trace("uid %d, fid %d : %s\n",
			frame->uid, frame->frame_id, (idle?"done":"processing"));

	mutex_lock(&info->fps_lock);
	tl = NULL;
	/* find load entry */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == frame->uid) {
			tl = l;
			break;
		}
	}
	/* if not, error !! */
	if (!tl) {
		npu_err("fps load data for uid %d NOT found\n", frame->uid);
		mutex_unlock(&info->fps_lock);
		return;
	}

	if (idle) {
		list_for_each(p, &info->fps_frame_list) {
			f = list_entry(p, struct npu_scheduler_fps_frame, list);
			if (f->uid == frame->uid && f->frame_id == frame->frame_id) {
				tl->tfc--;
				/* all frames are processed, check for process time */
				if (!tl->tfc) {
					frame_time = now - f->start_time;
					tl->tpf = frame_time / tl->frame_count + info->tpf_others;

					/* get current freqency of NPU */
					list_for_each_entry(d, &info->ip_list, ip_list) {
						/* calculate the initial frequency */
						if (!strcmp("NPU", d->name)) {
							mutex_lock(&info->exec_lock);
							new_init_freq = tl->tpf * (s64)d->cur_freq / tl->requested_tpf;
							old_init_freq = (s64) tl->init_freq_ratio * (s64) d->max_freq / 10000;
							/* calculate exponential moving average */
							tl->init_freq_ratio = (old_init_freq / 2 + new_init_freq / 2) * 10000 / (s64) d->max_freq;
							if (tl->init_freq_ratio > 10000)
								tl->init_freq_ratio = 10000;
							mutex_unlock(&info->exec_lock);
							break;
						}
					}

					if (info->load_policy == NPU_SCHEDULER_LOAD_FPS ||
							info->load_policy == NPU_SCHEDULER_LOAD_FPS_RQ)
						info->freq_interval = tl->frame_count /
							NPU_SCHEDULER_FREQ_INTERVAL + 1;
					tl->frame_count = 0;

					tl->fps_load = tl->tpf * 10000 / tl->requested_tpf;
					tl->time_stamp = now;

					npu_trace("load (uid %d) (%lld)/%lld %lld updated\n",
							tl->uid, tl->tpf, tl->requested_tpf, tl->init_freq_ratio);
				}
				/* delete frame entry */
				list_del(p);
				if (tl->tfc)
					npu_trace("uid %d fid %d / %d frames left\n",
						tl->uid, f->frame_id, tl->tfc);
				else
					npu_trace("uid %d fid %d / %lld us per frame\n",
						tl->uid, f->frame_id, tl->tpf);
				if (f)
					kfree(f);
				break;
			}
		}
	} else {
		f = kzalloc(sizeof(struct npu_scheduler_fps_frame), GFP_KERNEL);
		if (!f) {
			npu_err("fail to alloc fps frame info (U%d, F%d)",
					frame->uid, frame->frame_id);
			mutex_unlock(&info->fps_lock);
			return;
		}
		f->uid = frame->uid;
		f->frame_id = frame->frame_id;
		f->start_time = now;
		list_add(&f->list, &info->fps_frame_list);

		/* add frame counts */
		tl->tfc++;
		tl->frame_count++;

		npu_trace("new frame (uid %d (%d frames active), fid %d) added\n",
				tl->uid, tl->tfc, f->frame_id);
	}
	mutex_unlock(&info->fps_lock);
}

static void npu_scheduler_calculate_fps_load(s64 now, struct npu_scheduler_info *info)
{
	unsigned int tmp_load, tmp_min_load, tmp_max_load, tmp_load_count;
	struct npu_scheduler_fps_load *l;

	tmp_load = 0;
	tmp_max_load = 0;
	tmp_min_load = 1000000;
	tmp_load_count = 0;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		/* reset FPS load in inactive status */
		if (info->time_stamp >
				l->time_stamp + l->tpf *
				NPU_SCHEDULER_FPS_LOAD_RESET_FRAME_NUM) {
			npu_trace("UID %d FPS load reset\n", l->uid);
			l->fps_load = 0;
		}

		tmp_load += l->fps_load;
		if (tmp_max_load < l->fps_load)
			tmp_max_load = l->fps_load;
		if (tmp_min_load > l->fps_load)
			tmp_min_load = l->fps_load;
		tmp_load_count++;
	}
	mutex_unlock(&info->fps_lock);

	switch (info->fps_policy) {
	case NPU_SCHEDULER_FPS_MIN:
		info->fps_load = tmp_min_load;
		break;
	case NPU_SCHEDULER_FPS_MAX:
		info->fps_load = tmp_max_load;
		break;
	case NPU_SCHEDULER_FPS_AVG2:	/* average without min, max */
		tmp_load -= tmp_min_load;
		tmp_load -= tmp_max_load;
		tmp_load_count -= 2;
		info->fps_load = tmp_load / tmp_load_count;
		break;
	case NPU_SCHEDULER_FPS_AVG:
		if (tmp_load_count > 0)
			info->fps_load = tmp_load / tmp_load_count;
		break;
	default:
		npu_err("Invalid FPS policy : %d\n", info->fps_policy);
		break;
	}
	//npu_dbg("FPS load : %d\n", info->fps_load);
}

void npu_scheduler_rq_update_idle(struct npu_device *device, bool idle)
{
	s64 now, idle_time;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	now = npu_get_time_us();

	mutex_lock(&info->rq_lock);

	if (idle) {
		info->rq_idle_start_time = now;
	} else {
		idle_time = now - info->rq_idle_start_time;
		info->rq_idle_time += idle_time;
		info->rq_idle_start_time = 0;
	}

	mutex_unlock(&info->rq_lock);
}

static void npu_scheduler_calculate_rq_load(s64 now, struct npu_scheduler_info *info)
{
	s64 total_diff;

	mutex_lock(&info->rq_lock);

	/* temperary finish idle time */
	if (info->rq_idle_start_time)
		info->rq_idle_time += (now - info->rq_idle_start_time);

	/* calculate load */
	total_diff = now - info->rq_start_time;
	info->rq_load = (total_diff - info->rq_idle_time) * 10000 / total_diff;
	//npu_dbg("RQ load : %d (idle %d, total %d)\n",
	//		(int)info->rq_load, (int)info->rq_idle_time, (int)total_diff);

	/* reset data */
	info->rq_start_time = now;
	if (info->rq_idle_start_time)
		/* restart idle timer */
		info->rq_idle_start_time = now;
	else
		info->rq_idle_start_time = 0;
	info->rq_idle_time = 0;

	mutex_unlock(&info->rq_lock);
}

static int npu_scheduler_check_limit(struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d)
{
	return 0;
}

static int npu_scheduler_set_freq(struct npu_scheduler_dvfs_info *d, s32 freq)
{
	//npu_dbg("set freq for %s : %d => %d\n", d->name, d->cur_freq, freq);
	if (d->cur_freq == freq) {
		//npu_dbg("stick to current freq : %d\n", d->cur_freq);
		return 0;
	}

	npu_pm_qos_update_request(d, &d->qos_req_min, freq);
	//npu_pm_qos_update_request(d, &d->qos_req_max, freq);

	return 0;
}

static void npu_scheduler_execute_policy(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;
	s32	freq = 0;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!d->activated) {
			npu_trace("%s deactivated\n", d->name);
			continue;
		}

		if (d->delay > 0)
			d->delay -= info->time_diff;

		/* check limitation */
		if (npu_scheduler_check_limit(info, d)) {
			/* emergency status */
		} else {
			/* no emergency status but delay */
			if (d->delay > 0) {
				npu_info("no update by delay %d\n", d->delay);
				continue;
			}
		}

		freq = d->cur_freq;
		/* calculate frequency */
		if (d->gov->ops->target)
			d->gov->ops->target(info, d, &freq);

		/* check mode limit */
		if (freq < d->mode_min_freq[info->mode])
			freq = d->mode_min_freq[info->mode];

		/* set frequency */
		if (info->tfi >= info->freq_interval)
			npu_scheduler_set_freq(d, freq);
	}
	mutex_unlock(&info->exec_lock);
	return;
}

static void npu_scheduler_set_mode_freq(struct npu_scheduler_info *info, int uid)
{
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	struct npu_scheduler_dvfs_info *d;
	s32	freq = 0;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return;
	}

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		freq = d->cur_freq;

		/* check mode limit */
		if (freq < d->mode_min_freq[info->mode])
			freq = d->mode_min_freq[info->mode];

		if (info->mode == NPU_PERF_MODE_NORMAL) {
			if (uid == -1) {
				/* requested through sysfs */
				freq = d->max_freq;
			} else {
				/* requested through ioctl() */
				tl = NULL;
				/* find load entry */
				list_for_each_entry(l, &info->fps_load_list, list) {
					if (l->uid == uid) {
						tl = l;
						break;
					}
				}
				/* if not, error !! */
				if (!tl) {
					npu_err("fps load data for uid %d NOT found\n", uid);
					freq = d->max_freq;
				} else {
					freq = tl->init_freq_ratio * d->max_freq / 10000;
					freq = npu_scheduler_get_freq_ceil(d, freq);
					if (freq < d->cur_freq)
						freq = d->cur_freq;
				}
			}
			d->is_init_freq = 1;
		}

		/* set frequency */
		npu_scheduler_set_freq(d, freq);
	}
	mutex_unlock(&info->exec_lock);
	return;
}

static unsigned int __npu_scheduler_get_load(
		struct npu_scheduler_info *info, u32 load)
{
	int i, load_sum = 0;

	if (info->load_window_index >= info->load_window_size)
		info->load_window_index = 0;

	info->load_window[info->load_window_index++] = load;

	for (i = 0; i < info->load_window_size; i++)
		load_sum += info->load_window[i];

	return (unsigned int)(load_sum / info->load_window_size);
}

int npu_scheduler_boost_on(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;

	npu_info("boost on (count %d)\n", info->boost_count + 1);
	if (likely(info->boost_count == 0)) {
		if (unlikely(list_empty(&info->ip_list))) {
			npu_err("no device for scheduler\n");
			return -EPERM;
		}

		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			npu_pm_qos_update_request(d, &d->qos_req_min_nw_boost, d->max_freq);
			npu_info("boost on freq for %s : %d\n", d->name, d->max_freq);
		}
		mutex_unlock(&info->exec_lock);
	}
	info->boost_count++;
	return 0;
}

static int __npu_scheduler_boost_off(struct npu_scheduler_info *info)
{
	int ret = 0;
	struct npu_scheduler_dvfs_info *d;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		ret = -EPERM;
		goto p_err;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		npu_pm_qos_update_request(d, &d->qos_req_min_nw_boost, d->min_freq);
		npu_info("boost off freq for %s : %d\n", d->name, d->min_freq);
	}
	mutex_unlock(&info->exec_lock);
	return ret;
p_err:
	return ret;
}

int npu_scheduler_boost_off(struct npu_scheduler_info *info)
{
	int ret = 0;

	info->boost_count--;
	npu_info("boost off (count %d)\n", info->boost_count);

	if (info->boost_count <= 0) {
		ret = __npu_scheduler_boost_off(info);
		info->boost_count = 0;
	} else if (info->boost_count > 0)
		queue_delayed_work(info->sched_wq, &info->boost_off_work,
				msecs_to_jiffies(NPU_SCHEDULER_BOOST_TIMEOUT));

	return ret;
}

int npu_scheduler_boost_off_timeout(struct npu_scheduler_info *info, s64 timeout)
{
	int ret = 0;

	if (timeout == 0) {
		npu_scheduler_boost_off(info);
	} else if (timeout > 0) {
		queue_delayed_work(info->sched_wq, &info->boost_off_work,
				msecs_to_jiffies(timeout));
	} else {
		npu_err("timeout cannot be less than 0\n");
		ret = -EPERM;
		goto p_err;
	}
	return ret;
p_err:
	return ret;
}

static void npu_scheduler_boost_off_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, boost_off_work.work);
	npu_scheduler_boost_off(info);
}

static void __npu_scheduler_work(struct npu_scheduler_info *info)
{
	s64 now;
	int is_last_idle = 0;
	u32 load, load_idle;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return;
	}

	now = npu_get_time_us();
	info->time_diff = now - info->time_stamp;
	info->time_stamp = now;

	/* get idle information */

	/* get FPS information */
	npu_scheduler_calculate_fps_load(now, info);

	/* get RQ information */
	npu_scheduler_calculate_rq_load(now, info);

	/* get global npu load */
	switch (info->load_policy) {
	case NPU_SCHEDULER_LOAD_IDLE:
		load = info->idle_load;
		break;
	case NPU_SCHEDULER_LOAD_FPS:
	case NPU_SCHEDULER_LOAD_FPS_RQ:
		load = info->fps_load;
		break;
	case NPU_SCHEDULER_LOAD_RQ:
		load = info->rq_load;
		break;
	default:
		load = 0;
		break;
	}

	info->load = __npu_scheduler_get_load(info, load);

	if (info->load_policy == NPU_SCHEDULER_LOAD_FPS_RQ)
		load_idle = info->rq_load;
	else
		load_idle = info->load;

	if (load_idle) {
		if (info->load_idle_time) {
			info->load_idle_time +=
				(info->time_diff * (10000 - load_idle) / 10000);
			is_last_idle = 1;
		} else
			info->load_idle_time = 0;
	} else
		info->load_idle_time += info->time_diff;

	info->tfi++;
	npu_trace("__npu scheduler work : tfi %d freq interval %d "
			"timestamp %llu (diff %llu), idle time %llu %s\n",
			info->tfi, info->freq_interval,
			info->time_stamp, info->time_diff, info->load_idle_time,
			is_last_idle ? "last" : "");

	/* decide frequency change */
	npu_scheduler_execute_policy(info);
	if (info->tfi >= info->freq_interval)
		info->tfi = 0;

	if (is_last_idle)
		info->load_idle_time = 0;
}

static void npu_scheduler_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, sched_work.work);

	__npu_scheduler_work(info);

	//npu_dbg("queue scheduler work after %dms\n", info->period);
	queue_delayed_work(info->sched_wq, &info->sched_work,
			msecs_to_jiffies(info->period));
}

static ssize_t npu_show_attrs_scheduler(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_scheduler(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

static struct device_attribute npu_scheduler_attrs[] = {
	NPU_SCHEDULER_ATTR(scheduler_enable),
	NPU_SCHEDULER_ATTR(scheduler_mode),

	NPU_SCHEDULER_ATTR(timestamp),
	NPU_SCHEDULER_ATTR(timediff),
	NPU_SCHEDULER_ATTR(period),

	NPU_SCHEDULER_ATTR(load_idle_time),
	NPU_SCHEDULER_ATTR(load),
	NPU_SCHEDULER_ATTR(load_policy),

	NPU_SCHEDULER_ATTR(idle_load),

	NPU_SCHEDULER_ATTR(fps_tpf),
	NPU_SCHEDULER_ATTR(fps_load),
	NPU_SCHEDULER_ATTR(fps_policy),
	NPU_SCHEDULER_ATTR(fps_all_load),
	NPU_SCHEDULER_ATTR(fps_target),

	NPU_SCHEDULER_ATTR(rq_load),
};

enum {
	NPU_SCHEDULER_ENABLE = 0,
	NPU_SCHEDULER_MODE,

	NPU_SCHEDULER_TIMESTAMP,
	NPU_SCHEDULER_TIMEDIFF,
	NPU_SCHEDULER_PERIOD,

	NPU_SCHEDULER_LOAD_IDLE_TIME,
	NPU_SCHEDULER_LOAD,
	NPU_SCHEDULER_LOAD_POLICY,

	NPU_SCHEDULER_IDLE_LOAD,

	NPU_SCHEDULER_FPS_TPF,
	NPU_SCHEDULER_FPS_LOAD,
	NPU_SCHEDULER_FPS_POLICY,
	NPU_SCHEDULER_FPS_ALL_LOAD,
	NPU_SCHEDULER_FPS_TARGET,

	NPU_SCHEDULER_RQ_LOAD,
};

ssize_t npu_show_attrs_scheduler(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - npu_scheduler_attrs;
	int i = 0, k;
	struct npu_scheduler_fps_load *l;

	BUG_ON(!g_npu_scheduler_info);

	switch (offset) {
	case NPU_SCHEDULER_ENABLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->enable);
		break;
	case NPU_SCHEDULER_MODE:
		for (k = 0; k < ARRAY_SIZE(npu_perf_mode_name); k++) {
			if (k == g_npu_scheduler_info->mode)
				i += scnprintf(buf + i, PAGE_SIZE - i, "*");
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, " ");
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %s\n",
				k, npu_perf_mode_name[k]);
		}
		break;

	case NPU_SCHEDULER_TIMESTAMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%llu\n",
			g_npu_scheduler_info->time_stamp);
		break;
	case NPU_SCHEDULER_TIMEDIFF:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%llu\n",
			g_npu_scheduler_info->time_diff);
		break;
	case NPU_SCHEDULER_PERIOD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->period);
		break;
	case NPU_SCHEDULER_LOAD_IDLE_TIME:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%llu\n",
			g_npu_scheduler_info->load_idle_time);
		break;
	case NPU_SCHEDULER_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->load);
		break;
	case NPU_SCHEDULER_LOAD_POLICY:
		for (k = 0; k < ARRAY_SIZE(npu_scheduler_load_policy_name); k++) {
			if (k == g_npu_scheduler_info->load_policy)
				i += scnprintf(buf + i, PAGE_SIZE - i, "*");
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, " ");
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %s\n",
				k, npu_scheduler_load_policy_name[k]);
		}
		break;

	case NPU_SCHEDULER_IDLE_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->idle_load);
		break;

	case NPU_SCHEDULER_FPS_TPF:
		i += scnprintf(buf + i, PAGE_SIZE - i, " uid\ttpf\tKPI\tinit\n");
		mutex_lock(&g_npu_scheduler_info->fps_lock);
		list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
			i += scnprintf(buf + i, PAGE_SIZE - i, " %d\t%lld\t%lld\t%lld\n",
			l->uid, l->tpf, l->requested_tpf, l->init_freq_ratio);
		}
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		break;
	case NPU_SCHEDULER_FPS_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->fps_load);
		break;
	case NPU_SCHEDULER_FPS_POLICY:
		for (k = 0; k < ARRAY_SIZE(npu_scheduler_fps_policy_name); k++) {
			if (k == g_npu_scheduler_info->fps_policy)
				i += scnprintf(buf + i, PAGE_SIZE - i, "*");
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, " ");
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %s\n",
				k, npu_scheduler_fps_policy_name[k]);
		}
		break;
	case NPU_SCHEDULER_FPS_ALL_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, " uid\tload\n");
		mutex_lock(&g_npu_scheduler_info->fps_lock);
		list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
			i += scnprintf(buf + i, PAGE_SIZE - i, " %d\t%d\n",
			l->uid, l->fps_load);
		}
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		break;
	case NPU_SCHEDULER_FPS_TARGET:
		i += scnprintf(buf + i, PAGE_SIZE - i, " uid\ttarget\n");
		mutex_lock(&g_npu_scheduler_info->fps_lock);
		list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
			i += scnprintf(buf + i, PAGE_SIZE - i, " %d\t%lld\n",
			l->uid, l->requested_tpf);
		}
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		break;

	case NPU_SCHEDULER_RQ_LOAD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			g_npu_scheduler_info->rq_load);
		break;

	default:
		break;
	}

	return i;
}

ssize_t npu_store_attrs_scheduler(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - npu_scheduler_attrs;
	int ret = 0;
	int x = 0;
	int y = 0;
	struct npu_scheduler_fps_load *l;

	BUG_ON(!g_npu_scheduler_info);

	switch (offset) {
	case NPU_SCHEDULER_ENABLE:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x)
				x = 1;
			g_npu_scheduler_info->enable = x;
		}
		break;
	case NPU_SCHEDULER_MODE:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x >= ARRAY_SIZE(npu_perf_mode_name)) {
				npu_err("Invalid mode value %d, ignored\n", x);
				ret = -EINVAL;
				break;
			}
			g_npu_scheduler_info->mode = x;
			if (g_npu_scheduler_info->activated)
				npu_scheduler_set_mode_freq(g_npu_scheduler_info, -1);
			npu_scheduler_set_bts(g_npu_scheduler_info);
			npu_set_llc(g_npu_scheduler_info);
		}
		break;

	case NPU_SCHEDULER_TIMESTAMP:
		break;
	case NPU_SCHEDULER_TIMEDIFF:
		break;
	case NPU_SCHEDULER_PERIOD:
		if (sscanf(buf, "%d", &x) > 0)
			g_npu_scheduler_info->period = x;
		break;
	case NPU_SCHEDULER_LOAD_IDLE_TIME:
		break;
	case NPU_SCHEDULER_LOAD:
		break;
	case NPU_SCHEDULER_LOAD_POLICY:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x >= ARRAY_SIZE(npu_scheduler_load_policy_name)) {
				npu_err("Invalid load policy : %d\n", x);
				ret = -EINVAL;
				break;
			}
			g_npu_scheduler_info->load_policy = x;
		}
		break;

	case NPU_SCHEDULER_IDLE_LOAD:
		break;

	case NPU_SCHEDULER_FPS_TPF:
		break;
	case NPU_SCHEDULER_FPS_LOAD:
		break;
	case NPU_SCHEDULER_FPS_POLICY:
		if (sscanf(buf, "%d", &x) > 0) {
			if (x >= ARRAY_SIZE(npu_scheduler_fps_policy_name)) {
				npu_err("Invalid FPS policy : %d\n", x);
				ret = -EINVAL;
				break;
			}
			g_npu_scheduler_info->fps_policy = x;
		}
		break;
	case NPU_SCHEDULER_FPS_ALL_LOAD:
		break;
	case NPU_SCHEDULER_FPS_TARGET:
		if (sscanf(buf, "%d %d", &x, &y) > 0) {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
				if (l->uid == x) {
					l->requested_tpf = y;
					break;
				}
			}
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	case NPU_SCHEDULER_RQ_LOAD:
		break;

	default:
		break;
	}

	if (!ret)
		ret = count;
	return ret;
}

int npu_scheduler_probe(struct npu_device *device)
{
	int ret = 0;
	s64 now;
	struct npu_scheduler_info *info;

	BUG_ON(!device);

	info = kzalloc(sizeof(struct npu_scheduler_info), GFP_KERNEL);
	if (!info) {
		probe_err("failed to alloc info\n");
		ret = -ENOMEM;
		goto err_info;
	}
	memset(info, 0, sizeof(struct npu_scheduler_info));
	device->sched = info;
	device->system.qos_setting.info = info;
	info->device = device;
	info->dev = device->dev;

	now = npu_get_time_us();

	/* init scheduler data */
	ret = npu_scheduler_init_info(now, info);
	if (ret) {
		probe_err("fail(%d) init info\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	ret = npu_scheduler_create_attrs(info->dev,
			npu_scheduler_attrs, ARRAY_SIZE(npu_scheduler_attrs));
	if (ret) {
		probe_err("fail(%d) create attributes\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* register governors */
	ret = npu_scheduler_governor_register(info);
	if (ret) {
		probe_err("fail(%d) register governor\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* init scheduler with dt */
	ret = npu_scheduler_init_dt(info);
	if (ret) {
		probe_err("fail(%d) initial setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	info->sched_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!info->sched_wq) {
		probe_err("fail to create workqueue\n");
		ret = -EFAULT;
		goto err_info;
	}

	g_npu_scheduler_info = info;
	probe_info("done\n");
	return ret;
err_info:
	if (info)
		kfree(info);
	g_npu_scheduler_info = NULL;
	return ret;
}

int npu_scheduler_release(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	ret = npu_scheduler_governor_unregister(info);
	if (ret) {
		probe_err("fail(%d) unregister governor\n", ret);
		ret = -EFAULT;
	}
	g_npu_scheduler_info = NULL;

	return ret;
}

int npu_scheduler_load(struct npu_device *device, const struct npu_session *session)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	/* create load data for session */
	l = kzalloc(sizeof(struct npu_scheduler_fps_load), GFP_KERNEL);
	if (!l) {
		npu_err("failed to alloc fps_load\n");
		ret = -ENOMEM;
		mutex_unlock(&info->fps_lock);
		return ret;
	}
	l->uid = session->uid;
	l->priority = session->sched_param.priority;
	l->bound_id = session->sched_param.bound_id;
	l->tfc = 0;		/* temperary frame count */
	l->frame_count = 0;
	l->tpf = 0;		/* time per frame */
	l->requested_tpf = NPU_SCHEDULER_DEFAULT_TPF;
	l->init_freq_ratio = 10000; /* 100%, max frequency */
	list_add(&l->list, &info->fps_load_list);

	npu_info("load for uid %d (p %d b %d) added\n",
			l->uid, l->priority, l->bound_id);

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		npu_info("DVFS start : %s (%s)\n",
				d->name, d->gov->name);
		d->gov->ops->start(d);

		/* set frequency */
		npu_scheduler_set_freq(d, d->max_freq);
		d->is_init_freq = 1;
		d->activated = 1;
	}
	mutex_unlock(&info->exec_lock);
	mutex_unlock(&info->fps_lock);
	return ret;
}

void npu_scheduler_unload(struct npu_device *device, const struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	/* delete load data for session */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			list_del(&l->list);
			if (l)
				kfree(l);
			npu_info("load for uid %d deleted\n", session->uid);
			break;
		}
	}

	if (list_empty(&info->fps_load_list)) {
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			npu_info("DVFS stop : %s (%s)\n",
					d->name, d->gov->name);
			d->activated = 0;
			d->gov->ops->stop(d);
			d->is_init_freq = 0;
		}
		mutex_unlock(&info->exec_lock);
	}
	mutex_unlock(&info->fps_lock);

	npu_dbg("done\n");
}

void npu_scheduler_update_sched_param(struct npu_device *device, struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	/* delete load data for session */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			l->priority = session->sched_param.priority;
			l->bound_id = session->sched_param.bound_id;
			npu_info("update sched param for uid %d (p %d b %d)\n",
				l->uid, l->priority, l->bound_id);
			break;
		}
	}
	mutex_unlock(&info->fps_lock);
}

void npu_scheduler_set_init_freq(
		struct npu_device *device, npu_uid_t session_uid)
{
	s32 init_freq;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;

	BUG_ON(!device);
	info = device->sched;

	mutex_lock(&info->fps_lock);
	tl = NULL;
	/* find load entry */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session_uid) {
			tl = l;
			break;
		}
	}

	/* if not, error !! */
	if (!tl) {
		npu_err("fps load data for uid %d NOT found\n", session_uid);
		mutex_unlock(&info->fps_lock);
		return;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (d->is_init_freq == 0) {
			init_freq = tl->init_freq_ratio * d->max_freq / 10000;
			init_freq = npu_scheduler_get_freq_ceil(d, init_freq);

			if (init_freq < d->cur_freq)
				init_freq = d->cur_freq;

			/* set frequency */
			npu_scheduler_set_freq(d, init_freq);
			d->is_init_freq = 1;
		}
	}
	mutex_unlock(&info->exec_lock);
	mutex_unlock(&info->fps_lock);
}

int npu_scheduler_open(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	/* activate scheduler */
	info->activated = 1;

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
	wake_lock_timeout(&info->sched_wake_lock, msecs_to_jiffies(100));
	queue_delayed_work(info->sched_wq, &info->sched_work,
			msecs_to_jiffies(100));
#endif
	npu_info("done\n");
	return ret;
}

int npu_scheduler_close(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	info->load_window_index = 0;
	for (i = 0; i < NPU_SCHEDULER_LOAD_WIN_MAX; i++)
		info->load_window[i] = 0;
	info->load = 0;
	info->load_idle_time = 0;
	info->idle_load = 0;
	info->fps_load = 0;
	info->rq_load = 0;
	info->tfi = 0;

	/* de-activate scheduler */
	info->activated = 0;

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
	cancel_delayed_work_sync(&info->sched_work);
#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;

	npu_info("done\n");
	return ret;
}

int npu_scheduler_resume(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	/* re-schedule work */
	if (info->activated) {
		cancel_delayed_work_sync(&info->sched_work);
		wake_lock_timeout(&info->sched_wake_lock, msecs_to_jiffies(100));
		queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));
	}

	npu_info("done\n");
	return ret;
}

int npu_scheduler_suspend(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

	if (info->activated)
		cancel_delayed_work_sync(&info->sched_work);

	npu_info("done\n");
	return ret;
}

int npu_scheduler_start(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

#ifdef CONFIG_NPU_SCHEDULER_START_STOP
	wake_lock_timeout(&info->sched_wake_lock, msecs_to_jiffies(100));
	queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));
#endif
	npu_info("done\n");
	return ret;
}

int npu_scheduler_stop(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_scheduler_info *info;

	BUG_ON(!device);
	info = device->sched;

#ifdef CONFIG_NPU_SCHEDULER_START_STOP
	info->load_window_index = 0;
	for (i = 0; i < NPU_SCHEDULER_LOAD_WIN_MAX; i++)
		info->load_window[i] = 0;
	info->load = 0;
	info->load_idle_time = 0;
	info->idle_load = 0;
	info->fps_load = 0;
	info->rq_load = 0;
	info->tfi = 0;

	cancel_delayed_work_sync(&info->sched_work);

#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;

	npu_info("done\n");
	return ret;
}

npu_s_param_ret npu_scheduler_param_handler(struct npu_session *sess, struct vs4l_param *param, int *retval)
{
	int found = 0;
	struct npu_scheduler_fps_load *l;
	npu_s_param_ret ret = S_PARAM_HANDLED;

	BUG_ON(!sess);
	BUG_ON(!param);

	mutex_lock(&g_npu_scheduler_info->fps_lock);
	list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
		if (l->uid == sess->uid) {
			found = 1;
			break;
		}
	}
	if (!found) {
		npu_err("UID %d NOT found\n", sess->uid);
		ret = S_PARAM_NOMB;
		mutex_unlock(&g_npu_scheduler_info->fps_lock);
		return ret;
	}

	switch (param->target) {
	case NPU_S_PARAM_PERF_MODE:
		if (param->offset < NPU_PERF_MODE_NUM) {
			g_npu_scheduler_info->mode = param->offset;
			if (g_npu_scheduler_info->activated)
				npu_scheduler_set_mode_freq(g_npu_scheduler_info, sess->uid);
			npu_scheduler_set_bts(g_npu_scheduler_info);
			npu_set_llc(g_npu_scheduler_info);
			npu_dbg("new NPU performance mode : %s\n",
					npu_perf_mode_name[g_npu_scheduler_info->mode]);
		} else {
			npu_err("Invalid NPU performance mode : %d\n",
					param->offset);
			ret = S_PARAM_NOMB;
		}
		break;

	case NPU_S_PARAM_PRIORITY:
		if (param->offset > NPU_SCHEDULER_PRIORITY_MAX) {
			npu_err("Invalid priority : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		} else {
			l->priority = param->offset;

			/* TODO: hand over priority info to session */

			npu_dbg("set priority of uid %d as %d\n",
					l->uid, l->priority);
		}
		break;

	case NPU_S_PARAM_TPF:
		if (param->offset == 0) {
			npu_err("No TPF setting : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		}
		else {
			l->requested_tpf = param->offset;

			npu_dbg("set tpf of uid %d as %lld\n",
					l->uid, l->requested_tpf);
		}
		break;

	default:
		ret = S_PARAM_NOMB;
		break;
	}
	mutex_unlock(&g_npu_scheduler_info->fps_lock);

	return ret;
}
