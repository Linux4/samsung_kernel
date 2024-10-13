/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 * Supporting PCIe Gen4
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyounghye Yun <k-hye.yun@samsung.com>
 *         Seungo Jang <seungo.jang@samsung.com>
 *         Kwangho Kim <kwangho2.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <dt-bindings/pci/pci.h>
#include <linux/exynos-pci-noti.h>
#include <linux/exynos-pci-ctrl.h>
#include <linux/dma-map-ops.h>
#include <linux/pm_runtime.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
#include <soc/samsung/exynos/exynos-itmon.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-pm.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#include <soc/samsung/exynos_pm_qos.h>
#endif
/* #include <linux/kernel.h> To use dump_stack() */
#include "pcie-designware.h"
#include "pcie-exynos-phycal_common.h"
#include "pcie-exynos-rc-dwphy.h"
#if IS_ENABLED(CONFIG_SOC_S5E9945)
#include "pcie-exynos9945-dwphy-rom.h"
#endif

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#include <soc/samsung/shm_ipc.h>
#endif	/* CONFIG_LINK_DEVICE_PCIE */

#include <soc/samsung/exynos-pcie-iommu-exp.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <soc/samsung/exynos-pmu-if.h>

static struct exynos_pcie g_pcie_rc[MAX_RC_NUM];
static struct separated_msi_vector sep_msi_vec[MAX_RC_NUM][MAX_MSI_CTRLS];
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
static struct exynos_pm_qos_request exynos_pcie_int_qos[MAX_RC_NUM];
#endif

static struct pci_dev *exynos_pcie_get_pci_dev(struct exynos_pcie *exynos_pcie);
static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				   const struct cpumask *mask, bool force);
static int exynos_pcie_msi_set_wake(struct irq_data *irq_data, unsigned int on);
static void exynos_pcie_msi_irq_enable(struct irq_data *data);
static void exynos_pcie_msi_irq_disable(struct irq_data *data);
static inline void exynos_pcie_enable_sep_msi(struct exynos_pcie *exynos_pcie);
static void exynos_pcie_rc_prog_viewport_cfg0(struct dw_pcie_rp *pp, u32 busdev);
static void exynos_pcie_soc_specific_ctrl(struct exynos_pcie *exynos_pcie,
					  enum exynos_pcie_opr_state state);
static void exynos_pcie_cold_reset(struct exynos_pcie *exynos_pcie);
static void exynos_pcie_rc_disable_unused_lane(struct exynos_pcie *exynos_pcie);
static void exynos_pcie_rc_lane_config(struct exynos_pcie *exynos_pcie, int num);

static void exynos_pcie_mem_copy_epdev(struct exynos_pcie *exynos_pcie)
{
	struct device *epdev = &exynos_pcie->ep_pci_dev->dev;

	pcie_info("[%s] COPY ep_dev to PCIe memory struct\n", __func__);
	memcpy(&exynos_pcie->dup_ep_dev, epdev,
			sizeof(exynos_pcie->dup_ep_dev));
	exynos_pcie->dup_ep_dev.dma_ops = NULL;

	return;
}

static void *exynos_pcie_dma_alloc_attrs(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flags,
		unsigned long attrs)
{
	void *coherent_ptr;
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
	int ch_num = 0;
	int ret = 0;
	struct exynos_pcie *exynos_pcie;

	if (unlikely(dev == NULL)) {
		pr_err("EP device is NULL!!!\n");
		return NULL;
	}
	ch_num = pci_domain_nr(epdev->bus);

	exynos_pcie = &g_pcie_rc[ch_num];
	/* Some EP device need to copy EP device structure again. */
	if (exynos_pcie->copy_dup_ep == 0) {
		memcpy(&exynos_pcie->dup_ep_dev, dev,
				sizeof(exynos_pcie->dup_ep_dev));
		exynos_pcie->dup_ep_dev.dma_ops = NULL;
		exynos_pcie->copy_dup_ep = 1;
	}

	coherent_ptr = dma_alloc_attrs(&exynos_pcie->dup_ep_dev, size, dma_handle, flags, attrs);

	if (coherent_ptr != NULL) {
		ret = pcie_iommu_map(*dma_handle, *dma_handle, size,
				0, ch_num);
		if (ret != 0) {
			pr_err("DMA alloc - Can't map PCIe SysMMU table!!!\n");
			dma_free_attrs(&exynos_pcie->dup_ep_dev, size, coherent_ptr, *dma_handle, attrs);
			return NULL;
		}
	}
	return coherent_ptr;

}

static void exynos_pcie_dma_free_attrs(struct device *dev, size_t size,
		void *vaddr, dma_addr_t dma_handle,
		unsigned long attrs)
{
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
	int ch_num = 0;
	struct exynos_pcie *exynos_pcie;

	if (unlikely(dev == NULL)) {
		pr_err("EP device is NULL!!!\n");
		return;
	}
	ch_num = pci_domain_nr(epdev->bus);

	exynos_pcie = &g_pcie_rc[ch_num];
	dma_free_attrs(&exynos_pcie->dup_ep_dev, size, vaddr, dma_handle, attrs);

	pcie_iommu_unmap(dma_handle, size, ch_num);
}

static dma_addr_t exynos_pcie_dma_map_page_attrs(struct device *dev,
		struct page *page, unsigned long offset,
		size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	dma_addr_t dev_addr;
	int ch_num = 0;
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
	int ret = 0;
	struct exynos_pcie *exynos_pcie;

	if (unlikely(dev == NULL)) {
		pr_err("EP device is NULL!!!\n");
		return -EINVAL;
	}
	ch_num = pci_domain_nr(epdev->bus);

	exynos_pcie = &g_pcie_rc[ch_num];
	dev_addr = dma_map_page_attrs(&exynos_pcie->dup_ep_dev,
			page, offset, size, dir, attrs);
	if (dev_addr != DMA_MAPPING_ERROR) {
		ret = pcie_iommu_map(dev_addr, dev_addr, size, 0, ch_num);
		if (ret != 0) {
			pr_err("DMA map - Can't map PCIe SysMMU table!!!\n");

			return DMA_MAPPING_ERROR;
		}
	}

	return dev_addr;
}

static void exynos_pcie_dma_unmap_page_attrs(struct device *dev,
		dma_addr_t dev_addr,
		size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	int ch_num = 0;
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
	struct exynos_pcie *exynos_pcie;

	if (unlikely(dev == NULL)) {
		pr_err("EP device is NULL!!!\n");
		return;
	}
	ch_num = pci_domain_nr(epdev->bus);

	exynos_pcie = &g_pcie_rc[ch_num];
	dma_unmap_page_attrs(&exynos_pcie->dup_ep_dev, dev_addr, size, dir, attrs);

	pcie_iommu_unmap(dev_addr, size, ch_num);
}

static struct dma_map_ops exynos_pcie_dma_ops = {
	.alloc = exynos_pcie_dma_alloc_attrs,
	.free = exynos_pcie_dma_free_attrs,
	.map_page = exynos_pcie_dma_map_page_attrs,
	.unmap_page = exynos_pcie_dma_unmap_page_attrs,
};

static void exynos_pcie_set_dma_ops(struct device *dev)
{
	set_dma_ops(dev, &exynos_pcie_dma_ops);
}

int exynos_pcie_set_perst(int ch_num, bool on)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -EPERM;
	}

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info("%s: Can't set PERST.(Link Down State)\n", __func__);
		return -EPERM;
	}

	if (gpio_get_value(exynos_pcie->perst_gpio) == on) {
		pcie_info("%s: Can't set PERST.(already set)\n", __func__);
		return -EINVAL;
	}

	exynos_pcie_dbg_print_link_history(exynos_pcie);
	exynos_pcie_dbg_register_dump(exynos_pcie);

	pcie_info("%s: force setting for abnormal state\n", __func__);
	if (on) {
		gpio_set_value(exynos_pcie->perst_gpio, 1);
		pcie_info("%s: Set PERST to HIGH, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	} else {
		gpio_set_value(exynos_pcie->perst_gpio, 0);
		pcie_info("%s: Set PERST to LOW, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	}

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_set_perst);

int exynos_pcie_set_perst_gpio(int ch_num, bool on)
{
	return exynos_pcie_set_perst(ch_num, on);
}
EXPORT_SYMBOL(exynos_pcie_set_perst_gpio);

void exynos_pcie_set_ready_cto_recovery(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	u32 val;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;

	pcie_info("[%s] +++\n", __func__);

	disable_irq(pp->irq);

	exynos_pcie_set_perst_gpio(ch_num, 0);

	/* LTSSM disable */
	pcie_info("[%s] LTSSM disable\n", __func__);
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_3);
	val &= ~SSC_GEN_CTRL_3_LTSSM_EN;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_3);
}
EXPORT_SYMBOL(exynos_pcie_set_ready_cto_recovery);

int exynos_pcie_get_irq_num(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;

	pcie_info("%s: pp->irq = %d\n", __func__, pp->irq);
	return pp->irq;
}
EXPORT_SYMBOL(exynos_pcie_get_irq_num);

int exynos_pcie_rc_rd_own_conf(struct dw_pcie_rp *pp, int where, int size, u32 *val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret = 0;
	u32 __maybe_unused reg_val;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	ret = dw_pcie_read(exynos_pcie->rc_dbi_base + (where), size, val);

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

int exynos_pcie_rc_wr_own_conf(struct dw_pcie_rp *pp, int where, int size, u32 val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret = 0;
	u32 __maybe_unused reg_val;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE)
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	ret = dw_pcie_write(exynos_pcie->rc_dbi_base + (where), size, val);

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

int exynos_pcie_rc_rd_other_conf(struct dw_pcie_rp *pp, struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 *val)
{
	int ret;
	u32 busdev;
	void __iomem *va_cfg_base;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));
	va_cfg_base = pp->va_cfg0_base;
	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	ret = dw_pcie_read(va_cfg_base + where, size, val);

	return ret;
}

int exynos_pcie_rc_wr_other_conf(struct dw_pcie_rp *pp, struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 val)
{
	int ret;
	u32 busdev;
	void __iomem *va_cfg_base;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));

	va_cfg_base = pp->va_cfg0_base;
	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	ret = dw_pcie_write(va_cfg_base + where, size, val);

	return ret;
}

static void exynos_pcie_rc_prog_viewport_cfg0(struct dw_pcie_rp *pp, u32 busdev)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	if (exynos_pcie->current_busdev == busdev)
		return;

	/* Program viewport 0 : OUTBOUND : CFG0 */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND0, 4, pp->cfg0_base);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND0, 4,
					(pp->cfg0_base >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND0, 4,
					pp->cfg0_base + pp->cfg0_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND0, 4, busdev);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND0, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND0, 4, PCIE_ATU_TYPE_CFG0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND0, 4, PCIE_ATU_ENABLE);
	exynos_pcie->current_busdev = busdev;
}

static void exynos_pcie_rc_prog_viewport_mem_outbound(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	struct resource_entry *tmp = NULL, *entry = NULL;

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;

	if (unlikely(entry == NULL)) {
		pcie_err("entry is NULL\n");
		return;
	}

	/* Program viewport 0 : OUTBOUND : MEM */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND1, 4, PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND1, 4, entry->res->start);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND1, 4,
					(entry->res->start >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
				   entry->res->start + resource_size(entry->res) - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND1, 4,
				   entry->res->start - entry->offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND1, 4,
				   upper_32_bits(entry->res->start - entry->offset));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND1, 4, PCIE_ATU_ENABLE);
}

int exynos_pcie_rc_set_bar(struct exynos_pcie *exynos_pcie, u32 bar_num)
{
	struct pci_dev *ep_pci_dev;
	u32 val;

	pcie_info("%s: +++\n", __func__);

	if (exynos_pcie->state == STATE_LINK_UP) {
		ep_pci_dev = exynos_pcie_get_pci_dev(exynos_pcie);
	} else {
		pcie_info("%s: PCIe link is not up\n", __func__);
		return -EPIPE;
	}

	/* EP BAR setup */
	ep_pci_dev->resource[bar_num].start = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset;
	ep_pci_dev->resource[bar_num].end = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset + exynos_pcie->btl_size;
	ep_pci_dev->resource[bar_num].flags = 0x82000000;
	if (pci_assign_resource(ep_pci_dev, bar_num) != 0)
		pcie_warn("%s: failed to assign PCI resource for %i\n", __func__, bar_num);

	pci_read_config_dword(ep_pci_dev, PCI_BASE_ADDRESS_0 + (bar_num * 0x4), &val);
	pcie_info("%s: Check EP BAR[%d] = 0x%x\n", __func__, bar_num, val);

	pcie_info("%s: ---\n", __func__);
	return 0;
}

int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	int ret;
	struct resource_entry *tmp = NULL, *entry = NULL;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;

	pcie_info("%s: +++\n", __func__);

	exynos_pcie->btl_target_addr = target_addr;
	exynos_pcie->btl_offset = offset;
	exynos_pcie->btl_size = size;

	pcie_info("%s: target_addr = 0x%x, offset = 0x%x, size = 0x%x\n", __func__,
			exynos_pcie->btl_target_addr,
			exynos_pcie->btl_offset,
			exynos_pcie->btl_size);

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;
	if (entry == NULL) {
		pcie_err("entry is NULL\n");
		return -EPIPE;
	}

	/* Only for BTL */
	/* 0x1420_0000 ~ (size -1) */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4,
			entry->res->start + SZ_2M);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4,
			((entry->res->start + SZ_2M) >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4,
			entry->res->start + SZ_2M + exynos_pcie->btl_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4,
			exynos_pcie->btl_target_addr + exynos_pcie->btl_offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, PCIE_ATU_ENABLE);

	exynos_pcie_dbg_print_oatu_register(exynos_pcie);

	ret = exynos_pcie_rc_set_bar(exynos_pcie, 2);

	pcie_info("%s: ---\n", __func__);

	return ret;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_outbound_atu);

static void __iomem *exynos_pcie_rc_own_conf_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	return dw_pcie_own_conf_map_bus(bus, devfn, where);
}

static int exynos_rc_generic_own_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	struct dw_pcie_rp *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	ret = pci_generic_config_read(bus, devfn, where, size, val);

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static int exynos_rc_generic_own_config_write(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 val)
{
	struct dw_pcie_rp *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE)
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	ret = pci_generic_config_write(bus, devfn, where, size, val);

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static int exynos_pcie_rc_check_link(struct dw_pcie *pci)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (exynos_pcie->state != STATE_LINK_UP)
		return 0;

	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
			      SSC_LINK_DBG_2_LTSSM_STATE;
	if (val >= S_CFG_LINKWD_START && val < S_L2_IDLE)
		return 1;

	return 0;
}

static void __iomem *exynos_pcie_rc_other_conf_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	struct dw_pcie_rp *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	u32 busdev;

	if (!exynos_pcie_rc_check_link(pci))
		return NULL;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));

	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	return pp->va_cfg0_base + where;
}

static int exynos_rc_generic_other_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	struct dw_pcie_rp *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	ret = pci_generic_config_read(bus, devfn, where, size, val);

	if (exynos_pcie->ep_cfg->modify_read_val)
		exynos_pcie->ep_cfg->modify_read_val(exynos_pcie, where, val);

	return ret;
}

static int exynos_rc_generic_other_config_write(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 val)
{
	struct dw_pcie_rp *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	int ret;

	if (pci->dev->power.runtime_status != RPM_ACTIVE)
		return PCIBIOS_DEVICE_NOT_FOUND;

	ret = pci_generic_config_write(bus, devfn, where, size, val);

	return ret;
}

static struct pci_ops exynos_rc_own_pcie_ops = {
	.map_bus = exynos_pcie_rc_own_conf_map_bus,
	.read = exynos_rc_generic_own_config_read,
	.write = exynos_rc_generic_own_config_write,
};

static struct pci_ops exynos_rc_child_pcie_ops = {
	.map_bus = exynos_pcie_rc_other_conf_map_bus,
	.read = exynos_rc_generic_other_config_read,
	.write = exynos_rc_generic_other_config_write,
};

static u32 exynos_pcie_rc_read_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size)
{
	struct dw_pcie_rp *pp = &pci->pp;
	u32 val;

	exynos_pcie_rc_rd_own_conf(pp, reg, size, &val);

	return val;
}

