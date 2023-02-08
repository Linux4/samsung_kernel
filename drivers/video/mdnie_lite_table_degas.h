#ifndef __MDNIE_TABLE_H__
#define __MDNIE_TABLE_H__

/* 2014.04.10 */
/* SCR Position can be different each panel */
#define ASCR_CMD		MDNIE_CMD2

/* SCR Position can be different each DDI(S6D7AA0) */
#define MDNIE_RED_R		1		/* SCR_CR[7:0] */
#define MDNIE_RED_G		3		/* SCR_CG[7:0] */
#define MDNIE_RED_B		5		/* SCR_CB[7:0] */
#define MDNIE_BLUE_R		7		/* SCR_MR[7:0] */
#define MDNIE_BLUE_G		9		/* SCR_MG[7:0] */
#define MDNIE_BLUE_B		11		/* SCR_MB[7:0] */
#define MDNIE_GREEN_R		13		/* SCR_YR[7:0] */
#define MDNIE_GREEN_G		15		/* SCR_YG[7:0] */
#define MDNIE_GREEN_B		17		/* SCR_YB[7:0] */
#define MDNIE_WHITE_R		19		/* SCR_WR[7:0] */
#define MDNIE_WHITE_G		21		/* SCR_WG[7:0] */
#define MDNIE_WHITE_B		23		/* SCR_WB[7:0] */

#define MDNIE_COLOR_BLIND_OFFSET	MDNIE_RED_R

#define COLOR_OFFSET_F1(x, y)		(((y << 10) - (((x << 10) * 99) / 91) - (6 << 10)) >> 10)
#define COLOR_OFFSET_F2(x, y)		(((y << 10) - (((x << 10) * 164) / 157) - (8 << 10)) >> 10)
#define COLOR_OFFSET_F3(x, y)		(((y << 10) + (((x << 10) * 218) / 39) - (20166 << 10)) >> 10)
#define COLOR_OFFSET_F4(x, y)		(((y << 10) + (((x << 10) * 23) / 8) - (11610 << 10)) >> 10)

/* color coordination order is WR, WG, WB */
static unsigned char coordinate_data[][3] = {
	{0xff, 0xff, 0xff}, /* dummy */
	{0xff, 0xf8, 0xf9}, /* Tune_1 */
	{0xff, 0xfa, 0xfe}, /* Tune_2 */
	{0xfb, 0xf9, 0xff}, /* Tune_3 */
	{0xff, 0xfe, 0xfb}, /* Tune_4 */
	{0xff, 0xff, 0xff}, /* Tune_5 */
	{0xf9, 0xfb, 0xff}, /* Tune_6 */
	{0xfc, 0xff, 0xf9}, /* Tune_7 */
	{0xfb, 0xff, 0xfb}, /* Tune_8 */
	{0xfa, 0xff, 0xff}, /* Tune_9 */
};

////////////////// STANDARD_UI /////////////////////
static unsigned char STANDARD_UI_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x02, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char STANDARD_UI_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char STANDARD_UI_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char STANDARD_UI_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char STANDARD_UI_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char STANDARD_UI_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1 0.175
	0x7e,
	0x1f, //cc r2
	0x96,
	0x1f, //cc r3
	0xec,
	0x1f, //cc g1
	0xca,
	0x04, //cc g2
	0x4a,
	0x1f, //cc g3
	0xec,
	0x1f, //cc b1
	0xca,
	0x1f, //cc b2
	0x97,
	0x04, //cc b3
	0x9f,
	/* end */
};

////////////////// STANDARD_GALLERY /////////////////////
static unsigned char STANDARD_GALLERY_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x06, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char STANDARD_GALLERY_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char STANDARD_GALLERY_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char STANDARD_GALLERY_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char STANDARD_GALLERY_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char STANDARD_GALLERY_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1 0.175
	0x7e,
	0x1f, //cc r2
	0x96,
	0x1f, //cc r3
	0xec,
	0x1f, //cc g1
	0xca,
	0x04, //cc g2
	0x4a,
	0x1f, //cc g3
	0xec,
	0x1f, //cc b1
	0xca,
	0x1f, //cc b2
	0x97,
	0x04, //cc b3
	0x9f,
	/* end */
};

////////////////// STANDARD_VIDEO /////////////////////
static unsigned char STANDARD_VIDEO_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x06, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char STANDARD_VIDEO_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char STANDARD_VIDEO_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char STANDARD_VIDEO_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char STANDARD_VIDEO_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char STANDARD_VIDEO_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1 0.175
	0x7e,
	0x1f, //cc r2
	0x96,
	0x1f, //cc r3
	0xec,
	0x1f, //cc g1
	0xca,
	0x04, //cc g2
	0x4a,
	0x1f, //cc g3
	0xec,
	0x1f, //cc b1
	0xca,
	0x1f, //cc b2
	0x97,
	0x04, //cc b3
	0x9f,
	/* end */
};

