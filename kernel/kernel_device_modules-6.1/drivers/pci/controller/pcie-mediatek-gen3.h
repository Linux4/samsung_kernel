/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_PCIE_MEDIATEK_GEN3_H__
#define __MTK_PCIE_MEDIATEK_GEN3_H__

#include <linux/pci.h>

int mtk_pcie_probe_port(int port);
int mtk_pcie_remove_port(int port);
int mtk_pcie_soft_off(struct pci_bus *bus);
int mtk_pcie_soft_on(struct pci_bus *bus);
int mtk_msi_unmask_to_other_mcu(struct irq_data *data, u32 group);
u32 mtk_pcie_dump_link_info(int port);
int mtk_pcie_disable_data_trans(int port);
int mtk_pcie_mask_msi_to_ap(int port, u32 msi_addr, u32 mask);
int mtk_pcie_hw_control_vote(int port, bool hw_mode_en, u8 who);

#endif
