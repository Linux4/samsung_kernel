/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __GSP_KCFG_H__
#define __GSP_KCFG_H__
#include <asm/uaccess.h>
#include <video/gsp_types_shark_adf.h>
#include "gsp_sync.h"
#include "gsp_drv_common.h"


struct gsp_kcfg_info {
	struct gsp_cfg_info cfg;
	struct gsp_cfg_info __user *user_cfg_addr;
	struct gsp_fence_data data;
	int done;
	int async_flag;
	struct list_head kcfg_list;
	int tag;
	/*start from trigger, used to timeout judgment*/
    struct timespec start_time;
	int wait_cost;
	int sched_cost;
	int mmu_id;
	uint32_t frame_id;
};

void gsp_kcfg_init(struct gsp_kcfg_info *kcfg);
void gsp_kcfg_scale_set(struct gsp_kcfg_info *kcfg);
int gsp_kcfg_fill(struct gsp_cfg_info __user *ucfg,
					struct gsp_kcfg_info *kcfg,
					struct gsp_sync_timeline *tl,
					uint32_t frame_id);
void gsp_kcfg_destroy(struct gsp_kcfg_info *kcfg);
int gsp_kcfg_iommu_map(struct gsp_kcfg_info *kcfg);
int gsp_kcfg_iommu_unmap(struct gsp_kcfg_info *kcfg);
uint32_t gsp_kcfg_reg_set(struct gsp_kcfg_info *kcfg,
		unsigned long reg_addr);
void gsp_current_kcfg_set(struct gsp_context *ctx,
		struct gsp_kcfg_info *kcfg);
struct gsp_kcfg_info *gsp_current_kcfg_get(struct gsp_context *ctx);
uint32_t gsp_kcfg_get_des_addr(struct gsp_kcfg_info *kcfg);
#endif
