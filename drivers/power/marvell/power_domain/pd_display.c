#include <linux/module.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>

#include "pm_domain.h"

#define MMP_PD_POWER_ON_LATENCY	0
#define MMP_PD_POWER_OFF_LATENCY	0

struct mmp_pd_display_data {
	int id;
	char *name;
	int (*power_on)(struct generic_pm_domain *domain);
	int (*power_off)(struct generic_pm_domain *domain);
};

struct mmp_pd_display {
	struct generic_pm_domain genpd;
	void __iomem *reg_base;
	struct clk *axi_clk;
	struct clk *esc_clk;
	struct clk *disp1_clk;
	struct clk *vdma_clk;
	struct clk *hclk_clk;
	struct device *dev;
	/* latency for us. */
	u32 power_on_latency;
	u32 power_off_latency;
	const struct mmp_pd_display_data *data;
};

enum {
	MMP_PD_DISPLAY_PXA1U88,
	MMP_PD_DISPLAY_PXA1928,
};

#define PMUA_DISP_RSTCTRL 0x180
#define ACLK_RSTN		(1 << 0)
#define ACLK_PORSTN		(1 << 1)
#define VDMA_PORSTN	(1 << 2)
#define HCLK_RSTN		(1 << 3)
#define VDMA_CLK_RSTN	(1 << 4)
#define ESC_CLK_RSTN	(1 << 5)

#define CIU_FABRIC1_CKGT 0x464
#define X2H_CKGT_DISABLE	(1 << 0)

#define PMUA_ISLD_LCD_PWRCTRL 0x204
#define HWMODE_EN	(1 << 0)
#define PWRUP		(1 << 1)
#define INT_ISLD_CLR		(1 << 6)
#define INT_ISLD_MASK	(1 << 7)
#define INT_ISLD_STATUS		(1 << 8)

#define PMUA_ISLD_LCD_CTRL 0x1ac
#define DMMY_CLK	(1 << 4)

#define APMU_DEBUG 0x088
#define DSI_PHY_DVM_MASK (1 << 31)

static DEFINE_SPINLOCK(mmp_pd_display_lock);

static int mmp_pd_pxa1u88_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_display *pd = container_of(domain,
			struct mmp_pd_display, genpd);
	u32 val;

	if (pd->hclk_clk)
		clk_prepare_enable(pd->hclk_clk);
	if (pd->esc_clk)
		clk_prepare_enable(pd->esc_clk);

	val = readl_relaxed(pd->reg_base + APMU_DEBUG) | DSI_PHY_DVM_MASK;
	writel_relaxed(val, pd->reg_base + APMU_DEBUG);

	if (pd->disp1_clk)
		clk_prepare_enable(pd->disp1_clk);

	return 0;
}

static int mmp_pd_pxa1u88_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_display *pd = container_of(domain,
			struct mmp_pd_display, genpd);
	u32 val;

	if (pd->esc_clk)
		clk_disable_unprepare(pd->esc_clk);

	val = readl_relaxed(pd->reg_base + APMU_DEBUG);
	val &= ~(DSI_PHY_DVM_MASK);
	writel_relaxed(val, pd->reg_base + APMU_DEBUG);

	if (pd->disp1_clk)
		clk_disable_unprepare(pd->disp1_clk);
	if (pd->hclk_clk)
		clk_disable_unprepare(pd->hclk_clk);

	return 0;
}

