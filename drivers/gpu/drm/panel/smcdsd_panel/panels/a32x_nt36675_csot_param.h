/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NT36675_PARAM_H__
#define __NT36675_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define LCD_TYPE_VENDOR		"CSO"

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define LDI_REG_BRIGHTNESS	0x51
#define LDI_REG_ID		0xDA
#define LDI_LEN_BRIGHTNESS	((u16)ARRAY_SIZE(SEQ_NT36675_BRIGHTNESS))

/* len is read length */
#define LDI_LEN_ID		3

#define TYPE_WRITE		I2C_SMBUS_WRITE
#define TYPE_DELAY		U8_MAX

static u8 LM36274_INIT[] = {
	TYPE_WRITE, 0x0C, 0x2C,
	TYPE_WRITE, 0x0D, 0x26,
	TYPE_WRITE, 0x0E, 0x26,
	TYPE_WRITE, 0x09, 0xBE,
	TYPE_WRITE, 0x02, 0x6B,
	TYPE_WRITE, 0x03, 0x2F,
	TYPE_WRITE, 0x11, 0x74,
	TYPE_WRITE, 0x04, 0x02,
	TYPE_WRITE, 0x05, 0xBB,
	TYPE_WRITE, 0x10, 0x67,
	TYPE_WRITE, 0x08, 0x13,
};

static u8 LM36274_EXIT[] = {
	TYPE_WRITE, 0x09, 0x18,
	TYPE_DELAY, 1, 0,
	TYPE_WRITE, 0x08, 0x00,
};

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static unsigned char SEQ_NT36675_SLEEP_OUT[] = { 0x11 };
static unsigned char SEQ_NT36675_SLEEP_IN[] = { 0x10 };
static unsigned char SEQ_NT36675_DISPLAY_OFF[] = { 0x28 };
static unsigned char SEQ_NT36675_DISPLAY_ON[] = { 0x29 };

static unsigned char SEQ_NT36675_BRIGHTNESS[] = {
	0x51,
	0x00,
};

static unsigned char SEQ_NT36675_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

