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

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define ARM_SMMU_GR0_sACR		0x10
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)
#define sCR0_BSU_SHIFT			14
#define sCR0_BSU_MASK			0x3
#define sCR0_PREFETCHEN			(1 << 0)
#define sCR0_WC1EN			(1 << 1)
#define sCR0_WC2EN			(1 << 2)

#define ARM_SMMU_GR0_sGFSR		0x48

/* Global TLB invalidation */
#define ARM_SMMU_GR0_STLBIALL		0x60
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		(1 << 0)
#define TLB_LOOP_TIMEOUT		1000000	/* 1s! */

#define ARM_SMMU_NSCR0			0x400

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_TYPE_SHIFT			16
#define S2CR_TYPE_BYPASS		(1 << S2CR_TYPE_SHIFT)

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))

#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0_LO		0x20
#define ARM_SMMU_CB_TTBR0_HI		0x24
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_FSR			0x58

/* Translation context bank */
#define ARM_SMMU_CB_BASE(pd)		((pd)->reg_base + ((pd)->size >> 1))
#define ARM_SMMU_CB(data, n)		((n) * (data)->pagesize)

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)
#define FSR_IGN				(FSR_AFF | FSR_ASF | FSR_TLBMCF |	\
					 FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT |		\
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)
#define MAX_TIMEOUT	5000

#define MMP_PD_POWER_ON_LATENCY		300
#define MMP_PD_POWER_OFF_LATENCY	20

struct mmp_pd_smmu_data {
	int id;
	char *name;
	u32 num_mapping_groups;
	u32 num_context_banks;
	u32 pagesize;
};

struct mmp_pd_smmu_regs {
	u32 gr1_cbar;
	u32 gr1_cba2r;
	u32 cb_ttbcr2;
	u32 cb_ttbr0_lo;
	u32 cb_ttbr0_hi;
	u32 cb_ttbcr;
	u32 cb_s1_mair0;
	u32 cb_sctlr;
	u32 gr0_smr0;
	u32 gr0_s2cr0;
};

struct mmp_pd_smmu {
	struct generic_pm_domain genpd;
	void __iomem *reg_base;
	unsigned long size;
	struct clk *aclk;
	struct clk *fclk;
	struct device *dev;
	/* latency for us. */
	u32 power_on_latency;
	u32 power_off_latency;
	int regs_saved;
	struct mmp_pd_smmu_regs regs;
	const struct mmp_pd_smmu_data *data;
};

enum {
	MMP_PD_SMMU_PXA1U88,
	MMP_PD_SMMU_PXA1928,
};

extern u32 use_iommu;

static int arm_smmu_clk_enable(struct mmp_pd_smmu *pd)
{
	if (pd->aclk)
		clk_prepare_enable(pd->aclk);

	if (pd->fclk)
		clk_prepare_enable(pd->fclk);

	return 0;
}

static int arm_smmu_clk_disable(struct mmp_pd_smmu *pd)
{
	if (pd->aclk)
		clk_disable_unprepare(pd->aclk);

	if (pd->fclk)
		clk_disable_unprepare(pd->fclk);

	return 0;
}

/* Wait for any pending TLB invalidations to complete */
static void arm_smmu_tlb_sync(struct mmp_pd_smmu *pd)
{
	int count = 0;
	void __iomem *gr0_base = pd->reg_base;

	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_sTLBGSYNC);
	while (readl_relaxed(gr0_base + ARM_SMMU_GR0_sTLBGSTATUS)
	       & sTLBGSTATUS_GSACTIVE) {
		cpu_relax();
		if (++count == TLB_LOOP_TIMEOUT) {
			dev_err_ratelimited(pd->dev,
			"TLB sync timed out -- SMMU may be deadlocked\n");
			return;
		}
		udelay(1);
	}
}

