/*
 *  linux/drivers/mtd/onenand/pxa3xx_onenand.c
 *
 *  Copyright (C) 2009 Marvell Corporation
 *  Haojian Zhuang <haojian.zhuang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/onenand.h>
#include <linux/mtd/partitions.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mach/flash.h>
#include <asm/highmem.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include <asm/cacheflush.h>
#include <mach/dvfm.h>
#include <plat/pxa3xx_onenand.h>

#ifdef CONFIG_PXA3XX_BBM
#include <plat/pxa3xx_bbm.h>
#endif

#define DRIVER_NAME	"pxa3xx-onenand"

#define SEQUENCE_TWO


static struct dvfm_lock dvfm_lock = {
	.lock = __SPIN_LOCK_UNLOCKED(dvfmlock),
	.dev_idx = -1,
	.count = 0,
};

static void set_dvfm_constraint(void)
{
	spin_lock_irqsave(&dvfm_lock.lock, dvfm_lock.flags);
	if (dvfm_lock.count++ == 0) {
		/* Disable Low power mode */
		dvfm_disable_op_name("D1", dvfm_lock.dev_idx);
		dvfm_disable_op_name("D2", dvfm_lock.dev_idx);
		dvfm_disable_op_name("CG", dvfm_lock.dev_idx);
	}
	spin_unlock_irqrestore(&dvfm_lock.lock, dvfm_lock.flags);
}

static void unset_dvfm_constraint(void)
{
	spin_lock_irqsave(&dvfm_lock.lock, dvfm_lock.flags);
	if (dvfm_lock.count == 0) {
		spin_unlock_irqrestore(&dvfm_lock.lock, dvfm_lock.flags);
		return;
	}
	if (--dvfm_lock.count == 0) {
		/* Enable Low power mode */
		dvfm_enable_op_name("D1", dvfm_lock.dev_idx);
		dvfm_enable_op_name("D2", dvfm_lock.dev_idx);
		dvfm_enable_op_name("CG", dvfm_lock.dev_idx);
	}
	spin_unlock_irqrestore(&dvfm_lock.lock, dvfm_lock.flags);
}

static dma_addr_t map_addr(struct device *dev, void *buf, size_t sz, int dir)
{
#ifdef CONFIG_HIGHMEM
	if ((size_t) buf >= PKMAP_ADDR(0)
	    && (size_t) buf < PKMAP_ADDR(LAST_PKMAP)) {
		struct page *page =
		    pte_page(pkmap_page_table[PKMAP_NR((size_t) buf)]);
		return dma_map_page(dev, page, (size_t) buf & (PAGE_SIZE - 1),
				    sz, dir);
	}
#endif
	if (buf >= high_memory) {
		struct page *page;

		if (((size_t) buf & PAGE_MASK) !=
		    ((size_t) (buf + sz - 1) & PAGE_MASK))
			return ~0;

		page = vmalloc_to_page(buf);
		if (!page)
			return ~0;

		if (cache_is_vivt()) {
			dmac_map_area(buf, sz, dir);
			buf = page_address(page) + ((size_t) buf & ~PAGE_MASK);
		} else
			return dma_map_page(dev, page,
					   (size_t) buf & (PAGE_SIZE - 1),
					   sz, dir);
	}
	return dma_map_single(dev, buf, sz, dir);
}

static void unmap_addr(struct device *dev, dma_addr_t buf, void *orig_buf,
		       size_t sz, int dir)
{
	if (!buf)
		return;
#ifdef CONFIG_HIGHMEM
	if (orig_buf >= high_memory) {
		dma_unmap_page(dev, buf, sz, dir);
		return;
	} else if ((size_t) orig_buf >= PKMAP_ADDR(0)
		   && (size_t) orig_buf < PKMAP_ADDR(LAST_PKMAP)) {
		dma_unmap_page(dev, buf, sz, dir);
		return;
	}
#endif
	dma_unmap_single(dev, buf, sz, dir);
}

