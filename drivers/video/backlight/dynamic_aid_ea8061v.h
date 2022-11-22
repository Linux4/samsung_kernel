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

#define VREG_OUT_X1000		6000	/* VREG_OUT x 1000 */

static const int index_voltage_table[IV_MAX] =
{
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,		/* IV_151 */
	203,		/* IV_203 */
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
	9,	/* IBRIGHTNESS_8NT */
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
	0x00, 0x00, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100
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
	110,	/* IBRIGHTNESS_64NT */
	116,	/* IBRIGHTNESS_68NT */
	123,	/* IBRIGHTNESS_72NT */
	131,	/* IBRIGHTNESS_77NT */
	139,	/* IBRIGHTNESS_82NT */
	147,	/* IBRIGHTNESS_87NT */
	157,	/* IBRIGHTNESS_93NT */
	165,	/* IBRIGHTNESS_98NT */
	176,	/* IBRIGHTNESS_105NT */
	185,	/* IBRIGHTNESS_111NT */
	197,	/* IBRIGHTNESS_119NT */
	207,	/* IBRIGHTNESS_126NT */
	219,	/* IBRIGHTNESS_134NT */
	232,	/* IBRIGHTNESS_143NT */
	245,	/* IBRIGHTNESS_152NT */
	249,	/* IBRIGHTNESS_162NT */
	249,	/* IBRIGHTNESS_172NT */
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
	gamma_curve_2p15_table,	/* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table,	/* IBRIGHTNESS_350NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd_RevE[IBRIGHTNESS_MAX][2] =
{
	{0x04, 0xFC}, /* IBRIGHTNESS_2NT */
	{0x04, 0xF0}, /* IBRIGHTNESS_3NT */
	{0x04, 0xE5}, /* IBRIGHTNESS_4NT */
	{0x04, 0xD9}, /* IBRIGHTNESS_5NT */
	{0x04, 0xCD}, /* IBRIGHTNESS_6NT */
	{0x04, 0xC1}, /* IBRIGHTNESS_7NT */
	{0x04, 0xB5}, /* IBRIGHTNESS_8NT */
	{0x04, 0xA8}, /* IBRIGHTNESS_9NT */
	{0x04, 0x9C}, /* IBRIGHTNESS_10NT */
	{0x04, 0x91}, /* IBRIGHTNESS_11NT */
	{0x04, 0x85}, /* IBRIGHTNESS_12NT */
	{0x04, 0x7A}, /* IBRIGHTNESS_13NT */
	{0x04, 0x6F}, /* IBRIGHTNESS_14NT */
	{0x04, 0x60}, /* IBRIGHTNESS_15NT */
	{0x04, 0x54}, /* IBRIGHTNESS_16NT */
	{0x04, 0x48}, /* IBRIGHTNESS_17NT */
	{0x04, 0x30}, /* IBRIGHTNESS_19NT */
	{0x04, 0x23}, /* IBRIGHTNESS_20NT */
	{0x04, 0x1E}, /* IBRIGHTNESS_21NT */
	{0x04, 0x17}, /* IBRIGHTNESS_22NT */
	{0x03, 0xF4}, /* IBRIGHTNESS_24NT */
	{0x03, 0xE8}, /* IBRIGHTNESS_25NT */
	{0x03, 0xD1}, /* IBRIGHTNESS_27NT */
	{0x03, 0xB9}, /* IBRIGHTNESS_29NT */
	{0x03, 0xAC}, /* IBRIGHTNESS_30NT */
	{0x03, 0x93}, /* IBRIGHTNESS_32NT */
	{0x03, 0x7A}, /* IBRIGHTNESS_34NT */
	{0x03, 0x55}, /* IBRIGHTNESS_37NT */
	{0x03, 0x3C}, /* IBRIGHTNESS_39NT */
	{0x03, 0x23}, /* IBRIGHTNESS_41NT */
	{0x02, 0xFF}, /* IBRIGHTNESS_44NT */
	{0x02, 0xDA}, /* IBRIGHTNESS_47NT */
	{0x02, 0xB6}, /* IBRIGHTNESS_50NT */
	{0x02, 0x91}, /* IBRIGHTNESS_53NT */
	{0x02, 0x6D}, /* IBRIGHTNESS_56NT */
	{0x02, 0x3C}, /* IBRIGHTNESS_60NT */
	{0x02, 0x0B}, /* IBRIGHTNESS_64NT */
	{0x02, 0x09}, /* IBRIGHTNESS_68NT */
	{0x02, 0x09}, /* IBRIGHTNESS_72NT */
	{0x02, 0x09}, /* IBRIGHTNESS_77NT */
	{0x02, 0x09}, /* IBRIGHTNESS_82NT */
	{0x02, 0x09}, /* IBRIGHTNESS_87NT */
	{0x02, 0x09}, /* IBRIGHTNESS_93NT */
	{0x02, 0x09}, /* IBRIGHTNESS_98NT */
	{0x02, 0x09}, /* IBRIGHTNESS_105NT */
	{0x02, 0x09}, /* IBRIGHTNESS_111NT */
	{0x02, 0x09}, /* IBRIGHTNESS_119NT */
	{0x02, 0x09}, /* IBRIGHTNESS_126NT */
	{0x02, 0x09}, /* IBRIGHTNESS_134NT */
	{0x02, 0x09}, /* IBRIGHTNESS_143NT */
	{0x02, 0x09}, /* IBRIGHTNESS_152NT */
	{0x01, 0xE7}, /* IBRIGHTNESS_162NT */
	{0x01, 0xA5}, /* IBRIGHTNESS_172NT */
	{0x01, 0x6D}, /* IBRIGHTNESS_183NT */
	{0x01, 0x28}, /* IBRIGHTNESS_195NT */
	{0x00, 0xE2}, /* IBRIGHTNESS_207NT */
	{0x00, 0x98}, /* IBRIGHTNESS_220NT */
	{0x00, 0x46}, /* IBRIGHTNESS_234NT */
	{0x00, 0x08}, /* IBRIGHTNESS_249NT */
	{0x00, 0x08}, /* IBRIGHTNESS_265NT */
	{0x00, 0x08}, /* IBRIGHTNESS_282NT */
	{0x00, 0x08}, /* IBRIGHTNESS_300NT */
	{0x00, 0x08}, /* IBRIGHTNESS_316NT */
	{0x00, 0x08}, /* IBRIGHTNESS_333NT */
	{0x00, 0x08}, /* IBRIGHTNESS_350NT */
};

static const unsigned char aor_cmd_RevD[IBRIGHTNESS_MAX][2] =
{
	{0x04, 0xFE}, /* IBRIGHTNESS_2NT */
	{0x04, 0xF3}, /* IBRIGHTNESS_3NT */
	{0x04, 0xE9}, /* IBRIGHTNESS_4NT */
	{0x04, 0xDF}, /* IBRIGHTNESS_5NT */
	{0x04, 0xD2}, /* IBRIGHTNESS_6NT */
	{0x04, 0xC7}, /* IBRIGHTNESS_7NT */
	{0x04, 0xBD}, /* IBRIGHTNESS_8NT */
	{0x04, 0xB3}, /* IBRIGHTNESS_9NT */
	{0x04, 0xA8}, /* IBRIGHTNESS_10NT */
	{0x04, 0x9D}, /* IBRIGHTNESS_11NT */
	{0x04, 0x91}, /* IBRIGHTNESS_12NT */
	{0x04, 0x86}, /* IBRIGHTNESS_13NT */
	{0x04, 0x7A}, /* IBRIGHTNESS_14NT */
	{0x04, 0x6F}, /* IBRIGHTNESS_15NT */
	{0x04, 0x64}, /* IBRIGHTNESS_16NT */
	{0x04, 0x59}, /* IBRIGHTNESS_17NT */
	{0x04, 0x3F}, /* IBRIGHTNESS_19NT */
	{0x04, 0x33}, /* IBRIGHTNESS_20NT */
	{0x04, 0x27}, /* IBRIGHTNESS_21NT */
	{0x04, 0x1B}, /* IBRIGHTNESS_22NT */
	{0x04, 0x04}, /* IBRIGHTNESS_24NT */
	{0x03, 0xF9}, /* IBRIGHTNESS_25NT */
	{0x03, 0xE3}, /* IBRIGHTNESS_27NT */
	{0x03, 0xCC}, /* IBRIGHTNESS_29NT */
	{0x03, 0xC0}, /* IBRIGHTNESS_30NT */
	{0x03, 0xA8}, /* IBRIGHTNESS_32NT */
	{0x03, 0x90}, /* IBRIGHTNESS_34NT */
	{0x03, 0x6C}, /* IBRIGHTNESS_37NT */
	{0x03, 0x54}, /* IBRIGHTNESS_39NT */
	{0x03, 0x3D}, /* IBRIGHTNESS_41NT */
	{0x03, 0x1A}, /* IBRIGHTNESS_44NT */
	{0x02, 0xF6}, /* IBRIGHTNESS_47NT */
	{0x02, 0xD2}, /* IBRIGHTNESS_50NT */
	{0x02, 0xAE}, /* IBRIGHTNESS_53NT */
	{0x02, 0x8B}, /* IBRIGHTNESS_56NT */
	{0x02, 0x5B}, /* IBRIGHTNESS_60NT */
	{0x02, 0x2B}, /* IBRIGHTNESS_64NT */
	{0x02, 0x09}, /* IBRIGHTNESS_68NT */
	{0x02, 0x09}, /* IBRIGHTNESS_72NT */
	{0x02, 0x09}, /* IBRIGHTNESS_77NT */
	{0x02, 0x09}, /* IBRIGHTNESS_82NT */
	{0x02, 0x09}, /* IBRIGHTNESS_87NT */
	{0x02, 0x09}, /* IBRIGHTNESS_93NT */
	{0x02, 0x09}, /* IBRIGHTNESS_98NT */
	{0x02, 0x09}, /* IBRIGHTNESS_105NT */
	{0x02, 0x09}, /* IBRIGHTNESS_111NT */
	{0x02, 0x09}, /* IBRIGHTNESS_119NT */
	{0x02, 0x09}, /* IBRIGHTNESS_126NT */
	{0x02, 0x09}, /* IBRIGHTNESS_134NT */
	{0x02, 0x09}, /* IBRIGHTNESS_143NT */
	{0x02, 0x09}, /* IBRIGHTNESS_152NT */
	{0x01, 0xF1}, /* IBRIGHTNESS_162NT */
	{0x01, 0xB2}, /* IBRIGHTNESS_172NT */
	{0x01, 0x6D}, /* IBRIGHTNESS_183NT */
	{0x01, 0x25}, /* IBRIGHTNESS_195NT */
	{0x00, 0xDE}, /* IBRIGHTNESS_207NT */
	{0x00, 0x91}, /* IBRIGHTNESS_220NT */
	{0x00, 0x3D}, /* IBRIGHTNESS_234NT */
	{0x00, 0x08}, /* IBRIGHTNESS_249NT */
	{0x00, 0x08}, /* IBRIGHTNESS_265NT */
	{0x00, 0x08}, /* IBRIGHTNESS_282NT */
	{0x00, 0x08}, /* IBRIGHTNESS_300NT */
	{0x00, 0x08}, /* IBRIGHTNESS_316NT */
	{0x00, 0x08}, /* IBRIGHTNESS_333NT */
	{0x00, 0x08}, /* IBRIGHTNESS_350NT */
};

