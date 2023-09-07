/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <mt-plat/mtk_ccci_common.h>
#include "cldma_reg.h"
#include "ccci_debug.h"

#define MD_PERI_MISC_BASE			(0x20060000)
#define MD_P_TOPSM_BASE				(0x200D0000)
#define MD_L1_TOPSM_BASE			(0x26070000)
#define MD_MIXEDSYS_TOPSM_BASE			(0x2616B000)

#define	TAG	"pwr-off"

void power_off_md1(void)
{
	unsigned int reg_val = 0;
	void __iomem *md_peri_misc_reg_base = ioremap_nocache(MD_PERI_MISC_BASE, 0x100);
	void __iomem *md_p_topsm_reg_base = ioremap_nocache(MD_P_TOPSM_BASE, 0x1000);
	void __iomem *md_l1_topsm_reg_base = ioremap_nocache(MD_L1_TOPSM_BASE, 0x200);
	void __iomem *md_mixedsys_topsm_reg_base = ioremap_nocache(MD_MIXEDSYS_TOPSM_BASE, 0x500);

	/* 0. power on md */
	/* ret = spm_mtcmos_ctrl_md1(STA_POWER_ON);*/
	CCCI_INF_MSG(MD_SYS1, TAG, "[ccci-off]0.power on MD_INFRA/MODEM_TOP\n");
	/* 1. pms init */
	CCCI_INF_MSG(MD_SYS1, TAG, "[ccci-off]pms init\n");
	cldma_write32(md_peri_misc_reg_base, 0x0090, 0x0000FFFF);
	cldma_write32(md_peri_misc_reg_base, 0x0094, 0x0000FFFF);
	cldma_write32(md_peri_misc_reg_base, 0x0098, 0x0000FFFF);
	cldma_write32(md_peri_misc_reg_base, 0x009C, 0x0000FFFF);
	cldma_write32(md_peri_misc_reg_base, 0x00C4, 0x00000007);

	/* 2. mixedsys topsm init, for release srcclkena in kernel */
	CCCI_INF_MSG(MD_SYS1, TAG, "[ccci-off]mixedsys topsm init\n");
	cldma_write32(md_mixedsys_topsm_reg_base, 0x0C0, 0x3252511D);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x0C4, 0x05003337);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x0CC, 0x0);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x100, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x108, 0x01010101);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x120, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x140, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x160, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x170, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x180, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x200, 0x0);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x208, 0x03030300);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x220, 0x3);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x240, 0x0);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x260, 0x3);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x270, 0x3);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x280, 0x3);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x300, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x308, 0x01010101);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x320, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x340, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x360, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x370, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x380, 0x1);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x000, 0xB2002501);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x400, 0x50086);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x410, 0x4);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x418, 0x14);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x420, 0x14);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x430, 0x8);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x440, 0x0);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x450, 0x00000000);
	cldma_write32(md_mixedsys_topsm_reg_base, 0x458, 0x00000000);

	/* 3. Shutting off ARM7, HSPAL2, LTEL2 power domains */
	/* Shutting off ARM7 through software */
	reg_val = cldma_read32(md_p_topsm_reg_base, 0x0804);
	reg_val &= ~0xE6045;
	cldma_write32(md_p_topsm_reg_base, 0x0804, reg_val);
	reg_val = cldma_read32(md_p_topsm_reg_base, 0x0804);
	reg_val |= 0xB8;
	cldma_write32(md_p_topsm_reg_base, 0x0804, reg_val);
	/* Masking control of ostimer on ARM7,HSPAL2,LTEL2 */
	cldma_write32(md_p_topsm_reg_base, 0x0018, 0x01);
	/* De-asserting software power req */
	reg_val = cldma_read32(md_p_topsm_reg_base, 0x0800);
	reg_val &= ~0x44;
	cldma_write32(md_p_topsm_reg_base, 0x0800, reg_val); /* PSMCU */
	reg_val = cldma_read32(md_p_topsm_reg_base, 0x0808);
	reg_val &= ~0x44;
	cldma_write32(md_p_topsm_reg_base, 0x0808, reg_val); /* LTEL2 */
	reg_val = cldma_read32(md_p_topsm_reg_base, 0x080C);
	reg_val &= ~0x44;
	cldma_write32(md_p_topsm_reg_base, 0x080C, reg_val); /* LTEL2 */
	reg_val = cldma_read32(md_p_topsm_reg_base, 0x0810);
	reg_val &= ~0x44;
	cldma_write32(md_p_topsm_reg_base, 0x0810, reg_val); /* INFRA */

	/* 4. PSMCU and INFRA power domains should be shut off at the end,*/
	/* after complete register sequence has been executed: */
	cldma_write32(md_p_topsm_reg_base, 0x0018, 0x00); /* PSMCU into sleep */
	cldma_write32(md_p_topsm_reg_base, 0x001C, 0x00); /* INFRA into sleep */

	/* 5. Shutting off power domains except L1MCU by masking all ostimers control */
	/* on mtcmos power domain: */
	reg_val = cldma_read32(md_l1_topsm_reg_base, 0x0140);
	reg_val |= ~(0x1);
	cldma_write32(md_l1_topsm_reg_base, 0x0140, reg_val);
	cldma_write32(md_l1_topsm_reg_base, 0x0144, 0xFFFFFFFF);
	cldma_write32(md_l1_topsm_reg_base, 0x0148, 0xFFFFFFFF);
	cldma_write32(md_l1_topsm_reg_base, 0x014C, 0xFFFFFFFF);
	cldma_write32(md_l1_topsm_reg_base, 0x0150, 0xFFFFFFFF);

	/* 6. L1MCU power domain is shut off in the end */
	/* after all register sequence has been executed: */
	cldma_write32(md_l1_topsm_reg_base, 0x0140, 0xFFFFFFFF);
	CCCI_INF_MSG(MD_SYS1, TAG, "[ccci-off]8.power off ARM7, HSPAL2, LTEL2\n");
}
