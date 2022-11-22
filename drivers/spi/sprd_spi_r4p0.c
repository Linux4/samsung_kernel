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

#include <soc/sprd/dma_r4p0.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>

#include <linux/spi/sprd-spi.h>
#include "sprd_spi_r4p0.h"

unsigned long REGS_SPI0_BASE = 0;
unsigned long REGS_SPI1_BASE = 0;
unsigned long REGS_SPI2_BASE = 0;
#define SPRD_SPI0_BASE                  SCI_IOMAP(0x376000)
#define SPRD_SPI0_PHYS                  0X70A00000
#define SPRD_SPI0_SIZE                  SZ_4K

#define SPRD_SPI1_BASE                  SCI_IOMAP(0x378000)
#define SPRD_SPI1_PHYS                  0X70B00000
#define SPRD_SPI1_SIZE                  SZ_4K

#define SPRD_SPI2_BASE                  SCI_IOMAP(0x37a000)
#define SPRD_SPI2_PHYS                  0X70C00000
#define SPRD_SPI2_SIZE                  SZ_4K

#define SPI_BASE(id) \
		((id == 0) ? REGS_SPI0_BASE : ((id == 1) ? REGS_SPI1_BASE : REGS_SPI2_BASE ))

#define ALIGN_UP(a, b)\
	(((a) + ((b) - 1)) & ~((b) - 1))

#define MHz(inte, dec) ((inte) * 1000 * 1000 + (dec) * 1000 * 100)

#define SPI_DMA_BUF_SIZE 	4096*128
#define SPI_MAX_FREQ 		24000000


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
struct sprd_spi_dev_info test_spi = {
	.enable_dma = 1,
};
struct sprd_spi_devdata {
	void __iomem *reg_base;
	unsigned long *reg_base_p;
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
	u32 spi_dev_id;
	u32 dma_tx_dev_id;
	u32 dma_rx_dev_id;
	u32 dma_rx_chn;
	u32 dma_tx_chn;
	u32 dma_already_tx_len;
	u32 dma_already_rx_len;
	u32 trans_node_len;
	void *dma_buf_v_rx;
	dma_addr_t dma_buf_rx_p;
	void *dma_buf_v_tx;
	dma_addr_t dma_buf_tx_p;
	struct completion	done;
	enum spi_dma_data dma_complete;
	struct spi_transfer *transfer_node;
	struct sprd_spi_dev_info *dev_info;
};
enum spi_transfer_mode {
	tx_mode,
	rx_mode,
	rt_mode,
};

static int sprd_spi_transfer_dma_mode(struct spi_device *spi_dev, struct spi_transfer *trans_node);
static int sprd_spi_tx_dma_config(struct spi_device *spi_dev, struct spi_transfer *trans_node);
static int sprd_spi_rx_dma_config(struct spi_device *spi_dev, struct spi_transfer *trans_node);

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
			pr_err("spi send timeout!\n");
			sprd_spi_dump_regs(reg_base);
			BUG_ON(1);
		}
	}

	while (__raw_readl(reg_base + SPI_STS2) & SPI_TX_BUSY)
	{
		if (++timeout > SPI_TIME_OUT) {
			/*fixme, set timeout*/
			pr_err("spi send timeout!\n");
			sprd_spi_dump_regs(reg_base);
			BUG_ON(1);
		}
	}
}
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
		pr_debug("%s %d\n", __func__, __LINE__);
		reg_val = __raw_readl(spi_chip->reg_base + SPI_CTL0);
		reg_val &= ~(0x1f << 2);
		if (trans_node->bits_per_word != MAX_BITS_PER_WORD) {
			reg_val |= trans_node->bits_per_word << 2;
			pr_debug("%s %d\n", __func__, __LINE__);
		}
		__raw_writel(reg_val, spi_chip->reg_base + SPI_CTL0);
	}
	/*fixme!the bits_per_word alaign up with 8*/
	bytes_per_word = ALIGN_UP(trans_node->bits_per_word, 8) >> 3;
#else
	bits_per_word = ALIGN_UP(trans_node->bits_per_word, 8);

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

