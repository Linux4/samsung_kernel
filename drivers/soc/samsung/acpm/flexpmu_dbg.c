/*
 * Exynos regulator support.
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <soc/samsung/acpm_ipc_ctrl.h>

#include "acpm.h"
#include "fw_header/flexpmu_dbg.h"

struct flexpmu_dbg *flxp_dbg;
struct flexpmu_cpu_pm *flxp_cpu_pm;
struct flexpmu_mif *flxp_mif;
struct flexpmu_ext_clk_buf *flxp_eclkbuf;
struct mif_req_info *mif_req_info;
static u32 nfc_clkreq_idx;
static int mif_master_num;

u32 *apsoc_down_cnt;
u32 *apsoc_ewkup_cnt;
u32 *mif_down_cnt;

static struct latency_info *latency_cpu;
static struct latency_info *latency_cluster;
static struct latency_info *latency_pd;
static struct latency_info *latency_soc;
static struct latency_info *latency_mif;
static struct requester_info *requester_mif;

#define EXYNOS_FLEXPMU_DBG_PREFIX	"EXYNOS-FLEXPMU-DBG: "

#define BUF_MAX_LINE   10
#define BUF_LINE_SIZE  1023
#define BUF_SIZE       (BUF_MAX_LINE * BUF_LINE_SIZE)

/* enum for debugfs files */
enum flexpmu_debugfs_id {
	FID_CPU_STATUS,
	FID_LATENCY_PROFILER,
	FID_SEQ_STATUS,
	FID_CUR_SEQ,
	FID_SW_FLAG,
	FID_SEQ_COUNT,
	FID_MIF_ALWAYS_ON,
	FID_LPM_COUNT,
	FID_REQUESTER_PROFILER,
	FID_MID_DVS_EN,
	FID_MAX
};

char *flexpmu_debugfs_name[FID_MAX] = {
	"cpu_status",
	"latency_profiler",
	"seq_status",
	"cur_sequence",
	"sw_flag",
	"seq_count",
	"mif_always_on",
	"lpm_count",
	"requester_profiler",
	"mif_dvs_en",
};

struct dbgfs_info {
	int fid;
	struct dentry *den;
	struct file_operations fops;
};

struct dbgfs_info *flexpmu_dbg_info;
void __iomem *flexpmu_dbg_base;
static struct dentry *flexpmu_dbg_root;
struct device_node *flexpmu_dbg_np;

const char **mif_requester_names;

struct flexpmu_apm_req_info {
	unsigned int active_req_tick;
	unsigned int last_rel_tick;
	unsigned int total_count;
	unsigned int total_time_tick;
	unsigned long long int active_since_us;
	unsigned long long int last_rel_us;
	unsigned long long int total_time_us;
};

struct rtc_info {
	void __iomem *base;
	unsigned int offset;
	unsigned int max;
	unsigned int tick2us;
};
static struct rtc_info rtc;

int acpm_get_nfc_log_buf(struct nfc_clk_req_log **buf, u32 *last_ptr, u32 *len)
{
	struct ext_clk_log_buf *eclk_logbuf;
	struct ext_clk_log *nfc_logs;

	if (!flxp_eclkbuf)
		return -ENOENT;

	eclk_logbuf = (struct ext_clk_log_buf *)(flexpmu_dbg_base + flxp_eclkbuf->p_log_bufs);
	if (!eclk_logbuf)
		return -ENOENT;

	if (nfc_clkreq_idx >= flxp_eclkbuf->nr_reqs)
		return -EINVAL;

	nfc_logs = (struct ext_clk_log *)(flexpmu_dbg_base + eclk_logbuf[nfc_clkreq_idx].p_logs);
	if (!nfc_logs || !eclk_logbuf[nfc_clkreq_idx].len)
		return -ENOENT;

	*last_ptr = eclk_logbuf[nfc_clkreq_idx].ptr;
	*len = eclk_logbuf[nfc_clkreq_idx].len;
	*buf = (struct nfc_clk_req_log *)nfc_logs;

	return 0;
}

u32 acpm_get_mifdn_count(void)
{
	return mif_down_cnt[4];
}
EXPORT_SYMBOL_GPL(acpm_get_mifdn_count);

u32 acpm_get_apsocdn_count(void)
{
	return apsoc_down_cnt[4];
}
EXPORT_SYMBOL_GPL(acpm_get_apsocdn_count);

u32 acpm_get_early_wakeup_count(void)
{
	return apsoc_ewkup_cnt[4];
}
EXPORT_SYMBOL_GPL(acpm_get_early_wakeup_count);

