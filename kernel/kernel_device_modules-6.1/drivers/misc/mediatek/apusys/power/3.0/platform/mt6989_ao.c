// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/random.h>
#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "mt6989_apupwr.h"
#include "mt6989_apupwr_prot.h"
#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)

static uint32_t ce_pwr_on[] = {
	/* cmd_rcx_on_v1p1 */
	0x35011900,
	0xf803c812,
	0x01111203,
	0x0383c00d,
	0xbbbc001f,
	0xbbbe001f,
	0x0303c001,
	0x0283c00b,
	0x4b800004,
	0x38780000,
	0x30010500,
	0x43800002,
	0x38780000,
	0xb8b40011,
	0xb88e001f,
	0x48810005,
	0x38000004,
	0x0043c007,
	0x39781000,
	0x40810004,
	0x38000008,
	0x0043c007,
	0x39780000,
	0xb82c0016,
	0xb8030000,
	0xb806001f,
	0xb8010007,
	0x40010002,
	0x39100004,
	0xb82c0015,
	0xb8030000,
	0xb806001f,
	0xb8010007,
	0x40010002,
	0x39100010,
	0x41000004,
	0x0003c00a,
	0xb8000002,
	0x5011fffe,
	0x60000004,
	0x72c3c007,
	0x60000002,
	0x7303c007,
	0x60000006,
	0x73c3c007,
	0x7383c81d,
	0x07c3c81d,
	0x60000002,
	0x7303c81d,
	0x07c3c81d,
	0x60000002,
	0x48810002,
	0x8b83c00a,
	0x00028000,
	0xb804000a,
	0xb806001f,
	0x48010009,
	0x7503c007,
	0x39780008,
	0x31010000,
	0x60000006,
	0x0082c04c,
	0xb8880002,
	0x5091fffd,
	0x40010002,
	0x7543c007,
	0x4b80000d,
	0xb8340013,
	0xb806001f,
	0x48010005,
	0x39780001,
	0x31010022,
	0x0143c007,
	0x9203c00a,
	0x48000004,
	0x39780002,
	0x31010022,
	0x0143c007,
	0x4380000b,
	0xb836001f,
	0x48010005,
	0x39780002,
	0x31010021,
	0x0143c007,
	0x9283c00a,
	0x48000004,
	0x39780002,
	0x31010022,
	0x0143c007,
	0x00028000,
	0xb8040009,
	0xb806001f,
	0x48010009,
	0x7483c007,
	0x39780008,
	0x31010000,
	0x60000006,
	0x0082c054,
	0xb8880002,
	0x5091fffd,
	0x40010002,
	0x74c3c007,
	0x74c3c007,
	0x7243c81d,
	0x07c3c81d,
	0x60000002,
	0x7283c81e,
	0x07c3c81e,
	0x60000002,
	0x7103c002,
	0x39780000,
	0x31015000,
	0x0003c00a,
	0xb8000002,
	0x5011fffe,
	0x28010000,
};
static uint32_t ce_pwr_on_sz = sizeof(ce_pwr_on);
static uint32_t ce_pwr_off[] = {
	/* cmd_rcx_off_v1 */
	0x300e0000,
	0x20021048,
	0x20821049,
	0x35011900,
	0xf803c813,
	0x02011203,
	0x0003c00d,
	0xbb84001f,
	0xbbbe001f,
	0x0303c001,
	0x07c3c002,
	0x8f03c00a,
	0x9783c00a,
	0xb8340003,
	0xb806001f,
	0x48010004,
	0x7283c81d,
	0x07c3c81d,
	0x40010003,
	0x7283c81e,
	0x07c3c81e,
	0x60000002,
	0xb8b40019,
	0xb88e001f,
	0xb8010001,
	0x48010004,
	0x7243c81e,
	0x07c3c81e,
	0x40010003,
	0x7243c81d,
	0x07c3c81d,
	0x60000002,
	0x38780002,
	0x30010002,
	0x0043c007,
	0x4b800006,
	0xb8340013,
	0xb806001f,
	0x48010002,
	0x8a03c00a,
	0x43800004,
	0xb836001f,
	0x48010002,
	0x8a83c00a,
	0xb8b40011,
	0xb88e001f,
	0x48810003,
	0x7383c007,
	0x40810002,
	0x73c3c007,
	0x60000002,
	0xb8340002,
	0xb806001f,
	0x48010004,
	0x7303c81e,
	0x07c3c81e,
	0x40010003,
	0x7303c81d,
	0x07c3c81d,
	0x60000002,
	0x48810002,
	0x9403c00a,
	0xb834001f,
	0xb806001f,
	0x48010004,
	0x7383c81e,
	0x07c3c81e,
	0x40010003,
	0x7383c81d,
	0x07c3c81d,
	0x48810003,
	0x7283c007,
	0x40810002,
	0x72c3c007,
	0x60000002,
	0x70c3c007,
	0x48810002,
	0x8b03c00a,
	0x48810005,
	0x38782000,
	0x30018a00,
	0x0043c007,
	0x40810004,
	0x38781000,
	0x30018a00,
	0x0043c007,
	0x300e0000,
	0x2002104a,
	0x2082104b,
	0x28010000,
};
static uint32_t ce_pwr_off_sz = sizeof(ce_pwr_off);
#ifdef ENABLE_HW_VOTER
static uint32_t ce_hw_voter[] = {
	0x35011900,
	0x0182c356,
	0x0202c354,
	0x01c2c33a,
	0x0242c33b,
	0x7042c338,
	0x7042c339,
	0x7042c33c,
	0x01c2c354,
	0x3a78f3a5,
	0x3201ffff,
	0x02438230,
	0x28010000,
};
static uint32_t ce_hw_voter_sz = sizeof(ce_hw_voter);
#endif
static struct apu_power *papw;

static uint32_t vapu_pmic_slave_id = BUCK_VAPU_PMIC_ID;
static uint32_t vapu_en_set_offset = BUCK_VAPU_PMIC_REG_EN_SET_ADDR;
static uint32_t vapu_en_clr_offset = BUCK_VAPU_PMIC_REG_EN_CLR_ADDR;
static uint32_t vapu_en_shift = BUCK_VAPU_PMIC_REG_EN_SHIFT;
static uint32_t vapu_vosel_offset = BUCK_VAPU_PMIC_REG_VOSEL_ADDR;

/* regulator id */
static struct regulator *vapu_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;

static void _apu_w_are(int entry, ulong reg, ulong data)
{
	ulong are_entry_addr;
	/* (address of entry) = register */
	are_entry_addr = (ulong)papw->regs[apu_are] + 4 * ARE_ENTRY(entry);
	apu_writel(reg, (void __iomem *)are_entry_addr);
	apu_writel(data, (void __iomem *)(are_entry_addr + 4));
}

static void aputop_dump_reg(enum apupw_reg idx, uint32_t offset, uint32_t size)
{
	char buf[32];
	int ret = 0;

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08x: ",
		       (u32)(papw->phy_addr[idx]) + offset);
	if (ret)
		print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			papw->regs[idx] + offset, size, true);
}