static int sprd_spi_transfer_config(struct spi_device *spi_dev,struct spi_transfer *trans_node)
{
	u32 reg_base;
	u32 bits_per_word;
	u32 reg_val;

	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);
	reg_base = (u32)spi_chip->reg_base;
	bits_per_word = ALIGN_UP(trans_node->bits_per_word, 8);
	reg_val = __raw_readl(reg_base + SPI_CTL0);
	reg_val &= ~(0x1f << 2);
	
	if (SPRD_MAX_DATA_PRE != bits_per_word) {
		reg_val |= bits_per_word << 2;
	}
	writel(reg_val, reg_base + SPI_CTL0);

	reg_val = __raw_readl(reg_base + SPI_CTL1);
	reg_val &= ~(0x3 << 12);
	pr_debug("sprd_spi_transfer_config trans_node->rx_buf =%x\n",trans_node->rx_buf);
	if (!(trans_node->rx_buf)) {
		reg_val |= SPI_TX_MODE;
	}
	else if (!(trans_node->tx_buf)) {
		reg_val |= SPI_RX_MODE;
	}
	else {
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
	return 0;
}

static int sprd_spi_enter_idle(struct spi_device *spi_dev)
{
	u32 reg_base;
	u32 bits_per_word;
	u32 reg_val;

	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);
	reg_base = (u32)spi_chip->reg_base;

	reg_val = __raw_readl(reg_base + SPI_CTL0);
	reg_val |= 0xf << 8;
	__raw_writel(reg_val, reg_base + SPI_CTL0);

	reg_val = __raw_readl(reg_base + SPI_CTL1);
	reg_val &= ~(0x3 << 12);
	writel(reg_val, reg_base + SPI_CTL1);
	return 0;
}

static int sprd_spi_rx_buf(struct spi_device *spi_dev, struct spi_transfer *trans_node)
{
	int ret = 0;
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);

	memcpy((void*)(trans_node->rx_buf),(void *)(spi_chip->dma_buf_v_rx),trans_node->len);

	return ret;
}

static int sprd_spi_tx_buf(struct spi_device *spi_dev, struct spi_transfer *trans_node)
{
	int ret = 0;
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);

	memcpy((void *)(spi_chip->dma_buf_v_tx),(void*)(trans_node->tx_buf),trans_node->len);

	return ret;
}

static int sprd_spi_free_dma_tx_buf(struct sprd_spi_devdata *spi_chip,struct spi_transfer *trans_node)
{
	struct sprd_spi_dev_info *dev_info_p;

	dev_info_p = spi_chip->dev_info;

	if(dev_info_p&&dev_info_p->enable_dma){
		 dma_free_coherent(NULL,
						   trans_node->len*4,
				  		   spi_chip->dma_buf_v_tx,
				  		   spi_chip->dma_buf_tx_p );
	}

	return 0;
}

static int sprd_spi_free_dma_rx_buf(struct sprd_spi_devdata *spi_chip,struct spi_transfer *trans_node)
{
	struct sprd_spi_dev_info *dev_info_p;

	dev_info_p = spi_chip->dev_info;

	if(dev_info_p&&dev_info_p->enable_dma){
		 dma_free_coherent(NULL,
						   trans_node->len*4,
				  		   spi_chip->dma_buf_v_rx,
				  		   spi_chip->dma_buf_rx_p );
	}

	return 0;
}


static int sprd_spi_alloc_dma_tx_buf(struct sprd_spi_devdata *spi_chip,struct spi_transfer *trans_node)
{
	struct sprd_spi_dev_info *dev_info_p;

	dev_info_p = spi_chip->dev_info;

	if(dev_info_p&&dev_info_p->enable_dma){
	    spi_chip->dma_buf_v_tx =
		 dma_alloc_coherent(NULL,
						    trans_node->len*4,
				  		    &spi_chip->dma_buf_tx_p,
				  		    GFP_KERNEL);
		if (!spi_chip->dma_buf_v_tx) {
			return -ENOMEM;
		}
	}

	return 0;
}

 static int sprd_spi_alloc_dma_rx_buf(struct sprd_spi_devdata *spi_chip,struct spi_transfer *trans_node)
{
	struct sprd_spi_dev_info *dev_info_p;

	dev_info_p = spi_chip->dev_info;

	if(dev_info_p&&dev_info_p->enable_dma){
	    spi_chip->dma_buf_v_rx =
		 dma_alloc_coherent(NULL,
						    trans_node->len*4,
				  		    &spi_chip->dma_buf_rx_p,
				  		    GFP_KERNEL);
		if (!spi_chip->dma_buf_v_rx) {
			return -ENOMEM;
		}
	}

	return 0;
}

