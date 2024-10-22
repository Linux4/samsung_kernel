// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/bits.h>
#include <linux/build_bug.h>
#include <clk-fmeter.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MFD_MT6397)
#include <linux/mfd/mt6397/core.h>
#endif /* IS_ENABLED(CONFIG_DEVICE_MODULES_MFD_MT6397) */

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif /* IS_ENABLED(CONFIG_OF) */

#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)			"[scp_dvfs]: " fmt

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MFD_MT6397)
#define pmic_main_chip			mt6397_chip
#endif

/* DTS SCP Clock Names */
#define SCP_CLOCK_NAMES_SCP_SEL        "clk_mux"
#define SCP_CLOCK_NAMES_SCP_CLKS       "clk_pll_%d"
/* DTS Property Names */
#define PROPNAME_SCP_DVFS_DISABLE      "scp-dvfs-disable"
#define PROPNAME_SCP_DVFS_CORES        "scp-cores"
#define PROPNAME_SCP_CORE_ONLINE_MASK  "scp-core-online-mask"
#define PROPNAME_SCP_VLP_SUPPORT       "vlp-support"
#define PROPNAME_SCP_VLPCK_SUPPORT     "vlpck-support"
#define PROPNAME_PMIC                  "pmic"
#define PROPNAME_PMIC_VOW_LP_EN_GEAR   "vow-lp-en-gear"
#define PROPNAME_PMIC_NO_RG_RW         "no-pmic-rg-access"
#define PROPNAME_PMIC_SSHUB_SUP        "pmic-sshub-support"
#define PROPNAME_SCP_DVFSRC            "dvfsrc-vscp"
#define PROPNAME_SCP_VCORE             "sshub-vcore"
#define PROPNAME_SCP_VSRAM             "sshub-vsram"
#define PROPNAME_SCP_DVFS_OPP          "dvfs-opp"
#define OPP_ELEM_CNT                   (7)
#define NO_DVFSRC_OPP                  (0xff)
#define PROPNAME_DO_U2_CALI            "do-ulposc-cali"
#define PROPNAME_FM_CLK                "fmeter-clksys"
#define PROPNAME_ULPOSC_CLK            "ulposc-clksys"
#define PROPNAME_SCP_CLK_CTRL          "scp-clk-ctrl"
#define PROPNAME_SCP_CLK_HW_VER        "scp-clk-hw-ver"
#define PROPNAME_U2_CALI_VER           "ulposc-cali-ver"
#define PROPNAME_U2_CALI_NUM           "ulposc-cali-num"
#define PROPNAME_U2_CALI_TARGET        "ulposc-cali-target"
#define PROPNAME_U2_CALI_CONFIG        "ulposc-cali-config"
#define CALI_CONFIG_ELEM_CNT           (3)
#define PROPNAME_SCP_CLK_DBG_VER       "clk-dbg-ver"
#define PROPNAME_U2_FM_SUPPORT         "ccf-fmeter-support"
#define PROPNAME_NOT_SUPPORT_VLP_FM    "not-support-vlp-fmeter"
#define PROPNAME_SCP_DVFS_SECURE       "secure-access"
#define PROPNAME_SCP_DVFS_FLAG         "scp-dvfs-flag"

#define FM_CNT2FREQ(cnt)	(cnt * 26 / CALI_DIV_VAL)
#define FM_FREQ2CNT(freq)	(freq * CALI_DIV_VAL / 26)

unsigned int scp_ipi_ackdata0, scp_ipi_ackdata1;
struct ipi_tx_data_t {
	unsigned int arg1;
	unsigned int arg2;
};

struct ipi_request_freq_data {
	uint32_t scp_req_freq:16,
			 sap_req_freq:16;
};

/*
 * g_is_scp_dvfs_feature_enable: enable feature or not (by dts)
 * g_scp_dvfs_init_done: if probe was finished or feature was not enabled
 * g_scp_dvfs_flow_enable: run dvfs flow or not (by dts or adb command)
 */
static atomic_t g_is_scp_dvfs_feature_enable;
static atomic_t g_scp_dvfs_init_done;
static bool g_scp_dvfs_flow_enable = true; /* 1: RUN DVFS FLOW, others: NO DVFS */
/*
 * -1  : No SCP Debug CMD
 * >=0 : SCP DVFS Debug OPP.
 */
#define NO_SCP_DEBUG_OPP	(-1)
static int current_scp_debug_opp = NO_SCP_DEBUG_OPP;
static struct wakeup_source *scp_dvfs_lock;

/* Devive specific settings initialized from dts */
static struct mt_scp_pll_t mt_scp_pll_dev;
static struct scp_dvfs_hw g_dvfs_dev;
static struct regulator *dvfsrc_vscp_power;
static struct regulator *reg_vcore;
static struct regulator *reg_vsram;

const char *ulposc_ver[MAX_ULPOSC_VERSION] __initconst = {
	[ULPOSC_VER_1] = "v1",
	[ULPOSC_VER_2] = "v2",
};

const char *clk_dbg_ver[MAX_CLK_DBG_VERSION] __initconst = {
	[CLK_DBG_VER_1] = "v1",
	[CLK_DBG_VER_2] = "v2",
	[CLK_DBG_VER_3] = "v3",
	[CLK_DBG_VER_4] = "v4",
};

const char *scp_clk_ver[MAX_SCP_CLK_VERSION] __initconst = {
	[SCP_CLK_VER_1] = "v1",
};

const char *scp_dvfs_hw_chip_ver[MAX_SCP_DVFS_CHIP_HW] __initconst = {
	[MT6853] = "mediatek,mt6853",
	[MT6873] = "mediatek,mt6873",
	[MT6893] = "mediatek,mt6893",
	[MT6833] = "mediatek,mt6833",
};

struct ulposc_cali_regs cali_regs[MAX_ULPOSC_VERSION] __initdata = {
	[ULPOSC_VER_1] = {
		REG_DEFINE(con0, 0x2C0, REG_MAX_MASK, 0)
		REG_DEFINE(cali, 0x2C0, GENMASK(CAL_BITS - 1, 0), 0)
		REG_DEFINE(con1, 0x2C4, REG_MAX_MASK, 0)
		REG_DEFINE(con2, 0x2C8, REG_MAX_MASK, 0)
	},
	[ULPOSC_VER_2] = { /* Suppose VLP_CKSYS is from 0x1C013000 */
		REG_DEFINE(con0, 0x210, REG_MAX_MASK, 0)
		REG_DEFINE(cali_ext, 0x210, GENMASK(CAL_EXT_BITS - 1, 0), 7)
		REG_DEFINE_WITH_INIT(cali, 0x210, GENMASK(CAL_BITS - 1, 0), 0, 0x40, 0)
		REG_DEFINE(con1, 0x214, REG_MAX_MASK, 0)
		REG_DEFINE(con2, 0x218, REG_MAX_MASK, 0)
	},
};

struct clk_cali_regs clk_dbg_reg[MAX_CLK_DBG_VERSION] __initdata = {
	[CLK_DBG_VER_1] = {
		REG_DEFINE(clk_misc_cfg0, 0x140, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(meter_div, 0x140, 0xFF, 24, 0, 0)

		REG_DEFINE(clk_dbg_cfg, 0x17C, 0x3, 0)
		REG_DEFINE_WITH_INIT(fmeter_ck_sel, 0x17C, 0x3F, 16, 36, 0)
		REG_DEFINE_WITH_INIT(abist_clk, 0x17C, 0x3, 0, 0, 0)

		REG_DEFINE(clk26cali_0, 0x220, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(fmeter_en, 0x220, 0x1, 12, 1, 0)
		REG_DEFINE_WITH_INIT(trigger_cal, 0x220, 0x1, 4, 1, 0)

		REG_DEFINE(clk26cali_1, 0x224, REG_MAX_MASK, 0)
		REG_DEFINE(cal_cnt, 0x224, 0xFFFF, 0)
		REG_DEFINE_WITH_INIT(load_cnt, 0x224, 0x3FF, 16, 0x1FF, 0)
	},
	[CLK_DBG_VER_2] = {/* Suppose VLP_CKSYS is from 0x1C013000 */
		REG_DEFINE(clk26cali_0, 0x230, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(fmeter_ck_sel, 0x230, 0x1F, 16, 0x16, 0)
		REG_DEFINE_WITH_INIT(fmeter_rst, 0x230, 0x1, 15, 0x1, 0)
		REG_DEFINE_WITH_INIT(fmeter_en, 0x230, 0x1, 12, 0x1, 0)
		REG_DEFINE_WITH_INIT(trigger_cal, 0x230, 0x1, 4, 1, 0)

		REG_DEFINE(clk26cali_1, 0x234, REG_MAX_MASK, 0)
		REG_DEFINE(cal_cnt, 0x234, 0xFFFF, 0)
		REG_DEFINE_WITH_INIT(load_cnt, 0x234, 0x3FF, 16, 0x1FF, 0)
	},
	[CLK_DBG_VER_3] = {/* Suppose TOPCKGEN is from 0x10000000 */
		REG_DEFINE(clk26cali_0, 0x220, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(fmeter_en, 0x220, 0x1, 12, 0x1, 0)
		REG_DEFINE_WITH_INIT(trigger_cal, 0x220, 0x1, 4, 1, 0)

		REG_DEFINE(clk26cali_1, 0x224, REG_MAX_MASK, 0)
		REG_DEFINE(cal_cnt, 0x224, 0xFFFF, 0)
		REG_DEFINE_WITH_INIT(load_cnt, 0x224, 0x3FF, 16, 0x1FF, 0)

		REG_DEFINE_WITH_INIT(fmeter_ck_sel, 0x28C, 0x3F, 16, 0x3E, 0)

		REG_DEFINE_WITH_INIT(clk_misc_cfg0, 0x240, REG_MAX_MASK, 0, 0x90F00, 0)
	},
	[CLK_DBG_VER_4] = {
		REG_DEFINE(clk26cali_0, 0x230, REG_MAX_MASK, 0)
		REG_DEFINE_WITH_INIT(fmeter_ck_sel, 0x230, 0x1F, 16, 0x19, 0)
		REG_DEFINE_WITH_INIT(fmeter_rst, 0x230, 0x1, 15, 0x1, 0)
		REG_DEFINE_WITH_INIT(fmeter_en, 0x230, 0x1, 12, 0x1, 0)
		REG_DEFINE_WITH_INIT(trigger_cal, 0x230, 0x1, 4, 1, 0)

		REG_DEFINE(clk26cali_1, 0x234, REG_MAX_MASK, 0)
		REG_DEFINE(cal_cnt, 0x234, 0xFFFF, 0)
		REG_DEFINE_WITH_INIT(load_cnt, 0x234, 0x3FF, 16, 0x1FF, 0)
	},
};

struct scp_clk_hw scp_clk_hw_regs[MAX_SCP_CLK_VERSION] = {
	[SCP_CLK_VER_1] = {
		REG_DEFINE(clk_high_en, 0x4, 0x1, 1)
		REG_DEFINE(ulposc2_en, 0x6C, 0x1, 5)
		REG_DEFINE(ulposc2_cg, 0x5C, 0x1, 1)
		REG_DEFINE(sel_clk, 0x0, 0xF, 8)
	}
};

struct scp_pmic_regs scp_pmic_hw_regs[MAX_SCP_DVFS_CHIP_HW] = {
	[MT6853] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x1520, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1514, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x151a, 0x1, 11, 1, 1)
	},
	[MT6873] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x15a0, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1594, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x159a, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_buck_en, 0x15aa, 0x1, 0, 1, 0)
		REG_DEFINE_WITH_INIT(sshub_ldo_en, 0x1f28, 0x1, 0, 0, 0)
		REG_DEFINE_WITH_INIT(pmrc_en, 0x1ac, 0x1, 2, 0, 1)
	},
	[MT6893] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x15a0, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1594, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x159a, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_buck_en, 0x15aa, 0x1, 0, 1, 0)
		REG_DEFINE_WITH_INIT(sshub_ldo_en, 0x1f28, 0x1, 0, 0, 0)
		REG_DEFINE_WITH_INIT(pmrc_en, 0x1ac, 0x1, 2, 0, 1)
	},
	[MT6833] = {
		REG_DEFINE_WITH_INIT(sshub_op_mode, 0x1520, 0x1, 11, 0, 1)
		REG_DEFINE_WITH_INIT(sshub_op_en, 0x1514, 0x1, 11, 1, 1)
		REG_DEFINE_WITH_INIT(sshub_op_cfg, 0x151a, 0x1, 11, 1, 1)
	},
};

static bool is_core_online(uint32_t core_id)
{
	return (CORE_ONLINE_MSK << core_id) & g_dvfs_dev.core_online_msk;
}

