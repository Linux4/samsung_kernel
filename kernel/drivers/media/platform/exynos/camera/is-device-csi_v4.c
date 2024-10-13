/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * exynos fimc-is/mipi-csi functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/io.h>
#include <linux/phy/phy.h>
#include <linux/fs.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/firmware.h>
#include <linux/kfifo.h>

#include "pablo-debug.h"
#include "is-config.h"
#include "is-core.h"
#include "is-device-csi.h"
#include "is-device-sensor.h"
#include "is-param.h"
#include "pablo-irq.h"
#include "is-device-camif-dma.h"
#include "is-hw-phy.h"
#include "is-hw-api-csi.h"
#include "pablo-kernel-variant.h"
#include "is-vendor.h"
#include "pablo-blob.h"
#include "is-interface-sensor.h"

#define IS_CSI_VC_FRO_OUT(csi, idx_wdma, wdma_vc)                                                  \
	(csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_FRO)

#define INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc)                                        \
	(test_bit(PABLO_SUBDEV_ALLOC, &i_subdev->state) &&                                         \
		!IS_CSI_VC_FRO_OUT(csi, idx_wdma, wdma_vc))

#define WDMA_MAX(csi) ((csi->sensor_cfg) ? csi->sensor_cfg->dma_num : CSIS_MAX_NUM_DMA_ATTACH)

#define DBG_IRQ_INFO_MAX 20

static int csi_s_fro(struct is_device_csi *csi,
		struct is_device_sensor *device,
		int otf_ch)
{
	struct is_camif_wdma_module *wdma_mod;
	u32 dma_vc, dma_ch, idx_wdma;
	struct is_camif_wdma *wdma;

	FIMC_BUG(!csi->sensor_cfg);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		dma_ch = wdma->ch;
		wdma_mod = csi->wdma_mod[idx_wdma];

		if (!wdma_mod) {
			merr("[CSI%d][DMA%d] wdma_mod is NULL", csi, csi->otf_info.csi_ch, dma_ch);
			return -ENODEV;
		}

		for (dma_vc = DMA_VIRTUAL_CH_0; dma_vc < DMA_VIRTUAL_CH_MAX; dma_vc++) {
			if (csi->sensor_cfg->output[idx_wdma][dma_vc].type == VC_FRO)
				csi_hw_s_dma_common_frame_id_decoder(wdma_mod->regs, dma_ch,
								     csi->f_id_dec);
		}

		if ((csi->f_id_dec &&
		     csi_hw_get_version(csi->base_reg) < IS_CSIS_VERSION(5, 4, 0, 0)) ||
		    csi->otf_batch_num > 1) {
			/*
			* v5.1: Both FRO count and frame id decoder
			* dosen't have to be controlled
			* if shadowing scheme is supported
			* at start interrupt of frame id decoder.
			*/
			for (dma_vc = DMA_VIRTUAL_CH_0; dma_vc < DMA_VIRTUAL_CH_MAX; dma_vc++)
				csi_hw_s_fro_count(wdma->regs_ctl, csi->dma_batch_num, dma_vc);
		}
	}

	if (csi->fro_reg)
		csi_hw_s_otf_preview_only(csi->fro_reg, otf_ch, csi->f_id_dec);

	mcinfo(" fro batch_num (otf:%d, dma:%d)\n", csi, csi->otf_batch_num, csi->dma_batch_num);

	return 0;
}

static int csi_dma_attach(struct is_device_csi *csi)
{
	int ret = 0;
	int wdma_vc, idx_wdma;
	unsigned long flag;
	struct is_camif_wdma *wdma;
	int wdma_ch_hint;

	spin_lock_irqsave(&csi->dma_seq_slock, flag);

	if (csi->wdma[CSI_OTF_OUT_SINGLE]) {
		mcwarn(" DMA is already attached.", csi);
		goto err_already_attach;
	}

	/*
	 * If there is wdma_ch_hint, specific WDMA channel will be selected.
	 * Or, WDMA channel will be selected dynamically.
	 */
	if (csi->sensor_cfg && csi->sensor_cfg->wdma_ch_hint < INIT_WDMA_CH_HINT)
		wdma_ch_hint = csi->sensor_cfg->wdma_ch_hint;
	else
		wdma_ch_hint = csi->wdma_ch_hint;

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = is_camif_wdma_get(wdma_ch_hint);
		if (!wdma) {
			merr("[CSI%d] is_camif_wdma_get is failed", csi, csi->otf_info.csi_ch);
			ret = -ENODEV;
			goto err_wdma_get;
		}
		csi->wdma[idx_wdma] = wdma;
		csi->wdma_mod[idx_wdma] = is_camif_wdma_module_get(wdma->ch);

		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			csi_hw_dma_reset(wdma->regs_vc[wdma_vc]);
			csi_hw_s_fro_count(wdma->regs_ctl, 1, wdma_vc); /* reset FRO count */
		}
	}

	if (csi->sensor_cfg && csi->sensor_cfg->ex_mode == EX_PDSTAT_OFF) {
		struct is_vci_config *s_cfg_output;
		int cvc, stat_flag = false;

		for (idx_wdma = 0; idx_wdma < csi->sensor_cfg->dma_num; idx_wdma++) {
			for (cvc = DMA_VIRTUAL_CH_0; cvc < DMA_VIRTUAL_CH_MAX; cvc++) {
				s_cfg_output = &csi->sensor_cfg->output[idx_wdma][cvc];

				if (!s_cfg_output->width)
					continue;

				stat_flag = true;
				break;
			}
		}

		if (stat_flag) {
			wdma = is_camif_wdma_get(INIT_WDMA_CH_HINT);
			if (wdma) {
				csi->stat_wdma = wdma;
				csi->stat_wdma_mod = is_camif_wdma_module_get(wdma->ch);

				for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX;
				     wdma_vc++) {
					csi_hw_dma_reset(wdma->regs_vc[wdma_vc]);
					csi_hw_s_fro_count(wdma->regs_ctl, 1, wdma_vc);
				}
			}
		}
	}

	spin_unlock_irqrestore(&csi->dma_seq_slock, flag);

	return ret;

err_wdma_get:
err_already_attach:
	spin_unlock_irqrestore(&csi->dma_seq_slock, flag);

	return ret;
}

static void csi_dma_detach(struct is_device_csi *csi)
{
	struct is_camif_wdma *wdma;
	int idx_wdma;
	unsigned long flags;

	spin_lock_irqsave(&csi->dma_rta_lock, flags);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];

		csi->wdma[idx_wdma] = NULL;
		is_camif_wdma_put(wdma);
	}

	if (csi->stat_wdma) {
		is_camif_wdma_put(csi->stat_wdma);
		csi->stat_wdma = NULL;
	}

	spin_unlock_irqrestore(&csi->dma_rta_lock, flags);
}

/* External only */
static void csis_flush_vc_buf_done(struct is_device_csi *csi, u32 idx_wdma, u32 wdma_vc,
				   enum is_frame_state target, enum vb2_buffer_state state)
{
	struct is_device_sensor *device;
	struct is_subdev *dma_subdev;
	struct is_framemgr *ldr_framemgr;
	struct is_framemgr *framemgr;
	struct is_frame *ldr_frame;
	struct is_frame *frame;
	struct is_video_ctx *vctx;
	u32 findex;
	unsigned long flags;

	if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN))
		return;

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	FIMC_BUG_VOID(!device);

	/* buffer done for several virtual ch 0 ~ 8, internal vc is skipped */
	dma_subdev = csi->dma_subdev[idx_wdma][wdma_vc];
	if (!dma_subdev || !test_bit(IS_SUBDEV_START, &dma_subdev->state))
		return;

	ldr_framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev->leader);
	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
	vctx = dma_subdev->vctx;

	FIMC_BUG_VOID(!ldr_framemgr);
	FIMC_BUG_VOID(!framemgr);

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	frame = peek_frame(framemgr, target);
	while (frame) {
		if (target == FS_PROCESS) {
			findex = frame->stream->findex;
			ldr_frame = &ldr_framemgr->frames[findex];
			clear_bit(dma_subdev->id, &ldr_frame->out_flag);
		}

		if (state == VB2_BUF_STATE_ERROR)
			mserr("[F%d][%s] NDONE(%d, E%X)\n", dma_subdev, dma_subdev,
					frame->fcount, frame_state_name[target],
					frame->index, frame->result);

		CALL_VOPS(vctx, done, frame->index, state);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, target);
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);
}

/* Internal only */
static void csis_flush_vc_multibuf(struct is_device_csi *csi, u32 idx_wdma, u32 wdma_vc)
{
	int i;
	struct pablo_internal_subdev *i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	if (!INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc))
		return;

	framemgr = GET_SUBDEV_I_FRAMEMGR(i_subdev);
	if (framemgr) {
		framemgr_e_barrier_irqs(framemgr, 0, flags);
		for (i = 0; i < framemgr->num_frames; i++) {
			frame = &framemgr->frames[i];

			if (frame->state == FS_PROCESS
				|| frame->state == FS_COMPLETE) {
				trans_frame(framemgr, frame, FS_FREE);
			}
		}
		framemgr_x_barrier_irqr(framemgr, 0, flags);
	}

	clear_bit(CSIS_SET_MULTIBUF_DMA0 + idx_wdma, &csi->state);
}

static void csis_flush_all_vc_buf_done(struct is_device_csi *csi, u32 state)
{
	u32 idx_wdma, wdma_vc;

	FIMC_BUG_VOID(!csi->sensor_cfg);

	/* buffer done for several virtual ch */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			csis_flush_vc_buf_done(csi, idx_wdma, wdma_vc, FS_PROCESS, state);
			csis_flush_vc_buf_done(csi, idx_wdma, wdma_vc, FS_REQUEST, state);

			csis_flush_vc_multibuf(csi, idx_wdma, wdma_vc);
		}
	}
}

/* External only */
static void csis_check_vc_dma_buf(struct is_device_csi *csi)
{
	u32 idx_wdma, wdma_vc;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_subdev *dma_subdev;
	struct is_camif_wdma *wdma;
	unsigned long flags;

	if (!test_bit(CSIS_START_STREAM, &csi->state))
		return;

	FIMC_BUG_VOID(!csi->sensor_cfg);

	/* default disable dma setting for several virtual ch 0 ~ 7 */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			if (!IS_ENABLED(CONFIG_SENSOR_GROUP_LVN)) {
				dma_subdev = csi->dma_subdev[idx_wdma][wdma_vc];
				/* skip for internal vc use of not opened vc */
				if (!dma_subdev || !test_bit(IS_SUBDEV_OPEN, &dma_subdev->state))
					continue;

				framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
				if (wdma_vc != DMA_VIRTUAL_CH_0 && csi->stat_wdma)
					wdma = csi->stat_wdma;
				else
					wdma = csi->wdma[wdma_vc];

				if (likely(framemgr) && wdma) {
					framemgr_e_barrier_irqs(framemgr, 0, flags);

					/* NDONE for the frame in ERR state */
					frame = peek_frame(framemgr, FS_PROCESS);
					if (frame) {
						if (!csi->pre_dma_enable[idx_wdma][wdma_vc] &&
						    !csi->cur_dma_enable[idx_wdma][wdma_vc]) {
							mcinfo("[WDMA%d][VC%d] wq_csis_dma is being delayed. [P(%d)]\n",
							       csi, wdma->ch, wdma_vc,
							       framemgr->queued_count[FS_PROCESS]);
							print_frame_queue(framemgr, FS_PROCESS);

							if (!csi_hw_g_output_cur_dma_enable(
								    wdma->regs_vc[wdma_vc],
								    wdma_vc))
								frame->result = IS_SHOT_TIMEOUT;
						}

						if (frame->result) {
							mserr("[F%d] NDONE(%d, E%X)\n", dma_subdev,
							      dma_subdev, frame->fcount,
							      frame->index, frame->result);
							trans_frame(framemgr, frame, FS_COMPLETE);
							CALL_VOPS(dma_subdev->vctx, done,
								  frame->index,
								  VB2_BUF_STATE_ERROR);
						}
					}

					/* print information DMA on/off */
					if (csi->pre_dma_enable[idx_wdma][wdma_vc] !=
					    csi->cur_dma_enable[idx_wdma][wdma_vc]) {
						mcinfo("[WDMA%d][VC%d][F%d] DMA %s [%d/%d/%d]\n",
						       csi, wdma->ch, wdma_vc,
						       atomic_read(&csi->fcount),
						       (csi->cur_dma_enable[idx_wdma][wdma_vc] ?
								"on" :
								"off"),
						       framemgr->queued_count[FS_REQUEST],
						       framemgr->queued_count[FS_PROCESS],
						       framemgr->queued_count[FS_COMPLETE]);

						csi->pre_dma_enable[idx_wdma][wdma_vc] =
							csi->cur_dma_enable[idx_wdma][wdma_vc];
					}

					/* after update pre_dma_disable, clear dma enable state */
					csi->cur_dma_enable[idx_wdma][wdma_vc] = 0;

					framemgr_x_barrier_irqr(framemgr, 0, flags);
				} else {
					mcerr("[VC%d] framemgr is NULL", csi, wdma_vc);
				}
			} else {
				if (!csi->sensor_cfg->output[idx_wdma][wdma_vc].width)
					continue;

				/* print information DMA on/off */
				if (csi->pre_dma_enable[idx_wdma][wdma_vc] !=
				    csi->cur_dma_enable[idx_wdma][wdma_vc]) {
					mcinfo("[F%d] VC%d %s \n", csi, atomic_read(&csi->fcount),
					       wdma_vc,
					       (csi->cur_dma_enable[idx_wdma][wdma_vc] ? "on" :
											 "off"));

					csi->pre_dma_enable[idx_wdma][wdma_vc] =
						csi->cur_dma_enable[idx_wdma][wdma_vc];
				}

				/* after update pre_dma_disable, clear dma enable state */
				csi->cur_dma_enable[idx_wdma][wdma_vc] = 0;
			}
		}
	}
}

static inline void csi_s_multibuf_addr(struct is_device_csi *csi, struct is_frame *frame, u32 index,
				       u32 idx_wdma, u32 wdma_vc)
{
	struct is_camif_wdma *wdma;

	FIMC_BUG_VOID(!frame);

	if (wdma_vc != DMA_VIRTUAL_CH_0 && csi->stat_wdma)
		wdma = csi->stat_wdma;
	else
		wdma = csi->wdma[idx_wdma];

	csi_hw_s_multibuf_dma_addr(wdma->regs_ctl, wdma->regs_vc[wdma_vc], wdma_vc, index,
				   frame->dvaddr_buffer[0]);
}

static inline void csi_s_output_dma(struct is_device_csi *csi, u32 idx_wdma, u32 wdma_vc,
				    bool enable)
{
	struct is_camif_wdma *wdma;

	if (wdma_vc != DMA_VIRTUAL_CH_0 && csi->stat_wdma)
		wdma = csi->stat_wdma;
	else
		wdma = csi->wdma[idx_wdma];

	if (!wdma) {
		merr("[CSI%d][DMA%d][VC%d] wdma is NULL", csi, csi->otf_info.csi_ch, idx_wdma,
		     wdma_vc);
		return;
	}

	if (enable)
		csi->cur_dma_enable[idx_wdma][wdma_vc] = 1;

	csi_hw_s_output_dma(wdma->regs_vc[wdma_vc], wdma_vc, enable);
}

/* Internal only */
static void csis_s_vc_dma_multibuf(struct is_device_csi *csi)
{
	u32 idx_wdma, wdma_vc;
	int i;
	struct pablo_internal_subdev *i_subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	bool set_buf = false;

	FIMC_BUG_VOID(!csi->sensor_cfg);

	/* dma setting for several virtual ch */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		if (test_bit(CSIS_SET_MULTIBUF_DMA0 + idx_wdma, &csi->state))
			continue;

		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
			if (!INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc))
				continue;

			framemgr = GET_SUBDEV_I_FRAMEMGR(i_subdev);

			FIMC_BUG_VOID(!framemgr);

			framemgr_e_barrier_irqs(framemgr, 0, flags);
			for (i = 0; i < framemgr->num_frames; i++) {
				frame = &framemgr->frames[i];
				csi_s_multibuf_addr(csi, frame, i, idx_wdma, wdma_vc);
				csi_s_output_dma(csi, idx_wdma, wdma_vc, true);
				trans_frame(framemgr, frame, FS_FREE);
			}

			framemgr_x_barrier_irqr(framemgr, 0, flags);

			set_buf = true;
		}

		if (set_buf)
			set_bit(CSIS_SET_MULTIBUF_DMA0 + idx_wdma, &csi->state);
	}
}

static inline void csi_s_frameptr(struct is_device_csi *csi, u32 idx_wdma, u32 wdma_vc, bool clear)
{
	struct is_camif_wdma *wdma;
	u32 frameptr;

	/*
	 * Moving DMA frameptr is required for below two scenarios.
	 * case 1) Frame ID decoder based FRO
	 * case 2) CSIS link FRO
	 */
	if (!csi->f_id_dec && csi->otf_batch_num < 2)
		return;

	/* Moving DMA frameptr is available only for VC0 */
	if (wdma_vc != DMA_VIRTUAL_CH_0)
		return;

	wdma = csi->wdma[idx_wdma];
	if (!wdma) {
		mcerr("[WDMA_IDX%d][VC%d] wdma is NULL", csi, idx_wdma, wdma_vc);
		return;
	}

	frameptr = atomic_inc_return(&csi->bufring_cnt) % BUF_SWAP_CNT;
	frameptr *= csi->dma_batch_num;

	csi_hw_s_frameptr(wdma->regs_vc[wdma_vc], wdma_vc, frameptr, clear);
}

