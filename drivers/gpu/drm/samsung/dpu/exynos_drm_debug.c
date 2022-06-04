// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPU Event log file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <video/mipi_display.h>
#include <drm/drm_print.h>
#include <drm/drm_atomic.h>

#include <exynos_drm_debug.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_format.h>
#include <exynos_drm_partial.h>
#include <exynos_drm_tui.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_recovery.h>
#include <cal_config.h>
#include <decon_cal.h>
#include <dt-bindings/soc/samsung/s5e9925-devfreq.h>
#include <soc/samsung/exynos-devfreq.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif

/* Default is 1024 entries array for event log buffer */
static unsigned int dpu_event_log_max = 1024;
static unsigned int dpu_event_print_max = 512;

module_param_named(event_log_max, dpu_event_log_max, uint, 0);
module_param_named(event_print_max, dpu_event_print_max, uint, 0600);
MODULE_PARM_DESC(event_log_max, "entry count of event log buffer array");
MODULE_PARM_DESC(event_print_max, "print entry count of event log buffer");

/* If event are happened continuously, then ignore */
static bool dpu_event_ignore
	(enum dpu_event_type type, struct dpu_debug *d)
{
	int latest = atomic_read(&d->event_log_idx) % dpu_event_log_max;
	int idx, offset;

	if (IS_ERR_OR_NULL(d->event_log))
		return true;

	for (offset = 0; offset < DPU_EVENT_KEEP_CNT; ++offset) {
		idx = (latest + dpu_event_log_max - offset) % dpu_event_log_max;
		if (type != d->event_log[idx].type)
			return false;
	}

	return true;
}

static void dpu_event_save_freqs(struct dpu_log_freqs *freqs)
{
	freqs->mif_freq = exynos_devfreq_get_domain_freq(DEVFREQ_MIF);
	freqs->int_freq = exynos_devfreq_get_domain_freq(DEVFREQ_INT);
	freqs->disp_freq = exynos_devfreq_get_domain_freq(DEVFREQ_DISP);
}

/* ===== EXTERN APIs ===== */

/*
 * DPU_EVENT_LOG() - store information to log buffer by common API
 * @type: event type
 * @exynos_crtc: object to store event log, it can be NULL
 * @priv: pointer to DECON, DSIM or DPP device structure, it can be NULL
 *
 * Store information related to DECON, DSIM or DPP. Each DECON has event log
 * So, DECON id is used as @index
 */
void DPU_EVENT_LOG(enum dpu_event_type type, struct exynos_drm_crtc *exynos_crtc,
		void *priv)
{
	struct dpu_debug *d;
	struct exynos_drm_plane *exynos_plane = NULL;
	struct dpu_log *log;
	struct drm_crtc_state *crtc_state;
	struct exynos_partial *partial;
	struct drm_rect *partial_region;
	struct resolution_info *res_info;
	struct exynos_recovery_cond *recovery_info;
	unsigned long flags;
	int idx;

	if (!exynos_crtc)
		return;

	d = &exynos_crtc->d;

	switch (type) {
	case DPU_EVT_DSIM_UNDERRUN:
		d->underrun_cnt++;
	case DPU_EVT_TE_INTERRUPT:
		/*
		 * If the same event occurs DPU_EVENT_KEEP_CNT times
		 * continuously, it will be skipped.
		 */
		if (dpu_event_ignore(type, &exynos_crtc->d))
			return;
		break;
	default:
		break;
	}

	spin_lock_irqsave(&d->event_lock, flags);
	idx = atomic_inc_return(&d->event_log_idx) % dpu_event_log_max;
	log = &d->event_log[idx];
	spin_unlock_irqrestore(&d->event_lock, flags);

	log->time = ktime_get();
	log->type = type;

	switch (type) {
	case DPU_EVT_DPP_FRAMEDONE:
		exynos_plane = (struct exynos_drm_plane *)priv;
		log->data.dpp.id = exynos_plane->base.index;
		break;
	case DPU_EVT_DMA_RECOVERY:
		exynos_plane = (struct exynos_drm_plane *)priv;
		log->data.dpp.id = exynos_plane->base.index;
		log->data.dpp.comp_src = exynos_plane->comp_src;
		log->data.dpp.recovery_cnt = exynos_plane->recovery_cnt;
		break;
	case DPU_EVT_DECON_RSC_OCCUPANCY:
		pm_runtime_get_sync(exynos_crtc->dev);
		log->data.rsc.rsc_ch = decon_reg_get_rsc_ch(exynos_crtc->base.index);
		log->data.rsc.rsc_win = decon_reg_get_rsc_win(exynos_crtc->base.index);
		pm_runtime_put_sync(exynos_crtc->dev);
		break;
	case DPU_EVT_ENTER_HIBERNATION_IN:
	case DPU_EVT_ENTER_HIBERNATION_OUT:
	case DPU_EVT_EXIT_HIBERNATION_IN:
	case DPU_EVT_EXIT_HIBERNATION_OUT:
		log->data.pd.rpm_active = pm_runtime_active(exynos_crtc->dev);
		break;
	case DPU_EVT_PLANE_UPDATE:
	case DPU_EVT_PLANE_DISABLE:
		exynos_plane = (struct exynos_drm_plane *)priv;
		log->data.win.win_idx = exynos_plane->win_id;
		log->data.win.plane_idx = exynos_plane->base.index;
		break;
	case DPU_EVT_REQ_CRTC_INFO_OLD:
	case DPU_EVT_REQ_CRTC_INFO_NEW:
		crtc_state = (struct drm_crtc_state *)priv;
		log->data.crtc_info.enable = crtc_state->enable;
		log->data.crtc_info.active = crtc_state->active;
		log->data.crtc_info.planes_changed = crtc_state->planes_changed;
		log->data.crtc_info.mode_changed = crtc_state->mode_changed;
		log->data.crtc_info.active_changed = crtc_state->active_changed;
		log->data.crtc_info.color_mgmt_changed =
					crtc_state->color_mgmt_changed;
		break;
	case DPU_EVT_BTS_RELEASE_BW:
	case DPU_EVT_BTS_UPDATE_BW:
		dpu_event_save_freqs(&log->data.freqs);
		break;
	case DPU_EVT_BTS_CALC_BW:
		dpu_event_save_freqs(&log->data.bts.freqs);
		log->data.bts.calc_disp = *(unsigned int *)priv;
		break;
	case DPU_EVT_DSIM_UNDERRUN:
		dpu_event_save_freqs(&log->data.underrun.freqs);
		log->data.underrun.underrun_cnt = d->underrun_cnt;
		break;
	case DPU_EVT_NEXT_ADJ_VBLANK:
		log->data.timestamp = *(ktime_t *)priv;
		break;
	case DPU_EVT_PARTIAL_INIT:
		partial = priv;
		log->data.partial.min_w = partial->min_w;
		log->data.partial.min_h = partial->min_h;
		break;
	case DPU_EVT_PARTIAL_PREPARE:
		memcpy(&log->data.partial, priv, sizeof(struct dpu_log_partial));
		break;
	case DPU_EVT_PARTIAL_RESTORE:
	case DPU_EVT_PARTIAL_UPDATE:
		partial_region = priv;
		memcpy(&log->data.partial.prev, partial_region,
				sizeof(struct drm_rect));
		break;
	case DPU_EVT_TUI_ENTER:
		res_info = (struct resolution_info *)priv;
		log->data.tui.xres = res_info->xres;
		log->data.tui.yres = res_info->yres;
		log->data.tui.mode = res_info->mode;
		break;
	case DPU_EVT_WB_ATOMIC_COMMIT:
		log->data.atomic.win_config[0] = *(struct dpu_bts_win_config *)priv;
		break;
	case DPU_EVT_RECOVERY_START:
	case DPU_EVT_RECOVERY_END:
		recovery_info = (struct exynos_recovery_cond *)priv;
		log->data.recovery.id = recovery_info->id;
		log->data.recovery.count = recovery_info->count;
		break;
	default:
		break;
	}
}

