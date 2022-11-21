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
#include <linux/input.h>
#include <linux/delay.h>

#include <linux/kthread.h>

#include <linux/cpufreq_limit.h>

#define CPU_HOTPLUG_BOOT_DONE_TIME	(50 * HZ)
#define SPRD_HOTPLUG_SCHED_PERIOD_TIME	(40)

static DEFINE_MUTEX(cpu_num_min_limit_lock);
static LIST_HEAD(cpu_num_min_limit_requests);

static struct kobject hotplug_kobj;
static struct task_struct *ksprd_hotplug;
static struct sd_dbs_tuners *g_sd_tuners = NULL;
static unsigned int boot_done;
static int log_enable = 1;

#ifdef CONFIG_SS_TOUCH_BOOST_CPU_HOTPLUG
struct semaphore tb_sem;
static struct task_struct *ksprd_tb;
bool g_is_suspend=false;
static unsigned long tp_time;
#if 0
static struct workqueue_struct *input_wq;
static DEFINE_PER_CPU(struct work_struct, dbs_refresh_work);
#endif
#endif

static struct delayed_work plugin_work;
static struct delayed_work unplug_work;
static struct work_struct plugin_request_work;
static struct work_struct unplug_request_work;

u64 g_prev_cpu_wall[4] = {0};
u64 g_prev_cpu_idle[4] = {0};

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
#define DEF_CPU_LOAD_DOWN_THRESHOLD		(50)
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

#define DEF_CPU_UP_MID_THRESHOLD		(80)
#define DEF_CPU_UP_HIGH_THRESHOLD		(90)
#define DEF_CPU_DOWN_MID_THRESHOLD		(30)
#define DEF_CPU_DOWN_HIGH_THRESHOLD		(40)

static unsigned int percpu_load[4] = {0};
#define MAX_CPU_NUM  (4)
#define MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE  (8)
#define MAX_PLUG_AVG_LOAD_SIZE (2)

struct sd_dbs_tuners {
	unsigned int ignore_nice;
	unsigned int sampling_rate;
	unsigned int sampling_down_factor;
	unsigned int up_threshold;
	unsigned int adj_up_threshold;
	unsigned int powersave_bias;
	unsigned int io_is_busy;

	unsigned int cpu_hotplug_disable;
	unsigned int is_suspend;
	unsigned int cpu_score_up_threshold;
	unsigned int load_critical;
	unsigned int load_hi;
	unsigned int load_mid;
	unsigned int load_light;
	unsigned int load_lo;
	int load_critical_score;
	int load_hi_score;
	int load_mid_score;
	int load_light_score;
	int load_lo_score;
	unsigned int cpu_down_threshold;
	unsigned int cpu_down_count;
	unsigned int cpu_num_limit;
	unsigned int cpu_num_min_limit;
	unsigned int cpu_up_mid_threshold;
	unsigned int cpu_up_high_threshold;
	unsigned int cpu_down_mid_threshold;
	unsigned int cpu_down_high_threshold;
	unsigned int up_window_size;
	unsigned int down_window_size;
};

static unsigned int ga_percpu_total_load[MAX_CPU_NUM][MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE] = {{0}};

static unsigned int cur_window_size[MAX_CPU_NUM] ={0};
static unsigned int prev_window_size[MAX_CPU_NUM] ={0};

static int cur_window_index[MAX_CPU_NUM] = {0};
static unsigned int cur_window_cnt[MAX_CPU_NUM] = {0};
static int first_window_flag[4] = {0};

static unsigned int sum_load[4] = {0};


#define mod(n, div) ((n) % (div))

static int a_score_sub[4][4][11]=
{
	{
		{0,0,0,0,0,0,0,0,5,5,10},
		{-5,-5,0,0,0,0,0,0,0,5,5},
		{-10,-5,0,0,0,0,0,0,0,5,5},
		{0,0,0,0,0,0,0,0,0,0,0}
	},
	{
		{0,0,0,0,0,0,0,3,5,10,20},
		{-10,-5,-5,0,0,0,0,0,5,5,10},
		{-20,-10,-5,0,0,0,0,0,5,5,10},
		{0,0,0,0,0,0,0,0,0,0,0}
	},
	{
		{0,0,0,0,0,0,0,10,20,20,30},
		{0,0,0,0,0,0,0,5,10,10,20},
		{0,0,0,0,0,0,0,0,5,5,10},
		{0,0,0,0,0,0,0,0,0,0,0}
	},
	{
		{0,0,0,0,0,0,0,20,30,30,50},
		{0,0,0,0,0,0,0,10,20,20,30},
		{0,0,0,0,0,0,0,0,5,10,20},
		{0,0,0,0,0,0,0,0,0,0,0}
	}
};