static void csis_s_vc_dma_frobuf(struct is_device_csi *csi, u32 idx_wdma, u32 wdma_vc)
{
	struct pablo_internal_subdev *i_subdev;
	struct is_camif_wdma *wdma;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	u32 b_i, f_i, frameptr, p_size;
	dma_addr_t dva;

	/*
	 * VC out type of current sensor mode must be set with VC_FRO
	 * to use internal buffer.
	 */
	if (!IS_CSI_VC_FRO_OUT(csi, idx_wdma, wdma_vc))
		return;

	i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
	wdma = csi->wdma[idx_wdma];
	framemgr = GET_SUBDEV_I_FRAMEMGR(i_subdev);

	FIMC_BUG_VOID(!framemgr);

	csi_s_frameptr(csi, idx_wdma, wdma_vc, false);
	frameptr = csi->dma_batch_num * (atomic_read(&csi->bufring_cnt) % BUF_SWAP_CNT);

	for (b_i = 0; b_i < csi->dma_batch_num; b_i++) {
		f_i = b_i % framemgr->num_frames;
		frame = &framemgr->frames[f_i];
		dva = frame->dvaddr_buffer[0];

		csi_hw_s_dma_addr(wdma->regs_ctl, wdma->regs_vc[wdma_vc], wdma_vc, b_i + frameptr,
				  dva);

		mdbg_common(is_get_debug_param(IS_DEBUG_PARAM_CSI), "[%d][CSI%d][WDMA%d][VC%d]",
			    " frobuf:[I%d] dva %pad\n", csi->instance, csi->otf_info.csi_ch,
			    wdma->ch, wdma_vc, b_i + frameptr, &dva);

		if (wdma_vc == DMA_VIRTUAL_CH_0 && (i_subdev->sbwc_type & COMP)) {
			p_size = csi_hw_g_img_stride(i_subdev->width, i_subdev->bits_per_pixel,
					 i_subdev->memory_bitwidth, i_subdev->sbwc_type) *
				 i_subdev->height;

			dva += p_size;
			csi_hw_s_dma_header_addr(wdma->regs_ctl, wdma->regs_vc[wdma_vc],
						 b_i + frameptr, dva);
		}
	}

	csi_s_output_dma(csi, idx_wdma, wdma_vc, true);
}

static u32 g_print_cnt;
static inline void csi_trigger_gtask(struct is_device_sensor *sensor, struct is_device_csi *csi)
{
	struct is_group *g_sensor, *g_next;
	struct is_group_task *ss_gtask, *is_gtask;
	u32 g_sensor_id, g_next_id = GROUP_ID_MAX;
	u32 fcount, b_fcount;

	g_sensor = &sensor->group_sensor;
	g_sensor_id = g_sensor->id;
	ss_gtask = get_group_task(g_sensor_id);

	fcount = sensor->fcount;
	b_fcount = atomic_read(&g_sensor->backup_fcount);

	/* Update sensor_fcount for next shot */
	if (fcount + g_sensor->skip_shots > b_fcount)
		atomic_set(&g_sensor->sensor_fcount, fcount + g_sensor->skip_shots);

	/* When there is no pending request from user, next gtask would make frame drop. */
	if (unlikely(list_empty(&g_sensor->smp_trigger.wait_list))) {
		/* pcount : program count, current program count(location) in kthread */
		if ((!(g_print_cnt % LOG_INTERVAL_OF_DROPS)) ||
		    (g_print_cnt < LOG_INTERVAL_OF_DROPS)) {
			mcinfo("  GP%d(res %d, rcnt %d, scnt %d) fcount %d pcount %d\n", csi,
			       g_sensor_id, ss_gtask->smp_resource.count,
			       atomic_read(&g_sensor->rcount), atomic_read(&g_sensor->scount),
			       fcount, g_sensor->pcount);

			g_next = g_sensor->gnext;
			while (g_next) {
				g_next_id = g_next->id;
				is_gtask = get_group_task(g_next_id);

				mcinfo(" GP%d(res %d, rcnt %d, scnt %d)\n", csi, g_next_id,
				       is_gtask->smp_resource.count, atomic_read(&g_next->rcount),
				       atomic_read(&g_next->scount));

				g_next = g_next->gnext;
			}
		}
		g_print_cnt++;
	} else {
		if (fcount + g_sensor->skip_shots > b_fcount) {
			dbg("%s:[F%d] up(smp_trigger) - [backup_F%d]\n", __func__, fcount,
			    b_fcount);
			up(&g_sensor->smp_trigger);
		}
		g_print_cnt = 0;
	}
}

static inline void csi_frame_start_inline(struct is_device_csi *csi, ulong fs_bits)
{
	u32 inc = 1;
	u32 fcount, chain_fcount;
	struct v4l2_subdev *sd;
	struct is_device_sensor *sensor;
	u32 hashkey, hashkey_1, hashkey_2;
	u32 index;
	u32 hw_frame_id[2] = { 0, 0 };
	u64 timestamp;
	u64 timestampboot;

	/* frame start interrupt */
	csi->sw_checker = EXPECT_FRAME_END;
	atomic_set(&csi->vvalid, 1);

	if (!test_bit(CSI_VIRTUAL_CH_0, &fs_bits))
		return;

	if (!csi->f_id_dec) {
		chain_fcount = atomic_read(&csi->chain_fcount);
		fcount = atomic_read(&csi->fcount);
		if (chain_fcount > fcount)
			inc = chain_fcount - fcount;

		if (unlikely(inc != 1))
			mcwarn("fcount is increased (%d->%d)", csi, fcount, chain_fcount);

		/* SW FRO: start interrupt have to be called only once per batch number  */
		csi->hw_fcount = csi_hw_g_fcount(csi->base_reg, CSI_VIRTUAL_CH_0);
		if (csi->otf_batch_num > 1) {
			if ((csi->hw_fcount % csi->otf_batch_num) != 1)
				return;
		}
	}

	fcount = atomic_add_return(inc, &csi->fcount);

	dbg_isr(1, "[F%d] S\n", csi, fcount);

	sd = *csi->subdev;
	sensor = v4l2_get_subdev_hostdata(sd);
	if (!sensor) {
		err("sensor is NULL");
		BUG();
	}

	/*
	 * Sometimes, frame->fcount is bigger than sensor->fcount if gtask is delayed.
	 * hashkey_1~2 is to prenvet reverse timestamp.
	 */
	sensor->fcount = fcount;
	hashkey = fcount % IS_TIMESTAMP_HASH_KEY;
	hashkey_1 = (fcount + 1) % IS_TIMESTAMP_HASH_KEY;
	hashkey_2 = (fcount + 2) % IS_TIMESTAMP_HASH_KEY;

	timestamp = ktime_get_ns();
	timestampboot = ktime_get_boottime_ns();
	sensor->timestamp[hashkey] = timestamp;
	sensor->timestamp[hashkey_1] = timestamp;
	sensor->timestamp[hashkey_2] = timestamp;
	sensor->timestampboot[hashkey] = timestampboot;
	sensor->timestampboot[hashkey_1] = timestampboot;
	sensor->timestampboot[hashkey_2] = timestampboot;

	index = fcount % DEBUG_FRAME_COUNT;
	csi->debug_info[index].fcount = fcount;
	csi->debug_info[index].instance = csi->instance;
	csi->debug_info[index].cpuid[DEBUG_POINT_FRAME_START] = raw_smp_processor_id();
	csi->debug_info[index].time[DEBUG_POINT_FRAME_START] = cpu_clock(raw_smp_processor_id());

	v4l2_subdev_notify(sd, CSI_NOTIFY_VSYNC, &fcount);

	/* check all virtual channel's dma */
	csis_check_vc_dma_buf(csi);
	/* re-set internal vc dma if flushed */
	csis_s_vc_dma_multibuf(csi);

	v4l2_subdev_notify(sd, CSIS_NOTIFY_FSTART, &fcount);
	clear_bit(IS_SENSOR_WAIT_STREAMING, &sensor->state);

	csi_trigger_gtask(sensor, csi);

	if (csi->f_id_dec) {
		struct is_camif_wdma_module *wdma_mod;

		wdma_mod = csi->wdma_mod[CSI_OTF_OUT_SINGLE];
		if (!wdma_mod) {
			mcerr(" wdma_mod is NULL", csi);
			return;
		}

		/* after increase fcount */
		csi_hw_g_dma_common_frame_id(wdma_mod->regs, csi->dma_batch_num, hw_frame_id);
		sensor->frame_id[hashkey] = ((u64)hw_frame_id[1] << 32) | (u64)hw_frame_id[0];
	}
}

void csi_set_using_vc_buffer(void *ctrl)
{
	struct is_sensor_subdev_ioctl_PD_arg args;
	struct is_device_csi *csi;
	bool use;
	u32 buffer_total_num;
	u32 idx_wdma, wdma_vc;
	struct is_frame *frame;
	dma_addr_t addr;
	unsigned long flags;

	args = *(struct is_sensor_subdev_ioctl_PD_arg *)ctrl;
	frame = args.vc_frame;
	csi = args.csi;
	use = args.start_end;

	FIMC_BUG_VOID(!csi);
	FIMC_BUG_VOID(!csi->sensor_cfg);

	/* find vc channel num using for TAILPDAF */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			if (csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_TAILPDAF)
				break;
		}
	}

	if (idx_wdma >= WDMA_MAX(csi))
		return;

	buffer_total_num = csi->sensor_cfg->output[idx_wdma][wdma_vc].buffer_num;

	addr = frame->dvaddr_buffer[0];

	spin_lock_irqsave(&csi->dma_rta_lock, flags);
	if (!csi->wdma[idx_wdma]) {
		err("csi->wdma was already detached");
		goto p_err;
	}

	if (csi->wdma[idx_wdma]->regs_vc[wdma_vc]) {
		csi_hw_s_multibuf_dma_fcntseq(
			csi->wdma[idx_wdma]->regs_vc[wdma_vc], buffer_total_num, addr, use);
	} else {
		err("csi->wdma->regs_vc[%d] is null", wdma_vc);
		goto p_err;
	}

p_err:
	spin_unlock_irqrestore(&csi->dma_rta_lock, flags);
}

static inline void csi_frame_line_inline(struct is_device_csi *csi)
{
	dbg_isr(1, "[F%d] L\n", csi, atomic_read(&csi->fcount));
	/* frame line interrupt */
	tasklet_schedule(&csi->tasklet_csis_line);
}

static inline void csi_frame_end_emul(struct is_device_csi *csi)
{
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_device_sensor *device;
	struct is_group *group;

	if (hw->fake_otf_frame_end) {
		device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
		group = &device->group_sensor;
		hw->fake_otf_frame_end(group->hw_slot_id, group->instance);
	}
}

static void csi_err_iommu_perf(is_event_store_type_t event_type, u32 instance, u32 fcount)
{
	struct is_core *core;

	core = is_get_is_core();

	is_debug_iommu_perf(false, &core->pdev->dev);

	is_debug_event_count(event_type, instance, fcount);
}

static inline void csi_frame_end_inline(struct is_device_csi *csi, ulong fe_bits)
{
	struct pablo_camif_otf_info *otf_info;
	u32 fcount = atomic_read(&csi->fcount);
	u32 index;

	/* frame end interrupt */
	csi->sw_checker = EXPECT_FRAME_START;
	atomic_set(&csi->vvalid, 0);

	otf_info = &csi->otf_info;
	if (otf_info->req_otf_out_num != otf_info->act_otf_out_num)
		tasklet_schedule(&csi->tasklet_csis_otf_cfg);

	if (!test_bit(CSI_VIRTUAL_CH_0, &fe_bits))
		return;

	if (!csi->f_id_dec) {
		/* SW FRO: start interrupt have to be called only once per batch number  */
		if (csi->otf_batch_num > 1) {
			if ((csi->hw_fcount % csi->otf_batch_num) != 0)
				return;
		}
	}

	dbg_isr(1, "[F%d] E\n", csi, fcount);

	atomic_set(&csi->vblank_count, fcount);
	v4l2_subdev_notify(*csi->subdev, CSI_NOTIFY_VBLANK, &fcount);

	tasklet_schedule(&csi->tasklet_csis_end);

	index = fcount % DEBUG_FRAME_COUNT;
	csi->debug_info[index].cpuid[DEBUG_POINT_FRAME_END] = raw_smp_processor_id();
	csi->debug_info[index].time[DEBUG_POINT_FRAME_END] = cpu_clock(raw_smp_processor_id());
}

void csi_emulate_irq(u32 instance, u32 vvalid)
{
	struct is_device_ischain *ischain = is_get_ischain_device(instance);
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;

	FIMC_BUG_VOID(!ischain);

	sensor = ischain->sensor;
	if (!sensor || !sensor->subdev_csi) {
		merr("csi is NULL", ischain);
		return;
	}

	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	switch (vvalid) {
	case V_BLANK:
		csi_frame_end_inline(csi, BIT_MASK(CSI_VIRTUAL_CH_0));
		break;
	case V_VALID:
		csi_frame_start_inline(csi, BIT_MASK(CSI_VIRTUAL_CH_0));
		break;
	default:
		/* Nothing to do */
		break;
	}
}

static inline dma_addr_t csi_get_dva(struct is_frame *frame, int idx, int link_vc)
{
	dma_addr_t dvaddr;

	if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN))
		dvaddr = frame->dva_ssvc[link_vc][idx];
	else
		dvaddr = frame->dvaddr_buffer[idx];

	return dvaddr;
}

static inline dma_addr_t csi_get_dma_header_addr(
	struct is_device_csi *csi, dma_addr_t dvaddr, u32 idx_wdma, u32 wdma_vc)
{
	struct is_device_sensor *sensor;
	struct sensor_dma_output *dma_output;
	struct is_subdev *dma_subdev;
	u32 p_size;
	u32 width, height, bpp, bitwidth, sbwc_type, link_vc;

	if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN)) {
		sensor = v4l2_get_subdev_hostdata(*csi->subdev);
		if (!sensor) {
			err("failed to get sensor output");
			return dvaddr;
		}

		link_vc = csi->sensor_cfg->link_vc[idx_wdma][wdma_vc];
		dma_output = &sensor->dma_output[link_vc];
		width = dma_output->width;
		height = dma_output->height;
		bpp = dma_output->fmt.bitsperpixel[0];
		bitwidth = dma_output->fmt.hw_bitwidth;
		sbwc_type = dma_output->sbwc_type;

	} else {
		dma_subdev = csi->dma_subdev[idx_wdma][wdma_vc];
		width = dma_subdev->output.width;
		height = dma_subdev->output.height;
		bpp = dma_subdev->bits_per_pixel;
		bitwidth = dma_subdev->memory_bitwidth;
		sbwc_type = dma_subdev->sbwc_type;
	}

	if (wdma_vc == DMA_VIRTUAL_CH_0 && (sbwc_type & COMP)) {
		p_size = csi_hw_g_img_stride(width, bpp, bitwidth, sbwc_type) * height;
		dvaddr += p_size;
	}

	return dvaddr;
}

static inline void csi_s_buf_addr(struct is_device_csi *csi, struct is_frame *frame, u32 idx_wdma,
				  u32 wdma_vc)
{
	struct is_camif_wdma *wdma;
	int i = 0;
	u32 num_buffers, frameptr = 0;
	dma_addr_t dvaddr;
	unsigned long flag;

	FIMC_BUG_VOID(!frame);

	if (wdma_vc != DMA_VIRTUAL_CH_0 && csi->stat_wdma)
		wdma = csi->stat_wdma;
	else
		wdma = csi->wdma[idx_wdma];

	if (!wdma) {
		merr("[CSI%d] wdma is NULL", csi, csi->otf_info.csi_ch);
		return;
	}

	if (csi->f_id_dec)
		num_buffers = csi->dma_batch_num;
	else
		num_buffers = frame->num_buffers;

	if (num_buffers > 1)
		frameptr = num_buffers * (atomic_read(&csi->bufring_cnt) % BUF_SWAP_CNT);

	spin_lock_irqsave(&csi->dma_seq_slock, flag);
	do {
		dvaddr = csi_get_dva(frame, i, csi->sensor_cfg->link_vc[idx_wdma][wdma_vc]);
		if (!dvaddr) {
			mcinfo("[WDMA%d][VC%d] dvaddr is null\n", csi, wdma->ch, wdma_vc);
			continue;
		}

		csi_hw_s_dma_addr(wdma->regs_ctl, wdma->regs_vc[wdma_vc], wdma_vc, i + frameptr,
				  dvaddr);

		mdbg_common(is_get_debug_param(IS_DEBUG_PARAM_CSI), "[%d][CSI%d][WDMA%d][VC%d]",
			    " [I%d] dva %pad\n", csi->instance, csi->otf_info.csi_ch, wdma->ch,
			    wdma_vc, i + frameptr, &dvaddr);

		/* VC0 only has SBWC configuration */
		if (wdma_vc == DMA_VIRTUAL_CH_0) {
			dvaddr = csi_get_dma_header_addr(csi, dvaddr, idx_wdma, wdma_vc);
			csi_hw_s_dma_header_addr(wdma->regs_ctl, wdma->regs_vc[wdma_vc],
						 i + frameptr, dvaddr);
		}

	} while (++i < num_buffers);

	spin_unlock_irqrestore(&csi->dma_seq_slock, flag);
}

static inline int csi_g_otf_ch(struct pablo_camif_otf_info *otf_info, struct is_group *group,
			       unsigned int dma_num)
{
	struct is_group *child;
	u32 otf_ch = 0;

	/* Single bayer out or Long bayer out on AEB mode */
	child = group->child;
	if (child && child->slot == GROUP_SLOT_PAF) {
		otf_ch = child->id > GROUP_ID_PAF0 ?
				child->id - GROUP_ID_PAF0 : 0;
		if (otf_ch >= MAX_NUM_CSIS_OTF_CH)
			goto err;
	}

	otf_info->otf_out_ch[CAMIF_OTF_OUT_SINGLE] = otf_ch;
	otf_info->otf_out_num = 1;

	/* Short / Mid bayer out on AEB mode */
	child = group->pnext;
	while (child && child->slot == GROUP_SLOT_PAF) {
		otf_ch = child->id > GROUP_ID_PAF0 ?
				child->id - GROUP_ID_PAF0 : 0;
		if (otf_ch >= MAX_NUM_CSIS_OTF_CH)
			goto err;

		otf_info->otf_out_ch[CAMIF_OTF_OUT_SHORT] = otf_ch;
		otf_info->otf_out_num++;

		child = child->pnext;
	}

	return 0;
err:
	otf_info->otf_out_num = 0;
	return -EINVAL;
}

static inline void csi_g_otf_lc(struct pablo_camif_otf_info *otf_info,
				struct is_sensor_cfg *sensor_cfg, unsigned long state)
{
	u32 otf_ch;

	memset(otf_info->link_vc, 0, sizeof(otf_info->link_vc));
	memset(otf_info->otf_lc, 0, sizeof(otf_info->otf_lc));

