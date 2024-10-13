/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/syscore_ops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>

#include "pcie-designware.h"

#define MAX_TIMEOUT		1000
#define ID_MASK			0xffff

#define to_exynos_pcie(x)	container_of(x, struct exynos_pcie, pp)

int poweronoff_flag = 0;
int probe_ok = 0;

struct exynos_pcie {
	void __iomem		*elbi_base;
	void __iomem		*phy_base;
	void __iomem		*block_base;
	void __iomem		*pmu_base;
	void __iomem		*gpio_base;
	void __iomem		*cmu_base;
	int			reset_gpio;
	int			perst_gpio;
	struct clk		*clk;
	struct clk		*phy_clk;
	struct pcie_port	pp;
	struct pci_dev		*pci_dev;
	struct notifier_block	lpa_nb;
	struct mutex		lock;
	struct delayed_work	work;
};

static struct workqueue_struct *pcie_wq;
static struct exynos_pcie *g_pcie;

static struct raw_notifier_head pci_lpa_nh =
		RAW_NOTIFIER_INIT(pci_lpa_nh);

#define PCI_LPA_PREPARE			(0x1)
#define PCI_LPA_RESUME			(0x2)

/* PCIe ELBI registers */
#define PCIE_IRQ_PULSE			0x000
#define IRQ_INTA_ASSERT			(0x1 << 0)
#define IRQ_INTB_ASSERT			(0x1 << 2)
#define IRQ_INTC_ASSERT			(0x1 << 4)
#define IRQ_INTD_ASSERT			(0x1 << 6)
#define IRQ_RADM_PM_TO_ACK		(0x1 << 18)
#define PCIE_IRQ_LEVEL			0x004
#define PCIE_IRQ_SPECIAL		0x008
#define PCIE_IRQ_EN_PULSE		0x00c
#define PCIE_IRQ_EN_LEVEL		0x010
#define IRQ_MSI_ENABLE			(0x1 << 2)
#define PCIE_IRQ_EN_SPECIAL		0x014
#define PCIE_SW_WAKE			0x018
#define PCIE_BUS_NUM			0x01c
#define PCIE_RADM_MSG_LTR		0x020
#define PCIE_APP_LTR_LATENCY		0x024
#define PCIE_APP_INIT_RESET		0x028
#define PCIE_APP_LTSSM_ENABLE		0x02c
#define PCIE_L1_BUG_FIX_ENABLE		0x038
#define PCIE_APP_REQ_EXIT_L1		0x040
#define PCIE_APPS_PM_XMT_TURNOFF	0x04c
#define PCIE_ELBI_RDLH_LINKUP		0x074
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_PM_DSTATE			0x88
#define PCIE_D0_UNINIT_STATE		0x4
#define PCIE_ELBI_SLV_AWMISC		0x11c
#define PCIE_ELBI_SLV_ARMISC		0x120
#define PCIE_ELBI_SLV_DBI_ENABLE	(0x1 << 21)

/* PCIe Purple registers */
#define PCIE_L1SUB_CM_CON		0x1010
#define PCIE_PHY_GLOBAL_RESET		0x1040
#define PCIE_PHY_COMMON_RESET		0x1020
#define PCIE_PHY_CMN_REG		0x008
#define PCIE_PHY_MAC_RESET		0x208
#define PCIE_PHY_PLL_LOCKED		0x010
#define PCIE_PHY_TRSVREG_RESET		0x020
#define PCIE_PHY_TRSV_RESET		0x024

/* PCIe PHY registers */
#define PCIE_PHY_IMPEDANCE		0x004
#define PCIE_PHY_PLL_DIV_0		0x008
#define PCIE_PHY_PLL_BIAS		0x00c
#define PCIE_PHY_DCC_FEEDBACK		0x014
#define PCIE_PHY_PLL_DIV_1		0x05c
#define PCIE_PHY_TRSV0_EMP_LVL		0x084
#define PCIE_PHY_TRSV0_DRV_LVL		0x088
#define PCIE_PHY_TRSV0_RXCDR		0x0ac
#define PCIE_PHY_TRSV0_LVCC		0x0dc
#define PCIE_PHY_TRSV1_EMP_LVL		0x144
#define PCIE_PHY_TRSV1_RXCDR		0x16c
#define PCIE_PHY_TRSV1_LVCC		0x19c
#define PCIE_PHY_TRSV2_EMP_LVL		0x204
#define PCIE_PHY_TRSV2_RXCDR		0x22c
#define PCIE_PHY_TRSV2_LVCC		0x25c
#define PCIE_PHY_TRSV3_EMP_LVL		0x2c4
#define PCIE_PHY_TRSV3_RXCDR		0x2ec
#define PCIE_PHY_TRSV3_LVCC		0x31c

/* PCIe PMU registers */
#define PCIE_PHY_CONTROL		0x730

