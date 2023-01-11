/*
 * linux/drivers/marvell/mmp-debug.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/kexec.h>
#include <linux/signal.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <asm/cacheflush.h>
#include <asm/system_misc.h>

#define DRVNAME				"axi timeout"
#define FAB_TIMEOUT_CTRL		0x60
#define FAB_TIMEOUT_STATUS0		0x68
#define FAB_TIMEOUT_STATUS1		0x70

#define STATE_HOLD_CTRL		0x88
#define FABTIMEOUT_HELD_WSTATUS	0x78
#define FABTIMEOUT_HELD_RSTATUS	0x80

#define DVC_HELD_STATUS_V1	0xB0
#define FCDONE_HELD_STATUS_V1	0xB8
#define PMULPM_HELD_STATUS_V1	0xC0
#define CORELPM_HELD_STATUS_V1	0xC8

#define DVC_HELD_STATUS_V2	0xB8
#define FCDONE_HELD_STATUS_V2	0xC0
#define PMULPM_HELD_STATUS_V2	0xC8
#define CORELPM_HELD_STATUS_V2	0x100
#define PLL_HELD_STATUS_V2	0x108

#define INT_MODE			0x1
#define DATA_ABORT_MODE			0x2

struct held_status {
	u32 fabws;
	u32 fabrs;
	u32 dvcs;
	u32 fcdones;
	u32 pmulpms;
	u32 corelpms;
	/* pll status is only supported on ULC1 */
	u32 plls;
};

enum mmp_debug_version {
	MMP_DEBUG_VERSION_1 = 1,
	MMP_DEBUG_VERSION_2,
};

void __attribute__((weak)) set_emmd_indicator(void) { }

static void __iomem *squ_base;
static unsigned int version;

static u32 timeout_write_addr, timeout_read_addr;
static u32 finish_save_cpu_ctx;
static unsigned int  err_fsr;
static unsigned long err_addr;

static struct held_status recorded_helds;

static int axi_timeout_check(void)
{
	timeout_write_addr = readl_relaxed(squ_base + FAB_TIMEOUT_STATUS0);
	timeout_read_addr = readl_relaxed(squ_base + FAB_TIMEOUT_STATUS1);

	if ((timeout_write_addr & 0x1) || (timeout_read_addr & 0x1))
		return 0;

	return 1;
}

static void setup_crash_info(struct pt_regs *regs)
{
	struct pt_regs fixed_regs;

	set_emmd_indicator();

	keep_silent = 1;
	crash_setup_regs(&fixed_regs, regs);
	crash_save_vmcoreinfo();
	machine_crash_shutdown(&fixed_regs);
	finish_save_cpu_ctx = 1;

	flush_cache_all();

	/* Waiting wdt to reset the Soc */
	while (1)
		;
}

static int axi_timeout_data_abort_handler(unsigned long addr, unsigned int fsr,
						struct pt_regs *regs)
{
	if (axi_timeout_check())
		return 0;

	err_fsr = fsr;
	err_addr = addr;

	setup_crash_info(regs);

	return 0;
}

static irqreturn_t axi_timeout_int_handler(int irq, void *dev_id)
{
	if (axi_timeout_check())
		return IRQ_HANDLED;

	setup_crash_info(NULL);

	return IRQ_HANDLED;
}


/* dump Fabric/LPM/DFC/DVC held status and enable held feature */
static int __init mmp_dump_heldstatus(void __iomem *squ_base)
{
	recorded_helds.fabws =
		readl_relaxed(squ_base + FABTIMEOUT_HELD_WSTATUS);
	recorded_helds.fabrs =
		readl_relaxed(squ_base + FABTIMEOUT_HELD_RSTATUS);

	if (version == MMP_DEBUG_VERSION_1) {
		recorded_helds.dvcs =
			readl_relaxed(squ_base + DVC_HELD_STATUS_V1);
		recorded_helds.fcdones =
			readl_relaxed(squ_base + FCDONE_HELD_STATUS_V1);
		recorded_helds.pmulpms =
			readl_relaxed(squ_base + PMULPM_HELD_STATUS_V1);
		recorded_helds.corelpms =
			readl_relaxed(squ_base + CORELPM_HELD_STATUS_V1);
	} else if (version == MMP_DEBUG_VERSION_2) {
		recorded_helds.dvcs =
			readl_relaxed(squ_base + DVC_HELD_STATUS_V2);
		recorded_helds.fcdones =
			readl_relaxed(squ_base + FCDONE_HELD_STATUS_V2);
		recorded_helds.pmulpms =
			readl_relaxed(squ_base + PMULPM_HELD_STATUS_V2);
		recorded_helds.corelpms =
			readl_relaxed(squ_base + CORELPM_HELD_STATUS_V2);
		recorded_helds.plls =
			readl_relaxed(squ_base + PLL_HELD_STATUS_V2);
	} else
		/* should never get here */
		pr_err("Unsupported mmp-debug version!\n");

	/* after register dump, then enable the held feature for debug */
	writel_relaxed(0x1, squ_base + STATE_HOLD_CTRL);

	pr_info("*************************************\n");
	pr_info("Fabric/LPM/DFC/DVC held status dump:\n");

	if (recorded_helds.fabws & 0x1)
		pr_info("AXI time out occurred when write address 0x%x!!!\n",
			recorded_helds.fabws & 0xfffffffc);

	if (recorded_helds.fabrs & 0x1)
		pr_info("AXI time out occurred when read address 0x%x!!!\n",
			recorded_helds.fabrs & 0xfffffffc);

	pr_info("DVC[%x]\n", recorded_helds.dvcs);
	pr_info("FCDONE[%x]\n", recorded_helds.fcdones);
	pr_info("PMULPM[%x]\n", recorded_helds.pmulpms);
	pr_info("CORELPM[%x]\n", recorded_helds.corelpms);
	if (version == MMP_DEBUG_VERSION_2)
		pr_info("PLL[%x]\n", recorded_helds.plls);

	pr_info("*************************************\n");
	return 0;
}

