/* linux/arch/arm64/mach-exynos/asv-exynos7580_cal.c
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS7580 - Adoptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <mach/asv-exynos_cal.h>
#include <mach/asv-exynos7580.h>
#include <linux/types.h>
#include <plat/cpu.h>

static ASV_BANK0 asv_bank0;
static ASV_BANK2 asv_bank2;

u32 (*volt_table_arm)[MAX_ASV_GROUP + 1];
u32 (*volt_table_g3d)[MAX_ASV_GROUP + 1];
u32 (*volt_table_mif)[MAX_ASV_GROUP + 1];
u32 (*volt_table_int)[MAX_ASV_GROUP + 1];
u32 (*volt_table_isp)[MAX_ASV_GROUP + 1];
u32 (*abb_table_arm)[MAX_ASV_GROUP + 1];
u32 (*abb_table_mif)[MAX_ASV_GROUP + 1];
u32 (*abb_table_int)[MAX_ASV_GROUP + 1];
u32 (*abb_table_isp)[MAX_ASV_GROUP + 1];

static volatile void __iomem *abb_base_arr[SYSC_DVFS_NUM] = {
	[SYSC_DVFS_CPU] =	CHIPID_ABBG_BASE + 0x0780,
	[SYSC_DVFS_APL] =	CHIPID_ABBG_BASE + 0x0788,
	[SYSC_DVFS_MIF] =	CHIPID_ABBG_BASE + 0x0784,
	[SYSC_DVFS_INT] =	CHIPID_ABBG_BASE + 0x0784,
	[SYSC_DVFS_ISP] =	CHIPID_ABBG_BASE + 0x0784,
};

u32 re_err(void)
{
	dump_stack();
	pr_err("ASV: CAL is working wrong. \n");

	return 0;
}

void cal_init(void)
{
	u32 i;

	for (i = 0; i < 4; i++) {
		((u32*)(&asv_bank0))[i] = ((u32*)(CHIPID_ASV_TBL_BASE))[i];
		((u32*)(&asv_bank2))[i] = ((u32*)(CHIPID_ASV_TBL_BASE + 0x0020))[i];
	}

	switch (cal_get_table_ver()) {
	case 0:	/* 1st LOT 2nd LOT EVT0.0 */ /* fall through */
	case 1: /* EVT0.1 CS ASV Version 0.61, 0.81 */
		volt_table_arm =	(u32(*)[MAX_ASV_GROUP+1])volt_table_arm_1;
		volt_table_g3d =	(u32(*)[MAX_ASV_GROUP+1])volt_table_g3d_1;
		volt_table_mif =	(u32(*)[MAX_ASV_GROUP+1])volt_table_mif_1;
		volt_table_int =	(u32(*)[MAX_ASV_GROUP+1])volt_table_int_1;
		volt_table_isp =	(u32(*)[MAX_ASV_GROUP+1])volt_table_isp_1;
		abb_table_arm =		(u32(*)[MAX_ASV_GROUP+1])abb_table_arm_1;
		abb_table_mif =		(u32(*)[MAX_ASV_GROUP+1])abb_table_mif_1;
		abb_table_int =		(u32(*)[MAX_ASV_GROUP+1])abb_table_int_1;
		abb_table_isp =		(u32(*)[MAX_ASV_GROUP+1])abb_table_isp_1;
		break;
	case 2: /* ASV Version 1.0 */
		volt_table_arm =	(u32(*)[MAX_ASV_GROUP+1])volt_table_arm_2;
		volt_table_g3d =	(u32(*)[MAX_ASV_GROUP+1])volt_table_g3d_2;
		volt_table_mif =	(u32(*)[MAX_ASV_GROUP+1])volt_table_mif_2;
		volt_table_int =	(u32(*)[MAX_ASV_GROUP+1])volt_table_int_2;
		volt_table_isp =	(u32(*)[MAX_ASV_GROUP+1])volt_table_isp_2;
		abb_table_arm =		(u32(*)[MAX_ASV_GROUP+1])abb_table_arm_2;
		abb_table_mif =		(u32(*)[MAX_ASV_GROUP+1])abb_table_mif_2;
		abb_table_int =		(u32(*)[MAX_ASV_GROUP+1])abb_table_int_2;
		abb_table_isp =		(u32(*)[MAX_ASV_GROUP+1])abb_table_isp_2;
		break;
	case 3: /* ASV Version 1.1 */
		volt_table_arm =	(u32(*)[MAX_ASV_GROUP+1])volt_table_arm_3;
		volt_table_g3d =	(u32(*)[MAX_ASV_GROUP+1])volt_table_g3d_3;
		volt_table_mif =	(u32(*)[MAX_ASV_GROUP+1])volt_table_mif_3;
		volt_table_int =	(u32(*)[MAX_ASV_GROUP+1])volt_table_int_3;
		volt_table_isp =	(u32(*)[MAX_ASV_GROUP+1])volt_table_isp_3;
		abb_table_arm =		(u32(*)[MAX_ASV_GROUP+1])abb_table_arm_3;
		abb_table_mif =		(u32(*)[MAX_ASV_GROUP+1])abb_table_mif_3;
		abb_table_int =		(u32(*)[MAX_ASV_GROUP+1])abb_table_int_3;
		abb_table_isp =		(u32(*)[MAX_ASV_GROUP+1])abb_table_isp_3;
		break;
	default:
		volt_table_arm =	(u32(*)[MAX_ASV_GROUP+1])volt_table_arm_3;
		volt_table_g3d =	(u32(*)[MAX_ASV_GROUP+1])volt_table_g3d_3;
		volt_table_mif =	(u32(*)[MAX_ASV_GROUP+1])volt_table_mif_3;
		volt_table_int =	(u32(*)[MAX_ASV_GROUP+1])volt_table_int_3;
		volt_table_isp =	(u32(*)[MAX_ASV_GROUP+1])volt_table_isp_3;
		abb_table_arm =		(u32(*)[MAX_ASV_GROUP+1])abb_table_arm_3;
		abb_table_mif =		(u32(*)[MAX_ASV_GROUP+1])abb_table_mif_3;
		abb_table_int =		(u32(*)[MAX_ASV_GROUP+1])abb_table_int_3;
		abb_table_isp =		(u32(*)[MAX_ASV_GROUP+1])abb_table_isp_3;
		break;
	}
}

