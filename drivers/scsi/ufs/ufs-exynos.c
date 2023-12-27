/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include "ufs-cal-if.h"
#include "ufs-vs-mmio.h"
#include "ufs-vs-regs.h"
#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufshci.h"
#include "unipro.h"
#include "ufshcd-pltfrm.h"
#include "ufs_quirks.h"
#include "ufs-exynos.h"
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/bitfield.h>
#include <linux/pinctrl/consumer.h>
//#include <linux/soc/samsung/exynos-soc.h>
#include <trace/hooks/ufshcd.h>
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include <soc/samsung/exynos-smc.h>

#include <trace/events/ufs_exynos_perf.h>

#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
#include <soc/samsung/exynos/debug-snapshot.h>
#endif

#include "../scsi_priv.h"
/* Performance */
#include "ufs-exynos-perf.h"

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
#include "ufs-sec-feature.h"
#endif

#define LDO_DISCHARGE_GUARANTEE	15
#define IS_C_STATE_ON(h) ((h)->c_state == C_ON)
#define PRINT_STATES(h)							\
	dev_err((h)->dev, "%s: prev h_state %d, cur c_state %d\n",	\
				__func__, (h)->h_state, (h)->c_state);

static struct exynos_ufs *ufs_host_backup;

typedef enum {
	UFS_S_MON_LV1 = (1 << 0),
	UFS_S_MON_LV2 = (1 << 1),
} exynos_ufs_mon;

/* Functions to map registers or to something by other modules */
static void ufs_udelay(u32 n)
{
	udelay(n);
}

static inline void ufs_map_vs_regions(struct exynos_ufs *ufs)
{
	ufs->handle.hci = ufs->reg_hci;
	ufs->handle.ufsp = ufs->reg_ufsp;
	ufs->handle.unipro = ufs->reg_unipro;
	ufs->handle.pma = ufs->reg_phy;
	ufs->handle.cport = ufs->reg_cport;
	ufs->handle.pcs = ufs->reg_pcs;
	ufs->handle.udelay = ufs_udelay;
}

/* Helper for UFS CAL interface */
int ufs_call_cal(struct exynos_ufs *ufs, void *func)
{
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret;
	u32 reg;
	cal_if_func fn;

	/* Enable MPHY APB */
	reg = hci_readl(handle, HCI_CLKSTOP_CTRL);
	reg |= (MPHY_APBCLK_STOP | UNIPRO_MCLK_STOP);
	hci_writel(handle, reg, HCI_CLKSTOP_CTRL);

	reg = hci_readl(handle, HCI_FORCE_HCS);
	reg &= ~UNIPRO_MCLK_STOP_EN;
	hci_writel(handle, reg & ~MPHY_APBCLK_STOP_EN, HCI_FORCE_HCS);

	fn = (cal_if_func)func;
	ret = fn(p);
	if (ret) {
		dev_err(ufs->dev, "%s: %d\n", __func__, ret);
		ret = -EPERM;
	}

	/* Disable MPHY APB */
	hci_writel(handle, reg | MPHY_APBCLK_STOP_EN, HCI_FORCE_HCS);

	return ret;

}

static inline void __pm_qos_ctrl(struct exynos_ufs *ufs, bool op)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	struct uic_pwr_mode *pmd = &ufs->device_pmd_parm;
	s32 int_value, val;

	int_value = (pmd->gear == UFS_HS_G5) ? ufs->pm_qos_gear5_int :
		ufs->pm_qos_int_value;
	val = (op) ? int_value : 0;

	exynos_pm_qos_update_request(&ufs->pm_qos_int, val);
#endif
}

static inline void __sicd_ctrl(struct exynos_ufs *ufs, bool op)
{
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	/*
	 * 0 : block to enter system idle state
	 * 1 : allow to use system idle state
	 */
	exynos_update_ip_idle_status(ufs->idle_ip_index, (op) ? 0 : 1);
#endif
}

static void exynos_ufs_update_active_lanes(struct exynos_ufs *ufs)
{
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 active_tx_lane = 0;
	u32 active_rx_lane = 0;

	active_tx_lane = unipro_readl(handle, UNIP_PA_ACTIVETXDATALENS);
	active_rx_lane = unipro_readl(handle, UNIP_PA_ACTIVERXDATALENS);

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware
	 */
	if (active_rx_lane != active_tx_lane)
		goto out;

	if (active_rx_lane == 0 || active_tx_lane == 0)
		goto out;

	p->active_rx_lane = active_rx_lane;
	p->active_tx_lane = active_tx_lane;

	dev_info(ufs->dev,
		"PA_ActiveTxDataLanes(%d), PA_ActiveRxDataLanes(%d)\n",
		active_tx_lane, active_rx_lane);

	return;
out:
	dev_err(ufs->dev, "%s: invalid active lanes. rx=%d, tx=%d\n", __func__,
		active_rx_lane, active_tx_lane);
	WARN_ON(1);

	return;
}

static void exynos_ufs_update_max_gear(struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pmd = &ufs->hci_pmd_parm;
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 max_rx_hs_gear = 0;

	max_rx_hs_gear = unipro_readl(handle, UNIP_PA_MAXRXHSGEAR);

	p->max_gear = min_t(u8, max_rx_hs_gear, pmd->gear);
	pmd->gear = p->max_gear;

	dev_info(ufs->dev, "max_gear(%d), PA_MaxRxHSGear(%d)\n",
			p->max_gear, max_rx_hs_gear);

	/* set for sysfs */
	ufs->params[UFS_S_PARAM_EOM_SZ] =
			EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX *
			ufs_s_eom_repeat[ufs->cal_param.max_gear];
}

static inline void exynos_ufs_ctrl_phy_pwr(struct exynos_ufs *ufs, bool en)
{
	struct ext_cxt *cxt = &ufs->cxt_phy_iso;

	exynos_pmu_update(cxt->offset, cxt->mask, (en ? 1 : 0) ? cxt->val : 0);
}

static inline void __thaw_cport_logger(struct ufs_vs_handle *handle)
{
	hci_writel(handle, CPORT_LOG_TYPE, HCI_PH_CPORT_LOG_CFG);
	hci_writel(handle, CPORT_LOG_EN, HCI_PH_CPORT_LOG_CTRL);
}

static inline void __freeze_cport_logger(struct ufs_vs_handle *handle)
{
	hci_writel(handle, ~CPORT_LOG_EN, HCI_PH_CPORT_LOG_CTRL);
}

int exynos_ufs_check_ah8_fsm_state(struct ufs_hba *hba, u32 state)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int retry = 50;
	u32 reg;

	if (ufs->ah8_ahit == 0)
		return 0;

	while (retry--) {
		reg = hci_readl(handle, HCI_AH8_STATE);

		if (reg & HCI_AH8_STATE_ERROR)
			goto out;

		if (reg & state)
			break;

		usleep_range(1000, 1100);
	}

	if (!retry)
		goto out;

	return 0;
out:
	dev_err(hba->dev,
			"%s: Error ah8 state: req_state: %08X, reg_val: %08X\n",
			__func__, state, reg);

	return -EINVAL;
}

/*
 * Exynos debugging main function
 */
static void exynos_ufs_dump_debug_info(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	/* start cs, not permit overlapped dump */
	if (test_and_set_bit(EXYNOS_UFS_BIT_DBG_DUMP, &ufs->flag))
		goto out;

	pr_info("%s: ah8_enter count: %d, ah8_exit count: %d\n", __func__,
			ufs->hibern8_enter_cnt, ufs->hibern8_exit_cnt);

	/* freeze cport logger */
	__freeze_cport_logger(handle);

	exynos_ufs_dump_info(hba, handle, ufs->dev);

	/* thaw cport logger */
	__thaw_cport_logger(handle);

	/* finish cs */
	clear_bit(EXYNOS_UFS_BIT_DBG_DUMP, &ufs->flag);

out:
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
	if (hba->saved_err & SYSTEM_BUS_FATAL_ERROR)
		dbg_snapshot_expire_watchdog();
#endif
#endif
	return;
}

inline void exynos_ufs_ctrl_auto_hci_clk(struct exynos_ufs *ufs, bool en)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg = 0;

	reg = hci_readl(handle, HCI_FORCE_HCS);

	if (en)
		reg |= HCI_CORECLK_STOP_EN;
	else
		reg &= ~HCI_CORECLK_STOP_EN;

	hci_writel(handle, reg, HCI_FORCE_HCS);
}

static inline void exynos_ufs_ctrl_clk(struct exynos_ufs *ufs, bool en)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg = 0;

	reg = hci_readl(handle, HCI_FORCE_HCS);

	if (en)
		reg |= CLK_STOP_CTRL_EN_ALL;
	else
		reg &= ~CLK_STOP_CTRL_EN_ALL;

	hci_writel(handle, reg, HCI_FORCE_HCS);
}

static inline void exynos_ufs_gate_clk(struct exynos_ufs *ufs, bool en)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg = 0;

	reg = hci_readl(handle, HCI_CLKSTOP_CTRL);

	if (en)
		reg |= CLK_STOP_ALL;
	else
		reg &= ~CLK_STOP_ALL;

	hci_writel(handle, reg, HCI_CLKSTOP_CTRL);
}

static void exynos_ufs_set_unipro_mclk(struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pmd = &ufs->hci_pmd_parm;
	int ret;
	unsigned long val;

	val = clk_get_rate(ufs->clk_unipro);

	if (val != ufs->mclk_rate) {
		ret = clk_set_rate(ufs->clk_unipro, ufs->mclk_rate);
		dev_info(ufs->dev, "previous mclk: %lu\n", val);
	}

	if (pmd->gear > UFS_HS_G4) {
		ret = clk_set_rate(ufs->clk_unipro, ufs->mclk_gear5);
		ufs->mclk_rate = (u32)clk_get_rate(ufs->clk_unipro);
	}

	dev_info(ufs->dev, "mclk: %lu\n", ufs->mclk_rate);
}

