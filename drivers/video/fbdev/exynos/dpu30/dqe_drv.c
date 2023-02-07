/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung EXYNOS SoC series DQE driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/console.h>

#include "dqe.h"
#include "decon.h"
#if defined(CONFIG_SOC_EXYNOS9630)
#include "./cal_9630/regs-dqe.h"
#endif

struct dqe_device *dqe_drvdata;
struct class *dqe_class;

u32 gamma_lut[3][65];
u32 cgc_lut[143];
u32 hsc_lut[154];
u32 aps_lut[26];

#define TUNE_MODE_SIZE 4
u32 tune_mode[TUNE_MODE_SIZE][492];
char dqe_str_result[1024];

int dqe_log_level = 6;
module_param(dqe_log_level, int, 0644);

const u32 bypass_cgc_tune[143] = {
	3,0,0,3,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,512,512,512,512,512,512,512,512,512,1023,1023,1023,1023,1023,1023,1023,1023,1023,0,0,0,512,512,512,1023,1023,1023,0,0,0,512,512,512,1023,1023,1023,0,0,0,512,512,512,1023,1023,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023
};

const u32 bypass_gamma_tune[3][65] = {
	{0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,228,232,236,240,244,248,252,256},
	{0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,228,232,236,240,244,248,252,256},
	{0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,228,232,236,240,244,248,252,256},
};

const s32 bypass_hsc_tune[154] = {
	0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,5,-10,0,0,5,-10,0,0,0,0,0,0,0,51,204,10,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

int aps_gmarr_LUT64[3][65] = {
	/*set slope = 1*/
	{
		0, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		40, 44, 48, 52, 56, 60, 64, 68, 72, 76,
		80, 84, 88, 92, 96, 100, 104, 108, 112, 116,
		120, 124, 128, 132, 136, 140, 144, 148, 152, 156,
		160, 164, 168, 172, 176, 180, 184, 188, 192, 196,
		200, 204, 208, 212, 216, 220, 224, 228, 232, 236,
		240, 244, 248, 252, 256,
	},
	/*set slope = 2.2*/
	{
		0, 0, 0, 0, 0, 0, 1, 1, 2, 3,
		4, 5, 6, 7, 9, 10, 12, 13, 15, 17,
		19, 22, 24, 26, 29, 32, 35, 38, 41, 44,
		48, 51, 55, 59, 63, 67, 72, 76, 81, 86,
		91, 96, 101, 106, 112, 117, 123, 129, 135, 142,
		148, 155, 162, 169, 176, 183, 190, 198, 206, 214,
		222, 230, 238, 247, 256,
	},
	/*set slope = 1/2.2*/
	{
		0, 38, 52, 63, 72, 80, 87, 93, 99, 104,
		110, 114, 119, 124, 128, 132, 136, 140, 143, 147,
		150, 154, 157, 160, 163, 166, 169, 172, 175, 178,
		181, 184, 186, 189, 192, 194, 197, 199, 201, 204,
		206, 209, 211, 213, 215, 218, 220, 222, 224, 226,
		228, 230, 232, 234, 236, 238, 240, 242, 244, 246,
		248, 250, 252, 254, 256,
	},
};

int aps_gm_LUT64[65] = {
	0, 4, 8, 12, 16, 20, 24, 28, 32, 36,
	40, 44, 48, 52, 56, 60, 64, 68, 72, 76,
	80, 84, 88, 92, 96, 100, 104, 108, 112, 116,
	120, 124, 128, 132, 136, 140, 144, 148, 152, 156,
	160, 164, 168, 172, 176, 180, 184, 188, 192, 196,
	200, 204, 208, 212, 216, 220, 224, 228, 232, 236,
	240, 244, 248, 252, 256,
};

static void dqe_dump(void)
{
	int acquired = console_trylock();
	struct decon_device *decon = get_decon_drvdata(0);

	if (IS_DQE_OFF_STATE(decon)) {
		decon_info("%s: DECON is disabled, state(%d)\n",
				__func__, (decon) ? (decon->state) : -1);
		return;
	}

	decon_info("\n=== DQE SFR DUMP ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + DQE_BASE , 0x600, false);

	decon_info("\n=== DQE SHADOW SFR DUMP ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + SHADOW_DQE_OFFSET, 0x600, false);

	if (acquired)
		console_unlock();
}

static void dqe_load_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe->ctx.cgc[i].addr = DQECGCLUT_BASE + (i * 4);
		dqe->ctx.cgc[i].val = dqe_read(dqe->ctx.cgc[i].addr);
	}

	for (i = 0; i < DQEGAMMALUT_MAX; i++) {
		dqe->ctx.gamma[i].addr = DQEGAMMALUT_X_Y_BASE + (i * 4);
		dqe->ctx.gamma[i].val = dqe_read(dqe->ctx.gamma[i].addr);
	}

	for (i = 0; i < DQEHSCLUT_MAX; i++) {
		dqe->ctx.hsc[i].addr = DQEHSCLUT_BASE + (i * 4);
		dqe->ctx.hsc[i].val = dqe_read(dqe->ctx.hsc[i].addr);
	}

	for (i = 0; i < DQEAPSLUT_MAX - 4; i++) {
		dqe->ctx.aps[i].addr = DQEAPSLUT_BASE + (i * 4);
		dqe->ctx.aps[i].val = dqe_read(dqe->ctx.aps[i].addr);
	}
	dqe->ctx.aps[DQEAPSLUT_MAX - 4].addr = DQEAPS_PARTIAL_CON;
	dqe->ctx.aps[DQEAPSLUT_MAX - 4].val = dqe_read(dqe->ctx.aps[DQEAPSLUT_MAX - 4].addr);
	dqe->ctx.aps[DQEAPSLUT_MAX - 3].addr = DQEAPS_PARTIAL_ROI_UP_LEFT_POS;
	dqe->ctx.aps[DQEAPSLUT_MAX - 3].val = dqe_read(dqe->ctx.aps[DQEAPSLUT_MAX - 3].addr);
	dqe->ctx.aps[DQEAPSLUT_MAX - 2].addr = DQEAPS_PARTIAL_IBSI_01_00;
	dqe->ctx.aps[DQEAPSLUT_MAX - 2].val = dqe_read(dqe->ctx.aps[DQEAPSLUT_MAX - 2].addr);
	dqe->ctx.aps[DQEAPSLUT_MAX - 1].addr = DQEAPS_PARTIAL_IBSI_11_10;
	dqe->ctx.aps[DQEAPSLUT_MAX - 1].val = dqe_read(dqe->ctx.aps[DQEAPSLUT_MAX - 1].addr);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe_dbg("0x%04x %08x",
			dqe->ctx.cgc[i].addr,dqe->ctx.cgc[i].val);
	}

	for (i = 0; i < DQEGAMMALUT_MAX; i++) {
		dqe_dbg("0x%04x %d %d ",
			dqe->ctx.gamma[i].addr,
			DQEGAMMALUT_X_GET(dqe->ctx.gamma[i].val),
			DQEGAMMALUT_Y_GET(dqe->ctx.gamma[i].val));
	}

	for (i = 0; i < DQEHSCLUT_MAX; i++) {
		dqe_dbg("0x%04x %08x ",
			dqe->ctx.hsc[i].addr, dqe->ctx.hsc[i].val);
	}

	for (i = 0; i < DQEAPSLUT_MAX; i++) {
		dqe_dbg("0x%04x %08x ",
			dqe->ctx.aps[i].addr, dqe->ctx.aps[i].val);
	}
}