static void exynos_pcie_rc_write_dbi(struct dw_pcie *pci, void __iomem *base,
				  u32 reg, size_t size, u32 val)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct dw_pcie_rp *pp = &pci->pp;

	if (reg == PCIE_PORT_MSI_CTRL_INT1_MASK && exynos_pcie->linkup_fail == 1) {
		pcie_err("Skip MSI mask config in link-up fail.\n");
		return;
	}

	exynos_pcie_rc_wr_own_conf(pp, reg, size, val);
}

static int exynos_pcie_rc_link_up(struct dw_pcie *pci)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (!exynos_pcie->probe_done) {
		pcie_info("Force link-up return in probe time!\n");
		return 1;
	}

	if (exynos_pcie->state != STATE_LINK_UP)
		return 0;

	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
			      SSC_LINK_DBG_2_LTSSM_STATE;
	if (val >= S_RCVRY_LOCK && val < S_L2_IDLE)
		return 1;

	return 0;
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.read_dbi = exynos_pcie_rc_read_dbi,
	.write_dbi = exynos_pcie_rc_write_dbi,
	.link_up = exynos_pcie_rc_link_up,
};

static void exynos_msi_enable(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	u32 val, mask_val;
	u32 *vec_msi_use;
	int  num_ctrls, ctrl;

	if (pp == NULL)
		return;

	vec_msi_use = (u32 *)pp->msi_irq_in_use;
	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	for (ctrl = 0; ctrl < num_ctrls; ctrl++) {
		val = vec_msi_use[ctrl];
		if (val)
			pcie_dbg("msi_irq_in_use : Vector[%d] : 0x%x\n",
				  ctrl, val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &mask_val);
		mask_val &= ~(val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, mask_val);
		if (val) {
			exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
					(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &val);
			pcie_dbg("%s: MSI Enable 0x%x, MSI Mask 0x%x\n", __func__,
				 val, mask_val);
		}
	}

	exynos_pcie_enable_sep_msi(exynos_pcie);
}

int exynos_msi_prepare_irq(struct irq_domain *domain, struct device *dev, int nvec,
			 msi_alloc_info_t *arg)
{
	struct irq_domain *irq_domain = dev->msi.domain->parent;
	struct irq_domain *msi_domain = dev->msi.domain;
	struct dw_pcie_rp *pp = irq_domain->host_data;
	struct msi_domain_info *msi_info = msi_domain->host_data;

	if (pp == NULL)
		return -EINVAL;

	msi_info->chip->irq_set_affinity = exynos_pcie_msi_set_affinity;
	msi_info->chip->irq_set_wake = exynos_pcie_msi_set_wake;
	msi_info->chip->irq_enable = exynos_pcie_msi_irq_enable;
	msi_info->chip->irq_disable = exynos_pcie_msi_irq_disable;

	return 0;
}

static void exynos_pcie_rc_set_iocc(struct exynos_pcie *exynos_pcie, int enable)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	int val;

	if (!exynos_pcie->sysreg_sharability) {
		pcie_err("%s: sysreg_shareablility is NOT defined!\n", __func__);
		return;
	}

	if (enable) {
		pcie_dbg("Enable cache coherency(IOCC).\n");

		/* set PCIe Axcache[1] = 1 */
		exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, 0x10101010);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, &val);

		/* set PCIe Shareability */
		regmap_update_bits(exynos_pcie->sysreg,
				exynos_pcie->sysreg_sharability,
				PCIE_SYSREG_HSI1_SHARABLE_MASK << 0,
				PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 0);
	} else {
		pcie_dbg("Disable cache coherency(IOCC).\n");
		/* clear PCIe Axcache[1] = 1 */
		exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, 0x0);

		/* clear PCIe Shareability */
		regmap_update_bits(exynos_pcie->sysreg,
				exynos_pcie->sysreg_sharability,
				PCIE_SYSREG_HSI1_SHARABLE_MASK << 0,
				PCIE_SYSREG_HSI1_SHARABLE_DISABLE << 0);
	}

	/* Check configurations for IOCC.
	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, &val);
	pcie_info("%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
	regmap_read(exynos_pcie->sysreg, exynos_pcie->sysreg_sharability, &val);
	pcie_info("%s: PCIe Shareability = 0x%x.\n", __func__, val);
	*/
}

static int exynos_pcie_rc_parse_dt(struct exynos_pcie *exynos_pcie, struct device *dev)
{
	struct device_node *np = dev->of_node;
	const char *use_sysmmu;
	const char *use_ia;
	const char *use_pcieon_sleep;

	if (of_property_read_u32(np, "ip-ver",
					&exynos_pcie->ip_ver)) {
		pcie_err("Failed to parse the number of ip-ver\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "pmu-phy-isolation",
				&exynos_pcie->pmu_phy_isolation)) {
		pcie_err("Failed to parse PMU PHY iosolation offset\n");
	}

	if (of_property_read_u32(np, "pmu-gpio-retention",
				&exynos_pcie->pmu_gpio_retention)) {
		pcie_err("Failed to parse PMU GPIO retention offset\n");
	}

	if (of_property_read_u32(np, "sysreg-sharability",
				&exynos_pcie->sysreg_sharability)) {
		pcie_err("Failed to parse SYSREG sharability offset\n");
	}

	if (of_property_read_u32(np, "sysreg-ia0-channel-sel",
				&exynos_pcie->sysreg_ia0_sel)) {
		pcie_err("Failed to parse I/A0 CH sel offset\n");
	}

	if (of_property_read_u32(np, "sysreg-ia1-channel-sel",
				&exynos_pcie->sysreg_ia1_sel)) {
		pcie_err("Failed to parse I/A1 CH sel offset\n");
	}

	if (of_property_read_u32(np, "sysreg-ia2-channel-sel",
				&exynos_pcie->sysreg_ia2_sel)) {
		pcie_err("Failed to parse I/A2 CH sel offset\n");
	}

	if (of_property_read_u32(np, "ep-device-type",
				&exynos_pcie->ep_device_type)) {
		pcie_err("EP device type is NOT defined, device type is 'EP_NO_DEVICE(0)'\n");
		exynos_pcie->ep_device_type = EP_TYPE_NO_DEVICE;
	}

	if (of_property_read_u32(np, "max-link-speed",
				&exynos_pcie->max_link_speed)) {
		pcie_err("MAX Link Speed is NOT defined...(GEN1)\n");
		/* Default Link Speet is GEN1 */
		exynos_pcie->max_link_speed = LINK_SPEED_GEN1;
	}

	if (!of_property_read_string(np, "use-pcieon-sleep", &use_pcieon_sleep)) {
		if (!strcmp(use_pcieon_sleep, "true")) {
			pcie_info("## PCIe ON Sleep is ENABLED.\n");
			exynos_pcie->use_pcieon_sleep = true;
		} else {
			pcie_info("## PCIe ON Sleep is DISABLED.\n");
			exynos_pcie->use_pcieon_sleep = false;
		}
	}

	if (!of_property_read_string(np, "use-sysmmu", &use_sysmmu)) {
		if (!strcmp(use_sysmmu, "true")) {
			pcie_info("## PCIe SysMMU is ENABLED.\n");
			exynos_pcie->use_sysmmu = true;
		} else {
			pcie_info("## PCIe SysMMU is DISABLED.\n");
			exynos_pcie->use_sysmmu = false;
		}
	}

	if (!of_property_read_string(np, "use-ia", &use_ia)) {
		if (!strcmp(use_ia, "true")) {
			pcie_info("## PCIe I/A is ENABLED.\n");
			exynos_pcie->use_ia = true;
		} else {
			pcie_info("## PCIe I/A is DISABLED.\n");
			exynos_pcie->use_ia = false;
		}
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if (of_property_read_u32(np, "pcie-pm-qos-int", &exynos_pcie->int_min_lock))
		exynos_pcie->int_min_lock = 0;
	if (exynos_pcie->int_min_lock)
		exynos_pm_qos_add_request(&exynos_pcie_int_qos[exynos_pcie->ch_num],
					  PM_QOS_DEVICE_THROUGHPUT, 0);
	pcie_info("%s: pcie int_min_lock = %d\n",
			__func__, exynos_pcie->int_min_lock);
#endif

	exynos_pcie->sysreg = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	/* Check definitions to access SYSREG in DT*/
	if (IS_ERR(exynos_pcie->sysreg)) {
		pcie_err("SYSREG is not defined.\n");
		return PTR_ERR(exynos_pcie->sysreg);
	}

	/* SSD & WIFI power control */
	exynos_pcie->ep_power_gpio = of_get_named_gpio(np, "pcie,ep-power-gpio", 0);
	if (exynos_pcie->ep_power_gpio < 0) {
		pcie_err("wlan gpio is not defined -> don't use wifi through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->ep_power_gpio, 0);
	}

	return 0;
}

static int exynos_pcie_rc_get_pin_state(struct exynos_pcie *exynos_pcie,
					struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	exynos_pcie->perst_gpio = of_get_gpio(np, 0);
	if (exynos_pcie->perst_gpio < 0) {
		pcie_err("cannot get perst_gpio\n");
	} else {
		ret = devm_gpio_request_one(dev, exynos_pcie->perst_gpio,
					    GPIOF_OUT_INIT_LOW, dev_name(dev));
		if (ret)
			return -EINVAL;
	}
	/* Get pin state */
	exynos_pcie->pcie_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(exynos_pcie->pcie_pinctrl)) {
		pcie_err("Can't get pcie pinctrl!!!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_ACTIVE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "active");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_ACTIVE])) {
		pcie_err("Can't set pcie clkerq to output high!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_IDLE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "idle");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
		pcie_err("No idle pin state(but it's OK)!!\n");
	else
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_IDLE]);

	return 0;
}

static int exynos_pcie_rc_get_resource(struct exynos_pcie *exynos_pcie,
				       struct platform_device *pdev)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct resource *temp_rsc;
	int ret;

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ssc");
	exynos_pcie->ssc_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->ssc_base)) {
		ret = PTR_ERR(exynos_pcie->ssc_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctrl");
	exynos_pcie->ctrl_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->ctrl_base)) {
		ret = PTR_ERR(exynos_pcie->ctrl_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	exynos_pcie->rc_dbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->rc_dbi_base)) {
		ret = PTR_ERR(exynos_pcie->rc_dbi_base);
		return ret;
	}

	pci->dbi_base = exynos_pcie->rc_dbi_base;

	if (exynos_pcie->use_ia) {
		temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ia");
		exynos_pcie->ia_base =
				devm_ioremap_resource(&pdev->dev, temp_rsc);
		if (IS_ERR(exynos_pcie->ia_base)) {
			ret = PTR_ERR(exynos_pcie->ia_base);
			return ret;
		}
	}

	return 0;
}

static void exynos_pcie_rc_enable_interrupts(struct exynos_pcie *exynos_pcie, int enable)
{
	u32 val;

	pcie_dbg("## %s PCIe INTERRUPT ##\n", enable ? "ENABLE" : "DISABLE");

	if (enable) {
		/* Interrupt clear before enable */
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_ERR_INT_STS);
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_ERR_INT_STS);
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_INT_STSCTL);
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_INT_STSCTL);

		/* Enable INTX interrupt */
		val = SSC_INT_CTL_INTA | SSC_INT_CTL_INTB | SSC_INT_CTL_INTC |
			SSC_INT_CTL_INTD;
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_INT_STSCTL);

		/* Enable ERR interrupt - only LINKDOWN and CPL timeout */
		val = SSC_ERR_CTL_LINK_DOWN_EVENT_EN |
			SSC_ERR_CTL_RADM_CPL_TIMEOUT_EN;
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_ERR_INT_CTL);

		if (exynos_pcie->debug_irq) {
			val = exynos_ctrl_read(exynos_pcie,
					       PCIE_CTRL_DEBUG_INTERRUPT_EN);
			val |= (0x1);
			exynos_ctrl_write(exynos_pcie, val,
					  PCIE_CTRL_DEBUG_INTERRUPT_EN);
		}

	} else {
		exynos_ssc_write(exynos_pcie, 0, PCIE_SSC_ERR_INT_STS);
		exynos_ssc_write(exynos_pcie, 0, PCIE_SSC_INT_STSCTL);
	}

}

static void exynos_pcie_notify_callback(struct exynos_pcie *exynos_pcie, int event)
{
	u32 id;

	if (event == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (event == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		exynos_pcie_dbg_print_link_history(exynos_pcie);
		id = 1;
	} else {
		pcie_err("PCIe: unknown event!!!\n");
		return;
	}

	pcie_err("[%s] event = 0x%x, id = %d\n", __func__, event, id);

	if (exynos_pcie->rc_event_reg[id] && exynos_pcie->rc_event_reg[id]->callback &&
			(exynos_pcie->rc_event_reg[id]->events & event)) {
		struct exynos_pcie_notify *notify =
			&exynos_pcie->rc_event_reg[id]->notify;
		notify->event = event;
		notify->user = exynos_pcie->rc_event_reg[id]->user;
		pcie_info("Callback for the event : %d\n", event);
		exynos_pcie->rc_event_reg[id]->callback(notify);
		return;
	} else {
		pcie_info("Client driver does not have registration "
				"of the event : %d\n", event);

		if (exynos_pcie->force_recover_linkdn) {
			pcie_info("Force PCIe poweroff --> poweron\n");
			exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
			exynos_pcie_rc_poweron(exynos_pcie->ch_num);
		}
	}
}

void exynos_pcie_rc_print_msi_register(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return;
	}

	exynos_pcie_dbg_print_msi_register(exynos_pcie);
}
EXPORT_SYMBOL(exynos_pcie_rc_print_msi_register);

void exynos_pcie_rc_register_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return;
	}

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info("SKIP register dump(Link-down state!)\n");
	} else {
		exynos_pcie_dbg_print_link_history(exynos_pcie);
		exynos_pcie_dbg_register_dump(exynos_pcie);
	}
}
EXPORT_SYMBOL(exynos_pcie_rc_register_dump);

void exynos_pcie_rc_all_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct device *dev;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return;
	}
	pci = exynos_pcie->pci;
	dev = pci->dev;

	pm_runtime_get_sync(dev);
	exynos_pcie_dbg_print_link_history(exynos_pcie);
	exynos_pcie_dbg_dump_link_down_status(exynos_pcie);
	if (exynos_pcie->state == STATE_LINK_DOWN)
		pcie_info("SKIP register dump(Link-down state!)\n");
	else
		exynos_pcie_dbg_register_dump(exynos_pcie);
	pm_runtime_put_sync(dev);
}
EXPORT_SYMBOL(exynos_pcie_rc_all_dump);

static void exynos_pcie_rc_dislink_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, dislink_work.work);

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	exynos_pcie_rc_all_dump(exynos_pcie->ch_num);
	exynos_pcie->pcie_is_linkup = 0;
	exynos_pcie->linkdown_cnt++;
	pcie_info("link down and recovery cnt: %d\n",
			exynos_pcie->linkdown_cnt);

	if (!exynos_pcie->ep_cfg->linkdn_callback_direct)
		exynos_pcie_notify_callback(exynos_pcie, EXYNOS_PCIE_EVENT_LINKDOWN);
}

static void exynos_pcie_rc_cpl_timeout_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, cpl_timeout_work.work);

	pcie_err("[%s] +++\n", __func__);

	exynos_pcie_rc_all_dump(exynos_pcie->ch_num);

	if (!exynos_pcie->ep_cfg->linkdn_callback_direct) {
		pcie_info("[%s] call PCIE_CPL_TIMEOUT callback func.\n", __func__);
		exynos_pcie_notify_callback(exynos_pcie, EXYNOS_PCIE_EVENT_CPL_TIMEOUT);
	}

	pcie_err("[%s] ---\n", __func__);
}

/* DWPHY ROM Change */
static void exynos_pcie_rc_dwphy_rom_change(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	pcie_dbg("PCIe PHY ROM download sz(%d)\n", exynos_pcie->phy_rom_size);

	memcpy(exynos_pcie->phy_base + DWPHY_SRAM_OFFSET, exynos_pcie->phy_rom,
	       exynos_pcie->phy_rom_size);

	exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_ROM_CHANGE);

	/* Set external load done */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_PHY_SRAM_CTRL);
	val |= SSC_PHY_SRAM_EXT_LD_DONE;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_PHY_SRAM_CTRL);

	exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_ADDITIONAL_PHY_SET);

	/* Clear WARM reset */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_RST_CTRL);
	val &= ~SSC_RST_CTRL_WARM_RESET;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_RST_CTRL);
}