/*
 * DPU_EVENT_LOG_ATOMIC_COMMIT() - store all windows information
 * @exynos_crtc: object to store event log
 *
 * Store all windows information which includes window id, DVA, source and
 * destination coordinates, connected DPP and so on
 */
void DPU_EVENT_LOG_ATOMIC_COMMIT(struct exynos_drm_crtc *exynos_crtc)
{
	struct dpu_debug *d = &exynos_crtc->d;
	struct dpu_log *log;
	struct dpu_bts_win_config *win_config;
	unsigned long flags;
	int idx, i, win_cnt = get_plane_num(exynos_crtc->base.dev);

	if (IS_ERR_OR_NULL(d->event_log))
		return;

	spin_lock_irqsave(&d->event_lock, flags);
	idx = atomic_inc_return(&d->event_log_idx) % dpu_event_log_max;
	log = &d->event_log[idx];
	spin_unlock_irqrestore(&d->event_lock, flags);

	log->type = DPU_EVT_ATOMIC_COMMIT;
	log->time = ktime_get();

	win_config = log->data.atomic.win_config;
	for (i = 0; i < win_cnt; ++i)
		win_config[i] = exynos_crtc->bts->win_config[i];
}

#if defined(CONFIG_UML)
static inline void *return_address(unsigned int level) { return NULL; }
#else
extern void *return_address(unsigned int);
#endif

/*
 * DPU_EVENT_LOG_CMD() - store DSIM command information
 * @exynos_crtc: object to store event log, it can be NULL
 * @cmd_id : DSIM command id
 * @data: command buffer data
 *
 * Stores command id and first data in command buffer and return addresses
 * in callstack which lets you know who called this function.
 */
void DPU_EVENT_LOG_CMD(struct exynos_drm_crtc *exynos_crtc, u32 cmd_id,
		unsigned long data)
{
	struct dpu_debug *d;
	struct dpu_log *log;
	unsigned long flags;
	int idx;
	unsigned int i;

	if (!exynos_crtc)
		return;

	d = &exynos_crtc->d;

	spin_lock_irqsave(&d->event_lock, flags);
	idx = atomic_inc_return(&d->event_log_idx) % dpu_event_log_max;
	log = &d->event_log[idx];
	spin_unlock_irqrestore(&d->event_lock, flags);

	log->type = DPU_EVT_DSIM_COMMAND;
	log->time = ktime_get();

	log->data.cmd.id = cmd_id;
	if (cmd_id == MIPI_DSI_DCS_LONG_WRITE)
		log->data.cmd.buf = *(u8 *)(data);
	else
		log->data.cmd.buf = (u8)data;

	for (i = 0; i < DPU_CALLSTACK_MAX; i++)
		log->data.cmd.caller[i] =
			(void *)((size_t)return_address(i + 1));
}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
void DPU_EVENT_LOG_FREQ_HOP(struct exynos_drm_crtc *exynos_crtc, u32 orig_m, u32 orig_k, u32 target_m, u32 target_k)
{
	struct dpu_debug *d = &exynos_crtc->d;
	struct dpu_log *log;
	unsigned long flags;
	int idx;
	struct dpu_log_freq_hop *freq_hop;

	if (IS_ERR_OR_NULL(d->event_log))
		return;

	spin_lock_irqsave(&d->event_lock, flags);
	idx = atomic_inc_return(&d->event_log_idx) % dpu_event_log_max;
	log = &d->event_log[idx];
	spin_unlock_irqrestore(&d->event_lock, flags);

	log->type = DPU_EVT_FREQ_HOP;
	log->time = ktime_get();

	freq_hop = &log->data.freq_hop;
	freq_hop->orig_m = orig_m;
	freq_hop->orig_k = orig_k;
	freq_hop->target_m = target_m;
	freq_hop->target_k = target_k;

}


