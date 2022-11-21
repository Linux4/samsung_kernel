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
	IBRIGHTNESS_111NT,
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
	IBRIGHTNESS_550NT,
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
	2,	/* IBRIGHTNESS_2NT */
	3,	/* IBRIGHTNESS_3NT */
	4,	/* IBRIGHTNESS_4NT */
	5,	/* IBRIGHTNESS_5NT */
	6,	/* IBRIGHTNESS_6NT */
	7,	/* IBRIGHTNESS_7NT */
	8,	/* IBRIGHTNESS_8NT */
	9,	/* IBRIGHTNESS_9NT */
	10,	/* IBRIGHTNESS_10NT */
	11,	/* IBRIGHTNESS_11NT */
	12,	/* IBRIGHTNESS_12NT */
	13,	/* IBRIGHTNESS_13NT */
	14,	/* IBRIGHTNESS_14NT */
	15,	/* IBRIGHTNESS_15NT */
	16,	/* IBRIGHTNESS_16NT */
	17,	/* IBRIGHTNESS_17NT */
	19,	/* IBRIGHTNESS_19NT */
	20,	/* IBRIGHTNESS_20NT */
	21,	/* IBRIGHTNESS_21NT */
	22,	/* IBRIGHTNESS_22NT */
	24,	/* IBRIGHTNESS_24NT */
	25,	/* IBRIGHTNESS_25NT */
	27,	/* IBRIGHTNESS_27NT */
	29,	/* IBRIGHTNESS_29NT */
	30,	/* IBRIGHTNESS_30NT */
	32,	/* IBRIGHTNESS_32NT */
	34,	/* IBRIGHTNESS_34NT */
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
	98,	/* IBRIGHTNESS_98NT */
	105,	/* IBRIGHTNESS_105NT */
	111,	/* IBRIGHTNESS_111NT */
	119,	/* IBRIGHTNESS_119NT */
	126,	/* IBRIGHTNESS_126NT */
	134,	/* IBRIGHTNESS_134NT */
	143,	/* IBRIGHTNESS_143NT */
	152,	/* IBRIGHTNESS_152NT */
	162,	/* IBRIGHTNESS_162NT */
	172,	/* IBRIGHTNESS_172NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	360,	/* IBRIGHTNESS_360NT */
	360		/* IBRIGHTNESS_550NT */
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
	113, /* IBRIGHTNESS_2NT */
	113, /* IBRIGHTNESS_3NT */
	113, /* IBRIGHTNESS_4NT */
	113, /* IBRIGHTNESS_5NT */
	113, /* IBRIGHTNESS_6NT */
	113, /* IBRIGHTNESS_7NT */
	113, /* IBRIGHTNESS_8NT */
	113, /* IBRIGHTNESS_9NT */
	113, /* IBRIGHTNESS_10NT */
	113, /* IBRIGHTNESS_11NT */
	113, /* IBRIGHTNESS_12NT */
	113, /* IBRIGHTNESS_13NT */
	113, /* IBRIGHTNESS_14NT */
	113, /* IBRIGHTNESS_15NT */
	113, /* IBRIGHTNESS_16NT */
	113, /* IBRIGHTNESS_17NT */
	113, /* IBRIGHTNESS_19NT */
	113, /* IBRIGHTNESS_20NT */
	113, /* IBRIGHTNESS_21NT */
	113, /* IBRIGHTNESS_22NT */
	113, /* IBRIGHTNESS_24NT */
	113, /* IBRIGHTNESS_25NT */
	113, /* IBRIGHTNESS_27NT */
	113, /* IBRIGHTNESS_29NT */
	113, /* IBRIGHTNESS_30NT */
	113, /* IBRIGHTNESS_32NT */
	113, /* IBRIGHTNESS_34NT */
	113, /* IBRIGHTNESS_37NT */
	113, /* IBRIGHTNESS_39NT */
	113, /* IBRIGHTNESS_41NT */
	113, /* IBRIGHTNESS_44NT */
	113, /* IBRIGHTNESS_47NT */
	113, /* IBRIGHTNESS_50NT */
	113, /* IBRIGHTNESS_53NT */
	113, /* IBRIGHTNESS_56NT */
	113, /* IBRIGHTNESS_60NT */
	113, /* IBRIGHTNESS_64NT */
	113, /* IBRIGHTNESS_68NT */
	113, /* IBRIGHTNESS_72NT */
	113, /* IBRIGHTNESS_77NT */
	122, /* IBRIGHTNESS_82NT */
	128, /* IBRIGHTNESS_87NT */
	136, /* IBRIGHTNESS_93NT */
	145, /* IBRIGHTNESS_98NT */
	155, /* IBRIGHTNESS_105NT */
	164, /* IBRIGHTNESS_111NT */
	175, /* IBRIGHTNESS_119NT */
	187, /* IBRIGHTNESS_126NT */
	196, /* IBRIGHTNESS_134NT */
	208, /* IBRIGHTNESS_143NT */
	223, /* IBRIGHTNESS_152NT */
	234, /* IBRIGHTNESS_162NT */
	253, /* IBRIGHTNESS_172NT */
	253, /* IBRIGHTNESS_183NT */
	253, /* IBRIGHTNESS_195NT */
	253, /* IBRIGHTNESS_207NT */
	253, /* IBRIGHTNESS_220NT */
	253, /* IBRIGHTNESS_234NT */
	253, /* IBRIGHTNESS_249NT */
	268, /* IBRIGHTNESS_265NT */
	283, /* IBRIGHTNESS_282NT */
	300, /* IBRIGHTNESS_300NT */
	319, /* IBRIGHTNESS_316NT */
	337, /* IBRIGHTNESS_333NT */
	360, /* IBRIGHTNESS_360NT */
	360	/* IBRIGHTNESS_550NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table,	/* IBRIGHTNESS_2NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_3NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_4NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_5NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_6NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_7NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_8NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_9NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_10NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_11NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_12NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_13NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_14NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_15NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_16NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_17NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_19NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_24NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_30NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_32NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_34NT */
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
	gamma_curve_2p15_table,	/* IBRIGHTNESS_98NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_111NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_172NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_183NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_195NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_207NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_220NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_234NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_249NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_265NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_282NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_300NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table,	/* IBRIGHTNESS_360NT */
	gamma_curve_2p20_table	/* IBRIGHTNESS_550NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0xE5, 0x90},  /* IBRIGHTNESS_2NT */
	{0xCC, 0x90},  /* IBRIGHTNESS_3NT */
	{0xB6, 0x90},  /* IBRIGHTNESS_4NT */
	{0xA3, 0x90},  /* IBRIGHTNESS_5NT */
	{0x8E, 0x90},  /* IBRIGHTNESS_6NT */
	{0x79, 0x90},  /* IBRIGHTNESS_7NT */
	{0x66, 0x90},  /* IBRIGHTNESS_8NT */
	{0x57, 0x90},  /* IBRIGHTNESS_9NT */
	{0x44, 0x90},  /* IBRIGHTNESS_10NT */
	{0x35, 0x90},  /* IBRIGHTNESS_11NT */
	{0x25, 0x90},  /* IBRIGHTNESS_12NT */
	{0x14, 0x90},  /* IBRIGHTNESS_13NT */
	{0x05, 0x90},  /* IBRIGHTNESS_14NT */
	{0xEB, 0x80},  /* IBRIGHTNESS_15NT */
	{0xE0, 0x80},  /* IBRIGHTNESS_16NT */
	{0xC7, 0x80},  /* IBRIGHTNESS_17NT */
	{0xA4, 0x80},  /* IBRIGHTNESS_19NT */
	{0x92, 0x80},  /* IBRIGHTNESS_20NT */
	{0x7B, 0x80},  /* IBRIGHTNESS_21NT */
	{0x67, 0x80},  /* IBRIGHTNESS_22NT */
	{0x43, 0x80},  /* IBRIGHTNESS_24NT */
	{0x2F, 0x80},  /* IBRIGHTNESS_25NT */
	{0xFD, 0x70},  /* IBRIGHTNESS_27NT */
	{0xDF, 0x70},  /* IBRIGHTNESS_29NT */
	{0xC7, 0x70},  /* IBRIGHTNESS_30NT */
	{0xA4, 0x70},  /* IBRIGHTNESS_32NT */
	{0x6F, 0x70},  /* IBRIGHTNESS_34NT */
	{0x3D, 0x70},  /* IBRIGHTNESS_37NT */
	{0x14, 0x70},  /* IBRIGHTNESS_39NT */
	{0xEE, 0x60},  /* IBRIGHTNESS_41NT */
	{0xB2, 0x60},  /* IBRIGHTNESS_44NT */
	{0x73, 0x60},  /* IBRIGHTNESS_47NT */
	{0x34, 0x60},  /* IBRIGHTNESS_50NT */
	{0xF4, 0x50},  /* IBRIGHTNESS_53NT */
	{0xB5, 0x50},  /* IBRIGHTNESS_56NT */
	{0x5F, 0x50},  /* IBRIGHTNESS_60NT */
	{0x0E, 0x50},  /* IBRIGHTNESS_64NT */
	{0xB4, 0x40},  /* IBRIGHTNESS_68NT */
	{0x62, 0x40},  /* IBRIGHTNESS_72NT */
	{0xED, 0x30},  /* IBRIGHTNESS_77NT */
	{0xED, 0x30},  /* IBRIGHTNESS_82NT */
	{0xED, 0x30},  /* IBRIGHTNESS_87NT */
	{0xED, 0x30},  /* IBRIGHTNESS_93NT */
	{0xED, 0x30},  /* IBRIGHTNESS_98NT */
	{0xED, 0x30},  /* IBRIGHTNESS_105NT */
	{0xED, 0x30},  /* IBRIGHTNESS_111NT */
	{0xED, 0x30},  /* IBRIGHTNESS_119NT */
	{0xED, 0x30},  /* IBRIGHTNESS_126NT */
	{0xED, 0x30},  /* IBRIGHTNESS_134NT */
	{0xED, 0x30},  /* IBRIGHTNESS_143NT */
	{0xED, 0x30},  /* IBRIGHTNESS_152NT */
	{0xED, 0x30},  /* IBRIGHTNESS_162NT */
	{0xED, 0x30},  /* IBRIGHTNESS_172NT */
	{0xA7, 0x30},  /* IBRIGHTNESS_183NT */
	{0x26, 0x30},  /* IBRIGHTNESS_195NT */
	{0xB5, 0x20},  /* IBRIGHTNESS_207NT */
	{0x26, 0x20},  /* IBRIGHTNESS_220NT */
	{0x8F, 0x10},  /* IBRIGHTNESS_234NT */
	{0x03, 0x10},  /* IBRIGHTNESS_249NT */
	{0x03, 0x10},  /* IBRIGHTNESS_265NT */
	{0x03, 0x10},  /* IBRIGHTNESS_282NT */
	{0x03, 0x10},  /* IBRIGHTNESS_300NT */
	{0x03, 0x10},  /* IBRIGHTNESS_316NT */
	{0x03, 0x10},  /* IBRIGHTNESS_333NT */
	{0x03, 0x10},  /* IBRIGHTNESS_360NT */
	{0x03, 0x10}  /* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 4, 27, 35, 33, 27, 22, 17, 9, 0},  /* IBRIGHTNESS_2NT */
	{0, 4, 29, 29, 25, 17, 17, 12, 8, 0},  /* IBRIGHTNESS_3NT */
	{0, 23, 26, 24, 21, 19, 16, 12, 7, 0},	/* IBRIGHTNESS_4NT */
	{0, 21, 22, 20, 17, 15, 13, 9, 5, 0},  /* IBRIGHTNESS_5NT */
	{0, 21, 21, 19, 16, 14, 12, 10, 6, 0},	/* IBRIGHTNESS_6NT */
	{0, 21, 20, 18, 14, 13, 11, 9, 5, 0},  /* IBRIGHTNESS_7NT */
	{0, 19, 19, 17, 13, 12, 10, 8, 4, 0},  /* IBRIGHTNESS_8NT */
	{0, 18, 19, 16, 13, 12, 9, 7, 4, 0},  /* IBRIGHTNESS_9NT */
	{0, 20, 18, 15, 12, 10, 9, 7, 4, 0},  /* IBRIGHTNESS_10NT */
	{0, 16, 17, 15, 11, 10, 8, 7, 4, 0},  /* IBRIGHTNESS_11NT */
	{0, 17, 16, 14, 11, 10, 8, 6, 4, 0},  /* IBRIGHTNESS_12NT */
	{0, 17, 16, 14, 11, 10, 7, 6, 4, 0},  /* IBRIGHTNESS_13NT */
	{0, 17, 15, 13, 10, 9, 6, 5, 4, 0},  /* IBRIGHTNESS_14NT */
	{0, 15, 15, 12, 10, 9, 7, 6, 4, 0},  /* IBRIGHTNESS_15NT */
	{0, 16, 14, 12, 9, 8, 6, 5, 4, 0},	/* IBRIGHTNESS_16NT */
	{0, 13, 14, 11, 9, 8, 6, 5, 4, 0},	/* IBRIGHTNESS_17NT */
	{0, 14, 13, 10, 8, 7, 6, 5, 4, 0},	/* IBRIGHTNESS_19NT */
	{0, 11, 13, 10, 8, 7, 5, 5, 4, 0},	/* IBRIGHTNESS_20NT */
	{0, 10, 13, 10, 7, 7, 5, 5, 4, 0},	/* IBRIGHTNESS_21NT */
	{0, 11, 12, 9, 7, 6, 5, 5, 4, 0},  /* IBRIGHTNESS_22NT */
	{0, 12, 11, 8, 6, 6, 5, 4, 4, 0},  /* IBRIGHTNESS_24NT */
	{0, 11, 11, 8, 6, 6, 5, 5, 4, 0},  /* IBRIGHTNESS_25NT */
	{0, 12, 10, 8, 6, 5, 4, 4, 4, 0},  /* IBRIGHTNESS_27NT */
	{0, 9, 10, 8, 6, 5, 4, 4, 4, 0},  /* IBRIGHTNESS_29NT */
	{0, 8, 10, 8, 5, 5, 4, 5, 3, 0},  /* IBRIGHTNESS_30NT */
	{0, 10, 9, 7, 5, 5, 4, 4, 4, 0},  /* IBRIGHTNESS_32NT */
	{0, 7, 9, 7, 5, 4, 4, 4, 4, 0},  /* IBRIGHTNESS_34NT */
	{0, 8, 8, 6, 4, 4, 3, 4, 4, 0},  /* IBRIGHTNESS_37NT */
	{0, 8, 8, 6, 5, 4, 4, 4, 3, 0},  /* IBRIGHTNESS_39NT */
	{0, 10, 7, 6, 4, 4, 3, 4, 4, 0},  /* IBRIGHTNESS_41NT */
	{0, 8, 7, 6, 4, 4, 3, 4, 3, 0},  /* IBRIGHTNESS_44NT */
	{0, 5, 7, 6, 4, 3, 3, 4, 3, 0},  /* IBRIGHTNESS_47NT */
	{0, 8, 6, 5, 3, 4, 3, 4, 3, 0},  /* IBRIGHTNESS_50NT */
	{0, 6, 6, 5, 3, 3, 3, 4, 3, 0},  /* IBRIGHTNESS_53NT */
	{0, 8, 5, 5, 3, 3, 3, 4, 3, 0},  /* IBRIGHTNESS_56NT */
	{0, 6, 5, 5, 3, 3, 3, 4, 3, 0},  /* IBRIGHTNESS_60NT */
	{0, 4, 5, 4, 2, 3, 2, 4, 3, 0},  /* IBRIGHTNESS_64NT */
	{0, 6, 4, 4, 2, 3, 2, 3, 3, 0},  /* IBRIGHTNESS_68NT */
	{0, 4, 4, 4, 2, 3, 2, 3, 3, 0},  /* IBRIGHTNESS_72NT */
	{0, 2, 4, 3, 2, 3, 2, 3, 3, 0},  /* IBRIGHTNESS_77NT */
	{0, 2, 4, 3, 3, 2, 2, 3, 2, 0},  /* IBRIGHTNESS_82NT */
	{0, 1, 4, 3, 2, 2, 2, 2, 1, 0},  /* IBRIGHTNESS_87NT */
	{0, 1, 4, 3, 2, 2, 3, 3, 2, 0},  /* IBRIGHTNESS_93NT */
	{0, 5, 3, 3, 2, 2, 3, 4, 2, 0},  /* IBRIGHTNESS_98NT */
	{0, 4, 3, 3, 2, 2, 2, 3, 1, 0},  /* IBRIGHTNESS_105NT */
	{0, 2, 3, 3, 1, 1, 2, 2, 0, 0},  /* IBRIGHTNESS_111NT */
	{0, 4, 3, 3, 2, 2, 3, 4, 0, 0},  /* IBRIGHTNESS_119NT */
	{0, 6, 3, 3, 2, 2, 3, 3, -2, 0},  /* IBRIGHTNESS_126NT */
	{0, 5, 3, 3, 1, 2, 2, 3, -1, 0},  /* IBRIGHTNESS_134NT */
	{0, 0, 3, 2, 1, 2, 3, 2, 0, 0},  /* IBRIGHTNESS_143NT */
	{0, 2, 3, 2, 2, 2, 3, 3, 0, 0},  /* IBRIGHTNESS_152NT */
	{0, 0, 2, 2, 2, 2, 3, 3, 0, 0},  /* IBRIGHTNESS_162NT */
	{0, 3, 2, 1, 2, 1, 2, 2, 0, 0},  /* IBRIGHTNESS_172NT */
	{0, 2, 1, 1, 1, 1, 2, 3, 1, 0},  /* IBRIGHTNESS_183NT */
	{0, 1, 1, 1, 1, 1, 2, 2, 0, 0},  /* IBRIGHTNESS_195NT */
	{0, 0, 1, 0, 1, 1, 1, 1, -1, 0},  /* IBRIGHTNESS_207NT */
	{0, 0, 1, 0, 1, 1, 1, 1, -1, 0},  /* IBRIGHTNESS_220NT */
	{0, 2, 1, 0, 1, 0, 1, 1, -1, 0},  /* IBRIGHTNESS_234NT */
	{0, 2, 0, 0, 0, 0, 1, 1, 0, 0},  /* IBRIGHTNESS_249NT */
	{0, 3, 0, 1, 0, 0, 1, 1, 1, 0},  /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 0},  /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 0},  /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, -1, 0, 1, 0, 1, 0},  /* IBRIGHTNESS_316NT */
	{0, 0, 0, -1, -1, 0, 0, 0, 0, 0},  /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* IBRIGHTNESS_360NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}  /* IBRIGHTNESS_500NT */
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, 21, -2, 1, 2, 2, -8, -7, -1, -5, -22, 0, -14, -14, -1, -6, -5, 0, -3, -1, 1, 0, -3, 4, 0},	/* IBRIGHTNESS_2NT */
	{0, 0, 0, 0, 0, 0, 21, -2, 1, -3, 1, -8, -4, -1, -3, -26, -1, -15, -10, 1, -4, -4, 1, -1, -2, -1, -1, -3, 3, -1},	/* IBRIGHTNESS_3NT */
	{0, 0, 0, 0, 0, 0, 10, -2, -6, -7, 2, -10, -10, 4, -4, -18, -1, -8, -9, 1, -5, -3, 1, -1, -2, -1, -1, 0, 4, 1},   /* IBRIGHTNESS_4NT */
	{0, 0, 0, 0, 0, 0, 10, 0, -6, -8, 2, -8, -12, 1, -6, -13, 1, -3, -7, 1, -4, -3, 1, -1, -1, 1, 0, 0, 3, 1},	 /* IBRIGHTNESS_5NT */
	{0, 0, 0, 0, 0, 0, 10, 2, -7, -7, 2, -8, -12, 2, -6, -11, 2, -2, -6, 1, -4, -3, 1, -1, -1, -1, -1, -2, 1, -2},	 /* IBRIGHTNESS_6NT */
	{0, 0, 0, 0, 0, 0, 4, 4, -9, -6, 2, -8, -11, 2, -6, -9, 2, -3, -6, 1, -4, -3, 1, -1, 0, 0, 0, -1, 1, -1},	/* IBRIGHTNESS_7NT */
	{0, 0, 0, 0, 0, 0, 4, 5, -11, -7, 1, -9, -12, 2, -7, -7, 2, -3, -5, 2, -3, -2, 1, 0, -2, 1, 0, 0, 0, -1},	/* IBRIGHTNESS_8NT */
	{0, 0, 0, 0, 0, 0, 0, 1, -14, -7, 4, -9, -10, 1, -7, -7, 3, -4, -5, 2, -2, -2, 1, 0, -1, 1, 0, 0, 0, -1},	/* IBRIGHTNESS_9NT */
	{0, 0, 0, 0, 0, 0, 0, 2, -14, -7, 3, -9, -9, 1, -7, -7, 3, -4, -5, 2, -2, -2, 1, 0, 0, 1, 1, 0, 0, -1},   /* IBRIGHTNESS_10NT */
	{0, 0, 0, 0, 0, 0, 1, 5, -14, -7, 2, -9, -9, 1, -7, -7, 2, -4, -3, 2, -2, -2, 1, 0, 0, 1, 1, 0, 0, -1},   /* IBRIGHTNESS_11NT */
	{0, 0, 0, 0, 0, 0, 2, 5, -13, -6, 3, -9, -7, 1, -6, -7, 2, -4, -3, 2, -2, -1, 1, 0, 0, 1, 1, 0, 0, -1},   /* IBRIGHTNESS_12NT */
	{0, 0, 0, 0, 0, 0, 1, 6, -14, -7, 2, -11, -7, 1, -6, -7, 2, -4, -2, 2, -2, -1, 1, 0, -1, 0, 0, 0, 0, -1},	/* IBRIGHTNESS_13NT */
	{0, 0, 0, 0, 0, 0, 5, 8, -13, -6, 5, -11, -5, 1, -4, -7, 1, -4, -2, 2, -2, -1, 1, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_14NT */
	{0, 0, 0, 0, 0, 0, 0, 3, -14, -6, 5, -11, -5, 1, -4, -6, 1, -4, -2, 2, -2, -1, 1, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_15NT */
	{0, 0, 0, 0, 0, 0, 6, 10, -13, -8, 2, -11, -5, 1, -4, -6, 1, -4, -2, 2, -2, -1, 1, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_16NT */
	{0, 0, 0, 0, 0, 0, 3, 6, -13, -7, 3, -11, -5, 1, -4, -6, 1, -3, -2, 2, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_17NT */
	{0, 0, 0, 0, 0, 0, 8, 9, -13, -7, 3, -11, -5, 1, -5, -6, 1, -3, -2, 2, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_19NT */
	{0, 0, 0, 0, 0, 0, 6, 5, -16, -7, 4, -10, -6, 0, -6, -6, 1, -3, -2, 2, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_20NT */
	{0, 0, 0, 0, 0, 0, 1, 3, -17, -7, 3, -8, -5, 1, -5, -5, 1, -3, -2, 2, 0, -1, 0, -2, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_21NT */
	{0, 0, 0, 0, 0, 0, 5, 7, -15, -5, 3, -8, -5, 2, -5, -5, 1, -2, -2, 2, 0, -1, 0, -2, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_22NT */
	{0, 0, 0, 0, 0, 0, 6, 8, -15, -3, 5, -10, -5, 2, -4, -3, 1, -2, -2, 2, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_24NT */
	{0, 0, 0, 0, 0, 0, 5, 7, -15, -5, 4, -10, -5, 2, -4, -3, 1, -2, -2, 2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_25NT */
	{0, 0, 0, 0, 0, 0, 11, 12, -15, -5, 2, -10, -5, 1, -4, -3, 1, -2, -2, 2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_27NT */
	{0, 0, 0, 0, 0, 0, 8, 10, -16, -5, 2, -9, -5, 1, -5, -3, 0, -2, -2, 2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_29NT */
	{0, 0, 0, 0, 0, 0, 7, 9, -16, -7, -1, -9, -2, 1, -4, -3, 2, -1, -2, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_30NT */
	{0, 0, 0, 0, 0, 0, 11, 12, -14, -2, 2, -9, -3, 0, -5, -3, 2, 0, -2, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_32NT */
	{0, 0, 0, 0, 0, 0, 7, 8, -16, -4, 1, -9, -3, 0, -5, -2, 2, 1, -1, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_34NT */
	{0, 0, 0, 0, 0, 0, 7, 9, -16, 1, 5, -7, -3, 0, -5, -2, 2, 1, -1, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_37NT */
	{0, 0, 0, 0, 0, 0, 8, 9, -15, 0, 2, -8, -3, 0, -5, -2, 2, 1, -1, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_39NT */
	{0, 0, 0, 0, 0, 0, 15, 16, -12, -1, 2, -9, -3, 0, -5, -2, 2, 1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_41NT */
	{0, 0, 0, 0, 0, 0, 13, 14, -12, 0, 2, -8, -5, -2, -7, -2, 2, 1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_44NT */
	{0, 0, 0, 0, 0, 0, 10, 12, -14, 1, 0, -9, -4, -2, -7, -2, 2, 1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_47NT */
	{0, 0, 0, 0, 0, 0, 12, 14, -12, 3, 3, -8, -2, 1, -5, -2, 0, 1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_50NT */
	{0, 0, 0, 0, 0, 0, 10, 12, -13, 3, 2, -8, -2, 1, -4, -2, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_53NT */
	{0, 0, 0, 0, 0, 0, 19, 18, -11, 3, 1, -8, -2, 1, -4, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_56NT */
	{0, 0, 0, 0, 0, 0, 14, 15, -13, 2, 0, -9, -1, 0, -4, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_60NT */
	{0, 0, 0, 0, 0, 0, 7, 8, -11, 7, 3, -8, 0, 2, -2, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_64NT */
	{0, 0, 0, 0, 0, 0, 13, 12, -11, 7, 3, -8, 0, 1, -2, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_68NT */
	{0, 0, 0, 0, 0, 0, 11, 11, -13, 6, 1, -9, 0, 1, -2, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_72NT */
	{0, 0, 0, 0, 0, 0, 5, 3, -12, 11, 4, -8, 1, 2, -1, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_77NT */
	{0, 0, 0, 0, 0, 0, 6, 6, -15, 6, -1, -10, 1, 2, -1, -2, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_82NT */
	{0, 0, 0, 0, 0, 0, 5, 5, -12, 6, 0, -10, 1, 2, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_87NT */
	{0, 0, 0, 0, 0, 0, 4, 3, -10, 3, -4, -12, 1, 2, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_93NT */
	{0, 0, 0, 0, 0, 0, 13, 10, -7, 4, -2, -10, 1, 1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_98NT */
	{0, 0, 0, 0, 0, 0, 17, 14, -3, -1, -3, -10, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_105NT */
	{0, 0, 0, 0, 0, 0, 16, 12, -4, 0, -3, -9, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_111NT */
	{0, 0, 0, 0, 0, 0, 15, 11, -4, 4, 0, -6, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_119NT */
	{0, 0, 0, 0, 0, 0, 11, 8, -4, -1, -4, -9, 0, 0, -1, -1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_126NT */
	{0, 0, 0, 0, 0, 0, 10, 6, -5, 0, -3, -7, 0, 0, -1, -2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_134NT */
	{0, 0, 0, 0, 0, 0, 10, 4, -5, 2, -1, -5, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_143NT */
	{0, 0, 0, 0, 0, 0, 15, 10, -1, -3, -4, -7, 0, 0, 0, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_152NT */
	{0, 0, 0, 0, 0, 0, 15, 9, -1, -1, -2, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_162NT */
	{0, 0, 0, 0, 0, 0, 15, 9, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* IBRIGHTNESS_172NT */
	{0, 0, 0, 0, 0, 0, 13, 9, 0, 0, -1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_183NT */
	{0, 0, 0, 0, 0, 0, 13, 8, -1, 0, -2, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_195NT */
	{0, 0, 0, 0, 0, 0, 11, 1, -2, 4, 3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_207NT */
	{0, 0, 0, 0, 0, 0, 8, -3, -5, 5, 3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_220NT */
	{0, 0, 0, 0, 0, 0, 7, -4, -5, 5, 3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 /* IBRIGHTNESS_234NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_249NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* IBRIGHTNESS_360NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}   /* IBRIGHTNESS_500NT */
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
	20, /* IBRIGHTNESS_20NT */
	21, /* IBRIGHTNESS_21NT */
	22, /* IBRIGHTNESS_22NT */
	23, /* IBRIGHTNESS_23NT */
	25, /* IBRIGHTNESS_25NT */
	27, /* IBRIGHTNESS_27NT */
	29, /* IBRIGHTNESS_29NT */
	31, /* IBRIGHTNESS_31NT */
	33, /* IBRIGHTNESS_33NT */
	35, /* IBRIGHTNESS_35NT */
	37, /* IBRIGHTNESS_37NT */
	39, /* IBRIGHTNESS_39NT */
	41, /* IBRIGHTNESS_41NT */
	44, /* IBRIGHTNESS_44NT */
	47, /* IBRIGHTNESS_47NT */
	50, /* IBRIGHTNESS_50NT */
	53, /* IBRIGHTNESS_53NT */
	56, /* IBRIGHTNESS_56NT */
	60, /* IBRIGHTNESS_60NT */
	64, /* IBRIGHTNESS_64NT */
	68, /* IBRIGHTNESS_68NT */
	72, /* IBRIGHTNESS_72NT */
	77, /* IBRIGHTNESS_77NT */
	82, /* IBRIGHTNESS_82NT */
	87, /* IBRIGHTNESS_87NT */
	93, /* IBRIGHTNESS_93NT */
	99, /* IBRIGHTNESS_99NT */
	105, /* IBRIGHTNESS_105NT */
	150, /* IBRIGHTNESS_150NT */
	360, /* IBRIGHTNESS_360NT */
};

static const int hmt_brightness_base_table[HMT_IBRIGHTNESS_MAX] = {
	98, /* IBRIGHTNESS_20NT */
	101, /* IBRIGHTNESS_21NT */
	105, /* IBRIGHTNESS_22NT */
	108, /* IBRIGHTNESS_23NT */
	117, /* IBRIGHTNESS_25NT */
	126, /* IBRIGHTNESS_27NT */
	134, /* IBRIGHTNESS_29NT */
	141, /* IBRIGHTNESS_31NT */
	151, /* IBRIGHTNESS_33NT */
	157, /* IBRIGHTNESS_35NT */
	166, /* IBRIGHTNESS_37NT */
	173, /* IBRIGHTNESS_39NT */
	181, /* IBRIGHTNESS_41NT */
	192, /* IBRIGHTNESS_44NT */
	206, /* IBRIGHTNESS_47NT */
	215, /* IBRIGHTNESS_50NT */
	228, /* IBRIGHTNESS_53NT */
	239, /* IBRIGHTNESS_56NT */
	252, /* IBRIGHTNESS_60NT */
	268, /* IBRIGHTNESS_64NT */
	284, /* IBRIGHTNESS_68NT */
	297, /* IBRIGHTNESS_72NT */
	224, /* IBRIGHTNESS_77NT */
	237, /* IBRIGHTNESS_82NT */
	252, /* IBRIGHTNESS_87NT */
	265, /* IBRIGHTNESS_93NT */
	281, /* IBRIGHTNESS_99NT */
	295, /* IBRIGHTNESS_105NT */
	550, /* IBRIGHTNESS_150NT */
	360, /* IBRIGHTNESS_150NT */
};

static const int *hmt_gamma_curve_tables[HMT_IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table, /* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_23NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_31NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_33NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_35NT */
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
	gamma_curve_2p15_table, /* IBRIGHTNESS_99NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_105NT */
	gamma_curve_2p20_table, /* IBRIGHTNESS_150NT */
	gamma_curve_2p20_table, /* IBRIGHTNESS_360NT */
};

static const unsigned char hmt_aor_cmd[HMT_IBRIGHTNESS_MAX][3] = {
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_20NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_21NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_22NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_23NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_25NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_27NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_29NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_31NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_33NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_35NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_37NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_39NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_41NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_44NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_47NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_50NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_53NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_56NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_60NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_64NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_68NT */
	{0x03, 0x12, 0x03},  /* IBRIGHTNESS_72NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_77NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_82NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_87NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_93NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_99NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_105NT */
	{0x03, 0x13, 0x04},  /* IBRIGHTNESS_150NT */
	{0x03, 0x10, 0x00},	/* IBRIGHTNESS_360NT */
};

static const int hmt_offset_gradation[HMT_IBRIGHTNESS_MAX][IV_MAX] = {
	/* V0 ~ V255 */
	{0, 9, 8, 8, 7, 6, 6, 3, 3, 0},  /* IBRIGHTNESS_20NT */
	{0, 9, 8, 7, 6, 5, 6, 4, 3, 0},  /* IBRIGHTNESS_21NT */
	{0, 13, 8, 7, 7, 6, 6, 5, 4, 0},  /* IBRIGHTNESS_22NT */
	{0, 11, 8, 7, 7, 6, 6, 6, 4, 0},  /* IBRIGHTNESS_23NT */
	{0, 14, 8, 7, 7, 6, 8, 6, 5, 0},  /* IBRIGHTNESS_25NT */
	{0, 10, 9, 7, 7, 6, 8, 6, 3, 0},  /* IBRIGHTNESS_27NT */
	{0, 12, 8, 8, 7, 6, 8, 6, 4, 0},  /* IBRIGHTNESS_29NT */
	{0, 19, 8, 7, 7, 6, 8, 7, 4, 0},  /* IBRIGHTNESS_31NT */
	{0, 8, 9, 8, 7, 6, 8, 7, 3, 0},  /* IBRIGHTNESS_33NT */
	{0, 19, 8, 7, 7, 7, 8, 7, 3, 0},  /* IBRIGHTNESS_35NT */
	{0, 9, 9, 7, 7, 7, 8, 8, 2, 0},  /* IBRIGHTNESS_37NT */
	{0, 10, 9, 8, 7, 7, 9, 7, 3, 0},  /* IBRIGHTNESS_39NT */
	{0, 12, 8, 7, 7, 7, 9, 8, 3, 0},  /* IBRIGHTNESS_41NT */
	{0, 12, 8, 7, 6, 7, 9, 7, 3, 0},  /* IBRIGHTNESS_44NT */
	{0, 7, 9, 8, 8, 8, 9, 8, 4, 0},  /* IBRIGHTNESS_47NT */
	{0, 9, 9, 8, 8, 8, 9, 10, 5, 0},  /* IBRIGHTNESS_50NT */
	{0, 12, 8, 8, 8, 8, 10, 9, 4, 0},  /* IBRIGHTNESS_53NT */
	{0, 13, 8, 8, 8, 9, 10, 10, 6, 0},	/* IBRIGHTNESS_56NT */
	{0, 13, 8, 7, 8, 8, 9, 8, 3, 0},  /* IBRIGHTNESS_60NT */
	{0, 8, 9, 8, 8, 10, 10, 9, 6, 0},  /* IBRIGHTNESS_64NT */
	{0, 11, 8, 8, 8, 10, 10, 9, 5, 0},	/* IBRIGHTNESS_68NT */
	{0, 11, 8, 8, 8, 11, 11, 9, 6, 0},	/* IBRIGHTNESS_72NT */
	{0, 8, 5, 5, 5, 6, 8, 8, 4, 0},  /* IBRIGHTNESS_77NT */
	{0, 8, 5, 5, 6, 6, 8, 8, 3, 0},  /* IBRIGHTNESS_82NT */
	{0, 9, 5, 5, 6, 7, 9, 8, 5, 0},  /* IBRIGHTNESS_87NT */
	{0, 6, 6, 6, 6, 8, 9, 9, 4, 0},  /* IBRIGHTNESS_93NT */
	{0, 7, 5, 6, 6, 8, 9, 8, 5, 0},  /* IBRIGHTNESS_99NT */
	{0, 7, 5, 6, 6, 8, 10, 7, 5, 0},  /* IBRIGHTNESS_105NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* IBRIGHTNESS_150NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* IBRIGHTNESS_360NT */ /*should be fixed for finding max brightness */
};

static const int hmt_offset_color[HMT_IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	{0, 0, 0, 0, 0, 0, -13, 5, -10, -7, 0, -3, -2, 0, 0, -1, -1, 3, 1, 0, 0, 0, 0, 1, 0, 0, 0, 13, -3, 16},   /* IBRIGHTNESS_20NT */
	{0, 0, 0, 0, 0, 0, -12, 4, -9, -6, 0, -3, -2, 0, 0, -1, 0, 2, 1, 0, 2, 0, 0, 0, 1, 0, 1, 13, -3, 16},	/* IBRIGHTNESS_21NT */
	{0, 0, 0, 0, 0, 0, -19, 5, -11, -9, 0, -4, -2, 0, 0, 0, -1, 2, 0, 0, 0, 1, 0, 2, 0, 0, 0, 12, -3, 16},	 /* IBRIGHTNESS_22NT */
	{0, 0, 0, 0, 0, 0, -15, 5, -10, -7, 0, -3, -2, 0, 0, 0, 0, 2, 1, 0, 2, 0, 0, 1, 1, 0, 1, 13, -3, 16},	/* IBRIGHTNESS_23NT */
	{0, 0, 0, 0, 0, 0, -18, 5, -12, -6, 0, -2, -3, 0, 0, 0, -1, 2, 1, 0, 1, 0, 0, 0, 0, 0, 1, 12, -3, 15},	 /* IBRIGHTNESS_25NT */
	{0, 0, 0, 0, 0, 0, -12, 4, -9, -8, 0, -2, -2, 0, 0, 0, -1, 2, 0, 0, 1, 0, 0, 0, 0, 0, 1, 13, -3, 16},	/* IBRIGHTNESS_27NT */
	{0, 0, 0, 0, 0, 0, -15, 4, -10, -7, 0, -1, 0, 0, 1, 0, 0, 1, 1, 0, 2, 1, 0, 1, 0, 0, 0, 13, -3, 16},   /* IBRIGHTNESS_29NT */
	{0, 0, 0, 0, 0, 0, -15, 4, -10, -7, 0, -1, -2, 0, 0, 0, -1, 2, 1, 0, 1, 0, 0, 0, 0, 0, 0, 14, -3, 16},	 /* IBRIGHTNESS_31NT */
	{0, 0, 0, 0, 0, 0, -12, 3, -8, -6, 0, -1, -2, 0, 0, 0, -1, 2, 1, 0, 1, 0, 0, 0, 0, 0, 1, 14, -3, 16},	/* IBRIGHTNESS_33NT */
	{0, 0, 0, 0, 0, 0, -14, 4, -10, -7, 0, -2, 0, 0, 2, 0, -1, 2, 2, 0, 2, 0, 0, 1, 0, 0, 0, 14, -3, 16},	/* IBRIGHTNESS_35NT */
	{0, 0, 0, 0, 0, 0, -11, 4, -8, -6, 0, -1, -2, 0, 1, 0, -1, 2, 2, 0, 2, 0, 0, 0, 0, 0, 1, 13, -3, 16},	/* IBRIGHTNESS_37NT */
	{0, 0, 0, 0, 0, 0, -12, 3, -8, -5, 0, 0, 0, 0, 2, 1, 0, 2, 0, 0, 1, 0, 0, 0, 1, 0, 1, 13, -3, 16},	 /* IBRIGHTNESS_39NT */
	{0, 0, 0, 0, 0, 0, -12, 3, -8, -4, 0, -1, -1, 0, 2, 1, -1, 2, 1, 0, 2, 1, 0, 0, 0, 0, 1, 14, -3, 16},	/* IBRIGHTNESS_41NT */
	{0, 0, 0, 0, 0, 0, -12, 3, -8, -4, -1, -1, -1, -1, 2, 1, 0, 2, 2, 0, 2, 0, 0, 0, 1, 0, 1, 14, -3, 16},	 /* IBRIGHTNESS_44NT */
	{0, 0, 0, 0, 0, 0, -10, 3, -6, -2, 0, 0, -1, 0, 2, 0, 0, 1, 0, 0, 2, 0, 0, 0, 1, 0, 1, 14, -3, 16},   /* IBRIGHTNESS_47NT */
	{0, 0, 0, 0, 0, 0, -10, 3, -6, -3, -1, 0, 0, -1, 2, 0, -1, 2, 1, 0, 1, 1, 0, 1, 0, 0, 0, 13, -4, 16},	/* IBRIGHTNESS_50NT */
	{0, 0, 0, 0, 0, 0, -12, 3, -6, -3, -1, 0, -1, -1, 2, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 13, -4, 16},	/* IBRIGHTNESS_53NT */
	{0, 0, 0, 0, 0, 0, -12, 2, -6, -2, 0, 0, -1, 0, 2, 1, 0, 2, 2, 0, 2, 1, 0, 1, 0, 0, 0, 12, -4, 16},   /* IBRIGHTNESS_56NT */
	{0, 0, 0, 0, 0, 0, -12, 3, -6, -4, 0, -1, 0, 0, 2, 1, -1, 2, 1, 0, 2, 1, 0, 0, 1, 0, 1, 13, -3, 16},   /* IBRIGHTNESS_60NT */
	{0, 0, 0, 0, 0, 0, -10, 2, -5, -2, -1, 0, 0, -1, 3, 1, 0, 2, 2, 0, 1, 0, 0, 1, 0, 0, 0, 13, -4, 16},   /* IBRIGHTNESS_64NT */
	{0, 0, 0, 0, 0, 0, -10, 2, -5, -1, -1, 0, 0, -1, 3, 1, 0, 1, 1, 0, 2, 1, 0, 1, 0, 0, 0, 13, -3, 16},   /* IBRIGHTNESS_68NT */
	{0, 0, 0, 0, 0, 0, -9, 2, -5, -2, 0, 0, 0, 0, 2, 2, 0, 2, 2, 0, 1, 0, 0, 1, 1, 0, 0, 13, -4, 16},	/* IBRIGHTNESS_72NT */
	{0, 0, 0, 0, 0, 0, -7, 2, -6, 0, -1, 0, 0, -1, 2, 1, -1, 2, 1, -1, 2, 0, 0, 1, 1, 0, 1, 14, -3, 16},   /* IBRIGHTNESS_77NT */
	{0, 0, 0, 0, 0, 0, -6, 2, -6, -1, -1, 0, 1, -1, 4, 1, 0, 1, 1, -1, 2, 1, 0, 1, 1, 0, 1, 13, -3, 16},   /* IBRIGHTNESS_82NT */
	{0, 0, 0, 0, 0, 0, -6, 2, -6, 0, -1, 1, 0, -1, 2, 2, 0, 2, 2, 0, 2, 0, 0, 1, 1, 0, 1, 13, -4, 16},	 /* IBRIGHTNESS_87NT */
	{0, 0, 0, 0, 0, 0, -5, 2, -5, 0, -1, 1, 1, -1, 3, 2, 0, 2, 1, 0, 2, 0, 0, 1, 0, 0, 0, 13, -4, 16},	 /* IBRIGHTNESS_93NT */
	{0, 0, 0, 0, 0, 0, -5, 2, -4, 0, -1, 1, 1, -1, 3, 2, 0, 2, 0, 0, 2, 1, 0, 1, 1, 0, 1, 13, -4, 16},	 /* IBRIGHTNESS_99NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -4, 0, -1, 1, 0, -1, 3, 2, 0, 2, 2, 0, 2, 0, 0, 1, 1, 0, 1, 13, -4, 16},	 /* IBRIGHTNESS_105NT */
	{0, 0, 0, 0, 0, 0, 13, 17, 10, 5, 3, 6, 0, 0, 3, 6, 5, 7, 10, 8, 9, 6, 5, 6, -2, -2, -3, -12, -22, -8},	 /* IBRIGHTNESS_150NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},	 /* IBRIGHTNESS_360NT */
};
#endif

#endif /* __DYNAMIC_AID_XXXX_H */
