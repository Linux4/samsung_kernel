/*
 * Exynos MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mipi-lli.h>

#include "exynos-mipi-lli.h"
#include "exynos-mipi-lli-mphy.h"

#define EXYNOS_LLI_LINK_START		(0x4000)
/*
 * 5ns, Default System Clock 100MHz
 * SYSTEM_CLOCK_PERIOD = 1000MHz / System Clock
 */
#define SYSTEM_CLOCK_PERIOD		(10)
#define SIG_INT_MASK0			(0xFFFFFFFF)
#define SIG_INT_MASK1			(0xFFFFFFFF)

static int poll_bit_set(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = readl(ptr);
		if (reg & val)
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

static u32 exynos_lli_cal_remap(u32 base_addr, unsigned long size)
{
	u32 remap_addr, bit_pos;

	if (!size)
		return 0;

	bit_pos = find_first_bit(&size, 32);

	/* check size is pow of 2 */
	if (size != (1 << bit_pos))
		return 0;

	/* check base_address is aligned with size */
	if (base_addr & (size - 1))
		return 0;

	remap_addr = (base_addr >> bit_pos) << LLI_REMAP_BASE_ADDR_SHIFT;
	remap_addr |= bit_pos;
	remap_addr |= LLI_REMAP_ENABLE;

	return remap_addr;
}

static void exynos_lli_print_dump(struct work_struct *work)
{
	struct mipi_lli *lli = container_of(work, struct mipi_lli,
			wq_print_dump);
	struct mipi_lli_dump *dump = &lli->dump;
	int len = 0, i = 0;

	len = sizeof(lli_debug_clk_info) / sizeof(lli_debug_clk_info[0]);
	for (i = 0; i < len; i++)
	{
		dev_err(lli->dev, "[LLI-CLK]0x%p : 0x%08x\n",
				lli_debug_clk_info[i], dump->clk[i]);
	}

	len = sizeof(lli_debug_info) / sizeof(lli_debug_info[0]);
	for (i = 0; i < len; i++)
	{
		dev_err(lli->dev, "[LLI]0x%x : 0x%08x\n",
				0x10F24000 + (lli_debug_info[i]),
				dump->lli[i]);
	}

	len = sizeof(phy_std_debug_info) / sizeof(phy_std_debug_info[0]);
	for (i = 0; i < len; i++)
	{
		dev_err(lli->dev, "[MPHY-STD]0x%x : 0x%08x\n",
				0x10F20000 + (phy_std_debug_info[i] / 4),
				dump->mphy_std[i]);
	}

	len = sizeof(phy_cmn_debug_info) / sizeof(phy_cmn_debug_info[0]);
	for (i = 0; i < len; i++)
	{
		dev_err(lli->dev, "[MPHY-CMN]0x%x : 0x%08x\n",
				0x10F20000 + (phy_cmn_debug_info[i] / 4),
				dump->mphy_cmn[i]);
	}

	len = sizeof(phy_ovtm_debug_info) / sizeof(phy_ovtm_debug_info[0]);
	for (i = 0; i < len; i++)
	{
		dev_err(lli->dev, "[MPHY-OVTM]0x%x : 0x%08x\n",
				0x10F20000 + (phy_ovtm_debug_info[i] / 4),
				dump->mphy_ovtm[i]);
	}

	memset(dump, 0, sizeof(struct mipi_lli_dump));
}

static int exynos_lli_reg_dump(struct mipi_lli *lli)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);
	struct mipi_lli_dump *dump = &lli->dump;
	int len = 0, i = 0;

	memset(dump, 0, sizeof(struct mipi_lli_dump));

	len = sizeof(lli_debug_clk_info) / sizeof(lli_debug_clk_info[0]);
	for (i = 0; i < len; i++)
		dump->clk[i] = readl(lli_debug_clk_info[i]);

	len = sizeof(lli_debug_info) / sizeof(lli_debug_info[0]);
	for (i = 0; i < len; i++){
		dump->lli[i] = readl(lli->regs + lli_debug_info[i]);
	}

	len = sizeof(phy_std_debug_info) / sizeof(phy_std_debug_info[0]);
	for (i = 0; i < len; i++)
		dump->mphy_std[i] = readl(phy->loc_regs + phy_std_debug_info[i]);

	len = sizeof(phy_cmn_debug_info) / sizeof(phy_cmn_debug_info[0]);
	writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	for (i = 0; i < len; i++)
		dump->mphy_cmn[i] = readl(phy->loc_regs + phy_cmn_debug_info[i]);
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);

	len = sizeof(phy_ovtm_debug_info) / sizeof(phy_ovtm_debug_info[0]);
	writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	for (i = 0; i < len; i++)
		dump->mphy_ovtm[i] = readl(phy->loc_regs + phy_ovtm_debug_info[i]);
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);

	return 0;
}