void DPU_EVENT_LOG_MODE_SET(struct exynos_drm_crtc *exynos_crtc, u32 event, char *mode_name)
{
	struct dpu_debug *d = &exynos_crtc->d;
	struct dpu_log *log;
	unsigned long flags;
	int idx;
	struct dpu_log_mode_set *mode_set;

	if (IS_ERR_OR_NULL(d->event_log))
		return;

	spin_lock_irqsave(&d->event_lock, flags);
	idx = atomic_inc_return(&d->event_log_idx) % dpu_event_log_max;
	log = &d->event_log[idx];
	spin_unlock_irqrestore(&d->event_lock, flags);

	log->type = DPU_EVT_MODE_SET;
	log->time = ktime_get();

	mode_set = &log->data.mode_set;
	mode_set->event = event;
	strncpy(mode_set->mode, mode_name, DRM_DISPLAY_MODE_LEN);

}

#endif

static void __dpu_print_log_atomic(struct dpu_bts_win_config *win,
		int index, struct drm_printer *p)
{
	char *str_state[3] = {"DISABLED", "COLOR", "BUFFER"};
	const char *str_comp;
	const struct dpu_fmt *fmt;
	char buf[128];
	int len;

	if (win->state == DPU_WIN_STATE_DISABLED)
		return;

	fmt = dpu_find_fmt_info(win->format);

	len = scnprintf(buf, sizeof(buf),
			"\t\t\t\t\tWIN%d: %s[0x%llx] SRC[%d %d %d %d] ",
			index, str_state[win->state],
			(win->state == DPU_WIN_STATE_BUFFER) ?
			win->dbg_dma_addr : 0,
			win->src_x, win->src_y, win->src_w, win->src_h);
	len += scnprintf(buf + len, sizeof(buf) - len,
			"DST[%d %d %d %d] ",
			win->dst_x, win->dst_y, win->dst_w, win->dst_h);
	len += scnprintf(buf + len, sizeof(buf) - len,
			"ROT[%d] COMP[%d] ", win->is_rot, win->is_comp);
	if (win->state == DPU_WIN_STATE_BUFFER)
		scnprintf(buf + len, sizeof(buf) - len, "CH%d ", win->dpp_ch);

	str_comp = get_comp_src_name(win->comp_src);
	drm_printf(p, "%s %s %s\n", buf, fmt->name, str_comp);
}

static void dpu_print_log_atomic(const struct drm_device *drm_dev,
		struct dpu_log_atomic *atomic, struct drm_printer *p)
{
	int i;
	struct dpu_bts_win_config *win;
	int win_cnt = get_plane_num(drm_dev);

	for (i = 0; i < win_cnt; ++i) {
		win = &atomic->win_config[i];
		__dpu_print_log_atomic(win, i, p);
	}
}

static void dpu_print_log_atomic_wb(const struct drm_device *drm_dev,
		struct dpu_log_atomic *atomic, struct drm_printer *p)
{
	__dpu_print_log_atomic(&atomic->win_config[0], 0, p);
}

#define RSC_STATUS_IDLE	7
static void dpu_print_log_rsc(const struct drm_device *drm_dev, char *buf,
		int len, struct dpu_log_rsc_occupancy *rsc)
{
	int i, len_chs, len_wins;
	int win_cnt = get_plane_num(drm_dev);
	unsigned int decon_id;
	char str_chs[128];
	char str_wins[128];

	len_chs = sprintf(str_chs, "CHs: ");
	len_wins = sprintf(str_wins, "WINs: ");

	for (i = 0; i < win_cnt; ++i) {
		decon_id = is_decon_using_ch(rsc->rsc_ch, i);
		len_chs += sprintf(str_chs + len_chs, "%d[%c] ", i,
				decon_id < RSC_STATUS_IDLE ? '0' + decon_id : 'X');

		decon_id = is_decon_using_win(rsc->rsc_win, i);
		len_wins += sprintf(str_wins + len_wins, "%d[%c] ", i,
				decon_id < RSC_STATUS_IDLE ? '0' + decon_id : 'X');
	}

	sprintf(buf + len, "\t%s\n\t\t\t\t\t%s", str_chs, str_wins);
}

#define LOG_BUF_SIZE	256
static int dpu_print_log_freqs(char *buf, int len, struct dpu_log_freqs *freqs)
{
	return scnprintf(buf + len, LOG_BUF_SIZE - len,
			"\tmif(%lu) int(%lu) disp(%lu)",
			freqs->mif_freq, freqs->int_freq, freqs->disp_freq);
}

