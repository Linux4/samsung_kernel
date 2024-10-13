/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include "pcie_remapper.h"
#include "mif_reg.h"

void pcie_set_remappers(struct pcie_mif *pcie)
{
	u32 cmd_dw;

	if (!pcie_is_on(pcie))
		return;

	/* Set remmapers */
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_MCPU0_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_MCPU0_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WLAN_MCPU0_RMP_BOOT_ADDR 0x%x\n",
			  (pcie->remap_addr_wlan >> 12));
	writel((pcie->remap_addr_wlan >> 12), pcie->registers_pert + PMU_WLAN_MCPU0_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_MCPU0_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_MCPU0_RMP_BOOT_ADDR 0x%x\n", cmd_dw);

	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_MCPU1_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_MCPU1_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WLAN_MCPU1_RMP_BOOT_ADDR 0x%x\n",
			  (pcie->remap_addr_wlan >> 12));
	writel((pcie->remap_addr_wlan >> 12), pcie->registers_pert + PMU_WLAN_MCPU1_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_MCPU1_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_MCPU1_RMP_BOOT_ADDR 0x%x\n", cmd_dw);

	cmd_dw = readl(pcie->registers_pert + PMU_WPAN_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WPAN_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WPAN_RMP_BOOT_ADDR 0x%x\n", (pcie->remap_addr_wpan >> 12));
	writel((pcie->remap_addr_wpan >> 12), pcie->registers_pert + PMU_WPAN_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WPAN_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WPAN_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
}