static int exynos_pci_lpa_event(struct notifier_block *nb, unsigned long event, void *data);
static void exynos_pcie_sideband_dbi_r_mode(struct pcie_port*, bool);
static void exynos_pcie_sideband_dbi_w_mode(struct pcie_port*, bool);
static void exynos_pcie_assert_phy_reset(struct pcie_port*);
static inline void exynos_pcie_writel_rc(struct pcie_port*, u32, void*);
static inline void exynos_pcie_readl_rc(struct pcie_port*, void*, u32*);
extern void dw_pcie_config_l1ss(struct pcie_port *pp);
void exynos_pcie_register_dump(void);

int check_rev(void)
{
	if (samsung_rev() >= EXYNOS5433_REV_1_1)
		return 1;
	else
		return 0;
}

int exynos_pcie_reset(struct pcie_port *pp)
{
	u32 val;
	int count = 0, try_cnt = 0;

	if (!check_rev())
		return -EPIPE;

retry:
	writel((readl(g_pcie->gpio_base + 0x8) & ~(0x3 << 12)) | (0x1 << 12), g_pcie->gpio_base + 0x8);
	udelay(20);
	writel(readl(g_pcie->gpio_base + 0x8) | (0x3 << 12), g_pcie->gpio_base + 0x8);

	exynos_pcie_assert_phy_reset(pp);

	exynos_pcie_sideband_dbi_r_mode(pp, true);
	exynos_pcie_sideband_dbi_w_mode(pp, true);
	/* setup root complex */
	dw_pcie_setup_rc(pp);
	exynos_pcie_sideband_dbi_r_mode(pp, false);
	exynos_pcie_sideband_dbi_w_mode(pp, false);

	/* set #PERST high */
	gpio_set_value(g_pcie->perst_gpio, 1);
	msleep(80);

	printk("D state: %x, %x\n", readl(g_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7, readl(g_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f);

	/* assert LTSSM enable */
	writel(PCIE_ELBI_LTSSM_ENABLE, g_pcie->elbi_base + PCIE_APP_LTSSM_ENABLE);
	count = 0;
	while (count < MAX_TIMEOUT) {
		val = readl(g_pcie->elbi_base + PCIE_IRQ_LEVEL) & 0x10;
		if(val)
			break;

		count++;

		udelay(10);
	}

	/* wait to check whether link down again(D0 UNINIT) or not for retry */
	msleep(1);
	val = readl(g_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7;
	if (count >= MAX_TIMEOUT || val == PCIE_D0_UNINIT_STATE) {
		try_cnt++;
		dev_info(pp->dev, "%s: Link is not up, try count: %d, %x\n", __func__, try_cnt, readl(g_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f);
		if (try_cnt < 10) {
			gpio_set_value(g_pcie->perst_gpio, 0);
			/* LTSSM disable */
			writel(PCIE_ELBI_LTSSM_DISABLE, g_pcie->elbi_base + PCIE_APP_LTSSM_ENABLE);

			writel(readl(g_pcie->phy_base + 0x55*4) & ~(0x1 << 3), g_pcie->phy_base + 0x55*4);
			udelay(100);
			writel(readl(g_pcie->phy_base + 0x21*4) & ~(0x1 << 4), g_pcie->phy_base + 0x21*4);

			goto retry;
		} else {
			exynos_pcie_register_dump();
			BUG_ON(1);
			return -EPIPE;
		}
	} else {
		dev_info(pp->dev, "%s: Link up:%x\n", __func__, readl(g_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7);
	}

	/* setup ATU for cfg/mem outbound */
	dw_pcie_prog_viewport_cfg0(pp, 0x1000000);
	dw_pcie_prog_viewport_mem_outbound(pp);

	/* EP BAR register setting */
	writel(0x0c200004, g_pcie->pp.va_cfg0_base + 0x10);
	writel(0x0c400004, g_pcie->pp.va_cfg0_base + 0x18);

	/* L1.2 ASPM enable */
	dw_pcie_config_l1ss(pp);

	return 0;
}

extern void dhd_lsi_send_hang_message(void);
void exynos_pcie_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie = container_of(work, struct exynos_pcie, work.work);
	u32 val;

	if (!poweronoff_flag)
		return;

	mutex_lock(&exynos_pcie->lock);

	val = readl(g_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7;

	/* check whether D0 uninit state and link down or not */
	if ((val != PCIE_D0_UNINIT_STATE) && (~(readl(g_pcie->elbi_base + PCIE_IRQ_SPECIAL)) & (0x1 << 2))) {
		queue_delayed_work(pcie_wq, &exynos_pcie->work, msecs_to_jiffies(1000));
		goto exit;
	}

	if (val == PCIE_D0_UNINIT_STATE)
		printk("%s: d0uninit state\n", __func__);

	if (readl(g_pcie->elbi_base + PCIE_IRQ_SPECIAL) & (0x1 << 2))
		printk("%s: link down event\n", __func__);

	/* Wifi off and on */
	dhd_lsi_send_hang_message();

exit:
	mutex_unlock(&exynos_pcie->lock);
}

static void exynos_pcie_sideband_dbi_w_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (on) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
	} else {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
	}
}

static void exynos_pcie_sideband_dbi_r_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (on) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
	} else {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
	}
}

static void exynos_pcie_assert_phy_reset(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *block_base = exynos_pcie->block_base;
	void __iomem *elbi_base = exynos_pcie->elbi_base;
	void __iomem *phy_base = exynos_pcie->phy_base;
	u32 val;

	/* PHY Common Reset */
	val = readl(block_base + PCIE_PHY_COMMON_RESET);
	val |= 0x1;
	writel(val, block_base + PCIE_PHY_COMMON_RESET);

	/* PHY Mac Reset */
	val = readl(block_base + PCIE_PHY_MAC_RESET);
	val &= ~(0x1 << 4);
	writel(val, block_base + PCIE_PHY_MAC_RESET);

	if (check_rev()) {

		/* PHY refclk 24MHz */
		val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x10);
		val &= ~(0x06);
		val |= 0x1 << 1;
		writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

		/* PHY Global Reset */
		val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x1);
		writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

		writel(0x11, phy_base + 0x03*4);

		/* band gap reference on */
		//	writel(0x0, phy_base + 0x20*4);
		//	writel(0x0, phy_base + 0x4b*4);

		/* jitter tunning */
		writel(0x34, phy_base + 0x04*4);
		writel(0x02, phy_base + 0x07*4);
		writel(0x41, phy_base + 0x21*4);
		writel(0x7F, phy_base + 0x14*4);
		writel(0xC0, phy_base + 0x15*4);
		writel(0x61, phy_base + 0x36*4);

		/* D0 uninit.. */
		writel(0x44, phy_base + 0x3D*4);

		/* 24MHz */
		writel(0x94, phy_base + 0x08*4);
		writel(0xA7, phy_base + 0x09*4);
		writel(0x93, phy_base + 0x0A*4);
		writel(0x6B, phy_base + 0x0C*4);
		writel(0xA5, phy_base + 0x0F*4);
		writel(0x34, phy_base + 0x16*4);
		writel(0xA3, phy_base + 0x17*4);
		writel(0xA7, phy_base + 0x1A*4);
		writel(0x71, phy_base + 0x23*4);
		//writel(0x06, phy_base + 0x26*4);
		writel(0x0E, phy_base + 0x26*4);
		writel(0x14, phy_base + 0x07*4);
		writel(0x48, phy_base + 0x43*4);
		writel(0x44, phy_base + 0x44*4);
		writel(0x03, phy_base + 0x45*4);
		writel(0xA7, phy_base + 0x48*4);
		writel(0x13, phy_base + 0x54*4);
		writel(0x04, phy_base + 0x31*4);
		writel(0x00, phy_base + 0x32*4);

		/* PHY Common Reset */
		val = readl(block_base + PCIE_PHY_COMMON_RESET);
		val &= ~(0x1);
		writel(val, block_base + PCIE_PHY_COMMON_RESET);

		/* PHY Mac Reset */
		val = readl(block_base + PCIE_PHY_MAC_RESET);
		val |= 0x1 << 4;
		writel(val, block_base + PCIE_PHY_MAC_RESET);

		/* Bus number enable */
		val = readl(elbi_base + PCIE_SW_WAKE);
		val &= ~(0x1<<1);
		writel(val, elbi_base + PCIE_SW_WAKE);
	} else {
		/* PHY refclk 24MHz */
		val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x10);
		val &= ~(0x06);
		val |= 0x1 << 1;
		writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

		/* PHY Global Reset */
		val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x1);
		writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

		writel(0x11, phy_base + 0x03*4);

		/* jitter tunning */
		writel(0x34, phy_base + 0x04*4);
		writel(0x02, phy_base + 0x07*4);
		writel(0x41, phy_base + 0x21*4);
		writel(0x7F, phy_base + 0x14*4);
		writel(0xC0, phy_base + 0x15*4);
		writel(0x61, phy_base + 0x36*4);

		/* D0 uninit.. */
		writel(0x44, phy_base + 0x3D*4);

		/* 24MHz */
		writel(0x94, phy_base + 0x08*4);
		writel(0xA7, phy_base + 0x09*4);
		writel(0x93, phy_base + 0x0A*4);
		writel(0x6B, phy_base + 0x0C*4);
		writel(0xA5, phy_base + 0x0F*4);
		writel(0x34, phy_base + 0x16*4);
		writel(0xA3, phy_base + 0x17*4);
		writel(0xA7, phy_base + 0x1A*4);
		writel(0x71, phy_base + 0x23*4);
		writel(0x06, phy_base + 0x26*4);
		writel(0x14, phy_base + 0x07*4);
		writel(0x44, phy_base + 0x44*4);
		writel(0x03, phy_base + 0x45*4);
		writel(0xA7, phy_base + 0x48*4);
		writel(0x13, phy_base + 0x54*4);
		writel(0x04, phy_base + 0x31*4);
		writel(0x00, phy_base + 0x32*4);

		/* PHY Common Reset */
		val = readl(block_base + PCIE_PHY_COMMON_RESET);
		val &= ~(0x1);
		writel(val, block_base + PCIE_PHY_COMMON_RESET);

		/* PHY Mac Reset */
		val = readl(block_base + PCIE_PHY_MAC_RESET);
		val |= 0x1 << 4;
		writel(val, block_base + PCIE_PHY_MAC_RESET);

		/* Bus number enable */
		val = readl(elbi_base + PCIE_SW_WAKE);
		val &= ~(0x1<<1);
		writel(val, elbi_base + PCIE_SW_WAKE);
	}
}

