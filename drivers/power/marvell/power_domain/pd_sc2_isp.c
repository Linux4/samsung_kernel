#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/of_device.h>

#include "pm_domain.h"

#define PMUA_ISLD_ISP_PWRCTRL 0x214
#define		ISP_HWMODE_EN (0x1 << 0)
#define		ISP_PWRUP (0x1 << 1)
#define		ISP_PWR_STATUS (0x1 << 4)
#define		ISP_REDUN_STATUS (0x1 << 5)
#define		ISP_INT_CLR (0x1 << 6)
#define		ISP_INT_MASK (0x1 << 7)
#define		ISP_INT_STATUS (0x1 << 8)
#define		IPE0_HWMODE_EN (0x1 << 9)
#define		IPE0_PWRUP (0x1 << 10)
#define		IPE0_PWR_STATUS (0x1 << 13)
#define		IPE0_REDUN_STATUS (0x1 << 14)
#define		IPE0_INT_CLR (0x1 << 15)
#define		IPE0_INT_MASK (0x1 << 16)
#define		IPE0_INT_STATUS (0x1 << 17)
#define		IPE1_HWMODE_EN (0x1 << 18)
#define		IPE1_PWRUP (0x1 << 19)
#define		IPE1_PWR_STATUS (0x1 << 22)
#define		IPE1_REDUN_STATUS (0x1 << 23)
#define		IPE1_INT_CLR (0x1 << 24)
#define		IPE1_INT_MASK (0x1 << 25)
#define		IPE1_INT_STATUS (0x1 << 26)

#define PMUA_ISLD_ISP_CTRL 0x1a4
#define		CMEM_DMMYCLK_EN (0x1 << 4)
#define PMUA_ISP_RSTCTRL 0x1e0
#define		ALL_RST_MASK (0xff)
#define		RST_DEASSERT (0xfb)
#define		RST_ASSERT (0x06)
#define		P_PWDN (0x1 << 8)
#define		ARSTN (0x1 << 1)

#define		FC_EN (0x1 << 9)
#define PMUA_ISP_CLKCTRL 0x1e4
#define		CORE_CLKSRC_SEL (0x7 << 28)
#define		CORE_DIV (0xF << 24)
#define		PIPE_CLKSRC_SEL (0x7 << 20)
#define		PIPE_DIV (0x7 << 16)
#define		ACLK_CLKSRC_SEL (0x7 << 12)
#define		ACLK_DIV (0x7 << 8)
#define		CLK4X_CLKSRC_SEL (0x7 << 4)
#define		CLK4X_DIV (0x7 << 0)
#define PMUA_ISP_CLKCTRL2 0x1e8
#define		CSI_TXCLK_ESC_2LN_EN (0x1 << 9)
#define		CSI_TXCLK_ESC_4LN_EN (0x1 << 8)
#define		CSI_CLK_CLKSRC_SEL (0x7 << 4)
#define		CSI_CLK_DIV (0x7 << 0)

#define ISP_CKGT 0x68
#define		MC_P3_CKGT_DISABLE 0x1

#define PMUA_DEBUG_REG 0x88
#define		MIPI_CSI_DVDD_VALID	(0x3 << 27)
#define		MIPI_CSI_AVDD_VALID	(0x3 << 25)

#define AXI_PHYS_BASE	0xd4200000
#define PMUM_SC2_ISIM_CLK_CR	0xd4051140
#define CCIC_CTRL_1	0xd420a040

#define MMP_PD_POWER_ON_LATENCY 0
#define MMP_PD_POWER_OFF_LATENCY 0
struct mmp_pd_sc2_isp_data {
	int id;
	char *name;
	u32 reg_clk_res_ctrl;
	u32 bit_hw_mode;
	u32 bit_auto_pwr_on;
	u32 bit_pwr_stat;
};

struct mmp_pd_sc2_isp {
	struct generic_pm_domain genpd;
	void __iomem *reg_base;
	struct clk *clk;
	struct device *dev;
	/* latency for us. */
	u32 power_on_latency;
	u32 power_off_latency;
	const struct mmp_pd_sc2_isp_data *data;
};

enum {
	MMP_PD_SC2,
	MMP_PD_ISP,
};

