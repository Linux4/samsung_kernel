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
#include "ufs-cal-snr-if.h"
#include "ufs-vs-mmio.h"
#include "ufs-vs-regs.h"
#include <ufs/ufshcd.h>
#include "../core/ufshcd-crypto.h"
#include "../core/ufshcd-priv.h"
#include <ufs/ufshci.h>
#include <ufs/unipro.h>
#include "ufshcd-pltfrm.h"
#include <ufs/ufs_quirks.h>
#include "ufs-exynos.h"
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/bitfield.h>
#include <linux/pinctrl/consumer.h>
#include <soc/samsung/exynos/exynos-soc.h>
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
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
#include <soc/samsung/exynos/exynos-itmon.h>
#endif

#undef RELEASE
#include <soc/samsung/exynos-devfreq.h>

#include <scsi/scsi_cmnd.h>

/* Performance */
#include "ufs-exynos-perf.h"

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
#include "ufs-sec-feature.h"
#endif

#define LDO_DISCHARGE_GUARANTEE	10
#define IS_C_STATE_ON(h) ((h)->c_state == C_ON)
#define PRINT_STATES(h)							\
	dev_err((h)->dev, "%s: prev h_state %d, cur c_state %d\n",	\
				__func__, (h)->h_state, (h)->c_state);

#define MCQ_QCFGPTR_MASK	GENMASK(7, 0)
#define MCQ_QCFGPTR_UNIT	0x200
#define MCQ_SQATTR_OFFSET(c) \
	((((c) >> 16) & MCQ_QCFGPTR_MASK) * MCQ_QCFGPTR_UNIT)
#define MCQ_QCFG_SIZE	0x40
#define STD_MCQ_OFFSET			0x400

#define MCQ_CFG_MAC_MASK	GENMASK(16, 8)
#define MCQ_ENTRY_SIZE_IN_DWORD	8
#define QUEUE_EN_OFFSET 31
#define QUEUE_ID_OFFSET 16

#define DEVFREQ_INT	1

static struct exynos_ufs *ufs_host_backup;

typedef enum {
	UFS_S_MON_LV1 = (1 << 0),
	UFS_S_MON_LV2 = (1 << 1),
} exynos_ufs_mon;

static bool is_smdk;
module_param(is_smdk, bool, 0);
MODULE_PARM_DESC(is_smdk, "To distinguish whether this board is smdk or erd");

/* Functions to map registers or to something by other modules */
static void ufs_udelay(u32 n)
{
	udelay(n);
}

static inline void ufs_map_vs_regions(struct exynos_ufs *ufs)
{
	ufs->handle.ufsp = ufs->reg_ufsp;
	ufs->handle.unipro = ufs->reg_unipro;
	ufs->handle.pma = ufs->reg_phy;
	ufs->handle.cport = ufs->reg_cport;
	ufs->handle.pcs = ufs->reg_pcs;
	ufs->handle.udelay = ufs_udelay;
}

static inline void ufs_map_vs_mcq(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;

	ufs->handle.mcq = hba->res[RES_MCQ].base;
	ufs->handle.sqcq = hba->res[RES_MCQ_SQD].base;
}

/* Helper for UFS CAL interface */
int ufs_call_cal(struct exynos_ufs *ufs, void *func)
{
	struct ufs_cal_param *p = &ufs->cal_param;
	int ret;
	cal_if_func fn;

	fn = (cal_if_func)func;
	ret = fn(p);
	if (ret) {
		dev_err(ufs->dev, "%s: %d\n", __func__, ret);
		ret = -EPERM;
	}

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

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	pr_info("%s: current INT level: %lukhz\n", __func__,
			exynos_devfreq_get_domain_freq(DEVFREQ_INT));
#endif

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

	/*
	 * bypass internal gate for unipro clock.
	 * we assume hwacg is enabled
	 */
	reg &= ~(UNIPRO_MCLK_STOP_EN | UNIPRO_PCLK_STOP_EN);

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

static void __exynos_ufs_gate_clk(struct ufs_cal_param *p)
{
	struct exynos_ufs *ufs = container_of(p, struct exynos_ufs, cal_param);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg = 0;

	switch (p->save_and_restore_mode) {
	case SAVE_MODE:
		exynos_ufs_gate_clk(ufs, 1);

		reg = hci_readl(handle, HCI_FORCE_HCS);
		reg &= ~MPHY_APBCLK_STOP_EN;
		hci_writel(handle, reg, HCI_FORCE_HCS);
		break;

	case RESTORE_MODE:
		exynos_ufs_ctrl_clk(ufs, 1);
		exynos_ufs_gate_clk(ufs, 0);
		break;

	default:
		break;
	}
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

	/* IA_TICK_SEL : 1(1us_TO_CNT_VAL) */
	reg = hci_readl(handle, HCI_UFSHCI_V2P1_CTRL);
	reg |= IA_TICK_SEL;
	hci_writel(handle, reg, HCI_UFSHCI_V2P1_CTRL);

	if (ufs->freq_for_1us_cntval) {
		/* To make 40us */
		reg = hci_readl(handle, HCI_1US_TO_CNT_VAL);
		reg &= ~CNT_VAL_1US_MASK;
		reg |= (ufs->freq_for_1us_cntval & CNT_VAL_1US_MASK);
		hci_writel(handle, reg, HCI_1US_TO_CNT_VAL);
	}

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

static void exynos_ufs_ah8_run_by_sw(struct exynos_ufs *ufs, bool en)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	if (!ufs->ah8_by_h8)
		return;

	reg = hci_readl(handle, HCI_UFSHCI_V2P1_CTRL);

	if (en)
		reg |= AH8_RUN_BY_SW_H8;
	else
		reg &= ~AH8_RUN_BY_SW_H8;

	hci_writel(handle, reg, HCI_UFSHCI_V2P1_CTRL);
}

static void exynos_ufs_config_host(struct exynos_ufs *ufs)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufs_hba *hba = ufs->hba;
	u32 reg = 0;

	/* internal clock control */
	exynos_ufs_ctrl_auto_hci_clk(ufs, false);
	exynos_ufs_set_unipro_mclk(ufs);

	/* period for interrupt aggregation */
	exynos_ufs_set_internal_timer(ufs);

	/* misc HCI configurations */
	hci_writel(handle, 0xA, HCI_DATA_REORDER);

	/*
	 * Disable PRDT_PREFETCH with MCQ
	 */
	if (!hba->mcq_sup || (ufs->cal_param.evt_ver != 0))
		reg = PRDT_PREFECT_EN;
	reg |= PRDT_SET_SIZE(12);
	hci_writel(handle, reg,	HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(handle, PRDT_SET_SIZE(12), HCI_RXPRDT_ENTRY_SIZE);

	/* I_T_L_Q isn't used at the beginning */
	ufs->nexus = 0;
	hci_writel(handle, ufs->nexus, HCI_UTRL_NEXUS_TYPE);
	hci_writel(handle, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);

	reg = hci_readl(handle, HCI_AXIDMA_RWDATA_BURST_LEN) &
					~BURST_LEN(0);
	hci_writel(handle, WLU_EN | BURST_LEN(3),
					HCI_AXIDMA_RWDATA_BURST_LEN);

	reg = hci_readl(handle, HCI_CLKMODE);
	reg |= (REF_CLK_MODE | PMA_CLKDIV_VAL);
	hci_writel(handle, reg, HCI_CLKMODE);

	reg = hci_readl(handle, HCI_UFSHCI_V2P1_CTRL);
	/* complete after ind signal from UniPro */
	reg |= UIC_CMD_COMPLETE_SEL;
	hci_writel(handle, reg, HCI_UFSHCI_V2P1_CTRL);

	/* support SW_H8 cmd during enable AH8 */
	exynos_ufs_ah8_run_by_sw(ufs, true);

	/* enable AXIDMA_FSM_TIMER for detecting bus error */
	reg = AXIDMA_FSM_TIMER(AXIDMA_FSM_TIMER_VALUE);
	reg |= AXIDMA_FSM_TIMER_ENABLE;
	hci_writel(handle, reg, HCI_AXIDMA_FSM_TIMER);

	/*
	 * enable HWACG control
	 *
	 * default value 1->0 from KC.
	 * always "0"(controlled by UFS_ACG_DISABLE)
	 * but this makes sure zero value.
	 */
	reg = hci_readl(handle, HCI_UFS_ACG_DISABLE);
	hci_writel(handle, reg & (~HCI_UFS_ACG_DISABLE_EN),
			HCI_UFS_ACG_DISABLE);
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
		/* enable PAD retention */
		cxt = &ufs->cxt_pad_ret;
		if (cxt) {
			u32 reg;

			exynos_pmu_read(cxt->offset, &reg);
			reg |= cxt->mask;
			exynos_pmu_write(cxt->offset, reg);
		}
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
	hba->caps = UFSHCD_CAP_WB_EN | UFSHCD_CAP_CLK_GATING;
	if (ufs->ah8_ahit == 0)
		hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	/* quirks of common driver */
	hba->quirks = UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR |
			UFSHCD_QUIRK_4KB_DMA_ALIGNMENT |
			UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL |
			UFSHCD_QUIRK_SKIP_DEF_UNIPRO_TIMEOUT_SETTING;
}

static int ufs_set_affinity(unsigned int irq, u32 affinity)
{
       int num_cpu;

#if defined(CONFIG_VENDOR_NR_CPUS)
       num_cpu = CONFIG_VENDOR_NR_CPUS;
#else
       num_cpu = 8;
#endif
       if (affinity >= num_cpu) {
               pr_err("%s: idx:%d affinity:%d error. cpu max:%d\n", __func__,
                       irq, affinity, num_cpu);
               return -EINVAL;
       }

       return irq_set_affinity_hint(irq, cpumask_of(affinity));
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

	if (!hba->mcq_enabled)
		ufs_set_affinity(hba->irq, 4);

	return 0;
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
	u32 reg;

	/* report to IS.UE reg when UIC error happens during AH8 */
#if defined(CONFIG_SOC_S5E8845)
	reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IE);
	reg |= AH8_ERR_REPORT_UE;
	hci_writel(handle, reg, HCI_VENDOR_SPECIFIC_IE);
#else
	reg = hci_readl(handle, HCI_AH8_UE_CONFIG);
	reg |= HCI_AH8_ERR_REPORT;
	hci_writel(handle, reg, HCI_AH8_UE_CONFIG);
#endif
}

