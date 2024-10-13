// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_writeback.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	Wonyeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/dma-buf.h>
#include <soc/samsung/exynos/exynos-hvc.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>

#include <drm/drm_atomic.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include <exynos_drm_fb.h>
#include <exynos_drm_format.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_crtc.h>

static int dpu_writeback_log_level = 6;
module_param(dpu_writeback_log_level, int, 0600);
MODULE_PARM_DESC(dpu_writeback_log_level, "log level for writeback [default : 6]");

#define wb_info(wb, fmt, ...)	\
dpu_pr_info(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

#define wb_warn(wb, fmt, ...)	\
dpu_pr_warn(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

#define wb_err(wb, fmt, ...)	\
dpu_pr_err(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

#define wb_debug(wb, fmt, ...)	\
dpu_pr_debug(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

static const char * writeback_get_type_name(enum exynos_drm_writeback_type type);

static inline enum exynos_drm_writeback_type
get_wb_type(const struct drm_connector_state *state)
{
	struct drm_crtc *crtc = state->crtc;
	const struct decon_device *decon;

	if (!crtc) {
		pr_err("[%s] no attach wb to crtc\n", __func__);
		return EXYNOS_WB_NONE;
	}
	decon = to_exynos_crtc(crtc)->ctx;

	return decon->config.out_type == DECON_OUT_WB ?
				EXYNOS_WB_SWB : EXYNOS_WB_CWB;
}

static inline bool is_wb_job_empty(struct drm_writeback_connector *wb_conn)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&wb_conn->job_lock, flags);
	ret = list_empty(&wb_conn->job_queue);
	spin_unlock_irqrestore(&wb_conn->job_lock, flags);

	return ret;
}

void wb_dump(struct writeback_device *wb)
{
	if (!wb)
		return;

	if (wb->state != WB_STATE_ON) {
		wb_info(wb, "writeback state is off\n");
		return;
	}

	__dpp_dump(wb->id, &wb->regs);
}

static const struct of_device_id wb_of_match[] = {
	{
		.compatible = "samsung,exynos-writeback",
	},
	{},
};

static inline bool repeater_buf_is_used(const struct repeater_dma_buf *rdb,
				const struct exynos_drm_writeback_state *wb_state)
{
	return rdb->active && wb_state->use_repeater_buffer;
}

static void repeater_buf_update_config(struct dpp_params_info *config,
		const struct exynos_drm_writeback_state *state)
{
	int i;
	struct drm_framebuffer *fb = state->base.writeback_job->fb;
	struct writeback_device *wb = conn_to_wb_dev(state->base.connector);

	/* 1. init */
	for (i = 0; i < MAX_PLANE_ADDR_CNT; i++)
		config->addr[i] = wb->rdb.dma_addr[state->repeater_buf_idx];
	config->votf_o_idx = state->repeater_buf_idx;
	config->votf_o_en = true;

	/* 2. adjustment */
	switch (fb->format->format) {
	case DRM_FORMAT_NV12:
		config->addr[1] = NV12N_CBCR_BASE(config->addr[0],
				fb->width, fb->height);
		config->src.w = NV12N_STRIDE(fb->width);
		config->src.f_w = NV12N_STRIDE(fb->width);
		break;
	case DRM_FORMAT_P010:
		config->addr[1] = config->addr[0] +
			NV12N_P010_Y_SIZE(fb->width, fb->height);
		config->src.w = fb->width;
		config->src.f_w = fb->width;

		break;
	default:
		wb_err(wb, "format(%p4cc) is not supported\n", &fb->format->format);
		break;
	}
}

static void _repeater_buf_put(struct repeater_dma_buf *rdb)
{
	struct shared_buffer_info *rdb_info = &rdb->buf_info;
	int i;

	for (i = 0; i < rdb_info->buffer_count; i++) {
		if (rdb->attachment[i] && rdb->sg_table[i])
			dma_buf_unmap_attachment(rdb->attachment[i],
					rdb->sg_table[i], DMA_TO_DEVICE);
		if (rdb_info->bufs[i] && rdb->attachment[i])
			dma_buf_detach(rdb_info->bufs[i], rdb->attachment[i]);
		if (rdb_info->bufs[i])
			dma_buf_put(rdb_info->bufs[i]);
	}
}

static void repeater_buf_put(struct repeater_dma_buf *rdb)
{
	if (!rdb->active)
		return;

	_repeater_buf_put(rdb);
	rdb->active = false;
	repeater_dpu_dpms_off();
}

static void repeater_buf_get(struct writeback_device *wb,
			     struct exynos_drm_writeback_state *wb_state)
{
	struct repeater_dma_buf *rdb = &wb->rdb;
	struct shared_buffer_info *rdb_info = &rdb->buf_info;
	const struct decon_device *decon = wb_get_decon(wb);
	int i;

	/* already repeater buffer requested */
	if (wb->rdb.active)
		return;

	/* votf can't be supported */
	if (!wb->rdb.votf_supported || wb_state->type == EXYNOS_WB_CWB)
		return;

	if (repeater_request_buffer(&rdb->buf_info, 0)) {
		wb_debug(wb, "repeater buffer request failed\n");
		return;
	}

	for (i = 0; i < rdb_info->buffer_count; i++) {
		rdb->attachment[i] = dma_buf_attach(rdb->buf_info.bufs[i], decon->dev);
		if (IS_ERR(rdb->attachment[i])) {
			rdb->attachment[i] = NULL;
			goto err_map_buf;
		}

		rdb->sg_table[i] = dma_buf_map_attachment(rdb->attachment[i], DMA_TO_DEVICE);
		if (IS_ERR(rdb->sg_table[i])) {
			rdb->sg_table[i] = NULL;
			goto err_map_buf;
		}

		rdb->dma_addr[i] = sg_dma_address(rdb->sg_table[i]->sgl);
	}
	wb->rdb.active = true;

	return;

err_map_buf:
	_repeater_buf_put(&wb->rdb);
	wb_err(wb, "repeater buffer map failed\n");

	return;
}

static int repeater_buf_acquire(struct writeback_device *wb,
				struct exynos_drm_writeback_state *wb_state)
{
	struct drm_framebuffer *fb = wb_state->base.writeback_job->fb;
	struct repeater_dma_buf *rdb = &wb->rdb;
	size_t size = 0, rdb_size;

	if (!wb_state->use_repeater_buffer)
		return 0;

	repeater_buf_get(wb, wb_state);
	if (!rdb->active) {
		wb_err(wb, "failed to request repeater buffer\n");
		return -ENOMEM;
	}

	if (repeater_get_valid_buffer(&rdb->buf_idx)) {
		wb_err(wb, "failed to get valid repeater buffer idx\n");
		return -EINVAL;
	}
	wb_state->repeater_buf_idx = rdb->buf_idx;

	switch (fb->format->format) {
	case DRM_FORMAT_NV12:
		size = NV12N_Y_SIZE(fb->width, fb->height) +
			NV12N_CBCR_SIZE(fb->width, fb->height);
		break;
	case DRM_FORMAT_P010:
		size =  NV12N_P010_Y_SIZE(fb->width, fb->height) +
			NV12N_P010_CBCR_SIZE(fb->width, fb->height);
		break;
	default:
		wb_err(wb, "format(%p4cc) is not supported\n", &fb->format->format);
		return -EINVAL;
	}

	rdb_size = rdb->buf_info.bufs[wb_state->repeater_buf_idx]->size;
	if (size > rdb_size) {
		wb_err(wb, "bad rdb size(%zu), should be >= %zu\n", rdb_size, size);
		return -ENOMEM;
	}

	return 0;
}

void repeater_buf_release(const struct drm_connector *conn,
			  const struct drm_connector_state *new_conn_state)
{
	const struct writeback_device *wb = conn_to_wb_dev(conn);
	const struct exynos_drm_writeback_state *wb_state =
		to_exynos_wb_state(new_conn_state);

	if (repeater_buf_is_used(&wb->rdb, wb_state)) {
		tsmux_blending_start(wb_state->repeater_buf_idx);
		repeater_set_valid_buffer(wb_state->repeater_buf_idx,
				wb_state->repeater_buf_idx);
	}
}

bool wb_notify_error(struct writeback_device *wb)
{
	const struct drm_connector_state *conn_state;
	const struct exynos_drm_writeback_state *wb_state;

	if (!wb)
		return false;

	conn_state = wb->writeback.base.state;
	wb_state = to_exynos_wb_state(conn_state);
	if (!repeater_buf_is_used(&wb->rdb, wb_state))
		return false;

	repeater_notify_error(REQ_BUF_OWNER_DPU);
	return true;
}

static int writeback_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	const static struct drm_display_mode dft_mode = {
		DRM_MODE("4096x4096", DRM_MODE_TYPE_DRIVER, 556188, 4096,
				4097, 4098, 4099, 0, 4096, 4097, 4098, 4099, 0,
				DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) };
	int num_modes = 0;

	mode = drm_mode_duplicate(connector->dev, &dft_mode);
	if (!mode)
		return num_modes;

	drm_mode_probed_add(connector, mode);
	++num_modes;

	mode->type |= DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return num_modes;
}

static void wb_convert_connector_state_to_config(struct dpp_params_info *config,
				const struct exynos_drm_writeback_state *state)
{
	struct drm_framebuffer *fb = state->base.writeback_job->fb;
	const struct writeback_device *wb = conn_to_wb_dev(state->base.connector);
	const struct drm_crtc_state *crtc_state = state->base.crtc->state;
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	wb_debug(wb, "+\n");

	config->src.x = 0;
	config->src.y = 0;
	config->src.f_w = fb->width;
	config->src.f_h = fb->height;
	config->in_bpc = exynos_crtc_state->in_bpc >= 10 ? DPP_BPC_10 : DPP_BPC_8;
	switch (state->type) {
	case EXYNOS_WB_CWB:
		config->src.w = crtc_state->mode.hdisplay;
		config->src.h = crtc_state->mode.vdisplay;
		break;
	case EXYNOS_WB_SWB:
		config->src.w = fb->width;
		config->src.h = fb->height;
		break;
	default:
		config->src.w = fb->width;
		config->src.h = fb->height;
		wb_err(wb, "unsupported wb_type(%d)\n", state->type);
		break;
	}

	config->comp_type = COMP_TYPE_NONE;

	config->format = fb->format->format;
	config->standard = state->standard;
	config->range = state->range;
	config->stride[0] = 0;
	config->stride[1] = 0;
	config->stride[2] = 0;
	config->stride[3] = 0;

	config->votf_o_en = false;
	config->addr[0] = exynos_drm_fb_dma_addr(fb, 0);
	config->addr[1] = exynos_drm_fb_dma_addr(fb, 1);
	config->addr[2] = exynos_drm_fb_dma_addr(fb, 2);
	config->addr[3] = exynos_drm_fb_dma_addr(fb, 3);
	if (repeater_buf_is_used(&wb->rdb, state))
		repeater_buf_update_config(config, state);

	/* TODO: blocking mode will be implemented later */
	config->is_block = false;
	/* TODO: very big count.. recovery will be not working... */
	config->rcv_num = 0x7FFFFFFF;

	wb_debug(wb, "-\n");
}

static int writeback_check_cwb(const struct writeback_device *wb,
		struct drm_crtc_state *crtc_state, const struct drm_framebuffer *fb)
{
	if (!fb)
		return 0;

	/* the fb smaller than lcd resol are not supported */
	if (fb->width < crtc_state->mode.hdisplay ||
			fb->height < crtc_state->mode.vdisplay) {
		wb_err(wb, "Invalid fb size, fb size < LCD resolution\n");
		return -EINVAL;
	}

	return 0;
}

static int writeback_check_swb(const struct writeback_device *wb,
		struct drm_crtc_state *crtc_state, const struct drm_framebuffer *fb)
{
	if (!fb && crtc_state->active && crtc_state->plane_mask) {
		struct exynos_drm_crtc_state *exynos_crtc_state = to_exynos_crtc_state(crtc_state);

		exynos_crtc_state->modeset_only = true;
		wb_warn(wb, "SWB commit is requested w/o out-buffer\n");
	}

	/* the fb bigger than 4K x 4K are not supported */
	if (fb && (fb->width > crtc_state->mode.hdisplay ||
			fb->height > crtc_state->mode.vdisplay)) {
		wb_err(wb, "Invalid fb size, fb size > max wb size\n");
		return -EINVAL;
	}

	return 0;
}

struct exynos_writeback_job {
	int wb_type;
};

static int writeback_prepare_job(struct writeback_device *wb,
				 struct drm_connector_state *conn_state)
{
	struct drm_writeback_job *job;
	struct exynos_writeback_job *exynos_job;
	struct exynos_drm_writeback_state *wb_state;

	job = conn_state->writeback_job;
	if (!job)
		return 0;

	wb_state = to_exynos_wb_state(conn_state);

	exynos_job = kzalloc(sizeof(*exynos_job), GFP_KERNEL);
	if (!exynos_job)
		return -ENOMEM;

	exynos_job->wb_type = wb_state->type;
	if (repeater_buf_acquire(wb, wb_state)) {
		kfree(exynos_job);
		return -ENOMEM;
	}

	job->priv = exynos_job;
	job->prepared = true;

	return 0;
}

static void writeback_cleanup_job(struct drm_writeback_connector *connector,
				  struct drm_writeback_job *job)
{
	struct exynos_writeback_job *exynos_job = job->priv;

	kfree(exynos_job);
}

static int __check_protection_mismatch(const struct drm_crtc_state *new_crtc_state,
				      const struct drm_writeback_job *job)
{
	struct writeback_device *wb = to_wb_dev(job->connector);
	const struct drm_plane_state *new_plane_state;
	struct drm_plane *plane;

	if (DRM_MODE_GET_PROT(job->fb->modifier))
		return 0;

	drm_atomic_crtc_state_for_each_plane_state(plane, new_plane_state, new_crtc_state) {
		if (!new_plane_state->fb)
			continue;

		if (DRM_MODE_GET_PROT(new_plane_state->fb->modifier)) {
			wb_err(wb, "plane[%d] is protectd, but wb isn't protectd\n",
					drm_plane_index(plane));
			return -EINVAL;
		}
	}

	return 0;
}

static int writeback_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct writeback_device *wb = enc_to_wb_dev(encoder);
	const struct drm_framebuffer *fb = NULL;
	struct exynos_drm_writeback_state *wb_state = to_exynos_wb_state(conn_state);
	int wb_type, ret = 0;

	if (wb_check_job(conn_state))
		fb = conn_state->writeback_job->fb;

	wb_type = get_wb_type(conn_state);
	if (wb_type == EXYNOS_WB_CWB)
		ret = writeback_check_cwb(wb, crtc_state, fb);
	else if (wb_type == EXYNOS_WB_SWB)
		ret = writeback_check_swb(wb, crtc_state, fb);
	if (ret)
		goto out;

	if (!is_wb_job_empty(&wb->writeback)) {
		wb_err(wb, "previous job is not yet signaled\n");
		ret = -EBUSY;
		goto out;
	}

	if (!wb_check_job(conn_state))
		goto out;

	ret = __check_protection_mismatch(crtc_state, conn_state->writeback_job);
	if (ret)
		goto out;

	ret = drm_atomic_helper_check_wb_encoder_state(encoder, conn_state);
	if (ret)
		goto out;

	wb_state->type = wb_type;
	ret = writeback_prepare_job(wb, conn_state);
	if (ret)
		goto out;

out:
	if (ret && fb)
		wb_err(wb, "fb size %ux%u, %s mode size %ux%u\n",
				fb->width, fb->height, writeback_get_type_name(wb_type),
				crtc_state->mode.hdisplay, crtc_state->mode.vdisplay);

	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
static void set_protection(struct writeback_device *wb, uint64_t modifier)
{
	bool protection;
	u32 protection_id;
	int ret = 0;

	protection = DRM_MODE_GET_PROT(modifier);
	if (wb->protection == protection)
		return;

	protection_id = PROT_WB0;

	ret = exynos_hvc(HVC_PROTECTION_SET, 0, protection_id,
			(protection ? HVC_PROTECTION_ENABLE :
			HVC_PROTECTION_DISABLE), 0);
	if (ret) {
		wb_err(wb, "failed to %s protection(ret:%d)\n",
				protection ? "enable" : "disable", ret);
		return;
	}
	wb->protection = protection;

	wb_debug(wb, "en:%d\n", protection);

	return;
}
#else
static inline void
set_protection(struct writeback_device *wb, uint64_t modifier) { }
#endif

static void writeback_atomic_commit(struct drm_connector *connector,
				    struct drm_atomic_state *state)
{
	struct drm_connector_state *new_conn_state =
			drm_atomic_get_new_connector_state(state, connector);
	struct drm_writeback_connector *wb_conn = conn_to_wb_conn(connector);
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(new_conn_state->crtc);
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct exynos_drm_writeback_state *wb_state = to_exynos_wb_state(new_conn_state);
	struct dpp_params_info *config = &wb->win_config;

	if (wb->state == WB_STATE_OFF) {
		wb_info(wb, "disabled(%d), can't support commit\n", wb->state);
		return;
	}

	wb_debug(wb, "+\n");

	wb_convert_connector_state_to_config(config, to_exynos_wb_state(new_conn_state));
	set_protection(wb, new_conn_state->writeback_job->fb->modifier);
	dpp_reg_configure_params(wb->id, config, wb->attr);
	if (wb_state->type == EXYNOS_WB_SWB)
		decon_reg_config_wb_size(wb->decon_id, config->src.h, config->src.w);

	drm_writeback_queue_job(wb_conn, new_conn_state);

	DPU_EVENT_LOG("WB_ATOMIC_COMMIT", exynos_crtc, EVENT_FLAG_LONG,
			dpu_config_to_string, &exynos_crtc->bts->wb_config);

	wb_debug(wb, "-\n");
}

static const struct drm_connector_helper_funcs wb_connector_helper_funcs = {
	.get_modes = writeback_get_modes,
	.atomic_commit = writeback_atomic_commit,
	.cleanup_writeback_job = writeback_cleanup_job,
};

struct drm_connector_state *
exynos_drm_writeback_duplicate_state(struct drm_connector *connector)
{
	struct exynos_drm_writeback_state *exynos_state;
	struct exynos_drm_writeback_state *copy;

	exynos_state = to_exynos_wb_state(connector->state);
	copy = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (!copy)
		return NULL;

	memcpy(copy, exynos_state, sizeof(*exynos_state));
	copy->type = EXYNOS_WB_NONE;
	copy->use_repeater_buffer = false;
	__drm_atomic_helper_connector_duplicate_state(connector, &copy->base);

	return &copy->base;
}

static void exynos_drm_writeback_destroy_state(struct drm_connector *connector,
		struct drm_connector_state *old_state)
{
	struct exynos_drm_writeback_state *old_exynos_state =
						to_exynos_wb_state(old_state);

	__drm_atomic_helper_connector_destroy_state(old_state);
	kfree(old_exynos_state);
}

static void exynos_drm_writeback_reset(struct drm_connector *connector)
{
	struct exynos_drm_writeback_state *exynos_state;

	if (connector->state) {
		exynos_drm_writeback_destroy_state(connector, connector->state);
		connector->state = NULL;
	}

	exynos_state = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (exynos_state) {
		connector->state = &exynos_state->base;
		connector->state->connector = connector;
		connector->state->self_refresh_aware = true;
		/*
		 * If it needs to initialize the specific property value,
		 * please add to here.
		 */
	}
}

static void exynos_drm_writeback_destroy(struct drm_connector *connector)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);

	drm_connector_cleanup(connector);
	kfree(wb);
}

