/*
 * Copyright (C) 2013~2014 Marvell International Ltd.
 *		Jialing Fu <jlfu@marvell.com>
 *
 * Try to create a set of APIs to let module/driver can read/write
 * eMMC directly and by-pass the filesystem in kernel
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
#define DEBUG

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <marvell/emmc_rsv.h>

#include "rsvpage.h"

/*
 * Background:
 * As we known, during the kernel bootup/init stage, some parameters are needed
 * by driver or module, and now the DTB/DTS can meet the requirement well
 *
 * But if kernel can fine tune the parameter and want to save it for next kernel
 * boot up cycle; DTB may be not a good option.
 * As in kernel side, there is no ready-made solution to write/update DTB yet.
 *
 * So we need to find out a proper solution to meet this requirement.
 *
 *
 * Idea/Solutions:
 *  1) reserve a small range both in eMMC and memory, now it is 4KB
 *  2) The reserve information are only saved in bootloader, like the eMMC
 *  sector offset and count, the memory address and size.
 *  3) Boot Loader read data from eMMC and save the data to the reserved memory
 *   and pass the information to kernel by bootargs
 *  4) Kernel parse the bootargs and so that get the eMMC/Memory information
 *  5) Kernel defines a global structure for this memory and use them
 *  6) If kernel want to update/save new parameter, kernel can update data to
 *  eMMC by pass the file system
 *
 * So some functions/APIs are added in this file.
 * Also bootloader needs to support us too. For stability, even the relate code
 * is not supported in boot loader, this APIs can still run without bug.
 *
 * Limitations/Notes:
 * 1) eMMC read/write base on sector, please reserve eMMC/DDR memory by 512Byte
 * align.
 * 2) During kernel driver/module init, write data to eMMC maybe not ready yet,
 * so a kthread is used, this kthread would update data to eMMC by retry.
 * Please note there is risk (but very little) to lose data if power off
 * abnormally.
 *
 */

static struct block_device *rsv_page_bdev;
static void *kbuf;
static struct task_struct *my_kthread;
static unsigned long thread_flags;
enum {
	 RSV_PAGE_WRITE_PENDING,
	 RSV_PAGE_READ_PENDING,
};

/* memory addr and size are counted by Byte */
static void *rsv_page_mem_addr;
static long rsv_page_mem_size;

/* eMMC block and count are counted by 512Bytes/sector */
static sector_t rsv_page_blk_sector;
static long rsv_page_blk_cnt;

/*
 * Using "MAGIC" to check whether the buffer passed from u-boot is valid or not
 * Let other driver/module know little detail about it
 * So the "MAGIC" is defined in this C file, but not expored in h file
 *
 */
#define RSV_PAGE_MAGIC1 0x52735670 /* RsVp */
#define RSV_PAGE_MAGIC2 0x4D72566C /* MrVl */

/* get some info passed from bootloader */
static int __init early_get_rsv_blk(char *p)
{
	/*
	 * Get the eMMC's sector offset and count
	 * Please note it is sector, not Byte
	 */
	char *endp;

	rsv_page_blk_cnt = memparse(p, &endp);
	if (*endp == '@')
		rsv_page_blk_sector = memparse(endp + 1, NULL);

	return 0;
}
early_param("rsv_page_blk", early_get_rsv_blk);

static int __init early_get_rsv_mem(char *p)
{
	/*
	* Get the memory address and size
	* Now we use crashkernel, so this function is only reserved for furture
	*
	* As Bootloader pass the phys Addr, here needs to change to kaddr
	*/
	char *endp;
	phys_addr_t paddr;

	rsv_page_mem_size = memparse(p, &endp);
	if (*endp == '@') {
		paddr = (phys_addr_t)memparse(endp + 1, NULL);

		memblock_reserve(paddr, rsv_page_mem_size);
		rsv_page_mem_addr = phys_to_virt(paddr);
	}

	return 0;
}
early_param("rsv_page_mem", early_get_rsv_mem);
/* end of get some info passed from bootloader */

/* start of some debug code to show memory */
#define BYTE_EACH_LINE 16
#define STRING_LEN_EACH_LINE (BYTE_EACH_LINE*3 + 100)

/*
 * Here a static memory(dump_buf) is used and shared by hex_dump functoin
 * so we need to add a mutex to avoid conflict
 *
 * TODO: Consider to use kmalloc within hex_dump instead?
 */
static char dump_buf[STRING_LEN_EACH_LINE];
static DEFINE_MUTEX(dump_lock);
static void dump_line(const unsigned char *data, unsigned int len)
{
	int i;
	char *tmp_buf = &dump_buf[0];

	tmp_buf += sprintf(tmp_buf, "Addr 0x%p: ", data);
	for (i = 0; i < len; i++)
		tmp_buf += sprintf(tmp_buf, "%02x ", data[i]);

	pr_info("%s\n", dump_buf);
}

