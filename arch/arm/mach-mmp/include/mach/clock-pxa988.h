#ifndef __MACH_CLK_PXA988_H
#define __MACH_CLK_PXA988_H

#include <linux/clk.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <plat/clock.h>
#include <linux/time.h>
#include <asm/cputime.h>

enum {
	CORE_900M = 902,
	CORE_1p1G = 1101,
	CORE_1p3G = 1283,
#if !defined(CONFIG_CORE_1248)
	CORE_1p2G = 1205,
#else
	CORE_1p2G = 1248,
#endif
	CORE_1p18G = 1183,
	CORE_1p5G = 1482,
};

extern unsigned long max_freq;
extern void pxa988_init_one_clock(struct clk *c);
extern int is_pxa988a0svc;
extern int is_pxa986a0svc;

extern int ddr_mode;
extern int is_pxa988a0svc;
/* Interface used to get components avaliable rates, unit Khz */
extern unsigned int pxa988_get_vpu_op_num(void);
extern unsigned int pxa988_get_vpu_op_rate(unsigned int index);

extern unsigned int pxa988_get_ddr_op_num(void);
extern unsigned int pxa988_get_ddr_op_rate(unsigned int index);

extern unsigned int get_profile(void);
extern int get_max_cpurate(void);

#ifdef CONFIG_DEBUG_FS
#define MAX_BREAKDOWN_TIME 11
/* use the largest possible number is 10 */
#define MAX_LPM_INDEX_DC  10

struct op_dcstat_info {
	unsigned int ppindex;
	unsigned long pprate;
	struct timespec prev_ts;
	long idle_time;		/* ms */
	long busy_time;		/* ms */
	/* used for core stat */
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
};

struct idle_dcstat_info {
	ktime_t all_idle_start;
	ktime_t all_idle_end;
	s64 total_all_idle;
	s64 total_all_idle_count;
	ktime_t all_active_start;
	ktime_t all_active_end;
	s64 total_all_active;
	s64 total_all_active_count;
	ktime_t M2_idle_start;
	s64 M2_idle_total;
	s64 M2_count;
	ktime_t D1P_idle_start;
	s64 D1P_idle_total;
	s64 D1p_count;
	ktime_t D1_idle_start;
	s64 D1_idle_total;
	s64 D1_count;
	ktime_t D2_idle_start;
	s64 D2_idle_total;
	s64 D2_count;
	s64 cal_duration;
	s64 all_idle_op_total[MAX_LPM_INDEX_DC];
	int all_idle_op_index;
	u64 all_idle_count[MAX_LPM_INDEX_DC];
};

struct clk_dc_stat_info {
	bool stat_start;
	struct op_dcstat_info *ops_dcstat;
	u32 power_mode;
	unsigned int idle_flag;
	unsigned int ops_stat_size;
	unsigned int curopindex;
	ktime_t C1_idle_start;
	ktime_t C1_idle_end;
	s64 C1_op_total[MAX_LPM_INDEX_DC];
	int C1_op_index;
	u64 C1_count[MAX_LPM_INDEX_DC];
	ktime_t C2_idle_start;
	ktime_t C2_idle_end;
	s64 C2_op_total[MAX_LPM_INDEX_DC];
	int C2_op_index;
	u64 C2_count[MAX_LPM_INDEX_DC];
	ktime_t breakdown_start;
	ktime_t breakdown_end;
	s64 breakdown_time_total[MAX_BREAKDOWN_TIME+1];
	s64 breakdown_time_count[MAX_BREAKDOWN_TIME+1];
};

struct clk_dcstat {
	struct clk *clk;
	struct clk_dc_stat_info clk_dcstat;
	struct list_head node;
};

enum clk_stat_msg {
	CLK_STAT_START = 0,
	CLK_STAT_STOP,
	CLK_STATE_ON,
	CLK_STATE_OFF,
	CLK_RATE_CHANGE,
	CLK_DYNVOL_CHANGE,
	CPU_IDLE_ENTER,
	CPU_IDLE_EXIT,
	CPU_M2_OR_DEEPER_ENTER,
};

static inline void clk_get_lock(struct clk *clk)
{
	if (clk->cansleep)
		mutex_lock(&clk->mutex);
	else
		spin_lock(&clk->spinlock);
}

static inline void clk_release_lock(struct clk *clk)
{
	if (clk->cansleep)
		mutex_unlock(&clk->mutex);
	else
		spin_unlock(&clk->spinlock);
}


/* function used for clk duty cycle stat */
static inline long ts2ms(struct timespec cur, struct timespec prev)
{
	return (cur.tv_sec - prev.tv_sec) * MSEC_PER_SEC + \
		(cur.tv_nsec - prev.tv_nsec) / NSEC_PER_MSEC;
}

static inline u32 calculate_dc(u32 busy, u32 total, u32 *fraction)
{
	u32 result, remainder;
	result = div_u64_rem((u64)(busy * 100), total, &remainder);
	*fraction = remainder * 100 / total;
	return result;
}

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu,
		u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu,
		cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

extern struct dentry *stat;
extern int pxa988_clk_register_dcstat(struct clk *clk,
	unsigned long *opt, unsigned int opt_size);
extern int pxa988_clk_dcstat_event(struct clk *clk,
	enum clk_stat_msg msg, unsigned int tgtstate);
extern int pxa988_show_dc_stat_info(struct clk *clk,
	char *buf, ssize_t size);
extern int pxa988_start_stop_dc_stat(struct clk *clk,
	unsigned int start);
extern void pxa988_cpu_dcstat_event(unsigned int cpu,
		enum clk_stat_msg msg, unsigned int tgtop);
#endif

#ifdef CONFIG_CPU_PXA988
#define DDR_COMBINDEDCLK_SOLUTION	1
#endif

#ifdef DDR_COMBINDEDCLK_SOLUTION
extern int mck4_wr_enabled;
extern int trigger_bind2ddr_clk_rate(unsigned long ddr_rate);
#endif

#define cpu_is_z1z2() \
	((cpu_is_pxa988_z1() || cpu_is_pxa988_z2()\
	|| cpu_is_pxa986_z1() || cpu_is_pxa986_z2()))

#endif /* __MACH_CLK_PXA988_H */
