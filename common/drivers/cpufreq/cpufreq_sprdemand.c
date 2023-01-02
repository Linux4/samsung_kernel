/*
 *  drivers/cpufreq/cpufreq_sprdemand.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "cpufreq_governor.h"
#include <linux/input.h>
#include <linux/sprd_cpu_cooling.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

/* On-demand governor macros */
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)

/* whether plugin cpu according to this score up threshold */
#define DEF_CPU_SCORE_UP_THRESHOLD		(100)
/* whether unplug cpu according to this down threshold*/
#define DEF_CPU_LOAD_DOWN_THRESHOLD		(30)
#define DEF_CPU_DOWN_COUNT			(3)

#define LOAD_CRITICAL 100
#define LOAD_HI 90
#define LOAD_MID 80
#define LOAD_LIGHT 50
#define LOAD_LO 0

#define LOAD_CRITICAL_SCORE 10
#define LOAD_HI_SCORE 5
#define LOAD_MID_SCORE 0
#define LOAD_LIGHT_SCORE -10
#define LOAD_LO_SCORE -20

#define GOVERNOR_BOOT_TIME	(50*HZ)
static unsigned long boot_done;

unsigned int cpu_hotplug_disable_set = false;

struct unplug_work_info {
	unsigned int cpuid;
	struct delayed_work unplug_work;
	struct dbs_data *dbs_data;
};

struct delayed_work plugin_work;
struct delayed_work unplug_work;

static DEFINE_PER_CPU(struct unplug_work_info, uwi);

static DEFINE_SPINLOCK(g_lock);
static unsigned int percpu_total_load[CONFIG_NR_CPUS] = {0};
static unsigned int percpu_check_count[CONFIG_NR_CPUS] = {0};
static int cpu_score = 0;

struct thermal_cooling_info_t {
	struct thermal_cooling_device *cdev;
	unsigned long cooling_state;
	struct sprd_cpu_cooling_platform_data *pdata;
	int max_state;
	int limit_freq;
} thermal_cooling_info = {
	.cdev = NULL,
	.cooling_state = 0,
};

static DEFINE_PER_CPU(struct od_cpu_dbs_info_s, sd_cpu_dbs_info);

static struct od_ops sd_ops;

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
static struct cpufreq_governor cpufreq_gov_sprdemand;
#endif

static void update_sampling_rate(struct dbs_data *dbs_data,	unsigned int new_rate);

static void sprdemand_powersave_bias_init_cpu(int cpu)
{
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info, cpu);

	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