static int exynos_drm_writeback_set_property(struct drm_connector *connector,
				struct drm_connector_state *state,
				struct drm_property *property, uint64_t val)
{
	const struct exynos_drm_properties *p = dev_get_exynos_props(connector->dev);
	struct exynos_drm_writeback_state *exynos_state =
						to_exynos_wb_state(state);

	if (property == p->standard)
		exynos_state->standard = val;
	else if (property == p->range)
		exynos_state->range = val;
	else if (property == p->use_repeater_buffer)
		exynos_state->use_repeater_buffer = val;
	else
		return -EINVAL;

	return 0;
}

static int exynos_drm_writeback_get_property(struct drm_connector *connector,
				const struct drm_connector_state *state,
				struct drm_property *property, uint64_t *val)
{
	const struct exynos_drm_properties *p = dev_get_exynos_props(connector->dev);
	struct exynos_drm_writeback_state *exynos_state =
						to_exynos_wb_state(state);

	if (property == p->standard)
		*val = exynos_state->standard;
	else if (property == p->range)
		*val = exynos_state->range;
	else if (property == p->use_repeater_buffer)
		*val = exynos_state->use_repeater_buffer;
	else
		return -EINVAL;

	return 0;
}

static const char * writeback_get_type_name(enum exynos_drm_writeback_type type)
{
	static const char * const writeback_type_name[] = {
		[EXYNOS_WB_NONE] = "None",
		[EXYNOS_WB_CWB] = "Concurrent",
		[EXYNOS_WB_SWB] = "Stand_alone",
	};

	if (WARN_ON(type >= ARRAY_SIZE(writeback_type_name)))
		return "Unknown";

	return writeback_type_name[type];
}

