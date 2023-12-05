// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_reset_reason.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/sec_debug.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/ctype.h>
#include <asm/stacktrace.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "sec_debug_internal.h"

#define PWRSRC_LEN 16
#define RSTSTAT_LEN 32

static char regs_bit[][16][PWRSRC_LEN] = {
	{ "UNINIT0", "UNINIT2", "UNINIT3", "UNINIT4", "UNINIT5", "UNINIT6", "UNINIT7",
	  "UNINIT8", "UNINIT9", "UNINIT10", "UNINIT11", "UNINIT12", "UNINIT13", "UNINIT14", "UNINIT15",
	},
	{ "UNINIT0", "UNINIT1", "UNINIT2", "UNINIT3", "UNINIT4", "UNINIT5", "UNINIT6", "UNINIT7",
	  "UNINIT8", "UNINIT9", "UNINIT10", "UNINIT11", "UNINIT12", "UNINIT13", "UNINIT14", "UNINIT15",
	},
}; /* S2MPS23 */

static char dword_regs_bit[][32][RSTSTAT_LEN] = {
	{ "CLUSTER0_DBGRSTREQ0", "CLUSTER0_DBGRSTREQ1", "CLUSTER0_DBGRSTREQ2", "CLUSTER0_DBGRSTREQ3",
	  "CLUSTER1_DBGRSTREQ0", "CLUSTER1_DBGRSTREQ1", "CLUSTER2_DBGRSTREQ0", "CLUSTER2_DBGRSTREQ1",
	  "RSVD8", "RSVD9", "RSVD10", "RSVD11",
	  "RSVD12", "RSVD13", "CLUSTER2_WARMRSTREQ0", "CLUSTER2_WARMRSTREQ1",
	  "PINRESET", "RSVD17", "RSVD18", "RSVD19",
	  "APM_CPU_WDTRESET", "APM_CPU_SYSRESET", "VTS_CPU_WDTRESET", "CHUB_WDTRESET",
	  "CLUSTER0_WDTRESET", "CLUSTER2_WDTRESET", "AUD_CPU0_WDTRESET", "SSS_WDTRESET",
	  "DBGCORE_CPU_WDTRESET", "WRESET", "SWRESET", "PORESET"
	}, /* RST_STAT */
}; /* EXYNOS 2100 */

static struct outbuf extra_buf;
static struct outbuf pwrsrc_buf;

static int power_off_src_cnt;
static int power_on_src_cnt;
static int rst_stat_cnt;

static int __read_mostly reset_reason;
module_param(reset_reason, int, 0440);

static long __read_mostly pwrsrc;
module_param(pwrsrc, long, 0440);

static long __read_mostly rstcnt_rs;
module_param(rstcnt_rs, long, 0440);

static void _secdbg_rere_write_buf(struct outbuf *obuf, int len, const char *fmt, ...)
{
	va_list list;
	char *base;
	int rem, ret;

	base = obuf->buf;
	base += obuf->index;

	rem = sizeof(obuf->buf);
	rem -= obuf->index;

	if (rem <= 0)
		return;

	if ((len > 0) && (len < rem))
		rem = len;

	va_start(list, fmt);
	ret = vsnprintf(base, rem, fmt, list);
	if (ret)
		obuf->index += ret;

	va_end(list);
}

static int secdbg_reset_reason_proc_show(struct seq_file *m, void *v)
{
	pr_info("%s: reset_reason: %d\n", __func__, reset_reason);

	if (reset_reason == RR_S)
		seq_puts(m, "SPON\n");
	else if (reset_reason == RR_W)
		seq_puts(m, "WPON\n");
	else if (reset_reason == RR_D)
		seq_puts(m, "DPON\n");
	else if (reset_reason == RR_K)
		seq_puts(m, "KPON\n");
	else if (reset_reason == RR_M)
		seq_puts(m, "MPON\n");
	else if (reset_reason == RR_P)
		seq_puts(m, "PPON\n");
	else if (reset_reason == RR_R)
		seq_puts(m, "RPON\n");
	else if (reset_reason == RR_B)
		seq_puts(m, "BPON\n");
	else if (reset_reason == RR_T)
		seq_puts(m, "TPON\n");
	else if (reset_reason == RR_C)
		seq_puts(m, "CPON\n");
	else
		seq_puts(m, "NPON\n");

	return 0;
}