/*
 * Not all CPUs want IO time to be accounted as busy; this depends on how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (android.com) claims this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) and later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
			boot_cpu_data.x86 == 6 &&
			boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 1;
}

struct sd_dbs_tuners *g_sd_tuners = NULL;

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int generic_powersave_bias_target(struct cpufreq_policy *policy,
		unsigned int freq_next, unsigned int relation)
{
	unsigned int freq_req, freq_reduc, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info,
						   policy->cpu);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("generic_powersave_bias_target governor %s return\n", policy->governor->name);
		if (g_sd_tuners == NULL)
			return freq_next;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}

	if (!dbs_info->freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_next,
			relation, &index);
	freq_req = dbs_info->freq_table[index].frequency;
	freq_reduc = freq_req * sd_tuners->powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_H, &index);
	freq_lo = dbs_info->freq_table[index].frequency;
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_L, &index);
	freq_hi = dbs_info->freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_lo;
	}
	jiffies_total = usecs_to_jiffies(sd_tuners->sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_jiffies = jiffies_lo;
	dbs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static void sprdemand_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		sprdemand_powersave_bias_init_cpu(i);
	}
}

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	struct dbs_data *dbs_data = p->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("dbs_freq_increase governor %s return\n", p->governor->name);
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}

	if (sd_tuners->powersave_bias)
		freq = sd_ops.powersave_bias_target(p, freq,
				CPUFREQ_RELATION_H);
	else if (p->cur == p->max)
		return;

	__cpufreq_driver_target(p, freq, sd_tuners->powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

static void sprd_unplug_one_cpu(struct work_struct *work)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;
	int cpuid;

	if(NULL == dbs_data)
	{
		pr_info("sprd_unplug_one_cpu return\n");
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}


#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() > 1) {
		if (!sd_tuners->cpu_hotplug_disable) {
			cpuid = cpumask_next(0, cpu_online_mask);
			pr_info("!!  we gonna unplug cpu%d  !!\n", cpuid);
			cpu_down(cpuid);
		}
	}
#endif
	return;
}

static void sprd_plugin_one_cpu(struct work_struct *work)
{
	int cpuid;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("sprd_plugin_one_cpu return\n");
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}


#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() < sd_tuners->cpu_num_limit) {
		cpuid = cpumask_next_zero(0, cpu_online_mask);
		if (!sd_tuners->cpu_hotplug_disable) {
			pr_info("!!  we gonna plugin cpu%d  !!\n", cpuid);
			cpu_up(cpuid);
		}
	}
#endif
	return;
}

unsigned int percpu_load[4] = {0};
#define MAX_CPU_NUM  (4)
#define MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE  (10)
#define MAX_PLUG_AVG_LOAD_SIZE (2)

unsigned int ga_percpu_total_load[MAX_CPU_NUM][MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE] = {{0}};
extern unsigned int dvfs_unplug_select;
extern unsigned int dvfs_plug_select;

unsigned int cur_window_size[MAX_CPU_NUM] ={0};
unsigned int prev_window_size[MAX_CPU_NUM] ={0};

int cur_window_index[MAX_CPU_NUM] = {0};
unsigned int cur_window_cnt[MAX_CPU_NUM] = {0};
int first_window_flag[4] = {0};

unsigned int sum_load[4] = {0};

unsigned int plug_avg_load[MAX_CPU_NUM][MAX_PLUG_AVG_LOAD_SIZE] = {{50}};
unsigned int plug_avg_load_index[MAX_CPU_NUM] = {0};

#define mod(n, div) ((n) % (div))

extern unsigned int dvfs_score_select;
extern unsigned int dvfs_score_hi[4];
extern unsigned int dvfs_score_mid[4];
extern unsigned int dvfs_score_critical[4];

int a_score_sub[4][4][11]=
{
	{
		{0,0,0,0,0,0,0,0,5,5,10},
		{-5,-5,0,0,0,0,0,0,0,5,5},
		{-10,-5,0,0,0,0,0,0,0,5,5},
		{0,0,0,0,0,0,0,0,0,0,0}
	},
	{
		{0,0,0,0,0,0,0,0,5,10,20},
		{-10,-5,-5,0,0,0,0,0,0,5,10},
		{-20,-10,-5,0,0,0,0,0,0,5,10},
		{0,0,0,0,0,0,0,0,0,0,0}
	},
	{
		{0,0,0,0,0,0,0,10,20,20,30},
		{0,0,0,0,0,0,0,5,10,10,20},
		{0,0,0,0,0,0,0,0,5,5,10},
		{0,0,0,0,0,0,0,0,0,0,0}
	},
	{
		{0,0,0,0,0,0,0,0,30,30,50},
		{-20,-20,0,0,0,0,0,0,20,20,30},
		{-40,-20,0,0,0,0,0,0,5,10,20},
		{0,0,0,0,0,0,0,0,0,0,0}
	}
};

int ga_samp_rate[11] = {100000,100000,100000,100000,100000,100000,50000,50000,30000,30000,30000};

unsigned int a_sub_windowsize[8][6] =
{
	{0,0,0,0,0,0},
	{0,0,0,0,0,0},
	{4,5,5,6,7,7},
	{4,5,5,6,7,7},
	{3,4,4,5,6,6},
	{2,3,3,4,5,5},
	{1,2,2,3,4,4},
	{0,1,1,2,3,3}
};

static int cpu_evaluate_score(int cpu, struct sd_dbs_tuners *sd_tunners , unsigned int load)
{
	int score = 0;
	static int rate[4] = {1};
	int delta = 0;
	int a_samp_rate[5] = {30000,30000,50000,50000,50000};

	if(dvfs_score_select < 4)
	{
		if (load >= sd_tunners->load_critical)
		{
			score = dvfs_score_critical[num_online_cpus()];
			sd_tunners->sampling_rate = a_samp_rate[0];
		}
		else if (load >= sd_tunners->load_hi)
		{
			score = dvfs_score_hi[num_online_cpus()];
			sd_tunners->sampling_rate = a_samp_rate[1];
		}
		else if (load >= sd_tunners->load_mid)
		{
			score = dvfs_score_mid[num_online_cpus()];
			sd_tunners->sampling_rate = a_samp_rate[2];
		}
		else if (load >= sd_tunners->load_light)
		{
			score = sd_tunners->load_light_score;
			sd_tunners->sampling_rate = a_samp_rate[3];
		}
		else if (load >= sd_tunners->load_lo)
		{
			score = sd_tunners->load_lo_score;
			sd_tunners->sampling_rate = a_samp_rate[4];
		}
		else
		{
			score = 0;
			sd_tunners->sampling_rate = a_samp_rate[4];
		}

	}
	else
	{
		delta = abs(percpu_load[cpu] - load);
		if((delta > 30)
			&&(load > 80))
		{
			rate[cpu] +=2;
			score = a_score_sub[dvfs_score_select - 4][num_online_cpus() - 1][load/10] * rate[cpu];
			rate[cpu] --;
		}
		else
		{
			score = a_score_sub[dvfs_score_select - 4][num_online_cpus() - 1][load/10];
			rate[cpu] = 1;
		}
	}
	pr_debug("[DVFS SCORE] rate[%d] %d load %d score %d\n",cpu,rate[cpu],load,score);
	return score;
}



static int sd_adjust_window(struct sd_dbs_tuners *sd_tunners , unsigned int load)
{
	unsigned int cur_window_size = 0;

	if (load >= sd_tunners->load_critical)
		cur_window_size = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - a_sub_windowsize[dvfs_unplug_select][0];
	else if (load >= sd_tunners->load_hi)
		cur_window_size = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - a_sub_windowsize[dvfs_unplug_select][1];
	else if (load >= sd_tunners->load_mid)
		cur_window_size = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - a_sub_windowsize[dvfs_unplug_select][2];
	else if (load >= sd_tunners->load_light)
		cur_window_size = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - a_sub_windowsize[dvfs_unplug_select][3];
	else if (load >= sd_tunners->load_lo)
		cur_window_size = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - a_sub_windowsize[dvfs_unplug_select][4];
	else
		cur_window_size = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - a_sub_windowsize[dvfs_unplug_select][5];

	return cur_window_size;
}

static unsigned int sd_unplug_avg_load(int cpu, struct sd_dbs_tuners *sd_tunners , unsigned int load)
{
	int sum_idx_lo = 0;
	unsigned int sum_idx_hi = 0;
	unsigned int * p_valid_pos = NULL;
	unsigned int sum_load = 0;

	/*
	initialize the window size for the first time
	*/
	if(!cur_window_size[cpu])
	{
		cur_window_size[cpu] = sd_adjust_window(sd_tunners,load);
		pr_debug("[DVFS_UNPLUG]cur_window_size[%d] = %d\n",cpu,cur_window_size[cpu]);
		return 100;
	}
	else
	{
		/*
		record the load in the percpu array
		*/
		ga_percpu_total_load[cpu][cur_window_index[cpu]] = load;
		cur_window_cnt[cpu]++;
		/*
		update the windw index
		*/
		cur_window_index[cpu]++;
		cur_window_index[cpu] = mod(cur_window_index[cpu], MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

		/*
		window array is not full, break
		*/
		if(cur_window_cnt[cpu] < cur_window_size[cpu])
		{
               	return 100;
		}
		else
		{
			/*
			adjust the window index for it be added one more extra time
			*/
			if(!cur_window_index[cpu])
			{
				cur_window_index[cpu] = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - 1;
			}
			else
			{
				cur_window_index[cpu]--;
			}
			/*
			find the valid position according to current window size and indexs
			*/
			p_valid_pos = (unsigned int *)&ga_percpu_total_load[cpu][cur_window_index[cpu]];
			/*
			calculate the average load value by decrease the index, for we need the very updated value which locate in the end of the array
			*/
			for(sum_idx_lo = 0; sum_idx_lo < cur_window_size[cpu]; sum_idx_lo++)
			{
				/*
				calculate the lower part
				*/
				if((cur_window_index[cpu] - sum_idx_lo) >=0)
				{
					sum_load += *(unsigned int *)((unsigned int)p_valid_pos - sum_idx_lo * sizeof(p_valid_pos));
				}
				else
				{
					/*
					calculate the higher part
					*/
					sum_idx_hi = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - (cur_window_size[cpu] - sum_idx_lo);
					for(; sum_idx_hi < MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE; sum_idx_hi++)
					{
						sum_load += ga_percpu_total_load[cpu][sum_idx_hi];
					}
					break;

				}

			}
			sum_load = sum_load / cur_window_size[cpu];
			/*
			adjust the window according to previews load
			*/
			cur_window_size[cpu] = sd_adjust_window(sd_tunners, sum_load);
			cur_window_cnt[cpu] = 0;
			pr_debug("[DVFS_UNPLUG]cur_window_size %d sum_load %d\n",cur_window_size[cpu],sum_load);
		}
		return sum_load;
	}

}


