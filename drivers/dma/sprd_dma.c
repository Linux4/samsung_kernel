/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/of_dma.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include "dmaengine.h"

#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sprd_dma.h>

//#define DMA_DEBUG
#ifdef DMA_DEBUG
#define pr_dma(fmt, ...)	printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_dma(fmt, ...)
#endif

#define MPC_DMA_DESCRIPTORS		(64)
#define DMA_CHN_OFFSET			(0x40)
#define DMA_MEMCPY_MIN_SIZE		(64)
#define DMA_CFG_COUNT			(MPC_DMA_DESCRIPTORS)
#define DMA_CHx_OFFSET			(0x40)
#define SPRD_DMA_REQ_CID(base,uid)		((unsigned long)base + 0x2000 + 0x4 * ((uid) -1))

/* dma channel register definition */
struct sprd_dma_chn_reg {
	u32 pause;
	u32 req;
	u32 cfg;
	u32 intc;
	u32 src_addr;
	u32 des_addr;
	u32 frg_len;
	u32 blk_len;
	/* only full chn have following regs */
	u32 trsc_len;
	u32 trsf_step;
	u32 wrap_ptr;
	u32 wrap_to;
	u32 llist_ptr;
	u32 frg_step;
	u32 src_blk_step;
	u32 des_blk_step;
};

/* dma request description */
struct sprd_dma_desc {
	struct dma_async_tx_descriptor	desc;
	struct sprd_dma_chn_reg			*dma_chn_reg;
	dma_addr_t						dma_chn_reg_paddr;
	struct list_head				node;
	struct list_head				next_node;
	int								done;
	int								cycle;
	int								mutiple_chan;
};

/* dma channel description */
struct sprd_dma_chn {
	struct dma_chan			chan;
	struct list_head		free;
	struct list_head		prepared;
	struct list_head		queued;
	struct list_head		active;
	struct list_head		completed;
	spinlock_t 				chn_lock;
	int						chan_num;
	u32 					dev_id;
	enum dma_chn_status 	chan_status;
	void __iomem 			*dma_chn_base;
	void __iomem			*dma_desc;
	dma_addr_t				dma_desc_paddr;
	enum dma_chn_type 		chn_type;
	enum request_mode 		re_mode;
	int						irq_handle_enable;
};

/* dma device */
struct sprd_dma_dev {
	struct dma_device		dma_dev;
	spinlock_t				dma_lock;
	void __iomem 			*dma_glb_base;
	void __iomem 			*aon_dma_glb_base;
	void __iomem 			*agcp_dma_glb_base;
	int						irq;
	int						aon_irq;
	int						agcp_irq;
	struct tasklet_struct 	tasklet;
	struct kmem_cache 		*dma_desc_node_cachep;
	u32						ap_chn_cnt;
	u32						aon_chn_cnt;
	u32						agcp_chn_cnt;
	u32						chn_std_cnt;
	struct sprd_dma_chn		channels[0];
};

struct dma_cfg_group_t {
	struct semaphore 		cfg_sema;
	int						dma_cfg_cnt;
	struct sprd_dma_cfg 	dma_cfg[DMA_CFG_COUNT];
};

struct dma_cfg_group_t dma_cfg_group;

static struct of_dma_filter_info sprd_dma_info = {
	.filter_fn = sprd_dma_filter_fn,
};

static inline struct sprd_dma_chn *to_sprd_dma_chan(struct dma_chan *c)
{
	return container_of(c, struct sprd_dma_chn, chan);
}

static inline struct sprd_dma_dev *to_sprd_dma_dev(struct dma_chan *c)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(c);
	return container_of(mchan, struct sprd_dma_dev, channels[c->chan_id]);
}

static inline struct sprd_dma_desc *to_sprd_dma_desc(struct dma_async_tx_descriptor *tx)
{
	return container_of(tx, struct sprd_dma_desc, desc);
}

/* dma debug function */
static int __dma_cfg_check_register(struct dma_chan *chan, void __iomem * dma_reg_addr)
{
	volatile struct sprd_dma_chn_reg *dma_reg = (struct sprd_dma_chn_reg *)dma_reg_addr;
	volatile struct sprd_dma_glb_reg *ap_dma_glb_reg;
	volatile struct sprd_dma_glb_reg *aon_dma_glb_reg;
	volatile struct sprd_dma_glb_reg *agcp_dma_glb_reg;
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(chan);
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);

	ap_dma_glb_reg = sdev->dma_glb_base;
	aon_dma_glb_reg = sdev->aon_dma_glb_base;
	agcp_dma_glb_reg = sdev->agcp_dma_glb_base;

	pr_dma("------------------------------------------------------------------------------------>>>\n");
	pr_dma("DMA register:\n pause=0x%x,\n req=0x%x,\n cfg=0x%x,\n int=0x%x,\n src_addr=0x%x,\n"
			"des_addr=0x%x,\n frg_len=0x%x,\n blk_len=0x%x,\n trsc_len=0x%x,\n trsf_step=0x%x,\n"
			"wrap_ptr=0x%x,\n wrap_to=0x%x,\n llist_ptr=0x%x,\n frg_step=0x%x,\n src_blk_step=0x%x,\n"
			"des_blk_step=0x%x,\n",dma_reg->pause,dma_reg->req,dma_reg->cfg,dma_reg->intc,
			dma_reg->src_addr,dma_reg->des_addr,dma_reg->frg_len,dma_reg->blk_len,dma_reg->trsc_len,
			dma_reg->trsf_step,dma_reg->wrap_ptr,dma_reg->wrap_to,dma_reg->llist_ptr,dma_reg->frg_step,
			dma_reg->src_blk_step,dma_reg->des_blk_step);
	if ((ap_dma_glb_reg != NULL) && (mchan->chan_num < sdev->ap_chn_cnt))
		pr_dma("AP global debug reg:debug sts=0x%x,pause=0x%x,req_sts=0x%x,en_sts=0x%x,arb_sel_sts=0x%x!\n",
				ap_dma_glb_reg->debug_sts, ap_dma_glb_reg->pause, ap_dma_glb_reg->req_sts,
				ap_dma_glb_reg->en_sts, ap_dma_glb_reg->arb_sel_sts);
	else if ((aon_dma_glb_reg != NULL) && (mchan->chan_num < sdev->ap_chn_cnt + sdev->aon_chn_cnt))
		pr_dma("AON global debug reg:debug sts=0x%x,pause=0x%x,req_sts=0x%x,en_sts=0x%x,arb_sel_sts=0x%x!\n",
				aon_dma_glb_reg->debug_sts, aon_dma_glb_reg->pause, aon_dma_glb_reg->req_sts,
				aon_dma_glb_reg->en_sts, aon_dma_glb_reg->arb_sel_sts);
	else if (agcp_dma_glb_reg != NULL)
		pr_dma("AGCP global debug reg:debug sts=0x%x,pause=0x%x,req_sts=0x%x,en_sts=0x%x,arb_sel_sts=0x%x!\n",
				agcp_dma_glb_reg->debug_sts, agcp_dma_glb_reg->pause, agcp_dma_glb_reg->req_sts,
				agcp_dma_glb_reg->en_sts, agcp_dma_glb_reg->arb_sel_sts);
	pr_dma("<<<------------------------------------------------------------------------------------\n");
	return 0;
}

