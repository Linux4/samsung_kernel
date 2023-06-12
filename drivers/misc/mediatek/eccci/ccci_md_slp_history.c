/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <mt-plat/mt_ccci_common.h>
#include "ccci_config.h"

struct md_slp_history_info {
	volatile unsigned int guarden_pattern;
	volatile unsigned int write_pointer;
	volatile unsigned int control_status;
	volatile unsigned int version;
};
static struct md_slp_history_info *s_md1_slp_history;

#define MD_SLP_TMP_BUF_SIZE	4096
#define MD_TICK_BASE		64
#define MD_SLP_ONE_LINE_SIZE	100
#define MD_SLP_REGION_MAGIC	0x12345678

struct ccci_md_slp_proc_ctlb {
	unsigned int summary_size;
	unsigned int summary_read;
	unsigned int history_log_num;
	unsigned int history_read_out_num;
	unsigned char *history_buff;
	unsigned int history_start_idx;
	unsigned char *tmp_buff;
	unsigned int tail_size;
	unsigned int tail_read;
	struct mutex file_lock;
};

struct slp_log_item {
	unsigned int tik_num;
	unsigned int info;
};

static int get_md_slp_raw_data(void *raw_mem_start, void *info_ctlb)
{
	struct md_slp_history_info *history_ctlb = (struct md_slp_history_info *)raw_mem_start;
	struct ccci_md_slp_proc_ctlb *user_info = (struct ccci_md_slp_proc_ctlb *)info_ctlb;
	unsigned int magic;
	volatile unsigned int wp;
	volatile unsigned int wp_tmp;
	unsigned int ctrl;
	unsigned int *curr = (unsigned int *)user_info->history_buff;
	unsigned int record_num = 0, i;
	unsigned int *raw_data_ptr;
	unsigned int cpy_num, first_cpy;
	unsigned int *ptr;

	user_info->history_log_num = 0;
	user_info->history_read_out_num = 0;
	user_info->history_start_idx = 0;

	if (s_md1_slp_history == NULL) {
		pr_err("[ccci0/mdslp] raw buff not ready\r\n");
		return 0;
	}
	magic = history_ctlb->guarden_pattern;
	wp = history_ctlb->write_pointer;
	ctrl = history_ctlb->control_status;

	ptr = (unsigned int *)s_md1_slp_history;

	if (magic != MD_SLP_REGION_MAGIC) {
		pr_err("[ccci0/mdslp] invalid magic\r\n");
		return 0;
	}

	if ((wp == 0) && (ctrl == 0)) {
		pr_err("[ccci0/mdslp] no data\r\n");
		return 0;
	}

	wp = wp & (~CCCI_SMEM_MD_SLPLOG_SIZE);
	if (ctrl > 0) {/* Over flow case */
		record_num =  (CCCI_SMEM_MD_SLPLOG_SIZE - sizeof(struct md_slp_history_info))>>3;
		first_cpy = CCCI_SMEM_MD_SLPLOG_SIZE - wp - 8;
		first_cpy = (first_cpy>>3); /* /8 */
		raw_data_ptr = (unsigned int *)(raw_mem_start + wp);
		raw_data_ptr += 2; /* write pointer is the newest, next one is the oldest */
		for (i = 0; i < first_cpy; i++) {
			curr[2*i] = *raw_data_ptr++;
			curr[2*i+1] = *raw_data_ptr++;
		}
		cpy_num = (wp - sizeof(struct md_slp_history_info))>>3;
		cpy_num++;
		raw_data_ptr = (unsigned int *)(raw_mem_start + sizeof(struct md_slp_history_info));
		for (; i < (cpy_num + first_cpy); i++) {
			curr[2*i] = *raw_data_ptr++;
			curr[2*i+1] = *raw_data_ptr++;
		}
		/* read wp again */
		wp_tmp = history_ctlb->write_pointer;
		if (wp < wp_tmp)
			user_info->history_start_idx = ((wp_tmp - wp)>>3) + 1;
		else if (wp > wp_tmp)
			user_info->history_start_idx = ((CCCI_SMEM_MD_SLPLOG_SIZE - wp + wp_tmp)>>3) + 1;
	} else {
		cpy_num = ((wp - sizeof(struct md_slp_history_info))>>3) + 1;
		record_num = cpy_num;
		raw_data_ptr = (unsigned int *)(raw_mem_start + sizeof(struct md_slp_history_info));
		for (i = 0; i < cpy_num; i++) {
			curr[2*i] = *raw_data_ptr++;
			curr[2*i+1] = *raw_data_ptr++;
		}
		/* read wp again */
		if (wp > history_ctlb->write_pointer)
			user_info->history_start_idx = (history_ctlb->write_pointer>>3) + 1;
	}
	/* Final adjust */
	if (user_info->history_start_idx) { /* This means roll back case */
		if (user_info->history_start_idx < (record_num - 2))
			user_info->history_start_idx += 2; /* Some times the first recod is the newest,by pass it */
		else
			user_info->history_start_idx = record_num; /* Make no data out */
	}

	user_info->history_log_num = record_num - user_info->history_start_idx;

	return user_info->history_log_num;
}

