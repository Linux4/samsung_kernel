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
#include <soc/samsung/exynos/exynos-soc.h>

#include "npu-config.h"
#include "npu-scheduler-governor.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-dvfs.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#include "npu-util-memdump.h"

int npu_dvfs_get_ip_max_freq(struct vs4l_freq_param *param)
{
	int ret = 0;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	param->ip_count = 0;
	if (!info->activated || info->is_dvfs_cmd)
		return -ENODATA;
	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		strcpy(param->ip[param->ip_count].name, d->name);
		param->ip[param->ip_count].max_freq = d->max_freq;
		param->ip_count++;
	}
	mutex_unlock(&info->exec_lock);

	return ret;
}

int npu_dvfs_set_freq_boost(struct npu_scheduler_dvfs_info *d, void *req, u32 freq)
{
	u32 *cur_freq;
	struct exynos_pm_qos_request *target_req;

	WARN_ON(!d);

	if (req)
		target_req = (struct exynos_pm_qos_request *)req;
	else
		target_req = &d->qos_req_min;

	if (target_req->exynos_pm_qos_class == get_pm_qos_max(d->name))
		cur_freq = &d->limit_max;
	else if (target_req->exynos_pm_qos_class == get_pm_qos_min(d->name))
		cur_freq = &d->limit_min;
	else {
		npu_err("Invalid pm_qos class : %d\n", target_req->exynos_pm_qos_class);
		return -EINVAL;
	}

	//if (freq && *cur_freq == freq) {
		//npu_dbg("stick to current freq : %d\n", cur_freq);
	//	return 0;
	//}

	if (d->max_freq < freq)
		freq = d->max_freq;
	else if (d->min_freq > freq)
		freq = d->min_freq;

	npu_dvfs_pm_qos_update_request_boost(target_req, freq);

	*cur_freq = freq;

	d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);

	return freq;
}

int npu_dvfs_set_freq(struct npu_scheduler_dvfs_info *d, void *req, u32 freq)
{
	u32 *cur_freq;
	struct exynos_pm_qos_request *target_req;

	WARN_ON(!d);

	if (req)
		target_req = (struct exynos_pm_qos_request *)req;
	else
		target_req = &d->qos_req_min;

	if (target_req->exynos_pm_qos_class == get_pm_qos_max(d->name))
		cur_freq = &d->limit_max;
	else if (target_req->exynos_pm_qos_class == get_pm_qos_min(d->name))
		cur_freq = &d->limit_min;
	else {
		npu_err("Invalid pm_qos class : %d\n", target_req->exynos_pm_qos_class);
		return -EINVAL;
	}

	//if (freq && *cur_freq == freq) {
		//npu_dbg("stick to current freq : %d\n", cur_freq);
	//	return 0;
	//}

	if (d->max_freq < freq)
		freq = d->max_freq;
	else if (d->min_freq > freq)
		freq = d->min_freq;

	npu_dvfs_pm_qos_update_request(target_req, freq);

	*cur_freq = freq;

	d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);

	return freq;
}

void npu_dvfs_activate_peripheral(unsigned long freq)
{
	bool dvfs_active = false;
	bool is_core = false;
	int i;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	if (!info->activated || info->is_dvfs_cmd)
		return;

#endif

	/* get NPU max freq */
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp("NPU0", d->name) ||
		    !strcmp("NPU1", d->name)) {
			if (freq >= d->max_freq) {
				dvfs_active = true;
				break;
			}
		}
	}
	npu_trace("NPU maxlock at %ld\n", freq);

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
			npu_dvfs_set_freq(d, &d->qos_req_min, d->min_freq);
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
			if (dvfs_active)
				d->activated = 1;
			else
				d->activated = 0;
			npu_trace("%s %sactivated\n", d->name,
					d->activated ? "" : "de");
		}
	}

#else
		}
	}
#endif
}

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
u32 npu_dvfs_get_freq_ceil(struct npu_scheduler_dvfs_info *d, u32 freq)
{
	struct dev_pm_opp *opp;
	unsigned long f;

	/* Calculate target frequency */
	f = (unsigned long) freq;
	opp = dev_pm_opp_find_freq_ceil(&d->dvfs_dev->dev, &f);

	if (IS_ERR(opp)) {
		npu_err("err : target %d, check min/max freq\n", freq);
		return 0;
	} else {
		dev_pm_opp_put(opp);
		return f;
	}
}