static int exynos_lli_debug_info(struct mipi_lli *lli)
{
	exynos_lli_reg_dump(lli);
	schedule_work(&lli->wq_print_dump);

	return 0;
}

static int exynos_lli_get_clk_info(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	clks->aclk_cpif_200 = devm_clk_get(lli->dev, "aclk_cpif_200");
	/* To gate/ungate clocks */
	clks->gate_cpifnm_200 = devm_clk_get(lli->dev, "gate_cpifnm_200");
	clks->gate_lli_svc_loc = devm_clk_get(lli->dev, "gate_lli_svc_loc");
	clks->gate_lli_svc_rem = devm_clk_get(lli->dev, "gate_lli_svc_rem");
	clks->gate_lli_ll_init = devm_clk_get(lli->dev, "gate_lli_ll_init");
	clks->gate_lli_be_init = devm_clk_get(lli->dev, "gate_lli_be_init");
	clks->gate_lli_cmn_cfg = devm_clk_get(lli->dev, "gate_lli_cmn_cfg");
	clks->gate_lli_tx0_cfg = devm_clk_get(lli->dev, "gate_lli_tx0_cfg");
	clks->gate_lli_rx0_cfg = devm_clk_get(lli->dev, "gate_lli_rx0_cfg");
	clks->gate_lli_tx0_symbol = devm_clk_get(lli->dev, "gate_lli_tx0_symbol");
	clks->gate_lli_rx0_symbol = devm_clk_get(lli->dev, "gate_lli_rx0_symbol");

	/* For mux selection of clocks */
	clks->mout_phyclk_lli_tx0_symbol_user = devm_clk_get(lli->dev,
			"mout_phyclk_lli_tx0_symbol_user");
	clks->phyclk_lli_tx0_symbol = devm_clk_get(lli->dev,
			"phyclk_lli_tx0_symbol");
	clks->mout_phyclk_lli_rx0_symbol_user = devm_clk_get(lli->dev,
			"mout_phyclk_lli_rx0_symbol_user");
	clks->phyclk_lli_rx0_symbol = devm_clk_get(lli->dev,
			"phyclk_lli_rx0_symbol");

	if (IS_ERR(clks->aclk_cpif_200) ||
		IS_ERR(clks->gate_cpifnm_200) ||
		IS_ERR(clks->gate_lli_svc_loc) ||
		IS_ERR(clks->gate_lli_svc_rem) ||
		IS_ERR(clks->gate_lli_ll_init) ||
		IS_ERR(clks->gate_lli_be_init) ||
		IS_ERR(clks->gate_lli_cmn_cfg) ||
		IS_ERR(clks->gate_lli_tx0_cfg) ||
		IS_ERR(clks->gate_lli_rx0_cfg) ||
		IS_ERR(clks->gate_lli_tx0_symbol) ||
		IS_ERR(clks->gate_lli_rx0_symbol) ||
		IS_ERR(clks->mout_phyclk_lli_tx0_symbol_user) ||
		IS_ERR(clks->phyclk_lli_tx0_symbol) ||
		IS_ERR(clks->mout_phyclk_lli_rx0_symbol_user) ||
		IS_ERR(clks->phyclk_lli_rx0_symbol)
	) {
		dev_err(lli->dev, "exynos_lli_get_clks - failed\n");
		return -ENODEV;
	}

	return 0;
}

static void exynos_lli_system_config(struct mipi_lli *lli)
{
	if (lli->sys_regs) {
		writel(SIG_INT_MASK0, lli->sys_regs + CPIF_LLI_SIG_INT_MASK0);
		writel(SIG_INT_MASK1, lli->sys_regs + CPIF_LLI_SIG_INT_MASK1);
	}
}

static int exynos_lli_init(struct mipi_lli *lli)
{
	/* LLI Interrupt enable */
	writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);
	writel(1, lli->regs + EXYNOS_DME_LLI_RESET);
	return 0;
}