static int mmp_pd_pxa1928_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_display *pd = container_of(domain,
			struct mmp_pd_display, genpd);
	u32 val;
	int count = 0;

	/* 1. clock enable, in prepare process */
	if (pd->axi_clk)
		clk_prepare_enable(pd->axi_clk);
	if (pd->esc_clk)
		clk_prepare_enable(pd->esc_clk);
	if (pd->disp1_clk)
		clk_prepare_enable(pd->disp1_clk);
	if (pd->vdma_clk)
		clk_prepare_enable(pd->vdma_clk);

	spin_lock(&mmp_pd_display_lock);

	/* 2. disable fabirc x2h dynamical clock gating */
	val = readl_relaxed(pd->reg_base + CIU_FABRIC1_CKGT) | X2H_CKGT_DISABLE;
	writel_relaxed(val, pd->reg_base + CIU_FABRIC1_CKGT);

	/* 3. power up */
	val = readl_relaxed(pd->reg_base + PMUA_ISLD_LCD_PWRCTRL) | HWMODE_EN;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_PWRCTRL);
	val &= ~INT_ISLD_MASK;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_PWRCTRL);
	val |= PWRUP;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_PWRCTRL);
	while (!(readl_relaxed(pd->reg_base + PMUA_ISLD_LCD_PWRCTRL) & INT_ISLD_STATUS)) {
		count++;
		if (count > 1000) {
			pr_err("Timeout for polling active interrupt\n");
			return -1;
		}
	}
	val = readl_relaxed(pd->reg_base + PMUA_ISLD_LCD_PWRCTRL) | INT_ISLD_MASK | INT_ISLD_CLR;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_PWRCTRL);

	/* 4. dummy clock for SRAM access */
	val = readl_relaxed(pd->reg_base + PMUA_ISLD_LCD_CTRL) | DMMY_CLK;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_CTRL);
	udelay(2);
	val &= ~DMMY_CLK;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_CTRL);

	/* 5. release from reset */
	val = readl_relaxed(pd->reg_base + PMUA_DISP_RSTCTRL) | ACLK_PORSTN | VDMA_PORSTN;
	writel_relaxed(val, pd->reg_base + PMUA_DISP_RSTCTRL);
	udelay(2);
	val = readl_relaxed(pd->reg_base + PMUA_DISP_RSTCTRL) | HCLK_RSTN
		| VDMA_CLK_RSTN | ESC_CLK_RSTN;
	writel_relaxed(val, pd->reg_base + PMUA_DISP_RSTCTRL);
	udelay(2);
	val = readl_relaxed(pd->reg_base + PMUA_DISP_RSTCTRL) | ACLK_RSTN;
	writel_relaxed(val, pd->reg_base + PMUA_DISP_RSTCTRL);
	udelay(2);

	/* 6. enable fabric x2h dynamical clock gating */
	val = readl_relaxed(pd->reg_base + CIU_FABRIC1_CKGT);
	val &= ~X2H_CKGT_DISABLE;
	writel_relaxed(val, pd->reg_base + CIU_FABRIC1_CKGT);

	spin_unlock(&mmp_pd_display_lock);

	return 0;
}

static int mmp_pd_pxa1928_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_display *pd = container_of(domain,
			struct mmp_pd_display, genpd);
	u32 val;

	/* 1. disable clk */
	if (pd->axi_clk)
		clk_disable_unprepare(pd->axi_clk);
	if (pd->esc_clk)
		clk_disable_unprepare(pd->esc_clk);
	if (pd->disp1_clk)
		clk_disable_unprepare(pd->disp1_clk);
	if (pd->vdma_clk)
		clk_disable_unprepare(pd->vdma_clk);

	spin_lock(&mmp_pd_display_lock);

	/* 2. reset all clks*/
	val = readl_relaxed(pd->reg_base + PMUA_DISP_RSTCTRL);
	val &= ~0x3f;
	writel_relaxed(val, pd->reg_base + PMUA_DISP_RSTCTRL);

	/* 3. power down the island */
	val = readl_relaxed(pd->reg_base + PMUA_ISLD_LCD_PWRCTRL);
	val &= ~PWRUP;
	writel_relaxed(val, pd->reg_base + PMUA_ISLD_LCD_PWRCTRL);

	spin_unlock(&mmp_pd_display_lock);

	return 0;
}

