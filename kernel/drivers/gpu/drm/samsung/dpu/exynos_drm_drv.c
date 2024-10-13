// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/dma-fence.h>
#include <linux/sort.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_vblank.h>
#include <drm/drm_managed.h>
#include <drm/drm_blend.h>
#include <drm/drm_bridge.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_self_refresh_helper.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_fbdev.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_encoder.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_writeback.h>
#include <exynos_display_common.h>
#include <exynos_drm_profiler.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_partial.h>
#include <exynos_drm_tui.h>
#include <exynos_drm_recovery.h>
#include <exynos_drm_hibernation.h>
#include <exynos_drm_encoder.h>
#include <exynos_drm_bridge.h>

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <mcd_drm_helper.h>
#include <linux/delay.h>
#endif

#define CREATE_TRACE_POINTS
#include <dpu_trace.h>

#define DRIVER_NAME	"exynos-drmdpu"
#define DRIVER_DESC	"Samsung SoC DRM"
#define DRIVER_DATE	"20110530"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static int dpu_log_level = 6;
module_param(dpu_log_level, int, 0600);
MODULE_PARM_DESC(dpu_log_level, "log level for dpu [default : 6]");

#define dpu_info(drm, fmt, ...)	\
dpu_pr_info(drv_name((drm)), 0, dpu_log_level, fmt, ##__VA_ARGS__)

#define dpu_warn(drm, fmt, ...)	\
dpu_pr_warn(drv_name((drm)), 0, dpu_log_level, fmt, ##__VA_ARGS__)

#define dpu_err(drm, fmt, ...)	\
dpu_pr_err(drv_name((drm)), 0, dpu_log_level, fmt, ##__VA_ARGS__)

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
int no_display;
EXPORT_SYMBOL(no_display);
module_param(no_display, int, 0444);
unsigned int drm_debug;
module_param(drm_debug, int, 0444);
int bypass_display;
EXPORT_SYMBOL(bypass_display);
int commit_retry;
EXPORT_SYMBOL(commit_retry);
#endif

#define dpu_debug(drm, fmt, ...)	\
dpu_pr_debug(drv_name((drm)), 0, dpu_log_level, fmt, ##__VA_ARGS__)

#define for_each_crtc_in_state(__state, crtc, __i)			\
	for ((__i) = 0;							\
	     (__i) < (__state)->dev->mode_config.num_crtc;		\
	     (__i)++)							\
		for_each_if ((__state)->crtcs[__i].ptr &&		\
			     ((crtc) = (__state)->crtcs[__i].ptr,	\
		/* Only to avoid unused-but-set-variable warning */	\
			     (void)(crtc), 1))

int exynos_drm_replace_property_blob_from_id(struct drm_device *dev,
					     struct drm_property_blob **blob,
					     uint64_t blob_id,
					     ssize_t expected_size)
{
	struct drm_property_blob *new_blob = NULL;

	if (blob_id != 0) {
		new_blob = drm_property_lookup_blob(dev, blob_id);
		if (!new_blob)
			return -EINVAL;

		if (new_blob->length != expected_size) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
	}

	drm_property_replace_blob(blob, new_blob);
	drm_property_blob_put(new_blob);

	return 0;
}

struct exynos_drm_priv_state *
exynos_drm_get_priv_state(struct drm_atomic_state *state)
{
	struct exynos_drm_private *priv = state->dev->dev_private;
	struct drm_private_state *priv_state;

	priv_state = drm_atomic_get_private_obj_state(state, &priv->obj);
	if (IS_ERR(priv_state))
		return ERR_CAST(priv_state);

	return to_exynos_priv_state(priv_state);
}

static unsigned int find_set_bits_mask(unsigned int mask, size_t count)
{
	unsigned int out = 0;
	int i;

	for (i = count; i > 0; i--) {
		int bit = ffs(mask) - 1;

		if (bit < 0)
			return 0;

		mask &= ~BIT(bit);
		out |= BIT(bit);
	}

	return out;
}

static unsigned int find_set_bits_mask_reverse_order(unsigned int mask, size_t count)
{
	unsigned int out = 0;
	int i;

	for (i = count; i > 0; i--) {
		int bit = fls(mask) - 1;

		if (bit < 0)
			return 0;

		mask &= ~BIT(bit);
		out |= BIT(bit);
	}

	return out;
}

static unsigned int exynos_drm_crtc_get_win_cnt(struct drm_crtc_state *crtc_state)
{
	unsigned int num_planes;
	const struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc_state->crtc);

	if (!crtc_state->active)
		return 0;

	num_planes = hweight32(crtc_state->plane_mask &
			       ~exynos_crtc->rcd_plane_mask);

	/* at least one window is required for color map, if active
	   and there are no planes on DSI path*/
	if (crtc_needs_colormap(crtc_state))
		num_planes = 1;

	return num_planes;
}

static void
exynos_drm_crtc_alloc_windows(struct drm_device *dev, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct exynos_drm_private *priv;
	unsigned int win_mask;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);

		hibernation_trig_reset(exynos_crtc->hibernation);
		if(!new_exynos_crtc_state->win_inserted)
			continue;

		/* allocate windows after swap state phase */
		win_mask = new_exynos_crtc_state->reserved_win_mask;
		priv = dev->dev_private;
		mutex_lock(&priv->lock);
		priv->available_win_mask &= ~win_mask;
		mutex_unlock(&priv->lock);
	}
}

static int
exynos_atomic_check_windows(struct drm_device *dev, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	unsigned int win_mask;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		struct exynos_drm_crtc_state *new_exynos_crtc_state;
		struct exynos_drm_crtc_state *old_exynos_crtc_state;
		unsigned int old_win_cnt, new_win_cnt;

		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_crtc_state = to_exynos_crtc_state(old_crtc_state);

		old_win_cnt = old_crtc_state->active ? hweight32(old_exynos_crtc_state->reserved_win_mask) : 0;
		new_win_cnt = exynos_drm_crtc_get_win_cnt(new_crtc_state);

		if (old_win_cnt == new_win_cnt)
			continue;

		pr_debug("%s: win cnt changed: %d -> %d\n", crtc->name, old_win_cnt, new_win_cnt);

		if (old_win_cnt > new_win_cnt) {
			const unsigned int curr_win_mask = new_exynos_crtc_state->reserved_win_mask;

			if (new_win_cnt) {
				if (crtc->index == 2) {
					win_mask = find_set_bits_mask(curr_win_mask,
							old_win_cnt - new_win_cnt);
				} else {
					win_mask = find_set_bits_mask_reverse_order(curr_win_mask,
							old_win_cnt - new_win_cnt);
				}
			} else /* freeing all windows */
				win_mask = curr_win_mask;

			WARN(!win_mask, "%s: invalid reserved mask (0x%x) for win_cnt (%d->%d)",
					crtc->name, curr_win_mask, old_win_cnt, new_win_cnt);

			new_exynos_crtc_state->freed_win_mask |= win_mask;
			new_exynos_crtc_state->reserved_win_mask &= ~win_mask;
		} else {
			struct exynos_drm_private *priv = dev->dev_private;
			mutex_lock(&priv->lock);
			if (crtc->index == 2) {
				win_mask = find_set_bits_mask_reverse_order(priv->available_win_mask,
						new_win_cnt - old_win_cnt);
			} else {
				win_mask = find_set_bits_mask(priv->available_win_mask,
						new_win_cnt - old_win_cnt);
			}
			if (!win_mask) {
				DRM_WARN("%s: No windows available for req win cnt=%d->%d (0x%x)\n",
						crtc->name, old_win_cnt, new_win_cnt,
						priv->available_win_mask);
				mutex_unlock(&priv->lock);
				return -ENOENT;
			}
			pr_debug("%s: current win_mask=0x%x new=0x%x avail=0x%x\n", crtc->name,
					new_exynos_crtc_state->reserved_win_mask, win_mask,
					priv->available_win_mask);

			mutex_unlock(&priv->lock);
			new_exynos_crtc_state->reserved_win_mask |= win_mask;
			new_exynos_crtc_state->win_inserted = true;
		}
	}

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);
		const unsigned long reserved_win_mask =
			new_exynos_crtc_state->reserved_win_mask;
		unsigned long visible_zpos = 0;
		struct drm_plane *plane;
		const struct drm_plane_state *plane_state;
		unsigned int pmask, bit, win_cnt;

		new_exynos_crtc_state->visible_win_mask = 0;

		/* TODO: move to check display config */
		if (!new_exynos_crtc_state->reserved_win_mask)
			new_exynos_crtc_state->skip_frameupdate = true;

		if (!new_crtc_state->plane_mask)
			continue;

		drm_atomic_crtc_state_for_each_plane_state(plane, plane_state,
				new_crtc_state)
			if (plane_state->visible)
				visible_zpos |= BIT(plane_state->normalized_zpos);

		pmask = 1;
		win_cnt = get_plane_num(dev);
		for_each_set_bit(bit, &reserved_win_mask, win_cnt) {
			if (pmask & visible_zpos)
				new_exynos_crtc_state->visible_win_mask |= BIT(bit);
			pmask <<= 1;
		}
		pr_debug("%s: visible_win_mask(0x%x)\n", __func__,
				new_exynos_crtc_state->visible_win_mask);
	}

	return 0;
}

