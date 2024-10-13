// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/iopoll.h>
#include <cal_config.h>
#include <regs-dqe.h>
#include <regs-dpp.h>
#include <regs-decon.h>
#include <dqe_cal.h>
#include <decon_cal.h>
#include <linux/dma-buf.h>

static struct cal_regs_desc regs_dqe[REGS_DQE_TYPE_MAX][REGS_DQE_ID_MAX];

#define dqe_regs_desc(id)			(&regs_dqe[REGS_DQE][id])
#define dqe_read(id, offset)	cal_read(dqe_regs_desc(id), offset)
#define dqe_write(id, offset, val)	cal_write(dqe_regs_desc(id), offset, val)
#define dqe_read_mask(id, offset, mask)		\
		cal_read_mask(dqe_regs_desc(id), offset, mask)
#define dqe_write_mask(id, offset, val, mask)	\
		cal_write_mask(dqe_regs_desc(id), offset, val, mask)
#define dqe_write_relaxed(id, offset, val)		\
		cal_write_relaxed(dqe_regs_desc(id), offset, val)

#define dma_regs_desc(id)			(&regs_dqe[REGS_EDMA][id])
#define dma_read(id, offset)	cal_read(dma_regs_desc(id), offset)
#define dma_write(id, offset, val)	cal_write(dma_regs_desc(id), offset, val)
#define dma_read_mask(id, offset, mask)		\
		cal_read_mask(dma_regs_desc(id), offset, mask)
#define dma_write_mask(id, offset, val, mask)	\
		cal_write_mask(dma_regs_desc(id), offset, val, mask)
#define dma_write_relaxed(id, offset, val)		\
		cal_write_relaxed(dma_regs_desc(id), offset, val)

/****************** EDMA CAL functions ******************/
void edma_reg_set_irq_mask_all(u32 id, u32 en)
{
#if defined(EDMA_CGC_IRQ)
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, EDMA_CGC_IRQ, val, CGC_ALL_IRQ_MASK);
#endif
}

void edma_reg_set_irq_enable(u32 id, u32 en)
{
#if defined(EDMA_CGC_IRQ)
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, EDMA_CGC_IRQ, val, CGC_IRQ_ENABLE);
#endif
}

void edma_reg_clear_irq_all(u32 id)
{
#if defined(EDMA_CGC_IRQ)
	dma_write_mask(id, EDMA_CGC_IRQ, ~0, CGC_ALL_IRQ_CLEAR);
#endif
}

u32 edma_reg_get_interrupt_and_clear(u32 id)
{
	u32 val = 0;

#if defined(EDMA_CGC_IRQ)
	val = dqe_read(id, EDMA_CGC_IRQ);
	edma_reg_clear_irq_all(id);

	if (val & CGC_CONFIG_ERR_IRQ)
		cal_log_err(id, "CGC_CONFIG_ERR_IRQ\n");
	if (val & CGC_READ_SLAVE_ERR_IRQ)
		cal_log_err(id, "CGC_READ_SLAVE_ERR_IRQ\n");
	if (val & CGC_DEADLOCK_IRQ)
		cal_log_err(id, "CGC_DEADLOCK_IRQ\n");
	if (val & CGC_FRAME_DONE_IRQ)
		cal_log_info(id, "CGC_FRAME_DONE_IRQ\n");
#endif
	return val;
}

void edma_reg_set_sw_reset(u32 id)
{
#if defined(EDMA_CGC_ENABLE)
	dma_write_mask(id, EDMA_CGC_ENABLE, ~0, CGC_SRESET);
#endif
}

void edma_reg_set_start(u32 id, u32 en)
{
#if defined(EDMA_CGC_ENABLE) && defined(EDMA_CGC_IN_CTRL_1)
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, EDMA_CGC_IN_CTRL_1, val, CGC_START_EN_SET0);
	dma_write_mask(id, EDMA_CGC_ENABLE, val, CGC_START);
#endif
}

void edma_reg_set_base_addr(u32 id, dma_addr_t addr)
{
#if defined(EDMA_CGC_BASE_ADDR_SET0)
	dma_write(id, EDMA_CGC_BASE_ADDR_SET0, addr);
#endif
}