static void arm_smmu_device_reset(struct mmp_pd_smmu *pd)
{
	void __iomem *gr0_base = pd->reg_base;
	const struct mmp_pd_smmu_data *data = pd->data;
	void __iomem *cb_base;
	int i = 0;
	u32 reg;

	/* Clear Global FSR */
	reg = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSR);
	writel(reg, gr0_base + ARM_SMMU_GR0_sGFSR);

	/* Mark all SMRn as invalid and all S2CRn as bypass */
	for (i = 0; i < data->num_mapping_groups; ++i) {
		writel_relaxed(~SMR_VALID, gr0_base + ARM_SMMU_GR0_SMR(i));
		writel_relaxed(S2CR_TYPE_BYPASS, gr0_base + ARM_SMMU_GR0_S2CR(i));
	}

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < data->num_context_banks; ++i) {
		cb_base = ARM_SMMU_CB_BASE(pd) + ARM_SMMU_CB(data, i);
		writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
		writel_relaxed(FSR_FAULT, cb_base + ARM_SMMU_CB_FSR);
	}

	/* Invalidate the TLB, just in case */
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_STLBIALL);
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLH);
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLNSNH);

	reg = readl_relaxed(gr0_base + ARM_SMMU_GR0_sCR0);

	/* Enable fault reporting */
	reg |= (sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE);

	/* Disable TLB broadcasting. */
	reg |= (sCR0_VMIDPNE | sCR0_PTM);

	/* Enable client access, but bypass when no mapping is found */
	reg &= ~(sCR0_CLIENTPD | sCR0_USFCFG);

	/* Disable forced broadcasting */
	reg &= ~sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(sCR0_BSU_MASK << sCR0_BSU_SHIFT);

	/* Push the button */
	arm_smmu_tlb_sync(pd);
	writel_relaxed(reg, gr0_base + ARM_SMMU_GR0_sCR0);

	/*
	 * Non-secure configuration the same as secure configuration,
	 * as device maybe use non-secure transaction like coda7542.
	 */
	writel_relaxed(reg, gr0_base + ARM_SMMU_NSCR0);

	reg = readl_relaxed(gr0_base + ARM_SMMU_GR0_sACR);

	/* Enable pre-fetch, walk cache 1, walk cache 2 */
	reg |= (sCR0_PREFETCHEN | sCR0_WC1EN | sCR0_WC2EN);
	writel(reg, gr0_base + ARM_SMMU_GR0_sACR);
}

static int mmp_pd_smmu_power_on(struct generic_pm_domain *domain)
{
	struct mmp_pd_smmu *pd = container_of(domain,
					struct mmp_pd_smmu, genpd);
	const struct mmp_pd_smmu_data *data = pd->data;
	struct mmp_pd_smmu_regs *regs = &pd->regs;
	void __iomem *gr0_base = pd->reg_base, *gr1_base, *cb_base;
	u32 idx = 0, cbndx = 0;

	dev_dbg(pd->dev, "%s: enter\n", __func__);

	if (!use_iommu)
		return 0;

	if (!pd->regs_saved) {
		dev_dbg(pd->dev, "skip as the registers were not saved!\n");
		return 0;
	}

	arm_smmu_clk_enable(pd);

	arm_smmu_device_reset(pd);
	gr1_base = gr0_base + data->pagesize;
	/* FIX ME: just use context bank index 0 */
	cb_base = ARM_SMMU_CB_BASE(pd) + ARM_SMMU_CB(data, idx);

	writel_relaxed(regs->gr1_cbar, gr1_base + ARM_SMMU_GR1_CBAR(cbndx));
	writel_relaxed(regs->gr1_cba2r, gr1_base + ARM_SMMU_GR1_CBA2R(cbndx));
	writel_relaxed(regs->cb_ttbcr2, cb_base + ARM_SMMU_CB_TTBCR2);
	writel_relaxed(regs->cb_ttbr0_lo, cb_base + ARM_SMMU_CB_TTBR0_LO);
	writel_relaxed(regs->cb_ttbr0_hi, cb_base + ARM_SMMU_CB_TTBR0_HI);
	writel_relaxed(regs->cb_ttbcr, cb_base + ARM_SMMU_CB_TTBCR);
	writel_relaxed(regs->cb_s1_mair0, cb_base + ARM_SMMU_CB_S1_MAIR0);
	writel_relaxed(regs->cb_sctlr, cb_base + ARM_SMMU_CB_SCTLR);
	writel_relaxed(regs->gr0_smr0, gr0_base + ARM_SMMU_GR0_SMR(idx));
	writel_relaxed(regs->gr0_s2cr0, gr0_base + ARM_SMMU_GR0_S2CR(idx));

	arm_smmu_clk_disable(pd);

	return 0;
}