static void exynos_drm_writeback_print_state(struct drm_printer *p,
		const struct drm_connector_state *state)
{
	const struct exynos_drm_writeback_state *wb_state =
		to_exynos_wb_state(state);
	const struct writeback_device *wb = conn_to_wb_dev(state->connector);

	drm_printf(p, "\trepeater_buffer: en=%d idx=%d\n",
		wb_state->use_repeater_buffer, wb_state->repeater_buf_idx);
	drm_printf(p, "\ttype: %s\n", writeback_get_type_name(wb_state->type));

	drm_printf(p, "\tDPP #%d", wb->id);
	if (wb->state == WB_STATE_OFF) {
		drm_printf(p, " (off)\n");
		return;
	}

	drm_printf(p, "\n\t\tport=%d\n", wb->port);
	drm_printf(p, "\t\tdecon_id=%d\n", wb->decon_id);
	drm_printf(p, "\t\tprotection=%d\n", wb->protection);
}

static const struct drm_connector_funcs wb_connector_funcs = {
	.reset = exynos_drm_writeback_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = exynos_drm_writeback_destroy,
	.atomic_duplicate_state = exynos_drm_writeback_duplicate_state,
	.atomic_destroy_state = exynos_drm_writeback_destroy_state,
	.atomic_set_property = exynos_drm_writeback_set_property,
	.atomic_get_property = exynos_drm_writeback_get_property,
	.atomic_print_state = exynos_drm_writeback_print_state,
};

