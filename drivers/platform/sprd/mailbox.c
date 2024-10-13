#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/resource.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/irqs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/mailbox.h>

#ifndef CONFIG_SPRD_MAILBOX_FIFO
#include <linux/sipc.h>
#include <linux/sipc_priv.h>
#endif

/* temp add for debug, will remove it finally */
#if 0
#define mb_debug pr_info

#ifdef BUG_ON
#undef BUG_ON
#define BUG_ON(n) if(n){pr_info("mbox:panic on line %d!\n", __LINE__);return(-1);}
#endif
#else
#define mb_debug(fmt, ...)
#endif

static unsigned long sprd_inbox_base;
static unsigned long sprd_outbox_base;


#define REGS_SEND_MBOX_BASE  (sprd_inbox_base)
#define REGS_RECV_MBOX_BASE  (sprd_outbox_base)
#define REGS_SEND_MBOX_RANGE (mbox_cfg.inbox_range)
#define REGS_OUT_MBOX_RANGE  (mbox_cfg.outbox_range)

#define MBOX_GET_FIFO_RD_PTR(val) ((val >> mbox_cfg.rd_bit)& mbox_cfg.rd_mask)
#define MBOX_GET_FIFO_WR_PTR(val) ((val >> mbox_cfg.wr_bit)& mbox_cfg.wr_mask)

#define MBOX_NR  (mbox_cfg.core_cnt)

#define MBOX_GO_TO_OLD_PARSE (0x12345678)

typedef struct mbox_cfg_tag{
	uint32_t inbox_irq;
	uint32_t inbox_base;
	uint32_t inbox_range;
	uint32_t inbox_fifo_size;
	uint32_t inbox_irq_mask;

	uint32_t outbox_irq;
	uint32_t outbox_base;
	uint32_t outbox_range;
	uint32_t outbox_fifo_size;
	uint32_t outbox_irq_mask;

	uint32_t rd_bit;
	uint32_t rd_mask;
	uint32_t wr_bit;
	uint32_t wr_mask;

	uint32_t enable_reg;
	uint32_t mask_bit;

	uint32_t core_cnt;
}mbox_cfg_data;

typedef struct mbox_chn_tag {
#ifdef CONFIG_SPRD_MAILBOX_FIFO
	MBOX_FUNCALL mbox_smsg_handler;
#else
	irq_handler_t mbox_smsg_handler;
#endif
	void *mbox_priv_data;
}mbox_chn_data;


#ifdef CONFIG_SPRD_MAILBOX_FIFO
typedef struct  mbox_fifo_data_tag{
	u8 core_id;
	u64 msg;
}mbox_fifo_data;

static mbox_fifo_data mbox_fifo[MBOX_FIFO_SIZE];
static mbox_fifo_data mbox_fifo_bak[MAX_SMSG_BAK];
static int mbox_fifo_bak_len = 0;

static u8 mbox_read_all_fifo_msg(void);
static void mbox_raw_recv(mbox_fifo_data *fifo);
#endif

static u8 bMboxInited = 0;
static spinlock_t mbox_lock;
static mbox_chn_data mbox_chns[MBOX_MAX_CORE_CNT];
static mbox_cfg_data mbox_cfg;

/*
static irqreturn_t  mbox_src_irqhandle(int irq_num, void *dev)
{
	u32 fifo_sts;
	u32 reg_val;

	reg_val = __raw_readl((void __iomem *)( REGS_SEND_MBOX_BASE + MBOX_ID));
	fifo_sts = __raw_readl((void __iomem *)( REGS_SEND_MBOX_BASE + MBOX_FIFO_STS));
	fifo_sts = fifo_sts & 0x3f;
	mb_debug("mbox_src_irqhandle,fifo_sts=%x\n",fifo_sts);

	if(fifo_sts & FIFO_BLOCK_STS )
		panic("target 0x%x mailbox blocked", reg_val);

	return IRQ_HANDLED;
}*/