	for (otf_ch = CAMIF_OTF_OUT_SINGLE; otf_ch < otf_info->otf_out_num; otf_ch++) {
		otf_info->otf_lc[otf_ch][CAMIF_VC_IMG] = 0xf;
		otf_info->otf_lc[otf_ch][CAMIF_VC_HPD] = 0xf;
		otf_info->otf_lc[otf_ch][CAMIF_VC_VPD] = 0xf;

		otf_info->link_vc[otf_ch][CAMIF_VC_IMG] = sensor_cfg->img_vc[otf_ch];

		/* Disable pdp mux in sensor only test*/
		if (!test_bit(IS_SENSOR_ONLY, &state))
			otf_info->otf_lc[otf_ch][CAMIF_VC_IMG] =
				otf_info->link_vc[otf_ch][CAMIF_VC_IMG] % CSIS_OTF_CH_LC_NUM;

		if (sensor_cfg->hpd_vc[otf_ch]) {
			otf_info->link_vc[otf_ch][CAMIF_VC_HPD] = sensor_cfg->hpd_vc[otf_ch];
			otf_info->otf_lc[otf_ch][CAMIF_VC_HPD] =
				otf_info->link_vc[otf_ch][CAMIF_VC_HPD] % CSIS_OTF_CH_LC_NUM;
		}

		if (sensor_cfg->vpd_vc[otf_ch]) {
			otf_info->link_vc[otf_ch][CAMIF_VC_VPD] = sensor_cfg->vpd_vc[otf_ch];
			otf_info->otf_lc[otf_ch][CAMIF_VC_VPD] =
				otf_info->link_vc[otf_ch][CAMIF_VC_VPD] % CSIS_OTF_CH_LC_NUM;
		}
	}
}

static void csi_debug_otf(struct is_group *group, struct is_device_csi *csi)
{
	struct is_core *core;
	struct is_device_sensor *sensor;

	sensor = v4l2_get_subdev_hostdata(*csi->subdev);
	if (!sensor) {
		err("failed to get sensor");
		return;
	}
	core = sensor->private_data;

	is_hardware_debug_otf(&core->hardware, group);
}

static inline void csi_s_config_dma(struct is_device_csi *csi)
{
	int wdma_ch, idx_wdma, wdma_vc, link_vc, otf_out_id;
	struct is_subdev *dma_subdev;
	struct pablo_internal_subdev *i_subdev;
	struct is_queue *queue;
	struct is_frame_cfg framecfg;
	struct is_fmt fmt;
	struct is_camif_wdma *wdma;
	struct is_camif_wdma_module *wdma_mod;
	struct pablo_camif_otf_info *otf_info = &csi->otf_info;
	struct sensor_dma_output *dma_output;
	struct is_device_sensor *sensor;
	u32 sbwc_type, otf_ch;
	bool bns_en = false;
	u32 bns_mux_val = 0;
	struct is_vci_config *vci_config;
	/*
	 * CSIS WDMA input mux
	 * Recommended sequence: DMA path select --> DMA config
	 */

	FIMC_BUG_VOID(!csi->sensor_cfg);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		if (!wdma)
			continue;

		otf_out_id = csi->sensor_cfg->otf_out_id[idx_wdma];
		otf_ch = otf_info->otf_out_ch[otf_out_id];
		if (wdma->regs_mux) {
			if (csi->bns) {
				bns_en = csi->bns->en;
				bns_mux_val = csi->bns->dma_mux_val;
			}

			csi->wdma_info.set_info = false;
			csi_hw_s_dma_input_mux(wdma->regs_mux, idx_wdma, wdma->ch, bns_en,
					       bns_mux_val, otf_info->csi_ch, otf_ch, otf_out_id,
					       otf_info->link_vc_list[otf_out_id], &csi->wdma_info);
		}

		if (otf_out_id == CSI_OTF_OUT_SINGLE && csi->stat_wdma) {
			csi_hw_s_dma_input_mux(csi->stat_wdma->regs_mux, idx_wdma,
					       csi->stat_wdma->ch, false, 0, otf_info->csi_ch,
					       otf_ch, otf_out_id, 0, 0);
		}

		sensor = v4l2_get_subdev_hostdata(*csi->subdev);

		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			memset(&framecfg, 0x0, sizeof(struct is_frame_cfg));

			vci_config = &csi->sensor_cfg->output[idx_wdma][wdma_vc];

			if (wdma_vc != DMA_VIRTUAL_CH_0 && csi->stat_wdma) {
				wdma = csi->stat_wdma;
				wdma_ch = wdma->ch;
				wdma_mod = csi->stat_wdma_mod;
			} else {
				wdma_ch = wdma->ch;
				wdma_mod = csi->wdma_mod[idx_wdma];
			}

			if (!wdma_mod) {
				mcerr("Failed to get wdma_module", csi);
				return;
			}

			link_vc = csi->sensor_cfg->link_vc[idx_wdma][wdma_vc];
			i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
			if (INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc)) {
				/* set from internal subdev setting */
				if ((vci_config->type == VC_TAILPDAF) ||
				    (vci_config->type == VC_VPDAF)) {
					if (CHECK_PACKED_EN(vci_config->extformat))
						fmt.pixelformat = V4L2_PIX_FMT_SBGGR10P;
					else
						fmt.pixelformat = V4L2_PIX_FMT_SBGGR16;
				} else {
					fmt.pixelformat = V4L2_PIX_FMT_PRIV_MAGIC;
				}
				fmt.hw_bitwidth = i_subdev->memory_bitwidth;
				fmt.bitsperpixel[0] = i_subdev->bits_per_pixel;
				framecfg.format = &fmt;
				framecfg.width = i_subdev->width;
				framecfg.height = i_subdev->height;
				sbwc_type = i_subdev->sbwc_type;
			} else {
				if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN)) {
					dma_output = &sensor->dma_output[link_vc];
					sbwc_type = dma_output->sbwc_type;

					framecfg.format = &dma_output->fmt;
					framecfg.width = dma_output->width;
					framecfg.height = dma_output->height;
				} else {
					dma_subdev = csi->dma_subdev[idx_wdma][wdma_vc];
					if (!dma_subdev ||
						!test_bit(IS_SUBDEV_START, &dma_subdev->state))
						continue;

					sbwc_type = dma_subdev->sbwc_type;

					/* cpy format from vc video context */
					queue = GET_SUBDEV_QUEUE(dma_subdev);
					if (queue) {
						framecfg = queue->framecfg;
					} else {
						err("vc[%d] subdev queue is NULL!!", wdma_vc);
						return;
					}
				}
			}

			if (vci_config->width) {
				framecfg.width = vci_config->width;
				framecfg.height = vci_config->height;
			}

			framecfg.format->sbwc_type = sbwc_type;

			csi_hw_s_config_dma(wdma->regs_ctl, wdma->regs_vc[wdma_vc], wdma_vc,
					    &framecfg, vci_config->extformat, &vci_config->stride);

			if (!idx_wdma) {
				if (sbwc_type)
					csi_hw_s_dma_common_sbwc_ch(wdma_mod->regs, wdma_ch);
			}

			mcinfo("[VC%d] DMA CH%d VC%d OUT size %dx%d format 0x%x SBWC %s\n", csi,
			       link_vc, wdma_ch, wdma_vc, framecfg.width, framecfg.height,
			       vci_config->extformat, csi_hw_g_dma_sbwc_name(wdma->regs_ctl));

			/* vc: determine for vc0 img format for otf path */
			csi_hw_s_config_dma_cmn(wdma->regs_ctl, wdma_vc, vci_config->extformat,
						vci_config->hwformat, csi->potf);
		}
	}
}

static struct is_framemgr *csis_get_vc_framemgr(struct is_device_csi *csi, u32 idx_wdma,
						u32 wdma_vc)
{
	struct is_subdev *dma_subdev;
	struct pablo_internal_subdev *i_subdev;

	if (wdma_vc >= DMA_VIRTUAL_CH_MAX) {
		err("VC(%d of %d) is out-of-range", wdma_vc, DMA_VIRTUAL_CH_MAX);
		return NULL;
	}

	i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
	if (INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc))
		return GET_SUBDEV_I_FRAMEMGR(i_subdev);

	if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN))
		return NULL;

	dma_subdev = csi->dma_subdev[idx_wdma][wdma_vc];
	if (test_bit(IS_SUBDEV_START, &dma_subdev->state))
		return GET_SUBDEV_FRAMEMGR(dma_subdev);

	return NULL;
}

static void csi_dma_tag(struct v4l2_subdev *subdev, struct is_device_csi *csi,
			struct is_framemgr *framemgr, u32 idx_wdma, u32 wdma_vc)
{
	u32 findex;
	u32 done_state = 0, actuator_position;
	unsigned long flags;
	unsigned int data_type;
	struct is_subdev *f_subdev;
	struct is_framemgr *ldr_framemgr;
	struct is_video_ctx *vctx = NULL;
	struct is_frame *ldr_frame;
	struct is_frame *frame = NULL;
	struct pablo_internal_subdev *i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
	struct is_camif_wdma *wdma;
	struct is_device_sensor *device = v4l2_get_subdev_hostdata(subdev);
	struct is_sensor_interface *itf = is_sensor_get_sensor_interface(device);

	if (wdma_vc != DMA_VIRTUAL_CH_0 && csi->stat_wdma)
		wdma = csi->stat_wdma;
	else
		wdma = csi->wdma[idx_wdma];

	if (!INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc)) {
		if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN))
			return;

		framemgr_e_barrier_irqs(framemgr, 0, flags);

		frame = peek_frame(framemgr, FS_PROCESS);
		if (frame) {
			trans_frame(framemgr, frame, FS_COMPLETE);

			/* get subdev and video context */
			f_subdev = frame->subdev;
			WARN_ON(!f_subdev);

			vctx = f_subdev->vctx;
			WARN_ON(!vctx);

			/* get the leader's framemgr */
			ldr_framemgr = GET_SUBDEV_FRAMEMGR(f_subdev->leader);
			WARN_ON(!ldr_framemgr);

			findex = frame->stream->findex;
			ldr_frame = &ldr_framemgr->frames[findex];
			clear_bit(f_subdev->id, &ldr_frame->out_flag);
			done_state = (frame->result) ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE;

			if (frame->result)
				msrinfo("[CSI%d][ERR] NDONE(%d, E%X)\n", f_subdev, f_subdev,
					ldr_frame, csi->otf_info.csi_ch, frame->index,
					frame->result);
			else
				msrdbgs(1, "[CSI%d] DONE(%d)\n", f_subdev, f_subdev, ldr_frame,
					csi->otf_info.csi_ch, frame->index);

			CALL_VOPS(vctx, done, frame->index, done_state);
		}

		framemgr_x_barrier_irqr(framemgr, 0, flags);

		data_type = CSIS_NOTIFY_DMA_END;
	} else {
		/* get internal VC buffer for embedded data */
		if ((csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_EMBEDDED) ||
		    (csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_EMBEDDED2)) {
			u32 frameptr = csi_hw_g_frameptr(wdma->regs_vc[wdma_vc], wdma_vc);

			if (frameptr < framemgr->num_frames) {
				frame = &framemgr->frames[frameptr];

				/* cache invalidate */
				CALL_BUFOP(i_subdev->pb[frame->index], sync_for_cpu,
					i_subdev->pb[frame->index], 0,
					i_subdev->pb[frame->index]->size, DMA_FROM_DEVICE);
				mdbgd_front("%s, %s[%d] = %d\n", csi, __func__, "VC_EMBEDDED",
					    frameptr, frame->fcount);
			}

			data_type = CSIS_NOTIFY_DMA_END_VC_EMBEDDED;
		} else if (csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_MIPISTAT) {
			u32 frameptr = csi_hw_g_frameptr(wdma->regs_vc[wdma_vc], wdma_vc);

			frameptr = frameptr % framemgr->num_frames;
			frame = &framemgr->frames[frameptr];

			data_type = CSIS_NOTIFY_DMA_END_VC_MIPISTAT;

			mdbgd_front("%s, %s[%d] = %d\n", csi, __func__, "VC_MIPISTAT", frameptr,
				    frame->fcount);
		} else if ((csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_TAILPDAF) ||
			   (csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_VPDAF)) {
			u32 frameptr = csi_hw_g_frameptr(wdma->regs_vc[wdma_vc], wdma_vc);

			frameptr = frameptr % framemgr->num_frames;
			frame = &framemgr->frames[frameptr];

			mdbgd_front("%s, %s[%d] = %d\n", csi, __func__,
				    (csi->sensor_cfg->output[idx_wdma][wdma_vc].type ==
				     VC_TAILPDAF) ?
					    "VC_TAILPDAF" :
					    "VC_VPDAF",
				    frameptr, frame->fcount);

			/* This dump is for tuning. */
			itf->actuator_itf_ops.get_cur_frame_position(itf, &actuator_position);
			pablo_blob_pd_dump(is_debug_get()->blob_pd, frame,
					   "CSI%d_TYPE%d_F%d_%d_%d_%d", csi->device_id,
					   csi->sensor_cfg->output[idx_wdma][wdma_vc].type,
					   frame->fcount,
					   csi->sensor_cfg->output[idx_wdma][wdma_vc].width,
					   csi->sensor_cfg->output[idx_wdma][wdma_vc].height,
					   actuator_position);

			return;
		} else {
			return;
		}
	}

	v4l2_subdev_notify(subdev, data_type, frame);
}

static void csi_err_check(struct is_device_csi *csi, ulong *err_id)
{
	int link_vc, err, idx_wdma, link_vc_out_idx;
	struct is_device_sensor *sensor;
	struct is_camif_wdma *wdma;
	unsigned long prev_err_flag = 0;
	u32 fcount = atomic_read(&csi->fcount);

	/* 1. Check error */
	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++)
		prev_err_flag |= csi->error_id[link_vc];

	/* 2. If err occurs first in 1 frame, request DMA abort */
	if (!prev_err_flag) {
		for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
			wdma = csi->wdma[idx_wdma];

			if (wdma)
				csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
			else
				mwarn("[CSI%d] wdma is NULL", csi, csi->otf_info.csi_ch);
		}

		if (csi->stat_wdma)
			csi_hw_s_control(csi->stat_wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
	}

	/* 3. Cumulative error */
	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++)
		csi->error_id[link_vc] |= err_id[link_vc];

	/* 4. VC ch0 only exception case */
	err = find_first_bit(&err_id[CSI_VIRTUAL_CH_0], CSIS_ERR_END);
	while (err < CSIS_ERR_END) {
		switch (err) {
		case CSIS_ERR_LOST_FE_VC:
			/* 1. disable next dma */
			for (link_vc_out_idx = 0;
			     link_vc_out_idx < csi->sensor_cfg->link_vc_out_idx[CSI_VIRTUAL_CH_0];
			     link_vc_out_idx++) {
				idx_wdma =
					csi->sensor_cfg->dma_idx[CSI_VIRTUAL_CH_0][link_vc_out_idx];
				csi_s_output_dma(csi, idx_wdma, DMA_VIRTUAL_CH_0, false);
			}
			/* 2. schedule the end tasklet */
			csi_frame_end_inline(csi, BIT_MASK(CSI_VIRTUAL_CH_0));
			/* 3. increase the sensor fcount */
			/* 4. schedule the start tasklet */
			csi_frame_start_inline(csi, BIT_MASK(CSI_VIRTUAL_CH_0));
#if defined(CONFIG_CAMERA_VENDOR_MCD) // W/A to prevent CSTAT H/W Error
			csi->crc_flag = true;
#endif
			break;
		case CSIS_ERR_LOST_FS_VC:
			/* disable next dma */
			for (link_vc_out_idx = 0;
			     link_vc_out_idx < csi->sensor_cfg->link_vc_out_idx[CSI_VIRTUAL_CH_0];
			     link_vc_out_idx++) {
				idx_wdma =
					csi->sensor_cfg->dma_idx[CSI_VIRTUAL_CH_0][link_vc_out_idx];
				csi_s_output_dma(csi, idx_wdma, DMA_VIRTUAL_CH_0, false);
			}
#if defined(CONFIG_CAMERA_VENDOR_MCD) // W/A to prevent CSTAT H/W Error
			csi->crc_flag = true;
#endif
			break;
		case CSIS_ERR_CRC:
		case CSIS_ERR_MAL_CRC:
		case CSIS_ERR_CRC_CPHY:
			if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE))
				break;

			csi->crc_flag = true;

			mcerr("[F%d] CSIS_ERR_CRC_XXX (ID %d)", csi, fcount, err);

			if (!test_bit(CSIS_ERR_CRC, &prev_err_flag)
				&& !test_bit(CSIS_ERR_MAL_CRC, &prev_err_flag)
				&& !test_bit(CSIS_ERR_CRC_CPHY, &prev_err_flag)) {
				sensor = v4l2_get_subdev_hostdata(*csi->subdev);
				if (sensor)
					is_sensor_dump(sensor);
			}

			csi_err_iommu_perf(IS_EVENT_CSI_LINK_ERR, csi->instance, fcount);
			break;
		case CSIS_ERR_OVERFLOW_VC:
		case CSIS_ERR_DESKEW_OVER:
			mcerr("[F%d] CSIS_ERR_OVER_XXX (ID %d)", csi, fcount, err);
			csi_err_iommu_perf(IS_EVENT_OVERFLOW_CSI, csi->instance, fcount);
			break;
		default:
			break;
		}

		/* Check next bit */
		err = find_next_bit(&err_id[CSI_VIRTUAL_CH_0], CSIS_ERR_END, err + 1);
	}
}

