/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/clk-provider.h>

#include <soc/samsung/bts.h>
#include "cal_bts7570.h"

#define BUS_WIDTH		16
#define MIF_UTIL		15
#define MIF_UTIL_WITH_CAM	10

#ifdef BTS_DBGGEN
#define BTS_DBG(x...)		pr_err(x)
#else
#define BTS_DBG(x...)		do {} while (0)
#endif

enum bts_index {
	BTS_IDX_CP_CELLULAR,
	BTS_IDX_ISP,
	BTS_IDX_DISPAUD,
	BTS_IDX_CPU,
	BTS_IDX_APM,
	BTS_IDX_CP_GNSS,
	BTS_IDX_CP_WLBT,
	BTS_IDX_MFCMSCL,
	BTS_IDX_G3D,
	BTS_IDX_FSYS,
	BTS_MAX,
};

enum exynos_bts_scenario {
	BS_DEFAULT,
	BS_DISABLE,
	BS_MAX,

	BS_USER = 2,
};

enum exynos_bts_function {
	F_NOP,
	BF_SETQOS,
	BF_SETQOS_BW,
	BF_SETQOS_MO,
	BF_DISABLE,
	TF_SETQOS,
	TF_SETQOS_MO,
	TF_DISABLE,
	SF_SETQOS,
	SF_DISABLE,
};

struct bts_table {
	enum exynos_bts_function fn;
	unsigned int priority;
	unsigned int window;
	unsigned int token;
	unsigned int mo;
	struct bts_info *next_bts;
	int prev_scen;
	int next_scen;
};

struct bts_info {
	enum bts_index id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	bool on;
	struct list_head list;
	bool enable;
	struct clk_info *ct_ptr;
	enum exynos_bts_scenario top_scen;
};

struct bts_scenario {
	const char *name;
	enum exynos_bts_scenario id;
	struct bts_info *head;
};

struct clk_info {
	const char *clk_name;
	struct clk *clk;
	struct bts_info *bts;
};

static struct pm_qos_request exynos7_mif_bts_qos;
static struct pm_qos_request exynos7_int_bts_qos;
static DEFINE_MUTEX(media_mutex);
static void __iomem *base_drex;

static struct bts_info exynos7_bts[] = {
	[BTS_IDX_CPU] = {
		.id = BTS_SYSREG_CPU,
		.name = "cpu",
		.pa_base = EXYNOS7570_PA_SYSREG_CPU,
		.pd_name = "pd-cpu",
		.table[BS_DISABLE].fn = SF_DISABLE,
		.table[BS_DEFAULT].fn = SF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_APM] = {
		.id = BTS_SYSREG_APM,
		.name = "apm",
		.pa_base = EXYNOS7570_PA_SYSREG_APM,
		.pd_name = "pd-apm",
		.table[BS_DISABLE].fn = SF_DISABLE,
		.table[BS_DEFAULT].fn = SF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_CP_GNSS] = {
		.id = BTS_SYSREG_CPGNSS,
		.name = "cp_gnss",
		.pa_base = EXYNOS7570_PA_SYSREG_CP,
		.pd_name = "pd-pmu",
		.table[BS_DISABLE].fn = SF_DISABLE,
		.table[BS_DEFAULT].fn = SF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_CP_CELLULAR] = {
		.name = "cp_cell",
		.pa_base = EXYNOS7570_PA_TREX_CELLULAR,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_DISABLE,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_CP_WLBT] = {
		.name = "cp_wlbt",
		.pa_base = EXYNOS7570_PA_TREX_WLBT,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_MFCMSCL] = {
		.name = "mfcmscl",
		.pa_base = EXYNOS7570_PA_TREX_MFCMSCL,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_G3D] = {
		.name = "g3d",
		.pa_base = EXYNOS7570_PA_TREX_G3D,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_FSYS] = {
		.name = "fsys",
		.pa_base = EXYNOS7570_PA_TREX_FSYS,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_SETQOS,
		.table[BS_DEFAULT].priority = 0x4,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_ISP] = {
		.name = "isp",
		.pa_base = EXYNOS7570_PA_TREX_ISP,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_SETQOS,
		.table[BS_DEFAULT].priority = 0xC,
		.on = false,
		.enable = true,
	},
	[BTS_IDX_DISPAUD] = {
		.name = "dispaud",
		.pa_base = EXYNOS7570_PA_TREX_DISPAUD,
		.pd_name = "pd-trex",
		.table[BS_DISABLE].fn = TF_DISABLE,
		.table[BS_DEFAULT].fn = TF_SETQOS,
		.table[BS_DEFAULT].priority = 0xA,
		.on = false,
		.enable = true,
	},
};
static struct clk_info clk_table[0];