/*notify to flexpmu, it is SICD w MIF(is_dsu_cpd=0) or SICD wo MIF(is_dsu_cpd=1).*/
u32 acpm_noti_dsu_cpd(bool is_dsu_cpd)
{
	flxp_cpu_pm->mif_always_on = is_dsu_cpd;
	return 0;
}
EXPORT_SYMBOL_GPL(acpm_noti_dsu_cpd);

u32 acpm_get_dsu_cpd(void)
{
	return flxp_cpu_pm->mif_always_on;
}
EXPORT_SYMBOL_GPL(acpm_get_dsu_cpd);

u32 acpm_get_mif_request(void)
{
	return flxp_mif->requests;
}
EXPORT_SYMBOL_GPL(acpm_get_mif_request);

void acpm_print_mif_request(void)
{
	int i = 0;

	for (i = 0; i < mif_master_num - 1; i++) {
		if (flxp_mif->requests & (1 << mif_req_info[i].up))
			pr_info("%s: MIF blocker is %s\n", EXYNOS_FLEXPMU_DBG_PREFIX, mif_requester_names[i]);
	}
}
EXPORT_SYMBOL_GPL(acpm_print_mif_request);

static ssize_t flexpmu_dbg_cpu_status_read(int fid, char *buf)
{
	ssize_t ret = 0;
	return ret;
}

static ssize_t flexpmu_dbg_latency_profiler_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int i = 0;
	int avg[2];

	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %7s %10s %10s %5s %10s %10s\n",
			"CPU", "", "avg", "total_cnt", "", "avg", "total_cnt");
	for (i = 0 ; i < flxp_dbg->cpu_list_size; i++) {
		avg[0] = latency_cpu[i].sum[0]/latency_cpu[i].cnt;
		avg[1] = latency_cpu[i].sum[1]/latency_cpu[i].cnt;
		ret += snprintf(buf + ret, BUF_SIZE - ret,
				"cpu[%d]: off: %10d %10d   on: %10d %10d\n",
				i, avg[0], latency_cpu[i].cnt,
				avg[1], latency_cpu[i].cnt);
	}

	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"\n%s: %7s %10s %10s %5s %10s %10s\n",
			"CLUSTER", "", "avg", "total_cnt", "", "avg", "total_cnt");
	for (i = 0 ; i < flxp_dbg->cluster_list_size; i++) {
		avg[0] = latency_cluster[i].sum[0]/latency_cluster[i].cnt;
		avg[1] = latency_cluster[i].sum[1]/latency_cluster[i].cnt;
		ret += snprintf(buf + ret, BUF_SIZE - ret,
				"cluster[%d]: off: %10d %10d   on: %10d %10d\n",
				i, avg[0], latency_cluster[i].cnt,
				avg[1], latency_cluster[i].cnt);
	}

	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"\n%s: %7s %10s %10s %5s %10s %10s\n",
			"PD", "", "avg", "total_cnt", "", "avg", "total_cnt");
	for (i = 0 ; i < flxp_dbg->pd_list_size; i++) {
		avg[0] = latency_pd[i].sum[0]/latency_pd[i].cnt;
		avg[1] = latency_pd[i].sum[1]/latency_pd[i].cnt;
		ret += snprintf(buf + ret, BUF_SIZE - ret,
				"pd[%2d]: off: %10d %10d   on: %10d %10d\n",
				i, avg[0], latency_pd[i].cnt,
				avg[1], latency_pd[i].cnt);
	}

	ret += snprintf(buf + ret, BUF_SIZE -ret,
			"\n%s: %7s %10s %10s %5s %10s %10s\n",
			"SYSTEM", "", "avg", "total_cnt", "", "avg", "total_cnt");
	for (i = 0 ; i < flxp_dbg->system_list_size; i++) {
		avg[0] = latency_soc[i].sum[0]/latency_soc[i].cnt;
		avg[1] = latency_soc[i].sum[1]/latency_soc[i].cnt;
		ret += snprintf(buf + ret, BUF_SIZE -ret,
				"%5s[%d]: off: %10d %10d   on: %10d %10d\n",
				"APSOC", i, avg[0], latency_soc[i].cnt,
				avg[1], latency_soc[i].cnt);
	}

	for (i = 0 ; i < flxp_dbg->system_list_size; i++) {
		avg[0] = latency_mif[i].sum[0]/latency_mif[i].cnt;
		avg[1] = latency_mif[i].sum[1]/latency_mif[i].cnt;
		ret += snprintf(buf + ret, BUF_SIZE -ret,
				"%5s[%d]: off: %10d %10d   on: %10d %10d\n",
				"MIF", i, avg[0], latency_mif[i].cnt,
				avg[1], latency_mif[i].cnt);

	}

	return ret;
}

