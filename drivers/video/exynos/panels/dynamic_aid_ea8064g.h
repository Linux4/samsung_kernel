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

#define VREG_OUT_X1000		6000	/* VREG_OUT x 1000 */

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
	110,	/* IBRIGHTNESS_2NT */
	110,	/* IBRIGHTNESS_3NT */
	110,	/* IBRIGHTNESS_4NT */
	110,	/* IBRIGHTNESS_5NT */
	110,	/* IBRIGHTNESS_6NT */
	110,	/* IBRIGHTNESS_7NT */
	110,	/* IBRIGHTNESS_8NT */
	110,	/* IBRIGHTNESS_9NT */
	110,	/* IBRIGHTNESS_10NT */
	110,	/* IBRIGHTNESS_11NT */
	110,	/* IBRIGHTNESS_12NT */
	110,	/* IBRIGHTNESS_13NT */
	110,	/* IBRIGHTNESS_14NT */
	110,	/* IBRIGHTNESS_15NT */
	110,	/* IBRIGHTNESS_16NT */
	110,	/* IBRIGHTNESS_17NT */
	110,	/* IBRIGHTNESS_19NT */
	110,	/* IBRIGHTNESS_20NT */
	110,	/* IBRIGHTNESS_21NT */
	110,	/* IBRIGHTNESS_22NT */
	110,	/* IBRIGHTNESS_24NT */
	110,	/* IBRIGHTNESS_25NT */
	110,	/* IBRIGHTNESS_27NT */
	110,	/* IBRIGHTNESS_29NT */
	110,	/* IBRIGHTNESS_30NT */
	110,	/* IBRIGHTNESS_32NT */
	110,	/* IBRIGHTNESS_34NT */
	110,	/* IBRIGHTNESS_37NT */
	110,	/* IBRIGHTNESS_39NT */
	110,	/* IBRIGHTNESS_41NT */
	110,	/* IBRIGHTNESS_44NT */
	110,	/* IBRIGHTNESS_47NT */
	110,	/* IBRIGHTNESS_50NT */
	110,	/* IBRIGHTNESS_53NT */
	110,	/* IBRIGHTNESS_56NT */
	110,	/* IBRIGHTNESS_60NT */
	110,	/* IBRIGHTNESS_64NT */
	110,	/* IBRIGHTNESS_68NT */
	117,	/* IBRIGHTNESS_72NT */
	123,	/* IBRIGHTNESS_77NT */
	131,	/* IBRIGHTNESS_82NT */
	140,	/* IBRIGHTNESS_87NT */
	149,	/* IBRIGHTNESS_93NT */
	156,	/* IBRIGHTNESS_98NT */
	168,	/* IBRIGHTNESS_105NT */
	176,	/* IBRIGHTNESS_111NT */
	189,	/* IBRIGHTNESS_119NT */
	199,	/* IBRIGHTNESS_126NT */
	210,	/* IBRIGHTNESS_134NT */
	223,	/* IBRIGHTNESS_143NT */
	237,	/* IBRIGHTNESS_152NT */
	249,	/* IBRIGHTNESS_162NT */
	249,	/* IBRIGHTNESS_172NT */
	249,	/* IBRIGHTNESS_183NT */
	249,	/* IBRIGHTNESS_195NT */
	249,	/* IBRIGHTNESS_207NT */
	249,	/* IBRIGHTNESS_220NT */
	249,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	267,	/* IBRIGHTNESS_265NT */
	283,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	318,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
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
	{0x05, 0x26},	/* IBRIGHTNESS_2NT */
	{0x05, 0x1A},	/* IBRIGHTNESS_3NT */
	{0x05, 0x0E},	/* IBRIGHTNESS_4NT */
	{0x05, 0x04},	/* IBRIGHTNESS_5NT */
	{0x04, 0xF8},	/* IBRIGHTNESS_6NT */
	{0x04, 0xEC},	/* IBRIGHTNESS_7NT */
	{0x04, 0xE0},	/* IBRIGHTNESS_8NT */
	{0x04, 0xD4},	/* IBRIGHTNESS_9NT */
	{0x04, 0xC8},	/* IBRIGHTNESS_10NT */
	{0x04, 0xBE},	/* IBRIGHTNESS_11NT */
	{0x04, 0xB0},	/* IBRIGHTNESS_12NT */
	{0x04, 0xA4},	/* IBRIGHTNESS_13NT */
	{0x04, 0x98},	/* IBRIGHTNESS_14NT */
	{0x04, 0x8C},	/* IBRIGHTNESS_15NT */
	{0x04, 0x7E},	/* IBRIGHTNESS_16NT */
	{0x04, 0x72},	/* IBRIGHTNESS_17NT */
	{0x04, 0x5A},	/* IBRIGHTNESS_19NT */
	{0x04, 0x4E},	/* IBRIGHTNESS_20NT */
	{0x04, 0x40},	/* IBRIGHTNESS_21NT */
	{0x04, 0x34},	/* IBRIGHTNESS_22NT */
	{0x04, 0x1E},	/* IBRIGHTNESS_24NT */
	{0x04, 0x11},	/* IBRIGHTNESS_25NT */
	{0x03, 0xF8},	/* IBRIGHTNESS_27NT */
	{0x03, 0xE0},	/* IBRIGHTNESS_29NT */
	{0x03, 0xD4},	/* IBRIGHTNESS_30NT */
	{0x03, 0xBC},	/* IBRIGHTNESS_32NT */
	{0x03, 0xA4},	/* IBRIGHTNESS_34NT */
	{0x03, 0x7E},	/* IBRIGHTNESS_37NT */
	{0x03, 0x66},	/* IBRIGHTNESS_39NT */
	{0x03, 0x4C},	/* IBRIGHTNESS_41NT */
	{0x03, 0x28},	/* IBRIGHTNESS_44NT */
	{0x03, 0x02},	/* IBRIGHTNESS_47NT */
	{0x02, 0xDD},	/* IBRIGHTNESS_50NT */
	{0x02, 0xB8},	/* IBRIGHTNESS_53NT */
	{0x02, 0x90},	/* IBRIGHTNESS_56NT */
	{0x02, 0x5E},	/* IBRIGHTNESS_60NT */
	{0x02, 0x2C},	/* IBRIGHTNESS_64NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_68NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_72NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_77NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_82NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_87NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_93NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_98NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_105NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_111NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_119NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_126NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_134NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_143NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_152NT */
	{0x01, 0xF8},	/* IBRIGHTNESS_162NT */
	{0x01, 0xC2},	/* IBRIGHTNESS_172NT */
	{0x01, 0x84},	/* IBRIGHTNESS_183NT */
	{0x01, 0x40},	/* IBRIGHTNESS_195NT */
	{0x00, 0xFA},	/* IBRIGHTNESS_207NT */
	{0x00, 0xB3},	/* IBRIGHTNESS_220NT */
	{0x00, 0x62},	/* IBRIGHTNESS_234NT */
	{0x00, 0x08},	/* IBRIGHTNESS_249NT */
	{0x00, 0x08},	/* IBRIGHTNESS_265NT */
	{0x00, 0x08},	/* IBRIGHTNESS_282NT */
	{0x00, 0x08},	/* IBRIGHTNESS_300NT */
	{0x00, 0x08},	/* IBRIGHTNESS_316NT */
	{0x00, 0x08},	/* IBRIGHTNESS_333NT */
	{0x00, 0x08},	/* IBRIGHTNESS_360NT */
	{0x00, 0x08}	/* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 40 , 39 , 38 , 36 , 31 , 24 , 15 , 9 , 0},
	{0, 33 , 32 , 31 , 27 , 23 , 19 , 11 , 7 , 0},
	{0, 31 , 30 , 29 , 26 , 21 , 17 , 9 , 6 , 0},
	{0, 29 , 28 , 25 , 21 , 17 , 13 , 6 , 5 , 0},
	{0, 28 , 27 , 23 , 19 , 16 , 12 , 6 , 5 , 0},
	{0, 26 , 25 , 21 , 18 , 14 , 11 , 6 , 4 , 0},
	{0, 25 , 24 , 20 , 17 , 14 , 10 , 5 , 4 , 0},
	{0, 24 , 23 , 18 , 16 , 12 , 9 , 4 , 4 , 0},
	{0, 22 , 21 , 17 , 14 , 11 , 9 , 4 , 4 , 0},
	{0, 22 , 21 , 17 , 14 , 11 , 9 , 4 , 4 , 0},
	{0, 20 , 19 , 15 , 12 , 10 , 8 , 3 , 3 , 0},
	{0, 20 , 19 , 15 , 12 , 10 , 7 , 3 , 3 , 0},
	{0, 20 , 19 , 15 , 12 , 10 , 7 , 3 , 3 , 0},
	{0, 19 , 18 , 14 , 11 , 9 , 7 , 3 , 3 , 0},
	{0, 19 , 18 , 14 , 11 , 9 , 7 , 3 , 3 , 0},
	{0, 18 , 17 , 14 , 11 , 9 , 6 , 3 , 3 , 0},
	{0, 17 , 16 , 12 , 10 , 8 , 6 , 3 , 3 , 0},
	{0, 16 , 15 , 12 , 10 , 8 , 6 , 3 , 3 , 0},
	{0, 16 , 15 , 12 , 10 , 7 , 5 , 3 , 3 , 0},
	{0, 16 , 15 , 11 , 9 , 7 , 5 , 3 , 3 , 0},
	{0, 16 , 15 , 11 , 9 , 6 , 5 , 3 , 3 , 0},
	{0, 15 , 14 , 10 , 8 , 6 , 5 , 3 , 3 , 0},
	{0, 14 , 13 , 9 , 8 , 6 , 4 , 3 , 3 , 0},
	{0, 14 , 13 , 9 , 7 , 5 , 4 , 3 , 3 , 0},
	{0, 13 , 12 , 8 , 7 , 4 , 4 , 3 , 3 , 0},
	{0, 13 , 12 , 8 , 6 , 4 , 4 , 3 , 3 , 0},
	{0, 12 , 11 , 7 , 6 , 4 , 4 , 3 , 3 , 0},
	{0, 11 , 10 , 7 , 5 , 4 , 3 , 3 , 3 , 0},
	{0, 11 , 10 , 6 , 5 , 3 , 3 , 3 , 3 , 0},
	{0, 10 , 9 , 6 , 5 , 3 , 3 , 3 , 3 , 0},
	{0, 9 , 8 , 5 , 4 , 3 , 3 , 3 , 3 , 0},
	{0, 9 , 8 , 5 , 4 , 3 , 3 , 3 , 3 , 0},
	{0, 8 , 7 , 5 , 4 , 2 , 3 , 3 , 2 , 0},
	{0, 8 , 7 , 5 , 3 , 2 , 3 , 3 , 2 , 0},
	{0, 7 , 6 , 4 , 3 , 2 , 3 , 3 , 2 , 0},
	{0, 6 , 5 , 4 , 3 , 2 , 3 , 3 , 2 , 0},
	{0, 6 , 5 , 3 , 3 , 2 , 3 , 3 , 2 , 0},
	{0, 6 , 5 , 3 , 2 , 1 , 2 , 2 , 2 , 0},
	{0, 5 , 4 , 3 , 2 , 1 , 2 , 3 , 2 , 0},
	{0, 5 , 4 , 3 , 2 , 1 , 2 , 3 , 2 , 0},
	{0, 5 , 4 , 3 , 2 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 3 , 2 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 3 , 2 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 3 , 2 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 3 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 2 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 2 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 2 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 2 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 2 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 3 , 2 , 2 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 4 , 3 , 1 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 3 , 2 , 1 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 3 , 2 , 1 , 1 , 1 , 2 , 3 , 2 , 0},
	{0, 2 , 1 , 0 , 1 , 0 , 2 , 3 , 2 , 0},
	{0, 2 , 1 , 0 , 0 , 0 , 1 , 2 , 2 , 0},
	{0, 2 , 1 , 0 , 0 , 0 , 1 , 2 , 1 , 0},
	{0, 2 , 1 , 0 , 0 , 0 , 1 , 2 , 1 , 0},
	{0, 2 , 1 , 0 , 0 , 0 , 1 , 1 , 1 , 0},
	{0, 2 , 1 , 0 , 0 , 0 , 1 , 2 , 2 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 0 , 1 , 0 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0}
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, -8, 6, -11, -3, 5, -6, -3, 4, -8, -8, 4, -12, -7, 3, -8, -3, 1, -3, -4, 0, -3, -3, 0, -3},
	{0, 0, 0, 0, 0, 0, -6, 6, -10, -4, 4, -6, -4, 4, -8, -8, 4, -10, -7, 2, -7, -2, 1, -2, -4, 0, -3, -3, 0, -2},
	{0, 0, 0, 0, 0, 0, -7, 6, -9, -3, 4, -7, -6, 3, -8, -7, 3, -8, -6, 2, -7, -3, 0, -2, -2, 0, -2, -3, 0, -1},
	{0, 0, 0, 0, 0, 0, -5, 4, -10, -3, 3, -6, -8, 3, -10, -7, 3, -9, -4, 2, -6, -2, 0, -2, -2, 0, -1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -7, -3, 3, -6, -9, 3, -12, -9, 2, -8, -5, 1, -7, -2, 0, -3, -2, 0, -1, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 4, -8, -6, 3, -8, -7, 3, -9, -8, 2, -8, -5, 1, -7, -2, 0, -3, -2, 0, -1, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 4, -7, -6, 2, -8, -6, 3, -8, -6, 2, -6, -4, 1, -6, -2, 0, -3, -2, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 3, -5, -6, 3, -11, -4, 3, -5, -8, 2, -8, -4, 1, -7, -1, 0, -2, -2, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 4, -7, -4, 3, -9, -6, 3, -7, -7, 2, -8, -4, 1, -6, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -7, -3, 3, -7, -5, 3, -6, -7, 2, -7, -3, 1, -5, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -10, -5, 3, -10, -6, 3, -8, -5, 2, -6, -3, 1, -5, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 3, -6, -6, 3, -9, -5, 3, -7, -5, 2, -6, -3, 1, -5, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -6, -5, 3, -9, -4, 3, -6, -5, 2, -6, -3, 1, -5, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -6, -5, 3, -9, -4, 3, -6, -5, 2, -6, -2, 1, -4, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -8, -6, 3, -9, -5, 2, -8, -5, 1, -6, -2, 1, -4, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 3, -9, -6, 2, -9, -5, 2, -7, -5, 1, -5, -1, 1, -3, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 3, -6, -5, 3, -11, -6, 1, -7, -6, 1, -6, 0, 1, -2, -1, 0, -2, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 4, -13, -4, 3, -9, -4, 1, -6, -5, 1, -5, 0, 1, -3, -1, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 4, -11, -3, 3, -8, -4, 2, -6, -5, 1, -6, -2, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -9, -3, 3, -9, -5, 2, -7, -4, 1, -5, -2, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 2, -9, -4, 3, -7, -3, 2, -5, -4, 1, -4, -2, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -12, -4, 3, -8, -5, 2, -7, -4, 0, -4, -2, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -13, -6, 3, -10, -4, 2, -6, -4, 0, -4, -1, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -11, -4, 3, -6, -4, 2, -6, -4, 0, -4, -1, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 3, -9, -3, 3, -6, -3, 2, -5, -4, 0, -4, -1, 0, -3, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 2, -8, -3, 3, -6, -3, 2, -6, -5, 0, -4, 0, 0, -2, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -9, -3, 3, -6, -2, 2, -5, -4, 0, -3, 0, 0, -2, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -11, -1, 2, -4, -2, 2, -5, -2, 0, -2, -1, 0, -1, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -9, -2, 2, -6, -3, 2, -5, -2, 0, -2, 0, 0, -1, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -13, -1, 2, -5, -4, 2, -5, -1, 0, -1, 0, 0, -1, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -15, -1, 2, -5, -2, 2, -5, -2, 0, -2, 1, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 4, -13, -3, 2, -6, -2, 1, -5, -1, 0, -1, 1, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 4, -16, -1, 2, -4, -2, 1, -5, -2, 0, -1, 0, 0, 0, 1, 0, -1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -13, 0, 2, -6, -3, 1, -5, -2, 0, -1, 1, 0, -1, 0, 0, 0, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -13, 0, 2, -7, -3, 1, -4, -2, 0, -1, 1, 0, 0, 0, 0, 0, -1, 0, -1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 3, -15, 0, 2, -6, -2, 1, -4, -2, 0, -1, 1, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 3, -13, 0, 2, -6, -2, 1, -3, 0, 0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -8, -1, 2, -8, -1, 1, -4, -1, 0, 0, 2, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 4, -11, -1, 2, -7, -1, 1, -3, -1, 0, 0, 1, 0, 0, 1, 0, 0, -2, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 3, -11, -1, 2, -7, -1, 1, -3, -1, 0, 1, 1, 0, 0, 0, 0, 0, -2, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -13, 0, 1, -7, -1, 1, -3, -1, 0, 1, 1, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -15, 0, 1, -6, -1, 1, -2, -2, 0, 0, 1, 0, -1, 0, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 4, -17, 0, 1, -6, -1, 0, -3, -2, 0, 0, 1, 0, 0, 0, 0, 0, -2, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -16, 0, 1, -7, -1, 0, -2, -2, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 4, -13, 0, 1, -5, -1, 0, -3, -2, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -6, 2, -11, -1, 1, -5, -1, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -9, -1, 1, -5, -1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 3, -11, -2, 1, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, -2, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -9, -2, 1, -4, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -9, -3, 1, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 2, -9, -4, 1, -4, 0, 0, -2, 1, 0, 1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 1, -6, -2, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 1, -6, -2, 1, -4, -1, 0, -2, -2, 0, 0, 1, 0, -2, 0, 0, 2, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 1, -5, -2, 1, -3, 0, 0, -1, -1, 0, 0, 1, 0, 0, 0, 0, 1, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 1, -5, -1, 1, -3, 0, 0, -1, -1, 0, 0, 1, 0, 0, 0, 0, 1, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 0, -4, 1, 1, -5, 0, 0, 0, 0, 0, 1, 1, 0, 2, 0, 0, 0, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 0, -2, 1, 1, -4, 0, 0, 0, 0, 0, 1, 1, 0, 2, 0, 0, 0, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 1, -2, 0, 0, -1, 0, 0, 1, 1, 0, 2, -1, 0, 0, 0, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -3, 0, 0, -1, 1, 0, 2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, -2, 0, 0, 0, 1, 0, 2, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, -2, -1, 0, -1, 2, 0, 1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, -3, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#if 0
static const int index_brightness_table_f[IBRIGHTNESS_MAX] = {
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
	350,	/* IBRIGHTNESS_350NT */
	350	/* IBRIGHTNESS_550NT */
};

