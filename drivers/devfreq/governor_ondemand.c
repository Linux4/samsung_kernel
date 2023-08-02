/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/earlysuspend.h>
#include "governor.h"

/* Default constants for DevFreq-Ondemand (DFO) */
#define DFO_UPTHRESHOLD		(80)
#define DFO_DOWNDIFFERENCTIAL	(30)

/*
* TODO: add kernel space requests
*/
extern void devfreq_min_freq_cnt_reset(unsigned int, unsigned int);
extern int devfreq_request_ignore(void);
static void dfs_req_timer_timeout(unsigned long arg);
#define REQ_TIMEOUT_DEF (HZ/20);
static DEFINE_SPINLOCK(dfs_req_lock);
static unsigned int dfs_req_timeout;
static DEFINE_TIMER(dfs_req_timer, dfs_req_timer_timeout, 0, 0);
struct dfs_request_state{
	int req_sum;  /* in KHz */
	int req_timeout;  /* in KHz */
	int req_quirk;  /* in KHz */
	u32 ddr_freq_after_req;  /* in KHz */
};
static struct dfs_request_state user_requests;
static struct devfreq *g_devfreq; /* for requests from kernel */
static int gov_eb = 1;
struct userspace_data {
	int req_bw;
	unsigned long set_freq;
	unsigned long set_count;
	unsigned long upthreshold;
	unsigned long downdifferential;
	unsigned long (*convert_bw_to_freq)(u32 req_bw);
	bool enable; /*sysfs only*/
	bool devfreq_enable;
};

/************ kernel interface *****************/
/*
* get dfs enable flag
* enabled == 0 --> dfs disable
* enabled == 1 --> dfs enable
*/
bool dfs_get_enable(void)
{
	struct userspace_data *user_data;
	bool enabled = false;

	if(g_devfreq && g_devfreq->data){
                user_data = (struct userspace_data *)(g_devfreq->data);
		enabled = user_data->enable;
		pr_debug("%s, enabled = %d, user_data->enable = %d\n", __func__, enabled, user_data->enable);
	}

	return enabled;
}
EXPORT_SYMBOL(dfs_get_enable);
/*
* set ddr frequnecy
* @freq: KHz
* if ddr frequency is set through this function, DVS is disabled
*/
int dfs_set_freq(int freq)
{
	struct userspace_data *user_data;
	int err;

	if(freq < 0){
		err = -1;
		pr_debug("*** %s,freq < 0\n",__func__);
		goto done;
	}

	user_data = (struct userspace_data *)(g_devfreq->data);
	mutex_lock(&g_devfreq->lock);
	if(user_data){
		if(freq > 0){
			user_data->set_count++;
			user_data->devfreq_enable = false;
			if(freq > user_data->set_freq)
				user_data->set_freq = freq;
		}else{
			if(user_data->set_count > 0){
				user_data->set_count--;
				if(user_data->set_count == 0){
					user_data->set_freq = 0;
					user_data->devfreq_enable = true;
				}
			}
		}
		pr_debug("*** %s, set freq:%d KHz, set_count:%lu ***\n", __func__, freq, user_data->set_count );
	}
	else
	{
		pr_debug("*** %s,user_data == 0\n",__func__);
	}
	err = update_devfreq(g_devfreq);
	mutex_unlock(&g_devfreq->lock);	
done:
	return err;
}

/*
*  add a new ddr bandwidth request.
*  @req_bw: KB
*      + addition(add>=0) or - subtraction(add<0)
*/
void dfs_request_bw(int req_bw)
{
	u32 req_freq = 0;
	int add = 1;
	struct userspace_data *user_data;

	if (req_bw < 0) {
		req_bw = -req_bw;
		add = -1;
	}

	if(g_devfreq && g_devfreq->data){
		user_data = (struct userspace_data *)(g_devfreq->data);
		if(user_data->convert_bw_to_freq){
			req_freq = (user_data->convert_bw_to_freq)(req_bw);
		}
	}
	pr_debug("*** %s, pid:%u, %creq_bw:%u, req_freq:%u ***\n",
				__func__, current->pid, add>=0?'+':'-', req_bw, req_freq );
	if(req_freq){
		mutex_lock(&g_devfreq->lock);
		if(add >= 0)
			user_requests.req_sum += req_freq;
		else
			user_requests.req_sum -= req_freq;
		if(user_requests.req_sum < 0)
			user_requests.req_sum = 0;
		update_devfreq(g_devfreq);
		mutex_unlock(&g_devfreq->lock);
	}
}

/*
* set request timer timeout
* @timeout, ms
*/
void dfs_req_set_timeout(unsigned int timeout)
{
	spin_lock(&dfs_req_lock);
	dfs_req_timeout = msecs_to_jiffies(timeout);
	spin_unlock(&dfs_req_lock);
}
EXPORT_SYMBOL(dfs_req_set_timeout);