static void exynos_ufs_restore_externals(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (ufs->regmap_sys) {
		struct ext_cxt *cxt;

		cxt = &ufs->cxt_iocc;
		regmap_update_bits(ufs->regmap_sys, cxt->offset,
				cxt->mask, cxt->val);
	}
}

static void exynos_ufs_ctrl_gpio(struct ufs_cal_param *p)
{
	struct exynos_ufs *ufs = container_of(p, struct exynos_ufs, cal_param);
	int ret = -EINVAL;

	if (p->save_and_restore_mode == SAVE_MODE)
		ret = pinctrl_select_state(ufs->pinctrl, ufs->ufs_stat_sleep);
	else if (p->save_and_restore_mode == RESTORE_MODE)
		ret = pinctrl_select_state(ufs->pinctrl, ufs->ufs_stat_wakeup);
	else
		pr_err("%s: not defined operation\n", __func__);

	if (ret)
		dev_err(ufs->dev, "Fail to control gpio(mode: %d)\n",
				p->save_and_restore_mode);
}

static void exynos_ufs_pad_retention(struct ufs_cal_param *p)
{
	exynos_pmu_update(PMU_TOP_OUT, PAD_RTO_UFS_EMBD, PAD_RTO_UFS_EMBD);
}

static void exynos_ufs_pmu_input_iso(struct ufs_cal_param *p)
{
	if (p->save_and_restore_mode == SAVE_MODE)
		exynos_pmu_update(PMU_UFS_OUT, IP_INISO_PHY, IP_INISO_PHY);
	else if (p->save_and_restore_mode == RESTORE_MODE)
		exynos_pmu_update(PMU_UFS_OUT, IP_INISO_PHY, 0);
	else
		pr_err("%s: not defined operation\n", __func__);
}

/* Operation and runtime registers configuration */
#define MCQ_CFG_n(r, i)	((r) + MCQ_QCFG_SIZE * (i))
#define MCQ_OPR_OFFSET_n(p, i) \
	(hba->mcq_opr[(p)].offset + hba->mcq_opr[(p)].stride * (i))

#define MAX_SUPP_MAX	64
static int exynos_ufs_get_hba_mac(struct ufs_hba *hba)
{
	return MAX_SUPP_MAX;
}

static void __iomem *mcq_opr_base(struct ufs_hba *hba,
					 enum ufshcd_mcq_opr n, int i)
{
	struct ufshcd_mcq_opr_info_t *opr = &hba->mcq_opr[n];

	return opr->base + opr->stride * i;
}

static u32 exynos_mcq_read_cqis(struct ufs_hba *hba, int i)
{
	return readl(mcq_opr_base(hba, OPR_CQIS, i) + REG_CQIS);
}

static int exynos_ufs_get_outstanding_cqs(struct ufs_hba *hba,
			unsigned long *ocqs)
{
	u32 cqis_vs = 0;
	struct ufshcd_res_info *mcq_vs_res = &hba->res[RES_MCQ_SQD];
	int i = 0;
	u32 events;

	if (!mcq_vs_res->base)
		return -EINVAL;

	for (i = 0; i < hba->nr_hw_queues; i++) {
		events = exynos_mcq_read_cqis(hba, i);

		if (events)
			cqis_vs |= (1 << i);
	}

	*ocqs = cqis_vs;
	return 0;
}

