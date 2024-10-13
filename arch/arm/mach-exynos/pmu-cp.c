#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#if defined(CONFIG_SOC_EXYNOS3475)
#include <mach/smc.h>
#else
#include <linux/smc.h>
#endif

#include <mach/pmu.h>
#if defined(CONFIG_SOC_EXYNOS7580)
#include <mach/regs-pmu-exynos7580.h>
#elif defined(CONFIG_SOC_EXYNOS7890)
#include <mach/regs-pmu-exynos7890.h>
#elif defined(CONFIG_SOC_EXYNOS3475)
#include <mach/regs-pmu-exynos3475.h>
#endif

#if defined(CONFIG_SOC_EXYNOS3475)
#define CP_DVS_INVERSION	BIT(2)
static void exynos_cp_dvs_ctrl(bool enable)
{
	u32 dvs_ctrl = __raw_readl(EXYNOS_PMU_CP_DVS_CTRL);
	u32 lpi_mask = __raw_readl(EXYNOS_PMU_LPI_MASK_MIF_ASB);
	u32 cp_debug = __raw_readl(EXYNOS_PMU_CP_DEBUG);
	u32 central_seq =  __raw_readl(EXYNOS_PMU_CENTRAL_SEQ_MIF_OPTION);

	/*
	 * Exynos3475 share the PMIC buck with CP, If CP was not started, lpm mode,
	 * AP set the DVS_INVERSION bit for disable CP buck control
	 *
	 * DVS_NVERSION
	 * 1 : XDVS_CTRL[1] = 0 // enable false, cp off
	 * 0 : XDVS_CTRL[1] = 1 // enable true
	 */

	if (enable) {
		dvs_ctrl &= ~CP_DVS_INVERSION;
		lpi_mask &= ~LPI_MASK_MIF_ASB;
		cp_debug &= ~(MASK_CLKREQ_CPBLK | MASK_PWR_DOWN_CPBLK);
		central_seq |= USE_CP_ACCESS_MIF;

		__raw_writel(lpi_mask, EXYNOS_PMU_LPI_MASK_MIF_ASB);
		__raw_writel(cp_debug, EXYNOS_PMU_CP_DEBUG);
		__raw_writel(central_seq, EXYNOS_PMU_CENTRAL_SEQ_MIF_OPTION);
	} else
		dvs_ctrl |= CP_DVS_INVERSION;

	__raw_writel(dvs_ctrl, EXYNOS_PMU_CP_DVS_CTRL);
	pr_info("%s: enable(%d), value(0x%08x / 0x%08x / 0x%08x /  0x%08x)\n", __func__, enable,
		dvs_ctrl, lpi_mask, cp_debug, central_seq);
}
#else

static inline void exynos_cp_dvs_ctrl(bool enable) { }
#endif

#if defined(CONFIG_CP_SECURE_BOOT)
static u32 exynos_smc_read(enum cp_control reg)
{
	u32 cp_ctrl;

	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, reg);
	if (!(cp_ctrl & 0xffff)) {
		cp_ctrl >>= 16;
	} else {
		pr_err("%s: ERR! read Fail: %d\n", __func__, cp_ctrl & 0xffff);

		return -1;
	}

	return cp_ctrl;
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
int exynos_cp_reset()
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s\n", __func__);
	exynos_cp_dvs_ctrl(true);

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_cp(CP_RESET);

#if defined(CONFIG_CP_SECURE_BOOT)
	/* assert cp_reset */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl | CP_RESET_SET);
	if (ret < 0) {
		pr_err("%s: ERR! CP Reset Fail: %d\n", __func__, ret);
		return -1;
	}
#else
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
	cp_ctrl |= CP_RESET_SET;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_NS);