static const unsigned char aor_cmd_RevBC[IBRIGHTNESS_MAX][2] =
{
	{0x04, 0xFD}, /* IBRIGHTNESS_2NT */
	{0x04, 0xF1}, /* IBRIGHTNESS_3NT */
	{0x04, 0xE6}, /* IBRIGHTNESS_4NT */
	{0x04, 0xDB}, /* IBRIGHTNESS_5NT */
	{0x04, 0xCF}, /* IBRIGHTNESS_6NT */
	{0x04, 0xC4}, /* IBRIGHTNESS_7NT */
	{0x04, 0xB8}, /* IBRIGHTNESS_8NT */
	{0x04, 0xAD}, /* IBRIGHTNESS_9NT */
	{0x04, 0xA1}, /* IBRIGHTNESS_10NT */
	{0x04, 0x95}, /* IBRIGHTNESS_11NT */
	{0x04, 0x8A}, /* IBRIGHTNESS_12NT */
	{0x04, 0x7E}, /* IBRIGHTNESS_13NT */
	{0x04, 0x73}, /* IBRIGHTNESS_14NT */
	{0x04, 0x67}, /* IBRIGHTNESS_15NT */
	{0x04, 0x5C}, /* IBRIGHTNESS_16NT */
	{0x04, 0x51}, /* IBRIGHTNESS_17NT */
	{0x04, 0x3A}, /* IBRIGHTNESS_19NT */
	{0x04, 0x2E}, /* IBRIGHTNESS_20NT */
	{0x04, 0x21}, /* IBRIGHTNESS_21NT */
	{0x04, 0x14}, /* IBRIGHTNESS_22NT */
	{0x03, 0xFB}, /* IBRIGHTNESS_24NT */
	{0x03, 0xEE}, /* IBRIGHTNESS_25NT */
	{0x03, 0xD5}, /* IBRIGHTNESS_27NT */
	{0x03, 0xBB}, /* IBRIGHTNESS_29NT */
	{0x03, 0xAF}, /* IBRIGHTNESS_30NT */
	{0x03, 0x95}, /* IBRIGHTNESS_32NT */
	{0x03, 0x7C}, /* IBRIGHTNESS_34NT */
	{0x03, 0x56}, /* IBRIGHTNESS_37NT */
	{0x03, 0x3C}, /* IBRIGHTNESS_39NT */
	{0x03, 0x23}, /* IBRIGHTNESS_41NT */
	{0x02, 0xFD}, /* IBRIGHTNESS_44NT */
	{0x02, 0xD7}, /* IBRIGHTNESS_47NT */
	{0x02, 0xB1}, /* IBRIGHTNESS_50NT */
	{0x02, 0x8B}, /* IBRIGHTNESS_53NT */
	{0x02, 0x65}, /* IBRIGHTNESS_56NT */
	{0x02, 0x32}, /* IBRIGHTNESS_60NT */
	{0x01, 0xFF}, /* IBRIGHTNESS_64NT */
	{0x02, 0x09}, /* IBRIGHTNESS_68NT */
	{0x02, 0x09}, /* IBRIGHTNESS_72NT */
	{0x02, 0x09}, /* IBRIGHTNESS_77NT */
	{0x02, 0x09}, /* IBRIGHTNESS_82NT */
	{0x02, 0x09}, /* IBRIGHTNESS_87NT */
	{0x02, 0x09}, /* IBRIGHTNESS_93NT */
	{0x02, 0x09}, /* IBRIGHTNESS_98NT */
	{0x02, 0x09}, /* IBRIGHTNESS_105NT */
	{0x02, 0x09}, /* IBRIGHTNESS_111NT */
	{0x02, 0x09}, /* IBRIGHTNESS_119NT */
	{0x02, 0x09}, /* IBRIGHTNESS_126NT */
	{0x02, 0x09}, /* IBRIGHTNESS_134NT */
	{0x02, 0x09}, /* IBRIGHTNESS_143NT */
	{0x02, 0x09}, /* IBRIGHTNESS_152NT */
	{0x01, 0xE7}, /* IBRIGHTNESS_162NT */
	{0x01, 0xAD}, /* IBRIGHTNESS_172NT */
	{0x01, 0x6D}, /* IBRIGHTNESS_183NT */
	{0x01, 0x28}, /* IBRIGHTNESS_195NT */
	{0x00, 0xE2}, /* IBRIGHTNESS_207NT */
	{0x00, 0x98}, /* IBRIGHTNESS_220NT */
	{0x00, 0x47}, /* IBRIGHTNESS_234NT */
	{0x00, 0x08}, /* IBRIGHTNESS_249NT */
	{0x00, 0x08}, /* IBRIGHTNESS_265NT */
	{0x00, 0x08}, /* IBRIGHTNESS_282NT */
	{0x00, 0x08}, /* IBRIGHTNESS_300NT */
	{0x00, 0x08}, /* IBRIGHTNESS_316NT */
	{0x00, 0x08}, /* IBRIGHTNESS_333NT */
	{0x00, 0x08}, /* IBRIGHTNESS_350NT */
};

