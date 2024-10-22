// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/bitfield.h>
#include <linux/bvec.h>
#include <linux/clk.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/printk.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/rpmb.h>
#include <linux/sched/clock.h>
#include <linux/stddef.h>
#include <linux/tracepoint.h>
#include <scsi/scsi_proto.h>
#include <scsi/scsi_dbg.h>
#include <ufs/ufs_quirks.h>
#include <ufs/ufshcd.h>
#include <ufs/unipro.h>

#include "ufshcd-crypto.h"
#include "ufshcd-pltfrm.h"
#include "ufshcd-priv.h"

/* MediaTek UFS facilities */
#include "ufs-mediatek-btag.h"
#include "ufs-mediatek-dbg.h"
#include "ufs-mediatek-mimic.h"
#include "ufs-mediatek-priv.h"
#include "ufs-mediatek-rpmb.h"
#include "ufs-mediatek-sip.h"
#include "ufs-mediatek-sysfs.h"
#include "ufs-mediatek.h"

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
#include "ufs-sec-feature.h"
#endif

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
#include <clk-mtk.h>
static struct ufs_hba *ufshba;
#endif

/* Power Throttling */
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif

#define UFS_WAKE_LOCK_TIMEOUT_MS	5000

extern void mt_irq_dump_status(unsigned int irq);
static int ufs_mtk_config_mcq(struct ufs_hba *hba, bool irq);
static void _ufs_mtk_clk_scale(struct ufs_hba *hba, bool scale_up);

#define CREATE_TRACE_POINTS
#include "ufs-mediatek-trace.h"
#undef CREATE_TRACE_POINTS

#define MAX_SUPP_MAC 64
#define MCQ_QUEUE_OFFSET(c) ((((c) >> 16) & 0xFF) * 0x200)
#define MCQ_QCFG_SIZE 0x40
#define MCQ_CFG_n(r, i) ((r) + MCQ_QCFG_SIZE * (i))
#define QUEUE_EN_OFFSET 31
#define QUEUE_ID_OFFSET 16

static const struct ufs_dev_quirk ufs_mtk_dev_fixups[] = {
	{ .wmanufacturerid = UFS_ANY_VENDOR,
	  .model = UFS_ANY_MODEL,
	  .quirk = UFS_DEVICE_QUIRK_DELAY_AFTER_LPM |
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM },
	{ .wmanufacturerid = UFS_VENDOR_SKHYNIX,
	  .model = "H9HQ21AFAMZDAR",
	  .quirk = UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES },
	{}
};

static const struct of_device_id ufs_mtk_of_match[] = {
	{ .compatible = "mediatek,mt8183-ufshci" },
	{},
};

/*
 * Details of UIC Errors
 */
static const char *const ufs_uic_err_str[] = {
	"PHY Adapter Layer",
	"Data Link Layer",
	"Network Link Layer",
	"Transport Link Layer",
	"DME"
};

static const char *const ufs_uic_pa_err_str[] = {
	"PHY error on Lane 0",
	"PHY error on Lane 1",
	"PHY error on Lane 2",
	"PHY error on Lane 3",
	"Generic PHY Adapter Error. This should be the LINERESET indication"
};

static const char *const ufs_uic_dl_err_str[] = {
	"NAC_RECEIVED",
	"TCx_REPLAY_TIMER_EXPIRED",
	"AFCx_REQUEST_TIMER_EXPIRED",
	"FCx_PROTECTION_TIMER_EXPIRED",
	"CRC_ERROR",
	"RX_BUFFER_OVERFLOW",
	"MAX_FRAME_LENGTH_EXCEEDED",
	"WRONG_SEQUENCE_NUMBER",
	"AFC_FRAME_SYNTAX_ERROR",
	"NAC_FRAME_SYNTAX_ERROR",
	"EOF_SYNTAX_ERROR",
	"FRAME_SYNTAX_ERROR",
	"BAD_CTRL_SYMBOL_TYPE",
	"PA_INIT_ERROR",
	"PA_ERROR_IND_RECEIVED",
	"PA_INIT"
};

static bool ufs_mtk_is_boost_crypt_enabled(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BOOST_CRYPT_ENGINE);
}

static bool ufs_mtk_is_va09_supported(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_VA09_PWR_CTRL);
}

static bool ufs_mtk_is_broken_vcc(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BROKEN_VCC);
}

static bool ufs_mtk_is_pmc_via_fastauto(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return (host->caps & UFS_MTK_CAP_PMC_VIA_FASTAUTO);
}

static bool ufs_mtk_is_tx_skew_fix(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_TX_SKEW_FIX);
}

static bool ufs_mtk_is_rtff_mtcmos(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_RTFF_MTCMOS);
}

static bool ufs_mtk_is_clk_scale_ready(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_clk *mclk = &host->mclk;

	return mclk->ufs_sel_clki &&
		mclk->ufs_sel_max_clki &&
		mclk->ufs_sel_min_clki;
}

static bool ufs_mtk_is_allow_vccqx_lpm(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_ALLOW_VCCQX_LPM);
}

static bool ufs_mtk_is_mphy_dump(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_MPHY_DUMP);
}

static bool ufs_mtk_is_bypass_vccqx_lpm(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BYPASS_VCCQX_LPM);
}

static void ufs_mtk_cfg_unipro_cg(struct ufs_hba *hba, bool enable)
{
	u32 tmp;

	if (enable) {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp |
		      (1 << RX_SYMBOL_CLK_GATE_EN) |
		      (1 << SYS_CLK_GATE_EN) |
		      (1 << TX_CLK_GATE_EN);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp & ~(1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	} else {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp & ~((1 << RX_SYMBOL_CLK_GATE_EN) |
			      (1 << SYS_CLK_GATE_EN) |
			      (1 << TX_CLK_GATE_EN));
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp | (1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	}
}

static void ufs_mtk_crypto_enable(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

	ufs_mtk_crypto_ctrl(res, 1);
	if (res.a0) {
		dev_info(hba->dev, "%s: crypto enable failed, err: %lu\n",
			 __func__, res.a0);
		hba->caps &= ~UFSHCD_CAP_CRYPTO;
	}
}

static void ufs_mtk_host_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_check_device_stuck();
#endif

	reset_control_assert(host->hci_reset);
	reset_control_assert(host->crypto_reset);
	reset_control_assert(host->unipro_reset);
	reset_control_assert(host->mphy_reset);

	usleep_range(100, 110);

	reset_control_deassert(host->unipro_reset);
	reset_control_deassert(host->crypto_reset);
	reset_control_deassert(host->hci_reset);
	reset_control_deassert(host->mphy_reset);

	/* restore mphy setting aftre mphy reset */
	if (host->mphy_reset)
		ufs_mtk_mphy_ctrl(UFS_MPHY_RESTORE, res);
}

static void ufs_mtk_init_reset_control(struct ufs_hba *hba,
				       struct reset_control **rc,
				       char *str)
{
	*rc = devm_reset_control_get(hba->dev, str);
	if (IS_ERR(*rc)) {
		dev_info(hba->dev, "Failed to get reset control %s: %ld\n",
			 str, PTR_ERR(*rc));
		*rc = NULL;
	}
}

static void ufs_mtk_init_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufs_mtk_init_reset_control(hba, &host->hci_reset,
				   "hci_rst");
	ufs_mtk_init_reset_control(hba, &host->unipro_reset,
				   "unipro_rst");
	ufs_mtk_init_reset_control(hba, &host->crypto_reset,
				   "crypto_rst");
	/*
	ufs_mtk_init_reset_control(hba, &host->mphy_reset,
				   "mphy_rst");
	*/
}

static int ufs_mtk_hce_enable_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	unsigned long flags;

	if (status == PRE_CHANGE) {
		if (host->unipro_lpm) {
			hba->vps->hba_enable_delay_us = 0;
		} else {
			hba->vps->hba_enable_delay_us = 600;
			ufs_mtk_host_reset(hba);
		}

		if (hba->caps & UFSHCD_CAP_CRYPTO)
			ufs_mtk_crypto_enable(hba);

		if (host->caps & UFS_MTK_CAP_DISABLE_AH8) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0,
				      REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock,
					       flags);

			hba->capabilities &= ~MASK_AUTO_HIBERN8_SUPPORT;
			hba->ahit = 0;
		}

		/*
		 * Turn on CLK_CG early to bypass abnormal ERR_CHK signal
		 * to prevent host hang issue
		 */
		ufshcd_writel(hba,
			      ufshcd_readl(hba, REG_UFS_XOUFS_CTRL) | 0x80,
			      REG_UFS_XOUFS_CTRL);

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
		/* Turn on H8 monitor */
		ufshcd_rmwl(hba, MON_EN, MON_EN, REG_UFS_MMIO_OPT_CTRL_0);
#endif

		/* DDR_EN setting */
		if (host->ip_ver >= IP_VER_MT6989) {
			ufshcd_rmwl(hba, UFS_MASK(0x7FFF, 8),
				0x453000, REG_UFS_MMIO_OPT_CTRL_0);
		}

	}

	return 0;
}

static int ufs_mtk_bind_mphy(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	int err = 0;

	host->mphy = devm_of_phy_get_by_index(dev, np, 0);

	if (host->mphy == ERR_PTR(-EPROBE_DEFER)) {
		/*
		 * UFS driver might be probed before the phy driver does.
		 * In that case we would like to return EPROBE_DEFER code.
		 */
		err = -EPROBE_DEFER;
		dev_info(dev,
			 "%s: required phy hasn't probed yet. err = %d\n",
			__func__, err);
	} else if (IS_ERR(host->mphy)) {
		err = PTR_ERR(host->mphy);
		if (err != -ENODEV) {
			dev_info(dev, "%s: PHY get failed %d\n", __func__,
				 err);
		}
	}

	if (err)
		host->mphy = NULL;
	/*
	 * Allow unbound mphy because not every platform needs specific
	 * mphy control.
	 */
	if (err == -ENODEV)
		err = 0;

	return err;
}

static int ufs_mtk_setup_ref_clk(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;
	ktime_t timeout, time_checked;
	u32 value;

	if (host->ref_clk_enabled == on)
		return 0;

	ufs_mtk_ref_clk_notify(on, PRE_CHANGE, res);

	if (on) {
		ufshcd_writel(hba, REFCLK_REQUEST, REG_UFS_REFCLK_CTRL);
	} else {
		ufshcd_delay_us(host->ref_clk_gating_wait_us, 10);
		ufshcd_writel(hba, REFCLK_RELEASE, REG_UFS_REFCLK_CTRL);
	}

	/*
	 * Make sure that ref-clk on/off control register
	 * is writed done before read it.
	 */
	mb();

	/* Wait for ack */
	timeout = ktime_add_us(ktime_get(), REFCLK_REQ_TIMEOUT_US);
	do {
		time_checked = ktime_get();
		value = ufshcd_readl(hba, REG_UFS_REFCLK_CTRL);

		/* Wait until ack bit equals to req bit */
		if (((value & REFCLK_ACK) >> 1) == (value & REFCLK_REQUEST))
			goto out;

		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	dev_err(hba->dev, "missing ack of refclk req, reg: 0x%x\n", value);

	/*
	 * If clock on timeout, assume clock is off, notify tfa do clock
	 * off setting.(keep DIFN disable, release resource)
	 * If clock off timeout, assume clock will off finally,
	 * set ref_clk_enabled directly.(keep DIFN disable, keep resource)
	 */
	if (on)
		ufs_mtk_ref_clk_notify(false, POST_CHANGE, res);
	else
		host->ref_clk_enabled = false;

	return -ETIMEDOUT;

out:
	host->ref_clk_enabled = on;
	if (on)
		ufshcd_delay_us(host->ref_clk_ungating_wait_us, 10);

	ufs_mtk_ref_clk_notify(on, POST_CHANGE, res);

	return 0;
}

static void ufs_mtk_setup_ref_clk_wait_us(struct ufs_hba *hba,
					  u16 gating_us)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (hba->dev_info.clk_gating_wait_us) {
		host->ref_clk_gating_wait_us =
			hba->dev_info.clk_gating_wait_us;
	} else {
		host->ref_clk_gating_wait_us = gating_us;
	}

	host->ref_clk_ungating_wait_us = REFCLK_DEFAULT_WAIT_US;
}