static void __inline __dma_enable(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	u32 ap_chn_cnt = mdev->ap_chn_cnt;
	u32 aon_chn_cnt = mdev->aon_chn_cnt;

	if ((!mchan) || (!mdev))
		return ;

#if defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2)
	if (mchan->chan_num < ap_chn_cnt) {
		if (!sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_AP_AHB_DMA_EB))
			sci_glb_set((unsigned long)REG_AP_AHB_AHB_EB, BIT_AP_AHB_DMA_EB);
	} else if (mchan->chan_num < ap_chn_cnt + aon_chn_cnt) {
		if (!sci_glb_read((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB))
			sci_glb_set((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB);
	} else {
		if (!sci_glb_read((unsigned long)REGS_AGCP_AHB_BASE, BIT_DMA_AP_EB | BIT_DMA_AP_ASHB_EB))
			sci_glb_set((unsigned long)REGS_AGCP_AHB_BASE, BIT_DMA_AP_EB | BIT_DMA_AP_ASHB_EB);
	}
#else
	if (mchan->chan_num < ap_chn_cnt) {
		if (!sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB))
			sci_glb_set((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	} else if (mchan->chan_num < ap_chn_cnt + aon_chn_cnt) {
		if (!sci_glb_read((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB))
			sci_glb_set((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB);
	}
#endif
}

static void __inline __dma_disable(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	struct sprd_dma_chn *tchan;
	u32 ap_chn_cnt = mdev->ap_chn_cnt;
	u32 aon_chn_cnt = mdev->aon_chn_cnt;
	u32 agcp_chn_cnt = mdev->agcp_chn_cnt;
	int i = 0;

	if ((!mchan) || (!mdev))
		return ;

	for (i = 0; i < ap_chn_cnt + aon_chn_cnt + agcp_chn_cnt; i++) {
		tchan = &mdev->channels[i];
		if(tchan->chan_status == USED)
			break;
	}

#if defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2)
	if ((mchan->chan_num < ap_chn_cnt) && (i >= ap_chn_cnt)) {
		if (sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_AP_AHB_DMA_EB))
			sci_glb_clr((unsigned long)REG_AP_AHB_AHB_EB, BIT_AP_AHB_DMA_EB);
	} else if ((mchan->chan_num < ap_chn_cnt + aon_chn_cnt) &&
			(i >= ap_chn_cnt + aon_chn_cnt)){
		if (sci_glb_read((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB))
			sci_glb_clr((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB);
	} else  if (i == ap_chn_cnt + aon_chn_cnt + agcp_chn_cnt) {
		if (sci_glb_read((unsigned long)REGS_AGCP_AHB_BASE, BIT_DMA_AP_EB | BIT_DMA_AP_ASHB_EB))
			sci_glb_clr((unsigned long)REGS_AGCP_AHB_BASE, BIT_DMA_AP_EB | BIT_DMA_AP_ASHB_EB);
	}
#else
	if ((mchan->chan_num < ap_chn_cnt) && (i >= ap_chn_cnt)) {
		if (sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB))
			sci_glb_clr((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	} else if ((mchan->chan_num < ap_chn_cnt + aon_chn_cnt) &&
			(i >= ap_chn_cnt + aon_chn_cnt)){
		if (sci_glb_read((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB))
			sci_glb_clr((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB);
	}
#endif

}

static void __inline __dma_softreset(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	struct sprd_dma_chn *tchan;
	u32 ap_chn_cnt = mdev->ap_chn_cnt;
	u32 aon_chn_cnt = mdev->aon_chn_cnt;
	u32 agcp_chn_cnt = mdev->agcp_chn_cnt;
	int i = 0;

	if ((!mchan) || (!mdev))
		return ;

	for (i = 0; i < ap_chn_cnt + aon_chn_cnt + agcp_chn_cnt; i++) {
		tchan = &mdev->channels[i];
		if(tchan->chan_status == USED)
			break;
	}

#if defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2)
	if ((mchan->chan_num < ap_chn_cnt) && (i >= ap_chn_cnt)) {
		sci_glb_set(REG_AP_AHB_AHB_RST, BIT_AP_AHB_DMA_SOFT_RST);
		udelay(1);
		sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_AP_AHB_DMA_SOFT_RST);
	} else if ((mchan->chan_num < ap_chn_cnt + aon_chn_cnt) &&
			(i >= ap_chn_cnt + aon_chn_cnt)) {
		sci_glb_set(REG_AON_APB_APB_RST1, BIT_AON_APB_AON_DMA_SOFT_RST);
		udelay(1);
		sci_glb_clr(REG_AON_APB_APB_RST1, BIT_AON_APB_AON_DMA_SOFT_RST);
	} else if (i == ap_chn_cnt + aon_chn_cnt + agcp_chn_cnt) {
		sci_glb_set(REG_AGCP_AHB_MODULE_RST0_STS, BIT_DMA_AP_SOFT_RST);
		udelay(1);
		sci_glb_clr(REG_AGCP_AHB_MODULE_RST0_STS, BIT_DMA_AP_SOFT_RST);
	}
#else
	if ((mchan->chan_num < ap_chn_cnt) && (i >= ap_chn_cnt)) {
		sci_glb_set(REG_AP_AHB_AHB_RST, BIT_DMA_SOFT_RST);
		udelay(1);
		sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_DMA_SOFT_RST);
	} else if ((mchan->chan_num < ap_chn_cnt + aon_chn_cnt) &&
			(i >= ap_chn_cnt + aon_chn_cnt)) {
		sci_glb_set(REG_AON_APB_APB_RST1, BIT_AON_DMA_SOFT_RST);
		udelay(1);
		sci_glb_clr(REG_AON_APB_APB_RST1, BIT_AON_DMA_SOFT_RST);
	}
#endif
}

static void __inline __dma_int_enable(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	u32 ap_chn_cnt = mdev->ap_chn_cnt;
	u32 aon_chn_cnt = mdev->aon_chn_cnt;

	if ((!mchan) || (!mdev))
		return ;

	if (mchan->chan_num < ap_chn_cnt) {
		//do nothing
	} else if (mchan->chan_num < ap_chn_cnt + aon_chn_cnt) {
#if !(defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2))
		if (!sci_glb_read((unsigned long)(SPRD_INTC2_BASE+0x8), BIT(6)))
			sci_glb_set((unsigned long)(SPRD_INTC2_BASE+0x8), BIT(6));
		if (!sci_glb_read((unsigned long)REG_AON_APB_AON_DMA_INT_EN, BIT_AON_DMA_INT_AP_EN))
			sci_glb_set((unsigned long)REG_AON_APB_AON_DMA_INT_EN, BIT_AON_DMA_INT_AP_EN);
#endif
	} else {
		//do nothing
	}
}

static void __inline __dma_int_disable(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	struct sprd_dma_chn *tchan;
	u32 ap_chn_cnt = mdev->ap_chn_cnt;
	u32 aon_chn_cnt = mdev->aon_chn_cnt;
	u32 agcp_chn_cnt = mdev->agcp_chn_cnt;
	int i = 0;

	if ((!mchan) || (!mdev))
		return ;

	for (i = 0; i < ap_chn_cnt + aon_chn_cnt + agcp_chn_cnt; i++) {
		tchan = &mdev->channels[i];
		if(tchan->chan_status == USED)
			break;
	}

	if ((mchan->chan_num < ap_chn_cnt) && (i >= ap_chn_cnt)) {
		//do nothing
	} else if ((mchan->chan_num < ap_chn_cnt + aon_chn_cnt) && (i >= ap_chn_cnt + aon_chn_cnt)) {
#if !(defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2))
		if (sci_glb_read((unsigned long)REG_AON_APB_AON_DMA_INT_EN, BIT_AON_DMA_INT_AP_EN))
			sci_glb_clr((unsigned long)REG_AON_APB_AON_DMA_INT_EN, BIT_AON_DMA_INT_AP_EN);
#endif
	} else if (i == ap_chn_cnt + aon_chn_cnt + agcp_chn_cnt) {
		//do nothing
	}
}

static void __inline __dma_set_uid(struct sprd_dma_dev *sdev,
						struct sprd_dma_chn *mchan, u32 dev_id)
{
	u32 ap_chn_cnt = sdev->ap_chn_cnt;
	u32 aon_chn_cnt = sdev->aon_chn_cnt;

	if (DMA_UID_SOFTWARE != dev_id) {
		if (mchan->chan_num < ap_chn_cnt)
			writel_relaxed((mchan->chan_num + 1),
						(void __iomem *)SPRD_DMA_REQ_CID(sdev->dma_glb_base, dev_id));
		else if (mchan->chan_num < (ap_chn_cnt + aon_chn_cnt))
			writel_relaxed((mchan->chan_num - ap_chn_cnt + 1),
						(void __iomem *)SPRD_DMA_REQ_CID(sdev->aon_dma_glb_base, dev_id));
		else
			writel_relaxed((mchan->chan_num - ap_chn_cnt - aon_chn_cnt + 1),
						(void __iomem *)SPRD_DMA_REQ_CID(sdev->agcp_dma_glb_base, dev_id));
	}
}

static void __inline __dma_unset_uid(struct sprd_dma_dev *sdev,
						struct sprd_dma_chn *mchan, u32 dev_id)
{
	u32 ap_chn_cnt = sdev->ap_chn_cnt;
	u32 aon_chn_cnt = sdev->aon_chn_cnt;

	if (DMA_UID_SOFTWARE != dev_id) {
		if (mchan->chan_num < ap_chn_cnt)
			writel_relaxed(0x0,(void __iomem *)SPRD_DMA_REQ_CID(sdev->dma_glb_base, dev_id));
		else if (mchan->chan_num < (ap_chn_cnt + aon_chn_cnt))
			writel_relaxed(0x0,(void __iomem *)SPRD_DMA_REQ_CID(sdev->aon_dma_glb_base, dev_id));
		else
			writel_relaxed(0x0,(void __iomem *)SPRD_DMA_REQ_CID(sdev->agcp_dma_glb_base, dev_id));
	}
}

static void __inline __dma_int_clr(struct sprd_dma_chn *mchan)
{
	volatile struct sprd_dma_chn_reg *dma_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;

	dma_reg->intc |= 0x1f << 24;
}

static void __inline __dma_int_dis(struct sprd_dma_chn *mchan)
{
	volatile struct sprd_dma_chn_reg *dma_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;

	dma_reg->intc |= 0x1f << 24;
	dma_reg->intc &= ~0x1f;
}

static void __inline __dma_chn_enable(struct sprd_dma_chn *mchan)
{
	volatile struct sprd_dma_chn_reg *dma_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;

	dma_reg->cfg |= 0x1;
}

static void __inline __dma_soft_request(struct sprd_dma_chn *mchan)
{
	volatile struct sprd_dma_chn_reg *dma_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;

	dma_reg->req |= 0x1;
}

static void __dma_stop_and_disable(struct sprd_dma_chn *mchan)
{
	u32 timeout = 0x2000;

	volatile struct sprd_dma_chn_reg *dma_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(&mchan->chan);
	u32 ap_chn_cnt = sdev->ap_chn_cnt;

	if (!(dma_reg->cfg & 0x1))
		return;

	dma_reg->pause |= 0x1;

	/* fixme, need to deal with timeout */
	if (mchan->chan_num >= ap_chn_cnt){
		while (!(dma_reg->pause & AON_DMA_CHN_PAUSE)){
			if (--timeout == 0) {
				__dma_softreset(&mchan->chan);
				break;
			}
			cpu_relax();
		}
	} else {
		while (!(dma_reg->pause & DMA_CHN_PAUSE)){
			if (--timeout == 0) {
				__dma_softreset(&mchan->chan);
				break;
			}
			cpu_relax();
		}
	}

	dma_reg->cfg &= ~0x1;
	dma_reg->pause = 0x0;
}

/* get dma source reg addr */
static unsigned long __dma_get_src_addr(struct dma_chan *dma_chn)
{
    struct sprd_dma_chn *mchan = to_sprd_dma_chan(dma_chn);
    unsigned long addr = (unsigned long)mchan->dma_chn_base + 0x10;
	unsigned long addr_val;

	addr_val = readl_relaxed((void __iomem *)addr);
	addr_val |= (readl_relaxed((void __iomem *)(addr + 0x28))
		& 0xF0000000) << 4;
	return addr_val;
}

/* get dma dest reg addr */
static unsigned long __dma_get_dst_addr(struct dma_chan *dma_chn)
{
    struct sprd_dma_chn *mchan = to_sprd_dma_chan(dma_chn);
	unsigned long addr = (unsigned long)mchan->dma_chn_base + 0x14;
	unsigned long addr_val;

	addr_val = readl_relaxed((void __iomem *)addr);
	addr_val |= (readl_relaxed((void __iomem *)(addr + 0x2C))
		& 0xF0000000) << 4;
	return addr_val;
}

static int __dma_config(struct dma_chan *chan,struct sprd_dma_desc *mdesc,
		struct sprd_dma_cfg *cfg_list,struct sprd_dma_chn_reg *dma_reg_addr,
		enum config_type type)
{
	volatile struct sprd_dma_chn_reg *dma_reg;
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_cfg *dma_cfg_tmp = cfg_list;
	u32 fix_mode = 0, llist_en = 0, wrap_en = 0;
	u32 list_end = 0, fix_en = 0,irq_mode = 0,wrap_mode = 0;
	int chn_type;

	/* check dma fix mode */
	if (dma_cfg_tmp->src_step != 0 && dma_cfg_tmp->des_step != 0) {
		fix_en = 0x0;
	} else {
		if ((dma_cfg_tmp->src_step | dma_cfg_tmp->des_step) == 0) {
			fix_en = 0x0;
		} else {
			fix_en = 0x1;
			if (dma_cfg_tmp->src_step)
				fix_mode = 0x1;
			else
				fix_mode = 0x0;
		}
	}

	/* check dma wrap mode */
	if (dma_cfg_tmp->wrap_ptr && dma_cfg_tmp->wrap_to) {
		wrap_en = 0x1;
		if (dma_cfg_tmp->wrap_to == dma_cfg_tmp->src_addr) {
			wrap_mode = 0x0;
		} else {
			if (dma_cfg_tmp->wrap_to == dma_cfg_tmp->des_addr)
				wrap_mode = 0x1;
			else
				return -EINVAL;
		}
	}

	/* linklist configuration */
	if (dma_cfg_tmp->linklist_ptr) {
		llist_en = 0x1;
		if (dma_cfg_tmp->is_end == 1)
			list_end = 0x1;
		else
			list_end = 0;
	}

	chn_type = mchan->chn_type;
	irq_mode = dma_cfg_tmp->irq_mode;

	/* check dma mode */
	if ((chn_type == STANDARD_DMA) &&
		(irq_mode == TRANS_DONE || irq_mode == LIST_DONE)) {
		pr_err("Irq type isn't compatible with channel type!");
		return -EINVAL;
	}

	if (!IS_ALIGNED(dma_cfg_tmp->src_step, dma_cfg_tmp->datawidth)){
		pr_err("Source step is not aligned!");
		return -EINVAL;
	}

	if (!IS_ALIGNED(dma_cfg_tmp->des_step, dma_cfg_tmp->datawidth)){
		pr_err("Destination step is not aligned!");
		return -EINVAL;
	}

	if (!mchan->dev_id)
		mchan->dev_id = dma_cfg_tmp->dev_id;

	if (type == CONFIG_DESC)
		dma_reg = mdesc->dma_chn_reg;
	else if (type == CONFIG_LINKLIST)
		dma_reg = dma_reg_addr;
	else
		return -EINVAL;

	dma_reg->pause = 0x0;
	dma_reg->req = 0x0;
	if (wrap_en) {
#if defined(CONFIG_ARCH_WHALE)
		dma_reg->wrap_ptr = (dma_cfg_tmp->wrap_ptr - 1) & WRAP_DATA_MASK;
#else
		dma_reg->wrap_ptr = dma_cfg_tmp->wrap_ptr  & WRAP_DATA_MASK;
#endif
		dma_reg->wrap_to = dma_cfg_tmp->wrap_to  & WRAP_DATA_MASK;
	}

	/* set default priority = 1 */
	dma_reg->cfg = DMA_PRI_1 << CHN_PRIORITY_OFFSET |
	    llist_en << LLIST_EN_OFFSET |
	    DMA_DONOT_WAIT_BDONE << CHN_WAIT_BDONE;

	/* src and des addr */
	dma_reg->wrap_ptr |= (u32)((dma_cfg_tmp->src_addr >> 4) & SRC_DEST_HADDR_MASK);
	dma_reg->wrap_to |= (u32)((dma_cfg_tmp->des_addr >> 4) & SRC_DEST_HADDR_MASK);

	dma_reg->src_addr = (u32)dma_cfg_tmp->src_addr;
	dma_reg->des_addr = (u32)dma_cfg_tmp->des_addr;

	/* frag len */
	dma_reg->frg_len =
	    (dma_cfg_tmp->datawidth << SRC_DATAWIDTH_OFFSET) |
	    (dma_cfg_tmp->datawidth << DES_DATAWIDTH_OFFSET) |
	    (0x0 << SWT_MODE_OFFSET) |
	    (dma_cfg_tmp->req_mode << REQ_MODE_OFFSET) |
	    (wrap_mode << ADDR_WRAP_SEL_OFFSET) |
	    (wrap_en << ADDR_WRAP_EN_OFFSET) |
	    (fix_mode << ADDR_FIX_SEL_OFFSET) |
	    (fix_en << ADDR_FIX_SEL_EN) |
	    (list_end << LLIST_END_OFFSET) |
		(dma_cfg_tmp->fragmens_len & FRG_LEN_MASK);

	/* blk len */
	dma_reg->blk_len = dma_cfg_tmp->block_len & BLK_LEN_MASK;

	/* set interrupt type*/
	if (type == CONFIG_DESC){
		if (irq_mode == NO_INT)
			mchan->irq_handle_enable = 0;
		else
			mchan->irq_handle_enable = 1;

		dma_reg->intc &= ~0x1f;
		dma_reg->intc |= 0x1 << 4;
		switch (irq_mode) {
		case NO_INT:
			break;
		case FRAG_DONE:
			dma_reg->intc |= 0x1;
			break;
		case BLK_DONE:
			dma_reg->intc |= 0x2;
			break;
		case TRANS_DONE:
			dma_reg->intc |= 0x4;
			break;
		case LIST_DONE:
			dma_reg->intc |= 0x8;
			break;
		case CONFIG_ERR:
			dma_reg->intc |= 0x10;
			break;
		default:
			return -EINVAL;
		}
	}
	else
		dma_reg->intc = 0;

	pr_dma("dma_config:cfg=0x%x,frg_len=0x%x,blk_len=0x%x,intc=0x%x!\n",
			dma_reg->cfg,dma_reg->frg_len,dma_reg->blk_len,dma_reg->intc);

	if (chn_type == STANDARD_DMA)
		return 0;

	/* full dma config */
	if (0x0 == dma_cfg_tmp->transcation_len)
		dma_reg->trsc_len = dma_cfg_tmp->block_len & TRSC_LEN_MASK;
	else
		dma_reg->trsc_len = dma_cfg_tmp->transcation_len & TRSC_LEN_MASK;

	dma_reg->trsf_step =
	    (dma_cfg_tmp->des_step & TRSF_STEP_MASK) << DEST_TRSF_STEP_OFFSET |
	    (dma_cfg_tmp->src_step & TRSF_STEP_MASK) << SRC_TRSF_STEP_OFFSET;


	dma_reg->frg_step =
	    (dma_cfg_tmp->dst_frag_step & FRAG_STEP_MASK) << DEST_FRAG_STEP_OFFSET |
	    (dma_cfg_tmp->src_frag_step & FRAG_STEP_MASK) << SRC_FRAG_STEP_OFFSET;

	dma_reg->src_blk_step = dma_cfg_tmp->src_blk_step;
	dma_reg->des_blk_step = dma_cfg_tmp->dst_blk_step;

	dma_reg->src_blk_step |= (u32)((dma_cfg_tmp->linklist_ptr >> 4) & 0xF0000000);
	dma_reg->llist_ptr = (u32)dma_cfg_tmp->linklist_ptr;

	pr_dma("dma_config:trsc_len=0x%x,trsf_step=0x%x,llist_ptr=0x%x,frg_step=0x%x!\n",
			dma_reg->trsc_len,dma_reg->trsf_step,dma_reg->llist_ptr,dma_reg->frg_step);

	return 0;
}

/* config dma linklist */
static int __dma_config_linklist(struct dma_chan *chan,struct sprd_dma_desc *mdesc,
								struct sprd_dma_cfg *cfg_list,u32 node_size)
{
	int ret, i;
	struct sprd_dma_chn_reg *dma_reg_list;
	struct sprd_dma_cfg list_cfg;
	dma_addr_t cfg_p;

	if(node_size < 2)
		return -EINVAL;

	/* check dma linklist node memory */
	if (cfg_list[0].link_cfg_v == 0 || cfg_list[0].link_cfg_p == 0){
		pr_err("Haven't allocated memory for list node!\n");
		return -EINVAL;
	}

	/* get linklist node virtual addr and physical addr */
	dma_reg_list = (struct sprd_dma_chn_reg *)cfg_list[0].link_cfg_v;
	cfg_p = (dma_addr_t)cfg_list[0].link_cfg_p;

	pr_dma("Linklist:alloc addr virt:0x%lx,phys addr: 0x%lx\n",
			(unsigned long)dma_reg_list,(unsigned long)cfg_p);

	/* linklist configuration */
	for (i = 0; i < node_size; i++) {
		cfg_list[i].linklist_ptr = (cfg_p +
						((i + 1) % node_size) * sizeof(struct sprd_dma_chn_reg) + 0x10);

		ret = __dma_config(chan,NULL,cfg_list + i,dma_reg_list + i,CONFIG_LINKLIST);
		if (ret < 0) {
			pr_err("Linklist configuration error!\n");
			return -EINVAL;
		}
		pr_dma("Configuration the link list!\n");
		__dma_cfg_check_register(chan, dma_reg_list + i);
	}

	memset((void *)&list_cfg, 0x0, sizeof(list_cfg));
	list_cfg.linklist_ptr = cfg_p + 0x10;
	list_cfg.irq_mode = cfg_list[0].irq_mode;
	list_cfg.src_addr = cfg_list[0].src_addr;
	list_cfg.des_addr = cfg_list[0].des_addr;

	/* support for audio */
	if (cfg_list[node_size - 1].is_end > 1)
		mdesc->cycle = 1;

	ret = __dma_config(chan,mdesc,&list_cfg,NULL,CONFIG_DESC);

	return ret;
}

static int __dma_chn_start_chn_dis(volatile struct sprd_dma_glb_reg *dma_glb_reg,
				enum group_idx group)
{
	switch (group) {
	case DMA_CHN_GROUP_1:
		dma_glb_reg->cfg_group1 = 0x0;
		break;
	case DMA_CHN_GROUP_2:
		dma_glb_reg->cfg_group2 = 0x0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int __dma_check_pwr_state(void)
{
	int dma_pwr_st= 0;
#if defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2)
	if (sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_AP_AHB_DMA_EB))
		dma_pwr_st |= AP_DMA_ENABLE;
#else
	if (sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB))
		dma_pwr_st |= AP_DMA_ENABLE;
#endif
	if (sci_glb_read((unsigned long)REG_AON_APB_APB_EB1, BIT_AON_DMA_EB))
		dma_pwr_st |= AON_DMA_ENABLE;

#if defined(CONFIG_ARCH_WHALE) || defined(CONFIG_ARCH_WHALE2)
	if (sci_glb_read((unsigned long)REGS_AGCP_AHB_BASE, BIT_DMA_AP_EB | BIT_DMA_AP_ASHB_EB))
		dma_pwr_st |= AGCP_DMA_ENABLE;
#endif
	return dma_pwr_st;
}

static dma_int_type __dma_check_int_type(u32 intc_reg)
{
	if (intc_reg & 0x1000)
		return CONFIG_ERR;
	else if (intc_reg & 0x800)
		return LIST_DONE;
	else if (intc_reg & 0x400)
		return TRANS_DONE;
	else if (intc_reg & 0x200)
		return BLK_DONE;
	else if (intc_reg & 0x100)
		return FRAG_DONE;
	else
		return NO_INT;
}

static dma_request_mode __dma_check_req_type(u32 frag_reg)
{
	u32 frag_reg_t = frag_reg >> 24;
	if ((frag_reg_t & 0x3) == 0)
		return	FRAG_REQ_MODE;
	else if ((frag_reg_t & 0x3) == 0x1)
		return	BLOCK_REQ_MODE;
	else if ((frag_reg_t & 0x3) == 0x2)
		return	TRANS_REQ_MODE;
	else if ((frag_reg_t & 0x3) == 0x3)
		return	LIST_REQ_MODE;
	else
		return FRAG_REQ_MODE;
}

/* check if the dma request desc is done */
static void __dma_check_mdesc_done(struct sprd_dma_desc *mdesc,
				dma_int_type int_type,dma_request_mode req_mode)
{
	if(mdesc->cycle == 1){
		mdesc->done = 0;
		return;
	}

	if((unsigned int)int_type >= ((unsigned int)req_mode + 1))
		mdesc->done = 1;
	else
		mdesc->done = 0;
}

static void __dma_check_int(struct sprd_dma_dev *sdev, int type)
{
	struct sprd_dma_chn *mchan = NULL;
	struct sprd_dma_chn_reg *dma_reg = NULL;
	struct sprd_dma_desc *mdesc = NULL;
	struct dma_async_tx_descriptor *desc = NULL;
	u32 irq_status = 0, aon_irq_status = 0, agcp_irq_status = 0;
	u32 i = 0;
	int dma_pwr_st = 0;
	dma_int_type int_type;
	dma_request_mode req_type;
	volatile struct sprd_dma_glb_reg *dma_glb_reg = sdev->dma_glb_base;
	volatile struct sprd_dma_glb_reg *aon_dma_glb_reg = NULL;
	volatile struct sprd_dma_glb_reg *agcp_dma_glb_reg = NULL;

	if (sdev->aon_chn_cnt > 0)
		 aon_dma_glb_reg = sdev->aon_dma_glb_base;
	if (sdev->agcp_chn_cnt > 0)
		 agcp_dma_glb_reg = sdev->agcp_dma_glb_base;

	dma_pwr_st = __dma_check_pwr_state();
	if (type == 1) {
		if (dma_pwr_st & AP_DMA_ENABLE)
			irq_status = dma_glb_reg->int_msk_sts;
		if (dma_pwr_st & AON_DMA_ENABLE)
			aon_irq_status = (aon_dma_glb_reg != NULL) ? (aon_dma_glb_reg->int_msk_sts) : 0;
		if (dma_pwr_st & AGCP_DMA_ENABLE)
			agcp_irq_status = (agcp_dma_glb_reg != NULL) ? (agcp_dma_glb_reg->int_msk_sts) : 0;
		pr_dma("Enter DMA interrupt handle function!\n");
	}

	pr_dma("Check DMA interrupt, irq_status=0x%x,\
			aon_irq_status=0x%x!, agcp_irq_status=0x%x!\n",
			irq_status, aon_irq_status, agcp_irq_status);

	while (irq_status || aon_irq_status || agcp_irq_status) {
		if (irq_status != 0) {
			i = __ffs(irq_status);
			irq_status &= (irq_status - 1);
		} else if (aon_irq_status != 0) {
			i = __ffs(aon_irq_status);
			aon_irq_status &= (aon_irq_status - 1);
			i += sdev->ap_chn_cnt;
		} else if (agcp_irq_status != 0) {
			i = __ffs(agcp_irq_status);
			agcp_irq_status &= (agcp_irq_status - 1);
			i = i + (sdev->ap_chn_cnt + sdev->aon_chn_cnt);
		}else
			break;

		mchan = &sdev->channels[i];
		spin_lock(&mchan->chn_lock);
		dma_reg = (struct sprd_dma_chn_reg *)(mchan->dma_chn_base);
		int_type = __dma_check_int_type(dma_reg->intc);
		req_type = __dma_check_req_type(dma_reg->frg_len);
		pr_dma("DMA channel [%d] interrupt,intc=0x%x,int_type=%d,"
			   "req_type=%d!\n",i,dma_reg->intc,int_type,(req_type + 1));

		/* check if the dma request desc is done */
		dma_reg->intc |= 0x1f << 24;
		if (!list_empty(&mchan->active)){
			mdesc = list_first_entry(&mchan->active,struct sprd_dma_desc, node);
			__dma_check_mdesc_done(mdesc,int_type,req_type);
			if (mdesc->done == 1)
				list_splice_tail_init(&mchan->active, &mchan->completed);

			/* support for audio.
			 * this callback check for cycle mode, and there only one scenec:
			 * interrupt and cycle mode, int mask is high. The complete callback
			 * will be called in complete function.
			 */
			if (mdesc->cycle == 1) {
				desc = &mdesc->desc;
				if (desc->callback)
					desc->callback(desc->callback_param);
				spin_unlock(&mchan->chn_lock);
				return;
			}
		}

		if (!list_empty(&mchan->queued)) {
			mdesc = list_first_entry(&mchan->queued,struct sprd_dma_desc, node);
			__dma_check_mdesc_done(mdesc,int_type,req_type);
			if (mdesc->done == 1)
				list_splice_tail_init(&mchan->queued, &mchan->completed);

			/* support for audio.
			 * the mutilpe chn int mask is high, the mutiple chn callback.
			 */
			if ((mdesc->cycle == 1) &&
				(mdesc->mutiple_chan != 0xFF)) {
				desc = &mdesc->desc;
				if (desc->callback)
					desc->callback(desc->callback_param);
			}
		}
		spin_unlock(&mchan->chn_lock);
	}
}

static int sprd_dma_start(struct sprd_dma_dev *sdev, struct sprd_dma_chn *mchan,
							struct sprd_dma_desc *mdesc, u32 dev_id)
{
	__dma_set_uid(sdev,mchan,dev_id);
	__dma_chn_enable(mchan);

	if(DMA_UID_SOFTWARE == dev_id)
		__dma_soft_request(mchan);

	/* for debug info */
	if (dev_id > DMA_UID_SOFTWARE) {
		if ((sdev->aon_dma_glb_base) && (mchan->chan_num < sdev->ap_chn_cnt))
			pr_dma("sprd_dma_start,dev_id=%d,ap_req_cid=%d,!\n", dev_id,
				readl_relaxed((void __iomem *)SPRD_DMA_REQ_CID(sdev->dma_glb_base, dev_id)));
		else if ((sdev->aon_dma_glb_base) && (mchan->chan_num < (sdev->ap_chn_cnt + sdev->aon_chn_cnt)))
			pr_dma("sprd_dma_start,dev_id=%d,aon_req_cid=%d!\n", dev_id,
				readl_relaxed((void __iomem *)SPRD_DMA_REQ_CID(sdev->aon_dma_glb_base, dev_id)));
		else if ((sdev->agcp_dma_glb_base))
			pr_dma("sprd_dma_start,dev_id=%d,agcp_req_cid=%d!\n", dev_id,
				readl_relaxed((void __iomem *)SPRD_DMA_REQ_CID(sdev->agcp_dma_glb_base, dev_id)));
	}

	return 0;
}

static int sprd_dma_stop(struct sprd_dma_chn *mchan)
{
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(&mchan->chan);

	__dma_unset_uid(sdev, mchan, mchan->dev_id);
	__dma_stop_and_disable(mchan);
	__dma_int_clr(mchan);

	return 0;
}

static int sprd_dma_execute(struct sprd_dma_chn *mchan)
{
	struct sprd_dma_desc *first = NULL;
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(&mchan->chan);
	struct sprd_dma_chn_reg *dma_chn_reg = NULL;
	struct sprd_dma_chn_reg *first_reg = NULL;

	if (!list_empty(&mchan->active))
		first = list_first_entry(&mchan->active,struct sprd_dma_desc, node);
	else
		return 0;

	pr_dma("Before copy data to DMA reg!\n");
	__dma_cfg_check_register(&mchan->chan, first->dma_chn_reg);

	//memcpy((void __iomem *)mchan->dma_chn_base, (void *)first->dma_chn_reg,
	//			sizeof(struct sprd_dma_chn_reg));
	dma_chn_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;
	first_reg = (struct sprd_dma_chn_reg *)first->dma_chn_reg;
	dma_chn_reg->pause = first_reg->pause;
	dma_chn_reg->req = first_reg->req;
	dma_chn_reg->cfg = first_reg->cfg;
	dma_chn_reg->intc = first_reg->intc;
	dma_chn_reg->src_addr = first_reg->src_addr;
	dma_chn_reg->des_addr = first_reg->des_addr;
	dma_chn_reg->frg_len = first_reg->frg_len;
	dma_chn_reg->blk_len = first_reg->blk_len;
	dma_chn_reg->trsc_len = first_reg->trsc_len;
	dma_chn_reg->trsf_step = first_reg->trsf_step;
	dma_chn_reg->wrap_ptr = first_reg->wrap_ptr;
	dma_chn_reg->wrap_to = first_reg->wrap_to;
	dma_chn_reg->llist_ptr = first_reg->llist_ptr;
	dma_chn_reg->frg_step = first_reg->frg_step;
	dma_chn_reg->src_blk_step = first_reg->src_blk_step;
	dma_chn_reg->des_blk_step = first_reg->des_blk_step;

	pr_dma("After copy data to DMA reg!\n");
	__dma_cfg_check_register(&mchan->chan, mchan->dma_chn_base);

	mb();
	sprd_dma_start(sdev, mchan, first, mchan->dev_id);

	pr_dma("After start the DMA!\n");
	__dma_cfg_check_register(&mchan->chan, mchan->dma_chn_base);

	return 0;
}

static dma_cookie_t sprd_desc_submit(struct dma_async_tx_descriptor *tx)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(tx->chan);
	struct sprd_dma_desc *mdesc = to_sprd_dma_desc(tx);
	struct sprd_dma_desc *first = NULL;
	unsigned long flags;
	dma_cookie_t cookie;

	/* assign cookie */
	spin_lock_irqsave(&mchan->chn_lock, flags);
	cookie = dma_cookie_assign(tx);
	list_move_tail(&mdesc->node, &mchan->queued);
	if (!list_empty(&mdesc->next_node)){
		list_splice_tail_init(&mdesc->next_node, &mchan->queued);
		pr_dma("Submitting has next node!\n");
	}

	/* execute the dma desc !
	 * only execute normal chn, mutiple chn will not be executed.
	 */
	if (mdesc->mutiple_chan == 0xFF) {
		if (list_empty(&mchan->active)) {
			first = list_first_entry(&mchan->queued,struct sprd_dma_desc, node);
			list_move_tail(&first->node, &mchan->active);
			sprd_dma_execute(mchan);
		}
	}
	spin_unlock_irqrestore(&mchan->chn_lock, flags);

	return cookie;
}

static irqreturn_t __dma_irq_handle(int irq, void *dev_id)
{
	struct sprd_dma_dev *sdev = (struct sprd_dma_dev *)dev_id;

	//spin_lock(&sdev->dma_lock);
	__dma_check_int(sdev, 1);
	//spin_unlock(&sdev->dma_lock);

	tasklet_schedule(&sdev->tasklet);

	return IRQ_HANDLED;
}

static int sprd_dma_process_completed(struct sprd_dma_dev *sdev)
{
	struct sprd_dma_chn *mchan;
	struct sprd_dma_desc *mdesc;
	struct sprd_dma_desc *first;
	dma_cookie_t last_cookie = 0;
	struct dma_async_tx_descriptor *desc;
	unsigned long flags;
	LIST_HEAD(list);
	u32 dma_chn_cnt = sdev->ap_chn_cnt + sdev->aon_chn_cnt + sdev->agcp_chn_cnt;
	int i;

	for (i = 0; i < dma_chn_cnt; i++) {
		mchan = &sdev->channels[i];

		/* deal the compelete dma request */
		spin_lock_irqsave(&mchan->chn_lock, flags);
		if(!list_empty(&mchan->completed))
			list_splice_tail_init(&mchan->completed, &list);
		spin_unlock_irqrestore(&mchan->chn_lock, flags);

		if (list_empty(&list))
			continue;

		list_for_each_entry(mdesc, &list, node) {
			pr_dma("Channel [%d] complete list have node!\n",i);
			desc = &mdesc->desc;

			if (desc->callback)
				desc->callback(desc->callback_param);

			/* submit desc->next */
			dma_run_dependencies(desc);
			last_cookie = desc->cookie;
		}

		spin_lock_irqsave(&mchan->chn_lock, flags);
		list_splice_tail_init(&list, &mchan->free);

		/* continue to process new adding queued request */
		if (mdesc->mutiple_chan == 0xFF) {
			if (!list_empty(&mchan->queued)) {
				pr_dma("Channel [%d] queued list have node!\n",i);
				if (list_empty(&mchan->active)) {
					first = list_first_entry(&mchan->queued,struct sprd_dma_desc, node);
					list_move_tail(&first->node, &mchan->active);
					sprd_dma_execute(mchan);
				}
			} else {
				mchan->chan.completed_cookie = last_cookie;
				pr_dma("Channel [%d] queued list is NULL,and transfer done!\n",i);
			}
		}

		spin_unlock_irqrestore(&mchan->chn_lock, flags);
	}

	return 0;
}

static void sprd_dma_tasklet(unsigned long data)
{
	struct sprd_dma_dev *sdev = (void *)data;
	sprd_dma_process_completed(sdev);
}

static int sprd_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	dma_addr_t chn_reg_paddr;
	struct sprd_dma_desc *mdesc;
	struct sprd_dma_chn_reg *chn_reg;
	unsigned long i,flags;
	LIST_HEAD(descs);

	chn_reg = dmam_alloc_coherent(mdev->dma_dev.dev,
				MPC_DMA_DESCRIPTORS * sizeof(struct sprd_dma_chn_reg),&chn_reg_paddr,GFP_KERNEL);
	if (!chn_reg)
		return -ENOMEM;

	/* init the dma desc */
	for (i = 0; i < MPC_DMA_DESCRIPTORS; i++) {
		mdesc = (struct sprd_dma_desc *)kmem_cache_zalloc(mdev->dma_desc_node_cachep, GFP_ATOMIC);
		if (!mdesc) {
			pr_err("Memory allocation error: Allocated only %ld descriptors\n", i);
			break;
		}

		dma_async_tx_descriptor_init(&mdesc->desc, chan);
		mdesc->desc.flags = DMA_CTRL_ACK;
		mdesc->desc.tx_submit = sprd_desc_submit;
		mdesc->dma_chn_reg = &chn_reg[i];
		mdesc->dma_chn_reg_paddr = chn_reg_paddr + (i * sizeof(struct sprd_dma_chn_reg));
		mdesc->done = 0;
		mdesc->cycle = 0;
		mdesc->mutiple_chan = 0xFF;
		INIT_LIST_HEAD(&mdesc->node);
		INIT_LIST_HEAD(&mdesc->next_node);
		list_add_tail(&mdesc->node, &descs);
	}

	if (i == 0) {
		dma_free_coherent(mdev->dma_dev.dev,
				MPC_DMA_DESCRIPTORS * sizeof(struct sprd_dma_chn_reg),chn_reg, chn_reg_paddr);
		return -ENOMEM;
	}

	spin_lock_irqsave(&mchan->chn_lock, flags);
	mchan->dma_desc = chn_reg;
	mchan->dma_desc_paddr = chn_reg_paddr;
	list_splice_tail_init(&descs, &mchan->free);
	spin_unlock_irqrestore(&mchan->chn_lock, flags);

	/* enable dma */
	mchan->dev_id = 0;
	mchan->chan_status = USED;
	__dma_enable(chan);
	__dma_int_enable(chan);

	pr_notice("Alloc chan %d resources is OK, and chn_reg=0x%lx, chn_reg_paddr=0x%lx, paddr = 0x%lx!\n",
			mchan->chan_num, (unsigned long)chn_reg, (unsigned long)chn_reg_paddr,
			(unsigned long)mchan->dma_chn_base);

	return 0;
}

static void sprd_dma_free_chan_resources(struct dma_chan *chan)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	dma_addr_t chn_reg_paddr;
	struct sprd_dma_desc *mdesc,*tmp;
	struct sprd_dma_chn_reg *chn_reg;
	struct sprd_dma_chn *tchan;
	unsigned long flags;
	int i;
	u32 dma_chn_cnt = mdev->ap_chn_cnt + mdev->aon_chn_cnt + mdev->aon_chn_cnt;

	LIST_HEAD(descs);

	/* delete list */
	spin_lock_irqsave(&mchan->chn_lock, flags);
	list_splice_tail_init(&mchan->prepared, &mchan->free);
	list_splice_tail_init(&mchan->queued, &mchan->free);
	list_splice_tail_init(&mchan->active, &mchan->free);
	list_splice_tail_init(&mchan->completed, &mchan->free);

	list_splice_tail_init(&mchan->free, &descs);
	chn_reg = mchan->dma_desc;
	chn_reg_paddr = mchan->dma_desc_paddr;
	spin_unlock_irqrestore(&mchan->chn_lock, flags);

	pr_notice("Release chan resources chn_reg=0x%lx, chn_reg_paddr=0x%lx!\n",
			(unsigned long)chn_reg,(unsigned long)chn_reg_paddr);
	dmam_free_coherent(mdev->dma_dev.dev,
			MPC_DMA_DESCRIPTORS * sizeof(struct sprd_dma_chn_reg), chn_reg, chn_reg_paddr);

	list_for_each_entry_safe(mdesc, tmp, &descs, node)
		kmem_cache_free(mdev->dma_desc_node_cachep, mdesc);

	/* stop and disable dma */
	sprd_dma_stop(mchan);
	__dma_int_disable(chan);
	__dma_softreset(chan);
	__dma_disable(chan);

	mchan->chan_status = NO_USED;

	pr_info("Release chan resources is OK!\n");
}

static int sprd_dma_check_int(struct dma_chan *chan)
{
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(chan);

	__dma_check_int(sdev, 0);
	sprd_dma_process_completed(sdev);

	return 0;
}

static enum dma_status sprd_dma_tx_status(struct dma_chan *chan,
					dma_cookie_t cookie,struct dma_tx_state *txstate)
{
	enum dma_status ret;
	int residue = txstate->residue;

	/*spin_lock(&mchan->chn_lock);*/
	ret = dma_cookie_status(chan, cookie, txstate);
	/*spin_unlock(&mchan->chn_lock);*/

	/* get dma sour&dest addr */
	if (residue == SPRD_SRC_ADDR)
        txstate->residue =  __dma_get_src_addr(chan);
    else if (residue == SPRD_DST_ADDR)
        txstate->residue =  __dma_get_dst_addr(chan);
    else
        txstate->residue = 0;

	pr_dma("%s cookie=%d, residue=0x%x!\n",__func__, cookie, txstate->residue);
	return ret;
}

static void sprd_dma_issue_pending(struct dma_chan *chan)
{
	/*
	 * We are posting descriptors to the hardware as soon as
	 * they are ready, so this function does nothing.
	 */
}

struct dma_async_tx_descriptor *sprd_dma_prep_dma_memcpy(
		struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
		size_t len, unsigned long flags)
{
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(chan);
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_desc *mdesc = NULL;
	unsigned long irq_flags;
	u32 datawidth = 0, src_step = 0,des_step = 0;
	struct sprd_dma_cfg *dma_cfg;
	int dma_cfg_cnt = dma_cfg_group.dma_cfg_cnt;
	int ret;

	/* for configuration */
	if (flags & DMA_CFG_FLAG) {
		if (dma_cfg_cnt < 1 || dma_cfg_cnt > DMA_CFG_COUNT) {
			dma_cfg_group.dma_cfg_cnt = 0;
			up(&dma_cfg_group.cfg_sema);
			pr_err("DMA wrong configuration number!\n");
			return NULL;
		} else {
			dma_cfg = (struct sprd_dma_cfg *)kzalloc(sizeof(struct sprd_dma_cfg) * dma_cfg_cnt, GFP_KERNEL);
			memcpy(dma_cfg,dma_cfg_group.dma_cfg,sizeof(struct sprd_dma_cfg)*dma_cfg_cnt);

			dma_cfg_group.dma_cfg_cnt = 0;
			memset(dma_cfg_group.dma_cfg,0,sizeof(struct sprd_dma_cfg) * DMA_CFG_COUNT);
			up(&dma_cfg_group.cfg_sema);

			goto Have_configured;
		}
	} else
		dma_cfg = (struct sprd_dma_cfg *)kzalloc(sizeof(struct sprd_dma_cfg), GFP_KERNEL);

	if (len > BLK_LEN_MASK && mchan->chn_type != FULL_DMA) {
		pr_err("Channel type isn't support!\n");
		return NULL;
	}

	/* set step automatically */
	if ((len & 0x3) == 0) {
		datawidth = 2;
		src_step = 4;
		des_step = 4;
	} else {
		if ((len & 0x1) == 0) {
			datawidth = 1;
			src_step = 2;
			des_step = 2;
		} else {
			datawidth = 0;
			src_step = 1;
			des_step = 1;
		}
	}

	/* dma reg setup */
	memset(&dma_cfg[0],0,sizeof(struct sprd_dma_cfg));
	dma_cfg[0].src_addr = src;
	dma_cfg[0].des_addr = dest;
	dma_cfg[0].datawidth = datawidth;
	dma_cfg[0].src_step = src_step;
	dma_cfg[0].des_step = src_step;
	dma_cfg[0].fragmens_len = DMA_MEMCPY_MIN_SIZE;
	if (len <= BLK_LEN_MASK) {
		dma_cfg[0].block_len = len;
		dma_cfg[0].req_mode = BLOCK_REQ_MODE;
		dma_cfg[0].irq_mode = BLK_DONE;
	} else {
		dma_cfg[0].block_len = DMA_MEMCPY_MIN_SIZE;
		dma_cfg[0].transcation_len = len;
		dma_cfg[0].req_mode = TRANS_REQ_MODE;
		dma_cfg[0].irq_mode = TRANS_DONE;
	}
	dma_cfg_cnt = 1;

Have_configured:
	/* get a free dma desc */
	spin_lock_irqsave(&mchan->chn_lock, irq_flags);
	if(!list_empty(&mchan->free)){
		mdesc = list_first_entry(&mchan->free, struct sprd_dma_desc, node);
		list_del(&mdesc->node);
	}
	spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);

	if (!mdesc) {
		sprd_dma_process_completed(sdev);
		kfree(dma_cfg);
		return NULL;
	}

	/* config into dma reg */
	if (dma_cfg_cnt == 1)
		ret = __dma_config(chan,mdesc,&dma_cfg[0],NULL,CONFIG_DESC);
	else if (dma_cfg_cnt > 1)
		ret = __dma_config_linklist(chan,mdesc,&dma_cfg[0],dma_cfg_cnt);
	else
		pr_err("DMA configuration count isn't available!\n");

	kfree(dma_cfg);
	if (ret < 0) {
		spin_lock_irqsave(&mchan->chn_lock, irq_flags);
		list_add_tail(&mdesc->node, &mchan->free);
		spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);
		pr_err("Configuration is error!\n");
		return NULL;
	}

	/* support hardware request */
	if (flags & DMA_HARDWARE_FLAG)
		mchan->re_mode = HARDWARE_REQ;
	else {
		mchan->re_mode = SOFTWARE_REQ;
		mchan->dev_id =  DMA_UID_SOFTWARE;
	}

	/* add the prepare list */
	spin_lock_irqsave(&mchan->chn_lock, irq_flags);
	list_add_tail(&mdesc->node, &mchan->prepared);
	spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);

	return &mdesc->desc;
}