static void exynos_pcie_rc_assert_phy_reset(struct exynos_pcie *exynos_pcie)
{
	exynos_pcie_cold_reset(exynos_pcie);

	if (exynos_pcie->phy->phy_ops.phy_config != NULL)
		exynos_pcie->phy->phy_ops.phy_config(exynos_pcie, exynos_pcie->ch_num);

	if (exynos_pcie->phy_rom_change) {
		exynos_pcie_rc_dwphy_rom_change(exynos_pcie);
	}

	/* Configuration for IA */
	if (exynos_pcie->use_ia && exynos_pcie->ep_cfg->set_ia_code)
		exynos_pcie->ep_cfg->set_ia_code(exynos_pcie);
}

static void exynos_pcie_rc_phy_pwrdn(struct exynos_pcie *exynos_pcie)
{
	/* phy all power down */
	if (exynos_pcie->phy->phy_ops.phy_all_pwrdn != NULL) {
		exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie,
				exynos_pcie->ch_num);
	}
}

static void exynos_pcie_rc_resumed_phydown(struct exynos_pcie *exynos_pcie)
{
	/* phy all power down */
	if (exynos_pcie->phy->phy_ops.phy_all_pwrdn != NULL)
		exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
}

static void exynos_pcie_setup_rc(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	u32 pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
	u32 val;

	/* Set Device Type to RC */
	val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_SS_RW_9);
	val &= ~(CTRL_SS_RW_9_DEV_TYPE_MASK);
	val |= CTRL_SS_RW_9_DEV_TYPE_RC;
	exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_SS_RW_9);

	/* enable writing to DBI read-only registers */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MISC_CONTROL_1_OFF, 4, &val);
	val |= PCIE_DBI_RO_WR_EN;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MISC_CONTROL_1_OFF, 4, val);

	/* change vendor ID and device ID for PCIe */
	exynos_pcie_rc_wr_own_conf(pp, PCI_VENDOR_ID, 2, PCI_VENDOR_ID_SAMSUNG);
	exynos_pcie_rc_wr_own_conf(pp, PCI_DEVICE_ID, 2,
				   PCI_DEVICE_ID_EXYNOS + exynos_pcie->ch_num);

	/* set auxiliary clock frequency: 76(0x4C)MHz */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_AUX_CLK_FREQ, 4, &val);
	val &= ~(PORT_AUX_CLK_FREQ_MASK);
	val |= PORT_AUX_CLK_FREQ_76MHZ;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_AUX_CLK_FREQ, 4, val);

	/* set completion timeout */
	/*
	Range/ Encoding/ Spec Minimum/ Spec Maximum/ PCIe controller Minimum/ PCIe controller Maximum
		Default 0000b 50 us 50 ms 28 ms 44 ms (M-PCIe: 45)
		A 0001b 50 us 100 us 65 us 99 us (M-PCIe: 100)
		A 0010b 1 ms 10 ms 4.1 ms 6.2 ms (M-PCIe: 6.4)
		B 0101b 16 ms 55 ms 28 ms 44 ms (M-PCIe: 45)
		B 0110b 65 ms 210 ms 86 ms 131 ms (M-PCIe: 133)
		C 1001b 260 ms 900 ms 260 ms 390 ms (M-PCIe: 398)
		C 1010b 1s 3.5 s 1.8 s 2.8 s (M-PCIe: 2.8)
		D 1101b 4s 13 s 5.4 s 8.2 s (M-PCIe: 8.4)
		D 1110b 17s 64 s 38 s 58 s (M-PCIe: 59) */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
	pcie_dbg("%s: before device_ctrl_status(0x98) = 0x%x\n", __func__, val);
	val &= ~(PCI_EXP_DEVCTL2_COMP_TIMEOUT);
	val |= PCI_EXP_DEVCTL2_COMP_TOUT_6_2MS;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
	pcie_dbg("%s: after device_ctrl_status(0x98) = 0x%x\n", __func__, val);

	/* RC Payload configuration to MAX value */
	if (exynos_pcie->pci_dev) {
		exynos_pcie_rc_rd_own_conf(pp,
				pcie_cap_off + PCI_EXP_DEVCAP, 4, &val);

		val &= PCI_EXP_DEVCAP_PAYLOAD;
		pcie_dbg("%s: Set supported payload size : %dbyte\n",
			  __func__, 128 << val);
		pcie_set_mps(exynos_pcie->pci_dev, 128 << val);
	}

	exynos_pcie_rc_rd_own_conf(pp, GEN3_RELATED_OFF, 4, &val);
	val &= ~(GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL);
	if (!exynos_pcie->ep_cfg->enable_eq)
		val |= GEN3_RELATED_OFF_GEN3_EQ_DISABLE; /* Disable EQ */
	exynos_pcie_rc_wr_own_conf(pp, GEN3_RELATED_OFF, 4, val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_FORCE_OFF, 4, &val);
	val |= PORT_FORCE_PART_LANE_RXEI;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_FORCE_OFF, 4, val);

	exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_SETUP_RC);

	if (exynos_pcie->phy->dbg_ops != NULL)
		exynos_pcie->phy->dbg_ops(exynos_pcie, PWRON);

	/* disable writing to DBI read-only registers */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MISC_CONTROL_1_OFF, 4, &val);
	val &= ~PCIE_DBI_RO_WR_EN;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MISC_CONTROL_1_OFF, 4, val);
}

static int exynos_pcie_rc_init(struct dw_pcie_rp *pp)
{
	/* Set default bus ops */
	pp->bridge->ops = &exynos_rc_own_pcie_ops;
	pp->bridge->child_ops = &exynos_rc_child_pcie_ops;

	/* Setup RC to avoid initialization faile in PCIe stack */
	dw_pcie_setup_rc(pp);
	return 0;
}

static struct dw_pcie_host_ops exynos_pcie_rc_ops = {
	.host_init = exynos_pcie_rc_init,
};

static irqreturn_t exynos_pcie_rc_dbg_irq_handler(int irq, void *arg)
{
	struct dw_pcie_rp *pp = arg;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 pending_val;

	pending_val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DEBUG_INT_CLR);
	printk_ratelimited("PCIe PLL is recovered - (%#x)\n", pending_val);

	/* Debug interrupt clear */
	exynos_ctrl_write(exynos_pcie, 0x1, PCIE_CTRL_DEBUG_INT_CLR);
	exynos_ctrl_write(exynos_pcie, 0x0, PCIE_CTRL_DEBUG_INT_CLR);

	return IRQ_HANDLED;
}
static irqreturn_t exynos_pcie_rc_irq_handler(int irq, void *arg)
{
	struct dw_pcie_rp *pp = arg;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val_irq_err, val_irq_sts, val, i;

	if (exynos_pcie->state == STATE_LINK_DOWN ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		pcie_info("SKIP irq_handler - (It's power-down state)\n");
		disable_irq_nosync(pp->irq);

		return IRQ_HANDLED;
	}

	/* Check flush of slave requests */
	for (i = 0; i < 10; i++) {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2);
		val &= SSC_LINK_DBG_2_SLV_XFER_PEND;
		if (val == 0x0)
			break;
	}

	if (val != 0x0) {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2);
		pcie_err("Check fail of SLV_XFER_PEND - %#x\n", val);
	}

	/* handle error interrupt */
	val_irq_err = exynos_ssc_read(exynos_pcie, PCIE_SSC_ERR_INT_STS);
	exynos_ssc_write(exynos_pcie, val_irq_err, PCIE_SSC_ERR_INT_STS);

	/* handle error interrupt */
	val_irq_sts = exynos_ssc_read(exynos_pcie, PCIE_SSC_INT_STSCTL);
	exynos_ssc_write(exynos_pcie, val_irq_sts, PCIE_SSC_INT_STSCTL);

	if (val_irq_err & SSC_ERR_STS_LINK_DOWN_EVENT) {
		pcie_info("!!! PCIE_LINK_DOWN !!!\n");
		pcie_info("!!! irq_err = 0x%x, irq_sts = 0x%x\n", val_irq_err,
			  val_irq_sts);

		if (exynos_pcie->cpl_timeout_recovery) {
			pcie_info("!!!now is already cto recovering..\n");
		} else {
			exynos_pcie->sudden_linkdown = 1;
			exynos_pcie->state = STATE_LINK_DOWN_TRY;

			if (exynos_pcie->ep_cfg->linkdn_callback_direct) {
				exynos_pcie->linkdown_cnt++;
				pcie_info("link down and recovery cnt: %d\n",
						exynos_pcie->linkdown_cnt);

				exynos_pcie_notify_callback(exynos_pcie,
						EXYNOS_PCIE_EVENT_LINKDOWN);
			}
			queue_work(exynos_pcie->pcie_wq,
					&exynos_pcie->dislink_work.work);
		}
	}

	if (val_irq_err & SSC_ERR_STS_RADM_CPL_TIMEOUT) {
		pcie_info("!!! PCIE_CPL_TIMEOUT !!!\n");
		pcie_info("!!! irq_err = 0x%x, irq_sts = 0x%x\n", val_irq_err,
			  val_irq_sts);

		if (exynos_pcie->sudden_linkdown) {
			pcie_info("!!!now is already link down recovering..\n");
		} else {
			if (exynos_pcie->cpl_timeout_recovery == 0) {
				exynos_pcie->state = STATE_LINK_DOWN_TRY;
				exynos_pcie->cpl_timeout_recovery = 1;

				if (exynos_pcie->ep_cfg->linkdn_callback_direct)
					exynos_pcie_notify_callback(exynos_pcie,
							EXYNOS_PCIE_EVENT_CPL_TIMEOUT);

				pcie_info("!!!call cpl_timeout work\n");
				queue_work(exynos_pcie->pcie_wq,
						&exynos_pcie->cpl_timeout_work.work);
			} else {
				pcie_info("!!!now is already cto recovering..\n");
			}
		}
	}

	return IRQ_HANDLED;
}

static int exynos_pcie_rc_msi_init_default(struct exynos_pcie *exynos_pcie)
{
	/* Do something for common configuration for MSI */

	exynos_msi_enable(exynos_pcie);

	return 0;
}

static inline void exynos_pcie_rc_modify_read_val_cp(struct exynos_pcie *exynos_pcie,
						     int where, u32 *val)
{
	struct pci_dev *pdev = exynos_pcie->ep_pci_dev;

	if (where == PCI_CLASS_REVISION && (*val >> 8) == 0x0) {
		dev_dbg(&pdev->dev, "Fake return as a Network device...\n");
		*val |= (PCI_CLASS_NETWORK_OTHER << 16);
	}

	if (pdev == NULL) /* It should be SKIP before EP scan */
		return;

	if (where == pdev->msi_cap + PCI_MSI_FLAGS &&
	    (*val & PCI_MSI_FLAGS_QMASK) == 0x0) {
		dev_dbg(&pdev->dev, "Fake return as supporting 16 MSI...\n");
		*val |= (0x4 << 1);
	}
}

static inline void exynos_pcie_rc_modify_read_val_samsung_wifi(struct exynos_pcie *exynos_pcie,
						     int where, u32 *val)
{
	struct pci_dev *pdev = exynos_pcie->ep_pci_dev;

	if (pdev == NULL) /* It should be SKIP before EP scan */
		return;

	if (where == PCI_CLASS_REVISION && (*val >> 8) == 0x0) {
		dev_info(&pdev->dev, "Fake return as a Network device...\n");
		*val |= (PCI_CLASS_NETWORK_OTHER << 16);
	}

	if (where == pdev->msi_cap + PCI_MSI_FLAGS &&
	    (*val & PCI_MSI_FLAGS_QMASK) == 0x0) {
		dev_info(&pdev->dev, "Fake return as supporting 32 MSI...\n");
		*val |= (0x5 << 1);
	}
}

static int exynos_pcie_rc_msi_init_cp(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	unsigned long msi_addr_from_dt;
#endif
	/*
	 * The following code is added to avoid duplicated allocation.
	 */
	if (!exynos_pcie->is_ep_scaned) {
		pcie_info("%s: allocate MSI data\n", __func__);
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
		/* get the MSI target address from DT */
		msi_addr_from_dt = shm_get_msi_base();
		if (msi_addr_from_dt) {
			pcie_info("%s: MSI target addr. from DT: 0x%lx\n",
					__func__, msi_addr_from_dt);
			pp->msi_data = msi_addr_from_dt;
		} else {
			pcie_err("%s: msi_addr_from_dt is null\n", __func__);
			return -EINVAL;
		}
#else
		pcie_info("EP device is Modem but, ModemIF isn't enabled\n");
#endif
	}

	pcie_info("%s: Program the MSI data: %lx (probe ok:%d)\n", __func__,
				(unsigned long int)pp->msi_data, exynos_pcie->is_ep_scaned);
	/* Program the msi_data */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
			lower_32_bits(pp->msi_data));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
			upper_32_bits(pp->msi_data));

	exynos_pcie_enable_sep_msi(exynos_pcie);

	return 0;
}

static void exynos_pcie_rc_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	int count = 0, retry_cnt = 0;
	u32 val;

	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
			      SSC_LINK_DBG_2_LTSSM_STATE;
	if (!(val >= S_RCVRY_LOCK && val <= S_L1_IDLE)) {
		pcie_info("%s, pcie link is not up\n", __func__);
		return;
	}

	/* Set s/w L1 exit mode to keep L0 */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_PM_CTRL);
	val |= SSC_PM_CTRL_EXIT_ASPM_L1;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_PM_CTRL);

retry_pme_turnoff:
	if (retry_cnt > 0) {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
				      SSC_LINK_DBG_2_LTSSM_STATE;
		pcie_err("Current LTSSM State is 0x%x with retry_cnt =%d.\n",
				val, retry_cnt);
	}
	/* Clear RX_MSG_STS */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_RX_MSG_STS);
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_RX_MSG_STS);

	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
			      SSC_LINK_DBG_2_LTSSM_STATE;

	/* Set PME_TURN_OFF request */
	pcie_dbg("%s: link state: %#x.\n", __func__, val);
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_TX_MSG_REQ);
	val |= SSC_TX_MSG_REQ_PME_TURN_OFF;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_TX_MSG_REQ);

	while (count < MAX_L2_TIMEOUT) {
		if ((exynos_ssc_read(exynos_pcie, PCIE_SSC_RX_MSG_STS)
					& SSC_RX_MSG_STS_PME_TO_ACK_STS)) {
			pcie_dbg("ACK message is ok\n");
			udelay(10);
			break;
		}

		udelay(10);
		count++;
	}
	if (count >= MAX_L2_TIMEOUT)
		pcie_err("cannot receive ack message from EP\n");

	count = 0;
	do {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
				      SSC_LINK_DBG_2_LTSSM_STATE;
		if (val == S_L2_IDLE) {
			pcie_dbg("received Enter_L23_READY DLLP packet.(LTSSM:%#x)\n",
				  val);
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_L2_TIMEOUT);

	if (count >= MAX_L2_TIMEOUT) {
		if (retry_cnt < 5) {
			retry_cnt++;

			goto retry_pme_turnoff;
		}
		pcie_err("cannot receive L23_READY DLLP packet(0x%x)\n", val);
	}
}

/* Control Q-channel */
static inline void exynos_pcie_rc_qch_control(struct exynos_pcie *exynos_pcie, int enable)
{
	if (enable)
		exynos_ctrl_write(exynos_pcie, CTRL_QCH_CTRL_ENABLE_VAL,
				  PCIE_CTRL_QCH_CTRL);
	else
		exynos_ctrl_write(exynos_pcie, CTRL_QCH_CTRL_DISABLE_VAL,
				  PCIE_CTRL_QCH_CTRL);
}

