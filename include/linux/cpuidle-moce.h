/*
 * Copyright (c) 2018 Hong Hyunji, Samsung Electronics Co., Ltd <hyunji.hong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos MOCE(MOCE that can Diversify Conditions for Entry into Idle State) driver implementation
 * MOCE can vary the target residency and exit latency in accordance with the frequency
 */

typedef enum _factor_type {
	NO_FACTOR = -1,
	FREQ_FACTOR,
	/* Add enum value for new-factor */
} factor_type;

/*
 * Information about each factor that affects idle state entry,
 * initialized through the device tree.
 */
struct factor {
	/* list of factor */
	struct list_head	list;

	/* lock */
	spinlock_t		lock;

	/* factor type */
	factor_type		type;

	/* factor sibling-cpus */
	struct cpumask		cpus;

	/* factor ratio */
	unsigned int		size;
	unsigned int		ratio;
	unsigned int		*ratio_table;
};

/*
 * Information that is directly required to bias the entry conditions of the idle state,
 * and is managed by the per-cpu variable.
 */
struct bias_cpuidle {
	/* moce active or not */
	bool			biased;

	/*total ratio of factors */
	unsigned int		total_ratio;

	/* head of factor list */
	struct list_head	factor_list;
};

#ifdef CONFIG_ARM64_EXYNOS_MOCE
/* CPUIdle MOCE APIs */
extern unsigned int exynos_moce_get_ratio(unsigned int cpu);
#else
static inline unsigned int exynos_moce_get_ratio(unsigned int cpu)
{ return 100; }
#endif