struct dma_async_tx_descriptor *sprd_prep_dma_sg(struct dma_chan *chan,
								struct scatterlist *dst_sg, unsigned int dst_nents,
								struct scatterlist *src_sg, unsigned int src_nents,
								unsigned long flags)
{
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(chan);
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	struct sprd_dma_desc *mdesc = NULL;
	struct sprd_dma_desc *first_mdesc = NULL;
	struct scatterlist *sg_d;
	struct scatterlist *sg_s;
	unsigned int scatterlist_entry,src_dma_len,dst_dma_len;
	unsigned long len;
	int i, ret;
	u32 datawidth = 0, src_step = 0,des_step = 0;
	struct sprd_dma_cfg dma_cfg_t;
	dma_addr_t dst_dma_addr,src_dma_addr;
	unsigned long irq_flags;

	/* security check */
	if (dst_nents != src_nents ) {
		pr_err("DMA scatterlist entry count is not equal!\n");
		return NULL;
	} else
		scatterlist_entry = src_nents;

	if (scatterlist_entry > MPC_DMA_DESCRIPTORS) {
		pr_err("DMA scatterlist is overrun!\n");
		return NULL;
	}

	if (flags & DMA_HARDWARE_FLAG) {
		pr_err("DMA scatterlist do not support hardware request!\n");
		return NULL;
	}

	/* for scatter list */
	for (i = 0, sg_d = dst_sg, sg_s = src_sg; i < scatterlist_entry;
					i++, sg_d = sg_next(sg_d), sg_s = sg_next(sg_s)){
		dst_dma_addr = sg_dma_address(sg_d);
		dst_dma_len = sg_dma_len(sg_d);
		src_dma_addr = sg_dma_address(sg_s);
		src_dma_len = sg_dma_len(sg_s);

		pr_dma("DMA scatterlist dst_dma_addr=0x%x, src_dma_addr=0x%x,dst_len=%d,src_len=%d!\n",
				(unsigned int)dst_dma_addr,(unsigned int)src_dma_addr,dst_dma_len,src_dma_len);

		if (dst_dma_len != src_dma_len)
			continue;
		else
			len = src_dma_len;

		if ((len & 0x3) == 0) {
			datawidth = 2;
			src_step = 4;
			des_step = 4;
		} else {
			if ((len & 0x1) == 0) {
				datawidth = 1;
				src_step = 2;
				des_step = 2;
			} else {
				datawidth = 0;
				src_step = 1;
				des_step = 1;
			}
		}

		memset(&dma_cfg_t,0,sizeof(struct sprd_dma_cfg));
		dma_cfg_t.src_addr = src_dma_addr;
		dma_cfg_t.des_addr = dst_dma_addr;
		dma_cfg_t.datawidth = datawidth;
		dma_cfg_t.src_step = src_step;
		dma_cfg_t.des_step = src_step;
		dma_cfg_t.fragmens_len = DMA_MEMCPY_MIN_SIZE;
		if (len <= BLK_LEN_MASK) {
			dma_cfg_t.block_len = len;
			dma_cfg_t.req_mode = BLOCK_REQ_MODE;
			dma_cfg_t.irq_mode = BLK_DONE;
		} else {
			dma_cfg_t.block_len = DMA_MEMCPY_MIN_SIZE;
			dma_cfg_t.transcation_len = len;
			dma_cfg_t.req_mode = TRANS_REQ_MODE;
			dma_cfg_t.irq_mode = TRANS_DONE;
		}

		spin_lock_irqsave(&mchan->chn_lock, irq_flags);
		if (!list_empty(&mchan->free)) {
			mdesc = list_first_entry(&mchan->free, struct sprd_dma_desc, node);
			list_del(&mdesc->node);
		}
		spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);
		if (!mdesc) {
			sprd_dma_process_completed(sdev);
			pr_err("Warning: There are not enough mdesc for scatterlist!\n");
		}

		ret = __dma_config(chan,mdesc,&dma_cfg_t,NULL,CONFIG_DESC);
		if (ret < 0) {
			pr_err("Warning: Configuration is error!\n");
			spin_lock_irqsave(&mchan->chn_lock, irq_flags);
			list_add_tail(&mdesc->node, &mchan->free);
			spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);
			continue;
		}

		if (!first_mdesc) {
			first_mdesc = mdesc;
			spin_lock_irqsave(&mchan->chn_lock, irq_flags);
			list_add_tail(&mdesc->node, &mchan->prepared);
			spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);
		} else {
			spin_lock_irqsave(&mchan->chn_lock, irq_flags);
			list_add_tail(&mdesc->node, &first_mdesc->next_node);
			spin_unlock_irqrestore(&mchan->chn_lock, irq_flags);
		}
	}

	mchan->re_mode = SOFTWARE_REQ;

	if(first_mdesc)
		return &first_mdesc->desc;
	else
		return NULL;
}

