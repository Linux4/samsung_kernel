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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/pm_qos.h>
#include <dt-bindings/pci/pci.h>
#include <linux/exynos-pci-noti.h>
#include <linux/exynos-pci-ctrl.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_fdt.h>
#include <linux/dma-map-ops.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/random.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) /* Need to check after feature enable!!! */
#include <soc/samsung/exynos-itmon.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM) /* Need to check after feature enable!!! */
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos_pm_qos.h>
#endif
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"
#include "pcie-exynos-dbg.h"

#include "pcie-exynos-phycal_common.h"

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#include <soc/samsung/shm_ipc.h>
#endif	/* CONFIG_LINK_DEVICE_PCIE */

#include <soc/samsung/exynos-pcie-iommu-exp.h>
#include <soc/samsung/exynos/exynos-soc.h>

static struct exynos_pcie g_pcie_rc[MAX_RC_NUM];
static struct separated_msi_vector sep_msi_vec[MAX_RC_NUM][PCIE_MAX_SEPA_IRQ_NUM];
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
static struct exynos_pm_qos_request exynos_pcie_int_qos[MAX_RC_NUM];
#endif

static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp);
static int exynos_pcie_rc_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val);
static int exynos_pcie_rc_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val);
static int exynos_pcie_rc_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val);
static int exynos_pcie_rc_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val);
static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				   const struct cpumask *mask, bool force);
static int exynos_pcie_msi_set_wake(struct irq_data *irq_data, unsigned int on);
static void exynos_pcie_msi_irq_enable(struct irq_data *data);
static void exynos_pcie_msi_irq_disable(struct irq_data *data);
static inline void exynos_pcie_enable_sep_msi(struct exynos_pcie *exynos_pcie,
					      struct pcie_port *pp);

static void exynos_pcie_save_rmem(struct exynos_pcie *exynos_pcie)
{
	int ret = 0;
	struct device_node *np;
	struct reserved_mem *rmem;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct device *dev = pci->dev;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		pcie_err(exynos_pcie, "%s: there is no memory-region node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		pcie_err(exynos_pcie, "%s: no such rmem node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	exynos_pcie->rmem_msi_name = (char *)rmem->name;
	exynos_pcie->rmem_msi_base = rmem->base;
	exynos_pcie->rmem_msi_size = rmem->size;

	pcie_info(exynos_pcie, "%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);
	pcie_info(exynos_pcie, "%s: success init rmem\n", __func__);
out:
	return;
}

#define to_pci_dev_from_dev(dev) container_of((dev), struct pci_dev, dev)

static void exynos_pcie_mem_copy_epdev(int ch_num, struct device *epdev)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pcie_info(exynos_pcie, "[%s] COPY ep_dev to PCIe memory struct\n", __func__);
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

void exynos_pcie_set_dma_ops(struct device *dev)
{
	set_dma_ops(dev, &exynos_pcie_dma_ops);
}

static phys_addr_t get_msi_base(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pcie_info(exynos_pcie, "%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);

	return exynos_pcie->rmem_msi_base;
}

void exynos_pcie_set_perst(int ch_num, bool on)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	pcie_info(exynos_pcie, "%s: force settig for abnormal state\n", __func__);
	if (on) {
		gpio_set_value(exynos_pcie->perst_gpio, 1);
		pcie_info(exynos_pcie, "%s: Set PERST to HIGH, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	} else {
		gpio_set_value(exynos_pcie->perst_gpio, 0);
		pcie_info(exynos_pcie, "%s: Set PERST to LOW, gpio val = %d\n",
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
	struct pcie_port *pp = &pci->pp;

	pcie_info(exynos_pcie, "[%s] +++\n", __func__);

	disable_irq(pp->irq);

	exynos_pcie_set_perst_gpio(ch_num, 0);

	/* LTSSM disable */
	pcie_info(exynos_pcie, "[%s] LTSSM disable\n", __func__);
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
			PCIE_APP_LTSSM_ENABLE);
}
EXPORT_SYMBOL(exynos_pcie_set_ready_cto_recovery);

int exynos_pcie_get_irq_num(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	pcie_info(exynos_pcie, "%s: pp->irq = %d\n", __func__, pp->irq);
	return pp->irq;
}
EXPORT_SYMBOL(exynos_pcie_get_irq_num);

static int exynos_pcie_rc_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
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

static int exynos_pcie_rc_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
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

static void exynos_pcie_rc_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev)
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
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND0, 4, EXYNOS_PCIE_ATU_TYPE_CFG0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND0, 4, EXYNOS_PCIE_ATU_ENABLE);
	exynos_pcie->current_busdev = busdev;
}

static void exynos_pcie_rc_prog_viewport_mem_outbound(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct resource_entry *tmp = NULL, *entry = NULL;

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;

	if (unlikely(entry == NULL)) {
		pcie_err(exynos_pcie, "entry is NULL\n");
		return;
	}

	/* Program viewport 0 : OUTBOUND : MEM */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND1, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND1, 4, entry->res->start);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND1, 4,
					(entry->res->start >> 32));
	/* remove orgin code for btl */
	/* exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
				entry->res->start + pp->mem_size - 1); */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
			entry->res->start + resource_size(entry->res) - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND1, 4, entry->res->start - entry->offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND1, 4,
					upper_32_bits(entry->res->start - entry->offset));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND1, 4, EXYNOS_PCIE_ATU_ENABLE);
}

int exynos_pcie_rc_set_bar(int ch_num, u32 bar_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_dev *ep_pci_dev;
	u32 val;

	pcie_info(exynos_pcie, "%s: +++\n", __func__);

	if (exynos_pcie->state == STATE_LINK_UP) {
		ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	} else {
		pcie_info(exynos_pcie, "%s: PCIe link is not up\n", __func__);
		return -EPIPE;
	}

	/* EP BAR setup */
	ep_pci_dev->resource[bar_num].start = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset;
	ep_pci_dev->resource[bar_num].end = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset + exynos_pcie->btl_size;
	ep_pci_dev->resource[bar_num].flags = 0x82000000;
	if (pci_assign_resource(ep_pci_dev, bar_num) != 0)
		pr_warn("%s: failed to assign PCI resource for %i\n", __func__, bar_num);

	pci_read_config_dword(ep_pci_dev, PCI_BASE_ADDRESS_0 + (bar_num * 0x4), &val);
	pcie_info(exynos_pcie, "%s: Check EP BAR[%d] = 0x%x\n", __func__, bar_num, val);

	pcie_info(exynos_pcie, "%s: ---\n", __func__);
	return 0;
}

int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int ret;
	struct resource_entry *tmp = NULL, *entry = NULL;

	pcie_info(exynos_pcie, "%s: +++\n", __func__);

	exynos_pcie->btl_target_addr = target_addr;
	exynos_pcie->btl_offset = offset;
	exynos_pcie->btl_size = size;

	pcie_info(exynos_pcie, "%s: target_addr = 0x%x, offset = 0x%x, size = 0x%x\n", __func__,
			exynos_pcie->btl_target_addr,
			exynos_pcie->btl_offset,
			exynos_pcie->btl_size);

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;
	if (entry == NULL) {
		pcie_err(exynos_pcie, "entry is NULL\n");
		return -EPIPE;
	}

	/* Only for BTL */
	/* 0x1420_0000 ~ (size -1) */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4,
			entry->res->start + SZ_2M);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4,
			((entry->res->start + SZ_2M) >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4,
			entry->res->start + SZ_2M + exynos_pcie->btl_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4,
			exynos_pcie->btl_target_addr + exynos_pcie->btl_offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, EXYNOS_PCIE_ATU_ENABLE);

	exynos_pcie_dbg_print_oatu_register(exynos_pcie);

	ret = exynos_pcie_rc_set_bar(ch_num, 2);

	pcie_info(exynos_pcie, "%s: ---\n", __func__);

	return ret;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_outbound_atu);

static int exynos_pcie_rc_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val)
{
	int ret;
	u32 busdev;
	void __iomem *va_cfg_base;

	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));
	va_cfg_base = pp->va_cfg0_base;
	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	ret = dw_pcie_read(va_cfg_base + where, size, val);

	return ret;
}

static int exynos_pcie_rc_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{
	int ret;
	u32 busdev;
	void __iomem *va_cfg_base;

	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));

	va_cfg_base = pp->va_cfg0_base;
	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	ret = dw_pcie_write(va_cfg_base + where, size, val);

	return ret;
}

static void __iomem *exynos_pcie_rc_own_conf_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	return dw_pcie_own_conf_map_bus(bus, devfn, where);
}

static int exynos_rc_generic_own_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	struct pcie_port *pp = bus->sysdata;
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
	struct pcie_port *pp = bus->sysdata;
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
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	u32 busdev;

	if (!exynos_pcie_rc_check_link(pci))
		return NULL;

	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));

	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	return pp->va_cfg0_base + where;
}

static int exynos_rc_generic_other_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	struct pcie_port *pp = bus->sysdata;
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
	struct pcie_port *pp = bus->sysdata;
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

u32 exynos_pcie_rc_read_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size)
{
	struct pcie_port *pp = &pci->pp;
	u32 val;

	exynos_pcie_rc_rd_own_conf(pp, reg, size, &val);

	return val;
}

void exynos_pcie_rc_write_dbi(struct dw_pcie *pci, void __iomem *base,
				  u32 reg, size_t size, u32 val)
{
	struct pcie_port *pp = &pci->pp;

	exynos_pcie_rc_wr_own_conf(pp, reg, size, val);
}

static int exynos_pcie_rc_link_up(struct dw_pcie *pci)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (!exynos_pcie->probe_done) {
		pcie_info(exynos_pcie, "Force link-up return in probe time!\n");
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

static void exynos_msi_enable(struct exynos_pcie *exynos_pcie, struct pcie_port *pp)
{
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
			pcie_info(exynos_pcie, "msi_irq_in_use : Vector[%d] : 0x%x\n",
				  ctrl, val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, val);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &mask_val);
		mask_val &= ~(val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, mask_val);
		if (val)
			pcie_err(exynos_pcie, "%s: MSI Enable 0x%x, MSI Mask 0x%x \n", __func__,
				 val, mask_val);
	}

	if (exynos_pcie->separated_msi)
		exynos_pcie_enable_sep_msi(exynos_pcie, pp);
}

static void exynos_msi_setup_irq(msi_alloc_info_t *arg, int retval)
{
	struct device *dev = arg->desc->dev;
	struct irq_domain *irq_domain = dev->msi_domain->parent;
	struct irq_domain *msi_domain = dev->msi_domain;
	struct pcie_port *pp = irq_domain->host_data;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct msi_domain_info *msi_info = msi_domain->host_data;
	int i;

	if (pp == NULL)
		return;

	exynos_msi_enable(exynos_pcie, pp);

	msi_info->chip->irq_set_affinity = exynos_pcie_msi_set_affinity;
	msi_info->chip->irq_set_wake = exynos_pcie_msi_set_wake;
	msi_info->chip->irq_enable = exynos_pcie_msi_irq_enable;
	msi_info->chip->irq_disable = exynos_pcie_msi_irq_disable;

	if (exynos_pcie->separated_msi) {
		struct irq_data *irq_data;
		int virq, mapped_irq, changed_irq = 0;

		virq = irq_find_mapping(pp->irq_domain, 0);
		mapped_irq = pp->irq_domain->mapcount;

		dev_info(dev, "Start virq = %d, Total mapped irq = %d\n",
				virq, mapped_irq);

		for (i = 0; i < pp->num_vectors; i++) {
			irq_data = irq_domain_get_irq_data(pp->irq_domain, virq);
			if (irq_data == NULL) {
				virq++;
				continue;
			}

			dev_info(dev, "Change flow interrupt for virq(%d)\n", virq);
			irq_domain_set_info(pp->irq_domain, virq, irq_data->hwirq,
					pp->msi_irq_chip,
					pp, handle_level_irq,
					NULL, NULL);

			virq++;
			changed_irq++;

			if (changed_irq == mapped_irq)
				break;
		}

	}
}