static void dqe_init_context(void)
{
	int i, j, k, val;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	/* DQEGAMMALUT_R_01_00 -- DQEGAMMALUT_B_64 */
	for (j = 0, k = 0; j < 3; j++) {
		val = 0;
		for (i = 0; i < 64; i += 2) {
			dqe->ctx.gamma[k++].val = (DQEGAMMALUT_X(val) | DQEGAMMALUT_Y(val + 4));
			val += 8;
		}
		dqe->ctx.gamma[k++].val = DQEGAMMALUT_X(val);
	}

	dqe->ctx.aps[0].val = 0x00808080; /* DQEAPS_GAIN */
	dqe->ctx.aps[1].val = 0x000e000a; /* DQEAPS_WEIGHT */
	dqe->ctx.aps[2].val = 0x00000001; /* DQEAPS_CTMODE */
	dqe->ctx.aps[3].val = 0x00000001; /* DQEAPS_PPEN */
	dqe->ctx.aps[4].val = 0x00ff0040; /* DQEAPS_TDRMINMAX */
	dqe->ctx.aps[5].val = 0x0000008c; /* DQEAPS_AMBIENT_LIGHT */
	dqe->ctx.aps[6].val = 0x000000ff; /* DQEAPS_BACK_LIGHT */
	dqe->ctx.aps[7].val = 0x00000001; /* DQEAPS_DSTEP */
	dqe->ctx.aps[8].val = 0x00000001; /* DQEAPS_SCALE_MODE*/
	dqe->ctx.aps[9].val = 0x00000015; /* DQEAPS_THRESHOLD */
	dqe->ctx.aps[10].val = 0x0000007f; /* DQEAPS_GAIN_LIMIT */
	dqe->ctx.aps[11].val = 0x00000000; /* DQEAPS_PARTIAL_CON */
	dqe->ctx.aps[12].val = 0x00000000; /* DQEAPS_PARTIAL_ROI_UP_LEFT_POS */
	dqe->ctx.aps[13].val = 0x00000000; /* DQEAPS_PARTIAL_IBSI_01_00 */
	dqe->ctx.aps[14].val = 0x00000000; /* DQEAPS_PARTIAL_IBSI_11_10 */

	dqe->ctx.cgc_on = 0;
	dqe->ctx.gamma_on = 0;
	dqe->ctx.hsc_on = 0;
	dqe->ctx.aps_on = 0;

	dqe->ctx.need_udpate = true;

	k = 0;
	for (i = 0; i < 143; i++)
		tune_mode[0][k++] = bypass_cgc_tune[i];

	for (i = 0; i < 3; i++)
		for (j = 0; j < 65; j++)
			tune_mode[0][k++] = bypass_gamma_tune[i][j];

	for (i = 0; i < 154; i++)
		tune_mode[0][k++] = (u32)bypass_hsc_tune[i];

	for (i = 0; i < 492; i++)
		dqe_dbg("%04x, %d", tune_mode[0][i], tune_mode[0][i]);

	for (i = 1; i < TUNE_MODE_SIZE; i++)
		for (k = 0; k < 492; k++)
			tune_mode[i][k] = tune_mode[i-1][k];
}

static int dqe_save_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	if (dqe->ctx.need_udpate)
		return 0;

	dqe_dbg("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++)
		dqe->ctx.cgc[i].val =
			dqe_read(dqe->ctx.cgc[i].addr);

	dqe->ctx.cgc_on = dqe_reg_get_cgc_on();

	for (i = 0; i < DQEGAMMALUT_MAX; i++)
		dqe->ctx.gamma[i].val =
			dqe_read(dqe->ctx.gamma[i].addr);

	dqe->ctx.gamma_on = dqe_reg_get_gamma_on();

	for (i = 0; i < DQEHSCLUT_MAX; i++)
		dqe->ctx.hsc[i].val =
			dqe_read(dqe->ctx.hsc[i].addr);

	dqe->ctx.hsc_on = dqe_reg_get_hsc_on();

	dqe->ctx.aps_on = dqe_reg_get_aps_on();

	return 0;
}

static int dqe_restore_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;
	struct decon_device *decon = dqe->decon;

	dqe_dbg("%s\n", __func__);

	mutex_lock(&dqe->restore_lock);

	dqe_reg_set_dqecon_all_reset();

	for (i = 0; i < DQECGCLUT_MAX; i++)
		dqe_write(dqe->ctx.cgc[i].addr,
				dqe->ctx.cgc[i].val);

	if (dqe->ctx.cgc_on)
		dqe_reg_set_cgc_on(1);

	for (i = 0; i < DQEGAMMALUT_MAX; i++)
		dqe_write(dqe->ctx.gamma[i].addr,
				dqe->ctx.gamma[i].val);

	if (dqe->ctx.gamma_on)
		dqe_reg_set_gamma_on(1);

	for (i = 0; i < DQEHSCLUT_MAX; i++)
		dqe_write(dqe->ctx.hsc[i].addr,
				dqe->ctx.hsc[i].val);

	if (dqe->ctx.hsc_on) {
		if (decon) {
			dqe_reg_set_hsc_full_pxl_num(decon->lcd_info);
			dqe_dbg("dqe DQEHSC_FULL_PXL_NUM: %d\n",
				dqe_reg_get_hsc_full_pxl_num());
		}
		dqe_reg_set_hsc_on(1);
	}

	for (i = 0; i < DQEAPSLUT_MAX; i++)
		dqe_write(dqe->ctx.aps[i].addr,
				dqe->ctx.aps[i].val);

	if (dqe->ctx.aps_on) {
		if (decon) {
			dqe_reg_set_aps_full_pxl_num(decon->lcd_info);
			dqe_dbg("dqe DQEAPS_FULL_PXL_NUM: %d\n",
				dqe_reg_get_aps_full_pxl_num());

			dqe_dbg("dqe DQEAPS_FULL_IMG_SIZESET: %x\n",
				dqe_read(DQEAPS_FULL_IMG_SIZESET));
		}
		dqe_reg_set_aps_on(1);
	}

	dqe->ctx.need_udpate = false;

	mutex_unlock(&dqe->restore_lock);

	return 0;
}

