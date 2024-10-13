/*
 * Copyright (C) 2012-2016 Samsung Electronics, Inc.
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

#include <linux/atomic.h>
#include <linux/buffer_head.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pid.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <asm/segment.h>
#include <asm/uaccess.h>

#include "sysdep.h"
#include "tzdev.h"
#include "tz_boost.h"
#include "tz_cdev.h"
#include "tz_cma.h"
#include "tz_iw_boot_log.h"
#include "tz_iwlog.h"
#include "tzlog.h"
#include "tz_mem.h"
#include "tz_panic_dump.h"
#include "tz_platform.h"
#include "tzprofiler.h"

MODULE_AUTHOR("Jaemin Ryu <jm77.ryu@samsung.com>");
MODULE_AUTHOR("Vasily Leonenko <v.leonenko@samsung.com>");
MODULE_AUTHOR("Alex Matveev <alex.matveev@samsung.com>");
MODULE_DESCRIPTION("TZDEV driver");
MODULE_LICENSE("GPL");

int tzdev_verbosity = 0;

module_param(tzdev_verbosity, int, 0644);
MODULE_PARM_DESC(tzdev_verbosity, "0: normal, 1: verbose, 2: debug");

enum tzdev_swd_state {
	TZDEV_SWD_DOWN,
	TZDEV_SWD_UP,
	TZDEV_SWD_DEAD
};

static int tzdev_fd_open;
static DEFINE_MUTEX(tzdev_fd_mutex);

static atomic_t tzdev_swd_state = ATOMIC_INIT(TZDEV_SWD_DOWN);

DECLARE_COMPLETION(tzdev_iwi_event_done);

static struct tzio_sysconf tz_sysconf;

static DEFINE_SPINLOCK(tzdev_rsp_lock);
static unsigned int tzdev_rsp_masks_present, tzdev_rsp_event_mask;
static struct tz_iwd_cpu_mask tzdev_rsp_cpu_mask;

#define TZDEV_RSP_UFIFO_DEPTH		1024
#define TZDEV_RSP_KFIFO_DEPTH		16

static DEFINE_KFIFO(tzdev_rsp_ufifo, unsigned int, TZDEV_RSP_UFIFO_DEPTH);
static DEFINE_KFIFO(tzdev_rsp_kfifo, unsigned int, TZDEV_RSP_KFIFO_DEPTH);

#ifdef CONFIG_TZDEV_SK_MULTICORE

static DEFINE_PER_CPU(struct tzio_aux_channel *, tzdev_aux_channel);

#define aux_channel_get(ch)		get_cpu_var(ch)
#define aux_channel_put(ch)		put_cpu_var(ch)
#define aux_channel_init(ch, cpu)	per_cpu(ch, cpu)

#else /* CONFIG_TZDEV_SK_MULTICORE */

static DEFINE_MUTEX(tzdev_aux_channel_lock);
static struct tzio_aux_channel *tzdev_aux_channel[NR_CPUS];

static struct tzio_aux_channel *aux_channel_get(struct tzio_aux_channel *ch[])
{
	mutex_lock(&tzdev_aux_channel_lock);
	BUG_ON(smp_processor_id() != 0);
	return ch[0];
}

#define aux_channel_put(ch)		mutex_unlock(&tzdev_aux_channel_lock)
#define aux_channel_init(ch, cpu)	ch[cpu]

#endif /* CONFIG_TZDEV_SK_MULTICORE */

void tzdev_rsp_convert(struct tzio_smc_data *s, struct tzdev_smc_data *d)
{
	int i;

	for (i = 0; i < NR_SMC_ARGS; i++)
		s->args[i] = d->args[i];
}