static void exynos_pcie_rc_set_iocc(struct pcie_port *pp, int enable)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int val;

	pcie_info(exynos_pcie, "IOCC enable.\n");
	if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x992500) {
		if (exynos_pcie->ch_num == 0) {/* PCIe GEN2 channel */
			if (enable) {
				pcie_dbg(exynos_pcie, "enable cache coherency.\n");

				/* set PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);
				exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);

				/* set PCIe Shareability */
				regmap_update_bits(exynos_pcie->sysreg,
						PCIE_SYSREG_HSI1_SHARABILITY_CTRL,
						PCIE_SYSREG_HSI1_SHARABLE_MASK << 14, PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 14);
			} else {
				pcie_dbg(exynos_pcie, "disable cache coherency.\n");
				/* clear PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

				/* clear PCIe Shareability */
				regmap_update_bits(exynos_pcie->sysreg,
						PCIE_SYSREG_HSI1_SHARABILITY_CTRL,
						PCIE_SYSREG_HSI1_SHARABLE_MASK << 14, PCIE_SYSREG_HSI1_SHARABLE_DISABLE << 14);
			}

			exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
			dev_info(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
			regmap_read(exynos_pcie->sysreg, PCIE_SYSREG_HSI1_SHARABILITY_CTRL, &val);
			dev_info(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__, val);

		}
		if (exynos_pcie->ch_num == 1) {/* PCIe GEN3 channel */
			if (enable) {
				pcie_info(exynos_pcie, "enable cache coherency.\n");

				/* set PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);
				exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);

				/* set PCIe Shareability */
				regmap_update_bits(exynos_pcie->sysreg,
						PCIE_SYSREG_HSI1_SHARABILITY_CTRL,
						PCIE_SYSREG_HSI1_SHARABLE_MASK << 18, PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 18);
			} else {
				pcie_info(exynos_pcie, "disable cache coherency.\n");
				/* clear PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

				/* clear PCIe Shareability */
				regmap_update_bits(exynos_pcie->sysreg,
						PCIE_SYSREG_HSI1_SHARABILITY_CTRL,
						PCIE_SYSREG_HSI1_SHARABLE_MASK << 18, PCIE_SYSREG_HSI1_SHARABLE_DISABLE << 18);
			}

			exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
			dev_info(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
			regmap_read(exynos_pcie->sysreg, PCIE_SYSREG_HSI1_SHARABILITY_CTRL, &val);
			dev_info(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__, val);
		}
	} else if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x993500) {
		if (enable) {
			pcie_dbg(exynos_pcie, "enable cache coherency.\n");

			/* set PCIe Axcache[1] = 1 */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);
			exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);

			/* set PCIe Shareability */
			regmap_update_bits(exynos_pcie->sysreg,
					exynos_pcie->sysreg_sharability,
					PCIE_SYSREG_HSI1_SHARABLE_MASK << 0,
					PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 0);
		} else {
			pcie_dbg(exynos_pcie, "disable cache coherency.\n");
			/* clear PCIe Axcache[1] = 1 */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

			/* clear PCIe Shareability */
			regmap_update_bits(exynos_pcie->sysreg,
					exynos_pcie->sysreg_sharability,
					PCIE_SYSREG_HSI1_SHARABLE_MASK << 0,
					PCIE_SYSREG_HSI1_SHARABLE_DISABLE << 0);
		}

		exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
		dev_info(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
		regmap_read(exynos_pcie->sysreg, exynos_pcie->sysreg_sharability, &val);
		dev_info(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__, val);

	} else {
		pcie_dbg(exynos_pcie, "%s: not supported!!!\n", __func__);
	}
}

static int exynos_pcie_rc_parse_dt(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	struct device_node *np = dev->of_node;
	const char *use_sysmmu;
	const char *use_ia;
	const char *use_pcieon_sleep;

	if (of_property_read_u32(np, "ip-ver",
					&exynos_pcie->ip_ver)) {
		pcie_err(exynos_pcie, "Failed to parse the number of ip-ver\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "pmu-phy-isolation",
				&exynos_pcie->pmu_phy_isolation)) {
		pcie_err(exynos_pcie, "Failed to parse PMU PHY iosolation offset\n");
	}

	if (of_property_read_u32(np, "pmu-phy-ctrl",
				&exynos_pcie->pmu_phy_ctrl)) {
		pcie_err(exynos_pcie, "Failed to parse PMU PHY control offset\n");
	}

	if (of_property_read_u32(np, "pmu-gpio-retention",
				&exynos_pcie->pmu_gpio_retention)) {
		pcie_err(exynos_pcie, "Failed to parse PMU GPIO retention offset\n");
	}

	if (of_property_read_u32(np, "sysreg-sharability",
				&exynos_pcie->sysreg_sharability)) {
		pcie_err(exynos_pcie, "Failed to parse SYSREG sharability offset\n");
	}

	if (of_property_read_u32(np, "sysreg-ia0-channel-sel",
				&exynos_pcie->sysreg_ia0_sel)) {
		pcie_err(exynos_pcie, "Failed to parse I/A0 CH sel offset\n");
	}

	if (of_property_read_u32(np, "sysreg-ia1-channel-sel",
				&exynos_pcie->sysreg_ia1_sel)) {
		pcie_err(exynos_pcie, "Failed to parse I/A1 CH sel offset\n");
	}

	if (of_property_read_u32(np, "sysreg-ia2-channel-sel",
				&exynos_pcie->sysreg_ia2_sel)) {
		pcie_err(exynos_pcie, "Failed to parse I/A2 CH sel offset\n");
	}

	if (of_property_read_u32(np, "ep-device-type",
				&exynos_pcie->ep_device_type)) {
		pcie_err(exynos_pcie, "EP device type is NOT defined, device type is 'EP_NO_DEVICE(0)'\n");
		exynos_pcie->ep_device_type = EP_TYPE_NO_DEVICE;
	}

	if (of_property_read_u32(np, "max-link-speed",
				&exynos_pcie->max_link_speed)) {
		pcie_err(exynos_pcie, "MAX Link Speed is NOT defined...(GEN1)\n");
		/* Default Link Speet is GEN1 */
		exynos_pcie->max_link_speed = LINK_SPEED_GEN1;
	}

	if (!of_property_read_string(np, "use-pcieon-sleep", &use_pcieon_sleep)) {
		if (!strcmp(use_pcieon_sleep, "true")) {
			pcie_info(exynos_pcie, "## PCIe use PCIE ON Sleep\n");
			exynos_pcie->use_pcieon_sleep = true;
		} else if (!strcmp(use_pcieon_sleep, "false")) {
			pcie_info(exynos_pcie, "## PCIe don't use PCIE ON Sleep\n");
			exynos_pcie->use_pcieon_sleep = false;
		} else {
			pcie_err(exynos_pcie, "Invalid use-pcieon-sleep value"
				       "(set to default -> false)\n");
			exynos_pcie->use_pcieon_sleep = false;
		}
	} else {
		exynos_pcie->use_pcieon_sleep = false;
	}

	if (!of_property_read_string(np, "use-sysmmu", &use_sysmmu)) {
		if (!strcmp(use_sysmmu, "true")) {
			pcie_info(exynos_pcie, "PCIe SysMMU is ENABLED.\n");
			exynos_pcie->use_sysmmu = true;
		} else if (!strcmp(use_sysmmu, "false")) {
			pcie_info(exynos_pcie, "PCIe SysMMU is DISABLED.\n");
			exynos_pcie->use_sysmmu = false;
		} else {
			pcie_err(exynos_pcie, "Invalid use-sysmmu value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sysmmu = false;
		}
	} else {
		exynos_pcie->use_sysmmu = false;
	}

	if (!of_property_read_string(np, "use-ia", &use_ia)) {
		if (!strcmp(use_ia, "true")) {
			pcie_info(exynos_pcie, "PCIe I/A is ENABLED.\n");
			exynos_pcie->use_ia = true;
		} else if (!strcmp(use_ia, "false")) {
			pcie_info(exynos_pcie, "PCIe I/A is DISABLED.\n");
			exynos_pcie->use_ia = false;
		} else {
			pcie_err(exynos_pcie, "Invalid use-ia value"
				       "(set to default -> false)\n");
			exynos_pcie->use_ia = false;
		}
	} else {
		exynos_pcie->use_ia = false;
	}

	if (of_property_read_u32(np, "separated-msi", &exynos_pcie->separated_msi)) {
		dev_info(dev, "Unset separated-msi value, default '0'\n");
		exynos_pcie->separated_msi = 0;
	} else {
		dev_info(dev, "parse the separated msi: %d\n", exynos_pcie->separated_msi);
	}

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	if (of_property_read_u32(np, "pcie-pm-qos-int",
					&exynos_pcie->int_min_lock))
		exynos_pcie->int_min_lock = 0;
	if (exynos_pcie->int_min_lock)
		exynos_pm_qos_add_request(&exynos_pcie_int_qos[exynos_pcie->ch_num],
				PM_QOS_DEVICE_THROUGHPUT, 0);
	pcie_info(exynos_pcie, "%s: pcie int_min_lock = %d\n",
			__func__, exynos_pcie->int_min_lock);
#endif

	exynos_pcie->pmureg = syscon_regmap_lookup_by_phandle(np,
					"samsung,pmu-phandle");
	if (IS_ERR(exynos_pcie->pmureg)) {
		pcie_err(exynos_pcie, "syscon regmap lookup failed.\n");
		return PTR_ERR(exynos_pcie->pmureg);
	}

	exynos_pcie->sysreg = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	/* Check definitions to access SYSREG in DT*/
	if (IS_ERR(exynos_pcie->sysreg)) {
		pcie_err(exynos_pcie, "SYSREG is not defined.\n");
		return PTR_ERR(exynos_pcie->sysreg);
	}

	/* SSD & WIFI power control */
	exynos_pcie->wlan_gpio = of_get_named_gpio(np, "pcie,wlan-gpio", 0);
	if (exynos_pcie->wlan_gpio < 0) {
		pcie_err(exynos_pcie, "wlan gpio is not defined -> don't use wifi through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->wlan_gpio, 0);
	}

	exynos_pcie->ssd_gpio = of_get_named_gpio(np, "pcie,ssd-gpio", 0);
	if (exynos_pcie->ssd_gpio < 0) {
		pcie_err(exynos_pcie, "ssd gpio is not defined -> don't use ssd through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->ssd_gpio, 0);
	}

	return 0;
}

static int exynos_pcie_rc_get_pin_state(struct platform_device *pdev,
				struct exynos_pcie *exynos_pcie)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	exynos_pcie->perst_gpio = of_get_gpio(np, 0);
	if (exynos_pcie->perst_gpio < 0) {
		pcie_err(exynos_pcie, "cannot get perst_gpio\n");
	} else {
		ret = devm_gpio_request_one(dev, exynos_pcie->perst_gpio,
					    GPIOF_OUT_INIT_LOW, dev_name(dev));
		if (ret)
			return -EINVAL;
	}
	/* Get pin state */
	exynos_pcie->pcie_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(exynos_pcie->pcie_pinctrl)) {
		pcie_err(exynos_pcie, "Can't get pcie pinctrl!!!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_ACTIVE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "active");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_ACTIVE])) {
		pcie_err(exynos_pcie, "Can't set pcie clkerq to output high!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_IDLE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "idle");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
		pcie_err(exynos_pcie, "No idle pin state(but it's OK)!!\n");
	else
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_IDLE]);

	return 0;
}

static int exynos_pcie_rc_get_resource(struct platform_device *pdev,
			struct exynos_pcie *exynos_pcie)
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

static void exynos_pcie_rc_enable_interrupts(struct pcie_port *pp, int enable)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	pcie_info(exynos_pcie, "## %s PCIe INTERRUPT ##\n", enable ? "ENABLE" : "DISABLE");

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

static void exynos_pcie_notify_callback(struct pcie_port *pp, int event)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 id;

	if (event == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (event == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		exynos_pcie_dbg_print_link_history(exynos_pcie);
		id = 1;
	} else {
		pcie_err(exynos_pcie, "PCIe: unknown event!!!\n");
		return;
	}

	pcie_err(exynos_pcie,"[%s] event = 0x%x, id = %d\n", __func__, event, id);

	if (exynos_pcie->rc_event_reg[id] && exynos_pcie->rc_event_reg[id]->callback &&
			(exynos_pcie->rc_event_reg[id]->events & event)) {
		struct exynos_pcie_notify *notify =
			&exynos_pcie->rc_event_reg[id]->notify;
		notify->event = event;
		notify->user = exynos_pcie->rc_event_reg[id]->user;
		pcie_info(exynos_pcie, "Callback for the event : %d\n", event);
		exynos_pcie->rc_event_reg[id]->callback(notify);
		return;
	} else {
		pcie_info(exynos_pcie, "Client driver does not have registration "
				"of the event : %d\n", event);

		if (exynos_pcie->force_recover_linkdn) {
			pcie_info(exynos_pcie, "Force PCIe poweroff --> poweron\n");
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

void exynos_pcie_rc_dislink_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, dislink_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	exynos_pcie_rc_all_dump(exynos_pcie->ch_num);
	exynos_pcie->pcie_is_linkup = 0;
	exynos_pcie->linkdown_cnt++;
	pcie_info(exynos_pcie, "link down and recovery cnt: %d\n",
			exynos_pcie->linkdown_cnt);

	if (!exynos_pcie->info->linkdn_callback_direct)
		exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
}

void exynos_pcie_rc_cpl_timeout_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, cpl_timeout_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	pcie_err(exynos_pcie, "[%s] +++ \n", __func__);

	exynos_pcie_rc_all_dump(exynos_pcie->ch_num);

	if (!exynos_pcie->info->linkdn_callback_direct) {
		pcie_info(exynos_pcie, "[%s] call PCIE_CPL_TIMEOUT callback func.\n", __func__);
		exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_CPL_TIMEOUT);
	}

	pcie_err(exynos_pcie, "[%s] --- \n", __func__);
}

void exynos_pcie_work_l1ss(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, l1ss_boot_delay_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	if (exynos_pcie->work_l1ss_cnt == 0) {
		pcie_info(exynos_pcie, "[%s]boot_cnt: %d, work_l1ss_cnt: %d\n",
				__func__, exynos_pcie->boot_cnt,
				exynos_pcie->work_l1ss_cnt);
		if (exynos_pcie->info->l1ss_set)
			exynos_pcie->info->l1ss_set(1, pp, PCIE_L1SS_CTRL_BOOT);
		exynos_pcie->work_l1ss_cnt++;
	} else {
		pcie_info(exynos_pcie, "[%s]boot_cnt: %d, work_l1ss_cnt: %d\n",
				__func__, exynos_pcie->boot_cnt,
				exynos_pcie->work_l1ss_cnt);
		return;
	}
}

static void exynos_pcie_rc_assert_phy_reset(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	if (exynos_pcie->phy->phy_ops.phy_config != NULL)
		exynos_pcie->phy->phy_ops.phy_config(exynos_pcie, exynos_pcie->ch_num);

	/* Configuration for IA */
	if (exynos_pcie->use_ia && exynos_pcie->info->set_ia_code)
		exynos_pcie->info->set_ia_code(exynos_pcie);
	else
		pcie_info(exynos_pcie, "[%s] Not support I/A!!!\n", __func__);
}

static void exynos_pcie_rc_resumed_phydown(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* SKIP PHY configuration in resumed_phydown.
	 * exynos_pcie_rc_assert_phy_reset(pp);
	 */

	/* phy all power down */
	if (exynos_pcie->phy->phy_ops.phy_all_pwrdn != NULL)
		exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
}

static void exynos_pcie_setup_rc(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
	u32 pm_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_PM];
	u32 val;

	/* enable writing to DBI read-only registers */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MISC_CONTROL, 4, DBI_RO_WR_EN);

	if (exynos_pcie->max_link_speed >= LINK_SPEED_GEN3) {
		/* Disable BAR and Exapansion ROM BAR */
		exynos_pcie_rc_wr_own_conf(pp, 0x100010, 4, 0);
		exynos_pcie_rc_wr_own_conf(pp, 0x100030, 4, 0);
	}

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
	pcie_dbg(exynos_pcie, "PCIE_CAP_LINK_CAPABILITIES_REG(0x7c: 0x%x)\n", val);

	/* set auxiliary clock frequency: 76(0x4C)MHz */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_AUX_CLK_FREQ_OFF, 4, 0x4c);

	/* set duration of L1.2 & L1.2.Entry */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);

	/* clear power management control and status register */
	exynos_pcie_rc_wr_own_conf(pp, pm_cap_off + PCI_PM_CTRL, 4, 0x0);

	/* set target speed from DT */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, &val);
	val &= ~PCI_EXP_LNKCTL2_TLS;
	val |= exynos_pcie->max_link_speed;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, val);

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
	pcie_dbg(exynos_pcie, "%s: before device_ctrl_status(0x98) = 0x%x\n", __func__, val);
	val &= ~(PCIE_CAP_CPL_TIMEOUT_VAL_MASK);
	val |= PCIE_CAP_CPL_TIMEOUT_VAL_6_2MS;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
	pcie_dbg(exynos_pcie, "%s: after device_ctrl_status(0x98) = 0x%x\n", __func__, val);

	/* RC Payload configuration to MAX value */
	if (exynos_pcie->pci_dev) {
		exynos_pcie_rc_rd_own_conf(pp,
				pcie_cap_off + PCI_EXP_DEVCAP, 4, &val);

		val &= PCI_EXP_DEVCAP_PAYLOAD;
		pcie_info(exynos_pcie, "%s: Set supported payload size : %dbyte\n",
			  __func__, 128 << val);
		pcie_set_mps(exynos_pcie->pci_dev, 128 << val);
	}

	if (exynos_pcie->max_link_speed >= LINK_SPEED_GEN3) {
		/* EQ Off */
		exynos_pcie_rc_wr_own_conf(pp, 0x890, 4, 0x12000);
	}
}