static void exynos_atomic_prepare_partial_update(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct exynos_drm_crtc_state *old_exynos_crtc_state, *new_exynos_crtc_state;
	struct exynos_partial *partial;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);
		partial = exynos_crtc->partial;

		if (!new_crtc_state->active || !partial)
			continue;

		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_crtc_state = to_exynos_crtc_state(old_crtc_state);

		if (drm_atomic_crtc_needs_modeset(new_crtc_state) ||
				(new_exynos_crtc_state->seamless_modeset
				& SEAMLESS_MODESET_MRES)) {
			const struct exynos_drm_connector_state *exynos_conn_state =
				crtc_get_exynos_connector_state(state, new_crtc_state,
						DRM_MODE_CONNECTOR_DSI);

			if (exynos_conn_state) {
				const struct exynos_display_mode *exynos_mode =
					&exynos_conn_state->exynos_mode;

				exynos_partial_initialize(partial,
						&new_crtc_state->mode, exynos_mode);
			}
		}

		if (!partial->enabled)
			continue;

		exynos_partial_prepare(partial, old_exynos_crtc_state,
				new_exynos_crtc_state);
	}
}

static int exynos_atomic_add_affected_objects(struct drm_device *dev,
					       struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		conn = crtc_get_conn(new_crtc_state, DRM_MODE_CONNECTOR_WRITEBACK);
		if (conn) {
			new_conn_state = drm_atomic_get_connector_state(state, conn);
			if (IS_ERR(new_conn_state))
				return PTR_ERR(new_conn_state);
		}
	}

	return 0;
}

static int exynos_drm_atomic_zpos_cmp(const void *a, const void *b)
{
	const struct drm_plane_state *sa = *(struct drm_plane_state **)a;
	const struct drm_plane_state *sb = *(struct drm_plane_state **)b;
	const struct exynos_drm_plane_state *esa = to_exynos_plane_state(sa);
	const struct exynos_drm_plane_state *esb = to_exynos_plane_state(sb);
	const struct drm_crtc_state *csa = sa->crtc->state;
	const struct drm_crtc_state *csb = sb->crtc->state;
	int position_a = 0, position_b = 0;

	if (esa->split) {
		const struct exynos_split *split_a = (struct exynos_split *)esa->split->data;
		position_a = split_a->position;
	}
	if (esb->split) {
		const struct exynos_split *split_b = (struct exynos_split *)esb->split->data;
		position_b = split_b->position;
	}
	if ((position_a == 0) && (sa->crtc_x >= csa->mode.hdisplay / 2))
		position_a = 2;
	if ((position_b == 0) && (sb->crtc_x >= csb->mode.hdisplay / 2))
		position_b = 2;
	if (position_a != position_b)
		return position_a - position_b;

	if (sa->zpos != sb->zpos)
		return sa->zpos - sb->zpos;
	else
		return sa->plane->base.id - sb->plane->base.id;
}

static int exynos_drm_atomic_reorder_zpos(struct drm_atomic_state *state,
					  struct drm_crtc *crtc,
					  struct drm_crtc_state *crtc_state)
{
	struct drm_device *dev = crtc->dev;
	int total_planes = dev->mode_config.num_total_plane;
	struct drm_plane_state **states;
	struct drm_plane *plane;
	int i, n = 0;
	int ret = 0;

	states = kmalloc_array(total_planes, sizeof(*states), GFP_KERNEL);
	if (!states)
		return -ENOMEM;

	drm_for_each_plane_mask(plane, dev, crtc_state->plane_mask) {
		struct drm_plane_state *plane_state =
			drm_atomic_get_new_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			goto done;
		}
		states[n++] = plane_state;
	}

	sort(states, n, sizeof(*states), exynos_drm_atomic_zpos_cmp, NULL);

	for (i = 0; i < n; i++) {
		plane = states[i]->plane;
		states[i]->normalized_zpos = i;
		pr_debug("[PLANE:%d:%s] normalized zpos value %d\n",
				 plane->base.id, plane->name, i);
	}
	crtc_state->zpos_changed = true;

done:
	kfree(states);
	return ret;
}

