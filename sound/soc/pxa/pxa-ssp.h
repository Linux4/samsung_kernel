/*
 * ASoC PXA SSP port support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PXA_SSP_H
#define _PXA_SSP_H

/* pxa DAI SSP IDs */
#define PXA_DAI_SSP1			0
#define PXA_DAI_SSP2			1
#define PXA_DAI_SSP3			2
#define PXA_DAI_SSP4			3

/* SSP clock sources */
#define PXA_SSP_CLK_PLL	0
#define PXA_SSP_CLK_EXT	1
#define PXA_SSP_CLK_NET	2
#define PXA_SSP_CLK_AUDIO	3
#define PXA_SSP_CLK_NET_PLL	4

/* SSP audio dividers */
#define PXA_SSP_AUDIO_DIV_ACDS		0
#define PXA_SSP_AUDIO_DIV_SCDB		1
#define PXA_SSP_DIV_SCR			2
#define PXA_SSP_AUDIO_DIV_ACPS		3

/* SSP ACDS audio dividers values */
#define PXA_SSP_CLK_AUDIO_DIV_1		0
#define PXA_SSP_CLK_AUDIO_DIV_2		1
#define PXA_SSP_CLK_AUDIO_DIV_4		2
#define PXA_SSP_CLK_AUDIO_DIV_8		3
#define PXA_SSP_CLK_AUDIO_DIV_16	4
#define PXA_SSP_CLK_AUDIO_DIV_32	5

/* SSP divider bypass */
#define PXA_SSP_CLK_SCDB_4		0
#define PXA_SSP_CLK_SCDB_1		1
#define PXA_SSP_CLK_SCDB_8		2

#define PXA_SSP_PLL_OUT  0

#define PXA_SSP_FIFO_DEPTH		16

/*
 * FixMe: for port 5 (gssp), it is shared by ap
 * and cp. When AP want to handle it, AP need to
 * configure APB to connect gssp. Also reset gspp
 * clk to clear the potential impact from cp
 */
#define APBCONTROL_BASE		0xD403B000
#define APBCONTROL_SIZE		0x3C
#define APBC_GBS		0xC
#define APBC_GCER		0x34

#define GSSP_BUS_APB_SEL	0x1
#define GSSP_CLK_SEL_MASK	0x3
#define GSSP_CLK_SEL_OFF	0x8
#define GSSP_CLK_EN	(1 << 0)  /* APB Bus Clock Enable */
#define GSSP_FNCLK_EN	(1 << 1)  /* Functional Clock Enable */
#define GSSP_RST	(1 << 2)  /* Reset Generation */

int mmp_pcm_platform_register(struct device *dev);
void mmp_pcm_platform_unregister(struct device *dev);
int pxa_pcm_platform_register(struct device *dev);
void pxa_pcm_platform_unregister(struct device *dev);

#endif
