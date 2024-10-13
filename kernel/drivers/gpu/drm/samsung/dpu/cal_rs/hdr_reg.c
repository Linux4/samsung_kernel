// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <cal_config.h>
#include <hdr_cal.h>
#include <regs-hdr.h>
#include <dpp_cal.h>
#include <regs-dpp.h>

#define REGS_HDR_ID_MAX REGS_DPP_ID_MAX

static struct cal_regs_desc regs_hdr[HDR_HW_TYPE_MAX][REGS_HDR_ID_MAX];

#define hdr_regs_desc(id, hw_type)			(&regs_hdr[hw_type][id])
#define hdr_read(id, hw_type, offset)	cal_read(hdr_regs_desc(id, hw_type), offset)
#define hdr_write(id, hw_type, offset, val)	cal_write(hdr_regs_desc(id, hw_type), offset, val)
#define hdr_read_mask(id, hw_type, offset, mask)		\
		cal_read_mask(hdr_regs_desc(id, hw_type), offset, mask)
#define hdr_write_mask(id, hw_type, offset, val, mask)	\
		cal_write_mask(hdr_regs_desc(id, hw_type), offset, val, mask)
#define hdr_write_relaxed(id, hw_type, offset, val)		\
		cal_write_relaxed(hdr_regs_desc(id, hw_type), offset, val)

/* HDR DUMP */
void __hdr_dump(u32 id, struct hdr_regs	*base_regs)
{
	void __iomem *hdr_base_regs = base_regs->regs;

	cal_log_info(id, "=== HDR_LSI SFR DUMP ===\n");
	dpu_print_hex_dump(hdr_base_regs, hdr_base_regs + 0x0000, 0x470);
}

/* HDR VERIFY */
static bool hdr_reg_verify_v1p2(u32 id, u32 start_offset, u32 length)
{
	switch (start_offset) {
	case HDR_LSI_L_COM_CTRL:
		if (length != 1 && length != 2)
			goto error;
		break;
	case HDR_LSI_L_MOD_CTRL:
		if (length != 1)
			goto error;
		break;
	case HDR_LSI_L_OETF_POSX(0):
		if (length != HDR_OETF_L_POSX_LUT_REG_CNT)
			goto error;
		break;
	case HDR_LSI_L_OETF_POSY(0):
		if (length != HDR_OETF_L_POSY_LUT_REG_CNT)
			goto error;
		break;
	case HDR_LSI_L_EOTF_POSX(0):
		if (length != HDR_EOTF_L_POSX_LUT_REG_CNT)
			goto error;
		break;
	case HDR_LSI_L_EOTF_POSY(0):
		if (length != HDR_EOTF_L_POSY_LUT_REG_CNT)
			goto error;
		break;
	case HDR_LSI_L_GM_COEF(0):
		if (length != HDR_GM_L_COEF_REG_CNT)
			goto error;
		break;
	case HDR_LSI_L_GM_OFFS(0):
		if (length != HDR_GM_L_OFFS_REG_CNT)
			goto error;
		break;
	default:
		goto error;
	}

	return true;
error:
	cal_log_err(id, "invalid params: off 0x%x len %d\n", start_offset, length);
	return false;
}

static bool hdr_reg_verify_v2p2(u32 id, u32 start_offset, u32 length)
{
	switch (start_offset) {
	case HDR_HDR_CON:
		if (length != 1)
			goto error;
		break;
	case HDR_EOTF_SCALER:
		if (length != 1)
			goto error;
		break;
	case HDR_EOTF_LUT_TS(0):
		if (length != HDR_EOTF_LUT_TS_REG_CNT)
			goto error;
		break;
	case HDR_EOTF_LUT_VS(0):
		if (length != HDR_EOTF_LUT_VS_REG_CNT)
			goto error;
		break;
	case HDR_GM_COEF(0):
		if (length != HDR_GM_COEF_REG_CNT)
			goto error;
		break;
	case HDR_GM_OFF(0):
		if (length != HDR_GM_OFF_REG_CNT)
			goto error;
		break;
	case HDR_TM_COEF(0):
		if (length != HDR_TM_COEF_REG_CNT)
			goto error;
		break;
	case HDR_TM_YMIX_TF:
	case HDR_TM_YMIX_VF:
	case HDR_TM_YMIX_SLOPE:
	case HDR_TM_YMIX_DV:
		if (length != 1)
			goto error;
		break;
	case HDR_TM_LUT_TS(0):
		if (length != HDR_TM_LUT_TS_REG_CNT)
			goto error;
		break;
	case HDR_TM_LUT_VS(0):
		if (length != HDR_TM_LUT_VS_REG_CNT)
			goto error;
		break;
	case HDR_OETF_LUT_TS(0):
		if (length != HDR_OETF_LUT_TS_REG_CNT)
			goto error;
		break;
	case HDR_OETF_LUT_VS(0):
		if (length != HDR_OETF_LUT_VS_REG_CNT)
			goto error;
		break;
	default:
		goto error;
	}

	return true;
error:
	cal_log_err(id, "invalid params: off 0x%x len %d\n", start_offset, length);
	return false;
}

bool hdr_reg_verify(u32 id, u32 hw_type, u32 start_offset, u32 length)
{
	if (hw_type == HDR_HW_TYPE_NONE)	// none
		goto error;

	if (hw_type == HDR_HW_TYPE_BASE) {	// S.LSI
		if (IS_HDR_LAYER(id))
			return hdr_reg_verify_v2p2(id, start_offset, length);
		else
			return hdr_reg_verify_v1p2(id, start_offset, length);
	}

	return true;
error:
	cal_log_err(id, "invalid params: type %d off 0x%x len %d\n", hw_type, start_offset, length);
	return false;
}

/* HDR LUT */
void hdr_reg_set_lut(u32 id, u32 hw_type, u32 offset, const u32 lut)
{
	if (!hdr_reg_check_hw_type(id, hw_type))
		return;
	hdr_write_relaxed(id, hw_type, offset, lut);
}

void hdr_reg_set_multiple_lut(u32 id, u32 hw_type, u32 offset,
					u32 length, const u32 *lut)
{
	int i;

	if (!lut)
		return;
	for (i = 0; i < length; i++)
		hdr_reg_set_lut(id, hw_type, offset+(i*4), lut[i]);
}

void hdr_regs_desc_init(u32 id, void __iomem *regs, const char *name,
		u32 hw_type)
{
	regs_hdr[hw_type][id].regs = regs;
	regs_hdr[hw_type][id].name = name;
}

bool hdr_reg_check_hw_type(u32 id, u32 hw_type)
{
	if (regs_hdr[hw_type][id].regs == NULL)
		return false;
	return true;
}