static int ga_samp_rate[11] = {100000,100000,100000,100000,100000,100000,50000,50000,30000,30000,30000};

static unsigned int a_sub_windowsize[8][6] =
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

static int cpu_score = 0;

static unsigned int cpufreq_min_limit = ULONG_MAX;
static unsigned int cpufreq_max_limit = 0;
static unsigned int dvfs_score_select = 4;
static unsigned int dvfs_unplug_select = 3;
static unsigned int dvfs_plug_select = 0;
static unsigned int dvfs_score_hi[4] = {0};
static unsigned int dvfs_score_mid[4] = {0};
static unsigned int dvfs_score_critical[4] = {0};

struct cpufreq_conf {
	struct clk 					*clk;
	struct clk 					*mpllclk;
	struct clk 					*tdpllclk;
	struct regulator 				*regulator;
	struct cpufreq_frequency_table			*freq_tbl;
	unsigned int					*vddarm_mv;
};

extern struct cpufreq_conf *sprd_cpufreq_conf;

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = cputime_to_usecs(cur_wall_time);

	return cputime_to_usecs(idle_time);
}

static inline u64 get_cpu_idle_time_sprd(unsigned int cpu, u64 *wall, int io_busy)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, io_busy ? wall : NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else if (!io_busy)
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static void __cpuinit sprd_plugin_one_cpu_ss(struct work_struct *work)
{
	int cpuid;

#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() < g_sd_tuners->cpu_num_limit) {
		cpuid = cpumask_next_zero(0, cpu_online_mask);
		if (!g_sd_tuners->cpu_hotplug_disable) {
			pr_info("!!  we gonna plugin cpu%d  !!\n", cpuid);
			cpu_up(cpuid);
		}
	}
#endif
	return;
}

static void sprd_unplug_one_cpu_ss()
{
	unsigned int cpuid = 0;

#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() > 1) {
		if (!g_sd_tuners->cpu_hotplug_disable) {
			cpuid = cpumask_next(0, cpu_online_mask);
			pr_info("!!  we gonna unplug cpu%d  !!\n",cpuid);
			cpu_down(cpuid);
		}
	}
#endif
	return;
}

static void sprd_unplug_cpus(struct work_struct *work)
{
	int cpu;
	int be_offline_num;

#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() > g_sd_tuners->cpu_num_limit) {
		be_offline_num = num_online_cpus() -
				g_sd_tuners->cpu_num_limit;
		for_each_online_cpu(cpu) {
			if (0 == cpu)
				continue;
			pr_info("!!  all gonna unplug cpu%d  !!\n", cpu);
			cpu_down(cpu);
			if (--be_offline_num <= 0)
				break;
		}
	}
#endif
	return;
}

static void sprd_plugin_cpus(struct work_struct *work)
{
	int cpu, max_num;
	int be_online_num = 0;

#ifdef CONFIG_HOTPLUG_CPU
	max_num = g_sd_tuners->cpu_num_limit;
	if (num_online_cpus() < max_num) {
		be_online_num = max_num - num_online_cpus();
		for_each_possible_cpu(cpu) {
			if (!cpu_online(cpu)) {
				pr_info("!! all gonna plugin cpu%d  !!\n",
						cpu);
				cpu_up(cpu);
				if (--be_online_num <= 0)
					break;
			}
		}
	}
#endif
	return;
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
			if (unlikely(rate[cpu] > 100))
				rate[cpu] = 1;

			rate[cpu] +=2;
			score = a_score_sub[dvfs_score_select % 4][num_online_cpus() - 1][load/10] * rate[cpu];
			rate[cpu] --;
		}
		else
		{
			score = a_score_sub[dvfs_score_select % 4][num_online_cpus() - 1][load/10];
			rate[cpu] = 1;
		}
	}
	pr_debug("[DVFS SCORE] rate[%d] %d load %d score %d\n",cpu,rate[cpu],load,score);
	return score;
}

#define MAX_ARRAY_SIZE  (10)
#define UP_LOAD_WINDOW_SIZE  (3)
#define DOWN_LOAD_WINDOW_SIZE  (3)
unsigned int load_array[CONFIG_NR_CPUS][MAX_ARRAY_SIZE] = { {0} };
unsigned int window_index[CONFIG_NR_CPUS] = {0};

