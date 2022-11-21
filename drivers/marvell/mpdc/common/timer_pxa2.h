/*
 * ** (C) Copyright 2009 Marvell International Ltd.
 * **              All Rights Reserved
 *
 * ** This software file (the "File") is distributed by Marvell International Ltd.
 * ** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
 * ** You may use, redistribute and/or modify this File in accordance with the terms and
 * ** conditions of the License, a copy of which is available along with the File in the
 * ** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * ** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * ** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
 * ** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
 * ** The License provides additional details about this warranty disclaimer.
 * */

#ifndef __TIMER_PXA2_H__
#define __TIMER_PXA2_H__

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <mach/irqs.h>
#else
#include <asm/arch/pxa-regs.h>
#endif

/* Timer interrupt */
#define PX_IRQ_PXA2_TIMER IRQ_OST_4_11

#endif /* __TIMER_PXA2_H__ */
