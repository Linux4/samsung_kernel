// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-fence.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "mtk_vkms_drv.h"

static enum hrtimer_restart vkms_vblank_simulate(struct hrtimer *timer)
{
	struct vkms_output *output = container_of(timer, struct vkms_output,
						  vblank_hrtimer);
	struct drm_crtc *crtc = &output->crtc;
	struct vkms_crtc_state *state;
	u64 ret_overrun;
	bool ret, fence_cookie;

	fence_cookie = dma_fence_begin_signalling();

	ret_overrun = hrtimer_forward_now(&output->vblank_hrtimer,
					  output->period_ns);
	//if (ret_overrun != 1)
		//pr_warn("%s: vblank timer overrun\n", __func__);

	spin_lock(&output->lock);
	ret = drm_crtc_handle_vblank(crtc);
	if (!ret)
		DRM_ERROR("vkms failure on handling vblank");

	state = output->composer_state;
	spin_unlock(&output->lock);

	if (state && output->composer_enabled) {
		u64 frame = drm_crtc_accurate_vblank_count(crtc);

		/* update frame_start only if a queued vkms_composer_worker()
		 * has read the data
		 */
		spin_lock(&output->composer_lock);
		if (!state->crc_pending)
			state->frame_start = frame;
		else {
			DRM_DEBUG_DRIVER("crc worker falling behind, frame_start: %llu\n",
					 state->frame_start);
			DRM_DEBUG_DRIVER("crc worker falling behind, frame_end: %llu\n", frame);
		}
		state->frame_end = frame;
		state->crc_pending = true;
		spin_unlock(&output->composer_lock);

		ret = queue_work(output->composer_workq, &state->composer_work);
		if (!ret)
			DRM_DEBUG_DRIVER("Composer worker already queued\n");
	}

	dma_fence_end_signalling(fence_cookie);

	return HRTIMER_RESTART;
}

static int vkms_enable_vblank(struct drm_crtc *crtc)
{
	struct vkms_output *out = drm_crtc_to_vkms_output(crtc);

	drm_calc_timestamping_constants(crtc, &crtc->mode);

	hrtimer_init(&out->vblank_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	out->vblank_hrtimer.function = &vkms_vblank_simulate;
	out->period_ns = ktime_set(0, VKMS_VBLANK_TIMER);
	hrtimer_start(&out->vblank_hrtimer, out->period_ns, HRTIMER_MODE_REL);

	return 0;
}

static void vkms_disable_vblank(struct drm_crtc *crtc)
{
	struct vkms_output *out = drm_crtc_to_vkms_output(crtc);

	hrtimer_cancel(&out->vblank_hrtimer);
}

static bool vkms_get_vblank_timestamp(struct drm_crtc *crtc,
				      int *max_error, ktime_t *vblank_time,
				      bool in_vblank_irq)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = crtc->index;
	struct vkms_device *vkmsdev = drm_device_to_vkms_device(dev);
	struct vkms_output *output = &vkmsdev->output;
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	if (!READ_ONCE(vblank->enabled)) {
		*vblank_time = ktime_get();
		return true;
	}

	*vblank_time = READ_ONCE(output->vblank_hrtimer.node.expires);

	if (WARN_ON(*vblank_time == vblank->time))
		return true;

	/*
	 * To prevent races we roll the hrtimer forward before we do any
	 * interrupt processing - this is how real hw works (the interrupt is
	 * only generated after all the vblank registers are updated) and what
	 * the vblank core expects. Therefore we need to always correct the
	 * timestampe by one frame.
	 */
	*vblank_time -= output->period_ns;

	return true;
}

static struct drm_crtc_state *
vkms_atomic_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct vkms_crtc_state *vkms_state;

	if (WARN_ON(!crtc->state))
		return NULL;

	vkms_state = kzalloc(sizeof(*vkms_state), GFP_KERNEL);
	if (!vkms_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &vkms_state->base);

	INIT_WORK(&vkms_state->composer_work, vkms_composer_worker);

	return &vkms_state->base;
}

static void vkms_atomic_crtc_destroy_state(struct drm_crtc *crtc,
					   struct drm_crtc_state *state)
{
	struct vkms_crtc_state *vkms_state = to_vkms_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(state);

