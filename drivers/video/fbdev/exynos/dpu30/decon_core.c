/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/console.h>
#include <linux/dma-buf.h>
#include <linux/ion_exynos.h>
#include <linux/sched/types.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/of_address.h>
#include <linux/debugfs.h>
#include <linux/pinctrl/consumer.h>
#include <video/mipi_display.h>
#include <media/v4l2-subdev.h>
#if defined(CONFIG_EXYNOS_DPU_SYSTRACE)
#include <trace/events/systrace.h>
#endif
#if defined(CONFIG_CAL_IF)
#include <soc/samsung/cal-if.h>
#endif
#include <dt-bindings/soc/samsung/exynos9830-devfreq.h>
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/exynos-debug.h>


#include "format.h"
#include "decon.h"
#include "dsim.h"
#include "./panels/exynos_panel_drv.h"
#include "../../../../dma-buf/sync_debug.h"
#include "dpp.h"
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
#include "displayport.h"
#endif
#if defined(CONFIG_EXYNOS_DECON_DQE)
#include "dqe.h"
#endif

#include "../../../../iommu/exynos-iommu.h"



int decon_log_level = 6;
module_param(decon_log_level, int, 0644);
int dpu_bts_log_level = 6;
module_param(dpu_bts_log_level, int, 0644);
int win_update_log_level = 6;
module_param(win_update_log_level, int, 0644);
int dpu_mres_log_level = 6;
module_param(dpu_mres_log_level, int, 0644);
int dpu_fence_log_level = 6;
module_param(dpu_fence_log_level, int, 0644);
int dpu_dma_buf_log_level = 6;
module_param(dpu_dma_buf_log_level, int, 0644);
int decon_systrace_enable;

#ifdef CONFIG_EXYNOS_FPS_CHANGE_NOTIFY
u64 ems_frame_cnt;
EXPORT_SYMBOL(ems_frame_cnt);
#endif

struct decon_device *decon_drvdata[MAX_DECON_CNT];
EXPORT_SYMBOL(decon_drvdata);

/*
 * This spin lock protects to read and write prev_used_dpp and prev_req_win
 * variables when multi display is in operation.
 */
DEFINE_SPINLOCK(g_slock);

/*
 * This variable is moved from decon_ioctl function,
 * because stack frame of decon_ioctl is over.
 */
static struct dpp_restrictions_info disp_res;

static char *decon_state_names[] = {
	"INIT",
	"ON",
	"DOZE",
	"HIBER",
#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
	"DOZE_WAKE",
#endif
	"DOZE_SUSPEND",
	"OFF",
	"TUI",
};

void decon_tracing_mark_write(struct decon_device *decon, char id, char *str1, int value)
{
#if defined(CONFIG_EXYNOS_DPU_SYSTRACE)
	char buf[DECON_TRACE_BUF_SIZE] = {0,};

	if (!decon->systrace.pid)
		return;

	switch (id) {
	case 'B': /* B : Begin */
		snprintf(buf, DECON_TRACE_BUF_SIZE, "B|%d|%s",
				decon->systrace.pid, str1);
		trace_tracing_mark_write(buf);
		break;
	case 'E': /* E : End */
		strcpy(buf, "E");
		trace_tracing_mark_write(buf);
		break;
	case 'C': /* C : Category */
		snprintf(buf, DECON_TRACE_BUF_SIZE,
				"C|%d|%s|%d", decon->systrace.pid, str1, value);
		trace_tracing_mark_write(buf);
		break;
	default:
		decon_err("%s:argument fail\n", __func__);
		return;
	}
#endif
}

static void decon_dump_using_dpp(struct decon_device *decon)
{
	int i;

	for (i = 0; i < decon->dt.dpp_cnt; i++) {
		if (test_bit(i, &decon->prev_used_dpp)) {
			struct v4l2_subdev *sd = NULL;
			sd = decon->dpp_sd[i];
			BUG_ON(!sd);
			v4l2_subdev_call(sd, core, ioctl, DPP_DUMP, NULL);
		}
	}
}

static void decon_up_list_saved(void)
{
	int i;
	struct decon_device *decon;
	int decon_cnt;

	decon_cnt = get_decon_drvdata(0)->dt.decon_cnt;

	for (i = 0; i < decon_cnt; i++) {
		decon = get_decon_drvdata(i);
		if (decon) {
			if (!list_empty(&decon->up.list) ||
					!list_empty(&decon->up.saved_list)) {
				decon->up_list_saved = true;
				decon_info("\n=== DECON%d TIMELINE %d ===\n",
						decon->id,
						atomic_read(&decon->fence.timeline));
			} else {
				decon->up_list_saved = false;
			}
		}
	}
}

void decon_dump(struct decon_device *decon, bool panel_dump)
{
	int acquired = console_trylock();
	struct decon_device *decon0 = get_decon_drvdata(0);
	void __iomem *base_regs = decon0->res.regs;
	u32 dsc_en = decon0->lcd_info->dsc.en;

	if (IS_DECON_OFF_STATE(decon)) {
		decon_info("%s: DECON%d is disabled, state(%d)\n",
				__func__, decon->id, decon->state);
		return;
	}

	/* decon2 needs decon0 setting info */
	if (decon->id) {
		 decon_info("\n>>> DECON0 Remain Frames : %d <<<",
				atomic_read(&decon0->up.remaining_frame));
		__decon_dump(0, base_regs, base_regs, dsc_en);
	}

	decon_info("\n>>> DECON%d Remain Frames : %d <<<", decon->id,
				atomic_read(&decon->up.remaining_frame));
	if (decon->d.buf_cnt_bak.count) {
		decon_info("First iovmm_map Fail Time: %llu <<<",
				ktime_to_ns(decon->d.buf_cnt_bak.timestamp));
		dpu_show_dma_attach_info("decon_dump", decon, 1);
	}
	dpu_show_readback_buf_info(decon, 1);

	__decon_dump(decon->id, decon->res.regs, base_regs,
			decon->lcd_info->dsc.en);

	if (decon->dt.out_type == DECON_OUT_DSI)
		v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				DSIM_IOC_DUMP, &panel_dump);
	decon_dump_using_dpp(decon);

	if (acquired)
		console_unlock();
}

#ifdef CONFIG_LOGGING_BIGDATA_BUG
extern unsigned int get_panel_bigdata(void);

/* Gen Big Data Error for Decon's Bug
 *
 * return value
 * 1. 31 ~ 28 : decon_id
 * 2. 27 ~ 24 : decon eint pend register
 * 3. 23 ~ 16 : dsim underrun count
 * 4. 15 ~  8 : 0x0e panel register
 * 5.  7 ~  0 : 0x0a panel register
 * */

static unsigned int gen_decon_bug_bigdata(struct decon_device *decon)
{
	struct dsim_device *dsim;
	unsigned int value, panel_value;
	unsigned int underrun_cnt = 0;

	/* for decon id */
	value = decon->id << 28;

	if (decon->id == 0) {
		/* for eint pend value */
		value |= (decon->eint_pend & 0x0f) << 24;

		/* for underrun count */
		dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
		if (dsim != NULL) {
			underrun_cnt = dsim->total_underrun_cnt;
			if (underrun_cnt > 0xff) {
				decon_info("DECON:INFO:%s:dsim underrun exceed 1byte : %d\n",
						__func__, underrun_cnt);
				underrun_cnt = 0xff;
			}
		}
		value |= underrun_cnt << 16;

		/* for panel dump */
		panel_value = get_panel_bigdata();
		value |= panel_value & 0xffff;
	}

	decon_info("DECON:INFO:%s:big data : %x\n", __func__, value);
	return value;
}

void log_decon_bigdata(struct decon_device *decon)
{
	unsigned int bug_err_num;

	bug_err_num = gen_decon_bug_bigdata(decon);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	sec_debug_set_extra_info_decon(bug_err_num);
#endif
}

#ifdef CONFIG_DISPLAY_USE_INFO
static int decon_dpui_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct decon_device *decon;
	struct dpui_info *dpui = data;
	int i, recovery_cnt = 0, value;
	static int prev_recovery_cnt;

	if (dpui == NULL) {
		panel_err("%s: dpui is null\n", __func__);
		return 0;
	}

	decon = container_of(self, struct decon_device, dpui_notif);

	for (i = 0; i < MAX_DPP_SUBDEV; i++) {
		value = 0;
		v4l2_subdev_call(decon->dpp_sd[i], core, ioctl,
				DPP_GET_RECOVERY_CNT, &value);
		recovery_cnt += value;
	}

	inc_dpui_u32_field(DPUI_KEY_EXY_SWRCV,
			max(0, recovery_cnt - prev_recovery_cnt));
	prev_recovery_cnt = recovery_cnt;

	return 0;
}
#endif /* CONFIG_DISPLAY_USE_INFO */
#endif /* CONFIG_LOGGING_BIGDATA_BUG */

/* ---------- CHECK FUNCTIONS ----------- */
static void decon_win_config_to_regs_param
	(int transp_length, struct decon_win_config *win_config,
	 struct decon_window_regs *win_regs, int ch, int idx)
{
	win_regs->wincon = wincon(idx);
	win_regs->start_pos = win_start_pos(win_config->dst.x, win_config->dst.y);
	win_regs->end_pos = win_end_pos(win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
	win_regs->pixel_count = (win_config->dst.w * win_config->dst.h);
	win_regs->whole_w = win_config->dst.f_w;
	win_regs->whole_h = win_config->dst.f_h;
	win_regs->offset_x = win_config->dst.x;
	win_regs->offset_y = win_config->dst.y;
	win_regs->ch = ch; /* ch */
	win_regs->plane_alpha = win_config->plane_alpha;
	win_regs->format = win_config->format;
	win_regs->blend = win_config->blending;

	decon_dbg("CH%d@ SRC:(%d,%d) %dx%d  DST:(%d,%d) %dx%d\n",
			ch,
			win_config->src.x, win_config->src.y,
			win_config->src.f_w, win_config->src.f_h,
			win_config->dst.x, win_config->dst.y,
			win_config->dst.w, win_config->dst.h);
}

u32 wincon(int idx)
{
	u32 data = 0;

	data |= WIN_EN_F(idx);

	return data;
}

bool decon_validate_x_alignment(struct decon_device *decon, int x, u32 w,
		u32 bits_per_pixel)
{
	uint8_t pixel_alignment = 32 / bits_per_pixel;

	if (x % pixel_alignment) {
		decon_err("left x not aligned to %u-pixel(bpp = %u, x = %u)\n",
				pixel_alignment, bits_per_pixel, x);
		return 0;
	}
	if ((x + w) % pixel_alignment) {
		decon_err("right X not aligned to %u-pixel(bpp = %u, x = %u, w = %u)\n",
				pixel_alignment, bits_per_pixel, x, w);
		return 0;
	}

	return 1;
}

void decon_dpp_stop(struct decon_device *decon, bool do_reset)
{
	int i;
	bool rst = false;
	struct v4l2_subdev *sd;

	for (i = 0; i < decon->dt.dpp_cnt; i++) { /* dpp channel order */
		if (test_bit(i, &decon->prev_used_dpp) &&
				!test_bit(i, &decon->cur_using_dpp)) {
			sd = decon->dpp_sd[i];
			BUG_ON(!sd);
			if (test_bit(i, &decon->dpp_err_stat) || do_reset)
				rst = true;

			v4l2_subdev_call(sd, core, ioctl, DPP_STOP, (bool *)rst);

			clear_bit(i, &decon->dpp_err_stat);
		}
	}

	spin_lock(&g_slock);

	for (i = 0; i < decon->dt.dpp_cnt; i++)
		if (test_bit(i, &decon->prev_used_dpp) &&
				!test_bit(i, &decon->cur_using_dpp))
			clear_bit(i, &decon->prev_used_dpp);

	for (i = 0; i < decon->dt.max_win; i++) /* window number order */
		if (test_bit(i, &decon->prev_req_win) &&
				!test_bit(i, &decon->cur_req_win))
			clear_bit(i, &decon->prev_req_win);

	spin_unlock(&g_slock);

	DPU_EVENT_LOG(DPU_EVT_RELEASE_RSC, &decon->sd, ktime_set(0, 0));
}



static void decon_free_unused_buf(struct decon_device *decon,
		struct decon_reg_data *regs, int win, int plane)
{
	struct decon_dma_buf_data *dma = &regs->dma_buf_data[win][plane];

	decon_info("%s, win[%d]plane[%d]\n", __func__, win, plane);

	if (!IS_ERR_OR_NULL(dma->attachment) && !IS_ERR_VALUE(dma->dma_addr)) {
		ion_iovmm_unmap(dma->attachment, dma->dma_addr);
		DPU_EVENT_LOG_MEMMAP(DPU_EVT_MEM_UNMAP, &decon->sd,
				dma->dma_addr, dma->dpp_ch, dma->sg_table);
		decon->d.buf_cnt.iovmm_unmap_cnt++;
	} else {
		decon_info("[DECON:INFO]:%s:attachment is null\n", __func__);
	}

	if (!IS_ERR_OR_NULL(dma->attachment) && !IS_ERR_OR_NULL(dma->sg_table)) {
		dma_buf_unmap_attachment(dma->attachment,
				dma->sg_table, DMA_TO_DEVICE);
		decon->d.buf_cnt.unmap_attach_cnt++;
	} else {
		decon_info("[DECON:INFO]:%s:sg_table is null\n", __func__);
	}

	if (dma->dma_buf && !IS_ERR_OR_NULL(dma->attachment)) {
		dma_buf_detach(dma->dma_buf, dma->attachment);
		decon->d.buf_cnt.buf_detach_cnt++;
#ifdef DEBUG_DMA_BUF_LEAK
		decon->free_cnt++;
#endif
	} else {
		decon_info("[DECON:INFO]:%s:dma->dma_buf is null\n", __func__);
	}

	if (dma->dma_buf)
		dma_buf_put(dma->dma_buf);

	memset(dma, 0, sizeof(struct decon_dma_buf_data));
}


static void decon_free_dma_buf(struct decon_device *decon,
		struct decon_dma_buf_data *dma)
{
	if (!dma->dma_addr) {
		decon_err("DECON:ERR:%s:dma addr is null\n", __func__);
		return;
	}

	if (dma->fence) {
		dma_fence_put(dma->fence);
		dma->fence = NULL;
	}

	if (!IS_ERR_OR_NULL(dma->attachment) && !IS_ERR_VALUE(dma->dma_addr)) {
		ion_iovmm_unmap(dma->attachment, dma->dma_addr);
		DPU_EVENT_LOG_MEMMAP(DPU_EVT_MEM_UNMAP, &decon->sd, dma->dma_addr,
				dma->dpp_ch, dma->sg_table);
		decon->d.buf_cnt.iovmm_unmap_cnt++;
	} else {
		decon_info("[DECON:INFO]:%s:attachment is null\n", __func__);
	}

	if (!IS_ERR_OR_NULL(dma->attachment) && !IS_ERR_OR_NULL(dma->sg_table)) {
		dma_buf_unmap_attachment(dma->attachment, dma->sg_table,
				DMA_TO_DEVICE);
		decon->d.buf_cnt.unmap_attach_cnt++;
	} else {
		decon_info("[DECON:INFO]:%s:sg_table is null\n", __func__);
	}

	if (dma->dma_buf && !IS_ERR_OR_NULL(dma->attachment)) {
		dma_buf_detach(dma->dma_buf, dma->attachment);
		decon->d.buf_cnt.buf_detach_cnt++;
#ifdef DEBUG_DMA_BUF_LEAK
		decon->free_cnt++;
#endif
	} else {
		decon_info("[DECON:INFO]:%s:dma->dma_buf is null\n", __func__);
	}

	if (dma->dma_buf)
		dma_buf_put(dma->dma_buf);

	memset(dma, 0, sizeof(struct decon_dma_buf_data));
}

#ifdef CONFIG_EXYNOS_DOZE_FIRST_FRAME_BLACK
static void decon_set_black_window(struct decon_device *decon)
{
	struct decon_window_regs win_regs;
	struct exynos_panel_info *lcd = decon->lcd_info;

	memset(&win_regs, 0, sizeof(struct decon_window_regs));
	win_regs.wincon = wincon(decon->dt.dft_win);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, lcd->xres, lcd->yres);
	decon_info("xres %d yres %d win_start_pos %x win_end_pos %x\n",
			lcd->xres, lcd->yres, win_regs.start_pos,
			win_regs.end_pos);
	win_regs.colormap = 0x000000;
	win_regs.pixel_count = lcd->xres * lcd->yres;
	win_regs.whole_w = lcd->xres;
	win_regs.whole_h = lcd->yres;
	win_regs.offset_x = 0;
	win_regs.offset_y = 0;
	decon_info("pixel_count(%d), whole_w(%d), whole_h(%d), x(%d), y(%d)\n",
			win_regs.pixel_count, win_regs.whole_w,
			win_regs.whole_h, win_regs.offset_x,
			win_regs.offset_y);
	decon_reg_set_window_control(decon->id, decon->dt.dft_win,
			&win_regs, true);
	decon_reg_all_win_shadow_update_req(decon->id);
}
#endif


int _decon_tui_protection(bool tui_en)
{
	int ret = 0;
	int win_idx;
	struct decon_mode_info psr;
	struct decon_device *decon = decon_drvdata[0];
	unsigned long aclk_khz = 0;

	decon_info("%s:state %d: out_type %d:+\n", __func__,
				tui_en, decon->dt.out_type);
	if (tui_en) {
		decon_hiber_block_exit(decon);

		kthread_flush_worker(&decon->up.worker);

		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		dpu_set_win_update_config(decon, NULL);
		decon_to_psr_info(decon, &psr);
		decon_reg_stop(decon->id, decon->dt.out_idx[0], &psr, false,
				decon->lcd_info->fps);

		decon->cur_using_dpp = 0;
		decon_dpp_stop(decon, false);

		/* after stopping decon, we can now update registers
		 * without considering per frame condition (8895) */
		for (win_idx = 0; win_idx < decon->dt.max_win; win_idx++)
			decon_reg_set_win_enable(decon->id, win_idx, false);
		decon_reg_all_win_shadow_update_req(decon->id);
		decon_reg_update_req_global(decon->id);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

		decon->state = DECON_STATE_TUI;
		aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				EXYNOS_DPU_GET_ACLK, NULL) / 1000U;
		decon_info("%s:DPU_ACLK(%ld khz)\n", __func__, aclk_khz);

#if defined(CONFIG_EXYNOS_BTS)
		decon->bts.ops->bts_acquire_bw(decon);
		decon_info("MIF(%lu), INT(%lu), DISP(%lu), total bw(%u, %u)\n",
				exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
				exynos_devfreq_get_domain_freq(DEVFREQ_INT),
				exynos_devfreq_get_domain_freq(DEVFREQ_DISP),
				decon->bts.prev_total_bw,
				decon->bts.total_bw);
#endif
	} else {
		decon->win_up.force_full = true;
		aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				EXYNOS_DPU_GET_ACLK, NULL) / 1000U;
		decon_info("%s:DPU_ACLK(%ld khz)\n", __func__, aclk_khz);
#if defined(CONFIG_EXYNOS_BTS)
		decon_info("MIF(%lu), INT(%lu), DISP(%lu), total bw(%u, %u)\n",
				exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
				exynos_devfreq_get_domain_freq(DEVFREQ_INT),
				exynos_devfreq_get_domain_freq(DEVFREQ_DISP),
				decon->bts.prev_total_bw,
				decon->bts.total_bw);
#endif
		decon->state = DECON_STATE_ON;
		decon_hiber_unblock(decon);
	}
	decon_info("%s:state %d: out_type %d:-\n", __func__,
				tui_en, decon->dt.out_type);
	return ret;
}

int decon_tui_protection(bool tui_en)
{
	int ret;
	struct decon_device *decon = decon_drvdata[0];

	mutex_lock(&decon->lock);
	ret = _decon_tui_protection(tui_en);
	mutex_unlock(&decon->lock);

	return ret;
}

int decon_set_out_sd_state(struct decon_device *decon, enum decon_state state)
{
	int i, ret = 0;
	int num_dsi = (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) ? 2 : 1;
	enum decon_state prev_state = decon->state;

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (decon->dt.out_type == DECON_OUT_DP)
		decon_displayport_set_cur_sst_id(decon->id);
#endif

	for (i = 0; i < num_dsi; i++) {
		decon_dbg("decon-%d state:%s -> %s, set dsi-%d\n", decon->id,
				decon_state_names[prev_state],
				decon_state_names[state], i);
		if (state == DECON_STATE_OFF) {
			ret = v4l2_subdev_call(decon->out_sd[i], video, s_stream, 0);
			if (ret) {
				decon_err("stopping stream failed for %s\n",
						decon->out_sd[i]->name);
				goto err;
			}
#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
		} else if (state == DECON_STATE_DOZE ||
				state == DECON_STATE_DOZE_WAKE) {
#else
		} else if (state == DECON_STATE_DOZE) {
#endif
			ret = v4l2_subdev_call(decon->out_sd[i], core, ioctl,
					DSIM_IOC_DOZE, NULL);
			if (ret < 0) {
				decon_err("decon-%d failed to set %s (ret %d)\n",
						decon->id,
						decon_state_names[state], ret);
				goto err;
			}
		} else if (state == DECON_STATE_ON) {
			if (prev_state == DECON_STATE_HIBER) {
				ret = v4l2_subdev_call(decon->out_sd[i], core, ioctl,
						DSIM_IOC_ENTER_ULPS, (unsigned long *)0);
				if (ret) {
					decon_warn("starting(ulps) stream failed for %s\n",
							decon->out_sd[i]->name);
					goto err;
				}
			} else {
				ret = v4l2_subdev_call(decon->out_sd[i], video, s_stream, 1);
				if (ret) {
					decon_err("starting stream failed for %s\n",
							decon->out_sd[i]->name);
					goto err;
				}
			}
		} else if (state == DECON_STATE_DOZE_SUSPEND) {
			ret = v4l2_subdev_call(decon->out_sd[i], core, ioctl,
					DSIM_IOC_DOZE_SUSPEND, NULL);
			if (ret < 0) {
				decon_err("decon-%d failed to set %s (ret %d)\n",
						decon->id,
						decon_state_names[state], ret);
				goto err;
			}
		} else if (state == DECON_STATE_HIBER) {
			ret = v4l2_subdev_call(decon->out_sd[i], core, ioctl,
					DSIM_IOC_ENTER_ULPS, (unsigned long *)1);
			if (ret) {
				decon_warn("stopping(ulps) stream failed for %s\n",
						decon->out_sd[i]->name);
				goto err;
			}
		}
	}

err:
	return ret;
}

#ifdef CONFIG_EXYNOS_DOZE_FIRST_FRAME_BLACK
/*
 * In case of Entering doze from power off,
 * Some DDI should display on with black image and keep it 1 frame.
 * Enter doze => trigger black => display on => black => AOD
 */
void decon_doze_first_frame_black(struct decon_device *decon,
		enum decon_state prev_state, enum decon_state next_state)
{
	struct decon_mode_info psr;

	if (decon->dt.out_type == DECON_OUT_DSI && prev_state == DECON_STATE_OFF &&
		(next_state == DECON_STATE_DOZE ||next_state == DECON_STATE_DOZE_WAKE)) {

		decon_info("%s decon-%d (%s) ++\n", __func__,
				decon->id, decon_state_names[next_state]);

		decon_to_psr_info(decon, &psr);
		decon_set_black_window(decon);
		decon_reg_start(decon->id, &psr);

		/* 1. Black frame start */
		if (decon_reg_wait_update_done_and_mask(decon->id, &psr, SHADOW_UPDATE_TIMEOUT) < 0)
			decon_err("%s: wait_for_update_timeout\n", __func__);
		/* 2. Black frame done  */
		if (decon_reg_wait_idle_status_timeout(decon->id, IDLE_WAIT_TIMEOUT) <0 )
			decon_err("%s: wait_for_idle_status_timeout\n", __func__);
		/* 3. Display on cmd tx by panel drv  */
		/* 4. Panel show black  */
		if (decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC) < 0)
			decon_err("%s: wait_for_vsync_timeout\n", __func__);

