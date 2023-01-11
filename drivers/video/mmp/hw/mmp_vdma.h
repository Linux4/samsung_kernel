/*
 * drivers/video/mmp/hw/mmp_vdma.h
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Guoqing Li <ligq@marvell.com>
 *           Jing Xiang <jxiang@marvell.com>
 *           Baoyin Shan <byshan@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MMP_VDMA_H_
#define _MMP_VDMA_H_

#include <video/mmp_disp.h>
#include <linux/genalloc.h>

#define SQULN_CTRL(overlay_id)			(((overlay_id) >> 1) ? \
	(((overlay_id) & 0x2) ? LCD_TV_SQULN_CTRL : \
	LCD_PN2_SQULN_CTRL) : LCD_PN_SQULN_CTRL)
#define LCD_SCALING_SQU_CTL_INDEX		(0x02AC)
#define SQULN_CTL_INDEX_MASK(overlay_id)	(0x3 << (26 + \
	((overlay_id) & (~0x1))))
#define SQULN_CTL_INDEX(overlay_id, index)	((index) << \
	(26 + ((overlay_id) & (~0x1))))

#define PN_GRA_VDMA_SEL_MASK			(0x3 << 20)
#define PN_VID_VDMA_SEL_MASK			(0x3 << 22)
#define TV_GRA_VDMA_SEL_MASK			(0x3 << 24)
#define TV_VID_VDMA_SEL_MASK			(0x3 << 26)
#define PN2_GRA_VDMA_SEL_MASK			(0x3 << 28)
#define PN2_VID_VDMA_SEL_MASK			(0x3 << 30)
#define VDMA_RDY_SEL_MASK(overlay_id)		(0x3 << \
	(20 + ((overlay_id) << 1)))
#define VDMA_RDY_SEL(overlay_id, voverlay_id)	((voverlay_id) << \
	(20 + ((overlay_id) << 1)))
#define VDMA_SEL_MASK(voverlay_id)		(0x7 << \
	((voverlay_id) << 2))
#define VDMA_SEL(overlay_id, voverlay_id)	((overlay_id) << \
	((voverlay_id) << 2))
#define ALL_LAYER_ALPHA_SEL			(0x02F4)

/* DC4.5 defines regs */
#define VDMA_CLK_AUTO_CTRL_DIS_MASK	(0x1 << 31)

/* DC4 lites defines the AXI_RD_CNT_MAX_SHIF as 16 while on DC4 it's 20 */
#define AXI_RD_CNT_MAX_SHIFT			((vdma->version == 0x14) ? (16) : (20))
#define AXI_RD_CNT_MAX(cnt)			((cnt) << AXI_RD_CNT_MAX_SHIFT)
#define RD_BURST_SIZE_SHIFT			(4)
#define RD_BURST_SIZE(rbs)			((rbs) << RD_BURST_SIZE_SHIFT)
#define WR_BURST_SIZE_SHIFT			(6)
#define WR_BURST_SIZE(wbs)			((wbs) << WR_BURST_SIZE_SHIFT)
#define DC_ENA(en)				((en) ? 2 : 0)
#define CH_ENA(en)				(en)
#define HDR_ENA(en)				((en) << 3)
#define DEC_HAS_COMP(vdma_id)			(1 << (vdma_id))
#define DEC_HAS_ALPHA(vdma_id)			(1 << ((vdma_id) + 4))
#define DEC_BYPASS_EN				(1 << 31)
#define squ_switch_index(overlay_id, index) do { \
	tmp = readl_relaxed(vdma->lcd_reg_base + LCD_SCALING_SQU_CTL_INDEX); \
	tmp &= ~SQULN_CTL_INDEX_MASK(overlay_id);       \
	tmp |= SQULN_CTL_INDEX(overlay_id, index++);      \
	writel_relaxed(tmp, vdma->lcd_reg_base + LCD_SCALING_SQU_CTL_INDEX); \
} while (0)

#define SETTING_READY(id) (1 << (24 + (id)))
#define VDMA_CLK_DIS(id)       (((id) < 2) ? (1 << (id)) : \
	(7 << (2 + ((id) - 2) * 3)))

#define VDMA_SRAM_ALIGN(x)	ALIGN(x, 256)

struct mmp_vdma_ch_reg {
	u32 dc_saddr;
	u32 dc_size;
	u32 ctrl;
	u32 src_size;
	u32 src_addr;
	u32 dst_addr;
	u32 dst_size;
	u32 pitch;
	u32 ram_ctrl0;
	u32 reserved0[0x3];
	u32 hdr_addr;
	u32 hdr_size;
	u32 hskip;
	u32 vskip;
	union {
		u32 dbg0;
		u32 ch_err_stat;
	};
	union {
		u32 dbg1;
		u32 ch_dbg1;
	};
	union {
		u32 dbg2;
		u32 ch_dbg2;
	};
	union {
		u32 dbg3;
		u32 ch_dbg3;
	};
	u32 ch_dbg4;
	u32 ch_dbg5;
	u32 ch_dbg6;
	u32 ch_dbg7;
	u32 reserved1[0x28];
};
struct mmp_vdma_reg {
	union {
		u32 irqr_0to3;
		u32 irq;
	};
	union {
		u32 irqr_4to7;
		u32 irq_en;
	};
	u32 irqm_0to3;
	u32 irqm_4to7;
	u32 irqs_0to3;
	u32 irqs_4to7;
	u32 reserved0[0x2];
	u32 main_ctrl;
	u32 clk_ctrl;
	u32 dec_ctrl;
	u32 dec_dbg;
	u32 reserved2[0x4];
	u32 top_dbg0;
	u32 top_dbg1;
	u32 top_dbg2;
	u32 top_dbg3;
	u32 top_dbg4;
	u32 top_dbg5;
	u32 top_dbg6;
	u32 top_dbg7;
	u32 reserved3[0x28];
	struct mmp_vdma_ch_reg ch_reg[8];
};

struct mmp_vdma {
	const char *name;
	struct clk *clk;
	void *reg_base;
	void *lcd_reg_base;
	struct device *dev;
	struct gen_pool *pool;
	u32 version;
	atomic_t decompress_ref;
	int status;

	u32 *regs_store;
	u32 regs_len;
	int vdma_channel_num;
	struct mmp_vdma_info vdma_info[0];
};

extern int vdma_dbg_init(struct device *dev);
extern void vdma_dbg_uninit(struct device *dev);
#endif	/* _MMP_VDMA_H_ */