static void __maybe_unused hex_dump(const unsigned char *data,
	unsigned int len)
{
	int offset;

	mutex_lock(&dump_lock);

	memset(&dump_buf[0], 0, STRING_LEN_EACH_LINE);

	offset = 0;
	while (len >= BYTE_EACH_LINE) {
		dump_line(data + offset, BYTE_EACH_LINE);

		len -= BYTE_EACH_LINE;
		offset += BYTE_EACH_LINE;
	}

	if (len) {
		memset(&dump_buf[0], 0, STRING_LEN_EACH_LINE);
		dump_line(data + offset, len);
	}

	mutex_unlock(&dump_lock);
	return;
}
/* <<<<< End of debug code to show memory >>>>> */

/* >>>>> start of mrvl_rsv_page handler <<<<< */
struct mrvl_rsv_page *rsv_page;
static struct mrvl_rsv_page *get_rsv_page(void)
{
	return rsv_page;
}

static bool is_rsv_mem_valid(struct mrvl_rsv_page *rsv_page)
{
	if (rsv_page && (rsv_page->magic1 == RSV_PAGE_MAGIC1)
		&& (rsv_page->magic2 == RSV_PAGE_MAGIC2))
		return true;
	else
		return false;
}

static int rsv_mem_init(struct mrvl_rsv_page *rsv_page)
{
	memset(rsv_page, 0, sizeof(struct mrvl_rsv_page));
	rsv_page->magic1 = RSV_PAGE_MAGIC1;
	rsv_page->magic2 = RSV_PAGE_MAGIC2;

	return 0;
}

static bool is_rsv_blk_valid(void)
{
	/* whether we got emmc reserved block sucessfully from bootloader? */
	int ret;

	ret = rsv_page_blk_cnt && rsv_page_blk_sector;

	return ret ? true : false;
}

static bool is_rsv_blk_write_safe(void)
{
	/*
	 * if the memory size is larger than blk size reserved
	 * it has risk to over write data to block device
	 */
	return ((rsv_page_blk_cnt << 9) >= rsv_page_mem_size) ? true : false;
}

/*
 * "rsv_page_get_kaddr" functions is exported, so that it may be called by
 * different threads at the same time; should we need to add something to avoid
 * conflict?
 * It is not need now, as this function only read rsv_page without modify, so
 * it is still safe!
 */
void *rsv_page_get_kaddr(enum rsv_page_index index, size_t want)
{
	struct mrvl_rsv_page *rsv_page;
	void *addr = NULL;

	if (index > RSV_PAGE_INDEX_MAX)
		return NULL;

	rsv_page = get_rsv_page();

	switch (index) {
	case RSV_PAGE_SDH_SD:
		if (want <= RSV_PAGE_SIZE_SDH)
			addr = &rsv_page->sdh_sd;
		break;

	case RSV_PAGE_SDH_SDIO:
		if (want <= RSV_PAGE_SIZE_SDH)
			addr = &rsv_page->sdh_sdio;
		break;

	case RSV_PAGE_SDH_EMMC:
		if (want <= RSV_PAGE_SIZE_SDH)
			addr = &rsv_page->sdh_emmc;
		break;

	default:
		break;
	}

	return addr;
}
EXPORT_SYMBOL(rsv_page_get_kaddr);

void rsv_page_update(void)
{
	if (my_kthread) {
		set_bit(RSV_PAGE_WRITE_PENDING, &thread_flags);
		wake_up_process(my_kthread);
	} else
		pr_err("RSV_PAGE: can't update to block device!\n");
}
EXPORT_SYMBOL(rsv_page_update);

static struct block_device *rsv_page_get_bdev(void)
{
	if (IS_ERR_OR_NULL(rsv_page_bdev)) {
		rsv_page_bdev = blkdev_get_by_path("/dev/block/mmcblk0",
			FMODE_READ | FMODE_WRITE, NULL);

		if (IS_ERR_OR_NULL(rsv_page_bdev)) {
			rsv_page_bdev = blkdev_get_by_path("/dev/mmcblk0",
				FMODE_READ | FMODE_WRITE, NULL);
		}
	}

	return IS_ERR_OR_NULL(rsv_page_bdev) ?  NULL :  rsv_page_bdev;
}

