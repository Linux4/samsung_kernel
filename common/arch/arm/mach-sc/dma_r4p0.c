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

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/dma_reg.h>

struct sci_dma_desc {
	const char *dev_name;
	void (*irq_handler) (int, void *);
	void *data;
	u32 dev_id;
};

static struct sci_dma_desc *dma_chns;

static DEFINE_SPINLOCK(dma_lock);
static DEFINE_MUTEX(dma_mutex);

static void __inline __dma_clk_enable(void)
{
	if (!sci_glb_read(REG_AP_AHB_AHB_EB, BIT_DMA_EB)) {
		sci_glb_set(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	}
}

static void __inline __dma_clk_disable(void)
{
	sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
}

#if 0
static void __dma_set_prio(u32 dma_chn, dma_pri_level chn_prio)
{
	u32 reg_val;

	reg_val = __raw_readl(DMA_CHN_CFG(dma_chn));
	reg_val &= ~(0x3 << 12);
	reg_val |= chn_prio << 12;

	__raw_writel(reg_val, DMA_CHN_CFG(dma_chn));
}

static int __dma_set_request_mode(u32 dma_chn, dma_request_mode mode)
{
	u32 reg_val = __raw_readl(DMA_CHN_FRAG_LEN(dma_chn));
	u32 req_mod = 0;

	switch (mode) {
	case FRAG_REQ_MODE:
		req_mod = 0x0;
		break;

	case BLOCK_REQ_MODE:
		req_mod = 0x1;
		break;

	case TRANS_REQ_MODE:
		if (dma_chn < FULL_CHN_START || dma_chn > FULL_CHN_END)
			return -EINVAL;
		req_mod = 0x2;
		break;

	case LIST_REQ_MODE:
		if (dma_chn < FULL_CHN_START || dma_chn > FULL_CHN_END)
			return -EINVAL;
		req_mod = 0x3;
		break;

	default:
		return -EINVAL;
	}

	reg_val &= ~(REQ_MODE_MASK << REQ_MODE_OFFSET);
	reg_val |= mode << REQ_MODE_OFFSET;

	__raw_writel(reg_val, DMA_CHN_FRAG_LEN(dma_chn));

	return 0;
}
#endif
static int __dma_set_int_type(u32 dma_chn, dma_int_type int_type)
{
	u32 reg_val;

	reg_val = __raw_readl(DMA_CHN_INT(dma_chn));
	reg_val &= ~0x1f;

	switch (int_type) {
	case NO_INT:
		break;

	case FRAG_DONE:
		reg_val |= 0x1;
		break;

	case BLK_DONE:
		reg_val |= 0x2;
		break;

	case TRANS_DONE:
		reg_val |= 0x4;
		break;

	case LIST_DONE:
		reg_val |= 0x8;
		break;

	case CONFIG_ERR:
		reg_val |= 0x10;
		break;

	default:
		return -EINVAL;
	}

	__raw_writel(reg_val, DMA_CHN_INT(dma_chn));

	return 0;
}

/*convert struct sci_dma_cfg to struct sci_dma_reg*/
static int __dma_cfg_check_and_convert(u32 dma_chn,
				       const struct sci_dma_cfg *cfg,
				       u32 dma_reg_addr)
{
	volatile struct sci_dma_reg *dma_reg;
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

	dma_reg = (struct sci_dma_reg *)dma_reg_addr;

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
	    (0x0 << SWT_MODE_OFFSET) |
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

static void __inline __dma_int_clr(u32 dma_chn)
{
	u32 reg_val;

	reg_val = __raw_readl(DMA_CHN_INT(dma_chn));
	__raw_writel(0x1f << 24 | reg_val, DMA_CHN_INT(dma_chn));
}

static void __inline __dma_int_dis(u32 dma_chn)
{
	__raw_writel(0x1f << 24, DMA_CHN_INT(dma_chn));
}

static void __init __dma_reg_init(void)
{
	int i = 0x100;

	/*reset the DMA */
	sci_glb_set(REG_AP_AHB_AHB_RST, 0x1 << 8);
	while (i--) ;
	sci_glb_clr(REG_AP_AHB_AHB_RST, 0x1 << 8);

	__raw_writel(0x0, DMA_FRAG_WAIT);
}

static irqreturn_t __dma_irq_handle(int irq, void *dev_id)
{
	int i;
	u32 irq_status;
	u32 reg_addr;

	spin_lock(&dma_lock);

	irq_status = readl(DMA_INT_MSK_STS);

	if (unlikely(0 == irq_status)) {
		spin_unlock(&dma_lock);
		return IRQ_NONE;
	}

	/*read again */
	irq_status = readl(DMA_INT_MSK_STS);
	while (irq_status) {
		i = __ffs(irq_status);
		irq_status &= (irq_status - 1);

		/*the dma chn index is start with 1,notice! */
		reg_addr = DMA_CHN_INT(i + 1);
		/*clean all type interrupt */
		writel(readl(reg_addr) | (0x1f << 24), reg_addr);

		if (dma_chns[i + 1].irq_handler)
			/*audio driver need to get the dma chn */
			dma_chns[i + 1].irq_handler(i + 1,
						    dma_chns[i + 1].data);
	}

	spin_unlock(&dma_lock);

	return IRQ_HANDLED;
}

static void __inline __dma_set_uid(u32 dma_chn, u32 dev_id)
{
	if (DMA_UID_SOFTWARE != dev_id) {
		__raw_writel(dma_chn, DMA_REQ_CID(dev_id));
	}
}

static void __inline __dma_chn_enable(u32 dma_chn)
{
	writel(readl(DMA_CHN_CFG(dma_chn)) | 0x1, DMA_CHN_CFG(dma_chn));
}

static void __inline __dma_chn_disable(u32 dma_chn)
{
	writel(readl(DMA_CHN_CFG(dma_chn)) & ~0x1, DMA_CHN_CFG(dma_chn));
}

static void __inline __dma_soft_request(u32 dma_chn)
{
	writel(readl(DMA_CHN_REQ(dma_chn)) | 0x1, DMA_CHN_REQ(dma_chn));
}

static void __dma_stop_and_disable(u32 dma_chn)
{
	/*if the chn has disable already, do nothing */
	if (!(readl(DMA_CHN_CFG(dma_chn)) & 0x1))
		return;

	writel(readl(DMA_CHN_PAUSE(dma_chn)) | 0x1, DMA_CHN_PAUSE(dma_chn));
	/*fixme, need to set timeout */
	while (!(readl(DMA_CHN_PAUSE(dma_chn)) & (0x1 << 16))) ;

	writel(readl(DMA_CHN_CFG(dma_chn)) & ~0x1, DMA_CHN_CFG(dma_chn));

	writel(readl(DMA_CHN_PAUSE(dma_chn)) & ~0x1, DMA_CHN_PAUSE(dma_chn));
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
	struct sci_dma_reg *dma_reg_list;

	if (dma_chn > DMA_CHN_MAX)
		return -EINVAL;

	__dma_clk_enable();

	if (node_size > 1)
		goto linklist_config;

	ret =
	    __dma_cfg_check_and_convert(dma_chn, cfg_list,
					DMA_CHx_BASE(dma_chn));
	if (ret < 0) {
		printk("%s %d error\n", __func__, __LINE__);
	}

	return ret;

 linklist_config:
	if (NULL == cfg_addr)
		return -EINVAL;

	dma_reg_list = (struct sci_dma_reg *)cfg_addr->virt_addr;

	for (i = 0; i < node_size; i++) {
		cfg_list[i].linklist_ptr = cfg_addr->phys_addr +
		    ((i + 1) % node_size) * sizeof(struct sci_dma_reg) + 0x10;
		ret =
		    __dma_cfg_check_and_convert(dma_chn, cfg_list + i,
						(u32) (dma_reg_list + i));
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
					DMA_CHx_BASE(dma_chn));

	return ret;
}

int sci_dma_request(const char *dev_name, dma_chn_type chn_type)
{
	int i;
	int dma_chn;
	u32 dma_chn_start, dma_chn_end;

	if (!dev_name)
		return -EINVAL;

	dma_chn = -EBUSY;

	if (FULL_DMA_CHN == chn_type) {
		dma_chn_start = FULL_CHN_START;
		dma_chn_end = FULL_CHN_END;
	} else {
		dma_chn_start = DMA_CHN_MIN;
		dma_chn_end = DMA_CHN_MAX;
	}

	mutex_lock(&dma_mutex);

	for (i = dma_chn_start; i <= dma_chn_end; i++) {
		if (!dma_chns[i].dev_name) {
			dma_chns[i].dev_name = dev_name;
			dma_chn = i;
			break;
		}
	}

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
	__dma_set_uid(0, dma_chns[dma_chn].dev_id);

	mutex_lock(&dma_mutex);

	memset(dma_chns + dma_chn, 0x0, sizeof(*dma_chns));

	/*if all chn be free, disbale the dma's clk */
	for (i = DMA_CHN_MIN; i <= DMA_CHN_MAX; i++) {
		if (dma_chns[i].dev_name)
			break;
	}

	if (i > DMA_CHN_MAX)
		__dma_clk_disable();

	mutex_unlock(&dma_mutex);

	return 0;
}

int sci_dma_ioctl(u32 dma_chn, dma_cmd cmd, void *arg)
{
	switch (cmd) {
	case SET_IRQ_TYPE:
		break;
	case SET_WRAP_MODE:
		break;

	default:
		break;
	}
	return 0;
}

/*support for audio driver to get current src and dst addr*/
u32 sci_dma_get_src_addr(u32 dma_chn)
{
	if (dma_chn > DMA_CHN_MAX) {
		printk("dma chn %d is overflow!\n", dma_chn);
		return 0;
	}

	return __raw_readl(DMA_CHN_SRC_ADR(dma_chn));
}

u32 sci_dma_get_dst_addr(u32 dma_chn)
{
	if (dma_chn > DMA_CHN_MAX) {
		printk("dma chn %d is overflow!\n", dma_chn);
		return 0;
	}

	return __raw_readl(DMA_CHN_DES_ADR(dma_chn));
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
	int ret;

	/*the first dma chn index is 1, notice!! */
	dma_chns = kzalloc(sizeof(*dma_chns) * (DMA_CHN_NUM + 1), GFP_KERNEL);
	if (dma_chns == NULL)
		return -ENOMEM;

	__dma_clk_enable();

	__dma_reg_init();

	__dma_clk_disable();

	ret = request_irq(IRQ_DMA_INT, __dma_irq_handle, 0, "sci-dma", NULL);
	if (ret) {
		printk(KERN_ERR "request dma irq failed %d\n", ret);
		goto request_irq_err;
	}

	return ret;

 request_irq_err:
	kfree(dma_chns);

	return ret;
}

arch_initcall(sci_init_dma);

EXPORT_SYMBOL_GPL(sci_dma_request);
EXPORT_SYMBOL_GPL(sci_dma_free);
EXPORT_SYMBOL_GPL(sci_dma_config);
EXPORT_SYMBOL_GPL(sci_dma_register_irqhandle);
EXPORT_SYMBOL_GPL(sci_dma_start);
EXPORT_SYMBOL_GPL(sci_dma_stop);
EXPORT_SYMBOL_GPL(sci_dma_ioctl);
EXPORT_SYMBOL_GPL(sci_dma_get_src_addr);
EXPORT_SYMBOL_GPL(sci_dma_get_dst_addr);
EXPORT_SYMBOL_GPL(sci_dma_memcpy);
