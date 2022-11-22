/*
 * linux/arch/arm/mach-mmp/eden.c
 *
 * Copyright (C) 2009 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/ion.h>
#include <linux/memblock.h>

#include <asm/smp_twd.h>
#include <asm/mach/time.h>
#include <asm/hardware/gic.h>

#include <mach/gpio.h>
#include <mach/addr-map.h>
#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/cputype.h>
#include <mach/irqs.h>
#include <mach/dma.h>
#include <mach/devices.h>
#include <mach/eden.h>
#include <mach/regs-usb.h>
#ifdef CONFIG_UIO_HANTRO
#include <mach/soc_hantro.h>
#endif

#include <linux/memblock.h>
#include <linux/platform_device.h>

#include <plat/mfp.h>

#include "common.h"
#include "clock.h"

#define MFPR_VIRT_BASE	(APB_VIRT_BASE + 0x1e000)
#define GPIOE_VIRT_BASE	(APB_VIRT_BASE + 0x19800)

static struct mfp_addr_map eden_addr_map[] __initdata = {
	MFP_ADDR_X(GPIO0, GPIO58, 0x54),
	MFP_ADDR_X(GPIO59, GPIO73, 0x280),
	MFP_ADDR_X(GPIO74, GPIO101, 0x170),
	MFP_ADDR_X(GPIO102, GPIO103, 0x0),
	MFP_ADDR_X(GPIO115, GPIO122, 0x260),
	MFP_ADDR_X(GPIO124, GPIO141, 0xc),
	MFP_ADDR_X(GPIO143, GPIO151, 0x220),
	MFP_ADDR_X(GPIO152, GPIO153, 0x248),
	MFP_ADDR_X(GPIO154, GPIO155, 0x254),

	MFP_ADDR(GPIO142, 0x8),
	MFP_ADDR(GPIO114, 0x164),
	MFP_ADDR(GPIO123, 0x148),

	MFP_ADDR(GPIO168, 0x1e0),
	MFP_ADDR(GPIO167, 0x1e4),
	MFP_ADDR(GPIO166, 0x1e8),
	MFP_ADDR(GPIO165, 0x1ec),
	MFP_ADDR(GPIO107, 0x1f0),
	MFP_ADDR(GPIO106, 0x1f4),
	MFP_ADDR(GPIO105, 0x1f8),
	MFP_ADDR(GPIO104, 0x1fc),
	MFP_ADDR(GPIO111, 0x200),
	MFP_ADDR(GPIO164, 0x204),
	MFP_ADDR(GPIO163, 0x208),
	MFP_ADDR(GPIO162, 0x20c),
	MFP_ADDR(GPIO161, 0x210),
	MFP_ADDR(GPIO110, 0x214),
	MFP_ADDR(GPIO109, 0x218),
	MFP_ADDR(GPIO108, 0x21c),
	MFP_ADDR(GPIO110, 0x214),
	MFP_ADDR(GPIO112, 0x244),
	MFP_ADDR(GPIO160, 0x250),
	MFP_ADDR(GPIO113, 0x25c),
	MFP_ADDR(GPIO171, 0x2c8),

	MFP_ADDR_X(TWSI1_SCL, TWSI1_SDA, 0x140),
	MFP_ADDR_X(TWSI4_SCL, TWSI4_SDA, 0x2bc),
	MFP_ADDR(PMIC_INT, 0x2c4),
	MFP_ADDR(CLK_REQ, 0x160),

	MFP_ADDR_END,
};

void __init eden_reserve(void)
{
}

void __init eden_init_irq(void)
{
#if defined(CONFIG_GIC_BYPASS) /* Use ICU for Interrupt Handling */
#if defined(CONFIG_SMP)	/* Use GIC+ICU Mode */
	gic_init(0, 29, (void __iomem *) GIC_DIST_VIRT_BASE, (void __iomem *) GIC_CPU_VIRT_BASE);
#endif
	eden_init_icu_gic();
#else	/* GIC Only Mode */
	gic_init(0, 29, (void __iomem *) GIC_DIST_VIRT_BASE, (void __iomem *) GIC_CPU_VIRT_BASE);
	eden_init_gic();
