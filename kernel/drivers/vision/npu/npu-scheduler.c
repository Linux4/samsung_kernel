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

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/printk.h>
#include <linux/cpuidle.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos-devfreq.h>
#endif

#include "npu-config.h"
#include "npu-scheduler-governor.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-dvfs.h"
#include "npu-bts.h"
#include "npu-dtm.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"

static struct npu_scheduler_info *g_npu_scheduler_info;
static struct npu_scheduler_control *npu_sched_ctl_ref;
#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
const struct npu_scheduler_utilization_ops s_utilization_ops = {
	.dev = NULL,
	.utilization_ops = &n_utilization_ops,
};
#endif

static void npu_scheduler_work(struct work_struct *work);
static void npu_scheduler_boost_off_work(struct work_struct *work);
#if IS_ENABLED(CONFIG_NPU_USE_LLC_PRESET)
static void npu_scheduler_set_llc(struct npu_session *sess, u32 size);
#endif

struct npu_scheduler_info *npu_scheduler_get_info(void)
{
	BUG_ON(!g_npu_scheduler_info);
	return g_npu_scheduler_info;
}

#define NPU_HWACG_MAX_MODE	0x03
void npu_scheduler_hwacg_mode_to_dhcp(u32 hwacg)
{
	struct npu_system *system = &g_npu_scheduler_info->device->system;

	npu_info("hwacg mode is  %u\n", hwacg);
	if (hwacg < NPU_HWACG_MAX_MODE) {
		npu_info("set %u to NPUMEM_MODE\n", hwacg);
		dsp_dhcp_write_reg_idx(system->dhcp, DSP_DHCP_NPUMEM_MODE, hwacg);
	}
}

static void npu_scheduler_hwacg_set(u32 hw, u32 hwacg)
{
	int ret = 0;
	struct npu_hw_device *hdev;

	npu_info("start HWACG setting[%u], value : %u\n", hw, hwacg);

	npu_scheduler_hwacg_mode_to_dhcp(hwacg);

	if (hw == HWACG_NPU) {
		hdev = npu_get_hdev("NPU");
		if (atomic_read(&hdev->boot_cnt.refcount)) {
			if (hwacg == 0x01) {
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgdisnpu");
			} else if (hwacg == 0x02) {
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "acgdisnpum");
				if (ret)
					goto done;
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgenlh");
			} else if (hwacg == NPU_SCH_DEFAULT_VALUE) {
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgennpu");
			}
		}
	} else if (hw == HWACG_DSP) {
		hdev = npu_get_hdev("DSP");
		if (atomic_read(&hdev->boot_cnt.refcount)) {
			if (hwacg == 0x01)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgdisdsp");
			else if (hwacg == 0x02)
				goto done;
			else if (hwacg == NPU_SCH_DEFAULT_VALUE)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgendsp");
		}
	} else if (hw == HWACG_DNC) {
		hdev = npu_get_hdev("DNC");
		if (atomic_read(&hdev->boot_cnt.refcount)) {
			if (hwacg == 0x01)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgdisdnc");
			else if (hwacg == 0x02)
				goto done;
			else if (hwacg == NPU_SCH_DEFAULT_VALUE)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgendnc");
		}
	}

done:
	if (ret)
		npu_err("fail(%d) in npu_cmd_map for hwacg\n", ret);
}

static void npu_scheduler_set_dsp_type(
	__attribute__((unused))struct npu_scheduler_info *info,
	__attribute__((unused))struct npu_qos_setting *qos_setting)
{
}


/* Call-back from Protodrv */
static int npu_scheduler_save_result(struct npu_session *dummy, struct nw_result result)
{
	BUG_ON(!npu_sched_ctl_ref);
	npu_trace("scheduler request completed : result = %u\n", result.result_code);

	npu_sched_ctl_ref->result_code = result.result_code;
	atomic_set(&npu_sched_ctl_ref->result_available, 1);

	wake_up(&npu_sched_ctl_ref->wq);
	return 0;
}

static void npu_scheduler_set_policy(struct npu_scheduler_info *info,
							struct npu_nw *nw)
{
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
#if !IS_ENABLED(CONFIG_NPU_USE_LLC_PRESET)
	if (g_npu_scheduler_info->mode == NPU_PERF_MODE_NPU_BOOST ||
		g_npu_scheduler_info->mode == NPU_PERF_MODE_NPU_BOOST_BLOCKING ||
		g_npu_scheduler_info->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE) {
		nw->param0 = g_npu_scheduler_info->mode;
	}
#else
	nw->param0 = info->mode;
	nw->param1 = ((info->llc_mode >> 24) & 0xFF) << 19;
#endif
#else
	nw->param0 = info->mode;
#endif
	npu_info("llc_mode(%u)\n", nw->param0);
}


/* Send mode info to FW and check its response */
static int npu_scheduler_send_mode_to_hw(struct npu_session *session,
					struct npu_scheduler_info *info)
{
	int ret;
	struct npu_nw nw;
	int retry_cnt;
	struct npu_vertex_ctx *vctx = &session->vctx;

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		info->wait_hw_boot_flag = 1;
		npu_info("HW power off state: %d %d\n", info->mode, info->llc_ways);
		return 0;
	}

	memset(&nw, 0, sizeof(nw));
	nw.cmd = NPU_NW_CMD_MODE;
	nw.uid = session->uid;

	/* Set callback function on completion */
	nw.notify_func = npu_scheduler_save_result;
	/* Set LLC policy for FW */
	npu_scheduler_set_policy(info, &nw);

	if ((info->prev_mode == info->mode) && (info->llc_status)) {
		npu_dbg("same mode and llc\n");
		return 0;
	}

	retry_cnt = 0;
	atomic_set(&npu_sched_ctl_ref->result_available, 0);
	while ((ret = npu_ncp_mgmt_put(&nw)) <= 0) {
		npu_info("queue full when inserting scheduler control message. Retrying...");
		if (retry_cnt++ >= NPU_SCHEDULER_CMD_POST_RETRY_CNT) {
			npu_err("timeout exceeded.\n");
			ret = -EWOULDBLOCK;
			goto err_exit;
		}
		msleep(NPU_SCHEDULER_CMD_POST_RETRY_INTERVAL);
	}
	/* Success */
	npu_info("scheduler control message has posted\n");

	ret = wait_event_timeout(
		npu_sched_ctl_ref->wq,
		atomic_read(&npu_sched_ctl_ref->result_available),
		NPU_SCHEDULER_HW_RESP_TIMEOUT);
	if (ret < 0) {
		npu_err("wait_event_timeout error(%d)\n", ret);
		goto err_exit;
	}
	if (!atomic_read(&npu_sched_ctl_ref->result_available)) {
		npu_err("timeout waiting H/W response\n");
		ret = -ETIMEDOUT;
		goto err_exit;
	}
	if (npu_sched_ctl_ref->result_code != NPU_ERR_NO_ERROR) {
		npu_err("hardware reply with NDONE(%d)\n", npu_sched_ctl_ref->result_code);
		ret = -EFAULT;
		goto err_exit;
	}
	ret = 0;

err_exit:
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
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
		npu_dvfs_set_freq(d, &d->qos_req_min, d->min_freq);
	}
	mutex_unlock(&info->exec_lock);

	npu_info("done\n");
	return 1;
}
#endif