static struct mmp_pd_sc2_isp_data sc2_data = {
	.id = MMP_PD_SC2,
	.name = "power-domain-sc2",
};
static struct mmp_pd_sc2_isp_data isp_data = {
	.id = MMP_PD_ISP,
	.name = "power-domain-isp",
};

/*
 * The below functions are used for sc2 power-up or
 * power-down.
 * The normal sequence for power up is:
 *   1. call sc2_power_switch(1)
 *   2. enable the clks by using clk-tree
 *   3. call sc2_deassert_reset(1)
 * The normal sequence for power down is:
 *   1. call sc2_deassert_reset(0)
 *   2. disalbe the clks by using clk-tree
 *   3. call sc2_power_switch(0)
 */

static void sc2_deassert_reset(int on)
{
	unsigned int regval;
	void __iomem *apmu_base;
	void __iomem *ciu_base;
	static void __iomem *ccic_ctrl1;

	apmu_base = ioremap(AXI_PHYS_BASE + 0x82800, SZ_4K);
	if (apmu_base == NULL) {
		pr_err("error to ioremap APMU base\n");
		return;
	}

	ciu_base = ioremap(AXI_PHYS_BASE + 0x82c00, 0x100);
	if (ciu_base == NULL) {
		pr_err("error to ioremap CIU base\n");
		iounmap(apmu_base);
		return;
	}

	if (ccic_ctrl1 == NULL)
		ccic_ctrl1 = ioremap(CCIC_CTRL_1, 4);

	if (on) {
		regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
		regval &= ~ALL_RST_MASK;
		regval |= RST_DEASSERT;
		writel(regval, apmu_base + PMUA_ISP_RSTCTRL);
		udelay(200);
		regval = readl(ciu_base + ISP_CKGT);
		regval &= ~(MC_P3_CKGT_DISABLE);
		writel(regval, ciu_base + ISP_CKGT);

		/*Enable mclk*/
		writel(readl(ccic_ctrl1) & ~(1 << 28), ccic_ctrl1);
	} else {
		regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
		regval &= ~ALL_RST_MASK;
		regval |= RST_ASSERT;
		writel(regval, apmu_base + PMUA_ISP_RSTCTRL);
	}

	iounmap(apmu_base);
	iounmap(ciu_base);
	return;
}

