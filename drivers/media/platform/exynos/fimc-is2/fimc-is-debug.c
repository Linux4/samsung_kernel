/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>

#include "fimc-is-config.h"
#include "fimc-is-debug.h"
#include "fimc-is-device-ischain.h"

struct fimc_is_debug fimc_is_debug;

#define DEBUG_FS_ROOT_NAME	"fimc-is"
#define DEBUG_FS_LOGFILE_NAME	"isfw-msg"
#define DEBUG_FS_IMGFILE_NAME	"dump-img"

static const struct file_operations debug_log_fops;
static const struct file_operations debug_img_fops;

void fimc_is_dmsg_init(void)
{
	fimc_is_debug.dsentence_pos = 0;
	memset(fimc_is_debug.dsentence, 0x0, DEBUG_SENTENCE_MAX);
}

void fimc_is_dmsg_concate(const char *fmt, ...)
{
	va_list ap;
	char term[50];
	u32 copy_len;

	va_start(ap, fmt);
	vsnprintf(term, sizeof(term), fmt, ap);
	va_end(ap);

	copy_len = min((DEBUG_SENTENCE_MAX - fimc_is_debug.dsentence_pos), strlen(term));
	strncpy(fimc_is_debug.dsentence + fimc_is_debug.dsentence_pos, term, copy_len);
	fimc_is_debug.dsentence_pos += copy_len;
}

char *fimc_is_dmsg_print(void)
{
	return fimc_is_debug.dsentence;
}

void fimc_is_print_buffer(char *buffer, size_t len)
{
	u32 read_cnt, sentence_i;
	char sentence[250];
	char letter;

	BUG_ON(!buffer);

	sentence_i = 0;

	for (read_cnt = 0; read_cnt < len; read_cnt++) {
		letter = buffer[read_cnt];
		if (letter) {
			sentence[sentence_i] = letter;
			if (sentence_i >= 247) {
				sentence[sentence_i + 1] = '\n';
				sentence[sentence_i + 2] = 0;
				printk(KERN_DEBUG "%s", sentence);
				sentence_i = 0;
			} else if (letter == '\n') {
				sentence[sentence_i + 1] = 0;
				printk(KERN_DEBUG "%s", sentence);
				sentence_i = 0;
			} else {
				sentence_i++;
			}
		}
	}
}

int fimc_is_debug_probe(void)
{
	fimc_is_debug.read_vptr = 0;
	fimc_is_debug.minfo = NULL;

	fimc_is_debug.dump_count = DBG_IMAGE_DUMP_COUNT;
	fimc_is_debug.img_kvaddr = 0;
	fimc_is_debug.img_cookie = 0;
	fimc_is_debug.size = 0;
	fimc_is_debug.dsentence_pos = 0;
	memset(fimc_is_debug.dsentence, 0x0, DEBUG_SENTENCE_MAX);

#ifdef ENABLE_DBG_FS
	fimc_is_debug.root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (fimc_is_debug.root)
		probe_info("%s is created\n", DEBUG_FS_ROOT_NAME);

	fimc_is_debug.logfile = debugfs_create_file(DEBUG_FS_LOGFILE_NAME, S_IRUSR,
		fimc_is_debug.root, &fimc_is_debug, &debug_log_fops);
	if (fimc_is_debug.logfile)
		probe_info("%s is created\n", DEBUG_FS_LOGFILE_NAME);

#ifdef DBG_IMAGE_DUMP
	fimc_is_debug.imgfile = debugfs_create_file(DEBUG_FS_IMGFILE_NAME, S_IRUSR,
		fimc_is_debug.root, &fimc_is_debug, &debug_img_fops);
	if (fimc_is_debug.imgfile)
		probe_info("%s is created\n", DEBUG_FS_IMGFILE_NAME);
#endif
#endif

	return 0;
}

int fimc_is_debug_open(struct fimc_is_minfo *minfo)
{
	/*
	 * debug control should be reset on camera entrance
	 * because firmware doesn't update this area after reset
	 */
	*((int *)(minfo->kvaddr + DEBUGCTL_OFFSET)) = 0;
	fimc_is_debug.read_vptr = 0;
	fimc_is_debug.minfo = minfo;

	set_bit(FIMC_IS_DEBUG_OPEN, &fimc_is_debug.state);

	return 0;
}

int fimc_is_debug_close(void)
{
	clear_bit(FIMC_IS_DEBUG_OPEN, &fimc_is_debug.state);

	return 0;
}