static void slp_ipi_init(void)
{
	int ret;

	ret = mtk_ipi_register(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		NULL, NULL, &scp_ipi_ackdata0);
	if (ret) {
		pr_notice("scp0 slp ipi register failed\n");
		WARN_ON(1);
	}

	if (is_core_online(SCP_CORE_1)) {
		ret = mtk_ipi_register(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			NULL, NULL, &scp_ipi_ackdata1);
		if (ret)
			pr_notice("scp1 slp ipi register failed\n");
	}
	if (!ret)
		g_dvfs_dev.sleep_init_done = true;
}

static int scp_get_vcore_table(unsigned int gear)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, VCORE_ACQUIRE,
		gear, 0, 0, 0, 0, 0, &res);
	if (!(res.a0))
		return res.a1;

	pr_notice("[%s]: should not end here\n", __func__);
	return -1;
}

int scp_resource_req(unsigned int req_type)
{
	struct arm_smccc_res res;

	if (req_type >= SCP_REQ_MAX)
		return 0;

	pr_notice("%s(0x%x)\n", __func__, req_type);

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, RESOURCE_REQ,
		req_type, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		pr_notice("[%s]: resource request failed, req: %u\n",
			__func__, req_type);

	return res.a0;
}

static int scp_set_scp2spm_vol(unsigned int spm_opp)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, SCP2SPM_VOL_SET,
		spm_opp, 0, 0, 0, 0, 0, &res);

	if (res.a0)
		pr_notice("[%s] smc call failed with error: %lu\n",
		__func__, res.a0);
	return res.a0;
}

static int scp_reg_update(struct regmap *regmap, struct reg_info *reg, u32 val)
{
	u32 mask;
	int ret = 0;
	bool need_scp_wakelock = false;

	if (!reg->msk) {
		pr_notice("[%s]: reg not support\n", __func__);
		return -ESCP_REG_NOT_SUPPORTED;
	}
	mask = reg->msk << reg->bit;
	val = (val & reg->msk) << reg->bit;

	/* Keep SCP active to read */
	need_scp_wakelock = g_dvfs_dev.clk_hw->scp_clk_regmap == regmap;
	if (need_scp_wakelock)
		if (scp_awake_lock((void *)SCP_A_ID)) {
			pr_notice("[%s] scp_awake_lock failed\n", __func__);
			return 0;
		}

	if (reg->setclr) {
		ret = regmap_write(regmap, reg->ofs + 4, mask);
		ret = regmap_write(regmap, reg->ofs + 2, val);
	} else {
		ret = regmap_update_bits(regmap, reg->ofs, mask, val);
	}

	if (need_scp_wakelock)
		scp_awake_unlock((void *)SCP_A_ID);

	return ret;
}

static int scp_reg_read(struct regmap *regmap, struct reg_info *reg, u32 *val)
{
	int ret = 0;
	bool need_scp_wakelock = false;

	if (!reg->msk) {
		pr_notice("[%s]: reg not support\n", __func__);
		return -ESCP_REG_NOT_SUPPORTED;
	}

	/* Keep SCP active to read */
	need_scp_wakelock = g_dvfs_dev.clk_hw->scp_clk_regmap == regmap;
	if (need_scp_wakelock)
		if (scp_awake_lock((void *)SCP_A_ID)) {
			pr_notice("[%s] scp_awake_lock failed\n", __func__);
			return 0;
		}

	ret = regmap_read(regmap, reg->ofs, val);
	if (!ret)
		*val = (*val >> reg->bit) & reg->msk;

	if (need_scp_wakelock)
		scp_awake_unlock((void *)SCP_A_ID);

	return ret;
}

static inline int scp_reg_init(struct regmap *regmap, struct reg_info *reg)
{
	return scp_reg_update(regmap, reg, reg->init_config);
}

static int scp_get_freq_idx(unsigned int clk_opp)
{
	int i;

	for (i = 0; i < g_dvfs_dev.scp_opp_nums; i++)
		if (clk_opp == g_dvfs_dev.opp[i].freq)
			break;

	if (i == g_dvfs_dev.scp_opp_nums) {
		pr_notice("no available opp, freq: %u\n", clk_opp);
		return -EINVAL;
	}

	return i;
}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static int scp_update_pmic_vow_lp_mode(bool on)
{
	int ret = 0;

	if (g_dvfs_dev.vlp_support || g_dvfs_dev.bypass_pmic_rg_access) {
		pr_notice("[%s]: VCORE DVS is not supported\n", __func__);
		WARN_ON(1);
		return -ESCP_DVFS_DVS_SHOULD_BE_BYPASSED;
	}

	if (on)
		ret = scp_reg_update(g_dvfs_dev.pmic_regmap,
			&g_dvfs_dev.pmic_regs->_sshub_op_cfg, 1);
	else
		ret = scp_reg_update(g_dvfs_dev.pmic_regmap,
			&g_dvfs_dev.pmic_regs->_sshub_op_cfg, 0);

	return ret;
}
#endif /* CONFIG_FPGA_EARLY_PORTING */

static int scp_set_pmic_vcore(unsigned int cur_freq)
{
	int ret = 0;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int idx = 0;
	unsigned int ret_vc = 0, ret_vs = 0;

	if (g_dvfs_dev.vlp_support) {
		pr_notice("[%s]: VCORE DVS is not supported\n", __func__);
		WARN_ON(1);
		return -ESCP_DVFS_DVS_SHOULD_BE_BYPASSED;
	}

	idx = scp_get_freq_idx(cur_freq);
	if (idx >= 0 && idx < g_dvfs_dev.scp_opp_nums) {
		ret = scp_get_vcore_table(g_dvfs_dev.opp[idx].dvfsrc_opp);
		if (ret > 0)
			g_dvfs_dev.opp[idx].tuned_vcore = ret;

		ret_vc = regulator_set_voltage(reg_vcore,
				g_dvfs_dev.opp[idx].tuned_vcore,
				g_dvfs_dev.opp[g_dvfs_dev.scp_opp_nums - 1].vcore + 100000);

		ret_vs = regulator_set_voltage(reg_vsram,
				g_dvfs_dev.opp[idx].vsram,
				g_dvfs_dev.opp[g_dvfs_dev.scp_opp_nums - 1].vsram + 100000);
	} else {
		ret = -ESCP_DVFS_OPP_OUT_OF_BOUND;
		pr_notice("[%s]: cur_freq=%d is not supported\n",
			__func__, cur_freq);
		WARN_ON(1);
	}

	if (ret_vc != 0 || ret_vs != 0) {
		ret = -ESCP_DVFS_PMIC_REGULATOR_FAILED;
		pr_notice("[%s]: ERROR: scp vcore/vsram setting error, (%d, %d)\n",
				__func__, ret_vc, ret_vs);
		WARN_ON(1);
	}

	if (g_dvfs_dev.vow_lp_en_gear != -1) {
		/* vcore > 0.6v cannot hold pmic/vcore in lp mode */
		if (idx < g_dvfs_dev.vow_lp_en_gear)
			/* enable VOW low power mode */
			scp_update_pmic_vow_lp_mode(true);
		else
			/* disable VOW low power mode */
			scp_update_pmic_vow_lp_mode(false);
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

	return ret;
}

static uint32_t sum_required_freq(uint32_t core_id)
{
	uint32_t i = 0;
	uint32_t sum = 0;

	if (!is_core_online(core_id)) {
		pr_notice("[%s]: ERROR: core_id is invalid: %u\n",
				__func__, core_id);
		WARN_ON(1);
		core_id = SCPSYS_CORE0;
	}

	/*
	 * calculate scp frequency for core_id
	 */
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (i != VCORE_TEST_FEATURE_ID &&
			feature_table[i].enable == 1 &&
			feature_table[i].sys_id == core_id)
			sum += feature_table[i].freq;
	}

	return sum;
}

static uint32_t _mt_scp_dvfs_set_test_freq(uint32_t sum)
{
	uint32_t freq = 0, added_freq = 0, i = 0;

	if (current_scp_debug_opp == NO_SCP_DEBUG_OPP)
		return 0;

	pr_info("manually set opp = %d\n", current_scp_debug_opp);

	for (i = 0; i < g_dvfs_dev.scp_opp_nums; i++) {
		freq = g_dvfs_dev.opp[i].freq;

		if (current_scp_debug_opp == i && sum < freq) {
			added_freq = freq - sum;
			break;
		}
	}
	feature_table[VCORE_TEST_FEATURE_ID].freq = added_freq;
	pr_notice("[%s]test freq: %d + %d = %d (MHz)\n",
			__func__,
			sum,
			added_freq,
			sum + added_freq);

	return added_freq;
}

uint32_t scp_get_freq(void)
{
	uint32_t i;
	uint32_t single_core_sum = 0;
	uint32_t sum = 0;
	uint32_t return_freq = 0;

	/*
	 * calculate scp frequency requirement (debug freq is not included)
	 * - Only to find the max required freq all enabled cores.
	 * - The freq of core2 should be caculated seperately.
	 */
	for (i = 0; i < SCP_MAX_CORE_NUM ; i++) {
		if (!is_core_online(i))
			continue;
		single_core_sum = sum_required_freq(i);

		if(i == SCP_CORE_2) {
			/* core2 has its own clock source */
			sap_expected_freq = single_core_sum;
			continue;
		}

		if (single_core_sum > sum) {
			sum = single_core_sum;
			feature_table[VCORE_TEST_FEATURE_ID].sys_id = i;
		}
	}

	/*
	 * add scp test cmd frequency (if in debug mode)
	 */
	sum += _mt_scp_dvfs_set_test_freq(sum);

	for (i = 0; i < g_dvfs_dev.scp_opp_nums; i++) {
		if (sum <= g_dvfs_dev.opp[i].freq) {
			return_freq = g_dvfs_dev.opp[i].freq;
			break;
		}
	}

	if (i == g_dvfs_dev.scp_opp_nums) {
		return_freq = g_dvfs_dev.opp[g_dvfs_dev.scp_opp_nums - 1].freq;
		pr_notice("warning: request freq %d > max opp %d\n",
				sum, return_freq);
	}

	return return_freq;
}

static void scp_vcore_request(unsigned int clk_opp)
{
	int idx;
	int ret = 0;

	pr_debug("[%s]: opp(%d)\n", __func__, clk_opp);

	if (g_dvfs_dev.vlp_support)
		return;

	/* SCP vcore request to PMIC */
	if (g_dvfs_dev.pmic_sshub_en)
		ret = scp_set_pmic_vcore(clk_opp);

	idx = scp_get_freq_idx(clk_opp);
	if (idx < 0) {
		pr_notice("[%s]: invalid clk_opp %d\n", __func__, clk_opp);
		WARN_ON(1);
		return;
	}

	/* SCP vcore request to DVFSRC
	 * min & max set to requested Vcore value
	 * DVFSRC SW will find corresponding idx to process
	 * if opp[idx].dvfsrc_opp == NO_DVFSRC_OPP, means that
	 * opp[idx] is not supported by DVFSRC
	 */
	if (g_dvfs_dev.opp[idx].dvfsrc_opp != NO_DVFSRC_OPP)
		ret = regulator_set_voltage(dvfsrc_vscp_power,
			g_dvfs_dev.opp[idx].vcore,
			g_dvfs_dev.opp[idx].vcore);
	if (ret) {
		pr_notice("[%s]: dvfsrc vcore update error, opp: %d\n",
			__func__, idx);
		WARN_ON(1);
	}

	/* SCP vcore request to SPM */
	if (g_dvfs_dev.secure_access_scp)
		scp_set_scp2spm_vol(g_dvfs_dev.opp[idx].spm_opp);
	else {
		/* LEGACY - For projects that do not support SMC SCP2SPM_VOL_SET
		 * New projects should use the scp_set_scp2spm_vol() instead.
		 */
		writel(g_dvfs_dev.opp[idx].spm_opp, SCP_SCP2SPM_VOL_LV);
	}
}

void scp_init_vcore_request(void)
{
	/* Init vcore gear in case dvfs flow was disabled */
	if (!g_scp_dvfs_flow_enable)
		scp_vcore_request(g_dvfs_dev.opp[0].freq);
}

