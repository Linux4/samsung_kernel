#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/smc.h>
#include <soc/samsung/exynos-pmu.h>
#include "pmu-gnss.h"

static void __set_shdmem_size(struct gnss_ctl *gc, u32 reg_offset, u32 memsz)
{
	u32 tmp;
	memsz = (memsz >> MEMSIZE_SHIFT);
#ifdef USE_IOREMAP_NOPMU
	{
		u32 memcfg_val;
		memcfg_val = __raw_readl(gc->pmu_reg + reg_offset);
		memcfg_val &= ~(MEMSIZE_MASK << MEMSIZE_OFFSET);
		memcfg_val |= (memsz << MEMSIZE_OFFSET);
		__raw_writel(memcfg_val, gc->pmu_reg + reg_offset);
		tmp = __raw_readl(gc->pmu_reg + reg_offset);
	}
#else
	exynos_pmu_update(reg_offset, MEMSIZE_MASK << MEMSIZE_OFFSET,
			memsz << MEMSIZE_OFFSET);
	exynos_pmu_read(reg_offset, &tmp);
#endif
}

static void set_shdmem_size(struct gnss_ctl *gc, u32 memsz)
{
	gif_err("[GNSS]Set shared mem size: %dB\n", memsz);

#if !defined(CONFIG_SOC_EXYNOS7870) && !defined(CONFIG_SOC_EXYNOS7880)
	__set_shdmem_size(gc, EXYNOS_PMU_GNSS2AP_MEM_CONFIG, memsz);
	__set_shdmem_size(gc, EXYNOS_PMU_GNSS2AP_MEM_CONFIG3, memsz);
#else
	__set_shdmem_size(gc, EXYNOS_PMU_GNSS2AP_MEM_CONFIG, memsz);
#endif
}

static void __set_shdmem_base(struct gnss_ctl *gc, u32 reg_offset, u32 shmem_base)
{
	u32 tmp, base_addr;
	base_addr = (shmem_base >> MEMBASE_ADDR_SHIFT);

#ifdef USE_IOREMAP_NOPMU
	{
		u32 memcfg_val;
		gif_err("Access Reg : 0x%p\n", gc->pmu_reg + reg_offset);

		memcfg_val = __raw_readl(gc->pmu_reg + reg_offset);
		memcfg_val &= ~(MEMBASE_ADDR_MASK << MEMBASE_ADDR_OFFSET);
		memcfg_val |= (base_addr << MEMBASE_ADDR_OFFSET);
		__raw_writel(memcfg_val, gc->pmu_reg + reg_offset);
		tmp = __raw_readl(gc->pmu_reg + reg_offset);
	}
#else
	exynos_pmu_update(reg_offset, MEMBASE_ADDR_MASK << MEMBASE_ADDR_OFFSET,
			base_addr << MEMBASE_ADDR_OFFSET);
	exynos_pmu_read(reg_offset, &tmp);
#endif
}

static void set_shdmem_base(struct gnss_ctl *gc, u32 shmem_base)
{
	gif_err("[GNSS]Set shared mem baseaddr : 0x%x\n", shmem_base);

#if !defined(CONFIG_SOC_EXYNOS7870) && !defined(CONFIG_SOC_EXYNOS7880)
	__set_shdmem_base(gc, EXYNOS_PMU_GNSS2AP_MEM_CONFIG1, shmem_base);
	__set_shdmem_base(gc, EXYNOS_PMU_GNSS2AP_MEM_CONFIG2, shmem_base);
#else
	__set_shdmem_base(gc, EXYNOS_PMU_GNSS2AP_MEM_CONFIG, shmem_base);
#endif
}

static void exynos_sys_powerdown_conf_gnss(struct gnss_ctl *gc)
{
#ifdef USE_IOREMAP_NOPMU
	__raw_writel(0, gc->pmu_reg + EXYNOS_PMU_CENTRAL_SEQ_GNSS_CONFIGURATION);
	__raw_writel(0, gc->pmu_reg + EXYNOS_PMU_RESET_AHEAD_GNSS_SYS_PWR_REG);
	__raw_writel(0, gc->pmu_reg + EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG);
	__raw_writel(0, gc->pmu_reg + EXYNOS_PMU_LOGIC_RESET_GNSS_SYS_PWR_REG);
	__raw_writel(0, gc->pmu_reg + EXYNOS_PMU_TCXO_GATE_GNSS_SYS_PWR_REG);
	__raw_writel(0, gc->pmu_reg + EXYNOS_PMU_RESET_ASB_GNSS_SYS_PWR_REG);
#else
	exynos_pmu_write(EXYNOS_PMU_CENTRAL_SEQ_GNSS_CONFIGURATION, 0);
	exynos_pmu_write(EXYNOS_PMU_RESET_AHEAD_GNSS_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_CLEANY_BUS_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_LOGIC_RESET_GNSS_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_TCXO_GATE_GNSS_SYS_PWR_REG, 0);
	exynos_pmu_write(EXYNOS_PMU_RESET_ASB_GNSS_SYS_PWR_REG, 0);
#endif
}