static const int brightness_base_table_f[IBRIGHTNESS_MAX] = {
	110,	/* IBRIGHTNESS_2NT */
	110,	/* IBRIGHTNESS_3NT */
	110,	/* IBRIGHTNESS_4NT */
	110,	/* IBRIGHTNESS_5NT */
	110,	/* IBRIGHTNESS_6NT */
	110,	/* IBRIGHTNESS_7NT */
	110,	/* IBRIGHTNESS_8NT */
	110,	/* IBRIGHTNESS_9NT */
	110,	/* IBRIGHTNESS_10NT */
	110,	/* IBRIGHTNESS_11NT */
	110,	/* IBRIGHTNESS_12NT */
	110,	/* IBRIGHTNESS_13NT */
	110,	/* IBRIGHTNESS_14NT */
	110,	/* IBRIGHTNESS_15NT */
	110,	/* IBRIGHTNESS_16NT */
	110,	/* IBRIGHTNESS_17NT */
	110,	/* IBRIGHTNESS_19NT */
	110,	/* IBRIGHTNESS_20NT */
	110,	/* IBRIGHTNESS_21NT */
	110,	/* IBRIGHTNESS_22NT */
	110,	/* IBRIGHTNESS_24NT */
	110,	/* IBRIGHTNESS_25NT */
	110,	/* IBRIGHTNESS_27NT */
	110,	/* IBRIGHTNESS_29NT */
	110,	/* IBRIGHTNESS_30NT */
	110,	/* IBRIGHTNESS_32NT */
	110,	/* IBRIGHTNESS_34NT */
	110,	/* IBRIGHTNESS_37NT */
	110,	/* IBRIGHTNESS_39NT */
	110,	/* IBRIGHTNESS_41NT */
	110,	/* IBRIGHTNESS_44NT */
	110,	/* IBRIGHTNESS_47NT */
	110,	/* IBRIGHTNESS_50NT */
	110,	/* IBRIGHTNESS_53NT */
	110,	/* IBRIGHTNESS_56NT */
	110,	/* IBRIGHTNESS_60NT */
	110,	/* IBRIGHTNESS_64NT */
	110,	/* IBRIGHTNESS_68NT */
	117,	/* IBRIGHTNESS_72NT */
	123,	/* IBRIGHTNESS_77NT */
	133,	/* IBRIGHTNESS_82NT */
	139,	/* IBRIGHTNESS_87NT */
	150,	/* IBRIGHTNESS_93NT */
	157,	/* IBRIGHTNESS_98NT */
	167,	/* IBRIGHTNESS_105NT */
	177,	/* IBRIGHTNESS_111NT */
	189,	/* IBRIGHTNESS_119NT */
	198,	/* IBRIGHTNESS_126NT */
	210,	/* IBRIGHTNESS_134NT */
	225,	/* IBRIGHTNESS_143NT */
	239,	/* IBRIGHTNESS_152NT */
	250,	/* IBRIGHTNESS_162NT */
	250,	/* IBRIGHTNESS_172NT */
	250,	/* IBRIGHTNESS_183NT */
	250,	/* IBRIGHTNESS_195NT */
	250,	/* IBRIGHTNESS_207NT */
	250,	/* IBRIGHTNESS_220NT */
	250,	/* IBRIGHTNESS_234NT */
	250,	/* IBRIGHTNESS_249NT */
	268,	/* IBRIGHTNESS_265NT */
	283,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	334,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
	350	/* IBRIGHTNESS_550NT */
};

