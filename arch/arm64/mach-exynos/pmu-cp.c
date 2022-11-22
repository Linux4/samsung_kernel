#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/pmu.h>
#include <mach/regs-pmu-exynos7580.h>

/* reset cp */
int exynos_cp_reset()
{
	u32 cp_ctrl;
	int ret = 0;
	enum reset_mode mode = CP_SW_RESET;

	pr_info("%s mode[%d]\n", __func__, mode);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp(CP_RESET);

	/* assert cp_reset */
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL);
	if (mode == CP_SW_RESET)
		cp_ctrl |= CP_RESET_SET;
	else
		cp_ctrl &= ~CP_PWRON;

	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);

	return ret;
}
EXPORT_SYMBOL(exynos_cp_reset);

/* release cp */
int exynos_cp_release(void)
{
	u32 cp_ctrl;

	pr_info("%s\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL);
	cp_ctrl |= CP_START;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);
	pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
		__raw_readl(EXYNOS_PMU_CP_CTRL));

	return 0;
}

/* clear cp active */
int exynos_cp_active_clear(void)
{
	u32 cp_ctrl;

	pr_info("%s\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL);
	cp_ctrl |= (1 << 6);//CP_ACTIVE_REQ_CLR;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);

	pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
		__raw_readl(EXYNOS_PMU_CP_CTRL));

	return 0;
}

/* clear cp_reset_req from cp */
int exynos_clear_cp_reset(void)
{
	u32 cp_ctrl;

	pr_info("%s\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL);
	cp_ctrl |= (1 << 8);//CP_RESET_REQ_CLR;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);

	pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
		__raw_readl(EXYNOS_PMU_CP_CTRL));

	return 0;
}

int exynos_get_cp_power_status(void)
{
	u32 cp_state;
	cp_state = __raw_readl(EXYNOS_PMU_CP_CTRL);
	cp_state &= CP_PWRON;

	if (cp_state)
		return 1;
	else
		return 0;
}

int exynos_set_cp_power_onoff(enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s: mode[%d]\n", __func__, mode);

	/* set power on/off */
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL);

	if (mode == CP_POWER_ON) {
		if (cp_ctrl & CP_PWRON) {
			pr_err("CP is already power ON!!\n");
			return -1;
		}
		cp_ctrl |= (CP_PWRON | CP_START);
		__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);
		pr_err("mif: %s: CP Power Register: 0x%x, Read Register: 0x%x\n", __func__,
			cp_ctrl, __raw_readl(EXYNOS_PMU_CP_CTRL));
	} else {
		cp_ctrl &= ~CP_PWRON;
		__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);
	}

	return ret;
}
EXPORT_SYMBOL(exynos_set_cp_power_onoff);

void exynos_sys_powerdown_conf_cp(enum cp_mode mode)
{
	pr_info("%s: mode[%d]\n", __func__, mode);

	__raw_writel(0, EXYNOS_PMU_CENTRAL_SEQ_CP_CONFIGURATION);
	__raw_writel(0, EXYNOS_PMU_LOGIC_RESET_CP_SYS_PWR_REG);
	__raw_writel(0, EXYNOS_PMU_RESET_ASB_CP_SYS_PWR_REG);
	__raw_writel(0, EXYNOS_PMU_TCXO_GATE_SYS_PWR_REG);
	__raw_writel(0, EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG);
	__raw_writel(0, EXYNOS_PMU_OSCCLK_GATE_CPU_SYS_PWR_REG);
}

int exynos_pmu_cp_init(void)
{
	u32 cp_ctrl;

	pr_info("%s\n", __func__);

	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL);
	cp_ctrl |= MASK_CP_PWRDN_DONE;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL);

#ifdef DEBUG_SELLP_WITHOUT_CP
	exynos_set_cp_power_onoff(CP_POWER_ON);

	exynos_cp_reset();

	exynos_clear_cp_reset();

	exynos_cp_active_clear();

	exynos_set_cp_power_onoff(CP_POWER_OFF);
	exynos_set_cp_power_onoff(CP_POWER_ON);
#endif

	return 0;
}
