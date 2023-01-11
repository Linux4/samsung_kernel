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
#include <linux/clk/mmpdcstat.h>

#include "pm_domain.h"

#define MMP_PD_POWER_ON_LATENCY	0
#define MMP_PD_POWER_OFF_LATENCY	0

static struct mmp_pd_common_data gc3d_data = {
	.id			= 0,
	.name			= "power-domain-gc3d",
};

/* GC 3D/2D pwr function */
#define APMU_ISLD_GC3D_PWRCTRL	0x20c
#define APMU_ISLD_GC_CTRL	0x1b4
#define APMU_GC3D_CKGT		0x43c
#define APMU_GC3D_CLKCTRL	0x174
#define APMU_GC3D_RSTCTRL	0x170

#define HWMODE_EN	(1u << 0)
#define PWRUP		(1u << 1)
#define PWR_STATUS	(1u << 4)
#define REDUN_STATUS	(1u << 5)
#define INT_CLR		(1u << 6)
#define INT_MASK	(1u << 7)
#define INT_STATUS	(1u << 8)

#define X2H_CKGT_DISABLE	(1u << 0)

#define GC3D_ACLK_RST	(1u << 0)
#define GC3D_CLK1X_RST	(1u << 1)
#define GC3D_HCLK_RST	(1u << 2)
#define GC3D_PWRON_RST	(1u << 7)
#define GC3D_FC_EN	(1u << 9)

#define GC3D_ACLK_DIV_MASK	(7 << 0)
#define GC3D_ACLK_DIV_SHIFT	0

#define GC3D_ACLKSRC_SEL_MASK	(7 << 4)
#define GC3D_ACLKSRC_SEL_SHIFT	4

#define GC3D_CLK1X_DIV_MASK	(7 << 8)
#define GC3D_CLK1X_DIV_SHIFT	8

#define GC3D_CLK1X_CLKSRC_SEL_MASK	(7 << 12)
#define GC3D_CLK1X_CLKSRC_SEL_SHIFT	12

#define GC3D_CLKSH_DIV_MASK	(7 << 16)
#define GC3D_CLKSH_DIV_SHIFT	16

#define GC3D_CLKSH_CLKSRC_SEL_MASK	(7 << 20)
#define GC3D_CLKSH_CLKSRC_SEL_SHIFT	20

#define GC3D_HCLK_EN	(1u << 24)
#define GC3D_UPDATE_RTCWTC	(1u << 31)

#define GC3D_FAB_CKGT_DISABLE	(1u << 1)
#define MC_P4_CKGT_DISABLE	(1u << 0)

#define CMEM_DMMYCLK_EN	(1u << 4)

/* used to protect gc2d/gc3d power sequence */
static DEFINE_SPINLOCK(gc3d_pwr_lock);

