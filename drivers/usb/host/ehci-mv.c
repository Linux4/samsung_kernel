/*
 * Copyright (C) 2011 Marvell International Ltd. All rights reserved.
 * Author: Chao Xie <chao.xie@marvell.com>
 *        Neil Zhang <zhangwm@marvell.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/usb/otg.h>
#include <linux/usb/mv_usb2_phy.h>
#include <linux/platform_data/mv_usb.h>
#include <dt-bindings/usb/mv_usb.h>

#define CAPLENGTH_MASK         (0xff)

struct ehci_hcd_mv {
	struct usb_hcd *hcd;
	struct usb_phy *phy;

	/* Which mode does this ehci running OTG/Host ? */
	int mode;

	void __iomem *cap_regs;
	void __iomem *op_regs;

	struct usb_phy *otg;

	struct mv_usb_platform_data *pdata;

	struct clk *clk;
};

static void ehci_clock_enable(struct ehci_hcd_mv *ehci_mv)
{
	clk_enable(ehci_mv->clk);
}

static void ehci_clock_disable(struct ehci_hcd_mv *ehci_mv)
{
	clk_disable(ehci_mv->clk);
}

static int mv_ehci_enable(struct ehci_hcd_mv *ehci_mv)
{
	ehci_clock_enable(ehci_mv);

	return usb_phy_init(ehci_mv->phy);
}

static void mv_ehci_disable(struct ehci_hcd_mv *ehci_mv)
{
	usb_phy_shutdown(ehci_mv->phy);

	ehci_clock_disable(ehci_mv);
}

static int mv_ehci_reset(struct usb_hcd *hcd)
{
	struct device *dev = hcd->self.controller;
	struct ehci_hcd_mv *ehci_mv = dev_get_drvdata(dev);
	int retval;

	if (ehci_mv == NULL) {
		dev_err(dev, "Can not find private ehci data\n");
		return -ENODEV;
	}

	hcd->has_tt = 1;

	retval = ehci_setup(hcd);
	if (retval)
		dev_err(dev, "ehci_setup failed %d\n", retval);

	return retval;
}

static const struct hc_driver mv_ehci_hc_driver = {
	.description = hcd_name,
	.product_desc = "Marvell EHCI",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	.flags = HCD_MEMORY | HCD_USB2 | HCD_BH,

	/*
	 * basic lifecycle operations
	 */
	.reset = mv_ehci_reset,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,
	.endpoint_reset = ehci_endpoint_reset,
	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
};

static int mv_ehci_dt_parse(struct platform_device *pdev,
			struct mv_usb_platform_data *pdata)
{
	struct device_node *np = pdev->dev.of_node;

	if (of_property_read_string(np,
			"marvell,ehci-name", &((pdev->dev).init_name)))
		return -EINVAL;

	if (of_property_read_u32(np, "marvell,udc-mode", &(pdata->mode)))
		return -EINVAL;

	if (of_property_read_u32(np, "marvell,dev-id", &(pdata->id)))
		pdata->id = PXA_USB_DEV_OTG;

	of_property_read_u32(np, "marvell,extern-attr", &(pdata->extern_attr));
	pdata->otg_force_a_bus_req = of_property_read_bool(np,
					"marvell,otg-force-a-bus-req");
	pdata->disable_otg_clock_gating = of_property_read_bool(np,
						"marvell,disable-otg-clock-gating");

	return 0;
}

