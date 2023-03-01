/*
 * PCIe phy driver for Samsung EXYNOS9830
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
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
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif

/* avoid checking rx elecidle when access DBI */
void exynos_pcie_rc_phy_check_rx_elecidle(void *phy_pcs_base_regs, int val, int ch_num)
{
	//Todo need guide
}

/* PHY all power down */
void exynos_pcie_rc_phy_all_pwrdn(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void *phy_base_regs = exynos_pcie->phy_base;

	pr_info("%s: phybase + 0x400 : 0x23\n", __func__);
	writel(0x23, phy_base_regs + 0x400);
}

/* PHY all power down clear */
void exynos_pcie_rc_phy_all_pwrdn_clear(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;

	pr_info("%s: phybase + 0x400 : 0x0\n", __func__);
        writel(0x0, phy_base_regs + 0x400);
}

void exynos_pcie_rc_pcie_phy_otp_config(void *phy_base_regs, int ch_num)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#else
	return ;
#endif
}
#define LCPLL_REF_CLK_SEL	(0x3 << 4)
void exynos_pcie_rc_pcie_phy_config(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *elbi_base_regs = exynos_pcie->elbi_base;
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phy_pcs_base_regs = exynos_pcie->phy_pcs_base;
	void __iomem *sysreg_base_regs = exynos_pcie->sysreg_base;
	int chip_ver = exynos_pcie->chip_ver;

	u32 val;

	val = readl(sysreg_base_regs);

	/* pcs_g_rst */
	writel(0x1, elbi_base_regs + 0x1404);
	udelay(10);
	writel(0x0, elbi_base_regs + 0x1404);
	mdelay(1);
	writel(0x1, elbi_base_regs + 0x1404);
	udelay(10);

	writel(0x1, elbi_base_regs + 0x1400);
	udelay(10);
	writel(0x0, elbi_base_regs + 0x1400);
	mdelay(1);

	writel(0x1, elbi_base_regs + 0x1408);
	udelay(10);
	writel(0x0, elbi_base_regs + 0x1408);
	mdelay(1);

	if (chip_ver == 0) {
		/* for EVT0 */
		writel(0x50, phy_base_regs + 0x18);		// Common
		writel(0x33, phy_base_regs + 0x48);		//
		writel(0xB9, phy_base_regs + 0x54);
		writel(0x14, phy_base_regs + 0xB0);
		writel(0x50, phy_base_regs + 0xB8);		//
		writel(0x50, phy_base_regs + 0xE0);		//
		writel(0x00, phy_base_regs + 0x100);
		writel(0x8F, phy_base_regs + 0x788);

		writel(0x4E, phy_base_regs + 0x920);		//Lane0
		writel(0x7A, phy_base_regs + 0x92C);
		writel(0x4E, phy_base_regs + 0x93C);
		writel(0x98, phy_base_regs + 0x94C);
		writel(0x0C, phy_base_regs + 0x980);
		writel(0x00, phy_base_regs + 0x9BC);
		writel(0x03, phy_base_regs + 0xA6C);
		writel(0x3E, phy_base_regs + 0xA70);
		writel(0x08, phy_base_regs + 0xB40);
		writel(0x08, phy_base_regs + 0xB44);
		writel(0x08, phy_base_regs + 0xB48);
		writel(0x0A, phy_base_regs + 0xB4C);
		writel(0x04, phy_base_regs + 0xB50);
		writel(0x04, phy_base_regs + 0xB54);
		writel(0x05, phy_base_regs + 0xB58);
		writel(0x05, phy_base_regs + 0xB5C);
		writel(0x07, phy_base_regs + 0xB6C);
		writel(0x3F, phy_base_regs + 0xB70);
		writel(0x3F, phy_base_regs + 0xB74);
		writel(0x3F, phy_base_regs + 0xB78);
		writel(0x3F, phy_base_regs + 0xB7C);
		//writel( , phy_base_regs + ); 0xB80) = 0x00;
		writel(0x03, phy_base_regs + 0xB84);
		writel(0x01, phy_base_regs + 0xB88);
		writel(0x03, phy_base_regs + 0xB9C);
		writel(0x09, phy_base_regs + 0xC10);
		writel(0x3F, phy_base_regs + 0xCAC);
		writel(0x02, phy_base_regs + 0xDC4);

		writel(0x4E, phy_base_regs + 0x1120);		//Lane1
		writel(0x7A, phy_base_regs + 0x112C);
		writel(0x4E, phy_base_regs + 0x113C);
		writel(0x98, phy_base_regs + 0x114C);
		writel(0x0C, phy_base_regs + 0x1180);
		writel(0x00, phy_base_regs + 0x11BC);
		writel(0x03, phy_base_regs + 0x126C);
		writel(0x3E, phy_base_regs + 0x1270);
		writel(0x08, phy_base_regs + 0x1340);
		writel(0x08, phy_base_regs + 0x1344);
		writel(0x08, phy_base_regs + 0x1348);
		writel(0x0A, phy_base_regs + 0x134C);
		writel(0x04, phy_base_regs + 0x1350);
		writel(0x04, phy_base_regs + 0x1354);
		writel(0x05, phy_base_regs + 0x1358);
		writel(0x05, phy_base_regs + 0x135C);
		writel(0x07, phy_base_regs + 0x136C);
		writel(0x3F, phy_base_regs + 0x1370);
		writel(0x3F, phy_base_regs + 0x1374);
		writel(0x3F, phy_base_regs + 0x1378);
		writel(0x3F, phy_base_regs + 0x137C);
		//writel( , phy_base_regs + ); 0x1380) = 0x00;
		writel(0x03, phy_base_regs + 0x1384);
		writel(0x01, phy_base_regs + 0x1388);
		writel(0x03, phy_base_regs + 0x139C);
		writel(0x09, phy_base_regs + 0x1410);
		writel(0x3F, phy_base_regs + 0x14AC);
		writel(0x02, phy_base_regs + 0x15C4);

		//CTS add //Test
		writel(0x1F, phy_base_regs + 0x0818);
		writel(0x77, phy_base_regs + 0x0820);
		writel(0x1F, phy_base_regs + 0x1018);
		writel(0x77, phy_base_regs + 0x1020);
		writel(0x06, phy_base_regs + 0x0B4C);
		writel(0x03, phy_base_regs + 0x0B5C);
		writel(0x1C, phy_base_regs + 0x0CCC);
		writel(0x06, phy_base_regs + 0x134C);
		writel(0x03, phy_base_regs + 0x135C);
		writel(0x1C, phy_base_regs + 0x14CC);

		/* ical code */
		writel(0x76, phy_base_regs + 0x18);
	} else if (chip_ver == 1) {
		pr_info("[%s]## 9830 EVT1 GEN4 PHY\n", __func__);
		/* for EVT1 */
		writel(0x50, phy_base_regs + 0x18);
		writel(0x33, phy_base_regs + 0x48);
		writel(0x01, phy_base_regs + 0x68);
		writel(0x12, phy_base_regs + 0x70);
		writel(0x7F, phy_base_regs + 0x78);
		writel(0x0A, phy_base_regs + 0x84);
		writel(0x58, phy_base_regs + 0x88);
		writel(0x00, phy_base_regs + 0x8C);
		writel(0x21, phy_base_regs + 0x90);
		writel(0xB6, phy_base_regs + 0x104);
		writel(0x06, phy_base_regs + 0x458);	//only RC
		writel(0x38, phy_base_regs + 0x550);
		writel(0x34, phy_base_regs + 0x5B0);    //diff RC EP
		writel(0x01, phy_base_regs + 0x8D0);
		writel(0x30, phy_base_regs + 0x8F4);
		writel(0x60, phy_base_regs + 0x90C);
		writel(0x06, phy_base_regs + 0x9B4);
		writel(0x3F, phy_base_regs + 0x9C0);
		writel(0xFB, phy_base_regs + 0x9C4);
		writel(0x03, phy_base_regs + 0xA6C);
		writel(0x06, phy_base_regs + 0xB40);
		writel(0x06, phy_base_regs + 0xB44);
		writel(0x06, phy_base_regs + 0xB48);
		writel(0x06, phy_base_regs + 0xB4C);
		writel(0x00, phy_base_regs + 0xB50);
		writel(0x00, phy_base_regs + 0xB54);
		writel(0x00, phy_base_regs + 0xB58);
		writel(0x03, phy_base_regs + 0xB5C);
		writel(0x16, phy_base_regs + 0xC44);
		writel(0x16, phy_base_regs + 0xC48);
		writel(0x16, phy_base_regs + 0xC4C);
		writel(0x16, phy_base_regs + 0xC50);
		writel(0x0A, phy_base_regs + 0xC54);
		writel(0x0A, phy_base_regs + 0xC58);
		writel(0x0A, phy_base_regs + 0xC5C);
		writel(0x0A, phy_base_regs + 0xC60);
		writel(0x08, phy_base_regs + 0xC64);
		writel(0x08, phy_base_regs + 0xC68);
		writel(0x02, phy_base_regs + 0xC6C);
		writel(0x0A, phy_base_regs + 0xC70);
		writel(0xE7, phy_base_regs + 0xCA8);
		writel(0x00, phy_base_regs + 0xCAC);
		writel(0x0E, phy_base_regs + 0xCB0);
		writel(0x1C, phy_base_regs + 0xCCC);
		writel(0x05, phy_base_regs + 0xCD4);
		writel(0x77, phy_base_regs + 0xCD8);
		writel(0x7A, phy_base_regs + 0xCDC);

		writel(0x2F, phy_base_regs + 0xDB4);
		writel(0x2F, phy_base_regs + 0x15B4);

		//lane1
		writel(0x01, phy_base_regs + 0x10d0);
		writel(0x30, phy_base_regs + 0x10F4);
		writel(0x60, phy_base_regs + 0x110C);
		writel(0x06, phy_base_regs + 0x11B4);
		writel(0x3F, phy_base_regs + 0x11C0);
		writel(0xFB, phy_base_regs + 0x11C4);
		writel(0x03, phy_base_regs + 0x126C);
		writel(0x06, phy_base_regs + 0x1340);
		writel(0x06, phy_base_regs + 0x1344);
		writel(0x06, phy_base_regs + 0x1348);
		writel(0x06, phy_base_regs + 0x134C);
		writel(0x00, phy_base_regs + 0x1350);
		writel(0x00, phy_base_regs + 0x1354);
		writel(0x00, phy_base_regs + 0x1358);
		writel(0x03, phy_base_regs + 0x135C);
		writel(0x16, phy_base_regs + 0x1444);
		writel(0x16, phy_base_regs + 0x1448);
		writel(0x16, phy_base_regs + 0x144C);
		writel(0x16, phy_base_regs + 0x1450);
		writel(0x0A, phy_base_regs + 0x1454);
		writel(0x0A, phy_base_regs + 0x1458);
		writel(0x0A, phy_base_regs + 0x145C);
		writel(0x0A, phy_base_regs + 0x1460);
		writel(0x08, phy_base_regs + 0x1464);
		writel(0x08, phy_base_regs + 0x1468);
		writel(0x02, phy_base_regs + 0x146C);
		writel(0x0A, phy_base_regs + 0x1470);
		writel(0xE7, phy_base_regs + 0x14A8);
		writel(0x00, phy_base_regs + 0x14AC);
		writel(0x0E, phy_base_regs + 0x14B0);
		writel(0x1C, phy_base_regs + 0x14CC);
		writel(0x05, phy_base_regs + 0x14D4);
		writel(0x77, phy_base_regs + 0x14D8);
		writel(0x7A, phy_base_regs + 0x14DC);
		//
		//   //for Gen4 0709
		writel(0x00, phy_base_regs + 0xC70);
		writel(0xE7, phy_base_regs + 0xCA8);
		writel(0x00, phy_base_regs + 0xCAC);
		writel(0x0A, phy_base_regs + 0xCB0);
		writel(0xC0, phy_base_regs + 0x9B4);
		writel(0x01, phy_base_regs + 0xA6C);
		writel(0x01, phy_base_regs + 0xBA8);
		writel(0x04, phy_base_regs + 0xC08);
		writel(0x06, phy_base_regs + 0xC40);
		writel(0x04, phy_base_regs + 0xB4C);

		writel(0x00, phy_base_regs + 0x1470);
		writel(0xE7, phy_base_regs + 0x14A8);
		writel(0x00, phy_base_regs + 0x14AC);
		writel(0x0A, phy_base_regs + 0x14B0);
		writel(0xC0, phy_base_regs + 0x11B4);
		writel(0x01, phy_base_regs + 0x126C);
		writel(0x01, phy_base_regs + 0x13A8);
		writel(0x04, phy_base_regs + 0x1408);
		writel(0x06, phy_base_regs + 0x1440);
		writel(0x04, phy_base_regs + 0x134C);

		writel(0x20, phy_base_regs + 0x450);
	}

	/* tx amplitude control */
	/* writel(0x14, phy_base_regs + (0x5C * 4)); */

	/* PCIE_MAC RST */
	mdelay(1);
	writel(0x1, elbi_base_regs + 0x1400);
	mdelay(1);
	/* PCIE_PHY PCS&PMA(CMN)_RST */
	mdelay(1);
	writel(0x1, elbi_base_regs + 0x1408);
	mdelay(1);
	/* CDR Reset */	//TBD

	//writel(0x12001, dbi_base_regs + 0x890);
	/* EQ Off --> DBI_Base + 0x890h //Need to insert at the Setup_RC code */

	pr_info("%s: phy pcs + 0x154, + 0x954 = 0x700D5\n",__func__);
	writel(0x700D5, phy_pcs_base_regs + 0x154);
	writel(0x700D5, phy_pcs_base_regs + 0x954);

	if (chip_ver == 1){
		pr_info("[%s] GEN4PHY: For L2 power\n", __func__);
		writel(0x300FF, phy_pcs_base_regs + 0x150);
	}
}