static int exynos_pcie_rc_init(struct pcie_port *pp)
{

	/* Set default bus ops */
	pp->bridge->ops = &exynos_rc_own_pcie_ops;
	pp->bridge->child_ops = &exynos_rc_child_pcie_ops;

	/* Setup RC to avoid initialization faile in PCIe stack */
	dw_pcie_setup_rc(pp);
	return 0;
}

/*
static void exynos_pcie_set_num_vectors(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;

	pcie_dbg(exynos_pcie, "Set MSI vector number to : 256\n");
	pp->num_vectors = MAX_MSI_IRQS;
}
*/

static struct dw_pcie_host_ops exynos_pcie_rc_ops = {
	.host_init = exynos_pcie_rc_init,
	/* .set_num_vectors = exynos_pcie_set_num_vectors, */
};

static irqreturn_t exynos_pcie_rc_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;
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

	/* only support after EXYNOS9820 EVT 1.1 */
	if (val_irq1 & IRQ_LINK_DOWN) {
		pcie_info(exynos_pcie, "!!! PCIE LINK DOWN !!!\n");
		pcie_info(exynos_pcie, "!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);

		if (exynos_pcie->cpl_timeout_recovery) {
			pcie_info(exynos_pcie, "!!!now is already cto recovering..\n");
		} else {
			exynos_pcie->sudden_linkdown = 1;
			exynos_pcie->state = STATE_LINK_DOWN_TRY;

			if (exynos_pcie->info->linkdn_callback_direct) {
				exynos_pcie->linkdown_cnt++;
				pcie_info(exynos_pcie, "link down and recovery cnt: %d\n",
						exynos_pcie->linkdown_cnt);

				exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
			}
			queue_work(exynos_pcie->pcie_wq,
					&exynos_pcie->dislink_work.work);
		}
	}

	if (val_irq2 & IRQ_RADM_CPL_TIMEOUT) {
		pcie_info(exynos_pcie, "!!!PCIE_CPL_TIMEOUT (PCIE_IRQ2: 0x%x)!!!\n", val_irq2);
		pcie_info(exynos_pcie, "!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);

		if (exynos_pcie->sudden_linkdown) {
			pcie_info(exynos_pcie, "!!!now is already link down recovering..\n");
		} else {
			if (exynos_pcie->cpl_timeout_recovery == 0) {
				exynos_pcie->state = STATE_LINK_DOWN_TRY;
				exynos_pcie->cpl_timeout_recovery = 1;

				if (exynos_pcie->info->linkdn_callback_direct)
					exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_CPL_TIMEOUT);

				pcie_info(exynos_pcie, "!!!call cpl_timeout work\n");
				queue_work(exynos_pcie->pcie_wq,
						&exynos_pcie->cpl_timeout_work.work);
			} else {
				pcie_info(exynos_pcie, "!!!now is already cto recovering..\n");
			}
		}
	}

	if (val_irq2 & IRQ_MSI_RISING_ASSERT && !exynos_pcie->separated_msi) {
		int ctrl, num_ctrls;
		num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

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
static int exynos_pcie_rc_msi_init_wifi_shdmem(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	phys_addr_t msi_addr_from_dt = 0;

	msi_addr_from_dt = get_msi_base(exynos_pcie->ch_num);
	pp->msi_data = msi_addr_from_dt;
	pcie_dbg(exynos_pcie, "%s: msi_addr_from_dt: 0x%llx, pp->msi_data: 0x%llx\n",
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

	exynos_msi_enable(exynos_pcie, pp);

	return 0;
}

static int exynos_pcie_rc_msi_init_wifi(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* enable MSI interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	exynos_msi_enable(exynos_pcie, pp);

	return 0;
}

static int exynos_pcie_rc_msi_init_cp(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	unsigned long msi_addr_from_dt;
#endif
	/*
	 * The following code is added to avoid duplicated allocation.
	 */
	if (!exynos_pcie->is_ep_scaned) {
		pcie_info(exynos_pcie, "%s: allocate MSI data \n", __func__);

		if (exynos_pcie->ep_pci_bus == NULL)
			exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

		exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, MSI_CONTROL,
				4, &val);
		pcie_info(exynos_pcie, "%s: EP support %d-bit MSI address (0x%x)\n", __func__,
				(val & MSI_64CAP_MASK) ? 64 : 32, val);

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
		/* get the MSI target address from DT */
		msi_addr_from_dt = shm_get_msi_base();
		if (msi_addr_from_dt) {
			pcie_info(exynos_pcie, "%s: MSI target addr. from DT: 0x%lx\n",
					__func__, msi_addr_from_dt);
			pp->msi_data = msi_addr_from_dt;
		} else {
			pcie_err(exynos_pcie, "%s: msi_addr_from_dt is null\n", __func__);
			return -EINVAL;
		}
#else
		pcie_info(exynos_pcie, "EP device is Modem but, ModemIF isn't enabled\n");
#endif
	}

	pcie_info(exynos_pcie, "%s: Program the MSI data: %lx (probe ok:%d)\n", __func__,
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

	if (exynos_pcie->separated_msi)
		exynos_pcie_enable_sep_msi(exynos_pcie, pp);

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	pcie_info(exynos_pcie, "MSI INIT check INTR0 ENABLE, 0x%x: 0x%x \n", PCIE_MSI_INTR0_ENABLE, val);
	if (val != 0xf1) {
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, 0xf1);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	}
#endif
	pcie_info(exynos_pcie, "%s: MSI INIT END, 0x%x: 0x%x \n", __func__, PCIE_MSI_INTR0_ENABLE, val);

	return 0;
}

static void exynos_pcie_rc_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int count = 0;
	int retry_cnt = 0;
	u32 val;

	/* L1.2 enable check */
	pcie_dbg(exynos_pcie, "Current PM state(PCS + 0x188) : 0x%x \n",
			readl(exynos_pcie->phy_pcs_base + 0x188));

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
	pcie_dbg(exynos_pcie, "DBI Link Control Register: 0x%x \n", val);

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	pcie_dbg(exynos_pcie, "%s: link state:%x\n", __func__, val);
	if (!(val >= 0x0d && val <= 0x14)) {
		pcie_info(exynos_pcie, "%s, pcie link is not up\n", __func__);
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
		pcie_err(exynos_pcie, "Current LTSSM State is 0x%x with retry_cnt =%d.\n",
				val, retry_cnt);
	}

	exynos_elbi_write(exynos_pcie, 0x1, XMIT_PME_TURNOFF);

	while (count < MAX_L2_TIMEOUT) {
		if ((exynos_elbi_read(exynos_pcie, PCIE_IRQ0)
						& IRQ_RADM_PM_TO_ACK)) {
			pcie_dbg(exynos_pcie, "ack message is ok\n");
			udelay(10);
			break;
		}

		udelay(10);
		count++;
	}
	if (count >= MAX_L2_TIMEOUT)
		pcie_err(exynos_pcie, "cannot receive ack message from EP\n");

	exynos_elbi_write(exynos_pcie, 0x0, XMIT_PME_TURNOFF);

	count = 0;
	do {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			pcie_dbg(exynos_pcie, "received Enter_L23_READY DLLP packet\n");
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
		pcie_err(exynos_pcie, "cannot receive L23_READY DLLP packet(0x%x)\n", val);
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

static int exynos_pcie_rc_establish_link(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val, busdev;
	int count = 0, try_cnt = 0;
	u32 speed_recovery_cnt = 0;
	unsigned long udelay_before_perst, udelay_after_perst;

	udelay_before_perst = exynos_pcie->info->udelay_before_perst;
	udelay_after_perst = exynos_pcie->info->udelay_after_perst;

retry:
	/* avoid checking rx elecidle when access DBI */
	if (exynos_pcie->phy->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);

	exynos_pcie_rc_assert_phy_reset(pp);

	/* Q-ch disable */
	exynos_pcie_rc_qch_control(exynos_pcie, 0);

	if (exynos_pcie->ip_ver >= 0x992501) {
		/*
		 * PMU setting for PHY power gating enable @L1.2
		 * "1" = Enable PHY power(CDR) gating
		 */
		regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_phy_ctrl,
				PCIE_PHY_CONTROL_MASK1, (0x0 << 5));
		regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_phy_ctrl,
				PCIE_PHY_CONTROL_MASK1, (0x1 << 5));
	}

	if (udelay_before_perst)
		usleep_range(udelay_before_perst, udelay_before_perst + 2000);

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);

	pcie_info(exynos_pcie, "%s: Set PERST to HIGH, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

	if (udelay_after_perst)
		usleep_range(udelay_after_perst, udelay_after_perst + 2000);
	else /* Use default delay 18~20msec */
		usleep_range(18000, 20000);

	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val |= APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
	exynos_elbi_write(exynos_pcie, PCIE_LINKDOWN_RST_MANUAL,
			  PCIE_LINKDOWN_RST_CTRL_SEL);

	/* NAK enable when AXI pending */
	exynos_elbi_write(exynos_pcie, NACK_ENABLE, PCIE_MSTR_PEND_SEL_NAK);
	pcie_dbg(exynos_pcie, "%s: NACK option enable: 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_MSTR_PEND_SEL_NAK));

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	exynos_pcie_setup_rc(pp);

	if (dev_is_dma_coherent(pci->dev))
		exynos_pcie_rc_set_iocc(pp, 1);

	if (exynos_pcie->phy->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);

	pcie_info(exynos_pcie, "D state: %x, %x\n",
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
		pcie_err(exynos_pcie, "%s: Link is not up, try count: %d, linksts: %s(0x%x)\n",
			__func__, try_cnt, LINK_STATE_DISP(val), val);

		if (try_cnt < 10) {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			pcie_info(exynos_pcie, "%s: Set PERST to LOW, gpio val = %d\n",
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
		pcie_dbg(exynos_pcie, "%s: %s(0x%x)\n", __func__,
				LINK_STATE_DISP(val), val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
		val = (val >> 16) & 0xf;
		pcie_dbg(exynos_pcie, "Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		speed_recovery_cnt = 0;
		while (speed_recovery_cnt < MAX_TIMEOUT_GEN1_GEN2) {
			exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
			val = (val >> 16) & 0xf;
			if (val == exynos_pcie->max_link_speed) {
				pcie_dbg(exynos_pcie, "%s: [cnt:%d] Current Link Speed is GEN%d (MAX GEN%d)\n",
						__func__,
						speed_recovery_cnt, val, exynos_pcie->max_link_speed);
				break;
			}

			speed_recovery_cnt++;

			udelay(10);
		}

		/* check link training result(speed) */
		if (exynos_pcie->ip_ver >= 0x982000 && val < exynos_pcie->max_link_speed) {
			try_cnt++;

			pcie_err(exynos_pcie, "%s: [cnt:%d] Link is up. But not GEN%d speed, try count: %d\n",
					__func__, speed_recovery_cnt, exynos_pcie->max_link_speed, try_cnt);

			if (try_cnt < 10) {
				gpio_set_value(exynos_pcie->perst_gpio, 0);
				pcie_info(exynos_pcie, "%s: Set PERST to LOW, gpio val = %d\n",
						__func__,
						gpio_get_value(exynos_pcie->perst_gpio));

				/* LTSSM disable */
				exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
						PCIE_APP_LTSSM_ENABLE);

				goto retry;
			} else {
				pcie_info(exynos_pcie, "[cnt:%d] Current Link Speed is GEN%d (MAX GEN%d)\n",
						speed_recovery_cnt, val, exynos_pcie->max_link_speed);
			}
		}

		/* one more check L0 state for Gen3 recovery */
		count = 0;
		pcie_dbg(exynos_pcie, "%s: check L0 state for Gen3 recovery\n", __func__);
		while (count < MAX_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
			if (val == 0x11)
				break;

			count++;

			udelay(10);
		}

		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
		val = (val >> 16) & 0xf;
		exynos_pcie->current_speed = val;
		pcie_info(exynos_pcie, "Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		/* Q-ch enable */
		exynos_pcie_rc_qch_control(exynos_pcie, 1);

		exynos_pcie_rc_enable_interrupts(pp, 1);

		/* setup ATU for cfg/mem outbound */
		busdev = EXYNOS_PCIE_ATU_BUS(1) | EXYNOS_PCIE_ATU_DEV(0) | EXYNOS_PCIE_ATU_FUNC(0);
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
		exynos_pcie_rc_prog_viewport_mem_outbound(pp);

	}
	return 0;
}

int exynos_pcie_rc_speedchange(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int i;
	u32 val, val1, current_spd;

	if (exynos_pcie->state != STATE_LINK_UP) {
		pcie_err(exynos_pcie, "Link is not up\n");
		return 1;
	}

	pcie_err(exynos_pcie, "%s: force l1ss disable\n", __func__);
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, ch_num);

	if (spd > 3 || spd < 1) {
		pcie_err(exynos_pcie, "Unable to change to GEN%d\n", spd);
		return 1;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &current_spd);
	current_spd = current_spd >> 16;
	current_spd &= PCIE_CAP_LINK_SPEED;
	pcie_info(exynos_pcie, "Current link speed(0x80) : from GEN%d\n", current_spd);

	if (current_spd == spd) {
		pcie_err(exynos_pcie, "Already changed to GEN%d\n", spd);
		return 1;
	}

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

	exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, LINK_CONTROL2_LINK_STATUS2_REG, 4, &val);
	val = val & PCIE_CAP_TARGET_LINK_SPEED_MASK;
	val = val | 0x3;
	exynos_pcie_rc_wr_other_conf(pp, exynos_pcie->ep_pci_bus, 0, LINK_CONTROL2_LINK_STATUS2_REG, 4, val);
	pcie_info(exynos_pcie, "Check EP Current Target Speed Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, LINK_CONTROL2_LINK_STATUS2_REG, 4, &val);
	val = val & PCIE_CAP_TARGET_LINK_SPEED_MASK;
	val = val | spd;
	exynos_pcie_rc_wr_own_conf(pp, LINK_CONTROL2_LINK_STATUS2_REG, 4, val);
	pcie_info(exynos_pcie, "Set RC Target Speed Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val = val & DIRECT_SPEED_CHANGE_MASK;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
	pcie_info(exynos_pcie, "Clear Direct Speed Change Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val = val | DIRECT_SPEED_CHANGE_ENABLE;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
	pcie_info(exynos_pcie, "Set Direct Speed Change Val = 0x%x\n", val);

	for (i = 0; i < MAX_TIMEOUT_SPEEDCHANGE; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &current_spd);
		current_spd = current_spd >> 16;
		current_spd &= PCIE_CAP_LINK_SPEED;

		if (current_spd == spd)
			break;
		udelay(10);
	}

	for (i = 0; i < MAX_TIMEOUT_SPEEDCHANGE; i++) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		val1 = exynos_phy_pcs_read(exynos_pcie, 0x188);

		if ((val == 0x11) || ((val == 0x14) && (val1 == 0x6)))
			break;
		udelay(10);
	}

	if (current_spd != spd) {
		pcie_err(exynos_pcie, "Fail: Unable to change to GEN%d\n", spd);
		return 1;
	}

	pcie_info(exynos_pcie, "Changed link speed(0x80) : to GEN%d\n", current_spd);

	pcie_err(exynos_pcie, "%s: force l1ss enable\n", __func__);
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, ch_num);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_speedchange);

int exynos_pcie_rc_lanechange(int ch_num, int lane)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int i;
	u32 val, val1, lane_num;

	if (exynos_pcie->ip_ver >= 0x992500) {
		pcie_err(exynos_pcie, "9925_EVT0 don't support lane change\n");
		return 1;
	}

	if (exynos_pcie->state != STATE_LINK_UP) {
		pcie_err(exynos_pcie, "Link is not up\n");
		return 1;
	}

	if (lane > 2 || lane < 1) {
		pcie_err(exynos_pcie, "Unable to change to %d lane\n", lane);
		return 1;
	}

	pcie_err(exynos_pcie, "%s: force l1ss disable\n", __func__);
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, ch_num);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
	lane_num = lane_num >> 20;
	lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;
	pcie_info(exynos_pcie, "Current lane_num(0x80) : from %d lane\n", lane_num);

	if (lane_num == lane) {
		pcie_err(exynos_pcie, "Already changed to %d lane\n", lane);
		return 1;
	}

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

	exynos_pcie_rc_rd_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, &val);
	val = val & TARGET_LINK_WIDTH_MASK;
	val = val | lane;
	exynos_pcie_rc_wr_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, val);

	exynos_pcie_rc_rd_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, &val);
	val = val | DIRECT_LINK_WIDTH_CHANGE_SET;
	exynos_pcie_rc_wr_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, val);

	for (i = 0; i < MAX_TIMEOUT_LANECHANGE; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
		lane_num = lane_num >> 20;
		lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;

		if (lane_num == lane)
			break;
		udelay(10);
	}

	for (i = 0; i < MAX_TIMEOUT_LANECHANGE; i++) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		val1 = exynos_phy_pcs_read(exynos_pcie, 0x188);

		if ((val == 0x11) || ((val == 0x14) && (val1 == 0x6)))
			break;
		udelay(10);
	}

	if (lane_num != lane) {
		pcie_err(exynos_pcie, "Unable to change to %d lane\n", lane);
		return 1;
	}

	pcie_info(exynos_pcie, "Changed lane_num(0x80) : to %d lane\n", lane_num);

	pcie_err(exynos_pcie, "%s: force l1ss enable\n", __func__);
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, ch_num);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_lanechange);

