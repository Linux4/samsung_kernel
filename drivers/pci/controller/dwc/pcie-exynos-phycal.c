/*
 * PCIe phy driver for Samsung
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/exynos-pci-noti.h>
#include <linux/regmap.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#include <crypto/hash.h>
#include <linux/platform_device.h>

#include "pcie-exynos-phycal_common.h"
#include "pcie-exynos-phycal_data.h"

#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"

#if IS_ENABLED(CONFIG_PCI_EXYNOS_PHYCAL_DEBUG)
#define PHYCAL_DEBUG 1
#else
#define PHYCAL_DEBUG 0
#endif

/* PHY all power down clear */
void exynos_pcie_rc_phy_all_pwrdn_clear(struct exynos_pcie *exynos_pcie, int ch_num)
{

	pcie_err(exynos_pcie, "[%s] PWRDN CLR, CH#%d\n",
			__func__, exynos_pcie->ch_num);
	exynos_phycal_seq(exynos_pcie->phy->pcical_lst->pwrdn_clr,
			exynos_pcie->phy->pcical_lst->pwrdn_clr_size,
			exynos_pcie->ep_device_type);
}

/* PHY all power down */
void exynos_pcie_rc_phy_all_pwrdn(struct exynos_pcie *exynos_pcie, int ch_num)
{
	pcie_err(exynos_pcie, "[%s] PWRDN, CH#%d\n",
			__func__, exynos_pcie->ch_num);
	exynos_phycal_seq(exynos_pcie->phy->pcical_lst->pwrdn,
			exynos_pcie->phy->pcical_lst->pwrdn_size,
			exynos_pcie->ep_device_type);
}

void exynos_pcie_rc_pcie_phy_config(struct exynos_pcie *exynos_pcie, int ch_num)
{
	pcie_err(exynos_pcie, "[%s] CONFIG: PCIe CH#%d PHYCAL %s \n",
			__func__, exynos_pcie->ch_num, exynos_pcie->phy->revinfo);

	exynos_phycal_seq(exynos_pcie->phy->pcical_lst->config,
			exynos_pcie->phy->pcical_lst->config_size,
			exynos_pcie->ep_device_type);
}