/* copy dma configuration */
static void sprd_dma_copy(struct sprd_dma_chn *mchan, unsigned long arg)
{
	unsigned int i = 0;
	do {
		memcpy(&dma_cfg_group.dma_cfg[i],(struct sprd_dma_cfg *)arg,sizeof(struct sprd_dma_cfg));
		arg += sizeof(struct sprd_dma_cfg);

		pr_dma("%s, i:%d block_len:0x%x,fragmens_len:0x%x,src_step:0x%x,des_addr:0x%x,"
			"src_addr:0x%x,link_cfg_p:0x%x,link_cfg_v:0x%lx,is_end:%d,transcation_len:0x%x\n",
			__func__,i,dma_cfg_group.dma_cfg[i].block_len,dma_cfg_group.dma_cfg[i].fragmens_len,
			dma_cfg_group.dma_cfg[i].src_step,dma_cfg_group.dma_cfg[i].des_addr,
			dma_cfg_group.dma_cfg[i].src_addr,dma_cfg_group.dma_cfg[i].link_cfg_p,
			dma_cfg_group.dma_cfg[i].link_cfg_v,dma_cfg_group.dma_cfg[i].is_end,
			dma_cfg_group.dma_cfg[i].transcation_len);

	} while(dma_cfg_group.dma_cfg[i++].is_end == 0 && i < (DMA_CFG_COUNT - 1));
	dma_cfg_group.dma_cfg_cnt = i;
	pr_dma("Get dma configuration number is %d!\n",i);
}

