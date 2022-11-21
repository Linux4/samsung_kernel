/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>

#include "vertex-log.h"
#include "vertex-device.h"
#include "vertex-graphmgr.h"
#include "vertex-graph.h"
#include "vertex-debug.h"

#define DEBUG_FS_ROOT_NAME		"vertex"
#define DEBUG_FS_LOGFILE_NAME		"fw-msg"
#define DEBUG_FS_IMGFILE_NAME		"dump-img"
#define DEBUG_FS_GRPFILE_NAME		"graph"
#define DEBUG_FS_BUFFILE_NAME		"buffer"

#define DEBUG_MONITORING_PERIOD		(HZ * 5)
#define CHUNK_SIZE			(32)

static struct vertex_device *debug_device;

int vertex_debug_write_log_binary(void)
{
	int ret;
	struct vertex_system *sys;
	struct vertex_binary *bin;

	vertex_enter();
	sys = &debug_device->system;
	bin = &sys->binary;

	if (!sys->memory.debug.kvaddr)
		return -ENOMEM;

	ret = vertex_binary_write(bin, VERTEX_DEBUG_BIN_PATH,
			"vertex_log.bin",
			sys->memory.debug.kvaddr,
			sys->memory.debug.size);
	if (!ret)
		vertex_info("%s/vertex_log.bin was created for debugging\n",
				VERTEX_DEBUG_BIN_PATH);

	vertex_leave();
	return ret;
}

int vertex_debug_dump_debug_regs(void)
{
	struct vertex_system *sys;

	vertex_enter();
	sys = &debug_device->system;

	sys->ctrl_ops->debug_dump(sys);
	vertex_leave();
	return 0;
}

int vertex_debug_atoi(const char *psz_buf)
{
	const char *pch = psz_buf;
	int base;

	while (isspace(*pch))
		pch++;

	if (*pch == '-' || *pch == '+') {
		base = 10;
		pch++;
	} else if (*pch && tolower(pch[strlen(pch) - 1]) == 'h') {
		base = 16;
	} else {
		base = 0;
	}

	return kstrtoul(pch, base, NULL);
}

int vertex_debug_bitmap_scnprintf(char *buf, unsigned int buflen,
		const unsigned long *maskp, int nmaskbits)
{
	int chunksz;
	int idx;
	unsigned int chunkmask;
	int word, bit, len;
	unsigned long val;
	const char *sep = "";

	chunksz = nmaskbits & (CHUNK_SIZE - 1);
	if (chunksz == 0)
		chunksz = CHUNK_SIZE;

	for (idx = ALIGN(nmaskbits, CHUNK_SIZE) - CHUNK_SIZE, len = 0; idx >= 0;
			idx -= CHUNK_SIZE) {
		chunkmask = ((1ULL << chunksz) - 1);
		word = idx / BITS_PER_LONG;
		bit = idx % BITS_PER_LONG;
		val = (maskp[word] >> bit) & chunkmask;
		len += scnprintf(buf + len, buflen - len, "%s%0*lx", sep,
				(chunksz + 3) / 4, val);
		chunksz = CHUNK_SIZE;
		sep = ",";
	}

	return len;
}

void vertex_dmsg_concate(struct vertex_debug_log *log, const char *fmt, ...)
{
	va_list ap;
	char term[50];
	size_t size;

	va_start(ap, fmt);
	vsnprintf(term, sizeof(term), fmt, ap);
	va_end(ap);

	if (log->dsentence_pos >= DEBUG_SENTENCE_MAX) {
		vertex_err("debug message(%zu) over max\n", log->dsentence_pos);
		return;
	}

	size = min((DEBUG_SENTENCE_MAX - log->dsentence_pos - 1),
			strlen(term));
	strncpy(log->dsentence + log->dsentence_pos, term, size);
	log->dsentence_pos += size;
	log->dsentence[log->dsentence_pos] = 0;
}

char *vertex_dmsg_print(struct vertex_debug_log *log)
{
	log->dsentence_pos = 0;
	return log->dsentence;
}

void vertex_debug_memdump(const char *prefix, void *start, size_t size)
{
	char log[30];

	snprintf(log, sizeof(log), "[VIPx-VERTEX]%s ", prefix);
	print_hex_dump(KERN_DEBUG, log, DUMP_PREFIX_OFFSET, 32, 4, start,
			size, false);
}

static int vertex_debug_log_open(struct inode *inode, struct file *file)
{
	vertex_enter();
	if (inode->i_private)
		file->private_data = inode->i_private;
	vertex_leave();
	return 0;
}