/*
* get request timer timeout
* @return, ms
*/
unsigned int dfs_req_get_timeout(void)
{
	unsigned int timeout;

	spin_lock(&dfs_req_lock);
	timeout = dfs_req_timeout;
	spin_unlock(&dfs_req_lock);

	timeout = jiffies_to_msecs(timeout);
	return timeout;
}
EXPORT_SYMBOL(dfs_req_get_timeout);

static void dfs_req_timer_timeout(unsigned long arg)
{
	spin_lock(&dfs_req_lock);
	user_requests.req_timeout = 0;
	spin_unlock(&dfs_req_lock);
	return;
}

/*
*  add a new ddr bandwidth request. when time is up, request is cleared automatically
*  @req_bw, KB
*/
void dfs_request_bw_timeout(unsigned int req_bw)
{
	struct userspace_data *user_data;
	unsigned int req_freq;

	req_freq = 0;
	if(req_bw == 0)
		return;

	spin_lock(&dfs_req_lock);
	if( user_requests.req_timeout ){
		spin_unlock(&dfs_req_lock);
		pr_debug("*** %s, ignore, req_timeout:%d ***\n", __func__, user_requests.req_timeout);
		return;
	}
	spin_unlock(&dfs_req_lock);

	if(g_devfreq && g_devfreq->data){
		user_data = (struct userspace_data *)(g_devfreq->data);
		if(user_data->convert_bw_to_freq){
			req_freq = (user_data->convert_bw_to_freq)(req_bw);
			printk("*** %s, req_freq:%d ***\n", __func__, req_freq );
		}
	}
	spin_lock(&dfs_req_lock);
	user_requests.req_timeout = req_freq;
	spin_unlock(&dfs_req_lock);

	if(req_freq)
		mod_timer(&dfs_req_timer, jiffies+dfs_req_timeout);
	else
		del_timer_sync(&dfs_req_timer);

	if(req_freq){
		mutex_lock(&g_devfreq->lock);
		update_devfreq(g_devfreq);
		mutex_unlock(&g_devfreq->lock);
	}
}
EXPORT_SYMBOL(dfs_request_bw_timeout);

/*
*  raise ddr frequency up temporarily
*  @req_bw, KB
*/
void dfs_freq_raise_quirk(unsigned int req_bw)
{
	if(req_bw == 0)
		return;

	spin_lock(&dfs_req_lock);
	if(user_requests.req_quirk || devfreq_request_ignore() ){
		spin_unlock(&dfs_req_lock);
		return;
	}
	user_requests.req_quirk = req_bw;
	spin_unlock(&dfs_req_lock);

	mutex_lock(&g_devfreq->lock);
	devfreq_min_freq_cnt_reset(-1, 1);
	update_devfreq(g_devfreq);
	devfreq_min_freq_cnt_reset(-1, 0);
	user_requests.req_quirk = 0;
	mutex_unlock(&g_devfreq->lock);
}
EXPORT_SYMBOL(dfs_freq_raise_quirk);


/************ early suspend  *****************/

static void devfreq_early_suspend(struct early_suspend *h)
{
#ifdef CONFIG_ARCH_SCX15
	dfs_set_freq(192000);
#else
#ifdef CONFIG_ARCH_SCX35
        dfs_set_freq(200000);
#endif
#endif
	gov_eb = 0;
}

static void devfreq_late_resume(struct early_suspend *h)
{
	dfs_set_freq(0);
}

static struct early_suspend devfreq_early_suspend_desc = {
        .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 100,
        .suspend = devfreq_early_suspend,
        .resume = devfreq_late_resume,
};

static void devfreq_enable_late_resume(struct early_suspend *h)
{
	gov_eb = 1;
}
static struct early_suspend devfreq_enable_desc = {
        .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
        .resume = devfreq_enable_late_resume,
};
/************ userspace interface *****************/

static ssize_t store_upthreshold(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	unsigned long wanted;


	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	sscanf(buf, "%lu", &wanted);
	if(data)
		data->upthreshold = wanted;
	mutex_unlock(&devfreq->lock);
	return count;
}

static ssize_t show_upthreshold(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	if(data){
		err = sprintf(buf, "%lu\n", data->upthreshold);
	}else
		err = sprintf(buf, "%d\n", DFO_UPTHRESHOLD);
	mutex_unlock(&devfreq->lock);
	return err;
}

static ssize_t store_downdifferential(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	unsigned long wanted;


	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	sscanf(buf, "%lu", &wanted);
	if(data)
		data->downdifferential = wanted;
	mutex_unlock(&devfreq->lock);
	return count;
}

