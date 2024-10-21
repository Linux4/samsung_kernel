// SPDX-License-Identifier: GPL-2.0-only
/*
 * Gear scale with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <linux/of.h>
#include <linux/devfreq.h>
#include "ufs-exynos-perf.h"
#include "ufs-cal-if.h"
#include "ufs-exynos.h"
#include "ufs-vs-regs.h"
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include <linux/reboot.h>

#include <trace/events/ufs_exynos_perf.h>

void exynos_get_devfreq_noti(struct work_struct *work)
{
	struct ufs_perf_stat_v2 *stat = container_of(to_delayed_work(work),
				struct ufs_perf_stat_v2, devfreq_work);
	struct ufs_perf *perf = container_of(stat, struct ufs_perf, stat_v2);
	struct ufs_hba *hba = perf->hba;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct devfreq *devfreq;

	devfreq = devfreq_get_devfreq_by_phandle(ufs->dev, "devfreq", 0);

	if (IS_ERR(devfreq)) {
		schedule_delayed_work(&stat->devfreq_work,
				msecs_to_jiffies(10000));
		dev_info(ufs->dev, "%s: failed to get phandle!![%d]\n",
				__func__, devfreq);
	} else {
		stat->exynos_stat = UFS_EXYNOS_PERF_OPERATIONAL;
		dev_info(ufs->dev, "%s: success get phandle!!\n", __func__);
	}
}

static int exynos_ufs_reboot_handler(struct notifier_block *nb,	unsigned long l,
				void *p)
{
	struct ufs_perf_stat_v2 *stat = container_of(nb,
				struct ufs_perf_stat_v2, reboot_nb);

	stat->exynos_stat = UFS_EXYNOS_SHUTDOWN;
	return 0;
}

static int exynos_change_power_mode(struct ufs_hba *hba, struct uic_pwr_mode *pmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = 0;

	/* info gear update */
	hba->pwr_info.gear_rx = pmd->gear;
	hba->pwr_info.gear_tx = pmd->gear;
	hba->pwr_info.lane_rx = pmd->lane;
	hba->pwr_info.lane_tx = pmd->lane;
	hba->pwr_info.pwr_rx = FAST_MODE;
	hba->pwr_info.pwr_tx = FAST_MODE;
	hba->pwr_info.hs_rate = PA_HS_MODE_B;

	/* Unipro Attribute set */
	unipro_writel(handle, pmd->gear, UNIP_PA_RXGEAR); /* PA_RxGear */
	unipro_writel(handle, pmd->gear, UNIP_PA_TXGEAR); /* PA_TxGear */
	unipro_writel(handle, pmd->lane, UNIP_PA_ACTIVERXDATALENS); /* PA_ActiveRxDataLanes */
	unipro_writel(handle, pmd->lane, UNIP_PA_ACTIVETXDATALENS); /* PA_ActiveTxDataLanes */
	unipro_writel(handle, 1, UNIP_PA_RXTERMINATION); /* PA_RxTermination */
	unipro_writel(handle, 1, UNIP_PA_TXTERMINATION); /* PA_TxTermination */
	unipro_writel(handle, pmd->hs_series, UNIP_PA_HSSERIES); /* PA_HSSeries */

	ret = ufshcd_uic_change_pwr_mode(hba,
			(TX_PWRMODE(FAST_MODE) | RX_PWRMODE(FAST_MODE)));
	if (ret)
		dev_err(hba->dev, "%s: failed to change pwr_mode ret %d\n",
				__func__, ret);

	return ret;
}

int ufs_gear_change(struct ufs_hba *hba, bool en)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *pmd = &ufs->hci_pmd_parm;
	struct uic_pwr_mode *act_pmd = &ufs->device_pmd_parm;
	struct ufs_perf *perf = ufs->perf;
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;
	unsigned long flags;
	int prev, ret = 0, val;

	prev = act_pmd->gear;
	if (en) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		val = (pmd->gear == UFS_HS_G5) ? ufs->pm_qos_gear5_int :
			ufs->pm_qos_int_value;
		if (val)
			exynos_pm_qos_update_request(&stat->pm_qos_int, val);