static void exynos_ufs_set_internal_timer(struct exynos_ufs *ufs)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	/* To make 40us */
	reg = hci_readl(handle, HCI_1US_TO_CNT_VAL);
	reg &= ~CNT_VAL_1US_MASK;
	reg |= (ufs->freq_for_1us_cntval & CNT_VAL_1US_MASK);
	hci_writel(handle, reg, HCI_1US_TO_CNT_VAL);

	/* IA_TICK_SEL : 1(1us_TO_CNT_VAL) */
	reg = hci_readl(handle, HCI_UFSHCI_V2P1_CTRL);
	reg |= IA_TICK_SEL;
	hci_writel(handle, reg, HCI_UFSHCI_V2P1_CTRL);
}

static void exynos_ufs_init_pmc_req(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{

	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *hci_pmd = &ufs->hci_pmd_parm;
	struct uic_pwr_mode *device_pmd = &ufs->device_pmd_parm;

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware.
	 * pwr_max comes from core driver, i.e. ufshcd. That keeps the number
	 * of connected lanes.
	 */
	if (pwr_max->lane_rx != pwr_max->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes. rx=%d, tx=%d\n",
			__func__, pwr_max->lane_rx, pwr_max->lane_tx);
		WARN_ON(1);
	}

	/* gear change parameters to core driver */
	if (device_pmd->gear) {
		pwr_req->gear_rx = pwr_req->gear_tx = device_pmd->gear;
	} else {
		pwr_req->gear_rx
			= device_pmd->gear= min_t(u8, pwr_max->gear_rx, hci_pmd->gear);
		pwr_req->gear_tx
			= device_pmd->gear = min_t(u8, pwr_max->gear_tx, hci_pmd->gear);
	}

	pwr_req->lane_rx
		= device_pmd->lane = min_t(u8, pwr_max->lane_rx, hci_pmd->lane);
	pwr_req->lane_tx
		= device_pmd->lane = min_t(u8, pwr_max->lane_tx, hci_pmd->lane);
	pwr_req->pwr_rx = device_pmd->mode = hci_pmd->mode;
	pwr_req->pwr_tx = device_pmd->mode = hci_pmd->mode;
	pwr_req->hs_rate = device_pmd->hs_series = hci_pmd->hs_series;
}

static void exynos_ufs_dev_hw_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	/* bit[1] for resetn */
	hci_writel(handle, 0 << 0, HCI_GPIO_OUT);
	udelay(5);
	hci_writel(handle, 1 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_config_host(struct exynos_ufs *ufs)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg = 0;

	/* internal clock control */
	exynos_ufs_ctrl_auto_hci_clk(ufs, false);
	exynos_ufs_set_unipro_mclk(ufs);

	/* period for interrupt aggregation */
	exynos_ufs_set_internal_timer(ufs);

	/* misc HCI configurations */
	hci_writel(handle, 0xA, HCI_DATA_REORDER);
	hci_writel(handle, PRDT_PREFECT_EN | PRDT_SET_SIZE(12),
			HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(handle, PRDT_SET_SIZE(12), HCI_RXPRDT_ENTRY_SIZE);

	/* I_T_L_Q isn't used at the beginning */
	ufs->nexus = 0xFFFFFFFF;
	hci_writel(handle, ufs->nexus, HCI_UTRL_NEXUS_TYPE);
	hci_writel(handle, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);

	reg = hci_readl(handle, HCI_AXIDMA_RWDATA_BURST_LEN) &
					~BURST_LEN(0);
	hci_writel(handle, WLU_EN | BURST_LEN(3),
					HCI_AXIDMA_RWDATA_BURST_LEN);

	reg = hci_readl(handle, HCI_CLKMODE);
	reg |= (REF_CLK_MODE | PMA_CLKDIV_VAL);
	hci_writel(handle, reg, HCI_CLKMODE);

	/* complete after ind signal from UniPro */
	reg = hci_readl(handle, HCI_UFSHCI_V2P1_CTRL);
	reg |= UIC_CMD_COMPLETE_SEL;
	hci_writel(handle, reg, HCI_UFSHCI_V2P1_CTRL);

	/*
	 * enable HWACG control by IOP
	 *
	 * default value 1->0 at KC.
	 * always "0"(controlled by UFS_ACG_DISABLE)
	 */
	reg = hci_readl(handle, HCI_IOP_ACG_DISABLE);
	hci_writel(handle, reg & (~HCI_IOP_ACG_DISABLE_EN),
			HCI_IOP_ACG_DISABLE);
}

static int exynos_ufs_config_externals(struct exynos_ufs *ufs)
{
	struct ext_cxt *cxt;
	int ret = 0;

	/* PHY isolation bypass */
	exynos_ufs_ctrl_phy_pwr(ufs, true);

	if (ufs->is_dma_coherent == 0)
		goto out;

	if (IS_ERR_OR_NULL(ufs->regmap_sys)) {
		dev_err(ufs->dev, "Fail to access regmap\n");
		goto out;
	}

	cxt = &ufs->cxt_iocc;
	regmap_update_bits(ufs->regmap_sys, cxt->offset, cxt->mask, cxt->val);

	if (ufs->always_on) {
		u32 reg;

		/* enable PAD retention */
		cxt = &ufs->cxt_pad_ret;
		exynos_pmu_read(cxt->offset, &reg);
		reg |= cxt->mask;
		exynos_pmu_write(cxt->offset, reg);
	}
out:
	return ret;
}

static int exynos_ufs_get_clks(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;

	if (!head || list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		/*
		 * get clock with an order listed in device tree
		 *
		 * hci, unipro
		 */
		if (IS_ERR(clki->clk))
			goto out;

		if (!strcmp(clki->name, "GATE_UFS_EMBD")) {
			ufs->clk_hci = clki->clk;
		} else if (!strcmp(clki->name, "UFS_EMBD")) {
			ufs->clk_unipro = clki->clk;
			ufs->mclk_rate = (u32)clk_get_rate(ufs->clk_unipro);
		} else {
			dev_err(ufs->dev, "%s: Not defined clk!!!\n", __func__);
		}
	}
out:
	if (!ufs->clk_hci || !ufs->clk_unipro)
		return -EINVAL;

	return 0;
}

static void exynos_ufs_set_features(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* caps */
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	hba->caps = UFSHCD_CAP_CLK_GATING;
#else
	hba->caps = UFSHCD_CAP_WB_EN | UFSHCD_CAP_CLK_GATING;
#endif
	if (ufs->ah8_ahit == 0)
		hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;


	/* quirks of common driver */
	hba->quirks = UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR |
			UFSHCD_QUIRK_ALIGN_SG_WITH_PAGE_SIZE |
			UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL |
			UFSHCD_QUIRK_SKIP_DEF_UNIPRO_TIMEOUT_SETTING;
}

/*
 * Exynos-specific callback functions
 */
static int exynos_ufs_init(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	/* refer to hba */
	ufs->hba = hba;

	/* configure externals */
	ret = exynos_ufs_config_externals(ufs);
	if (ret)
		return ret;

	/* to read standard hci registers */
	ufs->handle.std = hba->mmio_base;

	/* get some clock sources and debug infomation structures */
	ret = exynos_ufs_get_clks(hba);
	if (ret)
		return ret;

	/* set features, such as caps or quirks */
	exynos_ufs_set_features(hba);

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_adjust_caps_quirks(hba);
#endif

	exynos_ufs_fmp_init(hba);

	exynos_ufs_srpmb_config(hba);

	/* init io perf stat, need an identifier later */
	if (!ufs_perf_init(&ufs->perf, ufs->hba))
		dev_err(ufs->dev, "Not enable UFS performance mode\n");

	hba->ahit = ufs->ah8_ahit;

	return 0;
}

static void __requeue_after_reset(struct ufs_hba *hba)
{
	int index;
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *cmd;
	unsigned long completed_reqs = hba->outstanding_reqs;

	for_each_set_bit(index, &completed_reqs, hba->nutrs) {
		if (!test_and_clear_bit(index, &hba->outstanding_reqs))
			continue;
		lrbp = &hba->lrb[index];
		lrbp->compl_time_stamp = ktime_get();
		cmd = lrbp->cmd;
		if (!cmd)
			return;
		trace_android_vh_ufs_compl_command(hba, lrbp);
		scsi_dma_unmap(cmd);
		cmd->result = DID_REQUEUE << 16;
		pr_err("%s: tag %d requeued\n", __func__, index);
		ufshcd_crypto_clear_prdt(hba, lrbp);
		lrbp->cmd = NULL;
		cmd->scsi_done(cmd);
		ufshcd_release(hba);
	}

	pr_info("%s: clk_gating.active_reqs: %d\n", __func__, hba->clk_gating.active_reqs);
}

static void hibern8_enter_stat(struct exynos_ufs *ufs)
{
	u32 upmcrs;

	/*
	 * hibern8 enter results are comprised of upmcrs and uic result,
	 * but you may not get the uic result because an interrupt
	 * for ah8 error is raised and ISR handles this before this is called.
	 * Honestly, upmcrs may also be the same case because UFS driver
	 * considers ah8 error as fatal and host reset is asserted subsequently.
	 * So you don't trust this return value.
	 */
	upmcrs = __get_upmcrs(ufs);
	trace_ufshcd_profile_hibern8(dev_name(ufs->dev), "enter", 0, upmcrs);
	ufs->hibern8_enter_cnt++;
	ufs->hibern8_state = UFS_STATE_AH8;
}

static void hibern8_exit_stat(struct exynos_ufs *ufs)
{

	/*
	 * this is called before actual hibern8 exit,
	 * so return value is meaningless
	 */
	trace_ufshcd_profile_hibern8(dev_name(ufs->dev), "exit", 0, 0);
	ufs->hibern8_exit_cnt++;
	ufs->hibern8_state = UFS_STATE_IDLE;
}

static inline bool exynos_is_ufs_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int retry = 10;
	u32 reg;

	/* host reset */
	hci_writel(handle, UFS_SW_RST_MASK, HCI_SW_RST);

	while (retry--) {
		reg = hci_readl(handle, HCI_SW_RST);
		if (reg & UFS_SW_RST_MASK)
			usleep_range(1000, 1100);
		else
			return 0;
	}

	return -EAGAIN;
}

