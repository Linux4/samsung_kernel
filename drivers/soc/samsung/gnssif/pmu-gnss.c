#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/smc.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/cal-if.h>

#include "../cal-if/acpm_dvfs.h"
#include "pmu-gnss.h"
#include "gnss_prj.h"

/* For connectivity I/F */
//#define SMC_CMD_CONN_IF		(0x82000710)
/* Connectivity sub system */
#define EXYNOS_GNSS		(0)
/* Target to set */
#define EXYNOS_SET_CONN_TZPC	(0)

#define gnss_pmu_read	exynos_pmu_read
#define gnss_pmu_write	exynos_pmu_write
#define gnss_pmu_update exynos_pmu_update

#if defined(CONFIG_SOC_EXYNOS9630)
#define BAAW_GNSS_CMGP_ADDR	(0x13FE0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x13FD0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)

#elif defined(CONFIG_SOC_EXYNOS3830)
#define BAAW_GNSS_CMGP_ADDR	(0x13FE0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x13FD0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)
#endif

static u32 g_shmem_size;
static u32 g_shmem_base;

static void __iomem *baaw_cmgp_reg;
static void __iomem *baaw_dbus_reg;

int gnss_cmgp_read(unsigned int reg_offset, unsigned int *ret)
{
	if (baaw_cmgp_reg == NULL)
		return -EIO;

	*ret = __raw_readl(baaw_cmgp_reg + reg_offset);
	return 0;
}

int gnss_cmgp_write(unsigned int reg_offset, unsigned int val)
{
	unsigned int read_val = 0;

	if (baaw_cmgp_reg == NULL)
		return -EIO;

	__raw_writel(val, baaw_cmgp_reg + reg_offset);
	read_val = __raw_readl(baaw_cmgp_reg + reg_offset);

	if (val != read_val)
		gif_err("ADDR:%08X DATA:%08X => Read to verify:%08X\n", BAAW_GNSS_CMGP_ADDR + reg_offset, val, __raw_readl(baaw_cmgp_reg + reg_offset));

	return 0;
}

int gnss_dbus_read(unsigned int reg_offset, unsigned int *ret)
{
	if (baaw_dbus_reg == NULL)
		return -EIO;

	*ret = __raw_readl(baaw_dbus_reg + reg_offset);
	return 0;
}

int gnss_dbus_write(unsigned int reg_offset, unsigned int val)
{
	unsigned int read_val = 0;

	if (baaw_dbus_reg == NULL)
		return -EIO;

	__raw_writel(val, baaw_dbus_reg + reg_offset);
	read_val = __raw_readl(baaw_dbus_reg + reg_offset);

	if (val != read_val)
		gif_info("ADDR:%08X DATA:%08X => Read to verify:%08X\n", BAAW_GNSS_DBUS_ADDR + reg_offset, val, __raw_readl(baaw_dbus_reg + reg_offset));

	return 0;
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

#if defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830)
static void gnss_get_swreg(struct gnss_swreg *swreg)
{
	exynos_smc_readsfr(0x13E43000, (unsigned long *)&swreg->swreg_0);
	exynos_smc_readsfr(0x13E43004, (unsigned long *)&swreg->swreg_1);
	exynos_smc_readsfr(0x13E43008, (unsigned long *)&swreg->swreg_2);
	exynos_smc_readsfr(0x13E4300C, (unsigned long *)&swreg->swreg_3);
	exynos_smc_readsfr(0x13E43010, (unsigned long *)&swreg->swreg_4);
	exynos_smc_readsfr(0x13E43014, (unsigned long *)&swreg->swreg_5);

	gif_info("SWREG 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
			swreg->swreg_0, swreg->swreg_1, swreg->swreg_2,
			swreg->swreg_3, swreg->swreg_4, swreg->swreg_5);
}

static void gnss_get_apreg(struct gnss_apreg *apreg)
{
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &apreg->CTRL_NS);
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_S, &apreg->CTRL_S);
	gnss_pmu_read(EXYNOS_PMU_GNSS_STAT, &apreg->STAT);
	gnss_pmu_read(EXYNOS_PMU_GNSS_DEBUG, &apreg->DEBUG);
	gif_info("APREG CTRL_NS 0x%08X CTRL_S 0x%08X STAT 0x%08X DEBUG 0x%08X\n",
			apreg->CTRL_NS, apreg->CTRL_S, apreg->STAT, apreg->DEBUG);
}
#endif

#if defined(CONFIG_SOC_EXYNOS9630)
static void __iomem *intr_bid_pend; /* check APM pending before release reset */
static bool check_apm_int_pending()
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
static bool check_apm_int_pending()
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

	ret = exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_GNSS << 31) |
			EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret)
		gif_err("ERR: fail to TZPC setting - %X\n", ret);

	return ret;
}

static void gnss_request_gnss2ap_baaw(void)
{
	gif_info("Config GNSS2AP BAAW\n");

	gif_info("DRAM Configuration\n");
	gnss_dbus_write(0x0, (MEMBASE_GNSS_ADDR >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x4, (MEMBASE_GNSS_ADDR >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x8, (g_shmem_base >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0xC, 0x80000003);

	gnss_dbus_write(0x10, (MEMBASE_GNSS_ADDR_2ND >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x14, (MEMBASE_GNSS_ADDR_2ND >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x18, (g_shmem_base >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x1C, 0x80000003);

#if defined(CONFIG_SOC_EXYNOS3830)
	gif_info("MAILBOX CP APM AP CHUB WLBT\n");
	gnss_cmgp_write(0x00, 0x000B1960);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B19B0);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00011960);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B0E00);
	gnss_cmgp_write(0x14, 0x000B0E40);
	gnss_cmgp_write(0x18, 0x00010E00);
	gnss_cmgp_write(0x1C, 0x80000003);

#elif defined(CONFIG_SOC_EXYNOS9630)
	gif_info("MAILBOX CP APM AP CHUB WLBT\n");
	gnss_cmgp_write(0x00, 0x000B0F60);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B0FB0);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00010F60);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B1A00);
	gnss_cmgp_write(0x14, 0x000B1A68);
	gnss_cmgp_write(0x18, 0x00011A00);
	gnss_cmgp_write(0x1C, 0x80000003);
#endif
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

	baaw_cmgp_reg = devm_ioremap(gc->dev, BAAW_GNSS_CMGP_ADDR, BAAW_GNSS_CMGP_SIZE);
	if (baaw_cmgp_reg == NULL) {
		gif_err("%s: pmu ioremap failed.\n", gc->pdata->name);
		return -EIO;
	} else
		gif_info("baaw_cmgp_reg : 0x%p\n", baaw_cmgp_reg);

	baaw_dbus_reg = devm_ioremap(gc->dev, BAAW_GNSS_DBUS_ADDR, BAAW_GNSS_DBUS_SIZE);
	if (baaw_dbus_reg == NULL) {
		gif_err("%s: pmu ioremap failed.\n", gc->pdata->name);
		return -EIO;
	} else
		gif_info("baaw_dbus_reg : 0x%p\n", baaw_dbus_reg);

	g_shmem_size = gc->pdata->shmem_size;
	g_shmem_base = gc->pdata->shmem_base;

	gif_info("GNSS SHM address:%X size:%X\n", g_shmem_base, g_shmem_size);

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
#if defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830)
	.get_swreg = gnss_get_swreg,
	.get_apreg = gnss_get_apreg,
#endif
};

void gnss_get_pmu_ops(struct gnss_ctl *gc)
{
	gc->pmu_ops = &pmu_ops;
	return;
}
