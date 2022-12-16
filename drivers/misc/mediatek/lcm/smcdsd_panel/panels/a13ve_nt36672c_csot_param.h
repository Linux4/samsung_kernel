/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NT36672C_CSOT_PARAM_H__
#define __NT36672C_CSOT_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define LCD_TYPE_VENDOR		"CSO"

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define LDI_REG_BRIGHTNESS	0x51
#define LDI_REG_ID		0xDA
#define LDI_LEN_BRIGHTNESS	((u16)ARRAY_SIZE(SEQ_NT36672C_CSOT_BRIGHTNESS))

/* len is read length */
#define LDI_LEN_ID		3

#define TYPE_WRITE		I2C_SMBUS_WRITE
#define TYPE_DELAY		U8_MAX

static u8 KTZ8864_INIT[] = {
	TYPE_WRITE, 0x0C, 0x24,
	TYPE_WRITE, 0x0D, 0x1E,
	TYPE_WRITE, 0x0E, 0x1E,
	TYPE_WRITE, 0x09, 0x99,
	TYPE_WRITE, 0x02, 0x6B,
	TYPE_WRITE, 0x03, 0x0D,
	TYPE_WRITE, 0x11, 0x74,
	TYPE_WRITE, 0x04, 0x03,
	TYPE_WRITE, 0x05, 0xC2,
	TYPE_WRITE, 0x10, 0x66,
	TYPE_WRITE, 0x08, 0x13,
};

static u8 KTZ8864_EXIT[] = {
	TYPE_WRITE, 0x09, 0x9C,
	TYPE_WRITE, 0x09, 0x18,
	TYPE_DELAY, 1, 0,
	TYPE_WRITE, 0x08, 0x00,
};

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static unsigned char SEQ_NT36672C_CSOT_SLEEP_OUT[] = { 0x11 };
static unsigned char SEQ_NT36672C_CSOT_SLEEP_IN[] = { 0x10 };
static unsigned char SEQ_NT36672C_CSOT_DISPLAY_OFF[] = { 0x28 };
static unsigned char SEQ_NT36672C_CSOT_DISPLAY_ON[] = { 0x29 };