#endif

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
	int ret = 0;

	pr_info("%s\n", __func__);
	exynos_cp_dvs_ctrl(true);

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
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_S);
	cp_ctrl |= CP_START;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_S);

	pr_info("%s, cp_ctrl[0x%08x] -> [0x%08x]\n", __func__, cp_ctrl,
			 __raw_readl(EXYNOS_PMU_CP_CTRL_S));
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
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
	cp_ctrl |= CP_ACTIVE_REQ_CLR;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_NS);

	pr_info("%s: cp_ctrl[0x%08x]\n", __func__,
		 __raw_readl(EXYNOS_PMU_CP_CTRL_NS));
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
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
	cp_ctrl |= CP_RESET_REQ_CLR;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_NS);

	pr_info("%s: cp_ctrl[0x%08x]\n", __func__, __raw_readl(EXYNOS_PMU_CP_CTRL_NS));
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
	cp_state = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
#endif

	if (cp_state & CP_PWRON)
		return 1;
	else
		return 0;
}

int exynos_cp_init()
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s: cp_ctrl init\n", __func__);

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_RESET & ~CP_PWRON);
	if (ret < 0)
		pr_err("%s: ERR! write Fail: %d\n", __func__, ret);

	cp_ctrl = exynos_smc_read(CP_CTRL_S);
	if (cp_ctrl == -1)
		return -1;

	ret = exynos_smc_write(CP_CTRL_S, cp_ctrl & ~CP_START);
	if (ret < 0)
		pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
#else
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
	cp_ctrl &= ~CP_RESET_SET & ~CP_PWRON;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_NS);

	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_S);
	cp_ctrl &= ~CP_START;
	__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_S);
#endif
	return ret;
}

int exynos_set_cp_power_onoff(enum cp_mode mode)
{
	u32 cp_ctrl;
	int ret = 0;

	pr_info("%s: mode[%d]\n", __func__, mode);

	if (mode == CP_POWER_ON)
		exynos_cp_dvs_ctrl(true);

#if defined(CONFIG_CP_SECURE_BOOT)
	/* set power on/off */
	cp_ctrl = exynos_smc_read(CP_CTRL_NS);
	if (cp_ctrl == -1)
		return -1;

	if (mode == CP_POWER_ON) {
		ret = exynos_smc_write(CP_CTRL_NS, (cp_ctrl | CP_PWRON));
		if (ret < 0)
			pr_err("%s: ERR! write Fail: %d\n", __func__, ret);
		else
			pr_info("%s: CP Power: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_NS));

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
		ret = exynos_smc_write(CP_CTRL_NS, cp_ctrl & ~CP_PWRON);
		if (ret < 0)
			pr_err("ERR! write Fail: %d\n", ret);
		else
			pr_info("%s: CP Power Down: [0x%08X] -> [0x%08X]\n", __func__,
				cp_ctrl, exynos_smc_read(CP_CTRL_NS));
	}
#else
	if (mode == CP_POWER_ON) {
		cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
		if  (cp_ctrl & CP_PWRON) {
			pr_err("CP is already power ON!!\n");
			ret = -1;
			goto exit;
		}
		cp_ctrl |= CP_PWRON;
		__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_NS);

		cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_S);
		cp_ctrl |= CP_START;
		__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_S);
		pr_err("%s: CP Power Register: (NS) 0x%x, (S)0x%x\n", __func__,
			__raw_readl(EXYNOS_PMU_CP_CTRL_NS), __raw_readl(EXYNOS_PMU_CP_CTRL_S));
	} else {
		cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
		cp_ctrl &= ~CP_PWRON;
		__raw_writel(cp_ctrl, EXYNOS_PMU_CP_CTRL_NS);
	}
exit:
#endif
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
#if defined(CONFIG_SOC_EXYNOS7580)
	__raw_writel(0, EXYNOS_PMU_OSCCLK_GATE_CPU_SYS_PWR_REG);
#endif
}

#if !defined(CONFIG_CP_SECURE_BOOT)

#define MEMSIZE		92
#define SHDMEM_BASE	0x70000000
#define MEMSIZE_OFFSET	16
#define MEMBASE_ADDR_OFFSET	0