static void csi_dma_err_check(struct is_device_csi *csi, ulong *err_id, u32 idx_wdma)
{
	int wdma_vc;
	struct is_camif_wdma *wdma;
	unsigned long prev_err_flag = 0;

	/* 1. Check error */
	for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
		prev_err_flag |= csi->dma_error_id[idx_wdma][wdma_vc];

	/* 2. If err occurs first in 1 frame, request DMA abort */
	if (!prev_err_flag) {
		wdma = csi->wdma[idx_wdma];

		if (wdma)
			csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
		else
			mwarn("[CSI%d] wdma is NULL", csi, csi->otf_info.csi_ch);

		if (csi->stat_wdma)
			csi_hw_s_control(csi->stat_wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
	}

	/* 3. Cumulative error */
	for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
		csi->dma_error_id[idx_wdma][wdma_vc] |= err_id[wdma_vc];
}

static void csi_err_print(struct is_device_csi *csi)
{
	const char *err_str = NULL;
	int link_vc, idx_wdma, wdma_vc, err;
	u32 fcount = atomic_read(&csi->fcount);
	bool err_report = false;
	struct is_device_sensor *device = NULL;

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++) {
		/* Skip error handling if there's no error in this virtual ch. */
		if (!csi->error_id[link_vc])
			continue;

		err = find_first_bit(&csi->error_id[link_vc], CSIS_ERR_END);
		while (err < CSIS_ERR_END) {
			switch (err) {
			case CSIS_ERR_ID:
				err_str = GET_STR(CSIS_ERR_ID);
				break;
			case CSIS_ERR_CRC:
				err_str = GET_STR(CSIS_ERR_CRC);
				break;
			case CSIS_ERR_ECC:
				err_str = GET_STR(CSIS_ERR_ECC);
				break;
			case CSIS_ERR_WRONG_CFG:
				err_str = GET_STR(CSIS_ERR_WRONG_CFG);
				break;
			case CSIS_ERR_OVERFLOW_VC:
				err_str = GET_STR(CSIS_ERR_OVERFLOW_VC);
				break;
			case CSIS_ERR_LOST_FE_VC:
				err_str = GET_STR(CSIS_ERR_LOST_FE_VC);
				break;
			case CSIS_ERR_LOST_FS_VC:
				err_str = GET_STR(CSIS_ERR_LOST_FS_VC);
				break;
			case CSIS_ERR_SOT_VC:
				err_str = GET_STR(CSIS_ERR_SOT_VC);
				break;
			case CSIS_ERR_INVALID_CODE_HS:
				err_str = GET_STR(CSIS_ERR_INVALID_CODE_HS);
				break;
			case CSIS_ERR_SOT_SYNC_HS:
				err_str = GET_STR(CSIS_ERR_SOT_SYNC_HS);
				break;
			case CSIS_ERR_DESKEW_OVER:
				err_str = GET_STR(CSIS_ERR_DESKEW_OVER);
				break;
			case CSIS_ERR_SKEW:
				err_str = GET_STR(CSIS_ERR_SKEW);
				break;
			case CSIS_ERR_MAL_CRC:
				err_str = GET_STR(CSIS_ERR_MAL_CRC);
				break;
			case CSIS_ERR_VRESOL_MISMATCH:
				err_str = GET_STR(CSIS_ERR_VRESOL_MISMATCH);
				break;
			case CSIS_ERR_HRESOL_MISMATCH:
				err_str = GET_STR(CSIS_ERR_HRESOL_MISMATCH);
				break;
			case CSIS_ERR_CRC_CPHY:
				err_str = GET_STR(CSIS_ERR_CRC_CPHY);
				break;
			}

			/* Print error log */
			mcerr("[LINK][VC%d][F%d] Occurred the %s(ID %d)", csi, link_vc, fcount,
				err_str, err);

#ifdef CONFIG_CAMERA_VENDOR_MCD
			if (err >= CSIS_ERR_ID && err < CSIS_ERR_END)
				err_report = true;
#endif

			/* Check next bit */
			err = find_next_bit((unsigned long *)&csi->error_id[link_vc], CSIS_ERR_END,
				err + 1);
		}
	}

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			/* Skip error handling if there's no error in this virtual ch. */
			if (!csi->dma_error_id[idx_wdma][wdma_vc])
				continue;

			err = find_first_bit(&csi->dma_error_id[idx_wdma][wdma_vc],
				CSIS_ERR_DMA_END);
			while (err < CSIS_ERR_DMA_END) {
				switch (err) {
				case CSIS_ERR_DMA_OTF_OVERLAP_VC:
					err_str = GET_STR(CSIS_ERR_DMA_OTF_OVERLAP_VC);
					break;
				case CSIS_ERR_DMA_DMAFIFO_FULL:
					err_str = GET_STR(CSIS_ERR_DMA_DMAFIFO_FULL);
					if (IS_ENABLED(CONFIG_PANIC_ON_CSIS_OVERFLOW)) {
#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
						smc_ppc_enable(0);
#endif
					}
					csi_err_iommu_perf(
						IS_EVENT_OVERFLOW_DMA, csi->instance, fcount);
					break;
				case CSIS_ERR_DMA_ABORT_DONE:
					err_str = GET_STR(CSIS_ERR_DMA_ABORT_DONE);
					break;
				case CSIS_ERR_DMA_FRAME_DROP_VC:
					err_str = GET_STR(CSIS_ERR_DMA_FRAME_DROP_VC);
					break;
				case CSIS_ERR_DMA_LASTDATA_ERROR:
					err_str = GET_STR(CSIS_ERR_DMA_LASTDATA_ERROR);
					break;
				case CSIS_ERR_DMA_LASTADDR_ERROR:
					err_str = GET_STR(CSIS_ERR_DMA_LASTADDR_ERROR);
					break;
				case CSIS_ERR_DMA_FSTART_IN_FLUSH_VC:
					err_str = GET_STR(CSIS_ERR_DMA_FSTART_IN_FLUSH_VC);
					break;
				case CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC:
					err_str = GET_STR(CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC);
					break;
				}

				/* Print error log */
				mcerr("[WDMA%d][VC%d][F%d] Occurred the %s(ID %d)", csi,
					csi->wdma[idx_wdma]->ch, wdma_vc, fcount, err_str, err);

#ifdef CONFIG_CAMERA_VENDOR_MCD
				/* temporarily hwparam count except CSIS_ERR_DMA_ABORT_DONE */
				if ((err != CSIS_ERR_DMA_ABORT_DONE)
					&& err >= CSIS_ERR_DMA_ID && err < CSIS_ERR_DMA_END)
					err_report = true;
#endif

				/* Check next bit */
				err = find_next_bit(
					(unsigned long *)&csi->dma_error_id[idx_wdma][wdma_vc],
					CSIS_ERR_DMA_END, err + 1);
			}
		}
	}

	if (err_report)
		is_vendor_csi_err_handler(device->position);
}

void csi_hw_cdump_all(struct is_device_csi *csi)
{
	int wdma_vc, idx_wdma;
	struct is_camif_wdma *wdma;
	struct is_camif_wdma_module *wdma_mod;

	csi_hw_cdump(csi->base_reg);
	csi_hw_phy_cdump(csi->phy_reg, csi->otf_info.csi_ch);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		wdma_mod = csi->wdma_mod[idx_wdma];

		if (wdma) {
			info("CSIS_DMA%d REG DUMP\n", wdma->ch);
			csi_hw_vcdma_cmn_cdump(wdma->regs_ctl);

			for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
				info("CSIS_DMA%d_VC%d REG DUMP\n", wdma->ch, wdma_vc);
				csi_hw_vcdma_cdump(wdma->regs_vc[wdma_vc]);
			}
		}

		if (wdma_mod)
			csi_hw_common_dma_cdump(wdma_mod->regs);
	}

	if (csi->stat_wdma) {
		info("CSIS_DMA%d REG DUMP\n", csi->stat_wdma->ch);
		csi_hw_vcdma_cmn_cdump(csi->stat_wdma->regs_ctl);

		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			info("CSIS_DMA%d_VC%d REG DUMP\n", csi->stat_wdma->ch, wdma_vc);
			csi_hw_vcdma_cdump(csi->stat_wdma->regs_vc[wdma_vc]);
		}
	}

	if (csi->mcb)
		csi_hw_mcb_cdump(csi->mcb->regs);

	if (csi->ebuf)
		csi_hw_ebuf_cdump(csi->ebuf->regs);
}

void csi_hw_dump_all(struct is_device_csi *csi)
{
	int wdma_vc, idx_wdma;
	struct is_camif_wdma *wdma;
	struct is_camif_wdma_module *wdma_mod;

	csi_hw_dump(csi->base_reg);
	csi_hw_phy_dump(csi->phy_reg, csi->otf_info.csi_ch);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		wdma_mod = csi->wdma_mod[idx_wdma];

		if(wdma) {
			info("CSIS_DMA%d REG DUMP\n", wdma->ch);
			csi_hw_vcdma_cmn_dump(wdma->regs_ctl);

			for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
				info("CSIS_DMA%d_VC%d REG DUMP\n", wdma->ch, wdma_vc);
				csi_hw_vcdma_dump(wdma->regs_vc[wdma_vc]);
			}
		}

		if (wdma_mod)
			csi_hw_common_dma_dump(wdma_mod->regs);
	}

	if (csi->fro_reg)
		csi_hw_fro_dump(csi->fro_reg);

	if (csi->stat_wdma) {
		info("CSIS_DMA%d REG DUMP\n", csi->stat_wdma->ch);
		csi_hw_vcdma_cmn_dump(csi->stat_wdma->regs_ctl);

		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			info("CSIS_DMA%d_VC%d REG DUMP\n", csi->stat_wdma->ch, wdma_vc);
			csi_hw_vcdma_dump(csi->stat_wdma->regs_vc[wdma_vc]);
		}
	}

	if (csi->mcb)
		csi_hw_mcb_dump(csi->mcb->regs);

	if (csi->ebuf)
		csi_hw_ebuf_dump(csi->ebuf->regs);

	if (csi->top)
		pablo_camif_csis_pdp_top_dump(csi->top->regs);
}

static void csi_err_handle_ext(struct is_device_csi *csi, u32 error_id_all)
{
	struct is_device_sensor *device =
		container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	struct is_group *group = &device->group_sensor;

	if (csi_hw_get_version(csi->base_reg) == IS_CSIS_VERSION(5, 4, 0, 0)) {
		/* get s2d dump in only CSIS_ERR_DMA_OTF_OVERLAP_VC case for ESD recovery */
		if (error_id_all == (1 << CSIS_ERR_DMA_OTF_OVERLAP_VC)) {
			csi->error_count_vc_overlap++;
			mcinfo(" error_count_vc_overlap = %d",
				csi, csi->error_count_vc_overlap);

			/* get connected ip status for detect reason of dma overlap */
			csi_debug_otf(group, csi);
		}

		if (csi->error_count_vc_overlap >= CSI_ERR_COUNT - 1)
			is_debug_s2d(true, "CSIS error!! - CSIS_ERR_DMA_OTF_OVERLAP_VC");
	}
}

static void csi_err_handle(struct is_device_csi *csi)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_camif_wdma *wdma;
	u32 link_vc, idx_wdma, wdma_vc, link_vc_out_idx;
	u32 error_id_all = 0;

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	core = device->private_data;

	/* 1. Print error & Clear error status */
	csi_err_print(csi);

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++) {
		/* 2. Clear err */
		error_id_all |= csi->error_id_last[link_vc] = csi->error_id[link_vc];
		csi->error_id[link_vc] = 0;

		for (link_vc_out_idx = 0;
		     link_vc_out_idx < csi->sensor_cfg->link_vc_out_idx[link_vc];
		     link_vc_out_idx++) {
			idx_wdma = csi->sensor_cfg->dma_idx[link_vc][link_vc_out_idx];
			wdma_vc = csi->sensor_cfg->dma_vc[link_vc][link_vc_out_idx];

			/* 3. Frame flush */
			csis_flush_vc_buf_done(
				csi, idx_wdma, wdma_vc, FS_PROCESS, VB2_BUF_STATE_ERROR);
			csis_flush_vc_multibuf(csi, idx_wdma, wdma_vc);
			err("[F%d][VC%d] frame was done with error", atomic_read(&csi->fcount),
				link_vc);
			set_bit((CSIS_BUF_ERR_VC0 + link_vc), &csi->state);
		}
	}

	csi_err_handle_ext(csi, error_id_all);

	/* 4. Clear DMA err */
	memset(csi->dma_error_id, 0, sizeof(csi->dma_error_id));

	/* 5. Add error count */
	csi->error_count++;

	if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE))
		return;

	/* 6. Call sensor SNR if Err count is more than a certain amount */
	if (csi->error_count >= CSI_ERR_COUNT) {

		/* Call sensor SNR */
		set_bit(IS_SENSOR_FRONT_SNR_STOP, &device->state);

		/* Disable CSIS */
		csi_hw_disable(csi->base_reg);

		for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
			wdma = csi->wdma[idx_wdma];

			if (wdma)
				csi_hw_s_dma_irq_msk(wdma->regs_ctl, false);
		}

		if (csi->stat_wdma)
			csi_hw_s_dma_irq_msk(csi->stat_wdma->regs_ctl, false);

		csi_hw_s_irq_msk(csi->base_reg, false, csi->f_id_dec);

		/* CSIS register dump */
		if ((csi->error_count == CSI_ERR_COUNT) || (csi->error_count % 20 == 0)) {
			csi_hw_dump_all(csi);
			/* CLK dump */
			schedule_work(&core->wq_data_print_clk);
		}
	}
}

static void link_dump(struct work_struct *data)
{
	struct is_device_csi *csi;

	csi = container_of(data, struct is_device_csi, wq_link_dump);

	csi_hw_dump(csi->base_reg);
	csi_hw_phy_dump(csi->phy_reg, csi->otf_info.csi_ch);
}

static void wq_ebuf_reset(struct work_struct *data)
{
	struct is_device_csi *csi;
	struct is_device_sensor *sensor;
	struct v4l2_subdev *subdev_module;
	struct pablo_camif_ebuf *ebuf;
	int ret;
	u32 otf_ch;

	FIMC_BUG_VOID(!data);

	csi = container_of(data, struct is_device_csi, wq_ebuf_reset);
	sensor = v4l2_get_subdev_hostdata(*csi->subdev);

	if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state) ||
			!csi->otf_info.otf_out_num)
		return;

	mcinfo("%s\n", csi, __func__);

	/* sensor stream off */
	subdev_module = sensor->subdev_module;
	if (subdev_module) {
		ret = v4l2_subdev_call(subdev_module, video, s_stream, false);
		if (ret)
			merr("v4l2_subdev_call(s_stream) is fail(%d)", sensor, ret);
	} else {
		merr("subdev_module is NULL", sensor);
		return;
	}

	/* ebuf reset */
	msleep(1000);
	ebuf = csi->ebuf;
	otf_ch = csi->otf_info.otf_out_ch[CAMIF_OTF_OUT_SINGLE];

	mutex_lock(&ebuf->lock);
	csi_hw_s_ebuf_enable(ebuf->regs, false, otf_ch, EBUF_AUTO_MODE,
			ebuf->num_of_ebuf, ebuf->fake_done_offset);
	csi_hw_s_ebuf_enable(ebuf->regs, true, otf_ch, EBUF_AUTO_MODE,
			ebuf->num_of_ebuf, ebuf->fake_done_offset);
	mutex_unlock(&ebuf->lock);

	/* sensor stream on */
	ret = v4l2_subdev_call(subdev_module, video, s_stream, true);
	if (ret)
		merr("v4l2_subdev_call(s_stream) is fail(%d)", sensor, ret);
}