int npu_dvfs_set_mode_freq(struct npu_scheduler_info *info, int uid)
{
	int ret = 0;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;
	struct npu_scheduler_dvfs_info *d;
	u32	freq = 0;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return 0;
	}

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return ret;
	}

	if (info->mode == NPU_PERF_MODE_NPU_BOOST_ONEXE) {
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			/* no actual governor or no name */
			if (!d->gov || !strcmp(d->gov->name, ""))
				continue;

			freq = d->cur_freq;

			/* check mode limit */
			if (freq < d->mode_min_freq[info->mode])
				freq = d->mode_min_freq[info->mode];

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
					freq = npu_dvfs_get_freq_ceil(d, freq);
					if (!freq)
						freq = d->cur_freq;
				}
			}
			d->is_init_freq = 1;

			/* set frequency */
			npu_dvfs_set_freq(d, NULL, freq);
		}
		mutex_unlock(&info->exec_lock);
	} else {
		/* set dvfs command list for the mode */
		ret = npu_dvfs_cmd_map(info, npu_perf_mode_name[info->mode]);
	}
	return ret;
}

void npu_dvfs_set_initial_freq(
		struct npu_device *device, npu_uid_t session_uid)
{
	u32 init_freq;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_fps_load *l;
	struct npu_scheduler_fps_load *tl;

	BUG_ON(!device);
	info = device->sched;

	if (!info->enable) {
		npu_dbg("scheduler disabled\n");
		return;
	}

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
		/* no actual governor or no name */
		if (!d->gov || !strcmp(d->gov->name, ""))
			continue;

		if (d->is_init_freq == 0 || tl->requested_tpf == 1) {
			init_freq = tl->init_freq_ratio * d->max_freq / 10000;
			init_freq = npu_dvfs_get_freq_ceil(d, init_freq);
			if (!init_freq)
				init_freq = d->cur_freq;
			/* set frequency */
			npu_dvfs_set_freq(d, NULL, init_freq);
			d->is_init_freq = 1;
		}
	}
	mutex_unlock(&info->exec_lock);
	mutex_unlock(&info->fps_lock);
}
#endif

u32 npu_dvfs_devfreq_get_domain_freq(struct npu_scheduler_dvfs_info *d)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	struct platform_device *pdev;
	struct exynos_devfreq_data *data;
	u32 devfreq_type;

	pdev = d->dvfs_dev;
	if (!pdev) {
		npu_err("platform_device is NULL!\n");
		return d->min_freq;
	}

	data = platform_get_drvdata(pdev);
	if (!data) {
		npu_err("exynos_devfreq_data is NULL!\n");
		return d->min_freq;
	}

	devfreq_type = data->devfreq_type;

	return exynos_devfreq_get_domain_freq(devfreq_type);
#else
	return 0;
#endif
}

void npu_dvfs_pm_qos_add_request(struct exynos_pm_qos_request *req,
					int exynos_pm_qos_class, s32 value)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_request(req, exynos_pm_qos_class, value);
#endif

}

void npu_dvfs_pm_qos_update_request_boost(struct exynos_pm_qos_request *req,
							s32 new_value)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(req, new_value);
#endif
}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
#define TOKEN_REQUEST	0x1
#define TOKEN_RESPONSE	0x2
#define TOKEN_OK	0xC0FFEE
#define REQ_DVFS	0x3333
#define REQ_FW		0x5555
static struct npu_system *npu_dvfs_token_system;
static bool npu_dvfs_get_token(struct npu_system *system, u32 request)
{
	bool ret;
	unsigned long flags;

	spin_lock_irqsave(&system->token_lock, flags);
	if (system->token) {
		system->token = false;
		ret = true;
		if (request == REQ_FW)
			dsp_dhcp_write_reg_idx(system->dhcp, DSP_DHCP_TOKEN_STATUS, TOKEN_OK);
	} else {
		ret = false;
		if (request == REQ_FW)
			system->token_fail = true;
	}
	spin_unlock_irqrestore(&system->token_lock, flags);

	return ret;
}

static void npu_dvfs_put_token(struct npu_system *system, u32 request)
{
	unsigned long flags;

	spin_lock_irqsave(&system->token_lock, flags);
	system->token = true;
	if (system->token_fail) {
		system->token = false;
		system->token_fail = false;
		dsp_dhcp_write_reg_idx(system->dhcp, DSP_DHCP_TOKEN_STATUS, TOKEN_OK);
	}
	spin_unlock_irqrestore(&system->token_lock, flags);
}

void npu_dvfs_update_token_status(struct npu_system *system)
{
	volatile u32 token_req;
	volatile u32 backup_token_req_cnt;

	backup_token_req_cnt = dsp_dhcp_read_reg_idx(system->dhcp, DSP_DHCP_TOKEN_REQ_CNT);
	if (system->backup_token_req_cnt != backup_token_req_cnt) {
		system->backup_token_req_cnt = backup_token_req_cnt;
	} else { // same
		npu_info("same backup : %u, FW req cnt : %u\n", system->backup_token_req_cnt, backup_token_req_cnt);
		return;
	}

	token_req = dsp_dhcp_read_reg_idx(system->dhcp, DSP_DHCP_TOKEN_REQ);
	if (token_req == TOKEN_REQUEST) {
		npu_dvfs_get_token(system, REQ_FW);
		system->token_req_cnt++;
	} else if (token_req == TOKEN_RESPONSE) {
		system->token_res_cnt++;
		npu_dvfs_put_token(system, REQ_FW);
	} else {
		npu_info("no req, res, just set by DD\n");
	}
	dsp_dhcp_write_reg_idx(system->dhcp, DSP_DHCP_TOKEN_REQ, 0x0);
}