		decon_info("%s decon-%d (%s) --\n", __func__,
				decon->id, decon_state_names[next_state]);

	}
}
#endif

/* ---------- FB_BLANK INTERFACE ----------- */
int _decon_enable(struct decon_device *decon, enum decon_state state)
{
	struct decon_mode_info psr;
	struct decon_param p;
	int ret = 0;

	if (IS_DECON_ON_STATE(decon)) {
		decon_warn("%s decon-%d already on(%s) state\n", __func__,
				decon->id, decon_state_names[decon->state]);
		ret = decon_set_out_sd_state(decon, state);
		if (ret < 0) {
			decon_err("%s decon-%d failed to set subdev %s state\n",
					__func__, decon->id, decon_state_names[state]);
			return ret;
		}
		decon->state = state;
		return 0;
	}

	pm_stay_awake(decon->dev);
	dev_warn(decon->dev, "pm_stay_awake");

#if defined(CONFIG_EXYNOS_BTS)
	decon->bts.ops->bts_acquire_bw(decon);
#endif

	if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
		if (decon->res.pinctrl && decon->res.hw_te_on) {
			if (pinctrl_select_state(decon->res.pinctrl,
						decon->res.hw_te_on)) {
				decon_err("failed to turn on Decon_TE\n");
			}
		}
	}

	ret = decon_set_out_sd_state(decon, state);
	if (ret < 0) {
		decon_err("%s decon-%d failed to set subdev %s state\n",
				__func__, decon->id, decon_state_names[state]);
	}

	decon_to_init_param(decon, &p);
	decon_reg_init(decon->id, decon->dt.out_idx[0], &p);

	decon_to_psr_info(decon, &psr);

#if 0
	if ((decon->dt.out_type == DECON_OUT_DSI) &&
#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
			(state != DECON_STATE_DOZE_WAKE) &&
#endif
			(state != DECON_STATE_DOZE)) {
		if (psr.trig_mode == DECON_HW_TRIG) {
			decon_set_black_window(decon);
			/*
			 * Blender configuration must be set before DECON start.
			 * If DECON goes to start without window and
			 * blender configuration,
			 * DECON will go into abnormal state.
			 * DECON2(for DISPLAYPORT) start in winconfig
			 */
			decon_reg_start(decon->id, &psr);
		}
	}
#endif
	/*
	 * After turned on LCD, previous update region must be set as FULL size.
	 * DECON, DSIM and Panel are initialized as FULL size during UNBLANK
	 */
 	if (decon->dt.out_type == DECON_OUT_DSI && decon->state == DECON_STATE_OFF)
		DPU_FULL_RECT(&decon->win_up.prev_up_region, decon->lcd_info);

	if ((decon->dt.psr_mode == DECON_MIPI_COMMAND_MODE) &&
		(decon->dt.trig_mode == DECON_HW_TRIG) && !decon->eint_status) {
		enable_irq(decon->res.irq);
		decon->eint_status = 1;
	}

	decon->state = state;
	decon_reg_set_int(decon->id, &psr, 1);

#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_dqe_enable(decon);
#endif

	return ret;
}

static int decon_enable(struct decon_device *decon)
{
	int ret = 0, retry = 3;
	enum decon_state prev_state = decon->state;
	enum decon_state next_state = DECON_STATE_ON;

	mutex_lock(&decon->lock);
	if (decon->state == next_state) {
		decon_warn("decon-%d %s already %s state\n", decon->id,
				__func__, decon_state_names[decon->state]);
		goto out;
	}

retry_enable:
	DPU_EVENT_LOG(DPU_EVT_UNBLANK, &decon->sd, ktime_set(0, 0));
	decon_info("decon-%d %s +\n", decon->id, __func__);

	decon->wc_idx = 0;

	ret = _decon_enable(decon, next_state);
	if (ret < 0) {
		decon_err("decon-%d failed to set %s (ret %d)\n",
				decon->id, decon_state_names[next_state], ret);
		if (prev_state == DECON_STATE_OFF ||
			prev_state == DECON_STATE_DOZE_SUSPEND)
			_decon_disable(decon, prev_state);

		if (--retry >= 0 && ret == -EAGAIN) {
			decon_err("decon-%d retry set %s (remained cnt:%d)\n",
					decon->id, decon_state_names[next_state], retry);
			goto retry_enable;
		}

		goto out;
	}
	decon_info("decon-%d %s - (state:%s -> %s)\n", decon->id, __func__,
			decon_state_names[prev_state],
			decon_state_names[decon->state]);

out:
	mutex_unlock(&decon->lock);
	return ret;
};

static int decon_doze(struct decon_device *decon)
{
	int ret = 0, retry = 3;
	enum decon_state prev_state = decon->state;
	enum decon_state next_state = DECON_STATE_DOZE;

	mutex_lock(&decon->lock);
	if (decon->state == next_state) {
		decon_warn("decon-%d %s already %s state\n", decon->id,
				__func__, decon_state_names[decon->state]);
		goto out;
	}

	DPU_EVENT_LOG(DPU_EVT_DOZE, &decon->sd, ktime_set(0, 0));

	decon_info("%s: remain frame: %d", __func__, atomic_read(&decon->up.remaining_frame));
	kthread_flush_worker(&decon->up.worker);
	decon_info("%s: kthread_flush done", __func__);

retry_enable:
	decon_info("decon-%d %s +\n", decon->id, __func__);
	decon->wc_idx = 0;

	ret = _decon_enable(decon, next_state);
	if (ret < 0) {
		decon_err("decon-%d failed to set %s (ret %d)\n",
				decon->id, decon_state_names[next_state], ret);
		if (prev_state == DECON_STATE_OFF ||
			prev_state == DECON_STATE_DOZE_SUSPEND)
			_decon_disable(decon, prev_state);

		if (--retry >= 0 && ret == -EAGAIN) {
			decon_err("decon-%d retry set %s (remained cnt:%d)\n",
					decon->id, decon_state_names[next_state], retry);
			goto retry_enable;
		}
		goto out;
	}
	decon_info("decon-%d %s - (state:%s -> %s)\n", decon->id, __func__,
			decon_state_names[prev_state],
			decon_state_names[decon->state]);

#ifdef CONFIG_EXYNOS_DOZE_FIRST_FRAME_BLACK
	decon_doze_first_frame_black(decon, prev_state, next_state);
#endif
out:
	mutex_unlock(&decon->lock);
	return ret;
}

#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
int decon_doze_wake(struct decon_device *decon)
{
	int ret = 0;
	enum decon_state prev_state;
	enum decon_state next_state = DECON_STATE_DOZE_WAKE;

	mutex_lock(&decon->lock);
	if (decon->state == next_state) {
		decon_warn("decon-%d %s already %s state\n", decon->id,
				__func__, decon_state_names[decon->state]);
		goto out;
	}

	prev_state = decon->state;
	DPU_EVENT_LOG(DPU_EVT_DOZE_WAKE, &decon->sd, ktime_set(0, 0));
	decon_info("decon-%d %s +\n", decon->id, __func__);
	ret = _decon_enable(decon, next_state);
	if (ret < 0) {
		decon_err("decon-%d failed to set %s (ret %d)\n",
				decon->id, decon_state_names[next_state], ret);
		goto out;
	}
	decon_info("decon-%d %s - (state:%s -> %s)\n", decon->id, __func__,
			decon_state_names[prev_state], decon_state_names[decon->state]);

out:
	mutex_unlock(&decon->lock);
	return ret;
}
#endif

int cmu_dpu_dump(void)
{
	void __iomem	*cmu_regs;
	void __iomem	*pmu_regs;

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12800100 ===\n");
	cmu_regs = ioremap(0x12800100, 0x10);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x0C, false);

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12800800 ===\n");
	cmu_regs = ioremap(0x12800800, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x04, false);

	cmu_regs = ioremap(0x12800810, 0x10);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x08, false);

	decon_info("\n=== CMUdd_DPU0 SFR DUMP 0x12801800 ===\n");
	cmu_regs = ioremap(0x12801808, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x04, false);

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12802000 ===\n");
	cmu_regs = ioremap(0x12802000, 0x74);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x70, false);

	cmu_regs = ioremap(0x1280207c, 0x100);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x94, false);

	decon_info("\n=== CMU_DPU0 SFR DUMP 0x12803000 ===\n");
	cmu_regs = ioremap(0x12803004, 0x10);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x0c, false);

	cmu_regs = ioremap(0x12803014, 0x2C);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x28, false);

	cmu_regs = ioremap(0x1280304c, 0x20);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			cmu_regs, 0x18, false);

	decon_info("\n=== PMU_DPU0 SFR DUMP 0x16484064 ===\n");
	pmu_regs = ioremap(0x16484064, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			pmu_regs, 0x04, false);

	decon_info("\n=== PMU_DPU1 SFR DUMP 0x16484084 ===\n");
	pmu_regs = ioremap(0x16484084, 0x08);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			pmu_regs, 0x04, false);

	return 0;
}

int _decon_disable(struct decon_device *decon, enum decon_state state)
{
	struct decon_mode_info psr;
	int ret = 0;

	if (IS_DECON_OFF_STATE(decon)) {
		decon_warn("%s decon-%d already off (%s)\n", __func__,
				decon->id, decon_state_names[decon->state]);
		ret = decon_set_out_sd_state(decon, state);
		if (ret < 0) {
			decon_err("%s decon-%d failed to set subdev %s state\n",
					__func__, decon->id,
					decon_state_names[state]);
			return ret;
		}
		decon->state = state;
		return 0;
	}

	if (atomic_read(&decon->up.remaining_frame))
		kthread_flush_worker(&decon->up.worker);

	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 0);

	if (!decon->id && (decon->vsync.irq_refcount <= 0) &&
			decon->eint_status) {
		disable_irq(decon->res.irq);
		decon->eint_status = 0;
	}

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	/* for remove under flow interrupt log when decon disable */
	if (decon->dt.out_type == DECON_OUT_DP)
		decon_displayport_under_flow_int_mask(decon->id);
#endif

#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_dqe_disable(decon);
#endif
	ret = decon_reg_stop(decon->id, decon->dt.out_idx[0], &psr, true,
			decon->lcd_info->fps);
	if (ret < 0)
		decon_dump(decon, true);

	/* DMA protection disable must be happen on dpp domain is alive */
	if (decon->dt.out_type != DECON_OUT_WB) {
#if defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
		decon_set_protected_content(decon, NULL);
#endif
		decon->cur_using_dpp = 0;
		decon->cur_req_win = 0;
		decon_dpp_stop(decon, false);
	}

#if defined(CONFIG_EXYNOS_BTS)
	decon->bts.ops->bts_release_bw(decon);
#endif

	ret = decon_set_out_sd_state(decon, state);
	if (ret < 0) {
		decon_err("%s decon-%d failed to set subdev %s state\n",
				__func__, decon->id, decon_state_names[state]);
	}

	pm_relax(decon->dev);
	dev_warn(decon->dev, "pm_relax");

	if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
		if (decon->res.pinctrl && decon->res.hw_te_off) {
			if (pinctrl_select_state(decon->res.pinctrl,
						decon->res.hw_te_off)) {
				decon_err("failed to turn off Decon_TE\n");
			}
		}
	}

	decon->state = state;

#if defined(CONFIG_EXYNOS_PD)
	if (decon->pm_domain) {
		if (dpu_pm_domain_check_status(decon->pm_domain)) {
			decon_info("decon%d %s still on\n", decon->id,
				decon->dt.pd_name);
			/* TODO : saimple dma/decon logging in cal code */
		} else
			decon_info("decon%d %s off\n", decon->id,
				decon->dt.pd_name);
	}
#endif

	return ret;
}

static int decon_disable(struct decon_device *decon)
{
	int ret = 0;
	enum decon_state prev_state = decon->state;
	enum decon_state next_state = DECON_STATE_OFF;

	mutex_lock(&decon->lock);
	if (decon->state == next_state) {
		decon_warn("decon-%d %s already %s state\n", decon->id,
				__func__, decon_state_names[decon->state]);
		goto out;
	}

	if (decon->state == DECON_STATE_TUI)
		_decon_tui_protection(false);

	DPU_EVENT_LOG(DPU_EVT_BLANK, &decon->sd, ktime_set(0, 0));
	decon_info("decon-%d %s +\n", decon->id, __func__);
	ret = _decon_disable(decon, next_state);
	if (ret < 0) {
		decon_err("decon-%d failed to set %s (ret %d)\n",
				decon->id, decon_state_names[next_state], ret);
		goto out;
	}
	decon_info("decon-%d %s - (state:%s -> %s)\n", decon->id, __func__,
			decon_state_names[prev_state],
			decon_state_names[decon->state]);

out:
	mutex_unlock(&decon->lock);
	return ret;
}

int decon_doze_suspend(struct decon_device *decon)
{
	int ret = 0;
	enum decon_state prev_state = decon->state;
	enum decon_state next_state = DECON_STATE_DOZE_SUSPEND;

	mutex_lock(&decon->lock);
	if (decon->state == next_state) {
		decon_warn("decon-%d %s already %s state\n", decon->id,
				__func__, decon_state_names[decon->state]);
		goto out;
	}

	DPU_EVENT_LOG(DPU_EVT_DOZE_SUSPEND, &decon->sd, ktime_set(0, 0));
	decon_info("decon-%d %s +\n", decon->id, __func__);
	ret = _decon_disable(decon, next_state);
	if (ret < 0) {
		decon_err("decon-%d failed to set %s (ret %d)\n",
				decon->id, decon_state_names[next_state], ret);
		goto out;
	}
	decon_info("decon-%d %s - (state:%s -> %s)\n", decon->id, __func__,
			decon_state_names[prev_state],
			decon_state_names[decon->state]);

#ifdef CONFIG_EXYNOS_DOZE_FIRST_FRAME_BLACK
	decon_doze_first_frame_black(decon, prev_state, next_state);
#endif
out:
	mutex_unlock(&decon->lock);
	return ret;
}

struct disp_pwr_state decon_pwr_state[] = {
	[DISP_PWR_OFF] = {
		.state = DECON_STATE_OFF,
		.set_pwr_state = (set_pwr_state_t)decon_disable,
	},
	[DISP_PWR_DOZE] = {
		.state = DECON_STATE_DOZE,
		.set_pwr_state = (set_pwr_state_t)decon_doze,
	},
	[DISP_PWR_NORMAL] = {
		.state = DECON_STATE_ON,
		.set_pwr_state = (set_pwr_state_t)decon_enable,
	},
#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
	[DISP_PWR_DOZE_SUSPEND] = {
		.state = DECON_STATE_DOZE_SUSPEND,
		.set_pwr_state = (set_pwr_state_t)decon_doze_wake,
	},
#else
	[DISP_PWR_DOZE_SUSPEND] = {
		.state = DECON_STATE_DOZE_SUSPEND,
		.set_pwr_state = (set_pwr_state_t)decon_doze_suspend,
	},
#endif
};

int decon_update_pwr_state(struct decon_device *decon, enum disp_pwr_mode mode)
{
	int ret = 0;

	if (mode >= DISP_PWR_MAX) {
		decon_err("DECON:ERR:%s:invalid mode : %d\n", __func__, mode);
		return -EINVAL;
	}

	mutex_lock(&decon->pwr_state_lock);
	if (decon_pwr_state[mode].state == decon->state) {
		decon_warn("decon-%d already %s state\n",
				decon->id, decon_state_names[decon->state]);
		goto out;
	}

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (mode == DISP_PWR_NORMAL && decon->dt.out_type == DECON_OUT_DP) {
		struct decon_device *decon0 = get_decon_drvdata(0);
		const int max_wait = 100;
		int wait_cnt = 0;

		if (decon0) {
			while (decon0->state == DECON_STATE_TUI && wait_cnt++ < max_wait)
				msleep(20);

			if (wait_cnt >= max_wait) {
				decon_err("Displayport: tui close timeout\n");
				goto out;
			} else if (wait_cnt) {
				decon_warn("Displayport: tui close wait(%dms)\n", wait_cnt * 20);
			}
		}
	}
	if (mode == DISP_PWR_OFF && decon->dt.out_type == DECON_OUT_DP
			&& IS_DISPLAYPORT_HPD_PLUG_STATE()) {
		if (IS_DISPLAYPORT_SST_HPD_PLUG_STATE(displayport_get_sst_id_with_decon_id(decon->id))) {
			decon_info("skip decon-%d disable(hpd plug)\n", decon->id);
			goto out;
		}
	}
#endif

	if (IS_DECON_OFF_STATE(decon)) {
		if (mode == DISP_PWR_OFF) {
			ret = decon_enable(decon);
			if (ret < 0) {
				decon_err("DECON:ERR:%s: failed to set mode(%d)\n",
						__func__, DISP_PWR_NORMAL);
				goto out;
			}
		} else if (mode == DISP_PWR_DOZE_SUSPEND) {
			ret = decon_doze(decon);
			if (ret < 0) {
				decon_err("DECON:ERR:%s: failed to set mode(%d)\n",
						__func__, DISP_PWR_DOZE);
				goto out;
			}
		}
	}

	ret = decon_pwr_state[mode].set_pwr_state(decon);
	if (ret < 0) {
		decon_err("DECON:ERR:%s: failed to set mode(%d)\n",
				__func__, mode);
		goto out;
	}

#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
	if (mode == DISP_PWR_DOZE_SUSPEND) {
		decon->doze_hiber.doze_suspend_timestamp = ktime_get();
		wake_up_interruptible_all(&decon->doze_hiber.doze_suspend_wait);
	}
#endif

out:
	mutex_unlock(&decon->pwr_state_lock);

	return ret;
}

static int decon_dp_disable(struct decon_device *decon)
{
	struct decon_mode_info psr;
	int ret = 0;

	decon_info("disable decon displayport\n");

	mutex_lock(&decon->lock);

	if (IS_DECON_OFF_STATE(decon)) {
		decon_info("decon%d already disabled\n", decon->id);
		goto err;
	}

	kthread_flush_worker(&decon->up.worker);

	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 0);
	ret = decon_reg_stop(decon->id, decon->dt.out_idx[0], &psr, true,
			decon->lcd_info->fps);
	if (ret < 0)
		decon_dump(decon, false);

	/* DMA protection disable must be happen on dpp domain is alive */
	if (decon->dt.out_type != DECON_OUT_WB) {
#if defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
		decon_set_protected_content(decon, NULL);
#endif
		decon->cur_using_dpp = 0;
		decon->cur_req_win = 0;
		decon_dpp_stop(decon, false);
	}

#if defined(CONFIG_EXYNOS_BTS)
	decon->bts.ops->bts_release_bw(decon);
#endif

	decon->state = DECON_STATE_OFF;
err:
	mutex_unlock(&decon->lock);
	return ret;
}

static int decon_blank(int blank_mode, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	int ret = 0;

	decon_info("decon-%d %s mode: %d type (0: DSI, 1: eDP, 2:DP, 3: WB)\n",
			decon->id,
			blank_mode == FB_BLANK_UNBLANK ? "UNBLANK" : "POWERDOWN",
			decon->dt.out_type);

	if (IS_ENABLED(CONFIG_EXYNOS_VIRTUAL_DISPLAY)) {
		decon_info("decon%d virtual display mode\n", decon->id);
		return 0;
	}

	decon_hiber_block_exit(decon);
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	mutex_lock(&decon->esd.lock);
#endif
	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		DPU_EVENT_LOG(DPU_EVT_BLANK, &decon->sd, ktime_set(0, 0));
		ret = decon_update_pwr_state(decon, DISP_PWR_OFF);
		if (ret) {
			decon_err("failed to disable decon\n");
			goto blank_exit;
		}
		lcd_status_notifier(LCD_OFF);
		break;
	case FB_BLANK_UNBLANK:
		DPU_EVENT_LOG(DPU_EVT_UNBLANK, &decon->sd, ktime_set(0, 0));
		lcd_status_notifier(LCD_ON);
		ret = decon_update_pwr_state(decon, DISP_PWR_NORMAL);
		if (ret) {
			decon_err("failed to enable decon\n");
			goto blank_exit;
		}
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
		if (decon->esd.thread)
			wake_up_process(decon->esd.thread);
#endif
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}

blank_exit:
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	mutex_unlock(&decon->esd.lock);
#endif
	decon_hiber_unblock(decon);
	decon_info("%s -\n", __func__);
	return ret;
}

/* ---------- FB_IOCTL INTERFACE ----------- */
static void decon_activate_vsync(struct decon_device *decon)
{
	int prev_refcount;

	mutex_lock(&decon->vsync.lock);

	prev_refcount = decon->vsync.irq_refcount++;
	if (!prev_refcount)
		DPU_EVENT_LOG(DPU_EVT_ACT_VSYNC, &decon->sd, ktime_set(0, 0));

	mutex_unlock(&decon->vsync.lock);
}

static void decon_deactivate_vsync(struct decon_device *decon)
{
	int new_refcount;

	mutex_lock(&decon->vsync.lock);

	new_refcount = --decon->vsync.irq_refcount;
	WARN_ON(new_refcount < 0);
	if (!new_refcount)
		DPU_EVENT_LOG(DPU_EVT_DEACT_VSYNC, &decon->sd, ktime_set(0, 0));

	mutex_unlock(&decon->vsync.lock);
}

int decon_wait_for_vsync(struct decon_device *decon, u32 timeout)
{
	ktime_t timestamp;
	struct decon_mode_info psr;
	int ret;

	decon_to_psr_info(decon, &psr);

#if defined(CONFIG_EXYNOS_COMMON_PANEL) || \
	defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	if (decon_is_bypass(decon))
		return 0;
#endif

	if (psr.trig_mode != DECON_HW_TRIG)
		return 0;

	timestamp = decon->vsync.timestamp;
	decon_activate_vsync(decon);

	if (timeout) {
		ret = wait_event_interruptible_timeout(decon->vsync.wait,
				timestamp != decon->vsync.timestamp,
				msecs_to_jiffies(timeout));
	} else {
		ret = wait_event_interruptible(decon->vsync.wait,
				timestamp != decon->vsync.timestamp);
	}

	decon_deactivate_vsync(decon);

	if (timeout && ret == 0) {
		if (decon->d.eint_pend) {
#ifdef CONFIG_LOGGING_BIGDATA_BUG
			decon->eint_pend = readl(decon->d.eint_pend);
			decon_err("decon%d wait for vsync timeout(p:0x%x)\n",
				decon->id, decon->eint_pend);
#else
			decon_err("decon%d wait for vsync timeout(p:0x%x)\n",
				decon->id, readl(decon->d.eint_pend));
#endif
		} else {
			decon_err("decon%d wait for vsync timeout\n", decon->id);
		}

		return -ETIMEDOUT;
	}

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
		if (decon->profile_sd != NULL) {
			v4l2_subdev_call(decon->profile_sd, core, ioctl,
				PROFILE_TE, (void *)ktime_to_us(decon->vsync.timestamp));
		}
#endif

	return 0;
}

#if defined(CONFIG_DECON_VRR_MODULATION)
/*
 * decon_wait_for_active_region()
 * This function is intended to wait until trigger mask safe region.
 */