static inline void exynos_pcie_rc_history_ctrl_phy_essen(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	/* Set Samsung CP Debug Signal Mask */
	val = CTRL_DEBUG_SIG_MASK_LTSSM | CTRL_DEBUG_SIG_MASK_L1SUB |
		CTRL_DEBUG_SIG_MASK_PHY_DTB_PAT | CTRL_DEBUG_SIG_MASK_PHY_ESSEN |
		CTRL_DEBUG_SIG_MASK_LOCK_SIG | CTRL_DEBUG_SIG_MASK_PHY_FIXED |
		CTRL_DEBUG_SIG_MASK_PHY_DYN_3B;

	exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_SIGNAL_MASK);

	/* Configurations for PHY DTB signal */
	/* PHY DTB shows CLKREQ info in history buffer[15:14]
	 * 15 : RC clkreq
	 * 14 : EP clkreq
	 */
	val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DTB_SEL);
	val |= CTRL_DTB_SEL_CLKREQ;
	exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DTB_SEL);

	/* Configurations for PHY essential siganl */
	/* debug_mode -> Clear debug_mode bit [11:8] */
	val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DEBUG_MODE);
	val &= ~CTRL_DEBUG_MODE_CP_TRACE;
	exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_MODE);

	/* DBG_SEL_LANE -> lane0 fix : Clear CFG_DBG_SEL_LANE */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_3);
	val &= ~SSC_GEN_CTRL_3_SEL_LANE_MASK;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_3);

	/* DBG_SEL_GROUP selection : Clear CFG_DBG_SEL_GROUP */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_3);
	val &= ~SSC_GEN_CTRL_3_SEL_GROUP_MASK;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_3);

	/* DBG_SIGNAL Selection */
	exynos_ssc_write(exynos_pcie, 0x0, PCIE_SSC_DBG_SIGANL_1);
	val = 0x0;
	/*
	 * 0x21 : smlh_ts2_rcvd
	 * 0x22 : smlh_ts1_rcvd
	 * 0x78 : mac_phy_rate[0] (00 : GEN1, 01 : GEN2)
	 * 0x79 : mac_phy_rate[1] (10 : GEN3, 11 : GEN4)
	 * 0x26 : reserved(don't use)
	 */
	val |= (0x21 << SSC_DBG_SIG1_0) | (0x22 << SSC_DBG_SIG1_1) |
		(0x78 << SSC_DBG_SIG1_2) | (0x79 << SSC_DBG_SIG1_3);
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_DBG_SIGANL_1);

	pcie_info("DBG_SIGNAL Selection : %#x\n",
		  exynos_ssc_read(exynos_pcie, PCIE_SSC_DBG_SIGANL_1));
}

/* Control History Buffer */
static inline void exynos_pcie_rc_history_buff_control(struct exynos_pcie *exynos_pcie,
						int enable)
{
	u32 val;

	if (enable) {
		/* Disable hsi_debug_en */
		val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DEBUG_MODE);
		val &= ~CTRL_DEBUG_MODE_DEBUG_EN;
		exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_MODE);

		if (exynos_pcie->ep_cfg->history_dbg_ctrl) {
			pcie_dbg("Set specific history buffer config.\n");
			exynos_pcie->ep_cfg->history_dbg_ctrl(exynos_pcie);

		} else {
			/* Set DEFAULT Debug Signal Mask */
			val = CTRL_DEBUG_SIG_MASK_LTSSM | CTRL_DEBUG_SIG_MASK_L1SUB |
				CTRL_DEBUG_SIG_MASK_LOCK_SIG;
			exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_SIGNAL_MASK);
		}

		/* Set debug_store_start */
		val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DEBUG_MODE);
		val &= ~(CTRL_DEBUG_MODE_RX_STAT);
		val |= CTRL_DEBUG_MODE_STORE_START | CTRL_DEBUG_MODE_STM_MODE_EN |
			CTRL_DEBUG_MODE_BUFFER_MODE_EN | CTRL_DEBUG_MODE_RX_STAT_VAL;
		exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_MODE);

		/* Enable hsi_debug_en */
		val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DEBUG_MODE);
		val |= CTRL_DEBUG_MODE_DEBUG_EN;
		exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_MODE);
	} else {
		/* Disable hsi_debug_en */
		val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_DEBUG_MODE);
		val &= ~CTRL_DEBUG_MODE_DEBUG_EN;
		exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_DEBUG_MODE);
	}
}

/* EP PCIe reset for retrying link-up */
static inline void exynos_pcie_rc_reset_ep(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	gpio_set_value(exynos_pcie->perst_gpio, 0);
	pcie_info("%s: Set PERST to LOW, gpio val = %d\n", __func__,
		  gpio_get_value(exynos_pcie->perst_gpio));

	/* LTSSM disable */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_3);
	val &= ~SSC_GEN_CTRL_3_LTSSM_EN;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_3);

}

static int exynos_pcie_rc_establish_link(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	u32 val, busdev, current_speed, current_lane;
	int count = 0, try_cnt = 0, max_try_cnt = 10;
	unsigned long udelay_before_perst, udelay_after_perst;

	udelay_before_perst = exynos_pcie->ep_cfg->udelay_before_perst;
	udelay_after_perst = exynos_pcie->ep_cfg->udelay_after_perst;
	if (exynos_pcie->ep_cfg->linkup_max_count)
		max_try_cnt = exynos_pcie->ep_cfg->linkup_max_count;

retry:
	exynos_pcie_rc_assert_phy_reset(exynos_pcie);

	/* Q-ch disable */
	exynos_pcie_rc_qch_control(exynos_pcie, 0);

	if (try_cnt && udelay_before_perst)
		usleep_range(udelay_before_perst, udelay_before_perst + 2000);

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);

	pcie_info("%s: Set PERST to HIGH, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

	if (udelay_after_perst)
		usleep_range(udelay_after_perst, udelay_after_perst + 2000);
	else /* Use default delay 18~20msec */
		usleep_range(18000, 20000);

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	exynos_pcie_setup_rc(exynos_pcie);

	if (dev_is_dma_coherent(pci->dev))
		exynos_pcie_rc_set_iocc(exynos_pcie, 1);

	/* Set LTSSM-Enable clear at sudden Link-down */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_4);
	val |= SSC_GEN_CTRL_4_LTSSM_EN_CLR;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_4);

	pcie_dbg("Add code to recognize PERST high!\n");
	val = exynos_ctrl_read(exynos_pcie, PCIE_CTRL_LINK_CTRL);
	val |= CTRL_LINK_CTRL_PERSTN_IN_SEL;
	exynos_ctrl_write(exynos_pcie, val, PCIE_CTRL_LINK_CTRL);

	pcie_dbg("Disable APB timeout!\n");
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_APB_CLKREQ_TOUT);
	val |= SSC_CFG_APB_TIMER_DIS;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_APB_CLKREQ_TOUT);

	/* Enable history buffer */
	exynos_pcie_rc_history_buff_control(exynos_pcie, 1);

	/* Link-up Start - LTSSM enable */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_3);
	val |= SSC_GEN_CTRL_3_LTSSM_EN;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_3);
	count = 0;

	while (count < MAX_LINK_UP_TIMEOUT) {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
				      SSC_LINK_DBG_2_LTSSM_STATE;
		if (val == S_L0)
			break;

		count++;

		udelay(10);
	}

	/* Check LTSSM state */
	if (count >= MAX_LINK_UP_TIMEOUT) {
		try_cnt++;

		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
				      SSC_LINK_DBG_2_LTSSM_STATE;
		pcie_err("%s: Link is not up, try count: %d, linksts: %s(0x%x)\n",
			__func__, try_cnt, LINK_STATE_DISP(val), val);

		if (try_cnt < max_try_cnt) {
			exynos_pcie_rc_reset_ep(exynos_pcie);
			goto retry;
		} else {
			exynos_pcie_dbg_print_link_history(exynos_pcie);
			exynos_pcie_dbg_register_dump(exynos_pcie);

			return -EPIPE;
		}
	}

recheck_speed:
	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL, 4, &current_speed);
	current_lane = (current_speed >> 20) & PCI_EXP_LNKSTA_NEGO_LANE;
	current_speed = (current_speed >> 16) & PCI_EXP_LNKSTA_CLS;
	pcie_dbg("Check Link Speed : GEN%d %dlane (Max GEN%d)\n",
		 current_speed, current_lane, exynos_pcie->max_link_speed);

	/* Check LTSSM state again to make sure L0. */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
			      SSC_LINK_DBG_2_LTSSM_STATE;

	/* check link training result(speed) */
	if (current_speed < exynos_pcie->max_link_speed || val != S_L0) {
		if (exynos_pcie->ep_cfg->no_speed_check == 1 && val == S_L0)
			goto skip_speed_check;

		if (count < MAX_LINK_UP_TIMEOUT) { /* Wait until MAX link-up time */
			count++;
			udelay(10);
			goto recheck_speed;
		}

		pcie_err("%s: Link is up(%#x). But not GEN%d speed, try count: %d\n",
			 __func__, val, exynos_pcie->max_link_speed, try_cnt);
		try_cnt++;

		if (try_cnt < max_try_cnt) {
			exynos_pcie_rc_reset_ep(exynos_pcie);
			goto retry;
		} else {
			pcie_err("WARN : NEED TO CHECK CURRENT SPEED GEN%d (MAX GEN%d)\n",
				  current_speed, exynos_pcie->max_link_speed);
		}
	}

skip_speed_check:

	exynos_pcie->linkup_fail = 0;
	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL, 4, &current_speed);
	current_speed = (current_speed >> 16) & PCI_EXP_LNKSTA_CLS;
	pcie_info("Current Link Speed is GEN%d %dlane (MAX GEN%d)\n",
		  current_speed, current_lane, exynos_pcie->max_link_speed);

	/* Q-ch enable */
	exynos_pcie_rc_qch_control(exynos_pcie, 1);

	exynos_pcie_rc_enable_interrupts(exynos_pcie, 1);

	/* setup ATU for cfg/mem outbound */
	busdev = PCIE_ATU_BUS(1) | PCIE_ATU_DEV(0) | PCIE_ATU_FUNC(0);
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
	exynos_pcie_rc_prog_viewport_mem_outbound(exynos_pcie);

	return 0;
}

static void exynos_pcie_check_payload(struct exynos_pcie *exynos_pcie)
{
	int rc_mps, ep_mps, available_mps, rc_ret, ep_ret;

	if (exynos_pcie->pci_dev == NULL || exynos_pcie->ep_pci_dev == NULL)
		return;

	rc_mps = pcie_get_mps(exynos_pcie->pci_dev);
	ep_mps = pcie_get_mps(exynos_pcie->ep_pci_dev);

	if (rc_mps != ep_mps) {
		pcie_err("MPS(Max Payload Size) is NOT equal.\n");
		rc_mps = exynos_pcie->pci_dev->pcie_mpss;
		ep_mps = exynos_pcie->ep_pci_dev->pcie_mpss;

		available_mps = rc_mps > ep_mps ? ep_mps : rc_mps;

		rc_ret = pcie_set_mps(exynos_pcie->pci_dev, 128 << available_mps);
		ep_ret = pcie_set_mps(exynos_pcie->ep_pci_dev, 128 << available_mps);

		if (rc_ret == -EINVAL || ep_ret == -EINVAL) {
			pcie_err("Can't set payload value.(%d)\n",
				 available_mps);

			/* Set to default value : 128byte */
			pcie_set_mps(exynos_pcie->pci_dev, 128);
			pcie_set_mps(exynos_pcie->ep_pci_dev, 128);
		}
	}
}

static void exynos_pcie_cold_reset(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	spin_lock(&exynos_pcie->reg_lock);
	/* force S/W Cold Reset */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_RST_CTRL);
	val |= SSC_RST_CTRL_COLD_RESET;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_RST_CTRL);
	udelay(15);
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_RST_CTRL);
	val &= ~SSC_RST_CTRL_COLD_RESET;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_RST_CTRL);
	spin_unlock(&exynos_pcie->reg_lock);
}

void exynos_pcie_warm_reset(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	spin_lock(&exynos_pcie->reg_lock);
	/* force S/W warm Reset */
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_RST_CTRL);
	val |= SSC_RST_CTRL_WARM_RESET | SSC_RST_CTRL_PHY_RESET;
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_RST_CTRL);
	udelay(15);
	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_RST_CTRL);
	val &= ~(SSC_RST_CTRL_WARM_RESET | SSC_RST_CTRL_PHY_RESET);
	exynos_ssc_write(exynos_pcie, val, PCIE_SSC_RST_CTRL);
	spin_unlock(&exynos_pcie->reg_lock);
}

int exynos_pcie_rc_poweron(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	struct device *dev;
	int ret;
	struct irq_desc *exynos_pcie_desc;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -EPROBE_DEFER;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;
	dev = pci->dev;
	exynos_pcie_desc = irq_to_desc(pp->irq);

	pcie_info("%s, start of poweron, pcie state: %d\n", __func__,
							 exynos_pcie->state);
	if (!exynos_pcie->is_ep_scaned) {
		ret = exynos_pcie_rc_phy_load(exynos_pcie, PHYCAL_BIN);
		if (ret)
			pcie_info("%s, PHYCAL binary load isn't supported\n", __func__);

		ret = exynos_pcie_rc_phy_load(exynos_pcie, DBG_BIN);
		if (ret)
			pcie_info("%s, DBG binary load isn't supported\n", __func__);
	}

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pm_runtime_get_sync(dev);

		exynos_pcie->pcie_is_linkup = 1;

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
		pcie_dbg("%s, ip idle status : %d, idle_ip_index: %d\n",
				__func__, PCIE_IS_ACTIVE, exynos_pcie->idle_ip_index);
		exynos_update_ip_idle_status(
				exynos_pcie->idle_ip_index,
				PCIE_IS_ACTIVE);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num],
					exynos_pcie->int_min_lock);
			pcie_dbg("%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif

		/* Enable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_enable(ch_num);

		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_ACTIVE]);

		/* phy all power down clear */
		if (exynos_pcie->phy->phy_ops.phy_all_pwrdn_clear != NULL)
			exynos_pcie->phy->phy_ops.phy_all_pwrdn_clear(exynos_pcie,
								exynos_pcie->ch_num);

		exynos_pcie->state = STATE_LINK_UP_TRY;

		if ((exynos_pcie_desc) && (exynos_pcie_desc->depth > 0))
			enable_irq(pp->irq);
		else
			pcie_dbg("%s, already enable_irq, so skip\n", __func__);

		exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_POWER_ON);

		if (exynos_pcie_rc_establish_link(exynos_pcie)) {
			pcie_err("pcie link up fail\n");
			goto poweron_fail;
		}

		exynos_pcie->state = STATE_LINK_UP;

		exynos_pcie->sudden_linkdown = 0;

		ret = exynos_pcie->ep_cfg->msi_init(exynos_pcie);
		if (ret) {
			pcie_err("%s: failed to MSI initialization(%d)\n",
					__func__, ret);
			return ret;
		}

		pcie_dbg("[%s] exynos_pcie->is_ep_scaned : %d\n", __func__,
			 exynos_pcie->is_ep_scaned);

		if (!exynos_pcie->is_ep_scaned) {
			/*
			 * To restore registers that initialized at probe time.
			 * (AER, PME capabilities)
			 */
			pcie_info("Restore saved state(%d)\n",
				  exynos_pcie->pci_dev->state_saved);
			pci_restore_state(exynos_pcie->pci_dev);

			if (exynos_pcie->phy->dbg_ops != NULL)
				exynos_pcie->phy->dbg_ops(exynos_pcie, RESTORE);

			pci_rescan_bus(exynos_pcie->pci_dev->bus);

			exynos_pcie->ep_pci_dev = exynos_pcie_get_pci_dev(exynos_pcie);
			if (exynos_pcie->ep_pci_dev != NULL) {
				exynos_pcie_mem_copy_epdev(exynos_pcie);

				exynos_pcie->ep_pci_dev->dma_mask = DMA_BIT_MASK(36);
				exynos_pcie->ep_pci_dev->dev.coherent_dma_mask =
							DMA_BIT_MASK(36);
				dma_set_seg_boundary(&(exynos_pcie->ep_pci_dev->dev),
						     DMA_BIT_MASK(36));

				if (exynos_pcie->ep_cfg->set_dma_ops) {
					exynos_pcie->ep_cfg->set_dma_ops(&exynos_pcie->ep_pci_dev->dev);
					pcie_dbg("DMA operations are changed to support SysMMU\n");
				}
			} else {
				pcie_err("Can't find PCIe EP DEV!!!\n");
			}

			/* To check before save state */
			if (exynos_pcie->phy->dbg_ops != NULL)
				exynos_pcie->phy->dbg_ops(exynos_pcie, SAVE);

			if (pci_save_state(exynos_pcie->pci_dev)) {
				pcie_err("Failed to save pcie state\n");
				goto poweron_fail;
			}
			exynos_pcie->pci_saved_configs =
				pci_store_saved_state(exynos_pcie->pci_dev);

			exynos_pcie->is_ep_scaned = 1;
		} else if (exynos_pcie->is_ep_scaned) {
			if (pci_load_saved_state(exynos_pcie->pci_dev,
					     exynos_pcie->pci_saved_configs)) {
				pcie_err("Failed to load pcie state\n");
				goto poweron_fail;
			}
			pci_restore_state(exynos_pcie->pci_dev);

			/* To check after restore state */
			if (exynos_pcie->phy->dbg_ops != NULL)
				exynos_pcie->phy->dbg_ops(exynos_pcie, RESTORE);
		}

		exynos_pcie->cpl_timeout_recovery = 0;
		exynos_pcie_check_payload(exynos_pcie);
	}

	pcie_dbg("%s, end of poweron, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	return 0;

poweron_fail:
	exynos_pcie->linkup_fail = 1; /* Set Link-up Fail value */
	exynos_pcie_rc_qch_control(exynos_pcie, 1);

	exynos_pcie->state = STATE_LINK_UP;
	exynos_pcie->cpl_timeout_recovery = 0;
	exynos_pcie_rc_poweroff(exynos_pcie->ch_num);

	return -EPIPE;
}
EXPORT_SYMBOL(exynos_pcie_rc_poweron);