static int exynos_drm_atomic_check_zpos(struct drm_device *dev,
					struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i, ret = 0;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		if (old_crtc_state->plane_mask != new_crtc_state->plane_mask ||
		    new_crtc_state->zpos_changed) {
			if ((crtc->index == 2) && is_dual_blender_by_mode(&new_crtc_state->mode)) {
				ret = exynos_drm_atomic_reorder_zpos(state, crtc, new_crtc_state);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

static int exynos_drm_atomic_normalize_zpos(struct drm_device *dev,
					    struct drm_atomic_state *state)
{
	int ret;

	ret = drm_atomic_normalize_zpos(dev, state);
	if (ret)
		return ret;
	ret = exynos_drm_atomic_check_zpos(dev, state);

	return ret;
}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
int exynos_drm_check_frame_skip(struct drm_atomic_state *state)
{
	int i;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);
		if (mcd_drm_check_commit_retry(exynos_crtc, __func__)) {
			usleep_range(16600, 16700);
			return -EAGAIN;
		}
	}

	return 0;
}
#endif

static int exynos_atomic_check(struct drm_device *dev, struct drm_atomic_state *state)
{
	int ret;

	DPU_ATRACE_BEGIN(__func__);

	ret = exynos_atomic_add_affected_objects(dev, state);
	if (ret)
		goto out;

	ret = exynos_drm_atomic_check_tui(state);
	if (ret)
		goto out;

	ret = drm_atomic_helper_check_modeset(dev, state);
	if (ret)
		goto out;

	exynos_atomic_prepare_partial_update(state);

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	ret = exynos_drm_check_frame_skip(state);
	if (ret)
		goto out;
#endif

	ret = exynos_drm_atomic_normalize_zpos(dev, state);
	if (ret)
		goto out;

	ret = drm_atomic_helper_check_planes(dev, state);
	if (ret)
		goto out;

	exynos_drm_init_resource_cnt();
	ret = exynos_atomic_check_windows(dev, state);
	if (ret)
		goto out;

	drm_self_refresh_helper_alter_state(state);
out:
	DPU_ATRACE_END(__func__);
	return ret;
}

static struct
drm_private_state *exynos_atomic_duplicate_priv_state(struct drm_private_obj *obj)
{
	const struct exynos_drm_priv_state *old_state =
					to_exynos_priv_state(obj->state);
	struct exynos_drm_priv_state *new_state;

	new_state = kmemdup(old_state, sizeof(*old_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &new_state->base);
	return &new_state->base;
}

static void exynos_atomic_destroy_priv_state(struct drm_private_obj *obj,
		struct drm_private_state *state)
{
	struct exynos_drm_priv_state *priv_state = to_exynos_priv_state(state);

	kfree(priv_state);
}

static const struct drm_private_state_funcs exynos_priv_state_funcs = {
	.atomic_duplicate_state = exynos_atomic_duplicate_priv_state,
	.atomic_destroy_state = exynos_atomic_destroy_priv_state,
};

/* See also: drm_atomic_helper_wait_for_fences() */
#define NUM_VBLANK		(5)			/* vblank wait count */
#define DFT_FPS			(60)			/* default fps */
static int exynos_drm_atomic_helper_wait_for_fences(struct drm_device *dev,
				      struct drm_atomic_state *state,
				      bool pre_swap)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct dma_fence *fence;
	ktime_t fence_wait_start;
	s64 fence_wait_ms, debug_timeout_ms;
	int i, ret = 0, fps;
	bool timeout;
#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
	struct exynos_drm_plane_state *new_exynos_plane_state;
#endif

	DPU_ATRACE_BEGIN(__func__);
	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		if (!new_plane_state->fence)
			continue;

		WARN_ON(!new_plane_state->fb);

		/*
		 * If waiting for fences pre-swap (ie: nonblock), userspace can
		 * still interrupt the operation. Instead of blocking until the
		 * timer expires, make the wait interruptible.
		 */

		fence = new_plane_state->fence;
		exynos_crtc = to_exynos_crtc(new_plane_state->crtc);
		DPU_EVENT_LOG("WAIT_PLANE_IN_FENCE", exynos_crtc, EVENT_FLAG_FENCE,
				FENCE_FMT, FENCE_ARG(fence));

		new_crtc_state = new_plane_state->crtc->state;
		fps = drm_mode_vrefresh(&new_crtc_state->adjusted_mode) ? : DFT_FPS;
		debug_timeout_ms = NUM_VBLANK * MSEC_PER_SEC / fps;

		timeout = false;
		fence_wait_start = ktime_get();
		ret = dma_fence_wait_timeout(fence, pre_swap,
				msecs_to_jiffies(debug_timeout_ms));
		if (ret <= 0) {
			const struct dma_fence_ops *ops;

			DPU_EVENT_LOG("TIMEOUT_PLANE_IN_FENCE", exynos_crtc,
					EVENT_FLAG_FENCE, "in_fence_wait: %lldmsec",
					debug_timeout_ms);
			ops = fence->ops;
			dpu_err(dev, "plane-%u fence waiting timeout(%lldmsec)\n",
					plane->index,	debug_timeout_ms);
			dpu_err(dev, "driver=%s, timeline =%s\n",
					ops->get_driver_name(fence),
					ops->get_timeline_name(fence));
			timeout = true;
		}

		ret = dma_fence_wait(fence, pre_swap);
		if (timeout) {
			fence_wait_ms = ktime_ms_delta(ktime_get(), fence_wait_start);
			DPU_EVENT_LOG("ELAPSED_PLANE_IN_FENCE", exynos_crtc,
					EVENT_FLAG_FENCE, "in_fence_wait: %lldmsec",
					fence_wait_ms);
			dpu_err(dev, "fence elapsed %lldmsec\n", fence_wait_ms);
		}
		if (ret)
			goto out;
#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
		new_exynos_plane_state = to_exynos_plane_state(new_plane_state);
		snprintf(new_exynos_plane_state->fence_info, 256, FENCE_FMT, FENCE_ARG(fence));
#endif
		dma_fence_put(new_plane_state->fence);
		new_plane_state->fence = NULL;
	}
out:
	DPU_ATRACE_END(__func__);

	return ret;
}

static bool check_recovery_state(const struct drm_crtc* crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->is_recovering &&
			exynos_crtc->ops->is_recovering(exynos_crtc))
		return true;

	return false;
}

static void commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	const struct drm_mode_config_helper_funcs *funcs;
	const struct drm_crtc *crtc;
	bool recovery = false;
	int i;

	funcs = dev->mode_config.helper_private;

	exynos_atomic_commit_check_buf_sanity(old_state);

	exynos_drm_atomic_helper_wait_for_fences(dev, old_state, false);

	exynos_atomic_commit_prepare_buf_sanity(old_state);

	for_each_crtc_in_state(old_state, crtc, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

		exynos_profiler_update_ems_fence_cnt(exynos_crtc);
		recovery = check_recovery_state(crtc);
	}

	if (unlikely(recovery))
		pr_info("recovery, skip for all preceding commits\n");
	else
		drm_atomic_helper_wait_for_dependencies(old_state);

	if (funcs && funcs->atomic_commit_tail)
		funcs->atomic_commit_tail(old_state);
	else
		drm_atomic_helper_commit_tail(old_state);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void commit_kthread_work(struct kthread_work *work)
{
	struct exynos_drm_crtc_state *new_exynos_crtc_state =
		container_of(work, struct exynos_drm_crtc_state, commit_work);
	struct drm_atomic_state *old_state = new_exynos_crtc_state->old_state;

	commit_tail(old_state);
}

static void commit_work(struct work_struct *work)
{
	struct drm_atomic_state *old_state =
		container_of(work, struct drm_atomic_state, commit_work);

	commit_tail(old_state);
}

static void exynos_atomic_queue_work(struct drm_atomic_state *old_state, bool nonblock)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	int i;

	/*
	 * our driver don't support multiple display update with same commit
	 * but if need, consider to split work per display
	 */
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);

		new_exynos_crtc_state->old_state = old_state;
		kthread_init_work(&new_exynos_crtc_state->commit_work,
				commit_kthread_work);
		kthread_queue_work(&exynos_crtc->worker,
				&new_exynos_crtc_state->commit_work);
		return;
	}

	/* fallback to regular commit work if we get here (no crtcs in commit state) */
	INIT_WORK(&old_state->commit_work, commit_work);
	queue_work(system_highpri_wq, &old_state->commit_work);
}

static int exynos_atomic_commit(struct drm_device *dev,
				struct drm_atomic_state *state,
				bool nonblock)
{
	int ret;

	DPU_ATRACE_BEGIN(__func__);
#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
	nonblock = false;
#endif
	ret = drm_atomic_helper_setup_commit(state, nonblock);
	if (ret)
		goto err;

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		goto err;

	/*
	 * This is the point of no return - everything below never fails except
	 * when the hw goes bonghits. Which means we can commit the new state on
	 * the software side now.
	 */
	ret = drm_atomic_helper_swap_state(state, true);
	if (ret) {
		drm_atomic_helper_cleanup_planes(dev, state);
		goto err;
	}

	exynos_drm_crtc_alloc_windows(dev, state);

	/*
	 * Everything below can be run asynchronously without the need to grab
	 * any modeset locks at all under one condition: It must be guaranteed
	 * that the asynchronous work has either been cancelled (if the driver
	 * supports it, which at least requires that the framebuffers get
	 * cleaned up with drm_atomic_helper_cleanup_planes()) or completed
	 * before the new state gets committed on the software side with
	 * drm_atomic_helper_swap_state().
	 *
	 * This scheme allows new atomic state updates to be prepared and
	 * checked in parallel to the asynchronous completion of the previous
	 * update. Which is important since compositors need to figure out the
	 * composition of the next frame right after having submitted the
	 * current layout.
	 *
	 * NOTE: Commit work has multiple phases, first hardware commit, then
	 * cleanup. We want them to overlap, hence need system_unbound_wq to
	 * make sure work items don't artificially stall on each another.
	 */

	drm_atomic_state_get(state);
	if (!nonblock)
		commit_tail(state);
	else
		exynos_atomic_queue_work(state, nonblock);

err:
	DPU_ATRACE_END(__func__);
	return ret;
}

