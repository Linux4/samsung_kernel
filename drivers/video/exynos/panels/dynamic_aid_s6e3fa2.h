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
	IBRIGHTNESS_350NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6300	/* VREG_OUT x 1000 */

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
	115,	/* IBRIGHTNESS_64NT */
	121,	/* IBRIGHTNESS_68NT */
	128,	/* IBRIGHTNESS_72NT */
	136,	/* IBRIGHTNESS_77NT */
	143,	/* IBRIGHTNESS_82NT */
	151,	/* IBRIGHTNESS_87NT */
	161,	/* IBRIGHTNESS_93NT */
	169,	/* IBRIGHTNESS_98NT */
	180,	/* IBRIGHTNESS_105NT */
	189,	/* IBRIGHTNESS_111NT */
	200,	/* IBRIGHTNESS_119NT */
	213,	/* IBRIGHTNESS_126NT */
	224,	/* IBRIGHTNESS_134NT */
	236,	/* IBRIGHTNESS_143NT */
	249,	/* IBRIGHTNESS_152NT */
	245,	/* IBRIGHTNESS_162NT */
	245,	/* IBRIGHTNESS_172NT */
	249,	/* IBRIGHTNESS_183NT */
	249,	/* IBRIGHTNESS_195NT */
	249,	/* IBRIGHTNESS_207NT */
	249,	/* IBRIGHTNESS_220NT */
	249,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
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