static void parsing_raw_data(struct ccci_md_slp_proc_ctlb *user_info,
					unsigned long long *slp, unsigned long long *wk,
					unsigned int *start_t, unsigned int *end_t)
{
	unsigned int i;
	struct slp_log_item *curr, *first, *prev;
	unsigned long long slp_total = 0, wake_total = 0, tick1, tick2, delta;
	unsigned int offset;
	int record_num;

	/* Calculate data one by one */
	offset = (user_info->history_start_idx)<<3;
	first = (struct slp_log_item *)&user_info->history_buff[offset];
	record_num = user_info->history_log_num;

	if (record_num < 2)
		return;

	prev = first;
	curr = prev;
	curr++;
	for (i = 0; i < record_num - 1; i++) {
		tick1 = prev->tik_num;
		tick2 = curr->tik_num;
		if (tick2 < tick1)
			delta = tick2 + 0xFFFFFFFF + 1 - tick1;
		else
			delta = tick2 - tick1;
		if (curr->info & (1<<31))
			slp_total += delta;
		else
			wake_total += delta;

		prev++;
		curr++;
	}

	*slp = slp_total;
	*wk = wake_total;
	*start_t = first->tik_num;
	*end_t = prev->tik_num;
}

static ssize_t ccci_md_slp_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct ccci_md_slp_proc_ctlb *user_info;
	unsigned long long ll_tmp1, ll_tmp2, ll_tmp3;
	int update = 0;
	int ret;
	int has_cpy = 0;
	struct slp_log_item *slp_record;
	int record_num;
	unsigned int tmp_tick, tmp_info;
	unsigned long usec1, usec2;
	char ch;
	int avai, cpy_size;
	unsigned int offset;
	unsigned int start_tick = 0, end_tick = 0;
	unsigned long long sleep_tick = 0, awake_tick = 0;

	user_info = (struct ccci_md_slp_proc_ctlb *)file->private_data;
	if (user_info == NULL)
		return 0;

	/* This make sure avoid read when close */
	mutex_lock(&user_info->file_lock);
	if (user_info->history_buff == NULL) {
		mutex_unlock(&user_info->file_lock);
		return 0;
	}

	/* Prepare summary info if needed */
	if (user_info->summary_size == 0) {
		/* Copy raw data */
		ret = get_md_slp_raw_data(s_md1_slp_history, user_info);
		if (ret < 2) {
			mutex_unlock(&user_info->file_lock);
			return 0;
		}

		/***********************************************/
		/*****  2.Tine at start writing log to file  ***/
		/***********************************************/
		ll_tmp1 = local_clock();
		ll_tmp2 = ll_tmp1;
		usec1 = do_div(ll_tmp1, 1000000000);
		ret =  snprintf(&user_info->tmp_buff[update], MD_SLP_TMP_BUF_SIZE - update,
				"Start : %llu, %lu.%06lusec\n\n", ll_tmp2, (unsigned long)ll_tmp1, usec1 / 1000);
		if (ret > 0)
			update += ret;

		/***********************************************/
		/*****   3. Sleep statics   ********************/
		/***********************************************/
		ret =  snprintf(&user_info->tmp_buff[update], MD_SLP_TMP_BUF_SIZE - update,
				"Sleep Statics\n===== Client : 0 - MTK =====\n");
		if (ret > 0)
			update += ret;
		parsing_raw_data(user_info, &sleep_tick, &awake_tick, &start_tick, &end_tick);
		ll_tmp3 = sleep_tick + awake_tick;
		ll_tmp1 = sleep_tick*100000000;
		ll_tmp2 = awake_tick*100000000;
		usec1 = do_div(ll_tmp1, ll_tmp3);
		usec1 = do_div(ll_tmp1, 1000000);
		usec2 = do_div(ll_tmp2, ll_tmp3);
		usec2 = do_div(ll_tmp2, 1000000);

		ret =  snprintf(&user_info->tmp_buff[update], MD_SLP_TMP_BUF_SIZE - update,
				"Time Sleep : %llu\nTime Awake : %llu\nSleep : %llu.%06lu%% Awake : %llu.%06lu%%\n",
				sleep_tick, awake_tick, ll_tmp1, usec1, ll_tmp2, usec2);
		if (ret > 0)
			update += ret;

		ll_tmp1 = start_tick;
		ll_tmp1 = ll_tmp1*MD_TICK_BASE;
		ll_tmp2 = end_tick;
		ll_tmp2 = ll_tmp2*MD_TICK_BASE;
		usec1 = do_div(ll_tmp1, 1000000);
		usec2 = do_div(ll_tmp2, 1000000);
		ret = snprintf(&user_info->tmp_buff[update], MD_SLP_TMP_BUF_SIZE - update,
				"Log available %llu ~ %llusec\n", ll_tmp1, ll_tmp2);
		if (ret > 0)
			update += ret;

		/***********************************************/
		/*****   4. Title   ****************************/
		/***********************************************/
		ret = snprintf(&user_info->tmp_buff[update], MD_SLP_TMP_BUF_SIZE - update,
				"\nTime sclk\tTime sec\tStatus\tDuration\n===== Client : 0 - MTK =====\n");
		if (ret > 0)
			update += ret;

		user_info->summary_size = update;
	}
	/* Copy summary info to user buffer */
	if (user_info->summary_read < user_info->summary_size) {
		avai = (int)(user_info->summary_size - user_info->summary_read);
		cpy_size = avai > size?size:avai;
		ret = copy_to_user(buf, &user_info->tmp_buff[user_info->summary_read], cpy_size);
		if (ret == 0) {
			has_cpy += cpy_size;
			size -= cpy_size;
			user_info->summary_read += cpy_size;
		} else
			pr_err("[ccci1/mdslp]summary copy fail\n");
	}

	if (size == 0) {
		mutex_unlock(&user_info->file_lock);
		return has_cpy;
	}

	if (user_info->history_log_num < 2) {
		mutex_unlock(&user_info->file_lock);
		return has_cpy;
	}

	/* Copy data one by one */
	if (user_info->history_read_out_num == 0) /* First read out */
		offset = (user_info->history_start_idx)<<3;
	else
		offset = (user_info->history_start_idx + user_info->history_read_out_num - 1)<<3;

	slp_record = (struct slp_log_item *)&user_info->history_buff[offset];

	record_num = user_info->history_log_num - user_info->history_read_out_num - 1;
	while ((size > MD_SLP_ONE_LINE_SIZE) && (record_num > 0)) {
		ll_tmp1 = slp_record->tik_num;
		ll_tmp1 = ll_tmp1*MD_TICK_BASE;
		slp_record++;
		record_num--;
		tmp_tick = slp_record->tik_num;
		tmp_info = slp_record->info;
		ll_tmp2 = tmp_tick*MD_TICK_BASE;

		if (ll_tmp2 < ll_tmp1)
			ll_tmp1 = ll_tmp2 + 0xFFFFFFFF + 1 - ll_tmp1;
		else
			ll_tmp1 = ll_tmp2 - ll_tmp1;
		usec1 = do_div(ll_tmp1, 1000000); /* delta */
		usec2 = do_div(ll_tmp2, 1000000); /* Time from tick */
		if (tmp_info & (1<<31))
			ch = 'S';
		else
			ch = 'W';
		ret = snprintf(user_info->tmp_buff, MD_SLP_TMP_BUF_SIZE,
				"%010u %05llu.%06lu    %c %05llu.%06lu\r\n",
				tmp_tick, ll_tmp2, usec2, ch, ll_tmp1, usec1);
		if (ret > 0) {
			update = ret;
			size -= update;
			ret = copy_to_user(&buf[has_cpy], user_info->tmp_buff, update);
			if (ret == 0)
				has_cpy += update;
			else
				pr_err("[ccci1/mdslp]record copy fail\n");
		}
		user_info->history_read_out_num++;
	}

	if (user_info->tail_size == 0) {
		if ((size > MD_SLP_ONE_LINE_SIZE) && (record_num == 0)) { /* Means all data out */
			ll_tmp1 = local_clock();
			ll_tmp2 = ll_tmp1;
			usec1 = do_div(ll_tmp1, 1000000000);
			ret =  snprintf(user_info->tmp_buff, MD_SLP_TMP_BUF_SIZE,
					"\nEnd : %llu, %lu.%06lusec\n\n", ll_tmp2,
					(unsigned long)ll_tmp1, usec1 / 1000);
			if (ret > 0) {
				user_info->tail_size = ret;
				update = ret;
				size -= update;
				ret = copy_to_user(&buf[has_cpy], user_info->tmp_buff, update);
				if (ret == 0) {
					has_cpy += update;
					user_info->tail_read = user_info->tail_size;
				} else
					pr_err("[ccci1/mdslp]finish time copy fail\n");
			}
		}
	}

	mutex_unlock(&user_info->file_lock);

	return (ssize_t)has_cpy;
}

