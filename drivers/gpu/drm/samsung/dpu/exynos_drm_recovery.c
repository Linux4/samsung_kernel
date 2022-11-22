// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_recovery.c
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 * Authors:
 *	Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define pr_fmt(fmt)  "[RECOVERY] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/export.h>
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_vblank.h>
#include "exynos_drm_decon.h"
#include "exynos_drm_recovery.h"
#include "exynos_drm_debug.h"

#define RECOVERY_RETRY_CNT 5

struct recovery_mapping {
	char name[RECOVERY_NAME_LEN];
	bool (*func)(const struct drm_crtc *crtc);
};

static int __set_panel_reset(struct decon_device *decon, bool reset);

__weak bool dsim_condition_check(const struct drm_crtc *crtc)
{
	return false;
}

__weak bool customer_condition_check(const struct drm_crtc *crtc)
{
	return false;
}

struct recovery_mapping recovery_map[RECOVERY_MODE_MAX] = {
	{"dsim", dsim_condition_check},
	{"customer", customer_condition_check},
};

const char recovery_state_string[6][RECOVERY_NAME_LEN] = {
	"not_supported", "idle", "trigger",
	"begin", "restore", "max_error"
};

static bool __rq_is_empty(struct recovery_queue *rq)
{
	return ((rq->front == rq->rear) ? true : false);
}

static bool __rq_is_full(struct recovery_queue *rq)
{
	return ((rq->front == ((rq->rear + 1) % rq->fsz)) ? true : false);
}

static ktime_t __rq_top(struct recovery_queue *rq)
{
	return __rq_is_empty(rq) ? ms_to_ktime(0) : rq->queue[rq->front];
}

static void __rq_delq(struct recovery_queue *rq)
{
	if (!__rq_is_empty(rq))
		rq->front = (rq->front + 1) % rq->fsz;
}

static void __rq_addq(struct recovery_queue *rq, ktime_t data)
{
	if (__rq_is_full(rq))
		__rq_delq(rq);

	rq->queue[rq->rear] = data;
	rq->rear = (rq->rear + 1) % rq->fsz;
}

static bool __verify_in_time(struct exynos_recovery_cond *cond)
{
	struct recovery_queue *rq;
	ktime_t ctime = ktime_get();

	if (!cond || cond->limits_num_in_time == 0 || cond->in_time == 0)
		return true;

	rq = &cond->rq;
	if (__rq_is_full(rq)) {
		ktime_t diff = ktime_sub(ctime, __rq_top(rq));

		if (ktime_compare(diff, cond->in_time) <= 0) {
			pr_info("not matched:%s:%dtimes in %lld ms, cond = %lld ms\n",
					cond->name, cond->limits_num_in_time,
					ktime_to_ms(diff),
					ktime_to_ms(cond->in_time));
			return false;
		}
	}
	__rq_addq(rq, ctime);
	return true;
}

static bool __verify_max_count(struct exynos_recovery_cond *cond)
{
	if (!cond || cond->max_recovery_count == 0)
		return true;

	if (cond->count >= cond->max_recovery_count)
		return false;
	else
		return true;
}

static inline bool is_primary_crtc(const struct drm_crtc *crtc)
{
	return ((drm_crtc_index(crtc) == 0) ? true : false);
}

static bool is_supported_mode(struct exynos_recovery *recovery)
{
	bool supported = false;

	mutex_lock(&recovery->r_lock);
	if (recovery->req_mode_id != RECOVERY_MODE_MAX)
		supported = true;
	mutex_unlock(&recovery->r_lock);

	return supported;
}

static bool exynos_recovery_condition_check(const struct drm_crtc *crtc,
				struct exynos_recovery *recovery)
{
	struct exynos_recovery_cond *cond;
	struct exynos_recovery_cond *cond_iter;
	int i;

	if (!crtc || !recovery) {
		pr_err("nullptr\n");
		return false;
	}

	if (recovery->req_mode_id >= RECOVERY_MODE_MAX) {
		pr_err("invalid req_mode_id\n");
		return false;
	}
	cond = &recovery->r_cond[recovery->req_mode_id];

