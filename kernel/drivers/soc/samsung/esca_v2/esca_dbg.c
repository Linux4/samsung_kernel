/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esca_drv.h"

struct task_info *task_info[NR_ESCA_LAYERS];
bool *task_profile_running[NR_ESCA_LAYERS];

struct tstamp_info *tstamp_info;

struct workqueue_struct *update_log_wq;
bool is_esca_stop_log = false;
static bool is_esca_ramdump = false;
bool esca_stop_log_req = false;
static int timer_tick_max, timer_period;
u32 ESCA_TIMER_LAYER = -1;
u32 nr_phy_layer = -1;
/******************************************************************************
  esca
 *****************************************************************************/
u32 get_esca_phy_num(void)
{
	int i, count = 0;

	/* phy_layer counting done. */
	if (nr_phy_layer != -1)
		return nr_phy_layer;

	for (i = ESCA_LAYER__PHY0; i<= ESCA_LAYER__PHY1; i++) {
		if (exynos_esca[i])
			count++;
	}
	nr_phy_layer = count;

	return nr_phy_layer;
}
EXPORT_SYMBOL_GPL(get_esca_phy_num);
/******************************************************************************
  Timer
 *****************************************************************************/
ktime_t esca_time_calc(u32 start, u32 end)
{
	u32 interval;

	if (start > end)
		interval = timer_tick_max - start + end;
	else
		interval = end - start;

	return interval * timer_period;
}
EXPORT_SYMBOL_GPL(esca_time_calc);

u32 esca_get_gptimer(void)
{
	return exynos_get_gptimer_cur_tick();
}
EXPORT_SYMBOL_GPL(esca_get_gptimer);
/******************************************************************************
  Dump 
 *****************************************************************************/
static void dump_init(struct esca_info *esca)
{
	const __be32 *prop;
	struct device_node *np = esca->dev->of_node, *_np;
	u32 i = 0, cnt, len;
	struct esca_debug_info *esca_debug = &esca->dbg;
	struct device *dev = esca->dev;

	np = of_get_child_by_name(np, "dump");
	if (!np) {
		dev_err(dev, "dump info is missing.\n");
		return ;
	}

	cnt = of_get_child_count(np);
	if (!cnt) {
		dev_err(dev, "dump child info is missing.\n");
		return ;
	}

	esca_debug->num_dumps = cnt;
	esca_debug->sram_dumps =
		devm_kzalloc(dev, sizeof(void __iomem *) * cnt, GFP_KERNEL);
	esca_debug->dram_dumps =
		devm_kzalloc(dev, sizeof(void __iomem *) * cnt, GFP_KERNEL);
	esca_debug->dump_sizes =
		devm_kzalloc(dev, sizeof(unsigned int) * cnt, GFP_KERNEL);

	for_each_child_of_node(np, _np) {
		u32 addr, size;

		prop = of_get_property(_np, "size", &len);
		if (prop)
			esca_debug->dump_sizes[i] = be32_to_cpup(prop);

		if (!esca_debug->dump_sizes[i])
			continue;

		size = esca_debug->dump_sizes[i];

		prop = of_get_property(_np, "dump-base", &len);
		if (prop) {
			addr = be32_to_cpup(prop);
			dbg_snapshot_add_bl_item_info(_np->name, addr, size);
		}

		prop = of_get_property(_np, "sram-base", &len);
		if (prop) {
			addr = be32_to_cpup(prop);
			esca_debug->sram_dumps[i] = ioremap(addr, size);
			esca_debug->dram_dumps[i] = kzalloc(size, GFP_KERNEL);

			dbg_snapshot_add_bl_item_info(_np->name,
					virt_to_phys(esca_debug->dram_dumps[i]),
					size);
		}

		i++;
	}
}

