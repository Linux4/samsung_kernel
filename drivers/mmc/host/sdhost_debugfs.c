/*
 *  linux/drivers/mmc/host/sdhost_debugfs.c - Secure Digital Host Controller Interface driver
 *
 *  Copyright (C) 2014-2014 Jason.Wu(Jishuang.Wu), All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */
#include <linux/debugfs.h>
#include <linux/delay.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>

#include "sdhost.h"
#include "sdhost_debugfs.h"

#define ELEMENT(v) {v,#v}
#define ELEMENT_NUM	24
struct {
	uint32_t bit;
	char *capsName;
} capsInfo[3][ELEMENT_NUM] = {
	{
		ELEMENT(MMC_CAP_4_BIT_DATA),
		    ELEMENT(MMC_CAP_MMC_HIGHSPEED),
		    ELEMENT(MMC_CAP_SD_HIGHSPEED),
		    ELEMENT(MMC_CAP_SDIO_IRQ),
		    ELEMENT(MMC_CAP_SPI),
		    ELEMENT(MMC_CAP_NEEDS_POLL),
		    ELEMENT(MMC_CAP_8_BIT_DATA),
		    ELEMENT(MMC_CAP_NONREMOVABLE),
		    ELEMENT(MMC_CAP_WAIT_WHILE_BUSY),
		    ELEMENT(MMC_CAP_ERASE),
		    ELEMENT(MMC_CAP_1_8V_DDR),
		    ELEMENT(MMC_CAP_1_2V_DDR),
		    ELEMENT(MMC_CAP_POWER_OFF_CARD),
		    ELEMENT(MMC_CAP_BUS_WIDTH_TEST),
		    ELEMENT(MMC_CAP_UHS_SDR12),
		    ELEMENT(MMC_CAP_UHS_SDR25),
		    ELEMENT(MMC_CAP_UHS_SDR50),
		    ELEMENT(MMC_CAP_UHS_SDR104),
		    ELEMENT(MMC_CAP_UHS_DDR50),
		    ELEMENT(MMC_CAP_DRIVER_TYPE_A),
		    ELEMENT(MMC_CAP_DRIVER_TYPE_C), ELEMENT(MMC_CAP_DRIVER_TYPE_D), ELEMENT(MMC_CAP_CMD23), ELEMENT(MMC_CAP_HW_RESET)
	}, {
		ELEMENT(MMC_CAP2_BOOTPART_NOACC),
		    ELEMENT(MMC_CAP2_CACHE_CTRL),
		    ELEMENT(MMC_CAP2_POWEROFF_NOTIFY),
		    ELEMENT(MMC_CAP2_NO_MULTI_READ),
		    ELEMENT(MMC_CAP2_NO_SLEEP_CMD),
		    ELEMENT(MMC_CAP2_HS200_1_8V_SDR),
		    ELEMENT(MMC_CAP2_HS200_1_2V_SDR),
		    ELEMENT(MMC_CAP2_HS200),
		    ELEMENT(MMC_CAP2_BROKEN_VOLTAGE),
		    ELEMENT(MMC_CAP2_DETECT_ON_ERR),
		    ELEMENT(MMC_CAP2_HC_ERASE_SZ),
		    ELEMENT(MMC_CAP2_CD_ACTIVE_HIGH),
		    ELEMENT(MMC_CAP2_RO_ACTIVE_HIGH),
		    ELEMENT(MMC_CAP2_PACKED_RD), ELEMENT(MMC_CAP2_PACKED_WR), ELEMENT(MMC_CAP2_PACKED_CMD), ELEMENT(MMC_CAP2_NO_PRESCAN_POWERUP)
	}, {
		ELEMENT(MMC_PM_KEEP_POWER), ELEMENT(MMC_PM_WAKE_SDIO_IRQ), ELEMENT(MMC_PM_IGNORE_PM_NOTIFY)

	}

};

static int sdhost_param_show(struct seq_file *s, void *data)
{
	struct sdhost_host *host = s->private;
	uint32_t i;

	seq_printf(s, "\n"
		   "ioaddr\t= 0x%p\n"
		   "irq\t= %d\n"
		   "deviceName\t= %s\n"
		   "detect_gpio\t= %d\n"
		   "SD_Pwr_Name\t= %s\n"
		   "1.8v signal_Name\t= %s\n"
		   "baseClk\t= %d\n"
		   "writeDelay\t= %d\n"
		   "readPosDelay\t= %d\n"
		   "readNegDelay\t= %d\n",
		   host->ioaddr, host->irq, host->deviceName, host->detect_gpio, host->SD_Pwr_Name,
		   host->_1_8V_signal_Name, host->base_clk, host->writeDelay, host->readPosDelay, host->readNegDelay);
	seq_printf(s, "OCR 0x%x\n", host->ocr_avail);
	for (i = 0; i < ELEMENT_NUM; i++) {
		if ((capsInfo[0][i].bit == (host->caps & capsInfo[0][i].bit)) && capsInfo[0][i].bit) {
			seq_printf(s, "caps:%s\n", capsInfo[0][i].capsName);
		}
	}
	for (i = 0; i < ELEMENT_NUM; i++) {
		if ((capsInfo[1][i].bit == (host->caps2 & capsInfo[1][i].bit)) && capsInfo[1][i].bit) {
			seq_printf(s, "caps2:%s\n", capsInfo[1][i].capsName);
		}
	}
	for (i = 0; i < ELEMENT_NUM; i++) {
		if ((capsInfo[2][i].bit == (host->pm_caps & capsInfo[2][i].bit)) && capsInfo[2][i].bit) {
			seq_printf(s, "pm_caps:%s\n", capsInfo[2][i].capsName);
		}
	}

	return 0;
}