static int rsv_page_write(void)
{
	struct block_device *bdev;
	void *kaddr;
	int ret;

	if (!is_rsv_blk_valid()) {
		/* bootloader do not pass the blk info */
		return -EFAULT;
	}

	bdev = rsv_page_get_bdev();

	if (IS_ERR_OR_NULL(bdev)) {
		/* bdev may be not ready, try again */
		return -EAGAIN;
	}

	kaddr = rsv_page_mem_addr;
	ret = write_bio_sectors(bdev, rsv_page_blk_sector,
		kaddr, rsv_page_mem_size);

	return ret;
}

static int __always_unused rsv_page_read(void)
{
	struct block_device *bdev;
	int ret;

	if (!is_rsv_blk_valid()) {
		/* bootloader do not pass the blk info */
		return -EFAULT;
	}

	bdev = rsv_page_get_bdev();
	if (IS_ERR_OR_NULL(bdev)) {
		/* bdev may be not ready, try again */
		return -EAGAIN;
	}

	ret = read_bio_sectors(bdev, rsv_page_blk_sector,
		rsv_page_mem_addr, rsv_page_mem_size);

	return ret;
}

void rsv_page_cleanup_module(void)
{
	pr_debug("RSV_PAGE: start of cleanup_module\n");

	kfree(kbuf);

	if (rsv_page_bdev)
		blkdev_put(rsv_page_bdev, FMODE_READ | FMODE_WRITE);

	if (my_kthread)
		kthread_stop(my_kthread);

	pr_debug("RSV_PAGE: end of cleanup_module\n");
}
module_exit(rsv_page_cleanup_module);

static int rsv_page_thread(void *d)
{
#define MAX_RETRY 10
	int ret;
	int retry;

	do {
		set_current_state(TASK_INTERRUPTIBLE);

		if (test_and_clear_bit(RSV_PAGE_WRITE_PENDING, &thread_flags)) {
			set_current_state(TASK_RUNNING);

			retry = 0;
			while ((retry <= MAX_RETRY) && !kthread_should_stop()) {
				pr_debug("RSV_PAGE: trying to update to disk......\n");

				ret = rsv_page_write();
				if (ret == 0) {
					pr_info("RSV_PAGE: Sucessful update to disk!\n");
					break;
				} else if (ret == -EAGAIN) {
					set_current_state(TASK_INTERRUPTIBLE);
					if (!kthread_should_stop())
						schedule_timeout(HZ*2);
					set_current_state(TASK_RUNNING);

					retry++;
				} else {
					pr_err("RSV_PAGE: rsv_page_write fail!\n");
					break;
				}
			}

			if (retry > MAX_RETRY)
				pr_err("RSV_PAGE: Fail after max retry!\n");
		} else {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			schedule();
		}
	} while (1);

	return 0;

#undef MAX_RETRY
}

int rsv_page_init_module(void)
{
	int ret;

	pr_info("RSV_PAGE: init_module\n");

	if (!rsv_page_mem_addr || (rsv_page_mem_size < RSV_PAGE_SIZE)) {
		pr_info("RSV_PAGE: do not get rsv_mem, use kmalloc instead\n");
		kbuf = kmalloc(RSV_PAGE_SIZE, GFP_KERNEL);
		if (kbuf) {
			rsv_page_mem_addr = kbuf;
			rsv_page_mem_size = RSV_PAGE_SIZE;
		} else {
			pr_err("RSV_PAGE: init fail as not rsv_mem!!\n");
			return -ENOMEM;
		}
	}

	if (!is_rsv_mem_valid(rsv_page_mem_addr))
		rsv_mem_init(rsv_page_mem_addr);

	rsv_page = rsv_page_mem_addr;

	/*
	 * kthread is only used for update data to disk now
	 *
	 * So if any one of below is meet, we do not update mrvl_rsv_page,
	 * and so that the kthread is not need:
	 * 1. rsv_blk is not valid
	 * 2. rsv_mem is not passed from bootloader
	 * 3. it has risk to overwrite the blk device
	 *
	 */
	if (is_rsv_blk_valid() && (kbuf == NULL)
		&& (is_rsv_blk_write_safe())) {

		pr_debug("RSV_PAGE: Create a kthead for delay write\n");

		/* try to create a kthread to write data to eMMC */
		my_kthread = kthread_run(rsv_page_thread, NULL, "emmc_rsv");

		if (IS_ERR(my_kthread)) {
			ret = PTR_ERR(my_kthread);
			goto kthread_fail;
		}
	}

	return 0;

kthread_fail:
	pr_err("RSV_PAGE: init_module Fail\n");
	kfree(kbuf);

	return -ENOTBLK;
}
rootfs_initcall(rsv_page_init_module);

MODULE_AUTHOR("Jialing Fu");
MODULE_DESCRIPTION("Marvell reserve page");
MODULE_LICENSE("GPL");
