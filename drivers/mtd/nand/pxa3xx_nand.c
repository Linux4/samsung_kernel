/*
 * drivers/mtd/nand/pxa3xx_nand.c
 *
 * Copyright © 2005 Intel Corporation
 * Copyright © 2006 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/highmem.h>

#include <asm/cacheflush.h>
#include <mach/dma.h>
#include <plat/pxa3xx_nand.h>
#ifdef CONFIG_PXA3XX_BBM
#include <plat/pxa3xx_bbm.h>
#endif

#define	CHIP_DELAY_TIMEOUT	(2 * HZ/10)
#define NAND_STOP_DELAY		(2 * HZ/50)
#define PAGE_CHUNK_SIZE		(2048)
#define OOB_CHUNK_SIZE		(64)
#define CMD_POOL_SIZE		(5)
#define BCH_THRESHOLD           (8)
#define BCH_STRENGTH		(4)
#define HAMMING_STRENGTH	(1)
#define DMA_H_SIZE		(sizeof(struct pxa_dma_desc)*2)

/* registers and bit definitions */
#define NDCR		(0x00) /* Control register */
#define NDTR0CS0	(0x04) /* Timing Parameter 0 for CS0 */
#define NDTR1CS0	(0x0C) /* Timing Parameter 1 for CS0 */
#define NDSR		(0x14) /* Status Register */
#define NDPCR		(0x18) /* Page Count Register */
#define NDBDR0		(0x1C) /* Bad Block Register 0 */
#define NDBDR1		(0x20) /* Bad Block Register 1 */
#define NDECCCTRL	(0x28) /* ECC Control Register */
#define NDDB		(0x40) /* Data Buffer */
#define NDCB0		(0x48) /* Command Buffer0 */
#define NDCB1		(0x4C) /* Command Buffer1 */
#define NDCB2		(0x50) /* Command Buffer2 */

#define NDCR_SPARE_EN		(0x1 << 31)
#define NDCR_ECC_EN		(0x1 << 30)
#define NDCR_DMA_EN		(0x1 << 29)
#define NDCR_ND_RUN		(0x1 << 28)
#define NDCR_DWIDTH_C		(0x1 << 27)
#define NDCR_DWIDTH_M		(0x1 << 26)
#define NDCR_PAGE_SZ		(0x1 << 24)
#define NDCR_NCSX		(0x1 << 23)
#define NDCR_FORCE_CSX          (0x1 << 21)
#define NDCR_CLR_PG_CNT		(0x1 << 20)
#define NDCR_STOP_ON_UNCOR	(0x1 << 19)
#define NDCR_RD_ID_CNT_MASK	(0x7 << 16)
#define NDCR_RD_ID_CNT(x)	(((x) << 16) & NDCR_RD_ID_CNT_MASK)

#define NDCR_RA_START		(0x1 << 15)
#define NDCR_PG_PER_BLK_MASK	(0x3 << 13)
#define NDCR_PG_PER_BLK(x)	(((x) << 13) & NDCR_PG_PER_BLK_MASK)
#define NDCR_ND_ARB_EN		(0x1 << 12)
#define NDCR_RDYM               (0x1 << 11)
#define NDCR_INT_MASK           (0xFFF)

#define NDSR_MASK		(0xffffffff)
#define NDSR_ERR_CNT_MASK       (0x1F << 16)
#define NDSR_ERR_CNT(x)         (((x) << 16) & NDSR_ERR_CNT_MASK)
#define NDSR_RDY                (0x1 << 12)
#define NDSR_FLASH_RDY          (0x1 << 11)
#define NDSR_CS0_PAGED		(0x1 << 10)
#define NDSR_CS1_PAGED		(0x1 << 9)
#define NDSR_CS0_CMDD		(0x1 << 8)
#define NDSR_CS1_CMDD		(0x1 << 7)
#define NDSR_CS0_BBD		(0x1 << 6)
#define NDSR_CS1_BBD		(0x1 << 5)
#define NDSR_DBERR		(0x1 << 4)
#define NDSR_SBERR		(0x1 << 3)
#define NDSR_WRDREQ		(0x1 << 2)
#define NDSR_RDDREQ		(0x1 << 1)
#define NDSR_WRCMDREQ		(0x1)

#define NDCB0_CMD_XTYPE_MASK	(0x7 << 29)
#define NDCB0_CMD_XTYPE(x)	(((x) << 29) & NDCB0_CMD_XTYPE_MASK)
#define NDCB0_LEN_OVRD		(0x1 << 28)
#define NDCB0_ST_ROW_EN         (0x1 << 26)
#define NDCB0_AUTO_RS		(0x1 << 25)
#define NDCB0_CSEL		(0x1 << 24)
#define NDCB0_CMD_TYPE_MASK	(0x7 << 21)
#define NDCB0_CMD_TYPE(x)	(((x) << 21) & NDCB0_CMD_TYPE_MASK)
#define NDCB0_NC		(0x1 << 20)
#define NDCB0_DBC		(0x1 << 19)
#define NDCB0_ADDR_CYC_MASK	(0x7 << 16)
#define NDCB0_ADDR_CYC(x)	(((x) << 16) & NDCB0_ADDR_CYC_MASK)
#define NDCB0_CMD2_MASK		(0xff << 8)
#define NDCB0_CMD1_MASK		(0xff)
#define NDCB0_ADDR_CYC_SHIFT	(16)

/* ECC Control Register */
#define NDECCCTRL_ECC_SPARE_MSK (0xFF << 7)
#define NDECCCTRL_ECC_SPARE(x)  (((x) << 7) & NDECCCTRL_ECC_SPARE_MSK)
#define NDECCCTRL_ECC_THR_MSK   (0x3F << 1)
#define NDECCCTRL_ECC_THRESH(x) (((x) << 1) & NDECCCTRL_ECC_THR_MSK)
#define NDECCCTRL_BCH_EN        (0x1)

/* macros for registers read/write */
#define nand_writel(info, off, val)	\
	__raw_writel((val), (info)->mmio_base + (off))

#define nand_readl(info, off)		\
	__raw_readl((info)->mmio_base + (off))

/* error code and state */
enum {
	ERR_NONE	= 0,
	ERR_DMABUSERR	= -1,
	ERR_SENDCMD	= -2,
	ERR_DBERR	= -3,
	ERR_BBERR	= -4,
	ERR_SBERR	= -5,
};

enum {
	STATE_IDLE = 0,
	STATE_PREPARED,
	STATE_CMD_HANDLE,
	STATE_DMA_READING,
	STATE_DMA_WRITING,
	STATE_DMA_DONE,
	STATE_PIO_READING,
	STATE_PIO_WRITING,
	STATE_CMD_DONE,
	STATE_READY,
};

struct pxa3xx_nand_host {
	struct nand_chip	chip;
	struct pxa3xx_nand_cmdset *cmdset;
	struct mtd_info         *mtd;
	void			*info_data;

	/* page size of attached chip */
	unsigned int		page_size;
	unsigned int		ecc_strength;
	int			cs;

	/* calculated from pxa3xx_nand_flash data */
	unsigned int		col_addr_cycles;
	unsigned int		row_addr_cycles;
	size_t			read_id_bytes;

	/* cached register value */
	uint32_t		reg_ndcr;
	uint32_t		ndtr0cs0;
	uint32_t		ndtr1cs0;
};

struct pxa3xx_nand_info {
	struct nand_hw_control	controller;
	struct platform_device	 *pdev;

	struct clk		*clk;
	void __iomem		*mmio_base;
	unsigned long		mmio_phys;
	struct completion	cmd_complete;

	int			command;
	int			total_cmds;

	/* DMA information */
	int			drcmr_dat;
	int			drcmr_cmd;

	unsigned char		*data_buff;
	unsigned char		*oob_buff;
	dma_addr_t 		data_buff_phys;
	int 			data_dma_ch;
	struct pxa_dma_desc	*data_desc;
	dma_addr_t 		data_desc_addr;

	struct pxa3xx_nand_host *host[NUM_CHIP_SELECT];
	unsigned int		state;

