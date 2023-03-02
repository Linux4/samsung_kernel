#include "../pmucal_common.h"
#include "../pmucal_cpu.h"
#include "../pmucal_local.h"
#include "../pmucal_rae.h"
#include "../pmucal_system.h"
#include "../pmucal_powermode.h"

#include "flexpmu_cal_cpu_exynos9830.h"
#include "flexpmu_cal_local_exynos9830.h"
#include "flexpmu_cal_p2vmap_exynos9830.h"
#include "flexpmu_cal_system_exynos9830.h"
#include "flexpmu_cal_powermode_exynos9830.h"
#include "flexpmu_cal_define_exynos9830.h"

#ifdef CONFIG_CP_PMUCAL
#include "../pmucal_cp.h"
#include "pmucal_cp_exynos9830.h"
#endif

#include "cmucal-node.c"
#include "cmucal-qch.c"
#include "cmucal-sfr.c"
#include "cmucal-vclk.c"
#include "cmucal-vclklut.c"

#include "clkout_exynos9830.c"
#include "acpm_dvfs_exynos9830.h"
#include "asv_exynos9830.h"
#include "../ra.h"
#include <linux/smc.h>

#include <soc/samsung/exynos-cpupm.h>

#if defined(CONFIG_SEC_DEBUG)
#include <soc/samsung/exynos-pm.h>
#endif

/* defines for EWF WA */
#include <soc/samsung/cmu_ewf.h>
#define EXYNOS9830_CMU_BUS0_BASE	(0x1A300000)
#define QCH_CON_TREX_D0_BUS0_QCH_OFFSET	(0x30f0)
#define IGNORE_FORCE_PM_EN		(2)

/* defines for PLL_MMC SSC settings */
#define EXYNOS9830_CMU_TOP_BASE		(0x1a330000)

#define PLL_CON0_PLL_MMC	(0x140)
#define PLL_CON1_PLL_MMC	(0x144)
#define PLL_CON2_PLL_MMC	(0x148)
#define PLL_CON3_PLL_MMC	(0x14c)
#define PLL_CON4_PLL_MMC	(0x150)
#define PLL_CON5_PLL_MMC	(0x154)

#define PLL_ENABLE_SHIFT	(31)
#define MANUAL_MODE		(0x2)
#define PLL_MMC_MUX_BUSY_SHIFT	(16)
#define MFR_MASK		(0xff)
#define MRR_MASK		(0x3f)
#define MFR_SHIFT		(16)
#define MRR_SHIFT		(24)
#define SSCG_EN			(16)

/* defines for CPUCL2 smpl_warn SW release */
#define EXYNOS9830_CCMU_CPUCL2_BASE		(0x1d200000)
#define CCMU_SMPL_WARN_CFG		(0x9c)
#define SW_RELEASE			(1 << 26)

void __iomem *cmu_top;
void __iomem *cmu_bus0;
void __iomem *ccmu_cpucl2;

void __iomem *dll_apm_base;
void __iomem *sysreg_apm_base;
void __iomem *cmu_apm_base;

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

int cal_dll_apm_enable(void)
{
	u32 timeout = 0, reg = 0;
	int ret = 0;

	if (!dll_apm_base || !sysreg_apm_base || !cmu_apm_base)
		return -EINVAL;

	/* DLL_APM_N_DCO settings */
	__raw_writel(0x726, dll_apm_base + 0x4);

	/* DLL_APM_CTRL0 settings */
	__raw_writel(0x111, sysreg_apm_base + 0x0440);
	while (1) {
		if (__raw_readl(sysreg_apm_base + 0x0444) & 0x1)
			break;
		timeout++;
		usleep_range(10, 11);
		if (timeout > 1000) {
			pr_err("%s, timed out during dll locking\n", __func__);
			return -ETIMEDOUT;
		}
	}

	/* MUX_DLL_USER set to select CLK_DLL_DCO */
	reg = __raw_readl(cmu_apm_base + 0x0640);
	__raw_writel(reg | (0x1 << 4), cmu_apm_base + 0x0640);
	ret = cmu_stable_done(cmu_apm_base + 0x0640, 16, 0, 100);
	if (ret) {
		pr_err("MUX_DLL_USER change time out\n");
		return ret;
	}

	/* MUX_CLKCMU_VTS_BUS set to select MUX_DLL_USER */
	reg = __raw_readl(cmu_apm_base + 0x1004);
	__raw_writel(reg | (0x1 << 0), cmu_apm_base + 0x1004);
	ret = cmu_stable_done(cmu_apm_base + 0x1004, 16, 0, 100);
	if (ret) {
		pr_err("MUX_CLKCMU_SHUB_BUS change time out\n");
		return ret;
	}

	return 0;
}