static int exynos_pcie_establish_link(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;
	void __iomem *pmu_base = exynos_pcie->pmu_base;
	void __iomem *block_base = exynos_pcie->block_base;
	int count = 0;
	u32 val;

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP);
	printk("LINK STATUS: %x\n", val);

	writel(1, pmu_base + PCIE_PHY_CONTROL);

	if (check_rev()) {
		/* APP_REQ_EXIT_L1_MODE : BIT5 (0x0 : H/W mode, 0x1 : S/W mode) */
		val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x1 << 5);
		writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

		/* PCIE_L1SUB_CM_CON : BIT0 (0x0 : REFCLK GATING DISABLE, 0x1 : REFCLK GATING ENABLE) */
		val = readl(block_base + PCIE_L1SUB_CM_CON);
		val |= 0x1;
		writel(val, block_base + PCIE_L1SUB_CM_CON);
	}

	exynos_pcie_assert_phy_reset(pp);

	exynos_pcie_sideband_dbi_r_mode(pp, true);
	exynos_pcie_sideband_dbi_w_mode(pp, true);
	/* setup root complex */
	dw_pcie_setup_rc(pp);
	exynos_pcie_sideband_dbi_r_mode(pp, false);
	exynos_pcie_sideband_dbi_w_mode(pp, false);

	if (probe_ok) {
		writel(readl(exynos_pcie->gpio_base + 4) | (0x1 << 5), exynos_pcie->gpio_base + 4);
		mdelay(80);
	}
	/* assert LTSSM enable */
	writel(PCIE_ELBI_LTSSM_ENABLE, elbi_base + PCIE_APP_LTSSM_ENABLE);
	while (count < MAX_TIMEOUT) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		if(val >= 0x0d && val <= 0x15)
			break;
		count++;
		udelay(10);
	}

	if (count >= MAX_TIMEOUT) {
		dev_info(pp->dev, "%s: Link is not up\n", __func__);
		return exynos_pcie_reset(pp);
	} else
		dev_info(pp->dev, "%s: Link up: %x\n", __func__, val);

	return 0;
}