static void csi_dma_work_fn(struct work_struct *data)
{
	struct is_device_csi *csi;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_framemgr *framemgr;
	u32 idx_wdma, wdma_vc;

	FIMC_BUG_VOID(!data);

	csi = container_of(data, struct is_device_csi, wq_csis_dma);
	work_list = &csi->work_list;
	get_req_work(work_list, &work);
	while (work) {
		wdma_vc = work->msg.param1;
		idx_wdma = work->msg.param2;

		framemgr = csis_get_vc_framemgr(csi, idx_wdma, wdma_vc);
		if (framemgr)
			csi_dma_tag(*csi->subdev, csi, framemgr, idx_wdma, wdma_vc);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

PKV_DECLARE_TASKLET_CALLBACK(csis_otf_cfg, t)
{
	struct is_device_csi *csi = from_tasklet(csi, t, tasklet_csis_otf_cfg);
	struct pablo_camif_otf_info *otf_info;
	u32 act_num, req_num, otf_out_id;

#ifdef TASKLET_MSG
	is_info("E%d\n", atomic_read(&csi->fcount));
#endif
	if (!test_bit(CSIS_START_STREAM, &csi->state))
		return;

	if (atomic_read(&csi->vvalid))
		return;

	otf_info = &csi->otf_info;
	act_num = otf_info->act_otf_out_num;
	req_num = otf_info->req_otf_out_num;

	if (req_num > act_num) {
		for (otf_out_id = act_num; otf_out_id < req_num; otf_out_id++) {
			CALL_CAMIF_TOP_OPS(csi->top, s_otf_out_mux, otf_info, otf_out_id, true);
			CALL_CAMIF_TOP_OPS(csi->top, set_ibuf, otf_info, otf_out_id, csi->sensor_cfg, csi->potf);
		}
	} else {
		for (otf_out_id = act_num - 1; otf_out_id >= req_num; otf_out_id--) {
			CALL_CAMIF_TOP_OPS(csi->top, s_otf_out_mux, otf_info, otf_out_id, false);
		}
	}

	otf_info->act_otf_out_num = req_num;
}

PKV_DECLARE_TASKLET_CALLBACK(csis_end, t)
{
	struct is_device_csi *csi = from_tasklet(csi, t, tasklet_csis_end);
	u32 link_vc, ch, err_flag = 0, idx_wdma, wdma_vc;
	u32 status = IS_SHOT_SUCCESS;

#ifdef TASKLET_MSG
	is_info("E%d\n", atomic_read(&csi->fcount));
#endif

	for (ch = CSI_VIRTUAL_CH_0; ch < CSI_VIRTUAL_CH_MAX; ch++)
		err_flag |= csi->error_id[ch];

	for (idx_wdma = 0; idx_wdma < csi->sensor_cfg->dma_num; idx_wdma++)
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
			err_flag |= csi->dma_error_id[idx_wdma][wdma_vc];

	if (err_flag) {
		csi_err_handle(csi);
	} else {
		/* If error does not occur continously, the system should error count clear */
		csi->error_count = 0;
		csi->error_count_vc_overlap = 0;
	}

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++) {
		if (test_bit((CSIS_BUF_ERR_VC0 + link_vc), &csi->state)) {
			clear_bit((CSIS_BUF_ERR_VC0 + link_vc), &csi->state);
			status = IS_SHOT_CORRUPTED_FRAME;
		}
	}

	v4l2_subdev_notify(*csi->subdev, CSIS_NOTIFY_FEND, &status);
}

PKV_DECLARE_TASKLET_CALLBACK(csis_line, t)
{
	struct is_device_csi *csi = from_tasklet(csi, t, tasklet_csis_line);

#ifdef TASKLET_MSG
	is_info("L%d\n", atomic_read(&csi->fcount));
#endif
	v4l2_subdev_notify(*csi->subdev, CSIS_NOTIFY_LINE, NULL);
}

static inline void csi_wq_func_schedule(struct is_device_csi *csi,
		struct work_struct *work_wq)
{
	if (csi->workqueue)
		queue_work(csi->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

static void do_dma_done_work_func(struct is_device_csi *csi, u32 idx_wdma, u32 wdma_vc)
{
	bool retry_flag = true;
	struct work_struct *work_wq;
	struct is_work_list *work_list;
	struct is_work *work;

	work_wq = &csi->wq_csis_dma;
	work_list = &csi->work_list;

retry:
	get_free_work(work_list, &work);
	if (work) {
		work->msg.id = 0;
		work->msg.instance = csi->instance;
		work->msg.param1 = wdma_vc;
		work->msg.param2 = idx_wdma;

		work->fcount = atomic_read(&csi->fcount);
		set_req_work(work_list, work);

		if (!work_pending(work_wq))
			csi_wq_func_schedule(csi, work_wq);
	} else {
		mcerr("[VC%d]free work list is empty. retry(%d)", csi, wdma_vc, retry_flag);
		if (retry_flag) {
			retry_flag = false;
			goto retry;
		}
	}
}

static void _logging_link_interrupt(struct is_device_csi *csi, struct csis_irq_src *irq_src)
{
	struct dbg_irq_info dbg_info;
	u32 otf_out_ch;
	int ret;

	otf_out_ch = csi->otf_info.otf_out_ch[CAMIF_OTF_OUT_SINGLE];
	if (test_bit(otf_out_ch + IBUF0_ERR_WRONGSIZE, &csi->top->err_status)) {
		clear_bit(otf_out_ch + IBUF0_ERR_WRONGSIZE, &csi->top->err_status);

		while (!kfifo_is_empty(&csi->dbg_irq_info_fifo)) {
			ret = kfifo_out(&csi->dbg_irq_info_fifo, &dbg_info, sizeof(dbg_info));
			if (ret == sizeof(dbg_info)) {
				mcinfo(
				"err_src0 0x%08X, err_src1 0x%08X, lost_fs 0x%08X, lost_fe 0x%08X, vresol 0x%08X, hresol 0x%08X",
				csi, dbg_info.err_src0, dbg_info.err_src1, dbg_info.lost_fs,
				dbg_info.lost_fe, dbg_info.vresol, dbg_info.hresol);
			}
		}

		if (!work_pending(&csi->wq_link_dump))
			csi_wq_func_schedule(csi, &csi->wq_link_dump);
	}

	if ((kfifo_len(&csi->dbg_irq_info_fifo) / sizeof(dbg_info)) == DBG_IRQ_INFO_MAX) {
		ret = kfifo_out(&csi->dbg_irq_info_fifo, &dbg_info, sizeof(dbg_info));
		if (ret != sizeof(dbg_info))
			mcerr("Failed to pop dbg irq", csi);
	}

	memcpy(&dbg_info, &irq_src->dbg, sizeof(dbg_info));

	ret = kfifo_in(&csi->dbg_irq_info_fifo, &dbg_info, sizeof(dbg_info));
	if (!ret)
		mcerr("Failed to push dbg irq", csi);
}

static irqreturn_t is_isr_csi(int irq, void *data)
{
	struct is_device_csi *csi;
	struct pablo_camif_otf_info *otf_info;
	struct csis_irq_src irq_src;
	ulong frame_start = 0L, frame_end = 0L;
	u32 ch, img_vc, err_flag = 0;
	ulong err = 0;

	csi = data;
	otf_info = &csi->otf_info;
	memset(&irq_src, 0x0, sizeof(struct csis_irq_src));
	csi_hw_g_irq_src(csi->base_reg, &irq_src, true);

	if (irq_src.err_flag)
		_logging_link_interrupt(csi, &irq_src);

	if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE) != PABLO_PHY_TUNE_DISABLE) {
		if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE) == PABLO_PHY_TUNE_DPHY) {
			err = (irq_src.err_id[CSI_VIRTUAL_CH_0] & (BIT(CSIS_ERR_ECC) | BIT(CSIS_ERR_CRC)));
		} else {
			for (ch = CSI_VIRTUAL_CH_0; ch < CSI_VIRTUAL_CH_MAX; ch++)
				err |= irq_src.err_id[ch];
		}
	} else {
		err = irq_src.err_id[CSI_VIRTUAL_CH_0];
	}

	csi->state_cnt.err += err ? 1: 0;
	csi->state_cnt.str += (irq_src.otf_start & BIT(CSI_VIRTUAL_CH_0)) ? 1 : 0;
	csi->state_cnt.end += (irq_src.otf_end & BIT(CSI_VIRTUAL_CH_0)) ? 1 : 0;

	dbg_isr(2, "link: ERR(0x%lX, 0x%lX, 0x%lX, 0x%lX)(0x%lX, 0x%lX, 0x%lX, 0x%lX) S(0x%X) L(0x%X) E(0x%X) D(0x%X)\n", csi,
		irq_src.err_id[CSI_VIRTUAL_CH_0],
		irq_src.err_id[CSI_VIRTUAL_CH_1],
		irq_src.err_id[CSI_VIRTUAL_CH_2],
		irq_src.err_id[CSI_VIRTUAL_CH_3],
		irq_src.err_id[CSI_VIRTUAL_CH_4],
		irq_src.err_id[CSI_VIRTUAL_CH_5],
		irq_src.err_id[CSI_VIRTUAL_CH_6],
		irq_src.err_id[CSI_VIRTUAL_CH_7],
		irq_src.otf_start, irq_src.line_end, irq_src.otf_end, irq_src.otf_dbg);

	/* Gather Frame Start/End Status */
	for (ch = CAMIF_OTF_OUT_SINGLE; ch < otf_info->otf_out_num; ch++) {
		img_vc = otf_info->link_vc[ch][CAMIF_VC_IMG];
		frame_start |= (irq_src.otf_start & BIT_MASK(img_vc));
		frame_end |= (irq_src.otf_end & BIT_MASK(img_vc));
	}

	/* LINE Irq */
	if (irq_src.line_end & (1 << CSI_VIRTUAL_CH_0))
		csi_frame_line_inline(csi);


	/* Frame Start and End */
	if (frame_start && frame_end) {
		warn("[CSIS%d] start/end overlapped", otf_info->csi_ch);
		/* frame both interrupt since latency */
		if (csi->sw_checker == EXPECT_FRAME_START) {
			csi_frame_start_inline(csi, frame_start);
			csi_frame_end_inline(csi, frame_end);
		} else {
			csi_frame_end_inline(csi, frame_end);
			csi_frame_start_inline(csi, frame_start);
		}
	/* Frame Start */
	} else if (frame_start) {
		/* W/A: Skip start tasklet at interrupt lost case */
		if (csi->sw_checker != EXPECT_FRAME_START) {
			warn("[CSIS%d] Lost end interrupt\n", otf_info->csi_ch);
			/*
			 * Even though it skips to start tasklet,
			 * framecount of CSI device should be increased
			 * to match with chain device including DDK.
			 */
			atomic_inc(&csi->fcount);
			goto clear_status;
		}
		csi_frame_start_inline(csi, frame_start);
	/* Frame End */
	} else if (frame_end) {
		/* W/A: Skip end tasklet at interrupt lost case */
		if (csi->sw_checker != EXPECT_FRAME_END) {
			warn("[CSIS%d] Lost start interrupt\n", otf_info->csi_ch);
			/*
			 * Even though it skips to start tasklet,
			 * framecount of CSI device should be increased
			 * to match with chain device including DDK.
			 */
			atomic_inc(&csi->fcount);

			/*
			 * If LOST_FS_VC_ERR is happened, there is only end interrupt
			 * so, check if error occur continuously during 10 frame.
			 */
			/* check to error */
			for (ch = CSI_VIRTUAL_CH_0; ch < CSI_VIRTUAL_CH_MAX; ch++)
				err_flag |= (u32)csi->error_id[ch];

			/* error handling */
			if (err_flag)
				csi_err_handle(csi);

			goto clear_status;
		}
		csi_frame_end_inline(csi, frame_end);
		csi_frame_end_emul(csi);
	}

	/* Check error */
	if (irq_src.err_flag)
		csi_err_check(csi, irq_src.err_id);
clear_status:
	return IRQ_HANDLED;
}

static irqreturn_t is_isr_csi_dma(int irq, void *data)
{
	struct is_device_csi *csi;
	int dma_frame_str;
	int dma_frame_end;
	int dma_abort_done;
	struct csis_dma_irq_src irq_src, stat_irq_src;
	int wdma_vc, idx_wdma;
	struct is_framemgr *framemgr;
	struct is_device_sensor *device;
	struct is_group *group;
	struct pablo_internal_subdev *i_subdev;
	struct is_camif_wdma *wdma = 0;
	int wdma_ch;

	csi = data;

	if (!csi->sensor_cfg) {
		mcerr("csi->sensor_cfg is null", csi);
		return IRQ_HANDLED;
	}

	spin_lock(&csi->dma_irq_slock);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		if (csi->wdma[idx_wdma]->irq == irq) {
			wdma = csi->wdma[idx_wdma];
			wdma_ch = wdma->ch;
			break;
		}
	}

	if (!wdma) {
		mcerr(" wdma is NULL", csi);
		spin_unlock(&csi->dma_irq_slock);

		return IRQ_HANDLED;
	}

	memset(&irq_src, 0x0, sizeof(struct csis_dma_irq_src));
	csi_hw_g_dma_irq_src_vc(wdma->regs_ctl, &irq_src, idx_wdma, 0, true);

	dma_frame_str = irq_src.dma_start;
	dma_frame_end = irq_src.dma_end;
	dma_abort_done = irq_src.dma_abort;

	if (csi->stat_wdma) {
		csi_hw_g_dma_irq_src_vc(csi->stat_wdma->regs_ctl, &stat_irq_src, idx_wdma, 0, true);

		dma_frame_str |= stat_irq_src.dma_start;
		dma_frame_end |= stat_irq_src.dma_end;
		dma_abort_done |= stat_irq_src.dma_abort;

		if (stat_irq_src.err_flag) {
			irq_src.err_flag = true;
			for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
				irq_src.err_id[idx_wdma][wdma_vc] |=
					stat_irq_src.err_id[idx_wdma][wdma_vc];
		}
	}
	spin_unlock(&csi->dma_irq_slock);

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	group = &device->group_sensor;

	if (dma_frame_str) {
		u32 level = (atomic_read(&group->scount) == 1) ? 0 : 1;

		dbg_isr(level, "DMA%d DS 0x%X\n", csi, wdma_ch, dma_frame_str);
	}

	if (dma_frame_end)
		dbg_isr(1, "DMA%d DE 0x%X\n", csi, wdma_ch, dma_frame_end);

	if (dma_abort_done) {
		dbg_isr(1, "DMA%d ABORT DONE 0x%X\n", csi, wdma_ch, dma_abort_done);
		clear_bit(CSIS_DMA_FLUSH_WAIT, &csi->state);
		wake_up(&csi->dma_flush_wait_q);
	}

	for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
		csi_hw_s_dma_dbg_cnt(wdma->regs_vc[wdma_vc], &irq_src, wdma_vc);

		if (dma_frame_end & (1 << wdma_vc)) {
			if (csi->f_id_dec &&
			    csi->sensor_cfg->link_vc[idx_wdma][wdma_vc] == CSI_VIRTUAL_CH_0) {
				if (csi_hw_get_version(csi->base_reg) <
					IS_CSIS_VERSION(5, 4, 0, 0)) {
					struct is_camif_wdma_module *wdma_mod =
						csi->wdma_mod[idx_wdma];

					/*
					 * TODO: Both FRO count and frame id decoder dosen't have to be controlled
					 * if shadowing scheme is supported at start interrupt of frame id decoder.
					 */
					if (wdma_mod)
						csi_hw_clear_fro_count(wdma_mod->regs,
								       wdma->regs_vc[wdma_vc]);
					else
						mwarn("[CSI%d] wdma_mod is NULL", csi,
								csi->otf_info.csi_ch);
				}

				csi_frame_end_inline(csi, BIT_MASK(CSI_VIRTUAL_CH_0));
			}

			/*
			 * The embedded data is done at fraem end.
			 * But updating frame_id have to be faster than is_sensor_dm_tag().
			 * So, csi_dma_tag is performed in ISR in only case of embedded data.
			 */
			if (IS_ENABLED(CHAIN_TAG_VC0_DMA_IN_HARDIRQ_CONTEXT) ||
			    (csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_EMBEDDED) ||
			    (csi->sensor_cfg->output[idx_wdma][wdma_vc].type == VC_EMBEDDED2)) {
				framemgr = csis_get_vc_framemgr(csi, idx_wdma, wdma_vc);
				if (framemgr)
					csi_dma_tag(*csi->subdev, csi, framemgr, idx_wdma, wdma_vc);
				else if (!IS_ENABLED(CONFIG_SENSOR_GROUP_LVN))
					mcerr("[VC%d] framemgr is NULL", csi, wdma_vc);
			} else {
				do_dma_done_work_func(csi, idx_wdma, wdma_vc);
			}
		}

		if (dma_frame_str & (1 << wdma_vc)) {
			if (csi->f_id_dec &&
			    csi->sensor_cfg->link_vc[idx_wdma][wdma_vc] == CSI_VIRTUAL_CH_0)
				csi_frame_start_inline(csi, BIT_MASK(CSI_VIRTUAL_CH_0));

			i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
			if (test_bit(PABLO_SUBDEV_ALLOC, &i_subdev->state)) {
				csis_s_vc_dma_frobuf(csi, idx_wdma, wdma_vc);

				continue;
			}

			/* dma disable if interrupt occurred */
			csi_s_output_dma(csi, idx_wdma, wdma_vc, false);
		}
	}

	/* Check error */
	if (irq_src.err_flag) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			if (irq_src.err_id[idx_wdma][wdma_vc] & ~(1 << CSIS_ERR_DMA_ABORT_DONE))
				mcinfo("[WDMA%d][VC%d] CSIS_ERR_DMA ID(0x%08lX)", csi, wdma_ch,
				       wdma_vc, irq_src.err_id[idx_wdma][wdma_vc]);
		}
		csi_dma_err_check(csi, irq_src.err_id[idx_wdma], idx_wdma);
	}

	return IRQ_HANDLED;
}

static irqreturn_t is_isr_csi_ebuf(int irq, void *data)
{
	struct is_device_csi *csi = (struct is_device_csi *)data;
	struct pablo_camif_ebuf *ebuf = csi->ebuf;
	struct csis_irq_src irq_src;
	int ebuf_ch;
	ulong sensor_abort, fake_frame_done;
	unsigned int num_of_ebuf = ebuf->num_of_ebuf;
	unsigned int offset_fake_frame_done = ebuf->fake_done_offset;
	u32 mask;
	struct work_struct *work_wq;

	ebuf_ch = csi->otf_info.otf_out_ch[CAMIF_OTF_OUT_SINGLE];

	csi_hw_g_ebuf_irq_src(ebuf->regs, &irq_src, ebuf_ch, offset_fake_frame_done);
	if (!irq_src.ebuf_status)
		return IRQ_NONE;

	if (num_of_ebuf > sizeof(ulong)) {
		mcerr("num_of_ebuf(%d) is invalid", csi, num_of_ebuf);
		return IRQ_HANDLED;
	}

	mask = GENMASK(num_of_ebuf - 1, 0);
	sensor_abort = irq_src.ebuf_status & mask;
	fake_frame_done = (irq_src.ebuf_status >> offset_fake_frame_done) & mask;

	/* sensor abort */
	ebuf_ch = find_first_bit(&sensor_abort, num_of_ebuf);
	while (ebuf_ch < num_of_ebuf) {
		/* EBUF_MANUAL_MODE only */
		mcinfo("sensor_abort(EBUF #%d, hw=%d, bit=%d)\n", csi, ebuf_ch, num_of_ebuf, offset_fake_frame_done);

		csi_hw_s_ebuf_fake_sign(ebuf->regs, ebuf_ch);

		ebuf_ch = find_next_bit(&sensor_abort, num_of_ebuf, ebuf_ch + 1);
	}

	/* fake frame done */
	ebuf_ch = find_first_bit(&fake_frame_done, num_of_ebuf);
	while (ebuf_ch < num_of_ebuf) {
		mcinfo("fake_frame_done(EBUF #%d)\n", csi, ebuf_ch);

		/* WQ */
		work_wq = &csi->wq_ebuf_reset;
		if (!work_pending(work_wq))
			csi_wq_func_schedule(csi, work_wq);

		ebuf_ch = find_next_bit(&fake_frame_done, num_of_ebuf, ebuf_ch + 1);
	}

	return IRQ_HANDLED;
}

int is_csi_open(struct v4l2_subdev *subdev,
	struct is_framemgr *framemgr)
{
	int ret = 0;
	u32 link_vc;
	struct is_device_csi *csi;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	csi->sensor_cfg = NULL;
	csi->error_count = 0;
	csi->error_count_vc_overlap = 0;

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++)
		csi->error_id[link_vc] = 0;

	memset(csi->dma_error_id, 0, sizeof(csi->dma_error_id));
	memset(&csi->image, 0, sizeof(struct is_image));
	memset(csi->pre_dma_enable, -1, sizeof(csi->pre_dma_enable));
	memset(csi->cur_dma_enable, 0, sizeof(csi->cur_dma_enable));

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	csi->stm = pablo_lib_get_stream_prefix(device->instance);
	csi->instance = device->instance;
	csi->top = pablo_camif_csis_pdp_top_get();

	csi->framemgr = framemgr;
	atomic_set(&csi->fcount, 0);
	atomic_set(&csi->vblank_count, 0);
	atomic_set(&csi->vvalid, 0);

	return 0;

p_err:
	return ret;
}

int is_csi_close(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_device_csi *csi;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	csi_dma_detach(csi);

p_err:
	return ret;
}

/* value : module enum */
static int csi_init(struct v4l2_subdev *subdev, u32 value)
{
	int ret = 0;
	struct is_device_csi *csi;
	struct is_device_sensor *device;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	csi_hw_reset(csi->base_reg);

	mdbgd_front("%s(%d)\n", csi, __func__, ret);

p_err:
	return ret;
}

static int csi_s_power(struct v4l2_subdev *subdev,
	int on)
{
	int ret = 0;
	struct is_device_csi *csi;
	struct pablo_camif_otf_info *otf_info;
	struct is_camif_wdma_module *wdma_mod;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	otf_info = &csi->otf_info;
	wdma_mod = is_camif_wdma_module_get(0); /* before DMA attach */
	if (!wdma_mod) {
		merr("[CSI%d] wdma_mod is NULL", csi, otf_info->csi_ch);
		return -ENODEV;
	}

	if (on) {
		if (atomic_inc_return(&wdma_mod->active_cnt) == 1) {
			/* IP processing = 1 */
			csi_hw_dma_common_reset(wdma_mod->regs, on);
		}
	} else {
		if (atomic_dec_return(&wdma_mod->active_cnt) == 0) {
			/* For safe power-off: IP processing = 0 */
			csi_hw_dma_common_reset(wdma_mod->regs, on);
		}
	}

	mdbgd_front("%s(csi %d, %d, %d)\n", csi, __func__, otf_info->csi_ch, on, ret);

	return ret;
}