int exynos_pcie_rc_eom(struct device *dev, void *phy_base_regs)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	unsigned int val;
	unsigned int num_of_smpl;
	unsigned int lane_width = 1;
	unsigned int timeout;
	int i, ret;
	int test_cnt = 0;
	struct pcie_eom_result **eom_result;

	u32 phase_sweep = 0;
	u32 vref_sweep = 0;
	u32 err_cnt = 0;
	u32 err_cnt_13_8;
	u32 err_cnt_7_0;

	dev_info(dev, "[%s] START! \n", __func__);

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		dev_err(dev, "[%s] failed to get num of lane, lane width = 0\n", __func__);
		lane_width = 0;
	} else
		dev_info(dev, "[%s] num-lanes : %d\n", __func__, lane_width);

	/* eom_result[lane_num][test_cnt] */
	eom_result = kzalloc(sizeof(struct pcie_eom_result*) * lane_width, GFP_KERNEL);
	for (i = 0; i < lane_width; i ++) {
		eom_result[i] = kzalloc(sizeof(struct pcie_eom_result) *
				EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX, GFP_KERNEL);
	}
	if (eom_result == NULL) {
		return -ENOMEM;
	}
	exynos_pcie->eom_result = eom_result;

	num_of_smpl = 0xf;

	for (i = 0; i < lane_width; i++)
	{
		val = readl(phy_base_regs + RX_EFOM_MODE);
		val = val | (0x3 << 4);
		writel(val, phy_base_regs + RX_EFOM_MODE);

		writel(0x27, phy_base_regs + RX_SSLMS_ADAP_HOLD_PMAD);

		val = readl(phy_base_regs + RX_EFOM_MODE);
		val = val & ~(0x7 << 0);
		writel(val, phy_base_regs + RX_EFOM_MODE);
		val = readl(phy_base_regs + RX_EFOM_MODE);
		val = val | (0x4 << 0);
		writel(val, phy_base_regs + RX_EFOM_MODE);

		writel(0x0, phy_base_regs + RX_EFOM_NUM_OF_SAMPLE_13_8);
		writel(num_of_smpl, phy_base_regs + RX_EFOM_NUM_OF_SAMPLE_7_0);
		udelay(10);

		for (phase_sweep = 0; phase_sweep < PHASE_MAX; phase_sweep++)
		{
			val = readl(phy_base_regs + RX_EFOM_EOM_PH_SEL);
			val = val & ~(0xff << 0);
			writel(val, phy_base_regs + RX_EFOM_EOM_PH_SEL);
			val = readl(phy_base_regs + RX_EFOM_EOM_PH_SEL);
			val = val | (phase_sweep << 0);
			writel(val, phy_base_regs + RX_EFOM_EOM_PH_SEL);

			for (vref_sweep = 0; vref_sweep < VREF_MAX; vref_sweep++)
			{
				writel(vref_sweep, phy_base_regs + RX_EFOM_DFE_VREF_CTRL);
				val = readl(phy_base_regs + RX_EFOM_START);
				val = val | (1 << 0);
				writel(val, phy_base_regs + RX_EFOM_START);

				timeout = 0;
				do {
					if (timeout == 100) {
						dev_err(dev, "[%s] timeout happened \n", __func__);
						return false;
					}

					udelay(1);
					val = readl(phy_base_regs + RX_EFOM_DONE) & (0x1);

					timeout++;
				} while(val != 0x1);

				err_cnt_13_8 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_13_8) << 8;
				err_cnt_7_0 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_7_0);
				err_cnt = err_cnt_13_8 | err_cnt_7_0;

				//dev_dbg(dev, "[%s] %d,%d : %d %d %d\n",
				//		__func__, i, test_cnt, phase_sweep, vref_sweep, err_cnt);

				//save result
				eom_result[i][test_cnt].phase = phase_sweep;
				eom_result[i][test_cnt].vref = vref_sweep;
				eom_result[i][test_cnt].err_cnt = err_cnt;
				test_cnt++;

				val = readl(phy_base_regs + RX_EFOM_START);
				val = val & ~(1 << 0);
				writel(val, phy_base_regs + RX_EFOM_START);
				udelay(10);
			}
		}
		writel(0x21, phy_base_regs + RX_SSLMS_ADAP_HOLD_PMAD);
		writel(0x0, phy_base_regs + RX_EFOM_MODE);

		/* goto next lane */
		phy_base_regs += 0x1000;
		test_cnt = 0;
	}

	return 0;
}

void exynos_pcie_rc_phy_getva(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	int i;

	pcie_err(exynos_pcie, "[%s]\n", __func__);

	if (!phy->pcical_p2vmap->size) {
		pr_err("PCIE CH#%d hasn't p2vmap, ERROR!!!!\n", exynos_pcie->ch_num);
		return;
	}
	pcie_err(exynos_pcie, "PCIE CH#%d p2vmap size: %d\n", exynos_pcie->ch_num,
						phy->pcical_p2vmap->size);

	for (i = 0; i < phy->pcical_p2vmap->size; i++) {
		pcie_dbg(exynos_pcie, "[%d] pa: 0x%x\n", i, phy->pcical_p2vmap->p2vmap[i].pa);
		phy->pcical_p2vmap->p2vmap[i].va = ioremap(phy->pcical_p2vmap->p2vmap[i].pa, SZ_64K);
		if (phy->pcical_p2vmap->p2vmap[i].va == NULL) {
			pcie_err(exynos_pcie, "PCIE CH#%d p2vmap[%d] ioremap failed!!, ERROR!!!!\n",
								exynos_pcie->ch_num, i);
			return;
		}
	}
}

/* avoid checking rx elecidle when access DBI */
void exynos_pcie_rc_phy_check_rx_elecidle(void *phy_pcs_base_regs, int val, int ch_num)
{

}

