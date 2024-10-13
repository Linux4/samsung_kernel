// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_hibernation.c
 *
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 * Authors:
 *	Jiun Yu <jiun.yu@samsung.com>
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

#include <drm/drm_modeset_helper_vtables.h>
#include <exynos_drm_hibernation.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_partial.h>

#include "panel/panel-samsung-drv.h"

#define HIBERNATION_ENTRY_DEFAULT_FPS		60
#define HIBERNATION_ENTRY_MIN_TIME_MS		50
#define HIBERNATION_ENTRY_MIN_ENTRY_CNT		2

static int dpu_hiber_log_level = 6;
module_param(dpu_hiber_log_level, int, 0600);
MODULE_PARM_DESC(dpu_hiber_log_level, "log level for hibernation [default : 6]");

#define HIBER_NAME "exynos-hiber"
#define hiber_info(fmt, ...)	\
	dpu_pr_info(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

#define hiber_warn(fmt, ...)	\
	dpu_pr_warn(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

#define hiber_err(fmt, ...)	\
	dpu_pr_err(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

#define hiber_debug(fmt, ...)	\
	dpu_pr_debug(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

static bool is_dsr_operating(struct exynos_hibernation *hiber)
{
	struct drm_crtc_state *crtc_state = hiber->decon->crtc->base.state;
	struct exynos_drm_crtc_state *exynos_crtc_state;

	if (!crtc_state)
		return false;

	exynos_crtc_state = to_exynos_crtc_state(crtc_state);

	return exynos_crtc_state->dsr_status;
}

static bool is_dim_operating(struct exynos_hibernation *hiber)
{
	const struct decon_device *decon = hiber->decon;

	if (!decon)
		return false;

	return decon->dimming;
}

int exynos_hibernation_queue_exit_work(struct exynos_drm_crtc *exynos_crtc)
{
	if (!exynos_crtc->hibernation || !exynos_crtc->hibernation->early_wakeup_enable)
		return -EPERM;

	kthread_queue_work(&exynos_crtc->hibernation->exit_worker,
			&exynos_crtc->hibernation->exit_work);

	return 0;
}

static ssize_t exynos_show_hibernation_exit(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct exynos_drm_crtc *exynos_crtc = decon->crtc;
	char *p = buf;
	int len = 0;

	hiber_debug("+\n");

	/* If decon is OFF state, just return here */
	if (decon->state == DECON_STATE_OFF)
		return len;

	if (exynos_hibernation_queue_exit_work(exynos_crtc))
		return len;

	len = sprintf(p, "%d\n", ++exynos_crtc->hibernation->early_wakeup_cnt);
	hiber_debug("-\n");
	return len;
}
static DEVICE_ATTR(hiber_exit, S_IRUGO, exynos_show_hibernation_exit, NULL);

void hibernation_trig_reset(struct exynos_hibernation *hiber)
{
	struct drm_crtc_state *crtc_state;
	unsigned int fps = HIBERNATION_ENTRY_DEFAULT_FPS;
	int entry_cnt;

	if (!hiber || !hiber->decon->crtc)
		return;

	crtc_state = hiber->decon->crtc->base.state;
	if (crtc_state)
		fps = drm_mode_vrefresh(&crtc_state->mode);

	entry_cnt = (fps <= hiber->min_entry_fps) ?
		HIBERNATION_ENTRY_MIN_ENTRY_CNT :
		DIV_ROUND_UP(fps * HIBERNATION_ENTRY_MIN_TIME_MS, MSEC_PER_SEC);

	if (entry_cnt < HIBERNATION_ENTRY_MIN_ENTRY_CNT)
		entry_cnt = HIBERNATION_ENTRY_MIN_ENTRY_CNT;

	atomic_set(&hiber->trig_cnt, entry_cnt);
}

static bool exynos_hibernation_check(struct exynos_hibernation *hiber)
{
	hiber_debug("+\n");

	return (!is_hibernaton_blocked(hiber) && !is_dsr_operating(hiber) &&
		!is_dim_operating(hiber) && atomic_dec_and_test(&hiber->trig_cnt));
}

static bool needs_self_refresh_change(struct drm_crtc_state *old_crtc_state, bool sr_active)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(old_crtc_state->crtc);

	if (sr_active && atomic_read(&exynos_crtc->hibernation->trig_cnt) > 0)
		return false;

	if (!old_crtc_state->enable) {
		hiber_debug("skipping due to crtc disabled\n");
		return false;
	}

	if ((old_crtc_state->self_refresh_active == sr_active) ||
			(old_crtc_state->active == !sr_active)) {
		hiber_debug("skipping due to active=%d sr_active=%d, requested sr_active=%d\n",
				old_crtc_state->active, old_crtc_state->self_refresh_active,
				sr_active);
		return false;
	}

	return true;
}

static int exynos_crtc_self_refresh_update(struct drm_crtc *crtc, bool sr_active)
{
	struct drm_device *dev = crtc->dev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state;
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_crtc_state *crtc_state, *old_crtc_state;
	struct exynos_drm_crtc_state *exynos_crtc_state;
	int i, ret = 0;
	ktime_t start = ktime_get();

	drm_modeset_acquire_init(&ctx, 0);

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto out_drop_locks;
	}

retry:
	state->acquire_ctx = &ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}

	old_crtc_state = drm_atomic_get_old_crtc_state(state, crtc);
	if (!needs_self_refresh_change(old_crtc_state, sr_active))
		goto out;

	crtc_state->active = !sr_active;
	crtc_state->self_refresh_active = sr_active;
	exynos_crtc_state = to_exynos_crtc_state(crtc_state);
	exynos_crtc_state->skip_update = !sr_active;

	ret = drm_atomic_add_affected_connectors(state, crtc);
	if (ret)
		goto out;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		if (!conn_state->self_refresh_aware)
			goto out;
	}

	ret = drm_atomic_commit(state);
	if (ret)
		goto out;

out:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		ret = drm_modeset_backoff(&ctx);
		if (!ret)
			goto retry;
	}

	drm_atomic_state_put(state);

