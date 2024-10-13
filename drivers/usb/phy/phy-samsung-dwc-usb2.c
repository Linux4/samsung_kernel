/* linux/drivers/usb/phy/phy-samsung-dwc-usb2.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Kyounghye Yun <k-hye.yun@samsung.com>
 *
 * Samsung USB2.0 PHY transceiver; talks to S3C HS OTG controller, EHCI-S5P and
 * OHCI-EXYNOS controllers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/usb/otg.h>
#include <linux/usb/samsung_usb_phy.h>

#include <mach/exynos-pm.h>
#include <mach/exynos-powermode.h>

#include "phy-samsung-usb.h"
#include "phy-samsung-dwc-usb2.h"

static const char *samsung_dwc_usb2phy_clk_names[] = {"otg_aclk", "otg_hclk",
	"upsizer_otg", "xiu_d_fsys1", "upsizer_ahb_usbhs", NULL};
static const char *samsung_dwc_usb2_phyclk_names[] = {"phy_otg", "freeclk",
	"phyclk", "clk48mohci", NULL};

static int samsung_dwc_usb2phy_clk_get(struct samsung_usbphy *sphy)
{
	const char *clk_id;
	struct clk *clk;
	int i;

	dev_info(sphy->dev, "IP clock gating is N/A\n");

	sphy->clocks = (struct clk **) kmalloc(sizeof(struct clk *) *
			ARRAY_SIZE(samsung_dwc_usb2phy_clk_names), GFP_KERNEL);
	if (!sphy->clocks)
		return -ENOMEM;

	sphy->phy_clocks = (struct clk **) kmalloc(sizeof(struct clk *) *
			ARRAY_SIZE(samsung_dwc_usb2_phyclk_names), GFP_KERNEL);
	if (!sphy->phy_clocks){
		kfree(sphy->clocks);
		return -ENOMEM;
	}

	for (i = 0; samsung_dwc_usb2phy_clk_names[i] != NULL; i++) {
		clk_id = samsung_dwc_usb2phy_clk_names[i];
		clk = devm_clk_get(sphy->dev, clk_id);
		if (IS_ERR_OR_NULL(clk))
			goto err;

		sphy->clocks[i] = clk;
	}

	sphy->clocks[i] = NULL;

	for (i = 0; samsung_dwc_usb2_phyclk_names[i] != NULL; i++) {
		clk_id = samsung_dwc_usb2_phyclk_names[i];
		clk = devm_clk_get(sphy->dev, clk_id);
		if (IS_ERR_OR_NULL(clk))
			goto err;

		sphy->phy_clocks[i] = clk;
	}

	sphy->phy_clocks[i] = NULL;

	return 0;

err:
	kfree(sphy->phy_clocks);
	kfree(sphy->clocks);
	dev_err(sphy->dev, "couldn't get %s clock\n", clk_id);

	return -EINVAL;

}

static int samsung_dwc_usb2phy_clk_prepare(struct samsung_usbphy *sphy)
{
	int i, j;
	int ret;

	for (i = 0; sphy->clocks[i] != NULL; i++) {
		ret = clk_prepare(sphy->clocks[i]);
		if (ret)
			goto err1;
	}
	for (j = 0; sphy->phy_clocks[j] != NULL; j++) {
		ret = clk_prepare(sphy->phy_clocks[j]);
		if (ret)
			goto err2;
	}

	return 0;

err2:
	/* roll back */
	for (j = j - 1; j >= 0; j--)
		clk_unprepare(sphy->phy_clocks[j]);

err1:
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(sphy->clocks[i]);

	return ret;
}