void pxa3xx_onenand_get_device(struct mtd_info *mtd)
{
	struct pxa3xx_onenand_info *info =
	    container_of(mtd, struct pxa3xx_onenand_info, mtd);
	clk_enable(info->smc_clk);
	set_dvfm_constraint();
}
EXPORT_SYMBOL(pxa3xx_onenand_get_device);

void pxa3xx_onenand_release_device(struct mtd_info *mtd)
{
	struct pxa3xx_onenand_info *info =
	    container_of(mtd, struct pxa3xx_onenand_info, mtd);
	unset_dvfm_constraint();
	clk_disable(info->smc_clk);
}
EXPORT_SYMBOL(pxa3xx_onenand_release_device);

static void dma_nodesc_handler(int irq, void *data)
{
	struct pxa3xx_onenand_info *info = (struct pxa3xx_onenand_info *)data;
	int i;

	DCSR(irq) = DCSR_STARTINTR | DCSR_ENDINTR | DCSR_BUSERR;

	for (i = 0; i < info->channel_cnt; i++) {
		if (info->dma_ch[i] == irq)
			info->dma_ch_done[i] = 1;
	}
	for (i = 0; i < info->channel_cnt; i++) {
		if (info->dma_ch_done[i] == 0)
			return;
	}
	complete(&info->dma_done);
	return;
}

static inline int pxa3xx_onenand_bufferram_offset(struct mtd_info *mtd,
						  int area)
{
	struct onenand_chip *this = mtd->priv;

	if (ONENAND_CURRENT_BUFFERRAM(this)) {
		if (area == ONENAND_DATARAM)
			return mtd->writesize;
		if (area == ONENAND_SPARERAM)
			return mtd->oobsize;
	}
	return 0;
}

static int pxa3xx_onenand_interrupt(struct pxa3xx_onenand_info *info,
				    dma_addr_t dma_dst, dma_addr_t dma_src,
				    size_t count)
{
	int i, sum, off, len;
	for (i = 0; i < MAX_DMA_CHANNEL; i++)
		info->dma_ch_done[i] = 0;
	INIT_COMPLETION(info->dma_done);

	if (MAX_DMA_CHANNEL > 1) {
		/* use multiple channel */
		sum = count;
		len = ALIGN(sum / MAX_DMA_CHANNEL - 31, 32);
		for (i = 0, off = 0; i < MAX_DMA_CHANNEL; i++, off += len) {
			/* update remaing count */
			if (i == (MAX_DMA_CHANNEL - 1))
				len = sum;
			sum = sum - len;
			DCSR(info->dma_ch[i]) = DCSR_NODESC;
			DSADR(info->dma_ch[i]) = dma_src + off;
			DTADR(info->dma_ch[i]) = dma_dst + off;
			DCMD(info->dma_ch[i]) =
			    DCMD_INCSRCADDR | DCMD_INCTRGADDR | DCMD_BURST32 |
			    DCMD_WIDTH2 | DCMD_ENDIRQEN | len;
			DCSR(info->dma_ch[i]) |= DCSR_RUN;
		}
		info->channel_cnt = MAX_DMA_CHANNEL;
	} else {
		/* use single channel */
		DCSR(info->dma_ch[0]) = DCSR_NODESC;
		DSADR(info->dma_ch[0]) = dma_src;
		DTADR(info->dma_ch[0]) = dma_dst;
		DCMD(info->dma_ch[0]) = DCMD_INCSRCADDR | DCMD_INCTRGADDR
		    | DCMD_BURST32 | DCMD_WIDTH2 | DCMD_ENDIRQEN | count;
		DCSR(info->dma_ch[0]) |= DCSR_RUN;
		info->channel_cnt = 1;
	}
	wait_for_completion(&info->dma_done);
	return 0;
}

static int (*pxa3xx_dma_ops) (struct pxa3xx_onenand_info *info,
			      dma_addr_t dst, dma_addr_t src, size_t count);
/**
 * pxa3xx_onenand_read_bufferram - [OneNAND Interface] Read the bufferram area
 *			with Sync. Burst mode
 * @param mtd		MTD data structure
 * @param area		BufferRAM area
 * @param buffer	the databuffer to put/get data
 * @param offset	offset to read from or write to
 * @param count		number of bytes to read/write
 *
 * Read the BufferRAM area with Sync. Burst Mode
 */