static int scp_request_freq_vcore(void)
{
	int timeout = 50;
	int ret = 0;
	unsigned long spin_flags;
	bool is_increasing_freq = false;
	int opp_idx;

	if (!g_scp_dvfs_flow_enable) {
		pr_debug("[%s]: warning: SCP DVFS is OFF\n", __func__);
		return 0;
	}

	/* because we are waiting for scp to update register:scp_current_freq
	 * use wake lock to prevent AP from entering suspend state
	 */
	__pm_stay_awake(scp_dvfs_lock);

	if (scp_current_freq != scp_expected_freq) {
		/* wake up scp */
		if (scp_awake_lock((void *)SCP_A_ID)) {
			pr_notice("[%s] scp_awake_lock failed\n", __func__);
			goto FINISH;
		}

		/* do DVS before DFS if increasing frequency */
		if (scp_current_freq < scp_expected_freq) {
			scp_vcore_request(scp_expected_freq);
			is_increasing_freq = true;
		}

		/* Request SPM not to turn off mainpll/26M/infra */
		/* because SCP may park in it during DFS process */
		scp_resource_req(SCP_REQ_26M |
				SCP_REQ_INFRA |
				SCP_REQ_SYSPLL);

		/*  turn on PLL if necessary */
		scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);

		do {
			ret = mtk_ipi_send(&scp_ipidev,
				IPI_OUT_DVFS_SET_FREQ_0,
				IPI_SEND_WAIT, &scp_expected_freq,
				PIN_OUT_SIZE_DVFS_SET_FREQ_0, 0);
			if (ret != IPI_ACTION_DONE)
				pr_notice("SCP send IPI fail - %d, %d\n", ret, timeout);

			mdelay(2);
			timeout -= 1; /*try 50 times, total about 100ms*/
			if (timeout <= 0) {
				pr_notice("set freq fail, current(%d) != expect(%d)\n",
					scp_current_freq, scp_expected_freq);
				scp_resource_req(SCP_REQ_RELEASE);
				scp_awake_unlock((void *)SCP_A_ID);
				__pm_relax(scp_dvfs_lock);
				WARN_ON(1);
				return -ESCP_DVFS_IPI_FAILED;
			}

			/* read scp_current_freq again */
			spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
			scp_current_freq = readl(CURRENT_FREQ_REG); /* Keep SCP active to read */
			spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

		} while (scp_current_freq != scp_expected_freq);

		/* turn off PLL if necessary */
		scp_pll_ctrl_set(PLL_DISABLE, scp_expected_freq);

		/* do DVS after DFS if decreasing frequency */
		if (!is_increasing_freq)
			scp_vcore_request(scp_expected_freq);

		scp_awake_unlock((void *)SCP_A_ID);

		opp_idx = scp_get_freq_idx(scp_current_freq);
		if (g_dvfs_dev.opp[opp_idx].resource_req)
			scp_resource_req(g_dvfs_dev.opp[opp_idx].resource_req);
		else
			scp_resource_req(SCP_REQ_RELEASE);
	}

	pr_debug("[SCP] succeed to set freq, expect=%d, cur=%d\n",
			scp_expected_freq, scp_current_freq);
FINISH:
	__pm_relax(scp_dvfs_lock);
	return 0;
}

static int scp_request_freq_vlp(void)
{
	struct ipi_request_freq_data ipi_data;
	int timeout = 50;
	int ret = 0;
	int opp_idx;

	if (!g_scp_dvfs_flow_enable) {
		pr_debug("[%s]: warning: SCP DVFS is OFF\n", __func__);
		return 0;
	}

	if (!g_dvfs_dev.vlp_support) {
		pr_notice("[%s]: should not end here: vlp not supported!\n", __func__);
		return 0;
	}

	/*
	 * In order to prevent sending the same freq request from kernel repeatedly,
	 * we used last_scp_expected_freq to record last freq request.
	 */
	if (last_scp_expected_freq == scp_expected_freq
			&& last_sap_expected_freq == sap_expected_freq) {
		pr_debug("[SCP] Skip DFS: the same freq request SCP: %dMhz, SAP: %dMhz\n",
			last_scp_expected_freq, last_sap_expected_freq);
		return 0;
	}

	/* because we are waiting for scp to update the frequency
	 * use wake lock to prevent AP from entering suspend state
	 */
	__pm_stay_awake(scp_dvfs_lock);

	if (last_scp_expected_freq != scp_expected_freq
			|| last_sap_expected_freq != sap_expected_freq) {

		ipi_data.scp_req_freq = scp_expected_freq;
		ipi_data.sap_req_freq = sap_expected_freq;

		/* wake up scp */
		if (scp_awake_lock((void *)SCP_A_ID)) {
			pr_notice("[%s] scp_awake_lock failed\n", __func__);
			goto FINISH;
		}

		if (g_dvfs_dev.has_pll_opp) {
			/* Request SPM not to turn off mainpll/26M/infra */
			/* because SCP may park in it during DFS process */
			scp_resource_req(SCP_REQ_26M |
					SCP_REQ_INFRA |
					SCP_REQ_SYSPLL);

			/*  turn on PLL if necessary */
			scp_pll_ctrl_set(PLL_ENABLE, scp_expected_freq);
		}

		do {
			ret = mtk_ipi_send(&scp_ipidev,
				IPI_OUT_DVFS_SET_FREQ_0,
				IPI_SEND_WAIT, &ipi_data,
				PIN_OUT_SIZE_DVFS_SET_FREQ_0, 0);
			mdelay(2);
			if (ret != IPI_ACTION_DONE) {
				pr_notice("SCP send IPI fail - %d, %d\n", ret, timeout);
			} else {
				last_scp_expected_freq = scp_expected_freq;
				last_sap_expected_freq = sap_expected_freq;
				break;
			}

			timeout -= 1; /*try 50 times, total about 100ms*/
		} while (timeout > 0);

		/* if ipi send fail 50(=timeout) times */
		if (timeout <= 0) {
			pr_notice("IPI failed to set freq SCP(%d) SAP(%d)\n",
				scp_expected_freq, sap_expected_freq);
			if (g_dvfs_dev.has_pll_opp)
				scp_resource_req(SCP_REQ_RELEASE);
			scp_awake_unlock((void *)SCP_A_ID);
			__pm_relax(scp_dvfs_lock);
			WARN_ON(1);
			return -ESCP_DVFS_IPI_FAILED;
		}

		if (g_dvfs_dev.has_pll_opp) {
			/* turn off PLL if necessary */
			scp_pll_ctrl_set(PLL_DISABLE, scp_expected_freq);

			opp_idx = scp_get_freq_idx(scp_expected_freq);
			if (g_dvfs_dev.opp[opp_idx].resource_req)
				scp_resource_req(g_dvfs_dev.opp[opp_idx].resource_req);
			else
				scp_resource_req(SCP_REQ_RELEASE);
		}

		scp_awake_unlock((void *)SCP_A_ID);
	}

	pr_debug("[SCP] succeed to request freq, expected SCP=%d SAP=%d\n",
			scp_expected_freq, sap_expected_freq);
FINISH:
	__pm_relax(scp_dvfs_lock);
	return 0;
}

/* scp_request_freq
 * return :<0 means the scp request freq. error
 * return :0  means the request freq. finished
 */
int scp_request_freq(void)
{
	if (g_dvfs_dev.vlp_support)
		return scp_request_freq_vlp();
	else
		return scp_request_freq_vcore();
}

void wait_scp_dvfs_init_done(void)
{
	int count = 0;

	while (atomic_read(&g_scp_dvfs_init_done) != 1) {
		mdelay(1);
		count++;
		if (count > 3000) {
			pr_notice("SCP dvfs driver init fail\n");
			WARN_ON(1);
		}
	}
}

static int set_scp_clk_mux(unsigned int  pll_ctrl_flag)
{
	int ret = 0;

	if (pll_ctrl_flag == PLL_ENABLE) {
		if (!g_dvfs_dev.pre_mux_en) {
			/* should not enable twice or CCF ref_cnt will be wrong */
			ret = clk_prepare_enable(mt_scp_pll_dev.clk_mux);
			if (ret) {
				pr_notice("[%s]: clk_prepare_enable failed\n",
					__func__);
				WARN_ON(1);
				return -1;
			}
			g_dvfs_dev.pre_mux_en = true;
		}
	} else if (pll_ctrl_flag == PLL_DISABLE) {
		/* should not disable twice or CCF ref_cnt will be wrong */
		clk_disable_unprepare(mt_scp_pll_dev.clk_mux);
		g_dvfs_dev.pre_mux_en = false;
	}

	return 0;
}

static int __scp_pll_sel_26M(unsigned int pll_ctrl_flag, unsigned int pll_sel)
{
	int ret = 0;

	if (pll_sel != CLK_26M)
		return -EINVAL;

	ret = set_scp_clk_mux(pll_ctrl_flag);
	if (ret)
		return ret;

	if (pll_ctrl_flag == PLL_ENABLE) {
		ret = clk_set_parent(mt_scp_pll_dev.clk_mux, mt_scp_pll_dev.clk_pll[0]);
		if (ret) {
			pr_notice("[%s]: clk_set_parent() failed for 26M\n", __func__);
			WARN_ON(1);
		}
	}

	return ret;
}

int scp_pll_ctrl_set(unsigned int pll_ctrl_flag, unsigned int pll_sel)
{
	int idx;
	int mux_idx = 0;
	int ret = 0;

	pr_debug("[%s]: (%d, %d)\n", __func__, pll_ctrl_flag, pll_sel);

	if (pll_sel == CLK_26M)
		return __scp_pll_sel_26M(pll_ctrl_flag, pll_sel);

	idx = scp_get_freq_idx(pll_sel);
	if (idx < 0) {
		pr_notice("invalid idx %d\n", idx);
		WARN_ON(1);
		return -EINVAL;
	}

	mux_idx = g_dvfs_dev.opp[idx].clk_mux;

	if (mux_idx < 0) {
		pr_notice("invalid mux_idx %d\n", mux_idx);
		WARN_ON(1);
		return -EINVAL;
	}

	if (pll_ctrl_flag == PLL_ENABLE) {
		ret = set_scp_clk_mux(pll_ctrl_flag);
		if (ret)
			return ret;
		if (idx >= 0 && idx < g_dvfs_dev.scp_opp_nums
				&& mux_idx < mt_scp_pll_dev.pll_num)
			ret = clk_set_parent(mt_scp_pll_dev.clk_mux,
					mt_scp_pll_dev.clk_pll[mux_idx]);
		else {
			pr_notice("[%s]: not support opp freq %d\n",
				__func__, pll_sel);
			WARN_ON(1);
		}

		if (ret) {
			pr_notice("[%s]: clk_set_parent() failed, opp=%d\n",
				__func__, pll_sel);
			WARN_ON(1);
		}
	} else if ((pll_ctrl_flag == PLL_DISABLE) &&
			(g_dvfs_dev.opp[idx].resource_req == SCP_REQ_RELEASE)) {
		/* resource_req != 0 means we are possibly using main/unipll,
		 * so we should not disable mux at this time.
		 */
		set_scp_clk_mux(pll_ctrl_flag);
	}
	return ret;
}

#ifdef CONFIG_PROC_FS
/*
 * PROC
 */

/****************************
 * show SCP state
 *****************************/
static int mt_scp_dvfs_state_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "current debug scp core: %d\n",
		g_dvfs_dev.cur_dbg_core);

	return 0;
}

/****************************
 * show SCP DVFS OPP
 *****************************/
static int mt_scp_dvfs_opp_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "# freq volt   mux resc\n");
	for (i = 0; i < g_dvfs_dev.scp_opp_nums; i++) {
		seq_printf(m, "%d %4u %6u %3u %4u\n", i,
			g_dvfs_dev.opp[i].freq,
			g_dvfs_dev.opp[i].vcore,
			g_dvfs_dev.opp[i].clk_mux,
			g_dvfs_dev.opp[i].resource_req);
	}
	return 0;
}

/****************************
 * show SCP count
 *****************************/
static int mt_scp_dvfs_sleep_cnt_proc_show(struct seq_file *m, void *v)
{
	struct ipi_tx_data_t ipi_data;
	unsigned int *scp_ack_data = NULL;
	int ret;

	if (!g_dvfs_dev.sleep_init_done)
		slp_ipi_init();

	ipi_data.arg1 = SCP_SLEEP_GET_COUNT;
	if (g_dvfs_dev.cur_dbg_core == SCP_CORE_0) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
		scp_ack_data = &scp_ipi_ackdata0;
	} else if (g_dvfs_dev.cur_dbg_core == SCP_CORE_1) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
		scp_ack_data = &scp_ipi_ackdata1;
	} else {
		pr_notice("[%s]: invalid scp core num: %d\n",
			__func__, g_dvfs_dev.cur_dbg_core);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[%s] ipi send failed with error: %d\n",
			__func__, ret);
		return -ESCP_DVFS_IPI_FAILED;
	}
	seq_printf(m, "scp core%d sleep count: %d\n",
		g_dvfs_dev.cur_dbg_core, *scp_ack_data);
	pr_notice("[%s]: scp core%d sleep count :%d\n",
		__func__, g_dvfs_dev.cur_dbg_core, *scp_ack_data);


	return 0;
}

/**********************************
 * write scp dvfs sleep
 ***********************************/
