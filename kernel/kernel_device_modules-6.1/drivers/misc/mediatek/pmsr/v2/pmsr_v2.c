// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/slab.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
#include <linux/scmi_protocol.h>
#include <tinysys-scmi.h>
#endif

#include "pmsr_v2.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
struct scmi_tinysys_status rvalue;
static struct scmi_tinysys_info_st *tinfo;
static int scmi_apmcupm_id;
static int pmsr_sspm_ready;
struct pmsr_tool_scmi_data pmsr_scmi_data;
#endif

static unsigned int timer_window_len;
static struct pmsr_cfg cfg;
struct hrtimer pmsr_timer;

static char *ch_name[] = {
	"ch0",
	"ch1",
	"ch2",
	"ch3",
	"ch4",
	"ch5",
	"ch6",
	"ch7",
};

static void pmsr_cfg_init(void)
{
	int i;

	cfg.enable = false;
	cfg.pmsr_speed_mode = DEFAULT_SPEED_MODE;
	cfg.pmsr_window_len = 0;
	cfg.err = 0;
	cfg.test = 0;

	for (i = 0 ; i < SET_CH_MAX; i++) {
		cfg.ch[i].dpmsr_id = 0xFFFFFFFF;
		cfg.ch[i].signal_id = 0xFFFFFFFF; /* default disabled */
	}

	for (i = 0 ; i < cfg.dpmsr_count; i++) {
		cfg.dpmsr[i].seltype = DEFAULT_SELTYPE;
		cfg.dpmsr[i].montype = DEFAULT_MONTYPE;
		cfg.dpmsr[i].signum = 0;
		cfg.dpmsr[i].en = 0;
	}
}

static int pmsr_ipi_init(void)
{
	unsigned int ret = 0;

	/* for AP to SSPM */
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	tinfo = get_scmi_tinysys_info();

	if (!tinfo) {
		pr_info("get scmi info fail\n");
		return ret;
	}

	ret = of_property_read_u32(tinfo->sdev->dev.of_node, "scmi-apmcupm",
			&scmi_apmcupm_id);
	if (ret) {
		pr_info("get scmi-apmcupm fail, ret %d\n", ret);
		pmsr_sspm_ready = -2;
		ret = -1;
		return ret;
	}

	pmsr_sspm_ready = 1;
#endif
	return ret;
}

static void pmsr_procfs_exit(void)
{
	remove_proc_entry("pmsr", NULL);
}

static ssize_t remote_data_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t remote_data_write(struct file *fp, const char __user *userbuf,
				 size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));
	int ret;
	int i;

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= MTK_PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	if (!tinfo)
		return -EFAULT;

	if ((void *)v == (void *)&cfg.enable) {
		if (cfg.enable == true) {
			/* pass the channel setting */
			for (i = 0 ; i < SET_CH_MAX; i++) {
				if (cfg.ch[i].dpmsr_id == 0xFFFFFFFF)
					continue;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
				pmsr_scmi_data.uid = 0x0;
				pmsr_scmi_data.action = PMSR_TOOL_ACT_SIGNAL;
				pmsr_scmi_data.param1 = i;
				pmsr_scmi_data.param2 = cfg.ch[i].dpmsr_id;
				pmsr_scmi_data.param3 = cfg.ch[i].signal_id;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_data.uid, pmsr_scmi_data.action,
					pmsr_scmi_data.param1, pmsr_scmi_data.param2,
					pmsr_scmi_data.param3);
				if (ret)
					cfg.err |= pmsr_scmi_data.action;
#endif
			}
			/* pass the window length */
			timer_window_len = cfg.pmsr_window_len;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_data.uid = 0x0;
			pmsr_scmi_data.action = PMSR_TOOL_ACT_WINDOW;
			pmsr_scmi_data.param1 = cfg.pmsr_window_len;
			pmsr_scmi_data.param2 = cfg.pmsr_speed_mode;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_data.uid, pmsr_scmi_data.action,
				pmsr_scmi_data.param1, pmsr_scmi_data.param2, 0);
			if (ret)
				cfg.err |= pmsr_scmi_data.action;
