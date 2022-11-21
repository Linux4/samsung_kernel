/*
 * cpu mode driver
 * Jungwook Kim <jwook1.kim@samsung.com>
 */

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/thermal.h>
#include <asm/topology.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/gpu_cooling.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/miscdevice.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include "../../../kernel/sched/sched.h"
#include <trace/events/sched.h>
#include <trace/events/ems_debug.h>
#include <soc/samsung/freq-qos-tracer.h>

static const char *prefix = "exynos_perf";

static uint cal_id_mif = 0;
static uint cal_id_g3d = 0;
static uint devfreq_mif = 0;

static int is_running = 0;
static int run = 0;
static uint polling_ms = 1000;
static uint bind_cpu = 0;
static int boost = 0;
static int big_thd = 600;
static int mid_thd = 50;
static int mid_thd_h = 600;
static int boost_timer = 5;
static int debug = 0;
static int gpu_util_thd = 80;


static uint cpu_util_avgs[NR_CPUS];

// module -> gki hooking function
static void update_cpu_util_avg(void *data, struct cfs_rq *cfs_rq)
{
	unsigned int util = READ_ONCE(cfs_rq->avg.util_avg);
	util = (util > 1024)? 1024 : util;
	cpu_util_avgs[cpu_of(rq_of(cfs_rq))] = util;
}

static struct kobject *gmc_kobj;

//---------------------------------------
// thread main
#define GMC_T1	1
#define GMC_T2	2
#define GMC_T3	3
#define TYPE_LEN	(3+1)
static int boost_vals[TYPE_LEN];
static int boost_vals_old[TYPE_LEN];

static int gmc_thread(void *data)
{
	int cpu;
	char msg[32];
	char *envp[] = { msg, NULL };
	int ret = 0;
	int big_avg_sum = 0;
	int mid_avg_sum = 0;
	int big_avg, mid_avg;
	int boost_cnt = 0;
	int gpu_util = 0;

	if (is_running) {
		pr_info("[%s] gmc already running!!\n", prefix);
		return 0;
	}

	// start
	is_running = 1;
	pr_info("[%s] gmc start\n", prefix);


	while (is_running) {
		/******************************
		  GPU heavy --> wakeup boost down
		*/
		gpu_util = gpu_dvfs_get_utilization();
		if (gpu_util > gpu_util_thd)
			boost_vals[GMC_T3] = 1;
		else
			boost_vals[GMC_T3] = 0;

		if (boost_vals_old[GMC_T3] != boost_vals[GMC_T3]) {
			snprintf(msg, sizeof(msg), "GMC_TYPE=%d,GMC_BOOST=%d", GMC_T3, boost_vals[GMC_T3]);
			ret = kobject_uevent_env(gmc_kobj->parent, KOBJ_CHANGE, envp);
			if (ret)
				pr_warn("[%s] fail to send uevent to gmc\n", prefix);
			boost_vals_old[GMC_T3] = boost_vals[GMC_T3];
			if (debug)
				pr_info("[%s] gmc gpu_util %d boost %d\n", prefix, gpu_util, boost_vals[GMC_T3]);
		}

		/******************************
		  Big heavy + Mid light --> Mid boost up
		*/
		for_each_possible_cpu(cpu) {
			if (cpu >= 4 && cpu <= 6)
				mid_avg_sum += cpu_util_avgs[cpu];
			else if (cpu == 7)
				big_avg_sum += cpu_util_avgs[cpu];
		}
		if (boost_cnt++ >= boost_timer) {
			mid_avg = mid_avg_sum / boost_timer / 3;
			big_avg = big_avg_sum / boost_timer;
			if (big_avg >= big_thd && (mid_avg >= mid_thd && mid_avg < mid_thd_h))
				boost_vals[GMC_T1] = 1;
			else
				boost_vals[GMC_T1] = 0;

			if (debug)
				pr_info("[%s] gmc big_thd %d mid_thd %d big_avg %d mid_avg %d boost_timer %d boost %d\n",
						prefix, big_thd, mid_thd, big_avg, mid_avg, boost_timer, boost_vals[GMC_T1]);

			if (boost_vals_old[GMC_T1] != boost_vals[GMC_T1]) {
				snprintf(msg, sizeof(msg), "GMC_TYPE=%d,GMC_BOOST=%d", GMC_T1, boost_vals[GMC_T1]);
				ret = kobject_uevent_env(gmc_kobj->parent, KOBJ_CHANGE, envp);
				if (ret)
					pr_warn("[%s] fail to send uevent to gmc\n", prefix);
				boost_vals_old[GMC_T1] = boost_vals[GMC_T1];
			}
			boost_cnt = 0;
			big_avg_sum = 0;
			mid_avg_sum = 0;
		}
		msleep(polling_ms);
	}

	return 0;
}