static int dpu_print_log_partial(char *buf, int len, struct dpu_log_partial *p)
{
	len += scnprintf(buf + len, LOG_BUF_SIZE - len,
			"\treq[%d %d %d %d] adj[%d %d %d %d] prev[%d %d %d %d]",
			p->req.x1, p->req.y1,
			drm_rect_width(&p->req), drm_rect_height(&p->req),
			p->adj.x1, p->adj.y1,
			drm_rect_width(&p->adj), drm_rect_height(&p->adj),
			p->prev.x1, p->prev.y1,
			drm_rect_width(&p->prev), drm_rect_height(&p->prev));
	return scnprintf(buf + len, LOG_BUF_SIZE - len,
			" reconfig(%d)", p->reconfigure);
}

#define MAX_EVENT_NAME (32)
const char *get_event_name(enum dpu_event_type type)
{
	static const char events[][MAX_EVENT_NAME] = {
		"NONE",				"DECON_ENABLED",
		"DECON_DISABLED",		"DECON_FRAMEDONE",
		"DECON_FRAMESTART",		"DECON_RSC_OCCUPANCY",
		"DECON_TRIG_MASK",		"DSIM_ENABLED",
		"DSIM_DISABLED",		"DSIM_COMMAND",
		"DSIM_UNDERRUN",		"DSIM_FRAMEDONE",
		"DPP_FRAMEDONE",		"DMA_RECOVERY",
		"ATOMIC_COMMIT",		"TE_INTERRUPT",
		"ENTER_HIBERNATION_IN",		"ENTER_HIBERNATION_OUT",
		"EXIT_HIBERNATION_IN",		"EXIT_HIBERNATION_OUT",
		"ATOMIC_BEGIN",			"ATOMIC_FLUSH",
		"WB_ENABLE",			"WB_DISABLE",
		"WB_ATOMIC_COMMIT",		"WB_FRAMEDONE",
		"WB_ENTER_HIBERNATION",		"WB_EXIT_HIBERNATION",
		"PLANE_UPDATE",			"PLANE_DISABLE",
		"REQ_CRTC_INFO_OLD",		"REQ_CRTC_INFO_NEW",
		"FRAMESTART_TIMEOUT",
		"BTS_RELEASE_BW",		"BTS_CALC_BW",
		"BTS_UPDATE_BW",		"NEXT_ADJ_VBLANK",
		"VBLANK_ENABLE",		"VBLANK_DISABLE",
		"PARTIAL_INIT",			"PARTIAL_PREPARE",
		"PARTIAL_UPDATE",		"PARTIAL_RESTORE",
		"DQE_RESTORE",			"DQE_COLORMODE",
		"DQE_PRESET",			"DQE_CONFIG",
		"DQE_DIMSTART",			"DQE_DIMEND",
		"TUI_ENTER",			"TUI_EXIT",
		"RECOVERY_START",		"RECOVERY_END",
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		"FREQ_HOP",			"MODE_SET",
#endif
	};

	if (type >= DPU_EVT_MAX)
		return NULL;

	return events[type];
}
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
void DPU_EVENT_SHOW(const struct exynos_drm_crtc *exynos_crtc, struct drm_printer *p)
#else
static void
DPU_EVENT_SHOW(const struct exynos_drm_crtc *exynos_crtc, struct drm_printer *p)
#endif
{
	const struct dpu_debug *d = &exynos_crtc->d;
	const struct drm_device *drm_dev = exynos_crtc->base.dev;
	int idx = atomic_read(&d->event_log_idx);
	struct dpu_log *log;
	int latest = idx % dpu_event_log_max;
	struct timespec64 ts;
	ktime_t prev_ktime;
	const char *str_comp;
	char buf[LOG_BUF_SIZE];
	int len;

	if (IS_ERR_OR_NULL(d->event_log))
		return;

	drm_printf(p, "----------------------------------------------------\n");
	drm_printf(p, "%14s  %20s  %20s\n", "Time", "Event ID", "Remarks");
	drm_printf(p, "----------------------------------------------------\n");

	/* Seek a oldest from current index */
	if (dpu_event_print_max > dpu_event_log_max)
		dpu_event_print_max = dpu_event_log_max;

	if (idx < dpu_event_print_max)
		idx = 0;
	else
		idx = (idx - dpu_event_print_max) % dpu_event_log_max;

	prev_ktime = ktime_set(0, 0);
	do {
		if (++idx >= dpu_event_log_max)
			idx = 0;

		/* Seek a index */
		log = &d->event_log[idx];

		/* TIME */
		ts = ktime_to_timespec64(log->time);

		len = scnprintf(buf, sizeof(buf), "[%6lld.%06ld] ", ts.tv_sec,
				ts.tv_nsec/NSEC_PER_USEC);

		/* If there is no timestamp, then exit directly */
		if (!ts.tv_sec)
			break;

		len += scnprintf(buf + len, sizeof(buf) - len,  "%20s",
				get_event_name(log->type));

		switch (log->type) {
		case DPU_EVT_DECON_RSC_OCCUPANCY:
			dpu_print_log_rsc(drm_dev, buf, len, &log->data.rsc);
			break;
		case DPU_EVT_DSIM_COMMAND:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tCMD_ID: 0x%x\tDATA[0]: 0x%x",
					log->data.cmd.id, log->data.cmd.buf);
			break;
		case DPU_EVT_DPP_FRAMEDONE:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tID:%d", log->data.dpp.id);
			break;
		case DPU_EVT_DMA_RECOVERY:
			str_comp = get_comp_src_name(log->data.dpp.comp_src);
			scnprintf(buf + len, sizeof(buf) - len,
					"\tID:%d SRC:%s COUNT:%d",
					log->data.dpp.id, str_comp,
					log->data.dpp.recovery_cnt);
			break;
		case DPU_EVT_ENTER_HIBERNATION_IN:
		case DPU_EVT_ENTER_HIBERNATION_OUT:
		case DPU_EVT_EXIT_HIBERNATION_IN:
		case DPU_EVT_EXIT_HIBERNATION_OUT:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tDPU POWER %s",
					log->data.pd.rpm_active ? "ON" : "OFF");
			break;
		case DPU_EVT_PLANE_UPDATE:
		case DPU_EVT_PLANE_DISABLE:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tCH:%d, WIN:%d",
					log->data.win.plane_idx,
					log->data.win.win_idx);
			break;
		case DPU_EVT_REQ_CRTC_INFO_OLD:
		case DPU_EVT_REQ_CRTC_INFO_NEW:
			scnprintf(buf + len, sizeof(buf) - len,
				"\tenable(%d) active(%d) [p:%d m:%d a:%d c:%d]",
					STATE_ARG(&log->data.crtc_info));
			break;
		case DPU_EVT_BTS_RELEASE_BW:
		case DPU_EVT_BTS_UPDATE_BW:
			dpu_print_log_freqs(buf, len, &log->data.freqs);
			break;
		case DPU_EVT_BTS_CALC_BW:
			len += dpu_print_log_freqs(buf, len,
					&log->data.bts.freqs);
			scnprintf(buf + len, sizeof(buf) - len,
					" calculated disp(%u)",
					log->data.bts.calc_disp);
			break;
		case DPU_EVT_DSIM_UNDERRUN:
			len += dpu_print_log_freqs(buf, len,
					&log->data.underrun.freqs);
			scnprintf(buf + len, sizeof(buf) - len,
					" underrun count(%d)",
					log->data.underrun.underrun_cnt);
			break;
		case DPU_EVT_NEXT_ADJ_VBLANK:
			scnprintf(buf + len, sizeof(buf) - len,
					"\ttimestamp(%lld)",
					log->data.timestamp);
			break;
		case DPU_EVT_PARTIAL_INIT:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tminimum rect size[%dx%d]",
					log->data.partial.min_w,
					log->data.partial.min_h);
			break;
		case DPU_EVT_PARTIAL_PREPARE:
			dpu_print_log_partial(buf, len,
					&log->data.partial);
			break;
		case DPU_EVT_PARTIAL_RESTORE:
		case DPU_EVT_PARTIAL_UPDATE:
			scnprintf(buf + len, sizeof(buf) - len,
					"\t[%d %d %d %d]",
					log->data.partial.prev.x1,
					log->data.partial.prev.y1,
					drm_rect_width(&log->data.partial.prev),
					drm_rect_height(&log->data.partial.prev));
			break;
		case DPU_EVT_TUI_ENTER:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tresolution[%ux%u] mode[%s]",
					log->data.tui.xres,
					log->data.tui.yres,
					(log->data.tui.mode) ? "Video" : "Command");
			break;
		case DPU_EVT_RECOVERY_START:
		case DPU_EVT_RECOVERY_END:
			scnprintf(buf + len, sizeof(buf) - len,
					"\tid[%d] count[%d]",
					log->data.recovery.id,
					log->data.recovery.count);
			break;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		case DPU_EVT_FREQ_HOP:
			scnprintf(buf + len, sizeof(buf) - len,
					"\t[m: %d->%d, k: %d->%d]",
					log->data.freq_hop.orig_m,
					log->data.freq_hop.target_m,
					log->data.freq_hop.orig_k,
					log->data.freq_hop.target_k);
			break;
		case DPU_EVT_MODE_SET:
			scnprintf(buf + len, sizeof(buf) - len,
					"\t[MRES: %d, VREF: %d: mode: %s]",
					(log->data.mode_set.event & SEAMLESS_MODESET_MRES) ? 1 : 0,
					(log->data.mode_set.event & SEAMLESS_MODESET_VREF) ? 1 : 0,
					log->data.mode_set.mode);
			break;
