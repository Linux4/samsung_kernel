/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#ifndef __PX_PMU_DEF_H__
#define __PX_PMU_DEF_H__

#include <linux/irq.h>

#ifdef PX_CPU_PXA2
#include "pmu_pxa2.h"
#endif

#ifdef PX_CPU_PJ1
#include "pmu_pj1.h"
#endif

#ifdef PX_CPU_PJ4
#include "pmu_pj4.h"
#endif

#ifdef PX_CPU_A9
#include "pmu_a9.h"
#endif

#ifdef PX_CPU_PJ4B
#include "pmu_pj4b.h"
#endif

typedef irqreturn_t (*pmu_isr_t)(int irq, void *dev_id);

extern int allocate_pmu(void);
extern int free_pmu(void);

extern int register_pmu_isr(pmu_isr_t pmu_isr);
extern int unregister_pmu_isr(void);

extern int set_cpu_irq_affinity(unsigned int cpu);

#endif /* __PX_PMU_DEF_H__ */