static void plane_state_to_win_config(struct dpu_bts_win_config *win_config,
				      const struct drm_plane_state *plane_state)
{
	const struct drm_framebuffer *fb = plane_state->fb;
	unsigned int simplified_rot;
	struct exynos_drm_plane_state *exynos_state =
				to_exynos_plane_state(plane_state);

	win_config->src_x = plane_state->src.x1 >> 16;
	win_config->src_y = plane_state->src.y1 >> 16;
	win_config->src_w = drm_rect_width(&plane_state->src) >> 16;
	win_config->src_h = drm_rect_height(&plane_state->src) >> 16;

	win_config->dst_x = plane_state->dst.x1;
	win_config->dst_y = plane_state->dst.y1;
	win_config->dst_w = drm_rect_width(&plane_state->dst);
	win_config->dst_h = drm_rect_height(&plane_state->dst);

	win_config->dbg_dma_addr = exynos_drm_fb_dma_addr(plane_state->fb, 0);

	win_config->mod = fb->modifier;
	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0), fb->modifier) ||
		has_all_bits(DRM_FORMAT_MOD_ARM_AFBC(0), fb->modifier) ||
		has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0, 0),
				fb->modifier) ||
		has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSY, 0, 0),
				fb->modifier))
		win_config->is_comp = true;
	else
		win_config->is_comp = false;

	if (exynos_drm_fb_is_colormap(fb))
		win_config->state = DPU_WIN_STATE_COLOR;
	else
		win_config->state = DPU_WIN_STATE_BUFFER;

	win_config->format = fb->format->format;
	win_config->dpp_ch = plane_state->plane->index;

	win_config->comp_src = 0;
	if (has_all_bits(DRM_FORMAT_MOD_ARM_AFBC(0), fb->modifier))
		win_config->comp_src =
			(fb->modifier & AFBC_FORMAT_MOD_SOURCE_MASK);

	simplified_rot = drm_rotation_simplify(plane_state->rotation,
			DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);
	win_config->is_rot = false;
	if (simplified_rot & DRM_MODE_ROTATE_90)
		win_config->is_rot = true;
	win_config->is_hdr = exynos_state->hdr_en;

	DRM_DEBUG("src[%d %d %d %d], dst[%d %d %d %d]\n",
			win_config->src_x, win_config->src_y,
			win_config->src_w, win_config->src_h,
			win_config->dst_x, win_config->dst_y,
			win_config->dst_w, win_config->dst_h);
	DRM_DEBUG("rot[%d] afbc[%d] format[%d] ch[%d] zpos[%d] comp_src[%#llx] hdr[%d]\n",
			win_config->is_rot, win_config->is_comp,
			win_config->format, win_config->dpp_ch,
			plane_state->normalized_zpos,
			win_config->comp_src, win_config->is_hdr);
	DRM_DEBUG("alpha[%d] blend mode[%d]\n",
			plane_state->alpha, plane_state->pixel_blend_mode);
	DRM_DEBUG("simplified rot[0x%x]\n", simplified_rot);
}

static void conn_state_to_win_config(struct dpu_bts_win_config *win_config,
				const struct drm_connector_state *conn_state)
{
	const struct writeback_device *wb = conn_to_wb_dev(conn_state->connector);
	const struct drm_framebuffer *fb = conn_state->writeback_job->fb;

	win_config->src_x = 0;
	win_config->src_y = 0;
	win_config->src_w = fb->width;
	win_config->src_h = fb->height;
	win_config->dst_x = 0;
	win_config->dst_y = 0;
	win_config->dst_w = fb->width;
	win_config->dst_h = fb->height;

	win_config->dbg_dma_addr = exynos_drm_fb_dma_addr(fb, 0);

	win_config->is_comp = false;
	win_config->state = DPU_WIN_STATE_BUFFER;
	win_config->format = fb->format->format;
	win_config->dpp_ch = wb->id;
	win_config->comp_src = 0;
	win_config->is_rot = false;

	DRM_DEBUG("src[%d %d %d %d], dst[%d %d %d %d]\n",
			win_config->src_x, win_config->src_y,
			win_config->src_w, win_config->src_h,
			win_config->dst_x, win_config->dst_y,
			win_config->dst_w, win_config->dst_h);
	DRM_DEBUG("rot[%d] afbc[%d] format[%d] ch[%d] comp_src[%#llx]\n",
			win_config->is_rot, win_config->is_comp,
			win_config->format, win_config->dpp_ch,
			win_config->comp_src);
}

static void exynos_atomic_update_conn_bts(struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_connector *conn;
	const struct drm_connector_state *old_conn_state, *new_conn_state;
	struct dpu_bts_win_config *win_config;
	int i;

	for_each_oldnew_connector_in_state(old_state, conn, old_conn_state,
					new_conn_state, i) {
		bool new_job;

		if (conn->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;

		new_job = wb_check_job(new_conn_state);

		if (new_job) {
			exynos_crtc = to_exynos_crtc(new_conn_state->crtc);
			win_config = &exynos_crtc->bts->wb_config;
			conn_state_to_win_config(win_config, new_conn_state);
		} else {
			if (!old_conn_state->crtc)
				continue;
			exynos_crtc = to_exynos_crtc(old_conn_state->crtc);
			win_config = &exynos_crtc->bts->wb_config;
			win_config->state = DPU_WIN_STATE_DISABLED;
		}
	}
}

static void exynos_atomic_bts_pre_update(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc *crtc;
	const struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	const struct drm_plane_state *old_plane_state, *new_plane_state;
	struct dpu_bts_win_config *win_config;
	int i;

	if (!IS_ENABLED(CONFIG_EXYNOS_BTS))
		return;

	DPU_ATRACE_BEGIN(__func__);
	for_each_oldnew_plane_in_state(old_state, plane, old_plane_state,
				       new_plane_state, i) {
		struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
		const int zpos = new_plane_state->normalized_zpos;

		if (exynos_plane->is_rcd)
			continue;

		if (new_plane_state->crtc) {
			exynos_crtc = to_exynos_crtc(new_plane_state->crtc);
			win_config = &exynos_crtc->bts->win_config[zpos];
			plane_state_to_win_config(win_config, new_plane_state);
		} else if (old_plane_state->crtc) {
			exynos_crtc = to_exynos_crtc(old_plane_state->crtc);
			win_config = &exynos_crtc->bts->win_config[zpos];
			win_config->dbg_dma_addr = 0;
		}
	}

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
					to_exynos_crtc_state(new_crtc_state);
		const size_t max_planes = get_plane_num(dev);
		const size_t num_planes =
			hweight32(new_crtc_state->plane_mask &
				  ~exynos_crtc->rcd_plane_mask);
		int num_rcd = new_exynos_crtc_state->rcd_enabled ? 1 : 0;
		int j;

		if (!new_crtc_state->active)
			continue;

		if (new_exynos_crtc_state->skip_update)
			continue;

		if (new_crtc_state->planes_changed) {
			for (j = num_planes; j < max_planes-num_rcd; j++) {
				win_config = &exynos_crtc->bts->win_config[j];
				win_config->state = DPU_WIN_STATE_DISABLED;
			}
		}

		if (exynos_crtc->ops->update_bts_fps)
			exynos_crtc->ops->update_bts_fps(exynos_crtc);

		for (j = 0; j < num_planes; j++)
			DPU_EVENT_LOG(j == 0 ? "ATOMIC_COMMIT" : "", exynos_crtc,
					EVENT_FLAG_LONG, dpu_config_to_string,
					&exynos_crtc->bts->win_config[j]);

		/*
		 * In the case of seamless modeset, update the bts
		 * in decon_seamless_set_mode.
		 */
		if (!new_exynos_crtc_state->seamless_modeset) {
			exynos_crtc->bts->ops->calc_bw(exynos_crtc);
			exynos_crtc->bts->ops->update_bw(exynos_crtc, false);
		}
	}
	DPU_ATRACE_END(__func__);
}