#endif
		default:
			break;
		}

		drm_printf(p, "%s\n", buf);

		switch (log->type) {
		case DPU_EVT_ATOMIC_COMMIT:
			dpu_print_log_atomic(drm_dev, &log->data.atomic, p);
			break;
		case DPU_EVT_WB_ATOMIC_COMMIT:
			dpu_print_log_atomic_wb(drm_dev, &log->data.atomic, p);
			break;
		default:
			break;
		}
	} while (latest != idx);

	drm_printf(p, "----------------------------------------------------\n");
}

static int dpu_debug_event_show(struct seq_file *s, void *unused)
{
	struct exynos_drm_crtc *exynos_crtc = s->private;
	struct drm_printer p = drm_seq_file_printer(s);

	DPU_EVENT_SHOW(exynos_crtc, &p);
	return 0;
}

static int dpu_debug_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_debug_event_show, inode->i_private);
}

static const struct file_operations dpu_event_fops = {
	.open = dpu_debug_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int recovery_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int recovery_open(struct inode *inode, struct file *file)
{
	return single_open(file, recovery_show, inode->i_private);
}

static ssize_t recovery_write(struct file *file, const char *user_buf,
			      size_t count, loff_t *f_pos)
{
	struct seq_file *s = file->private_data;
	struct exynos_drm_crtc *exynos_crtc = s->private;

	if (exynos_crtc->ops->recovery)
		exynos_crtc->ops->recovery(exynos_crtc, "force");

	return count;
}

static const struct file_operations recovery_fops = {
	.open = recovery_open,
	.read = seq_read,
	.write = recovery_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

void dpu_profile_hiber_enter(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (!profile->started)
		return;

	profile->hiber_entry_time = ktime_get();
	profile->profile_enter_cnt++;
}

void dpu_profile_hiber_exit(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (!profile->started)
		return;

	profile->hiber_time += ktime_us_delta(ktime_get(), profile->hiber_entry_time);
	profile->profile_exit_cnt++;
}

static int dpu_get_hiber_ratio(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;
	s64 residency = profile->hiber_time;

	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile->profile_time);

	return residency;
}

static void _dpu_profile_hiber_show(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (profile->started) {
		pr_info("%s: hibernation profiling is ongoing\n", __func__);
		return;
	}

	pr_info("#########################################\n");
	pr_info("Profiling Time: %llu us\n", profile->profile_time);
	pr_info("Hibernation Entry Time: %llu us\n", profile->hiber_time);
	pr_info("Hibernation Entry Ratio: %d %%\n", dpu_get_hiber_ratio(exynos_crtc));
	pr_info("Entry count: %d, Exit count: %d\n", profile->profile_enter_cnt,
			profile->profile_exit_cnt);
	pr_info("Framedone count: %d, FPS: %lld\n", profile->frame_cnt,
			(profile->frame_cnt * 1000000) / profile->profile_time);
	pr_info("#########################################\n");
}

static int dpu_profile_hiber_show(struct seq_file *s, void *unused)
{
	struct exynos_drm_crtc *exynos_crtc = s->private;
	struct drm_printer p = drm_seq_file_printer(s);

	if (!exynos_crtc->hibernation) {
		drm_printf(&p, "decon%d is not support hibernation\n",
				drm_crtc_index(&exynos_crtc->base));
		return 0;
	}

	_dpu_profile_hiber_show(exynos_crtc);

	return 0;
}

static int dpu_profile_hiber_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_profile_hiber_show, inode->i_private);
}

