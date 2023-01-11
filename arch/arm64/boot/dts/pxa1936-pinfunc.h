/*
 * Copyright 2013 Marvell Tech. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __DTS_PXA1908_PINFUNC_H
#define __DTS_PXA1908_PINFUNC_H

/* drive-strength: value, mask */
#define DS_SLOW0		pinctrl-single,drive-strength = <0x0000 0x1800>
#define DS_SLOW1		pinctrl-single,drive-strength = <0x0800 0x1800>
#define DS_MEDIUM		pinctrl-single,drive-strength = <0x1000 0x1800>
#define DS_FAST			pinctrl-single,drive-strength = <0x1800 0x1800>

/* input-schmitt: value, mask ; input-schmitt-enable: value, enable, disable, mask */
#define EDGE_NONE		pinctrl-single,input-schmitt = <0 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x40 0 0x40 0x40>
#define EDGE_RISE		pinctrl-single,input-schmitt = <0x10 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x0 0 0x40 0x40>
#define EDGE_FALL		pinctrl-single,input-schmitt = <0x20 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x0 0 0x40 0x40>
#define EDGE_BOTH		pinctrl-single,input-schmitt = <0x30 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x0 0 0x40 0x40>

/* bias-pullup/down: value, enable, disable, mask */
#define PULL_NONE		pinctrl-single,bias-pullup = <0x0000 0xc000 0 0xc000>;	\
				pinctrl-single,bias-pulldown = <0x0000 0xa000 0 0xa000>
#define PULL_UP			pinctrl-single,bias-pullup = <0xc000 0xc000 0 0xc000>;	\
				pinctrl-single,bias-pulldown = <0x8000 0xa000 0x8000 0xa000>
#define PULL_DOWN		pinctrl-single,bias-pullup = <0x8000 0xc000 0x8000 0xc000>; \
				pinctrl-single,bias-pulldown = <0xa000 0xa000 0 0xa000>
#define PULL_BOTH		pinctrl-single,bias-pullup = <0xc000 0xc000 0x8000 0xc000>; \
				pinctrl-single,bias-pulldown = <0xa000 0xa000 0x8000 0xa000>
#define PULL_FLOAT		pinctrl-single,bias-pullup = <0x8000 0x8000 0 0xc000>;	\
				pinctrl-single,bias-pulldown = <0x8000 0x8000 0 0xa000>

/*
 * Table that determines the low power modes outputs, with actual settings
 * used in parentheses for don't-care values. Except for the float output,
 * the configured driven and pulled levels match, so if there is a need for
 * non-LPM pulled output, the same configuration could probably be used.
 *
 * Output value sleep_oe_n sleep_data sleep_sel
 *               (bit 7)    (bit 8)   (bit 9/3)
 * None            X(0)       X(0)      00
 * Drive 0         0          0         11
 * Drive 1         0          1         11
 * Z (float)       1          X(0)      11
 * lpm-output: value, mask
 */
#define LPM_NONE		pinctrl-single,lpm-output = <0x0 0x388>
#define LPM_DRIVE_LOW		pinctrl-single,lpm-output = <0x208 0x388>
#define LPM_DRIVE_HIGH		pinctrl-single,lpm-output = <0x308 0x388>
#define LPM_FLOAT		pinctrl-single,lpm-output = <0x288 0x388>

#define MFP_DEFAULT		DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_NONE

#define MFP_PULL_UP		DS_MEDIUM;PULL_UP;EDGE_NONE;LPM_NONE
#define MFP_PULL_DOWN		DS_MEDIUM;PULL_DOWN;EDGE_NONE;LPM_NONE
#define MFP_PULL_FLOAT		DS_MEDIUM;PULL_FLOAT;EDGE_NONE;LPM_NONE

/* LPM output */
#define MFP_LPM_DRIVE_HIGH	DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_DRIVE_HIGH
#define MFP_LPM_DRIVE_LOW	DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_DRIVE_LOW
/* LPM input */
#define MFP_LPM_FLOAT		DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_FLOAT
#define MFP_LPM_PULL_UP		DS_MEDIUM;PULL_UP;EDGE_NONE;LPM_FLOAT
#define MFP_LPM_PULL_DW		DS_MEDIUM;PULL_DOWN;EDGE_NONE;LPM_FLOAT

/*
 * MFP alternative functions 0-7
 */
#define AF0			0x0
#define AF1			0x1
#define AF2			0x2
#define AF3			0x3
#define AF4			0x4
#define AF5			0x5
#define AF6			0x6
#define AF7			0x7

/*
 * Pin names and MFPR addresses
 */