static void exynos_pcie_check_payload(struct exynos_pcie *exynos_pcie)
{
	int rc_mps, ep_mps, available_mps, rc_ret, ep_ret;

	if (exynos_pcie->pci_dev == NULL || exynos_pcie->ep_pci_dev == NULL)
		return;

	rc_mps = pcie_get_mps(exynos_pcie->pci_dev);
	ep_mps = pcie_get_mps(exynos_pcie->ep_pci_dev);

	if (rc_mps != ep_mps) {
		pcie_err(exynos_pcie, "MPS(Max Payload Size) is NOT equal.\n");
		rc_mps = exynos_pcie->pci_dev->pcie_mpss;
		ep_mps = exynos_pcie->ep_pci_dev->pcie_mpss;

		available_mps = rc_mps > ep_mps ? ep_mps : rc_mps;

		rc_ret = pcie_set_mps(exynos_pcie->pci_dev, 128 << available_mps);
		ep_ret = pcie_set_mps(exynos_pcie->ep_pci_dev, 128 << available_mps);

		if (rc_ret == -EINVAL || ep_ret == -EINVAL) {
			pcie_err(exynos_pcie, "Can't set payload value.(%d)\n",
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
	struct pcie_port *pp;
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

	pcie_info(exynos_pcie, "%s, start of poweron, pcie state: %d\n", __func__,
							 exynos_pcie->state);
	if (!exynos_pcie->is_ep_scaned) {
		ret = exynos_pcie_rc_phy_load(exynos_pcie);
		if (ret)
			pcie_info(exynos_pcie, "%s, PHYCAL binary load isn't supported \n", __func__);
	}

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pm_runtime_get_sync(dev);

		/* set input clk path change to enable */
		if (exynos_pcie->phy->phy_ops.phy_input_clk_change != NULL)
			exynos_pcie->phy->phy_ops.phy_input_clk_change(exynos_pcie, 1);

		exynos_pcie->pcie_is_linkup = 1;

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
		pcie_dbg(exynos_pcie, "%s, ip idle status : %d, idle_ip_index: %d \n",
				__func__, PCIE_IS_ACTIVE, exynos_pcie->idle_ip_index);
		exynos_update_ip_idle_status(
				exynos_pcie->idle_ip_index,
				PCIE_IS_ACTIVE);

		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num],
					exynos_pcie->int_min_lock);
			pcie_dbg(exynos_pcie, "%s: pcie int_min_lock = %d\n",
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

		if ((exynos_pcie_desc) && (exynos_pcie_desc->depth > 0)) {
			enable_irq(pp->irq);
		} else {
			pcie_dbg(exynos_pcie, "%s, already enable_irq, so skip\n", __func__);
		}

		if (exynos_pcie_rc_establish_link(pp)) {
			pcie_err(exynos_pcie, "pcie link up fail\n");
			goto poweron_fail;
		}

		exynos_pcie->state = STATE_LINK_UP;

		exynos_pcie->sudden_linkdown = 0;

		ret = exynos_pcie->info->msi_init(pp);
		if (ret) {
			pcie_err(exynos_pcie, "%s: failed to MSI initialization(%d)\n",
					__func__, ret);
			return ret;
		}
		pcie_dbg(exynos_pcie, "[%s] exynos_pcie->is_ep_scaned : %d\n", __func__, exynos_pcie->is_ep_scaned);

		if (!exynos_pcie->is_ep_scaned) {
			/*
			 * To restore registers that initialized at probe time.
			 * (AER, PME capabilities)
			 */
			pcie_info(exynos_pcie, "Restore saved state(%d)\n",
				  exynos_pcie->pci_dev->state_saved);
			pci_restore_state(exynos_pcie->pci_dev);

			pci_rescan_bus(exynos_pcie->pci_dev->bus);

			exynos_pcie->ep_pci_dev = exynos_pcie_get_pci_dev(&pci->pp);
			exynos_pcie_mem_copy_epdev(exynos_pcie->ch_num, &exynos_pcie->ep_pci_dev->dev);

			exynos_pcie->ep_pci_dev->dma_mask = DMA_BIT_MASK(36);
			exynos_pcie->ep_pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
			dma_set_seg_boundary(&(exynos_pcie->ep_pci_dev->dev), DMA_BIT_MASK(36));

			if (exynos_pcie->info->set_dma_ops) {
				exynos_pcie->info->set_dma_ops(&exynos_pcie->ep_pci_dev->dev);
				pcie_dbg(exynos_pcie, "DMA operations are changed to support SysMMU\n");
			}

			if (pci_save_state(exynos_pcie->pci_dev)) {
				pcie_err(exynos_pcie, "Failed to save pcie state\n");
				goto poweron_fail;
			}
			exynos_pcie->pci_saved_configs =
				pci_store_saved_state(exynos_pcie->pci_dev);

			exynos_pcie->is_ep_scaned = 1;
		} else if (exynos_pcie->is_ep_scaned) {
			if (exynos_pcie->info->l1ss_delayed_work) {
				if (exynos_pcie->boot_cnt == 0) {
					schedule_delayed_work(
							&exynos_pcie->l1ss_boot_delay_work,
							msecs_to_jiffies(40000));
					exynos_pcie->boot_cnt++;
					exynos_pcie->l1ss_ctrl_id_state |=
						PCIE_L1SS_CTRL_BOOT;
				}
			}

			if (pci_load_saved_state(exynos_pcie->pci_dev,
					     exynos_pcie->pci_saved_configs)) {
				pcie_err(exynos_pcie, "Failed to load pcie state\n");
				goto poweron_fail;
			}
			pci_restore_state(exynos_pcie->pci_dev);
		}

		if (exynos_pcie->info->l1ss_ep_cfg)
			exynos_pcie->info->l1ss_ep_cfg(pp);

		exynos_pcie->cpl_timeout_recovery = 0;
		exynos_pcie_check_payload(exynos_pcie);
	}

	pcie_info(exynos_pcie, "%s, end of poweron, pcie state: %d\n", __func__,
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
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;
	unsigned long flags;
	u32 val;

	pcie_info(exynos_pcie, "%s, start of poweroff, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_UP ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		disable_irq(pp->irq);

		if (exynos_pcie->ip_ver == 0x982001) { /* 9820 EVT1 */
			/* only support EVT1.1 */
				val = exynos_elbi_read(exynos_pcie, PCIE_IRQ1_EN);
				val &= ~IRQ_LINKDOWN_ENABLE_EVT1_1;
				exynos_elbi_write(exynos_pcie, val, PCIE_IRQ1_EN);
		}

		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		exynos_pcie_rc_send_pme_turn_off(exynos_pcie);

		if (exynos_pcie->ip_ver >= 0x992500) { /* for SAMSUNG PHY */
			writel(0xA, exynos_pcie->phy_base + 0x580);
			writel(0x300DE, exynos_pcie->phy_pcs_base + 0x150);
		}

		exynos_pcie->state = STATE_LINK_DOWN;

		if (exynos_pcie->info->mdelay_before_perst_low) {
			pcie_dbg(exynos_pcie, "%s, It needs delay after setting perst to low\n", __func__);
			mdelay(exynos_pcie->info->mdelay_before_perst_low);
		}

		/* Disable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_disable(ch_num);

		if (exynos_pcie->use_ia && exynos_pcie->info->set_ia_code) {
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
		pcie_info(exynos_pcie, "%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

		if (exynos_pcie->info->mdelay_after_perst_low) {
			pcie_dbg(exynos_pcie, "%s, It needs delay after setting perst to low\n", __func__);
			mdelay(exynos_pcie->info->mdelay_after_perst_low);
		}

		/* LTSSM disable */
		exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
				PCIE_APP_LTSSM_ENABLE);

		spin_lock(&exynos_pcie->reg_lock);
		/* force SOFT_PWR_RESET */
		pcie_dbg(exynos_pcie, "%s: Set PERST to LOW, gpio val = %d\n",
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

		/* set input clk path change to disable */
		if (exynos_pcie->phy->phy_ops.phy_input_clk_change != NULL)
			exynos_pcie->phy->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

		exynos_pcie->current_busdev = 0x0;

		if (!IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
			pinctrl_select_state(exynos_pcie->pcie_pinctrl,
					exynos_pcie->pin_state[PCIE_PIN_IDLE]);

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num], 0);
			pcie_dbg(exynos_pcie, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}

		pcie_dbg(exynos_pcie, "%s, ip idle status : %d, idle_ip_index: %d \n",
				__func__, PCIE_IS_IDLE,	exynos_pcie->idle_ip_index);
		exynos_update_ip_idle_status(
				exynos_pcie->idle_ip_index,
				PCIE_IS_IDLE);
#endif
		pm_runtime_put_sync(dev);
	}
	exynos_pcie->pcie_is_linkup = 0;

	pcie_info(exynos_pcie, "%s, end of poweroff, pcie state: %d\n",  __func__,
			exynos_pcie->state);

	return;
}
EXPORT_SYMBOL(exynos_pcie_rc_poweroff);

/* Get EP pci_dev structure of BUS 1 */
static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp)
{
	int domain_num;
	struct pci_dev *ep_pci_dev;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
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

	exynos_pcie->ep_pcie_cap_off = ep_pci_dev->pcie_cap;
	exynos_pcie->ep_ext_l1ss_cap_off = pci_find_ext_capability(ep_pci_dev, PCI_EXT_CAP_ID_L1SS);
	if (!exynos_pcie->ep_ext_l1ss_cap_off)
		pcie_info(exynos_pcie, "%s: cannot find l1ss extended capability offset\n", __func__);
	exynos_pcie->ep_ext_ltr_cap_off = pci_find_ext_capability(ep_pci_dev, PCI_EXT_CAP_ID_LTR);
	if (!exynos_pcie->ep_ext_ltr_cap_off)
		pcie_info(exynos_pcie, "%s: cannot find LTR extended capability offset\n", __func__);

	return ep_pci_dev;
}