static int mv_ehci_probe(struct platform_device *pdev)
{
	struct mv_usb_platform_data *pdata;
	struct device *dev = &pdev->dev;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct ehci_hcd_mv *ehci_mv;
	struct resource *r;
	int retval = -ENODEV;
	u32 offset;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate memory for platform_data\n");
		return -ENODEV;
	}
	mv_ehci_dt_parse(pdev, pdata);
	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	if (!dev->coherent_dma_mask)
		dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (usb_disabled())
		return -ENODEV;

	hcd = usb_create_hcd(&mv_ehci_hc_driver, &pdev->dev, "mv ehci");
	if (!hcd)
		return -ENOMEM;

	ehci_mv = devm_kzalloc(&pdev->dev, sizeof(*ehci_mv), GFP_KERNEL);
	if (ehci_mv == NULL) {
		dev_err(&pdev->dev, "cannot allocate ehci_hcd_mv\n");
		retval = -ENOMEM;
		goto err_put_hcd;
	}

	platform_set_drvdata(pdev, ehci_mv);
	ehci_mv->pdata = pdata;
	ehci_mv->hcd = hcd;

	ehci_mv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(ehci_mv->clk)) {
		dev_err(&pdev->dev, "error getting clock\n");
		retval = PTR_ERR(ehci_mv->clk);
		goto err_clear_drvdata;
	}
	clk_prepare(ehci_mv->clk);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "no I/O memory resource defined\n");
		retval = -ENODEV;
		goto err_clear_drvdata;
	}

	ehci_mv->cap_regs = devm_ioremap(&pdev->dev, r->start,
					 resource_size(r));
	if (ehci_mv->cap_regs == NULL) {
		dev_err(&pdev->dev, "failed to map I/O memory\n");
		retval = -EFAULT;
		goto err_clear_drvdata;
	}

	ehci_mv->phy = devm_usb_get_phy_dev(&pdev->dev, MV_USB2_PHY_INDEX);
	if (IS_ERR_OR_NULL(ehci_mv->phy)) {
		retval = PTR_ERR(ehci_mv->phy);
		if (retval != -EPROBE_DEFER && retval != -ENODEV)
			dev_err(&pdev->dev, "failed to get the outer phy\n");
		else {
			kfree(hcd->bandwidth_mutex);
			kfree(hcd);

			return -EPROBE_DEFER;
		}
		goto err_clear_drvdata;
	}

	retval = mv_ehci_enable(ehci_mv);
	if (retval) {
		dev_err(&pdev->dev, "init phy error %d\n", retval);
		goto err_clear_drvdata;
	}

	offset = readl(ehci_mv->cap_regs) & CAPLENGTH_MASK;
	ehci_mv->op_regs =
		(void __iomem *) ((unsigned long) ehci_mv->cap_regs + offset);

	hcd->rsrc_start = r->start;
	hcd->rsrc_len = resource_size(r);
	hcd->regs = ehci_mv->op_regs;

	hcd->irq = platform_get_irq(pdev, 0);
	if (!hcd->irq) {
		dev_err(&pdev->dev, "Cannot get irq.");
		retval = -ENODEV;
		goto err_disable_clk;
	}

	ehci = hcd_to_ehci(hcd);
	ehci->caps = (struct ehci_caps *) ehci_mv->cap_regs;

	ehci_mv->mode = pdata->mode;
	if (ehci_mv->mode == MV_USB_MODE_OTG) {
		ehci_mv->otg = devm_usb_get_phy_dev(&pdev->dev,
						MV_USB2_OTG_PHY_INDEX);
		if (IS_ERR(ehci_mv->otg)) {
			retval = PTR_ERR(ehci_mv->otg);

			if (retval == -ENXIO)
				dev_info(&pdev->dev, "MV_USB_MODE_OTG "
						"must have CONFIG_USB_PHY enabled\n");
			else if (retval != -EPROBE_DEFER)
				dev_err(&pdev->dev,
						"unable to find transceiver\n");
			goto err_disable_clk;
		}

		retval = otg_set_host(ehci_mv->otg->otg, &hcd->self);
		if (retval < 0) {
			dev_err(&pdev->dev,
				"unable to register with transceiver\n");
			retval = -ENODEV;
			goto err_disable_clk;
		}
		/* otg will enable clock before use as host */
		mv_ehci_disable(ehci_mv);
	} else {
		pxa_usb_extern_call(pdata->id, vbus, set_vbus, 1);

		retval = usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
		if (retval) {
			dev_err(&pdev->dev,
				"failed to add hcd with err %d\n", retval);
			goto err_set_vbus;
		}
	}

	dev_info(&pdev->dev,
		 "successful find EHCI device with regs 0x%p irq %d"
		 " working in %s mode\n", hcd->regs, hcd->irq,
		 ehci_mv->mode == MV_USB_MODE_OTG ? "OTG" : "Host");

	return 0;

err_set_vbus:
	pxa_usb_extern_call(pdata->id, vbus, set_vbus, 0);
err_disable_clk:
	mv_ehci_disable(ehci_mv);
err_clear_drvdata:
	platform_set_drvdata(pdev, NULL);
err_put_hcd:
	usb_put_hcd(hcd);

	return retval;
}

static int mv_ehci_remove(struct platform_device *pdev)
{
	struct ehci_hcd_mv *ehci_mv = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_mv->hcd;

	if (hcd->rh_registered)
		usb_remove_hcd(hcd);

	if (!IS_ERR_OR_NULL(ehci_mv->otg))
		otg_set_host(ehci_mv->otg->otg, NULL);

	if (ehci_mv->mode == MV_USB_MODE_HOST) {
		pxa_usb_extern_call(ehci_mv->pdata->id, vbus, set_vbus, 0);

		mv_ehci_disable(ehci_mv);

		clk_unprepare(ehci_mv->clk);
	}

	usb_put_hcd(hcd);

	return 0;
}

MODULE_ALIAS("mv-ehci");

static const struct platform_device_id ehci_id_table[] = {
	{"pxa-u2oehci", PXA_U2OEHCI},
	{"pxa-sph", PXA_SPH},
	{"mmp3-hsic", MMP3_HSIC},
	{"mmp3-fsic", MMP3_FSIC},
	{},
};

static const struct of_device_id mv_ehci_dt_match[] = {
	{.compatible = "marvell,pxa-u2oehci"},
	{.compatible = "marvell,pxa-sph"},
	{.compatible = "marvell,mmp3-hsic"},
	{.compatible = "marvell,mmp3-fsic"},
	{},
};
MODULE_DEVICE_TABLE(of, mv_ehci_dt_match);

static void mv_ehci_shutdown(struct platform_device *pdev)
{
	struct ehci_hcd_mv *ehci_mv = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_mv->hcd;

	if (!hcd->rh_registered)
		return;

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static struct platform_driver ehci_mv_driver = {
	.probe = mv_ehci_probe,
	.remove = mv_ehci_remove,
	.shutdown = mv_ehci_shutdown,
	.driver = {
		   .name = "mv-ehci",
		   .of_match_table = of_match_ptr(mv_ehci_dt_match),
		   .bus = &platform_bus_type,
		   },
	.id_table = ehci_id_table,
};
