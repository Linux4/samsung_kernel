/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_reg_api.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_REG_API_H
#define __MFC_PLUGIN_REG_API_H __FILE__

#include "base/mfc_utils.h"

#include "base/mfc_regs.h"

#define mfc_fg_enable_irq(val)	MFC_CORE_WRITEL(val, REG_FG_INTERRUPT_MASK)
#define mfc_fg_clear_int()						\
		do {							\
			MFC_CORE_WRITEL(0, REG_FG_PICTURE_DONE);	\
			MFC_CORE_WRITEL(0, REG_FG_INTERRUPT_STATUS);	\
		} while (0)

#define mfc_fg_get_rdma_sbwc_err()	((MFC_CORE_READL(REG_FG_INTERRUPT_STATUS)	\
					>> REG_FG_INT_STAT_RDMA_SBWC_ERR_SHFT)		\
					& REG_FG_INT_STAT_RDMA_SBWC_ERR_MASK)
#define mfc_fg_get_wdma_y_addr(idx)	MFC_FG_DMA_READL(REG_FG_WDMA_LUM_IMG_BASE_ADDR_1P_FRO0 + ((idx) * 4))

#define mfc_fg_picture_start()		MFC_CORE_WRITEL(1, REG_FG_PICTURE_START)

#define mfc_fg_run_cmd()		MFC_CORE_WRITEL(1, REG_FG_RUN_CMD)
#define mfc_fg_update_run_cnt(cnt)	MFC_CORE_WRITEL(cnt, REG_FG_RUN_CNT)

#define MFC_FG_SFR_INTERLEAVED(__SFR, idx, offset) \
	(((idx) == 0) ? __SFR : __SFR##_FRO1 + (((idx) - 1) * (offset)))

int mfc_fg_set_cmd_to_idle(struct mfc_core *core);
int mfc_fg_reset(struct mfc_core *core);
void mfc_fg_set_geometry(struct mfc_core *core, struct mfc_ctx *ctx);
void mfc_fg_set_sei_info(struct mfc_core *core, struct mfc_ctx *ctx, int index);
void mfc_fg_set_rdma(struct mfc_core *core, struct mfc_ctx *ctx, struct mfc_buf *mfc_buf);
void mfc_fg_set_wdma(struct mfc_core *core, struct mfc_ctx *ctx, struct mfc_buf *mfc_buf);
bool mfc_fg_is_shadow_avail(struct mfc_core *core, struct mfc_core_ctx *core_ctx);

#endif /* __MFC_PLUGIN_REG_API_H */