static irqreturn_t mbox_recv_irqhandle(int irq_num, void *dev)
{
	u32 fifo_sts, irq_mask;
	int i = 0;
	void *priv_data;
	unsigned long flags;

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	u8 target_id;
	u8	fifo_len;
#else
	struct smsg_ipc *ipc = NULL;
#endif

	/* get fifo status */
	fifo_sts = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_FIFO_STS));
	irq_mask = fifo_sts & 0x0000ff00;

	/* clear irq mask & irq */
	__raw_writel(irq_mask | MBOX_IRQ_CLR_BIT,
		(void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_STS));
	mb_debug("%s,fifo_sts=0x%x\n", __func__, fifo_sts);

	spin_lock_irqsave(&mbox_lock,flags);

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	fifo_len = mbox_read_all_fifo_msg();

	while(i < fifo_len)
	{
		target_id = mbox_fifo[i].core_id;

		BUG_ON(target_id >= MBOX_NR);

		if (mbox_chns[target_id].mbox_smsg_handler){
			mb_debug("mbox msg handle,index =%d, id = %d\n", i, target_id);

			priv_data = mbox_chns[target_id].mbox_priv_data;
			mbox_chns[target_id].mbox_smsg_handler(&(mbox_fifo[i].msg), priv_data);
		}
		else if(mbox_fifo_bak_len < MAX_SMSG_BAK)
		{
			mb_debug("mbox msg bak here,index =%d, id = %d\n", i, target_id);
			memcpy(&mbox_fifo_bak[mbox_fifo_bak_len],
				&mbox_fifo[i], sizeof(mbox_fifo_data));
			mbox_fifo_bak_len++;
		}
		else
		{
			pr_debug("mbox msg drop here,index =%d, id = %d\n", i, target_id);
		}
		i++;
	}
#else
	/* if many core send a mail to this core in the same time, it may lost some interrupts,
	so in here used smsg wrptr to judge*/
	for(i = 0; i < MBOX_NR; i++)
	{
		if(mbox_chns[i].mbox_priv_data)
		{
			ipc = (struct smsg_ipc*)(mbox_chns[i].mbox_priv_data);
			if(readl(ipc->rxbuf_wrptr)
				!= readl(ipc->rxbuf_rdptr))
			{
				mb_debug("mbox msg handle,target_id = %d\n", i);
				priv_data = mbox_chns[i].mbox_priv_data;
				mbox_chns[i].mbox_smsg_handler(irq_num, priv_data);
			}
		}
	}

	/*
	irq_mask = (irq_mask) >> 8;
	while (irq_mask) {
			target_id = __ffs(irq_mask);
			if (target_id >= MBOX_NR){
			break;
		}
		irq_mask &= irq_mask - 1;

		if (mbox_chns[target_id].mbox_smsg_handler) {
			priv_data = mbox_chns[target_id].mbox_priv_data;
			mbox_chns[target_id].mbox_smsg_handler(irq_num, priv_data);
		}
	}
	*/
#endif
	spin_unlock_irqrestore(&mbox_lock,flags);
	return IRQ_HANDLED;
}

#ifdef CONFIG_SPRD_MAILBOX_FIFO
static void mailbox_process_bak_msg(void)
{
	int i;
	int cnt = 0;
	int target_id = 0;
	void *priv_data;

	for (i = 0; i < mbox_fifo_bak_len; i++)
	{
		target_id = mbox_fifo[i].core_id;

		/* has been procced */
		if(MBOX_MAX_CORE_CNT == target_id)continue;

		if (mbox_chns[target_id].mbox_smsg_handler) {
			pr_debug("mbox bak msg pass to handler,index = %d, id = %d\n",
				i, target_id);

			priv_data = mbox_chns[target_id].mbox_priv_data;
			mbox_chns[target_id].mbox_smsg_handler(&(mbox_fifo[i].msg), priv_data);
			/* set a mask indicate the bak msg is been procced*/
			mbox_fifo_bak[target_id].core_id = MBOX_MAX_CORE_CNT;
		}
		cnt++;
	}

	/* reset  mbox_fifo_bak_len*/
	if(mbox_fifo_bak_len == cnt)mbox_fifo_bak_len = 0;
}

