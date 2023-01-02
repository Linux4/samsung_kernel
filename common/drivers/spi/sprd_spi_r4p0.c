/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <mach/hardware.h>
#include <mach/dma.h>

#include "sprd_spi_r4p0.h"

#define ALIGN_UP(a, b)\
	(((a) + ((b) - 1)) & ~((b) - 1))

#define MHz(inte, dec) ((inte) * 1000 * 1000 + (dec) * 1000 * 100)

struct clk_src {
	u32 freq;
	const char *name;
};

static struct clk_src spi_src_tab[] = {
	{
		.freq = MHz(26, 0),
		.name = "ext_26m",
	},
	{
		.freq = MHz(96, 0),
		.name = "clk_96m",
	},
	{
		.freq = MHz(153, 6),
		.name = "clk_153m6",
	},
	{
		.freq = MHz(192, 0),
		.name = "clk_192m",
	},
};

struct sprd_spi_devdata {
	void __iomem *reg_base;
	int irq_num;
	struct clk *clk;
	spinlock_t lock;
	struct list_head msg_queue;
	struct work_struct work;
	struct workqueue_struct *work_queue;
	/* backup list
	 *0  clk_div
	 *1  ctl0
	 *2  ctl1
	 *3  ctl2
	 *4  ctl3
	 *5  ctl4
	 *6  ctl5
	 *7  int_en
	 *8  dsp_wait
	 *9  ctl6
	 *10 ctl7
	 */
	u32 reg_backup[11];
	bool is_active;
};

extern void clk_force_disable(struct clk *);

static void sprd_spi_dump_regs(u32 reg_base)
{
	printk("SPI_CLKD:0x%x \n", __raw_readl(reg_base + SPI_CLKD));
	printk("SPI_CTL0:0x%x \n", __raw_readl(reg_base + SPI_CTL0));
	printk("SPI_CTL1:0x%x \n", __raw_readl(reg_base + SPI_CTL1));
	printk("SPI_CTL2:0x%x \n", __raw_readl(reg_base + SPI_CTL2));
	printk("SPI_CTL3:0x%x \n", __raw_readl(reg_base + SPI_CTL3));
	printk("SPI_CTL4:0x%x \n", __raw_readl(reg_base + SPI_CTL4));
	printk("SPI_CTL5:0x%x \n", __raw_readl(reg_base + SPI_CTL5));
	printk("SPI_INT_EN:0x%x \n", __raw_readl(reg_base + SPI_INT_EN));
	printk("SPI_DSP_WAIT:0x%x \n", __raw_readl(reg_base + SPI_DSP_WAIT));
	printk("SPI_CTL6:0x%x \n", __raw_readl(reg_base + SPI_CTL6));
	printk("SPI_CTL7:0x%x \n", __raw_readl(reg_base + SPI_CTL7));
}

static void sprd_spi_wait_for_send_complete(u32 reg_base)
{
	u32 timeout = 0;
	while (!(__raw_readl(reg_base + SPI_STS2) & SPI_TX_FIFO_REALLY_EMPTY))
	{
		if (++timeout > SPI_TIME_OUT) {
			/*fixme, set timeout*/
			printk("spi send timeout!\n");
			sprd_spi_dump_regs(reg_base);
			BUG_ON(1);
		}
	}

	while (__raw_readl(reg_base + SPI_STS2) & SPI_TX_BUSY)
	{
		if (++timeout > SPI_TIME_OUT) {
			/*fixme, set timeout*/
			printk("spi send timeout!\n");
			sprd_spi_dump_regs(reg_base);
			BUG_ON(1);
		}
	}
}

enum spi_transfer_mode {
	tx_mode,
	rx_mode,
	rt_mode,
};

static int sprd_spi_transfer_full_duplex(struct spi_device *spi_dev,
	struct spi_transfer *trans_node)
{
	int i;
	u32 reg_val;
	u32 bits_per_word, block_num;
	u32 send_data_msk;
	u8 *tx_u8_p, *rx_u8_p;
	u16 *tx_u16_p, *rx_u16_p;
	u32 *tx_u32_p, *rx_u32_p;
	const void *src_buf;
	void *dst_buf;
	u32 transfer_mode;
	u32 reg_base;
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);
	reg_base = (u32)spi_chip->reg_base;
