/*
 * linux/drivers/devfreq/mck4_memorybus.c
 *
 *  Copyright (C) 2012 Marvell International Ltd.
 *  All rights reserved.
 *
 *  2012-03-16  Yifan Zhang <zhangyf@marvell.com>
 *		Zhoujie Wu<zjwu@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <plat/devfreq.h>
#include <mach/dvfs.h>

#include <plat/pxa_trace.h>

#define DDR_FREQ_MAX 8
#define DDR_DEVFREQ_UPTHRESHOLD 65
#define DDR_DEVFREQ_DOWNDIFFERENTIAL 5

#define KHZ_TO_HZ   1000

struct ddr_devfreq_data {
	struct devfreq *pdev_ddr;
	struct clk *ddr_clk;
	u32 mc_base_pri;/* primary addr */
	u32 mc_base_sec;/* secondray addr, only valid if interleave_is_on */
	/* used for platform have more than one DDR controller */
	u32 interleave_is_on;
	/* DDR frequency table used for platform */
	u32 ddr_freq_tbl[DDR_FREQ_MAX];	/* unit Khz */
	u32 ddr_freq_tbl_len;
	unsigned long last_polled_at;
	spinlock_t lock;

	/* used for performance optimization */
	u32 high_upthrd_swp;
	u32 high_upthrd;

	/* used for debug interface */
	atomic_t is_disabled;
	struct timespec last_ts;
};

