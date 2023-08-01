/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/*
 * Return a multiplier for the exit latency that is intended
 * to take performance requirements into account.
 * The more performance critical we estimate the system
 * to be, the higher this multiplier, and thus the higher
 * the barrier to go to an expensive C state.
 */

#include <linux/sched.h>
#include "idle_utility.h"

int performance_multiplier(void)
{
	int mult = 1;

	/* for IO wait tasks (per cpu!) we add 5x each */
	mult += 10 * nr_iowait_cpu(smp_processor_id());

	return mult;
}


int which_bucket(unsigned int duration)
{
	int bucket = 0;

	/*
	 * We keep two groups of stats; one with no
	 * IO pending, one without.
	 * This allows us to calculate
	 * E(duration)|iowait
	 */
	if (nr_iowait_cpu(smp_processor_id()))
		bucket = BUCKETS/2;

	if (duration < 10)
		return bucket;
	if (duration < 100)
		return bucket + 1;
	if (duration < 1000)
		return bucket + 2;
	if (duration < 10000)
		return bucket + 3;
	if (duration < 100000)
		return bucket + 4;
	return bucket + 5;
}

void update_correction_factor(unsigned int measured_us,
		struct idle_prediction *cpu_pred,
		int resolution, int decay,
		int index)
{
	u64 new_factor;

	/*
	 * We correct for the exit latency; we are assuming here that the
	 * exit latency happens after the event that we're interested in.
	 */
	if (measured_us > cpu_pred->latency_us)
		measured_us -= cpu_pred->latency_us;

	/* update our correction ratio */

	new_factor = div_u64(cpu_pred->correction_factor[cpu_pred->bucket]
			* (decay - 1), decay);

	if ((cpu_pred->expected_us > 0) && (measured_us <
			50000))
		new_factor += div_u64(resolution * measured_us, cpu_pred->expected_us);
	else
		/*
		 * we were idle so long that we count it as a perfect
		 * prediction
		 */
		new_factor += resolution;

	trace_printk("C%d: Exp_us: %llu Pred: %llu Idle: %u idx:%d\n",
			 smp_processor_id(),
			cpu_pred->expected_us, cpu_pred->predicted_us,  measured_us, index);
	/*
	 * We don't want 0 as factor; we always want at least
	 * a tiny bit of estimated time.
	 */
	if (new_factor == 0)
		new_factor = 1;

	cpu_pred->correction_factor[cpu_pred->bucket] = new_factor;
}