#define GPIO0			0x00dc
#define GPIO1			0x00e0
#define GPIO2			0x00e4
#define GPIO3			0x00e8
#define GPIO4			0x00ec
#define GPIO5			0x00f0
#define GPIO6			0x00f4
#define GPIO7			0x00f8
#define GPIO8			0x00fc
#define GPIO9			0x0100
#define GPIO10			0x0104
#define GPIO11			0x0108
#define GPIO12			0x010c
#define GPIO13			0x0110
#define GPIO14			0x0114
#define GPIO15			0x0118
#define GPIO16			0x011c
#define GPIO17			0x0120
#define GPIO18			0x0124
#define GPIO19			0x0128
#define GPIO20			0x012c
#define GPIO21			0x0130
#define GPIO22			0x0134
#define GPIO23			0x0138
#define GPIO24			0x013c
#define GPIO25			0x0140
#define GPIO26			0x0144
#define GPIO27			0x0148
#define GPIO28			0x014c
#define GPIO29			0x0150
#define GPIO30			0x0154
#define GPIO31			0x0158
#define GPIO32			0x015c
#define GPIO33			0x0160
#define GPIO34			0x0164
#define GPIO35			0x0168
#define GPIO36			0x016c
#define GPIO37			0x0170
#define GPIO38			0x0174
#define GPIO39			0x0178
#define GPIO40			0x017c
#define GPIO41			0x0180
#define GPIO42			0x0184
#define GPIO43			0x0188
#define GPIO44			0x018c
#define GPIO45			0x0190
#define GPIO46			0x0194
#define GPIO47			0x0198
#define GPIO48			0x019c
#define GPIO49			0x01a0
#define GPIO50			0x01a4
#define GPIO51			0x01a8
#define GPIO52			0x01ac
#define GPIO53			0x01b0
#define GPIO54			0x01b4
#define GPIO67			0x01b8
#define GPIO68			0x01bc
#define GPIO69			0x01c0
#define GPIO70			0x01c4
#define GPIO71			0x01c8
#define GPIO72			0x01cc
#define GPIO73			0x01d0
#define GPIO74			0x01d4
#define GPIO75			0x01d8
#define GPIO76			0x01dc
#define GPIO77			0x01e0
#define GPIO78			0x01e4
#define GPIO79			0x01e8
#define GPIO80			0x01ec
#define GPIO81			0x01f0
#define GPIO82			0x01f4
#define GPIO83			0x01f8
#define GPIO84			0x01fc
#define GPIO85			0x0200
#define GPIO86			0x0204
#define GPIO87			0x0208
#define GPIO88			0x020c
#define GPIO89			0x0210
#define GPIO90			0x0214
#define GPIO91			0x0218
#define GPIO92			0x021c
#define GPIO93			0x0220
#define GPIO94			0x0224
#define GPIO95			0x0228
#define GPIO96			0x022c
#define GPIO97			0x0230
#define GPIO98			0x0234
#define GPIO110			0x0298
#define GPIO111			0x029c
#define GPIO124			0x00d0
#define DF_IO0			0x0040
#define DF_IO1			0x003c
#define DF_IO2			0x0038
#define DF_IO3			0x0034
#define DF_IO4			0x0030
#define DF_IO5			0x002c
#define DF_IO6			0x0028
#define DF_IO7			0x0024
#define DF_IO8			0x0020
#define DF_IO9			0x001c
#define DF_IO10			0x0018
#define DF_IO11			0x0014
#define DF_IO12			0x0010
#define DF_IO13			0x000c
#define DF_IO14			0x0008
#define DF_IO15			0x0004
#define DF_nCS0_SM_nCS2	0x0044
#define DF_nCS1_SM_nCS3	0x0048
#define SM_nCS0			0x004c
#define SM_nCS1			0x0050
#define DF_WEn			0x0054
#define DF_REn			0x0058
#define DF_CLE_SM_OEn	0x005c
#define DF_ALE_SM_WEn	0x0060
#define DF_RDY0			0x0068
#define DF_RDY1			0x0078
#define SM_SCLK			0x0064
#define SM_BE0			0x006c
#define SM_BE1			0x0070
#define SM_ADV			0x0074
#define SM_ADVMUX		0x007c
#define SM_RDY_GPIO_3		0x0080
#define MMC1_DAT7		0x0084
#define MMC1_DAT6		0x0088
#define MMC1_DAT5		0x008c
#define MMC1_DAT4		0x0090
#define MMC1_DAT3		0x0094
#define MMC1_DAT2		0x0098
#define MMC1_DAT1		0x009c
#define MMC1_DAT0		0x00a0
#define MMC1_CMD		0x00a4
#define MMC1_CLK		0x00a8
#define MMC1_CD			0x00ac
#define MMC1_WP			0x00b0
#define ANT_SW4			0x026c
#define PA_MODE                 0x0270
#define RF_CONF_4               0x0274
#define VCXO_REQ		0x00d4

#define PRI_TDI			0x00b4
#define PRI_TMS			0x00b8
#define PRI_TCK			0x00bc
#define PRI_TDO			0x00c0
#define SLAVE_RESET_OUT 0x00c8

#define USIM2_UCLK		0x0260
#define USIM2_UIO		0x0264
#define USIM2_URSTn		0x0268

#define ND_IO10			DF_IO10
#define ND_IO11			DF_IO11
#define ND_IO12			DF_IO12
#define ND_IO13			DF_IO13
#define ND_IO14			DF_IO14
#define ND_IO15			DF_IO15
#define ND_IO7			DF_IO7
#define ND_IO6			DF_IO6
#define ND_IO5			DF_IO5
#define ND_IO4			DF_IO4
#define ND_IO3			DF_IO3
#define ND_IO2			DF_IO2
#define ND_IO1			DF_IO1
#define ND_IO0			DF_IO0

#define ND_CLE_SM_OEN	DF_CLE_SM_OEn

#define SM_BEN0			SM_BE0

#endif /* __DTS_PXA1908_PINFUNC_H */