static void dpu_profile_hiber_start(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (profile->started) {
		pr_err("%s: hibernation profiling is ongoing\n", __func__);
		return;
	}

	/* reset profiling variables */
	memset(profile, 0, sizeof(*profile));

	/* profiling is just started */
	profile->profile_start_time = ktime_get();
	profile->started = true;

	/* hibernation status when profiling is started */
	if (IS_DECON_HIBER_STATE(exynos_crtc))
		dpu_profile_hiber_enter(exynos_crtc);

	pr_info("display hibernation profiling is started\n");
}

static void dpu_profile_hiber_finish(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (!profile->started) {
		pr_err("%s: hibernation profiling is not started\n", __func__);
		return;
	}

	profile->profile_time = ktime_us_delta(ktime_get(),
			profile->profile_start_time);

	/* hibernation status when profiling is finished */
	if (IS_DECON_HIBER_STATE(exynos_crtc))
		dpu_profile_hiber_exit(exynos_crtc);

	profile->started = false;

	_dpu_profile_hiber_show(exynos_crtc);

	pr_info("display hibernation profiling is finished\n");
}

static ssize_t dpu_profile_hiber_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	char *buf_data;
	int ret;
	int input;
	struct seq_file *s = file->private_data;
	struct exynos_drm_crtc *exynos_crtc = s->private;

	if (!count)
		return count;

	buf_data = kmalloc(count, GFP_KERNEL);
	if (buf_data == NULL)
		goto out_cnt;

	ret = copy_from_user(buf_data, buf, count);
	if (ret < 0)
		goto out;

	ret = sscanf(buf_data, "%u", &input);
	if (ret < 0)
		goto out;

	if (input)
		dpu_profile_hiber_start(exynos_crtc);
	else
		dpu_profile_hiber_finish(exynos_crtc);

out:
	kfree(buf_data);
out_cnt:
	return count;
}