#endif
}

static void __init eden_timer_init(void)
{
	uint32_t clk_rst;

	/* this is early, we have to initialize the CCU registers by
	 * ourselves instead of using clk_* API. Clock rate is defined
	 * by APBC_TIMERS_FNCLKSEL and enabled free-running
	 */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_MMP2_TIMERS);

	/* VCTCXO/4 = 6.5 MHz, bus/functional clock enabled, release reset */
	clk_rst = APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(1);
	__raw_writel(clk_rst, APBC_MMP2_TIMERS);

	timer_init(IRQ_EDEN_TIMER1);
}

struct sys_timer eden_timer = {
	.init   = eden_timer_init,
};

#define GC2D_CLK_EN			(1u << 15)
#define GC2D_CLK_RST		(1u << 14)
#define GC2D_ACLK_EN		(1u << 19)
#define GC2D_ACLK_RST		(1u << 18)

#define GC3D_CLK_EN			(1u << 3)
#define GC3D_CLK_RST		(1u << 1)
#define GC3D_ACLK_EN		(1u << 2)
#define GC3D_ACLK_RST		(1u << 0)

#define GC_PWRUP(n)		((n & 3) << 9)
#define GC_PWRUP_MSK		GC_PWRUP(3)
#define GC_ISB			(1u << 8)
#define APMU_ISLD_GC_CMEM_DMMYCLK_EN	(1 << 4)

static DEFINE_SPINLOCK(gc_lock);
static int gc_fab_cnt = 0;

void gc3d_pwr(unsigned int power_on)
{
	unsigned long regval;

	regval = __raw_readl(APMU_GC_CLK_RES_CTRL);
	if (power_on) {
		if (regval & (GC_PWRUP_MSK | GC_ISB))
			return; /*Pwr is already on*/

		/* increase using count of shared fabric */
		spin_lock(&gc_lock);
		gc_fab_cnt++;
		spin_unlock(&gc_lock);

		/* 1. slow ramp */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC_PWRUP(1);
		writel(regval, APMU_GC_CLK_RES_CTRL);
		mdelay(10);
		/* 2. power up */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC_PWRUP(3);
		writel(regval, APMU_GC_CLK_RES_CTRL);
		mdelay(10);
		/* 3. disable GC isolation */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC_ISB;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);
		/* 4. enable dummy clocks to SRAM */
		regval = readl(APMU_ISLD_GC_CTRL);
		regval |= APMU_ISLD_GC_CMEM_DMMYCLK_EN;
		writel(regval, APMU_ISLD_GC_CTRL);
		udelay(300);
		regval = readl(APMU_ISLD_GC_CTRL);
		regval &= ~APMU_ISLD_GC_CMEM_DMMYCLK_EN;
		writel(regval, APMU_ISLD_GC_CTRL);
		udelay(10);
		/* 5. enable peripheral clock */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC3D_CLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);
		/* 6. enable AXI clock */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC3D_ACLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);
		/* 7. de-assert pheriperal reset */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC3D_CLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);
		/* 8. de-assert AXI reset */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC3D_ACLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);

		/* 9. gate GC3D AXI clock if only GC3D is working */
		spin_lock(&gc_lock);
		if (1 == gc_fab_cnt) {
			regval = readl(APMU_GC_CLK_RES_CTRL);
			regval &= ~GC3D_ACLK_EN;
			writel(regval, APMU_GC_CLK_RES_CTRL);
		}
		spin_unlock(&gc_lock);

		/* 10. gate peripheral clock */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval &= ~GC3D_CLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL);
	} else {

		if ((regval & (GC_PWRUP_MSK | GC_ISB)) == 0)
			return; /*Pwr is already off*/

		/* decrease using count of shared fabric */
		spin_lock(&gc_lock);
		if (gc_fab_cnt)
			gc_fab_cnt--;
		spin_unlock(&gc_lock);

		/* 1. isolation */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval &= ~GC_ISB;
		writel(regval, APMU_GC_CLK_RES_CTRL);

		/* 2. pheriperal reset */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval &= ~GC3D_CLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL);

		/* 3. GC3D AXI reset */
		spin_lock(&gc_lock);
		if (gc_fab_cnt == 0) {
			regval = readl(APMU_GC_CLK_RES_CTRL);
			regval &= ~GC3D_ACLK_RST;
			writel(regval, APMU_GC_CLK_RES_CTRL);
		}
		spin_unlock(&gc_lock);

		/* 4. make sure clock disabled*/
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval &= ~GC3D_CLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		/* disable GC3D AXI clock */
		spin_lock(&gc_lock);
		if (gc_fab_cnt == 0) {
			regval = readl(APMU_GC_CLK_RES_CTRL);
			regval &= ~GC3D_ACLK_EN;
			writel(regval, APMU_GC_CLK_RES_CTRL);
		}
		spin_unlock(&gc_lock);

		/* 5. turn off power */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval &= ~GC_PWRUP_MSK;
		writel(regval, APMU_GC_CLK_RES_CTRL);
	}

}
EXPORT_SYMBOL_GPL(gc3d_pwr);