void npu_dvfs_get_token_loop(struct npu_system *system)
{
	if ((atomic_inc_return(&system->dvfs_token_cnt) == 0x1)) {
		while (!npu_dvfs_get_token(system, REQ_DVFS)) {}
		system->token_dvfs_req_cnt++;
	}
}

void npu_dvfs_put_token_loop(struct npu_system *system)
{
	if ((atomic_dec_return(&system->dvfs_token_cnt) == 0x0)) {
		npu_dvfs_put_token(system, REQ_DVFS);
		system->token_dvfs_res_cnt++;
	}
}

static void npu_dvfs_token_work(struct work_struct *work)
{
	unsigned long flags;
	volatile u32 interrupt, hw_intr;

	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev >= 2)) {
		npu_info("SKIP token work on EVT1.2\n");
		return;
	}

	spin_lock_irqsave(&(npu_dvfs_token_system->token_wq_lock), flags);
	interrupt = dsp_dhcp_read_reg_idx(npu_dvfs_token_system->dhcp, DSP_DHCP_TOKEN_INTERRUPT);
	hw_intr = npu_read_hw_reg(npu_get_io_area(npu_dvfs_token_system, "sfrmbox1"), 0x2000, 0xFFFFFFFF, 0);
	if (interrupt || hw_intr) {
		dsp_dhcp_write_reg_idx(npu_dvfs_token_system->dhcp, DSP_DHCP_TOKEN_INTERRUPT, 0x0);
		npu_write_hw_reg(npu_get_io_area(npu_dvfs_token_system, "sfrmbox1"), 0x2000, 0x0, 0xFFFFFFFF, 0);
		npu_dvfs_update_token_status(npu_dvfs_token_system);
		npu_dvfs_token_system->token_work_cnt++;
		npu_dvfs_token_system->token_clear_cnt++;
		npu_dvfs_token_system->token_intr_cnt++;
	}
	spin_unlock_irqrestore(&(npu_dvfs_token_system->token_wq_lock), flags);

	queue_delayed_work(npu_dvfs_token_system->token_wq,
			&npu_dvfs_token_system->token_work, msecs_to_jiffies(100));
}

int npu_dvfs_token_work_probe(struct npu_device *device)
{
	int ret = 0;
	struct npu_system *system = &device->system;

	npu_dvfs_token_system = system;

	INIT_DELAYED_WORK(&system->token_work, npu_dvfs_token_work);
	system->token_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->token_wq) {
		probe_err("fail to create workqueue -> system->token_wq\n");
		ret = -EFAULT;
	}

	return ret;
}

static bool is_governor_request(struct exynos_pm_qos_request *req) {
	struct npu_scheduler_info *info = npu_scheduler_get_info();
	struct npu_sessionmgr *sessionmgr = &info->device->sessionmgr;
	return (req == &sessionmgr->npu_qos_req_dnc)
		|| (req == &sessionmgr->npu_qos_req_npu0)
		|| (req == &sessionmgr->npu_qos_req_npu1)
		|| (req == &sessionmgr->npu_qos_req_dsp);
}
#endif