static inline void exynos_enable_vendor_irq(struct exynos_ufs *ufs)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct device *dev = ufs->dev;
	u32 reg;
	bool need_preproc;

	/* report to IS.UE reg when UIC error happens during AH8 */
	reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IE);
	reg |= AH8_ERR_REPORT_UE;
	hci_writel(handle, reg, HCI_VENDOR_SPECIFIC_IE);

	/* check AH8_ERR_AT_PRE_PROC in vendor hook:
	   vh_ufs_mcq_has_oustanding_reqs at ufshcd_intr() */
	need_preproc = !!of_find_property(dev->of_node, "check-ah8-preproc", NULL);
	if (need_preproc) {
		reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IE);
		reg |= (AH8_ERR_AT_PRE_PROC | AH8_TIMEOUT);
		hci_writel(handle, reg, HCI_VENDOR_SPECIFIC_IE);

		hci_writel(handle, VS_INT_MERGE2PH_EN, HCI_VS_INT_MERGE2PH);
	}
}

static inline bool exynos_crypto_enable(struct ufs_hba *hba)
{
	if (!(hba->caps & UFSHCD_CAP_CRYPTO))
		return false;

	/* Reset might clear all keys, so reprogram all the keys. */
	if (hba->ksm.num_slots)
		blk_ksm_reprogram_all_keys(&hba->ksm);

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_CRYPTO_ENABLE)
		return false;

	return true;
}

static inline int exynos_is_ufs_present(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int retry = 50;
	u32 reg;

	while (retry--) {
		reg = std_readl(handle, REG_CONTROLLER_STATUS);
		if (reg & DEVICE_PRESENT)
			return 0;
		else
			usleep_range(1000, 1100);
	}

	return -ENXIO;
}

static inline int exynos_is_ufs_active(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	reg = std_readl(handle, REG_CONTROLLER_ENABLE);
	if (reg & CONTROLLER_ENABLE)
		return 0;
	else
		return -EBUSY;
}

static inline int exynos_ufs_start(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int retry = 50;
	int ret;
	u32 reg;

	reg = std_readl(handle, REG_CONTROLLER_ENABLE);
	reg |= CONTROLLER_ENABLE;

	if (exynos_crypto_enable(hba))
		reg |= CRYPTO_GENERAL_ENABLE;

	std_writel(handle, reg, REG_CONTROLLER_ENABLE);

	while (retry--) {
		ret = exynos_is_ufs_active(hba);
		if (ret)
			usleep_range(1000, 1100);
		else
			return 0;
	}

	if (ret)
		dev_err(hba->dev, "%s: Controller enable failed\n", __func__);

	return ret;
}

static int exynos_ufs_host_restore(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = 0;

	hci_writel(handle, DFES_ERR_EN | DFES_DEF_DL_ERRS,
			HCI_ERROR_EN_DL_LAYER);
	hci_writel(handle, DFES_ERR_EN | DFES_DEF_N_ERRS,
			HCI_ERROR_EN_N_LAYER);
	hci_writel(handle, DFES_ERR_EN | DFES_DEF_T_ERRS,
			HCI_ERROR_EN_T_LAYER);

	ret = ufshcd_make_hba_operational(hba);
	if (ret)
		dev_err(ufs->dev,
			"Host controller not ready to process requests[%d]\n", ret);

	return ret;
}

static int exynos_ufs_execute_hce(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	// HCI eanbel(HCE set to 1)
	if (exynos_is_ufs_active(hba))
		ufshcd_hba_stop(hba);

	ret = exynos_ufs_start(hba);
	if (ret)
		dev_err(hba->dev, "Controller enable failed[%d]\n", ret);

	ret = ufs_call_cal(ufs, ufs_cal_during_hce_enable);
	if (ret)
		goto out;

	// read DP
	ret = exynos_is_ufs_present(hba);
	if (ret)
		dev_err(hba->dev, "UFS device not present[%d]\n", ret);
out:
	return ret;
}

static int exynos_ufs_ctrl_gpio(struct exynos_ufs *ufs, int en)
{
	int ret;

	ret = pinctrl_select_state(ufs->pinctrl,
			en ? ufs->ufs_stat_wakeup : ufs->ufs_stat_sleep);
	if (ret)
		dev_err(ufs->dev, "Fail to control gpio(%d)\n", en);

	return ret;
}

static void exynos_ufs_init_host(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	if (exynos_is_ufs_reset(hba))
		goto out;

	/* performance */
	if (ufs->perf)
		ufs_perf_reset(ufs->perf);

	/* configure host */
	exynos_ufs_config_host(ufs);
	exynos_ufs_fmp_enable(hba);

	if (ufshcd_is_auto_hibern8_supported(hba))
		exynos_enable_vendor_irq(ufs);

	__requeue_after_reset(hba);

	ufs_sec_check_device_stuck();

	return;
out:
	dev_err(ufs->dev, "timeout host sw-reset\n");

	exynos_ufs_dump_info(hba, handle, ufs->dev);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
	dbg_snapshot_expire_watchdog();
#endif
#endif

	return;
}

static int exynos_ufs_setup_clocks(struct ufs_hba *hba, bool on,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *pmd = &ufs->device_pmd_parm;
	int ret = 0;

	if (on) {
		if (notify == PRE_CHANGE) {
			/* Clear for SICD */
			__sicd_ctrl(ufs, true);
			/* PM Qos hold for stability */
			if (pmd->gear != UFS_HS_G1)
				__pm_qos_ctrl(ufs, true);
		} else {
			hibern8_exit_stat(ufs);
			ufs->c_state = C_ON;
		}
	} else {
		if (notify == PRE_CHANGE) {
			ufs->c_state = C_OFF;
			hibern8_enter_stat(ufs);

			/* reset perf context to start again */
			if (ufs->perf)
				ufs_perf_reset(ufs->perf);
		} else {
			/* PM Qos Release for stability */
			__pm_qos_ctrl(ufs, false);
			/* Set for SICD */
			__sicd_ctrl(ufs, false);
		}
	}

	return ret;
}

static int exynos_ufs_get_available_lane(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = -EINVAL;

	/* Get the available lane count */
	ufs->available_lane_tx = unipro_readl(handle, UNIP_PA_AVAILTXDATALENS);
	ufs->available_lane_rx = unipro_readl(handle, UNIP_PA_AVAILRXDATALENS);

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware
	 */
	if (ufs->available_lane_rx != ufs->available_lane_tx)
		goto out;

	if (ufs->available_lane_rx == 0 || ufs->available_lane_tx == 0)
		goto out;

	ret = exynos_ufs_dbg_set_lanes(handle, ufs->dev,
				ufs->available_lane_rx);
	if (ret)
		goto out;

	ufs->num_rx_lanes = ufs->available_lane_rx;
	ufs->num_tx_lanes = ufs->available_lane_tx;
	ret = 0;

	return ret;
out:
	dev_err(hba->dev, "invalid host available lanes. rx=%d, tx=%d\n",
			__func__, ufs->available_lane_rx,
			ufs->available_lane_tx);
	BUG_ON(1);
	return ret;
}

static void exynos_ufs_override_hba_params(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (ufs->always_on)
		hba->spm_lvl = UFS_PM_LVL_3;
	else
		hba->spm_lvl = UFS_PM_LVL_5;
}

static int exynos_ufs_hce_enable_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = 0;

	PRINT_STATES(ufs);

	switch (notify) {
	case PRE_CHANGE:
		/*
		 * This function is called in ufshcd_hba_enable,
		 * maybe boot, wake-up or link start-up failure cases.
		 * To start safely, reset of entire logics of host
		 * is required
		 */
		exynos_ufs_init_host(hba);

		/* device reset */
		exynos_ufs_dev_hw_reset(hba);
		break;
	case POST_CHANGE:
		exynos_ufs_ctrl_clk(ufs, true);
		exynos_ufs_gate_clk(ufs, false);

		ret = exynos_ufs_get_available_lane(hba);

		/* freeze cport logger */
		__thaw_cport_logger(handle);

		ufs->h_state = H_RESET;

		unipro_writel(handle, DBG_SUITE1_ENABLE,
				UNIP_PA_DBG_OPTION_SUITE_1);
		unipro_writel(handle, DBG_SUITE2_ENABLE,
				UNIP_PA_DBG_OPTION_SUITE_2);
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_ufs_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	struct uic_pwr_mode *pmd = &ufs->hci_pmd_parm;
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		/* override some parameters from core driver */
		exynos_ufs_override_hba_params(hba);

		if (!IS_C_STATE_ON(ufs) || ufs->h_state != H_RESET)
			PRINT_STATES(ufs);

		/* hci */
		hci_writel(handle, DFES_ERR_EN | DFES_DEF_DL_ERRS,
			HCI_ERROR_EN_DL_LAYER);
		hci_writel(handle, DFES_ERR_EN | DFES_DEF_N_ERRS,
			HCI_ERROR_EN_N_LAYER);
		hci_writel(handle, DFES_ERR_EN | DFES_DEF_T_ERRS,
			HCI_ERROR_EN_T_LAYER);

		/* cal */
		p->mclk_rate = ufs->mclk_rate;
		p->available_lane = ufs->num_rx_lanes;
		p->max_gear = pmd->gear;

		ret = ufs_call_cal(ufs, ufs_cal_pre_link);
		if (ret) {
			dev_err(ufs->dev, "Fail to ufs_cal_pre_link: %d\n",
					ret);
			goto out;
		}

		break;
	case POST_CHANGE:
		/* update max gear after link*/
		exynos_ufs_update_max_gear(ufs);

		p->connected_tx_lane = unipro_readl(handle,
					UNIP_PA_CONNECTEDTXDATALENS);
		p->connected_rx_lane = unipro_readl(handle,
					UNIP_PA_CONNECTEDRXDATALENS);
		/* cal */
		ret = ufs_call_cal(ufs, ufs_cal_post_link);
		if (ret) {
			dev_err(ufs->dev, "Fail to ufs_cal_post_link: %d\n",
					ret);
			goto out;
		}

		/* print link start-up result */
		dev_info(ufs->dev, "UFS link start-up passes\n");
		ufs->h_state = H_LINK_UP;
		break;
	default:
		break;
	}