static unsigned int sd_unplug_avg_load1(int cpu, struct sd_dbs_tuners *sd_tunners , unsigned int load)
{
	int avg_load = 0;
	int cur_window_pos = 0;
	int cur_window_pos_tail = 0;
	int idx = 0;

	/*
	initialize the window size for the first time
	cur_window_cnt[cpu] will be cleared when the core is unpluged
	*/
	if((!first_window_flag[cpu])
		||(!cur_window_size[cpu]))
	{
		if(!cur_window_size[cpu])
		{
			cur_window_size[cpu] = sd_adjust_window(sd_tunners,load);
			prev_window_size[cpu] = cur_window_size[cpu];
		}
		if(cur_window_cnt[cpu] < (cur_window_size[cpu] - 1))
		{
			/*
			record the load in the percpu array
			*/
			ga_percpu_total_load[cpu][cur_window_index[cpu]] = load;
			/*
			update the windw index
			*/
			cur_window_index[cpu]++;
			cur_window_index[cpu] = mod(cur_window_index[cpu], MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

			cur_window_cnt[cpu]++;

			sum_load[cpu] += load;

			return LOAD_LIGHT;
		}
		else
		{
			first_window_flag[cpu] = 1;
		}
	}
	/*
	record the load in the percpu array
	*/
	ga_percpu_total_load[cpu][cur_window_index[cpu]] = load;
	/*
	update the windw index
	*/
	cur_window_index[cpu]++;
	cur_window_index[cpu] = mod(cur_window_index[cpu], MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

	/*
	adjust the window index for it be added one more extra time
	*/
	if(!cur_window_index[cpu])
	{
		cur_window_pos = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - 1;
	}
	else
	{
		cur_window_pos =  cur_window_index[cpu] - 1;
	}

	/*
	tail = (c_w_p + max_window_size - c_w_s) % max_window_size
	tail = (2 + 8 - 5) % 8 = 5
	tail = (6 + 8 - 5) % 8 = 1
	*/
	cur_window_pos_tail = mod(MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE + cur_window_pos - cur_window_size[cpu],MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

	/*
	no window size change
	*/
	if(prev_window_size[cpu] == cur_window_size[cpu] )
	{
		sum_load[cpu] = sum_load[cpu] + ga_percpu_total_load[cpu][cur_window_pos] - ga_percpu_total_load[cpu][cur_window_pos_tail] ;
	}
	else
	{
		/*
		window size change, recalculate the sum load
		*/
		sum_load[cpu] = 0;
		while(idx < cur_window_size[cpu])
		{
			sum_load[cpu] += ga_percpu_total_load[cpu][mod(cur_window_pos_tail + 1 +idx,MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE)];
			idx++;
		}
	}
	avg_load = sum_load[cpu] / cur_window_size[cpu];

	percpu_load[cpu] = avg_load;

	prev_window_size[cpu] = cur_window_size[cpu];

	cur_window_size[cpu] = (load > avg_load) ? sd_adjust_window(sd_tunners, load) : prev_window_size[cpu];

	sd_tunners->sampling_rate = ga_samp_rate[mod(avg_load/10,11)];

	pr_debug("[DVFS_UNPLUG]sum_load[%d]=%d tail[%d]=%d cur[%d]=%d  cur_window_size %d load %d avg_load %d\n",cpu,sum_load[cpu],cur_window_pos_tail,
		ga_percpu_total_load[cpu][cur_window_pos_tail],cur_window_pos,ga_percpu_total_load[cpu][cur_window_pos],cur_window_size[cpu],load,avg_load);
	if(avg_load > 100)
	{
		pr_info("cur_window_pos %d cur_window_pos_tail %d load %d sum_load %d\n",cur_window_pos,cur_window_pos_tail,load,sum_load[cpu] );
	}
	return avg_load;

}

static unsigned int sd_unplug_avg_load11(int cpu, struct sd_dbs_tuners *sd_tunners , unsigned int load)
{
	int avg_load = 0;
	int cur_window_pos = 0;
	int cur_window_pos_tail = 0;
	int idx = 0;
	/*
	initialize the window size for the first time
	cur_window_cnt[cpu] will be cleared when the core is unpluged
	*/
	if((!first_window_flag[cpu])
		||(!cur_window_size[cpu]))
	{
		if(!cur_window_size[cpu])
		{
			cur_window_size[cpu] = sd_adjust_window(sd_tunners,load);
			prev_window_size[cpu] = cur_window_size[cpu];
		}
		if(cur_window_cnt[cpu] < (cur_window_size[cpu] - 1))
		{
			/*
			record the load in the percpu array
			*/
			ga_percpu_total_load[cpu][cur_window_index[cpu]] = load;
			/*
			update the windw index
			*/
			cur_window_index[cpu]++;
			cur_window_index[cpu] = mod(cur_window_index[cpu], MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

			cur_window_cnt[cpu]++;

			sum_load[cpu] += load;

			return LOAD_LIGHT;
		}
		else
		{
			first_window_flag[cpu] = 1;
		}
	}
	/*
	record the load in the percpu array
	*/
	ga_percpu_total_load[cpu][cur_window_index[cpu]] = load;
	/*
	update the windw index
	*/
	cur_window_index[cpu]++;
	cur_window_index[cpu] = mod(cur_window_index[cpu], MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

	/*
	adjust the window index for it be added one more extra time
	*/
	if(!cur_window_index[cpu])
	{
		cur_window_pos = MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE - 1;
	}
	else
	{
		cur_window_pos =  cur_window_index[cpu] - 1;
	}

	/*
	tail = (c_w_p + max_window_size - c_w_s) % max_window_size
	tail = (2 + 8 - 5) % 8 = 5
	tail = (6 + 8 - 5) % 8 = 1
	*/
	cur_window_pos_tail = mod(MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE + cur_window_pos - cur_window_size[cpu],MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);

	/*
	sum load = current load + current new data - tail data
	*/
	sum_load[cpu] = sum_load[cpu] + ga_percpu_total_load[cpu][cur_window_pos] - ga_percpu_total_load[cpu][cur_window_pos_tail] ;

	/*
	calc the average load
	*/
	avg_load = sum_load[cpu] / cur_window_size[cpu];

	percpu_load[cpu] = avg_load;

	sd_tunners->sampling_rate = ga_samp_rate[mod(avg_load/10,11)];

	return avg_load;
}

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Every sampling_rate, we look
 * for the lowest frequency which can sustain the load while keeping idle time
 * over 30%. If such a frequency exist, we try to decrease to this frequency.
 *
 * Any frequency increase takes it to the maximum frequency. Frequency reduction
 * happens at minimum steps of 5% (default) of current frequency
 */
static void sd_check_cpu(int cpu, unsigned int load_freq)
{
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int local_load = 0;
	unsigned int itself_avg_load = 0;
	struct unplug_work_info *puwi;
	int local_cpu = 0;

	if(time_before(jiffies, boot_done))
		return;

	local_cpu = smp_processor_id();

	if(local_cpu)
		return;

	/* skip cpufreq adjustment if system enter into suspend */
	if(true == sd_tuners->is_suspend){
		pr_info("%s: is_suspend=%s, skip cpufreq adjust\n",
			__func__, sd_tuners->is_suspend?"true":"false");
		goto plug_check;
	}


	dbs_info->freq_lo = 0;

	local_load = load_freq/policy->cur;

        pr_debug("[DVFS %d] load %d %x load_freq %d policy->cur %d\n",cpu,local_load,local_load,load_freq,policy->cur);

	if (thermal_cooling_info.cooling_state && policy->cur > thermal_cooling_info.limit_freq){
		__cpufreq_driver_target(policy, thermal_cooling_info.limit_freq, CPUFREQ_RELATION_L);
	}

	/* Check for frequency increase */
	if (load_freq > sd_tuners->up_threshold * policy->cur) {
		if (thermal_cooling_info.cooling_state){
			__cpufreq_driver_target(policy,
					thermal_cooling_info.limit_freq, CPUFREQ_RELATION_L);
			goto plug_check;
		}

		/* If switching to max speed, apply sampling_down_factor */
		if (policy->cur < policy->max)
			dbs_info->rate_mult =
				sd_tuners->sampling_down_factor;
		if(num_online_cpus() == sd_tuners->cpu_num_limit)
			dbs_freq_increase(policy, policy->max);
		else
			dbs_freq_increase(policy, policy->max-1);

		goto plug_check;
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		goto plug_check;

	/*
	 * The optimal frequency is the frequency that is the lowest that can
	 * support the current CPU usage without triggering the up policy. To be
	 * safe, we focus 3 points under the threshold.
	 */
	if (load_freq < sd_tuners->adj_up_threshold
			* policy->cur) {
		unsigned int freq_next;
		freq_next = load_freq / sd_tuners->adj_up_threshold;

		/* No longer fully busy, reset rate_mult */
		dbs_info->rate_mult = 1;

		if (freq_next < policy->min)
			freq_next = policy->min;

		if (!sd_tuners->powersave_bias) {
			__cpufreq_driver_target(policy, freq_next,
					CPUFREQ_RELATION_L);
			goto plug_check;
		}

		freq_next = sd_ops.powersave_bias_target(policy, freq_next,
					CPUFREQ_RELATION_L);
		__cpufreq_driver_target(policy, freq_next, CPUFREQ_RELATION_L);

	}

plug_check:

	/* skip cpu hotplug check if hotplug is disabled */
	if (sd_tuners->cpu_hotplug_disable)
		return;

	/* cpu plugin check */
	if(num_online_cpus() < sd_tuners->cpu_num_limit) {
		cpu_score += cpu_evaluate_score(policy->cpu,sd_tuners, local_load);
		if (cpu_score < 0)
			cpu_score = 0;
		if (cpu_score >= sd_tuners->cpu_score_up_threshold) {
			pr_debug("cpu_score=%d, begin plugin cpu!\n", cpu_score);
			cpu_score = 0;
			schedule_delayed_work_on(0, &plugin_work, 0);
		}
	}


	/* cpu unplug check */
	puwi = &per_cpu(uwi, local_cpu);
	if((num_online_cpus() > 1) && (dvfs_unplug_select == 1)){
		percpu_total_load[local_cpu] += local_load;
		percpu_check_count[local_cpu]++;
		if(percpu_check_count[cpu] == sd_tuners->cpu_down_count) {
			/* calculate itself's average load */
			itself_avg_load = percpu_total_load[local_cpu]/sd_tuners->cpu_down_count;
			pr_debug("check unplug: for cpu%u avg_load=%d\n", local_cpu, itself_avg_load);
			if(itself_avg_load < sd_tuners->cpu_down_threshold) {
					pr_info("cpu%u's avg_load=%d,begin unplug cpu\n",
						policy->cpu, itself_avg_load);
					schedule_delayed_work_on(0, &unplug_work, 0);
			}
			percpu_check_count[local_cpu] = 0;
			percpu_total_load[local_cpu] = 0;
		}
	}
	else if((num_online_cpus() > 1) && (dvfs_unplug_select == 2))
	{
		/* calculate itself's average load */
		itself_avg_load = sd_unplug_avg_load1(local_cpu, sd_tuners, local_load);
		pr_debug("check unplug: for cpu%u avg_load=%d\n", local_cpu, itself_avg_load);
		if(itself_avg_load < sd_tuners->cpu_down_threshold)
		{
				pr_info("cpu%u's avg_load=%d,begin unplug cpu\n",
						local_cpu, itself_avg_load);
				percpu_load[local_cpu] = 0;
				cur_window_size[local_cpu] = 0;
				cur_window_index[local_cpu] = 0;
				cur_window_cnt[local_cpu] = 0;
				prev_window_size[local_cpu] = 0;
				first_window_flag[local_cpu] = 0;
				sum_load[local_cpu] = 0;
				memset(&ga_percpu_total_load[local_cpu][0],0,sizeof(int) * MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);
				schedule_delayed_work_on(0, &unplug_work, 0);
		}
	}
	else if((num_online_cpus() > 1) && (dvfs_unplug_select > 2))
	{
		/* calculate itself's average load */
		itself_avg_load = sd_unplug_avg_load11(local_cpu, sd_tuners, local_load);
		pr_debug("check unplug: for cpu%u avg_load=%d\n", local_cpu, itself_avg_load);
		if(itself_avg_load < sd_tuners->cpu_down_threshold)
		{
				pr_info("cpu%u's avg_load=%d,begin unplug cpu\n",
						local_cpu, itself_avg_load);
				percpu_load[local_cpu] = 0;
				cur_window_size[local_cpu] = 0;
				cur_window_index[local_cpu] = 0;
				cur_window_cnt[local_cpu] = 0;
				prev_window_size[local_cpu] = 0;
				first_window_flag[local_cpu] = 0;
				sum_load[local_cpu] = 0;
				memset(&ga_percpu_total_load[local_cpu][0],0,sizeof(int) * MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE);
				schedule_delayed_work_on(0, &unplug_work, 0);
		}
	}
}

static void sd_dbs_timer(struct work_struct *work)
{
	struct od_cpu_dbs_info_s *dbs_info =
		container_of(work, struct od_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct od_cpu_dbs_info_s *core_dbs_info = &per_cpu(sd_cpu_dbs_info,
			cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int delay = 0, sample_type = core_dbs_info->sample_type;
	bool modify_all = true;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if(time_before(jiffies, boot_done))
		goto max_delay;

	if (!need_load_eval(&core_dbs_info->cdbs, sd_tuners->sampling_rate)) {
		modify_all = false;
		goto max_delay;
	}

	/* Common NORMAL_SAMPLE setup */
	core_dbs_info->sample_type = OD_NORMAL_SAMPLE;
	if (sample_type == OD_SUB_SAMPLE) {
		delay = core_dbs_info->freq_lo_jiffies;
		__cpufreq_driver_target(core_dbs_info->cdbs.cur_policy,
				core_dbs_info->freq_lo, CPUFREQ_RELATION_H);
	} else {
		dbs_check_cpu(dbs_data, cpu);
		if (core_dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			core_dbs_info->sample_type = OD_SUB_SAMPLE;
			delay = core_dbs_info->freq_hi_jiffies;
		}
	}

max_delay:
	if (!delay)
		delay = delay_for_sampling_rate(sd_tuners->sampling_rate
				* core_dbs_info->rate_mult);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

/************************** sysfs interface ************************/
static struct common_dbs_data sd_dbs_cdata;

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updating
 * dbs_tuners_int.sampling_rate might not be appropriate. For example, if the
 * original sampling_rate was 1 second and the requested new sampling rate is 10
 * ms because the user needs immediate reaction from ondemand governor, but not
 * sure if higher frequency will be required or not, then, the governor may
 * change the sampling rate too late; up to 1 second later. Thus, if we are
 * reducing the sampling rate, we need to make the new value effective
 * immediately.
 */
static void update_sampling_rate(struct dbs_data *dbs_data,
		unsigned int new_rate)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int cpu;

	sd_tuners->sampling_rate = new_rate = max(new_rate,
			dbs_data->min_sampling_rate);

	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct od_cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (policy->governor != &cpufreq_gov_sprdemand) {
			cpufreq_cpu_put(policy);
			continue;
		}
		dbs_info = &per_cpu(sd_cpu_dbs_info, cpu);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->cdbs.timer_mutex);

		if (!delayed_work_pending(&dbs_info->cdbs.work)) {
			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			continue;
		}

		next_sampling = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->cdbs.work.timer.expires;

		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			cancel_delayed_work_sync(&dbs_info->cdbs.work);
			mutex_lock(&dbs_info->cdbs.timer_mutex);

			gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy,
					usecs_to_jiffies(new_rate), true);

		}
		mutex_unlock(&dbs_info->cdbs.timer_mutex);
	}
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	update_sampling_rate(dbs_data, input);
	return count;
}

static ssize_t store_io_is_busy(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	sd_tuners->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info,
									j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
			&dbs_info->cdbs.prev_cpu_wall, sd_tuners->io_is_busy);
	}
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	/* Calculate the new adj_up_threshold */
	sd_tuners->adj_up_threshold += input;
	sd_tuners->adj_up_threshold -= sd_tuners->up_threshold;

	sd_tuners->up_threshold = input;
	return count;
}

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	sd_tuners->sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info,
				j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == sd_tuners->ignore_nice) { /* nothing to do */
		return count;
	}
	sd_tuners->ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(sd_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
			&dbs_info->cdbs.prev_cpu_wall, sd_tuners->io_is_busy);
		if (sd_tuners->ignore_nice)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 1000)
		input = 1000;

	sd_tuners->powersave_bias = input;
	sprdemand_powersave_bias_init();
	return count;
}

