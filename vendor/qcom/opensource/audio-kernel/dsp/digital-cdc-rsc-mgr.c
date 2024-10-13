// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/ratelimit.h>
#include <dsp/digital-cdc-rsc-mgr.h>
#include <linux/dev_printk.h>

struct mutex hw_vote_lock;
static bool is_init_done;

/**
 * digital_cdc_rsc_mgr_hw_vote_enable - Enables hw vote in DSP
 *
 * @vote_handle: vote handle for which voting needs to be done
 * @dev: indicate which device votes
 *
 * Returns 0 on success or -EINVAL/error code on failure
 */
/*M55 code for QN6887A-5087 by tangjie at 2023/1/24 start*/
int digital_cdc_rsc_mgr_hw_vote_enable(struct clk *vote_handle, struct device *dev)
{
	int ret = 0;

	if (!is_init_done || vote_handle == NULL) {
		pr_err_ratelimited("%s: init failed or vote handle NULL\n",
				   __func__);
		return -EINVAL;
	}

	mutex_lock(&hw_vote_lock);
	ret = clk_prepare_enable(vote_handle);
	mutex_unlock(&hw_vote_lock);

	dev_dbg(dev, "%s: return %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(digital_cdc_rsc_mgr_hw_vote_enable);
/*M55 code for QN6887A-5087 by tangjie at 2023/1/24 end*/
/**
 * digital_cdc_rsc_mgr_hw_vote_disable - Disables hw vote in DSP
 *
 * @vote_handle: vote handle for which voting needs to be disabled
 * @dev: indicate which device unvotes
 *
 */
/*M55 code for QN6887A-5087 by tangjie at 2023/1/24 start*/
void digital_cdc_rsc_mgr_hw_vote_disable(struct clk *vote_handle, struct device *dev)
{
	if (!is_init_done || vote_handle == NULL) {
		pr_err_ratelimited("%s: init failed or vote handle NULL\n",
				   __func__);
		return;
	}

	mutex_lock(&hw_vote_lock);
	clk_disable_unprepare(vote_handle);
	mutex_unlock(&hw_vote_lock);
	dev_dbg(dev, "%s: leave\n", __func__);
}
EXPORT_SYMBOL(digital_cdc_rsc_mgr_hw_vote_disable);

/**
 * digital_cdc_rsc_mgr_hw_vote_reset - Resets hw vote count
 *
 */
void digital_cdc_rsc_mgr_hw_vote_reset(struct clk* vote_handle)
{
	int count = 0;

	if (!is_init_done || vote_handle == NULL) {
		pr_err_ratelimited("%s: init failed or vote handle NULL\n",
				   __func__);
		return;
	}

	mutex_lock(&hw_vote_lock);
	while (__clk_is_enabled(vote_handle)) {
		clk_disable_unprepare(vote_handle);
		count++;
	}
	pr_debug("%s: Vote count after SSR: %d\n", __func__, count);

	while (count--)
		clk_prepare_enable(vote_handle);
	mutex_unlock(&hw_vote_lock);
}
EXPORT_SYMBOL(digital_cdc_rsc_mgr_hw_vote_reset);

void digital_cdc_rsc_mgr_init(void)
{
	mutex_init(&hw_vote_lock);
	is_init_done = true;
}

void digital_cdc_rsc_mgr_exit(void)
{
	mutex_destroy(&hw_vote_lock);
	is_init_done = false;
}
/*M55 code for QN6887A-5087 by tangjie at 2023/1/24 end*/