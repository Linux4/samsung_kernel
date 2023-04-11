/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Support SoC specific Reboot
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <soc/samsung/debug-snapshot.h>
#ifdef CONFIG_EXYNOS_ACPM
#include <soc/samsung/acpm_ipc_ctrl.h>
#endif
#include <soc/samsung/exynos-pmu.h>
#include <linux/mfd/samsung/s2mpu12-regulator.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define SWRESET				(0x2)
#define SYSTEM_CONFIGURATION		(0x3a00)
#define PS_HOLD_CONTROL			(0x030C)
#define EXYNOS_PMU_SYSIP_DAT0		(0x0810)
#define EXYNOS_PMU_DREXCAL7		(0x09BC)
#define EXYNOS_PMU_SYSIP_DAT0		(0x0810)

#define REBOOT_MODE_NORMAL		(0x00)
#define REBOOT_MODE_USER_FASTBOOTD	(0x05)
#define REBOOT_MODE_CHARGE		(0x04)
#define REBOOT_MODE_FASTBOOT		(0x03)
#define REBOOT_MODE_FACTORY             (0x02)
#define REBOOT_MODE_RECOVERY		(0x01)
#define REBOOT_MODE_MASK		((1 << 3) - 1)


void (*__arm_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);

struct exynos_reboot_variant {
	int ps_hold_reg;
	int ps_hold_data_bit;
	int swreset_reg;
	int swreset_bit;
	int reboot_mode_reg;
	void (*reboot)(enum reboot_mode mode, const char *cmd);
	void (*power_off)(void);
	void (*reset_control)(void);
	int (*power_key_chk)(void);
};

static struct exynos_exynos_reboot {
	struct device *dev;
	const struct exynos_reboot_variant *variant;
	struct regmap *pmureg;
	char reset_reason[SZ_32];
} exynos_reboot;

static const struct exynos_reboot_variant *exynos_reboot_get_variant(struct platform_device *pdev)
{
	const struct exynos_reboot_variant *variant = of_device_get_match_data(&pdev->dev);

	if (!variant) {
		/* Device matched by platform_device_id */
		variant = (const struct exynos_reboot_variant *)
			   platform_get_device_id(pdev)->driver_data;
	}

	return variant;
}

#if !IS_ENABLED(CONFIG_SEC_REBOOT)
static void exynos_power_off_v1(void)
{
	const struct exynos_reboot_variant *variant = exynos_reboot.variant;
	int poweroff_try = 0;

	dev_info(exynos_reboot.dev, "Power off(%d)\n", s2mpu12_read_pwron_status());

	while (1) {
		/* wait for power button release &&
		 * after exynos_acpm_reboot is called
		 * power on status cannot be read */
		if ((poweroff_try) || (!s2mpu12_read_pwron_status())) {
#ifdef CONFIG_EXYNOS_ACPM
			exynos_acpm_reboot();
#endif
			dbg_snapshot_scratch_clear();
			dev_emerg(exynos_reboot.dev, "Set PS_HOLD Low.\n");

			regmap_update_bits(exynos_reboot.pmureg, variant->ps_hold_reg, 1 << variant->ps_hold_data_bit, 0);
			++poweroff_try;
			dev_emerg(exynos_reboot.dev, "Should not reach here! (poweroff_try:%d)\n", poweroff_try);
		} else {
			/* if power button is not released, wait and check TA again */
			dev_info(exynos_reboot.dev, "PowerButton is not released.\n");
		}
		mdelay(1000);
	}
}
#endif

static void exynos_reboot_parse(void *cmd)
{
	const struct exynos_reboot_variant *variant = exynos_reboot.variant;

	if (cmd) {
		pr_info("Reboot command: (%s)\n", (char *)cmd);
		if (!strcmp((char *)cmd, "charge"))
			regmap_update_bits(exynos_reboot.pmureg, variant->reboot_mode_reg, REBOOT_MODE_MASK, REBOOT_MODE_CHARGE);
		else if (!strcmp((char *)cmd, "bootloader") || !strcmp((char *)cmd, "bl"))
			regmap_update_bits(exynos_reboot.pmureg, variant->reboot_mode_reg, REBOOT_MODE_MASK, REBOOT_MODE_FASTBOOT);
		else if (!strcmp((char *)cmd, "fastboot") || !strcmp((char *)cmd, "fb"))
			regmap_update_bits(exynos_reboot.pmureg, variant->reboot_mode_reg, REBOOT_MODE_MASK, REBOOT_MODE_USER_FASTBOOTD);
		else if (!strcmp((char *)cmd, "recovery"))
			regmap_update_bits(exynos_reboot.pmureg, variant->reboot_mode_reg, REBOOT_MODE_MASK, REBOOT_MODE_RECOVERY);
		else if (!strcmp((char *)cmd, "sfactory"))
			regmap_update_bits(exynos_reboot.pmureg, variant->reboot_mode_reg, REBOOT_MODE_MASK, REBOOT_MODE_FACTORY);
		else
			regmap_update_bits(exynos_reboot.pmureg, variant->reboot_mode_reg, REBOOT_MODE_MASK, REBOOT_MODE_NORMAL);
	}
}