int decon_wait_for_active_region(struct decon_device *decon, u32 timeout)
{
	int ret;
	u64 count, div_count;

	/*
	 * TODO : * this is critical section, so it should be synchronized.
	 * error case)
	 * initial state: vsync.count = 1, vsync.div_count = 2
	 * if isr updates 'vsync.count++' right after 'count' is assigned,
	 * (count = 1, vsync.count = 2), routine won't wait vsync,
	 * but decon_vsync_thread() will notify "vsync" to hw composer.
	 *
	 * then panel re-sync according to frame(2C command) at next vsync.
	 * during this time, vsync will be occurred as 120hz.
	 */

	/*
	 * wait for n-vsync times.
	 * n-vsync : max(0, aligned(vsync.count - vsync.div_count) - 1)
	 */
	div_count = IS_DECON_DOZE_STATE(decon) ? 1ULL : decon->vsync.div_count;
	for (count = (decon->vsync.count % div_count) + 1;
			count < div_count; count++) {
		ret = decon_wait_for_vsync(decon, timeout);
		if (ret < 0) {
			decon_err("decon%d wait for active region among (%llu)-vsync\n",
					decon->id, div_count);
			return ret;
		}
	}

	return 0;
}
#endif

static int decon_find_biggest_block_rect(struct decon_device *decon,
		int win_no, struct decon_win_config *win_config,
		struct decon_rect *block_rect, bool *enabled)
{
	struct decon_rect r1, r2, overlap_rect;
	unsigned int overlap_size = 0, blocking_size = 0;
	struct decon_win_config *config;
	const struct dpu_fmt *fmt_info;
	int j;

	/* Get the rect in which we try to get the block region */
	config = &win_config[win_no];
	r1.left = config->dst.x;
	r1.top = config->dst.y;
	r1.right = r1.left + config->dst.w - 1;
	r1.bottom = r1.top + config->dst.h - 1;

	/* Find the biggest block region from overlays by the top windows */
	for (j = win_no + 1; j < decon->dt.max_win; j++) {
		config = &win_config[j];
		if (config->state != DECON_WIN_STATE_BUFFER)
			continue;

		/* If top window has plane alpha, blocking mode not appliable */
		if ((config->plane_alpha < 255) && (config->plane_alpha >= 0))
			continue;

		fmt_info = dpu_find_fmt_info(config->format);
		if (IS_RGB32(fmt_info) && IS_OPAQUE(fmt_info)) {
			config->opaque_area.x = config->dst.x;
			config->opaque_area.y = config->dst.y;
			config->opaque_area.w = config->dst.w;
			config->opaque_area.h = config->dst.h;
		} else
			continue;

		r2.left = config->opaque_area.x;
		r2.top = config->opaque_area.y;
		r2.right = r2.left + config->opaque_area.w - 1;
		r2.bottom = r2.top + config->opaque_area.h - 1;
		/* overlaps or not */
		if (decon_intersect(&r1, &r2)) {
			decon_intersection(&r1, &r2, &overlap_rect);
			if (!is_decon_rect_differ(&r1, &overlap_rect)) {
				/* if overlaping area intersects the window
				 * completely then disable the window */
				win_config[win_no].state = DECON_WIN_STATE_DISABLED;
				return 1;
			}

			if (overlap_rect.right - overlap_rect.left + 1 <
					MIN_BLK_MODE_WIDTH ||
				overlap_rect.bottom - overlap_rect.top + 1 <
					MIN_BLK_MODE_HEIGHT)
				continue;

			overlap_size = (overlap_rect.right - overlap_rect.left) *
					(overlap_rect.bottom - overlap_rect.top);

			if (overlap_size > blocking_size) {
				memcpy(block_rect, &overlap_rect,
						sizeof(struct decon_rect));
				blocking_size =
					(block_rect->right - block_rect->left) *
					(block_rect->bottom - block_rect->top);
				*enabled = true;
			}
		}
	}

	return 0;
}

static int decon_set_win_blocking_mode(struct decon_device *decon,
		int win_no, struct decon_win_config *win_config,
		struct decon_reg_data *regs)
{
	struct decon_rect block_rect;
	bool enabled;
	int ret = 0;
	struct decon_win_config *config = &win_config[win_no];
	const struct dpu_fmt *fmt_info;

	enabled = false;

	if (!IS_ENABLED(CONFIG_EXYNOS_BLOCK_MODE))
		return ret;

	if (config->state != DECON_WIN_STATE_BUFFER)
		return ret;

	if (config->compression)
		return ret;

	/* Blocking mode is supported only for RGB32 color formats */
	fmt_info = dpu_find_fmt_info(config->format);
	if (!IS_RGB32(fmt_info))
		return ret;

	/* Blocking Mode is not supported if there is a rotation */
	if (config->dpp_parm.rot || is_scaling(config))
		return ret;

	/* Initialization */
	memset(&block_rect, 0, sizeof(struct decon_rect));

	/* Find the biggest block region from possible block regions
	 * 	Possible block regions
	 * 	- overlays by top windows
	 *
	 * returns :
	 * 	1  - corresponding window is blocked whole way,
	 * 	     meaning that the window could be disabled
	 *
	 * 	0, enabled = true  - blocking area has been found
	 * 	0, enabled = false - blocking area has not been found
	 */
	ret = decon_find_biggest_block_rect(decon, win_no, win_config,
						&block_rect, &enabled);
	if (ret)
		return ret;

	/* If there was a block region, set regs with results */
	if (enabled) {
		regs->block_rect[win_no].w = block_rect.right - block_rect.left + 1;
		regs->block_rect[win_no].h = block_rect.bottom - block_rect.top + 1;
		regs->block_rect[win_no].x = block_rect.left - config->dst.x;
		regs->block_rect[win_no].y = block_rect.top -  config->dst.y;
		decon_dbg("win-%d: block_rect[%d %d %d %d]\n", win_no,
			regs->block_rect[win_no].x, regs->block_rect[win_no].y,
			regs->block_rect[win_no].w, regs->block_rect[win_no].h);
		memcpy(&config->block_area, &regs->block_rect[win_no],
				sizeof(struct decon_win_rect));
	}

	return ret;
}

int decon_set_vsync_int(struct fb_info *info, bool active)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	bool prev_active = decon->vsync.active;

	decon->vsync.active = active;
	smp_wmb();

	if (active && !prev_active)
		decon_activate_vsync(decon);
	else if (!active && prev_active)
		decon_deactivate_vsync(decon);

	return 0;
}

#ifdef DEBUG_DMA_BUF_LEAK

static void logging_dma_leak_info(struct decon_device *decon,
	u32 type, int import_cnt, ktime_t import_time)
{
	int pos;
	struct dma_leak_info *leak_info;

	pos = decon->leak_cnt % LEAK_INFO_ARRAY_CNT;
	leak_info = &decon->leak_info[pos];

	leak_info->type = type;
	leak_info->import_cnt = import_cnt;
	leak_info->import_time = import_time;
	leak_info->free_cnt = decon->free_cnt;
	leak_info->free_time = ktime_get();

	decon->leak_cnt++;
}

static void print_dma_leak_info(struct decon_device *decon)
{
	int i;
	int pos;
	int printcnt;
	struct dma_leak_info *leak_info;

	dpu_show_readback_buf_info(decon, 0);

	decon_info("[DECON:INFO]: leak count : %d\n", decon->leak_cnt);

	pos = (decon->leak_cnt % LEAK_INFO_ARRAY_CNT);

	printcnt = decon->leak_cnt;
	if (decon->leak_cnt > LEAK_INFO_ARRAY_CNT)
		printcnt = LEAK_INFO_ARRAY_CNT;

	for (i = 0; i < printcnt; i++) {
		pos = pos - 1;
		if (pos < 0)
			pos = LEAK_INFO_ARRAY_CNT + pos;

		if ((pos < LEAK_INFO_ARRAY_CNT) && (pos >= 0)) {
			leak_info = &decon->leak_info[pos];
			decon_info("[DECON:INFO]: %d, type: %d, time import: %lld, free:%lld, count import:%d, free cnt:%d\n",
				pos, leak_info->type, ktime_to_us(leak_info->import_time), ktime_to_us(leak_info->free_time),
				leak_info->import_cnt, leak_info->free_cnt);
		}
	}
}
#endif

#if defined(CONFIG_EXYNOS_IOVMM)
static inline struct exynos_iovmm *__exynos_get_iovmm(struct device *dev)
{
	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: System MMU is not configured\n", __func__);
		return NULL;
	}

	return ((struct exynos_iommu_owner *)dev->archdata.iommu)->vmm_data;
}
#endif

static unsigned int decon_map_ion_handle(struct decon_device *decon,
		struct device *dev, struct decon_dma_buf_data *dma,
		struct dma_buf *buf, int win_no)
{
//debug DMA_BUF
	struct dma_buf_attachment *attach_obj;
	struct dma_buf *dma_buf;
	int attach_count = 0;
//for debug
#if defined(CONFIG_EXYNOS_IOVMM)
	struct exynos_iovmm *vmm;
#endif

	dma->fence = NULL;
	dma->dma_buf = buf;

	if (IS_ERR_OR_NULL(dev)) {
		decon_err("%s: dev ptr is invalid\n", __func__);
		goto err_buf_map_attach;
	}

#ifdef DEBUG_DMA_BUF_LEAK
	dma_buf = dma->dma_buf;
	mutex_lock(&dma_buf->lock);
	list_for_each_entry(attach_obj, &dma_buf->attachments, node) {
		if ((attach_obj != NULL) && (attach_obj->dev != NULL)) {
			if (strcmp(dev_name(attach_obj->dev), dev_name(dev)) == 0)
				attach_count++;

		}
	}
	mutex_unlock(&dma_buf->lock);

	if (attach_count > 50) {
		decon_info("attached count : %d\n", attach_count);
		if (attach_count > 100) {
			decon_info("leak cnt : %d\n", decon->leak_cnt);
			print_dma_leak_info(decon);
			BUG();
		}
	}
#endif

	dpu_show_readback_buf_info(decon, 5);

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		decon_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}
	decon->d.buf_cnt.buf_attach_cnt++;

#if defined(CONFIG_EXYNOS_IOVMM)
	vmm = __exynos_get_iovmm(dma->attachment->dev);
	if (vmm != NULL) {
		if (vmm->num_map > vmm->num_unmap) {
			if ((vmm->num_map - vmm->num_unmap) > 200)
				decon_info("%d : vmm num_map : %d, unnum_map: %ld\n",
					(vmm->num_map - vmm->num_unmap), vmm->num_map, vmm->num_unmap);
		}
	}
#endif

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		decon_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}
	decon->d.buf_cnt.map_attach_cnt++;

	/* This is DVA(Device Virtual Address) for setting base address SFR */
	dma->dma_addr = ion_iovmm_map(dma->attachment, 0, dma->dma_buf->size,
				      DMA_TO_DEVICE, 0);
	if (IS_ERR_VALUE(dma->dma_addr)) {
		decon_err("ion_iovmm_map() failed: %pa\n", &dma->dma_addr);
		decon_err("remaining_frame : %d", atomic_read(&decon->up.remaining_frame));

		decon_info("attached count : %d\n", attach_count);
		decon_info("leak cnt : %d\n", decon->leak_cnt);
		print_dma_leak_info(decon);

		BUG();
		goto err_iovmm_map;
	}
	decon->d.buf_cnt.iovmm_map_cnt++;

	DPU_EVENT_LOG_MEMMAP(DPU_EVT_MEM_MAP, &decon->sd, dma->dma_addr,
			dma->dpp_ch, dma->sg_table);

	return dma->dma_buf->size;

err_iovmm_map:
err_buf_map_attachment:
err_buf_map_attach:
	dpu_show_dma_attach_info("decon_map_ion_handle", decon, 0);
	/* save first fail */
	if (!decon->d.buf_cnt_bak.count) {
		memcpy(&decon->d.buf_cnt_bak, &decon->d.buf_cnt,
				sizeof(struct dpu_dma_info));
		decon->d.buf_cnt_bak.timestamp = ktime_get();
		decon->d.buf_cnt_bak.count++;
	}

	return 0;
}

static int decon_import_buffer(struct decon_device *decon, int idx,
		struct decon_win_config *config,
		struct decon_reg_data *regs)
{
	struct dma_buf *buf = NULL;
	struct decon_dma_buf_data *dma_buf_data = NULL;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct displayport_device *displayport;
#endif
	struct dsim_device *dsim;
	struct dpp_device *dpp;
	struct device *dev = NULL;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(config->format);
	int i;
	size_t buf_size = 0;

	decon_dbg("%s +\n", __func__);

	DPU_EVENT_LOG(DPU_EVT_DECON_SET_BUFFER, &decon->sd, ktime_set(0, 0));

	regs->plane_cnt[idx] = fmt_info->num_buffers;
	/* If HDR is requested, meta data is transferred to last plane buffer */
	if (config->dpp_parm.hdr_std == DPP_HDR_ST2084 ||
			config->dpp_parm.hdr_std == DPP_HDR_HLG)
		regs->plane_cnt[idx] += fmt_info->num_meta_planes;

	memset(&regs->dma_buf_data[idx], 0,
			sizeof(struct decon_dma_buf_data) * MAX_PLANE_CNT);

	for (i = 0; i < regs->plane_cnt[idx]; ++i) {
		dma_buf_data = &regs->dma_buf_data[idx][i];
		/* dma_addr in dma_buf_data structure will be used by dpp_ch */
		dma_buf_data->dpp_ch = config->channel;

		buf = dma_buf_get(config->fd_idma[i]);
		if (IS_ERR_OR_NULL(buf)) {
			decon_err("failed to get dma_buf:%ld\n", PTR_ERR(buf));
			return PTR_ERR(buf);
		}
		if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
			displayport = v4l2_get_subdevdata(decon->out_sd[0]);
			dev = displayport->dev;
#endif
		} else if (decon->dt.out_type == DECON_OUT_DSI) {
			dsim = v4l2_get_subdevdata(decon->out_sd[0]);
			dev = dsim->dev;
		} else {	/* writeback case */
			dpp = v4l2_get_subdevdata(decon->out_sd[0]);
			dev = dpp->dev;
		}

		buf_size = decon_map_ion_handle(decon, dev, dma_buf_data, buf, idx);
		if (!buf_size) {
			decon_err("failed to map buffer\n");
			return -ENOMEM;
		}
#ifdef DEBUG_DMA_BUF_LEAK
		regs->import_cnt++;
#endif
		/* DVA is passed to DPP parameters structure */
		config->dpp_parm.addr[i] = dma_buf_data->dma_addr;
	}
	decon_dbg("%s -\n", __func__);

	return 0;
}

int decon_check_limitation(struct decon_device *decon, int idx,
		struct decon_win_config *config)
{
	if (config->format >= DECON_PIXEL_FORMAT_MAX) {
		decon_err("unknown pixel format %u\n", config->format);
		return -EINVAL;
	}

	if (decon_check_supported_formats(config->format)) {
		decon_err("not supported pixel format\n");
		return -EINVAL;
	}

	if (config->blending >= DECON_BLENDING_MAX) {
		decon_err("unknown blending %u\n", config->blending);
		return -EINVAL;
	}

	if ((config->plane_alpha < 0) || (config->plane_alpha > 0xff)) {
		decon_err("plane alpha value(%d) is out of range(0~255)\n",
				config->plane_alpha);
		return -EINVAL;
	}

	if (config->dst.w == 0 || config->dst.h == 0 ||
			config->dst.x < 0 || config->dst.y < 0) {
		decon_err("win[%d] size is abnormal (w:%d, h:%d, x:%d, y:%d)\n",
				idx, config->dst.w, config->dst.h,
				config->dst.x, config->dst.y);
		return -EINVAL;
	}

	if (config->dst.w < 16) {
		decon_err("window wide < 16pixels, width = %u)\n",
				config->dst.w);
		return -EINVAL;
	}

	if ((config->dst.x + config->dst.w > config->dst.f_w) ||
		(config->dst.y + config->dst.h > config->dst.f_h)) {
		decon_err("dst coordinate is out of range(%d %d %d %d %d %d %d %d)\n",
				config->dst.x, config->dst.w, config->dst.f_w,
				config->dst.y, config->dst.h, config->dst.f_h,
				decon->lcd_info->xres, decon->lcd_info->yres);
		return -EINVAL;
	}

	/* TODO: currently writeback is not supported */
	if (config->channel >= decon->dt.dpp_cnt) { /* ch */
		decon_err("ch(%d) is wrong\n", config->channel);
		return -EINVAL;
	}

	return 0;
}

/* To avoid sysmmu page fault due to small buffer allocation */
static int decon_check_buffer_size(struct decon_device *decon,
		struct decon_win_config *config,
		struct decon_reg_data *regs, int idx,
		const struct dpu_fmt *fmt_info)
{
	int ret = 0, i;
	u32 cfg_size[3] = {0, 0, 0};
	u32 buf_size[3] = {0, 0, 0};
	u32 mpc = 1; /* memory per component : byte - default: 8bit */
	u32 hv_sub = 2; /* sub-sampling of chroma - default: YUV420 */

	if (fmt_info->cs == DPU_COLORSPACE_RGB) { /* 1-plane */
		cfg_size[0] = config->src.f_w * config->src.f_h *
			(fmt_info->bpp + fmt_info->padding); /* bits */
		cfg_size[0] = (cfg_size[0] + 7) / 8; /* bytes */

		buf_size[0] = (u32)(regs->dma_buf_data[idx][0].dma_buf->size);
	} else {
		if (fmt_info->bpp == 10)
			mpc = 2;
		/* Y : bytes */
		cfg_size[0] = config->src.f_w * config->src.f_h * mpc;
		/* C : bytes */
		if (fmt_info->cs == DPU_COLORSPACE_YUV422)
			hv_sub = 1;
		cfg_size[1] = cfg_size[0] / hv_sub;

		for (i = 0; i < fmt_info->num_buffers; i++)
			buf_size[i] = (u32)(regs->dma_buf_data[idx][i].dma_buf->size);

		/* single-fd case */
		if (fmt_info->num_buffers == 1)
			cfg_size[0] = cfg_size[0] + cfg_size[1];
	}

	for (i = 0; i < fmt_info->num_buffers; i++) {
		decon_dbg("%s[fd%d]: cfg size(0x%x B), buf size(0x%x B)\n",
			__func__, i, cfg_size[i], buf_size[i]);
		if (cfg_size[i] > buf_size[i]) {
			decon_err("[w%d:fd%d] buf size(0x%x) < required size(0x%x)\n",
				idx, i, buf_size[i], cfg_size[i]);
			ret = -EINVAL;
		}
	}

	return ret;
}

static int decon_set_win_buffer(struct decon_device *decon,
		struct decon_win_config *config,
		struct decon_reg_data *regs, int idx)
{
	int ret;
	struct decon_rect r;
	struct dma_fence *fence = NULL;
	const struct dpu_fmt *fmt_info;

	fmt_info = dpu_find_fmt_info(config->format);

	if (decon->dt.out_type == DECON_OUT_WB) {
		r.left = config->dst.x;
		r.top = config->dst.y;
		r.right = r.left + config->dst.w - 1;
		r.bottom = r.top + config->dst.h - 1;
		dpu_unify_rect(&regs->blender_bg, &r, &regs->blender_bg);
	}

	ret = decon_import_buffer(decon, idx, config, regs);
	if (ret)
		goto err;

	if (config->acq_fence >= 0) {
		/* fence is managed by buffer not plane */
		fence = sync_file_get_fence(config->acq_fence);
		regs->dma_buf_data[idx][0].fence = fence;
		if (!fence) {
			decon_err("failed to import fence fd\n");
			ret = -EINVAL;
			goto err;
		}
		decon_dbg("acq_fence(%d), fence(%p)\n", config->acq_fence, fence);
	}

	if (decon_check_buffer_size(decon, config, regs, idx, fmt_info) < 0)
		goto err;

	regs->protection[idx] = config->protection;
	if (config->protection)
		regs->readback.drm = true;
	decon_win_config_to_regs_param(fmt_info->len_alpha, config,
			&regs->win_regs[idx], config->channel, idx); /* ch */

	return 0;

err:
	return ret;
}

void decon_reg_chmap_validate(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	unsigned short i, bitmap = 0;

	for (i = 0; i < decon->dt.max_win; i++) {
		if (!(regs->win_regs[i].wincon & WIN_EN_F(i)) ||
				(regs->win_regs[i].winmap_state))
			continue;

		if (bitmap & (1 << regs->dpp_config[i].channel)) { /* ch */
			decon_warn("Channel-%d is mapped to multiple windows\n",
					regs->dpp_config[i].channel); /* ch */
			regs->win_regs[i].wincon &= (~WIN_EN_F(i));
		}
		bitmap |= 1 << regs->dpp_config[i].channel; /* ch */
	}
}

static int decon_check_rsc_conflict(struct decon_device *decon,
				unsigned long cur_dpps, unsigned long cur_wins)
{
	struct decon_device *other_decon;
	int i;

	for (i = 0; i < decon->dt.decon_cnt; ++i) {
		other_decon = get_decon_drvdata(i);
		if (other_decon->id == decon->id)
			continue;

		if (other_decon->prev_used_dpp & cur_dpps) {
			decon_err("DPP resources conflict\n");
			decon_err("\tdecon%d prev(0x%lx), decon%d cur(0x%lx)\n",
					other_decon->id, other_decon->prev_used_dpp,
					decon->id, cur_dpps);
			return -EINVAL;
		}

		if (other_decon->prev_req_win & cur_wins) {
			decon_err("WINDOW resources conflict\n");
			decon_err("\tdecon%d prev(0x%lx), decon%d cur(0x%lx)\n",
					other_decon->id, other_decon->prev_req_win,
					decon->id, cur_wins);
			return -EINVAL;
		}
	}

	return 0;
}

static int decon_check_used_dpp(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i = 0;
	unsigned long prev_dpps, prev_wins;
	unsigned long cur_dpps = 0, cur_wins = 0;

	spin_lock(&g_slock);

	prev_dpps = decon->prev_used_dpp;
	prev_wins = decon->prev_req_win;

	for (i = 0; i < decon->dt.max_win; i++) {
		struct decon_win *win = decon->win[i];
		if (!regs->win_regs[i].winmap_state)
			win->dpp_id = regs->dpp_config[i].channel; /* ch */
		else
			win->dpp_id = 0xF;

		if ((regs->win_regs[i].wincon & WIN_EN_F(i)) &&
			(!regs->win_regs[i].winmap_state)) {
			set_bit(win->dpp_id, &cur_dpps);
			set_bit(win->dpp_id, &prev_dpps);
		}

		if (regs->win_regs[i].wincon & WIN_EN_F(i)) {
			set_bit(i, &cur_wins);
			set_bit(i, &prev_wins);
		}
	}

	if (decon->dt.out_type == DECON_OUT_WB || regs->readback.request) {
		set_bit(ODMA_WB, &cur_dpps);
		set_bit(ODMA_WB, &prev_dpps);
	}

	/* check resource conflict here */
	if (decon_check_rsc_conflict(decon, cur_dpps, cur_wins)) {
		spin_unlock(&g_slock);
		return -EINVAL;
	}

	/*
	 * prev_used_dpp and prev_req_win of decon structure are NOT updated
	 * when resource conflict occurs.
	 * If not, sw and hw resource information will be mis-matched.
	 */
	decon->prev_used_dpp = prev_dpps;
	decon->cur_using_dpp = cur_dpps;
	decon->prev_req_win = prev_wins;
	decon->cur_req_win = cur_wins;

	spin_unlock(&g_slock);

	DPU_EVENT_LOG(DPU_EVT_ACQUIRE_RSC, &decon->sd, ktime_set(0, 0));
	return 0;
}

