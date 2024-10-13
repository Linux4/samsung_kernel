/*
 * drivers/video/decon/panels/s6e3fa3_a8xe_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3fa3_a8xe_aid_dimming.h"
#endif
#include "s6e3fa3_a8xe_param.h"
#include "../dsim.h"
#include <video/mipi_display.h>

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
void update_mdnie_coordinate( u16 coordinate0, u16 coordinate1 );
static int lcd_reload_mtp(struct dsim_device *dsim);
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned int br_tbl_420[EXTEND_BRIGHTNESS + 1] = {
	2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
	6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10,
	10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15,
	15, 16, 16, 17, 17, 19, 19, 20, 20, 21, 21, 22, 22, 24, 24, 25,
	25, 27, 27, 29, 29, 30, 30, 32, 32, 34, 34, 37, 37, 39, 39, 41,
	41, 44, 44, 47, 47, 50, 50, 53, 53, 56, 56, 60, 60, 64, 64, 68,
	68, 72, 72, 77, 77, 82, 82, 87, 87, 93, 93, 98, 98, 105, 105, 111,
	111, 119, 119, 126, 126, 134, 134, 143, 143, 152, 152, 162, 162, 172, 172, 183,
	183, 195, 195, 195, 195, 195, 195, 195, 207, 207, 207, 207, 207, 207, 207, 220,
	220, 220, 220, 220, 220, 220, 234, 234, 234, 234, 234, 234, 234, 234, 249, 249,
	249, 249, 249, 249, 249, 249, 265, 265, 265, 265, 265, 265, 265, 265, 265, 282,
	282, 282, 282, 282, 282, 282, 282, 282, 300, 300, 300, 300, 300, 300, 300, 300,
	300, 300, 316, 316, 316, 316, 316, 316, 316, 316, 333, 333, 333, 333, 333, 333,
	333, 333, 333, 350, 350, 350, 350, 350, 350, 350, 350, 350, 357, 357, 357, 357,
	365, 365, 365, 365, 372, 372, 372, 380, 380, 380, 380, 387, 387, 387, 387, 395,
	395, 395, 395, 403, 403, 403, 403, 412, 412, 412, 412, 420, 420, 420, 420, 420,
	[256 ... 281] = 420,
	[282 ... 295] = 465,
	[296 ... 309] = 488,
	[310 ... 323] = 510,
	[324 ... 336] = 533,
	[337 ... 350] = 555,
	[351 ... 364] = 578,
	[365 ... 365] = 600
};

static const short center_gamma[NUM_VREF][CI_MAX] = {
	{ 0x000, 0x000, 0x000 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x100, 0x100, 0x100 },
};

// aid sheet
struct SmtDimInfo dimming_info[MAX_BR_INFO] = { // add hbm array
	{ .br = 2,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl2nit,	.cTbl = ctbl2nit,	.elv = elvss_2nit,	.way = W1},
	{ .br = 3,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl3nit,	.cTbl = ctbl3nit,	.elv = elvss_3nit,	.way = W1},
	{ .br = 4,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl4nit,	.cTbl = ctbl4nit,	.elv = elvss_4nit,	.way = W1},
	{ .br = 5,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl5nit,	.cTbl = ctbl5nit,	.elv = elvss_5nit,	.way = W1},
	{ .br = 6,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl6nit,	.cTbl = ctbl6nit,	.elv = elvss_6nit,	.way = W1},
	{ .br = 7,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl7nit,	.cTbl = ctbl7nit,	.elv = elvss_7nit,	.way = W1},
	{ .br = 8,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl8nit,	.cTbl = ctbl8nit,	.elv = elvss_8nit,	.way = W1},
	{ .br = 9,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl9nit,	.cTbl = ctbl9nit,	.elv = elvss_9nit,	.way = W1},
	{ .br = 10,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl10nit,	.cTbl = ctbl10nit,	.elv = elvss_10nit,	.way = W1},
	{ .br = 11,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl11nit,	.cTbl = ctbl11nit,	.elv = elvss_11nit,	.way = W1},
	{ .br = 12,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl12nit,	.cTbl = ctbl12nit,	.elv = elvss_12nit,	.way = W1},
	{ .br = 13,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl13nit,	.cTbl = ctbl13nit,	.elv = elvss_13nit,	.way = W1},
	{ .br = 14,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl14nit,	.cTbl = ctbl14nit,	.elv = elvss_14nit,	.way = W1},
	{ .br = 15,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl15nit,	.cTbl = ctbl15nit,	.elv = elvss_15nit,	.way = W1},
	{ .br = 16,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl16nit,	.cTbl = ctbl16nit,	.elv = elvss_16nit,	.way = W1},
	{ .br = 17,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl17nit,	.cTbl = ctbl17nit,	.elv = elvss_17nit,	.way = W1},
	{ .br = 19,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl19nit,	.cTbl = ctbl19nit,	.elv = elvss_19nit,	.way = W1},
	{ .br = 20,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl20nit,	.cTbl = ctbl20nit,	.elv = elvss_20nit,	.way = W1},
	{ .br = 21,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl21nit,	.cTbl = ctbl21nit,	.elv = elvss_21nit,	.way = W1},
	{ .br = 22,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl22nit,	.cTbl = ctbl22nit,	.elv = elvss_22nit,	.way = W1},
	{ .br = 24,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl24nit,	.cTbl = ctbl24nit,	.elv = elvss_24nit,	.way = W1},
	{ .br = 25,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl25nit,	.cTbl = ctbl25nit,	.elv = elvss_25nit,	.way = W1},
	{ .br = 27,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl27nit,	.cTbl = ctbl27nit,	.elv = elvss_27nit,	.way = W1},
	{ .br = 29,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl29nit,	.cTbl = ctbl29nit,	.elv = elvss_29nit,	.way = W1},
	{ .br = 30,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl30nit,	.cTbl = ctbl30nit,	.elv = elvss_30nit,	.way = W1},
	{ .br = 32,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl32nit,	.cTbl = ctbl32nit,	.elv = elvss_32nit,	.way = W1},
	{ .br = 34,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl34nit,	.cTbl = ctbl34nit,	.elv = elvss_34nit,	.way = W1},
	{ .br = 37,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl37nit,	.cTbl = ctbl37nit,	.elv = elvss_37nit,	.way = W1},
	{ .br = 39,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl39nit,	.cTbl = ctbl39nit,	.elv = elvss_39nit,	.way = W1},
	{ .br = 41,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl41nit,	.cTbl = ctbl41nit,	.elv = elvss_41nit,	.way = W1},
	{ .br = 44,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl44nit,	.cTbl = ctbl44nit,	.elv = elvss_44nit,	.way = W1},
	{ .br = 47,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl47nit,	.cTbl = ctbl47nit,	.elv = elvss_47nit,	.way = W1},
	{ .br = 50,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl50nit,	.cTbl = ctbl50nit,	.elv = elvss_50nit,	.way = W1},
	{ .br = 53,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl53nit,	.cTbl = ctbl53nit,	.elv = elvss_53nit,	.way = W1},
	{ .br = 56,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl56nit,	.cTbl = ctbl56nit,	.elv = elvss_56nit,	.way = W1},
	{ .br = 60,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl60nit,	.cTbl = ctbl60nit,	.elv = elvss_60nit,	.way = W1},
	{ .br = 64,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl64nit,	.cTbl = ctbl64nit,	.elv = elvss_64nit,	.way = W1},
	{ .br = 68,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl68nit,	.cTbl = ctbl68nit,	.elv = elvss_68nit,	.way = W1},
	{ .br = 72,	.refBr = 120, .cGma = gma2p15, .rTbl = rtbl72nit,	.cTbl = ctbl72nit,	.elv = elvss_72nit,	.way = W1},
	{ .br = 77,	.refBr = 125, .cGma = gma2p15, .rTbl = rtbl77nit,	.cTbl = ctbl77nit,	.elv = elvss_77nit,	.way = W1},
	{ .br = 82,	.refBr = 133, .cGma = gma2p15, .rTbl = rtbl82nit,	.cTbl = ctbl82nit,	.elv = elvss_82nit,	.way = W1},
	{ .br = 87,	.refBr = 140, .cGma = gma2p15, .rTbl = rtbl87nit,	.cTbl = ctbl87nit,	.elv = elvss_87nit,	.way = W1},
	{ .br = 93,	.refBr = 149, .cGma = gma2p15, .rTbl = rtbl93nit,	.cTbl = ctbl93nit,	.elv = elvss_93nit,	.way = W1},
	{ .br = 98,	.refBr = 157, .cGma = gma2p15, .rTbl = rtbl98nit,	.cTbl = ctbl98nit,	.elv = elvss_98nit,	.way = W1},
	{ .br = 105,	.refBr = 167, .cGma = gma2p15, .rTbl = rtbl105nit,	.cTbl = ctbl105nit,	.elv = elvss_105nit,	.way = W1},
	{ .br = 111,	.refBr = 175, .cGma = gma2p15, .rTbl = rtbl111nit,	.cTbl = ctbl111nit,	.elv = elvss_111nit,	.way = W1},
	{ .br = 119,	.refBr = 187, .cGma = gma2p15, .rTbl = rtbl119nit,	.cTbl = ctbl119nit,	.elv = elvss_119nit,	.way = W1},
	{ .br = 126,	.refBr = 197, .cGma = gma2p15, .rTbl = rtbl126nit,	.cTbl = ctbl126nit,	.elv = elvss_126nit,	.way = W1},
	{ .br = 134,	.refBr = 207, .cGma = gma2p15, .rTbl = rtbl134nit,	.cTbl = ctbl134nit,	.elv = elvss_134nit,	.way = W1},
	{ .br = 143,	.refBr = 221, .cGma = gma2p15, .rTbl = rtbl143nit,	.cTbl = ctbl143nit,	.elv = elvss_143nit,	.way = W1},
	{ .br = 152,	.refBr = 235, .cGma = gma2p15, .rTbl = rtbl152nit,	.cTbl = ctbl152nit,	.elv = elvss_152nit,	.way = W1},
	{ .br = 162,	.refBr = 246, .cGma = gma2p15, .rTbl = rtbl162nit,	.cTbl = ctbl162nit,	.elv = elvss_162nit,	.way = W1},
	{ .br = 172,	.refBr = 262, .cGma = gma2p15, .rTbl = rtbl172nit,	.cTbl = ctbl172nit,	.elv = elvss_172nit,	.way = W1},
	{ .br = 183,	.refBr = 275, .cGma = gma2p15, .rTbl = rtbl183nit,	.cTbl = ctbl183nit,	.elv = elvss_183nit,	.way = W1},
	{ .br = 195,	.refBr = 275, .cGma = gma2p15, .rTbl = rtbl195nit,	.cTbl = ctbl195nit,	.elv = elvss_195nit,	.way = W1},
	{ .br = 207,	.refBr = 275, .cGma = gma2p15, .rTbl = rtbl207nit,	.cTbl = ctbl207nit,	.elv = elvss_207nit,	.way = W1},
	{ .br = 220,	.refBr = 275, .cGma = gma2p15, .rTbl = rtbl220nit,	.cTbl = ctbl220nit,	.elv = elvss_220nit,	.way = W1},
	{ .br = 234,	.refBr = 275, .cGma = gma2p15, .rTbl = rtbl234nit,	.cTbl = ctbl234nit,	.elv = elvss_234nit,	.way = W1},
	{ .br = 249,	.refBr = 282, .cGma = gma2p15, .rTbl = rtbl249nit,	.cTbl = ctbl249nit,	.elv = elvss_249nit,	.way = W1},
	{ .br = 265,	.refBr = 299, .cGma = gma2p15, .rTbl = rtbl265nit,	.cTbl = ctbl265nit,	.elv = elvss_265nit,	.way = W1},
	{ .br = 282,	.refBr = 317, .cGma = gma2p15, .rTbl = rtbl282nit,	.cTbl = ctbl282nit,	.elv = elvss_282nit,	.way = W1},
	{ .br = 300,	.refBr = 337, .cGma = gma2p15, .rTbl = rtbl300nit,	.cTbl = ctbl300nit,	.elv = elvss_300nit,	.way = W1},
	{ .br = 316,	.refBr = 353, .cGma = gma2p15, .rTbl = rtbl316nit,	.cTbl = ctbl316nit,	.elv = elvss_316nit,	.way = W1},
	{ .br = 333,	.refBr = 373, .cGma = gma2p15, .rTbl = rtbl333nit,	.cTbl = ctbl333nit,	.elv = elvss_333nit,	.way = W1},
	{ .br = 350,	.refBr = 390, .cGma = gma2p15, .rTbl = rtbl350nit,	.cTbl = ctbl350nit,	.elv = elvss_350nit,	.way = W1},
	{ .br = 357,	.refBr = 396, .cGma = gma2p15, .rTbl = rtbl357nit,	.cTbl = ctbl357nit,	.elv = elvss_357nit,	.way = W1},
	{ .br = 365,	.refBr = 405, .cGma = gma2p15, .rTbl = rtbl365nit,	.cTbl = ctbl365nit,	.elv = elvss_365nit,	.way = W1},
	{ .br = 372,	.refBr = 405, .cGma = gma2p15, .rTbl = rtbl372nit,	.cTbl = ctbl372nit,	.elv = elvss_372nit,	.way = W1},
	{ .br = 380,	.refBr = 405, .cGma = gma2p15, .rTbl = rtbl380nit,	.cTbl = ctbl380nit,	.elv = elvss_380nit,	.way = W1},
	{ .br = 387,	.refBr = 405, .cGma = gma2p15, .rTbl = rtbl387nit,	.cTbl = ctbl387nit,	.elv = elvss_387nit,	.way = W1},
	{ .br = 395,	.refBr = 405, .cGma = gma2p15, .rTbl = rtbl395nit,	.cTbl = ctbl395nit,	.elv = elvss_395nit,	.way = W1},
	{ .br = 403,	.refBr = 407, .cGma = gma2p15, .rTbl = rtbl403nit,	.cTbl = ctbl403nit,	.elv = elvss_403nit,	.way = W1},
	{ .br = 412,	.refBr = 414, .cGma = gma2p15, .rTbl = rtbl412nit,	.cTbl = ctbl412nit,	.elv = elvss_412nit,	.way = W1},
	{ .br = 420,	.refBr = 420, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_420nit,	.way = W2},
/*hbm interpolation */
	{ .br = 443,	.refBr = 443, .cGma = gma2p20, .rTbl = rtbl420nit, 	.cTbl = ctbl420nit,	.elv = elvss_443nit,	.way = W3},
	{ .br = 465,	.refBr = 465, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_465nit,	.way = W3},
	{ .br = 488,	.refBr = 488, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_488nit,	.way = W3},
	{ .br = 510,	.refBr = 510, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_510nit,	.way = W3},
	{ .br = 533,	.refBr = 533, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_533nit,	.way = W3},
	{ .br = 555,	.refBr = 555, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_555nit,	.way = W3},
	{ .br = 578,	.refBr = 578, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_578nit,	.way = W3},
