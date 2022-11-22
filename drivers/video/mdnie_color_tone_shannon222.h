#ifndef __MDNIE_COLOR_TONE_H__
#define __MDNIE_COLOR_TONE_H__

#include "mdnie.h"

static const unsigned short tune_scr_setting[9][3] = {
	{0xff, 0xf7, 0xf8},
	{0xff, 0xf9, 0xfe},
	{0xfa, 0xf8, 0xff},
	{0xff, 0xfc, 0xf9},
	{0xff, 0xff, 0xff},
	{0xf8, 0xfa, 0xff},
	{0xfc, 0xff, 0xf8},
	{0xfb, 0xff, 0xfb},
	{0xf9, 0xff, 0xff},
};

static unsigned short tune_negative[] = {
/*start K mini negative*/
	0x0007, 0x0006,		/*DNR roi latency clk on 00 01 1 0 */
	0x0009, 0x0006,		/*DE roi latency clk on 00 01 1 0 */
	0x000a, 0x0006,		/*CS roi latency clk on 00 01 1 0 */
	0x000b, 0x0006,		/*CC roi latency clk on 00 01 1 0 */
	0x000c, 0x0007,		/*SCR roi latency clk on 00 01 1 0 */
	0x0091, 0x00ff,		/*SCR RrCr */
	0x0092, 0xff00,		/*SCR RgCg */
	0x0093, 0xff00,		/*SCR RbCb */
	0x0094, 0xff00,		/*SCR GrMr */
	0x0095, 0x00ff,		/*SCR GgMg */
	0x0096, 0xff00,		/*SCR GbMb */
	0x0097, 0xff00,		/*SCR BrYr */
	0x0098, 0xff00,		/*SCR BgYg */
	0x0099, 0x00ff,		/*SCR BbYb */
	0x009a, 0xff00,		/*SCR KrWr */
	0x009b, 0xff00,		/*SCR KgWg */
	0x009c, 0xff00,		/*SCR KbWb */
	0x00ff, 0x0000,		/*Mask Release */
/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_color_blind[] = {
/*start K mini color adjustment*/
	0x0007, 0x0006,		/*DNR roi latency clk on 00 01 1 0 */
	0x0009, 0x0006,		/*DE roi latency clk on 00 01 1 0 */
	0x000a, 0x0006,		/*CS roi latency clk on 00 01 1 0 */
	0x000b, 0x0006,		/*CC roi latency clk on 00 01 1 0 */
	0x000c, 0x0007,		/*SCR roi latency clk on 00 01 1 0 */
	0x0091, 0xff00,		/*SCR RrCr */
	0x0092, 0x00ff,		/*SCR RgCg */
	0x0093, 0x00ff,		/*SCR RbCb */
	0x0094, 0x00ff,		/*SCR GrMr */
	0x0095, 0xff00,		/*SCR GgMg */
	0x0096, 0x00ff,		/*SCR GbMb */
	0x0097, 0x00ff,		/*SCR BrYr */
	0x0098, 0x00ff,		/*SCR BgYg */
	0x0099, 0xff00,		/*SCR BbYb */
	0x009a, 0x00ff,		/*SCR KrWr */
	0x009b, 0x00ff,		/*SCR KgWg */
	0x009c, 0x00ff,		/*SCR KbWb */
	0x00ff, 0x0000,		/*Mask Release */
/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_screen_curtain[] = {
	/*start K mini dark screen*/
	0x0007,0x0006,	/*DNR roi latency clk on 00 01 1 0*/
	0x0009,0x0006,	/*DE roi latency clk on 00 01 1 0*/
	0x000a,0x0006,	/*CS roi latency clk on 00 01 1 0*/
	0x000b,0x0006,	/*CC roi latency clk on 00 01 1 0*/
	0x000c,0x0007,	/*SCR roi latency clk on 00 01 1 0*/
	0x0091,0x0000,	/*SCR RrCr*/
	0x0092,0x0000,	/*SCR RgCg*/
	0x0093,0x0000,	/*SCR RbCb*/
	0x0094,0x0000,	/*SCR GrMr*/
	0x0095,0x0000,	/*SCR GgMg*/
	0x0096,0x0000,	/*SCR GbMb*/
	0x0097,0x0000,	/*SCR BrYr*/
	0x0098,0x0000,	/*SCR BgYg*/
	0x0099,0x0000,	/*SCR BbYb*/
	0x009a,0x0000,	/*SCR KrWr*/
	0x009b,0x0000,	/*SCR KgWg*/
	0x009c,0x0000,	/*SCR KbWb*/
	0x00ff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_grayscale[] = {
	/*start K mini grayscale*/
	0x0007,0x0006,	/*DNR roi latency clk on 00 01 1 0*/
	0x0009,0x0006,	/*DE roi latency clk on 00 01 1 0*/
	0x000a,0x0006,	/*CS roi latency clk on 00 01 1 0*/
	0x000b,0x0006,	/*CC roi latency clk on 00 01 1 0*/
	0x000c,0x0007,	/*SCR roi latency clk on 00 01 1 0*/
	0x0091,0x4cb3,	/*SCR RrCr*/
	0x0092,0x4cb3,	/*SCR RgCg*/
	0x0093,0x4cb3,	/*SCR RbCb*/
	0x0094,0x9669,	/*SCR GrMr*/
	0x0095,0x9669,	/*SCR GgMg*/
	0x0096,0x9669,	/*SCR GbMb*/
	0x0097,0x1de2,	/*SCR BrYr*/
	0x0098,0x1de2,	/*SCR BgYg*/
	0x0099,0x1de2,	/*SCR BbYb*/
	0x009a,0x00ff,	/*SCR KrWr*/
	0x009b,0x00ff,	/*SCR KgWg*/
	0x009c,0x00ff,	/*SCR KbWb*/
	0x00ff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_grayscale_negative[] = {
	/*start K mini grayscale_negative*/
	0x0007,0x0006,	/*DNR roi latency clk on 00 01 1 0*/
	0x0009,0x0006,	/*DE roi latency clk on 00 01 1 0*/
	0x000a,0x0006,	/*CS roi latency clk on 00 01 1 0*/
	0x000b,0x0006,	/*CC roi latency clk on 00 01 1 0*/
	0x000c,0x0007,	/*SCR roi latency clk on 00 01 1 0*/
	0x0091,0xb34c,	/*SCR RrCr*/
	0x0092,0xb34c,	/*SCR RgCg*/
	0x0093,0xb34c,	/*SCR RbCb*/
	0x0094,0x6996,	/*SCR GrMr*/
	0x0095,0x6996,	/*SCR GgMg*/
	0x0096,0x6996,	/*SCR GbMb*/
	0x0097,0xe21d,	/*SCR BrYr*/
	0x0098,0xe21d,	/*SCR BgYg*/
	0x0099,0xe21d,	/*SCR BbYb*/
	0x009a,0xff00,	/*SCR KrWr*/
	0x009b,0xff00,	/*SCR KgWg*/
	0x009c,0xff00,	/*SCR KbWb*/
	0x00ff,0x0000,	/*Mask Release*/
	/*end*/
	END_SEQ, 0x0000,
};

static unsigned short tune_bypass_off[] = {
/*start K mini bypass*/
	0x0007, 0x0006,		/*DNR roi latency clk on 00 01 1 0 */
	0x0009, 0x0006,		/*DE roi latency clk on 00 01 1 0 */
	0x000a, 0x0006,		/*CS roi latency clk on 00 01 1 0 */
	0x000b, 0x0006,		/*CC roi latency clk on 00 01 1 0 */
	0x000c, 0x0006,		/*SCR roi latency clk on 00 01 1 0 */
	0x00ff, 0x0000,		/*Mask Release */
/*end*/
	END_SEQ, 0x0000,
};

unsigned short tune_bypass_on[] = {
/*start K mini bypass*/
	0x0007, 0x0006,		/*DNR roi latency clk on 00 01 1 0 */
	0x0009, 0x0006,		/*DE roi latency clk on 00 01 1 0 */
	0x000a, 0x0006,		/*CS roi latency clk on 00 01 1 0 */
	0x000b, 0x0006,		/*CC roi latency clk on 00 01 1 0 */
	0x000c, 0x0006,		/*SCR roi latency clk on 00 01 1 0 */
	0x00ff, 0x0000,		/*Mask Release */
/*end*/
	END_SEQ, 0x0000,
};

struct mdnie_tuning_info accessibility_table[ACCESSIBILITY_MAX] = {
	{NULL,			NULL},
	{"negative",		tune_negative},
	{"color_blind",		tune_color_blind},
	{"screen_curtain",	tune_screen_curtain},
	{"grayscale",	tune_grayscale},
	{"grayscale_negative",	tune_grayscale_negative},	
};

struct mdnie_tuning_info bypass_table[BYPASS_MAX] = {
	{"bypass_off",		tune_bypass_off},
	{"bypass_on",		tune_bypass_on},
};

#endif /* __MDNIE_COLOR_TONE_H__ */