static void set_shdmem_size(int memsz)
{
	u32 tmp;
	pr_info("[Modem_IF]Set shared mem size: %dMB\n", memsz);

	memsz /= 4;
	tmp = readl(EXYNOS_PMU_CP2AP_MEM_CONFIG);

	tmp &= ~(0x1ff << MEMSIZE_OFFSET);
	tmp |= (memsz << MEMSIZE_OFFSET);

	writel(tmp, EXYNOS_PMU_CP2AP_MEM_CONFIG);

	pr_info("[Modem_IF]Set shared mem size: %x\n", readl(EXYNOS_PMU_CP2AP_MEM_CONFIG));
}

static void set_shdmem_base(void)
{
	u32 tmp, base_addr;
	pr_info("[Modem_IF]Set shared mem baseaddr : 0x%x\n", SHDMEM_BASE);

	base_addr = (SHDMEM_BASE >> 22);

	tmp = readl(EXYNOS_PMU_CP2AP_MEM_CONFIG);

	tmp &= ~(0x3fff << MEMBASE_ADDR_OFFSET);
	tmp |= (base_addr << MEMBASE_ADDR_OFFSET);

	writel(tmp, EXYNOS_PMU_CP2AP_MEM_CONFIG);
	pr_info("[Modem_IF]Set shared mem baseaddr : %x\n", readl(EXYNOS_PMU_CP2AP_MEM_CONFIG));
}

static void set_batcher(void)
{
	u32 window;

	writel(0x444CCCCC, EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN0);
	writel(0x4CC44444, EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN1);

	window = readl(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2);
	window |= APBSEM_BATCHER | APBSEM_EN;
	writel(window, EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2);

	pr_info("[Modem_IF]Set access_window0 : %x\n", readl(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN0));
	pr_info("[Modem_IF]Set access_window1 : %x\n", readl(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN1));
	pr_info("[Modem_IF]Set access_window2 : %x\n", readl(EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2));
}
#endif

#define PR_REG(name, reg) pr_info("%s: 0x%08x\n", name, __raw_readl(reg));