static void _writeback_enable(struct writeback_device *wb)
{
	exynos_dpuf_set_ring_clk(wb->dpuf, true);
	dpp_reg_init(wb->id, wb->attr);
	enable_irq(wb->odma_irq);
}

static void writeback_atomic_enable(struct drm_encoder *encoder,
		struct drm_atomic_state *state)
{
	struct writeback_device *wb = enc_to_wb_dev(encoder);
	const struct decon_device *decon;

	if (wb->state == WB_STATE_ON) {
		wb_info(wb, "already enabled(%d)\n", wb->state);
		return;
	}

	wb_debug(wb, "+\n");

	pm_runtime_get_sync(wb->dev);
	_writeback_enable(wb);
	decon = wb_get_decon(wb);
	wb->decon_id = decon->id;
	wb->state = WB_STATE_ON;

	DPU_EVENT_LOG("WB_ENABLE", decon->crtc, 0, NULL);

	wb_debug(wb, "-\n");
}

static void _writeback_disable(struct writeback_device *wb)
{
	disable_irq(wb->odma_irq);
	exynos_dpuf_set_ring_clk(wb->dpuf, false);
	dpp_reg_deinit(wb->id, false, wb->attr);
}

static void writeback_atomic_disable(struct drm_encoder *encoder,
			       struct drm_atomic_state *state)
{
	struct writeback_device *wb = enc_to_wb_dev(encoder);
	struct drm_connector_state *old_conn_state;
	struct exynos_drm_crtc *exynos_crtc;