int gnss_pmu_clear_interrupt(struct gnss_ctl *gc, enum gnss_int_clear gnss_int)
{
	int ret = 0;

	gif_debug("%s\n", __func__);
#ifdef USE_IOREMAP_NOPMU
	{
		u32 reg_val = 0;

		reg_val = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);

		if (gnss_int == GNSS_INT_WAKEUP_CLEAR) {
			reg_val |= GNSS_WAKEUP_REQ_CLR;
		} else if (gnss_int == GNSS_INT_ACTIVE_CLEAR) {
			reg_val |= GNSS_ACTIVE_REQ_CLR;
		} else if (gnss_int == GNSS_INT_WDT_RESET_CLEAR) {
			reg_val |= GNSS_WAKEUP_REQ_CLR;
		} else {
			gif_err("Unexpected interrupt value!\n");
			return -EIO;
		}
		__raw_writel(reg_val, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
	}
#else
	if (gnss_int == GNSS_INT_WAKEUP_CLEAR) {
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				GNSS_WAKEUP_REQ_CLR, GNSS_WAKEUP_REQ_CLR);
	} else if (gnss_int == GNSS_INT_ACTIVE_CLEAR) {
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				GNSS_ACTIVE_REQ_CLR, GNSS_ACTIVE_REQ_CLR);
	} else if (gnss_int == GNSS_INT_WDT_RESET_CLEAR) {
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				GNSS_RESET_REQ_CLR, GNSS_RESET_REQ_CLR);
	} else {
		gif_err("Unexpected interrupt value!\n");
		return -EIO;
	}

	if (ret < 0) {
		gif_err("ERR! GNSS Reset Fail: %d\n", ret);
		return -EIO;
	}
#endif

	return ret;
}

int gnss_pmu_release_reset(struct gnss_ctl *gc)
{
	u32 gnss_ctrl = 0;
	int ret = 0;

#ifdef USE_IOREMAP_NOPMU
	gnss_ctrl = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
	{
		u32 tmp_reg_val;
		if (!(gnss_ctrl & GNSS_PWRON)) {
			gnss_ctrl |= GNSS_PWRON;
			__raw_writel(gnss_ctrl, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		}

		tmp_reg_val = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_S);
		tmp_reg_val |= GNSS_START;
		__raw_writel(tmp_reg_val, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_S);

		gif_err("PMU_GNSS_CTRL_S : 0x%x\n",
				__raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_S));
	}
#else
	exynos_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &gnss_ctrl);
	if (!(gnss_ctrl & GNSS_PWRON)) {
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS, GNSS_PWRON,
				GNSS_PWRON);
		if (ret < 0) {
			gif_err("ERR! write Fail: %d\n", ret);
			ret = -EIO;
		}
	}
	ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_S, GNSS_START, GNSS_START);
	if (ret < 0)
		gif_err("ERR! GNSS Release Fail: %d\n", ret);
	else {
		exynos_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &gnss_ctrl);
		gif_info("PMU_GNSS_CTRL_S[0x%08x]\n", gnss_ctrl);
		ret = -EIO;
	}
#endif

	return ret;
}

int gnss_pmu_hold_reset(struct gnss_ctl *gc)
{
	int ret = 0;
	u32 __maybe_unused gnss_ctrl;

	/* set sys_pwr_cfg registers */
	exynos_sys_powerdown_conf_gnss(gc);

#ifdef USE_IOREMAP_NOPMU
	{
		u32 reg_val;
		reg_val = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		reg_val |= GNSS_RESET_SET;
		__raw_writel(reg_val, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
	}
#else
	ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS, GNSS_RESET_SET,
			GNSS_RESET_SET);
	if (ret < 0) {
		gif_err("ERR! GNSS Reset Fail: %d\n", ret);
		return -1;
	}
#endif

	/* some delay */
	cpu_relax();
	usleep_range(80, 100);

	return ret;
}

