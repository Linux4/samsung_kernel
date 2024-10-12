// SPDX-License-Identifier: GPL-2.0
#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/cal-if.h>
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#else
#include <soc/samsung/exynos-pmu.h>
#endif

#include "pmu-gnss.h"
#include "gnss_prj.h"

/* Connectivity sub system */
#define EXYNOS_GNSS		(0)
/* Target to set */
#define EXYNOS_SET_CONN_TZPC	(0)

#define gnss_pmu_read	exynos_pmu_read
#define gnss_pmu_write	exynos_pmu_write
#define gnss_pmu_update exynos_pmu_update

#if IS_ENABLED(CONFIG_SOC_S5E8825)
#define SYSREG_ALIVE_ADDR	(0x11820000)
#define LPP_RA1_HD_ADME_OFFSET	(0x00000314)

static void __iomem *sysreg_alive_reg;
#endif

int gnss_baaw_write(struct gnss_baaw *window)
{
	u32 gnss_start = 0;
	u32 gnss_end = 0;
	u32 ap_start = 0;
	u32 authority = 0;

	if (window == NULL)
		return -EIO;

	gif_info("BAAW configuration at sfr: 0x%X\n", window->sfr_addr);

	__raw_writel(window->gnss_start, window->baaw_reg + 0x0);
	gnss_start = __raw_readl(window->baaw_reg + 0x0);
	if (gnss_start != window->gnss_start) {
		gif_err("gnss_start:0x%08X => Read to verify:0x%08X\n",
			window->gnss_start, gnss_start);
		goto fail;
	}
	__raw_writel(window->gnss_end, window->baaw_reg + 0x4);
	gnss_end = __raw_readl(window->baaw_reg + 0x4);
	if (gnss_end != window->gnss_end) {
		gif_err("gnss_end:0x%08X => Read to verify:0x%08X\n",
			window->gnss_end, gnss_end);
		goto fail;
	}
	__raw_writel(window->ap_start, window->baaw_reg + 0x8);
	ap_start = __raw_readl(window->baaw_reg + 0x8);
	if (ap_start != window->ap_start) {
		gif_err("ap_start:0x%08X => Read to verify:0x%08X\n",
			window->ap_start, ap_start);
		goto fail;
	}
	__raw_writel(window->authority, window->baaw_reg + 0xC);
	authority = __raw_readl(window->baaw_reg + 0xC);
	if (authority != window->authority) {
		gif_err("authority:0x%08X => Read to verify:0x%08X\n",
			window->authority, authority);
		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static int gnss_pmu_clear_interrupt(enum gnss_int_clear gnss_int)
{
	switch (gnss_int) {
	case GNSS_INT_WAKEUP_CLEAR:
		break;
	case GNSS_INT_ACTIVE_CLEAR:
		cal_gnss_active_clear();
		break;
	case GNSS_INT_WDT_RESET_CLEAR:
		break;
	default:
		gif_err("Unexpected interrupt value!\n");
		return -EIO;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SOC_EXYNOS9630) || IS_ENABLED(CONFIG_SOC_EXYNOS3830)
static void gnss_get_swreg(struct gnss_swreg *swreg)
{
	exynos_smc_readsfr(CMUPMU_ADDR, (unsigned long *)&swreg->swreg_0);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x4, (unsigned long *)&swreg->swreg_1);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x8, (unsigned long *)&swreg->swreg_2);
	exynos_smc_readsfr(CMUPMU_ADDR + 0xC, (unsigned long *)&swreg->swreg_3);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x10, (unsigned long *)&swreg->swreg_4);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x14, (unsigned long *)&swreg->swreg_5);

	gif_info("SWREG 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
			swreg->swreg_0, swreg->swreg_1, swreg->swreg_2,
			swreg->swreg_3, swreg->swreg_4, swreg->swreg_5);
}
#endif

static void gnss_get_apreg(struct gnss_apreg *apreg)
{
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &apreg->CTRL_NS);
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_S, &apreg->CTRL_S);
	gnss_pmu_read(EXYNOS_PMU_GNSS_STAT, &apreg->STAT);
	gnss_pmu_read(EXYNOS_PMU_GNSS_DEBUG, &apreg->DEBUG);
	gif_info("APREG CTRL_NS 0x%08X CTRL_S 0x%08X STAT 0x%08X DEBUG 0x%08X\n",
			apreg->CTRL_NS, apreg->CTRL_S, apreg->STAT, apreg->DEBUG);
}

