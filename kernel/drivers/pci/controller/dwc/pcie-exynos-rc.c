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
#include "pcie-exynos-rc.h"

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#include <soc/samsung/shm_ipc.h>
#endif	/* CONFIG_LINK_DEVICE_PCIE */

#include <soc/samsung/exynos-pcie-iommu-exp.h>
#include <soc/samsung/exynos/exynos-soc.h>

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

/* Reserved memory reference code
#include <linux/of_reserved_mem.h>

static int __init exynos_pcie_rmem_setup(struct reserved_mem *rmem)
{
	pr_err("%s: Reserved memory for pcie: addr=%lx, size=%lx\n",
			__func__, rmem->base, rmem->size);

	return 0;
}
RESERVEDMEM_OF_DECLARE(seclog_mem, "exynos,pcie_rmem", exynos_pcie_rmem_setup);

static void exynos_pcie_save_rmem(struct exynos_pcie *exynos_pcie)
{
	int ret = 0;
	struct device_node *np;
	struct reserved_mem *rmem;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct device *dev = pci->dev;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		pcie_err("%s: there is no memory-region node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		pcie_err("%s: no such rmem node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	exynos_pcie->rmem_msi_name = (char *)rmem->name;
	exynos_pcie->rmem_msi_base = rmem->base;
	exynos_pcie->rmem_msi_size = rmem->size;

	pcie_info("%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);
	pcie_info("%s: success init rmem\n", __func__);
out:
	return;
}
*/

static phys_addr_t get_msi_base(struct exynos_pcie *exynos_pcie)
{
	pcie_info("%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);

	return exynos_pcie->rmem_msi_base;
}

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
	ret = pcie_iommu_map(dev_addr, dev_addr, size, 0, ch_num);
	if (ret != 0) {
		pr_err("DMA map - Can't map PCIe SysMMU table!!!\n");

		return 0;
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

void exynos_pcie_set_perst(int ch_num, bool on)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	pcie_info("%s: force settig for abnormal state\n", __func__);
	if (on) {
		gpio_set_value(exynos_pcie->perst_gpio, 1);
		pcie_info("%s: Set PERST to HIGH, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	} else {
		gpio_set_value(exynos_pcie->perst_gpio, 0);
		pcie_info("%s: Set PERST to LOW, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	}
}
EXPORT_SYMBOL(exynos_pcie_set_perst);

void exynos_pcie_set_perst_gpio(int ch_num, bool on)
{
	exynos_pcie_set_perst(ch_num, on);
}
EXPORT_SYMBOL(exynos_pcie_set_perst_gpio);

void exynos_pcie_set_ready_cto_recovery(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;

	pcie_info("[%s] +++\n", __func__);

	disable_irq(pp->irq);

	exynos_pcie_set_perst_gpio(ch_num, 0);

	/* LTSSM disable */
	pcie_info("[%s] LTSSM disable\n", __func__);
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
			PCIE_APP_LTSSM_ENABLE);
}
EXPORT_SYMBOL(exynos_pcie_set_ready_cto_recovery);

int exynos_pcie_get_irq_num(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;

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
	/* remove orgin code for btl */
	/* exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
				entry->res->start + pp->mem_size - 1); */
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
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	int ret;
	struct resource_entry *tmp = NULL, *entry = NULL;

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

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15)
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
	int ret;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	ret = pci_generic_config_read(bus, devfn, where, size, val);

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
	struct dw_pcie_rp *pp = &pci->pp;

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

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15)
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
			pcie_info("msi_irq_in_use : Vector[%d] : 0x%x\n",
				  ctrl, val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &mask_val);
		mask_val &= ~(val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, mask_val);
		if (val)
			pcie_err("%s: MSI Enable 0x%x, MSI Mask 0x%x\n", __func__,
				 val, mask_val);
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
		pcie_info("Enable cache coherency(IOCC).\n");

		/* set PCIe Axcache[1] = 1 */
		exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, 0x10101010);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, &val);

		/* set PCIe Shareability */
		regmap_update_bits(exynos_pcie->sysreg,
				exynos_pcie->sysreg_sharability,
				PCIE_SYSREG_HSI1_SHARABLE_MASK << 0,
				PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 0);
	} else {
		pcie_info("Disable cache coherency(IOCC).\n");
		/* clear PCIe Axcache[1] = 1 */
		exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, 0x0);

		/* clear PCIe Shareability */
		regmap_update_bits(exynos_pcie->sysreg,
				exynos_pcie->sysreg_sharability,
				PCIE_SYSREG_HSI1_SHARABLE_MASK << 0,
				PCIE_SYSREG_HSI1_SHARABLE_DISABLE << 0);
	}

	exynos_pcie_rc_rd_own_conf(pp, PCIE_PORT_COHERENCY_CTR_3, 4, &val);
	pcie_info("%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
	regmap_read(exynos_pcie->sysreg, exynos_pcie->sysreg_sharability, &val);
	pcie_info("%s: PCIe Shareability = 0x%x.\n", __func__, val);
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

	exynos_pcie->pmureg = syscon_regmap_lookup_by_phandle(np,
					"samsung,pmu-phandle");
	if (IS_ERR(exynos_pcie->pmureg)) {
		pcie_err("syscon regmap lookup failed.\n");
		return PTR_ERR(exynos_pcie->pmureg);
	}

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

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->elbi_base)) {
		ret = PTR_ERR(exynos_pcie->elbi_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "soc");
	exynos_pcie->soc_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->soc_base)) {
		ret = PTR_ERR(exynos_pcie->soc_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phyudbg");
	exynos_pcie->phyudbg_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phyudbg_base)) {
		ret = PTR_ERR(exynos_pcie->phyudbg_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	exynos_pcie->rc_dbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->rc_dbi_base)) {
		ret = PTR_ERR(exynos_pcie->rc_dbi_base);
		return ret;
	}

	pci->dbi_base = exynos_pcie->rc_dbi_base;

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
	exynos_pcie->phy_pcs_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_pcs_base)) {
		ret = PTR_ERR(exynos_pcie->phy_pcs_base);
		return ret;
	}

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

	pcie_info("## %s PCIe INTERRUPT ##\n", enable ? "ENABLE" : "DISABLE");

	if (enable) {
		/* Interrupt clear before enable */
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ0);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ0);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ1);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ1);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2);

		/* enable INTX interrupt */
		val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
			IRQ_INTC_ASSERT | IRQ_INTD_ASSERT;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ0_EN);

		/* enable IRQ1 interrupt - only LINKDOWN */
		exynos_elbi_write(exynos_pcie, 0x400, PCIE_IRQ1_EN);
		/* exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_IRQ1_EN); */

		/* enable IRQ2 interrupt */
		val = IRQ_RADM_CPL_TIMEOUT;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);
	} else {
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ0_EN);
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ1_EN);
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ2_EN);
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

	exynos_pcie_dbg_print_msi_register(exynos_pcie);
}
EXPORT_SYMBOL(exynos_pcie_rc_print_msi_register);