static const unsigned char aor_cmd_revF[IBRIGHTNESS_MAX][2] =
{
	{0x07, 0x60}, /* IBRIGHTNESS_2NT */
	{0x07, 0x53}, /* IBRIGHTNESS_3NT */
	{0x07, 0x42}, /* IBRIGHTNESS_4NT */
	{0x07, 0x31}, /* IBRIGHTNESS_5NT */
	{0x07, 0x1D}, /* IBRIGHTNESS_6NT */
	{0x07, 0x0C}, /* IBRIGHTNESS_7NT */
	{0x06, 0xFA}, /* IBRIGHTNESS_8NT */
	{0x06, 0xE9}, /* IBRIGHTNESS_9NT */
	{0x06, 0xD1}, /* IBRIGHTNESS_10NT */
	{0x06, 0xC7}, /* IBRIGHTNESS_11NT */
	{0x06, 0xB5}, /* IBRIGHTNESS_12NT */
	{0x06, 0xA3}, /* IBRIGHTNESS_13NT */
	{0x06, 0x93}, /* IBRIGHTNESS_14NT */
	{0x06, 0x82}, /* IBRIGHTNESS_15NT */
	{0x06, 0x6E}, /* IBRIGHTNESS_16NT */
	{0x06, 0x5D}, /* IBRIGHTNESS_17NT */
	{0x06, 0x3A}, /* IBRIGHTNESS_19NT */
	{0x06, 0x1B}, /* IBRIGHTNESS_20NT */
	{0x06, 0x13}, /* IBRIGHTNESS_21NT */
	{0x06, 0x03}, /* IBRIGHTNESS_22NT */
	{0x05, 0xE0}, /* IBRIGHTNESS_24NT */
	{0x05, 0xCB}, /* IBRIGHTNESS_25NT */
	{0x05, 0xA6}, /* IBRIGHTNESS_27NT */
	{0x05, 0x82}, /* IBRIGHTNESS_29NT */
	{0x05, 0x70}, /* IBRIGHTNESS_30NT */
	{0x05, 0x4A}, /* IBRIGHTNESS_32NT */
	{0x05, 0x22}, /* IBRIGHTNESS_34NT */
	{0x04, 0xEA}, /* IBRIGHTNESS_37NT */
	{0x04, 0xC4}, /* IBRIGHTNESS_39NT */
	{0x04, 0xAA}, /* IBRIGHTNESS_41NT */
	{0x04, 0x6E}, /* IBRIGHTNESS_44NT */
	{0x04, 0x35}, /* IBRIGHTNESS_47NT */
	{0x03, 0xFB}, /* IBRIGHTNESS_50NT */
	{0x03, 0xC2}, /* IBRIGHTNESS_53NT */
	{0x03, 0x89}, /* IBRIGHTNESS_56NT */
	{0x03, 0x3D}, /* IBRIGHTNESS_60NT */
	{0x02, 0xF2}, /* IBRIGHTNESS_64NT */
	{0x02, 0xAD}, /* IBRIGHTNESS_68NT */
	{0x02, 0x54}, /* IBRIGHTNESS_72NT */
	{0x03, 0x06}, /* IBRIGHTNESS_77NT */
	{0x03, 0x06}, /* IBRIGHTNESS_82NT */
	{0x03, 0x06}, /* IBRIGHTNESS_87NT */
	{0x03, 0x06}, /* IBRIGHTNESS_93NT */
	{0x03, 0x06}, /* IBRIGHTNESS_98NT */
	{0x03, 0x06}, /* IBRIGHTNESS_105NT */
	{0x03, 0x06}, /* IBRIGHTNESS_111NT */
	{0x03, 0x06}, /* IBRIGHTNESS_119NT */
	{0x03, 0x06}, /* IBRIGHTNESS_126NT */
	{0x03, 0x06}, /* IBRIGHTNESS_134NT */
	{0x03, 0x06}, /* IBRIGHTNESS_143NT */
	{0x03, 0x06}, /* IBRIGHTNESS_152NT */
	{0x02, 0xC2}, /* IBRIGHTNESS_162NT */
	{0x02, 0x75}, /* IBRIGHTNESS_172NT */
	{0x02, 0x24}, /* IBRIGHTNESS_183NT */
	{0x01, 0xC2}, /* IBRIGHTNESS_195NT */
	{0x01, 0x5A}, /* IBRIGHTNESS_207NT */
	{0x00, 0xF5}, /* IBRIGHTNESS_220NT */
	{0x00, 0x7B}, /* IBRIGHTNESS_234NT */
	{0x00, 0x06}, /* IBRIGHTNESS_249NT */
	{0x00, 0x06}, /* IBRIGHTNESS_265NT */
	{0x00, 0x06}, /* IBRIGHTNESS_282NT */
	{0x00, 0x06}, /* IBRIGHTNESS_300NT */
	{0x00, 0x06}, /* IBRIGHTNESS_316NT */
	{0x00, 0x06}, /* IBRIGHTNESS_333NT */
	{0x00, 0x06}, /* IBRIGHTNESS_350NT */
};

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] =
{
	{0x07, 0x68}, /* IBRIGHTNESS_2NT */
	{0x07, 0x58}, /* IBRIGHTNESS_3NT */
	{0x07, 0x49}, /* IBRIGHTNESS_4NT */
	{0x07, 0x39}, /* IBRIGHTNESS_5NT */
	{0x07, 0x29}, /* IBRIGHTNESS_6NT */
	{0x07, 0x19}, /* IBRIGHTNESS_7NT */
	{0x07, 0x09}, /* IBRIGHTNESS_8NT */
	{0x06, 0xF8}, /* IBRIGHTNESS_9NT */
	{0x06, 0xE7}, /* IBRIGHTNESS_10NT */
	{0x06, 0xD7}, /* IBRIGHTNESS_11NT */
	{0x06, 0xC8}, /* IBRIGHTNESS_12NT */
	{0x06, 0xB7}, /* IBRIGHTNESS_13NT */
	{0x06, 0xA5}, /* IBRIGHTNESS_14NT */
	{0x06, 0x95}, /* IBRIGHTNESS_15NT */
	{0x06, 0x83}, /* IBRIGHTNESS_16NT */
	{0x06, 0x73}, /* IBRIGHTNESS_17NT */
	{0x06, 0x53}, /* IBRIGHTNESS_19NT */
	{0x06, 0x2F}, /* IBRIGHTNESS_20NT */
	{0x06, 0x27}, /* IBRIGHTNESS_21NT */
	{0x06, 0x17}, /* IBRIGHTNESS_22NT */
	{0x05, 0xFF}, /* IBRIGHTNESS_24NT */
	{0x05, 0xEF}, /* IBRIGHTNESS_25NT */
	{0x05, 0xCC}, /* IBRIGHTNESS_27NT */
	{0x05, 0xAB}, /* IBRIGHTNESS_29NT */
	{0x05, 0x98}, /* IBRIGHTNESS_30NT */
	{0x05, 0x76}, /* IBRIGHTNESS_32NT */
	{0x05, 0x53}, /* IBRIGHTNESS_34NT */
	{0x05, 0x1D}, /* IBRIGHTNESS_37NT */
	{0x04, 0xFB}, /* IBRIGHTNESS_39NT */
	{0x04, 0xD2}, /* IBRIGHTNESS_41NT */
	{0x04, 0x9E}, /* IBRIGHTNESS_44NT */
	{0x04, 0x62}, /* IBRIGHTNESS_47NT */
	{0x04, 0x2B}, /* IBRIGHTNESS_50NT */
	{0x03, 0xED}, /* IBRIGHTNESS_53NT */
	{0x03, 0xAD}, /* IBRIGHTNESS_56NT */
	{0x03, 0x56}, /* IBRIGHTNESS_60NT */
	{0x03, 0x29}, /* IBRIGHTNESS_64NT */
	{0x03, 0x29}, /* IBRIGHTNESS_68NT */
	{0x03, 0x29}, /* IBRIGHTNESS_72NT */
	{0x03, 0x29}, /* IBRIGHTNESS_77NT */
	{0x03, 0x29}, /* IBRIGHTNESS_82NT */
	{0x03, 0x29}, /* IBRIGHTNESS_87NT */
	{0x03, 0x29}, /* IBRIGHTNESS_93NT */
	{0x03, 0x29}, /* IBRIGHTNESS_98NT */
	{0x03, 0x29}, /* IBRIGHTNESS_105NT */
	{0x03, 0x29}, /* IBRIGHTNESS_111NT */
	{0x03, 0x29}, /* IBRIGHTNESS_119NT */
	{0x03, 0x29}, /* IBRIGHTNESS_126NT */
	{0x03, 0x29}, /* IBRIGHTNESS_134NT */
	{0x03, 0x29}, /* IBRIGHTNESS_143NT */
	{0x03, 0x29}, /* IBRIGHTNESS_152NT */
	{0x02, 0xBE}, /* IBRIGHTNESS_162NT */
	{0x02, 0x67}, /* IBRIGHTNESS_172NT */
	{0x02, 0x1D}, /* IBRIGHTNESS_183NT */
	{0x01, 0xBD}, /* IBRIGHTNESS_195NT */
	{0x01, 0x4E}, /* IBRIGHTNESS_207NT */
	{0x00, 0xD6}, /* IBRIGHTNESS_220NT */
	{0x00, 0x53}, /* IBRIGHTNESS_234NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_249NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_265NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_282NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_300NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_316NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_333NT */
	{0x00, 0x0E}, /* IBRIGHTNESS_350NT */
};

