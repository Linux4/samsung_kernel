/* linux/drivers/video/exynos/decon/dpu_reg_7420.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Haowei Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "dpu_common.h"
#include "regs-dpu.h"

/* gamma LUT */

#define SAVE_ITEM(x)    { .addr = (x), .val = 0x0, }

static struct dpu_reg_dump dpu_dump[] = {
	SAVE_ITEM(DPUCON),
	SAVE_ITEM(DPUSCR_RED),
	SAVE_ITEM(DPUSCR_GREEN),
	SAVE_ITEM(DPUSCR_BLUE),
	SAVE_ITEM(DPUSCR_CYAN),
	SAVE_ITEM(DPUSCR_MAGENTA),
	SAVE_ITEM(DPUSCR_YELLOW),
	SAVE_ITEM(DPUSCR_WHITE),
	SAVE_ITEM(DPUSCR_BLACK),
	SAVE_ITEM(DPUHSC_CONTROL),
	SAVE_ITEM(DPUHSC_PAIMGAIN2_1_0),
	SAVE_ITEM(DPUHSC_PAIMGAIN5_4_3),
	SAVE_ITEM(DPUHSC_PAIMSCALE_SHIFT),
	SAVE_ITEM(DPUHSC_PPHCGAIN2_1_0),
	SAVE_ITEM(DPUHSC_PPHCGAIN5_4_3),
	SAVE_ITEM(DPUHSC_TSCGAIN_YRATIO),
};

struct dpu_reg_dump dpu_gamma_dump[DPUGAMMALUT_MAX];

const u32 gamma_table1[][65] = {
	{   0,  3,    6,   9,  12,  16,  19,  22,  25,  29,
	   32,  35,  39,  43,  46,  50,  54,  58,  62,  66,
	   71,  75,  80,  84,  89,  94,  99, 103, 108, 113,
	  118, 123, 128, 133, 138, 143, 148, 153, 158, 162,
	  167, 172, 176, 181, 185, 190, 194, 198, 202, 206,
	  210, 213, 217, 221, 224, 227, 231, 234, 237, 240,
	  243, 247, 250, 253, 256 },
	{   0,   3,   6,   9,  12,  16,  19,  22,  25,  29,
	   32,  35,  39,  43,  46,  50,  54,  58,  62,  66,
	   71,  75,  80,  84,  89,  94,  99, 103, 108, 113,
	  118, 123, 128, 133, 138, 143, 148, 153, 158, 162,
	  167, 172, 176, 181, 185, 190, 194, 198, 202, 206,
	  210, 213, 217, 221, 224, 227, 231, 234, 237, 240,
	  243, 247, 250, 253, 256 },
	{   0,   3,   6,   9,  12,  16,  19,  22,  25,  29,
	   32,  35,  39,  43,  46,  50,  54,  58,  62,  66,
	   71,  75,  80,  84,  89,  94,  99, 103, 108, 113,
	  118, 123, 128, 133, 138, 143, 148, 153, 158, 162,
	  167, 172, 176, 181, 185, 190, 194, 198, 202, 206,
	  210, 213, 217, 221, 224, 227, 231, 234, 237, 240,
	  243, 247, 250, 253, 256 },
};

const u32 gamma_table2[][65] = {
	{   0,   4,   9,  13,  18,  22,  27,  31,  36,  40,
	   45,  49,  54,  58,  63,  67,  72,  76,  81,  85,
	   90,  94,  99, 103, 108, 112, 117, 121, 126, 130,
	  135, 139, 144, 148, 153, 158, 163, 168, 173, 178,
	  183, 188, 193, 198, 203, 208, 212, 216, 220, 224,
	  227, 230, 233, 236, 239, 241, 243, 245, 247, 249,
	  251, 252, 253, 254, 256 },
	{   0,   4,   9,  13,  18,  22,  27,  31,  36,  40,
	   45,  49,  54,  58,  63,  67,  72,  76,  81,  85,
	   90,  94,  99, 103, 108, 112, 117, 121, 126, 130,
	  135, 139, 144, 148, 153, 158, 163, 168, 173, 178,
	  183, 188, 193, 198, 203, 208, 212, 216, 220, 224,
	  227, 230, 233, 236, 239, 241, 243, 245, 247, 249,
	  251, 252, 253, 254, 256 },
	{   0,   4,   9,  13,  18,  22,  27,  31,  36,  40,
	   45,  49,  54,  58,  63,  67,  72,  76,  81,  85,
	   90,  94,  99, 103, 108, 112, 117, 121, 126, 130,
	  135, 139, 144, 148, 153, 158, 163, 168, 173, 178,
	  183, 188, 193, 198, 203, 208, 212, 216, 220, 224,
	  227, 230, 233, 236, 239, 241, 243, 245, 247, 249,
	  251, 252, 253, 254, 256 },
};