static int pxa3xx_onenand_read_bufferram(struct mtd_info *mtd, int area,
					 unsigned char *buffer, int offset,
					 size_t count)
{
	struct pxa3xx_onenand_info *info =
	    container_of(mtd, struct pxa3xx_onenand_info, mtd);
	struct onenand_chip *this = mtd->priv;
	void __iomem *bufferram;
	dma_addr_t dma_src, dma_dst;
	void *buf = (void *)buffer;
	int dma_map_used = 0;
	struct device *dev = &info->pdev->dev;

	bufferram = this->base + area;
	bufferram += pxa3xx_onenand_bufferram_offset(mtd, area);

	if ((offset & 31) || (count & 31))
		goto normal;

	dma_src = info->phys_base + (bufferram - this->base) + offset;
	dma_dst = info->sram_addr_phys;

	if ((size_t) buf & 31)
		goto dma_routine;

	/* DMA mapping */
	dma_dst = map_addr(dev, buf, count, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, dma_dst))
		dma_dst = info->sram_addr_phys;
	else
		dma_map_used = 1;

dma_routine:
	if (this->mmcontrol)
		this->mmcontrol(mtd, ONENAND_SYS_CFG1_SYNC_READ);

	pxa3xx_dma_ops(info, dma_dst, dma_src, count);

	if (0 == dma_map_used)
		memcpy(buffer, info->sram_addr, count);
	else if (1 == dma_map_used)
		unmap_addr(dev, dma_dst, buffer, count, DMA_FROM_DEVICE);

	if (this->mmcontrol)
		this->mmcontrol(mtd, 0);

	return 0;
normal:

	if (ONENAND_CHECK_BYTE_ACCESS(count)) {
		unsigned short word;

		/* Align with word(16-bit) size */
		count--;

		/* Read word and save byte */
		word = this->read_word(bufferram + offset + count);
		buffer[count] = (word & 0xff);
	}

	memcpy(buffer, bufferram + offset, count);
	return 0;
}

static int pxa3xx_onenand_bbt(struct mtd_info *mtd)
{
	int ret;
	struct pxa3xx_onenand_info *info =
	    container_of(mtd, struct pxa3xx_onenand_info, mtd);

	ret = pxa3xx_scan_bbt(mtd);
	if (ret) {
		printk(KERN_INFO "pxa3xx_scan_bbt  failed\n");
		goto out;
	}
	info->bbm = mtd->bbm;

out:
	return onenand_default_bbt(mtd);
}

