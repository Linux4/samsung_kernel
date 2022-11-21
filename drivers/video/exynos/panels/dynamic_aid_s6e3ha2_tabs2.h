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

/* brightness <-> level table */
static unsigned int brightness_table[356] = {
	IBRIGHTNESS_2NT,
	IBRIGHTNESS_2NT,
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
	IBRIGHTNESS_19NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_21NT,
	IBRIGHTNESS_22NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_27NT,
	IBRIGHTNESS_29NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_32NT,
	IBRIGHTNESS_37NT,
	IBRIGHTNESS_39NT,
	IBRIGHTNESS_41NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_360NT,
	[255 ... 354]	=	IBRIGHTNESS_360NT,
	[355 ... 355]	=	IBRIGHTNESS_500NT
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
	112,	/* IBRIGHTNESS_2NT */
	112,	/* IBRIGHTNESS_3NT */
	112,	/* IBRIGHTNESS_4NT */
	112,	/* IBRIGHTNESS_5NT */
	112,	/* IBRIGHTNESS_6NT */
	112,	/* IBRIGHTNESS_7NT */
	112,	/* IBRIGHTNESS_8NT */
	112,	/* IBRIGHTNESS_9NT */
	112,	/* IBRIGHTNESS_10NT */
	112,	/* IBRIGHTNESS_11NT */
	112,	/* IBRIGHTNESS_12NT */
	112,	/* IBRIGHTNESS_13NT */
	112,	/* IBRIGHTNESS_14NT */
	112,	/* IBRIGHTNESS_15NT */
	112,	/* IBRIGHTNESS_16NT */
	112,	/* IBRIGHTNESS_17NT */
	112,	/* IBRIGHTNESS_19NT */
	112,	/* IBRIGHTNESS_20NT */
	112,	/* IBRIGHTNESS_21NT */
	112,	/* IBRIGHTNESS_22NT */
	112,	/* IBRIGHTNESS_24NT */
	112,	/* IBRIGHTNESS_25NT */
	112,	/* IBRIGHTNESS_27NT */
	112,	/* IBRIGHTNESS_29NT */
	112,	/* IBRIGHTNESS_30NT */
	112,	/* IBRIGHTNESS_32NT */
	112,	/* IBRIGHTNESS_34NT */
	112,	/* IBRIGHTNESS_37NT */
	112,	/* IBRIGHTNESS_39NT */
	112,	/* IBRIGHTNESS_41NT */
	112,	/* IBRIGHTNESS_44NT */
	112,	/* IBRIGHTNESS_47NT */
	112,	/* IBRIGHTNESS_50NT */
	112,	/* IBRIGHTNESS_53NT */
	112,	/* IBRIGHTNESS_56NT */
	112,	/* IBRIGHTNESS_60NT */
	112,	/* IBRIGHTNESS_64NT */
	112,	/* IBRIGHTNESS_68NT */
	112,	/* IBRIGHTNESS_72NT */
	112,	/* IBRIGHTNESS_77NT */
	118,	/* IBRIGHTNESS_82NT */
	126,	/* IBRIGHTNESS_87NT */
	136,	/* IBRIGHTNESS_93NT */
	141,	/* IBRIGHTNESS_98NT */
	152,	/* IBRIGHTNESS_105NT */
	160,	/* IBRIGHTNESS_111NT */
	172,	/* IBRIGHTNESS_119NT */
	180,	/* IBRIGHTNESS_126NT */
	190,	/* IBRIGHTNESS_134NT */
	202,	/* IBRIGHTNESS_143NT */
	216,	/* IBRIGHTNESS_152NT */
	228,	/* IBRIGHTNESS_162NT */
	240,	/* IBRIGHTNESS_172NT */
	253,	/* IBRIGHTNESS_183NT */
	253,	/* IBRIGHTNESS_195NT */
	253,	/* IBRIGHTNESS_207NT */
	253,	/* IBRIGHTNESS_220NT */
	253,	/* IBRIGHTNESS_234NT */
	253,	/* IBRIGHTNESS_249NT */
	268,	/* IBRIGHTNESS_265NT */
	283,	/* IBRIGHTNESS_282NT */
	303,	/* IBRIGHTNESS_300NT */
	319,	/* IBRIGHTNESS_316NT */
	335,	/* IBRIGHTNESS_333NT */
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

static const unsigned char inter_aor_cmd[356][2] = {
	{0xF3, 0x70},	{0xF3, 0x70},	{0xF3, 0x70},	{0xE4, 0x70},	{0xD5, 0x70},
	{0xC7, 0x70},	{0xB7, 0x70},	{0xA8, 0x70},	{0x98, 0x70},	{0x89, 0x70},
	{0x7A, 0x70},	{0x6B, 0x70},	{0x5A, 0x70},	{0x4D, 0x70},	{0x3C, 0x70},
	{0x2C, 0x70},	{0x1D, 0x70},	{0x0D, 0x70},	{0xFD, 0x60},	{0xEE, 0x60},
	{0xDD, 0x60},	{0xCD, 0x60},	{0xBD, 0x60},	{0xA3, 0x60},	{0x8A, 0x60},
	{0x6A, 0x60},	{0x48, 0x60},	{0x38, 0x60},	{0x16, 0x60},	{0xC0, 0x50},
	{0x9E, 0x50},	{0x7A, 0x50},	{0x61, 0x50},	{0x47, 0x50},	{0x2B, 0x50},
	{0x0F, 0x50},	{0xF2, 0x40},	{0xD6, 0x40},	{0xBC, 0x40},	{0xA1, 0x40},
	{0x83, 0x40},	{0x66, 0x40},	{0x4D, 0x40},	{0x33, 0x40},	{0x1A, 0x40},
	{0x01, 0x40},	{0xE9, 0x30},	{0xD0, 0x30},	{0xB8, 0x30},	{0x9F, 0x30},
	{0x87, 0x30},	{0x6E, 0x30},	{0x56, 0x30},	{0x3D, 0x30},	{0x25, 0x30},
	{0x0E, 0x30},	{0xF7, 0x20},	{0xDF, 0x20},	{0x10, 0x30},	{0xFF, 0x20},
	{0xDF, 0x20},	{0x1C, 0x30},	{0xFE, 0x20},	{0xEE, 0x20},	{0xDF, 0x20},
	{0x18, 0x30},	{0x0A, 0x30},	{0xED, 0x20},	{0xDF, 0x20},	{0x15, 0x30},
	{0xFA, 0x20},	{0xED, 0x20},	{0xDF, 0x20},	{0x2B, 0x30},	{0x12, 0x30},
	{0x05, 0x30},	{0xEC, 0x20},	{0xDF, 0x20},	{0x0F, 0x30},	{0x03, 0x30},
	{0xEB, 0x20},	{0xDF, 0x20},	{0x2D, 0x30},	{0x17, 0x30},	{0x0C, 0x30},
	{0x00, 0x30},	{0xEA, 0x20},	{0xDF, 0x20},	{0x1E, 0x30},	{0x09, 0x30},
	{0xFF, 0x20},	{0xEA, 0x20},	{0xDF, 0x20},	{0x24, 0x30},	{0x11, 0x30},
	{0x07, 0x30},	{0xFD, 0x20},	{0xE9, 0x20},	{0xDF, 0x20},	{0x29, 0x30},
	{0x17, 0x30},	{0x0D, 0x30},	{0x04, 0x30},	{0xFB, 0x20},	{0xE8, 0x20},
	{0xDF, 0x20},	{0x1C, 0x30},	{0x13, 0x30},	{0x02, 0x30},	{0xF9, 0x20},
	{0xE8, 0x20},	{0xDF, 0x20},	{0x29, 0x30},	{0x18, 0x30},	{0x10, 0x30},
	{0x08, 0x30},	{0x00, 0x30},	{0xEF, 0x20},	{0xE7, 0x20},	{0xDF, 0x20},
	{0x25, 0x30},	{0x15, 0x30},	{0x0D, 0x30},	{0xFE, 0x20},	{0xF6, 0x20},
	{0xE7, 0x20},	{0xDF, 0x20},	{0xDF, 0x20},	{0xDF, 0x20},	{0xDF, 0x20},
	{0xDF, 0x20},	{0xDF, 0x20},	{0xDF, 0x20},	{0xDF, 0x20},	{0xDF, 0x20},
	{0xD2, 0x20},	{0xC6, 0x20},	{0xB9, 0x20},	{0xAD, 0x20},	{0xA0, 0x20},
	{0x94, 0x20},	{0x87, 0x20},	{0x7B, 0x20},	{0x6E, 0x20},	{0x60, 0x20},
	{0x52, 0x20},	{0x44, 0x20},	{0x36, 0x20},	{0x28, 0x20},	{0x1A, 0x20},
	{0x0C, 0x20},	{0xFE, 0x10},	{0xF1, 0x10},	{0xE5, 0x10},	{0xD8, 0x10},
	{0xCC, 0x10},	{0xBF, 0x10},	{0xB3, 0x10},	{0xA7, 0x10},	{0x9A, 0x10},
	{0x8E, 0x10},	{0x81, 0x10},	{0x73, 0x10},	{0x65, 0x10},	{0x57, 0x10},
	{0x49, 0x10},	{0x3C, 0x10},	{0x2E, 0x10},	{0x20, 0x10},	{0x12, 0x10},
	{0x04, 0x10},	{0xF6, 0x00},	{0xE9, 0x00},	{0xDC, 0x00},	{0xCF, 0x00},
	{0xC2, 0x00},	{0xB5, 0x00},	{0xA8, 0x00},	{0x9B, 0x00},	{0x8E, 0x00},
	{0x81, 0x00},	{0x74, 0x00},	{0x67, 0x00},	{0xD6, 0x00},	{0xC7, 0x00},
	{0xC0, 0x00},	{0xB8, 0x00},	{0xAA, 0x00},	{0xA2, 0x00},	{0x9B, 0x00},
	{0x8C, 0x00},	{0x85, 0x00},	{0x7D, 0x00},	{0x6E, 0x00},	{0x67, 0x00},
	{0xD6, 0x00},	{0xC8, 0x00},	{0xC1, 0x00},	{0xB3, 0x00},	{0xAD, 0x00},
	{0x9F, 0x00},	{0x98, 0x00},	{0x91, 0x00},	{0x83, 0x00},	{0x7C, 0x00},
	{0x6E, 0x00},	{0x67, 0x00},	{0xD6, 0x00},	{0xC9, 0x00},	{0xC3, 0x00},
	{0xB5, 0x00},	{0xAF, 0x00},	{0xA8, 0x00},	{0x9B, 0x00},	{0x95, 0x00},
	{0x8E, 0x00},	{0x81, 0x00},	{0x7B, 0x00},	{0x6E, 0x00},	{0x67, 0x00},
	{0xC4, 0x00},	{0xB8, 0x00},	{0xB1, 0x00},	{0xAB, 0x00},	{0x9F, 0x00},
	{0x99, 0x00},	{0x92, 0x00},	{0x86, 0x00},	{0x80, 0x00},	{0x7A, 0x00},
	{0x6D, 0x00},	{0x67, 0x00},	{0xC5, 0x00},	{0xB9, 0x00},	{0xB4, 0x00},
	{0xA8, 0x00},	{0xA2, 0x00},	{0x96, 0x00},	{0x90, 0x00},	{0x8A, 0x00},
	{0x7F, 0x00},	{0x79, 0x00},	{0x6D, 0x00},	{0x67, 0x00},	{0xEF, 0x00},
	{0xDF, 0x00},	{0xD4, 0x00},	{0xC4, 0x00},	{0xB9, 0x00},	{0xA8, 0x00},
	{0x9D, 0x00},	{0x8D, 0x00},	{0x82, 0x00},	{0x72, 0x00},	{0x67, 0x00},
	[255 ... 355] = {0x67, 0x00}
};

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0xF3, 0x70},	/* IBRIGHTNESS_2NT */
	{0xE4, 0x70},	/* IBRIGHTNESS_3NT */
	{0xD5, 0x70},	/* IBRIGHTNESS_4NT */
	{0xC7, 0x70},	/* IBRIGHTNESS_5NT */
	{0xB7, 0x70},	/* IBRIGHTNESS_6NT */
	{0xA8, 0x70},	/* IBRIGHTNESS_7NT */
	{0x98, 0x70},	/* IBRIGHTNESS_8NT */
	{0x89, 0x70},	/* IBRIGHTNESS_9NT */
	{0x7A, 0x70},	/* IBRIGHTNESS_10NT */
	{0x6B, 0x70},	/* IBRIGHTNESS_11NT */
	{0x5A, 0x70},	/* IBRIGHTNESS_12NT */
	{0x4D, 0x70},	/* IBRIGHTNESS_13NT */
	{0x3C, 0x70},	/* IBRIGHTNESS_14NT */
	{0x2C, 0x70},	/* IBRIGHTNESS_15NT */
	{0x1D, 0x70},	/* IBRIGHTNESS_16NT */
	{0x0D, 0x70},	/* IBRIGHTNESS_17NT */
	{0xEE, 0x60},	/* IBRIGHTNESS_19NT */
	{0xDD, 0x60},	/* IBRIGHTNESS_20NT */
	{0xCD, 0x60},	/* IBRIGHTNESS_21NT */
	{0xBD, 0x60},	/* IBRIGHTNESS_22NT */
	{0x9C, 0x60},	/* IBRIGHTNESS_24NT */
	{0x8A, 0x60},	/* IBRIGHTNESS_25NT */
	{0x6A, 0x60},	/* IBRIGHTNESS_27NT */
	{0x48, 0x60},	/* IBRIGHTNESS_29NT */
	{0x38, 0x60},	/* IBRIGHTNESS_30NT */
	{0x16, 0x60},	/* IBRIGHTNESS_32NT */
	{0xF5, 0x50},	/* IBRIGHTNESS_34NT */
	{0xC0, 0x50},	/* IBRIGHTNESS_37NT */
	{0x9E, 0x50},	/* IBRIGHTNESS_39NT */
	{0x7A, 0x50},	/* IBRIGHTNESS_41NT */
	{0x47, 0x50},	/* IBRIGHTNESS_44NT */
	{0x0F, 0x50},	/* IBRIGHTNESS_47NT */
	{0xD6, 0x40},	/* IBRIGHTNESS_50NT */
	{0xA1, 0x40},	/* IBRIGHTNESS_53NT */
	{0x66, 0x40},	/* IBRIGHTNESS_56NT */
	{0x1A, 0x40},	/* IBRIGHTNESS_60NT */
	{0xD0, 0x30},	/* IBRIGHTNESS_64NT */
	{0x87, 0x30},	/* IBRIGHTNESS_68NT */
	{0x3D, 0x30},	/* IBRIGHTNESS_72NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_77NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_82NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_87NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_93NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_98NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_105NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_111NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_119NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_126NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_134NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_143NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_152NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_162NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_172NT */
	{0xDF, 0x20},	/* IBRIGHTNESS_183NT */
	{0x6E, 0x20},	/* IBRIGHTNESS_195NT */
	{0xFE, 0x10},	/* IBRIGHTNESS_207NT */
	{0x81, 0x10},	/* IBRIGHTNESS_220NT */
	{0xF6, 0x00},	/* IBRIGHTNESS_234NT */
	{0x67, 0x00},	/* IBRIGHTNESS_249NT */
	{0x67, 0x00},	/* IBRIGHTNESS_265NT */
	{0x67, 0x00},	/* IBRIGHTNESS_282NT */
	{0x67, 0x00},	/* IBRIGHTNESS_300NT */
	{0x67, 0x00},	/* IBRIGHTNESS_316NT */
	{0x67, 0x00},	/* IBRIGHTNESS_333NT */
	{0x67, 0x00},	/* IBRIGHTNESS_360NT */
	{0x67, 0x00}	/* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 41, 42, 37, 32, 30, 22, 14, 9, 0},
	{0, 36, 36, 32, 27, 24, 19, 12, 7, 0},
	{0, 29, 32, 27, 22, 19, 15, 10, 6, 0},
	{0, 26, 28, 24, 20, 17, 13, 9, 6, 0},
	{0, 26, 25, 21, 17, 15, 13, 8, 5, 0},
	{0, 19, 24, 20, 15, 13, 11, 7, 5, 0},
	{0, 20, 22, 18, 14, 13, 10, 6, 5, 0},
	{0, 21, 20, 16, 12, 11, 9, 6, 5, 0},
	{0, 21, 20, 16, 12, 11, 9, 6, 5, 0},
	{0, 21, 18, 15, 11, 10, 9, 6, 5, 0},
	{0, 20, 18, 14, 11, 10, 8, 6, 4, 0},
	{0, 19, 16, 13, 10, 9, 7, 6, 4, 0},
	{0, 18, 15, 12, 9, 8, 7, 5, 4, 0},
	{0, 18, 15, 12, 9, 8, 6, 5, 4, 0},
	{0, 16, 15, 11, 8, 8, 6, 5, 4, 0},
	{0, 17, 14, 11, 8, 7, 5, 5, 4, 0},
	{0, 15, 13, 10, 7, 6, 5, 5, 4, 0},
	{0, 15, 13, 9, 7, 6, 5, 4, 4, 0},
	{0, 15, 13, 9, 7, 6, 5, 4, 4, 0},
	{0, 15, 12, 9, 7, 6, 5, 4, 4, 0},
	{0, 14, 12, 8, 7, 6, 5, 4, 4, 0},
	{0, 14, 12, 8, 6, 6, 5, 4, 4, 0},
	{0, 14, 12, 8, 6, 5, 5, 4, 4, 0},
	{0, 13, 11, 8, 5, 5, 4, 3, 4, 0},
	{0, 13, 11, 8, 5, 5, 4, 3, 4, 0},
	{0, 13, 10, 7, 5, 5, 4, 3, 4, 0},
	{0, 12, 10, 7, 4, 5, 4, 3, 3, 0},
	{0, 12, 10, 7, 5, 4, 4, 3, 3, 0},
	{0, 12, 9, 7, 4, 4, 3, 3, 3, 0},
	{0, 11, 9, 6, 4, 4, 4, 3, 3, 0},
	{0, 10, 8, 6, 4, 4, 4, 3, 3, 0},
	{0, 10, 8, 5, 3, 4, 4, 3, 3, 0},
	{0, 10, 8, 5, 3, 3, 4, 3, 3, 0},
	{0, 9, 8, 5, 3, 4, 4, 2, 3, 0},
	{0, 9, 8, 5, 3, 4, 4, 2, 3, 0},
	{0, 9, 7, 5, 3, 3, 3, 2, 3, 0},
	{0, 8, 6, 4, 3, 3, 3, 2, 3, 0},
	{0, 8, 6, 4, 2, 3, 2, 2, 3, 0},
	{0, 7, 5, 4, 2, 2, 2, 2, 3, 0},
	{0, 7, 5, 3, 2, 2, 2, 2, 3, 0},
	{0, 7, 5, 3, 2, 2, 1, 2, 2, 0},
	{0, 7, 5, 3, 2, 1, 2, 2, 1, 0},
	{0, 7, 4, 2, 1, 1, 2, 2, 2, 0},
	{0, 7, 4, 3, 2, 1, 2, 3, 2, 0},
	{0, 7, 5, 4, 2, 2, 2, 3, 2, 0},
	{0, 7, 5, 3, 2, 1, 2, 2, 1, 0},
	{0, 7, 5, 3, 3, 2, 2, 4, 1, 0},
	{0, 7, 4, 2, 2, 2, 2, 3, 1, 0},
	{0, 7, 4, 3, 2, 1, 2, 4, 1, 0},
	{0, 6, 4, 3, 2, 2, 2, 3, 1, 0},
	{0, 6, 4, 3, 2, 2, 2, 4, 1, 0},
	{0, 6, 3, 2, 2, 2, 3, 3, 1, 0},
	{0, 5, 3, 2, 2, 2, 3, 4, 1, 0},
	{0, 5, 3, 2, 2, 2, 2, 3, 1, 0},
	{0, 5, 3, 1, 2, 2, 2, 3, 1, 0},
	{0, 4, 3, 1, 1, 1, 1, 2, 1, 0},
	{0, 3, 2, 1, 1, 1, 1, 2, 0, 0},
	{0, 3, 2, 0, 1, 1, 1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 1, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, -1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, -3, 2, -4, -3, 2, -5, -4, 3, -7, -9, 3, -9, -6, 4, -7, -3, 1, -4, -4, 0, -4, -7, 0, -1},
	{0, 0, 0, 0, 0, 0, -3, 2, -6, -3, 2, -6, -7, 2, -7, -7, 3, -9, -7, 3, -8, -3, 0, -4, -4, 0, -4, -5, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 0, -8, -5, 1, -8, -7, 2, -8, -7, 3, -9, -8, 2, -8, -3, 0, -4, -2, 0, -2, -4, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -6, -5, 3, -7, -6, 2, -7, -7, 3, -9, -7, 3, -7, -3, 0, -4, -2, 0, -3, -3, 0, 2},
	{0, 0, 0, 0, 0, 0, -3, 3, -8, -3, 3, -6, -5, 3, -7, -7, 3, -9, -6, 2, -6, -3, 0, -4, -2, 0, -2, -2, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 2, -10, -5, 1, -7, -5, 3, -6, -8, 3, -9, -6, 2, -6, -2, 0, -3, -2, 0, -2, -2, 0, 2},
	{0, 0, 0, 0, 0, 0, -2, 3, -8, -6, 2, -8, -6, 3, -7, -6, 2, -8, -6, 2, -6, -2, 0, -2, -2, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, 0, 3, -7, -8, 2, -9, -5, 3, -7, -5, 3, -6, -5, 2, -5, -2, 0, -2, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -2, 3, -8, -6, 2, -8, -5, 3, -7, -6, 3, -7, -5, 1, -5, -2, 0, -2, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -2, 3, -11, -7, 2, -9, -5, 3, -7, -7, 2, -8, -5, 1, -5, -1, 0, -1, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 2, -10, -6, 3, -7, -4, 3, -6, -6, 2, -7, -5, 1, -5, -1, 0, -1, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -4, 4, -13, -6, 3, -9, -8, 2, -8, -7, 2, -7, -4, 1, -4, -1, 0, -1, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -2, 4, -11, -6, 3, -9, -7, 2, -7, -6, 2, -7, -4, 1, -4, -1, 0, -1, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 4, -12, -6, 3, -8, -5, 3, -6, -6, 2, -7, -4, 1, -4, -1, 0, -1, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -4, 2, -11, -7, 3, -9, -4, 3, -5, -6, 2, -7, -3, 1, -3, -1, 0, -1, -1, 0, -2, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 3, -14, -6, 3, -7, -5, 3, -6, -6, 2, -6, -4, 1, -4, 0, 0, -1, -1, 0, -1, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -4, 4, -14, -5, 3, -6, -4, 3, -5, -6, 2, -6, -3, 1, -3, 0, 0, -1, -1, 0, -1, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 4, -12, -5, 4, -8, -6, 2, -7, -6, 2, -6, -3, 1, -3, 0, 0, -1, -1, 0, -1, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -2, 4, -11, -4, 4, -7, -6, 2, -6, -6, 1, -7, -3, 1, -3, -1, 0, -1, -1, 0, -1, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -4, 4, -15, -5, 4, -7, -5, 2, -6, -6, 1, -6, -3, 1, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 3, -12, -6, 4, -9, -5, 2, -5, -6, 1, -6, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 3, -12, -6, 3, -9, -5, 2, -6, -6, 1, -6, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 3, -11, -6, 3, -8, -4, 2, -5, -6, 1, -6, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -5, 3, -15, -4, 3, -6, -4, 2, -5, -5, 1, -5, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -5, 3, -16, -4, 3, -7, -4, 2, -5, -5, 1, -5, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -5, 5, -15, -5, 3, -8, -4, 2, -4, -4, 1, -5, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -5, 4, -14, -5, 2, -8, -4, 2, -4, -3, 1, -4, -3, 0, -3, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -6, 4, -15, -6, 2, -9, -4, 1, -4, -4, 1, -5, -2, 0, -2, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -5, 5, -17, -5, 1, -8, -5, 1, -5, -3, 1, -4, -2, 0, -2, 0, 0, 0, -1, 0, -2, 0, 0, 3},
	{0, 0, 0, 0, 0, 0, -3, 4, -15, -7, 2, -9, -3, 2, -4, -3, 1, -3, -2, 0, -3, 0, 0, 0, -1, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 5, -16, -3, 3, -5, -3, 2, -3, -3, 1, -4, -2, 0, -2, 0, 0, 0, -1, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -3, 4, -13, -4, 3, -7, -3, 2, -4, -3, 1, -3, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -5, 3, -15, -3, 3, -6, -2, 2, -3, -4, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -5, 3, -14, -4, 2, -7, -2, 2, -3, -4, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -5, 3, -14, -3, 2, -6, -2, 2, -3, -4, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 5, -15, -3, 2, -6, -3, 1, -3, -3, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 5, -17, -4, 2, -7, -3, 1, -3, -2, 0, -3, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 5, -15, -3, 2, -6, -3, 1, -3, -1, 0, -3, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -5, 6, -18, -2, 2, -5, -4, 1, -4, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, -1, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -15, -3, 2, -6, -3, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -6, 5, -17, -2, 2, -4, -2, 1, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -6, 5, -16, -2, 1, -5, -2, 1, -2, -2, 0, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 2},
	{0, 0, 0, 0, 0, 0, -6, 5, -16, -2, 1, -5, -2, 1, -2, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 2},
	{0, 0, 0, 0, 0, 0, -4, 5, -15, -3, 1, -5, -1, 1, -1, 0, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 5, -14, -2, 1, -4, -3, 0, -3, 0, 0, -1, -2, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 5, -12, -2, 1, -4, -3, 0, -3, -2, 0, -2, -1, 0, -1, 1, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -14, -2, 1, -4, -2, 0, -3, -2, 0, -2, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -11, -2, 1, -4, -2, 0, -2, -1, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -13, -3, 1, -3, -2, 0, -2, 0, 0, -1, -2, 0, -2, 0, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 4, -13, -2, 1, -3, -3, 0, -3, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 2},
	{0, 0, 0, 0, 0, 0, -6, 4, -13, -1, 1, -2, -2, 0, -3, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 4, -12, -3, 1, -3, -1, 0, -2, -1, 0, -1, -1, 0, -1, 1, 0, -1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 4, -10, -3, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 4, -10, -3, 0, -3, -1, 0, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 3, -10, -3, 0, -3, -1, 0, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -8, -3, 0, -3, 0, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -9, -3, 0, -3, 0, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 2, -7, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
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