	int			cs;
	unsigned int		page_size;	/* page size of attached chip */

	int			retcode;
	unsigned int		ecc_strength;
	unsigned int		bad_count;
	int			use_dma;	/* use DMA ? */
	int			is_ready;

	unsigned int		data_size;	/* data size in FIFO */
	unsigned int		oob_size;
	unsigned int		buf_start;
	unsigned int		buf_count;
	unsigned int		data_column;
	unsigned int		oob_column;

	/* generated NDCBx register values */
	uint8_t			cmd_seqs;
	uint8_t			wait_ready[CMD_POOL_SIZE];
	uint32_t		ndcb0[CMD_POOL_SIZE];
	uint32_t		ndcb1;
	uint32_t		ndcb2;
};

static bool use_dma = 1;
module_param(use_dma, bool, 0444);
MODULE_PARM_DESC(use_dma, "enable DMA for data transferring to/from NAND HW");

static int use_polling = 0;
module_param(use_polling, int, 0444);
MODULE_PARM_DESC(use_polling, "Use full polling mode");

/*
 * Default NAND flash controller configuration setup by the
 * bootloader. This configuration is used only when CONFIG_KEEP attr is set
 */
static struct pxa3xx_nand_cmdset default_cmdset = {
	.read1		= 0x3000,
	.read2		= 0x0050,
	.program	= 0x1080,
	.read_status	= 0x0070,
	.read_id	= 0x0090,
	.erase		= 0xD060,
	.reset		= 0x00FF,
	.lock		= 0x002A,
	.unlock		= 0x2423,
	.lock_status	= 0x007A,
};

static struct pxa3xx_nand_timing timing[] = {
	{ 40, 80, 60, 100, 80, 100, 90000, 400, 40, },
	{ 10,  0, 20,  40, 30,  40, 11123, 110, 10, },
	{ 10, 25, 15,  25, 15,  30, 25000,  60, 10, },
	{ 10, 35, 15,  25, 15,  25, 25000,  60, 10, },
	{  5, 20, 15,  25, 15,  25, 25000,  80, 10, },
	{  5, 20, 10,  12, 15,  12, 25000,  80, 10, },
	{ 10, 25, 15,  25, 15,  25, 25000,  80, 10, },
};

static struct pxa3xx_nand_flash builtin_flash_types[] = {
{ "DEFAULT FLASH",      0,      0,   0, 2048,  8,  8, 0,    0, &timing[0] },
{ "64MiB 16-bit",  0x46ec, 0xffff,  32,  512, 16, 16, 1, 4096, &timing[1] },
{ "256MiB 8-bit",  0xdaec, 0xffff,  64, 2048,  8,  8, 1, 2048, &timing[1] },
{ "4GiB 8-bit",    0xd7ec, 0xb655, 128, 4096,  8,  8, 4, 8192, &timing[1] },
{ "4GiB 8-bit",    0xd7ec, 0x29d5, 128, 4096,  8,  8, 8, 8192, &timing[1] },
{ "128MiB 8-bit",  0xa12c, 0xffff,  64, 2048,  8,  8, 1, 1024, &timing[2] },
{ "128MiB 16-bit", 0xb12c, 0xffff,  64, 2048, 16, 16, 1, 1024, &timing[2] },
{ "512MiB 8-bit",  0xdc2c, 0xffff,  64, 2048,  8,  8, 1, 4096, &timing[2] },
{ "512MiB 16-bit", 0xcc2c, 0xffff,  64, 2048, 16, 16, 1, 4096, &timing[2] },
{ "1GiB 8-bit",    0x382c, 0xffff, 128, 4096,  8,  8, 4, 2048, &timing[2] },
{ "256MiB 16-bit", 0xba20, 0xffff,  64, 2048, 16, 16, 1, 2048, &timing[3] },
{ "512MiB 16-bit", 0xbcad, 0xffff,  64, 2048, 16, 16, 1, 4096, &timing[4] },
{ "512MiB 16-bit BCH", 0xbc2c, 0x5590, 64, 2048, 16, 16, 4, 4096, &timing[2] },
{ "ST 1Gb 128MiB 8-bit", 0xa120, 0x1500, 64, 2048,  8,  8, 1, 1024, &timing[6] },
{ "Micron 4bit ECC 1Gb 128MiB 8-bit 1.8V", 0xa12c, 0x1580, 64, 2048,  8,  8, 4, 1024, &timing[5] },
{ "Hynix 1G 128M 8-bit", 0xa1ad, 0x1500, 64, 2048,  8,  8, 1, 1024, &timing[3] },
};

