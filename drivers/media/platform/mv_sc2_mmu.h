/*
 * Copyright (C) 2011 Marvell International Ltd.
 *				 Libin Yang <lbyang@marvell.com>
 *
 * This driver is based on soc_camera.
 * Ported from Jonathan's marvell-ccic driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MV_SC2_MMU_H
#define __MV_SC2_MMU_H

#include <linux/io.h>
#include <linux/delay.h>
#include <media/mv_sc2.h>

/* channel n */
#define REG_SC2_DSA(n)		(0x0000 + 0x80 * (n))
#define REG_SC2_DSZ(n)		(0x0004 + 0x80 * (n))
#define REG_SC2_SVA(n)		(0x0008 + 0x80 * (n))
#define REG_SC2_CTRL(n)		(0x000c + 0x80 * (n))
#define REG_SC2_IRQSRC(n)	(0x0010 + 0x80 * (n))
#define REG_SC2_IRQENA(n)	(0x0014 + 0x80 * (n))
#define REG_SC2_IRQSTAT(n)	(0x0018 + 0x80 * (n))

/* channel 0 */
#define REG_SC2_DSA0		0x0000
#define REG_SC2_DSZ0		0x0004
#define REG_SC2_SVA0		0x0008
#define REG_SC2_CTRL0		0x000c
#define REG_SC2_IRQSRC0		0x0010
#define REG_SC2_IRQENA0		0x0014
#define REG_SC2_IRQSTAT0	0x0018
/* channel 1 */
#define REG_SC2_DSA1		0x0080
#define REG_SC2_DSZ1		0x0084
#define REG_SC2_SVA1		0x0088
#define REG_SC2_CTRL1		0x008c
#define REG_SC2_IRQSRC1		0x0090
#define REG_SC2_IRQENA1		0x0094
#define REG_SC2_IRQSTAT1	0x0098
/* channel 2 */
#define REG_SC2_DSA2		0x0100
#define REG_SC2_DSZ2		0x0104
#define REG_SC2_SVA2		0x0108
#define REG_SC2_CTRL2		0x010c
#define REG_SC2_IRQSRC2		0x0110
#define REG_SC2_IRQENA2		0x0114
#define REG_SC2_IRQSTAT2	0x0118
/* channel 3 */
#define REG_SC2_DSA3		0x0180
#define REG_SC2_DSZ3		0x0184
#define REG_SC2_SVA3		0x0188
#define REG_SC2_CTRL3		0x018c
#define REG_SC2_IRQSRC3		0x0190
#define REG_SC2_IRQENA3		0x0194
#define REG_SC2_IRQSTAT3	0x0198
/* channel 4 */
#define REG_SC2_DSA4		0x0200
#define REG_SC2_DSZ4		0x0204
#define REG_SC2_SVA4		0x0208
#define REG_SC2_CTRL4		0x020c
#define REG_SC2_IRQSRC4		0x0210
#define REG_SC2_IRQENA4		0x0214
#define REG_SC2_IRQSTAT4	0x0218
/* channel 5 */
#define REG_SC2_DSA5		0x0280
#define REG_SC2_DSZ5		0x0284
#define REG_SC2_SVA5		0x0288
#define REG_SC2_CTRL5		0x028c
#define REG_SC2_IRQSRC5		0x0290
#define REG_SC2_IRQENA5		0x0294
#define REG_SC2_IRQSTAT5	0x0298
/* channel 6 */
#define REG_SC2_DSA6		0x0300
#define REG_SC2_DSZ6		0x0304
#define REG_SC2_SVA6		0x0308
#define REG_SC2_CTRL6		0x030c
#define REG_SC2_IRQSRC6		0x0310
#define REG_SC2_IRQENA6		0x0314
#define REG_SC2_IRQSTAT6	0x0318
/* channel 7 */
#define REG_SC2_DSA7		0x0380
#define REG_SC2_DSZ7		0x0384
#define REG_SC2_SVA7		0x0388
#define REG_SC2_CTRL7		0x038c
#define REG_SC2_IRQSRC7		0x0390
#define REG_SC2_IRQENA7		0x0394
#define REG_SC2_IRQSTAT7	0x0398
/* channel 8 */
#define REG_SC2_DSA8		0x0400
#define REG_SC2_DSZ8		0x0404
#define REG_SC2_SVA8		0x0408
#define REG_SC2_CTRL8		0x040c
#define REG_SC2_IRQSRC8		0x0410
#define REG_SC2_IRQENA8		0x0414
#define REG_SC2_IRQSTAT8	0x0418
/* channel 9 */
#define REG_SC2_DSA9		0x0480
#define REG_SC2_DSZ9		0x0484
#define REG_SC2_SVA9		0x0488
#define REG_SC2_CTRL9		0x048c
#define REG_SC2_IRQSRC9		0x0490
#define REG_SC2_IRQENA9		0x0494
#define REG_SC2_IRQSTAT9	0x0498
/* channel 10 */
#define REG_SC2_DSA10		0x0500
#define REG_SC2_DSZ10		0x0504
#define REG_SC2_SVA10		0x0508
#define REG_SC2_CTRL10		0x050c
#define REG_SC2_IRQSRC10	0x0510
#define REG_SC2_IRQENA10	0x0514
#define REG_SC2_IRQSTAT10	0x0518
/* channel 11 */
#define REG_SC2_DSA11		0x0580
#define REG_SC2_DSZ11		0x0584
#define REG_SC2_SVA11		0x0588
#define REG_SC2_CTRL11		0x058c
#define REG_SC2_IRQSRC11	0x0590
#define REG_SC2_IRQENA11	0x0594
#define REG_SC2_IRQSTAT11	0x0598
/* channel 12 */
#define REG_SC2_DSA12		0x0600
#define REG_SC2_DSZ12		0x0604
#define REG_SC2_SVA12		0x0608
#define REG_SC2_CTRL12		0x060c
#define REG_SC2_IRQSRC12	0x0610
#define REG_SC2_IRQENA12	0x0614
#define REG_SC2_IRQSTAT12	0x0618
/* channel 13 */
#define REG_SC2_DSA13		0x0680
#define REG_SC2_DSZ13		0x0684
#define REG_SC2_SVA13		0x0688
#define REG_SC2_CTRL13		0x068c
#define REG_SC2_IRQSRC13	0x0690
#define REG_SC2_IRQENA13	0x0694
#define REG_SC2_IRQSTAT13	0x0698
/* channel 14 */
#define REG_SC2_DSA14		0x0700
#define REG_SC2_DSZ14		0x0704
#define REG_SC2_SVA14		0x0708
#define REG_SC2_CTRL14		0x070c
#define REG_SC2_IRQSRC14	0x0710
#define REG_SC2_IRQENA14	0x0714
#define REG_SC2_IRQSTAT14	0x0718
/* channel 15 */
#define REG_SC2_DSA15		0x0780
#define REG_SC2_DSZ15		0x0784
#define REG_SC2_SVA15		0x0788
#define REG_SC2_CTRL15		0x078c
#define REG_SC2_IRQSRC15	0x0790
#define REG_SC2_IRQENA15	0x0794
#define REG_SC2_IRQSTAT15	0x0798