void __iomem *get_esca_dump_addr(u32 layer)
{
	int i;
	struct esca_debug_info *esca_debug = &exynos_esca[layer]->dbg;

	for (i = 0 ; i < esca_debug->num_dumps; i++) {
		if (esca_debug->sram_dumps[i])
			return esca_debug->sram_dumps[i];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(get_esca_dump_addr);

unsigned int get_esca_dump_size(u32 layer)
{
	int i;
	struct esca_debug_info *esca_debug = &exynos_esca[layer]->dbg;

	for (i = 0 ; i < esca_debug->num_dumps; i++) {
		if (esca_debug->sram_dumps[i])
			return esca_debug->dump_sizes[i];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(get_esca_dump_size);
/******************************************************************************
  Log
 *****************************************************************************/
void esca_ktime_sram_sync(void)
{
	unsigned long long cur_clk;
	unsigned long flags;
	unsigned int cur_tick;
	struct esca_debug_info *esca_debug = &exynos_esca[ESCA_TIMER_LAYER]->dbg;

	spin_lock_irqsave(&esca_debug->lock, flags);

	cur_clk = sched_clock();
	cur_tick = esca_get_gptimer();

	tstamp_info->ktime_base = cur_clk;
	tstamp_info->esca_time_base = cur_tick;

	spin_unlock_irqrestore(&esca_debug->lock, flags);
}

static void esca_update_log(struct work_struct *work)
{
	esca_log_print();
}

void esca_stop_log(void)
{
	esca_stop_log_req = true;
	esca_log_print();
}
EXPORT_SYMBOL_GPL(esca_stop_log);

static unsigned long long esca_tick_to_ktime(unsigned long long count)
{
	unsigned long long ktime_base, esca_timer_base;

	esca_timer_base = tstamp_info->esca_time_base;
	ktime_base = tstamp_info->ktime_base;

	if (count > esca_timer_base)
		return ((count - esca_timer_base) * timer_period) + ktime_base;
	else
		return ktime_base - ((esca_timer_base - count) * timer_period);
}

void esca_ramdump(void)
{
	unsigned int rear = 0;
	unsigned char str[9] = {0,};
	unsigned int val;
	unsigned int time_tick;
	unsigned long long time;
	struct esca_debug_info *esca_debug = &exynos_esca[ESCA_TIMER_LAYER]->dbg;

	if (!is_esca_ramdump)
		is_esca_ramdump = true;
	else
		return;

	do {
		time_tick = __raw_readl(esca_debug->log_buff_base + esca_debug->log_buff_size * rear);

		/* string length: log_buff_size - header(4) - integer_data(4) */
		memcpy_align_4(str, esca_debug->log_buff_base + (esca_debug->log_buff_size * rear) + 4,
				esca_debug->log_buff_size - 8);

		val = __raw_readl(esca_debug->log_buff_base + esca_debug->log_buff_size * rear +
				esca_debug->log_buff_size - 4);

		time = esca_tick_to_ktime(time_tick);
		dbg_snapshot_esca(time, str, val);

		if (esca_debug->log_buff_len == (rear + 1))
			rear = 0;
		else
			rear++;

	} while (rear != 0);

	for (int i = 0; i < esca_debug->num_dumps; i++) {
		void __iomem *src = esca_debug->sram_dumps[i];
		void __iomem *dst = esca_debug->dram_dumps[i];
		u32 size = esca_debug->dump_sizes[i];

		if (!src || !dst || !size)
			continue;

		memcpy(dst, src, size);
	}
}

void esca_log_idx_update(void)
{
	unsigned int front;
	unsigned int rear;
	struct esca_debug_info *esca_debug = &exynos_esca[ESCA_TIMER_LAYER]->dbg;

	if (esca_stop_log_req)
		return ;
	/* ESCA Log data dequeue & print */
	front = __raw_readl(esca_debug->log_buff_front);
	rear = __raw_readl(esca_debug->log_buff_rear);

	if (rear != front)
		__raw_writel(front, esca_debug->log_buff_rear);
}

void esca_log_print(void)
{
	unsigned int front;
	unsigned int rear;
	unsigned char str[9] = {0,};
	unsigned int val;
	unsigned int time_tick;
	unsigned long long time;
	struct esca_debug_info *esca_debug = &exynos_esca[ESCA_TIMER_LAYER]->dbg;

	if (is_esca_stop_log)
		return ;

	/* ESCA Log data dequeue & print */
	front = __raw_readl(esca_debug->log_buff_front);
	rear = __raw_readl(esca_debug->log_buff_rear);

	while (rear != front) {
		time_tick = __raw_readl(esca_debug->log_buff_base + esca_debug->log_buff_size * rear);

		/* string length: log_buff_size - header(4) - integer_data(4) */
		memcpy_align_4(str, esca_debug->log_buff_base + (esca_debug->log_buff_size * rear) + 4,
				esca_debug->log_buff_size - 8);

		val = __raw_readl(esca_debug->log_buff_base + esca_debug->log_buff_size * rear +
				esca_debug->log_buff_size - 4);

		time = esca_tick_to_ktime(time_tick);
		dbg_snapshot_esca(time, str, val);

		if (esca_debug->debug_log_level == 1)
			pr_info("[ACPM_FW] : time:%llu %s, %x ", time, str, val);

		if (esca_debug->log_buff_len == (rear + 1))
			rear = 0;
		else
			rear++;

		__raw_writel(rear, esca_debug->log_buff_rear);
	}

	if (esca_stop_log_req) {
		is_esca_stop_log = true;
		esca_ramdump();
	}
}

static void log_buffer_init(struct esca_info *esca)
{
	const __be32 *prop;
	unsigned int len = 0;
	struct device_node *np = esca->dev->of_node;
	struct esca_debug_info *esca_debug;

	np = of_get_child_by_name(np, "log");
	if (!np)
		return ;

	esca_debug = &esca->dbg;

	esca_debug->log_buff_rear = esca->sram_base + esca->initdata->log_buf_rear;
	esca_debug->log_buff_front = esca->sram_base + esca->initdata->log_buf_front;
	esca_debug->log_buff_base = esca->sram_base + esca->initdata->log_data;
	esca_debug->log_buff_len = esca->initdata->log_entry_len;
	esca_debug->log_buff_size = esca->initdata->log_entry_size;

	update_log_wq = alloc_workqueue("%s",
			__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS,
			1, "esca_update_log");
	INIT_WORK(&esca_debug->update_log_work, esca_update_log);

	prop = of_get_property(np, "debug-log-level", &len);
	if (prop)
		esca_debug->debug_log_level = be32_to_cpup(prop);

	spin_lock_init(&esca_debug->lock);

	esca_debug->ipc_info = &esca->ipc;
}

/******************************************************************************
  DebugFS Interface
 *****************************************************************************/
struct esca_dbgfs_info {
	int layer;
	struct dentry *den;
};
struct esca_dbgfs_info *esca_dbgfs_info;


static int debug_log_level_get(void *data, unsigned long long *val)
{
	return 0;
}

static int debug_log_level_set(void *data, unsigned long long val)
{
	struct esca_debug_info *esca_debug = &exynos_esca[ESCA_MBOX__PHY]->dbg;

	esca_debug->debug_log_level = (unsigned int)val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_log_level_fops,
		debug_log_level_get, debug_log_level_set, "%llu\n");

static int debug_ipc_loopback_test_get(void *data, unsigned long long *val)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };
	static unsigned long long ipc_time_start;
	static unsigned long long ipc_time_end;

	config.cmd = cmd;
	config.cmd[0] = (1 << ESCA_IPC_PROTOCOL_TEST);
	config.cmd[0] |= 0x3 << ESCA_IPC_PROTOCOL_ID;

	config.response = true;
	config.indirection = false;

	ipc_time_start = sched_clock();
	ret = esca_send_data(exynos_esca[ESCA_MBOX__PHY]->dev->of_node, 3, &config);
	ipc_time_end = sched_clock();

	if (!ret)
		*val = ipc_time_end - ipc_time_start;
	else
		*val = 0;

	config.cmd = NULL;

	return 0;
}

static int debug_ipc_loopback_test_set(void *data, unsigned long long val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ipc_loopback_test_fops,
		debug_ipc_loopback_test_get, debug_ipc_loopback_test_set, "%llu\n");

static bool is_running_task_profiler(u32 layer)
{
	return __raw_readl(task_profile_running[layer]);
}

static int get_task_num(u32 layer)
{
	return task_info[layer]->num_tasks;
}

static int get_task_profile_data(u32 layer, int task_id, int cpu_id)
{
	struct esca_task_addr *task_addr;
	struct esca_task_struct *tasks;

	if (task_id >= get_task_num(layer))
		return -EINVAL;

	task_addr = (struct esca_task_addr *)(exynos_esca[layer]->sram_base + task_info[layer]->tasks);

	tasks =  (struct esca_task_struct *)(exynos_esca[layer]->sram_base + task_addr->cpu[cpu_id] + (task_id * sizeof(struct esca_task_struct)));

	return tasks->profile.latency;
}

static int esca_enable_task_profiler(unsigned int layer, int start)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;

	if (start) {
		config.cmd[0] = (1 << ESCA_IPC_TASK_PROF_START);
		if (exynos_esca[layer]->num_cores > 1)
			config.cmd[0] |= ESCA_TASK_PROF_IPI;

		ret = esca_send_data(exynos_esca[layer]->dev->of_node, ESCA_IPC_TASK_PROF_START, &config);
	} else {
		config.cmd[0] = (1 << ESCA_IPC_TASK_PROF_END);
		if (exynos_esca[layer]->num_cores > 1)
			config.cmd[0] |= ESCA_TASK_PROF_IPI;

		ret = esca_send_data(exynos_esca[layer]->dev->of_node, ESCA_IPC_TASK_PROF_END, &config);
	}

	config.cmd = NULL;

	return 0;
}

static ssize_t task_profile_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	char buf[32];
	struct esca_dbgfs_info *d2f = file->private_data;

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	if (buf[0] == '0') esca_enable_task_profiler(d2f->layer, 0);
	if (buf[0] == '1') esca_enable_task_profiler(d2f->layer, 1);

	return ret;
}
static ssize_t task_profile_read(struct file *file, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *buf;
	int r, task_num, i;
	unsigned int *util, sum = 0;
	ssize_t size = 0;
	int cpu_id, num_cores;

	struct esca_dbgfs_info *d2f = file->private_data;

	buf = kzalloc(sizeof(char) * 2048, GFP_KERNEL);

	if (is_running_task_profiler(d2f->layer)){
		size += sprintf(buf + size, "!!task profiler running!!\n");
		size += sprintf(buf + size, "[stop task profiler]\n");
		size += sprintf(buf + size, "echo 0 > /d/esca_framework/task_profile\n");

		r = simple_read_from_buffer(ubuf, cnt, ppos, buf, (int)size);
		kfree(buf);
		return r;
	}

	num_cores = exynos_esca[d2f->layer]->num_cores;

	task_num = get_task_num(d2f->layer);

	util = kzalloc(sizeof(unsigned int) * task_num, GFP_KERNEL);

	for (cpu_id = 0; cpu_id < num_cores; cpu_id++) {
		size += sprintf(buf + size, "Core[%d] util infomation\n", cpu_id);
		sum = 0;
		for (i = 0; i < task_num; i++) {
			util[i] = get_task_profile_data(d2f->layer, i, cpu_id);
			sum += util[i];
		}

		for (i = 0; i < task_num; i++) {
			if (util[i] == 0)
				size += sprintf(buf + size, "TASK[%d] util : 0 %%\n", i);
			else
				size += sprintf(buf + size, "TASK[%d] util : %d.%02d %%\n", i,
					(util[i] * 100) / sum,
					DIV_ROUND_CLOSEST(((util[i] * 100) % sum) * 100, sum));
		}
	}

	r = simple_read_from_buffer(ubuf, cnt, ppos, buf, (int)size);
	kfree(buf);
	kfree(util);

	return r;
}