/* terminate dma channel */
static int sprd_terminate_all(struct sprd_dma_chn *mchan)
{
	struct dma_chan *chan = &mchan->chan;

	sprd_dma_free_chan_resources(chan);
	return 0;
}

/* dma control method */
static int sprd_dma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd, unsigned long arg)
{
	int ret = 0;
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);

	switch(cmd){
	case DMA_SLAVE_CONFIG:
		if (down_trylock(&dma_cfg_group.cfg_sema)) {
			pr_err("DMA resource is busy, try again...\n");
			return -ENXIO;
		}
		sprd_dma_copy(mchan, arg);
		break;
	case DMA_PAUSE:
		ret = sprd_dma_stop(mchan);
		break;
	case DMA_TERMINATE_ALL:
		ret = sprd_terminate_all(mchan);
		break;
	case DMA_RESUME:
		ret = sprd_dma_execute(mchan);
		break;
	default:
		ret = -ENXIO;
		break;
	}

	return ret;
}

/* for dma channel request filter*/
bool sprd_dma_filter_fn(struct dma_chan *chan, void *filter_param)
{
	struct sprd_dma_chn *mchan = NULL;
	unsigned int type = *(unsigned int *)filter_param;
	struct sprd_dma_dev *mdev = to_sprd_dma_dev(chan);
	u32 ap_chn_cnt = mdev->ap_chn_cnt;
	u32 aon_chn_cnt = mdev->aon_chn_cnt;

	mchan = to_sprd_dma_chan(chan);
	if(!mchan)
		return false;

	if(type == AP_STANDARD_DMA){
		if (mchan->chn_type == STANDARD_DMA && mchan->chan_num < ap_chn_cnt)
			return true;
	}else if(type == AP_FULL_DMA){
		if (mchan->chn_type == FULL_DMA && mchan->chan_num < ap_chn_cnt)
			return true;
	}else if(type == AON_STANDARD_DMA){
		if (mchan->chn_type == STANDARD_DMA && mchan->chan_num > (ap_chn_cnt - 1))
			return true;
	}else if(type == AON_FULL_DMA){
		if (mchan->chn_type == FULL_DMA && mchan->chan_num > (ap_chn_cnt - 1))
			return true;
	}else if(type == AGCP_FULL_DMA){
		if (mchan->chn_type == FULL_DMA && mchan->chan_num > (ap_chn_cnt + aon_chn_cnt - 1))
			return true;
	}else if((type & 0xf00) == NUM_REQUEST_DMA){
		if (mchan->chan_num == (type & 0xff))
			return true;
	}

	return false;
}

