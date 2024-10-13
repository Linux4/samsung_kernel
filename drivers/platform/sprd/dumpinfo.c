/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * the sprd_dumpinfo tool consist of the following parts:
 *
 *
 *  java layer   (collect java crash infos)
 *     |
 *     |
 *     v
 *    jni
 *     |
 *     |
 *     v
 *       native layer  (collect native crash infos)
 *	    |
 *	    |
 *          v
 *        kernel       (for userspace  layer, store the
 *                      collected infos into specified memory area)
 *                     (for kernesapce layer, collect crash infos
 *                      and store them into specified memory area)
 *
 *
 *        u-boot       (retrieve the collected infos and display
 *                      them on the phone's LCD screen)
 *
 *
 * when kernel crash or framework crash occured, the sprd_dumpinfo tool will
 *
 * collect some infos, the phone would be reboot automaticlly, the sprd_dumpinfo
 *
 * will display the collected infos on the phone's LCD.
 *
 * the sprd_dumpinfo tool created for QA and Test team. mightbe useful for
 * developers
 *
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/init.h>
#include <linux/highuid.h>
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/memory.h>
#include <asm/cacheflush.h>
#ifndef CONFIG_64BIT
#include <asm/outercache.h>
#endif

#include <asm-generic/sections.h>
#include <linux/utsname.h>
#include <linux/kallsyms.h>

#include "sysdump.h"

#define U_MAGIC             "sprd_framework"
#define K_MAGIC             "sprd_kernel"
#define MAX_LEN             2048

struct kernel_dumpinfo {
	int  m_dumpinfo_len;
	char m_dumpinfo[MAX_LEN];
};


static DEFINE_MUTEX(s_fdump_mutex);
static DEFINE_MUTEX(s_kdump_mutex);

static struct kernel_dumpinfo *kc_info;

static int s_dump_length = 16 * 6;

static int  dumpinfodev_init(void);
static void dumpinfodev_exit(void);

static int framework_info_put(char *payload, int len)
{
	int ret = len;

	if (len < 0 ||  len > MAX_LEN) {
		pr_debug(KERN_ERR"len is %d, max_len is 2048\n\n", len);
		return -EINVAL;
	}

	mutex_lock(&s_fdump_mutex);

	ret = sysdump_fill_crash_content(FRAMEWORK_CRASH,
					 U_MAGIC, payload, len);

	mutex_unlock(&s_fdump_mutex);

	return ret;
}

static void add_log(struct kernel_dumpinfo *self, const char *buf)
{
	int len = 0;
	char *dest = self->m_dumpinfo;

	len = strlen(buf);

	if (self->m_dumpinfo_len + len > (MAX_LEN - 1)) {
		pr_info("[%s,%d]overflow, Leave %s\n",
			__FILE__, __LINE__, __func__);
		return;
	}

	memcpy(dest + self->m_dumpinfo_len, buf, len);
	self->m_dumpinfo_len += len;
}

static void kernel_info_put(struct kernel_dumpinfo *self)
{
	if ((self->m_dumpinfo_len > 0) && (0 != self->m_dumpinfo[0])) {
		pr_debug("[%s,%d]m_dumpinfo_len is %d, strlen(m_framework_dumpinfo) is %d\n",
			 __FILE__, __LINE__, self->m_dumpinfo_len, strlen(self->m_dumpinfo));
		pr_debug("[%s,%d]info would be written to kernel:\n %s\n",
			 __FILE__, __LINE__, self->m_dumpinfo);
		{
			unsigned long long t;
			unsigned long nanosec_rem;

			mutex_lock(&s_kdump_mutex);

			sysdump_fill_crash_content(KERNEL_CRASH,
						   K_MAGIC,
						   self->m_dumpinfo,
						   self->m_dumpinfo_len + 1);

			mutex_unlock(&s_kdump_mutex);

		}
	}
}



int dumpinfo_init(void)
{
	kc_info = kmalloc(sizeof(*kc_info), GFP_KERNEL);
	if (NULL == kc_info) {
		pr_warn("[%s,%d,%s]why kc_info is  NULL\n",
			__FILE__, __LINE__, __func__);
		return -1;
	}

	kc_info->m_dumpinfo_len = 0;
	memset(kc_info->m_dumpinfo, 0, MAX_LEN);

	dumpinfodev_init();

	return 0;
}



void dumpinfo_exit()
{
	kfree(kc_info);

	kc_info = NULL;

	dumpinfodev_exit();
}


#ifndef CONFIG_64BIT
#define BUF_SIZE 1024
#else
#define BUF_SIZE 1000
#endif

void add_kernel_log(const char *format, ...)
{
	va_list arglist;
	char buf[BUF_SIZE];

	memset(buf, 0, BUF_SIZE);
	va_start(arglist, format);
	vsnprintf(buf, BUF_SIZE - 1, format, arglist);
	va_end(arglist);

	add_log(kc_info, buf);
}


#ifdef CONFIG_PREEMPT
#define S_PREEMPT " PREEMPT"
#else
#define S_PREEMPT ""
#endif
#ifdef CONFIG_SMP
#define S_SMP " SMP"
#else
#define S_SMP ""
#endif
#ifdef CONFIG_THUMB2_KERNEL
#define S_ISA " THUMB2"
#else
#define S_ISA " ARM"
#endif


void dump_kernel_crash(const char *reason, struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];
	struct task_struct *tsk = current;

#if 0
{
	struct module *mod;
	char buf[8];

	pr_debug("Modules linked in:");
	add_kernel_log("Modules linked in:");
	/* Most callers should already have preempt disabled, but make sure */
	preempt_disable();
	list_for_each_entry_rcu(mod, &modules, list) {
		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		pr_debug(" %s%s[start=0x%lx,size=%d]",
			 mod->name, module_flags(mod, buf),
			 mod->module_core, mod->core_size);
		add_kernel_log(" %s%s[start=0x%lx,size=%d]",
			     mod->name, module_flags(mod, buf),
			     mod->module_core, mod->core_size);
	}
	preempt_enable();
	if (last_unloaded_module[0]) {
		pr_debug(" [last unloaded: %s]", last_unloaded_module);
		add_kernel_log(" [last unloaded: %s]", last_unloaded_module);
	}
	pr_debug("\n");
	add_kernel_log("\n");
}
#endif

{
	unsigned long addr;
	char buffer[256];
	addr = (unsigned long)__builtin_extract_return_addr((void *)instruction_pointer(regs));
	sprint_symbol(buffer, addr);
	add_kernel_log("%s", buffer);

	addr = (unsigned long)__builtin_extract_return_addr((void *)instruction_return(regs));
	if (addr == 0) {
		addr = (unsigned long)__builtin_extract_return_addr((void *)instruction_pointer(regs));
		addr = ((unsigned long)__builtin_extract_return_addr((void *)addr));
	}
	sprint_symbol(buffer, addr);
	add_kernel_log(" <--- %s", buffer);
}

	kernel_info_put(kc_info);
}