static void exynos_atomic_bts_post_update(struct drm_device *dev,
					  struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_connector *conn;
	const struct drm_connector_state *new_conn_state;
	struct drm_crtc *crtc;
	const struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_plane *exynos_plane;
	struct drm_plane *plane;
	const struct dpu_bts_win_config *win_config;
	int i, win_id, win_cnt = get_plane_num(dev);

	if (!IS_ENABLED(CONFIG_EXYNOS_BTS))
		return;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
					to_exynos_crtc_state(new_crtc_state);

		if (new_exynos_crtc_state->skip_update)
			continue;

		exynos_crtc = to_exynos_crtc(crtc);

		if (new_crtc_state->planes_changed && new_crtc_state->active) {

			/*
			 * keeping a copy of comp src in dpp after it has been
			 * applied in hardware for debugging purposes.
			 * plane order == dpp channel order
			 */
			drm_for_each_plane(plane, dev) {
				exynos_plane = to_exynos_plane(plane);
				if (exynos_plane->win_id >= win_cnt)
					continue;

				win_id = exynos_plane->win_id;
				win_config =
					&exynos_crtc->bts->win_config[win_id];
				if (win_config->state == DPU_WIN_STATE_BUFFER &&
						win_config->is_comp)
					exynos_plane->comp_src = win_config->comp_src;
			}

			exynos_crtc->bts->ops->update_bw(exynos_crtc, true);
			DPU_EVENT_LOG("DECON_CH_OCCUPANCY", exynos_crtc, EVENT_FLAG_LONG,
					 dpu_rsc_ch_to_string, exynos_crtc);
			DPU_EVENT_LOG("DECON_WIN_OCCUPANCY", exynos_crtc, EVENT_FLAG_LONG,
					 dpu_rsc_win_to_string, exynos_crtc);
		}

		if (!new_crtc_state->active && new_crtc_state->active_changed)
			exynos_crtc->bts->ops->release_bw(exynos_crtc);
	}

	for_each_new_connector_in_state(old_state, conn, new_conn_state, i) {
		struct exynos_drm_connector *exynos_conn;
		const struct exynos_drm_connector_funcs *funcs;

		if (conn->connector_type != DRM_MODE_CONNECTOR_DSI)
			continue;

		exynos_conn = to_exynos_connector(conn);
		funcs = exynos_conn->funcs;
		if (funcs && funcs->update_fps_config)
			funcs->update_fps_config(exynos_conn);
	}
}

#define DEFAULT_TIMEOUT_FPS 60
static void exynos_atomic_wait_for_vblanks(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i, ret;
	unsigned crtc_mask = 0;

	/*
	 * Legacy cursor ioctls are completely unsynced, and userspace
	 * relies on that (by doing tons of cursor updates).
	 */
	if (old_state->legacy_cursor_update)
		return;

	DPU_ATRACE_BEGIN(__func__);
	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc;
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);

		if (!new_crtc_state->active || new_crtc_state->no_vblank)
			continue;

		if (new_exynos_crtc_state->skip_update)
			continue;

		exynos_crtc = to_exynos_crtc(crtc);
		if (exynos_crtc->ops->check_svsync_start)
			if (exynos_crtc->ops->check_svsync_start(exynos_crtc))
				continue;

		ret = drm_crtc_vblank_get(crtc);
		if (ret != 0)
			continue;

		crtc_mask |= drm_crtc_mask(crtc);
		old_state->crtcs[i].last_vblank_count = drm_crtc_vblank_count(crtc);
	}

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		static ktime_t timestamp_s;
		s64 diff_msec;
		u64 fps;
		u32 default_vblank_timeout, max_vblank_timeout, interval;

		if (!(crtc_mask & drm_crtc_mask(crtc)))
			continue;

		fps = drm_mode_vrefresh(&new_crtc_state->mode) ?: DEFAULT_TIMEOUT_FPS;
		interval = USEC_PER_SEC / fps / MSEC_PER_SEC;

		if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
			interval *= 1000;

		max_vblank_timeout = 12 * interval;
		default_vblank_timeout = 6 * interval;

		timestamp_s = ktime_get();
		ret = wait_event_timeout(dev->vblank[i].queue,
				old_state->crtcs[i].last_vblank_count !=
				drm_crtc_vblank_count(crtc),
				msecs_to_jiffies(max_vblank_timeout));

		diff_msec = ktime_to_ms(ktime_sub(ktime_get(), timestamp_s));

		if (diff_msec >= default_vblank_timeout)
			pr_warn("[CRTC:%d:%s] vblank wait timed out!!!(%lld msec / %u msec)\n",
				crtc->base.id, crtc->name, diff_msec, default_vblank_timeout);

		if (!ret) {
#if defined(CONFIG_EXYNOS_UEVENT_RECOVERY_SOLUTION)
			struct exynos_drm_crtc *exynos_crtc;
#endif
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
			/* Code for bypass commit when panel was not connected */
			if (mcd_drm_check_commit_skip(NULL, __func__)) {
				drm_crtc_handle_vblank(crtc);
			} else {
				DPU_EVENT_LOG("VBLANK_TIMEOUT", to_exynos_crtc(crtc),
						EVENT_FLAG_ERROR, NULL);
				dpu_print_eint_state(crtc);
				dpu_check_panel_status(crtc);
			}
#else
			DPU_EVENT_LOG("VBLANK_TIMEOUT", to_exynos_crtc(crtc),
					EVENT_FLAG_ERROR, NULL);
			dpu_print_eint_state(crtc);
			dpu_check_panel_status(crtc);
#endif
#if defined(CONFIG_EXYNOS_UEVENT_RECOVERY_SOLUTION)
			if (check_recovery_state(crtc)) {
				pr_info("[CRTC:%d:%s]recovery already running.\n",
					crtc->base.id, crtc->name);
			} else {
				//send event to HWC, request for reset.
				exynos_crtc = to_exynos_crtc(crtc);
				if (exynos_crtc) {
					exynos_crtc->ops->recovery(to_exynos_crtc(crtc), "uevent_r");
					drm_atomic_helper_commit_hw_done(old_state);
					drm_atomic_helper_cleanup_planes(dev, old_state);
				}
			}
#endif
		}

		drm_crtc_vblank_put(crtc);
	}

	DPU_ATRACE_END(__func__);
}

static void
exynos_atomic_framestart_post_processing(struct drm_atomic_state *old_state)
{
	const struct drm_crtc *crtc;
	const struct drm_crtc_state *new_crtc_state;
	const struct drm_connector *conn;
	const struct drm_connector_state *new_conn_state;
	const struct exynos_drm_crtc_state *new_exynos_crtc_state;
	struct exynos_drm_private *priv;
	int i;

	for_each_new_connector_in_state(old_state, conn, new_conn_state, i) {
		if (conn->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;

		repeater_buf_release(conn, new_conn_state);
	}

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		if (new_exynos_crtc_state->freed_win_mask) {
			priv = old_state->dev->dev_private;

			/* release windows after framestart */
			mutex_lock(&priv->lock);
			priv->available_win_mask |=
				new_exynos_crtc_state->freed_win_mask;
			mutex_unlock(&priv->lock);
		}

		exynos_crtc_send_event(to_exynos_crtc(crtc));
	}
}

static bool
crtc_needs_disable(struct drm_crtc_state *old_state,
		   struct drm_crtc_state *new_state)
{
	/*
	 * No new_state means the CRTC is off, so the only criteria is whether
	 * it's currently active or in self refresh mode.
	 */
	if (!new_state)
		return drm_atomic_crtc_effectively_active(old_state);