static const unsigned char aor_cmd_RevA[IBRIGHTNESS_MAX][2] =
{
	{0x05, 0x01}, /* IBRIGHTNESS_2NT */
	{0x04, 0xF6}, /* IBRIGHTNESS_3NT */
	{0x04, 0xE9}, /* IBRIGHTNESS_4NT */
	{0x04, 0xDE}, /* IBRIGHTNESS_5NT */
	{0x04, 0xD1}, /* IBRIGHTNESS_6NT */
	{0x04, 0xC6}, /* IBRIGHTNESS_7NT */
	{0x04, 0xBA}, /* IBRIGHTNESS_8NT */
	{0x04, 0xB0}, /* IBRIGHTNESS_9NT */
	{0x04, 0xA4}, /* IBRIGHTNESS_10NT */
	{0x04, 0x98}, /* IBRIGHTNESS_11NT */
	{0x04, 0x8D}, /* IBRIGHTNESS_12NT */
	{0x04, 0x80}, /* IBRIGHTNESS_13NT */
	{0x04, 0x74}, /* IBRIGHTNESS_14NT */
	{0x04, 0x68}, /* IBRIGHTNESS_15NT */
	{0x04, 0x5C}, /* IBRIGHTNESS_16NT */
	{0x04, 0x51}, /* IBRIGHTNESS_17NT */
	{0x04, 0x3A}, /* IBRIGHTNESS_19NT */
	{0x04, 0x2E}, /* IBRIGHTNESS_20NT */
	{0x04, 0x22}, /* IBRIGHTNESS_21NT */
	{0x04, 0x16}, /* IBRIGHTNESS_22NT */
	{0x03, 0xFF}, /* IBRIGHTNESS_24NT */
	{0x03, 0xF3}, /* IBRIGHTNESS_25NT */
	{0x03, 0xDA}, /* IBRIGHTNESS_27NT */
	{0x03, 0xB4}, /* IBRIGHTNESS_29NT */
	{0x03, 0xAD}, /* IBRIGHTNESS_30NT */
	{0x03, 0x9E}, /* IBRIGHTNESS_32NT */
	{0x03, 0x86}, /* IBRIGHTNESS_34NT */
	{0x03, 0x61}, /* IBRIGHTNESS_37NT */
	{0x03, 0x49}, /* IBRIGHTNESS_39NT */
	{0x03, 0x31}, /* IBRIGHTNESS_41NT */
	{0x03, 0x0B}, /* IBRIGHTNESS_44NT */
	{0x02, 0xE6}, /* IBRIGHTNESS_47NT */
	{0x02, 0xC0}, /* IBRIGHTNESS_50NT */
	{0x02, 0x9A}, /* IBRIGHTNESS_53NT */
	{0x02, 0x74}, /* IBRIGHTNESS_56NT */
	{0x02, 0x41}, /* IBRIGHTNESS_60NT */
	{0x02, 0x12}, /* IBRIGHTNESS_64NT */
	{0x02, 0x06}, /* IBRIGHTNESS_68NT */
	{0x02, 0x06}, /* IBRIGHTNESS_72NT */
	{0x02, 0x06}, /* IBRIGHTNESS_77NT */
	{0x02, 0x06}, /* IBRIGHTNESS_82NT */
	{0x02, 0x06}, /* IBRIGHTNESS_87NT */
	{0x02, 0x06}, /* IBRIGHTNESS_93NT */
	{0x02, 0x06}, /* IBRIGHTNESS_98NT */
	{0x02, 0x06}, /* IBRIGHTNESS_105NT */
	{0x02, 0x06}, /* IBRIGHTNESS_111NT */
	{0x02, 0x06}, /* IBRIGHTNESS_119NT */
	{0x02, 0x06}, /* IBRIGHTNESS_126NT */
	{0x02, 0x06}, /* IBRIGHTNESS_134NT */
	{0x02, 0x06}, /* IBRIGHTNESS_143NT */
	{0x02, 0x06}, /* IBRIGHTNESS_152NT */
	{0x01, 0xE4}, /* IBRIGHTNESS_162NT */
	{0x01, 0xAD}, /* IBRIGHTNESS_172NT */
	{0x01, 0x7A}, /* IBRIGHTNESS_183NT */
	{0x01, 0x2E}, /* IBRIGHTNESS_195NT */
	{0x00, 0xED}, /* IBRIGHTNESS_207NT */
	{0x00, 0xA7}, /* IBRIGHTNESS_220NT */
	{0x00, 0x54}, /* IBRIGHTNESS_234NT */
	{0x00, 0x10}, /* IBRIGHTNESS_249NT */
	{0x00, 0x10}, /* IBRIGHTNESS_265NT */
	{0x00, 0x10}, /* IBRIGHTNESS_282NT */
	{0x00, 0x10}, /* IBRIGHTNESS_300NT */
	{0x00, 0x10}, /* IBRIGHTNESS_316NT */
	{0x00, 0x10}, /* IBRIGHTNESS_333NT */
	{0x00, 0x10}, /* IBRIGHTNESS_350NT */
};

static const unsigned char (*paor_cmd)[2] = aor_cmd_RevA;

