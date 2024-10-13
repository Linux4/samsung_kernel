/* arch/arm/mach-exynos/cal_bts.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - BTS CAL code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cal_bts7570.h"

#define LOG(x, ...)           	                      \
({                                                    \
	seq_printf(buf, x, ##__VA_ARGS__);            \
})

/* for BTS V2.1 Register */
#define BTS_RCON				0x100
#define BTS_RMODE				0x104
#define BTS_WCON				0x180
#define BTS_WMODE				0x184
#define BTS_PRIORITY				0x200
#define BTS_TOKENMAX				0x204
#define BTS_BWUPBOUND				0x20C
#define BTS_BWLOBOUND				0x210
#define BTS_INITTKN				0x214
#define BTS_RSTCLK				0x218
#define BTS_RSTTKN				0x21C
#define BTS_DEMWIN				0x220
#define BTS_DEMTKN				0x224
#define BTS_DEFWIN				0x228
#define BTS_DEFTKN				0x22C
#define BTS_PRMWIN				0x230
#define BTS_PRMTKN				0x234
#define BTS_MOUPBOUND				0x240
#define BTS_MOLOBOUND				0x244
#define BTS_FLEXIBLE				0x280
#define BTS_POLARITY				0x284
#define BTS_FBMGRP0ADRS				0x290
#define BTS_FBMGRP0ADRE				0x294
#define BTS_FBMGRP1ADRS				0x298
#define BTS_FBMGRP1ADRE				0x29C
#define BTS_FBMGRP2ADRS				0x2A0
#define BTS_FBMGRP2ADRE				0x2A4
#define BTS_FBMGRP3ADRS				0x2A8
#define BTS_FBMGRP3ADRE				0x2AC
#define BTS_FBMGRP4ADRS				0x2B0
#define BTS_FBMGRP4ADRE				0x2B4
#define BTS_EMERGENTID				0x2C0
#define BTS_RISINGTH				0x2C4
#define BTS_FALLINGTH				0x2C8
#define BTS_FALLINGMO				0x2CC
#define BTS_MOCOUNTER				0x2F0
#define BTS_STATUS				0x2F4
#define BTS_BWMONLOW				0x2F8
#define BTS_BWMONUP				0x2FC
#define WOFFSET					0x100

#define TREX_CON				0x000
#define TREX_TIMEOUT				0x010
#define TREX_BLOCK_IDMASK			0x018
#define TREX_BLOCK_IDVALUE			0x01C
#define TREX_RCON				0x020
#define TREX_WCON				0x040
#define TREX_RBLOCK_UPPER			0x024
#define TREX_WBLOCK_UPPER			0x044
#define TREX_RBLOCK_UPPER_NORMAL		0x028
#define TREX_WBLOCK_UPPER_NORMAL		0x048
#define TREX_RBLOCK_UPPER_FULL			0x02C
#define TREX_WBLOCK_UPPER_FULL			0x04C
#define TREX_RBLOCK_UPPER_BUSY			0x030
#define TREX_WBLOCK_UPPER_BUSY			0x050
#define TREX_RBLOCK_UPPER_MAX			0x034
#define TREX_WBLOCK_UPPER_MAX			0x054
#define SYSREG_CP_QOS				0x0078
#define SYSREG_GNSS_QOS				0x00B8
#define SYSREG_WIFI_QOS				0x0178
#define SYSREG_APM_QOS				0x000C
#define SYSREG_DISPAUD_QOS			0x0600
#define SYSREG_DISPAUD_SEL			0x0604
#define SYSREG_FSYS_VAL0			0x0600
#define SYSREG_FSYS_VAL1			0x0604
#define SYSREG_FSYS_VAL2			0x0608
#define SYSREG_ISP_QOS				0x0500
#define SYSREG_CPU_QOS				0x0610
#define SYSREG_MFCMSCL_QOS			0x0600


void bts_setqos(addr_u32 base, unsigned int priority)  /* QOS :  [RRRRWWWW] */
{
        __raw_writel(0x0, base + BTS_RCON);
        __raw_writel(0x0, base + BTS_RCON);
        __raw_writel(0x0, base + BTS_WCON);
        __raw_writel(0x0, base + BTS_WCON);
        __raw_writel(((priority >> 16) & 0xFFFF), base + BTS_PRIORITY);
        __raw_writel(0xFFDF, base + BTS_TOKENMAX);
        __raw_writel(0x18, base + BTS_BWUPBOUND);
        __raw_writel(0x1, base + BTS_BWLOBOUND);
        __raw_writel(0x8, base + BTS_INITTKN);
        __raw_writel((priority & 0xFFFF), base + BTS_PRIORITY + WOFFSET);
        __raw_writel(0xFFDF, base + BTS_TOKENMAX + WOFFSET);
        __raw_writel(0x18, base + BTS_BWUPBOUND + WOFFSET);
        __raw_writel(0x1, base + BTS_BWLOBOUND + WOFFSET);
        __raw_writel(0x8, base + BTS_INITTKN + WOFFSET);
        __raw_writel(0x1, base + BTS_RCON);
        __raw_writel(0x1, base + BTS_WCON);
}