void decon_dpp_wait_wb_framedone(struct decon_device *decon)
{
	struct v4l2_subdev *sd = NULL;

	sd = decon->dpp_sd[ODMA_WB];
	v4l2_subdev_call(sd, core, ioctl,
			DPP_WB_WAIT_FOR_FRAMEDONE, NULL);

}

static int decon_set_dpp_config(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i, ret = 0, err_cnt = 0;
	struct v4l2_subdev *sd;
	struct decon_win *win;
	struct dpp_config dpp_config;
	unsigned long aclk_khz;

#ifdef CONFIG_EXYNOS_MCD_HDR
	int plane;
	struct dma_buf *meta_dma_buf;
	struct exynos_video_meta *video_meta;
	const struct dpu_fmt *fmt_info;
#endif

	/* 1 msec */
	aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			EXYNOS_DPU_GET_ACLK, NULL) / 1000U;

	for (i = 0; i < decon->dt.max_win; i++) {
		win = decon->win[i];
		/*
		 * Although DPP number is set in cur_using_dpp, connected window
		 * can be disabled. If window related parameter has problem,
		 * requested window from user will be disabled because of
		 * error handling code.
		 */
		if (!test_bit(win->dpp_id, &decon->cur_using_dpp) ||
				!(regs->win_regs[i].wincon & WIN_EN_F(i)))
			continue;

		sd = decon->dpp_sd[win->dpp_id];
		memcpy(&dpp_config.config, &regs->dpp_config[i],
				sizeof(struct decon_win_config));
		dpp_config.rcv_num = aclk_khz;

#ifdef CONFIG_EXYNOS_MCD_HDR
		dpp_config.wcg_mode = decon->color_mode;
		dpp_config.hdr_info.dst_max_luminance = decon->hdr_info.max_luma / 10000;

		fmt_info = dpu_find_fmt_info(regs->dpp_config[i].format);
		plane = fmt_info->num_buffers + fmt_info->num_meta_planes - 1;

		if (IS_HDR_FMT(regs->dpp_config[i].dpp_parm.hdr_std)
			&& (plane > 0)) {
			decon_dbg("DECON:INFO:%s:win%d:hdr mode : %d\n", __func__,
				i, regs->dpp_config[i].dpp_parm.hdr_std);

			meta_dma_buf = regs->dma_buf_data[i][plane].dma_buf;
			if (meta_dma_buf == NULL) {
				decon_err("DECON:ERR:%s:hdr meta buffer is null", __func__);
				goto dpp_config;
			}

			video_meta = (struct exynos_video_meta *)dma_buf_vmap(meta_dma_buf);
			if (IS_ERR_OR_NULL(video_meta)) {
				decon_err("%s: failed to map meta data\n", __func__);
				goto dpp_config;
			}

			dpp_config.hdr_info.type = video_meta->etype;

			if (video_meta->etype & VIDEO_INFO_TYPE_HDR_DYNAMIC)
				memcpy(&dpp_config.hdr_info.lut, video_meta->sHdrDynamicInfo.reserved,
					sizeof(unsigned int) * MAX_HDR10P_LUT);

			dma_buf_vunmap(meta_dma_buf, video_meta);
		}
dpp_config:
#endif
		ret = v4l2_subdev_call(sd, core, ioctl,
				DPP_WIN_CONFIG, &dpp_config);
		if (ret) {
			decon_err("failed to config (WIN%d : DPP%d)\n",
					i, win->dpp_id);
			regs->win_regs[i].wincon &= (~WIN_EN_F(i));
			decon_reg_set_win_enable(decon->id, i, false);
			if (regs->num_of_window != 0)
				regs->num_of_window--;
			clear_bit(win->dpp_id, &decon->cur_using_dpp);
			clear_bit(i, &decon->cur_req_win);
			set_bit(win->dpp_id, &decon->dpp_err_stat);
			err_cnt++;
		}
	}

	if (decon->dt.out_type == DECON_OUT_WB || regs->readback.request) {
		sd = decon->dpp_sd[ODMA_WB];
		memcpy(&dpp_config.config, &regs->dpp_config[decon->dt.wb_win],
				sizeof(struct decon_win_config));
		dpp_config.rcv_num = aclk_khz;

#ifdef CONFIG_EXYNOS_MCD_HDR
		dpp_config.wcg_mode = decon->color_mode;
#endif
		ret = v4l2_subdev_call(sd, core, ioctl, DPP_WIN_CONFIG,
				&dpp_config);
		if (ret) {
			decon_err("failed to config ODMA_WB\n");
			clear_bit(ODMA_WB, &decon->cur_using_dpp);
			set_bit(ODMA_WB, &decon->dpp_err_stat);
			err_cnt++;
		}
	}

	if (decon->prev_aclk_khz != aclk_khz)
		decon_info("DPU_ACLK(%ld khz)\n", aclk_khz);

	decon->prev_aclk_khz = aclk_khz;

	return err_cnt;
}

#if defined(CONFIG_EXYNOS_AFBC_DEBUG)
static void decon_save_afbc_enabled_win_id(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i;
	struct v4l2_subdev *sd;
	int afbc_enabled;

	for (i = 0; i < decon->dt.max_win; ++i)
		decon->d.prev_afbc_win_id[i] = -1;

	for (i = 0; i < decon->dt.max_win; ++i) {
		if (regs->dpp_config[i].state == DECON_WIN_STATE_BUFFER) {
			sd = decon->dpp_sd[regs->dpp_config[i].channel]; /* ch */
			afbc_enabled = 0;
			v4l2_subdev_call(sd, core, ioctl,
					DPP_AFBC_ATTR_ENABLED, &afbc_enabled);
			/* if afbc enabled, DMA2CH <-> win_id mapping */
			if (regs->dpp_config[i].compression && afbc_enabled)
				decon->d.prev_afbc_win_id[regs->dpp_config[i].channel] = i; /* ch */
			else
				decon->d.prev_afbc_win_id[regs->dpp_config[i].channel] = -1; /* ch */

			decon_dbg("%s:%d win(%d), ch(%d),\
				afbc(%d), save(%d)\n", __func__, __LINE__,
				i, regs->dpp_config[i].channel, afbc_enabled,
				decon->d.prev_afbc_win_id[regs->dpp_config[i].channel]);
		}
	}
}

static void decon_dump_afbc_handle(struct decon_device *decon,
		struct decon_dma_buf_data (*dma_bufs)[MAX_PLANE_CNT])
{
	int i;
	int win_id = 0;
	int size;
	void *v_addr;

	decon_info("%s +\n", __func__);

	for (i = 0; i < decon->dt.max_win; i++) {
		/* check each channel whether they were prev-enabled */
		if (decon->d.prev_afbc_win_id[i] != -1
				&& test_bit(i, &decon->prev_used_dpp)) {
			win_id = decon->d.prev_afbc_win_id[i];

			decon->d.dmabuf[win_id][0] =
				dma_bufs[win_id][0].dma_buf;
			decon_info("DMA%d(WIN%d): dmabuf=0x%p\n",
				DPU_CH2DMA(i), win_id, decon->d.dmabuf[win_id][0]);
			v_addr = dma_buf_vmap(dma_bufs[win_id][0].dma_buf);
			if (IS_ERR_OR_NULL(v_addr)) {
				decon_err("%s: failed to map afbc buffer\n",
						__func__);
				return;
			}
			size = dma_bufs[win_id][0].dma_buf->size;

			decon_info("DV(0x%p), KV(0x%p), size(%d)\n",
					(void *)dma_bufs[win_id][0].dma_addr,
					v_addr, size);
			dma_buf_vunmap(dma_bufs[win_id][0].dma_buf, v_addr);
		}
	}

	decon_info("%s -\n", __func__);
}
#endif

#ifdef CONFIG_EXYNOS_MCD_HDR

static void decon_init_hdr_info(struct decon_device *decon)
{
	if (decon->lcd_info == NULL) {
		return;
	}

	if (decon->lcd_info->hdr.num)
		memcpy(&decon->hdr_info, &decon->lcd_info->hdr, sizeof(struct lcd_hdr_info));

	decon_info("DECON:INFO:%s:new hdr num:%d\n",
		__func__, decon->lcd_info->hdr.num);
}

static ssize_t show_wcg_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct decon_device *decon = dev_get_drvdata(dev);

	len = snprintf(buf, PAGE_SIZE, "WCG_MODE : %d\n",
		decon->color_mode);

	return len;
}

static ssize_t store_wcg_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct decon_device *decon = dev_get_drvdata(dev);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;

	decon->color_mode = value;

	return size;
}

static DEVICE_ATTR(wcg_mode, 0644, show_wcg_mode, store_wcg_mode);

int create_wcg_sysfs(struct decon_device *decon)
{
	int ret = 0;

	if (decon->id != 0)
		return 0;

	ret = device_create_file(decon->dev, &dev_attr_wcg_mode);
	if (ret) {
		decon_err("failed to create psr info file\n");
		return ret;
	}

	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_MASK_LAYER
static int decon_set_mask_layer(struct decon_device *decon, struct decon_reg_data *regs, bool after)
{
	int ret = 0;
	struct mask_layer_data req_data;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return 0;

	if (regs->mask_layer == decon->cur_mask_layer)
		return 0;

	req_data.trigger_time  = after ? MASK_LAYER_TRIGGER_AFTER : MASK_LAYER_TRIGGER_BEFORE;
	req_data.req_mask_layer = regs->mask_layer ? MASK_LAYER_ON : MASK_LAYER_OFF;

	decon_info("DECON:INFO:%s:cur:%s req:%s trigger:%s\n", __func__,
			decon->cur_mask_layer ? "on" : "off",
			regs->mask_layer ? "on" : "off",
			after ? "after" : "before");

	decon_systrace(decon, 'C', "decon_mask_layer", 1);
	ret = v4l2_subdev_call(decon->panel_sd, core, ioctl,
		PANEL_IOC_SET_MASK_LAYER, &req_data);

	decon_systrace(decon, 'C', "decon_mask_layer", 0);

	if (after) {
		decon->cur_mask_layer = regs->mask_layer;
		if (decon->wait_mask_layer_trigger) {
			decon->wait_mask_layer_trigger = 0;
			decon_info("DECON:INFO:%s:wq wait:%d\n", __func__, decon->wait_mask_layer_trigger);
			wake_up_interruptible_all(&decon->wait_mask_layer_trigger_queue);
		}
	}

	return 0;
}

static bool decon_get_mask_layer(struct decon_device *decon,
	struct decon_win_config_data *win_data)
{
	int i;
	bool req_mask_layer= false;
	struct decon_win_config *config;
	struct decon_win_config *win_config = win_data->config;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return 0;

	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];
		if (config && (config->state == DECON_WIN_STATE_FINGERPRINT)) {
			decon_dbg("%s: mask layer enable: %d\n", __func__, i);
			config->state = DECON_WIN_STATE_BUFFER;
			req_mask_layer = true;
			break;
		}
	}

	if (req_mask_layer != decon->cur_mask_layer) {
		decon->wait_mask_layer_trigger = 1;
		decon_info("DECON:INFO:%s:wq wait:%d\n", __func__, decon->wait_mask_layer_trigger);
	}

	return req_mask_layer;
}

static void decon_wait_mask_layer_trigger(struct decon_device *decon)
{
	int timeout = 0;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return;

	if (decon->wait_mask_layer_trigger) {
		timeout = wait_event_interruptible_timeout(decon->wait_mask_layer_trigger_queue,
				!decon->wait_mask_layer_trigger,
				msecs_to_jiffies(100));
		if (timeout > 0) {
			decon_info("DECON:INFO:%s:wq wait:%d\n", __func__,
				decon->wait_mask_layer_trigger);
		} else {
			decon_warn("DECON:WARN:%s: (timeout) wq wait:%d cnt:%d\n",  __func__,
				decon->wait_mask_layer_trigger,
				atomic_read(&decon->up.remaining_frame));
		}
		decon->wait_mask_layer_trigger = 0;
	}
}
#endif

static int __decon_update_regs(struct decon_device *decon, struct decon_reg_data *regs)
{
	int err_cnt = 0;
	unsigned short i, j;
	struct decon_mode_info psr;
	struct decon_param p;
	bool has_cursor_win = false;

	decon_to_psr_info(decon, &psr);
	/*
	 * Shadow update bit must be cleared before setting window configuration,
	 * If shadow update bit is not cleared, decon initial state or previous
	 * window configuration has problem.
	 */
	if (decon_reg_wait_update_done_and_mask(decon->id, &psr,
				SHADOW_UPDATE_TIMEOUT) > 0) {
		decon_warn("decon SHADOW_UPDATE_TIMEOUT\n");
		return -ETIMEDOUT;
	}

	for (i = 0; i < decon->dt.max_win; i++) {
		if (regs->is_cursor_win[i]) {
			has_cursor_win = true;
			break;
		}
	}

	/* TODO: check and wait until the required IDMA is free */
	decon_reg_chmap_validate(decon, regs);

	/* apply multi-resolution configuration */
	dpu_set_mres_config(decon, regs);

	/* apply window update configuration to DECON, DSIM and panel */
	dpu_set_win_update_config(decon, regs);

	/* request to change DPHY PLL frequency */
	dpu_set_freq_hop(decon, regs, true);

	err_cnt = decon_set_dpp_config(decon, regs);
	if (!regs->num_of_window) {
		decon_err("decon%d: num_of_window=0 during dpp_config(err_cnt:%d)\n",
			decon->id, err_cnt);
		/*
		 * The global update is not cleared in command mode because
		 * trigger is masked. If num_of_window becomes zero, trigger doesn't
		 * have any chance to change unmask status.
		 * So, just window is disabled in error case and then next update handler
		 * will update to shadow register
		 */
		if (decon->dt.psr_mode == DECON_VIDEO_MODE) {
			for (i = 0; i < decon->dt.max_win; i++)
				decon_reg_set_win_enable(decon->id, i, false);
			decon_reg_all_win_shadow_update_req(decon->id);
			decon_reg_update_req_global(decon->id);
		}
		return 0;
	}

#if defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	decon_set_protected_content(decon, regs);
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	/* buffer should be filled with black data if drm contents */
	if (regs->readback.request && !regs->readback.drm) {
		decon_dbg("%s: CWB path enable!\n", __func__);
		decon_reg_set_cwb_enable(decon->id, true);
	} else
		decon_reg_set_cwb_enable(decon->id, false);
#endif

	for (i = 0; i < decon->dt.max_win; i++) {
		/* set decon registers for each window */
		decon_reg_set_window_control(decon->id, i, &regs->win_regs[i],
						regs->win_regs[i].winmap_state);

		if (decon->dt.out_type != DECON_OUT_WB) {
			/* backup cur dma_buf_data for freeing next update_handler_regs */
			for (j = 0; j < regs->plane_cnt[i]; ++j)
				decon->win[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];
			decon->win[i]->plane_cnt = regs->plane_cnt[i];
#ifdef DEBUG_DMA_BUF_LEAK
			decon->import_cnt = regs->import_cnt;
			decon->import_time = regs->import_time;
#endif
		}
	}

	if (decon->dt.out_type == DECON_OUT_WB) {
		/* update resolution info for decon blender size */
		decon->lcd_info->xres = regs->dpp_config[decon->dt.wb_win].dst.w;
		decon->lcd_info->yres = regs->dpp_config[decon->dt.wb_win].dst.h;

		decon_to_init_param(decon, &p);
		decon_reg_config_wb_size(decon->id, decon->lcd_info, &p);
	}

	decon_reg_all_win_shadow_update_req(decon->id);
	decon_to_psr_info(decon, &psr);

#ifdef CONFIG_DYNAMIC_FREQ
	if (decon->dt.out_type == DECON_OUT_DSI)
		decon->last_update_time = ktime_get();
#endif
#if defined(CONFIG_DECON_VRR_MODULATION)
	decon_wait_for_active_region(decon, VSYNC_TIMEOUT_MSEC);
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	decon_set_mask_layer(decon, regs, 0);
#endif
	if (decon_reg_start(decon->id, &psr) < 0) {
		decon_up_list_saved();
		decon_dump(decon, true);
		BUG();
	}

	decon_set_cursor_unmask(decon, has_cursor_win);

	DPU_EVENT_LOG(DPU_EVT_TRIG_UNMASK, &decon->sd, ktime_set(0, 0));

	return 0;
}

void decon_wait_for_vstatus(struct decon_device *decon, u32 timeout)
{
	int ret;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return;

	decon_systrace(decon, 'C', "decon_frame_start", 1);
	ret = wait_event_interruptible_timeout(decon->wait_vstatus,
			(decon->frame_cnt_target <= decon->frame_cnt),
			msecs_to_jiffies(timeout));
	decon_systrace(decon, 'C', "decon_frame_start", 0);
	DPU_EVENT_LOG(DPU_EVT_DECON_FRAMESTART, &decon->sd, ktime_set(0, 0));
}

static void decon_acquire_old_bufs(struct decon_device *decon,
		struct decon_reg_data *regs,
		struct decon_dma_buf_data (*dma_bufs)[MAX_PLANE_CNT],
		int *plane_cnt, int *import_cnt, ktime_t *import_time)
{
	int i, j;

	for (i = 0; i < decon->dt.max_win; i++) {
		for (j = 0; j < MAX_PLANE_CNT; ++j)
			memset(&dma_bufs[i][j], 0, sizeof(struct decon_dma_buf_data));
		plane_cnt[i] = 0;
	}

	for (i = 0; i < decon->dt.max_win; i++) {
		if (decon->dt.out_type == DECON_OUT_WB)
			plane_cnt[i] = regs->plane_cnt[i];
		else
			plane_cnt[i] = decon->win[i]->plane_cnt;
		for (j = 0; j < plane_cnt[i]; ++j)
			dma_bufs[i][j] = decon->win[i]->dma_buf_data[j];
	}
#ifdef DEBUG_DMA_BUF_LEAK
	*import_cnt = decon->import_cnt;

*import_time = decon->import_time;
#endif

}

static void decon_release_old_bufs(struct decon_device *decon,
		struct decon_reg_data *regs,
		struct decon_dma_buf_data (*dma_bufs)[MAX_PLANE_CNT],
		int *plane_cnt, int import_cnt, ktime_t import_time)
{
	int i, j;
	struct dsim_device *dsim;

#ifdef DEBUG_DMA_BUF_LEAK
	decon->free_cnt = 0;
#endif

	for (i = 0; i < decon->dt.max_win; i++) {
		for (j = 0; j < plane_cnt[i]; ++j)
			if (decon->dt.out_type == DECON_OUT_WB)
				decon_free_dma_buf(decon, &regs->dma_buf_data[i][j]);
			else
				decon_free_dma_buf(decon, &dma_bufs[i][j]);
	}

	if (decon->dt.out_type == DECON_OUT_WB) {
		for (j = 0; j < plane_cnt[0]; ++j)
			decon_free_dma_buf(decon,
					&regs->dma_buf_data[decon->dt.wb_win][j]);
	}

	if (decon->dt.out_type == DECON_OUT_DSI) {
		if (decon->lcd_info->mode == DECON_VIDEO_MODE) {
			dsim = v4l2_get_subdevdata(decon->out_sd[0]);
			if (dsim->fb_handover.reserved) {
				v4l2_subdev_call(decon->out_sd[0], core, ioctl,
						DSIM_IOC_FREE_FB_RES, NULL);
			}
		}
	}
#ifdef DEBUG_DMA_BUF_LEAK
	if (import_cnt != decon->free_cnt) {
		decon_info("[DECON:INFO]:%s: import: %d, free:%d\n",
			__func__, import_cnt, decon->free_cnt);
		if (import_cnt > decon->free_cnt) {
			decon_info("[DECON:INFO]:%s dma buf import time : %lld\n",
				__func__, ktime_to_us(import_time));
			logging_dma_leak_info(decon, 0, import_cnt, import_time);
		}
	}
#endif

}


static int decon_set_hdr_info(struct decon_device *decon,
		struct decon_reg_data *regs, int win_num, bool on)
{
	struct exynos_video_meta *video_meta;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	int ret = 0;
#endif
	int hdr_cmp = 0;
	int mp_idx = 0;	/* meta plane index */
	const struct dpu_fmt *fmt_info;

	if (!on) {
		struct exynos_hdr_static_info hdr_static_info;

		hdr_static_info.mid = -1;
		decon->prev_hdr_info.mid = -1;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		ret = v4l2_subdev_call(decon->displayport_sd, core, ioctl,
				DISPLAYPORT_IOC_SET_HDR_METADATA,
				&hdr_static_info);
		if (ret)
			goto err_hdr_io;
#endif
		return 0;
	}

	fmt_info = dpu_find_fmt_info(regs->dpp_config[win_num].format);
	mp_idx = fmt_info->num_buffers + fmt_info->num_meta_planes - 1;
	if (mp_idx < 0) {
		decon_err("Unsupported hdr metadata format\n");
		return -EINVAL;
	}

	if (!regs->dma_buf_data[win_num][mp_idx].dma_addr) {
		decon_err("hdr metadata address is NULL\n");
		return -EINVAL;
	}
	video_meta = (struct exynos_video_meta *)dma_buf_vmap(
			regs->dma_buf_data[win_num][mp_idx].dma_buf);
	if (IS_ERR_OR_NULL(video_meta)) {
		decon_err("Failed to get virtual address (err %pK)\n", video_meta);
		return -ENOMEM;
	}

	hdr_cmp = memcmp(&decon->prev_hdr_info,
			&video_meta->shdr_static_info,
			sizeof(struct exynos_hdr_static_info));

	/* HDR metadata is same, so skip subdev call.
	 * Also current hdr_static_info is not copied.
	 */
	if (hdr_cmp == 0) {
		dma_buf_vunmap(regs->dma_buf_data[win_num][mp_idx].dma_buf, video_meta);
		return 0;
	}
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	ret = v4l2_subdev_call(decon->displayport_sd, core, ioctl,
			DISPLAYPORT_IOC_SET_HDR_METADATA,
			&video_meta->shdr_static_info);
	if (ret)
		goto err_hdr_io;
#endif
	memcpy(&decon->prev_hdr_info,
			&video_meta->shdr_static_info,
			sizeof(struct exynos_hdr_static_info));
	dma_buf_vunmap(regs->dma_buf_data[win_num][mp_idx].dma_buf, video_meta);

	return 0;

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
err_hdr_io:
#endif
	/* When the subdev call is failed,
	 * current hdr_static_info is not copied to prev.
	 */
	decon_err("hdr metadata info subdev call is failed\n");

	dma_buf_vunmap(regs->dma_buf_data[win_num][mp_idx].dma_buf, video_meta);

	return -EFAULT;
}

static void decon_update_hdr_info(struct decon_device *decon,
		struct decon_reg_data *regs)
{

	int i, ret = 0, win_num = 0, hdr_cnt = 0;
	unsigned long cur_hdr_bits = 0;

	/* On only DP case, hdr static info could be transfered to DP */
	if (decon->dt.out_type != DECON_OUT_DP)
		return;

	decon_dbg("%s +\n", __func__);
	/* Check hdr configuration of enabled window */
	for (i = 0; i < decon->dt.max_win; i++) {
		if (regs->dpp_config[i].state == DECON_WIN_STATE_BUFFER
#ifndef CONFIG_EXYNOS_MCD_HDR
			&& regs->dpp_config[i].dpp_parm.hdr_std) {
#else
			&& IS_HDR_FMT(regs->dpp_config[i].dpp_parm.hdr_std)) {
#endif
			set_bit(i, &cur_hdr_bits);

			if (cur_hdr_bits)
				win_num = i;

			hdr_cnt++;
#ifndef CONFIG_EXYNOS_MCD_HDR
			if (hdr_cnt > 1) {
				decon_err("DP support Only signle HDR\n");
				ret = -EINVAL;
				goto err_hdr;
			}
#endif
		}
	}