static void ufs_mtk_dbg_sel(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (((host->ip_ver >> 16) & 0xFF) >= 0x36) {
		ufshcd_writel(hba, 0x820820, REG_UFS_DEBUG_SEL);
		ufshcd_writel(hba, 0x0, REG_UFS_DEBUG_SEL_B0);
		ufshcd_writel(hba, 0x55555555, REG_UFS_DEBUG_SEL_B1);
		ufshcd_writel(hba, 0xaaaaaaaa, REG_UFS_DEBUG_SEL_B2);
		ufshcd_writel(hba, 0xffffffff, REG_UFS_DEBUG_SEL_B3);
	} else {
		ufshcd_writel(hba, 0x20, REG_UFS_DEBUG_SEL);
	}
}

static void ufs_mtk_wait_idle_state(struct ufs_hba *hba,
			    unsigned long retry_ms)
{
	u64 timeout, time_checked;
	u32 val, sm;
	bool wait_idle;

	/* cannot use plain ktime_get() in suspend */
	timeout = ktime_get_mono_fast_ns() + retry_ms * 1000000UL;

	/* wait a specific time after check base */
	udelay(10);
	wait_idle = false;

	do {
		time_checked = ktime_get_mono_fast_ns();
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);

		sm = val & 0x1f;

		/*
		 * if state is in H8 enter and H8 enter confirm
		 * wait until return to idle state.
		 */
		if ((sm >= VS_HIB_ENTER) && (sm <= VS_HIB_EXIT)) {
			wait_idle = true;
			udelay(50);
			continue;
		} else if (!wait_idle)
			break;

		if (wait_idle && (sm == VS_HCE_BASE))
			break;
	} while (time_checked < timeout);

	if (wait_idle && sm != VS_HCE_BASE)
		dev_info(hba->dev, "wait idle tmo: 0x%x\n", val);
}

static int ufs_mtk_wait_link_state(struct ufs_hba *hba, u32 state,
				   unsigned long max_wait_ms)
{
	ktime_t timeout, time_checked;
	u32 val;

	timeout = ktime_add_ms(ktime_get(), max_wait_ms);
	do {
		time_checked = ktime_get();
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		val = val >> 28;

		if (val == state)
			return 0;

		/* Sleep for max. 200us */
		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	if (val == state)
		return 0;

	return -ETIMEDOUT;
}

static void ufs_mtk_pm_qos(struct ufs_hba *hba, bool qos_en)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host && host->pm_qos_init) {
		if (qos_en)
			cpu_latency_qos_update_request(
				&host->pm_qos_req, 0);
		else
			cpu_latency_qos_update_request(
				&host->pm_qos_req,
				PM_QOS_DEFAULT_VALUE);
	}
}

static int ufs_mtk_mphy_power_on(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct phy *mphy = host->mphy;
	struct arm_smccc_res res;
	int ret = 0;

	if (!mphy || !(on ^ host->mphy_powered_on))
		return 0;

	if (on) {
		if (ufs_mtk_is_va09_supported(hba)) {
			ret = regulator_enable(host->reg_va09);
			if (ret < 0)
				goto out;
			/* wait 200 us to stablize VA09 */
			usleep_range(200, 210);
			ufs_mtk_va09_pwr_ctrl(res, 1);
		}
		phy_power_on(mphy);
	} else {
		phy_power_off(mphy);
		if (ufs_mtk_is_va09_supported(hba)) {
			ufs_mtk_va09_pwr_ctrl(res, 0);
			ret = regulator_disable(host->reg_va09);
		}
	}
out:
	if (ret) {
		dev_info(hba->dev,
			 "failed to %s va09: %d\n",
			 on ? "enable" : "disable",
			 ret);
	} else {
		host->mphy_powered_on = on;
	}

	return ret;
}

static int ufs_mtk_get_host_clk(struct device *dev, const char *name,
				struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk))
		err = PTR_ERR(clk);
	else
		*clk_out = clk;

	return err;
}

static void ufs_mtk_boost_crypt(struct ufs_hba *hba, bool boost)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_crypt_cfg *cfg;
	struct regulator *reg;
	int volt, ret;

	if (!ufs_mtk_is_boost_crypt_enabled(hba))
		return;

	cfg = host->crypt;
	volt = cfg->vcore_volt;
	reg = cfg->reg_vcore;

	ret = clk_prepare_enable(cfg->clk_crypt_mux);
	if (ret) {
		dev_info(hba->dev, "clk_prepare_enable(): %d\n",
			 ret);
		return;
	}

	if (boost) {
		ret = regulator_set_voltage(reg, volt, INT_MAX);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set vcore to %d\n", volt);
			goto out;
		}

		ret = clk_set_parent(cfg->clk_crypt_mux,
				     cfg->clk_crypt_perf);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set clk_crypt_perf\n");
			regulator_set_voltage(reg, 0, INT_MAX);
			goto out;
		}
	} else {
		ret = clk_set_parent(cfg->clk_crypt_mux,
				     cfg->clk_crypt_lp);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set clk_crypt_lp\n");
			goto out;
		}

		ret = regulator_set_voltage(reg, 0, INT_MAX);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set vcore to MIN\n");
		}
	}
out:
	clk_disable_unprepare(cfg->clk_crypt_mux);
}

static int ufs_mtk_init_host_clk(struct ufs_hba *hba, const char *name,
				 struct clk **clk)
{
	int ret;

	ret = ufs_mtk_get_host_clk(hba->dev, name, clk);
	if (ret) {
		dev_info(hba->dev, "%s: failed to get %s: %d", __func__,
			 name, ret);
	}

	return ret;
}

static void ufs_mtk_init_boost_crypt(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_crypt_cfg *cfg;
	struct device *dev = hba->dev;
	struct regulator *reg;
	u32 volt;

	host->crypt = devm_kzalloc(dev, sizeof(*(host->crypt)),
				   GFP_KERNEL);
	if (!host->crypt)
		goto disable_caps;

	reg = devm_regulator_get_optional(dev, "dvfsrc-vcore");
	if (IS_ERR(reg)) {
		dev_info(dev, "failed to get dvfsrc-vcore: %ld",
			 PTR_ERR(reg));
		goto disable_caps;
	}

	if (of_property_read_u32(dev->of_node, "boost-crypt-vcore-min",
				 &volt)) {
		dev_info(dev, "failed to get boost-crypt-vcore-min");
		goto disable_caps;
	}

	cfg = host->crypt;
	if (ufs_mtk_init_host_clk(hba, "crypt_mux",
				  &cfg->clk_crypt_mux))
		goto disable_caps;

	if (ufs_mtk_init_host_clk(hba, "crypt_lp",
				  &cfg->clk_crypt_lp))
		goto disable_caps;

	if (ufs_mtk_init_host_clk(hba, "crypt_perf",
				  &cfg->clk_crypt_perf))
		goto disable_caps;

	cfg->reg_vcore = reg;
	cfg->vcore_volt = volt;
	host->caps |= UFS_MTK_CAP_BOOST_CRYPT_ENGINE;

disable_caps:
	return;
}

static void ufs_mtk_init_va09_pwr_ctrl(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	host->reg_va09 = regulator_get(hba->dev, "va09");
	if (IS_ERR(host->reg_va09))
		dev_info(hba->dev, "failed to get va09");
	else
		host->caps |= UFS_MTK_CAP_VA09_PWR_CTRL;
}

static void ufs_mtk_init_host_caps(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device_node *np = hba->dev->of_node;
	struct tag_bootmode *tag = NULL;
	struct arm_smccc_res res;
	bool mcq_en = false;

	if (of_property_read_bool(np, "mediatek,ufs-boost-crypt"))
		ufs_mtk_init_boost_crypt(hba);

	if (of_property_read_bool(np, "mediatek,ufs-support-va09"))
		ufs_mtk_init_va09_pwr_ctrl(hba);

	if (of_property_read_bool(np, "mediatek,ufs-disable-ah8"))
		host->caps |= UFS_MTK_CAP_DISABLE_AH8;

	if (of_property_read_bool(np, "mediatek,ufs-qos"))
		host->qos_enabled = host->qos_allowed = true;

	if (of_property_read_bool(np, "mediatek,ufs-broken-vcc"))
		host->caps |= UFS_MTK_CAP_BROKEN_VCC;

	if (of_property_read_bool(np, "mediatek,ufs-pmc-via-fastauto"))
		host->caps |= UFS_MTK_CAP_PMC_VIA_FASTAUTO;

	if (of_property_read_bool(np, "mediatek,ufs-tx-skew-fix"))
		host->caps |= UFS_MTK_CAP_TX_SKEW_FIX;

	if (of_property_read_bool(np, "mediatek,ufs-disable-mcq"))
		host->caps |= UFS_MTK_CAP_DISABLE_MCQ;

	if (of_property_read_bool(np, "mediatek,ufs-rtff-mtcmos"))
		host->caps |= UFS_MTK_CAP_RTFF_MTCMOS;

	if (of_property_read_bool(np, "mediatek,ufs-broken-rtc"))
		host->caps |= UFS_MTK_CAP_MCQ_BROKEN_RTC;

	if (of_property_read_bool(np, "mediatek,ufs-bypass-vccqx-lpm"))
		host->caps |= UFS_MTK_CAP_BYPASS_VCCQX_LPM;

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	if (of_property_read_bool(np, "mediatek,ufs-mphy-debug"))
		host->caps |= UFS_MTK_CAP_MPHY_DUMP;
#endif
	/* Check if MCQ is allowed */
	ufs_mtk_get_mcq_en(res);
	mcq_en = !!res.a1;
	if (!mcq_en)
		host->caps |= UFS_MTK_CAP_DISABLE_MCQ;

	dev_info(hba->dev, "caps=0x%x", host->caps);

	/* Get boot type from bootmode */
	tag = (struct tag_bootmode *)ufs_mtk_get_boot_property(np,
								"atag,boot", NULL);
	if (!tag)
		dev_info(hba->dev, "failed to get atag,boot\n");
	else if (tag->boottype == BOOTDEV_UFS)
		host->boot_device = true;

}

static void ufs_mtk_mcq_disable_irq(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 irq, i;

	if (!is_mcq_enabled(hba))
		return;

	if (host->mcq_nr_intr == 0)
		return;

	for (i = 0; i < host->mcq_nr_intr; i++) {
		irq = host->mcq_intr_info[i].irq;
		disable_irq(irq);
	}
	host->is_mcq_intr_enabled = false;
}

static void ufs_mtk_mcq_enable_irq(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 irq, i;

	if (!is_mcq_enabled(hba))
		return;

	if (host->mcq_nr_intr == 0)
		return;

	if (host->is_mcq_intr_enabled == true)
		return;

	for (i = 0; i < host->mcq_nr_intr; i++) {
		irq = host->mcq_intr_info[i].irq;
		enable_irq(irq);
	}
	host->is_mcq_intr_enabled = true;
}

/**
 * ufs_mtk_setup_clocks - enables/disable clocks
 * @hba: host controller instance
 * @on: If true, enable clocks else disable them.
 * @status: PRE_CHANGE or POST_CHANGE notify
 *
 * Returns 0 on success, non-zero on failure.
 */
static int ufs_mtk_setup_clocks(struct ufs_hba *hba, bool on,
				enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	bool clk_pwr_off = false;
	int ret = 0;
	struct arm_smccc_res res;

	/*
	 * In case ufs_mtk_init() is not yet done, simply ignore.
	 * This ufs_mtk_setup_clocks() shall be called from
	 * ufs_mtk_init() after init is done.
	 */
	if (!host)
		return 0;

	if (!on && status == PRE_CHANGE) {
		if (ufshcd_is_link_off(hba)) {
			clk_pwr_off = true;
		} else if (ufshcd_is_link_hibern8(hba) ||
			 (!ufshcd_can_hibern8_during_gating(hba) &&
			 ufshcd_is_auto_hibern8_enabled(hba))) {
			/*
			 * Gate ref-clk and poweroff mphy if link state is in
			 * OFF or Hibern8 by either Auto-Hibern8 or
			 * ufshcd_link_state_transition().
			 */
			ret = ufs_mtk_wait_link_state(hba,
						      VS_LINK_HIBERN8,
						      15);
			if (!ret)
				clk_pwr_off = true;
		}

		if (clk_pwr_off) {
			if (!ufshcd_is_clkscaling_supported(hba) ||
			    !hba->clk_scaling.is_enabled)
				ufs_mtk_pm_qos(hba, on);
			ufs_mtk_boost_crypt(hba, on);
			ufs_mtk_setup_ref_clk(hba, on);
			phy_power_off(host->mphy);
		}
		ufs_mtk_mcq_disable_irq(hba);
	} else if (on && status == POST_CHANGE) {
		phy_power_on(host->mphy);
		ufs_mtk_setup_ref_clk(hba, on);
		ufs_mtk_boost_crypt(hba, on);
		if (!ufshcd_is_clkscaling_supported(hba) ||
		    !hba->clk_scaling.is_enabled)
			ufs_mtk_pm_qos(hba, on);
		ufs_mtk_mcq_enable_irq(hba);
	} else if (on && status == PRE_CHANGE) {
		/* Reqeust spm resource */
		ufs_mtk_spm_rsc_ctrl(true, res);
	} else if (!on && status == POST_CHANGE) {
		/* Release spm resource */
		ufs_mtk_spm_rsc_ctrl(false, res);		
	}

	return ret;
}

