/*
 * Watchdog driver for the SC8830
 *
 * Copyright (C) 2013 Spreadtrum
 *
 * based on sa1100_wdt.c and sc8830 arch reset implementation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/timex.h>
#include <linux/syscore_ops.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <mach/watchdog.h>
#include <asm/io.h>
#include <asm/fiq.h>
#include <asm/fiq_glue.h>
#include <asm/cacheflush.h>

#define KERNEL_ONLY_CHIP_DOG 0
#define KERNEL_WATCHDOG_FEEDER 1

static int feed_period = 3; /* (secs) Default is 3 sec */
static int ca7_irq_margin = 9; /* (secs) Default is 9 sec */
static int ca7_margin = 10; /* (secs) Default is 10 sec */
static int ap_margin = 12; /* (secs) Default is 12 sec */
#if KERNEL_ONLY_CHIP_DOG
static int chip_margin = 8; /* (secs) Default is 8 sec */
#else
static int chip_margin = 15; /* (secs) Default is 15 sec */
#endif
static unsigned long wdt_enabled;

#define wdt_feed_all() FEED_ALL_WDG(chip_margin, ap_margin, ca7_margin, ca7_irq_margin)

extern int in_calibration(void);
#ifndef CONFIG_SPRD_WATCHDOG_SYS_FIQ
static irqreturn_t ca7_wdg_isr(int irq, void *dev_id)
{
	pr_debug("%s\n", __func__);
	pr_info("watchdog timeout interrupt happen\n");
	sci_glb_set(AP_WDG_INT_CLR, WDG_INT_CLEAR_BIT);
	handle_sysrq('m');
	handle_sysrq('w');
	pr_info("Current PID %d is %s\n", task_pid_nr(current), current->comm);
	panic("watchdog timeout interrupt happen\n");
	return IRQ_HANDLED;
}
#else
static void ca7_wdg_fiq(struct fiq_glue_handler *h, void *regs, void *svc_sp)
{
	flush_cache_all();
	outer_disable(); /* l2x0_disable */
	oops_in_progress = 1;
	mdelay(50); /* wait for other application processor finish printk */
	arch_trigger_all_cpu_backtrace();
	panic("Hardware Watchdog interrupt barks on <cpu%d>\n", smp_processor_id());
	__raw_writel(WDG_INT_CLEAR_BIT, CA7_WDG_INT_CLR);
}

static struct fiq_glue_handler ca7_wdg_fiq_glue_handler = {
	.fiq = ca7_wdg_fiq,
};
#endif

#define sprd_wdt_shutdown NULL

static void sprd_wdt_resume(void)
{
	pr_debug("%s\n", __FUNCTION__);
	if (!wdt_enabled)
		return;
	wdt_feed_all();
	ENABLE_ALL_WDG();
	return;
}

static int sprd_wdt_suspend(void)
{
	pr_debug("%s\n", __FUNCTION__);
	if (!wdt_enabled)
		return 0;
	DISABLE_ALL_WDG();
	return 0;
}

static struct syscore_ops sprd_wdt_syscore_ops = {
	.shutdown   = sprd_wdt_shutdown,
	.suspend    = sprd_wdt_suspend,
	.resume     = sprd_wdt_resume,
};

#if KERNEL_WATCHDOG_FEEDER
#define check_first_enter() \
do { \
	static bool first = 1; \
	if (first == 0) \
		return 0; \
	first = 0; \
} while (0)
#else
#define check_first_enter()
#endif

static inline int wdt_init(void)
{
	int ret;
	if (in_calibration()) {
		pr_info("calibration mode, quit...\n");
		return -1;
	}

	check_first_enter();

	/* 0xf0 coressponding to wachdog reboot mode of bootloader */
	sci_adi_raw_write(ANA_RST_STATUS, 0xf0);
#ifndef CONFIG_SPRD_WATCHDOG_SYS_FIQ
	ret = request_irq(IRQ_CA7WDG_INT, ca7_wdg_isr, IRQF_NO_SUSPEND, "sprd_wdg", NULL);
	if (ret) {
		pr_info("sprd wdg isr register failed\n");
		BUG();
	}
#else
	ret = fiq_glue_register_handler(&ca7_wdg_fiq_glue_handler);
	if (ret == 0)
		enable_fiq(IRQ_CA7WDG_INT);
	else
		pr_err("<%s> fiq_glue_register_handler failed %d!\n", __func__, ret);
#endif

	register_syscore_ops(&sprd_wdt_syscore_ops);
	return ret;
}