static unsigned int sd_avg_load(int cpu, struct sd_dbs_tuners *sd_tuners,
			unsigned int load, bool up)
{
	unsigned int window_size;
	unsigned int count;
	unsigned int scale;
	unsigned int sum_scale = 0;
	unsigned int sum_load = 0;
	unsigned int window_tail = 0, window_head = 0;

	if (up) {
		window_size = sd_tuners->up_window_size;
	} else {
		window_size = sd_tuners->down_window_size;
		goto skip_load;
	}

	load_array[cpu][window_index[cpu]] = load;
	window_index[cpu]++;
	window_index[cpu] = mod(window_index[cpu], MAX_ARRAY_SIZE);

skip_load:
	if (!window_index[cpu])
		window_tail = MAX_ARRAY_SIZE - 1;
	else
		window_tail = window_index[cpu] - 1;

	window_head = mod(MAX_ARRAY_SIZE + window_tail - window_size + 1,
			MAX_ARRAY_SIZE);
	for (scale = 1, count = 0; count < window_size;
			scale += scale, count++) {
		pr_debug("%s load_array[%d][%d]: %d, scale: %d\n",
				up ? "up" : "down", cpu, window_head,
				load_array[cpu][window_head], scale);
		sum_load += (load_array[cpu][window_head] * scale);
		sum_scale += scale;
		window_head++;
		window_head = mod(window_head, MAX_ARRAY_SIZE);
	}

	return sum_load / sum_scale;
}

void sd_check_cpu_sprd(unsigned int load)
{
	unsigned int local_load = 0;
	unsigned int itself_avg_load = 0;
	struct unplug_work_info *puwi;
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	int cpu_num_limit = 0;
	struct cpu_num_min_limit_handle *handle = NULL;

	if (time_before(jiffies, boot_done))
		return;

	if (sd_tuners->cpu_hotplug_disable)
		return;

	pr_debug("efficient load %d, ---- online CPUs %d ----\n",
					load, num_online_cpus());

	mutex_lock(&cpu_num_min_limit_lock);
	g_sd_tuners->cpu_num_min_limit = 1;
	list_for_each_entry(handle, &cpu_num_min_limit_requests, node) {
		if (handle->cpu_num_min_limit > g_sd_tuners->cpu_num_min_limit)
			g_sd_tuners->cpu_num_min_limit = handle->cpu_num_min_limit;
	}
	mutex_unlock(&cpu_num_min_limit_lock);

	/* cpu plugin check */
	itself_avg_load = sd_avg_load(0, sd_tuners, load, true);
	pr_debug("up itself_avg_load %d\n", itself_avg_load);
	cpu_num_limit = min(sd_tuners->cpu_num_min_limit, sd_tuners->cpu_num_limit);
	if (num_online_cpus() < cpu_num_limit) {
		pr_debug("cpu_num_limit=%d, begin plugin cpu!\n",cpu_num_limit);
		schedule_delayed_work_on(0, &plugin_work, 0);
	}
	else {
		int cpu_up_threshold;

		if (num_online_cpus() == 1)
			cpu_up_threshold = sd_tuners->cpu_up_mid_threshold;
		else
			cpu_up_threshold = sd_tuners->cpu_up_high_threshold;

		if (itself_avg_load > cpu_up_threshold) {
			schedule_delayed_work_on(0, &plugin_work, 0);
			return;
		}
	}

	/* don't to check unplug if online cpus is less than or equal to cpu min limit */
	if(num_online_cpus() <= cpu_num_limit)
		return;

	/* cpu unplug check */
	cpu_num_limit = max(sd_tuners->cpu_num_min_limit, sd_tuners->cpu_num_limit);
	if (num_online_cpus() > 1) {
		int cpu_down_threshold;

		itself_avg_load = sd_avg_load(0, sd_tuners, load, false);
		pr_debug("down itself_avg_load %d\n", itself_avg_load);

		if (num_online_cpus() > 2)
			cpu_down_threshold = sd_tuners->cpu_down_high_threshold;
		else
			cpu_down_threshold = sd_tuners->cpu_down_mid_threshold;

		if((num_online_cpus() > cpu_num_limit) || (itself_avg_load < cpu_down_threshold))
			schedule_delayed_work_on(0, &unplug_work, 0);
	}
}

