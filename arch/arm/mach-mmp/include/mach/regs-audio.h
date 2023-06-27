/*
 * linux/arch/arm/mach-mmp/include/mach/regs-audio.h
 *
 *   Audio Subsystem Unit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_REGS_AUDIO_H
#define __ASM_MACH_REGS_AUDIO_H

#include <mach/addr-map.h>

/* SSPA register base */
#define SSPA_VIRT_BASE		(AUD_VIRT_BASE + 0x400)
#define AUD_REG(x)		(SSPA_VIRT_BASE + (x))

#define AUD_CTL			AUD_REG(0x34)
#define AUD_PLL_CTL0		AUD_REG(0x38)
#define AUD_PLL_CTL1		AUD_REG(0x3c)

/* Audio Control Register */
#define AUD_CTL_S2_CLK_SEL_MASK			(1 << 23)
#define AUD_CTL_S2_CLK_SEL_I2S			(1 << 23)
#define AUD_CTL_S2_CLK_SEL_AUDIO_PLL		(0 << 23)
#define AUD_CTL_S2_CLK_DIV_MASK			(0x3f << 17)
#define AUD_CTL_S2_CLK_DIV(x)			((x) << 17)
#define AUD_CTL_S2_ENA				(1 << 16)

#define AUD_CTL_S1_CLK_SEL_MASK			(1 << 7)
#define AUD_CTL_S1_CLK_SEL_I2S			(1 << 7)
#define AUD_CTL_S1_CLK_SEL_AUDIO_PLL		(0 << 7)
#define AUD_CTL_S1_CLK_DIV_MASK			(0x3f << 9)
#define AUD_CTL_S1_CLK_DIV(x)			((x) << 9)
#define AUD_CTL_S1_ENA				(1 << 8)

#define AUD_CTL_SYSCLK_DIV_MASK			(0x3f << 1)
#define AUD_CTL_SYSCLK_DIV(x)			((x) << 1)
#define AUD_CTL_SYSCLK_ENA			(1 << 0)

/* Audio PLL Control 0 Register */
#define AUD_PLL_CTL0_DIV_MCLK1(x)		((x) << 31)
#define AUD_PLL_CTL0_DIV_OCLK_MODULO(x)		((x) << 28)
#define AUD_PLL_CTL0_FRACT(x)			((x) << 8)
#define AUD_PLL_CTL0_ENA_DITHER			(1 << 7)
#define AUD_PLL_CTL0_ICP_2UA			(0 << 5)
#define AUD_PLL_CTL0_ICP_5UA			(1 << 5)
#define AUD_PLL_CTL0_ICP_7UA			(2 << 5)
#define AUD_PLL_CTL0_ICP_10UA			(3 << 5)
#define AUD_PLL_CTL0_DIV_FBCCLK0_1(x)		((x) << 3)
#define AUD_PLL_CTL0_DIV_MCLK(x)		((x) << 2)
#define AUD_PLL_CTL0_PD_OVPROT_DIS		(1 << 1)
#define AUD_PLL_CTL0_PU				(1 << 0)

/* Audio PLL Control 1 Register */
#define AUD_PLL_CTL0_DIV_MCLK2_3(x)		((x) << 29)
#define AUD_PLL_CTL0_DIV_FBCCLK2_5(x)		((x) << 25)
#define AUD_PLL_CTL1_EN_VCOX2			(1 << 17)
#define AUD_PLL_CTL1_PLL_LOCK			(1 << 16)
#define AUD_PLL_CTL1_CLK_SEL_AUDIO_PLL		(1 << 11)
#define AUD_PLL_CTL1_CLK_SEL_VCXO		(0 << 11)
#define AUD_PLL_CTL1_DIV_OCLK_PATTERN(x)	((x) << 0)

/*
 * SSPA Registers
 */