/* hbm */
	{ .br = 600,	.refBr = 600, .cGma = gma2p20, .rTbl = rtbl420nit,	.cTbl = ctbl420nit,	.elv = elvss_600nit,	.way = W4},
};

static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_ON};
static const unsigned char *OPR_TABLE[OPR_STATUS_MAX] = {SEQ_OPR_ACL_OFF, SEQ_OPR_ACL_ON, SEQ_OPR_ACL_ON_HBM};

static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
	int     i, j;
	int     ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = (unsigned char)((center_gamma[i][j] >> 8) & 0x01);
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
			else {
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
		}
	}
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}

static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, u8 * hbm)
{
	int     ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	memset(result, 0, OLED_CMD_GAMMA_CNT);

	result[index++] = OLED_CMD_GAMMA;
	result[index++] = ((hbm[0] >> 2) & 0x1);
	result[index++] = hbm[1];
	result[index++] = ((hbm[0] >> 1) & 0x1);
	result[index++] = hbm[2];
	result[index++] = (hbm[0] & 0x1);
	result[index++] = hbm[3];
	memcpy(result + index, hbm + 4, S6E3FA3_HBMGAMMA_LEN - 4);

	return ret;
}

/* gamma interpolaion table */
const unsigned int tbl_hbm_inter[7] = {
	131, 256, 387, 512, 643, 768, 899
};

