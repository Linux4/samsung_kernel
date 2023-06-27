/*
 * Static Memory Controller
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>

#include <mach/hardware.h>
#include <mach/smemc.h>

DEFINE_MUTEX(smc_lock);

#if defined(CONFIG_PM) && !defined(CONFIG_PXA95x)
static unsigned long msc[2];
static unsigned long sxcnfg, memclkcfg;
static unsigned long csadrcfg[4];

static int pxa3xx_smemc_suspend(void)
{
	mutex_lock(&smc_lock);
	msc[0] = __raw_readl(MSC0);
	msc[1] = __raw_readl(MSC1);
	sxcnfg = __raw_readl(SXCNFG);
	memclkcfg = __raw_readl(MEMCLKCFG);
	csadrcfg[0] = __raw_readl(CSADRCFG0);
	csadrcfg[1] = __raw_readl(CSADRCFG1);
	csadrcfg[2] = __raw_readl(CSADRCFG2);
	csadrcfg[3] = __raw_readl(CSADRCFG3);
	mutex_unlock(&smc_lock);

	return 0;
}

static void pxa3xx_smemc_resume(void)
{
	mutex_lock(&smc_lock);
	__raw_writel(msc[0], MSC0);
	__raw_writel(msc[1], MSC1);
	__raw_writel(sxcnfg, SXCNFG);
	__raw_writel(memclkcfg, MEMCLKCFG);
	__raw_writel(csadrcfg[0], CSADRCFG0);
	__raw_writel(csadrcfg[1], CSADRCFG1);
	__raw_writel(csadrcfg[2], CSADRCFG2);
	__raw_writel(csadrcfg[3], CSADRCFG3);
	mutex_unlock(&smc_lock);
}

static struct syscore_ops smemc_syscore_ops = {
	.suspend	= pxa3xx_smemc_suspend,
	.resume		= pxa3xx_smemc_resume,
};

static int __init smemc_init(void)
{
	if (cpu_is_pxa3xx())
		register_syscore_ops(&smemc_syscore_ops);

	return 0;
}
subsys_initcall(smemc_init);
#endif

#if (defined(CONFIG_MTD_ONENAND) || defined(CONFIG_MTD_ONENAND_MODULE))

void onenand_mmcontrol_smc_cfg(void)
{
	unsigned int csadrcfg2, msc1, sxcnfg;

	mutex_lock(&smc_lock);
	csadrcfg2 = __raw_readl(CSADRCFG2);
	msc1 = __raw_readl(MSC1);
	sxcnfg = __raw_readl(SXCNFG);
	__raw_writel(0x0032091d, CSADRCFG2);
	__raw_writel(((msc1 & 0xffff0000) | 0x7f18), MSC1);
	__raw_writel(((sxcnfg & 0xffff) | 0x30110000), SXCNFG);
	__raw_writel(0x04, CLK_RET_DEL);
	__raw_writel(0x00, ADV_RET_DEL);
	mutex_unlock(&smc_lock);
}
EXPORT_SYMBOL(onenand_mmcontrol_smc_cfg);

void onenand_sync_clk_cfg(void)
{
	u32 temp;
	mutex_lock(&smc_lock);
	temp = __raw_readl(MEMCLKCFG);
	/*
	 * bit18~bit16
	 */
	temp &= (~(0x07<<16));
	/*106Mhz divided by 2*/
	__raw_writel(temp | DFCLK_CTL_EN | DFCLK_CTL_FRE, MEMCLKCFG);
	mutex_unlock(&smc_lock);
}
EXPORT_SYMBOL(onenand_sync_clk_cfg);
#endif