static struct nand_ecclayout bch_nand_oob_64 = {
	.eccbytes = 32,
	.eccpos = {
		32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = { {2, 30} }
};

static struct nand_ecclayout bch_nand_oob_128 = {
	.eccbytes = 64,
	.eccpos = {
		64, 65, 66, 67, 68, 69, 70, 71,
		72, 73, 74, 75, 76, 77, 78, 79,
		80, 81, 82, 83, 84, 85, 86, 87,
		88, 89, 90, 91, 92, 93, 94, 95,
		96, 97, 98, 99, 100, 101, 102, 103,
		104, 105, 106, 107, 108, 109, 110, 111,
		112, 113, 114, 115, 116, 117, 118, 119,
		120, 121, 122, 123, 124, 125, 126, 127},
	.oobfree = { {2, 62} }
};

/* Define a default flash type setting serve as flash detecting only */
#define DEFAULT_FLASH_TYPE (&builtin_flash_types[0])

const char *mtd_names[] = {"pxa3xx_nand-0", "pxa3xx_nand-1", NULL};

#define NDTR0_tCH(c)	(min((c), 7) << 19)
#define NDTR0_tCS(c)	(min((c), 7) << 16)
#define NDTR0_tWH(c)	(min((c), 7) << 11)
#define NDTR0_tWP(c)	(min((c), 7) << 8)
#define NDTR0_tRH(c)	(min((c), 7) << 3)
#define NDTR0_tRP(c)	(min((c), 7) << 0)

#define NDTR1_tR(c)	(min((c), 65535) << 16)
#define NDTR1_tWHR(c)	(min((c), 15) << 4)
#define NDTR1_tAR(c)	(min((c), 15) << 0)

/* convert nano-seconds to nand flash controller clock cycles */
#define ns2cycle(ns, clk)	(int)((ns) * (clk / 1000000) / 1000)

static dma_addr_t map_addr(struct pxa3xx_nand_info *info, void *buf,
			   size_t sz, int dir)
{
	struct device *dev = &info->pdev->dev;
	struct page *page;
	/* if not cache aligned, don't use dma */
	if (((size_t)buf & 0x1f) || (sz & 0x1f))
		return ~0;
#ifdef CONFIG_HIGHMEM
	if ((size_t)buf >= PKMAP_ADDR(0)
	    && (size_t)buf < PKMAP_ADDR(LAST_PKMAP)) {
		page = pte_page(pkmap_page_table[PKMAP_NR((size_t)buf)]);
		return dma_map_page(dev, page, (size_t)buf & (PAGE_SIZE - 1),
				    sz, dir);
	}
#endif
	if (buf >= high_memory) {
		if (((size_t) buf & PAGE_MASK) !=
		    ((size_t) (buf + sz - 1) & PAGE_MASK))
			return ~0;

		page = vmalloc_to_page(buf);
		if (!page)
			return ~0;

		if (cache_is_vivt()) {
			dmac_map_area(buf, sz, dir);
			buf = page_address(page) + ((size_t)buf & ~PAGE_MASK);
		} else
			return dma_map_page(dev, page,
					    (size_t)buf & (PAGE_SIZE - 1),
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
	} else if ((size_t)orig_buf >= PKMAP_ADDR(0)
		   && (size_t)orig_buf < PKMAP_ADDR(LAST_PKMAP)) {
		dma_unmap_page(dev, buf, sz, dir);
		return;
	}
#endif
	dma_unmap_single(dev, buf, sz, dir);
}

static void pxa3xx_nand_set_timing(struct pxa3xx_nand_host *host,
				   const struct pxa3xx_nand_timing *t)
{
	struct pxa3xx_nand_info *info = host->info_data;
	unsigned long nand_clk = clk_get_rate(info->clk);
	struct pxa3xx_nand_platform_data *pdata;
	uint32_t ndtr0, ndtr1, tr;

	pdata = info->pdev->dev.platform_data;
	if (pdata->attr & NAKED_CMD)
		tr = 0;
	else
		tr = t->tR;

	ndtr0 = NDTR0_tCH(ns2cycle(t->tCH, nand_clk)) |
		NDTR0_tCS(ns2cycle(t->tCS, nand_clk)) |
		NDTR0_tWH(ns2cycle(t->tWH, nand_clk)) |
		NDTR0_tWP(ns2cycle(t->tWP, nand_clk)) |
		NDTR0_tRH(ns2cycle(t->tRH, nand_clk)) |
		NDTR0_tRP(ns2cycle(t->tRP, nand_clk));

	ndtr1 = NDTR1_tR(ns2cycle(tr, nand_clk)) |
		NDTR1_tWHR(ns2cycle(t->tWHR, nand_clk)) |
		NDTR1_tAR(ns2cycle(t->tAR, nand_clk));

	host->ndtr0cs0 = ndtr0;
	host->ndtr1cs0 = ndtr1;
	nand_writel(info, NDTR0CS0, ndtr0);
	nand_writel(info, NDTR1CS0, ndtr1);
}

static void pxa3xx_set_datasize(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand_host *host = info->host[info->cs];
	int oob_enable = host->reg_ndcr & NDCR_SPARE_EN;

	info->data_size = (host->page_size < PAGE_CHUNK_SIZE)
				? 512 : PAGE_CHUNK_SIZE;
	if (!oob_enable) {
		info->oob_size = 0;
		return;
	}

	if (host->page_size < PAGE_CHUNK_SIZE) {
		switch (info->ecc_strength) {
		case 0:
			info->oob_size = 16;
			break;
		case HAMMING_STRENGTH:
			info->oob_size = 8;
			break;
		default:
			dev_err(&info->pdev->dev, "Don't support BCH on "
				"small page device!!!\n");
			BUG();
		}
		return;
	}

	switch (info->ecc_strength) {
	case 0:
		info->oob_size = 64;
		break;
	case HAMMING_STRENGTH:
		info->oob_size = 40;
		break;
	default:
		info->oob_size = 32;
	}
}

/**
 * NOTE: it is a must to set ND_RUN firstly, then write
 * command buffer, otherwise, it does not work.
 * We enable all the interrupt at the same time, and
 * let pxa3xx_nand_irq to handle all logic.
 */
static void pxa3xx_nand_start(struct pxa3xx_nand_info *info)
{
	struct pxa3xx_nand_host *host = info->host[info->cs];
	uint32_t ndcr, ndeccctrl = 0;

	ndcr = host->reg_ndcr;
	ndcr |= info->use_dma ? NDCR_DMA_EN : 0;
	ndcr |= NDCR_ND_RUN;
	ndcr |= use_polling ? NDCR_INT_MASK : 0;
	switch (info->ecc_strength) {
	default:
		ndeccctrl |= NDECCCTRL_BCH_EN;
		ndeccctrl |= NDECCCTRL_ECC_THRESH(BCH_THRESHOLD);
	case HAMMING_STRENGTH:
		ndcr |= NDCR_ECC_EN;
	case 0:
		break;
	}

	/* clear status bits and run */
	nand_writel(info, NDCR, 0);
	nand_writel(info, NDECCCTRL, ndeccctrl);
	nand_writel(info, NDSR, NDSR_MASK);
	nand_writel(info, NDCR, ndcr);
}

static void pxa3xx_nand_stop(struct pxa3xx_nand_info *info)
{
	uint32_t ndcr;
	int timeout = NAND_STOP_DELAY;

	/* wait RUN bit in NDCR become 0 */
	ndcr = nand_readl(info, NDCR);
	while ((ndcr & NDCR_ND_RUN) && (timeout-- > 0)) {
		ndcr = nand_readl(info, NDCR);
		udelay(1);
	}

	if (timeout <= 0) {
		ndcr &= ~NDCR_ND_RUN;
		nand_writel(info, NDCR, ndcr);
	}
	/* clear status bits */
	nand_writel(info, NDSR, NDSR_MASK);
}

static void enable_int(struct pxa3xx_nand_info *info, uint32_t int_mask)
{
	uint32_t ndcr;

	ndcr = nand_readl(info, NDCR);
	nand_writel(info, NDCR, ndcr & ~int_mask);
}

static void disable_int(struct pxa3xx_nand_info *info, uint32_t int_mask)
{
	uint32_t ndcr;

	ndcr = nand_readl(info, NDCR);
	nand_writel(info, NDCR, ndcr | int_mask);
}

static void handle_data_pio(struct pxa3xx_nand_info *info)
{
	switch (info->state) {
	case STATE_PIO_WRITING:
		__raw_writesl(info->mmio_base + NDDB,
				info->data_buff + info->data_column,
				DIV_ROUND_UP(info->data_size, 4));
		if (info->oob_size > 0)
			__raw_writesl(info->mmio_base + NDDB,
					info->oob_buff + info->oob_column,
					DIV_ROUND_UP(info->oob_size, 4));
		break;
	case STATE_PIO_READING:
		__raw_readsl(info->mmio_base + NDDB,
				info->data_buff + info->data_column,
				DIV_ROUND_UP(info->data_size, 4));
		if (info->oob_size > 0)
			__raw_readsl(info->mmio_base + NDDB,
					info->oob_buff + info->oob_column,
					DIV_ROUND_UP(info->oob_size, 4));
		break;
	default:
		dev_err(&info->pdev->dev, "%s: invalid state %d\n", __func__,
				info->state);
		BUG();
	}

	info->data_column += info->data_size;
	info->oob_column += info->oob_size;
}

static void start_data_dma(struct pxa3xx_nand_info *info)
{
	struct pxa_dma_desc *desc, *desc_oob;
	struct pxa3xx_nand_host *host = info->host[info->cs];
	struct mtd_info *mtd = host->mtd;
	unsigned int data_len = ALIGN(info->data_size, 32);
	unsigned int oob_len = ALIGN(info->oob_size, 32);
	dma_addr_t oob_phys;

	desc = info->data_desc;
	desc->dcmd = DCMD_WIDTH4 | DCMD_BURST32;
	if (!use_polling)
		desc->dcmd |= DCMD_ENDIRQEN;
	desc_oob = NULL;
	oob_phys = 0;
	if (oob_len) {
		/*
		 * Calculate out oob phys position by nand_buffers structure
		 * and how oob_poi get its value
		 */
		oob_phys = info->data_desc_addr + DMA_H_SIZE
			+ (NAND_MAX_OOBSIZE * 2) + mtd->writesize;
		desc->ddadr = info->data_desc_addr
				+ sizeof(struct pxa_dma_desc);
		desc_oob = desc + 1;
		desc_oob->ddadr = DDADR_STOP;
		desc_oob->dcmd = desc->dcmd;
	} else
		desc->ddadr = DDADR_STOP;

	switch (info->state) {
	case STATE_DMA_WRITING:
		desc->dsadr = info->data_buff_phys + info->data_column;
		desc->dtadr = info->mmio_phys + NDDB;
		desc->dcmd |= DCMD_INCSRCADDR | DCMD_FLOWTRG | data_len;
		if (oob_len) {
			desc_oob->dsadr = oob_phys + info->oob_column;
			desc_oob->dcmd |= DCMD_INCSRCADDR
					| DCMD_FLOWTRG | oob_len;
			desc_oob->dtadr = desc->dtadr;
		}
		break;
	case STATE_DMA_READING:
		desc->dtadr = info->data_buff_phys + info->data_column;
		desc->dsadr = info->mmio_phys + NDDB;
		desc->dcmd |= DCMD_INCTRGADDR | DCMD_FLOWSRC | data_len;
		if (oob_len) {
			desc_oob->dtadr = oob_phys + info->oob_column;
			desc_oob->dcmd |= DCMD_INCTRGADDR
					| DCMD_FLOWSRC | oob_len;
			desc_oob->dsadr = desc->dsadr;
		}
		break;
	default:
		dev_err(&info->pdev->dev, "%s: invalid state %d\n", __func__,
				info->state);
		BUG();
	}

	DRCMR(info->drcmr_dat) = DRCMR_MAPVLD | info->data_dma_ch;
	DDADR(info->data_dma_ch) = info->data_desc_addr;
	DCSR(info->data_dma_ch) |= DCSR_RUN;
}

static inline void dma_complete(int channel, struct pxa3xx_nand_info *info)
{
	uint32_t dcsr;

	dcsr = DCSR(channel);
	DCSR(channel) = dcsr;

	if (dcsr & DCSR_BUSERR) {
		info->retcode = ERR_DMABUSERR;
	}

	info->state = STATE_DMA_DONE;
	info->data_column += info->data_size;
	info->oob_column += info->oob_size;
}

static void pxa3xx_nand_data_dma_irq(int channel, void *data)
{
	struct pxa3xx_nand_info *info = data;

	dma_complete(channel, info);

	if (!use_polling)
		enable_int(info, NDCR_INT_MASK);
	nand_writel(info, NDSR, NDSR_WRDREQ | NDSR_RDDREQ);
}

static int pxa3xx_nand_transaction(struct pxa3xx_nand_info *info)
{
	unsigned int status, is_completed = 0;
	unsigned int ready, cmd_done, ndcb1, ndcb2;

	if (info->cs == 0) {
		ready           = NDSR_FLASH_RDY;
		cmd_done        = NDSR_CS0_CMDD;
	} else {
		ready           = NDSR_RDY;
		cmd_done        = NDSR_CS1_CMDD;
	}

	status = nand_readl(info, NDSR);

	if (status & NDSR_DBERR)
		info->retcode = ERR_DBERR;
	if (status & NDSR_SBERR)
		info->retcode = ERR_SBERR;
	if (status & (NDSR_RDDREQ | NDSR_WRDREQ)) {
		/* whether use dma to transfer data */
		if (info->use_dma) {
			disable_int(info, NDCR_INT_MASK);
			info->state = (status & NDSR_RDDREQ) ?
				      STATE_DMA_READING : STATE_DMA_WRITING;
			start_data_dma(info);
			if (use_polling) {
				while (!(DCSR(info->data_dma_ch) & DCSR_STOPSTATE))
					;
				dma_complete(info->data_dma_ch, info);
			}
			else
				goto NORMAL_IRQ_EXIT;
		} else {
			info->state = (status & NDSR_RDDREQ) ?
				      STATE_PIO_READING : STATE_PIO_WRITING;
			handle_data_pio(info);
		}
	}
	if (status & ready) {
		info->is_ready = 1;
		info->state = STATE_READY;
		if (info->wait_ready[info->cmd_seqs]) {
			if (!use_polling)
				enable_int(info, NDCR_INT_MASK);
			if (info->cmd_seqs == info->total_cmds)
				is_completed = 1;
		}
	}

	if (info->wait_ready[info->cmd_seqs] && info->state != STATE_READY) {
		if (!use_polling)
			disable_int(info, NDCR_INT_MASK & ~NDCR_RDYM);
		goto NORMAL_IRQ_EXIT;
	}

	if (status & cmd_done) {
		info->state = STATE_CMD_DONE;
		if (info->cmd_seqs == info->total_cmds
		    && !info->wait_ready[info->cmd_seqs])
			is_completed = 1;
	}

	if (status & NDSR_WRCMDREQ) {
		nand_writel(info, NDSR, NDSR_WRCMDREQ);
		status &= ~NDSR_WRCMDREQ;
		info->state = STATE_CMD_HANDLE;
		if (info->cmd_seqs < info->total_cmds) {
			if (info->cmd_seqs == 0) {
				ndcb1 = info->ndcb1;
				ndcb2 = info->ndcb2;
			} else {
				ndcb1 = 0;
				ndcb2 = 0;
			}
			nand_writel(info, NDCB0, info->ndcb0[info->cmd_seqs]);
			nand_writel(info, NDCB0, ndcb1);
			nand_writel(info, NDCB0, ndcb2);
			if (info->ndcb0[info->cmd_seqs] & NDCB0_LEN_OVRD)
				nand_writel(info, NDCB0, info->data_size
					    + info->oob_size);
		} else
			is_completed = 1;

		info->cmd_seqs++;
	}

	/* clear NDSR to let the controller exit the IRQ */
	nand_writel(info, NDSR, status);
NORMAL_IRQ_EXIT:
	return is_completed;
}

static irqreturn_t pxa3xx_nand_irq(int irq, void *devid)
{
	struct pxa3xx_nand_info *info = devid;
	if (pxa3xx_nand_transaction(info))
		complete(&info->cmd_complete);
	return IRQ_HANDLED;
}

static int pxa3xx_nand_polling(struct pxa3xx_nand_info *info)
{
	int ret = 0, old = 0;
	unsigned long timeout = ~0, i;

	for (i = 0; i < timeout; i++) {
		ret = nand_readl(info, NDSR);
		if (old != ret) {
			old = ret;
			if (ret)
				ret = pxa3xx_nand_transaction(info);

			if (ret)
				break;
		}
	}

	return ret;
}
static inline int is_buf_blank(uint8_t *buf, size_t len)
{
	for (; len > 0; len--)
		if (*buf++ != 0xff)
			return 0;
	return 1;
}

static int prepare_command_pool(struct pxa3xx_nand_info *info, int command,
		uint16_t column, int page_addr)
{
	uint16_t cmd;
	int addr_cycle, exec_cmd, ndcb0, i, chunks = 0;
	struct pxa3xx_nand_host *host;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct pxa3xx_nand_platform_data *pdata;

	pdata = info->pdev->dev.platform_data;
	host = info->host[info->cs];
	mtd = host->mtd;
	chip = mtd->priv;
	addr_cycle = 0;
	exec_cmd = 1;
	if (info->cs != 0)
		ndcb0 = NDCB0_CSEL;
	else
		ndcb0 = 0;

	switch (command) {
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_RNDOUT:
		pxa3xx_set_datasize(info);
		chunks = (host->page_size < PAGE_CHUNK_SIZE) ?
			 1 : (host->page_size / PAGE_CHUNK_SIZE);
		if (host->ecc_strength > BCH_STRENGTH) {
			i = host->ecc_strength / BCH_STRENGTH;
			info->data_size /= i;
			ndcb0 |= NDCB0_LEN_OVRD;
			chunks *= i;
		}
		break;
	case NAND_CMD_READOOB:
		if (host->ecc_strength > BCH_STRENGTH) {
			printk(KERN_ERR "we don't support oob command if use"
					" 8bit per 512bytes ecc feature!!\n");
			BUG();
		}
	default:
		i = (uint32_t)(&info->retcode) - (uint32_t)info;
		memset(&info->retcode, 0, sizeof(*info) - i);
		break;
	}

	for (i = 0; i < CMD_POOL_SIZE; i++)
		info->ndcb0[i] = ndcb0;
	addr_cycle = NDCB0_ADDR_CYC(host->row_addr_cycles
				    + host->col_addr_cycles);

	info->total_cmds = 1;
	info->buf_start = column;
	switch (command) {
	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
	case NAND_CMD_READOOB:
		info->ecc_strength = host->ecc_strength;
		/* small page addr setting */
		if (unlikely(host->page_size < PAGE_CHUNK_SIZE)) {
			info->ndcb1 = ((page_addr & 0xFFFFFF) << 8)
					| (column & 0xFF);

			info->ndcb2 = 0;
		} else {
			info->ndcb1 = ((page_addr & 0xFFFF) << 16)
					| (column & 0xFFFF);

			if (page_addr & 0xFF0000)
				info->ndcb2 = (page_addr & 0xFF0000) >> 16;
			else
				info->ndcb2 = 0;
		}

		info->buf_count = mtd->writesize + mtd->oobsize;
		exec_cmd = 0;
		break;

	case NAND_CMD_RNDOUT:
		cmd = host->cmdset->read1;
		if (command == NAND_CMD_READOOB)
			info->buf_start = mtd->writesize + column;
		else
			info->buf_start = column;

		if (unlikely(host->page_size < PAGE_CHUNK_SIZE)
		    || !(pdata->attr & NAKED_CMD)) {
			if (unlikely(host->page_size < PAGE_CHUNK_SIZE))
				info->ndcb0[0] |= NDCB0_CMD_TYPE(0)
						| addr_cycle
						| (cmd & NDCB0_CMD1_MASK);
			else
				info->ndcb0[0] |= NDCB0_CMD_TYPE(0)
						| NDCB0_DBC
						| addr_cycle
						| cmd;
			break;
		}
		i = 0;
		info->ndcb0[0] &= ~NDCB0_LEN_OVRD;
		info->ndcb0[i++] |= NDCB0_CMD_XTYPE(0x6)
				| NDCB0_CMD_TYPE(0)
				| NDCB0_DBC
				| NDCB0_NC
				| addr_cycle
				| cmd;

		ndcb0 = info->ndcb0[i] | NDCB0_CMD_XTYPE(0x5) | NDCB0_NC;
		info->total_cmds = chunks + i;
		for (; i <= info->total_cmds - 1; i++)
			info->ndcb0[i] = ndcb0;
		info->ndcb0[info->total_cmds - 1] &= ~NDCB0_NC;

		/* we should wait RnB go high again before read out data */
		info->wait_ready[1] = 1;
		break;

	case NAND_CMD_PAGEPROG:
		if (info->command == NAND_CMD_NONE) {
			exec_cmd = 0;
			break;
		}

		cmd = host->cmdset->program;
		if (unlikely(host->page_size < PAGE_CHUNK_SIZE)
		    || !(pdata->attr & NAKED_CMD)) {
			info->ndcb0[0] |= NDCB0_CMD_TYPE(0x1)
					| NDCB0_AUTO_RS
					| NDCB0_ST_ROW_EN
					| NDCB0_DBC
					| cmd
					| addr_cycle;
			break;
		}

		info->total_cmds = chunks + 1;
		info->ndcb0[0] |= NDCB0_CMD_XTYPE(0x4)
				| NDCB0_CMD_TYPE(0x1)
				| NDCB0_NC
				| NDCB0_AUTO_RS
				| (cmd & NDCB0_CMD1_MASK)
				| addr_cycle;

		for (i = 1; i < chunks; i++)
			info->ndcb0[i] |= NDCB0_CMD_XTYPE(0x5)
					| NDCB0_NC
					| NDCB0_AUTO_RS
					| (cmd & NDCB0_CMD1_MASK)
					| NDCB0_CMD_TYPE(0x1);

		info->ndcb0[chunks] |= NDCB0_CMD_XTYPE(0x3)
					| NDCB0_CMD_TYPE(0x1)
					| NDCB0_ST_ROW_EN
					| NDCB0_DBC
					| (cmd & NDCB0_CMD2_MASK)
					| NDCB0_CMD1_MASK;
		info->ndcb0[chunks] &= ~NDCB0_LEN_OVRD;
		/*
		 * we should wait for RnB goes high which
		 * indicate the data has been written succesfully
		 */
		info->wait_ready[info->total_cmds] = 1;
		break;

	case NAND_CMD_READID:
		cmd = host->cmdset->read_id;
		info->buf_count = host->read_id_bytes;
		info->data_buff = (unsigned char *)chip->buffers->databuf;
		info->ndcb0[0] |= NDCB0_CMD_TYPE(3)
				| NDCB0_ADDR_CYC(1)
				| cmd;

		info->data_size = 8;
		break;
	case NAND_CMD_STATUS:
		cmd = host->cmdset->read_status;
		info->buf_count = 1;
		info->data_buff = (unsigned char *)chip->buffers->databuf;
		info->ndcb0[0] |= NDCB0_CMD_TYPE(4)
				| NDCB0_ADDR_CYC(1)
				| cmd;

		info->data_size = 8;
		break;

	case NAND_CMD_ERASE1:
		cmd = host->cmdset->erase;
		info->ndcb0[0] |= NDCB0_CMD_TYPE(2)
				| NDCB0_AUTO_RS
				| NDCB0_ADDR_CYC(3)
				| NDCB0_DBC
				| cmd;
		info->ndcb1 = page_addr;
		info->ndcb2 = 0;

		break;
	case NAND_CMD_RESET:
		cmd = host->cmdset->reset;
		info->ndcb0[0] |= NDCB0_CMD_TYPE(5)
				| cmd;
		info->wait_ready[1] = 1;

		break;

	case NAND_CMD_ERASE2:
		exec_cmd = 0;
		break;

	default:
		exec_cmd = 0;
		dev_err(&info->pdev->dev, "non-supported command %x\n",
				command);
		break;
	}

	info->command = command;
	return exec_cmd;
}

static void pxa3xx_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
				int column, int page_addr)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	int ret, exec_cmd;
#ifdef CONFIG_PXA3XX_BBM
	struct pxa3xx_bbm *pxa3xx_bbm = mtd->bbm;
	loff_t addr;