/* PHY input clk change */
void exynos_pcie_rc_phy_input_clk_change(struct exynos_pcie *exynos_pcie, bool enable)
{

}

void exynos_pcie_rc_ia(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_phy *phy = exynos_pcie->phy;

	pcie_info(exynos_pcie, "[%s] Set I/A for refclk common voltage off ver.13\n", __func__);

	if (phy->pcical_lst->ia0_size > 0) {
		pcie_info(exynos_pcie, "[%s] IA0, CH#%d - sysreg offset : 0x%x\n",
			  __func__, exynos_pcie->ch_num, exynos_pcie->sysreg_ia0_sel);
		regmap_set_bits(exynos_pcie->sysreg, exynos_pcie->sysreg_ia0_sel,
				exynos_pcie->ch_num);
		exynos_phycal_seq(exynos_pcie->phy->pcical_lst->ia0,
				exynos_pcie->phy->pcical_lst->ia0_size,
				exynos_pcie->ep_device_type);
	}
	if (phy->pcical_lst->ia1_size > 0) {
		pcie_info(exynos_pcie, "[%s] IA1, CH#%d - sysreg offset : 0x%x\n",
			  __func__, exynos_pcie->ch_num, exynos_pcie->sysreg_ia1_sel);
		regmap_set_bits(exynos_pcie->sysreg, exynos_pcie->sysreg_ia1_sel,
				exynos_pcie->ch_num);
		exynos_phycal_seq(exynos_pcie->phy->pcical_lst->ia1,
				exynos_pcie->phy->pcical_lst->ia1_size,
				exynos_pcie->ep_device_type);
	}
	if (phy->pcical_lst->ia2_size > 0) {
		pcie_info(exynos_pcie, "[%s] IA2, CH#%d - sysreg offset : 0x%x\n",
			  __func__, exynos_pcie->ch_num, exynos_pcie->sysreg_ia2_sel);
		regmap_set_bits(exynos_pcie->sysreg, exynos_pcie->sysreg_ia2_sel,
				exynos_pcie->ch_num);
		exynos_phycal_seq(exynos_pcie->phy->pcical_lst->ia2,
				exynos_pcie->phy->pcical_lst->ia2_size,
				exynos_pcie->ep_device_type);
	}

}
EXPORT_SYMBOL(exynos_pcie_rc_ia);

static int exynos_pcie_rc_phy_phy2virt(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	int ret = 0;

	ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
			phy->pcical_lst->pwrdn, phy->pcical_lst->pwrdn_size);
	if (ret) {
		dev_err(pci->dev, "ERROR on PA2VA conversion, seq: pwrdn (i: %d)\n", ret);
		return ret;
	}
	ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
			phy->pcical_lst->pwrdn_clr, phy->pcical_lst->pwrdn_clr_size);
	if (ret) {
		dev_err(pci->dev, "ERROR on PA2VA conversion, seq: pwrdn_clr (i: %d)\n", ret);
		return ret;
	}
	ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
			phy->pcical_lst->config, phy->pcical_lst->config_size);
	if (ret) {
		dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
		return ret;
	}
	if (phy->pcical_lst->ia0_size > 0) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				phy->pcical_lst->ia0, phy->pcical_lst->ia0_size);
		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
			return ret;
		}
	} else {
		dev_err(pci->dev, "CH#%d IA0 is NULL!!\n", exynos_pcie->ch_num);
	}

	if (phy->pcical_lst->ia1_size > 0) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				phy->pcical_lst->ia1, phy->pcical_lst->ia1_size);
		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
			return ret;
		}
	} else {
		dev_err(pci->dev, "CH#%d IA1 is NULL!!\n", exynos_pcie->ch_num);
	}
	if (phy->pcical_lst->ia2_size > 0) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				phy->pcical_lst->ia2, phy->pcical_lst->ia2_size);
		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
			return ret;
		}
	} else {
		dev_err(pci->dev, "CH#%d IA2 is NULL!!\n", exynos_pcie->ch_num);
	}

	return ret;
}
static void exynos_pcie_rc_phy_cal_override(struct exynos_pcie *exynos_pcie,
				int id, int size, struct phycal_seq *memblock)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;

	switch (id) {
	case ID_PWRDN:
		phy->pcical_lst->pwrdn = memblock;
		phy->pcical_lst->pwrdn_size = size;
		break;
	case ID_PWRDN_CLR:
		phy->pcical_lst->pwrdn_clr = memblock;
		phy->pcical_lst->pwrdn_clr_size = size;
		break;
	case ID_CONFIG:
		phy->pcical_lst->config = memblock;
		phy->pcical_lst->config_size = size;
		break;
	case ID_IA0:
		phy->pcical_lst->ia0 = memblock;
		phy->pcical_lst->ia0_size = size;
		break;
	case ID_IA1:
		phy->pcical_lst->ia1 = memblock;
		phy->pcical_lst->ia1_size = size;
		break;
	case ID_IA2:
		phy->pcical_lst->ia2 = memblock;
		phy->pcical_lst->ia2_size = size;
		break;
	default:
		dev_err(pci->dev, "%s: Invalid ID (0x%x) !!!! \n", __func__, id);
	}

	return;
}

