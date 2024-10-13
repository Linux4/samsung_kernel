/*
 * phy-exynos-usbdp-gen2-v2.c
 *
 *  Created on: 2018. 9. 5.
 *      Author: daeman.ko
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/delay.h>

#include "phy-samsung-usb-cal.h"

#include "phy-exynos-usb3p1-reg.h"
#include "phy-exynos-usbdp-gen2-v2.h"
#include "phy-exynos-usbdp-gen2-v2-reg.h"
#include "phy-exynos-usbdp-gen2-v2-reg-pcs.h"

extern struct exynos_chipid_info exynos_soc_info;
static inline u32 mach_get_evt(void)
{
	return exynos_soc_info.main_rev;
}

static inline u32 mach_get_subrev(void)
{
	return exynos_soc_info.sub_rev;
}

#if defined(USBDP_GEN2_DBG)
static inline u32 usbdp_cal_reg_rd(void *addr)
{
	u32 reg;

	reg = readl(addr);
	print_log("[USB/DP] Rd addr = 0x%08x\t\tdata = 0x%08x", addr, reg);
	return reg;
}

static inline void usbdp_cal_reg_wr(u32 val, void *addr)
{
	print_log("[USB/DP] Wr addr = 0x%08x\t\tdata = 0x%08x", addr, val);
	writel(val, addr);
}
#else
#define usbdp_cal_reg_rd(addr)		readl(addr)
#define usbdp_cal_reg_wr(val, addr)	writel(val, addr)
#endif

#if defined(USBDP_GEN2_DBG_OFFSET_CAL_CODE)
#define usbdp_print_offset_cal_code(...)	printk(__VA_ARGS__)
#else
#define usbdp_print_offset_cal_code(...)
#endif

void phy_exynos_usbdp_g2_v2_tune_each(struct exynos_usbphy_info *info, char *name, u32 val)
{
	void __iomem *regs_base = info->pma_base;
	void __iomem *pcs_base = info->pcs_base;
	void __iomem *link_base = info->link_base;
	u32 reg = 0;

	if (!name)
		return;

	if (val == -1)
		return;

	/*
	RX Squelch Detect Threshold Control
	SQTH
	Gne2
	0x0DF8 [3:0]
	0x1DF8 [3:0]
	Gne1
	0x0DF8 [7:4]
	0x1DF8 [7:4]
			0000	0mV
			0011	75mV
			0100	90mV
			1000	105mV
			1111	255mV

	 */
	if (!strcmp(name, "ssrx_sqhs_th_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037E);
		reg &= USBDP_TRSV_REG037E_LN0_RX_SQHS_TH_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG037E_LN0_RX_SQHS_TH_CTRL_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077E);
		reg &= USBDP_TRSV_REG077E_LN2_RX_SQHS_TH_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG077E_LN2_RX_SQHS_TH_CTRL_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077E);
	} else if (!strcmp(name, "ssrx_sqhs_th_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037E);
		reg &= USBDP_TRSV_REG037E_LN0_RX_SQHS_TH_CTRL_SP_CLR;
		reg |= USBDP_TRSV_REG037E_LN0_RX_SQHS_TH_CTRL_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077E);
		reg &= USBDP_TRSV_REG077E_LN2_RX_SQHS_TH_CTRL_SP_CLR;
		reg |= USBDP_TRSV_REG077E_LN2_RX_SQHS_TH_CTRL_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077E);
	}

	/*
	LFPS Detect Threshold Control
	LFPS RX
	0x0A0C [3:1]
	0x1A0C [3:1]
			000	60mV
			001	90mV
			010	130mV
			011	160mV
			111	280mV
	 */
	else if (!strcmp(name, "ssrx_lfps_th")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0283);
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_TH_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0283);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0683);
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_TH_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0683);
	}

	/*
	Adapation logic off
	0x0AD0=00
	0x1AD0=00

	DFE 1tap Adapation
	0x0AD0=03
	0x1AD0=03

	DFE all-tap Adapation (Recommand)
	0x0AD0=3F
	0x1AD0=3F

	DFE,CTLE Adapation
	0x0AD0=FF
	0x1AD0=FF
	 */
	else if (!strcmp(name, "ssrx_adap_coef_sel")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02B4);
		reg &= USBDP_TRSV_REG02B4_LN0_RX_SSLMS_ADAP_COEF_SEL__7_0_CLR;
		reg |= USBDP_TRSV_REG02B4_LN0_RX_SSLMS_ADAP_COEF_SEL__7_0_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06B4);
		reg &= USBDP_TRSV_REG06B4_LN2_RX_SSLMS_ADAP_COEF_SEL__7_0_CLR;
		reg |= USBDP_TRSV_REG06B4_LN2_RX_SSLMS_ADAP_COEF_SEL__7_0_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06B4);
	}

	/*
	RX PEQ Control (Mid freq boost)
	Gen1
	0x0D94[0] = 1 : Forcing, 0 : adaptation
	0x1D94[0]
	Gen2
	0x0DF0[5] = 1 :  Forcing, 0 : adaptation
	0x1DF0[5]
		Gen1
		0x0D9C [2:0]
		0x1D9C [2:0]
		Gen2
		0x0DF0 [2:0]
		0x1DF0 [2:0]
			000	0dB
			∙∙∙
			100	5dB
			∙∙∙
			111	6dB (Max)
	 */
	else if (!strcmp(name, "ssrx_mf_eq_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0365);
		reg &= USBDP_TRSV_REG0365_LN0_OVRD_RX_SSLMS_MF_BIN_SP_CLR;
		reg |= USBDP_TRSV_REG0365_LN0_OVRD_RX_SSLMS_MF_BIN_SP_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0365);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0367);
		reg &= USBDP_TRSV_REG0367_LN0_RX_SSLMS_MF_BIN_SP__7_0_CLR;
		reg |= USBDP_TRSV_REG0367_LN0_RX_SSLMS_MF_BIN_SP__7_0_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0367);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0765);
		reg &= USBDP_TRSV_REG0765_LN2_OVRD_RX_SSLMS_MF_BIN_SP_CLR;
		reg |= USBDP_TRSV_REG0765_LN2_OVRD_RX_SSLMS_MF_BIN_SP_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0765);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0767);
		reg &= USBDP_TRSV_REG0767_LN2_RX_SSLMS_MF_BIN_SP__7_0_CLR;
		reg |= USBDP_TRSV_REG0767_LN2_RX_SSLMS_MF_BIN_SP__7_0_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0767);
	} else if (!strcmp(name, "ssrx_mf_eq_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037C);
		reg &= USBDP_TRSV_REG037C_LN0_OVRD_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037C_LN0_OVRD_RX_SSLMS_MF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG037C_LN0_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037C_LN0_RX_SSLMS_MF_BIN_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077C);
		reg &= USBDP_TRSV_REG077C_LN2_OVRD_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077C_LN2_OVRD_RX_SSLMS_MF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG077C_LN2_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077C_LN2_RX_SSLMS_MF_BIN_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077C);
	}

	/*
	RX HF EQ Control (High freq boost)
	Gen1
	0x0D88[0] = 1 :  Forcing, 0 : adapation
	0x1D88[0]
	Gen2
	0x0DEC[5] = 1 :  Forcing, 0 : adapation
	0x1DEC[5]
		Gen1
		0x0D90 [4:0]
		0x1D90 [4:0]
		Gen2
		0x0DEC [4:0]
		0x1DEC [4:0]
				00000	0dB
				...
				01100	15dB
				...
				10000	20dB (Max)
	 */
	else if (!strcmp(name, "ssrx_hf_eq_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0362);
		reg &= USBDP_TRSV_REG0362_LN0_OVRD_RX_SSLMS_HF_BIN_SP_CLR;
		reg |= USBDP_TRSV_REG0362_LN0_OVRD_RX_SSLMS_HF_BIN_SP_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0362);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0364);
		reg &= USBDP_TRSV_REG0364_LN0_RX_SSLMS_HF_BIN_SP__7_0_CLR;
		reg |= USBDP_TRSV_REG0364_LN0_RX_SSLMS_HF_BIN_SP__7_0_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0364);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0762);
		reg &= USBDP_TRSV_REG0762_LN2_OVRD_RX_SSLMS_HF_BIN_SP_CLR;
		reg |= USBDP_TRSV_REG0762_LN2_OVRD_RX_SSLMS_HF_BIN_SP_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0762);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0764);
		reg &= USBDP_TRSV_REG0764_LN2_RX_SSLMS_HF_BIN_SP__7_0_CLR;
		reg |= USBDP_TRSV_REG0764_LN2_RX_SSLMS_HF_BIN_SP__7_0_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0764);
	} else if (!strcmp(name, "ssrx_hf_eq_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037B);
		reg &= USBDP_TRSV_REG037B_LN0_OVRD_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037B_LN0_OVRD_RX_SSLMS_HF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG037B_LN0_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037B_LN0_RX_SSLMS_HF_BIN_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077B);
		reg &= USBDP_TRSV_REG077B_LN2_OVRD_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077B_LN2_OVRD_RX_SSLMS_HF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG077B_LN2_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077B_LN2_RX_SSLMS_HF_BIN_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077B);
	}

	/*
	DFE 1 tap  Control
	0x0AD0[1] = 0 : Tune, 1 : adaptation
	0x1AD0[1]
		Gen1
		0x0CD0 [6:0]
		0x0D00 [6:0]
		0x1CD0 [6:0]
		0x1D00 [6:0]
		Gen2
		0x0CE8 [6:0]
		0x0D18 [6:0]
		0x1CE8 [6:0]
		0x1D18 [6:0]
				00	0mV
				...
				04	8mV
				...
				7F	254mV
	 */
	else if (!strcmp(name, "ssrx_dfe_1tap_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0334);
		reg &= USBDP_TRSV_REG0334_LN0_RX_SSLMS_C1_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0334_LN0_RX_SSLMS_C1_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0334);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0340);
		reg &= USBDP_TRSV_REG0340_LN0_RX_SSLMS_C1_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0340_LN0_RX_SSLMS_C1_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0340);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0734);
		reg &= USBDP_TRSV_REG0734_LN2_RX_SSLMS_C1_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0734_LN2_RX_SSLMS_C1_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0734);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0740);
		reg &= USBDP_TRSV_REG0740_LN2_RX_SSLMS_C1_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0740_LN2_RX_SSLMS_C1_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0740);
	} else if (!strcmp(name, "ssrx_dfe_1tap_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG033A);
		reg &= USBDP_TRSV_REG033A_LN0_RX_SSLMS_C1_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG033A_LN0_RX_SSLMS_C1_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG033A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0346);
		reg &= USBDP_TRSV_REG0346_LN0_RX_SSLMS_C1_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0346_LN0_RX_SSLMS_C1_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0346);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG073A);
		reg &= USBDP_TRSV_REG073A_LN2_RX_SSLMS_C1_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG073A_LN2_RX_SSLMS_C1_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG073A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0746);
		reg &= USBDP_TRSV_REG0746_LN2_RX_SSLMS_C1_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0746_LN2_RX_SSLMS_C1_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0746);
	}

	/*
	DFE 2 tap  Control
	0x0AD0[2] = 0 : Tune, 1 : adaptation
	0x1AD0[2]
		Gen1
		0x0CD4 [6:0]
		0x0D04 [6:0]
		0x1CD4 [6:0]
		0x1D04 [6:0]
		Gen2
		0x0CEC [6:0]
		0x0D1C [6:0]
		0x1CEC [6:0]
		0x1D1C [6:0]
				40	-240mV (2's complement)
				7F	-3.75mV
				00	0mV
				01	3.75mV
				3F	240mV
	 */
	else if (!strcmp(name, "ssrx_dfe_2tap_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0335);
		reg &= USBDP_TRSV_REG0335_LN0_RX_SSLMS_C2_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0335_LN0_RX_SSLMS_C2_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0335);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0341);
		reg &= USBDP_TRSV_REG0341_LN0_RX_SSLMS_C2_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0341_LN0_RX_SSLMS_C2_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0341);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0735);
		reg &= USBDP_TRSV_REG0735_LN2_RX_SSLMS_C2_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0735_LN2_RX_SSLMS_C2_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0735);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0741);
		reg &= USBDP_TRSV_REG0741_LN2_RX_SSLMS_C2_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0741_LN2_RX_SSLMS_C2_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0741);
	} else if (!strcmp(name, "ssrx_dfe_2tap_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG033B);
		reg &= USBDP_TRSV_REG033B_LN0_RX_SSLMS_C2_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG033B_LN0_RX_SSLMS_C2_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG033B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0347);
		reg &= USBDP_TRSV_REG0347_LN0_RX_SSLMS_C2_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0347_LN0_RX_SSLMS_C2_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0347);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG073B);
		reg &= USBDP_TRSV_REG073B_LN2_RX_SSLMS_C2_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG073B_LN2_RX_SSLMS_C2_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG073B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0747);
		reg &= USBDP_TRSV_REG0747_LN2_RX_SSLMS_C2_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0747_LN2_RX_SSLMS_C2_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0747);
	}

	/*
	DFE 3 tap  Control
	0x0AD0[3] = 0 : Tune, 1 : adapation
	0x1AD0[3]
		Gen1
		0x0CD8 [6:0]
		0x0D08 [6:0]
		0x1CD8 [6:0]
		0x1D08 [6:0]
		Gen2
		0x0CF0 [6:0]
		0x0D20 [6:0]
		0x1CF0 [6:0]
		0x1D20 [6:0]
				40	-240mV (2's complement)
				7F	-3.75mV
				00	0mV
				01	3.75mV
				3F	240mV
	 */
	else if (!strcmp(name, "ssrx_dfe_3tap_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0336);
		reg &= USBDP_TRSV_REG0336_LN0_RX_SSLMS_C3_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0336_LN0_RX_SSLMS_C3_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0336);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0342);
		reg &= USBDP_TRSV_REG0342_LN0_RX_SSLMS_C3_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0342_LN0_RX_SSLMS_C3_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0342);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0736);
		reg &= USBDP_TRSV_REG0736_LN2_RX_SSLMS_C3_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0736_LN2_RX_SSLMS_C3_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0736);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0742);
		reg &= USBDP_TRSV_REG0742_LN2_RX_SSLMS_C3_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0742_LN2_RX_SSLMS_C3_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0742);
	} else if (!strcmp(name, "ssrx_dfe_3tap_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG033C);
		reg &= USBDP_TRSV_REG033C_LN0_RX_SSLMS_C3_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG033C_LN0_RX_SSLMS_C3_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG033C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0348);
		reg &= USBDP_TRSV_REG0348_LN0_RX_SSLMS_C3_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0348_LN0_RX_SSLMS_C3_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0348);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG073C);
		reg &= USBDP_TRSV_REG073C_LN2_RX_SSLMS_C3_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG073C_LN2_RX_SSLMS_C3_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG073C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0748);
		reg &= USBDP_TRSV_REG0748_LN2_RX_SSLMS_C3_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0748_LN2_RX_SSLMS_C3_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0748);
	}

	/*
	DFE 4 tap  Control
	0x0AD0[4] = 0 : Tune, 1 : adapation
	0x1AD0[4]
		Gen1
		0x0CDC [5:0]
		0x0D0C [5:0]
		0x1CDC [5:0]
		0x1D0C [5:0]
		Gen2
		0x0CF4 [5:0]
		0x0D24 [5:0]
		0x1CF4 [5:0]
		0x1D24 [5:0]
				20	-120mV (2's complement)
				3F	-3.75mV
				00	0mV
				01	3.75mV
				1F	120mV
	 */
	else if (!strcmp(name, "ssrx_dfe_4tap_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0337);
		reg &= USBDP_TRSV_REG0337_LN0_RX_SSLMS_C4_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0337_LN0_RX_SSLMS_C4_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0337);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0343);
		reg &= USBDP_TRSV_REG0343_LN0_RX_SSLMS_C4_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0343_LN0_RX_SSLMS_C4_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0343);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0737);
		reg &= USBDP_TRSV_REG0737_LN2_RX_SSLMS_C4_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0737_LN2_RX_SSLMS_C4_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0737);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0743);
		reg &= USBDP_TRSV_REG0743_LN2_RX_SSLMS_C4_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0743_LN2_RX_SSLMS_C4_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0743);
	} else if (!strcmp(name, "ssrx_dfe_4tap_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG033D);
		reg &= USBDP_TRSV_REG033D_LN0_RX_SSLMS_C4_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG033D_LN0_RX_SSLMS_C4_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG033D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0349);
		reg &= USBDP_TRSV_REG0349_LN0_RX_SSLMS_C4_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0349_LN0_RX_SSLMS_C4_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0349);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG073D);
		reg &= USBDP_TRSV_REG073D_LN2_RX_SSLMS_C4_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG073D_LN2_RX_SSLMS_C4_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG073D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0749);
		reg &= USBDP_TRSV_REG0749_LN2_RX_SSLMS_C4_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0749_LN2_RX_SSLMS_C4_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0749);
	}

	/*
	DFE 5 tap  Control
	0x0AD0[5] = 0 : Tune, 1 : adaptation
	0x1AD0[5]
		Gen1
		0x0CE0 [5:0]
		0x0D10 [5:0]
		0x1CE0 [5:0]
		0x1D10 [5:0]
		Gen2
		0x0CF8 [5:0]
		0x0D28 [7:2]
		0x1CF8 [5:0]
		0x1D28 [7:2]
				20	-120mV (2's complement)
				3F	-3.75mV
				00	0mV
				01	3.75mV
				1F	120mV
	 */
	else if (!strcmp(name, "ssrx_dfe_5tap_ctrl_ss")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0338);
		reg &= USBDP_TRSV_REG0338_LN0_RX_SSLMS_C5_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0338_LN0_RX_SSLMS_C5_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0338);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0344);
		reg &= USBDP_TRSV_REG0344_LN0_RX_SSLMS_C5_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0344_LN0_RX_SSLMS_C5_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0344);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0738);
		reg &= USBDP_TRSV_REG0738_LN2_RX_SSLMS_C5_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0738_LN2_RX_SSLMS_C5_E_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0738);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0744);
		reg &= USBDP_TRSV_REG0744_LN2_RX_SSLMS_C5_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0744_LN2_RX_SSLMS_C5_O_INIT_SP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0744);
	} else if (!strcmp(name, "ssrx_dfe_5tap_ctrl_ssp")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG033E);
		reg &= USBDP_TRSV_REG033E_LN0_RX_SSLMS_C5_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG033E_LN0_RX_SSLMS_C5_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG033E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG034A);
		reg &= USBDP_TRSV_REG034A_LN0_RX_SSLMS_C5_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG034A_LN0_RX_SSLMS_C5_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG034A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG073E);
		reg &= USBDP_TRSV_REG073E_LN2_RX_SSLMS_C5_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG073E_LN2_RX_SSLMS_C5_E_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG073E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG074A);
		reg &= USBDP_TRSV_REG074A_LN2_RX_SSLMS_C5_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG074A_LN2_RX_SSLMS_C5_O_INIT_SSP_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG074A);
	}

	/*
	RX Termination
	0x0BB0 [1:0]: 1 : Tune, 0 : calibration
	0x1BB0 [1:0]
		0x0BB4 [7:4]
		0x1BB4 [7:4]
				0000	57 Ohm
				0001	57 Ohm
				0010	53 Ohm
				0011	44 Ohm
				1111	37 Ohm
	 */
	else if (!strcmp(name, "ssrx_term_cal")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02EC);
		reg &= USBDP_TRSV_REG02EC_LN0_RX_RCAL_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG02EC_LN0_RX_RCAL_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02EC);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02ED);
		reg &= USBDP_TRSV_REG02ED_LN0_RX_RTERM_CTRL_CLR;
		reg |= USBDP_TRSV_REG02ED_LN0_RX_RTERM_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02ED);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06EC);
		reg &= USBDP_TRSV_REG06EC_LN2_RX_RCAL_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG06EC_LN2_RX_RCAL_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06EC);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06ED);
		reg &= USBDP_TRSV_REG06ED_LN2_RX_RTERM_CTRL_CLR;
		reg |= USBDP_TRSV_REG06ED_LN2_RX_RTERM_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06ED);
	}

	/* Gen1 Tx DRIVER pre-shoot, de-emphasis, level ctrl
	 * [17:12] Deemphasis
	 * [11:6] Level
	 * [5:0] Preshoot
	 * */
	else if (!strcmp(name, "sstx_amp_ss")) {
		reg = usbdp_cal_reg_rd(pcs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
		reg &= ~(0x00FC0);
		reg |= val << 6;
		usbdp_cal_reg_wr(reg, pcs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
	}

	else if (!strcmp(name, "sstx_deemp_ss")) {
		reg = usbdp_cal_reg_rd(pcs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
		reg &= ~(0x3F000);
		reg |= val << 12;
		usbdp_cal_reg_wr(reg, pcs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
	}

	else if (!strcmp(name, "sstx_pre_shoot_ss")) {
		reg = usbdp_cal_reg_rd(pcs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
		reg &= ~(0x0003F);
		reg |= val;
		usbdp_cal_reg_wr(reg, pcs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
	}

	/* Gen2 Tx DRIVER pre-shoot, de-emphasis, level ctrl
	 * [17:12] Deemphasis
	 * [11:6] Level
	 * [5:0] Preshoot
	 * */
	else if (!strcmp(name, "sstx_amp_ssp")) {
		reg = usbdp_cal_reg_rd(pcs_base + EXYNOS_USBDP_PCS_LEQ_LOCAL_COEF);
		reg &= USBDP_PCS_LEQ_LOCAL_COEF_PMA_CENTER_COEF_CLR;
		reg |= USBDP_PCS_LEQ_LOCAL_COEF_PMA_CENTER_COEF_SET(val);
		usbdp_cal_reg_wr(reg, pcs_base + EXYNOS_USBDP_PCS_LEQ_LOCAL_COEF);
	}

	else if (!strcmp(name, "sstx_deemp_ssp")) {
		reg = usbdp_cal_reg_rd(link_base + USB31DRD_LINK_LCSR_TX_DEEMPH);
		reg &= ~(0x3F000);
		reg |= val << 12;
		usbdp_cal_reg_wr(reg, link_base + USB31DRD_LINK_LCSR_TX_DEEMPH);
	}

	else if (!strcmp(name, "sstx_pre_shoot_ssp")) {
		reg = usbdp_cal_reg_rd(link_base + USB31DRD_LINK_LCSR_TX_DEEMPH);
		reg &= ~(0x0003F);
		reg |= val;
		usbdp_cal_reg_wr(reg, link_base + USB31DRD_LINK_LCSR_TX_DEEMPH);
	}

	/*
	TX IDRV UP
	0x101C [7]: 1: Tune, 0 : From PnR logic
	0x201C [7]
		0x101C [6:4]
		0x201C [6:4]
				111	0.85V
				110	1V
				011	1.1V
				...
				000	1.2V ( Max )
	 */
	else if (!strcmp(name, "sstx_idrv_up")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0407);
		reg &= USBDP_TRSV_REG0407_OVRD_LN1_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_TRSV_REG0407_OVRD_LN1_TX_DRV_IDRV_IUP_CTRL_SET(1);
		reg &= USBDP_TRSV_REG0407_LN1_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_TRSV_REG0407_LN1_TX_DRV_IDRV_IUP_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0407);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0807);
		reg &= USBDP_TRSV_REG0807_OVRD_LN3_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_TRSV_REG0807_OVRD_LN3_TX_DRV_IDRV_IUP_CTRL_SET(1);
		reg &= USBDP_TRSV_REG0807_LN3_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_TRSV_REG0807_LN3_TX_DRV_IDRV_IUP_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0807);
	}

	/*
	TX IDRV UP for LFPS TX
	It is valid where
	0x101C [7]: 0x201C [7] = 0 (Default setting)
		0x0408 [6:4]
				111	0.85V
				110	1V
				011	1.1V
				...
				000	1.2V ( Max )
	 */
	else if (!strcmp(name, "sstx_lfps_idrv_up")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0102);
		reg &= USBDP_CMN_REG0102_TX_DRV_LFPS_MODE_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_CMN_REG0102_TX_DRV_LFPS_MODE_IDRV_IUP_CTRL_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0102);
	}

	/*
	TX UP Termination
	0x10B0 [3:2]: 1 : Tune, 0 : calibration
	0x20B0 [3:2]
		0x10B4 [7:4]
		0x20B4 [7:4]
				0000	57 Ohm
				0001	57 Ohm
				0010	53 Ohm
				0011	44 Ohm
				1111	37 Ohm
	 */
	else if (!strcmp(name, "sstx_up_term")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042C);
		reg &= USBDP_TRSV_REG042C_LN1_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG042C_LN1_TX_RCAL_UP_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042D);
		reg &= USBDP_TRSV_REG042D_LN1_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG042D_LN1_TX_RCAL_UP_CODE_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082C);
		reg &= USBDP_TRSV_REG082C_LN3_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG082C_LN3_TX_RCAL_UP_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082D);
		reg &= USBDP_TRSV_REG082D_LN3_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG082D_LN3_TX_RCAL_UP_CODE_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082D);
	}

	/*
	TX DN Termination
	0x10B0 [1:0]: 1 : Tune, 0 : calibration
	0x20B0 [1:0]
		0x10B4 [3:0]
		0x20B4 [3:0]
				0000	57 Ohm
				0001	57 Ohm
				0010	53 Ohm
				0011	44 Ohm
				1111	37 Ohm
	 */
	else if (!strcmp(name, "sstx_dn_term")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042C);
		reg &= USBDP_TRSV_REG042C_LN1_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG042C_LN1_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042D);
		reg &= USBDP_TRSV_REG042D_LN1_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG042D_LN1_TX_RCAL_DN_CODE_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082C);
		reg &= USBDP_TRSV_REG082C_LN3_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG082C_LN3_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082D);
		reg &= USBDP_TRSV_REG082D_LN3_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG082D_LN3_TX_RCAL_DN_CODE_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082D);
	}

	/* REXT ovrd */
	else if (!strcmp(name, "rext_ovrd")) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0006);
		reg |= USBDP_CMN_REG0006_OVRD_BIAS_ICAL_CODE_SET(1);
		reg &= USBDP_CMN_REG0006_BIAS_ICAL_CODE_CLR;
		reg |= USBDP_CMN_REG0006_BIAS_ICAL_CODE_SET(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0006);
	}
}

