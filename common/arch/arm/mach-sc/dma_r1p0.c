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
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <mach/dma.h>

static DEFINE_SPINLOCK(dma_lock);

static struct sprd_irq_handler sprd_irq_handlers[DMA_CHN_NUM];

static irqreturn_t sprd_dma_irq(int irq, void *dev_id)
{
	int i;
	u32 irq_status;

	spin_lock(&dma_lock);

	irq_status = __raw_readl(DMA_INT_STS);

	if (unlikely(0 == irq_status)) {
		spin_unlock(&dma_lock);
		return IRQ_NONE;
	}

	__raw_writel(0xffffffff, DMA_TRANSF_INT_CLR);
	__raw_writel(0xffffffff, DMA_BURST_INT_CLR);
	__raw_writel(0xffffffff, DMA_LISTDONE_INT_CLR);

	spin_unlock(&dma_lock);

	while (irq_status) {
		i = DMA_CHN_MAX - __builtin_clz(irq_status);
		irq_status &= ~(1<<i);

		if (sprd_irq_handlers[i].handler) {
			sprd_irq_handlers[i].handler(i, sprd_irq_handlers[i].dev_id);
		} else {
			printk(KERN_ERR "DMA channel %d needs handler!\n", i);
		}
	}

	return IRQ_HANDLED;
}

/*
* sprd_dma_channel_int_clr: clear intterrupts of a dma channel
* @chn: dma channle
*/
static inline void sprd_dma_channel_int_clr(u32 chn)
{
	__raw_writel(1<<chn, DMA_BURST_INT_CLR);
	__raw_writel(1<<chn, DMA_TRANSF_INT_CLR);
	__raw_writel(1<<chn, DMA_LISTDONE_INT_CLR);
}

/*
* sprd_dma_channel_workmode_clr: clear work mode of a dma channel
* @chn: dma channle
*/
static inline void sprd_dma_channel_workmode_clr(u32 chn_id)
{
	reg_bits_and(~(1<<chn_id), DMA_LINKLIST_EN);
	reg_bits_and(~(1<<chn_id), DMA_SOFTLINK_EN);
}

/**
 * sprd_dma_set_uid: dedicate uid to a dma channel
 * @dma_chn: dma channel id, in range 0~31
 * @dma_uid: dma uid
 **/
static void sprd_dma_set_uid(u32 dma_chn, u32 dma_uid)
{
	int chn_uid_shift = 0;
	u32 dma_chn_uid_reg = 0;

	dma_chn_uid_reg = DMA_CHN_UID_BASE + DMA_UID_UNIT * (dma_chn/DMA_UID_UNIT);
	chn_uid_shift = DMA_UID_SHIFT_STP * (dma_chn%DMA_UID_UNIT);

	__raw_writel( (~(DMA_UID_MASK<<chn_uid_shift)) & __raw_readl(dma_chn_uid_reg),
									dma_chn_uid_reg);
	dma_uid = dma_uid << chn_uid_shift;
	dma_uid |= __raw_readl(dma_chn_uid_reg);
	__raw_writel(dma_uid, dma_chn_uid_reg);

	return;
}

/**
 * sprd_dma_request_channel: check the whole dma channels according to uid, return the one
 * not occupied
 * @uid: dma uid
 * @return: dma channel number
 **/
static int sprd_dma_request_channel(u32 uid)
{
	int chn = DMA_CHN_MIN;

#if 1
	if(uid == DMA_UID_SOFTWARE)
		chn = DMA_CHN_MIN;
	else
		chn = DMA_CHN_MIN + 1;
#endif

	for(; chn < DMA_CHN_NUM; chn++) {
		if (sprd_irq_handlers[chn].used == 0)
			return chn;

		/* the channel has be requestd already */
		if (sprd_irq_handlers[chn].dma_uid == uid)
			return chn;
	}

	return -EBUSY;
}

static void sprd_dma_channel_enable(int dma_chn)
{
	writel((1<<dma_chn), DMA_CHx_EN);
}



void sprd_dma_channel_disable(int dma_chn)
{
	unsigned long flags;

	spin_lock_irqsave(&dma_lock, flags);

	writel(readl(DMA_CFG) | (0x1 << 8), DMA_CFG);
	while (!(readl(DMA_TRANS_STS) & (0x1 << 30)));
	writel(0x1 << dma_chn, DMA_CHx_DIS);
	writel(readl(DMA_CFG) & ~(0x1 << 8), DMA_CFG);

	spin_unlock_irqrestore(&dma_lock, flags);
}