void exynos_pcie_rc_register_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	exynos_pcie_dbg_print_link_history(exynos_pcie);
	exynos_pcie_dbg_register_dump(exynos_pcie);
}
EXPORT_SYMBOL(exynos_pcie_rc_register_dump);

void exynos_pcie_rc_all_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct device *dev = pci->dev;

	pm_runtime_get_sync(dev);
	exynos_pcie_dbg_print_link_history(exynos_pcie);
	exynos_pcie_dbg_dump_link_down_status(exynos_pcie);
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

static void exynos_pcie_rc_assert_phy_reset(struct exynos_pcie *exynos_pcie)
{
	if (exynos_pcie->phy->phy_ops.phy_config != NULL)
		exynos_pcie->phy->phy_ops.phy_config(exynos_pcie, exynos_pcie->ch_num);

	/* Configuration for IA */
	if (exynos_pcie->use_ia && exynos_pcie->ep_cfg->set_ia_code)
		exynos_pcie->ep_cfg->set_ia_code(exynos_pcie);
	else
		pcie_info("[%s] Not support I/A!!!\n", __func__);
}

static void exynos_pcie_rc_resumed_phydown(struct exynos_pcie *exynos_pcie)
{
	/* SKIP PHY configuration in resumed_phydown.
	 * exynos_pcie_rc_assert_phy_reset(exynos_pcie);
	 */

	/* phy all power down */
	if (exynos_pcie->phy->phy_ops.phy_all_pwrdn != NULL)
		exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
}

