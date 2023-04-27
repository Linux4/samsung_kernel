/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS9SoC series HDR driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "mcd_cm_lut.h"
#include "mcd_cm_lut_tune.h"
#include "hdr_drv.h"
#include "hdr_reg.h"
#include "mcd_cm.h"

enum pq_index luminance2pqindex(unsigned int luminance)
{
	return	(luminance <   100) ? INDEX_PQ1000 :
			(luminance <=  200) ? INDEX_PQ0200 :
			(luminance <=  250) ? INDEX_PQ0250 :
			(luminance <=  300) ? INDEX_PQ0300 :
			(luminance <=  350) ? INDEX_PQ0350 :
			(luminance <=  400) ? INDEX_PQ0400 :
			(luminance <=  450) ? INDEX_PQ0450 :
			(luminance <=  500) ? INDEX_PQ0500 :
			(luminance <=  550) ? INDEX_PQ0550 :
			(luminance <=  600) ? INDEX_PQ0600 :
			(luminance <=  650) ? INDEX_PQ0650 :
			(luminance <=  700) ? INDEX_PQ0700 :
			(luminance <=  750) ? INDEX_PQ0750 :
			(luminance <=  800) ? INDEX_PQ0800 :
			(luminance <=  850) ? INDEX_PQ0850 :
			(luminance <=  900) ? INDEX_PQ0900 :
			(luminance <=  950) ? INDEX_PQ0950 :
			(luminance <= 1000) ? INDEX_PQ1000 :
			(luminance <= 2000) ? INDEX_PQ2000 :
			(luminance <= 3000) ? INDEX_PQ3000 :
			(luminance <= 4000) ? INDEX_PQ4000 : INDEX_PQ4000;
}
enum tn_index luminance2targetindex(unsigned int luminance)
{
	return	(luminance <=  200) ? INDEX_T0200 :
			(luminance <=  250) ? INDEX_T0250 :
			(luminance <=  300) ? INDEX_T0300 :
			(luminance <=  350) ? INDEX_T0350 :
			(luminance <=  400) ? INDEX_T0400 :
			(luminance <=  450) ? INDEX_T0450 :
			(luminance <=  500) ? INDEX_T0500 :
			(luminance <=  550) ? INDEX_T0550 :
			(luminance <=  600) ? INDEX_T0600 :
			(luminance <=  650) ? INDEX_T0650 :
			(luminance <=  700) ? INDEX_T0700 :
			(luminance <=  750) ? INDEX_T0750 :
			(luminance <=  800) ? INDEX_T0800 :
			(luminance <=  850) ? INDEX_T0850 :
			(luminance <=  900) ? INDEX_T0900 :
			(luminance <=  950) ? INDEX_T0950 :
			(luminance <= 1000) ? INDEX_T1000 : INDEX_T1000;
}