static int npu_scheduler_init_dt(struct npu_scheduler_info *info)
{
	int i, count, ret = 0;
	unsigned long f = 0;
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	struct dev_pm_opp *opp;
#endif
	char *tmp_name;
	struct npu_scheduler_dvfs_info *dinfo;
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
			probe_err("failed to read dvfs name %d from %s node : %d\n",
					i, info->dev->of_node->name, ret);
			goto err_dinfo;
		}
		/* get governor name  */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i + 1,
				(const char **)&tmp_name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node : %d\n",
					i + 1, info->dev->of_node->name, ret);
			goto err_dinfo;
		}

		probe_info("set up %s with %s governor\n", dinfo->name, tmp_name);
		/* get and set governor */

		dinfo->gov = npu_scheduler_governor_get(info, tmp_name);
		/* need to add error check for dinfo->gov */

		/* get dvfs and pm-qos info */
		ret = of_parse_phandle_with_fixed_args(info->dev->of_node,
				"samsung,npusched-dvfs",
				NPU_SCHEDULER_DVFS_TOTAL_ARG_NUM, i / 2, &pa);
		if (ret) {
			probe_err("failed to read dvfs args %d from %s node : %d\n",
					i / 2, info->dev->of_node->name, ret);
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
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
		opp = dev_pm_opp_find_freq_floor(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid max freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->max_freq = f;
			dev_pm_opp_put(opp);
		}
#else
		dinfo->max_freq = f;
#endif
		f = 0;
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
		opp = dev_pm_opp_find_freq_ceil(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid min freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->min_freq = f;
			dev_pm_opp_put(opp);
		}
#else
		dinfo->min_freq = f;
#endif
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_min,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_min),
				dinfo->min_freq);
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max),
				dinfo->max_freq);
#endif
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_min_dvfs_cmd,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_min_dvfs_cmd),
				dinfo->min_freq);
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_dvfs_cmd,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_dvfs_cmd),
				dinfo->max_freq);
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_min_nw_boost,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request for boosting %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_min_nw_boost),
				dinfo->min_freq);
#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION) && IS_ENABLED(CONFIG_SOC_S5E9945)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_cam_noti,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request for camera %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_cam_noti),
				dinfo->max_freq);
#endif
#if IS_ENABLED(CONFIG_NPU_AFM)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_afm,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request for afm %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_afm),
				dinfo->max_freq);
#endif
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_precision,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request for precision %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_precision),
				dinfo->max_freq);

		probe_info("%s %d %d %d %d %d %d\n", dinfo->name,
				pa.args[0], pa.args[1], pa.args[2],
				pa.args[3], pa.args[4], pa.args[5]);

		/* reset values */
		dinfo->cur_freq = dinfo->min_freq;
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
		dinfo->delay = 0;
		dinfo->limit_min = 0;
		dinfo->limit_max = INT_MAX;
		dinfo->curr_up_delay = 0;
		dinfo->curr_down_delay = 0;

		/* get governor property slot */
		if (dinfo->gov) {
			dinfo->gov_prop = (void *)dinfo->gov->ops->init_prop(dinfo, info->dev, pa.args);
			if (!dinfo->gov_prop) {
				probe_err("failed to init governor property for %s\n",
						dinfo->name);
				goto err_dinfo;
			}

			/* add device in governor */
			list_add_tail(&dinfo->dev_list, &dinfo->gov->dev_list);
		}
#endif
#endif
		/* add device in scheduler */
		list_add_tail(&dinfo->ip_list, &info->ip_list);

		probe_info("add %s in list\n", dinfo->name);
	}

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM) || IS_ENABLED(CONFIG_NPU_USE_IFD)
#if IS_ENABLED(CONFIG_SOC_S5E9935) || IS_ENABLED(CONFIG_SOC_S5E9945) || IS_ENABLED(CONFIG_SOC_S5E8845)
	ret = of_property_read_u32_array(info->dev->of_node, "samsung,npudvfs-table-num", info->dvfs_table_num, 2);
	if (ret) {
		probe_err("failed to get npudvfs-table-num (%d)\n", ret);
		ret = -EINVAL;
		goto err_dinfo;
	}
	probe_info("DVFS table num < %d %d >", info->dvfs_table_num[0], info->dvfs_table_num[1]);

	info->dvfs_table = (u32 *)devm_kzalloc(info->dev, sizeof(u32) * info->dvfs_table_num[0] * info->dvfs_table_num[1], GFP_KERNEL);
	ret = of_property_read_u32_array(info->dev->of_node, "samsung,npudvfs-table", (u32 *)info->dvfs_table,
			info->dvfs_table_num[0] * info->dvfs_table_num[1]);
	if (ret) {
		probe_err("failed to get npudvfs-table (%d)\n", ret);
		ret = -EINVAL;
		goto err_dinfo;
	}

	for (i = 0; i < info->dvfs_table_num[0] * info->dvfs_table_num[1]; i++) {
			probe_info("DVFS table[%d][%d] < %d >", i / info->dvfs_table_num[1], i % info->dvfs_table_num[1], info->dvfs_table[i]);
	}
#else
	list_for_each_entry(dinfo, &info->ip_list, ip_list) {
		if (!strcmp(dinfo->name, "NPU0")) {
			u32 dvfs_NPU_freq[30];
			u32 dvfs_NPU_freq_cnt = 1;

			dvfs_NPU_freq[0] = dinfo->max_freq;
			f = dinfo->max_freq;

#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
			while (1) {
				f = (f > 1) ? f - 1 : 0;
				opp = dev_pm_opp_find_freq_floor(&dinfo->dvfs_dev->dev, &f);
				if (IS_ERR(opp)) {
					probe_err("invalid freq for %s\n", dinfo->name);
					ret = -EINVAL;
					goto err_dinfo;
				} else {
					dvfs_NPU_freq[dvfs_NPU_freq_cnt] = f;
					dev_pm_opp_put(opp);
				}
				dvfs_NPU_freq_cnt++;
				if (f == dinfo->min_freq)
					break;
			}
#endif
			info->dvfs_table_num[0] = dvfs_NPU_freq_cnt;
			info->dvfs_table_num[1] = 1;
			info->dvfs_table = (u32 *)devm_kzalloc(info->dev, sizeof(u32) * info->dvfs_table_num[0], GFP_KERNEL);

			for (i = 0; i < info->dvfs_table_num[0]; i++) {
				info->dvfs_table[i] = dvfs_NPU_freq[i];
				probe_info("DVFS table[%d] < %d >", i, info->dvfs_table[i]);
			}
		}
	}
#endif
#endif
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	info->pid_max_clk = (info->pid_max_clk < info->dvfs_table[0]) ? info->dvfs_table[0] : info->pid_max_clk;
	{
		u32 t_dtm_param[7];

		ret = of_property_read_u32_array(info->dev->of_node, "samsung,npudtm-param", t_dtm_param, 7);
		if (ret) {
			probe_err("failed to get npudtm-param (%d)\n", ret);
			ret = -EINVAL;
			goto err_dinfo;
		}
		info->pid_target_thermal = t_dtm_param[0];
		info->pid_p_gain = t_dtm_param[1];
		info->pid_i_gain = t_dtm_param[2];
		info->pid_inv_gain = t_dtm_param[3];
		info->pid_period = t_dtm_param[4];
		info->dtm_nm_lut[0] = t_dtm_param[5];
		info->dtm_nm_lut[1] = t_dtm_param[6];
	}
#endif
	return ret;
err_dinfo:
	devm_kfree(info->dev, dinfo);
err_exit:
	return ret;
}

u32 npu_get_perf_mode(void)
{
	struct npu_scheduler_info *info = npu_scheduler_get_info();
	return info->mode;
}