static int check_if_rpc_alive(void)
{
	unsigned int regValue = 0x0;
	int bit_offset = 26; // [31:26] is reserved for debug

	regValue = apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	pr_info("%s , before: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);
	regValue |= (0x3a << bit_offset);
	apu_writel(regValue, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	regValue = 0x0;
	regValue = apu_readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	pr_info("%s , after: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);
	apu_clearl((BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)),
					papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	return ((regValue >> bit_offset) & 0x3f) == 0x3a ? 1 : 0;
}

// WARNING: can not call this API after acc initial or you may cause bus hang !
static void dump_rpc_lite_reg(int line)
{
	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
			__func__, line, 0,
			apu_readl(papw->regs[apu_acx0_rpc_lite]
				+ APU_RPC_TOP_SEL));
	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
			__func__, line, 1,
			apu_readl(papw->regs[apu_acx1_rpc_lite]
				+ APU_RPC_TOP_SEL));
	pr_info("%s ln_%d acx%d APU_RPC_TOP_SEL=0x%08x\n",
			__func__, line, 2,
			apu_readl(papw->regs[apu_acx2_rpc_lite]
				+ APU_RPC_TOP_SEL));
}

static int init_plat_pwr_res(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s %d ++\n", __func__, __LINE__);
	// vapu Buck
	vapu_reg_id = devm_regulator_get_optional(&pdev->dev, "vapu");
	if (IS_ERR_OR_NULL(vapu_reg_id)) {
		pr_info("regulator_get vapu_reg_id failed\n");
		return -ENOENT;
	}

	// vcore
	vcore_reg_id = devm_regulator_get_optional(&pdev->dev, "vcore");
	if (IS_ERR_OR_NULL(vcore_reg_id)) {
		pr_info("regulator_get vcore_reg_id failed\n");
		return -ENOENT;
	}

	// vsram
	vsram_reg_id = devm_regulator_get_optional(&pdev->dev, "vsram_core");
	if (IS_ERR_OR_NULL(vsram_reg_id)) {
		pr_info("regulator_get vsram_reg_id failed\n");
		return -ENOENT;
	}

	// enable vapu buck
	ret = regulator_enable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail enable vapu : %d\n", __func__, ret);
		return -1;
	}

	pr_info("%s %d vapu = %d(uv, %s) --\n",
			__func__, __LINE__,
			regulator_get_voltage(vapu_reg_id),
		    regulator_is_enabled(vapu_reg_id) ? "enabled" : "disabled");

	return 0;
}

static void destroy_plat_pwr_res(void)
{
	int ret = 0;

	// disable vapu buck
	ret = regulator_disable(vapu_reg_id);
	if (ret < 0) {
		pr_info("%s fail disable vapu : %d\n", __func__, ret);
		return;
	}

	regulator_put(vcore_reg_id);
	regulator_put(vsram_reg_id);
	regulator_put(vapu_reg_id);
	vcore_reg_id = NULL;
	vsram_reg_id = NULL;
	vapu_reg_id = NULL;
}

static void get_pll_pcw(uint32_t clk_rate, uint32_t *r1, uint32_t *r2)
{
	unsigned int fvco = clk_rate;
	unsigned int pcw_val;
	unsigned int postdiv_val = 1;
	unsigned int postdiv_reg = 0;

	while (fvco <= 1500) {
		postdiv_val = postdiv_val << 1;
		postdiv_reg = postdiv_reg + 1;
		fvco = fvco << 1;
	}

	pcw_val = (fvco * 1 << 14) / 26;
	if (postdiv_reg == 0) { //Fvco * 2 with post_divider = 2
		pcw_val = pcw_val * 2;
		postdiv_val = postdiv_val << 1;
		postdiv_reg = postdiv_reg + 1;
	} //Post divider is 1 is not available

	*r1 = postdiv_reg;
	*r2 = pcw_val;
}

static void __apu_engine_acc_on(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t eng_acc[] = {MDLA_ACC_BASE, MVPU_ACC_BASE};
	int eng_acc_arr_size = ARRAY_SIZE(eng_acc);
	ulong addr = 0;
	uint32_t val = 0;
	int ret = 0, acc_idx;

	for (acc_idx = 0; acc_idx < eng_acc_arr_size; acc_idx++) {
		addr = (ulong)papw->regs[apu_acc] + eng_acc[acc_idx] + APU_ACC_AUTO_CTRL_SET0;
		/* TINFO="[pllon/off]Step2: auto enable acc_idx clock" */
		apu_setl(1 << 9, (void __iomem *)addr);
		addr = (ulong)papw->regs[apu_acc] + eng_acc[acc_idx] + APU_ACC_AUTO_STATUS0;
		ret = readl_relaxed_poll_timeout_atomic((void *)addr, val,
							(val & (0x1UL << 6)), 50, 10000);
		if (ret)
			pr_info("%s %d wait hacc-%d on fail, ret = %d\n",
			       __func__, __LINE__, acc_idx, ret);
	}
}

static void __apu_pll_init(void)
{
	// need to 1-1 in order mapping to these two array
	uint32_t pll_b[] = {MNOC_PLL_BASE, UP_PLL_BASE,
					MVPU_PLL_BASE, MDLA_PLL_BASE};
	int32_t pll_freq_out[] = {400, 400, 400, 400}; // MHz
	uint32_t pcw_val, posdiv_val;
	int pll_arr_size = ARRAY_SIZE(pll_b);
	int pll_i, are_idx;

	// Step4. Initial PLL setting
	pr_info("PLL init %s %d --\n", __func__, __LINE__);
	/* Turn on RCX_AO ARE */
	apu_writel(1 << 21, papw->regs[apu_are]);
	apu_writel(1 << 21, papw->regs[apu_are] + 0x10000);
	/* write RCX_AO start entry/size */
	apu_writel(ARE_ENTRY(RCX_AO_BEGIN) | (ARE_ENTRIES(RCX_AO_BEGIN, RCX_AO_END) << 16),
			papw->regs[apu_are] + ARE_RCX_AO_CONFIG);
	are_idx = PLL_ENTRY_BEGIN;
	for (pll_i = 0 ; pll_i < pll_arr_size ; pll_i++) {
		// PCW value always from hopping function: ofs 0x300
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_HP_EN,
			   0x1 << 0);
		// Hopping function reset release: ofs 0x30C
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_RST_CON,
			   0x1 << 0);
		// Hopping function clock enable: ofs 0x308
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL_CLK_CON,
			   0x1 << 0);
		// Hopping function enable: ofs 0x314
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL0_CFG,
			   (0x1 << 0) | (0x1 << 2));
		posdiv_val = 0;
		pcw_val = 0;
		get_pll_pcw(pll_freq_out[pll_i], &posdiv_val, &pcw_val);
		// POSTDIV: ofs 0x20C , [26:24] RG_PLL_POSDIV
		// 3'b000: /1 , 3'b001: /2 , 3'b010: /4
		// 3'b011: /8 , 3'b100: /16
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1C_PLL1_CON1,
			   ((0x1 << 31) | (posdiv_val << 24) | pcw_val));
		// PCW register: ofs 0x31C
		// [31] FHCTL0_PLL_TGL_ORG
		// [21:0] FHCTL0_PLL_ORG set to PCW value
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_pll] + pll_b[pll_i] + PLL1CPLL_FHCTL0_DDS,
			   ((0x1 << 31) | pcw_val));
	}
}

/* Cost 18 ARE entries, ARDCM(8) + ACC(4+6=10) */
static void __apu_acc_init(void)
{
	uint32_t top[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t eng[] = {MVPU_ACC_BASE, MDLA_ACC_BASE};
	int top_acc_arr_size = ARRAY_SIZE(top);
	int eng_acc_arr_size = ARRAY_SIZE(eng);
	int acc_idx;
	int are_idx = ACC_ENTRY_BEGIN;

	// Step6. Initial ACC setting (@ACC)
	for (acc_idx = 0 ; acc_idx < top_acc_arr_size ; acc_idx++) {
		// DCM_EN/DBC_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL1,
			   0x00001006);
		// APB_DCM_EN/APB_DBC_EN/APB_IDLE_FSEL_UPD_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000016);
		// IDLE_FSEL/DBC_CNT
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL1,
			   0x07F0F006);
		// APB_LOAD_TOG
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000036);
		// CGEN_SOC
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx]  + APU_ACC_CONFG_CLR0,
			   0x00000004);
		// HW_CTRL_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + top[acc_idx] + APU_ACC_CONFG_SET0,
			   0x00008000);
	}

	for (acc_idx = 0 ; acc_idx < eng_acc_arr_size ; acc_idx++) {
		// DCM_EN/DBC_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL1,
			   0x00001006);
		// APB_DCM_EN/APB_DBC_EN/APB_IDLE_FSEL_UPD_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000016);
		// IDLE_FSEL/DBC_CNT
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL1,
			   0x07F0F006);
		// APB_LOAD_TOG
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ARDCM_CTRL0,
			   0x00000036);
		// CGEN_SOC
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx]  + APU_ACC_CONFG_CLR0,
			   0x00000004);
		// HW_CTRL_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx] + APU_ACC_CONFG_SET0,
			   0x00008000);
		// CLK_REQ_SW_EN
		_apu_w_are(are_idx++,
			   (ulong)papw->phy_addr[apu_acc] + eng[acc_idx] + APU_ACC_AUTO_CTRL_SET0,
			   0x00000100);
	}
}