void phy_exynos_usbdp_g2_v2_tune(struct exynos_usbphy_info *info)
{
	u32 cnt = 0;

	if (!info) {
		return;
	}

	if (!info->tune_param) {
		return;
	}

	for (; info->tune_param[cnt].value != EXYNOS_USB_TUNE_LAST; cnt++) {
		char *para_name;
		int val;

		val = info->tune_param[cnt].value;
		if (val == -1) {
			continue;
		}
		para_name = info->tune_param[cnt].name;
		if (!para_name) {
			break;
		}
		phy_exynos_usbdp_g2_v2_tune_each(info, para_name, val);
	}
}

void phy_exynos_usbdp_g2_v2_set_dtb_mux(struct exynos_usbphy_info *info, int mux_val)
{
	// TODO
}

static void phy_exynos_usbdp_g2_v2_ctrl_pma_ready(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->ctrl_base;
	u32 reg;

	/* link pipe_clock selection to pclk of PMA */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_CLKRST);
	reg |= CLKRST_LINK_PCLK_SEL;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_CLKRST);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg |= PMA_REF_FREQ_SEL_SET(1);
	/* SFR reset */
	reg |= PMA_LOW_PWR;
	reg |= PMA_APB_SW_RST;
	/* reference clock 26MHz path from XTAL */
	reg &= ~PMA_ROPLL_REF_CLK_SEL_MASK;
	reg &= ~PMA_LCPLL_REF_CLK_SEL_MASK;
	/* PMA_POWER_OFF */
	reg |= PMA_TRSV_SW_RST;
	reg |= PMA_CMN_SW_RST;
	reg |= PMA_INIT_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);

	udelay(1);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_LOW_PWR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);

	/* APB enable */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_APB_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
}

static void phy_exynos_usbdp_g2_v2_aux_force_off(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0008);
	reg &= USBDP_CMN_REG0008_OVRD_AUX_EN_CLR;
	reg |= USBDP_CMN_REG0008_OVRD_AUX_EN_SET(1);
	reg &= USBDP_CMN_REG0008_AUX_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0008);
}