out:
	return ret;
}

static int exynos_ufs_pwr_change_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify,
					struct ufs_pa_layer_attr *pwr_max,
					struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *act_pmd = &ufs->device_pmd_parm;
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		if (!IS_C_STATE_ON(ufs) ||
				ufs->h_state != H_REQ_BUSY)
			PRINT_STATES(ufs);

		/* Set PMC parameters to be requested */
		exynos_ufs_init_pmc_req(hba, pwr_max, pwr_req);

		/* cal */
		ufs->cal_param.pmd = act_pmd;
		ret = ufs_call_cal(ufs, ufs_cal_pre_pmc);
		if (ret)
			dev_err(ufs->dev, "Fail to ufs_cal_pre_pmc: %d\n", ret);

		break;
	case POST_CHANGE:
		/* update active lanes after pmc */
		exynos_ufs_update_active_lanes(ufs);

		/* cal */
		ret = ufs_call_cal(ufs, ufs_cal_post_pmc);
		if (ret) {
			dev_err(ufs->dev, "Fail to ufs_cal_post_pmc: %d\n",
					ret);
			goto out;
		}

		dev_info(ufs->dev,
				"Power mode change(%d): M(%d)G(%d)L(%d)HS-series(%d)\n",
				ret, act_pmd->mode, act_pmd->gear,
				act_pmd->lane, act_pmd->hs_series);
		dev_info(ufs->dev, "HS mode config passes\n");

		ufs->h_state = H_LINK_BOOST;
		ufs->skip_flush = false;
		break;
	default:
		break;
	}

out:
	return ret;
}

static void exynos_ufs_set_nexus_t_xfer_req(struct ufs_hba *hba,
				int tag, bool cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufshcd_lrb *lrbp;
	struct ufs_vs_handle *handle = &ufs->handle;
	ufs_perf_op op = UFS_PERF_OP_NONE;
	int timeout_cnt = 50000 / 10;
	int wait_ns = 10;

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_UP &&
			ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	/* perf */
	lrbp = &hba->lrb[tag];
	if (lrbp && lrbp->cmd) {
		struct scsi_cmnd *scmd = lrbp->cmd;

		/* performance, only for SCSI */
		switch( scmd->cmnd[0]) {
		case READ_10:
			op = UFS_PERF_OP_R;
			break;
		case WRITE_10:
			op = UFS_PERF_OP_W;
			break;
		case SYNCHRONIZE_CACHE:
			op = UFS_PERF_OP_S;
			break;
		default:
			;
		}

		if (ufs->perf) {
			u32 qd;

			qd = __bitmap_weight(&hba->outstanding_reqs, SZ_32);
			ufs_perf_update(ufs->perf, qd, scmd, op);
		}

		/* cmd_logging */
		exynos_ufs_cmd_log_start(handle, hba, scmd);
	}

	/* check if an update is needed */
	while (test_and_set_bit(EXYNOS_UFS_BIT_CHK_NEXUS, &ufs->flag)
	       && timeout_cnt--)
		ndelay(wait_ns);

	if (cmd) {
		if (test_and_set_bit(tag, &ufs->nexus))
			goto out;
	} else {
		if (!test_and_clear_bit(tag, &ufs->nexus))
			goto out;
	}
	hci_writel(handle, (u32)ufs->nexus, HCI_UTRL_NEXUS_TYPE);
out:
	clear_bit(EXYNOS_UFS_BIT_CHK_NEXUS, &ufs->flag);
	ufs->h_state = H_REQ_BUSY;
}

static void exynos_ufs_check_uac(struct ufs_hba *hba, int tag, bool cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *scmd;
	struct utp_upiu_rsp *ucd_rsp_ptr;
	int result = 0;

	if (!cmd)
		return;

	lrbp = &hba->lrb[tag];
	ucd_rsp_ptr = lrbp->ucd_rsp_ptr;
	scmd = lrbp->cmd;

	result = be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24;
	if (result != UPIU_TRANSACTION_RESPONSE)
		return;

	result = be32_to_cpu(ucd_rsp_ptr->header.dword_1);
	if (result & SAM_STAT_CHECK_CONDITION) {
		result &= ~SAM_STAT_CHECK_CONDITION;
		result |= SAM_STAT_BUSY;
		ucd_rsp_ptr->header.dword_1 = cpu_to_be32(result);

		scmd->result |= (DID_SOFT_ERROR << 16);
	}

	ufs->resume_state = 0;
}

static void exynos_ufs_compl_nexus_t_xfer_req(void *data, struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	unsigned long completed_reqs;
	u32 tr_doorbell;
	int tag;

	if (!lrbp) {
		dev_err(hba->dev, "%s: lrbp: local reference block is null\n",
				__func__);
		return;
	}

	tag = lrbp->task_tag;

	/* When it's first command completion after resume, check uac. */
	if (ufs->resume_state || (lrbp->lun != 0))
		exynos_ufs_check_uac(hba, tag, (lrbp->cmd ? true : false));

	/* cmd_logging */
	if (lrbp->cmd)
		exynos_ufs_cmd_log_end(handle, hba, tag);

	tr_doorbell = std_readl(handle, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	completed_reqs = tr_doorbell ^ hba->outstanding_reqs;
	if (!(hba->outstanding_reqs^completed_reqs)) {
		if (ufs->perf)
			ufs_perf_reset(ufs->perf);
	}
}

static void exynos_ufs_set_nexus_t_task_mgmt(struct ufs_hba *hba, int tag,
						u8 tm_func)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 type;

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_TM_BUSY &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	type =  hci_readl(handle, HCI_UTMRL_NEXUS_TYPE);

	switch (tm_func) {
	case UFS_ABORT_TASK:
	case UFS_QUERY_TASK:
		type |= (1 << tag);
		break;
	case UFS_ABORT_TASK_SET:
	case UFS_CLEAR_TASK_SET:
	case UFS_LOGICAL_RESET:
	case UFS_QUERY_TASK_SET:
		type &= ~(1 << tag);
		break;
	}

	hci_writel(handle, type, HCI_UTMRL_NEXUS_TYPE);

	ufs->h_state = H_TM_BUSY;

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
}

static void __check_int_errors(void *data, struct ufs_hba *hba, bool queue_eh_work)
{
	if (hba->errors & UIC_ERROR) {
		if (ufshcd_is_auto_hibern8_supported(hba)) {
			struct exynos_ufs *ufs = to_exynos_ufs(hba);
			struct ufs_vs_handle *handle = &ufs->handle;
			u32 reg;

			reg = hci_readl(handle, HCI_AH8_STATE);
			if (reg & HCI_AH8_STATE_ERROR) {

				reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IS);
				pr_err("%s: occured AH8 ERROR State[%08X]\n",
						__func__, reg);
				ufshcd_update_evt_hist(hba, UFS_EVT_AUTO_HIBERN8_ERR, reg);
				ufshcd_set_link_broken(hba);
			}

			hci_writel(handle, AH8_ERR_REPORT_UE,
					HCI_VENDOR_SPECIFIC_IS);
		}
	}
}

static void __check_vendor_is(void *data, struct ufs_hba *hba, bool *has_outstanding)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 vs_is;

	vs_is = hci_readl(handle, HCI_VENDOR_SPECIFIC_IS);
	vs_is = vs_is & (hci_readl(handle, HCI_VENDOR_SPECIFIC_IE));

	if (vs_is)
		dev_info(hba->dev, "%s: vs_is = 0x%08X\n", __func__, vs_is);

	/* check PRE_PROC error, as we want to prevent cmd timeout */
	if (vs_is & (AH8_ERR_AT_PRE_PROC | AH8_TIMEOUT)) {
		hci_writel(handle, AH8_ERR_AT_PRE_PROC | AH8_TIMEOUT,
				HCI_VENDOR_SPECIFIC_IS);

		hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
		ufshcd_set_link_broken(hba);
		ufshcd_update_evt_hist(hba, UFS_EVT_AUTO_HIBERN8_ERR, vs_is);
		queue_work(hba->eh_wq, &hba->eh_work);
	}
}

static void exynos_ufs_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	if (cmd == UIC_CMD_DME_HIBER_ENTER) {
		if (!IS_C_STATE_ON(ufs) ||
				(ufs->h_state != H_LINK_UP &&
				 ufs->h_state != H_REQ_BUSY &&
				 ufs->h_state != H_LINK_BOOST))
			PRINT_STATES(ufs);

		if (notify == PRE_CHANGE) {
			if (ufshcd_is_auto_hibern8_supported(hba) && ufs->ah8_ahit) {
				ufshcd_auto_hibern8_update(hba, 0);
				ret = exynos_ufs_check_ah8_fsm_state(hba, HCI_AH8_IDLE_STATE);
				if (ret)
					dev_err(hba->dev,
						"%s: exynos_ufs_check_ah8_fsm_state return value is %d\n",
						__func__, ret);
			}
		} else {
			/* cal */
			ufs_call_cal(ufs, ufs_cal_post_h8_enter);

			ufs->h_state_prev = ufs->h_state;
			ufs->h_state = H_HIBERN8;
		}
	} else {
		if (notify == PRE_CHANGE) {
			ufs->h_state = ufs->h_state_prev;

			/* cal */
			ufs_call_cal(ufs, ufs_cal_pre_h8_exit);
		} else {
			int h8_delay_ms_ovly =
				ufs->params[UFS_S_PARAM_H8_D_MS];

			/* override h8 enter delay */
			if (h8_delay_ms_ovly)
				hba->clk_gating.delay_ms =
					(unsigned long)h8_delay_ms_ovly;
		}
	}
}

