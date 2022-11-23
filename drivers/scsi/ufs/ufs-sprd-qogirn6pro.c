/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/time.h>
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
#include <linux/sprd_sip_svc.h>
#endif
#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "ufs-sprd-qogirn6pro.h"
#include "ufs_quirks.h"
#include "unipro.h"

/**
 * ufs_sprd_rmwl - read modify write into a register
 * @base - base address
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufs_sprd_rmwl(void __iomem *base, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	tmp &= ~mask;
	tmp |= (val & mask);
	writel(tmp, (base) + (reg));
}

void ufs_sprd_reset(struct ufs_sprd_host *host)
{
	dev_info(host->hba->dev, "ufs hardware reset!\n");

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   host->aon_apb_ufs_rst.mask);

	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
			   host->ap_ahb_ufs_rst.reg,
			   host->ap_ahb_ufs_rst.mask,
			   host->ap_ahb_ufs_rst.mask);

	mdelay(1);

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   0);

	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
			   host->ap_ahb_ufs_rst.reg,
			   host->ap_ahb_ufs_rst.mask,
			   0);
}

static int ufs_sprd_get_syscon_reg(struct device_node *np,
				   struct syscon_ufs *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];
	int ret;

	regmap = syscon_regmap_lookup_by_name(np, name);
	if (IS_ERR(regmap)) {
		pr_err("read ufs syscon %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}

	ret = syscon_get_args_by_name(np, name, 2, syscon_args);
	if (ret < 0)
		return ret;
	else if (ret != 2) {
		pr_err("read ufs syscon %s fail,ret = %d\n", name, ret);
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

/**
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_host *host;
	int ret = 0;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	struct sprd_sip_svc_handle *svc_handle;
#endif

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	host->vdd_mphy = devm_regulator_get(dev, "vdd-mphy");
	ret = regulator_enable(host->vdd_mphy);
	if (ret)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ap_ahb_ufs_rst,
				      "ap_ahb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_rst,
				      "aon_apb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->phy_sram_init_done,
				      "phy_sram_init_done");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_clk_en,
				      "aon_apb_ufs_clk_en");
	if (ret < 0)
		 return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ufsdev_refclk_en,
				      "ufsdev_refclk_en");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node,
				      &host->usb31pllv_ref2mphy_en,
				      "usb31pllv_ref2mphy_en");
	if (ret < 0)
		return -ENODEV;

	host->hclk = devm_clk_get(&pdev->dev, "ufs_hclk");
	if (IS_ERR(host->hclk)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ufs_pclk\n");
			 host->hclk = NULL;
	}

	host->hclk_source = devm_clk_get(&pdev->dev, "ufs_hclk_source");
	if (IS_ERR(host->hclk_source)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ufs_hclk_source\n");
			 host->hclk_source = NULL;
	}

	clk_set_parent(host->hclk, host->hclk_source);

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
					  host->ap_ahb_ufs_rst.reg,
					  host->ap_ahb_ufs_rst.mask,
					  host->ap_ahb_ufs_rst.mask);

	mdelay(1);

	regmap_update_bits(host->ap_ahb_ufs_rst.regmap,
					  host->ap_ahb_ufs_rst.reg,
					  host->ap_ahb_ufs_rst.mask,
					  0);

	ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
	if ((ufshcd_readl(hba, REG_UFS_CCAP) & (1 << 27)))
		ufshcd_writel(hba, (CRYPTO_GENERAL_ENABLE | CONTROLLER_ENABLE),
			      REG_CONTROLLER_ENABLE);
	svc_handle = sprd_sip_svc_get_handle();
	if (!svc_handle) {
		pr_err("%s: failed to get svc handle\n", __func__);
		return -ENODEV;
	}

	pr_err("ufs init, get svc_handle:0x%x, get storage_ops handle: 0x%x\n",
	       svc_handle, svc_handle->storage_ops);

	ret = svc_handle->storage_ops.ufs_crypto_enable();
	pr_err("smc: enable cfg, ret:0x%x", ret);
#endif

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;

	hba->caps |= UFSHCD_CAP_CLK_GATING |
		     UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	return 0;
}

/**
 * ufs_sprd_hw_init - controller enable and reset
 * @hba: host controller instance
 */
void ufs_sprd_hw_init(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ufs_sprd_reset(host);
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	regmap_update_bits(host->aon_apb_ufs_clk_en.regmap,
			   host->aon_apb_ufs_clk_en.reg,
			   host->aon_apb_ufs_clk_en.mask,
			   0);

	ret = regulator_disable(host->vdd_mphy);
	if (ret)
		pr_err("disable vdd_mphy failed ret =0x%x!\n", ret);

	devm_kfree(dev, host);
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_30;
}

static int ufs_sprd_phy_sram_init_done(struct ufs_hba *hba)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t retry = 10;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	do {
		ret = regmap_read(host->phy_sram_init_done.regmap,
				  host->phy_sram_init_done.reg, &val);
		if (ret < 0)
			return ret;

		if ((val&0x1) == 0x1) {
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRLSB), 0x1c);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRMSB), 0x40);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRLSB), 0x04);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRMSB), 0x00);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGRDWRSEL), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRLSB), 0x1c);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGADDRMSB), 0x41);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRLSB), 0x04);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGWRMSB), 0x00);
			ufshcd_dme_set(hba, UIC_ARG_MIB(CBCREGRDWRSEL), 0x01);
			ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);

			return 0;
		} else {
			udelay(1000);
			retry--;
		}
	} while (retry > 0);
		return -1;
}