static ssize_t ccci_md_slp_raw_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct ccci_md_slp_proc_ctlb *user_info;
	int cpy_size;
	int data_size;
	int ret, i;
	int has_copy;
	unsigned int *cpy_start;
	unsigned int *user_start;

	user_info = (struct ccci_md_slp_proc_ctlb *)file->private_data;
	if (user_info == NULL)
		return 0;

	/* This make sure avoid read when close */
	mutex_lock(&user_info->file_lock);
	if (s_md1_slp_history == NULL) {
		mutex_unlock(&user_info->file_lock);
		return 0;
	}
	/* Copy to temp buffe */
	if (user_info->history_log_num == 0) {
		user_start = (unsigned int *)user_info->history_buff;
		cpy_start = (unsigned int *)s_md1_slp_history;
		for (i = 0; i < (CCCI_SMEM_MD_SLPLOG_SIZE>>2); i++)
			user_start[i] = cpy_start[i];
	}

	/* remove copyed data */
	data_size = CCCI_SMEM_MD_SLPLOG_SIZE - user_info->history_read_out_num;
	cpy_start = (unsigned int *)&user_info->history_buff[user_info->history_read_out_num];

	i = 0;
	has_copy = 0;

	while (size > 48) {
		if (data_size == 0)
			break;
		cpy_size = snprintf(user_info->tmp_buff, MD_SLP_TMP_BUF_SIZE, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
				cpy_start[i], cpy_start[i+1], cpy_start[i+2], cpy_start[i+3]);
		i += 4;
		ret = copy_to_user(&buf[has_copy], user_info->tmp_buff, cpy_size);
		if (ret == 0) {
			size -= cpy_size;
			has_copy += cpy_size;
			user_info->history_read_out_num += 16;
			data_size -= 16;
		} else
			break;
	}

	mutex_unlock(&user_info->file_lock);
	return (ssize_t)has_copy;
}

