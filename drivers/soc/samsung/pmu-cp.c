#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/smc.h>
#include <linux/shm_ipc.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/pmu-cp.h>

#if defined(CONFIG_CP_SECURE_BOOT)
static u32 exynos_smc_read(enum cp_control reg)
{
	u32 cp_ctrl;
	u32 cp_ctrl_low;
	u32 cp_ctrl_high;

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, reg);
	
	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
		cp_ctrl_low = cp_ctrl;
	} else {
		pr_err("%s: ERR! read Fail: %d\n", __func__, cp_ctrl & 0xffff);

		return -1;
	}

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 1, reg);

	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
		cp_ctrl_high = cp_ctrl;
	} else {
		pr_err("%s: ERR! read Fail: %d\n", __func__, cp_ctrl & 0xffff);

		return -1;
	}

	return (cp_ctrl_high << 16) | cp_ctrl_low;
}

static u32 exynos_smc_write(enum cp_control reg, u32 value)
{
	int ret = 0;

	ret = exynos_smc(SMC_ID, WRITE_CTRL, value, reg);
	if (ret > 0) {
		pr_err("%s: ERR! CP_CTRL Write Fail: %d\n", __func__, ret);
		return -1;
	}

	return 0;
}
#endif

/* reset cp */
int exynos_cp_reset(void)
{
	int ret = 0;
	u32 __maybe_unused cp_ctrl;

	pr_info("%s\n", __func__);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp();

#if defined(CONFIG_CP_SECURE_BOOT)
	/* assert cp_reset */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_SET | CP_PWRON);
	if (ret < 0) {
		pr_err("%s: ERR! CP Reset Fail: %d\n", __func__, ret);
		return -1;
	}
#else
	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	cp_ctrl |= CP_RESET_SET | CP_PWRON;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
	if (ret < 0) {
		pr_err("%s: ERR! CP Reset Fail: %d\n", __func__, ret);
		return -1;
	}
#endif

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);

	return ret;
}

/* release cp */
int exynos_cp_release(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl | CP_START);
	if (ret < 0)
		pr_err("ERR! CP Release Fail: %d\n", ret);
	else
		pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_S));
#else

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_S, &cp_ctrl);
	cp_ctrl |= CP_START;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_S, cp_ctrl);
	if (ret < 0)
		pr_err("ERR! CP Release Fail: %d\n", ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_CP_CTRL_S, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_S[0x%08x]\n", __func__, cp_ctrl);
	}
#endif

	return ret;
}

/* clear cp active */
int exynos_cp_active_clear(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_ACTIVE_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERR! CP active_clear Fail: %d\n", __func__, ret);
	else
		pr_info("%s: cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_NS));
#else

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	cp_ctrl |= CP_ACTIVE_REQ_CLR;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
	if (ret < 0)
		pr_err("%s: ERR! CP active_clear Fail: %d\n", __func__, ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_NS[0x%08x]\n", __func__, cp_ctrl);
	}
#endif
	return ret;
}

/* clear cp_reset_req from cp */
int exynos_clear_cp_reset(void)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_REQ_CLR);
	if (ret < 0)
		pr_err("%s: ERR! CP clear_cp_reset Fail: %d\n", __func__, ret);
	else
		pr_info("%s: cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			exynos_smc_read(CP_CTRL_NS));
#else

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	cp_ctrl |= CP_RESET_REQ_CLR;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
	if (ret < 0)
		pr_err("%s: ERR! CP clear_cp_reset Fail: %d\n", __func__, ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		pr_info("%s, PMU_CP_CTRL_NS[0x%08x]\n", __func__, cp_ctrl);
	}
#endif

	return ret;
}

int exynos_get_cp_power_status(void)
{
	u32 cp_state;

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_state = exynos_smc_read(CP_CTRL_NS);
	if (cp_state == -1)
		return -1;
#else
	exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_state);
#endif

	if (cp_state & CP_PWRON)
		return 1;
	else
		return 0;
}

int exynos_cp_init(void)
{
	u32 __maybe_unused cp_ctrl;
	int ret = 0;

	pr_info("%s: cp_ctrl init\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_RESET_SET & ~CP_PWRON);
	if (ret < 0)
		pr_err("%s: ERR! write Fail: %d\n", __func__, ret);

	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl & ~CP_START);
	if (ret < 0)
		pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
#else
	exynos_pmu_cp_init();

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	cp_ctrl &= ~CP_RESET_SET;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
	if (ret < 0)
		pr_err("%s: ERR! CP_RESET_SET Fail: %d\n", __func__, ret);

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	cp_ctrl &= ~CP_PWRON;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
	if (ret < 0)
		pr_err("%s: ERR! CP_PWRON Fail: %d\n", __func__, ret);

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	cp_ctrl &= ~CP_START;

	ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
	if (ret < 0)
		pr_err("%s: ERR! CP_START Fail: %d\n", __func__, ret);

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_S, &cp_ctrl);
	pr_err("EXYNOS_PMU_CP_CTRL_S: %08X\n", cp_ctrl);

	ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	pr_err("EXYNOS_PMU_CP_CTRL_NS: %08X\n", cp_ctrl);