void gc2d_pwr(unsigned int power_on)
{
	unsigned long regval;

	if (power_on) {
		/* increase using count of shared fabric */
		spin_lock(&gc_lock);
		gc_fab_cnt++;
		spin_unlock(&gc_lock);

		/* 1. enable peripheral clock */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval |= GC2D_CLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
		udelay(10);
		/* 2a. enable GC2D AXI clock */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval |= GC2D_ACLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
		/* 2b. enable GC3D AXI clock since it's a shared fabric  */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC3D_ACLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);
		/* 3. de-assert pheriperal reset */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval |= GC2D_CLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
		udelay(10);
		/* 4a. de-assert GC2D AXI reset */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval |= GC2D_ACLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
		/* 4b. de-assert GC3D AXI reset since it's a shared fabric */
		regval = readl(APMU_GC_CLK_RES_CTRL);
		regval |= GC3D_ACLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL);
		udelay(10);

		/* 5a. gate GC2D AXI clock */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval &= ~GC2D_ACLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL2);

		/* 5b. gate GC3D AXI clock if only GC2D is working*/
		spin_lock(&gc_lock);
		if (1 == gc_fab_cnt) {
			regval = readl(APMU_GC_CLK_RES_CTRL);
			regval &= ~GC3D_ACLK_EN;
			writel(regval, APMU_GC_CLK_RES_CTRL);
		}
		spin_unlock(&gc_lock);

		/* 6. gate peripheral clock */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval &= ~GC2D_CLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
	} else {

		/* decrease using count of shared fabric */
		spin_lock(&gc_lock);
		if (gc_fab_cnt)
			gc_fab_cnt--;
		spin_unlock(&gc_lock);

		/* 1. pheriperal reset */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval &= ~GC2D_CLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL2);

		/* 2a. GC2D AXI reset */
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval &= ~GC2D_ACLK_RST;
		writel(regval, APMU_GC_CLK_RES_CTRL2);

		/* 2b. GC3D AXI reset */
		spin_lock(&gc_lock);
		if (gc_fab_cnt == 0) {
			regval = readl(APMU_GC_CLK_RES_CTRL);
			regval &= ~GC3D_ACLK_RST;
			writel(regval, APMU_GC_CLK_RES_CTRL);
		}
		spin_unlock(&gc_lock);

		/* 4. make sure clock disabled*/
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval &= ~GC2D_CLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
		regval = readl(APMU_GC_CLK_RES_CTRL2);
		regval &= ~GC2D_ACLK_EN;
		writel(regval, APMU_GC_CLK_RES_CTRL2);
		/* disable GC3D AXI clock */
		spin_lock(&gc_lock);
		if (gc_fab_cnt == 0) {
			regval = readl(APMU_GC_CLK_RES_CTRL);
			regval &= ~GC3D_ACLK_EN;
			writel(regval, APMU_GC_CLK_RES_CTRL);
		}
		spin_unlock(&gc_lock);

	}

}
EXPORT_SYMBOL_GPL(gc2d_pwr);