static ssize_t mt_scp_dvfs_sleep_cnt_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[32];
	unsigned int len = 0;
	unsigned int ipi_cmd = -1;
	unsigned int ipi_cmd_size = -1;
	int ret = 0;
	int n = 0;
	struct ipi_tx_data_t ipi_data;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!g_dvfs_dev.sleep_init_done)
		slp_ipi_init();

	n = sscanf(desc, "%31s", cmd);
	if (n != 1) {
		pr_notice("[%s]: invalid command", __func__);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}

	if (!strcmp(cmd, "reset")) {
		ipi_data.arg1 = SCP_SLEEP_RESET;
		if (g_dvfs_dev.cur_dbg_core == SCP_CORE_0) {
			ipi_cmd = IPI_OUT_C_SLEEP_0;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_0;
		} else if (g_dvfs_dev.cur_dbg_core == SCP_CORE_1) {
			ipi_cmd = IPI_OUT_C_SLEEP_1;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_1;
		} else {
			pr_notice("[%s]: invalid core index: %d\n",
				__func__, g_dvfs_dev.cur_dbg_core);
			return -ESCP_DVFS_DBG_INVALID_CMD;
		}
		ret = mtk_ipi_send(&scp_ipidev, ipi_cmd, IPI_SEND_WAIT,
			&ipi_data, ipi_cmd_size, 500);
		if (ret != SCP_IPI_DONE) {
			pr_info("[%s]: SCP send IPI failed - %d\n",
				__func__, ret);
			return -ESCP_DVFS_IPI_FAILED;
		}
	} else {
		pr_notice("[%s]: invalid command: %s\n", __func__, cmd);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}

	return count;
}

/****************************
 * show scp dvfs sleep
 *****************************/
static int mt_scp_dvfs_sleep_proc_show(struct seq_file *m, void *v)
{
	struct ipi_tx_data_t ipi_data;
	unsigned int *scp_ack_data = NULL;
	int ret = 0;

	if (!g_dvfs_dev.sleep_init_done)
		slp_ipi_init();

	ipi_data.arg1 = SCP_SLEEP_GET_DBG_FLAG;

	if (g_dvfs_dev.cur_dbg_core == SCP_CORE_0) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
		scp_ack_data = &scp_ipi_ackdata0;
	} else if (g_dvfs_dev.cur_dbg_core == SCP_CORE_1) {
		ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
			IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
		scp_ack_data = &scp_ipi_ackdata1;
	} else {
		pr_notice("[%s]: invalid scp core index: %d\n",
			__func__, g_dvfs_dev.cur_dbg_core);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[%s] ipi send failed with error: %d\n",
			__func__, ret);
	} else {
		if (*scp_ack_data <= SCP_SLEEP_ON)
			seq_printf(m, "scp sleep flag: %d\n",
				*scp_ack_data);
		else
			seq_printf(m, "invalid sleep flag: %d\n",
				*scp_ack_data);
	}

	return 0;
}

/**********************************
 * write scp dvfs sleep
 ***********************************/
static ssize_t mt_scp_dvfs_sleep_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[32];
	unsigned int len = 0;
	unsigned int ipi_cmd = -1;
	unsigned int ipi_cmd_size = -1;
	int ret = 0;
	int n = 0;
	int dbg_core = -1, slp_cmd = -1;
	struct ipi_tx_data_t ipi_data;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!g_dvfs_dev.sleep_init_done)
		slp_ipi_init();

	n = sscanf(desc, "%31s %d", cmd, &slp_cmd);
	if (n != 2) {
		pr_notice("[%s]: invalid command", __func__);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}
	if (!strcmp(cmd, "sleep")) {
		if (slp_cmd < SCP_SLEEP_OFF
				|| slp_cmd > SCP_SLEEP_ON) {
			pr_info("[%s]: invalid slp cmd: %d\n",
				__func__, slp_cmd);
			return -ESCP_DVFS_DBG_INVALID_CMD;
		}
		ipi_data.arg1 = slp_cmd;
		if (g_dvfs_dev.cur_dbg_core == SCP_CORE_0) {
			ipi_cmd = IPI_OUT_C_SLEEP_0;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_0;
		} else if (g_dvfs_dev.cur_dbg_core == SCP_CORE_1) {
			ipi_cmd = IPI_OUT_C_SLEEP_1;
			ipi_cmd_size = PIN_OUT_C_SIZE_SLEEP_1;
		} else {
			pr_notice("[%s]: invalid debug core: %d\n",
				__func__, g_dvfs_dev.cur_dbg_core);
			return -ESCP_DVFS_DBG_INVALID_CMD;
		}
		ret = mtk_ipi_send(&scp_ipidev, ipi_cmd, IPI_SEND_WAIT,
			&ipi_data, ipi_cmd_size, 500);
		if (ret != SCP_IPI_DONE) {
			pr_info("%s: SCP send IPI fail - %d\n",
				__func__, ret);
			return -ESCP_DVFS_IPI_FAILED;
		}
	} else if (!strcmp(cmd, "dbg_core")) {
		dbg_core = (enum scp_core_enum) slp_cmd;
		/* IPI/Mbox of Core2 is not implemented. */
		if (is_core_online(dbg_core) && (dbg_core != SCP_CORE_2))
			g_dvfs_dev.cur_dbg_core = dbg_core;
	} else {
		pr_notice("[%s]: invalid command: %s\n", __func__, cmd);
		return -ESCP_DVFS_DBG_INVALID_CMD;
	}

	return count;
}

/****************************
 * show scp dvfs ctrl
 *****************************/
static int mt_scp_dvfs_ctrl_proc_show(struct seq_file *m, void *v)
{
	volatile unsigned int scp_current_freq_reg, scp_expected_freq_reg;
	int i;

	/* Keep SCP active to read */
	if (scp_awake_lock((void *)SCP_A_ID)) {
		pr_notice("[%s] scp_awake_lock failed\n", __func__);
		return 0;
	}
	scp_current_freq_reg = readl(CURRENT_FREQ_REG);
	scp_expected_freq_reg = readl(EXPECTED_FREQ_REG);
	scp_awake_unlock((void *)SCP_A_ID);

	seq_printf(m, "SCP DVFS: %s\n", g_scp_dvfs_flow_enable? "ON": "OFF");
	seq_printf(m, "SCP frequency: cur=%dMHz, expect=%dMHz, kernel=%dMHz\n",
				scp_current_freq_reg, scp_expected_freq_reg, scp_expected_freq);

	for (i = 0; i < NUM_FEATURE_ID; i++)
		seq_printf(m, "feature=%d, freq=%d, enable=%d\n",
			feature_table[i].feature, feature_table[i].freq,
			feature_table[i].enable);

	return 0;
}

/**********************************
 * write scp dvfs ctrl
 ***********************************/
static ssize_t mt_scp_dvfs_ctrl_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], cmd[32];
	unsigned int len = 0;
	int dvfs_opp;
	int n;

	if (count <= 0)
		return 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	n = sscanf(desc, "%31s %d", cmd, &dvfs_opp);
	if (n == 1 || n == 2) {
		if (!strcmp(cmd, "on")) {
			g_scp_dvfs_flow_enable = true;
			pr_info("SCP DVFS: ON\n");
		} else if (!strcmp(cmd, "off")) {
			g_scp_dvfs_flow_enable = false;
			pr_info("SCP DVFS: OFF\n");
		} else if (!strcmp(cmd, "opp")) {
			if (dvfs_opp == NO_SCP_DEBUG_OPP) {
				/* deregister dvfs debug feature */
				pr_info("remove the opp setting of command\n");
				feature_table[VCORE_TEST_FEATURE_ID].freq = 0;
				current_scp_debug_opp = dvfs_opp;
				scp_deregister_feature(
						VCORE_TEST_FEATURE_ID);
			} else if (dvfs_opp >= 0 &&
					dvfs_opp < g_dvfs_dev.scp_opp_nums) {
				/* register dvfs debug feature */
				current_scp_debug_opp = dvfs_opp;
				scp_register_feature(
						VCORE_TEST_FEATURE_ID);
			} else {
				pr_info("invalid opp value %d\n", dvfs_opp);
			}
		} else {
			pr_info("invalid command %s\n", cmd);
		}
	} else {
		pr_info("invalid length %d\n", n);
	}

	return count;
}

#define PROC_FOPS_RW(name) \
static int mt_ ## name ## _proc_open(\
					struct inode *inode, \
					struct file *file) \
{ \
	return single_open(file, \
					mt_ ## name ## _proc_show, \
					pde_data(inode)); \
} \
static const struct proc_ops \
	mt_ ## name ## _proc_fops = {\
	.proc_open		= mt_ ## name ## _proc_open, \
	.proc_read		= seq_read, \
	.proc_lseek		= seq_lseek, \
	.proc_release	= single_release, \
	.proc_write		= mt_ ## name ## _proc_write, \
}

#define PROC_FOPS_RO(name) \
static int mt_ ## name ## _proc_open(\
				struct inode *inode,\
				struct file *file)\
{\
	return single_open(file, \
						mt_ ## name ## _proc_show, \
						pde_data(inode)); \
} \
static const struct proc_ops mt_ ## name ## _proc_fops = {\
	.proc_open		= mt_ ## name ## _proc_open,\
	.proc_read		= seq_read,\
	.proc_lseek		= seq_lseek,\
	.proc_release	= single_release,\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RO(scp_dvfs_state);
PROC_FOPS_RO(scp_dvfs_opp);
PROC_FOPS_RW(scp_dvfs_sleep_cnt);
PROC_FOPS_RW(scp_dvfs_sleep);
PROC_FOPS_RW(scp_dvfs_ctrl);

static int mt_scp_dvfs_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(scp_dvfs_state),
		PROC_ENTRY(scp_dvfs_opp),
		PROC_ENTRY(scp_dvfs_sleep_cnt),
		PROC_ENTRY(scp_dvfs_sleep),
		PROC_ENTRY(scp_dvfs_ctrl)
	};

	dir = proc_mkdir("scp_dvfs", NULL);
	if (!dir) {
		pr_notice("fail to create /proc/scp_dvfs @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
						0664,
						dir,
						entries[i].fops)) {
			pr_notice("ERROR: %s: create /proc/scp_dvfs/%s failed\n",
						__func__, entries[i].name);
			ret = -ENOMEM;
		}
	}

	return ret;
}
#endif /* CONFIG_PROC_FS */

static const struct of_device_id scpdvfs_of_ids[] = {
	{.compatible = "mediatek,scp-dvfs",},
	{}
};

static void __init mt_pmic_sshub_init(void)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int max_vcore = g_dvfs_dev.opp[g_dvfs_dev.scp_opp_nums - 1].tuned_vcore + 100000;
	int max_vsram = g_dvfs_dev.opp[g_dvfs_dev.scp_opp_nums - 1].vsram + 100000;

	if (g_dvfs_dev.pmic_sshub_en) {
		/* set SCP VCORE voltage */
		if (regulator_set_voltage(reg_vcore, g_dvfs_dev.opp[0].tuned_vcore,
				max_vcore) != 0)
			pr_notice("Set wrong vcore voltage\n");

		/* set SCP VSRAM voltage */
		if (regulator_set_voltage(reg_vsram, g_dvfs_dev.opp[0].vsram,
				max_vsram) != 0)
			pr_notice("Set wrong vsram voltage\n");
	}

	if (!g_dvfs_dev.bypass_pmic_rg_access) {
		scp_reg_init(g_dvfs_dev.pmic_regmap, &g_dvfs_dev.pmic_regs->_sshub_op_mode);
		scp_reg_init(g_dvfs_dev.pmic_regmap, &g_dvfs_dev.pmic_regs->_sshub_op_en);
		scp_reg_init(g_dvfs_dev.pmic_regmap, &g_dvfs_dev.pmic_regs->_sshub_op_cfg);
	}

	if (g_dvfs_dev.pmic_sshub_en) {
		if (!g_dvfs_dev.bypass_pmic_rg_access) {
			/* BUCK_VCORE_SSHUB_EN: ON */
			/* LDO_VSRAM_OTHERS_SSHUB_EN: ON */
			/* PMRC mode: OFF */
			scp_reg_init(g_dvfs_dev.pmic_regmap,
				&g_dvfs_dev.pmic_regs->_sshub_buck_en);
			scp_reg_init(g_dvfs_dev.pmic_regmap,
				&g_dvfs_dev.pmic_regs->_sshub_ldo_en);
			scp_reg_init(g_dvfs_dev.pmic_regmap,
				&g_dvfs_dev.pmic_regs->_pmrc_en);
		}

		if (regulator_enable(reg_vcore) != 0)
			pr_notice("Enable vcore failed!!!\n");
		if (regulator_enable(reg_vsram) != 0)
			pr_notice("Enable vsram failed!!!\n");
	}
#endif
}