/**
*	dump_reg
*/
void exynos_cp_dump_pmu_reg(void)
{
	u32 cp_ctrl = 0;

#if defined(CONFIG_CP_SECURE_BOOT)
	cp_ctrl = exynos_smc(SMC_ID, READ_CTRL, 0, 0);
#else

#if !defined(CONFIG_CP_SECURE_BOOT)
	/* To do: will be removed after EL mon work */
	set_shdmem_size(MEMSIZE);
	set_shdmem_base();
	set_batcher();
#endif
	cp_ctrl = __raw_readl(EXYNOS_PMU_CP_CTRL_NS);
#endif

	pr_info("---------- PMU CP RegDump ----------\n");
	pr_info("%s: 0x%08x\n", "CP_CTRL", cp_ctrl);

#if defined(CONFIG_SOC_EXYNOS7580)

	PR_REG("CP_STAT", EXYNOS_PMU_CP_START);
	PR_REG("CP_DEBUG", EXYNOS_PMU_CP_DEBUG);
	PR_REG("CP_SEMAPHORE", EXYNOS_PMU_CP_SEMAPHORE);

	PR_REG("CP2AP_MEM_CONFIG", EXYNOS_PMU_CP2AP_MEM_CONFIG);
	PR_REG("CP2AP_MIF_ACCESS_WIN0", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN0);
	PR_REG("CP2AP_MIF_ACCESS_WIN1", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN1);
	PR_REG("CP2AP_MIF_ACCESS_WIN2", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2);
	PR_REG("CP2AP_PERI_ACCESS_WIN", EXYNOS_PMU_CP2AP_PERI_ACCESS_WIN);

	PR_REG("CP_BOOT_TEST_RST_CONFIG", EXYNOS_PMU_CP_BOOT_TEST_RST_CONFIG);
	PR_REG("MIF_TZPC_CFG", EXYNOS_PMU_MIF_TZPC_CONFIG);
	PR_REG("AUD_PATH_CFG", EXYNOS_PMU_AUD_PATH_CFG);
	PR_REG("CP_MIF_CLKCTRL", EXYNOS_PMU_CP_MIF_CLKCTRL);

#elif defined(CONFIG_SOC_EXYNOS7890)

	PR_REG("CP_CTRL_NS", EXYNOS_PMU_CP_CTRL_NS);
	PR_REG("CP_CTRL_S", EXYNOS_PMU_CP_CTRL_S);

	PR_REG("CP_STAT", EXYNOS_PMU_CP_STAT);
	PR_REG("CP_DEBUG", EXYNOS_PMU_CP_DEBUG);

	PR_REG("CP2AP_MEM_CONFIG", EXYNOS_PMU_CP2AP_MEM_CONFIG);
	PR_REG("CP2AP_CCORE0_PERI_ACCESS_CON", EXYNOS_PMU_CP2AP_CCORE0_PERI_ACCESS_CON);
	PR_REG("CP2AP_CCORE1_PERI_ACCESS_CON", EXYNOS_PMU_CP2AP_CCORE1_PERI_ACCESS_CON);
	PR_REG("CP2AP_MIF0_PERI_ACCESS_CON", EXYNOS_PMU_CP2AP_MIF0_PERI_ACCESS_CON);
	PR_REG("CP2AP_MIF1_PERI_ACCESS_CON", EXYNOS_PMU_CP2AP_MIF1_PERI_ACCESS_CON);
	PR_REG("CP2AP_MIF2_PERI_ACCESS_CON", EXYNOS_PMU_CP2AP_MIF2_PERI_ACCESS_CON);
	PR_REG("CP2AP_MIF3_PERI_ACCESS_CON", EXYNOS_PMU_CP2AP_MIF3_PERI_ACCESS_CON);

	PR_REG("CP2AP_PERI_ACCESS_WIN", EXYNOS_PMU_CP2AP_PERI_ACCESS_WIN);

	PR_REG("CP_BOOT_TEST_RST_CONFIG", EXYNOS_PMU_CP_BOOT_TEST_RST_CONFIG);
	PR_REG("MODAPIF_CONFIG", EXYNOS_PMU_MODAPIF_CONFIG);
	PR_REG("CCORE_RCG_EN", EXYNOS_PMU_CCORE_RCG_EN);
	PR_REG("CP_CLK_CTRL", EXYNOS_PMU_CP_CLK_CTRL);

#elif defined(CONFIG_SOC_EXYNOS3475)
	PR_REG("CP_STAT", EXYNOS_PMU_CP_STAT);
	PR_REG("CP_DEBUG", EXYNOS_PMU_CP_DEBUG);
	PR_REG("CP_SEMAPHORE", EXYNOS_PMU_CP_SEMAPHORE);

#if !defined(CONFIG_CP_SECURE_BOOT)
	PR_REG("CP2AP_MEM_CONFIG", EXYNOS_PMU_CP2AP_MEM_CONFIG);
	PR_REG("CP2AP_MIF_ACCESS_WIN0", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN0);
	PR_REG("CP2AP_MIF_ACCESS_WIN1", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN1);
	PR_REG("CP2AP_MIF_ACCESS_WIN2", EXYNOS_PMU_CP2AP_MIF_ACCESS_WIN2);
	PR_REG("CP2AP_PERI_ACCESS_WIN", EXYNOS_PMU_CP2AP_PERI_ACCESS_WIN);

	PR_REG("CP_BOOT_TEST_RST_CONFIG", EXYNOS_PMU_CP_BOOT_TEST_RST_CONFIG);
	PR_REG("MIF_TZPC_CFG", EXYNOS_PMU_MIF_TZPC_CONFIG);
	PR_REG("AUD_PATH_CFG", EXYNOS_PMU_AUD_PATH_CFG);
#endif
#endif
}
EXPORT_SYMBOL(exynos_cp_dump_pmu_reg);

int exynos_pmu_cp_init(void)
{
	exynos_cp_dvs_ctrl(false);
#if !defined(CONFIG_CP_SECURE_BOOT)
	set_shdmem_size(MEMSIZE);
	set_shdmem_base();
	set_batcher();
#endif

	return 0;
}
