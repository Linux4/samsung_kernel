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
#include <linux/resource.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/irqs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/mailbox.h>

unsigned long sprd_mailbox_base;
#define REGS_SEND_MBOX_BASE sprd_mailbox_base
#define REGS_RECV_MBOX_BASE (sprd_mailbox_base + 0x8000)

/*reg offset*/
#define MBOX_ID 0x00
#define MBOX_MSG_L 0x04
#define MBOX_MSG_H 0x08
#define MBOX_TRI 0x0c
#define MBOX_FIFO_RST 0x10
#define MBOX_FIFO_STS 0x14
#define MBOX_IRQ_STS 0x18
#define MBOX_IRQ_MSK 0x1c
#define MBOX_LOCK 0x20
#define FIFO_BLOCK_STS (0x20)
#define MBOX_UNLOCK_KEY 0x5a5a5a5a

struct mbox_chn {
	spinlock_t mbox_lock;
	irq_handler_t mbox_recv_irq_handler;
	void *mbox_priv_data;
};

static struct mbox_chn mbox_chns[MBOX_NR];
static irqreturn_t  mbox_src_irqhandle(int irq_num, void *dev)
{
	u8 target_id;
	u32 irq_sts;
	u32 reg_val;

        reg_val = __raw_readl(REGS_SEND_MBOX_BASE + MBOX_ID);
	irq_sts = __raw_readl(REGS_SEND_MBOX_BASE + MBOX_FIFO_STS);
        irq_sts = irq_sts & 0x3f;
        if(irq_sts & FIFO_BLOCK_STS )
            panic("target 0x%x mailbox blocked", reg_val);
}

static irqreturn_t mbox_recv_irqhandle(int irq_num, void *dev)
{
	u8 target_id;
	u32 irq_sts;
	u32 reg_val;
	void *priv_data;
        unsigned long flags;

	irq_sts = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_FIFO_STS));
	irq_sts = irq_sts & 0x0000ff00;
	__raw_writel(irq_sts, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_STS));
	irq_sts = (irq_sts) >> 8;

	while (irq_sts) {
		target_id = __ffs(irq_sts);
		if (target_id >= MBOX_NR){
			break;
		}
		irq_sts &= irq_sts - 1;
		spin_lock_irqsave(&mbox_chns[target_id].mbox_lock,flags);
		if (mbox_chns[target_id].mbox_recv_irq_handler) {
			priv_data = mbox_chns[target_id].mbox_priv_data;
			/*we will use tasklet later*/
			mbox_chns[target_id].mbox_recv_irq_handler(irq_num, priv_data);
		}
		spin_unlock_irqrestore(&mbox_chns[target_id].mbox_lock,flags);
	}

	return IRQ_HANDLED;
}

int mbox_register_irq_handle(u8 target_id, irq_handler_t irq_handler, void *priv_data)
{
	u32 reg_val;

	if (target_id >= MBOX_NR || mbox_chns[target_id].mbox_recv_irq_handler) {
		return -EINVAL;
	}

	mbox_chns[target_id].mbox_recv_irq_handler = irq_handler;
	mbox_chns[target_id].mbox_priv_data = priv_data;

	/*enable the irq*/
	reg_val = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	reg_val &= ~((0x1 << target_id) << 8);
	__raw_writel(reg_val, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));

	return 0;
}

int mbox_unregister_irq_handle(u8 target_id)
{
	u32 reg_val;

	if (target_id >= MBOX_NR || !mbox_chns[target_id].mbox_recv_irq_handler) {
		return -EINVAL;
	}

	spin_lock(&mbox_chns[target_id].mbox_lock);

	mbox_chns[target_id].mbox_recv_irq_handler = NULL;

	/*disable the irq*/
	reg_val = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	reg_val |= (0x1 << target_id) << 8;
	__raw_writel(reg_val, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));
	reg_val = __raw_readl((void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_STS));
	reg_val |= (0x1 << target_id) << 8;
	__raw_writel(reg_val, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_STS));

	/*clean the irq status*/
	spin_unlock(&mbox_chns[target_id].mbox_lock);

	return 0;
}

int mbox_raw_sent(u8 target_id, u64 msg)
{
	/*lock the mbox, fix me*/
	while (!(__raw_readl((void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_LOCK)) & 0x1));
#if 0
	__raw_writeq(msg, REGS_SEND_MBOX_BASE + MBOX_MSG_L );
#endif
	__raw_writel(target_id, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_ID));
	__raw_writel(0x1, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_TRI));
	__raw_writel(MBOX_UNLOCK_KEY, (void __iomem *)(REGS_SEND_MBOX_BASE + MBOX_LOCK));

	return 0;
}

EXPORT_SYMBOL_GPL(mbox_register_irq_handle);
EXPORT_SYMBOL_GPL(mbox_unregister_irq_handle);
EXPORT_SYMBOL_GPL(mbox_raw_sent);

static int __init mbox_init(void)
{
	int i;
	int ret;
	struct resource res;
	struct device_node *np;
        int vmailbox_tag_irq = SCI_IRQ(69);
        int vmailbox_src_irq = SCI_IRQ(68);

	np = of_find_compatible_node(NULL, NULL, "sprd,mailbox");
	if (of_can_translate_address(np)) {
		of_address_to_resource(np, 0, &res);
		sprd_mailbox_base = ioremap_nocache(res.start,
				resource_size(&res));
	}
	if (!sprd_mailbox_base)
		panic("can't create iomap for mailbox!");

	/*glb enable and rst*/
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_MBOX_EB);

	for (i = 0; i < 0x100; i++);

	__raw_writel(0x1, (void __iomem *)(REGS_SEND_MBOX_BASE  + MBOX_FIFO_RST));
	__raw_writel(0x1, (void __iomem *)(REGS_RECV_MBOX_BASE +  MBOX_FIFO_RST));

	for (i = 0; i < 0x100; i++);

	__raw_writel(0x0, (void __iomem *)(REGS_SEND_MBOX_BASE  + MBOX_FIFO_RST));
	__raw_writel(0xffffffff, REGS_SEND_MBOX_BASE + MBOX_IRQ_MSK);
        ret = request_irq(vmailbox_src_irq, mbox_src_irqhandle, IRQF_NO_SUSPEND, "sprd-mailbox_source", NULL);
	if (ret) {
            pr_err("mbox request source irq:%d failed\n", vmailbox_src_irq);
            return -1; /*fixme*/
	}
        enable_irq_wake(vmailbox_src_irq);
	/*recv use the irq_type_1 default*/
	__raw_writel(0x10000, (void __iomem *)(REGS_RECV_MBOX_BASE +  MBOX_FIFO_RST));
	/* FIXME: irq num */
	ret = request_irq(vmailbox_tag_irq, mbox_recv_irqhandle, IRQF_NO_SUSPEND, "sprd-mailbox_target", NULL);
	if (ret) {
            pr_err("mbox request target irq:%d failed\n",vmailbox_tag_irq);
            return -1; /*fixme*/
	}
        enable_irq_wake(vmailbox_tag_irq);
	/*disable recv irq*/
	__raw_writel(0xff00, (void __iomem *)(REGS_RECV_MBOX_BASE + MBOX_IRQ_MSK));

	for (i = 0; i <  MBOX_NR; i++) {
		spin_lock_init(&mbox_chns[i].mbox_lock);
	}

	return 0;
}

arch_initcall(mbox_init);