	if (pxa3xx_bbm && (command == NAND_CMD_READOOB
			|| command == NAND_CMD_READ0
			|| command == NAND_CMD_SEQIN
			|| command == NAND_CMD_ERASE1)) {

		addr = (loff_t)page_addr << mtd->writesize_shift;
		addr = pxa3xx_bbm->search(mtd, addr);
		page_addr = addr >> mtd->writesize_shift;
	}
#endif

	/*
	 * if this is a x16 device ,then convert the input
	 * "byte" address into a "word" address appropriate
	 * for indexing a word-oriented device
	 */
	if (host->reg_ndcr & NDCR_DWIDTH_M)
		column /= 2;

	/*
	 * There may be different NAND chip hooked to
	 * different chip select, so check whether
	 * chip select has been changed, if yes, reset the timing
	 */
	if (info->cs != host->cs) {
		info->cs = host->cs;
		nand_writel(info, NDTR0CS0, host->ndtr0cs0);
		nand_writel(info, NDTR1CS0, host->ndtr1cs0);
	}

	info->state = STATE_PREPARED;
	exec_cmd = prepare_command_pool(info, command, column, page_addr);
	if (exec_cmd) {
		init_completion(&info->cmd_complete);
		pxa3xx_nand_start(info);

		if (!use_polling)
			ret = wait_for_completion_timeout(&info->cmd_complete,
					CHIP_DELAY_TIMEOUT);
		else
			ret = pxa3xx_nand_polling(info);
		if (!ret) {
			dev_err(&info->pdev->dev, "Wait time out!!!\n");
			/* Stop State Machine for next command cycle */
			pxa3xx_nand_stop(info);
		}
	}
	info->state = STATE_IDLE;
}