static struct bts_scenario bts_scen[BS_MAX] = {
	[BS_DISABLE] = {
		.name = "disable",
	},
	[BS_DEFAULT] = {
		.name = "default",
	},
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static void bts_clk_on(struct bts_info *bts)
{
	struct clk_info *ptr;

	ptr = bts->ct_ptr;
	if (ptr) {
		bts = ptr->bts;
		do {
			clk_enable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->bts == bts);
	}
}

static void bts_clk_off(struct bts_info *bts)
{
	struct clk_info *ptr;

	ptr = bts->ct_ptr;
	if (ptr) {
		bts = ptr->bts;
		do {
			clk_disable(ptr->clk);
		} while (++ptr < clk_table + ARRAY_SIZE(clk_table)
				&& ptr->bts == bts);
	}
}

static void bts_set_ip_table(struct bts_info *bts)
{
	enum exynos_bts_scenario scen = bts->top_scen;
	enum exynos_bts_function fn = bts->table[scen].fn;

	BTS_DBG("[BTS] %s on:%d bts scen: [%s]->[%s]\n", bts->name, bts->on,
			bts_scen[scen].name, bts_scen[scen].name);

	switch (fn) {
	case BF_SETQOS:
		bts_setqos(bts->va_base, bts->table[scen].priority);
		break;
	case BF_SETQOS_MO:
		bts_setqos_mo(bts->va_base, bts->table[scen].priority,
				bts->table[scen].mo);
		break;
	case BF_DISABLE:
		bts_disable(bts->va_base);
		break;
	case TF_SETQOS:
		trex_setqos(bts->va_base, bts->table[scen].priority);
		break;
	case TF_SETQOS_MO:
		trex_setqos_mo(bts->va_base, bts->table[scen].priority,
				bts->table[scen].mo);
		break;
	case TF_DISABLE:
		trex_disable(bts->va_base);
		break;
	case SF_SETQOS:
		sysreg_setqos(bts->id, bts->va_base,
				bts->table[scen].priority);
		break;
	case SF_DISABLE:
		sysreg_disable(bts->id, bts->va_base);
		break;
	default:
		break;
	}
}

static void bts_add_scen(enum exynos_bts_scenario scen)
{
	struct bts_info *first = bts_scen[scen].head;
	struct bts_info *bts = bts_scen[scen].head;
	int next = 0;
	int prev = 0;

	if (!bts || scen < BS_USER)
		return;

	BTS_DBG("[bts] scen %s on\n", bts_scen[scen].name);

	do {
		if (bts->enable &&!bts->table[scen].next_scen) {
			if (scen >= bts->top_scen) {
				/* insert at top priority */
				bts->table[scen].prev_scen = bts->top_scen;
				bts->table[bts->top_scen].next_scen = scen;
				bts->top_scen = scen;
				bts->table[scen].next_scen = -1;

				if (bts->on)
					bts_set_ip_table(bts);

			} else {
				/* insert at middle */
				for (prev = bts->top_scen; prev > scen; prev = bts->table[prev].prev_scen)
					next = prev;

				bts->table[scen].prev_scen = bts->table[next].prev_scen;
				bts->table[scen].next_scen = bts->table[prev].next_scen;
				bts->table[next].prev_scen = scen;
				bts->table[prev].next_scen = scen;
			}
		}

		bts = bts->table[scen].next_bts;
	/* set all bts ip in the current scenario */
	} while (bts && bts != first);
}

static void bts_del_scen(enum exynos_bts_scenario scen)
{
	struct bts_info *first = bts_scen[scen].head;
	struct bts_info *bts = bts_scen[scen].head;
	int next = 0;
	int prev = 0;

	if (!bts || scen < BS_USER)
		return;

	BTS_DBG("[bts] scen %s off\n", bts_scen[scen].name);

	do {
		if (bts->enable && bts->table[scen].next_scen) {
			if (scen == bts->top_scen) {
				/* revert to prev scenario */
				prev = bts->table[scen].prev_scen;
				bts->top_scen = prev;
				bts->table[prev].next_scen = -1;
				bts->table[scen].next_scen = 0;
				bts->table[scen].prev_scen = 0;

				if (bts->on)
					bts_set_ip_table(bts);
			} else if (scen < bts->top_scen) {
				/* delete mid scenario */
				prev = bts->table[scen].prev_scen;
				next = bts->table[scen].next_scen;

				bts->table[next].prev_scen = bts->table[scen].prev_scen;
				bts->table[prev].next_scen = bts->table[scen].next_scen;

				bts->table[scen].prev_scen = 0;
				bts->table[scen].next_scen = 0;

			} else {
				BTS_DBG("[BTS]%s scenario couldn't exist above top_scen\n", bts_scen[scen].name);
			}
		}

		bts = bts->table[scen].next_bts;
	/* revert all bts ip to prev in the current scenario */
	} while (bts && bts != first);
}

void bts_scen_update(enum bts_scen_type type, unsigned int val)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;
	bool on = val ? 1 : 0;