static int exynos_lli_setting(struct mipi_lli *lli)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);
	u32 remap_addr;

	/* update lli_link_state as reset */
	if (atomic_read(&lli->state) & LLI_WAITFORMOUNT)
		atomic_set(&lli->state, LLI_WAITFORMOUNT);
	else
		atomic_set(&lli->state, LLI_UNMOUNTED);

	exynos_lli_system_config(lli);
	/* enable LLI_PHY_CONTROL */
	writel(1, lli->pmu_regs);

	/* set_system clk period */
	writel(SYSTEM_CLOCK_PERIOD, lli->regs + EXYNOS_PA_SYSTEM_CLK_PERIOD);
	/* Set DriveTactiveDuration */
	writel(0xf, lli->regs + EXYNOS_PA_DRIVE_TACTIVATE_DURATION);

	remap_addr = exynos_lli_cal_remap(lli->phy_addr, lli->shdmem_size);
	if (!remap_addr) {
		dev_err(lli->dev, "remap calculation error\n");
		return -EINVAL;
	}

	/* Un-set LL_INIT REMAP Address */
	writel(remap_addr, lli->regs + EXYNOS_IAL_LL_INIT_ADDR_REMAP);
	/* Set BE_INIT REMAP Address */
	writel(remap_addr, lli->regs + EXYNOS_IAL_BE_INIT_ADDR_REMAP);

	/* LLI Interrupt enable */
	writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

	/* Set BE TC enable */
	writel(0, lli->regs + EXYNOS_DME_BE_TC_DISABLE);
	/* Set Breaking point enable */
	writel(0, lli->regs + EXYNOS_PA_DBG_PA_BREAK_POINT_ENABLE);
	/* Set Error count enable */
	writel(1, lli->regs + EXYNOS_PA_PHIT_ERR_COUNT_ENABLE);
	/* Set Receive count enable */
	writel(1, lli->regs + EXYNOS_PA_PHIT_RECEIVE_COUNT_ENABLE);

	/* Set Acti Tx/Rx count enable */
	writel(1, lli->regs + EXYNOS_PA_TX_COUNT);
	writel(1, lli->regs + EXYNOS_PA_RX_COUNT);

	/*
	 Set Rx Latch
	 Edge and number of latches from M-RX to PA.
	 [0:7] where,
	 [0] : Rising edge. 1 cycle.
	 [1] : Rising edge. 2 cycles.
	 [2] : No latch (Bypass)
	 [3-7] : Reserved
	 */
	writel(2, lli->regs + EXYNOS_PA_PA_DBG_RX_LATCH_MODE);

	writel(0x40, lli->regs + EXYNOS_PA_NACK_RTT);
	writel(0x0, lli->regs + EXYNOS_PA_MK0_INSERTION_ENABLE);
#if defined(CONFIG_UMTS_MODEM_SS300) || defined(CONFIG_UMTS_MODEM_SS303)
	writel((128<<0) | (15<<8) | (1<<12), lli->regs + EXYNOS_PA_MK0_CONTROL);
#else
	writel((128<<0) | (1<<12), lli->regs + EXYNOS_PA_MK0_CONTROL);
#endif
	/* Set Scrambler enable */
	if (lli->modem_info.scrambler)
		writel(1, lli->regs + EXYNOS_PA_USR_SCRAMBLER_ENABLE);

	/* MPHY configuration */
	if (phy->init)
		phy->init(phy);

	if (phy->cmn_init) {
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
		phy->cmn_init(phy);
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	}

	if (phy->ovtm_init) {
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
		phy->ovtm_init(phy);
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	}
	/* Update PA configuration for MPHY standard attributes */
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);

	/* Set SNF FIFO for LL&BE */
	writel(((0x1F<<1) | 1), lli->regs + EXYNOS_IAL_LL_SNF_FIFO);
	writel(((0x1F<<1) | 1), lli->regs + EXYNOS_IAL_BE_SNF_FIFO);

	writel(0x1000, lli->regs + EXYNOS_PA_WORSTCASE_RTT);

	dev_err(lli->dev, "MIPI LLI is initialized\n");

	return 0;
}

static int exynos_lli_set_master(struct mipi_lli *lli, bool is_master)
{
	lli->is_master = is_master;

	writel((is_master << 1), lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);

	return 0;
}

