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

#ifndef __ASM_ARCH_SCI_IRQS_H
#define __ASM_ARCH_SCI_IRQS_H

#if defined(CONFIG_ARCH_SCX35)
#include "__irqs-sc8830.h"
#elif defined(CONFIG_ARCH_SC8825)
#include "__irqs-sc8825.h"
#else
#error "Unknown architecture specification"
#endif

#ifndef __ASSEMBLY__

/*raw interrupt intc interrupt enable */
void sci_intc_unmask(u32 irq);
/*raw interrupt intc interrupt disable */
void sci_intc_mask(u32 irq);

#endif

#endif