#endif
		act_pmd->gear = pmd->gear;
	} else {
		act_pmd->gear = UFS_HS_G1;
	}

	if (prev == act_pmd->gear)
		return ret;

	pr_info("%s: prev: %u, target gear = %u\n", __func__, prev,
			(u32)act_pmd->gear);

	/* pre pmc */
	ufs->cal_param.pmd = act_pmd;

	ufshcd_auto_hibern8_update(hba, 0);
	ret = exynos_ufs_check_ah8_fsm_state(hba, HCI_AH8_IDLE_STATE);
	if (ret) {
		dev_err(hba->dev,
			"%s: exynos_ufs_check_ah8_fsm_state return value is %d\n",
			__func__, ret);

		goto out;
	}

	ret = ufs_call_cal(ufs, ufs_cal_pre_pmc);
	if (ret) {
		dev_info(ufs->dev, "cal pre pmc fail\n");
		goto out;
	}

	/* gear change */
	ret = exynos_change_power_mode(hba, act_pmd);
	if (ret) {
		dev_info(ufs->dev, "pmc set fail\n");
		goto out;
	}

	/* post pmc */
	ret = ufs_call_cal(ufs, ufs_cal_post_pmc);
	if (ret) {
		dev_info(ufs->dev, "cal post pmc fail\n");
		goto out;
	}

	ufshcd_auto_hibern8_update(hba, ufs->ah8_ahit);

	dev_info(ufs->dev, "ufs power mode change: m(%d)g(%d)l(%d)hs-series(%d)\n",
			(act_pmd->mode & 0xF), act_pmd->gear, act_pmd->lane,
			act_pmd->hs_series);

	trace_ufs_perf_gear("gear change", prev, act_pmd->gear);

	if (act_pmd->gear > UFS_HS_G1) {
		val = (pmd->gear == UFS_HS_G5) ? ufs->pm_qos_gear5_int :
			ufs->pm_qos_int_value;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (ufs->pm_qos_int_value)
			exynos_pm_qos_update_request(&ufs->pm_qos_int, val);
#endif
	} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif
	}

	/* release gear-scale minlock */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_update_request(&stat->pm_qos_int, 0);
#endif

	return ret;
out:
	spin_lock_irqsave(&perf->lock_handle, flags);
	stat->o_traffic = (prev > UFS_HS_G1) ? TRAFFIC_HIGH : TRAFFIC_LOW;
	spin_unlock_irqrestore(&perf->lock_handle, flags);

	act_pmd->gear = prev;

	return ret;
}

static void ufs_g_scale_handler(struct work_struct *work)
{
	struct ufs_perf_stat_v2 *stat = container_of(work,
			struct ufs_perf_stat_v2, gear_work);
	struct ufs_perf *perf = container_of(stat, struct ufs_perf, stat_v2);
	struct ufs_hba *hba = perf->hba;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (stat->exynos_stat != UFS_EXYNOS_PERF_OPERATIONAL)
		return;

	if (hba->pm_op_in_progress)
		return;

	if (!ufshcd_is_link_active(hba) || !ufshcd_is_ufs_dev_active(hba))
		return;

	ret = ufs_gear_change(hba, stat->g_scale_en);
	if (ret)
		pr_err("gear change fail\n");
}

