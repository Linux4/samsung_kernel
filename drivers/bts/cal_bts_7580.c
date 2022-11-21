/* arch/arm/mach-exynos/cal_bts.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - BTS CAL code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cal_bts_7580.h"

void bts_setotf_sysreg(BWL_SYSREG_RT_NRT_SEL path_sel, addr_u32 base, bool enable)
{
	unsigned int tmp_reg;

	tmp_reg = Inp32(base + ISP_USER_CON);

	if (enable)
		Outp32(base + ISP_USER_CON, tmp_reg | (0x1<<path_sel));
	else
		Outp32(base + ISP_USER_CON, tmp_reg & ~(0x1<<path_sel));
}

void bts_setmo_sysreg(BWL_MO_SYSREG_IP mo_id, addr_u32 base, unsigned int ar,
			unsigned int aw)
{
	unsigned int tmp_reg;

	switch (mo_id) {
	case BTS_DISP_M0:
		tmp_reg = Inp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON) & ~(0x3F<<25);
		/* reset value(0x20) [30:25] for AR */
		Outp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON, tmp_reg | ((ar&0x3F)<<25));
		break;
	case BTS_DISP_S2:
		tmp_reg = Inp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON) & ~(0x1F<<10);
		/* reset value(0x10) [14:10] for AR */
		Outp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON, tmp_reg | ((ar&0x1F)<<10));
		break;
	case BTS_DISP_S1:
		tmp_reg = Inp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON) & ~(0x1F<<5);
		/* reset value(0x10) [9:5] for AR*/
		Outp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON, tmp_reg | ((ar&0x1F)<<5));
		break;
	case BTS_DISP_S0:
		tmp_reg = Inp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON) & ~(0x1F);
		/* reset value(0x10) [4:0] for AR */
		Outp32(base + DISP_XIU_DISP1_AR_AC_TARGET_CON, tmp_reg | (ar&0x1F));
		break;
	case BTS_ISP1_M0:
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON) & ~(0x3F<<24);
		/* reset value(0x20), [29:24] for AR */
		Outp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON, tmp_reg | ((ar&0x3F)<<24));
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON) & ~(0x3F<<24);
		/* reset value(0x20), [29:24] for AW */
		Outp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON, tmp_reg | ((aw&0x3F)<<24));
		break;
	case BTS_ISP1_S2:
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON) & ~(0x3F<<16);
		/* reset value(0x10), [21:16] for AR */
		Outp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON, tmp_reg | ((ar&0x3F)<<16));
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON) & ~(0x3F<<16);
		/* reset value(0x10), [21:16] for AW */
		Outp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON, tmp_reg | ((aw&0x3F)<<16));
		break;
	case BTS_ISP1_S1:
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON) & ~(0x3F<<8);
		/* reset value(0x10), [13:8] for AR */
		Outp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON, tmp_reg | ((ar&0x3F)<<8));
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON) & ~(0x3F<<8);
		/* reset value(0x10), [13:8] for AW */
		Outp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON, tmp_reg | ((aw&0x3F)<<8));
		break;
	case BTS_ISP1_S0:
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON) & ~(0x3F);
		/* reset value(0x20), [5:0] for AR */
		Outp32(base + ISP_XIU_ISP1_AR_AC_TARGET_CON, tmp_reg | (ar&0x3F));
		tmp_reg = Inp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON) & ~(0x3F);
		/* reset value(0x20), [5:0] for AW */
		Outp32(base + ISP_XIU_ISP1_AW_AC_TARGET_CON, tmp_reg | (aw&0x3F));
		break;
	case BTS_MSCL_M0:
		/* reset value(0x2020), [13:8], [5:0] for AW, AR */
		Outp32(base + MFCMSCL_XIU_MSCL0DX_M0, ((aw&0x3F)<<8) || (ar&0x3F));
		break;
	case BTS_JPEG_S0:
		/* reset value(0x2020), [12:8], [4:0] for AW, AR */
		Outp32(base + MFCMSCL_XIU_MSCL0DX_S0, ((aw&0x1F)<<8) || (ar&0x1F));
		break;
	case BTS_MSCALER0_S1:
		/* reset value(0x2020), [12:8], [4:0] for AW, AR */
		Outp32(base + MFCMSCL_XIU_MSCL0DX_S1, ((aw&0x1F)<<8) || (ar&0x1F));
		break;
	case BTS_MSCALER1_S2:
		/* reset value(0x2020), [12:8], [4:0] for AW, AR */
		Outp32(base + MFCMSCL_XIU_MSCL0DX_S2, ((aw&0x1F)<<8) || (ar&0x1F));
		break;
	default:
		break;
	}
}

