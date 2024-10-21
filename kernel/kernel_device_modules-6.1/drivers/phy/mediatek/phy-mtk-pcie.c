// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "phy-mtk-io.h"

/* PHY sif registers */
#define PEXTP_DIG_GLB_00		0x0
#define PEXTP_DIG_GLB_04		0x4
#define PEXTP_DIG_GLB_10		0x10
#define PEXTP_DIG_GLB_20		0x20
#define RG_XTP_BYPASS_PIPE_RST		BIT(4)
#define PEXTP_DIG_GLB_28		0x28
#define RG_XTP_PHY_CLKREQ_N_IN		GENMASK(13, 12)
#define PEXTP_DIG_GLB_50		0x50
#define RG_XTP_CKM_EN_L1S0		BIT(13)
#define PEXTP_DIG_PROBE_OUT		0xd0

/* PHY ANA GLB registers */
#define PEXTP_ANA_GLB_00_REG		0x9000
#define PEXTP_ANA_GLB_6			(PEXTP_ANA_GLB_00_REG + 0x18)
#define PEXTP_ANA_GLB_9			(PEXTP_ANA_GLB_00_REG + 0x24)

/* Internal Resistor Selection of TX Bias Current */
#define EFUSE_GLB_INTR_SEL		GENMASK(28, 24)

#define PEXTP_ANA_LN0_TRX_REG		0xa000

#define PEXTP_ANA_TX_REG		0x04
/* TX PMOS impedance selection */
#define EFUSE_LN_TX_PMOS_SEL		GENMASK(5, 2)
/* TX NMOS impedance selection */
#define EFUSE_LN_TX_NMOS_SEL		GENMASK(11, 8)

#define PEXTP_ANA_RX_REG		0x3c
/* RX impedance selection */
#define EFUSE_LN_RX_SEL			GENMASK(3, 0)

#define PEXTP_ANA_LANE_OFFSET		0x100

/* PHY ckm regsiters */
#define XTP_CKM_DA_REG_3C		0x3C
#define RG_CKM_PADCK_REQ		GENMASK(13, 12)

/**
 * struct mtk_pcie_lane_efuse - eFuse data for each lane
 * @tx_pmos: TX PMOS impedance selection data
 * @tx_nmos: TX NMOS impedance selection data
 * @rx_data: RX impedance selection data
 * @lane_efuse_supported: software eFuse data is supported for this lane
 */
struct mtk_pcie_lane_efuse {
	u32 tx_pmos;
	u32 tx_nmos;
	u32 rx_data;
	bool lane_efuse_supported;
};

/**
 * struct mtk_pcie_phy_data - phy data for each SoC
 * @num_lanes: supported lane numbers
 * @sw_efuse_supported: support software to load eFuse data
 * @phy_int: special init function of each SoC
 */
struct mtk_pcie_phy_data {
	int num_lanes;
	bool sw_efuse_supported;
	int (*phy_init)(struct phy *phy);
};

/**
 * struct mtk_pcie_phy - PCIe phy driver main structure
 * @dev: pointer to device
 * @phy: pointer to generic phy
 * @sif_base: IO mapped register SIF base address of system interface
 * @ckm_base: IO mapped register CKM base address of system interface
 * @clks: PCIe PHY clocks
 * @num_clks: PCIe PHY clocks count
 * @data: pointer to SoC dependent data
 * @sw_efuse_en: software eFuse enable status
 * @efuse_glb_intr: internal resistor selection of TX bias current data
 * @efuse: pointer to eFuse data for each lane
 */
struct mtk_pcie_phy {
	struct device *dev;
	struct phy *phy;
	void __iomem *sif_base;
	void __iomem *ckm_base;
	struct clk_bulk_data *clks;
	int num_clks;
	const struct mtk_pcie_phy_data *data;

	bool sw_efuse_en;
	u32 efuse_glb_intr;
	struct mtk_pcie_lane_efuse *efuse;
};