	spin_lock(&bts_lock);

	switch (type) {
	default:
		spin_unlock(&bts_lock);
		return;
	}

	if (on)
		bts_add_scen(scen);
	else
		bts_del_scen(scen);

	spin_unlock(&bts_lock);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;

	spin_lock(&bts_lock);

	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strncmp(bts->pd_name, pd_name, strlen(pd_name))) {
			BTS_DBG("[BTS] %s on/off:%d->%d\n", bts->name, bts->on, on);

			if (!bts->enable) {
				bts->on = on;
				continue;
			}

			if (on) {
				bts->on = true;
				bts_set_ip_table(bts);
			} else {
				bts->on = false;
			}
		}

	spin_unlock(&bts_lock);
}

static void scen_chaining(enum exynos_bts_scenario scen)
{
	struct bts_info *prev = NULL;
	struct bts_info *first = NULL;
	struct bts_info *bts;

	list_for_each_entry(bts, &bts_list, list) {
		if (bts->table[scen].fn) {
			if (!first)
				first = bts;
			if (prev)
				prev->table[scen].next_bts = bts;

			prev = bts;
		}
	}

	if (prev)
		prev->table[scen].next_bts = first;

	bts_scen[scen].head = first;
}

static int exynos7_qos_status_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *bts;

	spin_lock(&bts_lock);

	for (bts = exynos7_bts; bts <= &exynos7_bts[BTS_MAX - 1]; bts++) {
		if (!bts->enable)
			continue;
		seq_printf(buf, "[BTS] %7s: scen %s, ", bts->name,
				bts_scen[bts->top_scen].name);
		if (bts->on) {
			if (bts->ct_ptr)
				bts_clk_on(bts);

			switch (bts->table[BS_DISABLE].fn) {
			case BF_DISABLE:
				bts_showqos(bts->va_base, buf);
				break;
			case TF_DISABLE:
				trex_showqos(bts->va_base, buf);
				break;
			case SF_DISABLE:
				sysreg_showqos(bts->id, bts->va_base, buf);
				break;
			default:
				seq_puts(buf, "none\n");
				break;
			}

			if (bts->ct_ptr)
				bts_clk_off(bts);
		} else {
			seq_puts(buf, "off\n");
		}
	}

	spin_unlock(&bts_lock);

	return 0;
}

static int exynos7_qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos7_qos_status_open_show, inode->i_private);
}

