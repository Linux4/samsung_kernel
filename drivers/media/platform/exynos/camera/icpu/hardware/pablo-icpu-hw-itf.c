// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/fs.h>
#include <linux/clk.h>

#include "pablo-icpu.h"
#include "pablo-icpu-hw-itf.h"
#include "pablo-icpu-hw.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-HW-ITF]",
};

struct icpu_logger *get_icpu_hw_itf_log(void)
{
	return &_log;
}

static struct icpu_hw hw;

int pablo_icpu_hw_set_base_address(struct icpu_platform_data *pdata, u32 dst_addr)
{
	if (!pdata)
		return -EINVAL;

	ICPU_INFO("firmware remap base address: 0x%x", dst_addr);
	return HW_OPS(set_base_addr, pdata->mcuctl_reg_base, dst_addr);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_set_base_address);

void pablo_icpu_hw_misc_prepare(struct icpu_platform_data *pdata)
{
	HW_OPS(hw_misc_prepare);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_misc_prepare);

int pablo_icpu_hw_reset(struct icpu_platform_data *pdata, int on)
{
	if (!pdata)
		return -EINVAL;

	return HW_OPS(reset, pdata->sysreg_reset, pdata->sysreg_reset_bit, on);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_reset);

int pablo_icpu_hw_wait_teardown_timeout(struct icpu_platform_data *pdata, u32 ms)
{
	if (!pdata)
		return -EINVAL;

	return HW_OPS(wait_wfi_state_timeout, pdata->mcuctl_reg_base, ms);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_wait_teardown_timeout);

void pablo_icpu_hw_force_powerdown(struct icpu_platform_data *pdata)
{
	int clk_prepare_err = -1;
	int clk_enable_err = -1;

	if (!pdata)
		return;

	/* Clock api has strict sequence.
	   Prepare and enable clk to disable it */
	if (!IS_ERR(pdata->clk))
		clk_prepare_err = clk_prepare(pdata->clk);

	if (!clk_prepare_err)
		clk_enable_err = clk_enable(pdata->clk);

	HW_OPS(force_powerdown, pdata->num_force_powerdown_step, pdata->force_powerdown_seq);

	if (!clk_enable_err)
		clk_disable(pdata->clk);

	if (!clk_prepare_err)
		clk_unprepare(pdata->clk);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_force_powerdown);

void pablo_icpu_hw_panic_handler(struct icpu_platform_data *pdata)
{
	if (!pdata)
		return;

	HW_OPS(panic_handler, pdata->sysctrl_reg_base);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_panic_handler);

u32 pablo_icpu_hw_get_num_tx_mbox(struct icpu_platform_data *pdata)
{
	if (!pdata)
		return 0;

	return pdata->num_tx_mbox;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_get_num_tx_mbox);

void *pablo_icpu_hw_get_tx_info(struct icpu_platform_data *pdata, u32 index)
{
	if (!pdata)
		return NULL;

	return &pdata->tx_infos[index];
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_get_tx_info);

u32 pablo_icpu_hw_get_num_rx_mbox(struct icpu_platform_data *pdata)
{
	if (!pdata)
		return 0;

	return pdata->num_rx_mbox;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_get_num_rx_mbox);

void *pablo_icpu_hw_get_rx_info(struct icpu_platform_data *pdata, u32 index)
{
	if (!pdata)
		return NULL;

	return &pdata->rx_infos[index];
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_get_rx_info);

u32 pablo_icpu_hw_get_num_channels(struct icpu_platform_data *pdata)
{
	if (!pdata)
		return 0;

	return pdata->num_chans;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_get_num_channels);

void pablo_icpu_hw_set_debug_reg(struct icpu_platform_data *pdata, u32 index, u32 val)
{
	if (!pdata)
		return;

	HW_OPS(set_debug_reg, pdata->sysctrl_reg_base, index, val);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_hw_set_debug_reg);