void exynos_pcie_config_l1ss_bcm_wifi(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct pci_dev *ep_pci_dev;
	unsigned long flags;
	u32 val;
	u32 rc_pcie_cap_off, rc_ext_l1ss_cap_off;
	u32 ep_pcie_cap_off, ep_ext_l1ss_cap_off, ep_ext_ltr_cap_off;
	u32 tpoweron_time = exynos_pcie->info->tpoweron_time;

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		pcie_err(exynos_pcie, "Failed to set L1SS Enable (pci_dev == NULL)!\n");
		return;
	}

	rc_pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
	rc_ext_l1ss_cap_off = exynos_pcie->pci_ext_cap[PCI_EXT_CAP_ID_L1SS];
	if (rc_pcie_cap_off == 0 || rc_ext_l1ss_cap_off == 0) {
		pcie_err(exynos_pcie, "There are no RC capabilities!(0x%x, 0x%x)\n",
			 rc_pcie_cap_off, rc_ext_l1ss_cap_off);
		return;
	}

	ep_pcie_cap_off = exynos_pcie->ep_pcie_cap_off;
	ep_ext_l1ss_cap_off = exynos_pcie->ep_ext_l1ss_cap_off;
	ep_ext_ltr_cap_off = exynos_pcie->ep_ext_ltr_cap_off;
	if (ep_pcie_cap_off == 0 || ep_ext_l1ss_cap_off == 0 || ep_ext_ltr_cap_off == 0) {
		pcie_err(exynos_pcie, "There are no EP capabilities!(0x%x, 0x%x, 0x%x)\n",
			 ep_pcie_cap_off, ep_ext_l1ss_cap_off, ep_ext_ltr_cap_off);
		return;
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);

	pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL2,
			       tpoweron_time);
	pci_write_config_dword(ep_pci_dev, ep_ext_ltr_cap_off + PCI_LTR_MAX_SNOOP_LAT,
			       0x10031003);
	pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_DEVCTL2, &val);
	pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_DEVCTL2,
			       val | (1 << 10)); /* LTR Enable */

	exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
	val |= PORT_LINK_TCOMMON_70US | PORT_LINK_L1SS_ENABLE |
		PORT_LINK_L12_LTR_THRESHOLD;
	exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
	exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL2, 4,
				   tpoweron_time);

	/*
	   val = PORT_LINK_L1SS_T_PCLKACK | PORT_LINK_L1SS_T_L1_2 |
	   PORT_LINK_L1SS_T_POWER_OFF;
	 */
	//exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);
	exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_DEVCTL2, 4,
			PCI_EXP_DEVCTL2_LTR_EN);

	/* Enable L1SS on Root Complex */
	exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val &= ~PCI_EXP_LNKCTL_ASPMC;
	val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
	exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, val);

	/* Enable supporting ASPM/PCIPM */
	pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
	pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1,
			val | WIFI_COMMON_RESTORE_TIME | WIFI_ALL_PM_ENABEL |
			PORT_LINK_L12_LTR_THRESHOLD);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_CLK_REQ_EN | WIFI_USE_SAME_REF_CLK |
			WIFI_ASPM_L1_ENTRY_EN;
		pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, val);

		pcie_dbg(exynos_pcie, "l1ss enabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	} else {
		pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_CLK_REQ_EN | WIFI_USE_SAME_REF_CLK;
		pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, val);

		exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL,
				4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL,
				4, val);
		pcie_dbg(exynos_pcie, "l1ss disabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
}

void exynos_pcie_config_l1ss_qc_wifi(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct pci_dev *ep_pci_dev;
	unsigned long flags;
	u32 val;
	u32 rc_pcie_cap_off, rc_ext_l1ss_cap_off;
	u32 ep_pcie_cap_off, ep_ext_l1ss_cap_off;
	u32 tpoweron_time = exynos_pcie->info->tpoweron_time;

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		pcie_err(exynos_pcie,
				"Failed to set L1SS Enable (pci_dev == NULL)!!!\n");
		return ;
	}

	rc_pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
	rc_ext_l1ss_cap_off = exynos_pcie->pci_ext_cap[PCI_EXT_CAP_ID_L1SS];
	if (rc_pcie_cap_off == 0 || rc_ext_l1ss_cap_off == 0) {
		pcie_err(exynos_pcie, "There are no RC capabilities!(0x%x, 0x%x)\n",
			 rc_pcie_cap_off, rc_ext_l1ss_cap_off);
		return;
	}

	ep_pcie_cap_off = exynos_pcie->ep_pcie_cap_off;
	ep_ext_l1ss_cap_off = exynos_pcie->ep_ext_l1ss_cap_off;
	if (ep_pcie_cap_off == 0 || ep_ext_l1ss_cap_off == 0) {
		pcie_err(exynos_pcie, "There are no EP capabilities!(0x%x, 0x%x)\n",
			 ep_pcie_cap_off, ep_ext_l1ss_cap_off);
		return;
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);

	pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL2,
			       tpoweron_time);

	/* Get COMMON_MODE Value */
	exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CAP, 4, &val);
	val &= ~(PORT_LINK_TCOMMON_MASK);
	val |= WIFI_COMMON_RESTORE_TIME_70US;
	exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CAP, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CAP, 4, &val);
	pcie_dbg(exynos_pcie, "%s:RC l1ss cap: 0x%x\n", __func__, val);

	//exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);
	exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_DEVCTL2, 4,
			PCI_EXP_DEVCTL2_LTR_EN);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* RC L1ss Enable */
		exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
		val &= ~(PORT_LINK_TCOMMON_MASK);
		val |= WIFI_COMMON_RESTORE_TIME_70US | PORT_LINK_L1SS_ENABLE |
			PORT_LINK_L12_LTR_THRESHOLD;
		exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
		exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL2, 4,
					   tpoweron_time);

		exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
		pcie_dbg(exynos_pcie, "%s: RC l1ss control: 0x%x\n", __func__, val);

		/* EP L1ss Enable */
		pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
		pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1,
				val | WIFI_ALL_PM_ENABEL | PORT_LINK_L12_LTR_THRESHOLD);
		pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
		pcie_dbg(exynos_pcie, "%s: EP l1ss control: 0x%x\n", __func__, val);

		/* RC ASPM Enable */
		exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
		exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, val);
		pcie_dbg(exynos_pcie, "%s: RC link control: 0x%x\n", __func__, val);

		/* EP ASPM Enable */
		pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_USE_SAME_REF_CLK |
			WIFI_ASPM_L1_ENTRY_EN;
		pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, val);
		pcie_dbg(exynos_pcie, "%s: EP link control: 0x%x\n", __func__, val);

		pcie_dbg(exynos_pcie, "l1ss enabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	} else {
		/* EP ASPM Dsiable */
		pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_USE_SAME_REF_CLK;
		pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, val);
		pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
		pcie_dbg(exynos_pcie, "%s: EP link control: 0x%x\n", __func__, val);

		/* RC ASPM Disable */
		exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL,
				4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		val |= PCI_EXP_LNKCTL_CCC;
		exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL,
				4, val);
		pcie_dbg(exynos_pcie, "%s: RC link control: 0x%x\n", __func__, val);

		/* EP L1ss Disable */
		pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
		val |= PORT_LINK_L12_LTR_THRESHOLD;
		val &= ~(WIFI_ALL_PM_ENABEL);
		pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);
		pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
		pcie_dbg(exynos_pcie, "%s: EP l1ss control: 0x%x\n", __func__, val);

		/* RC L1ss Disable */
		exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
		val &= ~(PORT_LINK_TCOMMON_MASK);
		val |= WIFI_COMMON_RESTORE_TIME_70US | PORT_LINK_L12_LTR_THRESHOLD;
		val &= ~(PORT_LINK_L1SS_ENABLE);
		exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
		exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL2, 4,
					   tpoweron_time);
		exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
		pcie_dbg(exynos_pcie, "%s: RC l1ss control: 0x%x\n", __func__, val);

		pcie_dbg(exynos_pcie, "l1ss disabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
}