void bts_setqos_bw(addr_u32 base, unsigned int priority,
			unsigned int window, unsigned int token) /* QOS :  [RRRRWWWW] */
{
	__raw_writel(0x0, base + BTS_RCON);
	__raw_writel(0x0, base + BTS_RCON);
	__raw_writel(0x0, base + BTS_WCON);
	__raw_writel(0x0, base + BTS_WCON);
	__raw_writel(((priority >> 16) & 0xFFFF), base + BTS_PRIORITY);
	__raw_writel(0xFFDF, base + BTS_TOKENMAX);
	__raw_writel(0x18, base + BTS_BWUPBOUND);
	__raw_writel(0x1, base + BTS_BWLOBOUND);
	__raw_writel(0x8, base + BTS_INITTKN);
	__raw_writel(window, base + BTS_DEMWIN);
	__raw_writel(token, base + BTS_DEMTKN);
	__raw_writel(window, base + BTS_DEFWIN);
	__raw_writel(token, base + BTS_DEFTKN);
	__raw_writel(window, base + BTS_PRMWIN);
	__raw_writel(token, base + BTS_PRMTKN);
	__raw_writel(0x0, base + BTS_FLEXIBLE);
	__raw_writel((priority & 0xFFFF), base + BTS_PRIORITY + WOFFSET);
	__raw_writel(0xFFDF, base + BTS_TOKENMAX + WOFFSET);
	__raw_writel(0x18, base + BTS_BWUPBOUND + WOFFSET);
	__raw_writel(0x1, base + BTS_BWLOBOUND + WOFFSET);
	__raw_writel(0x8, base + BTS_INITTKN + WOFFSET);
	__raw_writel(window, base + BTS_DEMWIN + WOFFSET);
	__raw_writel(token, base + BTS_DEMTKN + WOFFSET);
	__raw_writel(window, base + BTS_DEFWIN + WOFFSET);
	__raw_writel(token, base + BTS_DEFTKN + WOFFSET);
	__raw_writel(window, base + BTS_PRMWIN + WOFFSET);
	__raw_writel(token, base + BTS_PRMTKN + WOFFSET);
	__raw_writel(0x0, base + BTS_FLEXIBLE + WOFFSET);
	__raw_writel(0x1, base + BTS_RMODE);
	__raw_writel(0x1, base + BTS_WMODE);
	__raw_writel(0x3, base + BTS_RCON);
	__raw_writel(0x3, base + BTS_WCON);
}

void bts_setqos_mo(addr_u32 base, unsigned int priority, unsigned int mo)  /* QOS :  [RRRRWWWW] */
{
	__raw_writel(0x0, base + BTS_RCON);
	__raw_writel(0x0, base + BTS_RCON);
	__raw_writel(0x0, base + BTS_WCON);
	__raw_writel(0x0, base + BTS_WCON);
	__raw_writel(((priority >> 16) & 0xFFFF), base + BTS_PRIORITY);
	__raw_writel(0x7F - mo, base + BTS_MOUPBOUND);
	__raw_writel(mo, base + BTS_MOLOBOUND);
	__raw_writel(0x0, base + BTS_FLEXIBLE);
	__raw_writel((priority & 0xFFFF), base + BTS_PRIORITY + WOFFSET);
	__raw_writel(0x7F - mo, base + BTS_MOUPBOUND + WOFFSET);
	__raw_writel(mo, base + BTS_MOLOBOUND + WOFFSET);
	__raw_writel(0x0, base + BTS_FLEXIBLE + WOFFSET);
	__raw_writel(0x2, base + BTS_RMODE);
	__raw_writel(0x2, base + BTS_WMODE);
	__raw_writel(0x3, base + BTS_RCON);
	__raw_writel(0x3, base + BTS_WCON);
}