	old_conn_state = drm_atomic_get_old_connector_state(state,
			&wb->writeback.base);

	wb_debug(wb, "+\n");

	if (wb->state == WB_STATE_OFF) {
		wb_info(wb, "already disabled(%d)\n", wb->state);
		return;
	}

	_writeback_disable(wb);
	set_protection(wb, 0);
	repeater_buf_put(&wb->rdb);
	wb->state = WB_STATE_OFF;
	wb->decon_id = -1;
	pm_runtime_put_sync(wb->dev);
	exynos_crtc = to_exynos_crtc(old_conn_state->crtc);
	DPU_EVENT_LOG("WB_DISABLE", exynos_crtc, 0, NULL);

	wb_debug(wb, "-\n");
}

static const struct drm_encoder_helper_funcs wb_encoder_helper_funcs = {
	.atomic_enable = writeback_atomic_enable,
	.atomic_disable = writeback_atomic_disable,
	.atomic_check = writeback_atomic_check,
};

static int writeback_bind(struct device *dev, struct device *master, void *data)
{
	struct writeback_device *wb = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct drm_connector *connector = &wb->writeback.base;
	const struct exynos_drm_properties *p = dev_get_exynos_props(drm_dev);
	int ret;

	wb_info(wb, "+\n");

	drm_connector_helper_add(connector, &wb_connector_helper_funcs);
	ret = drm_writeback_connector_init(drm_dev, &wb->writeback,
			&wb_connector_funcs, &wb_encoder_helper_funcs,
			wb->pixel_formats, wb->num_pixel_formats, 0);
	if (ret) {
		wb_err(wb, "failed to init writeback connector\n");
		return ret;
	}

	drm_object_attach_property(&connector->base, p->standard,
			EXYNOS_STANDARD_UNSPECIFIED);
	drm_object_attach_property(&connector->base, p->range,
			EXYNOS_RANGE_UNSPECIFIED);
	if (wb->rdb.votf_supported)
		drm_object_attach_property(&connector->base,
				p->use_repeater_buffer, false);

	wb_info(wb, "-\n");

	return 0;
}