static int mmp_pd_smmu_power_off(struct generic_pm_domain *domain)
{
	struct mmp_pd_smmu *pd = container_of(domain,
					struct mmp_pd_smmu, genpd);
	const struct mmp_pd_smmu_data *data = pd->data;
	struct mmp_pd_smmu_regs *regs = &pd->regs;
	void __iomem *gr0_base = pd->reg_base, *gr1_base, *cb_base;
	u32 idx = 0, cbndx = 0;

	dev_dbg(pd->dev, "%s: enter\n", __func__);

	if (!use_iommu)
		return 0;
	/* currently the register value only need to be saved once */
	if (pd->regs_saved)
		return 0;

	arm_smmu_clk_enable(pd);

	gr1_base = gr0_base + data->pagesize;
	/* FIX ME: just use context bank index 0 */
	cb_base = ARM_SMMU_CB_BASE(pd) + ARM_SMMU_CB(data, idx);

	regs->gr1_cbar = readl_relaxed(gr1_base + ARM_SMMU_GR1_CBAR(cbndx));
	regs->gr1_cba2r = readl_relaxed(gr1_base + ARM_SMMU_GR1_CBA2R(cbndx));
	regs->cb_ttbcr2 = readl_relaxed(cb_base + ARM_SMMU_CB_TTBCR2);
	regs->cb_ttbr0_lo = readl_relaxed(cb_base + ARM_SMMU_CB_TTBR0_LO);
	regs->cb_ttbr0_hi = readl_relaxed(cb_base + ARM_SMMU_CB_TTBR0_HI);
	regs->cb_ttbcr = readl_relaxed(cb_base + ARM_SMMU_CB_TTBCR);
	regs->cb_s1_mair0 = readl_relaxed(cb_base + ARM_SMMU_CB_S1_MAIR0);
	regs->cb_sctlr = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
	regs->gr0_smr0 = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(idx));
	regs->gr0_s2cr0 = readl_relaxed(gr0_base + ARM_SMMU_GR0_S2CR(idx));
	pd->regs_saved = 1;

	arm_smmu_clk_disable(pd);

	return 0;
}

/* pxa1U88 */
static struct mmp_pd_smmu_data smmu_pxa1u88_data = {
	.id			= MMP_PD_SMMU_PXA1U88,
	.name			= "power-domain-smmu", /* generic power domain name */
	.num_mapping_groups	= 4,
	.num_context_banks	= 1,
	.pagesize		= SZ_4K,
};

static struct mmp_pd_smmu_data smmu_pxa1928_data = {
	.id			= MMP_PD_SMMU_PXA1928,
	.name			= "power-domain-smmu", /* generic power domain name */
	.num_mapping_groups	= 32,
	.num_context_banks	= 8,
	.pagesize		= SZ_4K,
};

static const struct of_device_id of_mmp_pd_match[] = {
	{
		.compatible = "marvell,power-domain-smmu-pxa1u88",
		.data = (void *)&smmu_pxa1u88_data,
	},
	{
		.compatible = "marvell,power-domain-smmu-pxa1928",
		.data = (void *)&smmu_pxa1928_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mmp_pd_match);

static int mmp_pd_smmu_probe(struct platform_device *pdev)
{
	struct mmp_pd_smmu *pd;
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
	pd->size = resource_size(res);

	/* Some power domain may need clk for power on. */
	pd->aclk = devm_clk_get(&pdev->dev, "VPUACLK");
	if (IS_ERR(pd->aclk)) {
		dev_err(&pdev->dev, "cannot get pd_smmu axi clock\n");
		pd->aclk = NULL;
	}
	pd->fclk = devm_clk_get(&pdev->dev, "VPUCLK");
	if (IS_ERR(pd->fclk)) {
		dev_err(&pdev->dev, "cannot get pd_smmu function clock\n");
		pd->fclk = NULL;
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
	pd->genpd.power_on = mmp_pd_smmu_power_on;
	pd->genpd.power_on_latency_ns = pd->power_on_latency * 1000;
	pd->genpd.power_off = mmp_pd_smmu_power_off;
	pd->genpd.power_off_latency_ns = pd->power_off_latency * 1000;

	ret = mmp_pd_init(&pd->genpd, NULL, true);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pd);

	return 0;
}

static int mmp_pd_smmu_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mmp_pd_smmu_driver = {
	.probe		= mmp_pd_smmu_probe,
	.remove		= mmp_pd_smmu_remove,
	.driver		= {
		.name	= "mmp-pd-smmu",
		.owner	= THIS_MODULE,
		.of_match_table = of_mmp_pd_match,
	},
};

static int __init mmp_pd_smmu_init(void)
{
	return platform_driver_register(&mmp_pd_smmu_driver);
}
subsys_initcall(mmp_pd_smmu_init);