	/*
	 * We need to disable bridge(s) and CRTC if we're transitioning out of
	 * self-refresh and changing CRTCs at the same time, because the
	 * bridge tracks self-refresh status via CRTC state.
	 */
	if (old_state->self_refresh_active &&
	    old_state->crtc != new_state->crtc)
		return true;

	/*
	 * We need to run through the crtc_funcs->disable() function if the CRTC
	 * is currently on, if it's transitioning to self refresh mode, or if
	 * it's in self refresh mode and needs to be fully disabled.
	 */
	return old_state->active ||
	       (old_state->self_refresh_active && !new_state->active) ||
	       new_state->self_refresh_active;
}

static void
disable_outputs(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	unsigned int disabling_encoder_mask = 0;
	int i;

	for_each_oldnew_connector_in_state(old_state, conn, old_conn_state,
			new_conn_state, i) {
		if (!old_conn_state->crtc)
			continue;

		old_crtc_state = drm_atomic_get_old_crtc_state(old_state,
				old_conn_state->crtc);

		if (new_conn_state->crtc)
			new_crtc_state = drm_atomic_get_new_crtc_state(old_state,
						new_conn_state->crtc);
		else
			new_crtc_state = NULL;

		if (!crtc_needs_disable(old_crtc_state, new_crtc_state) ||
		    !drm_atomic_crtc_needs_modeset(old_conn_state->crtc->state))
			continue;

		encoder = old_conn_state->best_encoder;
		if (WARN_ON(!encoder))
			continue;

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_atomic_bridge_chain_disable(bridge, old_state);
		disabling_encoder_mask |= drm_encoder_mask(encoder);
	}

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
			new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		if (!crtc_needs_disable(old_crtc_state, new_crtc_state))
			continue;

		DRM_DEBUG_ATOMIC("disabling [CRTC:%d:%s]\n",
				 crtc->base.id, crtc->name);

		funcs = crtc->helper_private;
		if (funcs && funcs->atomic_disable)
			funcs->atomic_disable(crtc, old_state);

	}

	drm_for_each_encoder_mask(encoder, dev, disabling_encoder_mask) {
		const struct drm_encoder_helper_funcs *funcs;

		DRM_DEBUG_ATOMIC("disabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		funcs = encoder->helper_private;
		if (funcs && funcs->atomic_disable)
			funcs->atomic_disable(encoder, old_state);

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_atomic_bridge_chain_post_disable(bridge, old_state);
	}
}

static void
crtc_set_mode(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!new_crtc_state->mode_changed)
			continue;

		funcs = crtc->helper_private;
		if (!funcs)
			continue;

		if (new_crtc_state->enable && funcs->mode_set_nofb) {
			DRM_DEBUG_ATOMIC("modeset on [CRTC:%d:%s]\n",
					 crtc->base.id, crtc->name);

			funcs->mode_set_nofb(crtc);
		}
	}

	for_each_new_connector_in_state(old_state, conn, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_display_mode *mode, *adjusted_mode;
		struct drm_bridge *bridge;

		if (!new_conn_state->best_encoder)
			continue;

		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;
		new_crtc_state = new_conn_state->crtc->state;
		mode = &new_crtc_state->mode;
		adjusted_mode = &new_crtc_state->adjusted_mode;

		if (!new_crtc_state->mode_changed)
			continue;

		DRM_DEBUG_ATOMIC("modeset on [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call mode_set hooks twice.
		 */
		if (funcs && funcs->atomic_mode_set) {
			funcs->atomic_mode_set(encoder, new_crtc_state,
					       new_conn_state);
		}

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_bridge_chain_mode_set(bridge, mode, adjusted_mode);
	}
}

/* See also drm_atomic_helper_commit_modeset_disables() */
static void
exynos_atomic_commit_modeset_disables(struct drm_device *dev,
				      struct drm_atomic_state *old_state)
{
	DPU_ATRACE_BEGIN(__func__);
	disable_outputs(dev, old_state);

	drm_atomic_helper_update_legacy_modeset_state(dev, old_state);
	drm_atomic_helper_calc_timestamping_constants(old_state);

	crtc_set_mode(dev, old_state);
	DPU_ATRACE_END(__func__);
}

static void exynos_drm_atomic_commit_writebacks(struct drm_device *dev,
						struct drm_atomic_state *old_state)
{
	struct drm_connector *conn;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, conn, new_conn_state, i) {
		const struct drm_connector_helper_funcs *funcs;

		funcs = conn->helper_private;
		if (!funcs || !funcs->atomic_commit)
			continue;

		if (!new_conn_state->writeback_job || !new_conn_state->writeback_job->fb)
			continue;

		WARN_ON(conn->connector_type != DRM_MODE_CONNECTOR_WRITEBACK);
		funcs->atomic_commit(conn, old_state);
	}
}

/* See also drm_atomic_helper_commit_modeset_enables() */
void exynos_atomic_commit_modeset_enables(struct drm_device *dev,
					  struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *new_conn_state;
	int i;

	DPU_ATRACE_BEGIN(__func__);
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		/* Need to filter out CRTCs where only planes change. */
		if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		if (!new_crtc_state->active ||!new_crtc_state->enable)
			continue;

		DRM_DEBUG_ATOMIC("enabling [CRTC:%d:%s]\n",
				crtc->base.id, crtc->name);

		funcs = crtc->helper_private;
		if (funcs && funcs->atomic_enable)
			funcs->atomic_enable(crtc, old_state);
	}

	for_each_new_connector_in_state(old_state, conn, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_bridge *bridge;

		if (!new_conn_state->best_encoder)
			continue;

		if (!new_conn_state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(new_conn_state->crtc->state))
			continue;

		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;

		DRM_DEBUG_ATOMIC("enabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call enable hooks twice.
		 */
		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_atomic_bridge_chain_pre_enable(bridge, old_state);
		exynos_drm_bridge_reset(bridge, BRIDGE_RESET_LP00);

		if (funcs && funcs->atomic_enable) {
			funcs->atomic_enable(encoder, old_state);
			exynos_drm_bridge_reset(bridge, BRIDGE_RESET_LP11);
			if (encoder->encoder_type == DRM_MODE_ENCODER_DSI)
				exynos_drm_encoder_activate(encoder, old_state);
		}

		exynos_drm_bridge_reset(bridge, BRIDGE_RESET_LP11_HS);
		drm_atomic_bridge_chain_enable(bridge, old_state);
	}

	exynos_drm_atomic_commit_writebacks(dev, old_state);
	DPU_ATRACE_END(__func__);
}

static void exynos_atomic_block_hiber(struct drm_atomic_state *old_state,
				      unsigned int *hibernation_crtc_mask)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
			new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

		DRM_DEBUG("[CRTC-%d] old "STATE_FMT"\n", drm_crtc_index(crtc),
				STATE_ARG(old_crtc_state));
		DRM_DEBUG("[CRTC-%d] new "STATE_FMT"\n", drm_crtc_index(crtc),
				STATE_ARG(new_crtc_state));

		DPU_EVENT_LOG("REQ_CRTC_INFO_OLD", exynos_crtc, 0, STATE_FMT,
				STATE_ARG(old_crtc_state));
		DPU_EVENT_LOG("REQ_CRTC_INFO_NEW", exynos_crtc,	0, STATE_FMT,
				STATE_ARG(new_crtc_state));

		if (exynos_crtc->hibernation && (new_crtc_state->active ||
					new_crtc_state->active_changed)) {
			hibernation_block(exynos_crtc->hibernation);
			*hibernation_crtc_mask |= drm_crtc_mask(crtc);
		}
	}
}