#ifdef CONFIG_MACH_EDEN_FPGA

#define GC_RST                  (1 << 1)
#define GC_AXI_RST              ((1 << 0) | (1 << 18))
#define GC_AXICLK_EN            ((1u << 2) | (1u << 19))
#define GC_CLK_EN               ((1u << 3) | (1u << 15))

void gc_pwr(int power_on)
{
        unsigned long regval;

        regval = __raw_readl(APMU_GC_CLK_RES_CTRL);
        if (power_on) {
                if (regval & (GC_PWRUP_MSK | GC_ISB))
                        return; /*Pwr is already on*/

                /* 1. slow ramp */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |= (0x1<<9);
                udelay(300);
                /* 2. power up */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |=(0x3 <<9);
                writel(regval, APMU_GC_CLK_RES_CTRL);
                udelay(300);
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |=(0x1<<15);  /* gc320 */
                regval |=(0x1<<3);  /*gc2100 */
                writel(regval, APMU_GC_CLK_RES_CTRL);
                udelay(300);
                /* 3. enable AXI clock */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |=(0x1 <<19);
                regval |=(0x1 <<2);
                writel(regval, APMU_GC_CLK_RES_CTRL);
                udelay(300);
                /* disable GC isolation */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |=(0x1 <<8);
                writel(regval, APMU_GC_CLK_RES_CTRL);
                udelay(300);
                /* 5. de-assert pheriperal reset */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |=(0x1 <<1);   /* GC2100 */
                regval |=(0x1 <<14);   /* Gc320 */
                writel(regval, APMU_GC_CLK_RES_CTRL);
                udelay(300);

                /* 6.de-assert AXI reset */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval |=(0x1 <<18);
                regval |=(0x1);
                writel(regval, APMU_GC_CLK_RES_CTRL);
                udelay(300);

                /* 7.   */
                regval = readl(APMU_ISLD_GC_CTRL);
                regval |= 0x10;
                writel(regval, APMU_ISLD_GC_CTRL);
                udelay(300);

                regval = readl(APMU_ISLD_GC_CTRL);
                regval &=0xFFFFFFEF;
                writel(regval, APMU_ISLD_GC_CTRL);
                udelay(300);
        } else { /*not touch yet for GC2100 */

                if ((regval & (GC_PWRUP_MSK | GC_ISB)) == 0)
                        return; /*Pwr is already off*/

                /* 1. isolation */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval &= ~GC_ISB;
                writel(regval, APMU_GC_CLK_RES_CTRL);

                /* 2. reset*/
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval &= ~GC_AXI_RST;
                writel(regval, APMU_GC_CLK_RES_CTRL);
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval &= ~GC_RST;
                writel(regval, APMU_GC_CLK_RES_CTRL);

                /* 3. make sure clock disabled*/
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval &= ~GC_AXICLK_EN;
                writel(regval, APMU_GC_CLK_RES_CTRL);
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval &= ~GC_CLK_EN;
                writel(regval, APMU_GC_CLK_RES_CTRL);

                /* 4. turn off power */
                regval = readl(APMU_GC_CLK_RES_CTRL);
                regval &= ~GC_PWRUP_MSK;
                writel(regval, APMU_GC_CLK_RES_CTRL);
        }
}
EXPORT_SYMBOL_GPL(gc_pwr);

#endif	/* end of CONFIG_MACH_EDEN_FPGA */

#ifdef CONFIG_UIO_HANTRO

#define VPU_PWRUP_SLOW_RAMP		(1 << 9)
#define VPU_PWRUP_ON			(3 << 9)
#define VPU_PWRUP_MASK			(3 << 9)
#define VPU_ISB					(1 << 8)
#define VPU_REDUN_START			(1 << 2)

#define VPU_DECODER_RST				(1 << 28)
#define VPU_DECODER_CLK_EN			(1 << 27)
#define VPU_DECODER_CLK_DIV_MASK	(7 << 24)
#define VPU_DECODER_CLK_DIV_SHIFT	24
#define VPU_DECODER_CLK_SEL_MASK	(3 << 22)
#define VPU_DECODER_CLK_SEL_SHIFT	22