int  exynos_restart_v1(struct notifier_block *this, unsigned long mode, void *cmd)
{
	const struct exynos_reboot_variant *variant = exynos_reboot.variant;
	u32 soc_id, revision;

#ifdef CONFIG_EXYNOS_ACPM
	exynos_acpm_reboot();
#endif
	exynos_reboot_parse(cmd);

	if (variant->reset_control)
		variant->reset_control();
	/* Check by each SoC */
	soc_id = exynos_soc_info.product_id;
	revision = exynos_soc_info.revision;
	dev_info(exynos_reboot.dev, "SOC ID %X. Revision: %x\n", soc_id, revision);

	/* Do S/W Reset */
	dev_emerg(exynos_reboot.dev, "Exynos SoC reset right now\n");

	regmap_write(exynos_reboot.pmureg, variant->swreset_reg, 1 << variant->swreset_bit);
	/* Wait for S/W reset */
	dbg_snapshot_spin_func();
	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_SEC_REBOOT)
void exynos_mach_restart(const char *cmd)
{
	exynos_restart_v1(NULL, 0, (void *)cmd);
}
EXPORT_SYMBOL_GPL(exynos_mach_restart);
#endif

#if !IS_ENABLED(CONFIG_SEC_REBOOT)
static ssize_t reset_reason_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, SZ_32, "%s\n", exynos_reboot.reset_reason);
}
DEVICE_ATTR_RO(reset_reason);

static struct attribute *exynos_reboot_attrs[] = {
	&dev_attr_reset_reason.attr,
	NULL,
};

ATTRIBUTE_GROUPS(exynos_reboot);
#endif

static struct notifier_block exynos_restart_nb = {
        .notifier_call = exynos_restart_v1,
        .priority = 128,
};

static int exynos_reboot_probe(struct platform_device *pdev)
{
	struct resource *res;
	int err;

	//dev_set_socdata(&pdev->dev, "Exynos", "Reboot");

	exynos_reboot.dev = &pdev->dev;
	exynos_reboot.variant = exynos_reboot_get_variant(pdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	exynos_reboot.pmureg = syscon_regmap_lookup_by_phandle((exynos_reboot.dev)->of_node, "samsung,syscon-phandle");
	if (IS_ERR(exynos_reboot.pmureg))
		pr_warn("Fail to get regmap of PMU\n");

	if (!exynos_reboot.variant) {
		dev_err(&pdev->dev, "variant is null\n");
		return -ENODEV;
	}

	if (exynos_reboot.variant->reboot) {
		__arm_pm_restart = exynos_reboot.variant->reboot;
		dev_info(&pdev->dev, "Success to register arm_pm_restart\n");
	}

	if (exynos_reboot.variant->power_off) {
		pm_power_off = exynos_reboot.variant->power_off;
		dev_info(&pdev->dev, "Success to register pm_power_off\n");
	}
#if !IS_ENABLED(CONFIG_SEC_REBOOT)
	if (sysfs_create_groups(&pdev->dev.kobj, exynos_reboot_groups)) {
		dev_err(&pdev->dev, "failed create sysfs\n");
		return -ENOMEM;
	} else {
		dev_info(&pdev->dev, "Success to register sysfs\n");
	}
#endif
	err = register_restart_handler(&exynos_restart_nb);
	if (err) {
		dev_err(&pdev->dev, "cannot register restart handler (err=%d)\n",
				err);
	}

	return 0;
}

static const struct exynos_reboot_variant drv_data_v1 = {
	.ps_hold_reg = PS_HOLD_CONTROL,
	.ps_hold_data_bit = 8,
	.swreset_reg = SYSTEM_CONFIGURATION,
	.swreset_bit = 1,
	.reboot_mode_reg = EXYNOS_PMU_SYSIP_DAT0,
	//.reboot = exynos_restart_v1,
#if !IS_ENABLED(CONFIG_SEC_REBOOT)
	.power_off = exynos_power_off_v1,
#endif
};

static const struct of_device_id exynos_reboot_match[] = {
	{
		.compatible = "exynos,reboot-v1",
		.data = &drv_data_v1,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_reboot_match);

static const struct platform_device_id exynos_reboot_ids[] = {
	{
		.name = "exynos-reboot",
		.driver_data = (unsigned long)&drv_data_v1,
	},
	{},
};

static struct platform_driver exynos_reboot_driver = {
	.probe		= exynos_reboot_probe,
	.driver = {
		.name = "exynos-reboot",
		.owner = THIS_MODULE,
		.of_match_table	= of_match_ptr(exynos_reboot_match),
#if !IS_ENABLED(CONFIG_SEC_REBOOT)
		.groups = exynos_reboot_groups,
#endif
	},
	.id_table	= exynos_reboot_ids,
};

static int reset_reason_setup(char *str)
{
	strncpy(exynos_reboot.reset_reason, str, strlen(str));

	return 1;
}
__setup("androidboot.bootreason=", reset_reason_setup);
module_platform_driver(exynos_reboot_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Khalid Shaik <khalid.s@samsung.com>");