static ssize_t store_cpu_num_limit(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_num_limit = input;
	return count;
}

static ssize_t store_cpu_score_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_score_up_threshold = input;
	return count;
}

static ssize_t store_load_critical(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_critical = input;
	return count;
}

static ssize_t store_load_hi(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_hi = input;
	return count;
}

static ssize_t store_load_mid(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_mid = input;
	return count;
}

static ssize_t store_load_light(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_light = input;
	return count;
}

static ssize_t store_load_lo(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_lo = input;
	return count;
}

static ssize_t store_load_critical_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_critical_score = input;
	return count;
}

static ssize_t store_load_hi_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_hi_score = input;
	return count;
}


static ssize_t store_load_mid_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_mid_score = input;
	return count;
}

static ssize_t store_load_light_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_light_score = input;
	return count;
}

static ssize_t store_load_lo_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_lo_score = input;
	return count;
}

static ssize_t store_cpu_down_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_down_threshold = input;
	return count;
}

static ssize_t store_cpu_down_count(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_down_count = input;
	return count;
}

static ssize_t store_cpu_hotplug_disable(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input, cpu;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}

	if (sd_tuners->cpu_hotplug_disable == input) {
		return count;
	}
	if (sd_tuners->cpu_num_limit > 1)
		sd_tuners->cpu_hotplug_disable = input;

	if(sd_tuners->cpu_hotplug_disable > 0)
		cpu_hotplug_disable_set = true;
	else
		cpu_hotplug_disable_set = false;

	smp_wmb();
	/* plug-in all offline cpu mandatory if we didn't
	 * enbale CPU_DYNAMIC_HOTPLUG
         */