static int __devinit pxa3xx_onenand_probe(struct platform_device *dev)
{
	struct pxa3xx_onenand_info *info;
	struct platform_device *pdev = dev;
	struct pxa3xx_onenand_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res = pdev->resource;
	struct onenand_chip *this = NULL;
	unsigned long size = res->end - res->start + 1;
	int err = 0, i, ret;
	char name[80];

	info = kzalloc(sizeof(struct pxa3xx_onenand_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	this = &info->onenand;
	init_completion(&info->irq_done);
	init_completion(&info->dma_done);

	if (!request_mem_region(res->start, size, pdev->name)) {
		err = -EBUSY;
		goto out_free_info;
	}

	info->phys_base = res->start;
	info->onenand.base = ioremap(res->start, size);
	if (!info->onenand.base) {
		err = -ENOMEM;
		goto out_release_mem_region;
	}

	size = ONENAND_REG_MANUFACTURER_ID - ONENAND_DATARAM;
	if (pdata->mmcontrol) {
		/*
		 * If synchronous mode is to be used, mmcontrol() should be
		 * defined in platform driver.
		 */
		info->onenand.mmcontrol = pdata->mmcontrol;
		info->onenand.read_bufferram = pxa3xx_onenand_read_bufferram;
	}
#ifdef CONFIG_PXA3XX_BBM
	info->onenand.scan_bbt = pxa3xx_onenand_bbt;
	info->onenand.block_markbad = pxa3xx_block_markbad;
	info->onenand.block_bad = pxa3xx_block_bad;
#endif

	info->onenand.irq = platform_get_irq(pdev, 0);
	info->mtd.name = dev_name(&pdev->dev);

	info->mtd.priv = &info->onenand;
	info->mtd.owner = THIS_MODULE;

	dvfm_register("ONENAND", &dvfm_lock.dev_idx);

	info->smc_clk = clk_get(NULL, "SMCCLK");
	clk_enable(info->smc_clk);

	for (i = 0; i < MAX_DMA_CHANNEL; i++)
		info->dma_ch[i] = -1;	/* not allocated */

	for (i = 0; i < MAX_DMA_CHANNEL; i++) {
		sprintf(name, "ONENAND_%d", i);
		info->dma_ch[i] = pxa_request_dma(name, DMA_PRIO_HIGH,
						  dma_nodesc_handler,
						  (void *)info);
		if (info->dma_ch[i] < 0) {
			ret = -ENODEV;
			goto out_freedma;
		}
	}
	pxa3xx_dma_ops = pxa3xx_onenand_interrupt;
	info->sram_addr = dma_alloc_coherent(NULL, 4096,
					     &info->sram_addr_phys, GFP_KERNEL);
	if (onenand_scan(&info->mtd, 1)) {
		err = -ENXIO;
		goto out_freedma;
	}

	/* remount mmcontrol in order to enable private read_bufferram */
	if (pdata->mmcontrol)
		info->onenand.read_bufferram = pxa3xx_onenand_read_bufferram;

	if (pdata->set_partition_info) {
		pdata->set_partition_info((info->onenand.chipsize) >> 20,
					  info->onenand.writesize, pdata);
	}
	ret = mtd_device_parse_register(&info->mtd, NULL, 0,
					pdata->parts, pdata->nr_parts);

	dev_set_drvdata(&pdev->dev, info);

	clk_disable(info->smc_clk);

	return 0;

out_freedma:
	clk_disable(info->smc_clk);

	for (i = 0; i < MAX_DMA_CHANNEL; i++) {
		if (info->dma_ch[i] >= 0)
			pxa_free_dma(info->dma_ch[i]);
	}
out_release_mem_region:
	release_mem_region(res->start, size);
out_free_info:
	kfree(info);

	return err;
}

static int __devexit pxa3xx_onenand_remove(struct platform_device *dev)
{
	struct platform_device *pdev = dev;
	struct pxa3xx_onenand_info *info = dev_get_drvdata(&pdev->dev);
	struct resource *res = pdev->resource;
	unsigned long size = res->end - res->start + 1;
	int i;

	dev_set_drvdata(&pdev->dev, NULL);

	if (info) {
		for (i = 0; i < MAX_DMA_CHANNEL; i++) {
			if (info->dma_ch[i] >= 0)
				pxa_free_dma(info->dma_ch[i]);
		}

		mtd_device_unregister(&info->mtd);

		onenand_release(&info->mtd);
		if (info->bbm && info->bbm->uninit)
			info->bbm->uninit(&info->mtd);
		release_mem_region(res->start, size);
		iounmap(info->onenand.base);
		kfree(info);
	}

	dvfm_unregister("ONENAND", &dvfm_lock.dev_idx);

	return 0;
}

static struct platform_driver pxa3xx_onenand_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = pxa3xx_onenand_probe,
	.remove = pxa3xx_onenand_remove,
#ifdef CONFIG_PM
	.suspend = NULL,
	.resume = NULL,
#endif
};

static int __init pxa3xx_onenand_init(void)
{
	return platform_driver_register(&pxa3xx_onenand_driver);
}

static void __exit pxa3xx_onenand_exit(void)
{
	platform_driver_unregister(&pxa3xx_onenand_driver);
}

module_init(pxa3xx_onenand_init);
module_exit(pxa3xx_onenand_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_AUTHOR("Haojian Zhuang <haojian.zhuang@marvell.com>");
MODULE_DESCRIPTION("Glue layer for OneNAND flash on PXA3xx");
