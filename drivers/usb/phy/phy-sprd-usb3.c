/**
 * phy-sprd-usb3.c - Spreadtrum USB3 PHY Glue layer
 *
 * Copyright (c) 2015 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Miao Zhu <miao.zhu@spreadtrum.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <soc/sprd/sci_glb_regs.h>

#define PHY_REG_BASE	(phy->base)

/* PHY registers offset */

#define RST_CTRL	(PHY_REG_BASE + 0x00)
#define PWR_CTRL	(PHY_REG_BASE + 0x08)
#define PHY_TUNE1	(PHY_REG_BASE + 0xe4)
#define PHY_TUNE2	(PHY_REG_BASE + 0xe8)
#define PHY_TEST	(PHY_REG_BASE + 0xec)
#define PHY_CTRL1	(PHY_REG_BASE + 0xf0)
#define PHY_CTRL2	(PHY_REG_BASE + 0xf4)
#define PHY_DBG1	(PHY_REG_BASE + 0xf8)
#define PHY_DBG2	(PHY_REG_BASE + 0xfc)
#define PHY_CTRL3	(PHY_REG_BASE + 0x214)

/* PHY registers bits */
#define CORE_SOFT_RST	BIT_AP_AHB_USB3_SOFT_RST
#define PHY_SOFT_RST	BIT_ANA_APB_USB30_PHY_SOFT_RST
#define PHY_PS_PD		BIT_ANA_APB_USB30_PS_PD
#define PIPEP_POWERPRESENT_CFG_SEL	BIT_ANA_APB_USB30_PIPEP_POWERPRESENT_CFG_SEL
#define PIPEP_POWERPRESENT_REG		BIT_ANA_APB_USB30_PIPEP_POWERPRESENT_REG
#define REF_CLKDIV2		BIT_ANA_APB_USB30_REF_CLKDIV2
#define REF_SSP_EN		BIT_ANA_APB_USB30_REF_SSP_EN


struct sprd_ssphy{
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*axi_rst;
	struct regulator	*vdd;
	uint32_t	vdd_vol;
	uint32_t	phy_tune1;
	uint32_t	phy_tune2;
	atomic_t	reset;		/* Core reset flag */
	atomic_t	inited;		/* PHY init flag */
	atomic_t	susped;		/* PHY suspend flag */
};

static inline void __reset_core(void __iomem *addr)
{
	uint32_t reg;

	reg = readl(addr);
	reg |= CORE_SOFT_RST;
	writel(reg, addr);

	msleep(3);

	reg &= ~CORE_SOFT_RST;
	writel(reg, addr);
}

/* Reset USB Core */
static int sprd_ssphy_init(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);
	uint32_t reg;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/* Turn On VDD */
	regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);

	/* Due to chip design, some chips may turn on vddusb by default,
	 * We MUST avoid turning it on twice
	 */
	if (!regulator_is_enabled(phy->vdd))
		regulator_enable(phy->vdd);

	/* Clear PHY PD */
	reg = readl(PWR_CTRL);
	reg &= ~PHY_PS_PD;
	writel(reg, PWR_CTRL);

	/* Restore PHY tunes */
	writel(phy->phy_tune1, PHY_TUNE1);
	writel(phy->phy_tune2, PHY_TUNE2);

	/* Set PipeP_PowerPresent */
	reg = readl(PHY_TEST);
	reg |= PIPEP_POWERPRESENT_CFG_SEL | PIPEP_POWERPRESENT_REG;
	writel(reg, PHY_TEST);

	/* Set SSP_EN and Clear CLKDIV2 */
	reg = readl(PHY_CTRL1);
	reg |= REF_SSP_EN;
	reg &= ~REF_CLKDIV2;
	writel(reg, PHY_CTRL1);

	/* Reset PHY */
	reg = readl(RST_CTRL);
	reg |= PHY_SOFT_RST;
	writel(reg, RST_CTRL);

	msleep(5);

	reg &= ~PHY_SOFT_RST;
	writel(reg, RST_CTRL);

	if (!atomic_read(&phy->reset)) {
		msleep(5);
		__reset_core(phy->axi_rst);

		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);

	return 0;
}

/* Turn off PHY and core */
static void sprd_ssphy_shutdown(struct usb_phy * x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);
	uint32_t reg;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	/* Backup PHY Tune value */
	phy->phy_tune1 = readl(PHY_TUNE1);
	phy->phy_tune2 = readl(PHY_TUNE2);

	reg = readl(PWR_CTRL);
	reg |= PHY_PS_PD;
	writel(reg, PWR_CTRL);

	reg = readl(PHY_TEST);
	reg &= ~(PIPEP_POWERPRESENT_CFG_SEL | PIPEP_POWERPRESENT_REG);
	writel(reg, PHY_TEST);

	/* Due to chip design, some chips may turn on vddusb by default,
	 * We MUST avoid turning it off twice
	 */
	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
}

