/*
 * Power Management Routines
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 *
 * (C) Copyright 2011 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef __PM_H__
#define __PM_H__

#include <linux/pm_qos.h>

/* MMP2 cpuidle exit latency */
#define EXIT_LATENCY_CORE_EXTIDLE		1
#define EXIT_LATENCY_APPS_IDLE			10
#define EXIT_LATENCY_APPS_SLEEP			20
#define EXIT_LATENCY_CHIP_SLEEP			100

#ifdef CONFIG_CPU_PXA988
#define PM_QOS_CONSTRAINT 1
#elif defined(CONFIG_CPU_MMP3)
#define PM_QOS_CONSTRAINT PM_QOS_DEFAULT_VALUE
#define HDMI_FREQ_CONSTRAINT 200
#else
#define PM_QOS_CONSTRAINT PM_QOS_DEFAULT_VALUE
#define HDMI_FREQ_CONSTRAINT PM_QOS_DEFAULT_VALUE
#endif

static inline void pwr_i2c_conflict_mutex_lock(void) {}
static inline void pwr_i2c_conflict_mutex_unlock(void) {}

#endif