static int samsung_dwc_usb2phy_clk_enable(struct samsung_usbphy *sphy,
						bool umux)
{
	int i, ret;
	int en_count = 0;
	int clk_num = sphy->drv_data->devphy_clk_num;

	if (!umux) {
		for (i = 0; sphy->clocks[i] != NULL; i++) {
			ret = clk_enable(sphy->clocks[i]);
			if (ret)
				goto err1;
		}
	} else {
		/* enable USERMUX for phy clock */
		switch (sphy->phy_type) {
		case USB_PHY_TYPE_DEVICE:
			for (i = 0; i < clk_num; i++) {
				ret = clk_enable(sphy->phy_clocks[i]);
				en_count++;
				if (ret)
					goto err2;
			}
			break;

		case USB_PHY_TYPE_HOST:
			for (i = clk_num; sphy->phy_clocks[i] != NULL; i++) {
				ret = clk_enable(sphy->phy_clocks[i]);
				en_count++;
				if (ret)
					goto err2;
			}
			break;
		}
	}

	return 0;
err1:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable(sphy->clocks[i]);

	return ret;
err2:
	for (i = i - 1; en_count >= 0; i--) {
		clk_disable(sphy->phy_clocks[i]);
		en_count--;
	}

	return ret;
}

static void samsung_dwc_usb2phy_clk_unprepare(struct samsung_usbphy *sphy)
{
	int i;

	for (i = 0; sphy->clocks[i] != NULL; i++)
		clk_unprepare(sphy->clocks[i]);

	for (i = 0; sphy->phy_clocks[i] != NULL; i++)
		clk_unprepare(sphy->phy_clocks[i]);
}

static void samsung_dwc_usb2phy_clk_free(struct samsung_usbphy *sphy)
{
	if (sphy->phy_clocks)
		kfree(sphy->phy_clocks);
	if (sphy->clocks)
		kfree(sphy->clocks);
}

static void samsung_dwc_usb2phy_clk_disable(struct samsung_usbphy *sphy,
						bool umux)
{
	int i;
	int clk_num = sphy->drv_data->devphy_clk_num;

	if (!umux) {
		for (i = 0; sphy->clocks[i] != NULL; i++)
			clk_disable(sphy->clocks[i]);
	} else {
		/* disable USERMUX for phy clock */
		switch (sphy->phy_type) {
		case USB_PHY_TYPE_DEVICE:
			for (i = 0; i < clk_num; i++)
				clk_disable(sphy->phy_clocks[i]);
			break;

		case USB_PHY_TYPE_HOST:
			for (i = clk_num; sphy->phy_clocks[i] != NULL; i++)
				clk_disable(sphy->phy_clocks[i]);
			break;
		}
	}
}

static int samsung_dwc_usbphy_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	if (!otg)
		return -ENODEV;

	if (!otg->host)
		otg->host = host;

	return 0;
}

static void samsung_dwc_usb2phy_enable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;

	/*
	 * CAL (chip abstraction layer) is used to keep synchronization
	 * between kernel code and F/W verification code for the manageability.
	 * When CAL option is enabled, CAL enable function will be called,
	 * and the legacy enable routine will be skipped.
	 */

	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos_cal_dwc_usb2phy_enable(regs, sphy->ref_clk_freq);
		return;
	}
}

static void samsung_dwc_usb2phy_reset(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;

	/*
	 * CAL (chip abstraction layer) is used to keep synchronization
	 * between kernel code and F/W verification code for the manageability.
	 * When CAL option is enabled, CAL enable function will be called,
	 * and the legacy enable routine will be skipped.
	 */

	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos_cal_dwc_usb2phy_reset(regs, sphy->phy_type);
		return;
	}
}

static void samsung_dwc_usb2phy_disable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;

	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos_cal_dwc_usb2phy_disable(regs);
		return;
	}
}

static void samsung_dwc_usb2phy_tune(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy = phy_to_sphy(phy);
	void __iomem *regs = sphy->regs;
	int cpu_type = sphy->drv_data->cpu_type;

	/*
	 * CAL (Chip Abstraction Layer) is used to keep synchronization
	 * between kernel code and F/W verification code for the manageability.
	 * When CAL option is enabled, CAL enable function will be called,
	 * and the legacy enable routine will be skipped.
	 */

	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos_cal_dwc_usb2phy_tune(cpu_type, regs, sphy->phy_type);
		return;
	}
}

/*
 * The function passed to the usb driver for phy initialization
 */