#if IS_ENABLED(CONFIG_SOC_EXYNOS9630)
static void __iomem *intr_bid_pend; /* check APM pending before release reset */
static bool check_apm_int_pending(void)
{
	bool ret = false;
	int reg_val = 0;
	int count = 20; /* 50ms * 20 times = 1 sec */
	if (intr_bid_pend == NULL) {
		intr_bid_pend = ioremap(0x10E71A04, SZ_4);
		if (intr_bid_pend == NULL) {
			gif_err("Err: failed to ioremap GRP26_INTR_BID_PEND!\n");
			return ret;
		}
	}
	while (count > 0) {
		reg_val = __raw_readl(intr_bid_pend);
		gif_info("APM PENDING CHECK REGISTER VAL: 0x%08x\n", reg_val);
		if ((reg_val >> 17) & 0x3F) {/* check if one or more of bits [22:17] are 1 */
			count--;
			msleep(50);
			continue;
		} else {
			ret = true;
			break;
		}
	}
	return ret;
}
#else
static bool check_apm_int_pending(void)
{
	return true;
}
#endif

static int gnss_pmu_release_reset(void)
{
	int ret = 0;

	if (check_apm_int_pending())
		cal_gnss_reset_release();
	else
		ret = -1;

	return ret;
}

static int gnss_pmu_hold_reset(void)
{
	int ret = 0;

	if (check_apm_int_pending()) {
		cal_gnss_reset_assert();
		msleep(50);
	} else
		ret = 1;

	return ret;
}

static int gnss_request_tzpc(void)
{
	int ret;

	ret = (int)exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_GNSS << 31) |
			EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret)
		gif_err("ERR: fail to TZPC setting - %d\n", ret);

	return ret;
}

static int gnss_request_gnss2ap_baaw(struct gnss_ctl *gc)
{
	struct gnss_pdata *pdata = gc->pdata;
#if IS_ENABLED(CONFIG_SOC_S5E8825)
	int lpp_adme_val = 0;
#endif
	int ret = 0;
	int i = 0;

	gif_info("Config GNSS2AP BAAW\n");

	gif_info("DRAM Configuration\n");

	for (i = 0; i < pdata->num_wnd; i++) {
		ret = gnss_baaw_write(&pdata->wnd_set[i]);
		if (ret)
			goto end;
	}

#if IS_ENABLED(CONFIG_SOC_S5E8825)
	lpp_adme_val = __raw_readl(sysreg_alive_reg + LPP_RA1_HD_ADME_OFFSET);
	gif_info("lpp adme val before set: 0x%08x\n", lpp_adme_val);

	lpp_adme_val = (0x7 << 20) | (lpp_adme_val & ~(0x7 << 20));
	gif_info("lpp adme val after set: 0x%08x\n", lpp_adme_val);

	__raw_writel(lpp_adme_val, sysreg_alive_reg + LPP_RA1_HD_ADME_OFFSET);
#endif

end:
	return ret;
}

static int gnss_pmu_power_on(enum gnss_mode mode)
{
	int ret = 0;

	gif_info("mode[%d]\n", mode);

	if (mode == GNSS_POWER_ON) {
		if (cal_gnss_status() > 0) {
			if (check_apm_int_pending()) {
				gif_info("GNSS is already Power on, try reset\n");
				cal_gnss_reset_assert();
			} else
				ret = -1;
		} else {
			gif_info("GNSS Power On\n");
			cal_gnss_init();
		}
	} else {
		if (check_apm_int_pending())
			cal_gnss_reset_release();
		else
			ret = -1;
	}

	return ret;

}

static int gnss_pmu_init_conf(struct gnss_ctl *gc)
{
	u32 __maybe_unused shmem_size, shmem_base;

#if IS_ENABLED(CONFIG_SOC_S5E8825)
	sysreg_alive_reg = devm_ioremap(gc->dev, SYSREG_ALIVE_ADDR, SZ_64K);
	if (!sysreg_alive_reg) {
		gif_err("sysreg alive ioremap failed.");
		return -EIO;
	}
#endif

	return 0;
}

static struct gnssctl_pmu_ops pmu_ops = {
	.init_conf = gnss_pmu_init_conf,
	.hold_reset = gnss_pmu_hold_reset,
	.release_reset = gnss_pmu_release_reset,
	.power_on = gnss_pmu_power_on,
	.clear_int = gnss_pmu_clear_interrupt,
	.req_security = gnss_request_tzpc,
	.req_baaw = gnss_request_gnss2ap_baaw,
#if IS_ENABLED(CONFIG_SOC_EXYNOS9630) || IS_ENABLED(CONFIG_SOC_EXYNOS3830)
	.get_swreg = gnss_get_swreg,
#endif
	.get_apreg = gnss_get_apreg,
};

void gnss_get_pmu_ops(struct gnss_ctl *gc)
{
	gc->pmu_ops = &pmu_ops;
}
