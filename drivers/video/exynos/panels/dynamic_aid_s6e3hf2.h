#ifndef __DYNAMIC_AID_XXXX_H
#define __DYNAMIC_AID_XXXX_H __FILE__

#include "dynamic_aid.h"
#include "dynamic_aid_gamma_curve.h"

enum {
	IV_VT,
	IV_3,
	IV_11,
	IV_23,
	IV_35,
	IV_51,
	IV_87,
	IV_151,
	IV_203,
	IV_255,
	IV_MAX,
};

enum {
	IBRIGHTNESS_2NT,
	IBRIGHTNESS_3NT,
	IBRIGHTNESS_4NT,
	IBRIGHTNESS_5NT,
	IBRIGHTNESS_6NT,
	IBRIGHTNESS_7NT,
	IBRIGHTNESS_8NT,
	IBRIGHTNESS_9NT,
	IBRIGHTNESS_10NT,
	IBRIGHTNESS_11NT,
	IBRIGHTNESS_12NT,
	IBRIGHTNESS_13NT,
	IBRIGHTNESS_14NT,
	IBRIGHTNESS_15NT,
	IBRIGHTNESS_16NT,
	IBRIGHTNESS_17NT,
	IBRIGHTNESS_19NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_21NT,
	IBRIGHTNESS_22NT,
	IBRIGHTNESS_24NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_27NT,
	IBRIGHTNESS_29NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_32NT,
	IBRIGHTNESS_34NT,
	IBRIGHTNESS_37NT,
	IBRIGHTNESS_39NT,
	IBRIGHTNESS_41NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_110NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_500NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6400	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] = {
	0,	/* IV_VT */
	3,	/* IV_3 */
	11,	/* IV_11 */
	23,	/* IV_23 */
	35,	/* IV_35 */
	51,	/* IV_51 */
	87,	/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255	/* IV_255 */
};

static const int index_brightness_table[IBRIGHTNESS_MAX] = {
	2, /*IBRIGHTNESS_2NT*/
	3, /*IBRIGHTNESS_3NT*/
	4, /*IBRIGHTNESS_4NT*/
	5, /*IBRIGHTNESS_5NT*/
	6, /*IBRIGHTNESS_6NT*/
	7, /*IBRIGHTNESS_7NT*/
	8, /*IBRIGHTNESS_8NT*/
	9, /*IBRIGHTNESS_9NT*/
	10, /*IBRIGHTNESS_10NT*/
	11, /*IBRIGHTNESS_11NT*/
	12, /*IBRIGHTNESS_12NT*/
	13, /*IBRIGHTNESS_13NT*/
	14, /*IBRIGHTNESS_14NT*/
	15, /*IBRIGHTNESS_15NT*/
	16, /*IBRIGHTNESS_16NT*/
	17, /*IBRIGHTNESS_17NT*/
	19, /*IBRIGHTNESS_19NT*/
	20, /*IBRIGHTNESS_20NT*/
	21, /*IBRIGHTNESS_21NT*/
	22, /*IBRIGHTNESS_22NT*/
	24, /*IBRIGHTNESS_24NT*/
	25, /*IBRIGHTNESS_25NT*/
	27, /*IBRIGHTNESS_27NT*/
	29, /*IBRIGHTNESS_29NT*/
	30, /*IBRIGHTNESS_30NT*/
	32, /*IBRIGHTNESS_32NT*/
	34, /*IBRIGHTNESS_34NT*/
	37, /*IBRIGHTNESS_37NT*/
	39, /*IBRIGHTNESS_39NT*/
	41, /*IBRIGHTNESS_41NT*/
	44, /*IBRIGHTNESS_44NT*/
	47, /*IBRIGHTNESS_47NT*/
	50, /*IBRIGHTNESS_50NT*/
	53, /*IBRIGHTNESS_53NT*/
	56, /*IBRIGHTNESS_56NT*/
	60, /*IBRIGHTNESS_60NT*/
	64, /*IBRIGHTNESS_64NT*/
	68, /*IBRIGHTNESS_68NT*/
	72, /*IBRIGHTNESS_72NT*/
	77, /*IBRIGHTNESS_77NT*/
	82, /*IBRIGHTNESS_82NT*/
	87, /*IBRIGHTNESS_87NT*/
	93, /*IBRIGHTNESS_93NT*/
	98, /*IBRIGHTNESS_98NT*/
	105, /*IBRIGHTNESS_105NT*/
	110, /*IBRIGHTNESS_110NT*/
	119, /*IBRIGHTNESS_119NT*/
	126, /*IBRIGHTNESS_126NT*/
	134, /*IBRIGHTNESS_134NT*/
	143, /*IBRIGHTNESS_143NT*/
	152, /*IBRIGHTNESS_152NT*/
	162, /*IBRIGHTNESS_162NT*/
	172, /*IBRIGHTNESS_172NT*/
	183, /*IBRIGHTNESS_183NT*/
	195, /*IBRIGHTNESS_195NT*/
	207, /*IBRIGHTNESS_207NT*/
	220, /*IBRIGHTNESS_220NT*/
	234, /*IBRIGHTNESS_234NT*/
	249, /*IBRIGHTNESS_249NT*/
	265, /*IBRIGHTNESS_265NT*/
	282, /*IBRIGHTNESS_282NT*/
	300, /*IBRIGHTNESS_300NT*/
	316, /*IBRIGHTNESS_316NT*/
	333, /*IBRIGHTNESS_333NT*/
	360, /*IBRIGHTNESS_360NT*/
	360 /* IBRIGHTNESS_500NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] = {
	0x00, 0x00, 0x00,	/* IV_VT */
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100,	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] = {
	{0, 860},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{72, 860},	/* IV_255 */
};