void npu_dvfs_pm_qos_update_request(struct exynos_pm_qos_request *req,
							s32 new_value)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	bool is_set_dnc, is_set_npu0, is_set_npu0_max, is_set_npu1, is_set_npu1_max, lock_skip = false;
	struct npu_qos_setting *qos_setting;
	struct npu_scheduler_info *info = npu_scheduler_get_info();
	struct npu_scheduler_dvfs_info *d;
	struct npu_sessionmgr *sessionmgr;
	int dnc_value = 0;

	qos_setting = &(info->device->system.qos_setting);
	is_set_dnc = is_set_npu0 = is_set_npu0_max = is_set_npu1 = is_set_npu1_max = false;
	sessionmgr = &info->device->sessionmgr;

	if (req->exynos_pm_qos_class == PM_QOS_BUS_THROUGHPUT || req->exynos_pm_qos_class == PM_QOS_DEVICE_THROUGHPUT) {
		lock_skip = true;
		goto qos_request;
	}

	{
		if (qos_setting->info == NULL) {
			npu_err("info is NULL!\n");
			return;
		}

		if (exynos_soc_info.sub_rev < 2)
			npu_dvfs_get_token_loop(&info->device->system);

		if (is_governor_request(req)) {
			if (req->exynos_pm_qos_class == PM_QOS_NPU0_THROUGHPUT) {
				dnc_value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);
				if (dnc_value > new_value) {
					exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_dnc, new_value);
					is_set_dnc = true;
				}
			}
			if (req->exynos_pm_qos_class == PM_QOS_NPU1_THROUGHPUT) {
				dnc_value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);
				if (dnc_value > new_value) {
					exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_dnc, new_value);
					is_set_dnc = true;
				}
			}
			if (req->exynos_pm_qos_class == PM_QOS_DNC_THROUGHPUT) {
				int npu0_min_value = exynos_pm_qos_request(PM_QOS_NPU0_THROUGHPUT);
				int npu1_min_value = exynos_pm_qos_request(PM_QOS_NPU1_THROUGHPUT);
				if (new_value > npu0_min_value) {
					exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_npu0, new_value);
					is_set_npu0 = true;
				}
				if (new_value > npu1_min_value) {
					exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_npu1, new_value);
					is_set_npu1 = true;
				}
			}
		} else {
			if (req->exynos_pm_qos_class == PM_QOS_NPU0_THROUGHPUT) {
				dnc_value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);
				if (dnc_value > new_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_dnc, new_value);
					is_set_dnc = true;
				}
			}
			if (req->exynos_pm_qos_class == PM_QOS_NPU1_THROUGHPUT) {
				dnc_value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);
				if (dnc_value > new_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_dnc, new_value);
					is_set_dnc = true;
				}
			}
			if (req->exynos_pm_qos_class == PM_QOS_NPU0_THROUGHPUT_MAX) {
				dnc_value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);
				if (dnc_value > new_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_dnc, new_value);
					is_set_dnc = true;
				}
			}
			if (req->exynos_pm_qos_class == PM_QOS_NPU1_THROUGHPUT_MAX) {
				dnc_value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);
				if (dnc_value > new_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_dnc, new_value);
					is_set_dnc = true;
				}
			}
			if (req->exynos_pm_qos_class == PM_QOS_DNC_THROUGHPUT) {
				int npu0_min_value = exynos_pm_qos_request(PM_QOS_NPU0_THROUGHPUT);
				int npu0_max_value = exynos_pm_qos_request(PM_QOS_NPU0_THROUGHPUT_MAX);
				int npu1_min_value = exynos_pm_qos_request(PM_QOS_NPU1_THROUGHPUT);
				int npu1_max_value = exynos_pm_qos_request(PM_QOS_NPU1_THROUGHPUT_MAX);
				if (new_value > npu0_min_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_npu0, new_value);
					is_set_npu0 = true;
				}
				if (new_value > npu0_max_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_npu0_max, new_value);
					is_set_npu0_max = true;
				}
				if (new_value > npu1_min_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_npu1, new_value);
					is_set_npu1 = true;
				}
				if (new_value > npu1_max_value) {
					exynos_pm_qos_update_request(&qos_setting->npu_qos_req_npu1_max, new_value);
					is_set_npu1_max = true;
				}
			}
		}
		list_for_each_entry(d, &info->ip_list, ip_list) {
			if (!strcmp("DNC", d->name) && is_set_dnc) {
				d->limit_min = new_value;
				d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);
			}
			if (!strcmp("NPU0", d->name)) {
				if (is_set_npu0) {
					d->limit_min = new_value;
				}
				if (is_set_npu0_max) {
					d->limit_max = new_value;
				}
				if (is_set_npu0 || is_set_npu0_max) {
					d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);
				}
			}
			if (!strcmp("NPU1", d->name)) {
				if (is_set_npu1) {
					d->limit_min = new_value;
				}
				if (is_set_npu1_max) {
					d->limit_max = new_value;
				}
				if (is_set_npu1 || is_set_npu1_max) {
					d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);
				}
			}
		}

	}
qos_request:

	if (req->exynos_pm_qos_class == PM_QOS_DNC_THROUGHPUT) {
		if(new_value >= 800000) {
			exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_mif_for_bw,
					1352000); //set 1352000 Khz for minlock
			exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_int_for_bw,
					533000); // set 533000 Khz for minlock
		} else {
			exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_mif_for_bw,
					0); //set 0 Khz for minlock
			exynos_pm_qos_update_request(&sessionmgr->npu_qos_req_int_for_bw,
					0); // set 0 Khz for minlock
		}
	}
#endif

	exynos_pm_qos_update_request(req, new_value);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if (!lock_skip && (exynos_soc_info.sub_rev < 2))
		npu_dvfs_put_token_loop(&info->device->system);
#endif
#endif
}

int npu_dvfs_pm_qos_get_class(struct exynos_pm_qos_request *req)
{
	return req->exynos_pm_qos_class;
}

