/* linux/arch/arm/mach-exynos/include/mach/asv-exynos3475_cal.c
*
* Copyright (c) 2015 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS3475 - Adaptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
#include <linux/printk.h>
#include <mach/asv-exynos_cal.h>
#include <mach/asv-exynos3475.h>

#if 0
__forceinline u32 Stop(void) { while(1); return 1; }
#define Assert(b) (!(b) ? cprintf("\n %s(line %d)\n", __FILE__, __LINE__), Stop() : 0)
#endif
#define unsign2sign(a) (((a)>=8) ? ((a)-16) : (a))

inline void SetBits (void __iomem *uAddr, u32 uBaseBit, u32 uMaskValue, u32 uSetValue)
{
	u32 val;

	val = __raw_readl(uAddr);
	val &= ~((uMaskValue)<<(uBaseBit));
	val |= (((uMaskValue)&(uSetValue))<<(uBaseBit));
	 __raw_writel (val, uAddr);
}

u32 re_err(void)
{
	    pr_err("ASV: CAL is working wrong. \n");
		    return 0;
}

u32 cal_get_freq(u32 id, s32 level)
{
	u32 freq = 0;
	switch(id)
	{
	case SYSC_DVFS_CL0 :
		freq = volt_table_cpu[level][0];
		break;
	case SYSC_DVFS_G3D :
		freq = volt_table_g3d[level][0];
		break;
	case SYSC_DVFS_MIF :
		freq = volt_table_mif[level][0];
		break;
	case SYSC_DVFS_INT :
		freq = volt_table_int[level][0];
		break;
	default:
		Assert(0);
	}
	return freq;
}

u32 cal_get_asv_grp(u32 id, s32 level) 	
{
	s32 asv_group = 0;

	//Check ASV Method
	if(get_asv_method()==ASV_RUNTIME)
	{
		asv_group=0;
	}
	else
	{
		switch(id)
		{
			case SYSC_DVFS_CL0:
				asv_group = get_asv_cpu_group() + unsign2sign(get_asv_cpu_modify());
				break;
			case SYSC_DVFS_G3D:
				asv_group = get_asv_g3d_group() + unsign2sign(get_asv_g3d_modify());
				break;
			case SYSC_DVFS_MIF:
				asv_group = get_asv_mif_group() + unsign2sign(get_asv_mif_modify());
				break;
			case SYSC_DVFS_INT:
				asv_group = get_asv_int_group() + unsign2sign(get_asv_int_modify());
				break;
			default:
				Assert(0);
		}	
	}
	if (asv_group<0)
		asv_group = 0;
	else if (asv_group>15)
		asv_group = 15;
	return (u32)asv_group;
}


u32 cal_get_volt(u32 id, s32 level)
{
	u32 volt;
	u32 asvgrp;
	u32 minlvl = cal_get_min_lv(id);
	const u32 *p_table;
	u32 idx;
	Assert(level >= 0);
	Assert(id<SYSC_DVFS_NUM);

	if (level >= minlvl)
		level = minlvl;
	idx = level;

	if (cal_get_table_ver() <= 1) 
	{
		p_table = ((id == SYSC_DVFS_CL0) ? volt_table_cpu[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d[idx] : 
				(id == SYSC_DVFS_MIF) ? volt_table_mif[idx] : volt_table_int[idx]);
	}
	else
	{
		p_table = ((id == SYSC_DVFS_CL0) ? volt_table_cpu_V02[idx] :
				(id == SYSC_DVFS_G3D) ? volt_table_g3d_V02[idx] : 
				(id == SYSC_DVFS_MIF) ? volt_table_mif_V02[idx] : volt_table_int_V02[idx]);
	}
	asvgrp = cal_get_asv_grp(id, level);		

	volt = p_table[asvgrp+1]; 

	if (cal_get_ssa_volt(id) > volt)
		volt = cal_get_ssa_volt(id);

	volt += cal_get_boost_volt(id, level);

	return volt;
}

bool cal_use_dynimic_abb(u32 id)
{
	    return 0;
}

u32 cal_get_abb(u32 id, s32 level)
{
	return 0;
}

bool cal_get_use_abb(u32 id)
{
	return false;
}

void cal_set_abb(u32 id, u32 abb)
{
}

u32 cal_get_max_volt(u32 id)
{
	s32 volt = 0;
	switch (id) {
		case SYSC_DVFS_CL0:
			volt = CPU_MAX_VOLT;
			break;
		case SYSC_DVFS_G3D:
			volt = G3D_MAX_VOLT;
			break;
		case SYSC_DVFS_MIF:
			volt = MIF_MAX_VOLT;
			break;
		case SYSC_DVFS_INT:
			volt = INT_MAX_VOLT;
			break;
		default:
			Assert(0);
	}
	return volt;
}


s32 cal_get_max_lv(u32 id)
{
	s32 lvl = (id == SYSC_DVFS_CL0) ? 2 :
		(id == SYSC_DVFS_G3D) ? 0 : 0;
	return lvl;
}

s32 cal_get_min_lv(u32 id)
{
	s32 level = 0;
	switch (id) {
		case SYSC_DVFS_CL0:
			level = SYSC_DVFS_END_LVL_CPU;
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
		default:
			Assert(0);
	}
	return level;
}

u32 cal_get_table_ver(void)
{
	return get_asv_table_version();
}

u32 cal_calc_ssa_volt(u32 id, u32 ssavalue)
{
	u32 volt, ssa;
	//	Assert(ssavalue<=SSA_MAX_LIMIT);

	ssa = (ssavalue>SSA_MAX_LIMIT) ? SSA_MAX_LIMIT : ssavalue;
	volt = SSA_VOLT_BASE + ssa * SSA_VOLT_STEP;

	return volt;
}

u32 cal_calc_ssa_value(u32 id, u32 volt)
{
	s32 ssavalue = (volt-SSA_VOLT_BASE)/SSA_VOLT_STEP;

	if ( ssavalue < 0 )
		ssavalue = 0;
	if ( ssavalue > SSA_MAX_LIMIT )
		ssavalue = SSA_MAX_LIMIT;

	return ssavalue;
}

u32 cal_get_ssa_volt(u32 id)
{
	u32 ssavalue = 0;

	switch(id)
	{
		case SYSC_DVFS_CL0:
			ssavalue = get_asv_cpu_ssa();
			break;
		case SYSC_DVFS_G3D:
			ssavalue = get_asv_g3d_ssa();
			break;
		case SYSC_DVFS_MIF:
			ssavalue = get_asv_mif_ssa();
			break;
		case SYSC_DVFS_INT:
			ssavalue = get_asv_int_ssa();
			break;
		default:
			Assert(0);
	}

	return cal_calc_ssa_volt(id, ssavalue);
}

u32 cal_get_boost_volt(u32 id, s32 level)
{
	u32 volt = 0, boost = 0;

	switch(id)
	{
		case SYSC_DVFS_CL0:
			boost = (level <= 5) ? get_asv_cpu_boost0() :
				(level <= 8) ? get_asv_cpu_boost1() : 0;
			break;
		case SYSC_DVFS_G3D:
			boost = (level<=0) ? get_asv_g3d_boost0() :
				(level<=1) ? get_asv_g3d_boost1() : 0;
			break;
		case SYSC_DVFS_MIF:
			boost = (level<=2) ? get_asv_mif_boost0() :
				(level<=3) ? get_asv_mif_boost1() : 0;
			break;
		case SYSC_DVFS_INT:
			boost = (level<=0) ? get_asv_int_boost0() :
				(level<=1) ? get_asv_int_boost1() : 0;
			break;
		default:
			Assert(0);
	}		

	if  ( boost <= 2 )
		volt = 12500 * boost;
	else
		volt = 50000;

	return volt;
}

u32 cal_get_ro(u32 id)
{
	return get_asv_ro();
}

bool cal_use_dynimic_ema(u32 id)
{
	if ( (id == SYSC_DVFS_CL0) || (id == SYSC_DVFS_G3D) )
		return true;
	else
		return false;
}

u32 cal_get_ema(u32 id, s32 level)
{
	u32 grp, ema_value;

	grp = cal_get_asv_grp(id, level);

	switch (id)
	{
		case SYSC_DVFS_CL0:
			ema_value = ema_table_cpu[level][grp];
			break;
		case SYSC_DVFS_G3D:
			ema_value = ema_table_g3d[level][grp];
			break;
		default:
			ema_value = 3;
			break;
	}
	return ema_value;
}

u32 cal_set_ema(u32 id, u32 volt)
{
	u32 ema_val;

	if (!cal_use_dynimic_ema(id))
		return 0;

	/* the original cal code is 
	 * ema_val = (volt >= 1100000) ? 1 : (volt >= 1000000) ? 3 : 4;
	 */
	if (volt < 1000000) {
		ema_val = 4;
	} else if ((volt >= 1000000) && (volt < 1100000)) {
		ema_val = 3;
	} else {
		ema_val = 1;
	}

	switch (id)
	{
		case SYSC_DVFS_CL0:
			SetBits(EXYNOS3475_SYSREG_CPU_EMA_RF1_HD_CON_CPU, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_CPU_EMA_RF1_HS_CON_CPU, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_CPU_EMA_HD_CON, 8, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_CPU_EMA_CON, 8, 0x7, ema_val);
			break;

		case SYSC_DVFS_G3D:
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RA1_HD_CON, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RA1_HS_CON, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RA2_HD_CON, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RF1_HD_CON, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RF1_HS_CON, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RF2_HD_CON, 0, 0x7, ema_val);
			SetBits(EXYNOS3475_SYSREG_G3D_EMA_RF2_HS_CON, 0, 0x7, ema_val);

			/* MCS_RD0, MCS_WR0 : 1 @ 0.9V, 0 @ 1.0V/1.1V */
			if ( ema_val >= 4 )
			{
				SetBits(EXYNOS3475_SYSREG_G3D_EMA_UHD_CON, 0, 0x1, 1);
				SetBits(EXYNOS3475_SYSREG_G3D_EMA_UHD_CON, 2, 0x1, 1);
			}
			else
			{
				SetBits(EXYNOS3475_SYSREG_G3D_EMA_UHD_CON, 0, 0x1, 0);
				SetBits(EXYNOS3475_SYSREG_G3D_EMA_UHD_CON, 2, 0x1, 0);
			}
			break;

		default:
			break;
	}
	return 1;
}