#endif

	return ret;
}

int exynos_set_cp_power_onoff(enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s: mode[%d]\n", __func__, mode);

#if defined(CONFIG_CP_SECURE_BOOT)
	/* set power on/off */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	if (mode == CP_POWER_ON) {
		if (!(cp_ctrl & CP_PWRON)) {
			ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_PWRON);
			if (ret < 0)
				pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
			else
				pr_info("%s: CP Power: [0x%08X] -> [0x%08X]\n",
					__func__, cp_ctrl, exynos_smc_read(CP_CTRL_NS));
		}
		cp_ctrl = exynos_smc_read(CP_CTRL_S);
		if (cp_ctrl == -1)
			return -1;

		ret = exynos_smc_write(CP_CTRL_S, cp_ctrl | CP_START);
		if (ret < 0)
			pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
		else
			pr_info("%s: CP Start: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_S));
	} else {
		/* set sys_pwr_cfg registers */
		exynos_sys_powerdown_conf_cp();

		ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_SET | CP_PWRON);
		if (ret < 0)
			pr_err("ERR! write Fail: %d\n", ret);
		else
			pr_info("%s: CP Power Down: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_NS));
	}
#else
	exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
	if (mode == CP_POWER_ON) {
		if (!(cp_ctrl & CP_PWRON)) {
			ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
			cp_ctrl |= CP_PWRON;

			ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
			if (ret < 0)
				pr_err("%s: ERR! write Fail: %d\n",
						__func__, ret);
		}

		ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_S, &cp_ctrl);
		cp_ctrl |= CP_START;

		ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_S, cp_ctrl);
		if (ret < 0)
			pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
	} else {
		/* set sys_pwr_cfg registers */
		exynos_sys_powerdown_conf_cp();

		ret = exynos_pmu_read(EXYNOS_PMU_CP_CTRL_NS, &cp_ctrl);
		cp_ctrl |= CP_RESET_SET | CP_PWRON;

		ret = exynos_pmu_write(EXYNOS_PMU_CP_CTRL_NS, cp_ctrl);
		if (ret < 0)
			pr_err("ERR! write Fail: %d\n", ret);
	}
#endif
	return ret;
}

void exynos_sys_powerdown_conf_cp(void)
{
	pr_info("%s\n", __func__);

	exynos_pmu_write(EXYNOS_PMU_RESET_AHEAD_CP_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_LOGIC_RESET_CP_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_RESET_ASB_CP_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_TCXO_GATE_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_CENTRAL_SEQ_CP_CONFIGURATION, 0);
}

#if !defined(CONFIG_CP_SECURE_BOOT)
static void __init set_shdmem_size(unsigned memsz)
{
	u32 tmp;
	pr_info("[Modem_IF]Set shared mem size: %dMB\n", memsz);

#if defined(CONFIG_SOC_EXYNOS7570)
	memsz *= 256;
#else
	memsz /= 4;
#endif
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MEM_CONFIG, memsz);

	exynos_pmu_read(EXYNOS_PMU_CP2AP_MEM_CONFIG, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MEM_CONFIG: 0x%x\n", tmp);

#if defined(CONFIG_SOC_EXYNOS7570)
	exynos_pmu_write(EXYNOS_PMU_CP2AP_ADDR_RNG, memsz);

	exynos_pmu_read(EXYNOS_PMU_CP2AP_ADDR_RNG, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_ADDR_RNG: 0x%x\n", tmp);
#endif
}

static void __init set_shdmem_base(unsigned long base)
{
	u32 tmp, base_addr;
	pr_info("[Modem_IF]Set shared mem baseaddr : 0x%x\n", (unsigned int)base);

#if defined(CONFIG_SOC_EXYNOS7570)
	base_addr = (base >> 12);
#else
	base_addr = (base >> 22);
#endif
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF_CONFIG1, base_addr);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF_CONFIG2, base_addr);

	exynos_pmu_read(EXYNOS_PMU_CP2AP_MIF_CONFIG1, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MIF_CONFIG1: 0x%x\n", tmp);

	exynos_pmu_read(EXYNOS_PMU_CP2AP_MIF_CONFIG2, &tmp);
	pr_info("[Modem_IF] EXYNOS_PMU_CP2AP_MIF_CONFIG2: 0x%x\n", tmp);
}

static void set_batcher(void)
{
	exynos_pmu_write(EXYNOS_PMU_MODAPIF_CONFIG, 0x3);
}
#endif

int exynos_pmu_cp_init(void)
{

#if !defined(CONFIG_CP_SECURE_BOOT)
	unsigned shm_size;
	unsigned long shm_base;

	shm_size = shm_get_phys_size();
	shm_base = shm_get_phys_base();

	/* Change size from byte to Mega byte*/
	shm_size /= 1048510;

	set_shdmem_size(shm_size);
	set_shdmem_base(shm_base);
	set_batcher();

	/* set access window for CP */
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN0, 0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN1, 0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2, 0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN3, 0xffffffff);
	exynos_pmu_write(EXYNOS_PMU_CP2AP_PERI_ACCESS_WIN, 0xffffffff);
#endif

	return 0;
}