#define VPU_AXI_RST					(1 << 0)
#define VPU_AXI_CLK_EN				(1 << 3)
#define VPU_AXI_CLK_DIV_MASK		(7 << 19)
#define VPU_AXI_CLK_DIV_SHIFT		19
#define VPU_AXI_CLK_SEL_MASK		(3 << 12)
#define VPU_AXI_CLK_SEL_SHIFT		12

#define VPU_ENCODER_RST				(1 << 1)
#define VPU_ENCODER_CLK_EN			(1 << 4)
#define VPU_ENCODER_CLK_DIV_MASK	(7 << 16)
#define VPU_ENCODER_CLK_DIV_SHIFT	16
#define VPU_ENCODER_CLK_SEL_MASK	(3 << 6)
#define VPU_ENCODER_CLK_SEL_SHIFT	6

#define APMU_ISLD_VPU_CMEM_DMMYCLK_EN	(1 << 4)

void hantro_power_switch(unsigned int enable)
{
	unsigned int reg = 0;
	unsigned int timeout;
	static unsigned int count = 0;

	reg = readl(APMU_VPU_CLK_RES_CTRL);
	if (enable && count++ == 0) {
		if (reg & (VPU_PWRUP_ON | VPU_ISB))
			return; /*Pwr is already on*/

		/* 1. Turn on power switches */
		reg &= VPU_PWRUP_MASK;
		reg |= VPU_PWRUP_SLOW_RAMP;
		writel(reg, APMU_VPU_CLK_RES_CTRL);
		udelay(10);

		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_PWRUP_ON;
		writel(reg, APMU_VPU_CLK_RES_CTRL);
		udelay(10);

		/* 2. deassert isolation*/
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_ISB;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 3a. Assert the redundancy repair signal */
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_REDUN_START;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 3b. Poll and wait the REDUN_START bit back to 0*/
		timeout = 1000;
		do {
			if (--timeout == 0) {
				WARN(1, "hantro: REDUN_START timeout!\n");
				return;
			}
			udelay(100);
			reg = readl(APMU_VPU_CLK_RES_CTRL);
		} while (reg & VPU_REDUN_START);

		/* 4. enable dummy clks to the SRAM */
		reg = readl(APMU_ISLD_VPU_CTRL);
		reg |= APMU_ISLD_VPU_CMEM_DMMYCLK_EN;
		writel(reg, APMU_ISLD_VPU_CTRL);
		udelay(10);
		reg = readl(APMU_ISLD_VPU_CTRL);
		reg &= ~APMU_ISLD_VPU_CMEM_DMMYCLK_EN;
		writel(reg, APMU_ISLD_VPU_CTRL);

		/* 5. enable AXI clock */
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_AXI_CLK_EN;
		reg &= ~VPU_AXI_CLK_SEL_MASK;
		reg &= ~VPU_AXI_CLK_DIV_MASK;
		reg |= (0x0) << VPU_AXI_CLK_SEL_SHIFT;
		reg |= (0x4) << VPU_AXI_CLK_DIV_SHIFT;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 6a. enable encoder clk */
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_ENCODER_CLK_EN;
		reg &= ~VPU_ENCODER_CLK_SEL_MASK;
		reg &= ~VPU_ENCODER_CLK_DIV_MASK;
		reg |= (0x0) << VPU_ENCODER_CLK_SEL_SHIFT;
		reg |= (0x4) << VPU_ENCODER_CLK_DIV_SHIFT;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 6b. enable decoder clock */
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_DECODER_CLK_EN;
		reg &= ~VPU_DECODER_CLK_SEL_MASK;
		reg &= ~VPU_DECODER_CLK_DIV_MASK;
		reg |= (0x0) << VPU_DECODER_CLK_SEL_SHIFT;
		reg |= (0x4) << VPU_DECODER_CLK_DIV_SHIFT;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 7. deassert AXI reset*/
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_AXI_RST;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 8a. deassert encoder reset*/
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_ENCODER_RST;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 8b. deassert decoder reset*/
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg |= VPU_DECODER_RST;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		/* 9. gate clocks */
		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg &= ~VPU_DECODER_CLK_EN;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg &= ~VPU_ENCODER_CLK_EN;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		reg = readl(APMU_VPU_CLK_RES_CTRL);
		reg &= ~VPU_AXI_CLK_EN;
		writel(reg, APMU_VPU_CLK_RES_CTRL);

		pr_info("%s, power on\n", __func__);

	} else if (0 == enable) {
		if (count == 0)
			return;
		if (--count == 0) {

			if ((reg & (VPU_PWRUP_ON | VPU_ISB)) == 0)
				return; /*Pwr is already off*/

			/* 1. isolation */
			reg &= ~VPU_ISB;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			/* 2. reset*/
			reg = readl(APMU_VPU_CLK_RES_CTRL);
			reg &= ~VPU_AXI_RST;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			reg = readl(APMU_VPU_CLK_RES_CTRL);
			reg &= ~VPU_ENCODER_RST;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			reg = readl(APMU_VPU_CLK_RES_CTRL);
			reg &= ~VPU_DECODER_RST;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			/* 3. make sure clock disabled*/
			reg = readl(APMU_VPU_CLK_RES_CTRL);
			reg &= ~VPU_AXI_CLK_EN;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			reg = readl(APMU_VPU_CLK_RES_CTRL);
			reg &= ~VPU_ENCODER_CLK_EN;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			reg = readl(APMU_VPU_CLK_RES_CTRL);
			reg &= ~VPU_DECODER_CLK_EN;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			/* 4. turn off power */
			reg &= ~VPU_PWRUP_ON;
			writel(reg, APMU_VPU_CLK_RES_CTRL);

			pr_info("%s, power off\n", __func__);
		}
	}
}
#endif