static void __maybe_unused phy_exynos_usbdp_g2_v2_pma_default_sfr_update(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	if (mach_get_evt() == 1) {
		/* base
		 * EVT1 ML4_DEV00
		 * TN_DIPD_USBDP_2020_EVT1_SFR.xls ver.10.8 */

		/* ========================================================================
		 * Common
		 */

		/* TX impedance setting
		0x0BBC	0x77	ln0_tx_rcal_up_code=7
						ln0_tx_rcal_dn_code=7
		0x10B4	0x77	ln1_tx_rcal_up_code=7
						ln1_tx_rcal_dn_code=7
		0x1BBC	0x77	ln2_tx_rcal_up_code=7
						ln2_tx_rcal_dn_code=7
		0x20B4	0x77	ln3_tx_rcal_up_code=7
						ln3_tx_rcal_dn_code=7
		0x10B0	0x05	tx_rcal_up_opt_code=1
						tx_rcal_dn_opt_code=1
		0x20B0	0x05	tx_rcal_up_opt_code=1
						tx_rcal_dn_opt_code=1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02EF);
		reg &= USBDP_TRSV_REG02EF_LN0_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG02EF_LN0_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG02EF_LN0_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG02EF_LN0_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02EF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042D);
		reg &= USBDP_TRSV_REG042D_LN1_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG042D_LN1_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG042D_LN1_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG042D_LN1_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06EF);
		reg &= USBDP_TRSV_REG06EF_LN2_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG06EF_LN2_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG06EF_LN2_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG06EF_LN2_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06EF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082D);
		reg &= USBDP_TRSV_REG082D_LN3_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG082D_LN3_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG082D_LN3_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG082D_LN3_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042C);
		reg &= USBDP_TRSV_REG042C_LN1_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG042C_LN1_TX_RCAL_UP_OPT_CODE_SET(1);
		reg &= USBDP_TRSV_REG042C_LN1_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG042C_LN1_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082C);
		reg &= USBDP_TRSV_REG082C_LN3_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG082C_LN3_TX_RCAL_UP_OPT_CODE_SET(1);
		reg &= USBDP_TRSV_REG082C_LN3_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG082C_LN3_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082C);

		/* turn off for power saving
		0x0298	0x18	ropll_cd_vreg_en = 0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00A6);
		reg &= USBDP_CMN_REG00A6_ROPLL_CD_VREG_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00A6);

		/* set C0~C1 of DFE during Offset calibration pll_lock_done watchdog enable
		0x0594	0x08	rx_sslms_wait_ofs_cal_done = 1
		0x0420	0x77	lcpll_afc_timeout_opt = 7
						ropll_afc_timeout_opt = 7
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0165);
		reg &= USBDP_CMN_REG0165_RX_SSLMS_WAIT_OFS_CAL_DONE_CLR;
		reg |= USBDP_CMN_REG0165_RX_SSLMS_WAIT_OFS_CAL_DONE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0165);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0108);
		reg &= USBDP_CMN_REG0108_LCPLL_AFC_TIMEOUT_OPT_CLR;
		reg |= USBDP_CMN_REG0108_LCPLL_AFC_TIMEOUT_OPT_SET(7);
		reg &= USBDP_CMN_REG0108_ROPLL_AFC_TIMEOUT_OPT_CLR;
		reg |= USBDP_CMN_REG0108_ROPLL_AFC_TIMEOUT_OPT_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0108);

		/* Common reset for 4lane tx
		0x0858	0x63	ovrd_ln0_tx_lane_rstn_sel=1
		0x1058	0x63	ovrd_ln1_tx_lane_rstn_sel=1
		0x1858	0x63	ovrd_ln2_tx_lane_rstn_sel=1
		0x2058	0x63	ovrd_ln3_tx_lane_rstn_sel=1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0216);
		reg &= USBDP_TRSV_REG0216_OVRD_LN0_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0216_OVRD_LN0_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0216);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0416);
		reg &= USBDP_TRSV_REG0416_OVRD_LN1_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0416_OVRD_LN1_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0416);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0616);
		reg &= USBDP_TRSV_REG0616_OVRD_LN2_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0616_OVRD_LN2_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0616);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0816);
		reg &= USBDP_TRSV_REG0816_OVRD_LN3_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0816_OVRD_LN3_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0816);

		/* when using reset value, ATB enable
		0x0894	0x00	ln0_ana_tx_reserved=0
		0x0A1C	0x00	ln0_ana_rx_reserved=0
		0x1094	0x00	ln1_ana_tx_reserved=0
		0x1894	0x00	ln2_ana_tx_reserved=0
		0x1A1C	0x00	ln2_ana_rx_reserved=0
		0x2094	0x00	ln3_ana_tx_reserved=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0225);
		reg &= USBDP_TRSV_REG0225_LN0_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0225);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0287);
		reg &= USBDP_TRSV_REG0287_LN0_ANA_RX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0287);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0425);
		reg &= USBDP_TRSV_REG0425_LN1_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0425);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0625);
		reg &= USBDP_TRSV_REG0625_LN2_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0625);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0687);
		reg &= USBDP_TRSV_REG0687_LN2_ANA_RX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0687);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0825);
		reg &= USBDP_TRSV_REG0825_LN3_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0825);

		/*
		 * Common
		 ========================================================================*/


		/* ========================================================================
		 * USB
		 */

		/* Set CDR lock delay to same value with PLL lock
		0x0324	0x27	cdr_lock_delay_code = 7
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00C9);
		reg &= USBDP_CMN_REG00C9_CDR_LOCK_DELAY_CODE_CLR;
		reg |= USBDP_CMN_REG00C9_CDR_LOCK_DELAY_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00C9);

		/* Can be fail to calibration by unknown data from RX_DP/DN during rate change
		0x0450	0x06	ofs_cal_rate_change_restart_en = 0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0114);
		reg &= USBDP_CMN_REG0114_OFS_CAL_RATE_CHANGE_RESTART_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0114);

		/* LCPLL default setting 변경 요청 from PLL designer
		0x0070	0x10	ana_lcpll_ana_lc_gm_comp_vref_sel =1
						ana_lcpll_ana_lc_vreg_i_ctrl=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG001C);
		reg &= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_GM_COMP_VREF_SEL_CLR;
		reg |= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_GM_COMP_VREF_SEL_SET(1);
		reg &= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_VREG_I_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG001C);

		/* AFE setting 최적화 for JTOL
		0x093C	0x28	ln0_rx_ctle_vga_i_ctrl_ssp=5
		0x0948	0x32	ln0_rx_ctle_vga_rl_ctrl_ssp=6
		0x0A60	0x0B	ln0_rx_sslms_hf_init_ssp=B
		0x0A78	0x05	ln0_rx_sslms_mf_init_ssp=5
		0x193C	0x28	ln2_rx_ctle_vga_i_ctrl_ssp=5
		0x1948	0x32	ln2_rx_ctle_vga_rl_ctrl_ssp=6
		0x1A60	0x0B	ln2_rx_sslms_hf_init_ssp=B
		0x1A78	0x05	ln2_rx_sslms_mf_init_ssp=5
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG024F);
		reg &= USBDP_TRSV_REG024F_LN0_RX_CTLE_VGA_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG024F_LN0_RX_CTLE_VGA_I_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG024F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0252);
		reg &= USBDP_TRSV_REG0252_LN0_RX_CTLE_VGA_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0252_LN0_RX_CTLE_VGA_RL_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0252);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG064F);
		reg &= USBDP_TRSV_REG064F_LN2_RX_CTLE_VGA_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG064F_LN2_RX_CTLE_VGA_I_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG064F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0652);
		reg &= USBDP_TRSV_REG0652_LN2_RX_CTLE_VGA_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0652_LN2_RX_CTLE_VGA_RL_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0652);

		/* rate change 시 RX_DP/DN으로 unknown data가 들어와 cal fail가능성 있음
		0x0450	0x06	ofs_cal_rate_change_restart_en = 0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0114);
		reg &= USBDP_CMN_REG0114_OFS_CAL_RATE_CHANGE_RESTART_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0114);

		/* Adapation done 안올라오는 문제 해결
		0x0C3C	0x05	ln0_rx_sslms_block_adap_en=1
		0x0AC8	0x3B	ln0_rx_sslms_settle_cycle=3
						ln0_rx_sslms_hysterisis=1
						ln0_rx_sslms_adap_tol=3
		0x1C3C	0x05	ln2_rx_sslms_block_adap_en=1
		0x1AC8	0x3B	ln0_rx_sslms_settle_cycle=3
						ln0_rx_sslms_hysterisis=1
						ln0_rx_sslms_adap_tol=3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG030F);
		reg &= USBDP_TRSV_REG030F_LN0_RX_SSLMS_BLOCK_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG030F_LN0_RX_SSLMS_BLOCK_ADAP_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG030F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02B2);
		reg &= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_SETTLE_CYCLE_CLR;
		reg |= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_SETTLE_CYCLE_SET(3);
		reg &= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_HYSTERISIS_CLR;
		reg |= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_HYSTERISIS_SET(1);
		reg &= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_ADAP_TOL_CLR;
		reg |= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_ADAP_TOL_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02B2);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG070F);
		reg &= USBDP_TRSV_REG070F_LN2_RX_SSLMS_BLOCK_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG070F_LN2_RX_SSLMS_BLOCK_ADAP_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG070F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06B2);
		reg &= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_SETTLE_CYCLE_CLR;
		reg |= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_SETTLE_CYCLE_SET(3);
		reg &= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_HYSTERISIS_CLR;
		reg |= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_HYSTERISIS_SET(1);
		reg &= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_ADAP_TOL_CLR;
		reg |= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_ADAP_TOL_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06B2);

		/* EDS Test RX 산포 최적화 셋팅
		0x090C	0x06	ln0_ana_rx_cdr_cco_vreg_r_sel=6
		0x190C	0x06	ln0_ana_rx_cdr_cco_vreg_r_sel=6
		0x0E04	0xFF	ln0_rx_dfe_madd_pbias_ctrl_sp=3
						ln0_rx_dfe_madd_pbias_ctrl_ssp=3
						ln0_rx_dfe_madd_pbias_ctrl_rbr=3
						ln0_rx_dfe_madd_pbias_ctrl_hbr=3

		0x1E04	0xFF	ln2_rx_dfe_madd_pbias_ctrl_sp=3
						ln2_rx_dfe_madd_pbias_ctrl_ssp=3
						ln2_rx_dfe_madd_pbias_ctrl_rbr=3
						ln2_rx_dfe_madd_pbias_ctrl_hbr=3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0243);
		reg &= USBDP_TRSV_REG0243_LN0_ANA_RX_CDR_CCO_VREG_R_SEL_CLR;
		reg |= USBDP_TRSV_REG0243_LN0_ANA_RX_CDR_CCO_VREG_R_SEL_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0243);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0643);
		reg &= USBDP_TRSV_REG0643_LN2_ANA_RX_CDR_CCO_VREG_R_SEL_CLR;
		reg |= USBDP_TRSV_REG0643_LN2_ANA_RX_CDR_CCO_VREG_R_SEL_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0643);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0381);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SP_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SP_SET(3);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SSP_SET(3);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_RBR_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_RBR_SET(3);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_HBR_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_HBR_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0381);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0781);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SP_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SP_SET(3);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SSP_SET(3);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_RBR_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_RBR_SET(3);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_HBR_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_HBR_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0781);

		/* USB Gen2 23dB loss CTLE 셋팅
		0x0EBC	0x05	ln0_rx_cdr_fbb_fine_ctrl_ssp=5
		0x091C	0x1D	ln0_rx_ctle_hf_rl_ctrl_ssp=5
		0x0928	0x0E	ln0_rx_ctle_hf_i_ctrl_ssp=6
		0x0A60	0x0B	ln0_rx_sslms_hf_init_rate_ssp=B
		0x0A78	0x05	ln0_rx_sslms_mf_init_ssp = 5
		0x0AD0	0x03	ln0_rx_sslms_adap_coef_sel__7_0=3
		0x1EBC	0x05	ln2_rx_cdr_fbb_fine_ctrl_ssp=5
		0x191C	0x1D	ln2_rx_ctle_hf_rl_ctrl_ssp=5
		0x1928	0x0E	ln2_rx_ctle_hf_i_ctrl_ssp=6
		0x1A60	0x0B	ln2_rx_sslms_hf_init_rate_ssp=B
		0x1A78	0x05	ln2_rx_sslms_mf_init_ssp = 5
		0x1AD0	0x03	ln2_rx_sslms_adap_coef_sel__7_0=3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03AF);
		reg &= USBDP_TRSV_REG03AF_LN0_RX_CDR_FBB_FINE_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG03AF_LN0_RX_CDR_FBB_FINE_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG03AF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0247);
		reg &= USBDP_TRSV_REG0247_LN0_RX_CTLE_HF_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0247_LN0_RX_CTLE_HF_RL_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0247);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG024A);
		reg &= USBDP_TRSV_REG024A_LN0_RX_CTLE_HF_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG024A_LN0_RX_CTLE_HF_I_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG024A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0298);
		reg &= USBDP_TRSV_REG0298_LN0_RX_SSLMS_HF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0298_LN0_RX_SSLMS_HF_INIT_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0298);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG029E);
		reg &= USBDP_TRSV_REG029E_LN0_RX_SSLMS_MF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG029E_LN0_RX_SSLMS_MF_INIT_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG029E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02B4);
		reg &= USBDP_TRSV_REG02B4_LN0_RX_SSLMS_ADAP_COEF_SEL__7_0_CLR;
		reg |= USBDP_TRSV_REG02B4_LN0_RX_SSLMS_ADAP_COEF_SEL__7_0_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07AF);
		reg &= USBDP_TRSV_REG07AF_LN2_RX_CDR_FBB_FINE_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG07AF_LN2_RX_CDR_FBB_FINE_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG07AF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0647);
		reg &= USBDP_TRSV_REG0647_LN2_RX_CTLE_HF_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0647_LN2_RX_CTLE_HF_RL_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0647);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG064A);
		reg &= USBDP_TRSV_REG064A_LN2_RX_CTLE_HF_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG064A_LN2_RX_CTLE_HF_I_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG064A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0698);
		reg &= USBDP_TRSV_REG0698_LN2_RX_SSLMS_HF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0698_LN2_RX_SSLMS_HF_INIT_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0698);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG069E);
		reg &= USBDP_TRSV_REG069E_LN2_RX_SSLMS_MF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG069E_LN2_RX_SSLMS_MF_INIT_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG069E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06B4);
		reg &= USBDP_TRSV_REG06B4_LN2_RX_SSLMS_ADAP_COEF_SEL__7_0_CLR;
		reg |= USBDP_TRSV_REG06B4_LN2_RX_SSLMS_ADAP_COEF_SEL__7_0_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06B4);

		/* LCPLL AFC Start code = 2
		0x0064	0x12	ana_lcpll_avc_vci_mid_sel=2
						ana_lcpll_avc_vci_min_sel=2
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0019);
		reg &= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MID_SEL_CLR;
		reg |= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MID_SEL_SET(2);
		reg &= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MIN_SEL_CLR;
		reg |= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MIN_SEL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0019);

		/* Offset calibraion 및 RXAFE 최적화
		0x0AA0	0x07	ln0_rx_sslms_c0_adap_speed=7
		0x0AA4	0x3F	ln0_rx_sslms_c1_adap_speed=7
						ln0_rx_sslms_c2_adap_speed=7
		0x0AA8	0x3F	ln0_rx_sslms_c3_adap_speed=7
						ln0_rx_sslms_c4_adap_speed=7
		0x0AAC	0x38	ln0_rx_sslms_c5_adap_speed=7
		0x0DEC	0x2B	ln0_ovrd_rx_sslms_hf_bin_ssp=1
						ln0_rx_sslms_hf_bin_ssp=B
		0x0DF0	0x25	ln0_ovrd_rx_sslms_mf_bin_ssp=1
						ln0_rx_sslms_mf_bin_ssp=5
		0x1AA0	0x07	ln2_rx_sslms_c0_adap_speed=7
		0x1AA4	0x3F	ln2_rx_sslms_c1_adap_speed=7
						ln2_rx_sslms_c2_adap_speed=7
		0x1AA8	0x3F	ln2_rx_sslms_c3_adap_speed=7
						ln2_rx_sslms_c4_adap_speed=7
		0x1AAC	0x38	ln2_rx_sslms_c5_adap_speed=7
		0x1DEC	0x2B	ln2_ovrd_rx_sslms_hf_bin_ssp=1
						ln2_rx_sslms_hf_bin_ssp=B
		0x1DF0	0x25	ln2_ovrd_rx_sslms_mf_bin_ssp=1
						ln2_rx_sslms_mf_bin_ssp=5
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02A8);
		reg &= USBDP_TRSV_REG02A8_LN0_RX_SSLMS_C0_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02A8_LN0_RX_SSLMS_C0_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02A8);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02A9);
		reg &= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C1_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C1_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C2_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C2_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02A9);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02AA);
		reg &= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C3_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C3_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C4_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C4_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02AA);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02AB);
		reg &= USBDP_TRSV_REG02AB_LN0_RX_SSLMS_C5_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02AB_LN0_RX_SSLMS_C5_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02AB);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037B);
		reg &= USBDP_TRSV_REG037B_LN0_OVRD_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037B_LN0_OVRD_RX_SSLMS_HF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG037B_LN0_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037B_LN0_RX_SSLMS_HF_BIN_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037C);
		reg &= USBDP_TRSV_REG037C_LN0_OVRD_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037C_LN0_OVRD_RX_SSLMS_MF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG037C_LN0_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037C_LN0_RX_SSLMS_MF_BIN_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06A8);
		reg &= USBDP_TRSV_REG06A8_LN2_RX_SSLMS_C0_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06A8_LN2_RX_SSLMS_C0_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06A8);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06A9);
		reg &= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C1_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C1_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C2_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C2_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06A9);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06AA);
		reg &= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C3_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C3_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C4_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C4_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06AA);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06AB);
		reg &= USBDP_TRSV_REG06AB_LN2_RX_SSLMS_C5_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06AB_LN2_RX_SSLMS_C5_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06AB);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077B);
		reg &= USBDP_TRSV_REG077B_LN2_OVRD_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077B_LN2_OVRD_RX_SSLMS_HF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG077B_LN2_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077B_LN2_RX_SSLMS_HF_BIN_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077C);
		reg &= USBDP_TRSV_REG077C_LN2_OVRD_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077C_LN2_OVRD_RX_SSLMS_MF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG077C_LN2_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077C_LN2_RX_SSLMS_MF_BIN_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077C);

		/* CDR data mode exit GEN1 ON / GEN2 OFF
		0x0C8C	0xFF	ln0_rx_cdr_data_mode_exit_en_sp = 4'hF
						ln0_rx_cdr_data_mode_exit_sel_sp = 4'hF
		0x1C8C	0xFF	ln2_rx_cdr_data_mode_exit_en_sp = 4'hF
						ln2_rx_cdr_data_mode_exit_sel_sp = 4'hF
		0x0C9C	0x7D	ln0_rx_cdr_data_mode_exit_bw_sel_sp = 2'h3
						ln0_rx_cdr_data_mode_exit_bw_sel_ssp = 2'h3
		0x1C9C	0x7D	ln2_rx_cdr_data_mode_exit_bw_sel_sp = 2'h3
						ln2_rx_cdr_data_mode_exit_bw_sel_ssp = 2'h3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0323);
		reg &= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_EN_SP_CLR;
		reg |= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_EN_SP_SET(0xf);
		reg &= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_SEL_SP_SET(0xf);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0323);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0723);
		reg &= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_EN_SP_CLR;
		reg |= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_EN_SP_SET(0xf);
		reg &= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_SEL_SP_SET(0xf);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0723);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0327);
		reg &= USBDP_TRSV_REG0327_LN0_RX_CDR_DATA_MODE_EXIT_BW_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0327_LN0_RX_CDR_DATA_MODE_EXIT_BW_SEL_SP_SET(0x3);
		reg &= USBDP_TRSV_REG0327_LN0_RX_CDR_DATA_MODE_EXIT_BW_SEL_SSP_CLR;
		reg |= USBDP_TRSV_REG0327_LN0_RX_CDR_DATA_MODE_EXIT_BW_SEL_SSP_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0327);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0727);
		reg &= USBDP_TRSV_REG0727_LN2_RX_CDR_DATA_MODE_EXIT_BW_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0727_LN2_RX_CDR_DATA_MODE_EXIT_BW_SEL_SP_SET(0x3);
		reg &= USBDP_TRSV_REG0727_LN2_RX_CDR_DATA_MODE_EXIT_BW_SEL_SSP_CLR;
		reg |= USBDP_TRSV_REG0727_LN2_RX_CDR_DATA_MODE_EXIT_BW_SEL_SSP_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0727);

		/* EDS Test 산포 최적화 셋팅
		0x0E7C	0x06	ln0_rx_cdr_fbb_ugamp_en=1
						ln0_rx_cdr_fbb_ugamp_iboost_en=1
		0x09E0	0x00	ln0_ana_rx_dfe_vref_dac_lsb_ctrl=0
		0x09E4	0x36	ln0_rx_dfe_madd_rl_cont_sp=6
						ln0_rx_dfe_madd_rl_cont_ssp=6
		0x1E7C	0x06	ln2_rx_cdr_fbb_ugamp_en=1
						ln2_rx_cdr_fbb_ugamp_iboost_en=1
		0x19E0	0x00	ln2_ana_rx_dfe_vref_dac_lsb_ctrl=0
		0x19E4	0x36	ln2_rx_dfe_madd_rl_cont_sp=6
						ln2_rx_dfe_madd_rl_cont_ssp=6
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG039F);
		reg &= USBDP_TRSV_REG039F_LN0_RX_CDR_FBB_UGAMP_EN_CLR;
		reg |= USBDP_TRSV_REG039F_LN0_RX_CDR_FBB_UGAMP_EN_SET(1);
		reg &= USBDP_TRSV_REG039F_LN0_RX_CDR_FBB_UGAMP_IBOOST_EN_CLR;
		reg |= USBDP_TRSV_REG039F_LN0_RX_CDR_FBB_UGAMP_IBOOST_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG039F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0278);
		reg &= USBDP_TRSV_REG0278_LN0_ANA_RX_DFE_VREF_DAC_LSB_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0278);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0279);
		reg &= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SP_CLR;
		reg |= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SP_SET(6);
		reg &= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SSP_CLR;
		reg |= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0279);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG079F);
		reg &= USBDP_TRSV_REG079F_LN2_RX_CDR_FBB_UGAMP_EN_CLR;
		reg |= USBDP_TRSV_REG079F_LN2_RX_CDR_FBB_UGAMP_EN_SET(1);
		reg &= USBDP_TRSV_REG079F_LN2_RX_CDR_FBB_UGAMP_IBOOST_EN_CLR;
		reg |= USBDP_TRSV_REG079F_LN2_RX_CDR_FBB_UGAMP_IBOOST_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG079F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0678);
		reg &= USBDP_TRSV_REG0678_LN2_ANA_RX_DFE_VREF_DAC_LSB_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0678);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0679);
		reg &= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SP_CLR;
		reg |= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SP_SET(6);
		reg &= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SSP_CLR;
		reg |= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0679);

		/* Offset calibration code average from +,- direction
		0x0E5C	0x02	ln0_rx_oc_mode_sel=2
		0x1E5C	0x02	ln2_rx_oc_mode_sel=2
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0397);
		reg &= USBDP_TRSV_REG0397_LN0_RX_OC_MODE_SEL_CLR;
		reg |= USBDP_TRSV_REG0397_LN0_RX_OC_MODE_SEL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0397);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0797);
		reg &= USBDP_TRSV_REG0797_LN2_RX_OC_MODE_SEL_CLR;
		reg |= USBDP_TRSV_REG0797_LN2_RX_OC_MODE_SEL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0797);

		/* DFE offset cal range setting
		0x0E80	0x03	ln0_ana_rx_dfe_madd_pbias_oc_ctrl=3
		0x1E80	0x03	ln2_ana_rx_dfe_madd_pbias_oc_ctrl=3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03A0);
		reg &= USBDP_TRSV_REG03A0_LN0_ANA_RX_DFE_MADD_PBIAS_OC_CTRL_CLR;
		reg |= USBDP_TRSV_REG03A0_LN0_ANA_RX_DFE_MADD_PBIAS_OC_CTRL_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG03A0);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07A0);
		reg &= USBDP_TRSV_REG07A0_LN2_ANA_RX_DFE_MADD_PBIAS_OC_CTRL_CLR;
		reg |= USBDP_TRSV_REG07A0_LN2_ANA_RX_DFE_MADD_PBIAS_OC_CTRL_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG07A0);

		/* EDS LVCC 개선
		0x08F0	0x30	ln0_rx_cdr_integ_pulse_sel_sp=0	EDS LVCC 개선
		0x18F0	0x30	ln2_rx_cdr_integ_pulse_sel_sp=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG023C);
		reg &= USBDP_TRSV_REG023C_LN0_RX_CDR_INTEG_PULSE_SEL_SP_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG023C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG063C);
		reg &= USBDP_TRSV_REG063C_LN2_RX_CDR_INTEG_PULSE_SEL_SP_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG063C);

		/* LFPS RX BW tuning
		0x0A08	0x10	ln0_ana_rx_sqls_in_lpf_ctrl=4
		0x1A08	0x10	ln2_ana_rx_sqls_in_lpf_ctrl=4
		0x0A0C	0x05	ln0_ana_rx_lfps_bw_ctrl=0
						ln0_ana_rx_lfps_th_ctrl=2
						ln0_ana_rx_lfps_i_ctrl=1
		0x1A0C	0x05	ln2_ana_rx_lfps_bw_ctrl=0
						ln2_ana_rx_lfps_th_ctrl=2
						ln2_ana_rx_lfps_i_ctrl=1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0282);
		reg &= USBDP_TRSV_REG0282_LN0_ANA_RX_SQLS_IN_LPF_CTRL_CLR;
		reg |= USBDP_TRSV_REG0282_LN0_ANA_RX_SQLS_IN_LPF_CTRL_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0282);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0682);
		reg &= USBDP_TRSV_REG0682_LN2_ANA_RX_SQLS_IN_LPF_CTRL_CLR;
		reg |= USBDP_TRSV_REG0682_LN2_ANA_RX_SQLS_IN_LPF_CTRL_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0682);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0283);
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_BW_CTRL_CLR;
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_TH_CTRL_SET(2);
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_I_CTRL_CLR;
		reg |= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_I_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0283);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0683);
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_BW_CTRL_CLR;
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_TH_CTRL_SET(2);
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_I_CTRL_CLR;
		reg |= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_I_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0683);

		/*
		 * USB
		 ========================================================================*/
	} else { /* mach_get_evt() == 0 */
		/* base
		 * EVT0 ML4_DEV05
		 * TN_DIPD_USBDP_2020_EVT0_SFR.xls ver.35.48 */

		/* ========================================================================
		 * Common
		 */

		/* JEQ off
		0x0830	0x07	tx_jeq_en=0
		0x1030	0x07	tx_jeq_en=0
		0x1830	0x07	tx_jeq_en=0
		0x2030	0x07	tx_jeq_en=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG020C);
		reg &= USBDP_TRSV_REG020C_LN0_ANA_TX_JEQ_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG020C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG040C);
		reg &= USBDP_TRSV_REG040C_LN1_ANA_TX_JEQ_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG040C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG060C);
		reg &= USBDP_TRSV_REG060C_LN2_ANA_TX_JEQ_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG060C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG080C);
		reg &= USBDP_TRSV_REG080C_LN3_ANA_TX_JEQ_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG080C);

		/* Common reset for 4lane tx
		0x0858	0x63	ovrd_ln*_tx_lane_rstn_sel=1
		0x1058	0x63	ovrd_ln*_tx_lane_rstn_sel=1
		0x1858	0x63	ovrd_ln*_tx_lane_rstn_sel=1
		0x2058	0x63	ovrd_ln*_tx_lane_rstn_sel=1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0216);
		reg &= USBDP_TRSV_REG0216_OVRD_LN0_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0216_OVRD_LN0_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0216);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0416);
		reg &= USBDP_TRSV_REG0416_OVRD_LN1_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0416_OVRD_LN1_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0416);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0616);
		reg &= USBDP_TRSV_REG0616_OVRD_LN2_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0616_OVRD_LN2_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0616);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0816);
		reg &= USBDP_TRSV_REG0816_OVRD_LN3_TX_LANE_RSTN_SEL_CLR;
		reg |= USBDP_TRSV_REG0816_OVRD_LN3_TX_LANE_RSTN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0816);

		/* AC driver setting
		0x0820	0x38	tx_drv_accdrv_ctrl=7
						tx_drv_accdrv_pol_sel =0
		0x1020	0x38	tx_drv_accdrv_ctrl=7
						tx_drv_accdrv_pol_sel =0
		0x1820	0x38	tx_drv_accdrv_ctrl=7
						tx_drv_accdrv_pol_sel =0
		0x2020	0x38	tx_drv_accdrv_ctrl=7
						tx_drv_accdrv_pol_sel =0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0208);
		reg &= USBDP_TRSV_REG0208_LN0_ANA_TX_DRV_ACCDRV_POL_SEL_CLR;
		reg &= USBDP_TRSV_REG0208_LN0_ANA_TX_DRV_ACCDRV_CTRL_CLR;
		reg |= USBDP_TRSV_REG0208_LN0_ANA_TX_DRV_ACCDRV_CTRL_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0208);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0408);
		reg &= USBDP_TRSV_REG0408_LN1_ANA_TX_DRV_ACCDRV_POL_SEL_CLR;
		reg &= USBDP_TRSV_REG0408_LN1_ANA_TX_DRV_ACCDRV_CTRL_CLR;
		reg |= USBDP_TRSV_REG0408_LN1_ANA_TX_DRV_ACCDRV_CTRL_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0408);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0608);
		reg &= USBDP_TRSV_REG0608_LN2_ANA_TX_DRV_ACCDRV_POL_SEL_CLR;
		reg &= USBDP_TRSV_REG0608_LN2_ANA_TX_DRV_ACCDRV_CTRL_CLR;
		reg |= USBDP_TRSV_REG0608_LN2_ANA_TX_DRV_ACCDRV_CTRL_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0608);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0808);
		reg &= USBDP_TRSV_REG0808_LN3_ANA_TX_DRV_ACCDRV_POL_SEL_CLR;
		reg &= USBDP_TRSV_REG0808_LN3_ANA_TX_DRV_ACCDRV_CTRL_CLR;
		reg |= USBDP_TRSV_REG0808_LN3_ANA_TX_DRV_ACCDRV_CTRL_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0808);

		/* TX impedance setting
		0x0BB8	0x05	tx_rcal_up_opt_code=1
						tx_rcal_dn_opt_code=1
		0x10B0	0x05	tx_rcal_up_opt_code=1
						tx_rcal_dn_opt_code=1
		0x1BB8	0x05	tx_rcal_up_opt_code=1
						tx_rcal_dn_opt_code=1
		0x20B0	0x05	tx_rcal_up_opt_code=1
						tx_rcal_dn_opt_code=1
		0x0BBC	0x77	tx_rcal_up_code=7
						tx_rcal_dn_code=7
		0x10B4	0x77	tx_rcal_up_code=7
						tx_rcal_dn_code=7
		0x1BBC	0x77	tx_rcal_up_code=7
						tx_rcal_dn_code=7
		0x20B4	0x77	tx_rcal_up_code=7
						tx_rcal_dn_code=7
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02EE);
		reg &= USBDP_TRSV_REG02EE_LN0_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG02EE_LN0_TX_RCAL_UP_OPT_CODE_SET(1);
		reg &= USBDP_TRSV_REG02EE_LN0_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG02EE_LN0_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02EE);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042C);
		reg &= USBDP_TRSV_REG042C_LN1_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG042C_LN1_TX_RCAL_UP_OPT_CODE_SET(1);
		reg &= USBDP_TRSV_REG042C_LN1_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG042C_LN1_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06EE);
		reg &= USBDP_TRSV_REG06EE_LN2_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG06EE_LN2_TX_RCAL_UP_OPT_CODE_SET(1);
		reg &= USBDP_TRSV_REG06EE_LN2_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG06EE_LN2_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06EE);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082C);
		reg &= USBDP_TRSV_REG082C_LN3_TX_RCAL_UP_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG082C_LN3_TX_RCAL_UP_OPT_CODE_SET(1);
		reg &= USBDP_TRSV_REG082C_LN3_TX_RCAL_DN_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG082C_LN3_TX_RCAL_DN_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02EF);
		reg &= USBDP_TRSV_REG02EF_LN0_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG02EF_LN0_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG02EF_LN0_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG02EF_LN0_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02EF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG042D);
		reg &= USBDP_TRSV_REG042D_LN1_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG042D_LN1_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG042D_LN1_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG042D_LN1_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG042D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06EF);
		reg &= USBDP_TRSV_REG06EF_LN2_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG06EF_LN2_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG06EF_LN2_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG06EF_LN2_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06EF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG082D);
		reg &= USBDP_TRSV_REG082D_LN3_TX_RCAL_UP_CODE_CLR;
		reg |= USBDP_TRSV_REG082D_LN3_TX_RCAL_UP_CODE_SET(7);
		reg &= USBDP_TRSV_REG082D_LN3_TX_RCAL_DN_CODE_CLR;
		reg |= USBDP_TRSV_REG082D_LN3_TX_RCAL_DN_CODE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG082D);

		/* prevent GFMUX stuck problem when bias_en low by MAC reset except U3 state
		 * only for EVT0
		0x0388	0xF0	ovrd_pcs_bgr/bias_en -> 1
						pcs_bgr/bias_en -> 1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E2);
		reg &= USBDP_CMN_REG00E2_OVRD_PCS_BGR_EN_CLR;
		reg |= USBDP_CMN_REG00E2_OVRD_PCS_BGR_EN_SET(1);
		reg &= USBDP_CMN_REG00E2_PCS_BGR_EN_CLR;
		reg |= USBDP_CMN_REG00E2_PCS_BGR_EN_SET(1);
		reg &= USBDP_CMN_REG00E2_OVRD_PCS_BIAS_EN_CLR;
		reg |= USBDP_CMN_REG00E2_OVRD_PCS_BIAS_EN_SET(1);
		reg &= USBDP_CMN_REG00E2_PCS_BIAS_EN_CLR;
		reg |= USBDP_CMN_REG00E2_PCS_BIAS_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E2);

		/* when using reset value, ATB enable
		0x0894	0x00	ln0_ana_tx_reserved=0
		0x0A1C	0x00	ln0_ana_rx_reserved=0
		0x1094	0x00	ln1_ana_tx_reserved=0
		0x1894	0x00	ln2_ana_tx_reserved=0
		0x1A1C	0x00	ln2_ana_rx_reserved=0
		0x2094	0x00	ln3_ana_tx_reserved=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0225);
		reg &= USBDP_TRSV_REG0225_LN0_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0225);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0287);
		reg &= USBDP_TRSV_REG0287_LN0_ANA_RX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0287);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0425);
		reg &= USBDP_TRSV_REG0425_LN1_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0425);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0625);
		reg &= USBDP_TRSV_REG0625_LN2_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0625);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0687);
		reg &= USBDP_TRSV_REG0687_LN2_ANA_RX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0687);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0825);
		reg &= USBDP_TRSV_REG0825_LN3_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0825);

		/* set C0~C1 of DFE during Offset calibration pll_lock_done watchdog enable
		0x0594	0x08	rx_sslms_wait_ofs_cal_done = 1
		0x0420	0x77	lcpll_afc_timeout_opt = 7
						ropll_afc_timeout_opt = 7
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0165);
		reg &= USBDP_CMN_REG0165_RX_SSLMS_WAIT_OFS_CAL_DONE_CLR;
		reg |= USBDP_CMN_REG0165_RX_SSLMS_WAIT_OFS_CAL_DONE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0165);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0108);
		reg &= USBDP_CMN_REG0108_LCPLL_AFC_TIMEOUT_OPT_CLR;
		reg |= USBDP_CMN_REG0108_LCPLL_AFC_TIMEOUT_OPT_SET(7);
		reg &= USBDP_CMN_REG0108_ROPLL_AFC_TIMEOUT_OPT_CLR;
		reg |= USBDP_CMN_REG0108_ROPLL_AFC_TIMEOUT_OPT_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0108);
		/*
		 * Common
		 ========================================================================*/


		/* ========================================================================
		 * USB
		 */

		/* rx_sigval digital off
		0x0C84	0x0F	ln0_rx_sigval_digital_en 1 -> 0
		0x1C84	0x0F	ln2_rx_sigval_digital_en 1 -> 0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0321);
		reg &= USBDP_TRSV_REG0321_LN0_RX_SIGVAL_DIGITAL_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0321);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0721);
		reg &= USBDP_TRSV_REG0721_LN2_RX_SIGVAL_DIGITAL_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0721);

		/* LCPLL setting
		0x0040	0x73	lcpll_afc_man_bsel_m=7
		0x0050	0x08	lcpll_agmc_man_gm_sel=8
		0x005C	0x01	ana_lcpll_avc_force_en=1
		0x0068	0x24	lcpll_ana_cpi_ctrl_coarse=4
		0x006C	0x77	lcpll_ana_cpp_ctrl_coarse=7
		0x0078	0x76	lcpll_ana_lpf_r_sel_coarse=6
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0010);
		reg &= USBDP_CMN_REG0010_ANA_LCPLL_AFC_MAN_BSEL_M_CLR;
		reg |= USBDP_CMN_REG0010_ANA_LCPLL_AFC_MAN_BSEL_M_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0010);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0014);
		reg &= USBDP_CMN_REG0014_ANA_LCPLL_AGMC_MAN_GM_SEL_CLR;
		reg |= USBDP_CMN_REG0014_ANA_LCPLL_AGMC_MAN_GM_SEL_SET(8);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0014);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0017);
		reg &= USBDP_CMN_REG0017_ANA_LCPLL_AVC_FORCE_EN_CLR;
		reg |= USBDP_CMN_REG0017_ANA_LCPLL_AVC_FORCE_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0017);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG001A);
		reg &= USBDP_CMN_REG001A_LCPLL_ANA_CPI_CTRL_COARSE_CLR;
		reg |= USBDP_CMN_REG001A_LCPLL_ANA_CPI_CTRL_COARSE_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG001A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG001B);
		reg &= USBDP_CMN_REG001B_LCPLL_ANA_CPP_CTRL_COARSE_CLR;
		reg |= USBDP_CMN_REG001B_LCPLL_ANA_CPP_CTRL_COARSE_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG001B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG001E);
		reg &= USBDP_CMN_REG001E_LCPLL_ANA_LPF_R_SEL_COARSE_CLR;
		reg |= USBDP_CMN_REG001E_LCPLL_ANA_LPF_R_SEL_COARSE_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG001E);

		/* RX impedance setting
		0x0BB0	0x01	rx_rcal_opt_code=1
		0x0BB4	0xE0	rx_rterm_ctrl=E
		0x1BB0	0x01	rx_rcal_opt_code=1
		0x1BB4	0xE0	rx_rterm_ctrl=E
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02EC);
		reg &= USBDP_TRSV_REG02EC_LN0_RX_RCAL_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG02EC_LN0_RX_RCAL_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02EC);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02ED);
		reg &= USBDP_TRSV_REG02ED_LN0_RX_RTERM_CTRL_CLR;
		reg |= USBDP_TRSV_REG02ED_LN0_RX_RTERM_CTRL_SET(0xe);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02ED);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06EC);
		reg &= USBDP_TRSV_REG06EC_LN2_RX_RCAL_OPT_CODE_CLR;
		reg |= USBDP_TRSV_REG06EC_LN2_RX_RCAL_OPT_CODE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06EC);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06ED);
		reg &= USBDP_TRSV_REG06ED_LN2_RX_RTERM_CTRL_CLR;
		reg |= USBDP_TRSV_REG06ED_LN2_RX_RTERM_CTRL_SET(0xe);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06ED);

		/* DFE setting
		0x09F0	0x60	rx_dfe_sadd_rl_ctrl =1
		0x19F0	0x60	rx_dfe_sadd_rl_ctrl =1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG027C);
		reg &= USBDP_TRSV_REG027C_LN0_ANA_RX_DFE_SADD_RL_CTRL_CLR;
		reg |= USBDP_TRSV_REG027C_LN0_ANA_RX_DFE_SADD_RL_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG027C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG067C);
		reg &= USBDP_TRSV_REG067C_LN2_ANA_RX_DFE_SADD_RL_CTRL_CLR;
		reg |= USBDP_TRSV_REG067C_LN2_ANA_RX_DFE_SADD_RL_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG067C);

		/* recommend not to using FBB[2:0] LSB 3bit
		0x0B28	0x05	ln0_rx_cdr_fbb_man_sel=1
		0x0B38	0x10	ln0_rx_cdr_fbb_coarse_ctrl=0x10
		0x1B28	0x05	ln2_rx_cdr_fbb_man_sel=1
		0x1B38	0x10	ln2_rx_cdr_fbb_coarse_ctrl=0x10
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02CA);
		reg &= USBDP_TRSV_REG02CA_LN0_RX_CDR_FBB_MAN_SEL_CLR;
		reg |= USBDP_TRSV_REG02CA_LN0_RX_CDR_FBB_MAN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02CA);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02CE);
		reg &= USBDP_TRSV_REG02CE_LN0_RX_CDR_FBB_COARSE_CTRL_CLR;
		reg |= USBDP_TRSV_REG02CE_LN0_RX_CDR_FBB_COARSE_CTRL_SET(0x10);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02CE);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06CA);
		reg &= USBDP_TRSV_REG06CA_LN2_RX_CDR_FBB_MAN_SEL_CLR;
		reg |= USBDP_TRSV_REG06CA_LN2_RX_CDR_FBB_MAN_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06CA);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06CE);
		reg &= USBDP_TRSV_REG06CE_LN2_RX_CDR_FBB_COARSE_CTRL_CLR;
		reg |= USBDP_TRSV_REG06CE_LN2_RX_CDR_FBB_COARSE_CTRL_SET(0x10);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06CE);

		/* Gen1 CDR VCO change 5GHz->2.5GHz, change EOM div setting
		0x09A0	0x24	ln0_rx_dfe_eom_pi_div_sel_sp=4
		0x19A0	0x24	ln2_rx_dfe_eom_pi_div_sel_sp=4
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0268);
		reg &= USBDP_TRSV_REG0268_LN0_RX_DFE_EOM_PI_DIV_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0268_LN0_RX_DFE_EOM_PI_DIV_SEL_SP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0268);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0668);
		reg &= USBDP_TRSV_REG0668_LN2_RX_DFE_EOM_PI_DIV_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0668_LN2_RX_DFE_EOM_PI_DIV_SEL_SP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0668);

		/* LFPS RX VIH shmoo hole
		0x0A08	0x1C	ln0_ana_rx_sqls_in_lpf_ctrl = 7
		0x1A08	0x1C	ln2_ana_rx_sqls_in_lpf_ctrl = 7
		0x0A0C	0x03	ln0_ana_rx_lfps_bw_ctrl=0
						ln0_ana_rx_lfps_th_ctrl=1
						ln0_ana_rx_lfps_i_ctrl=1
		0x1A0C	0x03	ln2_ana_rx_lfps_bw_ctrl=0
						ln2_ana_rx_lfps_th_ctrl=1
						ln2_ana_rx_lfps_i_ctrl=1
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0282);
		reg &= USBDP_TRSV_REG0282_LN0_ANA_RX_SQLS_IN_LPF_CTRL_CLR;
		reg |= USBDP_TRSV_REG0282_LN0_ANA_RX_SQLS_IN_LPF_CTRL_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0282);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0682);
		reg &= USBDP_TRSV_REG0682_LN2_ANA_RX_SQLS_IN_LPF_CTRL_CLR;
		reg |= USBDP_TRSV_REG0682_LN2_ANA_RX_SQLS_IN_LPF_CTRL_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0682);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0283);
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_BW_CTRL_CLR;
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_TH_CTRL_SET(1);
		reg &= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_I_CTRL_CLR;
		reg |= USBDP_TRSV_REG0283_LN0_ANA_RX_LFPS_I_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0283);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0683);
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_BW_CTRL_CLR;
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_TH_CTRL_SET(1);
		reg &= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_I_CTRL_CLR;
		reg |= USBDP_TRSV_REG0683_LN2_ANA_RX_LFPS_I_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0683);

		/* Offset cal settle time
		0x0A38	0xF8	ln0_rx_oc_settle_time=3
						ln0_rx_oc_num_of_sample =3
		0x1A38	0xF8	ln2_rx_oc_settle_time=3
						ln2_rx_oc_num_of_sample =3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG028E);
		reg &= USBDP_TRSV_REG028E_LN0_RX_OC_SETTLE_TIME_CLR;
		reg |= USBDP_TRSV_REG028E_LN0_RX_OC_SETTLE_TIME_SET(3);
		reg &= USBDP_TRSV_REG028E_LN0_RX_OC_NUM_OF_SAMPLE_CLR;
		reg |= USBDP_TRSV_REG028E_LN0_RX_OC_NUM_OF_SAMPLE_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG028E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG068E);
		reg &= USBDP_TRSV_REG068E_LN2_RX_OC_SETTLE_TIME_CLR;
		reg |= USBDP_TRSV_REG068E_LN2_RX_OC_SETTLE_TIME_SET(3);
		reg &= USBDP_TRSV_REG068E_LN2_RX_OC_NUM_OF_SAMPLE_CLR;
		reg |= USBDP_TRSV_REG068E_LN2_RX_OC_NUM_OF_SAMPLE_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG068E);

		/* USB Gen1 23dB loss CTLE
		0x0A5C	0x08	ln0_rx_sslms_hf_init_rate_sp=8
		0x1A5C	0x08	ln2_rx_sslms_hf_init_rate_sp=8
		0x0A74	0x06	ln0_rx_sslms_mf_init_sp = 6
		0x1A74	0x06	ln2_rx_sslms_mf_init_sp = 6
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0297);
		reg &= USBDP_TRSV_REG0297_LN0_RX_SSLMS_HF_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0297_LN0_RX_SSLMS_HF_INIT_SP_SET(8);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0297);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0697);
		reg &= USBDP_TRSV_REG0697_LN2_RX_SSLMS_HF_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0697_LN2_RX_SSLMS_HF_INIT_SP_SET(8);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0697);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG029D);
		reg &= USBDP_TRSV_REG029D_LN0_RX_SSLMS_MF_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG029D_LN0_RX_SSLMS_MF_INIT_SP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG029D);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG069D);
		reg &= USBDP_TRSV_REG069D_LN2_RX_SSLMS_MF_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG069D_LN2_RX_SSLMS_MF_INIT_SP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG069D);

		/* improve Offset cal cover range 187mVpp -> 469mVpp
		0x09E0	0x08	ln0_ana_rx_dfe_oc_dac_lsb_ctrl=0
		0x19E0	0x08	ln0_ana_rx_dfe_oc_dac_lsb_ctrl=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0278);
		reg &= USBDP_TRSV_REG0278_LN0_ANA_RX_DFE_OC_DAC_LSB_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0278);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0678);
		reg &= USBDP_TRSV_REG0678_LN2_ANA_RX_DFE_OC_DAC_LSB_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0678);

		/* improve LB Refclk 8~11MHz range CDR lock fail
		0x0B34	0x10	ln0_rx_cdr_fbb_pll_mode_ctrl=0x10
		0x1B34	0x10	ln2_rx_cdr_fbb_pll_mode_ctrl=0x10
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02CD);
		reg &= USBDP_TRSV_REG02CD_LN0_RX_CDR_FBB_PLL_MODE_CTRL_CLR;
		reg |= USBDP_TRSV_REG02CD_LN0_RX_CDR_FBB_PLL_MODE_CTRL_SET(0x10);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02CD);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06CD);
		reg &= USBDP_TRSV_REG06CD_LN2_RX_CDR_FBB_PLL_MODE_CTRL_CLR;
		reg |= USBDP_TRSV_REG06CD_LN2_RX_CDR_FBB_PLL_MODE_CTRL_SET(0x10);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06CD);

		/* improve LFPS TX Peak to Peak HVCC fail
		0x0408	0x20	tx_drv_lfps_mode_idrv_iup_ctrl = 2
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0102);
		reg &= USBDP_CMN_REG0102_TX_DRV_LFPS_MODE_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_CMN_REG0102_TX_DRV_LFPS_MODE_IDRV_IUP_CTRL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0102);

		/* C1 init SFR -> Rate control change
		0x0CD0	0x04	rx_sslms_c1_init_sp=4
		0x0CE8	0x04	rx_sslms_c1_init_ssp=4
		0x1CD0	0x04	rx_sslms_c1_init_sp=4
		0x1CE8	0x04	rx_sslms_c1_init_ssp=4
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0334);
		reg &= USBDP_TRSV_REG0334_LN0_RX_SSLMS_C1_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0334_LN0_RX_SSLMS_C1_E_INIT_SP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0334);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG033A);
		reg &= USBDP_TRSV_REG033A_LN0_RX_SSLMS_C1_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG033A_LN0_RX_SSLMS_C1_E_INIT_SSP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG033A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0734);
		reg &= USBDP_TRSV_REG0734_LN2_RX_SSLMS_C1_E_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0734_LN2_RX_SSLMS_C1_E_INIT_SP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0734);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG073A);
		reg &= USBDP_TRSV_REG073A_LN2_RX_SSLMS_C1_E_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG073A_LN2_RX_SSLMS_C1_E_INIT_SSP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG073A);

		/* C1 init SFR -> Rate control 변경
		0x0D00	0x04	ln0_rx_sslms_c1_o_init_sp=4
		0x0D18	0x04	ln0_rx_sslms_c1_o_init_ssp=4
		0x1D00	0x04	ln2_rx_sslms_c1_o_init_sp=4
		0x1D18	0x04	ln2_rx_sslms_c1_o_init_ssp=4
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0340);
		reg &= USBDP_TRSV_REG0340_LN0_RX_SSLMS_C1_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0340_LN0_RX_SSLMS_C1_O_INIT_SP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0340);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0346);
		reg &= USBDP_TRSV_REG0346_LN0_RX_SSLMS_C1_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0346_LN0_RX_SSLMS_C1_O_INIT_SSP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0346);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0740);
		reg &= USBDP_TRSV_REG0740_LN2_RX_SSLMS_C1_O_INIT_SP_CLR;
		reg |= USBDP_TRSV_REG0740_LN2_RX_SSLMS_C1_O_INIT_SP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0740);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0746);
		reg &= USBDP_TRSV_REG0746_LN2_RX_SSLMS_C1_O_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0746_LN2_RX_SSLMS_C1_O_INIT_SSP_SET(4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0746);

		/* LCPLL default setting 변경 요청 from PLL designer
		0x0070	0x10	ana_lcpll_ana_lc_gm_comp_vref_sel =1
						ana_lcpll_ana_lc_vreg_i_ctrl=0
		0x0074	0x00	lcpll_ana_lc_vreg_r_sel=0
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG001C);
		reg &= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_GM_COMP_VREF_SEL_CLR;
		reg |= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_GM_COMP_VREF_SEL_SET(1);
		reg &= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_VREG_I_CTRL_CLR;
		reg |= USBDP_CMN_REG001C_ANA_LCPLL_ANA_LC_VREG_I_CTRL_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG001C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG001D);
		reg &= USBDP_CMN_REG001D_ANA_LCPLL_ANA_LC_VREG_R_SEL_CLR;
		reg |= USBDP_CMN_REG001D_ANA_LCPLL_ANA_LC_VREG_R_SEL_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG001D);


		/* SX -> SR 변경으로 레지스터 이동
		0x053C	0xF0	lcpll_cd_hsclk_west_ctrl_sp =3
						lcpll_cd_hsclk_west_ctrl_ssp =3
		0x0544	0xF0	lcpll_cd_hsclk_east_ctrl_sp =3
						lcpll_cd_hsclk_east_ctrl_ssp =3
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG014F);
		reg &= USBDP_CMN_REG014F_LCPLL_CD_HSCLK_WEST_CTRL_SP_CLR;
		reg |= USBDP_CMN_REG014F_LCPLL_CD_HSCLK_WEST_CTRL_SP_SET(3);
		reg &= USBDP_CMN_REG014F_LCPLL_CD_HSCLK_WEST_CTRL_SSP_CLR;
		reg |= USBDP_CMN_REG014F_LCPLL_CD_HSCLK_WEST_CTRL_SSP_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG014F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0151);
		reg &= USBDP_CMN_REG0151_LCPLL_CD_HSCLK_EAST_CTRL_SP_CLR;
		reg |= USBDP_CMN_REG0151_LCPLL_CD_HSCLK_EAST_CTRL_SP_SET(3);
		reg &= USBDP_CMN_REG0151_LCPLL_CD_HSCLK_EAST_CTRL_SSP_CLR;
		reg |= USBDP_CMN_REG0151_LCPLL_CD_HSCLK_EAST_CTRL_SSP_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0151);


		/* Gen2 RX CTS tunning 반영
		0x0948	0x32	rx_ctle_vga_rl_ctrl_ssp=6
		0x0A60	0x0B	rx_sslms_hf_init_ssp=B
		0x0A78	0x05	rx_sslms_mf_init_ssp=5
		0x1948	0x32	rx_ctle_vga_rl_ctrl_ssp=6
		0x1A60	0x0B	rx_sslms_hf_init_ssp=B
		0x1A78	0x05	rx_sslms_mf_init_ssp=5
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0252);
		reg &= USBDP_TRSV_REG0252_LN0_RX_CTLE_VGA_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0252_LN0_RX_CTLE_VGA_RL_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0252);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0298);
		reg &= USBDP_TRSV_REG0298_LN0_RX_SSLMS_HF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0298_LN0_RX_SSLMS_HF_INIT_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0298);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG029E);
		reg &= USBDP_TRSV_REG029E_LN0_RX_SSLMS_MF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG029E_LN0_RX_SSLMS_MF_INIT_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG029E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0652);
		reg &= USBDP_TRSV_REG0652_LN2_RX_CTLE_VGA_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0652_LN2_RX_CTLE_VGA_RL_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0652);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0698);
		reg &= USBDP_TRSV_REG0698_LN2_RX_SSLMS_HF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0698_LN2_RX_SSLMS_HF_INIT_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0698);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG069E);
		reg &= USBDP_TRSV_REG069E_LN2_RX_SSLMS_MF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG069E_LN2_RX_SSLMS_MF_INIT_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG069E);

		/* Disable Offset Cal restart after USB rate changed
		0x0450	0x06	ofs_cal_rate_change_restart_en = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0114);
		reg &= USBDP_CMN_REG0114_OFS_CAL_RATE_CHANGE_RESTART_EN_CLR;
		reg |= USBDP_CMN_REG0114_OFS_CAL_RATE_CHANGE_RESTART_EN_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0114);

		/* Solve Adaptation done problem
		0x0C3C	0x05	ln0_rx_sslms_block_adap_en=1
		0x1C3C	0x05	ln2_rx_sslms_block_adap_en=1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG030F);
		reg &= USBDP_TRSV_REG030F_LN0_RX_SSLMS_BLOCK_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG030F_LN0_RX_SSLMS_BLOCK_ADAP_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG030F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG070F);
		reg &= USBDP_TRSV_REG070F_LN2_RX_SSLMS_BLOCK_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG070F_LN2_RX_SSLMS_BLOCK_ADAP_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG070F);


		/* LC_VCO_CAP_BIAS_VREF_VCI_SEL=2
		0x0128	0x20	ana_lcpll_reserved=0x20
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG004A);
		reg &= USBDP_CMN_REG004A_ANA_LCPLL_RESERVED_CLR;
		reg |= USBDP_CMN_REG004A_ANA_LCPLL_RESERVED_SET(0x20);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG004A);


		/* EDS Test RX scatter optimization setting
		0x090C	0x06	ln0_ana_rx_cdr_cco_vreg_r_sel=6
		0x190C	0x06	ln0_ana_rx_cdr_cco_vreg_r_sel=6
		0x0E04	0xFF	ln0_rx_dfe_madd_pbias_ctrl_sp=3
						ln0_rx_dfe_madd_pbias_ctrl_ssp=3
						ln0_rx_dfe_madd_pbias_ctrl_rbr=3
						ln0_rx_dfe_madd_pbias_ctrl_hbr=3
		0x09E4	0x3F	ln0_rx_dfe_madd_rl_cont_sp=7
						ln0_rx_dfe_madd_rl_cont_ssp=7"
		0x1E04	0xFF	ln2_rx_dfe_madd_pbias_ctrl_sp=3
						ln2_rx_dfe_madd_pbias_ctrl_ssp=3
						ln2_rx_dfe_madd_pbias_ctrl_rbr=3
						ln2_rx_dfe_madd_pbias_ctrl_hbr=3"
		0x19E4	0x3F	ln2_rx_dfe_madd_rl_cont_sp=7
						ln2_rx_dfe_madd_rl_cont_ssp=7
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0243);
		reg &= USBDP_TRSV_REG0243_LN0_ANA_RX_CDR_CCO_VREG_R_SEL_CLR;
		reg |= USBDP_TRSV_REG0243_LN0_ANA_RX_CDR_CCO_VREG_R_SEL_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0243);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0643);
		reg &= USBDP_TRSV_REG0643_LN2_ANA_RX_CDR_CCO_VREG_R_SEL_CLR;
		reg |= USBDP_TRSV_REG0643_LN2_ANA_RX_CDR_CCO_VREG_R_SEL_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0643);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0381);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SP_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SP_SET(3);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_SSP_SET(3);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_RBR_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_RBR_SET(3);
		reg &= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_HBR_CLR;
		reg |= USBDP_TRSV_REG0381_LN0_RX_DFE_MADD_PBIAS_CTRL_HBR_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0381);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0279);
		reg &= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SP_CLR;
		reg |= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SP_SET(7);
		reg &= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SSP_CLR;
		reg |= USBDP_TRSV_REG0279_LN0_RX_DFE_MADD_RL_CONT_SSP_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0279);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0781);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SP_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SP_SET(3);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_SSP_SET(3);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_RBR_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_RBR_SET(3);
		reg &= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_HBR_CLR;
		reg |= USBDP_TRSV_REG0781_LN2_RX_DFE_MADD_PBIAS_CTRL_HBR_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0781);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0679);
		reg &= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SP_CLR;
		reg |= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SP_SET(7);
		reg &= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SSP_CLR;
		reg |= USBDP_TRSV_REG0679_LN2_RX_DFE_MADD_RL_CONT_SSP_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0679);


		/* USB Gen2 23dB loss CTLE setting
		0x091C	0x1D	ln0_rx_ctle_hf_rl_ctrl_ssp=5
		0x0928	0x0E	ln0_rx_ctle_hf_i_ctrl_ssp=6
		0x0A60	0x0B	ln0_rx_sslms_hf_init_rate_ssp=B
		0x0A78	0x05	ln0_rx_sslms_mf_init_ssp = 5
		0x0AD0	0x03	ln0_rx_sslms_adap_coef_sel__7_0=3
		0x191C	0x1D	ln2_rx_ctle_hf_rl_ctrl_ssp=5
		0x1928	0x0C	ln2_rx_ctle_hf_i_ctrl_ssp=6
		0x1A60	0x0B	ln2_rx_sslms_hf_init_rate_ssp=B
		0x1A78	0x05	ln2_rx_sslms_mf_init_ssp = 5
		0x1AD0	0x03	ln2_rx_sslms_adap_coef_sel__7_0=3
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0247);
		reg &= USBDP_TRSV_REG0247_LN0_RX_CTLE_HF_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0247_LN0_RX_CTLE_HF_RL_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0247);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG024A);
		reg &= USBDP_TRSV_REG024A_LN0_RX_CTLE_HF_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG024A_LN0_RX_CTLE_HF_I_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG024A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0298);
		reg &= USBDP_TRSV_REG0298_LN0_RX_SSLMS_HF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0298_LN0_RX_SSLMS_HF_INIT_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0298);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG029E);
		reg &= USBDP_TRSV_REG029E_LN0_RX_SSLMS_MF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG029E_LN0_RX_SSLMS_MF_INIT_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG029E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02B4);
		reg &= USBDP_TRSV_REG02B4_LN0_RX_SSLMS_ADAP_COEF_SEL__7_0_CLR;
		reg |= USBDP_TRSV_REG02B4_LN0_RX_SSLMS_ADAP_COEF_SEL__7_0_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0647);
		reg &= USBDP_TRSV_REG0647_LN2_RX_CTLE_HF_RL_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG0647_LN2_RX_CTLE_HF_RL_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0647);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG064A);
		reg &= USBDP_TRSV_REG064A_LN2_RX_CTLE_HF_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG064A_LN2_RX_CTLE_HF_I_CTRL_SSP_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG064A);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0698);
		reg &= USBDP_TRSV_REG0698_LN2_RX_SSLMS_HF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG0698_LN2_RX_SSLMS_HF_INIT_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0698);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG069E);
		reg &= USBDP_TRSV_REG069E_LN2_RX_SSLMS_MF_INIT_SSP_CLR;
		reg |= USBDP_TRSV_REG069E_LN2_RX_SSLMS_MF_INIT_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG069E);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06B4);
		reg &= USBDP_TRSV_REG06B4_LN2_RX_SSLMS_ADAP_COEF_SEL__7_0_CLR;
		reg |= USBDP_TRSV_REG06B4_LN2_RX_SSLMS_ADAP_COEF_SEL__7_0_SET(3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06B4);


		/* LCPLL AFC Start code = 2
		0x0064	0x12	ana_lcpll_avc_vci_mid_sel=2
						ana_lcpll_avc_vci_min_sel=2
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0019);
		reg &= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MID_SEL_CLR;
		reg |= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MID_SEL_SET(2);
		reg &= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MIN_SEL_CLR;
		reg |= USBDP_CMN_REG0019_ANA_LCPLL_AVC_VCI_MIN_SEL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0019);


		/* Offset calibration code average from +,- direction
		0x0E5C	0x02	ln0_rx_oc_mode_sel=2
		0x1E5C	0x02	ln2_rx_oc_mode_sel=2
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0397);
		reg &= USBDP_TRSV_REG0397_LN0_RX_OC_MODE_SEL_CLR;
		reg |= USBDP_TRSV_REG0397_LN0_RX_OC_MODE_SEL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0397);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0797);
		reg &= USBDP_TRSV_REG0797_LN2_RX_OC_MODE_SEL_CLR;
		reg |= USBDP_TRSV_REG0797_LN2_RX_OC_MODE_SEL_SET(2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0797);


		/* Offset calibration and RXAFE optimization
		0x093C	0x28	ln0_rx_ctle_vga_i_ctrl_ssp=5
		0x0B3C	0x05	ln0_rx_cdr_fbb_fine_ctrl=5
		0x0AA0	0x07	ln0_rx_sslms_c0_adap_speed=7
		0x0AA4	0x3F	ln0_rx_sslms_c1_adap_speed=7
						ln0_rx_sslms_c2_adap_speed=7
		0x0AA8	0x3F	ln0_rx_sslms_c3_adap_speed=7
						ln0_rx_sslms_c4_adap_speed=7
		0x0AAC	0x38	ln0_rx_sslms_c5_adap_speed=7
		0x0AC8	0x3B	ln0_rx_sslms_settle_cycle=3
						ln0_rx_sslms_hysterisis=1
						ln0_rx_sslms_adap_tol=6
		0x0DEC	0x2B	ln0_ovrd_rx_sslms_hf_bin_ssp=1
						ln0_rx_sslms_hf_bin_ssp=B
		0x0DF0	0x25	ln0_ovrd_rx_sslms_mf_bin_ssp=1
						ln0_rx_sslms_mf_bin_ssp=5
		0x193C	0x28	ln2_rx_ctle_vga_i_ctrl_ssp=5
		0x1B3C	0x05	ln2_rx_cdr_fbb_fine_ctrl=5
		0x1AA0	0x07	ln2_rx_sslms_c0_adap_speed=7
		0x1AA4	0x3F	ln2_rx_sslms_c1_adap_speed=7
						ln2_rx_sslms_c2_adap_speed=7
		0x1AA8	0x3F	ln2_rx_sslms_c3_adap_speed=7
						ln2_rx_sslms_c4_adap_speed=7
		0x1AAC	0x38	ln2_rx_sslms_c5_adap_speed=7
		0x1AC8	0x3B	ln2_rx_sslms_settle_cycle=3
						ln2_rx_sslms_hysterisis=1
						ln2_rx_sslms_adap_tol=6
		0x1DEC	0x2B	ln2_ovrd_rx_sslms_hf_bin_ssp=1
						ln2_rx_sslms_hf_bin_ssp=B
		0x1DF0	0x25	ln2_ovrd_rx_sslms_mf_bin_ssp=1
						ln2_rx_sslms_mf_bin_ssp=5
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG024F);
		reg &= USBDP_TRSV_REG024F_LN0_RX_CTLE_VGA_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG024F_LN0_RX_CTLE_VGA_I_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG024F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02CF);
		reg &= USBDP_TRSV_REG02CF_LN0_RX_CDR_FBB_FINE_CTRL_CLR;
		reg |= USBDP_TRSV_REG02CF_LN0_RX_CDR_FBB_FINE_CTRL_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02CF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02A8);
		reg &= USBDP_TRSV_REG02A8_LN0_RX_SSLMS_C0_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02A8_LN0_RX_SSLMS_C0_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02A8);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02A9);
		reg &= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C1_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C1_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C2_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02A9_LN0_RX_SSLMS_C2_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02A9);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02AA);
		reg &= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C3_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C3_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C4_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02AA_LN0_RX_SSLMS_C4_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02AA);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02AB);
		reg &= USBDP_TRSV_REG02AB_LN0_RX_SSLMS_C5_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG02AB_LN0_RX_SSLMS_C5_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02AB);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02B2);
		reg &= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_SETTLE_CYCLE_CLR;
		reg |= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_SETTLE_CYCLE_SET(3);
		reg &= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_HYSTERISIS_CLR;
		reg |= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_HYSTERISIS_SET(1);
		reg &= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_ADAP_TOL_CLR;
		reg |= USBDP_TRSV_REG02B2_LN0_RX_SSLMS_ADAP_TOL_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02B2);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037B);
		reg &= USBDP_TRSV_REG037B_LN0_OVRD_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037B_LN0_OVRD_RX_SSLMS_HF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG037B_LN0_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037B_LN0_RX_SSLMS_HF_BIN_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG037C);
		reg &= USBDP_TRSV_REG037C_LN0_OVRD_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037C_LN0_OVRD_RX_SSLMS_MF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG037C_LN0_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG037C_LN0_RX_SSLMS_MF_BIN_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG037C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG064F);
		reg &= USBDP_TRSV_REG064F_LN2_RX_CTLE_VGA_I_CTRL_SSP_CLR;
		reg |= USBDP_TRSV_REG064F_LN2_RX_CTLE_VGA_I_CTRL_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG064F);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06CF);
		reg &= USBDP_TRSV_REG06CF_LN2_RX_CDR_FBB_FINE_CTRL_CLR;
		reg |= USBDP_TRSV_REG06CF_LN2_RX_CDR_FBB_FINE_CTRL_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06CF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06A8);
		reg &= USBDP_TRSV_REG06A8_LN2_RX_SSLMS_C0_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06A8_LN2_RX_SSLMS_C0_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06A8);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06A9);
		reg &= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C1_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C1_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C2_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06A9_LN2_RX_SSLMS_C2_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06A9);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06AA);
		reg &= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C3_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C3_ADAP_SPEED_SET(7);
		reg &= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C4_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06AA_LN2_RX_SSLMS_C4_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06AA);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06AB);
		reg &= USBDP_TRSV_REG06AB_LN2_RX_SSLMS_C5_ADAP_SPEED_CLR;
		reg |= USBDP_TRSV_REG06AB_LN2_RX_SSLMS_C5_ADAP_SPEED_SET(7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06AB);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06B2);
		reg &= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_SETTLE_CYCLE_CLR;
		reg |= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_SETTLE_CYCLE_SET(3);
		reg &= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_HYSTERISIS_CLR;
		reg |= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_HYSTERISIS_SET(1);
		reg &= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_ADAP_TOL_CLR;
		reg |= USBDP_TRSV_REG06B2_LN2_RX_SSLMS_ADAP_TOL_SET(6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06B2);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077B);
		reg &= USBDP_TRSV_REG077B_LN2_OVRD_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077B_LN2_OVRD_RX_SSLMS_HF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG077B_LN2_RX_SSLMS_HF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077B_LN2_RX_SSLMS_HF_BIN_SSP_SET(0xb);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077B);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG077C);
		reg &= USBDP_TRSV_REG077C_LN2_OVRD_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077C_LN2_OVRD_RX_SSLMS_MF_BIN_SSP_SET(1);
		reg &= USBDP_TRSV_REG077C_LN2_RX_SSLMS_MF_BIN_SSP_CLR;
		reg |= USBDP_TRSV_REG077C_LN2_RX_SSLMS_MF_BIN_SSP_SET(5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG077C);

		/* CDR data mode exit GEN1 ON / GEN2 OFF
		0x0C8C	0xFF	ln0_rx_cdr_data_mode_exit_en_sp = 4'hF
						ln0_rx_cdr_data_mode_exit_sel_sp = 4'hF
		0x1C8C	0xFF	ln2_rx_cdr_data_mode_exit_en_sp = 4'hF
						ln2_rx_cdr_data_mode_exit_sel_sp = 4'hF
		0x0C94	0x0F	ln0_rx_cdr_data_mode_exit_en_ssp = 4'h0
						ln0_rx_cdr_data_mode_exit_sel_ssp = 4'hF
		0x1C94	0x0F	ln2_rx_cdr_data_mode_exit_en_ssp = 4'h0
						ln2_rx_cdr_data_mode_exit_sel_ssp = 4'hF
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0323);
		reg &= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_EN_SP_CLR;
		reg |= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_EN_SP_SET(0xf);
		reg &= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0323_LN0_RX_CDR_DATA_MODE_EXIT_SEL_SP_SET(0xf);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0323);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0723);
		reg &= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_EN_SP_CLR;
		reg |= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_EN_SP_SET(0xf);
		reg &= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_SEL_SP_CLR;
		reg |= USBDP_TRSV_REG0723_LN2_RX_CDR_DATA_MODE_EXIT_SEL_SP_SET(0xf);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0723);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0325);
		reg &= USBDP_TRSV_REG0325_LN0_RX_CDR_DATA_MODE_EXIT_EN_SSP_CLR;
		reg |= USBDP_TRSV_REG0325_LN0_RX_CDR_DATA_MODE_EXIT_EN_SSP_SET(0x0);
		reg &= USBDP_TRSV_REG0325_LN0_RX_CDR_DATA_MODE_EXIT_SEL_SSP_CLR;
		reg |= USBDP_TRSV_REG0325_LN0_RX_CDR_DATA_MODE_EXIT_SEL_SSP_SET(0xf);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0325);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0725);
		reg &= USBDP_TRSV_REG0725_LN2_RX_CDR_DATA_MODE_EXIT_EN_SSP_CLR;
		reg |= USBDP_TRSV_REG0725_LN2_RX_CDR_DATA_MODE_EXIT_EN_SSP_SET(0x0);
		reg &= USBDP_TRSV_REG0725_LN2_RX_CDR_DATA_MODE_EXIT_SEL_SSP_CLR;
		reg |= USBDP_TRSV_REG0725_LN2_RX_CDR_DATA_MODE_EXIT_SEL_SSP_SET(0xf);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0725);

		/*
		 * USB
		 ========================================================================*/
	} /* mach_get_evt() == 0 */
}