static void decon_dqe_update_context(struct decon_device *decon)
{
	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		return;
	}

	decon_hiber_block_exit(decon);

	dqe_restore_context();

	if (decon->lcd_info->mode == DECON_VIDEO_MODE)
		decon_reg_update_req_dqe(decon->id);

	decon_hiber_unblock(decon);
}

static void dqe_gamma_lut_set(void)
{
	int i, j, k;
	struct dqe_device *dqe = dqe_drvdata;

	for (j = 0, k = 0; j < 3; j++) {
		for (i = 0; i < 64; i += 2) {
			dqe->ctx.gamma[k++].val = (
				DQEGAMMALUT_X(gamma_lut[j][i]) |
				DQEGAMMALUT_Y(gamma_lut[j][i+1]));
		}
		dqe->ctx.gamma[k++].val =
			DQEGAMMALUT_X(gamma_lut[j][64]);
	}
}

static void dqe_cgc_lut_set(void)
{
	int i, j, k;
	struct dqe_device *dqe = dqe_drvdata;

	/* DQECGC_CONTROL1*/
	dqe->ctx.cgc[0].val = (
		CGC_DITHER_CNT_DIFF(cgc_lut[1]) | CGC_DITHER_ON(cgc_lut[2]) |
		CGC_DITHER_CON(cgc_lut[3]) | CGC_GAMMA_DEC_EN(cgc_lut[4]) |
		CGC_GAMMA_ENC_EN(cgc_lut[5])
	);

	/* DQECGC_CONTROL2*/
	dqe->ctx.cgc[1].val = (
		CGC_GRAY_MODE(cgc_lut[6]) | CGC_GRAY_BYPASS(cgc_lut[7]) |
		CGC_GRAY_GAIN(cgc_lut[8])
	);

	/* DQECGC_CONTROL3*/
	dqe->ctx.cgc[2].val = (
		CGC_GRAY_WEIGHT_TH(cgc_lut[9]) | CGC_MC_GAIN(cgc_lut[10])
	);

	/* DQECGC1_RED1 --- DQECGC1_BLUE9*/
	for (i = 3, j = 11, k = 1; i < 30; i++, k++) {
		if (k % 9) {
			dqe->ctx.cgc[i].val = (
				DQECGC_GRAYLUT_R0(cgc_lut[j]) | DQECGC_GRAYLUT_R1(cgc_lut[j + 1])
			);
			j += 2;
		} else {
			dqe->ctx.cgc[i].val = (
				DQECGC_GRAYLUT_R0(cgc_lut[j])
			);
			j += 1;
		}
	}

	/* DQECGC1_LUT27_RED1 --- DQECGC1_LUT27_BLUE14*/
	for (i = 30, k = 1; i < DQECGCLUT_MAX; i++, k++) {
		if (k % 14) {
			dqe->ctx.cgc[i].val = (
				DQECGC_LUT27_000_R(cgc_lut[j]) | DQECGC_LUT27_001_R(cgc_lut[j + 1])
			);
			j += 2;
		} else {
			dqe->ctx.cgc[i].val = (
				DQECGC_LUT27_000_R(cgc_lut[j])
			);
			j += 1;
		}
	}
}