static struct ddr_devfreq_data *cur_data;
static int dvfs_notifier_freq(struct notifier_block *nb,
			      unsigned long val, void *data)
{
	struct dvfs_freqs *freqs = (struct dvfs_freqs *)data;
	if (strcmp(freqs->dvfs->clk_name, cur_data->ddr_clk->name))
		return 0;

	switch (val) {
	case DVFS_FREQ_POSTCHANGE:
		cur_data->pdev_ddr->previous_freq = freqs->new;
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block notifier_freq_block = {
	.notifier_call = dvfs_notifier_freq,
};

/* notifier to change the devfreq govoner's upthreshold */
static int upthreshold_freq_notifer_call(struct notifier_block *nb,
	       unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct devfreq *devfreq = cur_data->pdev_ddr;
	struct devfreq_simple_ondemand_data *gov_data;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	mutex_lock(&devfreq->lock);
	gov_data = devfreq->data;
	if (freq->new >= cur_data->high_upthrd_swp)
		gov_data->upthreshold = cur_data->high_upthrd;
	else
		gov_data->upthreshold = DDR_DEVFREQ_UPTHRESHOLD;
	mutex_unlock(&devfreq->lock);

	trace_pxa_ddr_upthreshold(gov_data->upthreshold);

	return NOTIFY_OK;
}

static struct notifier_block upthreshold_freq_notifier = {
	.notifier_call = upthreshold_freq_notifer_call
};

/* default using ondemand governor */
static const struct devfreq_governor *default_gov =
	&devfreq_simple_ondemand;

/* default using 70% as upthreshold and 5% as downthreshold */
static struct devfreq_simple_ondemand_data ddr_ondemand_data = {
	.upthreshold = DDR_DEVFREQ_UPTHRESHOLD,
	.downdifferential = DDR_DEVFREQ_DOWNDIFFERENTIAL,
};

#define PERF_CONFIG_REG 0x440
#define PERF_STATUS_REG 0x444
#define PERF_CONTRL_REG 0x448
#define PERF_CNT_0_REG  0x450
#define PERF_CNT_1_REG  0x454
#define PERF_CNT_2_REG  0x458
#define PERF_CNT_3_REG  0x45C

/*
 *  reg[0] ddr_totalticks
 *  reg[1] ddr_DPC_idle
 *  reg[2] ddr_rw_cmd
 *  reg[3] axi_req
 */
struct ddr_cycle_type {
	u64 reg[4];
};

static unsigned int ddr_perf_cnt_old[4];
static struct ddr_cycle_type ddr_ticks_array[DDR_FREQ_MAX];
static int is_ddr_stats_working;

static void reset_ddr_performance_counter(void)
{
	void *mck4_base_vaddr = (void *)cur_data->mc_base_pri;

	if (!mck4_base_vaddr) {
		dev_err(&cur_data->pdev_ddr->dev,
			"ddr_perf: mcu_base map failure!\n");
		return ;
	}

	/* Step1: Write to Performance Counter Configuration Register to
	  disable interrupts and counters */
	writel(0x0, mck4_base_vaddr + PERF_CONFIG_REG);

	/* Step2: Write to Performance Counter Control Register to select
	   the desired settings
	   bit18:16 0x0 = Divide clock by 1
	   bit4     0x1 = Continue counting on any counter overflow
	   bit0     0x0 = Enabled counters begin counting */
	writel(0x10, mck4_base_vaddr + PERF_CONTRL_REG);

	/* Step3: Write to Performance Counter Register to set the starting
	   value */
	writel(0x0, mck4_base_vaddr + PERF_CNT_0_REG);
	writel(0x0, mck4_base_vaddr + PERF_CNT_1_REG);
	writel(0x0, mck4_base_vaddr + PERF_CNT_2_REG);
	writel(0x0, mck4_base_vaddr + PERF_CNT_3_REG);

	/* clear overflow flag */
	writel(0xf, mck4_base_vaddr + PERF_STATUS_REG);

	/* reset old data */
	memset(ddr_perf_cnt_old, 0, sizeof(ddr_perf_cnt_old));
}

static void stop_ddr_performance_counter(void)
{
	void *mck4_base_vaddr = (void *)cur_data->mc_base_pri;
	/* Step1: Write to Performance Counter Configuration Register to
	  disable interrupts and counters */
	writel(0x0, mck4_base_vaddr + PERF_CONFIG_REG);
}

static void start_ddr_performance_counter(void)
{
	void *mck4_base_vaddr = (void *)cur_data->mc_base_pri;
	/* Step4: Write to Performance Counter Configuration Register to
	   enable counters and/or interrupts, choose the events for counters
	   simultaneously.
	   bit 0: 7 cnt0, event=0x00, clock cycles
	   bit 8:15 cnt1, event=0x01, DPC idle cycles
	   bit16:23 cnt2, event=0x14, Read + Write command count
	   bit24:31 cnt3, event=0x04, no bus utilization when not idle */
	writel((((0x80 | 0x00) <<  0) | ((0x80 | 0x01) <<  8) |
		((0x80 | 0x14) << 16) | ((0x80 | 0x04) << 24)),
		mck4_base_vaddr + PERF_CONFIG_REG);
}

static int ddr_rate2_index(u32 rate)
{
	int i;
	for (i = 0; i < cur_data->ddr_freq_tbl_len; i++)
		if (cur_data->ddr_freq_tbl[i] == rate)
			return i;
	dev_err(&cur_data->pdev_ddr->dev, "unknow ddr rate %d\n", rate);
	return -1;
}

static u32 ddr_index2_rate(int index)
{
	if ((index >= 0) && (index < cur_data->ddr_freq_tbl_len))
		return cur_data->ddr_freq_tbl[index];
	else {
		dev_err(&cur_data->pdev_ddr->dev,
			"unknow ddr index %d\n", index);
		return 0;
	}
}

static void update_ddr_performance_data(void)
{
	void *mck4_base_vaddr = (void *)cur_data->mc_base_pri;
	unsigned int reg[4], i, overflow_flag;
	int ddr_idx;
	unsigned long flags;

	spin_lock_irqsave(&cur_data->lock, flags);

	/* stop counters, to keep data synchronized */
	stop_ddr_performance_counter();

	/* overflow flag seems has some hardware issue, when the counters
	   overflows, the overflow flag can't be set reliably  */
	overflow_flag = readl(mck4_base_vaddr + PERF_STATUS_REG) & 0xf;

	ddr_idx = ddr_rate2_index(clk_get_rate(cur_data->ddr_clk) / KHZ_TO_HZ);

	if ((ddr_idx >= 0) && (ddr_idx < cur_data->ddr_freq_tbl_len)) {
		for (i = 0; i < 4; i++) {
			reg[i] = readl(mck4_base_vaddr +
					PERF_CNT_0_REG + i * 4);

			if (overflow_flag & (1 << i)) {
				dev_dbg(&cur_data->pdev_ddr->dev,
					 "DDR perf counter overflow!\n");
				ddr_ticks_array[ddr_idx].reg[i] +=
						0x100000000LLU + reg[i] -
						ddr_perf_cnt_old[i];
			} else
				ddr_ticks_array[ddr_idx].reg[i] += reg[i] -
							ddr_perf_cnt_old[i];

			ddr_perf_cnt_old[i] = reg[i];
		}
	} else
		dev_err(&cur_data->pdev_ddr->dev, "%s: invalid ddr_idx %u\n",
			__func__, ddr_idx);

	spin_unlock_irqrestore(&cur_data->lock, flags);
}

static int __init ddr_perf_counter_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&cur_data->lock, flags);
	memset(ddr_ticks_array, 0,
		sizeof(ddr_ticks_array));
	reset_ddr_performance_counter();
	start_ddr_performance_counter();
	spin_unlock_irqrestore(&cur_data->lock, flags);

	return 0;
}