////////////////// STANDARD_VT /////////////////////
static unsigned char STANDARD_VT_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x06, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char STANDARD_VT_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char STANDARD_VT_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char STANDARD_VT_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char STANDARD_VT_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char STANDARD_VT_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1 0.175
	0x7e,
	0x1f, //cc r2
	0x96,
	0x1f, //cc r3
	0xec,
	0x1f, //cc g1
	0xca,
	0x04, //cc g2
	0x4a,
	0x1f, //cc g3
	0xec,
	0x1f, //cc b1
	0xca,
	0x1f, //cc b2
	0x97,
	0x04, //cc b3
	0x9f,
	/* end */
};

////////////////// STANDARD_BROWSER /////////////////////
static unsigned char STANDARD_BROWSER_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x02, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char STANDARD_BROWSER_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char STANDARD_BROWSER_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char STANDARD_BROWSER_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char STANDARD_BROWSER_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char STANDARD_BROWSER_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1 0.175
	0x7e,
	0x1f, //cc r2
	0x96,
	0x1f, //cc r3
	0xec,
	0x1f, //cc g1
	0xca,
	0x04, //cc g2
	0x4a,
	0x1f, //cc g3
	0xec,
	0x1f, //cc b1
	0xca,
	0x1f, //cc b2
	0x97,
	0x04, //cc b3
	0x9f,
	/* end */
};

////////////////// STANDARD_STANDARD_EBOOK /////////////////////
static unsigned char STANDARD_EBOOK_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x30, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x00, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char STANDARD_EBOOK_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char STANDARD_EBOOK_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xf5, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xe4, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char STANDARD_EBOOK_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char STANDARD_EBOOK_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char STANDARD_EBOOK_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1
	0x00,
	0x00, //cc r2
	0x00,
	0x00, //cc r3
	0x00,
	0x00, //cc g1
	0x00,
	0x04, //cc g2
	0x00,
	0x00, //cc g3
	0x00,
	0x00, //cc b1
	0x00,
	0x00, //cc b2
	0x00,
	0x04, //cc b3
	0x00,
	/* end */
};

////////////////// CAMERA /////////////////////
static unsigned char CAMERA_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x02, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char CAMERA_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char CAMERA_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char CAMERA_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char CAMERA_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char CAMERA_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1 0.175
	0x7e,
	0x1f, //cc r2
	0x96,
	0x1f, //cc r3
	0xec,
	0x1f, //cc g1
	0xca,
	0x04, //cc g2
	0x4a,
	0x1f, //cc g3
	0xec,
	0x1f, //cc b1
	0xca,
	0x1f, //cc b2
	0x97,
	0x04, //cc b3
	0x9f,
	/* end */
};


////////////////// BYPASS /////////////////////
static unsigned char BYPASS_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x00, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x00, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char BYPASS_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char BYPASS_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char BYPASS_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char BYPASS_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char BYPASS_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1
	0x00,
	0x00, //cc r2
	0x00,
	0x00, //cc r3
	0x00,
	0x00, //cc g1
	0x00,
	0x04, //cc g2
	0x00,
	0x00, //cc g3
	0x00,
	0x00, //cc b1
	0x00,
	0x00, //cc b2
	0x00,
	0x04, //cc b3
	0x00,
	/* end */
};

////////////////// EMAIL /////////////////////
static unsigned char AUTO_EMAIL_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x30, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x00, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char AUTO_EMAIL_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char AUTO_EMAIL_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xf5, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xe4, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char AUTO_EMAIL_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char AUTO_EMAIL_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char AUTO_EMAIL_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1
	0x00,
	0x00, //cc r2
	0x00,
	0x00, //cc r3
	0x00,
	0x00, //cc g1
	0x00,
	0x04, //cc g2
	0x00,
	0x00, //cc g3
	0x00,
	0x00, //cc b1
	0x00,
	0x00, //cc b2
	0x00,
	0x04, //cc b3
	0x00,
	/* end */
};


////////////////// COLOR_BLIND /////////////////////
static unsigned char COLOR_BLIND_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x30, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x00, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char COLOR_BLIND_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char COLOR_BLIND_2[] = {
	/* start */
	0xE9,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x00, //scr Rg Bg
	0xff, //scr Cb Yr
	0x00, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x00, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	/* end */
};

static unsigned char COLOR_BLIND_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char COLOR_BLIND_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char COLOR_BLIND_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1
	0x00,
	0x00, //cc r2
	0x00,
	0x00, //cc r3
	0x00,
	0x00, //cc g1
	0x00,
	0x04, //cc g2
	0x00,
	0x00, //cc g3
	0x00,
	0x00, //cc b1
	0x00,
	0x00, //cc b2
	0x00,
	0x04, //cc b3
	0x00,
	/* end */
};