static int exynos_pcie_rc_phy_chksum_cmpr(struct exynos_pcie *exynos_pcie,
		unsigned int *memblock, char *chksum, int down_len)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	int y, success = 0;
	struct crypto_shash *shash = NULL;
	struct shash_desc *sdesc = NULL;
	char *result = NULL;

	dev_dbg(pci->dev, "++++++ BINARY CHKSUM ++++++\n");

	shash = crypto_alloc_shash("md5", 0, 0);
	if (IS_ERR(shash)) {
		dev_err(pci->dev, "md5 ERROR : shash alloc fail\n");
		return -EINVAL;
	}

	sdesc = devm_kzalloc(pci->dev,
			sizeof(struct shash_desc) + crypto_shash_descsize(shash),
			GFP_KERNEL);
	if (sdesc == NULL) {
		dev_err(pci->dev, "md5 ERROR : 'sdesc' alloc fail\n");
		return -ENOMEM;
	}

	result = devm_kzalloc(pci->dev,
			sizeof(struct shash_desc) + crypto_shash_descsize(shash),
			GFP_KERNEL);
	if (result == NULL) {
		dev_err(pci->dev, "md5 ERROR : 'result' alloc fail\n");
		return -ENOMEM;
	}

	sdesc->tfm = shash;
	success = crypto_shash_init(sdesc);
	if (success < 0) {
		dev_err(pci->dev, "md5 ERROR : cryto_shash_init fail(%d)\n", success);
	}

	success = crypto_shash_update(sdesc, (char*)memblock, (down_len - 24));
	if (success < 0) {
		dev_err(pci->dev, "md5 ERROR : cryto_shash_update fail(%d)\n", success);
	}

	success = crypto_shash_final(sdesc, result);
	if (success < 0) {
		dev_err(pci->dev, "md5 ERROR : cryto_shash_final fail(%d)\n", success);
	}

	y = crypto_shash_digestsize(sdesc->tfm);
	while(y--){
		dev_dbg(pci->dev, "(%d): %02x / %02x", y, *result, *chksum);
		if (*result != *chksum) {
			dev_err(pci->dev, "(%d): %02x / %02x", y, *result, *chksum);
			return -1;
		}
		result++;
		chksum++;
	}
	dev_info(pci->dev, "++ BINARY CHKSUM Compare DONE ++\n");

	return 0;
}