static const struct file_operations debug_qos_status_fops = {
	.open		= exynos7_qos_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void bts_debugfs(void)
{
	struct dentry *den;

	den = debugfs_create_dir("bts", NULL);
	debugfs_create_file("qos", 0440,	den, NULL, &debug_qos_status_fops);
}

static void bts_drex_init(void __iomem *base)
{

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, base + QOS_TIMEOUT_0xF);
	__raw_writel(0x00000004, base + QOS_TIMEOUT_0xE);
	__raw_writel(0x00000008, base + QOS_TIMEOUT_0xD);
	__raw_writel(0x00000010, base + QOS_TIMEOUT_0xC);
	__raw_writel(0x00000020, base + QOS_TIMEOUT_0xB);
	__raw_writel(0x00000040, base + QOS_TIMEOUT_0xA);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x9);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x8);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x7);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x6);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x5);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x4);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x3);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x2);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x1);
	__raw_writel(0x00000100, base + QOS_TIMEOUT_0x0);
}

static void bts_initialize_domains(void)
{
	bts_initialize("pd-cpu", true);
	bts_initialize("pd-apm", true);
	bts_initialize("pd-pmu", true);
	bts_initialize("pd-trex", true);
}

static int exynos_bts_notifier_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	switch (event) {
	case PM_POST_SUSPEND:
		bts_drex_init(base_drex);
		bts_initialize_domains();

		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

void exynos7_init_bts_ioremap(void)
{
	base_drex = ioremap(EXYNOS7570_PA_DREX, SZ_4K);
}

void exynos_update_media_scenario(enum bts_media_type media_type,
		unsigned int bw, int bw_type)
{
	static unsigned int decon_bw, cam_bw;
	unsigned int mif_freq;
	unsigned int total_bw;
	mutex_lock(&media_mutex);

	switch (media_type) {
	case TYPE_DECON_INT:
		if (decon_bw == bw)
			goto out;
		decon_bw = bw >> 10;
		break;
	case TYPE_CAM:
		if (cam_bw == bw)
			goto out;
		cam_bw = bw >> 10;
		break;
	default:
		pr_err("DEVFREQ(MIF) : unsupportd media_type - %u", media_type);
		break;
	}

	total_bw = decon_bw + cam_bw;

	/* MIF minimum frequency calculation as per BTS guide */
	if (cam_bw) {
		mif_freq = decon_bw * 100 / BUS_WIDTH / MIF_UTIL_WITH_CAM;
	} else {
		mif_freq = decon_bw * 100 / BUS_WIDTH / MIF_UTIL;
	}

	BTS_DBG("[BTS BW] total: %uKB/s, decon %uKB/s, cam %uKB/s\n",
			total_bw, decon_bw, cam_bw);
	BTS_DBG("[BTS FREQ] mif_freq: %uKhz\n", mif_freq);

	pm_qos_update_request(&exynos7_mif_bts_qos, mif_freq);
out:
	mutex_unlock(&media_mutex);
}

static int __init exynos7_bts_init(void)
{
	int i;
	int ret;
	struct bts_info *bts = NULL;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	/* clk initialize */
	for (i = 0; i < ARRAY_SIZE(clk_table); i++) {

		if (bts && bts != clk_table[i].bts) {
			bts = clk_table[i].bts;
			bts->ct_ptr = clk_table + i;
		}
		clk_table[i].clk = clk_get(NULL, clk_table[i].clk_name);

		if (IS_ERR(clk_table[i].clk)){
			BTS_DBG("failed to get bts clk %s\n",
					clk_table[i].clk_name);
			bts->ct_ptr = NULL;
		}
		else {
			ret = clk_prepare(clk_table[i].clk);
			if (ret) {
				pr_err("[BTS] failed to prepare bts clk %s\n",
						clk_table[i].clk_name);
				for (; i >= 0; i--)
					clk_put(clk_table[i].clk);
				return ret;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(exynos7_bts); i++) {
		exynos7_bts[i].va_base = ioremap(exynos7_bts[i].pa_base, SZ_2K);

		list_add(&exynos7_bts[i].list, &bts_list);
	}

	for (i = BS_USER; i < BS_MAX; i++) {
		scen_chaining(i);
		BTS_DBG("[BTS][%s] scene(%d) is chanined\n", __func__, i);
	}

	exynos7_init_bts_ioremap();
	bts_drex_init(base_drex);
	bts_initialize_domains();

	pm_qos_add_request(&exynos7_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&exynos7_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, 0);

	register_pm_notifier(&exynos_bts_notifier);
	bts_debugfs();
	return 0;
}
arch_initcall(exynos7_bts_init);