#endif

			/* pass the signum */
			for (i = 0; i < cfg.dpmsr_count; i++) {
				pmsr_scmi_data.uid = 0x0;
				pmsr_scmi_data.action = PMSR_TOOL_ACT_SIGNUM;
				pmsr_scmi_data.param1 = i;
				pmsr_scmi_data.param2 = cfg.dpmsr[i].signum;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_data.uid, pmsr_scmi_data.action,
					pmsr_scmi_data.param1, pmsr_scmi_data.param2,
					0);
				if (ret)
					cfg.err |= pmsr_scmi_data.action;
			}
			/* pass the seltype */
			for (i = 0; i < cfg.dpmsr_count; i++) {
				pmsr_scmi_data.uid = 0x0;
				pmsr_scmi_data.action = PMSR_TOOL_ACT_SELTYPE;
				pmsr_scmi_data.param1 = i;
				pmsr_scmi_data.param2 = cfg.dpmsr[i].seltype;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_data.uid, pmsr_scmi_data.action,
					pmsr_scmi_data.param1, pmsr_scmi_data.param2,
					0);
				if (ret)
					cfg.err |= pmsr_scmi_data.action;
			}
			/* pass the montype */
			for (i = 0; i < cfg.dpmsr_count; i++) {
				pmsr_scmi_data.uid = 0x0;
				pmsr_scmi_data.action = PMSR_TOOL_ACT_MONTYPE;
				pmsr_scmi_data.param1 = i;
				pmsr_scmi_data.param2 = cfg.dpmsr[i].montype;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_data.uid, pmsr_scmi_data.action,
					pmsr_scmi_data.param1, pmsr_scmi_data.param2,
					0);
				if (ret)
					cfg.err |= pmsr_scmi_data.action;
			}
			/* pass the en */
			for (i = 0; i < cfg.dpmsr_count; i++) {
				pmsr_scmi_data.uid = 0x0;
				pmsr_scmi_data.action = PMSR_TOOL_ACT_EN;
				pmsr_scmi_data.param1 = i;
				pmsr_scmi_data.param2 = cfg.dpmsr[i].en;
				ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
					pmsr_scmi_data.uid, pmsr_scmi_data.action,
					pmsr_scmi_data.param1, pmsr_scmi_data.param2,
					0);
				if (ret)
					cfg.err |= pmsr_scmi_data.action;
			}
			/* pass the enable command */
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_data.uid = 0x0;
			pmsr_scmi_data.action = PMSR_TOOL_ACT_ENABLE;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_data.uid, pmsr_scmi_data.action,
				0, 0, 0);
#endif
			if (!ret) {
				hrtimer_start(&pmsr_timer,
						ns_to_ktime(timer_window_len * NSEC_PER_USEC),
						HRTIMER_MODE_REL_PINNED);
			} else {
				cfg.err |= pmsr_scmi_data.action;
			}
		} else {
			pmsr_cfg_init();
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_data.uid = 0x0;
			pmsr_scmi_data.action = PMSR_TOOL_ACT_DISABLE;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_data.uid, pmsr_scmi_data.action,
				0, 0, 0);
			if (ret)
				cfg.err |= pmsr_scmi_data.action;
#endif
			hrtimer_try_to_cancel(&pmsr_timer);
		}
	} else if ((void *)v == (void *)&cfg.test) {
		if (cfg.test == 1) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			pmsr_scmi_data.uid = 0x0;
			pmsr_scmi_data.action = PMSR_TOOL_ACT_TEST;
			ret = scmi_tinysys_common_set(tinfo->ph, scmi_apmcupm_id,
				pmsr_scmi_data.uid, pmsr_scmi_data.action,
				0, 0, 0);
			if (ret)
				cfg.err |= pmsr_scmi_data.action;
#endif
		}
	}
	return count;
}
static const struct proc_ops remote_data_fops = {
	.proc_read = remote_data_read,
	.proc_write = remote_data_write,
};

static char dbgbuf[1024] = {0};
#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))
#undef log
#define log(fmt, args...)   log2buf(p, dbgbuf, fmt, ##args)

static ssize_t local_ipi_read(struct file *fp, char __user *userbuf,
			      size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbgbuf;

	p[0] = '\0';

	log("pmsr state:\n");
	log("enable %d\n", cfg.enable ? 1 : 0);
	log("speed_mode %u\n", cfg.pmsr_speed_mode);
	log("window_len %u (0x%x)\n",
	    cfg.pmsr_window_len, cfg.pmsr_window_len);
	for (i = 0; i < SET_CH_MAX; i++) {
		if (cfg.ch[i].dpmsr_id < cfg.dpmsr_count)
			log("ch%d: dpmsr %u id %u\n",
			    i,
			    cfg.ch[i].dpmsr_id,
			    cfg.ch[i].signal_id);
		else
			log("ch%d: off\n", i);
	}
	for (i = 0; i < cfg.dpmsr_count; i++) {
		log("dpmsr %u seltype %u montype %u (%s) signum %u en %u\n",
				i,
				cfg.dpmsr[i].seltype,
				cfg.dpmsr[i].montype,
				cfg.dpmsr[i].montype == 0 ? "rising" :
				cfg.dpmsr[i].montype == 1 ? "falling" :
				cfg.dpmsr[i].montype == 2 ? "high level" :
				cfg.dpmsr[i].montype == 3 ? "low level" :
				"unknown",
				cfg.dpmsr[i].signum,
				cfg.dpmsr[i].en);
	}
	log("err 0x%x\n", cfg.err);

	len = p - dbgbuf;
	return simple_read_from_buffer(userbuf, count, f_pos, dbgbuf, len);
}