#ifdef CONFIG_ION
static struct ion_platform_data ion_data = {
	.nr	= 2,
	.heaps	= {
		[0] = {
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.id	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "carveout_heap",
		},
		[1] = {
			.type	= ION_HEAP_TYPE_SYSTEM,
			.id	= ION_HEAP_TYPE_SYSTEM,
			.name	= "system_heap",
		},
	},
};

struct platform_device device_ion = {
	.name	= "pxa-ion",
	.id	= -1,
	.dev	= {
		.platform_data = &ion_data,
	},
};

static int __init ion_mem_carveout(char *p)
{
	struct ion_platform_data *ip = &ion_data;
	unsigned long size;
	phys_addr_t start;
	char *endp;
	int i;

	size  = memparse(p, &endp);
	if (*endp == '@')
		start = memparse(endp + 1, NULL);
	else
		BUG_ON(1);

	pr_info("ION carveout memory: 0x%08lx@0x%08lx\n",
		size, (unsigned long)start);
	/* set the carveout heap range */
	ion_data.heaps[0].size = size;
	ion_data.heaps[0].base = start;

	for (i = 0; i < ip->nr; i++)
		BUG_ON(memblock_reserve(ip->heaps[i].base, ip->heaps[i].size));

	return 0;
}
early_param("ioncarv", ion_mem_carveout);
#endif

static struct sram_platdata eden_isram_info = {
	.pool_name = "isram",
	.granularity = SRAM_GRANULARITY,
};

static int __init eden_init(void)
{
	mfp_init_base(MFPR_VIRT_BASE);
	mfp_init_addr(eden_addr_map);

	pxa_init_dma(IRQ_EDEN_DMA_RIQ, 16);

	/* add gpio device */
	platform_device_register(&eden_device_gpio);
#ifdef CONFIG_ION
	platform_device_register(&device_ion);
#endif

	eden_add_isram(&eden_isram_info);

#ifdef CONFIG_UIO_HANTRO
	pxa_register_hantro();
#endif

	return 0;
}

postcore_initcall(eden_init);