int __tzdev_smc_cmd(struct tzdev_smc_data *data,
		unsigned int swd_ctx_present)
{
	int ret;

#ifdef TZIO_DEBUG
	printk(KERN_ERR "tzdev_smc_cmd: p0=0x%lx, p1=0x%lx, p2=0x%lx, p3=0x%lx\n",
			data->a0, data->a1, data->a2, data->a3);
#endif
	tzprofiler_enter_sw();
	tz_iwlog_schedule_delayed_work();

	ret = tzdev_platform_smc_call(data);

	tz_iwlog_cancel_delayed_work();

	if (swd_ctx_present) {
		tzdev_check_cpu_mask(&data->cpu_mask);
		tz_iwnotify_call_chains(data->iwnotify_oem_flags);
		data->iwnotify_oem_flags = 0;
	}

	tzprofiler_wait_for_bufs();
	tzprofiler_exit_sw();
	tz_iwlog_read_channels();

	return ret;
}

/*  TZDEV interface functions */
unsigned int tzdev_is_up(void)
{
	return atomic_read(&tzdev_swd_state) == TZDEV_SWD_UP;
}

struct tzio_aux_channel *tzdev_get_aux_channel(void)
{
	return aux_channel_get(tzdev_aux_channel);
}

void tzdev_put_aux_channel(void)
{
	aux_channel_put(tzdev_aux_channel);
}

static int tzdev_alloc_aux_channel(int cpu)
{
	struct tzio_aux_channel *channel;
	struct page *page;
	int ret;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	channel = page_address(page);

	tzdev_print(0, "AUX Channel[%d] = 0x%p\n", cpu, channel);

	ret = tzdev_smc_connect(TZDEV_CONNECT_AUX, page_to_pfn(page), 1);
	if (ret) {
		__free_page(page);
		return ret;
	}

	aux_channel_init(tzdev_aux_channel, cpu) = channel;

	return 0;
}

static int tzdev_alloc_aux_channels(void)
{
	int ret;
	unsigned int i;

	for (i = 0; i < NR_SW_CPU_IDS; ++i) {
		ret = tzdev_alloc_aux_channel(i);
		if (ret)
			return ret;
	}

	return 0;
}

static void tzdev_get_nwd_sysconf(struct tzio_sysconf *s)
{
	s->flags = 0;
#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	s->flags |= SYSCONF_CRYPTO_CLOCK_MANAGEMENT;
#endif
}

static int tzdev_get_sysconf(struct tzio_sysconf *s)
{
	struct tzio_sysconf nwd_sysconf;
	struct tzio_aux_channel *ch;
	int ret = 0;

	/* Get sysconf from SWd */
	ch = aux_channel_get(tzdev_aux_channel);
	ret = tzdev_smc_get_swd_sysconf();
	aux_channel_put(tzdev_aux_channel);

	if (ret) {
		tzdev_print(0, "tzdev_smc_get_swd_sysconf() failed with %d\n", ret);
		return ret;
	}
	memcpy(s, ch->buffer, sizeof(struct tzio_sysconf));

	tzdev_get_nwd_sysconf(&nwd_sysconf);

	/* Merge NWd and SWd sysconf structures */
	s->flags |= nwd_sysconf.flags;

	return ret;
}

static irqreturn_t tzdev_event_handler(int irq, void *ptr)
{
	complete(&tzdev_iwi_event_done);

	return IRQ_HANDLED;
}

#if CONFIG_TZDEV_IWI_PANIC != 0
static void dump_kernel_panic_bh(struct work_struct *work)
{
	atomic_set(&tzdev_swd_state, TZDEV_SWD_DEAD);
	tz_iw_boot_log_read();
	tz_iwlog_read_channels();
}

static DECLARE_WORK(dump_kernel_panic, dump_kernel_panic_bh);

static irqreturn_t tzdev_panic_handler(int irq, void *ptr)
{
	schedule_work(&dump_kernel_panic);
	return IRQ_HANDLED;
}
#endif