////////////////// NEGATIVE /////////////////////
static unsigned char NEGATIVE_6[] = {
	/* start */
	0xE7,
	0x08, //roi_ctrl rgb_if_type mdnie_en mask 00 00 0 000
	0x30, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //HSIZE
	0x20,
	0x05, //VSIZE
	0x00,
	0x00, //sharpen cc gamma 00 0 0
	/* end */
};

static unsigned char NEGATIVE_1[] = {
	/* start */
	0xE8,
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	/* end */
};

static unsigned char NEGATIVE_2[] = {
	/* start */
	0xE9,
	0xff, //scr Cr Yb
	0x00, //scr Rr Bb
	0x00, //scr Cg Yg
	0xff, //scr Rg Bg
	0x00, //scr Cb Yr
	0xff, //scr Rb Br
	0x00, //scr Mr Mb
	0xff, //scr Gr Gb
	0xff, //scr Mg Mg
	0x00, //scr Gg Gg
	0x00, //scr Mb Mr
	0xff, //scr Gb Gr
	0x00, //scr Yr Cb
	0xff, //scr Br Rb
	0x00, //scr Yg Cg
	0xff, //scr Bg Rg
	0xff, //scr Yb Cr
	0x00, //scr Bb Rr
	0x00, //scr Wr Wb
	0xff, //scr Kr Kb
	0x00, //scr Wg Wg
	0xff, //scr Kg Kg
	0x00, //scr Wb Wr
	0xff, //scr Kb Kr
	/* end */
};

static unsigned char NEGATIVE_3[] = {
	/* start */
	0xEA,
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x00, //curve 5 b
	0x20, //curve 5 a
	0x00, //curve 6 b
	0x20, //curve 6 a
	0x00, //curve 7 b
	0x20, //curve 7 a
	0x00, //curve 8 b
	0x20, //curve 8 a
	0x00, //curve 9 b
	0x20, //curve 9 a
	0x00, //curve10 b
	0x20, //curve10 a
	0x00, //curve11 b
	0x20, //curve11 a
	0x00, //curve12 b
	0x20, //curve12 a
	/* end */
};

static unsigned char NEGATIVE_4[] = {
	/* start */
	0xEB,
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	/* end */
};

static unsigned char NEGATIVE_5[] = {
	/* start */
	0xEC,
	0x04, //cc r1
	0x00,
	0x00, //cc r2
	0x00,
	0x00, //cc r3
	0x00,
	0x00, //cc g1
	0x00,
	0x04, //cc g2
	0x00,
	0x00, //cc g3
	0x00,
	0x00, //cc b1
	0x00,
	0x00, //cc b2
	0x00,
	0x04, //cc b3
	0x00,
	/* end */
};

////////////////// HBM /////////////////////
static unsigned char LOCAL_CE_6[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
};

static unsigned char LOCAL_CE_1[] = {
	0xBA, // Start offset 0x09D4, base B1h
	/* end */
};

static unsigned char LOCAL_CE_2[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
};

static unsigned char LOCAL_CE_3[] = {
	0xBA, // Start offset 0x09D4, base B1h
	/* end */
};

static unsigned char LOCAL_CE_4[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
};

static unsigned char LOCAL_CE_5[] = {
	0xBA, // Start offset 0x09D4, base B1h
	/* end */
};


static unsigned char LEVEL1_UNLOCK[] = {
	0xF0,
	0x5A, 0x5A
};

static unsigned char LEVEL1_LOCK[] = {
	0xF0,
	0xA5, 0xA5
};

struct mdnie_table bypass_table[BYPASS_MAX] = {
	[BYPASS_ON] = MDNIE_SET(BYPASS)
};

struct mdnie_table accessibility_table[ACCESSIBILITY_MAX] = {
	[NEGATIVE] = MDNIE_SET(NEGATIVE),
	MDNIE_SET(COLOR_BLIND),
};

struct mdnie_table hbm_table[HBM_MAX] = {
	[HBM_ON] = MDNIE_SET(LOCAL_CE)
};

struct mdnie_table tuning_table[SCENARIO_MAX][MODE_MAX] = {
	{
		MDNIE_SET(STANDARD_UI),
	}, {
		MDNIE_SET(STANDARD_VIDEO),
	},
	[CAMERA_MODE] = {
		MDNIE_SET(CAMERA),
	},
	[GALLERY_MODE] = {
		MDNIE_SET(STANDARD_GALLERY),
	}, {
		MDNIE_SET(STANDARD_VT),
	}, {
		MDNIE_SET(STANDARD_BROWSER),
	}, {
		MDNIE_SET(STANDARD_EBOOK),
	}, {
		MDNIE_SET(AUTO_EMAIL),
	},
};

#endif