static int __get_force_powerdown_sequence(struct device_node *np, struct icpu_platform_data *pdata)
{
	int ret;
	u32 tmp;
	int i, k;
	u32 *args;
	u32 offset;

	if (!of_get_property(np, "force-powerdown-seq", &tmp)) {
		ICPU_ERR("fail to get force-powerdown-seq property");
		return -EINVAL;
	}

	pdata->num_force_powerdown_step = tmp / (sizeof(u32) * ICPU_IO_SEQUENCE_LEN);

	pdata->force_powerdown_seq =
		kzalloc(sizeof(struct icpu_io_sequence) * pdata->num_force_powerdown_step, GFP_KERNEL);
	if (!pdata->force_powerdown_seq) {
		ICPU_ERR("fail to alloc");
		return -ENOMEM;
	}

	for (i = 0; i < pdata->num_force_powerdown_step; i++) {
		offset = i * ICPU_IO_SEQUENCE_LEN;
		args = (u32 *)&pdata->force_powerdown_seq[i];

		for (k = 0; k < ICPU_IO_SEQUENCE_LEN; k++) {
			if (of_property_read_u32_index(np, "force-powerdown-seq", offset + k, &args[k])) {
				ICPU_ERR("icpu fail to get arg, offset(%d), k(%d)", offset , k);
				ret = -EINVAL;
				goto error;
			}
		}

		ICPU_INFO("[%d] 0x%x 0x%x 0x%x 0x%x %d", i,
				pdata->force_powerdown_seq[i].type,
				pdata->force_powerdown_seq[i].addr,
				pdata->force_powerdown_seq[i].mask,
				pdata->force_powerdown_seq[i].val,
				pdata->force_powerdown_seq[i].timeout);
	}

	return 0;

error:
	kfree(pdata->force_powerdown_seq);
	pdata->force_powerdown_seq = NULL;

	return ret;
}