	/* Check hdr configuration or hdr window is chagned */
	if (decon->prev_hdr_bits != cur_hdr_bits) {
		if (cur_hdr_bits == 0) {
			/* Case : HDR ON -> HDR OFF, turn off hdr */
			ret = decon_set_hdr_info(decon, regs, win_num, false);
			if (ret)
				goto err_hdr;
		} else {
			/* Case : HDR OFF -> HDR ON, turn on hdr*/
			/* Case : HDR ON -> HDR ON, hdr window is changed */
			ret = decon_set_hdr_info(decon, regs, win_num, true);
			if (ret)
				goto err_hdr;
		}
		/* Save current hdr configuration information */
		decon->prev_hdr_bits = cur_hdr_bits;
	} else {
		if (cur_hdr_bits == 0) {
			/* Case : HDR OFF -> HDR OFF, Do nothing */
		} else {
			/* Case : HDR ON -> HDR ON, compare hdr metadata with prev info */
			ret = decon_set_hdr_info(decon, regs, win_num, true);
			if (ret)
				goto err_hdr;
		}
	}
	decon_dbg("%s -\n", __func__);

err_hdr:
	/* HDR STANDARD information should be changed to OFF.
	 * Because DP doesn't use the HDR engine
	 */
#ifndef CONFIG_EXYNOS_MCD_HDR
	for (i = 0; i < decon->dt.max_win; i++)
		if (regs->dpp_config[i].dpp_parm.hdr_std != DPP_HDR_OFF)
			regs->dpp_config[i].dpp_parm.hdr_std = DPP_HDR_OFF;
#endif
	if (ret) {
		decon_err("set hdr metadata is failed, err no is %d\n", ret);
		decon->prev_hdr_bits = 0;
	}
}

#if defined(CONFIG_EXYNOS_AFBC_DEBUG)
static void decon_update_afbc_info(struct decon_device *decon,
		struct decon_reg_data *regs, bool cur)
{
	int i, ch;
	struct dpu_afbc_info *afbc_info;

	decon_dbg("%s +\n", __func__);

	if (cur) /* save current AFBC information */
		afbc_info = &decon->d.cur_afbc_info;
	else /* save previous AFBC information */
		afbc_info = &decon->d.prev_afbc_info;

	memset(afbc_info, 0, sizeof(struct dpu_afbc_info));

	for (i = 0; i < decon->dt.max_win; i++) {
		if (!regs->dpp_config[i].compression)
			continue;

		ch = regs->dpp_config[i].channel; /* ch */
		if (test_bit(ch, &decon->cur_using_dpp)) {
			if (regs->dma_buf_data[i][0].dma_buf == NULL)
				continue;

			afbc_info->is_afbc[ch] = true;

			afbc_info->dma_addr[ch] =
				regs->dma_buf_data[i][0].dma_addr;
			afbc_info->dma_buf[ch] =
				regs->dma_buf_data[i][0].dma_buf;
			afbc_info->sg_table[ch] =
				regs->dma_buf_data[i][0].sg_table;
		}
	}

	decon_dbg("%s -\n", __func__);
}
#endif

static void decon_save_cur_buf_info(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i, j;

	for (i = 0; i < decon->dt.max_win; i++) {
		if (decon->dt.out_type != DECON_OUT_WB) {
			/* backup cur dma_buf_data for freeing next update_handler_regs */
			for (j = 0; j < regs->plane_cnt[i]; ++j)
				decon->win[i]->dma_buf_data[j] = regs->dma_buf_data[i][j];
			decon->win[i]->plane_cnt = regs->plane_cnt[i];
		}
	}
}

void decon_readback_wq(struct work_struct *work)
{
	struct decon_device *decon = get_decon_drvdata(0);
	int i;

	if (!decon->readback.enabled) {
		decon_info("%s: readback feature is disabled\n", __func__);
		return;
	}

	decon_dbg("%s +\n", __func__);

	for (i = 0; i < decon->readback.num_buffers; ++i)
		decon_free_dma_buf(decon, &decon->readback.dma_buf_data[i]);
	decon_signal_fence(decon, decon->readback.fence);
	dma_fence_put(decon->readback.fence);

	decon->readback.unmap_cnt++;

	decon_dbg("%s -\n", __func__);
}

#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
static void decon_handle_readback_buffer(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int i;
	void *buf_addr;
	size_t buf_size;
	const struct dpu_fmt *fmt_info;

	decon_dbg("%s +\n", __func__);

	fmt_info = dpu_find_fmt_info(regs->dpp_config[decon->dt.wb_win].format);

	decon->readback.request = true;
	decon->readback.num_buffers = fmt_info->num_buffers;
	for (i = 0; i < fmt_info->num_buffers; ++i)
		decon->readback.dma_buf_data[i] = regs->dma_buf_data[decon->dt.wb_win][i];
	decon->readback.fence = regs->readback.fence;

	if (regs->readback.drm) {
		for (i = 0; i < fmt_info->num_buffers; ++i) {
			/* kernel virtual */
			buf_addr = dma_buf_vmap(decon->readback.dma_buf_data[i].dma_buf);
			if (IS_ERR_OR_NULL(buf_addr)) {
				decon_err("%s: failed to map readback buffer\n",
						__func__);
				return;
			}
			buf_size = regs->dma_buf_data[decon->dt.wb_win][i].dma_buf->size;
			memset(buf_addr, 0x00, buf_size);
			dma_buf_vunmap(decon->readback.dma_buf_data[i].dma_buf, buf_addr);
		}
	}

	decon_dbg("%s -\n", __func__);
}
#endif

static void decon_update_fps(struct decon_device *decon,
			struct decon_reg_data *regs)
{
	struct vrr_config_data *vrr_config = &regs->vrr_config;

	decon_info("[VRR] %s:%d%s\n",
		__func__, vrr_config->fps,
		EXYNOS_VRR_MODE_STR(vrr_config->mode));

	decon_exit_hiber(decon);
	decon_systrace(decon, 'B', "decon_update_fps", 1);
	dpu_update_vrr_lcd_info(decon, vrr_config);
	dpu_set_vrr_config(decon, vrr_config);
	decon_systrace(decon, 'B', "decon_update_fps", 0);
}


#define GET_WINCONFIG_TIME(regs)	(ktime_to_us(ktime_sub(ktime_get(), regs->create_time)))

#define TIME_UPDATE_THREAD			1000
#define TIME_EXIT_HIBERNATION		4000
#define TIME_FENCE_WAIT				16600


#ifdef CONFIG_SUPPORT_MAFPC
static int decon_wait_mafpc_complete(struct decon_device *decon)
{
	int ret = 0;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return 0;

	if (decon->panel_state->disp_on == PANEL_DISPLAY_OFF)
		ret = v4l2_subdev_call(decon->panel_sd, core, ioctl,
			PANEL_IOC_WAIT_MAFPC, NULL);

	return ret;
}
#endif

static void decon_update_regs(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	struct decon_dma_buf_data old_dma_bufs[decon->dt.max_win][MAX_PLANE_CNT];
	int old_plane_cnt[MAX_DECON_WIN];
	int import_cnt;
	ktime_t import_time;
	struct decon_mode_info psr;
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
	struct dsim_device *dsim;
#endif
	int i, j, err;
#ifdef CONFIG_PROFILE_WINCONFIG
	s64 update_time;
	s64 hiber_time;
	s64 fence_time;
#endif

	if (!decon->systrace.pid)
		decon->systrace.pid = current->pid;

	decon_systrace(decon, 'B', "decon_update_regs", 0);

#ifdef CONFIG_PROFILE_WINCONFIG
	update_time = GET_WINCONFIG_TIME(regs);
	if ((regs->remained_frames == 0) && (update_time > TIME_UPDATE_THREAD)) {
		decon_info("[DECON:PROFILE]%s time execute update thread %lld :%d\n",
			__func__, update_time, regs->remained_frames);
	}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
	err = decon_wait_mafpc_complete(decon);
	if (err) {
		decon_err("DECON:ERR:%s: failed to wait mafpc complete\n", __func__);
	}
#endif
	decon_exit_hiber(decon);

#ifdef CONFIG_PROFILE_WINCONFIG
	hiber_time = GET_WINCONFIG_TIME(regs) - update_time;
	if (hiber_time > TIME_EXIT_HIBERNATION) {
		decon_info("[DECON:PROFILE]%s time execute update thread %lld\n",
			__func__, hiber_time);
	}
#endif

	decon_acquire_old_bufs(decon, regs, old_dma_bufs, old_plane_cnt, &import_cnt, &import_time);

	decon_systrace(decon, 'C', "decon_fence_wait", 1);
	for (i = 0; i < decon->dt.max_win; i++) {
		if (regs->dma_buf_data[i][0].fence) {
			err = decon_wait_fence(decon,
					regs->dma_buf_data[i][0].fence,
					regs->dpp_config[i].acq_fence);
			if (err <= 0) {
				decon_save_cur_buf_info(decon, regs);
				decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
				decon->win_up.force_full = true;
				DPU_EVENT_LOG_WINCON(&decon->sd, regs,
					UH_ID_IFENCE_ERR);
				goto fence_err;
			}
		}
	}
#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	if (regs->readback.request) {
		/* wait release fence to write data */
		if (regs->dma_buf_data[decon->dt.wb_win][0].fence) {
			err = decon_wait_fence(decon,
					regs->dma_buf_data[decon->dt.wb_win][0].fence,
					regs->dpp_config[decon->dt.wb_win].rel_fence);
			if (err <= 0) {
				decon_save_cur_buf_info(decon, regs);
				decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
				decon->win_up.force_full = true;
				DPU_EVENT_LOG_WINCON(&decon->sd, regs,
					UH_ID_OFENCE_ERR);
				goto fence_err;
			}
		}
	}
#endif
	decon_systrace(decon, 'C', "decon_fence_wait", 0);

#ifdef CONFIG_PROFILE_WINCONFIG
	fence_time = GET_WINCONFIG_TIME(regs) - update_time;
	if (fence_time > TIME_FENCE_WAIT) {
		decon_info("[DECON:PROFILE]%s time execute fence %lld\n",
			__func__, fence_time);
	}
#endif

#if defined(CONFIG_EXYNOS_AFBC_DEBUG)
	decon_update_afbc_info(decon, regs, true);
#endif

	decon_to_psr_info(decon, &psr);
	if (decon_check_used_dpp(decon, regs)) {
		/*
		 * If this is the first handling of update_handler thread
		 * after unblanked, HW trigger is unmasked. It makes unnecessary
		 * data transmission
		 */
		if ((decon->dt.psr_mode == DECON_MIPI_COMMAND_MODE) &&
				(decon->dt.trig_mode == DECON_HW_TRIG))
			decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);
		decon_save_cur_buf_info(decon, regs);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		decon->win_up.force_full = true;
		DPU_EVENT_LOG_WINCON(&decon->sd, regs, UH_ID_DPP_ERR);
		goto fence_err;
	}

	decon_update_hdr_info(decon, regs);

	/*
	 * LCD resolution infomation updating is required to run BTS
	 * because it is used to calculate Frequency & Bandwidth.
	 */
	if (regs->dpp_config[DECON_WIN_UPDATE_IDX].state ==
			DECON_WIN_STATE_MRESOL)
		dpu_update_mres_lcd_info(decon, regs);

#if defined(CONFIG_EXYNOS_BTS)
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
	/* add calc and update bw : cur > prev */
	/* TODO: if multi resolution requeset X */
	if (!(regs->dpp_config[DECON_WIN_UPDATE_IDX].state &
			DECON_WIN_STATE_MRESOL)) {
		decon->bts.ops->bts_calc_bw(decon, regs);
		decon->bts.ops->bts_update_bw(decon, regs, 0);
	}
#else
	/* add calc and update bw : cur > prev */
	decon->bts.ops->bts_calc_bw(decon, regs);
	decon->bts.ops->bts_update_bw(decon, regs, 0);
#endif
#endif
	DPU_EVENT_LOG_WINCON(&decon->sd, regs, UH_ID_NORMAL);

	if (regs->num_of_window) {
		if (__decon_update_regs(decon, regs) < 0) {
#if defined(CONFIG_EXYNOS_AFBC_DEBUG)
			decon_dump_afbc_handle(decon, old_dma_bufs);
#endif
			decon_dump(decon, true);
#ifdef CONFIG_LOGGING_BIGDATA_BUG
			log_decon_bigdata(decon);
#endif
			s3c2410wdt_set_emergency_reset(0,0); /* watch dog reset */
		}

		if (!regs->num_of_window) {
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
			if (regs->dpp_config[DECON_WIN_UPDATE_IDX].state &
				DECON_WIN_STATE_MRESOL)
				dpu_set_mres_config(decon, regs);
#endif
			decon_save_cur_buf_info(decon, regs);
			decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
			goto end;
		}
	} else {
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
		if (regs->dpp_config[DECON_WIN_UPDATE_IDX].state &
				DECON_WIN_STATE_MRESOL) {
			dpu_set_mres_config(decon, regs);
#if IS_ENABLED(CONFIG_EXYNOS_FPS_CHANGE_NOTIFY)
			if (decon->lcd_info->fps != regs->fps)
				notify_fps_change(regs->fps);
#endif
			dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
			dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SET_VREFRESH,
					&(regs->fps));
		} else {
			if ((decon->dt.psr_mode == DECON_MIPI_COMMAND_MODE) &&
					(decon->dt.trig_mode == DECON_HW_TRIG)) {
				decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_ENABLE);

				DPU_EVENT_LOG(DPU_EVT_TRIG_UNMASK, &decon->sd,
						ktime_set(0, 0));
			}

			for (i = 0; i < decon->dt.max_win; i++)
				decon_reg_set_win_enable(decon->id, i, false);
			decon_reg_all_win_shadow_update_req(decon->id);
			decon_reg_update_req_global(decon->id);

			DPU_EVENT_LOG(DPU_EVT_STORE_RSC, &decon->sd, ktime_set(0, 0));
		}
#endif
		decon_save_cur_buf_info(decon, regs);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

		if (!(regs->dpp_config[DECON_WIN_UPDATE_IDX].state & DECON_WIN_STATE_MRESOL)
				&& (decon->dt.psr_mode == DECON_MIPI_COMMAND_MODE)
				&& (decon->dt.trig_mode == DECON_HW_TRIG)) {
			decon_reg_wait_update_done_timeout(decon->id, SHADOW_UPDATE_TIMEOUT);
			decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);
			DPU_EVENT_LOG(DPU_EVT_TRIG_MASK, &decon->sd, ktime_set(0, 0));
			DPU_EVENT_LOG(DPU_EVT_STORE_RSC, &decon->sd, ktime_set(0, 0));
		}

		goto end;
	}

	if (decon->dt.out_type == DECON_OUT_WB) {
		decon_reg_release_resource(decon->id, &psr);
		decon_dpp_wait_wb_framedone(decon);
		/* Stop to prevent resource conflict */
		decon->cur_using_dpp = 0;
#if defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
		decon_set_protected_content(decon, NULL);
#endif
	} else {
		decon->frame_cnt_target = decon->frame_cnt + 1;

		decon_systrace(decon, 'C', "decon_wait_vsync", 1);
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		decon_systrace(decon, 'C', "decon_wait_vsync", 0);

		decon_wait_for_vstatus(decon, 50);
		if (decon_reg_wait_update_done_timeout(decon->id, SHADOW_UPDATE_TIMEOUT) < 0) {
#if defined(CONFIG_EXYNOS_COMMON_PANEL) || \
	defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
			if (decon_is_bypass(decon))
				goto end;
#endif
			decon_up_list_saved();
#if defined(CONFIG_EXYNOS_AFBC_DEBUG)
			decon_dump_afbc_handle(decon, old_dma_bufs);
#endif
			decon_dump(decon, true);
#ifdef CONFIG_LOGGING_BIGDATA_BUG
			log_decon_bigdata(decon);
#endif
			BUG();
		}
		DPU_DEBUG_DMA_BUF("frame_start\n");
		for (i = 0; i < decon->dt.max_win; i++) {
			if (regs->win_regs[i].wincon & WIN_EN_F(i)) {
				for (j = 0; j < MAX_PLANE_CNT; j++) {
					if (regs->dma_buf_data[i][j].dma_buf)
						DPU_DEBUG_DMA_BUF("dma_buf_%d[%p]\n",
								i, regs->dma_buf_data[i][j].dma_buf);
				}
			}
		}
		if (!decon->low_persistence) {
			decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);
			DPU_EVENT_LOG(DPU_EVT_TRIG_MASK, &decon->sd, ktime_set(0, 0));
		}
	}

end:
#if defined(CONFIG_EXYNOS_BTS)
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
	/* add update bw : cur < prev */
	/* TODO: if multi resolution requeset X */
	if (!(regs->dpp_config[DECON_WIN_UPDATE_IDX].state &
			DECON_WIN_STATE_MRESOL))
		decon->bts.ops->bts_update_bw(decon, regs, 1);
#else
	/* add update bw : cur < prev */
	decon->bts.ops->bts_update_bw(decon, regs, 1);
#endif
#endif

	/*
	 * After shadow update, changed PLL is applied and
	 * target M value is stored
	 */
	dpu_set_freq_hop(decon, regs, false);

	decon_dpp_stop(decon, false);

#ifdef CONFIG_SUPPORT_MASK_LAYER
	decon_set_mask_layer(decon, regs, 1);
#endif

fence_err:
	decon_release_old_bufs(decon, regs, old_dma_bufs, old_plane_cnt, import_cnt, import_time);
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
	if (!(regs->dpp_config[DECON_WIN_UPDATE_IDX].state &
			DECON_WIN_STATE_MRESOL)) {
		decon_signal_fence(decon, regs->retire_fence);
		dma_fence_put(regs->retire_fence);
	}
#else
	decon_signal_fence(decon, regs->retire_fence);
	dma_fence_put(regs->retire_fence);
#endif
	DPU_EVENT_LOG(DPU_EVT_FENCE_RELEASE, &decon->sd, ktime_set(0, 0));
#ifdef CONFIG_EXYNOS_FPS_CHANGE_NOTIFY
	ems_frame_cnt++;
#endif
#if defined(CONFIG_EXYNOS_AFBC_DEBUG)
	decon_save_afbc_enabled_win_id(decon, regs);
	decon_update_afbc_info(decon, regs, false);
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	if (regs->readback.request)
		decon_handle_readback_buffer(decon, regs);
	else
		decon->readback.request = false;
#endif
	decon_systrace(decon, 'E', "decon_update_regs", 0);
}

/*
 * this function is made for refresh last decon_reg_data that is stored
 * in update_handler. it will be called after recovery subdev.
 * TODO : combine decon_update_regs and decon_update_last_regs
 */
int decon_update_last_regs(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	int ret = 0;
	struct decon_mode_info psr;

	decon_info("%s +\n", __func__);

	decon_exit_hiber(decon);

	decon_check_used_dpp(decon, regs);

	decon_update_hdr_info(decon, regs);

	if (regs->dpp_config[DECON_WIN_UPDATE_IDX].state ==
			DECON_WIN_STATE_MRESOL)
		dpu_update_mres_lcd_info(decon, regs);

#if defined(CONFIG_EXYNOS_BTS)
	/* add calc and update bw : cur > prev */
	decon->bts.ops->bts_calc_bw(decon, regs);
	decon->bts.ops->bts_update_bw(decon, regs, 0);
#endif

	DPU_EVENT_LOG_WINCON(&decon->sd, regs, UH_ID_LAST_REGS);

	decon_to_psr_info(decon, &psr);
	if (regs->num_of_window) {
		if (__decon_update_regs(decon, regs) < 0) {
			decon_dump(decon, true);
			BUG();
		}
		if (!regs->num_of_window) {
			decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
			goto end;
		}
	} else {
		decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		goto end;
	}

	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

	if (decon->cursor.unmask)
		decon_set_cursor_unmask(decon, false);

	decon_wait_for_vstatus(decon, 50);
	if (decon_reg_wait_update_done_timeout(decon->id, SHADOW_UPDATE_TIMEOUT) < 0) {
		decon_err("%s shadow update timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto end;
	}

	decon_info("%s ...\n", __func__);

	if (!decon->low_persistence)
		decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_DISABLE);

end:
	DPU_EVENT_LOG(DPU_EVT_FENCE_RELEASE, &decon->sd, ktime_set(0, 0));

#if defined(CONFIG_EXYNOS_BTS)
	/* add update bw : cur < prev */
	decon->bts.ops->bts_update_bw(decon, regs, 1);
#endif

	decon_dpp_stop(decon, false);

	decon_info("%s -\n", __func__);

	return ret;
}

static void decon_update_regs_handler(struct kthread_work *work)
{
	struct decon_update_regs *up =
			container_of(work, struct decon_update_regs, work);
	struct decon_device *decon =
			container_of(up, struct decon_device, up);

	struct decon_reg_data *data, *next;
	struct list_head saved_list;

	mutex_lock(&decon->up.lock);
	decon->up.saved_list = decon->up.list;
	saved_list = decon->up.list;
	if (!decon->up_list_saved)
		list_replace_init(&decon->up.list, &saved_list);
	else
		list_replace(&decon->up.list, &saved_list);
	mutex_unlock(&decon->up.lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		decon_systrace(decon, 'C', "update_regs_list", 1);
		if (data->fps_update & VRR_UPDATE) {
			decon_update_fps(decon, data);
		} else {
			decon_set_cursor_reset(decon, data);
			decon_update_regs(decon, data);
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
			memcpy(&decon->last_regs, data, sizeof(struct decon_reg_data));
#endif
		}
		decon_hiber_unblock(decon);

		if (!decon->up_list_saved) {
			list_del(&data->list);
			decon_systrace(decon, 'C',
					"update_regs_list", 0);
			kfree(data);
			atomic_dec(&decon->up.remaining_frame);
		}
	}
}

static int decon_get_active_win_count(struct decon_device *decon,
		struct decon_win_config_data *win_data, bool *readback_req)
{
	int i;
	int win_cnt = 0;
	struct decon_win_config *config;
	struct decon_win_config *win_config = win_data->config;

	for (i = 0; i < decon->dt.max_win; i++) {
		config = &win_config[i];

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			break;

		case DECON_WIN_STATE_COLOR:
		case DECON_WIN_STATE_BUFFER:
		case DECON_WIN_STATE_CURSOR:
			win_cnt++;
			break;

		default:
			decon_warn("DECON:WARN:%s:unrecognized window state %u",
					__func__, config->state);
			break;
		}
	}

	*readback_req = false;
#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	if (win_config[decon->dt.wb_win].state == DECON_WIN_STATE_BUFFER) {
		if (decon->dt.out_type != DECON_OUT_WB) {
			decon_dbg("Readback Buffer Frame!\n");
			*readback_req = true;
		}
	}
#endif

	return win_cnt;
}

void decon_set_full_size_win(struct decon_device *decon,
	struct decon_win_config *config)
{
	config->dst.x = 0;
	config->dst.y = 0;
	config->dst.w = decon->lcd_info->xres;
	config->dst.h = decon->lcd_info->yres;
	config->dst.f_w = decon->lcd_info->xres;
	config->dst.f_h = decon->lcd_info->yres;
}

int decon_check_global_limitation(struct decon_device *decon,
		struct decon_win_config *config)
{
	int i, j;
	int ret = 0;