/* global registers */
#define REG_SC2_GCTRL		0x0800
#define REG_SC2_GIRQ_SRC	0x0810
#define REG_SC2_GIRQ_ENA	0x0814
#define REG_SC2_GIRQ_STAT	0x0818
#define REG_SC2_GMISC_CTRL	0x081c
#define REG_SC2_CHDIS_DONE	0x0820
#define REG_SC2_AXICLK_CG	0x083c
/* bit defination in registers */
/* REG_SC2_CTRLx */
#define CCTRL_CH_ID(x)			(x & 0x000703FF)
/* REG_SC2_IRQSRCx */
#define CIRQSRC_CH_IRQSRC(x)	(x)
/* REG_SC2_IRQENAx */
/* REG_SC2_IRQSTATx */
#define CIRQ_ERR_HIGH		5
#define CIRQ_ALL		((1 << CIRQ_ERR_HIGH) - 1)
/* REG_SC2_GCTRL */
#define GCTRL_CH_ENA(x)			(0x1 << (x))
/* REG_SC2_IRQ_SRC */
#define GSRC_MUL_MATCH_SRC		(0x1 << 16)
/* REG_SC2_GIRQ_ENA */
#define GENA_MAIN_IRQ_ENA	(0x1 << 31)
#define GENA_MUL_MATCH_ENA	(0x1 << 16)
#define GIRQ_ALL		(GENA_MAIN_IRQ_ENA | GENA_MUL_MATCH_ENA)
/* REG_SC2_GIRQ_STAT */
#define GSTAT_MUL_MATCH_STAT	(0x1 << 16)
#define GSTAT_CH_IRQ_STAT(x)	(0x1 << (x))
/* REG_SC2_GMISC_CTRL */
#define GMCTRL_BYP_TAR			(0x1 << 2)
#define GMCTRL_BYP_TAW			(0x1 << 1)
#define GMCTRL_SOFT_RST			(0x1 << 0)