static void __apu_buck_off_cfg(void)
{
	pr_info("%s %d ++\n", __func__, __LINE__);
	// Step12. After APUsys is finished, update the following register to 1,
	//     ARE will use this information to ensure the SRAM in ARE is
	//     trusted or not
	//     apusys_initial_done

	pr_info("%s %d --\n", __func__, __LINE__);
}

/*
 * low 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF1 (or APU_PCU_BUCK_ON_DAT0_L)
 *	[31:16] offset to update
 *	[15:00] data to update
 *
 * high 32-bit data for PMIC control
 *	APU_PCU_PMIC_TAR_BUF2 (or APU_PCU_BUCK_ON_DAT0_H)
 *	[2:0] cmd_op, read:0x3 , write:0x7
 *	[3]: pmifid,
 *	[7:4]: slvid
 *	[8]: bytecnt
 */
static void __apu_pcu_init(void)
{
	uint32_t cmd_op_w = 0x7;
	uint32_t pmif_id = 0x1;

	if (papw->env == FPGA)
		return;

	// auto buck enable
	apu_writel((0x1 << 3), papw->regs[apu_pcu] + APU_PCUTOP_CTRL_SET);
	/*
	 * Step1. enable cmd operation in auto buck on/off flow
	 * [0]: enable auto ON cmd0 (set vapu voltage to 0.75v),
	 * [1]: enable auto ON cmd1 (turn vapu buck ON)
	 * [4]: enable auto OFF cmd0 (turn vapu buck OFF),
	 */
	apu_writel(0x13,  papw->regs[apu_pcu] + APU_PCU_BUCK_STEP_SEL);

	// Step1. fill-in auto ON cmd0
	apu_writel((vapu_vosel_offset << 16) | (750000 / 6250),
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_L);
	apu_writel((vapu_pmic_slave_id << 4) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT0_H);

	// Step2. fill-in auto ON cmd1
	apu_writel((vapu_en_set_offset << 16) | (0x1U << vapu_en_shift),
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT1_L);
	apu_writel((vapu_pmic_slave_id << 4) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_ON_DAT1_H);

	// Step3. fill-in auto OFF cmd0
	apu_writel((vapu_en_clr_offset << 16) | (0x1U << vapu_en_shift),
		papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_L);
	apu_writel((vapu_pmic_slave_id << 4) | (pmif_id << 3) | cmd_op_w,
		papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_DAT0_H);

	// Step4. fill-in settle time for auto ON/OFF cmd
	apu_writel(0x1,  papw->regs[apu_pcu] + APU_PCU_BUCK_ON_SLE0); // 300us
	apu_writel(0xC8,  papw->regs[apu_pcu] + APU_PCU_BUCK_ON_SLE1); // 300us
	apu_writel(0x1,  papw->regs[apu_pcu] + APU_PCU_BUCK_OFF_SLE0); // 300us
	pr_info("PCU init %s %d %d--\n", __func__, __LINE__, pmif_id);
}

static void __apu_rpclite_init(enum t_acx_id acx_idx)
{
	uint32_t sleep_type_offset[] = {0x0210, 0x0214, 0x0218, 0x021C};
	enum apupw_reg rpc_lite_base[CLUSTER_NUM];
	int ofs_arr_size = sizeof(sleep_type_offset) / sizeof(uint32_t);
	int ofs_idx;

	pr_info("%s %d ++\n", __func__, __LINE__);
	rpc_lite_base[ACX0] = apu_acx0_rpc_lite;
	rpc_lite_base[ACX1] = apu_acx1_rpc_lite;
	rpc_lite_base[ACX2] = apu_acx2_rpc_lite;

	for (ofs_idx = 0; (ofs_idx < ofs_arr_size); ofs_idx++) {
		// Memory setting
		apu_clearl((0x1 << 0),
			   papw->regs[rpc_lite_base[acx_idx]] + sleep_type_offset[ofs_idx]);
	}

	// Control setting
	apu_setl(0x0000009E,
		 papw->regs[rpc_lite_base[acx_idx]] + APU_RPC_TOP_SEL);
	pr_info("%s %d ++\n", __func__, __LINE__);
}

static void __apu_rpc_mdla_init(void)
{
	pr_info("%s %d ++\n", __func__, __LINE__);
	// RPC-mdla: iTCM in MDLA need to setup to sleep type
	apu_clearl((0x1 << 0), papw->regs[apu_rpctop_mdla] + 0x0210);
	pr_info("%s %d ++\n", __func__, __LINE__);
}

static void __apu_rpclite_init_all(void)
{
	uint32_t sleep_type_offset[] = {0x0210, 0x0214, 0x0218, 0x021C};
	enum apupw_reg rpc_lite_base[CLUSTER_NUM];
	int ofs_arr_size = ARRAY_SIZE(sleep_type_offset);
	int acx_idx, ofs_idx;

	pr_info("%s %d ++\n", __func__, __LINE__);
	rpc_lite_base[ACX0] = apu_acx0_rpc_lite;

	for (acx_idx = 0 ; acx_idx < CLUSTER_NUM ; acx_idx++) {
		for (ofs_idx = 0; (ofs_idx < ofs_arr_size); ofs_idx++) {
			// Memory setting
			apu_clearl((0x1 << 0),
					papw->regs[rpc_lite_base[acx_idx]]
					+ sleep_type_offset[ofs_idx]);
		}
		// Control setting
		apu_setl(0x0000009E, papw->regs[rpc_lite_base[acx_idx]]
					+ APU_RPC_TOP_SEL);
	}

	dump_rpc_lite_reg(__LINE__);
	pr_info("%s %d ++\n", __func__, __LINE__);
}

static void __apu_rpc_init(void)
{
	pr_info("RPC init %s %d ++\n", __func__, __LINE__);
	// Step7. RPC: memory types (sleep or PD type)
	// RPC: APU TCM set sleep type
	apu_writel(0x74, papw->regs[apu_rpc] + 0x0200);
	// Step9. RPCtop initial
	/* 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 (bit offset)
	 *  1  0  1  1  1  0  0  0  0  0  0  0  0  0  0  0 --> 0xB800
	 * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 (bit offset)
	 *  1  1  0  1  0  1  0  1  0  0  0  0  1  1  1  1 --> 0xD50F, unmask mbox
	 */
	apu_setl(0xB800D50F, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	/* if PRC_HW, turn off CE wake up RPC */
	if (papw->rcx == RPC_HW)
		apu_clearl(1 << 10, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);

	// BUCK_PROT_SEL
	apu_setl((0x1 << 20), papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1);
	pr_info("%s APU_RPC_TOP_SEL  0x%08x = 0x%08x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_TOP_SEL),
			readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL));
	pr_info("%s APU_RPC_TOP_SEL_1 0x%08x = 0x%08x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_TOP_SEL_1),
			readl(papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1));
	pr_info("RPC init %s %d --\n", __func__, __LINE__);
}