static void phy_exynos_usbdp_g2_v2_set_pcs(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pcs_base;
	void __iomem *link_base = info->link_base;
	u32 reg;

	/* Change Elastic buffer threshold */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_RX_EBUF_PARAM);
	reg &= USBDP_PCS_RX_EBUF_PARAM_NUM_INIT_BUFFERING_CLR;
	reg &= USBDP_PCS_RX_EBUF_PARAM_SKP_REMOVE_TH_CLR;
	reg |= USBDP_PCS_RX_EBUF_PARAM_SKP_REMOVE_TH_SET(0x1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_RX_EBUF_PARAM);

	/* Change Elastic buffer Drain param
	 * -> enable TSEQ Mask
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_RX_EBUF_DRAINER_PARAM);
	reg |= USBDP_PCS_RX_EBUF_DRAINER_PARAM_EN_MASK_TSEQ_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_RX_EBUF_DRAINER_PARAM);

	/* abnormal comman pattern mask */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_RX_BACK_END_MODE_VEC);
	reg &= USBDP_PCS_RX_BACK_END_MODE_VEC_DISABLE_DATA_MASK_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_RX_BACK_END_MODE_VEC);

	/* De-serializer enabled when U2 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_PM_OUT_VEC_2);
	reg &= USBDP_PCS_PM_OUT_VEC_2_B4_DYNAMIC_CLR;
	reg |= USBDP_PCS_PM_OUT_VEC_2_B4_SEL_OUT_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_PM_OUT_VEC_2);

	/* TX Keeper Disable, Squelch off when U3 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_PM_OUT_VEC_3);
	reg &= USBDP_PCS_PM_OUT_VEC_3_B7_DYNAMIC_CLR;
	reg |= USBDP_PCS_PM_OUT_VEC_3_B7_SEL_OUT_SET(1);
	reg &= USBDP_PCS_PM_OUT_VEC_3_B2_SEL_OUT_CLR;	// Squelch off when U3
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_PM_OUT_VEC_3);

	// 20180822 PCS SFR setting : Noh M.W
	usbdp_cal_reg_wr(0x05700000, regs_base + EXYNOS_USBDP_PCS_PM_NS_VEC_PS1_N1);
	usbdp_cal_reg_wr(0x01700202, regs_base + EXYNOS_USBDP_PCS_PM_NS_VEC_PS2_N0);
	usbdp_cal_reg_wr(0x01700707, regs_base + EXYNOS_USBDP_PCS_PM_NS_VEC_PS3_N0);
	usbdp_cal_reg_wr(0x0070, regs_base + EXYNOS_USBDP_PCS_PM_TIMEOUT_0);
	usbdp_cal_reg_wr(0x1000, regs_base + EXYNOS_USBDP_PCS_PM_TIMEOUT_3);
	usbdp_cal_reg_wr(0x1401, regs_base + EXYNOS_USBDP_PCS_RX_EBUF_DRAINER_PARAM);

	// Block Aligner Type B
	if (mach_get_evt() == 1 && mach_get_subrev() == 1) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_RX_RX_CONTROL);
		reg &= USBDP_PCS_RX_RX_CONTROL_EN_BLOCK_ALIGNER_TYPE_B_CLR;
		reg |= USBDP_PCS_RX_RX_CONTROL_EN_BLOCK_ALIGNER_TYPE_B_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_RX_RX_CONTROL);
	}

    /* Gen1 Tx DRIVER pre-shoot, de-emphasis, level ctrl
     * [15:12] de-emphasis
     * [9:6] level - 0xb (max)
     * [3:0] pre-shoot - Gen1 does not support pre-shoot
     */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);
	reg &= USBDP_PCS_LEQ_HS_TX_COEF_MAP_0_LEVEL_CLR;
	reg |= USBDP_PCS_LEQ_HS_TX_COEF_MAP_0_LEVEL_SET((8 << 12 | 0xB << 6 | 0x0)); // 0x82C0
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_LEQ_HS_TX_COEF_MAP_0);

	/* Gen2 Tx DRIVER level ctrl */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_LEQ_LOCAL_COEF);
	reg &= USBDP_PCS_LEQ_LOCAL_COEF_PMA_CENTER_COEF_CLR;
	reg |= USBDP_PCS_LEQ_LOCAL_COEF_PMA_CENTER_COEF_SET(0xB);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_LEQ_LOCAL_COEF);

	/* Gen2 Tx DRIVER pre-shoot, de-emphasis ctrl
	 * [17:12] Deemphasis
	 * [11:6] Level (not valid)
	 * [5:0] Preshoot
	 * */
	/* normal operation, compliance pattern 15 */
	reg = usbdp_cal_reg_rd(link_base + USB31DRD_LINK_LCSR_TX_DEEMPH);
	reg &= ~(0x3FFFF);
	reg |= 4 << 12 | 4;
	usbdp_cal_reg_wr(reg, link_base + USB31DRD_LINK_LCSR_TX_DEEMPH);

	/* compliance pattern 13 */
	reg = usbdp_cal_reg_rd(link_base + USB31DRD_LINK_LCSR_TX_DEEMPH_1);
	reg &= ~(0x3FFFF);
	reg |= 4;
	usbdp_cal_reg_wr(reg, link_base + USB31DRD_LINK_LCSR_TX_DEEMPH_1);

	/* compliance pattern 14 */
	reg = usbdp_cal_reg_rd(link_base + USB31DRD_LINK_LCSR_TX_DEEMPH_2);
	reg &= ~(0x3FFFF);
	reg |= 4 << 12;
	usbdp_cal_reg_wr(reg, link_base + USB31DRD_LINK_LCSR_TX_DEEMPH_2);
}