static int npu_scheduler_init_info(s64 now, struct npu_scheduler_info *info)
{
	int i, ret = 0;
	const char *mode_name;
	struct npu_qos_setting *qos_setting;

	probe_info("scheduler info init\n");
	qos_setting = &(info->device->system.qos_setting);

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	info->enable = 0;
	atomic_set(&info->dvfs_queue_cnt, 0);
	atomic_set(&info->dvfs_unlock_activated, 0);
#else
	info->enable = 1;	/* default enable */
#endif
	ret = of_property_read_string(info->dev->of_node,
			"samsung,npusched-mode", &mode_name);
	if (ret)
		info->mode = NPU_PERF_MODE_NORMAL;
	else {
		for (i = 0; i < ARRAY_SIZE(npu_perf_mode_name); i++) {
			if (!strcmp(npu_perf_mode_name[i], mode_name))
				break;
		}
		if (i == ARRAY_SIZE(npu_perf_mode_name)) {
			probe_err("Fail on %s, number out of bounds in array=[%lu]\n", __func__,
									ARRAY_SIZE(npu_perf_mode_name));
			return -1;
		}
		info->mode = i;
	}

	for (i = 0; i < NPU_PERF_MODE_NUM; i++)
		info->mode_ref_cnt[i] = 0;

	probe_info("NPU mode : %s\n", npu_perf_mode_name[info->mode]);
	info->bts_scenindex = -1;
	info->llc_status = 0;
	info->llc_ways = 0;
	info->hwacg_status = HWACG_STATUS_ENABLE;

	info->time_stamp = now;
	info->time_diff = 0;
	info->freq_interval = 1;
	info->tfi = 0;
	info->boost_count = 0;
	info->boost_flag = 0;
	info->dd_direct_path = 0;
	info->wait_hw_boot_flag = 0;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	info->pid_en = 0;
	info->debug_log_en = 0;
	info->curr_thermal = 0;
	info->dtm_prev_freq = -1;
#endif

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-period", &info->period);
	if (ret)
		info->period = NPU_SCHEDULER_DEFAULT_PERIOD;

	probe_info("NPU period %d ms\n", info->period);

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
		info->load_policy = NPU_SCHEDULER_LOAD_FPS;
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
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	INIT_LIST_HEAD(&info->dsi_list);
#endif
	npu_scheduler_set_dsp_type(info, qos_setting);

	/* de-activated scheduler */
	info->activated = 0;
	mutex_init(&info->exec_lock);
	mutex_init(&info->param_handle_lock);
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	mutex_init(&info->dvfs_info_lock);
#endif
	info->is_dvfs_cmd = false;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_init(info->dev, &info->sws,
				NPU_WAKE_LOCK_SUSPEND, "npu-scheduler");
#endif

	memset((void *)&info->sched_ctl, 0, sizeof(struct npu_scheduler_control));
	npu_sched_ctl_ref = &info->sched_ctl;

	init_waitqueue_head(&info->sched_ctl.wq);

	INIT_DELAYED_WORK(&info->sched_work, npu_scheduler_work);
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	INIT_DELAYED_WORK(&info->dvfs_work, npu_dvfs_unset_freq_work);
#endif
	INIT_DELAYED_WORK(&info->boost_off_work, npu_scheduler_boost_off_work);

	probe_info("scheduler info init done\n");

	return 0;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void npu_scheduler_gate(
		struct npu_device *device, struct npu_frame *frame, bool idle)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	u32 bid = NPU_BOUND_CORE0;

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
	} else {
		bid = tl->bound_id;
		mutex_unlock(&info->fps_lock);
	}

	npu_trace("try to gate %s for UID %d\n", idle ? "on" : "off", frame->uid);
	if (idle) {
		if (bid == NPU_BOUND_UNBOUND) {
			info->used_count--;
			if (!info->used_count) {
				/* gating activated */
				/* HWACG */
				/* clock OSC switch */
				ret = npu_core_clock_off(&device->system);
				if (ret)
					npu_err("fail(%d) in npu_core_clock_off\n", ret);
			}
		}
	} else {
		if (bid == NPU_BOUND_UNBOUND) {
			if (!info->used_count) {
				/* gating deactivated */
				/* clock OSC switch */
				ret = npu_core_clock_on(&device->system);
				if (ret)
					npu_err("fail(%d) in npu_core_clock_on\n", ret);
			}
			info->used_count++;
		}
	}
}
#endif

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
/*
 * FIXME: BY: should consider separate dvfs
 */
void npu_scheduler_fps_update_idle(
		struct npu_device *device, struct npu_frame *frame, bool idle)
{
	s64 now, frame_time;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_frame *f;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	struct list_head *p;
	u64 new_init_freq, old_init_freq, cur_freq;
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
					frame->output->timestamp[4].tv_usec = frame_time;