#ifdef ENABLE_HW_VOTER_TEST
static void apu_hw_voter_test(void)
{
	uint32_t idx, regValue;
	uint32_t usr_idx;
	uint32_t min_val = HW_VOTER_TOTAL_OPP_ENTRY - 1;
	uint32_t cur_val;
	uint32_t upp_val = 0;
	uint32_t low_val = HW_VOTER_TOTAL_OPP_ENTRY - 1;
	uint32_t random;

	apu_writel(upp_val, papw->regs[apu_are] + APU_ACE_UPP_LV_0);
	apu_writel(upp_val, papw->regs[apu_are] + APU_ACE_UPP_LV_1);
	apu_writel(low_val, papw->regs[apu_are] + APU_ACE_LOW_LV_0);
	apu_writel(low_val, papw->regs[apu_are] + APU_ACE_LOW_LV_1);

	for (usr_idx = 0; usr_idx < 16; usr_idx++) {
		cur_val = apu_readl(papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x350);
		/* set DVFS OPP for each user */
		get_random_bytes(&random, sizeof(random));
		random = random % 19u;
		apu_writel(random,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x300 + usr_idx * 0x4);

		//Calculate min_val for check design correctness
		if (random < upp_val)
			min_val = upp_val;
		else if (random > low_val)
			min_val = low_val;
		else if (random < cur_val)
			min_val = random;
		else
			min_val = cur_val;

		pr_info("%s %d min_val = %x, cur_val = %x, random = %x, MT0(%x) != MT1(%x)\n",
		__func__, __LINE__, min_val, cur_val, random,
		apu_readl(papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x2E8),
		apu_readl(papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x2EC));

		do {
			regValue = apu_readl(papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x350);
		} while (min_val != regValue);

		do {
			regValue = apu_readl(papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x35c);
			regValue = regValue >> 8;
		} while (regValue != 1);

		/* check snapshot value */
		for (idx = 0; idx < 7; idx++) {
			regValue = apu_readl(papw->regs[apu_are] +
					APU_RCX_HW_VOTER_BASE + 0x280 + idx * 0x4);

			if (regValue != (min_val << idx))
				pr_info("%s %d ERROR:snapshot 0 data check fail, %x != %x\n",
					__func__, __LINE__, regValue, min_val << idx);
		}

		for (idx = 0; idx < 7; idx++) {
			regValue = apu_readl(papw->regs[apu_are] +
					APU_RCX_HW_VOTER_BASE + 0x2a0 + idx * 0x4);

			if (regValue != (cur_val << idx))
				pr_info("%s %d ERROR:snapshot 1 data check fail, %x != %x\n",
					__func__, __LINE__, regValue, cur_val << idx);
		}

		for (idx = 0; idx < 4; idx++) {
			regValue = apu_readl(papw->regs[apu_are] +
					APU_RCX_HW_VOTER_BASE + 0x2d0 + idx * 0x4);
			if (regValue != idx)
				pr_info("%s %d ERROR:snapshot DP data check fail!!!\n",
					__func__, __LINE__);
		}
		apu_writel(HW_VOTER_TOTAL_OPP_ENTRY - 1,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x300 + usr_idx * 0x4);
	}
}
#endif

#ifdef ENABLE_HW_VOTER
static void apu_hw_voter_opp_table(void)
{
	uint32_t idx;

	pr_info("%s %d ++\n", __func__, __LINE__);

	/* mt_20x164 */
	for (idx = 0; idx <= HW_VOTER_TOTAL_OPP_ENTRY - 1; idx++) {
		apu_writel(idx << 0,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + idx * 0x4); //PLL0
		apu_writel(idx << 1,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x50 + idx * 0x4); //PLL1
		apu_writel(idx << 2,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0xa0 + idx * 0x4); //PLL2
		apu_writel(idx << 3,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0xf0 + idx * 0x4); //PLL3
		apu_writel(idx << 4,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x140 + idx * 0x4); //PLL4
		apu_writel(idx << 5,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x190 + idx * 0x4); //PMIC
		apu_writel(idx << 6,
			papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x1e0 + idx * 0x4); //DUMMY
	}

	/* dp_32x4 */
	for (idx = 0; idx < 4; idx++)
		apu_writel(idx, papw->regs[apu_are] + APU_RCX_HW_VOTER_BASE + 0x2c0 + idx * 0x4);

	pr_info("%s %d --\n", __func__, __LINE__);
}

static void __apu_hw_voter_init(void)
{
	uint32_t kernel_size, idx, reg;

	pr_info("%s %d ++\n", __func__, __LINE__);

	/* fill in HW voter firmware */
	kernel_size = ce_hw_voter_sz / 4;
	for (idx = 0; idx < kernel_size; idx++)
		apu_writel(ce_hw_voter[idx], papw->regs[apu_are] + 0x4000 + idx * 0x4);

	/* fill in entry 20 */
	apu_writel((0x4000 / 4) << 16, papw->regs[apu_are] + APU_ACE_HW_CONFIG_7);

	/* Didcate CE_0 for HW voter(HW_JOB_15) */
	apu_writel(0x0E0E0E0E, papw->regs[apu_are] + APU_ACE_HW_JOB_BITMAP_0);
	apu_writel(0x0E0E0E0E, papw->regs[apu_are] + APU_ACE_HW_JOB_BITMAP_4);
	apu_writel(0x0E0E0E0E, papw->regs[apu_are] + APU_ACE_HW_JOB_BITMAP_8);
	apu_writel(0x010E0E0E, papw->regs[apu_are] + APU_ACE_HW_JOB_BITMAP_12);

	apu_hw_voter_opp_table();

	/* 1. Fill the current opp in HW voter */
	apu_writel(HW_VOTER_TOTAL_OPP_ENTRY - 1, papw->regs[apu_are] + APU_ACE_CMN_LV);
	/* 2. Initial 16 ballot boxes */
	for (reg = APU_ACE_USR_LV_0; reg <= APU_ACE_USR_LV_15; reg += 4)
		apu_writel(HW_VOTER_TOTAL_OPP_ENTRY - 1, papw->regs[apu_are] + reg);

	/* 3. Configure upper and lower bound of opp */
	apu_writel(0x00, papw->regs[apu_are] + APU_ACE_UPP_LV_0);
	apu_writel(0x00, papw->regs[apu_are] + APU_ACE_UPP_LV_1);
	apu_writel(HW_VOTER_TOTAL_OPP_ENTRY - 1, papw->regs[apu_are] + APU_ACE_LOW_LV_0);
	apu_writel(HW_VOTER_TOTAL_OPP_ENTRY - 1, papw->regs[apu_are] + APU_ACE_LOW_LV_1);
	/* 4. Enable HW voter */
	apu_writel(0x1 << 4 | 0x1 << 6, papw->regs[apu_are] + APU_ACE_DVFS_ST);

	#ifdef ENABLE_HW_VOTER_TEST
	apu_hw_voter_test();
	#endif

	pr_info("%s %d --\n", __func__, __LINE__);
}
#endif

static int _poll_hwvoter_pd(void)
{
	int retry = 1000;

	while (((apu_readl(papw->regs[apu_are] + APU_ACE_DVFS_ST) >> 8) & 0xF) != 0x1) {
		udelay(1);
		if (--retry < 0) {
			pr_info("%s %d failed\n", __func__, __LINE__);
			return -1;
		}
	}

	return 0;
}

static void __apu_hw_voter_disable(void)
{
	pr_info("%s %d ++\n", __func__, __LINE__);

	/* 1. Disable HW voter */
	apu_writel(0x1 << 5 | 0x1 << 7, papw->regs[apu_are] + APU_ACE_DVFS_ST);
	/* 2. Polling HW voter as IDLE */
	_poll_hwvoter_pd();
	/* 3. disable CE0 */
	apu_writel(0x1 << 1, papw->regs[apu_are] + APU_CE0_ABORT);

	pr_info("%s %d --\n", __func__, __LINE__);
}

