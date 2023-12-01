/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG RCD CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_RCD_CAL_H__
#define __SAMSUNG_RCD_CAL_H__

void rcd_reg_init(u32 id);
int rcd_reg_deinit(u32 id, bool reset);
void rcd_reg_set_config(u32 id);
void rcd_reg_set_partial(u32 id, struct rcd_rect rect);
void rcd_reg_set_block_mode(u32 id, bool en, u32 x, u32 y, u32 w, u32 h);
u32 rcd_reg_get_irq_and_clear(u32 id);

void rcd_regs_desc_init(void __iomem *regs, const char *name, unsigned int id);
void rcd_dma_dump_regs(u32 id, void __iomem *dma_regs);

#endif /* __SAMSUNG_RCD_CAL_H__ */