#ifdef CONFIG_HOTPLUG_CPU
	if (sd_tuners->cpu_hotplug_disable) {
		for_each_cpu(cpu, cpu_possible_mask) {
			if (!cpu_online(cpu))
				{
				cpu_up(cpu);
			  }
		}
	}
#endif
	return count;
}


show_store_one(sd, sampling_rate);
show_store_one(sd, io_is_busy);
show_store_one(sd, up_threshold);
show_store_one(sd, sampling_down_factor);
show_store_one(sd, ignore_nice);
show_store_one(sd, powersave_bias);
declare_show_sampling_rate_min(sd);
show_store_one(sd, cpu_score_up_threshold);
show_store_one(sd, load_critical);
show_store_one(sd, load_hi);
show_store_one(sd, load_mid);
show_store_one(sd, load_light);
show_store_one(sd, load_lo);
show_store_one(sd, load_critical_score);
show_store_one(sd, load_hi_score);
show_store_one(sd, load_mid_score);
show_store_one(sd, load_light_score);
show_store_one(sd, load_lo_score);
show_store_one(sd, cpu_down_threshold);
show_store_one(sd, cpu_down_count);
show_store_one(sd, cpu_hotplug_disable);
show_store_one(sd, cpu_num_limit);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(io_is_busy);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(ignore_nice);
gov_sys_pol_attr_rw(powersave_bias);
gov_sys_pol_attr_ro(sampling_rate_min);
gov_sys_pol_attr_rw(cpu_score_up_threshold);
gov_sys_pol_attr_rw(load_critical);
gov_sys_pol_attr_rw(load_hi);
gov_sys_pol_attr_rw(load_mid);
gov_sys_pol_attr_rw(load_light);
gov_sys_pol_attr_rw(load_lo);
gov_sys_pol_attr_rw(load_critical_score);
gov_sys_pol_attr_rw(load_hi_score);
gov_sys_pol_attr_rw(load_mid_score);
gov_sys_pol_attr_rw(load_light_score);
gov_sys_pol_attr_rw(load_lo_score);
gov_sys_pol_attr_rw(cpu_down_threshold);
gov_sys_pol_attr_rw(cpu_down_count);
gov_sys_pol_attr_rw(cpu_hotplug_disable);
gov_sys_pol_attr_rw(cpu_num_limit);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&ignore_nice_gov_sys.attr,
	&powersave_bias_gov_sys.attr,
	&io_is_busy_gov_sys.attr,
	&cpu_score_up_threshold_gov_sys.attr,
	&load_critical_gov_sys.attr,
	&load_hi_gov_sys.attr,
	&load_mid_gov_sys.attr,
	&load_light_gov_sys.attr,
	&load_lo_gov_sys.attr,
	&load_critical_score_gov_sys.attr,
	&load_hi_score_gov_sys.attr,
	&load_mid_score_gov_sys.attr,
	&load_light_score_gov_sys.attr,
	&load_lo_score_gov_sys.attr,
	&cpu_down_threshold_gov_sys.attr,
	&cpu_down_count_gov_sys.attr,
	&cpu_hotplug_disable_gov_sys.attr,
	&cpu_num_limit_gov_sys.attr,
	NULL
};