static int sprd_spi_alloc_dma_buf(struct sprd_spi_devdata *spi_chip, unsigned len)
{
	struct sprd_spi_dev_info *dev_info_p;

	dev_info_p = spi_chip->dev_info;

	if(dev_info_p&&dev_info_p->enable_dma){
	    spi_chip->dma_buf_v_rx =
		 dma_alloc_coherent(NULL,
						    len,
				  		    &spi_chip->dma_buf_rx_p,
				  		    GFP_KERNEL);
		if (!spi_chip->dma_buf_v_rx) {
			return -ENOMEM;
		}
	}

	if(dev_info_p&&dev_info_p->enable_dma){
	    spi_chip->dma_buf_v_tx =
		 dma_alloc_coherent(NULL,
						    len,
				  		    &spi_chip->dma_buf_tx_p,
				  		    GFP_KERNEL);
		if (!spi_chip->dma_buf_v_tx) {
			return -ENOMEM;
		}
	}

	return 0;
}

static int sprd_spi_dma_tx_chn_req(struct sprd_spi_devdata *spi_chip)
{
	struct sprd_spi_dev_info *dev_info_p;

	dev_info_p = spi_chip->dev_info;

	if (spi_chip && dev_info_p->enable_dma && spi_chip->dma_tx_dev_id) {
		if (spi_chip->dma_tx_chn == 0) {
			spi_chip->dma_tx_chn =sci_dma_request("spi", FULL_DMA_CHN);
			pr_debug("spi%d tx alloc dma chn %d,spi_chip->dma_tx_chn =%d\n", 
				spi_chip->spi_dev_id,spi_chip->dma_tx_dev_id,spi_chip->dma_tx_chn);
			pr_debug("spi%d tx the dma buf addr is %x\n", spi_chip->spi_dev_id,spi_chip->dma_buf_tx_p);
		} else {
			/*rset the dma chn */
			sci_dma_stop(spi_chip->dma_tx_chn, spi_chip->dma_tx_dev_id);
			pr_debug("spi%d tx alloc dma chn is already use\n", spi_chip->spi_dev_id);
		}
	}
	else{
		pr_debug("spi%d sprd_spi_dma_tx_chn_req %d\n", spi_chip->spi_dev_id,spi_chip->dma_tx_dev_id);
		return -1;
	}
	return 0;
}

static int sprd_spi_dma_rx_chn_req(struct sprd_spi_devdata *spi_chip)
{
	struct sprd_spi_dev_info *dev_info_p;

        if(spi_chip == NULL){
          //  pr_debug("sprd_spi_dma_rx_chn_req: spi_chip is NULL!\n");
            return -1;
        }

	dev_info_p = spi_chip->dev_info;
	if (spi_chip && dev_info_p->enable_dma && spi_chip->dma_rx_dev_id) {
		if (spi_chip->dma_rx_chn == 0) {
			spi_chip->dma_rx_chn =sci_dma_request("spi", FULL_DMA_CHN);
			pr_debug("spi%d rx alloc dma chn %d,spi_chip->dma_rx_chn=%d \n",
				spi_chip->spi_dev_id,spi_chip->dma_rx_dev_id,spi_chip->dma_rx_chn);
			pr_debug("spi%d rx the dma buf addr is %x\n", spi_chip->spi_dev_id,spi_chip->dma_buf_rx_p);
		} else {
			/*rset the dma chn */
			sci_dma_stop(spi_chip->dma_rx_chn, spi_chip->dma_rx_dev_id);
			pr_debug("spi%d rx alloc dma chn is already use\n", spi_chip->spi_dev_id);
		}
	}
	else{
		pr_debug("spi%d sprd_spi_dma_rx_chn_req %d\n", spi_chip->spi_dev_id,spi_chip->dma_tx_dev_id);
		return -1;
	}
	return 0;
}

static void sprd_spi_dma_mode_enable(struct sprd_spi_devdata *spi_chip)
{
	unsigned int temp_reg = 0;
	pr_debug("sprd_spi_dma_mode_enable\n");
	temp_reg  = __raw_readl(spi_chip->reg_base+ SPI_CTL2);
	temp_reg |=SPI_DMA_EN;
	__raw_writel(temp_reg, spi_chip->reg_base+ SPI_CTL2);
}

static void sprd_spi_dma_mode_disable(struct sprd_spi_devdata *spi_chip)
{
	unsigned int temp_reg = 0;
	pr_debug("sprd_spi_dma_mode_disable\n");
	temp_reg  = __raw_readl(spi_chip->reg_base+ SPI_CTL2);
	temp_reg &=(~SPI_DMA_EN);
	__raw_writel(temp_reg, spi_chip->reg_base+ SPI_CTL2);
}

