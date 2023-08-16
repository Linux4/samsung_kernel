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
	IBRIGHTNESS_500NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6800	/* VREG_OUT x 1000 */

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
	360	/* IBRIGHTNESS_500NT */
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
	113,	/* IBRIGHTNESS_2NT */
	113,	/* IBRIGHTNESS_3NT */
	113,	/* IBRIGHTNESS_4NT */
	113,	/* IBRIGHTNESS_5NT */
	113,	/* IBRIGHTNESS_6NT */
	113,	/* IBRIGHTNESS_7NT */
	113,	/* IBRIGHTNESS_8NT */
	113,	/* IBRIGHTNESS_9NT */
	113,	/* IBRIGHTNESS_10NT */
	113,	/* IBRIGHTNESS_11NT */
	113,	/* IBRIGHTNESS_12NT */
	113,	/* IBRIGHTNESS_13NT */
	113,	/* IBRIGHTNESS_14NT */
	113,	/* IBRIGHTNESS_15NT */
	113,	/* IBRIGHTNESS_16NT */
	113,	/* IBRIGHTNESS_17NT */
	113,	/* IBRIGHTNESS_19NT */
	113,	/* IBRIGHTNESS_20NT */
	113,	/* IBRIGHTNESS_21NT */
	113,	/* IBRIGHTNESS_22NT */
	113,	/* IBRIGHTNESS_24NT */
	113,	/* IBRIGHTNESS_25NT */
	113,	/* IBRIGHTNESS_27NT */
	113,	/* IBRIGHTNESS_29NT */
	113,	/* IBRIGHTNESS_30NT */
	113,	/* IBRIGHTNESS_32NT */
	113,	/* IBRIGHTNESS_34NT */
	113,	/* IBRIGHTNESS_37NT */
	113,	/* IBRIGHTNESS_39NT */
	113,	/* IBRIGHTNESS_41NT */
	113,	/* IBRIGHTNESS_44NT */
	113,	/* IBRIGHTNESS_47NT */
	113,	/* IBRIGHTNESS_50NT */
	113,	/* IBRIGHTNESS_53NT */
	113,	/* IBRIGHTNESS_56NT */
	113,	/* IBRIGHTNESS_60NT */
	113,	/* IBRIGHTNESS_64NT */
	113,	/* IBRIGHTNESS_68NT */
	113,	/* IBRIGHTNESS_72NT */
	122,	/* IBRIGHTNESS_77NT */
	130,	/* IBRIGHTNESS_82NT */
	136,	/* IBRIGHTNESS_87NT */
	145,	/* IBRIGHTNESS_93NT */
	153,	/* IBRIGHTNESS_98NT */
	163,	/* IBRIGHTNESS_105NT */
	173,	/* IBRIGHTNESS_111NT */
	183,	/* IBRIGHTNESS_119NT */
	192,	/* IBRIGHTNESS_126NT */
	204,	/* IBRIGHTNESS_134NT */
	218,	/* IBRIGHTNESS_143NT */
	229,	/* IBRIGHTNESS_152NT */
	243,	/* IBRIGHTNESS_162NT */
	256,	/* IBRIGHTNESS_172NT */
	256,	/* IBRIGHTNESS_183NT */
	256,	/* IBRIGHTNESS_195NT */
	256,	/* IBRIGHTNESS_207NT */
	256,	/* IBRIGHTNESS_220NT */
	256,	/* IBRIGHTNESS_234NT */
	256,	/* IBRIGHTNESS_249NT */
	270,	/* IBRIGHTNESS_265NT */
	288,	/* IBRIGHTNESS_282NT */
	304,	/* IBRIGHTNESS_300NT */
	322,	/* IBRIGHTNESS_316NT */
	336,	/* IBRIGHTNESS_333NT */
	360,	/* IBRIGHTNESS_360NT */
	360	/* IBRIGHTNESS_500NT */
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
	gamma_curve_2p20_table	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0xF2, 0x70},	/* IBRIGHTNESS_2NT */
	{0xE3, 0x70},	/* IBRIGHTNESS_3NT */
	{0xD2, 0x70},	/* IBRIGHTNESS_4NT */
	{0xC2, 0x70},	/* IBRIGHTNESS_5NT */
	{0xB2, 0x70},	/* IBRIGHTNESS_6NT */
	{0xA3, 0x70},	/* IBRIGHTNESS_7NT */
	{0x92, 0x70},	/* IBRIGHTNESS_8NT */
	{0x82, 0x70},	/* IBRIGHTNESS_9NT */
	{0x71, 0x70},	/* IBRIGHTNESS_10NT */
	{0x60, 0x70},	/* IBRIGHTNESS_11NT */
	{0x50, 0x70},	/* IBRIGHTNESS_12NT */
	{0x40, 0x70},	/* IBRIGHTNESS_13NT */
	{0x30, 0x70},	/* IBRIGHTNESS_14NT */
	{0x1E, 0x70},	/* IBRIGHTNESS_15NT */
	{0x0F, 0x70},	/* IBRIGHTNESS_16NT */
	{0xFD, 0x60},	/* IBRIGHTNESS_17NT */
	{0xDD, 0x60},	/* IBRIGHTNESS_19NT */
	{0xCB, 0x60},	/* IBRIGHTNESS_20NT */
	{0xB8, 0x60},	/* IBRIGHTNESS_21NT */
	{0xA8, 0x60},	/* IBRIGHTNESS_22NT */
	{0x87, 0x60},	/* IBRIGHTNESS_24NT */
	{0x73, 0x60},	/* IBRIGHTNESS_25NT */
	{0x51, 0x60},	/* IBRIGHTNESS_27NT */
	{0x2E, 0x60},	/* IBRIGHTNESS_29NT */
	{0x1A, 0x60},	/* IBRIGHTNESS_30NT */
	{0xF9, 0x50},	/* IBRIGHTNESS_32NT */
	{0xD7, 0x50},	/* IBRIGHTNESS_34NT */
	{0x9E, 0x50},	/* IBRIGHTNESS_37NT */
	{0x7A, 0x50},	/* IBRIGHTNESS_39NT */
	{0x54, 0x50},	/* IBRIGHTNESS_41NT */
	{0x1E, 0x50},	/* IBRIGHTNESS_44NT */
	{0xE2, 0x40},	/* IBRIGHTNESS_47NT */
	{0xAA, 0x40},	/* IBRIGHTNESS_50NT */
	{0x72, 0x40},	/* IBRIGHTNESS_53NT */
	{0x35, 0x40},	/* IBRIGHTNESS_56NT */
	{0xE4, 0x30},	/* IBRIGHTNESS_60NT */
	{0x97, 0x30},	/* IBRIGHTNESS_64NT */
	{0x47, 0x30},	/* IBRIGHTNESS_68NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_72NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_77NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_82NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_87NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_93NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_98NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_105NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_111NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_119NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_126NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_134NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_143NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_152NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_162NT */
	{0xF2, 0x20},	/* IBRIGHTNESS_172NT */
	{0x92, 0x20},	/* IBRIGHTNESS_183NT */
	{0x21, 0x20},	/* IBRIGHTNESS_195NT */
	{0xAA, 0x10},	/* IBRIGHTNESS_207NT */
	{0x2C, 0x10},	/* IBRIGHTNESS_220NT */
	{0xA3, 0x00},	/* IBRIGHTNESS_234NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_249NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_265NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_282NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_300NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_316NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_333NT */
	{0x0A, 0x00},	/* IBRIGHTNESS_360NT */
	{0x0A, 0x00}  /* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 32, 33, 36, 31, 28, 21, 14, 7, 0},
	{0, 32, 33 , 30, 24, 21, 16, 11, 6, 0},
	{0, 31, 28, 24, 20, 17, 13, 9, 5, 0},
	{0, 26, 26, 22, 17, 15, 11, 8, 5, 0},
	{0, 23, 23, 19, 15, 13, 10, 7, 4, 0},
	{0, 21, 21, 17, 13, 12, 8, 6, 4, 0},
	{0, 21, 19, 16, 12, 11, 7, 6, 4, 0},
	{0, 16, 18, 14, 11, 10, 7, 6, 4, 0},
	{0, 20, 16, 13, 10, 9, 6, 5, 3, 0},
	{0, 16, 16, 13, 9, 8, 6, 5, 3, 0},
	{0, 18, 15, 12, 9, 8, 5, 5, 3, 0},
	{0, 13, 15, 12, 9, 8, 5, 5, 3, 0},
	{0, 13, 14, 10, 8, 7, 5, 5, 3, 0},
	{0, 13, 14, 10, 8, 7, 5, 4, 3, 0},
	{0, 13, 13, 9, 7, 6, 4, 4, 3, 0},
	{0, 14, 12, 9, 6, 6, 4, 4, 3, 0},
	{0, 10, 12, 8, 6, 6, 3, 4, 3, 0},
	{0, 9, 12, 8, 6, 6, 3, 4, 3, 0},
	{0, 12, 11, 8, 5, 5, 3, 4, 3, 0},
	{0, 10, 11, 8, 5, 5, 3, 4, 3, 0},
	{0, 10, 10, 7, 4, 4, 3, 4, 3, 0},
	{0, 11, 10, 7, 4, 4, 3, 4, 3, 0},
	{0, 7, 10, 6, 4, 4, 2, 4, 3, 0},
	{0, 9, 9, 6, 3, 4, 2, 4, 3, 0},
	{0, 9, 9, 6, 3, 4, 2, 4, 3, 0},
	{0, 7, 9, 6, 3, 4, 2, 3, 3, 0},
	{0, 10, 8, 6, 3, 4, 2, 3, 3, 0},
	{0, 7, 8, 6, 3, 3, 2, 3, 2, 0},
	{0, 6, 8, 5, 3, 3, 2, 3, 2, 0},
	{0, 5, 8, 5, 3, 3, 2, 3, 2, 0},
	{0, 6, 7, 5, 2, 3, 2, 3, 2, 0},
	{0, 4, 7, 5, 2, 3, 2, 3, 2, 0},
	{0, 4, 7, 4, 2, 3, 2, 3, 2, 0},
	{0, 3, 7, 5, 2, 3, 1, 3, 2, 0},
	{0, 7, 6, 4, 2, 3, 1, 3, 2, 0},
	{0, 5, 6, 4, 2, 3, 1, 3, 2, 0},
	{0, 2, 6, 4, 2, 2, 1, 3, 2, 0},
	{0, 5, 5, 4, 1, 2, 1, 3, 2, 0},
	{0, 2, 5, 3, 1, 2, 1, 3, 2, 0},
	{0, 3, 5, 3, 2, 2, 0, 2, 2, 0},
	{0, 4, 5, 3, 2, 2, 2, 3, 2, 0},
	{0, 4, 4, 2, 1, 2, 2, 3, 2, 0},
	{0, 5, 4, 3, 2, 2, 2, 3, 2, 0},
	{0, 5, 4, 3, 1, 1, 2, 3, 2, 0},
	{0, 2, 5, 3, 1, 1, 2, 3, 2, 0},
	{0, 4, 5, 3, 2, 2, 3, 4, 2, 0},
	{0, 4, 4, 2, 2, 2, 2, 4, 1, 0},
	{0, 5, 4, 3, 2, 2, 3, 4, 2, 0},
	{0, 4, 4, 2, 2, 2, 3, 4, 2, 0},
	{0, 5, 4, 2, 2, 2, 3, 4, 2, 0},
	{0, 4, 3, 2, 2, 2, 3, 5, 3, 0},
	{0, 4, 3, 2, 1, 2, 2, 4, 2, 0},
	{0, 4, 3, 1, 2, 2, 3, 4, 2, 0},
	{0, 1, 3, 1, 2, 2, 3, 4, 2, 0},
	{0, 0, 3, 1, 1, 1, 3, 3, 2, 0},
	{0, 2, 2, 1, 1, 1, 3, 4, 1, 0},
	{0, 0, 2, 1, 1, 1, 2, 3, 1, 0},
	{0, 0, 2, 0, 1, 1, 2, 3, 1, 0},
	{0, 3, 1, 0, 1, 1, 1, 2, 1, 0},
	{0, 3, 1, 1, 0, 1, 1, 3, 1, 0},
	{0, 3, 0, 0, 0, 0, 0, 2, 2, 0},
	{0, 0, 1, 0, 0, 0, 0, 1, 1, 0},
	{0, 0, 1, 0, 0, 0, 0, 2, 2, 0},
	{0, 0, 1, -1, 1, 0, 1, 2, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, -7, 3, -6, -2, 1, -4, -6, 2, -6, -7, 4, -9, -7, 3, -8, -4, 1, -4, -2, 0, -3, -4, 0, -1},
	{0, 0, 0, 0, 0, 0, -7, 3, -6, -2, 1, -4, -7, 3, -8, -7, 3, -8, -7, 3, -8, -5, 0, -4, -1, 0, -2, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -6, -6, 3, -7, -5, 3, -6, -8, 4, -9, -6, 2, -6, -5, 0, -4, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -7, -5, 2, -6, -7, 4, -8, -5, 3, -8, -6, 3, -6, -3, 0, -3, 0, 0, 0, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -7, -5, 3, -8, -6, 3, -6, -6, 4, -8, -5, 3, -6, -2, 0, -2, 1, 0, 0, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -7, -6, 3, -8, -6, 3, -8, -5, 4, -8, -5, 2, -6, -2, 0, -2, 1, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 4, -10, -5, 3, -6, -6, 3, -8, -6, 3, -8, -3, 2, -4, 0, 0, -2, -1, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 3, -7, -6, 4, -8, -4, 3, -6, -7, 3, -8, -3, 1, -4, 0, 0, -2, -1, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 4, -9, -5, 4, -8, -5, 3, -6, -5, 4, -8, -4, 2, -4, 0, 0, -2, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -8, -4, 3, -6, -6, 3, -7, -5, 3, -8, -3, 2, -4, 0, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 4, -10, -6, 3, -8, -5, 2, -6, -5, 3, -8, -3, 2, -4, 0, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 4, -9, -3, 3, -6, -6, 2, -6, -5, 3, -8, -3, 1, -4, 0, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 4, -9, -5, 3, -8, -4, 2, -5, -5, 3, -6, -3, 1, -4, 0, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 4, -10, -6, 3, -8, -3, 2, -5, -6, 3, -7, -1, 1, -4, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 5, -10, -6, 4, -9, -4, 2, -5, -5, 3, -7, -1, 1, -4, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 6, -12, -4, 3, -7, -4, 2, -6, -4, 3, -6, -1, 1, -4, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 4, -10, -5, 3, -8, -3, 2, -5, -4, 3, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -10, -4, 3, -8, -3, 2, -5, -4, 3, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 6, -14, -4, 3, -7, -4, 2, -5, -3, 3, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 6, -12, -4, 3, -7, -3, 2, -5, -4, 2, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 6, -12, -5, 3, -8, -3, 2, -4, -3, 3, -6, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 6, -13, -5, 3, -8, -3, 2, -4, -3, 3, -6, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 4, -10, -6, 4, -9, -3, 2, -4, -3, 2, -6, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 6, -14, -5, 3, -8, -2, 1, -4, -3, 2, -5, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 6, -13, -5, 4, -8, -3, 1, -4, -2, 2, -5, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 6, -13, -5, 3, -8, -3, 1, -4, -2, 2, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -6, 7, -16, -4, 3, -7, -3, 1, -4, -2, 2, -5, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 7, -15, -4, 3, -6, -3, 1, -3, -2, 2, -5, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 5, -12, -5, 3, -8, -3, 1, -3, -1, 2, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -12, -6, 3, -8, -1, 1, -2, -1, 2, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 6, -14, -4, 3, -6, -2, 1, -3, -1, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 6, -14, -4, 3, -6, -1, 1, -2, -1, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 5, -12, -5, 4, -9, -1, 1, -2, -1, 2, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 6, -13, -4, 2, -6, -2, 1, -3, -2, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 7, -15, -5, 4, -8, -1, 1, -2, -2, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 7, -14, -5, 3, -8, -1, 1, -2, -2, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 6, -13, -4, 3, -7, 0, 0, -2, -3, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 7, -16, -4, 2, -6, 0, 1, -2, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 6, -14, -3, 3, -6, 0, 0, -2, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 6, -14, -3, 3, -6, -1, 0, -1, 0, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 6, -14, -3, 2, -6, 0, 0, -1, -2, 1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 7, -14, -3, 2, -6, 0, 0, -1, -2, 1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 6, -14, -2, 2, -5, -1, 0, -1, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 6, -14, -2, 2, -4, -1, 0, -1, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 5, -12, -2, 1, -4, -2, 0, -2, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -12, -2, 2, -4, -1, 0, -2, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 5, -12, -2, 2, -4, -1, 0, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 6, -13, -2, 1, -4, 0, 0, -1, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -12, -2, 1, -4, -1, 1, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -12, -2, 1, -4, -1, 0, -2, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 5, -12, 0, 1, -2, -1, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -11, 0, 1, -2, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 4, -10, 0, 1, -2, -2, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 3, -8, -1, 0, -2, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -7, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -8, 0, 0, -1, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -7, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 2, -6, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#endif /* __DYNAMIC_AID_XXXX_H */