static int csi_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	struct is_device_csi *csi;
	int ret = 0;
	int link_vc = 0;
	struct is_camif_wdma *wdma;
	u32 idx_wdma, wdma_vc, link_vc_out_idx;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	switch (ctrl->id) {
	case SENSOR_IOCTL_G_VC1_FRAMEPTR:
	case SENSOR_IOCTL_G_VC2_FRAMEPTR:
	case SENSOR_IOCTL_G_VC3_FRAMEPTR:
	case SENSOR_IOCTL_G_VC4_FRAMEPTR:
	case SENSOR_IOCTL_G_VC5_FRAMEPTR:
	case SENSOR_IOCTL_G_VC6_FRAMEPTR:
	case SENSOR_IOCTL_G_VC7_FRAMEPTR:
		link_vc = CSI_VIRTUAL_CH_1 + (ctrl->id - SENSOR_IOCTL_G_VC1_FRAMEPTR);

		for (link_vc_out_idx = 0;
		     link_vc_out_idx < csi->sensor_cfg->link_vc_out_idx[link_vc];
		     link_vc_out_idx++) {
			idx_wdma = csi->sensor_cfg->dma_idx[link_vc][link_vc_out_idx];
			wdma_vc = csi->sensor_cfg->dma_vc[link_vc][link_vc_out_idx];
			wdma = csi->wdma[idx_wdma];
			if (!wdma) {
				merr("[CSI%d][VC%d] wdma is NULL", csi, csi->otf_info.csi_ch,
				     link_vc);
				return -ENODEV;
			}

			ctrl->value = csi_hw_g_frameptr(wdma->regs_vc[wdma_vc], link_vc);
		}
		break;
	default:
		break;
	}

	return ret;
}

static long csi_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct is_device_csi *csi;
	struct is_device_sensor *device;
	struct is_group *group;
	struct is_sensor_cfg *sensor_cfg;
	int wdma_vc, idx_wdma;
	struct v4l2_control *ctrl;
	struct is_sysfs_debug *sysfs_debug;
	struct pablo_camif_otf_info *otf_info;
	struct is_camif_wdma_module *wdma_mod;
	struct is_camif_wdma *wdma;
	u32 retry = 0;

	FIMC_BUG(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	otf_info = &csi->otf_info;
	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);

	switch (cmd) {
	/* cancel the current and next dma setting */
	case SENSOR_IOCTL_DMA_CANCEL:
		group = &device->group_sensor;

		for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
			wdma = csi->wdma[idx_wdma];
			if (!wdma) {
				merr("[CSI%d] wdma is NULL", csi, otf_info->csi_ch);
				return -ENODEV;
			}

			csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);

			for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
				csi_s_output_dma(csi, idx_wdma, wdma_vc, false);
		}
		break;
	case SENSOR_IOCTL_PATTERN_ENABLE:
		wdma_mod = csi->wdma_mod[CSI_OTF_OUT_SINGLE];
		if (!wdma_mod) {
			merr("[CSI%d] wdma_mod is NULL", csi, otf_info->csi_ch);
			return -ENODEV;
		}

		/* Wait CSI idleness before turn off pattern_gen. */
		while (atomic_read(&csi->vvalid) && retry++ < CSI_WAIT_ABORT_TIMEOUT)
			mdelay(1);

		if (is_camif_wdma_module_test_quirk(wdma_mod,
		    IS_CAMIF_WDMA_MODULE_QUIRK_BIT_HAS_TEST_PATTERN_GEN)) {
			u32 clk = 533000000; /* Unit: Hz, This is just for debugging. So, value is fixed */
			u32 fps;

			sensor_cfg = csi->sensor_cfg;
			if (!sensor_cfg) {
				mcerr(" sensor cfg is null", csi);
				ret = -EINVAL;
				goto p_err;
			}
			sysfs_debug = is_get_sysfs_debug();
			fps = sysfs_debug->pattern_fps > 0 ?
				sysfs_debug->pattern_fps : sensor_cfg->framerate;

			ret = csi_hw_s_dma_common_pattern_enable(wdma_mod->regs,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].width,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].height,
				fps, clk);
			if (ret) {
				mcerr(" csi_hw_s_dma_common_pattern is fail(%d)\n", csi, ret);
				goto p_err;
			}
		} else if (csi->top) {
			u32 dvfs_freq[IS_DVFS_END];

			is_dvfs_get_freq(&device->resourcemgr->dvfs_ctrl, dvfs_freq);

			CALL_CAMIF_TOP_OPS(csi->top, enable_ibuf_ptrn_gen, csi->sensor_cfg,
				otf_info, dvfs_freq[IS_DVFS_CSIS], true);
		}
		break;
	case SENSOR_IOCTL_PATTERN_DISABLE:
		wdma_mod = csi->wdma_mod[CSI_OTF_OUT_SINGLE];
		if (!wdma_mod) {
			merr("[CSI%d] wdma_mod is NULL", csi, otf_info->csi_ch);
			return -ENODEV;
		}

		if (is_camif_wdma_module_test_quirk(wdma_mod,
		    IS_CAMIF_WDMA_MODULE_QUIRK_BIT_HAS_TEST_PATTERN_GEN))
			csi_hw_s_dma_common_pattern_disable(wdma_mod->regs);
		else if (csi->top)
			CALL_CAMIF_TOP_OPS(csi->top, enable_ibuf_ptrn_gen, csi->sensor_cfg,
				otf_info, 0, false);
		break;
	case SENSOR_IOCTL_G_FRAME_ID:
		{
			u32 *hw_frame_id = (u32 *)arg;

			if (csi->f_id_dec) {
				wdma_mod = csi->wdma_mod[CSI_OTF_OUT_SINGLE];
				if (!wdma_mod) {
					merr("[CSI%d] wdma_mod is NULL", csi, otf_info->csi_ch);
					return -ENODEV;
				}
				csi_hw_g_dma_common_frame_id(wdma_mod->regs, csi->dma_batch_num, hw_frame_id);
			} else {
				ret = 1; /* HW frame ID decoder is not available. */
			}
		}
		break;
	case V4L2_CID_SENSOR_SET_FRS_CONTROL:
		ctrl = arg;

		switch (ctrl->value) {
		case FRS_SSM_MODE_FPS_960:
			csi->dma_batch_num = 960 / 60;
			break;
		case FRS_SSM_MODE_FPS_480:
			csi->dma_batch_num = 480 / 60;
			break;
		default:
			return ret;
		}

		if (csi_hw_get_version(csi->base_reg)
			< IS_CSIS_VERSION(5, 4, 0, 0)) {
			/*
			 * TODO: Both FRO count and frame id decoder dosen't have to be controlled
			 * if shadowing scheme is supported at start interrupt of frame id decoder.
			 */
			wdma_vc = DMA_VIRTUAL_CH_0;
			wdma = csi->wdma[0];
			if (!wdma) {
				merr("[CSI%d] wdma is NULL", csi, otf_info->csi_ch);
				return -ENODEV;
			}
			csi_hw_s_fro_count(wdma->regs_ctl, csi->dma_batch_num, wdma_vc);
		}

		mcinfo(" change dma_batch_num %d\n", csi, csi->dma_batch_num);
		break;
	case SENSOR_IOCTL_G_HW_FCOUNT:
		{
			u32 *hw_fcount = (u32 *)arg;

			*hw_fcount = csi_hw_g_fcount(csi->base_reg, CSI_VIRTUAL_CH_0);
		}
		break;
	case SENSOR_IOCTL_CSI_G_CTRL:
		ctrl = (struct v4l2_control *)arg;
		ret = csi_g_ctrl(subdev, ctrl);
		if (ret) {
			err("[CSI%d] csi_g_ctrl failed", otf_info->csi_ch);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_CSI_DMA_ATTACH:
		ret = csi_dma_attach(csi);
		if (ret) {
			merr("[CSI%d] csi_dma_attach is failed", csi, otf_info->csi_ch);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_G_DMA_CH:
		wdma = csi->wdma[CSI_OTF_OUT_SINGLE];
		if (!wdma) {
			merr("[CSI%d] wdma is NULL", csi, otf_info->csi_ch);
			return -ENODEV;
		}

		*(int *)arg = wdma->ch;
		break;
	case SENSOR_IOCTL_S_OTF_OUT:
		{
			struct pablo_camif_otf_info *otf_info = &csi->otf_info;
			u32 req_num, act_num;

			act_num = otf_info->act_otf_out_num;
			req_num = otf_info->req_otf_out_num = *(u32 *)arg;

			minfo("[CSI%d] Change OTF out num: %d -> %d\n", csi, otf_info->csi_ch, act_num, req_num);
		}
		break;
	case SENSOR_IOCTL_CSI_S_PDBUF_MASKING:
		csi_set_using_vc_buffer(arg);
		break;
	default:
		break;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = csi_init,
	.s_power = csi_s_power,
	.ioctl = csi_ioctl,
};

static void csi_free_irq(struct is_device_csi *csi)
{
	struct pablo_camif_ebuf *ebuf = pablo_camif_ebuf_get();
	struct is_camif_wdma *wdma;
	int idx_wdma;

	/* EBUF */
	if (ebuf)
		pablo_free_irq(ebuf->irq, csi);

	/* WDMA */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		csi_hw_s_dma_irq_msk(wdma->regs_ctl, false);

		pablo_free_irq(wdma->irq, csi);
	}


	if (csi->stat_wdma) {
		csi_hw_s_dma_irq_msk(csi->stat_wdma->regs_ctl, false);
		pablo_free_irq(csi->stat_wdma->irq, csi);
	}

	/* LINK */
	csi_hw_s_irq_msk(csi->base_reg, false, csi->f_id_dec);
	pablo_free_irq(csi->irq, csi);

	return;
}

static int csi_request_irq(struct is_device_csi *csi)
{
	int ret = 0, idx_wdma;
	size_t name_len;
	struct pablo_camif_ebuf *ebuf = pablo_camif_ebuf_get();
	struct is_camif_wdma *wdma;

	/* Registration of CSIS LINK IRQ */
	name_len = sizeof(csi->irq_name);
	snprintf(csi->irq_name, name_len, "%s-%d", "CSI", csi->otf_info.csi_ch);
	ret = pablo_request_irq(csi->irq, is_isr_csi, csi->irq_name, 0, csi);
	if (ret) {
		mcerr("failed to request IRQ for CSI(%d): %d", csi, csi->irq, ret);
		goto err_req_csi_irq;
	}
	csi_hw_s_irq_msk(csi->base_reg, true, csi->f_id_dec);

	/* Registration of DMA IRQ */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		ret = pablo_request_irq(wdma->irq, is_isr_csi_dma, wdma->irq_name, 0, csi);
		if (ret) {
			mcerr("failed to request IRQ(%d) for DMA#%d : ret(%d)", csi, wdma->irq,
			      csi->wdma[idx_wdma]->ch, ret);
			goto err_req_dma_irq;
		}
		csi_hw_s_dma_irq_msk(wdma->regs_ctl, true);
	}

	if (csi->stat_wdma) {
		wdma = csi->stat_wdma;
		ret = pablo_request_irq(wdma->irq, is_isr_csi_dma, wdma->irq_name, 0, csi);
		if (ret) {
			mcerr("failed to request IRQ(%d) for DMA#%d : ret(%d)", csi, wdma->irq,
			      wdma->ch, ret);
			goto err_req_dma_irq;
		}
		csi_hw_s_dma_irq_msk(wdma->regs_ctl, true);
	}

	/* Registration of CSIS_PDP_TOP */
	if (ebuf) {
		ret = pablo_request_irq(ebuf->irq, is_isr_csi_ebuf, "CSI_EBUF", IRQF_SHARED, csi);
		if (ret) {
			mcerr("failed to request IRQ for CSI_EBUF(%d): %d", csi, ebuf->irq, ret);
			goto err_req_ebuf_irq;
		}
	}

	return 0;

err_req_ebuf_irq:
err_req_dma_irq:
	csi_free_irq(csi);

err_req_csi_irq:

	return ret;
}

static int is_csi_get_phy_setfile_from_fw(struct is_device_csi *csi,
					  struct phy_setfile_table *phy_sf_tbl, struct device *dev)
{
	struct phy_setfile *sf[PPS_MAX];
	struct phy_setfile_header *sf_h;
	struct phy_setfile_body *sf_b;
	struct phy_setfile *sf_iter;
	struct pablo_camif_otf_info *otf_info;
	const struct firmware *fw;
	size_t size[PPS_MAX + 1];
	int i, j, k, sf_b_offset, sf_offset;
	bool match = false;
	int ret = 0;

	otf_info = &csi->otf_info;

	ret = request_firmware(&fw, "dbg_phy_setfile.bin", dev);
	if (ret) {
		mcerr("failed to load phy setfile", csi);
		return ret;
	}

	if (phy_sf_tbl->sz_comm) {
		kfree(phy_sf_tbl->sf_comm);
		phy_sf_tbl->sz_comm = 0;
	}

	if (phy_sf_tbl->sz_lane) {
		kfree(phy_sf_tbl->sf_lane);
		phy_sf_tbl->sz_lane = 0;
	}

	sf_h = (struct phy_setfile_header *)fw->data;

	for (i = 0, sf_b_offset = 0; i < sf_h->body_cnt; i++) {
		sf_b = (struct phy_setfile_body *)&sf_h->body[sf_b_offset];

		size[PPS_MAX] = 0;
		for (j = 0; j < PPS_MAX; j++) {
			size[j] = sf_b->cnt[j];
			size[PPS_MAX] += size[j];
		}

		if ((sf_b->phy_type == PPT_MIPI_DPHY && csi->use_cphy) ||
		    (sf_b->phy_type == PPT_MIPI_CPHY && !csi->use_cphy)) {
			sf_b_offset += sizeof(struct phy_setfile_body) +
				       sizeof(struct phy_setfile) * size[PPS_MAX];
			continue;
		}

		if (sf_b->phy_port != PPP_COMMON &&
		    sf_b->phy_port - 1 != csi_hw_g_mapped_phy_port(otf_info->csi_ch)) {
			sf_b_offset += sizeof(struct phy_setfile_body) +
				       sizeof(struct phy_setfile) * size[PPS_MAX];
			continue;
		}

		for (j = 0; j < PPS_MAX; j++) {
			sf[j] = kcalloc(size[j], sizeof(struct phy_setfile), GFP_KERNEL);
			if (!sf[j]) {
				while (j--)
					kfree(sf[j]);

				ret = -ENOMEM;
				goto out;
			}
		}

		for (j = 0, sf_offset = 0; j < PPS_MAX; j++) {
			for (k = 0; k < sf_b->cnt[j]; k++) {
				sf_iter = (struct phy_setfile *)&sf_b->data[sf_offset];
				memcpy(&sf[j][k], sf_iter, sizeof(struct phy_setfile));

				if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_CSI) > 2)) {
					info("%s %d) addr(0x%04x), start_bit(%d)"
					     ", bit_width(%d), val(0x%08x), "
					     "index(%d), max_lane(%d)",
					     phy_setfile_str[j], k, sf[j][k].addr, sf[j][k].start,
					     sf[j][k].width, sf[j][k].val, sf[j][k].index,
					     sf[j][k].max_lane);
				}

				sf_offset += sizeof(struct phy_setfile);
			}
		}

		phy_sf_tbl->sf_comm = sf[PPS_COMM];
		phy_sf_tbl->sz_comm = size[PPS_COMM];
		phy_sf_tbl->sf_lane = sf[PPS_LANE];
		phy_sf_tbl->sz_lane = size[PPS_LANE];

		{
			char str[] = "A";

			str[0] = str[0] + sf_b->phy_port - 1;

			mcinfo("phy setfile: %c-phy, XMIPI_CSI%s\n",
				csi, sf_b->phy_type?'C':'D',
				sf_b->phy_port?str:"_COMMON");
		}

		if (sf_b->phy_port == PPP_COMMON ||
		    sf_b->phy_port - 1 == csi_hw_g_mapped_phy_port(otf_info->csi_ch)) {
			match = true;
			break;
		}

		sf_b_offset += sizeof(struct phy_setfile_body) + sf_offset;
	}

	ret = 0;

	if (!match)
		ret = -EPERM;

out:
	release_firmware(fw);

	return ret;
}

static void csi_dma_init(struct is_device_csi *csi,
	struct is_sensor_cfg *sensor_cfg)
{
	u32 idx_wdma, wdma_vc, link_vc;

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++) {
		/* runtime buffer done state for error */
		clear_bit(CSIS_BUF_ERR_VC0 + link_vc, &csi->state);
	}

	csi_s_config_dma(csi);
	memset(csi->pre_dma_enable, -1, sizeof(csi->pre_dma_enable));

	/* for multi frame buffer setting for internal vc */
	csis_s_vc_dma_multibuf(csi);

	/* DMA Workqueue Setting */
	INIT_WORK(&csi->wq_csis_dma, csi_dma_work_fn);
	init_work_list(&csi->work_list, WORK_CSIS_WDMA, MAX_WORK_COUNT);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
			csi->dma_error_id[idx_wdma][wdma_vc] = 0;
	}
}

