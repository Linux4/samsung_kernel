#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/printk.h>

#include <linux/soc/sprd/sprd_hrtimer_debug.h>

static inline int hrtimer_hres_active_cpu(int cpu)
{
	struct hrtimer_cpu_base *cpu_base;

	cpu_base = per_cpu_ptr(&hrtimer_bases, cpu);

	return cpu_base->hres_active;
}

/*
 * get per-cpu hrtimer state
 */
static ssize_t hrtimer_state_read(struct file *file,
	char __user *userbuf, size_t count, loff_t *ppos)
{
	int cpu;
	bool active_state;

	pr_info("Online cpu hrtimer state:\n");
	for_each_online_cpu(cpu) {
		active_state = hrtimer_hres_active_cpu(cpu);
		pr_info("core [%d] : hres mode %d\n", cpu, active_state);
	}

	return 0;
}

static const struct file_operations hrtimer_status_fops = {
	.owner = THIS_MODULE,
	.read = hrtimer_state_read,
};

static int hrtimer_sys_debug_init(void)
{
	static struct dentry *hrtimer_debugfs_root;

	hrtimer_debugfs_root = debugfs_create_dir("time", NULL);

	debugfs_create_file("hres_state", 0400, hrtimer_debugfs_root,
				NULL, &hrtimer_status_fops);

	return 0;
}

late_initcall(hrtimer_sys_debug_init);

MODULE_DESCRIPTION("sprd hrtimer_debug driver");
MODULE_LICENSE("GPL v2");