void get_tables_standard(struct cm_tables *tables, const unsigned int target_bit,
	const enum gamma_index src_gamma, const enum gamut_index src_gamut, const unsigned int src_max_luminance,
	const enum gamma_index dst_gamma, const enum gamut_index dst_gamut, const unsigned int dst_max_luminance)
{
	//-------------------------------------------------------------------------------------------------
	tables->eotf = NULL;
	tables->gm = NULL;
	tables->oetf = NULL;
	tables->sc = NULL;
	tables->tm_off = NULL;
	tables->tm_tune = NULL;
	tables->tm_gamut = NULL;
	tables->tm_curve = NULL;
	tables->tm_ext = NULL;
	tables->tm_dynamic = NULL;
	//-------------------------------------------------------------------------------------------------
	{
		const enum gamma_type_index i_type = (src_gamma == INDEX_GAMMA_ST2084) ? INDEX_TYPE_PQ :
											(src_gamma == INDEX_GAMMA_HLG) ? INDEX_TYPE_HLG : INDEX_TYPE_SDR;
		const enum gamma_type_index o_type = (dst_gamma == INDEX_GAMMA_ST2084) ? INDEX_TYPE_PQ :
											(dst_gamma == INDEX_GAMMA_HLG) ? INDEX_TYPE_HLG : INDEX_TYPE_SDR;
		const enum tn_index idx_td = (o_type == INDEX_TYPE_HLG) ? INDEX_THLG :
									(dst_max_luminance != 0) ? luminance2targetindex(dst_max_luminance) :
									(o_type == INDEX_TYPE_SDR) ? INDEX_T0250 : INDEX_T1000;
		unsigned int half_mode = 0;

		tables->gm = TABLE_GM[src_gamut][dst_gamut];
		if (((i_type != INDEX_TYPE_SDR) || (o_type != INDEX_TYPE_SDR)) && (tables->gm))
			half_mode = 1;

		if (i_type == INDEX_TYPE_PQ) {
			const enum pq_index idx_pq = luminance2pqindex(src_max_luminance);

			tables->sc = TABLE_SC_PQ[idx_td][half_mode];
			tables->tm_curve = TABLE_TM_CURVE_PQ[idx_td][idx_pq];
			if (tables->tm_curve)
				tables->tm_tune = TABLE_TM_TUNE_PQ[o_type][idx_pq];
		} else if (i_type == INDEX_TYPE_HLG) {
			if ((o_type != INDEX_TYPE_HLG) || tables->gm) {
				tables->sc = TABLE_SC_HLG[o_type][half_mode];
				tables->tm_curve = TABLE_TM_CURVE_HLG[idx_td];
				if (tables->tm_curve)
					tables->tm_tune = TABLE_TM_TUNE_HLG[o_type];
			}
		} else if (o_type != INDEX_TYPE_SDR) {
			tables->sc = TABLE_SC_SDR[o_type][half_mode];
			tables->tm_curve = TABLE_TM_CURVE_SDR[o_type];
			if (tables->tm_curve)
				tables->tm_tune = TABLE_TM_TUNE_SDR[o_type];
		}

		if (tables->tm_curve) {
			tables->tm_gamut = TABLE_TM_LUMINANCE[src_gamut];
			tables->tm_ext = TABLE_TM_EXT;
		}
		if (tables->sc) {
			if (tables->tm_curve == NULL) {
				tables->tm_off = TABLE_TM_OFF;
			}
		}
		if (tables->gm || tables->sc || tables->tm_curve || (src_gamma != dst_gamma)) {
			tables->eotf = (i_type == INDEX_TYPE_HLG) ? TABLE_EOTF_HLG[o_type] : TABLE_EOTF[src_gamma];
			if (o_type != INDEX_TYPE_PQ) {
				tables->oetf = (target_bit == 8) ? TABLE_OETF08[dst_gamma][half_mode] :
												TABLE_OETF10[dst_gamma][half_mode];
			} else {
				tables->oetf = (i_type == INDEX_TYPE_PQ) ? TABLE_OETF_PQ_TO_PQ[idx_td][half_mode] :
							(i_type == INDEX_TYPE_HLG) ? TABLE_OETF_HLG_TO_PQ[idx_td][half_mode] :
														TABLE_OETF_SDR_TO_PQ[idx_td][half_mode];
			}
		}
	}
}
void set_tables_OTT_HDR10(struct cm_tables *tables, const enum type_ott type, const unsigned int target_bit)
{
	if (type == OTT_CUSTOM4000) {
		tables->eotf = CUSTOM4000_TABLE_EOTF;
		tables->gm = CUSTOM4000_TABLE_GM;
		tables->oetf = (target_bit == 8) ? CUSTOM4000_TABLE_OETF08 : CUSTOM4000_TABLE_OETF10;
		tables->sc = CUSTOM4000_TABLE_SC;
		tables->tm_curve = CUSTOM4000_TABLE_TM_CURVE;
		tables->tm_gamut = CUSTOM4000_TABLE_TM_GAMUT;
		tables->tm_tune = CUSTOM4000_TABLE_TM_TUNE;
		tables->tm_ext = TABLE_TM_EXT;
		tables->tm_dynamic = NULL;
		tables->tm_off = NULL;
	} else if (type == OTT_CUSTOM1000) {
		tables->eotf = CUSTOM1000_TABLE_EOTF;
		tables->gm = CUSTOM1000_TABLE_GM;
		tables->oetf = (target_bit == 8) ? CUSTOM1000_TABLE_OETF08 : CUSTOM1000_TABLE_OETF10;
		tables->sc = CUSTOM1000_TABLE_SC;
		tables->tm_curve = CUSTOM1000_TABLE_TM_CURVE;
		tables->tm_gamut = CUSTOM1000_TABLE_TM_GAMUT;
		tables->tm_tune = CUSTOM1000_TABLE_TM_TUNE;
		tables->tm_ext = TABLE_TM_EXT;
		tables->tm_dynamic = NULL;
		tables->tm_off = NULL;
	}
}
void get_tables(struct cm_tables *tables, unsigned int *tm_dynamic, unsigned int ott_type,
	const enum gamma_index src_gamma, const enum gamut_index src_gamut, const unsigned int src_max_luminance,
	const enum gamma_index dst_gamma, const enum gamut_index dst_gamut, const unsigned int dst_max_luminance)
{
#ifdef TARGET_SDR_8BIT
	unsigned int targetBit = ((dst_gamma == INDEX_GAMMA_ST2084) || (dst_gamma == INDEX_GAMMA_HLG)) ? 10 : 8;
#else
	unsigned int targetBit = 10;
#endif
	get_tables_standard(tables, targetBit,
		src_gamma, src_gamut, src_max_luminance,
		dst_gamma, dst_gamut, dst_max_luminance);
	{
		const unsigned int is_hdr10 = ((src_gamma == INDEX_GAMMA_ST2084) && (src_gamut == INDEX_GAMUT_BT2020)) ? 1 : 0;
		//const enum gamma_type_index i_type = (src_gamma == INDEX_GAMMA_ST2084) ? INDEX_TYPE_PQ  :
		//                                     (src_gamma == INDEX_GAMMA_HLG) ? INDEX_TYPE_HLG : INDEX_TYPE_SDR;
		const enum gamma_type_index o_type = (dst_gamma == INDEX_GAMMA_ST2084) ? INDEX_TYPE_PQ  :
											(dst_gamma == INDEX_GAMMA_HLG) ? INDEX_TYPE_HLG : INDEX_TYPE_SDR;

		if (is_hdr10 == 1) {
			if ((o_type == INDEX_TYPE_SDR) || ((o_type == INDEX_TYPE_PQ) && (dst_max_luminance > 0))) {
				if (tm_dynamic) {
					tables->tm_dynamic = tm_dynamic;
					tables->tm_curve = NULL;
					tables->tm_gamut = NULL;
					tables->tm_tune = NULL;
					tables->tm_ext = NULL;
					tables->tm_off = NULL;
				} else if ((dst_gamma == INDEX_GAMMA_GAMMA2_2) && (dst_gamut == INDEX_GAMUT_DCI_P3)) {
					set_tables_OTT_HDR10(tables, ott_type, targetBit);
				}
			}
		}
    }
}
void get_con(struct cm_tables *tables, const unsigned int isAlphaPremultiplied, const unsigned int applyDither)
{
	tables->con = 0;
	if (tables->gm)
		tables->con = tables->con | CON_SFR_GAMUT;
	if (tables->sc || tables->tm_curve || tables->tm_dynamic)
		tables->con = tables->con | CON_SFR_REMAP;
	if (tables->eotf)
		tables->con = tables->con | CON_SFR_EOTF;
	if (tables->oetf)
		tables->con = tables->con | CON_SFR_OETF;

	if ((tables->con > 0) && (isAlphaPremultiplied))
		tables->con = tables->con | CON_SFR_ALPHA;

	if (applyDither)
		tables->con = tables->con | CON_SFR_DITHER;

	if (tables->con > 0)
		tables->con = tables->con | CON_SFR_ALL;
}