out_drop_locks:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	if (!ret) {
		ktime_t delta = ktime_sub(ktime_get(), start);

		if (ktime_to_us(delta) > 8000)
			hiber_debug("%s took %lldus\n", sr_active ? "on" : "off",
					ktime_to_us(delta));
	} else if (ret == -EBUSY) {
		hiber_info("aborted due to normal commit pending\n");
	} else {
		hiber_warn("unable to enter self refresh (%d)\n", ret);
	}

	return ret;
}

static void exynos_hibernation_enter(struct exynos_hibernation *hiber)
{
	struct decon_device *decon = hiber->decon;
	struct exynos_drm_crtc *exynos_crtc;
	int ret;

	hiber_debug("+\n");

	if (!decon)
		return;

	exynos_crtc = decon->crtc;

	mutex_lock(&hiber->lock);
	hibernation_block(hiber);

	DPU_ATRACE_BEGIN(__func__);

	DPU_EVENT_LOG("ENTER_HIBERNATION_IN", decon->crtc, 0, "DPU POWER %s",
			pm_runtime_active(exynos_crtc->dev) ? "ON" : "OFF");

	ret = exynos_crtc_self_refresh_update(&exynos_crtc->base, true);
	if (ret) {
		hiber_err("failed to commit self refresh\n");
		goto out;
	}

	DPU_EVENT_LOG("ENTER_HIBERNATION_OUT", decon->crtc, 0, "DPU POWER %s",
			pm_runtime_active(exynos_crtc->dev) ? "ON" : "OFF");

	dpu_profile_hiber_enter(exynos_crtc);

	DPU_ATRACE_END(__func__);
out:
	hibernation_unblock(hiber);
	mutex_unlock(&hiber->lock);

	hiber_debug("DPU power %s -\n",	pm_runtime_active(decon->dev) ? "on" : "off");
}

static void exynos_hibernation_exit(struct exynos_hibernation *hiber)
{
	struct decon_device *decon = hiber->decon;
	struct exynos_drm_crtc *exynos_crtc;
	int ret;

	hiber_debug("+\n");

	if (!decon)
		return;

	exynos_crtc = decon->crtc;
	hibernation_block(hiber);

	/*
	 * Cancel and/or wait for finishing previous queued hibernation entry work. It only
	 * goes to sleep when work is currently executing. If not, there is no operation here.
	 */
	kthread_cancel_work_sync(&hiber->work);

	mutex_lock(&hiber->lock);

	DPU_ATRACE_BEGIN(__func__);

	DPU_EVENT_LOG("EXIT_HIBERNATION_IN", decon->crtc, 0, "DPU POWER %s",
			pm_runtime_active(exynos_crtc->dev) ? "ON" : "OFF");

	ret = exynos_crtc_self_refresh_update(&exynos_crtc->base, false);
	if (ret) {
		hiber_err("failed to commit self refresh\n");
		goto out;
	}

	DPU_EVENT_LOG("EXIT_HIBERNATION_OUT", decon->crtc, 0, "DPU POWER %s",
			pm_runtime_active(exynos_crtc->dev) ? "ON" : "OFF");
	DPU_ATRACE_END(__func__);
	dpu_profile_hiber_exit(exynos_crtc);
out:
	mutex_unlock(&hiber->lock);
	hibernation_unblock(hiber);

	hiber_debug("DPU power %s -\n", pm_runtime_active(decon->dev) ? "on" : "off");
}