static int samsung_dwc_usb2phy_init(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy;
	struct usb_bus *host = NULL;
	unsigned long flags;
	bool has_pll;
	int ret = 0;

	sphy = phy_to_sphy(phy);

	dev_vdbg(sphy->dev, "%s\n", __func__);

	host = phy->otg->host;

	/* Enable external USB_PLL */
	has_pll = sphy->drv_data->usb_pll;
	if (has_pll) {
		sphy->pll = devm_clk_get(sphy->dev, "usb_pll");
		ret = clk_prepare_enable(sphy->pll);
		if (ret)
			dev_err(sphy->dev, "USB_PLL enable fail\n");
	}

	/* Enable the phy clock */
	ret = samsung_dwc_usb2phy_clk_enable(sphy, false);
	if (ret) {
		dev_err(sphy->dev, "%s: clk_enable failed\n", __func__);
		return ret;
	}

	spin_lock_irqsave(&sphy->lock, flags);

	sphy->usage_count++;

	if (sphy->usage_count - 1) {
		dev_vdbg(sphy->dev, "PHY is already initialized\n");
		goto exit;
	}

	/* update idle status from idle to non-idle */
	exynos_update_ip_idle_status(sphy->idle_ip_index, 0);

	/* Disable phy isolation */
	samsung_usbphy_set_isolation(sphy, false);

	/* Initialize usb phy registers */
	samsung_dwc_usb2phy_enable(sphy);

	if (host) {
		/* setting default phy-type for USB 2.0 */
		if (!strstr(dev_name(host->controller), "ehci") ||
				!strstr(dev_name(host->controller), "ohci"))
			samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_HOST);
	} else {
		samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_DEVICE);
	}

	samsung_dwc_usb2phy_tune(phy);

	/* reset usb phy */
	samsung_dwc_usb2phy_reset(sphy);

	/* Selecting Host/OTG mode; After reset USB2.0PHY_CFG: HOST */
	samsung_usbphy_cfg_sel(sphy);

	/* Enable Usermux for PHY Clock from PHY
	   USERMUX should be enabled after PHY initialization*/
	ret = samsung_dwc_usb2phy_clk_enable(sphy, true);
	if (ret) {
		dev_err(sphy->dev, "%s: user mux enable failed\n"
				, __func__);
		spin_unlock_irqrestore(&sphy->lock, flags);
		return ret;
	}

exit:
	spin_unlock_irqrestore(&sphy->lock, flags);

	dev_dbg(sphy->dev, "end of %s\n", __func__);

	return ret;
}

/*
 * The function passed to the usb driver for phy shutdown
 */
static void samsung_dwc_usb2phy_shutdown(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy;
	struct usb_bus *host = NULL;
	unsigned long flags;
	bool has_pll;

	sphy = phy_to_sphy(phy);

	dev_vdbg(sphy->dev, "%s\n", __func__);

	host = phy->otg->host;

	spin_lock_irqsave(&sphy->lock, flags);

	if (!sphy->usage_count) {
		dev_vdbg(sphy->dev, "PHY is already shutdown\n");
		goto exit;
	}

	sphy->usage_count--;

	if (sphy->usage_count) {
		dev_vdbg(sphy->dev, "PHY is still in use\n");
		goto exit;
	}

	if (host) {
		/* setting default phy-type for USB 2.0 */
		if (!strstr(dev_name(host->controller), "ehci") ||
				!strstr(dev_name(host->controller), "ohci"))
			samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_HOST);
	} else {
		samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_DEVICE);
	}

	samsung_dwc_usb2phy_clk_disable(sphy, true);

	/* De-initialize usb phy registers */
	samsung_dwc_usb2phy_disable(sphy);

	/* Enable phy isolation */
	samsung_usbphy_set_isolation(sphy, true);

	/* update idle status from non-idle to idle */
	exynos_update_ip_idle_status(sphy->idle_ip_index, 1);

	dev_dbg(sphy->dev, "%s: End of setting for shutdown\n", __func__);
