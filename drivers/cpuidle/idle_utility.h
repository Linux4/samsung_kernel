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


#define BUCKETS 12

struct idle_prediction {
	u64 correction_factor[12];
	u64 predicted_us;
	int bucket;
	u64 latency_us;
	u64 expected_us;
};

int performance_multiplier(void);
int which_bucket(unsigned int duration);
void update_correction_factor(unsigned int measured_us,
		struct idle_prediction *idle,
		int resolution, int decay,
		int index);