void customize_dataspace(enum gamma_index *src_gamma, enum gamut_index *src_gamut,
						enum gamma_index *dst_gamma, enum gamut_index *dst_gamut,
						const unsigned int s_gamma, const unsigned int s_gamut,
						const unsigned int d_gamma, const unsigned int d_gamut)
{
	const unsigned int legacy_mode = ((d_gamma == INDEX_GAMMA_UNSPECIFIED) &&
									(d_gamut == INDEX_GAMUT_UNSPECIFIED)) ? 1 : 0;
    const enum gamma_type_index i_type = (s_gamma == INDEX_GAMMA_ST2084) ? INDEX_TYPE_PQ :
										(s_gamma == INDEX_GAMMA_HLG) ? INDEX_TYPE_HLG : INDEX_TYPE_SDR;
	//-------------------------------------------------------------------------------------------------
	if (legacy_mode == 1) {
		*src_gamut = (i_type == INDEX_TYPE_SDR) ? INDEX_GAMUT_DCI_P3 : ((enum gamut_index)s_gamut);
		*src_gamma = (i_type == INDEX_TYPE_SDR) ? INDEX_GAMMA_SRGB : ((enum gamma_index)s_gamma);
		*dst_gamut = INDEX_GAMUT_DCI_P3;
		*dst_gamma = (i_type == INDEX_TYPE_SDR) ? INDEX_GAMMA_SRGB : INDEX_GAMMA_GAMMA2_2;
	} else {
		*src_gamut = (enum gamut_index)s_gamut;
		*dst_gamut = (enum gamut_index)d_gamut;
		*src_gamma = (enum gamma_index)s_gamma;
		*dst_gamma = (enum gamma_index)d_gamma;
#ifdef ASSUME_SRGB_IS_GAMMA22_FOR_HDR
		if ((i_type != INDEX_TYPE_SDR) && ((*dst_gamma) == INDEX_GAMMA_SRGB))
			*dst_gamma = INDEX_GAMMA_GAMMA2_2;
#endif
	}
}