void dbs_check_cpu_sprd()
{
	unsigned int max_load = 0;
	unsigned int j;

	/* Get Absolute Load (in terms of freq for ondemand gov) */
	for_each_cpu(j, cpu_online_mask) {
		u64 cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;
		unsigned int load;
		int io_busy = 0;
		u64 prev_cpu_wall;
		u64 prev_cpu_idle;

		prev_cpu_wall = g_prev_cpu_wall[j];
		prev_cpu_idle = g_prev_cpu_idle[j];

		/*
		 * For the purpose of ondemand, waiting for disk IO is
		 * an indication that you're performance critical, and
		 * not that the system is actually idle. So do not add
		 * the iowait time to the cpu idle time.
		 */
		io_busy = g_sd_tuners->io_is_busy;
		cur_idle_time = get_cpu_idle_time_sprd(j, &cur_wall_time, io_busy);

		wall_time = (unsigned int)
			(cur_wall_time - prev_cpu_wall);

		idle_time = (unsigned int)
			(cur_idle_time - prev_cpu_idle);

		g_prev_cpu_wall[j] = cur_wall_time;
		g_prev_cpu_idle[j] = cur_idle_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
#if 0
		pr_debug("***[cpu %d]cur_idle_time %lld prev_cpu_idle %lld cur_wall_time %lld prev_cpu_wall %lld wall_time %ld idle_time %ld load %ld\n",
			j,cur_idle_time,prev_cpu_idle,cur_wall_time,prev_cpu_wall,wall_time,idle_time,load);
#endif
		if (load > max_load)
			max_load = load;
	}

	sd_check_cpu_sprd(max_load);
}

int _store_cpu_num_limit(unsigned int input)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;

	if (log_enable)
		printk("%s: input = %d\n", __func__, input);

	if(sd_tuners)
	{
		sd_tuners->cpu_num_limit = input;
		sd_check_cpu_sprd(50);
	}
	else
	{
		pr_info("[store_cpu_num_min_limit] current governor is not sprdemand\n");
		return -EINVAL;
	}

	return 0;
}

struct cpu_num_min_limit_handle *_store_cpu_num_min_limit(unsigned int input, char *label)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	struct cpu_num_min_limit_handle *handle = NULL;

	if (log_enable)
		printk("%s: input = %d\n", __func__, input);

	if(sd_tuners)
	{
		handle = kzalloc(sizeof(*handle), GFP_KERNEL);
		if (!handle)
			return ERR_PTR(-ENOMEM);

		if (input > sd_tuners->cpu_num_limit)
		{
			printk("%s input value(%d) is higher than cpu_num_limit(%d)!\n", __func__, input, sd_tuners->cpu_num_limit);
			input = sd_tuners->cpu_num_limit;
		}

		handle->cpu_num_min_limit = input;

		memset(handle->label, 0x0, sizeof(handle->label));
		if (strlen(label) < sizeof(handle->label))
			strcpy(handle->label, label);
		else
			strncpy(handle->label, label, sizeof(handle->label) - 1);

		mutex_lock(&cpu_num_min_limit_lock);
		list_add_tail(&handle->node, &cpu_num_min_limit_requests);
		mutex_unlock(&cpu_num_min_limit_lock);

#ifdef CONFIG_HOTPLUG_CPU
		if (input == CONFIG_NR_CPUS) {
			if (num_online_cpus() < sd_tuners->cpu_num_limit) {
				schedule_work_on(0, &plugin_request_work);
				do {
					msleep(5);
					pr_debug("wait for all cpu online!\n");
				} while (num_online_cpus() < sd_tuners->cpu_num_limit);
			}
		}
		else
#endif
		sd_check_cpu_sprd(50);
	}
	else
	{
		pr_info("[store_cpu_num_min_limit] current governor is not sprdemand\n");
		return ERR_PTR(-EINVAL);
	}

	return handle;
}

int cpu_num_min_limit_put(struct cpu_num_min_limit_handle *handle)
{
	if (handle == NULL || IS_ERR(handle))
		return -EINVAL;

	mutex_lock(&cpu_num_min_limit_lock);
	list_del(&handle->node);
	mutex_unlock(&cpu_num_min_limit_lock);

	kfree(handle);
	return 0;
}

static int should_io_be_busy(void)
{
	return 1;
}