#define u32 unsigned int

static inline u32 msc2_mmu_reg_read(struct msc2_mmu_dev *sc2_dev,
						unsigned int reg)
{
	return ioread32(sc2_dev->mmu_regs + reg);
}

static inline void msc2_mmu_reg_write(struct msc2_mmu_dev *sc2_dev,
						unsigned int reg, u32 val)
{
	iowrite32(val, sc2_dev->mmu_regs + reg);
}

static inline void msc2_mmu_reg_write_mask(struct msc2_mmu_dev *sc2_dev,
					unsigned int reg, u32 val, u32 mask)
{
	u32 v;

	v = msc2_mmu_reg_read(sc2_dev, reg);
	v = (v & ~mask) | (val & mask);
	msc2_mmu_reg_write(sc2_dev, reg, v);
}

static inline void msc2_mmu_reg_set_bit(struct msc2_mmu_dev *sc2_dev,
					unsigned int reg, u32 val)
{
	msc2_mmu_reg_write_mask(sc2_dev, reg, val, val);
}

static inline void msc2_mmu_reg_clear_bit(struct msc2_mmu_dev *sc2_dev,
					unsigned int reg, u32 val)
{
	msc2_mmu_reg_write_mask(sc2_dev, reg, 0, val);
}

/* reset sc2 mmu HW */
static inline void msc2_mmu_reset(struct msc2_mmu_dev *sc2_dev)
{
	msc2_mmu_reg_set_bit(sc2_dev, REG_SC2_GMISC_CTRL, GMCTRL_SOFT_RST);
	udelay(50);
	msc2_mmu_reg_clear_bit(sc2_dev, REG_SC2_GMISC_CTRL, GMCTRL_SOFT_RST);
}

/* Bypass sc2 mmu read */
static inline void msc2_mmu_read_disable(struct msc2_mmu_dev *sc2_dev)
{
	msc2_mmu_reg_clear_bit(sc2_dev, REG_SC2_GMISC_CTRL, GMCTRL_BYP_TAR);
}

/* Bypass sc2 mmu write */
static inline void msc2_mmu_write_disable(struct msc2_mmu_dev *sc2_dev)
{
	msc2_mmu_reg_clear_bit(sc2_dev, REG_SC2_GMISC_CTRL, GMCTRL_BYP_TAW);
}

/* No-Bypass sc2 mmu read */
static inline void msc2_mmu_read_enable(struct msc2_mmu_dev *sc2_dev)
{
	msc2_mmu_reg_set_bit(sc2_dev, REG_SC2_GMISC_CTRL, GMCTRL_BYP_TAR);
}