struct dumpinfo_dev {
	struct cdev cdev;
	int dev_major;
	char *data;
	unsigned long size;
	struct class *pclass;
	struct device *dev;
	struct mutex mutex;
};

static struct dumpinfo_dev *s_dumpinfo_dev;


static int dumpinfo_open(struct inode *inode, struct file *filp)
{
	if (NULL == s_dumpinfo_dev)
		return  -ENODEV;

	filp->private_data = s_dumpinfo_dev;

	return 0;
}


static int dumpinfo_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static ssize_t dumpinfo_write(struct file *filp,
			      const char __user *buf,
			      size_t size,
			      loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	char *tmp = buf;
	int ret = 0;
	struct dumpinfo_dev *dev = filp->private_data;

	if (p > MAX_LEN)
		return -EINVAL;

	if (count > MAX_LEN - p)
		count = MAX_LEN - p;

	ret = framework_info_put(tmp, count);
	if (ret >= 0)
		*ppos += count;
	pr_debug(KERN_INFO "written %d bytes(s) from %d\n", count, p);

	return ret;
}


static const struct file_operations dumpinfo_fops = {
	.owner      = THIS_MODULE,
	.open       = dumpinfo_open,
	.release    = dumpinfo_release,
	.write      = dumpinfo_write,
};



