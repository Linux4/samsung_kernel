/*
 * common function for clock framework source file
 *
 * Copyright (C) 2012 Marvell
 * Chao Xie <xiechao.mail@gmail.com>
 * Zhoujie Wu <zjwu@marvell.com>
 * Lu Cao <lucao@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk/mmpdcstat.h>

#include <linux/debugfs-pxa.h>
#include <linux/gpio.h>
#include <linux/miscled-rgb.h>
#include <linux/slab.h>

enum vlstat_mode {
	ACTIVE = 0,
	/* support max 6 LPM now, could be extended */
	LPM0,
	LPM1,
	/* support different vl from LPM2 */
	LPM2,
	LPM3,
	LPM4,
	LPM5,
	MODE_MAX,
};

struct vol_stat_info {
	u64 time;	/* ms */
	u32 vol;	/* mV */
};

struct vol_ts_info {
	bool stat_start;
	struct vol_stat_info *vlts;
	u32 vlnum;		/* how many voltage support in this board */
	u32 *vc_count;	/* [from] * [to] */
	u32 vc_total_count;
	u32 cur_lvl;	/* cur voltage vl */
	u32 mode_curlvl[MODE_MAX];	/* specific vl for different mode */

	ktime_t breakdown_start;
	ktime_t prev_ts;
	u64 total_time;	/* ms */
};

static DEFINE_SPINLOCK(vol_lock);
static struct vol_ts_info vol_dcstat;
static bool vol_tsinfo_inited;

static int vol_ledstatus_start;

static void vol_ledstatus_show(u32 mode)
{
	u32 hwlvl;

	hwlvl = vol_dcstat.mode_curlvl[mode];
	led_rgb_output(LED_R, hwlvl & 0x1);
	led_rgb_output(LED_G, hwlvl & 0x2);
	led_rgb_output(LED_B, hwlvl & 0x4);
}

void vol_ledstatus_event(u32 lpm)
{
	spin_lock(&vol_lock);
	if (!vol_ledstatus_start)
		goto out;
	vol_ledstatus_show(lpm);
out:
	spin_unlock(&vol_lock);
}

static ssize_t vol_led_read(struct file *filp, char __user *buffer,
			   size_t count, loff_t *ppos)
{
	char *buf;
	ssize_t ret, size = 2 * PAGE_SIZE - 1;
	u32 len = 0;

	buf = (char *)__get_free_pages(GFP_NOIO, get_order(size));
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, size - len,
			"Help information :\n");
	len += snprintf(buf + len, size - len,
			"echo 1 to start voltage led status:\n");
	len += snprintf(buf + len, size - len,
			"echo 0 to stop voltage led status:\n");
	len += snprintf(buf + len, size - len,
			"VLevel3: R, G\n");
	len += snprintf(buf + len, size - len,
			"VLevel2: R\n");
	len += snprintf(buf + len, size - len,
			"VLevel1: G\n");
	len += snprintf(buf + len, size - len,
			"VLevel0: B\n");
	len += snprintf(buf + len, size - len,
			"    lpm: off\n");

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	free_pages((unsigned long)buf, get_order(size));
	return ret;
}

static ssize_t vol_led_write(struct file *filp, const char __user *buffer,
			    size_t count, loff_t *ppos)
{
	unsigned int start;
	char buf[10] = { 0 };

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	if (sscanf(buf, "%d", &start) != 1)
		return -EFAULT;
	start = !!start;

	if (vol_ledstatus_start == start) {
		pr_err("[WARNING]Voltage led status is already %s\n",
		       vol_dcstat.stat_start ? "started" : "stopped");
		return -EINVAL;
	}

	spin_lock(&vol_lock);
	vol_ledstatus_start = start;
	if (vol_ledstatus_start)
		vol_ledstatus_show(MAX_LPM_INDEX);
	else {
		led_rgb_output(LED_R, 0);
		led_rgb_output(LED_G, 0);
		led_rgb_output(LED_B, 0);
	}
	spin_unlock(&vol_lock);

	return count;
}

static const struct file_operations vol_led_ops = {
	.owner = THIS_MODULE,
	.read = vol_led_read,
	.write = vol_led_write,
};