static int __exynos_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
		enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret;

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_HIBERN8)
		PRINT_STATES(ufs);

	if (notify != POST_CHANGE) {
		ufs->deep_suspended = false;
		return 0;
	}

	if (hba->shutting_down)
		ufs_sec_print_err_info(hba);

	if (ufs->always_on && !hba->shutting_down) {
		/*
		 * UFS PHY always-on
		 * AH8 disable -> save register(HCI, Unipro, Mphy) -> H8 enter ->
		 * REF_CLK gating -> gpio setting -> MPHY SFR Override
		 */
		struct ufs_cal_param *p = &ufs->cal_param;

		// have to add save register func using cal
		p->save_and_restore_mode = SAVE_MODE;
		ret = ufs_call_cal(ufs, ufs_cal_pre_pm);
		if (ret)
			return ret;

		exynos_ufs_gate_clk(ufs, true);

		//  setting RST_N, REF_CLK to gpio
		ret = exynos_ufs_ctrl_gpio(ufs, 0);
		if (ret)
			return ret;

		// have to add save register func using cal
		ret = exynos_smc_readsfr(EXYNOS_PHY_BIAS, &p->m_phy_bias);
		if (ret) {
			pr_err("%s: Fail to smc call[%d]", __func__, ret);
			return ret;
		}

		ret = ufs_call_cal(ufs, ufs_cal_post_pm);
		if (ret)
			return ret;

		exynos_pmu_update(PMU_UFS_OUT, IP_INISO_PHY, IP_INISO_PHY);

		pr_info("%s: dev_state: %d, link_state: %d\n", __func__,
				hba->curr_dev_pwr_mode, hba->uic_link_state);
	} else {
		/* Make sure AH8 FSM is at Hibern State.
		 * When doing SW H8 Enter UIC CMD, don't need to check this state,
		 * but SW should do refclk gating as well.
		 */
		if (ufshcd_is_auto_hibern8_enabled(hba)) {
			ret = exynos_ufs_check_ah8_fsm_state(hba,
					HCI_AH8_HIBERNATION_STATE);
			if (ret)
				dev_err(hba->dev,
					"%s: exynos_ufs_check_ah8_fsm_state return value = %d\n",
					__func__, ret);
		} else {
			exynos_ufs_gate_clk(ufs, true);
		}

		hci_writel(handle, 0 << 0, HCI_GPIO_OUT);

		exynos_ufs_ctrl_phy_pwr(ufs, false);
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif
	ufs->h_state = H_SUSPEND;
	pr_info("%s: notify=%d, is shutdown=%d", __func__, notify, hba->shutting_down);

	return 0;
}

#define UFSHCD_ENABLE_INTRS	(UTP_TRANSFER_REQ_COMPL |\
				 UTP_TASK_REQ_COMPL |\
				 UFSHCD_ERROR_MASK)

static int __exynos_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	if (!ufs->deep_suspended) {
		pr_info("%s: need ufs power discharge time.\n", __func__);
		mdelay(LDO_DISCHARGE_GUARANTEE);
	}

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_SUSPEND)
		PRINT_STATES(ufs);

	exynos_ufs_fmp_resume(hba);

	if (ufs->always_on) {
		struct ufs_cal_param *p = &ufs->cal_param;
		u32 reg;

		/* host reset */
		exynos_ufs_init_host(hba);

		p->save_and_restore_mode = RESTORE_MODE;
		ret = exynos_ufs_execute_hce(hba);
		if (ret) {
			dev_err(hba->dev, "Fail execute_hce!!!\n");
			goto out;
		}

		// hci register restore
		ret = exynos_ufs_host_restore(hba);
		if (ret)
			goto out;

		// GPIO conf
		hci_writel(&ufs->handle, 1, HCI_GPIO_OUT);
		ret = exynos_ufs_ctrl_gpio(ufs, 1);
		if (ret)
			goto out;

		// disable pad retention
		exynos_pmu_update(PMU_TOP_OUT, PAD_RTO_UFS_EMBD, PAD_RTO_UFS_EMBD);
		ret = ufs_call_cal(ufs, ufs_cal_pre_pm);
		if (ret)
			goto out;

		if (ufs->regmap_sys) {
			struct ext_cxt *cxt;

			cxt = &ufs->cxt_iocc;
			regmap_update_bits(ufs->regmap_sys, cxt->offset, cxt->mask, cxt->val);
		}

		exynos_pmu_update(PMU_UFS_OUT, IP_INISO_PHY, 0);

		// call call
		ret = ufs_call_cal(ufs, ufs_cal_post_pm);
		if (ret)
			goto out;

		// clk en
		exynos_ufs_ctrl_clk(ufs, true);
		exynos_ufs_gate_clk(ufs, false);

		// call call
		ret = ufs_call_cal(ufs, ufs_cal_resume_hibern8);
		if (ret)
			goto out;

		std_writel(&ufs->handle, UIC_COMMAND_COMPL, REG_INTERRUPT_STATUS);
		reg = std_readl(&ufs->handle, REG_INTERRUPT_ENABLE);
		reg |= (UFSHCD_UIC_MASK | UFSHCD_ENABLE_INTRS);
		std_writel(&ufs->handle, reg, REG_INTERRUPT_ENABLE);

		hba->ahit = ufs->ah8_ahit;

		pr_info("%s: dev_state: %d, link_state: %d\n", __func__,
				hba->curr_dev_pwr_mode, hba->uic_link_state);
	} else {
		/* system init */
		ret = exynos_ufs_config_externals(ufs);
		if (ret)
			return ret;
	}

	if (ufs->perf)
		ufs_perf_resume(ufs->perf);
	ufs->resume_state = 1;

	return ret;
out:
	exynos_ufs_dump_info(hba, &ufs->handle, ufs->dev);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
	dbg_snapshot_expire_watchdog();
#endif
#endif
	return 0;
}

static int exynos_ufs_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	ret = ufshcd_system_suspend(dev);
	if (ret) {
		/* Suspend failed when setup_clocks so vcc-off would not happened */
		dev_err(dev, "%s: failed to suspend: ret = %d\n", __func__, ret);
		return ret;
	}

	/* Save timestamp of vcc/vccq off time */
	ufs->vcc_off_time = ktime_get();

	dev_info(dev, "%s done\n", __func__);

	return ret;
}

static int exynos_ufs_suspend_noirq(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	ufs->deep_suspended = true;

	return 0;
}

static int exynos_ufs_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	ktime_t now;
	s64 discharge_period;
	int ret;

	/* treat as deep suspened here to prevent duplicated delay at vops_resume */
	ufs->deep_suspended = true;

	if (ufs->vcc_off_time == -1LL)
		goto resume;

	now = ktime_get();
	discharge_period = ktime_to_ms(
				ktime_sub(now, ufs->vcc_off_time));
	if (!ufs->always_on && discharge_period < LDO_DISCHARGE_GUARANTEE) {
		dev_info(dev, "%s: need to give delay: discharge_period = %lld\n",
				__func__, discharge_period);
		mdelay(LDO_DISCHARGE_GUARANTEE - discharge_period);
	}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_info(dev, "%s: discharge time = %d\n", __func__, discharge_period);
#endif

	ufs->vcc_off_time = -1LL;

resume:
	ret = ufshcd_system_resume(dev);
	if (ret)
		dev_err(dev, "%s: failed to resume: ret = %d\n", __func__, ret);

	return ret;
}

static int __apply_dev_quirks(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 peer_hibern8time;
	struct ufs_cal_param *p = &ufs->cal_param;
	/* 50us is a heuristic value, so it could change later */
	u32 ref_gate_margin = (hba->dev_info.wspecversion >= 0x300) ?
		hba->dev_info.clk_gating_wait_us : 45;

	peer_hibern8time = unipro_readl(handle, UNIP_PA_HIBERN8TIME);
	peer_hibern8time += 1;
	unipro_writel(handle, peer_hibern8time, UNIP_PA_HIBERN8TIME);

	p->ah8_thinern8_time = peer_hibern8time;
	p->ah8_brefclkgatingwaittime = ref_gate_margin;

	/*
	 * we're here, that means the sequence up
	 */
	dev_info(ufs->dev, "UFS device initialized\n");

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* check only at the first init */
	if (!(hba->eh_flags || hba->pm_op_in_progress)) {
		/* sec special features */
		ufs_sec_set_features(hba);
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
		dev_info(hba->dev, "UFS test mode enabled\n");
#endif
	}

	ufs_sec_config_features(hba);
#endif

	return 0;
}

static void __fixup_dev_quirks(struct ufs_hba *hba)
{
	struct device_node *np = hba->dev->of_node;

	hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS);

	if(of_find_property(np, "support-extended-features", NULL))
		hba->dev_quirks |= UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES;
}

static void exynos_ufs_register_vendor_hooks(void)
{
	register_trace_android_vh_ufs_compl_command(
			exynos_ufs_compl_nexus_t_xfer_req, NULL);
	register_trace_android_vh_ufs_check_int_errors(__check_int_errors, NULL);
	register_trace_android_vh_ufs_mcq_has_oustanding_reqs(__check_vendor_is, NULL);
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* register vendor hooks */
	ufs_sec_register_vendor_hooks();
#endif
}