static ssize_t flexpmu_dbg_seq_status_read(int fid, char *buf)
{
	ssize_t ret = 0;
	return ret;
}

static ssize_t flexpmu_dbg_cur_sequence_read(int fid, char *buf)
{
	ssize_t ret = 0;
	return ret;
}

static ssize_t flexpmu_dbg_sw_flag_read(int fid, char *buf)
{
	ssize_t ret = 0;
	return ret;
}

static ssize_t flexpmu_dbg_seq_count_read(int fid, char *buf)
{
	ssize_t ret = 0;
	return ret;
}

static ssize_t flexpmu_dbg_mif_always_on_read(int fid, char *buf)
{
	ssize_t ret = 0;

	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s : %d\n", "MIF always on", flxp_mif->always_on);
	return ret;
}

static ssize_t flexpmu_dbg_lpm_count_read(int fid, char *buf)
{
	ssize_t ret = 0;
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC CNT:", apsoc_down_cnt[0]);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC DSUPD:",
			apsoc_down_cnt[0] - (apsoc_down_cnt[3] + apsoc_down_cnt[4]));
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC DSUPD EARLY WAKEUP:",
			apsoc_ewkup_cnt[0] - (apsoc_ewkup_cnt[3] + apsoc_ewkup_cnt[4]));
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC SICD:", apsoc_down_cnt[3]);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC SICD EARLY WAKEUP:", apsoc_ewkup_cnt[3]);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC SLEEP:", apsoc_down_cnt[4]);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "APSOC SLEEP EARLY WAKEUP:", apsoc_ewkup_cnt[4]);

	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "MIF CNT:", mif_down_cnt[0]);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "MIF SICD:", mif_down_cnt[3]);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %d\n", "MIF SLEEP:", mif_down_cnt[4]);
	return ret;
}

static ssize_t flexpmu_dbg_requester_profiler_read(int fid, char *buf)
{
	size_t ret = 0;
	unsigned long long int curr_tick = 0;
	struct device_node *root = of_find_node_by_name(flexpmu_dbg_np, "mif_requesters");
	int nr_masters = of_property_count_strings(root, "requester-name");
	struct flexpmu_apm_req_info mif_req;
	int i = 0;

	if (!rtc.base) {
		ret = snprintf(buf + ret, BUF_SIZE - ret,
				"%s\n", "This node is not supported.\n");
		return ret;
	}

	curr_tick = __raw_readl(rtc.base + rtc.offset);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %lld\n", "curr_time", curr_tick * rtc.tick2us);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%8s   %32s %32s %32s %32s\n", "Master", "active_since(us ago)",
			"last_rel_time(us ago)", "total_req_time(us)", "req_count");

	for (i = 0; i < nr_masters; i++) {
		mif_req.active_req_tick = requester_mif[i].req_time;
		mif_req.last_rel_tick = requester_mif[i].rel_time;
		mif_req.total_count = requester_mif[i].total_cnt;
		mif_req.total_time_tick = requester_mif[i].total_time;
		if (mif_req.last_rel_tick > 0) {
			mif_req.last_rel_us =
				(curr_tick - mif_req.last_rel_tick) * rtc.tick2us;
		}

		mif_req.total_time_us =
			mif_req.total_time_tick * rtc.tick2us;

		if (mif_req.active_req_tick == 0) {
			mif_req.active_since_us = 0;
		} else {
			mif_req.active_since_us =
				(curr_tick - mif_req.active_req_tick) * rtc.tick2us;
			mif_req.total_time_us += mif_req.active_since_us;
		}

		if (BUF_SIZE - (BUF_MAX_LINE * 3) > ret) {
			ret += snprintf(buf + ret, BUF_SIZE - ret,
					"%8s : %32lld %32lld %32lld %32d\n",
					mif_requester_names[i],
					mif_req.active_since_us,
					mif_req.last_rel_us,
					mif_req.total_time_us,
					mif_req.total_count);
		}
	}

		return ret;
}

static ssize_t flexpmu_dbg_mid_dvs_en_read(int fid, char *buf)
{
	ssize_t ret = 0;
	return ret;
}

#define FLEXPMU_DBG_FUNC_READ(__name)          \
	       flexpmu_dbg_ ## __name ## _read