static int  mmp_pd_gc3d_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_common *pd = container_of(domain,
			struct mmp_pd_common, genpd);
	unsigned int regval, divval;
	unsigned int timeout;
	void __iomem *apmu_base;

	apmu_base = pd->reg_base;

	spin_lock(&gc3d_pwr_lock);

	/* 1. Disable GC3D fabric dynamic clock gating. */
	regval = readl(apmu_base + APMU_GC3D_CKGT);
	regval |= (GC3D_FAB_CKGT_DISABLE | MC_P4_CKGT_DISABLE);
	writel(regval, apmu_base + APMU_GC3D_CKGT);

	/* 2. Assert GC3D ACLK reset. */
	regval = readl(apmu_base + APMU_GC3D_RSTCTRL);
	regval &= ~GC3D_ACLK_RST;
	writel(regval, apmu_base + APMU_GC3D_RSTCTRL);

	/* 3. Enable HWMODE to power up the island and clear interrupt mask */
	regval = readl(apmu_base + APMU_ISLD_GC3D_PWRCTRL);
	regval |= HWMODE_EN;
	writel(regval, apmu_base + APMU_ISLD_GC3D_PWRCTRL);

	regval = readl(apmu_base + APMU_ISLD_GC3D_PWRCTRL);
	regval &= ~INT_MASK;
	writel(regval, apmu_base + APMU_ISLD_GC3D_PWRCTRL);

	/* 4. Power up the island. */
	regval = readl(apmu_base + APMU_ISLD_GC3D_PWRCTRL);
	regval |= PWRUP;
	writel(regval, apmu_base + APMU_ISLD_GC3D_PWRCTRL);

	/*
	 * 5. Wait for active interrupt pending, indicating
	 * completion of island power up sequence.
	 * */
	timeout = 1000;
	do {
		if (--timeout == 0) {
			WARN(1, "GC3D: active interrupt pending!\n");
			return -EBUSY;
		}
		udelay(10);
		regval = readl(apmu_base + APMU_ISLD_GC3D_PWRCTRL);
	} while (!(regval & INT_STATUS));

	/*
	 * 6. The island is now powered up. Clear the interrupt and
	 *    set the interrupt masks.
	 */
	regval = readl(apmu_base + APMU_ISLD_GC3D_PWRCTRL);
	regval |= INT_CLR | INT_MASK;
	writel(regval, apmu_base + APMU_ISLD_GC3D_PWRCTRL);

	/* 7. Enable Dummy Clocks to SRAMs. */
	regval = readl(apmu_base + APMU_ISLD_GC_CTRL);
	regval |= CMEM_DMMYCLK_EN;
	writel(regval, apmu_base + APMU_ISLD_GC_CTRL);

	/* 8. Wait for 500ns. */
	udelay(1);

	/* 9. Disable Dummy Clocks to SRAMs. */
	regval = readl(apmu_base + APMU_ISLD_GC_CTRL);
	regval &= ~CMEM_DMMYCLK_EN;
	writel(regval, apmu_base + APMU_ISLD_GC_CTRL);

	/* 10. set FC_EN to 0. */
	regval = readl(apmu_base + APMU_GC3D_RSTCTRL);
	regval &= ~GC3D_FC_EN;
	writel(regval, apmu_base + APMU_GC3D_RSTCTRL);

	/* 11. Program PMUA_GC3D_CLKCTRL to enable clocks. */
	/* Enable GC3D AXI clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	divval = (regval & GC3D_ACLK_DIV_MASK) >> GC3D_ACLK_DIV_SHIFT;
	if (!divval) {
		regval &= ~GC3D_ACLK_DIV_MASK;
		regval |= (2 << GC3D_ACLK_DIV_SHIFT);
		writel(regval, apmu_base + APMU_GC3D_CLKCTRL);
	}

	/* Enalbe GC3D CLK1X clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	divval = (regval & GC3D_CLK1X_DIV_MASK) >> GC3D_CLK1X_DIV_SHIFT;
	if (!divval) {
		regval &= ~GC3D_CLK1X_DIV_MASK;
		regval |= (2 << GC3D_CLK1X_DIV_SHIFT);
		writel(regval, apmu_base + APMU_GC3D_CLKCTRL);
	}

	/* Enable GC3D CLKSH clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	divval = (regval & GC3D_CLKSH_DIV_MASK) >> GC3D_CLKSH_DIV_SHIFT;
	if (!divval) {
		regval &= ~GC3D_CLKSH_DIV_MASK;
		regval |= (2 << GC3D_CLKSH_DIV_SHIFT);
		writel(regval, apmu_base + APMU_GC3D_CLKCTRL);
	}

	/* Enable GC3D HCLK clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	regval |= GC3D_HCLK_EN;
	writel(regval, apmu_base + APMU_GC3D_CLKCTRL);

	/* 12. Enable Frequency change. */
	regval = readl(apmu_base + APMU_GC3D_RSTCTRL);
	regval |= GC3D_FC_EN;
	writel(regval, apmu_base + APMU_GC3D_RSTCTRL);

	/* 13. Wait 32 cycles of the slowest clock. */
	udelay(1);

	/* 14. De-assert GC3D PWRON Reset. */
	regval = readl(apmu_base + APMU_GC3D_RSTCTRL);
	regval |= GC3D_PWRON_RST;
	writel(regval, apmu_base + APMU_GC3D_RSTCTRL);

	/* 15. De-assert GC3D ACLK Reset, CLK1X Reset and HCLK Reset. */
	regval = readl(apmu_base + APMU_GC3D_RSTCTRL);
	regval |= (GC3D_ACLK_RST | GC3D_CLK1X_RST | GC3D_HCLK_RST);
	writel(regval, apmu_base + APMU_GC3D_RSTCTRL);

	/* 16. Wait for 1000ns. */
	udelay(1);

	/* 16. Enable GC3D fabric dynamic clock gating. */
	regval = readl(apmu_base + APMU_GC3D_CKGT);
	regval &= ~(GC3D_FAB_CKGT_DISABLE | MC_P4_CKGT_DISABLE);
	writel(regval, apmu_base + APMU_GC3D_CKGT);

	clk_dcstat_event(pd->clk, PWR_ON, 0);

	spin_unlock(&gc3d_pwr_lock);

	return 0;
}