static ssize_t vertex_debug_log_read(struct file *file, char __user *user_buf,
		size_t buf_len, loff_t *ppos)
{
	int ret;
	size_t size;
	struct vertex_debug *debug;
	struct vertex_system *system;

	vertex_enter();
	debug = file->private_data;
	if (!debug) {
		vertex_err("Cannot find vertex_debug\n");
		return 0;
	}

	system = debug->system;
	if (!system) {
		vertex_err("Cannot find vertex_system\n");
		return 0;
	}

	if (buf_len > VERTEX_DEBUG_SIZE)
		size = VERTEX_DEBUG_SIZE;
	else
		size = buf_len;

	if (!system->memory.debug.kvaddr) {
		vertex_err("Cannot find debug region for vertex\n");
		return 0;
	}

	ret = copy_to_user(user_buf, system->memory.debug.kvaddr, size);
	if (ret)
		memcpy(user_buf, system->memory.debug.kvaddr, size);

	vertex_leave();
	return size;
}

static int __vertex_debug_grp_show(struct seq_file *s, void *unused)
{
	unsigned int idx;
	struct vertex_debug *debug;
	struct vertex_graphmgr *gmgr;
	struct vertex_graph *graph;

	vertex_enter();
	debug = s->private;

	seq_printf(s, "%7.s %7.s %7.s %7.s %7.s %7.s %7.s\n", "graph", "prio",
			"period", "input", "done", "cancel", "recent");

	gmgr = &debug->system->graphmgr;
	mutex_lock(&gmgr->mlock);
	for (idx = 0; idx < VERTEX_MAX_GRAPH; ++idx) {
		graph = gmgr->graph[idx];

		if (!graph)
			continue;

		seq_printf(s, "%2d(%3d) %7d %7d %7d %7d %7d\n",
				graph->idx, graph->uid, graph->priority,
				graph->input_cnt, graph->done_cnt,
				graph->cancel_cnt, graph->recent);
	}
	mutex_unlock(&gmgr->mlock);

	vertex_leave();
	return 0;
}

static int vertex_debug_grp_open(struct inode *inode, struct file *file)
{
	vertex_check();
	return single_open(file, __vertex_debug_grp_show, inode->i_private);
}

static int __vertex_debug_buf_show(struct seq_file *s, void *unused)
{
	vertex_enter();
	vertex_leave();
	return 0;
}

static int vertex_debug_buf_open(struct inode *inode, struct file *file)
{
	vertex_enter();
	vertex_leave();
	return single_open(file, __vertex_debug_buf_show, inode->i_private);
}

static int vertex_debug_img_open(struct inode *inode, struct file *file)
{
	vertex_enter();
	if (inode->i_private)
		file->private_data = inode->i_private;
	vertex_leave();
	return 0;
}

static ssize_t vertex_debug_img_read(struct file *file, char __user *user_buf,
		size_t len, loff_t *ppos)
{
	int ret;
	size_t size;
	struct vertex_debug *debug;
	struct vertex_debug_imgdump *imgdump;

	vertex_enter();
	debug = file->private_data;
	imgdump = &debug->imgdump;

	if (!imgdump->kvaddr) {
		vertex_err("kvaddr of imgdump for debugging is NULL\n");
		return 0;
	}

	/* TODO check */
	//if (!imgdump->cookie) {
	//	vertex_err("cookie is NULL\n");
	//	return 0;
	//}

	if (len <= imgdump->length)
		size = len;
	else
		size = imgdump->length;

	if (!size) {
		imgdump->cookie = NULL;
		imgdump->kvaddr = NULL;
		imgdump->length = 0;
		imgdump->offset = 0;
		return 0;
	}

	/* HACK for test */
	memset(imgdump->kvaddr, 0x88, size / 2);
	/* TODO check */
	//vb2_ion_sync_for_device(imgdump->cookie, imgdump->offset,
	//		size, DMA_FROM_DEVICE);
	ret = copy_to_user(user_buf, imgdump->kvaddr, size);
	if (ret)
		memcpy(user_buf, imgdump->kvaddr, size);

	imgdump->offset += size;
	imgdump->length -= size;
	imgdump->kvaddr = (char *)imgdump->kvaddr + size;

	vertex_leave();
	return size;
}

static ssize_t vertex_debug_img_write(struct file *file,
		const char __user *user_buf, size_t len, loff_t *ppos)
{
	vertex_enter();
	vertex_leave();
	return len;
}

static const struct file_operations vertex_debug_log_fops = {
	.open	= vertex_debug_log_open,
	.read	= vertex_debug_log_read,
	.llseek	= default_llseek
};