void hibernation_block_exit(struct exynos_hibernation *hiber)
{
	if (!hiber)
		return;

	hibernation_block(hiber);
	exynos_hibernation_exit(hiber);
}

static void exynos_hibernation_handler(struct kthread_work *work)
{
	struct exynos_hibernation *hibernation =
		container_of(work, struct exynos_hibernation, work);

	hiber_debug("Display hibernation handler is called(trig_cnt:%d)\n",
			atomic_read(&hibernation->trig_cnt));

	/* If hibernation entry condition does NOT meet, just return here */
	if (!exynos_hibernation_check(hibernation))
		return;

	exynos_hibernation_enter(hibernation);
}

static void exynos_hibernation_exit_handler(struct kthread_work *work)
{
	struct exynos_hibernation *hibernation =
		container_of(work, struct exynos_hibernation, exit_work);

	hiber_debug("Display hibernation exit handler is called\n");

	exynos_hibernation_exit(hibernation);
}

static ssize_t hiber_enter_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", decon->state);

	return ret;
}

static ssize_t hiber_enter_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct exynos_drm_crtc *exynos_crtc = decon->crtc;
	bool enabled;
	int ret;

	ret = kstrtobool(buf, &enabled);
	if (ret) {
		hiber_err("invalid hiber enter value\n");
		return ret;
	}

	if (enabled)
		exynos_hibernation_enter(exynos_crtc->hibernation);
	else
		exynos_hibernation_exit(exynos_crtc->hibernation);

	return len;
}
static DEVICE_ATTR_RW(hiber_enter);

struct exynos_hibernation *
exynos_hibernation_register(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct device_node *np;
	struct exynos_hibernation *hibernation;
	struct device *dev = decon->dev;
	struct sched_param param;
	int ret = 0;
	u32 val = 1;

	np = dev->of_node;
	ret = of_property_read_u32(np, "hibernation", &val);
	/* if ret == -EINVAL, property is not existed */
	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		hiber_info("display hibernation is not supported\n");
		return NULL;
	}

	if (decon->emul_mode)
		return NULL;

	hibernation = devm_kzalloc(dev, sizeof(struct exynos_hibernation),
			GFP_KERNEL);
	if (!hibernation)
		return NULL;

	hibernation->decon = decon;

	ret = of_property_read_u32(np, "hibernation-min-entry-fps", &val);
	if (ret == -EINVAL || (ret == 0 && val >= HIBERNATION_ENTRY_DEFAULT_FPS))
		hibernation->min_entry_fps = 0;
	else
		hibernation->min_entry_fps = val;

	mutex_init(&hibernation->lock);

	hibernation_trig_reset(hibernation);

	atomic_set(&hibernation->block_cnt, 0);

	kthread_init_work(&hibernation->work, exynos_hibernation_handler);

	hiber_info("display hibernation is supported\n");

	/* register asynchronous hibernation early wakeup feature */
	hibernation->early_wakeup_enable = false;
	if (!IS_ENABLED(CONFIG_DRM_SAMSUNG_HIBERNATION_EARLY_WAKEUP) || (decon->id)) {
		hiber_info("hibernation early wakeup is disabled\n");
		return hibernation;
	}

	/* initialize exit hibernation thread */
	kthread_init_worker(&hibernation->exit_worker);
	hibernation->exit_thread = kthread_run(kthread_worker_fn,
			&hibernation->exit_worker, "exynos_hibernation_exit");
	if (IS_ERR(hibernation->exit_thread)) {
		hibernation->exit_thread = NULL;
		hiber_err("failed to run exit hibernation thread\n");
		return hibernation;
	}
	param.sched_priority = 20;
	sched_setscheduler_nocheck(hibernation->exit_thread, SCHED_FIFO, &param);
	kthread_init_work(&hibernation->exit_work, exynos_hibernation_exit_handler);
	hibernation->early_wakeup_cnt = 0;
	ret = device_create_file(decon->dev, &dev_attr_hiber_exit);
	ret = device_create_file(decon->dev, &dev_attr_hiber_enter);
	if (ret) {
		hiber_err("failed to create hiber exit file\n");
		return hibernation;
	}
	hibernation->early_wakeup_enable = true;

	hiber_info("hibernation early wakeup is enabled\n");

	return hibernation;
}