#if 0
	/* We config the bits_per_word in function spi_new_device()
	 * but you can reconfig thi option in spi_transfer
	 */
	if (unlikely(trans_node->bits_per_word != spi_dev->bits_per_word)) {
		printk("%s %d\n", __func__, __LINE__);
		reg_val = __raw_readl(spi_chip->reg_base + SPI_CTL0);
		reg_val &= ~(0x1f << 2);
		if (trans_node->bits_per_word != MAX_BITS_PER_WORD) {
			reg_val |= trans_node->bits_per_word << 2;
			printk("%s %d\n", __func__, __LINE__);
		}
		__raw_writel(reg_val, spi_chip->reg_base + SPI_CTL0);
	}
	/*fixme!the bits_per_word alaign up with 8*/
	bytes_per_word = ALIGN_UP(trans_node->bits_per_word, 8) >> 3;
#else
	bits_per_word = ALIGN_UP(spi_dev->bits_per_word, 8);

	reg_val = __raw_readl(reg_base + SPI_CTL0);
	reg_val &= ~(0x1f << 2);
	if (32 != bits_per_word) {
		reg_val |= bits_per_word << 2;
	}
	writel(reg_val, reg_base + SPI_CTL0);

#endif
	/*send data buf*/
	src_buf = trans_node->tx_buf;

	/*recv data buf*/
	dst_buf = trans_node->rx_buf;

	send_data_msk = 0xffffffff;

	if (src_buf && dst_buf) {
		transfer_mode = rt_mode;
	} else {
		if (dst_buf) {
			transfer_mode = rx_mode;
			/*in rx mode, we always send 0 to slave*/
			send_data_msk = 0x0;
			src_buf = trans_node->rx_buf;
		} else {
			transfer_mode = tx_mode;
			if (src_buf == NULL)
			    BUG_ON(1);
		}
	}

	reg_val = __raw_readl(reg_base + SPI_CTL1);

	reg_val &= ~(0x3 << 12);

	if (transfer_mode == tx_mode) {
		reg_val |= SPI_TX_MODE;
	} else {
		reg_val |= SPI_RX_MODE |SPI_TX_MODE;
	}

	writel(reg_val, reg_base + SPI_CTL1);

	/*reset the fifo*/
	writel(0x1, reg_base + SPI_FIFO_RST);
	writel(0x0, reg_base + SPI_FIFO_RST);

	/*alaway set cs pin to low level when transfer */
	if (spi_dev->chip_select < SPRD_SPI_CHIP_CS_NUM) {
		reg_val = __raw_readl(reg_base + SPI_CTL0);
		reg_val &= ~(0x1 << (spi_dev->chip_select + 8));
		writel(reg_val, reg_base + SPI_CTL0);
	} else {
		/*fixme, need to support gpio cs*/
	}

	switch (bits_per_word) {
	case 8:
		tx_u8_p = (u8 *)src_buf;
		rx_u8_p = (u8 *)dst_buf;
		block_num = trans_node->len;
		for (; block_num >= SPRD_SPI_FIFO_SIZE; block_num -= SPRD_SPI_FIFO_SIZE)
		{
			for (i = 0; i < SPRD_SPI_FIFO_SIZE; i++, tx_u8_p++)
			{
				__raw_writeb(*tx_u8_p & send_data_msk,
					reg_base + SPI_TXD);
			}

			sprd_spi_wait_for_send_complete(reg_base);

			if (transfer_mode == tx_mode)
				continue;

			for (i = 0; i < SPRD_SPI_FIFO_SIZE; i++, rx_u8_p++)
			{
				*rx_u8_p = __raw_readb(reg_base + SPI_TXD);
			}
		}

		for (i = 0; i < block_num; i++, tx_u8_p++)
		{
			__raw_writeb(*tx_u8_p & send_data_msk, reg_base + SPI_TXD);
		}

		sprd_spi_wait_for_send_complete(reg_base);

		if (transfer_mode == tx_mode)
			break;

		for (i = 0; i < block_num; i++, rx_u8_p++)
		{
			*rx_u8_p = __raw_readb(reg_base + SPI_TXD);
		}
		break;

	case 16:
		tx_u16_p = (u16 *)src_buf;
		rx_u16_p = (u16 *)dst_buf;
		block_num = trans_node->len >> 1;

		for (;block_num >= SPRD_SPI_FIFO_SIZE; block_num -= SPRD_SPI_FIFO_SIZE)
		{
			for (i =0; i < SPRD_SPI_FIFO_SIZE; i++, tx_u16_p++)
			{
				__raw_writew(*tx_u16_p & send_data_msk,
					reg_base + SPI_TXD);
			}

			sprd_spi_wait_for_send_complete(reg_base);

			if (transfer_mode == tx_mode)
				continue;

			for (i = 0; i < SPRD_SPI_FIFO_SIZE; i++, rx_u16_p++)
			{
				*rx_u16_p = __raw_readw(reg_base + SPI_TXD);
			}
		}

		for (i = 0; i < block_num; i++, tx_u16_p++)
		{
			__raw_writew(*tx_u16_p & send_data_msk, reg_base + SPI_TXD);
		}

		sprd_spi_wait_for_send_complete(reg_base);

		if (transfer_mode == tx_mode)
			break;

		for (i = 0; i < block_num; i++, rx_u16_p++)
		{
			*rx_u16_p = __raw_readw(reg_base + SPI_TXD);
		}
		break;

	case 32:
		tx_u32_p = (u32 *)src_buf;
		rx_u32_p = (u32 *)dst_buf;
		block_num = (trans_node->len + 3) >> 2;

		for (;block_num >= SPRD_SPI_FIFO_SIZE; block_num -= SPRD_SPI_FIFO_SIZE)
		{
			for (i = 0; i < SPRD_SPI_FIFO_SIZE; i++, tx_u32_p++)
			{
				__raw_writel(*tx_u32_p & send_data_msk,
					reg_base + SPI_TXD);
			}

			sprd_spi_wait_for_send_complete(reg_base);

			if (transfer_mode == tx_mode)
				continue;

			for (i = 0; i < SPRD_SPI_FIFO_SIZE; i++, rx_u32_p++)
			{
				*rx_u32_p = __raw_readl(reg_base + SPI_TXD);
			}
		}

		for (i = 0; i < block_num; i++, tx_u32_p++)
		{
			__raw_writel(*tx_u32_p & send_data_msk, reg_base + SPI_TXD);
		}

		sprd_spi_wait_for_send_complete(reg_base);

		if (transfer_mode == tx_mode)
			break;

		for (i = 0; i < block_num; i++, rx_u32_p++)
		{
			*rx_u32_p = __raw_readl(reg_base + SPI_TXD);
		}

		break;
	default:
		/*fixme*/
		break;
	}

	if (spi_dev->chip_select < SPRD_SPI_CHIP_CS_NUM) {
		reg_val = __raw_readl(reg_base + SPI_CTL0);
		reg_val |= 0xf << 8;
		__raw_writel(reg_val, reg_base + SPI_CTL0);
	} else {
		/*fixme, need to support gpio cs*/
	}

	reg_val = __raw_readl(reg_base + SPI_CTL1);
	reg_val &= ~(0x3 << 12);
	writel(reg_val, reg_base + SPI_CTL1);

	return trans_node->len;
}