int exynos_pcie_rc_eom(struct device *dev, void *phy_base_regs)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct exynos_pcie_ops *pcie_ops
			= &exynos_pcie->exynos_pcie_ops;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device_node *np = dev->of_node;
	unsigned int val;
	unsigned int speed_rate, num_of_smpl;
	unsigned int lane_width = 1;
	int i, ret;
	int test_cnt = 0;
	struct pcie_eom_result **eom_result;

	u32 phase_sweep = 0;
	u32 phase_step = 1;
	u32 phase_loop;
	u32 vref_sweep = 0;
	u32 vref_step = 1;
	u32 err_cnt = 0;
	u32 cdr_value = 0;
	u32 eom_done = 0;
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

	pcie_ops->rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
	speed_rate = (val >> 16) & 0xf;

	num_of_smpl = 13;

	for (i = 0; i < lane_width; i++)
	{
		if (speed_rate == 3 || speed_rate == 4) {
			/* rx_efom_settle_time [7:4] = 0xE, rx_efom_bit_width_sel[3:2] = 0 */
			writel(0xE7, phy_base_regs + RX_EFOM_BIT_WIDTH_SEL);
		} else {
			writel(0xE3, phy_base_regs + RX_EFOM_BIT_WIDTH_SEL);
		}
		writel(0xF, phy_base_regs + ANA_RX_DFE_EOM_PI_STR_CTRL);
		writel(0x14, phy_base_regs + ANA_RX_DFE_EOM_PI_DIVSEL_G12);
		writel(0x24, phy_base_regs + ANA_RX_DFE_EOM_PI_DIVSEL_G32);

		val = readl(phy_base_regs + RX_CDR_LOCK) >> 2;
		cdr_value = val & 0x1;
		eom_done = readl(phy_base_regs + RX_EFOM_DONE) & 0x1;
		dev_info(dev, "eom_done 0x%x , cdr_value : 0x%x \n", eom_done, cdr_value);

		writel(0x0, phy_base_regs + RX_EFOM_NUMOF_SMPL_13_8);
		writel(num_of_smpl, phy_base_regs + RX_EFOM_NUMOF_SMPL_7_0);

		if (speed_rate == 1)
			phase_loop = 2;
		else
			phase_loop = 1;

		for (phase_sweep = 0; phase_sweep <= 0x47 * phase_loop; phase_sweep = phase_sweep + phase_step)
		{
			val = (phase_sweep % 72) << 1;
			writel(val, phy_base_regs + RX_EFOM_EOM_PH_SEL);

			for (vref_sweep = 0; vref_sweep <= 255; vref_sweep = vref_sweep + vref_step)
			{
				writel(0x12, phy_base_regs + RX_EFOM_MODE);
				writel(vref_sweep, phy_base_regs + RX_EFOM_DFE_VREF_CTRL);
				writel(0x13, phy_base_regs + RX_EFOM_MODE);

				val = readl(phy_base_regs + RX_EFOM_DONE) & 0x1;
				while (val != 0x1) {
					udelay(1);
					val = readl(phy_base_regs + RX_EFOM_DONE) & 0x1;
				}

				err_cnt_13_8 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_13_8) << 8;
				err_cnt_7_0 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_7_0);
				err_cnt = err_cnt_13_8 + err_cnt_7_0;

				//if (vref_sweep == 128)
				printk("%d,%d : %d %d %d\n", i, test_cnt, phase_sweep, vref_sweep, err_cnt);

				//save result
				eom_result[i][test_cnt].phase = phase_sweep;
				eom_result[i][test_cnt].vref = vref_sweep;
				eom_result[i][test_cnt].err_cnt = err_cnt;
				test_cnt++;

			}
		}
		/* goto next lane */
		phy_base_regs += 0x800;
		test_cnt = 0;
	}

	return 0;
}

void exynos_pcie_rc_phy_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	dev_info(pci->dev, "Initialize PHY functions.\n");

	exynos_pcie->phy_ops.phy_check_rx_elecidle =
		exynos_pcie_rc_phy_check_rx_elecidle;
	exynos_pcie->phy_ops.phy_all_pwrdn = exynos_pcie_rc_phy_all_pwrdn;
	exynos_pcie->phy_ops.phy_all_pwrdn_clear = exynos_pcie_rc_phy_all_pwrdn_clear;
	exynos_pcie->phy_ops.phy_config = exynos_pcie_rc_pcie_phy_config;
	exynos_pcie->phy_ops.phy_eom = exynos_pcie_rc_eom;
}