int mbox_register_irq_handle(u8 target_id, MBOX_FUNCALL irq_handler, void *priv_data)
#else
int mbox_register_irq_handle(u8 target_id, irq_handler_t irq_handler, void *priv_data)
#endif
{
	//u32 reg_val;
	unsigned long flags;

	pr_debug("%s,target_id =%d\n", __func__, target_id);

	BUG_ON(0 == bMboxInited);

	if (target_id >= MBOX_NR || mbox_chns[target_id].mbox_smsg_handler)
		return -EINVAL;

	spin_lock_irqsave(&mbox_lock,flags);

	mbox_chns[target_id].mbox_smsg_handler = irq_handler;
	mbox_chns[target_id].mbox_priv_data = priv_data;

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	/* must do it, Ap may be have already revieved some msgs */
	mailbox_process_bak_msg();
#endif
	spin_unlock_irqrestore(&mbox_lock,flags);

	/*enable the corresponding regist core irq, but it will not case this mask in whale and T8*/
	/*
	reg_val = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	reg_val &= ~((0x1 << target_id) << 8);
	__raw_writel(reg_val, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	*/

	return 0;
}

int mbox_unregister_irq_handle(u8 target_id)
{
	u32 reg_val;

	if (target_id >= MBOX_NR || !mbox_chns[target_id].mbox_smsg_handler)
		return -EINVAL;

	BUG_ON(0 == bMboxInited);

	spin_lock(&mbox_lock);

	mbox_chns[target_id].mbox_smsg_handler = NULL;

	/*disable the corresponding regist core irq, but it can't support now in whale*/
	/*
	reg_val = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	reg_val |= (0x1 << target_id) << 8;
	__raw_writel(reg_val, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	*/

	/*clean the the corresponding regist core  irq status*/
	reg_val = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_STS));
	reg_val |= (0x1 << target_id) << 8;
	__raw_writel(reg_val, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_STS));

	spin_unlock(&mbox_lock);

	return 0;
}

#ifdef CONFIG_SPRD_MAILBOX_FIFO
static void mbox_raw_recv(mbox_fifo_data *fifo)
{
	u64 msg_l, msg_h;
	int target_id;

	BUG_ON(0 == bMboxInited);

	msg_l = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_MSG_L));
	msg_h = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_MSG_H));
	target_id = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_ID));
	mb_debug("mbox_raw_recv, id =%d, msg_l = 0x%x, msg_h = 0x%x\n",
		target_id, (unsigned int)msg_l, (unsigned int)msg_h);

	fifo->msg = (msg_h << 32) | msg_l;
	fifo->core_id = target_id;

	__raw_writel(0x1, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_TRI));

	return;;
}

static u8 mbox_read_all_fifo_msg(void)
{
	u32 fifo_sts;
	u8 fifo_depth = 0;
	u8 rd, wt;

	BUG_ON(0 == bMboxInited);

	fifo_sts = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_FIFO_STS));
	wt = MBOX_GET_FIFO_WR_PTR(fifo_sts);
	rd = MBOX_GET_FIFO_RD_PTR(fifo_sts);
	mb_debug("mbox before read, rd =%d, wt = %d\n", rd, wt);

	while(rd != wt){
	    mbox_raw_recv(&(mbox_fifo[fifo_depth]));
		fifo_sts = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_FIFO_STS));
		wt = MBOX_GET_FIFO_WR_PTR(fifo_sts);
		rd = MBOX_GET_FIFO_RD_PTR(fifo_sts);
		mb_debug("mbox after read, rd =%d, wt = %d\n", rd, wt);
		fifo_depth++;
	}

	BUG_ON(fifo_depth > mbox_cfg.outbox_fifo_size);
	return fifo_depth;
}

