// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "aoltest_core.h"

#define CONN_SCP_DBG_PROCNAME "driver/connscp_dbg"

static struct proc_dir_entry *g_conn_scp_dbg_entry;

/* proc functions */
static ssize_t conn_scp_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t conn_scp_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos);

/* action functions */
typedef int(*CONN_SCP_DBG_FUNC) (unsigned int par1, unsigned int par2, unsigned int par3);
static int msd_ctrl(unsigned int par1, unsigned int par2, unsigned int par3);

static const CONN_SCP_DBG_FUNC g_conn_scp_dbg_func[] = {
	[0x0] = msd_ctrl,
};

static struct mutex g_dump_lock;


int msd_ctrl(unsigned int par1, unsigned int par2, unsigned int par3)
{
	pr_info("[%s] [%x][%x][%x]", __func__, par1, par2, par3);
	aoltest_core_send_dbg_msg(par2, par3);
	return 0;
}

ssize_t conn_scp_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

ssize_t conn_scp_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char *pBuf;
	unsigned int x = 0, y = 0, z = 0;
	char *pToken = NULL;
	char *pDelimiter = " \t";
	long res = 0;
	static char dbg_enabled;

	pr_info("write parameter len = %d\n\r", (int) len);
	if (len >= sizeof(buf)) {
		pr_notice("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		if (kstrtol(pToken, 16, &res) == 0)
			x = (unsigned int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		if (kstrtol(pToken, 16, &res) == 0)
			y = (unsigned int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		if (kstrtol(pToken, 16, &res) == 0)
			z = (unsigned int)res;
	}

	pr_info("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	/* For eng and userdebug load, have to enable wmt_dbg by writing 0xDB9DB9 to
	 * "/proc/driver/wmt_dbg" to avoid some malicious use
	 */
	if (x == 0xDB9DB9) {
		dbg_enabled = 1;
		return len;
	}

	if (dbg_enabled == 0)
		return len;

	if (ARRAY_SIZE(g_conn_scp_dbg_func) > x && NULL != g_conn_scp_dbg_func[x])
		(*g_conn_scp_dbg_func[x]) (x, y, z);
	else
		pr_notice("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}

int conn_scp_dbg_init(void)
{
	static const struct proc_ops conn_scp_dbg_fops = {
		.proc_read = conn_scp_dbg_read,
		.proc_write = conn_scp_dbg_write,
	};
	int ret = 0;

	g_conn_scp_dbg_entry = proc_create(CONN_SCP_DBG_PROCNAME, 0664, NULL, &conn_scp_dbg_fops);
	if (g_conn_scp_dbg_entry == NULL) {
		pr_notice("Unable to create connscp_dbg proc entry\n\r");
		ret = -1;
	}

	mutex_init(&g_dump_lock);

	//memset(g_dump_buf, '\0', CONNINFRA_DBG_DUMP_BUF_SIZE);
	return ret;
}

int conn_scp_dbg_deinit(void)
{
	mutex_destroy(&g_dump_lock);

	if (g_conn_scp_dbg_entry != NULL) {
		proc_remove(g_conn_scp_dbg_entry);
		g_conn_scp_dbg_entry = NULL;
	}

	return 0;
}