static bool is_target_reset_reason(void)
{
	if (reset_reason == RR_N ||
		reset_reason == RR_R)
		return false;

	return true;
}

static int secdbg_reset_reason_store_lastkmsg_proc_show(struct seq_file *m, void *v)
{
	if (is_target_reset_reason())
		seq_puts(m, "1\n");
	else
		seq_puts(m, "0\n");

	return 0;
}

static int secdbg_rere_get_rstcnt_from_cmdline(void)
{
	return rstcnt_rs >> 32;
}

static unsigned long get_offsrc_from_pwrsrc(unsigned long pwrsrc)
{
	return (pwrsrc & 0xffff0000) >> 16;
}

static unsigned long get_onsrc_from_pwrsrc(unsigned long pwrsrc)
{
	return (pwrsrc & 0x0000ffff);
}

static unsigned long get_rststat_from_rstcnt_rs(unsigned long rstcnt_rs)
{
	return (rstcnt_rs & 0xffffffff);
}

#define for_each_pmic_sfr(offset, i, j) \
	for (i = DEFAULT_SRC_NUM - offset, j = 0; i < DEFAULT_SRC_NUM; i++, j++)

#define for_each_pmic_sfr_with_set(offset, i, j, target) \
	for_each_pmic_sfr(offset, i, j) \
		if (target & (1 << i))

/*
 * proc/pwrsrc
 * OFFSRC (from PWROFF - 32) + ONSRC (from PWR - 32) + RSTSTAT (from RST - 32)
 * regs_bit (max 8) / dword_regs_bit (max 32)
 * total max : 48
 */

#define DEFAULT_SRC_NUM 16
static int secdbg_reset_reason_pwrsrc_show(struct seq_file *m, void *v)
{
	int i, reg_i;
	unsigned long rststat, offsrc, onsrc;

	if (pwrsrc_buf.already)
		goto out;

	memset(&pwrsrc_buf, 0, sizeof(pwrsrc_buf));

	_secdbg_rere_write_buf(&pwrsrc_buf, 0, "OFFSRC:");
	offsrc = get_offsrc_from_pwrsrc(pwrsrc);
	if (!offsrc)
		_secdbg_rere_write_buf(&pwrsrc_buf, 0, " -");
	else {
		for_each_pmic_sfr_with_set(power_off_src_cnt, i, reg_i, offsrc)
			_secdbg_rere_write_buf(&pwrsrc_buf, 0, " %s", regs_bit[0][reg_i]);
	}
	_secdbg_rere_write_buf(&pwrsrc_buf, 0, " /");

	_secdbg_rere_write_buf(&pwrsrc_buf, 0, " ONSRC:");
	onsrc = get_onsrc_from_pwrsrc(pwrsrc);
	if (!onsrc)
		_secdbg_rere_write_buf(&pwrsrc_buf, 0, " -");
	else {
		for_each_pmic_sfr_with_set(power_on_src_cnt, i, reg_i, onsrc)
			_secdbg_rere_write_buf(&pwrsrc_buf, 0, " %s", regs_bit[1][reg_i]);
	}

	_secdbg_rere_write_buf(&pwrsrc_buf, 0, " /");

	_secdbg_rere_write_buf(&pwrsrc_buf, 0, " RSTSTAT:");
	rststat = get_rststat_from_rstcnt_rs(rstcnt_rs);
	if (!rststat)
		_secdbg_rere_write_buf(&pwrsrc_buf, 0, " -");
	else
		for (i = 0; i < 32; i++)
			if (rststat & (1 << i))
				_secdbg_rere_write_buf(&pwrsrc_buf, 0, " %s", dword_regs_bit[0][i]);

	pwrsrc_buf.already = 1;
out:
	seq_printf(m, "%s", pwrsrc_buf.buf);

	return 0;
}

#define BBP_STR_LEN (64)