					/* get current freqency of NPU */
					list_for_each_entry(d, &info->ip_list, ip_list) {
						/* calculate the initial frequency */
						if (!strcmp("NPU0", d->name)) {
							mutex_lock(&info->exec_lock);
							cur_freq = npu_dvfs_devfreq_get_domain_freq(d);
							new_init_freq = tl->tpf * cur_freq / tl->requested_tpf;
							old_init_freq = tl->init_freq_ratio * d->max_freq / 10000;
							/* calculate exponential moving average */
							tl->init_freq_ratio = (old_init_freq / 2 + new_init_freq / 2) * 10000 / d->max_freq;
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
					{
						long long tmp_fps_load;

						tmp_fps_load = (long long)tl->tpf * 10000 / tl->requested_tpf;
						if (tmp_fps_load > UINT_MAX)
							tl->fps_load = UINT_MAX;
						else
							tl->fps_load = (unsigned int)tmp_fps_load;
					}
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

		tl->time_stamp = 0;
		if (tl->fps_load <= 0 && tl->old_fps_load > 0) {
			tl->fps_load = tl->old_fps_load;
			tl->old_fps_load = 0;
		}

		npu_trace("new frame (uid %d (%d frames active), fid %d) added\n",
				tl->uid, tl->tfc, f->frame_id);
	}
	mutex_unlock(&info->fps_lock);
}

static void npu_scheduler_calculate_fps_load(s64 now, struct npu_scheduler_info *info)
{
	unsigned int tmp_load, tmp_min_load, tmp_max_load, tmp_load_count;
	struct npu_scheduler_fps_load *l;
	unsigned int	*fps_load;	/* 0.01 unit */
	unsigned int sys_load, sys_min_load, sys_max_load, sys_load_count;
	struct npu_system *system = &info->device->system;
	struct npu_hw_device *hdev;
	int hi;

	sys_load = 0;
	sys_max_load = 0;
	sys_min_load = 1000000;
	sys_load_count = 0;
	for (hi = 0; hi < system->hwdev_num; hi++) {
		hdev = system->hwdev_list[hi];
		if (!hdev)
			continue;

		fps_load = &hdev->fps_load;

		tmp_load = 0;
		tmp_max_load = 0;
		tmp_min_load = 1000000;
		tmp_load_count = 0;

		mutex_lock(&info->fps_lock);
		list_for_each_entry(l, &info->fps_load_list, list) {
			if (!(hdev->id & NPU_HWDEV_ID_DNC) &&
					!(hdev->id & l->session->hids))
				continue;

			/* reset FPS load in inactive status */
			if (l->time_stamp && info->time_stamp >
					l->time_stamp + l->tpf *
					NPU_SCHEDULER_FPS_LOAD_RESET_FRAME_NUM) {
				if (l->fps_load)
					l->old_fps_load = l->fps_load;
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
			*fps_load = tmp_min_load;
			break;
		case NPU_SCHEDULER_FPS_MAX:
			*fps_load = tmp_max_load;
			break;
		case NPU_SCHEDULER_FPS_AVG2:	/* average without min, max */
			tmp_load -= tmp_min_load;
			tmp_load -= tmp_max_load;
			tmp_load_count -= 2;
			*fps_load = tmp_load / tmp_load_count;
			break;
		case NPU_SCHEDULER_FPS_AVG:
			if (tmp_load_count > 0)
				*fps_load = tmp_load / tmp_load_count;
			break;
		default:
			npu_err("Invalid FPS policy : %d\n", info->fps_policy);
			break;
		}

		if (*fps_load) {
			sys_load += *fps_load;
			if (sys_max_load < *fps_load)
				sys_max_load = *fps_load;
			if (sys_min_load > *fps_load)
				sys_min_load = *fps_load;
			sys_load_count++;
		}
	}
	switch (info->fps_policy) {
	case NPU_SCHEDULER_FPS_MIN:
		info->fps_load = sys_min_load;
		break;
	case NPU_SCHEDULER_FPS_MAX:
		info->fps_load = sys_max_load;
		break;
	case NPU_SCHEDULER_FPS_AVG2:	/* average without min, max */
		sys_load -= sys_min_load;
		sys_load -= sys_max_load;
		sys_load_count -= 2;
		info->fps_load = sys_load / sys_load_count;
		break;
	case NPU_SCHEDULER_FPS_AVG:
		if (sys_load_count > 0)
			info->fps_load = sys_load / sys_load_count;
		break;
	default:
		npu_err("Invalid FPS policy : %d\n", info->fps_policy);
		break;
	}
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
#endif

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
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

		info->load = 0;
		{
			int hi;
			struct npu_hw_device *hdev;
			struct npu_system *system = &info->device->system;

			for (hi = 0; hi < system->hwdev_num; hi++) {
				hdev = system->hwdev_list[hi];
				if (!strcmp(d->name, hdev->name)) {
					info->load = hdev->fps_load;
					break;
				}
			}
		}

		/* calculate frequency */
		if (d->gov && d->gov->ops->target)
			d->gov->ops->target(info, d, &freq);

		/* check mode limit */
		if (freq < d->mode_min_freq[info->mode])
			freq = d->mode_min_freq[info->mode];

		/* set frequency */
		if (info->tfi >= info->freq_interval)
			npu_dvfs_set_freq(d, NULL, freq);
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
#endif

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
#if IS_ENABLED(CONFIG_NPU_USE_IFD) || IS_ENABLED(CONFIG_NPU_GOVERNOR)
#if IS_ENABLED(CONFIG_SOC_S5E9935) || IS_ENABLED(CONFIG_SOC_S5E9945)
			if (!strcmp("DNC", d->name)) {
#else
			if (!strcmp("NPU0", d->name)) {
#endif
				if (!test_bit(NPU_DEVICE_STATE_OPEN, &info->device->state))
					continue;

				npu_dvfs_set_freq_boost(d, &d->qos_req_min_nw_boost, d->max_freq);
				npu_info("boost on freq for %s : %d\n", d->name, d->max_freq);
			}
#else
			/* no actual governor or no name */
			if (!d->gov || !strcmp(d->gov->name, ""))
				continue;

			if ((strcmp(d->name, "NPU0") == 0) || (strcmp(d->name, "DNC") == 0)) {
				if (!test_bit(NPU_DEVICE_STATE_OPEN, &info->device->state))
					continue;

				npu_dvfs_set_freq(d, &d->qos_req_min_nw_boost, d->max_freq);
				npu_info("boost on freq for %s : %d\n", d->name, d->max_freq);
			}
#endif
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
#if IS_ENABLED(CONFIG_NPU_USE_IFD) || IS_ENABLED(CONFIG_NPU_GOVERNOR)
#if IS_ENABLED(CONFIG_SOC_S5E9935) || IS_ENABLED(CONFIG_SOC_S5E9945)
		if (!strcmp("DNC", d->name)) {
#else
		if (!strcmp("NPU0", d->name)) {
#endif
			if (!test_bit(NPU_DEVICE_STATE_OPEN, &info->device->state))
				continue;

			npu_dvfs_set_freq(d, &d->qos_req_min_nw_boost, d->min_freq);
			npu_info("boost off freq for %s : %d\n", d->name, d->min_freq);
		}
#else
		/* no actual governor or no name */
		if (!d->gov || !strcmp(d->gov->name, ""))
			continue;

		if ((strcmp(d->name, "NPU0") == 0) || (strcmp(d->name, "DNC") == 0)) {
			if (!test_bit(NPU_DEVICE_STATE_OPEN, &info->device->state))
				continue;

			npu_dvfs_set_freq(d, &d->qos_req_min_nw_boost, d->min_freq);
			npu_info("boost off freq for %s : %d\n", d->name, d->min_freq);
		}
#endif
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

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
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

	/* decide frequency change */
	npu_scheduler_execute_policy(info);
	if (info->tfi >= info->freq_interval)
		info->tfi = 0;

	if (is_last_idle)
		info->load_idle_time = 0;
}
#endif

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM) || IS_ENABLED(CONFIG_NPU_USE_IFD)
int npu_scheduler_get_lut_idx(struct npu_scheduler_info *info, int clk, dvfs_ip_type ip_type)
{
	int lut_idx;

	if (ip_type >= info->dvfs_table_num[1])
		ip_type = DIT_CORE;

	for (lut_idx = 0; lut_idx < info->dvfs_table_num[0]; lut_idx++) {
		if (clk >= info->dvfs_table[lut_idx * info->dvfs_table_num[1] + ip_type])
			return lut_idx;
	}
	return lut_idx;
}
int npu_scheduler_get_clk(struct npu_scheduler_info *info, int lut_idx, dvfs_ip_type ip_type)
{
	if (lut_idx < 0)
		lut_idx = 0;
	else if (lut_idx > (info->dvfs_table_num[0] - 1))
		lut_idx = info->dvfs_table_num[0] -1;

	if (ip_type >= info->dvfs_table_num[1])
		ip_type = DIT_CORE;

	return info->dvfs_table[lut_idx * info->dvfs_table_num[1] + ip_type];
}
#endif

static void npu_scheduler_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, sched_work.work);

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	__npu_scheduler_work(info);
#endif
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	npu_dtm_set(info);
#endif

	queue_delayed_work(info->sched_wq, &info->sched_work,
			msecs_to_jiffies(info->period));
}

static ssize_t npu_scheduler_debugfs_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int i, ret = 0;
	char buf[30];
	ssize_t size;
	int x;

	for (i = 0; i < ARRAY_SIZE(npu_scheduler_debugfs_name); i++) {
		if (!strcmp(npu_scheduler_debugfs_name[i], filp->f_path.dentry->d_iname))
			break;
	}
	if (i == ARRAY_SIZE(npu_perf_mode_name)) {
		probe_err("Fail on %s, number out of bounds in array=[%lu]\n", __func__,
								ARRAY_SIZE(npu_perf_mode_name));
		return -1;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d", &x);
	if (ret != 1) {
		npu_err("Failed to get period parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}
	probe_info("input params is  %u\n", x);

	switch(i)
	{
#if !IS_ENABLED(CONFIG_NPU_USE_IFD)
	case NPU_SCHEDULER_PERIOD:
		g_npu_scheduler_info->period = (u32)x;
	break;
#endif

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	case NPU_SCHEDULER_PID_TARGET_THERMAL:
		g_npu_scheduler_info->pid_target_thermal = (u32)x;
		break;

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	case NPU_SCHEDULER_PID_P_GAIN:
		g_npu_scheduler_info->pid_p_gain = x;
		break;

	case NPU_SCHEDULER_PID_I_GAIN:
		g_npu_scheduler_info->pid_i_gain = x;
		break;

	case NPU_SCHEDULER_PID_D_GAIN:
		g_npu_scheduler_info->pid_inv_gain = x;
		break;

	case NPU_SCHEDULER_PID_PERIOD:
		g_npu_scheduler_info->pid_period = x;
		break;
#endif
#endif

#if IS_ENABLED(CONFIG_NPU_IMB_THRESHOLD)
	case NPU_IMB_THRESHOLD_MB:
		if (x > 1024 || x < 128) {
			npu_err("Invalid IMB Threshold size : %d\n", x);
			ret = -EINVAL;
			break;
		}
		npu_put_configs(NPU_IMB_THRESHOLD_SIZE, (((unsigned int)x) << 20));
		break;
#endif

	default:
		break;
	}

p_err:
	return ret;
}

static int npu_scheduler_debugfs_show(struct seq_file *file, void *unused)
{
	int i;
#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
	int j;
#endif

	for (i = 0; i < ARRAY_SIZE(npu_scheduler_debugfs_name); i++) {
		if (!strcmp(npu_scheduler_debugfs_name[i], file->file->f_path.dentry->d_iname))
			break;
	}
	if (i == ARRAY_SIZE(npu_perf_mode_name)) {
		probe_err("Fail on %s, number out of bounds in array=[%lu]\n", __func__,
								ARRAY_SIZE(npu_perf_mode_name));
		return -1;
	}

	switch(i)
	{
#if !IS_ENABLED(CONFIG_NPU_USE_IFD)
	case NPU_SCHEDULER_PERIOD:
		seq_printf(file, "%d\n", g_npu_scheduler_info->period);
	break;
#endif

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	case NPU_SCHEDULER_PID_TARGET_THERMAL:
		seq_printf(file, "%u\n", g_npu_scheduler_info->pid_target_thermal);
		break;

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	case NPU_SCHEDULER_PID_P_GAIN:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_p_gain);
		break;

	case NPU_SCHEDULER_PID_I_GAIN:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_i_gain);
		break;

	case NPU_SCHEDULER_PID_D_GAIN:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_inv_gain);
		break;

	case NPU_SCHEDULER_PID_PERIOD:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_period);
		break;

