/*
 * linux/include/pxa1936_powermode.h
 *
 * Author:	Xiaoguang Chen <chenxg@marvell.com>
 * Copyright:	(C) 2015 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __PXA1936_POWERMODE_H__
#define __PXA1936_POWERMODE_H__

	/* core level power modes */
#define POWER_MODE_CORE_INTIDLE    0	/* C12 */
#define POWER_MODE_CORE_EXTIDLE    1	/* C13 */
#define POWER_MODE_CORE_POWERDOWN  2	/* C22 */

	/* MP level power modes */
#define POWER_MODE_MP_IDLE_CORE_EXTIDLE	3	/* M11_C13 */
#define POWER_MODE_MP_IDLE_CORE_POWERDOWN	4	/* M11_C22 */
#define POWER_MODE_MP_POWERDOWN_L2_ON	5	/* M21 */
#define POWER_MODE_MP_POWERDOWN_L2_OFF	6	/* M22 */
#define POWER_MODE_MP_POWERDOWN POWER_MODE_MP_POWERDOWN_L2_OFF

	/* AP subsystem level power modes */
#define POWER_MODE_APPS_IDLE	7	/* D1P */
#define POWER_MODE_APPS_IDLE_DDR	8	/* D1PDDR */
#define POWER_MODE_APPS_SLEEP	9	/* D1PP */
#define POWER_MODE_APPS_SLEEP_UDR	10	/* D1PPSTDBY */

	/* chip level power modes */
#define POWER_MODE_SYS_SLEEP_VCTCXO	11	/* D1 */
#define POWER_MODE_SYS_SLEEP_VCTCXO_OFF	12/* D1 */
#define POWER_MODE_SYS_SLEEP POWER_MODE_SYS_SLEEP_VCTCXO_OFF
#define POWER_MODE_UDR_VCTCXO	13	/* D2 */
#define POWER_MODE_UDR_VCTCXO_OFF	14	/* D2PP */
#define POWER_MODE_UDR POWER_MODE_UDR_VCTCXO_OFF
#define POWER_MODE_MAX 15		/* maximum lowpower states */


#define PM_QOS_CPUIDLE_BLOCK_C1	POWER_MODE_CORE_INTIDLE
#define PM_QOS_CPUIDLE_BLOCK_C2	POWER_MODE_CORE_POWERDOWN
#define PM_QOS_CPUIDLE_BLOCK_M2	POWER_MODE_MP_POWERDOWN_L2_OFF
#define PM_QOS_CPUIDLE_BLOCK_AXI	POWER_MODE_APPS_IDLE
#define PM_QOS_CPUIDLE_BLOCK_DDR	POWER_MODE_SYS_SLEEP_VCTCXO_OFF
#define PM_QOS_CPUIDLE_BLOCK_UDR_VCTCXO	POWER_MODE_UDR_VCTCXO
#define PM_QOS_CPUIDLE_BLOCK_UDR	POWER_MODE_UDR

#endif /* __PXA1936_POWERMODE_H__ */