static void ufs_mtk_mcq_set_irq_affinity(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct blk_mq_tag_set *tag_set = &hba->host->tag_set;
	struct blk_mq_queue_map	*map = &tag_set->map[HCTX_TYPE_DEFAULT];
	unsigned int nr = map->nr_queues;
	unsigned int q_index, cpu, _cpu, irq;
	int ret;

	/* Set affinity, only map HCTX_TYPE_DEFAULT
	 * If CPU count > HWQ count, only mapping HWQ count
	 * For example, CPU count 8, HWQ count 6, only mapping 6 HWQ interrupt
	 */
	for (cpu = 0; cpu < nr; cpu++) {
		q_index = map->mq_map[cpu];
		if (q_index > (nr)) {
			dev_err(hba->dev, "hwq index %d exceed %d\n",
				q_index, nr);
			return;
		}

		irq = host->mcq_intr_info[q_index].irq;
		if (irq == MTK_MCQ_INVALID_IRQ) {
			dev_err(hba->dev, "invalid irq. unable to bind q%d to cpu%d",
				q_index, cpu);
			return;
		}

		/* force migrate irq of cpu0 to cpu3 */
		_cpu = (cpu == 0) ? 3 : cpu;
		ret = irq_set_affinity(irq, cpumask_of(_cpu));
		if (ret) {
			dev_err(hba->dev, "set irq %d affinity to CPU %d failed\n",
				irq, _cpu);
			return;
		}
		dev_info(hba->dev, "set irq %d affinity to CPU: %d\n", irq, _cpu);
	}
}

#define UFS_VEND_SAMSUNG  (1 << 0)

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
	unsigned int vend;
};

static void ufs_mtk_trace_vh_send_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	if (!lrbp->cmd)
		return;

	ufs_mtk_btag_send_command(hba, lrbp);
}

static void ufs_mtk_trace_vh_compl_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;

	if (!cmd)
		return;

#if IS_ENABLED(CONFIG_RPMB)
	ufs_rpmb_vh_compl_command(hba, lrbp);
#endif

	ufs_mtk_btag_compl_command(hba, lrbp);
}

static void ufs_mtk_trace_vh_update_sdev(void *data, struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	
#if !IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	sdev->broken_fua = 1;
#endif

	dev_dbg(hba->dev, "lu %llu slave configured", sdev->lun);

	blk_queue_flag_set(QUEUE_FLAG_SAME_FORCE, sdev->request_queue);
	if (hba->luns_avail == 1) {
		/* The last LUs */
		dev_info(hba->dev, "%s: LUNs ready", __func__);
		complete(&host->luns_added);
	}
}

void ufs_mtk_trace_vh_ufs_prepare_command(void *data, struct ufs_hba *hba,
		struct request *rq, struct ufshcd_lrb *lrbp, int *err)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	char *cmnd = cmd->cmnd;
	if (cmnd[0] == WRITE_10 | cmnd[0] == WRITE_16)
		cmnd[1] &= ~0x08;
}

void ufs_mtk_trace_vh_check_int_errors(void *data, struct ufs_hba *hba, bool queue_eh_work)
{
	/* Disable UIC Error intr since eh work is scheduled */
	if (queue_eh_work && hba->uic_error &&
	   (hba->uic_error != UFSM_UIC_PA_GENERIC_ERROR) &&
	   !((hba->dev_quirks & UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) &&
	    (hba->uic_error & UFSM_UIC_DL_NAC_RECEIVED_ERROR)))
		ufsm_disable_intr(hba, UIC_ERROR);
}

static struct tracepoints_table interests[] = {
	{
		.name = "android_vh_ufs_prepare_command",
		.func = ufs_mtk_trace_vh_ufs_prepare_command
	},
	{
		.name = "android_vh_ufs_send_command",
		.func = ufs_mtk_trace_vh_send_command
	},
	{
		.name = "android_vh_ufs_compl_command",
		.func = ufs_mtk_trace_vh_compl_command
	},
	{
		.name = "android_vh_ufs_update_sdev",
		.func = ufs_mtk_trace_vh_update_sdev
	},
	{
		.name = "android_vh_ufs_check_int_errors",
		.func = ufs_mtk_trace_vh_check_int_errors
	},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

static void ufs_mtk_lookup_tracepoints(struct tracepoint *tp,
				       void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void ufs_mtk_uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
		}
	}
}

static int ufs_mtk_install_tracepoints(struct ufs_hba *hba)
{
	int i;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(ufs_mtk_lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			dev_info(hba->dev, "Error: tracepoint %s not found\n",
				interests[i].name);
			continue;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}

/*
 * HW version format has been changed from 01MMmmmm to 1MMMmmmm, since
 * project MT6878. In order to perform correct version comparison,
 * version number is changed by SW for the following projects.
 * IP_VER_MT6897	0x01440000 to 0x10440000
 * IP_VER_MT6989	0x01450000 to 0x10450000
 * IP_VER_MT6991	0x01460000 to 0x10460000
 */
static void ufs_mtk_get_hw_ip_version(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 hw_ip_ver;

	hw_ip_ver = ufshcd_readl(hba, REG_UFS_MTK_IP_VER);

	if ((hw_ip_ver & 0xFF000000) == 0x01000000) {
		hw_ip_ver &= ~0xFF000000;
		hw_ip_ver |= 0x10000000;
	}

	host->ip_ver = hw_ip_ver;
}

static void ufs_mtk_get_controller_version(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret, ver = 0;

	if (host->hw_ver.major)
		return;

	/* Set default (minimum) version anyway */
	host->hw_ver.major = 2;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_LOCALVERINFO), &ver);
	if (!ret) {
		if (ver >= UFS_UNIPRO_VER_1_8) {
			host->hw_ver.major = 3;
			/*
			 * Fix HCI version for some platforms with
			 * incorrect version
			 */
			if (hba->ufs_version < ufshci_version(3, 0))
				hba->ufs_version = ufshci_version(3, 0);
		}
	}
}

static u32 ufs_mtk_get_ufs_hci_version(struct ufs_hba *hba)
{
	return hba->ufs_version;
}

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)

static int ufs_mtk_clk_notify_handler(struct notifier_block *nb,
	unsigned long flags, void *data)
{
	struct ufs_hba *hba = ufshba;
	struct clk_event_data *clkd;

	if (!hba) {
		WARN_ON_ONCE(1);
		return NOTIFY_OK;
	}

	if (!data) {
		WARN_ON_ONCE(1);
		return NOTIFY_OK;
	}

	clkd = (struct clk_event_data *)data;

	switch (clkd->event_type) {
	case CLK_EVT_CLK_TRACE:
		if (clkd->id == 0) { /* turning off clock */
			if (strncmp("ufs", clkd->name, 3) == 0) {  /* ufs clocks */
				if (hba->clk_gating.state == CLKS_ON &&
					!hba->clk_gating.is_suspended) { /* should be ungated */
					/* someone still need clocks */
					ufs_mtk_dbg_dump(10);
					BUG_ON(1);
				}
			}
		}
	break;
	}
	return NOTIFY_OK;
}
#endif
/**
 * ufs_mtk_init_clocks - Init mtk driver private clocks
 *
 * @param hba: per adapter instance
 * Returns zero on success else failed.
 */
static int ufs_mtk_init_clocks(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_clk_info *clki, *clki_tmp;
	struct list_head *head = &hba->clk_list_head;
	struct device *dev = hba->dev;
	struct regulator *reg;
	u32 volt;

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	int ret;
#endif
	/*
	 * Find private clocks and store in struct ufs_mtk_clk.
	 * Remove "ufs_sel_min_src" and "ufs_sel_min_src" from list to avoid
	 * being switched on/off in clock gating.
	 */
	list_for_each_entry_safe(clki, clki_tmp, head, list) {
		if (!strcmp(clki->name, "ufs_sel")) {
			/* clk scaling */
			dev_dbg(hba->dev, "ufs_sel found");
			host->mclk.ufs_sel_clki = clki;
		} else if (!strcmp(clki->name, "ufs_sel_max_src")) {
			/* clk scaling */
			host->mclk.ufs_sel_max_clki = clki;
			clk_disable_unprepare(clki->clk);
			list_del(&clki->list);
			dev_dbg(hba->dev, "ufs_sel_max_src found");
		} else if (!strcmp(clki->name, "ufs_sel_min_src")) {
			/* clk scaling */
			host->mclk.ufs_sel_min_clki = clki;
			clk_disable_unprepare(clki->clk);
			list_del(&clki->list);
			dev_dbg(hba->dev, "ufs_sel_min_clki found");
		} else if (!strcmp(clki->name, "ufs_fde")) {
			/* clk scaling */
			dev_dbg(hba->dev, "ufs_fde found");
			host->mclk.ufs_fde_clki = clki;
		} else if (!strcmp(clki->name, "ufs_fde_max_src")) {
			/* clk scaling */
			host->mclk.ufs_fde_max_clki = clki;
			clk_disable_unprepare(clki->clk);
			list_del(&clki->list);
			dev_dbg(hba->dev, "ufs_fde_max_src found");
		} else if (!strcmp(clki->name, "ufs_fde_min_src")) {
			/* clk scaling */
			host->mclk.ufs_fde_min_clki = clki;
			clk_disable_unprepare(clki->clk);
			list_del(&clki->list);
			dev_dbg(hba->dev, "ufs_fde_min_clki found");
		}
	}

	list_for_each_entry(clki, head, list) {
		dev_info(hba->dev, "clk \"%s\" present", clki->name);
	}

	if (!ufs_mtk_is_clk_scale_ready(hba)) {
		hba->caps &= ~UFSHCD_CAP_CLK_SCALING;
		dev_info(hba->dev, "%s: Clk scaling not ready. Feature disabled.", __func__);
		return -1;
	}

	/*
	 * Default get vcore if dts have these settings.
	 * No matter clock scaling support or not. (may disable by customer)
	 */
	reg = devm_regulator_get_optional(dev, "dvfsrc-vcore");
	if (IS_ERR(reg)) {
		dev_info(dev, "failed to get dvfsrc-vcore: %ld",
			 PTR_ERR(reg));
		goto out;
	}

	if (of_property_read_u32(dev->of_node, "clk-scale-up-vcore-min",
				 &volt)) {
		dev_info(dev, "failed to get clk-scale-up-vcore-min");
		goto out;
	}

	host->mclk.reg_vcore = reg;
	host->mclk.vcore_volt = volt;

	/* If default boot is max gear, request vcore */
	if (reg && volt && host->clk_scale_up) {
		if (regulator_set_voltage(reg, volt, INT_MAX)) {
			dev_info(hba->dev,
				"Failed to set vcore to %d\n", volt);
			goto out;
		}
	}

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	/* Setup clk callback */
	ufshba = hba;
	host->clk_notifier.notifier_call = ufs_mtk_clk_notify_handler;
	ret = register_mtk_clk_notifier(&host->clk_notifier);
	if (ret)
		dev_err(hba->dev, "register clk_notifier failed");
#endif


out:
	return 0;
}

static int _ufshcd_get_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;

	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		dev_info(dev, "%s: %s get failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

#define MAX_VCC_NAME 30
static int _ufshcd_populate_vreg(struct device *dev, const char *name,
			  struct ufs_vreg **out_vreg)
{
	char prop_name[MAX_VCC_NAME];
	struct ufs_vreg *vreg = NULL;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_info(dev, "%s: non DT initialization\n", __func__);
		goto out;
	}

	snprintf(prop_name, MAX_VCC_NAME, "%s-supply", name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "%s: Unable to find %s regulator, assuming enabled\n",
				__func__, prop_name);
		goto out;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	vreg->name = devm_kstrdup(dev, name, GFP_KERNEL);
	if (!vreg->name)
		return -ENOMEM;

	snprintf(prop_name, MAX_VCC_NAME, "%s-max-microamp", name);
	if (of_property_read_u32(np, prop_name, &vreg->max_uA)) {
		dev_info(dev, "%s: unable to find %s\n", __func__, prop_name);
		vreg->max_uA = 0;
	}
out:
	*out_vreg = vreg;
	return 0;
}