static void mtk_pcie_efuse_set_lane(struct mtk_pcie_phy *pcie_phy,
				    unsigned int lane)
{
	struct mtk_pcie_lane_efuse *data = &pcie_phy->efuse[lane];
	void __iomem *addr;

	if (!data->lane_efuse_supported)
		return;

	addr = pcie_phy->sif_base + PEXTP_ANA_LN0_TRX_REG +
	       lane * PEXTP_ANA_LANE_OFFSET;

	mtk_phy_update_field(addr + PEXTP_ANA_TX_REG, EFUSE_LN_TX_PMOS_SEL,
			     data->tx_pmos);

	mtk_phy_update_field(addr + PEXTP_ANA_TX_REG, EFUSE_LN_TX_NMOS_SEL,
			     data->tx_nmos);

	mtk_phy_update_field(addr + PEXTP_ANA_RX_REG, EFUSE_LN_RX_SEL,
			     data->rx_data);
}

/**
 * mtk_pcie_phy_init() - Initialize the phy
 * @phy: the phy to be initialized
 *
 * Initialize the phy by setting the efuse data.
 * The hardware settings will be reset during suspend, it should be
 * reinitialized when the consumer calls phy_init() again on resume.
 */
static int mtk_pcie_phy_init(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int i, ret;

	ret = pm_runtime_get_sync(&phy->dev);
	if (ret < 0)
		goto err_pm_get_sync;

	ret = clk_bulk_prepare_enable(pcie_phy->num_clks, pcie_phy->clks);
	if (ret) {
		dev_info(pcie_phy->dev, "failed to enable clocks\n");
		goto err_pm_get_sync;
	}

	if (pcie_phy->data->phy_init) {
		ret = pcie_phy->data->phy_init(phy);
		if (ret)
			goto err_phy_init;
	}

	if (!pcie_phy->sw_efuse_en)
		return 0;

	/* Set global data */
	mtk_phy_update_field(pcie_phy->sif_base + PEXTP_ANA_GLB_00_REG,
			     EFUSE_GLB_INTR_SEL, pcie_phy->efuse_glb_intr);

	for (i = 0; i < pcie_phy->data->num_lanes; i++)
		mtk_pcie_efuse_set_lane(pcie_phy, i);

	return 0;

err_phy_init:
	clk_bulk_disable_unprepare(pcie_phy->num_clks, pcie_phy->clks);
err_pm_get_sync:
	pm_runtime_put_sync(&phy->dev);

	return ret;
}

static int mtk_pcie_phy_exit(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	clk_bulk_disable_unprepare(pcie_phy->num_clks, pcie_phy->clks);
	pm_runtime_put_sync(&phy->dev);

	return 0;
}

/* Set partition when use PCIe PHY debug probe table */
static void mtk_pcie_phy_dbg_set_partition(void __iomem *phy_base, u32 partition)
{
	writel_relaxed(partition, phy_base + PEXTP_DIG_GLB_00);
}

/* Read the PCIe PHY internal signal corresponding to the debug probe table bus */
static u32 mtk_pcie_phy_dbg_read_bus(void __iomem *phy_base, u32 sel, u32 bus)
{
	writel_relaxed(bus, phy_base + sel);
	return readl_relaxed(phy_base + PEXTP_DIG_PROBE_OUT);
}

static int mtk_pcie_monitor_phy(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	u32 phy_table[8] = {0};

	mtk_pcie_phy_dbg_set_partition(pcie_phy->sif_base, 0x404);
	phy_table[0] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_10,
						 0x910090);
	phy_table[1] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_10,
						 0x1180092);
	phy_table[2] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_10,
						 0x11a011b);
	phy_table[3] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_10,
						 0x11c011d);
	phy_table[4] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_10,
						 0x11e011f);
	dev_info(pcie_phy->dev,
		 "phy ln0 probe: 0x0x910090=%#x, 0x1180092=%#x, 0x11a011b=%#x, 0x11c011d=%#x, 0x11e011f=%#x\n",
		 phy_table[0], phy_table[1], phy_table[2], phy_table[3], phy_table[4]);

	mtk_pcie_phy_dbg_set_partition(pcie_phy->sif_base, 0x0);
	writel_relaxed(0x5600038e, pcie_phy->sif_base + PEXTP_ANA_GLB_6);
	phy_table[5] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_04,
						 0x1a);

	mtk_pcie_phy_dbg_set_partition(pcie_phy->sif_base, 0x70002);
	writel_relaxed(0x98045600, pcie_phy->sif_base + PEXTP_ANA_GLB_9);
	phy_table[6] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_04,
						 0x1b1a);
	dev_info(pcie_phy->dev, "phy misc probe: 0x1a=%#x, 0x1b1a=%#x\n",
		 phy_table[5], phy_table[6]);

	mtk_pcie_phy_dbg_set_partition(pcie_phy->sif_base, 0x4);
	phy_table[7] = mtk_pcie_phy_dbg_read_bus(pcie_phy->sif_base, PEXTP_DIG_GLB_10,
						 0xe2);
	dev_info(pcie_phy->dev, "phy ln0 probe: 0xe2=%#x\n", phy_table[7]);

	return 0;
}