static inline int wdt_exit(void)
{
	if (in_calibration()) {
		pr_info("calibration mode, quit...\n");
		return 0;
	}
	check_first_enter();
	free_irq(IRQ_CA7WDG_INT, NULL);
	unregister_syscore_ops(&sprd_wdt_syscore_ops);
	return 0;
}

#if KERNEL_WATCHDOG_FEEDER
#define KERNEL_WATCHDOG_FEEDER_BIT 0
static unsigned long enabled;
static struct task_struct *feed_task;
/*
 * This is a hw watchdog starts after init, and fed by a thread started
 * by the watchdog driver itself. It can also be fed from userland.
 * The watchdog will be stopped in system suspend, and restarted in
 * system resume.
 */
static int watchdog_feeder(void *data)
{
	do {
		if (kthread_should_stop())
			break;
		pr_debug("%s, ca7_margin=%d, ap_margain=%d, chip_margin=%d, feed_period=%d, wdt_enabled=%ld\n",
			__FUNCTION__, ca7_margin, ap_margin, chip_margin, feed_period, wdt_enabled);
		if (wdt_enabled & (1 << KERNEL_WATCHDOG_FEEDER_BIT))
			wdt_feed_all();
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(feed_period * HZ);
	} while(1);

	pr_info(KERN_ERR "watchdog_feeder exit\n");
	return 0;
}

static void wdt_start(void)
{
	if (wdt_enabled) {
		pr_info("watchdog enable\n");
		watchdog_start(chip_margin, ap_margin, ca7_margin, ca7_irq_margin);
	} else {
		pr_info("watchdog disable\n");
		DISABLE_ALL_WDG();
	}
}

static int __init sci_wdt_kfeeder_init(void)
{

	if (wdt_init())
		return 0;

#if 0
	feed_task = kthread_create(watchdog_feeder, NULL, "watchdog_feeder");
#else
	do {
		int cpu = 0;
		feed_task = kthread_create_on_node(watchdog_feeder, NULL, cpu_to_node(cpu), "watchdog_feeder/%d", cpu);
		kthread_bind(feed_task, cpu);
	} while (0);
#endif
	if (feed_task == 0) {
		pr_err("Can't crate watchdog_feeder thread!\n");
	} else {
		#if 1
		set_bit(KERNEL_WATCHDOG_FEEDER_BIT, &wdt_enabled);
		enabled = 1;
		#endif
		wdt_start();
		wake_up_process(feed_task);
	}

	pr_info("SC8830 Watchdog: chip margin %d sec\n", chip_margin);
	pr_info("SC8830 Watchdog: ap margin %d sec\n", ap_margin);
	pr_info("SC8830 Watchdog: ca7 margin %d sec\n", ca7_margin);
	pr_info("SC8830 Watchdog: ca7 irq margin %d sec\n", ca7_irq_margin);

	return 0;
}

static void __exit sci_wdt_kfeeder_exit(void)
{
	wdt_exit();
	kthread_stop(feed_task);
}

module_init(sci_wdt_kfeeder_init);
module_exit(sci_wdt_kfeeder_exit);

int param_set_enabled(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_ulong(val, kp);
	if (ret < 0)
		return ret;
	if (enabled)
		set_bit(KERNEL_WATCHDOG_FEEDER_BIT, &wdt_enabled);
	else
		clear_bit(KERNEL_WATCHDOG_FEEDER_BIT, &wdt_enabled);
	wdt_start();
	wake_up_process(feed_task);
	return ret;
}

