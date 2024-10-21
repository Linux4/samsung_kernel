/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_C3D_H__
#define __MTK_DISP_C3D_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#define C3D_BYPASS_SHADOW BIT(0)

#define C3D_U32_PTR(x) ((unsigned int *)(unsigned long)(x))
#define c3d_min(a, b)  (((a) < (b)) ? (a) : (b))

/*******************************/
/* field definition */
/* ------------------------------------------------------------- */
#define C3D_EN                             (0x000)
#define C3D_CFG                            (0x004)
#define C3D_INTEN                          (0x00C)
#define C3D_INTSTA                         (0x010)
#define C3D_SIZE                           (0x024)
#define C3D_SHADOW_CTL                     (0x030)
#define C3D_C1D_000_001                    (0x034)
#define C3D_C1D_002_003                    (0x038)
#define C3D_C1D_004_005                    (0x03C)
#define C3D_C1D_006_007                    (0x040)
#define C3D_C1D_008_009                    (0x044)
#define C3D_C1D_010_011                    (0x048)
#define C3D_C1D_012_013                    (0x04C)
#define C3D_C1D_014_015                    (0x050)
#define C3D_C1D_016_017                    (0x054)
#define C3D_C1D_018_019                    (0x058)
#define C3D_C1D_020_021                    (0x05C)
#define C3D_C1D_022_023                    (0x060)
#define C3D_C1D_024_025                    (0x064)
#define C3D_C1D_026_027                    (0x068)
#define C3D_C1D_028_029                    (0x06C)
#define C3D_C1D_030_031                    (0x070)
#define C3D_SRAM_CFG                       (0x074)
#define C3D_SRAM_STATUS                    (0x078)
#define C3D_SRAM_RW_IF_0                   (0x07C)
#define C3D_SRAM_RW_IF_1                   (0x080)
#define C3D_SRAM_RW_IF_2                   (0x084)
#define C3D_SRAM_RW_IF_3                   (0x088)
#define C3D_SRAM_PINGPONG                  (0x08C)
#define C3D_R2Y_09                         (0x0C0)
#define C3D_Y2R_09                         (0x0E8)


#define C3D_3DLUT_SIZE_17BIN (17 * 17 * 17 * 3)
#define C3D_3DLUT_SIZE_9BIN (9 * 9 * 9 * 3)

#define DISP_C3D_SRAM_SIZE_17BIN (17 * 17 * 17)
#define DISP_C3D_SRAM_SIZE_9BIN (9 * 9 * 9)

struct DISP_C3D_REG_17BIN {
	unsigned int lut3d_reg[C3D_3DLUT_SIZE_17BIN];
};

struct DISP_C3D_REG_9BIN {
	unsigned int lut3d_reg[C3D_3DLUT_SIZE_9BIN];
};

struct mtk_disp_c3d_data {
	bool support_shadow;
	bool need_bypass_shadow;
	int bin_num;
	int c3d_sram_start_addr;
	int c3d_sram_end_addr;
};

struct mtk_disp_c3d_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_c3d_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_c3d_primary {
	struct DISP_C3D_REG_17BIN c3d_reg_17bin;
	struct DISP_C3D_REG_9BIN c3d_reg_9bin;
	unsigned int c3d_sram_cfg[DISP_C3D_SRAM_SIZE_17BIN];
	unsigned int c3d_sram_cfg_9bin[DISP_C3D_SRAM_SIZE_9BIN];
	unsigned int c3d_lut1d[DISP_C3D_1DLUT_SIZE];
	struct DISP_C3D_LUT c3d_ioc_data;
	atomic_t c3d_force_relay;
	atomic_t c3d_get_irq;
	atomic_t c3d_eventctl;
	wait_queue_head_t c3d_get_irq_wq;
	struct wakeup_source *c3d_wake_lock;
	bool c3d_wake_locked;
	bool set_lut_flag;
	bool update_sram_ignore;
	bool skip_update_sram;
	spinlock_t c3d_clock_lock;
	struct mutex c3d_global_lock;
	struct mutex c3d_power_lock;
	struct mutex c3d_lut_lock;
	struct cmdq_pkt *sram_pkt;
	atomic_t c3d_sram_hw_init;
};

struct mtk_disp_c3d {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_c3d_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_c3d_primary *primary_data;
	struct mtk_disp_c3d_tile_overhead tile_overhead;
	struct mtk_disp_c3d_tile_overhead_v tile_overhead_v;
	bool pkt_reused;
	struct cmdq_reuse reuse_c3d[4913 * 2];
	atomic_t c3d_is_clock_on;
	atomic_t c3d_force_sram_apb;
	bool has_set_1dlut;
	bool set_partial_update;
	unsigned int roi_height;
};


int mtk_drm_ioctl_c3d_get_bin_num(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_get_irq(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_get_irq_status(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_set_lut(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_bypass_c3d(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

void mtk_disp_c3d_debug(struct drm_crtc *crtc, const char *opt);
void disp_c3d_set_bypass(struct drm_crtc *crtc, int bypass);
inline struct mtk_disp_c3d *comp_to_c3d(struct mtk_ddp_comp *comp);
void mtk_c3d_regdump(struct mtk_ddp_comp *comp);
// for displayPQ update to swpm tppa
unsigned int disp_c3d_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