static int __apu_are_init(struct device *dev)
{
	uint32_t entry = 0;
	char buf[512];
	int ret = 0;
	uint32_t kernel_size, idx;

	pr_info("ARE init %s, ce_pwr_on_sz = %d, ce_pwr_off_sz = %d\n",
		__func__, ce_pwr_on_sz, ce_pwr_off_sz);

	/* Partial init ARE to release slpprot_ctl */
	apu_writel(0xFFFU<<20, papw->regs[apu_are]);
	/* Write SRAM entry 0's value to 0x190B0000 */
	apu_writel(0xFFFU<<20, papw->regs[apu_are] + 0x10000);

	/*  clean entry#1(0x190A0004) ~ #27(0x190A006C) */
	for (entry = 4; entry < 0x6C; entry += 4)
		apu_writel(0, papw->regs[apu_are] + entry);

	/* Disable sMMU by set HW flag 10 */
	apu_setl(0x1<<10, papw->regs[apu_are] + 0x105D4);

	if (papw->rcx == RPC_HW)
		return 0;

	apu_setl(1<<23, papw->regs[apu_are]);
	/* Turn on CE enable */

	/* fill in rpc power on/off firmware */
	kernel_size = ce_pwr_on_sz / 4;
	for (idx = 0; idx < kernel_size; idx++)
		apu_writel(ce_pwr_on[idx], papw->regs[apu_are] + 0x2000 + idx * 0x4);

	kernel_size = ce_pwr_off_sz / 4;
	for (idx = 0; idx < kernel_size; idx++)
		apu_writel(ce_pwr_off[idx], papw->regs[apu_are] + 0x3000 + idx * 0x4);

	/* fill in entry 4 */
	entry = (0x3000 / 4) << 16 | (0x2000 / 4);
	apu_writel(entry, papw->regs[apu_are] + APU_ACE_HW_CONFIG_0);

	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are]));
	if (!ret)
		print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
			       16, 4, papw->regs[apu_are], 0x20, 1);

	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are] + 0x2000));
	if (!ret)
		print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
			       16, 4, papw->regs[apu_are] + 0x2000, ce_pwr_on_sz, 1);

	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are] + 0x3000));
	if (!ret)
		print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
			       16, 4, papw->regs[apu_are] + 0x3000, ce_pwr_off_sz, 1);

	pr_info("ARE init %s %d --\n", __func__, __LINE__);

	return 0;
}

#ifdef BYPASS_CE_ENABLE
static int __apu_ce_init(struct device *dev)
{
	uint32_t entry = 0;
	char buf[512];
	int ret = 0;
	uint32_t kernel_size, idx;

	pr_info("CE init %s, ce_pwr_on_sz = %d, ce_pwr_off_sz = %d\n",
		__func__, ce_pwr_on_sz, ce_pwr_off_sz);

	/* Disable sMMU by set HW flag 10 */
	apu_setl(0x1<<10, papw->regs[apu_are] + 0x105D4);

	/* Turn on CE enable */
	apu_setl(1<<23, papw->regs[apu_are]);

	/* fill in rpc power on/off firmware */
	kernel_size = ce_pwr_on_sz / 4;
	for (idx = 0; idx < kernel_size; idx++)
		apu_writel(ce_pwr_on[idx], papw->regs[apu_are] + 0x2100 + idx * 0x4);

	kernel_size = ce_pwr_off_sz / 4;
	for (idx = 0; idx < kernel_size; idx++)
		apu_writel(ce_pwr_off[idx], papw->regs[apu_are] + 0x3000 + idx * 0x4);

	/* fill in entry 4 */
	entry = (0x3000 / 4) << 16 | (0x2100 / 4);
	apu_writel(entry, papw->regs[apu_are] + APU_ACE_HW_CONFIG_0);

	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are]));
	if (!ret)
		print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
			       16, 4, papw->regs[apu_are], 0x20, 1);

	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are] + 0x2100));
	if (!ret)
		print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
			       16, 4, papw->regs[apu_are] + 0x2100, ce_pwr_on_sz, 1);

	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, sizeof(buf), "phys 0x%08x ", (u32)(papw->phy_addr[apu_are] + 0x3000));
	if (!ret)
		print_hex_dump(KERN_WARNING, buf, DUMP_PREFIX_OFFSET,
			       16, 4, papw->regs[apu_are] + 0x3000, ce_pwr_off_sz, 1);

	pr_info("ARE init %s %d --\n", __func__, __LINE__);

	return 0;
}
#endif

// backup solution : send request for RPC sleep from APMCU
static int __apu_off_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;
	/* TINFO="APU_RPC_TOP_SEL[7] - BYPASS WFI" */
	apu_setl(1 << 7, papw->regs[apu_rpc] + APU_RPC_TOP_SEL);
	/* ONLY FPGA NEED, Ignore Sleep Protect Rdy */
	if (papw->env == FPGA)
		apu_setl(1 << 12, papw->regs[apu_rpc] + APU_RPC_MTCMOS_SW_CTRL0);
	/* SLEEP request */
	apu_writel(0x00000000, papw->regs[apu_md32_mbox] + 0x1380);
	apu_setl(1, papw->regs[apu_rpc] + APU_RPC_TOP_CON);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, !(val & 0x1UL), 50, 10000);
	if (ret)
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);

	aputop_dump_reg(apu_rpc, 0, 0x50);
	dev_info(dev, "%s APUSYS_VCORE_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APUSYS_VCORE_CG_CON),
			readl(papw->regs[apu_vcore] + APUSYS_VCORE_CG_CON));
	dev_info(dev, "%s APU_RCX_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rcx] + APU_RCX_CG_CON),
			readl(papw->regs[apu_rcx] + APU_RCX_CG_CON));
	dev_info(dev, "%s APU_ARE_GCONFIG 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_are] + APU_ARE_GCONFIG),
			readl(papw->regs[apu_are] + APU_ARE_GCONFIG));
	dev_info(dev, "%s APU_ARE_STATUS 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_are] + APU_ARE_STATUS),
			readl(papw->regs[apu_are] + APU_ARE_STATUS));
	dev_info(dev, "%s APU_CE_IF_PC 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_are] + APU_CE_IF_PC),
			readl(papw->regs[apu_are] + APU_CE_IF_PC));

	return ret;
}