static_assert(CAL_BITS + CAL_EXT_BITS <= 8 * sizeof(unsigned short),
"error: there are only 16bits available in IPI\n");
bool sync_ulposc_cali_data_to_scp(void)
{
	unsigned int ipi_data[2];
	unsigned short *p = (unsigned short *)&ipi_data[1];
	int i, ret;
	bool cali_ok = true;

	if (!g_dvfs_dev.ulposc_hw.do_ulposc_cali) {
		pr_notice("[%s]: ulposc2 calibration is not done by AP\n",
			__func__);
		/* u2 is usable, return true */
		return true;
	}

	if (g_dvfs_dev.ulposc_hw.cali_failed) {
		pr_notice("[%s]: ulposc2 calibration failed\n", __func__);
		return false;
	}

	if (!g_dvfs_dev.sleep_init_done)
		slp_ipi_init();

	ipi_data[0] = SCP_SYNC_ULPOSC_CALI;
	for (i = 0; i < g_dvfs_dev.ulposc_hw.cali_nums; i++) {
		*p = g_dvfs_dev.ulposc_hw.cali_freq[i];
		if ((!g_dvfs_dev.vlpck_support) || g_dvfs_dev.vlpck_bypass_phase1)
			*(p + 1) = g_dvfs_dev.ulposc_hw.cali_val[i];
		else
			*(p + 1) = g_dvfs_dev.ulposc_hw.cali_val[i] |
				(g_dvfs_dev.ulposc_hw.cali_val_ext[i] << CAL_BITS);

		pr_notice("[%s]: ipi to scp: freq=%d, cali_val=0x%x\n",
			__func__,
			g_dvfs_dev.ulposc_hw.cali_freq[i],
			*(p + 1));

		ret = mtk_ipi_send_compl(&scp_ipidev,
					IPI_OUT_C_SLEEP_0,
					IPI_SEND_WAIT,
					&ipi_data[0],
					PIN_OUT_C_SIZE_SLEEP_0,
					500);
		if (ret != IPI_ACTION_DONE) {
			pr_notice("[%s]: ipi send ulposc cali val(%d, 0x%x) fail\n",
				__func__,
				g_dvfs_dev.ulposc_hw.cali_freq[i],
				*(p + 1));
			WARN_ON(1);
			cali_ok = false;
		}
	}

	/*
	 * After syncing, scp will be changed to default freq, which is not set by kernel.
	 * Reset last_scp_expected_freq to prevent not updating freq in scp reset flow.
	 */
	last_scp_expected_freq = 0;
	last_sap_expected_freq = 0;

	return cali_ok;
}

static inline bool __init is_ulposc_cali_pass(unsigned int cur,
		unsigned int target)
{
	if (cur > (target * (1000 - CALI_MIS_RATE) / 1000)
			&& cur < (target * (1000 + CALI_MIS_RATE) / 1000))
		return 1;

	/* calibrated failed here */
	pr_notice("[%s]: cur: %dMHz, target: %dMHz calibrate failed\n",
		__func__,
		FM_CNT2FREQ(cur),
		FM_CNT2FREQ(target));
	return 0;
}

static unsigned int __init _get_freq_by_fmeter(void)
{
	unsigned int result = 0;
	unsigned int i = 0;
	unsigned int wait_for_measure = 0;
	int is_fmeter_timeout = 0;

	/* enable fmeter */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_en,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_en.init_config);

	/* trigger fmeter to start measure */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_trigger_cal,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_trigger_cal.init_config);

	/* wait for frequency measurement done */
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_trigger_cal, &wait_for_measure);
	while (wait_for_measure) {
		i++;
		udelay(10);
		if (i > 40000) {
			is_fmeter_timeout = 1;
			break;
		}
		scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
			&g_dvfs_dev.ulposc_hw.clkdbg_regs->_trigger_cal,
			&wait_for_measure);
	}

	/* disable fmeter */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_en,
		!g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_en.init_config);

	/* get fmeter result */
	if (!is_fmeter_timeout) {
		scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
			&g_dvfs_dev.ulposc_hw.clkdbg_regs->_cal_cnt,
			&result);
	} else {
		result = 0;
	}

	return result;
}

static unsigned int __init _get_ulposc_clk_by_fmeter_wrapper(void)
{
	unsigned int result_freq;

	result_freq = mt_get_fmeter_freq(g_dvfs_dev.ccf_fmeter_id, g_dvfs_dev.ccf_fmeter_type);
	if (result_freq == 0) {
		/* result_freq is not expected to be 0 */
		pr_notice("[%s]: mt_get_fmeter_freq() return %d, pls check CCF configs\n",
			__func__, result_freq);
		WARN_ON(1);
	}
	return FM_FREQ2CNT(result_freq) / 1000;
}

static unsigned int __init get_ulposc_clk_by_fmeter_topck_abist(void)
{
	unsigned int result = 0;
	unsigned int clk26cali_0_bk = 0, clk26cali_1_bk = 0;

	if (g_dvfs_dev.ccf_fmeter_support)
		return _get_ulposc_clk_by_fmeter_wrapper();

	/* 0. backup regsiters */
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_0, &clk26cali_0_bk);
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_1, &clk26cali_1_bk);

	/* 1. set load cnt */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_load_cnt,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_load_cnt.init_config);

	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk_misc_cfg0,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk_misc_cfg0.init_config);

	/*
	 * 2. select clock source
	 * ULPOSC_1 (63), ULPOSC_2 (62)
	 */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_ck_sel,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_ck_sel.init_config);

	/* 3. trigger fmeter and get fmeter result */
	result = _get_freq_by_fmeter();

	/* 0. restore freq meter registers */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_0, clk26cali_0_bk);
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_1, clk26cali_1_bk);

	return result;
}

static unsigned int __init get_ulposc_clk_by_fmeter_vlp(void)
{
	unsigned int result = 0;
	unsigned int vlp_fqmtr_con0_bk = 0, vlp_fqmtr_con1_bk = 0;

	if (g_dvfs_dev.ccf_fmeter_support)
		return _get_ulposc_clk_by_fmeter_wrapper();

	/* 0. backup regsiters */
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_0, &vlp_fqmtr_con0_bk);
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_1, &vlp_fqmtr_con1_bk);

	/* 1. set load cnt */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_load_cnt,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_load_cnt.init_config);

	/*
	* 2. select clock source VLP_FQMTR_CON0[20:16] =
	* ULPOSC_1 (0x16), ULPOSC_2 (0x17)
	*/
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_ck_sel,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_ck_sel.init_config);

	/*
	* Make sure @_fmeter_rst which is used to reset fmeter keeps 1.
	*     If it accidentally set to 0, fmeter would be reset. Maybe
	* someone would have initialized it to 1 previously, so you do
	* not need to set it by yourself.
	*/
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_rst,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_rst.init_config);

	/* 3. trigger fmeter and get fmeter result */
	result = _get_freq_by_fmeter();

	/* 0. restore freq meter registers */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_0, vlp_fqmtr_con0_bk);
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_1, vlp_fqmtr_con1_bk);

	return result;
}

static unsigned int __init get_vlp_ulposc_clk_by_fmeter(void)
{
	if (g_dvfs_dev.not_support_vlp_fmeter)
		return get_ulposc_clk_by_fmeter_topck_abist();
	return get_ulposc_clk_by_fmeter_vlp();
}

static unsigned int __init get_ulposc_clk_by_fmeter(void)
{
	unsigned int result = 0;
	unsigned int misc_org = 0, dbg_org = 0, cali0_org = 0, cali1_org = 0;

	if (g_dvfs_dev.ccf_fmeter_support)
		return _get_ulposc_clk_by_fmeter_wrapper();

	/* backup regsiters */
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk_misc_cfg0, &misc_org);
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk_dbg_cfg, &dbg_org);
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_0, &cali0_org);
	scp_reg_read(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_1, &cali1_org);

	/* set load cnt */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_load_cnt,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_load_cnt.init_config);

	/* select meter clock input CLK_DBG_CFG[1:0] = 0x0 */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_abist_clk,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_abist_clk.init_config);

	/* select clock source CLK_DBG_CFG[21:16] */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_ck_sel,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_fmeter_ck_sel.init_config);

	/* select meter div CLK_MISC_CFG_0[31:24] = 0x3 for div 1 */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_meter_div,
		g_dvfs_dev.ulposc_hw.clkdbg_regs->_meter_div.init_config);

	/* trigger fmeter and get fmeter result */
	result = _get_freq_by_fmeter();

	/* restore freq meter registers */
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk_misc_cfg0, misc_org);
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk_dbg_cfg, dbg_org);
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_0, cali0_org);
	scp_reg_update(g_dvfs_dev.ulposc_hw.fmeter_regmap,
		&g_dvfs_dev.ulposc_hw.clkdbg_regs->_clk26cali_1, cali1_org);

	return result;
}

static void __init set_ulposc_cali_value_ext(unsigned int cali_val)
{
	int ret = 0;

	ret = scp_reg_update(g_dvfs_dev.ulposc_hw.ulposc_regmap,
		&g_dvfs_dev.ulposc_hw.ulposc_regs->_cali_ext,
		cali_val);

	udelay(50);
}

static void __init set_ulposc_cali_value(unsigned int cali_val)
{
	int ret = 0;

	ret = scp_reg_update(g_dvfs_dev.ulposc_hw.ulposc_regmap,
		&g_dvfs_dev.ulposc_hw.ulposc_regs->_cali,
		cali_val);

	udelay(50);
}

/*
*	Since available frequencies are expanded when SCP using VLP_CKSYS,
*	we use 2 phases calibration to widen the searching range.
*		1. g_dvfs_dev.ulposc_hw.ulposc_regs->_cali_ext
*		2. g_dvfs_dev.ulposc_hw.ulposc_regs->_cali
*/
static int __init ulposc_cali_process_vlp(unsigned int cali_idx,
		unsigned short *cali_res1, unsigned short *cali_res2)
{
	unsigned int target_val = 0, current_val = 0;
	unsigned int min = CAL_MIN_VAL, max = CAL_MAX_VAL, mid;
	unsigned int diff_by_min = 0, diff_by_max = 0xffff;
	target_val = FM_FREQ2CNT(g_dvfs_dev.ulposc_hw.cali_freq[cali_idx]);

	/* phase1 */
	if (!g_dvfs_dev.vlpck_bypass_phase1) {
		/* fixed in phase1 */
		set_ulposc_cali_value(g_dvfs_dev.ulposc_hw.ulposc_regs->_cali.init_config);
		min = CAL_MIN_VAL_EXT;
		max = CAL_MAX_VAL_EXT;
		do {
			mid = (min + max) / 2;
			if (mid == min) {
				pr_debug("turning_factor1 mid(%u) == min(%u)\n", mid, min);
				break;
			}

			set_ulposc_cali_value_ext(mid);
			current_val = get_vlp_ulposc_clk_by_fmeter();

			if (current_val > target_val)
				max = mid;
			else
				min = mid;
		} while (min <= max);

		set_ulposc_cali_value_ext(min);
		current_val = get_vlp_ulposc_clk_by_fmeter();
		diff_by_min = (current_val > target_val) ?
			(current_val - target_val):(target_val - current_val);

		set_ulposc_cali_value_ext(max);
		current_val = get_vlp_ulposc_clk_by_fmeter();
		diff_by_max = (current_val > target_val) ?
			(current_val - target_val):(target_val - current_val);

		*cali_res1 = (diff_by_min < diff_by_max) ? min : max;
		set_ulposc_cali_value_ext(*cali_res1);
	}

	/* phase2 */
	min = CAL_MIN_VAL;
	max = CAL_MAX_VAL;
	do {
		mid = (min + max) / 2;
		if (mid == min) {
			pr_debug("turning_factor2 mid(%u) == min(%u)\n", mid, min);
			break;
		}

		set_ulposc_cali_value(mid);
		current_val = get_vlp_ulposc_clk_by_fmeter();

		if (current_val > target_val)
			max = mid;
		else
			min = mid;
	} while (min <= max);

	set_ulposc_cali_value(min);
	current_val = get_vlp_ulposc_clk_by_fmeter();
	diff_by_min = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	set_ulposc_cali_value(max);
	current_val = get_vlp_ulposc_clk_by_fmeter();
	diff_by_max = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	*cali_res2 = (diff_by_min < diff_by_max) ? min : max;
	set_ulposc_cali_value(*cali_res2);

	/* Checking final calibration result */
	current_val = get_vlp_ulposc_clk_by_fmeter();
	if (!is_ulposc_cali_pass(current_val, target_val)) {
		pr_notice("[%s]: calibration failed for: %dMHz\n",
			__func__,
			g_dvfs_dev.ulposc_hw.cali_freq[cali_idx]);
		*cali_res1 = 0;
		*cali_res2 = 0;
		WARN_ON(1);
		return -ESCP_DVFS_CALI_FAILED;
	}

	pr_notice("[%s]: target: %uMhz, calibrated = %uMHz\n",
		__func__,
		FM_CNT2FREQ(target_val),
		FM_CNT2FREQ(current_val));

	return 0;
}