static void vol_dcstat_update(u32 mode)
{
	ktime_t cur_ts;
	u32 hwlvl;
	u64 time_us;

	if (!vol_dcstat.stat_start)
		return;

	hwlvl = vol_dcstat.mode_curlvl[mode];
	if (vol_dcstat.cur_lvl == hwlvl)
		return;

	/* update voltage change times */
	vol_dcstat.vc_count[vol_dcstat.cur_lvl * vol_dcstat.vlnum + hwlvl]++;
	vol_dcstat.vc_total_count++;

	/* update voltage dc statistics */
	cur_ts = ktime_get();
	time_us = ktime_to_us(ktime_sub(cur_ts, vol_dcstat.prev_ts));
	vol_dcstat.vlts[vol_dcstat.cur_lvl].time += time_us;
	vol_dcstat.prev_ts = cur_ts;
	vol_dcstat.cur_lvl = hwlvl;
}

void vol_dcstat_event(enum vlstat_msg msg, u32 midx, u32 vl)
{
	spin_lock(&vol_lock);
	switch (msg) {
	case VLSTAT_LPM_ENTRY:
		vol_dcstat_update(LPM0 + midx);
		break;
	case VLSTAT_LPM_EXIT:
		vol_dcstat_update(ACTIVE);
		break;
	case VLSTAT_VOL_CHG:
		if (midx == ACTIVE) {
			vol_dcstat.mode_curlvl[midx] = vl;
			vol_dcstat.mode_curlvl[LPM0] = vl;
			vol_dcstat.mode_curlvl[LPM1] = vl;
			vol_dcstat_update(ACTIVE);
		} else
			vol_dcstat.mode_curlvl[LPM0 + midx] = vl;
		break;
	default:
		pr_err("%s invaid event %u\n", __func__, msg);
		break;
	}
	spin_unlock(&vol_lock);
}

int register_vldcstatinfo(int *vol, u32 vlnum)
{
	u32 idx;

	vol_dcstat.vlts = kzalloc(vlnum *
		sizeof(struct vol_stat_info), GFP_KERNEL);
	if (!vol_dcstat.vlts) {
		pr_err("%s vlts info malloc failed!\n", __func__);
		return -ENOMEM;
	}

	vol_dcstat.vc_count = kzalloc(vlnum * vlnum * sizeof(u32), GFP_KERNEL);
	if (!vol_dcstat.vc_count) {
		pr_err("%s vc_count info malloc failed!\n", __func__);
		kfree(vol_dcstat.vlts);
		return -ENOMEM;
	}
	vol_dcstat.vlnum = vlnum;
	for (idx = 0; idx < vlnum; idx++)
		vol_dcstat.vlts[idx].vol = vol[idx];

	vol_tsinfo_inited = true;
	return 0;
}

static ssize_t vol_dc_read(struct file *filp, char __user *buffer,
			   size_t count, loff_t *ppos)
{
	char *buf;
	ssize_t ret, size = 2 * PAGE_SIZE - 1;
	u32 i, j, dc_int = 0, dc_fra = 0, len = 0, vlnum = vol_dcstat.vlnum;

	buf = (char *)__get_free_pages(GFP_NOIO, get_order(size));
	if (!buf)
		return -ENOMEM;

	if (!vol_dcstat.total_time) {
		len += snprintf(buf + len, size - len,
				"No stat information! ");
		len += snprintf(buf + len, size - len,
				"Help information :\n");
		len += snprintf(buf + len, size - len,
				"1. echo 1 to start duty cycle stat:\n");
		len += snprintf(buf + len, size - len,
				"2. echo 0 to stop duty cycle stat:\n");
		len += snprintf(buf + len, size - len,
				"3. cat to check duty cycle info from start to stop:\n\n");
		goto out;
	}

	if (vol_dcstat.stat_start) {
		len += snprintf(buf + len, size - len,
				"Please stop the vol duty cycle stats at first\n");
		goto out;
	}

	len += snprintf(buf + len, size - len,
			"Total time:%8llums (%6llus)\n",
			div64_u64(vol_dcstat.total_time, (u64)(1000)),
			div64_u64(vol_dcstat.total_time, (u64)(1000000)));
	len += snprintf(buf + len, size - len,
			"|Level|Vol(mV)|Time(ms)|      %%|\n");
	for (i = 0; i < vlnum; i++) {
		dc_int = calculate_dc(vol_dcstat.vlts[i].time,
				vol_dcstat.total_time, &dc_fra);
		len += snprintf(buf + len, size - len,
				"| VL_%1d|%7u|%8llu|%3u.%02u%%|\n",
				i, vol_dcstat.vlts[i].vol,
				div64_u64(vol_dcstat.vlts[i].time,
					(u64)(1000)),
				dc_int, dc_fra);
	}

	/* show voltage-change times */
	len += snprintf(buf + len, size - len,
			"\nTotal voltage-change times:%8u",
			vol_dcstat.vc_total_count);
	len += snprintf(buf + len, size - len, "\n|from\\to|");
	for (j = 0; j < vlnum; j++)
		len += snprintf(buf + len, size - len, " Level%1d|", j);
	for (i = 0; i < vlnum; i++) {
		len += snprintf(buf + len, size - len, "\n| Level%1d|", i);
		for (j = 0; j < vlnum; j++)
			if (i == j)
				len += snprintf(buf + len, size - len,
						"  ---  |");
			else
				/* [from][to] */
				len += snprintf(buf + len, size - len, "%7u|",
					vol_dcstat.vc_count[i * vlnum + j]);
	}
	len += snprintf(buf + len, size - len, "\n");
out:
	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	free_pages((unsigned long)buf, get_order(size));
	return ret;
}