static int ufs_mtk_vreg_fix_vcc(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;
	struct device_node *np = hba->dev->of_node;
	struct device *dev = hba->dev;
	char vcc_name[MAX_VCC_NAME];
	struct arm_smccc_res res;
	int err, ver;

	if (info->vcc)
		return 0;

	if (of_property_read_bool(np, "mediatek,ufs-vcc-by-num")) {
		ufs_mtk_get_vcc_num(res);
		if (res.a1 > UFS_VCC_NONE && res.a1 < UFS_VCC_MAX)
			snprintf(vcc_name, MAX_VCC_NAME, "vcc-opt%u", (unsigned int) res.a1);
		else
			return -ENODEV;
	} else if (of_property_read_bool(np, "mediatek,ufs-vcc-by-ver")) {
		ver = (hba->dev_info.wspecversion & 0xF00) >> 8;
		snprintf(vcc_name, MAX_VCC_NAME, "vcc-ufs%u", ver);
	} else {
		return 0;
	}

	err = _ufshcd_populate_vreg(dev, vcc_name, &info->vcc);
	if (err)
		return err;

	err = _ufshcd_get_vreg(dev, info->vcc);
	if (err)
		return err;

	err = regulator_enable(info->vcc->reg);
	if (!err) {
		info->vcc->enabled = true;
		dev_info(dev, "%s: %s enabled\n", __func__, vcc_name);
	}

	return err;
}

static void ufs_mtk_vreg_fix_vccqx(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;
	struct ufs_vreg **vreg_on, **vreg_off;

	if (hba->dev_info.wspecversion >= 0x0300) {
		vreg_on = &info->vccq;
		vreg_off = &info->vccq2;
	} else {
		vreg_on = &info->vccq2;
		vreg_off = &info->vccq;
	}

	if (*vreg_on)
		(*vreg_on)->always_on = true;

	if (*vreg_off) {
		regulator_disable((*vreg_off)->reg);
		devm_kfree(hba->dev, (*vreg_off)->name);
		devm_kfree(hba->dev, *vreg_off);
		*vreg_off  = NULL;
	}
}

static void ufs_mtk_setup_clk_gating(struct ufs_hba *hba)
{
	unsigned long flags;
	u32 ah_ms = 10;
	u32 ah_scale, ah_timer;
	u32 scale_us[] = {1, 10, 100, 1000, 10000, 100000};

	if (ufshcd_is_clkgating_allowed(hba)) {
		if (ufshcd_is_auto_hibern8_supported(hba) && hba->ahit) {
			ah_scale = FIELD_GET(UFSHCI_AHIBERN8_SCALE_MASK,
					  hba->ahit);
			ah_timer = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK,
					  hba->ahit);
			if (ah_scale <= 5)
				ah_ms = ah_timer * scale_us[ah_scale] / 1000;
		}

		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.delay_ms = max(ah_ms, 10U);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}
}

/* Convert microseconds to Auto-Hibernate Idle Timer register value */
static u32 ufs_mtk_us_to_ahit(unsigned int timer)
{
	unsigned int scale;

	for (scale = 0; timer > UFSHCI_AHIBERN8_TIMER_MASK; ++scale)
		timer /= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, timer) |
	       FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, scale);
}

static void ufs_mtk_fix_ahit(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	unsigned int us;

	if (ufshcd_is_auto_hibern8_supported(hba)) {
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
		us = 2000;
#else
		switch (hba->dev_info.wmanufacturerid) {
		case UFS_VENDOR_SAMSUNG:
			/* configure auto-hibern8 timer to 3.5 ms */
			us = 3500;
			break;

		case UFS_VENDOR_MICRON:
			/* configure auto-hibern8 timer to 2 ms */
			us = 2000;
			break;

		default:
			/* configure auto-hibern8 timer to 1 ms */
			us = 1000;
			break;
		}
#endif

		hba->ahit = host->desired_ahit = ufs_mtk_us_to_ahit(us);
	}

	ufs_mtk_setup_clk_gating(hba);
}

static void ufs_mtk_fix_clock_scaling(struct ufs_hba *hba)
{
	/* UFS version is below 4.0, clock scaling is not necessary */
	if ((hba->dev_info.wspecversion < 0x0400)  &&
		ufs_mtk_is_clk_scale_ready(hba)) {
		hba->caps &= ~UFSHCD_CAP_CLK_SCALING;

		_ufs_mtk_clk_scale(hba, false);
	}
}

static void ufs_mtk_delay_eh_work_fn(struct work_struct *dwork)
{
	struct ufs_hba *hba;
	struct ufs_mtk_host *host;

	host = container_of(dwork, struct ufs_mtk_host, delay_eh_work.work);
	hba = host->hba;

	queue_work(hba->eh_wq, &hba->eh_work);
}

static void ufs_mtk_init_mcq_irq(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct platform_device *pdev;
	int i;
	int irq;

	host->mcq_nr_intr = UFSHCD_MAX_Q_NR;
	pdev = container_of(hba->dev, struct platform_device, dev);

	/* invalidate irq info */
	for (i = 0; i < host->mcq_nr_intr; i++)
		host->mcq_intr_info[i].irq = MTK_MCQ_INVALID_IRQ;

	if (host->caps & UFS_MTK_CAP_DISABLE_MCQ)
		goto failed;

	for (i = 0; i < host->mcq_nr_intr; i++) {
		/* irq index 0 is ufshcd system irq, sq, cq irq start from index 1 */
		irq = platform_get_irq(pdev, i + 1);
		if (irq < 0) {
			dev_err(hba->dev, "get platform mcq irq fail: %d\n", i);
			goto failed;
		}
		host->mcq_intr_info[i].hba = hba;
		host->mcq_intr_info[i].irq = irq;
		dev_info(hba->dev, "get platform mcq irq: %d, %d\n", i, irq);
	}

	return;
failed:
	host->mcq_nr_intr = 0;
}

#ifdef CONFIG_PM_SLEEP
static void ufs_mtk_rq_dwork_fn(struct work_struct *dwork)
{
	struct scsi_device *sdev;
	struct ufs_hba *hba;
	struct ufs_mtk_host *host;

	host = container_of(dwork, struct ufs_mtk_host, rq_dwork.work);
	hba = host->hba;

	__shost_for_each_device(sdev, hba->host)
		wake_up_all(&sdev->request_queue->mq_freeze_wq);
}
#endif

/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_mtk_init(struct ufs_hba *hba)
{
	const struct of_device_id *id;
	struct device *dev = hba->dev;
	struct ufs_mtk_host *host;
	int err = 0;
	struct arm_smccc_res res;
	struct tag_ufs *atag;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_info(dev, "%s: no memory for mtk ufs host\n", __func__);
		goto out;
	}

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	atag = (struct tag_ufs *)ufs_mtk_get_boot_property(dev->of_node,
			"atag,ufs", NULL);
	if (!atag)
		dev_err(hba->dev, "cannot find atag,ufs");
	else
		host->atag = atag;

	id = of_match_device(ufs_mtk_of_match, dev);
	if (!id) {
		err = -EINVAL;
		goto out;
	}

	/* Initialize host capability */
	ufs_mtk_init_host_caps(hba);

	ufs_mtk_init_mcq_irq(hba);

	err = ufs_mtk_bind_mphy(hba);
	if (err)
		goto out_variant_clear;

	ufs_mtk_init_reset(hba);

	/* backup mphy setting if mphy can reset */
	if (host->mphy_reset)
		ufs_mtk_mphy_ctrl(UFS_MPHY_BACKUP, res);

	/* Enable runtime autosuspend */
	hba->caps |= UFSHCD_CAP_RPM_AUTOSUSPEND;

	/* Enable clock-gating */
	hba->caps |= UFSHCD_CAP_CLK_GATING;

	/* Enable inline encryption */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Enable WriteBooster */
	hba->caps |= UFSHCD_CAP_WB_EN;

	/* Enable clk scaling*/
	hba->caps |= UFSHCD_CAP_CLK_SCALING;
	host->clk_scale_up = true; /* default is max freq */

	hba->quirks |= UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL;

	hba->quirks |= UFSHCD_QUIRK_MCQ_BROKEN_INTR;
	if (host->caps & UFS_MTK_CAP_MCQ_BROKEN_RTC)
		hba->quirks |= UFSHCD_QUIRK_MCQ_BROKEN_RTC;

	hba->vps->wb_flush_threshold = UFS_WB_BUF_REMAIN_PERCENT(80);

	if (host->caps & UFS_MTK_CAP_DISABLE_AH8)
		hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_adjust_caps_quirks(hba);
#endif

	if (ufs_mtk_is_mphy_dump(hba))
		ufs_mtk_dbg_phy_enable(hba);

	ufs_mtk_init_clocks(hba);

	ufs_mtk_init_sysfs(hba);

	/*
	 * ufshcd_vops_init() is invoked after
	 * ufshcd_setup_clock(true) in ufshcd_hba_init() thus
	 * phy clock setup is skipped.
	 *
	 * Enable phy power and clocks specifically here.
	 */
	ufs_mtk_mphy_power_on(hba, true);

	if (ufs_mtk_is_rtff_mtcmos(hba)) {
		/* First Restore here, to avoid backup unexpected value */
		ufs_mtk_mtcmos_ctrl(false, res);

		/* Power on to init */
		ufs_mtk_mtcmos_ctrl(true, res);
	}

	ufs_mtk_setup_clocks(hba, true, POST_CHANGE);

	ufs_mtk_get_hw_ip_version(hba);

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	ufs_mtk_check_bus_init(host->ip_ver);
#endif

	cpu_latency_qos_add_request(&host->pm_qos_req,
	     	   PM_QOS_DEFAULT_VALUE);
	host->pm_qos_init = true;

	init_completion(&host->luns_added);
	INIT_DELAYED_WORK(&host->delay_eh_work, ufs_mtk_delay_eh_work_fn);
	host->delay_eh_workq = create_singlethread_workqueue("ufs_mtk_eh_wq");
	host->ufs_wake_lock = wakeup_source_register(NULL, "ufs_wake_lock");

#ifdef CONFIG_PM_SLEEP
	INIT_DELAYED_WORK(&host->rq_dwork, ufs_mtk_rq_dwork_fn);
	host->rq_workq = create_singlethread_workqueue("ufs_mtk_rq_wq");
#endif

	ufs_mtk_btag_init(hba);

	ufs_mtk_dbg_register(hba);

	ufs_mtk_rpmb_init(hba);

	ufs_mtk_init_ioctl(hba);

	goto out;

out_variant_clear:
	ufshcd_set_variant(hba, NULL);
out:
	return err;
}

static bool ufs_mtk_pmc_via_fastauto(struct ufs_hba *hba,
	struct ufs_pa_layer_attr *dev_req_params)
{
	if (!ufs_mtk_is_pmc_via_fastauto(hba))
		return false;

	if (dev_req_params->hs_rate == hba->pwr_info.hs_rate)
		return false;

	if ((dev_req_params->pwr_tx != FAST_MODE) &&
		(dev_req_params->gear_tx < UFS_HS_G4))
		return false;

	if ((dev_req_params->pwr_rx != FAST_MODE) &&
		(dev_req_params->gear_rx < UFS_HS_G4))
		return false;

	if ((dev_req_params->pwr_tx == SLOW_MODE) ||
		(dev_req_params->pwr_rx == SLOW_MODE))
		return false;

	return true;
}

void ufs_mtk_adjust_sync_length(struct ufs_hba *hba)
{
	int i;
	u32 value;
	u32 cnt, att, min;
	struct attr_min {
		u32 attr;
		u32 min_value;
	} pa_min_sync_length[] = {
		{PA_TXHSG1SYNCLENGTH, 0x48},
		{PA_TXHSG2SYNCLENGTH, 0x48},
		{PA_TXHSG3SYNCLENGTH, 0x48},
		{PA_TXHSG4SYNCLENGTH, 0x48},
		{PA_TXHSG5SYNCLENGTH, 0x48}
	};

	cnt = sizeof(pa_min_sync_length) / sizeof(struct attr_min);
	for (i = 0; i < cnt; i++) {
		att = pa_min_sync_length[i].attr;
		min = pa_min_sync_length[i].min_value;
		ufshcd_dme_get(hba, UIC_ARG_MIB(att), &value);
		if (value < min)
			ufshcd_dme_set(hba, UIC_ARG_MIB(att), min);

		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(att), &value);
		if (value < min)
			ufshcd_dme_peer_set(hba, UIC_ARG_MIB(att), min);
	}
}