static void exynos_atomic_unblock_hiber(struct drm_atomic_state *old_state,
					unsigned int *hibernation_crtc_mask)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;
	int i;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);
		if (*hibernation_crtc_mask & drm_crtc_mask(crtc))
			hibernation_unblock(exynos_crtc->hibernation);
	}
}

static void exynos_atomic_wait_for_framestart(struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_drm_crtc_ops *ops;
	struct exynos_drm_crtc_state *exynos_crtc_state;
	int i;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		exynos_crtc = to_exynos_crtc(crtc);

		if (!new_crtc_state->active)
			continue;

		if (exynos_crtc_state->skip_update)
			continue;

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		if (exynos_crtc_state->skip_frame_update &&
				crtc_get_encoder(new_crtc_state, DRM_MODE_ENCODER_DSI)) {
			pr_info("skip_frame_update\n");
			continue;
		}
#endif
		ops = exynos_crtc->ops;
		if (ops->wait_framestart)
			ops->wait_framestart(exynos_crtc);

		if (ops->set_trigger)
			ops->set_trigger(exynos_crtc, exynos_crtc_state);
#if IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
		if (ops->set_fingerprint_mask)
			ops->set_fingerprint_mask(exynos_crtc, 1);
#endif
	}
}

static void exynos_atomic_set_freq_hop(struct drm_atomic_state *old_state, bool en)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_drm_crtc_ops *ops;
	const struct dpu_freq_hop_ops *fh_ops;
	int i;

	/* request to change DPHY PLL frequency */
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);

		if (!new_crtc_state->active)
			continue;

		fh_ops = exynos_crtc->freq_hop;
		if (!fh_ops)
			continue;

		if (fh_ops->update_freq_hop && en)
			fh_ops->update_freq_hop(exynos_crtc);

		if (fh_ops->set_freq_hop)
			fh_ops->set_freq_hop(exynos_crtc, en);

		ops = exynos_crtc->ops;
		if (en == false && ops->pll_sleep_mask)
			ops->pll_sleep_mask(exynos_crtc, false);
	}
}

static void exynos_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	unsigned int hibernation_crtc_mask = 0;

	DPU_ATRACE_BEGIN(__func__);

	DPU_ATRACE_BEGIN("modeset");
	exynos_atomic_block_hiber(old_state, &hibernation_crtc_mask);

	exynos_atomic_commit_modeset_disables(dev, old_state);

	exynos_atomic_update_conn_bts(old_state);

	exynos_atomic_commit_modeset_enables(dev, old_state);

	/* request to change DPHY PLL frequency */
	exynos_atomic_set_freq_hop(old_state, true);

	exynos_atomic_bts_pre_update(dev, old_state);
	DPU_ATRACE_END("modeset");

	DPU_ATRACE_BEGIN("commit_planes");
	drm_atomic_helper_commit_planes(dev, old_state,	DRM_PLANE_COMMIT_ACTIVE_ONLY);
	DPU_ATRACE_END("commit_planes");

	/*
	 * hw is flushed at this point, signal flip done for fake commit to
	 * unblock nonblocking atomic commits once vblank occurs
	 */
	if (old_state->fake_commit)
		complete_all(&old_state->fake_commit->flip_done);

	drm_atomic_helper_fake_vblank(old_state);

	exynos_atomic_wait_for_vblanks(dev, old_state);

	exynos_atomic_wait_for_framestart(old_state);

	exynos_atomic_framestart_post_processing(old_state);

	exynos_atomic_bts_post_update(dev, old_state);

	/* After shadow update, changed PLL is applied and target M value is stored */
	exynos_atomic_set_freq_hop(old_state, false);

	exynos_atomic_unblock_hiber(old_state, &hibernation_crtc_mask);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);

	DPU_ATRACE_END(__func__);
}

static struct drm_mode_config_helper_funcs exynos_drm_mode_config_helpers = {
	.atomic_commit_tail = exynos_atomic_commit_tail,
};

static const struct drm_mode_config_funcs exynos_drm_mode_config_funcs = {
	.fb_create = exynos_user_fb_create,
	.get_format_info = exynos_get_format_info,
	.output_poll_changed = exynos_drm_output_poll_changed,
	.atomic_check = exynos_atomic_check,
	.atomic_commit = exynos_atomic_commit,
};

static int exynos_drm_mode_config_init(struct drm_device *drm_dev)
{
	int ret;

	ret = drmm_mode_config_init(drm_dev);
	if (ret)
		return ret;

	drm_dev->mode_config.min_width = 0;
	drm_dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(8192x8192).
	 * this value would be used to check the limitation of framebuffer size
	 * and display mode resolution.
	 */
	drm_dev->mode_config.max_width = 8192;
	drm_dev->mode_config.max_height = 8192;
	drm_dev->mode_config.funcs = &exynos_drm_mode_config_funcs;
	drm_dev->mode_config.helper_private = &exynos_drm_mode_config_helpers;

	/* create properties ahead of binding to make them available to all drivers */
	ret = exynos_drm_crtc_create_properties(drm_dev);
	ret |= exynos_drm_plane_create_properties(drm_dev);
	ret |= exynos_drm_connector_create_properties(drm_dev);

	return ret;
}

static int exynos_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv;

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	return 0;
}

static void exynos_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	kfree(file->driver_priv);
	file->driver_priv = NULL;
}

static void exynos_drm_lastclose(struct drm_device *dev)
{
	exynos_drm_fbdev_restore_mode(dev);
}

static const struct file_operations exynos_drm_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= exynos_drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.unlocked_ioctl	= drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.release	= drm_release,
};

static struct drm_driver exynos_drm_driver = {
	.driver_features	   = DRIVER_MODESET | DRIVER_ATOMIC |
				     DRIVER_RENDER | DRIVER_GEM,
	.open			   = exynos_drm_open,
	.lastclose		   = exynos_drm_lastclose,
	.postclose		   = exynos_drm_postclose,
	.dumb_create		   = exynos_drm_gem_dumb_create,
	.dumb_map_offset	   = exynos_drm_gem_dumb_map_offset,
	.prime_handle_to_fd	   = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	   = drm_gem_prime_fd_to_handle,
	.gem_prime_import	   = exynos_drm_gem_prime_import,
	.gem_prime_import_sg_table = exynos_drm_gem_prime_import_sg_table,
	.fops			   = &exynos_drm_driver_fops,
	.name			   = DRIVER_NAME,
	.desc			   = DRIVER_DESC,
	.date			   = DRIVER_DATE,
	.major			   = DRIVER_MAJOR,
	.minor			   = DRIVER_MINOR,
};

/* forward declaration */
static struct platform_driver exynos_drm_platform_driver;
extern struct platform_driver decon_driver;
extern struct platform_driver dsim_driver;
extern struct platform_driver dpp_driver;
extern struct platform_driver writeback_driver;
extern struct platform_driver dsimfc_driver;
extern struct platform_driver dp_driver;
extern struct platform_driver rcd_driver;

struct exynos_drm_driver_info {
	struct platform_driver *driver;
	unsigned int flags;
};

#define DRM_COMPONENT_DRIVER	BIT(0)	/* supports component framework */
#define DRM_VIRTUAL_DEVICE	BIT(1)	/* create virtual platform device */

#define DRV_PTR(drv, cond) (IS_ENABLED(cond) ? &drv : NULL)

/*
 * NOTE:
 * Connector drivers should not be placed before associated crtc drivers,
 * because connector requires pipe number of its crtc during initialization.
 * Writeback connector should be bound before bts init.
 * Dsimfc_driver should be registered before dsim_driver,
 * because dsimfc drvdata need to be connected to dsim_driver.
 * Teset driver should be probed after all driver probed.
 */
