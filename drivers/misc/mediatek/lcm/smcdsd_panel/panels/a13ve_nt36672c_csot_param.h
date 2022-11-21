/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NT36672C_PARAM_H__
#define __NT36672C_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define LCD_TYPE_VENDOR		"CSO"

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define LDI_REG_BRIGHTNESS	0x51
#define LDI_REG_ID		0xDA
#define LDI_LEN_BRIGHTNESS	((u16)ARRAY_SIZE(SEQ_NT36672C_BRIGHTNESS))

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

static unsigned char SEQ_NT36672C_SLEEP_OUT[] = { 0x11 };
static unsigned char SEQ_NT36672C_SLEEP_IN[] = { 0x10 };
static unsigned char SEQ_NT36672C_DISPLAY_OFF[] = { 0x28 };
static unsigned char SEQ_NT36672C_DISPLAY_ON[] = { 0x29 };

static unsigned char SEQ_NT36672C_BRIGHTNESS[] = {
	0x51,
	0x00,
};

static unsigned char SEQ_NT36672C_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};
static unsigned char SEQ_NT36672C_001[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36672C_002[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_003[] = {
	0xB0,
	0x00,
};

static unsigned char SEQ_NT36672C_004[] = {
	0xC1,
	0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x2B,
	0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7,
};

static unsigned char SEQ_NT36672C_005[] = {
	0xC2,
	0x1B, 0xA0,
};

static unsigned char SEQ_NT36672C_006[] = {
	0xFF,
	0x20,
};

static unsigned char SEQ_NT36672C_007[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_008[] = {
	0x01,
	0x66,
};

static unsigned char SEQ_NT36672C_009[] = {
	0x06,
	0x64,
};

static unsigned char SEQ_NT36672C_010[] = {
	0x07,
	0x28,
};

static unsigned char SEQ_NT36672C_011[] = {
	0x17,
	0x66,
};

static unsigned char SEQ_NT36672C_012[] = {
	0x1B,
	0x01,
};

static unsigned char SEQ_NT36672C_013[] = {
	0x1F,
	0x02,
};

static unsigned char SEQ_NT36672C_014[] = {
	0x20,
	0x03,
};

static unsigned char SEQ_NT36672C_015[] = {
	0x5C,
	0x90,
};

static unsigned char SEQ_NT36672C_016[] = {
	0x5E,
	0x7E,
};

static unsigned char SEQ_NT36672C_017[] = {
	0x69,
	0xD0,
};

static unsigned char SEQ_NT36672C_018[] = {
	0x95,
	0xD1,
};

static unsigned char SEQ_NT36672C_019[] = {
	0x96,
	0xD1,
};

static unsigned char SEQ_NT36672C_020[] = {
	0xF2,
	0x66,
};

static unsigned char SEQ_NT36672C_021[] = {
	0xF3,
	0x54,
};

static unsigned char SEQ_NT36672C_022[] = {
	0xF4,
	0x66,
};

static unsigned char SEQ_NT36672C_023[] = {
	0xF5,
	0x54,
};

static unsigned char SEQ_NT36672C_024[] = {
	0xF6,
	0x66,
};

static unsigned char SEQ_NT36672C_025[] = {
	0xF7,
	0x54,
};

static unsigned char SEQ_NT36672C_026[] = {
	0xF8,
	0x66,
};

static unsigned char SEQ_NT36672C_027[] = {
	0xF9,
	0x54,
};

static unsigned char SEQ_NT36672C_028[] = {
	0xFF,
	0x21,
};

static unsigned char SEQ_NT36672C_029[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_030[] = {
	0xFF,
	0x24,
};

static unsigned char SEQ_NT36672C_031[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_032[] = {
	0x00,
	0x26,
};

static unsigned char SEQ_NT36672C_033[] = {
	0x01,
	0x13,
};

static unsigned char SEQ_NT36672C_034[] = {
	0x02,
	0x27,
};

static unsigned char SEQ_NT36672C_035[] = {
	0x03,
	0x15,
};

static unsigned char SEQ_NT36672C_036[] = {
	0x04,
	0x28,
};

static unsigned char SEQ_NT36672C_037[] = {
	0x05,
	0x17,
};

static unsigned char SEQ_NT36672C_038[] = {
	0x07,
	0x24,
};

static unsigned char SEQ_NT36672C_039[] = {
	0x08,
	0x24,
};

static unsigned char SEQ_NT36672C_040[] = {
	0x0A,
	0x22,
};

static unsigned char SEQ_NT36672C_041[] = {
	0x0C,
	0x10,
};

static unsigned char SEQ_NT36672C_042[] = {
	0x0D,
	0x0F,
};

static unsigned char SEQ_NT36672C_043[] = {
	0x0E,
	0x01,
};

static unsigned char SEQ_NT36672C_044[] = {
	0x10,
	0x2D,
};

static unsigned char SEQ_NT36672C_045[] = {
	0x11,
	0x2F,
};

static unsigned char SEQ_NT36672C_046[] = {
	0x12,
	0x31,
};

static unsigned char SEQ_NT36672C_047[] = {
	0x13,
	0x33,
};

static unsigned char SEQ_NT36672C_048[] = {
	0x15,
	0x0B,
};

static unsigned char SEQ_NT36672C_049[] = {
	0x17,
	0x0C,
};

static unsigned char SEQ_NT36672C_050[] = {
	0x18,
	0x26,
};

static unsigned char SEQ_NT36672C_051[] = {
	0x19,
	0x13,
};

static unsigned char SEQ_NT36672C_052[] = {
	0x1A,
	0x27,
};

static unsigned char SEQ_NT36672C_053[] = {
	0x1B,
	0x15,
};

static unsigned char SEQ_NT36672C_054[] = {
	0x1C,
	0x28,
};

static unsigned char SEQ_NT36672C_055[] = {
	0x1D,
	0x17,
};

static unsigned char SEQ_NT36672C_056[] = {
	0x1F,
	0x24,
};

static unsigned char SEQ_NT36672C_057[] = {
	0x20,
	0x24,
};

static unsigned char SEQ_NT36672C_058[] = {
	0x22,
	0x22,
};

static unsigned char SEQ_NT36672C_059[] = {
	0x24,
	0x10,
};

static unsigned char SEQ_NT36672C_060[] = {
	0x25,
	0x0F,
};

static unsigned char SEQ_NT36672C_061[] = {
	0x26,
	0x01,
};

static unsigned char SEQ_NT36672C_062[] = {
	0x28,
	0x2C,
};

static unsigned char SEQ_NT36672C_063[] = {
	0x29,
	0x2E,
};

static unsigned char SEQ_NT36672C_064[] = {
	0x2A,
	0x30,
};

static unsigned char SEQ_NT36672C_065[] = {
	0x2B,
	0x32,
};

static unsigned char SEQ_NT36672C_066[] = {
	0x2F,
	0x0B,
};

static unsigned char SEQ_NT36672C_067[] = {
	0x31,
	0x0C,
};

static unsigned char SEQ_NT36672C_068[] = {
	0x32,
	0x09,
};

static unsigned char SEQ_NT36672C_069[] = {
	0x33,
	0x03,
};

static unsigned char SEQ_NT36672C_070[] = {
	0x34,
	0x03,
};

static unsigned char SEQ_NT36672C_071[] = {
	0x35,
	0x07,
};

static unsigned char SEQ_NT36672C_072[] = {
	0x36,
	0x3C,
};

static unsigned char SEQ_NT36672C_073[] = {
	0x4E,
	0x37,
};

static unsigned char SEQ_NT36672C_074[] = {
	0x4F,
	0x37,
};

static unsigned char SEQ_NT36672C_075[] = {
	0x53,
	0x37,
};

static unsigned char SEQ_NT36672C_076[] = {
	0x77,
	0x80,
};

static unsigned char SEQ_NT36672C_077[] = {
	0x79,
	0x22,
};

static unsigned char SEQ_NT36672C_078[] = {
	0x7A,
	0x03,
};

static unsigned char SEQ_NT36672C_079[] = {
	0x7B,
	0x8E,
};

static unsigned char SEQ_NT36672C_080[] = {
	0x7D,
	0x04,
};

static unsigned char SEQ_NT36672C_081[] = {
	0x80,
	0x04,
};

static unsigned char SEQ_NT36672C_082[] = {
	0x81,
	0x04,
};

static unsigned char SEQ_NT36672C_083[] = {
	0x82,
	0x13,
};

static unsigned char SEQ_NT36672C_084[] = {
	0x84,
	0x31,
};

static unsigned char SEQ_NT36672C_085[] = {
	0x85,
	0x13,
};

static unsigned char SEQ_NT36672C_086[] = {
	0x86,
	0x22,
};

static unsigned char SEQ_NT36672C_087[] = {
	0x87,
	0x31,
};

static unsigned char SEQ_NT36672C_088[] = {
	0x90,
	0x13,
};

static unsigned char SEQ_NT36672C_089[] = {
	0x92,
	0x31,
};

static unsigned char SEQ_NT36672C_090[] = {
	0x93,
	0x13,
};

static unsigned char SEQ_NT36672C_091[] = {
	0x94,
	0x22,
};

static unsigned char SEQ_NT36672C_092[] = {
	0x95,
	0x31,
};

static unsigned char SEQ_NT36672C_093[] = {
	0x9C,
	0xF4,
};

static unsigned char SEQ_NT36672C_094[] = {
	0x9D,
	0x01,
};

static unsigned char SEQ_NT36672C_095[] = {
	0xA0,
	0x0E,
};

static unsigned char SEQ_NT36672C_096[] = {
	0xA2,
	0x0E,
};

static unsigned char SEQ_NT36672C_097[] = {
	0xA3,
	0x03,
};

static unsigned char SEQ_NT36672C_098[] = {
	0xA4,
	0x04,
};

static unsigned char SEQ_NT36672C_099[] = {
	0xA5,
	0x04,
};

static unsigned char SEQ_NT36672C_100[] = {
	0xC4,
	0x80,
};

static unsigned char SEQ_NT36672C_101[] = {
	0xC6,
	0xC0,
};

static unsigned char SEQ_NT36672C_102[] = {
	0xC9,
	0x00,
};

static unsigned char SEQ_NT36672C_103[] = {
	0xD9,
	0x80,
};

static unsigned char SEQ_NT36672C_104[] = {
	0xE9,
	0x03,
};

static unsigned char SEQ_NT36672C_105[] = {
	0xFF,
	0x25,
};

static unsigned char SEQ_NT36672C_106[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_107[] = {
	0x0F,
	0x1B,
};

static unsigned char SEQ_NT36672C_108[] = {
	0x18,
	0x22,
};

static unsigned char SEQ_NT36672C_109[] = {
	0x19,
	0xE4,
};

static unsigned char SEQ_NT36672C_110[] = {
	0x20,
	0x03,
};

static unsigned char SEQ_NT36672C_111[] = {
	0x21,
	0x40,
};

static unsigned char SEQ_NT36672C_112[] = {
	0x63,
	0x8F,
};

static unsigned char SEQ_NT36672C_113[] = {
	0x66,
	0x5D,
};

static unsigned char SEQ_NT36672C_114[] = {
	0x67,
	0x16,
};

static unsigned char SEQ_NT36672C_115[] = {
	0x68,
	0x58,
};

static unsigned char SEQ_NT36672C_116[] = {
	0x69,
	0x10,
};

static unsigned char SEQ_NT36672C_117[] = {
	0x6B,
	0x00,
};

static unsigned char SEQ_NT36672C_118[] = {
	0x70,
	0xE5,
};

static unsigned char SEQ_NT36672C_119[] = {
	0x71,
	0x6D,
};

static unsigned char SEQ_NT36672C_120[] = {
	0x77,
	0x62,
};

static unsigned char SEQ_NT36672C_121[] = {
	0x7E,
	0x2D,
};

static unsigned char SEQ_NT36672C_122[] = {
	0x84,
	0x78,
};

static unsigned char SEQ_NT36672C_123[] = {
	0x85,
	0x75,
};

static unsigned char SEQ_NT36672C_124[] = {
	0x8D,
	0x04,
};

static unsigned char SEQ_NT36672C_125[] = {
	0xC1,
	0xA9,
};

static unsigned char SEQ_NT36672C_126[] = {
	0xC2,
	0x5A,
};

static unsigned char SEQ_NT36672C_127[] = {
	0xC3,
	0x07,
};

static unsigned char SEQ_NT36672C_128[] = {
	0xC4,
	0x11,
};

static unsigned char SEQ_NT36672C_129[] = {
	0xC6,
	0x11,
};

static unsigned char SEQ_NT36672C_130[] = {
	0xF0,
	0x05,
};

static unsigned char SEQ_NT36672C_131[] = {
	0xEF,
	0x28,
};

static unsigned char SEQ_NT36672C_132[] = {
	0xF1,
	0x14,
};

static unsigned char SEQ_NT36672C_133[] = {
	0xFF,
	0x26,
};

static unsigned char SEQ_NT36672C_134[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_135[] = {
	0x00,
	0x11,
};

static unsigned char SEQ_NT36672C_136[] = {
	0x01,
	0x28,
};

static unsigned char SEQ_NT36672C_137[] = {
	0x03,
	0x01,
};

static unsigned char SEQ_NT36672C_138[] = {
	0x04,
	0x28,
};

static unsigned char SEQ_NT36672C_139[] = {
	0x05,
	0x08,
};

static unsigned char SEQ_NT36672C_140[] = {
	0x06,
	0x13,
};

static unsigned char SEQ_NT36672C_141[] = {
	0x08,
	0x13,
};

static unsigned char SEQ_NT36672C_142[] = {
	0x14,
	0x06,
};

static unsigned char SEQ_NT36672C_143[] = {
	0x15,
	0x01,
};

static unsigned char SEQ_NT36672C_144[] = {
	0x74,
	0xAF,
};

static unsigned char SEQ_NT36672C_145[] = {
	0x81,
	0x0E,
};

static unsigned char SEQ_NT36672C_146[] = {
	0x83,
	0x03,
};

static unsigned char SEQ_NT36672C_147[] = {
	0x84,
	0x03,
};

static unsigned char SEQ_NT36672C_148[] = {
	0x85,
	0x01,
};

static unsigned char SEQ_NT36672C_149[] = {
	0x86,
	0x03,
};

static unsigned char SEQ_NT36672C_150[] = {
	0x87,
	0x01,
};

static unsigned char SEQ_NT36672C_151[] = {
	0x88,
	0x07,
};

static unsigned char SEQ_NT36672C_152[] = {
	0x8A,
	0x1A,
};

static unsigned char SEQ_NT36672C_153[] = {
	0x8B,
	0x11,
};

static unsigned char SEQ_NT36672C_154[] = {
	0x8C,
	0x24,
};

static unsigned char SEQ_NT36672C_155[] = {
	0x8E,
	0x42,
};

static unsigned char SEQ_NT36672C_156[] = {
	0x8F,
	0x11,
};

static unsigned char SEQ_NT36672C_157[] = {
	0x90,
	0x11,
};

static unsigned char SEQ_NT36672C_158[] = {
	0x91,
	0x11,
};

static unsigned char SEQ_NT36672C_159[] = {
	0x9A,
	0x80,
};

static unsigned char SEQ_NT36672C_160[] = {
	0x9B,
	0x04,
};

static unsigned char SEQ_NT36672C_161[] = {
	0x9C,
	0x00,
};

static unsigned char SEQ_NT36672C_162[] = {
	0x9D,
	0x00,
};

static unsigned char SEQ_NT36672C_163[] = {
	0x9E,
	0x00,
};

static unsigned char SEQ_NT36672C_164[] = {
	0xFF,
	0x27,
};

static unsigned char SEQ_NT36672C_165[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_166[] = {
	0x01,
	0x68,
};

static unsigned char SEQ_NT36672C_167[] = {
	0x20,
	0x81,
};

static unsigned char SEQ_NT36672C_168[] = {
	0x21,
	0x76,
};

static unsigned char SEQ_NT36672C_169[] = {
	0x25,
	0x81,
};

static unsigned char SEQ_NT36672C_170[] = {
	0x26,
	0x9F,
};

static unsigned char SEQ_NT36672C_171[] = {
	0x6E,
	0x12,
};

static unsigned char SEQ_NT36672C_172[] = {
	0x6F,
	0x00,
};

static unsigned char SEQ_NT36672C_173[] = {
	0x70,
	0x00,
};

static unsigned char SEQ_NT36672C_174[] = {
	0x71,
	0x00,
};

static unsigned char SEQ_NT36672C_175[] = {
	0x72,
	0x00,
};

static unsigned char SEQ_NT36672C_176[] = {
	0x73,
	0x76,
};

static unsigned char SEQ_NT36672C_177[] = {
	0x74,
	0x10,
};

static unsigned char SEQ_NT36672C_178[] = {
	0x75,
	0x32,
};

static unsigned char SEQ_NT36672C_179[] = {
	0x76,
	0x54,
};

static unsigned char SEQ_NT36672C_180[] = {
	0x77,
	0x00,
};

static unsigned char SEQ_NT36672C_181[] = {
	0x7D,
	0x09,
};

static unsigned char SEQ_NT36672C_182[] = {
	0x7E,
	0x6B,
};

static unsigned char SEQ_NT36672C_183[] = {
	0x80,
	0x27,
};

static unsigned char SEQ_NT36672C_184[] = {
	0x82,
	0x09,
};

static unsigned char SEQ_NT36672C_185[] = {
	0x83,
	0x6B,
};

static unsigned char SEQ_NT36672C_186[] = {
	0x88,
	0x03,
};

static unsigned char SEQ_NT36672C_187[] = {
	0x89,
	0x03,
};

static unsigned char SEQ_NT36672C_188[] = {
	0xE3,
	0x01,
};

static unsigned char SEQ_NT36672C_189[] = {
	0xE4,
	0xF3,
};

static unsigned char SEQ_NT36672C_190[] = {
	0xE5,
	0x02,
};

static unsigned char SEQ_NT36672C_191[] = {
	0xE6,
	0xED,
};

static unsigned char SEQ_NT36672C_192[] = {
	0xE9,
	0x02,
};

static unsigned char SEQ_NT36672C_193[] = {
	0xEA,
	0x29,
};

static unsigned char SEQ_NT36672C_194[] = {
	0xEB,
	0x03,
};

static unsigned char SEQ_NT36672C_195[] = {
	0xEC,
	0x3E,
};

static unsigned char SEQ_NT36672C_196[] = {
	0xFF,
	0x2A,
};

static unsigned char SEQ_NT36672C_197[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_198[] = {
	0x00,
	0x91,
};

static unsigned char SEQ_NT36672C_199[] = {
	0x03,
	0x20,
};

static unsigned char SEQ_NT36672C_200[] = {
	0x06,
	0x06,
};

static unsigned char SEQ_NT36672C_201[] = {
	0x07,
	0x50,
};

static unsigned char SEQ_NT36672C_202[] = {
	0x0A,
	0x60,
};

static unsigned char SEQ_NT36672C_203[] = {
	0x0C,
	0x04,
};

static unsigned char SEQ_NT36672C_204[] = {
	0x0D,
	0x40,
};

static unsigned char SEQ_NT36672C_205[] = {
	0x0F,
	0x01,
};

static unsigned char SEQ_NT36672C_206[] = {
	0x11,
	0xE1,
};

static unsigned char SEQ_NT36672C_207[] = {
	0x15,
	0x12,
};

static unsigned char SEQ_NT36672C_208[] = {
	0x16,
	0x8D,
};

static unsigned char SEQ_NT36672C_209[] = {
	0x19,
	0x12,
};

static unsigned char SEQ_NT36672C_210[] = {
	0x1A,
	0x61,
};

static unsigned char SEQ_NT36672C_211[] = {
	0x1B,
	0x0C,
};

static unsigned char SEQ_NT36672C_212[] = {
	0x1D,
	0x0A,
};

static unsigned char SEQ_NT36672C_213[] = {
	0x1E,
	0x3F,
};

static unsigned char SEQ_NT36672C_214[] = {
	0x1F,
	0x48,
};

static unsigned char SEQ_NT36672C_215[] = {
	0x20,
	0x3F,
};

static unsigned char SEQ_NT36672C_216[] = {
	0x28,
	0xFD,
};

static unsigned char SEQ_NT36672C_217[] = {
	0x29,
	0x0B,
};

static unsigned char SEQ_NT36672C_218[] = {
	0x2A,
	0x1B,
};

static unsigned char SEQ_NT36672C_219[] = {
	0x2D,
	0x0B,
};

static unsigned char SEQ_NT36672C_220[] = {
	0x2F,
	0x01,
};

static unsigned char SEQ_NT36672C_221[] = {
	0x30,
	0x85,
};

static unsigned char SEQ_NT36672C_222[] = {
	0x31,
	0xB4,
};

static unsigned char SEQ_NT36672C_223[] = {
	0x33,
	0x22,
};

static unsigned char SEQ_NT36672C_224[] = {
	0x34,
	0xFF,
};

static unsigned char SEQ_NT36672C_225[] = {
	0x35,
	0x3F,
};

static unsigned char SEQ_NT36672C_226[] = {
	0x36,
	0x05,
};

static unsigned char SEQ_NT36672C_227[] = {
	0x37,
	0xF9,
};

static unsigned char SEQ_NT36672C_228[] = {
	0x38,
	0x44,
};

static unsigned char SEQ_NT36672C_229[] = {
	0x39,
	0x00,
};

static unsigned char SEQ_NT36672C_230[] = {
	0x3A,
	0x85,
};

static unsigned char SEQ_NT36672C_231[] = {
	0x45,
	0x04,
};

static unsigned char SEQ_NT36672C_232[] = {
	0x46,
	0x40,
};

static unsigned char SEQ_NT36672C_233[] = {
	0x48,
	0x01,
};

static unsigned char SEQ_NT36672C_234[] = {
	0x4A,
	0xE1,
};

static unsigned char SEQ_NT36672C_235[] = {
	0x4E,
	0x12,
};

static unsigned char SEQ_NT36672C_236[] = {
	0x4F,
	0x8D,
};

static unsigned char SEQ_NT36672C_237[] = {
	0x52,
	0x12,
};

static unsigned char SEQ_NT36672C_238[] = {
	0x53,
	0x61,
};

static unsigned char SEQ_NT36672C_239[] = {
	0x54,
	0x0C,
};

static unsigned char SEQ_NT36672C_240[] = {
	0x56,
	0x0A,
};

static unsigned char SEQ_NT36672C_241[] = {
	0x57,
	0x57,
};

static unsigned char SEQ_NT36672C_242[] = {
	0x58,
	0x61,
};

static unsigned char SEQ_NT36672C_243[] = {
	0x59,
	0x57,
};

static unsigned char SEQ_NT36672C_244[] = {
	0x7A,
	0x09,
};

static unsigned char SEQ_NT36672C_245[] = {
	0x7B,
	0x40,
};

static unsigned char SEQ_NT36672C_246[] = {
	0x7F,
	0xF0,
};

static unsigned char SEQ_NT36672C_247[] = {
	0x83,
	0x12,
};

static unsigned char SEQ_NT36672C_248[] = {
	0x84,
	0xBD,
};

static unsigned char SEQ_NT36672C_249[] = {
	0x87,
	0x12,
};

static unsigned char SEQ_NT36672C_250[] = {
	0x88,
	0x61,
};

static unsigned char SEQ_NT36672C_251[] = {
	0x89,
	0x0C,
};

static unsigned char SEQ_NT36672C_252[] = {
	0x8B,
	0x0A,
};

static unsigned char SEQ_NT36672C_253[] = {
	0x8C,
	0x7E,
};

static unsigned char SEQ_NT36672C_254[] = {
	0x8D,
	0x7E,
};

static unsigned char SEQ_NT36672C_255[] = {
	0x8E,
	0x7E,
};

static unsigned char SEQ_NT36672C_256[] = {
	0xFF,
	0x2C,
};

static unsigned char SEQ_NT36672C_257[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_258[] = {
	0x03,
	0x15,
};

static unsigned char SEQ_NT36672C_259[] = {
	0x04,
	0x15,
};

static unsigned char SEQ_NT36672C_260[] = {
	0x05,
	0x15,
};

static unsigned char SEQ_NT36672C_261[] = {
	0x0D,
	0x06,
};

static unsigned char SEQ_NT36672C_262[] = {
	0x0E,
	0x56,
};

static unsigned char SEQ_NT36672C_263[] = {
	0x17,
	0x4E,
};

static unsigned char SEQ_NT36672C_264[] = {
	0x18,
	0x4E,
};

static unsigned char SEQ_NT36672C_265[] = {
	0x19,
	0x4E,
};

static unsigned char SEQ_NT36672C_266[] = {
	0x2D,
	0xAF,
};

static unsigned char SEQ_NT36672C_267[] = {
	0x2F,
	0x11,
};

static unsigned char SEQ_NT36672C_268[] = {
	0x30,
	0x28,
};

static unsigned char SEQ_NT36672C_269[] = {
	0x32,
	0x01,
};

static unsigned char SEQ_NT36672C_270[] = {
	0x33,
	0x28,
};

static unsigned char SEQ_NT36672C_271[] = {
	0x35,
	0x19,
};

static unsigned char SEQ_NT36672C_272[] = {
	0x37,
	0x19,
};

static unsigned char SEQ_NT36672C_273[] = {
	0x4D,
	0x15,
};

static unsigned char SEQ_NT36672C_274[] = {
	0x4E,
	0x04,
};

static unsigned char SEQ_NT36672C_275[] = {
	0x4F,
	0x09,
};

static unsigned char SEQ_NT36672C_276[] = {
	0x56,
	0x1B,
};

static unsigned char SEQ_NT36672C_277[] = {
	0x58,
	0x1B,
};

static unsigned char SEQ_NT36672C_278[] = {
	0x59,
	0x1B,
};

static unsigned char SEQ_NT36672C_279[] = {
	0x62,
	0x6D,
};

static unsigned char SEQ_NT36672C_280[] = {
	0x6B,
	0x6A,
};

static unsigned char SEQ_NT36672C_281[] = {
	0x6C,
	0x6A,
};

static unsigned char SEQ_NT36672C_282[] = {
	0x6D,
	0x6A,
};

static unsigned char SEQ_NT36672C_283[] = {
	0x80,
	0xAF,
};

static unsigned char SEQ_NT36672C_284[] = {
	0x81,
	0x11,
};

static unsigned char SEQ_NT36672C_285[] = {
	0x82,
	0x29,
};

static unsigned char SEQ_NT36672C_286[] = {
	0x84,
	0x01,
};

static unsigned char SEQ_NT36672C_287[] = {
	0x85,
	0x29,
};

static unsigned char SEQ_NT36672C_288[] = {
	0x87,
	0x20,
};

static unsigned char SEQ_NT36672C_289[] = {
	0x89,
	0x20,
};

static unsigned char SEQ_NT36672C_290[] = {
	0x9E,
	0x04,
};

static unsigned char SEQ_NT36672C_291[] = {
	0x9F,
	0x1E,
};

static unsigned char SEQ_NT36672C_292[] = {
	0xFF,
	0xE0,
};

static unsigned char SEQ_NT36672C_293[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_294[] = {
	0x35,
	0x82,
};

static unsigned char SEQ_NT36672C_295[] = {
	0xFF,
	0xF0,
};

static unsigned char SEQ_NT36672C_296[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_297[] = {
	0x1C,
	0x01,
};

static unsigned char SEQ_NT36672C_298[] = {
	0x33,
	0x01,
};

static unsigned char SEQ_NT36672C_299[] = {
	0x5A,
	0x00,
};

static unsigned char SEQ_NT36672C_300[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36672C_301[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_302[] = {
	0x53,
	0x22,
};

static unsigned char SEQ_NT36672C_303[] = {
	0x54,
	0x02,
};

static unsigned char SEQ_NT36672C_304[] = {
	0xFF,
	0xC0,
};

static unsigned char SEQ_NT36672C_305[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_306[] = {
	0x9C,
	0x11,
};

static unsigned char SEQ_NT36672C_307[] = {
	0x9D,
	0x11,
};

static unsigned char SEQ_NT36672C_308[] = {
	0xFF,
	0x2B,
};

static unsigned char SEQ_NT36672C_309[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_310[] = {
	0xB7,
	0x1A,
};

static unsigned char SEQ_NT36672C_311[] = {
	0xB8,
	0x15,
};

static unsigned char SEQ_NT36672C_312[] = {
	0xC0,
	0x03,
};

static unsigned char SEQ_NT36672C_313[] = {
	0xFF,
	0xF0,
};

static unsigned char SEQ_NT36672C_314[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_315[] = {
	0xD2,
	0x50,
};

static unsigned char SEQ_NT36672C_316[] = {
	0xFF,
	0x23,
};

static unsigned char SEQ_NT36672C_317[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_318[] = {
	0x00,
	0x00,
};

static unsigned char SEQ_NT36672C_319[] = {
	0x07,
	0x60,
};

static unsigned char SEQ_NT36672C_320[] = {
	0x08,
	0x06,
};

static unsigned char SEQ_NT36672C_321[] = {
	0x09,
	0x1C,
};

static unsigned char SEQ_NT36672C_322[] = {
	0x0A,
	0x2B,
};

static unsigned char SEQ_NT36672C_323[] = {
	0x0B,
	0x2B,
};

static unsigned char SEQ_NT36672C_324[] = {
	0x0C,
	0x2B,
};

static unsigned char SEQ_NT36672C_325[] = {
	0x0D,
	0x00,
};

static unsigned char SEQ_NT36672C_326[] = {
	0x10,
	0x50,
};

static unsigned char SEQ_NT36672C_327[] = {
	0x11,
	0x01,
};

static unsigned char SEQ_NT36672C_328[] = {
	0x12,
	0x95,
};

static unsigned char SEQ_NT36672C_329[] = {
	0x15,
	0x68,
};

static unsigned char SEQ_NT36672C_330[] = {
	0x16,
	0x0B,
};

static unsigned char SEQ_NT36672C_331[] = {
	0x19,
	0x00,
};

static unsigned char SEQ_NT36672C_332[] = {
	0x1A,
	0x00,
};

static unsigned char SEQ_NT36672C_333[] = {
	0x1B,
	0x00,
};

static unsigned char SEQ_NT36672C_334[] = {
	0x1C,
	0x00,
};

static unsigned char SEQ_NT36672C_335[] = {
	0x1D,
	0x01,
};

static unsigned char SEQ_NT36672C_336[] = {
	0x1E,
	0x03,
};

static unsigned char SEQ_NT36672C_337[] = {
	0x1F,
	0x05,
};

static unsigned char SEQ_NT36672C_338[] = {
	0x20,
	0x0C,
};

static unsigned char SEQ_NT36672C_339[] = {
	0x21,
	0x13,
};

static unsigned char SEQ_NT36672C_340[] = {
	0x22,
	0x17,
};

static unsigned char SEQ_NT36672C_341[] = {
	0x23,
	0x1D,
};

static unsigned char SEQ_NT36672C_342[] = {
	0x24,
	0x23,
};

static unsigned char SEQ_NT36672C_343[] = {
	0x25,
	0x2C,
};

static unsigned char SEQ_NT36672C_344[] = {
	0x26,
	0x33,
};

static unsigned char SEQ_NT36672C_345[] = {
	0x27,
	0x39,
};

static unsigned char SEQ_NT36672C_346[] = {
	0x28,
	0x3F,
};

static unsigned char SEQ_NT36672C_347[] = {
	0x29,
	0x3F,
};

static unsigned char SEQ_NT36672C_348[] = {
	0x2A,
	0x3F,
};

static unsigned char SEQ_NT36672C_349[] = {
	0x2B,
	0x3F,
};

static unsigned char SEQ_NT36672C_350[] = {
	0x30,
	0xFF,
};

static unsigned char SEQ_NT36672C_351[] = {
	0x31,
	0xFE,
};

static unsigned char SEQ_NT36672C_352[] = {
	0x32,
	0xFD,
};

static unsigned char SEQ_NT36672C_353[] = {
	0x33,
	0xFC,
};

static unsigned char SEQ_NT36672C_354[] = {
	0x34,
	0xFB,
};

static unsigned char SEQ_NT36672C_355[] = {
	0x35,
	0xFA,
};

static unsigned char SEQ_NT36672C_356[] = {
	0x36,
	0xF9,
};

static unsigned char SEQ_NT36672C_357[] = {
	0x37,
	0xF7,
};

static unsigned char SEQ_NT36672C_358[] = {
	0x38,
	0xF5,
};

static unsigned char SEQ_NT36672C_359[] = {
	0x39,
	0xF3,
};

static unsigned char SEQ_NT36672C_360[] = {
	0x3A,
	0xF1,
};

static unsigned char SEQ_NT36672C_361[] = {
	0x3B,
	0xEE,
};

static unsigned char SEQ_NT36672C_362[] = {
	0x3D,
	0xEC,
};

static unsigned char SEQ_NT36672C_363[] = {
	0x3F,
	0xEA,
};

static unsigned char SEQ_NT36672C_364[] = {
	0x40,
	0xE8,
};

static unsigned char SEQ_NT36672C_365[] = {
	0x41,
	0xE6,
};

static unsigned char SEQ_NT36672C_366[] = {
	0x04,
	0x00,
};

static unsigned char SEQ_NT36672C_367[] = {
	0xA0,
	0x11,
};

static unsigned char SEQ_NT36672C_368[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36672C_369[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_370[] = {
	0x25,
	0xA9,
};

static unsigned char SEQ_NT36672C_371[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36672C_372[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36672C_373[] = {
	0x53,
	0x2C,
};

static unsigned char SEQ_NT36672C_374[] = {
	0x55,
	0x01,
};

static unsigned char SEQ_NT36672C_375[] = {
	0x68,
	0x00, 0x01,
};

static struct lcd_seq_info LCD_SEQ_INIT_1[] = {
	{SEQ_NT36672C_001, ARRAY_SIZE(SEQ_NT36672C_001), 0, },
	{SEQ_NT36672C_002, ARRAY_SIZE(SEQ_NT36672C_002), 0, },
	{SEQ_NT36672C_003, ARRAY_SIZE(SEQ_NT36672C_003), 0, },
	{SEQ_NT36672C_004, ARRAY_SIZE(SEQ_NT36672C_004), 0, },
	{SEQ_NT36672C_005, ARRAY_SIZE(SEQ_NT36672C_005), 0, },
	{SEQ_NT36672C_006, ARRAY_SIZE(SEQ_NT36672C_006), 0, },
	{SEQ_NT36672C_007, ARRAY_SIZE(SEQ_NT36672C_007), 0, },
	{SEQ_NT36672C_008, ARRAY_SIZE(SEQ_NT36672C_008), 0, },
	{SEQ_NT36672C_009, ARRAY_SIZE(SEQ_NT36672C_009), 0, },
	{SEQ_NT36672C_010, ARRAY_SIZE(SEQ_NT36672C_010), 0, },
	{SEQ_NT36672C_011, ARRAY_SIZE(SEQ_NT36672C_011), 0, },
	{SEQ_NT36672C_012, ARRAY_SIZE(SEQ_NT36672C_012), 0, },
	{SEQ_NT36672C_013, ARRAY_SIZE(SEQ_NT36672C_013), 0, },
	{SEQ_NT36672C_014, ARRAY_SIZE(SEQ_NT36672C_014), 0, },
	{SEQ_NT36672C_015, ARRAY_SIZE(SEQ_NT36672C_015), 0, },
	{SEQ_NT36672C_016, ARRAY_SIZE(SEQ_NT36672C_016), 0, },
	{SEQ_NT36672C_017, ARRAY_SIZE(SEQ_NT36672C_017), 0, },
	{SEQ_NT36672C_018, ARRAY_SIZE(SEQ_NT36672C_018), 0, },
	{SEQ_NT36672C_019, ARRAY_SIZE(SEQ_NT36672C_019), 0, },
	{SEQ_NT36672C_020, ARRAY_SIZE(SEQ_NT36672C_020), 0, },
	{SEQ_NT36672C_021, ARRAY_SIZE(SEQ_NT36672C_021), 0, },
	{SEQ_NT36672C_022, ARRAY_SIZE(SEQ_NT36672C_022), 0, },
	{SEQ_NT36672C_023, ARRAY_SIZE(SEQ_NT36672C_023), 0, },
	{SEQ_NT36672C_024, ARRAY_SIZE(SEQ_NT36672C_024), 0, },
	{SEQ_NT36672C_025, ARRAY_SIZE(SEQ_NT36672C_025), 0, },
	{SEQ_NT36672C_026, ARRAY_SIZE(SEQ_NT36672C_026), 0, },
	{SEQ_NT36672C_027, ARRAY_SIZE(SEQ_NT36672C_027), 0, },
	{SEQ_NT36672C_028, ARRAY_SIZE(SEQ_NT36672C_028), 0, },
	{SEQ_NT36672C_029, ARRAY_SIZE(SEQ_NT36672C_029), 0, },
	{SEQ_NT36672C_030, ARRAY_SIZE(SEQ_NT36672C_030), 0, },
	{SEQ_NT36672C_031, ARRAY_SIZE(SEQ_NT36672C_031), 0, },
	{SEQ_NT36672C_032, ARRAY_SIZE(SEQ_NT36672C_032), 0, },
	{SEQ_NT36672C_033, ARRAY_SIZE(SEQ_NT36672C_033), 0, },
	{SEQ_NT36672C_034, ARRAY_SIZE(SEQ_NT36672C_034), 0, },
	{SEQ_NT36672C_035, ARRAY_SIZE(SEQ_NT36672C_035), 0, },
	{SEQ_NT36672C_036, ARRAY_SIZE(SEQ_NT36672C_036), 0, },
	{SEQ_NT36672C_037, ARRAY_SIZE(SEQ_NT36672C_037), 0, },
	{SEQ_NT36672C_038, ARRAY_SIZE(SEQ_NT36672C_038), 0, },
	{SEQ_NT36672C_039, ARRAY_SIZE(SEQ_NT36672C_039), 0, },
	{SEQ_NT36672C_040, ARRAY_SIZE(SEQ_NT36672C_040), 0, },
	{SEQ_NT36672C_041, ARRAY_SIZE(SEQ_NT36672C_041), 0, },
	{SEQ_NT36672C_042, ARRAY_SIZE(SEQ_NT36672C_042), 0, },
	{SEQ_NT36672C_043, ARRAY_SIZE(SEQ_NT36672C_043), 0, },
	{SEQ_NT36672C_044, ARRAY_SIZE(SEQ_NT36672C_044), 0, },
	{SEQ_NT36672C_045, ARRAY_SIZE(SEQ_NT36672C_045), 0, },
	{SEQ_NT36672C_046, ARRAY_SIZE(SEQ_NT36672C_046), 0, },
	{SEQ_NT36672C_047, ARRAY_SIZE(SEQ_NT36672C_047), 0, },
	{SEQ_NT36672C_048, ARRAY_SIZE(SEQ_NT36672C_048), 0, },
	{SEQ_NT36672C_049, ARRAY_SIZE(SEQ_NT36672C_049), 0, },
	{SEQ_NT36672C_050, ARRAY_SIZE(SEQ_NT36672C_050), 0, },
	{SEQ_NT36672C_051, ARRAY_SIZE(SEQ_NT36672C_051), 0, },
	{SEQ_NT36672C_052, ARRAY_SIZE(SEQ_NT36672C_052), 0, },
	{SEQ_NT36672C_053, ARRAY_SIZE(SEQ_NT36672C_053), 0, },
	{SEQ_NT36672C_054, ARRAY_SIZE(SEQ_NT36672C_054), 0, },
	{SEQ_NT36672C_055, ARRAY_SIZE(SEQ_NT36672C_055), 0, },
	{SEQ_NT36672C_056, ARRAY_SIZE(SEQ_NT36672C_056), 0, },
	{SEQ_NT36672C_057, ARRAY_SIZE(SEQ_NT36672C_057), 0, },
	{SEQ_NT36672C_058, ARRAY_SIZE(SEQ_NT36672C_058), 0, },
	{SEQ_NT36672C_059, ARRAY_SIZE(SEQ_NT36672C_059), 0, },
	{SEQ_NT36672C_060, ARRAY_SIZE(SEQ_NT36672C_060), 0, },
	{SEQ_NT36672C_061, ARRAY_SIZE(SEQ_NT36672C_061), 0, },
	{SEQ_NT36672C_062, ARRAY_SIZE(SEQ_NT36672C_062), 0, },
	{SEQ_NT36672C_063, ARRAY_SIZE(SEQ_NT36672C_063), 0, },
	{SEQ_NT36672C_064, ARRAY_SIZE(SEQ_NT36672C_064), 0, },
	{SEQ_NT36672C_065, ARRAY_SIZE(SEQ_NT36672C_065), 0, },
	{SEQ_NT36672C_066, ARRAY_SIZE(SEQ_NT36672C_066), 0, },
	{SEQ_NT36672C_067, ARRAY_SIZE(SEQ_NT36672C_067), 0, },
	{SEQ_NT36672C_068, ARRAY_SIZE(SEQ_NT36672C_068), 0, },
	{SEQ_NT36672C_069, ARRAY_SIZE(SEQ_NT36672C_069), 0, },
	{SEQ_NT36672C_070, ARRAY_SIZE(SEQ_NT36672C_070), 0, },
	{SEQ_NT36672C_071, ARRAY_SIZE(SEQ_NT36672C_071), 0, },
	{SEQ_NT36672C_072, ARRAY_SIZE(SEQ_NT36672C_072), 0, },
	{SEQ_NT36672C_073, ARRAY_SIZE(SEQ_NT36672C_073), 0, },
	{SEQ_NT36672C_074, ARRAY_SIZE(SEQ_NT36672C_074), 0, },
	{SEQ_NT36672C_075, ARRAY_SIZE(SEQ_NT36672C_075), 0, },
	{SEQ_NT36672C_076, ARRAY_SIZE(SEQ_NT36672C_076), 0, },
	{SEQ_NT36672C_077, ARRAY_SIZE(SEQ_NT36672C_077), 0, },
	{SEQ_NT36672C_078, ARRAY_SIZE(SEQ_NT36672C_078), 0, },
	{SEQ_NT36672C_079, ARRAY_SIZE(SEQ_NT36672C_079), 0, },
	{SEQ_NT36672C_080, ARRAY_SIZE(SEQ_NT36672C_080), 0, },
	{SEQ_NT36672C_081, ARRAY_SIZE(SEQ_NT36672C_081), 0, },
	{SEQ_NT36672C_082, ARRAY_SIZE(SEQ_NT36672C_082), 0, },
	{SEQ_NT36672C_083, ARRAY_SIZE(SEQ_NT36672C_083), 0, },
	{SEQ_NT36672C_084, ARRAY_SIZE(SEQ_NT36672C_084), 0, },
	{SEQ_NT36672C_085, ARRAY_SIZE(SEQ_NT36672C_085), 0, },
	{SEQ_NT36672C_086, ARRAY_SIZE(SEQ_NT36672C_086), 0, },
	{SEQ_NT36672C_087, ARRAY_SIZE(SEQ_NT36672C_087), 0, },
	{SEQ_NT36672C_088, ARRAY_SIZE(SEQ_NT36672C_088), 0, },
	{SEQ_NT36672C_089, ARRAY_SIZE(SEQ_NT36672C_089), 0, },
	{SEQ_NT36672C_090, ARRAY_SIZE(SEQ_NT36672C_090), 0, },
	{SEQ_NT36672C_091, ARRAY_SIZE(SEQ_NT36672C_091), 0, },
	{SEQ_NT36672C_092, ARRAY_SIZE(SEQ_NT36672C_092), 0, },
	{SEQ_NT36672C_093, ARRAY_SIZE(SEQ_NT36672C_093), 0, },
	{SEQ_NT36672C_094, ARRAY_SIZE(SEQ_NT36672C_094), 0, },
	{SEQ_NT36672C_095, ARRAY_SIZE(SEQ_NT36672C_095), 0, },
	{SEQ_NT36672C_096, ARRAY_SIZE(SEQ_NT36672C_096), 0, },
	{SEQ_NT36672C_097, ARRAY_SIZE(SEQ_NT36672C_097), 0, },
	{SEQ_NT36672C_098, ARRAY_SIZE(SEQ_NT36672C_098), 0, },
	{SEQ_NT36672C_099, ARRAY_SIZE(SEQ_NT36672C_099), 0, },
	{SEQ_NT36672C_100, ARRAY_SIZE(SEQ_NT36672C_100), 0, },
	{SEQ_NT36672C_101, ARRAY_SIZE(SEQ_NT36672C_101), 0, },
	{SEQ_NT36672C_102, ARRAY_SIZE(SEQ_NT36672C_102), 0, },
	{SEQ_NT36672C_103, ARRAY_SIZE(SEQ_NT36672C_103), 0, },
	{SEQ_NT36672C_104, ARRAY_SIZE(SEQ_NT36672C_104), 0, },
	{SEQ_NT36672C_105, ARRAY_SIZE(SEQ_NT36672C_105), 0, },
	{SEQ_NT36672C_106, ARRAY_SIZE(SEQ_NT36672C_106), 0, },
	{SEQ_NT36672C_107, ARRAY_SIZE(SEQ_NT36672C_107), 0, },
	{SEQ_NT36672C_108, ARRAY_SIZE(SEQ_NT36672C_108), 0, },
	{SEQ_NT36672C_109, ARRAY_SIZE(SEQ_NT36672C_109), 0, },
	{SEQ_NT36672C_110, ARRAY_SIZE(SEQ_NT36672C_110), 0, },
	{SEQ_NT36672C_111, ARRAY_SIZE(SEQ_NT36672C_111), 0, },
	{SEQ_NT36672C_112, ARRAY_SIZE(SEQ_NT36672C_112), 0, },
	{SEQ_NT36672C_113, ARRAY_SIZE(SEQ_NT36672C_113), 0, },
	{SEQ_NT36672C_114, ARRAY_SIZE(SEQ_NT36672C_114), 0, },
	{SEQ_NT36672C_115, ARRAY_SIZE(SEQ_NT36672C_115), 0, },
	{SEQ_NT36672C_116, ARRAY_SIZE(SEQ_NT36672C_116), 0, },
	{SEQ_NT36672C_117, ARRAY_SIZE(SEQ_NT36672C_117), 0, },
	{SEQ_NT36672C_118, ARRAY_SIZE(SEQ_NT36672C_118), 0, },
	{SEQ_NT36672C_119, ARRAY_SIZE(SEQ_NT36672C_119), 0, },
	{SEQ_NT36672C_120, ARRAY_SIZE(SEQ_NT36672C_120), 0, },
	{SEQ_NT36672C_121, ARRAY_SIZE(SEQ_NT36672C_121), 0, },
	{SEQ_NT36672C_122, ARRAY_SIZE(SEQ_NT36672C_122), 0, },
	{SEQ_NT36672C_123, ARRAY_SIZE(SEQ_NT36672C_123), 0, },
	{SEQ_NT36672C_124, ARRAY_SIZE(SEQ_NT36672C_124), 0, },
	{SEQ_NT36672C_125, ARRAY_SIZE(SEQ_NT36672C_125), 0, },
	{SEQ_NT36672C_126, ARRAY_SIZE(SEQ_NT36672C_126), 0, },
	{SEQ_NT36672C_127, ARRAY_SIZE(SEQ_NT36672C_127), 0, },
	{SEQ_NT36672C_128, ARRAY_SIZE(SEQ_NT36672C_128), 0, },
	{SEQ_NT36672C_129, ARRAY_SIZE(SEQ_NT36672C_129), 0, },
	{SEQ_NT36672C_130, ARRAY_SIZE(SEQ_NT36672C_130), 0, },
	{SEQ_NT36672C_131, ARRAY_SIZE(SEQ_NT36672C_131), 0, },
	{SEQ_NT36672C_132, ARRAY_SIZE(SEQ_NT36672C_132), 0, },
	{SEQ_NT36672C_133, ARRAY_SIZE(SEQ_NT36672C_133), 0, },
	{SEQ_NT36672C_134, ARRAY_SIZE(SEQ_NT36672C_134), 0, },
	{SEQ_NT36672C_135, ARRAY_SIZE(SEQ_NT36672C_135), 0, },
	{SEQ_NT36672C_136, ARRAY_SIZE(SEQ_NT36672C_136), 0, },
	{SEQ_NT36672C_137, ARRAY_SIZE(SEQ_NT36672C_137), 0, },
	{SEQ_NT36672C_138, ARRAY_SIZE(SEQ_NT36672C_138), 0, },
	{SEQ_NT36672C_139, ARRAY_SIZE(SEQ_NT36672C_139), 0, },
	{SEQ_NT36672C_140, ARRAY_SIZE(SEQ_NT36672C_140), 0, },
	{SEQ_NT36672C_141, ARRAY_SIZE(SEQ_NT36672C_141), 0, },
	{SEQ_NT36672C_142, ARRAY_SIZE(SEQ_NT36672C_142), 0, },
	{SEQ_NT36672C_143, ARRAY_SIZE(SEQ_NT36672C_143), 0, },
	{SEQ_NT36672C_144, ARRAY_SIZE(SEQ_NT36672C_144), 0, },
	{SEQ_NT36672C_145, ARRAY_SIZE(SEQ_NT36672C_145), 0, },
	{SEQ_NT36672C_146, ARRAY_SIZE(SEQ_NT36672C_146), 0, },
	{SEQ_NT36672C_147, ARRAY_SIZE(SEQ_NT36672C_147), 0, },
	{SEQ_NT36672C_148, ARRAY_SIZE(SEQ_NT36672C_148), 0, },
	{SEQ_NT36672C_149, ARRAY_SIZE(SEQ_NT36672C_149), 0, },
	{SEQ_NT36672C_150, ARRAY_SIZE(SEQ_NT36672C_150), 0, },
	{SEQ_NT36672C_151, ARRAY_SIZE(SEQ_NT36672C_151), 0, },
	{SEQ_NT36672C_152, ARRAY_SIZE(SEQ_NT36672C_152), 0, },
	{SEQ_NT36672C_153, ARRAY_SIZE(SEQ_NT36672C_153), 0, },
	{SEQ_NT36672C_154, ARRAY_SIZE(SEQ_NT36672C_154), 0, },
	{SEQ_NT36672C_155, ARRAY_SIZE(SEQ_NT36672C_155), 0, },
	{SEQ_NT36672C_156, ARRAY_SIZE(SEQ_NT36672C_156), 0, },
	{SEQ_NT36672C_157, ARRAY_SIZE(SEQ_NT36672C_157), 0, },
	{SEQ_NT36672C_158, ARRAY_SIZE(SEQ_NT36672C_158), 0, },
	{SEQ_NT36672C_159, ARRAY_SIZE(SEQ_NT36672C_159), 0, },
	{SEQ_NT36672C_160, ARRAY_SIZE(SEQ_NT36672C_160), 0, },
	{SEQ_NT36672C_161, ARRAY_SIZE(SEQ_NT36672C_161), 0, },
	{SEQ_NT36672C_162, ARRAY_SIZE(SEQ_NT36672C_162), 0, },
	{SEQ_NT36672C_163, ARRAY_SIZE(SEQ_NT36672C_163), 0, },
	{SEQ_NT36672C_164, ARRAY_SIZE(SEQ_NT36672C_164), 0, },
	{SEQ_NT36672C_165, ARRAY_SIZE(SEQ_NT36672C_165), 0, },
	{SEQ_NT36672C_166, ARRAY_SIZE(SEQ_NT36672C_166), 0, },
	{SEQ_NT36672C_167, ARRAY_SIZE(SEQ_NT36672C_167), 0, },
	{SEQ_NT36672C_168, ARRAY_SIZE(SEQ_NT36672C_168), 0, },
	{SEQ_NT36672C_169, ARRAY_SIZE(SEQ_NT36672C_169), 0, },
	{SEQ_NT36672C_170, ARRAY_SIZE(SEQ_NT36672C_170), 0, },
	{SEQ_NT36672C_171, ARRAY_SIZE(SEQ_NT36672C_171), 0, },
	{SEQ_NT36672C_172, ARRAY_SIZE(SEQ_NT36672C_172), 0, },
	{SEQ_NT36672C_173, ARRAY_SIZE(SEQ_NT36672C_173), 0, },
	{SEQ_NT36672C_174, ARRAY_SIZE(SEQ_NT36672C_174), 0, },
	{SEQ_NT36672C_175, ARRAY_SIZE(SEQ_NT36672C_175), 0, },
	{SEQ_NT36672C_176, ARRAY_SIZE(SEQ_NT36672C_176), 0, },
	{SEQ_NT36672C_177, ARRAY_SIZE(SEQ_NT36672C_177), 0, },
	{SEQ_NT36672C_178, ARRAY_SIZE(SEQ_NT36672C_178), 0, },
	{SEQ_NT36672C_179, ARRAY_SIZE(SEQ_NT36672C_179), 0, },
	{SEQ_NT36672C_180, ARRAY_SIZE(SEQ_NT36672C_180), 0, },
	{SEQ_NT36672C_181, ARRAY_SIZE(SEQ_NT36672C_181), 0, },
	{SEQ_NT36672C_182, ARRAY_SIZE(SEQ_NT36672C_182), 0, },
	{SEQ_NT36672C_183, ARRAY_SIZE(SEQ_NT36672C_183), 0, },
	{SEQ_NT36672C_184, ARRAY_SIZE(SEQ_NT36672C_184), 0, },
	{SEQ_NT36672C_185, ARRAY_SIZE(SEQ_NT36672C_185), 0, },
	{SEQ_NT36672C_186, ARRAY_SIZE(SEQ_NT36672C_186), 0, },
	{SEQ_NT36672C_187, ARRAY_SIZE(SEQ_NT36672C_187), 0, },
	{SEQ_NT36672C_188, ARRAY_SIZE(SEQ_NT36672C_188), 0, },
	{SEQ_NT36672C_189, ARRAY_SIZE(SEQ_NT36672C_189), 0, },
	{SEQ_NT36672C_190, ARRAY_SIZE(SEQ_NT36672C_190), 0, },
	{SEQ_NT36672C_191, ARRAY_SIZE(SEQ_NT36672C_191), 0, },
	{SEQ_NT36672C_192, ARRAY_SIZE(SEQ_NT36672C_192), 0, },
	{SEQ_NT36672C_193, ARRAY_SIZE(SEQ_NT36672C_193), 0, },
	{SEQ_NT36672C_194, ARRAY_SIZE(SEQ_NT36672C_194), 0, },
	{SEQ_NT36672C_195, ARRAY_SIZE(SEQ_NT36672C_195), 0, },
	{SEQ_NT36672C_196, ARRAY_SIZE(SEQ_NT36672C_196), 0, },
	{SEQ_NT36672C_197, ARRAY_SIZE(SEQ_NT36672C_197), 0, },
	{SEQ_NT36672C_198, ARRAY_SIZE(SEQ_NT36672C_198), 0, },
	{SEQ_NT36672C_199, ARRAY_SIZE(SEQ_NT36672C_199), 0, },
	{SEQ_NT36672C_200, ARRAY_SIZE(SEQ_NT36672C_200), 0, },
	{SEQ_NT36672C_201, ARRAY_SIZE(SEQ_NT36672C_201), 0, },
	{SEQ_NT36672C_202, ARRAY_SIZE(SEQ_NT36672C_202), 0, },
	{SEQ_NT36672C_203, ARRAY_SIZE(SEQ_NT36672C_203), 0, },
	{SEQ_NT36672C_204, ARRAY_SIZE(SEQ_NT36672C_204), 0, },
	{SEQ_NT36672C_205, ARRAY_SIZE(SEQ_NT36672C_205), 0, },
	{SEQ_NT36672C_206, ARRAY_SIZE(SEQ_NT36672C_206), 0, },
	{SEQ_NT36672C_207, ARRAY_SIZE(SEQ_NT36672C_207), 0, },
	{SEQ_NT36672C_208, ARRAY_SIZE(SEQ_NT36672C_208), 0, },
	{SEQ_NT36672C_209, ARRAY_SIZE(SEQ_NT36672C_209), 0, },
	{SEQ_NT36672C_210, ARRAY_SIZE(SEQ_NT36672C_210), 0, },
	{SEQ_NT36672C_211, ARRAY_SIZE(SEQ_NT36672C_211), 0, },
	{SEQ_NT36672C_212, ARRAY_SIZE(SEQ_NT36672C_212), 0, },
	{SEQ_NT36672C_213, ARRAY_SIZE(SEQ_NT36672C_213), 0, },
	{SEQ_NT36672C_214, ARRAY_SIZE(SEQ_NT36672C_214), 0, },
	{SEQ_NT36672C_215, ARRAY_SIZE(SEQ_NT36672C_215), 0, },
	{SEQ_NT36672C_216, ARRAY_SIZE(SEQ_NT36672C_216), 0, },
	{SEQ_NT36672C_217, ARRAY_SIZE(SEQ_NT36672C_217), 0, },
	{SEQ_NT36672C_218, ARRAY_SIZE(SEQ_NT36672C_218), 0, },
	{SEQ_NT36672C_219, ARRAY_SIZE(SEQ_NT36672C_219), 0, },
	{SEQ_NT36672C_220, ARRAY_SIZE(SEQ_NT36672C_220), 0, },
	{SEQ_NT36672C_221, ARRAY_SIZE(SEQ_NT36672C_221), 0, },
	{SEQ_NT36672C_222, ARRAY_SIZE(SEQ_NT36672C_222), 0, },
	{SEQ_NT36672C_223, ARRAY_SIZE(SEQ_NT36672C_223), 0, },
	{SEQ_NT36672C_224, ARRAY_SIZE(SEQ_NT36672C_224), 0, },
	{SEQ_NT36672C_225, ARRAY_SIZE(SEQ_NT36672C_225), 0, },
	{SEQ_NT36672C_226, ARRAY_SIZE(SEQ_NT36672C_226), 0, },
	{SEQ_NT36672C_227, ARRAY_SIZE(SEQ_NT36672C_227), 0, },
	{SEQ_NT36672C_228, ARRAY_SIZE(SEQ_NT36672C_228), 0, },
	{SEQ_NT36672C_229, ARRAY_SIZE(SEQ_NT36672C_229), 0, },
	{SEQ_NT36672C_230, ARRAY_SIZE(SEQ_NT36672C_230), 0, },
	{SEQ_NT36672C_231, ARRAY_SIZE(SEQ_NT36672C_231), 0, },
	{SEQ_NT36672C_232, ARRAY_SIZE(SEQ_NT36672C_232), 0, },
	{SEQ_NT36672C_233, ARRAY_SIZE(SEQ_NT36672C_233), 0, },
	{SEQ_NT36672C_234, ARRAY_SIZE(SEQ_NT36672C_234), 0, },
	{SEQ_NT36672C_235, ARRAY_SIZE(SEQ_NT36672C_235), 0, },
	{SEQ_NT36672C_236, ARRAY_SIZE(SEQ_NT36672C_236), 0, },
	{SEQ_NT36672C_237, ARRAY_SIZE(SEQ_NT36672C_237), 0, },
	{SEQ_NT36672C_238, ARRAY_SIZE(SEQ_NT36672C_238), 0, },
	{SEQ_NT36672C_239, ARRAY_SIZE(SEQ_NT36672C_239), 0, },
	{SEQ_NT36672C_240, ARRAY_SIZE(SEQ_NT36672C_240), 0, },
	{SEQ_NT36672C_241, ARRAY_SIZE(SEQ_NT36672C_241), 0, },
	{SEQ_NT36672C_242, ARRAY_SIZE(SEQ_NT36672C_242), 0, },
	{SEQ_NT36672C_243, ARRAY_SIZE(SEQ_NT36672C_243), 0, },
	{SEQ_NT36672C_244, ARRAY_SIZE(SEQ_NT36672C_244), 0, },
	{SEQ_NT36672C_245, ARRAY_SIZE(SEQ_NT36672C_245), 0, },
	{SEQ_NT36672C_246, ARRAY_SIZE(SEQ_NT36672C_246), 0, },
	{SEQ_NT36672C_247, ARRAY_SIZE(SEQ_NT36672C_247), 0, },
	{SEQ_NT36672C_248, ARRAY_SIZE(SEQ_NT36672C_248), 0, },
	{SEQ_NT36672C_249, ARRAY_SIZE(SEQ_NT36672C_249), 0, },
	{SEQ_NT36672C_250, ARRAY_SIZE(SEQ_NT36672C_250), 0, },
	{SEQ_NT36672C_251, ARRAY_SIZE(SEQ_NT36672C_251), 0, },
	{SEQ_NT36672C_252, ARRAY_SIZE(SEQ_NT36672C_252), 0, },
	{SEQ_NT36672C_253, ARRAY_SIZE(SEQ_NT36672C_253), 0, },
	{SEQ_NT36672C_254, ARRAY_SIZE(SEQ_NT36672C_254), 0, },
	{SEQ_NT36672C_255, ARRAY_SIZE(SEQ_NT36672C_255), 0, },
	{SEQ_NT36672C_256, ARRAY_SIZE(SEQ_NT36672C_256), 0, },
	{SEQ_NT36672C_257, ARRAY_SIZE(SEQ_NT36672C_257), 0, },
	{SEQ_NT36672C_258, ARRAY_SIZE(SEQ_NT36672C_258), 0, },
	{SEQ_NT36672C_259, ARRAY_SIZE(SEQ_NT36672C_259), 0, },
	{SEQ_NT36672C_260, ARRAY_SIZE(SEQ_NT36672C_260), 0, },
	{SEQ_NT36672C_261, ARRAY_SIZE(SEQ_NT36672C_261), 0, },
	{SEQ_NT36672C_262, ARRAY_SIZE(SEQ_NT36672C_262), 0, },
	{SEQ_NT36672C_263, ARRAY_SIZE(SEQ_NT36672C_263), 0, },
	{SEQ_NT36672C_264, ARRAY_SIZE(SEQ_NT36672C_264), 0, },
	{SEQ_NT36672C_265, ARRAY_SIZE(SEQ_NT36672C_265), 0, },
	{SEQ_NT36672C_266, ARRAY_SIZE(SEQ_NT36672C_266), 0, },
	{SEQ_NT36672C_267, ARRAY_SIZE(SEQ_NT36672C_267), 0, },
	{SEQ_NT36672C_268, ARRAY_SIZE(SEQ_NT36672C_268), 0, },
	{SEQ_NT36672C_269, ARRAY_SIZE(SEQ_NT36672C_269), 0, },
	{SEQ_NT36672C_270, ARRAY_SIZE(SEQ_NT36672C_270), 0, },
	{SEQ_NT36672C_271, ARRAY_SIZE(SEQ_NT36672C_271), 0, },
	{SEQ_NT36672C_272, ARRAY_SIZE(SEQ_NT36672C_272), 0, },
	{SEQ_NT36672C_273, ARRAY_SIZE(SEQ_NT36672C_273), 0, },
	{SEQ_NT36672C_274, ARRAY_SIZE(SEQ_NT36672C_274), 0, },
	{SEQ_NT36672C_275, ARRAY_SIZE(SEQ_NT36672C_275), 0, },
	{SEQ_NT36672C_276, ARRAY_SIZE(SEQ_NT36672C_276), 0, },
	{SEQ_NT36672C_277, ARRAY_SIZE(SEQ_NT36672C_277), 0, },
	{SEQ_NT36672C_278, ARRAY_SIZE(SEQ_NT36672C_278), 0, },
	{SEQ_NT36672C_279, ARRAY_SIZE(SEQ_NT36672C_279), 0, },
	{SEQ_NT36672C_280, ARRAY_SIZE(SEQ_NT36672C_280), 0, },
	{SEQ_NT36672C_281, ARRAY_SIZE(SEQ_NT36672C_281), 0, },
	{SEQ_NT36672C_282, ARRAY_SIZE(SEQ_NT36672C_282), 0, },
	{SEQ_NT36672C_283, ARRAY_SIZE(SEQ_NT36672C_283), 0, },
	{SEQ_NT36672C_284, ARRAY_SIZE(SEQ_NT36672C_284), 0, },
	{SEQ_NT36672C_285, ARRAY_SIZE(SEQ_NT36672C_285), 0, },
	{SEQ_NT36672C_286, ARRAY_SIZE(SEQ_NT36672C_286), 0, },
	{SEQ_NT36672C_287, ARRAY_SIZE(SEQ_NT36672C_287), 0, },
	{SEQ_NT36672C_288, ARRAY_SIZE(SEQ_NT36672C_288), 0, },
	{SEQ_NT36672C_289, ARRAY_SIZE(SEQ_NT36672C_289), 0, },
	{SEQ_NT36672C_290, ARRAY_SIZE(SEQ_NT36672C_290), 0, },
	{SEQ_NT36672C_291, ARRAY_SIZE(SEQ_NT36672C_291), 0, },
	{SEQ_NT36672C_292, ARRAY_SIZE(SEQ_NT36672C_292), 0, },
	{SEQ_NT36672C_293, ARRAY_SIZE(SEQ_NT36672C_293), 0, },
	{SEQ_NT36672C_294, ARRAY_SIZE(SEQ_NT36672C_294), 0, },
	{SEQ_NT36672C_295, ARRAY_SIZE(SEQ_NT36672C_295), 0, },
	{SEQ_NT36672C_296, ARRAY_SIZE(SEQ_NT36672C_296), 0, },
	{SEQ_NT36672C_297, ARRAY_SIZE(SEQ_NT36672C_297), 0, },
	{SEQ_NT36672C_298, ARRAY_SIZE(SEQ_NT36672C_298), 0, },
	{SEQ_NT36672C_299, ARRAY_SIZE(SEQ_NT36672C_299), 0, },
	{SEQ_NT36672C_300, ARRAY_SIZE(SEQ_NT36672C_300), 0, },
	{SEQ_NT36672C_301, ARRAY_SIZE(SEQ_NT36672C_301), 0, },
	{SEQ_NT36672C_302, ARRAY_SIZE(SEQ_NT36672C_302), 0, },
	{SEQ_NT36672C_303, ARRAY_SIZE(SEQ_NT36672C_303), 0, },
	{SEQ_NT36672C_304, ARRAY_SIZE(SEQ_NT36672C_304), 0, },
	{SEQ_NT36672C_305, ARRAY_SIZE(SEQ_NT36672C_305), 0, },
	{SEQ_NT36672C_306, ARRAY_SIZE(SEQ_NT36672C_306), 0, },
	{SEQ_NT36672C_307, ARRAY_SIZE(SEQ_NT36672C_307), 0, },
	{SEQ_NT36672C_308, ARRAY_SIZE(SEQ_NT36672C_308), 0, },
	{SEQ_NT36672C_309, ARRAY_SIZE(SEQ_NT36672C_309), 0, },
	{SEQ_NT36672C_310, ARRAY_SIZE(SEQ_NT36672C_310), 0, },
	{SEQ_NT36672C_311, ARRAY_SIZE(SEQ_NT36672C_311), 0, },
	{SEQ_NT36672C_312, ARRAY_SIZE(SEQ_NT36672C_312), 0, },
	{SEQ_NT36672C_313, ARRAY_SIZE(SEQ_NT36672C_313), 0, },
	{SEQ_NT36672C_314, ARRAY_SIZE(SEQ_NT36672C_314), 0, },
	{SEQ_NT36672C_315, ARRAY_SIZE(SEQ_NT36672C_315), 0, },
	{SEQ_NT36672C_316, ARRAY_SIZE(SEQ_NT36672C_316), 0, },
	{SEQ_NT36672C_317, ARRAY_SIZE(SEQ_NT36672C_317), 0, },
	{SEQ_NT36672C_318, ARRAY_SIZE(SEQ_NT36672C_318), 0, },
	{SEQ_NT36672C_319, ARRAY_SIZE(SEQ_NT36672C_319), 0, },
	{SEQ_NT36672C_320, ARRAY_SIZE(SEQ_NT36672C_320), 0, },
	{SEQ_NT36672C_321, ARRAY_SIZE(SEQ_NT36672C_321), 0, },
	{SEQ_NT36672C_322, ARRAY_SIZE(SEQ_NT36672C_322), 0, },
	{SEQ_NT36672C_323, ARRAY_SIZE(SEQ_NT36672C_323), 0, },
	{SEQ_NT36672C_324, ARRAY_SIZE(SEQ_NT36672C_324), 0, },
	{SEQ_NT36672C_325, ARRAY_SIZE(SEQ_NT36672C_325), 0, },
	{SEQ_NT36672C_326, ARRAY_SIZE(SEQ_NT36672C_326), 0, },
	{SEQ_NT36672C_327, ARRAY_SIZE(SEQ_NT36672C_327), 0, },
	{SEQ_NT36672C_328, ARRAY_SIZE(SEQ_NT36672C_328), 0, },
	{SEQ_NT36672C_329, ARRAY_SIZE(SEQ_NT36672C_329), 0, },
	{SEQ_NT36672C_330, ARRAY_SIZE(SEQ_NT36672C_330), 0, },
	{SEQ_NT36672C_331, ARRAY_SIZE(SEQ_NT36672C_331), 0, },
	{SEQ_NT36672C_332, ARRAY_SIZE(SEQ_NT36672C_332), 0, },
	{SEQ_NT36672C_333, ARRAY_SIZE(SEQ_NT36672C_333), 0, },
	{SEQ_NT36672C_334, ARRAY_SIZE(SEQ_NT36672C_334), 0, },
	{SEQ_NT36672C_335, ARRAY_SIZE(SEQ_NT36672C_335), 0, },
	{SEQ_NT36672C_336, ARRAY_SIZE(SEQ_NT36672C_336), 0, },
	{SEQ_NT36672C_337, ARRAY_SIZE(SEQ_NT36672C_337), 0, },
	{SEQ_NT36672C_338, ARRAY_SIZE(SEQ_NT36672C_338), 0, },
	{SEQ_NT36672C_339, ARRAY_SIZE(SEQ_NT36672C_339), 0, },
	{SEQ_NT36672C_340, ARRAY_SIZE(SEQ_NT36672C_340), 0, },
	{SEQ_NT36672C_341, ARRAY_SIZE(SEQ_NT36672C_341), 0, },
	{SEQ_NT36672C_342, ARRAY_SIZE(SEQ_NT36672C_342), 0, },
	{SEQ_NT36672C_343, ARRAY_SIZE(SEQ_NT36672C_343), 0, },
	{SEQ_NT36672C_344, ARRAY_SIZE(SEQ_NT36672C_344), 0, },
	{SEQ_NT36672C_345, ARRAY_SIZE(SEQ_NT36672C_345), 0, },
	{SEQ_NT36672C_346, ARRAY_SIZE(SEQ_NT36672C_346), 0, },
	{SEQ_NT36672C_347, ARRAY_SIZE(SEQ_NT36672C_347), 0, },
	{SEQ_NT36672C_348, ARRAY_SIZE(SEQ_NT36672C_348), 0, },
	{SEQ_NT36672C_349, ARRAY_SIZE(SEQ_NT36672C_349), 0, },
	{SEQ_NT36672C_350, ARRAY_SIZE(SEQ_NT36672C_350), 0, },
	{SEQ_NT36672C_351, ARRAY_SIZE(SEQ_NT36672C_351), 0, },
	{SEQ_NT36672C_352, ARRAY_SIZE(SEQ_NT36672C_352), 0, },
	{SEQ_NT36672C_353, ARRAY_SIZE(SEQ_NT36672C_353), 0, },
	{SEQ_NT36672C_354, ARRAY_SIZE(SEQ_NT36672C_354), 0, },
	{SEQ_NT36672C_355, ARRAY_SIZE(SEQ_NT36672C_355), 0, },
	{SEQ_NT36672C_356, ARRAY_SIZE(SEQ_NT36672C_356), 0, },
	{SEQ_NT36672C_357, ARRAY_SIZE(SEQ_NT36672C_357), 0, },
	{SEQ_NT36672C_358, ARRAY_SIZE(SEQ_NT36672C_358), 0, },
	{SEQ_NT36672C_359, ARRAY_SIZE(SEQ_NT36672C_359), 0, },
	{SEQ_NT36672C_360, ARRAY_SIZE(SEQ_NT36672C_360), 0, },
	{SEQ_NT36672C_361, ARRAY_SIZE(SEQ_NT36672C_361), 0, },
	{SEQ_NT36672C_362, ARRAY_SIZE(SEQ_NT36672C_362), 0, },
	{SEQ_NT36672C_363, ARRAY_SIZE(SEQ_NT36672C_363), 0, },
	{SEQ_NT36672C_364, ARRAY_SIZE(SEQ_NT36672C_364), 0, },
	{SEQ_NT36672C_365, ARRAY_SIZE(SEQ_NT36672C_365), 0, },
	{SEQ_NT36672C_366, ARRAY_SIZE(SEQ_NT36672C_366), 0, },
	{SEQ_NT36672C_367, ARRAY_SIZE(SEQ_NT36672C_367), 0, },
	{SEQ_NT36672C_368, ARRAY_SIZE(SEQ_NT36672C_368), 0, },
	{SEQ_NT36672C_369, ARRAY_SIZE(SEQ_NT36672C_369), 0, },
	{SEQ_NT36672C_370, ARRAY_SIZE(SEQ_NT36672C_370), 0, },
	{SEQ_NT36672C_371, ARRAY_SIZE(SEQ_NT36672C_371), 0, },
	{SEQ_NT36672C_372, ARRAY_SIZE(SEQ_NT36672C_372), 0, },
	{SEQ_NT36672C_373, ARRAY_SIZE(SEQ_NT36672C_373), 0, },
	{SEQ_NT36672C_374, ARRAY_SIZE(SEQ_NT36672C_374), 0, },
	{SEQ_NT36672C_375, ARRAY_SIZE(SEQ_NT36672C_375), 0, },
};

/* DO NOT REMOVE: back to page 1 for setting DCS commands */
static unsigned char SEQ_NT36672C_BACK_TO_PAGE_1_A[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36672C_BACK_TO_PAGE_1_B[] = {
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

#endif /* __NT36672C_PARAM_H__ */