static void exynos_pcie_clear_irq_pulse(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	val = readl(elbi_base + PCIE_IRQ_PULSE);
	writel(val, elbi_base + PCIE_IRQ_PULSE);
	return;
}

static void exynos_pcie_enable_irq_pulse(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	/* enable INTX interrupt */
	val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
		IRQ_INTC_ASSERT | IRQ_INTD_ASSERT,
	writel(val, elbi_base + PCIE_IRQ_EN_PULSE);

	/* disable LEVEL interrupt */
	writel(0x0, elbi_base + PCIE_IRQ_EN_LEVEL);

	/* disable SPECIAL interrupt */
	writel(0x0, elbi_base + PCIE_IRQ_EN_SPECIAL);

	return;
}

static irqreturn_t exynos_pcie_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	exynos_pcie_clear_irq_pulse(pp);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PCI_MSI
static void exynos_pcie_clear_irq_level(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	val = readl(elbi_base + PCIE_IRQ_LEVEL);
	writel(val, elbi_base + PCIE_IRQ_LEVEL);
	return;
}

static irqreturn_t exynos_pcie_msi_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	/* handle msi irq */
	dw_handle_msi_irq(pp);
	exynos_pcie_clear_irq_level(pp);

	return IRQ_HANDLED;
}

static void exynos_pcie_msi_init(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	dw_pcie_msi_init(pp);

	/* enable MSI interrupt */
	val = readl(elbi_base + PCIE_IRQ_EN_LEVEL);
	val |= IRQ_MSI_ENABLE;
	writel(val, elbi_base + PCIE_IRQ_EN_LEVEL);
	return;
}
#endif

static void exynos_pcie_enable_interrupts(struct pcie_port *pp)
{
	exynos_pcie_enable_irq_pulse(pp);
#ifdef CONFIG_PCI_MSI
	exynos_pcie_msi_init(pp);
#endif
	return;
}

static inline void exynos_pcie_readl_rc(struct pcie_port *pp, void *dbi_base,
					u32 *val)
{
	exynos_pcie_sideband_dbi_r_mode(pp, true);
	*val = readl(dbi_base);
	exynos_pcie_sideband_dbi_r_mode(pp, false);
	return;
}