static int mmp_pd_sc2_isp_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_sc2_isp *pd = container_of(domain,
			struct mmp_pd_sc2_isp, genpd);
	void __iomem *apmu_base = pd->reg_base;
	unsigned int regval;
	unsigned int timeout = 1000;
	void __iomem *ciu_base;
	void __iomem *sc2_isim;

	sc2_isim = ioremap(PMUM_SC2_ISIM_CLK_CR, 8);
	if (sc2_isim == NULL) {
		pr_err("error to ioremap isim base\n");
		return -EINVAL;
	}

	ciu_base = ioremap(AXI_PHYS_BASE + 0x82c00, 0x100);
	if (ciu_base == NULL) {
		pr_err("error to ioremap CIU base\n");
		iounmap(sc2_isim);
		return -EINVAL;
	}

	/* 0. Keep SC2_ISLAND AXI RESET asserted */
	/* suppose clk setting will set this bit */
	regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
	regval &= ~(ARSTN);
	writel(regval, apmu_base + PMUA_ISP_RSTCTRL);

	/* 1. Disabling Dynamic Clock Gating before power up */
	regval = readl(ciu_base + ISP_CKGT);
	regval |= MC_P3_CKGT_DISABLE;
	writel(regval, ciu_base + ISP_CKGT);
	/* 2. Enable HWMODE to power up the island */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= ISP_HWMODE_EN;
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 3. Remove Interrupt Mask */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval &= ~(ISP_INT_MASK);
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 4. Power up the Island */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= ISP_PWRUP;
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 5. Wait for Island to be powered up */
	do {
		if (--timeout == 0) {
			WARN(1, "SC2: Island power up timeout!\n");
			goto out;
		}
		udelay(10);
		regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	} while (!(regval & ISP_PWR_STATUS));
	/* 6. Wait for Active Interrupt pending */
	timeout = 1000;
	do {
		if (--timeout == 0) {
			WARN(1, "SC2: Active Interrupt timeout!\n");
			goto out;
		}
		udelay(10);
		regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	} while (!(regval & ISP_INT_STATUS));
	/* 7. Clear the interrupt and set the interrupt mask */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= (ISP_INT_CLR | ISP_INT_MASK);
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 8. Enable OVT HW mode */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= IPE0_HWMODE_EN;
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 9. Remove OVT interrupt mask */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval &= ~(IPE0_INT_MASK);
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 10. Power up OVT */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= IPE0_PWRUP;
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 11. Wait for OVT Island to be powered up */
	timeout = 1000;
	do {
		if (--timeout == 0) {
			WARN(1, "SC2: OVT Island power up timeout!\n");
			goto out;
		}
		udelay(10);
		regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	} while (!(regval & IPE0_PWR_STATUS));
	/* 12. Wait for Active Interrupt pending */
	timeout = 1000;
	do {
		if (--timeout == 0) {
			WARN(1, "SC2: Active Interrupt timeout!\n");
			goto out;
		}
		udelay(10);
		regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	} while (!(regval & IPE0_INT_STATUS));
	/* 13. Clear the interrupt and set the interrupt mask */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= (IPE0_INT_CLR | IPE0_INT_MASK);
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 14. Enable CSI Digital VDD and Anolog VDD*/
	regval = readl(apmu_base + PMUA_DEBUG_REG);
	regval |= (MIPI_CSI_DVDD_VALID | MIPI_CSI_AVDD_VALID);
	writel(regval, apmu_base + PMUA_DEBUG_REG);
	/* 15. Enable OVTISP Power */
	regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
	regval &= ~(P_PWDN);
	writel(regval, apmu_base + PMUA_ISP_RSTCTRL);
	/* 16. Enable Dummy Clocks, wait for 500us, Disable Dummy Clocks*/
	regval = readl(apmu_base + PMUA_ISLD_ISP_CTRL);
	regval |= CMEM_DMMYCLK_EN;
	writel(regval, apmu_base + PMUA_ISLD_ISP_CTRL);
	udelay(500);
	regval = readl(apmu_base + PMUA_ISLD_ISP_CTRL);
	regval &= ~(CMEM_DMMYCLK_EN);
	writel(regval, apmu_base + PMUA_ISLD_ISP_CTRL);

	mdelay(5);
	/* 17. Disable Frequency Change */
	regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
	regval &= ~(FC_EN);
	writel(regval, apmu_base + PMUA_ISP_RSTCTRL);
	/* 18. Enable Island AXI clock to 312M*/
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
	regval &= ~(ACLK_CLKSRC_SEL); /* select 312MHz as source */
	regval &= ~(ACLK_DIV);
	regval |= 0x100; /* set divider = 1 */
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);
	/* 19. Enable island clk4x to 624M*/
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
	regval &= ~(CLK4X_CLKSRC_SEL); /* select 624MHz as source */
	regval &= ~(CLK4X_DIV);
	regval |= 0x1; /* set divider = 1 */
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);
	/* 20. Enable OVISP Core clock to 156M*/
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
		regval &= ~(CORE_CLKSRC_SEL);
		regval &= ~(CORE_DIV);
		regval |= (0x2<<24);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);

	/* 21. Enable  OVISP pipeline clock to 312M*/
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
		regval &= ~(PIPE_CLKSRC_SEL);
		regval &= ~(PIPE_DIV);
		regval |= (0x1<<16);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);

	udelay(1000);
	/* 22. Enable CSI_CLK to 624M*/
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL2);
	regval &= ~(CSI_CLK_CLKSRC_SEL); /* select 624MHz as source */
	regval |= (0x0<<4);
	regval &= ~(CSI_CLK_DIV);
	regval |= 0x1; /* set divider = 1 */
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL2);
	/* 23. Enable CSI TCLK DPHY4LN */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL2);
	regval |= CSI_TXCLK_ESC_4LN_EN;
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL2);
	/* 24. Enable CSI TCLK DPHY2LN */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL2);
	regval |= CSI_TXCLK_ESC_2LN_EN;
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL2);

	udelay(1000);
	/* 25. Enable Frequency Change */

	writel(7, sc2_isim + 0);
	writel(7, sc2_isim + 4);
	regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
	regval |= FC_EN;
	writel(regval, apmu_base + PMUA_ISP_RSTCTRL);
	/* 26. deassert reset */
	sc2_deassert_reset(1);

