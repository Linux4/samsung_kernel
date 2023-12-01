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

#include "../base/mfc_utils.h"

#include "../base/mfc_regs.h"

#define mfc_fg_clear_int()						\
		do {							\
			MFC_CORE_WRITEL(0, REG_FG_PICTURE_DONE);	\
			MFC_CORE_WRITEL(0, REG_FG_INTERRUPT_STATUS);	\
		} while (0)

#define mfc_fg_get_rdma_sbwc_err()	((MFC_CORE_READL(REG_FG_INTERRUPT_STATUS)	\
					>> REG_FG_INT_STAT_RDMA_SBWC_ERR_SHFT)		\
					& REG_FG_INT_STAT_RDMA_SBWC_ERR_MASK)
#define mfc_fg_get_wdma_y_addr()	MFC_FG_DMA_READL(REG_FG_WDMA_LUM_IMG_BASE_ADDR_1P_FRO0)

#define mfc_fg_reset()			MFC_CORE_WRITEL(1, REG_FG_CLK_RESET)
#define mfc_fg_picture_start()		MFC_CORE_WRITEL(1, REG_FG_PICTURE_START)

void mfc_fg_set_geometry(struct mfc_core *core, struct mfc_ctx *ctx);
void mfc_fg_set_sei_info(struct mfc_core *core, struct mfc_ctx *ctx, int index);
void mfc_fg_set_rdma(struct mfc_core *core, struct mfc_ctx *ctx, struct mfc_buf *mfc_buf);
void mfc_fg_set_wdma(struct mfc_core *core, struct mfc_ctx *ctx, struct mfc_buf *mfc_buf);

#endif /* __MFC_PLUGIN_REG_API_H */