int param_set_feed_period(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret < 0)
		return ret;
	wake_up_process(feed_task);
	return ret;
}

module_param_call(enabled, param_set_enabled, param_get_ulong,
					&enabled, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(enabled, "Enable kernel watchdog task to feed dog");
module_param_call(feed_period, param_set_feed_period, param_get_int,
					&feed_period, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(feed_period, "Watchdog feed period in seconds (default 3s)");
#endif

#define SPRD_WATCHDOG_MINOR (WATCHDOG_MINOR - 1)
static int boot_status = 0;

static int sci_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(1, &wdt_enabled))
		return -EBUSY;

	pr_debug("%s, ca7_margin=%d, ca7_irq_margin=%d, ap_margain=%d, chip_margin=%d, feed_period=%d\n", __FUNCTION__,
			ca7_margin,	ca7_irq_margin, ap_margin, chip_margin, feed_period);

	watchdog_start(chip_margin, ap_margin, ca7_margin, ca7_irq_margin);

	return nonseekable_open(inode, file);
}

static int sci_release(struct inode *inode, struct file *file)
{
	clear_bit(1, &wdt_enabled);
	DISABLE_ALL_WDG();
	return 0;
}

static ssize_t sci_write(struct file *file, const char __user *data, size_t len,
		loff_t *ppos)
{
	if (len)
		wdt_feed_all();
	return len;
}

static const struct watchdog_info ident = {
	.options = WDIOF_CARDRESET | WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.identity = "SC8830 Watchdog",
	.firmware_version = 1,
};

static long sci_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int time;
	void __user *argp = (void __user *) arg;
	int __user *p = argp;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		ret = copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;
		break;

	case WDIOC_GETSTATUS:
		ret = put_user(0, p);
		break;

	case WDIOC_GETBOOTSTATUS:
		ret = put_user(boot_status, p);
		break;

	case WDIOC_KEEPALIVE:
		wdt_feed_all();
		ret = 0;
		break;

	case WDIOC_SETTIMEOUT:
		ret = get_user(time, p);
		if (ret)
			break;

		if (time <= 0 || (WDG_CLK * (long long) time >= 0xffffffff)) {
			ret = -EINVAL;
			break;
		}

		ca7_irq_margin = time;
		wdt_feed_all();
		/*fall through*/
	case WDIOC_GETTIMEOUT:
		ret = put_user(ca7_irq_margin, p);
		break;
	}
	return ret;
}

static const struct file_operations sci_fops =
{
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = sci_write,
	.unlocked_ioctl = sci_ioctl,
	.open = sci_open,
	.release = sci_release,
};

static struct miscdevice sci_miscdev =
{
	.minor = SPRD_WATCHDOG_MINOR,
	.name = "sprd-watchdog",
	.fops = &sci_fops,
};

static int __init sci_wdt_init(void)
{
	int ret;

	if (wdt_init())
		return 0;

	boot_status = 0;

	ret = misc_register(&sci_miscdev);
	if (ret != 0)
		goto _out;

	pr_info("SC8830 Watchdog: userspace watchdog feeder\n");

_out:
	return ret;
}

static void __exit sci_wdt_exit(void)
{
	wdt_exit();
	misc_deregister(&sci_miscdev);
}

module_init(sci_wdt_init);
module_exit(sci_wdt_exit);

MODULE_ALIAS_MISCDEV(SPRD_WATCHDOG_MINOR);

module_param(chip_margin, int, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(chip_margin, "Chip Watchdog margin in seconds (default 15s)");
module_param(ca7_margin, int, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(ca7_margin, "CA7 Watchdog margin in seconds (default 10s)");
module_param(ca7_irq_margin, int, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(ca7_irq_margin, "CA7 Watchdog irq margin in seconds (default 9s)");
module_param(ap_margin, int, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(ap_margin, "AP Watchdog margin in seconds (default 12s)");

MODULE_DESCRIPTION("SPRD Watchdog");
MODULE_LICENSE("GPL");