static void pxa3xx_nand_write_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, const uint8_t *buf)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	dma_addr_t mapped_addr = 0;

	if (is_buf_blank((uint8_t *)buf, mtd->writesize)) {
		if (is_buf_blank(info->oob_buff, mtd->oobsize)) {
			info->command = NAND_CMD_NONE;
			return;
		}
		/*
		 * For hamming ecc would generate ecc accroding
		 * to data part only, so that in case of we want to
		 * write oob first, then fill data part later, the
		 * second write would always fail as the ecc becomes
		 * all 0 in this case
		 */
		if (host->ecc_strength == 1)
			info->ecc_strength = 0;
	}

	if (use_dma) {
		mapped_addr = map_addr(info, (void *)buf,
				       mtd->writesize, DMA_TO_DEVICE);
		if (dma_mapping_error(&info->pdev->dev, mapped_addr))
			info->use_dma = 0;
		else {
			info->use_dma = 1;
			info->data_buff_phys = mapped_addr;
		}
	}

	info->data_buff = (uint8_t *)buf;
	info->oob_buff = chip->oob_poi;
}

static int pxa3xx_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	int status = 0;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0, page);
	memset(chip->buffers->databuf, 0xff, mtd->writesize);
	pxa3xx_nand_write_page_hwecc(mtd, chip, chip->buffers->databuf);
	/* Send command to program the OOB data */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}


