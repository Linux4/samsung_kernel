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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#include <media/mv_sc2.h>
#include "mv_sc2_mmu.h"

#define MV_SC2_MMU_NAME "mv_sc2_mmu"

static LIST_HEAD(sc2_devices);
static DEFINE_MUTEX(list_lock);

static int tid_to_ch(struct msc2_mmu_dev *sc2_dev, u32 tid)
{
	int i;

	for (i = 0; i < SC2_CH_NUM; i++)
		if (sc2_dev->ch_matrix[i] == tid)
			return i;

	return -1;
}

static inline unsigned long msc2_find_free_channel(struct msc2_mmu_dev *sc2_dev)
{
	return ffz(sc2_dev->channel_map);
}

static int msc2_get_tid(struct msc2_mmu_dev *sc2_dev, u16 frame_st,
						u16 aid, u16 grp_id)
{
	u8 aid_shift;

	if (sc2_dev->version == MSC2_MMU_VERSION_1) {
		aid_shift = 2;
		/* version 1 has one MAC. CCIC is 0. MAC is 1. */
		if (grp_id > 2) {
			WARN_ON(1);
			return -EINVAL;
		}
	} else
		aid_shift = 3;

	return (frame_st << 16) | (aid << aid_shift) | grp_id;
}

static int msc2_acquire_channel(struct msc2_mmu_dev *sc2_dev, u32 tid)
{
	int ch;
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	ch = msc2_find_free_channel(sc2_dev);
	if (ch == -1 || ch >= SC2_CH_NUM) {
		dev_warn(sc2_dev->dev, "no free sc2 DMA channel\n");
		spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
		return -EBUSY;
	}
	sc2_dev->ch_matrix[ch] = tid;
	set_bit(ch, &sc2_dev->channel_map);
	sc2_dev->free_chs--;
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_release_channel(struct msc2_mmu_dev *sc2_dev, u32 tid)
{
	int ch;
	unsigned long flags = 0;

	ch = tid_to_ch(sc2_dev, tid);
	if (ch == -1) {
		dev_warn(sc2_dev->dev, "releasing not acquired channel\n");
		return -ENODEV;
	}
	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	sc2_dev->ch_matrix[ch] = 0;
	clear_bit(ch, &sc2_dev->channel_map);
	sc2_dev->free_chs++;
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_wbypass_enable(struct msc2_mmu_dev *sc2_dev)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	if (sc2_dev->usr_cnt > 1) {
		/*
		 * other host is using sc2
		 */
		spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
		return -EBUSY;
	}
	msc2_mmu_write_enable(sc2_dev);
	set_bit(SC2_WBYPASS, &sc2_dev->rw_bypass);
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_wbypass_disable(struct msc2_mmu_dev *sc2_dev)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	if (sc2_dev->usr_cnt > 1) {
		/*
		 * other host is using sc2
		 */
		spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
		return -EBUSY;
	}
	msc2_mmu_write_disable(sc2_dev);
	clear_bit(SC2_WBYPASS, &sc2_dev->rw_bypass);
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_wbypass(struct msc2_mmu_dev *sc2_dev, int bypass)
{
	if (bypass)
		return msc2_wbypass_enable(sc2_dev);
	else
		return msc2_wbypass_disable(sc2_dev);
}

static int msc2_rbypass_enable(struct msc2_mmu_dev *sc2_dev)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	if (sc2_dev->usr_cnt > 1) {
		/*
		 * other host is using sc2
		 */
		spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
		return -EBUSY;
	}
	msc2_mmu_read_enable(sc2_dev);
	set_bit(SC2_RBYPASS, &sc2_dev->rw_bypass);
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_rbypass_disable(struct msc2_mmu_dev *sc2_dev)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	if (sc2_dev->usr_cnt > 1) {
		/*
		 * other host is using sc2
		 */
		spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
		return -EBUSY;
	}
	msc2_mmu_read_disable(sc2_dev);
	clear_bit(SC2_RBYPASS, &sc2_dev->rw_bypass);
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_rbypass(struct msc2_mmu_dev *sc2_dev, int bypass)
{
	if (bypass)
		return msc2_rbypass_enable(sc2_dev);
	else
		return msc2_rbypass_disable(sc2_dev);
}

static int msc2_bypass_status(struct msc2_mmu_dev *sc2_dev)
{
	return sc2_dev->rw_bypass;
}

static int msc2_ch_enable(struct msc2_mmu_dev *sc2_dev, u32 tid)
{
	int ch;

	ch = tid_to_ch(sc2_dev, tid);
	if (ch == -1) {
		dev_warn(sc2_dev->dev,
			"No such channel: %d to enable\n", tid);
		return -ENODEV;
	}

	if (sc2_dev->ch_status[ch] == MSC2_CH_ENABLE)
		return 0;

	msc2_mmu_enable_ch(sc2_dev, ch);
	sc2_dev->ch_status[ch] = MSC2_CH_ENABLE;

	return 0;
}

static int msc2_ch_disable(struct msc2_mmu_dev *sc2_dev, u32 tid)
{
	int ch;

	ch = tid_to_ch(sc2_dev, tid);
	if (ch == -1) {
		dev_warn(sc2_dev->dev,
			"No such channel: %d to disable\n", tid);
		return -ENODEV;
	}

	if (sc2_dev->ch_status[ch] == MSC2_CH_DISABLE)
		return 0;

	msc2_mmu_disable_ch(sc2_dev, ch);
	sc2_dev->ch_status[ch] = MSC2_CH_DISABLE;

	return 0;
}

#define MSC2_CH_WAIT_CNT 1000
static int msc2_ch_reset(struct msc2_mmu_dev *sc2_dev, u32 tid)
{
	int ch;
	int i = 0;

	ch = tid_to_ch(sc2_dev, tid);
	if (ch == -1) {
		dev_warn(sc2_dev->dev, "No such channel: %d to reset\n", tid);
		return -ENODEV;
	}

	msc2_mmu_disable_ch(sc2_dev, ch);

	while (!msc2_mmu_disable_done(sc2_dev, ch)) {
		if (i++ > MSC2_CH_WAIT_CNT) {
			dev_warn(sc2_dev->dev, "unable to disable %d\n", tid);
			return -EAGAIN;
		}
		ndelay(10);
	}

	sc2_dev->ch_status[ch] = MSC2_CH_DISABLE;

	return 0;
}

static int msc2_chs_reset(struct msc2_mmu_dev *sc2_dev,
				const u32 *tid, int nr_chs)
{
	int i, ret;
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	for (i = 0; i < nr_chs; i++) {
		ret = msc2_ch_reset(sc2_dev, tid[i]);
		if (ret < 0) {
			spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
			return ret;
		}
	}
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_chs_enable(struct msc2_mmu_dev *sc2_dev,
				const u32 *tid, int nr_chs)
{
	int i, ret;
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	for (i = 0; i < nr_chs; i++) {
		ret = msc2_ch_enable(sc2_dev, tid[i]);
		if (ret) {
			while ((--i) >= 0)
				msc2_ch_disable(sc2_dev, tid[i]);
			spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
			return ret;
		}
	}
	/* set clock as dynamic gate for power optimization */
	msc2_mmu_reg_write(sc2_dev, REG_SC2_AXICLK_CG, 0x0);
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static int msc2_chs_disable(struct msc2_mmu_dev *sc2_dev,
				const u32 *tid, int nr_chs)
{
	int i;
	unsigned long flags = 0;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	for (i = 0; i < nr_chs; i++)
		msc2_ch_disable(sc2_dev, tid[i]);
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return 0;
}

static inline int msc2_daddr_valid(dma_addr_t paddr, u32 size)
{
	if ((paddr + size) <= 0x7fffffff)
		return 1;
	else
		return 0;
}

static int msc2_ch_config(struct msc2_mmu_dev *sc2_dev,
				const struct msc2_ch_info *info, int nr_chs)
{
	int i, ch;
	int ret = 0;
	unsigned long flags = 0;
	dma_addr_t addr;
	int size;

	spin_lock_irqsave(&sc2_dev->msc2_lock, flags);
	for (i = 0; i < nr_chs; i++) {
		ch = tid_to_ch(sc2_dev, info[i].tid);
		if (ch == -1) {
			dev_warn(sc2_dev->dev,
				"No such channel: %d to config\n", info[i].tid);
			ret = -ENODEV;
			goto out;
		}
		WARN_ON(CCTRL_CH_ID(info[i].tid) != info[i].tid);

		msc2_mmu_reg_write(sc2_dev, REG_SC2_CTRL(ch),
					CCTRL_CH_ID(info[i].tid));
		msc2_mmu_ch_set_sva(sc2_dev, ch, (void *)info[i].daddr);

#ifdef CONFIG_MARVELL_MEDIA_MMU
		if (unlikely(WARN_ON(info[i].bdt == NULL)
			|| WARN_ON(!info[i].bdt->dscr_dma)
			|| WARN_ON(!info[i].bdt->dscr_cnt))) {
			ret = -EINVAL;
			goto out;
		}
		addr = info[i].bdt->dscr_dma;
		size = info[i].bdt->dscr_cnt * info[i].bdt->bpd;
#else
		if (unlikely(!msc2_daddr_valid(info[i].dma_desc_pa,
					(info[i].dma_desc_nent) << 3))) {
			dev_err(sc2_dev->dev,
				"descriptor phys addr is not valid");
			ret = -EINVAL;
			goto out;
		}
		addr = info[i].dma_desc_pa;
		size = info[i].dma_desc_nent << 3;
#endif
		msc2_mmu_ch_set_daddr(sc2_dev, ch, addr);
		msc2_mmu_ch_set_dsize(sc2_dev, ch, size);
		msc2_mmu_ch_enable_irq(sc2_dev, ch);
	}
	msc2_mmu_enable_irq(sc2_dev);
out:
	spin_unlock_irqrestore(&sc2_dev->msc2_lock, flags);
	return ret;
}

/* Should be called with irq enabled */
/* supposet the caller has got the lock if necessary*/
static int msc2_alloc_dma_desc(struct vb2_buffer *vb)
{
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	struct msc2_ch_info *info;
	struct device *dev = &mbuf->pdev->dev;
	int ndesc, i;

	if (mbuf->status != MSC2_DESC_UNALLOCATED)
		return 0;

	for (i = 0; i < vb->num_planes; i++) {
		info = &mbuf->ch_info[i];
		if (unlikely(info->sizeimage <= 0)) {
			dev_err(dev, "sizeimage is 0\n");
			return -EINVAL;
		}
		ndesc = info->sizeimage/PAGE_SIZE + 1;
		info->dma_desc = dma_alloc_coherent(dev,
					ndesc * sizeof(struct msc2_dma_desc),
					&info->dma_desc_pa, GFP_KERNEL);
		if (info->dma_desc == NULL) {
			while ((--i) >= 0) {
				info = &mbuf->ch_info[i];
				ndesc = info->sizeimage/PAGE_SIZE + 1;
				dma_free_coherent(dev,
					ndesc * sizeof(struct msc2_dma_desc),
					info->dma_desc, info->dma_desc_pa);
				info->dma_desc = NULL;
				info->dma_desc_pa = 0;
			}
			dev_err(dev, "Unable to get DMA descriptor array\n");
			return -ENOMEM;
		}
	}

	mbuf->status = MSC2_DESC_ALLOCATED;
	return 0;
}

/* Should be called with irq enabled */
/* supposet the caller has got the lock if necessary*/
static void msc2_free_dma_desc(struct vb2_buffer *vb)
{
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	struct msc2_ch_info *info;
	struct device *dev = &mbuf->pdev->dev;
	int ndesc, i;

	for (i = 0; i < vb->num_planes; i++) {
		info = &mbuf->ch_info[i];
		if (unlikely(info->sizeimage == 0))
			dev_err(dev, "sizeimage is 0\n");
		ndesc = info->sizeimage/PAGE_SIZE + 1;
		dma_free_coherent(dev, ndesc * sizeof(struct msc2_dma_desc),
				info->dma_desc, info->dma_desc_pa);
		info->dma_desc = NULL;
		info->dma_desc_pa = 0;
	}
	mbuf->status = MSC2_DESC_UNALLOCATED;
}

static inline int msc2_buf_addr_valid(dma_addr_t daddr, unsigned int len)
{
	int ret;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	if ((daddr + len) <= 0x7fffffff)
		ret = 1;
	else if ((daddr >= 0x200000000) && ((daddr + len) <= 0x27fffffff))
		ret = 1;
	else
		ret = 0;
#else
	if ((daddr + len) <= 0x7fffffff)
		ret = 1;
	else
		ret = 0;
#endif

	return ret;
}

/*
 * sc2 mmu only use 32bit to locate the descriptor chain pbysical address
 * 0x0000_0000 ~ 0x7fff_ffff is to 0x0000_0000 ~ 0x7fff_ffff
 * 0x8000_0000 ~ 0xffff_ffff is to 0x2_0000_0000 ~ 0x2_7fff_ffff
 * msc2_buf_addr_valid() should be always called prior to this function.
 */
static u32 msc2_buf_addr_adjust(dma_addr_t addr)
{
	u32 daddr;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	if (addr <= 0x7fffffff)
		daddr = (u32)addr;
	else if (addr >= 0x200000000 && addr <= 0x27fffffff)
		daddr = ((u32)addr & 0x7fffffff) + 0x80000000;
	else
		return -EINVAL;	/* should never be here */
#else
	daddr = addr;
#endif
	return daddr;
}

/* supposet the caller has got the lock if necessary*/
static int msc2_setup_sglist(struct vb2_buffer *vb)
{
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	struct sg_table *sg_table;
	struct scatterlist *sg;
	struct msc2_ch_info *info;
	struct device *dev = &mbuf->pdev->dev;
	struct msc2_dma_desc *desc;
	dma_addr_t dma_addr;
	unsigned int dma_len;
	int i, j;

	for (i = 0; i < vb->num_planes; i++) {
		sg_table = vb2_dma_sg_plane_desc(vb, i);
		info = &mbuf->ch_info[i];
		info->dma_desc_nent = dma_map_sg(dev, sg_table->sgl,
				sg_table->nents, DMA_FROM_DEVICE);
		if (info->dma_desc_nent <= 0) {
			info->dma_desc_nent = 0;
			while ((--i) >= 0) {
				sg_table = vb2_dma_sg_plane_desc(vb, i);
				info = &mbuf->ch_info[i];
				dma_unmap_sg(dev, sg_table->sgl,
						sg_table->nents, DMA_FROM_DEVICE);
				info->dma_desc_nent = 0;
			}
			return -EIO;  /* Need reconsidera */
		}
		desc = info->dma_desc;
		for_each_sg(sg_table->sgl, sg, info->dma_desc_nent, j) {
			dma_addr = sg_dma_address(sg);
			dma_len = sg_dma_len(sg);
			if (unlikely(!msc2_buf_addr_valid(dma_addr, dma_len))) {
				dev_err(dev, "DMA phys addr is not valid");
				return -EIO;
			}
			desc->dma_addr = msc2_buf_addr_adjust(dma_addr);
			desc->segment_len = dma_len;
			desc++;
		}
	}
	dsb();
	return 0;
}

static int msc2_unmap_sglist(struct vb2_buffer *vb)
{
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	struct sg_table *sg_table;
	struct device *dev = &mbuf->pdev->dev;
	struct msc2_ch_info *info;
	int i;

	for (i = 0; i < vb->num_planes; i++) {
		sg_table = vb2_dma_sg_plane_desc(vb, i);
		info = &mbuf->ch_info[i];
		dma_unmap_sg(dev, sg_table->sgl, sg_table->nents, DMA_FROM_DEVICE);
		info->dma_desc_nent = 0;
	}
	return 0;
}

static int msc2_map_dmabuf(struct vb2_buffer *vb)
{
	int i;
	int ret;
	unsigned long size;
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	struct msc2_ch_info *info;

	for (i = 0; i < vb->num_planes; i++) {
		info = &mbuf->ch_info[i];
		size = vb2_plane_size(vb, i);
		vb2_set_plane_payload(vb, i, size);

#ifdef CONFIG_MARVELL_MEDIA_MMU
		/* if Descriptor Chain had been setup, skip */
		if (info->bdt && info->bdt->dscr_cnt)
			continue;

		info->bdt = m4u_get_bdt(vb->planes[i].dbuf,
			vb2_dma_sg_plane_desc(vb, i),
			vb->v4l2_planes[i].data_offset,
			size, 0x3f, PAGE_SIZE - 1);
		if (info->bdt == NULL) {
			ret = -EAGAIN;
			goto err;
		}
#endif
	}

	return 0;

#ifdef CONFIG_MARVELL_MEDIA_MMU
err:
#endif
	while (--i >= 0) {
		info = &mbuf->ch_info[i];
#ifdef CONFIG_MARVELL_MEDIA_MMU
		if (info->bdt) {
			m4u_put_bdt(info->bdt);
			info->bdt = NULL;
		}
#endif
	}
	return ret;
}

static int msc2_unmap_dmabuf(struct vb2_buffer *vb)
{
	int i;
	struct msc2_ch_info *info;
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);

	for (i = 0; i < vb->num_planes; i++) {
		info = &mbuf->ch_info[i];
#ifdef CONFIG_MARVELL_MEDIA_MMU
		if (info->bdt) {
			m4u_put_bdt(info->bdt);
			info->bdt = NULL;
		}
#endif
	}

	return 0;
}

int msc2_get_sc2(struct msc2_mmu_dev **sc2_host, int id)
{
	struct msc2_mmu_dev *sc2_dev = NULL;

	list_for_each_entry(sc2_dev, &sc2_devices, list) {
		if (sc2_dev->id == id)
			goto found;
	}
	return -ENODEV;

found:
	*sc2_host = sc2_dev;
	sc2_dev->usr_cnt++;
	dev_info(sc2_dev->dev, "acquire sc2 DMA succeed\n");
	return 0;
}
EXPORT_SYMBOL(msc2_get_sc2);

void msc2_put_sc2(struct msc2_mmu_dev **sc2_dev)
{
	(*sc2_dev)->usr_cnt--;
}
EXPORT_SYMBOL(msc2_put_sc2);

static int msc2_device_register(struct msc2_mmu_dev *sc2_dev)
{
	struct msc2_mmu_dev *other;

	mutex_lock(&list_lock);
	list_for_each_entry(other, &sc2_devices, list) {
		if (other->id == sc2_dev->id) {
			dev_warn(sc2_dev->dev,
				"sc2 id %d already registered\n",
				sc2_dev->id);
			mutex_unlock(&list_lock);
			return -EBUSY;
		}
	}

	list_add_tail(&sc2_dev->list, &sc2_devices);
	mutex_unlock(&list_lock);
	return 0;
}

static int msc2_device_unregister(struct msc2_mmu_dev *sc2_dev)
{
	mutex_lock(&list_lock);
	list_del(&sc2_dev->list);
	mutex_unlock(&list_lock);
	return 0;
}

static struct msc2_mmu_ops mmu_ops = {
	.owner = THIS_MODULE,
	.rbypass = msc2_rbypass,
	.wbypass = msc2_wbypass,
	.bypass_status = msc2_bypass_status,
	.get_tid = msc2_get_tid,
	.acquire_ch = msc2_acquire_channel,
	.release_ch = msc2_release_channel,
	.enable_ch = msc2_chs_enable,
	.disable_ch = msc2_chs_disable,
	.reset_ch = msc2_chs_reset,
	.config_ch = msc2_ch_config,
	.alloc_desc = msc2_alloc_dma_desc,
	.free_desc = msc2_free_dma_desc,
	.setup_sglist = msc2_setup_sglist,
	.unmap_sglist = msc2_unmap_sglist,
	.map_dmabuf = msc2_map_dmabuf,
	.unmap_dmabuf = msc2_unmap_dmabuf,
};

static void msc2_irq_ch_handler(struct msc2_mmu_dev *sc2_dev, int ch)
{
	u32 irqs, i;
	static const char * const err_msg[] = {
		"EOF Error",
		"Descriptor Undersize Error",
		"AXI Interleave Error",
		"SVA Address Error",
		"Descriptor Size Error",
	};

	irqs = msc2_mmu_reg_read(sc2_dev, REG_SC2_IRQSTAT(ch));

	for (i = 0; i < CIRQ_ERR_HIGH; i++)
		if (irqs & (1 << i))
			dev_dbg(sc2_dev->dev, "MMU ch%d: %s\n", ch, err_msg[i]);

	msc2_mmu_reg_write(sc2_dev, REG_SC2_IRQSTAT(ch), irqs);
}

static irqreturn_t msc2_irq_handler(int irq, void *data)
{
	struct msc2_mmu_dev *sc2_dev = data;
	u32 irqs;
	int i;

	irqs = msc2_mmu_reg_read(sc2_dev, REG_SC2_GIRQ_STAT);

	if (irqs == 0)
		return IRQ_NONE;

	if (irqs & GSTAT_MUL_MATCH_STAT) {
		dev_err(sc2_dev->dev, "Multiple IDs match same channel!\n");
		msc2_mmu_reg_write(sc2_dev, REG_SC2_GIRQ_STAT,
				GSTAT_MUL_MATCH_STAT);
	}

	for (i = 0; i <= 15; i++) {
		if (irqs & (0x1 << i))
			msc2_irq_ch_handler(sc2_dev, i);
	}

	return IRQ_HANDLED;
}

static int msc2_mmu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct msc2_mmu_dev *sc2_dev;
	struct resource *res;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	int version;
	int irq;
	int ret;

	ret = of_alias_get_id(np, "mv_sc2_mmu");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		return ret;
	}
	pdev->id = ret;

	sc2_dev = devm_kzalloc(dev, sizeof(struct msc2_mmu_dev), GFP_KERNEL);
	if (!sc2_dev) {
		dev_err(dev, "allocate sc2_dev failed\n");
		return -ENOMEM;
	}

	sc2_dev->channel_map = SC2_CH_MASK; /* only 16 channels */
	sc2_dev->free_chs = SC2_CH_NUM;
	sc2_dev->pdev = pdev;
	sc2_dev->dev = dev;
	if (pdev->id != 0)
		return -ENODEV;	/* currently only one SC2 mmu is used */
	sc2_dev->id = pdev->id;
	sc2_dev->ops = &mmu_ops;
	spin_lock_init(&sc2_dev->msc2_lock);
	dev_set_drvdata(dev, sc2_dev);

	ret = of_property_read_u32(np, "version", &version);
	if (ret < 0) {
		dev_err(dev, "get version failed, use version 1\n");
		version = MSC2_MMU_VERSION_1;
	}
	sc2_dev->version = version;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(dev, "fail to request and remap iomem\n");
		return PTR_ERR(base);
	};
	sc2_dev->mmu_regs = base;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "fail to get irq resource\n");
		return -ENXIO;
	}
	sc2_dev->irq = irq;
	ret = devm_request_irq(dev, irq, msc2_irq_handler, 0,
					MV_SC2_MMU_NAME, sc2_dev);
	if (ret) {
		dev_err(dev, "fail to request irq\n");
		return ret;
	}

	ret = msc2_device_register(sc2_dev);
	if (ret)
		return ret;

	return 0;
}

static int msc2_mmu_remove(struct platform_device *pdev)
{
	struct msc2_mmu_dev *sc2_dev;

	sc2_dev = dev_get_drvdata(&pdev->dev);
	msc2_device_unregister(sc2_dev);
	return 0;
}

static const struct of_device_id mv_sc2_mmu_dt_match[] = {
	{ .compatible = "marvell,mmp-sc2mmu", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, mv_usbphy_dt_match);

static struct platform_driver msc2_mmu_driver = {
	.driver = {
		.name = MV_SC2_MMU_NAME,
		/* TBD */
		/* .pm = &msc2_mmu_pm, */
		.of_match_table = of_match_ptr(mv_sc2_mmu_dt_match),
	},
	.probe = msc2_mmu_probe,
	.remove = msc2_mmu_remove,
};

module_platform_driver(msc2_mmu_driver);