	WARN_ON(work_pending(&vkms_state->composer_work));
	kfree(vkms_state->active_planes);
	kfree(vkms_state);
}

static void vkms_atomic_crtc_reset(struct drm_crtc *crtc)
{
	struct vkms_crtc_state *vkms_state =
		kzalloc(sizeof(*vkms_state), GFP_KERNEL);

	if (crtc->state)
		vkms_atomic_crtc_destroy_state(crtc, crtc->state);

	__drm_atomic_helper_crtc_reset(crtc, &vkms_state->base);
	if (vkms_state)
		INIT_WORK(&vkms_state->composer_work, vkms_composer_worker);
}
static int mtk_drm_crtc_get_property(struct drm_crtc *crtc,
				     const struct drm_crtc_state *state,
				     struct drm_property *property,
				     uint64_t *val)
{
	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);
	*val = 0;
	return 0;
}
int mtk_atomic_helper_set_config(struct drm_mode_set *set,
				 struct drm_modeset_acquire_ctx *ctx)
{

	DDPINFO("[TEST_drm]%s:%d bitmap begin\n", __func__, __LINE__);
	if (dummy_bitmap_enable == 1) {
		drm_framebuffer_get(set->fb);
		if (!mmp_fb_to_bitmap(set->fb))
			DDPINFO("[TEST_drm]%s:%d bitmap ok\n", __func__, __LINE__);
		else
			DDPINFO("[TEST_drm]%s:%d bitmap fail\n", __func__, __LINE__);
	};

	//ret = drm_atomic_helper_set_config(set,ctx);

	return 0;

}

static const struct drm_crtc_funcs vkms_crtc_funcs = {
	.set_config             = mtk_atomic_helper_set_config,
	.destroy                = drm_crtc_cleanup,
	.page_flip              = drm_atomic_helper_page_flip,
	.reset                  = vkms_atomic_crtc_reset,
	.atomic_duplicate_state = vkms_atomic_crtc_duplicate_state,
	.atomic_destroy_state   = vkms_atomic_crtc_destroy_state,
	.atomic_get_property = mtk_drm_crtc_get_property,
	.enable_vblank		= vkms_enable_vblank,
	.disable_vblank		= vkms_disable_vblank,
	.get_vblank_timestamp	= vkms_get_vblank_timestamp,
	.get_crc_sources	= vkms_get_crc_sources,
	.set_crc_source		= vkms_set_crc_source,
	.verify_crc_source	= vkms_verify_crc_source,
};

static int vkms_crtc_atomic_check(struct drm_crtc *crtc,
				  struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state,
									  crtc);
	struct vkms_crtc_state *vkms_state = to_vkms_crtc_state(crtc_state);
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	int i = 0, ret;

	if (vkms_state->active_planes)
		return 0;

	ret = drm_atomic_add_affected_planes(crtc_state->state, crtc);
	if (ret < 0)
		return ret;

	drm_for_each_plane_mask(plane, crtc->dev, crtc_state->plane_mask) {
		plane_state = drm_atomic_get_existing_plane_state(crtc_state->state,
								  plane);
		WARN_ON(!plane_state);

		if (!plane_state->visible)
			continue;

		i++;
	}

	vkms_state->active_planes = kcalloc(i, sizeof(plane), GFP_KERNEL);
	if (!vkms_state->active_planes)
		return -ENOMEM;
	vkms_state->num_active_planes = i;

	i = 0;
	drm_for_each_plane_mask(plane, crtc->dev, crtc_state->plane_mask) {
		plane_state = drm_atomic_get_existing_plane_state(crtc_state->state,
								  plane);

		if (!plane_state->visible)
			continue;

		vkms_state->active_planes[i++] =
			to_vkms_plane_state(plane_state);
	}

	return 0;
}

static void vkms_crtc_atomic_enable(struct drm_crtc *crtc,
				    struct drm_atomic_state *state)
{
	drm_crtc_vblank_on(crtc);
}

static void vkms_crtc_atomic_disable(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	drm_crtc_vblank_off(crtc);
}