static int exynos_pcie_set_l1ss_samsung(int enable, struct pcie_port *pp, int id)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;
	unsigned long flags;
	struct pci_dev *ep_pci_dev;
	u32 rc_pcie_cap_off, rc_ext_l1ss_cap_off;
	u32 ep_pcie_cap_off, ep_ext_l1ss_cap_off;
	u32 tpoweron_time = exynos_pcie->info->tpoweron_time;

	pcie_info(exynos_pcie, "%s:L1SS_START(l1ss_ctrl_id_state=0x%x, id=0x%x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	if (exynos_pcie->state != STATE_LINK_UP || !exynos_pcie->current_busdev) {
		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		pcie_info(exynos_pcie, "%s: It's not needed. This will be set later."
				"(state = 0x%x, id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);

		return -1;
	} else {
		ep_pci_dev = exynos_pcie_get_pci_dev(pp);
		if (ep_pci_dev == NULL) {
			pcie_err(exynos_pcie, "Failed to set L1SS %s (pci_dev == NULL)!!!\n",
					enable ? "ENABLE" : "FALSE");
			return -EINVAL;
		}

		rc_pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
		rc_ext_l1ss_cap_off = exynos_pcie->pci_ext_cap[PCI_EXT_CAP_ID_L1SS];
		if (rc_pcie_cap_off == 0 || rc_ext_l1ss_cap_off == 0) {
			pcie_err(exynos_pcie, "There are no RC capabilities!(0x%x, 0x%x)\n",
					rc_pcie_cap_off, rc_ext_l1ss_cap_off);
			return -EINVAL;
		}

		ep_pcie_cap_off = exynos_pcie->ep_pcie_cap_off;
		ep_ext_l1ss_cap_off = exynos_pcie->ep_ext_l1ss_cap_off;
		if (ep_pcie_cap_off == 0 || ep_ext_l1ss_cap_off == 0) {
			pcie_err(exynos_pcie, "There are no EP capabilities!(0x%x, 0x%x)\n",
					ep_pcie_cap_off, ep_ext_l1ss_cap_off);
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (enable) {	/* enable == 1 */
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);

		if (exynos_pcie->l1ss_ctrl_id_state == 0) {

			/* RC & EP L1SS & ASPM setting */

			/* 1-1 RC, set L1SS */
			exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
			/* Actual TCOMMON value is 32usec (val = 0x20 << 8) */
			val &= ~(PORT_LINK_L1SS_THRE_SCALE_MASK | /* to ignore mainline's control */
				 PORT_LINK_L1SS_THRE_VAL_MASK | /* to ignore mainline's control */
				 PORT_LINK_TCOMMON_MASK);
			val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
			exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
			pcie_info(exynos_pcie, "RC L1SS_CONTROL1(0x%x) = 0x%x\n",
				  rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);

			/* 1-2 RC: set TPOWERON */
			/* Set TPOWERON value for RC: check exynos info */
			exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL2,
						   4, tpoweron_time);
			exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL2, 4, &val);
			pcie_info(exynos_pcie, "RC L1SS_CONTROL2(0x%x) = 0x%x\n",
				  rc_ext_l1ss_cap_off + PCI_L1SS_CTL2, val);

			/* exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4,
			 * PCIE_L1_SUB_VAL);
			 */

			/* 1-3 RC: set LTR_EN */
			exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
			val |= PCI_EXP_DEVCTL2_LTR_EN;
			exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_DEVCTL2, 4, val);

			/* 2-1 EP: set LTR_EN (reg_addr = 0x98) */
			pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_DEVCTL2, &val);
			val |= PCI_EXP_DEVCTL2_LTR_EN;
			pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_DEVCTL2, val);

			/* 2-2 EP: set TPOWERON */
			/* Set TPOWERON value for EP: Check exynos info*/
			pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL2,
					       tpoweron_time);

			/* 2-3 EP: set Entrance latency */
			/* Set L1.2 Enterance Latency for EP: 64 usec */
			pci_read_config_dword(ep_pci_dev, PCIE_ACK_F_ASPM_CONTROL, &val);
			val &= ~PCIE_L1_ENTERANCE_LATENCY;
			val |= PCIE_L1_ENTERANCE_LATENCY_64us;
			pci_write_config_dword(ep_pci_dev, PCIE_ACK_F_ASPM_CONTROL, val);

			/* 2-4 EP: set L1SS */
			pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
			val |= PORT_LINK_L1SS_ENABLE;
			pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);
			pcie_info(exynos_pcie, "Enable EP L1SS_CONTROL(0x%x) = 0x%x\n",
						ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);

			/* 3. RC ASPM Enable*/
			exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, val);
			pcie_info(exynos_pcie, "Enable RC PCIE_LNKCTL : ASPM(0x80) = 0x%x\n", val);

			/* 4. EP ASPM Enable */
			pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_CLKREQ_EN |
				PCI_EXP_LNKCTL_ASPM_L1;
			pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, val);
			pcie_info(exynos_pcie, "Enable EP PCIE LNKCTL : ASPM(0x80) = 0x%x\n", val);

			/* DBG:
			 * pcie_info(exynos_pcie, "(%s): l1ss_enabled(l1ss_ctrl_id_state = 0x%x)\n",
			 *			__func__, exynos_pcie->l1ss_ctrl_id_state);
			 */
		}
	} else {	/* enable == 0 */
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			/* 1. EP ASPM Disable */
			pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
			val &= ~(PCI_EXP_LNKCTL_ASPMC);
			pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL, val);
			pcie_info(exynos_pcie, "Disable EP ASPM(0x80) = 0x%x\n", val);

			/* 2. RC ASPM Disable */
			exynos_pcie_rc_rd_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			exynos_pcie_rc_wr_own_conf(pp, rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, val);
			pcie_info(exynos_pcie, "Disable RC ASPM(0x80) = 0x%x\n", val);

			/* EP: clear L1SS */
			pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);
			pcie_info(exynos_pcie, "Disable EP L1SS_CONTROL(0x%x) = 0x%x\n",
						ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);

			/* RC: clear L1SS */
			exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
			pcie_info(exynos_pcie, "Disable RC L1SS_CONTROL(0x%x) = 0x%x\n", val, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1);

			/* DBG:
			 * pcie_info(exynos_pcie, "(%s): l1ss_disabled(l1ss_ctrl_id_state = 0x%x)\n",
			 *		__func__, exynos_pcie->l1ss_ctrl_id_state);
			 */
		}
	}
	pcie_info(exynos_pcie, "LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
			exynos_phy_pcs_read(exynos_pcie, 0x188));

	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

	pcie_info(exynos_pcie, "%s:L1SS_END(l1ss_ctrl_id_state=0x%x, id=0x%x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	return 0;
}

static int exynos_pcie_set_l1ss_wifi(int enable, struct pcie_port *pp, int id)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;
	unsigned long flags;
	struct pci_dev *ep_pci_dev;
	u32 rc_pcie_cap_off, rc_ext_l1ss_cap_off;
	u32 ep_pcie_cap_off, ep_ext_l1ss_cap_off;

	pcie_info(exynos_pcie, "%s: START (state = 0x%x, id = 0x%x, enable = %d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (exynos_pcie->state != STATE_LINK_UP || !exynos_pcie->current_busdev) {
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		pcie_info(exynos_pcie, "%s: It's not needed. This will be set later."
				"(state = 0x%x, id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);
		return -1;
	}

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		pcie_err(exynos_pcie, "Failed to set L1SS %s (pci_dev == NULL)!!!\n",
				enable ? "ENABLE" : "FALSE");
		return -EINVAL;
	}

	rc_pcie_cap_off = exynos_pcie->pci_cap[PCI_CAP_ID_EXP];
	rc_ext_l1ss_cap_off = exynos_pcie->pci_ext_cap[PCI_EXT_CAP_ID_L1SS];
	if (rc_pcie_cap_off == 0 || rc_ext_l1ss_cap_off == 0) {
		pcie_err(exynos_pcie, "There are no RC capabilities!(0x%x, 0x%x)\n",
			 rc_pcie_cap_off, rc_ext_l1ss_cap_off);
		return -EINVAL;
	}

	ep_pcie_cap_off = exynos_pcie->ep_pcie_cap_off;
	ep_ext_l1ss_cap_off = exynos_pcie->ep_ext_l1ss_cap_off;
	if (ep_pcie_cap_off == 0 || ep_ext_l1ss_cap_off == 0) {
		pcie_err(exynos_pcie, "There are no EP capabilities!(0x%x, 0x%x)\n",
			 ep_pcie_cap_off, ep_ext_l1ss_cap_off);
		return -EINVAL;
	}

	if (enable) {
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		if (exynos_pcie->l1ss_ctrl_id_state == 0) {
			/* RC L1ss Enable */
			exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
			val |= PORT_LINK_L1SS_ENABLE;
			exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
			pcie_dbg(exynos_pcie, "Enable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);

			/* EP L1ss Enable */
			pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
			val |= WIFI_ALL_PM_ENABEL;
			pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);
			pcie_dbg(exynos_pcie, "Enable L1ss: EP L1SS_CONTROL = 0x%x\n", val);

			/* RC ASPM Enable */
			exynos_pcie_rc_rd_own_conf(pp,
					rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_own_conf(pp,
					rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, val);

			/* EP ASPM Enable */
			pci_read_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL,
					&val);
			val &= ~(WIFI_ASPM_CONTROL_MASK);
			val |= WIFI_ASPM_L1_ENTRY_EN;
			pci_write_config_dword(ep_pci_dev, ep_pcie_cap_off + PCI_EXP_LNKCTL,
					val);
		}
	} else {
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			/* EP ASPM Disable */
			pci_read_config_dword(ep_pci_dev,
					ep_pcie_cap_off + PCI_EXP_LNKCTL, &val);
			val &= ~(WIFI_ASPM_CONTROL_MASK);
			pci_write_config_dword(ep_pci_dev,
					ep_pcie_cap_off + PCI_EXP_LNKCTL, val);

			/* RC ASPM Disable */
			exynos_pcie_rc_rd_own_conf(pp,
					rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			exynos_pcie_rc_wr_own_conf(pp,
					rc_pcie_cap_off + PCI_EXP_LNKCTL, 4, val);

			/* EP L1ss Disable */
			pci_read_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, &val);
			val &= ~(WIFI_ALL_PM_ENABEL);
			pci_write_config_dword(ep_pci_dev, ep_ext_l1ss_cap_off + PCI_L1SS_CTL1, val);
			pcie_dbg(exynos_pcie, "Disable L1ss: EP L1SS_CONTROL = 0x%x\n", val);

			/* RC L1ss Disable */
			exynos_pcie_rc_rd_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			exynos_pcie_rc_wr_own_conf(pp, rc_ext_l1ss_cap_off + PCI_L1SS_CTL1, 4, val);
			pcie_dbg(exynos_pcie, "Disable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);
		}
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
	pcie_info(exynos_pcie, "%s: END (state = 0x%x, id = 0x%x, enable = %d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	return 0;
}

int exynos_pcie_l1ss_ctrl(int enable, int id, int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	if (!pci) {
		pcie_err(exynos_pcie,"%s: pcie isn't registered!\n");
		return -EINVAL;
	}

	if (!exynos_pcie->info->l1ss_set) {
		pcie_err(exynos_pcie, "%s: l1ss_set func. isn't defined\n", __func__);
		return -EINVAL;
	}

	return exynos_pcie->info->l1ss_set(enable, pp, id);
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
			pcie_err(exynos_pcie,"%s: cannot change to L0(LTSSM = 0x%x, cnt = %d)\n",
					__func__, val, count);
			ret = -EPIPE;
		} else {
			/* remove too much print */
			/* pcie_err(exynos_pcie,"%s: L0 state(LTSSM = 0x%x, cnt = %d)\n",
			   __func__, val, count); */
		}

		/* Set h/w L1 exit mode */
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val |= APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
		exynos_elbi_write(exynos_pcie, 0x0, PCIE_APP_REQ_EXIT_L1);
	} else {
		/* remove too much print */
		/* pcie_err(exynos_pcie,"%s: skip!!! l1.2 is already diabled(id = 0x%x)\n",
		   __func__, exynos_pcie->l1ss_ctrl_id_state); */
	}

	spin_unlock_irqrestore(&exynos_pcie->pcie_l1_exit_lock, flags);

	return ret;

}
EXPORT_SYMBOL(exynos_pcie_l1_exit);