u32 cal_get_freq(u32 id, s32 level)
{
	u32 freq = 0;

	switch (id) {
	case SYSC_DVFS_CPU:
		freq = volt_table_arm[level][0];
		break;
	case SYSC_DVFS_APL:
		freq = volt_table_arm[level][0];
		break;
	case SYSC_DVFS_G3D:
		freq = volt_table_g3d[level][0];
		break;
	case SYSC_DVFS_MIF:
		freq = volt_table_mif[level][0];
		break;
	case SYSC_DVFS_INT:
		freq = volt_table_int[level][0];
		break;
	case SYSC_DVFS_ISP:
		freq = volt_table_isp[level][0];
		break;
	default:
		Assert(0);
	}

	return freq;
}

u32 cal_get_asv_grp(u32 id, s32 level)
{
	u32 asv_group;

	switch (id) {
	case SYSC_DVFS_CPU:
		asv_group = asv_bank0.apl0_grp;
		break;
	case SYSC_DVFS_APL:
		asv_group = asv_bank0.apl1_grp;
		break;
	case SYSC_DVFS_G3D:
		asv_group = asv_bank0.g3d_grp;
		break;
	case SYSC_DVFS_MIF: /* fall through */
	case SYSC_DVFS_INT: /* fall through */
	case SYSC_DVFS_ISP:
		asv_group = asv_bank0.mif_int_grp;
		break;
	}

	return asv_group;
}

u32 cal_check_ssa(u32 id, u32 uvol)
{
	u32 ssa_vol = 0;
	u32 ssa0;

	switch (id) {
	case SYSC_DVFS_CPU:
		ssa0 = asv_bank0.apl0_ssa;
		ssa_vol = (ssa0 == 1)?800000:(ssa0 == 2)?825000:(ssa0 == 3)?850000:(ssa0 == 4)?875000:(ssa0 == 5)?900000:0;
		break;
	case SYSC_DVFS_APL:
		ssa0 = asv_bank0.apl1_ssa;
		ssa_vol = (ssa0 == 1)?800000:(ssa0 == 2)?825000:(ssa0 == 3)?850000:(ssa0 == 4)?875000:(ssa0 == 5)?900000:0;
		break;
	case SYSC_DVFS_G3D:
		ssa0 = asv_bank0.g3d_ssa;
		ssa_vol = (ssa0 == 1)?800000:(ssa0 == 2)?825000:(ssa0 == 3)?850000:0;
		break;
	case SYSC_DVFS_MIF: /* fall through */
	case SYSC_DVFS_INT: /* fall through */
	case SYSC_DVFS_ISP:
		ssa0 = asv_bank0.mif_int_ssa;
		ssa_vol = (ssa0 == 1)?800000:(ssa0 == 2)?825000:(ssa0 == 3)?850000:(ssa0 == 4)?875000:(ssa0 == 5)?900000:0;
		break;
	}

	if (uvol < ssa_vol)
		return ssa_vol;
	else
		return uvol;
}