u32 mbox_core_fifo_full(int core_id)
{
	u32 fifo_sts = 0;
	u32 mail_box_offset;

	BUG_ON(0 == bMboxInited);

	BUG_ON(core_id > MBOX_NR);
	mail_box_offset = REGS_SEND_MBOX_RANGE * core_id;

	fifo_sts = __raw_readl((void __iomem *)(
		REGS_RECV_MBOX_BASE + mail_box_offset + MBOX_FIFO_STS));
	//mb_debug("mbox_core_fifo_full,core_id = %d, fifo_sts=0x%x\n",
		//core_id, fifo_sts);

	if(fifo_sts & FIFO_FULL_STS_MASK)
	{
		pr_err("mbox is full,core_id = %d, fifo_sts=0x%x\n", core_id, fifo_sts);
		return 1;
	}
	else
		return 0;

}
#endif

int mbox_raw_sent(u8 target_id, u64 msg)
{
#ifdef CONFIG_SPRD_MAILBOX_FIFO
	u32 l_msg = (u32)msg;
	u32 h_msg = (u32)(msg >> 32);
#endif
	unsigned long flags;

	BUG_ON(0 == bMboxInited);

	mb_debug("mbox_raw_sent,target_id=%d\n",target_id);

	/* lock the mbox, we used spin_lock_irqsave, not used the mailbox lock here */
	//while (__raw_readl((void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_LOCK)) & 0x1);
	spin_lock_irqsave(&mbox_lock,flags);

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	__raw_writel(l_msg, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_MSG_L ));
	__raw_writel(h_msg, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_MSG_H ));
#endif

	__raw_writel(target_id, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_ID));
	__raw_writel(0x1, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_TRI));

	//__raw_writel(MBOX_UNLOCK_KEY, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_LOCK));
	spin_unlock_irqrestore(&mbox_lock,flags);

	return 0;
}

static int mbox_parse_dts_new(void)
{
	int ret;
	uint32_t array[2];
	struct resource res;
	struct device_node *np;

	pr_debug("%s!\n", __func__);

	np = of_find_compatible_node(NULL, NULL, "sprd,mailbox_new");
	if(!np)
	{
		pr_err("mbox: can't find %s, will go to the old parse !\n",
			"sprd,mailbox_new");
		return(MBOX_GO_TO_OLD_PARSE);
	}

	if(!of_can_translate_address(np))
	{
		pr_err("mbox: np can't translate!\n");
		BUG_ON(1);
	}

	/*parse inbox base */
	of_address_to_resource(np, 0, &res);
	mbox_cfg.inbox_base = res.start;
	sprd_inbox_base = (unsigned long)ioremap_nocache(res.start,
			resource_size(&res));
	BUG_ON(!sprd_inbox_base);

	/*parse out base */
	of_address_to_resource(np, 1, &res);
	mbox_cfg.outbox_base = res.start;
	sprd_outbox_base = (unsigned long)ioremap_nocache(res.start,
			resource_size(&res));
	BUG_ON(!sprd_outbox_base);


	/* parse enable reg and mask bit */
	ret = of_property_read_u32_array(np, "sprd,enable-ctrl", array, 2);
	BUG_ON(ret);

	mbox_cfg.enable_reg = array[0];
	mbox_cfg.mask_bit = array[1];

	/* parse fifo size */
	ret = of_property_read_u32_array(np, "sprd,fifo-size", array, 2);
	BUG_ON(ret);

	mbox_cfg.inbox_fifo_size  = array[0];
	mbox_cfg.outbox_fifo_size = array[1];

	/* parse fifo read ptr */
	ret = of_property_read_u32_array(np, "sprd,fifo-read-ptr", array, 2);
	BUG_ON(ret);

	mbox_cfg.rd_bit  = array[0];
	mbox_cfg.rd_mask = array[1];

	/* parse fifo write ptr */
	ret = of_property_read_u32_array(np, "sprd,fifo-write-ptr", array, 2);
	BUG_ON(ret);

	mbox_cfg.wr_bit  = array[0];
	mbox_cfg.wr_mask = array[1];

	/* parse core cnt */
	ret = of_property_read_u32(np, "sprd,core-cnt", &(mbox_cfg.core_cnt));
	BUG_ON(ret);

	/* parse core range */
	ret = of_property_read_u32_array(np, "sprd,core-size", array, 2);
	BUG_ON(ret);
	mbox_cfg.inbox_range  = array[0];
	mbox_cfg.outbox_range = array[1];

	/* parse irq mask */
	ret = of_property_read_u32_array(np, "sprd,irq-disable", array, 2);
	BUG_ON(ret);
	mbox_cfg.inbox_irq_mask  = array[0];
	mbox_cfg.outbox_irq_mask = array[1];

	/*parse inbox irq */
	mbox_cfg.inbox_irq = irq_of_parse_and_map(np, 0);
	BUG_ON(!mbox_cfg.inbox_irq);

	/*parse inbox irq */
	mbox_cfg.outbox_irq = irq_of_parse_and_map(np, 1);
	BUG_ON(!mbox_cfg.outbox_irq);

	return 0;
}