static void  sprd_spi_transfer_work(struct work_struct *work)
{
	int ret;
	struct sprd_spi_devdata *spi_chip;
	struct spi_message *spi_msg;
	struct spi_transfer *transfer_node;
	unsigned long flags;

	spi_chip = container_of(work, struct sprd_spi_devdata, work);

	clk_enable(spi_chip->clk);

	/*fixme*/
	spin_lock_irqsave(&spi_chip->lock, flags);

	while (!list_empty(&spi_chip->msg_queue))
	{
		spi_msg = container_of(spi_chip->msg_queue.next,
			struct spi_message, queue);
		list_del_init(&spi_msg->queue);
		spin_unlock_irqrestore(&spi_chip->lock, flags);

		list_for_each_entry(transfer_node,
			&spi_msg->transfers, transfer_list)
		{
			if (transfer_node->tx_buf || transfer_node->rx_buf) {
				ret = sprd_spi_transfer_full_duplex(spi_msg->spi,
					transfer_node);
			}
		}

		if (spi_msg->complete) {
			spi_msg->status = 0x0;
			spi_msg->complete(spi_msg->context);
		}
		spin_lock_irqsave(&spi_chip->lock, flags);
	}

	clk_disable(spi_chip->clk);

	spin_unlock_irqrestore(&spi_chip->lock, flags);
}