static unsigned char SEQ_NT36675_001[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36675_002[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_003[] = {
	0xB0,
	0x00,
};

static unsigned char SEQ_NT36675_004[] = {
	0xBA,
	0x03,
};

static unsigned char SEQ_NT36675_005[] = {
	0xFF,
	0x20,
};

static unsigned char SEQ_NT36675_006[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_007[] = {
	0x06,
	0x64,
};

static unsigned char SEQ_NT36675_008[] = {
	0x07,
	0x28,
};

static unsigned char SEQ_NT36675_009[] = {
	0x69,
	0x92,
};

static unsigned char SEQ_NT36675_010[] = {
	0x95,
	0xD1,
};

static unsigned char SEQ_NT36675_011[] = {
	0x96,
	0xD1,
};

static unsigned char SEQ_NT36675_012[] = {
	0xF2,
	0x54,
};

static unsigned char SEQ_NT36675_013[] = {
	0xF4,
	0x54,
};

static unsigned char SEQ_NT36675_014[] = {
	0xF6,
	0x54,
};

static unsigned char SEQ_NT36675_015[] = {
	0xF8,
	0x54,
};

static unsigned char SEQ_NT36675_016[] = {
	0x1F,
	0x02,
};

static unsigned char SEQ_NT36675_017[] = {
	0xFF,
	0x24,
};

static unsigned char SEQ_NT36675_018[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_019[] = {
	0x00,
	0x13,
};

static unsigned char SEQ_NT36675_020[] = {
	0x01,
	0x15,
};

static unsigned char SEQ_NT36675_021[] = {
	0x02,
	0x17,
};

static unsigned char SEQ_NT36675_022[] = {
	0x03,
	0x26,
};

static unsigned char SEQ_NT36675_023[] = {
	0x04,
	0x27,
};

static unsigned char SEQ_NT36675_024[] = {
	0x05,
	0x28,
};

static unsigned char SEQ_NT36675_025[] = {
	0x07,
	0x24,
};

static unsigned char SEQ_NT36675_026[] = {
	0x08,
	0x24,
};

static unsigned char SEQ_NT36675_027[] = {
	0x0A,
	0x22,
};

static unsigned char SEQ_NT36675_028[] = {
	0x0C,
	0x00,
};

static unsigned char SEQ_NT36675_029[] = {
	0x0D,
	0x0F,
};

static unsigned char SEQ_NT36675_030[] = {
	0x0E,
	0x01,
};

static unsigned char SEQ_NT36675_031[] = {
	0x10,
	0x03,
};

static unsigned char SEQ_NT36675_032[] = {
	0x11,
	0x04,
};

static unsigned char SEQ_NT36675_033[] = {
	0x12,
	0x05,
};

static unsigned char SEQ_NT36675_034[] = {
	0x13,
	0x06,
};

static unsigned char SEQ_NT36675_035[] = {
	0x15,
	0x0B,
};

static unsigned char SEQ_NT36675_036[] = {
	0x17,
	0x0C,
};

static unsigned char SEQ_NT36675_037[] = {
	0x18,
	0x13,
};

static unsigned char SEQ_NT36675_038[] = {
	0x19,
	0x15,
};

static unsigned char SEQ_NT36675_039[] = {
	0x1A,
	0x17,
};

static unsigned char SEQ_NT36675_040[] = {
	0x1B,
	0x26,
};

static unsigned char SEQ_NT36675_041[] = {
	0x1C,
	0x27,
};

static unsigned char SEQ_NT36675_042[] = {
	0x1D,
	0x28,
};

static unsigned char SEQ_NT36675_043[] = {
	0x1F,
	0x24,
};

static unsigned char SEQ_NT36675_044[] = {
	0x20,
	0x24,
};

static unsigned char SEQ_NT36675_045[] = {
	0x22,
	0x22,
};

static unsigned char SEQ_NT36675_046[] = {
	0x24,
	0x00,
};

static unsigned char SEQ_NT36675_047[] = {
	0x25,
	0x0F,
};

static unsigned char SEQ_NT36675_048[] = {
	0x26,
	0x01,
};

static unsigned char SEQ_NT36675_049[] = {
	0x28,
	0x03,
};

static unsigned char SEQ_NT36675_050[] = {
	0x29,
	0x04,
};

static unsigned char SEQ_NT36675_051[] = {
	0x2A,
	0x05,
};

static unsigned char SEQ_NT36675_052[] = {
	0x2B,
	0x06,
};

static unsigned char SEQ_NT36675_053[] = {
	0x2F,
	0x0B,
};

static unsigned char SEQ_NT36675_054[] = {
	0x31,
	0x0C,
};

static unsigned char SEQ_NT36675_055[] = {
	0x32,
	0x09,
};

static unsigned char SEQ_NT36675_056[] = {
	0x33,
	0x03,
};

static unsigned char SEQ_NT36675_057[] = {
	0x34,
	0x03,
};

static unsigned char SEQ_NT36675_058[] = {
	0x35,
	0x01,
};

static unsigned char SEQ_NT36675_059[] = {
	0x36,
	0x68,
};

static unsigned char SEQ_NT36675_060[] = {
	0x45,
	0x04,
};

static unsigned char SEQ_NT36675_061[] = {
	0x47,
	0x04,
};

static unsigned char SEQ_NT36675_062[] = {
	0x4B,
	0x49,
};

static unsigned char SEQ_NT36675_063[] = {
	0x4E,
	0x75,
};

static unsigned char SEQ_NT36675_064[] = {
	0x4F,
	0x75,
};

static unsigned char SEQ_NT36675_065[] = {
	0x50,
	0x02,
};

static unsigned char SEQ_NT36675_066[] = {
	0x53,
	0x75,
};

static unsigned char SEQ_NT36675_067[] = {
	0x79,
	0x22,
};

static unsigned char SEQ_NT36675_068[] = {
	0x7A,
	0x82,
};

static unsigned char SEQ_NT36675_069[] = {
	0x7B,
	0x90,
};

static unsigned char SEQ_NT36675_070[] = {
	0x7D,
	0x03,
};

static unsigned char SEQ_NT36675_071[] = {
	0x80,
	0x03,
};

static unsigned char SEQ_NT36675_072[] = {
	0x81,
	0x03,
};

static unsigned char SEQ_NT36675_073[] = {
	0x82,
	0x16,
};

static unsigned char SEQ_NT36675_074[] = {
	0x83,
	0x25,
};

static unsigned char SEQ_NT36675_075[] = {
	0x84,
	0x34,
};

static unsigned char SEQ_NT36675_076[] = {
	0x85,
	0x43,
};

static unsigned char SEQ_NT36675_077[] = {
	0x86,
	0x52,
};

static unsigned char SEQ_NT36675_078[] = {
	0x87,
	0x61,
};

static unsigned char SEQ_NT36675_079[] = {
	0x90,
	0x43,
};

static unsigned char SEQ_NT36675_080[] = {
	0x91,
	0x52,
};

static unsigned char SEQ_NT36675_081[] = {
	0x92,
	0x61,
};

static unsigned char SEQ_NT36675_082[] = {
	0x93,
	0x16,
};

static unsigned char SEQ_NT36675_083[] = {
	0x94,
	0x25,
};

static unsigned char SEQ_NT36675_084[] = {
	0x95,
	0x34,
};

static unsigned char SEQ_NT36675_085[] = {
	0x9C,
	0xF4,
};

static unsigned char SEQ_NT36675_086[] = {
	0x9D,
	0x01,
};

static unsigned char SEQ_NT36675_087[] = {
	0xA0,
	0x10,
};

static unsigned char SEQ_NT36675_088[] = {
	0xA2,
	0x10,
};

static unsigned char SEQ_NT36675_089[] = {
	0xA3,
	0x03,
};

static unsigned char SEQ_NT36675_090[] = {
	0xA4,
	0x04,
};

static unsigned char SEQ_NT36675_091[] = {
	0xA5,
	0x04,
};

static unsigned char SEQ_NT36675_092[] = {
	0xAA,
	0x05,
};

static unsigned char SEQ_NT36675_093[] = {
	0xAD,
	0x02,
};

static unsigned char SEQ_NT36675_094[] = {
	0xB0,
	0x0B,
};

static unsigned char SEQ_NT36675_095[] = {
	0xB3,
	0x08,
};

static unsigned char SEQ_NT36675_096[] = {
	0xB6,
	0x11,
};

static unsigned char SEQ_NT36675_097[] = {
	0xB9,
	0x0E,
};

static unsigned char SEQ_NT36675_098[] = {
	0xBC,
	0x17,
};

static unsigned char SEQ_NT36675_099[] = {
	0xBF,
	0x14,
};

static unsigned char SEQ_NT36675_100[] = {
	0xC4,
	0xC0,
};

static unsigned char SEQ_NT36675_101[] = {
	0xCA,
	0xC9,
};

static unsigned char SEQ_NT36675_102[] = {
	0xD9,
	0x80,
};

static unsigned char SEQ_NT36675_103[] = {
	0xE5,
	0x06,
};

static unsigned char SEQ_NT36675_104[] = {
	0xE6,
	0x80,
};

static unsigned char SEQ_NT36675_105[] = {
	0xE9,
	0x03,
};

static unsigned char SEQ_NT36675_106[] = {
	0xED,
	0xC0,
};

static unsigned char SEQ_NT36675_107[] = {
	0xFF,
	0x25,
};

static unsigned char SEQ_NT36675_108[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_109[] = {
	0x67,
	0x21,
};

static unsigned char SEQ_NT36675_110[] = {
	0x68,
	0x58,
};

static unsigned char SEQ_NT36675_111[] = {
	0x69,
	0x10,
};

static unsigned char SEQ_NT36675_112[] = {
	0x6B,
	0x00,
};

static unsigned char SEQ_NT36675_113[] = {
	0x71,
	0x6D,
};

static unsigned char SEQ_NT36675_114[] = {
	0x77,
	0x72,
};

static unsigned char SEQ_NT36675_115[] = {
	0x7E,
	0x2D,
};

static unsigned char SEQ_NT36675_116[] = {
	0xC3,
	0x07,
};

static unsigned char SEQ_NT36675_117[] = {
	0xF0,
	0x05,
};

static unsigned char SEQ_NT36675_118[] = {
	0xF1,
	0x04,
};

static unsigned char SEQ_NT36675_119[] = {
	0xFF,
	0x26,
};

static unsigned char SEQ_NT36675_120[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_121[] = {
	0x01,
	0x2A,
};

static unsigned char SEQ_NT36675_122[] = {
	0x04,
	0x2A,
};

static unsigned char SEQ_NT36675_123[] = {
	0x05,
	0x08,
};

static unsigned char SEQ_NT36675_124[] = {
	0x06,
	0x20,
};

static unsigned char SEQ_NT36675_125[] = {
	0x08,
	0x20,
};

static unsigned char SEQ_NT36675_126[] = {
	0x74,
	0xAF,
};

static unsigned char SEQ_NT36675_127[] = {
	0x81,
	0x11,
};

static unsigned char SEQ_NT36675_128[] = {
	0x83,
	0x03,
};

static unsigned char SEQ_NT36675_129[] = {
	0x84,
	0x02,
};

static unsigned char SEQ_NT36675_130[] = {
	0x85,
	0x01,
};

static unsigned char SEQ_NT36675_131[] = {
	0x86,
	0x02,
};

static unsigned char SEQ_NT36675_132[] = {
	0x87,
	0x01,
};

static unsigned char SEQ_NT36675_133[] = {
	0x88,
	0x08,
};

static unsigned char SEQ_NT36675_134[] = {
	0x8A,
	0x1A,
};

static unsigned char SEQ_NT36675_135[] = {
	0x8C,
	0x54,
};

static unsigned char SEQ_NT36675_136[] = {
	0x8D,
	0x63,
};

static unsigned char SEQ_NT36675_137[] = {
	0x8E,
	0x72,
};

static unsigned char SEQ_NT36675_138[] = {
	0x8F,
	0x27,
};

static unsigned char SEQ_NT36675_139[] = {
	0x90,
	0x36,
};

static unsigned char SEQ_NT36675_140[] = {
	0x91,
	0x45,
};

static unsigned char SEQ_NT36675_141[] = {
	0x9A,
	0x80,
};

static unsigned char SEQ_NT36675_142[] = {
	0x9B,
	0x04,
};

static unsigned char SEQ_NT36675_143[] = {
	0x9C,
	0x00,
};

static unsigned char SEQ_NT36675_144[] = {
	0x9D,
	0x00,
};

static unsigned char SEQ_NT36675_145[] = {
	0x9E,
	0x00,
};

static unsigned char SEQ_NT36675_146[] = {
	0xFF,
	0x27,
};

static unsigned char SEQ_NT36675_147[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_148[] = {
	0x00,
	0x62,
};

static unsigned char SEQ_NT36675_149[] = {
	0x01,
	0x40,
};

static unsigned char SEQ_NT36675_150[] = {
	0x02,
	0xD0,
};

static unsigned char SEQ_NT36675_151[] = {
	0x20,
	0x80,
};

static unsigned char SEQ_NT36675_152[] = {
	0x21,
	0x8E,
};

static unsigned char SEQ_NT36675_153[] = {
	0x25,
	0x81,
};

static unsigned char SEQ_NT36675_154[] = {
	0x26,
	0xC8,
};

static unsigned char SEQ_NT36675_155[] = {
	0x88,
	0x03,
};

static unsigned char SEQ_NT36675_156[] = {
	0x89,
	0x03,
};

static unsigned char SEQ_NT36675_157[] = {
	0x8A,
	0x02,
};

static unsigned char SEQ_NT36675_158[] = {
	0xF9,
	0x00,
};

static unsigned char SEQ_NT36675_159[] = {
	0xFF,
	0x2A,
};

static unsigned char SEQ_NT36675_160[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_161[] = {
	0x07,
	0x64,
};

static unsigned char SEQ_NT36675_162[] = {
	0x0A,
	0x30,
};

static unsigned char SEQ_NT36675_163[] = {
	0x0C,
	0x04,
};

static unsigned char SEQ_NT36675_164[] = {
	0x0E,
	0x01,
};

static unsigned char SEQ_NT36675_165[] = {
	0x0F,
	0x01,
};

static unsigned char SEQ_NT36675_166[] = {
	0x11,
	0x40,
};

static unsigned char SEQ_NT36675_167[] = {
	0x15,
	0x12,
};

static unsigned char SEQ_NT36675_168[] = {
	0x16,
	0xAE,
};

static unsigned char SEQ_NT36675_169[] = {
	0x19,
	0x12,
};

static unsigned char SEQ_NT36675_170[] = {
	0x1A,
	0x82,
};

static unsigned char SEQ_NT36675_171[] = {
	0x1B,
	0x0A,
};

static unsigned char SEQ_NT36675_172[] = {
	0x1D,
	0x61,
};

static unsigned char SEQ_NT36675_173[] = {
	0x1E,
	0x7C,
};

static unsigned char SEQ_NT36675_174[] = {
	0x1F,
	0x84,
};

static unsigned char SEQ_NT36675_175[] = {
	0x20,
	0x7C,
};

static unsigned char SEQ_NT36675_176[] = {
	0x45,
	0xA0,
};

static unsigned char SEQ_NT36675_177[] = {
	0x75,
	0x6A,
};

static unsigned char SEQ_NT36675_178[] = {
	0xA5,
	0x50,
};

static unsigned char SEQ_NT36675_179[] = {
	0xD8,
	0x40,
};

static unsigned char SEQ_NT36675_180[] = {
	0xFF,
	0x2C,
};

static unsigned char SEQ_NT36675_181[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_182[] = {
	0xFF,
	0xE0,
};

static unsigned char SEQ_NT36675_183[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_184[] = {
	0xFF,
	0xF0,
};

static unsigned char SEQ_NT36675_185[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_186[] = {
	0xBB,
	0x40,
};

static unsigned char SEQ_NT36675_187[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36675_188[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_189[] = {
	0xFF,
	0x20,
};

static unsigned char SEQ_NT36675_190[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_191[] = {
	0xB0,
	0x00, 0x00, 0x00, 0x37, 0x00, 0x70, 0x00, 0x97, 0x00, 0xB6,
	0x00, 0xD0, 0x00, 0xE6, 0x00, 0xF9,
};

static unsigned char SEQ_NT36675_192[] = {
	0xB1,
	0x01, 0x0A, 0x01, 0x43, 0x01, 0x6A, 0x01, 0xAA, 0x01, 0xD5,
	0x02, 0x1B, 0x02, 0x4E, 0x02, 0x50,
};

static unsigned char SEQ_NT36675_193[] = {
	0xB2,
	0x02, 0x82, 0x02, 0xBB, 0x02, 0xE1, 0x03, 0x0F, 0x03, 0x2F,
	0x03, 0x54, 0x03, 0x62, 0x03, 0x6F,
};

static unsigned char SEQ_NT36675_194[] = {
	0xB3,
	0x03, 0x7F, 0x03, 0x91, 0x03, 0xA3, 0x03, 0xB0, 0x03, 0xD3,
	0x03, 0xD8,
};

static unsigned char SEQ_NT36675_195[] = {
	0xB4,
	0x00, 0x00, 0x00, 0x37, 0x00, 0x70, 0x00, 0x97, 0x00, 0xB6,
	0x00, 0xD0, 0x00, 0xE6, 0x00, 0xF9,
};

static unsigned char SEQ_NT36675_196[] = {
	0xB5,
	0x01, 0x0A, 0x01, 0x43, 0x01, 0x6A, 0x01, 0xAA, 0x01, 0xD5,
	0x02, 0x1B, 0x02, 0x4E, 0x02, 0x50,
};

static unsigned char SEQ_NT36675_197[] = {
	0xB6,
	0x02, 0x82, 0x02, 0xBB, 0x02, 0xE1, 0x03, 0x0F, 0x03, 0x2F,
	0x03, 0x54, 0x03, 0x62, 0x03, 0x6F,
};

static unsigned char SEQ_NT36675_198[] = {
	0xB7,
	0x03, 0x7F, 0x03, 0x91, 0x03, 0xA3, 0x03, 0xB0, 0x03, 0xD3,
	0x03, 0xD8,
};

static unsigned char SEQ_NT36675_199[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x37, 0x00, 0x70, 0x00, 0x97, 0x00, 0xB6,
	0x00, 0xD0, 0x00, 0xE6, 0x00, 0xF9,
};

static unsigned char SEQ_NT36675_200[] = {
	0xB9,
	0x01, 0x0A, 0x01, 0x43, 0x01, 0x6A, 0x01, 0xAA, 0x01, 0xD5,
	0x02, 0x1B, 0x02, 0x4E, 0x02, 0x50,
};

static unsigned char SEQ_NT36675_201[] = {
	0xBA,
	0x02, 0x82, 0x02, 0xBB, 0x02, 0xE1, 0x03, 0x0F, 0x03, 0x2F,
	0x03, 0x54, 0x03, 0x62, 0x03, 0x6F,
};

static unsigned char SEQ_NT36675_202[] = {
	0xBB,
	0x03, 0x7F, 0x03, 0x91, 0x03, 0xA3, 0x03, 0xB0, 0x03, 0xD3,
	0x03, 0xD8,
};

static unsigned char SEQ_NT36675_203[] = {
	0xC6,
	0x11,
};

static unsigned char SEQ_NT36675_204[] = {
	0xC7,
	0x11,
};

static unsigned char SEQ_NT36675_205[] = {
	0xC8,
	0x11,
};

static unsigned char SEQ_NT36675_206[] = {
	0xC9,
	0x11,
};

static unsigned char SEQ_NT36675_207[] = {
	0xCA,
	0x11,
};

static unsigned char SEQ_NT36675_208[] = {
	0xCB,
	0x11,
};

static unsigned char SEQ_NT36675_209[] = {
	0xCC,
	0x11,
};

static unsigned char SEQ_NT36675_210[] = {
	0xCD,
	0x11,
};

static unsigned char SEQ_NT36675_211[] = {
	0xCE,
	0x11,
};

static unsigned char SEQ_NT36675_212[] = {
	0xCF,
	0x11,
};

static unsigned char SEQ_NT36675_213[] = {
	0xD0,
	0x11,
};

static unsigned char SEQ_NT36675_214[] = {
	0xD1,
	0x11,
};

static unsigned char SEQ_NT36675_215[] = {
	0xD2,
	0x11,
};

static unsigned char SEQ_NT36675_216[] = {
	0xD3,
	0x11,
};

static unsigned char SEQ_NT36675_217[] = {
	0xD4,
	0x11,
};

static unsigned char SEQ_NT36675_218[] = {
	0xD5,
	0x11,
};

static unsigned char SEQ_NT36675_219[] = {
	0xD6,
	0x11,
};

static unsigned char SEQ_NT36675_220[] = {
	0xD7,
	0x11,
};

static unsigned char SEQ_NT36675_221[] = {
	0xD8,
	0x11,
};

static unsigned char SEQ_NT36675_222[] = {
	0xD9,
	0x11,
};

static unsigned char SEQ_NT36675_223[] = {
	0xDA,
	0x11,
};

static unsigned char SEQ_NT36675_224[] = {
	0xDB,
	0x11,
};

static unsigned char SEQ_NT36675_225[] = {
	0xDC,
	0x11,
};

static unsigned char SEQ_NT36675_226[] = {
	0xDD,
	0x11,
};

static unsigned char SEQ_NT36675_227[] = {
	0xDE,
	0x11,
};

static unsigned char SEQ_NT36675_228[] = {
	0xDF,
	0x11,
};

static unsigned char SEQ_NT36675_229[] = {
	0xE0,
	0x11,
};

static unsigned char SEQ_NT36675_230[] = {
	0xE1,
	0x11,
};

static unsigned char SEQ_NT36675_231[] = {
	0xE2,
	0x11,
};

static unsigned char SEQ_NT36675_232[] = {
	0xE3,
	0x11,
};

static unsigned char SEQ_NT36675_233[] = {
	0xE4,
	0x11,
};

static unsigned char SEQ_NT36675_234[] = {
	0xE5,
	0x11,
};

static unsigned char SEQ_NT36675_235[] = {
	0xE6,
	0x11,
};

static unsigned char SEQ_NT36675_236[] = {
	0xE7,
	0x11,
};

static unsigned char SEQ_NT36675_237[] = {
	0xE8,
	0x11,
};

static unsigned char SEQ_NT36675_238[] = {
	0xE9,
	0x11,
};

static unsigned char SEQ_NT36675_239[] = {
	0xFF,
	0x21,
};

static unsigned char SEQ_NT36675_240[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_241[] = {
	0xB0,
	0x00, 0x00, 0x00, 0x37, 0x00, 0x70, 0x00, 0x97, 0x00, 0xB6,
	0x00, 0xD0, 0x00, 0xE6, 0x00, 0xF9,
};

static unsigned char SEQ_NT36675_242[] = {
	0xB1,
	0x01, 0x0A, 0x01, 0x43, 0x01, 0x6A, 0x01, 0xAA, 0x01, 0xD5,
	0x02, 0x1B, 0x02, 0x4E, 0x02, 0x50,
};

static unsigned char SEQ_NT36675_243[] = {
	0xB2,
	0x02, 0x82, 0x02, 0xBB, 0x02, 0xE1, 0x03, 0x0F, 0x03, 0x2F,
	0x03, 0x54, 0x03, 0x62, 0x03, 0x6F,
};

static unsigned char SEQ_NT36675_244[] = {
	0xB3,
	0x03, 0x7F, 0x03, 0x91, 0x03, 0xA3, 0x03, 0xB0, 0x03, 0xD3,
	0x03, 0xD8,
};

static unsigned char SEQ_NT36675_245[] = {
	0xB4,
	0x00, 0x00, 0x00, 0x37, 0x00, 0x70, 0x00, 0x97, 0x00, 0xB6,
	0x00, 0xD0, 0x00, 0xE6, 0x00, 0xF9,
};

static unsigned char SEQ_NT36675_246[] = {
	0xB5,
	0x01, 0x0A, 0x01, 0x43, 0x01, 0x6A, 0x01, 0xAA, 0x01, 0xD5,
	0x02, 0x1B, 0x02, 0x4E, 0x02, 0x50,
};

static unsigned char SEQ_NT36675_247[] = {
	0xB6,
	0x02, 0x82, 0x02, 0xBB, 0x02, 0xE1, 0x03, 0x0F, 0x03, 0x2F,
	0x03, 0x54, 0x03, 0x62, 0x03, 0x6F,
};

static unsigned char SEQ_NT36675_248[] = {
	0xB7,
	0x03, 0x7F, 0x03, 0x91, 0x03, 0xA3, 0x03, 0xB0, 0x03, 0xD3,
	0x03, 0xD8,
};

static unsigned char SEQ_NT36675_249[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x37, 0x00, 0x70, 0x00, 0x97, 0x00, 0xB6,
	0x00, 0xD0, 0x00, 0xE6, 0x00, 0xF9,
};

static unsigned char SEQ_NT36675_250[] = {
	0xB9,
	0x01, 0x0A, 0x01, 0x43, 0x01, 0x6A, 0x01, 0xAA, 0x01, 0xD5,
	0x02, 0x1B, 0x02, 0x4E, 0x02, 0x50,
};

static unsigned char SEQ_NT36675_251[] = {
	0xBA,
	0x02, 0x82, 0x02, 0xBB, 0x02, 0xE1, 0x03, 0x0F, 0x03, 0x2F,
	0x03, 0x54, 0x03, 0x62, 0x03, 0x6F,
};

static unsigned char SEQ_NT36675_252[] = {
	0xBB,
	0x03, 0x7F, 0x03, 0x91, 0x03, 0xA3, 0x03, 0xB0, 0x03, 0xD3,
	0x03, 0xD8,
};

static unsigned char SEQ_NT36675_253[] = {
	0xFF,
	0x23,
};

static unsigned char SEQ_NT36675_254[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_255[] = {
	0x00,
	0x00,
};

static unsigned char SEQ_NT36675_256[] = {
	0x07,
	0x60,
};

static unsigned char SEQ_NT36675_257[] = {
	0x08,
	0x06,
};

static unsigned char SEQ_NT36675_258[] = {
	0x09,
	0x1C,
};

static unsigned char SEQ_NT36675_259[] = {
	0x04,
	0x00,
};

static unsigned char SEQ_NT36675_260[] = {
	0x0A,
	0x00,
};

static unsigned char SEQ_NT36675_261[] = {
	0x0B,
	0x00,
};

static unsigned char SEQ_NT36675_262[] = {
	0x0C,
	0x00,
};

static unsigned char SEQ_NT36675_263[] = {
	0x0D,
	0x00,
};

static unsigned char SEQ_NT36675_264[] = {
	0x11,
	0x00,
};

static unsigned char SEQ_NT36675_265[] = {
	0x12,
	0xB4,
};

static unsigned char SEQ_NT36675_266[] = {
	0x15,
	0x75,
};

static unsigned char SEQ_NT36675_267[] = {
	0x16,
	0x0A,
};

static unsigned char SEQ_NT36675_268[] = {
	0x19,
	0x1D,
};

static unsigned char SEQ_NT36675_269[] = {
	0x1A,
	0x1D,
};

static unsigned char SEQ_NT36675_270[] = {
	0x1B,
	0x1B,
};

static unsigned char SEQ_NT36675_271[] = {
	0x1C,
	0x19,
};

static unsigned char SEQ_NT36675_272[] = {
	0x1D,
	0x19,
};

static unsigned char SEQ_NT36675_273[] = {
	0x1E,
	0x19,
};

static unsigned char SEQ_NT36675_274[] = {
	0x1F,
	0x1F,
};

static unsigned char SEQ_NT36675_275[] = {
	0x20,
	0x29,
};

static unsigned char SEQ_NT36675_276[] = {
	0x21,
	0x20,
};

static unsigned char SEQ_NT36675_277[] = {
	0x22,
	0x32,
};

static unsigned char SEQ_NT36675_278[] = {
	0x23,
	0x37,
};

static unsigned char SEQ_NT36675_279[] = {
	0x24,
	0x37,
};

static unsigned char SEQ_NT36675_280[] = {
	0x25,
	0x3A,
};

static unsigned char SEQ_NT36675_281[] = {
	0x26,
	0x3A,
};

static unsigned char SEQ_NT36675_282[] = {
	0x27,
	0x3A,
};

static unsigned char SEQ_NT36675_283[] = {
	0x28,
	0x3A,
};

static unsigned char SEQ_NT36675_284[] = {
	0x29,
	0x3F,
};

static unsigned char SEQ_NT36675_285[] = {
	0x2A,
	0x3F,
};

static unsigned char SEQ_NT36675_286[] = {
	0x2B,
	0x3F,
};

static unsigned char SEQ_NT36675_287[] = {
	0x30,
	0xFF,
};

static unsigned char SEQ_NT36675_288[] = {
	0x31,
	0xFF,
};

static unsigned char SEQ_NT36675_289[] = {
	0x32,
	0xFE,
};

static unsigned char SEQ_NT36675_290[] = {
	0x33,
	0xFD,
};

static unsigned char SEQ_NT36675_291[] = {
	0x34,
	0xFD,
};

static unsigned char SEQ_NT36675_292[] = {
	0x35,
	0xFC,
};

static unsigned char SEQ_NT36675_293[] = {
	0x36,
	0xFB,
};

static unsigned char SEQ_NT36675_294[] = {
	0x37,
	0xF9,
};

static unsigned char SEQ_NT36675_295[] = {
	0x38,
	0xF7,
};

static unsigned char SEQ_NT36675_296[] = {
	0x39,
	0xF3,
};

static unsigned char SEQ_NT36675_297[] = {
	0x3A,
	0xEA,
};

static unsigned char SEQ_NT36675_298[] = {
	0x3B,
	0xE6,
};

static unsigned char SEQ_NT36675_299[] = {
	0x3D,
	0xE0,
};

static unsigned char SEQ_NT36675_300[] = {
	0x3F,
	0xDD,
};

static unsigned char SEQ_NT36675_301[] = {
	0x40,
	0xDB,
};

static unsigned char SEQ_NT36675_302[] = {
	0x41,
	0xD9,
};

static unsigned char SEQ_NT36675_303[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36675_304[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36675_305[] = {
	0x25,
	0xA9,
};

static unsigned char SEQ_NT36675_306[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36675_307[] = {
	0xFB,
	0x01,
};


static struct lcd_seq_info LCD_SEQ_INIT_1[] = {
	{SEQ_NT36675_001, ARRAY_SIZE(SEQ_NT36675_001), 0, },
	{SEQ_NT36675_002, ARRAY_SIZE(SEQ_NT36675_002), 0, },
	{SEQ_NT36675_003, ARRAY_SIZE(SEQ_NT36675_003), 0, },
	{SEQ_NT36675_004, ARRAY_SIZE(SEQ_NT36675_004), 0, },
	{SEQ_NT36675_005, ARRAY_SIZE(SEQ_NT36675_005), 0, },
	{SEQ_NT36675_006, ARRAY_SIZE(SEQ_NT36675_006), 0, },
	{SEQ_NT36675_007, ARRAY_SIZE(SEQ_NT36675_007), 0, },
	{SEQ_NT36675_008, ARRAY_SIZE(SEQ_NT36675_008), 0, },
	{SEQ_NT36675_009, ARRAY_SIZE(SEQ_NT36675_009), 0, },
	{SEQ_NT36675_010, ARRAY_SIZE(SEQ_NT36675_010), 0, },
	{SEQ_NT36675_011, ARRAY_SIZE(SEQ_NT36675_011), 0, },
	{SEQ_NT36675_012, ARRAY_SIZE(SEQ_NT36675_012), 0, },
	{SEQ_NT36675_013, ARRAY_SIZE(SEQ_NT36675_013), 0, },
	{SEQ_NT36675_014, ARRAY_SIZE(SEQ_NT36675_014), 0, },
	{SEQ_NT36675_015, ARRAY_SIZE(SEQ_NT36675_015), 0, },
	{SEQ_NT36675_016, ARRAY_SIZE(SEQ_NT36675_016), 0, },
	{SEQ_NT36675_017, ARRAY_SIZE(SEQ_NT36675_017), 0, },
	{SEQ_NT36675_018, ARRAY_SIZE(SEQ_NT36675_018), 0, },
	{SEQ_NT36675_019, ARRAY_SIZE(SEQ_NT36675_019), 0, },
	{SEQ_NT36675_020, ARRAY_SIZE(SEQ_NT36675_020), 0, },
	{SEQ_NT36675_021, ARRAY_SIZE(SEQ_NT36675_021), 0, },
	{SEQ_NT36675_022, ARRAY_SIZE(SEQ_NT36675_022), 0, },
	{SEQ_NT36675_023, ARRAY_SIZE(SEQ_NT36675_023), 0, },
	{SEQ_NT36675_024, ARRAY_SIZE(SEQ_NT36675_024), 0, },
	{SEQ_NT36675_025, ARRAY_SIZE(SEQ_NT36675_025), 0, },
	{SEQ_NT36675_026, ARRAY_SIZE(SEQ_NT36675_026), 0, },
	{SEQ_NT36675_027, ARRAY_SIZE(SEQ_NT36675_027), 0, },
	{SEQ_NT36675_028, ARRAY_SIZE(SEQ_NT36675_028), 0, },
	{SEQ_NT36675_029, ARRAY_SIZE(SEQ_NT36675_029), 0, },
	{SEQ_NT36675_030, ARRAY_SIZE(SEQ_NT36675_030), 0, },
	{SEQ_NT36675_031, ARRAY_SIZE(SEQ_NT36675_031), 0, },
	{SEQ_NT36675_032, ARRAY_SIZE(SEQ_NT36675_032), 0, },
	{SEQ_NT36675_033, ARRAY_SIZE(SEQ_NT36675_033), 0, },
	{SEQ_NT36675_034, ARRAY_SIZE(SEQ_NT36675_034), 0, },
	{SEQ_NT36675_035, ARRAY_SIZE(SEQ_NT36675_035), 0, },
	{SEQ_NT36675_036, ARRAY_SIZE(SEQ_NT36675_036), 0, },
	{SEQ_NT36675_037, ARRAY_SIZE(SEQ_NT36675_037), 0, },
	{SEQ_NT36675_038, ARRAY_SIZE(SEQ_NT36675_038), 0, },
	{SEQ_NT36675_039, ARRAY_SIZE(SEQ_NT36675_039), 0, },
	{SEQ_NT36675_040, ARRAY_SIZE(SEQ_NT36675_040), 0, },
	{SEQ_NT36675_041, ARRAY_SIZE(SEQ_NT36675_041), 0, },
	{SEQ_NT36675_042, ARRAY_SIZE(SEQ_NT36675_042), 0, },
	{SEQ_NT36675_043, ARRAY_SIZE(SEQ_NT36675_043), 0, },
	{SEQ_NT36675_044, ARRAY_SIZE(SEQ_NT36675_044), 0, },
	{SEQ_NT36675_045, ARRAY_SIZE(SEQ_NT36675_045), 0, },
	{SEQ_NT36675_046, ARRAY_SIZE(SEQ_NT36675_046), 0, },
	{SEQ_NT36675_047, ARRAY_SIZE(SEQ_NT36675_047), 0, },
	{SEQ_NT36675_048, ARRAY_SIZE(SEQ_NT36675_048), 0, },
	{SEQ_NT36675_049, ARRAY_SIZE(SEQ_NT36675_049), 0, },
	{SEQ_NT36675_050, ARRAY_SIZE(SEQ_NT36675_050), 0, },
	{SEQ_NT36675_051, ARRAY_SIZE(SEQ_NT36675_051), 0, },
	{SEQ_NT36675_052, ARRAY_SIZE(SEQ_NT36675_052), 0, },
	{SEQ_NT36675_053, ARRAY_SIZE(SEQ_NT36675_053), 0, },
	{SEQ_NT36675_054, ARRAY_SIZE(SEQ_NT36675_054), 0, },
	{SEQ_NT36675_055, ARRAY_SIZE(SEQ_NT36675_055), 0, },
	{SEQ_NT36675_056, ARRAY_SIZE(SEQ_NT36675_056), 0, },
	{SEQ_NT36675_057, ARRAY_SIZE(SEQ_NT36675_057), 0, },
	{SEQ_NT36675_058, ARRAY_SIZE(SEQ_NT36675_058), 0, },
	{SEQ_NT36675_059, ARRAY_SIZE(SEQ_NT36675_059), 0, },
	{SEQ_NT36675_060, ARRAY_SIZE(SEQ_NT36675_060), 0, },
	{SEQ_NT36675_061, ARRAY_SIZE(SEQ_NT36675_061), 0, },
	{SEQ_NT36675_062, ARRAY_SIZE(SEQ_NT36675_062), 0, },
	{SEQ_NT36675_063, ARRAY_SIZE(SEQ_NT36675_063), 0, },
	{SEQ_NT36675_064, ARRAY_SIZE(SEQ_NT36675_064), 0, },
	{SEQ_NT36675_065, ARRAY_SIZE(SEQ_NT36675_065), 0, },
	{SEQ_NT36675_066, ARRAY_SIZE(SEQ_NT36675_066), 0, },
	{SEQ_NT36675_067, ARRAY_SIZE(SEQ_NT36675_067), 0, },
	{SEQ_NT36675_068, ARRAY_SIZE(SEQ_NT36675_068), 0, },
	{SEQ_NT36675_069, ARRAY_SIZE(SEQ_NT36675_069), 0, },
	{SEQ_NT36675_070, ARRAY_SIZE(SEQ_NT36675_070), 0, },
	{SEQ_NT36675_071, ARRAY_SIZE(SEQ_NT36675_071), 0, },
	{SEQ_NT36675_072, ARRAY_SIZE(SEQ_NT36675_072), 0, },
	{SEQ_NT36675_073, ARRAY_SIZE(SEQ_NT36675_073), 0, },
	{SEQ_NT36675_074, ARRAY_SIZE(SEQ_NT36675_074), 0, },
	{SEQ_NT36675_075, ARRAY_SIZE(SEQ_NT36675_075), 0, },
	{SEQ_NT36675_076, ARRAY_SIZE(SEQ_NT36675_076), 0, },
	{SEQ_NT36675_077, ARRAY_SIZE(SEQ_NT36675_077), 0, },
	{SEQ_NT36675_078, ARRAY_SIZE(SEQ_NT36675_078), 0, },
	{SEQ_NT36675_079, ARRAY_SIZE(SEQ_NT36675_079), 0, },
	{SEQ_NT36675_080, ARRAY_SIZE(SEQ_NT36675_080), 0, },
	{SEQ_NT36675_081, ARRAY_SIZE(SEQ_NT36675_081), 0, },
	{SEQ_NT36675_082, ARRAY_SIZE(SEQ_NT36675_082), 0, },
	{SEQ_NT36675_083, ARRAY_SIZE(SEQ_NT36675_083), 0, },
	{SEQ_NT36675_084, ARRAY_SIZE(SEQ_NT36675_084), 0, },
	{SEQ_NT36675_085, ARRAY_SIZE(SEQ_NT36675_085), 0, },
	{SEQ_NT36675_086, ARRAY_SIZE(SEQ_NT36675_086), 0, },
	{SEQ_NT36675_087, ARRAY_SIZE(SEQ_NT36675_087), 0, },
	{SEQ_NT36675_088, ARRAY_SIZE(SEQ_NT36675_088), 0, },
	{SEQ_NT36675_089, ARRAY_SIZE(SEQ_NT36675_089), 0, },
	{SEQ_NT36675_090, ARRAY_SIZE(SEQ_NT36675_090), 0, },
	{SEQ_NT36675_091, ARRAY_SIZE(SEQ_NT36675_091), 0, },
	{SEQ_NT36675_092, ARRAY_SIZE(SEQ_NT36675_092), 0, },
	{SEQ_NT36675_093, ARRAY_SIZE(SEQ_NT36675_093), 0, },
	{SEQ_NT36675_094, ARRAY_SIZE(SEQ_NT36675_094), 0, },
	{SEQ_NT36675_095, ARRAY_SIZE(SEQ_NT36675_095), 0, },
	{SEQ_NT36675_096, ARRAY_SIZE(SEQ_NT36675_096), 0, },
	{SEQ_NT36675_097, ARRAY_SIZE(SEQ_NT36675_097), 0, },
	{SEQ_NT36675_098, ARRAY_SIZE(SEQ_NT36675_098), 0, },
	{SEQ_NT36675_099, ARRAY_SIZE(SEQ_NT36675_099), 0, },
	{SEQ_NT36675_100, ARRAY_SIZE(SEQ_NT36675_100), 0, },
	{SEQ_NT36675_101, ARRAY_SIZE(SEQ_NT36675_101), 0, },
	{SEQ_NT36675_102, ARRAY_SIZE(SEQ_NT36675_102), 0, },
	{SEQ_NT36675_103, ARRAY_SIZE(SEQ_NT36675_103), 0, },
	{SEQ_NT36675_104, ARRAY_SIZE(SEQ_NT36675_104), 0, },
	{SEQ_NT36675_105, ARRAY_SIZE(SEQ_NT36675_105), 0, },
	{SEQ_NT36675_106, ARRAY_SIZE(SEQ_NT36675_106), 0, },
	{SEQ_NT36675_107, ARRAY_SIZE(SEQ_NT36675_107), 0, },
	{SEQ_NT36675_108, ARRAY_SIZE(SEQ_NT36675_108), 0, },
	{SEQ_NT36675_109, ARRAY_SIZE(SEQ_NT36675_109), 0, },
	{SEQ_NT36675_110, ARRAY_SIZE(SEQ_NT36675_110), 0, },
	{SEQ_NT36675_111, ARRAY_SIZE(SEQ_NT36675_111), 0, },
	{SEQ_NT36675_112, ARRAY_SIZE(SEQ_NT36675_112), 0, },
	{SEQ_NT36675_113, ARRAY_SIZE(SEQ_NT36675_113), 0, },
	{SEQ_NT36675_114, ARRAY_SIZE(SEQ_NT36675_114), 0, },
	{SEQ_NT36675_115, ARRAY_SIZE(SEQ_NT36675_115), 0, },
	{SEQ_NT36675_116, ARRAY_SIZE(SEQ_NT36675_116), 0, },
	{SEQ_NT36675_117, ARRAY_SIZE(SEQ_NT36675_117), 0, },
	{SEQ_NT36675_118, ARRAY_SIZE(SEQ_NT36675_118), 0, },
	{SEQ_NT36675_119, ARRAY_SIZE(SEQ_NT36675_119), 0, },
	{SEQ_NT36675_120, ARRAY_SIZE(SEQ_NT36675_120), 0, },
	{SEQ_NT36675_121, ARRAY_SIZE(SEQ_NT36675_121), 0, },
	{SEQ_NT36675_122, ARRAY_SIZE(SEQ_NT36675_122), 0, },
	{SEQ_NT36675_123, ARRAY_SIZE(SEQ_NT36675_123), 0, },
	{SEQ_NT36675_124, ARRAY_SIZE(SEQ_NT36675_124), 0, },
	{SEQ_NT36675_125, ARRAY_SIZE(SEQ_NT36675_125), 0, },
	{SEQ_NT36675_126, ARRAY_SIZE(SEQ_NT36675_126), 0, },
	{SEQ_NT36675_127, ARRAY_SIZE(SEQ_NT36675_127), 0, },
	{SEQ_NT36675_128, ARRAY_SIZE(SEQ_NT36675_128), 0, },
	{SEQ_NT36675_129, ARRAY_SIZE(SEQ_NT36675_129), 0, },
	{SEQ_NT36675_130, ARRAY_SIZE(SEQ_NT36675_130), 0, },
	{SEQ_NT36675_131, ARRAY_SIZE(SEQ_NT36675_131), 0, },
	{SEQ_NT36675_132, ARRAY_SIZE(SEQ_NT36675_132), 0, },
	{SEQ_NT36675_133, ARRAY_SIZE(SEQ_NT36675_133), 0, },
	{SEQ_NT36675_134, ARRAY_SIZE(SEQ_NT36675_134), 0, },
	{SEQ_NT36675_135, ARRAY_SIZE(SEQ_NT36675_135), 0, },
	{SEQ_NT36675_136, ARRAY_SIZE(SEQ_NT36675_136), 0, },
	{SEQ_NT36675_137, ARRAY_SIZE(SEQ_NT36675_137), 0, },
	{SEQ_NT36675_138, ARRAY_SIZE(SEQ_NT36675_138), 0, },
	{SEQ_NT36675_139, ARRAY_SIZE(SEQ_NT36675_139), 0, },
	{SEQ_NT36675_140, ARRAY_SIZE(SEQ_NT36675_140), 0, },
	{SEQ_NT36675_141, ARRAY_SIZE(SEQ_NT36675_141), 0, },
	{SEQ_NT36675_142, ARRAY_SIZE(SEQ_NT36675_142), 0, },
	{SEQ_NT36675_143, ARRAY_SIZE(SEQ_NT36675_143), 0, },
	{SEQ_NT36675_144, ARRAY_SIZE(SEQ_NT36675_144), 0, },
	{SEQ_NT36675_145, ARRAY_SIZE(SEQ_NT36675_145), 0, },
	{SEQ_NT36675_146, ARRAY_SIZE(SEQ_NT36675_146), 0, },
	{SEQ_NT36675_147, ARRAY_SIZE(SEQ_NT36675_147), 0, },
	{SEQ_NT36675_148, ARRAY_SIZE(SEQ_NT36675_148), 0, },
	{SEQ_NT36675_149, ARRAY_SIZE(SEQ_NT36675_149), 0, },
	{SEQ_NT36675_150, ARRAY_SIZE(SEQ_NT36675_150), 0, },
	{SEQ_NT36675_151, ARRAY_SIZE(SEQ_NT36675_151), 0, },
	{SEQ_NT36675_152, ARRAY_SIZE(SEQ_NT36675_152), 0, },
	{SEQ_NT36675_153, ARRAY_SIZE(SEQ_NT36675_153), 0, },
	{SEQ_NT36675_154, ARRAY_SIZE(SEQ_NT36675_154), 0, },
	{SEQ_NT36675_155, ARRAY_SIZE(SEQ_NT36675_155), 0, },
	{SEQ_NT36675_156, ARRAY_SIZE(SEQ_NT36675_156), 0, },
	{SEQ_NT36675_157, ARRAY_SIZE(SEQ_NT36675_157), 0, },
	{SEQ_NT36675_158, ARRAY_SIZE(SEQ_NT36675_158), 0, },
	{SEQ_NT36675_159, ARRAY_SIZE(SEQ_NT36675_159), 0, },
	{SEQ_NT36675_160, ARRAY_SIZE(SEQ_NT36675_160), 0, },
	{SEQ_NT36675_161, ARRAY_SIZE(SEQ_NT36675_161), 0, },
	{SEQ_NT36675_162, ARRAY_SIZE(SEQ_NT36675_162), 0, },
	{SEQ_NT36675_163, ARRAY_SIZE(SEQ_NT36675_163), 0, },
	{SEQ_NT36675_164, ARRAY_SIZE(SEQ_NT36675_164), 0, },
	{SEQ_NT36675_165, ARRAY_SIZE(SEQ_NT36675_165), 0, },
	{SEQ_NT36675_166, ARRAY_SIZE(SEQ_NT36675_166), 0, },
	{SEQ_NT36675_167, ARRAY_SIZE(SEQ_NT36675_167), 0, },
	{SEQ_NT36675_168, ARRAY_SIZE(SEQ_NT36675_168), 0, },
	{SEQ_NT36675_169, ARRAY_SIZE(SEQ_NT36675_169), 0, },
	{SEQ_NT36675_170, ARRAY_SIZE(SEQ_NT36675_170), 0, },
	{SEQ_NT36675_171, ARRAY_SIZE(SEQ_NT36675_171), 0, },
	{SEQ_NT36675_172, ARRAY_SIZE(SEQ_NT36675_172), 0, },
	{SEQ_NT36675_173, ARRAY_SIZE(SEQ_NT36675_173), 0, },
	{SEQ_NT36675_174, ARRAY_SIZE(SEQ_NT36675_174), 0, },
	{SEQ_NT36675_175, ARRAY_SIZE(SEQ_NT36675_175), 0, },
	{SEQ_NT36675_176, ARRAY_SIZE(SEQ_NT36675_176), 0, },
	{SEQ_NT36675_177, ARRAY_SIZE(SEQ_NT36675_177), 0, },
	{SEQ_NT36675_178, ARRAY_SIZE(SEQ_NT36675_178), 0, },
	{SEQ_NT36675_179, ARRAY_SIZE(SEQ_NT36675_179), 0, },
	{SEQ_NT36675_180, ARRAY_SIZE(SEQ_NT36675_180), 0, },
	{SEQ_NT36675_181, ARRAY_SIZE(SEQ_NT36675_181), 0, },
	{SEQ_NT36675_182, ARRAY_SIZE(SEQ_NT36675_182), 0, },
	{SEQ_NT36675_183, ARRAY_SIZE(SEQ_NT36675_183), 0, },
	{SEQ_NT36675_184, ARRAY_SIZE(SEQ_NT36675_184), 0, },
	{SEQ_NT36675_185, ARRAY_SIZE(SEQ_NT36675_185), 0, },
	{SEQ_NT36675_186, ARRAY_SIZE(SEQ_NT36675_186), 0, },
	{SEQ_NT36675_187, ARRAY_SIZE(SEQ_NT36675_187), 0, },
	{SEQ_NT36675_188, ARRAY_SIZE(SEQ_NT36675_188), 0, },
	{SEQ_NT36675_189, ARRAY_SIZE(SEQ_NT36675_189), 0, },
	{SEQ_NT36675_190, ARRAY_SIZE(SEQ_NT36675_190), 0, },
	{SEQ_NT36675_191, ARRAY_SIZE(SEQ_NT36675_191), 0, },
	{SEQ_NT36675_192, ARRAY_SIZE(SEQ_NT36675_192), 0, },
	{SEQ_NT36675_193, ARRAY_SIZE(SEQ_NT36675_193), 0, },
	{SEQ_NT36675_194, ARRAY_SIZE(SEQ_NT36675_194), 0, },
	{SEQ_NT36675_195, ARRAY_SIZE(SEQ_NT36675_195), 0, },
	{SEQ_NT36675_196, ARRAY_SIZE(SEQ_NT36675_196), 0, },
	{SEQ_NT36675_197, ARRAY_SIZE(SEQ_NT36675_197), 0, },
	{SEQ_NT36675_198, ARRAY_SIZE(SEQ_NT36675_198), 0, },
	{SEQ_NT36675_199, ARRAY_SIZE(SEQ_NT36675_199), 0, },
	{SEQ_NT36675_200, ARRAY_SIZE(SEQ_NT36675_200), 0, },
	{SEQ_NT36675_201, ARRAY_SIZE(SEQ_NT36675_201), 0, },
	{SEQ_NT36675_202, ARRAY_SIZE(SEQ_NT36675_202), 0, },
	{SEQ_NT36675_203, ARRAY_SIZE(SEQ_NT36675_203), 0, },
	{SEQ_NT36675_204, ARRAY_SIZE(SEQ_NT36675_204), 0, },
	{SEQ_NT36675_205, ARRAY_SIZE(SEQ_NT36675_205), 0, },
	{SEQ_NT36675_206, ARRAY_SIZE(SEQ_NT36675_206), 0, },
	{SEQ_NT36675_207, ARRAY_SIZE(SEQ_NT36675_207), 0, },
	{SEQ_NT36675_208, ARRAY_SIZE(SEQ_NT36675_208), 0, },
	{SEQ_NT36675_209, ARRAY_SIZE(SEQ_NT36675_209), 0, },
	{SEQ_NT36675_210, ARRAY_SIZE(SEQ_NT36675_210), 0, },
	{SEQ_NT36675_211, ARRAY_SIZE(SEQ_NT36675_211), 0, },
	{SEQ_NT36675_212, ARRAY_SIZE(SEQ_NT36675_212), 0, },
	{SEQ_NT36675_213, ARRAY_SIZE(SEQ_NT36675_213), 0, },
	{SEQ_NT36675_214, ARRAY_SIZE(SEQ_NT36675_214), 0, },
	{SEQ_NT36675_215, ARRAY_SIZE(SEQ_NT36675_215), 0, },
	{SEQ_NT36675_216, ARRAY_SIZE(SEQ_NT36675_216), 0, },
	{SEQ_NT36675_217, ARRAY_SIZE(SEQ_NT36675_217), 0, },
	{SEQ_NT36675_218, ARRAY_SIZE(SEQ_NT36675_218), 0, },
	{SEQ_NT36675_219, ARRAY_SIZE(SEQ_NT36675_219), 0, },
	{SEQ_NT36675_220, ARRAY_SIZE(SEQ_NT36675_220), 0, },
	{SEQ_NT36675_221, ARRAY_SIZE(SEQ_NT36675_221), 0, },
	{SEQ_NT36675_222, ARRAY_SIZE(SEQ_NT36675_222), 0, },
	{SEQ_NT36675_223, ARRAY_SIZE(SEQ_NT36675_223), 0, },
	{SEQ_NT36675_224, ARRAY_SIZE(SEQ_NT36675_224), 0, },
	{SEQ_NT36675_225, ARRAY_SIZE(SEQ_NT36675_225), 0, },
	{SEQ_NT36675_226, ARRAY_SIZE(SEQ_NT36675_226), 0, },
	{SEQ_NT36675_227, ARRAY_SIZE(SEQ_NT36675_227), 0, },
	{SEQ_NT36675_228, ARRAY_SIZE(SEQ_NT36675_228), 0, },
	{SEQ_NT36675_229, ARRAY_SIZE(SEQ_NT36675_229), 0, },
	{SEQ_NT36675_230, ARRAY_SIZE(SEQ_NT36675_230), 0, },
	{SEQ_NT36675_231, ARRAY_SIZE(SEQ_NT36675_231), 0, },
	{SEQ_NT36675_232, ARRAY_SIZE(SEQ_NT36675_232), 0, },
	{SEQ_NT36675_233, ARRAY_SIZE(SEQ_NT36675_233), 0, },
	{SEQ_NT36675_234, ARRAY_SIZE(SEQ_NT36675_234), 0, },
	{SEQ_NT36675_235, ARRAY_SIZE(SEQ_NT36675_235), 0, },
	{SEQ_NT36675_236, ARRAY_SIZE(SEQ_NT36675_236), 0, },
	{SEQ_NT36675_237, ARRAY_SIZE(SEQ_NT36675_237), 0, },
	{SEQ_NT36675_238, ARRAY_SIZE(SEQ_NT36675_238), 0, },
	{SEQ_NT36675_239, ARRAY_SIZE(SEQ_NT36675_239), 0, },
	{SEQ_NT36675_240, ARRAY_SIZE(SEQ_NT36675_240), 0, },
	{SEQ_NT36675_241, ARRAY_SIZE(SEQ_NT36675_241), 0, },
	{SEQ_NT36675_242, ARRAY_SIZE(SEQ_NT36675_242), 0, },
	{SEQ_NT36675_243, ARRAY_SIZE(SEQ_NT36675_243), 0, },
	{SEQ_NT36675_244, ARRAY_SIZE(SEQ_NT36675_244), 0, },
	{SEQ_NT36675_245, ARRAY_SIZE(SEQ_NT36675_245), 0, },
	{SEQ_NT36675_246, ARRAY_SIZE(SEQ_NT36675_246), 0, },
	{SEQ_NT36675_247, ARRAY_SIZE(SEQ_NT36675_247), 0, },
	{SEQ_NT36675_248, ARRAY_SIZE(SEQ_NT36675_248), 0, },
	{SEQ_NT36675_249, ARRAY_SIZE(SEQ_NT36675_249), 0, },
	{SEQ_NT36675_250, ARRAY_SIZE(SEQ_NT36675_250), 0, },
	{SEQ_NT36675_251, ARRAY_SIZE(SEQ_NT36675_251), 0, },
	{SEQ_NT36675_252, ARRAY_SIZE(SEQ_NT36675_252), 0, },
	{SEQ_NT36675_253, ARRAY_SIZE(SEQ_NT36675_253), 0, },
	{SEQ_NT36675_254, ARRAY_SIZE(SEQ_NT36675_254), 0, },
	{SEQ_NT36675_255, ARRAY_SIZE(SEQ_NT36675_255), 0, },
	{SEQ_NT36675_256, ARRAY_SIZE(SEQ_NT36675_256), 0, },
	{SEQ_NT36675_257, ARRAY_SIZE(SEQ_NT36675_257), 0, },
	{SEQ_NT36675_258, ARRAY_SIZE(SEQ_NT36675_258), 0, },
	{SEQ_NT36675_259, ARRAY_SIZE(SEQ_NT36675_259), 0, },
	{SEQ_NT36675_260, ARRAY_SIZE(SEQ_NT36675_260), 0, },
	{SEQ_NT36675_261, ARRAY_SIZE(SEQ_NT36675_261), 0, },
	{SEQ_NT36675_262, ARRAY_SIZE(SEQ_NT36675_262), 0, },
	{SEQ_NT36675_263, ARRAY_SIZE(SEQ_NT36675_263), 0, },
	{SEQ_NT36675_264, ARRAY_SIZE(SEQ_NT36675_264), 0, },
	{SEQ_NT36675_265, ARRAY_SIZE(SEQ_NT36675_265), 0, },
	{SEQ_NT36675_266, ARRAY_SIZE(SEQ_NT36675_266), 0, },
	{SEQ_NT36675_267, ARRAY_SIZE(SEQ_NT36675_267), 0, },
	{SEQ_NT36675_268, ARRAY_SIZE(SEQ_NT36675_268), 0, },
	{SEQ_NT36675_269, ARRAY_SIZE(SEQ_NT36675_269), 0, },
	{SEQ_NT36675_270, ARRAY_SIZE(SEQ_NT36675_270), 0, },
	{SEQ_NT36675_271, ARRAY_SIZE(SEQ_NT36675_271), 0, },
	{SEQ_NT36675_272, ARRAY_SIZE(SEQ_NT36675_272), 0, },
	{SEQ_NT36675_273, ARRAY_SIZE(SEQ_NT36675_273), 0, },
	{SEQ_NT36675_274, ARRAY_SIZE(SEQ_NT36675_274), 0, },
	{SEQ_NT36675_275, ARRAY_SIZE(SEQ_NT36675_275), 0, },
	{SEQ_NT36675_276, ARRAY_SIZE(SEQ_NT36675_276), 0, },
	{SEQ_NT36675_277, ARRAY_SIZE(SEQ_NT36675_277), 0, },
	{SEQ_NT36675_278, ARRAY_SIZE(SEQ_NT36675_278), 0, },
	{SEQ_NT36675_279, ARRAY_SIZE(SEQ_NT36675_279), 0, },
	{SEQ_NT36675_280, ARRAY_SIZE(SEQ_NT36675_280), 0, },
	{SEQ_NT36675_281, ARRAY_SIZE(SEQ_NT36675_281), 0, },
	{SEQ_NT36675_282, ARRAY_SIZE(SEQ_NT36675_282), 0, },
	{SEQ_NT36675_283, ARRAY_SIZE(SEQ_NT36675_283), 0, },
	{SEQ_NT36675_284, ARRAY_SIZE(SEQ_NT36675_284), 0, },
	{SEQ_NT36675_285, ARRAY_SIZE(SEQ_NT36675_285), 0, },
	{SEQ_NT36675_286, ARRAY_SIZE(SEQ_NT36675_286), 0, },
	{SEQ_NT36675_287, ARRAY_SIZE(SEQ_NT36675_287), 0, },
	{SEQ_NT36675_288, ARRAY_SIZE(SEQ_NT36675_288), 0, },
	{SEQ_NT36675_289, ARRAY_SIZE(SEQ_NT36675_289), 0, },
	{SEQ_NT36675_290, ARRAY_SIZE(SEQ_NT36675_290), 0, },
	{SEQ_NT36675_291, ARRAY_SIZE(SEQ_NT36675_291), 0, },
	{SEQ_NT36675_292, ARRAY_SIZE(SEQ_NT36675_292), 0, },
	{SEQ_NT36675_293, ARRAY_SIZE(SEQ_NT36675_293), 0, },
	{SEQ_NT36675_294, ARRAY_SIZE(SEQ_NT36675_294), 0, },
	{SEQ_NT36675_295, ARRAY_SIZE(SEQ_NT36675_295), 0, },
	{SEQ_NT36675_296, ARRAY_SIZE(SEQ_NT36675_296), 0, },
	{SEQ_NT36675_297, ARRAY_SIZE(SEQ_NT36675_297), 0, },
	{SEQ_NT36675_298, ARRAY_SIZE(SEQ_NT36675_298), 0, },
	{SEQ_NT36675_299, ARRAY_SIZE(SEQ_NT36675_299), 0, },
	{SEQ_NT36675_300, ARRAY_SIZE(SEQ_NT36675_300), 0, },
	{SEQ_NT36675_301, ARRAY_SIZE(SEQ_NT36675_301), 0, },
	{SEQ_NT36675_302, ARRAY_SIZE(SEQ_NT36675_302), 0, },
	{SEQ_NT36675_303, ARRAY_SIZE(SEQ_NT36675_303), 0, },
	{SEQ_NT36675_304, ARRAY_SIZE(SEQ_NT36675_304), 0, },
	{SEQ_NT36675_305, ARRAY_SIZE(SEQ_NT36675_305), 0, },
	{SEQ_NT36675_306, ARRAY_SIZE(SEQ_NT36675_306), 0, },
	{SEQ_NT36675_307, ARRAY_SIZE(SEQ_NT36675_307), 0, },
};

/* DO NOT REMOVE: back to page 1 for setting DCS commands */
static unsigned char SEQ_NT36675_BACK_TO_PAGE_1_A[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36675_BACK_TO_PAGE_1_B[] = {
	0xFB,
	0x01,
};

static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	1, 1, 2, 3, 3, 4, 5, 5, 6, 7, /* 1: 1 */
	7, 8, 9, 9, 10, 11, 11, 12, 13, 14,
	14, 15, 16, 16, 17, 18, 18, 19, 20, 20,
	21, 22, 22, 23, 24, 24, 25, 26, 27, 27,
	28, 29, 29, 30, 31, 31, 32, 33, 33, 34,
	35, 35, 36, 37, 37, 38, 39, 40, 40, 41,
	42, 42, 43, 44, 44, 45, 46, 46, 47, 48,
	48, 49, 50, 51, 51, 52, 53, 53, 54, 55,
	55, 56, 57, 57, 58, 59, 59, 60, 61, 61,
	62, 63, 64, 64, 65, 66, 66, 67, 68, 68,
	69, 70, 70, 71, 72, 72, 73, 74, 74, 75,
	76, 77, 77, 78, 79, 79, 80, 81, 81, 82,
	83, 83, 84, 85, 85, 86, 87, 88, 88, 89, /* 128: 88 */
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
	110, 111, 112, 113, 114, 115, 116, 117, 118, 118,
	119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
	129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
	139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149, 149, 150, 151, 152, 153, 154, 155, 156, 157,
	158, 159, 160, 161, 162, 163, 164, 165, 166, 167,
	168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
	178, 179, 180, 180, 181, 182, 183, 184, 185, 186,
	187, 188, 189, 190, 191, 192, 193, 194, 195, 196,
	197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
	207, 208, 209, 210, 211, 211, 211, 211, 211, 211, /* 255: 211 */
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 254,
};

#endif /* __NT36675_PARAM_H__ */