static void sprd_spi_dma_receive_enable(struct sprd_spi_devdata *spi_chip)
{
	unsigned int temp_reg = 0;
	pr_debug("sprd_spi_dma_mode_disable\n");
	temp_reg  = __raw_readl(spi_chip->reg_base+ SPI_CTL4);
	temp_reg |= SPI_START_RX;
	__raw_writel(temp_reg, spi_chip->reg_base+ SPI_CTL4);
}

static void sprd_spi_dma_rx_irqhandler(int dma_chn, void *data)
{
	struct sprd_spi_devdata *spi_chip;
	struct spi_device *spi_dev;
		pr_debug("sprd_spi_dma_rx_irqhandler\n");
	spi_dev = (struct spi_device *)data;
	spi_chip = spi_master_get_devdata(spi_dev->master);

	pr_debug("sprd_spi_dma_rx_irqhandler spi_chip->dma_already_rx_len =%d\n",spi_chip->dma_already_rx_len);

	//sci_dma_stop(spi_chip->dma_rx_chn, spi_chip->dma_rx_dev_id);
	if((spi_chip->dma_already_rx_len)==spi_chip->trans_node_len){
		sprd_spi_rx_buf(spi_dev,spi_chip->transfer_node);
		spi_chip->dma_complete |= dma_receive_success;
		spi_chip->dma_already_rx_len = 0;
		sprd_spi_dma_mode_disable(spi_chip);
		pr_debug("sprd_spi_dma_rx_irqhandler complete \n");
		sprd_spi_enter_idle(spi_dev);
		complete(&spi_chip->done);
	}
	else{
		pr_debug("sprd_spi_dma_rx_irqhandler double\n");
		sprd_spi_rx_dma_config(spi_dev,spi_chip->transfer_node);
	}
}

static void sprd_spi_dma_tx_irqhandler(int dma_chn, void *data)
{
	struct sprd_spi_devdata *spi_chip;
	struct spi_device *spi_dev;
	struct spi_transfer *transfer_node;

	spi_dev = (struct spi_device *)data;
	spi_chip = spi_master_get_devdata(spi_dev->master);
	transfer_node = spi_chip->transfer_node;

	pr_debug("SPI_STS6:tx end 0x%x \n", __raw_readl(spi_chip->reg_base + SPI_STS6));

	spi_chip->dma_complete |= dma_transfer_success;

	if(!(transfer_node->rx_buf)){
		sprd_spi_dma_mode_disable(spi_chip);
		pr_debug("sprd_spi_dma_tx_irqhandler SPI_STS6:0x%x \n",transfer_node->rx_buf);
		sprd_spi_enter_idle(spi_dev);
		complete(&spi_chip->done);
	}
}

static int sprd_spi_tx_dma_config(struct spi_device *spi_dev, struct spi_transfer *trans_node)
{
	int ret = 0;
	u32 fragmens_len = 0;
	u32 bits_per_word = 0;
	struct sprd_spi_devdata *spi_chip;
	struct sci_dma_cfg tx_dma_cfg;	// , tx_dma_cfg;

	spi_chip = spi_master_get_devdata(spi_dev->master);

	pr_debug("spi%dtx trans_node->len=%d \n",spi_chip->spi_dev_id,trans_node->len);
	memset(&tx_dma_cfg, 0x0, sizeof(tx_dma_cfg));

	bits_per_word = ALIGN_UP(trans_node->bits_per_word, 8);
	if(bits_per_word == 8){
		tx_dma_cfg.datawidth = BYTE_WIDTH;
		tx_dma_cfg.src_step = 1;
		fragmens_len = SPRD_SPI_FIFO_RX_FULL;
	}
	else if(bits_per_word ==16){
		tx_dma_cfg.datawidth = SHORT_WIDTH;
		tx_dma_cfg.src_step = 2;
		fragmens_len = SPRD_SPI_FIFO_RX_FULL << 1;
	}
	else if(bits_per_word ==32){
		tx_dma_cfg.datawidth = WORD_WIDTH;
		tx_dma_cfg.src_step = 4;
		fragmens_len = SPRD_SPI_FIFO_RX_FULL << 2;
		tx_dma_cfg.swt_mode = DCBA;
	}

	tx_dma_cfg.src_addr = spi_chip->dma_buf_tx_p;
	tx_dma_cfg.des_addr = (unsigned long )(spi_chip->reg_base_p + SPI_TXD);
	tx_dma_cfg.des_step = 0x0;
	tx_dma_cfg.fragmens_len = fragmens_len;
	tx_dma_cfg.block_len = fragmens_len;
	tx_dma_cfg.transcation_len = trans_node->len;
	tx_dma_cfg.req_mode = FRAG_REQ_MODE;

	ret = sci_dma_config(spi_chip->dma_tx_chn, &tx_dma_cfg, 1, NULL);
	if (ret)
		pr_info("%s error: sci_dma_config returns %d\n", __func__, ret);

	ret = sci_dma_register_irqhandle(spi_chip->dma_tx_chn, TRANS_DONE,
				       sprd_spi_dma_tx_irqhandler,
				       spi_dev);
	if (ret)
		pr_info("%s error: sci_dma_register_irqhandle returns %d\n", __func__, ret);

	sci_dma_start(spi_chip->dma_tx_chn, spi_chip->dma_tx_dev_id);
	return ret;
}