static int interpolation_gamma_to_hbm(struct SmtDimInfo *dimInfo, int br_idx)
{
	int     i, j;
	int     ret = 0;
	int     idx = 0;
	int     tmp = 0;
	int     hbmcnt, refcnt, gap = 0;
	int     ref_idx = 0;
	int     hbm_idx = 0;
	int     rst = 0;
	int     hbm_tmp, ref_tmp;
	unsigned char *result = dimInfo[br_idx].gamma;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (dimInfo[i].br == S6E3FA3_MAX_BRIGHTNESS)
			ref_idx = i;

		if (dimInfo[i].br == S6E3FA3_HBM_BRIGHTNESS)
			hbm_idx = i;
	}

	if ((ref_idx == 0) || (hbm_idx == 0)) {
		dsim_info("%s failed to get index ref index : %d, hbm index : %d\n", __func__, ref_idx, hbm_idx);
		ret = -EINVAL;
		goto exit;
	}

	result[idx++] = OLED_CMD_GAMMA;
	tmp = (br_idx - ref_idx) - 1;

	hbmcnt = 1;
	refcnt = 1;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				hbm_tmp = (dimInfo[hbm_idx].gamma[hbmcnt] << 8) | (dimInfo[hbm_idx].gamma[hbmcnt + 1]);
				ref_tmp = (dimInfo[ref_idx].gamma[refcnt] << 8) | (dimInfo[ref_idx].gamma[refcnt + 1]);

				if (hbm_tmp > ref_tmp) {
					gap = hbm_tmp - ref_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst += ref_tmp;
				}
				else {
					gap = ref_tmp - hbm_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst = ref_tmp - rst;
				}
				result[idx++] = (unsigned char)((rst >> 8) & 0x01);
				result[idx++] = (unsigned char)rst & 0xff;
				hbmcnt += 2;
				refcnt += 2;
			}
			else {
				hbm_tmp = dimInfo[hbm_idx].gamma[hbmcnt++];
				ref_tmp = dimInfo[ref_idx].gamma[refcnt++];

				if (hbm_tmp > ref_tmp) {
					gap = hbm_tmp - ref_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst += ref_tmp;
				}
				else {
					gap = ref_tmp - hbm_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst = ref_tmp - rst;
				}
				result[idx++] = (unsigned char)rst & 0xff;
			}
		}
	}

	dsim_info("tmp index : %d\n", tmp);