static inline void exynos_pcie_writel_rc(struct pcie_port *pp, u32 val,
					void *dbi_base)
{
	exynos_pcie_sideband_dbi_w_mode(pp, true);
	writel(val, dbi_base);
	exynos_pcie_sideband_dbi_w_mode(pp, false);
	return;
}

static int exynos_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	int ret;

	exynos_pcie_sideband_dbi_r_mode(pp, true);
	ret = cfg_read(pp->dbi_base + (where & ~0x3), where, size, val);
	exynos_pcie_sideband_dbi_r_mode(pp, false);
	return ret;
}

static int exynos_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	int ret;

	exynos_pcie_sideband_dbi_w_mode(pp, true);
	ret = cfg_write(pp->dbi_base + (where & ~0x3), where, size, val);
	exynos_pcie_sideband_dbi_w_mode(pp, false);
	return ret;
}

static int exynos_pcie_link_up(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;

	if (val >= 0x0d && val <= 0x15)
		return 1;

	return 0;
}

static int exynos_pcie_host_init(struct pcie_port *pp)
{
	exynos_pcie_enable_interrupts(pp);
	return exynos_pcie_establish_link(pp);
}

static struct pcie_host_ops exynos_pcie_host_ops = {
	.readl_rc = exynos_pcie_readl_rc,
	.writel_rc = exynos_pcie_writel_rc,
	.rd_own_conf = exynos_pcie_rd_own_conf,
	.wr_own_conf = exynos_pcie_wr_own_conf,
	.link_up = exynos_pcie_link_up,
	.host_init = exynos_pcie_host_init,
};

static int add_pcie_port(struct pcie_port *pp, struct platform_device *pdev)
{
	int ret;

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_irq_handler,
				IRQF_SHARED, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

#ifdef CONFIG_PCI_MSI
	pp->msi_irq = platform_get_irq(pdev, 0);

	if (!pp->msi_irq) {
		dev_err(&pdev->dev, "failed to get msi irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, pp->msi_irq,
				exynos_pcie_msi_irq_handler,
				IRQF_SHARED, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request msi irq\n");
		return ret;
	}
#endif
	pp->root_bus_nr = -1;
	pp->ops = &exynos_pcie_host_ops;

	spin_lock_init(&pp->conf_lock);
	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static int __init exynos_pcie_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct pcie_port *pp;
	struct resource *elbi_base;
	struct resource *phy_base;
	struct resource *block_base;
	struct resource *pmu_base;
	struct device_node *root_node = pdev->dev.of_node;
	int ret;
	u32 val, vendor_id, device_id;

	exynos_pcie = devm_kzalloc(&pdev->dev, sizeof(*exynos_pcie),
				GFP_KERNEL);
	if (!exynos_pcie) {
		dev_err(&pdev->dev, "no memory for exynos pcie\n");
		return -ENOMEM;
	}

	pp = &exynos_pcie->pp;

	pp->dev = &pdev->dev;

	exynos_pcie->reset_gpio = of_get_gpio(root_node, 0);
	exynos_pcie->perst_gpio = of_get_gpio(root_node, 1);

	ret = devm_gpio_request_one(exynos_pcie->pp.dev, exynos_pcie->perst_gpio,
		    GPIOF_OUT_INIT_HIGH, dev_name(exynos_pcie->pp.dev));
	if (ret) {
		dev_err(&pdev->dev, "failed to request wifi perst gpio");
		return ret;
	}

	exynos_pcie->clk = devm_clk_get(&pdev->dev, "gate_pcie");
	if (IS_ERR(exynos_pcie->clk)) {
		dev_err(&pdev->dev, "Failed to get pcie rc clock\n");
		return PTR_ERR(exynos_pcie->clk);
	}
	ret = clk_prepare_enable(exynos_pcie->clk);
	if (ret)
		return ret;

	exynos_pcie->phy_clk = devm_clk_get(&pdev->dev, "gate_pcie_phy");
	if (IS_ERR(exynos_pcie->phy_clk)) {
		dev_err(&pdev->dev, "Failed to get pcie_phy rc clock\n");
		return PTR_ERR(exynos_pcie->phy_clk);
	}
	ret = clk_prepare_enable(exynos_pcie->phy_clk);
	if (ret)
		goto fail_phy_clk;

	elbi_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, elbi_base);
	if (IS_ERR(exynos_pcie->elbi_base))
		goto fail_bus_clk;

	phy_base = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, phy_base);
	if (IS_ERR(exynos_pcie->phy_base))
		goto fail_bus_clk;

	block_base = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	exynos_pcie->block_base = devm_ioremap_resource(&pdev->dev, block_base);
	if (IS_ERR(exynos_pcie->block_base))
		goto fail_bus_clk;

	pmu_base = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	exynos_pcie->pmu_base = devm_ioremap_resource(&pdev->dev, pmu_base);
	if (IS_ERR(exynos_pcie->pmu_base))
		goto fail_bus_clk;

	exynos_pcie->gpio_base = devm_ioremap(&pdev->dev, 0x156900a0, 100);
	if (IS_ERR(exynos_pcie->gpio_base))
		goto fail_bus_clk;

	exynos_pcie->cmu_base = devm_ioremap(&pdev->dev, 0x10030634, 100);
	if (IS_ERR(exynos_pcie->cmu_base))
		goto fail_bus_clk;

	g_pcie = exynos_pcie;

	ret = add_pcie_port(pp, pdev);
	if (ret)
		goto fail_bus_clk;

	exynos_pcie_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
	vendor_id = val & ID_MASK;
	device_id = (val >> 16) & ID_MASK;

	exynos_pcie->pci_dev = pci_get_device(vendor_id, device_id, NULL);
	if (!exynos_pcie->pci_dev) {
		dev_err(&pdev->dev, "Failed to get pci device\n");
		goto fail_bus_clk;
	}

	exynos_pcie->lpa_nb.notifier_call = exynos_pci_lpa_event;
	exynos_pcie->lpa_nb.next = NULL;
	exynos_pcie->lpa_nb.priority = 0;

	ret = raw_notifier_chain_register(&pci_lpa_nh, &exynos_pcie->lpa_nb);
	if (ret)
		dev_err(&pdev->dev, "Failed to register lpa notifier\n");

	platform_set_drvdata(pdev, exynos_pcie);

	if (check_rev()) {
		mutex_init(&exynos_pcie->lock);
		pcie_wq = create_freezable_workqueue("pcie_wq");
		if (IS_ERR(pcie_wq)) {
			pr_err("%s: couldn't create workqueue\n", __func__);
			goto fail_bus_clk;
		}
		INIT_DELAYED_WORK(&exynos_pcie->work, exynos_pcie_work);
		queue_delayed_work(pcie_wq, &exynos_pcie->work, msecs_to_jiffies(1000));
	}

	pci_save_state(g_pcie->pci_dev);
	poweronoff_flag = 1;
	probe_ok = 1;

	return 0;