static struct attribute_group sd_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "sprdemand",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&ignore_nice_gov_pol.attr,
	&powersave_bias_gov_pol.attr,
	&io_is_busy_gov_pol.attr,
	&cpu_score_up_threshold_gov_pol.attr,
	&load_critical_gov_pol.attr,
	&load_hi_gov_pol.attr,
	&load_mid_gov_pol.attr,
	&load_light_gov_pol.attr,
	&load_lo_gov_pol.attr,
	&load_critical_score_gov_pol.attr,
	&load_hi_score_gov_pol.attr,
	&load_mid_score_gov_pol.attr,
	&load_light_score_gov_pol.attr,
	&load_lo_score_gov_pol.attr,
	&cpu_down_threshold_gov_pol.attr,
	&cpu_down_count_gov_pol.attr,
	&cpu_hotplug_disable_gov_pol.attr,
	&cpu_num_limit_gov_pol.attr,
	NULL
};

static struct attribute_group sd_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "sprdemand",
};

/************************** sysfs end ************************/

static int sd_init(struct dbs_data *dbs_data)
{
	struct sd_dbs_tuners *tuners;
	u64 idle_time;
	int cpu, i;
	struct unplug_work_info *puwi;

	tuners = kzalloc(sizeof(struct sd_dbs_tuners), GFP_KERNEL);

	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	cpu = get_cpu();
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		tuners->up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = MICRO_FREQUENCY_UP_THRESHOLD -
			MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		dbs_data->min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = DEF_FREQUENCY_UP_THRESHOLD -
			DEF_FREQUENCY_DOWN_DIFFERENTIAL;

		/* For correct statistics, we need 10 ticks for each measure */
		dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
			jiffies_to_usecs(10);
	}

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice = 0;
	tuners->powersave_bias = 0;
	tuners->io_is_busy = should_io_be_busy();

	tuners->cpu_hotplug_disable = true;
	tuners->is_suspend = false;
	tuners->cpu_score_up_threshold = DEF_CPU_SCORE_UP_THRESHOLD;
	tuners->load_critical = LOAD_CRITICAL;
	tuners->load_hi = LOAD_HI;
	tuners->load_mid = LOAD_MID;
	tuners->load_light = LOAD_LIGHT;
	tuners->load_lo = LOAD_LO;
	tuners->load_critical_score = LOAD_CRITICAL_SCORE;
	tuners->load_hi_score = LOAD_HI_SCORE;
	tuners->load_mid_score = LOAD_MID_SCORE;
	tuners->load_light_score = LOAD_LIGHT_SCORE;
	tuners->load_lo_score = LOAD_LO_SCORE;
	tuners->cpu_down_threshold = DEF_CPU_LOAD_DOWN_THRESHOLD;
	tuners->cpu_down_count = DEF_CPU_DOWN_COUNT;
	tuners->cpu_num_limit = nr_cpu_ids;
	if (tuners->cpu_num_limit > 1)
		tuners->cpu_hotplug_disable = false;

	memcpy(g_sd_tuners,tuners,sizeof(struct sd_dbs_tuners));

	dbs_data->tuners = tuners;
	mutex_init(&dbs_data->mutex);

	INIT_DELAYED_WORK(&plugin_work, sprd_plugin_one_cpu);
	INIT_DELAYED_WORK(&unplug_work, sprd_unplug_one_cpu);

	for_each_possible_cpu(i) {
		puwi = &per_cpu(uwi, i);
		puwi->cpuid = i;
		puwi->dbs_data = dbs_data;
		INIT_DELAYED_WORK(&puwi->unplug_work, sprd_unplug_one_cpu);
	}

	return 0;
}