static const unsigned char aor_cmd_f[IBRIGHTNESS_MAX][2] = {
	{0x05, 0x26},	/* IBRIGHTNESS_2NT */
	{0x05, 0x1A},	/* IBRIGHTNESS_3NT */
	{0x05, 0x10},	/* IBRIGHTNESS_4NT */
	{0x05, 0x04},	/* IBRIGHTNESS_5NT */
	{0x04, 0xF8},	/* IBRIGHTNESS_6NT */
	{0x04, 0xEE},	/* IBRIGHTNESS_7NT */
	{0x04, 0xE2},	/* IBRIGHTNESS_8NT */
	{0x04, 0xD6},	/* IBRIGHTNESS_9NT */
	{0x04, 0xCA},	/* IBRIGHTNESS_10NT */
	{0x04, 0xC0},	/* IBRIGHTNESS_11NT */
	{0x04, 0xB2},	/* IBRIGHTNESS_12NT */
	{0x04, 0xA8},	/* IBRIGHTNESS_13NT */
	{0x04, 0x9A},	/* IBRIGHTNESS_14NT */
	{0x04, 0x90},	/* IBRIGHTNESS_15NT */
	{0x04, 0x83},	/* IBRIGHTNESS_16NT */
	{0x04, 0x78},	/* IBRIGHTNESS_17NT */
	{0x04, 0x5E},	/* IBRIGHTNESS_19NT */
	{0x04, 0x54},	/* IBRIGHTNESS_20NT */
	{0x04, 0x46},	/* IBRIGHTNESS_21NT */
	{0x04, 0x3A},	/* IBRIGHTNESS_22NT */
	{0x04, 0x22},	/* IBRIGHTNESS_24NT */
	{0x04, 0x16},	/* IBRIGHTNESS_25NT */
	{0x03, 0xFE},	/* IBRIGHTNESS_27NT */
	{0x03, 0xE6},	/* IBRIGHTNESS_29NT */
	{0x03, 0xDA},	/* IBRIGHTNESS_30NT */
	{0x03, 0xC2},	/* IBRIGHTNESS_32NT */
	{0x03, 0xAA},	/* IBRIGHTNESS_34NT */
	{0x03, 0x86},	/* IBRIGHTNESS_37NT */
	{0x03, 0x6E},	/* IBRIGHTNESS_39NT */
	{0x03, 0x56},	/* IBRIGHTNESS_41NT */
	{0x03, 0x30},	/* IBRIGHTNESS_44NT */
	{0x03, 0x0D},	/* IBRIGHTNESS_47NT */
	{0x02, 0xE7},	/* IBRIGHTNESS_50NT */
	{0x02, 0xC2},	/* IBRIGHTNESS_53NT */
	{0x02, 0x9C},	/* IBRIGHTNESS_56NT */
	{0x02, 0x6C},	/* IBRIGHTNESS_60NT */
	{0x02, 0x36},	/* IBRIGHTNESS_64NT */
	{0x02, 0x01},	/* IBRIGHTNESS_68NT */
	{0x02, 0x01},	/* IBRIGHTNESS_72NT */
	{0x02, 0x01},	/* IBRIGHTNESS_77NT */
	{0x02, 0x01},	/* IBRIGHTNESS_82NT */
	{0x02, 0x01},	/* IBRIGHTNESS_87NT */
	{0x02, 0x01},	/* IBRIGHTNESS_93NT */
	{0x02, 0x01},	/* IBRIGHTNESS_98NT */
	{0x02, 0x01},	/* IBRIGHTNESS_105NT */
	{0x02, 0x01},	/* IBRIGHTNESS_111NT */
	{0x02, 0x01},	/* IBRIGHTNESS_119NT */
	{0x02, 0x01},	/* IBRIGHTNESS_126NT */
	{0x02, 0x01},	/* IBRIGHTNESS_134NT */
	{0x02, 0x01},	/* IBRIGHTNESS_143NT */
	{0x02, 0x01},	/* IBRIGHTNESS_152NT */
	{0x02, 0x01},	/* IBRIGHTNESS_162NT */
	{0x01, 0xC4},	/* IBRIGHTNESS_172NT */
	{0x01, 0x84},	/* IBRIGHTNESS_183NT */
	{0x01, 0x40},	/* IBRIGHTNESS_195NT */
	{0x00, 0xFA},	/* IBRIGHTNESS_207NT */
	{0x00, 0xB6},	/* IBRIGHTNESS_220NT */
	{0x00, 0x65},	/* IBRIGHTNESS_234NT */
	{0x00, 0x08},	/* IBRIGHTNESS_249NT */
	{0x00, 0x08},	/* IBRIGHTNESS_265NT */
	{0x00, 0x08},	/* IBRIGHTNESS_282NT */
	{0x00, 0x08},	/* IBRIGHTNESS_300NT */
	{0x00, 0x08},	/* IBRIGHTNESS_316NT */
	{0x00, 0x08},	/* IBRIGHTNESS_333NT */
	{0x00, 0x08},	/* IBRIGHTNESS_350NT */
	{0x00, 0x08}	/* IBRIGHTNESS_550NT */
};