static void nand_read_page(struct mtd_info *mtd, uint8_t *buf, int page)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct nand_chip *chip = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	dma_addr_t mapped_addr = 0;

	info->use_dma = use_dma;
	if (!buf)
		info->data_buff_phys = info->data_desc_addr + DMA_H_SIZE
				       + (NAND_MAX_OOBSIZE * 2);
	else if (use_dma) {
		mapped_addr = map_addr(info, (void *)buf,
				       mtd->writesize, DMA_FROM_DEVICE);
		if (dma_mapping_error(&info->pdev->dev, mapped_addr)) {
			info->use_dma = 0;
			mapped_addr = 0;
		}
		else
			info->data_buff_phys = mapped_addr;
	}

	info->data_buff = (buf) ? buf : chip->buffers->databuf;
	info->oob_buff = chip->oob_poi;

	pxa3xx_nand_cmdfunc(mtd, NAND_CMD_RNDOUT, 0, page);
	if (mapped_addr)
		unmap_addr(&info->pdev->dev, mapped_addr,
			   buf, mtd->writesize, DMA_FROM_DEVICE);
	if (info->retcode == ERR_SBERR) {
		switch (info->ecc_strength) {
		default:
			if (info->bad_count > BCH_THRESHOLD)
				mtd->ecc_stats.corrected +=
					(info->bad_count - BCH_THRESHOLD);
			break;
		case HAMMING_STRENGTH:
			mtd->ecc_stats.corrected++;
		case 0:
			break;
		}
	} else if (info->retcode == ERR_DBERR) {
		/*
		 * for blank page (all 0xff), HW will calculate its ECC as
		 * 0, which is different from the ECC information within
		 * OOB, ignore such double bit errors
		 */
		if (is_buf_blank(info->data_buff, mtd->writesize))
			info->retcode = ERR_NONE;
		else
			mtd->ecc_stats.failed++;
	}
}

static int pxa3xx_nand_read_page_hwecc(struct mtd_info *mtd,
		struct nand_chip *chip, uint8_t *buf, int page)
{
	nand_read_page(mtd, buf, page);
	return 0;
}

static int pxa3xx_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
		int page, int sndcmd)
{
	if (sndcmd) {
		pxa3xx_nand_cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
		nand_read_page(mtd, NULL, page);
	}
	return 0;
}

static uint8_t pxa3xx_nand_read_byte(struct mtd_info *mtd)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	char retval = 0xFF;

	if (info->buf_start < info->buf_count)
		/* Has just send a new command? */
		retval = info->data_buff[info->buf_start++];

	return retval;
}

static u16 pxa3xx_nand_read_word(struct mtd_info *mtd)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	u16 retval = 0xFFFF;

	if (!(info->buf_start & 0x01) && info->buf_start < info->buf_count) {
		retval = *((u16 *)(info->data_buff+info->buf_start));
		info->buf_start += 2;
	}
	return retval;
}

static void pxa3xx_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(buf, info->data_buff + info->buf_start, real_len);
	info->buf_start += real_len;
}

static void pxa3xx_nand_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	int real_len = min_t(size_t, len, info->buf_count - info->buf_start);

	memcpy(info->data_buff + info->buf_start, buf, real_len);
	info->buf_start += real_len;
}

static int pxa3xx_nand_verify_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	return 0;
}

static void pxa3xx_nand_select_chip(struct mtd_info *mtd, int chip)
{
	return;
}

static int pxa3xx_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;

	if ((info->command == NAND_CMD_PAGEPROG) && info->use_dma)
		unmap_addr(&info->pdev->dev, info->data_buff_phys,
			   info->data_buff, mtd->writesize, DMA_TO_DEVICE);

	/* pxa3xx_nand_send_command has waited for command complete */
	if (this->state == FL_WRITING || this->state == FL_ERASING) {
		if (info->retcode == ERR_NONE)
			return 0;
		else {
			/*
			 * any error make it return 0x01 which will tell
			 * the caller the erase and write fail
			 */
			return 0x01;
		}
	}

	return 0;
}

static int pxa3xx_nand_config_flash(struct pxa3xx_nand_info *info,
				    const struct pxa3xx_nand_flash *f)
{
	struct platform_device *pdev = info->pdev;
	struct pxa3xx_nand_platform_data *pdata = pdev->dev.platform_data;
	struct pxa3xx_nand_host *host = info->host[info->cs];
	uint32_t ndcr = 0x0; /* enable all interrupts */

	if (f->page_size > PAGE_CHUNK_SIZE && !(pdata->attr & NAKED_CMD)) {
		dev_err(&pdev->dev, "Your controller don't support 4k or "
			"larger page NAND for don't support naked command\n");
		return -EINVAL;
	}

	if (f->flash_width != 16 && f->flash_width != 8) {
		dev_err(&pdev->dev, "Only support 8bit and 16 bit!\n");
		return -EINVAL;
	}

	if (f->ecc_strength != 0 && f->ecc_strength != HAMMING_STRENGTH
	    && (f->ecc_strength % BCH_STRENGTH != 0)) {
		printk(KERN_ERR "ECC strength definition error, please recheck!!\n");
		return -EINVAL;
	}
	host->ecc_strength = f->ecc_strength;
	/* calculate flash information */
	host->cmdset = &default_cmdset;
	host->page_size = f->page_size;
	host->read_id_bytes = (f->page_size >= 2048) ? 4 : 2;

	/* calculate addressing information */
	host->col_addr_cycles = (f->page_size >= 2048) ? 2 : 1;

	if (f->num_blocks * f->page_per_block > 65536)
		host->row_addr_cycles = 3;
	else
		host->row_addr_cycles = 2;

	ndcr |= (pdata->attr & ARBI_EN) ? NDCR_ND_ARB_EN : 0;
	ndcr |= (pdata->attr & FORCE_CS) ? NDCR_FORCE_CSX : 0;
	ndcr |= (host->col_addr_cycles == 2) ? NDCR_RA_START : 0;
	ndcr |= (f->flash_width == 16) ? NDCR_DWIDTH_M : 0;
	ndcr |= (f->dfc_width == 16) ? NDCR_DWIDTH_C : 0;

	switch (f->page_per_block) {
	case 32:
		ndcr |= NDCR_PG_PER_BLK(0x0);
		break;
	case 128:
		ndcr |= NDCR_PG_PER_BLK(0x1);
		break;
	case 256:
		ndcr |= NDCR_PG_PER_BLK(0x3);
		break;
	case 64:
	default:
		ndcr |= NDCR_PG_PER_BLK(0x2);
		break;
	}

	if (f->page_size >= 2048)
		ndcr |= NDCR_PAGE_SZ;

	ndcr |= NDCR_RD_ID_CNT(host->read_id_bytes);
	/* only enable spare area when ecc strength is lower than 8bits */
	if (f->ecc_strength <= BCH_STRENGTH)
		ndcr |= NDCR_SPARE_EN;

	host->reg_ndcr = ndcr;

	pxa3xx_nand_set_timing(host, f->timing);
	return 0;
}

static int pxa3xx_nand_detect_config(struct pxa3xx_nand_info *info)
{
	/*
	 * We set 0 by hard coding here, for we don't support CONFIG_KEEP
	 * when there is more than one chip attached to the controller
	 */
	struct pxa3xx_nand_host *host = info->host[0];
	uint32_t ndcr = nand_readl(info, NDCR), ndeccctrl;

	ndeccctrl = nand_readl(info, NDECCCTRL);
	if (ndcr & NDCR_PAGE_SZ) {
		host->page_size = 2048;
		host->read_id_bytes = 4;
	} else {
		host->page_size = 512;
		host->read_id_bytes = 2;
	}

	host->reg_ndcr = ndcr & ~NDCR_INT_MASK;
	host->ecc_strength = (ndeccctrl & NDECCCTRL_BCH_EN) ?
			     BCH_STRENGTH : HAMMING_STRENGTH;
	host->cmdset = &default_cmdset;

	host->ndtr0cs0 = nand_readl(info, NDTR0CS0);
	host->ndtr1cs0 = nand_readl(info, NDTR1CS0);

	return 0;
}