static void exynos_pcie_setup_rc(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	u32 pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
	u32 val;

	/* enable writing to DBI read-only registers */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MISC_CONTROL_1_OFF, 4, &val);
	val |= PCIE_DBI_RO_WR_EN;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MISC_CONTROL_1_OFF, 4, val);

	/* change vendor ID and device ID for PCIe */
	exynos_pcie_rc_wr_own_conf(pp, PCI_VENDOR_ID, 2, PCI_VENDOR_ID_SAMSUNG);
	exynos_pcie_rc_wr_own_conf(pp, PCI_DEVICE_ID, 2,
				   PCI_DEVICE_ID_EXYNOS + exynos_pcie->ch_num);

	/* set L1 exit latency & show max speed, max link width reset value */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, &val);
	val &= ~(PCI_EXP_LNKCAP_L1EL);
	val |= PCI_EXP_LNKCAP_L1EL_64USEC;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, &val);
	pcie_dbg("PCIE_CAP_LINK_CAPABILITIES_REG(0x7c: 0x%x)\n", val);

	/* set auxiliary clock frequency: 76(0x4C)MHz */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_AUX_CLK_FREQ, 4,
				   PORT_AUX_CLK_FREQ_76MHZ);

	/* set duration of L1.2 & L1.2.Entry */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_PORT_L1_SUBSTATES, 4,
				   PORT_L1_SUBSTATES_VAL);

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
		pcie_info("%s: Set supported payload size : %dbyte\n",
			  __func__, 128 << val);
		pcie_set_mps(exynos_pcie->pci_dev, 128 << val);
	}

	/* Need EQ enable at ROOT
	if (exynos_pcie->max_link_speed >= LINK_SPEED_GEN3) {
		exynos_pcie_rc_wr_own_conf(pp, 0x890, 4, 0x12000);
	}
	*/

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

static irqreturn_t exynos_pcie_rc_irq_handler(int irq, void *arg)
{
	struct dw_pcie_rp *pp = arg;
	u32 val_irq0, val_irq1, val_irq2;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* handle IRQ0 interrupt */
	val_irq0 = exynos_elbi_read(exynos_pcie, PCIE_IRQ0);
	exynos_elbi_write(exynos_pcie, val_irq0, PCIE_IRQ0);

	/* handle IRQ1 interrupt */
	val_irq1 = exynos_elbi_read(exynos_pcie, PCIE_IRQ1);
	exynos_elbi_write(exynos_pcie, val_irq1, PCIE_IRQ1);

	/* handle IRQ2 interrupt */
	val_irq2 = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
	exynos_elbi_write(exynos_pcie, val_irq2, PCIE_IRQ2);

	if (val_irq1 & IRQ_LINK_DOWN) {
		pcie_info("!!! PCIE LINK DOWN !!!\n");
		pcie_info("!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);

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

	if (val_irq2 & IRQ_RADM_CPL_TIMEOUT) {
		pcie_info("!!!PCIE_CPL_TIMEOUT (PCIE_IRQ2: 0x%x)!!!\n", val_irq2);
		pcie_info("!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);

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

	if (val_irq2 & IRQ_MSI_RISING_ASSERT) {
		int ctrl, num_ctrls;
		num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

		if (exynos_pcie->phy->dbg_ops != NULL)
			exynos_pcie->phy->dbg_ops(exynos_pcie, MSI_HNDLR);

		dw_handle_msi_irq(pp);

		/* Mask & Clear MSI to pend MSI interrupt.
		 * After clearing IRQ_PULSE, MSI interrupt can be ignored if
		 * lower MSI status bit is set while processing upper bit.
		 * Through the Mask/Unmask, ignored interrupts will be pended.
		 */
		for (ctrl = 0; ctrl < num_ctrls; ctrl++) {
			exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK +
					(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &val_irq2);
			if (val_irq2 == 0xffffffff)
				continue;
			exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
					(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, 0xffffffff);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
					(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, val_irq2);
		}
	}

	return IRQ_HANDLED;
}

/* MSI init function for shared memory */
static int exynos_pcie_rc_msi_init_shdmem(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	phys_addr_t msi_addr_from_dt = 0;
	u32 val;

	msi_addr_from_dt = get_msi_base(exynos_pcie);
	pp->msi_data = msi_addr_from_dt;
	pcie_dbg("%s: msi_addr_from_dt: 0x%llx, pp->msi_data: 0x%llx\n",
			__func__, msi_addr_from_dt, pp->msi_data);

	/* program the msi_data */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
			lower_32_bits(pp->msi_data));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
			upper_32_bits(pp->msi_data));

	/* enable MSI interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	exynos_msi_enable(exynos_pcie);

	return 0;
}

static int exynos_pcie_rc_msi_init_default(struct exynos_pcie *exynos_pcie)
{
	u32 val;

	/* enable MSI interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	exynos_msi_enable(exynos_pcie);

	return 0;
}

static int exynos_pcie_rc_msi_init_cp(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie_rp *pp = &exynos_pcie->pci->pp;
	u32 val;
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	unsigned long msi_addr_from_dt;
#endif
	/*
	 * The following code is added to avoid duplicated allocation.
	 */
	if (!exynos_pcie->is_ep_scaned) {
		pcie_info("%s: allocate MSI data\n", __func__);

		if (exynos_pcie->ep_pci_bus == NULL)
			exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
		exynos_pcie->ep_pci_dev = exynos_pcie_get_pci_dev(exynos_pcie);

		exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0,
				exynos_pcie->ep_pci_dev->msi_cap, 4, &val);
		pcie_info("%s: EP support %d-bit MSI address (0x%x)\n", __func__,
				(val & (PCI_MSI_FLAGS_64BIT << 16)) ? 64 : 32, val);

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

	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	val |= IRQ_RADM_CPL_TIMEOUT;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	/* Enable MSI interrupt after PCIe reset */
	val = (u32)(*pp->msi_irq_in_use);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);

	exynos_pcie_enable_sep_msi(exynos_pcie);

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	pcie_info("MSI INIT check INTR0 ENABLE, 0x%x: 0x%x\n", PCIE_MSI_INTR0_ENABLE, val);
	if (val != 0xf1) {
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, 0xf1);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	}
#endif
	pcie_info("%s: MSI INIT END, 0x%x: 0x%x\n", __func__, PCIE_MSI_INTR0_ENABLE, val);

	return 0;
}