static inline void get_mc_cnt(u32 mc_base, u32 *total,
			u32 *idle, struct ddr_devfreq_data *data)
{
	unsigned long flags;
	unsigned int total_ticks, no_util_ticks, rw_ticks;

	spin_lock_irqsave(&data->lock, flags);

	/* stop counters, to keep data synchronized */
	stop_ddr_performance_counter();

	total_ticks = readl(mc_base + PERF_CNT_0_REG);
	rw_ticks = readl(mc_base + PERF_CNT_2_REG);
	no_util_ticks = readl(mc_base + PERF_CNT_3_REG);
	*total = total_ticks;
	*idle = total_ticks - rw_ticks * 4;

	start_ddr_performance_counter();

	spin_unlock_irqrestore(&data->lock, flags);

}

static inline void reset_mc_cnt(u32 mc_base,
			struct ddr_devfreq_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	reset_ddr_performance_counter();
	start_ddr_performance_counter();
	spin_unlock_irqrestore(&data->lock, flags);
}

/* calculate ddr workload according to busy and total time, unit percent */
static inline u32 cal_workload(unsigned long busy_time,
	unsigned long total_time)
{
	u64 tmp0, tmp1;

	if (!total_time || !busy_time)
		return 0;
	tmp0 = busy_time * 100;
	tmp1 = div_u64(tmp0, total_time);
	return (u32)tmp1;
}

/*
 * get the mck4 total and idle performance cnt
 * if interleave is on, get the busy one of primary and secondary mc.
 * else get the primary mc only.
 */
static void get_ddr_cycles(struct ddr_devfreq_data *data,
	unsigned long *total, unsigned long *busy)
{
	u32 total_pri, idle_pri;
	u32 total_sec, idle_sec;

	get_mc_cnt(data->mc_base_pri, &total_pri, &idle_pri, data);

	if (data->interleave_is_on) {
		dev_dbg(&data->pdev_ddr->dev, "interleave is on\n");
		get_mc_cnt(data->mc_base_sec, &total_sec, &idle_sec, data);
		if (total_sec < total_pri) {
			*total = total_sec;
			*busy = total_sec - idle_sec;
		} else {
			*total = total_pri;
			*busy = total_pri - idle_pri;
		}
		dev_dbg(&data->pdev_ddr->dev,
			"t_pri %u, t_sec %u, i_pri %u, i_sec %u\n",
			total_pri, total_sec,
			idle_pri, idle_sec);
	} else {
		dev_dbg(&data->pdev_ddr->dev, "interleave is off\n");
		*total = total_pri;
		*busy = total_pri - idle_pri;
		dev_dbg(&data->pdev_ddr->dev,
			"t_pri %u, i_pri %u\n",
			total_pri, idle_pri);
	}
}