static int exynos_lli_link_startup_mount(struct mipi_lli *lli)
{
	u32 regs;
	u32 ret = 0;

	if (lli->is_master) {
		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
		if (regs & LLI_MOUNTED) {
			pr_debug("LLI master already mounted\n");
			return ret;
		}

		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
		writel((0x1 << 2) | regs,
			lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	} else {
		ret = poll_bit_set(lli->regs + EXYNOS_DME_LLI_INTR_STATUS,
			     INTR_MPHY_HIBERN8_EXIT_DONE,
			     1000);
		if (ret) {
			dev_err(lli->dev, "HIBERN8 Exit Failed\n");
			return ret;
		}

		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
		writel((0x1 << 2) | regs,
			lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	}

	return ret;
}

static int exynos_lli_get_status(struct mipi_lli *lli)
{
	u32 regs;

	regs = readl(lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	return regs;
}

static int exynos_lli_send_signal(struct mipi_lli *lli, u32 cmd)
{
	int is_mounted = 0;
	if (atomic_read(&lli->state) == LLI_MOUNTED) {
		/* For unmount failed issue, we should check the
		   LLI_MOUNT_CTRL is cleared or not */
		is_mounted = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
		if (is_mounted & LLI_MOUNT_CTRL) {
#if defined(CONFIG_SOC_EXYNOS5430) && defined(CONFIG_UMTS_MODEM_SS300)
			writel(0, lli->sys_regs + CPIF_LLI_SIG_IN0);
			udelay(10);
			writel(cmd, lli->sys_regs + CPIF_LLI_SIG_IN0);
#elif defined(CONFIG_LTE_MODEM_XMM7260)
			writel(cmd, lli->remote_regs + EXYNOS_TL_SIGNAL_SET_LSB
					+ 0x20C);
#else
			writel(cmd, lli->remote_regs +
					EXYNOS_TL_SIGNAL_SET_LSB);
#endif
			return 0;
		}
	}

	dev_err(lli->dev, "LLI not mounted !! mnt_reg = %x", is_mounted);

	return -EIO;
}

static int exynos_lli_reset_signal(struct mipi_lli *lli)
{
	writel(0xFFFFFFFF, lli->regs + EXYNOS_TL_SIGNAL_CLR_LSB);
	writel(0xFFFFFFFF, lli->regs + EXYNOS_TL_SIGNAL_CLR_MSB);

	return 0;
}

static int exynos_lli_read_signal(struct mipi_lli *lli)
{
	u32 intr_lsb, intr_msb;

	intr_lsb = readl(lli->regs + EXYNOS_TL_SIGNAL_STATUS_LSB);
	intr_msb = readl(lli->regs + EXYNOS_TL_SIGNAL_STATUS_MSB);

	if (intr_lsb)
		writel(intr_lsb, lli->regs + EXYNOS_TL_SIGNAL_CLR_LSB);
	if (intr_msb)
		writel(intr_msb, lli->regs + EXYNOS_TL_SIGNAL_CLR_MSB);

	/* TODO: change to dev_dbg */
	dev_dbg(lli->dev, "LSB = %x, MSB = %x\n", intr_lsb, intr_msb);

	return intr_lsb;
}

static int exynos_lli_clock_init(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if(!clks)
		return -EINVAL;

	clk_set_parent(clks->mout_phyclk_lli_tx0_symbol_user,
			clks->phyclk_lli_tx0_symbol);
	clk_set_parent(clks->mout_phyclk_lli_rx0_symbol_user,
			clks->phyclk_lli_rx0_symbol);

	return 0;
}

static int exynos_lli_clock_gating(struct mipi_lli *lli, int is_gating)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if(!clks)
		return -EINVAL;

	if (is_gating){
		clk_disable_unprepare(clks->gate_lli_rx0_symbol);
		clk_disable_unprepare(clks->gate_lli_tx0_symbol);
		clk_disable_unprepare(clks->gate_lli_rx0_cfg);
		clk_disable_unprepare(clks->gate_lli_tx0_cfg);
		clk_disable_unprepare(clks->gate_lli_cmn_cfg);
		clk_disable_unprepare(clks->gate_lli_be_init);
		clk_disable_unprepare(clks->gate_lli_ll_init);
		clk_disable_unprepare(clks->gate_lli_svc_rem);
		clk_disable_unprepare(clks->gate_lli_svc_loc);
		clk_disable_unprepare(clks->gate_cpifnm_200);
		/* it doesn't gate/ungate aclk_cpif_200
		   clk_disable_unprepare(clks->aclk_cpif_200);
		 */
	} else {
		/* it doesn't gate/ungate aclk_cpif_200
		   clk_prepare_enable(clks->aclk_cpif_200);
		 */
		if (clks->gate_cpifnm_200->enable_count < 1)
			clk_prepare_enable(clks->gate_cpifnm_200);
		if (clks->gate_lli_svc_loc->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_svc_loc);
		if (clks->gate_lli_svc_rem->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_svc_rem);
		if (clks->gate_lli_ll_init->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_ll_init);
		if (clks->gate_lli_be_init->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_be_init);
		if (clks->gate_lli_cmn_cfg->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_cmn_cfg);
		if (clks->gate_lli_tx0_cfg->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_tx0_cfg);
		if (clks->gate_lli_rx0_cfg->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_rx0_cfg);
		if (clks->gate_lli_tx0_symbol->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_tx0_symbol);
		if (clks->gate_lli_rx0_symbol->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_rx0_symbol);
	}

	return 0;
}

static int exynos_lli_intr_enable(struct mipi_lli *lli)
{
	u32 lli_intr = 0;

	if (mipi_lli_suspended())
		return -1;

	lli_intr = readl(lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

	if (lli_intr != 0x3FFFF)
		writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
/* exynos_lli_suspend must call by modem_if */
static int exynos_lli_suspend(struct mipi_lli *lli)
{
	/* masking all of lli interrupts */
	exynos_lli_system_config(lli);
	writel(0x0, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);
	/* clearing all of lli sideband signal */
	exynos_lli_reset_signal(lli);
	exynos_lli_clock_gating(lli, true);
	/* disable LLI_PHY_CONTROL */
	writel(0, lli->pmu_regs);

	lli->is_suspended = true;

	return 0;
}

/* exynos_lli_resume must call by modem_if */
static int exynos_lli_resume(struct mipi_lli *lli)
{
	/* enable LLI_PHY_CONTROL */
	writel(1, lli->pmu_regs);

	/* re-init clock mux selection for LLI & M-PHY */
	exynos_lli_clock_init(lli);
	exynos_lli_clock_gating(lli, false);

	/* re-init all of lli resource */
	exynos_lli_init(lli);

	lli->is_suspended = false;

	return 0;
}
#else
#define exynos_lli_suspend NULL
#define exynos_lli_resume NULL
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_RUNTIME
static int exynos_lli_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	lli->is_runtime_suspended = true;

	return 0;
}

static int exynos_lli_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	lli->is_runtime_suspended = false;

	return 0;
}
#else
#define exynos_lli_runtime_suspend NULL
#define exynos_lli_runtime_resume NULL
#endif /* CONFIG_PM_RUNTIME */

static void exynos_mipi_lli_set_automode(struct mipi_lli *lli, bool is_auto)
{
	if (is_auto)
		writel(CSA_AUTO_MODE, lli->regs + EXYNOS_PA_CSA_PA_SET);
	else
		writel(CSA_AUTO_MODE, lli->regs + EXYNOS_PA_CSA_PA_CLR);
}

static int exynos_lli_loopback_test(struct mipi_lli *lli)
{
	int uval = 0;
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);

	/* enable LLI_PHY_CONTROL */
	writel(1, lli->pmu_regs);

	/* software reset */
	writel(1, lli->regs + EXYNOS_DME_LLI_RESET);

	/* to set TxH8 as H8_EXIT : 0x2b -> 0 */
	writel(0x0, phy->loc_regs + PHY_TX_HIBERN8_CONTROL(0));
	/* to set RxH8 as H8_EXIT : 0xa7 -> 0 */
	writel(0x0, phy->loc_regs + PHY_RX_ENTER_HIBERN8(0));

	writel(0x1, phy->loc_regs + PHY_TX_MODE(0));
	writel(0x1, phy->loc_regs + PHY_RX_MODE(0));

	writel(0x0, phy->loc_regs + PHY_TX_LCC_ENABLE(0));
	/* 0x10F24128 sets 0xFFFFFFFF to
	   LLI_Set_Config_update_All(LLI_SVC_BASE_LOCAL); */
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);

	writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	/* RX setting */
	writel(0x75, phy->loc_regs + (0x0a*4)); /* Internal Serial loopback */
	writel(0x01, phy->loc_regs + (0x16*4)); /* Align Enable */

	writel(0x01, phy->loc_regs + (0x0b*4)); /* user pattern ALL 0 */
	writel(0x0F, phy->loc_regs + (0x0C*4));
	writel(0xFF, phy->loc_regs + (0x0D*4));
	writel(0xFF, phy->loc_regs + (0x0E*4));

	writel(0x2b, phy->loc_regs + (0x2f*4));
	writel(0x60, phy->loc_regs + (0x1a*4));
	writel(0x14, phy->loc_regs + (0x29*4)); /* failed LB test on ATE */
	writel(0xC0, phy->loc_regs + (0x2E*4));

	/* TX setting */
	writel(0x03, phy->loc_regs + (0x32*4));
	writel(0x25, phy->loc_regs + (0x78*4)); /* Internal serial loopback */
	writel(0x01, phy->loc_regs + (0x79*4)); /* user ALL 0 */

	writel(0x0F, phy->loc_regs + (0x7A*4));
	writel(0xFF, phy->loc_regs + (0x7B*4));
	writel(0xFF, phy->loc_regs + (0x7C*4));

	writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);

	mdelay(100);
	/* LLI_Set_RxBYpass as BY_PASS_ENTER */
	writel(0x1, phy->loc_regs + (0x2e*4));
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);
	/* LLI_Set_TxBYpass as BY_PASS_ENTER */
	writel(0x1, phy->loc_regs + (0xa8*4));
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);

	mdelay(200);

	writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	uval = readl(phy->loc_regs + (0x23*4));
	dev_err(lli->dev, "LB error : 0x%x\n", uval);
	if (uval == 0x24)
		dev_err(lli->dev, "PWM loopbacktest is Passed!!");
	else
		dev_err(lli->dev, "PWM loopbacktest is Failed!!");

	writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);

	writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	uval = readl(phy->loc_regs + (0x26*4)); /* o_pll_lock[7] */
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);

	dev_err(lli->dev, "uval : %d\n", uval);

	return 0;
}

