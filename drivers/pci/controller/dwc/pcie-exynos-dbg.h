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

#ifndef __PCIE_EXYNOS_RC_DBG_H
#define __PCIE_EXYNOS_RC_DBG_H

int exynos_pcie_dbg_unit_test(struct device *dev, struct exynos_pcie *exynos_pcie);
int exynos_pcie_dbg_link_test(struct device *dev, struct exynos_pcie *exynos_pcie, int enable);
int create_pcie_eom_file(struct device *dev);
void remove_pcie_eom_file(struct device *dev);
void exynos_pcie_dbg_register_dump(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_link_history(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_dump_link_down_status(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_msi_register(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_oatu_register(struct exynos_pcie *exynos_pcie);
int create_pcie_sys_file(struct device *dev);
void remove_pcie_sys_file(struct device *dev);

/* For coverage check */
void exynos_pcie_rc_check_function_validity(struct exynos_pcie *exynos_pcie);
#endif