int gnss_pmu_power_on(struct gnss_ctl *gc, enum gnss_mode mode)
{
	u32 gnss_ctrl;
	int ret = 0;

	gif_err("mode[%d]\n", mode);

#ifdef USE_IOREMAP_NOPMU
	gnss_ctrl = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
	if (mode == GNSS_POWER_ON) {
		u32 tmp_reg_val;
		if (!(gnss_ctrl & GNSS_PWRON)) {
			gnss_ctrl |= GNSS_PWRON;
			__raw_writel(gnss_ctrl, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		}

		tmp_reg_val = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_S);
		tmp_reg_val |= GNSS_START;
		__raw_writel(tmp_reg_val, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_S);
	} else {
		gif_err("Not supported!!!(%d)\n", mode);
		return -1;
	}
#else
	exynos_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &gnss_ctrl);
	if (mode == GNSS_POWER_ON) {
		if (!(gnss_ctrl & GNSS_PWRON)) {
			ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
					GNSS_PWRON, GNSS_PWRON);
			if (ret < 0)
				gif_err("ERR! write Fail: %d\n", ret);
		}

		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_S, GNSS_START,
				GNSS_START);
		if (ret < 0)
			gif_err("ERR! write Fail: %d\n", ret);
	} else {
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS, GNSS_PWRON, 0);
		if (ret < 0) {
			gif_err("ERR! write Fail: %d\n", ret);
			return ret;
		}
		/* set sys_pwr_cfg registers */
		exynos_sys_powerdown_conf_gnss(gc);
	}
#endif

	return ret;
}

int gnss_change_tcxo_mode(struct gnss_ctl *gc, enum gnss_tcxo_mode mode)
{
	int ret = 0;
#ifdef USE_IOREMAP_NOPMU
	{
		u32 regval, tmp;
		regval = __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		if (mode == TCXO_SHARED_MODE) {
			gif_err("Change TCXO mode to Shared Mode(%d)\n", mode);
			regval &= ~TCXO_26M_40M_SEL;
			__raw_writel(regval, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		} else if (mode == TCXO_NON_SHARED_MODE) {
			gif_err("Change TCXO mode to NON-sared Mode(%d)\n", mode);
			regval |= TCXO_26M_40M_SEL;
			__raw_writel(regval, gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		} else
			gif_err("Unexpected modem(Mode:%d)\n", mode);

		tmp =  __raw_readl(gc->pmu_reg + EXYNOS_PMU_GNSS_CTRL_NS);
		if (tmp != regval) {
			gif_err("ERR! GNSS change tcxo: %d\n", ret);
			return -1;
		}
	}
#else
	if (mode == TCXO_SHARED_MODE) {
		gif_err("Change TCXO mode to Shared Mode(%d)\n", mode);
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				TCXO_26M_40M_SEL, 0);
	} else if (mode == TCXO_NON_SHARED_MODE) {
		gif_err("Change TCXO mode to NON-sared Mode(%d)\n", mode);
		ret = exynos_pmu_update(EXYNOS_PMU_GNSS_CTRL_NS,
				TCXO_26M_40M_SEL, TCXO_26M_40M_SEL);
	} else
		gif_err("Unexpected modem(Mode:%d)\n", mode);

	if (ret < 0) {
		gif_err("ERR! GNSS change tcxo: %d\n", ret);
		return -1;
	}
#endif
	return 0;
}

int gnss_pmu_init_conf(struct gnss_ctl *gc)
{
	u32 shmem_size = gc->gnss_data->shmem_size;
	u32 shmem_base = gc->gnss_data->shmem_base;

	set_shdmem_size(gc, shmem_size);
	set_shdmem_base(gc, shmem_base);

#ifndef USE_IOREMAP_NOPMU
	/* set access window for GNSS */
	exynos_pmu_write(EXYNOS_PMU_GNSS2AP_MIF0_PERI_ACCESS_CON, 0x0);
	exynos_pmu_write(EXYNOS_PMU_GNSS2AP_MIF1_PERI_ACCESS_CON, 0x0);
#if !defined(CONFIG_SOC_EXYNOS7870)
	exynos_pmu_write(EXYNOS_PMU_GNSS2AP_MIF2_PERI_ACCESS_CON, 0x0);
	exynos_pmu_write(EXYNOS_PMU_GNSS2AP_MIF3_PERI_ACCESS_CON, 0x0);
#endif
	exynos_pmu_write(EXYNOS_PMU_GNSS2AP_PERI_ACCESS_WIN, 0x0);
#endif

	return 0;
}