static void sprd_dma_channel_set_software_req(int dma_chn, int on_off)
{
	switch (on_off) {
	case ON:
		__raw_writel(1<<dma_chn, DMA_SOFT_REQ);
		break;
	case OFF:
		break;
	default:
		printk("??? channel:%d, DMA_SOFT_REQ, ON or OFF \n", dma_chn);
	}

	return;
}

/**
 * sprd_dma_chn_cfg_update: configurate a dma channel
 * @chn: dma channel id
 * @desc: dma channel configuration descriptor
 **/
static void sprd_dma_chn_cfg_update(u32 chn_id, const struct sprd_dma_channel_desc *desc)
{
	u32 chn_cfg = 0;
	u32 chn_elem_postm = 0;
	u32 chn_src_blk_postm = 0;
	u32 chn_dst_blk_postm = 0;

	chn_cfg |= ( desc->cfg_swt_mode_sel |
			desc->cfg_src_data_width |
			desc->cfg_dst_data_width |
			desc->cfg_req_mode_sel |
			desc->cfg_src_wrap_en |
			desc->cfg_dst_wrap_en  |
			(desc->cfg_blk_len & CFG_BLK_LEN_MASK)
		);
	chn_elem_postm = ((desc->src_elem_postm & SRC_ELEM_POSTM_MASK) << SRC_ELEM_POSTM_SHIFT) |
		(desc->dst_elem_postm & DST_ELEM_POSTM_MASK);
	chn_src_blk_postm = (desc->src_burst_mode) |(desc->src_blk_postm & SRC_BLK_POSTM_MASK);
	chn_dst_blk_postm = (desc->dst_burst_mode) |(desc->dst_blk_postm & DST_BLK_POSTM_MASK);

	__raw_writel(chn_cfg, DMA_CHx_CFG0(chn_id) );
	__raw_writel(desc->total_len, DMA_CHx_CFG1(chn_id) );
	__raw_writel(desc->src_addr, DMA_CHx_SRC_ADDR(chn_id) );
	__raw_writel(desc->dst_addr, DMA_CHx_DEST_ADDR(chn_id) );
	__raw_writel(desc->llist_ptr, DMA_CHx_LLPTR(chn_id) );
	__raw_writel(chn_elem_postm, DMA_CHx_SDEP(chn_id) );
	__raw_writel(chn_src_blk_postm, DMA_CHx_SBP(chn_id) );
	__raw_writel(chn_dst_blk_postm, DMA_CHx_DBP(chn_id) );
}


/***
* sprd_dma_request: request dma channel resources, return the valid channel number
* @uid: dma uid
* @irq_handler: irq_handler of dma users
* @data: parameter pass to irq_handler
* @return: dma channle id
***/
int sprd_dma_request(u32 uid, irq_handler_t irq_handler, void *data)
{
	unsigned long flags;
	int ch_id;

	if (uid < DMA_UID_MIN || uid > DMA_UID_MAX) {
		return -EINVAL;
	}

	/* use spin_lock maybe better */
	spin_lock_irqsave(&dma_lock, flags);

	ch_id = sprd_dma_request_channel(uid);
	if (ch_id < 0) {
		printk("!!!! DMA UID:%u Is Already Requested !!!!\n", uid);
		spin_unlock_irqrestore(&dma_lock, flags);
		return ch_id;
	}

	spin_unlock_irqrestore(&dma_lock, flags);

	/* init a dma channel handler */
	sprd_irq_handlers[ch_id].handler = irq_handler;
	sprd_irq_handlers[ch_id].dev_id = data;
	sprd_irq_handlers[ch_id].dma_uid = uid;
	sprd_irq_handlers[ch_id].used = 1;

	/* init a dma channel configuration */
	sprd_dma_channel_disable(ch_id);
	sprd_dma_channel_set_software_req(ch_id, OFF);
	sprd_dma_channel_int_clr(ch_id);
	sprd_dma_channel_workmode_clr(ch_id);
	sprd_dma_set_uid(ch_id, uid);

	//printk("DMA: malloc: ch_id=%d, dma_uid=%d \n", ch_id, uid);

	return ch_id;
}
EXPORT_SYMBOL_GPL(sprd_dma_request);