void bts_showqos(addr_u32 base, struct seq_file *buf)
{
	LOG("conreg 0x%Xr 0x%Xw qos 0x%Xr 0x%Xw\n",
			__raw_readl(base + BTS_RCON),
			__raw_readl(base + BTS_WCON),
			__raw_readl(base + BTS_PRIORITY),
			__raw_readl(base + BTS_PRIORITY + WOFFSET));
}
void bts_disable(addr_u32 base)
{
	/* reset to default */
	__raw_writel(0x0, base + BTS_RCON);
	__raw_writel(0x0, base + BTS_RCON);
	__raw_writel(0x0, base + BTS_WCON);
	__raw_writel(0x0, base + BTS_WCON);
	__raw_writel(0x1, base + BTS_RMODE);
	__raw_writel(0x1, base + BTS_WMODE);
	__raw_writel(0xA942, base + BTS_PRIORITY);
	__raw_writel(0x0, base + BTS_TOKENMAX);
	__raw_writel(0x3FFF, base + BTS_BWUPBOUND);
	__raw_writel(0x3FFF, base + BTS_BWLOBOUND);
	__raw_writel(0x7FFF, base + BTS_INITTKN);
	__raw_writel(0x7FFF, base + BTS_DEMWIN);
	__raw_writel(0x1FFF, base + BTS_DEMTKN);
	__raw_writel(0x7FFF, base + BTS_DEFWIN);
	__raw_writel(0x1FFF, base + BTS_DEFTKN);
	__raw_writel(0x7FFF, base + BTS_PRMWIN);
	__raw_writel(0x1FFF, base + BTS_PRMTKN);
	__raw_writel(0x1F, base + BTS_MOUPBOUND);
	__raw_writel(0x1F, base + BTS_MOLOBOUND);
	__raw_writel(0x0, base + BTS_FLEXIBLE);
	__raw_writel(0xA942, base + BTS_PRIORITY + WOFFSET);
	__raw_writel(0x0, base + BTS_TOKENMAX + WOFFSET);
	__raw_writel(0x3FFF, base + BTS_BWUPBOUND + WOFFSET);
	__raw_writel(0x3FFF, base + BTS_BWLOBOUND + WOFFSET);
	__raw_writel(0x7FFF, base + BTS_INITTKN + WOFFSET);
	__raw_writel(0x7FFF, base + BTS_DEMWIN + WOFFSET);
	__raw_writel(0x1FFF, base + BTS_DEMTKN + WOFFSET);
	__raw_writel(0x7FFF, base + BTS_DEFWIN + WOFFSET);
	__raw_writel(0x1FFF, base + BTS_DEFTKN + WOFFSET);
	__raw_writel(0x7FFF, base + BTS_PRMWIN + WOFFSET);
	__raw_writel(0x1FFF, base + BTS_PRMTKN + WOFFSET);
	__raw_writel(0x1F, base + BTS_MOUPBOUND + WOFFSET);
	__raw_writel(0x1F, base + BTS_MOLOBOUND + WOFFSET);
	__raw_writel(0x0, base + BTS_FLEXIBLE + WOFFSET);
}

void trex_setqos(addr_u32 base, unsigned int priority)
{
	unsigned int tmp_reg = 0;
	/* override QoS value */
	tmp_reg |= 1 << 8;
	tmp_reg |= (priority & 0xf) << 12;
	__raw_writel(tmp_reg, base + TREX_RCON);
	__raw_writel(tmp_reg, base + TREX_WCON);

	__raw_writel(0x1, base + TREX_CON);
}

void trex_setqos_mo(addr_u32 base, unsigned int priority, unsigned int mo)
{
	unsigned int tmp_reg = 0;
	if (mo > 0xffff)
		mo = 0xffff;
	__raw_writel(mo, base + TREX_RBLOCK_UPPER);
	__raw_writel(mo, base + TREX_WBLOCK_UPPER);
	/* override QoS value */
	tmp_reg |= 1 << 8;
	tmp_reg |= (priority & 0xf) << 12;
	/* enable Blocking logic */
	tmp_reg |= 1 << 0;
	__raw_writel(tmp_reg, base + TREX_RCON);
	__raw_writel(tmp_reg, base + TREX_WCON);
	__raw_writel(0x1, base + TREX_CON);
}

void trex_showqos(addr_u32 base, struct seq_file *buf)
{
	LOG("conreg 0x%X qos(%d,%d) 0x%Xr 0x%Xw mo(%d,%d) %dr %dw\n",
			__raw_readl(base + TREX_CON),
			(__raw_readl(base + TREX_RCON) >> 8) & 0x1,
			(__raw_readl(base + TREX_WCON) >> 8) & 0x1,
			(__raw_readl(base + TREX_RCON) >> 12) & 0xf,
			(__raw_readl(base + TREX_WCON) >> 12) & 0xf,
			__raw_readl(base + TREX_RCON) & 0x1,
			__raw_readl(base + TREX_WCON) & 0x1,
			__raw_readl(base + TREX_RBLOCK_UPPER),
			__raw_readl(base + TREX_WBLOCK_UPPER));
}
void trex_disable(addr_u32 base)
{
	__raw_writel(0x0, base + TREX_CON);
	__raw_writel(0x4000, base + TREX_RCON);
	__raw_writel(0x4000, base + TREX_WCON);
}