int pablo_icpu_hw_probe(void *pdev)
{
	int ret;
	struct device *dev;
	struct device_node *np;
	struct device_node *btx_np;
	struct device_node *tx_np;
	struct device_node *brx_np;
	struct device_node *rx_np;
	u32 num_mbox;
	u32 max_data_len;
	u32 data_offset;
	u32 int_enable_offset;
	u32 int_gen_offset;
	u32 int_status_offset;
	int irq;
	struct resource *res;

	struct icpu_platform_data *pdata;

	dev = &((struct platform_device *)pdev)->dev;
	np = dev->of_node;

	pdata = kzalloc(sizeof(struct icpu_platform_data), GFP_KERNEL);
	if (!pdata) {
		ICPU_ERR("no memory for platform data");
		return -ENOMEM;
	}

	/* mcuctl reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mcuctl");
	if (!res) {
		ICPU_ERR("failed to get memory resource for mcuctl");
		goto alloc_fail;
	}
	pdata->mcuctl_reg_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pdata->mcuctl_reg_base) {
		ICPU_ERR("can't ioremap for mcuctl");
		goto alloc_fail;
	}

	/* sysctrl reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysctrl");
	if (!res) {
		ICPU_ERR("failed to get memory resource for sysctrl");
		goto alloc_fail;
	}
	pdata->sysctrl_reg_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pdata->sysctrl_reg_base) {
		ICPU_ERR("can't ioremap for sysctrl");
		goto alloc_fail;
	}

	/* cstore reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cstore");
	if (!res) {
		ICPU_ERR("failed to get memory resource for cstore");
		goto alloc_fail;
	}
	pdata->cstore_reg_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pdata->cstore_reg_base) {
		ICPU_ERR("can't ioremap for cstore");
		goto alloc_fail;
	}

	/* sysreg reset */
	pdata->sysreg_reset = syscon_regmap_lookup_by_phandle(np,
						"samsung,reset-sysreg");
	if (IS_ERR(pdata->sysreg_reset)) {
		pdata->sysreg_reset = NULL;
	} else {
		ret = of_property_read_u32(np, "reset", &pdata->sysreg_reset_bit);
		if (ret) {
			ICPU_ERR("cannot get ICPU sysreg reset bit");
			goto alloc_fail;
		}
	}

	pdata->clk = devm_clk_get(dev, "GATE_ICPU_QCH_CPU0");
	if (IS_ERR(pdata->clk)) {
		if (PTR_ERR(pdata->clk) != -ENOENT) {
			ICPU_ERR("Failed to get clock: %ld", PTR_ERR(pdata->clk));
			goto alloc_fail;
		}
		ICPU_INFO("'GATE_ICPU_QCH_CPU0' clock is not present");
	}

	btx_np = of_get_child_by_name(np, "tx_mbox");
	if (btx_np) {
		int i;
		char *name = __getname();

		if (!name)
			goto alloc_fail;

		of_property_read_u32(btx_np, "num-mbox", &num_mbox);
		ICPU_INFO("tx-mode num-mbox %d", num_mbox);

		pdata->num_tx_mbox = num_mbox;

		pdata->tx_infos = kzalloc(sizeof(struct icpu_mbox_tx_info) * num_mbox, GFP_KERNEL);
		if (!pdata->tx_infos) {
			__putname(name);
			goto alloc_fail;
		}

		for (i = 0; i < num_mbox; i++) {
			sprintf(name, "tx_mbox_%d", i);
			tx_np = of_get_child_by_name(btx_np, name);

			of_property_read_u32(tx_np, "int-enable-offset", &int_enable_offset);
			of_property_read_u32(tx_np, "int-gen-offset", &int_gen_offset);
			of_property_read_u32(tx_np, "int-status-offset", &int_status_offset);
			of_property_read_u32(tx_np, "max-data-len", &max_data_len);
			of_property_read_u32(tx_np, "data-offset", &data_offset);

			pdata->tx_infos[i].int_enable_reg = pdata->sysctrl_reg_base + int_enable_offset;
			pdata->tx_infos[i].int_gen_reg = pdata->sysctrl_reg_base + int_gen_offset;
			pdata->tx_infos[i].int_status_reg = pdata->sysctrl_reg_base + int_status_offset;
			pdata->tx_infos[i].data_reg = pdata->sysctrl_reg_base + data_offset;
			pdata->tx_infos[i].data_max_len = max_data_len;

			ICPU_INFO("int-enable-offset 0x%x", int_enable_offset);
			ICPU_INFO("int-gen-offset 0x%x", int_gen_offset);
			ICPU_INFO("int-status-offset 0x%x", int_status_offset);
			ICPU_INFO("max-data-len %d", max_data_len);
			ICPU_INFO("data-offset 0x%x", data_offset);
		}
		__putname(name);
	}

	brx_np = of_get_child_by_name(np, "rx_mbox");
	if (brx_np) {
		int i;
		char *name = __getname();

		if (!name)
			goto alloc_fail;

		of_property_read_u32(brx_np, "num-mbox", &num_mbox);
		ICPU_INFO("rx-mode num-mbox %d", num_mbox);

		pdata->num_rx_mbox = num_mbox;

		pdata->rx_infos = kzalloc(sizeof(struct icpu_mbox_rx_info) * num_mbox, GFP_KERNEL);
		if (!pdata->rx_infos) {
			__putname(name);
			goto alloc_fail;
		}

		for (i = 0; i < num_mbox; i++) {
			sprintf(name, "rx_mbox_%d%c", i, '\0');
			rx_np = of_get_child_by_name(brx_np, name);

			of_property_read_u32(rx_np, "int-status-offset", &int_status_offset);
			of_property_read_u32(rx_np, "max-data-len", &max_data_len);
			of_property_read_u32(rx_np, "data-offset", &data_offset);
			irq = platform_get_irq_byname(pdev, "isp-cpu-rx-mbox");

			pdata->rx_infos[i].int_status_reg = pdata->sysctrl_reg_base + int_status_offset;
			pdata->rx_infos[i].data_reg = pdata->sysctrl_reg_base + data_offset;
			pdata->rx_infos[i].data_max_len = max_data_len;
			pdata->rx_infos[i].irq = irq;

			ICPU_INFO("irq 0x%x", irq);
			ICPU_INFO("int-status-offset 0x%x", int_status_offset);
			ICPU_INFO("max-data-len %d", max_data_len);
			ICPU_INFO("data-offset 0x%x", data_offset);
		}
		__putname(name);
	}

	pdata->num_chans = pdata->num_tx_mbox + pdata->num_rx_mbox;

	ret = __get_force_powerdown_sequence(np, pdata);
	if (ret)
		goto alloc_fail;

	icpu_hw_init(&hw);

	dev->platform_data = pdata;

	return 0;

alloc_fail:
	kfree(pdata->tx_infos);
	kfree(pdata->rx_infos);
	kfree(pdata->force_powerdown_seq);
	kfree(pdata);

	return -ENOMEM;
}

void pablo_icpu_hw_remove(struct icpu_platform_data *pdata)
{
	if (!pdata)
		return;

	kfree(pdata->tx_infos);
	kfree(pdata->rx_infos);
	kfree(pdata->force_powerdown_seq);
	kfree(pdata);
}
