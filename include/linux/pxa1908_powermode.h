/* Copyright 2013 Marvell Tech. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __PXA1908_POWERMODE_H
#define __PXA1908_POWERMODE_H

/*
 * Below definitions must align with pxa988_lowpower_state
 * in pxa988_lowpower.h.
 */
#define PM_QOS_CPUIDLE_BLOCK_C1		0
#define PM_QOS_CPUIDLE_BLOCK_C2		1
#define PM_QOS_CPUIDLE_BLOCK_M2		2
#define PM_QOS_CPUIDLE_BLOCK_AXI	3
#define PM_QOS_CPUIDLE_BLOCK_DDR	4
#define PM_QOS_CPUIDLE_BLOCK_UDR_VCTCXO	5
#define PM_QOS_CPUIDLE_BLOCK_UDR	6

#endif /* __PXA1908_POWERMODE_H */