static const struct file_operations vertex_debug_grp_fops = {
	.open	= vertex_debug_grp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static const struct file_operations vertex_debug_buf_fops = {
	.open	= vertex_debug_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static const struct file_operations vertex_debug_img_fops = {
	.open	= vertex_debug_img_open,
	.read	= vertex_debug_img_read,
	.write	= vertex_debug_img_write,
	.llseek	= default_llseek
};

static void __vertex_debug_monitor_fn(unsigned long data)
{
	struct vertex_debug *debug;
	struct vertex_debug_monitor *monitor;
	struct vertex_graphmgr *gmgr;
	struct vertex_system *system;
	struct vertex_interface *itf;

	vertex_enter();
	debug = (struct vertex_debug *)data;
	monitor = &debug->monitor;
	system = debug->system;
	gmgr = &system->graphmgr;
	itf = &system->interface;

	if (!test_bit(VERTEX_DEBUG_STATE_START, &debug->state))
		return;

	if (monitor->tick_cnt == gmgr->tick_cnt)
		vertex_err("timer thread is stuck (%u,%u)\n",
				monitor->tick_cnt, gmgr->tick_pos);

	if (monitor->sched_cnt == gmgr->sched_cnt) {
		struct vertex_graph *graph;
		unsigned int idx;

		vertex_dbg("[DEBUGFS] GRAPH\n");
		for (idx = 0; idx < VERTEX_MAX_GRAPH; idx++) {
			graph = gmgr->graph[idx];
			if (!graph)
				continue;

			vertex_graph_print(graph);
		}

		vertex_dbg("[DEBUGFS] GRAPH-MGR\n");
		vertex_taskdesc_print(gmgr);

		vertex_dbg("[DEBUGFS] INTERFACE-MGR\n");
		vertex_task_print_all(&itf->taskmgr);
	}

	monitor->tick_cnt = gmgr->tick_cnt;
	monitor->sched_cnt = gmgr->sched_cnt;
	monitor->done_cnt = itf->done_cnt;

	monitor->time_cnt++;
	mod_timer(&monitor->timer, jiffies + DEBUG_MONITORING_PERIOD);
	vertex_leave();
}

static int __vertex_debug_stop(struct vertex_debug *debug)
{
	struct vertex_debug_monitor *monitor;

	vertex_enter();
	monitor = &debug->monitor;

	if (!test_bit(VERTEX_DEBUG_STATE_START, &debug->state))
		goto p_end;

	/* TODO check debug */
	//del_timer_sync(&monitor->timer);
	clear_bit(VERTEX_DEBUG_STATE_START, &debug->state);

	vertex_leave();
p_end:
	return 0;
}

int vertex_debug_start(struct vertex_debug *debug)
{
	struct vertex_system *system = debug->system;
	struct vertex_graphmgr *gmgr = &system->graphmgr;
	struct vertex_debug_monitor *monitor = &debug->monitor;
	struct vertex_interface *itf = &system->interface;

	vertex_enter();
	monitor->time_cnt = 0;
	monitor->tick_cnt = gmgr->tick_cnt;
	monitor->sched_cnt = gmgr->sched_cnt;
	monitor->done_cnt = itf->done_cnt;

	init_timer(&monitor->timer);
	monitor->timer.expires = jiffies + DEBUG_MONITORING_PERIOD;
	monitor->timer.data = (unsigned long)debug;
	monitor->timer.function = __vertex_debug_monitor_fn;
	/* TODO check debug */
	//add_timer(&monitor->timer);

	set_bit(VERTEX_DEBUG_STATE_START, &debug->state);

	vertex_leave();
	return 0;
}

int vertex_debug_stop(struct vertex_debug *debug)
{
	vertex_check();
	return __vertex_debug_stop(debug);
}

int vertex_debug_open(struct vertex_debug *debug)
{
	vertex_enter();
	vertex_leave();
	return 0;
}

int vertex_debug_close(struct vertex_debug *debug)
{
	vertex_check();
	return  __vertex_debug_stop(debug);
}

int vertex_debug_probe(struct vertex_device *device)
{
	struct vertex_debug *debug;

	vertex_enter();
	debug_device = device;
	debug = &device->debug;
	debug->system = &device->system;
	debug->state = 0;

	debug->root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (!debug->root) {
		vertex_err("Filed to create debug file[%s]\n",
				DEBUG_FS_ROOT_NAME);
		goto p_end;
	}

	debug->logfile = debugfs_create_file(DEBUG_FS_LOGFILE_NAME, 0400,
			debug->root, debug, &vertex_debug_log_fops);
	if (!debug->logfile)
		vertex_err("Filed to create debug file[%s]\n",
				DEBUG_FS_LOGFILE_NAME);

	debug->grpfile = debugfs_create_file(DEBUG_FS_GRPFILE_NAME, 0400,
			debug->root, debug, &vertex_debug_grp_fops);
	if (!debug->grpfile)
		vertex_err("Filed to create debug file[%s]\n",
				DEBUG_FS_GRPFILE_NAME);

	debug->buffile = debugfs_create_file(DEBUG_FS_BUFFILE_NAME, 0400,
			debug->root, debug, &vertex_debug_buf_fops);
	if (!debug->buffile)
		vertex_err("Filed to create debug file[%s]\n",
				DEBUG_FS_BUFFILE_NAME);

	debug->imgdump.file = debugfs_create_file(DEBUG_FS_IMGFILE_NAME, 0400,
			debug->root, debug, &vertex_debug_img_fops);
	if (!debug->imgdump.file)
		vertex_err("Filed to create debug file[%s]\n",
				DEBUG_FS_IMGFILE_NAME);

	vertex_leave();
p_end:
	return 0;
}

void vertex_debug_remove(struct vertex_debug *debug)
{
	debugfs_remove_recursive(debug->root);
}
