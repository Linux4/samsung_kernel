/*
 * Copyright (C) 2011 Marvell International Ltd.
 *				 Libin Yang <lbyang@marvell.com>
 *
 * This driver is based on soc_camera.
 * Ported from Jonathan's marvell-ccic driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MV_SC2_H
#define __MV_SC2_H

#include <linux/device.h>
#include <linux/platform_device.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-mediabus.h>
#include <linux/interrupt.h>
#include <media/mrvl-camera.h>
#ifdef CONFIG_MARVELL_MEDIA_MMU
#include <linux/m4u.h>
#endif

#define SC2_IDI_SEL_NONE      (0x0)
#define SC2_IDI_SEL_DPCM      (0x1)
#define SC2_IDI_SEL_REPACK    (0x2)
#define SC2_IDI_SEL_PARALLEL  (0x3)
#define SC2_IDI_SEL_AHB       (0x4)
#define SC2_IDI_SEL_REPACK_OTHER	(0x5)

#define SC2_CH_NUM 16
#define SC2_CH_MASK (~0xffff)

enum msc2_dma_desc_status {
	MSC2_DESC_UNALLOCATED = 0,
	MSC2_DESC_ALLOCATED,
};

enum msc2_ch_status {
	MSC2_CH_DISABLE = 0,
	MSC2_CH_ENABLE,
};

struct msc2_dma_desc {
	u32 dma_addr;
	u32 segment_len;
};

enum msc2_mmu_version {
	MSC2_MMU_VERSION_0 = 0,
	MSC2_MMU_VERSION_1,
};

struct msc2_ch_info {
	u32		tid;	/* translation id */
	dma_addr_t	daddr;
	__u32	sizeimage;
	__u16	byteperline;
	struct msc2_dma_desc *dma_desc;
	dma_addr_t	dma_desc_pa;
	int	dma_desc_nent;
#ifdef CONFIG_MARVELL_MEDIA_MMU
	/* M4U style descriptor chain */
	struct m4u_bdt	*bdt;
#endif
};

struct msc2_buffer {
	struct	vb2_buffer vb2_buf;
	struct platform_device *pdev;
	struct list_head queue;
	struct	msc2_ch_info	ch_info[VIDEO_MAX_PLANES];
	enum msc2_dma_desc_status status;
	int list_init_flag;
	/* spinlock_t	mbuf_lock; */
};

struct msc2_mmu_dev {
	unsigned long channel_map;
	u32 ch_matrix[SC2_CH_NUM];
	u32 ch_status[SC2_CH_NUM];
	int free_chs;
	int version;
	unsigned int irq;
	/*
	 * bit0: 1: write bypass sc2 mmu
	 *		 0: write no bypass sc2 mmu
	 * bit1: 1: read bypass sc2 mmu
	 *		 0: read no bypass sc2 mmu
	 */
#define SC2_WBYPASS 0
#define SC2_RBYPASS 1
	unsigned long rw_bypass;
	int usr_cnt;
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *mmu_regs;
	enum msc2_dma_desc_status status;
	spinlock_t msc2_lock;
	int id;		/* identify the SC2 if multiple SC2 devices exist */
	struct list_head list;		/* registered sc2 devices */

	struct msc2_mmu_ops *ops;
};

struct msc2_mmu_ops {
	struct module *owner;
	int (*rbypass)(struct msc2_mmu_dev *sc2_dev, int bypass);
	int (*wbypass)(struct msc2_mmu_dev *sc2_dev, int bypass);
	int (*bypass_status)(struct msc2_mmu_dev *sc2_dev);
	int (*get_tid)(struct msc2_mmu_dev *sc2_dev, u16 frame_st,
				u16 aid, u16 grp_id);
	int (*acquire_ch)(struct msc2_mmu_dev *sc2_dev, u32 tid);
	int (*release_ch)(struct msc2_mmu_dev *sc2_dev, u32 tid);
	int (*enable_ch)(struct msc2_mmu_dev *sc2_dev,
				const u32 *tid, int nr_chs);
	int (*disable_ch)(struct msc2_mmu_dev *sc2_dev,
				const u32 *tid, int nr_chs);
	int (*reset_ch)(struct msc2_mmu_dev *sc2_dev,
				const u32 *tid, int nr_chs);
	int (*config_ch)(struct msc2_mmu_dev *sc2_dev,
				const struct msc2_ch_info *info, int nr_chs);
	int (*alloc_desc)(struct vb2_buffer *vb);
	void (*free_desc)(struct vb2_buffer *vb);
	int (*setup_sglist)(struct vb2_buffer *vb);
	int (*unmap_sglist)(struct vb2_buffer *vb);
	int (*map_dmabuf)(struct vb2_buffer *vb);
	int (*unmap_dmabuf)(struct vb2_buffer *vb);
};

