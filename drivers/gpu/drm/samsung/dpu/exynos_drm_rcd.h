/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_rcd.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_RCD_H_
#define _EXYNOS_DRM_RCD_H_

#include <exynos_drm_drv.h>
#include <samsung_drm.h>

#define MAX_RCD_CNT 1
#define MAX_RCD_WIN 2

enum rcd_state {
	RCD_STATE_ON = 0,
	RCD_STATE_OFF,
};

enum {
	RCD_RECT_X = 0,
	RCD_RECT_Y,
	RCD_RECT_W,
	RCD_RECT_H,
};

struct rcd_rect {
	u32 x;
	u32 y;
	u32 w;
	u32 h;
};

struct img_desc {
	u32 size;
	u8* buf;
};

struct rcd_params_info {
	u32 total_width;
	u32 total_height;
	u32 num_win;
	u32 *win_pos[MAX_RCD_WIN];
	struct img_desc imgs[MAX_RCD_WIN];

	u32 block_en;
	struct rcd_rect block_rect;
};

struct rcd_device {
	u32 id;
	struct device *dev;
	struct decon_device *decon;

	int irq;
	void __iomem *reg_base;
	const struct exynos_rcd_funcs *funcs;

	bool initialized;
	enum rcd_state state;

	struct rcd_params_info param;

	u32 buf_size;
	struct dma_buf *buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
	dma_addr_t dma_addr;
	void *buf_vaddr;

	spinlock_t slock;
	struct mutex lock;
};

struct exynos_rcd_funcs {
	void (*prepare)(struct rcd_device *rcd);
	void (*update)(struct rcd_device *rcd);
	void (*set_partial)(struct rcd_device *rcd, const struct drm_rect *r);
	void (*unprepare)(struct rcd_device *rcd);
};

extern struct rcd_device *rcd_drvdata[MAX_RCD_CNT];

static inline struct rcd_device *get_rcd_drvdata(u32 id)
{
	if (id < MAX_RCD_CNT)
		return rcd_drvdata[id];

	return NULL;
}

void exynos_rcd_enable(struct rcd_device *rcd);
void exynos_rcd_disable(struct rcd_device *rcd);

#if IS_ENABLED(CONFIG_EXYNOS_DMA_RCD)
void rcd_dump(struct rcd_device *rcd);
struct rcd_device *exynos_rcd_register(struct decon_device *decon);
#else
static inline void rcd_dump(struct rcd_device *rcd) { }
static inline struct rcd_device *exynos_rcd_register(struct decon_device *decon) { return NULL; }
#endif

#endif