void mcd_cm_sfr_bypass(struct mcd_hdr_device *hdr)
{
	const unsigned int sfr_con = 0;

	mcd_reg_write(hdr, DPP_MCD_CM_CON_ADDR, sfr_con);
}

void mcd_cm_sfr(struct mcd_hdr_device *hdr, struct mcd_cm_params_info *params)
{
	enum gamma_index src_gamma, dst_gamma;
	enum gamut_index src_gamut, dst_gamut;

	customize_dataspace(&src_gamma, &src_gamut, &dst_gamma, &dst_gamut,
		params->src_gamma, params->src_gamut, params->dst_gamma, params->dst_gamut);
    //-------------------------------------------------------------------------------------------------
	if ((src_gamma == INDEX_GAMMA_UNSPECIFIED) || (dst_gamma == INDEX_GAMMA_UNSPECIFIED) ||
		(src_gamut == INDEX_GAMUT_UNSPECIFIED) || (dst_gamut == INDEX_GAMUT_UNSPECIFIED)) {
		mcd_cm_sfr_bypass(hdr);
	} else {
		unsigned int *tm_dynamic = NULL;
		struct cm_tables tables;

		{
			const enum gamma_type_index o_type = (dst_gamma == INDEX_GAMMA_ST2084) ? INDEX_TYPE_PQ :
												(dst_gamma == INDEX_GAMMA_HLG) ? INDEX_TYPE_HLG :
												INDEX_TYPE_SDR;
			enum type_ott tott =
#ifdef TUNE_OTT
				(params->dst_max_luminance == DEVICE_LUMINANCE) ?
#if (TUNE_OTT & TUNE_OTT_CUSTOM4000)
				(params->src_max_luminance == 4000) ? OTT_CUSTOM4000 :
#endif
#if (TUNE_OTT & TUNE_OTT_CUSTOM1000)
				(params->src_max_luminance == 1000) ? OTT_CUSTOM1000 :
#endif
				OTT_NONE :
#endif
				OTT_NONE;

			//-------------------------------------------------------------------------------------------------
			const unsigned int valid_hdr10 = ((src_gamma == INDEX_GAMMA_ST2084) && (src_gamut == INDEX_GAMUT_BT2020)) ? 1 : 0;
			const unsigned int valid_hdr10p = (valid_hdr10 && (params->hdr10p_lut != NULL)) ? (((*params->hdr10p_lut) == 1) ? 1 : 0) : 0;
			const unsigned int dither_allow = ((valid_hdr10p == 0) && (valid_hdr10) && (params->src_max_luminance > 3000)) ? 1 : 0;
			const unsigned int applyDither = (params->needDither && (o_type == INDEX_TYPE_SDR) && dither_allow) ? 1 : 0;
			//-------------------------------------------------------------------------------------------------
			unsigned int applied_max_luminance = params->dst_max_luminance;

			if ((hdr->id == MCD_L5) && (valid_hdr10p == 1)) {
				tm_dynamic = params->hdr10p_lut + 2;
				applied_max_luminance = *(params->hdr10p_lut + 1);
			}
			get_tables(&tables, tm_dynamic, tott,
				src_gamma, src_gamut, params->src_max_luminance,
				dst_gamma, dst_gamut, applied_max_luminance);
			get_con(&tables, params->isAlphaPremultiplied, applyDither);
		}

		if (hdr->id != MCD_L5) {
			unsigned int set_bypass = 0;

			tables.tm_ext = NULL;
			if (tables.tm_dynamic)
				set_bypass = 1;
			if ((hdr->id == MCD_L0) || (hdr->id == MCD_L1)) {
				tables.tm_off = NULL;
				if (tables.tm_curve || tables.tm_tune || tables.tm_gamut)
					set_bypass = 1;
			}
			if (set_bypass == 1) {
				mcd_cm_sfr_bypass(hdr);
				return;
			}
		}
		// sfr write
		mcd_reg_write(hdr, DPP_MCD_CM_CON_ADDR, tables.con);
		if (tables.eotf)
			mcd_reg_writes(hdr, DPP_MCD_CM_EOTF_ADDR(0), tables.eotf, DPP_MCD_CM_EOTF_SZ);
		if (tables.oetf)
			mcd_reg_writes(hdr, DPP_MCD_CM_OETF_ADDR(0), tables.oetf, DPP_MCD_CM_OETF_SZ);
		if (tables.gm)
			mcd_reg_writes(hdr, DPP_MCD_CM_GM_ADDR(0), tables.gm, DPP_MCD_CM_GM_SZ);
		if (tables.sc)
			mcd_reg_writes(hdr, DPP_MCD_CM_SC_ADDR(0), tables.sc, DPP_MCD_CM_SC_SZ);

		if (hdr->id == MCD_L5) {
			if (tables.tm_ext)
				mcd_reg_writes(hdr, DPP_MCD_CM_TM_EXT_ADDR(0), tables.tm_ext, DPP_MCD_CM_TM_EXT_SZ);
			if (tables.tm_dynamic)
				mcd_reg_writes(hdr, DPP_MCD_CM_TM_DYNAMIC_ADDR(0), tables.tm_dynamic, DPP_MCD_CM_TM_DYNAMIC_SZ);
		}
		if (!((hdr->id == MCD_L0) || (hdr->id == MCD_L1))) {
			if (tables.tm_off)
				mcd_reg_writes(hdr, DPP_MCD_CM_TM_OFF_ADDR(0), tables.tm_off, DPP_MCD_CM_TM_OFF_SZ);
			if (tables.tm_tune)
				mcd_reg_writes(hdr, DPP_MCD_CM_TM_TUNE_ADDR(0), tables.tm_tune, DPP_MCD_CM_TM_TUNE_SZ);
			if (tables.tm_gamut)
				mcd_reg_writes(hdr, DPP_MCD_CM_TM_GAMUT_ADDR(0), tables.tm_gamut, DPP_MCD_CM_TM_GAMUT_SZ);
			if (tables.tm_curve)
				mcd_reg_writes(hdr, DPP_MCD_CM_TM_CURVE_ADDR(0), tables.tm_curve, DPP_MCD_CM_TM_CURVE_SZ);
		}
	}
}