static int csi_stream_on(struct v4l2_subdev *subdev,
	struct is_device_csi *csi,
	int enable)
{
	int ret = 0;
	u32 otf_ch, otf_out_id, link_vc;
	u32 __iomem *base_reg;
	struct is_device_sensor *device = v4l2_get_subdev_hostdata(subdev);
	struct is_sensor_cfg *sensor_cfg;
	struct is_group *group = &device->group_sensor;
	struct is_sysfs_debug *sysfs_debug = is_get_sysfs_debug();
	struct pablo_camif_otf_info *otf_info;

	FIMC_BUG(!device);

	csi->state_cnt.err = 0;
	csi->state_cnt.str = 0;
	csi->state_cnt.end = 0;
	csi->sw_checker = EXPECT_FRAME_START;

	sensor_cfg = csi->sensor_cfg;
	if (!sensor_cfg) {
		mcerr(" sensor cfg is null", csi);
		ret = -EINVAL;
		goto err_invalid_sensor_cfg;
	}

	otf_info = &csi->otf_info;
	otf_info->max_vc_num = sensor_cfg->max_vc + 1;
	ret = csi_g_otf_ch(otf_info, group, sensor_cfg->dma_num);
	if (ret) {
		merr("[CSI%d] Failed to get OTF_OUT CH", csi, otf_info->csi_ch);
		goto err_get_otf_out_ch;
	}

	csi_g_otf_lc(otf_info, sensor_cfg, device->state);

	ret = csi_dma_attach(csi);
	if (ret) {
		merr("[CSI%d] csi_dma_attach is failed", csi, otf_info->csi_ch);
		goto err_dma_attach;
	}

	is_vendor_csi_stream_on(csi);

	if (test_bit(CSIS_START_STREAM, &csi->state)) {
		mcerr(" already start", csi);
		ret = -EINVAL;
		goto err_start_already;
	}

	base_reg = csi->base_reg;
	csi->hw_fcount = csi_hw_s_fcount(base_reg, CSI_VIRTUAL_CH_0, 0);
	atomic_set(&csi->chain_fcount, 0);
	csi->error_count = 0;
	csi->error_count_vc_overlap = 0;
	otf_ch = otf_info->otf_out_ch[CAMIF_OTF_OUT_SINGLE];

	ret = csi_s_fro(csi, device, otf_ch);
	if (ret) {
		mcerr(" csi_s_fro is fail", csi);
		goto err_csi_s_fro;
	}

	ret = is_hw_camif_cfg((void *)device);
	if (ret) {
		mcerr("hw_camif_cfg is error(%d)", csi, ret);
		goto err_camif_cfg;
	}

	ret = csi_request_irq(csi);
	if (ret) {
		mcerr(" csi_request_irq is fail", csi);
		goto err_csi_request_irq;
	}

	if (test_bit(IS_SENSOR_ONLY, &device->state) && !sysfs_debug->pattern_en)
		csi->potf = true;
	else
		csi->potf = false;

	mcinfo(" dma_num(%u), potf(%d), hw_fcount(%d)\n", csi, sensor_cfg->dma_num, csi->potf,
		csi->hw_fcount);

	/* PHY control */
	if (enable != IS_ENABLE_LINK_ONLY && !IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ)) {
		/* if sysreg_is is secure, skip phy reset */
		if (!(IS_ENABLED(SECURE_CAMERA_FACE) && IS_ENABLED(PROTECT_SYSREG_IS) &&
		      (device->ex_scenario == IS_SCENARIO_SECURE))) {
			static struct phy_setfile_table tmp_phy_sf_tbl;
			struct phy_setfile_table *phy_sf_tbl;
			int offset = is_vendor_is_dualized(device, device->position);

			phy_sf_tbl = &csi->phy_sf_tbl[offset];

			/* PHY be configured in PHY driver */
			if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_CSI) > 2)) {
				ret = is_csi_get_phy_setfile_from_fw(csi, &tmp_phy_sf_tbl,
								     &device->pdev->dev);

				if (ret) {
					mcerr("failed to load and get phy setfile from fw(%d)", csi,
					      ret);

					if (ret != -EPERM)
						goto err_set_phy;
				} else {
					phy_sf_tbl = &tmp_phy_sf_tbl;
				}
			}

			ret = csi_hw_s_phy_set(csi->phy, sensor_cfg->lanes, sensor_cfg->mipi_speed,
				sensor_cfg->settle, otf_info->csi_ch, csi->use_cphy, phy_sf_tbl,
				csi->phy_reg, csi->base_reg);
			if (ret) {
				mcerr(" csi_hw_s_phy_set is fail", csi);
				goto err_set_phy;
			}
		}
	}

	/* CSIS core setting */
	csi_hw_s_lane(base_reg, sensor_cfg->lanes, csi->use_cphy);
	csi_hw_s_control(base_reg, CSIS_CTRL_INTERLEAVE_MODE, sensor_cfg->interleave_mode);
	csi_hw_s_control(base_reg, CSIS_CTRL_PIXEL_ALIGN_MODE, 0x1);
	csi_hw_s_control(base_reg, CSIS_CTRL_LRTE, sensor_cfg->lrte);
	csi_hw_s_control(base_reg, CSIS_CTRL_DESCRAMBLE, device->pdata->scramble);

	if (sensor_cfg->interleave_mode == CSI_MODE_CH0_ONLY) {
		csi_hw_s_config(base_reg, CSI_VIRTUAL_CH_0, &sensor_cfg->input[CSI_VIRTUAL_CH_0],
				csi->image.window.width, csi->image.window.height, csi->potf);
	} else {
		u32 link_vc;

		for (link_vc = CSI_VIRTUAL_CH_0; link_vc <= sensor_cfg->max_vc; link_vc++) {
			csi_hw_s_config(base_reg, link_vc, &sensor_cfg->input[link_vc],
					sensor_cfg->input[link_vc].width,
					sensor_cfg->input[link_vc].height, csi->potf);

			if (CHECK_POTF_EN(sensor_cfg->input[link_vc].extformat)) {
				struct pablo_camif_mcb *mcb = pablo_camif_mcb_get();

				if (mcb)
					csi_hw_s_potf_ctrl(mcb->regs);
			}

			mcinfo("[VC%d] IN size %dx%d format 0x%x\n", csi, link_vc,
			       sensor_cfg->input[link_vc].width, sensor_cfg->input[link_vc].height,
			       sensor_cfg->input[link_vc].extformat);
		}
	}

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++) {
		/* clear fails alarm */
		csi->error_id[link_vc] = 0;
	}

	/* BNS configuration */
	if (csi->bns && otf_info->otf_out_num)
		pablo_camif_bns_cfg(csi->bns, sensor_cfg, otf_ch);

	/* CSIS OTF_OUT MUX configuration */
	if (csi->top) {
		memset(otf_info->link_vc_list, -1, sizeof(otf_info->link_vc_list));
		for (otf_out_id = 0; otf_out_id < otf_info->otf_out_num; otf_out_id++)
			CALL_CAMIF_TOP_OPS(csi->top, s_link_vc_list, otf_info, otf_out_id);

		CALL_CAMIF_TOP_OPS(csi->top, s_otf_out_mux, otf_info, CAMIF_OTF_OUT_SINGLE, true);
		CALL_CAMIF_TOP_OPS(csi->top, set_ibuf, otf_info, CAMIF_OTF_OUT_SINGLE,
				   csi->sensor_cfg, csi->potf);

		otf_info->act_otf_out_num = otf_info->req_otf_out_num = 1;
	}

	csi_dma_init(csi, sensor_cfg);

	/*
	 * A csis line interrupt does not used any more in actual scenario.
	 * But it can be used for debugging.
	 */
	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_CSI) >= 5)) {
		/* update line_fcount for sensor_notify_by_line */
		device->line_fcount = atomic_read(&csi->fcount) + 1;

		set_bit(CSIS_LINE_IRQ_ENABLE, &csi->state);

		csi_hw_s_control(base_reg, CSIS_CTRL_LINE_RATIO, csi->image.window.height * CSI_LINE_RATIO / 20);
		csi_hw_s_control(base_reg, CSIS_CTRL_ENABLE_LINE_IRQ, 0x1);

		mcinfo("start line IRQ fcount: %d, ratio: %d\n", csi, device->line_fcount, CSI_LINE_RATIO);
	}

	/* EBUF configuration */
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state) && otf_info->otf_out_num) {
		struct pablo_camif_mcb *mcb = pablo_camif_mcb_get();
		struct pablo_camif_ebuf *ebuf = pablo_camif_ebuf_get();

		if (mcb) {
			csi->mcb = mcb;

			mutex_lock(&mcb->lock);
			if (!mcb->active_ch)
				csi_hw_s_mcb_qch(mcb->regs, true);

			set_bit(otf_ch, &mcb->active_ch);
			mutex_unlock(&mcb->lock);
		}

		if (ebuf) {
			csi->ebuf = ebuf;

			mutex_lock(&ebuf->lock);
			csi_hw_s_ebuf_enable(ebuf->regs, true, otf_ch, EBUF_AUTO_MODE,
					ebuf->num_of_ebuf, ebuf->fake_done_offset);
			mutex_unlock(&ebuf->lock);

			/* ebuf support 2 vc */
			csi_hw_s_cfg_ebuf(ebuf->regs, otf_ch, CSI_VIRTUAL_CH_0,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].width,
				sensor_cfg->input[CSI_VIRTUAL_CH_0].height);

			csi_hw_s_cfg_ebuf(ebuf->regs, otf_ch, CSI_VIRTUAL_CH_1,
				sensor_cfg->input[CSI_VIRTUAL_CH_1].width,
				sensor_cfg->input[CSI_VIRTUAL_CH_1].height);

			INIT_WORK(&csi->wq_ebuf_reset, wq_ebuf_reset);

			mcinfo("CSI(%d) --> EBUF(%d)\n", csi, otf_info->csi_ch, otf_ch);
		}
	}

	if (!sysfs_debug->pattern_en)
		csi_hw_enable(base_reg, csi->use_cphy);

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_CSI)))
		csi_hw_dump_all(csi);

	set_bit(CSIS_START_STREAM, &csi->state);
	csi->crc_flag = false;

	return 0;

err_set_phy:
	csi_free_irq(csi);

err_csi_request_irq:
err_camif_cfg:
err_csi_s_fro:
err_start_already:
	csi_dma_detach(csi);

err_dma_attach:
err_get_otf_out_ch:
err_invalid_sensor_cfg:
	return ret;
}

static void csi_dma_deinit(struct is_device_csi *csi)
{
	u32 wdma_vc, idx_wdma;
	long timetowait;
	struct is_camif_wdma *wdma;

	clear_bit(CSIS_DMA_DISABLING, &csi->state);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			/*
		 * DMA HW doesn't have own reset register.
		 * So, all virtual ch dma should be disabled
		 * because previous state is remained after stream on.
		 */
			wdma = csi->wdma[idx_wdma];
			if (wdma) {
				csi_s_output_dma(csi, idx_wdma, wdma_vc, false);
				csi_hw_dma_reset(wdma->regs_vc[wdma_vc]);
			}

			if (csi->stat_wdma)
				csi_hw_dma_reset(csi->stat_wdma->regs_vc[wdma_vc]);

			if (flush_work(&csi->wq_csis_dma))
				mcinfo("[WDMA%d][VC%d] flush_work executed!\n", csi, wdma->ch,
					wdma_vc);
		}
	}

	/* Always run DMA abort done to flush data that would be remained */
	set_bit(CSIS_DMA_FLUSH_WAIT, &csi->state);

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		if (wdma)
			csi_hw_s_control(wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);
	}

	if (csi->stat_wdma)
		csi_hw_s_control(csi->stat_wdma->regs_ctl, CSIS_CTRL_DMA_ABORT_REQ, true);

	timetowait = wait_event_timeout(csi->dma_flush_wait_q,
		!test_bit(CSIS_DMA_FLUSH_WAIT, &csi->state), CSI_WAIT_ABORT_TIMEOUT);
	if (!timetowait)
		mcerr(" wait ABORT_DONE timeout!\n", csi);

	csis_flush_all_vc_buf_done(csi, VB2_BUF_STATE_ERROR);

	/* Reset DMA input MUX */
	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		wdma = csi->wdma[idx_wdma];
		if (wdma && wdma->regs_mux)
			csi_hw_s_init_input_mux(wdma->regs_mux, idx_wdma, wdma->ch);
	}

	if (csi->stat_wdma)
		csi_hw_s_init_input_mux(csi->stat_wdma->regs_mux, idx_wdma, csi->stat_wdma->ch);
}

static void csi_rst_bufring_cnt(struct is_device_csi *csi)
{
	u32 val;

	/*
	 * When current VC0 out type is internal FRO mode,
	 * initial bufring_cnt must be 0 due to SBWC.
	 * For internal FRO mode, the 's_frameptr()' is done by DMA start ISR.
	 */
	if (IS_CSI_VC_FRO_OUT(csi, 0, 0))
		val = 0;
	else
		val = 1;

	atomic_set(&csi->bufring_cnt, val);
}

static int csi_stream_off(struct v4l2_subdev *subdev,
	struct is_device_csi *csi)
{
	u32 __iomem *base_reg;
	struct is_device_sensor *device = v4l2_get_subdev_hostdata(subdev);
	struct is_group *group;
	struct pablo_camif_otf_info *otf_info;

	FIMC_BUG(!csi);
	FIMC_BUG(!device);

	if (!test_bit(CSIS_START_STREAM, &csi->state)) {
		mcerr(" already stop", csi);
		return -EINVAL;
	}

	base_reg = csi->base_reg;
	otf_info = &csi->otf_info;

	csi_hw_disable(base_reg);

	csi_hw_reset(base_reg);

	group = &device->group_sensor;
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state) && otf_info->otf_out_num) {
		struct pablo_camif_mcb *mcb = csi->mcb;
		struct pablo_camif_ebuf *ebuf = csi->ebuf;
		u32 otf_ch = otf_info->otf_out_ch[CAMIF_OTF_OUT_SINGLE];

		if (mcb) {
			mutex_lock(&mcb->lock);
			clear_bit(otf_ch, &mcb->active_ch);
			if (!mcb->active_ch)
				csi_hw_s_mcb_qch(mcb->regs, false);

			mutex_unlock(&mcb->lock);
			csi_hw_fifo_reset(mcb->regs, otf_ch);
		}

		if (ebuf) {
			mutex_lock(&ebuf->lock);
			csi_hw_s_ebuf_enable(ebuf->regs, false, otf_ch, EBUF_AUTO_MODE,
					ebuf->num_of_ebuf, ebuf->fake_done_offset);
			mutex_unlock(&ebuf->lock);

			if (flush_work(&csi->wq_ebuf_reset))
				mcinfo("ebuf_reset flush_work executed!\n", csi);
		}
	}

	if (test_and_clear_bit(CSIS_LINE_IRQ_ENABLE, &csi->state))
		tasklet_kill(&csi->tasklet_csis_line);

	tasklet_kill(&csi->tasklet_csis_end);
	csi_dma_deinit(csi);

	if (csi->bns)
		pablo_camif_bns_reset(csi->bns);

	if (csi->top) {
		u32 otf_out_id;

		tasklet_kill(&csi->tasklet_csis_otf_cfg);

		for (otf_out_id = 0; otf_out_id < otf_info->otf_out_num; otf_out_id++)
			CALL_CAMIF_TOP_OPS(csi->top, s_otf_out_mux,
					otf_info, otf_out_id, false);

		otf_info->act_otf_out_num = otf_info->req_otf_out_num = 0;
	}

	atomic_set(&csi->vvalid, 0);
	memset(csi->cur_dma_enable, 0, sizeof(csi->cur_dma_enable));

	/* Clear FRO related variables */
	csi_rst_bufring_cnt(csi);
	csi->otf_batch_num = 1;
	csi->dma_batch_num = 1;

	csi_free_irq(csi);

	is_vendor_csi_stream_off(csi);

	csi_dma_detach(csi);

	clear_bit(CSIS_START_STREAM, &csi->state);

	mcinfo(" stream off done\n", csi);

	return 0;
}

static int csi_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct is_device_csi *csi;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	if (enable) {
		ret = phy_power_on(csi->phy);
		if (ret)
			err("failed to csi%d power on", csi->otf_info.csi_ch);

		ret = csi_stream_on(subdev, csi, enable);
		if (ret) {
			mcerr(" csi_stream_on is fail(%d)", csi, ret);
			goto p_err;
		}

	} else {
		ret = csi_stream_off(subdev, csi);
		if (ret) {
			mcerr(" csi_stream_off is fail(%d)", csi, ret);
			goto p_err;
		}

		ret = phy_power_off(csi->phy);
		if (ret)
			err("failed to csi%d power off", csi->otf_info.csi_ch);
	}

p_err:
	mdbgd_front("%s(%d)\n", csi, __func__, ret);

	return ret;
}

static int csi_set_internal_subdev(struct is_device_csi *csi,
	struct pablo_internal_subdev *i_subdev, struct is_vci_config *vci_cfg, u32 link_vc,
	u32 max_target_fps)
{
	u32 w, h, bits_per_pixel, buffer_num;
	const char *type_name;
	struct is_core *core = is_get_is_core();
	struct is_mem *mem;

	w = vci_cfg->width;
	h = vci_cfg->height;

	/* VC type dependent value setting */
	switch (vci_cfg->type) {
	case VC_TAILPDAF:
		if (CHECK_PACKED_EN(vci_cfg->extformat))
			bits_per_pixel = 10;
		else
			bits_per_pixel = 16;

		buffer_num =
			CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_pdaf_buf_num, max_target_fps);
		if (!buffer_num)
			buffer_num = SUBDEV_INTERNAL_BUF_MAX;
		type_name = "VC_TAILPDAF";
		break;
	case VC_MIPISTAT:
		bits_per_pixel = 8;
		buffer_num = SUBDEV_INTERNAL_BUF_MAX;
		type_name = "VC_MIPISTAT";
		break;
	case VC_EMBEDDED:
		bits_per_pixel = 8;
		buffer_num = SUBDEV_INTERNAL_BUF_MAX;
		type_name = "VC_EMBEDDED";
		break;
	case VC_EMBEDDED2:
		bits_per_pixel = 8;
		buffer_num = SUBDEV_INTERNAL_BUF_MAX;
		type_name = "VC_EMBEDDED2";
		break;
	case VC_PRIVATE:
		bits_per_pixel = 8;
		buffer_num = SUBDEV_INTERNAL_BUF_MAX;
		type_name = "VC_PRIVATE";
		break;
	case VC_FRO:
		/* Just reuse a single buffer for every FRO DMAs */
		bits_per_pixel = 16;
		buffer_num = 1;
		type_name = "VC_FRO";
		break;
	case VC_VPDAF:
		if (CHECK_PACKED_EN(vci_cfg->extformat))
			bits_per_pixel = 10;
		else
			bits_per_pixel = 16;

		buffer_num =
			CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_pdaf_buf_num, max_target_fps);
		if (!buffer_num)
			buffer_num = SUBDEV_INTERNAL_BUF_MAX;
		type_name = "VC_VPDAF";
		break;
	default:
		merr("[CSI%d][VC%d] wrong internal vc type(%d)", csi, csi->otf_info.csi_ch, link_vc,
			vci_cfg->type);
		return -EINVAL;
	}

	mem = CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_iommu_mem, GROUP_ID_SS0);
	pablo_internal_subdev_probe(i_subdev, csi->instance, mem, type_name);

	vci_cfg->buffer_num = buffer_num;

	i_subdev->width = w;
	i_subdev->height = h;
	i_subdev->num_planes = 1;
	i_subdev->num_batch = 1;
	i_subdev->num_buffers = buffer_num;
	i_subdev->bits_per_pixel = bits_per_pixel;
	i_subdev->memory_bitwidth = bits_per_pixel;
	i_subdev->size[0] =
		ALIGN(DIV_ROUND_UP(i_subdev->width * i_subdev->memory_bitwidth, BITS_PER_BYTE),
			32) *
		i_subdev->height;

	return 0;
}