static int sprd_spi_rx_dma_config(struct spi_device *spi_dev, struct spi_transfer *trans_node)
{
	int ret = 0;
	u32 per_trans_len = 0;
	u32 last_data_len = 0;
	u32 fragmens_len = 0;
	u32 bits_per_word = 0;
	unsigned int temp_fifo_full_rx= 0;
	u32 datawidth = 0;

	struct sprd_spi_devdata *spi_chip;
	struct sci_dma_cfg rx_dma_cfg;
	spi_chip = spi_master_get_devdata(spi_dev->master);

//	sprd_spi_dump_regs(spi_chip->reg_base);

	memset(&rx_dma_cfg, 0x0, sizeof(rx_dma_cfg));

	bits_per_word = ALIGN_UP(trans_node->bits_per_word, 8);
	pr_debug("sprd_spi_rx_dma_config rx spi bits_per_word = %d spi_dev->bits_per_word =%d\n",
		bits_per_word,spi_dev->bits_per_word);

	if(bits_per_word == 8){
		rx_dma_cfg.datawidth = BYTE_WIDTH;
		rx_dma_cfg.des_step = 1;
	}
	else if(bits_per_word ==16){
		rx_dma_cfg.datawidth = SHORT_WIDTH;
		rx_dma_cfg.des_step = 2;
	}
	else if(bits_per_word ==32){
		rx_dma_cfg.datawidth = WORD_WIDTH;
		rx_dma_cfg.des_step = 4;
		rx_dma_cfg.swt_mode = DCBA;
	}

	datawidth =  rx_dma_cfg.des_step>>1;

	last_data_len =trans_node->len - spi_chip->dma_already_rx_len;
	per_trans_len	= ((last_data_len >> datawidth)>=SPRD_SPI_FIFO_RX_FULL)?
		(last_data_len -(last_data_len %(SPRD_SPI_FIFO_RX_FULL <<datawidth))):last_data_len;
	spi_chip->dma_already_rx_len+=per_trans_len;
	fragmens_len = ((per_trans_len >> datawidth) <= SPRD_SPI_FIFO_RX_FULL )?
		per_trans_len : (SPRD_SPI_FIFO_RX_FULL << datawidth);

	pr_debug("spi%drx trans_node->len=%d ,last_data_len =%d,per_trans_len=%d,fragmens_len=%d\n",
		spi_chip->spi_dev_id,trans_node->len,last_data_len,per_trans_len,fragmens_len);

	temp_fifo_full_rx = __raw_readl(spi_chip->reg_base + SPI_CTL3);
	temp_fifo_full_rx = (temp_fifo_full_rx&SPRD_RX_FIFO_FULL_MASK)|(fragmens_len >> datawidth);

	__raw_writel(temp_fifo_full_rx, spi_chip->reg_base + SPI_CTL3);
	pr_debug("spi%d temp_fifo_full_rx=%x\n", spi_chip->spi_dev_id,temp_fifo_full_rx);

	rx_dma_cfg.src_addr = (unsigned long )(spi_chip->reg_base_p + SPI_TXD);
	rx_dma_cfg.des_addr = spi_chip->dma_buf_rx_p;
	rx_dma_cfg.src_step = 0x0;
	rx_dma_cfg.fragmens_len = fragmens_len;
	rx_dma_cfg.block_len = fragmens_len;
	rx_dma_cfg.transcation_len = per_trans_len;
	rx_dma_cfg.req_mode = FRAG_REQ_MODE;
	//rx_dma_cfg.pri_level = DMA_PRI_1;

	ret = sci_dma_config(spi_chip->dma_rx_chn, &rx_dma_cfg, 1, NULL);
	if (ret)
		pr_info("%s error: sci_dma_config returns %d\n", __func__, ret);

	ret = sci_dma_register_irqhandle(spi_chip->dma_rx_chn, TRANS_DONE,
				       sprd_spi_dma_rx_irqhandler,
				       spi_dev);
	if (ret)
		pr_info("%s error: sci_dma_register_irqhandle returns %d\n", __func__, ret);

	sci_dma_start(spi_chip->dma_rx_chn, spi_chip->dma_rx_dev_id);
	return ret;
}