void exynos_pcie_rc_poweroff(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	struct device *dev;
	u32 val;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;
	dev = pci->dev;

	pcie_info("%s, start of poweroff, pcie state: %d\n", __func__,
		  exynos_pcie->state);

	if (exynos_pcie->phy->dbg_ops != NULL)
		exynos_pcie->phy->dbg_ops(exynos_pcie,PWROFF);

	if (exynos_pcie->state == STATE_LINK_UP ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		disable_irq(pp->irq);

		spin_lock(&exynos_pcie->conf_lock);

		exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_POWER_OFF);

		exynos_pcie_rc_send_pme_turn_off(exynos_pcie);

		exynos_pcie->state = STATE_LINK_DOWN;

		if (exynos_pcie->ep_cfg->mdelay_before_perst_low) {
			pcie_dbg("%s, It needs delay after setting perst to low\n", __func__);
			mdelay(exynos_pcie->ep_cfg->mdelay_before_perst_low);
		}

		/* Disable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_disable(ch_num);

		if (exynos_pcie->use_ia && exynos_pcie->ep_cfg->set_ia_code) {
			//IA disable
			exynos_ia_write(exynos_pcie, 0x0, 0x60);
			exynos_ia_write(exynos_pcie, 0x0, 0x0);
			exynos_ia_write(exynos_pcie, 0xD0000004, 0x100);
			exynos_ia_write(exynos_pcie, 0xB0020188, 0x108);
		}

		/* Disable history buffer */
		exynos_pcie_rc_history_buff_control(exynos_pcie, 0);

		gpio_set_value(exynos_pcie->perst_gpio, 0);
		pcie_info("%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

		if (exynos_pcie->ep_cfg->mdelay_after_perst_low) {
			pcie_dbg("%s, It needs delay after setting perst to low\n", __func__);
			mdelay(exynos_pcie->ep_cfg->mdelay_after_perst_low);
		}

		/* LTSSM disable */
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_GEN_CTRL_3);
		val &= ~SSC_GEN_CTRL_3_LTSSM_EN;
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_GEN_CTRL_3);

		udelay(10);

		exynos_pcie_rc_phy_pwrdn(exynos_pcie);

		spin_unlock(&exynos_pcie->conf_lock);

		exynos_pcie->current_busdev = 0x0;

		if (!IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
			pinctrl_select_state(exynos_pcie->pcie_pinctrl,
					exynos_pcie->pin_state[PCIE_PIN_IDLE]);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num], 0);
			pcie_dbg("%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
		pcie_dbg("%s, ip idle status : %d, idle_ip_index: %d\n",
				__func__, PCIE_IS_IDLE,	exynos_pcie->idle_ip_index);
		exynos_update_ip_idle_status(
				exynos_pcie->idle_ip_index,
				PCIE_IS_IDLE);
#endif
		pm_runtime_put_sync(dev);
	}
	exynos_pcie->pcie_is_linkup = 0;

	pcie_dbg("%s, end of poweroff, pcie state: %d\n",  __func__,
			exynos_pcie->state);

	return;
}
EXPORT_SYMBOL(exynos_pcie_rc_poweroff);

/* Get EP pci_dev structure of BUS 1 */
static struct pci_dev *exynos_pcie_get_pci_dev(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	int domain_num;
	struct pci_dev *ep_pci_dev;
	u32 val;

	if (exynos_pcie->ep_pci_dev != NULL)
		return exynos_pcie->ep_pci_dev;

	/* Get EP vendor/device ID to get pci_dev structure */
	domain_num = exynos_pcie->pci_dev->bus->domain_nr;

	if (exynos_pcie->ep_pci_bus == NULL) {
		exynos_pcie->ep_pci_bus = pci_find_bus(domain_num, 1);
		if (exynos_pcie->ep_pci_bus == NULL) {
			pcie_err("%s : ep_pci_bus is NULL!!!\n", __func__);
			return NULL;
		}
	}

	exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);

	ep_pci_dev = pci_get_device(val & ID_MASK, (val >> 16) & ID_MASK, NULL);
	if (ep_pci_dev == NULL)
		return NULL;

	return ep_pci_dev;
}

static int exynos_pcie_l1ss_samsung(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	struct pci_dev *ep_dev;
	struct pci_bus *ep_bus;
	u32 val;

	ep_dev = exynos_pcie_get_pci_dev(exynos_pcie);
	ep_bus = exynos_pcie->ep_pci_bus;

	/* Set L1.2 Entrance Latency for samsung devices.
	 * L1ss entrance time is set to 64usec(max), but the Actual
	 * entrance time is 32usec because the external sync is disabled
	 * by default.
	 */
	exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, PCIE_PORT_AFR, 4, &val);
	val &= ~PORT_AFR_L1_ENTRANCE_LAT_MASK;
	val |= PORT_AFR_L1_ENTRANCE_LAT_64us;
	exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, PCIE_PORT_AFR, 4, val);

	return 0;
}

static int exynos_pcie_l1ss_bcm(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	struct pci_dev *ep_dev;
	struct pci_bus *ep_bus;
	u32 ep_ext_ltr_cap_off;

	/* Set max snoop latency for bcm WIFI. */
	ep_dev = exynos_pcie_get_pci_dev(exynos_pcie);
	ep_bus = exynos_pcie->ep_pci_bus;

	ep_ext_ltr_cap_off = pci_find_ext_capability(ep_dev, PCI_EXT_CAP_ID_LTR);
	if (!ep_ext_ltr_cap_off) {
		pcie_info("%s: cannot find LTR extended capability offset\n", __func__);
		return -EINVAL;
	}

	exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_ext_ltr_cap_off + PCI_LTR_MAX_SNOOP_LAT,
			       4, 0x10031003);

	return 0;
}

static int exynos_pcie_set_l1ss(struct exynos_pcie *exynos_pcie, int enable, int id)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	u32 val;
	struct pci_dev *ep_dev, *rc_dev;
	u32 tpoweron_time = exynos_pcie->ep_cfg->tpoweron_time;
	u32 ltr_threshold = exynos_pcie->ep_cfg->ltr_threshold;
	u32 common_restore_time = exynos_pcie->ep_cfg->common_restore_time;
	struct pci_bus *ep_bus;

	pcie_info("%s:L1SS_START(l1ss_ctrl_id_state=%#x, id=%#x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	if (exynos_pcie->state != STATE_LINK_UP) {
		spin_lock(&exynos_pcie->conf_lock);
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock(&exynos_pcie->conf_lock);
		pcie_info("%s: It's not needed. This will be set later."
				"(state = %#x, id = %#x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);

		return -1;
	} else {
		rc_dev = exynos_pcie->pci_dev;
		ep_dev = exynos_pcie_get_pci_dev(exynos_pcie);
		if (exynos_pcie->ep_pci_bus == NULL)
			exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
		ep_bus = exynos_pcie->ep_pci_bus;

		if (ep_dev == NULL || ep_bus == NULL) {
			pcie_err("Failed to set L1SS %s (ep_dev/ep_bus == NULL)!!!\n",
				 enable ? "ENABLE" : "FALSE");
			return -EINVAL;
		}

		if (ep_dev->l1ss == 0x0 || ep_dev->pcie_cap == 0x0) {
			pcie_err("Can't set L1ss : No Capability Pointers.\n");
			return -EINVAL;
		}
	}

	spin_lock(&exynos_pcie->conf_lock);
	if (enable) {	/* enable == 1 */
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);

		if (exynos_pcie->l1ss_ctrl_id_state == 0) {
			/* RC & EP L1SS & ASPM setting */
			/* 1-1 RC : set LTR Threshold, Common mode restore time */
			exynos_pcie_rc_rd_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1, 4, &val);
			val &= ~(PCI_L1SS_CTL1_LTR_L12_TH_SCALE |
				 PCI_L1SS_CTL1_LTR_L12_TH_VALUE |
				 PCI_L1SS_CTL1_CM_RESTORE_TIME);
			val |= ltr_threshold | common_restore_time;
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1, 4, val);
			/* 1-2 RC: set TPOWERON time */
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL2,
						   4, tpoweron_time);
			/* 1-3 RC: set LTR_EN */
			exynos_pcie_rc_rd_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_DEVCTL2, 4, &val);
			val |= PCI_EXP_DEVCTL2_LTR_EN;
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_DEVCTL2, 4, val);

			/* 2-1 EP: set LTR Threshold */
			exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL1, 4, &val);
			val |= ltr_threshold;
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL1, 4, val);
			/* 2-2 EP: set TPOWERON */
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL2,
					       4, tpoweron_time);
			/* 2-3 EP: set LTR_EN */
			exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, ep_dev->pcie_cap + PCI_EXP_DEVCTL2, 4, &val);
			val |= PCI_EXP_DEVCTL2_LTR_EN;
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->pcie_cap + PCI_EXP_DEVCTL2, 4, val);

			/* EP Specific Configurations. */
			if (exynos_pcie->ep_cfg->l1ss_ep_specific_cfg)
				exynos_pcie->ep_cfg->l1ss_ep_specific_cfg(exynos_pcie);

			/* 3-1 RC, Enable PCI-PM L1.1/2, ASPM L1.1/2 */
			exynos_pcie_rc_rd_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1,
						   4, &val);
			val |= PCI_L1SS_CTL1_ALL_PM_EN;
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1, 4, val);
			/* 3-2 EP, Enable PCI-PM L1.1/2, ASPM L1.1/2 */
			exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL1, 4, &val);
			val |= PCI_L1SS_CTL1_ALL_PM_EN;
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL1, 4, val);

			/* 3-3 RC, Enable ASPM L1 Entry, Common Clock configuration */
			exynos_pcie_rc_rd_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_LNKCTL, 4, val);

			/* 3-4 EP, Enable ASPM L1 Entry, Common Clock configuration,
			 *	   Clock Power Management.
			 */
			exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, ep_dev->pcie_cap + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_CLKREQ_EN |
				PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->pcie_cap + PCI_EXP_LNKCTL, 4, val);

		if (exynos_pcie->phy->dbg_ops != NULL)
			exynos_pcie->phy->dbg_ops(exynos_pcie, L1SS_EN);

		}
	} else {	/* enable == 0 */
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			/* 1. EP ASPM Disable */
			exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, ep_dev->pcie_cap + PCI_EXP_LNKCTL, 4, &val);
			val &= ~(PCI_EXP_LNKCTL_ASPMC);
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->pcie_cap + PCI_EXP_LNKCTL, 4, val);

			/* 2. RC ASPM Disable */
			exynos_pcie_rc_rd_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_LNKCTL, 4, &val);
			val &= ~(PCI_EXP_LNKCTL_ASPMC);
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_LNKCTL, 4, val);

			/* EP: clear L1SS */
			exynos_pcie_rc_rd_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL1, 4, &val);
			val &= ~(PCI_L1SS_CTL1_ALL_PM_EN);
			exynos_pcie_rc_wr_other_conf(pp, ep_bus, 0, ep_dev->l1ss + PCI_L1SS_CTL1, 4, val);

			/* RC: clear L1SS */
			exynos_pcie_rc_rd_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1, 4, &val);
			val &= ~(PCI_L1SS_CTL1_ALL_PM_EN);
			exynos_pcie_rc_wr_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1, 4, val);

			if (exynos_pcie->phy->dbg_ops != NULL)
				exynos_pcie->phy->dbg_ops(exynos_pcie, L1SS_DIS);
		}
	}
	spin_unlock(&exynos_pcie->conf_lock);

	/* Prints L1ss related registers for debugging purpose
	exynos_pcie_rc_rd_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL1, 4, &val);
	pcie_info("RC : L1SUB CAP - Control1 : %#x\n", val);
	exynos_pcie_rc_rd_own_conf(pp, rc_dev->l1ss + PCI_L1SS_CTL2, 4, &val);
	pcie_info("RC : L1SUB CAP - Control2 : %#x\n", val);
	exynos_pcie_rc_rd_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_DEVCTL2, 4, &val);
	pcie_info("RC : PCIe CAP - Device Control2 : %#x\n", val);
	exynos_pcie_rc_rd_own_conf(pp, rc_dev->pcie_cap + PCI_EXP_LNKCTL, 4, &val);
	pcie_info("RC : PCIe CAP - Link Control : %#x\n", val);

	pci_read_config_dword(ep_dev, ep_dev->l1ss + PCI_L1SS_CTL1, &val);
	pcie_info("EP : L1SUB CAP - Control1 : %#x\n", val);
	pci_read_config_dword(ep_dev, ep_dev->l1ss + PCI_L1SS_CTL2, &val);
	pcie_info("EP : L1SUB CAP - Control2 : %#x\n", val);
	pci_read_config_dword(ep_dev, ep_dev->pcie_cap + PCI_EXP_DEVCTL2, &val);
	pcie_info("EP : PCIe CAP - Device Control2 : %#x\n", val);
	pci_read_config_dword(ep_dev, ep_dev->pcie_cap + PCI_EXP_LNKCTL, &val);
	pcie_info("EP : PCIe CAP - Link Control : %#x\n", val);

	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2);

	pcie_dbg("LTSSM: %#lx, PM_DSTATE = %#lx, L1SUB = %#lx\n",
		  (val & CTRL_LINK_STATE_LTSSM),
		  (val & CTRL_LINK_STATE_PM_DSTATE) >> 9,
		  (val & CTRL_LINK_STATE_L1SUB) >> 17);
	pcie_dbg("%s:L1SS_END(l1ss_ctrl_id_state=%#x, id=%#x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);
	*/

	return 0;
}

int exynos_pcie_l1ss_ctrl(int enable, int id, int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}

	return exynos_pcie_set_l1ss(exynos_pcie, enable, id);
}
EXPORT_SYMBOL(exynos_pcie_l1ss_ctrl);

int exynos_pcie_l1_exit(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	u32 count = 0, ret = 0, val = 0;
	unsigned long flags;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}

	spin_lock_irqsave(&exynos_pcie->pcie_l1_exit_lock, flags);

	/* Need to check... */
	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* Set s/w L1 exit mode */
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_PM_CTRL);
		val |= SSC_PM_CTRL_EXIT_ASPM_L1;
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_PM_CTRL);

		/* Max timeout = 3ms (300 * 10us) */
		while (count < MAX_L1_EXIT_TIMEOUT) {
			val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
					      SSC_LINK_DBG_2_LTSSM_STATE;
			if (val == S_L0)
				break;

			count++;

			udelay(10);
		}

		if (count >= MAX_L1_EXIT_TIMEOUT) {
			pcie_err("%s: cannot change to L0(LTSSM = 0x%x, cnt = %d)\n",
				 __func__, val, count);
			ret = -EPIPE;
		} else {
			/* remove too much print */
			/* pcie_err("%s: L0 state(LTSSM = 0x%x, cnt = %d)\n",
			   __func__, val, count); */
		}

		/* Clear s/w L1 exit mode */
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_PM_CTRL);
		val &= ~SSC_PM_CTRL_EXIT_ASPM_L1;
		exynos_ssc_write(exynos_pcie, val, PCIE_SSC_PM_CTRL);
	} else {
		/* remove too much print */
		/* pcie_err("%s: skip!!! l1.2 is already disabled(id = 0x%x)\n",
		   __func__, exynos_pcie->l1ss_ctrl_id_state); */
	}

	spin_unlock_irqrestore(&exynos_pcie->pcie_l1_exit_lock, flags);

	return ret;

}
EXPORT_SYMBOL(exynos_pcie_l1_exit);