static const struct phy_ops mtk_pcie_phy_ops = {
	.init		= mtk_pcie_phy_init,
	.exit		= mtk_pcie_phy_exit,
	.calibrate	= mtk_pcie_monitor_phy,
	.owner		= THIS_MODULE,
};

static int mtk_pcie_efuse_read_for_lane(struct mtk_pcie_phy *pcie_phy,
					unsigned int lane)
{
	struct mtk_pcie_lane_efuse *efuse = &pcie_phy->efuse[lane];
	struct device *dev = pcie_phy->dev;
	char efuse_id[16];
	int ret;

	snprintf(efuse_id, sizeof(efuse_id), "tx_ln%d_pmos", lane);
	ret = nvmem_cell_read_variable_le_u32(dev, efuse_id, &efuse->tx_pmos);
	if (ret) {
		dev_info(dev, "Failed to read %s\n", efuse_id);
		return ret;
	}

	snprintf(efuse_id, sizeof(efuse_id), "tx_ln%d_nmos", lane);
	ret = nvmem_cell_read_variable_le_u32(dev, efuse_id, &efuse->tx_nmos);
	if (ret) {
		dev_info(dev, "Failed to read %s\n", efuse_id);
		return ret;
	}

	snprintf(efuse_id, sizeof(efuse_id), "rx_ln%d", lane);
	ret = nvmem_cell_read_variable_le_u32(dev, efuse_id, &efuse->rx_data);
	if (ret) {
		dev_info(dev, "Failed to read %s\n", efuse_id);
		return ret;
	}

	if (!(efuse->tx_pmos || efuse->tx_nmos || efuse->rx_data)) {
		dev_info(dev,
			 "No eFuse data found for lane%d, but dts enable it\n",
			 lane);
		return -EINVAL;
	}

	efuse->lane_efuse_supported = true;

	return 0;
}

