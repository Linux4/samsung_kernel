// SPDX-License-Identifier: GPL-2.0-only
/*
 * cal_qt/rcd_reg.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register access functions for Samsung EXYNOS Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ktime.h>

#include <cal_config.h>
#include <exynos_drm_rcd.h>
#include <rcd_cal.h>
#include <regs-rcd.h>

static struct cal_regs_desc regs_rcd[REGS_RCD_ID_MAX];

#define rcd_regs_desc(id) (&regs_rcd[id])
#define rcd_read(id, offset) cal_read(rcd_regs_desc(id), offset)
#define rcd_write(id, offset, val) cal_write(rcd_regs_desc(id), offset, val)
#define rcd_read_mask(id, offset, mask)                                        \
	cal_read_mask(rcd_regs_desc(id), offset, mask)
#define rcd_write_mask(id, offset, val, mask)                                  \
	cal_write_mask(rcd_regs_desc(id), offset, val, mask)

void rcd_regs_desc_init(void __iomem *regs, const char *name, unsigned int id)
{
	regs_rcd[id].regs = regs;
	regs_rcd[id].name = name;
}

/****************** DMA RCD CAL functions ******************/
static void rcd_reg_set_sw_reset(u32 id)
{
	rcd_write_mask(id, DMA_RCD_ENABLE, ~0, DMA_RCD_ENABLE);
}

static int rcd_reg_wait_sw_reset_status(u32 id)
{
	struct rcd_device *rcd = get_rcd_drvdata(id);
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(rcd->reg_base + DMA_RCD_ENABLE, val,
									!(val & DMA_RCD_SRESET), 10, 2000);
	if (ret) {
		cal_log_err(id, "[rcd%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

static void rcd_reg_check_cleanup(id)
{
	if (rcd_read(id, DMA_RCD_IRQ) & DMA_RCD_DEADLOCK_IRQ) {
		rcd_reg_set_sw_reset(id);
		rcd_reg_wait_sw_reset_status(id);
	}
}

static void rcd_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	rcd_write_mask(id, DMA_RCD_IRQ, val, DMA_RCD_ALL_IRQ_MASK);
}

static void rcd_reg_clear_irq(u32 id, u32 irq)
{
	rcd_write_mask(id, DMA_RCD_IRQ, ~0, irq);
}

static void rcd_reg_set_irq_enable(u32 id)
{
	rcd_write_mask(id, DMA_RCD_IRQ, ~0, DMA_RCD_IRQ_ENABLE);
}

static void rcd_reg_set_irq_disable(u32 id)
{
	rcd_write_mask(id, DMA_RCD_IRQ, 0, DMA_RCD_IRQ_ENABLE);
}

static void rcd_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = DMA_RCD_QOS_LUT_LOW;
	else
		reg_id = DMA_RCD_QOS_LUT_HIGH;

	rcd_write(id, reg_id, qos_t);
}

static void rcd_reg_set_sram_clk_gate_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	rcd_write_mask(id, DMA_RCD_DYNAMIC_GATING_EN, val, DMA_RCD_SRAM_CG_EN);
}

static void rcd_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	rcd_write_mask(id, DMA_RCD_DYNAMIC_GATING_EN, val, DMA_RCD_DG_EN_ALL);
}

static void rcd_reg_set_ic_max(u32 id, u32 ic_max)
{
	rcd_write_mask(id, DMA_RCD_IN_CTRL_0, DMA_RCD_IC_MAX(ic_max),
				   DMA_RCD_IC_MAX_MASK);
}

void rcd_reg_set_block_mode(u32 id, bool en, u32 x, u32 y, u32 w, u32 h)
{
	if (!en) {
		rcd_write_mask(id, DMA_RCD_IN_CTRL_0, 0, DMA_RCD_BLOCK_EN);
		return;
	}

	rcd_write(id, DMA_RCD_BLOCK_OFFSET,
			  DMA_RCD_BLK_OFFSET_Y(y) | DMA_RCD_BLK_OFFSET_X(x));
	rcd_write(id, DMA_RCD_BLOCK_SIZE,
			  DMA_RCD_BLK_HEIGHT(h) | DMA_RCD_BLK_WIDTH(w));
	rcd_write_mask(id, DMA_RCD_BLK_VALUE, DMA_RCD_PIX_VALUE(0xFF),
				   DMA_RCD_PIX_VALUE_MASK);

	rcd_write_mask(id, DMA_RCD_IN_CTRL_0, ~0, DMA_RCD_BLOCK_EN);
}