	if (!strncmp(cond->name, "force", RECOVERY_NAME_LEN) ||
		!strncmp(cond->name, "always", RECOVERY_NAME_LEN)) {
		if (cond->func && !cond->func(crtc)) {
			pr_info("%s:not matched:cond-function\n", cond->name);
			return false;
		}
	} else {
		/* iterate all recovery conditions */
		for (i = 0; i < RECOVERY_MODE_MAX; i++) {
			cond_iter = &recovery->r_cond[i];
			if (!strncmp(cond_iter->name, "force", RECOVERY_NAME_LEN) ||
				!strncmp(cond_iter->name, "always", RECOVERY_NAME_LEN))
				continue;

			if (cond_iter->func && cond_iter->func(crtc)) {
				pr_info("set recovery condition to %s(%d)\n", cond_iter->name, i);
				cond = cond_iter;
				recovery->req_mode_id = i;
				break;
			}
		}
		if (i == RECOVERY_MODE_MAX)
			return false;
	}

	if (!__verify_max_count(cond)) {
		pr_info("%s:func matched, but over maximun retry\n", cond->name);
		return false;
	}

	if (!__verify_in_time(cond)) {
		pr_info("%s:func&max matched, but too manay recovery in time\n",
					cond->name);
		return false;
	}

	pr_debug("%s: passed!\n", cond->name);

	return true;
}

static struct drm_atomic_state *
exynos_recovery_atomic_helper_duplicate_state(struct drm_device *dev,
				  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	struct drm_crtc *primary_crtc = NULL;
	int err = 0;
	struct exynos_drm_crtc_state *exynos_crtc_state;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->acquire_ctx = ctx;
	state->duplicated = true;

	drm_for_each_crtc(crtc, dev) {
		struct drm_crtc_state *crtc_state;

		if (!is_primary_crtc(crtc))
			continue;

		primary_crtc = crtc;

		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			err = PTR_ERR(crtc_state);
			goto free;
		}

		exynos_crtc_state = to_exynos_crtc_state(crtc_state);

		exynos_crtc_state->dqe_fd = -1;
		exynos_crtc_state->reserved_win_mask = 0;
		exynos_crtc_state->visible_win_mask = 0;
	}

	drm_for_each_plane(plane, dev) {
		struct drm_plane_state *plane_state;

		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			err = PTR_ERR(plane_state);
			goto free;
		}
	}

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		struct drm_connector_state *conn_state;

		if (conn->state && primary_crtc != conn->state->crtc)
			continue;

		conn_state = drm_atomic_get_connector_state(state, conn);
		if (IS_ERR(conn_state)) {
			err = PTR_ERR(conn_state);
			drm_connector_list_iter_end(&conn_iter);
			goto free;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* clear the acquire context so that it isn't accidentally reused */
	state->acquire_ctx = NULL;

free:
	if (err < 0) {
		drm_atomic_state_put(state);
		state = ERR_PTR(err);
	}

	return state;
}

int exynos_recovery_atomic_helper_commit_duplicated_state(struct drm_atomic_state *state,
					      struct drm_modeset_acquire_ctx *ctx)
{
	int i, ret;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;

	state->acquire_ctx = ctx;

	for_each_new_plane_in_state(state, plane, new_plane_state, i)
		state->planes[i].old_state = plane->state;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (!is_primary_crtc(crtc))
			continue;

		state->crtcs[i].old_state = crtc->state;
	}

	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		state->connectors[i].old_state = connector->state;
	}

	ret = drm_atomic_commit(state);

	state->acquire_ctx = NULL;

	return ret;
}