u32 cal_get_volt(u32 id, s32 level)
{
	u32 volt, asvgrp, idx;
	u32 minlvl = cal_get_min_lv(id);
	const u32 *p_table;

	Assert(level >= 0);

	if (samsung_rev() == EXYNOS7580_REV_0)
		return 1200000;

	if (level >= minlvl)
		level = minlvl;
	idx = level;

	p_table = ((id == SYSC_DVFS_CPU) ? volt_table_arm[idx]:
			(id == SYSC_DVFS_APL) ? volt_table_arm[idx]:
			(id == SYSC_DVFS_G3D) ? volt_table_g3d[idx]:
			(id == SYSC_DVFS_MIF) ? volt_table_mif[idx]:
			(id == SYSC_DVFS_INT) ? volt_table_int[idx]:
			(id == SYSC_DVFS_ISP) ? volt_table_isp[idx]:
			NULL);

	Assert(p_table != NULL);

	if (p_table == NULL)
		return 0;

	asvgrp = cal_get_asv_grp(id, level);

	volt = p_table[asvgrp + 1];
	volt = cal_check_ssa(id, volt);

	return volt;

}

u32 cal_get_abb(u32 id, s32 level)
{
	u32 match_abb, asv_grp, idx;
	u32 min_lvl = cal_get_min_lv(id);
	const u32 *p_table;

	Assert(level >= 0);

	if (level >= min_lvl)
		level = min_lvl;
	idx = level;

	p_table = ((id == SYSC_DVFS_CPU) ? abb_table_arm[idx]:
			(id == SYSC_DVFS_APL) ? abb_table_arm[idx]:
			(id == SYSC_DVFS_MIF) ? abb_table_mif[idx]:
			(id == SYSC_DVFS_INT) ? abb_table_int[idx]:
			(id == SYSC_DVFS_ISP) ? abb_table_isp[idx]:
			NULL);

	if (p_table == NULL)
		return 0;

	asv_grp = cal_get_asv_grp(id, level);
	match_abb = p_table[asv_grp + 1];

	return match_abb;
}

void cal_set_abb(u32 id, u32 abb)
{
	u32 reg;

	Assert(id < SYSC_DVFS_NUM);

	reg = (1 << 31) | (1 << 7) | (abb & 0x1f);

	if (id != SYSC_DVFS_CPU && id != SYSC_DVFS_APL
	 && id != SYSC_DVFS_MIF && id != SYSC_DVFS_INT && id != SYSC_DVFS_ISP)
		return;

	if (abb == ABB_BYPASS) {
		SetBits(abb_base_arr[id], 7, 0x1, 0x0);
	} else {
		__raw_writel(reg, abb_base_arr[id]);
	}
}

s32 cal_get_min_lv(u32 id)
{
	s32 level = 0;

	switch (id) {
	case SYSC_DVFS_CPU:
		level = SYSC_DVFS_END_LVL_CPU;
		break;
	case SYSC_DVFS_APL:
		level = SYSC_DVFS_END_LVL_APL;
		break;
	case SYSC_DVFS_G3D:
		level = SYSC_DVFS_END_LVL_G3D;
		break;
	case SYSC_DVFS_MIF:
		level = SYSC_DVFS_END_LVL_MIF;
		break;
	case SYSC_DVFS_INT:
		level = SYSC_DVFS_END_LVL_INT;
		break;
	case SYSC_DVFS_ISP:
		level = SYSC_DVFS_END_LVL_ISP;
		break;
	default:
		Assert(0);
	}

	return level;
}

u32 cal_get_table_ver(void)
{
	return (asv_bank0.asv_tbl_ver_high << 2) | (asv_bank0.asv_tbl_ver_low);
}

u32 cal_get_max_volt(u32 id)
{
	return 0;
}

void cal_set_ema(u32 id, u32 setvolt)
{
}

u32 cal_get_hpm(void)
{
	return 0;
}

u32 cal_get_ids_hpm_group(void)
{
	return 0;
}

u32 cal_get_match_subgrp(u32 id, s32 level)
{
	return 0;
}

u32 cal_get_ids(void)
{
	return 0;
}

bool cal_use_dynimic_abb(u32 id)
{
	return true;
}

bool cal_is_fused_speed_grp(void)
{
	return true;
}