static void sd_exit(struct dbs_data *dbs_data)
{
	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(sd_cpu_dbs_info);

static struct od_ops sd_ops = {
	.powersave_bias_init_cpu = sprdemand_powersave_bias_init_cpu,
	.powersave_bias_target = generic_powersave_bias_target,
	.freq_increase = dbs_freq_increase,
};

static struct common_dbs_data sd_dbs_cdata = {
	/* sprdemand belong to ondemand gov */
	.governor = GOV_ONDEMAND,
	.attr_group_gov_sys = &sd_attr_group_gov_sys,
	.attr_group_gov_pol = &sd_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = sd_dbs_timer,
	.gov_check_cpu = sd_check_cpu,
	.gov_ops = &sd_ops,
	.init = sd_init,
	.exit = sd_exit,
};

static int sd_cpufreq_governor_dbs(struct cpufreq_policy *policy,
		unsigned int event)
{
	return cpufreq_governor_dbs(policy, &sd_dbs_cdata, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
static
#endif
struct cpufreq_governor cpufreq_gov_sprdemand = {
	.name			= "sprdemand",
	.governor		= sd_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

#if defined(CONFIG_THERMAL)
static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = 0;

	*state = thermal_cooling_info.max_state;

	return ret;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = 0;

	*state = thermal_cooling_info.cooling_state;

	return ret;
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;
	int cpus ,i;
	int max_core;

	if(time_before(jiffies, boot_done)){
		return 0;
	}
	if(NULL == dbs_data){
		pr_info("sprdemand_gov_pm_notifier_call governor %s return\n", policy->governor->name);
		return 0;
	}else{
		sd_tuners = dbs_data->tuners;
	}

	if (c_info->cooling_state == state){
		return 0;
	}else{
		c_info->cooling_state = state;
	}

	c_info->limit_freq = c_info->pdata->cpu_state[state].max_freq;
	max_core = c_info->pdata->cpu_state[state].max_core;
	pr_info("cpu cooling %s: %d limit_freq: %d kHz max_core: %d\n",
			__func__, state, c_info->limit_freq, max_core);

	if (sd_tuners->cpu_num_limit <=  max_core){
		sd_tuners->cpu_num_limit = max_core;
		return 0;
	}
	sd_tuners->cpu_num_limit = max_core;
	cpus = num_online_cpus();
	for (i = 0; i < cpus - max_core; ++i){
		schedule_delayed_work_on(0, &unplug_work, 0);
	}

	return 0;
}

static struct thermal_cooling_device_ops sprd_cpufreq_cooling_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};
#endif

static int sprdemand_gov_pm_notifier_call(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("sprdemand_gov_pm_notifier_call governor %s return\n", policy->governor->name);
		if (g_sd_tuners == NULL)
			return NOTIFY_OK;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}

	/* in suspend and hibernation process, we need set frequency to the orignal
	 * one to make sure all things go right */
	if (event == PM_SUSPEND_PREPARE || event == PM_HIBERNATION_PREPARE) {
		pr_info(" %s, recv pm suspend notify\n", __func__ );
		if (sd_tuners->cpu_num_limit > 1)
			if(cpu_hotplug_disable_set == false)
				sd_tuners->cpu_hotplug_disable = true;
		sd_tuners->is_suspend = true;
		dbs_freq_increase(policy, policy->max);
		pr_info(" %s, recv pm suspend notify done\n", __func__ );
	}

	return NOTIFY_OK;
}

static struct notifier_block sprdemand_gov_pm_notifier = {
	.notifier_call = sprdemand_gov_pm_notifier_call,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprdemand_gov_early_suspend(struct early_suspend *h)
{
	pr_info("%s do nothing\n", __func__);
	return;
}

static void sprdemand_gov_late_resume(struct early_suspend *h)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("sprdemand_gov_late_resume governor %s return\n", policy->governor->name);
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}


	if (sd_tuners->cpu_num_limit > 1)
		if(cpu_hotplug_disable_set == false)
			sd_tuners->cpu_hotplug_disable = false;
	sd_tuners->is_suspend = false;

	return;
}

static struct early_suspend sprdemand_gov_earlysuspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = sprdemand_gov_early_suspend,
	.resume = sprdemand_gov_late_resume,
};
#endif


