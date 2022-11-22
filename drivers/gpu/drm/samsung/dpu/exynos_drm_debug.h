/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for DPU debug.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DEBUG_H__
#define __EXYNOS_DRM_DEBUG_H__

#include <linux/iommu.h>
#include <drm/drm_rect.h>
#include <exynos_drm_modifier.h>
#include <exynos_drm_bts.h>
#include <drm/drm_modes.h>

/**
 * Display Subsystem event management status.
 *
 * These status labels are used internally by the DECON to indicate the
 * current status of a device with operations.
 */
enum dpu_event_type {
	DPU_EVT_NONE = 0,

	DPU_EVT_DECON_ENABLED,
	DPU_EVT_DECON_DISABLED,
	DPU_EVT_DECON_FRAMEDONE,
	DPU_EVT_DECON_FRAMESTART,
	DPU_EVT_DECON_RSC_OCCUPANCY,
	DPU_EVT_DECON_TRIG_MASK,

	DPU_EVT_DSIM_ENABLED,
	DPU_EVT_DSIM_DISABLED,
	DPU_EVT_DSIM_COMMAND,
	DPU_EVT_DSIM_UNDERRUN,
	DPU_EVT_DSIM_FRAMEDONE,

	DPU_EVT_DPP_FRAMEDONE,
	DPU_EVT_DMA_RECOVERY,

	DPU_EVT_ATOMIC_COMMIT,
	DPU_EVT_TE_INTERRUPT,

	DPU_EVT_ENTER_HIBERNATION_IN,
	DPU_EVT_ENTER_HIBERNATION_OUT,
	DPU_EVT_EXIT_HIBERNATION_IN,
	DPU_EVT_EXIT_HIBERNATION_OUT,

	DPU_EVT_ATOMIC_BEGIN,
	DPU_EVT_ATOMIC_FLUSH,

	DPU_EVT_WB_ENABLE,
	DPU_EVT_WB_DISABLE,
	DPU_EVT_WB_ATOMIC_COMMIT,
	DPU_EVT_WB_FRAMEDONE,
	DPU_EVT_WB_ENTER_HIBERNATION,
	DPU_EVT_WB_EXIT_HIBERNATION,

	DPU_EVT_PLANE_UPDATE,
	DPU_EVT_PLANE_DISABLE,

	DPU_EVT_REQ_CRTC_INFO_OLD,
	DPU_EVT_REQ_CRTC_INFO_NEW,

	DPU_EVT_FRAMESTART_TIMEOUT,

	DPU_EVT_BTS_RELEASE_BW,
	DPU_EVT_BTS_CALC_BW,
	DPU_EVT_BTS_UPDATE_BW,

	DPU_EVT_NEXT_ADJ_VBLANK,
	DPU_EVT_VBLANK_ENABLE,
	DPU_EVT_VBLANK_DISABLE,

	DPU_EVT_PARTIAL_INIT,
	DPU_EVT_PARTIAL_PREPARE,
	DPU_EVT_PARTIAL_UPDATE,
	DPU_EVT_PARTIAL_RESTORE,

	DPU_EVT_DQE_RESTORE,
	DPU_EVT_DQE_COLORMODE,
	DPU_EVT_DQE_PRESET,
	DPU_EVT_DQE_CONFIG,
	DPU_EVT_DQE_DIMSTART,
	DPU_EVT_DQE_DIMEND,

	DPU_EVT_TUI_ENTER,
	DPU_EVT_TUI_EXIT,

	DPU_EVT_RECOVERY_START,
	DPU_EVT_RECOVERY_END,

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	DPU_EVT_FREQ_HOP,
	DPU_EVT_MODE_SET,
#endif
	DPU_EVT_MAX, /* End of EVENT */
};

/* DPU fence event logger */
enum dpu_fence_event_type {
	DPU_FENCE_NONE = 0,
	/* queue crtc-out-fence in vblank_event_list */
	DPU_FENCE_ARM_CRTC_OUT_FENCE,
	/* signal pending crtc-out-fence */
	DPU_FENCE_SIGNAL_CRTC_OUT_FENCE,
	/* wait for plane-in-fence signaled */
	DPU_FENCE_WAIT_PLANE_IN_FENCE,
	/* wait timeout for plane-in-fence */
	DPU_FENCE_TIMEOUT_IN_FENCE,
	DPU_FENCE_ELAPSED_IN_FENCE,

	DPU_FENCE_MAX,
};