/* dt_name has prefix "samsung,npudvfs-" */
#define NPU_DVFS_CMD_PREFIX	"samsung,npudvfs-"
static int npu_dvfs_get_cmd_map(struct npu_system *system, struct dvfs_cmd_list *cmd_list)
{
	int i, ret = 0;
	struct dvfs_cmd_contents *cmd_clock = NULL;
	char dvfs_name[64], clock_name[64];
	char **cmd_dvfs;
	struct device *dev;

	dvfs_name[0] = '\0';
	clock_name[0] = '\0';
	strcpy(dvfs_name, NPU_DVFS_CMD_PREFIX);
	strcpy(clock_name, NPU_DVFS_CMD_PREFIX);
	strcat(strcat(dvfs_name, cmd_list->name), "-dvfs");
	strcat(strcat(clock_name, cmd_list->name), "-clock");

	dev = &(system->pdev->dev);
	cmd_list->count = of_property_count_strings(
			dev->of_node, dvfs_name);
	if (cmd_list->count <= 0) {
		probe_warn("invalid dvfs_cmd list by %s\n", dvfs_name);
		cmd_list->list = NULL;
		cmd_list->count = 0;
		ret = -ENODEV;
		goto err_exit;
	}
	probe_info("%s dvfs %d commands\n", dvfs_name, cmd_list->count);

	cmd_list->list = (struct dvfs_cmd_map *)devm_kmalloc(dev,
				(cmd_list->count + 1) * sizeof(struct dvfs_cmd_map),
				GFP_KERNEL);
	if (!cmd_list->list) {
		probe_err("failed to alloc for dvfs cmd map\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	(cmd_list->list)[cmd_list->count].name = NULL;

	cmd_dvfs = (char **)devm_kmalloc(dev,
			cmd_list->count * sizeof(char *), GFP_KERNEL);
	if (!cmd_dvfs) {
		probe_err("failed to alloc for dvfs_cmd devfreq for %s\n", dvfs_name);
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = of_property_read_string_array(dev->of_node, dvfs_name, (const char **)cmd_dvfs, cmd_list->count);
	if (ret < 0) {
		probe_err("failed to get dvfs_cmd for %s (%d)\n", dvfs_name, ret);
		ret = -EINVAL;
		goto err_dvfs;
	}

	cmd_clock = (struct dvfs_cmd_contents *)devm_kmalloc(dev,
			cmd_list->count * sizeof(struct dvfs_cmd_contents), GFP_KERNEL);
	if (!cmd_clock) {
		probe_err("failed to alloc for dvfs_cmd clock for %s\n", clock_name);
		ret = -ENOMEM;
		goto err_dvfs;
	}
	ret = of_property_read_u32_array(dev->of_node, clock_name, (u32 *)cmd_clock,
			cmd_list->count * NPU_DVFS_CMD_LEN);
	if (ret) {
		probe_err("failed to get reg_cmd for %s (%d)\n", clock_name, ret);
		ret = -EINVAL;
		goto err_clock;
	}

	for (i = 0; i < cmd_list->count; i++) {
		(*(cmd_list->list + i)).name = *(cmd_dvfs + i);
		memcpy((void *)(&((cmd_list->list + i)->contents)),
				(void *)(&cmd_clock[i]), sizeof(struct dvfs_cmd_contents));

		probe_info("copy %s cmd (%lu)\n", *(cmd_dvfs + i), sizeof(u32) * NPU_DVFS_CMD_LEN);
	}
err_clock:
	devm_kfree(dev, cmd_clock);
err_dvfs:
	devm_kfree(dev, cmd_dvfs);
err_exit:
	return ret;
}

static struct dvfs_cmd_list npu_dvfs_cmd_list[] = {
	{	.name = "open",	.list = NULL,	.count = 0	},
	{	.name = "close",	.list = NULL,	.count = 0	},
	{	.name = NULL,		.list = NULL,	.count = 0	}
};

int npu_dvfs_init_cmd_list(struct npu_system *system, struct npu_scheduler_info *info)
{
	int ret = 0;
	int i;

	BUG_ON(!info);

	info->dvfs_list = (struct dvfs_cmd_list *)npu_dvfs_cmd_list;

	for (i = 0; ((info->dvfs_list) + i)->name; i++) {
		if (npu_dvfs_get_cmd_map(system, (info->dvfs_list) + i) == -ENODEV)
			probe_info("No cmd for %s\n", ((info->dvfs_list) + i)->name);
		else
			probe_info("register cmd for %s\n", ((info->dvfs_list) + i)->name);
	}

	return ret;
}

int npu_dvfs_cmd(struct npu_scheduler_info *info, const struct dvfs_cmd_list *cmd_list)
{
	int ret = 0;
	size_t i;
	char *name;
	struct dvfs_cmd_contents *t;
	struct npu_scheduler_dvfs_info *d;

	BUG_ON(!info);

	if (!cmd_list) {
		npu_info("No cmd for dvfs\n");
		/* no error, just skip */
		return 0;
	}

	info->is_dvfs_cmd = true;
	mutex_lock(&info->exec_lock);
	for (i = 0; i < cmd_list->count; ++i) {
		name = (cmd_list->list + i)->name;
		t = &((cmd_list->list + i)->contents);
		if (name) {
			npu_info("set %s %s as %d\n", name, t->cmd ? "max" : "min", t->freq);
			d = get_npu_dvfs_info(info, name);
			switch (t->cmd) {
			case NPU_DVFS_CMD_MIN:
				npu_dvfs_set_freq(d, &d->qos_req_min_dvfs_cmd, t->freq);
				break;
			case NPU_DVFS_CMD_MAX:
				{
					unsigned long f;
					struct dev_pm_opp *opp;

					f = ULONG_MAX;

					opp = dev_pm_opp_find_freq_floor(&d->dvfs_dev->dev, &f);
					if (IS_ERR(opp)) {
						npu_err("invalid max freq for %s\n", d->name);
						f = d->max_freq;
					} else {
						dev_pm_opp_put(opp);
					}

					d->max_freq = NPU_MIN(f, t->freq);
				}
				npu_dvfs_set_freq(d, &d->qos_req_max_dvfs_cmd, d->max_freq);
				break;
			default:
				break;
			}
		} else
			break;
	}
	mutex_unlock(&info->exec_lock);
	info->is_dvfs_cmd = false;
	return ret;
}

int npu_dvfs_cmd_map(struct npu_scheduler_info *info, const char *cmd_name)
{
	BUG_ON(!info);
	BUG_ON(!cmd_name);

	return npu_dvfs_cmd(info, (const struct dvfs_cmd_list *)get_npu_dvfs_cmd_map(info, cmd_name));
}

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
void npu_dvfs_set_info_to_session(struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_sess_info *dvfs_info;
	struct npu_scheduler_dvfs_sess_info *t_di;
	bool found_match = 0;
	struct npu_scheduler_fps_load *l;
	int dvfs_info_cnt = 0;
	s64 oldest = LLONG_MAX;
	size_t sess_buf_size = 0;
	u32 sess_tpf = NPU_SCHEDULER_DEFAULT_REQUESTED_TPF;
	u32 sess_dvfs_unset_time = 0;
	bool sess_is_nm_mode = FALSE;

	info = npu_scheduler_get_info();

	if (session->ncp_mem_buf)
		sess_buf_size += session->ncp_mem_buf->size;

	if (session->IOFM_mem_buf)
		sess_buf_size += session->IOFM_mem_buf->size;

	if (session->IMB_mem_buf)
		sess_buf_size += session->IMB_mem_buf->size;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			sess_tpf = l->requested_tpf;
			sess_dvfs_unset_time = l->dvfs_unset_time;
			sess_is_nm_mode = (l->mode == NPU_PERF_MODE_NORMAL) ? TRUE : FALSE;
			break;
		}
	}
	mutex_unlock(&info->fps_lock);

	mutex_lock(&info->dvfs_info_lock);
	list_for_each_entry(t_di, &info->dsi_list, list) {
		if (sess_buf_size == t_di->buf_size && session->hids == t_di->hids && !t_di->is_linked) {
			if ((sess_tpf == t_di->tpf && sess_is_nm_mode) || !sess_is_nm_mode ) {
				found_match = 1;
				dvfs_info = t_di;
				npu_dbg("Session_info - reuse[%d][%lu]\n", session->hids, sess_buf_size);
				break;
			}
		}
		if (oldest > t_di->last_qbuf_usecs) {
			oldest = t_di->last_qbuf_usecs;
			dvfs_info = t_di;
		}
		dvfs_info_cnt++;
	}

	// Alloc and init
	if (!found_match) {
		if (dvfs_info_cnt < NPU_SCHEDULER_MAX_DVFS_INFO) {
			dvfs_info = (struct npu_scheduler_dvfs_sess_info *)devm_kzalloc(info->dev,
						sizeof(struct npu_scheduler_dvfs_sess_info), GFP_KERNEL);
			list_add_tail(&dvfs_info->list, &info->dsi_list);
			npu_dbg("Session_info - alloc[%d][%lu]\n", session->hids, sess_buf_size);
		}
		else
			npu_dbg("Session_info - replace[%d][%lu]\n", session->hids, sess_buf_size);

		if (!dvfs_info) {
			npu_warn("Failed to alloc npu_scheduler_dvfs_sess_info\n");
			return;
		}

		dvfs_info->buf_size = sess_buf_size;
		dvfs_info->hids = session->hids;
		dvfs_info->unified_op_id = session->unified_op_id;

		dvfs_info->last_qbuf_usecs = 0;
		dvfs_info->last_dqbuf_usecs = 0;
		dvfs_info->last_run_usecs = 0;
		dvfs_info->last_idle_msecs = NPU_SCHEDULER_DEFAULT_IDLE_DELAY;

		dvfs_info->prev_freq = 0;
		dvfs_info->req_freq = 1066000;
		dvfs_info->limit_freq = info->dvfs_table[0];
		dvfs_info->tpf = sess_tpf;
		dvfs_info->load = 1024;
		dvfs_info->dvfs_io_offset = 0;
		dvfs_info->is_unified = FALSE;
		dvfs_info->dvfs_clk_dn_delay = 0;
		dvfs_info->dvfs_unset_time= sess_dvfs_unset_time;

		list_for_each_entry(t_di, &info->dsi_list, list) {
			if (dvfs_info->unified_op_id == t_di->unified_op_id && dvfs_info != t_di) {
				dvfs_info->is_unified = TRUE;
				t_di->is_unified = TRUE;
				break;
			}
		}
	}
	dvfs_info->is_nm_mode = sess_is_nm_mode;

	session->dvfs_info = dvfs_info;
	dvfs_info->is_linked = TRUE;
	mutex_unlock(&info->dvfs_info_lock);

#if 0
	printk("dsi_info session_s_graph ncp buf : %lu\n", session->ncp_mem_buf->size);
	{
		int cnt = 0;
		list_for_each_entry(dvfs_info, &info->dsi_list, list) {
			printk("dsi_info session_s_graph list[%d] : %lu\n", cnt++,  dvfs_info->buf_size);
		}
	}
	printk("dsi_info session_s_graph session : %lu\n", sess_buf_size);
#endif

}
void npu_dvfs_unset_freq(struct npu_scheduler_info *info)
{
	if( atomic_read( &info->dvfs_queue_cnt) == 0) {
		struct npu_scheduler_dvfs_info *d;

		list_for_each_entry(d, &info->ip_list, ip_list) {
			if (!strcmp("NPU0", d->name) ||
				!strcmp("NPU1", d->name) ||
				!strcmp("DSP", d->name) ||
				!strcmp("DNC", d->name))
				npu_dvfs_set_freq(d, &d->qos_req_min, d->min_freq);
		}
	}
	atomic_set(&info->dvfs_unlock_activated, 0);
}

void npu_dvfs_unset_freq_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	mutex_lock(&info->exec_lock);
	npu_dvfs_unset_freq(info);
	mutex_unlock(&info->exec_lock);
}