static void dqe_hsc_lut_set(void)
{
	int i, j;
	struct dqe_device *dqe = dqe_drvdata;

	/* DQEHSC_CONTROL0 */
	dqe->ctx.hsc[0].val = (
		HSC_LSC_ON(hsc_lut[0]) | HSC_LHC_ON(hsc_lut[1]) |
		HSC_LBC_ON(hsc_lut[2]) | HSC_LSC_GROUPMODE(hsc_lut[3]) |
		HSC_LHC_GROUPMODE(hsc_lut[4]) | HSC_LBC_GROUPMODE(hsc_lut[5]) |
		HSC_IMG_PARTIAL_FRAME(hsc_lut[6]) | HSC_ROI_SAME(hsc_lut[7]) |
		HSC_PARTIAL_UPDATE_METHOD(hsc_lut[8])
	);

	/* DQEHSC_CONTROL2 */
	dqe->ctx.hsc[2].val = (
		HSC_GSC_ON(hsc_lut[9]) | HSC_GHC_ON(hsc_lut[10]) |
		HSC_GBC_ON(hsc_lut[11]) | HSC_GSC_GAIN(hsc_lut[12])
	);

	/* DQEHSC_CONTROL3 */
	dqe->ctx.hsc[3].val = (
		HSC_GHC_GAIN(hsc_lut[13]) | HSC_GBC_GAIN(hsc_lut[14])
	);

	/* DQEHSC_CONTROL_ALPHA_SAT */
	dqe->ctx.hsc[4].val = (
		HSC_ALPHA_SAT_ON(hsc_lut[15]) | HSC_ALPHA_SAT_SCALE(hsc_lut[16]) |
		HSC_ALPHA_SAT_SHIFT1(hsc_lut[17]) | HSC_ALPHA_SAT_SHIFT2(hsc_lut[18])
	);

	/* DQEHSC_CONTROL_ALPHA_BRI */
	dqe->ctx.hsc[5].val = (
		HSC_ALPHA_BRI_ON(hsc_lut[19]) | HSC_ALPHA_BRI_SCALE(hsc_lut[20]) |
		HSC_ALPHA_BRI_SHIFT1(hsc_lut[21]) | HSC_ALPHA_BRI_SHIFT2(hsc_lut[22])
	);

	/* DQEHSC_CONTROL_MC1 */
	dqe->ctx.hsc[6].val = (
		HSC_MC_ON(hsc_lut[23]) | HSC_MC_BOUNDARY_CONTROL_HUE(hsc_lut[24]) |
		HSC_MC_BOUNDARY_CONTROL_SAT(hsc_lut[25]) | HSC_MC_SAT_GAIN(hsc_lut[26])
	);

	/* DQEHSC_CONTROL_MC_BRI */
	dqe->ctx.hsc[7].val = (
		HSC_MC_HUE_GAIN(hsc_lut[27]) | HSC_MC_BRI_GAIN(hsc_lut[28])
	);

	/* DQEHSC_CONTROL_MC2 */
	dqe->ctx.hsc[8].val = (
		HSC_MC_S1(hsc_lut[29]) | HSC_MC_S2(hsc_lut[30])
	);

	/* DQEHSC_CONTROL_MC3 */
	dqe->ctx.hsc[9].val = (
		HSC_MC_H1(hsc_lut[31]) | HSC_MC_H2(hsc_lut[32])
	);

	/* DQEHSC_CONTROL_YCOMP */
	dqe->ctx.hsc[10].val = (
		HSC_YCOMP_ON(hsc_lut[33]) | HSC_YCOMP_DITH_ON(hsc_lut[34]) |
		HSC_BLEND_ON(hsc_lut[35]) | HSC_YCOMP_GAIN(hsc_lut[36]) |
		HSC_BLEND_MANUAL_GAIN(hsc_lut[37])
	);

	/* DQEHSC_LSC_GAIN_P1_0 -- DQEHSC_LSC_GAIN_P3_5 */
	for (i = 11, j = 38; i < 29; i++) {
		dqe->ctx.hsc[i].val = (
			HSC_LSC_GAIN_P1_0(hsc_lut[j]) | HSC_LSC_GAIN_P1_1(hsc_lut[j + 1])
		);
		j += 2;
	}

	/* DQEHSC_LHC_GAIN_P1_0 -- DQEHSC_LHC_GAIN_P3_5 */
	for (i = 29; i < 47; i++) {
		dqe->ctx.hsc[i].val = (
			HSC_LHC_GAIN_P1_0(hsc_lut[j]) | HSC_LHC_GAIN_P1_1(hsc_lut[j + 1])
		);
		j += 2;
	}

	/* DQEHSC_LBC_GAIN_P1_0 -- DQEHSC_LBC_GAIN_P3_5 */
	for (i = 47; i < 65; i++) {
		dqe->ctx.hsc[i].val = (
			HSC_LBC_GAIN_P1_0(hsc_lut[j]) | HSC_LBC_GAIN_P1_1(hsc_lut[j + 1])
		);
		j += 2;
	}

	/* DQEHSC_POLY0 -- DQEHSC_POLY3 */
	for (i = 65; i < DQEHSCLUT_MAX; i++) {
		dqe->ctx.hsc[i].val = (
			HSC_POLY_CURVE_1(hsc_lut[j]) | HSC_POLY_CURVE_2(hsc_lut[j + 1])
		);
		j += 2;
	}
}

static void dqe_aps_lut_set(void)
{
	struct dqe_device *dqe = dqe_drvdata;

	/* DQEAPS_GAIN */
	dqe->ctx.aps[0].val = (
		DQEAPSLUT_ST(aps_lut[0]) | DQEAPSLUT_NS(aps_lut[1]) |
		DQEAPSLUT_LT(aps_lut[2]));
	/* DQEAPS_WEIGHT */
	dqe->ctx.aps[1].val = (
		DQEAPSLUT_PL_W2(aps_lut[3]) | DQEAPSLUT_PL_W1(aps_lut[4]));
	/* DQEAPS_CTMODE */
	dqe->ctx.aps[2].val = (
		DQEAPSLUT_CTMODE(aps_lut[5]));
	/* DQEAPS_PPEN */
	dqe->ctx.aps[3].val = (
		DQEAPSLUT_PP_EN(aps_lut[6]));
	/* DQEAPS_TDRMINMAX */
	dqe->ctx.aps[4].val = (
		DQEAPSLUT_TDR_MAX(aps_lut[7]) | DQEAPSLUT_TDR_MIN(aps_lut[8]));
	/* DQEAPS_AMBIENT_LIGHT */
	dqe->ctx.aps[5].val = (
		DQEAPSLUT_AMBIENT_LIGHT(aps_lut[9]));
	/* DQEAPS_BACK_LIGHT */
	dqe->ctx.aps[6].val = (
		DQEAPSLUT_BACK_LIGHT(aps_lut[10]));
	/* DQEAPS_DSTEP */
	dqe->ctx.aps[7].val = (
		DQEAPSLUT_DSTEP(aps_lut[11]));
	/* DQEAPS_SCALE_MODE */
	dqe->ctx.aps[8].val = (
		DQEAPSLUT_SCALE_MODE(aps_lut[12]));
	/* DQEAPS_THRESHOLD */
	dqe->ctx.aps[9].val = (
		DQEAPSLUT_THRESHOLD_3(aps_lut[13]) | DQEAPSLUT_THRESHOLD_2(aps_lut[14]) |
		DQEAPSLUT_THRESHOLD_1(aps_lut[15]));
	/* DQEAPS_GAIN_LIMIT */
	dqe->ctx.aps[10].val = (
		DQEAPSLUT_GAIN_LIMIT(aps_lut[16]));
	/* DQEAPS_PARTIAL_CON */
	dqe->ctx.aps[11].val = (
		DQEAPSLUT_ROI_SAME(aps_lut[17]) | DQEAPSLUT_UPDATE_METHOD(aps_lut[18]) |
		DQEAPSLUT_PARTIAL_FRAME(aps_lut[19]));
	/* DQEAPS_PARTIAL_ROI_UP_LEFT_POS */
	dqe->ctx.aps[12].val = (
		DQEAPSLUT_ROI_Y1(aps_lut[20]) | DQEAPSLUT_ROI_X1(aps_lut[21]));
	/* DQEAPS_PARTIAL_IBSI_01_00 */
	dqe->ctx.aps[13].val = (
		DQEAPSLUT_IBSI_01(aps_lut[22]) | DQEAPSLUT_IBSI_00(aps_lut[23]));
	/* DQEAPS_PARTIAL_IBSI_11_10 */
	dqe->ctx.aps[14].val = (
		DQEAPSLUT_IBSI_11(aps_lut[24]) | DQEAPSLUT_IBSI_10(aps_lut[25]));
}