void tzdev_resolve_iwis_id(unsigned int *iwi_event, unsigned int *iwi_panic)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "samsung,blowfish");
	if (!node) {
		*iwi_event = CONFIG_TZDEV_IWI_EVENT;
#if CONFIG_TZDEV_IWI_PANIC != 0
		*iwi_panic = CONFIG_TZDEV_IWI_PANIC;
#endif
		tzdev_print(0, "of_find_compatible_node() failed\n");
		return;
	}
	
	*iwi_event = irq_of_parse_and_map(node, 0);
	if (!*iwi_event) {
		*iwi_event = CONFIG_TZDEV_IWI_EVENT;
		tzdev_print(0, "Use IWI event IRQ number from config %d\n",
			CONFIG_TZDEV_IWI_EVENT);
	}

#if CONFIG_TZDEV_IWI_PANIC != 0
	*iwi_panic = irq_of_parse_and_map(node, 1);
	if (!*iwi_panic) {
		*iwi_panic = CONFIG_TZDEV_IWI_PANIC;
		tzdev_print(0, "Use IWI panic IRQ number from config %d\n",
			CONFIG_TZDEV_IWI_PANIC);
	}
#endif
	of_node_put(node);
}

static void tzdev_register_iwis(void)
{
	int ret;
	int iwi_event;
	int iwi_panic;

	tzdev_resolve_iwis_id(&iwi_event, &iwi_panic);

	ret = request_irq(iwi_event, tzdev_event_handler, 0, "tzdev_iwi_event", NULL);
	if (ret)
		tzdev_print(0, "TZDEV_IWI_EVENT registration failed: %d\n", ret);

#if CONFIG_TZDEV_IWI_PANIC != 0
	ret = request_irq(iwi_panic, tzdev_panic_handler, 0, "tzdev_iwi_panic", NULL);
	if (ret)
		tzdev_print(0, "TZDEV_IWI_PANIC registration failed: %d\n", ret);
#endif
}