/* reset both mc1 and mc0 */
static void reset_ddr_counters(struct ddr_devfreq_data *data)
{
	reset_mc_cnt(data->mc_base_pri, data);

	if (data->interleave_is_on)
		reset_mc_cnt(data->mc_base_sec, data);
}

static int ddr_get_dev_status(struct device *dev,
			       struct devfreq_dev_status *stat)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	struct devfreq *df;
	u32 workload;
	unsigned long now = jiffies;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	df = data->pdev_ddr;

	stat->current_frequency = clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;
	/* ignore the profiling if it is not from devfreq_monitor or there is no profiling */
	if (!df->polling_jiffies ||
		(df->polling_jiffies && data->last_polled_at &&
		time_before(now, (data->last_polled_at + df->polling_jiffies)))) {
		dev_dbg(dev, "No profiling or "
			"interval is not expired %lu, %lu, %lu\n",
			df->polling_jiffies, now, data->last_polled_at);
		return -EINVAL;
	}

	get_ddr_cycles(data, &stat->total_time, &stat->busy_time);
	data->last_polled_at = now;

	if (is_ddr_stats_working)
		update_ddr_performance_data();
	reset_ddr_counters(data);

	/* Ajust the workload calculation here to align with devfreq governor */
	if (stat->busy_time >= (1 << 24) || stat->total_time >= (1 << 24)) {
		stat->busy_time >>= 7;
		stat->total_time >>= 7;
	}

	workload = cal_workload(stat->busy_time, stat->total_time);

	dev_dbg(dev, "workload is %d precent\n", workload);
	dev_dbg(dev, "busy time is 0x%x, %u\n", (u32)stat->busy_time,
		 (u32)stat->busy_time);
	dev_dbg(dev, "total time is 0x%x, %u\n\n",
		(u32)stat->total_time,
		(u32)stat->total_time);

	trace_pxa_ddr_workload(workload, stat->current_frequency);
	return 0;
}

static int ddr_set_rate(struct ddr_devfreq_data *data, unsigned long tgt_rate)
{
	unsigned long cur_freq, tgt_freq;

	cur_freq = clk_get_rate(data->ddr_clk);
	tgt_freq = tgt_rate * KHZ_TO_HZ;

	if (cur_freq == tgt_freq)
		return 0;

	dev_dbg(&data->pdev_ddr->dev, "%s: curfreq %lu, tgtfreq %lu\n",
		__func__, cur_freq, tgt_freq);

	/* update performance data before ddr clock change */
	if (is_ddr_stats_working)
		update_ddr_performance_data();

	/* clk_set_rate will find a frequency larger or equal tgt_freq */
	clk_set_rate(data->ddr_clk, tgt_freq);

	/* re-init ddr performance counters after ddr clock change */
	if (is_ddr_stats_working)
		reset_ddr_counters(data);

	return 0;
}

static int ddr_target(struct device *dev, unsigned long *freq, u32 flags)
{
	unsigned long tgt_freq;
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	struct devfreq *df;
	u32 *ddr_freq_table, ddr_freq_len;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);

	/* in normal case ddr fc will NOT be disabled */
	if (unlikely(atomic_read(&data->is_disabled))) {
		df = data->pdev_ddr;
		/*
		 * this function is called with df->locked, it is safe to
		 * read the polling_ms here
		 */
		if (df->profile->polling_ms)
			dev_err(dev, "[WARN] ddr ll fc is disabled from "\
				"debug interface, suggest to disable "\
				"the profiling at first!\n");
		*freq = clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;
		return 0;
	}

	ddr_freq_table = &data->ddr_freq_tbl[0];
	ddr_freq_len = data->ddr_freq_tbl_len;
	dev_dbg(dev, "%s: %u\n", __func__, (u32)*freq);
	/*
	 * if freq is u32 max, change ddr to the max freq.
	 * because pm_qos_update_request take u32 parameter, so
	 * there would be truncation.
	 */
	if (*freq == UINT_MAX)
		*freq = ddr_freq_table[ddr_freq_len - 1];

	tgt_freq = min(*freq,
			(unsigned long)ddr_freq_table[ddr_freq_len - 1]);
	ddr_set_rate(data, tgt_freq);

	*freq = clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;
	return 0;
}