static const int offset_gradation_f[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 43 , 42 , 41 , 37 , 33 , 24 , 16 , 9 , 0},
	{0, 36 , 39 , 34 , 31 , 27 , 21 , 12 , 7 , 0},
	{0, 28 , 34 , 30 , 26 , 21 , 17 , 9 , 6 , 0},
	{0, 27 , 28 , 25 , 22 , 18 , 15 , 8 , 5 , 0},
	{0, 27 , 27 , 23 , 20 , 17 , 13 , 7 , 5 , 0},
	{0, 26 , 26 , 22 , 18 , 15 , 11 , 5 , 3 , 0},
	{0, 23 , 24 , 19 , 17 , 13 , 10 , 4 , 3 , 0},
	{0, 22 , 23 , 18 , 15 , 12 , 9 , 4 , 3 , 0},
	{0, 18 , 22 , 17 , 14 , 11 , 8 , 4 , 3 , 0},
	{0, 20 , 21 , 17 , 14 , 11 , 8 , 4 , 3 , 0},
	{0, 18 , 20 , 16 , 13 , 10 , 7 , 3 , 3 , 0},
	{0, 16 , 20 , 16 , 12 , 10 , 7 , 3 , 3 , 0},
	{0, 17 , 18 , 14 , 11 , 9 , 7 , 3 , 3 , 0},
	{0, 19 , 18 , 14 , 12 , 9 , 7 , 3 , 3 , 0},
	{0, 15 , 18 , 14 , 11 , 9 , 7 , 3 , 3 , 0},
	{0, 17 , 17 , 13 , 11 , 8 , 6 , 3 , 3 , 0},
	{0, 12 , 16 , 12 , 10 , 7 , 6 , 3 , 3 , 0},
	{0, 12 , 16 , 12 , 10 , 7 , 5 , 3 , 3 , 0},
	{0, 13 , 15 , 11 , 9 , 7 , 5 , 3 , 3 , 0},
	{0, 12 , 15 , 11 , 9 , 6 , 5 , 3 , 3 , 0},
	{0, 13 , 14 , 10 , 8 , 6 , 5 , 2 , 3 , 0},
	{0, 10 , 14 , 10 , 8 , 6 , 5 , 2 , 3 , 0},
	{0, 10 , 13 , 9 , 7 , 5 , 4 , 2 , 3 , 0},
	{0, 10 , 12 , 8 , 7 , 5 , 4 , 2 , 3 , 0},
	{0, 10 , 12 , 8 , 7 , 5 , 4 , 2 , 3 , 0},
	{0, 10 , 11 , 8 , 6 , 4 , 4 , 2 , 3 , 0},
	{0, 7 , 11 , 7 , 6 , 4 , 3 , 2 , 3 , 0},
	{0, 7 , 10 , 6 , 5 , 3 , 3 , 2 , 3 , 0},
	{0, 7 , 9 , 6 , 5 , 3 , 3 , 2 , 3 , 0},
	{0, 6 , 9 , 5 , 4 , 3 , 3 , 2 , 3 , 0},
	{0, 6 , 8 , 5 , 4 , 2 , 3 , 2 , 3 , 0},
	{0, 6 , 8 , 5 , 4 , 2 , 3 , 2 , 3 , 0},
	{0, 6 , 7 , 4 , 3 , 2 , 2 , 1 , 3 , 0},
	{0, 6 , 7 , 4 , 3 , 2 , 2 , 1 , 2 , 0},
	{0, 6 , 6 , 4 , 3 , 2 , 2 , 1 , 2 , 0},
	{0, 6 , 5 , 3 , 3 , 2 , 2 , 1 , 2 , 0},
	{0, 6 , 5 , 3 , 2 , 2 , 2 , 1 , 3 , 0},
	{0, 6 , 4 , 3 , 2 , 1 , 2 , 1 , 2 , 0},
	{0, 4 , 4 , 2 , 1 , 1 , 1 , 1 , 2 , 0},
	{0, 4 , 4 , 2 , 1 , 0 , 2 , 2 , 2 , 0},
	{0, 4 , 3 , 2 , 1 , 1 , 2 , 2 , 2 , 0},
	{0, 3 , 3 , 2 , 1 , 0 , 1 , 2 , 2 , 0},
	{0, 3 , 3 , 2 , 1 , 0 , 2 , 2 , 2 , 0},
	{0, 3 , 3 , 2 , 1 , 0 , 2 , 2 , 1 , 0},
	{0, 3 , 3 , 2 , 1 , 0 , 1 , 2 , 1 , 0},
	{0, 3 , 3 , 2 , 1 , 1 , 2 , 2 , 1 , 0},
	{0, 3 , 3 , 2 , 1 , 1 , 3 , 4 , 2 , 0},
	{0, 3 , 3 , 1 , 1 , 1 , 2 , 2 , 1 , 0},
	{0, 3 , 3 , 2 , 1 , 0 , 2 , 3 , 2 , 0},
	{0, 3 , 2 , 1 , 1 , 1 , 2 , 3 , 1 , 0},
	{0, 3 , 2 , 2 , 1 , 1 , 2 , 3 , 1 , 0},
	{0, 3 , 2 , 1 , 1 , 1 , 1 , 2 , 1 , 0},
	{0, 3 , 2 , 0 , 1 , 1 , 1 , 2 , 1 , 0},
	{0, 3 , 1 , 0 , 1 , 0 , 1 , 2 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 1 , 2 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 0 , 2 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 0 , 2 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 1 , 1 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 1 , 1 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 0 , 1 , 0 , 0},
	{0, 3 , 1 , 0 , 0 , -1 , 0 , 0 , 0 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 0 , 1 , 1 , 0},
	{0, 3 , 1 , 0 , 0 , 0 , 0 , 0 , 0 , 0},
	{0, 3 , 0 , 0 , 0 , 0 , 1 , 0 , 1 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0},
	{0, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0}
};