static void exynos_mcq_make_queues_operational(struct ufs_hba *hba)
{
	struct ufs_hw_queue *hwq;
	u16 qsize;
	int i;

	for (i = 0; i < hba->nr_hw_queues; i++) {
		hwq = &hba->uhq[i];
		hwq->id = i;
		qsize = hwq->max_entries * MCQ_ENTRY_SIZE_IN_DWORD - 1;

		/* Submission Queue Lower Base Address */
		ufsmcq_writelx(hba, lower_32_bits(hwq->sqe_dma_addr),
			      MCQ_CFG_n(REG_SQLBA, i));
		/* Submission Queue Upper Base Address */
		ufsmcq_writelx(hba, upper_32_bits(hwq->sqe_dma_addr),
			      MCQ_CFG_n(REG_SQUBA, i));
		/* Submission Queue Doorbell Address Offset */
		ufsmcq_writelx(hba, MCQ_OPR_OFFSET_n(OPR_SQD, i),
			      MCQ_CFG_n(REG_SQDAO, i));
		/* Submission Queue Interrupt Status Address Offset */
		ufsmcq_writelx(hba, MCQ_OPR_OFFSET_n(OPR_SQIS, i),
			      MCQ_CFG_n(REG_SQISAO, i));

		/* Completion Queue Lower Base Address */
		ufsmcq_writelx(hba, lower_32_bits(hwq->cqe_dma_addr),
			      MCQ_CFG_n(REG_CQLBA, i));
		/* Completion Queue Upper Base Address */
		ufsmcq_writelx(hba, upper_32_bits(hwq->cqe_dma_addr),
			      MCQ_CFG_n(REG_CQUBA, i));
		/* Completion Queue Doorbell Address Offset */
		ufsmcq_writelx(hba, MCQ_OPR_OFFSET_n(OPR_CQD, i),
			      MCQ_CFG_n(REG_CQDAO, i));
		/* Completion Queue Interrupt Status Address Offset */
		ufsmcq_writelx(hba, MCQ_OPR_OFFSET_n(OPR_CQIS, i),
			      MCQ_CFG_n(REG_CQISAO, i));

		/* Save the base addresses for quicker access */
		hwq->mcq_sq_head = mcq_opr_base(hba, OPR_SQD, i) + REG_SQHP;
		hwq->mcq_sq_tail = mcq_opr_base(hba, OPR_SQD, i) + REG_SQTP;
		hwq->mcq_cq_head = mcq_opr_base(hba, OPR_CQD, i) + REG_CQHP;
		hwq->mcq_cq_tail = mcq_opr_base(hba, OPR_CQD, i) + REG_CQTP;

		/* Reinitializing is needed upon HC reset */
		hwq->sq_tail_slot = hwq->cq_tail_slot = hwq->cq_head_slot = 0;

		/* Enable Tail Entry Push Status interrupt only for non-poll queues */
		if (i < hba->nr_hw_queues - hba->nr_queues[HCTX_TYPE_POLL])
			writel(1, mcq_opr_base(hba, OPR_CQIS, i) + REG_CQIE);

		/* Completion Queue Enable|Size to Completion Queue Attribute */
		ufsmcq_writel(hba, (1 << QUEUE_EN_OFFSET) | qsize,
			      MCQ_CFG_n(REG_CQATTR, i));

		/*
		 * Submission Qeueue Enable|Size|Completion Queue ID to
		 * Submission Queue Attribute
		 */
		ufsmcq_writel(hba, (1 << QUEUE_EN_OFFSET) | qsize |
			      (i << QUEUE_ID_OFFSET),
			      MCQ_CFG_n(REG_SQATTR, i));
	}
}

static void exynos_mcq_config_mac(struct ufs_hba *hba, u32 max_active_cmds)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	reg = std_readl(handle, REG_UFS_MCQ_CFG);
	reg &= ~MCQ_CFG_MAC_MASK;
	reg |= FIELD_PREP(MCQ_CFG_MAC_MASK, max_active_cmds - 1);
	std_writel(handle, reg, REG_UFS_MCQ_CFG);
}

static struct exynos_ia_group ia_table[] = {
	{0, 0x0},
	// MCQIACR					IAGVLD[8] | IAG[4:0]
	{0, 0x1},
	{0, 0x2},
	{0, 0x3},
	{0, 0x4},
	{0, 0x5},
	{MCQIACR_IAEN | MCQIACR_IAPWEN | MCQIACR_IACTH | MCQIACR_OVAL_T, 0x106},
	{MCQIACR_IAEN | MCQIACR_IAPWEN | MCQIACR_IACTH | MCQIACR_OVAL_T, 0x107},
	{MCQIACR_IAEN | MCQIACR_IAPWEN | MCQIACR_IACTH | MCQIACR_OVAL_T, 0x107},
	{MCQIACR_IAEN | MCQIACR_IAPWEN | MCQIACR_IACTH | MCQIACR_OVAL_T, 0x106},
	{0, 0xa},
};

static void exynos_mcq_ia_config(struct ufs_hba *hba)
{
	for (int i = 0; i < hba->nr_hw_queues; i++) {
		writel(ia_table[i].enable, mcq_opr_base(hba, OPR_CQIS, i)
				+ REG_MCQIACR);
		ufsmcq_writelx(hba, ia_table[i].cfg,
				MCQ_CFG_n(REG_CQCFG, i));
	}
}

static void exynos_config_mcq(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	exynos_mcq_make_queues_operational(hba);
	exynos_mcq_config_mac(hba, hba->nutrs);
	/* SQ/CQ group config for evt1 */
	if (ufs->cal_param.evt_ver)
		exynos_mcq_ia_config(hba);

	/* Select MCQ mode */
	std_writel(handle, std_readl(handle, REG_UFS_MEM_CFG) | 0x1,
		      REG_UFS_MEM_CFG);
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

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE) && !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* check device_stuck info and call panic before host reset */
	ufs_sec_check_device_stuck();
#endif

	hba->ahit = ufs->ah8_ahit;

	ufs->hibern8_enter_cnt = 0;
	ufs->hibern8_exit_cnt = 0;

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
	int ret = 0;

	if (on) {
		if (notify == PRE_CHANGE) {
			/* Clear for SICD */
			__sicd_ctrl(ufs, true);
			/* PM Qos hold for stability */
			if (hba->pwr_info.gear_rx != UFS_HS_G1)
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
			if (hba->pwr_info.gear_rx != UFS_HS_G1)
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
	dev_err(hba->dev, "%s: invalid host available lanes. rx=%d, tx=%d\n",
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
		* The maximum segment size must be set after scsi_host_alloc()
		* has been called and before LUN scanning starts
		* (ufshcd_async_scan()). Note: this callback may also be called
		* from other functions than ufshcd_init().
		*/
		hba->host->max_segment_size = SZ_4K;

		/*
		 * This function is called in ufshcd_hba_enable,
		 * maybe boot, wake-up or link start-up failure cases.
		 * To start safely, reset of entire logics of host
		 * is required
		 */
		exynos_ufs_init_host(hba);

		/* device reset */
		exynos_ufs_dev_hw_reset(hba);

		/* Override clk_gating work delay */
		hba->clk_gating.delay_ms = ufs->params[UFS_S_PARAM_H8_D_MS];
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

		/*
		 * There are two cases, reset and recovery for LINERESET and
		 * in case of LINERESET, target gear is a previously used value.
		 * So need to use pwr_req to determine which value to be used.
		 */
		if (pwr_req->gear_rx > UFS_HS_G1)
			__pm_qos_ctrl(ufs, true);
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

static void exynos_ufs_check_uac(struct ufs_hba *hba, int tag, bool cmd)
{
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *scmd;
	struct utp_upiu_rsp *ucd_rsp_ptr;
	u8 sense_key;
	u32 result = 0;

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
		sense_key = lrbp->ucd_rsp_ptr->sr.sense_data[2] & 0x0F;
		if (sense_key == UNIT_ATTENTION) {
			pr_info("%s: lun = %u, opcode = %u, result = 0x%08X\n",
					__func__, lrbp->lun, scmd->cmnd[0], result);
			result &= ~SAM_STAT_CHECK_CONDITION;
			result |= SAM_STAT_BUSY;
			ucd_rsp_ptr->header.dword_1 = cpu_to_be32(result);

			scmd->result |= (DID_SOFT_ERROR << 16);
		}
	}
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

	/* check uac from PM_SUSPEND_PREPARE to PM_POST_SUSPEND */
	exynos_ufs_check_uac(hba, tag, (lrbp->cmd ? true : false));

	/* cmd_logging */
	if (lrbp->cmd)
		exynos_ufs_cmd_log_end(handle, hba, tag);

	if (hba->mcq_enabled) {
		if (!hba->clk_gating.active_reqs) {
			if (ufs->perf)
				ufs_perf_reset(ufs->perf);
		}
	} else {
		tr_doorbell = std_readl(handle, REG_UTP_TRANSFER_REQ_DOOR_BELL);
		completed_reqs = tr_doorbell ^ hba->outstanding_reqs;
		if (!(hba->outstanding_reqs^completed_reqs)) {
			if (ufs->perf)
				ufs_perf_reset(ufs->perf);
		}
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
	if (is_mcq_enabled(hba) && ufs->cal_param.evt_ver == 0) {
		ufs->tm_tag = tag;
		queue_work(ufs->tm_wq, &ufs->tm_work);
	}
}

static void __check_int_errors(void *data, struct ufs_hba *hba, bool queue_eh_work)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	if (hba->errors & UIC_ERROR) {
		if (ufshcd_is_auto_hibern8_supported(hba)) {
			reg = hci_readl(handle, HCI_AH8_STATE);
			if (reg & HCI_AH8_STATE_ERROR) {
#if defined(CONFIG_SOC_S5E8845)
				if (hci_readl(handle, HCI_VENDOR_SPECIFIC_IE)) {
					reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IS);
					hci_writel(handle, AH8_ERR_REPORT_UE,
							HCI_VENDOR_SPECIFIC_IS);
				}
#endif
				pr_err("%s: occured AH8 ERROR State[%08X]\n",
						__func__, reg);

#if defined(CONFIG_SOC_S5E9945)
				reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IS_ERR_AH8);
				if (reg & HCI_AH8_TIMEOUT) {
					exynos_ufs_dump_debug_info(hba);
					dbg_snapshot_expire_watchdog();
				}
#endif
				/* Set MSB of evt_hist value when evt_hist is updated
				 * from __check_int_errors().
				 */
				hba->errors |= UFSHCD_UIC_HIBERN8_MASK;
				ufshcd_set_link_broken(hba);
			}
		}

	}

#if defined(CONFIG_SOC_S5E9945)
	reg = hci_readl(handle, HCI_VENDOR_SPECIFIC_IS_ERR_UTP);
	if (reg & HCI_UTP_ERR_REPORT) {
		pr_err("%s: occured UTP error[%08X]\n", __func__, reg);
		exynos_ufs_dump_debug_info(hba);
	}
#endif
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
			if (ufs->ah8_by_h8)
				return;

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

			if (ufs->always_on && hba->pm_op_in_progress)
				exynos_ufs_ah8_run_by_sw(ufs, true);
		}
	}
}