static void handle_bug_string(char *buf, char *src)
{
	int idx = 0, len, i;

	len = strlen(src);
	if (len > BBP_STR_LEN)
		len = BBP_STR_LEN;

	for (i = 0; i < len; i++) {
		if (src[i] == '/')
			idx = i;
	}

	if (idx)
		strncpy(buf, &(src[idx + 1]), len - idx);
	else
		strncpy(buf, src, len);
}

static void handle_bus_string(char *buf, char *src)
{
	int idx = 0, len, max = 2, cnt = 0, i;

	len = strlen(src);
	if (len > BBP_STR_LEN)
		len = BBP_STR_LEN;

	for (i = 0; i < len; i++) {
		if (src[i] == '/') {
			idx = i;
			cnt++;

			if (cnt == max)
				goto out;
		}
	}

out:
	if (idx)
		strncpy(buf, src, idx);
	else
		strncpy(buf, src, len);
}

enum pnc_str {
	PNC_STR_IGNORE = 0,
	PNC_STR_UNRECV = 1,
	PNC_STR_REST = 2,
};

static int handle_panic_string(char *src)
{
	if (!src)
		return PNC_STR_IGNORE;

	if (!strncmp(src, "Fatal", 5))
		return PNC_STR_IGNORE;
	else if (!strncmp(src, "ITMON", 5))
		return PNC_STR_IGNORE;
	else if (!strncmp(src, "Unrecoverable", 13))
		return PNC_STR_UNRECV;

	return PNC_STR_REST;
}

/*
 * proc/extra
 * RSTCNT (32) + PC (256) + LR (256) (MAX: 544)
 * + BUG (256) + BUS (256) => get only 64 (BBP_STR_LEN) (MAX: 672)
 * + PANIC (256) => get only 64 (MAX: 736)
 * + SMU (256) => get only 64 (MAX: 800)
 * total max : 800
 */
static int secdbg_reset_reason_extra_show(struct seq_file *m, void *v)
{
	ssize_t ret = 0;
	char *rstcnt, *pc, *lr;
	char *bug, *bus, *pnc, *smu, *wdgc;
	char buf_bug[BBP_STR_LEN] = {0, };
	char buf_bus[BBP_STR_LEN] = {0, };

	if (extra_buf.already)
		goto out;

	memset(&extra_buf, 0, sizeof(extra_buf));

	rstcnt = get_bk_item_val("RSTCNT");
	pc = get_bk_item_val("PC");
	lr = get_bk_item_val("LR");

	bug = get_bk_item_val("BUG");
	bus = get_bk_item_val("BUS");
	pnc = get_bk_item_val("PANIC");
	smu = get_bk_item_val("SMU");
	wdgc = get_bk_item_val("WDGC");

	_secdbg_rere_write_buf(&extra_buf, 0, "RCNT:");
	if (rstcnt && strnlen(rstcnt, MAX_ITEM_VAL_LEN))
		_secdbg_rere_write_buf(&extra_buf, 0, " %s /", rstcnt);
	else
		_secdbg_rere_write_buf(&extra_buf, 0, " - /");

	_secdbg_rere_write_buf(&extra_buf, 0, " PC:");
	if (pc && strnlen(pc, MAX_ITEM_VAL_LEN))
		_secdbg_rere_write_buf(&extra_buf, 0, " %s", pc);
	else
		_secdbg_rere_write_buf(&extra_buf, 0, " -");

	_secdbg_rere_write_buf(&extra_buf, 0, " LR:");
	if (lr && strnlen(lr, MAX_ITEM_VAL_LEN))
		_secdbg_rere_write_buf(&extra_buf, 0, " %s", lr);
	else
		_secdbg_rere_write_buf(&extra_buf, 0, " -");

	/* BUG */
	if (bug && strnlen(bug, MAX_ITEM_VAL_LEN)) {
		handle_bug_string(buf_bug, bug);

		_secdbg_rere_write_buf(&extra_buf, 0, " BUG: %s", buf_bug);
	}

	/* BUS */
	if (bus && strnlen(bus, MAX_ITEM_VAL_LEN)) {
		handle_bus_string(buf_bus, bus);

		_secdbg_rere_write_buf(&extra_buf, 0, " BUS: %s", buf_bus);
	}

	/* PANIC */
	ret = handle_panic_string(pnc);
	if (ret == PNC_STR_UNRECV) {
		if (smu && strnlen(smu, MAX_ITEM_VAL_LEN))
			_secdbg_rere_write_buf(&extra_buf, BBP_STR_LEN, " SMU: %s", smu);
		else
			_secdbg_rere_write_buf(&extra_buf, BBP_STR_LEN, " PANIC: %s", pnc);
	} else if (ret == PNC_STR_REST) {
		if (strnlen(pnc, MAX_ITEM_VAL_LEN))
			_secdbg_rere_write_buf(&extra_buf, BBP_STR_LEN, " PANIC: %s", pnc);
	}

	/* Watchdog Caller */
	if (wdgc && strnlen(wdgc, MAX_ITEM_VAL_LEN))
		_secdbg_rere_write_buf(&extra_buf, 0, " WDGC: %s", wdgc);

	extra_buf.already = 1;

out:
	seq_printf(m, "%s", extra_buf.buf);

	return 0;
}