void bts_setqos_sysreg(BWL_QOS_SYSREG_IP qos_id, addr_u32 base, unsigned int *priority)
{
	switch (qos_id) {
	case BTS_SYSREG_AUD:
		Outp32(base + AUD_QOS_CON, (priority[0] << 4)|(priority[0]));
		break;
	case BTS_SYSREG_DISP:
		Outp32(base + DISP_QOS_SEL, 0x0); /* DISP Qos select : 0x0(SYSREG QOS), 0x1(DECON QOS) */
		Outp32(base + DISP_XIU_DISP_QOS_CON, (priority[0] << 8)|(priority[0] << 4)|(priority[0]));
		break;
	case BTS_SYSREG_FSYS0:
		Outp32(base + FSYS_QOS_CON0, (priority[0] << 28)|(priority[0] << 24) |
			(priority[0] << 20)|(priority[0] << 16)|(priority[0] << 12)|
			(priority[0] << 8)|(priority[0] << 4)|(priority[0]));
		break;
	case BTS_SYSREG_FSYS1:
		Outp32(base + FSYS_QOS_CON1, (priority[0] << 20)|(priority[0] << 16) |
			(priority[0] << 12)|(priority[0] << 8)|(priority[0] << 4)|
			(priority[0]));
		break;
	case BTS_SYSREG_IMEM:
		Outp32(base + IMEM_QOS_CON, (priority[0] << 16) | (priority[0] << 12) |
			(priority[0] << 8) | (priority[0] << 4) | (priority[0]));
		break;
	case BTS_SYSREG_ISP0:
		Outp32(base + ISP_QOS_CON0, (priority[1] << 28) | (priority[1] << 20) |
			(priority[1] << 16) | (priority[0] << 12) | (priority[0] << 8) |
			(priority[0] << 4) | (priority[0]));
		break;
	case BTS_SYSREG_ISP1:
		Outp32(base + ISP_QOS_CON1, (priority[0] << 12) | (priority[0] << 4));
		break;
	case BTS_SYSREG_MIF_MODAPIF:
		/* MODEM Qos select : 0x0(SYSREG QOS), 0x1(MODEM QOS) */
		Outp32(base + MIF_MODAPIF_QOS_CON, (priority[0] << 8)|(priority[0] << 4) | 0x01);
		break;
	case BTS_SYSREG_MIF_CPU:
		Outp32(base + MIF_CPU_QOS_CON, (priority[0] << 4) | priority[0]);
		break;
	case BTS_SYSREG_MIF_APL:
		Outp32(base + MIF_APL_QOS_CON, (priority[0] << 4) | priority[0]);
		break;
	case BTS_SYSREG_MFC:
		Outp32(base + MFCMSCL_QOS_CON, (priority[0] << 28) | (priority[0] << 24)| \
			(Inp32(base + MFCMSCL_QOS_CON)&0x00FFFFFF));
		break;
	case BTS_SYSREG_JPEG:
		Outp32(base + MFCMSCL_QOS_CON, (priority[0] << 20) | (priority[0] << 16) | \
			(Inp32(base + MFCMSCL_QOS_CON)&0xFF00FFFF));
		break;
	case BTS_SYSREG_MSCL0:
		Outp32(base + MFCMSCL_QOS_CON, (priority[0] << 4) | (priority[0]) | \
			(Inp32(base + MFCMSCL_QOS_CON)&0xFFFFFF00));
		break;
	case BTS_SYSREG_MSCL1:
		Outp32(base + MFCMSCL_QOS_CON, (priority[0] << 12) | (priority[0] << 8) |
			(Inp32(base + MFCMSCL_QOS_CON)&0xFFFF00FF));
		break;
	default:
		break;
	}
}