static int sd_tuners_init(struct sd_dbs_tuners *tuners)
{
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice = 0;
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
	tuners->cpu_up_mid_threshold = DEF_CPU_UP_MID_THRESHOLD;
	tuners->cpu_up_high_threshold = DEF_CPU_UP_HIGH_THRESHOLD;
	tuners->cpu_down_mid_threshold = DEF_CPU_DOWN_MID_THRESHOLD;
	tuners->cpu_down_high_threshold = DEF_CPU_DOWN_HIGH_THRESHOLD;
	tuners->up_window_size = UP_LOAD_WINDOW_SIZE;
	tuners->down_window_size = DOWN_LOAD_WINDOW_SIZE;
	tuners->cpu_num_limit = nr_cpu_ids;
	tuners->cpu_num_min_limit = 1;
	if (tuners->cpu_num_limit > 1)
		tuners->cpu_hotplug_disable = false;

	INIT_DELAYED_WORK(&plugin_work, sprd_plugin_one_cpu_ss);
	INIT_DELAYED_WORK(&unplug_work, sprd_unplug_one_cpu_ss);
	INIT_WORK(&plugin_request_work, sprd_plugin_cpus);
	INIT_WORK(&unplug_request_work, sprd_unplug_cpus);
	return 0;
}

#if 0
static int sprd_hotplug()
{
	while (1) {
		if (time_before(jiffies, boot_done))
			continue;
		dbs_check_cpu_sprd();
		msleep(40);
	}
	return 0;
}
#else
static int sprd_hotplug(void *data)
{
	unsigned int timeout_ms = SPRD_HOTPLUG_SCHED_PERIOD_TIME; //40 ~ 100ms

	pr_debug("%s: wait for boot done..\n", __func__);

	while (time_before(jiffies, boot_done))
		msleep(timeout_ms * 10);

	pr_debug("%s:start!\n", __func__);

	do {
		dbs_check_cpu_sprd();
		msleep(timeout_ms);
	} while (!kthread_should_stop());

	pr_debug("%s:exit!\n", __func__);

	return 0;
}
#endif

static ssize_t dvfs_score_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_score_input %x\n",value);

	dvfs_score_select = (value >> 24) & 0x0f;
	if(dvfs_score_select < 4)
	{
		dvfs_score_critical[dvfs_score_select] = (value >> 16) & 0xff;
		dvfs_score_hi[dvfs_score_select] = (value >> 8) & 0xff;
		dvfs_score_mid[dvfs_score_select] = value & 0xff;
	}


	return count;
}

static ssize_t dvfs_score_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_score_select %d\n",dvfs_score_select);
	ret += snprintf(buf + ret,200,"dvfs_score_critical[1] = %d dvfs_score_hi[1] = %d dvfs_score_mid[1] = %d\n",dvfs_score_critical[1],dvfs_score_hi[1],dvfs_score_mid[1]);
	ret += snprintf(buf + ret,200,"dvfs_score_critical[2] = %d dvfs_score_hi[2] = %d dvfs_score_mid[2] = %d\n",dvfs_score_critical[2],dvfs_score_hi[2],dvfs_score_mid[2]);
	ret += snprintf(buf + ret,200,"dvfs_score_critical[3] = %d dvfs_score_hi[3] = %d dvfs_score_mid[3] = %d\n",dvfs_score_critical[3],dvfs_score_hi[3],dvfs_score_mid[3]);

	ret += snprintf(buf + ret,200,"percpu_total_load[0] = %d,%d->%d\n",
		percpu_load[0],ga_percpu_total_load[0][(cur_window_index[0] - 1 + MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE) % MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE],ga_percpu_total_load[0][cur_window_index[0]]);
	ret += snprintf(buf + ret,200,"percpu_total_load[1] = %d,%d->%d\n",
		percpu_load[1],ga_percpu_total_load[1][(cur_window_index[1] - 1 + MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE) % MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE],ga_percpu_total_load[1][cur_window_index[1]]);
	ret += snprintf(buf + ret,200,"percpu_total_load[2] = %d,%d->%d\n",
		percpu_load[2],ga_percpu_total_load[2][(cur_window_index[2] - 1 + MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE) % MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE],ga_percpu_total_load[2][cur_window_index[2]]);
	ret += snprintf(buf + ret,200,"percpu_total_load[3] = %d,%d->%d\n",
		percpu_load[3],ga_percpu_total_load[3][(cur_window_index[3] - 1 + MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE) % MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE],ga_percpu_total_load[3][cur_window_index[3]]);

	return strlen(buf) + 1;
}