static ssize_t phy_tune1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);

	return sprintf(buf, "0x%x\n", phy->phy_tune1);
}

static ssize_t phy_tune1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);
	sscanf(buf, "%x", &phy->phy_tune1);

	return size;
}

static DEVICE_ATTR(phy_tune1, S_IRUGO | S_IWUSR,
	phy_tune1_show, phy_tune1_store);

static ssize_t phy_tune2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);

	return sprintf(buf, "0x%x\n", phy->phy_tune2);
}

static ssize_t phy_tune2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);
	sscanf(buf, "%x", &phy->phy_tune2);

	return size;
}

static DEVICE_ATTR(phy_tune2, S_IRUGO | S_IWUSR,
	phy_tune2_show, phy_tune2_store);

static ssize_t vdd_voltage_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);

	return sprintf(buf, "%d\n", phy->vdd_vol);
}

static ssize_t vdd_voltage_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;
	int vol;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);
	sscanf(buf, "%d", &vol);

	if (vol < 1200000 || vol > 3750000) {
		dev_err(dev, "Invalid voltage value %d, \
			a valid voltage should be 1v2 ~ 3v75", vol);
		return -EINVAL;
	}
	phy->vdd_vol = vol;

	return size;
}

static DEVICE_ATTR(vdd_voltage, S_IRUGO | S_IWUSR,
	vdd_voltage_show, vdd_voltage_store);

static struct sprd_ssphy *__sprd_ssphy;

static int sprd_ssphy_probe(struct platform_device *pdev)
{
	struct sprd_ssphy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	phy = devm_kzalloc(dev, sizeof *phy, GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	__sprd_ssphy = phy;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy_glb_regs");
	if (!res) {
		dev_err(dev, "missing USB PHY registers resource\n");
		return -ENODEV;
	}

	phy->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "axi_rst");
	if (!res) {
		dev_err(dev, "missing AXI reset resource\n");
		return -ENODEV;
	}

	phy->axi_rst = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (IS_ERR(phy->axi_rst))
		return PTR_ERR(phy->axi_rst);

	ret = of_property_read_u32(dev->of_node,
			"sprd,phy-tune1", &phy->phy_tune1);
	if (ret < 0 ) {
		dev_err(dev, "unable to read platform data ssphy usb phy tune1\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node,
			"sprd,phy-tune2", &phy->phy_tune2);
	if (ret < 0) {
		dev_err(dev, "unable to read platform data ssphy usb phy tune2\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
			 &phy->vdd_vol);
	if (ret < 0 ) {
		dev_err(dev, "unable to read platform data ssphy vdd voltage\n");
		return ret;
	}
#ifndef CONFIG_SC_FPGA
	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get ssphy vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set ssphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		return ret;
	}
#endif

	platform_set_drvdata(pdev, phy);
	phy->phy.dev				= dev;
	phy->phy.label				= "sprd-ssphy";
#ifndef CONFIG_SC_FPGA
	phy->phy.init				= sprd_ssphy_init;
	phy->phy.shutdown			= sprd_ssphy_shutdown;
#endif
	//phy->phy.set_suspend		= sprd_ssphy_set_suspend;
	//phy->phy.notify_connect 		= sprd_ssphy_notify_connect;
	//phy->phy.notify_disconnect	= sprd_ssphy_notify_disconnect;
	//phy->phy.reset				= sprd_ssphy_reset;
	phy->phy.type				= USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	device_create_file(dev, &dev_attr_phy_tune1);
	device_create_file(dev, &dev_attr_phy_tune2);
	device_create_file(dev, &dev_attr_vdd_voltage);

	return 0;
}

static int sprd_ssphy_remove(struct platform_device *pdev)
{
	struct sprd_ssphy *phy = platform_get_drvdata(pdev);

	usb_remove_phy(&phy->phy);
	regulator_disable(phy->vdd);
	kfree(phy);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprd_ssphy_match[] = {
	{ .compatible = "sprd,ssphy" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_ssphy_match);
#endif

static struct platform_driver sprd_ssphy_driver = {
	.probe		= sprd_ssphy_probe,
	.remove		= sprd_ssphy_remove,
	.driver		= {
		.name	= "sprd-ssphy",
		.of_match_table = sprd_ssphy_match,
	},
};

module_platform_driver(sprd_ssphy_driver);

MODULE_ALIAS("platform:sprd-ssphy	");
MODULE_AUTHOR("Miao Zhu <miao.zhu@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 SPRD PHY Glue Layer");
