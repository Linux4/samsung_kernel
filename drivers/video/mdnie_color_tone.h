#ifndef __MDNIE_COLOR_TONE_H__
#define __MDNIE_COLOR_TONE_H__

#include "mdnie.h"

/* 2013.08.30 */

static const unsigned short tune_scr_setting[9][3] = {
	{0xff, 0xf7, 0xf8},
	{0xff, 0xfa, 0xfe},
	{0xfb, 0xf9, 0xff},
	{0xff, 0xfd, 0xfa},
	{0xff, 0xff, 0xff},
	{0xf9, 0xfb, 0xff},
	{0xfc, 0xff, 0xf8},
	{0xfb, 0xff, 0xfb},
	{0xf9, 0xff, 0xff},
};

static unsigned short tune_negative[] = {
	//start Mega2 negative
	0x0007,0x0006,	//DNR roi latency clk module 00 01 1 0
	0x0009,0x0006,	//DE roi latency clk module 00 01 1 0
	0x000a,0x0002,	//CS roi latency clk module 00 00 1 0
	0x000b,0x0002,	//CC roi latency clk module 00 00 1 0
	0x000c,0x0003,	//SCR roi latency clk module 00 00 1 0
	0x0091,0x00ff,	//SCR RrCr
	0x0092,0xff00,	//SCR RgCg
	0x0093,0xff00,	//SCR RbCb
	0x0094,0xff00,	//SCR GrMr
	0x0095,0x00ff,	//SCR GgMg
	0x0096,0xff00,	//SCR GbMb
	0x0097,0xff00,	//SCR BrYr
	0x0098,0xff00,	//SCR BgYg
	0x0099,0x00ff,	//SCR BbYb
	0x009a,0xff00,	//SCR KrWr
	0x009b,0xff00,	//SCR KgWg
	0x009c,0xff00,	//SCR KbWb
	0x00ff,0x0000,	//Mask Release
	//end
	END_SEQ, 0x0000,
};

static unsigned short tune_color_blind[] = {
	//start Mega2 color blind
	0x0007,0x0006,	//DNR roi latency clk module 00 01 1 0
	0x0009,0x0006,	//DE roi latency clk module 00 01 1 0
	0x000a,0x0002,	//CS roi latency clk module 00 00 1 0
	0x000b,0x0002,	//CC roi latency clk module 00 00 1 0
	0x000c,0x0003,	//SCR roi latency clk module 00 00 1 0
	0x0091,0xff00,	//SCR RrCr
	0x0092,0x00ff,	//SCR RgCg
	0x0093,0x00ff,	//SCR RbCb
	0x0094,0x00ff,	//SCR GrMr
	0x0095,0xff00,	//SCR GgMg
	0x0096,0x00ff,	//SCR GbMb
	0x0097,0x00ff,	//SCR BrYr
	0x0098,0x00ff,	//SCR BgYg
	0x0099,0xff00,	//SCR BbYb
	0x009a,0x00ff,	//SCR KrWr
	0x009b,0x00ff,	//SCR KgWg
	0x009c,0x00ff,	//SCR KbWb
	0x00ff,0x0000,	//Mask Release
	//end
	END_SEQ, 0x0000,
};

static unsigned short tune_bypass_off[] = {
	//start Mega2 bypass
	0x0007,0x0006,	//DNR roi latency clk module 00 01 1 0
	0x0009,0x0006,	//DE roi latency clk module 00 01 1 0
	0x000a,0x0002,	//CS roi latency clk module 00 00 1 0
	0x000b,0x0002,	//CC roi latency clk module 00 00 1 0
	0x000c,0x0002,	//SCR roi latency clk module 00 00 1 0
	0x00ff,0x0000,	//Mask Release
	//end
	END_SEQ, 0x0000,
};

static unsigned short tune_bypass_on[] = {
	//start Mega2 bypass
	0x0007,0x0006,	//DNR roi latency clk module 00 01 1 0
	0x0009,0x0006,	//DE roi latency clk module 00 01 1 0
	0x000a,0x0002,	//CS roi latency clk module 00 00 1 0
	0x000b,0x0002,	//CC roi latency clk module 00 00 1 0
	0x000c,0x0002,	//SCR roi latency clk module 00 00 1 0
	0x00ff,0x0000,	//Mask Release
	//end
	END_SEQ, 0x0000,
};

struct mdnie_tuning_info negative_table = {
	"negative",		tune_negative
};

struct mdnie_tuning_info accessibility_table[ACCESSIBILITY_MAX] = {
	{NULL,			NULL},
	{"negative",		tune_negative},
	{"color_blind",		tune_color_blind},
};

struct mdnie_tuning_info bypass_table[BYPASS_MAX] = {
	{"bypass_off",		tune_bypass_off},
	{"bypass_on",		tune_bypass_on},
};

#endif /* __MDNIE_COLOR_TONE_H__ */