	case NPU_SCHEDULER_DEBUG_LOG:
	{
		int freq_avr = 0;
		int t;

		if (g_npu_scheduler_info->debug_dump_cnt == 0) {
			seq_printf(file, "[idx/total] 98C_cnt freq_avr [idx thermal T_freq freq_avr]\n");
			for (t = 0; t < g_npu_scheduler_info->debug_cnt; t++)
				freq_avr += npu_scheduler_get_clk(g_npu_scheduler_info,
							npu_scheduler_get_lut_idx(g_npu_scheduler_info,
								g_npu_scheduler_info->debug_log[t][2], 0), 0);

			freq_avr /= g_npu_scheduler_info->debug_cnt;
		}

		if (g_npu_scheduler_info->debug_dump_cnt + 20 < g_npu_scheduler_info->debug_cnt) {
			for (t = g_npu_scheduler_info->debug_dump_cnt;
					 t < g_npu_scheduler_info->debug_dump_cnt + 20; t++)
				seq_printf(file, "[%d/%d] %d [%d %d %d %d]\n",
						t,
						g_npu_scheduler_info->debug_cnt,
						freq_avr,
						(int)g_npu_scheduler_info->debug_log[t][0],
						(int)g_npu_scheduler_info->debug_log[t][1],
						(int)g_npu_scheduler_info->debug_log[t][2],
						(int)npu_scheduler_get_clk(g_npu_scheduler_info,
						npu_scheduler_get_lut_idx(g_npu_scheduler_info,
						g_npu_scheduler_info->debug_log[t][2], 0), 0));

			g_npu_scheduler_info->debug_dump_cnt += 20;
		}
	}
#else
	case NPU_SCHEDULER_DEBUG_LOG:
#endif
		g_npu_scheduler_info->debug_log_en = 1;
		break;

#endif

#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
	case NPU_SCHEDULER_CPU_UTILIZATION:
		if (g_npu_scheduler_info->activated)
			seq_printf(file, "%u\n", s_utilization_ops.utilization_ops->get_s_cpu_utilization());
		break;

	case NPU_SCHEDULER_DSP_UTILIZATION:
		if (g_npu_scheduler_info->activated)
			seq_printf(file, "%u\n", s_utilization_ops.utilization_ops->get_s_dsp_utilization());
		break;

	case NPU_SCHEDULER_NPU_UTILIZATION:
		if (g_npu_scheduler_info->activated) {
			for (j = 0; j < CONFIG_NPU_NUM_CORES; j++)
				seq_printf(file, "%u ", s_utilization_ops.utilization_ops->get_s_npu_utilization(j));
			seq_printf(file, "\n");
		}
		break;
#endif

#if IS_ENABLED(CONFIG_NPU_IMB_THRESHOLD)
	case NPU_IMB_THRESHOLD_MB:
		seq_printf(file, "%u\n", (npu_get_configs(NPU_IMB_THRESHOLD_SIZE) >> 20));
		break;
#endif

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	case NPU_DVFS_INFO:
	{
		struct npu_scheduler_dvfs_sess_info *dvfs_info;
		int dvfs_info_cnt = 0;
		int t_cnt = atomic_read( &g_npu_scheduler_info->dvfs_queue_cnt);

		seq_printf(file, "IDX: [hid buf_size] [freq] [tpf load] [run_time idle_time] [NPU_OP/DSP_OP] Q[%d]\n", t_cnt);

		list_for_each_entry(dvfs_info, &g_npu_scheduler_info->dsi_list, list) {
			if (dvfs_info->is_linked) {
				seq_printf(file, "%d: [%s %s %d] [%d %d] [%d %llu] [%d %d] [%d/%d]\n"
					, dvfs_info_cnt++
					, (dvfs_info->hids == NPU_HWDEV_ID_NPU) ? "NPU" : "DSP"
					, (dvfs_info->is_nm_mode) ? "NM" : "BM"
					, (int)dvfs_info->buf_size
					, dvfs_info->req_freq
					, dvfs_info->prev_freq
					, dvfs_info->tpf
					, dvfs_info->load
					, (u32)dvfs_info->last_run_usecs
					, (u32)(dvfs_info->last_idle_msecs*1000)
					, (dvfs_info->hids == NPU_HWDEV_ID_NPU) ? 1 : 0
					, (dvfs_info->hids == NPU_HWDEV_ID_DSP || dvfs_info->is_unified) ? 1 : 0);
			}
		}
	}
	break;

	case NPU_DVFS_INFO_LIST:
	{
		struct npu_scheduler_dvfs_sess_info *dvfs_info;
		int dvfs_info_cnt = 0;
		int t_cnt = atomic_read( &g_npu_scheduler_info->dvfs_queue_cnt);

		seq_printf(file, "IDX: <Linked> [hid buf_size] [freq] [tpf load] [run_time idle_time] [NPU_OP/DSP_OP] Q[%d]\n", t_cnt);
		list_for_each_entry(dvfs_info, &g_npu_scheduler_info->dsi_list, list) {
			seq_printf(file, "%d: <%d> [%s %s %d] [%d %d] [%d %llu] [%d %d] [%d/%d]\n"
				, dvfs_info_cnt++
				, dvfs_info->is_linked
				, (dvfs_info->hids == NPU_HWDEV_ID_NPU) ? "NPU" : "DSP"
				, (dvfs_info->is_nm_mode) ? "NM" : "BM"
				, (int)dvfs_info->buf_size
				, dvfs_info->req_freq
				, dvfs_info->prev_freq
				, dvfs_info->tpf
				, dvfs_info->load
				, (u32)dvfs_info->last_run_usecs
				, (u32)(dvfs_info->last_idle_msecs*1000)
				, (dvfs_info->hids == NPU_HWDEV_ID_NPU) ? 1 : 0
				, (dvfs_info->hids == NPU_HWDEV_ID_DSP || dvfs_info->is_unified) ? 1 : 0);
		}
	}
	break;
#endif

	default:
		break;
	}

	return 0;
}

static int npu_scheduler_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_scheduler_debugfs_show, inode->i_private);
}

const struct file_operations npu_scheduler_debugfs_fops = {
	.open           = npu_scheduler_debugfs_open,
	.read           = seq_read,
	.write		= npu_scheduler_debugfs_write,
	.llseek         = seq_lseek,
	.release        = single_release
};

