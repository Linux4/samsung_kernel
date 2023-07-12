#include "../pmucal_common.h"
#include "../pmucal_cpu.h"
#include "../pmucal_local.h"
#include "../pmucal_rae.h"
#include "../pmucal_system.h"
#include "../pmucal_powermode.h"
#include "../pmucal_gnss.h"

#include "flexpmu_cal_cpu_exynos3830.h"
#include "flexpmu_cal_local_exynos3830.h"
#include "flexpmu_cal_p2vmap_exynos3830.h"
#include "flexpmu_cal_system_exynos3830.h"
#include "flexpmu_cal_powermode_exynos3830.h"
#include "flexpmu_cal_define_exynos3830.h"
#include "pmucal_gnss_exynos3830.h"

#ifdef CONFIG_CP_PMUCAL
#include "../pmucal_cp.h"
#include "pmucal_cp_exynos3830.h"
#endif

#ifdef CONFIG_SHUB_PMUCAL
#include "../pmucal_shub.h"
#include "pmucal_chub_exynos3830.h"
#endif

#include "cmucal-node.c"
#include "cmucal-qch.c"
#include "cmucal-sfr.c"
#include "cmucal-vclk.c"
#include "cmucal-vclklut.c"

#include "clkout_exynos3830.c"

#include "acpm_dvfs_exynos3830.h"

#include "asv_exynos3830.h"

#include "../ra.h"

#if defined(CONFIG_SEC_DEBUG)
#include <soc/samsung/exynos-pm.h>
#endif

void __iomem *cmu_top;
void __iomem *cmu_busc;
void __iomem *cmu_cpucl0;
void __iomem *cmu_cpucl1;
void __iomem *cmu_cpucl2;
void __iomem *cmu_g3d;


#define PLL_CON0_PLL_MMC	(0x100)
#define PLL_CON1_PLL_MMC	(0x104)
#define PLL_CON2_PLL_MMC	(0x108)
#define PLL_CON3_PLL_MMC	(0x10c)
#define PLL_CON4_PLL_MMC	(0x110)
#define PLL_CON5_PLL_MMC	(0x114)

#define PLL_ENABLE_SHIFT	(31)
#define MANUAL_MODE		(0x2)
#define PLL_MMC_MUX_BUSY_SHIFT	(16)
#define MFR_MASK		(0xff)
#define MRR_MASK		(0x3f)
#define MFR_SHIFT		(16)
#define MRR_SHIFT		(24)
#define SSCG_EN			(16)

static int cmu_stable_done(void __iomem *cmu,
			unsigned char shift,
			unsigned int done,
			int usec)
{
	unsigned int result;

	do {
		result = get_bit(cmu, shift);

		if (result == done)
			return 0;
		udelay(1);
	} while (--usec > 0);

	return -EVCLKTIMEOUT;
}

int pll_mmc_enable(int enable)
{
	unsigned int reg;
	unsigned int cmu_mode;
	int ret;

	if (!cmu_top) {
		pr_err("%s: cmu_top cmuioremap failed\n", __func__);
		return -1;
	}

	/* set PLL to manual mode */
	cmu_mode = readl(cmu_top + PLL_CON1_PLL_MMC);
	writel(MANUAL_MODE, cmu_top + PLL_CON1_PLL_MMC);

	if (!enable) {
		/* select oscclk */
		reg = readl(cmu_top + PLL_CON0_PLL_MMC);
		reg &= ~(PLL_MUX_SEL);
		writel(reg, cmu_top + PLL_CON0_PLL_MMC);
		ret = cmu_stable_done(cmu_top + PLL_CON0_PLL_MMC, PLL_MMC_MUX_BUSY_SHIFT, 0, 100);
		if (ret)
			pr_err("pll mux change time out, \'PLL_MMC\'\n");
	}

	/* setting ENABLE of PLL */
	reg = readl(cmu_top + PLL_CON3_PLL_MMC);
	if (enable)
		reg |= 1 << PLL_ENABLE_SHIFT;
	else
		reg &= ~(1 << PLL_ENABLE_SHIFT);

	writel(reg, cmu_top + PLL_CON3_PLL_MMC);

	if (enable) {
		/* wait for PLL stable */
		ret = cmu_stable_done(cmu_top + PLL_CON3_PLL_MMC, PLL_STABLE_SHIFT, 1, 100);
		if (ret)
			pr_err("pll time out, \'PLL_MMC\' %d\n", enable);

		/* select FOUT_PLL_MMC */
		reg = readl(cmu_top + PLL_CON0_PLL_MMC);
		reg |= PLL_MUX_SEL;
		writel(reg, cmu_top + PLL_CON0_PLL_MMC);

		ret = cmu_stable_done(cmu_top + PLL_CON0_PLL_MMC, PLL_MMC_MUX_BUSY_SHIFT, 0, 100);
		if (ret)
			pr_err("pll mux change time out, \'PLL_MMC\'\n");
	}

	writel(cmu_mode, cmu_top + PLL_CON1_PLL_MMC);

	return ret;
}

int cal_pll_mmc_check(void)
{
       unsigned int reg;
       bool ret = false;

       reg = readl(cmu_top + PLL_CON4_PLL_MMC);

       if (reg & (1 << SSCG_EN))
               ret = true;

       return ret;
}