static int exynos_pcie_rc_phy_load_bin(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	struct file *fp;
	char *filepath = "/vendor/etc/pcie_phycal.bin";
	int file_size;
	int ret = 0;

	unsigned int *memblock = NULL;
	int down_len = 0;
	int copy_len = 0;
	int struct_size = 0;

	char *checksum;
	unsigned int magic_number, binsize;

	u32 header, id, ch;

	/* 1. file open */
	dev_info(pci->dev, "%s: PHYCAL Binary Load START(%s) \n", __func__, filepath);
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		dev_err(pci->dev, " fp ERROR!!\n");
		return -1;
	}
	if (!S_ISREG(file_inode(fp)->i_mode)) {
		dev_err(pci->dev, "%s: %s is not regular file\n", __func__, filepath);
		ret = -1;
		goto err;
	}
	file_size = i_size_read(file_inode(fp));
	if (file_size <= 0) {
		dev_err(pci->dev, "%s: %s file size invalid (%d)\n", __func__, filepath, file_size);
		ret = -1;
		goto err;
	}

	dev_err(pci->dev, "%s: opened file size is %d\n", __func__, file_size);

	/* 2. alloc buffer */
	memblock = devm_kzalloc(pci->dev, file_size, GFP_KERNEL);
	if (!memblock) {
		dev_err(pci->dev, "%s: file buffer allocation is failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	/* 3. Donwload image */
	down_len = kernel_read(fp, memblock, (size_t)file_size, &fp->f_pos);
	if (down_len != file_size) {
		dev_err(pci->dev, "%s: Image Download is failed, down_size: %d, file_size: %d\n",
					__func__, down_len, file_size);
		ret = -EIO;
		goto err;
	}

	/* 3-1. check Magic number */
	magic_number = *memblock;
	memblock++;
	copy_len += BIN_MAGIC_NUM_SIZE;
	if (BIN_MAGIC_NUMBER != magic_number){
		dev_err(pci->dev, "%s: Magic# : 0x%x / BinMagic# : 0x%x\n",
				__func__, BIN_MAGIC_NUMBER, magic_number);
		dev_err(pci->dev, "%s: It isn't PCIe PHY Binary!\n", __func__);
		ret = -1;
		goto err;
	}

	/* 3-2. check Binary Size */
	binsize = *memblock;
	memblock++;
	copy_len += BIN_TOTAL_SIZE;

	dev_err(pci->dev, "%s: DowloadSize : %d / BinSize# : %d\n",
			__func__, down_len, binsize);
	if (down_len != binsize){
		dev_err(pci->dev, "%s: PCIe PHY Binary Size is not matched!\n", __func__);
		ret = -1;
		goto err;
	}

	/*3-3. get checksum value in header */
	checksum = devm_kzalloc(pci->dev, BIN_CHKSUM_SIZE, GFP_KERNEL);
	memcpy(checksum, memblock, BIN_CHKSUM_SIZE);

	memblock += BIN_CHKSUM_SIZE / sizeof(*memblock);
	copy_len += BIN_CHKSUM_SIZE;

	/*3-4. compare checksum */
	ret = exynos_pcie_rc_phy_chksum_cmpr(exynos_pcie, memblock, checksum, down_len);
	if (ret) {
		dev_err(pci->dev, "CHECKSUM isn't Match!!!! \n");
		goto err;
	}

	/* 4. parse binary */
	phy->revinfo = (char*)memblock;
	memblock += BIN_REV_HEADER_SIZE / (sizeof(*memblock));
	copy_len += BIN_REV_HEADER_SIZE;
	dev_err(pci->dev, "%s: New Loaded Binary revision : %s\n", __func__, phy->revinfo);

	while (copy_len < down_len) {
		struct_size = ((*memblock) & BIN_SIZE_MASK);
		header = ((*memblock) & BIN_HEADER_MASK) >> 16;
		ch = (header & BIN_HEADER_CH_MASK) >> 8;
		id = (header & BIN_HEADER_ID_MASK);
		memblock++;
		copy_len += 4;

		dev_err(pci->dev, "%s: CH#%d / seq_id: 0x%x / size: 0x%x\n",
				__func__, ch, id, struct_size);
		if (exynos_pcie->ch_num == ch && struct_size > 0)
			exynos_pcie_rc_phy_cal_override(exynos_pcie, id, struct_size,
							(struct phycal_seq *)memblock);
		memblock += ((sizeof(struct phycal_seq) * struct_size) / sizeof(*memblock));
		copy_len += (sizeof(struct phycal_seq) * struct_size);
		dev_err(pci->dev, "%s: copy_len: %d/%d\n", __func__, copy_len, down_len);
	}
	dev_info(pci->dev, "%s: PHYCAL Binary Load END\n", __func__);

err:
	filp_close(fp, NULL);
	return ret;
}
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

int exynos_pcie_rc_phy_load(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	int ret;

	if (!PHYCAL_DEBUG) {
		dev_err(pci->dev, "%s: PHYCAL DEBUG IS DISABLED !! \n", __func__);
		return -1;
	}

	ret = exynos_pcie_rc_phy_load_bin(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: Load binary is failed, use default PHYCAL\n", __func__);
		return -1;
	}

	ret = exynos_pcie_rc_phy_phy2virt(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: PHY2VIRT is failed, use default PHYCAL\n", __func__);
		return ret;
	}

	return 0;

}
EXPORT_SYMBOL(exynos_pcie_rc_phy_load);

static int exynos_pcie_rc_phy_get_p2vmap(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	struct phycal_p2v_map *p2vmap_list;
	int i, r_cnt = 0;

	p2vmap_list = devm_kzalloc(pci->dev,
			sizeof(struct phycal_p2v_map) * pdev->num_resources,
			GFP_KERNEL);
	if (!p2vmap_list) {
		dev_err(pci->dev, "%s: p2vmap_list alloc is failed\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < pdev->num_resources; i++) {
		struct resource *r = &pdev->resource[i];

		if (unlikely(!r->name))
			continue;
		if (resource_type(r) == IORESOURCE_MEM) {
			r_cnt++;
			p2vmap_list[i].pa = r->start;
			dev_err(pci->dev, "%s\t: 0x%x\n", r->name, r->start);
		}
	}
	dev_err(pci->dev, "mem resource: %d (total: %d)\n", r_cnt, pdev->num_resources);
	phy->pcical_p2vmap->size = r_cnt;
	phy->pcical_p2vmap->p2vmap = p2vmap_list;

	return 0;
}

int exynos_pcie_rc_phy_init(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy;
	int ret;

	dev_info(pci->dev, "Initialize PHY functions.\n");

	phy = devm_kzalloc(pci->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(pci->dev, "%s: exynos_pcie_phy alloc is failed\n", __func__);
		return -ENOMEM;
	}

	phy->phy_ops.phy_check_rx_elecidle =
		exynos_pcie_rc_phy_check_rx_elecidle;
	phy->phy_ops.phy_all_pwrdn = exynos_pcie_rc_phy_all_pwrdn;
	phy->phy_ops.phy_all_pwrdn_clear = exynos_pcie_rc_phy_all_pwrdn_clear;
	phy->phy_ops.phy_config = exynos_pcie_rc_pcie_phy_config;
	phy->phy_ops.phy_eom = exynos_pcie_rc_eom;
	phy->phy_ops.phy_input_clk_change = exynos_pcie_rc_phy_input_clk_change;

	phy->pcical_lst = &pcical_list[exynos_pcie->ch_num];
	phy->revinfo = exynos_pcie_phycal_revinfo;

	exynos_pcie->phy = phy;

	phy->pcical_p2vmap = devm_kzalloc(pci->dev,
			sizeof(struct exynos_pcie_phycal_p2vmap), GFP_KERNEL);

	ret = exynos_pcie_rc_phy_get_p2vmap(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: Getting P2VMap is failed\n", __func__);
		return ret;
	}

	exynos_pcie_rc_phy_getva(exynos_pcie);

	ret = exynos_pcie_rc_phy_phy2virt(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: PHY2VIRT is failed\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_phy_init);

MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe Host PHY controller driver");
MODULE_LICENSE("GPL v2");
