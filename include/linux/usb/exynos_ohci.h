/*
 * Exynos OHCI support
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_OHCI_H
#define _EXYNOS_OHCI_H

struct exynos_ohci_drvdata {
	const char **clk_names;
	int clk_size;
	bool pm_enable;
};

struct exynos_ohci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
	struct clk **clocks;
	struct usb_phy *phy;
	struct usb_otg *otg;
	struct exynos4_ohci_platdata *pdata;
	struct notifier_block lpa_nb;
	const struct exynos_ohci_drvdata *drvdata;
	int power_on;
	int retention;
	unsigned post_lpa_resume:1;
};

static const struct of_device_id exynos_ohci_match[];

static inline const struct exynos_ohci_drvdata
*exynos_ohci_get_driver_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(exynos_ohci_match,
						pdev->dev.of_node);
		return match->data;
	}

	dev_err(&pdev->dev, "no device node specified and no driver data");
	return NULL;
}
#endif /* _EXYNOS_OHCI_H */