static const struct file_operations debug_task_profile_fops = {
	.open		= simple_open,
	.read		= task_profile_read,
	.write		= task_profile_write,
	.llseek		= default_llseek,
};


static void esca_debugfs_init(struct esca_info *esca)
{
	struct dentry *den;
	unsigned int layer;

	den = debugfs_lookup("esca", NULL);
	if (!den)
		den = debugfs_create_dir("esca", NULL);

	if (!esca_dbgfs_info)
		esca_dbgfs_info = kzalloc(sizeof(struct esca_dbgfs_info) * NR_ESCA_LAYERS, GFP_KERNEL);

	if (esca->layer >= NR_ESCA_LAYERS)
		return;

	layer = esca->layer;

	esca_dbgfs_info[layer].layer = layer;

	if (esca->layer == ESCA_LAYER__APP) {
		esca_dbgfs_info[layer].den = debugfs_create_dir("app_layer", den);
	} else if (layer == ESCA_LAYER__PHY0) {
		esca_dbgfs_info[layer].den = debugfs_create_dir("phy0_layer", den);
	} else if (layer == ESCA_LAYER__PHY1) {
		esca_dbgfs_info[layer].den = debugfs_create_dir("phy1_layer", den);
	} else
		return;

	debugfs_create_file("task_profile", 0644, esca_dbgfs_info[layer].den, &esca_dbgfs_info[layer], &debug_task_profile_fops);
	task_profile_running[layer] = &(esca->initdata->task_profile_running);
	task_info[layer] = (struct task_info*)(esca->sram_base + esca->initdata->task_info);

	if (layer == ESCA_MBOX__APP) {
		debugfs_create_file("ipc_loopback_test", 0644, esca_dbgfs_info[layer].den, esca, &debug_ipc_loopback_test_fops);
	} else if (layer == ESCA_MBOX__PHY) {
		debugfs_create_file("ipc_loopback_test", 0644, esca_dbgfs_info[layer].den, esca, &debug_ipc_loopback_test_fops);
		debugfs_create_file("log_level", 0644, esca_dbgfs_info[layer].den, NULL, &debug_log_level_fops);
	}
}

void esca_dbg_init(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const __be32 *prop;
	unsigned int len = 0, val;
	struct esca_info *esca;

	prop = of_get_property(np, "esca-layer", &len);
	val = be32_to_cpup(prop);
	esca = exynos_esca[val];
	
	esca_debugfs_init(esca);
	log_buffer_init(esca);
	dump_init(esca);

	np = of_get_child_by_name(np, "timer");
	if (!np)
		return ;

	ESCA_TIMER_LAYER = esca->layer;
	tstamp_info = (struct tstamp_info*)(esca->sram_base + esca->initdata->tstamp_info);
	of_property_read_u32(np, "timer_tick_max", &timer_tick_max);
	of_property_read_u32(np, "timer_period", &timer_period);

	esca_ktime_sram_sync();
}
