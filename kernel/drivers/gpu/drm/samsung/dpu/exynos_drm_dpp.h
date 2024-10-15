/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_dpp.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Seong-gyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_DPP_H_
#define _EXYNOS_DRM_DPP_H_

#include <exynos_drm_drv.h>
#include <samsung_drm.h>

#include <dpp_cal.h>

#define plane_to_dpp(p) ((p)->ctx)

enum dpp_state {
	DPP_STATE_OFF = 0,
	DPP_STATE_ON,
};

struct dpuf_votf {
	bool enabled;		/* whether the votf be enabled */
	spinlock_t slock;	/* slock for ref-count */
	u32 enable_count;	/* ring clock ref-count */
	u32 base_addr;		/* physical address of vOTF block in DPUF */
};

struct dpuf {
	u32 id;			/* ID for DPUF block */
	struct dpp_dpuf_resource resource;
	struct dpuf_votf *votf;
};

struct rcd_block_mode {
        int x;
        int y;
        u32 w;
        u32 h;
};

struct exynos_hdr;
struct dpp_device {
	struct device *dev;

	u32 id;
	u32 port;
	struct ip_version dpp_ver;
	struct ip_version dma_ver;
	unsigned long attr;
	enum dpp_state state;

	int dma_irq;
	int dpp_irq;

	uint32_t *pixel_formats;
	unsigned int num_pixel_formats;

	struct dpp_regs	regs;
	struct dpp_params_info win_config;

	spinlock_t slock;
	spinlock_t dma_slock;

	int decon_id;		/* connected DECON id */
	bool protection;

	const struct dpp_restrict restriction;
	struct dpuf *dpuf;

	struct exynos_drm_plane *plane;
	struct exynos_hdr *hdr;

	bool sfr_dma_en;
	bool rcd_block_mode_en;
	struct rcd_block_mode rcd_blk;
};

extern struct dpuf *dpuf_blkdata[MAX_DPUF_CNT];
extern struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static inline struct dpp_device *get_dpp_drvdata(u32 id)
{
	if (id < MAX_DPP_CNT)
		return dpp_drvdata[id];

	return NULL;
}

void dpp_dump(struct dpp_device *dpps[], int dpp_cnt);
void rcd_dump(struct dpp_device *dpp);

void dpp_init_dpuf_resource_cnt(void);
void exynos_dpuf_set_ring_clk(struct dpuf *dpuf, bool en);
int exynos_parse_formats(struct device *dev, uint32_t **pixel_formats);
#endif