static int __init ulposc_cali_process(unsigned int cali_idx,
		unsigned short *cali_res)
{
	unsigned int target_val = 0, current_val = 0;
	unsigned int min = CAL_MIN_VAL, max = CAL_MAX_VAL, mid;
	unsigned int diff_by_min = 0, diff_by_max = 0xffff;

	target_val = FM_FREQ2CNT(g_dvfs_dev.ulposc_hw.cali_freq[cali_idx]);

	do {
		mid = (min + max) / 2;
		if (mid == min) {
			pr_debug("mid(%u) == min(%u)\n", mid, min);
			break;
		}

		set_ulposc_cali_value(mid);
		current_val = get_ulposc_clk_by_fmeter();

		if (current_val > target_val)
			max = mid;
		else
			min = mid;
	} while (min <= max);

	set_ulposc_cali_value(min);
	current_val = get_ulposc_clk_by_fmeter();
	diff_by_min = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	set_ulposc_cali_value(max);
	current_val = get_ulposc_clk_by_fmeter();
	diff_by_max = (current_val > target_val) ?
		(current_val - target_val):(target_val - current_val);

	*cali_res = (diff_by_min < diff_by_max) ? min : max;

	set_ulposc_cali_value(*cali_res);
	current_val = get_ulposc_clk_by_fmeter();
	if (!is_ulposc_cali_pass(current_val, target_val)) {
		pr_notice("[%s]: calibration failed for: %dMHz\n",
			__func__,
			g_dvfs_dev.ulposc_hw.cali_freq[cali_idx]);
		*cali_res = 0;
		WARN_ON(1);
		return -ESCP_DVFS_CALI_FAILED;
	}

	pr_notice("[%s]: target: %uMhz, calibrated = %uMHz\n",
		__func__,
		FM_CNT2FREQ(target_val),
		FM_CNT2FREQ(current_val));

	return 0;
}

static int smc_ulposc2_cali_done(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, ULPOSC2_CALI_DONE,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static int smc_turn_on_ulposc2(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, ULPOSC2_TURN_ON,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static int smc_turn_off_ulposc2(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_SCP_DVFS_CONTROL, ULPOSC2_TURN_OFF,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static void turn_onoff_ulposc2(enum ulposc_onoff_enum on)
{
	if (on) {
		/* turn on ulposc */
		if (g_dvfs_dev.secure_access_scp) {
			smc_turn_on_ulposc2();
		} else {
			/* LEGACY - For projects that do not support SMC ULPOSC2_TURN_ON.
			 * New projects should use the smc_turn_on_ulposc2() instead.
			 */
			scp_reg_update(g_dvfs_dev.clk_hw->scp_clk_regmap,
				&g_dvfs_dev.clk_hw->_clk_high_en, on);
			scp_reg_update(g_dvfs_dev.clk_hw->scp_clk_regmap,
				&g_dvfs_dev.clk_hw->_ulposc2_en, !on);

			/* wait settle time */
			udelay(150);

			scp_reg_update(g_dvfs_dev.clk_hw->scp_clk_regmap,
				&g_dvfs_dev.clk_hw->_ulposc2_cg, on);
		}
	} else {
		/* turn off ulposc */
		if (g_dvfs_dev.secure_access_scp) {
			smc_turn_off_ulposc2();
		} else {
			/* LEGACY - For projects that do not support SMC ULPOSC2_TURN_OFF.
			 * New projects should use the smc_turn_off_ulposc2() instead.
			 */
			scp_reg_update(g_dvfs_dev.clk_hw->scp_clk_regmap,
				&g_dvfs_dev.clk_hw->_ulposc2_cg, on);
			udelay(50);
			scp_reg_update(g_dvfs_dev.clk_hw->scp_clk_regmap,
				&g_dvfs_dev.clk_hw->_ulposc2_en, !on);
		}
	}
	udelay(50);
}

static int __init mt_scp_dvfs_do_ulposc_cali_process(void)
{
	int ret = 0;
	unsigned int i;

	if (!g_dvfs_dev.ulposc_hw.do_ulposc_cali) {
		pr_notice("[%s]: ulposc2 calibration is not done by AP\n",
			__func__);
		return 0;
	}

	for (i = 0; i < g_dvfs_dev.ulposc_hw.cali_nums; i++) {
		turn_onoff_ulposc2(ULPOSC_OFF);

		ret += scp_reg_update(g_dvfs_dev.ulposc_hw.ulposc_regmap,
			&g_dvfs_dev.ulposc_hw.ulposc_regs->_con0,
			g_dvfs_dev.ulposc_hw.cali_configs[i].con0_val);
		ret += scp_reg_update(g_dvfs_dev.ulposc_hw.ulposc_regmap,
			&g_dvfs_dev.ulposc_hw.ulposc_regs->_con1,
			g_dvfs_dev.ulposc_hw.cali_configs[i].con1_val);
		ret += scp_reg_update(g_dvfs_dev.ulposc_hw.ulposc_regmap,
			&g_dvfs_dev.ulposc_hw.ulposc_regs->_con2,
			g_dvfs_dev.ulposc_hw.cali_configs[i].con2_val);
		if (ret) {
			pr_notice("[%s]: config ulposc register failed\n",
				__func__);
			return ret;
		}

		turn_onoff_ulposc2(ULPOSC_ON);

		if (g_dvfs_dev.vlpck_support)
			ret = ulposc_cali_process_vlp(i, &g_dvfs_dev.ulposc_hw.cali_val_ext[i], &g_dvfs_dev.ulposc_hw.cali_val[i]);
		else
			ret = ulposc_cali_process(i, &g_dvfs_dev.ulposc_hw.cali_val[i]);
		if (ret) {
			pr_notice("[%s]: cali %uMHz ulposc failed\n",
				__func__, g_dvfs_dev.ulposc_hw.cali_freq[i]);
			g_dvfs_dev.ulposc_hw.cali_failed = true;
			return -ESCP_DVFS_CALI_FAILED;
		}
	}

	/* Turn off ULPOSC2 & mark as calibration done */
	turn_onoff_ulposc2(ULPOSC_OFF);
	smc_ulposc2_cali_done();

	pr_notice("[%s]: ulposc calibration all done\n", __func__);

	return ret;
}

static int __init mt_scp_dts_get_cali_hw_setting(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	unsigned int i;
	int ret = 0;

	/* find hw calibration configuration data */
	/* update clk_dbg or ulposc_cali data if there is minor change by hw */
	ret = of_property_count_u32_elems(node, PROPNAME_U2_CALI_CONFIG);
	if ((ret / CALI_CONFIG_ELEM_CNT) <= 0
		|| (ret % CALI_CONFIG_ELEM_CNT) != 0) {
		pr_notice("[%s]: cali config count does not equal to cali nums\n",
			__func__);
		return ret;
	}

	cali_hw->cali_configs = kcalloc(cali_hw->cali_nums,
				sizeof(struct ulposc_cali_config),
				GFP_KERNEL);
	if (!(cali_hw->cali_configs))
		return -ENOMEM;

	for (i = 0; i < cali_hw->cali_nums; i++) {
		ret = of_property_read_u32_index(node, PROPNAME_U2_CALI_CONFIG,
			(i * CALI_CONFIG_ELEM_CNT),
			&(cali_hw->cali_configs[i].con0_val));
		if (ret) {
			pr_notice("[%s]: get con0 setting failed\n", __func__);
			goto CALI_DATA_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_U2_CALI_CONFIG,
			(i * CALI_CONFIG_ELEM_CNT + 1),
			&(cali_hw->cali_configs[i].con1_val));
		if (ret) {
			pr_notice("[%s]: get con1 setting failed\n", __func__);
			goto CALI_DATA_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_U2_CALI_CONFIG,
			(i * CALI_CONFIG_ELEM_CNT + 2),
			&(cali_hw->cali_configs[i].con2_val));
		if (ret) {
			pr_notice("[%s]: get con2 setting failed\n", __func__);
			goto CALI_DATA_INIT_FAILED;
		}
	}

	return 0;
CALI_DATA_INIT_FAILED:
	kfree(cali_hw->cali_configs);
	return ret;
}

static int __init mt_scp_dts_get_cali_target(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	int ret = 0;
	unsigned int i;
	unsigned int tmp = 0;

	/* find number of ulposc need to do calibration */
	ret = of_property_read_u32(node, PROPNAME_U2_CALI_NUM,
		&cali_hw->cali_nums);
	if (ret) {
		pr_notice("[%s]: find ulposc calibration numbers failed\n",
			__func__);
		return ret;
	}

	ret = of_property_count_u32_elems(node, PROPNAME_U2_CALI_TARGET);
	if (ret != cali_hw->cali_nums) {
		pr_notice("[%s]: target nums does not equals to ulposc-cali-num\n",
			__func__);
		return ret;
	}

	if (g_dvfs_dev.vlpck_support) {
		cali_hw->cali_val_ext = kcalloc(cali_hw->cali_nums, sizeof(unsigned short),
					GFP_KERNEL);
		if (!cali_hw->cali_val_ext)
			return -ENOMEM;
	} else {
		cali_hw->cali_val_ext = NULL;
	}

	cali_hw->cali_val = kcalloc(cali_hw->cali_nums, sizeof(unsigned short),
				GFP_KERNEL);
	if (!cali_hw->cali_val) {
		ret = -ENOMEM;
		goto CALI_EXT_TARGET_ALLOC_FAILED;
	}

	cali_hw->cali_freq = kcalloc(cali_hw->cali_nums, sizeof(unsigned short),
				GFP_KERNEL);
	if (!cali_hw->cali_freq) {
		ret = -ENOMEM;
		goto CALI_TARGET_ALLOC_FAILED;
	}

	for (i = 0; i < cali_hw->cali_nums; i++) {
		ret = of_property_read_u32_index(node, PROPNAME_U2_CALI_TARGET,
			i, &tmp);
		if (ret) {
			pr_notice("[%s]: find cali target failed, idx: %d\n",
				__func__, i);
			goto FIND_TARGET_FAILED;
		}
		cali_hw->cali_freq[i] = (unsigned short) tmp;
	}

	return ret;

FIND_TARGET_FAILED:
	kfree(cali_hw->cali_freq);
CALI_TARGET_ALLOC_FAILED:
	kfree(cali_hw->cali_val);
CALI_EXT_TARGET_ALLOC_FAILED:
	kfree(cali_hw->cali_val_ext);
	return ret;
}

static int __init mt_scp_dts_get_cali_hw_regs(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	const char *str = NULL;
	int ret = 0;
	int i;

	/* find ulposc register hw version */
	ret = of_property_read_string(node, PROPNAME_U2_CALI_VER, &str);
	if (ret) {
		pr_notice("[%s]: find ulposc-cali-ver failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0; i < MAX_ULPOSC_VERSION; i++)
		if (!strcmp(ulposc_ver[i], str))
			cali_hw->ulposc_regs = &cali_regs[i];

	if (!cali_hw->ulposc_regs) {
		pr_notice("[%s]: no ulposc cali reg found\n", __func__);
		return -ESCP_DVFS_NO_CALI_HW_FOUND;
	}

	/* find clk dbg register hw version */
	if (!g_dvfs_dev.ccf_fmeter_support) {
		ret = of_property_read_string(node, PROPNAME_SCP_CLK_DBG_VER, &str);
		if (ret) {
			pr_notice("[%s]: find clk-dbg-ver failed with err: %d\n",
				__func__, ret);
			return ret;
		}

		for (i = 0; i < MAX_CLK_DBG_VERSION; i++)
			if (!strcmp(clk_dbg_ver[i], str))
				cali_hw->clkdbg_regs = &clk_dbg_reg[i];
		if (!cali_hw->clkdbg_regs) {
			pr_notice("[%s]: no clkfbg regs found\n",
				__func__);
			return -ESCP_DVFS_NO_CALI_HW_FOUND;
		}
	}

	return ret;
}

static void mt_scp_start_res_prof(void)
{
	struct ipi_tx_data_t ipi_data;
	int ret = 0;

	ipi_data.arg1 = SCP_SLEEP_START_RES_PROF;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		return;
	}

	if (!is_core_online(SCP_CORE_1))
		return;

	/* if there are core0 & core1 */
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		return;
	}
}

static void mt_scp_stop_res_prof(void)
{
	struct wlock_duration_t wlock;
	struct res_duration_t res;
	struct ipi_tx_data_t ipi_data;
	struct slp_ack_t *addr[2];
	uint64_t suspend_time;
	size_t size = 0;
	int ret = 0;

	addr[SCP_CORE_0] = (struct slp_ack_t *)scp_get_reserve_mem_virt(SCP_LOW_PWR_DBG_MEM_ID);
	if (!addr[SCP_CORE_0])
		return;
	size = scp_get_reserve_mem_size(SCP_LOW_PWR_DBG_MEM_ID);
	memset(addr[SCP_CORE_0], 0, size);
	addr[SCP_CORE_1] = addr[SCP_CORE_0] + 0x100;

	ipi_data.arg1 = SCP_SLEEP_STOP_RES_PROF;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		return;
	}

	wlock = addr[SCP_CORE_0]->wlock;
	res = addr[SCP_CORE_0]->res;
	suspend_time = addr[SCP_CORE_0]->suspend_time;

	if (suspend_time) {
		pr_notice("[SCP] [%s:%d] [%d] susepnd time: %llums\n",
					__func__, __LINE__, SCP_CORE_0, suspend_time);
	}

	if (wlock.total_duration) {
		wlock.pcLockName[configMAX_LOCK_NAME_LEN - 1] = '\0';
		pr_notice("[SCP] [%s:%d] [%d] [wakelock] total: %llums, lock %s: %llums\n",
				__func__, __LINE__, SCP_CORE_0, wlock.total_duration,
				wlock.pcLockName, wlock.duration);
	}

	if (res.total_duration) {
		pr_notice("[SCP] [%s:%d] [%d] [26M] total: %llums, user %d: %llums\n",
				__func__, __LINE__, SCP_CORE_0, res.total_duration,
				res.user, res.duration);
	}

	if (!is_core_online(SCP_CORE_1))
		return;

	/* if there are core0 & core1 */
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		return;
	}

	wlock = addr[SCP_CORE_1]->wlock;
	res = addr[SCP_CORE_1]->res;
	suspend_time = addr[SCP_CORE_1]->suspend_time;

	if (suspend_time) {
		pr_notice("[SCP] [%s:%d] [%d] susepnd time: %llums\n",
					__func__, __LINE__, SCP_CORE_1, suspend_time);
	}

	if (wlock.total_duration) {
		wlock.pcLockName[configMAX_LOCK_NAME_LEN - 1] = '\0';
		pr_notice("[SCP] [%s:%d] [%d] [wakelock] total: %llums, lock %s: %llums\n",
			    __func__, __LINE__, SCP_CORE_1, wlock.total_duration,
				wlock.pcLockName, wlock.duration);
	}

	if (res.total_duration) {
		pr_notice("[SCP] [%s:%d] [%d] [26M] total: %llums, user %d: %llums\n",
				__func__, __LINE__, SCP_CORE_1, res.total_duration,
				res.user, res.duration);
	}
}

#if IS_ENABLED(CONFIG_PM)
static int mt_scp_dump_sleep_count(void)
{
	int ret = 0;
	struct ipi_tx_data_t ipi_data;

	if (!g_dvfs_dev.sleep_init_done)
		slp_ipi_init();

	ipi_data.arg1 = SCP_SLEEP_GET_COUNT;
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_0,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		goto FINISH;
	}

	/* if no enable core1 */
	if (!is_core_online(SCP_CORE_1)) {
		pr_notice("[SCP] [%s:%d] - scp_sleep_cnt_0 = %d\n",
			__func__, __LINE__, scp_ipi_ackdata0);
		goto FINISH;
	}

	/* if there are core0 & core1 */
	ret = mtk_ipi_send_compl(&scp_ipidev, IPI_OUT_C_SLEEP_1,
		IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_1, 500);
	if (ret != IPI_ACTION_DONE) {
		pr_notice("[SCP] [%s:%d] - scp ipi failed, ret = %d\n",
			__func__, __LINE__, ret);
		goto FINISH;
	}

	pr_notice("[SCP] [%s:%d] - scp_sleep_cnt_0 = %d, scp_sleep_cnt_1 = %d\n",
		__func__, __LINE__, scp_ipi_ackdata0, scp_ipi_ackdata1);

FINISH:
	return 0;
}