static void
writeback_unbind(struct device *dev, struct device *master, void *data)
{
	const struct writeback_device *wb = dev_get_drvdata(dev);

	wb_debug(wb, "+\n");
	wb_debug(wb, "-\n");
}

static const struct component_ops exynos_wb_component_ops = {
	.bind	= writeback_bind,
	.unbind	= writeback_unbind,
};

static int exynos_wb_parse_dt(struct writeback_device *wb,
				struct device_node *np)
{
	int dpuf_id, ret;
	struct dpp_params_info *config = &wb->win_config;

	ret = of_property_read_u32(np, "dpp,id", &wb->id);
	ret |= of_property_read_u32(np, "attr", (u32 *)&wb->attr);
	ret |= of_property_read_u32(np, "port", &wb->port);
	if (ret)
		return ret;

	if (of_property_read_u32(np, "dpuf,id", &dpuf_id))
		dpuf_id = 0;
	wb->dpuf = dpuf_blkdata[dpuf_id];

	wb->num_pixel_formats = exynos_parse_formats(wb->dev, &wb->pixel_formats);
	if (wb->num_pixel_formats <= 0) {
		wb_err(wb, "failed to parse format list\n");
		return wb->num_pixel_formats;
	}

	wb->rdb.active = false;
	wb->rdb.votf_supported = of_property_read_bool(np, "votf_o,enabled");
	if (wb->rdb.votf_supported) {
		ret = of_property_read_u32(np, "votf_o,mfc_base", &config->votf_o_mfc_addr);
		if (ret) {
			wb_err(wb, "failed to parse mfc_votf_base address(%d)\n", ret);
			return ret;
		}
	}