static ssize_t decon_dqe_gamma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, j, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 65; j++) {
			char num[5] = {0,};

			sprintf(num, "%d,", gamma_lut[i][j]);
			strcat(dqe_str_result, num);
		}
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = sprintf(buf, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_gamma_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("gamma write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 3; i++) {
			k = (i == 2) ? 64 : 65;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &gamma_lut[i][j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &gamma_lut[i-1][j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}


	for (i = 0; i < 3; i++)
		for (j = 0; j < 65; j++)
			dqe_dbg("%d ", gamma_lut[i][j]);

	dqe_gamma_lut_set();

	dqe->ctx.gamma_on = DQE_GAMMA_ON_MASK;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(gamma, 0660,
	decon_dqe_gamma_show,
	decon_dqe_gamma_store);

static ssize_t decon_dqe_cgc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 143; i++) {
		char num[5] = {0,};

		sprintf(num, "%d,", cgc_lut[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = sprintf(buf, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_cgc_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	s32 cgc_lut_signed[143] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 142;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtos32(head, 0, &cgc_lut_signed[j]);
				if (ret) {
					dqe_err("strtos32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtos32(head, 0, &cgc_lut_signed[j]);
		if (ret) {
			dqe_err("strtos32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (j = 0; j < 143; j++)
		dqe_dbg("%d ", cgc_lut_signed[j]);

	/* signed -> unsigned */
	for (j = 0; j < 143; j++) {
		cgc_lut[j] = (u32)cgc_lut_signed[j];
		dqe_dbg("%04x %d", cgc_lut[j], cgc_lut_signed[j]);
	}

	dqe_cgc_lut_set();

	dqe->ctx.cgc_on = DQE_CGC_ON_MASK;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(cgc, 0660,
	decon_dqe_cgc_show,
	decon_dqe_cgc_store);

static ssize_t decon_dqe_hsc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 154; i++) {
		char num[5] = {0,};

		sprintf(num, "%d,", hsc_lut[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = sprintf(buf, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_hsc_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	s32 hsc_lut_signed[154] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("hsc write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 153;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtos32(head, 0, &hsc_lut_signed[j]);
				if (ret) {
					dqe_err("strtos32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtos32(head, 0, &hsc_lut_signed[j]);
		if (ret) {
			dqe_err("strtos32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (j = 0; j < 154; j++)
		dqe_dbg("%d ", hsc_lut_signed[j]);

	/* signed -> unsigned */
	for (j = 0; j < 154; j++) {
		hsc_lut[j] = (u32)hsc_lut_signed[j];
		dqe_dbg("%04x %d", hsc_lut[j], hsc_lut_signed[j]);
	}

	dqe_hsc_lut_set();

	dqe->ctx.hsc_on = DQE_HSC_ON_MASK;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(hsc, 0660,
	decon_dqe_hsc_show,
	decon_dqe_hsc_store);

static ssize_t decon_dqe_aps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 26; i++) {
		char num[5] = {0,};

		sprintf(num, "%d,", aps_lut[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = sprintf(buf, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_aps_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("aps write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 25;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &aps_lut[j]);
				if (ret) {
					dqe_err("strtos32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtos32(head, 0, &aps_lut[j]);
		if (ret) {
			dqe_err("strtos32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (j = 0; j < 26; j++)
		dqe_dbg("%d ", aps_lut[j]);

	dqe_aps_lut_set();

	dqe->ctx.need_udpate = false;

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(aps, 0660,
	decon_dqe_aps_show,
	decon_dqe_aps_store);

static ssize_t decon_dqe_aps_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s\n", __func__);

	count = snprintf(buf, PAGE_SIZE, "aps_onoff = %d\n", dqe->ctx.aps_on);

	return count;
}

static ssize_t decon_dqe_aps_onoff_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("aps_onoff write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		goto err;

	dqe_info("%s: %d\n", __func__, value);

	if (!value)
		dqe->ctx.aps_on = 0;
	else
		dqe->ctx.aps_on = DQE_APS_ON_MASK;

	dqe->ctx.need_udpate = true;
#if 0
	switch (value) {
	case 1:
		dqe_write(DQEAPS_FULL_PXL_NUM, 0);
		dqe_write(DQEAPS_FULL_IMG_SIZESET, 0);
		dqe_reg_set_aps_full_pxl_num(decon->lcd_info);
		break;
	case 2:
		dqe_write(DQEAPS_FULL_PXL_NUM, 0);
		dqe_write(DQEAPS_FULL_IMG_SIZESET, 0);
		dqe_reg_set_aps_img_size(decon->lcd_info);
		break;
	case 3:
		dqe_write(DQEAPS_FULL_PXL_NUM, 0);
		dqe_write(DQEAPS_FULL_IMG_SIZESET, 0);
		dqe_reg_set_aps_full_pxl_num(decon->lcd_info);
		dqe_reg_set_aps_img_size(decon->lcd_info);
		break;
	case 4:
		dqe_write(DQEAPS_FULL_PXL_NUM, 0);
		dqe_write(DQEAPS_FULL_IMG_SIZESET, 0);
		break;
	case 11:
		for (i = 0; i < 65; i++)
			aps_gm_LUT64[i] = aps_gmarr_LUT64[0][i];
		break;
	case 12:
		for (i = 0; i < 65; i++)
			aps_gm_LUT64[i] = aps_gmarr_LUT64[1][i];
		break;
	case 13:
		for (i = 0; i < 65; i++)
			aps_gm_LUT64[i] = aps_gmarr_LUT64[2][i];
		break;
	default:
		break;
	}

	dqe_info("dqe DQEAPS_FULL_PXL_NUM: %x\n",
		dqe_read(DQEAPS_FULL_PXL_NUM));
	dqe_info("dqe DQEAPS_FULL_IMG_SIZESET: %x\n",
		dqe_read(DQEAPS_FULL_IMG_SIZESET));
#endif
	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(aps_onoff, 0660,
	decon_dqe_aps_onoff_show,
	decon_dqe_aps_onoff_store);

int dqe_aps_gamma_LUT64(int gm_input)
{
	int gm_index = 0;
	int gm_index_sub = 0;
	int gm_index_subS = 0;

	int gm_val_R = 0;
	int gm_val_L = 0;

	int gm_val_diff = 0;
	int gm_val_mul = 0;
	int gm_val_pre = 0;
	int gm_val_p = 0;

	//int gm_val = 0;
	int gm_DOUT_pre = 0;

	gm_index = (gm_input & APS_GAMMA_INPUT_MASK) >> APS_GAMMA_PHASE_SHIFT; //0~63
	gm_index_sub = (gm_input & APS_GAMMA_PHASE_MASK); //0~15
	gm_index_subS = (gm_input & APS_GAMMA_PHASE_MASK); //0~15

	gm_val_R = aps_gm_LUT64[gm_index + 1];
	gm_val_L = aps_gm_LUT64[gm_index];

	gm_val_diff = gm_val_R - gm_val_L; // LUT index Rvalue - Lvalue
	gm_val_mul = gm_val_diff * gm_index_subS;

	gm_val_pre = (0x0 | gm_val_L << APS_GAMMA_PHASE_SHIFT) + gm_val_mul;
	gm_val_p = ((gm_val_pre >> APS_GAMMA_PHASE_SHIFT) & APS_GAMMA_PIXEL_MAX) +
		((gm_val_pre >> (APS_GAMMA_PHASE_SHIFT - 1)) & 0x01);
	gm_DOUT_pre = (gm_val_p > APS_GAMMA_PIXEL_MAX) ? APS_GAMMA_PIXEL_MAX : gm_val_p;

	return gm_DOUT_pre;
}

int dqe_aps_convert_luma_to_al(int inLuma)
{
	int tmpAL;
	int maxLuma = 5000;
	int maxAL = 255;
	int outAL;

	if (inLuma > 5000)
		inLuma = 5000;

	// normailize
	tmpAL = (inLuma * maxAL) / maxLuma;

	outAL = dqe_aps_gamma_LUT64(tmpAL);

	dqe_dbg("%s: %4d -> %3d\n", __func__, inLuma, outAL);

	return outAL;
}

static ssize_t decon_dqe_aps_lux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	int ret = 0;

	mutex_lock(&dqe->lock);

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
	}

	if (ret > -1)
		ret = dqe->ctx.aps_lux;

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "%d\n", ret);

	return count;
}

static ssize_t decon_dqe_aps_lux_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;
	u32 outAL = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_dbg("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("aps_lux write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	decon_hiber_block_exit(decon);

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		goto err;

	dqe->ctx.aps_lux = value;
	outAL = (u32)dqe_aps_convert_luma_to_al(value);

	dqe_info("%s: %4d -> %3d\n", __func__, value, outAL);

	/* DQEAPS_AMBIENT_LIGHT */
	dqe->ctx.aps[5].val = (DQEAPSLUT_AMBIENT_LIGHT(outAL));

	dqe_write(dqe->ctx.aps[5].addr, dqe->ctx.aps[5].val);

	dqe->ctx.aps_on = DQE_APS_ON_MASK;

	dqe_reg_set_aps_full_pxl_num(decon->lcd_info);

	dqe_reg_set_aps_on(1);

	if (decon->lcd_info->mode == DECON_VIDEO_MODE)
		decon_reg_update_req_dqe(decon->id);

	decon_hiber_unblock(decon);

	mutex_unlock(&dqe->lock);

	dqe_dbg("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(aps_lux, 0660,
	decon_dqe_aps_lux_show,
	decon_dqe_aps_lux_store);

static ssize_t decon_dqe_xml_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dqe_device *dqe = dev_get_drvdata(dev);
	/*struct decon_device *decon = get_decon_drvdata(0);*/
	/*struct exynos_panel_info *lcd_info = decon->lcd_info;*/
	char *p = buf;
	int len;

	dqe_dbg("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';

	sprintf(dqe_str_result, "/vendor/etc/dqe/");

	strcat(dqe_str_result, "calib_data"/*lcd_info->ddi_name*/);

	strcat(dqe_str_result, ".xml");

	dqe_info("dqe xml_path: %s\n", dqe_str_result);

	len = sprintf(p, "%s\n", dqe_str_result);

	mutex_unlock(&dqe->lock);

	return len;
}

static DEVICE_ATTR(xml, 0440, decon_dqe_xml_show, NULL);

static int dqe_tune_mode_set(char *head, int mode)
{
	int ret = 0;
	int i = 0, j, k;
	char *ptr = NULL;
	s32 tune_mode_signed[492] = {0,};

	dqe_info("%s: %d \n", __func__, mode);

	if  (*head != 0) {
		dqe_dbg("%s\n", head);
			k = 492;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtos32(head, 0, &tune_mode_signed[j]);
				if (ret) {
					dqe_err("strtos32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}

	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	/* signed -> unsigned */
	for (i = 0; i < 492; i++) {
		tune_mode[mode][i] = (u32)tune_mode_signed[i];
		dqe_dbg("%04x %d", tune_mode[mode][i], tune_mode_signed[i]);
	}
err:
	if (ret)
		dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;

}

static ssize_t decon_dqe_tune_mode1_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	char *head = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("tune mode1 write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;

	ret = dqe_tune_mode_set(head, 1);
	if (ret)
		goto err;

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(tune_mode1, 0660,
	NULL,
	decon_dqe_tune_mode1_store);

static ssize_t decon_dqe_tune_mode2_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	char *head = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("tune mode2 write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;

	ret = dqe_tune_mode_set(head, 2);
	if (ret)
		goto err;

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(tune_mode2, 0660,
	NULL,
	decon_dqe_tune_mode2_store);

static ssize_t decon_dqe_tune_mode3_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	char *head = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("tune mode3 write count error\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;

	ret = dqe_tune_mode_set(head, 3);
	if (ret)
		goto err;

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(tune_mode3, 0660,
	NULL,
	decon_dqe_tune_mode3_store);

static ssize_t decon_dqe_aosp_color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t count = 0;

	dqe_info("%s\n", __func__);

	count = snprintf(buf, PAGE_SIZE, "aosp_colors = %d\n", val);

	return count;
}

static ssize_t decon_dqe_aosp_colors_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, ret = 0;
	unsigned int value = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	int cgc_len = 143;
	int gamma_len = 195;
	int hsc_len = 154;

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("aosp colors write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		goto err;

	dqe_info("%s: %d\n", __func__, value);

	switch (value) {
	case 0: /* Natural */
	case 1: /* Boosted */
	case 2: /* Saturated */
		for (i = 0; i < cgc_len; i++)
			cgc_lut[i] = tune_mode[value + 1][i];
		for (i = 0; i < hsc_len; i++)
			hsc_lut[i] = tune_mode[value + 1][i + cgc_len + gamma_len];
		break;
	default:
		for (i = 0; i < cgc_len; i++)
			cgc_lut[i] = tune_mode[0][i];
		for (i = 0; i < hsc_len; i++)
			hsc_lut[i] = tune_mode[0][i + cgc_len + gamma_len];
		break;
	}

	dqe_cgc_lut_set();
	dqe_hsc_lut_set();

	dqe->ctx.cgc_on = DQE_CGC_ON_MASK;
	dqe->ctx.hsc_on = DQE_HSC_ON_MASK;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static ssize_t decon_dqe_off_ctrl_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("dqe off write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		goto err;

	dqe_info("%s: %d\n", __func__, value);

	switch (value) {
	case 0:
		dqe->ctx.cgc_on = 0;
		dqe->ctx.gamma_on = 0;
		dqe->ctx.hsc_on = 0;
		dqe->ctx.aps_on = 0;
		break;
	case 1:
		dqe->ctx.cgc_on = 0;
		break;
	case 2:
		dqe->ctx.gamma_on = 0;
		break;
	case 3:
		dqe->ctx.hsc_on = 0;
		break;
	case 4:
		dqe->ctx.aps_on = 0;
		break;
	default:
		break;
	}

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(off_ctrl, 0660,
	NULL,
	decon_dqe_off_ctrl_store);

static DEVICE_ATTR(aosp_colors, 0660,
	decon_dqe_aosp_color_show,
	decon_dqe_aosp_colors_store);

static ssize_t decon_dqe_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t count = 0;
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s\n", __func__);

	decon_hiber_block_exit(decon);

	dqe_dump();

	decon_hiber_unblock(decon);

	count = snprintf(buf, PAGE_SIZE, "dump = %d\n", val);

	return count;
}

static DEVICE_ATTR(dump, 0440,
	decon_dqe_dump_show,
	NULL);

static struct attribute *dqe_attrs[] = {
	&dev_attr_gamma.attr,
	&dev_attr_cgc.attr,
	&dev_attr_hsc.attr,
	&dev_attr_aps.attr,
	&dev_attr_aps_onoff.attr,
	&dev_attr_aps_lux.attr,
	&dev_attr_xml.attr,
	&dev_attr_tune_mode1.attr,
	&dev_attr_tune_mode2.attr,
	&dev_attr_tune_mode3.attr,
	&dev_attr_aosp_colors.attr,
	&dev_attr_off_ctrl.attr,
	&dev_attr_dump.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dqe);

int decon_dqe_set_color_mode(struct decon_color_mode_with_render_intent_info *mode)
{
	int i, ret = 0;
	struct dqe_device *dqe = dqe_drvdata;
	struct decon_device *decon = get_decon_drvdata(0);

	mutex_lock(&dqe->lock);

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	dqe_info("%s : %d\n", __func__, mode->color_mode);

	switch (mode->color_mode) {
	case HAL_COLOR_MODE_SRGB:
	case HAL_COLOR_MODE_DCI_P3:
		if (dqe->ctx.boosted_on) {
			for (i = 0; i < 50; i++)
				cgc_lut[i] = tune_mode[2][i];
			for (i = 0; i < 35; i++)
				hsc_lut[i] = tune_mode[2][i + 245];
		} else {
			for (i = 0; i < 50; i++)
				cgc_lut[i] = tune_mode[1][i];
			for (i = 0; i < 35; i++)
				hsc_lut[i] = tune_mode[1][i + 245];
		}
		break;

	case HAL_COLOR_MODE_NATIVE:
		for (i = 0; i < 50; i++)
			cgc_lut[i] = tune_mode[3][i];
		for (i = 0; i < 35; i++)
			hsc_lut[i] = tune_mode[3][i + 245];
		break;

	default:
		for (i = 0; i < 50; i++)
			cgc_lut[i] = tune_mode[0][i];
		for (i = 0; i < 35; i++)
			hsc_lut[i] = tune_mode[0][i + 245];
		break;
	}

	dqe_cgc_lut_set();
	dqe_hsc_lut_set();
	if (!dqe->ctx.night_light_on)
		dqe_gamma_lut_set();

	dqe->ctx.color_mode = mode->color_mode;

	if (!dqe->ctx.night_light_on) {
		dqe->ctx.cgc_on = DQE_CGC_ON_MASK;
		dqe->ctx.hsc_on = DQE_HSC_ON_MASK;
	}
	else {
		dqe->ctx.cgc_on = 0;
		dqe->ctx.hsc_on = 0;
	}

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : ret(%d), cgc:%d, hsc:%d, night_light:%d\n", __func__,
		ret, dqe->ctx.cgc_on, dqe->ctx.hsc_on, dqe->ctx.night_light_on);

	return ret;
}

static void dqe_color_transform_make_sfr(int coeff[16], int inR, int inG, int inB, int *outR, int *outG, int *outB)
{
	*outR = (coeff[ 0] * inR) >> 15;
	*outR = (*outR & 0x1) ? (*outR >> 1) + 1 : *outR >> 1;
	*outG = (coeff[ 5] * inG) >> 15;
	*outG = (*outG & 0x1) ? (*outG >> 1) + 1 : *outG >> 1;
	*outB = (coeff[10] * inB) >> 15;
	*outB = (*outB & 0x1) ? (*outB >> 1) + 1 : *outB >> 1;
}

static void dqe_color_transform_night_light(int in[16], int out[16])
{
	int kk, jj;
	long long int inverse_i[16] = {60847, 1269, 1269, 0, 4260, 63838, 4260, 0, 429, 429, 60007, 0, 0, 0, 0, 65536};

	for (kk = 0; kk < 4; kk++)
		for (jj = 0; jj < 4; jj++)
			out[kk * 4 + jj] = (inverse_i[kk * 4] * in[jj] + inverse_i[kk * 4 + 1] * in[jj + 4] + inverse_i[kk * 4 + 2] * in[jj + 8] + inverse_i[kk * 4 + 3] * in[jj + 12]) >> 16;
}

int decon_dqe_set_color_transform(struct decon_color_transform_info *transform)
{
	int ret = 0;
	int i, j;
	int input[16] = {0,};
	int temp[16] = {0,};
	struct dqe_device *dqe = dqe_drvdata;
	struct decon_device *decon = get_decon_drvdata(0);
	int diag_matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

	mutex_lock(&dqe->lock);

	dqe->ctx.boosted_on = 0;

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		goto err;
	}

	if (transform->matrix[0] != 65536/*transform->hint*/) {
		for (i = 0; i < 16; i++)
			input[i] = temp[i] = transform->matrix[i];

		dqe_color_transform_night_light(temp, transform->matrix);

		for (i = 0; i < 16; i++)
			if (transform->matrix[i] == -1 || transform->matrix[i] == 1)
				transform->matrix[i] = 0;

		dqe->ctx.boosted_on = 1;
	}

	for (i = 0; i < 16; i++) {
		if (transform->matrix[i] && diag_matrix[i])
			continue;
		if (!transform->matrix[i] && !diag_matrix[i])
			continue;

		for (j = 0; j < 4; j++)
			dqe_info("%6d %6d %6d %6d\n",
				input[j * 4], input[j * 4 + 1],
				input[j * 4 + 2], input[j * 4 + 3]);
		for (j = 0; j < 4; j++)
			dqe_info("%6d %6d %6d %6d\n",
				transform->matrix[j * 4], transform->matrix[j * 4 + 1],
				transform->matrix[j * 4 + 2], transform->matrix[j * 4 + 3]);
		ret = -1;
		goto err;
	}

	if (transform->matrix[0] == 65536 &&
		transform->matrix[5] == 65536 &&
		transform->matrix[10] == 65536 &&
		transform->matrix[15] == 65536)
		dqe->ctx.night_light_on = 0;
	else
		dqe->ctx.night_light_on = 1;

	if (dqe->ctx.color_mode == HAL_COLOR_MODE_SRGB ||
		dqe->ctx.color_mode == HAL_COLOR_MODE_DCI_P3) {
		if (dqe->ctx.boosted_on) {
			for (i = 0; i < 50; i++)
				cgc_lut[i] = tune_mode[2][i];
			for (i = 0; i < 35; i++)
				hsc_lut[i] = tune_mode[2][i + 245];
		} else {
			for (i = 0; i < 50; i++)
				cgc_lut[i] = tune_mode[1][i];
			for (i = 0; i < 35; i++)
				hsc_lut[i] = tune_mode[1][i + 245];
		}
		dqe_cgc_lut_set();
		dqe_hsc_lut_set();
		dqe->ctx.cgc_on = DQE_CGC_ON_MASK;
		dqe->ctx.hsc_on = DQE_HSC_ON_MASK;
	}

	for (i = 0; i < 3; i++)
		for (j = 0; j < 65; j++)
			gamma_lut[i][j] = bypass_gamma_tune[i][j];

	for (j = 0; j < 65; j++) {
		int inR, inG, inB, outR, outG, outB;

		inR = gamma_lut[0][j];
		inG = gamma_lut[1][j];
		inB = gamma_lut[2][j];

		dqe_color_transform_make_sfr(transform->matrix, inR, inG, inB, &outR, &outG, &outB);

		gamma_lut[0][j] = (u32)outR;
		gamma_lut[1][j] = (u32)outG;
		gamma_lut[2][j] = (u32)outB;
	}

	for (i = 0; i < 3; i++)
		for (j = 0; j < 65; j++)
			dqe_dbg("%d ", gamma_lut[i][j]);

	dqe_gamma_lut_set();

	dqe->ctx.gamma_on = DQE_GAMMA_ON_MASK;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : ret=%d color_mode=%d, hint=%d, boosted=%d\n",
		__func__,ret , dqe->ctx.color_mode, transform->hint, dqe->ctx.boosted_on);

	return ret;
}

void decon_dqe_sw_reset(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_reg_hsc_sw_reset(decon);
}

void decon_dqe_lpd_data_read(struct decon_device *decon)
{
	struct dqe_device *dqe = dqe_drvdata;

	if (!decon->hiber.enabled)
		return;

	if (decon->id)
		return;

	dqe_reg_hsc_lpd_read(dqe);

	dqe_dbg("dqe lpd data read (%x, %x) DQECON (%x)\n",
		dqe->lpd_data[0], dqe->lpd_data[1], dqe_read(DQECON));
}

void decon_dqe_lpd_data_write(struct decon_device *decon)
{
	struct dqe_device *dqe = dqe_drvdata;

	if (!decon->hiber.enabled)
		return;

	if (decon->id)
		return;

	dqe_reg_set_lpd_mode_exit(1);
	dqe_reg_hsc_lpd_write(dqe);
}

void decon_dqe_restore_context(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_restore_context();
}

void decon_dqe_start(struct decon_device *decon, struct exynos_panel_info *lcd_info)
{
	if (decon->id)
		return;

	dqe_reg_start(decon->id, lcd_info);
}

void decon_dqe_enable(struct decon_device *decon)
{
	u32 val;

	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_restore_context();
	dqe_reg_start(decon->id, decon->lcd_info);

	val = dqe_read(DQECON);
	dqe_info("dqe gamma:%d cgc:%d hsc:%d aps:%d\n",
			DQE_GAMMA_ON_GET(val),
			DQE_CGC_ON_GET(val),
			DQE_HSC_ON_GET(val),
			DQE_APS_ON_GET(val));
}

void decon_dqe_disable(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_save_context();
	dqe_reg_stop(decon->id);
}

int decon_dqe_create_interface(struct decon_device *decon)
{
	int ret = 0;
	struct dqe_device *dqe;

	if (decon->id || (decon->dt.out_type != DECON_OUT_DSI)) {
		dqe_info("decon%d doesn't need dqe interface\n", decon->id);
		return 0;
	}

	dqe = kzalloc(sizeof(struct dqe_device), GFP_KERNEL);
	if (!dqe) {
		ret = -ENOMEM;
		goto exit0;
	}

	dqe_drvdata = dqe;
	dqe->decon = decon;

	if (IS_ERR_OR_NULL(dqe_class)) {
		dqe_class = class_create(THIS_MODULE, "dqe");
		if (IS_ERR_OR_NULL(dqe_class)) {
			pr_err("failed to create dqe class\n");
			ret = -EINVAL;
			goto exit1;
		}

		dqe_class->dev_groups = dqe_groups;
	}

	dqe->dev = device_create(dqe_class, decon->dev, 0,
			&dqe, "dqe", 0);
	if (IS_ERR_OR_NULL(dqe->dev)) {
		pr_err("failed to create dqe device\n");
		ret = -EINVAL;
		goto exit2;
	}

	mutex_init(&dqe->lock);
	mutex_init(&dqe->restore_lock);
	dev_set_drvdata(dqe->dev, dqe);

	dqe_load_context();

	dqe_init_context();

	dqe_info("decon_dqe_create_interface done.\n");

	return ret;
exit2:
	class_destroy(dqe_class);
exit1:
	kfree(dqe);
exit0:
	return ret;
}