/* for dma channel request filter*/
bool sprd_dma_filter_fn_t(struct dma_chan *chan, void *param)
{
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(chan);
	unsigned int req = *(unsigned int *)param;

	return req == (mchan->chan_num + 1);
}

/* for dma debug */
int sprd_dma_check_register(struct dma_chan *c)
{
	volatile struct sprd_dma_chn_reg *dma_reg = NULL;
	struct sprd_dma_chn *mchan = to_sprd_dma_chan(c);

	dma_reg = (struct sprd_dma_chn_reg *)mchan->dma_chn_base;
	__dma_cfg_check_register(c, (void __iomem *)dma_reg);

	return 0;
}

int sprd_dma_chn_start_chn_cfg(struct dma_async_tx_descriptor *desc,
		struct dma_chan *mstr_chn, struct dma_chan *slv_chn,
		int start_mode, int group)
{
	struct sprd_dma_desc *first = NULL;
	struct sprd_dma_glb_reg *dma_glb_reg;
	struct sprd_dma_dev *sdev = to_sprd_dma_dev(mstr_chn);
	struct sprd_dma_chn *mstr_chan = to_sprd_dma_chan(mstr_chn);
	struct sprd_dma_chn *slv_chan = to_sprd_dma_chan(slv_chn);
	struct sprd_dma_desc *mdesc = to_sprd_dma_desc(desc);
	struct sprd_dma_chn_reg *dma_chn_reg = NULL;
	struct sprd_dma_chn_reg *first_reg = NULL;
	u32 ap_chn_cnt = sdev->ap_chn_cnt;
	u32 aon_chn_cnt = sdev->aon_chn_cnt;
	u32 agcp_chn_cnt = sdev->agcp_chn_cnt;
	unsigned long flags = 0;
	u32 mstr_chn_idx = 0, slv_chn_idx = 0;

	if (mstr_chan->chan_num < 0 || slv_chan->chan_num < 0 ||
		start_mode <= NO_INT || start_mode >= CONFIG_ERR) {
		pr_err("dma mstr chn %d slv chn %d start mode %d group %d \n",
			mstr_chan->chan_num, slv_chan->chan_num, start_mode, group);
		return -EINVAL;
	}

	if (mstr_chan->chan_num < sdev->ap_chn_cnt)
		 dma_glb_reg = sdev->dma_glb_base;
	else
		dma_glb_reg = sdev->agcp_dma_glb_base;

	__dma_chn_start_chn_dis(dma_glb_reg, group);

	switch (start_mode) {
	case FRAG_DONE:
		start_mode = 0;
		break;
	case BLK_DONE:
		start_mode = 1;
		break;
	case TRANS_DONE:
		start_mode = 2;
		break;
	case LIST_DONE:
		start_mode = 3;
		break;
	default:
		return -EINVAL;
	}

	if (mstr_chan->chan_num < ap_chn_cnt) {
		mstr_chn_idx = mstr_chan->chan_num + 1;
		slv_chn_idx = slv_chan->chan_num + 1;
	} else if (mstr_chan->chan_num < (ap_chn_cnt + aon_chn_cnt)) {
		mstr_chn_idx = mstr_chan->chan_num - ap_chn_cnt + 1;
		slv_chn_idx = slv_chan->chan_num - ap_chn_cnt + 1;
	} else {
		mstr_chn_idx = mstr_chan->chan_num  - ap_chn_cnt - aon_chn_cnt + 1;
		slv_chn_idx = slv_chan->chan_num  - ap_chn_cnt - aon_chn_cnt + 1;
	}

	switch (group) {
	case DMA_CHN_GROUP_1:
		dma_glb_reg->cfg_group1 |= (mstr_chn_idx << MASTER_CHN_OFFSET);
		dma_glb_reg->cfg_group1 |= (slv_chn_idx << SLAVE_CHN_OFFSET);
		dma_glb_reg->cfg_group1 |= ((1 << start_mode) << START_MODE_OFFSET);
		dma_glb_reg->cfg_group1 |= CHN_START_CHN;
		break;
	case DMA_CHN_GROUP_2:
		dma_glb_reg->cfg_group2 |= (mstr_chn_idx << MASTER_CHN_OFFSET);
		dma_glb_reg->cfg_group2 |= (slv_chn_idx << SLAVE_CHN_OFFSET);
		dma_glb_reg->cfg_group2 |= ((1 << start_mode) << START_MODE_OFFSET);
		dma_glb_reg->cfg_group2 |= CHN_START_CHN;
		break;
	default:
		return -EINVAL;
	}

	if(!list_empty(&slv_chan->queued)) {
		list_for_each_entry(first, &slv_chan->queued, node) {
			if (first == mdesc) {
				flags = 1;
				break;
			}
		}
	}
	if (!flags) {
		if (!list_empty(&slv_chan->prepared)) {
			list_for_each_entry(first, &slv_chan->prepared, node) {
				if (first == mdesc) {
					flags = 1;
					break;
				}
			}
		}
	}
	if (!flags)
		return 0;

	//memcpy((void __iomem *)slv_chan->dma_chn_base, (void *)first->dma_chn_reg,
	//			sizeof(struct sprd_dma_chn_reg));
	dma_chn_reg = (struct sprd_dma_chn_reg *)slv_chan->dma_chn_base;
	first_reg = (struct sprd_dma_chn_reg *)first->dma_chn_reg;
	dma_chn_reg->pause = first_reg->pause;
	dma_chn_reg->req = first_reg->req;
	dma_chn_reg->cfg = first_reg->cfg;
	dma_chn_reg->intc = first_reg->intc;
	dma_chn_reg->src_addr = first_reg->src_addr;
	dma_chn_reg->des_addr = first_reg->des_addr;
	dma_chn_reg->frg_len = first_reg->frg_len;
	dma_chn_reg->blk_len = first_reg->blk_len;
	dma_chn_reg->trsc_len = first_reg->trsc_len;
	dma_chn_reg->trsf_step = first_reg->trsf_step;
	dma_chn_reg->wrap_ptr = first_reg->wrap_ptr;
	dma_chn_reg->wrap_to = first_reg->wrap_to;
	dma_chn_reg->llist_ptr = first_reg->llist_ptr;
	dma_chn_reg->frg_step = first_reg->frg_step;
	dma_chn_reg->src_blk_step = first_reg->src_blk_step;
	dma_chn_reg->des_blk_step = first_reg->des_blk_step;
	mb();
	mdesc->mutiple_chan = slv_chan;
	__dma_chn_enable(slv_chan);

	return 0;
}