#define DPU_FENCE_EVENT_LOG_RETRY       2
#define MAX_FENCE_DRV_NAME              32
struct dpu_fence_info {
	int fd;
	char name[MAX_FENCE_DRV_NAME];
	char timeline[MAX_FENCE_DRV_NAME];
	u64 context;
	unsigned int seqno;
	unsigned long flags;
	s64 in_fence_wait_ms;
};
struct dpu_fence_log {
	ktime_t time;
	enum dpu_fence_event_type type;
	struct dpu_fence_info fence_info;
};

#define DPU_CALLSTACK_MAX 10
struct dpu_log_dsim_cmd {
	u32 id;
	u8 buf;
	void *caller[DPU_CALLSTACK_MAX];
};

struct dpu_log_dpp {
	u32 id;
	u64 comp_src;
	u32 recovery_cnt;
};

struct dpu_log_win {
	u32 win_idx;
	u32 plane_idx;
};

struct dpu_log_rsc_occupancy {
	u64 rsc_ch;
	u64 rsc_win;
};

struct dpu_log_atomic {
	struct dpu_bts_win_config win_config[BTS_WIN_MAX];
};

/* Event log structure for DPU power domain status */
struct dpu_log_pd {
	bool rpm_active;
};

#define STATE_ARG(s) (s)->enable, (s)->active, (s)->planes_changed, \
	(s)->mode_changed, (s)->active_changed, (s)->color_mgmt_changed
struct dpu_log_crtc_info {
	bool enable;
	bool active;
	bool planes_changed;
	bool mode_changed;
	bool active_changed;
	bool color_mgmt_changed;
};

struct dpu_log_freqs {
	unsigned long mif_freq;
	unsigned long int_freq;
	unsigned long disp_freq;
};

struct dpu_log_bts {
	struct dpu_log_freqs freqs;
	unsigned int calc_disp;
};

struct dpu_log_underrun {
	struct dpu_log_freqs freqs;
	unsigned int underrun_cnt;
};

struct dpu_log_partial {
	u32 min_w;
	u32 min_h;
	struct drm_rect prev;
	struct drm_rect req;
	struct drm_rect adj;
	bool need_partial_update;
	bool reconfigure;
};

struct dpu_log_tui {
	u32 xres;
	u32 yres;
	u32 mode;
};

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
struct dpu_log_freq_hop {
	u32 orig_m;
	u32 orig_k;
	u32 target_m;
	u32 target_k;
};

#define EVENT_MODE_SET_VREF	(0x1 << 0)
#define EVENT_MODE_SET_RES	(0x1 << 1)

struct dpu_log_mode_set {
	u32 event;
	char mode[DRM_DISPLAY_MODE_LEN];
};

#endif

struct dpu_log_recovery {
	int id;
	int count;
};

struct dpu_log {
	ktime_t time;
	enum dpu_event_type type;

	union {
		struct dpu_log_dpp dpp;
		struct dpu_log_atomic atomic;
		struct dpu_log_dsim_cmd cmd;
		struct dpu_log_rsc_occupancy rsc;
		struct dpu_log_pd pd;
		struct dpu_log_win win;
		struct dpu_log_crtc_info crtc_info;
		struct dpu_log_freqs freqs;
		struct dpu_log_bts bts;
		struct dpu_log_underrun underrun;
		struct dpu_log_partial partial;
		struct dpu_log_tui tui;
		struct dpu_log_recovery recovery;
		ktime_t timestamp;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		struct dpu_log_freq_hop freq_hop;
		struct dpu_log_mode_set mode_set;
#endif
	} data;
};

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
struct exynos_drm_ioctl_time_t {
	unsigned int cmd;
	ktime_t ioctl_time_start;
	ktime_t ioctl_time_end;
	bool done;
};

void exynos_drm_ioctl_time_init(void);
void exynos_drm_ioctl_time_start(unsigned int cmd, ktime_t time_start);
void exynos_drm_ioctl_time_end(unsigned int cmd, ktime_t time_start, ktime_t time_end);
void exynos_drm_ioctl_time_print(void);
#endif

struct drm_crtc;
struct exynos_drm_crtc;
int dpu_init_debug(struct exynos_drm_crtc *exynos_crtc);
void DPU_EVENT_LOG(enum dpu_event_type type, struct exynos_drm_crtc *exynos_crtc,
		void *priv);