static const struct file_operations dpu_profile_hiber_fops = {
	.open = dpu_profile_hiber_open,
	.write = dpu_profile_hiber_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int dpu_init_debug(struct exynos_drm_crtc *exynos_crtc)
{
	int i;
	u32 event_cnt;
	struct drm_crtc *crtc;

	exynos_crtc->d.event_log = NULL;
	event_cnt = dpu_event_log_max;

	for (i = 0; i < DPU_EVENT_LOG_RETRY; ++i) {
		event_cnt = event_cnt >> i;
		exynos_crtc->d.event_log =
			vzalloc(sizeof(struct dpu_log) * event_cnt);
		if (IS_ERR_OR_NULL(exynos_crtc->d.event_log)) {
			DRM_WARN("failed to alloc event log buf[%d]. retry\n",
					event_cnt);
			continue;
		}

		DRM_INFO("#%d event log buffers are allocated\n", event_cnt);
		break;
	}
	spin_lock_init(&exynos_crtc->d.event_lock);
	exynos_crtc->d.event_log_cnt = event_cnt;
	atomic_set(&exynos_crtc->d.event_log_idx, -1);

	crtc = &exynos_crtc->base;

	debugfs_create_file("event", 0444, crtc->debugfs_entry,	exynos_crtc,
			&dpu_event_fops);

	debugfs_create_file("recovery", 0644, crtc->debugfs_entry, exynos_crtc,
			&recovery_fops);

	debugfs_create_u32("underrun_cnt", 0664, crtc->debugfs_entry,
			&exynos_crtc->d.underrun_cnt);

	debugfs_create_file("profile_hiber", 0444, crtc->debugfs_entry,
			exynos_crtc, &dpu_profile_hiber_fops);

	return 0;
}

#define PREFIX_LEN	40
#define ROW_LEN		32
void dpu_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	int i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_INFO, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

void dpu_print_eint_state(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (!exynos_crtc->d.eint_pend)
		return;

	DRM_INFO("%s: eint pend state(0x%x)\n", __func__,
			readl(exynos_crtc->d.eint_pend));
}

void dpu_check_panel_status(struct drm_crtc *crtc)
{
	struct drm_connector *connector =
		crtc_get_conn(crtc->state, DRM_MODE_CONNECTOR_DSI);

	if (connector) {
		struct exynos_drm_connector *exynos_conn =
			to_exynos_connector(connector);
		const struct exynos_drm_connector_funcs *funcs = exynos_conn->funcs;

		if (funcs && funcs->query_status)
			funcs->query_status(exynos_conn);
	}
}

void dpu_dump(struct exynos_drm_crtc *exynos_crtc)
{
	bool active;

	active = pm_runtime_active(exynos_crtc->dev);
	pr_info("DPU power %s state\n", active ? "on" : "off");

	if (active && exynos_crtc->ops->dump_register)
		exynos_crtc->ops->dump_register(exynos_crtc);
}

bool dpu_event(struct exynos_drm_crtc *exynos_crtc)
{
	bool active;
	struct drm_printer p = drm_info_printer(exynos_crtc->dev);

	active = pm_runtime_active(exynos_crtc->dev);
	pr_info("DPU power %s state\n", active ? "on" : "off");

	DPU_EVENT_SHOW(exynos_crtc, &p);

	return active;
}

void dpu_dump_with_event(struct exynos_drm_crtc *exynos_crtc)
{
	bool active = dpu_event(exynos_crtc);

	if (active && exynos_crtc->ops->dump_register)
		exynos_crtc->ops->dump_register(exynos_crtc);
}

int dpu_sysmmu_fault_handler(struct iommu_fault *fault, void *data)
{
	struct exynos_drm_crtc *exynos_crtc = data;
	pr_info("%s +\n", __func__);

	dpu_dump_with_event(exynos_crtc);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int dpu_itmon_notifier(struct notifier_block *nb, unsigned long act, void *data)
{
	struct dpu_debug *d;
	struct exynos_drm_crtc *exynos_crtc;
	struct itmon_notifier *itmon_data = data;
	struct drm_printer p;
	static bool is_dumped = false;

	d = container_of(nb, struct dpu_debug, itmon_nb);
	exynos_crtc = container_of(d, struct exynos_drm_crtc, d);
	p = drm_info_printer(exynos_crtc->dev);

	if (d->itmon_notified)
		return NOTIFY_DONE;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	if (is_dumped)
		return NOTIFY_DONE;

	/* port is master and dest is target */
	if ((itmon_data->port &&
		(strncmp("DPU", itmon_data->port, sizeof("DPU") - 1) == 0)) ||
		(itmon_data->dest &&
		(strncmp("DPU", itmon_data->dest, sizeof("DPU") - 1) == 0))) {

		pr_info("%s: DECON%d +\n", __func__, exynos_crtc->base.index);

		pr_info("%s: port: %s, dest: %s\n", __func__,
				itmon_data->port, itmon_data->dest);

		dpu_dump_with_event(exynos_crtc);

		d->itmon_notified = true;
		is_dumped = true;
		pr_info("%s -\n", __func__);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
#define BUFFER_SANITY_CHECK_SIZE (2048UL)
static u64 get_buffer_checksum(struct exynos_drm_gem *exynos_gem)
{
	struct drm_gem_object *gem = &exynos_gem->base;
	u64 checksum64 = 0;
	size_t i, size;

	size = gem->size;
	if (size > BUFFER_SANITY_CHECK_SIZE)
		size = BUFFER_SANITY_CHECK_SIZE;

	for (i = 0; i + sizeof(u64) < size; i += sizeof(u64))
		checksum64 += *((u64 *)(exynos_gem->vaddr + i));

	return checksum64;
}

void
exynos_atomic_commit_prepare_buf_sanity(struct drm_atomic_state *old_state)
{
	int i, j;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_framebuffer *fb;
	struct drm_gem_object **gem;
	struct exynos_drm_gem *exynos_gem;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		if (crtc->index == 0)
			break;
	}
	if (!crtc || crtc->index)
		return;

	for_each_new_plane_in_state(old_state, plane, new_plane_state, i) {
		fb = new_plane_state->fb;
		if (!fb)
			continue;
		gem = fb->obj;
		for (j = 0; gem[j]; j++) {
			exynos_gem = to_exynos_gem(gem[j]);
			if (!exynos_gem->vaddr)
				continue;

			exynos_gem->checksum64 = get_buffer_checksum(exynos_gem);
			pr_debug("%s: ch(%lu)[%d] fd(%d) checksum(%#llx)\n",
				__func__, plane->index, j, exynos_gem->acq_fence,
				exynos_gem->checksum64);
		}
	}
}

#define SANITY_DFT_FPS	(60)
bool exynos_atomic_commit_check_buf_sanity(struct drm_atomic_state *old_state)
{
	int i, j;
	int fps;
	u64 checksum64;
	bool sanity_ok = true;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_framebuffer *fb;
	struct drm_gem_object **gem;
	struct exynos_drm_gem *exynos_gem;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		if (crtc->index == 0)
			break;
	}
	if (!crtc || crtc->index)
		return sanity_ok;

	fps = drm_mode_vrefresh(&new_crtc_state->adjusted_mode);
	if (!fps)
		fps = SANITY_DFT_FPS;

	usleep_range(100000/fps/4, 100000/fps/4 + 10);

	for_each_new_plane_in_state(old_state, plane, new_plane_state, i) {
		fb = new_plane_state->fb;
		if (!fb)
			continue;
		gem = fb->obj;
		for (j = 0; gem[j]; j++) {
			exynos_gem = to_exynos_gem(gem[j]);
			if (!exynos_gem->vaddr)
				continue;

			checksum64 = get_buffer_checksum(exynos_gem);

			sanity_ok = (exynos_gem->checksum64 == checksum64);

			if (sanity_ok) {
				pr_debug("%s: ch(%lu)[%d] fd(%d) checksum(%#llx)\n",
						__func__, plane->index, j,
						exynos_gem->acq_fence,
						exynos_gem->checksum64);
			} else {
				pr_err("%s: sanity check error\n", __func__);
				pr_err("%s: ch(%lu)[%d] fd(%d) checksum(%#llx)\n",
						__func__, plane->index, j,
						exynos_gem->acq_fence,
						exynos_gem->checksum64);
			}
		}
	}

	return sanity_ok;
}
#endif

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#define MAX_EXYNOS_DRM_IOCTL_TIME (20)
struct exynos_drm_ioctl_time_t exynos_drm_ioctl_time[MAX_EXYNOS_DRM_IOCTL_TIME];
int exynos_drm_ioctl_time_cnt;
struct mutex exynos_drm_ioctl_time_mutex;

//#define DEBUG_EXYNOS_DRM_IOCTL_TIME

void exynos_drm_ioctl_time_init(void)
{
	mutex_init(&exynos_drm_ioctl_time_mutex);
	exynos_drm_ioctl_time_cnt = 0;
}

void exynos_drm_ioctl_time_start(unsigned int cmd, ktime_t time_start)
{
	int p;

	mutex_lock(&exynos_drm_ioctl_time_mutex);

	p = exynos_drm_ioctl_time_cnt;

	exynos_drm_ioctl_time[p].cmd = cmd;
	exynos_drm_ioctl_time[p].ioctl_time_start = time_start;
	exynos_drm_ioctl_time[p].ioctl_time_end = 0;
	exynos_drm_ioctl_time[p].done = false;

#ifdef DEBUG_EXYNOS_DRM_IOCTL_TIME
	pr_info("exynos_drm_ioctl_time [START] (%d)(%X)(%lld)\n", p, cmd, time_start);
#endif
	exynos_drm_ioctl_time_cnt = (exynos_drm_ioctl_time_cnt + 1) % 20;

	mutex_unlock(&exynos_drm_ioctl_time_mutex);

#ifdef DEBUG_EXYNOS_DRM_IOCTL_TIME
	exynos_drm_ioctl_time_print();
#endif
}

void exynos_drm_ioctl_time_end(unsigned int cmd, ktime_t time_start, ktime_t time_end)
{
	int i = 0;

	mutex_lock(&exynos_drm_ioctl_time_mutex);

	for (i = 0 ; i < MAX_EXYNOS_DRM_IOCTL_TIME; i++) {
		if (exynos_drm_ioctl_time[i].cmd == cmd && exynos_drm_ioctl_time[i].ioctl_time_start == time_start) {
			exynos_drm_ioctl_time[i].ioctl_time_end = time_end;
			exynos_drm_ioctl_time[i].done = true;

#ifdef DEBUG_EXYNOS_DRM_IOCTL_TIME
			pr_info("exynos_drm_ioctl_time [END] (%d)(%X)(%lld)(%lld)\n", i, cmd, time_start, time_end);
#endif
			mutex_unlock(&exynos_drm_ioctl_time_mutex);
#ifdef DEBUG_EXYNOS_DRM_IOCTL_TIME
			exynos_drm_ioctl_time_print();
#endif
			return;
		}
	}
#ifdef DEBUG_EXYNOS_DRM_IOCTL_TIME
	pr_info("exynos_drm_ioctl_time [END] (%d)(%X)(%lld)(%lld)\n", i, cmd, time_start, time_end);
	pr_info("somthing wrong!!\n");
#endif
	mutex_unlock(&exynos_drm_ioctl_time_mutex);
}

void exynos_drm_ioctl_time_print(void)
{
	int i = 0;
	struct timespec64 start, end, elapsed;

	mutex_lock(&exynos_drm_ioctl_time_mutex);
	pr_info("================ [ %s ] ================\n", __func__);

	for (i = 0 ; i < MAX_EXYNOS_DRM_IOCTL_TIME; i++) {
		start = ktime_to_timespec64(exynos_drm_ioctl_time[i].ioctl_time_start);
		end = ktime_to_timespec64(exynos_drm_ioctl_time[i].ioctl_time_end);

		if (exynos_drm_ioctl_time[i].done)
			elapsed = timespec64_sub(end, start); /* start ~ end */
		else
			elapsed = timespec64_sub(ktime_to_timespec64(ktime_get()), start); /* start ~ now */

		pr_info("[%3d]cmd:[0X%08X] start:(%lld.%06lld) end:(%lld.%06lld) elapse:(%lld.%06lld) (%s)\n",
			/* index */
			i,
			/* cmd */
			exynos_drm_ioctl_time[i].cmd,
			/* start */
			start.tv_sec, start.tv_nsec/NSEC_PER_USEC,
			/* end */
			end.tv_sec, end.tv_nsec/NSEC_PER_USEC,
			/* elapsed */
			elapsed.tv_sec, elapsed.tv_nsec/NSEC_PER_USEC,
			/* done */
			exynos_drm_ioctl_time[i].done ? "DONE" : "RUNNING!!!");
	}

	mutex_unlock(&exynos_drm_ioctl_time_mutex);
}
#endif