static int pxa3xx_nand_init_buff(struct pxa3xx_nand_info *info)
{
	struct platform_device *pdev = info->pdev;

	info->data_desc = dma_alloc_coherent(&pdev->dev,
			DMA_H_SIZE + sizeof(struct nand_buffers),
			&info->data_desc_addr, GFP_KERNEL);
	if (info->data_desc == NULL) {
		dev_err(&pdev->dev, "failed to allocate dma buffer\n");
		return -ENOMEM;
	}

	if (use_dma == 0)
		return 0;

	info->data_dma_ch = pxa_request_dma("nand-data", DMA_PRIO_LOW,
				pxa3xx_nand_data_dma_irq, info);
	if (info->data_dma_ch < 0) {
		dev_err(&pdev->dev, "failed to request data dma\n");
		dma_free_coherent(&pdev->dev,
				DMA_H_SIZE + sizeof(struct nand_buffers),
				info->data_desc, info->data_desc_addr);
		return info->data_dma_ch;
	}

	return 0;
}

static int pxa3xx_nand_sensing(struct pxa3xx_nand_info *info)
{
	struct mtd_info *mtd;
	int ret;
	mtd = info->host[info->cs]->mtd;
	/* use the common timing to make a try */
	ret = pxa3xx_nand_config_flash(info, &builtin_flash_types[0]);
	if (ret)
		return ret;

	pxa3xx_nand_cmdfunc(mtd, NAND_CMD_RESET, 0, 0);
	if (info->is_ready)
		return 0;

	return -ENODEV;
}

static int pxa3xx_nand_scan(struct mtd_info *mtd)
{
	struct pxa3xx_nand_host *host = mtd->priv;
	struct pxa3xx_nand_info *info = host->info_data;
	struct platform_device *pdev = info->pdev;
	struct pxa3xx_nand_platform_data *pdata = pdev->dev.platform_data;
	struct nand_flash_dev pxa3xx_flash_ids[2], *def = NULL;
	const struct pxa3xx_nand_flash *f = NULL;
	struct nand_chip *chip = mtd->priv;
	uint16_t *id;
	uint64_t chipsize;
	int i, ret, num;

	if ((pdata->attr & CONFIG_KEEP) && !pxa3xx_nand_detect_config(info))
		goto KEEP_CONFIG;

	ret = pxa3xx_nand_sensing(info);
	if (ret) {
		dev_info(&info->pdev->dev, "There is no chip on cs %d!\n",
			 info->cs);

		return ret;
	}

	chip->cmdfunc(mtd, NAND_CMD_READID, 0, 0);
	id = (uint16_t *)(info->data_buff);
	if (id[0] != 0)
		dev_info(&info->pdev->dev, "Detect a flash id %x:%x\n",
				id[0], id[1]);
	else {
		dev_warn(&info->pdev->dev,
			 "Read out ID 0, potential timing set wrong!!\n");

		return -EINVAL;
	}

	num = ARRAY_SIZE(builtin_flash_types) + pdata->num_flash - 1;
	for (i = 0; i < num; i++) {
		if (i < pdata->num_flash)
			f = pdata->flash + i;
		else
			f = &builtin_flash_types[i - pdata->num_flash + 1];

		/* find the chip in default list */
		if ((f->chip_id == id[0]) && ((f->ext_id & id[1]) == id[1]))
			break;
	}

	if (i >= (ARRAY_SIZE(builtin_flash_types) + pdata->num_flash - 1)) {
		dev_err(&info->pdev->dev, "ERROR!! flash not defined!!!\n");

		return -EINVAL;
	}

	ret = pxa3xx_nand_config_flash(info, f);
	if (ret) {
		dev_err(&info->pdev->dev, "ERROR! Configure failed\n");
		return ret;
	}

	pxa3xx_flash_ids[0].name = f->name;
	pxa3xx_flash_ids[0].id = (f->chip_id >> 8) & 0xffff;
	pxa3xx_flash_ids[0].pagesize = f->page_size;
	chipsize = (uint64_t)f->num_blocks * f->page_per_block * f->page_size;
	pxa3xx_flash_ids[0].chipsize = chipsize >> 20;
	pxa3xx_flash_ids[0].erasesize = f->page_size * f->page_per_block;
	pxa3xx_flash_ids[1].name = NULL;
	def = pxa3xx_flash_ids;

	if (f->ecc_strength > 1) {
		switch (f->page_size) {
		case 2048:
			chip->ecc.layout = &bch_nand_oob_64;
			break;
		case 4096:
			chip->ecc.layout = &bch_nand_oob_128;
			break;
		default:
			BUG();
		}
	}


KEEP_CONFIG:
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.size = host->page_size;
	chip->ecc.strength = 1;

	chip->options = NAND_NO_AUTOINCR;
	chip->options |= NAND_NO_READRDY;
	chip->options |= NAND_OWN_BUFFERS;
#ifdef CONFIG_PXA3XX_BBM
	chip->options |= BBT_RELOCATION_IFBAD;
#endif
	if (host->reg_ndcr & NDCR_DWIDTH_M)
		chip->options |= NAND_BUSWIDTH_16;
	if (def)
		def->options = chip->options;

	if (nand_scan_ident(mtd, 1, def))
		return -ENODEV;
	/* calculate addressing information */
	if (mtd->writesize >= 2048)
		host->col_addr_cycles = 2;
	else
		host->col_addr_cycles = 1;

	info->oob_buff = info->data_buff + mtd->writesize;
	if ((mtd->size >> chip->page_shift) > 65536)
		host->row_addr_cycles = 3;
	else
		host->row_addr_cycles = 2;

	mtd->name = mtd_names[0];
	return nand_scan_tail(mtd);
}