void mcd_cm_reg_set_params(struct mcd_hdr_device *hdr, struct mcd_cm_params_info *params)
{
	unsigned int set_bypass = 0;

	if (hdr->id == MCD_L0 || hdr->id == MCD_L1) {
	    int src_pq = (params->src_gamma == INDEX_GAMMA_ST2084) ? 1 : 0;
	    int src_hlg = (params->src_gamma == INDEX_GAMMA_HLG) ? 1 : 0;
	    int src_sdr = ((src_pq == 0) && (src_hlg == 0)) ? 1 : 0;
	    int dst_pq = (params->dst_gamma == INDEX_GAMMA_ST2084) ? 1 : 0;
	    int dst_hlg = (params->dst_gamma == INDEX_GAMMA_HLG) ? 1 : 0;
	    int dst_sdr = ((dst_pq == 0) && (dst_hlg == 0)) ? 1 : 0;

		int allow_case0 = ((src_sdr == 1) && (dst_sdr == 1)) ? 1 : 0;
		int allow_case1 = ((src_sdr == 1) && (dst_pq == 1)) ? 1 : 0;
		int allow_case2 = ((src_hlg == 1) && (dst_hlg == 1)) ? 1 : 0;
		int allow_case = ((allow_case0 == 1) || (allow_case1 == 1) || (allow_case2 == 1)) ? 1 : 0;

		if (allow_case == 0) {
		    hdr_err("HDR:ERR:%sunsupported gamma type!!\n", __func__);
		    set_bypass = 1;
		}
	}

	if (set_bypass)
		mcd_cm_sfr_bypass(hdr);
	else
		mcd_cm_sfr(hdr, params);
}


int mcd_cm_reg_reset(struct mcd_hdr_device *hdr)
{
	int ret = 0;

	mcd_reg_write(hdr, DPP_MCD_CM_CON_ADDR, 0);

	return ret;
}


int mcd_cm_reg_dump(struct mcd_hdr_device *hdr)
{
	int ret = 0;

	hdr_info("\n==== MCD IP SFR : %d ===\n", hdr->id);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			hdr->regs, 0x40, false);
#if 0
	hdr_info("\n==== MCD IP SHD SFR : %d ===\n", hdr->id);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			hdr->regs + DPP_MCD_CM_CON_ADDR_SHD, 0x40, false);
#endif
	return ret;
}