static const int vt_voltage_value[] = {
	0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186
};

static const int brightness_base_table[IBRIGHTNESS_MAX] = {
	114, /* IBRIGHTNESS_2NT */
	114, /* IBRIGHTNESS_3NT */
	114, /* IBRIGHTNESS_4NT */
	114, /* IBRIGHTNESS_5NT */
	114, /* IBRIGHTNESS_6NT */
	114, /* IBRIGHTNESS_7NT */
	114, /* IBRIGHTNESS_8NT */
	114, /* IBRIGHTNESS_9NT */
	114, /* IBRIGHTNESS_10NT */
	114, /* IBRIGHTNESS_11NT */
	114, /* IBRIGHTNESS_12NT */
	114, /* IBRIGHTNESS_13NT */
	114, /* IBRIGHTNESS_14NT */
	114, /* IBRIGHTNESS_15NT */
	114, /* IBRIGHTNESS_16NT */
	114, /* IBRIGHTNESS_17NT */
	114, /* IBRIGHTNESS_19NT */
	114, /* IBRIGHTNESS_20NT */
	114, /* IBRIGHTNESS_21NT */
	114, /* IBRIGHTNESS_22NT */
	114, /* IBRIGHTNESS_24NT */
	114, /* IBRIGHTNESS_25NT */
	114, /* IBRIGHTNESS_27NT */
	114, /* IBRIGHTNESS_29NT */
	114, /* IBRIGHTNESS_30NT */
	114, /* IBRIGHTNESS_32NT */
	114, /* IBRIGHTNESS_34NT */
	114, /* IBRIGHTNESS_37NT */
	114, /* IBRIGHTNESS_39NT */
	114, /* IBRIGHTNESS_41NT */
	114, /* IBRIGHTNESS_44NT */
	114, /* IBRIGHTNESS_47NT */
	114, /* IBRIGHTNESS_50NT */
	114, /* IBRIGHTNESS_53NT */
	114, /* IBRIGHTNESS_56NT */
	114, /* IBRIGHTNESS_60NT */
	114, /* IBRIGHTNESS_64NT */
	114, /* IBRIGHTNESS_68NT */
	114, /* IBRIGHTNESS_72NT */
	114, /* IBRIGHTNESS_77NT */
	120, /* IBRIGHTNESS_82NT */
	129, /* IBRIGHTNESS_87NT */
	137, /* IBRIGHTNESS_93NT */
	143, /* IBRIGHTNESS_98NT */
	153, /* IBRIGHTNESS_105NT */
	162, /* IBRIGHTNESS_110NT */
	171, /* IBRIGHTNESS_119NT */
	179, /* IBRIGHTNESS_126NT */
	190, /* IBRIGHTNESS_134NT */
	202, /* IBRIGHTNESS_143NT */
	213, /* IBRIGHTNESS_152NT */
	228, /* IBRIGHTNESS_162NT */
	238, /* IBRIGHTNESS_172NT */
	252, /* IBRIGHTNESS_183NT */
	252, /* IBRIGHTNESS_195NT */
	252, /* IBRIGHTNESS_207NT */
	252, /* IBRIGHTNESS_220NT */
	252, /* IBRIGHTNESS_234NT */
	252, /* IBRIGHTNESS_249NT */
	268, /* IBRIGHTNESS_265NT */
	283, /* IBRIGHTNESS_282NT */
	300, /* IBRIGHTNESS_300NT */
	315, /* IBRIGHTNESS_316NT */
	334, /* IBRIGHTNESS_333NT */
	360, /* IBRIGHTNESS_360NT */
	360	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table, /* IBRIGHTNESS_2NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_3NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_4NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_5NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_6NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_7NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_8NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_9NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_10NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_11NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_12NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_13NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_14NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_15NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_16NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_17NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_19NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_24NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_30NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_32NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_34NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_37NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_39NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_41NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_44NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_47NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_50NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_53NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_56NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_60NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_64NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_68NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_72NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_77NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_82NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_87NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_93NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_98NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_105NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_110NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_172NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_183NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_195NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_207NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_220NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_234NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_249NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_265NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_282NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_300NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table, /* IBRIGHTNESS_360NT */
	gamma_curve_2p20_table	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0xEB, 0x90},	/*IBRIGHTNESS_2NT */
	{0xD9, 0x90},	/*IBRIGHTNESS_3NT */
	{0xC7, 0x90},	/*IBRIGHTNESS_4NT */
	{0xB3, 0x90},	/*IBRIGHTNESS_5NT */
	{0xA1, 0x90},	/*IBRIGHTNESS_6NT */
	{0x8F, 0x90},	/*IBRIGHTNESS_7NT */
	{0x7B, 0x90},	/*IBRIGHTNESS_8NT */
	{0x68, 0x90},	/*IBRIGHTNESS_9NT */
	{0x55, 0x90},	/*IBRIGHTNESS_10NT */
	{0x42, 0x90},	/*IBRIGHTNESS_11NT */
	{0x2F, 0x90},	/*IBRIGHTNESS_12NT */
	{0x1A, 0x90},	/*IBRIGHTNESS_13NT */
	{0x09, 0x90},	/*IBRIGHTNESS_14NT */
	{0xF5, 0x80},	/*IBRIGHTNESS_15NT */
	{0xE2, 0x80},	/*IBRIGHTNESS_16NT */
	{0xCF, 0x80},	/*IBRIGHTNESS_17NT */
	{0xA7, 0x80},	/*IBRIGHTNESS_19NT */
	{0x95, 0x80},	/*IBRIGHTNESS_20NT */
	{0x80, 0x80},	/*IBRIGHTNESS_21NT */
	{0x6C, 0x80},	/*IBRIGHTNESS_22NT */
	{0x45, 0x80},	/*IBRIGHTNESS_24NT */
	{0x31, 0x80},	/*IBRIGHTNESS_25NT */
	{0x08, 0x80},	/*IBRIGHTNESS_27NT */
	{0xE1, 0x70},	/*IBRIGHTNESS_29NT */
	{0xCC, 0x70},	/*IBRIGHTNESS_30NT */
	{0xA4, 0x70},	/*IBRIGHTNESS_32NT */
	{0x7B, 0x70},	/*IBRIGHTNESS_34NT */
	{0x3C, 0x70},	/*IBRIGHTNESS_37NT */
	{0x12, 0x70},	/*IBRIGHTNESS_39NT */
	{0xE8, 0x60},	/*IBRIGHTNESS_41NT */
	{0xAA, 0x60},	/*IBRIGHTNESS_44NT */
	{0x6B, 0x60},	/*IBRIGHTNESS_47NT */
	{0x2A, 0x60},	/*IBRIGHTNESS_50NT */
	{0xEA, 0x50},	/*IBRIGHTNESS_53NT */
	{0xAA, 0x50},	/*IBRIGHTNESS_56NT */
	{0x50, 0x50},	/*IBRIGHTNESS_60NT */
	{0xF9, 0x40},	/*IBRIGHTNESS_64NT */
	{0x98, 0x40},	/*IBRIGHTNESS_68NT */
	{0x46, 0x40},	/*IBRIGHTNESS_72NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_77NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_82NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_87NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_93NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_98NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_105NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_110NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_119NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_126NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_134NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_143NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_152NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_162NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_172NT */
	{0xE2, 0x30},	/*IBRIGHTNESS_183NT */
	{0x62, 0x30},	/*IBRIGHTNESS_195NT */
	{0xDE, 0x20},	/*IBRIGHTNESS_207NT */
	{0x4D, 0x20},	/*IBRIGHTNESS_220NT */
	{0xAE, 0x10},	/*IBRIGHTNESS_234NT */
	{0x03, 0x10},	/*IBRIGHTNESS_249NT */
	{0x03, 0x10},	/*IBRIGHTNESS_265NT */
	{0x03, 0x10},	/*IBRIGHTNESS_282NT */
	{0x03, 0x10},	/*IBRIGHTNESS_300NT */
	{0x03, 0x10},	/*IBRIGHTNESS_316NT */
	{0x03, 0x10},	/*IBRIGHTNESS_333NT */
	{0x03, 0x10},	/*IBRIGHTNESS_360NT */
	{0x03, 0x10}	/* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 29, 30, 34, 29, 25, 19, 13, 8, 0},
	{0, 29, 30, 27, 22, 18, 15, 10, 7, 0},
	{0, 29, 26, 22, 19, 15, 13, 9, 6, 0},
	{0, 25, 23, 19, 15, 13, 11, 7, 6, 0},
	{0, 20, 21, 17, 13, 11, 10, 7, 5, 0},
	{0, 21, 19, 16, 12, 10, 9, 6, 5, 0},
	{0, 21, 17, 14, 11, 9, 8, 5, 5, 0},
	{0, 20, 16, 13, 10, 8, 8, 5, 4, 0},
	{0, 14, 15, 12, 9, 7, 7, 5, 4, 0},
	{0, 13, 14, 11, 8, 6, 6, 4, 4, 0},
	{0, 16, 14, 10, 8, 6, 6, 4, 4, 0},
	{0, 16, 13, 9, 7, 5, 6, 4, 4, 0},
	{0, 14, 13, 9, 7, 5, 5, 4, 4, 0},
	{0, 16, 12, 9, 6, 5, 5, 4, 4, 0},
	{0, 12, 12, 8, 6, 5, 5, 4, 4, 0},
	{0, 15, 11, 8, 5, 4, 5, 4, 4, 0},
	{0, 9, 10, 7, 5, 4, 5, 4, 4, 0},
	{0, 9, 10, 7, 5, 4, 4, 4, 4, 0},
	{0, 11, 10, 7, 5, 4, 5, 3, 4, 0},
	{0, 11, 10, 7, 4, 4, 5, 3, 4, 0},
	{0, 8, 9, 7, 4, 4, 4, 3, 4, 0},
	{0, 11, 9, 6, 4, 3, 4, 3, 4, 0},
	{0, 12, 8, 6, 3, 3, 4, 3, 4, 0},
	{0, 9, 8, 5, 3, 3, 4, 3, 4, 0},
	{0, 8, 8, 5, 3, 3, 4, 3, 4, 0},
	{0, 10, 7, 5, 3, 3, 4, 3, 4, 0},
	{0, 8, 7, 5, 3, 3, 4, 3, 3, 0},
	{0, 10, 6, 5, 2, 2, 4, 3, 3, 0},
	{0, 8, 6, 5, 2, 2, 3, 3, 3, 0},
	{0, 6, 6, 4, 2, 2, 3, 3, 3, 0},
	{0, 9, 5, 4, 2, 2, 3, 3, 3, 0},
	{0, 6, 5, 4, 2, 2, 3, 2, 3, 0},
	{0, 3, 4, 4, 1, 2, 3, 2, 3, 0},
	{0, 7, 4, 3, 1, 2, 3, 2, 3, 0},
	{0, 5, 4, 3, 1, 2, 3, 2, 3, 0},
	{0, 2, 3, 3, 1, 2, 3, 2, 3, 0},
	{0, 6, 3, 3, 1, 1, 2, 2 , 3 , 0},
	{0, 4, 3, 3, 1, 1, 2, 2 , 3 , 0},
	{0, 1, 3, 2, 1, 1, 2, 2 , 3 , 0},
	{0, 5, 2, 2, 0, 1, 2, 2 , 3 , 0},
	{0, 6, 2, 2, 1, 1, 2, 1 , 2 , 0},
	{0, 1, 3, 2, 1, 1, 2, 2 , 2 , 0},
	{0, 2, 2, 2, 0, 1, 3, 3 , 1 , 0},
	{0, 2, 2, 2, 1, 0, 3, 3 , 2 , 0},
	{0, 3, 2, 1, 1, 1, 2, 3 , 1 , 0},
	{0, 3, 2, 1, 1, 0, 2, 3 , 2 , 0},
	{0, 5, 2, 2, 1, 1, 2, 3 , 2 , 0},
	{0, 6, 1, 1, 1, 1, 3, 3 , 1 , 0},
	{0, 5, 1, 2, 1, 1, 3, 4 , 1 , 0},
	{0, 1, 2, 1, 1, 1, 2, 3 , 1 , 0},
	{0, 1, 2, 2, 1, 1, 3, 4, 2, 0},
	{0, 0, 1, 1, 1, 1, 3, 4, 1, 0},
	{0, 0, 1, 1, 1, 1, 3, 3, 3, 0},
	{0, 0, 1, 0, 1, 1, 2, 3, 2, 0},
	{0, 0, 1, 0, 1, 1, 2, 3, 2, 0},
	{0, 2, 0, 0, 0, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 2, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
	{0, 2, 0, 0, 0, 0, 1, 2, 0, 0},
	{0, 2, -1, 0, 0, -1, 0, 1, 1, 0},
	{0, 0, 0, 0, -1, -1, 0, 0, 1, 0},
	{0, 0, 0, -1, 0, -1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, -1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, -3, 7, -16, -9, 4, -9, -12, 4, -9, -23, 5, -11, -14, 1, -4, -4, 0, -2, -4, 0, -2, -7, 0, -2},
	{0, 0, 0, 0, 0, 0, -3, 7, -16, -9, 4, -9, -15, 4, -10, -20, 4, -9, -9, 1, -3, -5, 0, -2, -3, 0, -1, -4, 0, -1},
	{0, 0, 0, 0, 0, 0, -4, 7, -16, -13, 4, -12, -14, 4, -9, -17, 2, -5, -9, 2, -4, -3, 0, -1, -4, 0, -2, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 7, -16, -14, 5, -12, -17, 5, -10, -13, 1, -3, -8, 2, -4, -2, 0, -1, -2, 0, -1, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 7, -16, -16, 4, -12, -14, 4, -8, -13, 1, -2, -7, 1, -4, -2, 0, -1, -1, 0, -1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -7, 9, -19, -16, 3, -12, -14, 3, -8, -12, 0, -2, -5, 1, -3, -1, 0, 0, -1, 0, -1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 10, -20, -18, 3, -14, -13, 3, -6, -10, 0, -2, -5, 1, -3, -2, 0, -1, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -8, 9, -20, -17, 2, -13, -13, 2, -6, -10, 0, -2, -5, 1, -3, -1, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 11, -24, -19, 2, -14, -13, 2, -6, -9, 0, -2, -5, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -8, 12, -25, -18, 2, -14, -13, 2, -6, -8, 0, -1, -3, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -9, 9, -19, -18, 2, -14, -11, 2, -5, -8, 0, -1, -3, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -8, 11, -22, -19, 2, -14, -10, 2, -5, -6, 0, 0, -2, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -11, 9, -20, -18, 2, -13, -8, 2, -4, -6, 0, -1, -4, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -11, 12, -26, -14, 2, -10, -9, 2, -5, -6, 0, -1, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -11, 9, -20, -16, 2, -13, -8, 2, -4, -4, 0, 0, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -12, 13, -27, -15, 1, -12, -8, 1, -4, -4, 0, 0, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -13, 13, -28, -14, 1, -13, -7, 1, -4, -4, 0, 0, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -14, 12, -26, -14, 2, -12, -7, 2, -4, -3, 0, 0, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -14, 12, -25, -12, 1, -10, -6, 1, -3, -3, 0, 0, -3, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -15, 11, -24, -11, 1, -10, -8, 1, -4, -3, 0, 1, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -17, 14, -30, -10, 1, -9, -7, 1, -3, -3, 0, 0, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -15, 12, -25, -12, 1, -12, -5, 1, -3, -3, 0, 1, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -18, 14, -29, -9, 1, -10, -5, 1, -3, -1, 0, 2, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -14, 12, -24, -11, 1, -12, -4, 1, -3, -2, 0, 1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -15, 11, -23, -10, 1, -12, -3, 1, -2, -2, 0, 1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -16, 13, -28, -8, 1, -10, -3, 1, -2, -1, 0, 1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -15, 12, -26, -8, 0, -10, -3, 0, -2, -1, 0, 1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -17, 14, -30, -5, 1, -9, -4, 1, -2, 0, 0, 1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -16, 14, -28, -5, 0, -9, -4, 0, -2, -1, 0, 0, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -14, 12, -24, -5, 0, -11, -4, 0, -2, -1, 0, 0, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -17, 14, -30, -4, 0, -10, -3, 0, -2, -1, 0, 0, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -15, 12, -26, -3, 0, -10, -3, 0, -2, 0, 0, 1, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -15, 14, -30, -3, 1, -9, -2, 1, -2, 0, 0, 1, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -14, 12, -26, -2, 1, -11, -2, 1, -2, 0, 0, 1, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -12, 12, -24, -2, 0, -11, -3, 0, -2, 0, 0, 1, 0, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -11, 12, -26, -1, 0, -10, -2, 0, -2, 0, 0, 1, 0, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -8, 11, -23, 0, 0, -10, -1, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 9, -20, -1, 0, -10, 0, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 8, -16, 0, 0, -12, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 8, -16, 1, 1, -10, 0, 1, -2, 1, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 8, -18, 0, 0, -9, 0, 0, -2, 1, 0, 2, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 7, -16, 1, 0, -10, 0, 0, 0, 0, 0, 2, 0, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 8, -17, 0, 0, -8, 0, 0, -2, 1, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 8, -17, 0, 0, -8, 0, 0, -1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 8, -17, 1, 0, -7, 0, 0, 0, 1, 0, 2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 7, -15, 0, 0, -7, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 7, -16, 0, 0, -7, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 7, -16, 0, 0, -6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 7, -15, -1, 0, -5, 0, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 7, -15, 1, 0, -5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 7, -16, 0, 0, -4, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 7, -14, 0, 0, -3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 6, -14, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 6, -13, 0, 0, -4, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 3, 5, -12, 0, 0, -4, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 8, 4, -9, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 10, 4, -8, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 10, 3, -8, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#ifdef CONFIG_LCD_HMT
enum {
	HMT_OFF,
	HMT_SCAN_SINGLE,
	HMT_SCAN_MAX
};

enum {
	HMT_IBRIGHTNESS_20NT,
	HMT_IBRIGHTNESS_21NT,
	HMT_IBRIGHTNESS_22NT,
	HMT_IBRIGHTNESS_23NT,
	HMT_IBRIGHTNESS_25NT,
	HMT_IBRIGHTNESS_27NT,
	HMT_IBRIGHTNESS_29NT,
	HMT_IBRIGHTNESS_31NT,
	HMT_IBRIGHTNESS_33NT,
	HMT_IBRIGHTNESS_35NT,
	HMT_IBRIGHTNESS_37NT,
	HMT_IBRIGHTNESS_39NT,
	HMT_IBRIGHTNESS_41NT,
	HMT_IBRIGHTNESS_44NT,
	HMT_IBRIGHTNESS_47NT,
	HMT_IBRIGHTNESS_50NT,
	HMT_IBRIGHTNESS_53NT,
	HMT_IBRIGHTNESS_56NT,
	HMT_IBRIGHTNESS_60NT,
	HMT_IBRIGHTNESS_64NT,
	HMT_IBRIGHTNESS_68NT,
	HMT_IBRIGHTNESS_72NT,
	HMT_IBRIGHTNESS_77NT,
	HMT_IBRIGHTNESS_82NT,
	HMT_IBRIGHTNESS_87NT,
	HMT_IBRIGHTNESS_93NT,
	HMT_IBRIGHTNESS_99NT,
	HMT_IBRIGHTNESS_105NT,
	HMT_IBRIGHTNESS_150NT,
	HMT_IBRIGHTNESS_360NT,
	HMT_IBRIGHTNESS_MAX
};

static const int hmt_index_brightness_table[HMT_IBRIGHTNESS_MAX] = {
	20,	/* IBRIGHTNESS_20NT */
	21,	/* IBRIGHTNESS_21NT */
	22,	/* IBRIGHTNESS_22NT */
	23,	/* IBRIGHTNESS_23NT */
	25,	/* IBRIGHTNESS_25NT */
	27,	/* IBRIGHTNESS_27NT */
	29,	/* IBRIGHTNESS_29NT */
	31,	/* IBRIGHTNESS_31NT */
	33,	/* IBRIGHTNESS_33NT */
	35,	/* IBRIGHTNESS_35NT */
	37,	/* IBRIGHTNESS_37NT */
	39,	/* IBRIGHTNESS_39NT */
	41,	/* IBRIGHTNESS_41NT */
	44,	/* IBRIGHTNESS_44NT */
	47,	/* IBRIGHTNESS_47NT */
	50,	/* IBRIGHTNESS_50NT */
	53,	/* IBRIGHTNESS_53NT */
	56,	/* IBRIGHTNESS_56NT */
	60,	/* IBRIGHTNESS_60NT */
	64,	/* IBRIGHTNESS_64NT */
	68,	/* IBRIGHTNESS_68NT */
	72,	/* IBRIGHTNESS_72NT */
	77,	/* IBRIGHTNESS_77NT */
	82,	/* IBRIGHTNESS_82NT */
	87,	/* IBRIGHTNESS_87NT */
	93,	/* IBRIGHTNESS_93NT */
	99,	/* IBRIGHTNESS_99NT */
	105,	/* IBRIGHTNESS_105NT */
	150,	/* IBRIGHTNESS_105NT */
	360	/* IBRIGHTNESS_360NT */
};