static ssize_t (*flexpmu_debugfs_read_fptr[FID_MAX])(int, char *) = {
	FLEXPMU_DBG_FUNC_READ(cpu_status),
	FLEXPMU_DBG_FUNC_READ(latency_profiler),
	FLEXPMU_DBG_FUNC_READ(seq_status),
	FLEXPMU_DBG_FUNC_READ(cur_sequence),
	FLEXPMU_DBG_FUNC_READ(sw_flag),
	FLEXPMU_DBG_FUNC_READ(seq_count),
	FLEXPMU_DBG_FUNC_READ(mif_always_on),
	FLEXPMU_DBG_FUNC_READ(lpm_count),
	FLEXPMU_DBG_FUNC_READ(requester_profiler),
	FLEXPMU_DBG_FUNC_READ(mid_dvs_en),
};

static ssize_t flexpmu_dbg_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dbgfs_info *d2f = file->private_data;
	ssize_t ret;
	char *buf = NULL;

	buf = kzalloc(sizeof(char) * BUF_SIZE, GFP_KERNEL);

	ret = flexpmu_debugfs_read_fptr[d2f->fid](d2f->fid, buf);
	if (ret > sizeof(char) * BUF_SIZE)
		return ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static ssize_t flexpmu_dbg_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dbgfs_info *f2d = file->private_data;
	ssize_t ret;
	char buf[32];

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	switch (f2d->fid) {
	case FID_LATENCY_PROFILER:
		if (buf[0] == '0') acpm_enable_flexpmu_profiler(LATENCY_PROFILER_DISABLE);
		if (buf[0] == '1') acpm_enable_flexpmu_profiler(LATENCY_PROFILER_ENABLE);
		break;
	case FID_MIF_ALWAYS_ON:
		if (buf[0] == '0') flxp_mif->always_on = true;
		if (buf[0] == '1') flxp_mif->always_on = false;
		break;
	case FID_REQUESTER_PROFILER:
		if (buf[0] == '0') acpm_enable_flexpmu_profiler(REQUESTER_PROFILER_DISABLE);
		if (buf[0] == '1') acpm_enable_flexpmu_profiler(REQUESTER_PROFILER_ENABLE);
		break;
	default:
		break;
	}
	return ret;
}

