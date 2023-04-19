/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/exynos-pci-noti.h>
#include <linux/exynos-pci-ctrl.h>
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-dbg.h"
#include "pcie-exynos-rc.h"

#include "pcie-exynos-phycal_common.h"

int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size);
void exynos_pcie_rc_register_dump(int ch_num);
void exynos_pcie_set_perst_gpio(int ch_num, bool on);
void remove_pcie_sys_file(struct device *dev);

void exynos_pcie_dbg_print_oatu_register(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;

	pcie_ops->rd_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_CR1_OUTBOUND2(0x400) = 0x%x\n", __func__, val);
	pcie_ops->rd_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_LOWER_BASE_OUTBOUND2(0x408) = 0x%x\n", __func__, val);
	pcie_ops->rd_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_UPPER_BASE_OUTBOUND2(0x40C) = 0x%x\n", __func__, val);
	pcie_ops->rd_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_LIMIT_OUTBOUND2(0x410) = 0x%x\n", __func__, val);
	pcie_ops->rd_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_LOWER_TARGET_OUTBOUND2(0x414) = 0x%x\n", __func__, val);
	pcie_ops->rd_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_UPPER_TARGET_OUTBOUND2(0x418) = 0x%x\n", __func__, val);
	pcie_ops->rd_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, &val);
	pcie_info(exynos_pcie, "%s:  PCIE_ATU_CR2_OUTBOUND2(0x404) = 0x%x\n", __func__, val);
}

void exynos_pcie_dbg_print_msi_register(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;

	pcie_ops->rd_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, &val);
	pcie_info(exynos_pcie, "PCIE_MSI_INTR0_MASK(0x%x):0x%x\n", PCIE_MSI_INTR0_MASK, val);
	pcie_ops->rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	pcie_info(exynos_pcie, "PCIE_MSI_INTR0_ENABLE(0x%x):0x%x\n", PCIE_MSI_INTR0_ENABLE, val);
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	pcie_info(exynos_pcie, "PCIE_IRQ2_EN: 0x%x\n", val);
	pcie_ops->rd_own_conf(pp, PCIE_MSI_ADDR_LO, 4, &val);
	pcie_info(exynos_pcie, "PCIE_MSI_ADDR_LO: 0x%x\n", val);
	pcie_ops->rd_own_conf(pp, PCIE_MSI_ADDR_HI, 4, &val);
	pcie_info(exynos_pcie, "PCIE_MSI_ADDR_HI: 0x%x\n", val);
	pcie_ops->rd_own_conf(pp, PCIE_MSI_INTR0_STATUS, 4, &val);
	pcie_info(exynos_pcie, "PCIE_MSI_INTR0_STATUS: 0x%x\n", val);
}