void sysreg_setqos(QOS_SYSREG_IP qos_id, addr_u32 base, unsigned int priority)
{
	unsigned int tmp_reg;
	switch (qos_id) {
	case BTS_SYSREG_CPGNSS:
		tmp_reg = __raw_readl(base + SYSREG_CP_QOS);
		tmp_reg |= 1;
		__raw_writel(tmp_reg, base + SYSREG_CP_QOS);
		tmp_reg = __raw_readl(base + SYSREG_GNSS_QOS);
		tmp_reg &= ~1;
		tmp_reg &= ~((0xf << 12) | (0xf << 16));
		tmp_reg |= (priority & 0xf) << 8;
		tmp_reg |= (priority & 0xf) << 16;
		__raw_writel(tmp_reg, base + SYSREG_GNSS_QOS);
		break;
	case BTS_SYSREG_APM:
		tmp_reg = __raw_readl(base + SYSREG_APM_QOS);
		tmp_reg &= ~(0xf | (0xf << 4));
		tmp_reg |= (priority & 0xf);
		tmp_reg |= (priority & 0xf) << 4;
		__raw_writel(tmp_reg, base + SYSREG_APM_QOS);
		break;
	case BTS_SYSREG_CPU:
		tmp_reg = __raw_readl(base + SYSREG_CPU_QOS);
		tmp_reg &= ~(0xf | (0xf << 4));
		tmp_reg |= (priority & 0xf);
		tmp_reg |= (priority & 0xf) << 4;
		__raw_writel(tmp_reg, base + SYSREG_CPU_QOS);
		break;
	default:
		break;
	}
}
void sysreg_disable(QOS_SYSREG_IP qos_id, addr_u32 base)
{
	unsigned int tmp_reg;
	switch (qos_id) {
	case BTS_SYSREG_CPGNSS:
		tmp_reg = __raw_readl(base + SYSREG_CP_QOS);
		tmp_reg |= 1;
		__raw_writel(tmp_reg, base + SYSREG_CP_QOS);
		tmp_reg = __raw_readl(base + SYSREG_GNSS_QOS);
		tmp_reg |= 1;
		tmp_reg |= (0x4) << 8;
		tmp_reg |= (0x4) << 16;
		__raw_writel(tmp_reg, base + SYSREG_GNSS_QOS);
		break;
	case BTS_SYSREG_APM:
		tmp_reg = __raw_readl(base + SYSREG_APM_QOS);
		tmp_reg &= ~(0xf | (0xf << 4));
		tmp_reg |= 0x4;
		tmp_reg |= 0x4 << 4;
		__raw_writel(tmp_reg, base + SYSREG_APM_QOS);
		break;
	case BTS_SYSREG_CPU:
		tmp_reg = __raw_readl(base + SYSREG_CPU_QOS);
		tmp_reg &= ~(0xf | (0xf << 4));
		tmp_reg |= 0x4;
		tmp_reg |= 0x4 << 4;
		__raw_writel(tmp_reg, base + SYSREG_CPU_QOS);
		break;
	default:
		break;
	}
}

void sysreg_showqos(QOS_SYSREG_IP qos_id, addr_u32 base, struct seq_file *buf)
{
	unsigned int tmp_reg;
	switch (qos_id) {
	case BTS_SYSREG_CPGNSS:
		tmp_reg = __raw_readl(base + SYSREG_GNSS_QOS);
		LOG("(cp bypass %d)bypass %u qos 0x%Xr 0x%Xw\n",
		    __raw_readl(base + SYSREG_CP_QOS) && 0x1,
		    tmp_reg & 1, (tmp_reg >> 8) & 0xf, (tmp_reg >> 16) & 0xf);
		break;
	case BTS_SYSREG_APM:
		tmp_reg = __raw_readl(base + SYSREG_APM_QOS);
		LOG("qos 0x%Xr 0x%Xw\n", tmp_reg & 0xf,
				(tmp_reg >> 4) & 0xf);
		break;
	case BTS_SYSREG_CPU:
		tmp_reg = __raw_readl(base + SYSREG_CPU_QOS);
		LOG("qos 0x%Xr 0x%Xw\n", tmp_reg & 0xf,
				(tmp_reg >> 4) & 0xf);
		break;
	default:
		break;
	}
}