/* to make device state active */
static int __device_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	hba->ahit = ufs->ah8_ahit;

	ufs->hibern8_enter_cnt = 0;
	ufs->hibern8_exit_cnt = 0;

	reg = std_readl(&ufs->handle, REG_INTERRUPT_ENABLE);
	reg &= ~(UFSHCD_UIC_MASK | UFSHCD_ENABLE_INTRS);
	std_writel(&ufs->handle, reg, REG_INTERRUPT_ENABLE);

	reg = hci_readl(handle, HCI_AH8_STATE);
	if (reg & HCI_AH8_STATE_ERROR) {
		reg = std_readl(handle, REG_INTERRUPT_STATUS);
		if (reg & UFSHCD_AH8_CHECK_IS) {
			std_writel(handle, reg, REG_INTERRUPT_STATUS);
			reg = std_readl(handle, REG_INTERRUPT_STATUS);
		}

		if (reg & UFSHCD_AH8_CHECK_IS)
			pr_info("%s: can't handle AH8_RESET, IS:%08x\n",
				__func__, reg);
		else {
			pr_info("%s: before AH8_RESET, AH8_STATE:%08x\n",
				__func__, hci_readl(handle, HCI_AH8_STATE));
			hci_writel(handle, HCI_CLEAR_AH8_FSM, HCI_AH8_RESET);
			pr_info("%s: after AH8_RESET, AH8_STATE:%08x\n",
				__func__, hci_readl(handle, HCI_AH8_STATE));
		}
	}

	/* guarantee device internal cache flush */
	if (hba->eh_flags && !ufs->skip_flush) {
		dev_info(hba->dev, "%s: Waiting device internal cache flush\n",
			__func__);
		ssleep(2);
		ufs->skip_flush = true;
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
		ufs_sec_inc_hwrst_cnt();
#endif
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
		/* no dump for some cases, get dump before recovery */
		exynos_ufs_dump_debug_info(hba);
#endif
	}

	return 0;
}

#define UFS_TEST_COUNT 3

static void exynos_ufs_event_notify(struct ufs_hba *hba,
		enum ufs_event_type evt, void *data)
{
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	struct ufs_event_hist *e;

	e = &hba->ufs_stats.event[evt];

	if ((e->cnt > UFS_TEST_COUNT) &&
			(evt == UFS_EVT_PA_ERR ||
			evt == UFS_EVT_DL_ERR ||
			evt == UFS_EVT_LINK_STARTUP_FAIL)) {
		exynos_ufs_dump_debug_info(hba);
		BUG();
	}
#endif

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_inc_op_err(hba, evt, data);
#endif
}

static struct ufs_hba_variant_ops exynos_ufs_ops = {
	.init = exynos_ufs_init,
	.resume = __exynos_ufs_resume,
	.device_reset = __device_reset,
	.suspend = __exynos_ufs_suspend,
	.apply_dev_quirks = __apply_dev_quirks,
	.fixup_dev_quirks = __fixup_dev_quirks,
	.setup_clocks = exynos_ufs_setup_clocks,
	.hibern8_notify = exynos_ufs_hibern8_notify,
	.dbg_register_dump = exynos_ufs_dump_debug_info,
	.hce_enable_notify = exynos_ufs_hce_enable_notify,
	.pwr_change_notify = exynos_ufs_pwr_change_notify,
	.setup_xfer_req = exynos_ufs_set_nexus_t_xfer_req,
	.setup_task_mgmt = exynos_ufs_set_nexus_t_task_mgmt,
	.link_startup_notify = exynos_ufs_link_startup_notify,
	.event_notify = exynos_ufs_event_notify,
	.program_key = exynos_ufs_fmp_program_key,
};

static int exynos_ufs_populate_dt_extern(struct device *dev,
					struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	struct ext_cxt *cxt;
	int cxt_size, read_size;
	int ret = 0;

	/*
	 * pmu for phy isolation. for the pmu, we use api from outside, not regmap
	 */
	read_size = sizeof(struct ext_cxt) / sizeof(u32);

	cxt = &ufs->cxt_phy_iso;
	cxt_size = of_property_read_variable_u32_array(dev->of_node,
			"ufs,phy_iso", (u32 *)cxt, read_size, read_size);
	if (cxt_size < 0) {
		dev_err(dev, "%s: Fail to get ufs-phy-iso\n", __func__);
		ret = -ENOENT;
		goto out;
	}

	/* others */
	/*
	* w/o 'dma-coherent' means the descriptors would be non-cacheable.
	* so, iocc should be disabled.
	*/
	ufs->is_dma_coherent = !!of_find_property(dev->of_node,
						"dma-coherent", NULL);

	if (!ufs->is_dma_coherent) {
		dev_info(dev, "no 'dma-coherent', ufs iocc disabled\n");
		goto out;
	}

	/* below code are the codes that operate based on dma coherent */
	/* look up phandle for external regions */
	ufs->regmap_sys = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	if (IS_ERR(ufs->regmap_sys)) {
		dev_err(dev, "%s: Fail to find sysreg-phandle\n", __func__);
		ret = PTR_ERR(ufs->regmap_sys);
		goto out;
	}
	cxt = &ufs->cxt_iocc;
	cxt_size = of_property_read_variable_u32_array(dev->of_node,
			"ufs,iocc", (u32 *)cxt, read_size, read_size);
	if (cxt_size < 0) {
		dev_err(dev, "%s: Fail to get ufs-iocc\n", __func__);
		ret = -ENOENT;
	}

	dev_info(dev, "ufs-iocc: offset 0x%x, mask 0x%x, value 0x%x\n",
			cxt->offset, cxt->mask,	cxt->val);

	cxt = &ufs->cxt_pad_ret;
	cxt_size = of_property_read_variable_u32_array(dev->of_node,
			"ufs,pad_retention", (u32 *)cxt, read_size, read_size);
	if (cxt_size > 0)
		dev_info(dev, "ufs-pad: offset 0x%x, mask 0x%x, value 0x%x\n",
				cxt->offset, cxt->mask,	cxt->val);
out:
	return ret;
}

static int exynos_ufs_get_pwr_mode(struct device_node *np,
				struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pmd = &ufs->hci_pmd_parm;

	pmd->mode = FAST_MODE;

	if (of_property_read_u8(np, "ufs,pmd-attr-lane", &pmd->lane))
		pmd->lane = 1;

	if (of_property_read_u8(np, "ufs,pmd-attr-gear", &pmd->gear))
		pmd->gear = 1;

	if (pmd->gear > UFS_HS_G4)
		of_property_read_u32(np, "gear-max-frequency", &ufs->mclk_gear5);

	pmd->hs_series = PA_HS_MODE_B;

	return 0;
}

static int exynos_ufs_populate_dt(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	int ret;

	/* Regmap for external regions */
	ret = exynos_ufs_populate_dt_extern(dev, ufs);
	if (ret) {
		dev_err(dev, "%s: Fail to populate dt-pmu\n", __func__);
		goto out;
	}
#if 0
	/* Get exynos-evt version for featuring. Now get main_rev instead of dt */
	ufs->cal_param.evt_ver = exynos_soc_info.main_rev;

	dev_info(dev, "exynos ufs evt version : %d\n",
			ufs->cal_param.evt_ver);
#endif
	ufs->cal_param.evt_ver = 0;

	/* PM QoS */
	child_np = of_get_child_by_name(np, "ufs-pm-qos");
	ufs->pm_qos_int_value = 0;
	ufs->pm_qos_gear5_int = 0;
	if (!child_np)
		dev_info(dev, "No ufs-pm-qos node, not guarantee pm qos\n");
	else {
		of_property_read_u32(child_np, "freq-int",
					&ufs->pm_qos_int_value);
		of_property_read_u32(child_np, "freq-gear5-int",
					&ufs->pm_qos_gear5_int);
	}

	/* UIC specifics */
	exynos_ufs_get_pwr_mode(np, ufs);

	ufs->cal_param.board = 0;
	of_property_read_u32(np, "brd-for-cal", &ufs->cal_param.board);

//	ufs_perf_populate_dt(ufs->perf, np);

	if (of_find_property(np, "samsung,support-ah8", NULL))
		ufs->ah8_ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 20) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 2);
	else
		ufs->ah8_ahit = 0;

	if (ufs->ah8_ahit) {
		ufs->cal_param.support_ah8_cal = true;
		ufs->cal_param.ah8_thinern8_time = 3;
		ufs->cal_param.ah8_brefclkgatingwaittime = 1;
	} else {
		ufs->cal_param.support_ah8_cal = false;
	}

	if (of_find_property(np, "samsung,ufs-always-on", NULL)) {
		dev_info(ufs->dev, "UFS always-on supported\n");
		ufs->always_on = 1;

		ufs->pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR(ufs->pinctrl)) {
			dev_err(dev, "failed to get pinctrl\n");
			ret = PTR_ERR(ufs->pinctrl);
			ufs->pinctrl = NULL;
			goto out;
		}

		ufs->ufs_stat_wakeup = pinctrl_lookup_state(ufs->pinctrl,
				"ufs_stat_wakeup");
		if (IS_ERR(ufs->ufs_stat_wakeup)) {
			dev_err(dev, "failed to get rst_n, refclk_out pin state\n");
			ret = PTR_ERR(ufs->ufs_stat_wakeup);
			ufs->ufs_stat_wakeup = NULL;
			goto out;
		}

		ufs->ufs_stat_sleep = pinctrl_lookup_state(ufs->pinctrl,
				"ufs_stat_sleep");
		if (IS_ERR(ufs->ufs_stat_sleep)) {
			dev_err(dev, "failed to get gpio pin state\n");
			ret = PTR_ERR(ufs->ufs_stat_sleep);
			ufs->ufs_stat_sleep = NULL;
			goto out;
		}
	}

	ret = of_property_read_u32(np, "freq-for-1us-cntval",
			&ufs->freq_for_1us_cntval);
	if (ret)
		dev_err(dev, "%s: fail to populate freq-for-1us-cntval\n",
			__func__);
out:
	return ret;
}

static int exynos_ufs_ioremap(struct exynos_ufs *ufs,
				struct platform_device *pdev)
{
	/* Indicators for logs */
	static const char *ufs_region_names[NUM_OF_UFS_MMIO_REGIONS + 1] = {
		"",			/* standard hci */
		"reg_hci",		/* exynos-specific hci */
		"reg_unipro",		/* unipro */
		"reg_ufsp",		/* ufs protector */
		"reg_phy",		/* phy */
		"reg_cport",		/* cport */
		"reg_pcs",
	};
	struct device *dev = &pdev->dev;
	struct resource *res;
	void **p = NULL;
	int i = 0;
	int ret = 0;

