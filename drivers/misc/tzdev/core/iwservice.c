/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
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

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/sched/clock.h>
#include <soc/samsung/debug-snapshot.h>
#include "tzdev_internal.h"
#include "core/log.h"
#include "core/iwio.h"

struct iw_service_channel {
	uint32_t cpu_mask;
	uint32_t user_cpu_mask;
	uint32_t cpu_freq[NR_CPUS];
} __attribute__((__packed__));

static struct iw_service_channel *iw_channel;
static uint64_t cpu_offtime[NR_CPUS];
#define MID_START_CORE_NUM 4
#define DIFF_NS_SEC 1000000000L

extern unsigned int tzdev_is_up(void);

static int tz_iwservice_cpu_up_callback(unsigned int cpu)
{	
	if(cpu >= MID_START_CORE_NUM) {
		dbg_snapshot_printk("TZ_IWSERVICE : HP %d cpu_up clear time\n", cpu);
		cpu_offtime[cpu] = 0;	
	}

	return 0;
}

static int tz_iwservice_cpu_down_callback(unsigned int cpu)
{
	if(cpu >= MID_START_CORE_NUM) {
		cpu_offtime[cpu] = local_clock();
		dbg_snapshot_printk("TZ_IWSERVICE : HP %d cpu_down set time %lld\n", cpu, cpu_offtime[cpu]);		
	}	

	return 0;
}

static void tz_iwservice_register_hotplug_callback(void)
{	
	cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
					"teegris:online",
					tz_iwservice_cpu_up_callback, NULL);
	
	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
					"teegris:offline",
					NULL, tz_iwservice_cpu_down_callback);
}

static uint32_t tz_iwservice_check_swd_idle_mask(uint32_t cpu_mask)
{
	uint32_t mask = cpu_mask;
	uint64_t timediff;
	int i;

	for(i = MID_START_CORE_NUM; i < NR_CPUS; i++) {
		if(((1 << i) & mask) && cpu_offtime[i]) {
			timediff = local_clock() - cpu_offtime[i];
			if(timediff > DIFF_NS_SEC) {
				printk("TZ_IWSERVICE : clear cpu %d mask(hp out time %lld)\n", i, timediff);
				dbg_snapshot_printk("TZ_IWSERVICE : ---(sleep time %lld check core %d)Check mask diff pre_mask %x\n", timediff, i, mask);
				mask &= ~(1 << i);
				dbg_snapshot_printk("TZ_IWSERVICE : +++(check core %d)Check mask diff after %x\n", i, mask);
			}
		}
	}

	return mask;
}

static void tz_iwservice_should_retry(void)
{
	if(!tzdev_is_up())
		return;

	if(!iw_channel->cpu_mask && !iw_channel->user_cpu_mask)
		tzdev_smc_schedule();
}

unsigned long tz_iwservice_get_cpu_mask_in_atomic_preempt(void)
{
	uint32_t cal_mask;
	
	if (!iw_channel)
		return 0;

	cal_mask = tz_iwservice_check_swd_idle_mask(iw_channel->cpu_mask);

	return (cal_mask | iw_channel->user_cpu_mask);
}

unsigned long tz_iwservice_get_cpu_mask(void)
{
	uint32_t cal_mask;
	if (!iw_channel)
		return 0;

	cal_mask = tz_iwservice_check_swd_idle_mask(iw_channel->cpu_mask);
	tz_iwservice_should_retry();
	

	return (cal_mask | iw_channel->user_cpu_mask);
}

unsigned long tz_iwservice_get_user_cpu_mask(void)
{
	if (!iw_channel)
		return 0;

	return iw_channel->user_cpu_mask;
}

void tz_iwservice_set_cpu_freq(unsigned int cpu, unsigned int cpu_freq)
{
	if (!iw_channel)
		return;

	iw_channel->cpu_freq[cpu] = cpu_freq;
}

int tz_iwservice_init(void)
{
	struct iw_service_channel *ch;

	ch = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_SERVICE, 1, NULL, NULL, NULL);
	if (!ch) {
		log_error(tzdev_iwservice, "Failed to initialize iw service channel\n");
		return -ENOMEM;
	}

	iw_channel = ch;

	tz_iwservice_register_hotplug_callback();

	log_info(tzdev_iwservice, "IW service initialization done.\n");

	return 0;
}