static const unsigned char (*paor_cmd)[2] = aor_cmd;

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] =
{	/* VT ~ V255 */
	{0, 50, 50, 47, 46, 42, 32, 23, 12, 0}, /* IBRIGHTNESS_2NT */
	{0, 46, 42, 38, 37, 36, 27, 19, 9, 0}, /* IBRIGHTNESS_3NT */
	{0, 39, 37, 34, 32, 30, 24, 17, 8, 0}, /* IBRIGHTNESS_4NT */
	{0, 39, 36, 33, 31, 29, 22, 15, 8, 0}, /* IBRIGHTNESS_5NT */
	{0, 33, 31, 29, 27, 25, 20, 14, 7, 0}, /* IBRIGHTNESS_6NT */
	{0, 32, 29, 27, 25, 23, 18, 12, 7, 0}, /* IBRIGHTNESS_7NT */
	{0, 30, 27, 25, 23, 20, 16, 12, 7, 0}, /* IBRIGHTNESS_8NT */
	{0, 27, 26, 24, 22, 19, 15, 11, 7, 0}, /* IBRIGHTNESS_9NT */
	{0, 26, 25, 23, 21, 18, 15, 11, 6, 0}, /* IBRIGHTNESS_10NT */
	{0, 24, 23, 21, 19, 17, 12, 10, 6, 0}, /* IBRIGHTNESS_11NT */
	{0, 26, 22, 20, 18, 16, 12, 10, 6, 0}, /* IBRIGHTNESS_12NT */
	{0, 22, 21, 19, 17, 15, 11, 9, 5, 0}, /* IBRIGHTNESS_13NT */
	{0, 21, 20, 18, 16, 14, 10, 8, 5, 0}, /* IBRIGHTNESS_14NT */
	{0, 21, 18, 16, 14, 14, 10, 8, 5, 0}, /* IBRIGHTNESS_15NT */
	{0, 19, 17, 16, 14, 13, 10, 8, 5, 0}, /* IBRIGHTNESS_16NT */
	{0, 19, 16, 15, 13, 12, 9, 7, 4, 0}, /* IBRIGHTNESS_17NT */
	{0, 19, 16, 14, 12, 11, 8, 7, 4, 0}, /* IBRIGHTNESS_19NT */
	{0, 17, 15, 13, 11, 10, 7, 6, 4, 0}, /* IBRIGHTNESS_20NT */
	{0, 17, 14, 13, 11, 10, 7, 7, 4, 0}, /* IBRIGHTNESS_21NT */
	{0, 16, 14, 13, 11, 10, 7, 7, 4, 0}, /* IBRIGHTNESS_22NT */
	{0, 17, 14, 12, 10, 10, 7, 7, 4, 0}, /* IBRIGHTNESS_24NT */
	{0, 16, 14, 12, 10, 9, 6, 6, 4, 0}, /* IBRIGHTNESS_25NT */
	{0, 14, 13, 11, 10, 9, 6, 6, 4, 0}, /* IBRIGHTNESS_27NT */
	{0, 13, 12, 10, 9, 8, 6, 6, 4, 0}, /* IBRIGHTNESS_29NT */
	{0, 12, 12, 10, 9, 8, 6, 6, 4, 0}, /* IBRIGHTNESS_30NT */
	{0, 13, 11, 9, 8, 7, 6, 6, 4, 0}, /* IBRIGHTNESS_32NT */
	{0, 11, 10, 8, 7, 7, 5, 5, 4, 0}, /* IBRIGHTNESS_34NT */
	{0, 11, 9, 8, 7, 6, 5, 5, 3, 0}, /* IBRIGHTNESS_37NT */
	{0, 12, 9, 8, 6, 6, 4, 5, 3, 0}, /* IBRIGHTNESS_39NT */
	{0, 11, 9, 8, 6, 6, 4, 5, 3, 0}, /* IBRIGHTNESS_41NT */
	{0, 9, 8, 7, 6, 6, 5, 5, 3, 0}, /* IBRIGHTNESS_44NT */
	{0, 8, 9, 7, 6, 5, 3, 5, 3, 0}, /* IBRIGHTNESS_47NT */
	{0, 10, 8, 6, 5, 5, 3, 5, 3, 0}, /* IBRIGHTNESS_50NT */
	{0, 10, 7, 6, 5, 4, 4, 5, 3, 0}, /* IBRIGHTNESS_53NT */
	{0, 8, 8, 6, 5, 5, 3, 5, 3, 0}, /* IBRIGHTNESS_56NT */
	{0, 9, 6, 5, 5, 4, 3, 5, 3, 0}, /* IBRIGHTNESS_60NT */
	{0, 7, 7, 5, 5, 4, 4, 4, 3, 0}, /* IBRIGHTNESS_64NT */
	{0, 7, 7, 5, 4, 4, 4, 5, 3, 0}, /* IBRIGHTNESS_68NT */
	{0, 7, 7, 5, 5, 4, 4, 3, 3, 0}, /* IBRIGHTNESS_72NT */
	{0, 7, 6, 4, 4, 4, 4, 5, 2, 0}, /* IBRIGHTNESS_77NT */
	{0, 10, 7, 5, 5, 4, 5, 5, 3, 0}, /* IBRIGHTNESS_82NT */
	{0, 5, 6, 4, 5, 4, 4, 5, 3, 0}, /* IBRIGHTNESS_87NT */
	{0, 3, 6, 4, 4, 4, 3, 5, 2, 0}, /* IBRIGHTNESS_93NT */
	{0, 6, 7, 5, 5, 5, 5, 6, 2, 0}, /* IBRIGHTNESS_98NT */
	{0, 5, 6, 4, 4, 4, 4, 4, 2, 0}, /* IBRIGHTNESS_105NT */
	{0, 3, 5, 4, 3, 4, 5, 6, 3, 0}, /* IBRIGHTNESS_111NT */
	{0, 5, 5, 4, 5, 4, 4, 5, 3, 0}, /* IBRIGHTNESS_119NT */
	{0, 4, 5, 4, 4, 4, 5, 5, 2, 0}, /* IBRIGHTNESS_126NT */
	{0, 2, 5, 3, 4, 4, 4, 5, 2, 0}, /* IBRIGHTNESS_134NT */
	{0, 3, 5, 4, 3, 4, 4, 4, 2, 0}, /* IBRIGHTNESS_143NT */
	{0, 2, 5, 3, 4, 4, 5, 5, 2, 0}, /* IBRIGHTNESS_152NT */
	{0, 2, 3, 3, 3, 3, 3, 5, 3, 0}, /* IBRIGHTNESS_162NT */
	{0, 0, 3, 2, 2, 2, 3, 5, 3, 0}, /* IBRIGHTNESS_172NT */
	{0, 4, 3, 2, 3, 3, 4, 4, 1, 0}, /* IBRIGHTNESS_183NT */
	{0, 0, 3, 2, 2, 2, 3, 4, 1, 0}, /* IBRIGHTNESS_195NT */
	{0, 0, 3, 1, 2, 2, 3, 3, 1, 0}, /* IBRIGHTNESS_207NT */
	{0, 0, 2, 1, 2, 1, 2, 2, 1, 0}, /* IBRIGHTNESS_220NT */
	{0, 0, 2, 1, 1, 1, 1, 2, 1, 0}, /* IBRIGHTNESS_234NT */
	{0, 0, 2, 1, 1, 2, 3, 4, 3, 3}, /* IBRIGHTNESS_249NT */
	{0, 0, 2, 1, 1, 1, 1, 3, 2, 1}, /* IBRIGHTNESS_265NT */
	{0, 0, 1, 1, 1, 1, 1, 2, 1, 1}, /* IBRIGHTNESS_282NT */
	{0, 0, 1, 0, 0, 0, 0, 2, 1, 0}, /* IBRIGHTNESS_300NT */
	{0, 1, 1, 1, 1, 1, 2, 2, 2, 2}, /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1}, /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_350NT */
};