static void rcd_reg_set_coordinates(u32 id, struct rcd_params_info *p)
{
	rcd_write(id, DMA_RCD_SRC_OFFSET,
			  DMA_RCD_SRC_OFFSET_Y(0) | DMA_RCD_SRC_OFFSET_X(0));
	rcd_write(id, DMA_RCD_SRC_SIZE,
			  DMA_RCD_SRC_HEIGHT(p->total_height) |
				  DMA_RCD_SRC_WIDTH(p->total_width));
	rcd_write(id, DMA_RCD_IMG_SIZE,
			  DMA_RCD_IMG_HEIGHT(p->total_height) |
				  DMA_RCD_IMG_WIDTH(p->total_width));

	rcd_reg_set_block_mode(id, p->block_en, p->block_rect.x,
			       p->block_rect.y, p->block_rect.w,
			       p->block_rect.h);
}

void rcd_reg_set_partial(u32 id, struct rcd_rect rect)
{
	rcd_write(id, DMA_RCD_SRC_OFFSET,
			  DMA_RCD_SRC_OFFSET_Y(rect.y) | DMA_RCD_SRC_OFFSET_X(rect.x));
	rcd_write(id, DMA_RCD_IMG_SIZE,
			  DMA_RCD_IMG_HEIGHT(rect.h) | DMA_RCD_IMG_WIDTH(rect.w));

	cal_log_debug(id, "%s, x:%d, y:%d, w:%d, h:%d\n", __func__, rect.x, rect.y,
				 rect.w, rect.h);
}

static void rcd_reg_set_base_addr(u32 id, u32 addr)
{
	rcd_write(id, DMA_RCD_BASEADDR_P0, addr);
}

static void rcd_reg_set_deadlock(u32 id, u32 en, u32 dl_num)
{
	u32 val = en ? ~0 : 0;

	rcd_write_mask(id, DMA_RCD_DEADLOCK_CTRL, val, DMA_RCD_DEADLOCK_NUM_EN);
	rcd_write_mask(id, DMA_RCD_DEADLOCK_CTRL, DMA_RCD_DEADLOCK_NUM(dl_num),
				   DMA_RCD_DEADLOCK_NUM_MASK);
}

void rcd_reg_set_config(u32 id)
{
	struct rcd_device *rcd = get_rcd_drvdata(id);
	struct rcd_params_info *param = &rcd->param;

	rcd_reg_set_coordinates(id, param);
	rcd_reg_set_base_addr(id, rcd->dma_addr);
}

u32 rcd_reg_get_irq_and_clear(u32 id)
{
	u32 val;

	val = rcd_read(id, DMA_RCD_IRQ);
	rcd_reg_clear_irq(id, val);

	return val;
}

void rcd_reg_init(u32 id)
{
	rcd_reg_check_cleanup(id);
	rcd_reg_clear_irq(id, DMA_RCD_ALL_IRQ_CLEAR);
	rcd_reg_set_irq_mask_all(id, 0);
	rcd_reg_set_irq_enable(id);
	rcd_reg_set_in_qos_lut(id, 0, 0x44444444);
	rcd_reg_set_in_qos_lut(id, 1, 0x44444444);
	rcd_reg_set_sram_clk_gate_en(id, 0);
	rcd_reg_set_dynamic_gating_en_all(id, 0);
	rcd_reg_set_ic_max(id, 0x28);
	rcd_reg_set_deadlock(id, 1, 0x7fffffff);
}

int rcd_reg_deinit(u32 id, bool reset)
{
	rcd_reg_set_irq_disable(id);
	rcd_reg_clear_irq(id, DMA_RCD_ALL_IRQ_CLEAR);
	rcd_reg_set_irq_mask_all(id, 1);

	if (reset) {
		rcd_reg_set_sw_reset(id);
		if (rcd_reg_wait_sw_reset_status(id))
			return -1;
	}

	return 0;
}

void rcd_dma_dump_regs(u32 id, void __iomem *dma_regs)
{
	cal_log_info(id, "\n=== DPU_DMA(RCD%d) SFR DUMP ===\n", id);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0000, 0x144);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0300, 0x24);

	cal_log_info(id, "=== DPU_DMA(RCD%d) SHADOW SFR DUMP ===\n", id);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0000 + DMA_RCD_SHD_OFFSET, 0x144);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0300 + DMA_RCD_SHD_OFFSET, 0x24);
}