static int isfw_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static ssize_t isfw_debug_read(struct file *file, char __user *user_buf,
	size_t buf_len, loff_t *ppos)
{
	void *read_ptr;
	size_t write_vptr, read_vptr, buf_vptr;
	size_t read_cnt, read_cnt1, read_cnt2;
	struct fimc_is_minfo *minfo;

	while (!test_bit(FIMC_IS_DEBUG_OPEN, &fimc_is_debug.state))
		msleep(500);

	minfo = fimc_is_debug.minfo;

retry:
#ifdef ENABLE_IS_CORE
	vb2_ion_sync_for_device(minfo->fw_cookie, DEBUG_OFFSET, DEBUG_CNT, DMA_FROM_DEVICE);
#endif
	write_vptr = *((int *)(minfo->kvaddr + DEBUGCTL_OFFSET)) - DEBUG_OFFSET;
	read_vptr = fimc_is_debug.read_vptr;
	buf_vptr = buf_len;

	if (write_vptr >= read_vptr) {
		read_cnt1 = write_vptr - read_vptr;
		read_cnt2 = 0;
	} else {
		read_cnt1 = DEBUG_CNT - read_vptr;
		read_cnt2 = write_vptr;
	}

	if (buf_vptr && read_cnt1) {
		read_ptr = (void *)(minfo->kvaddr + DEBUG_OFFSET + fimc_is_debug.read_vptr);

		if (read_cnt1 > buf_vptr)
			read_cnt1 = buf_vptr;

		memcpy(user_buf, read_ptr, read_cnt1);
		fimc_is_debug.read_vptr += read_cnt1;
		buf_vptr -= read_cnt1;
	}

	if (fimc_is_debug.read_vptr >= DEBUG_CNT) {
		if (fimc_is_debug.read_vptr > DEBUG_CNT)
			err("[DBG] read_vptr(%zd) is invalid", fimc_is_debug.read_vptr);
		fimc_is_debug.read_vptr = 0;
	}

	if (buf_vptr && read_cnt2) {
		read_ptr = (void *)(minfo->kvaddr + DEBUG_OFFSET + fimc_is_debug.read_vptr);

		if (read_cnt2 > buf_vptr)
			read_cnt2 = buf_vptr;

		memcpy(user_buf, read_ptr, read_cnt2);
		fimc_is_debug.read_vptr += read_cnt2;
		buf_vptr -= read_cnt2;
	}

	read_cnt = buf_len - buf_vptr;

	/* info("[DBG] FW_READ : read_vptr(%zd), write_vptr(%zd) - dump(%zd)\n", read_vptr, write_vptr, read_cnt); */

	if (read_cnt == 0) {
		msleep(500);
		goto retry;
	}

	return read_cnt;
}

int imgdump_request(ulong cookie, ulong kvaddr, size_t size)
{
	if (fimc_is_debug.dump_count && (fimc_is_debug.size == 0) && (fimc_is_debug.img_kvaddr == 0)) {
		fimc_is_debug.dump_count--;
		fimc_is_debug.img_cookie = cookie;
		fimc_is_debug.img_kvaddr = kvaddr;
		fimc_is_debug.size = size;
	}

	return 0;
}

static int imgdump_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static ssize_t imgdump_debug_read(struct file *file, char __user *user_buf,
	size_t buf_len, loff_t *ppos)
{
	size_t size = 0;

	if (buf_len <= fimc_is_debug.size)
		size = buf_len;
	else
		size = fimc_is_debug.size;

	if (size) {
		vb2_ion_sync_for_device((void *)fimc_is_debug.img_cookie, 0, size, DMA_FROM_DEVICE);
		memcpy(user_buf, (void *)fimc_is_debug.img_kvaddr, size);
		info("DUMP : %p, SIZE : %zd\n", (void *)fimc_is_debug.img_kvaddr, size);
	}

	fimc_is_debug.img_cookie += size;
	fimc_is_debug.img_kvaddr += size;
	fimc_is_debug.size -= size;

	if (size == 0) {
		fimc_is_debug.img_cookie = 0;
		fimc_is_debug.img_kvaddr = 0;
	}

	return size;
}

static const struct file_operations debug_log_fops = {
	.open	= isfw_debug_open,
	.read	= isfw_debug_read,
	.llseek	= default_llseek
};

static const struct file_operations debug_img_fops = {
	.open	= imgdump_debug_open,
	.read	= imgdump_debug_read,
	.llseek	= default_llseek
};