exit:
	return ret;
}

static int init_dimming(struct dsim_device *dsim, u8 * mtp, u8 * hbm)
{
	int     i, j;
	int     pos = 0;
	int     ret = 0;
	short   temp;
	int     method = 0;
    	static struct dim_data *dimming = NULL;

	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	int     string_offset;
	char    string_buf[1024];

	if( dimming == NULL ) {
        dimming = (struct dim_data *) kmalloc(sizeof(struct dim_data), GFP_KERNEL);
	        if (!dimming) {
		        dsim_err("failed to allocate memory for dim data\n");
		        ret = -ENOMEM;
		        goto error;
	        }
	}

	dsim_info("%s init dimming info for S6E3FA3 panel\n", __func__);
	diminfo = (void *)dimming_info;
	panel->acl_opr_tbl = (unsigned char **)OPR_TABLE;

	panel->br_tbl = (unsigned int *)br_tbl_420;
	panel->inter_aor_tbl = (unsigned char *)aor_table;

	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->hbm_tbl = (unsigned char **)HBM_TABLE;
	panel->acl_cutoff_tbl = (unsigned char **)ACL_TABLE;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos + 1];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
	/* for vt */
	dimming->vt_mtp[CI_RED] = (mtp[pos] & 0xf0) >> 4;
	dimming->vt_mtp[CI_GREEN] = mtp[pos] & 0x0f;
	dimming->vt_mtp[CI_BLUE] = mtp[pos + 1] & 0x0f;