static void exynos_recovery_handler(struct work_struct *work)
{
	struct exynos_recovery *recovery = container_of(work,
					struct exynos_recovery, work);
	struct decon_device *decon = container_of(recovery, struct decon_device,
					recovery);
	struct drm_device *dev = decon->drm_dev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state, *rcv_state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc = &decon->crtc->base;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	// To Do : find out & fix how to disable correctly.
	/*
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	*/
	int ret;
	u64 time_s, time_c;
	int retry = 0;

	if (exynos_recovery_get_state(decon) == RECOVERY_NOT_SUPPORTED)
		return;

	if (!is_primary_crtc(crtc)) {
		exynos_recovery_set_state(decon, RECOVERY_IDLE);
		return;
	}

	if (!is_supported_mode(recovery)) {
		exynos_recovery_set_state(decon, RECOVERY_IDLE);
		return;
	}

	if (!exynos_recovery_condition_check(crtc, recovery)) {
		if (recovery->always)
			pr_info("not matched:but always-recovery enabled\n");
		else {
			pr_info("recovery condition not matched\n");
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#ifdef CONFIG_DISPLAY_USE_INFO
			log_decon_bigdata(decon);
#endif
#endif
			dpu_dump_with_event(exynos_crtc);
			dbg_snapshot_expire_watchdog();
			BUG();
			return;
		}
	}

	DPU_EVENT_LOG(DPU_EVT_RECOVERY_START, exynos_crtc,
		&recovery->r_cond[recovery->req_mode_id]);

	if (recovery->r_cond[recovery->req_mode_id].refresh_panel)
		__set_panel_reset(decon, true);
	else
		__set_panel_reset(decon, false);

	time_s = ktime_to_us(ktime_get());
	pm_stay_awake(decon->dev);
	exynos_recovery_set_state(decon, RECOVERY_BEGIN);

	drm_modeset_acquire_init(&ctx, 0);

	rcv_state = exynos_recovery_atomic_helper_duplicate_state(dev, &ctx);
	if (IS_ERR(rcv_state)) {
		ret = PTR_ERR(rcv_state);
		goto out_drop_locks;
	}

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto out_drop_locks;
	}

retry:
	++retry;
	state->acquire_ctx = &ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}

	crtc_state->active = false;

	// To Do : find out & fix how to disable correctly.
	/*
	ret = drm_atomic_set_mode_prop_for_crtc(crtc_state, NULL);
	if (ret)
		goto out;

	ret = drm_atomic_add_affected_planes(state, crtc);
	if (ret)
		goto out;

	ret = drm_atomic_add_affected_connectors(state, crtc);
	if (ret)
		goto out;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		ret = drm_atomic_set_crtc_for_connector(conn_state, NULL);
		if (ret)
			goto out;
	}

	for_each_new_plane_in_state(state, plane, plane_state, i) {
		ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
		if (ret)
			goto out;
		drm_atomic_set_fb_for_plane(plane_state, NULL);
	}
	*/

	if (recovery->r_cond[recovery->req_mode_id].reset_vblank)
		drm_crtc_vblank_off(crtc);

	ret = drm_atomic_commit(state);
	if (ret)
		goto out;

	drm_mode_config_reset(dev);
	exynos_recovery_set_state(decon, RECOVERY_RESTORE);
	if (recovery->r_cond[recovery->req_mode_id].reset_vblank)
		drm_crtc_vblank_on(crtc);
	ret = exynos_recovery_atomic_helper_commit_duplicated_state(rcv_state, &ctx);
	if (ret)
		goto out;

	recovery->count++;
	recovery->r_cond[recovery->req_mode_id].count++;
	time_c = (ktime_to_us(ktime_get())-time_s);

	DPU_EVENT_LOG(DPU_EVT_RECOVERY_END, exynos_crtc,
			&recovery->r_cond[recovery->req_mode_id]);

	pr_info("recovery(#%d) is successfully finished (%lld.%03lldms)\n",
			recovery->count,
			time_c/USEC_PER_MSEC, time_c%USEC_PER_MSEC);

out:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		drm_atomic_state_clear(rcv_state);
		ret = drm_modeset_backoff(&ctx);
		if (!ret) {
			if (retry >= RECOVERY_RETRY_CNT) {
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#ifdef CONFIG_DISPLAY_USE_INFO
				log_decon_bigdata(decon);
#endif
#endif
				dpu_dump_with_event(exynos_crtc);
				dbg_snapshot_expire_watchdog();
				BUG();
			}
			goto retry;
		}
	}

	drm_atomic_state_put(state);
	drm_atomic_state_put(rcv_state);

out_drop_locks:
	exynos_recovery_set_state(decon, RECOVERY_IDLE);
	pm_relax(decon->dev);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

static void __set_recovery_state(struct exynos_recovery *recovery, enum recovery_state state)
{
	enum recovery_state from = recovery->r_state;

	mutex_lock(&recovery->r_lock);
	recovery->r_state = state;
	mutex_unlock(&recovery->r_lock);

	pr_debug("state changed (%s -> %s)\n", recovery_state_string[from],
			recovery_state_string[recovery->r_state]);
}