static ssize_t dvfs_unplug_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_score_input %x\n",value);

	dvfs_unplug_select = (value >> 24) & 0x0f;
	if(dvfs_unplug_select > 7)
	{
		cur_window_size[0]= (value >> 8) & 0xff;
		cur_window_size[1]= (value >> 8) & 0xff;
		cur_window_size[2]= (value >> 8) & 0xff;
		cur_window_size[3]= (value >> 8) & 0xff;
	}
	return count;
}

static ssize_t dvfs_unplug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_unplug_select %d\n",dvfs_unplug_select);
	ret += snprintf(buf + ret,100,"cur_window_size[0] = %d\n",cur_window_size[0]);
	ret += snprintf(buf + ret,100,"cur_window_size[1] = %d\n",cur_window_size[1]);
	ret += snprintf(buf + ret,100,"cur_window_size[2] = %d\n",cur_window_size[2]);
	ret += snprintf(buf + ret,100,"cur_window_size[3] = %d\n",cur_window_size[3]);

	return strlen(buf) + 1;
}


static ssize_t dvfs_plug_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_plug_select %x\n",value);

	dvfs_plug_select = (value ) & 0x0f;
	return count;
}

static ssize_t dvfs_plug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_plug_select %d\n",dvfs_plug_select);

	return strlen(buf) + 1;
}
static ssize_t cpufreq_table_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,sprd_cpufreq_conf->freq_tbl,sizeof(* sprd_cpufreq_conf->freq_tbl));
	return sizeof(* sprd_cpufreq_conf->freq_tbl);
}

static ssize_t store_io_is_busy(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	sd_tuners->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		g_prev_cpu_idle[j] = get_cpu_idle_time_sprd(j,
				&g_prev_cpu_wall[j],should_io_be_busy());
	}
	return count;
}

static ssize_t show_io_is_busy(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->io_is_busy);
	return strlen(buf) + 1;
}

static ssize_t store_up_threshold(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
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

static ssize_t show_up_threshold(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->up_threshold);
	return strlen(buf) + 1;
}

static ssize_t store_sampling_down_factor(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;

	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	sd_tuners->sampling_down_factor = input;

	return count;
}

static ssize_t show_sampling_down_factor(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->sampling_down_factor);
	return strlen(buf) + 1;
}

static ssize_t store_ignore_nice(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == sd_tuners->ignore_nice) { /* nothing to do */
		return count;
	}
	sd_tuners->ignore_nice = input;

	return count;
}

static ssize_t show_ignore_nice(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->ignore_nice);
	return strlen(buf) + 1;
}

static ssize_t store_cpu_num_limit(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_num_limit = input;
	return count;
}

static ssize_t show_cpu_num_limit(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->cpu_num_limit);
	return strlen(buf) + 1;
}

DEFINE_MUTEX(cpu_num_min_limit_handle_lock);

static ssize_t store_cpu_num_min_limit(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	static struct cpu_num_min_limit_handle *handle = NULL;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}

	mutex_lock(&cpu_num_min_limit_handle_lock);
	if (handle!=NULL)
	{
		cpu_num_min_limit_put(handle);
		handle = NULL;
	}

	if (input > 1)
	{
		handle = _store_cpu_num_min_limit(input, current->comm);
	} else {
		printk("[%s] release num : %d\n", __func__, input);
	}

	mutex_unlock(&cpu_num_min_limit_handle_lock);
	return count;
}

static ssize_t show_cpu_num_min_limit(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct cpu_num_min_limit_handle *handle = NULL;
	int ret = 0;
	mutex_lock(&cpu_num_min_limit_lock);
	ret = snprintf(buf, 10,"%d\n",g_sd_tuners->cpu_num_min_limit);
	list_for_each_entry(handle, &cpu_num_min_limit_requests, node) {
		ret += snprintf(buf+ret, 30, "%s %d\n", handle->label, handle->cpu_num_min_limit);
	}
	mutex_unlock(&cpu_num_min_limit_lock);
	return strlen(buf) + 1;
}

static ssize_t store_cpu_score_up_threshold(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_score_up_threshold = input;
	return count;
}

static ssize_t show_cpu_score_up_threshold(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->cpu_score_up_threshold);
	return strlen(buf) + 1;
}

static ssize_t store_cpu_down_threshold(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_down_threshold = input;
	return count;
}

static ssize_t show_cpu_down_threshold(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->cpu_down_threshold);
	return strlen(buf) + 1;
}

static ssize_t store_cpu_down_count(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_down_count = input;
	return count;
}

static ssize_t show_cpu_down_count(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->cpu_down_count);
	return strlen(buf) + 1;
}

static ssize_t __ref store_cpu_hotplug_disable(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)