static const int offset_color_f[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, 0, 3, -5, -3, 3, -6, -4, 3, -11, -8, 3, -12, -9, 2, -10, -4, 0, -5, -4, 0, -4, -5, 0, -3},
	{0, 0, 0, 0, 0, 0, -1, 3, -3, -3, 3, -8, -4, 2, -7, -6, 3, -7, -6, 2, -7, -3, 0, -4, -3, 0, -2, -3, 0, -1},
	{0, 0, 0, 0, 0, 0, 1, 3, -6, -3, 3, -9, -5, 2, -8, -8, 3, -9, -6, 1, -8, -3, 0, -4, -3, 0, -2, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -4, 3, -10, -6, 2, -9, -8, 3, -8, -5, 1, -7, -2, 0, -3, -2, 0, -1, -2, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 3, -6, -6, 3, -12, -3, 3, -6, -7, 3, -7, -5, 1, -7, -2, 0, -2, -2, 0, -2, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 3, -7, -3, 3, -6, -6, 2, -8, -6, 3, -7, -4, 1, -7, -2, 0, -2, -2, 0, -2, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 2, -6, -4, 4, -12, -3, 3, -5, -7, 3, -8, -5, 1, -6, -2, 0, -3, -1, 0, -1, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 2, -7, -3, 4, -8, -4, 3, -6, -5, 3, -6, -5, 1, -6, -1, 0, -2, -1, 0, -1, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 2, 3, -3, -4, 4, -12, -4, 3, -7, -7, 2, -7, -4, 1, -6, -1, 0, -2, -1, 0, -1, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 3, -8, -3, 4, -9, -4, 3, -4, -4, 3, -5, -4, 1, -5, -1, 0, -2, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -4, 3, -11, -3, 3, -4, -5, 2, -5, -3, 1, -5, -1, 0, -2, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -4, 3, -8, -4, 3, -5, -5, 2, -5, -2, 1, -4, -1, 0, -2, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, 0, 3, -9, -4, 4, -12, -2, 3, -5, -6, 2, -6, -2, 1, -4, -1, 0, -2, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, 0, 4, -8, -5, 3, -14, -4, 2, -6, -5, 2, -6, -3, 1, -4, 0, 0, -1, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -4, 3, -12, -4, 2, -6, -5, 2, -5, -2, 1, -4, 0, 0, -1, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, 0, 3, -8, -6, 3, -14, -3, 2, -4, -4, 2, -4, -2, 1, -4, 0, 0, -1, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, 0, 3, -7, -6, 3, -13, -3, 2, -6, -3, 2, -4, -2, 1, -3, 0, 0, -1, -2, 0, -2, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -5, 3, -12, -3, 2, -6, -5, 1, -6, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -6, 3, -13, -3, 3, -6, -3, 2, -4, -1, 1, -3, 0, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -6, 3, -13, -3, 2, -6, -3, 2, -4, -1, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -6, 3, -13, -4, 2, -6, -3, 2, -4, -1, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -5, 3, -12, -3, 2, -5, -3, 2, -4, -1, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -5, 3, -12, -2, 2, -5, -2, 2, -2, -1, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -4, 3, -12, -2, 2, -5, -2, 1, -2, -2, 1, -2, 0, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -7, -4, 3, -12, -3, 2, -6, -2, 0, -2, -2, 1, -2, 0, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -4, 3, -12, -3, 2, -5, -2, 0, -2, -1, 1, -2, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -3, 3, -12, -3, 2, -5, -2, 0, -2, -1, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -3, 3, -12, -1, 2, -5, -2, 0, -2, 0, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -8, -3, 3, -12, 0, 2, -5, -2, 0, -2, 0, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 3, -9, -3, 4, -12, -1, 2, -4, -2, 0, -2, -1, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 1, 5, -9, -2, 3, -10, -1, 2, -4, -1, 0, -1, -1, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 4, -8, -2, 3, -10, -1, 1, -4, -1, 0, -1, -1, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -10, -2, 3, -12, -2, 1, -5, -2, 0, -1, 0, 0, 0, 1, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 4, -7, -2, 3, -11, -2, 1, -5, -1, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 1, 5, -10, -1, 3, -10, -2, 1, -4, -1, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 5, -12, 0, 3, -13, -1, 1, -2, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -10, -1, 3, -10, 0, 1, -4, 0, 0, 1, 1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 6, -11, -1, 2, -10, 0, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 6, -11, -1, 2, -9, 0, 1, -4, -1, 0, 1, 0, 0, 0, 1, 0, 1, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 6, -10, -1, 2, -10, 0, 0, -4, 0, 0, 2, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -1, 6, -10, -1, 2, -9, 0, 0, -4, -1, 0, 2, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 6, -11, -1, 2, -9, 0, 0, -3, 0, 0, 1, 0, 0, -1, 1, 0, 0, -1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 6, -12, -2, 2, -7, -1, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 3, 6, -9, -2, 2, -5, -1, 0, -4, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 2, 6, -9, -2, 2, -6, 0, 0, -2, 0, 0, 2, 0, 0, 0, 0, 0, -2, -1, 0, 1, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, 1, 6, -9, -2, 2, -6, -1, 0, -2, 0, 0, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 3, -11, -2, 1, -8, 0, 0, -1, -1, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -10, -1, 1, -7, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -9, -1, 0, -5, -1, 1, -2, -1, 0, 1, 1, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 2},
	{0, 0, 0, 0, 0, 0, -5, 2, -11, -1, 0, -6, -1, 0, -2, 0, 0, 0, 1, 0, -1, -1, 0, 0, -1, 0, -1, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 2, -10, -1, 0, -4, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, -1, -1, 0, -1, 1, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 3, -10, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 2, -6, -1, 1, -6, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0, -1, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 2, -8, -1, 1, -5, 1, 0, 0, 1, 0, 2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 2, -7, -2, -1, -6, 1, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 2, -5, -1, -1, -6, 0, 0, -1, 0, 0, 2, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 1, -4, -2, 0, -5, 1, -1, -1, 0, 0, 2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 2, -2, 0, -1, -4, 0, -1, -2, 1, 0, 2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 0, -3, 0, 0, -3, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -2, 0, -3, 0, 0, -2, 0, 0, 0, 0, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
#endif

#endif /* __DYNAMIC_AID_XXXX_H */