int exynos_pcie_host_v1_poweron(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pcie_err(exynos_pcie,"### [%s] requested spd: GEN%d\n", __func__, spd);
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

	pcie_info(exynos_pcie, "## exynos_pcie_pm_suspend(state: %d)\n",
								exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info(exynos_pcie, "RC%d already off\n",
							exynos_pcie->ch_num);
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

	pcie_err(exynos_pcie,"[%s] set cpl_timeout_state to recovery_on\n", __func__);
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
		pcie_err(exynos_pcie,"%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
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
		pcie_err(exynos_pcie, "Check unexpected state - H/W:0x%x, S/W:%d\n",
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
	struct pcie_port *pp;
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
		pcie_err(exynos_pcie,"PCIe: unknown event!!!\n");
		return -EINVAL;
	}
	pcie_err(exynos_pcie,"[%s] event = 0x%x, id = %d\n", __func__, reg->events, id);

	if (pp) {
		exynos_pcie->rc_event_reg[id] = reg;
		pcie_info(exynos_pcie,
				"Event 0x%x is registered for RC %d\n",
				reg->events, exynos_pcie->ch_num);
	} else {
		pcie_err(exynos_pcie,"PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_register_event);

int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
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
		pcie_err(exynos_pcie,"PCIe: unknown event!!!\n");
		return -EINVAL;
	}

	if (pp) {
		exynos_pcie->rc_event_reg[id] = NULL;
		pcie_info(exynos_pcie, "Event is deregistered for RC %d\n",
				exynos_pcie->ch_num);
	} else {
		pcie_err(exynos_pcie,"PCIe: did not find RC for pci endpoint device\n");
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
	struct pcie_port *pp;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}

	pci = exynos_pcie->pci;
	pp = &pci->pp;

	irq_set_affinity_hint(pp->irq, cpumask_of(affinity));

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_affinity);

static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				   const struct cpumask *mask, bool force)
{
	struct pcie_port *pp = irq_data->parent_data->domain->host_data;

	irq_set_affinity_hint(pp->irq, mask);

	return 0;
}

static int exynos_pcie_msi_set_wake(struct irq_data *irq_data, unsigned int enable)
{
	int ret = 0;
	struct pcie_port *pp;

	if (irq_data == NULL)
		return -EINVAL;

	pp = irq_data->parent_data->domain->host_data;
	if (enable)
		ret = enable_irq_wake(pp->irq);
	else
		ret = disable_irq_wake(pp->irq);

	return ret;
}

static void exynos_pcie_msi_irq_enable(struct irq_data *data)
{
	data->chip->irq_unmask(data);
}

static void exynos_pcie_msi_irq_disable(struct irq_data *data)
{
	data->chip->irq_mask(data);
}

static inline void exynos_pcie_enable_sep_msi(struct exynos_pcie *exynos_pcie,
					      struct pcie_port *pp)
{
	u32 val;
	int i;

	pr_info("Enable Separated MSI IRQs(Disable MSI RISING EDGE).\n");
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val &= ~IRQ_MSI_CTRL_EN_RISING_EDG;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	for (i = PCIE_START_SEP_MSI_VEC; i < PCIE_MAX_SEPA_IRQ_NUM; i++) {
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

static const char *sep_irq_name[PCIE_MAX_SEPA_IRQ_NUM] = {
	"exynos-pcie-msi0", "exynos-pcie-msi1", "exynos-pcie-msi2",
	"exynos-pcie-msi3", "exynos-pcie-msi4" };

/*
 * CAUTION : If PCIE_START_SEP_MSI_VEC definition is changed,
 * handler function must be changed to call dw_handle_msi_irq().
 */
PCIE_EXYNOS_MSI_DEFAULT_HANDLER_VEC(0);
PCIE_EXYNOS_MSI_HANDLER_VEC(1);
PCIE_EXYNOS_MSI_HANDLER_VEC(2);
PCIE_EXYNOS_MSI_HANDLER_VEC(3);
PCIE_EXYNOS_MSI_HANDLER_VEC(4);

static irqreturn_t (*msi_handler[PCIE_MAX_SEPA_IRQ_NUM])(int , void *) = {
	exynos_pcie_msi0_handler, exynos_pcie_msi1_handler,  exynos_pcie_msi2_handler,
	exynos_pcie_msi3_handler, exynos_pcie_msi4_handler };

int register_separated_msi_vector(int ch_num, irq_handler_t handler, void *context,
		int *irq_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int i, ret;

	for (i = 1; i < PCIE_MAX_SEPA_IRQ_NUM; i++) {
		if (!sep_msi_vec[ch_num][i].is_used)
			break;
	}

	if (i == PCIE_MAX_SEPA_IRQ_NUM) {
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
			msi_handler[i],
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			sep_irq_name[i], pp);
	if (ret) {
		pr_err("failed to request MSI%d irq\n", i);

		return ret;
	}

	return i * 32;
}
EXPORT_SYMBOL_GPL(register_separated_msi_vector);


#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int exynos_pcie_rc_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_pcie *exynos_pcie = container_of(nb, struct exynos_pcie, itmon_nb);
	struct device *dev = exynos_pcie->pci->dev;
	struct itmon_notifier *itmon_info = nb_data;
	unsigned int val;

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/* only for 9830 HSI2 block */
	if (exynos_pcie->ip_ver == 0x983000) {
		if ((itmon_info->port && !strcmp(itmon_info->port, "HSI2")) ||
				(itmon_info->dest && !strcmp(itmon_info->dest, "HSI2"))) {
			regmap_read(exynos_pcie->pmureg, exynos_pcie->pmu_phy_isolation, &val);
			pcie_info(exynos_pcie, "### PMU PHY Isolation : 0x%x\n", val);

			exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
		}
	} else {
		pcie_dbg(exynos_pcie, "skip register dump(ip_ver = 0x%x)\n", exynos_pcie->ip_ver);
	}

	return NOTIFY_DONE;
}
#endif

static int exynos_pcie_rc_add_port(struct platform_device *pdev,
					struct pcie_port *pp, int ch_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct irq_domain *msi_domain;
	struct msi_domain_info *msi_domain_info;
	int ret, i, sep_irq;
	u32 val, vendor_id, device_id;

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		pcie_err(exynos_pcie, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_rc_irq_handler,
			       IRQF_SHARED | IRQF_TRIGGER_HIGH, "exynos-pcie", pp);
	if (ret) {
		pcie_err(exynos_pcie, "failed to request irq\n");
		return ret;
	}

	if (exynos_pcie->separated_msi) {
		for (i = 0; i < PCIE_MAX_SEPA_IRQ_NUM; i++) {
			sep_irq = platform_get_irq(pdev, i + 1);
			if (sep_irq < 0) {
				if (i == 0)
					pr_err("WARN : There are no default Separated MSI!\n");
				goto skip_sep_request_irq;
			}

			dev_info(&pdev->dev, "%d separated MSI irq is defined.\n", sep_irq);
			sep_msi_vec[ch_num][i].irq = sep_irq;

			/* Request irq for MSI vector 0.(default MSI) */
			if (i < PCIE_START_SEP_MSI_VEC) {
				sep_msi_vec[ch_num][i].is_used = true;
				ret = devm_request_irq(pci->dev, sep_msi_vec[ch_num][i].irq,
						msi_handler[i],
						IRQF_SHARED | IRQF_TRIGGER_HIGH,
						sep_irq_name[i], pp);
				if (ret) {
					pr_err("failed to request MSI%d irq\n", i);

					return ret;
				}
				continue;
			}
		}
	}
skip_sep_request_irq:

	/* pp->root_bus_nr = 0; */
	pp->ops = &exynos_pcie_rc_ops;
	pp->num_vectors = MAX_MSI_IRQS;

	/* Set pp->msi_irq to block MSI related requests in designware stack. */
	pp->msi_irq = -ENODEV;

	exynos_pcie_setup_rc(pp);
	ret = dw_pcie_host_init(pp);
	if (ret) {
		pcie_err(exynos_pcie, "failed to dw pcie host init\n");
		return ret;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
	vendor_id = val & ID_MASK;
	device_id = (val >> 16) & ID_MASK;
	exynos_pcie->pci_dev = pci_get_device(vendor_id, device_id, NULL);
	if (!exynos_pcie->pci_dev) {
		pcie_err(exynos_pcie, "Failed to get pci device\n");
		return -ENODEV;
	}
	pcie_info(exynos_pcie, "(%s): Exynos RC vendor/device id = 0x%x\n",
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
		msi_domain_info->ops->msi_finish = exynos_msi_setup_irq;
	}

	return 0;
}

static void exynos_pcie_rc_pcie_ops_init(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;

	pcie_info(exynos_pcie, "Initialize PCIe function.\n");

	pcie_ops->poweron = exynos_pcie_rc_poweron;
	pcie_ops->poweroff = exynos_pcie_rc_poweroff;
	pcie_ops->rd_own_conf = exynos_pcie_rc_rd_own_conf;
	pcie_ops->wr_own_conf = exynos_pcie_rc_wr_own_conf;
	pcie_ops->rd_other_conf = exynos_pcie_rc_rd_other_conf;
	pcie_ops->wr_other_conf = exynos_pcie_rc_wr_other_conf;
}

static int exynos_pcie_rc_make_reg_tb(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	unsigned int pos, val, id;
	int i;

	/* initialize the reg table */
	for (i = 0; i < PCI_MAX_CAP_NUM; i++) {
		exynos_pcie->pci_cap[i] = 0;
		exynos_pcie->pci_ext_cap[i] = 0;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_CAPABILITY_LIST, 4, &val);
	pos = 0xFF & val;

	while (pos) {
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		id = val & CAP_ID_MASK;
		if (id >= PCI_MAX_CAP_NUM) {
			pcie_err(exynos_pcie, "Unexpected PCI_CAP number!\n");
			break;
		}
		exynos_pcie->pci_cap[id] = pos;
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		pos = (val & CAP_NEXT_OFFSET_MASK) >> 8;
		pcie_dbg(exynos_pcie, "Next Cap pointer : 0x%x\n", pos);
	}

	pos = PCI_CFG_SPACE_SIZE;

	while (pos) {
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		if (val == 0) {
			pcie_info(exynos_pcie, "we have no ext capabilities!\n");
			break;
		}
		id = PCI_EXT_CAP_ID(val);
		if (id >= PCI_MAX_CAP_NUM) {
			pcie_err(exynos_pcie, "Unexpected PCI_EXT_CAP number!\n");
			break;
		}
		exynos_pcie->pci_ext_cap[id] = pos;
		pos = PCI_EXT_CAP_NEXT(val);
		pcie_dbg(exynos_pcie, "Next ext Cap pointer : 0x%x\n", pos);
	}

	for (i = 0; i < PCI_MAX_CAP_NUM; i++) {
		if (exynos_pcie->pci_cap[i])
			pcie_info(exynos_pcie, "PCIe cap [0x%x][%s]: 0x%x\n",
						i, CAP_ID_NAME(i), exynos_pcie->pci_cap[i]);
	}
	for (i = 0; i < PCI_MAX_CAP_NUM; i++) {
		if (exynos_pcie->pci_ext_cap[i])
			pcie_info(exynos_pcie, "PCIe ext cap [0x%x][%s]: 0x%x\n",
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
	struct pcie_port *pp = &pci->pp;
	dma_addr_t temp_addr;
	struct exynos_pcie_register_event temp_event;
	int irq_num;
	u32 restore_ip_ver = 0;

	exynos_pcie_save_rmem(exynos_pcie);
	temp_addr = get_msi_base(exynos_pcie->ch_num);
	exynos_pcie_rc_set_outbound_atu(0, 0x27200000, 0x0, SZ_1M);
	exynos_pcie_rc_print_msi_register(exynos_pcie->ch_num);
	exynos_pcie_rc_msi_init_wifi_shdmem(pp);
	exynos_pcie_rc_msi_init_cp(pp);
	exynos_pcie_rc_speedchange(exynos_pcie->ch_num, 1);
	restore_ip_ver = exynos_pcie->ip_ver;
	exynos_pcie->ip_ver = 0x990000;
	exynos_pcie_rc_lanechange(exynos_pcie->ch_num, 1);
	exynos_pcie->ip_ver = restore_ip_ver;

	/* Check related L1ss functions */
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie_config_l1ss_bcm_wifi(pp);
	exynos_pcie_config_l1ss_qc_wifi(pp);
	exynos_pcie_set_l1ss_samsung(1, pp, PCIE_L1SS_CTRL_TEST);
	exynos_pcie_set_l1ss_wifi(1, pp, PCIE_L1SS_CTRL_TEST);
	exynos_pcie->l1ss_ctrl_id_state = PCIE_L1SS_CTRL_TEST;
	exynos_pcie_config_l1ss_bcm_wifi(pp);
	exynos_pcie_config_l1ss_qc_wifi(pp);
	exynos_pcie_set_l1ss_samsung(0, pp, PCIE_L1SS_CTRL_TEST);
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie_set_l1ss_wifi(0, pp, PCIE_L1SS_CTRL_TEST);
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
	exynos_pcie_msi0_handler(irq_num, pp);
	exynos_pcie_msi1_handler(irq_num, pp);
	exynos_pcie_msi2_handler(irq_num, pp);
	exynos_pcie_msi3_handler(irq_num, pp);
	exynos_pcie_msi4_handler(irq_num, pp);

	exynos_pcie_rc_ia(exynos_pcie);
}

/* SOC specific control after DT parsing */
static void exynos_pcie_soc_specific_ctrl(struct exynos_pcie *exynos_pcie)
{
	if ((exynos_pcie->ip_ver & EXYNOS_PCIE_AP_MASK) == 0x993500) {
		if (exynos_soc_info.main_rev == 1) {
			pcie_info(exynos_pcie, "Exynos9935 EVT1 doesn't need I/A\n");
			exynos_pcie->use_ia = false;
		}
	}
}

static int exynos_pcie_rc_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int ch_num;

	dev_info(&pdev->dev, "## PCIe RC PROBE start \n");

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
	exynos_pcie->info = of_device_get_match_data(&pdev->dev);

	pci->dev = &pdev->dev;
	pci->ops = &dw_pcie_ops;

	pp = &pci->pp;

	exynos_pcie->ep_pci_bus = NULL;
	exynos_pcie->ch_num = ch_num;
	exynos_pcie->state = STATE_LINK_DOWN;

	exynos_pcie->linkdown_cnt = 0;
	exynos_pcie->boot_cnt = exynos_pcie->info->skip_boot_l1ss_ctrl;
	if (exynos_pcie->boot_cnt)
		dev_info(&pdev->dev, "Skip boot L1ss control\n");
	exynos_pcie->work_l1ss_cnt = 0;
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie->cpl_timeout_recovery = 0;
	exynos_pcie->sudden_linkdown = 0;
	exynos_pcie->probe_done = 0;
	if (exynos_pcie->info->tpoweron_time == 0x0) {
		dev_info(&pdev->dev, "WARN : There is no tpoweron_time.\n");
		ret = -EINVAL;
		goto probe_fail;
	}

	exynos_pcie->app_req_exit_l1 = PCIE_APP_REQ_EXIT_L1;
	exynos_pcie->app_req_exit_l1_mode = PCIE_APP_REQ_EXIT_L1_MODE;
	exynos_pcie->linkup_offset = PCIE_ELBI_RDLH_LINKUP;

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	platform_set_drvdata(pdev, exynos_pcie);

	/* parsing pcie dts data for exynos */
	ret = exynos_pcie_rc_parse_dt(&pdev->dev, exynos_pcie);
	if (ret)
		goto probe_fail;

	exynos_pcie_soc_specific_ctrl(exynos_pcie);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	ret = exynos_pcie_rc_get_pin_state(pdev, exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_get_resource(pdev, exynos_pcie);
	if (ret)
		goto probe_fail;

	spin_lock_init(&exynos_pcie->reg_lock);
	spin_lock_init(&exynos_pcie->conf_lock);
	spin_lock_init(&exynos_pcie->pcie_l1_exit_lock);

	/* Mapping PHY functions */
	ret = exynos_pcie_rc_phy_init(exynos_pcie);
	if (ret)
		goto probe_fail;

	exynos_pcie_rc_pcie_ops_init(exynos_pcie);

	regmap_update_bits(exynos_pcie->pmureg,
			exynos_pcie->pmu_phy_ctrl,
			PCIE_PHY_CONTROL_MASK1, (0x0 << 5));

	regmap_update_bits(exynos_pcie->pmureg,
			exynos_pcie->pmu_phy_ctrl,
			PCIE_PHY_CONTROL_MASK2, (0x0 << 0));

	exynos_pcie_rc_resumed_phydown(pp);

	ret = exynos_pcie_rc_make_reg_tb(&pdev->dev, exynos_pcie);
	if (ret)
		goto probe_fail;

	if (exynos_pcie->info->save_rmem)
		exynos_pcie->info->save_rmem(exynos_pcie);

	ret = exynos_pcie_rc_add_port(pdev, pp, ch_num);
	if (ret)
		goto probe_fail;

	if (dev_is_dma_coherent(pci->dev))
		exynos_pcie_rc_set_iocc(pp, 1);

	disable_irq(pp->irq);

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	exynos_pcie->idle_ip_index =
			exynos_get_idle_ip_index(dev_name(&pdev->dev),
						 dev_is_dma_coherent(pci->dev));
	if (exynos_pcie->idle_ip_index < 0) {
		pcie_err(exynos_pcie, "Cant get idle_ip_dex!!!\n");
		ret = -EINVAL;
		goto probe_fail;
	}

	exynos_update_ip_idle_status(exynos_pcie->idle_ip_index, PCIE_IS_IDLE);
	pcie_info(exynos_pcie, "%s, ip idle status : %d, idle_ip_index: %d \n",
		  __func__, PCIE_IS_IDLE, exynos_pcie->idle_ip_index);
#endif

	exynos_pcie->pcie_wq = create_singlethread_workqueue("pcie_wq");
	if (IS_ERR(exynos_pcie->pcie_wq)) {
		pcie_err(exynos_pcie, "couldn't create workqueue\n");
		ret = EBUSY;
		goto probe_fail;
	}

	INIT_DELAYED_WORK(&exynos_pcie->dislink_work, exynos_pcie_rc_dislink_work);
	INIT_DELAYED_WORK(&exynos_pcie->cpl_timeout_work, exynos_pcie_rc_cpl_timeout_work);

	if (exynos_pcie->info->l1ss_delayed_work) {
		pcie_info(exynos_pcie, "%s: Initialize L1SS work delay\n", __func__);
		INIT_DELAYED_WORK(&exynos_pcie->l1ss_boot_delay_work,
				  exynos_pcie_work_l1ss);
	}

	pcie_sysmmu_add_pcie_fault_handler(exynos_pcie_rc_all_dump);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	exynos_pcie->itmon_nb.notifier_call = exynos_pcie_rc_itmon_notifier;
	itmon_notifier_chain_register(&exynos_pcie->itmon_nb);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PM)
	if (exynos_pcie->use_pcieon_sleep) {
		pcie_info(exynos_pcie, "## register pcie connection function\n");
		register_pcie_is_connect(pcie_linkup_stat);//tmp
	}
#endif

	/* force set input clk path change to disable */
	if (exynos_pcie->phy->phy_ops.phy_input_clk_change != NULL)
		exynos_pcie->phy->phy_ops.phy_input_clk_change(exynos_pcie, 0);

probe_fail:
	pm_runtime_put_sync(&pdev->dev);

	if (ret)
		pcie_err(exynos_pcie, "## %s: PCIe probe failed\n", __func__);
	else {
		exynos_pcie->probe_done = 1;
		pcie_info(exynos_pcie, "## %s: PCIe probe success\n", __func__);
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

	pcie_info(exynos_pcie, "## SUSPEND[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			exynos_pcie->pcie_is_linkup);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info(exynos_pcie, "%s: RC%d already off\n", __func__, exynos_pcie->ch_num);
		pm_runtime_put_sync(pci->dev);
		return 0;
	}

	if (!exynos_pcie->use_pcieon_sleep) { //PCIE WIFI Channel - it doesn't use PCIe on sleep.
		pcie_err(exynos_pcie,"WARNNING - Unexpected PCIe state(try to power OFF)\n");
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
	}
	/* To block configuration register by PCI stack driver
	else {
		pcie_info(exynos_pcie, "PCIe on sleep\n");
		pcie_info(exynos_pcie, "Default must_resume value : %d\n",
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

	pcie_info(exynos_pcie, "## RESUME[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			exynos_pcie->pcie_is_linkup);

	if (!exynos_pcie->pci_dev) {
		pcie_err(exynos_pcie, "Failed to get pci device\n");
	} else {
		exynos_pcie->pci_dev->state_saved = false;
		pcie_dbg(exynos_pcie, "state_saved flag = false");
	}

	pm_runtime_get_sync(pci->dev);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pcie_info(exynos_pcie, "%s: RC%d Link down state-> phypwr off\n", __func__,
							exynos_pcie->ch_num);
		exynos_pcie_rc_resumed_phydown(&pci->pp);
	}

	return 0;
}

static int exynos_pcie_suspend_prepare(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	pcie_info(exynos_pcie, "%s - PCIe(HSI) block enable to prepare suspend.\n", __func__);
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

	if (exynos_pcie->ip_ver >= 0x993500) {
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

static const struct exynos_pcie_info cp_info = {
	.msi_init = exynos_pcie_rc_msi_init_cp,
	.l1ss_set = exynos_pcie_set_l1ss_samsung,
	.l1ss_delayed_work = 0,
	.udelay_after_perst = 18000,
	.tpoweron_time = PORT_LINK_TPOWERON_180US,
};
static const struct exynos_pcie_info wifi_qc_info = {
	.msi_init = exynos_pcie_rc_msi_init_wifi,
	.l1ss_ep_cfg = exynos_pcie_config_l1ss_qc_wifi,
	.l1ss_set = exynos_pcie_set_l1ss_wifi,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.l1ss_delayed_work = 1,
	.udelay_before_perst = 10000,
	.udelay_after_perst = 18000,
	.linkdn_callback_direct = 1,
	.tpoweron_time = PORT_LINK_TPOWERON_180US,
};
static const struct exynos_pcie_info wifi_bcm_info = {
	.msi_init = exynos_pcie_rc_msi_init_wifi,
	.l1ss_ep_cfg = exynos_pcie_config_l1ss_bcm_wifi,
	.l1ss_set = exynos_pcie_set_l1ss_wifi,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.l1ss_delayed_work = 1,
	.udelay_after_perst = 20000, /* for bcm4389(Default : 18msec) */
	.tpoweron_time = PORT_LINK_TPOWERON_180US,
};

static const struct exynos_pcie_info wifi_ss_info = {
	.msi_init = exynos_pcie_rc_msi_init_wifi,
	.l1ss_set = exynos_pcie_set_l1ss_samsung,
	.set_dma_ops = exynos_pcie_set_dma_ops,
	.set_ia_code = exynos_pcie_rc_ia,
	.l1ss_delayed_work = 1,
	.udelay_after_perst = 18000,
	.mdelay_before_perst_low = 10,
	.mdelay_after_perst_low = 20,
	.skip_boot_l1ss_ctrl = 1, /* Skip disabling L1ss at booting time */
	.tpoweron_time = PORT_LINK_TPOWERON_180US,
};

static const struct of_device_id exynos_pcie_rc_of_match[] = {
	{ .compatible = "exynos-pcie-rc,cp_ss", .data = &cp_info },
	{ .compatible = "exynos-pcie-rc,wifi_qc", .data = &wifi_qc_info },
	{ .compatible = "exynos-pcie-rc,wifi_bcm", .data = &wifi_bcm_info },
	{ .compatible = "exynos-pcie-rc,wifi_ss", .data = &wifi_ss_info },
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