{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input, cpu;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}

	if (sd_tuners->cpu_hotplug_disable == input) {
		return count;
	}

	sd_tuners->cpu_hotplug_disable = input;

	smp_wmb();
	/* plug-in all offline cpu mandatory if we didn't
	 * enbale CPU_DYNAMIC_HOTPLUG
         */
#ifdef CONFIG_HOTPLUG_CPU
	if (sd_tuners->cpu_hotplug_disable &&
			num_online_cpus() < sd_tuners->cpu_num_limit) {
		schedule_work_on(0, &plugin_request_work);
		do {
			msleep(5);
			pr_debug("wait for all cpu online!\n");
		} while (num_online_cpus() < sd_tuners->cpu_num_limit);
	}
#endif
	return count;
}

static ssize_t show_cpu_hotplug_disable(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",g_sd_tuners->cpu_hotplug_disable);
	return strlen(buf) + 1;
}

static ssize_t store_cpu_up_mid_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	sd_tuners->cpu_up_mid_threshold = input;
	return count;
}

static ssize_t show_cpu_up_mid_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_up_mid_threshold);
}

static ssize_t store_cpu_up_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	sd_tuners->cpu_up_high_threshold = input;
	return count;
}

static ssize_t show_cpu_up_high_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_up_high_threshold);
}

static ssize_t store_cpu_down_mid_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	sd_tuners->cpu_down_mid_threshold = input;
	return count;
}

static ssize_t show_cpu_down_mid_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_down_mid_threshold);
}

static ssize_t store_cpu_down_high_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	sd_tuners->cpu_down_high_threshold = input;
	return count;
}

static ssize_t show_cpu_down_high_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_down_high_threshold);
}

static ssize_t store_up_window_size(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > MAX_ARRAY_SIZE || input < 1)
		return -EINVAL;

	sd_tuners->up_window_size = input;
	return count;
}

static ssize_t show_up_window_size(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->up_window_size);
}

static ssize_t store_down_window_size(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > MAX_ARRAY_SIZE || input < 1)
		return -EINVAL;

	sd_tuners->down_window_size = input;
	return count;
}

static ssize_t show_down_window_size(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->down_window_size);
}

static DEVICE_ATTR(cpufreq_table, 0440, cpufreq_table_show, NULL);
static DEVICE_ATTR(dvfs_score, 0660, dvfs_score_show, dvfs_score_store);
static DEVICE_ATTR(dvfs_unplug, 0660, dvfs_unplug_show, dvfs_unplug_store);
static DEVICE_ATTR(dvfs_plug, 0660, dvfs_plug_show, dvfs_plug_store);
static DEVICE_ATTR(io_is_busy, 0660,  show_io_is_busy,store_io_is_busy);
static DEVICE_ATTR(up_threshold, 0660,  show_up_threshold,store_up_threshold);
static DEVICE_ATTR(sampling_down_factor, 0660, show_sampling_down_factor,store_sampling_down_factor);
static DEVICE_ATTR(ignore_nice, 0660, show_ignore_nice,store_ignore_nice);
static DEVICE_ATTR(cpu_num_limit, 0660, show_cpu_num_limit,store_cpu_num_limit);
static DEVICE_ATTR(cpu_num_min_limit, 0660, show_cpu_num_min_limit,store_cpu_num_min_limit);
static DEVICE_ATTR(cpu_score_up_threshold, 0660, show_cpu_score_up_threshold,store_cpu_score_up_threshold);
static DEVICE_ATTR(cpu_down_threshold, 0660, show_cpu_down_threshold,store_cpu_down_threshold);
static DEVICE_ATTR(cpu_down_count, 0660, show_cpu_down_count,store_cpu_down_count);
static DEVICE_ATTR(cpu_hotplug_disable, 0660, show_cpu_hotplug_disable,store_cpu_hotplug_disable);
static DEVICE_ATTR(cpu_up_mid_threshold, 0660,
		show_cpu_up_mid_threshold, store_cpu_up_mid_threshold);
static DEVICE_ATTR(cpu_up_high_threshold, 0660,
		show_cpu_up_high_threshold, store_cpu_up_high_threshold);
static DEVICE_ATTR(cpu_down_mid_threshold, 0660,
		show_cpu_down_mid_threshold, store_cpu_down_mid_threshold);
static DEVICE_ATTR(cpu_down_high_threshold, 0660,
		show_cpu_down_high_threshold, store_cpu_down_high_threshold);
