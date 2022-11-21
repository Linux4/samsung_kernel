/*
 *
 * Copyright (C) 2006, Marvell Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MACH_TC35876x_H
#define __MACH_TC35876x_H

#include <mach/pxa988.h>
#include <mach/pxa168fb.h>

struct tc35876x_platform_data {
	int id;
	int id_reg;
	int (*platform_init) (void);
};
#define Vx5B3D_MIPI_MERGE

#define TC358765_CHIPID         0x6500
#define TC358765_CHIPID_REG     0x0580

/*DSI PPI Layer Registers*/
#define PPI_STARTPPI		0x0104
#define PPI_LPTXTIMECNT		0x0114
#define PPI_LANEENABLE		0x0134
#define PPI_TX_RX_TA		0x013c
#define PPI_D0S_CLRSIPOCOUNT	0x0164
#define PPI_D1S_CLRSIPOCOUNT	0x0168
#define PPI_D2S_CLRSIPOCOUNT	0x016c
#define PPI_D3S_CLRSIPOCOUNT	0x0170
/*DSI Protocol Layer Register*/
#define DSI_STARTDSI		0x0204
#define DSI_LANEENABLE		0x0210
/*Video Path Register*/
#define VPCTRL			0x0450
#define HTIM1			0x0454
#define HTIM2			0x0458
#define VTIM1			0x045C
#define VTIM2			0x0460
#define VFUEN			0x0464
/*LVDS Registers*/
#define LVMX0003		0x0480
#define LVMX0407		0x0484
#define LVMX0811		0x0488
#define LVMX1215		0x048c
#define LVMX1619		0x0490
#define LVMX2023		0x0494
#define LVMX2427		0x0498
#define LVCFG			0x049c
#define LVPHY0			0x04A0
/*DSI PPI Layer Registers*/
#define PPI_BUSYPPI		0x0108
/*DSI Protocol Layer Regsters*/
#define DSI_BUSYDSI		0x0208
#define DSI_LANESTATUS0		0x0214
#define DSI_LANESTATUS1		0x0218
#define DSI_INTSTAUS		0x0220
#define DSI_INTMASK		0x0224
#define DSI_INTCLR		0x0228
/*DSI General Registers*/
#define DSIERRCNT		0x0300
/*System Registers*/
#define SYSSTAT			0x0500
#define SYSRST			0x0504
/*GPIO registers*/
#define GPIOC			0x0520
#define GPIOO			0x0524
/*Chip/Rev Registers*/
#define IDREG			0x0580

#define D0W_DPHYCONTRX		0x0024
#define D1W_DPHYCONTRX		0x0028
#define D2W_DPHYCONTRX		0x002C
#define D3W_DPHYCONTRX		0x0030
#define D0W_CNTRL		0x0044
#define D1W_CNTRL		0x0048
#define D2W_CNTRL		0x004c
#define D3W_CNTRL		0x0050
#define PPI_D0S_ATMR		0x0144
#define PPI_D1S_ATMR		0x0148
#define PPI_D2S_ATMR		0x014C
#define PPI_D3S_ATMR		0x0150
#define PPI_D0S_CLRSIPOCOUNT	0x0164
#define PPI_D1S_CLRSIPOCOUNT	0x0168
#define PPI_D2S_CLRSIPOCOUNT	0x016c
#define PPI_D3S_CLRSIPOCOUNT	0x0170
#define D0S_PRE			0x0184
#define D1S_PRE			0x0188
#define D2S_PRE			0x018C
#define D3S_PRE			0x0190
#define D0S_PREP		0x01A4
#define D1S_PREP		0x01A8
#define D2S_PREP		0x01AC
#define D3S_PREP		0x01B0
#define D0S_ZERO		0x01C4
#define D1S_ZERO		0x01C8
#define D2S_ZERO		0x01CC
#define D3S_ZERO		0x01D0

/*For Toshiba mipi2lvds brige*/
extern int tc35876x_write32(u16 reg, u32 val);
extern int tc35876x_write16(u16 reg, u16 val);
extern int tc35876x_read16(u16 reg, u16 *pval);
extern int tc35876x_read32(u16 reg, u32 *pval);

/*For Quicklogic mipi2lvds brige*/
extern int vx5d3b_mipi_write(struct pxa168fb_info *fbi, u32 address, u32 value, u32 data_size);
extern int vx5d3b_i2c_read(u32 addr, u32 *val, u32 data_size) ;
extern int vx5d3b_i2c_release(void);
extern struct pxa168fb_info *fbi_global;
extern struct Vx5d3b_cabc_info *g_vx5d3b;
extern unsigned int system_rev;
extern int panel_id;
extern unsigned int lpcharge;
extern int recovery_mode;

/**************************************************
*		VX5B3DX MACRO FOR I2C			*
***************************************************/

#define CONTROL_BYTE_DCS       (0x08u)
#define CONTROL_BYTE_GEN       (0x09u)
#define CONTROL_BYTE_I2C_RELEASE (0x0u)
#define Vx5D3B_I2C_RELEASE  {\
		CONTROL_BYTE_I2C_RELEASE, \
}

#define GEN_QL_CSR_OFFSET_LENGTH  {\
		CONTROL_BYTE_GEN, \
        0x29,  /* Data ID */\
        0x05,  /* Vendor Id 1 */\
        0x01,  /* Vendor Id 2 */\
        0x41,  /* Vendor Unique Command */\
        0x00,  /* Address LS */\
        0x00,  /* Address MS */\
        0x00,  /* Length LS */\
        0x00,  /* Length MS */\
    }

#define GEN_QL_CSR_WRITE  {\
		CONTROL_BYTE_GEN, \
        0x29,  /* Data ID */\
        0x05,  /* Vendor Id 1 */\
        0x01,  /* Vendor Id 2 */\
        0x40,  /* Vendor Unique Command */\
        0x00,  /* Address LS */\
        0x00,  /* Address MS */\
        0x00,  /* data LS */\
	0x00, \
	0x00, \
        0x00,  /* data MS */\
    }
#endif
