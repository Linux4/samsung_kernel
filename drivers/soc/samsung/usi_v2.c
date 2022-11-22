/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - USI v2(Universal Serial Interface version 2) driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

/* USI v2 mode */
#define I2C_SW_CONF		(1<<2)
#define SPI_SW_CONF		(1<<1)
#define UART_SW_CONF		(1<<0)

struct usi_v2_mode {
	int val;
	const char *name;
};

struct usi_v2_data {
	void __iomem	*base;
	int 		mode;
	int		ch_id;
	struct regmap	*sysreg;
	unsigned int	sys_ps_cfg;
	unsigned int 	weak_pud_en;
	unsigned int 	weak_pud_val;
	int		secure_mode;
};

static const struct usi_v2_mode usi_v2_modes[] = {
	{ .val = I2C_SW_CONF ,     .name = "i2c" },
	{ .val = SPI_SW_CONF ,     .name = "spi" },
	{ .val = UART_SW_CONF ,    .name = "uart" },
};

enum {
	NONSECURE_MODE = 0,
	SECURE_MODE = 1,
};

static int get_usi_v2_mode(const char* mode_name)
{
	size_t i;

	for (i = 0; i < sizeof(usi_v2_modes)/sizeof(struct usi_v2_mode); i++) {
		if (!strncmp(mode_name, usi_v2_modes[i].name,
				strlen(usi_v2_modes[i].name)))
			return usi_v2_modes[i].val;
	}
	return -1;
}

static int usi_v2_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	const char* mode_name;
	struct usi_v2_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(struct usi_v2_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "no memory to save usi_v2_data\n");
		return -ENOMEM;
	}

	data->ch_id = of_alias_get_id(pdev->dev.of_node, "usi");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->weak_pud_en = 0;
	ret = of_property_read_u32(node, "samsung,weak-pull-up-mask", &data->sys_ps_cfg);
	if (!ret) {
		ret = of_property_read_u32(node, "samsung,weak-pud-val", &data->weak_pud_val);
		if (!ret) {
			data->sysreg = syscon_regmap_lookup_by_phandle(node, "samsung,ps-phandle");
			if (IS_ERR(data->sysreg))
				dev_err(&pdev->dev, "failed to get sysreg phandle\n");
			else {
				regmap_update_bits(data->sysreg, 0x0, 0x3 << data->sys_ps_cfg,
						data->weak_pud_val<< data->sys_ps_cfg);
				data->weak_pud_en = 1;
			}
		}
	}

	if (of_property_read_string(node, "usi_v2_mode", &mode_name) < 0) {
		dev_info(&pdev->dev, "missing usi_v2 mode configuration\n");
	} else {
		data->mode = get_usi_v2_mode(mode_name);

		if (data->mode < 0) {
			dev_dbg(&pdev->dev, "wrong usi_v2 mode: %s\n", mode_name);
			return -EINVAL;
		}
	}

	platform_set_drvdata(pdev, data);

	if (of_get_property(node, "secure-mode", NULL)) {
		dev_info(&pdev->dev,
			"secure-mode. Accessing usi register isn't allowed\n");
		data->secure_mode = SECURE_MODE;
	} else
		data->secure_mode = NONSECURE_MODE;

	if (data->secure_mode == SECURE_MODE) {
		dev_info(&pdev->dev,
			"usi configuration for secure channel is skipped\n");
		return 0;
	}

	if (data->mode)
		writel(data->mode, data->base);

	return 0;
}

static const struct of_device_id usi_v2_dt_match[] = {
	{ .compatible = "samsung,exynos-usi-v2", },
	{ },
};

MODULE_DEVICE_TABLE(of, usi_v2_dt_match);

static int usi_v2_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usi_v2_data *data = platform_get_drvdata(pdev);

	if (data->secure_mode == SECURE_MODE) {
		dev_info(&pdev->dev,
			"usi configuration for secure channel is skipped\n");
		return 0;
	}

	if (data->mode && data->base) {
		writel(data->mode, data->base);
		dev_info(&pdev->dev, "%s mode:%d\n", __func__, data->mode);
	}

	if (data->weak_pud_en)
		regmap_update_bits(data->sysreg, 0x0, 0x3 << data->sys_ps_cfg,
			data->weak_pud_val << data->sys_ps_cfg);

	return 0;
}

static const struct dev_pm_ops usi_v2_pm = {
	.resume_noirq = usi_v2_resume_noirq,
};

static struct platform_driver usi_v2_driver = {
	.driver = {
		.name		= "usi_v2",
		.owner		= THIS_MODULE,
		.pm		= &usi_v2_pm,
		.of_match_table	= usi_v2_dt_match,
	},
	.probe			= usi_v2_probe,
};

static int __init usi_v2_init(void)
{
	int ret = platform_driver_register(&usi_v2_driver);

	if (ret)
		pr_err("usi_v2 driver registrer failed\n");

	return ret;
}
arch_initcall(usi_v2_init);

static void __exit usi_v2_exit(void)
{
	platform_driver_unregister(&usi_v2_driver);
}
module_exit(usi_v2_exit);

MODULE_AUTHOR("Youngmin Nam <youngmin.nam@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG USI v2 driver");
MODULE_LICENSE("GPL");