static void exynos_pcie_rc_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	int count = 0;
	int retry_cnt = 0;
	u32 val;

	/* L1.2 enable check */
	pcie_dbg("Current PM state(PCS + 0x188) : 0x%x\n",
			readl(exynos_pcie->phy_pcs_base + 0x188));

	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
				   PCI_EXP_LNKCTL, 4, &val);
	pcie_dbg("DBI Link Control Register: 0x%x\n", val);

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	pcie_dbg("%s: link state:%x\n", __func__, val);
	if (!(val >= 0x0d && val <= 0x14)) {
		pcie_info("%s, pcie link is not up\n", __func__);
		return;
	}

	exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val &= ~APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

retry_pme_turnoff:
	if (retry_cnt > 0) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		pcie_err("Current LTSSM State is 0x%x with retry_cnt =%d.\n",
				val, retry_cnt);
	}

	exynos_elbi_write(exynos_pcie, 0x1, XMIT_PME_TURNOFF);

	while (count < MAX_L2_TIMEOUT) {
		if ((exynos_elbi_read(exynos_pcie, PCIE_IRQ0)
						& IRQ_RADM_PM_TO_ACK)) {
			pcie_dbg("ack message is ok\n");
			udelay(10);
			break;
		}

		udelay(10);
		count++;
	}
	if (count >= MAX_L2_TIMEOUT)
		pcie_err("cannot receive ack message from EP\n");

	exynos_elbi_write(exynos_pcie, 0x0, XMIT_PME_TURNOFF);

	count = 0;
	do {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			pcie_dbg("received Enter_L23_READY DLLP packet\n");
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
static void exynos_pcie_rc_qch_control(struct exynos_pcie *exynos_pcie, int enable)
{
	if (enable) {
		exynos_elbi_write(exynos_pcie, 0x577, 0x3A8);
		exynos_elbi_write(exynos_pcie, 0x0, 0xD5C);

	} else {
		exynos_elbi_write(exynos_pcie, 0x0, 0x3A8);
		exynos_elbi_write(exynos_pcie, 0x2, 0xD5C);
	}
}

static int exynos_pcie_rc_establish_link(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	u32 val, busdev;
	int count = 0, try_cnt = 0;
	u32 speed_recovery_cnt = 0;
	unsigned long udelay_before_perst, udelay_after_perst;

	udelay_before_perst = exynos_pcie->ep_cfg->udelay_before_perst;
	udelay_after_perst = exynos_pcie->ep_cfg->udelay_after_perst;

retry:
	exynos_pcie_rc_assert_phy_reset(exynos_pcie);

	/* Q-ch disable */
	exynos_pcie_rc_qch_control(exynos_pcie, 0);

	if (udelay_before_perst)
		usleep_range(udelay_before_perst, udelay_before_perst + 2000);

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);

	pcie_info("%s: Set PERST to HIGH, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

	if (udelay_after_perst)
		usleep_range(udelay_after_perst, udelay_after_perst + 2000);
	else /* Use default delay 18~20msec */
		usleep_range(18000, 20000);

	/* NAK enable when AXI pending */
	exynos_elbi_write(exynos_pcie, NACK_ENABLE, PCIE_MSTR_PEND_SEL_NAK);
	pcie_dbg("%s: NACK option enable: 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_MSTR_PEND_SEL_NAK));

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	exynos_pcie_setup_rc(exynos_pcie);

	if (dev_is_dma_coherent(pci->dev))
		exynos_pcie_rc_set_iocc(exynos_pcie, 1);

	pcie_info("D state: %x, %x\n",
			exynos_elbi_read(exynos_pcie, PCIE_PM_DSTATE) & 0x7,
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));

	exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);

	/* Enable history buffer */
	val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
	val &= ~(HISTORY_BUFFER_CONDITION_SEL);
	exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

	exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_STATE_POWER_S);

	exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_STATE_POWER_M);

	val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
	val |= HISTORY_BUFFER_ENABLE;
	exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

	/* assert LTSSM enable */
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_ENABLE,
				PCIE_APP_LTSSM_ENABLE);
	count = 0;
	while (count < MAX_TIMEOUT) {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		if (val == 0x11)
			break;

		count++;

		udelay(10);
	}

	if (count >= MAX_TIMEOUT) {
		try_cnt++;

		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		pcie_err("%s: Link is not up, try count: %d, linksts: %s(0x%x)\n",
			__func__, try_cnt, LINK_STATE_DISP(val), val);

		if (try_cnt < 10) {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			pcie_info("%s: Set PERST to LOW, gpio val = %d\n",
				__func__,
				gpio_get_value(exynos_pcie->perst_gpio));

			/* LTSSM disable */
			exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
					  PCIE_APP_LTSSM_ENABLE);

			goto retry;
		} else {
			exynos_pcie_dbg_print_link_history(exynos_pcie);
			exynos_pcie_dbg_register_dump(exynos_pcie);

			return -EPIPE;
		}
	} else {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		pcie_dbg("%s: %s(0x%x)\n", __func__,
				LINK_STATE_DISP(val), val);

		exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
					   PCI_EXP_LNKCTL, 4, &val);
		val = (val >> 16) & 0xf;
		pcie_dbg("Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		speed_recovery_cnt = 0;
		while (speed_recovery_cnt < MAX_TIMEOUT_GEN1_GEN2) {
			exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
						   PCI_EXP_LNKCTL, 4, &val);
			val = (val >> 16) & 0xf;
			if (val == exynos_pcie->max_link_speed) {
				pcie_dbg("%s: [cnt:%d] Current Link Speed is GEN%d (MAX GEN%d)\n",
						__func__,
						speed_recovery_cnt, val, exynos_pcie->max_link_speed);
				break;
			}

			speed_recovery_cnt++;

			udelay(10);
		}

		/* check link training result(speed) */
		if (val < exynos_pcie->max_link_speed) {
			try_cnt++;

			pcie_err("%s: [cnt:%d] Link is up. But not GEN%d speed, try count: %d\n",
					__func__, speed_recovery_cnt, exynos_pcie->max_link_speed, try_cnt);

			if (try_cnt < 10) {
				gpio_set_value(exynos_pcie->perst_gpio, 0);
				pcie_info("%s: Set PERST to LOW, gpio val = %d\n",
						__func__,
						gpio_get_value(exynos_pcie->perst_gpio));

				/* LTSSM disable */
				exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
						PCIE_APP_LTSSM_ENABLE);

				goto retry;
			} else {
				pcie_info("[cnt:%d] Current Link Speed is GEN%d (MAX GEN%d)\n",
						speed_recovery_cnt, val, exynos_pcie->max_link_speed);
			}
		}

		/* one more check L0 state for Gen3 recovery */
		count = 0;
		pcie_dbg("%s: check L0 state for Gen3 recovery\n", __func__);
		while (count < MAX_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
			if (val == 0x11)
				break;

			count++;

			udelay(10);
		}

		exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
					   PCI_EXP_LNKCTL, 4, &val);
		val = (val >> 16) & 0xf;
		pcie_info("Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		/* Q-ch enable */
		exynos_pcie_rc_qch_control(exynos_pcie, 1);

		exynos_pcie_rc_enable_interrupts(exynos_pcie, 1);

		/* setup ATU for cfg/mem outbound */
		busdev = PCIE_ATU_BUS(1) | PCIE_ATU_DEV(0) | PCIE_ATU_FUNC(0);
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
		exynos_pcie_rc_prog_viewport_mem_outbound(exynos_pcie);

	}

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

