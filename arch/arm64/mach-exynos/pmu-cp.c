#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/smc.h>

#include <mach/pmu.h>
#include <mach/regs-pmu-exynos7580.h>


#define PR_REG(name, reg) pr_info("%s: 0x%08x\n", name, __raw_readl(reg));

/**
*	dump_reg
*/
void exynos_cp_dump_pmu_reg(void)
{
	u32 cp_ctrl;

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, 0);

#if 0 /* To do: will be removed */
	struct pmu_cp_regs *regs = pcdata.regs;
	u32 win = 0;

	win = 0x4c444444;
	__raw_writel(win, &regs->mif_access_win0);
	win = 0x4cc44444;
	__raw_writel(win, &regs->mif_access_win1);
	win = 0x44034444;
	__raw_writel(win, &regs->mif_access_win2);
#endif

	pr_info("---------- PMU CP RegDump ----------\n");
	pr_info("%s: 0x%08x\n", "CP_CTRL", cp_ctrl);
	PR_REG("CP_STAT", EXYNOS_PMU_CP_START);
	PR_REG("CP_DEBUG", EXYNOS_PMU_CP_DEBUG);
	PR_REG("CP_SEMAPHORE", EXYNOS_PMU_CP_SEMAPHORE);
	PR_REG("AUD_PATH_CFG", EXYNOS_PMU_AUD_PATH_CFG);

#ifdef CP_NONSECURE_BOOT
	PR_REG("CP2AP_MEM_CONFIG", EXYNOS_PMU_CP2AP_MEM_CONFIG);
	PR_REG("CP2AP_MIF_ACCESS_WIN0", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN0);
	PR_REG("CP2AP_MIF_ACCESS_WIN1", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN1);
	PR_REG("CP2AP_MIF_ACCESS_WIN2", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2);
	PR_REG("CP2AP_PERI_ACCESS_WIN", EXYNOS_PMU_CP2AP_PERI_ACCESS_WIN);
	PR_REG("MIF_TZPC_CFG", EXYNOS_PMU_MIF_TZPC_CONFIG);
	PR_REG("CP_BOOT_TEST_RST_CONFIG", EXYNOS_PMU_CP_BOOT_TEST_RST_CONFIG);
	PR_REG("CP_MIF_CLKCTRL", EXYNOS_PMU_CP_MIF_CLKCTRL);
#endif
}
EXPORT_SYMBOL(exynos_cp_dump_pmu_reg);


/* reset cp */
int exynos_cp_reset()
{
	int ret = 0;
	enum reset_mode mode = CP_SW_RESET;

	pr_info("%s mode[%d]\n", __func__, mode);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp(CP_RESET);

	/* assert cp_reset */
	ret = exynos_smc(SMC_ID, WRITE_CTRL, CP_RESET_SET, 0);
	if (ret > 0) {
		pr_err("ERR! CP Reset Fail: %d\n", ret);
		ret = -1;
		goto exit;
	}

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);

exit:
	return ret;
}
EXPORT_SYMBOL(exynos_cp_reset);

/* release cp */
int exynos_cp_release(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, 0);

	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	}
	else {
		pr_err("ERR! read Fail: %d\n", cp_ctrl & 0xffff);
		ret = -1;
		goto exit;
	}

	ret = exynos_smc(SMC_ID, WRITE_CTRL, CP_START, 0);

	if (ret > 0)
		pr_err("ERR! CP Release Fail: %d\n", ret);
	else
		pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc(SMC_ID, READ_CTRL, 0, 0) >> 16);

exit:
	return ret;
}

/* clear cp active */
int exynos_cp_active_clear(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, 0);

	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	}
	else {
		pr_err("ERR! read Fail: %d\n", cp_ctrl & 0xffff);
		ret = -1;
		goto exit;
	}

	ret = exynos_smc(SMC_ID, WRITE_CTRL, CP_ACTIVE_REQ_CLR, 0);

	if (ret > 0)
		pr_err("ERR! CP active_clear Fail: %d\n", ret);
	else
		pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc(SMC_ID, READ_CTRL, 0, 0) >> 16);

exit:
	return ret;
}

/* clear cp_reset_req from cp */
int exynos_clear_cp_reset(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, 0);

	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	}
	else {
		pr_err("ERR! read Fail: %d\n", cp_ctrl & 0xffff);
		ret = -1;
		goto exit;
	}

	ret = exynos_smc(SMC_ID, WRITE_CTRL, CP_RESET_REQ_CLR, 0);

	if (ret > 0)
		pr_err("ERR! CP clear_cp_reset Fail: %d\n", ret);
	else
		pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc(SMC_ID, READ_CTRL, 0, 0) >> 16);

exit:
	return ret;
}

int exynos_get_cp_power_status(void)
{
	u32 cp_state;

	cp_state = exynos_smc(SMC_ID, READ_CTRL, 0, 0);

	pr_err("CP Power status: %x\n", cp_state);

	if (!(cp_state & 0xffff)) {
		cp_state >>= 16;
	}
	else {
		pr_err("ERR! Get CP Power status: %d\n", cp_state & 0xffff);
		return -1;
	}

	if (cp_state & CP_PWRON)
		return 1;
	else
		return 0;
}

int exynos_set_cp_power_onoff(enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_err("%s: mode[%d]\n", __func__, mode);

	/* set power on/off */
	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, 0);

	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	}
	else {
		pr_err("ERR! read Fail: %d\n", cp_ctrl & 0xffff);
		ret = -1;
		goto exit;
	}

	if (mode == CP_POWER_ON) {
		if (cp_ctrl & CP_PWRON) {
			pr_err("CP is already power ON!!\n");
			goto exit;
		}
		ret = exynos_smc(SMC_ID, WRITE_CTRL, (CP_PWRON | CP_START), 0);

		if (ret > 0)
			pr_err("ERR! write Fail: %d\n", ret);
		else
			pr_info("mif: %s: CP Power On & Start: [0x%08X] -> [0x%08X]\n", 
				__func__, cp_ctrl, exynos_smc(SMC_ID, READ_CTRL, 0, 0) >> 16);
	} else {
		ret = exynos_smc(SMC_ID, WRITE_CTRL, (cp_ctrl & ~CP_PWRON), 1);

		if (ret > 0)
			pr_err("ERR! write Fail: %d\n", ret);
		else
			pr_info("mif: %s: CP Power Down: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc(SMC_ID, READ_CTRL, 0, 0) >> 16);
	}

exit:
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
#ifdef DEBUG_SELLP_WITHOUT_CP
	exynos_set_cp_power_onoff(CP_POWER_ON);

	exynos_cp_reset();

	exynos_clear_cp_reset();

	exynos_cp_active_clear();

	exynos_set_cp_power_onoff(CP_POWER_OFF);

	exynos_set_cp_power_onoff(CP_POWER_ON);
#endif

	//exynos_sys_powerdown_conf_cp(CP_POWER_ON);

	return 0;
}