const struct lli_driver exynos_lli_driver = {
	.init = exynos_lli_init,
	.set_master = exynos_lli_set_master,
	.link_startup_mount = exynos_lli_link_startup_mount,
	.get_status = exynos_lli_get_status,
	.send_signal = exynos_lli_send_signal,
	.reset_signal = exynos_lli_reset_signal,
	.read_signal = exynos_lli_read_signal,
	.loopback_test = exynos_lli_loopback_test,
	.debug_info = exynos_lli_debug_info,
	.intr_enable = exynos_lli_intr_enable,
	.suspend = exynos_lli_suspend,
	.resume = exynos_lli_resume,
};

static irqreturn_t exynos_mipi_lli_irq(int irq, void *_dev)
{
	struct device *dev = _dev;
	struct mipi_lli *lli = dev_get_drvdata(dev);
	static int pa_err_cnt = 0;
	static int roe_cnt = 0;
	static int mnt_cnt = 0;
	static int mnt_fail_cnt = 0;
	int status;
	struct exynos_mphy *phy;

	phy = dev_get_drvdata(lli->mphy);
	status = readl(lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	if (status & INTR_SW_RESET_DONE) {
		dev_err(dev, "SW_RESET_DONE ++\n");
		exynos_lli_setting(lli);

		if (status & INTR_RESET_ON_ERROR_DETECTED) {
			dev_err(dev, "LLI is wating for mount "
					"after LINE-RESET..\n");
			atomic_set(&lli->state, LLI_WAITFORMOUNT);
		}

	}

	if (status & INTR_MPHY_HIBERN8_EXIT_DONE) {
		dev_info(dev, "HIBERN8_EXIT_DONE: rx=%x, tx=%x\n",
				readl(phy->loc_regs + PHY_RX_FSM_STATE(0)),
				readl(phy->loc_regs + PHY_TX_FSM_STATE(0)));
		mdelay(1);
		writel(LLI_MOUNT_CTRL, lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	}

	if (status & INTR_MPHY_HIBERN8_ENTER_DONE)
		dev_info(dev, "HIBERN8_ENTER_DONE\n");

	if (status & INTR_PA_PLU_DETECTED)
		dev_info(dev, "PLU_DETECT\n");

	if (status & INTR_PA_PLU_DONE) {
		dev_info(dev, "PLU_DONE\n");

		if (lli->modem_info.automode)
			exynos_mipi_lli_set_automode(lli, true);
	}

	if ((status & INTR_RESET_ON_ERROR_DETECTED)) {
		dev_err(dev, "Error detected ++ roe_cnt = %d\n", ++roe_cnt);
	}

	if (status & INTR_RESET_ON_ERROR_SENT) {
		dev_err(dev, "Error sent ++ roe_cnt = %d\n", ++roe_cnt);
		writel(1, lli->regs + EXYNOS_DME_LLI_RESET);

		return IRQ_HANDLED;
	}

	if (status & INTR_PA_ERROR_INDICATION) {
		dev_err_ratelimited(dev, "PA_REASON %x\n",
				readl(lli->regs + EXYNOS_DME_LLI_PA_INTR_REASON)
				);
		pa_err_cnt++;
	}

	if (status & INTR_DL_ERROR_INDICATION) {
		dev_err(dev, "DL_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_DL_INTR_REASON));
	}

	if (status & INTR_TL_ERROR_INDICATION) {
		dev_err(dev, "TL_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_TL_INTR_REASON));
	}

	if (status & INTR_IAL_ERROR_INDICATION) {
		dev_err(dev, "IAL_REASON0 %x, REASON1 %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_IAL_INTR_REASON0),
			readl(lli->regs + EXYNOS_DME_LLI_IAL_INTR_REASON1));
	}

	if (status & INTR_LLI_MOUNT_DONE) {
		static bool is_first = true;
		int credit = 0;
		int rx_fsm_state, tx_fsm_state, afc_val, csa_status;

		rx_fsm_state = readl(phy->loc_regs + PHY_RX_FSM_STATE(0));
		tx_fsm_state = readl(phy->loc_regs + PHY_TX_FSM_STATE(0));
		csa_status = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
		credit = readl(lli->regs + EXYNOS_DL_DBG_TX_CREDTIS);
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
		afc_val = readl(phy->loc_regs + (0x27*4));
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);

		if (is_first) {
			phy->afc_val = afc_val;
			is_first = false;
		}

		dev_err(dev, "rx=%x, tx=%x, afc=%x, status=%x, pa_err=%x\n"
				,rx_fsm_state, tx_fsm_state, afc_val,
				csa_status, pa_err_cnt);

		if (!credit) {
			u32 udelay = 0;
			for (udelay = 0 ; udelay < 200 ; udelay++) {
				udelay(1);
				credit = readl(lli->regs + EXYNOS_DL_DBG_TX_CREDTIS);

				if (credit)
					break;
			}
			dev_err(dev, "waiting %dus for CREDITS: tx=%x, rx=%x\n",
					udelay,
					readl(phy->loc_regs + PHY_RX_FSM_STATE(0)),
					readl(phy->loc_regs + PHY_TX_FSM_STATE(0)));

			if (!credit) {
				dev_err(dev, "ERR: Mount failed : %d\n",
						++mnt_fail_cnt);
				mipi_lli_debug_info();
				writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);
				dev_err(dev, "DUMP: ok:%d fail:%d roe:%d",
						mnt_cnt, mnt_fail_cnt, roe_cnt);
				return IRQ_HANDLED;
			}
		}

		atomic_set(&lli->state, LLI_MOUNTED);
		dev_err(dev, "Mount : ok:%d fail:%d roe:%d pa_err:%d\n",
				++mnt_cnt, mnt_fail_cnt, roe_cnt, pa_err_cnt);
	}

	if (status & INTR_LLI_UNMOUNT_DONE) {
		atomic_set(&lli->state, LLI_UNMOUNTED);
		writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);
		writel(1, lli->regs + EXYNOS_DME_LLI_RESET);
		dev_err(dev, "Unmount\n");
		return IRQ_HANDLED;
	}

	writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	return IRQ_HANDLED;
}

int mipi_lli_get_setting(struct mipi_lli *lli)
{
	struct device_node *lli_node = lli->dev->of_node;
	struct device_node *modem_node;
	const char *modem_name;
	const __be32 *prop;

	modem_name = of_get_property(lli_node, "modem-name", NULL);
	if (!modem_name) {
		dev_err(lli->dev, "parsing err : modem-name node\n");
		goto parsing_err;
	}
	modem_node = of_get_child_by_name(lli_node, "modems");
	if (!modem_node) {
		dev_err(lli->dev, "parsing err : modems node\n");
		goto parsing_err;
	}
	modem_node = of_get_child_by_name(modem_node, modem_name);
	if (!modem_node) {
		dev_err(lli->dev, "parsing err : modem node\n");
		goto parsing_err;
	}

	lli->modem_info.name = devm_kzalloc(lli->dev, strlen(modem_name),
			GFP_KERNEL);
	strncpy(lli->modem_info.name, modem_name, strlen(modem_name));

	prop = of_get_property(modem_node, "scrambler", NULL);
	if (prop)
		lli->modem_info.scrambler = be32_to_cpup(prop) ? true : false;
	else
		lli->modem_info.scrambler = false;

	prop = of_get_property(modem_node, "automode", NULL);
	if (prop)
		lli->modem_info.automode = be32_to_cpup(prop) ? true : false;
	else
		lli->modem_info.scrambler = false;

parsing_err:
	dev_err(lli->dev, "modem_name:%s, scrambler:%d, automode:%d\n",
			modem_name,
			lli->modem_info.scrambler,
			lli->modem_info.automode);
	return 0;
}

static int exynos_mipi_lli_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct mipi_lli *lli;
	struct resource *res;
	struct clk *dout_aclk_cpif_200;
	struct clk *dout_mif_pre;
	void __iomem *regs, *remote_regs, *sysregs, *pmuregs;
	int irq, irq_sig;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find register resource 0\n");
		return -ENXIO;
	}

	regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "cannot find register resource 1\n");
		return -ENXIO;
	}

	remote_regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "cannot find register resource 2\n");
		return -ENXIO;
	}

	sysregs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(dev, "cannot find register resource 3\n");
		return -ENXIO;
	}

	pmuregs = devm_request_and_ioremap(dev, res);
	if (!pmuregs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	/* Request LLI IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return -EBUSY;
	}

	/* Request Signal IRQ */
	irq_sig = platform_get_irq(pdev, 1);
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return -EBUSY;
	}

	ret = mipi_lli_add_driver(dev, &exynos_lli_driver, irq_sig);
	if (ret < 0)
		return ret;

	lli = dev_get_drvdata(dev);

	lli->regs = regs;
	lli->remote_regs = remote_regs;
	lli->sys_regs = sysregs;
	lli->pmu_regs = pmuregs;
	lli->is_master = false;
	INIT_WORK(&lli->wq_print_dump, exynos_lli_print_dump);

	mipi_lli_get_setting(lli);

	ret = request_irq(irq, exynos_mipi_lli_irq, 0, dev_name(dev), dev);
	if (ret < 0)
		return ret;

	if (node) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret)
			dev_err(dev, "failed to add mphy\n");
	} else {
		dev_err(dev, "no device node, failed to add mphy\n");
		ret = -ENODEV;
	}

	lli->mphy = exynos_get_mphy();
	if (!lli->mphy) {
		dev_err(dev, "failed get mphy\n");
		return -ENODEV;
	}

	exynos_lli_get_clk_info(lli);

	/* init clock mux selection for LLI & M-PHY */
	exynos_lli_clock_init(lli);

	/* When getting clock structure data at first, there is no way to
	   know whether the value of clock is enabled or disabled.
	   therefore, at first time, set the enable the clock value to
	   inform that clock is enabled */
	exynos_lli_clock_gating(lli, false);

	dout_aclk_cpif_200 = devm_clk_get(lli->dev, "dout_aclk_cpif_200");
	dout_mif_pre = devm_clk_get(lli->dev, "dout_mif_pre");

	ret = clk_set_rate(dout_aclk_cpif_200, 100000000);
	if (!ret)
		dev_err(dev, "failed clk_set_rate on dout_aclk_cpif_200 to 100000000");

	dev_err(dev, "dout_aclk_cpif_200 = %ld\n", dout_aclk_cpif_200->rate);
	dev_err(dev, "dout_mif_pre= %ld\n", dout_mif_pre->rate);

	dev_info(dev, "Registered MIPI-LLI interface\n");

	return ret;
}

static int exynos_mipi_lli_remove(struct platform_device *pdev)
{
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	mipi_lli_remove_driver(lli);

	return 0;
}

static const struct dev_pm_ops exynos_mipi_lli_pm = {
	/* suspend, resume functions must call by modem_if.
	SET_SYSTEM_SLEEP_PM_OPS(exynos_lli_suspend, exynos_lli_resume)
	*/
	SET_RUNTIME_PM_OPS(exynos_lli_runtime_suspend,
			   exynos_lli_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_mipi_lli_dt_match[] = {
	{
		.compatible = "samsung,exynos-mipi-lli"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mipi_lli_dt_match);
#endif

static struct platform_driver exynos_mipi_lli_driver = {
	.probe = exynos_mipi_lli_probe,
	.remove = exynos_mipi_lli_remove,
	.driver = {
		.name = "exynos-mipi-lli",
		.owner = THIS_MODULE,
		.pm = &exynos_mipi_lli_pm,
		.of_match_table = of_match_ptr(exynos_mipi_lli_dt_match),
	},
};

module_platform_driver(exynos_mipi_lli_driver);

MODULE_DESCRIPTION("Exynos MIPI LLI driver");
MODULE_AUTHOR("Yulgon Kim <yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
