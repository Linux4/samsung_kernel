/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/uaccess.h>

#include "tz_chtest.h"
#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/subsystem.h"
#include "core/sysdep.h"

MODULE_LICENSE("GPL");

#ifdef CUSTOM_HANDLER_DEBUG
#define LOG_DBG(...)		pr_info( "[tz_chtest] DBG : " __VA_ARGS__)
#else
#define LOG_DBG(...)
#endif /* CUSTOM_HANDLER_DEBUG */
#define LOG_ERR(...)		pr_alert("[tz_chtest] ERR : " __VA_ARGS__)

#define TZCH_PAGES_ORDER	4
#define TZCH_PAGES_COUNT	(1 << TZCH_PAGES_ORDER)

_Static_assert(TZCH_PAGES_ORDER <= MAX_ORDER, "Too big buffer's order");

static char *tzch_buf;
static phys_addr_t tzch_buf_phys;

static int tz_chtest_fd_open = 0;
static unsigned long nr_pages_used = 0;
static DEFINE_MUTEX(tz_chtest_fd_lock);

static int tz_chtest_drv_open(struct inode *n, struct file *f)
{
	int ret = 0;

	mutex_lock(&tz_chtest_fd_lock);
	if (tz_chtest_fd_open) {
		ret = -EBUSY;
		goto out;
	}
	tz_chtest_fd_open++;
	LOG_DBG("opened\n");

out:
	mutex_unlock(&tz_chtest_fd_lock);
	return ret;
}

static inline int tz_chtest_drv_release(struct inode *inode, struct file *file)
{
	mutex_lock(&tz_chtest_fd_lock);
	if (tz_chtest_fd_open)
		tz_chtest_fd_open--;
	nr_pages_used = 0;
	mutex_unlock(&tz_chtest_fd_lock);
	LOG_DBG("released\n");
	return 0;
}

static int tz_chtest_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	if (vma->vm_pgoff != nr_pages_used)
		return -ENXIO;

	if (vma_pages(vma) > TZCH_PAGES_COUNT - nr_pages_used)
		return -ENXIO;

	ret = remap_pfn_range(vma, vma->vm_start, __phys_to_pfn(tzch_buf_phys) + vma->vm_pgoff,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);

	if (!ret)
		nr_pages_used += vma_pages(vma);

	return ret;
}

static long tz_chtest_drv_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct tzdev_smc_data tzdev_smc_cmd;
	void __user *ioargp = (void __user *) arg;
	struct tz_chtest_data chtest_data;

	switch (cmd) {
	case IOCTL_CUSTOM_HANDLER_CMD:
		LOG_DBG("IOCTL_CUSTOM_HANDLER_CMD\n");
		ret = copy_from_user(&chtest_data, ioargp, sizeof(chtest_data));
		if (ret != 0) {
			LOG_ERR("copy_from_user failed, ret = 0x%08x\n", ret);
			return -EFAULT;
		}

		if (chtest_data.r0 < CUSTOM_HANDLER_CMD_BASE_RAW ||
				chtest_data.r0 >= (CUSTOM_HANDLER_EXEC_BASE_RAW	+ CUSTOM_HANDLER_COUNT)) {
			LOG_ERR("FID number is out of range\n");
			return -EFAULT;
		}

		/* Add FID base to produce the final SCM FID number */
		chtest_data.r0 |= CUSTOM_HANDLER_FID_BASE;

		tzdev_smc_cmd.args[0] = chtest_data.r0;
		tzdev_smc_cmd.args[1] = chtest_data.r1;
		tzdev_smc_cmd.args[2] = chtest_data.r2;
		tzdev_smc_cmd.args[3] = chtest_data.r3;
		tzdev_smc_cmd.args[4] = chtest_data.r4;
		tzdev_smc_cmd.args[5] = chtest_data.r5;
		tzdev_smc_cmd.args[6] = 0;

		LOG_DBG("Regs before : r0 = 0x%lx, r1, = 0x%lx, r2 = 0x%lx, r3 = 0x%lx, r4 = 0x%lx, r5 = 0x%lx\n",
			chtest_data.r0, chtest_data.r1, chtest_data.r2, chtest_data.r3, chtest_data.r4, chtest_data.r5);

		ret = tzdev_smc(&tzdev_smc_cmd);
		if (!ret) {
			chtest_data.r0 = tzdev_smc_cmd.args[0];
			chtest_data.r1 = tzdev_smc_cmd.args[1];
			chtest_data.r2 = tzdev_smc_cmd.args[2];
			chtest_data.r3 = tzdev_smc_cmd.args[3];
			chtest_data.r4 = tzdev_smc_cmd.args[4];
			chtest_data.r5 = tzdev_smc_cmd.args[5];
		} else {
			LOG_ERR("tzdev_smc() failed, ret = 0x%08x\n", ret);
			return -EFAULT;
		}

		LOG_DBG("Regs after  : r0 = 0x%lx, r1, = 0x%lx, r2 = 0x%lx, r3 = 0x%lx, r4 = 0x%lx, r5 = 0x%lx\n",
			chtest_data.r0, chtest_data.r1, chtest_data.r2, chtest_data.r3, chtest_data.r4, chtest_data.r5);

		ret = copy_to_user(ioargp, &chtest_data, sizeof(chtest_data));
		if (ret != 0) {
			LOG_ERR("copy_to_user failed, ret = 0x%08x\n", ret);
			return -EFAULT;
		}
		break;

	default:
		LOG_ERR("UNKNOWN CMD, cmd = 0x%08x\n", cmd);
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations misc_fops = {
	.owner = THIS_MODULE,
	.open = tz_chtest_drv_open,
	.unlocked_ioctl = tz_chtest_drv_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_chtest_drv_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap = tz_chtest_drv_mmap,
	.release = tz_chtest_drv_release,
};

static struct tz_cdev tz_chtest_cdev = {
	.name = CUSTOM_HANDLER_TEST_NAME,
	.fops = &misc_fops,
	.owner = THIS_MODULE,
};

static ssize_t physattr_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%#016llx\n", (unsigned long long)tzch_buf_phys);
};

static DEVICE_ATTR(physaddr, S_IRUSR | S_IRGRP, physattr_show, NULL);

int tz_chtest_drv_init(void)
{
	int ret = 0;
	struct page *tzch_buf_page = alloc_pages(GFP_KERNEL, TZCH_PAGES_ORDER);

	if (!tzch_buf_page)
		return -ENOMEM;

	tzch_buf_phys = page_to_phys(tzch_buf_page);
	tzch_buf = phys_to_virt(tzch_buf_phys);

	ret = tz_cdev_register(&tz_chtest_cdev);
	if (ret) {
		LOG_ERR("Unable to register tz_chtest driver, ret = 0x%08x\n", ret);
		goto free_buf;
	}

	ret = device_create_file(tz_chtest_cdev.device, &dev_attr_physaddr);
	if (ret) {
		pr_err("physaddr sysfs file creation failed\n");
		goto out_unregister;
	}
	LOG_DBG("INSTALLED\n");
	return 0;

out_unregister:
	tz_cdev_unregister(&tz_chtest_cdev);

free_buf:
	free_pages((unsigned long)tzch_buf, TZCH_PAGES_ORDER);

	return ret;
}

void tz_chtest_drv_exit(void)
{
	tz_cdev_unregister(&tz_chtest_cdev);
	free_pages((unsigned long)tzch_buf, TZCH_PAGES_ORDER);
	LOG_DBG("REMOVED\n");
}

tzdev_initcall(tz_chtest_drv_init);
tzdev_exitcall(tz_chtest_drv_exit);