#ifdef SMART_DIMMING_DEBUG
	dimm_info("Center Gamma Info : \n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
	}
#endif
	dimm_info("VT MTP : \n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
		dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
		dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

	dimm_info("MTP Info : \n");
	for (i = 0; i < VMAX; i++) {
		dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE]);
	}

	ret = generate_volt_table(dimming);
	if (ret) {
		dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;

		if (method == DIMMING_METHOD_FILL_CENTER) {
			ret = set_gamma_to_center(&diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to get center gamma\n", __func__);
				goto error;
			}
		}
		else if (method == DIMMING_METHOD_FILL_HBM) {
			ret = set_gamma_to_hbm(&diminfo[i], hbm);
			if (ret) {
				dsim_err("%s : failed to get hbm gamma\n", __func__);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;
		if (method == DIMMING_METHOD_AID) {
			ret = cal_gamma_from_index(dimming, &diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
		if (method == DIMMING_METHOD_INTERPOLATION) {
			ret = interpolation_gamma_to_hbm(diminfo, i);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		memset(string_buf, 0, sizeof(string_buf));
		string_offset = sprintf(string_buf, "gamma[%3d] : ", diminfo[i].br);

		for (j = 0; j < OLED_CMD_GAMMA_CNT; j++)
			string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[i].gamma[j]);

		dsim_info("%s\n", string_buf);
	}
error:
	return ret;

}

#endif

static int s6e3fa3_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
	int     i = 0;
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char buf[S6E3FA3_MTP_DATE_LEN] = { 0, };
	unsigned char bufForCoordi[S6E3FA3_COORDINATE_LEN] = { 0, };
	unsigned char hbm_gamma[S6E3FA3_HBMGAMMA_LEN] = { 0, };
	unsigned char bufForElvss[S6E3FA3_ELVSS_LEN];
	int len;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_read_hl_data(dsim, S6E3FA3_ID_REG, S6E3FA3_ID_LEN, dsim->priv.id);
	if (ret != S6E3FA3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_fail;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3FA3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_read_hl_data(dsim, S6E3FA3_MTP_REG, S6E3FA3_MTP_DATE_LEN, buf);
	if (ret < S6E3FA3_MTP_LEN) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E3FA3_MTP_LEN);
	memcpy(dsim->priv.date, &buf[S6E3FA3_MTP_DATE_OFFSET], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3FA3_MTP_LEN);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3FA3_MTP_LEN; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E3FA3_COORDINATE_REG, S6E3FA3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3FA3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	// code
	ret = dsim_read_hl_data(dsim, S6E3FA3_CHIP_ID_REG, S6E3FA3_CHIP_ID_LEN, dsim->priv.code);
	if (ret != S6E3FA3_CHIP_ID_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for (i = 0; i < S6E3FA3_CHIP_ID_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");

	// elvss
	ret = dsim_read_hl_data(dsim, S6E3FA3_ELVSS_REG, S6E3FA3_ELVSS_LEN, bufForElvss);
	if (ret < S6E3FA3_ELVSS_LEN) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("READ elvss : ");
	for (i = 0; i < S6E3FA3_ELVSS_LEN; i++)
		dsim_info("%x \n", bufForElvss[i]);

	len = sizeof(dsim->priv.elvss_set);
	if(len > sizeof(bufForElvss))
		len = sizeof(bufForElvss);

	memcpy(dsim->priv.elvss_set, bufForElvss, len);
	/* read hbm elvss 0xB6 23th value */
	panel->hbm_elvss = bufForElvss[S6E3FA3_ELVSS_LEN - 1];

	ret = dsim_read_hl_data(dsim, S6E3FA3_HBMGAMMA_REG, S6E3FA3_HBMGAMMA_LEN, hbm_gamma);
	if (ret != S6E3FA3_HBMGAMMA_LEN) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("HBM Gamma : ");
	memcpy(hbm, hbm_gamma, S6E3FA3_HBMGAMMA_LEN);

	for (i = 0; i < S6E3FA3_HBMGAMMA_LEN; i++)
		dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto read_exit;
	}
	ret = 0;
#ifdef CONFIG_CHECK_OCTA_CHIP_ID
	// octa_id
	ret = dsim_read_hl_data(dsim, S6E3FA3_OCTAID_REG, S6E3FA3_OCTAID_LEN, panel->octa_id);
	if (ret != S6E3FA3_OCTAID_LEN) {
		dsim_err("fail to read octa_id command.\n");
		goto read_fail;
	}
	dsim_info("READ octa_id : ");
	for(i = 1; i < S6E3FA3_OCTAID_LEN; i++)
		dsim_info("%x, ", dsim->priv.octa_id[i]);
	dsim_info("\n");
#endif

read_exit:
	return 0;

read_fail:
	return -ENODEV;
}
static int s6e3fa3_a8xe_dump(struct dsim_device *dsim)
{
	int     ret = 0;
	int     i;
	unsigned char id[S6E3FA3_ID_LEN];

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	// id
	ret = dsim_read_hl_data(dsim, S6E3FA3_ID_REG, S6E3FA3_ID_LEN, id);
	if (ret != S6E3FA3_ID_LEN) {
		dsim_err("%s : can't read panel id\n", __func__);
		goto dump_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3FA3_ID_LEN; i++)
		dsim_info("%02x, ", id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}
dump_exit:
	dsim_info(" - %s\n", __func__);
	return ret;

}
static int s6e3fa3_a8xe_probe(struct dsim_device *dsim)
{
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3FA3_MTP_LEN] = { 0, };
	unsigned char hbm[S6E3FA3_HBMGAMMA_LEN] = { 0, };
	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
#if defined(CONFIG_LCD_ALPM)
	panel->alpm = 0;
	panel->current_alpm = 0;
	mutex_init(&panel->alpm_lock);
	panel->alpm_support = 1;
#elif defined(CONFIG_LCD_DOZE_MODE)
	panel->alpm_support = SUPPORT_30HZALPM; // 0 : unsupport, 1 : 30hz, 2 : 1hz
	panel->hlpm_support = SUPPORT_30HZALPM;	// 0 : unsupport, 1 : 30hz
	panel->alpm_mode = 0;
	panel->curr_alpm_mode = 0;
#endif

	dsim_info("%s : +\n", __func__);

	ret = s6e3fa3_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	dsim->priv.esd_disable = 0;
	//fix
#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#ifdef CONFIG_LCD_HMT
	ret = hmt_init_dimming(dsim, mtp);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

probe_exit:
	dsim_info("%s : -\n", __func__);

	return ret;

}


static int s6e3fa3_a8xe_displayon(struct dsim_device *dsim)
{
	int     ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	msleep(10);

	ret = dsim_write_hl_data(dsim, SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_NORMAL_MODE_ON\n", __func__);
		goto displayon_err;
	}


displayon_err:
	return ret;

}

static int s6e3fa3_a8xe_exit(struct dsim_device *dsim)
{
	int     ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(10);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(150);

exit_err:
	return ret;
}

static int s6e3fa3_a8xe_init(struct dsim_device *dsim)
{
	int     ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}
	msleep(10);

	/* 7. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(20);

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
	ret = lcd_reload_mtp(dsim);
#endif

	ret = dsim_write_hl_data(dsim, SEQ_PARTIAL_MODE_ON, ARRAY_SIZE(SEQ_PARTIAL_MODE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PARTIAL_MODE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PARTIAL_AREA_SETTING, ARRAY_SIZE(SEQ_PARTIAL_AREA_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PARTIAL_AREA_SETTING\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_PCD_SET_DET_HIGH, ARRAY_SIZE(SEQ_PCD_SET_DET_HIGH));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PCD_SET_DET_HIGH\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_AVC_SETTING_1, ARRAY_SIZE(SEQ_AVC_SETTING_1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AVC_SETTING_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_AVC_SETTING_2, ARRAY_SIZE(SEQ_AVC_SETTING_2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AVC_SETTING_2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSHL_SETTING_1, ARRAY_SIZE(SEQ_TSHL_SETTING_1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSHL_SETTING_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSHL_SETTING_2, ARRAY_SIZE(SEQ_TSHL_SETTING_2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSHL_SETTING_2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TE_TIMING, ARRAY_SIZE(SEQ_TE_TIMING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_TIMING\n", __func__);
		goto init_exit;
	}

	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_AID_SETTING, ARRAY_SIZE(SEQ_AID_SETTING));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AID_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_VINT_SET, ARRAY_SIZE(SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}
	msleep(80);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}

#ifdef CONFIG_LCD_DOZE_MODE
int s6e3fa3_a8xe_setalpm(struct dsim_device *dsim, int mode)
{
	int ret = 0;

	msleep(20);

	switch(mode) {
		case HLPM_ON_LOW:
			ret = dsim_write_hl_data(dsim, SEQ_SELECT_HLPM, ARRAY_SIZE(SEQ_SELECT_HLPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SELECT_HLPM\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_ALPM_ON_2, ARRAY_SIZE(SEQ_ALPM_ON_2));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_ALPM_ON_2\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_1, ARRAY_SIZE(SEQ_LTPS_EQ_1));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_1\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_2, ARRAY_SIZE(SEQ_LTPS_EQ_2));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_2\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_SRC_LOAD_EN_HLPM, ARRAY_SIZE(SEQ_SRC_LOAD_EN_HLPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SRC_LOAD_EN_HLPM\n", __func__);
			}
			pr_info("%s : HLPM_ON_2NIT !\n", __func__);
			break;
		case HLPM_ON_HIGH:
			ret = dsim_write_hl_data(dsim, SEQ_SELECT_HLPM, ARRAY_SIZE(SEQ_SELECT_HLPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SELECT_HLPM\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_ALPM_ON_60, ARRAY_SIZE(SEQ_ALPM_ON_60));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_ALPM_ON_60\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_1, ARRAY_SIZE(SEQ_LTPS_EQ_1));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_1\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_2, ARRAY_SIZE(SEQ_LTPS_EQ_2));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_2\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_SRC_LOAD_EN_HLPM, ARRAY_SIZE(SEQ_SRC_LOAD_EN_HLPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SRC_LOAD_EN_HLPM\n", __func__);
			}
			pr_info("%s : HLPM_ON_60NIT !\n", __func__);
			break;
		case ALPM_ON_LOW:
			ret = dsim_write_hl_data(dsim, SEQ_SELECT_ALPM, ARRAY_SIZE(SEQ_SELECT_ALPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SELECT_ALPM\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_ALPM_ON_2, ARRAY_SIZE(SEQ_ALPM_ON_2));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_ALPM_ON_2\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_1, ARRAY_SIZE(SEQ_LTPS_EQ_1));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_1\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_2, ARRAY_SIZE(SEQ_LTPS_EQ_2));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_2\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_SRC_LOAD_EN_ALPM, ARRAY_SIZE(SEQ_SRC_LOAD_EN_ALPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SRC_LOAD_EN_ALPM\n", __func__);
			}
			pr_info("%s : ALPM_ON_2NIT !\n", __func__);
			break;
		case ALPM_ON_HIGH:
			ret = dsim_write_hl_data(dsim, SEQ_SELECT_ALPM, ARRAY_SIZE(SEQ_SELECT_ALPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SELECT_ALPM\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_ALPM_ON_60, ARRAY_SIZE(SEQ_ALPM_ON_60));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_ALPM_ON_60\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_1, ARRAY_SIZE(SEQ_LTPS_EQ_1));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_1\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_LTPS_EQ_2, ARRAY_SIZE(SEQ_LTPS_EQ_2));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_LTPS_EQ_2\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
			}
			ret = dsim_write_hl_data(dsim, SEQ_SRC_LOAD_EN_ALPM, ARRAY_SIZE(SEQ_SRC_LOAD_EN_ALPM));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : SEQ_SRC_LOAD_EN_ALPM\n", __func__);
			}
			pr_info("%s : ALPM_ON_60NIT !\n", __func__);
			break;
		default:
			pr_info("%s: input is out of range : %d \n", __func__, mode);
			break;
	}
	return ret;
}

static int s6e3fa3_a8xe_enteralpm(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s was called\n", __func__);

	if (panel->state == PANEL_STATE_SUSPENED) {
		dsim_err("ERR:%s:panel is not active\n", __func__);
		return ret;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DISPLAY_OFF\n", __func__);
	}
	msleep(20);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	ret = s6e3fa3_a8xe_setalpm(dsim, panel->alpm_mode);
	if (ret < 0) {
		dsim_err("%s : failed to set alpm\n", __func__);
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}
	return ret;
}

static int s6e3fa3_a8xe_exitalpm(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s was called\n", __func__);

	if (panel->state == PANEL_STATE_SUSPENED) {
		dsim_err("ERR:%s:panel is not active\n", __func__);
		return ret;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
	}
	msleep(20);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_ALPM_OFF, ARRAY_SIZE(SEQ_ALPM_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ALPM_OFF\n", __func__);
	}
	pr_info("%s : ALPM_OFF !\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_SRC_LOAD_EN_ALPM, ARRAY_SIZE(SEQ_SRC_LOAD_EN_ALPM));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SRC_LOAD_EN_ALPM\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}
	return ret;
}
#endif

struct dsim_panel_ops s6e3fa3_panel_ops = {
	.probe = s6e3fa3_a8xe_probe,
	.displayon = s6e3fa3_a8xe_displayon,
	.exit = s6e3fa3_a8xe_exit,
	.init = s6e3fa3_a8xe_init,
	.dump = s6e3fa3_a8xe_dump,
#ifdef CONFIG_LCD_DOZE_MODE
	.enteralpm = s6e3fa3_a8xe_enteralpm,
	.exitalpm = s6e3fa3_a8xe_exitalpm,
#endif
};

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
static int lcd_reload_mtp(struct dsim_device *dsim)
{
	int i, ret;
	unsigned char mtp[S6E3FA3_MTP_LEN] = { 0, };
	unsigned char hbm[S6E3FA3_HBMGAMMA_LEN] = { 0, };

	// retry 3 times
	for(i=0; i < 3; i++) {
		ret = s6e3fa3_read_init_info(dsim, mtp, hbm);
		dsim_info( "%s : load MTP of s6e3fa3\n", __func__ );

		if( ret == 0 )
			break;
	}
	if( ret != 0 ) return -EIO;

	update_mdnie_coordinate( dsim->priv.coordinate[0], dsim->priv.coordinate[1] );

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma table\n", __func__);
	}
#endif

#ifdef CONFIG_LCD_HMT
	ret = hmt_init_dimming(dsim, mtp);
	if (ret) {
		dsim_err("%s : failed to generate gamma table\n", __func__);
	}
#endif

	return 0;	//success
}
#endif	// CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3fa3_panel_ops;

}


//? why get_lcd_type locate this side?
static int __init get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);
	return 0;
}
early_param("lcdtype", get_lcd_type);

static int __init get_hw_rev(char *arg)
{
	return 0;
}

early_param("androidboot.hw_rev", get_hw_rev);