static unsigned char SEQ_NT36672C_CSOT_BRIGHTNESS[] = {
	0x51,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_001[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_002[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_003[] = {
	0xB0,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_004[] = {
	0xC1,
	0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x2B,
	0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7,
};

static unsigned char SEQ_NT36672C_CSOT_005[] = {
	0xC2,
	0x1B, 0xA0,
};

static unsigned char SEQ_NT36672C_CSOT_006[] = {
	0xFF,
	0x20,
};

static unsigned char SEQ_NT36672C_CSOT_007[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_008[] = {
	0x01,
	0x66,
};

static unsigned char SEQ_NT36672C_CSOT_009[] = {
	0x06,
	0x64,
};

static unsigned char SEQ_NT36672C_CSOT_010[] = {
	0x07,
	0x28,
};

static unsigned char SEQ_NT36672C_CSOT_011[] = {
	0x17,
	0x66,
};

static unsigned char SEQ_NT36672C_CSOT_012[] = {
	0x1B,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_013[] = {
	0x1F,
	0x02,
};

static unsigned char SEQ_NT36672C_CSOT_014[] = {
	0x20,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_015[] = {
	0x5C,
	0x90,
};

static unsigned char SEQ_NT36672C_CSOT_016[] = {
	0x5E,
	0x8B,
};

static unsigned char SEQ_NT36672C_CSOT_017[] = {
	0x69,
	0xD0,
};

static unsigned char SEQ_NT36672C_CSOT_018[] = {
	0x95,
	0xD1,
};

static unsigned char SEQ_NT36672C_CSOT_019[] = {
	0x96,
	0xD1,
};

static unsigned char SEQ_NT36672C_CSOT_020[] = {
	0xF2,
	0x66,
};

static unsigned char SEQ_NT36672C_CSOT_021[] = {
	0xF3,
	0x54,
};

static unsigned char SEQ_NT36672C_CSOT_022[] = {
	0xF4,
	0x66,
};

static unsigned char SEQ_NT36672C_CSOT_023[] = {
	0xF5,
	0x54,
};

static unsigned char SEQ_NT36672C_CSOT_024[] = {
	0xF6,
	0x66,
};

static unsigned char SEQ_NT36672C_CSOT_025[] = {
	0xF7,
	0x54,
};

static unsigned char SEQ_NT36672C_CSOT_026[] = {
	0xF8,
	0x66,
};

static unsigned char SEQ_NT36672C_CSOT_027[] = {
	0xF9,
	0x54,
};

static unsigned char SEQ_NT36672C_CSOT_028[] = {
	0xFF,
	0x21,
};

static unsigned char SEQ_NT36672C_CSOT_029[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_030[] = {
	0xFF,
	0x24,
};

static unsigned char SEQ_NT36672C_CSOT_031[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_032[] = {
	0x00,
	0x26,
};

static unsigned char SEQ_NT36672C_CSOT_033[] = {
	0x01,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_034[] = {
	0x02,
	0x27,
};

static unsigned char SEQ_NT36672C_CSOT_035[] = {
	0x03,
	0x15,
};

static unsigned char SEQ_NT36672C_CSOT_036[] = {
	0x04,
	0x28,
};

static unsigned char SEQ_NT36672C_CSOT_037[] = {
	0x05,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_038[] = {
	0x07,
	0x24,
};

static unsigned char SEQ_NT36672C_CSOT_039[] = {
	0x08,
	0x24,
};

static unsigned char SEQ_NT36672C_CSOT_040[] = {
	0x0A,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_041[] = {
	0x0C,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_042[] = {
	0x0D,
	0x0F,
};

static unsigned char SEQ_NT36672C_CSOT_043[] = {
	0x0E,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_044[] = {
	0x10,
	0x2D,
};

static unsigned char SEQ_NT36672C_CSOT_045[] = {
	0x11,
	0x2F,
};

static unsigned char SEQ_NT36672C_CSOT_046[] = {
	0x12,
	0x31,
};

static unsigned char SEQ_NT36672C_CSOT_047[] = {
	0x13,
	0x33,
};

static unsigned char SEQ_NT36672C_CSOT_048[] = {
	0x15,
	0x0B,
};

static unsigned char SEQ_NT36672C_CSOT_049[] = {
	0x17,
	0x0C,
};

static unsigned char SEQ_NT36672C_CSOT_050[] = {
	0x18,
	0x26,
};

static unsigned char SEQ_NT36672C_CSOT_051[] = {
	0x19,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_052[] = {
	0x1A,
	0x27,
};

static unsigned char SEQ_NT36672C_CSOT_053[] = {
	0x1B,
	0x15,
};

static unsigned char SEQ_NT36672C_CSOT_054[] = {
	0x1C,
	0x28,
};

static unsigned char SEQ_NT36672C_CSOT_055[] = {
	0x1D,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_056[] = {
	0x1F,
	0x24,
};

static unsigned char SEQ_NT36672C_CSOT_057[] = {
	0x20,
	0x24,
};

static unsigned char SEQ_NT36672C_CSOT_058[] = {
	0x22,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_059[] = {
	0x24,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_060[] = {
	0x25,
	0x0F,
};

static unsigned char SEQ_NT36672C_CSOT_061[] = {
	0x26,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_062[] = {
	0x28,
	0x2C,
};

static unsigned char SEQ_NT36672C_CSOT_063[] = {
	0x29,
	0x2E,
};

static unsigned char SEQ_NT36672C_CSOT_064[] = {
	0x2A,
	0x30,
};

static unsigned char SEQ_NT36672C_CSOT_065[] = {
	0x2B,
	0x32,
};

static unsigned char SEQ_NT36672C_CSOT_066[] = {
	0x2F,
	0x0B,
};

static unsigned char SEQ_NT36672C_CSOT_067[] = {
	0x31,
	0x0C,
};

static unsigned char SEQ_NT36672C_CSOT_068[] = {
	0x32,
	0x09,
};

static unsigned char SEQ_NT36672C_CSOT_069[] = {
	0x33,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_070[] = {
	0x34,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_071[] = {
	0x35,
	0x07,
};

static unsigned char SEQ_NT36672C_CSOT_072[] = {
	0x36,
	0x3C,
};

static unsigned char SEQ_NT36672C_CSOT_073[] = {
	0x4E,
	0x37,
};

static unsigned char SEQ_NT36672C_CSOT_074[] = {
	0x4F,
	0x37,
};

static unsigned char SEQ_NT36672C_CSOT_075[] = {
	0x53,
	0x37,
};

static unsigned char SEQ_NT36672C_CSOT_076[] = {
	0x77,
	0x80,
};

static unsigned char SEQ_NT36672C_CSOT_077[] = {
	0x79,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_078[] = {
	0x7A,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_079[] = {
	0x7B,
	0x8E,
};

static unsigned char SEQ_NT36672C_CSOT_080[] = {
	0x7D,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_081[] = {
	0x80,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_082[] = {
	0x81,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_083[] = {
	0x82,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_084[] = {
	0x84,
	0x31,
};

static unsigned char SEQ_NT36672C_CSOT_085[] = {
	0x85,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_086[] = {
	0x86,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_087[] = {
	0x87,
	0x31,
};

static unsigned char SEQ_NT36672C_CSOT_088[] = {
	0x90,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_089[] = {
	0x92,
	0x31,
};

static unsigned char SEQ_NT36672C_CSOT_090[] = {
	0x93,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_091[] = {
	0x94,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_092[] = {
	0x95,
	0x31,
};

static unsigned char SEQ_NT36672C_CSOT_093[] = {
	0x9C,
	0xF4,
};

static unsigned char SEQ_NT36672C_CSOT_094[] = {
	0x9D,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_095[] = {
	0xA0,
	0x0E,
};

static unsigned char SEQ_NT36672C_CSOT_096[] = {
	0xA2,
	0x0E,
};

static unsigned char SEQ_NT36672C_CSOT_097[] = {
	0xA3,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_098[] = {
	0xA4,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_099[] = {
	0xA5,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_100[] = {
	0xC4,
	0x80,
};

static unsigned char SEQ_NT36672C_CSOT_101[] = {
	0xC6,
	0xC0,
};

static unsigned char SEQ_NT36672C_CSOT_102[] = {
	0xC9,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_103[] = {
	0xD9,
	0x80,
};

static unsigned char SEQ_NT36672C_CSOT_104[] = {
	0xE9,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_105[] = {
	0xFF,
	0x25,
};

static unsigned char SEQ_NT36672C_CSOT_106[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_107[] = {
	0x0F,
	0x1B,
};

static unsigned char SEQ_NT36672C_CSOT_108[] = {
	0x18,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_109[] = {
	0x19,
	0xE4,
};

static unsigned char SEQ_NT36672C_CSOT_110[] = {
	0x20,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_111[] = {
	0x21,
	0x40,
};

static unsigned char SEQ_NT36672C_CSOT_112[] = {
	0x63,
	0x8F,
};

static unsigned char SEQ_NT36672C_CSOT_113[] = {
	0x66,
	0x5D,
};

static unsigned char SEQ_NT36672C_CSOT_114[] = {
	0x67,
	0x16,
};

static unsigned char SEQ_NT36672C_CSOT_115[] = {
	0x68,
	0x58,
};

static unsigned char SEQ_NT36672C_CSOT_116[] = {
	0x69,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_117[] = {
	0x6B,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_118[] = {
	0x70,
	0xE5,
};

static unsigned char SEQ_NT36672C_CSOT_119[] = {
	0x71,
	0x6D,
};

static unsigned char SEQ_NT36672C_CSOT_120[] = {
	0x77,
	0x62,
};

static unsigned char SEQ_NT36672C_CSOT_121[] = {
	0x7E,
	0x2D,
};

static unsigned char SEQ_NT36672C_CSOT_122[] = {
	0x84,
	0x78,
};

static unsigned char SEQ_NT36672C_CSOT_123[] = {
	0x85,
	0x75,
};

static unsigned char SEQ_NT36672C_CSOT_124[] = {
	0x8D,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_125[] = {
	0xC1,
	0xA9,
};

static unsigned char SEQ_NT36672C_CSOT_126[] = {
	0xC2,
	0x5A,
};

static unsigned char SEQ_NT36672C_CSOT_127[] = {
	0xC3,
	0x07,
};

static unsigned char SEQ_NT36672C_CSOT_128[] = {
	0xC4,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_129[] = {
	0xC6,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_130[] = {
	0xF0,
	0x05,
};

static unsigned char SEQ_NT36672C_CSOT_131[] = {
	0xEF,
	0x28,
};

static unsigned char SEQ_NT36672C_CSOT_132[] = {
	0xF1,
	0x14,
};

static unsigned char SEQ_NT36672C_CSOT_133[] = {
	0xFF,
	0x26,
};

static unsigned char SEQ_NT36672C_CSOT_134[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_135[] = {
	0x00,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_136[] = {
	0x01,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_137[] = {
	0x03,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_138[] = {
	0x04,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_139[] = {
	0x05,
	0x08,
};

static unsigned char SEQ_NT36672C_CSOT_140[] = {
	0x06,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_141[] = {
	0x08,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_142[] = {
	0x14,
	0x06,
};

static unsigned char SEQ_NT36672C_CSOT_143[] = {
	0x15,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_144[] = {
	0x74,
	0xAF,
};

static unsigned char SEQ_NT36672C_CSOT_145[] = {
	0x81,
	0x0E,
};

static unsigned char SEQ_NT36672C_CSOT_146[] = {
	0x83,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_147[] = {
	0x84,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_148[] = {
	0x85,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_149[] = {
	0x86,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_150[] = {
	0x87,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_151[] = {
	0x88,
	0x07,
};

static unsigned char SEQ_NT36672C_CSOT_152[] = {
	0x8A,
	0x1A,
};

static unsigned char SEQ_NT36672C_CSOT_153[] = {
	0x8B,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_154[] = {
	0x8C,
	0x24,
};

static unsigned char SEQ_NT36672C_CSOT_155[] = {
	0x8E,
	0x42,
};

static unsigned char SEQ_NT36672C_CSOT_156[] = {
	0x8F,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_157[] = {
	0x90,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_158[] = {
	0x91,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_159[] = {
	0x9A,
	0x80,
};

static unsigned char SEQ_NT36672C_CSOT_160[] = {
	0x9B,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_161[] = {
	0x9C,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_162[] = {
	0x9D,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_163[] = {
	0x9E,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_164[] = {
	0xFF,
	0x27,
};

static unsigned char SEQ_NT36672C_CSOT_165[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_166[] = {
	0x01,
	0x68,
};

static unsigned char SEQ_NT36672C_CSOT_167[] = {
	0x20,
	0x81,
};

static unsigned char SEQ_NT36672C_CSOT_168[] = {
	0x21,
	0x74,
};

static unsigned char SEQ_NT36672C_CSOT_169[] = {
	0x25,
	0x81,
};

static unsigned char SEQ_NT36672C_CSOT_170[] = {
	0x26,
	0x9D,
};

static unsigned char SEQ_NT36672C_CSOT_171[] = {
	0x6E,
	0x12,
};

static unsigned char SEQ_NT36672C_CSOT_172[] = {
	0x6F,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_173[] = {
	0x70,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_174[] = {
	0x71,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_175[] = {
	0x72,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_176[] = {
	0x73,
	0x76,
};

static unsigned char SEQ_NT36672C_CSOT_177[] = {
	0x74,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_178[] = {
	0x75,
	0x32,
};

static unsigned char SEQ_NT36672C_CSOT_179[] = {
	0x76,
	0x54,
};

static unsigned char SEQ_NT36672C_CSOT_180[] = {
	0x77,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_181[] = {
	0x7D,
	0x09,
};

static unsigned char SEQ_NT36672C_CSOT_182[] = {
	0x7E,
	0x6B,
};

static unsigned char SEQ_NT36672C_CSOT_183[] = {
	0x80,
	0x27,
};

static unsigned char SEQ_NT36672C_CSOT_184[] = {
	0x82,
	0x09,
};

static unsigned char SEQ_NT36672C_CSOT_185[] = {
	0x83,
	0x6B,
};

static unsigned char SEQ_NT36672C_CSOT_186[] = {
	0x88,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_187[] = {
	0x89,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_188[] = {
	0xE3,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_189[] = {
	0xE4,
	0xF1,
};

static unsigned char SEQ_NT36672C_CSOT_190[] = {
	0xE5,
	0x02,
};

static unsigned char SEQ_NT36672C_CSOT_191[] = {
	0xE6,
	0xE9,
};

static unsigned char SEQ_NT36672C_CSOT_192[] = {
	0xE9,
	0x02,
};

static unsigned char SEQ_NT36672C_CSOT_193[] = {
	0xEA,
	0x26,
};

static unsigned char SEQ_NT36672C_CSOT_194[] = {
	0xEB,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_195[] = {
	0xEC,
	0x39,
};

static unsigned char SEQ_NT36672C_CSOT_196[] = {
	0xFF,
	0x2A,
};

static unsigned char SEQ_NT36672C_CSOT_197[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_198[] = {
	0x00,
	0x91,
};

static unsigned char SEQ_NT36672C_CSOT_199[] = {
	0x03,
	0x20,
};

static unsigned char SEQ_NT36672C_CSOT_200[] = {
	0x06,
	0x06,
};

static unsigned char SEQ_NT36672C_CSOT_201[] = {
	0x07,
	0x50,
};

static unsigned char SEQ_NT36672C_CSOT_202[] = {
	0x0A,
	0x60,
};

static unsigned char SEQ_NT36672C_CSOT_203[] = {
	0x0C,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_204[] = {
	0x0D,
	0x40,
};

static unsigned char SEQ_NT36672C_CSOT_205[] = {
	0x0F,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_206[] = {
	0x11,
	0xE1,
};

static unsigned char SEQ_NT36672C_CSOT_207[] = {
	0x15,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_208[] = {
	0x16,
	0xCF,
};

static unsigned char SEQ_NT36672C_CSOT_209[] = {
	0x19,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_210[] = {
	0x1A,
	0xA3,
};

static unsigned char SEQ_NT36672C_CSOT_211[] = {
	0x1B,
	0x0C,
};

static unsigned char SEQ_NT36672C_CSOT_212[] = {
	0x1D,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_213[] = {
	0x1E,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_214[] = {
	0x1F,
	0x48,
};

static unsigned char SEQ_NT36672C_CSOT_215[] = {
	0x20,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_216[] = {
	0x28,
	0xFD,
};

static unsigned char SEQ_NT36672C_CSOT_217[] = {
	0x29,
	0x0B,
};

static unsigned char SEQ_NT36672C_CSOT_218[] = {
	0x2A,
	0x1B,
};

static unsigned char SEQ_NT36672C_CSOT_219[] = {
	0x2D,
	0x0B,
};

static unsigned char SEQ_NT36672C_CSOT_220[] = {
	0x2F,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_221[] = {
	0x30,
	0x85,
};

static unsigned char SEQ_NT36672C_CSOT_222[] = {
	0x31,
	0xB4,
};

static unsigned char SEQ_NT36672C_CSOT_223[] = {
	0x33,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_224[] = {
	0x34,
	0xFF,
};

static unsigned char SEQ_NT36672C_CSOT_225[] = {
	0x35,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_226[] = {
	0x36,
	0x05,
};

static unsigned char SEQ_NT36672C_CSOT_227[] = {
	0x37,
	0xF9,
};

static unsigned char SEQ_NT36672C_CSOT_228[] = {
	0x38,
	0x44,
};

static unsigned char SEQ_NT36672C_CSOT_229[] = {
	0x39,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_230[] = {
	0x3A,
	0x85,
};

static unsigned char SEQ_NT36672C_CSOT_231[] = {
	0x45,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_232[] = {
	0x46,
	0x40,
};

static unsigned char SEQ_NT36672C_CSOT_233[] = {
	0x48,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_234[] = {
	0x4A,
	0xE1,
};

static unsigned char SEQ_NT36672C_CSOT_235[] = {
	0x4E,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_236[] = {
	0x4F,
	0xCF,
};

static unsigned char SEQ_NT36672C_CSOT_237[] = {
	0x52,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_238[] = {
	0x53,
	0xA3,
};

static unsigned char SEQ_NT36672C_CSOT_239[] = {
	0x54,
	0x0C,
};

static unsigned char SEQ_NT36672C_CSOT_240[] = {
	0x56,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_241[] = {
	0x57,
	0x57,
};

static unsigned char SEQ_NT36672C_CSOT_242[] = {
	0x58,
	0x61,
};

static unsigned char SEQ_NT36672C_CSOT_243[] = {
	0x59,
	0x57,
};

static unsigned char SEQ_NT36672C_CSOT_244[] = {
	0x7A,
	0x09,
};

static unsigned char SEQ_NT36672C_CSOT_245[] = {
	0x7B,
	0x40,
};

static unsigned char SEQ_NT36672C_CSOT_246[] = {
	0x7F,
	0xF0,
};

static unsigned char SEQ_NT36672C_CSOT_247[] = {
	0x83,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_248[] = {
	0x84,
	0xCF,
};

static unsigned char SEQ_NT36672C_CSOT_249[] = {
	0x87,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_250[] = {
	0x88,
	0xA3,
};

static unsigned char SEQ_NT36672C_CSOT_251[] = {
	0x89,
	0x0C,
};

static unsigned char SEQ_NT36672C_CSOT_252[] = {
	0x8B,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_253[] = {
	0x8C,
	0x7E,
};

static unsigned char SEQ_NT36672C_CSOT_254[] = {
	0x8D,
	0x7E,
};

static unsigned char SEQ_NT36672C_CSOT_255[] = {
	0x8E,
	0x7E,
};

static unsigned char SEQ_NT36672C_CSOT_256[] = {
	0xFF,
	0x2C,
};

static unsigned char SEQ_NT36672C_CSOT_257[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_258[] = {
	0x03,
	0x15,
};

static unsigned char SEQ_NT36672C_CSOT_259[] = {
	0x04,
	0x15,
};

static unsigned char SEQ_NT36672C_CSOT_260[] = {
	0x05,
	0x15,
};

static unsigned char SEQ_NT36672C_CSOT_261[] = {
	0x0D,
	0x06,
};

static unsigned char SEQ_NT36672C_CSOT_262[] = {
	0x0E,
	0x56,
};

static unsigned char SEQ_NT36672C_CSOT_263[] = {
	0x17,
	0x4E,
};

static unsigned char SEQ_NT36672C_CSOT_264[] = {
	0x18,
	0x4E,
};

static unsigned char SEQ_NT36672C_CSOT_265[] = {
	0x19,
	0x4E,
};

static unsigned char SEQ_NT36672C_CSOT_266[] = {
	0x2D,
	0xAF,
};

static unsigned char SEQ_NT36672C_CSOT_267[] = {
	0x2F,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_268[] = {
	0x30,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_269[] = {
	0x32,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_270[] = {
	0x33,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_271[] = {
	0x35,
	0x19,
};

static unsigned char SEQ_NT36672C_CSOT_272[] = {
	0x37,
	0x19,
};

static unsigned char SEQ_NT36672C_CSOT_273[] = {
	0x4D,
	0x15,
};

static unsigned char SEQ_NT36672C_CSOT_274[] = {
	0x4E,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_275[] = {
	0x4F,
	0x09,
};

static unsigned char SEQ_NT36672C_CSOT_276[] = {
	0x56,
	0x1B,
};

static unsigned char SEQ_NT36672C_CSOT_277[] = {
	0x58,
	0x1B,
};

static unsigned char SEQ_NT36672C_CSOT_278[] = {
	0x59,
	0x1B,
};

static unsigned char SEQ_NT36672C_CSOT_279[] = {
	0x62,
	0x6D,
};

static unsigned char SEQ_NT36672C_CSOT_280[] = {
	0x6B,
	0x6A,
};

static unsigned char SEQ_NT36672C_CSOT_281[] = {
	0x6C,
	0x6A,
};

static unsigned char SEQ_NT36672C_CSOT_282[] = {
	0x6D,
	0x6A,
};

static unsigned char SEQ_NT36672C_CSOT_283[] = {
	0x80,
	0xAF,
};

static unsigned char SEQ_NT36672C_CSOT_284[] = {
	0x81,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_285[] = {
	0x82,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_286[] = {
	0x84,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_287[] = {
	0x85,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_288[] = {
	0x87,
	0x20,
};

static unsigned char SEQ_NT36672C_CSOT_289[] = {
	0x89,
	0x20,
};

static unsigned char SEQ_NT36672C_CSOT_290[] = {
	0x9E,
	0x04,
};

static unsigned char SEQ_NT36672C_CSOT_291[] = {
	0x9F,
	0x1E,
};

static unsigned char SEQ_NT36672C_CSOT_292[] = {
	0xFF,
	0xE0,
};

static unsigned char SEQ_NT36672C_CSOT_293[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_294[] = {
	0x35,
	0x82,
};

static unsigned char SEQ_NT36672C_CSOT_295[] = {
	0xFF,
	0xF0,
};

static unsigned char SEQ_NT36672C_CSOT_296[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_297[] = {
	0x1C,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_298[] = {
	0x33,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_299[] = {
	0x5A,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_300[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36672C_CSOT_301[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_302[] = {
	0x53,
	0x22,
};

static unsigned char SEQ_NT36672C_CSOT_303[] = {
	0x54,
	0x02,
};

static unsigned char SEQ_NT36672C_CSOT_304[] = {
	0xFF,
	0xC0,
};

static unsigned char SEQ_NT36672C_CSOT_305[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_306[] = {
	0x9C,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_307[] = {
	0x9D,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_308[] = {
	0xFF,
	0x2B,
};

static unsigned char SEQ_NT36672C_CSOT_309[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_310[] = {
	0xB7,
	0x23,
};

static unsigned char SEQ_NT36672C_CSOT_311[] = {
	0xB8,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_312[] = {
	0xC0,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_313[] = {
	0xFF,
	0xF0,
};

static unsigned char SEQ_NT36672C_CSOT_314[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_315[] = {
	0xD2,
	0x50,
};

static unsigned char SEQ_NT36672C_CSOT_316[] = {
	0xFF,
	0x23,
};

static unsigned char SEQ_NT36672C_CSOT_317[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_318[] = {
	0x00,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_319[] = {
	0x07,
	0x60,
};

static unsigned char SEQ_NT36672C_CSOT_320[] = {
	0x08,
	0x06,
};

static unsigned char SEQ_NT36672C_CSOT_321[] = {
	0x09,
	0x1C,
};

static unsigned char SEQ_NT36672C_CSOT_322[] = {
	0x0A,
	0x2B,
};

static unsigned char SEQ_NT36672C_CSOT_323[] = {
	0x0B,
	0x2B,
};

static unsigned char SEQ_NT36672C_CSOT_324[] = {
	0x0C,
	0x2B,
};

static unsigned char SEQ_NT36672C_CSOT_325[] = {
	0x0D,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_326[] = {
	0x10,
	0x50,
};

static unsigned char SEQ_NT36672C_CSOT_327[] = {
	0x11,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_328[] = {
	0x12,
	0x95,
};

static unsigned char SEQ_NT36672C_CSOT_329[] = {
	0x15,
	0x68,
};

static unsigned char SEQ_NT36672C_CSOT_330[] = {
	0x16,
	0x0B,
};

static unsigned char SEQ_NT36672C_CSOT_331[] = {
	0x19,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_332[] = {
	0x1A,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_333[] = {
	0x1B,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_334[] = {
	0x1C,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_335[] = {
	0x1D,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_336[] = {
	0x1E,
	0x03,
};

static unsigned char SEQ_NT36672C_CSOT_337[] = {
	0x1F,
	0x05,
};

static unsigned char SEQ_NT36672C_CSOT_338[] = {
	0x20,
	0x0C,
};

static unsigned char SEQ_NT36672C_CSOT_339[] = {
	0x21,
	0x13,
};

static unsigned char SEQ_NT36672C_CSOT_340[] = {
	0x22,
	0x17,
};

static unsigned char SEQ_NT36672C_CSOT_341[] = {
	0x23,
	0x1D,
};

static unsigned char SEQ_NT36672C_CSOT_342[] = {
	0x24,
	0x23,
};

static unsigned char SEQ_NT36672C_CSOT_343[] = {
	0x25,
	0x2C,
};

static unsigned char SEQ_NT36672C_CSOT_344[] = {
	0x26,
	0x33,
};

static unsigned char SEQ_NT36672C_CSOT_345[] = {
	0x27,
	0x39,
};

static unsigned char SEQ_NT36672C_CSOT_346[] = {
	0x28,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_347[] = {
	0x29,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_348[] = {
	0x2A,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_349[] = {
	0x2B,
	0x3F,
};

static unsigned char SEQ_NT36672C_CSOT_350[] = {
	0x30,
	0xFF,
};

static unsigned char SEQ_NT36672C_CSOT_351[] = {
	0x31,
	0xFE,
};

static unsigned char SEQ_NT36672C_CSOT_352[] = {
	0x32,
	0xFD,
};

static unsigned char SEQ_NT36672C_CSOT_353[] = {
	0x33,
	0xFC,
};

static unsigned char SEQ_NT36672C_CSOT_354[] = {
	0x34,
	0xFB,
};

static unsigned char SEQ_NT36672C_CSOT_355[] = {
	0x35,
	0xFA,
};

static unsigned char SEQ_NT36672C_CSOT_356[] = {
	0x36,
	0xF9,
};

static unsigned char SEQ_NT36672C_CSOT_357[] = {
	0x37,
	0xF7,
};

static unsigned char SEQ_NT36672C_CSOT_358[] = {
	0x38,
	0xF5,
};

static unsigned char SEQ_NT36672C_CSOT_359[] = {
	0x39,
	0xF3,
};

static unsigned char SEQ_NT36672C_CSOT_360[] = {
	0x3A,
	0xF1,
};

static unsigned char SEQ_NT36672C_CSOT_361[] = {
	0x3B,
	0xEE,
};

static unsigned char SEQ_NT36672C_CSOT_362[] = {
	0x3D,
	0xEC,
};

static unsigned char SEQ_NT36672C_CSOT_363[] = {
	0x3F,
	0xEA,
};

static unsigned char SEQ_NT36672C_CSOT_364[] = {
	0x40,
	0xE8,
};

static unsigned char SEQ_NT36672C_CSOT_365[] = {
	0x41,
	0xE6,
};

static unsigned char SEQ_NT36672C_CSOT_366[] = {
	0x04,
	0x00,
};

static unsigned char SEQ_NT36672C_CSOT_367[] = {
	0xA0,
	0x11,
};

static unsigned char SEQ_NT36672C_CSOT_368[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36672C_CSOT_369[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_370[] = {
	0x25,
	0xA9,
};

static unsigned char SEQ_NT36672C_CSOT_371[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_372[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_373[] = {
	0x53,
	0x2C,
};

static unsigned char SEQ_NT36672C_CSOT_374[] = {
	0x55,
	0x01,
};

static unsigned char SEQ_NT36672C_CSOT_375[] = {
	0x68,
	0x00, 0x01,
};

/* DO NOT REMOVE: back to page 1 for setting DCS commands */
static unsigned char SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_A[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_B[] = {
	0xFB,
	0x01,
};

static struct lcd_seq_info LCD_SEQ_INIT_1[] = {
	{SEQ_NT36672C_CSOT_001, ARRAY_SIZE(SEQ_NT36672C_CSOT_001), 0, },
	{SEQ_NT36672C_CSOT_002, ARRAY_SIZE(SEQ_NT36672C_CSOT_002), 0, },
	{SEQ_NT36672C_CSOT_003, ARRAY_SIZE(SEQ_NT36672C_CSOT_003), 0, },
	{SEQ_NT36672C_CSOT_004, ARRAY_SIZE(SEQ_NT36672C_CSOT_004), 0, },
	{SEQ_NT36672C_CSOT_005, ARRAY_SIZE(SEQ_NT36672C_CSOT_005), 0, },
	{SEQ_NT36672C_CSOT_006, ARRAY_SIZE(SEQ_NT36672C_CSOT_006), 0, },
	{SEQ_NT36672C_CSOT_007, ARRAY_SIZE(SEQ_NT36672C_CSOT_007), 0, },
	{SEQ_NT36672C_CSOT_008, ARRAY_SIZE(SEQ_NT36672C_CSOT_008), 0, },
	{SEQ_NT36672C_CSOT_009, ARRAY_SIZE(SEQ_NT36672C_CSOT_009), 0, },
	{SEQ_NT36672C_CSOT_010, ARRAY_SIZE(SEQ_NT36672C_CSOT_010), 0, },
	{SEQ_NT36672C_CSOT_011, ARRAY_SIZE(SEQ_NT36672C_CSOT_011), 0, },
	{SEQ_NT36672C_CSOT_012, ARRAY_SIZE(SEQ_NT36672C_CSOT_012), 0, },
	{SEQ_NT36672C_CSOT_013, ARRAY_SIZE(SEQ_NT36672C_CSOT_013), 0, },
	{SEQ_NT36672C_CSOT_014, ARRAY_SIZE(SEQ_NT36672C_CSOT_014), 0, },
	{SEQ_NT36672C_CSOT_015, ARRAY_SIZE(SEQ_NT36672C_CSOT_015), 0, },
	{SEQ_NT36672C_CSOT_016, ARRAY_SIZE(SEQ_NT36672C_CSOT_016), 0, },
	{SEQ_NT36672C_CSOT_017, ARRAY_SIZE(SEQ_NT36672C_CSOT_017), 0, },
	{SEQ_NT36672C_CSOT_018, ARRAY_SIZE(SEQ_NT36672C_CSOT_018), 0, },
	{SEQ_NT36672C_CSOT_019, ARRAY_SIZE(SEQ_NT36672C_CSOT_019), 0, },
	{SEQ_NT36672C_CSOT_020, ARRAY_SIZE(SEQ_NT36672C_CSOT_020), 0, },
	{SEQ_NT36672C_CSOT_021, ARRAY_SIZE(SEQ_NT36672C_CSOT_021), 0, },
	{SEQ_NT36672C_CSOT_022, ARRAY_SIZE(SEQ_NT36672C_CSOT_022), 0, },
	{SEQ_NT36672C_CSOT_023, ARRAY_SIZE(SEQ_NT36672C_CSOT_023), 0, },
	{SEQ_NT36672C_CSOT_024, ARRAY_SIZE(SEQ_NT36672C_CSOT_024), 0, },
	{SEQ_NT36672C_CSOT_025, ARRAY_SIZE(SEQ_NT36672C_CSOT_025), 0, },
	{SEQ_NT36672C_CSOT_026, ARRAY_SIZE(SEQ_NT36672C_CSOT_026), 0, },
	{SEQ_NT36672C_CSOT_027, ARRAY_SIZE(SEQ_NT36672C_CSOT_027), 0, },
	{SEQ_NT36672C_CSOT_028, ARRAY_SIZE(SEQ_NT36672C_CSOT_028), 0, },
	{SEQ_NT36672C_CSOT_029, ARRAY_SIZE(SEQ_NT36672C_CSOT_029), 0, },
	{SEQ_NT36672C_CSOT_030, ARRAY_SIZE(SEQ_NT36672C_CSOT_030), 0, },
	{SEQ_NT36672C_CSOT_031, ARRAY_SIZE(SEQ_NT36672C_CSOT_031), 0, },
	{SEQ_NT36672C_CSOT_032, ARRAY_SIZE(SEQ_NT36672C_CSOT_032), 0, },
	{SEQ_NT36672C_CSOT_033, ARRAY_SIZE(SEQ_NT36672C_CSOT_033), 0, },
	{SEQ_NT36672C_CSOT_034, ARRAY_SIZE(SEQ_NT36672C_CSOT_034), 0, },
	{SEQ_NT36672C_CSOT_035, ARRAY_SIZE(SEQ_NT36672C_CSOT_035), 0, },
	{SEQ_NT36672C_CSOT_036, ARRAY_SIZE(SEQ_NT36672C_CSOT_036), 0, },
	{SEQ_NT36672C_CSOT_037, ARRAY_SIZE(SEQ_NT36672C_CSOT_037), 0, },
	{SEQ_NT36672C_CSOT_038, ARRAY_SIZE(SEQ_NT36672C_CSOT_038), 0, },
	{SEQ_NT36672C_CSOT_039, ARRAY_SIZE(SEQ_NT36672C_CSOT_039), 0, },
	{SEQ_NT36672C_CSOT_040, ARRAY_SIZE(SEQ_NT36672C_CSOT_040), 0, },
	{SEQ_NT36672C_CSOT_041, ARRAY_SIZE(SEQ_NT36672C_CSOT_041), 0, },
	{SEQ_NT36672C_CSOT_042, ARRAY_SIZE(SEQ_NT36672C_CSOT_042), 0, },
	{SEQ_NT36672C_CSOT_043, ARRAY_SIZE(SEQ_NT36672C_CSOT_043), 0, },
	{SEQ_NT36672C_CSOT_044, ARRAY_SIZE(SEQ_NT36672C_CSOT_044), 0, },
	{SEQ_NT36672C_CSOT_045, ARRAY_SIZE(SEQ_NT36672C_CSOT_045), 0, },
	{SEQ_NT36672C_CSOT_046, ARRAY_SIZE(SEQ_NT36672C_CSOT_046), 0, },
	{SEQ_NT36672C_CSOT_047, ARRAY_SIZE(SEQ_NT36672C_CSOT_047), 0, },
	{SEQ_NT36672C_CSOT_048, ARRAY_SIZE(SEQ_NT36672C_CSOT_048), 0, },
	{SEQ_NT36672C_CSOT_049, ARRAY_SIZE(SEQ_NT36672C_CSOT_049), 0, },
	{SEQ_NT36672C_CSOT_050, ARRAY_SIZE(SEQ_NT36672C_CSOT_050), 0, },
	{SEQ_NT36672C_CSOT_051, ARRAY_SIZE(SEQ_NT36672C_CSOT_051), 0, },
	{SEQ_NT36672C_CSOT_052, ARRAY_SIZE(SEQ_NT36672C_CSOT_052), 0, },
	{SEQ_NT36672C_CSOT_053, ARRAY_SIZE(SEQ_NT36672C_CSOT_053), 0, },
	{SEQ_NT36672C_CSOT_054, ARRAY_SIZE(SEQ_NT36672C_CSOT_054), 0, },
	{SEQ_NT36672C_CSOT_055, ARRAY_SIZE(SEQ_NT36672C_CSOT_055), 0, },
	{SEQ_NT36672C_CSOT_056, ARRAY_SIZE(SEQ_NT36672C_CSOT_056), 0, },
	{SEQ_NT36672C_CSOT_057, ARRAY_SIZE(SEQ_NT36672C_CSOT_057), 0, },
	{SEQ_NT36672C_CSOT_058, ARRAY_SIZE(SEQ_NT36672C_CSOT_058), 0, },
	{SEQ_NT36672C_CSOT_059, ARRAY_SIZE(SEQ_NT36672C_CSOT_059), 0, },
	{SEQ_NT36672C_CSOT_060, ARRAY_SIZE(SEQ_NT36672C_CSOT_060), 0, },
	{SEQ_NT36672C_CSOT_061, ARRAY_SIZE(SEQ_NT36672C_CSOT_061), 0, },
	{SEQ_NT36672C_CSOT_062, ARRAY_SIZE(SEQ_NT36672C_CSOT_062), 0, },
	{SEQ_NT36672C_CSOT_063, ARRAY_SIZE(SEQ_NT36672C_CSOT_063), 0, },
	{SEQ_NT36672C_CSOT_064, ARRAY_SIZE(SEQ_NT36672C_CSOT_064), 0, },
	{SEQ_NT36672C_CSOT_065, ARRAY_SIZE(SEQ_NT36672C_CSOT_065), 0, },
	{SEQ_NT36672C_CSOT_066, ARRAY_SIZE(SEQ_NT36672C_CSOT_066), 0, },
	{SEQ_NT36672C_CSOT_067, ARRAY_SIZE(SEQ_NT36672C_CSOT_067), 0, },
	{SEQ_NT36672C_CSOT_068, ARRAY_SIZE(SEQ_NT36672C_CSOT_068), 0, },
	{SEQ_NT36672C_CSOT_069, ARRAY_SIZE(SEQ_NT36672C_CSOT_069), 0, },
	{SEQ_NT36672C_CSOT_070, ARRAY_SIZE(SEQ_NT36672C_CSOT_070), 0, },
	{SEQ_NT36672C_CSOT_071, ARRAY_SIZE(SEQ_NT36672C_CSOT_071), 0, },
	{SEQ_NT36672C_CSOT_072, ARRAY_SIZE(SEQ_NT36672C_CSOT_072), 0, },
	{SEQ_NT36672C_CSOT_073, ARRAY_SIZE(SEQ_NT36672C_CSOT_073), 0, },
	{SEQ_NT36672C_CSOT_074, ARRAY_SIZE(SEQ_NT36672C_CSOT_074), 0, },
	{SEQ_NT36672C_CSOT_075, ARRAY_SIZE(SEQ_NT36672C_CSOT_075), 0, },
	{SEQ_NT36672C_CSOT_076, ARRAY_SIZE(SEQ_NT36672C_CSOT_076), 0, },
	{SEQ_NT36672C_CSOT_077, ARRAY_SIZE(SEQ_NT36672C_CSOT_077), 0, },
	{SEQ_NT36672C_CSOT_078, ARRAY_SIZE(SEQ_NT36672C_CSOT_078), 0, },
	{SEQ_NT36672C_CSOT_079, ARRAY_SIZE(SEQ_NT36672C_CSOT_079), 0, },
	{SEQ_NT36672C_CSOT_080, ARRAY_SIZE(SEQ_NT36672C_CSOT_080), 0, },
	{SEQ_NT36672C_CSOT_081, ARRAY_SIZE(SEQ_NT36672C_CSOT_081), 0, },
	{SEQ_NT36672C_CSOT_082, ARRAY_SIZE(SEQ_NT36672C_CSOT_082), 0, },
	{SEQ_NT36672C_CSOT_083, ARRAY_SIZE(SEQ_NT36672C_CSOT_083), 0, },
	{SEQ_NT36672C_CSOT_084, ARRAY_SIZE(SEQ_NT36672C_CSOT_084), 0, },
	{SEQ_NT36672C_CSOT_085, ARRAY_SIZE(SEQ_NT36672C_CSOT_085), 0, },
	{SEQ_NT36672C_CSOT_086, ARRAY_SIZE(SEQ_NT36672C_CSOT_086), 0, },
	{SEQ_NT36672C_CSOT_087, ARRAY_SIZE(SEQ_NT36672C_CSOT_087), 0, },
	{SEQ_NT36672C_CSOT_088, ARRAY_SIZE(SEQ_NT36672C_CSOT_088), 0, },
	{SEQ_NT36672C_CSOT_089, ARRAY_SIZE(SEQ_NT36672C_CSOT_089), 0, },
	{SEQ_NT36672C_CSOT_090, ARRAY_SIZE(SEQ_NT36672C_CSOT_090), 0, },
	{SEQ_NT36672C_CSOT_091, ARRAY_SIZE(SEQ_NT36672C_CSOT_091), 0, },
	{SEQ_NT36672C_CSOT_092, ARRAY_SIZE(SEQ_NT36672C_CSOT_092), 0, },
	{SEQ_NT36672C_CSOT_093, ARRAY_SIZE(SEQ_NT36672C_CSOT_093), 0, },
	{SEQ_NT36672C_CSOT_094, ARRAY_SIZE(SEQ_NT36672C_CSOT_094), 0, },
	{SEQ_NT36672C_CSOT_095, ARRAY_SIZE(SEQ_NT36672C_CSOT_095), 0, },
	{SEQ_NT36672C_CSOT_096, ARRAY_SIZE(SEQ_NT36672C_CSOT_096), 0, },
	{SEQ_NT36672C_CSOT_097, ARRAY_SIZE(SEQ_NT36672C_CSOT_097), 0, },
	{SEQ_NT36672C_CSOT_098, ARRAY_SIZE(SEQ_NT36672C_CSOT_098), 0, },
	{SEQ_NT36672C_CSOT_099, ARRAY_SIZE(SEQ_NT36672C_CSOT_099), 0, },
	{SEQ_NT36672C_CSOT_100, ARRAY_SIZE(SEQ_NT36672C_CSOT_100), 0, },
	{SEQ_NT36672C_CSOT_101, ARRAY_SIZE(SEQ_NT36672C_CSOT_101), 0, },
	{SEQ_NT36672C_CSOT_102, ARRAY_SIZE(SEQ_NT36672C_CSOT_102), 0, },
	{SEQ_NT36672C_CSOT_103, ARRAY_SIZE(SEQ_NT36672C_CSOT_103), 0, },
	{SEQ_NT36672C_CSOT_104, ARRAY_SIZE(SEQ_NT36672C_CSOT_104), 0, },
	{SEQ_NT36672C_CSOT_105, ARRAY_SIZE(SEQ_NT36672C_CSOT_105), 0, },
	{SEQ_NT36672C_CSOT_106, ARRAY_SIZE(SEQ_NT36672C_CSOT_106), 0, },
	{SEQ_NT36672C_CSOT_107, ARRAY_SIZE(SEQ_NT36672C_CSOT_107), 0, },
	{SEQ_NT36672C_CSOT_108, ARRAY_SIZE(SEQ_NT36672C_CSOT_108), 0, },
	{SEQ_NT36672C_CSOT_109, ARRAY_SIZE(SEQ_NT36672C_CSOT_109), 0, },
	{SEQ_NT36672C_CSOT_110, ARRAY_SIZE(SEQ_NT36672C_CSOT_110), 0, },
	{SEQ_NT36672C_CSOT_111, ARRAY_SIZE(SEQ_NT36672C_CSOT_111), 0, },
	{SEQ_NT36672C_CSOT_112, ARRAY_SIZE(SEQ_NT36672C_CSOT_112), 0, },
	{SEQ_NT36672C_CSOT_113, ARRAY_SIZE(SEQ_NT36672C_CSOT_113), 0, },
	{SEQ_NT36672C_CSOT_114, ARRAY_SIZE(SEQ_NT36672C_CSOT_114), 0, },
	{SEQ_NT36672C_CSOT_115, ARRAY_SIZE(SEQ_NT36672C_CSOT_115), 0, },
	{SEQ_NT36672C_CSOT_116, ARRAY_SIZE(SEQ_NT36672C_CSOT_116), 0, },
	{SEQ_NT36672C_CSOT_117, ARRAY_SIZE(SEQ_NT36672C_CSOT_117), 0, },
	{SEQ_NT36672C_CSOT_118, ARRAY_SIZE(SEQ_NT36672C_CSOT_118), 0, },
	{SEQ_NT36672C_CSOT_119, ARRAY_SIZE(SEQ_NT36672C_CSOT_119), 0, },
	{SEQ_NT36672C_CSOT_120, ARRAY_SIZE(SEQ_NT36672C_CSOT_120), 0, },
	{SEQ_NT36672C_CSOT_121, ARRAY_SIZE(SEQ_NT36672C_CSOT_121), 0, },
	{SEQ_NT36672C_CSOT_122, ARRAY_SIZE(SEQ_NT36672C_CSOT_122), 0, },
	{SEQ_NT36672C_CSOT_123, ARRAY_SIZE(SEQ_NT36672C_CSOT_123), 0, },
	{SEQ_NT36672C_CSOT_124, ARRAY_SIZE(SEQ_NT36672C_CSOT_124), 0, },
	{SEQ_NT36672C_CSOT_125, ARRAY_SIZE(SEQ_NT36672C_CSOT_125), 0, },
	{SEQ_NT36672C_CSOT_126, ARRAY_SIZE(SEQ_NT36672C_CSOT_126), 0, },
	{SEQ_NT36672C_CSOT_127, ARRAY_SIZE(SEQ_NT36672C_CSOT_127), 0, },
	{SEQ_NT36672C_CSOT_128, ARRAY_SIZE(SEQ_NT36672C_CSOT_128), 0, },
	{SEQ_NT36672C_CSOT_129, ARRAY_SIZE(SEQ_NT36672C_CSOT_129), 0, },
	{SEQ_NT36672C_CSOT_130, ARRAY_SIZE(SEQ_NT36672C_CSOT_130), 0, },
	{SEQ_NT36672C_CSOT_131, ARRAY_SIZE(SEQ_NT36672C_CSOT_131), 0, },
	{SEQ_NT36672C_CSOT_132, ARRAY_SIZE(SEQ_NT36672C_CSOT_132), 0, },
	{SEQ_NT36672C_CSOT_133, ARRAY_SIZE(SEQ_NT36672C_CSOT_133), 0, },
	{SEQ_NT36672C_CSOT_134, ARRAY_SIZE(SEQ_NT36672C_CSOT_134), 0, },
	{SEQ_NT36672C_CSOT_135, ARRAY_SIZE(SEQ_NT36672C_CSOT_135), 0, },
	{SEQ_NT36672C_CSOT_136, ARRAY_SIZE(SEQ_NT36672C_CSOT_136), 0, },
	{SEQ_NT36672C_CSOT_137, ARRAY_SIZE(SEQ_NT36672C_CSOT_137), 0, },
	{SEQ_NT36672C_CSOT_138, ARRAY_SIZE(SEQ_NT36672C_CSOT_138), 0, },
	{SEQ_NT36672C_CSOT_139, ARRAY_SIZE(SEQ_NT36672C_CSOT_139), 0, },
	{SEQ_NT36672C_CSOT_140, ARRAY_SIZE(SEQ_NT36672C_CSOT_140), 0, },
	{SEQ_NT36672C_CSOT_141, ARRAY_SIZE(SEQ_NT36672C_CSOT_141), 0, },
	{SEQ_NT36672C_CSOT_142, ARRAY_SIZE(SEQ_NT36672C_CSOT_142), 0, },
	{SEQ_NT36672C_CSOT_143, ARRAY_SIZE(SEQ_NT36672C_CSOT_143), 0, },
	{SEQ_NT36672C_CSOT_144, ARRAY_SIZE(SEQ_NT36672C_CSOT_144), 0, },
	{SEQ_NT36672C_CSOT_145, ARRAY_SIZE(SEQ_NT36672C_CSOT_145), 0, },
	{SEQ_NT36672C_CSOT_146, ARRAY_SIZE(SEQ_NT36672C_CSOT_146), 0, },
	{SEQ_NT36672C_CSOT_147, ARRAY_SIZE(SEQ_NT36672C_CSOT_147), 0, },
	{SEQ_NT36672C_CSOT_148, ARRAY_SIZE(SEQ_NT36672C_CSOT_148), 0, },
	{SEQ_NT36672C_CSOT_149, ARRAY_SIZE(SEQ_NT36672C_CSOT_149), 0, },
	{SEQ_NT36672C_CSOT_150, ARRAY_SIZE(SEQ_NT36672C_CSOT_150), 0, },
	{SEQ_NT36672C_CSOT_151, ARRAY_SIZE(SEQ_NT36672C_CSOT_151), 0, },
	{SEQ_NT36672C_CSOT_152, ARRAY_SIZE(SEQ_NT36672C_CSOT_152), 0, },
	{SEQ_NT36672C_CSOT_153, ARRAY_SIZE(SEQ_NT36672C_CSOT_153), 0, },
	{SEQ_NT36672C_CSOT_154, ARRAY_SIZE(SEQ_NT36672C_CSOT_154), 0, },
	{SEQ_NT36672C_CSOT_155, ARRAY_SIZE(SEQ_NT36672C_CSOT_155), 0, },
	{SEQ_NT36672C_CSOT_156, ARRAY_SIZE(SEQ_NT36672C_CSOT_156), 0, },
	{SEQ_NT36672C_CSOT_157, ARRAY_SIZE(SEQ_NT36672C_CSOT_157), 0, },
	{SEQ_NT36672C_CSOT_158, ARRAY_SIZE(SEQ_NT36672C_CSOT_158), 0, },
	{SEQ_NT36672C_CSOT_159, ARRAY_SIZE(SEQ_NT36672C_CSOT_159), 0, },
	{SEQ_NT36672C_CSOT_160, ARRAY_SIZE(SEQ_NT36672C_CSOT_160), 0, },
	{SEQ_NT36672C_CSOT_161, ARRAY_SIZE(SEQ_NT36672C_CSOT_161), 0, },
	{SEQ_NT36672C_CSOT_162, ARRAY_SIZE(SEQ_NT36672C_CSOT_162), 0, },
	{SEQ_NT36672C_CSOT_163, ARRAY_SIZE(SEQ_NT36672C_CSOT_163), 0, },
	{SEQ_NT36672C_CSOT_164, ARRAY_SIZE(SEQ_NT36672C_CSOT_164), 0, },
	{SEQ_NT36672C_CSOT_165, ARRAY_SIZE(SEQ_NT36672C_CSOT_165), 0, },
	{SEQ_NT36672C_CSOT_166, ARRAY_SIZE(SEQ_NT36672C_CSOT_166), 0, },
	{SEQ_NT36672C_CSOT_167, ARRAY_SIZE(SEQ_NT36672C_CSOT_167), 0, },
	{SEQ_NT36672C_CSOT_168, ARRAY_SIZE(SEQ_NT36672C_CSOT_168), 0, },
	{SEQ_NT36672C_CSOT_169, ARRAY_SIZE(SEQ_NT36672C_CSOT_169), 0, },
	{SEQ_NT36672C_CSOT_170, ARRAY_SIZE(SEQ_NT36672C_CSOT_170), 0, },
	{SEQ_NT36672C_CSOT_171, ARRAY_SIZE(SEQ_NT36672C_CSOT_171), 0, },
	{SEQ_NT36672C_CSOT_172, ARRAY_SIZE(SEQ_NT36672C_CSOT_172), 0, },
	{SEQ_NT36672C_CSOT_173, ARRAY_SIZE(SEQ_NT36672C_CSOT_173), 0, },
	{SEQ_NT36672C_CSOT_174, ARRAY_SIZE(SEQ_NT36672C_CSOT_174), 0, },
	{SEQ_NT36672C_CSOT_175, ARRAY_SIZE(SEQ_NT36672C_CSOT_175), 0, },
	{SEQ_NT36672C_CSOT_176, ARRAY_SIZE(SEQ_NT36672C_CSOT_176), 0, },
	{SEQ_NT36672C_CSOT_177, ARRAY_SIZE(SEQ_NT36672C_CSOT_177), 0, },
	{SEQ_NT36672C_CSOT_178, ARRAY_SIZE(SEQ_NT36672C_CSOT_178), 0, },
	{SEQ_NT36672C_CSOT_179, ARRAY_SIZE(SEQ_NT36672C_CSOT_179), 0, },
	{SEQ_NT36672C_CSOT_180, ARRAY_SIZE(SEQ_NT36672C_CSOT_180), 0, },
	{SEQ_NT36672C_CSOT_181, ARRAY_SIZE(SEQ_NT36672C_CSOT_181), 0, },
	{SEQ_NT36672C_CSOT_182, ARRAY_SIZE(SEQ_NT36672C_CSOT_182), 0, },
	{SEQ_NT36672C_CSOT_183, ARRAY_SIZE(SEQ_NT36672C_CSOT_183), 0, },
	{SEQ_NT36672C_CSOT_184, ARRAY_SIZE(SEQ_NT36672C_CSOT_184), 0, },
	{SEQ_NT36672C_CSOT_185, ARRAY_SIZE(SEQ_NT36672C_CSOT_185), 0, },
	{SEQ_NT36672C_CSOT_186, ARRAY_SIZE(SEQ_NT36672C_CSOT_186), 0, },
	{SEQ_NT36672C_CSOT_187, ARRAY_SIZE(SEQ_NT36672C_CSOT_187), 0, },
	{SEQ_NT36672C_CSOT_188, ARRAY_SIZE(SEQ_NT36672C_CSOT_188), 0, },
	{SEQ_NT36672C_CSOT_189, ARRAY_SIZE(SEQ_NT36672C_CSOT_189), 0, },
	{SEQ_NT36672C_CSOT_190, ARRAY_SIZE(SEQ_NT36672C_CSOT_190), 0, },
	{SEQ_NT36672C_CSOT_191, ARRAY_SIZE(SEQ_NT36672C_CSOT_191), 0, },
	{SEQ_NT36672C_CSOT_192, ARRAY_SIZE(SEQ_NT36672C_CSOT_192), 0, },
	{SEQ_NT36672C_CSOT_193, ARRAY_SIZE(SEQ_NT36672C_CSOT_193), 0, },
	{SEQ_NT36672C_CSOT_194, ARRAY_SIZE(SEQ_NT36672C_CSOT_194), 0, },
	{SEQ_NT36672C_CSOT_195, ARRAY_SIZE(SEQ_NT36672C_CSOT_195), 0, },
	{SEQ_NT36672C_CSOT_196, ARRAY_SIZE(SEQ_NT36672C_CSOT_196), 0, },
	{SEQ_NT36672C_CSOT_197, ARRAY_SIZE(SEQ_NT36672C_CSOT_197), 0, },
	{SEQ_NT36672C_CSOT_198, ARRAY_SIZE(SEQ_NT36672C_CSOT_198), 0, },
	{SEQ_NT36672C_CSOT_199, ARRAY_SIZE(SEQ_NT36672C_CSOT_199), 0, },
	{SEQ_NT36672C_CSOT_200, ARRAY_SIZE(SEQ_NT36672C_CSOT_200), 0, },
	{SEQ_NT36672C_CSOT_201, ARRAY_SIZE(SEQ_NT36672C_CSOT_201), 0, },
	{SEQ_NT36672C_CSOT_202, ARRAY_SIZE(SEQ_NT36672C_CSOT_202), 0, },
	{SEQ_NT36672C_CSOT_203, ARRAY_SIZE(SEQ_NT36672C_CSOT_203), 0, },
	{SEQ_NT36672C_CSOT_204, ARRAY_SIZE(SEQ_NT36672C_CSOT_204), 0, },
	{SEQ_NT36672C_CSOT_205, ARRAY_SIZE(SEQ_NT36672C_CSOT_205), 0, },
	{SEQ_NT36672C_CSOT_206, ARRAY_SIZE(SEQ_NT36672C_CSOT_206), 0, },
	{SEQ_NT36672C_CSOT_207, ARRAY_SIZE(SEQ_NT36672C_CSOT_207), 0, },
	{SEQ_NT36672C_CSOT_208, ARRAY_SIZE(SEQ_NT36672C_CSOT_208), 0, },
	{SEQ_NT36672C_CSOT_209, ARRAY_SIZE(SEQ_NT36672C_CSOT_209), 0, },
	{SEQ_NT36672C_CSOT_210, ARRAY_SIZE(SEQ_NT36672C_CSOT_210), 0, },
	{SEQ_NT36672C_CSOT_211, ARRAY_SIZE(SEQ_NT36672C_CSOT_211), 0, },
	{SEQ_NT36672C_CSOT_212, ARRAY_SIZE(SEQ_NT36672C_CSOT_212), 0, },
	{SEQ_NT36672C_CSOT_213, ARRAY_SIZE(SEQ_NT36672C_CSOT_213), 0, },
	{SEQ_NT36672C_CSOT_214, ARRAY_SIZE(SEQ_NT36672C_CSOT_214), 0, },
	{SEQ_NT36672C_CSOT_215, ARRAY_SIZE(SEQ_NT36672C_CSOT_215), 0, },
	{SEQ_NT36672C_CSOT_216, ARRAY_SIZE(SEQ_NT36672C_CSOT_216), 0, },
	{SEQ_NT36672C_CSOT_217, ARRAY_SIZE(SEQ_NT36672C_CSOT_217), 0, },
	{SEQ_NT36672C_CSOT_218, ARRAY_SIZE(SEQ_NT36672C_CSOT_218), 0, },
	{SEQ_NT36672C_CSOT_219, ARRAY_SIZE(SEQ_NT36672C_CSOT_219), 0, },
	{SEQ_NT36672C_CSOT_220, ARRAY_SIZE(SEQ_NT36672C_CSOT_220), 0, },
	{SEQ_NT36672C_CSOT_221, ARRAY_SIZE(SEQ_NT36672C_CSOT_221), 0, },
	{SEQ_NT36672C_CSOT_222, ARRAY_SIZE(SEQ_NT36672C_CSOT_222), 0, },
	{SEQ_NT36672C_CSOT_223, ARRAY_SIZE(SEQ_NT36672C_CSOT_223), 0, },
	{SEQ_NT36672C_CSOT_224, ARRAY_SIZE(SEQ_NT36672C_CSOT_224), 0, },
	{SEQ_NT36672C_CSOT_225, ARRAY_SIZE(SEQ_NT36672C_CSOT_225), 0, },
	{SEQ_NT36672C_CSOT_226, ARRAY_SIZE(SEQ_NT36672C_CSOT_226), 0, },
	{SEQ_NT36672C_CSOT_227, ARRAY_SIZE(SEQ_NT36672C_CSOT_227), 0, },
	{SEQ_NT36672C_CSOT_228, ARRAY_SIZE(SEQ_NT36672C_CSOT_228), 0, },
	{SEQ_NT36672C_CSOT_229, ARRAY_SIZE(SEQ_NT36672C_CSOT_229), 0, },
	{SEQ_NT36672C_CSOT_230, ARRAY_SIZE(SEQ_NT36672C_CSOT_230), 0, },
	{SEQ_NT36672C_CSOT_231, ARRAY_SIZE(SEQ_NT36672C_CSOT_231), 0, },
	{SEQ_NT36672C_CSOT_232, ARRAY_SIZE(SEQ_NT36672C_CSOT_232), 0, },
	{SEQ_NT36672C_CSOT_233, ARRAY_SIZE(SEQ_NT36672C_CSOT_233), 0, },
	{SEQ_NT36672C_CSOT_234, ARRAY_SIZE(SEQ_NT36672C_CSOT_234), 0, },
	{SEQ_NT36672C_CSOT_235, ARRAY_SIZE(SEQ_NT36672C_CSOT_235), 0, },
	{SEQ_NT36672C_CSOT_236, ARRAY_SIZE(SEQ_NT36672C_CSOT_236), 0, },
	{SEQ_NT36672C_CSOT_237, ARRAY_SIZE(SEQ_NT36672C_CSOT_237), 0, },
	{SEQ_NT36672C_CSOT_238, ARRAY_SIZE(SEQ_NT36672C_CSOT_238), 0, },
	{SEQ_NT36672C_CSOT_239, ARRAY_SIZE(SEQ_NT36672C_CSOT_239), 0, },
	{SEQ_NT36672C_CSOT_240, ARRAY_SIZE(SEQ_NT36672C_CSOT_240), 0, },
	{SEQ_NT36672C_CSOT_241, ARRAY_SIZE(SEQ_NT36672C_CSOT_241), 0, },
	{SEQ_NT36672C_CSOT_242, ARRAY_SIZE(SEQ_NT36672C_CSOT_242), 0, },
	{SEQ_NT36672C_CSOT_243, ARRAY_SIZE(SEQ_NT36672C_CSOT_243), 0, },
	{SEQ_NT36672C_CSOT_244, ARRAY_SIZE(SEQ_NT36672C_CSOT_244), 0, },
	{SEQ_NT36672C_CSOT_245, ARRAY_SIZE(SEQ_NT36672C_CSOT_245), 0, },
	{SEQ_NT36672C_CSOT_246, ARRAY_SIZE(SEQ_NT36672C_CSOT_246), 0, },
	{SEQ_NT36672C_CSOT_247, ARRAY_SIZE(SEQ_NT36672C_CSOT_247), 0, },
	{SEQ_NT36672C_CSOT_248, ARRAY_SIZE(SEQ_NT36672C_CSOT_248), 0, },
	{SEQ_NT36672C_CSOT_249, ARRAY_SIZE(SEQ_NT36672C_CSOT_249), 0, },
	{SEQ_NT36672C_CSOT_250, ARRAY_SIZE(SEQ_NT36672C_CSOT_250), 0, },
	{SEQ_NT36672C_CSOT_251, ARRAY_SIZE(SEQ_NT36672C_CSOT_251), 0, },
	{SEQ_NT36672C_CSOT_252, ARRAY_SIZE(SEQ_NT36672C_CSOT_252), 0, },
	{SEQ_NT36672C_CSOT_253, ARRAY_SIZE(SEQ_NT36672C_CSOT_253), 0, },
	{SEQ_NT36672C_CSOT_254, ARRAY_SIZE(SEQ_NT36672C_CSOT_254), 0, },
	{SEQ_NT36672C_CSOT_255, ARRAY_SIZE(SEQ_NT36672C_CSOT_255), 0, },
	{SEQ_NT36672C_CSOT_256, ARRAY_SIZE(SEQ_NT36672C_CSOT_256), 0, },
	{SEQ_NT36672C_CSOT_257, ARRAY_SIZE(SEQ_NT36672C_CSOT_257), 0, },
	{SEQ_NT36672C_CSOT_258, ARRAY_SIZE(SEQ_NT36672C_CSOT_258), 0, },
	{SEQ_NT36672C_CSOT_259, ARRAY_SIZE(SEQ_NT36672C_CSOT_259), 0, },
	{SEQ_NT36672C_CSOT_260, ARRAY_SIZE(SEQ_NT36672C_CSOT_260), 0, },
	{SEQ_NT36672C_CSOT_261, ARRAY_SIZE(SEQ_NT36672C_CSOT_261), 0, },
	{SEQ_NT36672C_CSOT_262, ARRAY_SIZE(SEQ_NT36672C_CSOT_262), 0, },
	{SEQ_NT36672C_CSOT_263, ARRAY_SIZE(SEQ_NT36672C_CSOT_263), 0, },
	{SEQ_NT36672C_CSOT_264, ARRAY_SIZE(SEQ_NT36672C_CSOT_264), 0, },
	{SEQ_NT36672C_CSOT_265, ARRAY_SIZE(SEQ_NT36672C_CSOT_265), 0, },
	{SEQ_NT36672C_CSOT_266, ARRAY_SIZE(SEQ_NT36672C_CSOT_266), 0, },
	{SEQ_NT36672C_CSOT_267, ARRAY_SIZE(SEQ_NT36672C_CSOT_267), 0, },
	{SEQ_NT36672C_CSOT_268, ARRAY_SIZE(SEQ_NT36672C_CSOT_268), 0, },
	{SEQ_NT36672C_CSOT_269, ARRAY_SIZE(SEQ_NT36672C_CSOT_269), 0, },
	{SEQ_NT36672C_CSOT_270, ARRAY_SIZE(SEQ_NT36672C_CSOT_270), 0, },
	{SEQ_NT36672C_CSOT_271, ARRAY_SIZE(SEQ_NT36672C_CSOT_271), 0, },
	{SEQ_NT36672C_CSOT_272, ARRAY_SIZE(SEQ_NT36672C_CSOT_272), 0, },
	{SEQ_NT36672C_CSOT_273, ARRAY_SIZE(SEQ_NT36672C_CSOT_273), 0, },
	{SEQ_NT36672C_CSOT_274, ARRAY_SIZE(SEQ_NT36672C_CSOT_274), 0, },
	{SEQ_NT36672C_CSOT_275, ARRAY_SIZE(SEQ_NT36672C_CSOT_275), 0, },
	{SEQ_NT36672C_CSOT_276, ARRAY_SIZE(SEQ_NT36672C_CSOT_276), 0, },
	{SEQ_NT36672C_CSOT_277, ARRAY_SIZE(SEQ_NT36672C_CSOT_277), 0, },
	{SEQ_NT36672C_CSOT_278, ARRAY_SIZE(SEQ_NT36672C_CSOT_278), 0, },
	{SEQ_NT36672C_CSOT_279, ARRAY_SIZE(SEQ_NT36672C_CSOT_279), 0, },
	{SEQ_NT36672C_CSOT_280, ARRAY_SIZE(SEQ_NT36672C_CSOT_280), 0, },
	{SEQ_NT36672C_CSOT_281, ARRAY_SIZE(SEQ_NT36672C_CSOT_281), 0, },
	{SEQ_NT36672C_CSOT_282, ARRAY_SIZE(SEQ_NT36672C_CSOT_282), 0, },
	{SEQ_NT36672C_CSOT_283, ARRAY_SIZE(SEQ_NT36672C_CSOT_283), 0, },
	{SEQ_NT36672C_CSOT_284, ARRAY_SIZE(SEQ_NT36672C_CSOT_284), 0, },
	{SEQ_NT36672C_CSOT_285, ARRAY_SIZE(SEQ_NT36672C_CSOT_285), 0, },
	{SEQ_NT36672C_CSOT_286, ARRAY_SIZE(SEQ_NT36672C_CSOT_286), 0, },
	{SEQ_NT36672C_CSOT_287, ARRAY_SIZE(SEQ_NT36672C_CSOT_287), 0, },
	{SEQ_NT36672C_CSOT_288, ARRAY_SIZE(SEQ_NT36672C_CSOT_288), 0, },
	{SEQ_NT36672C_CSOT_289, ARRAY_SIZE(SEQ_NT36672C_CSOT_289), 0, },
	{SEQ_NT36672C_CSOT_290, ARRAY_SIZE(SEQ_NT36672C_CSOT_290), 0, },
	{SEQ_NT36672C_CSOT_291, ARRAY_SIZE(SEQ_NT36672C_CSOT_291), 0, },
	{SEQ_NT36672C_CSOT_292, ARRAY_SIZE(SEQ_NT36672C_CSOT_292), 0, },
	{SEQ_NT36672C_CSOT_293, ARRAY_SIZE(SEQ_NT36672C_CSOT_293), 0, },
	{SEQ_NT36672C_CSOT_294, ARRAY_SIZE(SEQ_NT36672C_CSOT_294), 0, },
	{SEQ_NT36672C_CSOT_295, ARRAY_SIZE(SEQ_NT36672C_CSOT_295), 0, },
	{SEQ_NT36672C_CSOT_296, ARRAY_SIZE(SEQ_NT36672C_CSOT_296), 0, },
	{SEQ_NT36672C_CSOT_297, ARRAY_SIZE(SEQ_NT36672C_CSOT_297), 0, },
	{SEQ_NT36672C_CSOT_298, ARRAY_SIZE(SEQ_NT36672C_CSOT_298), 0, },
	{SEQ_NT36672C_CSOT_299, ARRAY_SIZE(SEQ_NT36672C_CSOT_299), 0, },
	{SEQ_NT36672C_CSOT_300, ARRAY_SIZE(SEQ_NT36672C_CSOT_300), 0, },
	{SEQ_NT36672C_CSOT_301, ARRAY_SIZE(SEQ_NT36672C_CSOT_301), 0, },
	{SEQ_NT36672C_CSOT_302, ARRAY_SIZE(SEQ_NT36672C_CSOT_302), 0, },
	{SEQ_NT36672C_CSOT_303, ARRAY_SIZE(SEQ_NT36672C_CSOT_303), 0, },
	{SEQ_NT36672C_CSOT_304, ARRAY_SIZE(SEQ_NT36672C_CSOT_304), 0, },
	{SEQ_NT36672C_CSOT_305, ARRAY_SIZE(SEQ_NT36672C_CSOT_305), 0, },
	{SEQ_NT36672C_CSOT_306, ARRAY_SIZE(SEQ_NT36672C_CSOT_306), 0, },
	{SEQ_NT36672C_CSOT_307, ARRAY_SIZE(SEQ_NT36672C_CSOT_307), 0, },
	{SEQ_NT36672C_CSOT_308, ARRAY_SIZE(SEQ_NT36672C_CSOT_308), 0, },
	{SEQ_NT36672C_CSOT_309, ARRAY_SIZE(SEQ_NT36672C_CSOT_309), 0, },
	{SEQ_NT36672C_CSOT_310, ARRAY_SIZE(SEQ_NT36672C_CSOT_310), 0, },
	{SEQ_NT36672C_CSOT_311, ARRAY_SIZE(SEQ_NT36672C_CSOT_311), 0, },
	{SEQ_NT36672C_CSOT_312, ARRAY_SIZE(SEQ_NT36672C_CSOT_312), 0, },
	{SEQ_NT36672C_CSOT_313, ARRAY_SIZE(SEQ_NT36672C_CSOT_313), 0, },
	{SEQ_NT36672C_CSOT_314, ARRAY_SIZE(SEQ_NT36672C_CSOT_314), 0, },
	{SEQ_NT36672C_CSOT_315, ARRAY_SIZE(SEQ_NT36672C_CSOT_315), 0, },
	{SEQ_NT36672C_CSOT_316, ARRAY_SIZE(SEQ_NT36672C_CSOT_316), 0, },
	{SEQ_NT36672C_CSOT_317, ARRAY_SIZE(SEQ_NT36672C_CSOT_317), 0, },
	{SEQ_NT36672C_CSOT_318, ARRAY_SIZE(SEQ_NT36672C_CSOT_318), 0, },
	{SEQ_NT36672C_CSOT_319, ARRAY_SIZE(SEQ_NT36672C_CSOT_319), 0, },
	{SEQ_NT36672C_CSOT_320, ARRAY_SIZE(SEQ_NT36672C_CSOT_320), 0, },
	{SEQ_NT36672C_CSOT_321, ARRAY_SIZE(SEQ_NT36672C_CSOT_321), 0, },
	{SEQ_NT36672C_CSOT_322, ARRAY_SIZE(SEQ_NT36672C_CSOT_322), 0, },
	{SEQ_NT36672C_CSOT_323, ARRAY_SIZE(SEQ_NT36672C_CSOT_323), 0, },
	{SEQ_NT36672C_CSOT_324, ARRAY_SIZE(SEQ_NT36672C_CSOT_324), 0, },
	{SEQ_NT36672C_CSOT_325, ARRAY_SIZE(SEQ_NT36672C_CSOT_325), 0, },
	{SEQ_NT36672C_CSOT_326, ARRAY_SIZE(SEQ_NT36672C_CSOT_326), 0, },
	{SEQ_NT36672C_CSOT_327, ARRAY_SIZE(SEQ_NT36672C_CSOT_327), 0, },
	{SEQ_NT36672C_CSOT_328, ARRAY_SIZE(SEQ_NT36672C_CSOT_328), 0, },
	{SEQ_NT36672C_CSOT_329, ARRAY_SIZE(SEQ_NT36672C_CSOT_329), 0, },
	{SEQ_NT36672C_CSOT_330, ARRAY_SIZE(SEQ_NT36672C_CSOT_330), 0, },
	{SEQ_NT36672C_CSOT_331, ARRAY_SIZE(SEQ_NT36672C_CSOT_331), 0, },
	{SEQ_NT36672C_CSOT_332, ARRAY_SIZE(SEQ_NT36672C_CSOT_332), 0, },
	{SEQ_NT36672C_CSOT_333, ARRAY_SIZE(SEQ_NT36672C_CSOT_333), 0, },
	{SEQ_NT36672C_CSOT_334, ARRAY_SIZE(SEQ_NT36672C_CSOT_334), 0, },
	{SEQ_NT36672C_CSOT_335, ARRAY_SIZE(SEQ_NT36672C_CSOT_335), 0, },
	{SEQ_NT36672C_CSOT_336, ARRAY_SIZE(SEQ_NT36672C_CSOT_336), 0, },
	{SEQ_NT36672C_CSOT_337, ARRAY_SIZE(SEQ_NT36672C_CSOT_337), 0, },
	{SEQ_NT36672C_CSOT_338, ARRAY_SIZE(SEQ_NT36672C_CSOT_338), 0, },
	{SEQ_NT36672C_CSOT_339, ARRAY_SIZE(SEQ_NT36672C_CSOT_339), 0, },
	{SEQ_NT36672C_CSOT_340, ARRAY_SIZE(SEQ_NT36672C_CSOT_340), 0, },
	{SEQ_NT36672C_CSOT_341, ARRAY_SIZE(SEQ_NT36672C_CSOT_341), 0, },
	{SEQ_NT36672C_CSOT_342, ARRAY_SIZE(SEQ_NT36672C_CSOT_342), 0, },
	{SEQ_NT36672C_CSOT_343, ARRAY_SIZE(SEQ_NT36672C_CSOT_343), 0, },
	{SEQ_NT36672C_CSOT_344, ARRAY_SIZE(SEQ_NT36672C_CSOT_344), 0, },
	{SEQ_NT36672C_CSOT_345, ARRAY_SIZE(SEQ_NT36672C_CSOT_345), 0, },
	{SEQ_NT36672C_CSOT_346, ARRAY_SIZE(SEQ_NT36672C_CSOT_346), 0, },
	{SEQ_NT36672C_CSOT_347, ARRAY_SIZE(SEQ_NT36672C_CSOT_347), 0, },
	{SEQ_NT36672C_CSOT_348, ARRAY_SIZE(SEQ_NT36672C_CSOT_348), 0, },
	{SEQ_NT36672C_CSOT_349, ARRAY_SIZE(SEQ_NT36672C_CSOT_349), 0, },
	{SEQ_NT36672C_CSOT_350, ARRAY_SIZE(SEQ_NT36672C_CSOT_350), 0, },
	{SEQ_NT36672C_CSOT_351, ARRAY_SIZE(SEQ_NT36672C_CSOT_351), 0, },
	{SEQ_NT36672C_CSOT_352, ARRAY_SIZE(SEQ_NT36672C_CSOT_352), 0, },
	{SEQ_NT36672C_CSOT_353, ARRAY_SIZE(SEQ_NT36672C_CSOT_353), 0, },
	{SEQ_NT36672C_CSOT_354, ARRAY_SIZE(SEQ_NT36672C_CSOT_354), 0, },
	{SEQ_NT36672C_CSOT_355, ARRAY_SIZE(SEQ_NT36672C_CSOT_355), 0, },
	{SEQ_NT36672C_CSOT_356, ARRAY_SIZE(SEQ_NT36672C_CSOT_356), 0, },
	{SEQ_NT36672C_CSOT_357, ARRAY_SIZE(SEQ_NT36672C_CSOT_357), 0, },
	{SEQ_NT36672C_CSOT_358, ARRAY_SIZE(SEQ_NT36672C_CSOT_358), 0, },
	{SEQ_NT36672C_CSOT_359, ARRAY_SIZE(SEQ_NT36672C_CSOT_359), 0, },
	{SEQ_NT36672C_CSOT_360, ARRAY_SIZE(SEQ_NT36672C_CSOT_360), 0, },
	{SEQ_NT36672C_CSOT_361, ARRAY_SIZE(SEQ_NT36672C_CSOT_361), 0, },
	{SEQ_NT36672C_CSOT_362, ARRAY_SIZE(SEQ_NT36672C_CSOT_362), 0, },
	{SEQ_NT36672C_CSOT_363, ARRAY_SIZE(SEQ_NT36672C_CSOT_363), 0, },
	{SEQ_NT36672C_CSOT_364, ARRAY_SIZE(SEQ_NT36672C_CSOT_364), 0, },
	{SEQ_NT36672C_CSOT_365, ARRAY_SIZE(SEQ_NT36672C_CSOT_365), 0, },
	{SEQ_NT36672C_CSOT_366, ARRAY_SIZE(SEQ_NT36672C_CSOT_366), 0, },
	{SEQ_NT36672C_CSOT_367, ARRAY_SIZE(SEQ_NT36672C_CSOT_367), 0, },
	{SEQ_NT36672C_CSOT_368, ARRAY_SIZE(SEQ_NT36672C_CSOT_368), 0, },
	{SEQ_NT36672C_CSOT_369, ARRAY_SIZE(SEQ_NT36672C_CSOT_369), 0, },
	{SEQ_NT36672C_CSOT_370, ARRAY_SIZE(SEQ_NT36672C_CSOT_370), 0, },
	{SEQ_NT36672C_CSOT_371, ARRAY_SIZE(SEQ_NT36672C_CSOT_371), 0, },
	{SEQ_NT36672C_CSOT_372, ARRAY_SIZE(SEQ_NT36672C_CSOT_372), 0, },
	{SEQ_NT36672C_CSOT_373, ARRAY_SIZE(SEQ_NT36672C_CSOT_373), 0, },
	{SEQ_NT36672C_CSOT_374, ARRAY_SIZE(SEQ_NT36672C_CSOT_374), 0, },
	{SEQ_NT36672C_CSOT_375, ARRAY_SIZE(SEQ_NT36672C_CSOT_375), 0, },
	/* DO NOT REMOVE: back to page 1 for setting DCS commands */
	//{SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_A, ARRAY_SIZE(SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_A), 0, },
	//{SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_B, ARRAY_SIZE(SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_B), 0, },
	{SEQ_NT36672C_CSOT_BRIGHTNESS, ARRAY_SIZE(SEQ_NT36672C_CSOT_BRIGHTNESS), 0, },
	{SEQ_NT36672C_CSOT_SLEEP_OUT, ARRAY_SIZE(SEQ_NT36672C_CSOT_SLEEP_OUT), 100*1000, },	/* 100ms */
	{SEQ_NT36672C_CSOT_DISPLAY_ON, ARRAY_SIZE(SEQ_NT36672C_CSOT_DISPLAY_ON), 0, },
};

static struct lcd_seq_info LCD_SEQ_EXIT_1[] = {
	/* DO NOT REMOVE: back to page 1 for setting DCS commands */
	{SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_A, ARRAY_SIZE(SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_A), 0, },
	{SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_B, ARRAY_SIZE(SEQ_NT36672C_CSOT_BACK_TO_PAGE_1_B), 0, },
	{SEQ_NT36672C_CSOT_DISPLAY_OFF, ARRAY_SIZE(SEQ_NT36672C_CSOT_DISPLAY_OFF), 20*1000, },
	{SEQ_NT36672C_CSOT_SLEEP_IN, ARRAY_SIZE(SEQ_NT36672C_CSOT_SLEEP_IN), 0, },
};

static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	1, 1, 2, 2, 3, 3, 4, 4, 5, 5, /* 1: 1 */
	6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
	11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
	16, 16, 17, 17, 18, 19, 19, 20, 20, 21,
	22, 22, 23, 23, 24, 25, 25, 26, 26, 27,
	28, 28, 29, 29, 30, 31, 31, 32, 32, 33,
	34, 34, 35, 35, 36, 37, 37, 38, 38, 39,
	40, 40, 41, 41, 42, 43, 43, 44, 44, 45,
	46, 47, 47, 48, 49, 50, 51, 51, 52, 53,
	54, 55, 55, 56, 57, 58, 58, 59, 60, 61,
	62, 62, 63, 64, 65, 66, 66, 67, 68, 69,
	70, 70, 71, 72, 73, 74, 74, 75, 76, 77,
	77, 78, 79, 80, 81, 81, 82, 83, 84, 85, /* 128: 83 */
	86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100, 101, 102, 103, 104, 105,
	106, 107, 108, 109, 110, 111, 112, 113, 114, 116,
	117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
	127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
	137, 138, 139, 140, 141, 142, 143, 144, 145, 146,
	147, 148, 149, 150, 151, 152, 153, 154, 155, 156,
	157, 158, 159, 160, 161, 162, 163, 164, 165, 166,
	167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
	177, 178, 179, 181, 182, 183, 184, 185, 186, 187,
	188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
	198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215, 215, 216, /* 255: 212 */
	217, 218, 219, 220, 220, 221, 222, 223, 224, 225,
	225, 226, 227, 228, 229, 230, 231, 231, 232, 233,
	234, 235, 236, 236, 237, 238, 239, 240, 241, 242,
	242, 243, 244, 245, 246, 247, 247, 248, 249, 250,
	251, 252, 252, 253, 254, 255,
};
#endif /* __NT36672C_CSOT_PARAM_H__ */