static int sprd_spi_transfer(struct spi_device *spi_dev, struct spi_message *msg)
{
	struct sprd_spi_devdata *spi_chip;
	unsigned long flags;

	spi_chip = spi_master_get_devdata(spi_dev->master);

	spin_lock_irqsave(&spi_chip->lock, flags);

	list_add_tail(&msg->queue, &spi_chip->msg_queue);

	queue_work(spi_chip->work_queue, &spi_chip->work);

	spin_unlock_irqrestore(&spi_chip->lock, flags);

	return 0;
}

/*this function must be exectued in spi's clk is enable*/
static void sprd_spi_backup_config(struct sprd_spi_devdata *spi_chip)
{
	u32 reg_base;

	reg_base = (u32)spi_chip->reg_base;

	spi_chip->reg_backup[0] = __raw_readl(reg_base + SPI_CLKD);
	spi_chip->reg_backup[1] = __raw_readl(reg_base + SPI_CTL0);
	spi_chip->reg_backup[2] = __raw_readl(reg_base + SPI_CTL1);
	spi_chip->reg_backup[3] = __raw_readl(reg_base + SPI_CTL2);
	spi_chip->reg_backup[4] = __raw_readl(reg_base + SPI_CTL3);
	spi_chip->reg_backup[5] = __raw_readl(reg_base + SPI_CTL4);
	spi_chip->reg_backup[6] = __raw_readl(reg_base + SPI_CTL5);
	spi_chip->reg_backup[7] = __raw_readl(reg_base + SPI_INT_EN);
	spi_chip->reg_backup[8] = __raw_readl(reg_base + SPI_DSP_WAIT);
	spi_chip->reg_backup[9] = __raw_readl(reg_base + SPI_CTL6);
	spi_chip->reg_backup[10] = __raw_readl(reg_base + SPI_CTL7);
}

/*this function must be exectued in spi's clk is enable*/
static void sprd_spi_restore_config(const struct sprd_spi_devdata *spi_chip)
{
	u32 reg_base;

	reg_base = (u32)spi_chip->reg_base;

	__raw_writel(spi_chip->reg_backup[0], reg_base + SPI_CLKD);
	__raw_writel(spi_chip->reg_backup[1], reg_base + SPI_CTL0);
	__raw_writel(spi_chip->reg_backup[2], reg_base + SPI_CTL1);
	__raw_writel(spi_chip->reg_backup[3], reg_base + SPI_CTL2);
	__raw_writel(spi_chip->reg_backup[4], reg_base + SPI_CTL3);
	__raw_writel(spi_chip->reg_backup[5], reg_base + SPI_CTL4);
	__raw_writel(spi_chip->reg_backup[6], reg_base + SPI_CTL5);
	__raw_writel(spi_chip->reg_backup[7], reg_base + SPI_INT_EN);
	__raw_writel(spi_chip->reg_backup[8], reg_base + SPI_DSP_WAIT);
	__raw_writel(spi_chip->reg_backup[9], reg_base + SPI_CTL6);
	__raw_writel(spi_chip->reg_backup[10], reg_base + SPI_CTL7);
}