	for (i = 1, p = &ufs->reg_hci;
			i < NUM_OF_UFS_MMIO_REGIONS + 1; i++, p++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			continue;

		*p = devm_ioremap_resource(dev, res);
		if (!*p) {
			ret = -ENOMEM;
			break;
		}
		dev_info(dev, "%-10s 0x%llx\n", ufs_region_names[i], *p);
	}

	if (ret)
		dev_err(dev, "Fail to ioremap for %s, 0x%llx\n",
			ufs_region_names[i]);
	dev_info(dev, "\n");
	return ret;
}

/* sysfs to support utc, eom or whatever */
struct exynos_ufs_sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct exynos_ufs *ufs, char *buf,
			exynos_ufs_param_id id);
	int (*store)(struct exynos_ufs *ufs, const char *buf,
			exynos_ufs_param_id id);
	int id;
};

static ssize_t exynos_ufs_sysfs_default_show(struct exynos_ufs *ufs, char *buf,
					     exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", ufs->params[id]);
}

#define UFS_S_RO(_name, _id)						\
	static struct exynos_ufs_sysfs_attr ufs_s_##_name = {		\
		.attr = { .name = #_name, .mode = 0444 },		\
		.id = _id,						\
		.show = exynos_ufs_sysfs_default_show,			\
	}

#define UFS_S_RW(_name, _id)						\
	static struct exynos_ufs_sysfs_attr ufs_s_##_name = {		\
		.attr = { .name = #_name, .mode = 0666 },		\
		.id = _id,						\
		.show = exynos_ufs_sysfs_default_show,			\
	}

UFS_S_RO(eom_version, UFS_S_PARAM_EOM_VER);

static ssize_t
exynos_ufs_sysfs_ufs_eom_show(struct exynos_ufs *ufs, char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_eom_result_s *p;
	int len = 0;
	int i;

	p = ufs->cal_param.eom[ufs->params[UFS_S_PARAM_LANE]];
	p += ufs->params[UFS_S_PARAM_EOM_OFS];

	for (i = 0; i < EOM_DEF_VREF_MAX; i++) {
		len += snprintf(buf + len, PAGE_SIZE, "%u %u %u\n", p->v_phase,
				p->v_vref, p->v_err);
		p++;
	}

	return len;
}

static int exynos_ufs_sysfs_ufs_eom_store(struct exynos_ufs *ufs,
				const char *buf, exynos_ufs_param_id id)
{
	int value, offset;
	int ret;

	ret = sscanf(buf, "%d %d", &value, &offset);
	if (value >= ufs->num_rx_lanes) {
		dev_err(ufs->dev, "Fail set lane to %u. Its max is %u\n", value,
				ufs->num_rx_lanes);
		return -EINVAL;
	}

	ufs->params[UFS_S_PARAM_LANE] = value;
	ufs->params[UFS_S_PARAM_EOM_OFS] = offset * EOM_DEF_VREF_MAX;

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_ufs_eom = {
	.attr = { .name = "ufs_eom", .mode = 0666 },
	.id = UFS_S_PARAM_LANE,
	.show = exynos_ufs_sysfs_ufs_eom_show,
	.store = exynos_ufs_sysfs_ufs_eom_store,
};

static int exynos_ufs_sysfs_mon_store(struct exynos_ufs *ufs, const char *buf,
				      exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;
	int value;
	int ret;

	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;

	if (value & UFS_S_MON_LV1) {
		/* Trigger HCI error */
		dev_info(ufs->dev, "Interface error test\n");
		unipro_writel(handle, BIT_POS_DBG_DL_RX_INFO_FORCE |
			DL_RX_INFO_TYPE_ERROR_DETECTED | RX_BUFFER_OVERFLOW,
			UNIP_DBG_RX_INFO_CONTROL_DIRECT);

	} else if (value & UFS_S_MON_LV2) {
		/* Block all the interrupts */
		dev_info(ufs->dev, "Device error test\n");

		reg = std_readl(handle, REG_INTERRUPT_ENABLE);
		std_writel(handle, (reg & ~UTP_TRANSFER_REQ_COMPL),
				REG_INTERRUPT_ENABLE);
	} else {
		dev_err(ufs->dev, "Undefined level\n");
		return -EINVAL;
	}

	ufs->params[id] = value;
	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_monitor = {
	.attr = { .name = "monitor", .mode = 0666 },
	.id = UFS_S_PARAM_MON,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_mon_store,
};

static ssize_t exynos_ufs_sysfs_show_h8_delay(struct exynos_ufs *ufs,
					      char *buf, exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "%lu\n", ufs->hba->clk_gating.delay_ms);
}

static struct exynos_ufs_sysfs_attr ufs_s_h8_delay_ms = {
	.attr = { .name = "h8_delay_ms", .mode = 0666 },
	.id = UFS_S_PARAM_H8_D_MS,
	.show = exynos_ufs_sysfs_show_h8_delay,
};

static ssize_t exynos_ufs_sysfs_show_ah8_cnt(struct exynos_ufs *ufs,
					      char *buf, exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "AH8_Enter_cnt: %d, AH8_Exit_cnt: %d\n",
			ufs->hibern8_enter_cnt, ufs->hibern8_exit_cnt);
}

static struct exynos_ufs_sysfs_attr ufs_s_ah8_cnt = {
	.attr = { .name = "ah8_cnt_show", .mode = 0666 },
	.show = exynos_ufs_sysfs_show_ah8_cnt,
};

static int exynos_ufs_sysfs_eom_store(struct exynos_ufs *ufs, const char *buf,
				      exynos_ufs_param_id id)
{
	int value;
	int ret;

	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;

	ret = ufs_call_cal(ufs, ufs_cal_eom);
	if (ret)
		dev_err(ufs->dev, "Fail to store eom data\n");

	return ret;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom = {
	.attr = { .name = "eom", .mode = 0222 },
	.store = exynos_ufs_sysfs_eom_store,
};

/* Convert Auto-Hibernate Idle Timer register value to microseconds */
static int exynos_ufs_ahit_to_us(u32 ahit)
{
	int timer = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK, ahit);
	int scale = FIELD_GET(UFSHCI_AHIBERN8_SCALE_MASK, ahit);

	for (; scale > 0; --scale)
		timer *= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return timer;
}

/* Convert microseconds to Auto-Hibernate Idle Timer register value */
static u32 exynos_ufs_us_to_ahit(unsigned int timer)
{
	unsigned int scale;

	for (scale = 0; timer > UFSHCI_AHIBERN8_TIMER_MASK; ++scale)
		timer /= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, timer) |
	       FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, scale);
}

static ssize_t exynos_ufs_auto_hibern8_show(struct exynos_ufs *ufs,
					    char *buf, exynos_ufs_param_id id)
{
	u32 ahit;
	struct ufs_hba *hba = ufs->hba;

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return -EOPNOTSUPP;

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);
	ahit = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
	ufshcd_release(hba);
	pm_runtime_put_sync(hba->dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", exynos_ufs_ahit_to_us(ahit));
}

static int exynos_ufs_auto_hibern8_store(struct exynos_ufs *ufs, const char *buf,
					     exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;
	int value;
	int ret;

	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return -EOPNOTSUPP;

	if (value > UFSHCI_AHIBERN8_MAX)
		return -EINVAL;

	ufshcd_auto_hibern8_update(hba, exynos_ufs_us_to_ahit(value));

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_auto_hibern8 = {
	.attr = {.name = "auto_hibern8", .mode = 0666},
	.show = exynos_ufs_auto_hibern8_show,
	.store = exynos_ufs_auto_hibern8_store,
};

static void exynos_ufs_release(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.active_reqs--;

	if (hba->clk_gating.active_reqs || hba->clk_gating.is_suspended ||
	    hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL ||
	    hba->outstanding_tasks ||
	    hba->active_uic_cmd || hba->uic_async_done ||
	    hba->clk_gating.state == CLKS_OFF)
		return;

	hba->clk_gating.state = REQ_CLKS_OFF;
	queue_delayed_work(hba->clk_gating.clk_gating_workq,
			   &hba->clk_gating.gate_work,
			   msecs_to_jiffies(hba->clk_gating.delay_ms));
}

static ssize_t exynos_ufs_clkgate_enable_show(struct exynos_ufs *ufs,
					      char *buf, exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;

	return snprintf(buf, PAGE_SIZE, "%d\n", hba->clk_gating.is_enabled);
}

static int exynos_ufs_clkgate_enable_store(struct exynos_ufs *ufs, const char *buf,
						exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;
	unsigned long flags;
	int value;
	int ret;

	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (value == hba->clk_gating.is_enabled)
		goto out;

	if (value)
		exynos_ufs_release(hba);
	else
		hba->clk_gating.active_reqs++;

	hba->clk_gating.is_enabled = value;
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_clkgate_enable = {
	.attr = {.name = "clkgate_enable", .mode = 0644},
	.show = exynos_ufs_clkgate_enable_show,
	.store = exynos_ufs_clkgate_enable_store,
};

static ssize_t ufs_exynos_gear_scale_show(struct exynos_ufs *ufs, char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_perf *perf = ufs->perf;
	struct uic_pwr_mode *pmd = &ufs->device_pmd_parm;

	if (perf == NULL)
		return -EINVAL;

	if (!perf->exynos_cap_gear_scale)
		return snprintf(buf, PAGE_SIZE, "%s\n", "not supported");

	return snprintf(buf, PAGE_SIZE, "%s[%d]\n",
		perf->exynos_gear_scale? "enabled" : "disabled",
		pmd->gear);
}

static int ufs_exynos_gear_scale_store(struct exynos_ufs *ufs, const char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_perf *perf = ufs->perf;
	int value;
	int ret;

	if (perf == NULL)
		return -EINVAL;

	if (!perf->exynos_cap_gear_scale)
		return -ENOTSUPP;

	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;

	if (perf->exynos_gear_scale == !!value) {
		dev_info(ufs->dev, "already gear scale %s!!\n",
				perf->exynos_gear_scale? "enable" : "disable");
		return -EINVAL;
	}

	perf->exynos_gear_scale = !!value;
	dev_info(ufs->dev, "sysfs input: %s gear scale\n",
			perf->exynos_gear_scale? "enable" : "disable");

	ret = ufs_gear_scale_update(perf);

	return ret;
}

static struct exynos_ufs_sysfs_attr ufs_s_gear_scale = {
	.attr = { .name = "gear_scale", .mode = 0666 },
	.store = ufs_exynos_gear_scale_store,
	.show = ufs_exynos_gear_scale_show,
};

const static struct attribute *ufs_s_sysfs_attrs[] = {
	&ufs_s_eom_version.attr,
	&ufs_s_ufs_eom.attr,
	&ufs_s_eom.attr,
	&ufs_s_h8_delay_ms.attr,
	&ufs_s_monitor.attr,
	&ufs_s_auto_hibern8.attr,
	&ufs_s_clkgate_enable.attr,
	&ufs_s_ah8_cnt.attr,
	&ufs_s_gear_scale.attr,
	NULL,
};

static ssize_t exynos_ufs_sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct exynos_ufs *ufs = container_of(kobj,
			struct exynos_ufs, sysfs_kobj);
	struct exynos_ufs_sysfs_attr *param = container_of(attr,
			struct exynos_ufs_sysfs_attr, attr);

	if (!param->show) {
		dev_err(ufs->dev, "Fail to show thanks to no existence of show\n");
		return 0;
	}

	return param->show(ufs, buf, param->id);
}

static ssize_t exynos_ufs_sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t length)
{
	struct exynos_ufs *ufs = container_of(kobj,
			struct exynos_ufs, sysfs_kobj);
	struct exynos_ufs_sysfs_attr *param = container_of(attr,
			struct exynos_ufs_sysfs_attr, attr);
	int ret = 0;

	if (!param->store)
		return length;

	ret = param->store(ufs, buf, param->id);
	return (ret == 0) ? length : (ssize_t)ret;
}