int exynos_pcie_rc_speed_change(int ch_num, int req_speed)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	u32 current_speed;
	u32 val, count = 0;

	if (exynos_pcie == NULL || exynos_pcie->state != STATE_LINK_UP)
		return -EINVAL;

	pci = exynos_pcie->pci;
	pp = &pci->pp;

	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL, 4, &current_speed);
	current_speed = (current_speed >> 16) & PCI_EXP_LNKSTA_CLS;


	if (req_speed == current_speed) {
		pcie_err("%s : Don't need to change speed.(req:%d, cur:%d)\n",
			 __func__, req_speed, current_speed);
		return 0;
	}

	if (req_speed > exynos_pcie->max_link_speed) {
		pcie_err("%s : Unsupported speed.(req:%d, max:%d)\n",
			 __func__, req_speed, exynos_pcie->max_link_speed);
		return -EINVAL;
	}

	/* Check LTSSM before speed change */
	while (count < 3000) {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
				      SSC_LINK_DBG_2_LTSSM_STATE;
		if (val == S_L0)
			break;

		count++;

		udelay(10);
	}
	if (val != S_L0)
		pcie_err("%s : It's NOT L0 state!\n", __func__);

	pcie_info("Start Speed Change %d --> %d\n", current_speed, req_speed);

	/* Set Target Speed */
	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL2, 4, &val);
	val &= ~PCI_EXP_LNKCTL2_TLS;
	val |= req_speed;
	exynos_pcie_rc_wr_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL2, 4, val);

	/* Direct Speed Change toggle*/
	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val &= ~PORT_LOGIC_SPEED_CHANGE;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val |= PORT_LOGIC_SPEED_CHANGE;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);

	/* Check LTSSM to L0 */
	while (count < 3000) {
		val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
				      SSC_LINK_DBG_2_LTSSM_STATE;
		if (val == S_L0)
			break;

		count++;

		udelay(10);
	}

	if (val != S_L0) {
		pcie_err("%s : Link-up fail on dynamic speed change!\n", __func__);
		return -EPERM;
	}

	/* Check current speed */
	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL, 4, &current_speed);
	current_speed = (current_speed >> 16) & PCI_EXP_LNKSTA_CLS;

	if (current_speed != req_speed) {
		pcie_err("Speed change Failure!!! (req:%d, cur:%d)\n",
			 req_speed, current_speed);
		return -EPERM;
	} else
		pcie_info("%s : Speed Change Successful.\n", __func__);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_speed_change);

int exynos_pcie_rc_lane_change(int ch_num, int req_lane)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	u32 current_lane, max_lane;
	u32 val, i;

	if (exynos_pcie == NULL || exynos_pcie->state != STATE_LINK_UP)
		return -EINVAL;

	pci = exynos_pcie->pci;
	pp = &pci->pp;

	/* Check current lane state */
	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL, 4, &val);
	current_lane = (val >> 20) & PCI_EXP_LNKSTA_NEGO_LANE;
	if (req_lane == current_lane) {
		pcie_err("%s : Don't need to change lane.(req:%d, cur:%d)\n",
			 __func__, req_lane, current_lane);
		return 0;
	}

	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCAP, 4, &val);
	max_lane = (val >> 4) & PCI_EXP_LNKSTA_NEGO_LANE;
	if (req_lane > max_lane) {
		pcie_err("%s : Unsupported lane number.(req:%d, max:%d)\n",
			 __func__, req_lane, max_lane);
		return -EINVAL;
	}

	pcie_info("Start Lane Change %d --> %d\n", current_lane, req_lane);
	/* Don't need to change lane number configuration at here.
	 * exynos_pcie_rc_lane_config(exynos_pcie, req_lane);
	 */

	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_MULTI_LANE_CTRL, 4, &val);
	val &= ~PCI_EXP_LNKSTA_NEGO_LANE;
	val |= req_lane;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_MULTI_LANE_CTRL, 4, val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_MULTI_LANE_CTRL, 4, &val);
	val |= PORT_MLTI_DIR_LANE_CHANGE;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_MULTI_LANE_CTRL, 4, val);

	/* Wait for Lane Change */
	for (i = 0; i < 1000; i++) {
		exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				PCI_EXP_LNKCTL, 4, &val);
		current_lane = (val >> 20) & PCI_EXP_LNKSTA_NEGO_LANE;

		if (current_lane == req_lane)
			break;

		usleep_range(1000, 1200);
	}

	if (current_lane != req_lane) {
		pcie_err("Lane change Failure!!! (req:%d, cur:%d)\n",
			 req_lane, current_lane);
		return -EPERM;
	} else
		pcie_info("%s : Lane Change Successful.\n", __func__);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_lane_change);

int exynos_pcie_host_v1_poweron(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;

	pcie_err("### [%s] requested spd: GEN%d\n", __func__, spd);
	exynos_pcie->max_link_speed = spd;
	pci->link_gen = spd;

	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweron);

int exynos_pcie_rc_poweron_speed(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pcie_err("### [%s] requested spd: GEN%d\n", __func__, spd);
	exynos_pcie->target_speed = spd;

	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_rc_poweron_speed);

void exynos_pcie_host_v1_poweroff(int ch_num)
{
	return exynos_pcie_rc_poweroff(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweroff);

int exynos_pcie_pm_resume(int ch_num)
{
	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_resume);

void exynos_pcie_pm_suspend(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	unsigned long flags;

	pcie_dbg("## %s(state: %d)\n", __func__, exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info("RC%d already off\n", exynos_pcie->ch_num);
		return;
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	exynos_pcie->state = STATE_LINK_DOWN_TRY;
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

	exynos_pcie_rc_poweroff(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_suspend);

bool exynos_pcie_rc_get_cpl_timeout_state(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}

	return exynos_pcie->cpl_timeout_recovery;
}
EXPORT_SYMBOL(exynos_pcie_rc_get_cpl_timeout_state);

void exynos_pcie_rc_set_cpl_timeout_state(int ch_num, bool recovery)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return;
	}

	pcie_err("[%s] set cpl_timeout_state to recovery_on\n", __func__);
	exynos_pcie->cpl_timeout_recovery = recovery;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_cpl_timeout_state);

/* PCIe link status checking function from S359 ref. code */
int exynos_pcie_rc_chk_link_status(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct device *dev;

	u32 val;
	int link_status;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return 0;

	val = exynos_ssc_read(exynos_pcie, PCIE_SSC_LINK_DBG_2) &
			      SSC_LINK_DBG_2_LTSSM_STATE;
	if (val >= S_RCVRY_LOCK && val <= S_L1_IDLE) {
		link_status = 1;
	} else {
		pcie_err("Check unexpected state - H/W:0x%x, S/W:%d\n",
				val, exynos_pcie->state);
		/* exynos_pcie->state = STATE_LINK_DOWN; */
		link_status = 1;
	}

	return link_status;
}
EXPORT_SYMBOL(exynos_pcie_rc_chk_link_status);

int exynos_pcie_register_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct dw_pcie_rp *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	u32 id;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (reg->events == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (reg->events == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		id = 1;
	} else {
		pcie_err("PCIe: unknown event!!!\n");
		return -EINVAL;
	}
	pcie_err("[%s] event = 0x%x, id = %d\n", __func__, reg->events, id);

	if (pp) {
		exynos_pcie->rc_event_reg[id] = reg;
		pcie_info("Event 0x%x is registered for RC %d\n",
			  reg->events, exynos_pcie->ch_num);
	} else {
		pcie_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_register_event);

int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct dw_pcie_rp *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	u32 id;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}

	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (reg->events == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (reg->events == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		id = 1;
	} else {
		pcie_err("PCIe: unknown event!!!\n");
		return -EINVAL;
	}

	if (pp) {
		exynos_pcie->rc_event_reg[id] = NULL;
		pcie_info("Event is deregistered for RC %d\n",
				exynos_pcie->ch_num);
	} else {
		pcie_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_deregister_event);

int exynos_pcie_host_v1_register_event(struct exynos_pcie_register_event *reg)
{
	return exynos_pcie_register_event(reg);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_register_event);

int exynos_pcie_host_v1_deregister_event(struct exynos_pcie_register_event *reg)
{
	return exynos_pcie_deregister_event(reg);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_deregister_event);

int exynos_pcie_rc_set_affinity(int ch_num, int affinity)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	int i, j, num_ctrls, counter = 0;
	u32 *vec_msi_use;
	u32 val;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;
	vec_msi_use = (u32 *)pp->msi_irq_in_use;
	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	/* Find enabled MSI */
	for (i = 0; i < num_ctrls ; i++) {
		val = vec_msi_use[i];
		for (j = 0; j < 32; j++) {
			if (val & 0x1)
				counter++;
			val >>= 1;
		}
	}

	if (counter >= MAX_MSI_IRQS_PER_CTRL) {
		pcie_info("Set CPU affinity MSI vector1 - %d.\n", affinity);
		irq_set_affinity_hint(pp->msi_irq[1], cpumask_of(affinity));
	} else {
		pcie_info("Set CPU affinity MSI vector0 - %d.\n", affinity);
		irq_set_affinity_hint(pp->msi_irq[0], cpumask_of(affinity));
	}

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_affinity);

static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				   const struct cpumask *mask, bool force)
{
	struct dw_pcie_rp *pp = irq_data->parent_data->domain->host_data;
	struct irq_data *pdata = irq_data->parent_data;
	struct dw_pcie *pci;
	struct exynos_pcie *exynos_pcie;
	unsigned int ctrl;
	int irq;
	struct irq_data *idata;

	if (pp == NULL || pdata == NULL)
		return -EINVAL;

	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (!exynos_pcie->is_ep_scaned)
		return IRQ_SET_MASK_OK;

	ctrl = pdata->hwirq / MAX_MSI_IRQS_PER_CTRL;
	irq = sep_msi_vec[exynos_pcie->ch_num][ctrl].irq;

	pcie_info("%s : PCIe MSI vector%d(irq:%d) CPU affinity change : %#x\n",
		  __func__, ctrl, irq, *((u32*)mask));

	idata = irq_get_irq_data(irq);
	idata->chip->irq_set_affinity(idata, mask, force);

	return IRQ_SET_MASK_OK;
}

static int exynos_pcie_msi_set_wake(struct irq_data *irq_data, unsigned int enable)
{
	struct dw_pcie_rp *pp;
	struct irq_data *pdata;
	struct dw_pcie *pci;
	struct exynos_pcie *exynos_pcie;
	unsigned int ctrl;
	int irq, ret = 0;

	if (irq_data == NULL)
		return -EINVAL;

	pp = irq_data->parent_data->domain->host_data;
	pdata = irq_data->parent_data;

	if (pp == NULL || pdata == NULL)
		return -EINVAL;

	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	ctrl = pdata->hwirq / MAX_MSI_IRQS_PER_CTRL;
	irq = sep_msi_vec[exynos_pcie->ch_num][ctrl].irq;

	pr_info("%s : PCIe MSI vector%d(irq:%d) %s irq wake \n",
		__func__, ctrl, irq, enable ? "Enable" : "Disable");

	if (enable)
		ret = enable_irq_wake(irq);
	else
		ret = disable_irq_wake(irq);

	return ret;
}

static void exynos_pcie_msi_irq_enable(struct irq_data *data)
{
	/*
	struct irq_desc *desc = irq_data_to_desc(data);

	if (desc->handle_irq != handle_level_irq) {
		pr_info("PCIe MSI irq %d changed to level_irq.\n", data->irq);
		irq_set_handler_locked(data, handle_level_irq);
	}
	*/

	data->chip->irq_unmask(data);
}

static void exynos_pcie_msi_irq_disable(struct irq_data *data)
{
	data->chip->irq_mask(data);
}

static inline void exynos_pcie_enable_sep_msi(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	int i;

	for (i = exynos_pcie->separated_msi_start_num; i < MAX_MSI_CTRLS; i++) {
		if (sep_msi_vec[exynos_pcie->ch_num][i].is_used) {
			/* Enable MSI interrupt for separated MSI. */
			pr_info("Separated MSI%d is Enabled.\n", i);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
					(i * MSI_REG_CTRL_BLOCK_SIZE), 4, 0x1);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
					(i * MSI_REG_CTRL_BLOCK_SIZE), 4, ~(0x1));
		}
	}
}

static const char *sep_irq_name[MAX_MSI_CTRLS] = {
	"exynos-pcie-msi0", "exynos-pcie-msi1", "exynos-pcie-msi2",
	"exynos-pcie-msi3", "exynos-pcie-msi4", "exynos-pcie-msi5",
	"exynos-pcie-msi6", "exynos-pcie-msi7" };

static irqreturn_t exynos_pcie_rc_msi_default_handler(int irq, void *arg)
{
	struct dw_pcie_rp *pp = arg;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int vec_num = irq - sep_msi_vec[exynos_pcie->ch_num][0].irq;
	struct separated_msi_vector *msi_vec;

	if (unlikely(vec_num < 0 || vec_num >= MAX_MSI_CTRLS)) {
		pcie_err("Unexpected vector number(irq:%d, vec_num:%d)!!\n",
			 irq, vec_num);
		return IRQ_HANDLED;
	}

	msi_vec = &sep_msi_vec[exynos_pcie->ch_num][vec_num];
	if (unlikely(!msi_vec->is_used)) {
		pcie_err("Unexpected separated MSI%d interrupt!", vec_num);
		goto clear_irq;
	}

	if (exynos_pcie->phy->dbg_ops != NULL)					\
		exynos_pcie->phy->dbg_ops(exynos_pcie, MSI_HNDLR);		\

	dw_handle_msi_irq(pp);

clear_irq:
	return IRQ_HANDLED;
}

static irqreturn_t exynos_pcie_rc_msi_separated_handler(int irq, void *arg)
{
	struct dw_pcie_rp *pp = arg;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int vec_num = irq - sep_msi_vec[exynos_pcie->ch_num][0].irq;
	struct separated_msi_vector *msi_vec;

	if (unlikely(vec_num < 0 || vec_num >= MAX_MSI_CTRLS)) {
		pcie_err("Unexpected vector number(irq:%d, vec_num:%d)!!\n",
			 irq, vec_num);
		return IRQ_HANDLED;
	}

	msi_vec = &sep_msi_vec[exynos_pcie->ch_num][vec_num];
	if (unlikely(!msi_vec->is_used)) {
		pcie_err("Unexpected separated MSI%d interrupt!", vec_num);
		goto clear_irq;
	}

	if (exynos_pcie->phy->dbg_ops != NULL)					\
		exynos_pcie->phy->dbg_ops(exynos_pcie, MSI_HNDLR);		\

	if (msi_vec->msi_irq_handler != NULL)
		msi_vec->msi_irq_handler(irq, msi_vec->context);

clear_irq:
	return IRQ_HANDLED;
}