/* interface to change the switch point of high aggresive upthreshold */
static ssize_t high_swp_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	struct devfreq *devfreq;
	unsigned int swp;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	devfreq = data->pdev_ddr;

	if (0x1 != sscanf(buf, "%u", &swp)) {
		dev_err(dev, "<ERR> wrong parameter\n");
		return -E2BIG;
	}

	mutex_lock(&devfreq->lock);
	data->high_upthrd_swp = swp;
	mutex_unlock(&devfreq->lock);

	return size;
}

static ssize_t high_swp_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	return sprintf(buf, "%u\n", data->high_upthrd_swp);
}

/* interface to change the aggresive upthreshold value */
static ssize_t high_upthrd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	struct devfreq *devfreq;
	unsigned int high_upthrd;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	devfreq = data->pdev_ddr;

	if (0x1 != sscanf(buf, "%u", &high_upthrd)) {
		dev_err(dev, "<ERR> wrong parameter\n");
		return -E2BIG;
	}

	mutex_lock(&devfreq->lock);
	data->high_upthrd = high_upthrd;
	mutex_unlock(&devfreq->lock);

	return size;
}

static ssize_t high_upthrd_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	return sprintf(buf, "%u\n", data->high_upthrd);
}

/* debug interface used to totally disable ddr fc */
static ssize_t disable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	int is_disabled;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);

	if (0x1 != sscanf(buf, "%d", &is_disabled)) {
		dev_err(dev, "<ERR> wrong parameter\n");
		return -E2BIG;
	}

	is_disabled = !!is_disabled;
	if (is_disabled == atomic_read(&data->is_disabled)) {
		dev_info(dev, "[WARNING] ddr fc is already %s\n",
			atomic_read(&data->is_disabled) ? \
			"disabled" : "enabled");
		return size;
	}

	if (is_disabled)
		atomic_inc(&data->is_disabled);
	else
		atomic_dec(&data->is_disabled);

	dev_info(dev, "[WARNING]ddr fc is %s from debug interface!\n",\
		atomic_read(&data->is_disabled) ? "disabled" : "enabled");
	return size;
}

static ssize_t disable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	return sprintf(buf, "ddr fc is_disabled = %d\n",
		 atomic_read(&data->is_disabled));
}

/*
 * Debug interface used to change ddr rate.
 * It will ignore all devfreq and Qos requests.
 * Use interface disable_ddr_fc prior to it.
 */
static ssize_t ddr_freq_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	int freq;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);

	if (!atomic_read(&data->is_disabled)) {
		dev_err(dev, "<ERR> It will change ddr rate,"\
			"disable ddr fc at first\n");
		return -EPERM;
	}

	if (0x1 != sscanf(buf, "%d", &freq)) {
		dev_err(dev, "<ERR> wrong parameter, "\
			"echo freq > ddr_freq to set ddr rate(unit Khz)\n");
		return -E2BIG;
	}
	clk_set_rate(data->ddr_clk, freq * KHZ_TO_HZ);

	dev_dbg(dev, "ddr freq read back: %lu\n", \
		clk_get_rate(data->ddr_clk) / KHZ_TO_HZ);

	return size;
}

static ssize_t ddr_freq_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);
	return sprintf(buf, "current ddr freq is: %lu\n",
		 clk_get_rate(data->ddr_clk) / KHZ_TO_HZ);
}

