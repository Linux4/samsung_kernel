// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include "sugov/cpufreq.h"
#include "dsu_interface.h"

static void __iomem *clkg_sram_base_addr;
static void __iomem *wlc_sram_base_addr;
static void __iomem *l3ctl_sram_base_addr;

void __iomem *get_clkg_sram_base_addr(void)
{
	return clkg_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_clkg_sram_base_addr);

void __iomem *get_l3ctl_sram_base_addr(void)
{
	return l3ctl_sram_base_addr;
}
EXPORT_SYMBOL_GPL(get_l3ctl_sram_base_addr);

int dsu_pwr_swpm_init(void)
{

	struct device_node *dev_node;
	struct platform_device *pdev_temp;
	struct resource *sram_res;
	int ret = 0;

	/* init l3ctl sram*/
	dev_node = of_find_node_by_name(NULL, "cpuqos-v3");
	if (!dev_node) {
		pr_info("failed to find node cpuqos-v3 @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find cpuqos_v3 pdev @ %s\n", __func__);
		return -EINVAL;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (sram_res) {
		l3ctl_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't get cpuqos_v3 resource\n", __func__);
		return -EINVAL;
	}

	/* init wlc sram */
	dev_node = of_find_node_by_name(NULL, "wl-info");
	if (!dev_node) {
		pr_info("failed to find node wl_info @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find wl_info pdev @ %s\n", __func__);
		return -EINVAL;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (sram_res) {
		wlc_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	} else {
		pr_info("%s can't get wl_info resource\n", __func__);
		return -EINVAL;
	}

	/* init clkg sram */
	dev_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (!dev_node) {
		pr_info("failed to find node cpuhvfs @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dev_node);
	if (!pdev_temp) {
		pr_info("failed to find cpuhvfs pdev @ %s\n", __func__);
		return -EINVAL;
	}

	sram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);

	if (sram_res)
		clkg_sram_base_addr = ioremap(sram_res->start,
				resource_size(sram_res));
	else {
		pr_info("%s can't get cpuhvfs resource\n", __func__);
		return -ENODEV;
	}

	return ret;
}

unsigned int get_pelt_dsu_bw(void)
{
	unsigned int pelt_dsu_bw;

	pelt_dsu_bw = ioread32(l3ctl_sram_base_addr+PELT_DSU_BW_OFFSET);

	return pelt_dsu_bw;
}
EXPORT_SYMBOL_GPL(get_pelt_dsu_bw);

unsigned int get_pelt_emi_bw(void)
{
	unsigned int pelt_emi_bw;

	pelt_emi_bw = ioread32(l3ctl_sram_base_addr+PELT_EMI_BW_OFFSET);

	return pelt_emi_bw;
}
EXPORT_SYMBOL_GPL(get_pelt_emi_bw);

/* write pelt weight and pelt sum */
void update_pelt_data(unsigned int pelt_weight, unsigned int pelt_sum)
{
	iowrite32(pelt_sum, l3ctl_sram_base_addr+PELT_SUM_OFFSET);
	iowrite32(pelt_weight, l3ctl_sram_base_addr+PELT_WET_OFFSET);
}
EXPORT_SYMBOL_GPL(update_pelt_data);

/* get workload type */
unsigned int get_wl(unsigned int wl_idx)
{
	unsigned int wl_type;
	unsigned int offset;

	wl_type = ioread32(wlc_sram_base_addr);
	offset = wl_idx * 0x8;
	wl_type = (wl_type >> offset) & 0xff;

	return wl_type;
}