static int __exynos_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
		enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = 0;

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_HIBERN8)
		PRINT_STATES(ufs);

	if (notify != POST_CHANGE)
		return ret;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	if (hba->shutting_down)
		ufs_sec_print_err_info(hba);
	else
		ufs_sec_print_err();
#endif

	if (ufs->always_on && !hba->shutting_down) {
		struct ufs_cal_param *p = &ufs->cal_param;

		exynos_ufs_ah8_run_by_sw(ufs, false);

		p->save_and_restore_mode = SAVE_MODE;
		ret = ufs_call_cal(ufs, ufs_save_register);
		if (ret)
			return ret;

		pr_info("%s: dev_state: %d, link_state: %d\n", __func__,
				hba->curr_dev_pwr_mode, hba->uic_link_state);
	} else {
		exynos_ufs_gate_clk(ufs, true);

		hci_writel(handle, 0 << 0, HCI_GPIO_OUT);

		exynos_ufs_ctrl_phy_pwr(ufs, false);
	}

	__pm_qos_ctrl(ufs, false);

	ufs->h_state = H_SUSPEND;

	return ret;
}

static int __exynos_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_SUSPEND)
		PRINT_STATES(ufs);

	if (ufs->perf)
		ufs_perf_resume(ufs->perf);

	if (hba->pwr_info.gear_rx > UFS_HS_G1)
		__pm_qos_ctrl(ufs, true);

	exynos_ufs_fmp_resume(hba);

	if (ufs->always_on) {
		struct ufs_cal_param *p = &ufs->cal_param;
		struct ufs_vs_handle *handle = &ufs->handle;

		exynos_ufs_restore_externals(hba);

		p->save_and_restore_mode = RESTORE_MODE;
		ret = ufs_call_cal(ufs, ufs_restore_register);
		if (ret)
			goto out;

		pr_info("%s: dev_state: %d, link_state: %d\n", __func__,
				hba->curr_dev_pwr_mode, hba->uic_link_state);

		exynos_ufs_fmp_enable(hba);
		exynos_ufs_fmp_hba_start(hba);

		unipro_writel(handle, DBG_SUITE1_ENABLE,
				UNIP_PA_DBG_OPTION_SUITE_1);
		unipro_writel(handle, DBG_SUITE2_ENABLE,
				UNIP_PA_DBG_OPTION_SUITE_2);

		if (hba->mcq_enabled)
			exynos_config_mcq(hba);
	} else {
		/* system init */
		ret = exynos_ufs_config_externals(ufs);
		if (ret)
			return ret;
	}

	return ret;
out:
	exynos_ufs_dump_info(hba, &ufs->handle, ufs->dev);

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

	return ret;
}

static int exynos_ufs_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	ktime_t now;
	s64 discharge_period;
	int ret;

	if (ufs->vcc_off_time == -1LL)
		goto resume;

	now = ktime_get();
	discharge_period = ktime_to_ms(
				ktime_sub(now, ufs->vcc_off_time));
	if (discharge_period < LDO_DISCHARGE_GUARANTEE) {
		dev_info(dev, "%s: need to give delay: discharge_period = %lld\n",
				__func__, discharge_period);
		mdelay(LDO_DISCHARGE_GUARANTEE - discharge_period);
	}

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
	struct ufs_cal_param *p = &ufs->cal_param;
	struct irq_inform *vendor_irq = &ufs->vendor_irq;
	/* 50us is a heuristic value, so it could change later */
	u32 ref_gate_margin = (hba->dev_info.wspecversion >= 0x300) ?
		hba->dev_info.clk_gating_wait_us : 45;
	u32 peer_hibern8time, reg;

	peer_hibern8time = unipro_readl(handle, UNIP_PA_HIBERN8TIME);
	peer_hibern8time += 1;
	unipro_writel(handle, peer_hibern8time, UNIP_PA_HIBERN8TIME);

	p->ah8_thinern8_time = peer_hibern8time;
	p->ah8_brefclkgatingwaittime = ref_gate_margin;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* check only at the first init */
	if (!(hba->eh_flags || hba->pm_op_in_progress)) {
		/* sec special features */
		ufs_sec_set_features(hba);
	}
	ufs_sec_config_features(hba);
#endif

	if (hba->mcq_enabled) {
		reg = hci_readl(handle, HCI_UFSHCI_V2P1_CTRL);
		reg &= ~NEXUS_TYPE_SELECT_ENABLE;
		hci_writel(handle, reg, HCI_UFSHCI_V2P1_CTRL);

		/*
		 * disable MCQ_CQ_EVENT_STATUS(CQEE) with ESI
		 * prevent legacy interrupts from being dulicated
		 */

		if (vendor_irq->num) {
			reg = std_readl(handle, REG_INTERRUPT_ENABLE);
			reg &= ~(MCQ_CQ_EVENT_STATUS);
			std_writel(handle, reg, REG_INTERRUPT_ENABLE);

			/* SQ/CQ group config for evt1 */
			if (ufs->cal_param.evt_ver)
				exynos_mcq_ia_config(hba);
		}
	}

	/*
	 * we're here, that means the sequence up
	 */
	dev_info(ufs->dev, "UFS device initialized\n");

	return 0;
}

static void __fixup_dev_quirks(struct ufs_hba *hba)
{
	struct device_node *np = hba->dev->of_node;

	hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS);

	if(of_find_property(np, "support-extended-features", NULL))
		hba->dev_quirks |= UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES;
}

static void exynos_ufs_use_mcq_hook(void *data, struct ufs_hba *hba, bool *use_mcq)
{
}

static void exynos_ufs_mcq_abort(void *data, struct ufs_hba *hba, struct scsi_cmnd *cmd, int *ret)
{

}