/* on-chip devices */
EDEN_DEVICE(uart1, "pxa2xx-uart", 0, UART1, 0xd4030000, 0x30, 4, 5);
EDEN_DEVICE(uart2, "pxa2xx-uart", 1, UART2, 0xd4017000, 0x30, 20, 21);
EDEN_DEVICE(uart3, "pxa2xx-uart", 2, UART3, 0xd4018000, 0x30, 22, 23);
EDEN_DEVICE(ssp1, "eden-ssp", 1, SSP1, 0xd4035000, 0x40, 6, 7);
EDEN_DEVICE(ssp2, "eden-ssp", 2, SSP2, 0xd4036000, 0x40, 10, 11);
EDEN_DEVICE(ssp3, "eden-ssp", 3, SSP3, 0xd4037000, 0x40, 12, 13);
EDEN_DEVICE(fb, "pxa168-fb", 0, LCD, 0xd420b000, 0x500);
EDEN_DEVICE(keypad, "pxa27x-keypad", -1, KEYPAD, 0xd4012000, 0x4c);
EDEN_DEVICE(fb_ovly, "pxa168fb_ovly", 0, LCD, 0xd420b000, 0x500);
EDEN_DEVICE(sdh0, "sdhci-pxav3", 0, MMC, 0xd4280000, 0x120);
EDEN_DEVICE(sdh1, "sdhci-pxav3", 1, MMC2, 0xd4280800, 0x120);
EDEN_DEVICE(sdh2, "sdhci-pxav3", 2, MMC3, 0xd4217000, 0x120);
EDEN_DEVICE(sdh3, "sdhci-pxav3", 3, MMC4, 0xd4217800, 0x120);
EDEN_DEVICE(twsi1, "pxa2xx-i2c", 0, TWSI1, 0xd4011000, 0x70);
EDEN_DEVICE(twsi2, "pxa2xx-i2c", 1, TWSI2, 0xd4031000, 0x70);
EDEN_DEVICE(twsi3, "pxa2xx-i2c", 2, TWSI3, 0xd4032000, 0x70);
EDEN_DEVICE(twsi4, "pxa2xx-i2c", 3, TWSI4, 0xd4033000, 0x70);
EDEN_DEVICE(twsi5, "pxa2xx-i2c", 4, TWSI5, 0xd4033800, 0x70);
EDEN_DEVICE(twsi6, "pxa2xx-i2c", 5, TWSI6, 0xd4034000, 0x70);
EDEN_DEVICE(isram, "isram", 1, NONE, 0xd1020000, 0x10000);

struct resource eden_resource_gpio[] = {
	{
		.start	= 0xd4019000,
		.end	= 0xd4019fff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_EDEN_GPIO,
		.end	= IRQ_EDEN_GPIO,
		.name	= "gpio_mux",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device eden_device_gpio = {
	.name		= "pxa-gpio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(eden_resource_gpio),
	.resource	= eden_resource_gpio,
};

#ifdef CONFIG_RTC_DRV_SA1100
static struct resource eden_resource_rtc[] = {
	{ 0xd4010000, 0xd40100ff, NULL, IORESOURCE_MEM, },
	{ IRQ_EDEN_RTC, IRQ_EDEN_RTC, "rtc 1Hz", IORESOURCE_IRQ, },
	{ IRQ_EDEN_RTC_ALARM, IRQ_EDEN_RTC_ALARM, "rtc alarm", IORESOURCE_IRQ, },
};

struct platform_device eden_device_rtc = {
	.name           = "sa1100-rtc",
	.id             = -1,
	.resource       = eden_resource_rtc,
	.num_resources  = ARRAY_SIZE(eden_resource_rtc),
};
#endif

#if defined(CONFIG_TOUCHSCREEN_VNC)
struct platform_device eden_device_vnc_touch = {
	.name   = "vnc-ts",
	.id     = -1,
};
#endif

void eden_clear_keypad_wakeup(void)
{
	uint32_t val;
	uint32_t mask = (1 << 5);

	/* wake event clear is needed in order to clear keypad interrupt */
	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val | mask, APMU_WAKE_CLR);
}