static struct workqueue_struct *input_wq;
static struct work_struct dbs_refresh_work;

static void dbs_refresh_callback(struct work_struct *work)
{
	unsigned int cpu = smp_processor_id();
	struct od_cpu_dbs_info_s *core_dbs_info = &per_cpu(sd_cpu_dbs_info,
			cpu);

	struct cpufreq_policy *policy;

	policy = core_dbs_info->cdbs.cur_policy;

	if (!policy)
	{
		return;
	}


	if (policy->cur < policy->max)
	{
		if (thermal_cooling_info.cooling_state){
			cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);
		}

		core_dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(cpu,
				&core_dbs_info->cdbs.prev_cpu_wall,should_io_be_busy());
	}

}

static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	int i;
	bool ret;
	static int tp_time = 0;

	if(!dvfs_plug_select)
		return;

	if(jiffies <= (tp_time + 10)){
		tp_time = jiffies;
		return;
	}
	tp_time = jiffies;
	ret = queue_work_on(0, input_wq, &dbs_refresh_work);
	pr_debug("[DVFS] dbs_input_event %d\n",ret);

}

static int dbs_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpufreq";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	pr_debug("[DVFS] dbs_input_connect register success\n");
	return 0;
err1:
	pr_info("[DVFS] dbs_input_connect register fail err1\n");
	input_unregister_handle(handle);
err2:
	pr_info("[DVFS] dbs_input_connect register fail err2\n");
	kfree(handle);
	return error;
}

static void dbs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbs_ids[] = {
	{ .driver_info = 1 },
	{ },
};

#if defined(CONFIG_THERMAL)
#ifdef CONFIG_OF
static int get_cpu_cooling_dt_data(struct device *dev)
{
	struct sprd_cpu_cooling_platform_data *pdata = NULL;
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;
	struct device_node *np = dev->of_node;
	int max_freq[MAX_CPU_STATE];
	int max_core[MAX_CPU_STATE];
	int ret, i;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return ERR_PTR(-EINVAL);
	}
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return -1;
	}
	ret = of_property_read_u32(np, "state_num", &pdata->state_num);
	if(ret){
		dev_err(dev, "fail to get state_num\n");
		goto error;
	}
	ret = of_property_read_u32_array(np, "max_freq", max_freq, pdata->state_num);
	if(ret){
		dev_err(dev, "fail to get max_freq\n");
		goto error;
	}
	ret = of_property_read_u32_array(np, "max_core", max_core, pdata->state_num);
	if(ret){
		dev_err(dev, "fail to get max_core\n");
		goto error;
	}
	for (i = 0; i < pdata->state_num; ++i){
		pdata->cpu_state[i].max_freq = max_freq[i];
		pdata->cpu_state[i].max_core = max_core[i];
		dev_info(dev, "state:%d, max_freq:%d, max_core:%d\n", i, max_freq[i], max_core[i]);
	}
	c_info->max_state = pdata->state_num - 1;
	c_info->pdata = pdata;

	return 0;

error:
	return -1;
}
#endif

static int sprd_cpu_cooling_probe(struct platform_device *pdev)
{
	struct sprd_cpu_cooling_platform_data *pdata = NULL;
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;
	int ret = 0;

#ifdef CONFIG_OF
	ret = get_cpu_cooling_dt_data(&pdev->dev);
	if (ret < 0){
		return -1;
	}
#else
	pdata = dev_get_platdata(&pdev->dev);
	if (NULL == pdata){
		dev_err(&pdev->dev, "%s platform data is NULL!\n", __func__);
		return -1;
	}
	c_info->max_state = pdata->state_num - 1;
	c_info->pdata = pdata;
#endif

	c_info->cdev = thermal_cooling_device_register("thermal-cpufreq-0", 0,
						&sprd_cpufreq_cooling_ops);
	if (IS_ERR(c_info->cdev)){
		return PTR_ERR(c_info->cdev);
	}

	return ret;
}

static int sprd_cpu_cooling_remove(struct platform_device *pdev)
{
	thermal_cooling_device_unregister(thermal_cooling_info.cdev);
}

#ifdef CONFIG_OF
static const struct of_device_id cpu_cooling_of_match[] = {
       { .compatible = "sprd,sprd-cpu-cooling", },
       { }
};
#endif

static struct platform_driver cpu_cooling_driver = {
	.probe = sprd_cpu_cooling_probe,
	.remove = sprd_cpu_cooling_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-cpu-cooling",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(cpu_cooling_of_match),
#endif
	},
};
#endif

#ifndef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
struct input_handler dbs_input_handler = {
	.event		= dbs_input_event,
	.connect	= dbs_input_connect,
	.disconnect	= dbs_input_disconnect,
	.name		= "cpufreq_ond",
	.id_table	= dbs_ids,
};
#endif

static int __init cpufreq_gov_dbs_init(void)
{
	int i = 0;
	boot_done = jiffies + GOVERNOR_BOOT_TIME;
	register_pm_notifier(&sprdemand_gov_pm_notifier);
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&sprdemand_gov_earlysuspend_handler);
#endif

	input_wq = alloc_workqueue("iewq", WQ_MEM_RECLAIM|WQ_SYSFS, 1);

	if (!input_wq)
	{
		printk(KERN_ERR "Failed to create iewq workqueue\n");
		return -EFAULT;
	}

	INIT_WORK(&dbs_refresh_work, dbs_refresh_callback);


	g_sd_tuners = kzalloc(sizeof(struct sd_dbs_tuners), GFP_KERNEL);


	if(input_register_handler(&dbs_input_handler))
	{
		pr_err("[DVFS] input_register_handler failed\n");
	}

#if defined(CONFIG_THERMAL)
	platform_driver_register(&cpu_cooling_driver);
#endif

	return cpufreq_register_governor(&cpufreq_gov_sprdemand);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_sprdemand);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&sprdemand_gov_earlysuspend_handler);
#endif
	unregister_pm_notifier(&sprdemand_gov_pm_notifier);
#if defined(CONFIG_THERMAL)
	platform_driver_unregister(&cpu_cooling_driver);
#endif

	input_unregister_handler(&dbs_input_handler);

}

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_sprdemand' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