static ssize_t local_ipi_write(struct file *fp, const char __user *userbuf,
			       size_t count, loff_t *f_pos)
{
	unsigned int *v = pde_data(file_inode(fp));

	if (!userbuf || !v)
		return -EINVAL;

	if (count >= MTK_PMSR_BUF_WRITESZ)
		return -EINVAL;

	if (kstrtou32_from_user(userbuf, count, 10, v))
		return -EFAULT;

	return count;
}

static const struct proc_ops local_ipi_fops = {
	.proc_read = local_ipi_read,
	.proc_write = local_ipi_write,
};

static struct proc_dir_entry *pmsr_droot;

static int pmsr_procfs_init(void)
{
	int i;
	struct proc_dir_entry *ch;
	struct proc_dir_entry *dpmsr_dir_entry;

	pmsr_cfg_init();

	pmsr_droot = proc_mkdir("pmsr", NULL);
	if (pmsr_droot) {
		proc_create("state", 0644, pmsr_droot, &local_ipi_fops);
		proc_create_data("speed_mode", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_speed_mode));
		proc_create_data("window_len", 0644, pmsr_droot, &local_ipi_fops,
				 (void *) &(cfg.pmsr_window_len));
		proc_create_data("enable", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.enable));
		proc_create_data("test", 0644, pmsr_droot, &remote_data_fops,
				 (void *) &(cfg.test));

		for (i = 0 ; i < SET_CH_MAX; i++) {
			ch = proc_mkdir(ch_name[i], pmsr_droot);

			if (ch) {
				proc_create_data("dpmsr_id",
						 0644, ch, &local_ipi_fops,
						 (void *)&(cfg.ch[i].dpmsr_id));
				proc_create_data("signal_id",
						 0644, ch, &local_ipi_fops,
						 (void *)&(cfg.ch[i].signal_id));
			}
		}

		/* If cfg.dpmsr is NULL, return -1 since the memory allocation
		 * might go wrong before this step
		 */
		if (!cfg.dpmsr)
			return -1;

		for (i = 0 ; i < cfg.dpmsr_count; i++) {
			char name[10];
			int len = 0;

			len = snprintf(name, sizeof(name), "dpmsr%d", i);
			/* len should be less than the size of name[] */
			if (len >= sizeof(name))
				pr_notice("dpmsr name fail\n");

			dpmsr_dir_entry = proc_mkdir(name, pmsr_droot);
			if (dpmsr_dir_entry) {
				proc_create_data("seltype",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].seltype));
				proc_create_data("montype",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].montype));
				proc_create_data("signum",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].signum));
				proc_create_data("en",
						 0644, dpmsr_dir_entry, &local_ipi_fops,
						 (void *)&(cfg.dpmsr[i].en));
			}
		}
	}

	return 0;
}

static enum hrtimer_restart pmsr_timer_handle(struct hrtimer *timer)
{
	hrtimer_forward(timer, timer->base->get_time(),
			ns_to_ktime(timer_window_len * NSEC_PER_USEC));

	return HRTIMER_RESTART;
}

static int __init pmsr_parsing_nodes(void)
{
	struct device_node *pmsr_node = NULL;
	char pmsr_desc[] = "mediatek,mtk-pmsr";
	int ret = 0;

	/* find the root node in dts */
	pmsr_node = of_find_compatible_node(NULL, NULL, pmsr_desc);
	if (!pmsr_node) {
		pr_notice("unable to find pmsr device node\n");
		return -1;
	}

	/* find the node and get the value */
	ret = of_property_read_u32_index(pmsr_node, "dpmsr-count",
			0, &cfg.dpmsr_count);
	if (ret) {
		pr_notice("fail to get dpmsr_count from dts\n");
		return -1;
	}
	return 0;
}

static int __init pmsr_init(void)
{

	int ret;

	ret = pmsr_parsing_nodes();
	/* if an error occurs while parsing the nodes, return directly */
	if (ret)
		return 0;

	/* allocate a memory for dpmsr configurations */
	cfg.dpmsr = kmalloc_array(cfg.dpmsr_count,
					sizeof(struct pmsr_dpmsr_cfg), GFP_KERNEL);

	/* create debugfs node */
	pmsr_procfs_init();

	/* register ipi for AP2SSPM communication */
	pmsr_ipi_init();

	hrtimer_init(&pmsr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pmsr_timer.function = pmsr_timer_handle;

	return 0;
}

module_init(pmsr_init);

static void __exit pmsr_exit(void)
{
	/* remove debugfs node */
	pmsr_procfs_exit();
	kfree(cfg.dpmsr);
	hrtimer_try_to_cancel(&pmsr_timer);
}

module_exit(pmsr_exit);

MODULE_DESCRIPTION("Mediatek MT68XX pmsr driver");
MODULE_AUTHOR("SHChen <Show-Hong.Chen@mediatek.com>");
MODULE_LICENSE("GPL");