/*
 * sprd_dma_free: free the occupied dma channel
 * @chn_id: dma channel occupied
 */
void sprd_dma_free(u32 chn_id)
{
	unsigned long flags;

	if (chn_id > DMA_CHN_MAX) {
		printk("!!!! dma channel id is out of range:%u~%u !!!!\n",
							DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	//printk("DMA: free  : ch_id=%d, dma_uid=%d \n", chn_id, sprd_irq_handlers[chn_id].dma_uid);

	/* disable channels*/
	sprd_dma_channel_disable(chn_id);
	sprd_dma_channel_set_software_req(chn_id, OFF);

	/* disbale channels' interrupt */
	sprd_dma_set_irq_type(chn_id, BLOCK_DONE, OFF);
	sprd_dma_set_irq_type(chn_id, TRANSACTION_DONE, OFF);
	sprd_dma_set_irq_type(chn_id, LINKLIST_DONE, OFF);

	/* clear channels' work mode */
	sprd_dma_channel_workmode_clr(chn_id);

	/* clear UID, default is DMA_UID_SOFTWARE */
	sprd_dma_set_uid(chn_id, DMA_UID_SOFTWARE);/* default UID value */

	/* use spin_lock maybe better */
	spin_lock_irqsave(&dma_lock, flags);

	/* clear channels' interrupt handler */
	sprd_irq_handlers[chn_id].handler = NULL;
	sprd_irq_handlers[chn_id].dev_id = NULL;
	sprd_irq_handlers[chn_id].dma_uid = DMA_UID_SOFTWARE;/* default UID value */
	sprd_irq_handlers[chn_id].used = 0;/* not occupied */

	spin_unlock_irqrestore(&dma_lock, flags);

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_free);


/**
 * sprd_sprd_dma_channel_start: start one dma channel transfer
 * @chn_id: dma channel id, in range 0 to 31
 **/
void sprd_dma_channel_start(u32 chn_id)
{
	if ( chn_id > DMA_CHN_MAX ) {
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	sprd_dma_channel_enable(chn_id);

	if (DMA_UID_SOFTWARE == sprd_irq_handlers[chn_id].dma_uid)
		sprd_dma_channel_set_software_req(chn_id, ON);

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_start);

/**
 * sprd_dma_channel_stop: stop one dma channel transfer
 * @chn_id: dma channel id, in range 0 to 31
 **/
void sprd_dma_channel_stop(u32 chn_id)
{
	if( chn_id > DMA_CHN_MAX ){
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	sprd_dma_channel_disable(chn_id);

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_stop);

/**
 * sprd_dma_set_irq_type: enable or disable dma interrupt
 * @dma_chn: dma channel id, in range 0 to 31
 * @dma_done_type: BLOCK_DONE, TRANSACTION_DONE or LINKLIST_DONE
 * @on_off: ON:enable interrupt,
 *               OFF:disable interrupt
 **/
void sprd_dma_set_irq_type(u32 chn_id, dma_done_type irq_type, u32 on_off)
{
	unsigned long flags;

	if( chn_id > DMA_CHN_MAX ) {
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	spin_lock_irqsave(&dma_lock, flags);

	switch (irq_type) {
	case LINKLIST_DONE:
		switch (on_off) {
		case ON:
			reg_bits_or(1<<chn_id, DMA_LISTDONE_INT_EN);
			break;
		case OFF:
			reg_bits_and (~(1<<chn_id), DMA_LISTDONE_INT_EN);
			break;
		default:
			printk(" LLD_MODE, INT_EN ON OR OFF???\n");
		}
		break;

	case BLOCK_DONE:
		switch (on_off) {
		case ON:
			reg_bits_or(1<<chn_id, DMA_BURST_INT_EN);
			break;
		case OFF:
			reg_bits_and (~(1<<chn_id), DMA_BURST_INT_EN);
			break;
		default:
			printk(" BURST_MODE, INT_EN ON OR OFF???\n");
		}
		break;

	case TRANSACTION_DONE:
		switch (on_off) {
		case ON:
			reg_bits_or(1<<chn_id, DMA_TRANSF_INT_EN);
			break;
		case OFF:
			reg_bits_and (~(1<<chn_id), DMA_TRANSF_INT_EN);
			break;
		default:
			printk(" TRANSACTION_MODE, INT_EN ON OR OFF???\n");
		}
		break;

	default:
		printk("??? WHICH IRQ TYPE DID YOU SELECT ???\n");
	}

	spin_unlock_irqrestore(&dma_lock, flags);
}
EXPORT_SYMBOL_GPL(sprd_dma_set_irq_type);

/**
 * sprd_dma_set_chn_pri: set dma channel priority
 * @chn: dma channel
 * @pri: channel priority, in range lowest 0 to highest 3,
 */
void sprd_dma_set_chn_pri(u32 chn_id,  u32 pri)
{
	u32 shift;
	u32 reg_val;
	unsigned long flags;

	if(chn_id > DMA_CHN_MAX) {
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	if( pri > DMA_MAX_PRI ) {
		printk("!!! channel priority:%u out of range %d~%d !!!!\n",
						pri, DMA_MIN_PRI, DMA_MAX_PRI);
		return;
	}

	spin_lock_irqsave(&dma_lock, flags);

	shift = chn_id %16;
	switch (chn_id /16) {
	case 0:
		reg_val = __raw_readl(DMA_PRI_REG0);
		reg_val &= ~(DMA_MAX_PRI<<(2*shift));
		reg_val |= pri<<(2*shift);
		__raw_writel(reg_val, DMA_PRI_REG0);
		break;
	case 1:
		reg_val = __raw_readl(DMA_PRI_REG1);
		reg_val &= ~(DMA_MAX_PRI<<(2*shift));
		reg_val |= pri<<(2*shift);
		__raw_writel(reg_val, DMA_PRI_REG1);
		break;
	default:
		printk("!!!! WOW, WOW, WOW, chn:%u, pri%u !!!\n", chn_id, pri);
	}

	spin_unlock_irqrestore(&dma_lock, flags);

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_set_chn_pri);

/**
* setting the default parameters of the dma channel config
* in this case, user need to set following parameters only:
* cfg_blk_len
* total_len
* src_addr
* dst_addr
* ...
* @dma_cfg: dma channel config
**/
void sprd_dma_default_channel_setting(struct sprd_dma_channel_desc *chn_cfg)
{
	memset(chn_cfg, 0x0, sizeof(*chn_cfg));

	chn_cfg->src_burst_mode = SRC_BURST_MODE_8;
	chn_cfg->dst_burst_mode = SRC_BURST_MODE_8;
	chn_cfg->cfg_src_data_width = DMA_SDATA_WIDTH32;
	chn_cfg->cfg_dst_data_width = DMA_DDATA_WIDTH32;
	chn_cfg->cfg_req_mode_sel = DMA_REQMODE_TRANS;
	chn_cfg->cfg_swt_mode_sel = DMA_LIT_ENDIAN;
	chn_cfg->src_elem_postm = 0x0004;
	chn_cfg->dst_elem_postm = 0x0004;
}
EXPORT_SYMBOL_GPL(sprd_dma_default_channel_setting);

/**
 * sprd_dma_channel_config: configurate dma channel
 * @chn: dma channel
 * @work_mode: dma work mode, normal mode as default
 * @dma_cfg: dma channel configuration descriptor
 **/
void sprd_dma_channel_config(u32 chn_id, dma_work_mode work_mode,
				const struct sprd_dma_channel_desc *dma_cfg)
{
	unsigned long flags;

	if (chn_id > DMA_CHN_MAX) {
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	spin_lock_irqsave(&dma_lock, flags);

	switch (work_mode) {
	case DMA_NORMAL:
		break;
	case DMA_LINKLIST:
		reg_bits_and(~(1<<chn_id), DMA_SOFTLINK_EN);
		reg_bits_or(1<<chn_id, DMA_LINKLIST_EN);
		break;
	case DMA_SOFTLIST:
		reg_bits_and(~(1<<chn_id), DMA_LINKLIST_EN);
		reg_bits_or(1<<chn_id, DMA_SOFTLINK_EN);
		break;
	default:
		printk("???? Unsupported Work Mode You Seleced ????\n");
		return;
	}

	sprd_dma_chn_cfg_update(chn_id, dma_cfg);

	spin_unlock_irqrestore(&dma_lock, flags);
}
EXPORT_SYMBOL_GPL(sprd_dma_channel_config);

/**
* setting the default parameters of the dma channel config
* in this case, user need to set following parameters only:
* cfg_blk_len
* total_len
* src_addr
* dst_addr
* ...
* @dma_cfg: dma linklist config
**/
void sprd_dma_default_linklist_setting(struct sprd_dma_linklist_desc *chn_cfg)
{
	memset(chn_cfg, 0x0, sizeof(struct sprd_dma_channel_desc));

	chn_cfg->cfg = DMA_LIT_ENDIAN |
		DMA_SDATA_WIDTH32 |
		DMA_DDATA_WIDTH32 |
		DMA_REQMODE_LIST;

	chn_cfg->elem_postm = 0x4 << 16 | 0x4;
	chn_cfg->src_blk_postm = SRC_BURST_MODE_8;
	chn_cfg->dst_blk_postm = SRC_BURST_MODE_8;
}
EXPORT_SYMBOL_GPL(sprd_dma_default_linklist_setting);

/**
*sprd_dma_linklist_config:configurate dma linlklist mode
*@chn_id:channel id
*@chn_cfg:the struct dprd_dma_linklist_desc's phy address!!!
**/
void sprd_dma_linklist_config(u32 chn_id, u32 chn_cfg)
{
	unsigned long flags;

	if ( chn_id > DMA_CHN_MAX ) {
		printk("!!! channel id:%u out of range %u~%u !!!\n",
					chn_id, DMA_CHN_MIN, DMA_CHN_MAX);
		return;
	}

	spin_lock_irqsave(&dma_lock, flags);

	reg_bits_and(~(1 << chn_id), DMA_SOFTLINK_EN);
	reg_bits_or(1 << chn_id, DMA_LINKLIST_EN);

	__raw_writel(chn_cfg, DMA_CHx_LLPTR(chn_id));

	spin_unlock_irqrestore(&dma_lock, flags);
}
EXPORT_SYMBOL_GPL(sprd_dma_linklist_config);

/**
 * sprd_dma_softlist_config: configurate dma wrap address
 * @wrap_addr: dma wrap address descriptor
 **/
void sprd_dma_wrap_addr_config(const struct sprd_dma_wrap_addr *wrap_addr)
{
	unsigned long flags;

	spin_lock_irqsave(&dma_lock, flags);

	if (wrap_addr) {
		__raw_writel(wrap_addr->wrap_start_addr, DMA_WRAP_START);
		__raw_writel(wrap_addr->wrap_end_addr, DMA_WRAP_END);
	}

	spin_unlock_irqrestore(&dma_lock, flags);

	return;
}
EXPORT_SYMBOL_GPL(sprd_dma_wrap_addr_config);


/*
 * check all the dma channels for DEBUG
 *
 */
void sprd_dma_check_channel(void)
{
	int i;

	for ( i = DMA_CHN_MIN; i<DMA_CHN_NUM; i++) {
		if (sprd_irq_handlers[i].handler == NULL) {
			pr_debug("=== dma channel:%d is not occupied ====\n", i);
		}
	}
	for(i = DMA_CHN_MIN; i<DMA_CHN_NUM; i++) {
		pr_debug("=== sprd_irq_handlers[%d].handler:%p ====\n",
						i, sprd_irq_handlers[i].handler);
		pr_debug("=== sprd_irq_handlers[%d].dma_uid:%u ====\n",
						i, sprd_irq_handlers[i].dma_uid);
		pr_debug("=== sprd_irq_handlers[%d].used:%u ====\n",
						i, sprd_irq_handlers[i].used);
	}
}
EXPORT_SYMBOL_GPL(sprd_dma_request_channel);


/*
 * dump general dma regs for DEBUG
 *
 */
void sprd_dma_dump_regs(void){
	printk("==== DMA_CFG:0x%x ===\n", __raw_readl(DMA_CFG) );
	printk("==== DMA_CHx_EN_STATUS:0x%x ===\n", __raw_readl(DMA_CHN_EN_STATUS) );
	printk("==== DMA_LINKLIST_EN:0x%x ===\n", __raw_readl(DMA_LINKLIST_EN) );
	printk("==== DMA_SOFTLINK_EN:0x%x ===\n", __raw_readl(DMA_SOFTLINK_EN) );

	printk("==== DMA_CHN_UID0:0x%x ===\n", __raw_readl(DMA_CHN_UID0) );
	printk("==== DMA_CHN_UID1:0x%x ===\n", __raw_readl(DMA_CHN_UID1) );
	printk("==== DMA_CHN_UID2:0x%x ===\n", __raw_readl(DMA_CHN_UID2) );
	printk("==== DMA_CHN_UID3:0x%x ===\n", __raw_readl(DMA_CHN_UID3) );
	printk("==== DMA_CHN_UID4:0x%x ===\n", __raw_readl(DMA_CHN_UID4) );
	printk("==== DMA_CHN_UID5:0x%x ===\n", __raw_readl(DMA_CHN_UID5) );
	printk("==== DMA_CHN_UID6:0x%x ===\n", __raw_readl(DMA_CHN_UID6) );
	printk("==== DMA_CHN_UID7:0x%x ===\n", __raw_readl(DMA_CHN_UID7) );


	printk("==== DMA_INT_STS:0x%x ===\n", __raw_readl(DMA_INT_STS) );
	printk("==== DMA_INT_RAW:0x%x ===\n", __raw_readl(DMA_INT_RAW) );

	printk("==== DMA_LISTDONE_INT_EN:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_EN) );
	printk("==== DMA_BURST_INT_EN:0x%x ===\n", __raw_readl(DMA_BURST_INT_EN) );
	printk("==== DMA_TRANSF_INT_EN:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_EN) );

	printk("==== DMA_LISTDONE_INT_STS:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_STS) );
	printk("==== DMA_BURST_INT_STS:0x%x ===\n", __raw_readl(DMA_BURST_INT_STS) );
	printk("==== DMA_TRANSF_INT_STS:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_STS) );

	printk("==== DMA_LISTDONE_INT_RAW:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_RAW) );
	printk("==== DMA_BURST_INT_RAW:0x%x ===\n", __raw_readl(DMA_BURST_INT_RAW) );
	printk("==== DMA_TRANSF_INT_RAW:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_RAW) );

	printk("==== DMA_LISTDONE_INT_CLR:0x%x ===\n", __raw_readl(DMA_LISTDONE_INT_CLR) );
	printk("==== DMA_BURST_INT_CLR:0x%x ===\n", __raw_readl(DMA_BURST_INT_CLR) );
	printk("==== DMA_TRANSF_INT_CLR:0x%x ===\n", __raw_readl(DMA_TRANSF_INT_CLR) );


	printk("==== DMA_TRANS_STS:0x%x ===\n", __raw_readl(DMA_TRANS_STS) );
	printk("==== DMA_REQ_PEND:0x%x ===\n", __raw_readl(DMA_REQ_PEND) );
}
EXPORT_SYMBOL_GPL(sprd_dma_dump_regs);

static int __init sprd_dma_init(void)
{
	int ret;

	/* enable DMAC */
	sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_DMA_EN, AHB_CTL0);

	/* reset the DMAC */
	__raw_writel(0, DMA_CHx_EN);
	__raw_writel(0, DMA_LINKLIST_EN);
	__raw_writel(0, DMA_SOFT_REQ);
	__raw_writel(0, DMA_PRI_REG0);
	__raw_writel(0, DMA_PRI_REG1);
	__raw_writel(0xffffffff, DMA_LISTDONE_INT_CLR);
	__raw_writel(0xffffffff, DMA_TRANSF_INT_CLR);
	__raw_writel(0xffffffff, DMA_BURST_INT_CLR);
	__raw_writel(0, DMA_LISTDONE_INT_EN);
	__raw_writel(0, DMA_TRANSF_INT_EN);
	__raw_writel(0, DMA_BURST_INT_EN);

	/*set hard/soft burst wait time*/
	reg_writel(DMA_CFG, 0, DMA_HARD_WAITTIME, 0xff);
	reg_writel(DMA_CFG,16, DMA_SOFT_WAITTIME, 0xffff);

	/*register dma irq handle*/
	ret = request_irq(IRQ_DMA_INT, sprd_dma_irq, 0, "sprd-dma", NULL);
	if (ret == 0) {
		printk(KERN_INFO "request dma irq ok\n");
	} else {
		printk(KERN_ERR "request dma irq failed %d\n", ret);
		goto request_irq_err;
	}

	return ret;

request_irq_err:
	/* disable DMAC */
	sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, AHB_CTL0_DMA_EN, AHB_CTL0);

	return ret;
}

arch_initcall(sprd_dma_init);