static const int offset_gradation_revE[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{0, 39, 39, 39, 39, 35, 25, 14, 7, 0}, /* IBRIGHTNESS_2NT */
	{0, 35, 35, 35, 32, 29, 19, 12, 5, 0}, /* IBRIGHTNESS_3NT */
	{0, 31, 31, 31, 28, 25, 17, 10, 5, 0}, /* IBRIGHTNESS_4NT */
	{0, 28, 28, 28, 24, 21, 14, 7, 3, 0}, /* IBRIGHTNESS_5NT */
	{0, 26, 26, 26, 22, 20, 12, 6, 3, 0}, /* IBRIGHTNESS_6NT */
	{0, 25, 25, 23, 20, 18, 11, 6, 3, 0}, /* IBRIGHTNESS_7NT */
	{0, 23, 23, 21, 18, 16, 10, 5, 3, 0}, /* IBRIGHTNESS_8NT */
	{0, 22, 22, 20, 16, 14, 8, 4, 3, 0}, /* IBRIGHTNESS_9NT */
	{0, 21, 21, 18, 15, 13, 7, 4, 3, 0}, /* IBRIGHTNESS_10NT */
	{0, 20, 20, 17, 14, 12, 7, 4, 3, 0}, /* IBRIGHTNESS_11NT */
	{0, 20, 20, 16, 14, 12, 6, 4, 3, 0}, /* IBRIGHTNESS_12NT */
	{0, 19, 19, 16, 13, 11, 6, 3, 3, 0}, /* IBRIGHTNESS_13NT */
	{0, 18, 18, 16, 12, 10, 6, 2, 3, 0}, /* IBRIGHTNESS_14NT */
	{0, 17, 17, 15, 12, 10, 5, 2, 3, 0}, /* IBRIGHTNESS_15NT */
	{0, 16, 16, 14, 12, 10, 4, 2, 3, 0}, /* IBRIGHTNESS_16NT */
	{0, 16, 16, 14, 11, 9, 4, 2, 3, 0}, /* IBRIGHTNESS_17NT */
	{0, 15, 15, 12, 9, 8, 3, 2, 3, 0}, /* IBRIGHTNESS_19NT */
	{0, 15, 15, 12, 9, 7, 3, 2, 3, 0}, /* IBRIGHTNESS_20NT */
	{0, 14, 14, 12, 8, 6, 3, 2, 3, 0}, /* IBRIGHTNESS_21NT */
	{0, 14, 14, 11, 8, 6, 3, 2, 3, 0}, /* IBRIGHTNESS_22NT */
	{0, 13, 13, 10, 7, 5, 3, 2, 3, 0}, /* IBRIGHTNESS_24NT */
	{0, 13, 13, 10, 7, 5, 3, 2, 3, 0}, /* IBRIGHTNESS_25NT */
	{0, 12, 12, 9, 6, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_27NT */
	{0, 11, 11, 9, 6, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_29NT */
	{0, 11, 11, 9, 6, 4, 3, 2, 3, 1}, /* IBRIGHTNESS_30NT */
	{0, 10, 10, 8, 5, 4, 3, 2, 3, 1}, /* IBRIGHTNESS_32NT */
	{0, 10, 10, 8, 5, 4, 3, 2, 3, 1}, /* IBRIGHTNESS_34NT */
	{0, 9, 9, 7, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_37NT */
	{0, 8, 8, 6, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_39NT */
	{0, 8, 8, 6, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_41NT */
	{0, 8, 8, 6, 4, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_44NT */
	{0, 7, 7, 6, 3, 3, 3, 2, 3, 1}, /* IBRIGHTNESS_47NT */
	{0, 7, 7, 5, 3, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_50NT */
	{0, 7, 7, 5, 3, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_53NT */
	{0, 6, 6, 5, 3, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_56NT */
	{0, 6, 6, 5, 3, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_60NT */
	{0, 5, 5, 5, 3, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_64NT */
	{0, 6, 6, 4, 2, 2, 3, 2, 2, 0}, /* IBRIGHTNESS_68NT */
	{0, 6, 6, 4, 2, 2, 3, 2, 2, 0}, /* IBRIGHTNESS_72NT */
	{0, 6, 6, 4, 2, 2, 4, 3, 3, 0}, /* IBRIGHTNESS_77NT */
	{0, 6, 6, 4, 2, 2, 4, 3, 3, 0}, /* IBRIGHTNESS_82NT */
	{0, 6, 6, 4, 2, 2, 4, 3, 3, 0}, /* IBRIGHTNESS_87NT */
	{0, 6, 6, 4, 2, 3, 5, 4, 4, 0}, /* IBRIGHTNESS_93NT */
	{0, 6, 6, 4, 2, 3, 5, 4, 4, 0}, /* IBRIGHTNESS_98NT */
	{0, 6, 6, 4, 2, 3, 4, 4, 4, 0}, /* IBRIGHTNESS_105NT */
	{0, 6, 6, 4, 2, 3, 4, 4, 4, 0}, /* IBRIGHTNESS_111NT */
	{0, 5, 5, 3, 2, 2, 3, 3, 3, 0}, /* IBRIGHTNESS_119NT */
	{0, 5, 5, 3, 2, 2, 3, 3, 3, 0}, /* IBRIGHTNESS_126NT */
	{0, 5, 5, 3, 2, 2, 3, 3, 3, 0}, /* IBRIGHTNESS_134NT */
	{0, 4, 4, 2, 2, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_143NT */
	{0, 4, 4, 2, 2, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_152NT */
	{0, 4, 4, 2, 2, 2, 3, 2, 2, 0}, /* IBRIGHTNESS_162NT */
	{0, 3, 3, 2, 2, 2, 3, 2, 2, 0}, /* IBRIGHTNESS_172NT */
	{0, 3, 3, 1, 2, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_183NT */
	{0, 2, 2, 1, 2, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_195NT */
	{0, 1, 1, 1, 2, 2, 2, 2, 1, 0}, /* IBRIGHTNESS_207NT */
	{0, 1, 1, 1, 2, 2, 2, 1, 1, 0}, /* IBRIGHTNESS_220NT */
	{0, 1, 1, 1, 1, 1, 1, 1, 1, 0}, /* IBRIGHTNESS_234NT */
	{0, 1, 1, 1, 2, 2, 3, 4, 3, 3}, /* IBRIGHTNESS_249NT */
	{0, 1, 1, 1, 1, 1, 3, 4, 3, 3}, /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 1, 1, 2, 3, 2, 2}, /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 1, 1, 2, 2, 1, 1}, /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 1, 2, 1, 1}, /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 1, 2, 0, 0}, /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_350NT */
};

static const int offset_gradation_revD[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{0, 48, 48, 48, 43, 39, 28, 17, 8, 0}, /* IBRIGHTNESS_2NT */
	{0, 40, 40, 39, 34, 32, 23, 12, 6, 0}, /* IBRIGHTNESS_3NT */
	{0, 37, 37, 35, 30, 28, 21, 12, 6, 0}, /* IBRIGHTNESS_4NT */
	{0, 32, 32, 32, 28, 25, 19, 11, 5, 0}, /* IBRIGHTNESS_5NT */
	{0, 32, 32, 29, 24, 22, 17, 10, 4, 0}, /* IBRIGHTNESS_6NT */
	{0, 29, 29, 27, 23, 20, 15, 9, 4, 0}, /* IBRIGHTNESS_7NT */
	{0, 27, 27, 25, 22, 19, 14, 9, 4, 0}, /* IBRIGHTNESS_8NT */
	{0, 26, 26, 24, 20, 18, 12, 8, 4, 0}, /* IBRIGHTNESS_9NT */
	{0, 24, 24, 22, 19, 16, 11, 7, 4, 0}, /* IBRIGHTNESS_10NT */
	{0, 23, 23, 20, 17, 16, 10, 6, 4, 0}, /* IBRIGHTNESS_11NT */
	{0, 22, 22, 19, 17, 15, 9, 6, 4, 0}, /* IBRIGHTNESS_12NT */
	{0, 20, 20, 18, 15, 13, 8, 6, 4, 0}, /* IBRIGHTNESS_13NT */
	{0, 20, 20, 17, 14, 12, 8, 6, 4, 0}, /* IBRIGHTNESS_14NT */
	{0, 19, 19, 16, 13, 11, 8, 6, 4, 0}, /* IBRIGHTNESS_15NT */
	{0, 17, 17, 15, 12, 10, 8, 6, 4, 0}, /* IBRIGHTNESS_16NT */
	{0, 17, 17, 15, 12, 10, 7, 5, 4, 0}, /* IBRIGHTNESS_17NT */
	{0, 16, 16, 14, 11, 10, 6, 4, 3, 0}, /* IBRIGHTNESS_19NT */
	{0, 15, 15, 13, 11, 9, 6, 4, 3, 0}, /* IBRIGHTNESS_20NT */
	{0, 14, 14, 12, 10, 8, 6, 4, 3, 0}, /* IBRIGHTNESS_21NT */
	{0, 14, 14, 12, 10, 8, 6, 4, 3, 0}, /* IBRIGHTNESS_22NT */
	{0, 14, 14, 12, 9, 8, 5, 4, 3, 0}, /* IBRIGHTNESS_24NT */
	{0, 14, 14, 12, 9, 8, 5, 4, 3, 0}, /* IBRIGHTNESS_25NT */
	{0, 13, 13, 11, 8, 7, 4, 4, 3, 0}, /* IBRIGHTNESS_27NT */
	{0, 12, 12, 10, 7, 6, 4, 3, 3, 0}, /* IBRIGHTNESS_29NT */
	{0, 12, 12, 10, 7, 6, 4, 3, 3, 0}, /* IBRIGHTNESS_30NT */
	{0, 11, 11, 10, 6, 5, 4, 2, 3, 0}, /* IBRIGHTNESS_32NT */
	{0, 11, 11, 9, 6, 6, 3, 2, 3, 0}, /* IBRIGHTNESS_34NT */
	{0, 10, 10, 8, 6, 5, 3, 2, 3, 0}, /* IBRIGHTNESS_37NT */
	{0, 9, 9, 8, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_39NT */
	{0, 9, 9, 7, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_41NT */
	{0, 8, 8, 7, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_44NT */
	{0, 8, 8, 6, 4, 4, 2, 2, 3, 0}, /* IBRIGHTNESS_47NT */
	{0, 7, 7, 6, 4, 3, 2, 2, 3, 0}, /* IBRIGHTNESS_50NT */
	{0, 7, 7, 6, 3, 3, 2, 2, 3, 0}, /* IBRIGHTNESS_53NT */
	{0, 6, 6, 5, 3, 3, 2, 2, 3, 0}, /* IBRIGHTNESS_56NT */
	{0, 6, 6, 4, 2, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_60NT */
	{0, 5, 5, 4, 2, 2, 1, 2, 2, 0}, /* IBRIGHTNESS_64NT */
	{0, 5, 5, 4, 2, 2, 1, 2, 2, 0}, /* IBRIGHTNESS_68NT */
	{0, 5, 5, 4, 2, 2, 1, 2, 2, 0}, /* IBRIGHTNESS_72NT */
	{0, 5, 5, 4, 2, 2, 1, 2, 2, 0}, /* IBRIGHTNESS_77NT */
	{0, 5, 5, 4, 2, 2, 1, 2, 2, 0}, /* IBRIGHTNESS_82NT */
	{0, 5, 5, 3, 2, 2, 1, 1, 1, 0}, /* IBRIGHTNESS_87NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_93NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_98NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_105NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_111NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_119NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_126NT */
	{0, 5, 5, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_134NT */
	{0, 4, 4, 3, 2, 2, 1, 0, 0, 0}, /* IBRIGHTNESS_143NT */
	{0, 4, 4, 2, 2, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_152NT */
	{0, 4, 4, 3, 2, 2, 3, 0, 0, 0}, /* IBRIGHTNESS_162NT */
	{0, 4, 4, 3, 2, 2, 2, 0, 0, 0}, /* IBRIGHTNESS_172NT */
	{0, 3, 3, 2, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_183NT */
	{0, 2, 2, 2, 1, 1, 0, 0, 0, 0}, /* IBRIGHTNESS_195NT */
	{0, 2, 2, 1, 1, 1, 0, 0, 0, 0}, /* IBRIGHTNESS_207NT */
	{0, 1, 1, 1, 1, 1, 0, 0, 0, 0}, /* IBRIGHTNESS_220NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_234NT */
	{0, 1, 1, 1, 1, 1, 1, 1, 1, 2}, /* IBRIGHTNESS_249NT */
	{0, 1, 1, 1, 1, 1, 1, 1, 1, 2}, /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_350NT */
};

static const int offset_gradation_revBC[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{0, 56, 56, 52, 47, 42, 31, 19, 9, 0}, /* IBRIGHTNESS_2NT */
	{0, 46, 46, 42, 38, 34, 26, 15, 9, 0}, /* IBRIGHTNESS_3NT */
	{0, 42, 42, 39, 34, 30, 22, 14, 7, 0}, /* IBRIGHTNESS_4NT */
	{0, 38, 38, 34, 29, 25, 18, 11, 6, 0}, /* IBRIGHTNESS_5NT */
	{0, 35, 35, 31, 26, 23, 16, 10, 6, 0}, /* IBRIGHTNESS_6NT */
	{0, 32, 32, 28, 24, 20, 14, 10, 6, 0}, /* IBRIGHTNESS_7NT */
	{0, 30, 30, 26, 22, 19, 13, 9, 5, 0}, /* IBRIGHTNESS_8NT */
	{0, 28, 28, 25, 22, 18, 12, 8, 5, 0}, /* IBRIGHTNESS_9NT */
	{0, 27, 27, 23, 19, 16, 12, 7, 5, 0}, /* IBRIGHTNESS_10NT */
	{0, 26, 26, 22, 18, 16, 12, 6, 4, 0}, /* IBRIGHTNESS_11NT */
	{0, 25, 25, 22, 18, 15, 11, 6, 4, 0}, /* IBRIGHTNESS_12NT */
	{0, 24, 24, 21, 17, 14, 11, 6, 4, 0}, /* IBRIGHTNESS_13NT */
	{0, 23, 23, 20, 16, 13, 10, 6, 4, 0}, /* IBRIGHTNESS_14NT */
	{0, 22, 22, 19, 15, 12, 9, 6, 4, 0}, /* IBRIGHTNESS_15NT */
	{0, 21, 21, 18, 15, 12, 9, 6, 4, 0}, /* IBRIGHTNESS_16NT */
	{0, 20, 20, 17, 13, 11, 9, 5, 4, 0}, /* IBRIGHTNESS_17NT */
	{0, 19, 19, 16, 13, 10, 8, 4, 3, 0}, /* IBRIGHTNESS_19NT */
	{0, 18, 18, 16, 12, 10, 7, 4, 3, 0}, /* IBRIGHTNESS_20NT */
	{0, 18, 18, 15, 11, 9, 7, 4, 3, 0}, /* IBRIGHTNESS_21NT */
	{0, 17, 17, 15, 11, 9, 7, 4, 3, 0}, /* IBRIGHTNESS_22NT */
	{0, 16, 16, 14, 11, 8, 6, 4, 3, 0}, /* IBRIGHTNESS_24NT */
	{0, 16, 16, 14, 10, 8, 6, 4, 3, 0}, /* IBRIGHTNESS_25NT */
	{0, 14, 14, 12, 9, 7, 6, 4, 3, 0}, /* IBRIGHTNESS_27NT */
	{0, 14, 14, 12, 8, 6, 5, 3, 3, 0}, /* IBRIGHTNESS_29NT */
	{0, 14, 14, 11, 8, 6, 5, 3, 3, 0}, /* IBRIGHTNESS_30NT */
	{0, 13, 13, 10, 7, 6, 5, 3, 3, 0}, /* IBRIGHTNESS_32NT */
	{0, 12, 12, 9, 7, 6, 5, 3, 3, 0}, /* IBRIGHTNESS_34NT */
	{0, 11, 11, 8, 6, 5, 4, 2, 3, 0}, /* IBRIGHTNESS_37NT */
	{0, 10, 10, 8, 6, 4, 4, 2, 3, 0}, /* IBRIGHTNESS_39NT */
	{0, 10, 10, 7, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_41NT */
	{0, 10, 10, 7, 5, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_44NT */
	{0, 9, 9, 6, 4, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_47NT */
	{0, 8, 8, 6, 4, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_50NT */
	{0, 7, 7, 5, 3, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_53NT */
	{0, 7, 7, 5, 3, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_56NT */
	{0, 6, 6, 5, 3, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_60NT */
	{0, 6, 6, 5, 3, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_64NT */
	{0, 6, 6, 4, 3, 2, 3, 2, 2, 0}, /* IBRIGHTNESS_68NT */
	{0, 6, 6, 5, 3, 3, 4, 3, 3, 0}, /* IBRIGHTNESS_72NT */
	{0, 6, 6, 5, 4, 3, 4, 4, 4, 0}, /* IBRIGHTNESS_77NT */
	{0, 7, 7, 5, 4, 3, 5, 4, 4, 0}, /* IBRIGHTNESS_82NT */
	{0, 7, 7, 4, 4, 3, 5, 4, 4, 0}, /* IBRIGHTNESS_87NT */
	{0, 7, 7, 4, 3, 3, 5, 4, 4, 0}, /* IBRIGHTNESS_93NT */
	{0, 7, 7, 4, 3, 3, 5, 4, 4, 0}, /* IBRIGHTNESS_98NT */
	{0, 6, 6, 4, 4, 4, 5, 5, 4, 0}, /* IBRIGHTNESS_105NT */
	{0, 6, 6, 4, 4, 4, 5, 5, 4, 0}, /* IBRIGHTNESS_111NT */
	{0, 6, 6, 4, 3, 3, 4, 4, 3, 0}, /* IBRIGHTNESS_119NT */
	{0, 6, 6, 4, 3, 3, 4, 3, 3, 0}, /* IBRIGHTNESS_126NT */
	{0, 6, 6, 3, 3, 2, 3, 2, 2, 0}, /* IBRIGHTNESS_134NT */
	{0, 5, 5, 4, 3, 2, 2, 1, 1, 0}, /* IBRIGHTNESS_143NT */
	{0, 4, 5, 3, 3, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_152NT */
	{0, 5, 5, 3, 3, 2, 3, 0, 0, 0}, /* IBRIGHTNESS_162NT */
	{0, 4, 4, 2, 3, 2, 3, 0, 0, 0}, /* IBRIGHTNESS_172NT */
	{0, 3, 3, 2, 2, 2, 2, 0, 0, 0}, /* IBRIGHTNESS_183NT */
	{0, 2, 2, 2, 2, 2, 2, 0, 0, 0}, /* IBRIGHTNESS_195NT */
	{0, 2, 2, 2, 2, 2, 2, 0, 0, 0}, /* IBRIGHTNESS_207NT */
	{0, 2, 2, 1, 2, 2, 2, 0, 0, 0}, /* IBRIGHTNESS_220NT */
	{0, 1, 1, 1, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_234NT */
	{0, 1, 1, 1, 2, 2, 3, 4, 3, 3}, /* IBRIGHTNESS_249NT */
	{0, 1, 1, 1, 1, 1, 3, 4, 3, 3}, /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 1, 1, 2, 3, 2, 2}, /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 1, 1, 2, 2, 1, 1}, /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 1, 2, 1, 1}, /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 1, 2, 0, 0}, /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_350NT */
};

static const int offset_gradation_revA[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{0, 42, 42, 42, 38, 33, 23, 14, 7, 0}, /* IBRIGHTNESS_2NT */
	{0, 37, 37, 37, 33, 29, 20, 11, 6, 0}, /* IBRIGHTNESS_3NT */
	{0, 34, 34, 33, 27, 23, 16, 10, 6, 0}, /* IBRIGHTNESS_4NT */
	{0, 32, 32, 31, 25, 23, 16, 10, 6, 0}, /* IBRIGHTNESS_5NT */
	{0, 30, 30, 27, 23, 19, 15, 10, 6, 0}, /* IBRIGHTNESS_6NT */
	{0, 28, 28, 25, 21, 17, 13, 9, 5, 0}, /* IBRIGHTNESS_7NT */
	{0, 26, 26, 23, 19, 16, 12, 8, 5, 0}, /* IBRIGHTNESS_8NT */
	{0, 25, 25, 22, 19, 16, 12, 8, 5, 0}, /* IBRIGHTNESS_9NT */
	{0, 23, 23, 20, 17, 14, 11, 7, 5, 0}, /* IBRIGHTNESS_10NT */
	{0, 21, 21, 19, 16, 14, 10, 6, 4, 0}, /* IBRIGHTNESS_11NT */
	{0, 20, 20, 18, 15, 13, 9, 6, 4, 0}, /* IBRIGHTNESS_12NT */
	{0, 19, 19, 17, 14, 12, 9, 6, 4, 0}, /* IBRIGHTNESS_13NT */
	{0, 18, 18, 16, 13, 12, 8, 6, 4, 0}, /* IBRIGHTNESS_14NT */
	{0, 17, 17, 15, 12, 11, 8, 6, 4, 0}, /* IBRIGHTNESS_15NT */
	{0, 16, 16, 14, 12, 10, 8, 6, 4, 0}, /* IBRIGHTNESS_16NT */
	{0, 16, 16, 14, 11, 10, 7, 6, 4, 0}, /* IBRIGHTNESS_17NT */
	{0, 15, 15, 13, 10, 9, 7, 5, 4, 0}, /* IBRIGHTNESS_19NT */
	{0, 14, 14, 12, 10, 9, 6, 5, 4, 0}, /* IBRIGHTNESS_20NT */
	{0, 14, 14, 12, 9, 8, 6, 4, 4, 0}, /* IBRIGHTNESS_21NT */
	{0, 13, 13, 11, 8, 8, 6, 4, 4, 0}, /* IBRIGHTNESS_22NT */
	{0, 12, 12, 10, 8, 7, 5, 4, 4, 0}, /* IBRIGHTNESS_24NT */
	{0, 12, 12, 10, 8, 7, 5, 4, 4, 0}, /* IBRIGHTNESS_25NT */
	{0, 12, 12, 9, 7, 7, 5, 4, 4, 0}, /* IBRIGHTNESS_27NT */
	{0, 11, 11, 8, 6, 6, 4, 3, 4, 0}, /* IBRIGHTNESS_29NT */
	{0, 10, 10, 8, 6, 5, 4, 3, 4, 0}, /* IBRIGHTNESS_30NT */
	{0, 10, 10, 8, 6, 5, 4, 3, 4, 0}, /* IBRIGHTNESS_32NT */
	{0, 9, 9, 8, 6, 5, 3, 3, 3, 0}, /* IBRIGHTNESS_34NT */
	{0, 8, 8, 7, 5, 5, 3, 3, 3, 0}, /* IBRIGHTNESS_37NT */
	{0, 8, 8, 7, 4, 5, 3, 3, 3, 0}, /* IBRIGHTNESS_39NT */
	{0, 7, 7, 6, 4, 4, 3, 3, 3, 0}, /* IBRIGHTNESS_41NT */
	{0, 7, 7, 6, 4, 4, 3, 3, 3, 0}, /* IBRIGHTNESS_44NT */
	{0, 6, 6, 6, 3, 4, 2, 3, 2, 0}, /* IBRIGHTNESS_47NT */
	{0, 6, 6, 5, 3, 4, 2, 3, 2, 0}, /* IBRIGHTNESS_50NT */
	{0, 5, 5, 5, 3, 3, 2, 3, 2, 0}, /* IBRIGHTNESS_53NT */
	{0, 5, 5, 5, 3, 3, 2, 3, 2, 0}, /* IBRIGHTNESS_56NT */
	{0, 5, 5, 5, 3, 3, 2, 2, 2, 0}, /* IBRIGHTNESS_60NT */
	{0, 5, 5, 4, 1, 2, 1, 2, 1, 0}, /* IBRIGHTNESS_64NT */
	{0, 5, 5, 4, 3, 3, 3, 2, 2, 0}, /* IBRIGHTNESS_68NT */
	{0, 5, 5, 4, 3, 3, 3, 2, 2, 0}, /* IBRIGHTNESS_72NT */
	{0, 5, 5, 4, 3, 3, 2, 2, 2, 0}, /* IBRIGHTNESS_77NT */
	{0, 4, 4, 4, 3, 3, 2, 2, 2, 0}, /* IBRIGHTNESS_82NT */
	{0, 4, 4, 4, 3, 3, 2, 2, 2, 0}, /* IBRIGHTNESS_87NT */
	{0, 4, 4, 4, 3, 3, 2, 2, 2, 0}, /* IBRIGHTNESS_93NT */
	{0, 4, 4, 4, 3, 3, 2, 2, 2, 0}, /* IBRIGHTNESS_98NT */
	{0, 4, 4, 3, 2, 2, 1, 1, 1, 0}, /* IBRIGHTNESS_105NT */
	{0, 5, 5, 4, 2, 2, 1, 1, 1, 0}, /* IBRIGHTNESS_111NT */
	{0, 4, 4, 3, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_119NT */
	{0, 4, 4, 3, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_126NT */
	{0, 3, 3, 2, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_134NT */
	{0, 3, 3, 2, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_143NT */
	{0, 3, 3, 2, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_152NT */
	{0, 3, 3, 1, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_162NT */
	{0, 2, 2, 1, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_172NT */
	{0, 2, 2, 1, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_183NT */
	{0, 1, 1, 1, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_195NT */
	{0, 1, 1, 0, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_207NT */
	{0, 1, 1, 0, 1, 1, 1, 0, 0, 0}, /* IBRIGHTNESS_220NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_234NT */
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 2}, /* IBRIGHTNESS_249NT */
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 2}, /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_350NT */
};

static const struct rgb_t offset_color_revE[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -3}},{{0, 4, -7}},{{-8, 4, -12}},{{-11, 5, -12}},{{-5, 2, -5}},{{-4, 2, -5}},{{-5, 3, -10}}}, /* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -4}},{{0, 4, -7}},{{-8, 4, -12}},{{-10, 4, -10}},{{-4, 2, -4}},{{-4, 2, -4}},{{-4, 2, -8}}}, /* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -4}},{{-1, 4, -7}},{{-7, 4, -12}},{{-9, 3, -9}},{{-4, 1, -4}},{{-3, 1, -4}},{{-4, 2, -7}}}, /* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -5}},{{-2, 4, -7}},{{-8, 4, -12}},{{-9, 3, -9}},{{-3, 1, -3}},{{-2, 1, -4}},{{-3, 1, -5}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -5}},{{-2, 4, -7}},{{-8, 4, -12}},{{-8, 2, -8}},{{-2, 1, -2}},{{-2, 1, -4}},{{-2, 0, -4}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -1}},{{-1, 3, -5}},{{-3, 4, -7}},{{-7, 3, -11}},{{-8, 2, -8}},{{-2, 1, -2}},{{-1, 1, -3}},{{-2, 0, -4}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -2}},{{-1, 3, -5}},{{-3, 4, -7}},{{-7, 3, -11}},{{-8, 2, -8}},{{-2, 1, -2}},{{-1, 1, -3}},{{-2, 0, -3}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -2}},{{-1, 3, -5}},{{-3, 4, -7}},{{-7, 3, -10}},{{-8, 2, -8}},{{-2, 1, -2}},{{-1, 1, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -3}},{{-1, 3, -5}},{{-3, 4, -7}},{{-7, 3, -10}},{{-7, 2, -7}},{{-2, 1, -2}},{{-1, 1, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -4}},{{-1, 3, -5}},{{-3, 4, -7}},{{-7, 3, -10}},{{-7, 2, -7}},{{-2, 1, -2}},{{-1, 1, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -4}},{{-2, 2, -6}},{{-4, 4, -7}},{{-6, 2, -10}},{{-7, 2, -6}},{{-2, 0, -2}},{{-1, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -5}},{{-2, 2, -6}},{{-4, 4, -7}},{{-6, 2, -9}},{{-7, 2, -6}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -6}},{{-2, 2, -6}},{{-4, 4, -7}},{{-6, 2, -8}},{{-7, 2, -6}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -6}},{{-2, 2, -6}},{{-4, 4, -7}},{{-6, 2, -8}},{{-7, 2, -5}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -7}},{{-2, 2, -6}},{{-4, 4, -7}},{{-6, 2, -8}},{{-6, 2, -5}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -8}},{{-2, 2, -6}},{{-5, 3, -7}},{{-6, 2, -7}},{{-5, 2, -5}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -9}},{{-3, 2, -6}},{{-6, 3, -7}},{{-6, 2, -7}},{{-4, 2, -4}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -7}},{{-6, 2, -7}},{{-3, 2, -4}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -7}},{{-6, 2, -7}},{{-3, 2, -4}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -7}},{{-6, 2, -7}},{{-3, 2, -4}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -7}},{{-6, 2, -7}},{{-3, 2, -3}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 1}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -7}},{{-6, 2, -7}},{{-3, 2, -3}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -6}},{{-5, 1, -7}},{{-2, 1, -2}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -6}},{{-5, 1, -7}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 3, -6}},{{-5, 1, -7}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, -1, 0}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 2, -6}},{{-5, 1, -7}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 2, -6}},{{-5, 1, -7}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 2, -6}},{{-5, 1, -7}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 2, -6}},{{-6, 2, -6}},{{-5, 1, -6}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -11}},{{-3, 2, -6}},{{-6, 2, -5}},{{-5, 1, -6}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -11}},{{-4, 2, -6}},{{-6, 2, -4}},{{-4, 1, -6}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 1}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -11}},{{-4, 2, -5}},{{-6, 2, -4}},{{-4, 1, -5}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -11}},{{-4, 2, -5}},{{-6, 2, -3}},{{-3, 1, -5}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -11}},{{-4, 2, -5}},{{-6, 2, -2}},{{-2, 1, -5}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -12}},{{-4, 2, -5}},{{-6, 2, -2}},{{-2, 1, -5}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -12}},{{-4, 2, -4}},{{-6, 2, -2}},{{-2, 1, -4}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, -2}},{{0, -2, 0}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -12}},{{-5, 1, -4}},{{-5, 1, -1}},{{-1, 1, -4}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -9}},{{-3, 1, -3}},{{-3, 2, -1}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 3, -9}},{{-3, 1, -3}},{{-3, 2, -1}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -8}},{{-4, 1, -3}},{{-3, 1, -2}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -8}},{{-4, 1, -3}},{{-3, 1, -2}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -8}},{{-4, 1, -3}},{{-3, 1, -3}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -8}},{{-5, 0, -3}},{{-3, 0, -4}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -8}},{{-5, 0, -3}},{{-3, 0, -4}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -8}},{{-5, 0, -3}},{{-3, 0, -4}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -8}},{{-5, 0, -3}},{{-3, 0, -4}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -8}},{{-4, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -8}},{{-4, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 1, -8}},{{-4, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 0, -8}},{{-4, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 0, -8}},{{-3, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 0, -7}},{{-3, 0, -2}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -1}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 0, -6}},{{-3, 0, -1}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -1}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 0, -4}},{{-3, 0, 0}},{{-3, 0, -3}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 0, -3}},{{-3, 0, 0}},{{-3, 0, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 0, -3}},{{-3, 0, 0}},{{-3, 0, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, -2}},{{-3, 0, 0}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, 0}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_350NT */
};

static const struct rgb_t offset_color_revD[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -6}},{{-3, 3, -7}},{{-9, 4, -13}},{{-11, 4, -13}},{{-6, 2, -6}},{{-5, 2, -5}},{{-7, 3, -11}}}, /* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-3, 1, -9}},{{-4, 4, -8}},{{-9, 4, -13}},{{-10, 4, -12}},{{-6, 1, -5}},{{-5, 1, -4}},{{-5, 2, -9}}}, /* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -9}},{{-4, 4, -8}},{{-9, 4, -13}},{{-10, 2, -12}},{{-6, 1, -5}},{{-4, 1, -4}},{{-4, 2, -6}}}, /* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-5, 2, -9}},{{-4, 4, -8}},{{-9, 4, -13}},{{-10, 2, -12}},{{-6, 1, -4}},{{-4, 1, -4}},{{-2, 1, -5}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -2}},{{-5, 2, -10}},{{-4, 4, -8}},{{-9, 4, -12}},{{-9, 2, -10}},{{-6, 1, -4}},{{-4, 0, -4}},{{-2, 0, -5}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 1, -3}},{{-5, 3, -10}},{{-4, 3, -8}},{{-9, 4, -12}},{{-9, 2, -10}},{{-5, 0, -4}},{{-3, 0, -3}},{{-1, 0, -4}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -5}},{{-5, 3, -11}},{{-4, 3, -7}},{{-9, 3, -12}},{{-8, 2, -9}},{{-5, 0, -4}},{{-3, 0, -3}},{{-1, 0, -4}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -7}},{{-6, 3, -12}},{{-4, 3, -7}},{{-9, 2, -12}},{{-8, 2, -8}},{{-4, 0, -4}},{{-2, 0, -3}},{{-1, 0, -3}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -9}},{{-6, 3, -12}},{{-4, 3, -7}},{{-9, 2, -11}},{{-8, 2, -8}},{{-4, 0, -4}},{{-2, 0, -3}},{{-1, 0, -2}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -10}},{{-6, 3, -12}},{{-4, 3, -7}},{{-8, 2, -10}},{{-8, 2, -8}},{{-4, 0, -4}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -10}},{{-6, 3, -12}},{{-5, 3, -7}},{{-8, 2, -10}},{{-7, 2, -7}},{{-4, 0, -3}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -10}},{{-6, 3, -12}},{{-5, 3, -7}},{{-8, 2, -9}},{{-6, 2, -7}},{{-4, 0, -3}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -10}},{{-6, 4, -12}},{{-5, 2, -7}},{{-8, 2, -9}},{{-6, 2, -6}},{{-4, 0, -3}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -11}},{{-6, 4, -12}},{{-5, 2, -7}},{{-7, 2, -9}},{{-5, 1, -5}},{{-4, 0, -3}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 4, -12}},{{-5, 2, -7}},{{-7, 2, -10}},{{-5, 1, -5}},{{-4, 0, -2}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 4, -12}},{{-5, 2, -7}},{{-7, 2, -10}},{{-5, 1, -5}},{{-3, 0, -2}},{{-2, 0, -2}},{{0, 0, -2}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 4, -13}},{{-5, 2, -7}},{{-7, 2, -9}},{{-5, 0, -5}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 4, -13}},{{-5, 2, -7}},{{-7, 2, -8}},{{-5, 0, -5}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 4, -12}},{{-5, 2, -7}},{{-7, 2, -8}},{{-5, 0, -5}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 4, -12}},{{-5, 2, -7}},{{-7, 2, -8}},{{-5, 0, -5}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 3, -12}},{{-5, 2, -7}},{{-7, 2, -8}},{{-5, 0, -5}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -11}},{{-6, 3, -12}},{{-5, 2, -7}},{{-7, 2, -8}},{{-5, 0, -5}},{{-3, 0, -2}},{{-1, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -12}},{{-6, 2, -11}},{{-5, 2, -6}},{{-6, 2, -8}},{{-4, 0, -4}},{{-3, 0, -2}},{{0, 0, -2}},{{0, 0, -1}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -12}},{{-6, 2, -10}},{{-5, 1, -6}},{{-6, 1, -7}},{{-4, 0, -4}},{{-3, 0, -1}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -12}},{{-6, 2, -10}},{{-5, 1, -6}},{{-6, 1, -7}},{{-4, 0, -4}},{{-3, 0, -1}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -12}},{{-6, 2, -10}},{{-5, 1, -6}},{{-5, 1, -6}},{{-4, 0, -4}},{{-2, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -13}},{{-6, 2, -9}},{{-5, 1, -6}},{{-4, 1, -6}},{{-3, 0, -3}},{{-2, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -13}},{{-6, 2, -9}},{{-5, 1, -6}},{{-4, 1, -6}},{{-3, 0, -3}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 4, -13}},{{-6, 2, -8}},{{-5, 1, -6}},{{-4, 1, -6}},{{-3, 0, -3}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-5, 2, -8}},{{-5, 1, -5}},{{-4, 1, -6}},{{-3, 0, -3}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-4, 2, -7}},{{-5, 1, -5}},{{-4, 1, -5}},{{-3, 0, -3}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-4, 2, -6}},{{-4, 1, -4}},{{-4, 1, -4}},{{-3, 0, -2}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-3, 2, -6}},{{-4, 1, -4}},{{-3, 1, -4}},{{-3, 0, -2}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-3, 1, -6}},{{-4, 1, -4}},{{-3, 1, -4}},{{-3, 0, -2}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-3, 1, -6}},{{-3, 1, -4}},{{-3, 1, -3}},{{-3, 0, -2}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-2, 1, -6}},{{-3, 1, -4}},{{-2, 1, -2}},{{-2, 0, -2}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -13}},{{-2, 1, -5}},{{-3, 1, -3}},{{-2, 1, -2}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -11}},{{-3, 1, -3}},{{-3, 1, -1}},{{-2, 0, -4}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -11}},{{-3, 1, -3}},{{-3, 1, -1}},{{-2, 0, -4}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -11}},{{-3, 1, -3}},{{-3, 1, -1}},{{-2, 0, -4}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, -1}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -11}},{{-2, 1, -4}},{{-2, 0, -2}},{{-2, 0, -4}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -11}},{{-2, 1, -4}},{{-2, 0, -2}},{{-1, 0, -3}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -12}},{{-2, 1, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -12}},{{-2, 1, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -11}},{{-3, 0, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -11}},{{-3, 0, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -11}},{{-3, 0, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -11}},{{-3, 0, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -10}},{{-3, 0, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -8}},{{-3, 0, -4}},{{-2, 0, -2}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -6}},{{-3, 0, -4}},{{-3, 0, -3}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -6}},{{-2, 0, -2}},{{-3, 0, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -6}},{{-2, 0, -2}},{{-3, 0, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 1, -5}},{{-1, 0, -1}},{{-2, 0, -2}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 1, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 1, -4}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 1, -3}},{{-1, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_350NT */
};

static const struct rgb_t offset_color_revBC[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -3}},{{-3, 3, -11}},{{-2, 2, -10}},{{-5, 6, -13}},{{-11, 5, -16}},{{-6, 2, -7}},{{-5, 2, -7}},{{-7, 3, -12}}}, /* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -5}},{{-3, 3, -10}},{{-2, 2, -10}},{{-5, 6, -15}},{{-11, 4, -13}},{{-5, 2, -6}},{{-4, 2, -5}},{{-6, 2, -10}}}, /* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -7}},{{-3, 3, -10}},{{-2, 3, -10}},{{-8, 6, -17}},{{-11, 4, -12}},{{-4, 1, -6}},{{-4, 1, -4}},{{-4, 2, -7}}}, /* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -9}},{{-3, 2, -9}},{{-4, 3, -10}},{{-9, 5, -17}},{{-10, 4, -11}},{{-4, 1, -6}},{{-4, 1, -4}},{{-2, 2, -4}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -9}},{{-3, 2, -9}},{{-4, 3, -10}},{{-9, 5, -15}},{{-9, 4, -10}},{{-3, 1, -4}},{{-4, 1, -3}},{{-2, 2, -4}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -10}},{{-3, 3, -9}},{{-4, 3, -10}},{{-9, 5, -14}},{{-8, 4, -10}},{{-3, 1, -4}},{{-4, 0, -3}},{{-2, 2, -4}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -10}},{{-3, 3, -9}},{{-5, 3, -10}},{{-9, 5, -14}},{{-8, 3, -9}},{{-3, 1, -4}},{{-3, 0, -3}},{{-2, 1, -3}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -10}},{{-3, 3, -9}},{{-5, 3, -10}},{{-9, 4, -14}},{{-8, 3, -8}},{{-3, 1, -4}},{{-2, 0, -2}},{{-2, 0, -3}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -11}},{{-3, 3, -9}},{{-5, 3, -10}},{{-9, 4, -14}},{{-7, 3, -8}},{{-3, 1, -4}},{{-2, 0, -2}},{{-2, 0, -3}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -12}},{{-3, 3, -9}},{{-6, 3, -10}},{{-9, 4, -14}},{{-6, 3, -8}},{{-2, 0, -4}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -12}},{{-3, 3, -9}},{{-6, 3, -9}},{{-9, 4, -13}},{{-6, 3, -8}},{{-2, 0, -4}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -13}},{{-3, 3, -9}},{{-7, 3, -9}},{{-9, 4, -13}},{{-6, 2, -8}},{{-2, 0, -4}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -14}},{{-3, 3, -8}},{{-7, 2, -9}},{{-8, 4, -11}},{{-6, 2, -8}},{{-2, 0, -4}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -14}},{{-3, 3, -7}},{{-7, 2, -9}},{{-8, 4, -11}},{{-6, 2, -7}},{{-2, 0, -3}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -14}},{{-3, 3, -7}},{{-7, 2, -9}},{{-8, 4, -9}},{{-6, 2, -7}},{{-2, 0, -3}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 5, -15}},{{-3, 3, -7}},{{-7, 2, -9}},{{-8, 4, -9}},{{-5, 1, -7}},{{-2, 0, -3}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -7}},{{-7, 2, -9}},{{-8, 3, -8}},{{-5, 1, -6}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -6}},{{-7, 2, -9}},{{-8, 3, -8}},{{-5, 1, -6}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -6}},{{-7, 2, -9}},{{-8, 3, -8}},{{-5, 1, -6}},{{-2, 0, -2}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -6}},{{-7, 2, -9}},{{-8, 3, -8}},{{-5, 1, -5}},{{-2, 0, -1}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -6}},{{-7, 2, -9}},{{-7, 2, -8}},{{-4, 1, -4}},{{-2, 0, -1}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -6}},{{-6, 2, -9}},{{-7, 2, -8}},{{-4, 1, -4}},{{-2, 0, -1}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 6, -16}},{{-3, 3, -6}},{{-6, 2, -7}},{{-7, 2, -8}},{{-4, 1, -4}},{{-2, 0, -1}},{{-1, 0, -2}},{{-1, 0, -2}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -17}},{{-3, 3, -6}},{{-6, 2, -7}},{{-6, 1, -8}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -17}},{{-3, 3, -6}},{{-6, 2, -7}},{{-6, 1, -8}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -17}},{{-3, 3, -6}},{{-6, 2, -7}},{{-6, 1, -8}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -17}},{{-3, 3, -6}},{{-6, 2, -7}},{{-6, 1, -8}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -2}},{{-1, 0, -1}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -18}},{{-3, 3, -6}},{{-6, 2, -6}},{{-6, 1, -8}},{{-3, 1, -2}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -18}},{{-3, 3, -6}},{{-6, 2, -6}},{{-6, 1, -7}},{{-2, 1, -2}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -18}},{{-3, 3, -6}},{{-6, 2, -5}},{{-5, 1, -6}},{{-2, 1, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -18}},{{-3, 2, -6}},{{-6, 2, -4}},{{-5, 1, -6}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -19}},{{-3, 2, -5}},{{-5, 2, -3}},{{-4, 1, -5}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -19}},{{-3, 2, -5}},{{-5, 2, -2}},{{-3, 1, -4}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -19}},{{-3, 2, -5}},{{-5, 2, -2}},{{-2, 1, -4}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -19}},{{-3, 2, -5}},{{-5, 1, -1}},{{-2, 1, -4}},{{-2, 0, -1}},{{0, 0, 0}},{{-1, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -20}},{{-4, 2, -4}},{{-5, 1, -1}},{{-2, 1, -4}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, -2}},{{0, 0, 0}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 5, -20}},{{-5, 1, -4}},{{-5, 1, -1}},{{-1, 1, -4}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, 0}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 5, -20}},{{-5, 1, -4}},{{-3, 2, -1}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 5, -19}},{{-5, 1, -4}},{{-3, 2, -2}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 5, -18}},{{-5, 0, -4}},{{-3, 2, -2}},{{-2, 0, -4}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -18}},{{-5, 0, -4}},{{-3, 2, -3}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -17}},{{-5, 0, -3}},{{-3, 2, -3}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 5, -16}},{{-5, 0, -3}},{{-3, 2, -4}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 5, -16}},{{-5, 0, -3}},{{-3, 2, -4}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 5, -16}},{{-5, 0, -3}},{{-2, 2, -5}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 5, -16}},{{-5, 0, -3}},{{-2, 1, -5}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 5, -15}},{{-5, 0, -3}},{{-2, 0, -5}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 5, -15}},{{-5, 0, -3}},{{-3, 0, -5}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -14}},{{-5, 0, -3}},{{-3, 0, -4}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -14}},{{-4, 0, -3}},{{-3, 0, -4}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 4, -14}},{{-3, 0, -4}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 3, -12}},{{-3, 0, -2}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 3, -12}},{{-3, 0, -1}},{{-3, 0, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 3, -11}},{{-3, 0, 0}},{{-3, 0, -3}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 2, -9}},{{-3, 0, 0}},{{-3, 0, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -7}},{{-3, 0, 0}},{{-3, 0, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -5}},{{-3, 0, 0}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, 0}},{{0, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_350NT */
};

static const struct rgb_t offset_color_revA[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -1}},{{-2, 2, -6}},{{-7, 4, -10}},{{-8, 5, -12}},{{-4, 2, -3}},{{-1, 2, -4}},{{-3, 5, -4}}}, /* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 1, -2}},{{0, 2, -2}},{{-2, 2, -7}},{{-7, 4, -10}},{{-8, 4, -9}},{{-3, 2, -3}},{{-2, 1, -4}},{{-3, 4, -4}}}, /* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 2, -2}},{{-1, 2, -5}},{{-2, 1, -7}},{{-7, 4, -9}},{{-8, 4, -7}},{{-3, 2, -3}},{{-2, 0, -4}},{{-3, 2, -4}}}, /* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -2}},{{-1, 2, -5}},{{-3, 1, -9}},{{-7, 3, -9}},{{-7, 3, -7}},{{-3, 1, -3}},{{-3, 0, -5}},{{-3, 1, -3}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -2}},{{-1, 3, -6}},{{-4, 1, -9}},{{-7, 3, -9}},{{-6, 2, -7}},{{-2, 0, -2}},{{-3, 0, -4}},{{-2, 0, -3}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -3}},{{-1, 3, -6}},{{-6, 1, -8}},{{-7, 3, -9}},{{-6, 2, -6}},{{-2, 0, -2}},{{-3, 0, -4}},{{-2, 0, -3}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -4}},{{-1, 3, -6}},{{-7, 1, -8}},{{-7, 3, -9}},{{-5, 1, -6}},{{-2, 0, -2}},{{-3, 0, -3}},{{-2, 0, -3}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 4, -6}},{{0, 3, -6}},{{-7, 1, -8}},{{-7, 3, -9}},{{-4, 0, -6}},{{-2, 0, -2}},{{-3, 0, -2}},{{-2, 0, -3}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 5, -8}},{{0, 3, -6}},{{-7, 1, -7}},{{-7, 3, -9}},{{-4, 0, -6}},{{-2, 0, -2}},{{-3, 0, -2}},{{-2, 0, -3}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 6, -8}},{{0, 3, -6}},{{-6, 2, -6}},{{-6, 2, -8}},{{-4, 0, -6}},{{-2, 0, -2}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 6, -9}},{{0, 3, -6}},{{-6, 2, -6}},{{-6, 2, -8}},{{-4, 0, -5}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 6, -10}},{{-1, 3, -7}},{{-5, 2, -6}},{{-6, 2, -7}},{{-4, 0, -5}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 6, -10}},{{-1, 3, -7}},{{-4, 2, -6}},{{-6, 2, -6}},{{-4, 0, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 6, -11}},{{-1, 3, -7}},{{-3, 2, -5}},{{-6, 2, -6}},{{-4, 0, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 6, -11}},{{-1, 3, -7}},{{-3, 2, -5}},{{-6, 2, -6}},{{-4, 0, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 7, -11}},{{-1, 3, -7}},{{-3, 2, -5}},{{-6, 2, -6}},{{-4, 0, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-6, 2, -5}},{{-3, 0, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-6, 2, -5}},{{-3, 0, -4}},{{-1, 0, -1}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-6, 2, -5}},{{-2, 0, -4}},{{0, 0, 0}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 7, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-6, 2, -5}},{{-2, 0, -4}},{{0, 0, 0}},{{-2, 0, -2}},{{-2, 0, -2}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 7, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-5, 1, -5}},{{-2, 0, -3}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 7, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-5, 1, -5}},{{-2, 0, -3}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{4, 7, -11}},{{-2, 3, -7}},{{-3, 2, -5}},{{-5, 1, -5}},{{-2, 0, -3}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 3, -7}},{{-3, 2, -7}},{{-4, 1, -4}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 3, -7}},{{-3, 2, -7}},{{-4, 1, -3}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 3, -7}},{{-3, 2, -6}},{{-4, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -2}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 3, -7}},{{-3, 2, -6}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 3, -7}},{{-2, 2, -5}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 2, -7}},{{-2, 2, -4}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 2, -7}},{{-2, 2, -4}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 2, -6}},{{-2, 2, -4}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 2, -6}},{{-2, 2, -4}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 1, -5}},{{-1, 1, -3}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 1, -5}},{{-1, 1, -3}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -11}},{{-2, 1, -5}},{{-1, 1, -3}},{{-3, 1, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{-1, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -10}},{{-2, 0, -5}},{{-1, 1, -2}},{{-2, 0, -2}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-2, 0, 0}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{5, 7, -10}},{{-3, 0, -5}},{{-1, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 7, -12}},{{-3, 1, -5}},{{-1, 0, -2}},{{0, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 7, -12}},{{-3, 1, -5}},{{-1, 0, -2}},{{0, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-3, 1, -4}},{{-1, 0, -1}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 5, -11}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 5, -11}},{{-3, 1, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{-2, 0, -2}},{{0, 0, 0}},{{0, 0, -1}},{{-1, 0, 0}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 4, -10}},{{-2, 1, -2}},{{-1, 0, 0}},{{-2, 0, -1}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 4, -10}},{{-2, 1, -2}},{{-1, 0, 0}},{{-2, 0, -1}},{{-2, 0, -3}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 3, -9}},{{-2, 1, -2}},{{-1, 0, -1}},{{-2, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 3, -9}},{{-2, 1, -2}},{{-1, 0, -1}},{{-2, 0, -1}},{{-1, 0, -2}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 3, -9}},{{-2, 1, -1}},{{-1, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -8}},{{-2, 1, 0}},{{-1, 0, -2}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 3, -8}},{{-2, 1, 0}},{{-1, 0, -2}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -6}},{{-3, 0, -1}},{{0, 0, -1}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -6}},{{-3, 0, -1}},{{0, 0, -1}},{{-1, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -6}},{{-2, -1, 0}},{{0, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 1, -6}},{{-2, -1, 0}},{{0, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 2, -5}},{{-2, -1, 1}},{{0, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 1, -4}},{{-2, -1, 1}},{{0, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -2}},{{-1, -1, 2}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_350NT */
};
#endif /* __DYNAMIC_AID_XXXX_H */