void npu_dvfs_qbuf(struct npu_session *session)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_sess_info *dvfs_info = session->dvfs_info;
	s32 cur_req_freq;
	s32 t_queue_cnt;

	info = npu_scheduler_get_info();

	if (!dvfs_info->is_nm_mode) {
		npu_dbg("qbuf IFD disable %d %d\n", info->mode, dvfs_info->is_nm_mode);
		return;
	}

	if (atomic_read( &info->dvfs_unlock_activated)) {
		atomic_set(&info->dvfs_unlock_activated, 0);
		cancel_delayed_work_sync(&info->dvfs_work);
	}

	mutex_lock(&info->exec_lock);
	mutex_lock(&info->dvfs_info_lock);

	t_queue_cnt = (s32)atomic_read( &info->dvfs_queue_cnt);

	list_for_each_entry(d, &info->ip_list, ip_list) {
		if ((!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name)) && (dvfs_info->hids & NPU_HWDEV_ID_NPU)) {
			cur_req_freq = (dvfs_info->req_freq > d->cur_freq || t_queue_cnt == 0)
						? dvfs_info->req_freq : d->cur_freq;
			npu_dvfs_set_freq(d, &d->qos_req_min, cur_req_freq);
		} else if (!strcmp("DSP", d->name)) {
			if (dvfs_info->is_unified || (dvfs_info->hids & NPU_HWDEV_ID_DSP)) {
				cur_req_freq = (dvfs_info->req_freq > d->cur_freq || t_queue_cnt == 0)
							? dvfs_info->req_freq : d->cur_freq;
				npu_dvfs_set_freq(d, &d->qos_req_min, cur_req_freq);
			}
		} else if (!strcmp("DNC", d->name)) {
			if (dvfs_info->hids & NPU_HWDEV_ID_NPU || dvfs_info->hids & NPU_HWDEV_ID_DSP) {
				cur_req_freq = npu_scheduler_get_clk(info,
							npu_scheduler_get_lut_idx(info, dvfs_info->req_freq, DIT_CORE), DIT_DNC);

				cur_req_freq = (cur_req_freq > d->cur_freq || t_queue_cnt == 0)
							? cur_req_freq : d->cur_freq;
				npu_dvfs_set_freq(d, &d->qos_req_min, cur_req_freq);
			}
		}
	}
	dvfs_info->last_qbuf_usecs = npu_get_time_us();

	atomic_inc( &info->dvfs_queue_cnt);