static ssize_t show_downdifferential(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	if(data){
		err = sprintf(buf, "%lu\n", data->downdifferential);
	}else{
		err = sprintf(buf, "%d\n", DFO_DOWNDIFFERENCTIAL);
	}
	mutex_unlock(&devfreq->lock);
	return err;
}


static ssize_t store_request(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	int wanted;
	int req_freq;
	int err = 0;

	req_freq = 0;
	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	sscanf(buf, "%d", &wanted);
	if(data){
		data->req_bw += wanted;
		pr_debug("*** %s, request:%d, total request:%d ***\n",
				__func__, wanted, data->req_bw);
		if(data->req_bw < 0)
			data->req_bw = 0;
		if(data->convert_bw_to_freq)
			req_freq = data->convert_bw_to_freq(wanted);
	}
	user_requests.req_sum += req_freq;
	if(user_requests.req_sum < 0)
		user_requests.req_sum = 0;
	err = update_devfreq(devfreq);
	if (err == 0)
		err = count;
	mutex_unlock(&devfreq->lock);
	return err;

}

static ssize_t show_request(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	if(data)
		err = sprintf(buf, "%d KB\n", data->req_bw);
	mutex_unlock(&devfreq->lock);
	return err;
}

static ssize_t store_enable(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	unsigned long wanted;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	sscanf(buf, "%lu", &wanted);
	if(data){
		data->enable = wanted;
	}
	mutex_unlock(&devfreq->lock);
	return count;
}

static ssize_t show_enable(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	if(data)
		err = sprintf(buf, "%d \n", data->enable);
	mutex_unlock(&devfreq->lock);
	return err;
}

static ssize_t store_freq(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	unsigned long wanted;
	int err = 0;

	sscanf(buf, "%lu", &wanted);
	err = dfs_set_freq(wanted);
	if (err == 0)
		err = count;
	return err;
}

static ssize_t show_freq(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct userspace_data *data;
	int err = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	if(data)
		err = sprintf(buf, "%lu KHz, set count:%lu\n", data->set_freq, data->set_count);
	mutex_unlock(&devfreq->lock);
	return err;
}

static DEVICE_ATTR(set_freq, 0644, show_freq, store_freq);
static DEVICE_ATTR(set_enable, 0644, show_enable, store_enable);
static DEVICE_ATTR(set_request, 0644, show_request, store_request);
static DEVICE_ATTR(set_upthreshold, 0644, show_upthreshold, store_upthreshold);
static DEVICE_ATTR(set_downdifferential, 0644, show_downdifferential, store_downdifferential);
static struct attribute *dev_entries[] = {
	&dev_attr_set_freq.attr,
	&dev_attr_set_enable.attr,
	&dev_attr_set_request.attr,
	&dev_attr_set_upthreshold.attr,
	&dev_attr_set_downdifferential.attr,
	NULL,
};
static struct attribute_group dev_attr_group = {
	.name	= "ondemand",
	.attrs	= dev_entries,
};

static int devfreq_ondemand_start(struct devfreq *devfreq)
{
	int err = 0;
	struct userspace_data *data = kzalloc(sizeof(struct userspace_data),
			GFP_KERNEL);

	if (!data) {
		err = -ENOMEM;
		goto out;
	}
	data->req_bw = 0;
	data->set_freq = 0;
	data->upthreshold = DFO_UPTHRESHOLD;
	data->downdifferential = DFO_DOWNDIFFERENCTIAL;
#if defined(CONFIG_ARCH_SCX30G)
	data->enable = false;
#else
	data->enable = true;
#endif
	data->devfreq_enable = true;
	if(devfreq->data){
		data->convert_bw_to_freq = devfreq->data;
		pr_info("*** %s, data->convert_bw_to_freq:%pf ***\n", __func__, data->convert_bw_to_freq);
	}
	devfreq->data = data;
	g_devfreq = devfreq;
	err = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
	register_early_suspend(&devfreq_early_suspend_desc);
	/*
	* disable DFS before DISPC late resume
	*/
	register_early_suspend(&devfreq_enable_desc);

	spin_lock(&dfs_req_lock);
	dfs_req_timeout = REQ_TIMEOUT_DEF;
	spin_unlock(&dfs_req_lock);
out:
	return err;
}

static int devfreq_ondemand_stop(struct devfreq *devfreq)
{
	int err = 0;
	if(devfreq->data){
		kfree(devfreq->data);
	}

	return err;

}
/************ userspace interface *****************/