	wb_info(wb, "attr(0x%lx), port(%d)\n", wb->attr, wb->port);

	return ret;
}

static void out_fence_signal(struct drm_writeback_job *job, int status)
{
	struct dma_fence *out_fence = job->out_fence;

	if (!out_fence)
		return;

	if (status)
		dma_fence_set_error(out_fence, status);
	dma_fence_signal(out_fence);
	dma_fence_put(out_fence);
	job->out_fence = NULL;
}

static void cleanup_work(struct work_struct *work)
{
	struct drm_writeback_job *job = container_of(work,
						     struct drm_writeback_job,
						     cleanup_work);
	struct exynos_writeback_job *exynos_job = job->priv;

	if (exynos_job->wb_type == EXYNOS_WB_CWB) {
		drm_gem_fb_begin_cpu_access(job->fb, DMA_FROM_DEVICE);
		out_fence_signal(job, 0);
	}
	drm_writeback_cleanup_job(job);
}

/* See also: drm_writeback_signal_completion() */
static void
exynos_drm_writeback_signal_compeltion(struct writeback_device *wb, int status)
{
	struct drm_writeback_connector *wb_connector = &wb->writeback;
	const struct drm_connector_state *conn_state = wb_connector->base.state;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(conn_state->crtc);
	const struct exynos_drm_writeback_state *wb_state =
				to_exynos_wb_state(conn_state);
	unsigned long flags;
	struct drm_writeback_job *job;

	spin_lock_irqsave(&wb_connector->job_lock, flags);
	job = list_first_entry_or_null(&wb_connector->job_queue,
				       struct drm_writeback_job, list_entry);
	if (job)
		list_del(&job->list_entry);

	spin_unlock_irqrestore(&wb_connector->job_lock, flags);

	if (!job) {
		wb_info(wb, "framedone/instant off irq occurs w/o job\n");
		return;
	}

	if (wb_state->type == EXYNOS_WB_CWB)
		decon_reg_set_cwb_enable(wb->decon_id, false);
	else
		out_fence_signal(job, status);

	INIT_WORK(&job->cleanup_work, cleanup_work);
	queue_work(system_long_wq, &job->cleanup_work);

	DPU_EVENT_LOG("WB_FRAMEDONE", exynos_crtc, 0, NULL);
	if (repeater_buf_is_used(&wb->rdb, wb_state))
		tsmux_blending_end(wb_state->repeater_buf_idx);
}