void exynos_pcie_dbg_dump_link_down_status(struct exynos_pcie *exynos_pcie)
{
	pcie_info(exynos_pcie, "LTSSM: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
	pcie_info(exynos_pcie, "LTSSM_H: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_CXPL_DEBUG_INFO_H));
	pcie_info(exynos_pcie, "DMA_MONITOR1: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR1));
	pcie_info(exynos_pcie, "DMA_MONITOR2: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR2));
	pcie_info(exynos_pcie, "DMA_MONITOR3: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR3));
	pcie_info(exynos_pcie, "PCIE link state is %d\n",
			exynos_pcie->state);
}

void exynos_pcie_dbg_print_link_history(struct exynos_pcie *exynos_pcie)
{
	u32 history_buffer[32];
	int i;

	for (i = 31; i >= 0; i--)
		history_buffer[i] = exynos_elbi_read(exynos_pcie,
				PCIE_HISTORY_REG(i));
	for (i = 31; i >= 0; i--)
		pcie_info(exynos_pcie, "LTSSM: 0x%02x, L1sub: 0x%x, D state: 0x%x\n",
				LTSSM_STATE(history_buffer[i]),
				L1SUB_STATE(history_buffer[i]),
				PM_DSTATE(history_buffer[i]));
}

void exynos_pcie_dbg_register_dump(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	struct pcie_port *pp = &pci->pp;
	u32 i, val_0, val_4, val_8, val_c;

	pcie_err(exynos_pcie, "%s: +++\n", __func__);
	/* ---------------------- */
	/* Link Reg : 0x0 ~ 0x47C */
	/* ---------------------- */
	pcie_err(exynos_pcie, "[Print SUB_CTRL region]\n");
	pcie_err(exynos_pcie, "offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x480; i += 0x10) {
		pcie_err(exynos_pcie, "ELBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_elbi_read(exynos_pcie, i + 0x0),
				exynos_elbi_read(exynos_pcie, i + 0x4),
				exynos_elbi_read(exynos_pcie, i + 0x8),
				exynos_elbi_read(exynos_pcie, i + 0xC));
	}
	pcie_err(exynos_pcie, "\n");

	/* ---------------------- */
	/* PHY Reg : 0x0 ~ 0x19C */
	/* ---------------------- */
	pcie_err(exynos_pcie, "[Print PHY region]\n");
	pcie_err(exynos_pcie, "offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x200; i += 0x10) {
		pcie_err(exynos_pcie, "PHY 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_read(exynos_pcie, i + 0x0),
				exynos_phy_read(exynos_pcie, i + 0x4),
				exynos_phy_read(exynos_pcie, i + 0x8),
				exynos_phy_read(exynos_pcie, i + 0xC));
	}
	/* common */
	pcie_err(exynos_pcie, "PHY 0x03F0:    0x%08x\n", exynos_phy_read(exynos_pcie, 0x3F0));

	/* lane0 */
	for (i = 0xE00; i < 0xED0; i += 0x10) {
		pcie_err(exynos_pcie, "PHY 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_read(exynos_pcie, i + 0x0),
				exynos_phy_read(exynos_pcie, i + 0x4),
				exynos_phy_read(exynos_pcie, i + 0x8),
				exynos_phy_read(exynos_pcie, i + 0xC));
	}
	pcie_err(exynos_pcie, "PHY 0x0FC0:    0x%08x\n", exynos_phy_read(exynos_pcie, 0xFC0));

	/* lane1 */
	for (i = (0xE00 + 0x800); i < (0xED0 + 0x800); i += 0x10) {
		pcie_err(exynos_pcie, "PHY 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_read(exynos_pcie, i + 0x0),
				exynos_phy_read(exynos_pcie, i + 0x4),
				exynos_phy_read(exynos_pcie, i + 0x8),
				exynos_phy_read(exynos_pcie, i + 0xC));
	}
	pcie_err(exynos_pcie, "PHY 0x17C0 : 0x%08x\n", exynos_phy_read(exynos_pcie, 0xFC0 + 0x800));
	pcie_err(exynos_pcie, "\n");

	/* ---------------------- */
	/* PHY PCS : 0x0 ~ 0x19C */
	/* ---------------------- */
	pcie_err(exynos_pcie, "[Print PHY_PCS region]\n");
	pcie_err(exynos_pcie, "offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x200; i += 0x10) {
		pcie_err(exynos_pcie, "PCS 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_pcs_read(exynos_pcie, i + 0x0),
				exynos_phy_pcs_read(exynos_pcie, i + 0x4),
				exynos_phy_pcs_read(exynos_pcie, i + 0x8),
				exynos_phy_pcs_read(exynos_pcie, i + 0xC));
	}
	pcie_err(exynos_pcie, "\n");

	pcie_err(exynos_pcie, "[Print PHY_UDBG region]\n");
	pcie_err(exynos_pcie, "offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x1000; i += 0x10) {
		pcie_err(exynos_pcie, "PHYUDBG(+0xC000) 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0x0),
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0x4),
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0x8),
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0xC));
	}
	pcie_err(exynos_pcie, "\n");

	/* ---------------------- */
	/* DBI : 0x0 ~ 0x8FC */
	/* ---------------------- */
	pcie_err(exynos_pcie, "[Print DBI region]\n");
	pcie_err(exynos_pcie, "offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x900; i += 0x10) {
		pcie_ops->rd_own_conf(pp, i + 0x0, 4, &val_0);
		pcie_ops->rd_own_conf(pp, i + 0x4, 4, &val_4);
		pcie_ops->rd_own_conf(pp, i + 0x8, 4, &val_8);
		pcie_ops->rd_own_conf(pp, i + 0xC, 4, &val_c);
		pcie_err(exynos_pcie, "DBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i, val_0, val_4, val_8, val_c);
	}
	pcie_err(exynos_pcie, "\n");
	pcie_err(exynos_pcie, "%s: ---\n", __func__);
}

static int chk_pcie_dislink(struct exynos_pcie *exynos_pcie)
{
	int test_result = 0;
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	u32 linkup_offset = exynos_pcie->linkup_offset;
	u32 val;

	if (!pcie_ops->poweroff) {
		pcie_err(exynos_pcie, "Can't find PCIe poweroff function\n");
		return -1;
	}

	pcie_ops->poweroff(exynos_pcie->ch_num);

	pm_runtime_get_sync(exynos_pcie->pci->dev);

	val = exynos_elbi_read(exynos_pcie,
				linkup_offset) & 0x1f;
	if (val == 0x15 || val == 0x0) {
		pcie_info(exynos_pcie, "PCIe link Down test Success.\n");
	} else {
		pcie_info(exynos_pcie, "PCIe Link Down test Fail...\n");
		test_result = -1;
	}

	pm_runtime_put_sync(exynos_pcie->pci->dev);

	return test_result;
}

static int chk_link_recovery(struct exynos_pcie *exynos_pcie)
{
	int test_result = 0;
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	u32 linkup_offset = exynos_pcie->linkup_offset;
	u32 val;

	if (!pcie_ops->poweroff) {
		pcie_err(exynos_pcie, "Can't find PCIe poweroff function\n");
		return -1;
	}

	if (!pcie_ops->poweron) {
		pcie_err(exynos_pcie, "Can't find PCIe poweron function\n");
		return -1;
	}

	exynos_elbi_write(exynos_pcie, 0x1, exynos_pcie->app_req_exit_l1);
	val = exynos_elbi_read(exynos_pcie, exynos_pcie->app_req_exit_l1_mode);
	val &= ~APP_REQ_EXIT_L1_MODE;
	exynos_elbi_write(exynos_pcie, val, exynos_pcie->app_req_exit_l1_mode);
	pcie_info(exynos_pcie, "%s: Before set perst, gpio val = %d\n",
		__func__, gpio_get_value(exynos_pcie->perst_gpio));
	gpio_set_value(exynos_pcie->perst_gpio, 0);
	msleep(5000);
	pcie_info(exynos_pcie, "%s: After set perst, gpio val = %d\n",
		__func__, gpio_get_value(exynos_pcie->perst_gpio));
	val = exynos_elbi_read(exynos_pcie, exynos_pcie->app_req_exit_l1_mode);
	val |= APP_REQ_EXIT_L1_MODE;
	exynos_elbi_write(exynos_pcie, val, exynos_pcie->app_req_exit_l1_mode);
	exynos_elbi_write(exynos_pcie, 0x0, exynos_pcie->app_req_exit_l1);
	msleep(5000);

	val = exynos_elbi_read(exynos_pcie,
				linkup_offset) & 0x1f;
	if (val >= 0x0d && val <= 0x14) {
		pcie_info(exynos_pcie, "PCIe link Recovery test Success.\n");
	} else {
		/* If recovery callback is defined, pcie poweron
		 * function will not be called.
		 */
		pcie_ops->poweroff(exynos_pcie->ch_num);
		test_result = pcie_ops->poweron(exynos_pcie->ch_num);
		if (test_result != 0) {
			pcie_info(exynos_pcie, "PCIe Link Recovery test Fail...\n");
			return test_result;
		}
		val = exynos_elbi_read(exynos_pcie,
				linkup_offset) & 0x1f;
		if (val >= 0x0d && val <= 0x14) {
			pcie_info(exynos_pcie, "PCIe link Recovery test Success.\n");
		} else {
			pcie_info(exynos_pcie, "PCIe Link Recovery test Fail...\n");
			test_result = -1;
		}
	}

	return test_result;
}

static int chk_epmem_access(struct exynos_pcie *exynos_pcie)
{
	u32 val;
	int test_result = 0;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct exynos_pcie_ops *pcie_ops
			= &exynos_pcie->exynos_pcie_ops;

	struct pci_bus *ep_pci_bus;
	void __iomem *reg_addr;
	struct resource_entry *tmp = NULL, *entry = NULL;

	if (!pcie_ops->rd_other_conf) {
		pcie_err(exynos_pcie, "Can't find PCIe read other configuration function\n");
		return -1;
	}

	if (!pcie_ops->wr_other_conf) {
		pcie_err(exynos_pcie, "Can't find PCIe write other configuration function\n");
		return -1;
	}
	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;

	ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
	if (ep_pci_bus == NULL) {
		pcie_err(exynos_pcie, "Can't find PCIe ep_pci_bus structure\n");
		return -1;
	}

	pcie_ops->wr_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, lower_32_bits(entry->res->start));
	pcie_ops->rd_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, &val);
	pcie_info(exynos_pcie, "Set BAR0 : 0x%x\n", val);

	reg_addr = ioremap(entry->res->start, SZ_4K);

	val = readl(reg_addr);
	iounmap(reg_addr);
	if (val != 0xffffffff) {
		pcie_info(exynos_pcie, "PCIe EP Outbound mem access Success.\n");
	} else {
		pcie_info(exynos_pcie, "PCIe EP Outbound mem access Fail...\n");
		test_result = -1;
	}

	return test_result;
}

static int chk_epconf_access(struct exynos_pcie *exynos_pcie)
{
	u32 val;
	int test_result = 0;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_bus *ep_pci_bus;
	struct exynos_pcie_ops *pcie_ops
			= &exynos_pcie->exynos_pcie_ops;

	if (!pcie_ops->rd_other_conf) {
		pcie_err(exynos_pcie, "Can't find PCIe read other configuration function\n");
		return -1;
	}

	if (!pcie_ops->wr_other_conf) {
		pcie_err(exynos_pcie, "Can't find PCIe write other configuration function\n");
		return -1;
	}

	ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

	pcie_ops->rd_other_conf(pp, ep_pci_bus, 0, 0x0, 4, &val);
	pcie_info(exynos_pcie, "PCIe EP Vendor ID/Device ID = 0x%x\n", val);

	pcie_ops->wr_other_conf(pp, ep_pci_bus,
					0, PCI_COMMAND, 4, 0x146);
	pcie_ops->rd_other_conf(pp, ep_pci_bus,
					0, PCI_COMMAND, 4, &val);
	if ((val & 0xfff) == 0x146) {
		pcie_info(exynos_pcie, "PCIe EP conf access Success.\n");
	} else {
		pcie_info(exynos_pcie, "PCIe EP conf access Fail...\n");
		test_result = -1;
	}

	return test_result;
}

static int chk_dbi_access(struct exynos_pcie *exynos_pcie)
{
	u32 val;
	int test_result = 0;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct exynos_pcie_ops *pcie_ops
			= &exynos_pcie->exynos_pcie_ops;

	if (!pcie_ops->rd_own_conf) {
		pcie_err(exynos_pcie, "Can't find PCIe read own configuration function\n");
		return -1;
	}

	if (!pcie_ops->wr_own_conf) {
		pcie_err(exynos_pcie, "Can't find PCIe write own configuration function\n");
		return -1;
	}

	pcie_ops->wr_own_conf(pp, PCI_COMMAND, 4, 0x140);
	pcie_ops->rd_own_conf(pp, PCI_COMMAND, 4, &val);
	if ((val & 0xfff) == 0x140) {
		pcie_info(exynos_pcie, "PCIe DBI access Success.\n");
	} else {
		pcie_info(exynos_pcie, "PCIe DBI access Fail...\n");
		test_result = -1;
	}

	return test_result;
}

static int chk_pcie_link(struct exynos_pcie *exynos_pcie)
{
	int test_result = 0;
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	u32 linkup_offset = exynos_pcie->linkup_offset;
	u32 val;

	if (!pcie_ops->poweron) {
		pcie_err(exynos_pcie, "Can't find PCIe poweron function\n");
		return -1;
	}

	test_result = pcie_ops->poweron(exynos_pcie->ch_num);
	if (test_result != 0) {
		pcie_info(exynos_pcie, "PCIe Link test Fail...\n");
		return test_result;
	}

	val = exynos_elbi_read(exynos_pcie,
					linkup_offset) & 0x1f;
	if (val >= 0x0d && val <= 0x14) {
		pcie_info(exynos_pcie, "PCIe link test Success.\n");
	} else {
		pcie_info(exynos_pcie, "PCIe Link test Fail...\n");
		test_result = -1;
	}

	return test_result;
}

int exynos_pcie_dbg_unit_test(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	int ret = 0;

	if (exynos_pcie->ssd_gpio < 0) {
		pcie_warn(exynos_pcie, "can't find ssd pin info. Need to check EP device power pin\n");
	} else {
		gpio_set_value(exynos_pcie->ssd_gpio, 1);
		mdelay(100);
	}

	if (exynos_pcie->wlan_gpio < 0) {
		pcie_warn(exynos_pcie, "can't find wlan pin info. Need to check EP device power pin\n");
	} else {
		gpio_direction_output(exynos_pcie->wlan_gpio, 0);
		gpio_set_value(exynos_pcie->wlan_gpio, 1);
		mdelay(100);
	}

	pcie_info(exynos_pcie, "1. Test PCIe LINK... \n");
	/* Test PCIe Link */
	if (chk_pcie_link(exynos_pcie)) {
		pcie_info(exynos_pcie, "PCIe UNIT test FAIL[1/6]!!!\n");
		ret = -1;
		goto done;
	}

	pcie_info(exynos_pcie, "2. Test DBI access... \n");
	/* Test PCIe DBI access */
	if (chk_dbi_access(exynos_pcie)) {
		pcie_info(exynos_pcie, "PCIe UNIT test FAIL[2/6]!!!\n");
		ret = -2;
		goto done;
	}

	pcie_info(exynos_pcie, "3. Test EP configuration access... \n");
	/* Test EP configuration access */
	if (chk_epconf_access(exynos_pcie)) {
		pcie_info(exynos_pcie, "PCIe UNIT test FAIL[3/6]!!!\n");
		ret = -3;
		goto done;
	}

	pcie_info(exynos_pcie, "4. Test EP Outbound memory region... \n");
	/* Test EP Outbound memory region */
	if (chk_epmem_access(exynos_pcie)) {
		pcie_info(exynos_pcie, "PCIe UNIT test FAIL[4/6]!!!\n");
		ret = -4;
		goto done;
	}

	pcie_info(exynos_pcie, "5. Test PCIe Link recovery... \n");
	/* PCIe Link recovery test */
	if (chk_link_recovery(exynos_pcie)) {
		pcie_info(exynos_pcie, "PCIe UNIT test FAIL[5/6]!!!\n");
		ret = -5;
		goto done;
	}

	pcie_info(exynos_pcie, "6. Test PCIe Dislink... \n");
	/* PCIe DisLink Test */
	if (chk_pcie_dislink(exynos_pcie)) {
		pcie_info(exynos_pcie, "PCIe UNIT test FAIL[6/6]!!!\n");
		ret = -6;
		goto done;
	}

done:
	return ret;
}

int exynos_pcie_dbg_link_test(struct device *dev,
			struct exynos_pcie *exynos_pcie, int enable)
{
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	int ret;

	pcie_info(exynos_pcie, "TEST PCIe %sLink Test \n", enable ? "" : "Dis");

	if (enable) {
		if (!pcie_ops->poweron) {
			pcie_err(exynos_pcie, "Can't find PCIe poweron function\n");
			return -1;
		}

		if (exynos_pcie->ssd_gpio < 0) {
			pcie_warn(exynos_pcie, "can't find ssd pin info. Need to check EP device power pin\n");
		} else {
			gpio_set_value(exynos_pcie->ssd_gpio, 1);
			mdelay(100);
		}

		if (exynos_pcie->wlan_gpio < 0) {
			pcie_warn(exynos_pcie, "can't find wlan pin info. Need to check EP device power pin\n");
		} else {
			pcie_err(exynos_pcie, "## make gpio direction to output\n");
			gpio_direction_output(exynos_pcie->wlan_gpio, 0);

			pcie_err(exynos_pcie, "## make gpio set high\n");
			gpio_set_value(exynos_pcie->wlan_gpio, 1);
			mdelay(100);
		}

		mdelay(100);
		ret = pcie_ops->poweron(exynos_pcie->ch_num);
	} else {
		if (!pcie_ops->poweroff) {
			pcie_err(exynos_pcie, "Can't find PCIe poweroff function\n");
			return -1;
		}
		pcie_ops->poweroff(exynos_pcie->ch_num);

		if (exynos_pcie->ssd_gpio < 0) {
			pcie_warn(exynos_pcie, "can't find ssd pin info. Need to check EP device power pin\n");
		} else {
			gpio_set_value(exynos_pcie->ssd_gpio, 0);
		}

		if (exynos_pcie->wlan_gpio < 0) {
			pcie_warn(exynos_pcie, "can't find wlan pin info. Need to check EP device power pin\n");
		} else {
			gpio_set_value(exynos_pcie->wlan_gpio, 0);
		}
		ret = 0;
	}

	return ret;

}

static ssize_t exynos_pcie_eom1_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 0, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!!\n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret)
		lane_width = 0;

	while (current_cnt != EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX) {
		len += snprintf(buf + len, PAGE_SIZE,
				"%u %u %lu\n",
				eom_result[i][current_cnt].phase,
				eom_result[i][current_cnt].vref,
				eom_result[i][current_cnt].err_cnt);
		current_cnt++;
		test_cnt++;
		if (test_cnt == 100)
			break;
	}

	if (current_cnt == EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX)
		current_cnt = 0;