static void mtk_clk_acc_get_rate(void)
{
	int32_t output = 0, i = 0, j = 0;
	uint32_t tempValue = 0;
	bool timeout = false;
	//uint32_t phy_confg_set;
	//uint32_t phy_fm_confg_set, phy_fm_confg_clr, phy_fm_sel, phy_fm_cnt;
	ulong confg_set;
	ulong fm_confg_set, fm_confg_clr, fm_sel, fm_cnt;
	uint32_t loop_ref = 0;  // 0 for Max freq  ~ 1074MHz
	int32_t retry = 30;

	uint32_t acc_base_arr[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t acc_offset_arr[] = {
				APU_ACC_CONFG_SET0, APU_ACC_FM_SEL, APU_ACC_FM_CONFG_SET,
				APU_ACC_FM_CONFG_CLR, APU_ACC_FM_CNT};

	for (j = 0 ; j < 2 ; j++) {
		confg_set = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[0];
		fm_sel = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[1];
		fm_confg_set = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[2];
		fm_confg_clr = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[3];
		fm_cnt = (ulong)papw->regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[4];

		/* reset */
		apu_writel(0x0, (void __iomem *)fm_sel);
		apu_writel(apu_readl((void __iomem *)fm_sel), (void __iomem *)fm_sel);
		apu_writel(apu_readl((void __iomem *)fm_sel) | (loop_ref << 16),
			(void __iomem *)fm_sel);
		apu_writel(BIT(0), (void __iomem *)fm_confg_set);
		apu_writel(BIT(1), (void __iomem *)fm_confg_set);

		/* wait frequency meter finish */
		while (!(apu_readl((void __iomem *)fm_confg_set) & BIT(4))) {
			udelay(10);
			i++;
			if (i > retry) {
				timeout = true;
				pr_info("%s acc error, fm_sel = 0x%08x, fm_confg_set = 0x%08x\n",
					__func__,
					apu_readl((void __iomem *)fm_sel),
					apu_readl((void __iomem *)fm_confg_set));
				break;
			}
		}

		if ((!timeout) &&
			!(apu_readl((void __iomem *)fm_confg_set) & BIT(5))) {
			tempValue = apu_readl((void __iomem *)fm_cnt);
			tempValue = tempValue & 0xFFFFF;
			output = tempValue * 16384 / ((loop_ref + 1) * 1000);  //KHz
		} else {
			output = 0;
		}
		pr_info("%s: MNOC/UP ACC:%d\n", __func__, output);

		apu_writel(BIT(4), (void __iomem *)fm_confg_clr);
		apu_writel(BIT(1), (void __iomem *)fm_confg_clr);
		apu_writel(BIT(0), (void __iomem *)fm_confg_clr);
	}
}

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	dev_info(dev, "%s Before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));
	/* TINFO="Enable AFC enable" */
	apu_setl(0x1 << 16, papw->regs[apu_rpc] + APU_RPC_TOP_SEL_1);
	/* wake up RPC */
	//apu_writel(0x00000100, papw->regs[apu_rpc] + APU_RPC_TOP_CON);
	apu_writel(0x00000001, papw->regs[apu_md32_mbox] + 0x1380);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s polling RPC RDY timeout, val = 0x%x, ret %d\n", __func__, val, ret);
		goto out;
	}

	/* clear wakeup signal */
	apu_writel(0x1 << 12, papw->regs[apu_rpc] + APU_RPC_TOP_CON);
	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));
	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_STATUS_1),
			val, (val & (0x1 << 13)), 50, 10000);
	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		goto out;
	}

	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_PWR_ACK),
			val, (val & 0x3UL), 50, 10000);
	if (ret) {
		pr_info("%s power chain(vcore[1] rcx[0]) fail, ret %d\n", __func__, ret);
		goto out;
	}

	/* clear vcore/rcx cgs */
	apu_writel(0xFFFFFFFF, papw->regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, papw->regs[apu_rcx] + APU_RCX_CG_CLR);

	mtk_clk_acc_get_rate();

	dev_info(dev, "%s RCX before Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(papw->phy_addr[apu_rcx] + 0x0300),
		readl(papw->regs[apu_rcx] + 0x0300));

	dev_info(dev, "%s RCX before Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(papw->phy_addr[apu_rcx] + 0x0304),
		readl(papw->regs[apu_rcx] + 0x0304));

	/* access spare RG */
	apu_writel(0x12345678, papw->regs[apu_rcx] + 0x0300);
	apu_writel(0x12345678, papw->regs[apu_rcx] + 0x0304);

	dev_info(dev, "%s RCX after Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(papw->phy_addr[apu_rcx] + 0x0300),
		readl(papw->regs[apu_rcx] + 0x0300));

	dev_info(dev, "%s RCX after Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(papw->phy_addr[apu_rcx] + 0x0304),
		readl(papw->regs[apu_rcx] + 0x0304));
out:
	dev_info(dev, "%s APUSYS_VCORE_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_vcore] + APUSYS_VCORE_CG_CON),
			readl(papw->regs[apu_vcore] + APUSYS_VCORE_CG_CON));
	dev_info(dev, "%s APU_RCX_CG_CON 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_rcx] + APU_RCX_CG_CON),
			readl(papw->regs[apu_rcx] + APU_RCX_CG_CON));
	dev_info(dev, "%s APU_ARE_GCONFIG 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_are] + APU_ARE_GCONFIG),
			readl(papw->regs[apu_are] + APU_ARE_GCONFIG));
	dev_info(dev, "%s APU_ARE_STATUS 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_are] + APU_ARE_STATUS),
			readl(papw->regs[apu_are] + APU_ARE_STATUS));
	dev_info(dev, "%s APU_CE_IF_PC 0x%x = 0x%x\n",
			__func__,
			(u32)(papw->phy_addr[apu_are] + APU_CE_IF_PC),
			readl(papw->regs[apu_are] + APU_CE_IF_PC));
	return ret;
}

static int __apu_wake_rpc_acx(struct device *dev, enum t_acx_id acx_id)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else if (acx_id == ACX1) {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	} else if (acx_id == ACX2) {
		rpc_lite_base = apu_acx2_rpc_lite;
		acx_base = apu_acx2;
	} else {
		return -ENODEV;
	}

	dev_info(dev, "%s ctl p1:%d p2:%d\n",
			__func__, rpc_lite_base, acx_base);
	/* TINFO="Enable AFC enable" */
	apu_setl((0x1 << 16), papw->regs[rpc_lite_base] + APU_RPC_TOP_SEL_1);
	/* wake acx rpc lite */
	apu_writel((0x1 << 0), papw->regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(papw->regs[rpc_lite_base] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)), 50, 10000);
	if (ret) {
		pr_info("%s wake up acx%d_rpc fail, ret %d\n",
				__func__, acx_id, ret);
		goto out;
	}

	dev_info(dev, "%s ACX%d APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));
	if (acx_id == ACX0 || acx_id == ACX1) {
		dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
			readl(papw->regs[acx_base] + APU_ACX_CONN_CG_CON));
		/* clear acx0 CGs */
		apu_writel(0xFFFFFFFF, papw->regs[acx_base] + APU_ACX_CONN_CG_CLR);
		dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
			readl(papw->regs[acx_base] + APU_ACX_CONN_CG_CON));
		dev_info(dev, "%s ACX%d before Spare RG 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + 0x3C2B0),
			readl(papw->regs[acx_base] + 0x3C2B0));
		/* access spare RG */
		apu_writel(0x12345678, papw->regs[acx_base] + 0x3C2B0);
		dev_info(dev, "%s ACX%d after Spare RG 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + 0x3C2B0),
			readl(papw->regs[acx_base] + 0x3C2B0));
	} else {
		dev_info(dev, "%s ACX%d APU_NCX_CONN_CG_CON 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + APU_NCX_CONN_CG_CON),
			readl(papw->regs[acx_base] + APU_NCX_CONN_CG_CON));
		/* clear acx0 CGs */
		apu_writel(0xFFFFFFFF, papw->regs[acx_base] + APU_NCX_CONN_CG_CLR);
		dev_info(dev, "%s ACX%d APU_NCX_CONN_CG_CON 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + APU_NCX_CONN_CG_CON),
			readl(papw->regs[acx_base] + APU_NCX_CONN_CG_CON));
		dev_info(dev, "%s ACX%d before Spare RG 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + 0x3F2B0),
			readl(papw->regs[acx_base] + 0x3F2B0));
		/* access spare RG */
		apu_writel(0x12345678, papw->regs[acx_base] + 0x3F2B0);
		dev_info(dev, "%s ACX%d after Spare RG 0x%x = 0x%x\n",
			__func__, acx_id,
			(u32)(papw->phy_addr[acx_base] + 0x3F2B0),
			readl(papw->regs[acx_base] + 0x3F2B0));
	}

out:
	return ret;
}