/* used to collect ddr cnt during 20ms */
static ssize_t dp_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	int len = 0;
	unsigned long flags;

	int i, j, k;
	u32 glob_ratio, idle_ratio, busy_ratio, data_ratio;
	u32 util_ratio;
	u32 tmp_total, tmp_dpc_idle, tmp_rw_cmd, tmp_no_util;
	u64 glob_ticks;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);


	/* ddr ticks show */
	len += sprintf(buf + len, "\nddr_ticks operating point list:\n");

	len += sprintf(buf + len, "idx|dmcfs|  total_ticks   |"
			" DPC_idle_ticks |   R+W_cmd_cnt  |no_util_not_idle\n");

	len += sprintf(buf + len, "----------------------------------"
			"-------------------------------------------\n");

	spin_lock_irqsave(&data->lock, flags);

	for (i = 0; i < cur_data->ddr_freq_tbl_len; i++) {
		len += sprintf(buf + len,
			"%3d|%5u|%16llu|%16llu|%16llu|%16llu\n",
			i, ddr_index2_rate(i)/1000,
			ddr_ticks_array[i].reg[0],
			ddr_ticks_array[i].reg[1],
			ddr_ticks_array[i].reg[2],
			ddr_ticks_array[i].reg[3]);
	}
	spin_unlock_irqrestore(&data->lock, flags);

	len += sprintf(buf + len, "\n");

	/* ddr duty cycle show */
	glob_ticks = 0;

	len += sprintf(buf + len,
			"\nddr_duty_cycle operating point list:\n");

	len += sprintf(buf + len, "idx|dmcfs|glob_ratio|idle_ratio"
			"|busy_ratio|data_ratio|util_ratio\n");

	len += sprintf(buf + len, "-----------------------------"
			"------------------------------------\n");

	spin_lock_irqsave(&data->lock, flags);

	for (i = 0; i < cur_data->ddr_freq_tbl_len; i++)
		glob_ticks += ddr_ticks_array[i].reg[0];

	k = 0;
	while ((glob_ticks >> k) > 0x7FFF)
		k++;

	for (i = 0; i < cur_data->ddr_freq_tbl_len; i++) {

		if ((u32)(glob_ticks>>k) != 0)
			glob_ratio = (u32)(ddr_ticks_array[i].reg[0]>>k)
					* 100000 / (u32)(glob_ticks>>k) + 5;
		else
			glob_ratio = 0;

		j = 0;
		while ((ddr_ticks_array[i].reg[0] >> j) > 0x7FFF)
			j++;

		tmp_total = ddr_ticks_array[i].reg[0] >> j;
		tmp_dpc_idle = ddr_ticks_array[i].reg[1] >> j;
		tmp_rw_cmd = ddr_ticks_array[i].reg[2] >> j;
		tmp_no_util = ddr_ticks_array[i].reg[3] >> j;


		if (tmp_total != 0) {
			idle_ratio = (tmp_total - tmp_rw_cmd * 4 - tmp_no_util)
					* 100000 / tmp_total + 5;

			busy_ratio = (tmp_rw_cmd * 4 + tmp_no_util) * 100000
					/ tmp_total + 5;

			data_ratio = tmp_rw_cmd * 4 * 100000 / tmp_total + 5;

			util_ratio = tmp_rw_cmd * 4 * 100000
				/ (tmp_rw_cmd * 4 + tmp_no_util) + 5;
		} else {
			idle_ratio = 0;
			busy_ratio = 0;
			data_ratio = 0;
			util_ratio = 0;
		}

		len += sprintf(buf + len, "%3d|%5u|%6u.%02u%%|%6u.%02u%%"
			"|%6u.%02u%%|%6u.%02u%%|%6u.%02u%%\n",
			       i, ddr_index2_rate(i)/1000,
				glob_ratio/1000, (glob_ratio%1000)/10,
				idle_ratio/1000, (idle_ratio%1000)/10,
				busy_ratio/1000, (busy_ratio%1000)/10,
				data_ratio/1000, (data_ratio%1000)/10,
				util_ratio/1000, (util_ratio%1000)/10);
	}
	spin_unlock_irqrestore(&data->lock, flags);

	len += sprintf(buf + len, "\n");

	return len;
}