static int sprd_spi_transfer_dma_mode(struct spi_device *spi_dev, struct spi_transfer *trans_node)
{
	int ret = 0;
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi_dev->master);
	spi_chip->trans_node_len = trans_node->len;

	/*config spi ip status*/
	sprd_spi_transfer_config(spi_dev,trans_node);
	pr_debug("sprd_spi_transfer_dma_mode-------- rx_buf:0x%08x\n",trans_node->rx_buf);
	/*move the data to dma buf */
	if(!(trans_node->rx_buf)){
		sprd_spi_tx_buf(spi_dev,trans_node);
		ret= sprd_spi_tx_dma_config(spi_dev,trans_node);
		if(ret < 0){
			pr_err("sprd_spi_tx_dma_config fail %d\n",ret);
			return ret;
		}
		sprd_spi_dma_mode_enable(spi_chip);
	}
	else{
		pr_debug("sprd_spi_transfer_dma_mode-------- tx&rx tx_buf:0x%08x\n", trans_node->tx_buf);
		if(trans_node->tx_buf){
			ret = sprd_spi_tx_buf(spi_dev,trans_node);
		}
		ret= sprd_spi_rx_dma_config(spi_dev,trans_node);
		if(ret < 0){
			pr_err("sprd_spi_rx_dma_config fail %d\n",ret);
			return ret;
		}

		if (trans_node->tx_buf) {
			ret= sprd_spi_tx_dma_config(spi_dev,trans_node);
			if(ret < 0){
				pr_err("sprd_spi_tx_dma_config fail %d\n",ret);
				return ret;
			}
		}

		sprd_spi_dma_mode_enable(spi_chip);
		if (!(trans_node->tx_buf)) {
			sprd_spi_dma_receive_enable(spi_chip);
		}
	}
		/* Wait for the transfer to complete */
		wait_for_completion_interruptible(&(spi_chip->done));
		//sprd_spi_enter_idle(spi_dev,trans_node);
		pr_debug("sprd_spi wait_for_completion_interruptible \n");

		//mdelay(1);

	return ret;
}