out:
	iounmap(ciu_base);
	iounmap(sc2_isim);
	return 0;
}

static int mmp_pd_sc2_isp_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_sc2_isp *pd = container_of(domain,
			struct mmp_pd_sc2_isp, genpd);

	void __iomem *apmu_base = pd->reg_base;
	unsigned int regval;
	unsigned int timeout = 1000;

	/* 1. Assert the resets */
	sc2_deassert_reset(0);
	/* 2. Disable Frequency Change */
	regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
	regval &= ~(FC_EN);
	writel(regval, apmu_base + PMUA_ISP_RSTCTRL);
	/* 3. Disable Island AXI clock */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
	regval &= ~(ACLK_DIV);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);
	/* 4. Disable clk4x clocks */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
	regval &= ~(CLK4X_DIV);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);

	/* 5. Disable OVISP Core clock */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
	regval &= ~(CORE_DIV);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);

	/* 6. Disable OVISP Pipeline clock */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL);
	regval &= ~(PIPE_DIV);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL);

	/* 7. wait for 50ns */
	udelay(1000);
	/* 8. Disable CSI_CLK */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL2);
	regval &= ~(CSI_CLK_DIV);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL2);
	/* 9. Disable CSI TCLK DPHY4LN */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL2);
	regval &= ~(CSI_TXCLK_ESC_4LN_EN);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL2);
	/* 10. Disable CSI TCLK DPHY2LN */
	regval = readl(apmu_base + PMUA_ISP_CLKCTRL2);
	regval &= ~(CSI_TXCLK_ESC_2LN_EN);
	writel(regval, apmu_base + PMUA_ISP_CLKCTRL2);
	/* 11. wait for 50ns */
	udelay(1000);
	/* 12. Enable Frequency Change */
	regval = readl(apmu_base + PMUA_ISP_RSTCTRL);
	regval |= FC_EN;
	writel(regval, apmu_base + PMUA_ISP_RSTCTRL);

	udelay(1000);
	/* 13. Enable HWMODE to power down the island */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= ISP_HWMODE_EN;
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 14. Power down the Island */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval &= ~(ISP_PWRUP);
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 15. Wait for Island to be powered down */
	do {
		if (--timeout == 0) {
			WARN(1, "SC2: Island power down timeout!\n");
			goto out;
		}
		udelay(10);
		regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	} while (regval & ISP_PWR_STATUS);
	/* 16. Enable HWMODE to power down the OVT island */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval |= IPE0_HWMODE_EN;
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 17. Power down the OVT Island */
	regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	regval &= ~(IPE0_PWRUP);
	writel(regval, apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	/* 18. Wait for OVT Island to be powered down */
	timeout = 1000;
	do {
		if (--timeout == 0) {
			WARN(1, "SC2: OVT Island power up timeout!\n");
			goto out;
		}
		udelay(10);
		regval = readl(apmu_base + PMUA_ISLD_ISP_PWRCTRL);
	} while (regval & IPE0_PWR_STATUS);
out:
	return 0;
}

static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-sc2",
		.data = (void *)&sc2_data,
	},
	{
		.compatible = "marvell,power-domain-isp",
		.data = (void *)&isp_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_sc2_isp_probe(struct platform_device *pdev)
{
	struct mmp_pd_sc2_isp *pd;
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
	pd->genpd.power_on = mmp_pd_sc2_isp_power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = mmp_pd_sc2_isp_power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_sc2_isp_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_sc2_isp_driver = {
	.probe		= mmp_pd_sc2_isp_probe,
	.remove		= mmp_pd_sc2_isp_remove,
	.driver		= {
		.name	= "mmp-pd-sc2-isp",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_sc2_isp_init(void)
{
	return platform_driver_register(&mmp_pd_sc2_isp_driver);
}
subsys_initcall(mmp_pd_sc2_isp_init);