static void phy_exynos_usbdp_g2_v2_pma_lane_mux_sel(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	/* Lane configuration */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00B8);
	reg &= USBDP_CMN_REG00B8_LANE_MUX_SEL_DP_CLR;

	if (info->used_phy_port == 0) {
		reg |= USBDP_CMN_REG00B8_LANE_MUX_SEL_DP_SET(0xc);
	} else {
		reg |= USBDP_CMN_REG00B8_LANE_MUX_SEL_DP_SET(0x3);
	}
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00B8);

	/*
	ln1_tx_rxd_en = 0
	ln3_tx_rxd_en = 0
	*/
	if (info->used_phy_port == 0) {
		/* ln3_tx_rxd_en = 0 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0413);
		reg &= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_COMP_EN_CLR;
		reg &= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0413);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0813);
		reg &= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_COMP_EN_CLR;
		reg |= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_COMP_EN_SET(1);
		reg &= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_EN_CLR;
		reg |= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0813);
	} else {
		/* ln1_tx_rxd_en = 0 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0413);
		reg &= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_COMP_EN_CLR;
		reg |= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_COMP_EN_SET(1);
		reg &= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_EN_CLR;
		reg |= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0413);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0813);
		reg &= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_COMP_EN_CLR;
		reg &= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0813);
	}

	/* dp_lane_en = 0  */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00B9);
	reg &= USBDP_CMN_REG00B9_DP_LANE_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00B9);
}