exit:
	return len;
}

static ssize_t exynos_pcie_eom1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		if (exynos_pcie->phy->phy_ops.phy_eom != NULL)
			exynos_pcie->phy->phy_ops.phy_eom(dev,
					exynos_pcie->phy_base);
		break;
	}

	return count;
}

static ssize_t exynos_pcie_eom2_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/* prevent to print kerenl warning message
	   eom1_store function do all operation to get eom data */

	return count;
}

static ssize_t exynos_pcie_eom2_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 1, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!!\n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		lane_width = 0;
		len += snprintf(buf + len, PAGE_SIZE,
				"can't get num of lanes !!\n");
		goto exit;
	}

	if (lane_width == 1) {
		len += snprintf(buf + len, PAGE_SIZE,
				"EOM2NULL\n");
		goto exit;
	}

	while (current_cnt != EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX) {
		len += snprintf(buf + len, PAGE_SIZE,
				"%u %u %lu\n",
				eom_result[i][current_cnt].phase,
				eom_result[i][current_cnt].vref,
				eom_result[i][current_cnt].err_cnt);
		current_cnt++;
		test_cnt++;
		if (test_cnt == 100)
			break;
	}

	if (current_cnt == EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX)
		current_cnt = 0;

exit:
	return len;
}

static DEVICE_ATTR(eom1, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom1_show, exynos_pcie_eom1_store);