static int sdhost_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhost_param_show, inode->i_private);
}

static const struct file_operations sdhost_param_fops = {
	.open = sdhost_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define SDHOST_ATTR(PARAM_NAME)	\
	static int sdhost_##PARAM_NAME##_get(void *data, u64 * val)\
	{\
		struct sdhost_host* host = data;\
		*val = (u64)host->PARAM_NAME;\
		return 0;\
	}\
	static int sdhost_##PARAM_NAME##_set(void *data, u64 val)\
	{\
		struct sdhost_host* host = data;\
		if(0x7F >= (uint32_t)val){\
			host->PARAM_NAME = (uint32_t)val;\
			_sdhost_set_delay(host->ioaddr,host->writeDelay,host->readPosDelay,host->readNegDelay);\
		}\
		return 0;\
	}\
	DEFINE_SIMPLE_ATTRIBUTE(sdhost_##PARAM_NAME##_fops, sdhost_##PARAM_NAME##_get, sdhost_##PARAM_NAME##_set, "%llu\n");

SDHOST_ATTR(writeDelay);
SDHOST_ATTR(readPosDelay);
SDHOST_ATTR(readNegDelay);

void sdhost_add_debugfs(struct sdhost_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(host->deviceName, NULL);
	if (IS_ERR(root)) {
		/* Don't complain -- debugfs just isn't enabled */
		return;
	}
	if (!root) {
		return;
	}
	host->debugfs_root = root;

	if (!debugfs_create_file("basicResource", S_IRUSR, root, (void *)host, &sdhost_param_fops)) {
		goto err;
	}
	if (!debugfs_create_file("writeDelay", S_IRUSR | S_IWUSR, root, (void *)host, &sdhost_writeDelay_fops)) {
		goto err;
	}
	if (!debugfs_create_file("readPosDelay", S_IRUSR | S_IWUSR, root, (void *)host, &sdhost_readPosDelay_fops)) {
		goto err;
	}
	if (!debugfs_create_file("readNegDelay", S_IRUSR | S_IWUSR, root, (void *)host, &sdhost_readNegDelay_fops)) {
		goto err;
	}
	return;
err:
	debugfs_remove_recursive(root);
	host->debugfs_root = 0;
	return;
}

void dumpSDIOReg(struct sdhost_host *host)
{
	if (!host->mmc->card) {
		return;
	}
	printk(KERN_ERR "sdhost" ": =========== REGISTER DUMP (%s)===========\n", host->deviceName);

	printk(KERN_ERR "sdhost" ": Sys addr: 0x%08x | Version:  0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_SYS_ADDR),
			_sdhost_readl(host->ioaddr, SDHOST_8_SLT_INT_ST));
	printk(KERN_ERR "sdhost" ": Blk size: 0x%08x | Blk cnt:  0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_16_BLK_SIZE),
			_sdhost_readl(host->ioaddr, SDHOST_16_BLK_CNT));
	printk(KERN_ERR "sdhost" ": Argument: 0x%08x | Tr mode:  0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_ARG),
			_sdhost_readl(host->ioaddr, SDHOST_32_TR_MODE_AND_CMD));
	printk(KERN_ERR "sdhost" ": Present:  0x%08x | Host ctl: 0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_PRES_STATE),
			_sdhost_readl(host->ioaddr, SDHOST_8_HOST_CTRL));
	printk(KERN_ERR "sdhost" ": Blk gap:  0x%08x | Wake up:  0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_8_BLK_GAP),
			_sdhost_readl(host->ioaddr, SDHOST_8_WACKUP_CTRL));
	printk(KERN_ERR "sdhost" ": Clock:    0x%08x | Timeout:  0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_16_CLK_CTRL),
			_sdhost_readl(host->ioaddr, SDHOST_8_TIMEOUT));
	printk(KERN_ERR "sdhost" ": Int stat: 0x%08x | Int enab: 0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_INT_ST),
			_sdhost_readl(host->ioaddr, SDHOST_32_INT_ST_EN));
	printk(KERN_ERR "sdhost" ": Sig enab: 0x%08x | Acmd err: 0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_INT_SIG_EN),
			_sdhost_readl(host->ioaddr, SDHOST_16_ACMD_ERR));
	printk(KERN_ERR "sdhost" ": Resp 1:   0x%08x | Resp 0:   0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_RESPONSE + 0x4),
			_sdhost_readl(host->ioaddr, SDHOST_32_RESPONSE));
	printk(KERN_ERR "sdhost" ": Resp 3:   0x%08x | Resp 2:   0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_32_RESPONSE + 0xC),
			_sdhost_readl(host->ioaddr, SDHOST_32_RESPONSE + 0x8));
	printk(KERN_ERR "sdhost" ": Hostctl2: 0x%08x\n",
			_sdhost_readl(host->ioaddr, SDHOST_16_HOST_CTRL_2));

	printk(KERN_ERR "sdhost" ": ===========================================\n");
	//mdelay(100);
}