static ssize_t vol_dc_write(struct file *filp, const char __user *buffer,
			    size_t count, loff_t *ppos)
{
	unsigned int start, idx, vlnum = vol_dcstat.vlnum;
	char buf[10] = { 0 };
	ktime_t cur_ts;
	u64 time_us;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	if (sscanf(buf, "%d", &start) != 1)
		return -EFAULT;
	start = !!start;

	if (vol_dcstat.stat_start == start) {
		pr_err("[WARNING]Voltage duty-cycle statistics is already %s\n",
		       vol_dcstat.stat_start ? "started" : "stopped");
		return -EINVAL;
	}
	vol_dcstat.stat_start = start;

	spin_lock(&vol_lock);
	cur_ts = ktime_get();
	if (vol_dcstat.stat_start) {
		for (idx = 0; idx < vlnum; idx++)
			vol_dcstat.vlts[idx].time = 0;
		memset(vol_dcstat.vc_count, 0, sizeof(u32) * vlnum * vlnum);
		vol_dcstat.prev_ts = cur_ts;
		vol_dcstat.breakdown_start = cur_ts;
		vol_dcstat.cur_lvl = vol_dcstat.mode_curlvl[ACTIVE];
		vol_dcstat.total_time = -1UL;
		vol_dcstat.vc_total_count = 0;
	} else {
		time_us = ktime_to_us(ktime_sub(cur_ts, vol_dcstat.prev_ts));
		vol_dcstat.vlts[vol_dcstat.cur_lvl].time += time_us;
		vol_dcstat.total_time = ktime_to_us(ktime_sub(cur_ts,
					vol_dcstat.breakdown_start));
	}
	spin_unlock(&vol_lock);
	return count;
}

static const struct file_operations vol_dc_ops = {
	.owner = THIS_MODULE,
	.read = vol_dc_read,
	.write = vol_dc_write,
};

static int __init hwdvc_stat_init(void)
{
	struct dentry *vlstat_node, *volt_dc_stat, *volt_led;

	if (!vol_tsinfo_inited)
		return -ENOENT;

	vlstat_node = debugfs_create_dir("vlstat", pxa);
	if (!vlstat_node)
		return -ENOENT;

	volt_dc_stat = debugfs_create_file("vol_dc_stat", 0444,
		vlstat_node, NULL, &vol_dc_ops);
	if (!volt_dc_stat)
		goto err_1;

	volt_led = debugfs_create_file("vol_led", 0644,
		vlstat_node, NULL, &vol_led_ops);
	if (!volt_led)
		goto err_volt_led;

	return 0;

err_volt_led:
	debugfs_remove(volt_dc_stat);
err_1:
	debugfs_remove(vlstat_node);
	return -ENOENT;
}
late_initcall_sync(hwdvc_stat_init);