static int ufs_mtk_pre_pwr_change(struct ufs_hba *hba,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_dev_params host_cap;
	int ret;

	ufshcd_init_pwr_dev_param(&host_cap);
	host_cap.hs_rx_gear = UFS_HS_G5;
	host_cap.hs_tx_gear = UFS_HS_G5;

	if ((dev_max_params->pwr_rx == SLOW_MODE) ||
		(dev_max_params->pwr_tx == SLOW_MODE)) {
		host_cap.desired_working_mode = UFS_PWM_MODE;
	}

	ret = ufshcd_get_pwr_dev_param(&host_cap,
				       dev_max_params,
				       dev_req_params);
	if (ret) {
		pr_info("%s: failed to determine capabilities\n",
			__func__);
	}

	/* Assume default boot up is max gear */
	if (host->max_gear == 0)
		host->max_gear = dev_req_params->gear_rx;

	if (ufs_mtk_pmc_via_fastauto(hba, dev_req_params)) {
		ufs_mtk_adjust_sync_length(hba);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), true);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXGEAR), UFS_HS_G1);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), true);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXGEAR), UFS_HS_G1);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			dev_req_params->lane_tx);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			dev_req_params->lane_rx);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HSSERIES),
			dev_req_params->hs_rate);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSADAPTTYPE),
			PA_NO_ADAPT);

		if (!(hba->quirks & UFSHCD_QUIRK_SKIP_DEF_UNIPRO_TIMEOUT_SETTING)) {
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA0),
					DL_FC0ProtectionTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA1),
					DL_TC0ReplayTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA2),
					DL_AFC0ReqTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA3),
					DL_FC1ProtectionTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA4),
					DL_TC1ReplayTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA5),
					DL_AFC1ReqTimeOutVal_Default);

			ufshcd_dme_set(hba, UIC_ARG_MIB(DME_LocalFC0ProtectionTimeOutVal),
					DL_FC0ProtectionTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(DME_LocalTC0ReplayTimeOutVal),
					DL_TC0ReplayTimeOutVal_Default);
			ufshcd_dme_set(hba, UIC_ARG_MIB(DME_LocalAFC0ReqTimeOutVal),
					DL_AFC0ReqTimeOutVal_Default);
		}

		ret = ufshcd_uic_change_pwr_mode(hba,
			FASTAUTO_MODE << 4 | FASTAUTO_MODE);

		if (ret) {
			dev_info(hba->dev, "%s: HSG1B FASTAUTO failed ret=%d\n",
				__func__, ret);
		}
	}

	/* if already configured to the requested pwr_mode, skip adapt */
	if (dev_req_params->gear_rx == hba->pwr_info.gear_rx &&
	    dev_req_params->gear_tx == hba->pwr_info.gear_tx &&
	    dev_req_params->lane_rx == hba->pwr_info.lane_rx &&
	    dev_req_params->lane_tx == hba->pwr_info.lane_tx &&
	    dev_req_params->pwr_rx == hba->pwr_info.pwr_rx &&
	    dev_req_params->pwr_tx == hba->pwr_info.pwr_tx &&
	    dev_req_params->hs_rate == hba->pwr_info.hs_rate) {
		return ret;
	}

	if (dev_req_params->pwr_rx == FAST_MODE ||
		dev_req_params->pwr_rx == FASTAUTO_MODE) {
		if (host->hw_ver.major >= 3) {
			ret = ufshcd_dme_configure_adapt(hba,
						   dev_req_params->gear_tx,
						   PA_INITIAL_ADAPT);
		} else {
			ret = ufshcd_dme_configure_adapt(hba,
				   dev_req_params->gear_tx,
				   PA_NO_ADAPT);
		}
	} else {
		ret = ufshcd_dme_configure_adapt(hba,
			   dev_req_params->gear_tx,
			   PA_NO_ADAPT);
	}

	return ret;
}

static int ufs_mtk_pwr_change_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status stage,
				     struct ufs_pa_layer_attr *dev_max_params,
				     struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_pwr_change(hba, dev_max_params,
					     dev_req_params);
		break;
	case POST_CHANGE:
		dev_info(hba->dev,
				"Power mode change: M(%d)G(%d)L(%d)HS-series(%d)\n",
				dev_req_params->pwr_tx, dev_req_params->gear_tx,
				dev_req_params->lane_tx, dev_req_params->hs_rate);
		dev_info(hba->dev, "HS mode config passes\n");

		host->skip_flush = false;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_unipro_set_lpm(struct ufs_hba *hba, bool lpm)
{
	int ret;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ret = ufshcd_dme_set(hba,
			     UIC_ARG_MIB_SEL(VS_UNIPROPOWERDOWNCONTROL, 0),
			     lpm ? 1 : 0);
	if (!ret || !lpm) {
		/*
		 * Forcibly set as non-LPM mode if UIC commands is failed
		 * to use default hba_enable_delay_us value for re-enabling
		 * the host.
		 */
		host->unipro_lpm = lpm;
	}

	if (ret)
		ufs_mtk_eh_unipro_set_lpm(hba, ret);

	return ret;
}

static int ufs_mtk_pre_link(struct ufs_hba *hba)
{
	int ret;
	u32 tmp;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufs_mtk_get_controller_version(hba);

	ret = ufs_mtk_unipro_set_lpm(hba, false);
	if (ret)
		return ret;

	/*
	 * Setting PA_Local_TX_LCC_Enable to 0 before link startup
	 * to make sure that both host and device TX LCC are disabled
	 * once link startup is completed.
	 */
	ret = ufshcd_disable_host_tx_lcc(hba);
	if (ret)
		return ret;

	/* disable deep stall */
	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
	if (ret)
		return ret;

	tmp &= ~(1 << 6);

	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

	/* Enable the 1144 functions setting */
	if (host->ip_ver == IP_VER_MT6989) {
		ret = ufshcd_dme_get(hba, UIC_ARG_MIB(VS_DEBUGOMC), &tmp);
		if (ret)
			return ret;

		tmp |= 0x10;
		ret = ufshcd_dme_set(hba, UIC_ARG_MIB(VS_DEBUGOMC), tmp);
	}

	return ret;
}

static int ufs_mtk_post_link(struct ufs_hba *hba)
{
	/* enable unipro clock gating feature */
	ufs_mtk_cfg_unipro_cg(hba, true);

	return 0;
}

static int ufs_mtk_link_startup_notify(struct ufs_hba *hba,
				       enum ufs_notify_change_status stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = ufs_mtk_post_link(hba);
		if (!ret)
			dev_info(hba->dev, "UFS link start-up passes\n");
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_device_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;

	/* guarantee device internal cache flush */
	if (hba->eh_flags && !host->skip_flush) {
		dev_info(hba->dev, "%s: Waiting device internal cache flush\n",
			__func__);
		ssleep(2);
		host->skip_flush = true;
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
		ufs_sec_inc_hwrst_cnt();
#endif
	}

	ufs_mtk_device_reset_ctrl(0, res);

	/* disable hba in middle of device reset */
	ufshcd_hba_stop(hba);

	/*
	 * The reset signal is active low. UFS devices shall detect
	 * more than or equal to 1us of positive or negative RST_n
	 * pulse width.
	 *
	 * To be on safe side, keep the reset low for at least 10us.
	 */
	usleep_range(10, 15);

	ufs_mtk_device_reset_ctrl(1, res);

	/* Some devices may need time to respond to rst_n */
	usleep_range(10000, 15000);

	dev_info(hba->dev, "device reset done\n");

	return 0;
}

static void ufs_mtk_scsi_unblock_requests(struct ufs_hba *hba)
{
	if (atomic_dec_and_test(&hba->scsi_block_reqs_cnt))
		scsi_unblock_requests(hba->host);
}

static void ufs_mtk_scsi_block_requests(struct ufs_hba *hba)
{
	if (atomic_inc_return(&hba->scsi_block_reqs_cnt) == 1)
		scsi_block_requests(hba->host);
}

static u32 ufs_mtk_pending_cmds(struct ufs_hba *hba)
{
	const struct scsi_device *sdev;
	u32 pending = 0;

	lockdep_assert_held(hba->host->host_lock);
	__shost_for_each_device(sdev, hba->host)
		pending += sbitmap_weight(&sdev->budget_map);

	return pending;
}

