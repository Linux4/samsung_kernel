/*
 * drivers/cpufreq/cpufreq_limit.c
 *
 * Remade according to cpufreq change
 * (refer to commit df0eea4488081e0698b0b58ccd1e8c8823e22841
 *                 18c49926c4bf4915e5194d1de3299c0537229f9f)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_CPUFREQ_LIMIT_H__
#define __LINUX_CPUFREQ_LIMIT_H__

/* sched boost type from kernel/sched/sched.h */
extern int sched_set_boost(int enable);
#define NO_BOOST (CONSERVATIVE_BOOST * -1)
#define FULL_THROTTLE_BOOST 1
#define CONSERVATIVE_BOOST 2
#define RESTRAINED_BOOST 3

/* voltage based freq table */
#define NUM_THM_CPUS	2
#define NUM_MAX_FREQS	20

enum {
	CFLM_USERSPACE		= 0,	/* user(/sys/power/cpufreq*limit) */
	CFLM_TOUCH		= 1,	/* touch */
	CFLM_FINGER		= 2,	/* fingerprint */
	CFLM_ARGOS		= 3,	/* argos */

	CFLM_MAX_ITEM
};

#endif /* __LINUX_CPUFREQ_LIMIT_H__ */