static int __apu_off_rpc_acx(struct device *dev, enum t_acx_id acx_id)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;
	uint32_t rpc_status;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else if (acx_id == ACX1) {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	} else if (acx_id == ACX2) {
		rpc_lite_base = apu_acx2_rpc_lite;
		acx_base = apu_acx2;
	} else {
		return -ENODEV;
	}

	rpc_status = apu_readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY);
	/* if ACX0 alread off, just return */
	if (!(rpc_status & 0x1UL))
		goto out;
	/* Clear wakeup signal */
	apu_clearl((0x1 << 0), papw->regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret = readl_relaxed_poll_timeout_atomic(
			(papw->regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, !(val & 0x1UL), 50, 10000);
	if (ret)
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);

	dev_info(dev, "%s ACX%d APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));
	dev_info(dev, "%s ACX%d APU_ACX_CONN_CG_CON 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[acx_base] + APU_ACX_CONN_CG_CON),
		readl(papw->regs[acx_base] + APU_ACX_CONN_CG_CON));

out:
	return ret;
}

static int __apu_pwr_ctl_acx_engines(struct device *dev,
		enum t_acx_id acx_id, enum t_dev_id dev_id, int pwron)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_lite_base;
	enum apupw_reg acx_base;
	uint32_t dev_mtcmos_ctl, dev_cg_con, dev_cg_clr;
	uint32_t dev_mtcmos_chk;
	uint32_t last_power_status;

	if (acx_id == ACX0) {
		rpc_lite_base = apu_acx0_rpc_lite;
		acx_base = apu_acx0;
	} else if (acx_id == ACX1) {
		rpc_lite_base = apu_acx1_rpc_lite;
		acx_base = apu_acx1;
	} else if (acx_id == ACX2) {
		rpc_lite_base = apu_acx2_rpc_lite;
		acx_base = apu_acx2;
	} else {
		return -ENODEV;
	}

	switch (dev_id) {
	case VPU0:
		dev_mtcmos_ctl = (0x1 << 4); //bit[4]: 1(on), 0(off)
		dev_mtcmos_chk = 0x10UL; //bit[4]: 1(on complete), 0(off complete)
		dev_cg_con = APU_ACX_MVPU_CG_CON;
		dev_cg_clr = APU_ACX_MVPU_CG_CLR;
		break;
	case DLA0:
		if (acx_id == ACX2) {
			dev_mtcmos_ctl = (0x1 << 7); //bit[7]
			dev_mtcmos_chk = 0x80UL; //bit[7]
		} else {
			dev_mtcmos_ctl = (0x1 << 6); //bit[6]
			dev_mtcmos_chk = 0x40UL; //bit[6]
		}
		dev_cg_con = APU_ACX_MDLA0_CG_CON;
		dev_cg_clr = APU_ACX_MDLA0_CG_CLR;
		break;
	default:
		goto out;
	}

	if (pwron) {
		last_power_status = apu_readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY);
		dev_mtcmos_ctl = last_power_status | dev_mtcmos_ctl;
		dev_mtcmos_chk = last_power_status | dev_mtcmos_chk;
		dev_info(dev, "%s ctl p1:%d p2:%d p3:0x%x p4:0x%x p5:0x%x p6:0x%x\n",
			__func__, rpc_lite_base, acx_base,
			dev_mtcmos_ctl, dev_mtcmos_chk, dev_cg_con, dev_cg_clr);
		/* config acx rpc lite */
		apu_writel(dev_mtcmos_ctl,
				papw->regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
		ret = readl_relaxed_poll_timeout_atomic(
				(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
				val, (val & dev_mtcmos_chk) == dev_mtcmos_chk, 50, 200);
		if (ret) {
			pr_info("%s on acx%d_rpc 0x%x fail, ret %d\n",
					__func__, acx_id, dev_mtcmos_ctl, ret);
			goto out;
		}
		dev_info(dev, "%s before engine on ACX%d dev%d CG_CON 0x%x = 0x%x\n",
			__func__, acx_id, dev_id,
			(u32)(papw->phy_addr[acx_base] + dev_cg_con),
			readl(papw->regs[acx_base] + dev_cg_con));
		apu_writel(0xFFFFFFFF, papw->regs[acx_base] + dev_cg_clr);
	} else {
		dev_info(dev, "%s ctl p1:%d p2:%d p3:0x%x p4:0x%x p5:0x%x p6:0x%x\n",
			__func__, rpc_lite_base, acx_base,
			dev_mtcmos_ctl, dev_mtcmos_chk, dev_cg_con, dev_cg_clr);
		/* config acx rpc lite */
		apu_clearl(dev_mtcmos_ctl,
				papw->regs[rpc_lite_base] + APU_RPC_SW_FIFO_WE);
		ret = readl_relaxed_poll_timeout_atomic(
				(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
				val, (val & dev_mtcmos_chk) != dev_mtcmos_chk, 50, 200);
		if (ret) {
			pr_info("%s off acx%d_rpc 0x%x fail, ret %d\n",
					__func__, acx_id, dev_mtcmos_ctl, ret);
			goto out;
		}
	}

	dev_info(dev, "%s ACX%d %s APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, acx_id, pwron ? "on" : "off",
		(u32)(papw->phy_addr[rpc_lite_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_lite_base] + APU_RPC_INTF_PWR_RDY));
	dev_info(dev, "%s ACX%d dev%d CG_CON 0x%x = 0x%x\n",
		__func__, acx_id, dev_id,
		(u32)(papw->phy_addr[acx_base] + dev_cg_con),
		readl(papw->regs[acx_base] + dev_cg_con));
	dev_info(dev, "%s ACX%d MVPU0 before Spare RG 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[acx_base] + 0x2B190),
		readl(papw->regs[acx_base] + 0x2B190));
	/* access spare RG */
	apu_writel(0x12345678, papw->regs[acx_base] + 0x2B190);
	dev_info(dev, "%s ACX%d MVPU0 after Spare RG 0x%x = 0x%x\n",
		__func__, acx_id,
		(u32)(papw->phy_addr[acx_base] + 0x2B190),
		readl(papw->regs[acx_base] + 0x2B190));
out:
	return ret;
}

static int __apu_pwr_ctl_rcx_engines(struct device *dev,
		enum t_dev_id dev_id, int pwron)
{
	int ret = 0, val = 0;
	enum apupw_reg rpc_mdla_base;
	enum apupw_reg rcx_dla_base;
	uint32_t dev_mtcmos_ctl, dev_cg_con, dev_cg_clr;
	uint32_t dev_mtcmos_chk;

	if (dev_id == DLA0) {
		rpc_mdla_base = apu_rpctop_mdla;
		rcx_dla_base = apu_rcx_dla;
		dev_mtcmos_ctl = (0x1 << 0); //bit[0]
		dev_mtcmos_chk = 0x1UL; //bit[0]
		dev_cg_con = APU_RCX_MDLA0_CG_CON;
		dev_cg_clr = APU_RCX_MDLA0_CG_CLR;
	} else {
		return -ENODEV;
	}

	dev_info(dev, "%s ctl p1:%d p2:%d p3:0x%x p4:0x%x p5:0x%x p6:0x%x\n",
		__func__, rpc_mdla_base, rcx_dla_base,
		dev_mtcmos_ctl, dev_mtcmos_chk, dev_cg_con, dev_cg_clr);

	if (pwron) {
		/* config rpc mdla */
		apu_setl(dev_mtcmos_ctl,
				papw->regs[rpc_mdla_base] + APU_RPC_SW_FIFO_WE);
		ret = readl_relaxed_poll_timeout_atomic(
				(papw->regs[rpc_mdla_base] + APU_RPC_INTF_PWR_RDY),
				val, (val & dev_mtcmos_chk) == dev_mtcmos_chk, 50, 200);
		if (ret) {
			pr_info("%s on rpc_mdla 0x%x fail, ret %d\n",
					__func__, dev_mtcmos_ctl, ret);
			goto out;
		}
		dev_info(dev, "%s before engine on RCX dev%d CG_CON 0x%x = 0x%x\n",
			__func__, dev_id,
			(u32)(papw->phy_addr[rcx_dla_base] + dev_cg_con),
			readl(papw->regs[rcx_dla_base] + dev_cg_con));
		apu_writel(0xFFFFFFFF, papw->regs[rcx_dla_base] + dev_cg_clr);
	} else {
		/* config rpc mdla */
		apu_clearl(dev_mtcmos_ctl,
				papw->regs[rpc_mdla_base] + APU_RPC_SW_FIFO_WE);
		ret = readl_relaxed_poll_timeout_atomic(
				(papw->regs[rpc_mdla_base] + APU_RPC_INTF_PWR_RDY),
				val, (val & dev_mtcmos_chk) != dev_mtcmos_chk, 50, 200);
		if (ret) {
			pr_info("%s off rpc_mdla 0x%x fail, ret %d\n",
					__func__, dev_mtcmos_ctl, ret);
			goto out;
		}
	}

	dev_info(dev, "%s RCX %s APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
		__func__, pwron ? "on" : "off",
		(u32)(papw->phy_addr[rpc_mdla_base] + APU_RPC_INTF_PWR_RDY),
		readl(papw->regs[rpc_mdla_base] + APU_RPC_INTF_PWR_RDY));
	dev_info(dev, "%s RCX dev%d CG_CON 0x%x = 0x%x\n",
		__func__, dev_id,
		(u32)(papw->phy_addr[rcx_dla_base] + dev_cg_con),
		readl(papw->regs[rcx_dla_base] + dev_cg_con));
	dev_info(dev, "%s RCX dev%d before Spare RG 0x%x = 0x%x\n",
		__func__, dev_id,
		(u32)(papw->phy_addr[rcx_dla_base] + 0x00E0),
		readl(papw->regs[rcx_dla_base] + 0x00E0));
	/* access spare RG */
	apu_writel(0x12345678, papw->regs[rcx_dla_base] + 0x00E0);
	dev_info(dev, "%s RCX dev%d after Spare RG 0x%x = 0x%x\n",
		__func__, dev_id,
		(u32)(papw->phy_addr[rcx_dla_base] + 0x00E0),
		readl(papw->regs[rcx_dla_base] + 0x00E0));
out:
	return ret;
}

static void __apu_aoc_init(void)
{
	pr_info("AOC init %s %d ++\n", __func__, __LINE__);
	/* 1. Manually disable Buck els enable @SOC, vapu_ext_buck_iso */
	if (papw->env == AO) {
		apu_setl((0x1 << 4), papw->regs[sys_spm] + 0xF6C);
		apu_clearl((0x1 << 1), papw->regs[sys_spm] + 0x414);
	}
	/*
	 * 2. Vsram AO clock enable
	 */
	apu_writel(0x00000001, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_CONFIG);
	udelay(1);
	/*
	 * 3. keep AOC control from vsram ao register bit[9] bit[14] =1, bit[12] = 1
	 */
	apu_setl(1 << 9, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	apu_setl(1 << 12, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	apu_setl(1 << 14, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	/* ---------------------------------------------------------------*/
	apu_clearl(1 << 9, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	apu_clearl(1 << 12, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	apu_clearl(1 << 14, papw->regs[apu_ao_ctl] + APUSYS_AO_SRAM_SET);
	udelay(1);
	// 4. Roll back to APU Buck on stage
	//  The following setting need to in order
	//  and wait 1uS before setup next control signal
	// APU_BUCK_ELS_EN
	apu_writel(0x00000800, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	// APU_BUCK_RST_B
	apu_writel(0x00001000, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	// APU_BUCK_PROT_REQ
	apu_writel(0x00008000, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	// SRAM_AOC_ISO
	apu_writel(0x00000080, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	/* DPSW_AOC_ISO */
	apu_writel(1 << 1, papw->regs[apu_rpc] + APU_RPC_HW_CON1);
	udelay(1);
	/* PLL_AOC_ISO_EN */
	apu_writel(1 << 9, papw->regs[apu_rpc] + APU_RPC_HW_CON);
	udelay(1);
	pr_info("AOC init %s %d --\n", __func__, __LINE__);
}

static int init_hw_setting(struct device *dev)
{
	if (papw->env == LK2) {
		#ifdef BYPASS_CE_ENABLE
		__apu_ce_init(dev);
		#endif
		return 0;
	}

	__apu_aoc_init();
	__apu_pcu_init();
	__apu_rpc_init();

	if (papw->env != FPGA) {
		__apu_rpclite_init_all();
	} else {
		if (fpga_type == 1)
			__apu_rpclite_init(ACX0);
	}
	__apu_rpc_mdla_init();
	/*
	 * 1. clear all are entries and enable cores
	 * 2. if use ce_rpc, set up it
	 */
	__apu_are_init(dev);

	if (papw->env != FPGA) {
		__apu_pll_init();
		__apu_acc_init();
	}
	#ifdef ENABLE_HW_VOTER
	__apu_hw_voter_init();
	#endif
	__apu_buck_off_cfg();

	return 0;
}

int mt6989_all_on(struct platform_device *pdev, struct apu_power *g_papw)
{
	papw = g_papw;

	if (papw->env == AO || papw->env == LK2)
		init_plat_pwr_res(pdev);
	init_hw_setting(&pdev->dev);

	/* wake up RCX */
	if (__apu_wake_rpc_rcx(&pdev->dev)) {
		aputop_dump_reg(apu_pll, 0xa0c, 0x10);
		/* dump RCX_AO content */
		aputop_dump_reg(apu_are, 0,
			(0x40 + 4*ARE_ENTRIES(RCX_AO_BEGIN, RCX_AO_END)));
		check_if_rpc_alive();
		return -EIO;
	}

	aputop_dump_reg(apu_rpc, 0x0, 0x50);

	if (papw->env == AO) {
		/* wake up ACX*/
		__apu_wake_rpc_acx(&pdev->dev, ACX0);
		__apu_wake_rpc_acx(&pdev->dev, ACX1);
		__apu_wake_rpc_acx(&pdev->dev, ACX2);
		/* wake up Engines */
		__apu_engine_acc_on();
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 1);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX2, DLA0, 1);
		__apu_pwr_ctl_rcx_engines(&pdev->dev, DLA0, 1);
	} else {
		switch (fpga_type) {
		default:
		case 0: // do not power on
			pr_info("%s only RCX power on\n", __func__);
			break;
		case 1: //1.RCX MDLA0 + ACX0 MVPU
			__apu_wake_rpc_acx(&pdev->dev, ACX0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 1);
			__apu_pwr_ctl_rcx_engines(&pdev->dev, DLA0, 1);
			break;
		case 2: //2.RCX MDLA0 + ACX0 MDLA0
			__apu_wake_rpc_acx(&pdev->dev, ACX0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 1);
			__apu_pwr_ctl_rcx_engines(&pdev->dev, DLA0, 1);
			break;
		}
	}

	return 0;
}

void mt6989_all_off(struct platform_device *pdev)
{
	if (papw->env == AO) {
		/* turn off Engines */
		__apu_pwr_ctl_rcx_engines(&pdev->dev, DLA0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, DLA0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX1, VPU0, 0);
		__apu_pwr_ctl_acx_engines(&pdev->dev, ACX2, DLA0, 0);
		/* turn off ACX */
		__apu_off_rpc_acx(&pdev->dev, ACX0);
		__apu_off_rpc_acx(&pdev->dev, ACX1);
		__apu_off_rpc_acx(&pdev->dev, ACX2);
	} else {
		switch (fpga_type) {
		default:
		case 0: // do not power on
			pr_info("%s bypass pre-power-ON\n", __func__);
			__apu_hw_voter_disable();
			break;
		case 1: //1.RCX MDLA0 + ACX0 MVPU
			__apu_pwr_ctl_rcx_engines(&pdev->dev, DLA0, 0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, VPU0, 0);
			__apu_off_rpc_acx(&pdev->dev, ACX0);
			break;
		case 2: //2.RCX MDLA0 + ACX0 MDLA0
			__apu_pwr_ctl_rcx_engines(&pdev->dev, DLA0, 0);
			__apu_pwr_ctl_acx_engines(&pdev->dev, ACX0, DLA0, 0);
			__apu_off_rpc_acx(&pdev->dev, ACX0);
			break;
		}
	}

	/* turn off RCX */
	__apu_off_rpc_rcx(&pdev->dev);
	pm_runtime_put_sync(&pdev->dev);

	if (papw->env == AO)
		destroy_plat_pwr_res();
}