static int devfreq_ondemand_func(struct devfreq *df,
					unsigned long *freq)
{
	struct devfreq_dev_status stat;
	int err = df->profile->get_dev_status(df->dev.parent, &stat);
	unsigned long long a, b;
	unsigned int dfso_upthreshold = DFO_UPTHRESHOLD;
	unsigned int dfso_downdifferential = DFO_DOWNDIFFERENCTIAL;
	struct userspace_data *data = df->data;
	unsigned long max = (df->max_freq) ? df->max_freq : UINT_MAX;
	unsigned long req_freq;

	if (err)
		return err;

	if(user_requests.req_quirk &&
		stat.current_frequency==df->min_freq){
		/*
		* QUIRK: only set frequency larger than minimum
		*/
		*freq = df->min_freq + 1;
		pr_debug("*** %s, req_quirk, freq:%lu, return ***\n", __func__, *freq);
		return 0;
	}
	/*
	* TODO: add request frequency
	*/
	req_freq = user_requests.req_sum + user_requests.req_timeout;

	if (data) {
		if (data->enable==false || !(data->devfreq_enable) ||
					data->set_freq || !gov_eb){
			if(user_requests.ddr_freq_after_req == 0)
				user_requests.ddr_freq_after_req = max;
			*freq = (data->set_freq?data->set_freq:user_requests.ddr_freq_after_req);
			pr_debug("*** %s, data->enable:%d, data->set_freq:%u, gov_eb:%d ***\n",
				__func__, data->enable, data->set_freq, gov_eb );
			return 0;
		}
		if (data->upthreshold)
			dfso_upthreshold = data->upthreshold;
		if (data->downdifferential)
			dfso_downdifferential = data->downdifferential;
	}else{
		printk("*** %s, data is NULL ***\n", __func__ );
	}

	if (dfso_upthreshold > 100 ||
	    dfso_upthreshold < dfso_downdifferential){
		printk("*** %s, dfso_upthreshold:%u, dfso_downdifferential:%u ***\n",
				__func__, dfso_upthreshold, dfso_downdifferential );
		return -EINVAL;
	}

	/* Assume MAX if it is going to be divided by zero */
	if (stat.total_time == 0) {
		*freq = max;
		user_requests.ddr_freq_after_req = *freq;
		pr_debug("*** %s, stat.total_time == 0, freq:%lu ***\n", __func__, *freq);
		return 0;
	}

	/* Prevent overflow */
	if (stat.busy_time >= (1 << 24) || stat.total_time >= (1 << 24)) {
		stat.busy_time >>= 7;
		stat.total_time >>= 7;
	}

	/* Set MAX if it's busy enough */
	if (stat.busy_time * 100 >
	    stat.total_time * dfso_upthreshold) {
		*freq = max;
		user_requests.ddr_freq_after_req = *freq;
		pr_debug("*** %s, set max freq:%lu ***\n", __func__, *freq);
		return 0;
	}

	/* Set MAX if we do not know the initial frequency */
	if (stat.current_frequency == 0) {
		*freq = max;
		user_requests.ddr_freq_after_req = *freq;
		pr_debug("*** %s, stat.current_frequency == 0, freq:%lu ***\n", __func__, *freq);
		return 0;
	}

	/* Keep the current frequency */
	if (stat.busy_time * 100 >
	    stat.total_time * (dfso_upthreshold - dfso_downdifferential)) {
		*freq = stat.current_frequency + req_freq;
		user_requests.ddr_freq_after_req = *freq;
		pr_debug("*** %s, Keep the current frequency %lu, req_freq:%lu ***\n",
				__func__, stat.current_frequency, req_freq);
		return 0;
	}

	/* Set the desired frequency based on the load */
	a = stat.busy_time;
	a *= stat.current_frequency;
	b = div_u64(a, stat.total_time);
	b *= 100;
	b = div_u64(b, (dfso_upthreshold - dfso_downdifferential / 2));
	*freq = (unsigned long) b + req_freq;
	user_requests.ddr_freq_after_req = *freq;
	pr_debug("*** %s, calculate freq:%lu, req_freq:%lu ***\n",
				__func__, (unsigned long)b, req_freq);

	if (df->min_freq && *freq < df->min_freq)
		*freq = df->min_freq;
	if (df->max_freq && *freq > df->max_freq)
		*freq = df->max_freq;

	return 0;
}

static int devfreq_ondemand_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_ondemand_start(devfreq);
		devfreq_monitor_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
		//devfreq_ondemand_stop(devfreq);
		break;

	case DEVFREQ_GOV_INTERVAL:
		devfreq_interval_update(devfreq, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return 0;
}

const struct devfreq_governor devfreq_ondemand = {
	.name = "ondemand",
	.get_target_freq = devfreq_ondemand_func,
	.event_handler = devfreq_ondemand_handler,
};

static int __init devfreq_ondemand_init(void)
{
	return devfreq_add_governor(&devfreq_ondemand);
}
subsys_initcall(devfreq_ondemand_init);

static void __exit devfreq_ondemand_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_ondemand);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

	return;
}
module_exit(devfreq_ondemand_exit);
MODULE_LICENSE("GPL");