static const struct of_device_id axi_timeout_dt_match[] = {
	{.compatible = "marvell,mmp-debug"},
	{},
};

MODULE_DEVICE_TABLE(of, axi_timeout_dt_match);

static int __ref axi_timeout_probe(struct platform_device *pdev)
{
	u32 tmp, modes;
	int irq, i, ret = 0;
	struct resource *res;
	struct cpumask mask_val;
	struct device_node *np = pdev->dev.of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "%s: no IO memory defined\n", __func__);
		ret = -ENOENT;
		goto failed;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		dev_warn(&pdev->dev, "%s: no IRQ defined\n", __func__);

	if (!devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), DRVNAME)) {
		dev_err(&pdev->dev, "can't request region for resource %pR\n", res);
		ret = -EINVAL;
		goto failed;
	}

	squ_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (squ_base == NULL) {
		dev_err(&pdev->dev, "%s: res %lx - %lx map failed\n", __func__,
				(unsigned long)res->start, (unsigned long)res->end);
		ret = -ENOMEM;
		goto failed;
	}

	ret = of_property_read_u32(np, "version", &version);
	if (ret < 0) {
		pr_err("get version failed, use version 1\n");
		version = MMP_DEBUG_VERSION_1;
	}

	if (of_property_read_u32(np, "detection_modes", &modes)) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "%s: detection modes not defined!\n", __func__);
		goto failed;
	}

	tmp = readl_relaxed(squ_base + FAB_TIMEOUT_CTRL);
	tmp |= (1 << 29 | 1 << 30 | 1 << 31);
	writel_relaxed(tmp, squ_base + FAB_TIMEOUT_CTRL);

	if (modes & INT_MODE) {
		tmp = readl_relaxed(squ_base + FAB_TIMEOUT_CTRL);
		tmp &= ~(1 << 29);
		writel_relaxed(tmp, squ_base + FAB_TIMEOUT_CTRL);

		ret = devm_request_irq(&pdev->dev, irq, axi_timeout_int_handler,
				IRQF_TRIGGER_RISING | IRQF_MULTI_CPUS, "axi_timeout", NULL);
		if (ret) {
			dev_err(&pdev->dev, "%s: Failed to request irq!\n", __func__);
			goto failed;
		}

		/* Route interrupt to ALL cpus */
		for (i = 0; i < nr_cpu_ids; i++)
			cpumask_set_cpu(i, &mask_val);
		irq_set_affinity(irq, &mask_val);
	}

	if (modes & DATA_ABORT_MODE) {
		tmp = readl_relaxed(squ_base + FAB_TIMEOUT_CTRL);
		tmp &= ~(1 << 31);
		writel_relaxed(tmp, squ_base + FAB_TIMEOUT_CTRL);

#ifdef CONFIG_ARM
		hook_fault_code(0x8, axi_timeout_data_abort_handler, SIGBUS,
				0, "external abort on non-linefetch");
		hook_fault_code(0xc, axi_timeout_data_abort_handler, SIGBUS,
				0, "external abort on translation");
		hook_fault_code(0xe, axi_timeout_data_abort_handler, SIGBUS,
				0, "external abort on translation");
		hook_fault_code(0x10, axi_timeout_data_abort_handler, SIGBUS,
				0, "unknown 16");
		hook_fault_code(0x16, axi_timeout_data_abort_handler, SIGBUS,
				BUS_OBJERR, "imprecise external abort");
#endif

#ifdef CONFIG_ARM64
		hook_fault_code(0x10, axi_timeout_data_abort_handler, SIGBUS,
				0, "synchronous external abort");
		hook_fault_code(0x11, axi_timeout_data_abort_handler, SIGBUS,
				0, "asynchronous external abort");
#endif
	}

	mmp_dump_heldstatus(squ_base);

failed:
	return ret;
}

static struct platform_driver axi_timeout_driver = {
	.probe = axi_timeout_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = DRVNAME,
		.of_match_table = of_match_ptr(axi_timeout_dt_match),
	},
};

module_platform_driver(axi_timeout_driver);

MODULE_DESCRIPTION("AXI TIMEOUT DRIVER");
MODULE_LICENSE("GPL");
