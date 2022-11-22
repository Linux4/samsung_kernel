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
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>//test
#include <linux/delay.h>

#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/dma_r4p0.h>
#include <soc/sprd/dma_reg.h>

//#define DMA_DEBUG

struct sci_dma_chn_desc;
#define AP_DMA_CHN_NUM	32
#define DMA_CHN_SRC_ADR(base,chn)	(DMA_CHx_BASE(base,chn) + 0x0010)
#define DMA_CHN_DES_ADR(base,chn)	(DMA_CHx_BASE(base,chn) + 0x0014)

/*depict a dma controller*/
struct sci_dma_chip {
	void __iomem *dma_glb_base;
	spinlock_t chip_lock;
	/*point to them chns*/
	struct sci_dma_chn_desc *chn_desc;
};

struct sci_dma_chn_desc {
	void __iomem *dma_chn_base;
	struct sci_dma_chip *dma_chip;
	spinlock_t chn_lock;
	const char *dev_name;
	void (*irq_handler) (int, void *);
	void *data;
	u32 dev_id;
};

#define get_chn_reg_point(dma_chn) \
	((struct sci_dma_chn_reg *)(dma_chns[(dma_chn)].dma_chn_base))

#define get_dma_chip_point(dma_chn) (dma_chns[dma_chn].dma_chip)

#define SCI_DMA_DEBUG

/*support 5 dma controllers*/
#define SCI_MAX_DMA_SIZE 5
#define AP_DMA_INDEX	0
#define AON_DMA_INDEX	1

static struct sci_dma_chip dma_chips[SCI_MAX_DMA_SIZE];
static struct sci_dma_chn_desc dma_chns[DMA_CHN_MAX + 1];
static DEFINE_MUTEX(dma_mutex);

#ifdef DMA_DEBUG
static int __dma_cfg_check_register(void __iomem * dma_reg_addr)
{
	volatile struct sci_dma_chn_reg *dma_reg;
	dma_reg = (struct sci_dma_chn_reg *)dma_reg_addr;

	printk("DMA register:pause=0x%x,\n req=0x%x,\n cfg=0x%x,\n int=0x%x,\n src_addr=0x%x,\n des_addr=0x%x,\n frg_len=0x%x,\n blk_len=0x%x,\n trsc_len=0x%x,\n trsf_step=0x%x,\n wrap_ptr=0x%x,\n wrap_to=0x%x,\n llist_ptr=0x%x,\n frg_step=0x%x,\n src_blk_step=0x%x,\n des_blk_step=0x%x\n",dma_reg->pause,dma_reg->req,dma_reg->cfg,dma_reg->intc,dma_reg->src_addr,dma_reg->des_addr,dma_reg->frg_len,dma_reg->blk_len,dma_reg->trsc_len,dma_reg->trsf_step,dma_reg->wrap_ptr,dma_reg->wrap_to,dma_reg->llist_ptr,dma_reg->frg_step,dma_reg->src_blk_step,dma_reg->des_blk_step);
	return 0;
}
#endif

