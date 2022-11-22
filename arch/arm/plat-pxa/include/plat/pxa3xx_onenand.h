#ifndef __ASM_ARCH_PXA3XX_ONENAND_H
#define __ASM_ARCH_PXA3XX_ONENAND_H

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/onenand.h>

#ifdef CONFIG_PXA3XX_BBM
#include <plat/pxa3xx_bbm.h>
#endif

#define MAX_DMA_CHANNEL		4
#define ONENAND_RAM_SIZE        (1 << 11)
#define DMA_BUF_SIZE            2400
#define NUM_CHIP_SELECT		(2)

struct mtd_partition;
struct mtd_info;
struct pxa3xx_onenand_info {
	struct platform_device *pdev;
	struct mtd_info mtd;
	struct mtd_partition *parts;
	struct onenand_chip onenand;

	unsigned long phys_base;
	int channel_cnt;
	int dma_ch[MAX_DMA_CHANNEL];
	int dma_ch_done[MAX_DMA_CHANNEL];
	void *sram_addr;
	dma_addr_t sram_addr_phys;
	struct clk *smc_clk;
	struct pxa3xx_bbm *bbm;

	struct completion irq_done;
	struct completion dma_done;
};

/*
 * map_name:	the map probe function name
 * name:	flash device name (eg, as used with mtdparts=)
 * width:	width of mapped device
 * init:	method called at driver/device initialisation
 * exit:	method called at driver/device removal
 * set_vpp:	method called to enable or disable VPP
 * mmcontrol:	method called to enable or disable Sync. Burst Read in OneNAND
 * parts:	optional array of mtd_partitions for static partitioning
 * nr_parts:	number of mtd_partitions for static partitoning
 */
struct pxa3xx_onenand_platform_data {
	const char *map_name;
	const char *name;
	unsigned int width;
	int (*init) (void);
	void (*exit) (void);
	void (*set_vpp) (int on);
	void (*mmcontrol) (struct mtd_info *mtd, int sync_read);
	struct mtd_partition *parts;
	unsigned int nr_parts;
	int (*set_partition_info) (u32 flash_size, u32 page_size,
				   struct pxa3xx_onenand_platform_data *pdata);
};

#endif /* __ASM_ARCH_PXA3XX_NAND_H */
