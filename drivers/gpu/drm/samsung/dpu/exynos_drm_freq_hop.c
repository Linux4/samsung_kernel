// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_freq_hop.c
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 * Authors:
 *	Hwangjae Lee <hj-yo.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <uapi/linux/sched/types.h>

#include <dpu_trace.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_dsim.h>

static inline bool is_freq_hop_enabled(const struct dsim_device *dsim)
{
	if (!dsim)
		return false;

	if (!dsim->freq_hop || !dsim->freq_hop->enabled)
		return false;

	return true;
}

static int dsim_set_freq_hop(struct dsim_device *dsim,
		const struct dsim_freq_hop *freq, bool en)
{
	struct stdphy_pms *pms;

	if (dsim->state != DSIM_STATE_HSCLKEN) {
		pr_info("dsim power is off state(0x%x)\n", dsim->state);
		return -EINVAL;
	}

	pms = &dsim->config.dphy_pms;
	dsim_reg_set_dphy_freq_hopping(dsim->id, pms->p, freq->target_m,
			freq->target_k,	en);

	return 0;
}

void dpu_update_freq_hop(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_freq_hop *freq_hop;

	if (!is_freq_hop_enabled(dsim))
		return;

	freq_hop = dsim->freq_hop;
	mutex_lock(&freq_hop->lock);
	freq_hop->target_m = freq_hop->request_m;
	freq_hop->target_k = freq_hop->request_k;
	mutex_unlock(&freq_hop->lock);
}

void dpu_set_freq_hop(struct exynos_drm_crtc *exynos_crtc, bool en)
{
	struct stdphy_pms *pms;
	struct decon_device *decon = exynos_crtc->ctx;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_freq_hop *freq_hop;
	u32 target_m, target_k;

	if (!is_freq_hop_enabled(dsim))
		return;

	freq_hop = dsim->freq_hop;
	target_m = freq_hop->target_m;
	target_k = freq_hop->target_k;

	pms = &dsim->config.dphy_pms;
	if ((pms->m != target_m) || (pms->k != target_k)) {
		if (en) {
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			/* wakeup PLL if sleeping... */
			decon_reg_set_pll_wakeup(decon->id, true);
			decon_reg_set_pll_sleep(decon->id, false);
#endif
			dsim_set_freq_hop(dsim, freq_hop, true);
		} else {
			pms->m = freq_hop->target_m;
			pms->k = freq_hop->target_k;
			pr_info("%s: en(%d), pmsk[%d %d %d %d]\n", __func__,
					en, pms->p, pms->m, pms->s, pms->k);
			dsim_set_freq_hop(dsim, freq_hop, false);
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			decon_reg_set_pll_sleep(decon->id, true);
#endif
		}
	}
}

struct dpu_freq_hop_ops dpu_freq_hop_control = {
	.set_freq_hop	 = dpu_set_freq_hop,
	.update_freq_hop = dpu_update_freq_hop,
};

static int dpu_debug_freq_hop_show(struct seq_file *s, void *unused)
{
	struct decon_device *decon = s->private;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_freq_hop *freq_hop;

	if (!is_freq_hop_enabled(dsim))
		return 0;

	freq_hop = dsim->freq_hop;
	mutex_lock(&freq_hop->lock);
	seq_printf(s, "m(%u) k(%u)\n", freq_hop->request_m, freq_hop->request_k);
	mutex_unlock(&freq_hop->lock);

	return 0;
}

static int dpu_debug_freq_hop_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_debug_freq_hop_show, inode->i_private);
}

static ssize_t dpu_debug_freq_hop_write(struct file *file,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_freq_hop *freq_hop;
	char *buf_data;
	int ret;

	if (!is_freq_hop_enabled(dsim))
		return count;

	if (!count)
		return count;

	buf_data = kmalloc(count, GFP_KERNEL);
	if (buf_data == NULL)
		return count;

	memset(buf_data, 0, count);

	ret = copy_from_user(buf_data, buf, count);
	if (ret < 0)
		goto out;

	freq_hop = dsim->freq_hop;
	mutex_lock(&freq_hop->lock);
	ret = sscanf(buf_data, "%u %u", &freq_hop->request_m, &freq_hop->request_k);
	mutex_unlock(&freq_hop->lock);
	if (ret < 0)
		goto out;

out:
	kfree(buf_data);
	return count;
}

static const struct file_operations dpu_freq_hop_fops = {
	.open = dpu_debug_freq_hop_open,
	.write = dpu_debug_freq_hop_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

void dpu_freq_hop_debugfs(struct exynos_drm_crtc *exynos_crtc)
{
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debug_request_mk;
	struct decon_device *decon = exynos_crtc->ctx;
	struct drm_crtc *crtc = &exynos_crtc->base;

	decon->crtc->freq_hop = &dpu_freq_hop_control;
	debug_request_mk = debugfs_create_file("request_mk", 0444,
			crtc->debugfs_entry, decon, &dpu_freq_hop_fops);

	if (!debug_request_mk) {
		pr_err("failed to create debugfs debugfs request_mk directory\n");
	}
#endif
}

void dpu_init_freq_hop(struct dsim_device *dsim)
{
	struct stdphy_pms *pms;
	struct dsim_freq_hop *freq_hop = dsim->freq_hop;
	const struct dsim_pll_params *pll_params = dsim->pll_params;
	const struct dsim_pll_param *p;

	if (!freq_hop)
		return;

	if (dsim->config.mode == DSIM_VIDEO_MODE) {
		freq_hop->enabled = false;
		return;
	}

	if (pll_params->curr_idx < 0)
		return;

	p = &pll_params->params[pll_params->curr_idx];
	freq_hop->target_m = p->m;
	freq_hop->target_k = p->k;

	pms = &dsim->config.dphy_pms;
	if ((pms->m != freq_hop->target_m) || (pms->k != freq_hop->target_k)) {
		pms->m = freq_hop->target_m;
		pms->k = freq_hop->target_k;
	}

	if (freq_hop->enabled == false) {
		mutex_lock(&freq_hop->lock);
		freq_hop->request_m = p->m;
		freq_hop->request_k = p->k;
		mutex_unlock(&freq_hop->lock);
		freq_hop->enabled = true;
	}
}

struct dsim_freq_hop *dpu_register_freq_hop(struct dsim_device *dsim)
{
	struct dsim_freq_hop *freq_hop;
	const struct device_node *np = dsim->dev->of_node;
	int ret;
	u32 val = 1;

	ret = of_property_read_u32(np, "frequency-hopping", &val);
	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		pr_info("freq-hop feature feature is not supported\n");
		return NULL;
	}

	freq_hop = devm_kzalloc(dsim->dev, sizeof(struct dsim_freq_hop), GFP_KERNEL);
	if (!freq_hop) {
		pr_err("failed to alloc freq_hop");
		return NULL;
	}
	mutex_init(&freq_hop->lock);

	return freq_hop;
}