static int alloc_nand_resource(struct platform_device *pdev)
{
	struct pxa3xx_nand_platform_data *pdata;
	struct pxa3xx_nand_info *info;
	struct pxa3xx_nand_host *host;
	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct resource *r;
	int ret, irq, cs;

	pdata = pdev->dev.platform_data;
	info = kzalloc(sizeof(*info) + (sizeof(*mtd) +
		       sizeof(*host)) * pdata->num_cs, GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	spin_lock_init(&info->controller.lock);
	init_waitqueue_head(&info->controller.wq);
	info->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to get nand clock\n");
		ret = PTR_ERR(info->clk);
		goto fail_free_mtd;
	}
	clk_enable(info->clk);

	r = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no resource defined for data DMA\n");
		ret = -ENXIO;
		goto fail_put_clk;
	}
	info->drcmr_dat = r->start;

	r = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (r == NULL) {
		dev_err(&pdev->dev, "no resource defined for command DMA\n");
		ret = -ENXIO;
		goto fail_put_clk;
	}
	info->drcmr_cmd = r->start;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		ret = -ENXIO;
		goto fail_put_clk;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no IO memory resource defined\n");
		ret = -ENODEV;
		goto fail_put_clk;
	}

	r = request_mem_region(r->start, resource_size(r), pdev->name);
	if (r == NULL) {
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto fail_put_clk;
	}

	info->mmio_base = ioremap(r->start, resource_size(r));
	if (info->mmio_base == NULL) {
		dev_err(&pdev->dev, "ioremap() failed\n");
		ret = -ENODEV;
		goto fail_free_res;
	}
	info->mmio_phys = r->start;

	ret = pxa3xx_nand_init_buff(info);
	if (ret)
		goto fail_free_io;

	for (cs = 0; cs < pdata->num_cs; cs++) {
		mtd = (struct mtd_info *)((unsigned int)&info[1] +
		      (sizeof(*mtd) + sizeof(*host)) * cs);
		chip = (struct nand_chip *)(&mtd[1]);
		host = (struct pxa3xx_nand_host *)chip;
		info->host[cs] = host;
		host->mtd = mtd;
		host->cs = cs;
		host->info_data = info;
		mtd->priv = host;
		mtd->owner = THIS_MODULE;

		chip->buffers = (struct nand_buffers *)
				((void *)info->data_desc + DMA_H_SIZE);
		chip->ecc.read_page	= pxa3xx_nand_read_page_hwecc;
		chip->ecc.read_page_raw = pxa3xx_nand_read_page_hwecc;
		chip->ecc.read_oob	= pxa3xx_nand_read_oob;
		chip->ecc.write_page	= pxa3xx_nand_write_page_hwecc;
		chip->ecc.write_page_raw = pxa3xx_nand_write_page_hwecc;
		chip->ecc.write_oob     = pxa3xx_nand_write_oob;
		chip->controller        = &info->controller;
		chip->waitfunc		= pxa3xx_nand_waitfunc;
		chip->select_chip	= pxa3xx_nand_select_chip;
		chip->cmdfunc		= pxa3xx_nand_cmdfunc;
		chip->read_word		= pxa3xx_nand_read_word;
		chip->read_byte		= pxa3xx_nand_read_byte;
		chip->read_buf		= pxa3xx_nand_read_buf;
		chip->write_buf		= pxa3xx_nand_write_buf;
		chip->verify_buf	= pxa3xx_nand_verify_buf;
#ifdef CONFIG_PXA3XX_BBM
		chip->scan_bbt		= pxa3xx_scan_bbt;
		chip->block_markbad	= pxa3xx_block_markbad;
		chip->block_bad		= pxa3xx_block_bad;
#endif
	}

	/* initialize all interrupts to be disabled */
	disable_int(info, NDCR_INT_MASK);

	ret = request_irq(irq, pxa3xx_nand_irq, IRQF_DISABLED,
			  pdev->name, info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto fail_free_buf;
	}

	platform_set_drvdata(pdev, info);

	return 0;

fail_free_buf:
	free_irq(irq, info);
	if (use_dma) {
		pxa_free_dma(info->data_dma_ch);
		dma_free_coherent(&pdev->dev,
				DMA_H_SIZE + sizeof(struct nand_buffers),
				info->data_desc, info->data_desc_addr);
	} else
		kfree(info->data_buff);
fail_free_io:
	iounmap(info->mmio_base);
fail_free_res:
	release_mem_region(r->start, resource_size(r));
fail_put_clk:
	clk_disable(info->clk);
	clk_put(info->clk);
fail_free_mtd:
	kfree(info);
	return ret;
}

static int pxa3xx_nand_remove(struct platform_device *pdev)
{
	struct pxa3xx_nand_info *info = platform_get_drvdata(pdev);
	struct pxa3xx_nand_platform_data *pdata;
	struct resource *r;
	int irq, cs;

	if (!info)
		return 0;

	pdata = pdev->dev.platform_data;
	platform_set_drvdata(pdev, NULL);

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0)
		free_irq(irq, info);
	if (use_dma) {
		pxa_free_dma(info->data_dma_ch);
		dma_free_coherent(&pdev->dev,
				DMA_H_SIZE + sizeof(struct nand_buffers),
				info->data_desc, info->data_desc_addr);
	} else
		kfree(info->data_buff);

	iounmap(info->mmio_base);
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(r->start, resource_size(r));

	clk_disable(info->clk);
	clk_put(info->clk);

	for (cs = 0; cs < pdata->num_cs; cs++) {
		struct mtd_info *mtd = info->host[cs]->mtd;
#ifdef CONFIG_PXA3XX_BBM
		if (mtd->bbm)
			((struct pxa3xx_bbm *)mtd->bbm)->uninit(mtd);
#endif
		nand_release(mtd);
	}
	kfree(info);
	return 0;
}

static int pxa3xx_nand_probe(struct platform_device *pdev)
{
	struct pxa3xx_nand_platform_data *pdata;
	struct pxa3xx_nand_info *info;
	struct mtd_info *mtd;
	int ret, cs, probe_success;
#ifdef CONFIG_PXA3XX_BBM
	loff_t reserved_sz;
#endif

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -ENODEV;
	}

	if (pdata->attr & DMA_DIS)
		use_dma = 0;

	if (pdata->attr & POLLING)
		use_polling = 1;

	ret = alloc_nand_resource(pdev);
	if (ret) {
		dev_err(&pdev->dev, "alloc nand resource failed\n");
		return ret;
	}

	info = platform_get_drvdata(pdev);
	probe_success = 0;
	for (cs = 0; cs < pdata->num_cs; cs++) {
		info->cs = cs;
		mtd = info->host[cs]->mtd;
		ret = pxa3xx_nand_scan(mtd);
		if (ret) {
			dev_warn(&pdev->dev, "failed to scan nand at cs %d\n",
				cs);
			continue;
		}

#ifdef CONFIG_PXA95x_SUSPEND
		mtd->_suspend = NULL;
		mtd->_resume = NULL;
#endif

#ifdef CONFIG_PXA3XX_BBM
		reserved_sz =
			((struct pxa3xx_bbm *)mtd->bbm)->reserved_sz(mtd);
#endif
		ret = mtd_device_parse_register(mtd, NULL,
						NULL, pdata->parts[cs],
						pdata->nr_parts[cs]);
		if (!ret)
			probe_success = 1;
#ifdef CONFIG_PXA3XX_BBM
		mtd->size += reserved_sz;
#endif
	}

	if (!probe_success) {
		pxa3xx_nand_remove(pdev);
		return -ENODEV;
	}

	return 0;
}

#if (defined CONFIG_PM) && (!defined CONFIG_PXA95x_SUSPEND)
static int pxa3xx_nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pxa3xx_nand_info *info = platform_get_drvdata(pdev);
	struct pxa3xx_nand_platform_data *pdata;
	struct mtd_info *mtd;
	int cs;

	pdata = pdev->dev.platform_data;
	if (info->state) {
		dev_err(&pdev->dev, "driver busy, state = %d\n", info->state);
		return -EAGAIN;
	}

	for (cs = 0; cs < pdata->num_cs; cs++) {
		mtd = info->host[cs]->mtd;
		mtd_suspend(mtd);
	}

	return 0;
}

static int pxa3xx_nand_resume(struct platform_device *pdev)
{
	struct pxa3xx_nand_info *info = platform_get_drvdata(pdev);
	struct pxa3xx_nand_platform_data *pdata;
	struct mtd_info *mtd;
	int cs;

	pdata = pdev->dev.platform_data;
	/* We don't want to handle interrupt without calling mtd routine */
	disable_int(info, NDCR_INT_MASK);

	/*
	 * Directly set the chip select to a invalid value,
	 * then the driver would reset the timing according
	 * to current chip select at the beginning of cmdfunc
	 */
	info->cs = 0xff;

	/*
	 * As the spec says, the NDSR would be updated to 0x1800 when
	 * doing the nand_clk disable/enable.
	 * To prevent it damaging state machine of the driver, clear
	 * all status before resume
	 */
	nand_writel(info, NDSR, NDSR_MASK);
	for (cs = 0; cs < pdata->num_cs; cs++) {
		mtd = info->host[cs]->mtd;
		mtd_resume(mtd);
	}

	return 0;
}
#else
#define pxa3xx_nand_suspend	NULL
#define pxa3xx_nand_resume	NULL
#endif

static struct platform_driver pxa3xx_nand_driver = {
	.driver = {
		.name	= "pxa3xx-nand",
	},
	.probe		= pxa3xx_nand_probe,
	.remove		= pxa3xx_nand_remove,
	.suspend	= pxa3xx_nand_suspend,
	.resume		= pxa3xx_nand_resume,
};

module_platform_driver(pxa3xx_nand_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PXA3xx NAND controller driver");