static int sprd_spi_setup(struct spi_device *spi_dev)
{
	int i;
	u32 reg_val;
	u32 spi_work_clk, spi_src_clk, spi_clk_div;
	const char *src_clk_name;
	struct clk *clk_parent;
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);

	/*fixme, need to check the parmeter*/

	/*if the spi_src_clk is 48MHz,the spi max working clk is 24MHz*/
	spi_work_clk = spi_dev->max_speed_hz << 1;
	if (spi_work_clk > MHz(192, 0))
		return -EINVAL;
	for(i = 0; i < ARRAY_SIZE(spi_src_tab); i++)  {
		if(!(spi_src_tab[i].freq % spi_work_clk))
			break;
	}
	if(i == ARRAY_SIZE(spi_src_tab))
		i--;
	src_clk_name = spi_src_tab[i].name;
	spi_src_clk = spi_src_tab[i].freq;

	clk_parent = clk_get(&(spi_dev->master->dev), src_clk_name);
	if (IS_ERR(clk_parent)) {
		printk("Can't get the clock source: %s\n", src_clk_name);
		return -EINVAL;
	}

	clk_set_parent(spi_chip->clk, clk_parent);

	/*global enable*/
	clk_enable(spi_chip->clk);

	/*spi config*/
	if (spi_dev->bits_per_word > MAX_BITS_PER_WORD) {
		/*fixme*/
	}

	/*the default bits_per_word is 8*/
	if (spi_dev->bits_per_word == MAX_BITS_PER_WORD) {
		reg_val = 0x0 << 2;
	} else {
		reg_val = spi_dev->bits_per_word << 2;
	}

	/*set all cs to high level*/
	reg_val |= 0xf << 8;

	/*spi mode config*/
	switch (spi_dev->mode & 0x3) {
	case SPI_MODE_0:
		reg_val |= BIT(1);
		break;
	case SPI_MODE_1:
		reg_val |= BIT(0);
		break;
	case SPI_MODE_2:
		reg_val |= BIT(13) | BIT(1);
		break;
	case SPI_MODE_3:
		reg_val |= BIT(13) | BIT(0);
		break;
	default:
		/*fixme*/
		break;
	}
	__raw_writel(reg_val, spi_chip->reg_base + SPI_CTL0);
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL1);
	/*spi master mode*/
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL2);
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL4);
	__raw_writel(0x9, spi_chip->reg_base + SPI_CTL5);
	__raw_writel(0x0, spi_chip->reg_base + SPI_INT_EN);
	/*reset fifo*/
	__raw_writel(0x1, spi_chip->reg_base + SPI_FIFO_RST);
	for (i = 0; i < 0x20; i++);
	__raw_writel(0x0, spi_chip->reg_base + SPI_FIFO_RST);

	clk_set_rate(spi_chip->clk, spi_src_clk);

	spi_clk_div = spi_src_clk / (spi_dev->max_speed_hz << 1) - 1;

	__raw_writel(spi_clk_div, spi_chip->reg_base + SPI_CLKD);

	/*wait the clk config become effective*/
	msleep(5);

	sprd_spi_backup_config(spi_chip);
	spi_chip->is_active = true;

	/*disable the clk after config complete*/
	clk_disable(spi_chip->clk);

	return 0;
}

static void sprd_spi_cleanup(struct spi_device *spi)
{
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi->master);

	clk_force_disable(spi_chip->clk);
}

