// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "sapu_plat.h"

static void free_ref_cnt(struct kref *kref)
{
	pr_info("%s : kref ref_count reset\n", __func__);
	kref_init(kref);
}

static int sapu_lock_ipi_send(uint32_t lock, struct kref *ref_cnt)
{
	int i, ret = 0, retry_cnt = 50;
	unsigned int sapu_lock_ref_cnt;
	struct sapu_power_ctrl power_ctrl;
	struct sapu_lock_rpmsg_device *sapu_lock_rpm_dev;
	unsigned long long jiffies_start = 0, jiffies_end = 0;

	sapu_lock_rpm_dev = get_rpm_dev();

	if (!sapu_lock_rpm_dev->ept) {
		pr_info("%s: sapu_lock_rpm_dev.ept == NULL\n", __func__);
		return -ENXIO;
	}

	mutex_lock(get_rpm_mtx());

	if (lock) {
		kref_get(ref_cnt);
	} else {
		if (kref_put(ref_cnt, free_ref_cnt)) {
			ret = -EPERM;
			goto unlock_and_ret;
		}
	}

	sapu_lock_ref_cnt = kref_read(ref_cnt);
	pr_info("%s: lock = %d, sapu_lock_ref_cnt = %d\n",
			__func__, lock, sapu_lock_ref_cnt);

	/**
	 * sapu_lock_ref_cnt = 0 : X
	 * sapu_lock_ref_cnt = 1 : power unlock
	 * sapu_lock_ref_cnt = 2 : power lock
	 */
	if ((lock == 0 && sapu_lock_ref_cnt == 1)
		|| (lock != 0 && sapu_lock_ref_cnt == 2)) {

		power_ctrl.lock = sapu_lock_ref_cnt - 1;
		pr_info("%s: data.lock = %d\n", __func__, power_ctrl.lock);

		for (i = 0; i < retry_cnt; i++) {
			ret = rpmsg_send(sapu_lock_rpm_dev->ept,
							&power_ctrl, sizeof(power_ctrl));

			/* send busy, retry */
			if (ret == -EBUSY || ret == -EAGAIN) {
				pr_info("%s: re-send ipi(retry_cnt = %d)\n",
							__func__, retry_cnt);
				if (ret == -EBUSY)
					usleep_range(10000, 11000);
				else if (ret == -EAGAIN && i < 10)
					usleep_range(200, 500);
				else if (ret == -EAGAIN && i < 25)
					usleep_range(1000, 2000);
				else if (ret == -EAGAIN)
					usleep_range(10000, 11000);
				continue;
			}
			break;
		}

		/* only need to wait when ipi send success */
		if (ret == 0) {
			/* wait for receiving ack
			 * to ensure uP clear irq status done
			 */
			jiffies_start = get_jiffies_64();

			ret = wait_for_completion_timeout(
					&sapu_lock_rpm_dev->ack,
					msecs_to_jiffies(10000));

			jiffies_end = get_jiffies_64();

			if (likely(jiffies_end > jiffies_start)) {
				if (unlikely((jiffies_end-jiffies_start) > msecs_to_jiffies(100)))
					pr_info("completion ack is overtime => %u ms\n",
						jiffies_to_msecs(jiffies_end-jiffies_start));
			} else {
				pr_info("tick timer's value overflow\n");
			}

			if (ret == 0) {
				pr_info("%s: wait for completion timeout\n",
						__func__);
				ret = -EBUSY;
			} else {
				ret = 0;
			}
		}
	}

unlock_and_ret:
	mutex_unlock(get_rpm_mtx());
	return ret;
}

int power_ctrl_v1(struct sapu_private *sapu, u32 power_on)
{
	int ret = 0;
	struct platform_device *pdev;

	if (!sapu) {
		pr_info("[%s] sapu misc dev is NULL, return fail\n", __func__);
		return ret;
	}

	pdev = sapu->pdev;
	if (!pdev) {
		pr_info("[%s] sapu plat dev is NULL, return fail\n", __func__);
		return ret;
	}

	/**
	 * lock : ipi_send first, then smc to change state
	 * unlock : smc to change state first, then ipi_send
	 */
	if (power_on) {
		ret = sapu_lock_ipi_send(1, &sapu->lock_ref_cnt);
		if (ret) {
			pr_info("[%s]sapu_lock_ipi_send return(0x%x)\n",
							__func__, ret);
			return ret;
		}

		ret = trusty_std_call32(pdev->dev.parent,
				MTEE_SMCNR(MT_SMCF_SC_VPU,
				pdev->dev.parent),
				0, 1, 0);
		if (ret) {
			pr_info("[%s]trusty_std_call32 fail(0x%x), reset the power lock\n",
					__func__, ret);

			ret = sapu_lock_ipi_send(0, &sapu->lock_ref_cnt);
			if (ret) {
				pr_info("[%s]sapu_lock_ipi_send return(0x%x), reset failed\n",
						__func__, ret);
			}
			return ret;
		}
	} else {
		ret = trusty_std_call32(pdev->dev.parent,
				MTEE_SMCNR(MT_SMCF_SC_VPU,
				pdev->dev.parent),
				0, 0, 0);
		if (ret) {
			pr_info("[%s]trusty_std_call32 fail(0x%x)\n",
				__func__, ret);
			return ret;
		}

		ret = sapu_lock_ipi_send(0, &sapu->lock_ref_cnt);
		if (ret) {
			pr_info("[%s]sapu_lock_ipi_send return(0x%x)\n",
				__func__, ret);
			return ret;
		}
	}

	return ret;
}