/* probe method */
static int sprd_dma_probe(struct platform_device *pdev)
{
	struct sprd_dma_dev *sdev = NULL;
	struct sprd_dma_chn *dma_chn = NULL;
	void __iomem *dma_ap_base = NULL;
	void __iomem *dma_aon_base = NULL;
	void __iomem *dma_agcp_base = NULL;
	struct resource *res;
	#ifdef CONFIG_ARM64
	static u64 sprd_dma_dmamask = DMA_BIT_MASK(64);
	#else
	static u64 sprd_dma_dmamask = DMA_BIT_MASK(32);
	#endif
	u32 dma_irq, aon_dma_irq, agcp_dma_irq;
	u32 aon_offset, agcp_offset;
	u32 dma_chn_cnt, ap_chn_cnt, aon_chn_cnt, agcp_chn_cnt;
	u32 ap_full_type_offset, aon_full_type_offset, agcp_full_type_offset;
	int ret, i;

	if (!pdev->dev.of_node) {
		pr_err("Error: Can't find the dma node!\n");
		return -ENODEV;
	}

	if (of_property_read_u32(pdev->dev.of_node, "#dma-channels", &dma_chn_cnt)) {
		pr_err("Error: Can't get total dma channel number infor!\n");
		return -ENODEV;
	}

	if (of_property_read_u32(pdev->dev.of_node, "sprd,aon-offset", &aon_offset)) {
		pr_err("Error: Can't get aon offset dma infor!\n");
		return -ENODEV;
	}

	if (of_property_read_u32(pdev->dev.of_node, "sprd,agcp-offset", &agcp_offset)) {
		pr_err("Error: Can't get agcp offset dma infor!\n");
		agcp_offset = dma_chn_cnt;
		//return -ENODEV;
	}

	if (of_property_read_u32_index(pdev->dev.of_node, "sprd,full-type-offset",
								   0, &ap_full_type_offset)) {
		pr_err("Error: Can't get ap dma full type offset!\n");
		ap_full_type_offset = 25;
		//return -ENODEV;
	}

	if (of_property_read_u32_index(pdev->dev.of_node, "sprd,full-type-offset",
								   1, &aon_full_type_offset)) {
		pr_err("Error: Can't get ap dma full type offset!\n");
		aon_full_type_offset = 25;
		//return -ENODEV;
	}

	if (of_property_read_u32_index(pdev->dev.of_node, "sprd,full-type-offset",
								   2, &agcp_full_type_offset)) {
		pr_err("Error: Can't get ap dma full type offset!\n");
		agcp_full_type_offset = 0;
		//return -ENODEV;
	}

	agcp_chn_cnt = dma_chn_cnt - agcp_offset;
	aon_chn_cnt = dma_chn_cnt - aon_offset - agcp_chn_cnt;
	ap_chn_cnt = dma_chn_cnt - aon_chn_cnt - agcp_chn_cnt;
	if (ap_full_type_offset > ap_chn_cnt ||
		aon_full_type_offset > aon_chn_cnt ||
		agcp_full_type_offset > agcp_chn_cnt) {
		pr_err("Error: dma full type channel count is incorrect!\n");
		return -ENOMEM;
	}

	pr_dma("DMA total chn: %d ,ap chn :%d,aon chn :%d,agcp chn :%d\
		ap full chn offset: %d, aon full chn offset: %d, agcp full chn offset: %d\n",
		dma_chn_cnt, ap_chn_cnt, aon_chn_cnt, agcp_chn_cnt,
		ap_full_type_offset, aon_full_type_offset, agcp_full_type_offset);

	dma_irq = platform_get_irq(pdev, 0);
	if (dma_irq < 0) {
		pr_err("Error: Can't get the ap dma irq number!\n");
		return -EBUSY;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("Error: Can't get ap dma registers resource!\n");
		return -EBUSY;
	}

	dma_ap_base = devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!dma_ap_base) {
		pr_err("Error: Can't get the ap dma base addr!\n");
		return -ENOMEM;
	}

	if (aon_chn_cnt > 0) {
		aon_dma_irq = platform_get_irq(pdev, 1);
		if (aon_dma_irq < 0) {
			pr_err("Error: Can't get the aon dma irq number!\n");
			return -EBUSY;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res) {
			pr_err("Error: Can't get aon dma registers resource!\n");
			return -EBUSY;
		}

		dma_aon_base = devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
		if (!dma_aon_base) {
			pr_err("Error: Can't get the aon dma base addr!\n");
			return -ENOMEM;
		}
	}

	if (agcp_chn_cnt > 0) {
		agcp_dma_irq = platform_get_irq(pdev, 2);
		if (agcp_dma_irq < 0) {
			pr_err("Error: Can't get the agcp dma irq number!\n");
			return -EBUSY;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!res) {
			pr_err("Error: Can't get agcp dma registers resource!\n");
			return -EBUSY;
		}

		dma_agcp_base = devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
		if (!dma_agcp_base) {
			pr_err("Error: Can't get the agcp dma base addr!\n");
			return -ENOMEM;
		}
	}

	pr_dma("DMA irq number is %d,aon irq num is %d, agcp irq num is %d!\n",
		dma_irq, aon_dma_irq, agcp_dma_irq);

	/* dma device momery alloc */
	sdev = devm_kzalloc(&pdev->dev, (sizeof(*sdev) +
						(sizeof(struct sprd_dma_chn) * dma_chn_cnt)), GFP_KERNEL);
	if (!sdev) {
		pr_err("Error: DMA alloc dma dev failed!\n");
		return -ENOMEM;
	}

	/* initialize dma device */
	dma_cap_set(DMA_MEMCPY | DMA_SG, sdev->dma_dev.cap_mask);
	sdev->dma_dev.chancnt = dma_chn_cnt;
	INIT_LIST_HEAD(&sdev->dma_dev.channels);
	INIT_LIST_HEAD(&sdev->dma_dev.global_node);
	spin_lock_init(&sdev->dma_lock);
	pdev->dev.dma_mask = &sprd_dma_dmamask;
	pdev->dev.coherent_dma_mask = sprd_dma_dmamask;
	sdev->dma_dev.dev = &pdev->dev;

	sdev->dma_dev.device_alloc_chan_resources = sprd_dma_alloc_chan_resources;
	sdev->dma_dev.device_free_chan_resources = sprd_dma_free_chan_resources;
	sdev->dma_dev.device_tx_status = sprd_dma_tx_status;
	sdev->dma_dev.device_issue_pending = sprd_dma_issue_pending;
	sdev->dma_dev.device_prep_dma_memcpy = sprd_dma_prep_dma_memcpy;
	sdev->dma_dev.device_prep_dma_sg = sprd_prep_dma_sg;
	sdev->dma_dev.device_control = sprd_dma_control;
	sdev->dma_dev.device_chn_start_chn = sprd_dma_chn_start_chn_cfg;

	/* initialize dma chn */
	for (i = 0; i < dma_chn_cnt; i++) {
		dma_chn = &sdev->channels[i];
		dma_chn->chan.device = &sdev->dma_dev;
		dma_cookie_init(&dma_chn->chan);
		list_add_tail(&dma_chn->chan.device_node, &sdev->dma_dev.channels);

		dma_chn->chan_num = i;
		if (i < ap_full_type_offset -1 ||
			(i > (ap_chn_cnt - 1) && i < (ap_chn_cnt + aon_full_type_offset - 1)) ||
			(i > (ap_chn_cnt + aon_chn_cnt - 1) &&
			i < (ap_chn_cnt + aon_chn_cnt + agcp_full_type_offset - 1)))
			dma_chn->chn_type = STANDARD_DMA;
		else
			dma_chn->chn_type = FULL_DMA;

		dma_chn->chan_status = NO_USED;
		dma_chn->irq_handle_enable = 0;
		if (i < ap_chn_cnt)
			dma_chn->dma_chn_base = (void __iomem *)((unsigned long)dma_ap_base +
									0x1000 + DMA_CHN_OFFSET * (i));
		else if (i < (ap_chn_cnt + aon_chn_cnt)) {
			if (dma_aon_base)
				dma_chn->dma_chn_base = (void __iomem *)((unsigned long)dma_aon_base +
									0x1000 + DMA_CHN_OFFSET * (i - ap_chn_cnt));
			else
				dma_chn->dma_chn_base = NULL;
		} else {
			if (dma_agcp_base)
				dma_chn->dma_chn_base = (void __iomem *)((unsigned long)dma_agcp_base +
									0x1000 + DMA_CHN_OFFSET * (i - ap_chn_cnt - aon_chn_cnt));
			else
				dma_chn->dma_chn_base = NULL;
		}
		pr_info("dma_chn [%d] dma_chn_base = 0x%lx!\n",i,(unsigned long)dma_chn->dma_chn_base);

		spin_lock_init(&dma_chn->chn_lock);
		INIT_LIST_HEAD(&dma_chn->free);
		INIT_LIST_HEAD(&dma_chn->prepared);
		INIT_LIST_HEAD(&dma_chn->queued);
		INIT_LIST_HEAD(&dma_chn->active);
		INIT_LIST_HEAD(&dma_chn->completed);
	}

	sdev->dma_glb_base = dma_ap_base;
	sdev->irq = dma_irq;
	sdev->ap_chn_cnt = ap_chn_cnt;
	sdev->aon_dma_glb_base = dma_aon_base;
	sdev->aon_irq = aon_dma_irq;
	sdev->aon_chn_cnt = aon_chn_cnt;
	sdev->agcp_dma_glb_base = dma_agcp_base;
	sdev->agcp_irq = agcp_dma_irq;
	sdev->agcp_chn_cnt = agcp_chn_cnt;

	pr_info("dma:ap base = 0x%lx, aon base = 0x%lx, agcp base = 0x%lx!\n",
			(unsigned long)sdev->dma_glb_base, (unsigned long)sdev->aon_dma_glb_base,
			(unsigned long)sdev->agcp_dma_glb_base);

	sdev->dma_desc_node_cachep =
			kmem_cache_create("dma_desc_node",sizeof(struct sprd_dma_desc), 0,
			SLAB_HWCACHE_ALIGN,NULL);
	if(!sdev->dma_desc_node_cachep){
		pr_err("Error: DMA alloc cache failed!\n");
		return -ENOMEM;
	}

	/* irq request */
	ret = devm_request_irq(&pdev->dev, dma_irq, __dma_irq_handle, 0, "sprd_dma", (void *)sdev);
	if (ret < 0) {
		pr_err("Error: Request dma irq failed %d\n", ret);
		return ret;
	}

	if ((aon_chn_cnt > 0) && aon_dma_irq) {
		ret = devm_request_irq(&pdev->dev, aon_dma_irq, __dma_irq_handle, 0, "sprd_aon_dma", (void *)sdev);
		if (ret < 0) {
			pr_err("Error: Request aon dma irq failed %d\n", ret);
			return ret;
		}
	}

	if ((agcp_chn_cnt > 0) && agcp_dma_irq) {
		ret = devm_request_irq(&pdev->dev, agcp_dma_irq, __dma_irq_handle, 0, "sprd_agcp_dma", (void *)sdev);
		if (ret < 0) {
			pr_err("Error: Request agcp dma irq failed %d\n", ret);
			return ret;
		}
	}

	/* initial the tasklet */
	tasklet_init(&sdev->tasklet, sprd_dma_tasklet, (unsigned long)sdev);
	/* save the sdev as private data */
	platform_set_drvdata(pdev,sdev);

	/* dma device register */
	ret = dma_async_device_register(&sdev->dma_dev);
	if (ret < 0) {
		pr_err("SPRD-DMA: failed to register slave DMA engine device: %d\n",ret);
		goto register_fail;
	}

	/* Device-tree DMA controller registration */
	sprd_dma_info.dma_cap = sdev->dma_dev.cap_mask;
	ret = of_dma_controller_register(pdev->dev.of_node,
				of_dma_simple_xlate, &sprd_dma_info);
	if (ret) {
		pr_warn("OMAP-DMA: failed to register of DMA controller\n");
		goto of_register_fail;
	}

	sema_init(&dma_cfg_group.cfg_sema,1);
	dma_cfg_group.dma_cfg_cnt = 0;
	memset(dma_cfg_group.dma_cfg,0,sizeof(struct sprd_dma_cfg) * DMA_CFG_COUNT);
	pr_notice("SPRD DMA engine driver probe OK!\n");
	return 0;

