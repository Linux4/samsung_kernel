/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

//#include <linux/kallsyms.h>
//#include <linux/utsname.h>
//#include <asm/uaccess.h>
//#include <linux/printk.h>

#include <linux/platform_device.h>
#include "perfmgr_internal.h"
#include "mt_hotplug_strategy.h"
#include "mt_cpufreq.h"

/*--------------  MACRO -------------------*/
#define MAX_USER    4
#define MAX_CORE    (4)
#define MAX_FREQ    (1443000)

#ifdef max
#undef max
#endif
#define max(a,b) (((a) > (b)) ? (a) : (b))
/*----------------------------------------*/

/*--------------  DATA TYPE -------------------*/
typedef struct tUserScn{
	int core;
	int freq;
} tUserScn;

tUserScn ScnList[MAX_USER];
/*---------------------------------------------*/

void perfmgr_boost_core(int scn, int core)
{
	int i, coreToSet = PERFMGR_IGNORE;
	
	if(scn < PERFMGR_SCN_TOUCH || scn >= PERFMGR_SCN_NUM)
		return;
	
	ScnList[scn].core = (core >= 1 && core <= MAX_CORE) ? core : PERFMGR_IGNORE;
	
	for(i=0; i<PERFMGR_SCN_NUM && i<MAX_USER; i++) {
		coreToSet = max(coreToSet, ScnList[i].core);
	}
	
	if(coreToSet != PERFMGR_IGNORE)
		hps_set_cpu_num_base(BASE_TOUCH_BOOST, coreToSet, 0);
	else
		hps_set_cpu_num_base(BASE_TOUCH_BOOST, 1, 0);
}

void perfmgr_boost_freq(int scn, int freq)
{
	int i, freqToSet = PERFMGR_IGNORE;
	
	if(scn < PERFMGR_SCN_TOUCH || scn >= PERFMGR_SCN_NUM)
		return;
	
	ScnList[scn].freq = (freq > 0 && freq <= MAX_FREQ) ? freq : PERFMGR_IGNORE;
	
	for(i=0; i<PERFMGR_SCN_NUM && i<MAX_USER; i++) {
		freqToSet = max(freqToSet, ScnList[i].freq);
	}
	
	if(freqToSet != PERFMGR_IGNORE)
		mt_cpufreq_set_min_freq(MT_CPU_DVFS_LITTLE, freqToSet);
	else
		mt_cpufreq_set_min_freq(MT_CPU_DVFS_LITTLE, 0);
}

void perfmgr_boost_init(void)
{
	int i;

	for(i=0; i<PERFMGR_SCN_NUM && i<MAX_USER; i++) {
		ScnList[i].core = PERFMGR_IGNORE;
		ScnList[i].freq = PERFMGR_IGNORE;
	}
}

