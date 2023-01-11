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

static struct mmp_pd_common_data hantro_data = {
	.id			= 0,
	.name			= "power-domain-hantro",
};

/* VPU pwr function */
#define APMU_ISLD_VPU_CTRL	0x1b0
#define APMU_VPU_RSTCTRL	0x1f0
#define APMU_VPU_CLKCTRL	0x1f4
#define APMU_ISLD_VPU_PWRCTRL	0x208
#define APMU_VPU_CKGT		0x46c

#define HWMODE_EN	(1u << 0)
#define PWRUP		(1u << 1)
#define PWR_STATUS	(1u << 4)
#define REDUN_STATUS	(1u << 5)
#define INT_CLR		(1u << 6)
#define INT_MASK	(1u << 7)
#define INT_STATUS	(1u << 8)
#define CMEM_DMMYCLK_EN		(1u << 4)

#define VPU_ACLK_RST	(1u << 0)
#define VPU_DEC_CLK_RST	(1u << 1)
#define VPU_ENC_CLK_RST	(1u << 2)
#define VPU_HCLK_RST	(1u << 3)
#define VPU_PWRON_RST	(1u << 7)
#define VPU_FC_EN	(1u << 9)

#define VPU_ACLK_DIV_MASK	(7 << 0)
#define VPU_ACLK_DIV_SHIFT	0

#define VPU_ACLKSRC_SEL_MASK	(7 << 4)
#define VPU_ACLKSRC_SEL_SHIFT	4

#define VPU_DCLK_DIV_MASK	(7 << 8)
#define VPU_DCLK_DIV_SHIFT	8

#define VPU_DCLKSRC_SEL_MASK	(7 << 12)
#define VPU_DCLKSRC_SEL_SHIFT	12

#define VPU_ECLK_DIV_MASK	(7 << 16)
#define VPU_ECLK_DIV_SHIFT	16

#define VPU_ECLKSRC_SEL_MASK	(7 << 20)
#define VPU_ECLKSRC_SEL_SHIFT	20

#define VPU_UPDATE_RTCWTC	(1u << 31)

/* used to protect gc2d/gc3d power sequence */
static DEFINE_SPINLOCK(hantro_pwr_lock);