/* *******************************************
The old dts parse function is reseved for sharkl old dts,
because the pld project is  too nany to modify
**********************************************/
static int mbox_parse_dts_old(void)
{
	struct resource res;
	struct device_node *np;

	pr_debug("%s, It's sharkl mailbox!\n", __func__);

	np = of_find_compatible_node(NULL, NULL, "sprd,mailbox");
	if(!np)
	{
		pr_err("mbox: old can't find %s !\n", "sprd,mailbox");
		return(-EINVAL);
	}

	if(!of_can_translate_address(np))
	{
		pr_err("mbox:old np can't translate!\n");
		return(-EINVAL);
	}

	/*parse base */
	of_address_to_resource(np, 0, &res);
	mbox_cfg.inbox_base = res.start;
	sprd_inbox_base = (unsigned long)ioremap_nocache(res.start,
			resource_size(&res));
	BUG_ON(!sprd_inbox_base);
	mbox_cfg.outbox_base = mbox_cfg.inbox_base + 0x8000;
	sprd_outbox_base = sprd_inbox_base + 0x8000;

	/*other cfg */
	mbox_cfg.inbox_range = 0x100; // old dts parse for sharkl
	mbox_cfg.outbox_range = 0x100;

	mbox_cfg.inbox_fifo_size = 8;
	mbox_cfg.outbox_fifo_size = 8;

	mbox_cfg.inbox_irq_mask = 0xFFFFFFFF;
	mbox_cfg.outbox_irq_mask = 0xEF;

	mbox_cfg.enable_reg = 0x402e0004;
	mbox_cfg.mask_bit = 21;// bit 21

	mbox_cfg.inbox_irq = SCI_IRQ(68);
	mbox_cfg.outbox_irq = SCI_IRQ(69);

	mbox_cfg.rd_bit = 28;
	mbox_cfg.rd_mask= 7;

	mbox_cfg.wr_bit = 24;
	mbox_cfg.wr_mask= 7;

	mbox_cfg.core_cnt = 8;

	return 0;
}