#define SSPA_RXD		(0x00)
#define SSPA_RXID		(0x04)
#define SSPA_RXCTL		(0x08)
#define SSPA_RXSP		(0x0c)
#define SSPA_RXFIFO_UL		(0x10)
#define SSPA_RXINT_MASK		(0x14)
#define SSPA_RXC		(0x18)
#define SSPA_RXFIFO_NOFS	(0x1c)
#define SSPA_RXFIFO_SIZE	(0x20)

#define SSPA_TXD		(0x80)
#define SSPA_TXID		(0x84)
#define SSPA_TXCTL		(0x88)
#define SSPA_TXSP		(0x8c)
#define SSPA_TXFIFO_LL		(0x90)
#define SSPA_TXINT_MASK		(0x94)
#define SSPA_TXC		(0x98)
#define SSPA_TXFIFO_NOFS	(0x9c)
#define SSPA_TXFIFO_SIZE	(0xa0)

/* SSPA Control Register */
#define	SSPA_CTL_XPH		(1 << 31)	/* Read Phase */
#define	SSPA_CTL_XFIG		(1 << 15)	/* Transmit Zeros \
							when FIFO Empty */
#define	SSPA_CTL_JST		(1 << 3)	/* Audio Sample \
							Justification */
#define	SSPA_CTL_XFRLEN2_MASK	(7 << 24)
#define	SSPA_CTL_XFRLEN2(x)	((x) << 24)	/* Transmit Frame \
							Length in Phase 2 */
#define	SSPA_CTL_XWDLEN2_MASK	(7 << 21)
#define	SSPA_CTL_XWDLEN2(x)	((x) << 21)	/* Transmit Word \
							Length in Phase 2 */
#define	SSPA_CTL_XDATDLY(x)	((x) << 19)	/* Tansmit Data Delay */
#define	SSPA_CTL_XSSZ2_MASK	(7 << 16)
#define	SSPA_CTL_XSSZ2(x)	((x) << 16)	/* Transmit Sample \
							Audio Size */
#define	SSPA_CTL_XFRLEN1_MASK	(7 << 8)
#define	SSPA_CTL_XFRLEN1(x)	((x) << 8)	/* Transmit Frame \
							Length in Phase 1 */
#define	SSPA_CTL_XWDLEN1_MASK	(7 << 5)
#define	SSPA_CTL_XWDLEN1(x)	((x) << 5)	/* Transmit Word Length \
							in Phase 1 */
#define	SSPA_CTL_XSSZ1_MASK	(7 << 0)
#define	SSPA_CTL_XSSZ1(x)	((x) << 0)	/* XSSZ1 */

#define SSPA_CTL_8_BITS		(0x0)		/* Sample Size */
#define SSPA_CTL_12_BITS	(0x1)
#define SSPA_CTL_16_BITS	(0x2)
#define SSPA_CTL_20_BITS	(0x3)
#define SSPA_CTL_24_BITS	(0x4)
#define SSPA_CTL_32_BITS	(0x5)

/* SSPA Serial Port Register */
#define	SSPA_SP_WEN		(1 << 31)	/* Write Configuration \
								Enable */
#define	SSPA_SP_MSL		(1 << 18)	/* Master Slave \
							Configuration */
#define	SSPA_SP_CLKP		(1 << 17)	/* CLKP Polarity \
							Clock Edge Select */
#define	SSPA_SP_FSP		(1 << 16)	/* FSP Polarity \
							Clock Edge Select */
#define	SSPA_SP_FFLUSH		(1 << 2)	/* FIFO Flush */
#define	SSPA_SP_S_RST		(1 << 1)	/* Active High Reset Signal */
#define	SSPA_SP_S_EN		(1 << 0)	/* Serial Clock \
							Domain Enable */
#define	SSPA_SP_FWID(x)		((x) << 20)	/* Frame-Sync Width */
#define	SSPA_TXSP_FPER(x)	((x) << 4)	/* Frame-Sync Active */

/* SSPA and sysclk pll sources */
#define SSPA_AUDIO_PLL                          0
#define SSPA_I2S_PLL                            1
#define SSPA_VCXO_PLL                           2

#endif /* __ASM_MACH_REGS_AUDIO_H */