int cal_dll_apm_disable(void)
{
	u32 reg = 0;
	int ret = 0;

	if (!dll_apm_base || !sysreg_apm_base || !cmu_apm_base)
		return -EINVAL;

	/* MUX_CLKCMU_VTS_BUS set to select OSCCLK_RCO_VTS */
	reg = __raw_readl(cmu_apm_base + 0x1004);
	__raw_writel(reg & ~(0x1 << 0), cmu_apm_base + 0x1004);
	ret = cmu_stable_done(cmu_apm_base + 0x1004, 16, 0, 100);
	if (ret) {
		pr_err("MUX_CLKCMU_SHUB_BUS change time out\n");
		return ret;
	}

	/* MUX_DLL_USER set to select OSCCLK_RCO_APM */
	reg = __raw_readl(cmu_apm_base + 0x0640);
	__raw_writel(reg & ~(0x1 << 4), cmu_apm_base + 0x0640);
	ret = cmu_stable_done(cmu_apm_base + 0x0640, 16, 0, 100);
	if (ret) {
		pr_err("MUX_DLL_USER change time out\n");
		return ret;
	}

	/* DLL_APM off */
	__raw_writel(0x0, sysreg_apm_base + 0x0440);

	return 0;
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

	/* restore PLL mode */
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

void exynos9830_cal_data_init(void)
{
	pr_info("%s: cal data init\n", __func__);

	/* cpu inform sfr initialize */
	pmucal_sys_powermode[SYS_SICD] = CPU_INFORM_SICD;
	pmucal_sys_powermode[SYS_SLEEP] = CPU_INFORM_SLEEP;
	pmucal_sys_powermode[SYS_SLEEP_HSI2ON] = CPU_INFORM_SLEEP_HSI2ON;

	cpu_inform_c2 = CPU_INFORM_C2;
	cpu_inform_cpd = CPU_INFORM_CPD;

	cmu_top = ioremap(EXYNOS9830_CMU_TOP_BASE, SZ_4K);
	if (!cmu_top)
		pr_err("%s: cmu_top ioremap failed\n", __func__);

	cmu_bus0 = ioremap(EXYNOS9830_CMU_BUS0_BASE, SZ_16K);
	if (!cmu_bus0)
		pr_err("%s: cmu_bus0 ioremap failed\n", __func__);

	ccmu_cpucl2 = ioremap(EXYNOS9830_CCMU_CPUCL2_BASE, SZ_4K);
	if (!ccmu_cpucl2)
		pr_err("%s: ccmu_cpucl2 ioremap failed\n", __func__);

	dll_apm_base = ioremap(0x158b0000, SZ_4K);
	if (!dll_apm_base)
		pr_err("%s: dll_apm_base ioremap failed\n", __func__);

	sysreg_apm_base = ioremap(0x15820000, SZ_4K);
	if (!sysreg_apm_base)
		pr_err("%s: sysreg_apm_base ioremap failed\n", __func__);

	cmu_apm_base = ioremap(0x15800000, SZ_8K);
	if (!cmu_apm_base)
		pr_err("%s: cmu_apm_base ioremap failed\n", __func__);
}

void (*cal_data_init)(void) = exynos9830_cal_data_init;

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
	case mg:
		res = asv_get_grp(CPUCL1);
		break;
	case bg:
		res = asv_get_grp(CPUCL2);
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
	case mids:
		res = asv_get_ids_info(CPUCL1);
		break;
	case bids:
		res = asv_get_ids_info(CPUCL2);
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

static void __exynos9830_set_cmuewf(unsigned int index, unsigned int en, void *cmu_cmu)
{
	unsigned int reg;
	unsigned int reg_idx;

	if (index >= 32) {
		reg_idx = EARLY_WAKEUP_FORCED_ENABLE1;
		index = index - 32;
	} else {
		reg_idx = EARLY_WAKEUP_FORCED_ENABLE0;
	}

	if (en) {
		reg = __raw_readl(cmu_cmu + reg_idx);
		reg |= 1 << index;
		__raw_writel(reg, cmu_cmu + reg_idx);
	} else {
		reg = __raw_readl(cmu_cmu + reg_idx);
		reg &= ~(1 << index);
		__raw_writel(reg, cmu_cmu + reg_idx);
	}
}

int exynos9830_set_cmuewf(unsigned int index, unsigned int en, void *cmu_cmu, int *ewf_refcnt)
{
	unsigned int reg;
	int ret = 0;
	int tmp;

	if (en) {
		__exynos9830_set_cmuewf(index, en, cmu_cmu);

		reg = __raw_readl(cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);
		reg |= 1 << IGNORE_FORCE_PM_EN;
		__raw_writel(reg, cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);

		ewf_refcnt[index] += 1;
	} else {

		tmp = ewf_refcnt[index] - 1;

		if (tmp == 0) {
			reg = __raw_readl(cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);
			reg &= ~(1 << IGNORE_FORCE_PM_EN);
			__raw_writel(reg, cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);

			__exynos9830_set_cmuewf(index, en, cmu_cmu);

		} else if (tmp < 0) {
			pr_err("[EWF]%s ref count mismatch. ewf_index:%u\n",__func__,  index);

			ret = -EINVAL;
			goto exit;
		}

		ewf_refcnt[index] -= 1;
	}

exit:
	return ret;
}
int (*wa_set_cmuewf)(unsigned int index, unsigned int en, void *cmu_cmu, int *ewf_refcnt) = exynos9830_set_cmuewf;

int exynos9830_cal_check_hiu_dvfs_id(u32 id)
{
	if ((IS_ENABLED(CONFIG_EXYNOS_PSTATE_HAFM) || IS_ENABLED(CONFIG_EXYNOS_PSTATE_HAFM_TB)) && id == CPUCL2)
		return 1;
	else
		return 0;
}
int (*cal_check_hiu_dvfs_id)(u32 id) = exynos9830_cal_check_hiu_dvfs_id;

void exynos9830_set_cmu_smpl_warn(void)
{
}
void (*cal_set_cmu_smpl_warn)(void) = exynos9830_set_cmu_smpl_warn;

#if defined(CONFIG_SEC_PM_DEBUG) && defined(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>

#define ASV_SUMMARY_SZ	(MFC - MIF + 1)

static int asv_summary_show(struct seq_file *s, void *d)
{
	unsigned int i;
	const char *label[ASV_SUMMARY_SZ] = { "MIF", "INT", "CL0", "CL1", "CL2",
		"NPU", "DISP", "DSP", "AUD", "CP", "G3D", "INTCAM", "CAM",
		"TNR", "DNC", "MFC" };

	seq_printf(s, "Table ver: %d\n", asv_get_table_ver());

	for (i = 0; i < ASV_SUMMARY_SZ ; i++)
		seq_printf(s, "%s: %d\n", label[i], asv_get_grp(MIF + i));

	seq_printf(s, "IDS (b,m,l,g): %d, %d, %d, %d\n",
			asv_get_ids_info(CPUCL2),
			asv_get_ids_info(CPUCL1),
			asv_get_ids_info(CPUCL0),
			asv_get_ids_info(G3D));
	return 0;
}

static int asv_summary_open(struct inode *inode, struct file *file)
{
	return single_open(file, asv_summary_show, inode->i_private);
}

const static struct file_operations asv_summary_fops = {
	.open		= asv_summary_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init cal_data_late_init(void)
{
	debugfs_create_file("asv_summary", 0444, NULL, NULL, &asv_summary_fops);

	return 0;
}

late_initcall(cal_data_late_init);
#endif /* CONFIG_SEC_PM_DEBUG && CONFIG_DEBUG_FS */