/* used to collect ddr cnt during a time */
static ssize_t dp_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct platform_device *pdev;
	struct ddr_devfreq_data *data;
	unsigned int cap_flag;
	unsigned long flags;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);

	sscanf(buf, "%u", &cap_flag);

	if (cap_flag == 1) {
		spin_lock_irqsave(&data->lock, flags);
		memset(ddr_ticks_array, 0,
			sizeof(ddr_ticks_array));
		spin_unlock_irqrestore(&data->lock, flags);

		is_ddr_stats_working = 1;

	} else if (cap_flag == 0) {
		is_ddr_stats_working = 0;
	} else
		dev_err(&data->pdev_ddr->dev,
			"echo 1 > ddr_profiling to reset and start\n"
			"echo 0 > ddr_profiling to stop\n"
			"cat ddr_profiling to show ddr ticks and duty cycle\n");

	return size;
}

static DEVICE_ATTR(high_upthrd_swp, S_IRUGO | S_IWUSR, \
	high_swp_show, high_swp_store);
static DEVICE_ATTR(high_upthrd, S_IRUGO | S_IWUSR, \
	high_upthrd_show, high_upthrd_store);
static DEVICE_ATTR(disable_ddr_fc, S_IRUGO | S_IWUSR, \
	disable_show, disable_store);
static DEVICE_ATTR(ddr_freq, S_IRUGO | S_IWUSR, ddr_freq_show, ddr_freq_store);
static DEVICE_ATTR(ddr_profiling, S_IRUGO | S_IWUSR, dp_show, dp_store);

static struct devfreq_dev_profile ddr_devfreq_profile = {
	/* FIXME turn off profiling until ddr devfreq tests are completed */
	.polling_ms = 0,
	.target = ddr_target,
	.get_dev_status = ddr_get_dev_status,
};