int register_separated_msi_vector(int ch_num, irq_handler_t handler, void *context,
		int *irq_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	int i, ret;

	if (!exynos_pcie->probe_done) {
		pr_err("%s: RC%d has not been probed yet\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	pp = &pci->pp;

	for (i = 1; i < MAX_MSI_CTRLS; i++) {
		if (!sep_msi_vec[ch_num][i].is_used)
			break;
	}

	if (i == MAX_MSI_CTRLS) {
		pr_info("PCIe Ch%d : There is no empty MSI vector!\n", ch_num);
		return -EBUSY;
	}

	if (handler == NULL || context == NULL) {
		pcie_err("%s : There are no handler or context!\n", __func__);
		return -EINVAL;
	}

	pr_info("PCIe Ch%d MSI%d vector is registered!\n", ch_num, i);
	sep_msi_vec[ch_num][i].is_used = true;
	sep_msi_vec[ch_num][i].context = context;
	sep_msi_vec[ch_num][i].msi_irq_handler = handler;
	*irq_num = sep_msi_vec[ch_num][i].irq;

	/* Enable MSI interrupt for separated MSI. */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
			(i * MSI_REG_CTRL_BLOCK_SIZE), 4, 0x1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
			(i * MSI_REG_CTRL_BLOCK_SIZE), 4, ~(0x1));

	ret = devm_request_irq(pci->dev, sep_msi_vec[ch_num][i].irq,
			exynos_pcie_rc_msi_separated_handler,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			sep_irq_name[i], pp);
	if (ret) {
		pr_err("failed to request MSI%d irq\n", i);

		return ret;
	}

	return i * 32;
}
EXPORT_SYMBOL_GPL(register_separated_msi_vector);


#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
int exynos_pcie_rc_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_pcie *exynos_pcie = container_of(nb, struct exynos_pcie, itmon_nb);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct itmon_notifier *itmon_info = nb_data;
	unsigned int val = 0;

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	if (!strncmp(itmon_info->dest, "HSI1", 4) || !strncmp(itmon_info->master, "HSI1", 4)) {
		if (exynos_pcie->pmu_phy_isolation) {
			exynos_pmu_read(exynos_pcie->pmu_phy_isolation, &val);
			pcie_info("### PMU PHY Isolation : %#x(LinkState:%d)\n",
				  val, exynos_pcie->state);
		} else {
			pcie_info("### PCIe LinkState:%d\n", exynos_pcie->state);
		}
		pcie_info("### PCIe(HSI1) block power : %d (0:ACTIVE, 2:SUSPENDED)\n",
			  pci->dev->power.runtime_status);
		if (!val)
			return NOTIFY_DONE;

		exynos_pcie_rc_all_dump(exynos_pcie->ch_num);
	}

	return NOTIFY_DONE;
}
#endif

static int exynos_pcie_rc_set_msi_irq(struct platform_device *pdev,
					struct dw_pcie_rp *pp, int ch_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	char msi_name[] = "msiX";
	int ret = 0, i, sep_irq, irq;

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		sep_irq = platform_get_irq(pdev, i + 1);
		if (sep_irq < 0) {
			pcie_err("MSI vector%d is not defined...\n", i);
			sep_msi_vec[ch_num][i].irq = sep_irq;
			sep_msi_vec[ch_num][i].is_used = true;

			continue;
		}

		sep_msi_vec[ch_num][i].irq = sep_irq;

		msi_name[3] = '0' + i;
		irq = platform_get_irq_byname_optional(pdev, msi_name);
		if (irq > 0) {
			/* MSI name(msiX) is defined. It means using default
			 * msi handler in designware stack.
			 */
			sep_msi_vec[ch_num][i].is_used = true;
			exynos_pcie->separated_msi_start_num = i + 1;

			pcie_info("MSI vector%d(irq:%d) is used as default handler\n",
				   i, sep_irq);
		} else if (i < 2) {
			/* In case of using exynos msi default handler.
			 * In this case, both msi0 and msi1 should NOT defined in DT.
			 */
			pcie_err("WARN : msi0/1 is not defined in DT(vec:%d)!!\n", i);
			pcie_err("MSI vector%d(irq:%d) is set default handler\n", i,
				 sep_irq);

			sep_msi_vec[ch_num][i].is_used = true;
			exynos_pcie->separated_msi_start_num = i + 1;
			/* set default handler */
			ret = devm_request_irq(pci->dev, sep_msi_vec[ch_num][i].irq,
					exynos_pcie_rc_msi_default_handler,
					IRQF_SHARED | IRQF_TRIGGER_HIGH,
					sep_irq_name[i], pp);
			if (ret) {
				pr_err("failed to request MSI%d irq\n", i);

				return ret;
			}
			/* To avoid designware stack error */
			pp->msi_irq[0] = -EINVAL;
			pp->num_vectors = 64;
		} else {
			pcie_info("MSI vector%d(irq:%d) is used as separated MSI.\n",
				  i, sep_irq);
		}
	}

	return ret;
}

static int exynos_pcie_rc_check_range(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	struct resource_entry *tmp = NULL, *entry = NULL;

	pcie_info("Check PCIe outbound region.\n");

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;

	if (entry == NULL) {
		pcie_err("%s : Can't get resource entry.\n", __func__);
		return -EINVAL;
	}

	if (upper_32_bits(entry->res->start)) {
		pcie_info("Configure PCI_BRIDGE_MEM_WINDOW for 36bit range\n");
		exynos_pcie->pci_dev->resource[PCI_BRIDGE_MEM_WINDOW].flags |=
					IORESOURCE_MEM_64 | IORESOURCE_SIZEALIGN;
		exynos_pcie->pci_dev->resource[PCI_BRIDGE_MEM_WINDOW].start =
					entry->res->start;
		exynos_pcie->pci_dev->resource[PCI_BRIDGE_MEM_WINDOW].end =
					entry->res->end;
	} else {
		pcie_info("PCIe outbound region is in 32bit range.\n");
	}

	return 0;

}

static int exynos_pcie_rc_add_port(struct platform_device *pdev,
					struct dw_pcie_rp *pp, int ch_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct irq_domain *msi_domain;
	struct msi_domain_info *msi_domain_info;
	int ret;
	u32 val, vendor_id, device_id;

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		pcie_err("failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_rc_irq_handler,
			       IRQF_SHARED | IRQF_TRIGGER_HIGH, "exynos-pcie", pp);
	if (ret) {
		pcie_err("failed to request irq\n");
		return ret;
	}

	exynos_pcie->debug_irq = platform_get_irq(pdev, 9);
	if (exynos_pcie->debug_irq < 0) {
		pcie_err("Debug_irq is not defined.(It's OK)\n");
	} else {
		ret = devm_request_irq(&pdev->dev, exynos_pcie->debug_irq,
				exynos_pcie_rc_dbg_irq_handler,
				IRQF_SHARED | IRQF_TRIGGER_HIGH, "exynos-dbg-irq", pp);
		if (ret) {
			pcie_err("failed to request debug irq\n");
			return ret;
		}
	}

	exynos_pcie_rc_set_msi_irq(pdev, pp, ch_num);

	pp->ops = &exynos_pcie_rc_ops;

	exynos_pcie_setup_rc(exynos_pcie);
	ret = dw_pcie_host_init(pp);
	if (ret) {
		pcie_err("failed to dw pcie host init\n");
		return ret;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
	vendor_id = val & ID_MASK;
	device_id = (val >> 16) & ID_MASK;
	exynos_pcie->pci_dev = pci_get_device(vendor_id, device_id, NULL);
	if (!exynos_pcie->pci_dev) {
		pcie_err("Failed to get pci device\n");
		return -ENODEV;
	}
	pcie_info("(%s): Exynos RC vendor/device id = 0x%x\n",
		  __func__, val);

	/* Check PCIe slave region */
	exynos_pcie_rc_check_range(exynos_pcie);

	exynos_pcie->pci_dev->dma_mask = DMA_BIT_MASK(36);
	exynos_pcie->pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
	dma_set_seg_boundary(&(exynos_pcie->pci_dev->dev), DMA_BIT_MASK(36));

	if (pp->msi_domain) {
		msi_domain = pp->msi_domain;
		msi_domain_info = (struct msi_domain_info *)msi_domain->host_data;
		msi_domain_info->chip->irq_set_affinity = exynos_pcie_msi_set_affinity;
		msi_domain_info->chip->irq_set_wake = exynos_pcie_msi_set_wake;
		msi_domain_info->chip->irq_enable = exynos_pcie_msi_irq_enable;
		msi_domain_info->chip->irq_disable = exynos_pcie_msi_irq_disable;
		msi_domain_info->ops->msi_prepare = exynos_msi_prepare_irq;
	}

	return 0;
}

static int exynos_pcie_rc_make_reg_tb(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	unsigned int pos, val, id;
	int i;

	/* initialize the reg table */
	for (i = 0; i < PCI_FIND_CAP_TTL; i++) {
		exynos_pcie->pci_cap[i] = 0;
		exynos_pcie->pci_ext_cap[i] = 0;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_CAPABILITY_LIST, 4, &val);
	pos = 0xFF & val;

	while (pos) {
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		id = val & CAP_ID_MASK;
		if (id >= PCI_FIND_CAP_TTL) {
			pcie_err("Unexpected PCI_CAP number!\n");
			break;
		}
		exynos_pcie->pci_cap[id] = pos;
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		pos = (val & CAP_NEXT_OFFSET_MASK) >> 8;
		pcie_dbg("Next Cap pointer : 0x%x\n", pos);
	}

	pos = PCI_CFG_SPACE_SIZE;

	while (pos) {
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		if (val == 0) {
			pcie_info("we have no ext capabilities!\n");
			break;
		}
		id = PCI_EXT_CAP_ID(val);
		if (id >= PCI_FIND_CAP_TTL) {
			pcie_err("Unexpected PCI_EXT_CAP number!\n");
			break;
		}
		exynos_pcie->pci_ext_cap[id] = pos;
		pos = PCI_EXT_CAP_NEXT(val);
		pcie_dbg("Next ext Cap pointer : 0x%x\n", pos);
	}

	for (i = 0; i < PCI_FIND_CAP_TTL; i++) {
		if (exynos_pcie->pci_cap[i])
			pcie_info("PCIe cap [0x%x][%s]: 0x%x\n",
						i, CAP_ID_NAME(i), exynos_pcie->pci_cap[i]);
	}
	for (i = 0; i < PCI_FIND_CAP_TTL; i++) {
		if (exynos_pcie->pci_ext_cap[i])
			pcie_info("PCIe ext cap [0x%x][%s]: 0x%x\n",
					i, EXT_CAP_ID_NAME(i), exynos_pcie->pci_ext_cap[i]);
	}
	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_PM)
u32 pcie_linkup_stat(void)
{
	struct exynos_pcie *exynos_pcie;
	int i;

	for (i = 0; i < MAX_RC_NUM; i++) {
		exynos_pcie = &g_pcie_rc[i];
		if (exynos_pcie->pcie_is_linkup) {
			pr_info("[%s] pcie_is_linkup : %d\n",
					__func__, exynos_pcie->pcie_is_linkup);

			return exynos_pcie->pcie_is_linkup;
		}
	}
	pr_info("[%s] All PCIe channel  pcie_is_linkup: false\n",__func__);

	return exynos_pcie->pcie_is_linkup;
}
EXPORT_SYMBOL_GPL(pcie_linkup_stat);
#endif

void exynos_pcie_rc_check_function_validity(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	struct exynos_pcie_register_event temp_event;
	int irq_num;

	exynos_pcie_rc_set_outbound_atu(0, 0x27200000, 0x0, SZ_1M);
	exynos_pcie_rc_print_msi_register(exynos_pcie->ch_num);
	exynos_pcie_rc_msi_init_cp(exynos_pcie);

	/* Check external exported functions */
	temp_event.events = EXYNOS_PCIE_EVENT_CPL_TIMEOUT;
	temp_event.user = NULL;
	temp_event.mode = EXYNOS_PCIE_TRIGGER_CALLBACK;
	temp_event.callback = NULL;

	exynos_pcie_l1_exit(exynos_pcie->ch_num);
	exynos_pcie_rc_chk_link_status(exynos_pcie->ch_num);
	exynos_pcie_host_v1_register_event(&temp_event);
	exynos_pcie_host_v1_deregister_event(&temp_event);
	irq_num = exynos_pcie_get_irq_num(exynos_pcie->ch_num);

	exynos_pcie_cold_reset(exynos_pcie);
	exynos_pcie_warm_reset(exynos_pcie);

	exynos_pcie_set_perst_gpio(exynos_pcie->ch_num, 0);
	exynos_pcie_set_ready_cto_recovery(exynos_pcie->ch_num);
	exynos_pcie_host_v1_poweron(exynos_pcie->ch_num, 2);
	exynos_pcie_host_v1_poweroff(exynos_pcie->ch_num);
	exynos_pcie_pm_resume(exynos_pcie->ch_num);
	exynos_pcie_pm_suspend(exynos_pcie->ch_num);
	exynos_pcie_rc_get_cpl_timeout_state(exynos_pcie->ch_num);
	exynos_pcie_rc_set_cpl_timeout_state(exynos_pcie->ch_num, 0);
	exynos_pcie_msi_set_wake(NULL, 0);
	exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
	exynos_pcie_rc_set_affinity(exynos_pcie->ch_num, 0);
	exynos_pcie_rc_cpl_timeout_work(&exynos_pcie->cpl_timeout_work.work);

	/* Check separated MSI related function */
	register_separated_msi_vector(exynos_pcie->ch_num, NULL, NULL, &irq_num);
	exynos_pcie_rc_msi_default_handler(irq_num, pp);
	exynos_pcie_rc_msi_separated_handler(irq_num, pp);

	exynos_pcie_rc_ia(exynos_pcie);
}

static void exynos_pcie_rc_disable_unused_lane(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	/* Disable unused lane */
	val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_LANE1_OVRD_2);
	val &= ~DWPHY_LANE1_RXDET_OVRD_VAL;
	val |= DWPHY_LANE1_RXDET_OVRD_EN;
	exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_LANE1_OVRD_2);

	val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RAWLANEON1_OVRD_IN_0);
	val &= ~DWPHY_RX_TERM_OVRD_VAL;
	val |= DWPHY_RX_TERM_OVRD_EN;
	exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RAWLANEON1_OVRD_IN_0);
}

static void exynos_pcie_rc_lane_config(struct exynos_pcie *exynos_pcie, int num)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	u32 val;

	pcie_info("Lane number change to %d\n", num);

	pci->num_lanes = num;

	/* Set the number of lanes */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_LINK_CONTROL, 4, &val);
	val &= ~PORT_LINK_MODE_MASK;
	switch (pci->num_lanes) {
	case 1:
		val |= PORT_LINK_MODE_1_LANES;
		break;
	case 2:
		val |= PORT_LINK_MODE_2_LANES;
		break;
	case 4:
		val |= PORT_LINK_MODE_4_LANES;
		break;
	case 8:
		val |= PORT_LINK_MODE_8_LANES;
		break;
	default:
		dev_err(pci->dev, "num-lanes %u: invalid value\n", pci->num_lanes);
		return;
	}
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_LINK_CONTROL, 4, val);

	/* Set link width speed control register */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val &= ~PORT_LOGIC_LINK_WIDTH_MASK;
	switch (pci->num_lanes) {
	case 1:
		val |= PORT_LOGIC_LINK_WIDTH_1_LANES;
		break;
	case 2:
		val |= PORT_LOGIC_LINK_WIDTH_2_LANES;
		break;
	case 4:
		val |= PORT_LOGIC_LINK_WIDTH_4_LANES;
		break;
	case 8:
		val |= PORT_LOGIC_LINK_WIDTH_8_LANES;
		break;
	}
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
}

/* EXYNOS specific control and W/A code */
static void exynos_pcie_soc_specific_ctrl(struct exynos_pcie *exynos_pcie,
					  enum exynos_pcie_opr_state state)
{
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	u32 val;

	switch (state) {
	case OPR_STATE_PROBE:
		if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x994500) {
			pcie_info("PCIe DWPHY ROM Change Enabled.\n");
			exynos_pcie->phy_rom_change = 1;
			exynos_pcie->phy_rom = phy_rom_v2_18;
			exynos_pcie->phy_rom_size = sizeof(phy_rom_v2_18);
		}
		break;
	case OPR_STATE_POWER_ON:
		if (exynos_pcie->pmu_gpio_retention) {
			//HSI1 PCIe GPIO retention release
			exynos_pmu_update(exynos_pcie->pmu_gpio_retention,
					  PCIE_GPIO_RETENTION, PCIE_GPIO_RETENTION);
		}

		break;
	case OPR_STATE_SETUP_RC:
		pci = exynos_pcie->pci;
		pp = &pci->pp;