static void exynos_ufs_send_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	ufs_perf_op op = UFS_PERF_OP_NONE;
	struct ufs_hw_queue *hwq;
	u32 hwq_num, utag;
	unsigned long flags;

	if (!lrbp)
		return;

	if (is_mcq_enabled(hba)) {
		if (lrbp->task_tag != hba->nutrs - UFSHCD_NUM_RESERVED) {
			if (!lrbp->cmd)
				return;

			utag = blk_mq_unique_tag(scsi_cmd_to_rq(lrbp->cmd));
			hwq_num = blk_mq_unique_tag_to_hwq(utag);
			hwq = &hba->uhq[hwq_num];
		} else {
			hwq = hba->dev_cmd_queue;
		}
		spin_lock(&hwq->sq_lock);
	} else {
		spin_lock_irqsave(&hba->outstanding_lock, flags);
	}

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_UP &&
			ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	/* perf */
	if (lrbp->cmd) {
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

			qd = hba->clk_gating.active_reqs;
			ufs_perf_update(ufs->perf, qd, lrbp, op);
		}

		/* cmd_logging */
		exynos_ufs_cmd_log_start(handle, hba, scmd);
	}

	/*
	 * check if an update is needed. not require protection
	 * because this functions is wrapped with spin lock outside
	 */
	if (lrbp->cmd) {
		if (test_and_set_bit(lrbp->task_tag, &ufs->nexus))
			goto out;
	} else {
		if (!test_and_clear_bit(lrbp->task_tag, &ufs->nexus))
			goto out;
	}
	hci_writel(handle, (u32)ufs->nexus, HCI_UTRL_NEXUS_TYPE);
out:
	ufs->h_state = H_REQ_BUSY;

	if (is_mcq_enabled(hba))
		spin_unlock(&hwq->sq_lock);
	else
		spin_unlock_irqrestore(&hba->outstanding_lock, flags);
}

static void exynos_ufs_register_vendor_hooks(void)
{
	register_trace_android_vh_ufs_compl_command(
			exynos_ufs_compl_nexus_t_xfer_req, NULL);
	register_trace_android_vh_ufs_check_int_errors(__check_int_errors, NULL);
	register_trace_android_vh_ufs_use_mcq_hooks(exynos_ufs_use_mcq_hook, NULL);
	register_trace_android_vh_ufs_mcq_abort(exynos_ufs_mcq_abort, NULL);
	register_trace_android_vh_ufs_send_command(exynos_ufs_send_command, NULL);
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* register vendor hooks */
	ufs_sec_register_vendor_hooks();
#endif

}

/* to make device state active */
static int __device_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

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

static int exynos_ufs_op_runtime_config(struct ufs_hba *hba)
{
	struct ufshcd_mcq_opr_info_t *opr;
	struct ufshcd_res_info *mem_res, *sqdao_res;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int i;

	mem_res = &hba->res[RES_UFS];
	sqdao_res = &hba->res[RES_MCQ_SQD];

	if (!mem_res->base || !sqdao_res->base)
		return -EINVAL;

	for (i = 0; i < OPR_MAX; i++) {
		opr = &hba->mcq_opr[i];
		opr->offset = sqdao_res->resource->start -
			mem_res->resource->start + 0x16 * i;
		opr->stride = 0x1000;
		opr->base = sqdao_res->base + 0x20 * i;
	}

	// select multi circular
	std_writel(handle, 0x1, 0x180);
	pr_info("%s: set CFG value: %08X\n", __func__, std_readl(handle, 0x180));

	ufs_map_vs_mcq(ufs);

	return 0;
}

static void exynos_mcq_write_ctr(struct ufs_hba *hba, u32 val, int i)
{
	u32 reg;

	reg = readl(mcq_opr_base(hba, OPR_CQIS, i) + REG_MCQIACR);
	reg |= MCQIACR_CTR;
	writel(reg, mcq_opr_base(hba, OPR_CQIS, i) + REG_MCQIACR);
}

static irqreturn_t exynos_vendor_mcq_cq_events(struct ufs_hba *hba, int irq)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct irq_inform *vendor_irq = &ufs->vendor_irq;
	struct ufs_hw_queue *hwq;
	int num = 0;

	num = vendor_irq->irq_table[irq - vendor_irq->mcq_irq[0]];

	if (exynos_soc_info.main_rev) {
		exynos_mcq_write_ctr(hba, 0x1, num);

		if (irq == vendor_irq->mcq_irq[6])
			if (exynos_mcq_read_cqis(hba, num + 3))
				num = num + 3;

		if (irq == vendor_irq->mcq_irq[7])
			if (exynos_mcq_read_cqis(hba, num + 1))
				num = num + 1;
	}

	hwq = &hba->uhq[num];
	ufshcd_mcq_write_cqis(hba, 0x1, num);
	ufshcd_mcq_poll_cqe_lock(hba, hwq);

	return IRQ_HANDLED;
}

static irqreturn_t exynos_vendor_mcq_irq(int irq, void *__hba)
{
	irqreturn_t retval = IRQ_NONE;
	struct ufs_hba *hba = __hba;

	retval |= exynos_vendor_mcq_cq_events(hba, irq);

	return retval;
}

/* Resources */
static const struct ufshcd_res_info ufs_res_info[RES_MAX] = {
	{.name = "ufs_mem",},
	{.name = "mcq",},
	/* Submission Queue DAO */
	{.name = "mcq_sqd",},
	/* Submission Queue Interrupt Status */
	{.name = "mcq_sqis",},
	/* Completion Queue DAO */
	{.name = "mcq_cqd",},
	/* Completion Queue Interrupt Status */
	{.name = "mcq_cqis",},
	/* MCQ vendor specific */
	{.name = "mcq_vs",},
};

static int exynos_ufs_config_esi(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct irq_inform *vendor_irq = &ufs->vendor_irq;
	int i, ret = 0;

	if (!vendor_irq->num)
		return FAILED;

	for (i = 0; i < vendor_irq->num; i++) {
		ret = devm_request_irq(hba->dev, vendor_irq->mcq_irq[i],
				exynos_vendor_mcq_irq, IRQF_SHARED, "ufshcd-mcq", hba);
		if (ret) {
			dev_err(hba->dev, "Fail register mcq-irq\n");
			return ret;
		}
		irq_set_affinity_hint(vendor_irq->mcq_irq[i],
				cpumask_of(vendor_irq->levels[i]));
	}

	return ret;
}