fail_bus_clk:
	clk_disable_unprepare(exynos_pcie->phy_clk);
fail_phy_clk:
	clk_disable_unprepare(exynos_pcie->clk);
	probe_ok = 0;
	return ret;
}

static int __exit exynos_pcie_remove(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie = platform_get_drvdata(pdev);

	raw_notifier_chain_unregister(&pci_lpa_nh, &exynos_pcie->lpa_nb);

	writel(readl(exynos_pcie->phy_base + 0x21*4) | (0x1 << 4), exynos_pcie->phy_base + 0x21*4);
	writel(readl(exynos_pcie->phy_base + 0x55*4) | (0x1 << 3), exynos_pcie->phy_base + 0x55*4);

	mutex_destroy(&exynos_pcie->lock);
	destroy_workqueue(pcie_wq);
	clk_disable_unprepare(exynos_pcie->clk);

	return 0;
}

static const struct of_device_id exynos_pcie_of_match[] = {
	{ .compatible = "samsung,exynos5433-pcie", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_pcie_of_match);

void exynos_pcie_register_dump(void)
{
	u32 i;

	for (i = 0; i < 0x134; i = i + 4)
		printk("link reg %x : %x\n", i, readl(g_pcie->elbi_base + i));

	for (i = 0; i < 0x60; i++)
		printk("phy reg %x * 4: %x\n", i, readl(g_pcie->phy_base + (i * 4)));
}

static void exynos_pcie_resumed_phydown(void)
{
	u32 val;

	/* phy all power down on wifi off during suspend/resume */
	clk_prepare_enable(g_pcie->clk);
	clk_prepare_enable(g_pcie->phy_clk);

	exynos_pcie_enable_interrupts(&g_pcie->pp);
	writel(1, g_pcie->pmu_base + PCIE_PHY_CONTROL);

	/* APP_REQ_EXIT_L1_MODE : BIT5 (0x0 : H/W mode, 0x1 : S/W mode) */
	val = readl(g_pcie->block_base + PCIE_PHY_GLOBAL_RESET);
	val &= ~(0x1 << 5);
	writel(val, g_pcie->block_base + PCIE_PHY_GLOBAL_RESET);

	/* PCIE_L1SUB_CM_CON : BIT0 (0x0 : REFCLK GATING DISABLE, 0x1 : REFCLK GATING ENABLE) */
	val = readl(g_pcie->block_base + PCIE_L1SUB_CM_CON);
	val |= 0x1;
	writel(val, g_pcie->block_base + PCIE_L1SUB_CM_CON);

	exynos_pcie_assert_phy_reset(&g_pcie->pp);

	writel(readl(g_pcie->phy_base + 0x21*4) | (0x1 << 4), g_pcie->phy_base + 0x21*4);
	writel(readl(g_pcie->phy_base + 0x55*4) | (0x1 << 3), g_pcie->phy_base + 0x55*4);

	clk_disable_unprepare(g_pcie->clk);
	clk_disable_unprepare(g_pcie->phy_clk);
}

void exynos_pcie_poweron(void)
{
	u32 __maybe_unused val;
	int __maybe_unused count = 0;

	if (check_rev()) {
		printk("------%s------ probe_ok: %d, poweronoff_flag: %d\n", __func__, probe_ok, poweronoff_flag);
		if (!poweronoff_flag && probe_ok) {
			poweronoff_flag = 1;

			clk_prepare_enable(g_pcie->clk);
			clk_prepare_enable(g_pcie->phy_clk);

			writel(readl(g_pcie->phy_base + 0x55*4) & ~(0x1 << 3), g_pcie->phy_base + 0x55*4);
			udelay(100);
			writel(readl(g_pcie->phy_base + 0x21*4) & ~(0x1 << 4), g_pcie->phy_base + 0x21*4);

			exynos_pcie_reset(&g_pcie->pp);
			pci_restore_state(g_pcie->pci_dev);

			writel(readl(g_pcie->elbi_base + PCIE_IRQ_SPECIAL), g_pcie->elbi_base + PCIE_IRQ_SPECIAL);

			queue_delayed_work(pcie_wq, &g_pcie->work, msecs_to_jiffies(1000));
			return;
		}
	}
}
EXPORT_SYMBOL(exynos_pcie_poweron);

void exynos_pcie_poweroff(void)
{
	u32 __maybe_unused val;
	int __maybe_unused count = 0;

	if (check_rev()) {
		printk("------%s------ probe_ok: %d, poweronoff_flag: %d\n", __func__, probe_ok, poweronoff_flag);
		if (poweronoff_flag && probe_ok) {
			cancel_delayed_work_sync(&g_pcie->work);
			gpio_set_value(g_pcie->perst_gpio, 0);
			/* LTSSM disable */
			writel(PCIE_ELBI_LTSSM_DISABLE, g_pcie->elbi_base + PCIE_APP_LTSSM_ENABLE);

			writel(readl(g_pcie->phy_base + 0x55*4) | (0x1 << 3), g_pcie->phy_base + 0x55*4);
			writel(readl(g_pcie->phy_base + 0x21*4) | (0x1 << 4), g_pcie->phy_base + 0x21*4);

			clk_disable_unprepare(g_pcie->phy_clk);
			clk_disable_unprepare(g_pcie->clk);

			poweronoff_flag = 0;

			return;
		}
	}
}
EXPORT_SYMBOL(exynos_pcie_poweroff);

void exynos_pcie_suspend(void)
{
	int __maybe_unused count = 0;
	u32 __maybe_unused val;

	if (!probe_ok)
		return;

	val = readl(g_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if(!(val >= 0x0d && val <= 0x15)) {
		printk("Link is already down\n");
		return;
	}

	writel(0x0, g_pcie->elbi_base + 0xa8);

	writel(0x1, g_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
	writel(0x1, g_pcie->elbi_base + 0xac);
	writel(0x13, g_pcie->elbi_base + 0xb0);
	writel(0x19, g_pcie->elbi_base + 0xd0);
	writel(0x1, g_pcie->elbi_base + 0xa8);
	while (count < MAX_TIMEOUT) {
		if ((readl(g_pcie->elbi_base + PCIE_IRQ_PULSE) & IRQ_RADM_PM_TO_ACK)) {
			printk("ack message is ok\n");
			break;
		}

		udelay(10);
		count++;
	}
	writel(0x0, g_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);

	if (count >= MAX_TIMEOUT)
		printk("not received ack message\n");

	do {
		val = readl(g_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			printk("received Enter_L23_READY DLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_TIMEOUT);

	if (count >= MAX_TIMEOUT) {
		printk("enter L23_READY state failed\n");
		exynos_pcie_register_dump();
	}


	writel(readl(g_pcie->gpio_base + 0x10) & ~(0x3 << 10), g_pcie->gpio_base + 0x10);
	writel(readl(g_pcie->gpio_base + 4) & ~(0x1 << 5), g_pcie->gpio_base + 4);
}
EXPORT_SYMBOL(exynos_pcie_suspend);

void exynos_pcie_resume(void)
{
	if(!probe_ok)
		return;

	exynos_pcie_host_init(&g_pcie->pp);
}
EXPORT_SYMBOL(exynos_pcie_resume);

#ifdef CONFIG_PM
static int exynos_pcie_prepare(struct device *dev)
{
	if (!check_rev())
		writel(0x1, g_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);

	return 0;
}

static void exynos_pcie_complete(struct device *dev)
{
	if (!check_rev())
		writel(0x0, g_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
}

static int exynos_pcie_suspend_noirq(struct device *dev)
{
	int __maybe_unused count = 0;
	u32 __maybe_unused val;

	if (!check_rev())
		return 0;

	if (!probe_ok)
		return 0;

	if (!poweronoff_flag) {
		printk("wifi already off\n");
		return 0;
	}

	writel(0x0, g_pcie->elbi_base + 0xa8);

	val = readl(g_pcie->block_base + PCIE_PHY_GLOBAL_RESET);
	val |= 0x1 << 5;
	writel(val, g_pcie->block_base + PCIE_PHY_GLOBAL_RESET);

	writel(0x1, g_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
	writel(0x1, g_pcie->elbi_base + 0xac);
	writel(0x13, g_pcie->elbi_base + 0xb0);
	writel(0x19, g_pcie->elbi_base + 0xd0);
	writel(0x1, g_pcie->elbi_base + 0xa8);
	while (count < MAX_TIMEOUT) {
		if ((readl(g_pcie->elbi_base + PCIE_IRQ_PULSE) & IRQ_RADM_PM_TO_ACK)) {
			printk("ack message is ok\n");
			break;
		}

		udelay(10);
		count++;
	}

	if (count >= MAX_TIMEOUT)
		printk("cannot receive ack message from wifi\n");

	writel(0x0, g_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);

	do {
		val = readl(g_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			printk("received Enter_L23_READY DLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_TIMEOUT);

	if (count >= MAX_TIMEOUT)
		printk("cannot receive L23_READY DLLP packet\n");

	gpio_set_value(g_pcie->perst_gpio, 0);

	return 0;
}

static int exynos_pcie_resume_noirq(struct device *dev)
{
	if (!check_rev())
		return 0;

	if (!probe_ok)
		return 0;

	writel(readl(g_pcie->cmu_base) | (0x7 << 12), g_pcie->cmu_base);

	if (!poweronoff_flag) {
		exynos_pcie_resumed_phydown();
		return 0;
	}

	writel((readl(g_pcie->gpio_base + 0x8) & ~(0x3 << 12)) | (0x1 << 12), g_pcie->gpio_base + 0x8);
	udelay(20);
	writel(readl(g_pcie->gpio_base + 0x8) | (0x3 << 12), g_pcie->gpio_base + 0x8);

	gpio_set_value(g_pcie->perst_gpio, 1);
	msleep(80);
	exynos_pcie_host_init(&g_pcie->pp);

	/* L1.2 ASPM enable */
	dw_pcie_config_l1ss(&g_pcie->pp);

	writel(readl(g_pcie->elbi_base + PCIE_IRQ_SPECIAL), g_pcie->elbi_base + PCIE_IRQ_SPECIAL);

	return 0;
}

#else
#define exynos_pcie_prepare		NULL
#define exynos_pcie_complete		NULL
#define exynos_pcie_suspend_noirq	NULL
#define exynos_pcie_resume_noirq	NULL
#endif

static const struct dev_pm_ops exynos_pcie_dev_pm_ops = {
	.prepare	= exynos_pcie_prepare,
	.complete	= exynos_pcie_complete,
	.suspend_noirq	= exynos_pcie_suspend_noirq,
	.resume_noirq	= exynos_pcie_resume_noirq,
};

static struct platform_driver exynos_pcie_driver = {
	.remove		= __exit_p(exynos_pcie_remove),
	.driver = {
		.name	= "exynos-pcie",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_pcie_of_match),
		.pm	= &exynos_pcie_dev_pm_ops,
	},
};

static int exynos_pci_lpa_event(struct notifier_block *nb, unsigned long event, void *data)
{
	int ret = NOTIFY_OK;

	switch (event) {
	case PCI_LPA_PREPARE:
		break;
	case PCI_LPA_RESUME:
		exynos_pcie_resumed_phydown();
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return ret;
}

int check_pcie_link_status(void)
{
	u32 val;
	int linkStatus, deviceState;
	val = readl(g_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15) {
		linkStatus = 1;
	} else {
		printk("%s, Link is abnormal state(%x)\n", __func__, val);
		linkStatus = 0;
	}

	val = readl(g_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7;
        if (val == PCIE_D0_UNINIT_STATE) {
		printk("%s, Link is D0 uninit state(%x)\n", __func__, val);
		deviceState = 0;
	} else {
		deviceState = 1;
	}

	if (linkStatus && deviceState)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(check_pcie_link_status);

int check_wifi_op(void)
{
	if (!check_rev())
		return 1;
	else
		return poweronoff_flag;

}
EXPORT_SYMBOL(check_wifi_op);

void exynos_pci_lpa_resume(void)
{
	raw_notifier_call_chain(&pci_lpa_nh, PCI_LPA_RESUME, NULL);
}
EXPORT_SYMBOL_GPL(exynos_pci_lpa_resume);

/* Exynos PCIe driver does not allow module unload */

static int __init pcie_init(void)
{
	return platform_driver_probe(&exynos_pcie_driver, exynos_pcie_probe);
}
late_initcall(pcie_init);

MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe host controller driver");
MODULE_LICENSE("GPL v2");