static int ufs_mtk_wait_for_doorbell_clr(struct ufs_hba *hba,
					u64 wait_timeout_us)
{
	unsigned long flags;
	int ret = 0;
	u32 tm_doorbell;
	u32 tr_pending;
	bool timeout = false, do_last_check = false;
	ktime_t start;

	ufshcd_hold(hba, false);
	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * Wait for all the outstanding tasks/transfer requests.
	 * Verify by checking the doorbell registers are clear.
	 */
	start = ktime_get();
	do {
		if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
			ret = -EBUSY;
			goto out;
		}

		tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
		tr_pending = ufs_mtk_pending_cmds(hba);
		if (!tm_doorbell && !tr_pending) {
			timeout = false;
			break;
		} else if (do_last_check) {
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		io_schedule_timeout(msecs_to_jiffies(20));
		if (ktime_to_us(ktime_sub(ktime_get(), start)) >
		    wait_timeout_us) {
			timeout = true;
			/*
			 * We might have scheduled out for long time so make
			 * sure to check if doorbells are cleared by this time
			 * or not.
			 */
			do_last_check = true;
		}
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (tm_doorbell || tr_pending);

	if (timeout) {
		dev_err(hba->dev,
			"%s: timedout waiting for doorbell to clear (tm=0x%x, tr=0x%x)\n",
			__func__, tm_doorbell, tr_pending);
		ret = -EBUSY;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_release(hba);
	return ret;
}

static void ufs_mtk_config_pwr_mode(struct ufs_hba *hba, int mode,
	bool scale_allow)
{
	struct ufs_pa_layer_attr pwr_info = { 0 };
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ktime_t timeout, time_checked;
	int err;

	/* Config desired power info */
	memcpy(&pwr_info, &hba->pwr_info, sizeof(struct ufs_pa_layer_attr));

	if (mode == CLK_FORCE_SCALE_UP) {
		pwr_info.gear_rx = host->max_gear;
		pwr_info.gear_tx = host->max_gear;
	} else if (mode == CLK_FORCE_SCALE_DOWN) {
		if (scale_allow) {
			/* min_gear is set by MTK */
			pwr_info.gear_rx = hba->clk_scaling.min_gear;
			pwr_info.gear_tx = hba->clk_scaling.min_gear;
		} else {
			/*
			 * min_gear is set by default, which is G1.
			 * Just set to max_gear because in this case, we only
			 * use max gear as default gear.
			 */
			pwr_info.gear_rx = host->max_gear;
			pwr_info.gear_tx = host->max_gear;
		}
	} else if (mode == CLK_FORCE_SCALE_G1) {
		pwr_info.gear_rx = UFS_HS_G1;
		pwr_info.gear_tx = UFS_HS_G1;
	}

	__pm_stay_awake(host->ufs_wake_lock);

	/* make sure pm resume done */
	timeout = ktime_add_ms(ktime_get(), UFS_WAKE_LOCK_TIMEOUT_MS);
	do {
		time_checked = ktime_get();

		/* Wait until suspend flow exit */
		if (pm_suspend_target_state == PM_SUSPEND_ON)
			goto rpm;

		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	dev_warn(hba->dev, "Wait suspend status timeout, status = %d\n", pm_suspend_target_state);
	__pm_relax(host->ufs_wake_lock);
	return;
rpm:
	ufshcd_rpm_get_sync(hba);
	ufs_mtk_scsi_block_requests(hba);
	ufshcd_hold(hba, false);

	if (ufs_mtk_wait_for_doorbell_clr(hba, 1000 * 1000)) { /* 1 sec */
		dev_err(hba->dev, "%s: ufshcd_wait_for_doorbell_clr timeout!\n",
				__func__);
		goto out;
	}

	/* Scale up, change clk mux before change gear */
	if ((mode == CLK_FORCE_SCALE_UP) && scale_allow) {
		ufs_mtk_pm_qos(hba, true);
		_ufs_mtk_clk_scale(hba, true);
	}

	/* Start change power mode */
	err = ufshcd_config_pwr_mode(hba, &pwr_info);
	if (err) {
		dev_err(hba->dev, "%s: Failed setting power mode, err = %d\n",
				__func__, err);
	} else {
		dev_info(hba->dev,
		      "%s: PMC Success! [RX, TX]: gear=[%d, %d], pwr[%d, %d]\n",
		      __func__,
		      hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		      hba->pwr_info.pwr_rx, hba->pwr_info.pwr_tx);
	}

	/* Scale down, change clk mux after change gear */
	if ((mode != CLK_FORCE_SCALE_UP) && scale_allow) {
		ufs_mtk_pm_qos(hba, false);
		_ufs_mtk_clk_scale(hba, false);
	}

out:
	ufshcd_release(hba);
	ufs_mtk_scsi_unblock_requests(hba);
	ufshcd_rpm_put(hba);
	__pm_relax(host->ufs_wake_lock);
}

void ufs_mtk_dynamic_clock_scaling(struct ufs_hba *hba, int mode)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	static int scale_mode = CLK_SCALE_FREE_RUN;
	static u32 saved_gear;
	static bool is_forced;
	unsigned long flags;
	bool scale_allow = true;
	bool scale_suspend = false;
	bool scale_resume = false;

	/* Not allow change clock scaling */
	if (host->clk_scale_forbid)
		return;

	/* Already in desire mode */
	if (scale_mode == mode)
		return;
	scale_mode = mode;

	/* UFS version is below 4.0, clock scaling is not necessary */
	if (hba->dev_info.wspecversion < 0x0400)
		scale_allow = false;

	/* Host not support clock scaling or disable by customer */
	if (!hba->clk_scaling.is_initialized)
		scale_allow = false;

	/*
	 * Make sure call devfreq_resume_device/devfreq_suspend_device not in
	 * UFS runtime suspend state. Or we may have abnormal scale up in
	 * runtime suspend which mtcmos is not on.
	 */
	ufshcd_rpm_get_sync(hba);

	if (mode == CLK_SCALE_FREE_RUN) {
		ufs_mtk_scsi_block_requests(hba);
		if (saved_gear >= UFS_HS_G5)
			ufs_mtk_config_pwr_mode(hba, CLK_FORCE_SCALE_UP,
				scale_allow);
		else
			ufs_mtk_config_pwr_mode(hba, CLK_FORCE_SCALE_DOWN,
				scale_allow);

		if (scale_allow) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			if (hba->clk_scaling.is_suspended) {
				scale_resume = true;
				hba->clk_scaling.is_suspended = false;
			}
			spin_unlock_irqrestore(hba->host->host_lock, flags);

			if (scale_resume)
				devfreq_resume_device(hba->devfreq);
			hba->caps |= UFSHCD_CAP_CLK_SCALING;
			hba->clk_scaling.active_reqs = 0;
		}
		ufs_mtk_scsi_unblock_requests(hba);

		is_forced = false;
	} else {
		if (!is_forced) {
			if (scale_allow) {
				hba->caps &= ~UFSHCD_CAP_CLK_SCALING;
				spin_lock_irqsave(hba->host->host_lock, flags);
				if (!hba->clk_scaling.is_suspended) {
					scale_suspend = true;
					hba->clk_scaling.is_suspended = true;
					hba->clk_scaling.window_start_t = 0;
				}
				spin_unlock_irqrestore(hba->host->host_lock, flags);

				if (scale_suspend)
					devfreq_suspend_device(hba->devfreq);
			}

			saved_gear = hba->pwr_info.gear_rx;
			is_forced = true;
		}

		ufs_mtk_config_pwr_mode(hba, mode, scale_allow);
	}

	ufshcd_rpm_put(hba);
}

static void _ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == ufshci_version(1, 0)) {
		u32 rw;

		rw = set & INTERRUPT_MASK_RW_VER_10;
		set = rw | ((set ^ intrs) & intrs);
	} else {
		set |= intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static int ufs_mtk_link_set_hpm(struct ufs_hba *hba)
{
	int err;
	u32 val;

	err = ufshcd_hba_enable(hba);
	if (err)
		return err;

	err = ufs_mtk_unipro_set_lpm(hba, false);
	if (err) {
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		ufshcd_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)val);
		val = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		ufshcd_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)val);
		return err;
	}

	err = ufshcd_uic_hibern8_exit(hba);
	if (err)
		return err;

	/* Check link state to make sure exit h8 success */
	ufs_mtk_wait_idle_state(hba, 5);
	err = ufs_mtk_wait_link_state(hba, VS_LINK_UP, 100);
	if (err) {
		dev_warn(hba->dev, "exit h8 state fail, err=%d\n", err);
		return err;
	}
	ufshcd_set_link_active(hba);

	err = ufshcd_make_hba_operational(hba);
	if (err)
		return err;

	if (is_mcq_enabled(hba)) {
		ufs_mtk_config_mcq(hba, false);
		/* Enable required interrupts */
		_ufshcd_enable_intr(hba, UFSHCD_ENABLE_MTK_MCQ_INTRS);
		ufshcd_mcq_make_queues_operational(hba);
		ufshcd_mcq_config_mac(hba, hba->nutrs);
		ufshcd_writel(hba, ufshcd_readl(hba, REG_UFS_MEM_CFG) | 0x1,
			      REG_UFS_MEM_CFG);
	}

	return 0;
}

static int ufs_mtk_link_set_lpm(struct ufs_hba *hba)
{
	int err;

	/* not wait unipro resetCnf */
	ufshcd_writel(hba,
		(ufshcd_readl(hba, REG_UFS_XOUFS_CTRL) & ~0x100),
		REG_UFS_XOUFS_CTRL);

	err = ufs_mtk_unipro_set_lpm(hba, true);
	if (err) {
		/* Resume UniPro state for following error recovery */
		ufs_mtk_unipro_set_lpm(hba, false);
		return err;
	}

	return 0;
}

static void ufs_mtk_vccqx_set_lpm(struct ufs_hba *hba, bool lpm)
{
	struct ufs_vreg *vccqx = NULL;

	if (!hba->vreg_info.vcc ||
	    (!hba->vreg_info.vccq && !hba->vreg_info.vccq2) ||
	    (hba->vreg_info.vcc &&
	     hba->vreg_info.vcc->enabled &&
	     !ufs_mtk_is_allow_vccqx_lpm(hba)) ||
	    ufs_mtk_is_bypass_vccqx_lpm(hba))
		return;

	if (hba->vreg_info.vccq)
		vccqx = hba->vreg_info.vccq;
	else
		vccqx = hba->vreg_info.vccq2;

	regulator_set_mode(vccqx->reg,
		lpm ? REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL);
}

static void ufs_mtk_vsx_set_lpm(struct ufs_hba *hba, bool lpm)
{
	struct arm_smccc_res res;

	ufs_mtk_device_pwr_ctrl(!lpm,
	(unsigned long)hba->dev_info.wspecversion, res);
}

static void ufs_mtk_dev_vreg_set_lpm(struct ufs_hba *hba, bool lpm)
{
	/* Prevent entering LPM when device is still active */
	if (lpm && ufshcd_is_ufs_dev_active(hba))
		return;

	if (lpm) {
		ufs_mtk_vccqx_set_lpm(hba, lpm);
		ufs_mtk_vsx_set_lpm(hba, lpm);
	} else {
		ufs_mtk_vsx_set_lpm(hba, lpm);
		ufs_mtk_vccqx_set_lpm(hba, lpm);
	}
}

static int ufs_mtk_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
	enum ufs_notify_change_status status)
{
	int err;
	struct arm_smccc_res res;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (status == PRE_CHANGE) {
		if (!ufshcd_is_auto_hibern8_supported(hba))
			return 0;
		err = ufs_mtk_auto_hibern8_disable(hba);

		/* May trigger eh work without exit h8 error */
		if (ufsm_eh_in_progress(hba))
			return -EAGAIN;
		else
			return err;
	}

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	if (hba->shutting_down)
		ufs_sec_print_err_info(hba);
	else
		ufs_sec_print_err();
#endif

	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_lpm(hba);
		if (err)
			goto fail;
	}

	if (!ufshcd_is_link_active(hba)) {
		/*
		 * Make sure no error will be returned to prevent
		 * ufshcd_suspend() re-enabling regulators while vreg is still
		 * in low-power mode.
		 */
		err = ufs_mtk_mphy_power_on(hba, false);
		if (err)
			goto fail;
	}

	if (ufshcd_is_link_off(hba))
		ufs_mtk_device_reset_ctrl(0, res);

	ufs_mtk_sram_pwr_ctrl(false, res);

	/* Release pm_qos/clk if enter suspend is scale up mode */
	if (ufshcd_is_clkscaling_supported(hba) && (host->clk_scale_up)) {
		ufs_mtk_pm_qos(hba, false);
		_ufs_mtk_clk_scale(hba, false);
	}

	return 0;
fail:
	/*
	 * Set link as off state enforcedly to trigger
	 * ufshcd_host_reset_and_restore() in ufshcd_suspend()
	 * for completed host reset.
	 */
	ufshcd_set_link_off(hba);
	return -EAGAIN;
}

static int ufs_mtk_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int err;
	struct arm_smccc_res res;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)
		ufs_mtk_dev_vreg_set_lpm(hba, false);

	ufs_mtk_sram_pwr_ctrl(true, res);

	err = ufs_mtk_mphy_power_on(hba, true);
	if (err)
		goto fail;

	/* Request pm_qos/clk if enter suspend is scale up mode */
	if (ufshcd_is_clkscaling_supported(hba) && (host->clk_scale_up)) {
		ufs_mtk_pm_qos(hba, true);
		_ufs_mtk_clk_scale(hba, true);
	}

	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_hpm(hba);
		if (err)
			goto fail;
	}

	return 0;
fail:

	/*
	 * Should not return fail if platform(parent) dev is resume,
	 * which power, clock and mtcmos is all turn on.
	 */
	err = ufshcd_link_recovery(hba);
	if (err) {
		dev_err(hba->dev, "Device PM: req=%d, status:%d, err:%d\n",
			hba->dev->power.request,
			hba->dev->power.runtime_status,
			hba->dev->power.runtime_error);
	}

	return 0; /* cannot return fail, else IO hang */
}

static void ufs_mtk_dbg_register_dump(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int i;

	/*
	 * Skip debug dump since critical information has already
	 * been printed previously before the eh work was scheduled.
	 */
	if (ufsm_eh_in_progress(hba))
		goto out;

	mt_irq_dump_status(hba->irq);

	/* Dump ufshci register 0x0 ~ 0xA0 */
	ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE, "UFSHCI (0x0):");

	/* Dump ufshci register 0x140 ~ 0x14C */
	ufshcd_dump_regs(hba, REG_UFS_XOUFS_CTRL, 0x10, "XOUFS Ctrl (0x140): ");

	ufshcd_dump_regs(hba, REG_UFS_EXTREG, 0x4, "Ext Reg ");

	if (hba->mcq_enabled) {
		/* Dump ufshci register 0x100 ~ 0x160 */
		ufshcd_dump_regs(hba, REG_UFS_CCAP,
				 REG_UFS_MMIO_OPT_CTRL_0 - REG_UFS_CCAP + 4,
				 "UFSHCI (0x100): ");

		/* Dump ufshci register 0x300 */
		ufshcd_dump_regs(hba, REG_UFS_MEM_CFG, 4,
				 "UFSHCI (0x300): ");

		/* Dump ufshci register 0x380 */
		ufshcd_dump_regs(hba, REG_UFS_MCQ_CFG, 4,
				 "UFSHCI (0x380): ");
	}

	/* Dump ufshci register 0x2200 ~ 0x22AC */
	ufshcd_dump_regs(hba, REG_UFS_MPHYCTRL,
			 REG_UFS_REJECT_MON - REG_UFS_MPHYCTRL + 4,
			 "UFSHCI (0x2200): ");

	/* Dump ufshci register 0x22B0 ~ 0x22B4, 4 times */
	for (i = 0; i < 4; i++)
		ufshcd_dump_regs(hba, REG_UFS_AH8E_MON,
			 REG_UFS_AH8X_MON - REG_UFS_AH8E_MON + 4,
			 "UFSHCI (0x22B0): ");

	/* Direct debugging information to REG_MTK_PROBE */
	ufs_mtk_dbg_sel(hba);
	/* Dump ufshci register 0x22C8 */
	ufshcd_dump_regs(hba, REG_UFS_PROBE, 0x4, "Debug Probe ");

	if (hba->mcq_enabled) {
		/* Dump ufshci register 0x22FC ~ 0x2304 */
		ufshcd_dump_regs(hba, REG_UFS_SQ_DBR_DBG,
				 REG_UFS_ACT_STS - REG_UFS_SQ_DBR_DBG + 4,
				 "UFSHCI (0x22F0): ");
		/* Dump ufshci register 0x2800 ~ 0x297C */
		ufshcd_dump_regs(hba, REG_UFS_MTK_SQD,
				 REF_UFS_MTK_CQ7_IACR - REG_UFS_MTK_SQD + 4,
				 "UFSHCI (0x2800): ");
	}

	ufs_mtk_dbg_dump(100);

	ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_ERR);

	if (ufs_mtk_is_mphy_dump(hba)) {
		queue_delayed_work(host->phy_dmp_workq,
			&host->phy_dmp_work,
			msecs_to_jiffies(500));
	}