static const int hmt_brightness_base_table[HMT_IBRIGHTNESS_MAX] = {
	92,	/* IBRIGHTNESS_20NT */
	94,	/* IBRIGHTNESS_21NT */
	99,	/* IBRIGHTNESS_22NT */
	104,	/* IBRIGHTNESS_23NT */
	107,	/* IBRIGHTNESS_25NT */
	113,	/* IBRIGHTNESS_27NT */
	122,	/* IBRIGHTNESS_29NT */
	133,	/* IBRIGHTNESS_31NT */
	138,	/* IBRIGHTNESS_33NT */
	145,	/* IBRIGHTNESS_35NT */
	154,	/* IBRIGHTNESS_37NT */
	160,	/* IBRIGHTNESS_39NT */
	163,	/* IBRIGHTNESS_41NT */
	173,	/* IBRIGHTNESS_44NT */
	184,	/* IBRIGHTNESS_47NT */
	193,	/* IBRIGHTNESS_50NT */
	206,	/* IBRIGHTNESS_53NT */
	215,	/* IBRIGHTNESS_56NT */
	223,	/* IBRIGHTNESS_60NT */
	237,	/* IBRIGHTNESS_64NT */
	247,	/* IBRIGHTNESS_68NT */
	262,	/* IBRIGHTNESS_72NT */
	206,	/* IBRIGHTNESS_77NT */
	217,	/* IBRIGHTNESS_82NT */
	229,	/* IBRIGHTNESS_87NT */
	248,	/* IBRIGHTNESS_93NT */
	256,	/* IBRIGHTNESS_99NT */
	265,	/* IBRIGHTNESS_105NT */
	550,	/* IBRIGHTNESS_105NT */
	360	/* IBRIGHTNESS_360NT */
};