static const struct sysfs_ops exynos_ufs_sysfs_ops = {
	.show	= exynos_ufs_sysfs_show,
	.store	= exynos_ufs_sysfs_store,
};

static struct kobj_type ufs_s_ktype = {
	.sysfs_ops	= &exynos_ufs_sysfs_ops,
	.release	= NULL,
};

static int exynos_ufs_sysfs_init(struct exynos_ufs *ufs)
{
	int error = -ENOMEM;
	int i;
	struct ufs_eom_result_s *p;

	/* allocate memory for eom per lane */
	for (i = 0; i < MAX_LANE; i++) {
		ufs->cal_param.eom[i] =
			devm_kcalloc(ufs->dev, EOM_MAX_SIZE,
				     sizeof(struct ufs_eom_result_s),
				     GFP_KERNEL);
		p = ufs->cal_param.eom[i];
		if (!p) {
			dev_err(ufs->dev, "Fail to allocate eom data\n");
			goto fail_mem;
		}
	}

	/* create a path of /sys/kernel/ufs_x */
	kobject_init(&ufs->sysfs_kobj, &ufs_s_ktype);
	error = kobject_add(&ufs->sysfs_kobj, kernel_kobj, "ufs");
	if (error) {
		dev_err(ufs->dev, "Fail to register sysfs directory: %d\n",
				error);
		goto fail_kobj;
	}

	/* create attributes */
	error = sysfs_create_files(&ufs->sysfs_kobj, ufs_s_sysfs_attrs);
	if (error) {
		dev_err(ufs->dev, "Fail to create sysfs files: %d\n", error);
		goto fail_kobj;
	}

	/*
	 * Set sysfs params by default. The values could change or
	 * initial configuration could be done elsewhere in the future.
	 *
	 * As for eom_version, you have to move it to store a value
	 * from device tree when eom code is revised, even though I expect
	 * it's not gonna to happen.
	 */
	ufs->params[UFS_S_PARAM_EOM_VER] = 0;
	ufs->params[UFS_S_PARAM_MON] = 0;
	ufs->params[UFS_S_PARAM_H8_D_MS] = 4;

	return 0;

fail_kobj:
	kobject_put(&ufs->sysfs_kobj);
fail_mem:
	for (i = 0; i < MAX_LANE; i++) {
		if (ufs->cal_param.eom[i])
			devm_kfree(ufs->dev, ufs->cal_param.eom[i]);
		ufs->cal_param.eom[i] = NULL;
	}
	return error;
}

static inline void exynos_ufs_sysfs_exit(struct exynos_ufs *ufs)
{
	kobject_put(&ufs->sysfs_kobj);
}

static void __ufs_resume_async(struct work_struct *work)
{
	struct exynos_ufs *ufs =
		container_of(work, struct exynos_ufs, resume_work);
	struct ufs_hba *hba = ufs->hba;

	/* to block incoming commands prior to completion of resuming */
	hba->ufshcd_state = UFSHCD_STATE_RESET;
	ufshcd_system_resume(hba->dev);
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static int exynos_ufs_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_ufs *ufs = container_of(nb, struct exynos_ufs, itmon_nb);
	struct itmon_notifier *itmon_data = nb_data;
	int size;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	size = sizeof("UFS") - 1;
	if ((itmon_data->master && !strncmp("UFS", itmon_data->master, size)) ||
			(itmon_data->dest && !strncmp("UFS", itmon_data->dest,
				size))) {
		exynos_ufs_dump_info(hba, &ufs->handle, ufs->dev);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
		dbg_snapshot_expire_watchdog();
#endif
#endif
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
#endif

static int exynos_ufs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_ufs *ufs;
	int ret;

	dev_info(dev, "%s: start\n", __func__);
	dev_info(dev, "===============================\n");

	/* allocate memory */
	ufs = devm_kzalloc(dev, sizeof(*ufs), GFP_KERNEL);
	if (!ufs) {
		ret = -ENOMEM;
		goto out;
	}
	ufs->dev = dev;
	dev->platform_data = ufs;

	/* remap regions */
	ret = exynos_ufs_ioremap(ufs, pdev);
	if (ret)
		goto out;
	ufs_map_vs_regions(ufs);

	/* populate device tree nodes */
	ret = exynos_ufs_populate_dt(dev, ufs);
	if (ret) {
		dev_err(dev, "Fail to get dt info.\n");
		return ret;
	}

	if (ufs->always_on) {
		// save output state to high
		exynos_ufs_ctrl_gpio(ufs, 0);
		exynos_ufs_ctrl_gpio(ufs, 1);
	}

	/* init cal */
	ufs->cal_param.handle = &ufs->handle;
	ret = ufs_call_cal(ufs, ufs_cal_init);
	if (ret)
		return ret;
	dev_info(dev, "===============================\n");

	/* idle ip nofification for SICD, disable by default */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	ufs->idle_ip_index = exynos_get_idle_ip_index(dev_name(ufs->dev), 1);
	__sicd_ctrl(ufs, true);
#endif
	/* register pm qos knobs */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&ufs->pm_qos_int,
				PM_QOS_DEVICE_THROUGHPUT, 0);
#endif

	/* init dbg */
	exynos_ufs_init_mem_log(pdev);
	ret = exynos_ufs_init_dbg(&ufs->handle);
	if (ret)
		return ret;

	/* store ufs host symbols to analyse later */
	ufs_host_backup = ufs;

	/* init sysfs */
	exynos_ufs_sysfs_init(ufs);

	/* init specific states */
	ufs->h_state = H_DISABLED;
	ufs->c_state = C_OFF;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_init_logging(dev);
#endif

	/* async resume */
	INIT_WORK(&ufs->resume_work, __ufs_resume_async);

	/* init vcc-off timestamp */
	ufs->vcc_off_time = -1LL;

	/* register vendor hooks: compl_commmand, etc */
	exynos_ufs_register_vendor_hooks();

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	/* add itmon notifier */
	ufs->itmon_nb.notifier_call = exynos_ufs_itmon_notifier;
	itmon_notifier_chain_register(&ufs->itmon_nb);
#endif

	/* go to core driver through the glue driver */
	ret = ufshcd_pltfrm_init(pdev, &exynos_ufs_ops);
	if (ret)
		dev_err(dev, "%s: pltfrm_init failed %d\n", __func__, ret);
out:
	return ret;
}

static int exynos_ufs_remove(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	exynos_ufs_sysfs_exit(ufs);

	disable_irq(hba->irq);
	ufshcd_remove(hba);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_remove_request(&ufs->pm_qos_int);
#endif

	/* performance */
	if (ufs->perf)
		ufs_perf_exit(ufs->perf);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_remove_features(hba);
#endif

	return 0;
}

static void exynos_ufs_shutdown(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	struct ufs_hba *hba;

	pr_info("%s: +++\n", __func__);

	hba = (struct ufs_hba *)platform_get_drvdata(pdev);

	ufshcd_shutdown(hba);
	hba->ufshcd_state = UFSHCD_STATE_ERROR;

	if (ufs->perf)
		ufs_perf_exit(ufs->perf);

	pr_info("%s: ---\n", __func__);
}

static const struct dev_pm_ops exynos_ufs_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_ufs_suspend, exynos_ufs_resume)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
	.suspend_noirq = exynos_ufs_suspend_noirq,
};

static const struct of_device_id exynos_ufs_match[] = {
	{ .compatible = "samsung,exynos-ufs", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ufs_match);

static struct platform_driver exynos_ufs_driver = {
	.driver = {
		.name = "exynos-ufs",
		.owner = THIS_MODULE,
		.pm = &exynos_ufs_dev_pm_ops,
		.of_match_table = exynos_ufs_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_ufs_probe,
	.remove = exynos_ufs_remove,
	.shutdown = exynos_ufs_shutdown,
};

module_platform_driver(exynos_ufs_driver);
MODULE_DESCRIPTION("Exynos Specific UFSHCI driver");
MODULE_AUTHOR("Seungwon Jeon <tgih.jun@samsung.com>");
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_LICENSE("GPL");