static void  sprd_spi_transfer_work(struct work_struct *work)
{
	int ret;
	struct sprd_spi_devdata *spi_chip;
	struct spi_message *spi_msg;
	struct spi_transfer *transfer_node;
	unsigned long flags;
	struct sprd_spi_dev_info *dev_info_p;

	spi_chip = container_of(work, struct sprd_spi_devdata, work);
	dev_info_p = spi_chip->dev_info;
	if(dev_info_p&&dev_info_p->enable_dma){
		spi_chip->dma_complete = dma_spi_idle;
		INIT_COMPLETION(spi_chip->done);
	}
	clk_prepare_enable(spi_chip->clk);

	/*fixme*/
	spin_lock_irqsave(&spi_chip->lock, flags);

	while (!list_empty(&spi_chip->msg_queue))
	{
		spi_msg = container_of(spi_chip->msg_queue.next,struct spi_message, queue);
		list_del_init(&spi_msg->queue);
		spin_unlock_irqrestore(&spi_chip->lock, flags);

		list_for_each_entry(transfer_node,&spi_msg->transfers, transfer_list)
		{
			if (transfer_node->tx_buf || transfer_node->rx_buf) {
				spi_chip->transfer_node= transfer_node;
				if(dev_info_p&&dev_info_p->enable_dma){
					ret = sprd_spi_transfer_dma_mode(spi_msg->spi,transfer_node);
				}
				else{
					ret = sprd_spi_transfer_full_duplex(spi_msg->spi,transfer_node);
				}
			}
		}

		if (spi_msg->complete) {
			spi_msg->status = 0x0;
			spi_msg->complete(spi_msg->context);
		}
		spin_lock_irqsave(&spi_chip->lock, flags);
	}

	//clk_disable_unprepare(spi_chip->clk);

	spin_unlock_irqrestore(&spi_chip->lock, flags);
	
	clk_disable_unprepare(spi_chip->clk);
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
	u32 dma_ret = 0;
	u32 reg_val;
	u32 spi_work_clk, spi_src_clk, spi_clk_div;
	const char *src_clk_name;
	struct clk *clk_parent;
	struct sprd_spi_devdata *spi_chip;
	struct sprd_spi_dev_info *dev_info_p;

	spi_chip = spi_master_get_devdata(spi_dev->master);
	spi_chip->dev_info = (struct sprd_spi_dev_info *)(spi_dev->controller_data);
	spi_chip->dev_info =&test_spi;
	dev_info_p = spi_chip->dev_info;
	/*fixme, need to check the parmeter*/
	pr_debug("sprd_spi_setup max_speed_hz=%d mode=%d\n",spi_dev->max_speed_hz,spi_dev->mode);
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
		pr_err("Can't get the clock source: %s\n", src_clk_name);
		return -EINVAL;
	}

	clk_set_parent(spi_chip->clk, clk_parent);

	/*global enable*/
	clk_prepare_enable(spi_chip->clk);

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
	if (spi_dev->max_speed_hz <= SPI_MAX_FREQ) {
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
	}
	else {
		reg_val |= BIT(1) | BIT(0);
	}

	if (spi_dev->mode & SPI_LSB_FIRST) {
		reg_val |= BIT(7);
	}
	__raw_writel(reg_val, spi_chip->reg_base + SPI_CTL0);
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL1);
	/*spi master mode*/
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL2);
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL4);
	__raw_writel(0x0, spi_chip->reg_base + SPI_CTL5);
	__raw_writel(0x0, spi_chip->reg_base + SPI_INT_EN);
	/*spi fifo deep*/
	__raw_writel(0x0210, spi_chip->reg_base + SPI_CTL3);
	__raw_writel(0x0210, spi_chip->reg_base + SPI_CTL6);
	/*reset fifo*/
	__raw_writel(0x1, spi_chip->reg_base + SPI_FIFO_RST);
	for (i = 0; i < 0x20; i++);
	__raw_writel(0x0, spi_chip->reg_base + SPI_FIFO_RST);

	clk_set_rate(spi_chip->clk, spi_src_clk);

	spi_clk_div = spi_src_clk / (spi_dev->max_speed_hz << 1) -1;
	pr_debug("spi_src_clk = %x,spi_dev->max_speed_hz =%d,spi_clk_div =%x,reg_val=%d. \n",
		spi_src_clk,spi_dev->max_speed_hz,spi_clk_div, reg_val);

	__raw_writel(spi_clk_div, spi_chip->reg_base + SPI_CLKD);

	/*wait the clk config become effective*/
	msleep(5);

	if(dev_info_p && dev_info_p->enable_dma){
		init_completion(&(spi_chip->done));
		if(spi_chip->dma_tx_dev_id){
			dma_ret = sprd_spi_dma_tx_chn_req(spi_chip);
		}
		if(spi_chip->dma_rx_dev_id){
			dma_ret = sprd_spi_dma_rx_chn_req(spi_chip);
		}
		if(dma_ret < 0){
			dev_info_p->enable_dma = 0; /*disable dma*/
		}
		else{
			sprd_spi_alloc_dma_buf(spi_chip, SPI_DMA_BUF_SIZE);
		}
	}
	sprd_spi_backup_config(spi_chip);
	spi_chip->is_active = true;

	/*disable the clk after config complete*/
	clk_disable_unprepare(spi_chip->clk);

	return 0;
}

static void sprd_spi_cleanup(struct spi_device *spi)
{
	struct sprd_spi_devdata *spi_chip;

	spi_chip = spi_master_get_devdata(spi->master);

	clk_disable_unprepare(spi_chip->clk);
}

static int sprd_spi_probe(struct platform_device *pdev)
{
	int ret;
	int irq_num;
	struct resource *regs;
	struct spi_master *master;
	struct sprd_spi_devdata *spi_chip;

#ifdef CONFIG_OF
	u32 val = 0;
	pdev->id = of_alias_get_id(pdev->dev.of_node, "spi");
#else
	struct resource *dma_regs;
#endif
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "spi_%d get io resource failed!\n", pdev->id);
		return -ENODEV;
	}

	if (0 == pdev->id) {
		REGS_SPI0_BASE = (unsigned long)ioremap_nocache(regs->start,
				resource_size(regs));
		if (!REGS_SPI0_BASE)
			BUG();
	}
	else if (1 == pdev->id) {
		REGS_SPI1_BASE = (unsigned long)ioremap_nocache(regs->start,
				resource_size(regs));
		if (!REGS_SPI1_BASE)
			BUG();
	}
	else if (2 == pdev->id) {
		REGS_SPI2_BASE = (unsigned long)ioremap_nocache(regs->start,
				resource_size(regs));
		if (!REGS_SPI2_BASE)
			BUG();
	}

	dev_info(&pdev->dev, "probe spi %d get regbase 0x%lx\n", pdev->id,
			SPI_BASE(pdev->id));

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