static const int *hmt_gamma_curve_tables[HMT_IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table,	/* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_23NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_31NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_33NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_35NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_37NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_39NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_41NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_44NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_47NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_50NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_53NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_56NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_60NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_64NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_68NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_72NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_77NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_82NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_87NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_93NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_99NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p25_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p20_table,	/* IBRIGHTNESS_360NT */
};

static const unsigned char hmt_aor_cmd[HMT_IBRIGHTNESS_MAX][3] = {
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_20NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_21NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_22NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_23NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_25NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_27NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_29NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_31NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_33NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_35NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_37NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_39NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_41NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_44NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_47NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_50NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_53NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_56NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_60NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_64NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_68NT */
	{0x03, 0x12, 0x03},	/* IBRIGHTNESS_72NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_77NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_82NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_87NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_93NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_99NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_105NT */
	{0x03, 0x13, 0x04},	/* IBRIGHTNESS_150NT */
	{0x03, 0x10, 0x00},	/* IBRIGHTNESS_360NT */
};

static const int hmt_offset_gradation[HMT_IBRIGHTNESS_MAX][IV_MAX] = {
	/* V0 ~ V255 */
	{0,	11,	9,	7,	6,	6,	5,	3,	4,	0},	/* IBRIGHTNESS_20NT */
	{0,	15,	9,	7,	7,	5,	6,	3,	6,	0},	/* IBRIGHTNESS_21NT */
	{0,	16,	8,	8,	7,	6,	5,	3,	4,	0},	/* IBRIGHTNESS_22NT */
	{0,	15,	8,	7,	6,	5,	6,	4,	4,	0},	/* IBRIGHTNESS_23NT */
	{0,	13,	8,	7,	7,	7,	8,	7,	6,	0},	/* IBRIGHTNESS_25NT */
	{0,	9,	9,	7,	7,	7,	8,	9,	6,	0},	/* IBRIGHTNESS_27NT */
	{0,	9,	9,	7,	7,	7,	7,	9,	7,	0},	/* IBRIGHTNESS_29NT */
	{0,	17,	9,	7,	7,	7,	7,	9,	5,	0},	/* IBRIGHTNESS_31NT */
	{0,	16,	8,	7,	7,	7,	8,	9,	4,	0},	/* IBRIGHTNESS_33NT */
	{0,	15,	8,	7,	7,	7,	8,	9,	5,	0},	/* IBRIGHTNESS_35NT */
	{0,	16,	8,	6,	7,	7,	8,	7,	3,	0},	/* IBRIGHTNESS_37NT */
	{0,	12,	9,	7,	8,	8,	9,	11,	5,	0},	/* IBRIGHTNESS_39NT */
	{0,	10,	9,	7,	7,	8,	10,	9,	5,	0},	/* IBRIGHTNESS_41NT */
	{0,	17,	9,	8,	8,	8,	10,	11,	7,	0},	/* IBRIGHTNESS_44NT */
	{0,	16,	8,	7,	8,	8,	9,	10,	6,	0},	/* IBRIGHTNESS_47NT */
	{0,	8,	9,	8,	7,	8,	10,	12,	6,	0},	/* IBRIGHTNESS_50NT */
	{0,	10,	9,	8,	8,	8,	10,	10,	5,	0},	/* IBRIGHTNESS_53NT */
	{0,	10,	9,	8,	8,	8,	9,	11,	7,	0},	/* IBRIGHTNESS_56NT */
	{0,	12,	9,	7,	9,	8,	10,	11,	4,	0},	/* IBRIGHTNESS_60NT */
	{0,	15,	8,	8,	9,	9,	10,	12,	6,	0},	/* IBRIGHTNESS_64NT */
	{0,	8,	9,	9,	9,	9,	11,	13,	9,	0},	/* IBRIGHTNESS_68NT */
	{0,	10,	9,	8,	9,	10,	12,	14,	6,	0},	/* IBRIGHTNESS_72NT */
	{0,	5,	6,	5,	6,	7,	9,	10,	8,	0},	/* IBRIGHTNESS_77NT */
	{0,	6,	6,	6,	6,	6,	8,	10,	5,	0},	/* IBRIGHTNESS_82NT */
	{0,	7,	5,	5,	6,	7,	8,	10,	7,	0},	/* IBRIGHTNESS_87NT */
	{0,	9,	5,	6,	6,	6,	8,	9,	5,	0},	/* IBRIGHTNESS_93NT */
	{0,	6,	6,	6,	8,	8,	11,	12,	9,	0},	/* IBRIGHTNESS_99NT */
	{0,	8,	6,	6,	7,	8,	10,	11,	5,	0},	/* IBRIGHTNESS_105NT */
	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0},	/* IBRIGHTNESS_150NT */
	{0,	0,	0,	0,	0,	0,	0,	0,	0,	0},	/* IBRIGHTNESS_360NT */
};