static int set_debug_reset_rwc_proc_show(struct seq_file *m, void *v)
{
	char *rstcnt;

	rstcnt = get_bk_item_val("RSTCNT");

	if (!rstcnt)
		seq_printf(m, "%d", secdbg_rere_get_rstcnt_from_cmdline());
	else
		seq_printf(m, "%s", rstcnt);

	return 0;
}

static void secdbg_rere_parse_dt(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "sec_debug_reset_reason");
	const char *src_name;
	int ret, i;

	power_off_src_cnt = of_property_count_strings(np, "power_off_src");
	power_on_src_cnt = of_property_count_strings(np, "power_on_src");
	rst_stat_cnt = of_property_count_strings(np, "rst_stat");

	for (i = 0; i < power_off_src_cnt; i++) {
		ret = of_property_read_string_index(np, "power_off_src", i, &src_name);
		if (ret < 0)
			snprintf(regs_bit[0][i], PWRSRC_LEN, "ERR");
		else
			snprintf(regs_bit[0][i], PWRSRC_LEN, "%s", src_name);
	}

	for (i = 0; i < power_on_src_cnt; i++) {
		ret = of_property_read_string_index(np, "power_on_src", i, &src_name);
		if (ret < 0)
			snprintf(regs_bit[1][i], PWRSRC_LEN, "ERR");
		else
			snprintf(regs_bit[1][i], PWRSRC_LEN, "%s", src_name);
	}

	if (rst_stat_cnt > 0) {
		for (i = 0; i < rst_stat_cnt; i++) {
			ret = of_property_read_string_index(np, "rst_stat", i, &src_name);
			if (ret < 0)
				snprintf(dword_regs_bit[0][i], RSTSTAT_LEN, "ERR");
			else
				snprintf(dword_regs_bit[0][i], RSTSTAT_LEN, "%s", src_name);
		}
	}
}

static int __init secdbg_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	pr_info("%s: from cmdline: reset_reason: %d\n", __func__, reset_reason);
	pr_info("%s: from cmdline: pwrsrc: %lx, rstcnt_rs: %lx\n", __func__, pwrsrc, rstcnt_rs);

	secdbg_rere_parse_dt();

	entry = proc_create_single("reset_reason", 0400, NULL, secdbg_reset_reason_proc_show);
	if (!entry)
		return -ENOMEM;

	entry = proc_create_single("store_lastkmsg", 0400, NULL, secdbg_reset_reason_store_lastkmsg_proc_show);
	if (!entry)
		return -ENOMEM;

	entry = proc_create_single("extra", 0400, NULL, secdbg_reset_reason_extra_show);
	if (!entry)
		return -ENOMEM;

	entry = proc_create_single("pwrsrc", 0400, NULL, secdbg_reset_reason_pwrsrc_show);
	if (!entry)
		return -ENOMEM;

	entry = proc_create_single("reset_rwc", 0400, NULL, &set_debug_reset_rwc_proc_show);
	if (!entry)
		return -ENOMEM;

	return 0;
}
module_init(secdbg_reset_reason_init);

MODULE_DESCRIPTION("Samsung Debug HW Parameter driver");
MODULE_LICENSE("GPL v2");