	for (i = 0; i < MAX_DECON_WIN; i++) {
		if (config[i].state != DECON_WIN_STATE_BUFFER)
			continue;

		if (config[i].channel < 0 ||
				config[i].channel >= decon->dt.dpp_cnt) {
			ret = -EINVAL;
			decon_err("invalid dpp ch(%d)\n", config[i].channel);
			goto err;
		}

		for (j = 0; j < MAX_DECON_WIN; ++j) {
			if (config[j].state != DECON_WIN_STATE_BUFFER)
				continue;
			if (i == j)
				continue;

			if (config[i].channel == config[j].channel) {
				ret = -EINVAL;
				decon_err("win%d and %d try to connect ch%d\n",
						i, j, config[i].channel);
				goto err;
			}
		}
	}

	ret = decon_reg_check_global_limitation(decon, config);

err:
	return ret;
}

static int decon_prepare_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data,
		struct decon_reg_data *regs)
{
	int ret = 0;
	int i;
	bool color_map;
	struct decon_win_config *win_config = win_data->config;
	struct decon_win_config *config;
	struct decon_window_regs *win_regs;
	int cfg_cnt;
	struct dma_fence *fence = NULL;

	decon_dbg("%s +\n", __func__);

	ret = decon_check_global_limitation(decon, win_config);
	if (ret)
		goto config_err;

	for (i = 0; i < decon->dt.max_win && !ret; i++) {
		config = &win_config[i];
		win_regs = &regs->win_regs[i];
		color_map = true;

		switch (config->state) {
		case DECON_WIN_STATE_DISABLED:
			win_regs->wincon &= ~WIN_EN_F(i);
			break;
		case DECON_WIN_STATE_COLOR:
			regs->num_of_window++;
			config->color |= (0xFF << 24);
			win_regs->colormap = config->color;

			/* decon_set_full_size_win(decon, config); */
			decon_win_config_to_regs_param(0, config, win_regs,
					config->channel, i); /* ch */
			ret = 0;
			break;
		case DECON_WIN_STATE_BUFFER:
		case DECON_WIN_STATE_CURSOR:	/* cursor async */
			ret = decon_check_limitation(decon, i, config);
			if (!ret) {
				if (decon_set_win_blocking_mode(decon, i, win_config, regs))
					break;

				regs->num_of_window++;
				ret = decon_set_win_buffer(decon, config, regs, i);
				if (!ret)
					color_map = false;
			}
			regs->is_cursor_win[i] = false;
			if (config->state == DECON_WIN_STATE_CURSOR) {
				regs->is_cursor_win[i] = true;
				regs->cursor_win = i;
			}
			config->state = DECON_WIN_STATE_BUFFER;
			break;
		default:
			win_regs->wincon &= ~WIN_EN_F(i);
			decon_warn("unrecognized window state %u",
					config->state);
			ret = -EINVAL;
			break;
		}
		win_regs->winmap_state = color_map;
	}

#ifdef DEBUG_DMA_BUF_LEAK
	regs->import_time = ktime_get();
#endif

	/* handle standalone-WB/concurrent-WB case */
	cfg_cnt = decon->dt.dpp_cnt;
	if (decon->dt.out_type == DECON_OUT_WB || regs->readback.request) {
		cfg_cnt = decon->dt.dpp_cnt + 1;
		config = &win_config[decon->dt.wb_win];
		regs->protection[decon->dt.wb_win] = config->protection;
		ret = decon_import_buffer(decon, decon->dt.wb_win,
				config, regs);

		if (!ret && regs->readback.request)
			decon->readback.map_cnt++;

		if (regs->readback.request && (config->rel_fence >= 0)) {
			/* fence is managed by buffer not plane */
			fence = sync_file_get_fence(config->rel_fence);
			regs->dma_buf_data[decon->dt.wb_win][0].fence = fence;
			if (!fence) {
				decon_err("failed to import fence fd\n");
				ret = -EINVAL;
				goto config_err;
			}
			decon_dbg("rel_fence(%d), fence(%p)\n",
				config->rel_fence, fence);
		}

		decon_win_config_to_regs_param(0, config,
				&regs->win_regs[decon->dt.wb_win], ODMA_WB, 0);
	}

	for (i = 0; i < cfg_cnt; i++)
		memcpy(&regs->dpp_config[i], &win_config[i],
				sizeof(struct decon_win_config));

config_err:

	decon_dbg("%s -\n", __func__);

	return ret;
}

/* To check original win_config data delivered from HWC
 * - idx   : to find a matching up_handler
 *           if (NORMAL) updated, else (-1)
 * - fps   : newly requested fps or existing fps info
 * - winup : Partial or MRES config info
 */
static void decon_save_win_config_event(struct decon_device *decon,
		struct decon_win_config_data *win_data, enum dpu_wc_id id)
{
	decon->win_raw.id = id;
	decon->win_raw.idx = -1;
	decon->win_raw.fps = decon->lcd_info->fps;

	if (id & WC_ID_FAIL) {
		decon->win_raw.state = WC_ID_FAIL;
		memset(&decon->win_raw.winup, 0,
				sizeof(struct decon_frame));
	} else if (id & (WC_ID_SKIP | WC_ID_NORMAL)) {
		if (id & WC_ID_NORMAL)
			decon->win_raw.idx = decon->wc_idx;
		decon->win_raw.fps = win_data->fps;
		if (decon->win_raw.fps != 0)
			decon->win_raw.id |= WC_ID_FPS;
		decon->win_raw.state =
				win_data->config[DECON_WIN_UPDATE_IDX].state;
		memcpy(&decon->win_raw.winup,
				&win_data->config[DECON_WIN_UPDATE_IDX].dst,
				sizeof(struct decon_frame));
	} else {
		decon_err("Unknown Win_Config ID(%d)\n", id);
		memset(&decon->win_raw.winup, 0,
				sizeof(struct decon_frame));
	}

	DPU_EVENT_LOG(DPU_EVT_WIN_CONFIG, &decon->sd, ktime_set(0, 0));
}

#ifndef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
static int decon_set_vrr(struct decon_device *decon,
	struct decon_win_config_data *win_data, struct decon_reg_data *regs)
{
	unsigned int vrr_state;
	struct vrr_config_data  *vrr_config;

	regs->fps_update = 0;
	vrr_config = &regs->vrr_config;
	vrr_state = win_data->config[DECON_WIN_UPDATE_IDX].state;
	if ((vrr_state != DECON_WIN_STATE_VRR_NORMALMODE) &&
		(vrr_state != DECON_WIN_STATE_VRR_HSMODE)) {
		decon_err("[VRR] %s:invalid win_state(%x)\n",
				__func__, vrr_state);
		return -EINVAL;
	}

	if (win_data->fps == 0) {
		decon_err("[VRR] %s:invalid fps(%d)\n",
				__func__, win_data->fps);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_EXYNOS_FPS_CHANGE_NOTIFY)
	if (vrr_config->fps != win_data->fps)
		notify_fps_change(win_data->fps);
#endif
	vrr_config->fps = win_data->fps;
	vrr_config->mode = (vrr_state == DECON_WIN_STATE_VRR_HSMODE) ?
		WIN_VRR_HS_MODE : WIN_VRR_NORMAL_MODE;
	regs->fps_update = VRR_UPDATE;
	decon_info("[VRR] %s:%d%s\n",
			__func__, vrr_config->fps,
			EXYNOS_VRR_MODE_STR(vrr_config->mode));

	return 0;
}
#endif

static int decon_set_win_config(struct decon_device *decon,
		struct decon_win_config_data *win_data)
{
	int num_of_window = 0;
	struct decon_reg_data *regs;
	struct sync_file *sync_ifile = NULL;
	int i, j, ret = 0;
	bool readback_req = false;
#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	struct sync_file *sync_ofile;
	int readback_fence = -1;
#endif

	decon_dbg("%s +\n", __func__);

	mutex_lock(&decon->lock);

	if (IS_DECON_OFF_STATE(decon) ||
#if defined(CONFIG_EXYNOS_COMMON_PANEL) || \
	defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
		decon_is_bypass(decon) ||
#endif
#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
		decon->state == DECON_STATE_DOZE_WAKE ||
#endif
		decon->state == DECON_STATE_TUI ||
		IS_ENABLED(CONFIG_EXYNOS_VIRTUAL_DISPLAY)) {
		decon_save_win_config_event(decon, win_data, WC_ID_SKIP);
#if defined(CONFIG_EXYNOS_COMMON_PANEL) || \
	defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
		decon_warn("decon-%d skip win_config(state:%s, bypass:%s)\n",
				decon->id, decon_state_names[decon->state],
				decon_is_bypass(decon) ? "on" : "off");
#else
		decon_warn("decon-%d skip win_config(state:%s)\n",
				decon->id, decon_state_names[decon->state]);
#endif
		num_of_window = decon_get_active_win_count(decon, win_data, &readback_req);

		if (num_of_window) {
			win_data->retire_fence = decon_create_fence(decon, &sync_ifile);
			if (win_data->retire_fence < 0)
				goto err;
			fd_install(win_data->retire_fence, sync_ifile->file);
			decon_signal_fence(decon, sync_ifile->fence);
		}

#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
		num_of_window = decon_get_active_win_count(decon, win_data, &readback_req);
		if (readback_req) {
			readback_fence = decon_create_fence(decon, &sync_ofile);
			win_data->config[decon->dt.wb_win].acq_fence = readback_fence;
			if (readback_fence < 0)
				goto err;
			fd_install(readback_fence, sync_ofile->file);
			decon_signal_fence(decon, sync_ofile->fence);
		}
#endif
		goto err;
	}

	regs = kzalloc(sizeof(struct decon_reg_data), GFP_KERNEL);
	if (!regs) {
		decon_err("could not allocate decon_reg_data\n");
		ret = -ENOMEM;
		goto err;
	}
#ifdef CONFIG_PROFILE_WINCONFIG
	regs->create_time = ktime_get();
	regs->remained_frames = atomic_read(&decon->up.remaining_frame);
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	regs->mask_layer = decon_get_mask_layer(decon, win_data);
#endif

	decon->wc_idx++;
	decon_save_win_config_event(decon, win_data, WC_ID_NORMAL);

	num_of_window = decon_get_active_win_count(decon, win_data, &readback_req);
	if (num_of_window) {
		win_data->retire_fence = decon_create_fence(decon, &sync_ifile);
		if (win_data->retire_fence < 0)
			goto err_prepare;
#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
		if (readback_req) {
			regs->readback.request = readback_req;
			decon->win_up.force_full = true;
			readback_fence = decon_create_fence(decon, &sync_ofile);
			win_data->config[decon->dt.wb_win].acq_fence = readback_fence;
			if (readback_fence < 0)
				goto err_prepare;
			regs->readback.fence = dma_fence_get(sync_ofile->fence);
		}
#else
		if (!decon->readback.enabled && readback_req) {
			regs->readback.request = false;
			decon_warn("Not support readback(CWB)!!\n");
		}
#endif
	} else {
		decon_info("%s: fps(%d->%d) update request!\n", __func__,
				decon->lcd_info->fps, win_data->fps);
		win_data->retire_fence = -1;
#ifdef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
		regs->fps = win_data->fps;
#else
		if (decon_set_vrr(decon, win_data, regs) < 0) {
			decon_err("[VRR] %s:failed to set vrr\n", __func__);
			goto err_prepare;
		}
		decon_hiber_block(decon);
		goto add_new_regs;
#endif
	}

	dpu_prepare_win_update_config(decon, win_data, regs);

	ret = decon_prepare_win_config(decon, win_data, regs);
	if (ret) {
		goto err_prepare;
	}
#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	else {
		if ((readback_req) && (readback_fence >= 0))
			fd_install(readback_fence, sync_ofile->file);
	}
#endif

	/*
	 * If dpu_prepare_win_update_config returns error, prev_up_region is
	 * updated but that partial size is not applied to HW in previous code.
	 * So, updating prev_up_region is moved here.
	 */
	memcpy(&decon->win_up.prev_up_region, &regs->up_region,
			sizeof(struct decon_rect));

	if (num_of_window) {
		decon_create_release_fences(decon, win_data, sync_ifile);
		regs->retire_fence = dma_fence_get(sync_ifile->fence);
	}

	decon_hiber_block(decon);
#ifndef CONFIG_DYNAMIC_FREQ
	/*
	 * target m value is updated by user requested m value.
	 * target m value will be applied to DPHY PLL in update handler work
	 */
	dpu_update_freq_hop(decon);
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	if (decon->profile_sd != NULL) {
		v4l2_subdev_call(decon->profile_sd, core, ioctl,
			PROFILE_WIN_CONFIG, win_data);
	}
#endif

#ifndef CONFIG_EXYNOS_SET_ACTIVE_WITH_EMPTY_WINDOW
add_new_regs:
#endif
	mutex_lock(&decon->up.lock);
	list_add_tail(&regs->list, &decon->up.list);
	win_data->extra.remained_frames =
		atomic_inc_return(&decon->up.remaining_frame);
	if (win_data->extra.remained_frames >= 20) {
		decon_info("[DECON%d:WARN]:%s:remaining_frame: %d\n", decon->id,
			__func__, win_data->extra.remained_frames);
	}


	mutex_unlock(&decon->up.lock);

	regs->idx = decon->wc_idx;
	kthread_queue_work(&decon->up.worker, &decon->up.work);
#ifdef CONFIG_SUPPORT_MASK_LAYER
	decon_wait_mask_layer_trigger(decon);
#endif

	/**
	 * The code is moved here because the DPU driver may get a wrong fd
	 * through the released file pointer,
	 * if the user(HWC) closes the fd and releases the file pointer.
	 *
	 * Since the user land can use fd from this point/time,
	 * it can be guaranteed to use an unreleased file pointer
	 * when creating a rel_fence in decon_create_release_fences(...)
	 */
	if (num_of_window)
		fd_install(win_data->retire_fence, sync_ifile->file);

	if (decon->mres_enabled &&
			(win_data->config[DECON_WIN_UPDATE_IDX].state == DECON_WIN_STATE_MRESOL) &&
			((win_data->config[DECON_WIN_UPDATE_IDX].dst.f_w != decon->lcd_info->xres) ||
			 (win_data->config[DECON_WIN_UPDATE_IDX].dst.f_h != decon->lcd_info->yres))) {
		decon_dbg("MRESOL: flush up.worker(%d frames) +\n",
				atomic_read(&decon->up.remaining_frame));
		kthread_flush_worker(&decon->up.worker);
		decon_dbg("MRESOL: flush up.worker -\n");
	}

	mutex_unlock(&decon->lock);
	decon_systrace(decon, 'C', "decon_win_config", 0);

	decon_dbg("%s -\n", __func__);

	return ret;

err_prepare:
	if (win_data->retire_fence >= 0) {
		/* video mode should keep previous buffer object */
		if (decon->lcd_info->mode == DECON_MIPI_COMMAND_MODE)
			decon_signal_fence(decon, sync_ifile->fence);
		fput(sync_ifile->file);
		put_unused_fd(win_data->retire_fence);
	}
	win_data->retire_fence = -1;
	win_data->extra.remained_frames = -1;

#ifdef DEBUG_DMA_BUF_LEAK
	decon->free_cnt = 0;
#endif

	for (i = 0; i < decon->dt.max_win; i++)
		for (j = 0; j < regs->plane_cnt[i]; ++j)
			decon_free_unused_buf(decon, regs, i, j);

#ifdef DEBUG_DMA_BUF_LEAK
	if (regs->import_cnt != decon->free_cnt) {
		decon_info("[DECON:INFO]:%s: import: %d, free:%d\n",
			__func__, regs->import_cnt, decon->free_cnt);

		if (regs->import_cnt > decon->free_cnt) {
			decon_info("[DECON:INFO]:%s dma buf import time : %lld\n",
				__func__, ktime_to_us(regs->import_time));
			logging_dma_leak_info(decon, 2, regs->import_cnt, regs->import_time);
		}
	}
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_READBACK)
	if (readback_req) {
		if (readback_fence >= 0) {
			if (decon->lcd_info->mode == DECON_MIPI_COMMAND_MODE)
				decon_signal_fence(decon, sync_ofile->fence);
			fput(sync_ofile->file);
			put_unused_fd(readback_fence);
		}
		decon_free_unused_buf(decon, regs, decon->dt.wb_win, 0);
	}
#endif

	dpu_show_dma_attach_info("decon_set_win_config", decon, 0);
	kfree(regs);
err:
	mutex_unlock(&decon->lock);
	return ret;
}

static int decon_get_hdr_capa(struct decon_device *decon,
		struct decon_hdr_capabilities *hdr_capa)
{
	int ret = 0;
	int k;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		for (k = 0; k < decon->lcd_info->hdr.num; k++)
			hdr_capa->out_types[k] = decon->lcd_info->hdr.type[k];
	} else if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		decon_displayport_get_hdr_capa(decon, hdr_capa);
#endif
	} else
		memset(hdr_capa, 0, sizeof(struct decon_hdr_capabilities));

	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;
}

static int decon_get_hdr_capa_info(struct decon_device *decon,
		struct decon_hdr_capabilities_info *hdr_capa_info)
{
	int ret = 0;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		hdr_capa_info->out_num = decon->lcd_info->hdr.num;
		hdr_capa_info->max_luminance = decon->lcd_info->hdr.max_luma;
		hdr_capa_info->max_average_luminance =
			decon->lcd_info->hdr.max_avg_luma;
		hdr_capa_info->min_luminance = decon->lcd_info->hdr.min_luma;
	} else if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		decon_displayport_get_hdr_capa_info(decon, hdr_capa_info);
#endif
	} else
		memset(hdr_capa_info, 0, sizeof(struct decon_hdr_capabilities_info));

	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;

}

static int decon_get_color_mode(struct decon_device *decon,
		struct decon_color_mode_info *color_info)
{
	int ret = 0;
	struct panel_color_mode *color_mode = &decon->lcd_info->color_mode;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	if ((color_info->index > color_mode->cnt) ||
		(color_info->index >= MAX_COLOR_MODE) ||
		(color_info->index < 0)) {
		decon_err("DECON%d:ERR:%s:invalied color mode index : %d (max : %d)\n",
			decon->id, __func__, color_info->index, color_mode->cnt);
		mutex_unlock(&decon->lock);
		return -EINVAL;
	}

	color_info->color_mode = color_mode->mode[color_info->index];

	decon_info("decon%d: color mode index : %d : %d\n",
		decon->id, color_info->index, color_info->color_mode);

	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;
}


static int decon_set_color_mode(struct decon_device *decon,
		u32 mode)
{
	int i;
	int ret = 0;
	struct panel_color_mode *color_mode = &decon->lcd_info->color_mode;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	decon_info("DECON%d:INFO:%s:color mode : %d",
		decon->id, __func__, mode);

	for (i = 0; i < color_mode->cnt; i++) {
		if (color_mode->mode[i] == mode) {
			decon->color_mode = mode;
			break;
		}
	}

	if (i >= color_mode->cnt)
		decon_err("DECON:ERR:%s:Not supported color mode : %d\n",
		__func__, mode);

	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;
}

static int decon_get_readback_attribute(struct decon_device *decon,
		struct decon_readback_attribute *readback_attr)
{
	int ret = 0;

	decon_dbg("%s +\n", __func__);
	mutex_lock(&decon->lock);

	if (!decon->readback.enabled) {
		decon_info("Not support readback!\n");
		/* EPERM(1) : Operation not permitted */
		ret = -EPERM;
		goto end;
	}

	readback_attr->format = DECON_PIXEL_FORMAT_RGBX_8888;
	/* transfer[13:9] | range[8:6] | standard[5:0] */
	readback_attr->dataspace = (DPP_TRANSFER_SRGB << CSC_TRANSFER_SHIFT) |
				(CSC_RANGE_FULL << CSC_RANGE_SHIFT) |
				(CSC_DCI_P3 << CSC_STANDARD_SHIFT);

	decon_info("%s: readback_attribute - format(%d) dataspace(0x%x)\n",
		__func__, readback_attr->format, readback_attr->dataspace);

end:
	mutex_unlock(&decon->lock);
	decon_dbg("%s -\n", __func__);

	return ret;
}

static int decon_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct exynos_panel_info *lcd_info = decon->lcd_info;
	struct lcd_mres_info *mres_info = &lcd_info->mres;
	struct decon_win_config_data win_data;
	struct decon_disp_info disp_info;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct exynos_displayport_data displayport_data;
#endif
	struct decon_hdr_capabilities hdr_capa;
	struct decon_hdr_capabilities_info hdr_capa_info;
	struct decon_user_window user_window;	/* cursor async */
	struct decon_disp_info __user *argp_info;
	struct dpp_restrictions_info __user *argp_res;
	struct decon_color_mode_info cm_info;
	struct decon_readback_attribute readback_attr;
	struct decon_edid_data edid_data;
	struct dpp_ch_restriction dpp_ch_restriction;
#ifdef CONFIG_EXYNOS_SET_ACTIVE
	struct exynos_display_mode display_mode;
	struct exynos_display_mode *mode;