static void mbox_cfg_printk(void)
{
	pr_info("inbox_irq = %d, outbox_irq = %d \n",
		mbox_cfg.inbox_irq, mbox_cfg.outbox_irq);

	pr_info("inbox_base = 0x%x, outbox_base = 0x%x \n",
		mbox_cfg.inbox_base, mbox_cfg.outbox_base);

	pr_info("inbox_range = 0x%x, outbox_range = 0x%x \n",
		mbox_cfg.inbox_range, mbox_cfg.outbox_range);

	pr_info("inbox_fifo = %d, outbox_fifo = %d \n",
		mbox_cfg.inbox_fifo_size, mbox_cfg.outbox_fifo_size);

	pr_info("inbox_irq_mask = 0x%x, outbox_irq_mask = 0x%x \n",
		mbox_cfg.inbox_irq_mask, mbox_cfg.outbox_irq_mask);

	pr_info("rd_bit = %d, rd_mask = %d \n",
		mbox_cfg.rd_bit, mbox_cfg.rd_mask);

	pr_info("wr_bit = %d, wr_mask = %d \n",
		mbox_cfg.wr_bit, mbox_cfg.wr_mask);

	pr_info("enable_reg = 0x%x, mask_bit = %d, core_cnt = %d \n",
		mbox_cfg.enable_reg, mbox_cfg.mask_bit, mbox_cfg.core_cnt);
}

static int __init mbox_init(void)
{
	int ret;
	unsigned long reg;

	pr_debug("mbox_init \n");

	ret = mbox_parse_dts_new();
	if(MBOX_GO_TO_OLD_PARSE == ret)
	{
		ret = mbox_parse_dts_old();
	}

	if(ret)return(-EINVAL);

	mbox_cfg_printk();

	BUG_ON(MBOX_FIFO_SIZE < mbox_cfg.outbox_fifo_size);
	BUG_ON(MBOX_MAX_CORE_CNT < mbox_cfg.core_cnt);

	spin_lock_init(&mbox_lock);

	/* glb enable mailbox, no need enable, the default is enable */
	reg = (unsigned long)ioremap_nocache(mbox_cfg.enable_reg,
		0x100);

	sci_glb_set(reg,BIT(mbox_cfg.mask_bit));/*0x402e0004, bit 21*/
	iounmap((void*)reg);

	/* no need reset mailbox, if reset, may drop some msg from other core */
	/*
	for (i = 0; i < 0x100; i++);

	__raw_writel(0x3fffff, (void __iomem *)(REGS_SEND_MBOX_BASE  + MBOX_FIFO_RST));

	for (i = 0; i < 0x100; i++);
	__raw_writel(0x0, (void __iomem *)(REGS_RECV_MBOX_BASE +  MBOX_FIFO_RST));
	*/

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	/* normal mode */
	__raw_writel(0x0, (void __iomem *)(REGS_RECV_MBOX_BASE +  MBOX_FIFO_RST));
#else
	/* irq only mode*/
	__raw_writel(0x10000, (void __iomem *)(REGS_RECV_MBOX_BASE +  MBOX_FIFO_RST));
#endif
	/* set inbox irq mask, now disable all irq in dts */
	__raw_writel(mbox_cfg.inbox_irq_mask,
		(void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_IRQ_MSK));

	/* no need enable inbox arm irq */
	/*
	ret = request_irq(vmailbox_src_irq, mbox_src_irqhandle,
		IRQF_NO_SUSPEND, "sprd-mailbox_source", NULL);
	BUG_ON(ret);
	enable_irq_wake(vmailbox_src_irq);
	*/


	/* set outbox irq mask */
	__raw_writel(mbox_cfg.outbox_irq_mask,
		(void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));

	bMboxInited = 1;

	/* request irq */
	ret = request_irq(mbox_cfg.outbox_irq, mbox_recv_irqhandle,
		IRQF_NO_SUSPEND, "sprd-mailbox_target", NULL);
	BUG_ON(ret);

	return 0;
}


EXPORT_SYMBOL_GPL(mbox_register_irq_handle);
EXPORT_SYMBOL_GPL(mbox_unregister_irq_handle);
EXPORT_SYMBOL_GPL(mbox_raw_sent);
#ifdef CONFIG_SPRD_MAILBOX_FIFO
EXPORT_SYMBOL_GPL(mbox_core_fifo_full);
#endif

arch_initcall(mbox_init);