static irqreturn_t odma_irq_handler(int irq, void *priv)
{
	struct writeback_device *wb = priv;
	u32 irqs;

	spin_lock(&wb->odma_slock);
	if (wb->state == WB_STATE_OFF)
		goto irq_end;

	irqs = odma_reg_get_irq_and_clear(wb->id);

	if (irqs & (ODMA_STATUS_FRAMEDONE_IRQ | ODMA_INST_OFF_DONE_IRQ)) {
		if (irqs & ODMA_STATUS_FRAMEDONE_IRQ)
			wb_debug(wb, "framedone irq occurs\n");
		else
			wb_warn(wb, "instant off irq occurs\n");

		exynos_drm_writeback_signal_compeltion(wb, 0);
	}

	if ((irqs & ODMA_WRITE_SLAVE_ERROR) || (irqs & ODMA_STATUS_DEADLOCK_IRQ)) {
		wb_err(wb, "error irq occur(0x%x)\n", irqs);
		wb_dump(wb);
		goto irq_end;
	}

irq_end:
	spin_unlock(&wb->odma_slock);
	return IRQ_HANDLED;
}

static int wb_init_resources(struct writeback_device *wb)
{
	int ret = 0;
	struct device *dev = wb->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int i;

	wb_debug(wb, "+\n");

	pdev = to_platform_device(dev);

	i = of_property_match_string(np, "reg-names", "dma");
	wb->regs.dma_base_regs = of_iomap(np, i);
	if (!wb->regs.dma_base_regs) {
		wb_err(wb, "failed to remap DPU_DMA SFR region\n");
		return -EINVAL;
	}
	dpp_regs_desc_init(wb->regs.dma_base_regs, "dma", REGS_DMA, wb->id);

	wb->odma_irq = of_irq_get_byname(np, "dma");
	wb_info(wb, "dma irq no = %d\n", wb->odma_irq);
	ret = devm_request_irq(dev, wb->odma_irq, odma_irq_handler, 0,
			pdev->name, wb);
	if (ret) {
		wb_err(wb, "failed to install DPU DMA irq\n");
		return -EINVAL;
	}
	disable_irq(wb->odma_irq);

	if (test_bit(DPP_ATTR_DPP, &wb->attr)) {
		i = of_property_match_string(np, "reg-names", "dpp");
		wb->regs.dpp_base_regs = of_iomap(np, i);
		if (!wb->regs.dpp_base_regs) {
			wb_err(wb, "failed to remap DPP SFR region\n");
			return -EINVAL;
		}
		dpp_regs_desc_init(wb->regs.dpp_base_regs, "dpp", REGS_DPP,
				wb->id);
	}

	i = of_property_match_string(np, "reg-names", "votf");
	if (i >= 0) {
		wb->regs.votf_base_regs = of_iomap(np, i);
		if (!wb->regs.votf_base_regs) {
			wb_err(wb, "failed to remap VOTF SFR region\n");
			return -EINVAL;
		}
		dpp_regs_desc_init(wb->regs.votf_base_regs, "votf", REGS_VOTF, wb->id);
	}

	wb_debug(wb, "-\n");

	return ret;
}

static int writeback_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct writeback_device *writeback;

	writeback = devm_kzalloc(dev, sizeof(struct writeback_device),
			GFP_KERNEL);
	if (!writeback)
		return -ENOMEM;

	writeback->dev = dev;
	ret = exynos_wb_parse_dt(writeback, dev->of_node);
	if (ret)
		goto fail;

	writeback->output_type = EXYNOS_DISPLAY_TYPE_VIDI;

	spin_lock_init(&writeback->odma_slock);

	writeback->state = WB_STATE_OFF;
	pm_runtime_enable(writeback->dev);

	ret = wb_init_resources(writeback);
	if (ret)
		goto fail;

	/* dpp is not connected decon now */
	writeback->decon_id = -1;

	platform_set_drvdata(pdev, writeback);

	wb_info(writeback, "successfully probed\n");

	ret = component_add(dev, &exynos_wb_component_ops);
	if (ret)
		goto fail;

	return ret;

fail:
	wb_err(writeback, "probe failed\n");
	return ret;
}

static int writeback_remove(struct platform_device *pdev)
{
	struct writeback_device *wb = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	component_del(&pdev->dev, &exynos_wb_component_ops);

	if (test_bit(DPP_ATTR_DPP, &wb->attr))
		iounmap(wb->regs.dpp_base_regs);
	iounmap(wb->regs.dma_base_regs);

	return 0;
}

struct platform_driver writeback_driver = {
	.probe = writeback_probe,
	.remove = writeback_remove,
	.driver = {
		   .name = "exynos-writeback",
		   .owner = THIS_MODULE,
		   .of_match_table = wb_of_match,
	},
};
EXPORT_SYMBOL(writeback_driver);

MODULE_AUTHOR("Wonyeong Choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC Display WriteBack");
MODULE_LICENSE("GPL v2");