of_register_fail:
	dma_async_device_unregister(&sdev->dma_dev);
register_fail:
	kmem_cache_destroy(sdev->dma_desc_node_cachep);
	return ret;
}

/* remove method */
static int sprd_dma_remove(struct platform_device *pdev)
{
	struct sprd_dma_dev *sdev = platform_get_drvdata(pdev);

	dma_async_device_unregister(&sdev->dma_dev);
	kmem_cache_destroy(sdev->dma_desc_node_cachep);
	pr_notice("SPRD DMA engine driver remove OK!\n");
	return 0;
}

static const struct of_device_id sprd_dma_match[] = {
	{ .compatible = "sprd,sharkl64-dma",},
	{ .compatible = "sprd,whale-dma",},
	{},
};

static struct platform_driver sprd_dma_driver = {
	.probe = sprd_dma_probe,
	.remove = sprd_dma_remove,
	.driver = {
		.name = "sprd_dma",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_dma_match),
	},
};

int __init sprd_dma_init(void)
{
	return platform_driver_register(&sprd_dma_driver);
}

void __exit sprd_dma_exit(void)
{
	platform_driver_unregister(&sprd_dma_driver);
}

subsys_initcall(sprd_dma_init);
module_exit(sprd_dma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Baolin.wang <Baolin.wang@spreadtrum.com>");