static DEVICE_ATTR(eom2, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom2_show, exynos_pcie_eom2_store);

int create_pcie_eom_file(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	int num_lane;

	ret = of_property_read_u32(np, "num-lanes", &num_lane);
	if (ret)
		num_lane = 0;

	ret = device_create_file(dev, &dev_attr_eom1);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
				__func__, ret);
		return ret;
	}

	if (num_lane > 0) {
		ret = device_create_file(dev, &dev_attr_eom2);
		if (ret) {
			dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
					__func__, ret);
			return ret;
		}

	}

	return 0;
}

void remove_pcie_eom_file(struct device *dev)
{
	if (dev == NULL) {
		pr_err("Can't remove EOM files.\n");
		return;
	}
	device_remove_file(dev, &dev_attr_eom1);
}

static ssize_t exynos_pcie_rc_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	ret += snprintf(buf + ret, PAGE_SIZE - ret, ">>>> PCIe Test <<<<\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "0 : PCIe Unit Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "1 : Link Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "2 : DisLink Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "3 : Check LTSSM state\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "10 : L1.2 disable\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "11 : L1.2 enable\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "20 : TEM Test\n");

	return ret;
}

static ssize_t exynos_pcie_rc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	/*
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	*/
	int ret = 0;
	dma_addr_t phys_addr;
	void *virt_addr, *alloc;


	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		pcie_info(exynos_pcie, "## PCIe UNIT test START ##\n");
		ret = exynos_pcie_dbg_unit_test(dev, exynos_pcie);
		if (ret) {
			pcie_err(exynos_pcie, "PCIe UNIT test failed (%d)\n", ret);
			break;
		}
		pcie_err(exynos_pcie, "## PCIe UNIT test SUCCESS!!##\n");
		break;
	case 1:
		pcie_info(exynos_pcie, "## PCIe establish link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);
		if (ret) {
			pcie_err(exynos_pcie, "PCIe establish link test failed (%d)\n", ret);
			break;
		}
		pcie_err(exynos_pcie, "PCIe establish link test success\n");
		break;
	case 2:
		pcie_info(exynos_pcie, "## PCIe dis-link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 0);
		if (ret) {
			pcie_err(exynos_pcie, "PCIe dis-link test failed (%d)\n", ret);
			break;
		}
		pcie_err(exynos_pcie, "PCIe dis-link test success\n");
		break;
	case 3:
		pcie_info(exynos_pcie, "## LTSSM ##\n");
		ret = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0xff;
		pcie_info(exynos_pcie, "PCIE_ELBI_RDLH_LINKUP :0x%x \n", ret);
		break;
	case 10:
		pcie_info(exynos_pcie, "L1.2 Disable....\n");
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		break;

	case 11:
		pcie_info(exynos_pcie, "L1.2 Enable....\n");
		exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		break;

	case 12:
		pcie_info(exynos_pcie, "l1ss_ctrl_id_state = 0x%08x\n",
				exynos_pcie->l1ss_ctrl_id_state);
		pcie_info(exynos_pcie, "LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
				exynos_phy_pcs_read(exynos_pcie, 0x188));
		break;

	case 13:
		pcie_info(exynos_pcie, "%s: force perst setting \n", __func__);
		exynos_pcie_set_perst_gpio(1, 0);
		break;

	case 14:
		/* set input clk path change to disable */
		pcie_info(exynos_pcie, "%s: force set input clk path to disable", __func__);
		if (exynos_pcie->phy->phy_ops.phy_input_clk_change != NULL)
			exynos_pcie->phy->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		break;

	case 15:
		pcie_info(exynos_pcie, "%s: force all pwndn", __func__);
		exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
		break;

	case 16:
		exynos_pcie_rc_set_outbound_atu(1, 0x47200000, 0x0, SZ_1M);
		break;

	case 17:
		exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
		break;
	case 20: /* For the code coverage check */
		pcie_info(exynos_pcie, "Start TEM test.\n");

		pcie_info(exynos_pcie, "1. Start Unit Test\n");
		exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		exynos_pcie_dbg_unit_test(dev, exynos_pcie);


		pm_runtime_get_sync(dev);
		pcie_info(exynos_pcie, "2. PCIe Power on\n");
		exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);

		pcie_info(exynos_pcie, "3. SysMMU mapping\n");
		if (exynos_pcie->use_sysmmu) {
			virt_addr = dma_alloc_coherent(&exynos_pcie->ep_pci_dev->dev,
					SZ_4K, &phys_addr, GFP_KERNEL);
			alloc = kmalloc(SZ_4K, GFP_KERNEL);
			dma_map_single(&exynos_pcie->ep_pci_dev->dev,
					alloc,
					SZ_4K,
					DMA_FROM_DEVICE);
			dma_unmap_single(&exynos_pcie->ep_pci_dev->dev, phys_addr,
					SZ_4K, DMA_FROM_DEVICE);
			dma_free_coherent(&exynos_pcie->ep_pci_dev->dev, SZ_4K, virt_addr,
					phys_addr);
		}


		pcie_info(exynos_pcie, "4. Check EP related function\n");
		exynos_pcie_rc_check_function_validity(exynos_pcie);
		exynos_pcie_dbg_print_oatu_register(exynos_pcie);

		pm_runtime_put_sync(dev);

		remove_pcie_sys_file(NULL);
		remove_pcie_eom_file(NULL);

		if (exynos_pcie->use_sysmmu)
			kfree(alloc);
		break;
	}

	return count;
}

static DEVICE_ATTR(pcie_rc_test, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_rc_show, exynos_pcie_rc_store);

int create_pcie_sys_file(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_pcie_rc_test);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for test(%d)\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

void remove_pcie_sys_file(struct device *dev)
{
	if (dev == NULL) {
		pr_err("Can't remove pcie_rc_test file.\n");
		return;
	}
	device_remove_file(dev, &dev_attr_pcie_rc_test);
}