static int dumpinfodev_init()
{
	int result = 0;
	dev_t devno;
	int i;

	s_dumpinfo_dev = kmalloc(sizeof(struct dumpinfo_dev), GFP_KERNEL);
	if (NULL == s_dumpinfo_dev) {
		pr_warn("[%s,%d,%s]alloc memory failed\n",
			   __FILE__, __LINE__, __func__);
		return -1;
	}

	memset(s_dumpinfo_dev, 0, sizeof(struct dumpinfo_dev));
	result = alloc_chrdev_region(&devno, 0, 1, "dumpinfo");
	s_dumpinfo_dev->dev_major = MAJOR(devno);
	if (result < 0) {
		pr_warn("[%s,%d,%s]alloc_chrdev_region failed %d\n",
		       __FILE__, __LINE__, __func__, result);
		goto fail_alloc_chrdev;
	}
	cdev_init(&s_dumpinfo_dev->cdev, &dumpinfo_fops);
	s_dumpinfo_dev->cdev.owner = THIS_MODULE;
	s_dumpinfo_dev->cdev.ops   = &dumpinfo_fops;
	result = cdev_add(&s_dumpinfo_dev->cdev, devno, 1);
	if (result) {
		pr_warn("[%s,%d,%s]cdev_add failed %d\n",
			__FILE__, __LINE__, __func__, result);
		goto fail_cdev_add;
	}

	s_dumpinfo_dev->size = MAX_LEN;
	s_dumpinfo_dev->data = kmalloc(MAX_LEN, GFP_KERNEL);
	if (NULL == s_dumpinfo_dev->data) {
		pr_warn("[%s,%d,%s]malloc failed\n",
			   __FILE__, __LINE__, __func__);
		goto fail_alloc_memory;
	}
	memset(s_dumpinfo_dev->data, 0, MAX_LEN);

	s_dumpinfo_dev->pclass = class_create(THIS_MODULE, "dumpinfo_class");
	if (IS_ERR(s_dumpinfo_dev->pclass)) {
		pr_err("create class error\n");
		goto fail_create_class;
	}

	s_dumpinfo_dev->dev = device_create(s_dumpinfo_dev->pclass, NULL,
						  devno, NULL, "dumpinfo");
	if (IS_ERR(s_dumpinfo_dev->dev)) {
		pr_err("create device error\n");
		goto fail_create_device;
	}
	mutex_init(&s_dumpinfo_dev->mutex);

	return 0;

fail_create_device:
	class_destroy(s_dumpinfo_dev->pclass);

fail_create_class:
	kfree(s_dumpinfo_dev->data);

fail_alloc_memory:
	cdev_del(&s_dumpinfo_dev->cdev);

fail_cdev_add:
	unregister_chrdev_region(devno, 1);

fail_alloc_chrdev:
	kfree(s_dumpinfo_dev);

	return -1;
}

static void dumpinfodev_exit()
{
	cdev_del(&s_dumpinfo_dev->cdev);
	device_destroy(s_dumpinfo_dev->pclass,
		       MKDEV(s_dumpinfo_dev->dev_major, 0));
	class_destroy(s_dumpinfo_dev->pclass);
	unregister_chrdev_region(MKDEV(s_dumpinfo_dev->dev_major, 0), 1);
	kfree(s_dumpinfo_dev);

	s_dumpinfo_dev = NULL;
}