static int ddr_devfreq_probe(struct platform_device *pdev)
{
	int i = 0, res;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct devfreq_platform_data *pdata = NULL;
	struct ddr_devfreq_data *data = NULL;
	const char *ddr_clk_name;

	pdata = (struct devfreq_platform_data *)dev->platform_data;
	if (!pdata) {
		dev_err(dev, "No platform data!\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof(struct ddr_devfreq_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	ddr_clk_name = (pdata->clk_name) ? (pdata->clk_name) : "ddr";
	data->ddr_clk = clk_get_sys(NULL, ddr_clk_name);
	if (IS_ERR(data->ddr_clk)) {
		dev_err(dev, "Cannot get clk ptr.\n");
		ret = PTR_ERR(data->ddr_clk);
		goto err_clk_get;
	}

	data->interleave_is_on = pdata->interleave_is_on;
	data->mc_base_pri = pdata->hw_base[0];
	if (data->interleave_is_on)
		data->mc_base_sec = pdata->hw_base[1];

	/* save ddr frequency tbl */
	if (pdata->freq_table != NULL) {
		while (pdata->freq_table[i].frequency != DEVFREQ_TABLE_END) {
			data->ddr_freq_tbl[i] = pdata->freq_table[i].frequency;
			i++;
		}
		data->ddr_freq_tbl_len = i;
	}
	spin_lock_init(&data->lock);

	ddr_devfreq_profile.initial_freq =
		clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;
	/* Initilize the devfreq Qos if platform has registered the Qos req */
	if (pdata->qos_list) {
		ddr_devfreq_profile.qos_type = PM_QOS_DDR_DEVFREQ_MIN;
		ddr_devfreq_profile.qos_list = pdata->qos_list;
	}

	data->pdev_ddr = devfreq_add_device(&pdev->dev,
				       &ddr_devfreq_profile,
				       default_gov, &ddr_ondemand_data);
	if (IS_ERR(data->pdev_ddr)) {
		dev_err(dev, "devfreq add error !\n");
		ret =  (unsigned long)data->pdev_ddr;
		goto err_devfreq_add;
	}

	data->high_upthrd_swp = 800000;
	data->high_upthrd = 30;

	/* init default devfreq min_freq and max_freq */
	data->pdev_ddr->min_freq = data->ddr_freq_tbl[0];
	data->pdev_ddr->max_freq =
		data->ddr_freq_tbl[data->ddr_freq_tbl_len - 1];
	data->last_polled_at = jiffies;

	/* Pass the frequency table to devfreq framework */
	if (pdata->freq_table)
		devfreq_set_freq_table(data->pdev_ddr, pdata->freq_table);

	res = device_create_file(&pdev->dev, &dev_attr_disable_ddr_fc);
	if (res) {
		dev_err(dev,
			"device attr disable_ddr_fc create fail: %d\n", res);
		ret = -ENOENT;
		goto err_file_create0;
	}

	res = device_create_file(&pdev->dev, &dev_attr_ddr_freq);
	if (res) {
		dev_err(dev, "device attr ddr_freq create fail: %d\n", res);
		ret = -ENOENT;
		goto err_file_create1;
	}

	res = device_create_file(&pdev->dev, &dev_attr_ddr_profiling);
	if (res) {
		dev_err(dev, \
			"device attr ddr_profiling create fail: %d\n", res);
		ret = -ENOENT;
		goto err_file_create2;
	}

	res = device_create_file(&pdev->dev, &dev_attr_high_upthrd_swp);
	if (res) {
		dev_err(dev, \
			"device attr high_upthrd_swp create fail: %d\n", res);
		ret = -ENOENT;
		goto err_file_create3;
	}

	res = device_create_file(&pdev->dev, &dev_attr_high_upthrd);
	if (res) {
		dev_err(dev, \
			"device attr high_upthrd create fail: %d\n", res);
		ret = -ENOENT;
		goto err_file_create4;
	}

	cur_data = data;
	platform_set_drvdata(pdev, data);
	dvfs_register_notifier(&notifier_freq_block, DVFS_FREQUENCY_NOTIFIER);

	/*
	 * register the notifier to cpufreq driver,
	 * it is triggered when core freq-chg is done
	 */
	cpufreq_register_notifier(&upthreshold_freq_notifier,
		CPUFREQ_TRANSITION_NOTIFIER);

	ddr_perf_counter_init();
	return 0;

err_file_create4:
	device_remove_file(&pdev->dev, &dev_attr_high_upthrd_swp);
err_file_create3:
	device_remove_file(&pdev->dev, &dev_attr_ddr_profiling);
err_file_create2:
	device_remove_file(&pdev->dev, &dev_attr_ddr_freq);
err_file_create1:
	device_remove_file(&pdev->dev, &dev_attr_disable_ddr_fc);
err_file_create0:
	devfreq_remove_device(data->pdev_ddr);
err_devfreq_add:
err_clk_get:
	kfree(data);
	return ret;
}

static int ddr_devfreq_remove(struct platform_device *pdev)
{
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_disable_ddr_fc);
	device_remove_file(&pdev->dev, &dev_attr_ddr_freq);
	device_remove_file(&pdev->dev, &dev_attr_ddr_profiling);
	device_remove_file(&pdev->dev, &dev_attr_high_upthrd_swp);
	device_remove_file(&pdev->dev, &dev_attr_high_upthrd);
	devfreq_remove_device(data->pdev_ddr);
	kfree(data);
	dvfs_unregister_notifier(&notifier_freq_block, DVFS_FREQUENCY_NOTIFIER);
	return 0;
}

static struct platform_driver ddr_devfreq_driver = {
	.probe = ddr_devfreq_probe,
	.remove = ddr_devfreq_remove,
	.driver = {
		.name = "devfreq-ddr",
		.owner = THIS_MODULE,
	},
};

static void __init ddr_devfreq_exit(void)
{
	platform_driver_unregister(&ddr_devfreq_driver);
}

static int __init ddr_devfreq_init(void)
{
	return platform_driver_register(&ddr_devfreq_driver);
}

fs_initcall(ddr_devfreq_init);
module_exit(ddr_devfreq_exit);

MODULE_DESCRIPTION("mck4 memorybus devfreq driver");
MODULE_LICENSE("GPL");