#if 0
	if (atomic_read( &info->dvfs_queue_cnt) >= 0) {
		smp_mb();
		atomic_inc( &info->dvfs_queue_cnt);
		smp_mb();
	}
#endif
	mutex_unlock(&info->dvfs_info_lock);
	mutex_unlock(&info->exec_lock);

}

void npu_dvfs_dqbuf(struct npu_session *session)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_sess_info *dvfs_info = session->dvfs_info;
	u64 t_idle_time;
	u64 temp_freq;
	u64 temp_load;
	s32 t_queue_cnt;

	info = npu_scheduler_get_info();

	if (!dvfs_info->is_nm_mode) {
		npu_dbg("dqbuf IFD disable %d %d\n", info->mode, dvfs_info->is_nm_mode);
		return;
	}

	if (atomic_read( &info->dvfs_unlock_activated)) {
		atomic_set(&info->dvfs_unlock_activated, 0);
		cancel_delayed_work_sync(&info->dvfs_work);
	}

	mutex_lock(&info->exec_lock);
	mutex_lock(&info->dvfs_info_lock);

	// Calc. last idle time
	if (dvfs_info->dvfs_unset_time == 0) {
		// (value_msecs + 999) => ceil
		// (value * 5) >> 12 =>  value * 122%
		t_idle_time = ((dvfs_info->last_qbuf_usecs- dvfs_info->last_dqbuf_usecs + 999) * 5) >> 12;
		if (t_idle_time < NPU_SCHEDULER_MAX_IDLE_DELAY) {
			if (dvfs_info->last_idle_msecs > t_idle_time) {
				dvfs_info->last_idle_msecs = (dvfs_info->last_idle_msecs + t_idle_time) >> 1;
			} else {
				dvfs_info->last_idle_msecs = t_idle_time;
			}
		}
	} else {
		dvfs_info->last_idle_msecs = dvfs_info->dvfs_unset_time;
	}

	// Calc. last run time
	dvfs_info->last_dqbuf_usecs = npu_get_time_us();
	dvfs_info->last_run_usecs = dvfs_info->last_dqbuf_usecs - dvfs_info->last_qbuf_usecs;

	// Calc. load(fixed point value : fractional part = 10bit )
	if (dvfs_info->tpf)
		dvfs_info->load = (dvfs_info->load * 3 + ((dvfs_info->last_run_usecs << 10) / (u64)dvfs_info->tpf)) >> 2;

	// Calc. set frequency
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if ((!strcmp("NPU0", d->name) && (dvfs_info->hids & NPU_HWDEV_ID_NPU)) ||
			((!strcmp("DSP", d->name) && (dvfs_info->hids & NPU_HWDEV_ID_DSP) && !dvfs_info->is_unified))) {

			if (dvfs_info->prev_freq == 0)
				dvfs_info->prev_freq = dvfs_info->req_freq;
			else
				dvfs_info->prev_freq = (dvfs_info->prev_freq * 3 + dvfs_info->req_freq) >> 2;

			if (dvfs_info->load > (1 << 10))	{	// (1 << 10) = 100% [measure / target]
				int clk_idx = 1;

				while(1) {
					temp_freq = npu_scheduler_get_clk(info,
										npu_scheduler_get_lut_idx(info, dvfs_info->prev_freq, DIT_CORE) - clk_idx, DIT_CORE);
					temp_load = (temp_freq << 10) / dvfs_info->prev_freq;

					npu_dbg("DQbuf freq_up:[%s] %d ==t_f %llu, prev_f %d, t_l %llu, dvfs_info->load %llu\n"
						, d->name, clk_idx, temp_freq, dvfs_info->prev_freq, temp_load, dvfs_info->load);

					if ((temp_load > dvfs_info->load || dvfs_info->prev_freq >= d->max_freq) ||
						(clk_idx >= info->dvfs_table_num[0]))
						break;
					else
						clk_idx++;
				}
				dvfs_info->dvfs_clk_dn_delay = 0;
			} else {
					temp_freq = npu_scheduler_get_clk(info,
										npu_scheduler_get_lut_idx(info, dvfs_info->prev_freq, DIT_CORE) + 1, DIT_CORE);
					temp_load = (temp_freq << 10) / dvfs_info->prev_freq;

					if (temp_load < (dvfs_info->load * 6) >> 3) // (value * 6) >> 3 = 0.75%
						dvfs_info->dvfs_clk_dn_delay = 0;
					else
						dvfs_info->dvfs_clk_dn_delay++;

					if (dvfs_info->dvfs_clk_dn_delay < 4)
						temp_freq = dvfs_info->prev_freq;

					npu_dbg("DQbuf freq_dn:[%s] ==t_f %llu, prev_f %d, t_l %llu, dvfs_info->load %llu\n"
						, d->name, temp_freq, dvfs_info->prev_freq, temp_load, dvfs_info->load);
			}
			dvfs_info->req_freq = (temp_freq > dvfs_info->limit_freq ) ? dvfs_info->limit_freq : temp_freq;
		}
	}
	t_queue_cnt = (s32)atomic_read( &info->dvfs_queue_cnt);

	atomic_dec( &info->dvfs_queue_cnt);

	mutex_unlock(&info->dvfs_info_lock);
	mutex_unlock(&info->exec_lock);

	if ( t_queue_cnt == 1) {
		queue_delayed_work(info->dvfs_wq, &info->dvfs_work, msecs_to_jiffies(dvfs_info->last_idle_msecs));
		atomic_set(&info->dvfs_unlock_activated, 1);
		npu_dbg("DQbuf set queue_delayed_work: %d ms\n", dvfs_info->last_idle_msecs);
	}
}
#endif