static void phy_exynos_usbdp_g2_v2_ctrl_pma_rst_release(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->ctrl_base;
	u32 reg;

	/* Reset release from port */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_TRSV_SW_RST;
	reg &= ~PMA_CMN_SW_RST;
	reg &= ~PMA_INIT_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
}

static int phy_exynos_usbdp_g2_v2_pma_check_pll_lock(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 cnt;

	for (cnt = 1000; cnt != 0; cnt--) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01C0);
		if ((reg & (USBDP_CMN_REG01C0_ANA_LCPLL_LOCK_DONE_MSK
				| USBDP_CMN_REG01C0_ANA_LCPLL_AFC_DONE_MSK))) {
			break;
		}
		udelay(1);
	}

	if (!cnt) {
		return -1;
	}

	return 0;
}

static int phy_exynos_usbdp_g2_v2_pma_check_cdr_lock(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 cnt;

	if (info->used_phy_port == 0) {
		for (cnt = 1000; cnt != 0; cnt--) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03C3);
			if ((reg & (USBDP_TRSV_REG03C3_LN0_MON_RX_CDR_LOCK_DONE_MSK
					| USBDP_TRSV_REG03C3_LN0_MON_RX_CDR_FLD_PLL_MODE_DONE_MSK
					| USBDP_TRSV_REG03C3_LN0_MON_RX_CDR_CAL_DONE_MSK
					| USBDP_TRSV_REG03C3_LN0_MON_RX_CDR_AFC_DONE_MSK))) {
				break;
			}
			udelay(1);
		}
	} else {
		for (cnt = 1000; cnt != 0; cnt--) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07C3);
			if ((reg & (USBDP_TRSV_REG07C3_LN2_MON_RX_CDR_LOCK_DONE_MSK
					| USBDP_TRSV_REG07C3_LN2_MON_RX_CDR_FLD_PLL_MODE_DONE_MSK
					| USBDP_TRSV_REG07C3_LN2_MON_RX_CDR_CAL_DONE_MSK
					| USBDP_TRSV_REG07C3_LN2_MON_RX_CDR_AFC_DONE_MSK))) {
				break;
			}
			udelay(1);
		}
	}

	if (!cnt) {
		return -2;
	}

	return 0;
}