int npu_scheduler_probe(struct npu_device *device)
{
	int i, ret = 0;
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

	for(i=0; i< ARRAY_SIZE(npu_scheduler_debugfs_name); i++) {
		ret = npu_debug_register(npu_scheduler_debugfs_name[i], &npu_scheduler_debugfs_fops);
		if (ret) {
			probe_err("loading npu_debug : debugfs for dvfs_info can not be created(%d)\n", ret);
			goto err_info;
		}
	}

	/* register governors */
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	ret = npu_scheduler_governor_register(info);
	if (ret) {
		probe_err("fail(%d) register governor\n", ret);
		ret = -EFAULT;
		goto err_info;
	}
#endif
	/* init scheduler with dt */
	ret = npu_scheduler_init_dt(info);
	if (ret) {
		probe_err("fail(%d) initial setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* init dvfs command list with dt */
	ret = npu_dvfs_init_cmd_list(&device->system, info);
	if (ret) {
		probe_err("fail(%d) initial dvfs command setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	info->sched_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!info->sched_wq) {
		probe_err("fail to create workqueue\n");
		ret = -EFAULT;
		goto err_info;
	}
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	info->dvfs_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!info->dvfs_wq) {
		probe_err("fail to create workqueue -> dvfs_wq\n");
		ret = -EFAULT;
		goto err_info;
	}
#endif
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	/* Get NPU Thermal zone */
	info->npu_tzd = thermal_zone_get_zone_by_name("NPU");
#endif
	g_npu_scheduler_info = info;

	ufs_perf_add_boost_mode_request(&info->rq);
	atomic_set(&info->ufs_boost, 0x0);
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

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	ret = npu_scheduler_governor_unregister(info);
	if (ret) {
		probe_err("fail(%d) unregister governor\n", ret);
		ret = -EFAULT;
	}
#endif
	g_npu_scheduler_info = NULL;

	return ret;
}

int npu_scheduler_register_session(const struct npu_session *session)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;

	info = g_npu_scheduler_info;

	mutex_lock(&info->fps_lock);
	/* create load data for session */
	l = kzalloc(sizeof(struct npu_scheduler_fps_load), GFP_KERNEL);
	if (!l) {
		npu_err("failed to alloc fps_load\n");
		ret = -ENOMEM;
		mutex_unlock(&info->fps_lock);
		return ret;
	}
	l->session = session;
	l->uid = session->uid;
	l->priority = session->sched_param.priority;
	l->bound_id = session->sched_param.bound_id;
	l->frame_count = 0;
	l->tfc = 0;		/* temperary frame count */
	l->tpf = NPU_SCHEDULER_DEFAULT_TPF;	/* time per frame */
	l->requested_tpf = NPU_SCHEDULER_DEFAULT_REQUESTED_TPF;
	l->old_fps_load = 10000;	/* save previous fps load */
	l->fps_load = 10000;		/* 0.01 unit */
	l->init_freq_ratio = 10000;	/* 100%, max frequency */
	l->mode = NPU_PERF_MODE_NORMAL;
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	l->dvfs_unset_time = 0;
#endif
	l->time_stamp = npu_get_time_us();
	list_add(&l->list, &info->fps_load_list);

	npu_info("load for uid %d (p %d b %d) added\n",
			l->uid, l->priority, l->bound_id);
	mutex_unlock(&info->fps_lock);

	return ret;
}

void npu_scheduler_unregister_session(const struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;

	info = g_npu_scheduler_info;

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	mutex_lock(&info->dvfs_info_lock);
	if (session->dvfs_info)
		session->dvfs_info->is_linked = FALSE;
	mutex_unlock(&info->dvfs_info_lock);
#endif

	mutex_lock(&info->fps_lock);
	/* delete load data for session */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			list_del(&l->list);
			kfree(l);
			npu_info("load for uid %d deleted\n", session->uid);
			break;
		}
	}

	mutex_unlock(&info->fps_lock);
}

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
int npu_scheduler_load(struct npu_device *device, const struct npu_session *session)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;

	WARN_ON(!device);
	info = device->sched;

	/* session for only settings */
	if (!(session->hids) || session->hids == NPU_HWDEV_ID_MISC)
		return ret;

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		/* no actual governor or no name */
		if (!d->gov || !strcmp(d->gov->name, ""))
			continue;

		npu_info("DVFS start : %s (%s)\n",
				d->name, d->gov ? d->gov->name : "");
		if (d->gov)
			d->gov->ops->start(d);

		/* set frequency */
		npu_dvfs_set_freq(d, NULL, d->max_freq);
		d->is_init_freq = 1;
		d->activated = 1;
	}
	mutex_unlock(&info->exec_lock);

	return ret;
}

void npu_scheduler_unload(struct npu_device *device, const struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_dvfs_info *d;
	u32 models = 0;

	WARN_ON(!device);
	info = device->sched;

	/* session for only settings */
	if (!(session->hids) || session->hids == NPU_HWDEV_ID_MISC)
		return;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		/* count normal models */
		if (l->session->hids || l->session->hids != NPU_HWDEV_ID_MISC)
			models++;
	}
	/* if last model */
	if (models <= 1) {
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			npu_info("DVFS stop : %s (%s)\n",
					d->name, d->gov ? d->gov->name : "");
			d->activated = 0;
			if (d->gov)
				d->gov->ops->stop(d);
			d->is_init_freq = 0;
		}
		mutex_unlock(&info->exec_lock);
	}
	mutex_unlock(&info->fps_lock);

	npu_dbg("done\n");
}
#endif

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
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
#endif

int npu_scheduler_open(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	int i;
#endif

	BUG_ON(!device);
	info = device->sched;

	if (info->llc_status) {
		info->mode = NPU_PERF_MODE_NORMAL;
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
		npu_set_llc(info);
#endif
	}

	/* activate scheduler */
	info->activated = 1;
	info->is_dvfs_cmd = false;
	info->llc_status = 0;
	info->llc_ways = 0;
	info->dd_direct_path = 0;
	info->prev_mode = -1;
	info->cpuidle_cnt = 0;
	info->DD_log_cnt = 0;
	info->wait_hw_boot_flag = 0;
	info->enable = 1;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	info->debug_cnt = 0;
	info->debug_dump_cnt = 0;
#endif
	info->dtm_curr_freq = info->pid_max_clk;
	for (i = 0; i < PID_I_BUF_SIZE; i++)
		info->th_err_db[i] = 0;
#endif
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	atomic_set(&info->dvfs_queue_cnt, 0);
#endif

	/* set dvfs command list for default mode */
	npu_dvfs_cmd_map(info, "open");

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
	queue_delayed_work(info->sched_wq, &info->sched_work,
			msecs_to_jiffies(100));

#endif

	return ret;
}

int npu_scheduler_close(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;

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
	info->llc_ways = 0;
	info->dd_direct_path = 0;
	info->wait_hw_boot_flag = 0;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	info->pid_en = 0;
#endif

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
	cancel_delayed_work_sync(&info->sched_work);
#endif
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	if (atomic_read( &info->dvfs_unlock_activated)) {
		cancel_delayed_work_sync(&info->dvfs_work);
	}

	if (atomic_read(&info->dvfs_queue_cnt) != 0) {
		npu_dbg("Error : dvfs_queue_cnt -> %d\n", atomic_read(&info->dvfs_queue_cnt));
		atomic_set(&info->dvfs_queue_cnt, 0);
	}
	mutex_lock(&info->exec_lock);
	npu_dvfs_unset_freq(info);
	mutex_unlock(&info->exec_lock);

#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;
	info->is_dvfs_cmd = false;

	/* set dvfs command list for default mode */
	npu_dvfs_cmd_map(info, "close");

	list_for_each_entry(d, &info->ip_list, ip_list) {
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
		if (!d->activated)
			continue;
#endif
		/* set frequency */
		npu_dvfs_set_freq(d, NULL, 0);
	}
	npu_scheduler_system_param_unset();

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
#if IS_ENABLED(CONFIG_PM_SLEEP)
		npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
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
#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
	queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));
#endif

	return ret;
}

int npu_scheduler_stop(struct npu_device *device)
{
	int ret = 0;
#ifdef CONFIG_NPU_SCHEDULER_START_STOP
	int i;
#endif
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

	return ret;
}

static u32 calc_next_mode(struct npu_scheduler_info *info, u32 req_mode, u32 prev_mode, npu_uid_t uid)
{
	int i;
	u32 ret;
	u32 sess_prev_mode = 0;
	int found = 0;
	struct npu_scheduler_fps_load *l;

	ret = NPU_PERF_MODE_NORMAL;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == uid) {
			found = 1;
			/* read previous mode of a session and update next mode */
			sess_prev_mode = l->mode;
			l->mode = req_mode;
			break;
		}
	}
	mutex_unlock(&info->fps_lock);

	if (unlikely(!found))
		return ret;

	/* update reference count for each mode */
	if (req_mode == NPU_PERF_MODE_NORMAL) {
		if (info->mode_ref_cnt[sess_prev_mode] > 0)
			info->mode_ref_cnt[sess_prev_mode]--;
	} else {
		info->mode_ref_cnt[req_mode]++;
	}

	/* calculate next mode */
	for (i = 0; i < NPU_PERF_MODE_NUM; i++)
		if (info->mode_ref_cnt[i] > 0)
			ret = i;

	return ret;
}