static struct task_struct *task;
static void gmc_start(void)
{
	if (is_running) {
		pr_err("[%s] already running!!\n", prefix);
		return;
	}
	// run
	task = kthread_create(gmc_thread, NULL, "exynos_gmc_thread%u", 0);
	kthread_bind(task, bind_cpu);
	wake_up_process(task);
	return;
}

static void gmc_stop(void)
{
	is_running = 0;
	pr_info("[%s] gmc done\n", prefix);
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
//----------------------------------------
// DBGFS macro for MISC node
#if 0
#define DBGFS_NODE(name) \
	static int name##_seq_show(struct seq_file *file, void *iter) {	\
		seq_printf(file, "%d\n", name);	\
		return 0;	\
	}	\
	static ssize_t name##_seq_write(struct file *file, const char __user *buffer, size_t count, loff_t *off) {	\
		return count;	\
	}	\
	static int name##_debugfs_open(struct inode *inode, struct file *file) { \
		return single_open(file, name##_seq_show, inode->i_private);	\
	}	\
	static const struct file_operations name##_debugfs_fops = {	\
		.owner		= THIS_MODULE,	\
		.open		= name##_debugfs_open,	\
		.read		= seq_read,	\
		.write		= name##_seq_write,	\
		.llseek		= seq_lseek,	\
		.release	= single_release,	\
	};
DBGFS_NODE(is_game)

// misc
static struct miscdevice is_game_miscdev;

static int register_is_game_misc(void)
{
	int ret;
	is_game_miscdev.minor = MISC_DYNAMIC_MINOR;
	is_game_miscdev.name = "is_game";
	is_game_miscdev.fops = &is_game_debugfs_fops;
	ret = misc_register(&is_game_miscdev);
	return ret;
}
#endif

#define DEF_NODE(name) \
	static ssize_t show_##name(struct kobject *k, struct kobj_attribute *attr, char *buf) { \
		int ret = 0; \
		ret += sprintf(buf + ret, "%d\n", name); \
		return ret; } \
	static ssize_t store_##name(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) { \
		if (sscanf(buf, "%d", &name) != 1) \
			return -EINVAL; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR(name, 0644, show_##name, store_##name);

DEF_NODE(polling_ms)
DEF_NODE(bind_cpu)
DEF_NODE(boost)
DEF_NODE(big_thd)
DEF_NODE(mid_thd)
DEF_NODE(mid_thd_h)
DEF_NODE(boost_timer)
DEF_NODE(debug)
DEF_NODE(gpu_util_thd)


// run
static ssize_t show_run(struct kobject *k, struct kobj_attribute *attr, char *buf) 
{
	int ret = 0;
	ret += sprintf(buf + ret, "%d", run);
	return ret;
}
static ssize_t store_run(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (sscanf(buf, "%d", &run) != 1)
		return -EINVAL;
	if (run)
		gmc_start();
	else
		gmc_stop();
	return count;
}
static struct kobj_attribute run_attr = __ATTR(run, 0644, show_run, store_run);

static struct attribute *gmc_attrs[] = {
	&run_attr.attr,
	&polling_ms_attr.attr,
	&bind_cpu_attr.attr,
	&boost_attr.attr,
	&big_thd_attr.attr,
	&mid_thd_attr.attr,
	&mid_thd_h_attr.attr,
	&boost_timer_attr.attr,
	&debug_attr.attr,
	&gpu_util_thd_attr.attr,
	NULL
};

static struct attribute_group gmc_group = {
	.attrs = gmc_attrs,
};


/*--------------------------------------*/
// MAIN

int exynos_perf_gmc_init(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	int ret;

	ret = register_trace_pelt_cfs_tp(update_cpu_util_avg, NULL);
	WARN_ON(ret);

	of_property_read_u32(dn, "cal-id-mif", &cal_id_mif);
	of_property_read_u32(dn, "cal-id-g3d", &cal_id_g3d);
	of_property_read_u32(dn, "devfreq-mif", &devfreq_mif);

	gmc_kobj = kobject_create_and_add("gmc", &pdev->dev.kobj);

	if (!gmc_kobj) {
		pr_info("[%s] gmc create node failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	ret = sysfs_create_group(gmc_kobj, &gmc_group);
	if (ret) {
		pr_info("[%s] gmc create group failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

//	register_is_game_misc();

	run = 1;
	gmc_start();

	return 0;
}