static const struct rgb_t offset_color[IBRIGHTNESS_MAX][IV_MAX] =
{	/* VT ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -5}},{{-5, 0, -6}},{{-5, 1, -10}},{{-10, 2, -15}},{{-11, 1, -9}},{{-6, 0, -7}},{{-5, 0, -5}},{{-6, 0, -8}}}, /* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 0, -6}},{{-5, 0, -7}},{{-6, 2, -10}},{{-11, 2, -12}},{{-10, 1, -9}},{{-5, 0, -7}},{{-3, 0, -4}},{{-3, 0, -4}}}, /* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -6}},{{-6, 1, -8}},{{-7, 1, -10}},{{-10, 2, -10}},{{-9, 0, -9}},{{-5, 0, -5}},{{-3, 0, -4}},{{-2, 0, -3}}}, /* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 1, -9}},{{-6, 0, -11}},{{-9, 0, -9}},{{-10, 0, -11}},{{-9, 0, -9}},{{-5, 0, -5}},{{-3, 0, -3}},{{-1, 0, -2}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -9}},{{-5, 1, -8}},{{-9, 0, -10}},{{-9, 1, -9}},{{-7, 0, -7}},{{-5, 0, -5}},{{-2, 0, -3}},{{-1, 0, -1}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -9}},{{-7, 1, -9}},{{-9, 1, -10}},{{-9, 0, -9}},{{-7, 0, -7}},{{-3, 0, -4}},{{-3, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -13}},{{-7, 1, -7}},{{-8, 0, -10}},{{-8, 0, -9}},{{-6, 0, -7}},{{-4, 0, -3}},{{-2, 0, -3}},{{0, 0, 0}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -12}},{{-8, 0, -8}},{{-8, 0, -9}},{{-8, 0, -8}},{{-5, 0, -6}},{{-3, 0, -4}},{{-3, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -13}},{{-7, 0, -7}},{{-8, 0, -9}},{{-8, 0, -8}},{{-5, 0, -6}},{{-4, 0, -4}},{{-2, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -15}},{{-7, 0, -7}},{{-8, 0, -8}},{{-8, 0, -8}},{{-4, 0, -4}},{{-2, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 2, -17}},{{-7, 0, -7}},{{-8, 0, -8}},{{-8, 0, -8}},{{-3, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{1, 0, 1}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -15}},{{-7, 0, -7}},{{-9, 0, -9}},{{-7, 0, -8}},{{-4, 0, -3}},{{-2, 0, -2}},{{-2, 0, -2}},{{1, 0, 1}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -15}},{{-7, 0, -7}},{{-9, 0, -9}},{{-7, 0, -7}},{{-2, 0, -3}},{{-2, 0, -2}},{{-2, 0, -2}},{{1, 0, 1}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -14}},{{-7, 0, -9}},{{-7, 1, -7}},{{-6, 0, -7}},{{-3, 0, -3}},{{-2, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -16}},{{-5, 1, -6}},{{-7, 1, -7}},{{-7, 0, -7}},{{-3, 0, -3}},{{-1, 0, -2}},{{-2, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -16}},{{-7, 1, -7}},{{-7, 1, -7}},{{-6, 0, -7}},{{-2, 0, -2}},{{-2, 0, -3}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 3, -15}},{{-7, 0, -9}},{{-7, 0, -7}},{{-6, 0, -7}},{{-1, 0, 0}},{{-2, 0, -3}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 3, -18}},{{-4, 1, -6}},{{-7, 0, -8}},{{-6, 0, -5}},{{-2, 0, -2}},{{-2, 0, -2}},{{0, 0, -1}},{{0, 0, 0}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -16}},{{-5, 1, -7}},{{-6, 1, -6}},{{-5, 0, -5}},{{-1, 0, -2}},{{-2, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -16}},{{-5, 1, -7}},{{-6, 0, -6}},{{-6, 0, -6}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 3, -17}},{{-6, 0, -8}},{{-7, 0, -7}},{{-5, 0, -5}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-8, 2, -19}},{{-6, 0, -8}},{{-6, 0, -6}},{{-5, 0, -6}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-7, 2, -17}},{{-5, 0, -8}},{{-6, 0, -6}},{{-4, 0, -5}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 3, -15}},{{-4, 0, -7}},{{-5, 0, -6}},{{-5, 0, -6}},{{-1, 0, 0}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 2, -17}},{{-5, 0, -7}},{{-5, 0, -6}},{{-4, 0, -5}},{{-1, 0, 0}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -17}},{{-4, 0, -6}},{{-5, 0, -7}},{{-4, 0, -4}},{{0, 0, 0}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 4, -16}},{{-4, 0, -7}},{{-4, 0, -6}},{{-4, 0, -3}},{{0, 0, 1}},{{-1, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 5, -16}},{{-3, 0, -6}},{{-4, 0, -5}},{{-3, 0, -3}},{{0, 0, 1}},{{-2, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -17}},{{-4, 0, -6}},{{-4, 0, -7}},{{-3, 0, -4}},{{1, 0, 1}},{{-2, 0, -1}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -16}},{{-3, 0, -5}},{{-4, 0, -7}},{{-3, 0, -4}},{{1, 0, 2}},{{-2, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -15}},{{-3, 0, -6}},{{-3, 1, -5}},{{-3, 0, -3}},{{1, 0, 1}},{{-2, 0, -1}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 2, -16}},{{-3, 0, -5}},{{-3, 0, -6}},{{-4, 0, -4}},{{2, 0, 1}},{{-2, 0, -1}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -17}},{{-2, 0, -6}},{{-3, 0, -6}},{{-3, 0, -3}},{{1, 0, 1}},{{-1, 0, -1}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -17}},{{-1, 0, -5}},{{-2, 0, -6}},{{-3, 0, -2}},{{1, 0, 1}},{{-1, 0, -1}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -15}},{{-2, 0, -5}},{{-2, 0, -6}},{{-3, 0, -3}},{{2, 0, 2}},{{-2, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 4, -15}},{{-1, 0, -5}},{{-2, 0, -6}},{{-2, 0, -2}},{{2, 0, 2}},{{-2, 0, -2}},{{-1, 0, -1}},{{1, 0, 1}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -15}},{{-1, 0, -4}},{{-1, 0, -6}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -15}},{{-1, 0, -4}},{{-2, 0, -6}},{{-2, 0, -1}},{{2, 0, 1}},{{-2, 0, -2}},{{-1, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -16}},{{-2, 0, -4}},{{-3, 0, -6}},{{-1, 0, -1}},{{0, 0, 1}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -15}},{{-1, 0, -4}},{{-2, 0, -5}},{{-2, 0, -1}},{{1, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -13}},{{-2, 0, -5}},{{-3, 0, -5}},{{0, 0, -1}},{{0, 0, 0}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -12}},{{-2, 0, -6}},{{-2, 0, -3}},{{-2, 0, -1}},{{0, 0, 1}},{{-1, 0, -2}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -10}},{{-1, 1, -4}},{{-2, 0, -4}},{{-1, 0, -1}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -12}},{{-2, 0, -5}},{{-3, 0, -4}},{{0, 0, -1}},{{0, 0, 1}},{{-2, 0, -2}},{{-1, 0, -1}},{{0, 0, 1}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -12}},{{-2, 0, -4}},{{-3, 0, -4}},{{0, 0, -1}},{{1, 0, 1}},{{-2, 0, -2}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -10}},{{0, 1, -3}},{{-2, 0, -3}},{{0, 0, 1}},{{0, 0, 0}},{{-1, 0, -1}},{{-1, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -10}},{{-1, 0, -5}},{{-3, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, -1}},{{1, 0, 2}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -11}},{{-1, 0, -4}},{{-1, 0, -1}},{{1, 0, 0}},{{-1, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -9}},{{-1, 0, -5}},{{-3, 0, -2}},{{0, 0, -1}},{{-1, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -10}},{{-2, 0, -4}},{{-2, 0, -2}},{{1, 0, 0}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -9}},{{-2, 0, -5}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}},{{1, 0, 0}},{{-1, 0, 1}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -8}},{{-1, 0, -4}},{{-2, 0, -2}},{{1, 0, 1}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -8}},{{0, 0, -4}},{{-2, 0, -2}},{{0, 0, 1}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 1, -10}},{{0, 0, -4}},{{-1, 0, -1}},{{1, 0, 1}},{{0, 0, 1}},{{-1, 0, -1}},{{1, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, -9}},{{0, 0, -4}},{{-2, 0, -1}},{{1, 0, 1}},{{0, 0, 1}},{{0, 0, -2}},{{1, 0, 1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, -7}},{{1, 0, -4}},{{-2, 0, -1}},{{1, 0, 1}},{{-1, 0, 0}},{{-1, 0, -1}},{{1, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, -7}},{{2, 0, -3}},{{-2, 0, -3}},{{2, 0, 2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -6}},{{0, 0, -4}},{{-1, 0, -1}},{{2, 0, 2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_350NT */
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
		350,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	},{
		80,		/* IBRIGHTNESS_80NT */
		95,		/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		350,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
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