static int __maybe_unused phy_exynos_usbdp_g2_v2_pma_check_offset_cal_code(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 code = 0;

	if (info->used_phy_port == 0) {
		/*
		0x0F28	ln0_mon_rx_oc_dfe_adder_even
		0x0F2C	ln0_mon_rx_oc_dfe_adder_odd
		0x0F30	ln0_mon_rx_oc_dfe_dac_adder_even
				ln0_mon_rx_oc_dfe_dac_adder_odd
		0x0F34	ln0_mon_rx_oc_dfe_sa_edge_even
		0x0F38	ln0_mon_rx_oc_dfe_sa_edge_odd
		0x0F3C	ln0_mon_rx_oc_dfe_dac_edge_odd
				ln0_mon_rx_oc_dfe_dac_edge_even
		0x0F40	ln0_mon_rx_oc_dfe_sa_err_even
		0x0F44	ln0_mon_rx_oc_dfe_sa_err_odd
		0x0F48	ln0_mon_rx_oc_dfe_dac_err_even
				ln0_mon_rx_oc_dfe_dac_err_odd
		0x0F4C	ln0_mon_rx_oc_ctle
		0x072C	ln0_mon_rx_oc_init_vga_code
		0x0730	ln0_mon_rx_oc_init_dac_vga_code
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03CA);
		code = USBDP_TRSV_REG03CA_LN0_MON_RX_OC_DFE_ADDER_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] adder e       = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03CB);
		code = USBDP_TRSV_REG03CB_LN0_MON_RX_OC_DFE_ADDER_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] adder o       = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03CC);
		code = USBDP_TRSV_REG03CC_LN0_MON_RX_OC_DFE_DAC_ADDER_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] dac_adder e/o = %02X", code);
		code = USBDP_TRSV_REG03CC_LN0_MON_RX_OC_DFE_DAC_ADDER_ODD_GET(reg);
		usbdp_print_offset_cal_code("/%02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03CD);
		code = USBDP_TRSV_REG03CD_LN0_MON_RX_OC_DFE_SA_EDGE_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_edge e     = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03CE);
		code = USBDP_TRSV_REG03CE_LN0_MON_RX_OC_DFE_SA_EDGE_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_edge o     = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03CF);
		code = USBDP_TRSV_REG03CF_LN0_MON_RX_OC_DFE_DAC_EDGE_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] dac_edge o/e  = %02X", code);
		code = USBDP_TRSV_REG03CF_LN0_MON_RX_OC_DFE_DAC_EDGE_EVEN_GET(reg);
		usbdp_print_offset_cal_code("/%02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03D0);
		code = USBDP_TRSV_REG03D0_LN0_MON_RX_OC_DFE_SA_ERR_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_err e      = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03D1);
		code = USBDP_TRSV_REG03D1_LN0_MON_RX_OC_DFE_SA_ERR_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_err o      = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03D2);
		code = USBDP_TRSV_REG03D2_LN0_MON_RX_OC_DFE_DAC_ERR_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] dac_err e/o   = %02X", code);
		code = USBDP_TRSV_REG03D2_LN0_MON_RX_OC_DFE_DAC_ERR_ODD_GET(reg);
		usbdp_print_offset_cal_code("/%02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03D3);
		code = USBDP_TRSV_REG03D3_LN0_MON_RX_OC_CTLE_GET(reg);
		usbdp_print_offset_cal_code("[OC] ctle          = %02X\n", code);

		/*
		0x0F5C	ln0_mon_rx_oc_fail__7_0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03D7);
		code = USBDP_TRSV_REG03D7_LN0_MON_RX_OC_FAIL__7_0_GET(reg);
		usbdp_print_offset_cal_code("[OC] oc_fail       = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01CB);
		code = USBDP_CMN_REG01CB_LN0_MON_RX_OC_INIT_VGA_CODE_GET(reg);
		usbdp_print_offset_cal_code("[OC] oc_vga        = %02X\n", code);

#if 0
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01CC);
		code = USBDP_CMN_REG01CC_LN0_MON_RX_OC_INIT_DAC_VGA_CODE_GET(reg);
		usbdp_print_offset_cal_code("[OC] init_dac_vga_code = %02X\n", code);
#endif
	} else {
		/*
		0x0748	ln2_mon_rx_oc_init_vga_code
		0x074C	ln2_mon_rx_oc_init_dac_vga_code
		0x1F28	ln2_mon_rx_oc_dfe_adder_even
		0x1F2C	ln2_mon_rx_oc_dfe_adder_odd
		0x1F30	ln2_mon_rx_oc_dfe_dac_adder_even
				ln2_mon_rx_oc_dfe_dac_adder_odd
		0x1F34	ln2_mon_rx_oc_dfe_sa_edge_even
		0x1F38	ln2_mon_rx_oc_dfe_sa_edge_odd
		0x1F3C	ln2_mon_rx_oc_dfe_dac_edge_odd
				ln2_mon_rx_oc_dfe_dac_edge_even
		0x1F40	ln2_mon_rx_oc_dfe_sa_err_even
		0x1F44	ln2_mon_rx_oc_dfe_sa_err_odd
		0x1F48	ln2_mon_rx_oc_dfe_dac_err_even
				ln2_mon_rx_oc_dfe_dac_err_odd
		0x1F4C	ln2_mon_rx_oc_ctle
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07CA);
		code = USBDP_TRSV_REG07CA_LN2_MON_RX_OC_DFE_ADDER_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] adder e       = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07CB);
		code = USBDP_TRSV_REG07CB_LN2_MON_RX_OC_DFE_ADDER_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] adder o       = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07CC);
		code = USBDP_TRSV_REG07CC_LN2_MON_RX_OC_DFE_DAC_ADDER_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] dac_adder e/o = %02X", code);
		code = USBDP_TRSV_REG07CC_LN2_MON_RX_OC_DFE_DAC_ADDER_ODD_GET(reg);
		usbdp_print_offset_cal_code("/%02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07CD);
		code = USBDP_TRSV_REG07CD_LN2_MON_RX_OC_DFE_SA_EDGE_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_edge e     = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07CE);
		code = USBDP_TRSV_REG07CE_LN2_MON_RX_OC_DFE_SA_EDGE_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_edge o     = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07CF);
		code = USBDP_TRSV_REG07CF_LN2_MON_RX_OC_DFE_DAC_EDGE_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] dac_edge o/e  = %02X", code);
		code = USBDP_TRSV_REG07CF_LN2_MON_RX_OC_DFE_DAC_EDGE_EVEN_GET(reg);
		usbdp_print_offset_cal_code("/%02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07D0);
		code = USBDP_TRSV_REG07D0_LN2_MON_RX_OC_DFE_SA_ERR_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_err e      = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07D1);
		code = USBDP_TRSV_REG07D1_LN2_MON_RX_OC_DFE_SA_ERR_ODD_GET(reg);
		usbdp_print_offset_cal_code("[OC] sa_err o      = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07D2);
		code = USBDP_TRSV_REG07D2_LN2_MON_RX_OC_DFE_DAC_ERR_EVEN_GET(reg);
		usbdp_print_offset_cal_code("[OC] dac_err e/o   = %02X", code);
		code = USBDP_TRSV_REG07D2_LN2_MON_RX_OC_DFE_DAC_ERR_ODD_GET(reg);
		usbdp_print_offset_cal_code("/%02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07D3);
		code = USBDP_TRSV_REG07D3_LN2_MON_RX_OC_CTLE_GET(reg);
		usbdp_print_offset_cal_code("[OC] ctle          = %02X\n", code);

		/*
		0x1F5C	ln2_mon_rx_oc_fail__7_0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07D7);
		code = USBDP_TRSV_REG07D7_LN2_MON_RX_OC_FAIL__7_0_GET(reg);
		usbdp_print_offset_cal_code("[OC] oc_fail       = %02X\n", code);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01D2);
		code = USBDP_CMN_REG01D2_LN2_MON_RX_OC_INIT_VGA_CODE_GET(reg);
		usbdp_print_offset_cal_code("[OC] oc_vga        = %02X\n", code);
#if 0
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01D3);
		code = USBDP_CMN_REG01D3_LN2_MON_RX_OC_INIT_DAC_VGA_CODE_GET(reg);
		usbdp_print_offset_cal_code("[OC] init_dac_vga_code = %02X\n", code);
#endif
	}

	if (code)
		return -1;
	else
		return 0;
}

static void phy_exynos_usbdp_g2_v2_pma_ovrd_enable(struct exynos_usbphy_info *info, u32 cmn_rate)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	/*
	pcs_pll_en = 0
	0x0390 0x40
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E4);
	reg &= USBDP_CMN_REG00E4_OVRD_PCS_PLL_EN_CLR;
	reg |= USBDP_CMN_REG00E4_OVRD_PCS_PLL_EN_SET(1);
	reg &= USBDP_CMN_REG00E4_PCS_PLL_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E4);

	/*
	pcs_bgr_en = 0
	pcs_bias_en = 0
	0x0388 0xA0
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E2);
	reg &= USBDP_CMN_REG00E2_OVRD_PCS_BGR_EN_CLR;
	reg |= USBDP_CMN_REG00E2_OVRD_PCS_BGR_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_PCS_BGR_EN_CLR;
	reg &= USBDP_CMN_REG00E2_OVRD_PCS_BIAS_EN_CLR;
	reg |= USBDP_CMN_REG00E2_OVRD_PCS_BIAS_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_PCS_BIAS_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E2);

	/*
	cmn_rate
	0x0384 0x04 // Gen1
		   0x05 // Gen2
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E1);
	reg &= USBDP_CMN_REG00E1_OVRD_PCS_RATE_CLR;
	reg |= USBDP_CMN_REG00E1_OVRD_PCS_RATE_SET(1);
	reg &= USBDP_CMN_REG00E1_PCS_RATE_CLR;
	reg |= USBDP_CMN_REG00E1_PCS_RATE_SET(cmn_rate);	// 0: Gen1, 1: Gen2
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E1);

	/*
	lane_mux_sel_dp
	ln1_tx_rxd_en
	ln3_tx_rxd_en
	0x02E0 0x30
	0x104C 0x01
	0x204C 0x01
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00B8);
	reg &= USBDP_CMN_REG00B8_LANE_MUX_SEL_DP_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00B8);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0413);
	reg &= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_COMP_EN_CLR;
	reg &= USBDP_TRSV_REG0413_OVRD_LN1_TX_RXD_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0413);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0813);
	reg &= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_COMP_EN_CLR;
	reg &= USBDP_TRSV_REG0813_OVRD_LN3_TX_RXD_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0813);

	/*
	pcs_bgr_en = 1
	pcs_bias_en = 1
	pcs_powerdown = 0
	pcs_des_en = 0
	pcs_cdr_en = 1
	pcs_pll_en = 1
	pcs_rx_ctle_en = 1
	pcs_rx_sqhs_en = 1
	pcs_rx_term_en = 1
	pcs_tx_drv_en =1
	pcs_tx_elecidle = 1
	pcs_tx_lfps_en = 0
	pcs_tx_rcv_det_en =1
	pcs_tx_ser_en =0
	0x0388 0xFB
	0x038C 0x20
	0x0390 0x63
	0x0394 0x03
	0x0398 0xC3
	0x03A4 0x03
	0x03A8 0x8E
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E2);
	reg &= USBDP_CMN_REG00E2_OVRD_PCS_BGR_EN_CLR;
	reg |= USBDP_CMN_REG00E2_OVRD_PCS_BGR_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_PCS_BGR_EN_CLR;
	reg |= USBDP_CMN_REG00E2_PCS_BGR_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_OVRD_PCS_BIAS_EN_CLR;
	reg |= USBDP_CMN_REG00E2_OVRD_PCS_BIAS_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_PCS_BIAS_EN_CLR;
	reg |= USBDP_CMN_REG00E2_PCS_BIAS_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_OVRD_PCS_POWERDOWN_CLR;
	reg |= USBDP_CMN_REG00E2_OVRD_PCS_POWERDOWN_SET(1);
	reg &= USBDP_CMN_REG00E2_PCS_POWERDOWN_CLR;
	reg &= USBDP_CMN_REG00E2_OVRD_PCS_CDR_EN_CLR;
	reg |= USBDP_CMN_REG00E2_OVRD_PCS_CDR_EN_SET(1);
	reg &= USBDP_CMN_REG00E2_PCS_CDR_EN_CLR;
	reg |= USBDP_CMN_REG00E2_PCS_CDR_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E2);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E3);
	reg &= USBDP_CMN_REG00E3_OVRD_PCS_DES_EN_CLR;
	reg |= USBDP_CMN_REG00E3_OVRD_PCS_DES_EN_SET(1);
	reg &= USBDP_CMN_REG00E3_PCS_DES_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E3);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E4);
	reg &= USBDP_CMN_REG00E4_OVRD_PCS_PLL_EN_CLR;
	reg |= USBDP_CMN_REG00E4_OVRD_PCS_PLL_EN_SET(1);
	reg &= USBDP_CMN_REG00E4_PCS_PLL_EN_CLR;
	reg |= USBDP_CMN_REG00E4_PCS_PLL_EN_SET(1);
	reg &= USBDP_CMN_REG00E4_OVRD_PCS_RX_CTLE_EN_CLR;
	reg |= USBDP_CMN_REG00E4_OVRD_PCS_RX_CTLE_EN_SET(1);
	reg &= USBDP_CMN_REG00E4_PCS_RX_CTLE_EN_CLR;
	reg |= USBDP_CMN_REG00E4_PCS_RX_CTLE_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E4);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E5);
	reg &= USBDP_CMN_REG00E5_OVRD_PCS_RX_SQHS_EN_CLR;
	reg |= USBDP_CMN_REG00E5_OVRD_PCS_RX_SQHS_EN_SET(1);
	reg &= USBDP_CMN_REG00E5_PCS_RX_SQHS_EN_CLR;
	reg |= USBDP_CMN_REG00E5_PCS_RX_SQHS_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E5);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E6);
	reg &= USBDP_CMN_REG00E6_OVRD_PCS_RX_TERM_EN_CLR;
	reg |= USBDP_CMN_REG00E6_OVRD_PCS_RX_TERM_EN_SET(1);
	reg &= USBDP_CMN_REG00E6_PCS_RX_TERM_EN_CLR;
	reg |= USBDP_CMN_REG00E6_PCS_RX_TERM_EN_SET(1);
	reg &= USBDP_CMN_REG00E6_OVRD_PCS_TX_DRV_EN_CLR;
	reg |= USBDP_CMN_REG00E6_OVRD_PCS_TX_DRV_EN_SET(1);
	reg &= USBDP_CMN_REG00E6_PCS_TX_DRV_EN_CLR;
	reg |= USBDP_CMN_REG00E6_PCS_TX_DRV_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E6);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E9);
	reg &= USBDP_CMN_REG00E9_OVRD_PCS_TX_ELECIDLE_CLR;
	reg |= USBDP_CMN_REG00E9_OVRD_PCS_TX_ELECIDLE_SET(1);
	reg &= USBDP_CMN_REG00E9_PCS_TX_ELECIDLE_CLR;
	reg |= USBDP_CMN_REG00E9_PCS_TX_ELECIDLE_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E9);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00EA);
	reg &= USBDP_CMN_REG00EA_OVRD_PCS_TX_LFPS_EN_CLR;
	reg |= USBDP_CMN_REG00EA_OVRD_PCS_TX_LFPS_EN_SET(1);
	reg &= USBDP_CMN_REG00EA_PCS_TX_LFPS_EN_CLR;
	reg &= USBDP_CMN_REG00EA_OVRD_PCS_TX_RCV_DET_EN_CLR;
	reg |= USBDP_CMN_REG00EA_OVRD_PCS_TX_RCV_DET_EN_SET(1);
	reg &= USBDP_CMN_REG00EA_PCS_TX_RCV_DET_EN_CLR;
	reg |= USBDP_CMN_REG00EA_PCS_TX_RCV_DET_EN_SET(1);
	reg &= USBDP_CMN_REG00EA_OVRD_PCS_TX_SER_EN_CLR;
	reg |= USBDP_CMN_REG00EA_OVRD_PCS_TX_SER_EN_SET(1);
	reg &= USBDP_CMN_REG00EA_PCS_TX_SER_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00EA);
}

static void phy_exynos_usbdp_g2_v2_pma_ovrd_pcs_rst_release(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	/*
	dp_init/cmn_rstn = 1
	0x038C 0xEF
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E3);
	reg &= USBDP_CMN_REG00E3_OVRD_PCS_CMN_RSTN_CLR;
	reg |= USBDP_CMN_REG00E3_OVRD_PCS_CMN_RSTN_SET(1);
	reg &= USBDP_CMN_REG00E3_PCS_CMN_RSTN_CLR;
	reg |= USBDP_CMN_REG00E3_PCS_CMN_RSTN_SET(1);
	reg &= USBDP_CMN_REG00E3_OVRD_PCS_INIT_RSTN_CLR;
	reg |= USBDP_CMN_REG00E3_OVRD_PCS_INIT_RSTN_SET(1);
	reg &= USBDP_CMN_REG00E3_PCS_INIT_RSTN_CLR;
	reg |= USBDP_CMN_REG00E3_PCS_INIT_RSTN_SET(1);
	reg &= USBDP_CMN_REG00E3_OVRD_PCS_LANE_RSTN_CLR;
	reg |= USBDP_CMN_REG00E3_OVRD_PCS_LANE_RSTN_SET(1);
	reg &= USBDP_CMN_REG00E3_PCS_LANE_RSTN_CLR;
	reg |= USBDP_CMN_REG00E3_PCS_LANE_RSTN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E3);

}

static void phy_exynos_usbdp_g2_v2_pma_ovrd_power_on(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	/*
	pcs_des_en = 1
	pcs_tx_ser_en =1
	pcs_tx_elecidle = 0
	0x038C 0xFF
	0x03A4 0x02
	0x03A8 0x83
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E3);
	reg &= USBDP_CMN_REG00E3_OVRD_PCS_DES_EN_CLR;
	reg |= USBDP_CMN_REG00E3_OVRD_PCS_DES_EN_SET(1);
	reg &= USBDP_CMN_REG00E3_PCS_DES_EN_CLR;
	reg |= USBDP_CMN_REG00E3_PCS_DES_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E3);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E9);
	reg &= USBDP_CMN_REG00E9_OVRD_PCS_TX_ELECIDLE_CLR;
	reg |= USBDP_CMN_REG00E9_OVRD_PCS_TX_ELECIDLE_SET(1);
	reg &= USBDP_CMN_REG00E9_PCS_TX_ELECIDLE_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E9);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00EA);
	reg &= USBDP_CMN_REG00EA_OVRD_PCS_TX_RCV_DET_EN_CLR;
	reg &= USBDP_CMN_REG00EA_PCS_TX_RCV_DET_EN_CLR;
	reg &= USBDP_CMN_REG00EA_OVRD_PCS_TX_SER_EN_CLR;
	reg |= USBDP_CMN_REG00EA_OVRD_PCS_TX_SER_EN_SET(1);
	reg &= USBDP_CMN_REG00EA_PCS_TX_SER_EN_CLR;
	reg |= USBDP_CMN_REG00EA_PCS_TX_SER_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00EA);
}

static int phy_exynos_usbdp_g2_v2_pma_bist_en(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	int ret = 0;
	int temp;

	/* dp_lane_en = 0  */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00B9);
	reg &= USBDP_CMN_REG00B9_DP_LANE_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00B9);

	/* dp_bist_en = 0  */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00D6);
	reg &= USBDP_CMN_REG00D6_DP_BIST_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00D6);

	/*
	pcs_bgr_en, pcs_bias_en
	pcs_des_en, pcs_cdr_en
	pcs_pll_en pcs_rx_ctle_en
	pcs_rx_sqhs_en
	pcs_rx_term_en, pcs_tx_drv_en
	pcs_tx_elecidle
	pcs_tx_ser_en, pcs_tx_lfps_en
	ln0_ana_cdr_en
	ln2_ana_cdr_en
	0x0388 0xFB
	0x038C 0xFF
	0x0390 0x60
	0x0394 0x00
	0x0398 0x00
	0x03A4 0x03
	0x03A8 0x83
	0x08B0 0x00
	0x18B0 0x00
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E4);
	reg &= USBDP_CMN_REG00E4_OVRD_PCS_RX_CTLE_EN_CLR;
	reg &= USBDP_CMN_REG00E4_PCS_RX_CTLE_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E4);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E5);
	reg &= USBDP_CMN_REG00E5_OVRD_PCS_RX_SQHS_EN_CLR;
	reg &= USBDP_CMN_REG00E5_PCS_RX_SQHS_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E5);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E6);
	reg &= USBDP_CMN_REG00E6_OVRD_PCS_RX_TERM_EN_CLR;
	reg &= USBDP_CMN_REG00E6_PCS_RX_TERM_EN_CLR;
	reg &= USBDP_CMN_REG00E6_OVRD_PCS_TX_DRV_EN_CLR;
	reg &= USBDP_CMN_REG00E6_PCS_TX_DRV_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E6);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG022C);
	reg &= USBDP_TRSV_REG022C_OVRD_LN0_RX_CDR_EN_CLR;
	reg &= USBDP_TRSV_REG022C_LN0_RX_CDR_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG022C);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG062C);
	reg &= USBDP_TRSV_REG062C_OVRD_LN2_RX_CDR_EN_CLR;
	reg &= USBDP_TRSV_REG062C_LN2_RX_CDR_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG062C);

	/*
	ln0/2_bist_en, ln0/2_bist_tx_en
	ln0/2_bist_data_en
	0x0C00 0xC4
	0x1C00 0xC4
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0300);
	reg &= USBDP_TRSV_REG0300_LN0_BIST_EN_CLR;
	reg |= USBDP_TRSV_REG0300_LN0_BIST_EN_SET(1);
	reg &= USBDP_TRSV_REG0300_LN0_BIST_DATA_EN_CLR;
	reg |= USBDP_TRSV_REG0300_LN0_BIST_DATA_EN_SET(1);
	reg &= USBDP_TRSV_REG0300_LN0_BIST_TX_EN_CLR;
	reg |= USBDP_TRSV_REG0300_LN0_BIST_TX_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0300);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0700);
	reg &= USBDP_TRSV_REG0700_LN2_BIST_EN_CLR;
	reg |= USBDP_TRSV_REG0700_LN2_BIST_EN_SET(1);
	reg &= USBDP_TRSV_REG0700_LN2_BIST_DATA_EN_CLR;
	reg |= USBDP_TRSV_REG0700_LN2_BIST_DATA_EN_SET(1);
	reg &= USBDP_TRSV_REG0700_LN2_BIST_TX_EN_CLR;
	reg |= USBDP_TRSV_REG0700_LN2_BIST_TX_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0700);

	/*
	ln0/2_retimedlb_en
	0x0BF8 0x0A
	0x1BF8 0x0A
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02FE);
	reg &= USBDP_TRSV_REG02FE_LN0_RETIMEDLB_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02FE);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06FE);
	reg &= USBDP_TRSV_REG06FE_LN2_RETIMEDLB_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06FE);

	/*
	ln#_ana_tx_drv_accdrv_en
	ln#_ana_tx_slb_en = 1
	0x081C 0x02
	0x101C 0x02
	0x181C 0x02
	0x201C 0x02
	0x0880 0x00
	0x1080 0x02
	0x1880 0x00
	0x2080 0x02
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0207);
	reg &= USBDP_TRSV_REG0207_LN0_ANA_TX_DRV_ACCDRV_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0207);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0407);
	reg &= USBDP_TRSV_REG0407_LN1_ANA_TX_DRV_ACCDRV_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0407);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0607);
	reg &= USBDP_TRSV_REG0607_LN2_ANA_TX_DRV_ACCDRV_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0607);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0807);
	reg &= USBDP_TRSV_REG0807_LN3_ANA_TX_DRV_ACCDRV_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0807);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0220);
	reg &= USBDP_TRSV_REG0220_LN0_ANA_TX_SLB_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0220);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0420);
	reg &= USBDP_TRSV_REG0420_LN1_ANA_TX_SLB_EN_CLR;
	reg |= USBDP_TRSV_REG0420_LN1_ANA_TX_SLB_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0420);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0620);
	reg &= USBDP_TRSV_REG0620_LN2_ANA_TX_SLB_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0620);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0820);
	reg &= USBDP_TRSV_REG0820_LN3_ANA_TX_SLB_EN_CLR;
	reg |= USBDP_TRSV_REG0820_LN3_ANA_TX_SLB_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0820);

	/*
	ln0/2_bist_auto_run
	0x0BFC 0x80
	0x1BFC 0x80
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02FF);
	reg &= USBDP_TRSV_REG02FF_LN0_BIST_AUTO_RUN_CLR;
	reg |= USBDP_TRSV_REG02FF_LN0_BIST_AUTO_RUN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02FF);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06FF);
	reg &= USBDP_TRSV_REG06FF_LN2_BIST_AUTO_RUN_CLR;
	reg |= USBDP_TRSV_REG06FF_LN2_BIST_AUTO_RUN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06FF);

//	mdelay(5);	// TODO: it is necessary experimentally

	/*
	ln0/2_ana_rx_slb_d_lane_sel
	ln0/2_ana_rx_slb_en
	0x0A10 0x38
	0x1A10 0x38
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0284);
	reg &= USBDP_TRSV_REG0284_LN0_ANA_RX_SLB_D_LANE_SEL_CLR;
	reg |= USBDP_TRSV_REG0284_LN0_ANA_RX_SLB_D_LANE_SEL_SET(1);
	reg &= USBDP_TRSV_REG0284_LN0_ANA_RX_SLB_EN_CLR;
	reg |= USBDP_TRSV_REG0284_LN0_ANA_RX_SLB_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0284);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0684);
	reg &= USBDP_TRSV_REG0684_LN2_ANA_RX_SLB_D_LANE_SEL_CLR;
	reg |= USBDP_TRSV_REG0684_LN2_ANA_RX_SLB_D_LANE_SEL_SET(1);
	reg &= USBDP_TRSV_REG0684_LN2_ANA_RX_SLB_EN_CLR;
	reg |= USBDP_TRSV_REG0684_LN2_ANA_RX_SLB_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0684);

	udelay(10);

	temp = info->used_phy_port;
	info->used_phy_port = 0;
	ret |= phy_exynos_usbdp_g2_v2_pma_check_cdr_lock(info);
	info->used_phy_port = 1;
	ret |= phy_exynos_usbdp_g2_v2_pma_check_cdr_lock(info);
	info->used_phy_port = temp;

	/*
	ln0/2_bist_rx_en
	0x0C00 0xE4
	0x1C00 0xE4
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0300);
	reg &= USBDP_TRSV_REG0300_LN0_BIST_RX_EN_CLR;
	reg |= USBDP_TRSV_REG0300_LN0_BIST_RX_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0300);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0700);
	reg &= USBDP_TRSV_REG0700_LN2_BIST_RX_EN_CLR;
	reg |= USBDP_TRSV_REG0700_LN2_BIST_RX_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0700);

	if (ret)
		return -3;
	else
		return 0;
}

static int phy_exynos_usbdp_g2_v2_pma_bist_result(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 pass_flag = 0, start_flag = 0;

	/*
	LN0 BIST pass/start flag
	LN2 BIST pass/start flag
	0x0F20 (Read)
	0x1F20 (Read)
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03C8);
	pass_flag = USBDP_TRSV_REG03C8_LN0_MON_BIST_COMP_TEST_GET(reg);
	start_flag = USBDP_TRSV_REG03C8_LN0_MON_BIST_COMP_START_GET(reg);

	if (!(pass_flag & start_flag))
		return -4;

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07C8);
	pass_flag = USBDP_TRSV_REG07C8_LN2_MON_BIST_COMP_TEST_GET(reg);
	start_flag = USBDP_TRSV_REG07C8_LN2_MON_BIST_COMP_START_GET(reg);

	if (!(pass_flag & start_flag))
		return -5;

	return 0;
}

int phy_exynos_usbdp_g2_v2_internal_loopback(struct exynos_usbphy_info *info, u32 cmn_rate)
{
	int ret = 0;
	int temp;

	phy_exynos_usbdp_g2_v2_ctrl_pma_ready(info);
	phy_exynos_usbdp_g2_v2_aux_force_off(info);
	phy_exynos_usbdp_g2_v2_pma_default_sfr_update(info);
	phy_exynos_usbdp_g2_v2_tune(info);

	phy_exynos_usbdp_g2_v2_pma_ovrd_enable(info, cmn_rate);
	phy_exynos_usbdp_g2_v2_ctrl_pma_rst_release(info);
	phy_exynos_usbdp_g2_v2_pma_ovrd_pcs_rst_release(info);

	do {
		ret = phy_exynos_usbdp_g2_v2_pma_check_pll_lock(info);
		if (ret)
			break;

		phy_exynos_usbdp_g2_v2_pma_ovrd_power_on(info);

		temp = info->used_phy_port;
		info->used_phy_port = 0;
		ret = phy_exynos_usbdp_g2_v2_pma_check_cdr_lock(info);
		info->used_phy_port = temp;
		if (ret)
			break;

		info->used_phy_port = 1;
		ret = phy_exynos_usbdp_g2_v2_pma_check_cdr_lock(info);
		info->used_phy_port = temp;
		if (ret)
			break;

		mdelay(10);	// it is necessary experimentally

		ret = phy_exynos_usbdp_g2_v2_pma_bist_en(info);
		if (ret)
			break;

		udelay(100);

		ret = phy_exynos_usbdp_g2_v2_pma_bist_result(info);
		if (ret)
			break;
	} while (0);

	return ret;
}

void phy_exynos_usbdp_g2_v2_eom_init(struct exynos_usbphy_info *info, u32 cmn_rate)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM Sample Number ( 2^10 )  ----> applied 2^(sample_number)
	if (info->used_phy_port == 0) {
		/*
		0x0B7C 0x00	ln0_rx_efom_num_of_sample__13_8 = 0
		0x0B80 0x0A	ln0_rx_efom_num_of_sample__7_0 = 0xa
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DF);
		reg &= USBDP_TRSV_REG02DF_LN0_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		reg |= USBDP_TRSV_REG02DF_LN0_RX_EFOM_NUM_OF_SAMPLE__13_8_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02E0);
		reg &= USBDP_TRSV_REG02E0_LN0_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		reg |= USBDP_TRSV_REG02E0_LN0_RX_EFOM_NUM_OF_SAMPLE__7_0_SET(0xa);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02E0);
	} else {
		/*
		0x1B7C 0x00	ln2_rx_efom_num_of_sample__13_8 = 0
		0x1B80 0x0A	ln2_rx_efom_num_of_sample__7_0 = 0xa
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DF);
		reg &= USBDP_TRSV_REG06DF_LN2_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		reg |= USBDP_TRSV_REG06DF_LN2_RX_EFOM_NUM_OF_SAMPLE__13_8_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06E0);
		reg &= USBDP_TRSV_REG06E0_LN2_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		reg |= USBDP_TRSV_REG06E0_LN2_RX_EFOM_NUM_OF_SAMPLE__7_0_SET(0xa);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06E0);
	}

	// ovrd adap en, eom_en
	if (info->used_phy_port == 0) {
		/*
		0x09D0 0x01	ovrd_ln0_rx_dfe_vref_odd_ctrl = 1
		0x09D8 0x01	ovrd_ln0_rx_dfe_vref_even_ctrl = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0274);
		reg |= USBDP_TRSV_REG0274_OVRD_LN0_RX_DFE_VREF_ODD_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0274);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0276);
		reg |= USBDP_TRSV_REG0276_OVRD_LN0_RX_DFE_VREF_EVEN_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0276);
		/*
		0x099C 0x2F	ovrd_ln0_rx_dfe_adap_en = 1
					ln0_rx_dfe_adap_en = 0
					ovrd_ln0_rx_dfe_eom_en = 1
					ln0_rx_dfe_eom_en = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0267);
		reg |= USBDP_TRSV_REG0267_OVRD_LN0_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_TRSV_REG0267_LN0_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG0267_OVRD_LN0_RX_DFE_EOM_EN_SET(1);
		reg |= USBDP_TRSV_REG0267_LN0_RX_DFE_EOM_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0267);
	} else {
		/*
		0x19D0 0x01	ovrd_ln2_rx_dfe_vref_odd_ctrl = 1
		0x19D8 0x01	ovrd_ln2_rx_dfe_vref_even_ctrl = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0674);
		reg |= USBDP_TRSV_REG0674_OVRD_LN2_RX_DFE_VREF_ODD_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0674);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0676);
		reg |= USBDP_TRSV_REG0676_OVRD_LN2_RX_DFE_VREF_EVEN_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0676);

		/*
		0x199C 0x2F	ovrd_ln2_rx_dfe_adap_en = 1
					ln2_rx_dfe_adap_en = 0
					ovrd_ln2_rx_dfe_eom_en = 1
					ln2_rx_dfe_eom_en = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0667);
		reg |= USBDP_TRSV_REG0667_OVRD_LN2_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_TRSV_REG0667_LN2_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG0667_OVRD_LN2_RX_DFE_EOM_EN_SET(1);
		reg |= USBDP_TRSV_REG0667_LN2_RX_DFE_EOM_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0667);
	}

	// PI STR ( 0 min, f:max )
	if (info->used_phy_port == 0) {
		/*
		0x09AC 0x03	ln0_ana_rx_dfe_eom_pi_str_ctrl = 3
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG026B);
		reg &= USBDP_TRSV_REG026B_LN0_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		reg |= USBDP_TRSV_REG026B_LN0_ANA_RX_DFE_EOM_PI_STR_CTRL_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG026B);
	} else {
		/*
		0x19AC 0x03	ln2_ana_rx_dfe_eom_pi_str_ctrl = 3
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG066B);
		reg &= USBDP_TRSV_REG066B_LN2_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		reg |= USBDP_TRSV_REG066B_LN2_ANA_RX_DFE_EOM_PI_STR_CTRL_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG066B);
	}


	// EOM MODE
	if (info->used_phy_port == 0) {
		/*
		0x0B70 0x10	ln0_rx_efom_mode = 4
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DC);
		reg &= USBDP_TRSV_REG02DC_LN0_RX_EFOM_MODE_CLR;
		reg |= USBDP_TRSV_REG02DC_LN0_RX_EFOM_MODE_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DC);
	} else {
		/*
		0x1B70 0x10	ln2_rx_efom_mode = 4
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DC);
		reg &= USBDP_TRSV_REG06DC_LN2_RX_EFOM_MODE_CLR;
		reg |= USBDP_TRSV_REG06DC_LN2_RX_EFOM_MODE_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DC);
	}

	// EOM START SSM DISABLE
	if (info->used_phy_port == 0) {
		/*
		0x0B74 0x10	ln0_rx_efom_start_ssm_disable = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DD);
		reg &= USBDP_TRSV_REG02DD_LN0_RX_EFOM_START_SSM_DISABLE_CLR;
		reg |= USBDP_TRSV_REG02DD_LN0_RX_EFOM_START_SSM_DISABLE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DD);
	} else {
		/*
		0x1B74 0x10	ln2_rx_efom_start_ssm_disable = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DD);
		reg &= USBDP_TRSV_REG06DD_LN2_RX_EFOM_START_SSM_DISABLE_CLR;
		reg |= USBDP_TRSV_REG06DD_LN2_RX_EFOM_START_SSM_DISABLE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DD);
	}

	// PCS EOM INPUT
	/*
	0x0394 0x0C	ovrd_pcs_rx_fom_en = 1
				pcs_rx_fom_en = 1
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E5);
	reg |= USBDP_CMN_REG00E5_OVRD_PCS_RX_FOM_EN_SET(1);
	reg |= USBDP_CMN_REG00E5_PCS_RX_FOM_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E5);

	// NON DATA, DE-SER EN
	if (info->used_phy_port == 0) {
		/*
		0x0978 0x2C	ovrd_ln0_rx_des_non_data_sel = 1
					ln0_rx_des_non_data_sel = 0
					ovrd_ln0_rx_des_rstn = 1
					ln0_rx_des_rstn = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG025E);
		reg |= USBDP_TRSV_REG025E_OVRD_LN0_RX_DES_NON_DATA_SEL_SET(1);
		reg |= USBDP_TRSV_REG025E_OVRD_LN0_RX_DES_RSTN_SET(1);
		reg |= USBDP_TRSV_REG025E_LN0_RX_DES_RSTN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG025E);
	} else {
		/*
		0x1978 0x2C	ovrd_ln2_rx_des_non_data_sel = 1
					ln2_rx_des_non_data_sel = 0
					ovrd_ln2_rx_des_rstn = 1
					ln2_rx_des_rstn = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG065E);
		reg |= USBDP_TRSV_REG065E_OVRD_LN2_RX_DES_NON_DATA_SEL_SET(1);
		reg |= USBDP_TRSV_REG065E_OVRD_LN2_RX_DES_RSTN_SET(1);
		reg |= USBDP_TRSV_REG065E_LN2_RX_DES_RSTN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG065E);
	}

	// EOM CLOCK DIV  <----------------- Need to Confirm about value, it just predicted value
	/* it was set at phy_exynos_usbdp_g2_v2_pma_default_sfr_update
	0x09A0 0x24
	0x19A0 0x24
	 */

	// EOM BIT WIDTH  <----------------- Need to Confirm about value, it just predicted value
	if (info->used_phy_port == 0) {
		/*
		Gen2
		0x0B78 0x47	ln0_rx_efom_settle_time = 4
					ln0_rx_efom_bit_width_sel = 1

		Gen1
		0x0B78 0x4F	ln0_rx_efom_settle_time = 4
					ln0_rx_efom_bit_width_sel = 3
		 */
		if (cmn_rate) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DE);
			reg &= USBDP_TRSV_REG02DE_LN0_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_TRSV_REG02DE_LN0_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_TRSV_REG02DE_LN0_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_TRSV_REG02DE_LN0_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DE);
		} else {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DE);
			reg &= USBDP_TRSV_REG02DE_LN0_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_TRSV_REG02DE_LN0_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_TRSV_REG02DE_LN0_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_TRSV_REG02DE_LN0_RX_EFOM_BIT_WIDTH_SEL_SET(0x3);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DE);
		}
	} else {
		/*
		Gen2
		0x1B78 0x47	ln2_rx_efom_settle_time = 4
					ln2_rx_efom_bit_width_sel = 1

		Gen1
		0x1B78 0x4F	ln2_rx_efom_settle_time = 4
					ln2_rx_efom_bit_width_sel = 3
		 */
		if (cmn_rate) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DE);
			reg &= USBDP_TRSV_REG06DE_LN2_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_TRSV_REG06DE_LN2_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_TRSV_REG06DE_LN2_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_TRSV_REG06DE_LN2_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DE);
		} else {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DE);
			reg &= USBDP_TRSV_REG06DE_LN2_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_TRSV_REG06DE_LN2_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_TRSV_REG06DE_LN2_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_TRSV_REG06DE_LN2_RX_EFOM_BIT_WIDTH_SEL_SET(0x3);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DE);
		}
	}

	// Switch to 6:New EOM, E:legacy EFOM mode
	/*
	0x0450 0x06	efom_legacy_mode_en = 0
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG0114);
	reg &= USBDP_CMN_REG0114_EFOM_LEGACY_MODE_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG0114);
}

void phy_exynos_usbdp_g2_v2_eom_deinit(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM START ovrd clear
	if (info->used_phy_port == 0) {
		/*
		0x0B70 0x10	ln0_ovrd_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DC);
		reg &= USBDP_TRSV_REG02DC_LN0_OVRD_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DC);
	} else {
		/*
		0x1B70 0x10	ln2_ovrd_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DC);
		reg &= USBDP_TRSV_REG06DC_LN2_OVRD_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DC);
	}


	// EOM Sample Number clear
	if (info->used_phy_port == 0) {
		/*
		0x0B7C 0x00	ln0_rx_efom_num_of_sample__13_8 = 0
		0x0B80 0x00	ln0_rx_efom_num_of_sample__7_0 = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DF);
		reg &= USBDP_TRSV_REG02DF_LN0_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02E0);
		reg &= USBDP_TRSV_REG02E0_LN0_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02E0);
	} else {
		/*
		0x1B7C 0x00	ln2_rx_efom_num_of_sample__13_8 = 0
		0x1B80 0x00	ln2_rx_efom_num_of_sample__7_0 = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DF);
		reg &= USBDP_TRSV_REG06DF_LN2_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DF);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06E0);
		reg &= USBDP_TRSV_REG06E0_LN2_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06E0);
	}

	// ovrd adap en, eom_en
	if (info->used_phy_port == 0) {
		/*
		0x09D0 0x00	ovrd_ln0_rx_dfe_vref_odd_ctrl = 0
		0x09D8 0x00	ovrd_ln0_rx_dfe_vref_even_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0274);
		reg &= USBDP_TRSV_REG0274_OVRD_LN0_RX_DFE_VREF_ODD_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0274);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0276);
		reg &= USBDP_TRSV_REG0276_OVRD_LN0_RX_DFE_VREF_EVEN_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0276);

		/*
		0x099C 0x1C	ovrd_ln0_rx_dfe_adap_en = 0
					ln0_rx_dfe_adap_en = 1
					ovrd_ln0_rx_dfe_eom_en = 0
					ln0_rx_dfe_eom_en = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0267);
		reg &= USBDP_TRSV_REG0267_OVRD_LN0_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG0267_LN0_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_TRSV_REG0267_OVRD_LN0_RX_DFE_EOM_EN_CLR;
		reg &= USBDP_TRSV_REG0267_LN0_RX_DFE_EOM_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0267);
	} else {
		/*
		0x19D0 0x00	ovrd_ln2_rx_dfe_vref_odd_ctrl = 0
		0x19D8 0x00	ovrd_ln2_rx_dfe_vref_even_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0674);
		reg &= USBDP_TRSV_REG0674_OVRD_LN2_RX_DFE_VREF_ODD_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0674);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0676);
		reg &= USBDP_TRSV_REG0676_OVRD_LN2_RX_DFE_VREF_EVEN_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0676);

		/*
		0x099C 0x1C	ovrd_ln0_rx_dfe_adap_en = 0
					ln0_rx_dfe_adap_en = 1
					ovrd_ln0_rx_dfe_eom_en = 0
					ln0_rx_dfe_eom_en = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0667);
		reg &= USBDP_TRSV_REG0667_OVRD_LN2_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_TRSV_REG0667_LN2_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_TRSV_REG0667_OVRD_LN2_RX_DFE_EOM_EN_CLR;
		reg &= USBDP_TRSV_REG0667_LN2_RX_DFE_EOM_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0667);
	}

	// PI STR ( 0 min, f:max )
	if (info->used_phy_port == 0) {
		/*
		0x09AC 0x00	ln0_ana_rx_dfe_eom_pi_str_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG026B);
		reg &= USBDP_TRSV_REG026B_LN0_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG026B);
	} else {
		/*
		0x19AC 0x00	ln2_ana_rx_dfe_eom_pi_str_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG066B);
		reg &= USBDP_TRSV_REG066B_LN2_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG066B);
	}

	// EOM MODE
	if (info->used_phy_port == 0) {
		/*
		0x0B70 0x00	ln0_rx_efom_mode = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DC);
		reg &= USBDP_TRSV_REG02DC_LN0_RX_EFOM_MODE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DC);
	} else {
		/*
		0x1B70 0x00	ln2_rx_efom_mode = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DC);
		reg &= USBDP_TRSV_REG06DC_LN2_RX_EFOM_MODE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DC);
	}

	// EOM START SSM DISABLE
	if (info->used_phy_port == 0) {
		/*
		0x0B74 0x00	ln0_rx_efom_start_ssm_disable = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DD);
		reg &= USBDP_TRSV_REG02DD_LN0_RX_EFOM_START_SSM_DISABLE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DD);
	} else {
		/*
		0x1B74 0x00	ln2_rx_efom_start_ssm_disable = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DD);
		reg &= USBDP_TRSV_REG06DD_LN2_RX_EFOM_START_SSM_DISABLE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DD);
	}

	// PCS EOM INPUT
	/*
	0x0394 0x00	ovrd_pcs_rx_fom_en = 0
				pcs_rx_fom_en = 0
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG00E5);
	reg &= USBDP_CMN_REG00E5_OVRD_PCS_RX_FOM_EN_CLR;
	reg &= USBDP_CMN_REG00E5_PCS_RX_FOM_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_CMN_REG00E5);

	// NON DATA, DE-SER EN
	if (info->used_phy_port == 0) {
		/*
		0x0978 0x00	ovrd_ln0_rx_des_non_data_sel = 0
					ovrd_ln0_rx_des_rstn = 0
					ln0_rx_des_rstn = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG025E);
		reg &= USBDP_TRSV_REG025E_OVRD_LN0_RX_DES_NON_DATA_SEL_CLR;
		reg &= USBDP_TRSV_REG025E_OVRD_LN0_RX_DES_RSTN_CLR;
		reg &= USBDP_TRSV_REG025E_LN0_RX_DES_RSTN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG025E);
	} else {
		/*
		0x1978 0x00	ovrd_ln2_rx_des_non_data_sel = 0
					ovrd_ln2_rx_des_rstn = 0
					ln2_rx_des_rstn = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG065E);
		reg &= USBDP_TRSV_REG065E_OVRD_LN2_RX_DES_NON_DATA_SEL_CLR;
		reg &= USBDP_TRSV_REG065E_OVRD_LN2_RX_DES_RSTN_CLR;
		reg &= USBDP_TRSV_REG065E_LN2_RX_DES_RSTN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG065E);
	}

	// EOM BIT WIDTH
	if (info->used_phy_port == 0) {
		/*
		0x0B78 0x07	ln0_rx_efom_settle_time = 0
					ln0_rx_efom_bit_width_sel = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DE);
		reg &= USBDP_TRSV_REG02DE_LN0_RX_EFOM_SETTLE_TIME_CLR;
		reg &= USBDP_TRSV_REG02DE_LN0_RX_EFOM_BIT_WIDTH_SEL_CLR;
		reg |= USBDP_TRSV_REG02DE_LN0_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DE);
	} else {
		/*
		0x1B78 0x07	ln2_rx_efom_settle_time = 0
					ln2_rx_efom_bit_width_sel = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DE);
		reg &= USBDP_TRSV_REG06DE_LN2_RX_EFOM_SETTLE_TIME_CLR;
		reg &= USBDP_TRSV_REG06DE_LN2_RX_EFOM_BIT_WIDTH_SEL_CLR;
		reg |= USBDP_TRSV_REG06DE_LN2_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DE);
	}
}

void phy_exynos_usbdp_g2_v2_eom_start(struct exynos_usbphy_info *info, u32 ph_sel, u32 def_vref)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM PHASE SETTING
	if (info->used_phy_port == 0) {
		/*
		0x0B94	ln0_rx_efom_eom_ph_sel
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02E5);
		reg &= USBDP_TRSV_REG02E5_LN0_RX_EFOM_EOM_PH_SEL_CLR;
		reg |= USBDP_TRSV_REG02E5_LN0_RX_EFOM_EOM_PH_SEL_SET(ph_sel);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02E5);
	} else {
		/*
		0x1B94	ln2_rx_efom_eom_ph_sel
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06E5);
		reg &= USBDP_TRSV_REG06E5_LN2_RX_EFOM_EOM_PH_SEL_CLR;
		reg |= USBDP_TRSV_REG06E5_LN2_RX_EFOM_EOM_PH_SEL_SET(ph_sel);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06E5);
	}

	// EOM VREF SETTING
	if (info->used_phy_port == 0) {
		/*
		0x09D4	ln0_rx_dfe_vref_odd_ctrl__7_0
		0x09DC	ln0_rx_dfe_vref_even_ctrl__7_0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0275);
		reg &= USBDP_TRSV_REG0275_LN0_RX_DFE_VREF_ODD_CTRL__7_0_CLR;
		reg |= USBDP_TRSV_REG0275_LN0_RX_DFE_VREF_ODD_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0275);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0277);
		reg &= USBDP_TRSV_REG0277_LN0_RX_DFE_VREF_EVEN_CTRL__7_0_CLR;
		reg |= USBDP_TRSV_REG0277_LN0_RX_DFE_VREF_EVEN_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0277);
	} else {
		/*
		0x19D4	ln2_rx_dfe_vref_odd_ctrl__7_0
		0x19DC	ln2_rx_dfe_vref_even_ctrl__7_0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0675);
		reg &= USBDP_TRSV_REG0675_LN2_RX_DFE_VREF_ODD_CTRL__7_0_CLR;
		reg |= USBDP_TRSV_REG0675_LN2_RX_DFE_VREF_ODD_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0675);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG0677);
		reg &= USBDP_TRSV_REG0677_LN2_RX_DFE_VREF_EVEN_CTRL__7_0_CLR;
		reg |= USBDP_TRSV_REG0677_LN2_RX_DFE_VREF_EVEN_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG0677);
	}

	// EOM START
	if (info->used_phy_port == 0) {
		/*
		0x0B70 0x13	ln0_ovrd_rx_efom_start = 1
					ln0_rx_efom_start = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DC);
		reg |= USBDP_TRSV_REG02DC_LN0_OVRD_RX_EFOM_START_SET(1);
		reg |= USBDP_TRSV_REG02DC_LN0_RX_EFOM_START_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DC);
	} else {
		/*
		0x1B70 0x13	ln2_ovrd_rx_efom_start = 1
					ln2_rx_efom_start = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DC);
		reg |= USBDP_TRSV_REG06DC_LN2_OVRD_RX_EFOM_START_SET(1);
		reg |= USBDP_TRSV_REG06DC_LN2_RX_EFOM_START_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DC);
	}
}

int phy_exynos_usbdp_g2_v2_eom_get_done_status(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 eom_done = 0;

	// Check efom_done
	if (info->used_phy_port == 0) {
		/*
		0x0F98	ln0_mon_rx_efom_done
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG03E6);
		eom_done = USBDP_TRSV_REG03E6_LN0_MON_RX_EFOM_DONE_GET(reg);
	} else {
		/*
		0x1F98	ln2_mon_rx_efom_done
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG07E6);
		eom_done = USBDP_TRSV_REG07E6_LN2_MON_RX_EFOM_DONE_GET(reg);
	}

	if (eom_done)
		return 0;
	else
		return -1;
}

u64 phy_exynos_usbdp_g2_v2_eom_get_err_cnt(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u64 err_cnt_lo = 0;
	u64 err_cnt_hi = 0;

	// Get error count
	if (info->used_phy_port == 0) {
		/*
		0x077C	ln0_mon_rx_efom_err_cnt__7_0
		0x0778	ln0_mon_rx_efom_err_cnt__15_8
		0x0774	ln0_mon_rx_efom_err_cnt__23_16
		0x0770	ln0_mon_rx_efom_err_cnt__31_24
		0x076C	ln0_mon_rx_efom_err_cnt__39_32
		0x0768	ln0_mon_rx_efom_err_cnt__47_40
		0x0764	ln0_mon_rx_efom_err_cnt__52_48
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01DF);
		err_cnt_lo |= USBDP_CMN_REG01DF_LN0_MON_RX_EFOM_ERR_CNT__7_0_GET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01DE);
		err_cnt_lo |= USBDP_CMN_REG01DE_LN0_MON_RX_EFOM_ERR_CNT__15_8_GET(reg) << 8;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01DD);
		err_cnt_lo |= USBDP_CMN_REG01DD_LN0_MON_RX_EFOM_ERR_CNT__23_16_GET(reg) << 16;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01DC);
		err_cnt_lo |= USBDP_CMN_REG01DC_LN0_MON_RX_EFOM_ERR_CNT__31_24_GET(reg) << 24;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01DB);
		err_cnt_hi |= USBDP_CMN_REG01DB_LN0_MON_RX_EFOM_ERR_CNT__39_32_GET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01DA);
		err_cnt_hi |= USBDP_CMN_REG01DA_LN0_MON_RX_EFOM_ERR_CNT__47_40_GET(reg) << 4;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01D9);
		err_cnt_hi |= USBDP_CMN_REG01D9_LN0_MON_RX_EFOM_ERR_CNT__52_48_GET(reg) << 8;
	} else {
		/*
		0x0798	ln2_mon_rx_efom_err_cnt__7_0
		0x0794	ln2_mon_rx_efom_err_cnt__15_8
		0x0790	ln2_mon_rx_efom_err_cnt__23_16
		0x078c	ln2_mon_rx_efom_err_cnt__31_24
		0x0788	ln2_mon_rx_efom_err_cnt__39_32
		0x0784	ln2_mon_rx_efom_err_cnt__47_40
		0x0780	ln2_mon_rx_efom_err_cnt__52_48
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E6);
		err_cnt_lo |= USBDP_CMN_REG01E6_LN2_MON_RX_EFOM_ERR_CNT__7_0_GET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E5);
		err_cnt_lo |= USBDP_CMN_REG01E5_LN2_MON_RX_EFOM_ERR_CNT__15_8_GET(reg) << 8;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E4);
		err_cnt_lo |= USBDP_CMN_REG01E4_LN2_MON_RX_EFOM_ERR_CNT__23_16_GET(reg) << 16;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E3);
		err_cnt_lo |= USBDP_CMN_REG01E3_LN2_MON_RX_EFOM_ERR_CNT__31_24_GET(reg) << 24;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E2);
		err_cnt_hi |= USBDP_CMN_REG01E2_LN2_MON_RX_EFOM_ERR_CNT__39_32_SET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E1);
		err_cnt_hi |= USBDP_CMN_REG01E1_LN2_MON_RX_EFOM_ERR_CNT__47_40_GET(reg) << 4;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_CMN_REG01E0);
		err_cnt_hi |= USBDP_CMN_REG01E0_LN2_MON_RX_EFOM_ERR_CNT__52_48_SET(reg) << 8;
	}

	return err_cnt_hi << 32 | err_cnt_lo;
}

void phy_exynos_usbdp_g2_v2_eom_stop(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM STOP
	if (info->used_phy_port == 0) {
		/*
		0x0B70 0x12	ln0_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG02DC);
		reg &= USBDP_TRSV_REG02DC_LN0_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG02DC);
	} else {
		/*
		0x1B70 0x12	ln2_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_TRSV_REG06DC);
		reg &= USBDP_TRSV_REG06DC_LN2_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_TRSV_REG06DC);
	}
}

void phy_exynos_usbdp_g2_v2_eom(struct exynos_usbphy_info *info, u32 cmn_rate, struct usb_eom_result_s *eom_result)
{
	u32 ph_sel, def_vref = 0;
	u64 err_cnt = 0;
	u32 test_cnt = 0;

	phy_exynos_usbdp_g2_v2_eom_init(info, cmn_rate);	// 0: Gen1, 1: Gen2

	for (ph_sel = 0; ph_sel < EOM_PH_SEL_MAX; ph_sel++) {
		for (def_vref = 0; def_vref < EOM_DEF_VREF_MAX; def_vref++) {
			phy_exynos_usbdp_g2_v2_eom_start(info, ph_sel, def_vref);
			while (phy_exynos_usbdp_g2_v2_eom_get_done_status(info));
			err_cnt = phy_exynos_usbdp_g2_v2_eom_get_err_cnt(info);
			phy_exynos_usbdp_g2_v2_eom_stop(info);

			// Save result
			eom_result[test_cnt].phase = ph_sel;
			eom_result[test_cnt].vref = def_vref;
			eom_result[test_cnt].err = err_cnt;
			test_cnt++;
		}
	}

	phy_exynos_usbdp_g2_v2_eom_deinit(info);
}

int phy_exynos_usbdp_g2_v2_enable(struct exynos_usbphy_info *info)
{
	int ret = 0;

	phy_exynos_usbdp_g2_v2_ctrl_pma_ready(info);
	phy_exynos_usbdp_g2_v2_aux_force_off(info);
	phy_exynos_usbdp_g2_v2_pma_default_sfr_update(info);
	phy_exynos_usbdp_g2_v2_pma_lane_mux_sel(info);
	phy_exynos_usbdp_g2_v2_ctrl_pma_rst_release(info);

	ret = phy_exynos_usbdp_g2_v2_pma_check_pll_lock(info);
	if (!ret) {
		ret = phy_exynos_usbdp_g2_v2_pma_check_cdr_lock(info);
	}

	mdelay(10);

	phy_exynos_usbdp_g2_v2_set_pcs(info);
	phy_exynos_usbdp_g2_v2_tune(info);
	phy_exynos_usbdp_g2_v2_pma_check_offset_cal_code(info);

	return ret;
}

void phy_exynos_usbdp_g2_v2_disable(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->ctrl_base;
	u32 reg;

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_ROPLL_REF_CLK_SEL_MASK;     // ropll refclk off for reducing powerdown current
	reg |= PMA_ROPLL_REF_CLK_SEL_SET(1);
	reg &= ~PMA_LCPLL_REF_CLK_SEL_MASK;     // lcpll refclk off for reducing powerdown current
	reg |= PMA_LCPLL_REF_CLK_SEL_SET(1);
	reg |= PMA_LOW_PWR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
}
