/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARM_ARCH_GPIO_H
#define __ASM_ARM_ARCH_GPIO_H

/*
 * SC8825 GPIO&EIC bank and number summary:
 *
 * Bank	  From	  To	NR	Type
 * 1	  0   ~	  15		16	EIC
 * 2	  16  ~	  271	256	GPIO
 * 3	  272 ~	  287	16	ANA EIC
 * 4	  288 ~	  319	32	ANA GPIO
 */

#ifdef CONFIG_ARCH_SCX35
#define	D_GPIO_START	0
#define	D_GPIO_NR		256

#define	A_GPIO_START	( D_GPIO_START + D_GPIO_NR )
#if defined(CONFIG_ARCH_SCX15)
#define	A_GPIO_NR		0
#else
#define	A_GPIO_NR		32
#endif

#define	D_EIC_START		( A_GPIO_START + A_GPIO_NR)
#define	D_EIC_NR		16

#define	A_EIC_START		( D_EIC_START + D_EIC_NR )
#define	A_EIC_NR		16
#else
#define	D_EIC_START		0
#define	D_EIC_NR		16

#define	D_GPIO_START	( D_EIC_START + D_EIC_NR )
#define	D_GPIO_NR		256

#define	A_EIC_START		( D_GPIO_START + D_GPIO_NR )
#define	A_EIC_NR		16

#define	A_GPIO_START	( A_EIC_START + A_EIC_NR )
#define	A_GPIO_NR		32
#endif

#define ARCH_NR_GPIOS	( D_EIC_NR + D_GPIO_NR + A_EIC_NR + A_GPIO_NR )

#include <asm-generic/gpio.h>
#include <mach/irqs.h>

#define gpio_get_value  __gpio_get_value
#define gpio_set_value  __gpio_set_value
#define gpio_cansleep   __gpio_cansleep
#define gpio_to_irq     __gpio_to_irq


/* Digital GPIO/EIC base address */
#define CTL_GPIO_BASE          (SPRD_GPIO_BASE)
#define CTL_EIC_BASE           (SPRD_EIC_BASE)

/* Analog GPIO/EIC base address */

#if defined(CONFIG_ARCH_SCX35)
#define ANA_CTL_EIC_BASE	   (ANA_EIC_BASE)
#define ANA_CTL_GPIO_BASE      (ANA_GPIO_INT_BASE)
#else
#define ANA_CTL_EIC_BASE       (SPRD_MISC_BASE + 0x0100)
#define ANA_CTL_GPIO_BASE      (SPRD_MISC_BASE + 0x0480)
#endif


enum {
       ENUM_ID_D_GPIO = 0,
       ENUM_ID_D_EIC,
       ENUM_ID_A_GPIO,
       ENUM_ID_A_EIC,

       ENUM_ID_END_NR,
};

struct eic_gpio_resource {
       int irq;
       int base_addr;
       int chip_base;
       int chip_ngpio;
};

static inline int irq_to_gpio(int irq)
{
	return irq - GPIO_IRQ_START;
}

#endif