static DEVICE_ATTR(up_window_size, 0660,
		show_up_window_size, store_up_window_size);
static DEVICE_ATTR(down_window_size, 0660,
		show_down_window_size, store_down_window_size);

static struct attribute *g[] = {
	&dev_attr_cpufreq_table.attr,
	&dev_attr_dvfs_score.attr,
	&dev_attr_dvfs_unplug.attr,
	&dev_attr_dvfs_plug.attr,
	&dev_attr_io_is_busy.attr,
	&dev_attr_up_threshold.attr,
	&dev_attr_sampling_down_factor.attr,
	&dev_attr_ignore_nice.attr,
	&dev_attr_cpu_num_limit.attr,
	&dev_attr_cpu_num_min_limit.attr,
	&dev_attr_cpu_score_up_threshold.attr,
	&dev_attr_cpu_down_threshold.attr,
	&dev_attr_cpu_down_count.attr,
	&dev_attr_cpu_hotplug_disable.attr,
	&dev_attr_cpu_up_mid_threshold.attr,
	&dev_attr_cpu_up_high_threshold.attr,
	&dev_attr_cpu_down_mid_threshold.attr,
	&dev_attr_cpu_down_high_threshold.attr,
	&dev_attr_up_window_size.attr,
	&dev_attr_down_window_size.attr,
	NULL,
};

static struct kobj_type hotplug_dir_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
	.default_attrs	= g,
};


#ifdef CONFIG_SS_TOUCH_BOOST_CPU_HOTPLUG
static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{

	if (time_before(jiffies, boot_done))
		return;

	if (strcmp(handle->dev->name, "focaltech_ts"))
		return;

	if (time_after(jiffies, tp_time))
		tp_time = jiffies + HZ / 2;
	else
		return;

	up(&tb_sem);
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

	pr_info("[DVFS] dbs_input_connect register success\n");
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

struct input_handler dbs_input_handler = {
	.event		= dbs_input_event,
	.connect	= dbs_input_connect,
	.disconnect	= dbs_input_disconnect,
	.name		= "cpufreq_ond",
	.id_table	= dbs_ids,
};

static int sprd_tb_thread()
{
	while (1) {
                down(&tb_sem);
		if (num_online_cpus() < 3 && g_is_suspend == false)
			schedule_delayed_work_on(0, &plugin_work, 0);

	}
	return 0;
}
#endif

int cpu_core_thermal_limit(int cluster, int max_core)
{

	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;

	if (sd_tuners->cpu_num_limit <=  max_core) {
		sd_tuners->cpu_num_limit = max_core;
		return 0;
	}
	sd_tuners->cpu_num_limit = max_core;
	schedule_work_on(0, &unplug_request_work);

	return 0;
}

static void __init sprd_hotplug_init(void)
{
	int ret; 
	
	boot_done = jiffies + CPU_HOTPLUG_BOOT_DONE_TIME;
	
	g_sd_tuners = kzalloc(sizeof(struct sd_dbs_tuners), GFP_KERNEL);
	
	sd_tuners_init(g_sd_tuners);


#ifdef CONFIG_SS_TOUCH_BOOST_CPU_HOTPLUG
#if 0
	input_wq = alloc_workqueue("iewq", WQ_MEM_RECLAIM|WQ_SYSFS, 1);

	if (!input_wq)
	{
		printk(KERN_ERR "Failed to create iewq workqueue\n");
		return -EFAULT;
	}

	for_each_possible_cpu(i)
	{
		INIT_WORK(&per_cpu(dbs_refresh_work, i), dbs_refresh_callback);
	}
#endif
	tp_time = jiffies;

	if (input_register_handler(&dbs_input_handler))
		pr_err("[DVFS] input_register_handler failed\n");

	sema_init(&tb_sem, 0);

	ksprd_tb = kthread_create(sprd_tb_thread, NULL, "sprd_tb_thread");

	wake_up_process(ksprd_tb);

#endif
	ksprd_hotplug = kthread_create(sprd_hotplug, NULL, "sprd_hotplug");

	wake_up_process(ksprd_hotplug);

	ret = kobject_init_and_add(&hotplug_kobj, &hotplug_dir_ktype,
				   &(cpu_subsys.dev_root->kobj), "cpuhotplug");
	if (ret)
		pr_err("%s: Failed to add kobject for hotplug\n", __func__);
}

MODULE_AUTHOR("sprd");

MODULE_LICENSE("GPL");

module_init(sprd_hotplug_init);