static int scp_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		mt_scp_dump_sleep_count();
		if (scpreg.low_pwr_dbg)
			mt_scp_start_res_prof();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		mt_scp_dump_sleep_count();
		if (scpreg.low_pwr_dbg)
			mt_scp_stop_res_prof();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block scp_pm_notifier_func = {
	.notifier_call = scp_pm_event,
};
#endif /* IS_ENABLED(CONFIG_PM) */

static int __init mt_scp_dts_init_scp_clk_hw(struct device_node *node)
{
	const char *str = NULL;
	unsigned int i;
	int ret = 0;

	ret = of_property_read_string(node, PROPNAME_SCP_CLK_HW_VER, &str);
	if (ret) {
		pr_notice("[%s]: find scp-clk-hw-ver failed with err: %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0; i < MAX_SCP_CLK_VERSION; i++) {
		if (!strcmp(scp_clk_ver[i], str)) {
			g_dvfs_dev.clk_hw = &(scp_clk_hw_regs[i]);
			return 0;
		}
	}

	pr_notice("[%s]: no scp clk hw reg found\n", __func__);

	return -ESCP_DVFS_NO_CALI_HW_FOUND;
}

static int __init mt_scp_dts_init_cali_regmap(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	/* init regmap for calibration process */
	if (!g_dvfs_dev.ccf_fmeter_support) {
		cali_hw->fmeter_regmap = syscon_regmap_lookup_by_phandle(node,
								PROPNAME_FM_CLK);
		if (IS_ERR(cali_hw->fmeter_regmap)) {
			pr_notice("fmeter regmap init failed: %ld\n",
				PTR_ERR(cali_hw->fmeter_regmap));
			return PTR_ERR(cali_hw->fmeter_regmap);
		}
	}

	cali_hw->ulposc_regmap = syscon_regmap_lookup_by_phandle(node,
							PROPNAME_ULPOSC_CLK);
	if (IS_ERR(cali_hw->ulposc_regmap)) {
		pr_notice("ulposc regmap init failed: %ld\n",
			PTR_ERR(cali_hw->ulposc_regmap));
		return PTR_ERR(cali_hw->ulposc_regmap);
	}

	return 0;
}

static int __init mt_scp_dts_ulposc_cali_init(struct device_node *node,
		struct ulposc_cali_hw *cali_hw)
{
	int ret = 0;

	if (!cali_hw) {
		pr_notice("[%s]: ulposc calibration hw is NULL\n",
			__func__);
		WARN_ON(1);
		return -ESCP_DVFS_NO_CALI_HW_FOUND;
	}

	/* check if current platform need to do calibration */
	g_dvfs_dev.ulposc_hw.do_ulposc_cali = of_property_read_bool(node,
							PROPNAME_DO_U2_CALI);
	if (!g_dvfs_dev.ulposc_hw.do_ulposc_cali) {
		pr_notice("[%s]: skip ulposc calibration process\n", __func__);
		return 0;
	}

	ret = mt_scp_dts_init_cali_regmap(node, cali_hw);
	if (ret)
		return ret;

	ret = mt_scp_dts_init_scp_clk_hw(node);
	if (ret)
		return ret;

	ret = mt_scp_dts_get_cali_hw_regs(node, cali_hw);
	if (ret)
		return ret;

	g_dvfs_dev.clk_hw->scp_clk_regmap = syscon_regmap_lookup_by_phandle(node,
						PROPNAME_SCP_CLK_CTRL);
	if (!g_dvfs_dev.clk_hw->scp_clk_regmap) {
		pr_notice("[%s]: get scp clk regmap failed\n", __func__);
		return ret;
	}

	ret = mt_scp_dts_get_cali_target(node, cali_hw);
	if (ret)
		return ret;

	ret = mt_scp_dts_get_cali_hw_setting(node, cali_hw);
	if (ret)
		return ret;

	return ret;
}

static int __init mt_scp_dts_clk_init(struct platform_device *pdev)
{
	char buf[15];
	int ret = 0;
	int i;

	mt_scp_pll_dev.clk_mux = devm_clk_get(&pdev->dev, SCP_CLOCK_NAMES_SCP_SEL);
	if (IS_ERR(mt_scp_pll_dev.clk_mux)) {
		dev_notice(&pdev->dev, "cannot get clock mux\n");
		WARN_ON(1);
		return PTR_ERR(mt_scp_pll_dev.clk_mux);
	}

	/* scp_sel has most 9 member of clk source */
	mt_scp_pll_dev.pll_num = MAX_SUPPORTED_PLL_NUM;
	for (i = 0; i < MAX_SUPPORTED_PLL_NUM; i++) {
		ret = snprintf(buf, 15, SCP_CLOCK_NAMES_SCP_CLKS, i);
		if (ret < 0 || ret >= 15) {
			pr_notice("[%s]: clk name buf len: %d\n",
				__func__, ret);
			return ret;
		}

		mt_scp_pll_dev.clk_pll[i] = devm_clk_get(&pdev->dev, buf);
		if (IS_ERR(mt_scp_pll_dev.clk_pll[i])) {
			dev_notice(&pdev->dev,
					"cannot get %dst clock parent\n",
					i);
			mt_scp_pll_dev.pll_num = i;
			break;
		}
	}

	return 0;
}

static int __init mt_scp_dts_init_dvfs_data(struct device_node *node,
		struct dvfs_opp **opp)
{
	int ret = 0;
	int i;

	if (*opp) {
		pr_notice("[%s]: opp is initialized\n", __func__);
		return -ESCP_DVFS_DATA_RE_INIT;
	}

	/* get scp dvfs opp count */
	ret = of_property_count_u32_elems(node, PROPNAME_SCP_DVFS_OPP);
	if ((ret / OPP_ELEM_CNT) <= 0 || (ret % OPP_ELEM_CNT) != 0) {
		pr_notice("[%s]: get dvfs opp count failed, count: %d\n",
			__func__, ret);
		return ret;
	}
	g_dvfs_dev.scp_opp_nums = ret / 7;

	*opp = kcalloc(g_dvfs_dev.scp_opp_nums, sizeof(struct dvfs_opp), GFP_KERNEL);
	if (!(*opp))
		return -ENOMEM;

	/* get each dvfs opp data from dts node */
	for (i = 0; i < g_dvfs_dev.scp_opp_nums; i++) {
		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				i * OPP_ELEM_CNT,
				&(*opp)[i].vcore);
		if (ret) {
			pr_notice("Cannot get property vcore(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				(i * OPP_ELEM_CNT) + 1,
				&(*opp)[i].vsram);
		if (ret) {
			pr_notice("Cannot get property vsram(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				(i * OPP_ELEM_CNT) + 2,
				&(*opp)[i].dvfsrc_opp);
		if (ret) {
			pr_notice("Cannot get property dvfsrc opp(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}
		if ((!g_dvfs_dev.vlp_support) && ((*opp)[i].dvfsrc_opp != NO_DVFSRC_OPP)) {
			ret = scp_get_vcore_table((*opp)[i].dvfsrc_opp);
			if (ret > 0) {
				(*opp)[i].tuned_vcore = ret;
			} else {
				/* As default value, if atf is not available */
				(*opp)[i].tuned_vcore = (*opp)[i].vcore;
			}
		} else {
			(*opp)[i].tuned_vcore = (*opp)[i].vcore;
		}

		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				(i * OPP_ELEM_CNT) + 3,
				&(*opp)[i].spm_opp);
		if (ret) {
			pr_notice("Cannot get property spm opp(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				(i * OPP_ELEM_CNT) + 4,
				&(*opp)[i].freq);

		if (ret) {
			pr_notice("Cannot get property freq(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				(i * OPP_ELEM_CNT) + 5,
				&(*opp)[i].clk_mux);
		if (ret) {
			pr_notice("Cannot get property clk mux(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		ret = of_property_read_u32_index(node, PROPNAME_SCP_DVFS_OPP,
				(i * OPP_ELEM_CNT) + 6,
				&(*opp)[i].resource_req);
		if (ret) {
			pr_notice("Cannot get property opp resource(%d)\n", ret);
			goto OPP_INIT_FAILED;
		}

		if((*opp)[i].resource_req != SCP_REQ_RELEASE && !g_dvfs_dev.has_pll_opp) {
			/* if there is no opp gear using pll at all, we can skip some
			 * pll control related flow by checking this flag (optional).
			 */
			pr_notice("has_pll_opp\n");
			g_dvfs_dev.has_pll_opp = true;
		}
	}
	return ret;

OPP_INIT_FAILED:
	kfree(*opp);
	return ret;
}

static int __init mt_scp_dts_init_pmic_data(void)
{
	unsigned int i;

	for (i = 0; i < MAX_SCP_DVFS_CHIP_HW; i++) {
		if (of_machine_is_compatible(scp_dvfs_hw_chip_ver[i])) {
			g_dvfs_dev.pmic_regs = &scp_pmic_hw_regs[i];
			return 0;
		}
	}

	/* init pmic data failed here */
	pr_notice("[%s]: no scp pmic regs found\n", __func__);
	return -ESCP_DVFS_NO_PMIC_REG_FOUND;
}

static int __init mt_scp_dts_regmap_init(struct platform_device *pdev,
		struct device_node *node)
{
	struct platform_device *pmic_pdev;
	struct device_node *pmic_node;
	struct pmic_main_chip *chip;
	struct regmap *regmap;

	/* get PMIC regmap */
	if (g_dvfs_dev.vlp_support)
		goto BYPASS_PMIC;

	g_dvfs_dev.bypass_pmic_rg_access = of_property_read_bool(node, PROPNAME_PMIC_NO_RG_RW);
	pr_notice("bypass_pmic_rg_access: %s\n", g_dvfs_dev.bypass_pmic_rg_access?"Yes":"No");
	if (g_dvfs_dev.bypass_pmic_rg_access)
		goto BYPASS_PMIC;

	pmic_node = of_parse_phandle(node, PROPNAME_PMIC, 0);
	if (!pmic_node) {
		dev_notice(&pdev->dev, "fail to find pmic node\n");
		goto REGMAP_FIND_FAILED;
	}

	pmic_pdev = of_find_device_by_node(pmic_node);
	if (!pmic_pdev) {
		dev_notice(&pdev->dev, "fail to find pmic device\n");
		goto REGMAP_FIND_FAILED;
	}

	chip = dev_get_drvdata(&(pmic_pdev->dev));
	if (!chip) {
		dev_notice(&pdev->dev, "fail to find pmic drv data\n");
		goto REGMAP_FIND_FAILED;
	}

	regmap = chip->regmap;
	if (IS_ERR_VALUE(regmap)) {
		g_dvfs_dev.pmic_regmap = NULL;
		dev_notice(&pdev->dev, "get pmic regmap fail\n");
		goto REGMAP_FIND_FAILED;
	}

	g_dvfs_dev.pmic_regmap = regmap;

BYPASS_PMIC:
	return 0;
REGMAP_FIND_FAILED:
	WARN_ON(1);
	return -ESCP_DVFS_REGMAP_INIT_FAILED;
}

static int __init mt_scp_dts_init(struct platform_device *pdev)
{
	struct device_node *node;
	int ret = 0;
	bool is_scp_dvfs_disable;
	const char *str = NULL;

	/* find device tree node of scp_dvfs */
	node = pdev->dev.of_node;
	if (!node) {
		dev_notice(&pdev->dev, "fail to find SCPDVFS node\n");
		return -ENODEV;
	}

	is_scp_dvfs_disable = of_property_read_bool(node, PROPNAME_SCP_DVFS_DISABLE);
	if (is_scp_dvfs_disable) {
		pr_notice("SCP DVFS is disabled, so bypass its init\n");
		return 0;
	}
	atomic_set(&g_is_scp_dvfs_feature_enable, 1);

	/*
	* if set, no VCORE DVS is needed & PMIC setting should
	* be done in SCP side.
	*/
	g_dvfs_dev.vlp_support = of_property_read_bool(node, PROPNAME_SCP_VLP_SUPPORT);
	if (g_dvfs_dev.vlp_support)
		pr_notice("[%s]: VCORE DVS sould be bypassed\n", __func__);

	if (g_dvfs_dev.vlp_support) {
		g_dvfs_dev.vow_lp_en_gear = -1;
	} else {
		ret = of_property_read_u32(node, PROPNAME_PMIC_VOW_LP_EN_GEAR,
			&g_dvfs_dev.vow_lp_en_gear);
		if (ret) {
			pr_notice("[%s]: no vow-lp-enable-gear property, set gear to -1\n",
				__func__);
			g_dvfs_dev.vow_lp_en_gear = -1;
		}
	}

	g_dvfs_dev.vlpck_support = of_property_read_bool(node, PROPNAME_SCP_VLPCK_SUPPORT);
	if (g_dvfs_dev.vlpck_support) {
		g_dvfs_dev.vlpck_bypass_phase1 = of_property_read_bool(node, "vlpck-bypass-phase1");
		pr_notice("[%s]: Use %d-phase VLP_CKSYS in calibration flow\n",
			__func__,
			g_dvfs_dev.vlpck_bypass_phase1 ? 1:2);
	} else {
		g_dvfs_dev.vlpck_bypass_phase1 = false;
	}

	/* Either PROPNAME_SCP_DVFS_CORES or PROPNAME_SCP_CORE_ONLINE_MASK should be given */
	ret = of_property_read_u32(node, PROPNAME_SCP_DVFS_CORES, &g_dvfs_dev.core_nums);
	if (!ret && (g_dvfs_dev.core_nums == 1 || g_dvfs_dev.core_nums == 2)) {
		g_dvfs_dev.core_online_msk = BIT(g_dvfs_dev.core_nums) - 1;
	}
	ret = of_property_read_u32(node, PROPNAME_SCP_CORE_ONLINE_MASK,
		&g_dvfs_dev.core_online_msk);
	if(!g_dvfs_dev.core_online_msk) {
		pr_notice("[%s]: find invalid core mask: %u, set to 0x1\n",
			__func__, g_dvfs_dev.core_online_msk);
		g_dvfs_dev.core_online_msk = SCP_CORE_0_ONLINE_MASK;
		WARN_ON(1);
	}

	if (g_dvfs_dev.vlp_support)
		g_dvfs_dev.pmic_sshub_en = false;
	else
		g_dvfs_dev.pmic_sshub_en = of_property_read_bool(node, PROPNAME_PMIC_SSHUB_SUP);

	ret = mt_scp_dts_regmap_init(pdev, node);
	if (ret) {
		pr_notice("[%s]: scp regmap init failed with err: %d\n",
			__func__, ret);
		goto DTS_FAILED;
	}

	if (g_dvfs_dev.vlp_support || g_dvfs_dev.bypass_pmic_rg_access)
		pr_notice("bypass pmic rg init\n");
	else {
		ret = mt_scp_dts_init_pmic_data();
		if (ret)
			goto DTS_FAILED;
	}

	/*
	 * 1. If "#define PROPNAME_U2_FM_SUPPORT" was set, it means common clock framework has provided API
	 * to use fmeter. And we should use mt_get_fmeter_freq(id, type) defined in clk-fmeter.h,
	 * instead of using get_ulposc_clk_by_fmeter*().
	 *
	 * 2. Only wehn mt_get_fmeter_freq havn't been provide, get_ulposc_clk_by_fmeter*() can
	 * be used temporarily.
	 */
	g_dvfs_dev.not_support_vlp_fmeter = of_property_read_bool(node,
					PROPNAME_NOT_SUPPORT_VLP_FM);
	g_dvfs_dev.ccf_fmeter_support = of_property_read_bool(node, PROPNAME_U2_FM_SUPPORT);
	if (!g_dvfs_dev.ccf_fmeter_support) {
		pr_notice("[%s]: fmeter api havn't been provided, use legacy one\n", __func__);
	} else {
		/* enum FMETER_TYPE */
		if (g_dvfs_dev.vlpck_support && !g_dvfs_dev.not_support_vlp_fmeter)
			g_dvfs_dev.ccf_fmeter_type = VLPCK;
		else
			g_dvfs_dev.ccf_fmeter_type = ABIST;

		/* enum FMETER_ID */
		ret = mt_get_fmeter_id(FID_ULPOSC2);
		g_dvfs_dev.ccf_fmeter_id = ret;
		pr_notice("[%s]: init ccf fmeter api, id: %d, type: %d\n",
			__func__, g_dvfs_dev.ccf_fmeter_id, g_dvfs_dev.ccf_fmeter_type);
		if (ret < 0) {
			pr_notice("[%s]: failed to init ccf fmeter api, id: %d, type: %d\n",
				__func__, g_dvfs_dev.ccf_fmeter_id, g_dvfs_dev.ccf_fmeter_type);
			goto DTS_FAILED;
		}
	}

	/* init dvfs data */
	ret = mt_scp_dts_init_dvfs_data(node, &g_dvfs_dev.opp);
	if (ret) {
		pr_notice("[%s]: scp dvfs opp data init failed with err: %d\n",
			__func__, ret);
		goto DTS_FAILED;
	}

	/* init clock mux/pll */
	ret = mt_scp_dts_clk_init(pdev);
	if (ret) {
		pr_notice("[%s]: init scp clk failed with err: %d\n",
			__func__, ret);
		goto DTS_FAILED_FREE_RES;
	}

	/* init ulposc cali dts data */
	ret = mt_scp_dts_ulposc_cali_init(node, &g_dvfs_dev.ulposc_hw);
	if (ret) {
		pr_notice("[%s]: init scp ulposc cali data with err: %d\n",
			__func__, ret);
		goto DTS_FAILED_FREE_RES;
	}

	if (!g_dvfs_dev.vlp_support) {
		/* get dvfsrc regulator */
		dvfsrc_vscp_power = regulator_get(&pdev->dev, PROPNAME_SCP_DVFSRC);
		if (IS_ERR(dvfsrc_vscp_power) || !dvfsrc_vscp_power) {
			pr_notice("regulator dvfsrc-vscp is not available\n");
			ret = PTR_ERR(dvfsrc_vscp_power);
			goto PASS;
		}

		/* get Vcore/Vsram Regulator */
		reg_vcore = devm_regulator_get_optional(&pdev->dev, PROPNAME_SCP_VCORE);
		if (IS_ERR(reg_vcore) || !reg_vcore) {
			pr_notice("regulator vcore sshub supply is not available\n");
			ret = PTR_ERR(reg_vcore);
			goto PASS;
		}

		reg_vsram = devm_regulator_get_optional(&pdev->dev, PROPNAME_SCP_VSRAM);
		if (IS_ERR(reg_vsram) || !reg_vsram) {
			pr_notice("regulator vsram sshub supply is not available\n");
			ret = PTR_ERR(reg_vsram);
			goto PASS;
		}
	}

	/* get secure_access_scp node */
	if (g_dvfs_dev.vlp_support)
		g_dvfs_dev.secure_access_scp = 1;
	else {
		g_dvfs_dev.secure_access_scp = 0;
		of_property_read_string(node, PROPNAME_SCP_DVFS_SECURE, &str);
		if (str && strcmp(str, "enable") == 0)
			g_dvfs_dev.secure_access_scp = 1;
	}
	pr_notice("secure_access_scp: %s\n", g_dvfs_dev.secure_access_scp?"enable":"disable");

	/* get SCP DVFS enable/disable flag */
	of_property_read_string(node, PROPNAME_SCP_DVFS_FLAG, &str);
	if (str && strcmp(str, "disable") == 0) {
		g_scp_dvfs_flow_enable = false;
		pr_notice("g_scp_dvfs_flow_enable = %d\n", g_scp_dvfs_flow_enable);
	}

PASS:
	return 0;

DTS_FAILED_FREE_RES:
	kfree(g_dvfs_dev.opp);
DTS_FAILED:
	return ret;
}

int scp_dvfs_feature_enable(void)
{
	return atomic_read(&g_is_scp_dvfs_feature_enable);
}

static int __init mt_scp_dvfs_pdrv_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = mt_scp_dts_init(pdev);
	if (ret) {
		pr_notice("[%s]: dts init failed with err: %d\n",
			__func__, ret);
		goto DTS_INIT_FAILED;
	}

	if (!scp_dvfs_feature_enable()) {
		atomic_set(&g_scp_dvfs_init_done, 1);
		pr_notice("bypass scp dvfs init\n");
		return 0;
	}

	/* init sshub */
	if (!g_dvfs_dev.vlp_support)
		mt_pmic_sshub_init();

	/* do ulposc calibration */
	mt_scp_dvfs_do_ulposc_cali_process();
	kfree(g_dvfs_dev.ulposc_hw.cali_configs);
	g_dvfs_dev.ulposc_hw.cali_configs = NULL;

	scp_dvfs_lock = wakeup_source_register(NULL, "scp wakelock");

#if IS_ENABLED(CONFIG_PM)
	ret = register_pm_notifier(&scp_pm_notifier_func);
	if (ret) {
		pr_notice("[%s]: failed to register PM notifier.\n", __func__);
		WARN_ON(1);
	}
#endif /* IS_ENABLED(CONFIG_PM) */

#if IS_ENABLED(CONFIG_PROC_FS)
	/* init proc */
	if (mt_scp_dvfs_create_procfs()) {
		pr_notice("mt_scp_dvfs_create_procfs fail..\n");
		WARN_ON(1);
	}
#endif /* CONFIG_PROC_FS */

	atomic_set(&g_scp_dvfs_init_done, 1);
	pr_notice("[%s]: scp_dvfs probe done\n", __func__);

	return 0;

DTS_INIT_FAILED:
	WARN_ON(1);

	return -ESCP_DVFS_INIT_FAILED;
}

/***************************************
 * this function should never be called
 ****************************************/
static int mt_scp_dvfs_pdrv_remove(struct platform_device *pdev)
{
	if (!scp_dvfs_feature_enable()) {
		pr_notice("bypass scp dvfs pdrv remove\n");
		return 0;
	}

	kfree(g_dvfs_dev.opp);
	kfree(g_dvfs_dev.ulposc_hw.cali_val_ext);
	kfree(g_dvfs_dev.ulposc_hw.cali_val);
	kfree(g_dvfs_dev.ulposc_hw.cali_freq);
	kfree(g_dvfs_dev.ulposc_hw.cali_configs);

	return 0;
}

static struct platform_driver mt_scp_dvfs_pdrv __refdata = {
	.probe = mt_scp_dvfs_pdrv_probe,
	.remove = mt_scp_dvfs_pdrv_remove,
	.driver = {
		.name = "scp-dvfs",
		.owner = THIS_MODULE,
		.of_match_table = scpdvfs_of_ids,
	},
};

/**********************************
 * mediatek scp dvfs initialization
 ***********************************/
int __init scp_dvfs_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = platform_driver_register(&mt_scp_dvfs_pdrv);
	if (ret) {
		pr_notice("fail to register scp dvfs driver @ %s()\n", __func__);
		goto fail;
	}

	pr_notice("[%s]: scp_dvfs init done\n", __func__);
	return 0;
fail:
	WARN_ON(1);
	return -ESCP_DVFS_INIT_FAILED;
}

void scp_dvfs_exit(void)
{
	platform_driver_unregister(&mt_scp_dvfs_pdrv);
}