out:
	ufs_mtk_eh_err_cnt();
}

static int ufs_mtk_apply_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u16 mid = dev_info->wmanufacturerid;

	/* Replace default rpm_autosuspend_delay */
	hba->host->hostt->rpm_autosuspend_delay = MTK_RPM_AUTOSUSPEND_DELAY_MS;

	if (is_mcq_enabled(hba)) {
		/* Use none scheduler for mcq */
		if (hba->host->nr_hw_queues > 1) {
			hba->host->tag_set.flags |=
				BLK_MQ_F_NO_SCHED_BY_DEFAULT;
		}

		/* set affinity */
		ufs_mtk_mcq_set_irq_affinity(hba);
	}

	if (mid == UFS_VENDOR_SAMSUNG) {
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 6);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HIBERN8TIME), 10);
	} else if (mid == UFS_VENDOR_MICRON) {
		/* Only for the host which have TX skew issue */
		if (ufs_mtk_is_tx_skew_fix(hba) &&
			(STR_PRFX_EQUAL("MT128GBCAV2U31", dev_info->model) ||
			STR_PRFX_EQUAL("MT256GBCAV4U31", dev_info->model) ||
			STR_PRFX_EQUAL("MT512GBCAV8U31", dev_info->model) ||
			STR_PRFX_EQUAL("MT256GBEAX4U40", dev_info->model) ||
			STR_PRFX_EQUAL("MT512GAYAX4U40", dev_info->model) ||
			STR_PRFX_EQUAL("MT001TAYAX8U40", dev_info->model))) {
			ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 8);
		}
	}

	/*
	 * Decide waiting time before gating reference clock and
	 * after ungating reference clock according to vendors'
	 * requirements.
	 */
	if (mid == UFS_VENDOR_SAMSUNG)
		ufs_mtk_setup_ref_clk_wait_us(hba, 1);
	else if (mid == UFS_VENDOR_SKHYNIX)
		ufs_mtk_setup_ref_clk_wait_us(hba, 30);
	else if (mid == UFS_VENDOR_TOSHIBA)
		ufs_mtk_setup_ref_clk_wait_us(hba, 100);
	else
		ufs_mtk_setup_ref_clk_wait_us(hba,
					      REFCLK_DEFAULT_WAIT_US);

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* check only at the first init */
	if (!(hba->eh_flags || hba->pm_op_in_progress)) {
		/* sec specific features */
		ufs_sec_set_features(hba);
	}

	dev_info(hba->dev, "UFS device initialized\n");

	ufs_sec_config_features(hba);
#endif
	return 0;
}

static void ufs_mtk_fixup_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufshcd_fixup_dev_quirks(hba, ufs_mtk_dev_fixups);

	if (STR_PRFX_EQUAL("H9HQ15AFAMBDAR", dev_info->model))
		host->caps |= UFS_MTK_CAP_BROKEN_VCC |
			UFS_MTK_CAP_ALLOW_VCCQX_LPM;

	if (ufs_mtk_is_broken_vcc(hba) && hba->vreg_info.vcc &&
	    (hba->dev_quirks & UFS_DEVICE_QUIRK_DELAY_AFTER_LPM)) {
		hba->vreg_info.vcc->always_on = true;
		/*
		 * VCC will be kept always-on thus we don't
		 * need any delay during regulator operations
		 */
		hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM |
			UFS_DEVICE_QUIRK_DELAY_AFTER_LPM);
	}

	ufs_mtk_vreg_fix_vcc(hba);
	ufs_mtk_vreg_fix_vccqx(hba);
	ufs_mtk_fix_ahit(hba);
	ufs_mtk_fix_clock_scaling(hba);

	ufs_mtk_install_tracepoints(hba);
}

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
#define UFS_TEST_COUNT 3
#endif

static void ufs_mtk_event_notify(struct ufs_hba *hba,
				 enum ufs_event_type evt, void *data)
{
	unsigned int val = *(u32 *)data;
	unsigned long reg;
	uint8_t bit;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	trace_ufs_mtk_event(evt, val);

	/* Print details of UIC Errors */
	if (evt <= UFS_EVT_DME_ERR) {
		dev_info(hba->dev,
			 "Host UIC Error Code (%s): %08x\n",
			 ufs_uic_err_str[evt], val);
		reg = val;
	}

	if (evt == UFS_EVT_PA_ERR) {
		for_each_set_bit(bit, &reg, ARRAY_SIZE(ufs_uic_pa_err_str))
			dev_info(hba->dev, "%s\n", ufs_uic_pa_err_str[bit]);

		/* Got a LINERESET indication. */
		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
			ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_UIC_LINE_RESET);
		else
			ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_UIC);

		ufs_mtk_dbg_register_dump(hba);

		if ((reg & UIC_PHY_ADAPTER_LAYER_ERROR_CODE_MASK) == 0x3) {
			dev_info(hba->dev, "force reset only 0x80000003!\n");
			hba->force_reset = true;
			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;

			/* Disable UIC error intr to stop consecutive irq */
			ufsm_disable_intr(hba, UFSHCD_ERROR_MASK);

			queue_delayed_work(host->delay_eh_workq,
				&host->delay_eh_work,
				msecs_to_jiffies(1000));
		}
	}

	if (evt == UFS_EVT_FATAL_ERR) {
		/* intr_status */
		reg = val;
		if (reg & (CONTROLLER_FATAL_ERROR | SYSTEM_BUS_FATAL_ERROR)) {
			ufsm_disable_intr(hba, UFSHCD_ERROR_MASK);
			/* for fatal error, eh will be queued automatically */
		}
	}

	if (evt == UFS_EVT_DL_ERR) {
		for_each_set_bit(bit, &reg, ARRAY_SIZE(ufs_uic_dl_err_str))
			dev_info(hba->dev, "%s\n", ufs_uic_dl_err_str[bit]);

		ufs_mtk_dbg_phy_trace(hba, UFS_MPHY_UIC);

		ufs_mtk_dbg_register_dump(hba);
	}

	if (evt == UFS_EVT_ABORT)
		ufs_mtk_eh_abort(val);

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	if ((evt == UFS_EVT_PA_ERR ||
		evt == UFS_EVT_DL_ERR ||
		evt == UFS_EVT_LINK_STARTUP_FAIL)) {
		struct ufs_event_hist *e = &hba->ufs_stats.event[evt];

		if (e->cnt > UFS_TEST_COUNT) {
			if (evt == UFS_EVT_LINK_STARTUP_FAIL)
				ufs_mtk_dbg_register_dump(hba);
			BUG();
		}
	}
#endif

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_inc_op_err(hba, evt, data);
#endif
}

int ufs_mtk_auto_hibern8_disable(struct ufs_hba *hba)
{
	unsigned long flags;
	int ret;

	/* disable auto-hibern8 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* wait host return to idle state when auto-hibern8 off */
	ufs_mtk_wait_idle_state(hba, 5);

	ret = ufs_mtk_wait_link_state(hba, VS_LINK_UP, 100);
	if (ret) {
		dev_warn(hba->dev, "exit h8 state fail, ret=%d\n", ret);

		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->force_reset = true;
		hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
		schedule_work(&hba->eh_work);
		spin_unlock_irqrestore(hba->host->host_lock, flags);

		/* trigger error handler and break suspend */
		ret = -EBUSY;
	}

	return ret;
}

static void ufs_mtk_config_scaling_param(struct ufs_hba *hba,
				struct devfreq_dev_profile *profile,
				struct devfreq_simple_ondemand_data *data)
{
	/* Customize min gear in clk scaling */
	hba->clk_scaling.min_gear = UFS_HS_G4;

	hba->vps->devfreq_profile.polling_ms = 200;
	hba->vps->ondemand_data.upthreshold = 40;
	hba->vps->ondemand_data.downdifferential = 20;
}

static void _ufs_mtk_clk_scale(struct ufs_hba *hba, bool scale_up)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_clk *mclk = &host->mclk;
	struct ufs_clk_info *clki = mclk->ufs_sel_clki;
	struct ufs_clk_info *fde_clki = mclk->ufs_fde_clki;
	struct regulator *reg;
	int volt, ret = 0;
	bool clk_bind_vcore = false;
	bool clk_fde_scale = false;

	reg = host->mclk.reg_vcore;
	volt = host->mclk.vcore_volt;
	if (reg && volt != 0)
		clk_bind_vcore = true;

	if (mclk->ufs_fde_max_clki && mclk->ufs_fde_min_clki)
		clk_fde_scale = true;

	ret = clk_prepare_enable(clki->clk);
	if (ret) {
		dev_info(hba->dev,
			 "clk_prepare_enable() fail, ret: %d\n", ret);
		return;
	}

	if (clk_fde_scale) {
		ret = clk_prepare_enable(fde_clki->clk);
		if (ret) {
			dev_info(hba->dev,
				 "fde clk_prepare_enable() fail, ret: %d\n", ret);
			return;
		}
	}

	if (scale_up) {
		if (clk_bind_vcore) {
			ret = regulator_set_voltage(reg, volt, INT_MAX);
			if (ret) {
				dev_info(hba->dev,
					"Failed to set vcore to %d\n", volt);
				goto out;
			}
		}

		ret = clk_set_parent(clki->clk, mclk->ufs_sel_max_clki->clk);
		if (ret) {
			dev_info(hba->dev, "Failed to set clk mux, ret = %d\n",
				ret);
		}

		if (clk_fde_scale) {
			ret = clk_set_parent(fde_clki->clk,
				mclk->ufs_fde_max_clki->clk);
			if (ret) {
				dev_info(hba->dev,
					"Failed to set fde clk mux, ret = %d\n",
					ret);
			}
		}
	} else {
		if (clk_fde_scale) {
			ret = clk_set_parent(fde_clki->clk,
				mclk->ufs_fde_min_clki->clk);
			if (ret) {
				dev_info(hba->dev,
					"Failed to set fde clk mux, ret = %d\n",
					ret);
				goto out;
			}
		}

		ret = clk_set_parent(clki->clk, mclk->ufs_sel_min_clki->clk);
		if (ret) {
			dev_info(hba->dev, "Failed to set clk mux, ret = %d\n",
				ret);
			goto out;
		}

		if (clk_bind_vcore) {
			ret = regulator_set_voltage(reg, 0, INT_MAX);
			if (ret) {
				dev_info(hba->dev,
					"failed to set vcore to MIN\n");
			}
		}
	}

out:
	clk_disable_unprepare(clki->clk);

	if (clk_fde_scale)
		clk_disable_unprepare(fde_clki->clk);
}

/**
 * ufs_mtk_clk_scale - Internal clk scaling operation
 * MTK platform supports clk scaling by switching parent of ufs_sel(mux).
 * The ufs_sel downstream to ufs_ck and fed directly to UFS hardware.
 * Max and min clocks rate of ufs_sel defined in dts should match rate of
 * "ufs_sel_max_src" and "ufs_sel_min_src" respectively.
 * This prevent chaning rate of pll clock that is shared between modules.
 *
 * WARN: Need fix if pll clk rate is not static.
 *
 * @param hba: per adapter instance
 * @param scale_up: True for scaling up and false for scaling down
 */
static void ufs_mtk_clk_scale(struct ufs_hba *hba, bool scale_up)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_clk *mclk = &host->mclk;
	struct ufs_clk_info *clki = mclk->ufs_sel_clki;

	if (host->clk_scale_up == scale_up)
		goto out;

	if (scale_up)
		_ufs_mtk_clk_scale(hba, true);
	else
		_ufs_mtk_clk_scale(hba, false);

	host->clk_scale_up = scale_up;

	/* Must always set before clk_set_rate() */
	if (scale_up)
		clki->curr_freq = clki->max_freq;
	else
		clki->curr_freq = clki->min_freq;
out:
	trace_ufs_mtk_clk_scale(clki->name, scale_up, clk_get_rate(clki->clk));
}

static int ufs_mtk_clk_scale_notify(struct ufs_hba *hba, bool scale_up,
				    enum ufs_notify_change_status status)
{
	if (!ufshcd_is_clkscaling_supported(hba) || !hba->clk_scaling.is_enabled)
		return 0;

	if (status == PRE_CHANGE) {
		ufs_mtk_pm_qos(hba, scale_up);

		/* do parent/vcore switching before clk_set_rate() */
		ufs_mtk_clk_scale(hba, scale_up);
	}

	return 0;
}