#endif
	int ret = 0;
	u32 crtc;
	bool active;
	u32 crc_bit, crc_start;
	u32 crc_data[2];
	enum disp_pwr_mode pwr;
	int i;
	u32 cm_num;
	u32 color;

	decon_hiber_block_exit(decon);
	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		if (get_user(crtc, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		if (crtc == 0)
			ret = decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		else
			ret = -ENODEV;

		break;

	case S3CFB_SET_VSYNC_INT:
		if (get_user(active, (bool __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_vsync_int(info, active);
		break;

	case S3CFB_WIN_CONFIG_OLD:
	case S3CFB_WIN_CONFIG:
		DPU_EVENT_LOG(DPU_EVT_WIN_CONFIG, &decon->sd, ktime_set(0, 0));
		decon_systrace(decon, 'C', "decon_win_config", 1);
		if (copy_from_user(&win_data, (void __user *)arg, _IOC_SIZE(cmd))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_win_config(decon, &win_data);
		if (ret)
			break;

		if (copy_to_user((void __user *)arg, &win_data, _IOC_SIZE(cmd))) {
			ret = -EFAULT;
			break;
		}
		break;

	case S3CFB_GET_HDR_CAPABILITIES:
		ret = decon_get_hdr_capa(decon, &hdr_capa);
		if (ret)
			break;

		if (copy_to_user((struct decon_hdr_capabilities __user *)arg,
				&hdr_capa,
				sizeof(struct decon_hdr_capabilities))) {
			ret = -EFAULT;
			break;
		}
		break;

	case S3CFB_GET_HDR_CAPABILITIES_NUM:
		ret = decon_get_hdr_capa_info(decon, &hdr_capa_info);
		if (ret)
			break;

		if (copy_to_user((struct decon_hdr_capabilities_info __user *)arg,
				&hdr_capa_info,
				sizeof(struct decon_hdr_capabilities_info))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_DISP_INFO:
		argp_info = (struct decon_disp_info  __user *)arg;
		if (copy_from_user(&disp_info, argp_info,
				   sizeof(struct decon_disp_info))) {
			ret = -EFAULT;
			break;
		}

		decon_info("HWC version %d.0 is operating\n", disp_info.ver);
		disp_info.psr_mode = decon->dt.psr_mode;
		disp_info.chip_ver = decon->dt.chip_ver;
		disp_info.mres_info = *mres_info;

		if (copy_to_user(argp_info,
				 &disp_info, sizeof(struct decon_disp_info))) {
			ret = -EFAULT;
			break;
		}
		break;

	case S3CFB_START_CRC:
		if (get_user(crc_start, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}
		mutex_lock(&decon->lock);
		if (!IS_DECON_ON_STATE(decon)) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_reg_set_start_crc(decon->id, crc_start);
		break;

	case S3CFB_SEL_CRC_BITS:
		if (get_user(crc_bit, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}
		mutex_lock(&decon->lock);
		if (!IS_DECON_ON_STATE(decon)) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_reg_set_select_crc_bits(decon->id, crc_bit);
		break;

	case S3CFB_GET_CRC_DATA:
		mutex_lock(&decon->lock);
		if (!IS_DECON_ON_STATE(decon)) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_reg_get_crc_data(decon->id, &crc_data[0], &crc_data[1]);
		if (copy_to_user((u32 __user *)arg, &crc_data[0], sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		break;

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	case EXYNOS_GET_DISPLAYPORT_CONFIG:
		if (copy_from_user(&displayport_data,
				   (struct exynos_displayport_data __user *)arg,
				   sizeof(displayport_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_displayport_get_config(decon, &displayport_data);

		if (copy_to_user((struct exynos_displayport_data __user *)arg,
				&displayport_data, sizeof(displayport_data))) {
			ret = -EFAULT;
			break;
		}

		decon_dbg("DECON DISPLAYPORT IOCTL EXYNOS_GET_DISPLAYPORT_CONFIG\n");
		break;

	case EXYNOS_SET_DISPLAYPORT_CONFIG:
		if (copy_from_user(&displayport_data,
				   (struct exynos_displayport_data __user *)arg,
				   sizeof(displayport_data))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_displayport_set_config(decon, &displayport_data);
		decon_dbg("DECON DISPLAYPORT IOCTL EXYNOS_SET_DISPLAYPORT_CONFIG\n");
		break;

	case DISPLAYPORT_IOC_DP_SA_SORTING:
		decon_info("DISPLAY_IOC_DP_SA_SORTING is called\n");
		ret = displayport_reg_stand_alone_crc_sorting();
		break;
#endif
	case EXYNOS_DPU_DUMP:
		mutex_lock(&decon->lock);
		if (!IS_DECON_ON_STATE(decon)) {
			decon_err("DECON:WRN:%s:decon%d is not active:cmd=%d\n",
				__func__, decon->id, cmd);
			ret = -EIO;
			mutex_unlock(&decon->lock);
			break;
		}
		mutex_unlock(&decon->lock);
		decon_dump(decon, true);
		break;
	case DECON_WIN_CURSOR_POS:	/* cursor async */
		if (copy_from_user(&user_window,
			(struct decon_user_window __user *)arg,
			sizeof(struct decon_user_window))) {
			ret = -EFAULT;
			break;
		}
			decon_dbg("cursor pos : x:%d, y:%d\n",
					user_window.x, user_window.y);
			ret = decon_set_cursor_win_config(decon, user_window.x,
					user_window.y);
		break;

	case S3CFB_POWER_MODE:
		if (!IS_ENABLED(CONFIG_EXYNOS_DOZE)) {
			decon_info("DOZE mode is disabled\n");
			break;
		}

		if (get_user(pwr, (u32 __user *)arg)) {
			ret = -EFAULT;
			break;
		}


		if (pwr != DISP_PWR_DOZE && pwr != DISP_PWR_DOZE_SUSPEND) {
			decon_warn("%s: decon%d invalid pwr_state %d\n",
					__func__, decon->id, pwr);
			break;
		}

		ret = decon_update_pwr_state(decon, pwr);
		if (ret) {
			decon_err("%s: decon%d failed to set doze %d, ret %d\n",
					__func__, decon->id, pwr, ret);
			break;
		}
		break;

	case EXYNOS_DISP_RESTRICTIONS:
		argp_res = (struct dpp_restrictions_info  __user *)arg;

		for (i = 0; i < decon->dt.max_win; ++i)
			v4l2_subdev_call(decon->dpp_sd[i], core, ioctl,
					DPP_GET_RESTRICTION, &disp_res.dpp_ch[i]);

		disp_res.ver = DISP_RESTRICTION_VER;
		disp_res.dpp_cnt = decon->dt.dpp_cnt;

		if (copy_to_user(argp_res, &disp_res,
					sizeof(struct dpp_restrictions_info))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_GET_DPP_CNT:
		if (copy_to_user((u32 __user *)arg, &decon->dt.dpp_cnt,
					sizeof(u32))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_GET_DPP_RESTRICTION:
		if (copy_from_user(&dpp_ch_restriction,
				(struct dpp_ch_restriction __user *)arg,
				sizeof(struct dpp_ch_restriction))) {
			ret = -EFAULT;
			break;
		}

		if ((dpp_ch_restriction.id >= decon->dt.dpp_cnt) || (dpp_ch_restriction.id < 0)) {
			ret = -EINVAL;
			decon_err("invalid DPP(%d) channel number\n", dpp_ch_restriction.id);
			break;
		}

		v4l2_subdev_call(decon->dpp_sd[dpp_ch_restriction.id], core,
				ioctl, DPP_GET_RESTRICTION, &dpp_ch_restriction);
		if (copy_to_user((struct dpp_ch_restriction __user *)arg,
				&dpp_ch_restriction,
				sizeof(struct dpp_ch_restriction))) {
			ret = -EFAULT;
			break;
		}

		break;

	case EXYNOS_GET_COLOR_MODE_NUM:
		cm_num = decon->lcd_info->color_mode.cnt;
		if (copy_to_user((u32 __user *)arg, &cm_num, sizeof(u32)))
			ret = -EFAULT;
		break;

	case EXYNOS_GET_COLOR_MODE:
		if (copy_from_user(&cm_info,
				   (struct decon_color_mode_info __user *)arg,
				   sizeof(struct decon_color_mode_info))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_get_color_mode(decon, &cm_info);
		if (ret)
			break;

		if (copy_to_user((struct decon_color_mode_info __user *)arg,
				&cm_info,
				sizeof(struct decon_color_mode_info))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_SET_COLOR_MODE:
		if (get_user(color, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		ret = decon_set_color_mode(decon, color);
		if (ret)
			break;

		/* ADD additional action if necessary */

		break;

	case EXYNOS_GET_READBACK_ATTRIBUTE:
		if (copy_from_user(&readback_attr,
				   (struct decon_readback_attribute __user *)arg,
				   sizeof(struct decon_readback_attribute))) {
			ret = -EFAULT;
			break;
		}

		ret = decon_get_readback_attribute(decon, &readback_attr);
		if (ret)
			break;

		if (copy_to_user((struct decon_readback_attribute __user *)arg,
				&readback_attr,
				sizeof(struct decon_readback_attribute))) {
			ret = -EFAULT;
			break;
		}

		break;

	case EXYNOS_GET_EDID:
		if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
			ret = decon_displayport_get_edid(decon, &edid_data);

			if (copy_to_user((struct decon_edid_data __user *)arg,
					&edid_data, sizeof(edid_data))) {
				ret = -EFAULT;
				break;
			}
#endif
		} else if (decon->dt.out_type == DECON_OUT_DSI) {
			memset(&edid_data, 0, sizeof(struct decon_edid_data));
			decon_get_edid(decon, &edid_data);
			if (copy_to_user((struct decon_edid_data __user *)arg,
					&edid_data, sizeof(edid_data))) {
				ret = -EFAULT;
				break;
			}
		} else {
			ret = -EFAULT;
		}
		break;

#ifdef CONFIG_EXYNOS_SET_ACTIVE
	case EXYNOS_GET_DISPLAY_MODE_NUM:
		if (copy_to_user((int __user *)arg, &lcd_info->display_mode_count,
					sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		break;

	case EXYNOS_GET_DISPLAY_MODE_OLD:
	case EXYNOS_GET_DISPLAY_MODE:
		if (copy_from_user(&display_mode,
				   (void __user *)arg,
				   _IOC_SIZE(cmd))) {
			ret = -EFAULT;
			break;
		}

		if (display_mode.index >= lcd_info->display_mode_count) {
			decon_err("not valid display mode index(%d)\n",
					display_mode.index);
			ret = -EINVAL;
			break;
		}

		mode = &lcd_info->display_mode[display_mode.index].mode;
		memcpy(&display_mode, mode, sizeof(display_mode));

		decon_info("display mode[%d] : %dx%d@%d(%dx%dmm)\n",
				display_mode.index, mode->width, mode->height,
				mode->fps, mode->mm_width, mode->mm_height);

		if (copy_to_user((void __user *)arg,
					&display_mode, _IOC_SIZE(cmd))) {
			ret = -EFAULT;
			break;
		}
		break;
#endif

	case EXYNOS_GET_DISPLAY_CURRENT_MODE:
		if (copy_to_user((u32 __user *)arg, &lcd_info->cur_mode_idx, sizeof(u32)))
			ret = -EFAULT;
		decon_dbg("EXYNOS_GET_DISPLAY_CURRENT_MODE: current index(%d)\n",
					lcd_info->cur_mode_idx);
		break;

	default:
		decon_err("DECON:ERR:%s:invalid command : 0x%x :: %lx\n", __func__, cmd, S3CFB_WIN_CONFIG);
		ret = -ENOTTY;
	}

	decon_hiber_unblock(decon);
	return ret;
}

static ssize_t decon_fb_read(struct fb_info *info, char __user *buf,
		size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t decon_fb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	return 0;
}

int decon_release(struct fb_info *info, int user)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;

	decon_info("%s + : %d\n", __func__, decon->id);

	if (decon->id && decon->dt.out_type == DECON_OUT_DSI) {
		decon_get_out_sd(decon);
		decon_info("output device of decon%d is changed to %s\n",
				decon->id, decon->out_sd[0]->name);
	}

	if (decon->dt.out_type == DECON_OUT_DSI) {
		decon_hiber_block_exit(decon);
		/* Unused DECON state is DECON_STATE_INIT */
		if (IS_DECON_ON_STATE(decon))
			decon_disable(decon);
		decon_hiber_unblock(decon);
	}

	if (decon->dt.out_type == DECON_OUT_DP) {
		/* Unused DECON state is DECON_STATE_INIT */
		if (IS_DECON_ON_STATE(decon))
			decon_dp_disable(decon);
	}

	decon_info("%s - : %d\n", __func__, decon->id);

	return 0;
}

#ifdef CONFIG_COMPAT
static int decon_compat_ioctl(struct fb_info *info, unsigned int cmd,
		unsigned long arg)
{
	arg = (unsigned long) compat_ptr(arg);
	return decon_ioctl(info, cmd, arg);
}
#endif

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops decon_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= decon_check_var,
	.fb_set_par	= decon_set_par,
	.fb_blank	= decon_blank,
	.fb_setcolreg	= decon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = decon_compat_ioctl,
#endif
	.fb_ioctl	= decon_ioctl,
	.fb_read	= decon_fb_read,
	.fb_write	= decon_fb_write,
	.fb_pan_display	= decon_pan_display,
	.fb_mmap	= decon_mmap,
	.fb_release	= decon_release,
};

/* ---------- POWER MANAGEMENT ----------- */
void decon_clocks_info(struct decon_device *decon)
{
}

void decon_put_clocks(struct decon_device *decon)
{
}

int decon_runtime_resume(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	decon_dbg("decon%d %s +\n", decon->id, __func__);
	clk_prepare_enable(decon->res.aclk);
/*
 * TODO :
 * There was an under-run issue when DECON suspend/resume
 * was operating while DP was operating.
 */

	DPU_EVENT_LOG(DPU_EVT_DECON_RESUME, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

int decon_runtime_suspend(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	decon_dbg("decon%d %s +\n", decon->id, __func__);
	clk_disable_unprepare(decon->res.aclk);

/*
 * TODO :
 * There was an under-run issue when DECON suspend/resume
 * was operating while DP was operating.
 */

	DPU_EVENT_LOG(DPU_EVT_DECON_SUSPEND, &decon->sd, ktime_set(0, 0));
	decon_dbg("decon%d %s -\n", decon->id, __func__);

	return 0;
}

const struct dev_pm_ops decon_pm_ops = {
	.runtime_suspend = decon_runtime_suspend,
	.runtime_resume	 = decon_runtime_resume,
};

static int decon_register_subdevs(struct decon_device *decon)
{
	struct v4l2_device *v4l2_dev = &decon->v4l2_dev;
	int i, ret = 0;

	snprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s",
			dev_name(decon->dev));
	ret = v4l2_device_register(decon->dev, &decon->v4l2_dev);
	if (ret) {
		decon_err("failed to register v4l2 device : %d\n", ret);
		return ret;
	}

	for (i = 0;  i < MAX_DPP_CNT; ++i)
		decon->dpp_sd[i] = NULL;
	ret = dpu_get_sd_by_drvname(decon, DPP_MODULE_NAME);
	if (ret)
		return ret;

	for (i = 0; i < MAX_DSIM_CNT; ++i)
		decon->dsim_sd[i] = NULL;
	ret = dpu_get_sd_by_drvname(decon, DSIM_MODULE_NAME);
	if (ret)
		return ret;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	ret = dpu_get_sd_by_drvname(decon, DISPLAYPORT_MODULE_NAME);
	if (ret)
		return ret;
#endif

#ifdef CONFIG_EXYNOS_COMMON_PANEL
	ret = dpu_get_sd_by_drvname(decon, PANEL_DRV_NAME);
	if (ret)
		return ret;
#endif



	if (!decon->id) {
		/*
		 * TODO: Each sd pointer is registered to v4l2_dev
		 * I'm not sure whether it's necessary or not
		 * If it's necessary, dpp_cnt has to add ODMA if it's used.
		 */
		for (i = 0; i < decon->dt.dpp_cnt; i++) {
			if (IS_ERR_OR_NULL(decon->dpp_sd[i]))
				continue;
			ret = v4l2_device_register_subdev(v4l2_dev,
					decon->dpp_sd[i]);
			if (ret) {
				decon_err("failed to register dpp%d sd\n", i);
				return ret;
			}
		}

		for (i = 0; i < decon->dt.dsim_cnt; i++) {
			if (decon->dsim_sd[i] == NULL)
				continue;

			ret = v4l2_device_register_subdev(v4l2_dev,
					decon->dsim_sd[i]);
			if (ret) {
				decon_err("failed to register dsim%d sd\n", i);
				return ret;
			}
		}
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		ret = v4l2_device_register_subdev(v4l2_dev, decon->displayport_sd);
		if (ret) {
			decon_err("failed to register displayport sd\n");
			return ret;
		}
#endif
	}

	ret = v4l2_device_register_subdev_nodes(&decon->v4l2_dev);
	if (ret) {
		decon_err("failed to make nodes for subdev\n");
		return ret;
	}

	decon_dbg("Register V4L2 subdev nodes for DECON\n");

	if (decon->dt.out_type == DECON_OUT_DSI)
		ret = decon_get_out_sd(decon);
	else if (decon->dt.out_type == DECON_OUT_WB)
		ret = decon_wb_get_out_sd(decon);
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	else if (decon->dt.out_type == DECON_OUT_DP)
		ret = decon_displayport_get_out_sd(decon);
#endif
	else
		ret = -ENODEV;

	return ret;
}

static void decon_unregister_subdevs(struct decon_device *decon)
{
	int i;

	if (!decon->id) {
		for (i = 0; i < decon->dt.dpp_cnt; i++) {
			if (decon->dpp_sd[i] == NULL)
				continue;
			v4l2_device_unregister_subdev(decon->dpp_sd[i]);
		}

		for (i = 0; i < decon->dt.dsim_cnt; i++) {
			if (decon->dsim_sd[i] == NULL)
				continue;
			v4l2_device_unregister_subdev(decon->dsim_sd[i]);
		}

		if (decon->displayport_sd != NULL)
			v4l2_device_unregister_subdev(decon->displayport_sd);
	}

	v4l2_device_unregister(&decon->v4l2_dev);
}

static void decon_release_windows(struct decon_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

static int decon_fb_alloc_memory(struct decon_device *decon, struct decon_win *win)
{
	struct exynos_panel_info *lcd_info = decon->lcd_info;
	struct fb_info *fbi = win->fbinfo;
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	struct displayport_device *displayport;
#endif
	struct dsim_device *dsim;
	struct dpp_device *dpp;
	struct device *dev = NULL;
	unsigned int real_size, virt_size, size;
	dma_addr_t map_dma;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;

	decon_dbg("%s +\n", __func__);
	dev_info(decon->dev, "allocating memory for display\n");

	real_size = lcd_info->xres * lcd_info->yres;
	virt_size = lcd_info->xres * (lcd_info->yres * 2);

	dev_info(decon->dev, "real_size=%u (%u.%u), virt_size=%u (%u.%u)\n",
		real_size, lcd_info->xres, lcd_info->yres,
		virt_size, lcd_info->xres, lcd_info->yres * 2);

	size = (real_size > virt_size) ? real_size : virt_size;
	size *= DEFAULT_BPP / 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->idx);
	buf = ion_alloc_dmabuf("ion_system_heap", (size_t)size, 0);
	if (IS_ERR(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = dma_buf_vmap(buf);
	if (IS_ERR_OR_NULL(vaddr)) {
		dev_err(decon->dev, "dma_buf_vmap() failed\n");
		goto err_map;
	}

	memset(vaddr, 0x00, size);

	fbi->screen_base = vaddr;

	dma_buf_vunmap(buf, vaddr);

	fbi->screen_base = NULL;

	win->dma_buf_data[1].fence = NULL;
	win->dma_buf_data[2].fence = NULL;
	win->plane_cnt = 1;

	if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		displayport = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = displayport->dev;
#endif
	} else if (decon->dt.out_type == DECON_OUT_DSI) {
		dsim = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = dsim->dev;
	} else {	/* writeback case */
		dpp = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = dpp->dev;
	}

	ret = decon_map_ion_handle(decon, dev, &win->dma_buf_data[0],
			buf, win->idx);
	if (!ret)
		goto err_map;

	map_dma = win->dma_buf_data[0].dma_addr;

	dev_info(decon->dev, "alloated memory\n");

	fbi->fix.smem_start = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	decon_dbg("%s -\n", __func__);

	return 0;

err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	return -ENOMEM;
}

#if defined(CONFIG_FB_TEST)
static int decon_fb_test_alloc_memory(struct decon_device *decon, u32 size)
{
	struct fb_info *fbi = decon->win[decon->dt.dft_win]->fbinfo;
	struct decon_win *win = decon->win[decon->dt.dft_win];
	struct displayport_device *displayport;
	struct dpp_device *dpp;
	struct dsim_device *dsim;
	struct device *dev = NULL;
	dma_addr_t map_dma;
	struct dma_buf *buf;
	void *vaddr;
	unsigned int ret;

	decon_dbg("%s +\n", __func__);
	dev_info(decon->dev, "allocating memory for fb test\n");

	size = PAGE_ALIGN(size);
	fbi->fix.smem_len = size;

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->idx);

	buf = ion_alloc_dmabuf("ion_system_heap", (size_t)size, 0);
	if (IS_ERR(buf)) {
		dev_err(decon->dev, "ion_share_dma_buf() failed\n");
		goto err_share_dma_buf;
	}

	vaddr = dma_buf_vmap(buf);
	if (IS_ERR_OR_NULL(v_addr)) {
		dev_err(decon->dev, "dma_buf_vmap() failed\n");
		goto err_map;
	}

	memset(vaddr, 0x00, size);

	fbi->screen_base = vaddr;

	dma_buf_vunmap(buf, vaddr);
	fbi->screen_base = NULL;

	if (decon->dt.out_type == DECON_OUT_DP) {
		displayport = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = displayport->dev;
	} else if (decon->dt.out_type == DECON_OUT_DSI) {
		dsim = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = dsim->dev;
	} else {	/* writeback case */
		dpp = v4l2_get_subdevdata(decon->out_sd[0]);
		dev = dpp->dev;
	}

	ret = decon_map_ion_handle(decon, dev, &win->fb_buf_data,
			buf, win->idx);
	if (!ret)
		goto err_map;

	map_dma = win->fb_buf_data.dma_addr;

	dev_info(decon->dev, "alloated memory\n");
	fbi->fix.smem_start = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);

	decon_dbg("%s -\n", __func__);

	return 0;

err_map:
	dma_buf_put(buf);
err_share_dma_buf:
	return -ENOMEM;
}
#endif

static int decon_acquire_window(struct decon_device *decon, int idx)
{
	struct decon_win *win;
	struct fb_info *fbinfo;
	struct fb_var_screeninfo *var;
	struct exynos_panel_info *lcd_info = decon->lcd_info;
	int ret, i;

	decon_dbg("acquire DECON window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct decon_win), decon->dev);
	if (!fbinfo) {
		decon_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	decon->win[idx] = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	win->decon = decon;
	win->idx = idx;

	if (decon->dt.out_type == DECON_OUT_DSI
		|| decon->dt.out_type == DECON_OUT_DP) {
		win->videomode.left_margin = lcd_info->hbp;
		win->videomode.right_margin = lcd_info->hfp;
		win->videomode.upper_margin = lcd_info->vbp;
		win->videomode.lower_margin = lcd_info->vfp;
		win->videomode.hsync_len = lcd_info->hsa;
		win->videomode.vsync_len = lcd_info->vsa;
		win->videomode.xres = lcd_info->xres;
		win->videomode.yres = lcd_info->yres;
		fb_videomode_to_var(&fbinfo->var, &win->videomode);
	}

	for (i = 0; i < MAX_PLANE_CNT; ++i)
		memset(&win->dma_buf_data[i], 0, sizeof(win->dma_buf_data[i]));

	if (((decon->dt.out_type == DECON_OUT_DSI) || (decon->dt.out_type == DECON_OUT_DP))
			&& (idx == decon->dt.dft_win)) {
		ret = decon_fb_alloc_memory(decon, win);
		if (ret) {
			dev_err(decon->dev, "failed to alloc display memory\n");
			return ret;
		}
#if defined(CONFIG_FB_TEST)
		ret = decon_fb_test_alloc_memory(decon,
				win->fbinfo->fix.smem_len);
		if (ret) {
			dev_err(decon->dev, "failed to alloc test fb memory\n");
			return ret;
		}
#endif
	}

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = DEFAULT_BPP;
	fbinfo->var.width	= lcd_info->width;
	fbinfo->var.height	= lcd_info->height;
	fbinfo->var.yres_virtual = lcd_info->yres * 2;
	fbinfo->fbops		= &decon_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;
	/* 'divide by 8' means converting bit to byte number */
	fbinfo->fix.line_length = fbinfo->var.xres * fbinfo->var.bits_per_pixel / 8;
	fbinfo->fix.ypanstep = 1;
	decon_info("default_win %d win_idx %d xres %d yres %d\n",
			decon->dt.dft_win, idx,
			fbinfo->var.xres, fbinfo->var.yres);

	ret = decon_check_var(&fbinfo->var, fbinfo);
	if (ret < 0) {
		dev_err(decon->dev, "check_var failed on initial video params\n");
		return ret;
	}

	decon_dbg("decon%d window[%d] create\n", decon->id, idx);
	return 0;
}

static int decon_acquire_windows(struct decon_device *decon)
{
	int i, ret;

	for (i = 0; i < decon->dt.max_win; i++) {
		ret = decon_acquire_window(decon, i);
		if (ret < 0) {
			decon_err("failed to create decon-int window[%d]\n", i);
			for (; i >= 0; i--)
				decon_release_windows(decon->win[i]);
			return ret;
		}
	}

	ret = register_framebuffer(decon->win[decon->dt.dft_win]->fbinfo);
	if (ret) {
		decon_err("failed to register framebuffer\n");
		return ret;
	}

	return 0;
}

static void decon_parse_dt(struct decon_device *decon)
{
	struct device_node *te_eint;
	struct device *dev = decon->dev;
	int ret;

	if (!dev->of_node) {
		decon_warn("no device tree information\n");
		return;
	}

	decon->id = of_alias_get_id(dev->of_node, "decon");
	of_property_read_u32(dev->of_node, "max_win",
			&decon->dt.max_win);
	/* used for win_config[wb_win] in case of supporting WB */
	decon->dt.wb_win = decon->dt.max_win + 1;
	of_property_read_u32(dev->of_node, "default_win",
			&decon->dt.dft_win);
	of_property_read_u32(dev->of_node, "default_ch",
			&decon->dt.dft_ch);
	/* video mode: 0, dp: 1 mipi command mode: 2 */
	of_property_read_u32(dev->of_node, "psr_mode",
			&decon->dt.psr_mode);
	/* H/W trigger: 0, S/W trigger: 1 */
	of_property_read_u32(dev->of_node, "trig_mode", &decon->dt.trig_mode);
	decon_info("decon%d: max win%d, %s mode, %s trigger\n",
			decon->id, decon->dt.max_win,
			decon->dt.psr_mode ? "command" : "video",
			decon->dt.trig_mode ? "sw" : "hw");

	/* 0: DSI_MODE_SINGLE, 1: DSI_MODE_DUAL_DSI */
	of_property_read_u32(dev->of_node, "dsi_mode", &decon->dt.dsi_mode);
	decon_info("dsi mode(%d). 0: SINGLE 1: DUAL\n", decon->dt.dsi_mode);

	of_property_read_u32(dev->of_node, "out_type", &decon->dt.out_type);
	decon_info("out type(%d). 0: DSI 1: DISPLAYPORT 2: HDMI 3: WB\n",
			decon->dt.out_type);

	if (of_property_read_u32(dev->of_node, "ppc", (u32 *)&decon->bts.ppc))
		decon->bts.ppc = 2UL;

	decon_info("PPC(%llu)\n", decon->bts.ppc);

	if (of_property_read_u32(dev->of_node, "line_mem_cnt",
				(u32 *)&decon->bts.line_mem_cnt)) {
		decon->bts.line_mem_cnt = 4UL;
		decon_warn("WARN: line memory cnt is not defined in DT.\n");
	}
	decon_info("line memory cnt(%d)\n", decon->bts.line_mem_cnt);

	if (of_property_read_u32(dev->of_node, "cycle_per_line",
				(u32 *)&decon->bts.cycle_per_line)) {
		decon->bts.cycle_per_line = 8UL;
		decon_warn("WARN: cycle per line is not defined in DT.\n");
	}
	decon_info("cycle per line(%d)\n", decon->bts.cycle_per_line);

	of_property_read_u32(dev->of_node, "chip_ver", &decon->dt.chip_ver);
	of_property_read_u32(dev->of_node, "dpp_cnt", &decon->dt.dpp_cnt);
	of_property_read_u32(dev->of_node, "dsim_cnt", &decon->dt.dsim_cnt);
	of_property_read_u32(dev->of_node, "decon_cnt", &decon->dt.decon_cnt);
	decon_info("chip_ver %d, dpp cnt %d, dsim cnt %d\n", decon->dt.chip_ver,
			decon->dt.dpp_cnt, decon->dt.dsim_cnt);

	if (of_property_read_u32(dev->of_node, "cursor_margin",
					&decon->cursor.regset_margin)) {
		decon_info("cursor margin not declared\n");
		decon->cursor.regset_margin = 40;
	}

	if (decon->dt.out_type == DECON_OUT_DSI) {
		ret = of_property_read_u32_index(dev->of_node, "out_idx", 0,
				&decon->dt.out_idx[0]);
		if (ret) {
			decon->dt.out_idx[0] = 0;
			decon_info("failed to parse out_idx[0].\n");
		}
		decon_info("out idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
				decon->dt.out_idx[0]);

		if (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) {
			ret = of_property_read_u32_index(dev->of_node,
					"out_idx", 1, &decon->dt.out_idx[1]);
			if (ret) {
				decon->dt.out_idx[1] = 1;
				decon_info("failed to parse out_idx[1].\n");
			}
			decon_info("out1 idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
					decon->dt.out_idx[1]);
		}

		te_eint = of_get_child_by_name(decon->dev->of_node, "te_eint");
		if (!te_eint) {
			decon_info("No DT node for te_eint\n");
		} else {
			decon->d.eint_pend = of_iomap(te_eint, 0);
			if (!decon->d.eint_pend)
				decon_info("Failed to get te eint pend\n");
		}

	}
#if defined(CONFIG_EXYNOS_PD)
	if (of_property_read_string(dev->of_node, "pd_name", &decon->dt.pd_name)) {
		decon_info("no power domain\n");
		decon->pm_domain = NULL;
	} else {
		decon_info("power domain: %s\n", decon->dt.pd_name);
		decon->pm_domain = exynos_pd_lookup_name(decon->dt.pd_name);
	}
#endif
}

static int decon_init_resources(struct decon_device *decon,
		struct platform_device *pdev, char *name)
{
	struct resource *res;
	int ret = 0;

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	decon->res.regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(decon->res.regs)) {
		decon_err("failed to remap register region\n");
		ret = -ENOENT;
		goto err;
	}

	if (decon->dt.out_type == DECON_OUT_DSI) {
		decon_get_clocks(decon);
		ret = decon_register_irq(decon);
		if (ret)
			goto err;

		if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
			ret = decon_register_ext_irq(decon);
			if (ret)
				goto err;
		}
	} else if (decon->dt.out_type == DECON_OUT_WB) {
		decon_wb_get_clocks(decon);
		ret =  decon_wb_register_irq(decon);
		if (ret)
			goto err;
	} else if (decon->dt.out_type == DECON_OUT_DP) {
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
		decon_displayport_get_clocks(decon);
		ret =  decon_displayport_register_irq(decon);
		if (ret)
			goto err;
#endif
	} else {
		decon_err("not supported output type(%d)\n", decon->dt.out_type);
	}

	decon->res.ss_regs = dpu_get_sysreg_addr();
	if (IS_ERR_OR_NULL(decon->res.ss_regs)) {
		decon_err("failed to get sysreg addr\n");
		goto err;
	}

	return 0;
err:
	return ret;
}

static void decon_destroy_update_thread(struct decon_device *decon)
{
	if (decon->up.thread)
		kthread_stop(decon->up.thread);
}

static int decon_create_update_thread(struct decon_device *decon, char *name)
{
	struct sched_param param;

	INIT_LIST_HEAD(&decon->up.list);
	INIT_LIST_HEAD(&decon->up.saved_list);
	decon->up_list_saved = false;
	atomic_set(&decon->up.remaining_frame, 0);
	kthread_init_worker(&decon->up.worker);
	decon->up.thread = kthread_run(kthread_worker_fn,
			&decon->up.worker, name);
	if (IS_ERR(decon->up.thread)) {
		decon->up.thread = NULL;
		decon_err("failed to run update_regs thread\n");
		return PTR_ERR(decon->up.thread);
	}

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	if (decon->profile_sd != NULL) {
		v4l2_subdev_call(decon->profile_sd, core, ioctl,
			PROFILER_SET_PID, &decon->systrace.pid);
	}
#endif

	param.sched_priority = 20;
	sched_setscheduler_nocheck(decon->up.thread, SCHED_FIFO, &param);
	kthread_init_work(&decon->up.work, decon_update_regs_handler);

	return 0;
}

#if defined(CONFIG_EXYNOS_ITMON)
static int decon_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct decon_device *decon;
	struct itmon_notifier *itmon_data = nb_data;
	struct dsim_device *dsim = NULL;
	bool active;

	decon = container_of(nb, struct decon_device, itmon_nb);

	decon_info("%s: DECON%d +\n", __func__, decon->id);

	if (decon->notified)
		return NOTIFY_DONE;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	/* port is master and dest is target */
	if ((itmon_data->port && (strncmp("DPU", itmon_data->port,
					sizeof("DPU") - 1) == 0)) ||
			(itmon_data->dest && (strncmp("DPU", itmon_data->dest,
					sizeof("DPU") - 1) == 0))) {
		decon_info("%s: port: %s, dest: %s, action: %lu\n", __func__,
				itmon_data->port, itmon_data->dest, action);
		if (decon->dt.out_type == DECON_OUT_DSI) {
			dsim = v4l2_get_subdevdata(decon->out_sd[0]);
			if (IS_ERR_OR_NULL(dsim))
				return NOTIFY_OK;
			active = pm_runtime_active(dsim->dev);
			decon_info("DPU power %s state\n", active ? "on" : "off");
		}

		decon_dump(decon, false);
		decon->notified = true;
		return NOTIFY_OK;
	}

	decon_info("%s -\n", __func__);

	return NOTIFY_DONE;
}
#endif

static int decon_initial_display(struct decon_device *decon, bool is_colormap)
{
	struct decon_param p;
	struct fb_info *fbinfo;
	struct decon_window_regs win_regs;
	struct decon_win_config config;
	struct v4l2_subdev *sd = NULL;
	struct decon_mode_info psr;
	struct dsim_device *dsim;
	struct dsim_device *dsim1;
	struct dpp_config dpp_config;
	unsigned long aclk_khz;
	int dpp_id = decon->dt.dft_ch;
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	int connected;
#endif

	if ((decon->dt.out_type != DECON_OUT_DSI) ||
			IS_ENABLED(CONFIG_EXYNOS_VIRTUAL_DISPLAY)) {
		decon->state = DECON_STATE_OFF;
		decon_info("decon%d doesn't need to display\n", decon->id);
		return 0;
	}

	fbinfo = decon->win[decon->dt.dft_win]->fbinfo;

	pm_stay_awake(decon->dev);
	dev_warn(decon->dev, "pm_stay_awake");

	if (decon->dt.psr_mode != DECON_VIDEO_MODE) {
		if (decon->res.pinctrl && decon->res.hw_te_on) {
			if (pinctrl_select_state(decon->res.pinctrl,
						decon->res.hw_te_on)) {
				decon_err("failed to turn on Decon_TE\n");
				return -EINVAL;
			}
		}
	}

	decon_to_init_param(decon, &p);
	if (decon_reg_init(decon->id, decon->dt.out_idx[0], &p) < 0)
		goto decon_init_done;

#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	decon_info("%s was called\n", __func__);
	if (1) {
		/*
		 * TODO : call_panel_ops should be removed in decon_core.c
		 * keep this hierarchy ( decon - dsim - panel )
		 */
		dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
		connected = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_CONNECTED, NULL);
		if (connected < 0) {
			decon_err("decon-%d: failed to read panel state (ret %d)\n",
					decon->id, connected);
		} else {
			if (!connected)
				decon_bypass_on(decon);

			decon_info("decon-%d: connected:(%s) bypass:(%d)\n",
					decon->id, connected ? "connected" : "detached",
					decon_get_bypass_cnt_global(decon->id));
		}
		goto decon_init_done;
	}
#endif

	memset(&win_regs, 0, sizeof(struct decon_window_regs));
	win_regs.wincon = wincon(decon->dt.dft_win);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, fbinfo->var.xres, fbinfo->var.yres);
	decon_dbg("xres %d yres %d win_start_pos %x win_end_pos %x\n",
			fbinfo->var.xres, fbinfo->var.yres, win_regs.start_pos,
			win_regs.end_pos);
	win_regs.colormap = 0x00ff00;
	win_regs.pixel_count = fbinfo->var.xres * fbinfo->var.yres;
	win_regs.whole_w = fbinfo->var.xres_virtual;
	win_regs.whole_h = fbinfo->var.yres_virtual;
	win_regs.offset_x = fbinfo->var.xoffset;
	win_regs.offset_y = fbinfo->var.yoffset;
	win_regs.ch = dpp_id;
	decon_dbg("pixel_count(%d), whole_w(%d), whole_h(%d), x(%d), y(%d)\n",
			win_regs.pixel_count, win_regs.whole_w,
			win_regs.whole_h, win_regs.offset_x,
			win_regs.offset_y);

	set_bit(dpp_id, &decon->cur_using_dpp);
	set_bit(dpp_id, &decon->prev_used_dpp);
	set_bit(decon->dt.dft_win, &decon->prev_req_win);
	set_bit(decon->dt.dft_win, &decon->cur_req_win);
	memset(&config, 0, sizeof(struct decon_win_config));
	config.dpp_parm.addr[0] = fbinfo->fix.smem_start;
	config.format = DECON_PIXEL_FORMAT_BGRA_8888;
	config.src.w = fbinfo->var.xres;
	config.src.h = fbinfo->var.yres;
	config.src.f_w = fbinfo->var.xres;
	config.src.f_h = fbinfo->var.yres;
	config.dst.w = config.src.w;
	config.dst.h = config.src.h;
	config.dst.f_w = config.src.f_w;
	config.dst.f_h = config.src.f_h;

#if 0
	config.wcg_mode = decon->color_mode;
#endif

	sd = decon->dpp_sd[dpp_id];

	aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			EXYNOS_DPU_GET_ACLK, NULL) / 1000U;

	memcpy(&dpp_config.config, &config, sizeof(struct decon_win_config));
	dpp_config.rcv_num = aclk_khz;

	if (v4l2_subdev_call(sd, core, ioctl, DPP_WIN_CONFIG, &dpp_config)) {
		decon_err("Failed to config DPP-%d\n", dpp_id);
		clear_bit(dpp_id, &decon->cur_using_dpp);
		set_bit(dpp_id, &decon->dpp_err_stat);
	}

	decon_reg_set_window_control(decon->id, decon->dt.dft_win,
			&win_regs, is_colormap);

	decon_reg_all_win_shadow_update_req(decon->id);
	decon_to_psr_info(decon, &psr);

	/* TODO:
	 * 1. If below code is called after turning on 1st LCD.
	 *    2nd LCD is not turned on
	 * 2. It needs small delay between decon start and LCD on
	 *    for avoiding garbage display when dual dsi mode is used. */
	if (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) {
		decon_info("2nd LCD is on\n");
		msleep(1);
		dsim1 = container_of(decon->out_sd[1], struct dsim_device, sd);
		dsim_call_panel_ops(dsim1, EXYNOS_PANEL_IOC_DISPLAYON, NULL);

	}

	dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
	decon_reg_start(decon->id, &psr);
	decon_reg_set_int(decon->id, &psr, 1);
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DISPLAYON, NULL);
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
	if (decon_reg_wait_update_done_and_mask(decon->id, &psr,
				SHADOW_UPDATE_TIMEOUT) < 0)
		decon_err("%s: wait_for_update_timeout\n", __func__);

decon_init_done:

	decon->state = DECON_STATE_INIT;
#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_dqe_sw_reset(decon);
	decon_dqe_enable(decon);
#endif
	return 0;
}

void decon_init_readback(struct decon_device *decon)
{
	decon->readback.enabled = false;

	if (!IS_ENABLED(CONFIG_EXYNOS_SUPPORT_READBACK)) {
		decon_info("display doesn't support readback(CWB)\n");
		return;
	}

	decon->readback.enabled = true;
	decon_info("display supports readback(CWB)\n");

	if (decon->id != 2) {
		/* concurrent writeback can't be enabled in decon2 */
		decon->readback.wq = create_workqueue("decon_readback");
		INIT_WORK(&decon->readback.work, decon_readback_wq);
	}
}

/* --------- DRIVER INITIALIZATION ---------- */
static int decon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon;
	int ret = 0;
	char device_name[MAX_NAME_SIZE];

	dev_info(dev, "%s start\n", __func__);

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon) {
		decon_err("no memory for decon device\n");
		ret = -ENOMEM;
		goto err;
	}

	dma_set_mask(dev, DMA_BIT_MASK(36));

	decon->dev = dev;
	decon_parse_dt(decon);

	decon_drvdata[decon->id] = decon;

	spin_lock_init(&decon->slock);
	init_waitqueue_head(&decon->vsync.wait);
	init_waitqueue_head(&decon->wait_vstatus);
#if defined(CONFIG_EXYNOS_HIBERNATION_THREAD)
	init_waitqueue_head(&decon->hiber.wait);
#endif
#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
	init_waitqueue_head(&decon->doze_hiber.doze_suspend_wait);
	init_waitqueue_head(&decon->doze_hiber.doze_wake_wait);
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	init_waitqueue_head(&decon->wait_mask_layer_trigger_queue);
#endif
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	init_waitqueue_head(&decon->fsync.wait);
	mutex_init(&decon->fsync.lock);
	decon->fsync.active = true;
#endif
	mutex_init(&decon->vsync.lock);
	mutex_init(&decon->pwr_state_lock);
	mutex_init(&decon->lock);
	mutex_init(&decon->pm_lock);
	mutex_init(&decon->up.lock);
	/* cursor async */
	mutex_init(&decon->cursor.unmask_lock);
	spin_lock_init(&decon->cursor.pos_lock);
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	mutex_init(&decon->esd.lock);
#endif
#if defined(CONFIG_EXYNOS_COMMON_PANEL) || \
		defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	decon_set_bypass(decon, false);
#endif

	decon_enter_shutdown_reset(decon);

	snprintf(device_name, MAX_NAME_SIZE, "decon%d", decon->id);
	decon_create_timeline(decon, device_name);

#if defined(CONFIG_DECON_VRR_MODULATION)
	/* mod count */
	decon->vsync.div_count = 1ULL;
#endif

	/* systrace */
	decon_systrace_enable = 0;
	decon->systrace.pid = 0;

	ret = decon_init_resources(decon, pdev, device_name);
	if (ret)
		goto err_res;

	ret = decon_create_vsync_thread(decon);
	if (ret)
		goto err_vsync;

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	ret = decon_displayport_create_vsync_thread(decon);
	if (ret)
		goto err_vsync;
#endif

#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	ret = decon_create_fsync_thread(decon);
	if (ret)
		goto err_fsync;
#endif

	ret = decon_create_psr_info(decon);
	if (ret)
		goto err_psr;

	ret = decon_get_pinctrl(decon);
	if (ret)
		goto err_pinctrl;

	ret = decon_create_debugfs(decon);
	if (ret)
		goto err_pinctrl;

#if defined(CONFIG_EXYNOS_DOZE_HIBERNATION)
	ret = decon_register_doze_hiber_work(decon);
	if (ret)
		goto err_pinctrl;
#endif

	ret = decon_register_hiber_work(decon);
	if (ret)
		goto err_pinctrl;

	ret = decon_register_subdevs(decon);
	if (ret)
		goto err_subdev;

	ret = decon_acquire_windows(decon);
	if (ret)
		goto err_win;

	ret = decon_create_update_thread(decon, device_name);
	if (ret)
		goto err_win;

#ifdef CONFIG_EXYNOS_MCD_HDR
	decon_init_hdr_info(decon);

	ret = create_wcg_sysfs(decon);
	if (ret)
		decon_err("DECON:ERR:%s:failed to create sysfs for wcg\n", __func__);
#endif
	dpu_init_win_update(decon);
	decon_init_low_persistence_mode(decon);
	dpu_init_cursor_mode(decon);
	decon_init_readback(decon);

#if defined(CONFIG_EXYNOS_BTS)
	decon->bts.ops = &decon_bts_control;
	decon->bts.ops->bts_init(decon);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_FPS_CHANGE_NOTIFY)
	if (decon->id == 0)
		ATOMIC_INIT_NOTIFIER_HEAD(&decon->fps_change_notifier_list);
#endif

	platform_set_drvdata(pdev, decon);
	pm_runtime_enable(dev);

	/* prevent sleep enter during display(LCD, DP) on */
	ret = device_init_wakeup(decon->dev, true);
	if (ret) {
		dev_err(decon->dev, "failed to init wakeup device\n");
		goto err_display;
	}

#if defined(CONFIG_EXYNOS_ITMON)
	decon->itmon_nb.notifier_call = decon_itmon_notifier;
	itmon_notifier_chain_register(&decon->itmon_nb);
#endif

#ifdef CONFIG_LOGGING_BIGDATA_BUG
#ifdef CONFIG_DISPLAY_USE_INFO
	decon->dpui_notif.notifier_call = decon_dpui_notifier_callback;
	ret = dpui_logging_register(&decon->dpui_notif, DPUI_TYPE_CTRL);
	if (ret)
		panel_err("ERR:PANEL:%s:failed to register dpui notifier callback\n", __func__);
#endif
#endif /* CONFIG_LOGGING_BIGDATA_BUG */

	dpu_init_freq_hop(decon);

#if defined(CONFIG_EXYNOS_DECON_DQE)
	decon_dqe_create_interface(decon);
#endif

	ret = decon_initial_display(decon, false);
	if (ret)
		goto err_display;

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	ret = decon_create_esd_thread(decon);
	if (ret)
		goto err_esd;
#endif

	decon_info("decon%d registered successfully", decon->id);

	return 0;

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
err_esd:
	decon_destroy_esd_thread(decon);
#endif
err_display:
	decon_destroy_update_thread(decon);
err_win:
	decon_unregister_subdevs(decon);
err_subdev:
	decon_destroy_debugfs(decon);
err_pinctrl:
	decon_destroy_psr_info(decon);
err_psr:
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	decon_destroy_fsync_thread(decon);
err_fsync:
#endif
	decon_destroy_vsync_thread(decon);
err_vsync:
	iounmap(decon->res.ss_regs);
err_res:
	kfree(decon);
err:
	decon_err("decon probe fail");
	return ret;
}

static int decon_remove(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);
	int i;

	decon->bts.ops->bts_deinit(decon);

	pm_runtime_disable(&pdev->dev);
	decon_put_clocks(decon);
	unregister_framebuffer(decon->win[0]->fbinfo);

	if (decon->up.thread)
		kthread_stop(decon->up.thread);

	for (i = 0; i < decon->dt.max_win; i++)
		decon_release_windows(decon->win[i]);

	debugfs_remove_recursive(decon->d.debug_root);
	kfree(decon->d.event_log);

	decon_info("remove sucessful\n");
	return 0;
}

static void decon_shutdown(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);
	struct fb_info *fbinfo = decon->win[decon->dt.dft_win]->fbinfo;

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	mutex_lock(&decon->esd.lock);
#endif
	decon_enter_shutdown(decon);

	if (!lock_fb_info(fbinfo)) {
		decon_warn("%s: fblock is failed\n", __func__);
		return;
	}

	decon_info("%s + state:%d\n", __func__, decon->state);
	DPU_EVENT_LOG(DPU_EVT_DECON_SHUTDOWN, &decon->sd, ktime_set(0, 0));

	decon_hiber_block_exit(decon);
	/* Unused DECON state is DECON_STATE_INIT */
	if (IS_DECON_ON_STATE(decon))
		decon_disable(decon);

	unlock_fb_info(fbinfo);

	decon_info("%s -\n", __func__);
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	mutex_unlock(&decon->esd.lock);
#endif
	return;
}

static const struct of_device_id decon_of_match[] = {
	{ .compatible = "samsung,exynos9-decon" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_of_match);

static struct platform_driver decon_driver __refdata = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.shutdown	= decon_shutdown,
	.driver = {
		.name	= DECON_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &decon_pm_ops,
		.of_match_table = of_match_ptr(decon_of_match),
		.suppress_bind_attrs = true,
	}
};

static int exynos_decon_register(void)
{
	platform_driver_register(&decon_driver);

	return 0;
}

static void exynos_decon_unregister(void)
{
	platform_driver_unregister(&decon_driver);
}
late_initcall(exynos_decon_register);
module_exit(exynos_decon_unregister);

MODULE_AUTHOR("Jaehoe Yang <jaehoe.yang@samsung.com>");
MODULE_AUTHOR("Yeongran Shin <yr613.shin@samsung.com>");
MODULE_AUTHOR("Minho Kim <m8891.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DECON driver");
MODULE_LICENSE("GPL");