static int mmp_pd_gc3d_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_common *pd = container_of(domain,
			struct mmp_pd_common, genpd);
	unsigned int regval;
	void __iomem *apmu_base;

	apmu_base = pd->reg_base;

	spin_lock(&gc3d_pwr_lock);

	/* 1. Disable all clocks. */
	/* Disable GC3D HCLK clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	regval &= ~GC3D_HCLK_EN;
	writel(regval, apmu_base + APMU_GC3D_CLKCTRL);

	/* Disable GC3D SHADER clock */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	regval &= ~GC3D_CLKSH_DIV_MASK;
	writel(regval, apmu_base + APMU_GC3D_CLKCTRL);

	/* Disable GC3D CLK1X clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	regval &= ~GC3D_CLK1X_DIV_MASK;
	writel(regval, apmu_base + APMU_GC3D_CLKCTRL);

	/* Disable GC3D AXI clock. */
	regval = readl(apmu_base + APMU_GC3D_CLKCTRL);
	regval &= ~GC3D_ACLK_DIV_MASK;
	writel(regval, apmu_base + APMU_GC3D_CLKCTRL);

	/* 2. Assert HCLK and CLK1X resets. */
	regval = readl(apmu_base + APMU_GC3D_RSTCTRL);
	regval &= ~(GC3D_CLK1X_RST | GC3D_HCLK_RST);
	writel(regval, apmu_base + APMU_GC3D_RSTCTRL);

	/* 3. Power down the island. */
	regval = readl(apmu_base + APMU_ISLD_GC3D_PWRCTRL);
	regval &= ~PWRUP;
	writel(regval, apmu_base + APMU_ISLD_GC3D_PWRCTRL);

	clk_dcstat_event(pd->clk, PWR_OFF, 0);

	spin_lock(&gc3d_pwr_lock);

	return 0;
}

static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-gc3d",
		.data = (void *)&gc3d_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_gc3d_probe(struct platform_device *pdev)
{
	struct mmp_pd_common *pd;
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

	pd->tag = PD_TAG;
	/* Some power domain may need clk for power on. */
	pd->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pd->clk))
		pd->clk = NULL;

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
	pd->genpd.power_on = mmp_pd_gc3d_power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = mmp_pd_gc3d_power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_gc3d_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_gc3d_driver = {
	.probe		= mmp_pd_gc3d_probe,
	.remove		= mmp_pd_gc3d_remove,
	.driver		= {
		.name	= "mmp-pd-gc3d",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_gc3d_init(void)
{
	return platform_driver_register(&mmp_pd_gc3d_driver);
}
subsys_initcall(mmp_pd_gc3d_init);