static struct exynos_drm_driver_info exynos_drm_drivers[] = {
	{ DRV_PTR(dpp_driver, CONFIG_DRM_SAMSUNG_DPP),		DRM_COMPONENT_DRIVER},
	{ DRV_PTR(writeback_driver, CONFIG_DRM_SAMSUNG_WB), 	DRM_COMPONENT_DRIVER},
	{ DRV_PTR(decon_driver,	CONFIG_DRM_SAMSUNG_DECON), 	DRM_COMPONENT_DRIVER},
	{ DRV_PTR(dsimfc_driver, CONFIG_EXYNOS_DMA_DSIMFC), 	0},
	{ DRV_PTR(dsim_driver, CONFIG_DRM_SAMSUNG_DSI), 	DRM_COMPONENT_DRIVER},
	{ DRV_PTR(dp_driver, CONFIG_DRM_SAMSUNG_DP), 		DRM_COMPONENT_DRIVER},
	{ &exynos_drm_platform_driver,				DRM_VIRTUAL_DEVICE},
};

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static struct component_match *exynos_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(exynos_drm_drivers); ++i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];
		struct device *p = NULL, *d;

		if (!info->driver || !(info->flags & DRM_COMPONENT_DRIVER))
			continue;

		while ((d = platform_find_device_by_driver(p,
						&info->driver->driver))) {
			put_device(p);
			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);
}

static void exynos_drm_atomic_private_obj_fini(struct drm_device *dev, void *res)
{
	struct exynos_drm_private *private = res;

	drm_atomic_private_obj_fini(&private->obj);
}

static struct exynos_drm_private *
exynos_drm_atomic_private_obj_create(struct drm_device *dev)
{
	struct exynos_drm_private *private;
	struct exynos_drm_priv_state *priv_state;
	int ret;

	private = drmm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return ERR_PTR(-ENOMEM);

	mutex_init(&private->lock);

	priv_state = kzalloc(sizeof(*priv_state), GFP_KERNEL);
	if (!priv_state)
		return ERR_PTR(-ENOMEM);

	drm_atomic_private_obj_init(dev, &private->obj, &priv_state->base,
			&exynos_priv_state_funcs);

	ret = drmm_add_action_or_reset(dev, exynos_drm_atomic_private_obj_fini, private);
	if (ret)
		return ERR_PTR(ret);

	return private;
}

static void exynos_drm_kms_helper_poll_fini(struct drm_device *dev, void *res)
{
	drm_kms_helper_poll_fini(dev);
}

static int exynos_drm_kms_helper_poll_init(struct drm_device *dev)
{
	drm_kms_helper_poll_init(dev);

	return drmm_add_action_or_reset(dev, exynos_drm_kms_helper_poll_fini, NULL);
}

static int exynos_drm_vblank_init(struct drm_device *dev)
{
	/*
	 * If true, vblank interrupt will be disabled immediately when the
	 * refcount drops to zero, as opposed to via the vblank disable
	 * timer.
	 */
	dev->vblank_disable_immediate = true;

	return drm_vblank_init(dev, dev->mode_config.num_crtc);
}

static int exynos_drm_bind(struct device *dev)
{
	struct exynos_drm_private *private;
	struct exynos_drm_device *exynos_dev;
	struct drm_device *drm;
	int ret, win_cnt;

	exynos_dev = dev_get_drvdata(dev);
	drm = &exynos_dev->base;

	ret = exynos_drm_mode_config_init(drm);
	if (ret)
		return ret;

	private = exynos_drm_atomic_private_obj_create(drm);
	if (IS_ERR(private))
		return PTR_ERR(private);

	drm->dev_private = (void *)private;

	exynos_dev->fault_ctx = exynos_drm_create_dpu_fault_context(drm);
	if (IS_ERR(exynos_dev->fault_ctx))
		return PTR_ERR(exynos_dev->fault_ctx);

	/* Try to bind all sub drivers. */
	ret = component_bind_all(drm->dev, drm);
	if (ret)
		return ret;

	win_cnt = get_plane_num(drm);
	mutex_lock(&private->lock);
	private->available_win_mask = BIT(win_cnt) - 1;
	mutex_unlock(&private->lock);

	exynos_drm_encoder_set_possible_mask(drm);

	ret = exynos_drm_vblank_init(drm);
	if (ret)
		return ret;

	drm_mode_config_reset(drm);

	/* init kms poll for handling hpd */
	ret = exynos_drm_kms_helper_poll_init(drm);
	if (ret)
		return ret;

	ret = exynos_drm_fbdev_init(drm);
	if (ret)
		return ret;

	/* register the DRM device */
	ret = drm_dev_register(drm, 0);
	if (ret)
		return ret;

	return 0;
}

static void exynos_drm_unbind(struct device *dev)
{
	struct exynos_drm_device *exynos_dev = dev_get_drvdata(dev);
	struct drm_device *drm = &exynos_dev->base;

	drm_dev_unregister(drm);

	component_unbind_all(drm->dev, drm);

	drm->dev_private = NULL;
}

static const struct component_master_ops exynos_drm_ops = {
	.bind		= exynos_drm_bind,
	.unbind		= exynos_drm_unbind,
};

static int exynos_drm_platform_probe(struct platform_device *pdev)
{
	struct exynos_drm_device *exynos_dev;
	struct component_match *match;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	exynos_dev = devm_drm_dev_alloc(&pdev->dev, &exynos_drm_driver,
			struct exynos_drm_device, base);
	if (IS_ERR(exynos_dev))
		return PTR_ERR(exynos_dev);

	platform_set_drvdata(pdev, exynos_dev);

	match = exynos_drm_match_add(&pdev->dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(&pdev->dev, &exynos_drm_ops,
					       match);
}

static int exynos_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &exynos_drm_ops);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver exynos_drm_platform_driver = {
	.probe	= exynos_drm_platform_probe,
	.remove	= exynos_drm_platform_remove,
	.driver	= {
		.name	= "exynos-drm",
	},
};

static void exynos_drm_unregister_devices(void)
{
	int i;

	for (i = ARRAY_SIZE(exynos_drm_drivers) - 1; i >= 0; --i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];
		struct device *dev;

		if (!info->driver || !(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		while ((dev = platform_find_device_by_driver(NULL,
						&info->driver->driver))) {
			put_device(dev);
			platform_device_unregister(to_platform_device(dev));
		}
	}
}

static int exynos_drm_register_devices(void)
{
	struct platform_device *pdev;
	int i;

	for (i = 0; i < ARRAY_SIZE(exynos_drm_drivers); ++i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];

		if (!info->driver || !(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		pdev = platform_device_register_simple(
					info->driver->driver.name, -1, NULL, 0);
		if (IS_ERR(pdev))
			goto fail;
	}

	return 0;
fail:
	exynos_drm_unregister_devices();
	return PTR_ERR(pdev);
}

static void exynos_drm_unregister_drivers(void)
{
	int i;

	for (i = ARRAY_SIZE(exynos_drm_drivers) - 1; i >= 0; --i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];

		if (!info->driver)
			continue;

		platform_driver_unregister(info->driver);
	}
}

static int exynos_drm_register_drivers(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(exynos_drm_drivers); ++i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];

		if (!info->driver)
			continue;

		ret = platform_driver_register(info->driver);
		if (ret)
			goto fail;
	}
	return 0;
fail:
	exynos_drm_unregister_drivers();
	return ret;
}

static int exynos_drm_init(void)
{
	int ret;

	ret = exynos_drm_register_devices();
	if (ret)
		return ret;

	ret = exynos_drm_register_drivers();
	if (ret)
		goto err_unregister_pdevs;

	return 0;

err_unregister_pdevs:
	exynos_drm_unregister_devices();

	return ret;
}

static void exynos_drm_exit(void)
{
	exynos_drm_unregister_drivers();
	exynos_drm_unregister_devices();
}

module_init(exynos_drm_init);
module_exit(exynos_drm_exit);

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_AUTHOR("Seung-Woo Kim <sw0312.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC DRM Driver");
MODULE_LICENSE("GPL");
