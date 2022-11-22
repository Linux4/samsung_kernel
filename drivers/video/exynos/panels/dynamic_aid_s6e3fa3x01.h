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
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6200	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] =
{
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255		/* IV_255 */
};


static const int index_brightness_table[IBRIGHTNESS_MAX] =
{
	2,	/* IBRIGHTNESS_2 NT */
	3,	/* IBRIGHTNESS_3 NT */
	4,	/* IBRIGHTNESS_4 NT */
	5,	/* IBRIGHTNESS_5 NT */
	6,	/* IBRIGHTNESS_6 NT */
	7,	/* IBRIGHTNESS_7 NT */
	8,	/* IBRIGHTNESS_8 NT */
	9,	/* IBRIGHTNESS_9 NT */
	10,	/* IBRIGHTNESS_10 NT */
	11,	/* IBRIGHTNESS_11 NT */
	12,	/* IBRIGHTNESS_12 NT */
	13,	/* IBRIGHTNESS_13 NT */
	14,	/* IBRIGHTNESS_14 NT */
	15,	/* IBRIGHTNESS_15 NT */
	16,	/* IBRIGHTNESS_16 NT */
	17,	/* IBRIGHTNESS_17 NT */
	19,	/* IBRIGHTNESS_19 NT */
	20,	/* IBRIGHTNESS_20 NT */
	21,	/* IBRIGHTNESS_21 NT */
	22,	/* IBRIGHTNESS_22 NT */
	24,	/* IBRIGHTNESS_24 NT */
	25,	/* IBRIGHTNESS_25 NT */
	27,	/* IBRIGHTNESS_27 NT */
	29,	/* IBRIGHTNESS_29 NT */
	30,	/* IBRIGHTNESS_30 NT */
	32,	/* IBRIGHTNESS_32 NT */
	34,	/* IBRIGHTNESS_34 NT */
	37,	/* IBRIGHTNESS_37 NT */
	39,	/* IBRIGHTNESS_39 NT */
	41,	/* IBRIGHTNESS_41 NT */
	44,	/* IBRIGHTNESS_44 NT */
	47,	/* IBRIGHTNESS_47 NT */
	50,	/* IBRIGHTNESS_50 NT */
	53,	/* IBRIGHTNESS_53 NT */
	56,	/* IBRIGHTNESS_56 NT */
	60,	/* IBRIGHTNESS_60 NT */
	64,	/* IBRIGHTNESS_64 NT */
	68,	/* IBRIGHTNESS_68 NT */
	72,	/* IBRIGHTNESS_72 NT */
	77,	/* IBRIGHTNESS_77 NT */
	82,	/* IBRIGHTNESS_82 NT */
	87,	/* IBRIGHTNESS_87 NT */
	93,	/* IBRIGHTNESS_93 NT */
	98,	/* IBRIGHTNESS_98 NT */
	105,	/* IBRIGHTNESS_105 NT */
	111,	/* IBRIGHTNESS_111 NT */
	119,	/* IBRIGHTNESS_119 NT */
	126,	/* IBRIGHTNESS_126 NT */
	134,	/* IBRIGHTNESS_134 NT */
	143,	/* IBRIGHTNESS_143 NT */
	152,	/* IBRIGHTNESS_152 NT */
	162,	/* IBRIGHTNESS_162 NT */
	172,	/* IBRIGHTNESS_172 NT */
	183,	/* IBRIGHTNESS_183 NT */
	195,	/* IBRIGHTNESS_195 NT */
	207,	/* IBRIGHTNESS_207 NT */
	220,	/* IBRIGHTNESS_220 NT */
	234,	/* IBRIGHTNESS_234 NT */
	249,	/* IBRIGHTNESS_249 NT */
	265,	/* IBRIGHTNESS_265 NT */
	282,	/* IBRIGHTNESS_282 NT */
	300,	/* IBRIGHTNESS_300 NT */
	316,	/* IBRIGHTNESS_316 NT */
	333,	/* IBRIGHTNESS_333 NT */
	360,	/* IBRIGHTNESS_360 NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] =
{
	0x00, 0x00, 0x00,		/* IV_VT */
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

static const struct formular_t gamma_formula[IV_MAX] =
{
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

static const int vt_voltage_value[] =
{0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186};

static const int brightness_base_table[IBRIGHTNESS_MAX] =
{
	112,	/* IBRIGHTNESS_2 NT */
	112,	/* IBRIGHTNESS_3 NT */
	112,	/* IBRIGHTNESS_4 NT */
	112,	/* IBRIGHTNESS_5 NT */
	112,	/* IBRIGHTNESS_6 NT */
	112,	/* IBRIGHTNESS_7 NT */
	112,	/* IBRIGHTNESS_8 NT */
	112,	/* IBRIGHTNESS_9 NT */
	112,	/* IBRIGHTNESS_10 NT */
	112,	/* IBRIGHTNESS_11 NT */
	112,	/* IBRIGHTNESS_12 NT */
	112,	/* IBRIGHTNESS_13 NT */
	112,	/* IBRIGHTNESS_14 NT */
	112,	/* IBRIGHTNESS_15 NT */
	112,	/* IBRIGHTNESS_16 NT */
	112,	/* IBRIGHTNESS_17 NT */
	112,	/* IBRIGHTNESS_19 NT */
	112,	/* IBRIGHTNESS_20 NT */
	112,	/* IBRIGHTNESS_21 NT */
	112,	/* IBRIGHTNESS_22 NT */
	112,	/* IBRIGHTNESS_24 NT */
	112,	/* IBRIGHTNESS_25 NT */
	112,	/* IBRIGHTNESS_27 NT */
	112,	/* IBRIGHTNESS_29 NT */
	112,	/* IBRIGHTNESS_30 NT */
	112,	/* IBRIGHTNESS_32 NT */
	112,	/* IBRIGHTNESS_34 NT */
	112,	/* IBRIGHTNESS_37 NT */
	112,	/* IBRIGHTNESS_39 NT */
	112,	/* IBRIGHTNESS_41 NT */
	112,	/* IBRIGHTNESS_44 NT */
	112,	/* IBRIGHTNESS_47 NT */
	112,	/* IBRIGHTNESS_50 NT */
	112,	/* IBRIGHTNESS_53 NT */
	112,	/* IBRIGHTNESS_56 NT */
	112,	/* IBRIGHTNESS_60 NT */
	112,	/* IBRIGHTNESS_64 NT */
	112,	/* IBRIGHTNESS_68 NT */
	112,	/* IBRIGHTNESS_72 NT */
	112,	/* IBRIGHTNESS_77 NT */
	120,	/* IBRIGHTNESS_82 NT */
	128,	/* IBRIGHTNESS_87 NT */
	134,	/* IBRIGHTNESS_93 NT */
	141,	/* IBRIGHTNESS_98 NT */
	151,	/* IBRIGHTNESS_105 NT */
	161,	/* IBRIGHTNESS_111 NT */
	169,	/* IBRIGHTNESS_119 NT */
	180,	/* IBRIGHTNESS_126 NT */
	190,	/* IBRIGHTNESS_134 NT */
	201,	/* IBRIGHTNESS_143 NT */
	213,	/* IBRIGHTNESS_152 NT */
	228,	/* IBRIGHTNESS_162 NT */
	239,	/* IBRIGHTNESS_172 NT */
	253,	/* IBRIGHTNESS_183 NT */
	253,	/* IBRIGHTNESS_195 NT */
	253,	/* IBRIGHTNESS_207 NT */
	253,	/* IBRIGHTNESS_220 NT */
	253,	/* IBRIGHTNESS_234 NT */
	253,	/* IBRIGHTNESS_249 NT */
	269,	/* IBRIGHTNESS_265 NT */
	284,	/* IBRIGHTNESS_282 NT */
	300,	/* IBRIGHTNESS_300 NT */
	317,	/* IBRIGHTNESS_316 NT */
	333,	/* IBRIGHTNESS_333 NT */
	360,	/* IBRIGHTNESS_360 NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] =
{
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
	gamma_curve_2p15_table, /* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table, /* IBRIGHTNESS_350NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] =
{
	{0x70, 0x77}, /* IBRIGHTNESS_2NT */
	{0x70, 0x6D}, /* IBRIGHTNESS_3NT */
	{0x70, 0x5C}, /* IBRIGHTNESS_4NT */
	{0x70, 0x47}, /* IBRIGHTNESS_5NT */
	{0x70, 0x3C}, /* IBRIGHTNESS_6NT */
	{0x70, 0x2F}, /* IBRIGHTNESS_7NT */
	{0x70, 0x1F}, /* IBRIGHTNESS_8NT */
	{0x70, 0x0E}, /* IBRIGHTNESS_9NT */
	{0x70, 0x01}, /* IBRIGHTNESS_10NT */
	{0x60, 0xF0}, /* IBRIGHTNESS_11NT */
	{0x60, 0xE3}, /* IBRIGHTNESS_12NT */
	{0x60, 0xD9}, /* IBRIGHTNESS_13NT */
	{0x60, 0xC4}, /* IBRIGHTNESS_14NT */
	{0x60, 0xBA}, /* IBRIGHTNESS_15NT */
	{0x60, 0xA4}, /* IBRIGHTNESS_16NT */
	{0x60, 0x9A}, /* IBRIGHTNESS_17NT */
	{0x60, 0x7C}, /* IBRIGHTNESS_19NT */
	{0x60, 0x66}, /* IBRIGHTNESS_20NT */
	{0x60, 0x5C}, /* IBRIGHTNESS_21NT */
	{0x60, 0x45}, /* IBRIGHTNESS_22NT */
	{0x60, 0x27}, /* IBRIGHTNESS_24NT */
	{0x60, 0x1D}, /* IBRIGHTNESS_25NT */
	{0x50, 0xFE}, /* IBRIGHTNESS_27NT */
	{0x50, 0xDF}, /* IBRIGHTNESS_29NT */
	{0x50, 0xCE}, /* IBRIGHTNESS_30NT */
	{0x50, 0xAE}, /* IBRIGHTNESS_32NT */
	{0x50, 0x8F}, /* IBRIGHTNESS_34NT */
	{0x50, 0x62}, /* IBRIGHTNESS_37NT */
	{0x50, 0x44}, /* IBRIGHTNESS_39NT */
	{0x50, 0x24}, /* IBRIGHTNESS_41NT */
	{0x40, 0xF8}, /* IBRIGHTNESS_44NT */
	{0x40, 0xC2}, /* IBRIGHTNESS_47NT */
	{0x40, 0x90}, /* IBRIGHTNESS_50NT */
	{0x40, 0x5F}, /* IBRIGHTNESS_53NT */
	{0x40, 0x2F}, /* IBRIGHTNESS_56NT */
	{0x30, 0xEC}, /* IBRIGHTNESS_60NT */
	{0x30, 0xAB}, /* IBRIGHTNESS_64NT */
	{0x30, 0x6B}, /* IBRIGHTNESS_68NT */
	{0x30, 0x20}, /* IBRIGHTNESS_72NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_77NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_82NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_87NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_93NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_98NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_105NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_111NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_119NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_126NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_134NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_143NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_152NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_162NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_172NT */
	{0x20, 0xDB}, /* IBRIGHTNESS_183NT */
	{0x20, 0x76}, /* IBRIGHTNESS_195NT */
	{0x20, 0x12}, /* IBRIGHTNESS_207NT */
	{0x10, 0xB8}, /* IBRIGHTNESS_220NT */
	{0x10, 0x3E}, /* IBRIGHTNESS_234NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_249NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_265NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_282NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_300NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_316NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_333NT */
	{0x00, 0xC2}, /* IBRIGHTNESS_360NT */
};

static const unsigned char (*paor_cmd)[2] = aor_cmd;

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] =
{	/* VT ~ V255 */
	{0, 44, 42, 40, 35, 31, 24, 15, 9, 0 },		/* IBRIGHTNESS_2NT */
	{0, 43, 39, 37, 32, 28, 22, 13, 8, 0 },		/* IBRIGHTNESS_3NT */
	{0, 38, 36, 32, 27, 24, 19, 11, 7, 0 },		/* IBRIGHTNESS_4NT */
	{0, 35, 31, 26, 22, 19, 16, 9, 6, 0 },		/* IBRIGHTNESS_5NT */
	{0, 30, 29, 27, 22, 19, 15, 9, 6, 0 },		/* IBRIGHTNESS_6NT */
	{0, 30, 27, 24, 20, 17, 14, 8, 6, 0 },		/* IBRIGHTNESS_7NT */
	{0, 29, 25, 22, 18, 16, 13, 8, 5, 0 },		/* IBRIGHTNESS_8NT */
	{0, 23, 24, 20, 16, 14, 12, 7, 5, 0 },		/* IBRIGHTNESS_9NT */
	{0, 24, 22, 19, 15, 13, 11, 7, 5, 0 },		/* IBRIGHTNESS_10NT */
	{0, 24, 21, 18, 14, 13, 10, 7, 5, 0 },		/* IBRIGHTNESS_11NT */
	{0, 21, 20, 17, 13, 12, 10, 6, 5, 0 },		/* IBRIGHTNESS_12NT */
	{0, 21, 19, 16, 12, 11, 10, 6, 5, 0 },		/* IBRIGHTNESS_13NT */
	{0, 21, 18, 15, 12, 11, 9, 6, 4, 0 },		/* IBRIGHTNESS_14NT */
	{0, 19, 18, 15, 12, 10, 9, 6, 4, 0 },		/* IBRIGHTNESS_15NT */
	{0, 17, 16, 14, 11, 9, 8, 6, 4, 0 },		/* IBRIGHTNESS_16NT */
	{0, 17, 16, 13, 10, 9, 8, 5, 4, 0 },		/* IBRIGHTNESS_17NT */
	{0, 17, 15, 12, 9, 8, 7, 5, 4, 0 },		/* IBRIGHTNESS_19NT */
	{0, 18, 14, 11, 9, 8, 7, 5, 4, 0 },		/* IBRIGHTNESS_20NT */
	{0, 16, 14, 11, 9, 7, 7, 5, 4, 0 },		/* IBRIGHTNESS_21NT */
	{0, 16, 13, 10, 8, 7, 7, 5, 4, 0 },		/* IBRIGHTNESS_22NT */
	{0, 13, 13, 9, 8, 7, 7, 5, 4, 0 },		/* IBRIGHTNESS_24NT */
	{0, 13, 13, 10, 8, 7, 6, 5, 4, 0 },		/* IBRIGHTNESS_25NT */
	{0, 11, 13, 9, 7, 6, 6, 4, 4, 0 },		/* IBRIGHTNESS_27NT */
	{0, 12, 12, 9, 7, 6, 6, 4, 4, 0 },		/* IBRIGHTNESS_29NT */
	{0, 14, 11, 8, 6, 5, 6, 4, 4, 0 },		/* IBRIGHTNESS_30NT */
	{0, 13, 11, 8, 6, 5, 6, 4, 4, 0 },		/* IBRIGHTNESS_32NT */
	{0, 13, 10, 8, 6, 5, 5, 4, 4, 0 },		/* IBRIGHTNESS_34NT */
	{0, 13, 9, 7, 5, 4, 5, 4, 3, 0 },		/* IBRIGHTNESS_37NT */
	{0, 12, 9, 7, 4, 4, 5, 3, 3, 0 },		/* IBRIGHTNESS_39NT */
	{0, 8, 9, 6, 4, 4, 5, 3, 3, 0 },		/* IBRIGHTNESS_41NT */
	{0, 8, 8, 6, 4, 4, 4, 3, 3, 0 },		/* IBRIGHTNESS_44NT */
	{0, 7, 8, 6, 4, 4, 4, 3, 3, 0 },		/* IBRIGHTNESS_47NT */
	{0, 7, 7, 5, 3, 3, 4, 3, 3, 0 },		/* IBRIGHTNESS_50NT */
	{0, 7, 7, 5, 3, 3, 4, 3, 3, 0 },		/* IBRIGHTNESS_53NT */
	{0, 6, 6, 5, 3, 3, 4, 3, 3, 0 },		/* IBRIGHTNESS_56NT */
	{0, 8, 6, 5, 3, 3, 4, 3, 3, 0 },		/* IBRIGHTNESS_60NT */
	{0, 9, 5, 4, 2, 2, 3, 2, 3, 0 },		/* IBRIGHTNESS_64NT */
	{0, 8, 5, 4, 2, 2, 3, 3, 3, 0 },		/* IBRIGHTNESS_68NT */
	{0, 6, 5, 4, 2, 2, 3, 2, 3, 0 },		/* IBRIGHTNESS_72NT */
	{0, 7, 4, 3, 1, 2, 3, 2, 3, 0 },		/* IBRIGHTNESS_77NT */
	{0, 4, 4, 3, 2, 2, 3, 2, 2, 0 },		/* IBRIGHTNESS_82NT */
	{0, 4, 4, 3, 2, 2, 2, 2, 2, 0 },		/* IBRIGHTNESS_87NT */
	{0, 4, 3, 3, 2, 1, 3, 3, 3, 0 },		/* IBRIGHTNESS_93NT */
	{0, 5, 4, 3, 2, 2, 3, 3, 3, 0 },		/* IBRIGHTNESS_98NT */
	{0, 5, 4, 3, 2, 2, 3, 3, 3, 0 },		/* IBRIGHTNESS_105NT */
	{0, 3, 4, 2, 2, 2, 3, 3, 1, 0 },		/* IBRIGHTNESS_111NT */
	{0, 3, 4, 3, 2, 1, 3, 3, 1, 0 },		/* IBRIGHTNESS_119NT */
	{0, 3, 3, 2, 2, 1, 3, 2, 1, 0 },		/* IBRIGHTNESS_126NT */
	{0, 5, 3, 2, 2, 2, 3, 4, 1, 0 },		/* IBRIGHTNESS_134NT */
	{0, 4, 3, 2, 2, 1, 3, 3, 1, 0 },		/* IBRIGHTNESS_143NT */
	{0, 4, 3, 2, 2, 2, 4, 4, 1, 0 },		/* IBRIGHTNESS_152NT */
	{0, 5, 2, 1, 1, 2, 3, 3, 1, 0 },		/* IBRIGHTNESS_162NT */
	{0, 4, 2, 2, 2, 2, 2, 3, 3, 0 },		/* IBRIGHTNESS_172NT */
	{0, 6, 2, 1, 2, 2, 3, 3, 2, 0 },		/* IBRIGHTNESS_183NT */
	{0, 4, 2, 1, 2, 2, 3, 3, 2, 0 },		/* IBRIGHTNESS_195NT */
	{0, 3, 2, 1, 2, 2, 2, 3, 1, 0 },		/* IBRIGHTNESS_207NT */
	{0, 5, 1, 1, 1, 1, 2, 2, 1, 0 },		/* IBRIGHTNESS_220NT */
	{0, 3, 1, 1, 1, 1, 2, 2, 1, 0 },		/* IBRIGHTNESS_234NT */
	{0, 0, 1, 0, 0, 0, 1, 1, 0, 0 },		/* IBRIGHTNESS_249NT */
	{0, 1, 1, 1, 1, 1, 1, 1, 0, 0 },		/* IBRIGHTNESS_265NT */
	{0, 2, 0, 0, 1, 2, 1, 1, 1, 0 },		/* IBRIGHTNESS_282NT */
	{0, 2, 0, 0, 0, 0, 0, 1, 1, 0 },		/* IBRIGHTNESS_300NT */
	{0, 3, 0, 0, 0, 1, 0, 1, 0, 0 },		/* IBRIGHTNESS_316NT */
	{0, 3, 0, 0, 0, 0, 0, 0, 0, 0 },		/* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/* IBRIGHTNESS_360NT */
};

static const struct rgb_t offset_color[IBRIGHTNESS_MAX][IV_MAX] =
{	/* VT ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, -6, -4}},{{-12, -8, -9}},{{-11, -3, -4}},{{-21, -10, -11}},{{-11, -5, -8}},{{-4, -1, -3}},{{-3, -1, -3}},{{-10, -7, -10}}},		/*  IBRIGHTNESS_2 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, -4, -5}},{{-17, -10, -11}},{{-19, -9, -9}},{{-18, -8, -11}},{{-12, -6, -10}},{{-4, -2, -4}},{{-2, -1, -2}},{{-1, 0, 0}}},		/*  IBRIGHTNESS_3 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-16, -12, -14}},{{-15, -8, -9}},{{-15, -7, -8}},{{-16, -8, -11}},{{-10, -5, -9}},{{-2, -1, -2}},{{-4, -2, -5}},{{-1, 0, 0}}},		/*  IBRIGHTNESS_4 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-15, -13, -15}},{{-14, -7, -10}},{{-16, -8, -9}},{{-13, -7, -9}},{{-8, -4, -8}},{{-1, 0, 0}},{{-2, -1, -4}},{{-5, -4, -5}}},		/*  IBRIGHTNESS_5 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, -3, -7}},{{-18, -11, -13}},{{-18, -10, -12}},{{-14, -7, -10}},{{-8, -3, -7}},{{-1, -1, -1}},{{-2, 0, -3}},{{-2, -2, -2}}},		/*  IBRIGHTNESS_6 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-14, -8, -12}},{{-13, -7, -9}},{{-14, -8, -9}},{{-13, -5, -10}},{{-8, -4, -8}},{{-1, 0, 0}},{{-4, -2, -5}},{{1, 0, 1}}},		/*  IBRIGHTNESS_7 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-10, -5, -8}},{{-14, -7, -10}},{{-13, -6, -9}},{{-12, -6, -10}},{{-6, -2, -6}},{{-2, -2, -2}},{{-2, 0, -3}},{{0, -1, 0}}},		/*  IBRIGHTNESS_8 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-14, -7, -14}},{{-14, -8, -11}},{{-13, -6, -9}},{{-9, -2, -7}},{{-7, -4, -7}},{{-1, -1, -1}},{{-1, 1, -2}},{{-1, -2, -1}}},		/*  IBRIGHTNESS_9 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-10, -2, -5}},{{-15, -8, -15}},{{-10, -4, -8}},{{-9, -3, -7}},{{-6, -3, -7}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, -1, 0}}},		/*  IBRIGHTNESS_10 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-10, -3, -10}},{{-13, -6, -10}},{{-9, -4, -8}},{{-12, -5, -10}},{{-3, -1, -3}},{{-2, -1, -2}},{{-1, 0, -2}},{{-1, -2, -1}}},		/*  IBRIGHTNESS_11 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, -2, -9}},{{-14, -6, -11}},{{-10, -4, -9}},{{-8, -2, -7}},{{-5, -2, -5}},{{0, 1, 0}},{{-2, -1, -3}},{{-1, -2, -1}}},		/*  IBRIGHTNESS_12 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, -1, -9}},{{-12, -6, -10}},{{-9, -3, -8}},{{-7, -1, -6}},{{-5, -2, -5}},{{-1, 0, -1}},{{-1, 0, -2}},{{-2, -3, -2}}},		/*  IBRIGHTNESS_13 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, -1, -9}},{{-11, -3, -9}},{{-7, -2, -7}},{{-10, -4, -9}},{{-3, -1, -4}},{{-1, 0, 0}},{{-1, 0, -2}},{{-2, -3, -2}}},		/*  IBRIGHTNESS_14 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-10, -3, -11}},{{-10, -3, -9}},{{-11, -5, -10}},{{-7, -1, -6}},{{-3, -1, -4}},{{-2, -1, -1}},{{0, 1, -1}},{{-2, -3, -2}}},		/*  IBRIGHTNESS_15 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 1, -7}},{{-6, -1, -7}},{{-11, -4, -9}},{{-7, -2, -7}},{{-1, 1, -1}},{{-2, -1, -1}},{{0, 1, -1}},{{-2, -3, -2}}},		/*  IBRIGHTNESS_16 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, -1, -11}},{{-7, -2, -8}},{{-7, 0, -6}},{{-5, 0, -4}},{{-3, -1, -4}},{{-1, 0, 0}},{{0, 1, -1}},{{-1, -2, -1}}},		/*  IBRIGHTNESS_17 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 1, -8}},{{-8, -2, -10}},{{-4, 2, -4}},{{-5, 2, -4}},{{0, 1, -2}},{{-1, 0, 0}},{{-1, 0, -2}},{{1, 0, 1}}},		/*  IBRIGHTNESS_19 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -6}},{{-2, 5, -3}},{{-5, 2, -5}},{{-4, 2, -4}},{{-1, 1, -3}},{{0, 1, 1}},{{-3, -2, -4}},{{2, 1, 2}}},		/*  IBRIGHTNESS_20 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -7}},{{-4, 3, -6}},{{-3, 2, -4}},{{-5, 2, -4}},{{-1, 1, -3}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_21 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -7}},{{-4, 3, -6}},{{-3, 2, -4}},{{-4, 2, -4}},{{-1, 1, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_22 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 2, -6}},{{-5, 3, -6}},{{-3, 2, -4}},{{-4, 1, -4}},{{-1, 1, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_24 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -6}},{{-5, 2, -6}},{{-3, 2, -4}},{{-5, 1, -4}},{{-1, 1, -3}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_25 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 2, -6}},{{-5, 2, -6}},{{-5, 1, -4}},{{-4, 1, -4}},{{-1, 1, -3}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_27 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -6}},{{-5, 2, -6}},{{-5, 1, -4}},{{-3, 1, -4}},{{-1, 1, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_29 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -8}},{{-4, 2, -6}},{{-5, 1, -3}},{{-4, 2, -4}},{{0, 1, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_30 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -8}},{{-4, 2, -6}},{{-5, 1, -3}},{{-3, 1, -4}},{{-1, 1, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_32 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -8}},{{-4, 2, -6}},{{-4, 1, -2}},{{-3, 1, -4}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_34 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -8}},{{-4, 2, -5}},{{-4, 1, -2}},{{-2, 1, -4}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_37 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -8}},{{-3, 1, -4}},{{-5, 1, -4}},{{-2, 1, -3}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_39 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -6}},{{-4, 2, -5}},{{-5, 1, -3}},{{-1, 1, -3}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_41 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -8}},{{-4, 2, -5}},{{-4, 1, -2}},{{-3, 1, -3}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_44 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -6}},{{-4, 2, -5}},{{-4, 1, -2}},{{-2, 1, -2}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_47 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -8}},{{-4, 2, -6}},{{-3, 1, -2}},{{-2, 1, -3}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_50 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -7}},{{-2, 2, -4}},{{-3, 1, -2}},{{-2, 1, -3}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_53 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 4, -9}},{{-3, 2, -4}},{{-3, 1, -2}},{{-1, 1, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_56 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -8}},{{-3, 2, -4}},{{-3, 0, -2}},{{-1, 1, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_60 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -8}},{{-2, 2, -4}},{{-3, 0, -1}},{{-1, 1, -2}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_64 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -8}},{{-2, 2, -4}},{{-3, 0, -1}},{{-1, 0, -2}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_68 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -7}},{{-2, 1, -4}},{{-3, 0, -1}},{{-1, 0, -2}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_72 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 3, -8}},{{0, 1, -3}},{{-4, 0, -2}},{{0, 0, -1}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_77 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -8}},{{-3, 1, -4}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_82 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -8}},{{-2, 1, -4}},{{-3, 0, -1}},{{-1, 0, -2}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_87 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -7}},{{-3, 1, -4}},{{-2, 0, -1}},{{-1, 0, -2}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_93 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-9, -1, -10}},{{-3, -1, -4}},{{1, 4, 3}},{{-1, 0, -2}},{{1, 1, 1}},{{0, 1, 0}},{{0, 0, -1}},{{1, 0, 1}}},		/*  IBRIGHTNESS_98 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -6}},{{-3, 1, -3}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_105 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -6}},{{-2, 1, -2}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}},		/*  IBRIGHTNESS_111 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -5}},{{-2, 1, -2}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/*  IBRIGHTNESS_119 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -5}},{{-2, 1, -2}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, -1}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, 2}}},		/*  IBRIGHTNESS_126 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -5}},{{-1, 1, -2}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, 1}}},		/*  IBRIGHTNESS_134 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 1}}},		/*  IBRIGHTNESS_143 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -4}},{{-1, 1, -2}},{{-1, 0, -1}},{{0, 0, -1}},{{1, 0, 0}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, 1}}},		/*  IBRIGHTNESS_152 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 1, -4}},{{-1, 1, -2}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{1, 0, 2}}},		/*  IBRIGHTNESS_162 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -4}},{{-2, 1, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{1, 0, 2}}},		/*  IBRIGHTNESS_172 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -3}},{{3, 4, 2}},{{-2, 0, 0}},{{0, 0, -1}},{{0, 0, 1}},{{0, 1, 1}},{{0, 1, 0}},{{2, 0, 2}}},		/*  IBRIGHTNESS_183 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -4}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/*  IBRIGHTNESS_195 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -3}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, 1}}},		/*  IBRIGHTNESS_207 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -3}},{{-2, 0, -2}},{{0, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 1}}},		/*  IBRIGHTNESS_220 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -3}},{{-1, 0, -1}},{{0, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 1}}},		/*  IBRIGHTNESS_234 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_249 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_265 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_282 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_300 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_316 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_333 NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/*  IBRIGHTNESS_360 NT */
};

#ifdef CONFIG_LCD_HMT

enum {
	HMT_SCAN_SINGLE,
	HMT_SCAN_DUAL,
	HMT_SCAN_MAX
};

enum {
	HMT_IBRIGHTNESS_80NT,
	HMT_IBRIGHTNESS_95NT,
	HMT_IBRIGHTNESS_115NT,
	HMT_IBRIGHTNESS_130NT,
	HMT_IBRIGHTNESS_MAX
};

static const int hmt_index_brightness_table[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] =
{
	{
		80,		/* IBRIGHTNESS_80NT */
		95,		/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		360,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	},{
		80,		/* IBRIGHTNESS_80NT */
		95,		/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		360,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	}
};

static const int hmt_brightness_base_table[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] =
{
	{
		250,	/* IBRIGHTNESS_80NT */
		270,	/* IBRIGHTNESS_95NT */
		260,	/* IBRIGHTNESS_115NT */
		288,	/* IBRIGHTNESS_130NT */
	},{
		250,	/* IBRIGHTNESS_80NT */
		270,	/* IBRIGHTNESS_95NT */
		260,	/* IBRIGHTNESS_115NT */
		288,	/* IBRIGHTNESS_130NT */
	}
};

static const int *hmt_gamma_curve_tables[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] =
{
	{
		gamma_curve_2p20_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_95NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_115NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_130NT */
	},{
		gamma_curve_2p15_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_95NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_115NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_130NT */
	}
};

static const unsigned char hmt_aor_cmd[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX][3] =
{
	{
		{0x05, 0x4C, 0x08}, /* IBRIGHTNESS_80NT */
		{0x05, 0x4C, 0x08}, /* IBRIGHTNESS_95NT */
		{0x04, 0x8A, 0x08}, /* IBRIGHTNESS_115NT */
		{0x04, 0x8A, 0x08}, /* IBRIGHTNESS_130NT */
	},{
		{0x02, 0xC2, 0x08}, /* IBRIGHTNESS_80NT */
		{0x02, 0xC2, 0x08}, /* IBRIGHTNESS_95NT */
		{0x02, 0x5C, 0x08}, /* IBRIGHTNESS_115NT */
		{0x02, 0x5C, 0x08}, /* IBRIGHTNESS_130NT */
	}
};

static const int hmt_offset_gradation[HMT_SCAN_MAX][IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{
		{0, 9, 10, 10, 9, 10, 8, 8, 3, 0},
		{0, 7, 9, 7, 8, 8, 7, 7, 3, 0},
		{0, 7, 8, 7, 7, 5, 5, 6, 3, 0},
		{0, 7, 8, 5, 3, 2, 2, 5, 3, 0},
	},{

		{0, 14, 18, 16, 14, 13, 9, 11, 5, 0},
		{0, 13, 15, 11, 12, 11, 7, 9, 5, 0},
		{0, 12, 13, 11, 9, 9, 7, 9, 5, 0},
		{0, 13, 13, 11, 8, 10, 8, 5, 3, 0},
	}
};

static const struct rgb_t hmt_offset_color[HMT_SCAN_MAX][IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{
		{{{0, 0, 0}},{{0, 0, 0}},{{6, 10, -6}},{{-5, -1, 1}},{{-1, 1, -3}},{{1, 1, -1}},{{-3, -1, -3}},{{0, 0, 0}},{{1, 0, 0}},{{-3, -2, -3}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{2, 6, -7}},{{2, 3, 4}},{{-2, 1, -3}},{{-1, 1, -3}},{{-1, 0, -1}},{{0, 0, 0}},{{2, 1, 1}},{{0, 1, 2}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{1, 4, -6}},{{1, 2, 4}},{{-4, -2, -6}},{{0, 1, -1}},{{0, 1, 0}},{{0, 1, 0}},{{1, 0, 1}},{{-1, -1, 0}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{-9, -3, -12}},{{0, 1, 1}},{{-2, -1, -5}},{{4, 4, 3}},{{2, 4, 3}},{{2, 1, 0}},{{0, 0, 0}},{{0, 0, 1}}},
	},{

		{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-5, -3, 3}},{{-2, 1, -6}},{{-2, 0, -5}},{{-1, 1, -1}},{{0, 1, -1}},{{0, 0, 0}},{{-3, -2, -3}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{1, 5, -12}},{{2, 4, 7}},{{-3, 0, -7}},{{-2, 0, -4}},{{0, 3, 0}},{{0, 0, -1}},{{1, 0, 1}},{{0, 1, 2}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -14}},{{-2, 0, 4}},{{-2, 1, -4}},{{0, 1, -2}},{{0, 2, 0}},{{0, 1, -1}},{{0, 0, 0}},{{-1, -1, -1}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -13}},{{-3, -2, 3}},{{1, 3, -2}},{{1, 2, -1}},{{-1, 0, -1}},{{1, 1, -1}},{{1, 0, 1}},{{-3, -1, -2}}},
	}
};
#endif

#endif /* __DYNAMIC_AID_XXXX_H */