static int tzdev_run_init_sequence(void)
{
	int ret = 0;

	if (atomic_read(&tzdev_swd_state) == TZDEV_SWD_DOWN) {
		/* check kernel and driver version compatibility with Blowfish */
		ret = tzdev_smc_check_version();
		if (ret == -ENOSYS) {
			tzdev_print(0, "Minor version of TZDev driver is newer than version of"
				"Blowfish secure kernel.\nNot critical, continue...\n");
			ret = 0;
		} else if (ret) {
			tzdev_print(0, "The version of the Linux kernel or "
				"TZDev driver is not compatible with Blowfish "
				"secure kernel\n");
			goto out;
		}

		if (tzdev_cma_mem_register()) {
			tzdev_print(0, "tzdev_cma_mem_register() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		if (tzdev_alloc_aux_channels()) {
			tzdev_print(0, "tzdev_alloc_aux_channels() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		if (tzdev_mem_init()) {
			tzdev_print(0, "tzdev_mem_init() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		if (tz_iwlog_initialize()) {
			tzdev_print(0, "tz_iwlog_initialize() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		if (tzprofiler_initialize()) {
			tzdev_print(0, "tzprofiler_initialize() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		if (tz_panic_dump_alloc_buffer()) {
			tzdev_print(0, "tz_panic_dump_alloc_buffer() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		if (tzdev_get_sysconf(&tz_sysconf)) {
			tzdev_print(0, "tzdev_get_sysconf() failed\n");
			ret = -ESHUTDOWN;
			goto out;
		}

		tzdev_register_iwis();
		tz_iwnotify_initialize();

		if (atomic_cmpxchg(&tzdev_swd_state, TZDEV_SWD_DOWN, TZDEV_SWD_UP)) {
			ret = -ESHUTDOWN;
			goto out;
		}
	}
out:
	if (ret == -ESHUTDOWN) {
		atomic_set(&tzdev_swd_state, TZDEV_SWD_DEAD);
		tz_iw_boot_log_read();
		tz_iwlog_read_channels();
	}
	return ret;
}

static int tzdev_get_access_info(struct tzio_access_info *s)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct file *exe_file;

	rcu_read_lock();

	task = find_task_by_vpid(s->pid);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}

	get_task_struct(task);
	rcu_read_unlock();

	s->gid = task->tgid;

	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm)
		return -ESRCH;

	exe_file = get_mm_exe_file(mm);
	mmput(mm);
	if (!exe_file)
		return -ESRCH;

	strncpy(s->ca_name, exe_file->f_path.dentry->d_name.name, CA_ID_LEN);
	fput(exe_file);

	return 0;
}

static unsigned int _tzdev_rsp_save(unsigned int pipe,
		unsigned int event_mask,
		struct tz_iwd_cpu_mask cpu_mask,
		unsigned int is_pipe_user)
{
	unsigned int need_notify_user = 0, num_written;
	uint16_t cnt_old, cnt_new;

	tzdev_rsp_event_mask |= event_mask;
	if (tzdev_rsp_event_mask) {
		need_notify_user = 1;
		tzdev_rsp_masks_present = 1;
	}

	cnt_old = tzdev_rsp_cpu_mask.counter;
	cnt_new = cpu_mask.counter;
	if (cnt_new != cnt_old
			&& (uint16_t)(cnt_new - cnt_old) < U16_MAX / 2) {
		tzdev_rsp_cpu_mask = cpu_mask;
		need_notify_user = 1;
		tzdev_rsp_masks_present = 1;
	}

	if (pipe) {
		if (is_pipe_user && kfifo_is_empty(&tzdev_rsp_ufifo))
			need_notify_user = 1;

		num_written = is_pipe_user ?
			sysdep_kfifo_put(&tzdev_rsp_ufifo, pipe) :
			sysdep_kfifo_put(&tzdev_rsp_kfifo, pipe);
		if (!num_written) {
			tzdev_print(0, "Putting response to %s queue failed\n",
					is_pipe_user ? "user" : "kernel");
			/* User FIFO overflow is handled by dropping the message. */
			BUG_ON(!is_pipe_user);
		}
	}

	return need_notify_user;
}

static struct tzdev_smc_data _tzdev_rsp_load(unsigned int is_user,
		unsigned int *need_notify_user, unsigned int *have_result)
{
	struct tzdev_smc_data d;
	unsigned int num_read, pipe = 0;

	memset(&d, 0, sizeof(d));

	num_read = is_user ?
		kfifo_get(&tzdev_rsp_ufifo, &pipe) :
		kfifo_get(&tzdev_rsp_kfifo, &pipe);

	if (num_read
			&& is_user
			&& !kfifo_is_empty(&tzdev_rsp_ufifo)) {
		*need_notify_user = 1;
		if (have_result)
			*have_result = 1;
	}

	d.args[0] = pipe;

	if (is_user) {
		if (tzdev_rsp_masks_present) {
			d.event_mask |= tzdev_rsp_event_mask;
			tzdev_rsp_event_mask = 0;
			tzdev_rsp_masks_present = 0;
			if (have_result)
				*have_result = 1;
		}
		/* Always return cpu mask to user */
		d.cpu_mask = tzdev_rsp_cpu_mask;
	}

	return d;
}

static struct tzdev_smc_data tzdev_rsp_handle(struct tzdev_smc_data d, unsigned int is_user)
{
	unsigned int pipe = 0, is_pipe_user = 0, need_notify_user;

	if (d.pipe && tzdev_is_mem_exist(d.pipe, &is_pipe_user)) {
		pipe = d.args[0] & TZDEV_PIPE_TARGET_DEAD_MASK;
		d.args[0] &= ~TZDEV_TARGET_DEAD_MASK;
	}

	spin_lock(&tzdev_rsp_lock);
	need_notify_user = _tzdev_rsp_save(pipe, d.event_mask,
			d.cpu_mask, is_pipe_user);
	if (is_user)
		need_notify_user = 0;
	d = _tzdev_rsp_load(is_user, &need_notify_user, NULL);
	spin_unlock(&tzdev_rsp_lock);

	if (need_notify_user) {
		if (is_user)
			/* We don't really care what CPU is marked for a
			 * wakeup, but try to select current one in the
			 * expectation that userspace has set affinity. "Raw"
			 * version is used to silence "using in preemptible
			 * code" warning. */
			d.cpu_mask.mask |= 1 << raw_smp_processor_id();
		else
			complete(&tzdev_iwi_event_done);
	}

	return d;
}

static struct tzdev_smc_data tzdev_rsp_check(unsigned int is_user, unsigned int *have_result)
{
	struct tzdev_smc_data d;
	unsigned int need_notify_user = 0;

	spin_lock(&tzdev_rsp_lock);
	d = _tzdev_rsp_load(is_user, &need_notify_user, have_result);
	spin_unlock(&tzdev_rsp_lock);

	if (need_notify_user) {
		if (is_user)
			/* We don't really care what CPU is marked for a
			 * wakeup, but try to select current one in the
			 * expectation that userspace has set affinity. "Raw"
			 * version is used to silence "using in preemptible
			 * code" warning. */
			d.cpu_mask.mask |= 1 << raw_smp_processor_id();
		else
			complete(&tzdev_iwi_event_done);
	}

	return d;
}

static struct tzdev_smc_data _tzdev_send_command(unsigned int tid, unsigned int shm_id, unsigned int is_user)
{
	struct tzdev_smc_data d;

	d = tzdev_smc_command(tid, shm_id);

	return tzdev_rsp_handle(d, is_user);
}

static struct tzdev_smc_data tzdev_send_command_user(unsigned int tid, unsigned int shm_id)
{
	return _tzdev_send_command(tid, shm_id, 1);
}

struct tzdev_smc_data tzdev_send_command(unsigned int tid, unsigned int shm_id)
{
	return _tzdev_send_command(tid, shm_id, 0);
}

static struct tzdev_smc_data _tzdev_get_event(unsigned int is_user)
{
	unsigned int have_result = 0;
	struct tzdev_smc_data d;

	(void)tzdev_rsp_check;
	(void)have_result;

#if 0
	/* TODO: There are two different cases when clients call "get event":
	 * 1. to retrieve an answer (or poll for one), or
	 * 2. to enter SWd and let it do some work.
	 *
	 * Tzdev doesn't know the intention of "get event" call, and current
	 * implementation always does the safe thing - enters SWd and returns
	 * with an answer if there's any.
	 *
	 * Optimization below returns an answer, if present, without entering
	 * SWd. This can lead to lost SWd entry requests and TAs "hanging",
	 * therefore the optimization is disabled for now. To fix this, tzdev
	 * needs to track "wait event" notifications and remember that SWd
	 * entry is required. */
	d = tzdev_rsp_check(is_user, &have_result);
	if (have_result)
		return d;
#endif

	d = tzdev_smc_get_event();

	return tzdev_rsp_handle(d, is_user);
}

static struct tzdev_smc_data tzdev_get_event_user(void)
{
	return _tzdev_get_event(1);
}

struct tzdev_smc_data tzdev_get_event(void)
{
	return _tzdev_get_event(0);
}

static int tzdev_restart_swd_userspace(void)
{
#define TZDEV_SWD_USERSPACE_RESTART_TIMEOUT 10

	int ret;
	struct timespec ts, te;

	/* FIXME Ugly, but works.
	 * Will be corrected in the course of SMC handler work */
	getrawmonotonic(&ts);
	do {
		ret = tzdev_smc_swd_restart_userspace();
		getrawmonotonic(&te);
		if (te.tv_sec - ts.tv_sec > TZDEV_SWD_USERSPACE_RESTART_TIMEOUT) {
			atomic_set(&tzdev_swd_state, TZDEV_SWD_DEAD);
			tzdev_print(0, "Timeout on starting SWd userspace.\n");
			return -ETIME;
		}
	} while (ret != 1);

	/* We need to release all iwshmems here to avoid
	 * situation when we have alive SWd applications that
	 * are able to write to iwshmems and released iwshmems on
	 * the Linux kernel side, otherwise
	 * we can have random memory corruptions */
	tzdev_mem_release_all_user();

	return 0;
}

static int tzdev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	if (atomic_read(&tzdev_swd_state) == TZDEV_SWD_DEAD)
		return -ESHUTDOWN;

	mutex_lock(&tzdev_fd_mutex);

	if (tzdev_fd_open != 0) {
		ret = -EBUSY;
		goto out;
	}

	tzdev_platform_open();

#if !defined(CONFIG_TZDEV_EARLY_SWD_INIT)
	ret = tzdev_run_init_sequence();
	if (ret)
		goto out;

	ret = tzdev_restart_swd_userspace();
	if (ret)
		goto out;
#endif /* CONFIG_TZDEV_EARLY_SWD_INIT */

	tzdev_fd_open++;

out:
	mutex_unlock(&tzdev_fd_mutex);
	return ret;
}

static int tzdev_release(struct inode *inode, struct file *filp)
{
#if defined(CONFIG_TZDEV_NWD_PANIC_ON_CLOSE)
	panic("tzdev invalid close\n");
#endif /* CONFIG_TZDEV_NWD_PANIC_ON_CLOSE */

	mutex_lock(&tzdev_fd_mutex);
	tz_boost_disable();

	tzdev_fd_open--;
	BUG_ON(tzdev_fd_open);

	mutex_unlock(&tzdev_fd_mutex);

	tzdev_platform_close();

#if defined(CONFIG_TZDEV_EARLY_SWD_INIT)
	tzdev_restart_swd_userspace();
#endif /* CONFIG_TZDEV_EARLY_SWD_INIT */

	return 0;
}

static long tzdev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case TZIO_SMC: {
		struct tzio_smc_data __user *argp = (struct tzio_smc_data __user *)arg;
		struct tzio_smc_data s;
		struct tzdev_smc_data d;

		if (copy_from_user(&s, argp, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		d = tzdev_send_command_user(s.args[0], s.args[1]);
		tzdev_rsp_convert(&s, &d);

		if (copy_to_user(argp, &s, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		return 0;
	}
	case TZIO_GET_ACCESS_INFO: {
		int ret = 0;
		struct tzio_access_info __user *argp = (struct tzio_access_info __user *)arg;
		struct tzio_access_info s;

		if (copy_from_user(&s, argp, sizeof(struct tzio_access_info)))
			return -EFAULT;

		ret = tzdev_get_access_info(&s);
		if (ret)
			return ret;
		if (copy_to_user(argp, &s, sizeof(struct tzio_access_info)))
			return -EFAULT;

		return 0;
	}
	case TZIO_GET_SYSCONF: {
		struct tzio_sysconf __user *argp = (struct tzio_sysconf __user *)arg;

		if (copy_to_user(argp, &tz_sysconf, sizeof(struct tzio_sysconf)))
			return -EFAULT;

		return 0;
	}
	case TZIO_MEM_REGISTER: {
		int ret = 0;
		struct tzio_mem_register __user *argp = (struct tzio_mem_register __user *)arg;
		struct tzio_mem_register s;

		if (copy_from_user(&s, argp, sizeof(struct tzio_mem_register)))
			return -EFAULT;

		ret = tzdev_mem_register_user(&s);
		if (ret)
			return ret;

		if (copy_to_user(argp, &s, sizeof(struct tzio_mem_register)))
			return -EFAULT;

		return 0;
	}
	case TZIO_MEM_RELEASE: {
		return tzdev_mem_release_user(arg);
	}
	case TZIO_WAIT_EVT: {
		return wait_for_completion_interruptible(&tzdev_iwi_event_done);
	}
	case TZIO_GET_PIPE: {
		struct tzio_smc_data __user *argp = (struct tzio_smc_data __user *)arg;
		struct tzio_smc_data s;
		struct tzdev_smc_data d;

		d = tzdev_get_event_user();
		tzdev_rsp_convert(&s, &d);

		if (copy_to_user(argp, &s, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		return 0;
	}
	case TZIO_UPDATE_REE_TIME: {
		struct tzio_smc_data __user *argp = (struct tzio_smc_data __user *)arg;
		struct tzio_smc_data s;
		struct tzdev_smc_data d;
		struct timespec ts;

		if (copy_from_user(&s, argp, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		getnstimeofday(&ts);
#if defined(CONFIG_TZDEV_32BIT_SECURE_KERNEL)
		d = tzdev_smc_update_ree_time((int)ts.tv_sec, (int)ts.tv_nsec);
#else
		d = tzdev_smc_update_ree_time(ts.tv_sec, ts.tv_nsec);
#endif
		tzdev_rsp_convert(&s, &d);

		if (copy_to_user(argp, &s, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		return 0;
	}
	case TZIO_BOOST: {
		tz_boost_enable();
		return 0;
	}
	case TZIO_RELAX: {
		tz_boost_disable();
		return 0;
	}
	default:
		return tzdev_platform_ioctl(cmd, arg);
	}
}

static void tzdev_shutdown(void)
{
	if (atomic_read(&tzdev_swd_state) != TZDEV_SWD_DOWN)
		tzdev_smc_shutdown();
}

static const struct file_operations tzdev_fops = {
	.owner = THIS_MODULE,
	.open = tzdev_open,
	.release = tzdev_release,
	.unlocked_ioctl = tzdev_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tzdev_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tzdev_cdev = {
	.name = "tzdev",
	.fops = &tzdev_fops,
	.owner = THIS_MODULE,
};

static struct syscore_ops tzdev_syscore_ops = {
	.shutdown = tzdev_shutdown
};

static int __init init_tzdev(void)
{
	int rc;

	rc = tz_cdev_register(&tzdev_cdev);
	if (rc)
		return rc;

	rc = tzdev_platform_register();
	if (rc) {
		tzdev_print(0, "tzdev_platform_register() failed with error=%d\n", rc);
		goto platform_driver_registration_failed;
	}

	rc = tzdev_init_hotplug();
	if (rc) {
		tzdev_print(0, "tzdev_init_hotplug() failed with error=%d\n", rc);
		goto hotplug_initialization_failed;
	}

	tzdev_cma_mem_init(tzdev_cdev.device);

#if defined(CONFIG_TZDEV_EARLY_SWD_INIT)
	rc = tzdev_run_init_sequence();
	if (rc) {
		tzdev_print(0, "tzdev_run_init_sequence() failed with error=%d\n", rc);
		goto tzdev_initialization_failed;
	}

	rc = tzdev_restart_swd_userspace();
	if (rc) {
		tzdev_print(0, "tzdev_restart_swd_userspace() failed with error=%d\n", rc);
		goto tzdev_swd_restart_failed;
	}
#endif /* CONFIG_TZDEV_EARLY_SWD_INIT */

	register_syscore_ops(&tzdev_syscore_ops);

	return rc;

#if defined(CONFIG_TZDEV_EARLY_SWD_INIT)
tzdev_swd_restart_failed:
tzdev_initialization_failed:
	tzdev_cma_mem_release(tzdev_cdev.device);
#endif /* CONFIG_TZDEV_EARLY_SWD_INIT */
hotplug_initialization_failed:
	tzdev_platform_unregister();
platform_driver_registration_failed:
	tz_cdev_unregister(&tzdev_cdev);

	return rc;
}

static void __exit exit_tzdev(void)
{
	tzdev_platform_unregister();

	tz_cdev_unregister(&tzdev_cdev);

	tzdev_mem_fini();

	tzdev_cma_mem_release(tzdev_cdev.device);

	unregister_syscore_ops(&tzdev_syscore_ops);

	tzdev_shutdown();

	tzdev_exit_hotplug();
}

module_init(init_tzdev);
module_exit(exit_tzdev);