static int exynos_ufs_config_resource(struct ufs_hba *hba)
{
	struct platform_device *pdev = to_platform_device(hba->dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufshcd_res_info *res;
	struct resource *res_mem;
	int i, ret = 0;

	memcpy(hba->res, ufs_res_info, sizeof(ufs_res_info));

	for (i = 0; i < RES_MAX; i++) {
		res = &hba->res[i];
		res->resource = platform_get_resource_byname(pdev,
							     IORESOURCE_MEM,
							     res->name);
		if (!res->resource) {
			dev_info(hba->dev, "Resource %s not provided\n", res->name);
			if (i == RES_UFS)
				return -ENOMEM;
			continue;
		} else if (i == RES_UFS) {
			res_mem = res->resource;
			res->base = hba->mmio_base;
			continue;
		} else if (i == RES_MCQ) {
			res->base = hba->mmio_base + STD_MCQ_OFFSET;
			hba->mcq_base = res->base;
			continue;
		}

		res->base = devm_ioremap_resource(hba->dev, res->resource);
		if (IS_ERR(res->base)) {
			dev_err(hba->dev, "Failed to map res %s, err=%d\n",
					 res->name, (int)PTR_ERR(res->base));
			res->base = NULL;
			ret = PTR_ERR(res->base);
			return ret;
		}
	}

	if (ufs->cal_param.evt_ver) {
		/* do not use poll queue */
		hba->nr_hw_queues -= hba->nr_queues[HCTX_TYPE_POLL];
		hba->nr_hw_queues += 1;
		hba->nr_queues[HCTX_TYPE_POLL] = 0;
	}

	return ret;
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
	.setup_task_mgmt = exynos_ufs_set_nexus_t_task_mgmt,
	.link_startup_notify = exynos_ufs_link_startup_notify,
	.event_notify = exynos_ufs_event_notify,
	.program_key = exynos_ufs_fmp_program_key,
	.get_hba_mac = exynos_ufs_get_hba_mac,
	.op_runtime_config = exynos_ufs_op_runtime_config,
	.get_outstanding_cqs = exynos_ufs_get_outstanding_cqs,
	.mcq_config_resource = exynos_ufs_config_resource,
	.config_esi = exynos_ufs_config_esi,
};

static struct ufs_exynos_variant_ops exynos_cal_ops = {
	.gate_clk = __exynos_ufs_gate_clk,
	.ctrl_gpio = exynos_ufs_ctrl_gpio,
	.pad_retention = exynos_ufs_pad_retention,
	.pmu_input_iso = exynos_ufs_pmu_input_iso,
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
	struct irq_inform *vendor_irq = &ufs->vendor_irq;
	struct platform_device *pdev = to_platform_device(dev);
	int i;
	int ret;

	/* Regmap for external regions */
	ret = exynos_ufs_populate_dt_extern(dev, ufs);
	if (ret) {
		dev_err(dev, "%s: Fail to populate dt-pmu\n", __func__);
		goto out;
	}

	/* Get exynos-evt version for featuring. Now get main_rev instead of dt */
	ufs->cal_param.evt_ver = exynos_soc_info.main_rev;
	dev_info(dev, "exynos ufs evt version : %d\n", ufs->cal_param.evt_ver);

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
	if (is_smdk)
		ufs->cal_param.board = is_smdk;
	else
		of_property_read_u8(np, "brd-for-cal", &ufs->cal_param.board);
	pr_info("%s: board type: %d\n", __func__, ufs->cal_param.board);

	of_property_read_u32(np, "mcq-irq,affinity", &ufs->irq_affinity);

	/* select cal overwrite */
	ufs->cal_param.overwrite = 0;
	of_property_read_u8(np, "cal-overwrite", &ufs->cal_param.overwrite);

	ufs->ah8_ahit = 0;
	ufs->ah8_by_h8 = 0;
	if (of_find_property(np, "samsung,support-ah8", NULL)) {
		ufs->ah8_ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 20) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 2);

		if (of_find_property(np, "samsung,run-by-sw-h8", NULL))
			ufs->ah8_by_h8 = 1;
	}

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

	ufs->freq_for_1us_cntval = 0;
	of_property_read_u32(np, "freq-for-1us-cntval", &ufs->freq_for_1us_cntval);

	vendor_irq->num = of_property_count_u32_elems(np, "ufs,affinity-irq");
	if (vendor_irq->num > 0)
		of_property_read_u32_array(np, "ufs,affinity-irq",
				vendor_irq->levels, vendor_irq->num);
	else
		vendor_irq->num = 0;

	for (i = 0; i < vendor_irq->num; i++) {
		vendor_irq->mcq_irq[i] = platform_get_irq(pdev, i + 1);
		if (vendor_irq->mcq_irq[i] < 0) {
			dev_err(dev, "there is no MCQ-IRQ[%d]\n", i);
			break;
		}
	}

	for (i = 0; i < vendor_irq->num; i++) {
		int irq;

		irq = vendor_irq->mcq_irq[i] - vendor_irq->mcq_irq[0];
		vendor_irq->irq_table[irq] = i;
	}
out:
	return ret;
}

static int exynos_ufs_ioremap(struct exynos_ufs *ufs,
				struct platform_device *pdev)
{
	/* Indicators for logs */
	static const char *ufs_region_names[NUM_OF_UFS_MMIO_REGIONS + 1] = {
		"",			/* standard hci */
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

	for (i = 1, p = &ufs->reg_unipro;
			i < NUM_OF_UFS_MMIO_REGIONS + 1; i++, p++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			continue;

		*p = devm_ioremap_resource(dev, res);
		if (!*p) {
			ret = -ENOMEM;
			break;
		}
		dev_info(dev, "%-10s 0x%llx\n", ufs_region_names[i], (u64)*p);
	}

	if (ret)
		dev_err(dev, "Fail to ioremap for %s\n",
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

#define HOST		0
#define DEVICE		1

#define PA_SCRAMBLING	0x1585

static u32 exynos_eom40_pcs_read(struct exynos_ufs *ufs, u32 ofs, u32 lane, int dir)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufs_hba *hba = ufs->hba;
	u32 val = 0;

	if (dir == HOST) {
		val = pcs_readl(handle, ofs | (lane << 15));
	} else {
		ofs =(ofs - RX_EYEMON_CAPA) / 4 + RX_EYEMON_DEVICE_CAPA;
		ufshcd_dme_get_attr(hba, UIC_ARG_MIB(ofs) |
				(RX_EYEMON_DEVICE_LANE_BASE + lane), &val, dir);
	}
	return val;
}

static void exynos_eom40_pcs_write(struct exynos_ufs *ufs, u32 val, u32 ofs,
		u32 lane, int dir)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufs_hba *hba = ufs->hba;

	if (dir == HOST) {
		pcs_writel(handle, val, ofs | (lane << 15));
	} else {
		ofs =(ofs - RX_EYEMON_CAPA) / 4 + RX_EYEMON_DEVICE_CAPA;
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(ofs) |
				(RX_EYEMON_DEVICE_LANE_BASE + lane),
				ATTR_SET_NOR, val, dir);
	}
}

static void exynos_pcs_onoff(struct ufs_vs_handle *handle, bool on)
{
	pcs_writel(handle, on, PCS_RX0_STD_MUX_SEL);
	pcs_writel(handle, on, PCS_RX1_STD_MUX_SEL);
}

static int exynos_ufs_eom40_prepare_store(struct exynos_ufs *ufs, const char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct uic_pwr_mode *pmd = &ufs->device_pmd_parm;
	struct ufs_hba *hba = ufs->hba;
	struct ufs_eom40_data *data = &ufs->eom;
	int dir, status, lane;
	int ret;

	ret = sscanf(buf, "%d %d %d", &dir, &lane, &status);

	if (dir == HOST) {
		if (!status) {
			ufs_call_cal(ufs, ufs_cal_disable_eom);
			return 0;
		}
	}

	if (dir == HOST)
		exynos_pcs_onoff(handle, 1);

	status = exynos_eom40_pcs_read(ufs, RX_EYEMON_CAPA, lane, dir);
	if (!status)
		pr_err("%s: do not support Eye monitor 4.0!!\n", __func__);

	data->t_max_step = exynos_eom40_pcs_read(ufs, RX_EYEMON_T_MAX_STEP_CAPA,
			lane, dir);
	data->t_max_offset = exynos_eom40_pcs_read(ufs, RX_EYEMON_T_MAX_OFFSET_CAPA,
			lane, dir);
	data->v_max_step = exynos_eom40_pcs_read(ufs, RX_EYEMON_V_MAX_STEP_CAPA,
			lane, dir);
	data->v_max_offset = exynos_eom40_pcs_read(ufs, RX_EYEMON_V_MAX_OFFSET_CAPA,
			lane, dir);
	exynos_eom40_pcs_write(ufs, 1, RX_EYEMON_ENABLE, lane, dir);

	if (dir == HOST)
		exynos_pcs_onoff(handle, 0);

	if (data->result)
		devm_kfree(ufs->dev, data->result);
	data->result = devm_kcalloc(ufs->dev, (data->t_max_step * 2) *
			(data->v_max_step * 2),	sizeof(struct ufs_eom40_result),
			GFP_KERNEL);

	// unipro power mode chage with ADAPT_INIT
	ufshcd_dme_configure_adapt(hba, pmd->gear, 1);

	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_SCRAMBLING), ATTR_SET_NOR,
			0, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_HSSERIES), ATTR_SET_NOR,
			pmd->hs_series, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_TXTERMINATION), ATTR_SET_NOR,
			0x1, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_RXTERMINATION), ATTR_SET_NOR,
			0x1, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_TXGEAR), ATTR_SET_NOR,
			pmd->gear, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_RXGEAR), ATTR_SET_NOR,
			pmd->gear, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			ATTR_SET_NOR, pmd->lane, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			ATTR_SET_NOR, pmd->lane, HOST);
	ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_PWRMODE), ATTR_SET_NOR,
			(TX_PWRMODE(FAST_MODE) | RX_PWRMODE(FAST_MODE)), HOST);

	if (dir == HOST) {
		ret = ufs_call_cal(ufs, ufs_cal_enable_eom);
		if (ret)
			return ret;
	}

	return 0;
}