static policy_res __update_v2(struct ufs_perf *perf, u32 qd,
			ufs_perf_op op, ufs_perf_entry entry)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;
	traffic traffic = 0;
	policy_res res = R_OK;
	unsigned long flags;
	unsigned long  diff;
	ktime_t time = ktime_get();

	spin_lock_irqsave(&perf->lock_handle, flags);
	if (qd >= stat->th_qd_max && stat->count >= qd * SZ_512K) {
		if (stat->g_scale_en == 0) {
			stat->start_count_time = -1LL;
			stat->g_scale_en = 1;
			traffic = TRAFFIC_HIGH;

			res = R_CTRL;
			goto ctrl;
		}
	}

	if (stat->start_count_time == -1LL) {
		stat->start_count_time = time;
		traffic = TRAFFIC_NONE;
	} else {
		diff = ktime_to_ms(ktime_sub(time,
					stat->start_count_time));
		if (diff >= stat->th_duration) {
			if (stat->count / diff >= stat->th_max_count) {
				stat->start_count_time = -1LL;
				stat->g_scale_en = 1;
				stat->count = 0;
				traffic = TRAFFIC_HIGH;

				res = R_CTRL;
			} else if (stat->o_traffic == TRAFFIC_HIGH &&
				stat->count / diff >= stat->th_min_count) {
				stat->start_count_time = -1LL;
				stat->g_scale_en = 1;
				stat->count = 0;
				traffic = TRAFFIC_HIGH;

				res = R_CTRL;
			} else {
				stat->start_count_time = -1LL;
				stat->g_scale_en = 0;
				stat->count = 0;
				traffic = TRAFFIC_LOW;

				res = R_CTRL;
			}
		} else {
			traffic = TRAFFIC_NONE;
			res = R_OK;
		}
	}

ctrl:
	if (traffic) {
		if (stat->o_traffic != traffic) {
			stat->o_traffic = traffic;
			queue_work(stat->scale_wq, &stat->gear_work);
		}
	}
	spin_unlock_irqrestore(&perf->lock_handle, flags);

	return res;
}

static void __resume(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;
	unsigned long flags;

	spin_lock_irqsave(&perf->lock_handle, flags);
	stat->start_count_time = -1LL;
	stat->count = 0;
	spin_unlock_irqrestore(&perf->lock_handle, flags);
}

int ufs_gear_scale_update(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;
	int ret = 0;

	if (perf->exynos_gear_scale) {
		perf->stat_bits |= UPDATE_GEAR;
		stat->exynos_stat = UFS_EXYNOS_PERF_OPERATIONAL;
		stat->o_traffic = TRAFFIC_NONE;

		__resume(perf);
	} else {
		perf->stat_bits &= ~UPDATE_GEAR;
		stat->exynos_stat = UFS_EXYNOS_PERF_DISABLED;

		cancel_work_sync(&stat->gear_work);
		ret = ufs_gear_change(perf->hba, true);
		if (ret)
			pr_err("%s: ufs_gear_change failed: ret = %d\n",
					__func__, ret);
	}

	return ret;
}

void ufs_gear_scale_exit(struct ufs_perf *perf)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;

	exynos_pm_qos_remove_request(&stat->pm_qos_int);
#endif
}

void ufs_gear_scale_init(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;

	/* register callbacks */
	perf->update[__UPDATE_GEAR] = __update_v2;
	perf->resume[__UPDATE_GEAR] = __resume;

	/* default thresholds for stats */
	stat->start_count_time = -1LL;
	stat->th_duration = 200;
	stat->th_qd_max = 10;
	stat->th_max_count = 180000;
	stat->th_min_count = 50000;
	stat->o_traffic = TRAFFIC_HIGH;
	stat->exynos_stat = UFS_EXYNOS_BOOT;

	stat->reboot_nb.notifier_call = exynos_ufs_reboot_handler;
	register_reboot_notifier(&stat->reboot_nb);

	stat->scale_wq = create_singlethread_workqueue("scale_wq");
	if (!stat->scale_wq)
		pr_err("%s: failed to create eh workqueue\n", __func__);

	INIT_WORK(&stat->gear_work, ufs_g_scale_handler);
	INIT_DELAYED_WORK(&stat->devfreq_work, exynos_get_devfreq_noti);
	schedule_delayed_work(&stat->devfreq_work, msecs_to_jiffies(10000));

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&stat->pm_qos_int,
				  PM_QOS_DEVICE_THROUGHPUT, 0);
#endif
}

MODULE_DESCRIPTION("Exynos UFS gear scale");
MODULE_AUTHOR("Hoyoung Seo <hy50.seo@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