static int ufs_sprd_phy_init(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ufshcd_dme_set(hba, UIC_ARG_MIB(CBREFCLKCTRL2), 0x90);
	ufshcd_dme_set(hba, UIC_ARG_MIB(CBCRCTRL), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RXSQCONTROL,
		       UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RXSQCONTROL,
		       UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(CBRATESEL), 0x01);

	ret = ufs_sprd_phy_sram_init_done(hba);
	if (ret)
		return ret;

	regmap_update_bits(host->phy_sram_init_done.regmap,
			   host->phy_sram_init_done.reg,
			   host->phy_sram_init_done.mask,
			   host->phy_sram_init_done.mask);

	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYCFGUPDT), 0x01);
	ufshcd_dme_set(hba, UIC_ARG_MIB(VS_MPHYDISABLE), 0x0);

	return ret;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	int ret = 0;
	struct sprd_sip_svc_handle *svc_handle;
#endif

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		ufs_sprd_hw_init(hba);
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
		ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
		svc_handle = sprd_sip_svc_get_handle();
		if (!svc_handle) {
			pr_err("%s: failed to get svc handle\n", __func__);
			return -ENODEV;
		}

		pr_err("ufs init, get svc_handle:0x%x, get storage_ops handle: 0x%x\n",
		       svc_handle, svc_handle->storage_ops);
		ret = svc_handle->storage_ops.ufs_crypto_enable();
		pr_err("smc: enable cfg, ret:0x%x", ret);
#endif
		break;
	case POST_CHANGE:
		 if (hba->vops->phy_initialization) {
			err = hba->vops->phy_initialization(hba);
			if (err) {
				dev_err(hba->dev, "Phy setup failed (%d)\n",
					err);
			}
		}
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		break;
	default:
		break;
	}

	return err;
}

static int ufs_sprd_pwr_change_notify(struct ufs_hba *hba,
				enum ufs_notify_change_status status,
				struct ufs_pa_layer_attr *dev_max_params,
				struct ufs_pa_layer_attr *dev_req_params)
{
	int err = 0;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		err = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		memcpy(dev_req_params, dev_max_params,
				       sizeof(struct ufs_pa_layer_attr));
		break;
	case POST_CHANGE:
		/* Set auto h8 ilde time to 10ms */
		ufshcd_auto_hibern8_enable(hba);
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	return err;
}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
				enum uic_cmd_dme cmd,
				enum ufs_notify_change_status status)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			/* Set auto h8 ilde time to 0ms */
			ufshcd_auto_hibern8_disable(hba);
		}

		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			regmap_update_bits(host->ufsdev_refclk_en.regmap,
					   host->ufsdev_refclk_en.reg,
					   host->ufsdev_refclk_en.mask,
					   host->ufsdev_refclk_en.mask);

			regmap_update_bits(host->usb31pllv_ref2mphy_en.regmap,
					   host->usb31pllv_ref2mphy_en.reg,
					   host->usb31pllv_ref2mphy_en.mask,
					   host->usb31pllv_ref2mphy_en.mask);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			/* Set auto h8 ilde time to 10ms */
			ufshcd_auto_hibern8_enable(hba);
		}

		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			regmap_update_bits(host->ufsdev_refclk_en.regmap,
					   host->ufsdev_refclk_en.reg,
					   host->ufsdev_refclk_en.mask,
					   0);

			regmap_update_bits(host->usb31pllv_ref2mphy_en.regmap,
					   host->usb31pllv_ref2mphy_en.reg,
					   host->usb31pllv_ref2mphy_en.mask,
					   0);
		}
		break;
	default:
		break;
	}
}

/**
 * struct ufs_hba_sprd_vops - UFS sprd specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_sprd_vops = {
	.name = "sprd",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.phy_initialization = ufs_sprd_phy_init,
	.hibern8_notify = ufs_sprd_hibern8_notify,
};

/**
 * ufs_sprd_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_sprd_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_sprd_vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

/**
 * ufs_sprd_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always returns 0
 */
static int ufs_sprd_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	return 0;
}

static const struct of_device_id ufs_sprd_of_match[] = {
	{ .compatible = "sprd,ufshc"},
	{},
};

static const struct dev_pm_ops ufs_sprd_pm_ops = {
	.suspend = ufshcd_pltfrm_suspend,
	.resume = ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume = ufshcd_pltfrm_runtime_resume,
	.runtime_idle = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_sprd_pltform = {
	.probe = ufs_sprd_probe,
	.remove = ufs_sprd_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver = {
		.name = "ufshcd-sprd",
		.pm = &ufs_sprd_pm_ops,
		.of_match_table = of_match_ptr(ufs_sprd_of_match),
	},
};
module_platform_driver(ufs_sprd_pltform);

MODULE_DESCRIPTION("SPRD Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