static ssize_t
exynos_ufs_eom40_prepare_show(struct exynos_ufs *ufs, char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_eom40_data *data = &ufs->eom;

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			data->v_max_step, data->v_max_offset, data->t_max_step,
			data->t_max_offset);
}

static struct exynos_ufs_sysfs_attr ufs_s_eom40_prepare = {
	.attr = {.name = "ufs_eom40_prepare", .mode = 0644},
	.show = exynos_ufs_eom40_prepare_show,
	.store = exynos_ufs_eom40_prepare_store,
};

static int exynos_ufs_eom40_start_store(struct exynos_ufs *ufs,
				const char *buf, exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct uic_pwr_mode *pmd = &ufs->device_pmd_parm;
	struct ufs_hba *hba = ufs->hba;
	int t_setting, v_setting, dir, lane;
	int target_t_cnt = 62;
	int v_phase, v_ref, index;
	struct ufs_eom40_data *data = &ufs->eom;
	int ret;
	u32 reg;
	int v_set, t_set;

	ret = sscanf(buf, "%d %d %d %d", &dir, &lane, &v_phase, &v_set);
	if (!ret)
		return -EINVAL;

	reg = std_readl(handle, REG_INTERRUPT_ENABLE);
	reg &= ~(UIC_POWER_MODE);
	std_writel(handle, reg, REG_INTERRUPT_ENABLE);

	t_set = data->t_max_step;
	for (v_ref = 0; v_ref < data->t_max_step * 2 - 1; v_ref++) {
		if (v_ref / data->t_max_step) {
			t_setting = t_set;
			t_set += 1;
		} else {
			t_setting = t_set | (1 << 6);
			t_set -= 1;
		}

		if (v_phase / data->v_max_step)
			v_setting = v_set | (1 << 6);
		else
			v_setting = v_set;

		if (dir == HOST)
			exynos_pcs_onoff(handle, 1);

		exynos_eom40_pcs_write(ufs, target_t_cnt, RX_EYEMON_TARGET_TEST_CNT,
				lane, dir);
		exynos_eom40_pcs_write(ufs, t_setting, RX_EYEMON_T_STEP,
				lane, dir);
		exynos_eom40_pcs_write(ufs, v_setting, RX_EYEMON_V_STEP,
				lane, dir);

		exynos_eom40_pcs_write(ufs, 1, RX_EYEMON_START, lane, dir);

		if (dir == HOST)
			exynos_pcs_onoff(handle, 0);

		// Unipro power mode chagne with ADAPT=3
		ufshcd_dme_configure_adapt(hba, pmd->gear, 3);

		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_SCRAMBLING), ATTR_SET_NOR,
				1, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_HSSERIES), ATTR_SET_NOR,
				pmd->hs_series, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_TXTERMINATION),
				ATTR_SET_NOR, 0x1, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_RXTERMINATION),
				ATTR_SET_NOR, 0x1, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_TXGEAR), ATTR_SET_NOR,
				pmd->gear, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_RXGEAR), ATTR_SET_NOR,
				pmd->gear, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
				ATTR_SET_NOR, pmd->lane, HOST);
		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
				ATTR_SET_NOR, pmd->lane, HOST);

		ufshcd_dme_set_attr(hba, UIC_ARG_MIB(PA_PWRMODE), ATTR_SET_NOR,
				(TX_PWRMODE(FAST_MODE) | RX_PWRMODE(FAST_MODE)),
				HOST);

		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &reg);

		// eom40_save
		if (dir == HOST)
			exynos_pcs_onoff(handle, 1);
		do {
			reg = exynos_eom40_pcs_read(ufs, RX_EYEMON_START, lane, dir);
		} while (reg);
/*
		reg = exynos_eom40_pcs_read(ufs, RX_EYEMON_TARGET_TESTED_CNT |
				lane, dir);
*/
		index = (v_phase * (data->t_max_step * 2)) + v_ref;

		data->result[index].v_phase = v_phase;
		data->result[index].v_ref = v_ref;
		data->result[index].v_err = exynos_eom40_pcs_read(ufs, RX_EYEMON_ERROR_CNT,
				lane, dir);

		if (dir == HOST)
			exynos_pcs_onoff(handle, 0);
	}

	reg = std_readl(handle, REG_INTERRUPT_ENABLE);
	reg |= UIC_POWER_MODE;
	std_writel(handle, reg, REG_INTERRUPT_ENABLE);


	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom40_start = {
	.attr = {.name = "ufs_eom40_start", .mode = 0644},
	.store = exynos_ufs_eom40_start_store,
};

static int exynos_ufs_eom40_get_data_store(struct exynos_ufs *ufs, const char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_eom40_data *data = &ufs->eom;
	int ret;

	ret = sscanf(buf, "%d", &data->start);
	if (!ret)
		return -EINVAL;

	return 0;
}