void __dqe_dump(u32 id, struct dqe_regs *regs, bool en_lut)
{
	void __iomem *dqe_base_regs = regs->dqe_base_regs;
	void __iomem *edma_base_regs = regs->edma_base_regs;

	cal_log_info(id, "=== DQE_TOP SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x0000, 0x100);
	cal_log_info(id, "=== DQE_TOP SHADOW SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x0000 + SHADOW_DQE_OFFSET, 0x100);

	cal_log_info(id, "=== DQE_ATC SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x400, 0x84);
	cal_log_info(id, "=== DQE_ATC SHADOW SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x400 + SHADOW_DQE_OFFSET, 0x84);

	cal_log_info(id, "=== DQE_HSC SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x800, 0x760);

	cal_log_info(id, "=== DQE_GAMMA_MATRIX SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x1400, 0x20);

	cal_log_info(id, "=== DQE_DEGAMMA SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x1800, 0x100);

	cal_log_info(id, "=== DQE_CGC SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2000, 0x40);

	cal_log_info(id, "=== DQE_REGAMMA SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2400, 0x200);

	cal_log_info(id, "=== DQE_CGC_DITHER SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2800, 0x20);

	cal_log_info(id, "=== DQE_DISP_DITHER SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2C00, 0x20);

	cal_log_info(id, "=== DQE_RCD SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x3000, 0x20);

	cal_log_info(id, "=== DQE_DE SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x3200, 0x20);

	cal_log_info(id, "=== DQE_SCL SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x3400, 0x200);

	if (en_lut) {
		cal_log_info(id, "=== DQE_CGC_LUT SFR DUMP ===\n");
		cal_log_info(id, "== [ RED CGC LUT ] ==\n");
		dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x4000, 0x2664);
		cal_log_info(id, "== [ GREEN CGC LUT ] ==\n");
		dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x8000, 0x2664);
		cal_log_info(id, "== [ BLUE CGC LUT ] ==\n");
		dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0xC000, 0x2664);
	}

	if (edma_base_regs) {
		cal_log_info(id, "=== EDMA CGC SFR DUMP ===\n");
		dpu_print_hex_dump(edma_base_regs, edma_base_regs + 0x0000, 0x48);
	}
}

void dqe_reg_set_cgc_lut(u16 (*ctx)[3], u32 (*lut)[DQE_CGC_LUT_MAX])
{
	int i, rgb;

	for (i = 0; i < DQE_CGC_LUT_MAX; i++)
		for (rgb = 0; rgb < 3; rgb++)
			ctx[i][rgb] = (u16)lut[rgb][i]; // diff align between SFR and DMA
}

void dqe_reg_set_cgc_con(u32 *ctx, u32 *lut)
{
	/* DQE0_CGC_MC1_R0*/
	ctx[0] = (
		CGC_MC_ON_R(lut[1]) | CGC_MC_INVERSE_R(lut[2]) |
		CGC_MC_GAIN_R(lut[3]) | CGC_MC_BC_HUE_R(lut[4]) |
		CGC_MC_BC_SAT_R(lut[5]) | CGC_MC_BC_VAL_R(lut[6])
	);

	/* Dummy */
	ctx[1] = 0;

	/* DQE0_CGC_MC3_R0*/
	ctx[2] = (CGC_MC_S1_R(lut[7]) | CGC_MC_S2_R(lut[8]));

	/* DQE0_CGC_MC4_R0*/
	ctx[3] = (CGC_MC_H1_R(lut[9]) | CGC_MC_H2_R(lut[10]));

	/* DQE0_CGC_MC5_R0*/
	ctx[4] = (CGC_MC_V1_R(lut[11]) | CGC_MC_V2_R(lut[12]));

	/* DQE0_CGC_MC1_R1*/
	ctx[5] = (
		CGC_MC_ON_R(lut[13]) | CGC_MC_INVERSE_R(lut[14]) |
		CGC_MC_GAIN_R(lut[15]) | CGC_MC_BC_HUE_R(lut[16]) |
		CGC_MC_BC_SAT_R(lut[17]) | CGC_MC_BC_VAL_R(lut[18])
	);

	/* Dummy */
	ctx[6] = 0;

	/* DQE0_CGC_MC3_R1*/
	ctx[7] = (CGC_MC_S1_R(lut[19]) | CGC_MC_S2_R(lut[20]));

	/* DQE0_CGC_MC4_R1*/
	ctx[8] = (CGC_MC_H1_R(lut[21]) | CGC_MC_H2_R(lut[22]));

	/* DQE0_CGC_MC5_R1*/
	ctx[9] = (CGC_MC_V1_R(lut[23]) | CGC_MC_V2_R(lut[24]));


	/* DQE0_CGC_MC1_R2*/
	ctx[10] = (
		CGC_MC_ON_R(lut[25]) | CGC_MC_INVERSE_R(lut[26]) |
		CGC_MC_GAIN_R(lut[27]) | CGC_MC_BC_HUE_R(lut[28]) |
		CGC_MC_BC_SAT_R(lut[29]) | CGC_MC_BC_VAL_R(lut[30])
	);

	/* retain DQE0_CGC_MC2_R2*/

	/* DQE0_CGC_MC3_R2*/
	ctx[12] = (CGC_MC_S1_R(lut[31]) | CGC_MC_S2_R(lut[32]));

	/* DQE0_CGC_MC4_R2*/
	ctx[13] = (CGC_MC_H1_R(lut[33]) | CGC_MC_H2_R(lut[34]));

	/* DQE0_CGC_MC5_R2*/
	ctx[14] = (CGC_MC_V1_R(lut[35]) | CGC_MC_V2_R(lut[36]));
}

void dqe_reg_set_cgc_dither(u32 *ctx, u32 *lut, int bit_ext)
{
	/* DQE0_CGC_DITHER*/
	*ctx = (
		CGC_DITHER_EN(lut[0]) | CGC_DITHER_MODE(lut[1]) |
		CGC_DITHER_MASK_SPIN(lut[2]) | CGC_DITHER_MASK_SEL(lut[3], 0) |
		CGC_DITHER_MASK_SEL(lut[4], 1) | CGC_DITHER_MASK_SEL(lut[5], 2) |
		CGC_DITHER_BIT((bit_ext < 0) ? lut[6] : bit_ext) |
		CGC_DITHER_FRAME_OFFSET(lut[7])
	);
}

/*
 * If Gamma Matrix is transferred with this pattern,
 *
 * |A B C D|
 * |E F G H|
 * |I J K L|
 * |M N O P|
 *
 * Coeff and Offset will be applied as follows.
 *
 * |Rout|   |A E I||Rin|   |M|
 * |Gout| = |B F J||Gin| + |N|
 * |Bout|   |C G K||Bin|   |O|
 *
 */
void dqe_reg_set_gamma_matrix(u32 *ctx, int *lut, u32 shift)
{
	int i;
	u32 gamma_mat_lut[17] = {0,};
	int bound, tmp;

	gamma_mat_lut[0] = lut[0];

	/* Coefficient */
	for (i = 0; i < 12; i++)
		gamma_mat_lut[i + 1] = (lut[i + 1] >> 5);
	/* Offset */
	bound = 1023 >> shift; // 10bit: -1023~1023, 8bit: -255~255
	for (i = 12; i < 16; i++) {
		tmp = (lut[i + 1] >> (6+shift));
		if (tmp > bound)
			tmp = bound;
		if (tmp < -bound)
			tmp = -bound;
		gamma_mat_lut[i + 1] = tmp;
	}
	ctx[0] = GAMMA_MATRIX_EN(gamma_mat_lut[0]);

	/* GAMMA_MATRIX_COEFF */
	ctx[1] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[5]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[1])
	);

	ctx[2] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[2]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[9])
	);

	ctx[3] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[10]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[6])
	);

	ctx[4] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[7]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[3])
	);

	ctx[5] = GAMMA_MATRIX_COEFF_L(gamma_mat_lut[11]);

	/* GAMMA_MATRIX_OFFSET */
	ctx[6] = (
		GAMMA_MATRIX_OFFSET_1(gamma_mat_lut[14]) | GAMMA_MATRIX_OFFSET_0(gamma_mat_lut[13])
	);

	ctx[7] = GAMMA_MATRIX_OFFSET_2(gamma_mat_lut[15]);
}

void dqe_reg_set_gamma_lut(u32 *ctx, u32 *lut, u32 shift)
{
	int i, idx;
	u32 lut_l, lut_h;
	u32 *luts = &lut[1];

	ctx[0] = REGAMMA_EN(lut[0]);
	for (i = 1, idx = 0; i < DQE_REGAMMA_REG_MAX; i++) {
		lut_h = 0;
		lut_l = REGAMMA_LUT_L(luts[idx]>>shift);
		idx++;

		if (((DQE_REGAMMA_LUT_CNT%2) == 0) || ((idx % DQE_REGAMMA_LUT_CNT) != 0)) {
			lut_h = REGAMMA_LUT_H(luts[idx]>>shift);
			idx++;
		}

		ctx[i] = (lut_h | lut_l);
	}
}

void dqe_reg_set_degamma_lut(u32 *ctx, u32 *lut, u32 shift)
{
	int i, idx;
	u32 lut_l, lut_h;
	u32 *luts = &lut[1];

	ctx[0] = DEGAMMA_EN(lut[0]);
	for (i = 1, idx = 0; i < DQE_DEGAMMA_REG_MAX; i++) {
		lut_h = 0;
		lut_l = DEGAMMA_LUT_L(luts[idx]>>shift);
		idx++;

		if (((DQE_DEGAMMA_LUT_CNT%2) == 0) || ((idx % DQE_DEGAMMA_LUT_CNT) != 0)) {
			lut_h = DEGAMMA_LUT_H(luts[idx]>>shift);
			idx++;
		}

		ctx[i] = (lut_h | lut_l);
	}
}

void dqe_reg_set_hsc_lut(u32 *con, u32 *poly, u32 *gain, u32 *lut,
				u32 (*lcg)[DQE_HSC_LUT_LSC_GAIN_MAX], u32 shift)
{
	int i, j, k;

	/* DQE0_HSC_CONTROL */
	con[0] = (
		 HSC_EN(lut[0])
	);

	/* DQE0_HSC_LOCAL_CONTROL */
	con[1] = (
		HSC_LSC_ON(lut[1]) | HSC_LHC_ON(lut[2]) | HSC_LVC_ON(lut[3])
	);

	/* DQE0_HSC_GLOBAL_CONTROL_1 */
	con[2] = (
		HSC_GHC_ON(lut[4]) | HSC_GHC_GAIN(lut[5]>>shift)
	);

	/* DQE0_HSC_GLOBAL_CONTROL_0 */
	con[3] = (
		HSC_GSC_ON(lut[6]) | HSC_GSC_GAIN(lut[7]>>shift)
	);

	/* DQE0_HSC_GLOBAL_CONTROL_2 */
	con[4] = (
		HSC_GVC_ON(lut[8]) | HSC_GVC_GAIN(lut[9]>>shift)
	);

	/* Dummy */
	con[5] = con[6] = 0;

	/* DQE0_HSC_CONTROL_MC1_R0 */
	con[7] = (
		HSC_MC_ON_R0(lut[10]) | HSC_MC_BC_HUE_R0(lut[11]) |
		HSC_MC_BC_SAT_R0(lut[12]) | HSC_MC_BC_VAL_R0(lut[13]) |
		HSC_MC_SAT_GAIN_R0(lut[14]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC2_R0 */
	con[8] = (
		HSC_MC_HUE_GAIN_R0(lut[15]>>shift) | HSC_MC_VAL_GAIN_R0(lut[16]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC3_R0 */
	con[9] = (
		HSC_MC_S1_R0(lut[17]>>shift) | HSC_MC_S2_R0(lut[18]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC4_R0 */
	con[10] = (
		HSC_MC_H1_R0(lut[19]>>shift) | HSC_MC_H2_R0(lut[20]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC5_R0 */
	con[11] = (
		HSC_MC_V1_R0(lut[21]>>shift) | HSC_MC_V2_R0(lut[22]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC1_R1 */
	con[12] = (
		HSC_MC_ON_R1(lut[23]) | HSC_MC_BC_HUE_R1(lut[24]) |
		HSC_MC_BC_SAT_R1(lut[25]) | HSC_MC_BC_VAL_R1(lut[26]) |
		HSC_MC_SAT_GAIN_R1(lut[27]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC2_R1 */
	con[13] = (
		HSC_MC_HUE_GAIN_R1(lut[28]>>shift) | HSC_MC_VAL_GAIN_R1(lut[29]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC3_R1 */
	con[14] = (
		HSC_MC_S1_R1(lut[30]>>shift) | HSC_MC_S2_R1(lut[31]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC4_R1 */
	con[15] = (
		HSC_MC_H1_R1(lut[32]>>shift) | HSC_MC_H2_R1(lut[33]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC5_R1 */
	con[16] = (
		HSC_MC_V1_R1(lut[34]>>shift) | HSC_MC_V2_R1(lut[35]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC1_R2 */
	con[17] = (
		HSC_MC_ON_R2(lut[36]) | HSC_MC_BC_HUE_R2(lut[37]) |
		HSC_MC_BC_SAT_R2(lut[38]) | HSC_MC_BC_VAL_R2(lut[39]) |
		HSC_MC_SAT_GAIN_R2(lut[40]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC2_R2 */
	con[18] = (
		HSC_MC_HUE_GAIN_R2(lut[41]>>shift) | HSC_MC_VAL_GAIN_R2(lut[42]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC3_R2 */
	con[19] = (
		HSC_MC_S1_R2(lut[43]>>shift) | HSC_MC_S2_R2(lut[44]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC4_R2 */
	con[20] = (
		HSC_MC_H1_R2(lut[45]>>shift) | HSC_MC_H2_R2(lut[46]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC5_R2 */
	con[21] = (
		HSC_MC_V1_R2(lut[47]>>shift) | HSC_MC_V2_R2(lut[48]>>shift)
	);

	/* DQE0_HSC_POLY_S0 -- DQE0_HSC_POLY_S4 */
	poly[0] = (HSC_POLY_CURVE_S_L(lut[49]>>shift) | HSC_POLY_CURVE_S_H(lut[50]>>shift));
	poly[1] = (HSC_POLY_CURVE_S_L(lut[51]>>shift) | HSC_POLY_CURVE_S_H(lut[52]>>shift));
	poly[2] = (HSC_POLY_CURVE_S_L(lut[53]>>shift) | HSC_POLY_CURVE_S_H(lut[54]>>shift));
	poly[3] = (HSC_POLY_CURVE_S_L(lut[55]>>shift) | HSC_POLY_CURVE_S_H(lut[56]>>shift));
	poly[4] = (HSC_POLY_CURVE_S_L(lut[57]>>shift));

	/* DQE0_HSC_POLY_V0 -- DQE0_HSC_POLY_V4 */
	poly[5] = (HSC_POLY_CURVE_V_L(lut[58]>>shift) | HSC_POLY_CURVE_V_H(lut[59]>>shift));
	poly[6] = (HSC_POLY_CURVE_V_L(lut[60]>>shift) | HSC_POLY_CURVE_V_H(lut[61]>>shift));
	poly[7] = (HSC_POLY_CURVE_V_L(lut[62]>>shift) | HSC_POLY_CURVE_V_H(lut[63]>>shift));
	poly[8] = (HSC_POLY_CURVE_V_L(lut[64]>>shift) | HSC_POLY_CURVE_V_H(lut[65]>>shift));
	poly[9] = (HSC_POLY_CURVE_V_L(lut[66]>>shift));

	k = (DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX);
	for (i = 0; i < 3; i++)
		for (j = 0; j < DQE_HSC_LUT_LSC_GAIN_MAX; j++)
			lut[k++] = lcg[i][j];

	/* DQE0_HSC_LSC_GAIN_P1_0 -- DQE1_HSC_LBC_GAIN_P3_23 */
	for (i = 0, j = (DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX); i < DQE_HSC_REG_GAIN_MAX; i++) {
		gain[i] = (
			HSC_LSC_GAIN_P1_L(lut[j]>>shift) | HSC_LSC_GAIN_P1_H(lut[j + 1]>>shift)
		);
		j += 2;
	}
}

void dqe_reg_set_aps_lut(u32 *ctx, u32 *lut, u32 bpc, u32 shift)
{
	/* DQE0_ATC_CONTROL */
	ctx[0] = (
		 ATC_EN(lut[0]) |
		 ATC_AVG_SELECT_METHOD(lut[1]) |
		 ATC_PIXMAP_EN(lut[2])
	);

	/* DQE0_ATC_GAIN */
	ctx[1] = (
		ATC_LT(lut[3]) | ATC_NS(lut[4]) |
		ATC_ST(lut[5])
	);

	/* DQE0_ATC_WEIGHT */
	ctx[2] = (
		ATC_PL_W1(lut[6]) | ATC_PL_W2(lut[7]) |
		ATC_LA_W_ON(lut[8]) | ATC_LA_W(lut[9]) |
		ATC_LT_CALC_MODE(lut[10])
	);

	/* DQE0_ATC_CTMODE */
	ctx[3] = (
		ATC_CTMODE(lut[11])
	);

	/* DQE0_ATC_PPEN */
	ctx[4] = (
		ATC_PP_EN(lut[12])
	);

	/* DQE0_ATC_TDRMINMAX */
	ctx[5] = (
		ATC_TDR_MIN(lut[13]) | ATC_TDR_MAX(lut[14])
	);

	/* DQE0_ATC_AMBIENT_LIGHT */
	ctx[6] = (
		ATC_AMBIENT_LIGHT(lut[15])
	);

	/* DQE0_ATC_BACK_LIGHT */
	ctx[7] = (
		ATC_BACK_LIGHT(lut[16])
	);

	/* DQE0_ATC_DSTEP */
	ctx[8] = (
		ATC_TDR_DSTEP(lut[17]) | ATC_AVG_DSTEP(lut[18]) |
		ATC_HE_DSTEP(lut[19]) | ATC_LT_DSTEP(lut[20])
	);

	/* DQE0_ATC_SCALE_MODE */
	ctx[9] = (
		ATC_SCALE_MODE(lut[21])
	);

	/* DQE0_ATC_THRESHOLD */
	ctx[10] = (
		ATC_THRESHOLD_1(lut[22]) |
		ATC_THRESHOLD_2(lut[23]) |
		ATC_THRESHOLD_3(lut[24])
	);

	/* retain GAIN_LIMIT (deprecated) */
	/* retain DIMMING_DONE_INTR,PARTIAL_IBSI_P1,PARTIAL_IBSI_P2 */

	/* DQE0_ATC_DIMMING_CONTROL */
	ctx[16] = (
		ATC_DIM_PERIOD_SHIFT(lut[25]) |
		ATC_TARGET_DIM_RATIO(lut[26])
	);

	/* DQE0_ATC_GT_CONTROL */
	ctx[17] = (
		ATC_GT_HE_ENABLE(lut[27]) |
		ATC_GT_LAMDA(lut[28]) |
		ATC_GT_LAMDA_DSTEP(lut[29])
	);

	/* retain CDF_DIV */
	/* retain HE_CLIP_MIN_0/1/2/3 HE_CLIP_MAX_0/1/2/3 (deprecated) */

	/* DQE0_ATC_HE_CONTROL */
	ctx[27] = (
		ATC_HE_BLEND_WEIGHT(lut[30])
	);

	/* DQE0_ATC_HE_CLIP_CONTROL */
	ctx[28] = (
		ATC_HE_CLIP_THRESHOLD(lut[31]) |
		ATC_HE_CLIP_WEIGHT(lut[32]) |
		ATC_HE_CLIP_TRANSITION(lut[33])
	);

	/* DQE0_ATC_LT_WEIGHT1 */
	ctx[29] = (
		ATC_LA_WEIGHT0(lut[34]) |
		ATC_LA_WEIGHT1(lut[35])
	);

	/* Dummy */
	ctx[30] = ctx[31] = 0;

	/* DQE0_ATC_DITHER_CON */
	ctx[32] = (
		ATC_DITHER_EN(lut[36]) |
		ATC_DITHER_MODE(lut[37]) |
		ATC_DITHER_MASK_SPIN(lut[38]) |
		ATC_DITHER_MASK_SEL_R(lut[39]) |
		ATC_DITHER_MASK_SEL_G(lut[40]) |
		ATC_DITHER_MASK_SEL_B(lut[41]) |
		ATC_DITHER_FRAME_OFFSET(lut[42])
	);

}

void dqe_reg_set_scl_lut(u32 *ctx, u32 *lut)
{
	/* 8-tap Filter Coefficient */
	const s16 ps_h_c_8t[1][16][8] = {
		{	/* Ratio <= 65536 (~8:8) Selection = 0 */
			{   0,	 0,   0, 512,	0,   0,   0,   0 },
			{  -2,	 8, -25, 509,  30,  -9,   2,  -1 },
			{  -4,	14, -46, 499,  64, -19,   5,  -1 },
			{  -5,	20, -62, 482, 101, -30,   8,  -2 },
			{  -6,	23, -73, 458, 142, -41,  12,  -3 },
			{  -6,	25, -80, 429, 185, -53,  15,  -3 },
			{  -6,	26, -83, 395, 228, -63,  19,  -4 },
			{  -6,	25, -82, 357, 273, -71,  21,  -5 },
			{  -5,	23, -78, 316, 316, -78,  23,  -5 },
			{  -5,	21, -71, 273, 357, -82,  25,  -6 },
			{  -4,	19, -63, 228, 395, -83,  26,  -6 },
			{  -3,	15, -53, 185, 429, -80,  25,  -6 },
			{  -3,	12, -41, 142, 458, -73,  23,  -6 },
			{  -2,	 8, -30, 101, 482, -62,  20,  -5 },
			{  -1,	 5, -19,  64, 499, -46,  14,  -4 },
			{  -1,	 2,  -9,  30, 509, -25,   8,  -2 }
		},
	};

	/* 4-tap Filter Coefficient */
	const s16 ps_v_c_4t[1][16][4] = {
		{	/* Ratio <= 65536 (~8:8) Selection = 0 */
			{   0, 512,   0,   0 },
			{ -15, 508,  20,  -1 },
			{ -25, 495,  45,  -3 },
			{ -31, 473,  75,  -5 },
			{ -33, 443, 110,  -8 },
			{ -33, 408, 148, -11 },
			{ -31, 367, 190, -14 },
			{ -27, 324, 234, -19 },
			{ -23, 279, 279, -23 },
			{ -19, 234, 324, -27 },
			{ -14, 190, 367, -31 },
			{ -11, 148, 408, -33 },
			{  -8, 110, 443, -33 },
			{  -5,	75, 473, -31 },
			{  -3,	45, 495, -25 },
			{  -1,	20, 508, -15 }
		},
	};
	int i, j, cnt, idx = 0;
	u32 luts[DQE_SCL_LUT_MAX];

	for (cnt = 0; cnt < DQE_SCL_COEF_CNT; cnt++) {
		for (j = 0; j < DQE_SCL_VCOEF_MAX; j++)
			for (i = 0; i < DQE_SCL_COEF_SET; i++)
				luts[idx++] = ps_v_c_4t[0][i][j];

		for (j = 0; j < DQE_SCL_HCOEF_MAX; j++)
			for (i = 0; i < DQE_SCL_COEF_SET; i++)
				luts[idx++] = ps_h_c_8t[0][i][j];
	}

	if (lut[0]) {
		for (cnt = 0, idx = 0; cnt < DQE_SCL_COEF_CNT; cnt++)
			for (j = 1; j <= DQE_SCL_COEF_MAX; j++, idx += DQE_SCL_COEF_SET)
				luts[idx] = lut[j];
	}

	for (i = 0; i < DQE_SCL_REG_MAX; i++)
		ctx[i] = SCL_COEF(luts[i]) & SCL_COEF_MASK;
}

void dqe_reg_set_de_lut(u32 *ctx, u32 *lut, u32 shift)
{
	/* DQE0_DE_CONTROL */
	ctx[0] = (
		 DE_EN(lut[0]) | DE_ROI_EN(lut[1]) | DE_ROI_IN(lut[2]) |
		 DE_SMOOTH_EN(lut[3]) | DE_FILTER_TYPE(lut[4]) |
		 DE_BRATIO_SMOOTH(lut[5])
	);

	/* DQE0_DE_ROI_START */
	ctx[1] = (
		DE_ROI_START_X(lut[6]) | DE_ROI_START_Y(lut[7])
	);

	/* DQE0_DE_ROI_END */
	ctx[2] = (
		DE_ROI_END_X(lut[8]) | DE_ROI_END_Y(lut[9])
	);

	/* DQE0_DE_FLATNESS */
	ctx[3] = (
		DE_FLAT_EDGE_TH(lut[10]>>shift) | DE_EDGE_BALANCE_TH(lut[11]>>shift)
	);

	/* DQE0_DE_CLIPPING */
	ctx[4] = (
		DE_PLUS_CLIP(lut[12]>>shift) | DE_MINUS_CLIP(lut[13]>>shift)
	);

	/* DQE0_DE_GAIN */
	ctx[5] = (
		DE_GAIN(lut[14]) |
		DE_LUMA_DEGAIN_TH(lut[15]>>shift) | DE_FLAT_DEGAIN_FLAG(lut[16]) |
		DE_FLAT_DEGAIN_SHIFT(lut[17]) | DF_MAX_LUMINANCE(lut[18]>>shift)
	);
}

void dqe_reg_set_rcd_lut(u32 *ctx, u32 *lut)
{
	/* DQE_RCD */
	ctx[0] = RCD_EN(lut[0]);

	/* DQE_RCD_BG_ALPHA_OUTTER */
	ctx[1] = (
		BG_ALPHA_OUTTER_R(lut[1]) |
		BG_ALPHA_OUTTER_G(lut[2]) |
		BG_ALPHA_OUTTER_B(lut[3])
	);

	/* DQE_RCD_BG_ALPHA_ALIASING */
	ctx[2] = (
		BG_ALPHA_ALIASING_R(lut[1]) |
		BG_ALPHA_ALIASING_G(lut[2]) |
		BG_ALPHA_ALIASING_B(lut[3])
	);
}

u32 dqe_reg_get_version(u32 id)
{
	return dqe_read(id, DQE_TOP_VER);
}

static inline u32 dqe_reg_get_lut_addr(u32 id, enum dqe_reg_type type,
					u32 index, u32 opt)
{
	u32 addr = 0;

	switch (type) {
	case DQE_REG_GAMMA_MATRIX:
		if (index >= DQE_GAMMA_MATRIX_REG_MAX)
			return 0;
		addr = DQE_GAMMA_MATRIX_LUT(index);
		break;
	case DQE_REG_CGC_DITHER:
		addr = DQE_CGC_DITHER_CON;
		break;
	case DQE_REG_CGC_CON:
		if (index >= DQE_CGC_CON_REG_MAX)
			return 0;
		addr = DQE_CGC_MC_R0(index);
		break;
	case DQE_REG_CGC:
		if (index >= DQE_CGC_REG_MAX)
			return 0;
		switch (opt) {
		case DQE_REG_CGC_R:
			addr = DQE_CGC_LUT_R(index);
			break;
		case DQE_REG_CGC_G:
			addr = DQE_CGC_LUT_G(index);
			break;
		case DQE_REG_CGC_B:
			addr = DQE_CGC_LUT_B(index);
			break;
		}
		break;
	case DQE_REG_DEGAMMA:
		if (index >= DQE_DEGAMMA_REG_MAX)
			return 0;
		addr = DQE_DEGAMMA_LUT(index);
		break;
	case DQE_REG_REGAMMA:
		if (index >= DQE_REGAMMA_REG_MAX)
			return 0;
		addr = DQE_REGAMMA_LUT(index);
		break;
	case DQE_REG_HSC:
		if (index >= DQE_HSC_REG_MAX)
			return 0;
		switch (opt) {
		case DQE_REG_HSC_CON:
			if (index >= DQE_HSC_REG_CTRL_MAX)
				return 0;
			addr = DQE_HSC_CONTROL_LUT(index);
			break;
		case DQE_REG_HSC_POLY:
			if (index >= DQE_HSC_REG_POLY_MAX)
				return 0;
			addr = DQE_HSC_POLY_LUT(index);
			break;
		case DQE_REG_HSC_GAIN:
			if (index >= DQE_HSC_REG_GAIN_MAX)
				return 0;
			addr = DQE_HSC_LSC_GAIN_LUT(index);
			break;
		}
		break;
	case DQE_REG_ATC:
		if (index >= DQE_ATC_REG_MAX)
			return 0;
		addr = DQE_ATC_CONTROL_LUT(index);
		break;
	case DQE_REG_SCL:
		if (index >= DQE_SCL_REG_MAX)
			return 0;
		addr = DQE_SCL_Y_VCOEF(index);
		break;
	case DQE_REG_DE:
		if (index >= DQE_DE_REG_MAX)
			return 0;
		addr = DQE_DE_LUT(index);
		break;
	case DQE_REG_LPD:
		if (index >= DQE_LPD_REG_MAX)
			return 0;
		addr = DQE_TOP_LPD(index);
		break;
	case DQE_REG_RCD:
		if (index >= DQE_RCD_REG_MAX)
			return 0;
		addr = DQE_RCD_LUT(index);
		break;
	case DQE_REG_DISP_DITHER:
		return 0;
	default:
		cal_log_debug(id, "invalid reg type %d\n", type);
		return 0;
	}

	return addr;
}

u32 dqe_reg_get_lut(u32 id, enum dqe_reg_type type, u32 index, u32 opt)
{
	u32 addr = dqe_reg_get_lut_addr(id, type, index, opt);

	if (addr == 0)
		return 0;
	return dqe_read(id, addr);
}

void dqe_reg_set_lut(u32 id, enum dqe_reg_type type, u32 index, u32 value, u32 opt)
{
	u32 addr = dqe_reg_get_lut_addr(id, type, index, opt);

	if (addr == 0)
		return;
	dqe_write_relaxed(id, addr, value);
}

void dqe_reg_set_lut_on(u32 id, enum dqe_reg_type type, u32 on)
{
	u32 addr;
	u32 value;
	u32 mask;

	switch (type) {
	case DQE_REG_GAMMA_MATRIX:
		addr = DQE_GAMMA_MATRIX_CON;
		value = GAMMA_MATRIX_EN(on);
		mask = GAMMA_MATRIX_EN_MASK;
		break;
	case DQE_REG_CGC_DITHER:
		addr = DQE_CGC_DITHER_CON;
		value = CGC_DITHER_EN(on);
		mask = CGC_DITHER_EN_MASK;
		break;
	case DQE_REG_CGC_CON:
		addr = DQE_CGC_CON;
		value = CGC_EN(on);
		mask = CGC_EN_MASK;
		break;
	case DQE_REG_CGC_DMA:
		addr = DQE_CGC_CON;
		value = CGC_COEF_DMA_REQ(on);
		mask = CGC_COEF_DMA_REQ_MASK;
		break;
	case DQE_REG_DEGAMMA:
		addr = DQE_DEGAMMA_CON;
		value = DEGAMMA_EN(on);
		mask = DEGAMMA_EN_MASK;
		break;
	case DQE_REG_REGAMMA:
		addr = DQE_REGAMMA_CON;
		value = REGAMMA_EN(on);
		mask = REGAMMA_EN_MASK;
		break;
	case DQE_REG_HSC:
		addr = DQE_HSC_CONTROL;
		value = HSC_EN(on);
		mask = HSC_EN_MASK;
		break;
	case DQE_REG_ATC:
		addr = DQE_ATC_CONTROL;
		value = ATC_EN(on);
		mask = ATC_EN_MASK;
		break;
	case DQE_REG_DE:
		addr = DQE_DE_CONTROL;
		value = DE_EN(on);
		mask = DE_EN_MASK;
		break;
	case DQE_REG_LPD:
		addr = DQE_TOP_LPD_MODE_CONTROL;
		value = DQE_LPD_MODE_EXIT(on);
		mask = DQE_LPD_MODE_EXIT_MASK;
		break;
	case DQE_REG_RCD:
		addr = DQE_RCD;
		value = RCD_EN(on);
		mask = RCD_EN_MASK;
		break;
	case DQE_REG_DISP_DITHER:
		return;
	default:
		cal_log_debug(id, "invalid reg type %d\n", type);
		return;
	}

	dqe_write_mask(id, addr, value, mask);
}

int dqe_reg_wait_cgc_dmareq(u32 id)
{
#if defined(CGC_COEF_DMA_REQ_MASK)
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dqe_regs_desc(id)->regs + DQE_CGC_CON,
			val, !(val & CGC_COEF_DMA_REQ_MASK), 10, 10000); /* timeout 10ms */
	if (ret) {
		cal_log_err(id, "[edma] timeout dma reg\n");
		return ret;
	}
#endif
	return 0;
}

static void dqe_reg_set_atc_partial_ibsi(u32 id, u32 width, u32 height)
{
	u32 val, val1, val2;
	u32 small_n = width / 8;
	u32 large_N = small_n + 1;
	u32 grid = height / 16;

	val1 = (width % 8 == 0) ? ((1 << 16) / (small_n * 4)) : ((1 << 16) / (large_N * 4));
	val2 = (height % 16 == 0) ? ((1 << 16) / (grid * 4)) : ((1 << 16) / ((grid * 4) + 4));
	val = ATC_IBSI_X_P1(val1) | ATC_IBSI_Y_P1(val2);
	dqe_write_relaxed(id, DQE_ATC_PARTIAL_IBSI_P1, val);

	val1 = (width % 8 == 0) ? ((1 << 16) / (small_n * 2)) : ((1 << 16) / (large_N * 2));
	val2 = (height % 16 == 0) ? ((1 << 16) / (grid * 2)) : ((1 << 16) / ((grid * 2) + 2));
	val = ATC_IBSI_X_P2(val1) | ATC_IBSI_Y_P2(val2);
	dqe_write_relaxed(id, DQE_ATC_PARTIAL_IBSI_P2, val);
}

/*
 * ATC_CDF_SHIFT = ceil(log_2((481 x Width x Height) / 255 x 1 / 2^14))
 * ATC_CDF_DIV = (2^14 / (((Width x Height) >> ATC_CDF_SHIFT) ) x 255)
 */
static inline u32 ceil_log2(u32 n)
{
	u32 val;

	for (val = 0; BIT(val) < n; val++)
		continue;
	return val;
}

static void dqe_reg_set_atc_cdf_div(u32 id, u32 width, u32 height)
{
	u32 shift, div, val, size = width * height;

	shift = ceil_log2(DIV_ROUND_UP(481 * size, BIT(14) * 255));
	div = (BIT(14) * 255) / max(size >> shift, 1u);
	val = ATC_CDF_SHIFT(shift) | ATC_CDF_DIV(div);
	dqe_write_relaxed(id, DQE_ATC_CDF_DIV, val);
}

void dqe_reg_set_atc_img_size(u32 id, u32 width, u32 height)
{
	dqe_reg_set_atc_partial_ibsi(id, width, height);
	dqe_reg_set_atc_cdf_div(id, width, height);
}

void dqe_regs_desc_init(u32 id, void __iomem *regs, const char *name,
		enum dqe_regs_type type)
{
	regs_dqe[type][id].regs = regs;
	regs_dqe[type][id].name = name;
}

static void dqe_reg_set_img_size(u32 id, u32 width, u32 height)
{
	u32 val;

	val = DQE_FULL_IMG_VSIZE(height) | DQE_FULL_IMG_HSIZE(width);
	dqe_write_relaxed(id, DQE_TOP_FRM_SIZE, val);

	val = DQE_FULL_PXL_NUM(width * height);
	dqe_write_relaxed(id, DQE_TOP_FRM_PXL_NUM, val);

	val = DQE_PARTIAL_START_Y(0) | DQE_PARTIAL_START_X(0);
	dqe_write_relaxed(id, DQE_TOP_PARTIAL_START, val);

	val = DQE_IMG_VSIZE(height) | DQE_IMG_HSIZE(width);
	dqe_write_relaxed(id, DQE_TOP_IMG_SIZE, val);

	val = DQE_PARTIAL_UPDATE_EN(0);
	dqe_write_relaxed(id, DQE_TOP_PARTIAL_CON, val);
}

static void dqe_reg_set_scaled_img_size(u32 id, u32 xres, u32 yres)
{
#if !IS_ENABLED(CONFIG_SOC_S5E9925_EVT0) // EVT0 support DECON SCL but not implemented
	u32 addr;
	int h_ratio = 1 << 20; /* DECON post scaler is deprecated */
	int v_ratio = 1 << 20;

	addr = DQE_SCL_SCALED_IMG_SIZE;
	dqe_write_relaxed(id, addr, SCALED_IMG_HEIGHT(yres) | SCALED_IMG_WIDTH(xres));

	addr = DQE_SCL_MAIN_H_RATIO;
	dqe_write_mask(id, addr, H_RATIO(h_ratio), H_RATIO_MASK);

	addr = DQE_SCL_MAIN_V_RATIO;
	dqe_write_mask(id, addr, V_RATIO(v_ratio), V_RATIO_MASK);
#endif
}

/* exposed to driver layer for DQE CAL APIs */
void dqe_reg_init(u32 id, u32 width, u32 height)
{
	cal_log_debug(id, "%s +\n", __func__);
	dqe_reg_set_img_size(id, width, height);
	dqe_reg_set_scaled_img_size(id, width, height);
	decon_reg_set_dqe_enable(id, true);
	cal_log_debug(id, "%s -\n", __func__);
}