void npu_scheduler_system_param_unset(void)
{
	mutex_lock(&g_npu_scheduler_info->param_handle_lock);
	if (g_npu_scheduler_info->cpuidle_cnt) {
		g_npu_scheduler_info->cpuidle_cnt = 0;
		cpuidle_resume_and_unlock();
		npu_info("cpuidle_resume\n");
	}
	g_npu_scheduler_info->DD_log_cnt = 0;
	if (g_npu_scheduler_info->dd_log_level_preset && console_printk[0] == 0) {
		npu_info("DD_log turn on\n");
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
		if (g_npu_scheduler_info->debug_log_en == 0)
#endif
			console_printk[0] = g_npu_scheduler_info->dd_log_level_preset;
	}

	if (g_npu_scheduler_info->bts_scenindex > 0) {
		int ret = npu_bts_del_scenario(g_npu_scheduler_info->bts_scenindex);
		if (ret)
			npu_err("npu_bts_del_scenario failed : %d\n", ret);
	}

	mutex_unlock(&g_npu_scheduler_info->param_handle_lock);
}

static void npu_scheduler_set_DD_log(u32 val)
{
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	if (g_npu_scheduler_info->debug_log_en != 0)
		return;
#endif
	npu_info("preset_DD_log turn on/off : %u - %u - %u\n",
		val, g_npu_scheduler_info->dd_log_level_preset, g_npu_scheduler_info->DD_log_cnt);

	if (val == 0x01)
		npu_log_set_loglevel(val, MEMLOG_LEVEL_ERR);
	else
		npu_log_set_loglevel(0, MEMLOG_LEVEL_CAUTION);

}

#if IS_ENABLED(CONFIG_NPU_USE_LLC_PRESET)
static void npu_scheduler_set_llc(struct npu_session *sess, u32 size)
{
	int ways = 0;
	struct npu_vertex_ctx *vctx = &sess->vctx;

	if (size == NPU_SCH_DEFAULT_VALUE)
		size = 0;
	else if (size > 512*LLC_MAX_WAYS)
		size = 512*LLC_MAX_WAYS;

	ways = (size * K_SIZE) / npu_get_configs(NPU_LLC_CHUNK_SIZE);
	sess->llc_ways = ways;

	npu_dbg("last ways[%u], new ways[%u] req_size[%u]\n",
			g_npu_scheduler_info->llc_ways, ways, size);

#if IS_ENABLED(CONFIG_NPU_USE_LLC)
	g_npu_scheduler_info->llc_ways = ways;

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		g_npu_scheduler_info->wait_hw_boot_flag = 1;
		npu_info("set_llc - HW power off state: %d %d\n",
			g_npu_scheduler_info->mode, g_npu_scheduler_info->llc_ways);
		return;
	}

	npu_set_llc(g_npu_scheduler_info);
	npu_scheduler_send_mode_to_hw(sess, g_npu_scheduler_info);
#endif
}
#endif

void npu_scheduler_send_wait_info_to_hw(struct npu_session *session,
					struct npu_scheduler_info *info)
{
	int ret;

	if (info->wait_hw_boot_flag) {
		info->wait_hw_boot_flag = 0;
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
		npu_set_llc(info);
#endif
		ret = npu_scheduler_send_mode_to_hw(session, info);
		if (ret < 0)
			npu_err("send_wait_info_to_hw error %d\n", ret);
	}
}

void npu_scheduler_param_handler_dump(void)
{
	struct npu_scheduler_fps_load *l;

	if (!g_npu_scheduler_info)
		return;

	npu_dump("------------------ Param LLC Size --------------\n");
	npu_dump("npu scheduler LLC mode = 0x%x\n", g_npu_scheduler_info->llc_mode);
	npu_dump("npu scheduler LLC status = %d\n", g_npu_scheduler_info->llc_status);
	npu_dump("npu scheduler LLC ways = %d\n", g_npu_scheduler_info->llc_ways);
	npu_dump("------------------------------------------------\n");

	npu_dump("------------------ Param PERF Mode -------------\n");
	npu_dump("npu scheduler Perfomance Mode = %s\n",
		 npu_perf_mode_name[g_npu_scheduler_info->mode]);
	npu_dump("------------------------------------------------\n");

	npu_dump("------------------ Param FPS load --------------\n");
	mutex_lock(&g_npu_scheduler_info->fps_lock);
	list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
		npu_dump("npu scheduler fps, session id = %d\n", l->session->uid);
		npu_dump("npu scheduler fps, priority = %d\n", l->priority);
		npu_dump("npu scheduler fps, bound id = %d\n", l->bound_id);
		npu_dump("npu scheduler fps, time per frame = %lld\n", l->tpf);
		npu_dump("npu scheduler fps, requested tpf = %lld\n", l->requested_tpf);
		npu_dump("npu scheduler fps, old fps load = %d\n", l->old_fps_load);
		npu_dump("npu scheduler fps, fps load = %d\n", l->fps_load);
		npu_dump("npu scheduler fps, fps mode = %s\n", npu_perf_mode_name[l->mode]);
	}
	mutex_unlock(&g_npu_scheduler_info->fps_lock);
	npu_dump("------------------------------------------------\n");

	npu_dump("------------------ Param MO Scenario -----------\n");
	npu_dump("npu scheduler BTS index = %d\n", g_npu_scheduler_info->bts_scenindex);
	npu_dump("------------------------------------------------\n");

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	npu_dump("------------------ Param DTM EN/DIS ------------\n");
	npu_dump("npu scheduler DTM en/dis = %s\n", g_npu_scheduler_info->pid_en ? "Enable" : "Disable");
	// npu_dump("npu scheduler DTM target thermal = %d\n",
	// 	 g_npu_scheduler_info->pid_target_thermal);
	// npu_dump("npu scheduler DTM max clk = %d\n",
	// 	 g_npu_scheduler_info->pid_max_clk);
	npu_dump("------------------------------------------------\n");
#endif

	npu_dump("------------------ Param DVFS EN/DIS -----------\n");
	npu_dump("npu scheduler DVFS en/dis = %s\n", g_npu_scheduler_info->enable ? "Enable" : "Disable");
	npu_dump("------------------------------------------------\n");
}

static void npu_scheduler_ufs_boost(bool boost)
{
	if (boost) {
		if (atomic_inc_return(&g_npu_scheduler_info->ufs_boost) == 0x1)
			ufs_perf_request_boost_mode(&g_npu_scheduler_info->rq, REQUEST_BOOST);
	} else { /* !boost */
		if (atomic_dec_return(&g_npu_scheduler_info->ufs_boost) == 0x0)
			ufs_perf_request_boost_mode(&g_npu_scheduler_info->rq, REQUEST_NORMAL);
	}
	npu_dbg("ufs boost ref count : %u\n", atomic_read(&g_npu_scheduler_info->ufs_boost));
}