exit:
	spin_unlock_irqrestore(&sphy->lock, flags);

	samsung_dwc_usb2phy_clk_disable(sphy, false);

	/* Disable external USB_PLL */
	has_pll = sphy->drv_data->usb_pll;
	if (has_pll)
		clk_disable_unprepare(sphy->pll);
}

static int samsung_dwc_usb2phy_probe(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy;
	struct usb_otg *otg;
	const struct samsung_usbphy_drvdata *drv_data;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(dev, "This driver is required to be instantiated from device tree \n");
		return -EINVAL;
	}

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	sphy = devm_kzalloc(dev, sizeof(*sphy), GFP_KERNEL);
	if (!sphy)
		return -ENOMEM;

	otg = devm_kzalloc(dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	drv_data = samsung_usbphy_get_driver_data(pdev);

	sphy->dev = dev;

	/* get clk for OTG*/
	ret = samsung_dwc_usb2phy_clk_get(sphy);
	if (ret)
		return ret;

	ret = samsung_usbphy_parse_dt(sphy);
	if (ret < 0)
		return ret;

	sphy->regs		= phy_base;
	sphy->drv_data		= drv_data;
	sphy->phy.dev		= sphy->dev;
	sphy->phy.label		= "samsung-usb2phy";
	sphy->phy.type		= USB_PHY_TYPE_USB2;
	sphy->phy.init		= samsung_dwc_usb2phy_init;
	sphy->phy.shutdown	= samsung_dwc_usb2phy_shutdown;
	sphy->ref_clk_freq	= samsung_usbphy_get_refclk_freq(sphy);

	sphy->phy.otg		= otg;
	sphy->phy.otg->phy	= &sphy->phy;
	sphy->phy.otg->set_host = samsung_dwc_usbphy_set_host;

	spin_lock_init(&sphy->lock);

	ret = samsung_dwc_usb2phy_clk_prepare(sphy);
	if (ret) {
		dev_err(dev, "clk_prepare failed\n");
		return ret;
	}

	/* get USB IP index for Device powermode enable */
	sphy->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev));
	exynos_update_ip_idle_status(sphy->idle_ip_index, 1);

	platform_set_drvdata(pdev, sphy);

	ret = usb_add_phy_dev(&sphy->phy);
	if (ret) {
		dev_err(dev, "Failed to add PHY\n");
		goto err1;
	}

	return 0;

err1:
	samsung_dwc_usb2phy_clk_unprepare(sphy);

	return ret;
}

static int samsung_dwc_usb2phy_remove(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy = platform_get_drvdata(pdev);

	usb_remove_phy(&sphy->phy);
	samsung_dwc_usb2phy_clk_unprepare(sphy);
	samsung_dwc_usb2phy_clk_free(sphy);

	if (sphy->pmuregs)
		iounmap(sphy->pmuregs);
	if (sphy->sysreg)
		iounmap(sphy->sysreg);

	return 0;
}

static struct samsung_usbphy_drvdata dwc_usb2phy_exynos7580 = {
	.cpu_type		= TYPE_EXYNOS7580,
	.devphy_reg_offset	= EXYNOS_USB_PHY_CTRL_OFFSET,
	.devphy_en_mask		= EXYNOS_USBPHY_ENABLE,
	.hostphy_en_mask	= EXYNOS_USBPHY_ENABLE,
	.devphy_clk_num		= 1,
	.usb_pll		= 1,
};

static const struct of_device_id samsung_usbphy_dt_match[] = {
	{
		.compatible = "samsung,exynos7580-dwc-usb2phy",
		.data = &dwc_usb2phy_exynos7580,
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_usbphy_dt_match);

static struct platform_driver samsung_dwc_usb2phy_driver = {
	.probe		= samsung_dwc_usb2phy_probe,
	.remove		= samsung_dwc_usb2phy_remove,
	.driver		= {
		.name	= "samsung-dwc-usb2phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_usbphy_dt_match),
	},
};

module_platform_driver(samsung_dwc_usb2phy_driver);

MODULE_DESCRIPTION("Samsung DWC USB2.0 PHY controller");
MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:samsung-dwc-usb2phy");