static void __inline __ap_dma_clk_enable(void)
{
	if (!sci_glb_read((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB)) {
		sci_glb_set((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	}
}

static void __inline __ap_dma_clk_disable(void)
{
	sci_glb_clr((unsigned long)REG_AP_AHB_AHB_EB, BIT_DMA_EB);
}

static void __inline __dma_softreset(void)
{
	sci_glb_set(REG_AP_AHB_AHB_RST, BIT_DMA_SOFT_RST);
	udelay(1);
	sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_DMA_SOFT_RST);
}

static int __dma_set_int_type(u32 dma_chn, dma_int_type int_type)
{
	volatile struct sci_dma_chn_reg *dma_reg = get_chn_reg_point(dma_chn);

	dma_reg->intc &= ~0x1f;
	dma_reg->intc |= 0x1 << 4;
	switch (int_type) {
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

	return 0;
}

/*convert struct sci_dma_cfg to struct sci_dma_chn_reg*/
static int __dma_cfg_check_and_convert(u32 dma_chn,
				       const struct sci_dma_cfg *cfg,
				       void __iomem * dma_reg_addr)
{
	volatile struct sci_dma_chn_reg *dma_reg;
	u32 datawidth = 0, req_mode = 0, list_end = 0, fix_en = 0;
	u32 fix_mode = 0, llist_en = 0, wrap_en = 0, wrap_mode = 0;

	/* only full chn support following features:
	 * 1 transcation tranfer
	 * 2 linklist
	 * 3 copy data from fifo to fifo (not test yet, config the
	 * src_trsf_step = 0 and dst_trsf_step = 0)
	 * 4 wrap mode
	 * 5 src_frag_step or dst_frag_step
	 * 6 src_blk_step or dst_blk_step
	 */
	if (cfg->transcation_len ||
	    cfg->linklist_ptr ||
	    ((cfg->src_step | cfg->des_step) == 0) ||
	    (cfg->wrap_ptr && cfg->wrap_to) ||
	    (cfg->src_frag_step | cfg->dst_frag_step) ||
	    (cfg->src_blk_step | cfg->dst_blk_step)) {
		if (dma_chn < FULL_CHN_START) {
			return -EINVAL;
		}
	}

	switch (cfg->datawidth) {
	case BYTE_WIDTH:
		datawidth = 0;
		break;
	case SHORT_WIDTH:
		datawidth = 1;
		break;
	case WORD_WIDTH:
		datawidth = 2;
		break;
	default:
		/*linklist config */
		if (cfg->linklist_ptr)
			break;
		printk("DMA config datawidth error!\n");
		return -EINVAL;
	}

	/*check step, the step must be Integer multiple of the data width */
	if (!IS_ALIGNED(cfg->src_step, cfg->datawidth))
		return -EINVAL;

	if (!IS_ALIGNED(cfg->des_step, cfg->datawidth))
		return -EINVAL;

	req_mode = cfg->req_mode;
#if 0
	/*linklist ptr must aligned with 8 bytes */
	if (cfg->linklist_ptr) {
		if (!PTR_ALIGN(cfg->linklist_ptr, 8))
			return -EINVAL;
	}
#endif

	/*addr fix mode */
	if (cfg->src_step != 0 && cfg->des_step != 0) {
		fix_en = 0x0;
	} else {
		if ((cfg->src_step | cfg->des_step) == 0) {
			/*only full chn support data copy from fifo to fifo ??? */
			fix_en = 0x0;
		} else {
			fix_en = 0x1;
			if (cfg->src_step) {
				/*dest addr is fixed */
				fix_mode = 0x1;
			} else {
				/*src addr is fixed */
				fix_mode = 0x0;
			}
		}
	}

	/*wrap mode, if wrap_ptr point 0x0, there're some problems, Notices!!! */
	if (cfg->wrap_ptr && cfg->wrap_to) {
		wrap_en = 0x1;
		if (cfg->wrap_to == cfg->src_addr) {
			wrap_mode = 0x0;
		} else {
			if (cfg->wrap_to == cfg->des_addr) {
				wrap_mode = 0x1;
			} else {
				/*error message */
				return -EINVAL;
			}
		}
	}

	/*Notice!! if the linlklist point 0x0, thers're some porblems */
	if (cfg->linklist_ptr) {
		llist_en = 0x1;
		if (cfg->is_end) {
			list_end = 0x1;
		}
	}

	dma_reg = (struct sci_dma_chn_reg *)dma_reg_addr;

	dma_reg->pause = 0x0;
	dma_reg->req = 0x0;
	/*set default priority = 1 */
	dma_reg->cfg = DMA_PRI_1 << CHN_PRIORITY_OFFSET |
	    llist_en << LLIST_EN_OFFSET;
	/*src and des addr */
	dma_reg->src_addr = cfg->src_addr;
	dma_reg->des_addr = cfg->des_addr;
	/*frag len */
	dma_reg->frg_len =
	    (datawidth << SRC_DATAWIDTH_OFFSET) |
	    (datawidth << DES_DATAWIDTH_OFFSET) |
#if 0
	/*(((datawidth==2)?0x01:(datawidth==1)?0x02:0x00) << SWT_MODE_OFFSET) | */
	(((datawidth==2)?0x01:0x00) << SWT_MODE_OFFSET) |
#else
	    ((cfg->swt_mode) << SWT_MODE_OFFSET) |
#endif
	    (req_mode << REQ_MODE_OFFSET) |
	    (wrap_mode << ADDR_WRAP_SEL_OFFSET) |
	    (wrap_en << ADDR_WRAP_EN_OFFSET) |
	    (fix_mode << ADDR_FIX_SEL_OFFSET) |
	    (fix_en << ADDR_FIX_SEL_EN) |
	    (list_end << LLIST_END_OFFSET) | (cfg->fragmens_len & FRG_LEN_MASK);
	/*blk len */
	dma_reg->blk_len = cfg->block_len & BLK_LEN_MASK;

	if (dma_chn < FULL_CHN_START)
		return 0;

	/*trac len */
	if (0x0 == cfg->transcation_len) {
		dma_reg->trsc_len = cfg->block_len & TRSC_LEN_MASK;
	} else {
		dma_reg->trsc_len = cfg->transcation_len & TRSC_LEN_MASK;
	}

	/*trsf step */
	dma_reg->trsf_step =
	    (cfg->des_step & TRSF_STEP_MASK) << DEST_TRSF_STEP_OFFSET |
	    (cfg->src_step & TRSF_STEP_MASK) << SRC_TRSF_STEP_OFFSET;

	/*wrap ptr */
	dma_reg->wrap_ptr = cfg->wrap_ptr;
	dma_reg->wrap_to = cfg->wrap_to;

	dma_reg->llist_ptr = cfg->linklist_ptr;
	/*frag step */
	dma_reg->frg_step =
	    (cfg->dst_frag_step & FRAG_STEP_MASK) << DEST_FRAG_STEP_OFFSET |
	    (cfg->src_frag_step & FRAG_STEP_MASK) << SRC_FRAG_STEP_OFFSET;

	/*src and dst blk step */
	dma_reg->src_blk_step = cfg->src_blk_step;
	dma_reg->des_blk_step = cfg->dst_blk_step;

	return 0;
}

/*just clean, not disable*/
static void __inline __dma_int_clr(u32 dma_chn)
{
	volatile struct sci_dma_chn_reg *dma_reg = get_chn_reg_point(dma_chn);

	dma_reg->intc |= 0x1f << 24;
}

static void __inline __dma_int_dis(u32 dma_chn)
{
	volatile struct sci_dma_chn_reg *dma_reg = get_chn_reg_point(dma_chn);

	dma_reg->intc |= 0x1f << 24;
	dma_reg->intc &= ~0x1f;
}

static irqreturn_t __dma_irq_handle(int irq, void *dev_id)
{
	u32 i;
	u32 logic_dma_chn_offset, dma_chn;
	u32 irq_status;
	volatile struct sci_dma_chn_reg *dma_reg;
	struct sci_dma_chip *dma_chip = (struct sci_dma_chip*)dev_id;
	volatile struct sci_dma_glb_reg *dma_glb_reg =
		(struct sci_dma_glb_reg*)dma_chip->dma_glb_base;

	spin_lock(&dma_chip->chip_lock);

	irq_status = dma_glb_reg->int_msk_sts;

	if (unlikely(0 == irq_status)) {
		spin_unlock(&dma_chip->chip_lock);
		return IRQ_NONE;
	}

	/*AP dma,logic_dma_chn_offset = 1 */
	/*AON dma,logic_dma_chn_offset = 33 */
	logic_dma_chn_offset = dma_chip->chn_desc - dma_chns;

	while (irq_status) {
		i = __ffs(irq_status);
		irq_status &= (irq_status - 1);

		dma_chn = logic_dma_chn_offset + i;
		
		dma_reg = get_chn_reg_point(dma_chn);
		
		/*need to deal with DMA configuration error interrupt*/
		dma_reg->intc |= 0x1f << 24;

		if (dma_chns[dma_chn].irq_handler) {
			/*audio driver need to get the dma chn */
			dma_chns[dma_chn].irq_handler(dma_chn, dma_chns[dma_chn].data);
		}
	}

	spin_unlock(&dma_chip->chip_lock);

	return IRQ_HANDLED;
}

static void __inline __dma_set_uid(u32 dma_chn, u32 dev_id)
{
	struct sci_dma_chip *dma_chip = get_dma_chip_point(dma_chn);

	if (DMA_UID_SOFTWARE != dev_id) {
		__raw_writel(dma_chn,
			(volatile void *)DMA_REQ_CID(dma_chip->dma_glb_base, dev_id));
	}
}

static void __inline __dma_unset_uid(u32 dma_chn, u32 dev_id)
{
	struct sci_dma_chip *dma_chip = get_dma_chip_point(dma_chn);

	if (DMA_UID_SOFTWARE != dev_id) {
		__raw_writel(0x0,
			(volatile void *)DMA_REQ_CID(dma_chip->dma_glb_base, dev_id));
	}
}

static void __inline __dma_chn_enable(u32 dma_chn)
{
	volatile struct sci_dma_chn_reg *dma_reg = get_chn_reg_point(dma_chn);

	dma_reg->cfg |= 0x1;
}

static void __inline __dma_soft_request(u32 dma_chn)
{
	volatile struct sci_dma_chn_reg *dma_reg = get_chn_reg_point(dma_chn);

	dma_reg->req |= 0x1;
}

static void __dma_stop_and_disable(u32 dma_chn)
{
	u32 timeout = 0x2000;

	volatile struct sci_dma_chn_reg *dma_reg = get_chn_reg_point(dma_chn);

	/*if the chn has disable already, do nothing */
	if (!(dma_reg->cfg & 0x1)) {
		return;
	}

	dma_reg->pause |= 0x1;

	/*fixme, need to deal with timeout*/
	while (!(dma_reg->pause & (0x1 << 16))){
		timeout--;
		if(timeout == 0){
			__dma_softreset();
			break;
		}
	}

	dma_reg->cfg &= ~0x1;

	dma_reg->pause = 0x0;
}

/*HAL layer function*/
int sci_dma_start(u32 dma_chn, u32 dev_id)
{
	if (dma_chn > DMA_CHN_MAX)
		return -EINVAL;
	
	dma_chns[dma_chn].dev_id = dev_id;
	/*fixme, need to check dev_id */
	__dma_set_uid(dma_chn, dev_id);

	__dma_chn_enable(dma_chn);

	if (DMA_UID_SOFTWARE == dev_id)
		__dma_soft_request(dma_chn);

	return 0;
}

int sci_dma_stop(u32 dma_chn, u32 dev_id)
{
	if (dma_chn > DMA_CHN_MAX)
		return -EINVAL;

	__dma_stop_and_disable(dma_chn);

	__dma_int_clr(dma_chn);

	return 0;
}

int sci_dma_register_irqhandle(u32 dma_chn, dma_int_type int_type,
			       void (*irq_handle) (int, void *), void *data)
{
	int ret;

	if (dma_chn > DMA_CHN_MAX)
		return -EINVAL;

	if (NULL == irq_handle)
		return -EINVAL;

	ret = __dma_set_int_type(dma_chn, int_type);
	if (ret < 0)
		return ret;

	dma_chns[dma_chn].irq_handler = irq_handle;
	dma_chns[dma_chn].data = data;

	return 0;
}

int sci_dma_config(u32 dma_chn, struct sci_dma_cfg *cfg_list,
		   u32 node_size, struct reg_cfg_addr *cfg_addr)
{
	int ret, i;
	struct sci_dma_cfg list_cfg;
	struct sci_dma_chn_reg *dma_reg_list;

	if (dma_chn > DMA_CHN_MAX)
		return -EINVAL;

	__ap_dma_clk_enable();

	if (node_size > 1)
		goto linklist_config;

	ret =
	    __dma_cfg_check_and_convert(dma_chn, cfg_list,
			dma_chns[dma_chn].dma_chn_base);
	if (ret < 0) {
		printk("%s %d error\n", __func__, __LINE__);
	}

	return ret;

 linklist_config:
	if (NULL == cfg_addr)
		return -EINVAL;

	dma_reg_list = (struct sci_dma_chn_reg *)cfg_addr->virt_addr;

	for (i = 0; i < node_size; i++) {
		cfg_list[i].linklist_ptr = cfg_addr->phys_addr +
		    ((i + 1) % node_size) * sizeof(struct sci_dma_chn_reg) + 0x10;
		ret =
		    __dma_cfg_check_and_convert(dma_chn, cfg_list + i,
				dma_reg_list + i);
		if (ret < 0) {
			printk("%s %d error\n", __func__, __LINE__);
			return -EINVAL;
		}
	}

	memset((void *)&list_cfg, 0x0, sizeof(list_cfg));

	list_cfg.linklist_ptr = cfg_addr->phys_addr + 0x10;
	/*audio driver need get src and dest addr in first node */
	list_cfg.src_addr = cfg_list[0].src_addr;
	list_cfg.des_addr = cfg_list[0].des_addr;

	ret =
	    __dma_cfg_check_and_convert(dma_chn, &list_cfg,
					dma_chns[dma_chn].dma_chn_base);

	return ret;
}

int sci_dma_request(const char *dev_name, dma_chn_type chn_type)
{
	int i;
	int dma_chn;
	int dma_chn_start, dma_chn_end;

	if (!dev_name)
		return -EINVAL;

	dma_chn = -EBUSY;

	switch (chn_type) {
	case STD_DMA_CHN:
		dma_chn_start = AP_DMA_CHN_START + STD_CHN_START;
		dma_chn_end = AP_DMA_CHN_START + STD_CHN_END;
		break;

	case FULL_DMA_CHN:
		dma_chn_start = AP_DMA_CHN_START + FULL_CHN_START;
		dma_chn_end = AP_DMA_CHN_START + FULL_CHN_END;
		break;

#ifdef AON_DMA_SUPPORT
	case AON_STD_DMA_CHN:
		dma_chn_start = AON_DMA_CHN_START + STD_CHN_START;
		dma_chn_end = AON_DMA_CHN_START + STD_CHN_END;
		break;

	case AON_FULL_DMA_CHN:
		dma_chn_start = AON_DMA_CHN_START + FULL_CHN_START;
		dma_chn_end = AON_DMA_CHN_START + FULL_CHN_END;
		break;
#endif

	default:
		return -EINVAL;
	}

	mutex_lock(&dma_mutex);

	for (i = dma_chn_start; i <= dma_chn_end; i++) {
		if (!dma_chns[i].dev_name) {
			dma_chns[i].dev_name = dev_name;
			dma_chn = i;
			break;
		}
	}

#ifdef SCI_DMA_DEBUG
	if (dma_chn >= dma_chn_start) {
		struct sci_dma_chip *dma_chip;
		dma_chip = get_dma_chip_point(dma_chn);

		printk("alloc dma chn is %d, and chn reg base is %p,  dma chip base is %p\n",
			dma_chn, dma_chns[i].dma_chn_base, dma_chip->dma_glb_base);
	}
#endif
	mutex_unlock(&dma_mutex);

	return dma_chn;
}

int sci_dma_free(u32 dma_chn)
{
	int i;

	if (dma_chn > DMA_CHN_MAX)
		return -EINVAL;

	__dma_stop_and_disable(dma_chn);

	__dma_int_dis(dma_chn);

	/*set a valid dma chn for CID, the CID is start with 1 */
	__dma_unset_uid(dma_chn, dma_chns[dma_chn].dev_id);

	mutex_lock(&dma_mutex);

	dma_chns[dma_chn].dev_name = NULL;
	dma_chns[dma_chn].irq_handler = NULL;
	dma_chns[dma_chn].data = NULL;
	dma_chns[dma_chn].dev_id = 0;

	/*no need to stop AON dma*/
	if (dma_chn <= FULL_CHN_END) {
		/*if all AP chn be free, disbale the dma's clk */
		for (i = STD_CHN_START; i <= FULL_CHN_END; i++) {
			if (dma_chns[i].dev_name)
				break;
		}

		if (i >= DMA_CHN_MAX) {
			__dma_softreset();
			__ap_dma_clk_disable();
		}
	}

	mutex_unlock(&dma_mutex);

	return 0;
}

/*support for audio driver to get current src and dst addr*/
u32 sci_dma_get_src_addr(u32 dma_chn)
{
	unsigned long dma_chn_base;
	if (dma_chn > DMA_CHN_MAX) {
		printk("dma chn %d is overflow!\n", dma_chn);
		return 0;
	}

	if(dma_chn <= AP_DMA_CHN_NUM)
		dma_chn_base = (unsigned long)dma_chips[AP_DMA_INDEX].dma_glb_base;
	else
		dma_chn_base = (unsigned long)dma_chips[AON_DMA_INDEX].dma_glb_base;

	return __raw_readl((volatile void *)DMA_CHN_SRC_ADR(dma_chn_base,(dma_chn-1)));
}

u32 sci_dma_get_dst_addr(u32 dma_chn)
{
	unsigned long dma_chn_base;
	if (dma_chn > DMA_CHN_MAX) {
		printk("dma chn %d is overflow!\n", dma_chn);
		return 0;
	}
	if(dma_chn <= AP_DMA_CHN_NUM)
		dma_chn_base = (unsigned long)dma_chips[AP_DMA_INDEX].dma_glb_base;
	else
		dma_chn_base = (unsigned long)dma_chips[AON_DMA_INDEX].dma_glb_base;

	return __raw_readl((volatile void *)DMA_CHN_DES_ADR(dma_chn_base,(dma_chn-1)));
}

int sci_dma_dump_reg(u32 dma_chn, u32 *reg_base)
{
	unsigned long dma_chn_base;
	if (dma_chn >= DMA_CHN_MAX) {
		return -EINVAL;
	}
	if(dma_chn <= AP_DMA_CHN_NUM)
		dma_chn_base = (unsigned long)dma_chips[AP_DMA_INDEX].dma_glb_base;
	else
		dma_chn_base = (unsigned long)dma_chips[AON_DMA_INDEX].dma_glb_base;
	if(reg_base) {
		*reg_base = DMA_CHx_BASE(dma_chn_base, (dma_chn-1));
	}

	return  DMA_CHx_OFFSET;
}

#define DMA_MEMCPY_MIN_SIZE 64
#define DMA_MEMCPY_MAX_SIZE TRSC_LEN_MASK
static void sci_dma_memcpy_irqhandle(int chn, void *data)
{
	struct semaphore *dma_sema = (struct semaphore *)data;

	up(dma_sema);
}

int sci_dma_memcpy(u32 dest, u32 src, size_t size)
{
	int ret;
	int dma_chn;
	u32 data_width, src_step;
	u32 irq_mode;
	struct sci_dma_cfg cfg;
	struct semaphore dma_seam;

	if (size < DMA_MEMCPY_MIN_SIZE || size > DMA_MEMCPY_MAX_SIZE) {
		return -EINVAL;
	}

	if (size <= BLK_LEN_MASK) {
		dma_chn = sci_dma_request("dma memcpy", STD_DMA_CHN);
	} else {
		dma_chn = sci_dma_request("dma memcpy", FULL_DMA_CHN);
	}
	if (dma_chn < 0) {
		printk("alloc dma chn fail\n");
		return dma_chn;
	}

	sema_init(&dma_seam, 0);

	memset(&cfg, 0x0, sizeof(cfg));

	if ((size & 0x3) == 0) {
		data_width = WORD_WIDTH;
		src_step = 4;
	} else {
		if ((size & 0x1) == 0) {
			data_width = SHORT_WIDTH;
			src_step = 2;
		} else {
			data_width = BYTE_WIDTH;
			src_step = 1;
		}
	}

	cfg.src_addr = src;
	cfg.des_addr = dest;
	cfg.datawidth = data_width;
	cfg.src_step = src_step;
	cfg.des_step = src_step;
	cfg.fragmens_len = DMA_MEMCPY_MIN_SIZE;
	if (size <= BLK_LEN_MASK) {
		cfg.block_len = size;
		cfg.req_mode = BLOCK_REQ_MODE;
		irq_mode = BLK_DONE;
	} else {
		cfg.block_len = DMA_MEMCPY_MIN_SIZE;
		cfg.transcation_len = size;
		cfg.req_mode = TRANS_REQ_MODE;
		irq_mode = TRANS_DONE;
	}

	ret = sci_dma_config(dma_chn, &cfg, 1, NULL);
	if (ret < 0) {
		printk("dma memcpy config error!\n");
		sci_dma_free(dma_chn);
		return -EINVAL;
	}

	ret = sci_dma_register_irqhandle(dma_chn, irq_mode,
					 sci_dma_memcpy_irqhandle,
					 &dma_seam);
	if (ret < 0) {
		printk("dma register irqhandle failed!\n");
		sci_dma_free(dma_chn);
		return ret;
	}

	sci_dma_start(dma_chn, DMA_UID_SOFTWARE);
	down(&dma_seam);

	sci_dma_free(dma_chn);

	return 0;
}

static int __init sci_init_dma(void)
{
	int ret,i;
	u32 dma_irq;
	void __iomem *dma_reg_base;
	struct sci_dma_chip *dma_chip;
	struct sci_dma_chn_desc *chns_desc;
	struct device_node *dma_node;
	dma_node = of_find_compatible_node(NULL, NULL, "sprd,sprd-dma");
	if (!dma_node) {
		pr_warn("Can't get the dmac node!\n");
		return -ENODEV;
	}

	dma_irq = irq_of_parse_and_map(dma_node, 0);
	if (dma_irq == 0) {
		pr_warn("Can't get the dma irq number!\n");
		return -EIO;
	}
	pr_info(" dma irq number is %d!\n", dma_irq);
	dma_reg_base  = (void __iomem *)SPRD_DMA0_BASE;

#ifdef AON_DMA_SUPPORT
	u32 aon_dma_irq;
	void __iomem *aon_dma_reg_base;
	struct device_node *aon_dma_node;
	aon_dma_node = of_find_compatible_node(NULL, NULL, "sprd,aon_dma");
	if (!aon_dma_node) {
		pr_warn("Can't get the aon dmac node!\n");
		return -ENODEV;
	}

	aon_dma_irq = irq_of_parse_and_map(aon_dma_node, 0);
	if (dma_irq == 0) {
		pr_warn("Can't get the aon dma irq number!\n");
		return -EIO;
	}
	pr_info(" dma irq number is %d!\n", aon_dma_irq);
	aon_dma_reg_base = (void __iomem *)REGS_AON_DMA_BASE;
#endif

	dma_chip = &dma_chips[AP_DMA_INDEX];
	dma_chip->dma_glb_base = dma_reg_base;
	spin_lock_init(&dma_chip->chip_lock);

	chns_desc = dma_chns + AP_DMA_CHN_START;
	for (i = STD_CHN_START; i <= FULL_CHN_END; i++) {
		chns_desc[i].dma_chip = dma_chip;
		chns_desc[i].dma_chn_base = DMA_CHx_BASE(dma_reg_base, i);
		spin_lock_init(&chns_desc[i].chn_lock);
	}

	dma_chip->chn_desc = chns_desc;

	ret = request_irq(dma_irq, __dma_irq_handle, 0, "sci-dma",
		(void*)dma_chip);
	if (ret) {
		printk(KERN_ERR "request dma irq failed %d\n", ret);
		return ret;
	}

#ifdef AON_DMA_SUPPORT
	dma_chip = &dma_chips[AON_DMA_INDEX];
	dma_chip->dma_glb_base = aon_dma_reg_base;
	spin_lock_init(&dma_chip->chip_lock);

	chns_desc = dma_chns  + AON_DMA_CHN_START;
	for (i = STD_CHN_START; i <= FULL_CHN_END; i++) {
		chns_desc[i].dma_chip = dma_chip;
		chns_desc[i].dma_chn_base = DMA_CHx_BASE(aon_dma_reg_base, i);
		spin_lock_init(&chns_desc[i].chn_lock);
	}

	dma_chip->chn_desc = chns_desc;
	ret = request_irq(aon_dma_irq, __dma_irq_handle, 0, "sci-aon-dma",
		(void*)dma_chip);
	if (ret) {
		printk(KERN_ERR "request dma irq failed %d\n", ret);
		return ret;
	}
#endif

	return ret;
}

#ifdef DMA_DEBUG
static int __init dma_test(void)
{
	sci_dma_memcpy(0x806178d0,0x80000000,256);
	return 0;
}
#endif

int __init dma_new_init(void)
{
	int ret;
	ret = sci_init_dma();
#ifdef DMA_DEBUG
	dma_test();
#endif
	return ret;
}

void __exit dma_new_exit(void)
{
}

subsys_initcall(dma_new_init);
//module_init(dma_new_init);
module_exit(dma_new_exit);

MODULE_LICENSE("GPL");

EXPORT_SYMBOL_GPL(sci_dma_request);
EXPORT_SYMBOL_GPL(sci_dma_free);
EXPORT_SYMBOL_GPL(sci_dma_config);
EXPORT_SYMBOL_GPL(sci_dma_register_irqhandle);
EXPORT_SYMBOL_GPL(sci_dma_start);
EXPORT_SYMBOL_GPL(sci_dma_stop);
EXPORT_SYMBOL_GPL(sci_dma_memcpy);