static enum recovery_state __get_recovery_state(struct exynos_recovery *recovery)
{
	enum recovery_state state;

	mutex_lock(&recovery->r_lock);
	state = recovery->r_state;
	mutex_unlock(&recovery->r_lock);

	return state;
}

static int __set_panel_reset(struct decon_device *decon, bool reset)
{
	struct drm_connector *conn;
	struct exynos_drm_connector_state *exynos_conn_state;

	if (!decon)
		return -EINVAL;

	conn = crtc_get_conn(decon->crtc->base.state, DRM_MODE_CONNECTOR_DSI);
	if (!conn)
		return -EINVAL;

	exynos_conn_state = to_exynos_connector_state(conn->state);
	if (!exynos_conn_state)
		return -EINVAL;

	exynos_conn_state->requested_panel_recovery = reset;

	return 0;
}

static void __set_recovery_mode(struct exynos_recovery *recovery, char *mode)
{
	int id;
	bool find = false;
	struct exynos_recovery_cond *cond;
	struct decon_device *decon = container_of(recovery, struct decon_device,
					recovery);

	mutex_lock(&recovery->r_lock);
	for (id = 0; id < recovery->modes; ++id) {
		cond = &recovery->r_cond[id];
		if (strncmp(cond->name, mode, RECOVERY_NAME_LEN) == 0) {
			find = true;
			break;
		}
	}

	if (!find)
		recovery->req_mode_id = RECOVERY_MODE_MAX;
	else {
		recovery->req_mode_id = id;
		if (cond->refresh_panel)
			__set_panel_reset(decon, true);
		else
			__set_panel_reset(decon, false);
	}
	mutex_unlock(&recovery->r_lock);
}

int exynos_recovery_set_state(struct decon_device *decon, enum recovery_state state)
{
	struct exynos_recovery *recovery;
	const struct exynos_recovery_funcs *funcs;

	if (!decon || state >= RECOVERY_SMAX)
		return -EINVAL;

	recovery = &decon->recovery;
	if (!recovery || !recovery->funcs)
		return -EINVAL;

	funcs = recovery->funcs;
	if (funcs)
		funcs->set_state(recovery, state);

	return 0;
}

enum recovery_state exynos_recovery_get_state(struct decon_device *decon)
{
	struct exynos_recovery *recovery;
	const struct exynos_recovery_funcs *funcs;
	enum recovery_state cstate = RECOVERY_NOT_SUPPORTED;

	if (!decon)
		return cstate;

	recovery = &decon->recovery;
	if (!recovery || !recovery->funcs)
		return cstate;

	funcs = recovery->funcs;
	if (funcs)
		cstate = funcs->get_state(recovery);

	return cstate;
}

int exynos_recovery_set_mode(struct decon_device *decon, char *mode)
{
	struct exynos_recovery *recovery;
	const struct exynos_recovery_funcs *funcs;

	if (!decon)
		return -EINVAL;

	recovery = &decon->recovery;
	if (!recovery || !recovery->funcs)
		return -EINVAL;

	funcs = recovery->funcs;
	if (funcs)
		funcs->set_mode(recovery, mode);

	return 0;
}

static const struct exynos_recovery_funcs recovery_funcs = {
	.set_state = __set_recovery_state,
	.get_state = __get_recovery_state,
	.set_mode = __set_recovery_mode,
};

static int __init_condition_params(struct exynos_recovery *recovery,
					int id, const char *name)
{
	struct exynos_recovery_cond *cond;

	if (!recovery)
		return -EINVAL;

	cond = &recovery->r_cond[id];
	cond->id = id;
	strncpy(&cond->name[0], name, sizeof(cond->name) - 1);
	cond->count = 0;
	cond->max_recovery_count = 0;
	cond->limits_num_in_time = 0;
	cond->in_time = ms_to_ktime(0);
	cond->refresh_panel = false;
	cond->reset_vblank = false;

	return 0;
}