#ifdef CONFIG_OF
	ret=of_property_read_u32(pdev->dev.of_node, "sprd,dma_rx_no", &val);
	if (ret) {
		dev_err(&pdev->dev, "spi_%d rx do not have dma function,ret =%d\n",pdev->id,ret);
	}
	spi_chip->dma_rx_dev_id = val;
	ret=of_property_read_u32(pdev->dev.of_node, "sprd,dma_tx_no", &val);
	if (ret) {
		dev_err(&pdev->dev, "spi_%d tx do not have dma function,ret =%d\n",pdev->id,ret);
	}
	spi_chip->dma_tx_dev_id= val;
#else
	dma_regs = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!dma_regs) {
		dev_err(&pdev->dev, "spi_%d rx do not have dma function,ret =%d\n",pdev->id,ret);
	}
	spi_chip->dma_tx_dev_id= dma_regs->start;

	dma_regs = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!dma_regs) {
		dev_err(&pdev->dev, "spi_%d tx do not have dma function,ret =%d\n",pdev->id,ret);
	}
	spi_chip->dma_rx_dev_id= dma_regs->start;
#endif
	/*get clk source*/
	switch (pdev->id) {
		case sprd_spi_0:
			spi_chip->clk = clk_get(NULL, "clk_spi0");
			spi_chip->reg_base_p = (unsigned long *)SPRD_SPI0_PHYS;
			break;
		case sprd_spi_1:
			spi_chip->clk = clk_get(NULL, "clk_spi1");
			spi_chip->reg_base_p = (unsigned long *)SPRD_SPI1_PHYS;
			break;
		case sprd_spi_2:
			spi_chip->clk = clk_get(NULL, "clk_spi2");
			spi_chip->reg_base_p = (unsigned long *)SPRD_SPI2_PHYS;
			break;
		default:
			ret = -EINVAL;
			goto err_exit;
	}
	if (IS_ERR(spi_chip->clk)) {
		ret = -ENXIO;
		goto err_exit;
	}
	spi_chip->spi_dev_id = pdev->id;
	spi_chip->reg_base = (void __iomem *)SPI_BASE(pdev->id);
	spi_chip->irq_num  = irq_num;
	spi_chip->dma_rx_chn = 0;
	spi_chip->dma_tx_chn = 0;
	spi_chip->dma_already_tx_len = 0;
	spi_chip->dma_already_rx_len = 0;

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
#ifdef CONFIG_MACH_GRANDPRIMEVE3G_LTN_DTV
	spi_chip->work_queue = alloc_workqueue(pdev->name, WQ_UNBOUND | WQ_HIGHPRI | WQ_MEM_RECLAIM, 1);
#else
	spi_chip->work_queue = create_singlethread_workqueue(pdev->name);
#endif
	//spi_chip->work_queue = create_workqueue(pdev->name);
	INIT_WORK(&spi_chip->work, sprd_spi_transfer_work);

	platform_set_drvdata(pdev, master);

	ret = spi_register_master(master);
	if (ret) {
		pr_err("register spi master %d failed!\n", master->bus_num);
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

	clk_disable_unprepare(spi_chip->clk);

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
		clk_disable_unprepare(spi_chip->clk);

	return 0;
}

static int sprd_spi_resume(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);

	struct sprd_spi_devdata *spi_chip = spi_master_get_devdata(master);

	if (spi_chip->is_active) {

		clk_prepare_enable(spi_chip->clk);

		sprd_spi_restore_config(spi_chip);

		clk_disable_unprepare(spi_chip->clk);
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

static int __init sprd_spi_init(void)
{
	return platform_driver_register(&sprd_spi_driver);
}

subsys_initcall_sync(sprd_spi_init);

static void __exit sprd_spi_exit(void)
{
	platform_driver_unregister(&sprd_spi_driver);
}

module_exit(sprd_spi_exit);

MODULE_DESCRIPTION("SpreadTrum SPI(r4p0 version) Controller driver");
MODULE_AUTHOR("Jack.Jiang <Jack.Jiang@spreadtrum.com>");
MODULE_LICENSE("GPL");