static int mtk_pcie_read_efuse(struct mtk_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	bool nvmem_enabled;
	int ret, i;

	/* nvmem data is optional */
	nvmem_enabled = device_property_present(dev, "nvmem-cells");
	if (!nvmem_enabled)
		return 0;

	ret = nvmem_cell_read_variable_le_u32(dev, "glb_intr",
					      &pcie_phy->efuse_glb_intr);
	if (ret) {
		dev_info(dev, "Failed to read glb_intr\n");
		return ret;
	}

	pcie_phy->sw_efuse_en = true;

	pcie_phy->efuse = devm_kzalloc(dev, pcie_phy->data->num_lanes *
				       sizeof(*pcie_phy->efuse), GFP_KERNEL);
	if (!pcie_phy->efuse)
		return -ENOMEM;

	for (i = 0; i < pcie_phy->data->num_lanes; i++) {
		ret = mtk_pcie_efuse_read_for_lane(pcie_phy, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int mtk_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct mtk_pcie_phy *pcie_phy;
	int ret;

	pcie_phy = devm_kzalloc(dev, sizeof(*pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	pcie_phy->sif_base = devm_platform_ioremap_resource_byname(pdev, "sif");
	if (IS_ERR(pcie_phy->sif_base)) {
		dev_info(dev, "Failed to map phy-sif base\n");
		return PTR_ERR(pcie_phy->sif_base);
	}

	pm_runtime_enable(dev);

	pcie_phy->phy = devm_phy_create(dev, dev->of_node, &mtk_pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		dev_info(dev, "Failed to create PCIe phy\n");
		ret = PTR_ERR(pcie_phy->phy);
		goto err_probe;
	}

	pcie_phy->dev = dev;
	pcie_phy->data = of_device_get_match_data(dev);
	if (!pcie_phy->data) {
		dev_info(dev, "Failed to get phy data\n");
		ret = -EINVAL;
		goto err_probe;
	}

	if (pcie_phy->data->sw_efuse_supported) {
		/*
		 * Failed to read the efuse data is not a fatal problem,
		 * ignore the failure and keep going.
		 */
		ret = mtk_pcie_read_efuse(pcie_phy);
		if (ret == -EPROBE_DEFER || ret == -ENOMEM)
			goto err_probe;
	}

	phy_set_drvdata(pcie_phy->phy, pcie_phy);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_info(dev, "PCIe phy probe failed\n");
		ret = PTR_ERR(provider);
		goto err_probe;
	}

	pcie_phy->num_clks = devm_clk_bulk_get_all(dev, &pcie_phy->clks);
	if (pcie_phy->num_clks < 0) {
		dev_info(dev, "failed to get clocks\n");
		ret = pcie_phy->num_clks;
		goto err_probe;
	}

	return 0;

err_probe:
	pm_runtime_disable(dev);

	return ret;
}

static int mtk_pcie_phy_init_6985(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	struct device *dev = pcie_phy->dev;
	struct platform_device *pdev = to_platform_device(dev);
	u32 val;

	if (!pcie_phy->ckm_base) {
		pcie_phy->ckm_base = devm_platform_ioremap_resource_byname(pdev,
									   "ckm");
		if (IS_ERR(pcie_phy->ckm_base)) {
			dev_info(dev, "Failed to map phy-ckm base\n");
			return PTR_ERR(pcie_phy->ckm_base);
		}
	}

	val = readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_28);
	val |= RG_XTP_PHY_CLKREQ_N_IN;
	writel_relaxed(val, pcie_phy->sif_base + PEXTP_DIG_GLB_28);

	val = readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_50);
	val &= ~RG_XTP_CKM_EN_L1S0;
	writel_relaxed(val, pcie_phy->sif_base + PEXTP_DIG_GLB_50);

	val = readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_3C);
	val |= RG_CKM_PADCK_REQ;
	writel_relaxed(val, pcie_phy->ckm_base + XTP_CKM_DA_REG_3C);

	dev_info(dev, "PHY GLB_28=%#x, GLB_50=%#x, CKM_3C=%#x\n",
		 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_28),
		 readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_50),
		 readl_relaxed(pcie_phy->ckm_base + XTP_CKM_DA_REG_3C));

	return 0;
}

static int mtk_pcie_phy_init_6989(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	u32 val;

	val = readl_relaxed(pcie_phy->sif_base + PEXTP_DIG_GLB_20);
	val &= ~RG_XTP_BYPASS_PIPE_RST;
	writel_relaxed(val, pcie_phy->sif_base + PEXTP_DIG_GLB_20);

	return 0;
}

static const struct mtk_pcie_phy_data mt8195_data = {
	.num_lanes = 2,
	.sw_efuse_supported = true,
};

static const struct mtk_pcie_phy_data mt6985_data = {
	.num_lanes = 1,
	.sw_efuse_supported = false,
	.phy_init = mtk_pcie_phy_init_6985,
};

static const struct mtk_pcie_phy_data mt6989_data = {
	.num_lanes = 1,
	.sw_efuse_supported = false,
	.phy_init = mtk_pcie_phy_init_6989,
};

static const struct of_device_id mtk_pcie_phy_of_match[] = {
	{ .compatible = "mediatek,mt8195-pcie-phy", .data = &mt8195_data },
	{ .compatible = "mediatek,mt6985-pcie-phy", .data = &mt6985_data },
	{ .compatible = "mediatek,mt6989-pcie-phy", .data = &mt6989_data },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_pcie_phy_of_match);

static struct platform_driver mtk_pcie_phy_driver = {
	.probe	= mtk_pcie_phy_probe,
	.driver	= {
		.name = "mtk-pcie-phy",
		.of_match_table = mtk_pcie_phy_of_match,
	},
};
module_platform_driver(mtk_pcie_phy_driver);

MODULE_DESCRIPTION("MediaTek PCIe PHY driver");
MODULE_AUTHOR("Jianjun Wang <jianjun.wang@mediatek.com>");
MODULE_LICENSE("GPL");