int cal_pll_mmc_set_ssc(unsigned int mfr, unsigned int mrr, unsigned int ssc_on)
{
	unsigned int reg;
	int ret = 0;

	/* disable PLL_MMC */
	ret = pll_mmc_enable(0);
	if (ret) {
		pr_err("%s: pll_mmc_disable failed\n", __func__);
		return ret;
	}

	/* setting MFR, MRR */
	reg = readl(cmu_top + PLL_CON5_PLL_MMC);
	reg &= ~((MFR_MASK << MFR_SHIFT) | (MRR_MASK << MRR_SHIFT));

	if (ssc_on)
		reg |= ((mfr & MFR_MASK) << MFR_SHIFT) | ((mrr & MRR_MASK) << MRR_SHIFT);
	writel(reg, cmu_top + PLL_CON5_PLL_MMC);

	/* setting SSCG_EN */
	reg = readl(cmu_top + PLL_CON4_PLL_MMC);

	if (ssc_on)
		reg |= 1 << SSCG_EN;
	else
		reg &= ~(1 << SSCG_EN);

	writel(reg, cmu_top + PLL_CON4_PLL_MMC);

	/* enable PLL_MMC */
	ret = pll_mmc_enable(1);
	if (ret)
		pr_err("%s: pll_mmc_enable failed\n", __func__);

	return ret;
}

#define EXYNOS3830_CMU_TOP_BASE		(0x120E0000)
#define EXYNOS3830_CMU_MMC_BASE		(0x13C00000)
#define EXYNOS3830_CMU_BUSC_BASE	(0x1A200000)

#define EXYNOS3830_CMU_CPUCL0_BASE	(0x1D020000)
#define EXYNOS3830_CMU_CPUCL1_BASE	(0x1D030000)
#define EXYNOS3830_CMU_CPUCL2_BASE	(0x1D120000)
#define EXYNOS3830_CMU_G3D_BASE		(0x18400000)

#define EXYNOS3830_CPUCL0_CLKDIV2PH		(0x0840)
#define EXYNOS3830_CPUCL1_CLKDIV2PH		(0x0830)
#define EXYNOS3830_CPUCL2_CTRL_EXT_EVENT	(0x0838)
#define EXYNOS3830_G3D_CLKDIV2PH		(0x0824)

#define MASK_SMPL_INT				(7)
#define ENABLE					(0)

#define SET_1BIT(reg, bit, value)		((reg & ~(1 << bit)) | ((value & 0x1) << bit))

#define QCH_CON_TREX_D0_BUSC_QCH_OFFSET	(0x31a8)
#define IGNORE_FORCE_PM_EN		(2)

void exynos3830_cal_data_init(void)
{
	pr_info("%s: cal data init\n", __func__);

	/* cpu inform sfr initialize */
	pmucal_sys_powermode[SYS_SICD] = CPU_INFORM_SICD;
	pmucal_sys_powermode[SYS_SLEEP] = CPU_INFORM_SLEEP;

	cpu_inform_c2 = CPU_INFORM_C2;
	cpu_inform_cpd = CPU_INFORM_CPD;

	cmu_top = ioremap(EXYNOS3830_CMU_TOP_BASE, SZ_4K);
	if (!cmu_top)
		pr_err("%s: cmu_top ioremap failed\n", __func__);

	/* will be added
	cmu_busc = ioremap(EXYNOS3830_CMU_BUSC_BASE, SZ_16K);
	if (!cmu_busc)
		pr_err("%s: cmu_busc ioremap failed\n", __func__);

	cmu_cpucl0 = ioremap(EXYNOS3830_CMU_CPUCL0_BASE, SZ_4K);
	if (!cmu_cpucl0)
		pr_err("%s: cmu_cpucl0 ioremap failed\n", __func__);

	cmu_cpucl1 = ioremap(EXYNOS3830_CMU_CPUCL1_BASE, SZ_4K);
	if (!cmu_cpucl1)
		pr_err("%s: cmu_cpucl1 ioremap failed\n", __func__);

	cmu_cpucl2 = ioremap(EXYNOS3830_CMU_CPUCL2_BASE, SZ_4K);
	if (!cmu_cpucl2)
		pr_err("%s: cmu_cpucl2 ioremap failed\n", __func__);

	cmu_g3d = ioremap(EXYNOS3830_CMU_G3D_BASE, SZ_4K);
	if (!cmu_g3d)
		pr_err("%s: cmu_g3d ioremap failed\n", __func__);
	*/
}

void (*cal_data_init)(void) = exynos3830_cal_data_init;

#if defined(CONFIG_SEC_DEBUG)
int asv_ids_information(enum ids_info id)
{
	int res;

	switch (id) {
	case tg:
		res = asv_get_table_ver();
		break;
	case lg:
		res = asv_get_grp(CPUCL0);
		break;
	case bg:
		res = asv_get_grp(CPUCL1);
		break;
	case g3dg:
		res = asv_get_grp(G3D);
		break;
	case mifg:
		res = asv_get_grp(MIF);
		break;
	case lids:
		res = asv_get_ids_info(CPUCL0);
		break;
	case bids:
		res = asv_get_ids_info(CPUCL1);
		break;
	case gids:
		res = asv_get_ids_info(G3D);
		break;
	default:
		res = 0;
		break;
	};
	return res;
}
#endif