static int exynos_recovery_parse_dt_sub(struct device_node *np,
					struct exynos_recovery *recovery,
					int id, const char *name)
{
	struct exynos_recovery_cond *cond;
	u32 data[2];
	int ret, i;

	if (!recovery)
		return -EINVAL;

	cond = &recovery->r_cond[id];
	cond->id = id;
	strncpy(&cond->name[0], name, RECOVERY_NAME_LEN - 1);

	of_property_read_u32_array(np, "max-limit", data, 2);
	if (WARN_ON(data[0] >= INTERNAL_MAX_QUEUE_SZ))
		data[0] = INTERNAL_MAX_QUEUE_SZ - 1;
	cond->limits_num_in_time = data[0];
	cond->in_time = ms_to_ktime((u64)data[1] * 60 * MSEC_PER_SEC);
	cond->rq.front = 0;
	cond->rq.rear = 0;
	cond->rq.fsz = (int)data[0] + 1;

	cond->count = 0;
	ret = of_property_read_u32(np, "max-recovery", &data[0]);
	if (ret == -EINVAL || (ret == 0 && data[0] == 0))
		cond->max_recovery_count = 0;
	else
		cond->max_recovery_count = data[0];

	cond->refresh_panel = of_property_read_bool(np, "refresh-panel");
	pr_info("refresh panel=%d\n", cond->refresh_panel);

	cond->reset_vblank = of_property_read_bool(np, "reset-vblank");
	pr_info("reset vblank=%d\n", cond->reset_vblank);

	for (i = 0; i < sizeof(recovery_map)/sizeof(struct recovery_mapping) ; ++i) {
		if (strncmp(cond->name, recovery_map[i].name, RECOVERY_NAME_LEN) == 0)
			cond->func = recovery_map[i].func;
	}

	return 0;
}

static int exynos_recovery_parse_dt(struct device_node *np,
					struct decon_device *decon)
{
	struct exynos_recovery *recovery = &decon->recovery;
	const char *name[RECOVERY_NAME_LEN];
	struct device_node *child_np, *cchild_np;
	int num_cond, i;

	num_cond = of_property_count_strings(np, "recovery-modes");
	if (num_cond < 0) {
		pr_err("failed to get recovery mode count\n");
		return -EINVAL;
	}

	num_cond = of_property_read_string_array(np, "recovery-modes",
				name, num_cond);
	if (num_cond < 0) {
		pr_err("failed to get recovery mode name\n");
		return -EINVAL;
	}
	recovery->modes = num_cond;

	child_np = of_get_child_by_name(np, "recovery-condition");
	if (!child_np) {
		for (i = 0; i < num_cond; ++i)
			__init_condition_params(recovery, i, name[i]);
		goto exit;
	} else {
		for (i = 0; i < num_cond; ++i) {
			cchild_np = of_get_child_by_name(child_np, name[i]);
			if (!cchild_np) {
				__init_condition_params(recovery, i, name[i]);
			} else {
				exynos_recovery_parse_dt_sub(cchild_np,
						recovery, i, name[i]);
			}
		}
	}

exit:
	for (i = 0; i < num_cond; ++i) {
		if (strncmp(name[i], "always", RECOVERY_NAME_LEN) == 0) {
			recovery->always = true;
			break;
		}
	}

	return 0;
}

void exynos_recovery_register(struct decon_device *decon)
{
	struct exynos_recovery *recovery = &decon->recovery;
	struct device *dev = decon->dev;
	struct device_node *np;
	int ret = 0;
	u32 val = 1;

	mutex_init(&recovery->r_lock);
	np = dev->of_node;
	ret = of_property_read_u32(np, "recovery-enable", &val);
	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		exynos_recovery_set_state(decon, RECOVERY_NOT_SUPPORTED);
		pr_debug("decon#%d:recovery not supported\n", decon->id);
		return;
	}

	if (exynos_recovery_parse_dt(np, decon) < 0) {
		exynos_recovery_set_state(decon, RECOVERY_NOT_SUPPORTED);
		return;
	}

	INIT_WORK(&recovery->work, exynos_recovery_handler);
	recovery->count = 0;
	recovery->funcs = &recovery_funcs;
	exynos_recovery_set_state(decon, RECOVERY_IDLE);
	pr_info("decon#%d recovery registered\n", decon->id);
}
EXPORT_SYMBOL(exynos_recovery_register);