static int csi_s_format(struct v4l2_subdev *subdev,
	v4l2_subdev_config_t *cfg,
	struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	u32 link_vc, idx_wdma, wdma_vc;
	struct is_device_csi *csi;
	struct is_device_sensor *device;
	struct is_sensor_cfg *sensor_cfg;

	FIMC_BUG(!subdev);
	FIMC_BUG(!fmt);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -ENODEV;
	}

	csi->image.window.offs_h = 0;
	csi->image.window.offs_v = 0;
	csi->image.window.width = fmt->format.width;
	csi->image.window.height = fmt->format.height;
	csi->image.window.o_width = fmt->format.width;
	csi->image.window.o_height = fmt->format.height;
	csi->image.format.pixelformat = fmt->format.code;
	csi->image.format.field = fmt->format.field;

	device = v4l2_get_subdev_hostdata(subdev);
	if (!device) {
		merr("device is NULL", csi);
		ret = -ENODEV;
		goto p_err;
	}

	sensor_cfg = csi->sensor_cfg = device->cfg;
	if (!device->cfg) {
		merr("sensor cfg is invalid", csi);
		ret = -EINVAL;
		goto p_err;
	}

	if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE)) {
		csi->f_id_dec = false;
		csi->dma_batch_num = 1;
	} else {
		if (sensor_cfg->ex_mode == EX_DUALFPS_960) {
			csi->f_id_dec = true;
			csi->dma_batch_num = 960 / 60;
		} else if (sensor_cfg->ex_mode == EX_DUALFPS_480) {
			csi->f_id_dec = true;
			csi->dma_batch_num = 480 / 60;
		} else {
			csi->f_id_dec = false;
			csi->dma_batch_num = 1;
		}
	}

	csi_rst_bufring_cnt(csi);

	csi->bns = pablo_camif_bns_get();
	if (csi->bns)
		is_sensor_s_bns(device, sensor_cfg->output[0][CSI_VIRTUAL_CH_0].width,
				sensor_cfg->output[0][CSI_VIRTUAL_CH_0].height);
	/*
	 * DMA HW doesn't have own reset register.
	 * So, all virtual ch dma should be disabled
	 * because previous state is remained after stream on.
	 */

	for (idx_wdma = 0; idx_wdma < WDMA_MAX(csi); idx_wdma++) {
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++) {
			struct pablo_internal_subdev *i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];
			struct is_vci_config *vci_cfg;

			if (test_bit(PABLO_SUBDEV_ALLOC, &i_subdev->state)) {
				if (CALL_I_SUBDEV_OPS(i_subdev, free, i_subdev))
					mcerr("[VC%d][%s] is_subdev_internal_close is fail", csi,
						wdma_vc, i_subdev->name);
			}

			vci_cfg = &sensor_cfg->output[idx_wdma][wdma_vc];
			if (vci_cfg->type == VC_NOTHING)
				continue;

			link_vc = csi->sensor_cfg->link_vc[idx_wdma][wdma_vc];

			ret = csi_set_internal_subdev(
				csi, i_subdev, vci_cfg, link_vc, device->max_target_fps);
			if (ret)
				return ret;
		}
	}

p_err:
	mdbgd_front("%s(%dx%d, %X)\n", csi, __func__, fmt->format.width, fmt->format.height, fmt->format.code);
	return ret;
}


/* External only */
static int csi_s_buffer(struct v4l2_subdev *subdev, void *buf, unsigned int *vcp)
{
	struct is_device_csi *csi;
	struct is_framemgr *framemgr;
	struct is_subdev *dma_subdev;
	struct pablo_internal_subdev *i_subdev;
	struct is_frame *frame;
	struct pablo_camif_otf_info *otf_info;
	struct is_camif_wdma *wdma;
	u32 link_vc, idx_wdma, wdma_vc, link_vc_out_idx;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (unlikely(csi == NULL)) {
		err("csi is NULL");
		return -EINVAL;
	}

	otf_info = &csi->otf_info;

	/* for virtual channels */
	frame = (struct is_frame *)buf;
	if (!frame) {
		err("frame is NULL");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_SENSOR_GROUP_LVN)) {
		link_vc = *vcp;
		for (link_vc_out_idx = 0;
		     link_vc_out_idx < csi->sensor_cfg->link_vc_out_idx[link_vc];
		     link_vc_out_idx++) {
			idx_wdma = csi->sensor_cfg->dma_idx[link_vc][link_vc_out_idx];
			wdma_vc = csi->sensor_cfg->dma_vc[link_vc][link_vc_out_idx];
			i_subdev = &csi->i_subdev[idx_wdma][wdma_vc];

			msrdbgs(1, "[CSI%d] %s\n", i_subdev, i_subdev, frame, otf_info->csi_ch,
				__func__);

			if (!INTERNAL_BUF_ONLY(i_subdev, csi, idx_wdma, wdma_vc))
				break;
		}
	} else {
		dma_subdev = frame->subdev;
		link_vc = CSI_ENTRY_TO_CH(dma_subdev->id);
		framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
		wdma_vc = link_vc % DMA_VIRTUAL_CH_MAX;
		idx_wdma = CAMIF_OTF_OUT_SINGLE;

		if (!framemgr) {
			err("framemgr is NULL");
			return -EINVAL;
		}

		msrdbgs(1, "[CSI%d] %s\n", dma_subdev, dma_subdev, frame, otf_info->csi_ch,
			__func__);
	}

	if (wdma_vc != CSI_VIRTUAL_CH_0 && csi->stat_wdma)
		wdma = csi->stat_wdma;
	else
		wdma = csi->wdma[idx_wdma];

	if (!wdma) {
		merr("[CSI%d][VC%d] wdma is NULL", csi, otf_info->csi_ch, wdma_vc);
		return -ENODEV;
	}

	if (!IS_CSI_VC_FRO_OUT(csi, idx_wdma, wdma_vc)) {
		if (csi_hw_g_output_dma_enable(wdma->regs_vc[wdma_vc], wdma_vc)) {
			err("[WDMA%d][VC%d][F%d] already DMA enabled!!", wdma->ch, wdma_vc,
			    frame->fcount);
			frame->result = IS_SHOT_BAD_FRAME;
			return -EINVAL;
		}

		csi_s_frameptr(csi, idx_wdma, wdma_vc, false);
	}

	csi_s_buf_addr(csi, frame, idx_wdma, wdma_vc);
	csi_s_output_dma(csi, idx_wdma, wdma_vc, true);

	return 0;
}

static int csi_g_errorCode(struct v4l2_subdev *subdev, u32 *errorCode)
{
	int ret = 0;
	int link_vc;
	struct is_device_csi *csi;

	FIMC_BUG(!subdev);

	csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	for (link_vc = CSI_VIRTUAL_CH_0; link_vc < CSI_VIRTUAL_CH_MAX; link_vc++)
		*errorCode |= csi->error_id_last[link_vc];

	return ret;
}


static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = csi_s_stream,
	.s_rx_buffer = csi_s_buffer,
	.g_input_status = csi_g_errorCode
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = csi_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

static int is_csi_get_phy_setfile_from_dt(struct phy_setfile_table *phy_sf_tbl,
		struct device_node *sf_node)
{
	struct phy_setfile *sf[PPS_MAX];
	size_t sz[PPS_MAX];
	struct device_node *child;
	struct property *prop;
	int i_prop, i_child = 0;
	size_t size, elem_size;
	int ret;

	size = sizeof(struct phy_setfile);
	elem_size = size / sizeof(u32);

	for_each_child_of_node(sf_node, child) {
		i_prop = 0;
		for_each_property_of_node(child, prop) {
			dbg("%s[%s]: property #%d\n", child->name, prop->name,
				i_prop);
			i_prop++;
		}

		sz[i_child] = i_prop - 1;
		sf[i_child] = kcalloc(i_prop, size, GFP_KERNEL);
		if (!sf[i_child]) {
			ret = -ENOMEM;
			goto out;
		}

		i_prop = 0;
		for_each_property_of_node(child, prop) {
			if (i_prop >= sz[i_child])
				break;

			ret = of_property_read_u32_array(child,
					prop->name, (u32 *)&sf[i_child][i_prop],
					elem_size);
			if (ret) {
				err("failed of_property_read_u32_array: %s[%s] ret(%d)",
					child->name, prop->name, ret);

				i_child++;
				goto out;
			}

			dbg("%s[%s] addr(%04X) start_bit(%d) bit_width(%d)"
				" val(%08X) index(%d) max_lane(%d)\n",
				child->name, prop->name,
				sf[i_child][i_prop].addr,
				sf[i_child][i_prop].start,
				sf[i_child][i_prop].width,
				sf[i_child][i_prop].val,
				sf[i_child][i_prop].index,
				sf[i_child][i_prop].max_lane);
			i_prop++;
		}
		i_child++;
	}

	phy_sf_tbl->sf_comm = sf[0];
	phy_sf_tbl->sz_comm = sz[0];
	phy_sf_tbl->sf_lane = sf[1];
	phy_sf_tbl->sz_lane = sz[1];

	of_node_put(sf_node);

	return 0;

out:
	while (i_child--)
		kfree(sf[i_child]);

	of_node_put(sf_node);

	return ret;
}

static int is_csi_get_phy_setfile(struct v4l2_subdev *subdev)
{
	struct is_device_sensor *sensor = v4l2_get_subdev_hostdata(subdev);
	struct is_device_csi *csi = v4l2_get_subdevdata(subdev);
	struct device *dev = &sensor->pdev->dev;
	struct device_node *sf_node;
	int ret = 0, elems = 0, i;

	elems = of_property_count_u32_elems(dev->of_node, "phy_setfile");
	if (elems <= 0)
		return elems;

	minfo("[CSI%d] Has %d phy setfile in DT\n", csi, csi->otf_info.csi_ch, elems);

	csi->phy_sf_tbl = devm_kcalloc(dev, elems, sizeof(struct phy_setfile_table), GFP_KERNEL);
	if (!csi->phy_sf_tbl)
		return -ENOMEM;

	for (i = 0; i < elems; i++) {
		sf_node = of_parse_phandle(dev->of_node, "phy_setfile", i);
		if (sf_node) {
			ret = is_csi_get_phy_setfile_from_dt(&csi->phy_sf_tbl[i], sf_node);
			if (ret) {
				err("failed to get phy setfile from device tree(%d)", ret);
				goto out;
			}
			minfo("[CSI%d] %d index phy setfile is getting from DT\n",
				csi, csi->otf_info.csi_ch, i);
		}
	}

	return 0;

out:
	devm_kfree(dev, csi->phy_sf_tbl);

	return ret;
}

u32 csi_hw_g_bpp(u32 hwformat)
{
	u32 bitperpixel;

	switch (hwformat) {
	case HW_FORMAT_RAW8:
		bitperpixel = 8;
		break;
	case HW_FORMAT_RAW10:
		bitperpixel = 10;
		break;
	case HW_FORMAT_RAW12:
		bitperpixel = 12;
		break;
	case HW_FORMAT_RAW14:
		bitperpixel = 14;
		break;
	default:
		bitperpixel = 16;
	}

	return bitperpixel;
}

int is_csi_probe(void *parent, u32 device_id, u32 ch, int wdma_ch_hint)
{
	u32 idx_wdma, wdma_vc;
	int ret = 0;
	struct v4l2_subdev *subdev_csi;
	struct is_device_csi *csi;
	struct is_device_sensor *device = parent;
	struct is_core *core;
	struct resource *res;
	struct platform_device *pdev;
	struct device *dev;
	struct device_node *dnode;
	char name[IS_STR_LEN];
	struct pablo_camif_otf_info *otf_info;

	FIMC_BUG(!device);

	pdev = device->pdev;
	dev = &pdev->dev;
	dnode = dev->of_node;

	FIMC_BUG(!device->pdata);

	csi = devm_kzalloc(dev, sizeof(struct is_device_csi), GFP_KERNEL);
	if (!csi) {
		merr("failed to alloc memory for CSI device", device);
		ret = -ENOMEM;
		goto err_alloc_csi;
	}

	/* CSI */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csi");
	if (!res) {
		probe_err("failed to get memory resource for CSI");
		ret = -ENODEV;
		goto err_get_csi_res;
	}

	snprintf(name, IS_STR_LEN, "CSI%d", ch);
	is_debug_memlog_alloc_dump(res->start, resource_size(res), name);

	csi->regs_start = res->start;
	csi->regs_end = res->end;
	csi->base_reg = devm_ioremap(dev, res->start, resource_size(res));
	if (!csi->base_reg) {
		probe_err("can't ioremap for CSI");
		ret = -ENOMEM;
		goto err_ioremap_csi;
	}

	csi->irq = platform_get_irq_byname(pdev, "csi");
	if (csi->irq < 0) {
		probe_err("failed to get IRQ resource for CSI: %d", csi->irq);
		ret = csi->irq;
		goto err_get_csi_irq;
	}

	/* PHY */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	if (!res) {
		probe_err("failed to get memory resource for PHY");
		ret = -ENODEV;
		goto err_get_phy_res;
	}

	if (ch == CSI_ID_A) {
		snprintf(name, IS_STR_LEN, "PHY%d", ch);
		is_debug_memlog_alloc_dump(res->start, resource_size(res), name);
	}

	csi->phy_reg = devm_ioremap(dev, res->start, resource_size(res));
	if (!csi->phy_reg) {
		probe_err("can't ioremap for PHY");
		ret = -ENOMEM;
		goto err_ioremap_phy;
	}

	csi->phy = devm_phy_get(dev, "csis_dphy");
	if (IS_ERR(csi->phy)) {
		probe_err("failed to get PHY device for CSI");
		ret = PTR_ERR(csi->phy);
		goto err_get_phy_dev;
	}

	/* FRO */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fro");
	if (!res) {
		probe_warn("failed to get memory resource for FRO");
		csi->fro_reg = NULL;
	} else {
		csi->fro_reg = devm_ioremap(dev, res->start, resource_size(res));
		if (!csi->fro_reg) {
			probe_err("can't ioremap for FRO");
			ret = -ENOMEM;
			goto err_ioremap_fro;
		}
	}

	/* CSIS OTF_OUT MUX */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "otf_out_mux");
	if (res) {
		csi->otf_out_mux = devm_ioremap(dev, res->start, resource_size(res));
		if (!csi->otf_out_mux) {
			probe_err("can't ioremap for OTF_OUT_MUX");
			ret = -ENOMEM;
			goto err_ioremap_otf_out_mux;
		}
	}

	otf_info = &csi->otf_info;
	otf_info->csi_ch = ch;
	csi->wdma_ch_hint = wdma_ch_hint;
	csi->device_id = device_id;
	csi->use_cphy = device->pdata->use_cphy;
	minfo("[CSI%d] use_cphy(%d)\n", csi, otf_info->csi_ch, csi->use_cphy);

	subdev_csi = devm_kzalloc(dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -ENOMEM;
		goto err_alloc_subdev_csi;
	}

	device->subdev_csi = subdev_csi;

	csi->subdev = &device->subdev_csi;
	core = device->private_data;

	snprintf(csi->name, IS_STR_LEN, "CSI%d", otf_info->csi_ch);

	/* default state setting */
	clear_bit(CSIS_SET_MULTIBUF_DMA0, &csi->state);
	clear_bit(CSIS_SET_MULTIBUF_DMA1, &csi->state);
	clear_bit(CSIS_SET_MULTIBUF_DMA2, &csi->state);
	clear_bit(CSIS_SET_MULTIBUF_DMA3, &csi->state);
	clear_bit(CSIS_SET_MULTIBUF_DMA4, &csi->state);

	/* init dma subdev slots */
	for (idx_wdma = 0; idx_wdma < CSIS_MAX_NUM_DMA_ATTACH; idx_wdma++)
		for (wdma_vc = DMA_VIRTUAL_CH_0; wdma_vc < DMA_VIRTUAL_CH_MAX; wdma_vc++)
			csi->dma_subdev[idx_wdma][wdma_vc] = &device->ssvc[idx_wdma][wdma_vc];

	/*
	 * Setup tasklets
	 *
	 * These should always be enabled to handle abnormal IRQ from CSI
	 * for PHY tuning sequence.
	 */
	pkv_tasklet_setup(&csi->tasklet_csis_end, tasklet_csis_end);
	pkv_tasklet_setup(&csi->tasklet_csis_line, tasklet_csis_line);
	pkv_tasklet_setup(&csi->tasklet_csis_otf_cfg, tasklet_csis_otf_cfg);

	csi->workqueue = alloc_workqueue("is-csi/[H/U]", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!csi->workqueue)
		probe_warn("failed to alloc CSI own workqueue, will be use global one");

	v4l2_subdev_init(subdev_csi, &subdev_ops);
	v4l2_set_subdevdata(subdev_csi, csi);
	v4l2_set_subdev_hostdata(subdev_csi, device);
	snprintf(subdev_csi->name, V4L2_SUBDEV_NAME_SIZE, "csi-subdev.%d", ch);
	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_csi);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto err_reg_v4l2_subdev;
	}

	/* Link interrupt logging for debugging */
	ret = kfifo_alloc(&csi->dbg_irq_info_fifo,
		NEXT_POWER_OF_2((u32)sizeof(struct dbg_irq_info) * DBG_IRQ_INFO_MAX), GFP_KERNEL);
	if (ret) {
		merr("failed to alloc fifo(%d)", device, ret);
		ret = -ENOMEM;
		goto err_reg_v4l2_subdev;
	}

	INIT_WORK(&csi->wq_link_dump, link_dump);

	spin_lock_init(&csi->dma_seq_slock);
	spin_lock_init(&csi->dma_irq_slock);
	spin_lock_init(&csi->dma_rta_lock);

	init_waitqueue_head(&csi->dma_flush_wait_q);

	ret = is_csi_get_phy_setfile(subdev_csi);
	if (ret) {
		merr("failed to get phy setfile(%d)", device, ret);
		ret = -EINVAL;
		goto err_get_phy_setfile;
	}

	minfo("[CSI%d] %s(%d)\n", csi, otf_info->csi_ch, __func__, ret);
	return 0;

err_get_phy_setfile:
	kfifo_free(&csi->dbg_irq_info_fifo);
err_reg_v4l2_subdev:
	devm_kfree(dev, subdev_csi);
	device->subdev_csi = NULL;

err_alloc_subdev_csi:
	if (csi->otf_out_mux)
		devm_iounmap(dev, csi->otf_out_mux);

err_ioremap_otf_out_mux:
	if (csi->fro_reg)
		devm_iounmap(dev, csi->fro_reg);

err_ioremap_fro:
	devm_phy_put(dev, csi->phy);

err_get_phy_dev:
	devm_iounmap(dev, csi->phy_reg);

err_ioremap_phy:
err_get_phy_res:
err_get_csi_irq:
	devm_iounmap(dev, csi->base_reg);

err_ioremap_csi:
err_get_csi_res:
	devm_kfree(dev, csi);

err_alloc_csi:
	err("[SS%d][CSI%d] %s: %d", device_id, ch, __func__, ret);
	return ret;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pablo_kunit_csi_func pablo_kunit_csi_test = {
	.csi_s_multibuf_addr = csi_s_multibuf_addr,
	.csi_dma_work_fn = csi_dma_work_fn,
	PKV_ASSIGN_TASKLET_CALLBACK(csis_line),
	.csi_hw_cdump_all = csi_hw_cdump_all,
	.csi_hw_dump_all = csi_hw_dump_all,
	.csi_hw_g_bpp = csi_hw_g_bpp,
};

struct pablo_kunit_csi_func *pablo_kunit_get_csi_test(void) {
	return &pablo_kunit_csi_test;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_csi_test);
#endif