npu_s_param_ret npu_scheduler_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	int found = 0;
	struct npu_scheduler_fps_load *l;
	npu_s_param_ret ret = S_PARAM_HANDLED;
	u32 req_mode, prev_mode, next_mode;

	BUG_ON(!sess);
	BUG_ON(!param);

	if (!g_npu_scheduler_info)
		return S_PARAM_NOMB;

	mutex_lock(&g_npu_scheduler_info->fps_lock);
	list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
		if (l->uid == sess->uid) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&g_npu_scheduler_info->fps_lock);
	if (!found) {
		npu_err("UID %d NOT found\n", sess->uid);
		ret = S_PARAM_NOMB;
		return ret;
	}

	mutex_lock(&g_npu_scheduler_info->param_handle_lock);
	switch (param->target) {
	case NPU_S_PARAM_LLC_SIZE:
	case NPU_S_PARAM_LLC_SIZE_PRESET:
#if IS_ENABLED(CONFIG_NPU_USE_LLC_PRESET)
		if (g_npu_scheduler_info->activated) {
			npu_dbg("S_PARAM_LLC - size : %dKB\n", param->offset);
			npu_scheduler_set_llc(sess, param->offset);
		}
#endif
		break;

	case NPU_S_PARAM_PERF_MODE:
		if (param->offset < NPU_PERF_MODE_NUM) {
			req_mode = param->offset;
			prev_mode = g_npu_scheduler_info->mode;
			g_npu_scheduler_info->prev_mode = prev_mode;
			next_mode = calc_next_mode(g_npu_scheduler_info, req_mode, prev_mode, sess->uid);
			g_npu_scheduler_info->mode = next_mode;

			npu_dbg("req_mode:%u, prev_mode:%u, next_mode:%u\n",
					req_mode, prev_mode, next_mode);

			if (g_npu_scheduler_info->activated && req_mode == next_mode) {
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
				if (sess->dvfs_info)
					sess->dvfs_info->is_nm_mode = (next_mode == NPU_PERF_MODE_NORMAL) ? TRUE : FALSE;
#endif

#if IS_ENABLED(CONFIG_NPU_USE_LLC)
#if !IS_ENABLED(CONFIG_NPU_USE_LLC_PRESET)
				npu_set_llc(g_npu_scheduler_info);
#else
				npu_scheduler_set_llc(sess, npu_kpi_llc_size(g_npu_scheduler_info));
#endif
#endif

#if !IS_ENABLED(CONFIG_NPU_USE_LLC_PRESET)
				npu_scheduler_send_mode_to_hw(sess, g_npu_scheduler_info);
#endif
				npu_dbg("new NPU performance mode : %s\n",
						npu_perf_mode_name[g_npu_scheduler_info->mode]);
			}
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
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			l->priority = param->offset;

			/* TODO: hand over priority info to session */

			npu_dbg("set priority of uid %d as %d\n",
					l->uid, l->priority);
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	case NPU_S_PARAM_TPF:
		if (param->offset == 0) {
			npu_err("No TPF setting : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		} else {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			l->requested_tpf = param->offset;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
			sess->tpf = l->requested_tpf;
			sess->tpf_requested = true;
#endif
			npu_dbg("set tpf of uid %d as %lld\n",
					l->uid, l->requested_tpf);
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
			if (sess->dvfs_info)
				sess->dvfs_info->tpf = param->offset;
#endif
		}
		break;

	case NPU_S_PARAM_MO_SCEN_PRESET:
		npu_dbg("MO_SCEN_PRESET : %u\n", (u32)param->offset);
		if (param->offset == MO_SCEN_NORMAL) {
			npu_dbg("MO_SCEN_PRESET : npu_normal\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("npu_normal");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == MO_SCEN_PERF) {
			npu_dbg("MO_SCEN_PRESET : npu_performace\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("npu_performance");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == MO_SCEN_G3D_HEAVY) {
			npu_dbg("MO_SCEN_PRESET : g3d_heavy\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("g3d_heavy");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == MO_SCEN_G3D_PERF) {
			npu_dbg("MO_SCEN_PRESET : g3d_performance\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("g3d_performance");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("MO_SCEN_PRESET : default\n");
			if (g_npu_scheduler_info->bts_scenindex > 0) {
				ret = npu_bts_del_scenario(g_npu_scheduler_info->bts_scenindex);
				if (ret)
					npu_err("npu_bts_del_scenario failed : %d\n", ret);
			}
		}
		break;

	case NPU_S_PARAM_CPU_DISABLE_IDLE_PRESET:
		break;

	case NPU_S_PARAM_DD_LOG_OFF_PRESET:
		npu_scheduler_set_DD_log(param->offset);
		break;

	case NPU_S_PARAM_HWACG_NPU_DISABLE_PRESET:
		npu_scheduler_hwacg_set(HWACG_NPU, param->offset);
		break;

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	case NPU_S_PARAM_HWACG_DSP_DISABLE_PRESET:
		npu_scheduler_hwacg_set(HWACG_DSP, param->offset);
		break;
#endif

	case NPU_S_PARAM_HWACG_DNC_DISABLE_PRESET:
		npu_scheduler_hwacg_set(HWACG_DNC, param->offset);
		break;

	case NPU_S_PARAM_DD_DIRECT_PATH_PRESET:
		npu_dbg("DD_DIRECT_PATH val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			npu_dbg("DD_DIRECT_PATH On\n");
			g_npu_scheduler_info->dd_direct_path = 0x01;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("DD_DIRECT_PATH Off\n");
			g_npu_scheduler_info->dd_direct_path = 0x00;
		}

		break;
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
	case NPU_S_PARAM_FW_KPI_MODE:
		break;
#endif
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	case NPU_S_PARAM_DTM_PID_EN:
		npu_dbg("DTM_PID_EN val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			npu_dbg("DTM_PID On\n");
			g_npu_scheduler_info->pid_en = 0x01;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("DTM_PID Off\n");
			g_npu_scheduler_info->pid_en = 0x00;
		}
		break;

	case NPU_S_PARAM_DTM_TARGET_THERMAL:
		npu_dbg("DTM_TARGET_THERMAL : %u\n", (u32)param->offset);
		g_npu_scheduler_info->pid_target_thermal = param->offset;
		break;

	case NPU_S_PARAM_DTM_MAX_CLK:
		npu_dbg("DTM_MAX_CLK : %u\n", (u32)param->offset);
		g_npu_scheduler_info->pid_max_clk = param->offset;
		break;
#endif

	case NPU_S_PARAM_DVFS_DISABLE:
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
		npu_dbg("DVFS_DISABLE val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			npu_dbg("DVFS Off\n");
			g_npu_scheduler_info->enable = 0x00;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("DVFS On\n");
			g_npu_scheduler_info->enable = 0x01;
		}
#else
		g_npu_scheduler_info->enable = 0x00;
#endif
		break;

	case NPU_S_PARAM_BLOCKIO_UFS_MODE:
		npu_dbg("BLOCKIO_UFS_MODE val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			npu_dbg("UFS gear scale mode on\n");
			npu_scheduler_ufs_boost(false);
		} else if (param->offset == 0x02) {
			npu_dbg("UFS gear HS-G5 mode on\n");
			npu_scheduler_ufs_boost(true);
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("UFS gear scale mode on\n");
			npu_scheduler_ufs_boost(false);
		}
		break;

	case NPU_S_PARAM_DMABUF_LAZY_UNMAPPING_DISABLE:
		npu_dbg("DMABUF_LAZY_UNMAPPING_DISABLE  val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			sess->lazy_unmap_disable = NPU_SESSION_LAZY_UNMAP_DISABLE;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			sess->lazy_unmap_disable = NPU_SESSION_LAZY_UNMAP_ENABLE;
		}
		break;

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	case NPU_S_PARAM_DVFS_UNSET_TIME:
		if (param->offset == 0) {
			npu_err("No DVFS_UNSET_TIME setting : %u\n", (u32)param->offset);
			ret = S_PARAM_NOMB;
		} else {
			l->dvfs_unset_time = param->offset;
			npu_dbg("set DVFS_UNSET_TIME of uid %d as %u\n",
					l->uid, l->dvfs_unset_time);

			if (sess->dvfs_info)
				sess->dvfs_info->dvfs_unset_time = (u32)param->offset;
		}
		break;
#endif

	default:
		ret = S_PARAM_NOMB;
		break;
	}
	mutex_unlock(&g_npu_scheduler_info->param_handle_lock);

	return ret;
}

npu_s_param_ret npu_preference_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	u32 preference;
	npu_s_param_ret ret = S_PARAM_HANDLED;

	preference = param->offset;
	if ((preference >= CMD_LOAD_FLAG_IMB_PREFERENCE1) && (preference <= CMD_LOAD_FLAG_IMB_PREFERENCE5))
		sess->preference = param->offset;
	else
		sess->preference = CMD_LOAD_FLAG_IMB_PREFERENCE2;
	npu_dbg("preference = %u\n",preference);
	return ret;
}