static int sprd_spi_probe(struct platform_device *pdev)
{
	int ret;
	int irq_num;
	struct resource *regs;
	struct spi_master *master;
	struct sprd_spi_devdata *spi_chip;
	int rc = -ENXIO;

#ifdef CONFIG_OF
	pdev->id = of_alias_get_id(pdev->dev.of_node, "spi");
#endif
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "spi_%d get io resource failed!\n", pdev->id);
		return -ENODEV;
	}
	dev_info(&pdev->dev, "probe spi %d get regbase %p\n", pdev->id, regs->start);

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(&pdev->dev, "spi_%d get irq resource failed!\n", pdev->id);
		return irq_num;
	}
	dev_info(&pdev->dev, "probe spi %d get irq num %d\n", pdev->id, irq_num);

	master = spi_alloc_master(&pdev->dev, sizeof (*spi_chip));
	if (!master) {
		dev_err(&pdev->dev, "spi_%d spi_alloc_master failed!\n", pdev->id);
		return -ENOMEM;
	}

	if (pdev->dev.of_node)
		master->dev.of_node = pdev->dev.of_node;

	spi_chip = spi_master_get_devdata(master);

	/*get clk source*/
	switch (pdev->id) {
		case sprd_spi_0:
			spi_chip->clk = clk_get(NULL, "clk_spi0");
			break;
		case sprd_spi_1:
			spi_chip->clk = clk_get(NULL, "clk_spi1");
			break;
		case sprd_spi_2:
			spi_chip->clk = clk_get(NULL, "clk_spi2");
			break;
		default:
			ret = -EINVAL;
			goto err_exit;
	}
	if (IS_ERR(spi_chip->clk)) {
		ret = -ENXIO;
		goto err_exit;
	}

	rc = clk_prepare_enable(spi_chip->clk);
	if (rc) {
		dev_err(&pdev->dev, "[SPRD_SPI] unable to enable core_clk\n");
	}

	spi_chip->reg_base = (void __iomem *)regs->start;
	spi_chip->irq_num  = irq_num;

	master->mode_bits = SPI_CPOL | SPI_CPHA;
	master->bus_num = pdev->id;
	master->num_chipselect = SPRD_SPI_CHIP_CS_NUM;

	master->setup = sprd_spi_setup;
#ifdef SPI_NEW_INTERFACE
	master->prepare_transfer_hardware = xxxxx;
	master->transfer_one_message = xxxx;
	master->unprepare_transfer_hardware = xxxx;
#else
	master->transfer = sprd_spi_transfer;
	master->cleanup = sprd_spi_cleanup;
#endif

	INIT_LIST_HEAD(&spi_chip->msg_queue);
	spin_lock_init(&spi_chip->lock);
	/*on multi core system, use the create_workqueue() is better??*/
	spi_chip->work_queue = create_singlethread_workqueue(pdev->name);
//	spi_chip->work_queue = create_workqueue(pdev->name);
	INIT_WORK(&spi_chip->work, sprd_spi_transfer_work);

	platform_set_drvdata(pdev, master);

	ret = spi_register_master(master);
	if (ret) {
		printk("register spi master %d failed!\n", master->bus_num);
		goto err_exit;
	}

	return 0;

err_exit:
	spi_master_put(master);
	kfree(master);

	return ret;
}

static int __exit sprd_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct sprd_spi_devdata *spi_chip = spi_master_get_devdata(master);

	destroy_workqueue(spi_chip->work_queue);

	clk_force_disable(spi_chip->clk);

	spi_unregister_master(master);

	spi_master_put(master);

	return 0;
}

#ifdef CONFIG_PM
static int sprd_spi_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct spi_master *master = platform_get_drvdata(pdev);

	struct sprd_spi_devdata *spi_chip = spi_master_get_devdata(master);
	if (IS_ERR(spi_chip->clk)) {
		pr_err("can't get spi_clk when suspend()\n");
		return -1;
	}

	if (spi_chip->is_active)
		clk_force_disable(spi_chip->clk);

	return 0;
}

static int sprd_spi_resume(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);

	struct sprd_spi_devdata *spi_chip = spi_master_get_devdata(master);

	if (spi_chip->is_active) {

		clk_enable(spi_chip->clk);

		sprd_spi_restore_config(spi_chip);

		clk_force_disable(spi_chip->clk);
	}

	return 0;
}

#else
#define	sprd_spi_suspend NULL
#define	sprd_spi_resume NULL
#endif

static const struct of_device_id sprd_spi_of_match[] = {
	{ .compatible = "sprd,sprd-spi", },
	{ /* sentinel */ }
};

static struct platform_driver sprd_spi_driver = {
	.driver = {
		.name = "sprd spi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_spi_of_match),
	},
	.suspend = sprd_spi_suspend,
	.resume  = sprd_spi_resume,
	.probe = sprd_spi_probe,
	.remove  = __exit_p(sprd_spi_remove),
};

module_platform_driver(sprd_spi_driver);

MODULE_DESCRIPTION("SpreadTrum SPI(r4p0 version) Controller driver");
MODULE_AUTHOR("Jack.Jiang <Jack.Jiang@spreadtrum.com>");
MODULE_LICENSE("GPL");