static unsigned int ccci_md_slp_poll(struct file *fp, struct poll_table_struct *poll)
{
	return POLLIN | POLLRDNORM;
}

static int ccci_md_slp_open(struct inode *inode, struct file *file)
{
	struct ccci_md_slp_proc_ctlb *user_info;

	user_info = kzalloc(sizeof(struct ccci_md_slp_proc_ctlb), GFP_KERNEL);
	if (user_info == NULL)
		return -1;

	user_info->tmp_buff = kzalloc(MD_SLP_TMP_BUF_SIZE, GFP_KERNEL);
	if (user_info->tmp_buff == NULL) {
		kfree(user_info);
		return -2;
	}

	user_info->history_buff = vmalloc(CCCI_SMEM_MD_SLPLOG_SIZE);
	if (user_info->history_buff == NULL) {
		kfree(user_info->tmp_buff);
		kfree(user_info);
		return -3;
	}

	file->private_data = user_info;
	mutex_init(&user_info->file_lock);
	nonseekable_open(inode, file);
	return 0;
}

static int ccci_md_slp_close(struct inode *inode, struct file *file)
{
	struct ccci_md_slp_proc_ctlb *user_info;

	user_info = (struct ccci_md_slp_proc_ctlb *)file->private_data;
	if (user_info == NULL)
		return -1;

	mutex_lock(&user_info->file_lock);
	kfree(user_info->tmp_buff);
	vfree(user_info->history_buff);
	user_info->summary_size = 0;
	user_info->summary_read = 0;
	user_info->history_log_num = 0;
	user_info->history_read_out_num = 0;
	user_info->tail_size = 0;
	user_info->tail_read = 0;
	mutex_unlock(&user_info->file_lock);

	kfree(user_info);

	return 0;
}

static const struct file_operations ccci_md_slp_fops = {
	.read = ccci_md_slp_read,
	.open = ccci_md_slp_open,
	.release = ccci_md_slp_close,
	.poll = ccci_md_slp_poll,
};

static const struct file_operations ccci_md_slp_raw_fops = {
	.read = ccci_md_slp_raw_read,
	.open = ccci_md_slp_open,
	.release = ccci_md_slp_close,
	.poll = ccci_md_slp_poll,
};

void update_md_slp_raw_ptr(void *ptr)
{
	s_md1_slp_history = ptr;
}

void ccci_md_slp_buffer_init(void)
{
	struct proc_dir_entry *ccci_md_slp_proc;

	ccci_md_slp_proc = proc_create("ccci_md_slp", 0444, NULL, &ccci_md_slp_fops);
	if (ccci_md_slp_proc == NULL)
		pr_err("[ccci0/mdslp]fail to create proc entry for slp dump\n");

	ccci_md_slp_proc = proc_create("ccci_md_slp_raw", 0444, NULL, &ccci_md_slp_raw_fops);
	if (ccci_md_slp_proc == NULL)
		pr_err("[ccci0/mdslp]fail to create proc entry for slp dump raw\n");
}