static const int hmt_offset_color[HMT_IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	{0, 0, 0, 0, 0, 0, -40, 6, -14, -11, 1, -7, -3, 1, -2, -4, 0, 2, 0, 0, 0, 6, -1, 5, 0, 0, 1, 13, -4, 17},
	{0, 0, 0, 0, 0, 0, -42, 6, -14, -11, 0, -6, -4, 0, -2, -5, 0, 0, 0, 0, 0, 6, -1, 5, 1, 0, 2, 12, -4, 17},
	{0, 0, 0, 0, 0, 0, -44, 7, -16, -9, 1, -5, -3, 1, -2, -4, 0, 1, 0, 0, 1, 4, -1, 4, -1, 0, 0, 15, -4, 20},
	{0, 0, 0, 0, 0, 0, -42, 7, -14, -10, 0, -6, -3, 0, -2, -4, 0, 1, 2, 0, 2, 3, 0, 4, -1, 0, 0, 15, -4, 20},
	{0, 0, 0, 0, 0, 0, -44, 7, -15, -11, 0, -6, -3, 0, -2, -3, 0, 1, 4, -1, 3, 1, 0, 3, 0, 0, 1, 14, -4, 18},
	{0, 0, 0, 0, 0, 0, -29, 5, -11, -11, 0, -6, -2, 0, -1, -4, 0, 0, 5, -1, 4, -1, 0, 0, 1, 0, 2, 13, -4, 17},
	{0, 0, 0, 0, 0, 0, -30, 6, -13, -7, 0, -4, -2, 0, -1, -4, 0, 0, 5, -1, 4, 0, 0, 2, 0, 0, 0, 15, -4, 19},
	{0, 0, 0, 0, 0, 0, -34, 6, -14, -10, 0, -5, -4, 0, -1, -1, 0, 2, 2, -2, 4, 0, 0, 0, 0, 0, 0, 15, -4, 19},
	{0, 0, 0, 0, 0, 0, -32, 6, -14, -8, 0, -4, -2, 0, 0, -4, 0, 0, 5, -1, 4, -1, 0, 0, -1, 0, 1, 17, -4, 19},
	{0, 0, 0, 0, 0, 0, -29, 6, -13, -7, 0, -4, -3, 0, 0, -4, 0, 0, 5, -1, 4, 0, 0, 2, 0, 0, 0, 16, -4, 19},
	{0, 0, 0, 0, 0, 0, -32, 6, -14, -8, 0, -4, -3, 0, 0, -1, -1, 2, 3, -1, 3, -1, 0, 1, 0, 0, 0, 17, -4, 20},
	{0, 0, 0, 0, 0, 0, -26, 6, -12, -8, 0, -4, -2, 0, 0, -2, 0, 1, 1, -1, 2, -1, 0, 0, 0, 0, 0, 16, -5, 20},
	{0, 0, 0, 0, 0, 0, -24, 5, -12, -8, 0, -4, -2, 0, 0, 0, 0, 1, 2, -1, 3, -1, 0, 0, 2, 0, 2, 17, -4, 20},
	{0, 0, 0, 0, 0, 0, -27, 5, -12, -7, 0, -4, -2, 0, 0, -2, 0, 1, 2, -1, 3, -1, 0, 1, 0, 0, 0, 17, -5, 21},
	{0, 0, 0, 0, 0, 0, -24, 5, -11, -6, 0, -4, -3, 0, 0, -1, 0, 1, 1, -1, 2, 0, 0, 2, 0, 0, 0, 17, -5, 20},
	{0, 0, 0, 0, 0, 0, -19, 5, -10, -5, 0, -2, -4, 0, 0, -1, 0, 1, 3, -1, 4, 0, 0, 1, 0, 0, 0, 17, -5, 21},
	{0, 0, 0, 0, 0, 0, -20, 4, -10, -4, 0, -1, -4, 0, 0, -1, 0, 0, 2, -1, 3, -1, 0, 0, 0, 0, 0, 20, -5, 23},
	{0, 0, 0, 0, 0, 0, -20, 4, -10, -5, 0, -2, -3, 0, 0, 0, 0, 1, 2, -1, 3, -1, 0, 0, 0, 0, 0, 19, -5, 23},
	{0, 0, 0, 0, 0, 0, -20, 4, -10, -6, 0, -2, -4, 0, 0, 0, 0, 1, 2, -1, 3, -1, 0, 0, -1, 0, 0, 22, -5, 24},
	{0, 0, 0, 0, 0, 0, -20, 4, -10, -5, 0, -2, -4, 0, 0, 0, 0, 1, 0, -1, 2, -1, 0, 0, 2, 0, 2, 19, -5, 22},
	{0, 0, 0, 0, 0, 0, -15, 4, -8, -5, 0, -2, -3, 0, 0, 0, 0, 1, 2, -1, 2, 0, 0, 1, 0, 0, 0, 19, -5, 23},
	{0, 0, 0, 0, 0, 0, -16, 3, -8, -5, 0, -2, -2, 0, 0, 0, 0, 1, 1, -1, 2, -1, 0, -1, 0, 0, 1, 21, -6, 24},
	{0, 0, 0, 0, 0, 0, -11, 4, -8, -2, 0, 0, 0, 0, 2, 1, -1, 2, 1, -1, 2, 2, 0, 3, 0, 0, 0, 15, -5, 20},
	{0, 0, 0, 0, 0, 0, -11, 3, -8, -1, 0, -1, -1, 0, 2, 0, -1, 2, 3, -1, 3, 0, 0, 1, 0, 0, 1, 17, -4, 20},
	{0, 0, 0, 0, 0, 0, -10, 4, -8, -1, 0, 0, 0, 0, 2, 0, -1, 2, 2, -1, 3, 0, 0, 1, 0, 0, 0, 16, -5, 21},
	{0, 0, 0, 0, 0, 0, -11, 3, -8, -2, 0, 0, 0, 0, 2, 3, -1, 4, 0, 0, 1, 0, 0, 1, -1, 0, 0, 19, -5, 22},
	{0, 0, 0, 0, 0, 0, -9, 3, -6, -2, 0, -1, 0, 0, 2, 3, -2, 4, 0, 0, 1, 0, 0, 0, 0, 0, 1, 16, -5, 21},
	{0, 0, 0, 0, 0, 0, -8, 2, -6, -2, 0, -1, 0, 0, 1, 1, -2, 4, -1, 0, 1, 0, 0, -1, 0, 0, 1, 20, -5, 23},
	{0, 0, 0, 0, 0, 0, -34, -24, -13, -11, -10, -7, -8, -10, -6, -4, -10, -1, 1, 0, -2, 5, 4, 5, -4, -4, -5, -13, -32, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,}
};
#endif


#endif /* __DYNAMIC_AID_XXXX_H */