int exynos_pcie_rc_poweron(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct dw_pcie_rp *pp;
	struct device *dev;
	int ret;
	struct irq_desc *exynos_pcie_desc;

	if (exynos_pcie->probe_done != 1) {
		pr_err("%s: RC%d has not been successfully probed yet\n", __func__, ch_num);
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

		if (exynos_pcie->phy->dbg_ops != NULL)
			exynos_pcie->phy->dbg_ops(exynos_pcie, PWRON);

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
			exynos_pcie_mem_copy_epdev(exynos_pcie);

			exynos_pcie->ep_pci_dev->dma_mask = DMA_BIT_MASK(36);
			exynos_pcie->ep_pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
			dma_set_seg_boundary(&(exynos_pcie->ep_pci_dev->dev), DMA_BIT_MASK(36));

			if (exynos_pcie->ep_cfg->set_dma_ops) {
				exynos_pcie->ep_cfg->set_dma_ops(&exynos_pcie->ep_pci_dev->dev);
				pcie_dbg("DMA operations are changed to support SysMMU\n");
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

	pcie_info("%s, end of poweron, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	return 0;

poweron_fail:
	exynos_pcie->state = STATE_LINK_UP;
	exynos_pcie->cpl_timeout_recovery = 0;
	exynos_pcie_rc_poweroff(exynos_pcie->ch_num);

	return -EPIPE;
}
EXPORT_SYMBOL(exynos_pcie_rc_poweron);

void exynos_pcie_rc_poweroff(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	struct device *dev = pci->dev;
	unsigned long flags;
	u32 val;

	pcie_info("%s, start of poweroff, pcie state: %d\n", __func__,
		  exynos_pcie->state);

	if (exynos_pcie->phy->dbg_ops != NULL)
		exynos_pcie->phy->dbg_ops(exynos_pcie,PWROFF);

	if (exynos_pcie->state == STATE_LINK_UP ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		disable_irq(pp->irq);

		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		exynos_pcie_rc_send_pme_turn_off(exynos_pcie);

		exynos_pcie_soc_specific_ctrl(exynos_pcie, OPR_STATE_POWER_OFF);

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
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		val &= ~HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		gpio_set_value(exynos_pcie->perst_gpio, 0);
		pcie_info("%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

		if (exynos_pcie->ep_cfg->mdelay_after_perst_low) {
			pcie_dbg("%s, It needs delay after setting perst to low\n", __func__);
			mdelay(exynos_pcie->ep_cfg->mdelay_after_perst_low);
		}

		/* LTSSM disable */
		exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
				PCIE_APP_LTSSM_ENABLE);

		spin_lock(&exynos_pcie->reg_lock);
		/* force SOFT_PWR_RESET */
		pcie_dbg("%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));
		val = exynos_elbi_read(exynos_pcie, PCIE_SOFT_RESET);
		val &= ~SOFT_PWR_RESET;
		exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);
		udelay(20);
		val |= SOFT_PWR_RESET;
		exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);
		spin_unlock(&exynos_pcie->reg_lock);

		/* phy all power down */
		if (exynos_pcie->phy->phy_ops.phy_all_pwrdn != NULL) {
			exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie,
					exynos_pcie->ch_num);
		}

		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

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

	pcie_info("%s, end of poweroff, pcie state: %d\n",  __func__,
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

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(domain_num, 1);

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
	unsigned long flags;
	struct pci_dev *ep_dev, *rc_dev;
	u32 tpoweron_time = exynos_pcie->ep_cfg->tpoweron_time;
	u32 ltr_threshold = exynos_pcie->ep_cfg->ltr_threshold;
	u32 common_restore_time = exynos_pcie->ep_cfg->common_restore_time;
	struct pci_bus *ep_bus;

	pcie_info("%s:L1SS_START(l1ss_ctrl_id_state=%#x, id=%#x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	if (exynos_pcie->state != STATE_LINK_UP) {
		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
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

		if (ep_dev == NULL) {
			pcie_err("Failed to set L1SS %s (ep_dev == NULL)!!!\n",
					enable ? "ENABLE" : "FALSE");
			return -EINVAL;
		}

		if (ep_dev->l1ss == 0x0 || ep_dev->pcie_cap == 0x0) {
			pcie_err("Can't set L1ss : No Capability Pointers.\n");
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
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
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

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
	*/

	pcie_info("LTSSM: %#08x, PM_STATE = %#08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
			exynos_phy_pcs_read(exynos_pcie, 0x188));
	pcie_info("%s:L1SS_END(l1ss_ctrl_id_state=%#x, id=%#x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	return 0;
}

int exynos_pcie_l1ss_ctrl(int enable, int id, int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	return exynos_pcie_set_l1ss(exynos_pcie, enable, id);
}
EXPORT_SYMBOL(exynos_pcie_l1ss_ctrl);

int exynos_pcie_l1_exit(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	u32 count = 0, ret = 0, val = 0;
	unsigned long flags;

	spin_lock_irqsave(&exynos_pcie->pcie_l1_exit_lock, flags);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* Set s/w L1 exit mode */
		exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val &= ~APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

		/* Max timeout = 3ms (300 * 10us) */
		while (count < MAX_L1_EXIT_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x1f;
			if (val == 0x11)
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

		/* Set h/w L1 exit mode */
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val |= APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
		exynos_elbi_write(exynos_pcie, 0x0, PCIE_APP_REQ_EXIT_L1);
	} else {
		/* remove too much print */
		/* pcie_err("%s: skip!!! l1.2 is already disabled(id = 0x%x)\n",
		   __func__, exynos_pcie->l1ss_ctrl_id_state); */
	}

	spin_unlock_irqrestore(&exynos_pcie->pcie_l1_exit_lock, flags);

	return ret;

}
EXPORT_SYMBOL(exynos_pcie_l1_exit);

int exynos_pcie_host_v1_poweron(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pcie_err("### [%s] requested spd: GEN%d\n", __func__, spd);
	exynos_pcie->max_link_speed = spd;

	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweron);

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

	pcie_info("## %s(state: %d)\n", __func__, exynos_pcie->state);

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

	return exynos_pcie->cpl_timeout_recovery;
}
EXPORT_SYMBOL(exynos_pcie_rc_get_cpl_timeout_state);

void exynos_pcie_rc_set_cpl_timeout_state(int ch_num, bool recovery)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

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

	if (!exynos_pcie) {
		pcie_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return 0;

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x14) {
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

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
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

	/* We can't set affinity here after kernel6.1.(spin_lock is added)
	 *  - Use the function that we exported(exynos_pcie_rc_set_affinity()).
	 * irq_set_affinity_hint(irq, mask);
	 */

	return IRQ_SET_MASK_OK;
}

static int exynos_pcie_msi_set_wake(struct irq_data *irq_data, unsigned int enable)
{
	struct dw_pcie_rp *pp = irq_data->parent_data->domain->host_data;
	struct irq_data *pdata = irq_data->parent_data;
	struct dw_pcie *pci;
	struct exynos_pcie *exynos_pcie;
	unsigned int ctrl;
	int irq, ret = 0;

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
	u32 val;
	int i;

	pr_info("Enable Separated MSI IRQs(Disable MSI RISING EDGE).\n");
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val &= ~IRQ_MSI_CTRL_EN_RISING_EDG;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

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
	struct separated_msi_vector *msi_vec =
			&sep_msi_vec[exynos_pcie->ch_num][vec_num];

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
	struct separated_msi_vector *msi_vec =
			&sep_msi_vec[exynos_pcie->ch_num][vec_num];

	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_STATUS +
			(vec_num * MSI_REG_CTRL_BLOCK_SIZE), 4, 0x1);
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
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	int i, ret;

	for (i = 1; i < MAX_MSI_CTRLS; i++) {
		if (!sep_msi_vec[ch_num][i].is_used)
			break;
	}

	if (i == MAX_MSI_CTRLS) {
		pr_info("PCIe Ch%d : There is no empty MSI vector!\n", ch_num);
		return -EBUSY;
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
	unsigned int val;

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	if (!strncmp(itmon_info->dest, "HSI1", 4) || !strncmp(itmon_info->master, "HSI1", 4)) {
		regmap_read(exynos_pcie->pmureg, exynos_pcie->pmu_phy_isolation, &val);
		pcie_info("### PMU PHY Isolation : %#x(LinkState:%d)\n", val, exynos_pcie->state);
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
			pcie_err("WARN : MSIX is not defined in DT(vec:%d)!!\n", i);
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
	dma_addr_t temp_addr;
	struct exynos_pcie_register_event temp_event;
	int irq_num;
	u32 restore_ip_ver = 0;

	temp_addr = get_msi_base(exynos_pcie);
	exynos_pcie_rc_set_outbound_atu(0, 0x27200000, 0x0, SZ_1M);
	exynos_pcie_rc_print_msi_register(exynos_pcie->ch_num);
	exynos_pcie_rc_msi_init_shdmem(exynos_pcie);
	exynos_pcie_rc_msi_init_cp(exynos_pcie);
	restore_ip_ver = exynos_pcie->ip_ver;
	exynos_pcie->ip_ver = 0x990000;
	exynos_pcie->ip_ver = restore_ip_ver;

	/* Check related L1ss functions */
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie->l1ss_ctrl_id_state = PCIE_L1SS_CTRL_TEST;
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie->l1ss_ctrl_id_state = 0;

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

/* EXYNOS specific control and W/A code */
static void exynos_pcie_soc_specific_ctrl(struct exynos_pcie *exynos_pcie,
					  enum exynos_pcie_opr_state state)
{
	switch (state) {
	case OPR_STATE_PROBE:
		if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x993500) {
			if (exynos_soc_info.main_rev == 1) {
				pcie_info("Exynos9935 EVT1 doesn't need I/A\n");
				exynos_pcie->use_ia = false;
			}
		}
		break;
	case OPR_STATE_POWER_ON:
		break;
	case OPR_STATE_POWER_OFF:
		if (exynos_pcie->ip_ver == 0x993500) { /* for SAMSUNG PHY */
			writel(0xA, exynos_pcie->phy_base + 0x580);
			writel(0x300DE, exynos_pcie->phy_pcs_base + 0x150);
		}

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
	ret = exynos_pcie_rc_phy_init(exynos_pcie);
	if (ret)
		goto probe_fail;

	exynos_pcie_rc_resumed_phydown(exynos_pcie);

	ret = exynos_pcie_rc_make_reg_tb(exynos_pcie);
	if (ret)
		goto probe_fail;

	if (exynos_pcie->ep_cfg->save_rmem)
		exynos_pcie->ep_cfg->save_rmem(exynos_pcie);

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
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_phy_isolation,
			   PCIE_PHY_CONTROL_MASK, 0);

	return 0;
}

static int exynos_pcie_runtime_resume(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	/* DO PHY ByPass */
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_phy_isolation,
			   PCIE_PHY_CONTROL_MASK, 1);

	if (exynos_pcie->pmu_gpio_retention) {
		//HSI1 PCIe GPIO retention release
		regmap_set_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_gpio_retention,
				PCIE_GPIO_RETENTION(exynos_pcie->ch_num));
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
	.udelay_after_perst = 18000,
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_32US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL_0US,
	.l1ss_ep_specific_cfg = exynos_pcie_l1ss_samsung,
};
static const struct exynos_pcie_ep_cfg qc_wifi_cfg = {
	.msi_init = exynos_pcie_rc_msi_init_default,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.udelay_before_perst = 10000,
	.udelay_after_perst = 18000,
	.linkdn_callback_direct = 1,
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_70US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL,
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
};

static const struct exynos_pcie_ep_cfg samsung_wifi_cfg = {
	.msi_init = exynos_pcie_rc_msi_init_default,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.udelay_after_perst = 18000,
	.mdelay_before_perst_low = 10,
	.mdelay_after_perst_low = 20,
	.tpoweron_time = PCI_L1SS_CTL2_TPOWERON_180US,
	.common_restore_time = PCI_L1SS_TCOMMON_32US,
	.ltr_threshold = PCI_L1SS_CTL1_LTR_THRE_VAL_0US,
	.l1ss_ep_specific_cfg = exynos_pcie_l1ss_samsung,
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