static struct mmp_pd_display_data display_pxa1u88_data = {
	.id			= MMP_PD_DISPLAY_PXA1U88,
	.name			= "power-domain-display",
	.power_on = mmp_pd_pxa1u88_power_on,
	.power_off = mmp_pd_pxa1u88_power_off,
};

static struct mmp_pd_display_data display_pxa1928_data = {
	.id			= MMP_PD_DISPLAY_PXA1928,
	.name			= "power-domain-display",
	.power_on = mmp_pd_pxa1928_power_on,
	.power_off = mmp_pd_pxa1928_power_off,
};


static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-display-pxa1u88",
		.data = (void *)&display_pxa1u88_data,
	},
	{
		.compatible = "marvell,power-domain-display-pxa1928",
		.data = (void *)&display_pxa1928_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_display_probe(struct platform_device *pdev)
{
	struct mmp_pd_display *pd;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id;
	struct resource *res;
	int ret;
	u32 latency;

	if (!np)
		return -EINVAL;

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	of_id = of_match_device(of_mmp_pd_match, &pdev->dev);
	if (!of_id)
		return -ENODEV;

	pd->data = of_id->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	pd->reg_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!pd->reg_base)
		return -EINVAL;

	if (pd->data->id == MMP_PD_DISPLAY_PXA1928) {
		/* Some power domain may need clk for power on. */
		pd->axi_clk = devm_clk_get(&pdev->dev, "axi_clk");
		if (IS_ERR(pd->axi_clk))
			pd->axi_clk = NULL;

		/* Some power domain may need clk for power on. */
		pd->vdma_clk = devm_clk_get(&pdev->dev, "vdma_clk_gate");
		if (IS_ERR(pd->vdma_clk))
			pd->vdma_clk = NULL;

		/* Some power domain may need clk for power on. */
		pd->esc_clk = devm_clk_get(&pdev->dev, "esc_clk");
		if (IS_ERR(pd->esc_clk))
			pd->esc_clk = NULL;

		/* Some power domain may need clk for power on. */
		pd->disp1_clk = devm_clk_get(&pdev->dev, "disp1_clk_gate");
		if (IS_ERR(pd->disp1_clk))
			pd->disp1_clk = NULL;
	} else {
		/* Some power domain may need clk for power on. */
		pd->esc_clk = devm_clk_get(&pdev->dev, "esc_clk");
		if (IS_ERR(pd->esc_clk))
			pd->esc_clk = NULL;

		/* Some power domain may need clk for power on. */
		pd->disp1_clk = devm_clk_get(&pdev->dev, "disp1_clk_gate");
		if (IS_ERR(pd->disp1_clk))
			pd->disp1_clk = NULL;

		/* Some power domain may need clk for power on. */
		pd->hclk_clk = devm_clk_get(&pdev->dev, "LCDCIHCLK");
		if (IS_ERR(pd->hclk_clk))
			pd->hclk_clk = NULL;
	}

	latency = MMP_PD_POWER_ON_LATENCY;
	if (of_find_property(np, "power-on-latency", NULL)) {
		ret = of_property_read_u32(np, "power-on-latency",
						&latency);
		if (ret)
			return ret;
	}
	pd->power_on_latency = latency;

	latency = MMP_PD_POWER_OFF_LATENCY;
	if (of_find_property(np, "power-off-latency-ns", NULL)) {
		ret = of_property_read_u32(np, "power-off-latency-ns",
						&latency);
		if (ret)
			return ret;
	}
	pd->power_off_latency = latency;

	pd->dev = &pdev->dev;

	pd->genpd.of_node = np;
	pd->genpd.name = pd->data->name;
	pd->genpd.power_on = pd->data->power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = pd->data->power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_display_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_display_driver = {
	.probe		= mmp_pd_display_probe,
	.remove		= mmp_pd_display_remove,
	.driver		= {
		.name	= "mmp-pd-display",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_display_init(void)
{
	return platform_driver_register(&mmp_pd_display_driver);
}
subsys_initcall(mmp_pd_display_init);
