// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"

#define DISP_REG_DISP_R2Y_EN   0x000
	#define R2Y_EN BIT(0)
#define DISP_REG_DISP_R2Y_RST  0x004
#define DISP_REG_DISP_R2Y_SIZE 0x018
#define DISP_REG_DISP_R2Y_1TNP 0x01C
	#define R2Y_SW_1T2P BIT(0)
#define DISP_REG_DISP_R2Y_RELAY 0x024
	#define DISP_R2Y_RELAY_MODE BIT(0)
#define DISP_REG_DISP_R2Y_CT_CFG 0x02C
	#define DISP_R2Y_CT_EN BIT(0)
	#define RGB_TO_JPEG (0x0 << 4)
	#define RGB_TO_FULL709 (0x1 << 4)
	#define RGB_TO_BT601 (0x2 << 4)
	#define RGB_TO_BT709 (0x3 << 4)

#define DISP_REG_DISP_R2Y_DATA_CFG 0x034
	#define DISP_R2Y_DATACONV_EN BIT(0)
	#define YUV444 (0x0 << 4)
	#define YUV422 (0x1 << 4)
	#define YUV420 (0x2 << 4)

/**
 * struct mtk_disp_r2y - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_r2y {
	struct mtk_ddp_comp ddp_comp;
};

void mtk_r2y_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("0x000: 0x%08x 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x000),
		readl(baddr + 0x004), readl(baddr + 0x008), readl(baddr + 0x00C));
	DDPDUMP("0x010: 0x%08x 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x010),
		readl(baddr + 0x014), readl(baddr + 0x018), readl(baddr + 0x01C));
	DDPDUMP("0x020: 0x%08x 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x020),
		readl(baddr + 0x024), readl(baddr + 0x028), readl(baddr + 0x02C));
	DDPDUMP("0x030: 0x%08x  0x%08x\n", readl(baddr + 0x030), readl(baddr + 0x030));
}

static inline struct mtk_disp_r2y *comp_to_r2y(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_r2y, ddp_comp);
}

static void mtk_r2y_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	unsigned int hsize = 0, vsize = 0;

	DDPDBG("%s, regs_pa=0x%llx, w=%d, h=%d\n", __func__, comp->regs_pa, cfg->w, cfg->h);

	hsize = cfg->w & 0x1FFF;
	vsize = cfg->h & 0x1FFF;

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DISP_R2Y_SIZE,
			((hsize << 16) | vsize), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DISP_R2Y_1TNP,
			R2Y_SW_1T2P, R2Y_SW_1T2P);
}

int mtk_r2y_analysis(struct mtk_ddp_comp *comp)
{
	return 0;
}

static void mtk_r2y_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DISP_R2Y_EN,
			R2Y_EN, R2Y_EN);
}

static void mtk_r2y_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DISP_R2Y_EN,
			0, R2Y_EN);
}

static void mtk_r2y_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_r2y_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_r2y_funcs = {
	.start = mtk_r2y_start,
	.stop = mtk_r2y_stop,
	.config = mtk_r2y_config,
	.prepare = mtk_r2y_prepare,
	.unprepare = mtk_r2y_unprepare,
};

static int mtk_disp_r2y_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_r2y *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s &priv->ddp_comp:0x%lx\n", __func__, (unsigned long)&priv->ddp_comp);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_r2y_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_r2y *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_r2y_component_ops = {
	.bind = mtk_disp_r2y_bind, .unbind = mtk_disp_r2y_unbind,
};

static int mtk_disp_r2y_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_r2y *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_R2Y);
	DDPINFO("%s, comp_id:%d", __func__, comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_r2y_funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	DDPINFO("%s, &priv->ddp_comp:0x%lx", __func__, (unsigned long)&priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_r2y_component_ops);

	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_r2y_remove(struct platform_device *pdev)
{
	struct mtk_disp_r2y *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_r2y_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_r2y_driver_dt_match[] = {
	{.compatible = "mediatek,mt6989-disp1-r2y",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_r2y_driver_dt_match);

struct platform_driver mtk_disp_r2y_driver = {
	.probe = mtk_disp_r2y_probe,
	.remove = mtk_disp_r2y_remove,
	.driver = {
		.name = "mediatek-disp1-r2y",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_r2y_driver_dt_match,
	},
};