void bts_setqos(addr_u32 base, unsigned int priority)  /* QOS :  [RRRRWWWW] */
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_TOKENMAX, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND, 0x18);
	Outp32(base + BTS_BWLOBOUND, 0x1);
	Outp32(base + BTS_INITTKN, 0x8);

	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x18);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x1);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x8);

	Outp32(base + BTS_RCON, 0x1);
	Outp32(base + BTS_WCON, 0x1);
}

void bts_setqos_bw(addr_u32 base, unsigned int priority,
			unsigned int window, unsigned int token) /* QOS :  [RRRRWWWW] */
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_TOKENMAX, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND, 0x18);
	Outp32(base + BTS_BWLOBOUND, 0x1);
	Outp32(base + BTS_INITTKN, 0x8);
	Outp32(base + BTS_DEMWIN, window);
	Outp32(base + BTS_DEMTKN, token);
	Outp32(base + BTS_DEFWIN, window);
	Outp32(base + BTS_DEFTKN, token);
	Outp32(base + BTS_PRMWIN, window);
	Outp32(base + BTS_PRMTKN, token);
	Outp32(base + BTS_FLEXIBLE, 0x0);

	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x18);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x1);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x8);
	Outp32(base + BTS_DEMWIN + WOFFSET, window);
	Outp32(base + BTS_DEMTKN + WOFFSET, token);
	Outp32(base + BTS_DEFWIN + WOFFSET, window);
	Outp32(base + BTS_DEFTKN + WOFFSET, token);
	Outp32(base + BTS_PRMWIN + WOFFSET, window);
	Outp32(base + BTS_PRMTKN + WOFFSET, token);
	Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x0);

	Outp32(base + BTS_RMODE, 0x1);
	Outp32(base + BTS_WMODE, 0x1);
	Outp32(base + BTS_RCON, 0x3);
	Outp32(base + BTS_WCON, 0x3);
}

void bts_setqos_mo(addr_u32 base, unsigned int priority, unsigned int mo)  /* QOS :  [RRRRWWWW] */
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_MOUPBOUND, 0x7F - mo);
	Outp32(base + BTS_MOLOBOUND, mo);
	Outp32(base + BTS_FLEXIBLE, 0x0);

	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));
	Outp32(base + BTS_MOUPBOUND + WOFFSET, 0x7F - mo);
	Outp32(base + BTS_MOLOBOUND + WOFFSET, mo);
	Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x0);

	Outp32(base + BTS_RMODE, 0x2);
	Outp32(base + BTS_WMODE, 0x2);
	Outp32(base + BTS_RCON, 0x3);
	Outp32(base + BTS_WCON, 0x3);
}

void bts_disable(addr_u32 base)
{
	/* reset to default */
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_RMODE, 0x1);
	Outp32(base + BTS_WMODE, 0x1);

	Outp32(base + BTS_PRIORITY, 0xA942);
	Outp32(base + BTS_TOKENMAX, 0x0);
	Outp32(base + BTS_BWUPBOUND, 0x3FFF);
	Outp32(base + BTS_BWLOBOUND, 0x3FFF);
	Outp32(base + BTS_INITTKN, 0x7FFF);
	Outp32(base + BTS_DEMWIN, 0x7FFF);
	Outp32(base + BTS_DEMTKN, 0x1FFF);
	Outp32(base + BTS_DEFWIN, 0x7FFF);
	Outp32(base + BTS_DEFTKN, 0x1FFF);
	Outp32(base + BTS_PRMWIN, 0x7FFF);
	Outp32(base + BTS_PRMTKN, 0x1FFF);
	Outp32(base + BTS_MOUPBOUND, 0x1F);
	Outp32(base + BTS_MOLOBOUND, 0x1F);
	Outp32(base + BTS_FLEXIBLE, 0x0);

	Outp32(base + BTS_PRIORITY + WOFFSET, 0xA942);
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0x0);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x3FFF);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x3FFF);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_DEMWIN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_DEMTKN + WOFFSET, 0x1FFF);
	Outp32(base + BTS_DEFWIN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_DEFTKN + WOFFSET, 0x1FFF);
	Outp32(base + BTS_PRMWIN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_PRMTKN + WOFFSET, 0x1FFF);
	Outp32(base + BTS_MOUPBOUND + WOFFSET, 0x1F);
	Outp32(base + BTS_MOLOBOUND + WOFFSET, 0x1F);
	Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x0);
}