/* No-Bypass sc2 mmu write */
static inline void msc2_mmu_write_enable(struct msc2_mmu_dev *sc2_dev)
{
	msc2_mmu_reg_set_bit(sc2_dev, REG_SC2_GMISC_CTRL, GMCTRL_BYP_TAW);
}

/* get sc2 mmu read bypass or not */
static inline int msc2_mmu_read_bypassed(struct msc2_mmu_dev *sc2_dev)
{
	u32 val;

	val = msc2_mmu_reg_read(sc2_dev, REG_SC2_GMISC_CTRL);
	return val & GMCTRL_BYP_TAR;
}

/* get sc2 mmu read bypass or not */
static inline int msc2_mmu_write_bypassed(struct msc2_mmu_dev *sc2_dev)
{
	u32 val;

	val = msc2_mmu_reg_read(sc2_dev, REG_SC2_GMISC_CTRL);
	return val & GMCTRL_BYP_TAW;
}

/* sc2 mmu enable channel*/
static inline void msc2_mmu_enable_ch(struct msc2_mmu_dev *sc2_dev, int ch)
{
	msc2_mmu_reg_set_bit(sc2_dev, REG_SC2_GCTRL, GCTRL_CH_ENA(ch));
}

/* sc2 mmu disable channel*/
static inline void msc2_mmu_disable_ch(struct msc2_mmu_dev *sc2_dev, int ch)
{
	msc2_mmu_reg_clear_bit(sc2_dev, REG_SC2_GCTRL, GCTRL_CH_ENA(ch));
}

static inline int msc2_mmu_disable_done(struct msc2_mmu_dev *sc2_dev, int ch)
{
	u32 val;

	val = msc2_mmu_reg_read(sc2_dev, REG_SC2_CHDIS_DONE);
	return val & GCTRL_CH_ENA(ch);
}

static inline int msc2_ch_invalid(struct msc2_mmu_dev *sc2_dev, unsigned int ch)
{
	if (ch >= SC2_CH_NUM) {
		dev_err(sc2_dev->dev, "channel ID exceeds %d", SC2_CH_NUM);
		return 1;
	} else
		return 0;
}

static inline int msc2_mmu_ch_set_daddr(struct msc2_mmu_dev *sc2_dev,
						int ch, dma_addr_t addr)
{
	if (unlikely(msc2_ch_invalid(sc2_dev, ch)))
		return -EINVAL;

	/* only support 32bit */
	msc2_mmu_reg_write(sc2_dev, REG_SC2_DSA(ch), (u32)addr);
	return 0;
}

static inline int msc2_mmu_ch_set_dsize(struct msc2_mmu_dev *sc2_dev,
						int ch, int size)
{
	if (unlikely(msc2_ch_invalid(sc2_dev, ch)))
		return -EINVAL;
	msc2_mmu_reg_write_mask(sc2_dev, REG_SC2_DSZ(ch), size, 0x3ffff);
	return 0;
}

/* The src vitrual address must be the same with the bus address */
static inline int msc2_mmu_ch_set_sva(struct msc2_mmu_dev *sc2_dev,
						int ch, void *addr)
{
	unsigned long tmp = (unsigned long)addr & 0xffffffff;
	u32 vaddr = (u32)tmp;

	if (unlikely(msc2_ch_invalid(sc2_dev, ch)))
		return -EINVAL;
	/* only support 32bit */
	msc2_mmu_reg_write(sc2_dev, REG_SC2_SVA(ch), vaddr);
	return 0;
}

static inline void msc2_mmu_ch_enable_irq(struct msc2_mmu_dev *sc2_dev,
						int ch)
{
	msc2_mmu_reg_write(sc2_dev, REG_SC2_IRQENA(ch), CIRQ_ALL);
}

static inline void msc2_mmu_enable_irq(struct msc2_mmu_dev *sc2_dev)
{
	msc2_mmu_reg_write(sc2_dev, REG_SC2_GIRQ_ENA, GIRQ_ALL);
}

#endif