static int ufs_mtk_get_hba_mac(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	/* MCQ operation not permitted */
	if (host->caps & UFS_MTK_CAP_DISABLE_MCQ)
		return -EPERM;

	return MAX_SUPP_MAC;
}

static int ufs_mtk_op_runtime_config(struct ufs_hba *hba)
{
	struct ufshcd_mcq_opr_info_t *opr;
	int i;

	for (i = 0; i < OPR_MAX; i++) {
		opr = &hba->mcq_opr[i];
		opr->stride = REG_UFS_MCQ_STRIDE;
	}

	hba->mcq_opr[OPR_SQD].offset = REG_UFS_MTK_SQD;
	hba->mcq_opr[OPR_SQIS].offset = REG_UFS_MTK_SQIS;
	hba->mcq_opr[OPR_CQD].offset = REG_UFS_MTK_CQD;
	hba->mcq_opr[OPR_CQIS].offset = REG_UFS_MTK_CQIS;

	hba->mcq_opr[OPR_SQD].base = hba->mmio_base + REG_UFS_MTK_SQD;
	hba->mcq_opr[OPR_SQIS].base = hba->mmio_base + REG_UFS_MTK_SQIS;
	hba->mcq_opr[OPR_CQD].base = hba->mmio_base + REG_UFS_MTK_CQD;
	hba->mcq_opr[OPR_CQIS].base = hba->mmio_base + REG_UFS_MTK_CQIS;

	return 0;
}

static int ufs_mtk_mcq_config_resource(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	/* fail mcq initialization if interrupt is not filled properly */
	if (!host->mcq_nr_intr) {
		dev_info(hba->dev, "IRQs not ready. MCQ disabled.");
		return -EINVAL;
	}

	hba->mcq_base = hba->mmio_base + MCQ_QUEUE_OFFSET(hba->mcq_capabilities);
	return 0;
}

static irqreturn_t ufs_mtk_mcq_intr(int irq, void *__intr_info)
{
	struct ufs_mtk_mcq_intr_info *mcq_intr_info = __intr_info;
	struct ufs_hba *hba = mcq_intr_info->hba;
	struct ufs_hw_queue *hwq;
	u32 events;
	int i = mcq_intr_info->qid;

	hwq = &hba->uhq[i];

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	ufshcd_vops_check_bus_status(hba);
#endif

	events = ufshcd_mcq_read_cqis(hba, i);
	if (events)
		ufshcd_mcq_write_cqis(hba, events, i);

	if (events & UFSHCD_MCQ_CQIS_TAIL_ENT_PUSH_STS)
		ufshcd_mcq_poll_cqe_lock(hba, hwq);

	return IRQ_HANDLED;
}

static int ufs_mtk_config_mcq_irq(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	u32 irq, i;
	int ret;

	for (i = 0; i < host->mcq_nr_intr; i++) {
		irq = host->mcq_intr_info[i].irq;
		if (irq == MTK_MCQ_INVALID_IRQ) {
			dev_err(hba->dev, "invalid irq. %d\n", i);
			return -ENOPARAM;
		}

		host->mcq_intr_info[i].qid = i;
		ret = devm_request_irq(hba->dev, irq, ufs_mtk_mcq_intr, 0, UFSHCD,
				       &host->mcq_intr_info[i]);

		dev_info(hba->dev, "request irq %d intr %s\n", irq, ret ? "failed" : "");

		if (ret)
			return ret;
	}

	return 0;
}

static int ufs_mtk_config_mcq(struct ufs_hba *hba, bool irq)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	if (!host->mcq_set_intr) {
		/* Disable irq option register */
		ufshcd_rmwl(hba, MCQ_INTR_EN_MSK, 0, REG_UFS_MMIO_OPT_CTRL_0);

		if (irq)
			ret = ufs_mtk_config_mcq_irq(hba);

		if (ret)
			return ret;

		host->mcq_set_intr = true;
	}

	ufshcd_rmwl(hba, MCQ_AH8, MCQ_AH8, REG_UFS_MMIO_OPT_CTRL_0);
	ufshcd_rmwl(hba, MCQ_INTR_EN_MSK, MCQ_MULTI_INTR_EN, REG_UFS_MMIO_OPT_CTRL_0);

	return 0;
}

static int ufs_mtk_config_esi(struct ufs_hba *hba)
{
	return ufs_mtk_config_mcq(hba, true);
}

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
static void ufs_mtk_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	ufs_mtk_dbg_phy_hibern8_notify(hba, cmd, status);
}

void _ufs_mtk_dbg_dump(struct ufs_hba *hba, u32 latest_cnt)
{
	ufs_mtk_dbg_dump(latest_cnt);
}
#endif

/*
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static const struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	.name                = "mediatek.ufshci",
	.init                = ufs_mtk_init,
	.get_ufs_hci_version = ufs_mtk_get_ufs_hci_version,
	.setup_clocks        = ufs_mtk_setup_clocks,
	.hce_enable_notify   = ufs_mtk_hce_enable_notify,
	.link_startup_notify = ufs_mtk_link_startup_notify,
	.pwr_change_notify   = ufs_mtk_pwr_change_notify,
#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	.hibern8_notify      = ufs_mtk_hibern8_notify,
#endif
	.apply_dev_quirks    = ufs_mtk_apply_dev_quirks,
	.fixup_dev_quirks    = ufs_mtk_fixup_dev_quirks,
	.suspend             = ufs_mtk_suspend,
	.resume              = ufs_mtk_resume,
	.dbg_register_dump   = ufs_mtk_dbg_register_dump,
	.device_reset        = ufs_mtk_device_reset,
	.event_notify        = ufs_mtk_event_notify,
	.config_scaling_param = ufs_mtk_config_scaling_param,
	.clk_scale_notify    = ufs_mtk_clk_scale_notify,
	/* mcq vops */
	.get_hba_mac         = ufs_mtk_get_hba_mac,
	.op_runtime_config   = ufs_mtk_op_runtime_config,
	.mcq_config_resource = ufs_mtk_mcq_config_resource,
	.config_esi          = ufs_mtk_config_esi,
#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG_BUILD)
	.check_bus_status    = ufs_mtk_check_bus_status,
	.dbg_dump            = _ufs_mtk_dbg_dump,
#endif
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev = &pdev->dev, *phy_dev = NULL;
	struct device_node *reset_node, *phy_node = NULL;
	struct platform_device *reset_pdev, *phy_pdev = NULL;
	struct device_link *link;
	struct ufs_hba *hba;
	struct ufs_mtk_host *host;

	reset_node = of_find_compatible_node(NULL, NULL,
					     "ti,syscon-reset");
	if (!reset_node) {
		dev_notice(dev, "find ti,syscon-reset fail\n");
		goto skip_reset;
	}
	reset_pdev = of_find_device_by_node(reset_node);
	if (!reset_pdev) {
		dev_notice(dev, "find reset_pdev fail\n");
		goto skip_reset;
	}
	link = device_link_add(dev, &reset_pdev->dev,
		DL_FLAG_AUTOPROBE_CONSUMER);
	if (!link) {
		dev_notice(dev, "add reset device_link fail\n");
		goto skip_reset;
	}
	/* supplier is not probed */
	if (link->status == DL_STATE_DORMANT) {
		err = -EPROBE_DEFER;
		goto out;
	}

skip_reset:
	/* find phy node */
	phy_node = of_parse_phandle(dev->of_node, "phys", 0);

	if (phy_node) {
		phy_pdev = of_find_device_by_node(phy_node);
		if (!phy_pdev)
			goto skip_phy;
		phy_dev = &phy_pdev->dev;

		pm_runtime_set_active(phy_dev);
		pm_runtime_enable(phy_dev);
		pm_runtime_get_sync(phy_dev);

		dev_info(dev, "phys node found\n");
	} else {
		dev_notice(dev, "phys node not found\n");
	}

skip_phy:
	/* perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);
	if (err) {
		dev_err(dev, "probe failed %d\n", err);
		goto out;
	}

	hba = platform_get_drvdata(pdev);
	if (!hba)
		goto out;

	/* set affinity to cpu3 */
	if (hba->irq)
		irq_set_affinity_hint(hba->irq, get_cpu_mask(3));

	if ((phy_node) && (phy_dev)) {
		host = ufshcd_get_variant(hba);
		host->phy_dev = phy_dev;
	}

	/*
	 * Because the default power setting of VSx (the upper layer of
	 * VCCQ/VCCQ2) is HWLP, we need to prevent VSx from
	 * entering LPM.
	 */
	ufs_mtk_vsx_set_lpm(hba, false);

	/* For fool-proof, we need to prevent VUFSx from entering LPM. */
	if (hba->vreg_info.vccq)
		regulator_set_mode((hba->vreg_info.vccq)->reg,
				REGULATOR_MODE_NORMAL);

	if (hba->vreg_info.vccq2)
		regulator_set_mode((hba->vreg_info.vccq2)->reg,
				REGULATOR_MODE_NORMAL);

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_register_vendor_hooks();
	ufs_sec_init_logging(dev);
#endif

out:
	of_node_put(phy_node);
	of_node_put(reset_node);
	return err;
}

/**
 * ufs_mtk_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always return 0
 */
static int ufs_mtk_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);

	ufs_mtk_remove_sysfs(hba);

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_remove_features(hba);
#endif

	ufshcd_remove(hba);

	ufs_mtk_btag_exit(hba);

	ufs_mtk_uninstall_tracepoints();

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ufs_mtk_system_suspend(struct device *dev)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
	struct arm_smccc_res res;

	host = ufshcd_get_variant(hba);
	if (down_trylock(&host->rpmb_sem))
		return -EBUSY;

	/* Check if shutting down */
	if (!ufshcd_is_user_access_allowed(hba)) {
		ret = -EBUSY;
		goto out;
	}

	ret = ufshcd_system_suspend(dev);

	if (pm_runtime_suspended(hba->dev))
		goto out;

	if (!ret)
		ufs_mtk_dev_vreg_set_lpm(hba, true);

	if (!ret && ufs_mtk_is_rtff_mtcmos(hba))
		ufs_mtk_mtcmos_ctrl(false, res);
out:
	if (ret)
		up(&host->rpmb_sem);

	return ret;
}

static int ufs_mtk_system_resume(struct device *dev)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
	struct arm_smccc_res res;

	if (pm_runtime_suspended(hba->dev))
		goto out;

	if (ufs_mtk_is_rtff_mtcmos(hba))
		ufs_mtk_mtcmos_ctrl(true, res);

	ufs_mtk_dev_vreg_set_lpm(hba, false);

out:
	ret = ufshcd_system_resume(dev);

	host = ufshcd_get_variant(hba);
	if (!ret) {
		up(&host->rpmb_sem);
#ifdef CONFIG_PM_SLEEP
		queue_delayed_work(host->rq_workq, &host->rq_dwork,
				   msecs_to_jiffies(500));
#endif
	}

	return ret;
}
#endif

#ifdef CONFIG_PM
static int ufs_mtk_runtime_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;
	int ret = 0;

	ret = ufshcd_runtime_suspend(dev);
	if (ret)
		return ret;

	ufs_mtk_dev_vreg_set_lpm(hba, true);

	if (ufs_mtk_is_rtff_mtcmos(hba))
		ufs_mtk_mtcmos_ctrl(false, res);

	if (host->phy_dev)
		pm_runtime_put_sync(host->phy_dev);

	return 0;
}

static int ufs_mtk_runtime_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int ret = 0;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;

	if (ufs_mtk_is_rtff_mtcmos(hba))
		ufs_mtk_mtcmos_ctrl(true, res);

	if (host->phy_dev)
		pm_runtime_get_sync(host->phy_dev);

	ufs_mtk_dev_vreg_set_lpm(hba, false);

	ret = ufshcd_runtime_resume(dev);

	return ret;
}

void ufs_mtk_shutdown(struct platform_device *pdev)
{
	ufshcd_pltfrm_shutdown(pdev);
}
#endif

static const struct dev_pm_ops ufs_mtk_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ufs_mtk_system_suspend,
				ufs_mtk_system_resume)
	SET_RUNTIME_PM_OPS(ufs_mtk_runtime_suspend,
			   ufs_mtk_runtime_resume, NULL)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
};

static struct platform_driver ufs_mtk_pltform = {
	.probe      = ufs_mtk_probe,
	.remove     = ufs_mtk_remove,
	.shutdown   = ufs_mtk_shutdown,
	.driver = {
		.name   = "ufshcd-mtk",
		.pm     = &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_AUTHOR("Peter Wang <peter.wang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek UFS Host Driver");
MODULE_LICENSE("GPL v2");

module_platform_driver(ufs_mtk_pltform);