static int mmp_pd_hantro_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_common *pd = container_of(domain,
			struct mmp_pd_common, genpd);

	unsigned int regval, divval;
	unsigned int timeout;
	void __iomem *apmu_base;

	apmu_base = pd->reg_base;

	spin_lock(&hantro_pwr_lock);

	/* 1. Assert AXI Reset. */
	regval = readl(apmu_base + APMU_VPU_RSTCTRL);
	regval &= ~VPU_ACLK_RST;
	writel(regval, apmu_base + APMU_VPU_RSTCTRL);

	/* 2. Enable HWMODE to power up the island and clear interrupt mask. */
	regval = readl(apmu_base + APMU_ISLD_VPU_PWRCTRL);
	regval |= HWMODE_EN;
	writel(regval, apmu_base + APMU_ISLD_VPU_PWRCTRL);

	regval = readl(apmu_base + APMU_ISLD_VPU_PWRCTRL);
	regval &= ~INT_MASK;
	writel(regval, apmu_base + APMU_ISLD_VPU_PWRCTRL);

	/* 3. Power up the island. */
	regval = readl(apmu_base + APMU_ISLD_VPU_PWRCTRL);
	regval |= PWRUP;
	writel(regval, apmu_base + APMU_ISLD_VPU_PWRCTRL);

	/*
	 * 4. Wait for active interrupt pending, indicating
	 * completion of island power up sequence.
	 * */
	timeout = 1000;
	do {
		if (--timeout == 0) {
			WARN(1, "VPU: active interrupt pending!\n");
			return -EBUSY;
		}
		udelay(10);
		regval = readl(apmu_base + APMU_ISLD_VPU_PWRCTRL);
	} while (!(regval & INT_STATUS));

	/*
	 * 5. The island is now powered up. Clear the interrupt and
	 *    set the interrupt masks.
	 */
	regval = readl(apmu_base + APMU_ISLD_VPU_PWRCTRL);
	regval |= INT_CLR | INT_MASK;
	writel(regval, apmu_base + APMU_ISLD_VPU_PWRCTRL);

	/* 6. Enable Dummy Clocks to SRAMs. */
	regval = readl(apmu_base + APMU_ISLD_VPU_CTRL);
	regval |= CMEM_DMMYCLK_EN;
	writel(regval, apmu_base + APMU_ISLD_VPU_CTRL);

	/* 7. Wait for 500ns. */
	udelay(1);

	/* 8. Disable Dummy Clocks to SRAMs. */
	regval = readl(apmu_base + APMU_ISLD_VPU_CTRL);
	regval &= ~CMEM_DMMYCLK_EN;
	writel(regval, apmu_base + APMU_ISLD_VPU_CTRL);

	/* 9. Disable VPU fabric Dynamic Clock gating. */
	regval = readl(apmu_base + APMU_VPU_CKGT);
	regval |= 0x1f;
	writel(regval, apmu_base + APMU_VPU_CKGT);

	/* 10. De-assert VPU HCLK Reset. */
	regval = readl(apmu_base + APMU_VPU_RSTCTRL);
	regval |= VPU_HCLK_RST;
	writel(regval, apmu_base + APMU_VPU_RSTCTRL);

	/* 11. Set PMUA_VPU_RSTCTRL[9] = 0x0. */
	regval = readl(apmu_base + APMU_VPU_RSTCTRL);
	regval &= ~VPU_FC_EN;
	writel(regval, apmu_base + APMU_VPU_RSTCTRL);

	/* 12. program PMUA_VPU_CLKCTRL to enable clks*/
	/* Enable VPU AXI Clock. */
	regval = readl(apmu_base + APMU_VPU_CLKCTRL);
	divval = (regval & VPU_ACLK_DIV_MASK) >> VPU_ACLK_DIV_SHIFT;
	if (!divval) {
		regval &= ~VPU_ACLK_DIV_MASK;
		regval |= (2 << VPU_ACLK_DIV_SHIFT);
		writel(regval, apmu_base + APMU_VPU_CLKCTRL);
	}

	/* Enable Encoder Clock. */
	regval = readl(apmu_base + APMU_VPU_CLKCTRL);
	divval = (regval & VPU_ECLK_DIV_MASK) >> VPU_ECLK_DIV_SHIFT;
	if (!divval) {
		regval &= ~VPU_ECLK_DIV_MASK;
		regval |= (2 << VPU_ECLK_DIV_SHIFT);
		writel(regval, apmu_base + APMU_VPU_CLKCTRL);
	}

	/* Enable Decoder Clock. */
	regval = readl(apmu_base + APMU_VPU_CLKCTRL);
	divval = (regval & VPU_DCLK_DIV_MASK) >> VPU_DCLK_DIV_SHIFT;
	if (!divval) {
		regval &= ~VPU_DCLK_DIV_MASK;
		regval |= (2 << VPU_DCLK_DIV_SHIFT);
		writel(regval, apmu_base + APMU_VPU_CLKCTRL);
	}

	/* 13. Enable VPU Frequency Change. */
	regval = readl(apmu_base + APMU_VPU_RSTCTRL);
	regval |= VPU_FC_EN;
	writel(regval, apmu_base + APMU_VPU_RSTCTRL);

	/* 14. De-assert VPU ACLK/DCLK/ECLK Reset. */
	regval = readl(apmu_base + APMU_VPU_RSTCTRL);
	regval |= VPU_ACLK_RST | VPU_DEC_CLK_RST | VPU_ENC_CLK_RST;
	writel(regval, apmu_base + APMU_VPU_RSTCTRL);

	/* 15. Enable VPU fabric Dynamic Clock gating. */
	regval = readl(apmu_base + APMU_VPU_CKGT);
	regval &= ~0x1f;
	writel(regval, apmu_base + APMU_VPU_CKGT);

	/* Disable Peripheral Clock. */
	regval = readl(apmu_base + APMU_VPU_CLKCTRL);
	regval &= ~(VPU_DCLK_DIV_MASK | VPU_ECLK_DIV_MASK);
	writel(regval, apmu_base + APMU_VPU_CLKCTRL);

	clk_dcstat_event(pd->clk, PWR_ON, 0);

	spin_unlock(&hantro_pwr_lock);

	return 0;
}

static int mmp_pd_hantro_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_common *pd = container_of(domain,
			struct mmp_pd_common, genpd);
	unsigned int regval;
	void __iomem *apmu_base;

	apmu_base = pd->reg_base;

	spin_lock(&hantro_pwr_lock);

	/* 1. Assert Bus Reset and Perpheral Reset. */
	regval = readl(apmu_base + APMU_VPU_RSTCTRL);
	regval &= ~(VPU_DEC_CLK_RST | VPU_ENC_CLK_RST
			| VPU_HCLK_RST);
	writel(regval, apmu_base + APMU_VPU_RSTCTRL);

	/* 2. Disable Bus and Peripheral Clock. */
	regval = readl(apmu_base + APMU_VPU_CLKCTRL);
	regval &= ~(VPU_ACLK_DIV_MASK | VPU_DCLK_DIV_MASK | VPU_ECLK_DIV_MASK);
	writel(regval, apmu_base + APMU_VPU_CLKCTRL);

	/* 3. Power down the island. */
	regval = readl(apmu_base + APMU_ISLD_VPU_PWRCTRL);
	regval &= ~PWRUP;
	writel(regval, apmu_base + APMU_ISLD_VPU_PWRCTRL);

	clk_dcstat_event(pd->clk, PWR_OFF, 0);

	spin_unlock(&hantro_pwr_lock);

	return 0;
}

static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-hantro",
		.data = (void *)&hantro_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_hantro_probe(struct platform_device *pdev)
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

	pd->tag = PD_TAG;
	pd->reg_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!pd->reg_base)
		return -EINVAL;

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
	pd->genpd.power_on = mmp_pd_hantro_power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = mmp_pd_hantro_power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_hantro_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_hantro_driver = {
	.probe		= mmp_pd_hantro_probe,
	.remove		= mmp_pd_hantro_remove,
	.driver		= {
		.name	= "mmp-pd-hantro",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_hantro_init(void)
{
	return platform_driver_register(&mmp_pd_hantro_driver);
}
subsys_initcall(mmp_pd_hantro_init);