const u32 gamma_table3[][65] = {
	{   0,   2,   3,   5,   6,   8,  10,  12,  14,  16,
	   18,  21,  24,  27,  30,  33,  37,  41,  46,  50,
	   55,  61,  66,  72,  78,  84,  91,  98, 104, 111,
	  118, 125, 132, 139, 146, 152, 159, 166, 172, 178,
	  184, 190, 196, 201, 206, 211, 215, 219, 222, 226,
	  229, 232, 234, 237, 239, 241, 243, 245, 247, 248,
	  250, 251, 253, 254, 255 },
	{   0,   2,   3,   5,   6,   8,  10,  12,  14,  16,
	   18,  21,  24,  27,  30,  33,  37,  41,  46,  50,
	   55,  61,  66,  72,  78,  84,  91,  98, 104, 111,
	  118, 125, 132, 139, 146, 152, 159, 166, 172, 178,
	  184, 190, 196, 201, 206, 211, 215, 219, 222, 226,
	  229, 232, 234, 237, 239, 241, 243, 245, 247, 248,
	  250, 251, 253, 254, 255 },
	{   0,   2,   3,   5,   6,   8,  10,  12,  14,  16,
	   18,  21,  24,  27,  30,  33,  37,  41,  46,  50,
	   55,  61,  66,  72,  78,  84,  91,  98, 104, 111,
	  118, 125, 132, 139, 146, 152, 159, 166, 172, 178,
	  184, 190, 196, 201, 206, 211, 215, 219, 222, 226,
	  229, 232, 234, 237, 239, 241, 243, 245, 247, 248,
	  250, 251, 253, 254, 255 },
};

void dpu_reg_update_mask(u32 mask)
{
	dpu_write_mask(DPU_MASK_CTRL, mask, DPU_MASK_CTRL_MASK);
}

void dpu_reg_en_dither(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_DITHER_ON, DPU_DITHER_ON_MASK);
}

void dpu_reg_en_scr(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_SCR_ON, DPU_SCR_ON_MASK);
}

void dpu_reg_en_gamma(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_GAMMA_ON, DPU_GAMMA_ON_MASK);
}

void dpu_reg_en_hsc(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_HSC_ON, DPU_HSC_ON_MASK);
}

void dpu_reg_set_image_size(u32 width, u32 height)
{
	u32 data;

	data = (height << 16) | (width);
	dpu_write(DPUIMG_SIZESET, data);
}

void dpu_reg_module_on_off(bool en1, bool en2, bool en3, bool en4, bool en5)
{
	dpu_reg_update_mask(1);

	dpu_reg_en_dither(en1);
	dpu_reg_en_scr(en2);
	dpu_reg_en_gamma(en3);
	dpu_reg_en_hsc(en4);

	dpu_reg_update_mask(0);
}

void dpu_reg_set_gamma_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_gamma(val);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_gamma(u32 offset, u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask((DPUGAMMALUT_X_Y_BASE + offset), val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_hsc(val);
	dpu_write_mask(DPUHSC_CONTROL, (val << 3), TSC_ON);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_tscgain(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_TSCGAIN_YRATIO, val, TSC_GAIN);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_rgb(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PAIMGAIN2_1_0, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_cmy(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PAIMGAIN5_4_3, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_shift(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PAIMSCALE_SHIFT, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_scr(val);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_r(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_RED, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_g(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_GREEN, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_b(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_BLUE, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_c(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_CYAN, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_m(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_MAGENTA, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_y(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_YELLOW , val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_w(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_WHITE , val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_k(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUSCR_BLACK, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_hue_rgb(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PPHCGAIN2_1_0, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_hue_cmy(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PPHCGAIN5_4_3, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_hue_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_hsc(val);
	dpu_write_mask(DPUHSC_CONTROL, val << 1, PPHC_ON);
	dpu_reg_update_mask(0);
}

void dpu_reg_gamma_init(void)
{
	int i;

	for (i = 0; i < DPUGAMMALUT_MAX; i++) {
		dpu_gamma_dump[i].addr = DPUGAMMALUT_X_Y_BASE + (i*4);
		dpu_gamma_dump[i].val = 0;
	}
}

void dpu_reg_save(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpu_dump); i++) {
		dpu_dump[i].val = dpu_read(dpu_dump[i].addr);
	}
	for (i = 0; i < ARRAY_SIZE(dpu_gamma_dump); i++) {
		dpu_gamma_dump[i].val = dpu_read(dpu_gamma_dump[i].addr);
	}
}

/* Restore registers after hibernation */
void dpu_reg_restore(void)
{
	int i;

	dpu_write_mask(DPU_MASK_CTRL, ~0, DPU_MASK_CTRL_MASK);
	for (i = 0; i < ARRAY_SIZE(dpu_dump); i++) {
		dpu_write(dpu_dump[i].addr, dpu_dump[i].val);
	}
	for (i = 0; i < ARRAY_SIZE(dpu_gamma_dump); i++) {
		dpu_write(dpu_gamma_dump[i].addr, dpu_gamma_dump[i].val);
	}
	dpu_write_mask(DPU_MASK_CTRL, 0, DPU_MASK_CTRL_MASK);
}

void dpu_reg_start(u32 w, u32 h)
{
	u32 id = 0;

	decon_reg_enable_apb_clk(id, 1);

	dpu_reg_module_on_off(0, 0, 0, 0, 0);

	dpu_reg_set_image_size(w, h);
	decon_reg_set_pixel_count_se(id, w, h);
	decon_reg_set_image_size_se(id, w, h);
	decon_reg_set_porch_se(id, DPU_VFP, DPU_VSA, DPU_VBP,
				DPU_HFP, DPU_HSA, DPU_HBP);

	decon_reg_enable_dpu(id, 1);
}

void dpu_reg_stop(void)
{
	u32 id = 0;

	decon_reg_enable_dpu(id, 0);
	decon_reg_enable_apb_clk(id, 0);
}

