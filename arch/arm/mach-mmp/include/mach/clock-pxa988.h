#ifndef __MACH_CLK_PXA988_H
#define __MACH_CLK_PXA988_H

#include <linux/clk.h>
#include <plat/clock.h>
#include <linux/time.h>
#include <asm/cputime.h>

extern void pxa988_init_one_clock(struct clk *c);
extern int is_pxa988a0svc;
extern int ddr_mode;
/* Interface used to get components avaliable rates, unit Khz */
extern unsigned int pxa988_get_vpu_op_num(void);
extern unsigned int pxa988_get_vpu_op_rate(unsigned int index);

extern unsigned int pxa988_get_ddr_op_num(void);
extern unsigned int pxa988_get_ddr_op_rate(unsigned int index);

extern unsigned int get_profile(void);

#ifdef CONFIG_DEBUG_FS
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

struct clk_dc_stat_info {
	bool stat_start;
	struct op_dcstat_info *ops_dcstat;
	unsigned int ops_stat_size;
	unsigned int curopindex;
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

extern struct dentry *stat;
extern int pxa988_clk_register_dcstat(struct clk *clk,
	unsigned long *opt, unsigned int opt_size);
extern int pxa988_clk_dcstat_event(struct clk *clk,
	enum clk_stat_msg msg, unsigned int tgtstate);
extern int pxa988_show_dc_stat_info(struct clk *clk,
	char *buf, ssize_t size);
extern int pxa988_start_stop_dc_stat(struct clk *clk,
	unsigned int start);
#endif

#ifdef CONFIG_CPU_PXA988
#define Z1_MCK4_SYNC_WORKAROUND		1
#endif

#ifdef Z1_MCK4_SYNC_WORKAROUND
extern int mck4_wr_enabled;
extern int trigger_bind2ddr_clk_rate(unsigned long ddr_rate);
#endif

#define cpu_is_z1z2() \
	((cpu_is_pxa988_z1() || cpu_is_pxa988_z2()\
	|| cpu_is_pxa986_z1() || cpu_is_pxa986_z2()))

#endif /* __MACH_CLK_PXA988_H */