static ssize_t
exynos_ufs_eom40_get_data_show(struct exynos_ufs *ufs, char *buf,
		exynos_ufs_param_id id)
{
	struct ufs_eom40_data *data = &ufs->eom;
	struct  ufs_eom40_result *result = data->result;
	ssize_t len = 0;
	int i;

	result += (data->start * (data->t_max_step * 2));

	for (i = 0; i < data->t_max_step * 2 - 1; i++) {
		len += snprintf(buf + len, PAGE_SIZE, "%u %u %u\n",
				result->v_phase, result->v_ref, result->v_err);
		result++;
	}

	return len;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom40_get = {
	.attr = {.name = "ufs_eom40_get", .mode = 0644},
	.show = exynos_ufs_eom40_get_data_show,
	.store = exynos_ufs_eom40_get_data_store,
};

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

static void exynos_mcq_cq_stop(struct ufs_hba *hba)
{
	u32 i;

	for (i = 0; i < hba->nr_hw_queues; i++)
		writel(0, mcq_opr_base(hba, OPR_CQIS, i) + REG_CQIE);
}

static int exynos_ufs_sysfs_mon_store(struct exynos_ufs *ufs, const char *buf,
				      exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufs_hba *hba = ufs->hba;
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
		reg &= ~(UTP_TRANSFER_REQ_COMPL | MCQ_CQ_EVENT_STATUS);
		std_writel(handle, reg, REG_INTERRUPT_ENABLE);

		if (is_mcq_enabled(hba))
			exynos_mcq_cq_stop(hba);
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
	struct ufs_eom_result_s *p;
	int val;
	int i;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (!ret)
		return -EINVAL;

	if (val) {
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
	} else {
		goto fail_mem;
	}

	ret = ufs_call_cal(ufs, ufs_cal_eom);
	if (ret)
		dev_err(ufs->dev, "Fail to store eom data\n");

fail_mem:
	for (i = 0; i < MAX_LANE; i++) {
		if (ufs->cal_param.eom[i])
			devm_kfree(ufs->dev, ufs->cal_param.eom[i]);
		ufs->cal_param.eom[i] = NULL;
	}

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

static ssize_t exynos_ufs_update_cal_show(struct exynos_ufs *ufs, char *buf,
					     exynos_ufs_param_id id)
{
	struct ufs_cal_param *p = &ufs->cal_param;

	if (!p)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "read cal addr: %#x, value: %#x\n",
			p->addr, p->value);
}

static int exynos_ufs_update_cal_store(struct exynos_ufs *ufs, const char *buf,
					     exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufs_cal_param *p = &ufs->cal_param;
	char cmd[6];
	u32 addr, value;
	int ret;

	ret = sscanf(buf, "%s %x %x", cmd, &addr, &value);
	if (!ret)
		return -EINVAL;

	/*
	 * Only for following values are allowed to be changed
	 * Tx deemp, Tx Amplitude, Rx Rs, Rx Cs
	 */
	if (addr != 0x1054 && addr != 0x101C && addr != 0x19B0 && addr != 0x1370)
		return -EINVAL;

	if (!strcmp("write", cmd)) {
		p->addr = addr;
		p->value = value;

		ret = ufs_call_cal(ufs, ufs_cal_store);
		if (ret)
			return -EINVAL;

		if (ufs->always_on) {
			ret = ufs_call_cal(ufs, ufs_cal_snr_store);
			if (ret)
				return -EINVAL;
		}
	} else if (!strcmp("read", cmd)) {
		p->addr = addr;


		ret = ufs_call_cal(ufs, ufs_cal_show);
		if (ret)
			return -EINVAL;

		pr_info("%s: read cal addr: %#x, value: %#x\n", __func__,
				p->addr, p->value);

		if (ufs->always_on) {
			ret = ufs_call_cal(ufs, ufs_cal_snr_show);
			if (ret)
				return -EINVAL;

			pr_info("%s: read snr_cal addr: %#x, value: %#x\n",
				__func__, p->addr, p->value);
		}
	} else if (!strcmp("reinit", cmd)) {
		unipro_writel(handle, BIT_POS_DBG_DL_RX_INFO_FORCE |
			DL_RX_INFO_TYPE_ERROR_DETECTED | RX_BUFFER_OVERFLOW,
			UNIP_DBG_RX_INFO_CONTROL_DIRECT);

	} else {
		pr_err("%s: not define cmd!!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_update_cal = {
	.attr = { .name ="update_cal", .mode = 0666 },
	.show = exynos_ufs_update_cal_show,
	.store = exynos_ufs_update_cal_store,
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
		return snprintf(buf, PAGE_SIZE, "%d\n", -EINVAL);

	if (!perf->gear_scale_sup)
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

	if (perf == NULL || !perf->gear_scale_sup)
		return -EINVAL;

	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;

	if (!value)
		perf->active_cnt++;
	else
		perf->active_cnt--;

	pr_info("%s: gear scale %sable request: %d\n",
			__func__, value ? "en" : "dis", perf->active_cnt);

	if (perf->active_cnt < 0)
		perf->active_cnt = 0;

	ret = ufs_perf_request_boost_mode(&perf->gear_scale_rq,
			(perf->active_cnt > 0) ? REQUEST_BOOST : REQUEST_NORMAL);

	if (!value && (perf->active_cnt == 1))
		mod_timer(&perf->boost_timer, jiffies
				+ msecs_to_jiffies(30 * 60 * 1000));
	else if (perf->active_cnt == 0)
		del_timer(&perf->boost_timer);

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
	&ufs_s_eom40_get.attr,
	&ufs_s_eom40_start.attr,
	&ufs_s_eom40_prepare.attr,
	&ufs_s_update_cal.attr,
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

static void ufs_tm_cmd_handler(struct work_struct *work)
{
	struct exynos_ufs *ufs =
		container_of(work, struct exynos_ufs, tm_work);
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufs_hba *hba = ufs->hba;
	struct utp_task_req_desc *treq;
	enum utp_ocs ocs;
	struct request *req;
	u32 cnt = 100;

	while (cnt--) {
		if (test_bit(ufs->tm_tag, &hba->outstanding_tasks))
			break;
		usleep_range(100, 200);
	}
	if (!cnt)
		return;

	treq = &hba->utmrdl_base_addr[ufs->tm_tag];
	if (!treq)
		return;

	cnt = 100;
	while (cnt--) {
		ocs = le32_to_cpu(treq->header.dword_2) & MASK_OCS;

		if (ocs != OCS_INVALID_COMMAND_STATUS)
			break;
		usleep_range(100, 200);
	}

	std_writel(handle, 1 << ufs->tm_tag, REG_UTP_TASK_REQ_DOOR_BELL);
	req = hba->tmf_rqs[ufs->tm_tag];
	if (!req)
		return;

	if (cnt)
		complete(req->end_io_data);
	else
		pr_err("%s: did not update OCS!!\n", __func__);
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
static int exynos_ufs_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_ufs *ufs = container_of(nb, struct exynos_ufs, itmon_nb);
	struct ufs_hba *hba = ufs->hba;
	struct itmon_notifier *itmon_data = nb_data;
	int size;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	size = sizeof("UFS") - 1;
	if ((itmon_data->master && !strncmp("UFS", itmon_data->master, size)) ||
			(itmon_data->dest && !strncmp("UFS", itmon_data->dest,
				size))) {
		exynos_ufs_dump_debug_info(hba);
	}

	return NOTIFY_DONE;
}
#endif

static int exynos_ufs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_ufs *ufs;
	struct ufs_cal_param *p;
	int ret;
	int i;

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

	/* init cal */
	ufs->cal_param.handle = &ufs->handle;
	ret = ufs_call_cal(ufs, ufs_cal_init);
	if (ret)
		return ret;

	if (ufs->always_on)
		ufs_call_cal(ufs, ufs_cal_snr_init);

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

	ufs->tm_wq = create_singlethread_workqueue("ufs_tm_wq");
	if (!ufs->tm_wq) {
		pr_err("%s: failed to create tm workqueue\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	/* async resume */
	INIT_WORK(&ufs->resume_work, __ufs_resume_async);
	INIT_WORK(&ufs->tm_work, ufs_tm_cmd_handler);

	/* init vcc-off timestamp */
	ufs->vcc_off_time = -1LL;

	/* register vendor hooks: compl_commmand, etc */
	exynos_ufs_register_vendor_hooks();

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	/* add itmon notifier */
	ufs->itmon_nb.notifier_call = exynos_ufs_itmon_notifier;
	itmon_notifier_chain_register(&ufs->itmon_nb);
#endif

	// save&restore
	p = &ufs->cal_param;
	p->vops = &exynos_cal_ops;
	if (ufs->always_on) {
		unsigned long value;

		// have to add save register func using cal
		ret = exynos_smc_readsfr(EXYNOS_PHY_BIAS, &p->m_phy_bias);
		if (ret) {
			pr_err("%s: Fail to smc call[%d]", __func__, ret);
			return ret;
		}

		for (i = 0; i < 64; i += 4) {
			ret = exynos_smc_readsfr(EXYNOS_PMA_OTP + i, &value);
			if (ret) {
				pr_err("%s: Fail to smc call[%d]", __func__, ret);
				return ret;
			}

			p->pma_otp[i] = (u8)FIELD_GET(0xFF, value);
			p->pma_otp[i + 1] = (u8)(FIELD_GET(0xFF00, value) >> 8);
			p->pma_otp[i + 2] = (u8)(FIELD_GET(0xFF0000, value) >> 16);
			p->pma_otp[i + 3] = (u8)(FIELD_GET(0xFF000000, value) >> 24);
		}

		// mphy off with save&restore
		p->snr_phy_power_on = false;

		// save output state to high
		p->save_and_restore_mode = SAVE_MODE;
		exynos_ufs_ctrl_gpio(p);
		p->save_and_restore_mode = RESTORE_MODE;
		exynos_ufs_ctrl_gpio(p);
		p->save_and_restore_mode = 0;
	}

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