		if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x994500) {
			if (exynos_soc_info.main_rev == 1 &&
			    exynos_soc_info.sub_rev == 0) {
				exynos_pcie_rc_lane_config(exynos_pcie, 1);
			}
		}

		if (exynos_pcie->pci_dev && exynos_pcie->ep_cfg->no_speed_check == 1 &&
		    exynos_pcie->target_speed != 0) {
			/* Set Target Speed */
			exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
					PCI_EXP_LNKCTL2, 4, &val);
			val &= ~PCI_EXP_LNKCTL2_TLS;
			val |= exynos_pcie->target_speed;
			exynos_pcie_rc_wr_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
					PCI_EXP_LNKCTL2, 4, val);
		}

		break;
	case OPR_STATE_POWER_OFF:
		/* Set refclk delay to 0x200 before sending PME turn-off */
		exynos_ctrl_write(exynos_pcie, CTRL_REFCLK_OPT7_DELAY_VAL,
				  PCIE_CTRL_REFCLK_CTL_OPT7);

		/* Set PHY gating delay to 0x200 before sending PME turn-off */
		exynos_ctrl_write(exynos_pcie, CTRL_REFCLK_OPT6_DELAY_VAL,
				  PCIE_CTRL_REFCLK_CTL_OPT6);

		break;
	case OPR_STATE_ROM_CHANGE:
		if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x994500) {
			if (exynos_pcie->pci->num_lanes == 1 ||
			    (exynos_soc_info.main_rev == 1 &&
			    exynos_soc_info.sub_rev == 0)) {
				/* Disable unnecessary lane */
				exynos_pcie_rc_disable_unused_lane(exynos_pcie);
			}

			/* RX VCO */
			pcie_info("RX VCO ver0.4 & SPO disable ver0.2 & Gen1 DCC BYPASS\n");
			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN0);
			val &= ~DWPHY_RX_VCO_CNTR_PWRUP_MASK;
			val |= DWPHY_RX_VCO_CNTR_PWRUP_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN0);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN1);
			val &= ~DWPHY_RX_VCO_CNTR_PWRUP_MASK;
			val |= DWPHY_RX_VCO_CNTR_PWRUP_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN1);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN0);
			val &= ~DWPHY_RX_VCO_UPDATE_MASK;
			val |= DWPHY_RX_VCO_UPDATE_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN0);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN1);
			val &= ~DWPHY_RX_VCO_UPDATE_MASK;
			val |= DWPHY_RX_VCO_UPDATE_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME0_LN1);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN0);
			val &= ~DWPHY_RX_VCO_STARTUP_TIME_MASK;
			val |= DWPHY_RX_VCO_STARTUP_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN0);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN1);
			val &= ~DWPHY_RX_VCO_STARTUP_TIME_MASK;
			val |= DWPHY_RX_VCO_STARTUP_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN1);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN0);
			val &= ~DWPHY_RX_VCO_CNTR_SETTLE_MASK;
			val |= DWPHY_RX_VCO_CNTR_SETTLE_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN0);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN1);
			val &= ~DWPHY_RX_VCO_CNTR_SETTLE_MASK;
			val |= DWPHY_RX_VCO_CNTR_SETTLE_TIME;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RX_VCO_CAL_TIME1_LN1);

			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R1_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B0_R1);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R2_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B0_R2);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R3_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B0_R3);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R4_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B0_R4);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R5_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B0_R5);

			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R1_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B2_R1);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R2_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B2_R2);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R3_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B2_R3);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R4_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B2_R4);
			exynos_phy_write(exynos_pcie, DWPHY_RAWMEM_CMN24_BX_R5_VAL,
					 PCIE_DWPHY_RAWMEM_CMN24_B2_R5);

			/* SPO disable */
			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_MPLLB_ANA_CREG00);
			val &= ~DWPHY_MPLLB_ANA_SPO;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_MPLLB_ANA_CREG00);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_MPLLB_VCO_OVRD_IN_0);
			val &= ~(DWPHY_VCO_OVRD_CP_INT_MASK | DWPHY_VCO_OVRD_CP_INT_GS);
			val |= DWPHY_VCO_OVRD_CP_INT_GS_OVEN | DWPHY_VCO_OVRD_CP_INT_GS_VAL;
			val |= DWPHY_VCO_OVRD_CP_INT_OVEN | DWPHY_VCO_OVRD_CP_INT_VAL;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_MPLLB_VCO_OVRD_IN_0);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_MPLLB_VCO_OVRD_IN_1);
			val &= ~(DWPHY_VCO_OVRD_CP_PROP_MASK | DWPHY_VCO_OVRD_CP_PROP_GS);
			val |= DWPHY_VCO_OVRD_CP_PROP_GS_OVEN | DWPHY_VCO_OVRD_CP_PROP_GS_VAL;
			val |= DWPHY_VCO_OVRD_CP_PROP_OVEN | DWPHY_VCO_OVRD_CP_PROP_VAL;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_MPLLB_VCO_OVRD_IN_1);

			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R1_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R1);
			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R2_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R2);
			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R3_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R3);
			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R4_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R4);
			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R5_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R5);
			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R6_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R6);
			exynos_phy_write(exynos_pcie, DWPHY_MEM_CMN24_B8_R7_VAL,
					 PCIE_DWPHY_MEM_CMN24_B8_R7);

			val = exynos_phy_read(exynos_pcie, PCIE_DWPHY_RAWMEM_CMN39_B9_R20);
			val |= DWPHY_GEN1_DCC_BYPASS;
			exynos_phy_write(exynos_pcie, val, PCIE_DWPHY_RAWMEM_CMN39_B9_R20);
		}
		break;
	case OPR_STATE_ADDITIONAL_PHY_SET:
		break;
	default:
		pcie_err("%s : Unexpected state!\n", __func__);

	}
}

static int exynos_pcie_rc_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int ch_num;

	dev_info(&pdev->dev, "## PCIe RC PROBE start\n");

	if (create_pcie_sys_file(&pdev->dev))
		dev_err(&pdev->dev, "Failed to create pcie sys file\n");

	if (create_pcie_eom_file(&pdev->dev))
		dev_err(&pdev->dev, "Failed to create pcie eom file\n");

	if (of_property_read_u32(np, "ch-num", &ch_num)) {
		dev_err(&pdev->dev, "Failed to parse the channel number\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "## PCIe ch %d ##\n", ch_num);

	/* To allocate in 32bit address range.
	 * pci = page_address(alloc_page(GFP_DMA32 | __GFP_ZERO));
	 */
	pci = devm_kzalloc(&pdev->dev, sizeof(*pci), GFP_KERNEL);
	if (!pci) {
		dev_err(&pdev->dev, "dw_pcie allocation is failed\n");
		return -ENOMEM;
	}

	exynos_pcie = &g_pcie_rc[ch_num];
	exynos_pcie->pci = pci;
	exynos_pcie->ep_cfg = of_device_get_match_data(&pdev->dev);
	exynos_pcie->ch_num = ch_num;
	exynos_pcie->state = STATE_LINK_DOWN;
	if (exynos_pcie->ep_cfg->tpoweron_time == 0x0) {
		dev_info(&pdev->dev, "!!! WARN : There is no tpoweron_time. !!!\n");
		ret = -EINVAL;
		goto probe_fail;
	}

	pci->dev = &pdev->dev;
	pci->ops = &dw_pcie_ops;
	pp = &pci->pp;

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	platform_set_drvdata(pdev, exynos_pcie);

	/* parsing pcie dts data for exynos */
	ret = exynos_pcie_rc_parse_dt(exynos_pcie, &pdev->dev);
	if (ret)
		goto probe_fail;

	/* EXYNOS specific control after DT parsing */
	exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_PROBE);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	ret = exynos_pcie_rc_get_pin_state(exynos_pcie, pdev);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_get_resource(exynos_pcie, pdev);
	if (ret)
		goto probe_fail;

	spin_lock_init(&exynos_pcie->reg_lock);
	spin_lock_init(&exynos_pcie->conf_lock);
	spin_lock_init(&exynos_pcie->pcie_l1_exit_lock);

	/* Mapping PHY functions */
	ret = exynos_pcie_rc_phy_init(exynos_pcie, exynos_pcie->phy_rom_change);
	if (ret)
		goto probe_fail;

	exynos_pcie_rc_resumed_phydown(exynos_pcie);

	ret = exynos_pcie_rc_make_reg_tb(exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_add_port(pdev, pp, ch_num);
	if (ret)
		goto probe_fail;

	if (dev_is_dma_coherent(pci->dev))
		exynos_pcie_rc_set_iocc(exynos_pcie, 1);

	disable_irq(pp->irq);

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	exynos_pcie->idle_ip_index =
			exynos_get_idle_ip_index(dev_name(&pdev->dev),
						 dev_is_dma_coherent(pci->dev));
	if (exynos_pcie->idle_ip_index < 0) {
		pcie_err("Cant get idle_ip_dex!!!\n");
		ret = -EINVAL;
		goto probe_fail;
	}

	exynos_update_ip_idle_status(exynos_pcie->idle_ip_index, PCIE_IS_IDLE);
	pcie_info("%s, ip idle status : %d, idle_ip_index: %d\n",
		  __func__, PCIE_IS_IDLE, exynos_pcie->idle_ip_index);
#endif

	exynos_pcie->pcie_wq = create_singlethread_workqueue("pcie_wq");
	if (IS_ERR(exynos_pcie->pcie_wq)) {
		pcie_err("couldn't create workqueue\n");
		ret = EBUSY;
		goto probe_fail;
	}

	INIT_DELAYED_WORK(&exynos_pcie->dislink_work, exynos_pcie_rc_dislink_work);
	INIT_DELAYED_WORK(&exynos_pcie->cpl_timeout_work, exynos_pcie_rc_cpl_timeout_work);

	pcie_sysmmu_add_pcie_fault_handler(exynos_pcie_rc_all_dump);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	exynos_pcie->itmon_nb.notifier_call = exynos_pcie_rc_itmon_notifier;
	itmon_notifier_chain_register(&exynos_pcie->itmon_nb);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PM)
	if (exynos_pcie->use_pcieon_sleep) {
		pcie_info("## register pcie connection function\n");
		register_pcie_is_connect(pcie_linkup_stat);//tmp
	}
#endif

probe_fail:
	pm_runtime_put_sync(&pdev->dev);

	if (ret)
		pcie_err("## %s: PCIe probe failed\n", __func__);
	else {
		exynos_pcie->probe_done = 1;
		pcie_info("## %s: PCIe probe success\n", __func__);
	}
	return ret;
}

static int __exit exynos_pcie_rc_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

static void exynos_pcie_rc_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	dev_info(&pdev->dev, "%s - Set probe_done to 0!\n", __func__);
	exynos_pcie->probe_done = 0;
}

#if IS_ENABLED(CONFIG_PM)
static int exynos_pcie_rc_suspend_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	pcie_info("## SUSPEND[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			exynos_pcie->pcie_is_linkup);

	if (exynos_pcie->phy->dbg_ops != NULL)
		exynos_pcie->phy->dbg_ops(exynos_pcie, SUSPEND);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info("%s: RC%d already off\n", __func__, exynos_pcie->ch_num);
		pm_runtime_put_sync(pci->dev);
		return 0;
	}

	if (!exynos_pcie->use_pcieon_sleep) { //PCIE WIFI Channel - it doesn't use PCIe on sleep.
		pcie_err("WARNNING - Unexpected PCIe state(try to power OFF)\n");
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
	}
	/* To block configuration register by PCI stack driver
	else {
		pcie_info("PCIe on sleep\n");
		pcie_info("Default must_resume value : %d\n",
				exynos_pcie->ep_pci_dev->dev.power.must_resume);
		if (exynos_pcie->ep_pci_dev->dev.power.must_resume)
			exynos_pcie->ep_pci_dev->dev.power.must_resume = false;
	}
	*/

	return 0;
}

static int exynos_pcie_rc_resume_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	pcie_info("## RESUME[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			exynos_pcie->pcie_is_linkup);

	if (!exynos_pcie->pci_dev) {
		pcie_err("Failed to get pci device\n");
	} else {
		exynos_pcie->pci_dev->state_saved = false;
		pcie_dbg("state_saved flag = false");
	}

	pm_runtime_get_sync(pci->dev);

	if (exynos_pcie->phy->dbg_ops != NULL)
		exynos_pcie->phy->dbg_ops(exynos_pcie, RESUME);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info("%s: RC%d Link down state-> phypwr off\n", __func__,
							exynos_pcie->ch_num);
		exynos_pcie_rc_resumed_phydown(exynos_pcie);
	}

	return 0;
}

static int exynos_pcie_suspend_prepare(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	pcie_info("%s - PCIe(HSI) block enable to prepare suspend.\n", __func__);
	pm_runtime_get_sync(pci->dev);

	return 0;
}

static void exynos_pcie_resume_complete(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	pm_runtime_put_sync(pci->dev);

	/* To block configuration register by PCI stack driver
	if (exynos_pcie->use_pcieon_sleep)
		exynos_pcie->ep_pci_dev->dev.power.must_resume = true;
	*/
}

static int exynos_pcie_runtime_suspend(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	/* DO PHY Isolation */
	if (exynos_pcie->pmu_phy_isolation) {
		exynos_pmu_update(exynos_pcie->pmu_phy_isolation,
				  PCIE_PHY_CONTROL_MASK, 0);
	}

	return 0;
}

static int exynos_pcie_runtime_resume(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	/* DO PHY ByPass */
	if (exynos_pcie->pmu_phy_isolation) {
		exynos_pmu_update(exynos_pcie->pmu_phy_isolation,
				  PCIE_PHY_CONTROL_MASK, 1);
	}

	return 0;
}
#endif

static const struct dev_pm_ops exynos_pcie_rc_pm_ops = {
	.suspend_noirq	= exynos_pcie_rc_suspend_noirq,
	.resume_noirq	= exynos_pcie_rc_resume_noirq,
	.prepare	= exynos_pcie_suspend_prepare,
	.complete	= exynos_pcie_resume_complete,
	SET_RUNTIME_PM_OPS(exynos_pcie_runtime_suspend,
			   exynos_pcie_runtime_resume, NULL)
};

static const struct exynos_pcie_ep_cfg samsung_cp_cfg = {
	.msi_init = exynos_pcie_rc_msi_init_cp,
	.set_ia_code = exynos_pcie_rc_ia,
	.udelay_after_perst = 18000,
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_32US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL_0US,
	.l1ss_ep_specific_cfg = exynos_pcie_l1ss_samsung,
	.modify_read_val = exynos_pcie_rc_modify_read_val_cp,
	.history_dbg_ctrl = exynos_pcie_rc_history_ctrl_phy_essen,
	.enable_eq = 1,
};
static const struct exynos_pcie_ep_cfg qc_wifi_cfg = {
	.msi_init = exynos_pcie_rc_msi_init_default,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.udelay_before_perst = 10000,
	.udelay_after_perst = 1000,
	.linkdn_callback_direct = 1,
	.no_speed_check = 1,
	.linkup_max_count = 3,
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_70US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL,
	.history_dbg_ctrl = exynos_pcie_rc_history_ctrl_phy_essen,
};
static const struct exynos_pcie_ep_cfg bcm_wifi_cfg = {
	.msi_init = exynos_pcie_rc_msi_init_default,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.udelay_after_perst = 20000, /* for bcm4389(Default : 18msec) */
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_70US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL,
	.l1ss_ep_specific_cfg = exynos_pcie_l1ss_bcm,
	.history_dbg_ctrl = exynos_pcie_rc_history_ctrl_phy_essen,
};

static const struct exynos_pcie_ep_cfg samsung_wifi_cfg = {
	.msi_init = exynos_pcie_rc_msi_init_default,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.udelay_after_perst = 18000,
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_32US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL_0US,
	.l1ss_ep_specific_cfg = exynos_pcie_l1ss_samsung,
	.history_dbg_ctrl = exynos_pcie_rc_history_ctrl_phy_essen,
	.modify_read_val = exynos_pcie_rc_modify_read_val_samsung_wifi,
};

static const struct of_device_id exynos_pcie_rc_of_match[] = {
	{ .compatible = "exynos-pcie-rc,cp_ss", .data = &samsung_cp_cfg },
	{ .compatible = "exynos-pcie-rc,wifi_qc", .data = &qc_wifi_cfg },
	{ .compatible = "exynos-pcie-rc,wifi_bcm", .data = &bcm_wifi_cfg },
	{ .compatible = "exynos-pcie-rc,wifi_ss", .data = &samsung_wifi_cfg },
	{},
};

static struct platform_driver exynos_pcie_rc_driver = {
	.probe		= exynos_pcie_rc_probe,
	.remove		= exynos_pcie_rc_remove,
	.shutdown	= exynos_pcie_rc_shutdown,
	.driver = {
		.name		= "exynos-pcie-rc",
		.owner		= THIS_MODULE,
		.of_match_table = exynos_pcie_rc_of_match,
		.pm		= &exynos_pcie_rc_pm_ops,
	},
};
module_platform_driver(exynos_pcie_rc_driver);

MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_AUTHOR("Seungo Jang <seungo.jang@samsung.com>");
MODULE_AUTHOR("Kwangho Kim <kwangho2.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe RootComplex controller driver");
MODULE_LICENSE("GPL v2");