static void parse_rtc(struct device_node *np)
{
	rtc.base = of_iomap(np, 0);
	if (!rtc.base) {
		pr_info("%s : RTC is not available!\n",
					EXYNOS_FLEXPMU_DBG_PREFIX);
	} else {
		if (of_property_read_u32(np, "rtc-offset", &rtc.offset)) {
			pr_err("%s %s: can not read rtc-offset in DT\n",
					EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		}
		if (of_property_read_u32(np, "rtc-tick2us", &rtc.tick2us)) {
			pr_err("%s %s: can not read rtc tick to us in DT\n",
					EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		}
		if (of_property_read_u32(np, "rtc-max", &rtc.max)) {
			pr_err("%s %s: can not read rtc max in DT\n",
					EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		}
	}
}

static void parse_mif_requesters(struct device_node *np)
{
	struct device_node *root;
	int ret;
	int size = 0;

	root = of_find_node_by_name(np, "mif_requesters");
	size = of_property_count_strings(root, "requester-name");
	if (size <= 0 || size > 32) {
		pr_err("%s: failed to get flexpmu_master name cnt(%d)\n",
				__func__, size);
		return;
	}
	mif_master_num = size;

	mif_requester_names = kzalloc(sizeof(const char *)* size, GFP_KERNEL);
	if (!mif_requester_names)
		return;

	ret = of_property_read_string_array(root, "requester-name",
			mif_requester_names, size);
	if (ret < 0) {
		pr_err("%s: failed to read mif_requester name(%d)\n",
				__func__, ret);
		kfree(mif_requester_names);
		return;
	}
}

static int flexpmu_dbg_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	const __be32 *prop;
	unsigned int len = 0;
	unsigned int data_base = 0;
	unsigned int data_size = 0;
	unsigned int disable_mifdown = 0;
	u32 flexpmu_dbg_offset = 0;

	flexpmu_dbg_info = kzalloc(sizeof(struct dbgfs_info) * FID_MAX, GFP_KERNEL);
	if (!flexpmu_dbg_info) {
		pr_err("%s %s: can not allocate mem for flexpmu_dbg_info\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_flexpmu_info;
	}
	flexpmu_dbg_np = pdev->dev.of_node;

	prop = of_get_property(pdev->dev.of_node, "data-base", &len);
	if (!prop) {
		pr_err("%s %s: can not read data-base in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	data_base = be32_to_cpup(prop);

	prop = of_get_property(pdev->dev.of_node, "data-size", &len);
	if (!prop) {
		pr_err("%s %s: can not read data-size in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	data_size = be32_to_cpup(prop);

	if (data_base && data_size)
		flexpmu_dbg_base = ioremap(data_base, data_size);

	prop = of_get_property(pdev->dev.of_node, "flexpmu-dbg-offset", &len);
	if (!prop) {
		pr_err("%s %s: can not read flexpmu-dbg-base in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	flexpmu_dbg_offset = be32_to_cpup(prop);

	if (flexpmu_dbg_offset)
		flxp_dbg = (struct flexpmu_dbg *)(flexpmu_dbg_base + __raw_readl(flexpmu_dbg_base + flexpmu_dbg_offset));

	flxp_cpu_pm = (struct flexpmu_cpu_pm *)(flexpmu_dbg_base + flxp_dbg->cpu_pm);
	flxp_mif = (struct flexpmu_mif *)(flexpmu_dbg_base + flxp_dbg->mif);
	mif_req_info = (struct mif_req_info *)(flexpmu_dbg_base + flxp_mif->req_info);

	apsoc_down_cnt = (u32 *)(flexpmu_dbg_base + flxp_cpu_pm->p_apsoc_down_cnt);
	apsoc_ewkup_cnt = (u32 *)(flexpmu_dbg_base + flxp_cpu_pm->p_apsoc_ewkup_cnt);
	mif_down_cnt = (u32 *)(flexpmu_dbg_base + flxp_mif->p_down_cnt);

	of_property_read_u32(pdev->dev.of_node, "nfc-clkreq-idx", &nfc_clkreq_idx);
	flxp_eclkbuf = (struct flexpmu_ext_clk_buf *)(flexpmu_dbg_base + flxp_dbg->eclkbuf);

	latency_cpu = (struct latency_info *)(flexpmu_dbg_base + flxp_dbg->latency_cpu);
	latency_cluster = (struct latency_info *)(flexpmu_dbg_base + flxp_dbg->latency_cluster);
	latency_pd = (struct latency_info *)(flexpmu_dbg_base + flxp_dbg->latency_pd);
	latency_soc = (struct latency_info *)(flexpmu_dbg_base + flxp_dbg->latency_soc);
	latency_mif = (struct latency_info *)(flexpmu_dbg_base + flxp_dbg->latency_mif);
	requester_mif = (struct requester_info *)(flexpmu_dbg_base + flxp_dbg->requester_mif);

	flexpmu_dbg_root = debugfs_create_dir("flexpmu-dbg", NULL);
	if (!flexpmu_dbg_root) {
		pr_err("%s %s: could not create debugfs root dir\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbgfs_probe;
	}

	parse_rtc(pdev->dev.of_node);
	parse_mif_requesters(pdev->dev.of_node);

	for (i = 0; i < FID_MAX; i++) {
		flexpmu_dbg_info[i].fid = i;
		flexpmu_dbg_info[i].fops.open = simple_open;
		flexpmu_dbg_info[i].fops.read = flexpmu_dbg_read;
		flexpmu_dbg_info[i].fops.write = flexpmu_dbg_write;
		flexpmu_dbg_info[i].fops.llseek = default_llseek;
		flexpmu_dbg_info[i].den = debugfs_create_file(flexpmu_debugfs_name[i],
				0644, flexpmu_dbg_root, &flexpmu_dbg_info[i],
				&flexpmu_dbg_info[i].fops);
	}
	of_property_read_u32(pdev->dev.of_node, "disable_mifdown", &disable_mifdown);
	if (disable_mifdown)
		flxp_mif->always_on = true;

	platform_set_drvdata(pdev, flexpmu_dbg_info);

	return 0;

err_dbgfs_probe:
	kfree(flexpmu_dbg_info);
err_flexpmu_info:
	return ret;
}

static int flexpmu_dbg_remove(struct platform_device *pdev)
{
	struct dbgfs_info *flexpmu_dbg_info = platform_get_drvdata(pdev);

	debugfs_remove_recursive(flexpmu_dbg_root);
	kfree(flexpmu_dbg_info);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id flexpmu_dbg_match[] = {
	{
		.compatible = "samsung,flexpmu-dbg",
	},
	{},
};

static struct platform_driver flexpmu_dbg_drv = {
	.probe		= flexpmu_dbg_probe,
	.remove		= flexpmu_dbg_remove,
	.driver		= {
		.name	= "flexpmu_dbg",
		.owner	= THIS_MODULE,
		.of_match_table = flexpmu_dbg_match,
	},
};

static int flexpmu_dbg_init(void)
{
	return platform_driver_register(&flexpmu_dbg_drv);
}
late_initcall(flexpmu_dbg_init);

MODULE_LICENSE("GPL");