void DPU_EVENT_LOG_ATOMIC_COMMIT(struct exynos_drm_crtc *exynos_crtc);
void DPU_EVENT_LOG_CMD(struct exynos_drm_crtc *exynos_crtc, u32 cmd_id,
		unsigned long data);
void DPU_FENCE_EVENT_LOG(enum dpu_fence_event_type type,
		struct exynos_drm_crtc *exynos_crtc, void *priv);
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
void DPU_EVENT_LOG_FREQ_HOP(struct exynos_drm_crtc *exynos_crtc,
		u32 orig_m, u32 orig_k, u32 target_m, u32 target_k);
void DPU_EVENT_LOG_MODE_SET(struct exynos_drm_crtc *exynos_crtc, u32 event, char *mode_name);
void DPU_EVENT_SHOW(const struct exynos_drm_crtc *exynos_crtc, struct drm_printer *p);
#endif


void dpu_print_eint_state(struct drm_crtc *crtc);
void dpu_check_panel_status(struct drm_crtc *crtc);
void dpu_dump(struct exynos_drm_crtc *exynos_crtc);
bool dpu_event(struct exynos_drm_crtc *exynos_crtc);
void dpu_fence_event_show(struct exynos_drm_crtc *exynos_crtc);
void dpu_dump_with_event(struct exynos_drm_crtc *exynos_crtc);
int dpu_sysmmu_fault_handler(struct iommu_fault *fault, void *data);
void dpu_profile_hiber_enter(struct exynos_drm_crtc *exynos_crtc);
void dpu_profile_hiber_exit(struct exynos_drm_crtc *exynos_crtc);
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int dpu_itmon_notifier(struct notifier_block *nb, unsigned long action,
		void *data);
#endif

static __always_inline const char *get_comp_src_name(u64 comp_src)
{
	if (comp_src == AFBC_FORMAT_MOD_SOURCE_GPU)
		return "GPU";
	else if (comp_src == AFBC_FORMAT_MOD_SOURCE_G2D)
		return "G2D";
	else
		return "";
}

/* Definitions below are used in the DECON */
#define DPU_EVENT_LOG_RETRY	3
#define DPU_EVENT_KEEP_CNT	3

struct dpu_debug {
	/* ioremap region to read TE PEND register */
	void __iomem *eint_pend;
	/* count of shodow update timeout */
	atomic_t shadow_timeout_cnt;

	/* ring buffer of event log */
	struct dpu_log *event_log;
	/* count of log buffers in each event log */
	u32 event_log_cnt;
	/* count of underrun interrupt */
	u32 underrun_cnt;
	/* array index of log buffer in event log */
	atomic_t event_log_idx;
	/* lock for saving log to event log buffer */
	spinlock_t event_lock;

	/* ring buffer of fence event log */
	struct dpu_fence_log *fence_event_log;
	/* count of log buffers in each fence event log */
	u32 fence_event_log_cnt;
	/* array index of log buffer in event log */
	atomic_t fence_event_log_idx;
	/* lock for saving log to event log buffer */
	spinlock_t fence_event_lock;

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	struct notifier_block itmon_nb;
	bool itmon_notified;
#endif
};

#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
void
exynos_atomic_commit_prepare_buf_sanity(struct drm_atomic_state *old_state);
bool exynos_atomic_commit_check_buf_sanity(struct drm_atomic_state *old_state);
#endif

#define drv_name(d) ((d)->dev->driver->name)
#define dpu_pr_err(name, id, log_lv, fmt, ...)					\
	do {									\
		if ((log_lv) >= 3)						\
			pr_err("%s[%d]: "fmt, (name), (id), ##__VA_ARGS__);	\
	} while (0)

#define dpu_pr_warn(name, id, log_lv, fmt, ...)					\
	do {									\
		if ((log_lv) >= 5)						\
			pr_warn("%s[%d]: "fmt, (name), (id), ##__VA_ARGS__);	\
	} while (0)

#define dpu_pr_info(name, id, log_lv, fmt, ...)					\
	do {									\
		if ((log_lv) >= 6)						\
			pr_info("%s[%d]: "fmt, (name), (id), ##__VA_ARGS__);	\
	} while (0)

#define dpu_pr_debug(name, id, log_lv, fmt, ...)				\
	do {									\
		if ((log_lv) >= 7)						\
			pr_info("%s[%d]: "fmt, (name), (id), ##__VA_ARGS__);	\
	} while (0)

#endif /* __EXYNOS_DRM_DEBUG_H__ */