static void vkms_crtc_atomic_begin(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	struct vkms_output *vkms_output = drm_crtc_to_vkms_output(crtc);

	/* This lock is held across the atomic commit to block vblank timer
	 * from scheduling vkms_composer_worker until the composer is updated
	 */
	spin_lock_irq(&vkms_output->lock);
}

static void vkms_crtc_atomic_flush(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	struct vkms_output *vkms_output = drm_crtc_to_vkms_output(crtc);

	if (crtc->state->event) {
		spin_lock(&crtc->dev->event_lock);

		if (drm_crtc_vblank_get(crtc) != 0)
			drm_crtc_send_vblank_event(crtc, crtc->state->event);
		else
			drm_crtc_arm_vblank_event(crtc, crtc->state->event);

		spin_unlock(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}

	vkms_output->composer_state = to_vkms_crtc_state(crtc->state);

	spin_unlock_irq(&vkms_output->lock);
}

static const struct drm_crtc_helper_funcs vkms_crtc_helper_funcs = {
	.atomic_check	= vkms_crtc_atomic_check,
	.atomic_begin	= vkms_crtc_atomic_begin,
	.atomic_flush	= vkms_crtc_atomic_flush,
	.atomic_enable	= vkms_crtc_atomic_enable,
	.atomic_disable	= vkms_crtc_atomic_disable,
};
static struct mtk_drm_property mtk_crtc_property[CRTC_PROP_MAX] = {
	{DRM_MODE_PROP_ATOMIC, "PRESENT_FENCE", 0, UINT_MAX, 0},
};

static void mtk_drm_crtc_attach_property(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_property *prop;
	static struct drm_property *mtk_crtc_prop[CRTC_PROP_MAX];
	struct mtk_drm_property *crtc_prop;
	int index = drm_crtc_index(crtc);
	int i;
	static int num;

	DDPINFO("%s:%d crtc:%d\n", __func__, __LINE__, index);

	if (num == 0) {
		for (i = 0; i < CRTC_PROP_MAX; i++) {
			crtc_prop = &(mtk_crtc_property[i]);
			mtk_crtc_prop[i] = drm_property_create_range(
				dev, crtc_prop->flags, crtc_prop->name,
				crtc_prop->min, crtc_prop->max);
			if (!mtk_crtc_prop[i]) {
				DDPPR_ERR("fail to create property:%s\n",
					  crtc_prop->name);
				return;
			}
			DDPINFO("create property:%s, flags:0x%x\n",
				crtc_prop->name, mtk_crtc_prop[i]->flags);
		}
		num++;
	}

	for (i = 0; i < CRTC_PROP_MAX; i++) {
		DDPINFO("%s:%d\n", __func__, __LINE__);
		prop = private->crtc_property[index][i];
		DDPINFO("%s:%d\n", __func__, __LINE__);
		crtc_prop = &(mtk_crtc_property[i]);
		DDPINFO("%s:%d prop:%p\n", __func__, __LINE__, prop);
		if (!prop) {
			prop = mtk_crtc_prop[i];
		      private
			->crtc_property[index][i] = prop;
			drm_object_attach_property(&crtc->base, prop,
						   crtc_prop->val);
		}
	}
}

void vkms_crtc_para(struct drm_crtc *crtc)
{
	drm_mode_copy(&crtc->mode, &default_mode);
	crtc->gamma_size = MTK_LUT_SIZE;
	mtk_drm_crtc_attach_property(crtc);
}

int vkms_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
		   struct drm_plane *primary, struct drm_plane *cursor)
{
	struct vkms_output *vkms_out = drm_crtc_to_vkms_output(crtc);
	int ret;

	disp_dbg_init(dev);

	ret = drm_crtc_init_with_planes(dev, crtc, primary, cursor,
					&vkms_crtc_funcs, NULL);
	if (ret) {
		DRM_ERROR("Failed to init CRTC\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &vkms_crtc_helper_funcs);
	vkms_crtc_para(crtc);

	spin_lock_init(&vkms_out->lock);
	spin_lock_init(&vkms_out->composer_lock);

	vkms_out->composer_workq = alloc_ordered_workqueue("vkms_composer", 0);
	if (!vkms_out->composer_workq)
		return -ENOMEM;

	return ret;
}