struct ccic_dma_dev {
	int usr_cnt;
	struct msc2_ccic_dev *ccic_dev;
	struct ccic_dma_ops *ops;
	irqreturn_t (*handler)(struct ccic_dma_dev *dev, u32 irqs);
	void *private;
	int height;
	int width;
	u32 pixfmt;
	enum v4l2_mbus_pixelcode code;
	int bytesperline;
	int sizeimage;
	void *priv;
	int id;
};

struct ccic_dma_ops {
	int (*setup_image)(struct ccic_dma_dev *dma_dev);
	void (*shadow_ready)(struct ccic_dma_dev *dma_dev);
	void (*shadow_empty)(struct ccic_dma_dev *dma_dev);
	void (*set_yaddr)(struct ccic_dma_dev *dma_dev, u32 addr);
	void (*set_uaddr)(struct ccic_dma_dev *dma_dev, u32 addr);
	void (*set_vaddr)(struct ccic_dma_dev *dma_dev, u32 addr);
	void (*set_addr)(struct ccic_dma_dev *dma_dev, u8 chnl, u32 addr);
	int (*mbus_fmt_rating)(u32 fourcc, enum v4l2_mbus_pixelcode code);
	void (*ccic_enable)(struct ccic_dma_dev *dma_dev);
	void (*ccic_disable)(struct ccic_dma_dev *dma_dev);

};

struct ccic_ctrl_dev {
	int usr_cnt;
	struct msc2_ccic_dev *ccic_dev;
	struct ccic_ctrl_ops *ops;
	irqreturn_t (*handler)(struct ccic_ctrl_dev *ctrl_dev, u32 irqs);
	void *priv;
	int id;
	struct mipi_csi2 csi;
	struct clk *dphy_clk;
	struct clk *csi_clk;
	struct clk *clk4x;
	struct clk *ahb_clk;
	struct clk *mclk;
};

struct ccic_ctrl_ops {
	void (*irq_mask)(struct ccic_ctrl_dev *ctrl_dev, int on);
	void (*config_dphy)(struct ccic_ctrl_dev *ctrl_dev); /* reconsider */
	void (*config_idi)(struct ccic_ctrl_dev *ctrl_dev, int sel);
	void (*config_mbus)(struct ccic_ctrl_dev *ctrl_dev,
				enum v4l2_mbus_type bus_type,
				unsigned int mbus_flags, int enable);
	void (*power_up)(struct ccic_ctrl_dev *ctrl_dev);
	void (*power_down)(struct ccic_ctrl_dev *ctrl_dev);
	void (*clk_enable)(struct ccic_ctrl_dev *ctrl_dev);
	void (*clk_disable)(struct ccic_ctrl_dev *ctrl_dev);
	void (*clk_change)(struct ccic_ctrl_dev *ctrl_dev,
				u32 mipi_bps, u8 lanes, u8 bpp);
};

struct msc2_ccic_dev {
	struct ccic_dma_dev *dma_dev;
	struct ccic_ctrl_dev *ctrl_dev;
	struct list_head list;
	unsigned int irq;
	void __iomem *base;
	struct platform_device *pdev;
	int id;
	struct device *dev;
	struct resource *res;
	spinlock_t ccic_lock;  /* protect the struct members and HW */
	unsigned long flags;		/* Indicate frame buffer state */
	enum v4l2_mbus_type bus_type;
	int dma_burst;
	int lane_num;
	unsigned int mbus_flags;
	int ahb_enable;
	int sync_ccic1_pin;
};

int msc2_get_sc2(struct msc2_mmu_dev **sc2_host, int id);
void msc2_put_sc2(struct msc2_mmu_dev **sc2_dev);
int msc2_get_ccic_dma(struct ccic_dma_dev **dma_host, int id,
		irqreturn_t (*handler)(struct ccic_dma_dev *, u32));
void msc2_put_ccic_dma(struct ccic_dma_dev **dma_dev);
int msc2_get_ccic_ctrl(struct ccic_ctrl_dev **ctrl_host, int id,
		irqreturn_t (*handler)(struct ccic_ctrl_dev *, u32));
void msc2_put_ccic_ctrl(struct ccic_ctrl_dev **ctrl_dev);
void msc2_dump_regs(struct msc2_ccic_dev *ccic_dev);
int msc2_get_ccic_dev(struct msc2_ccic_dev **host_ccic, int id);

#endif
