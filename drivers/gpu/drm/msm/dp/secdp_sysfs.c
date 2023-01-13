/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <drm/drm_edid.h>
#include <linux/string.h>
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include <linux/displayport_bigdata.h>
#endif

#include "secdp.h"
#include "secdp_sysfs.h"
#include "sde_edid_parser.h"
#include "secdp_unit_test.h"

enum secdp_unit_test_cmd {
	SECDP_UTCMD_EDID_PARSE = 0,
};

struct secdp_sysfs_private {
	struct device *dev;
	struct secdp_sysfs dp_sysfs;
	struct secdp_misc *sec;
	enum secdp_unit_test_cmd test_cmd;
};

struct secdp_sysfs_private *g_secdp_sysfs;

static inline char *secdp_utcmd_to_str(u32 cmd_type)
{
	switch (cmd_type) {
	case SECDP_UTCMD_EDID_PARSE:
		return SECDP_ENUM_STR(SECDP_UTCMD_EDID_PARSE);
	default:
		return "unknown";
	}
}

/** check if buf has range('-') format
 * @buf		buf to be checked
 * @size	buf size
 * @retval	0 if args are ok, -1 if '-' included
 */
static int secdp_check_store_args(const char *buf, size_t size)
{
	int ret;

	if (strnchr(buf, size, '-')) {
		pr_err("range is forbidden!\n");
		ret = -1;
		goto exit;
	}

	ret = 0;
exit:
	return ret;
}

#ifdef CONFIG_SEC_FACTORY
static ssize_t dp_sbu_sw_sel_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int val[10] = {0,};
	int sbu_sw_sel, sbu_sw_oe;

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	get_options(buf, ARRAY_SIZE(val), val);

	sbu_sw_sel = val[1];
	sbu_sw_oe = val[2];
	pr_info("sbu_sw_sel(%d), sbu_sw_oe(%d)\n", sbu_sw_sel, sbu_sw_oe);

	if (sbu_sw_oe == 0/*on*/)
		secdp_config_gpios_factory(sbu_sw_sel, true);
	else if (sbu_sw_oe == 1/*off*/)
		secdp_config_gpios_factory(sbu_sw_sel, false);
	else
		pr_err("unknown sbu_sw_oe value: %d\n", sbu_sw_oe);

exit:
	return size;
}

static CLASS_ATTR_WO(dp_sbu_sw_sel);
#endif

#define SECDP_DEX_ADAPTER_SKIP	"SkipAdapterCheck"

static ssize_t dex_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int rc = 0;
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	struct secdp_dex *dex = &sysfs->sec->dex;

	if (!secdp_get_cable_status() || !secdp_get_hpd_status() ||
			secdp_get_poor_connection_status() || !secdp_get_link_train_status()) {
		pr_info("cable is out\n");
		dex->prev = dex->curr = dex->dex_node_status = DEX_DISABLED;
	}

	pr_info("prev:%d, curr:%d, status:%d, %s:%d\n",
			dex->prev, dex->curr, dex->dex_node_status,
			SECDP_DEX_ADAPTER_SKIP, secdp_dex_adapter_skip_show());
	rc = scnprintf(buf, PAGE_SIZE, "%d\n", dex->dex_node_status);

	if (dex->dex_node_status == DEX_DURING_MODE_CHANGE)
		dex->dex_node_status = dex->curr;

	return rc;
}

/*
 * assume that 1 HMD device has name(14),vid(4),pid(4) each, then
 * max 32 HMD devices(name,vid,pid) need 806 bytes including TAG, NUM, comba
 */
#define MAX_DEX_STORE_LEN	1024

static ssize_t dex_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t size)
{
	char str[MAX_DEX_STORE_LEN] = {0,}, *p, *tok;
	int len, val[4] = {0,};
	int setting_ui;	/* setting has Dex mode? if yes, 1. otherwise 0 */
	int run;	/* dex is running now?   if yes, 1. otherwise 0 */

	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	struct secdp_misc *sec = sysfs->sec;
	struct secdp_dex *dex = &sec->dex;

	if (secdp_check_if_lpm_mode()) {
		pr_info("it's LPM mode. skip\n");
		goto exit;
	}

	if (size >= MAX_DEX_STORE_LEN) {
		pr_err("too long args! %d\n", size);
		goto exit;
	}

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	mutex_lock(&sec->hmd_lock);
	memcpy(str, buf, size);
	p   = str;
	tok = strsep(&p, ",");
	len = strlen(tok);
	pr_debug("tok: %s, len: %d\n", tok, len);

	if (!strncmp(DEX_TAG_HMD, tok, len)) {
		int num_hmd = 0, sz = 0, ret;

		tok = strsep(&p, ",");
		sz  = strlen(tok);
		ret = kstrtouint(tok, 10, &num_hmd);
		pr_debug("[%s] num:%d,sz:%d,ret:%d\n", DEX_TAG_HMD,
			num_hmd, sz, ret);
		if (!ret) {
			ret = secdp_store_hmd_dev(str + (len + sz + 2),
					size - (len + sz + 2), num_hmd);
		}
		if (ret)
			size = ret;	/* error! */

		mutex_unlock(&sec->hmd_lock);
		goto exit;
	}
	mutex_unlock(&sec->hmd_lock);

	if (!strncmp(SECDP_DEX_ADAPTER_SKIP, tok, len)) {
		int param = 0, sz = 0, ret;

		tok = strsep(&p, ",");
		sz  = strlen(tok);
		ret = kstrtouint(tok, 2, &param);
		if (ret) {
			pr_err("error:%d\n", ret);
			goto exit;
		}

		pr_debug("[%s] param:%d,sz:%d,ret:%d\n", SECDP_DEX_ADAPTER_SKIP,
			param, sz, ret);

		secdp_dex_adapter_skip_store((!param) ? false : true);
		goto exit;
	}

	get_options(buf, ARRAY_SIZE(val), val);
	pr_info("%d(0x%02x)\n", val[1], val[1]);
	setting_ui = (val[1] & 0xf0) >> 4;
	run = (val[1] & 0x0f);

	pr_info("setting_ui: %d, run: %d, cable: %d\n",
		setting_ui, run, sec->cable_connected);

	dex->setting_ui = setting_ui;
	dex->dex_node_status = dex->curr = run;

	mutex_lock(&sec->notifier_lock);
	if (!sec->ccic_noti_registered) {
		int rc;

		pr_debug("notifier get registered by dex\n");

		/* cancel immediately */
		rc = cancel_delayed_work(&sec->ccic_noti_reg_work);
		pr_debug("cancel_work, rc(%d)\n", rc);
		destroy_delayed_work_on_stack(&sec->ccic_noti_reg_work);

		/* register */
		rc = secdp_ccic_noti_register_ex(sec, false);
		if (rc)
			pr_err("noti register fail, rc(%d)\n", rc);

		mutex_unlock(&sec->notifier_lock);
		goto exit;
	}
	mutex_unlock(&sec->notifier_lock);

	if (!secdp_get_cable_status() || !secdp_get_hpd_status() ||
			secdp_get_poor_connection_status() || !secdp_get_link_train_status()) {
		pr_info("cable is out\n");
		dex->prev = dex->curr = dex->dex_node_status = DEX_DISABLED;
		goto exit;
	}

	if (sec->hpd_noti_deferred) {
		secdp_send_deferred_hpd_noti();
		dex->prev = dex->setting_ui;
		goto exit;
	}

	if (dex->curr == dex->prev) {
		pr_info("dex is %s already\n", dex->curr ? "enabled" : "disabled");
		goto exit;
	}
	
	if (dex->curr != dex->setting_ui) {
		pr_info("values of cur(%d) and setting_ui(%d) are difference\n", dex->curr, dex->setting_ui);
		goto exit;
	}

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (run)
		secdp_bigdata_save_item(BD_DP_MODE, "DEX");
	else
		secdp_bigdata_save_item(BD_DP_MODE, "MIRROR");
#endif

	if (sec->dex.res == DEX_RES_NOT_SUPPORT) {
		pr_debug("this dongle does not support dex\n");
		goto exit;
	}

	if (!secdp_check_dex_reconnect()) {
		pr_info("not need reconnect\n");
		goto exit;
	}

	secdp_dex_do_reconnecting();

	dex->prev = run;
exit:
	return size;
}

static CLASS_ATTR_RW(dex);

static ssize_t dex_ver_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int rc;
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	struct secdp_misc *sec = sysfs->sec;
	struct secdp_dex *dex = &sec->dex;

	pr_info("branch revision: HW(0x%X), SW(0x%X, 0x%X)\n",
		dex->fw_ver[0], dex->fw_ver[1], dex->fw_ver[2]);

	rc = scnprintf(buf, PAGE_SIZE, "%02X%02X\n",
		dex->fw_ver[1], dex->fw_ver[2]);

	return rc;
}

static CLASS_ATTR_RO(dex_ver);

/* note: needs test once wifi is fixed */
static ssize_t monitor_info_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int rc = 0;
	short prod_id = 0;
	struct dp_panel *info = NULL;
	struct sde_edid_ctrl *edid_ctrl = NULL;
	struct edid *edid = NULL;

	info = secdp_get_panel_info();
	if (!info) {
		pr_err("unable to find panel info\n");
		goto exit;
	}

	edid_ctrl = info->edid_ctrl;
	if (!edid_ctrl) {
		pr_err("unable to find edid_ctrl\n");
		goto exit;
	}

	edid = edid_ctrl->edid;
	if (!edid) {
		pr_err("unable to find edid\n");
		goto exit;
	}

	pr_debug("prod_code[0]: %02x, [1]: %02x\n", edid->prod_code[0], edid->prod_code[1]);
	prod_id |= (edid->prod_code[0] << 8) | (edid->prod_code[1]);
	pr_debug("prod_id: %04x\n", prod_id);

	rc = sprintf(buf, "%s,0x%x,0x%x\n",
			edid_ctrl->vendor_id, prod_id, edid->serial); /* byte order? */

exit:
	return rc;
}

static CLASS_ATTR_RO(monitor_info);

#ifdef CONFIG_SEC_DISPLAYPORT_ENG
static ssize_t dp_forced_resolution_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	char tmp[MAX_DEX_STORE_LEN] = {0,};
	int  rc = 0, vic = 1;

	if (forced_resolution) {
		rc = scnprintf(buf, PAGE_SIZE,
			"%d: %s", forced_resolution,
			secdp_vic_to_string(forced_resolution));

	} else {
		while (secdp_vic_to_string(vic) != NULL) {
			rc += scnprintf(buf + rc, PAGE_SIZE - rc,
				"%d: %s", vic, secdp_vic_to_string(vic));
			vic++;
		}
	}

	secdp_show_hmd_dev(tmp);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "\n<< HMD >>\n%s\n", tmp);

	return rc;
}

static ssize_t dp_forced_resolution_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int val[10] = {0, };

	get_options(buf, 10, val);

	if (val[1] <= 0)
		forced_resolution = 0;
	else
		forced_resolution = val[1];

	return size;
}

static CLASS_ATTR_RW(dp_forced_resolution);

static ssize_t dp_unit_test_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int rc, cmd = sysfs->test_cmd;
	bool res = false;

	pr_info("test_cmd: %s\n", secdp_utcmd_to_str(cmd));

	switch (cmd) {
	case SECDP_UTCMD_EDID_PARSE:
		res = secdp_unit_test_edid_parse();
		break;
	default:
		pr_info("invalid test_cmd: %d\n", cmd);
		break;
	}

	rc = scnprintf(buf, 3, "%d\n", res ? 1 : 0);
	return rc;
}

static ssize_t dp_unit_test_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int val[10] = {0, };

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	get_options(buf, ARRAY_SIZE(val), val);
	sysfs->test_cmd = val[1];

	pr_info("test_cmd: %d...%s\n", sysfs->test_cmd, secdp_utcmd_to_str(sysfs->test_cmd));

exit:
	return size;
}

static CLASS_ATTR_RW(dp_unit_test);
#endif

#ifdef SECDP_SELF_TEST
struct secdp_sef_test_item g_self_test[] = {
	{DP_ENUM_STR(ST_CLEAR_CMD), .arg_cnt = 0, .arg_str = "clear all configurations"}, 
	{DP_ENUM_STR(ST_LANE_CNT), .arg_cnt = 1, .arg_str = "lane_count: 1 = 1 lane, 2 = 2 lane, 4 = 4 lane, -1 = disable"}, 
	{DP_ENUM_STR(ST_LINK_RATE), .arg_cnt = 1, .arg_str = "link_rate: 1 = 1.62G , 2 = 2.7G, 3 = 5.4G, -1 = disable"}, 
	{DP_ENUM_STR(ST_CONNECTION_TEST), .arg_cnt = 1, .arg_str = "reconnection time(sec) : range = 5 ~ 50, -1 = disable"}, 
	{DP_ENUM_STR(ST_HDCP_TEST), .arg_cnt = 1, .arg_str = "hdcp on/off time(sec): range = 5 ~ 50, -1 = disable"}, 
	{DP_ENUM_STR(ST_EDID), .arg_cnt = 0, .arg_str = "need to write edid to \"sys/class/dp_sec/dp_edid\" sysfs node, -1 = disable"}, 
	{DP_ENUM_STR(ST_PREEM_TUN), .arg_cnt = 16, .arg_str = "pre-emphasis calibration value, -1 = disable"}, 
	{DP_ENUM_STR(ST_VOLTAGE_TUN), .arg_cnt = 16, .arg_str = "voltage-level calibration value, -1 = disable"}, 
};

int secdp_self_test_status(int cmd)
{
	if (cmd >= ST_MAX)
		return -1;

	if (g_self_test[cmd].enabled)
		pr_info("%s : %s\n", g_self_test[cmd].cmd_str, g_self_test[cmd].enabled ? "true" : "false");

	return g_self_test[cmd].enabled ? g_self_test[cmd].arg_cnt : -1;
}

int *secdp_self_test_get_arg(int cmd)
{
	return g_self_test[cmd].arg;
}

void secdp_self_register_clear_func(int cmd, void (*func)(void))
{
	if (cmd >= ST_MAX)
		return;

	g_self_test[cmd].clear = func;
	pr_info("%s : clear func was registered.\n", g_self_test[cmd].cmd_str);
}

u8 *secdp_self_test_get_edid(void)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	return	sysfs->sec->self_test_edid;
}

static void secdp_self_test_reconnect_work(struct work_struct *work)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int delay = g_self_test[ST_CONNECTION_TEST].arg[0];
	static unsigned long test_cnt;

	if (!secdp_get_cable_status() || !secdp_get_hpd_status()) {
		pr_info("cable is out\n");
		test_cnt = 0;
		return;
	}

	if (sysfs->sec->self_test_reconnect_callback)
		sysfs->sec->self_test_reconnect_callback();

	test_cnt++;
	pr_info("test_cnt :%lu\n", test_cnt);

	schedule_delayed_work(&sysfs->sec->self_test_reconnect_work, msecs_to_jiffies(delay * 1000));
}

void secdp_self_test_start_reconnect(void (*func)(void))
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int delay = g_self_test[ST_CONNECTION_TEST].arg[0];

	if (delay > 50 || delay < 5)
		delay = g_self_test[ST_CONNECTION_TEST].arg[0] = 10;

	pr_info("start reconnect test : delay %d sec\n", delay);

	sysfs->sec->self_test_reconnect_callback = func;
	schedule_delayed_work(&sysfs->sec->self_test_reconnect_work, msecs_to_jiffies(delay * 1000));
}

static void secdp_self_test_hdcp_test_work(struct work_struct *work)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int delay = g_self_test[ST_HDCP_TEST].arg[0];
	static unsigned long test_cnt;

	if (!secdp_get_cable_status() || !secdp_get_hpd_status()) {
		pr_info("cable is out\n");
		test_cnt = 0;
		return;
	}

	if (sysfs->sec->self_test_hdcp_off_callback)
		sysfs->sec->self_test_hdcp_off_callback();

	msleep(3000);

	if (sysfs->sec->self_test_hdcp_on_callback)
		sysfs->sec->self_test_hdcp_on_callback();

	test_cnt++;
	pr_info("test_cnt :%lu\n", test_cnt);

	schedule_delayed_work(&sysfs->sec->self_test_hdcp_test_work, msecs_to_jiffies(delay * 1000));

}

void secdp_self_test_start_hdcp_test(void (*func_on)(void), void (*func_off)(void))
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int delay = g_self_test[ST_HDCP_TEST].arg[0];

	if (delay == 0) {
		pr_info("hdcp test is aborted\n");
		return;
	}

	if (delay > 50 || delay < 5)
		delay = g_self_test[ST_HDCP_TEST].arg[0] = 10;

	pr_info("start hdcp test : delay %d sec\n", delay);

	sysfs->sec->self_test_hdcp_on_callback = func_on;
	sysfs->sec->self_test_hdcp_off_callback = func_off;

	schedule_delayed_work(&sysfs->sec->self_test_hdcp_test_work, msecs_to_jiffies(delay * 1000));
}

static ssize_t dp_self_test_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int i, j;
	int rc = 0;

	for (i = 0; i < ST_MAX; i++) {

		rc += scnprintf(buf + rc, PAGE_SIZE - rc,
				"%d. %s : %s\n   ==>", i, g_self_test[i].cmd_str, g_self_test[i].arg_str);
		if (g_self_test[i].enabled) {
			rc += scnprintf(buf + rc, PAGE_SIZE - rc, "current value : enabled - arg :");
			for (j = 0; j < g_self_test[i].arg_cnt; j++)
				rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%x ", g_self_test[i].arg[j]);
			rc += scnprintf(buf + rc, PAGE_SIZE - rc, "\n\n");

		} else {
			rc += scnprintf(buf + rc, PAGE_SIZE - rc,
				"current value : disabled\n\n");
		}
	}

	return rc;
}

static void dp_self_test_clear_func(int cmd)
{
	int arg_cnt = g_self_test[cmd].arg_cnt < ST_ARG_CNT ? g_self_test[cmd].arg_cnt : ST_ARG_CNT;

	g_self_test[cmd].enabled = false;
	memset(g_self_test[cmd].arg, 0,	sizeof(int) * arg_cnt);

	if (g_self_test[cmd].clear)
		g_self_test[cmd].clear();
}

static ssize_t dp_self_test_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int val[ST_ARG_CNT + 2] = {0, };
	int arg, arg_cnt, cmd, i;

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto end;
	}

	get_options(buf, ARRAY_SIZE(val), val);

	cmd = val[1];
	arg = val[2];

	if (cmd < 0 || cmd >= ST_MAX) {
		pr_info("invalid cmd\n");
		goto end;
	}

	if (cmd == ST_CLEAR_CMD) {
		for (i = 1; i < ST_MAX; i++)
			dp_self_test_clear_func(i);

		pr_info("cmd : ST_CLEAR_CMD\n");
		goto end;
	}

	g_self_test[cmd].enabled = arg < 0 ? false : true;

	if (g_self_test[cmd].enabled) {
		if ((val[0] - 1) != g_self_test[cmd].arg_cnt) {
			pr_info("invalid param.\n");
			goto end;
		}
		
		arg_cnt = g_self_test[cmd].arg_cnt < ST_ARG_CNT ? g_self_test[cmd].arg_cnt : ST_ARG_CNT;
		memcpy(g_self_test[cmd].arg, val + 2, sizeof(int) * arg_cnt); 
	} else {
		dp_self_test_clear_func(cmd);
	}
	
	pr_info("cmd: %d. %s, enabled:%s\n", cmd,
				cmd < ST_MAX ? g_self_test[cmd].cmd_str : "null",
				cmd < ST_MAX ? (g_self_test[cmd].enabled ? "true" : "false"): "null");
end:
	return size;
}

static CLASS_ATTR_RW(dp_self_test);

static ssize_t dp_edid_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int i, flag = 0;
	int rc = 0;

	rc += scnprintf(buf + rc, PAGE_SIZE - rc,
			"EDID test is %s\n", g_self_test[ST_EDID].enabled ? "enabled" : "disabled");

	for (i = 0; i < ST_EDID_SIZE; i++) {

		rc += scnprintf(buf + rc, PAGE_SIZE - rc,
				"0x%02x ", sysfs->sec->self_test_edid[i]);
		if (!((i+1)%8)) {
			rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%s", flag ? "\n" : "  ");
			flag = !flag;

			if (!((i+1) % 128))
				rc += scnprintf(buf + rc, PAGE_SIZE - rc, "\n");
		}
	}

	return rc;
}

static ssize_t dp_edid_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	struct secdp_sysfs_private *sysfs = g_secdp_sysfs;
	int val[ST_EDID_SIZE + 1] = {0, };
	char *temp;
	size_t i, j = 0;

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto error;
	}

	temp = kzalloc(size, GFP_KERNEL);
	if (!temp) {
		pr_err("buffer alloc error\n");
		dp_self_test_clear_func(ST_EDID);

		goto error;
	}

	/* remove space */
	for (i = 0; i < size; i++) {
		if (buf[i] != ' ')
			temp[j++] = buf[i];
	}

	get_options(buf, ARRAY_SIZE(val), val);

	if (val[0] % 128) {
		pr_err("invalid EDID(%d)\n", val[0]);
		dp_self_test_clear_func(ST_EDID);

		goto end;
	}

	memset(sysfs->sec->self_test_edid, 0, sizeof(ST_EDID_SIZE));

	for (i = 0; i < val[0]; i++) {
		sysfs->sec->self_test_edid[i] = (u8)val[i+1];
	}

	g_self_test[ST_EDID].enabled = true;
end:
	kfree(temp);
error:
	return size;
}

static CLASS_ATTR_RW(dp_edid);
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
static ssize_t dp_error_info_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return _secdp_bigdata_show(class, attr, buf);
}

static ssize_t dp_error_info_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	return _secdp_bigdata_store(dev, attr, buf, size);
}

static CLASS_ATTR_RW(dp_error_info);
#endif

#ifdef SECDP_CALIBRATE_VXPX
static ssize_t dp_vx_lvl_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	pr_debug("+++\n");
	secdp_catalog_vx_show();
	return 0;
}

static ssize_t dp_vx_lvl_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int i, val[30] = {0, };

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	pr_debug("+++, size(%d)\n", (int)size);

	get_options(buf, ARRAY_SIZE(val), val);
	for (i = 0; i < 16; i=i+4)
		pr_debug("%02x,%02x,%02x,%02x\n", val[i+1],val[i+2],val[i+3],val[i+4]);

	secdp_catalog_vx_store(&val[1], 16);
exit:
	return size;
}

static CLASS_ATTR_RW(dp_vx_lvl);

static ssize_t dp_px_lvl_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	pr_debug("+++\n");
	secdp_catalog_px_show();
	return 0;
}

static ssize_t dp_px_lvl_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int i, val[30] = {0, };

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	pr_debug("+++, size(%d)\n", (int)size);

	get_options(buf, ARRAY_SIZE(val), val);
	for (i = 0; i < 16; i=i+4)
		pr_debug("%02x,%02x,%02x,%02x\n", val[i+1],val[i+2],val[i+3],val[i+4]);

	secdp_catalog_px_store(&val[1], 16);
exit:
	return size;
}

static CLASS_ATTR_RW(dp_px_lvl);

static ssize_t dp_pref_skip_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int skip, rc;

	pr_debug("+++\n");

	skip = secdp_debug_prefer_skip_show();
	rc = sprintf(buf, "%d\n", skip);

	return rc;
}

static ssize_t dp_pref_skip_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int i, val[30] = {0, };

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	pr_debug("+++, size(%d)\n", (int)size);

	get_options(buf, ARRAY_SIZE(val), val);
	for (i = 0; i < 16; i=i+4)
		pr_debug("%02x,%02x,%02x,%02x\n", val[i+1],val[i+2],val[i+3],val[i+4]);

	secdp_debug_prefer_skip_store(val[1]);
exit:
	return size;
}

static CLASS_ATTR_RW(dp_pref_skip);

static ssize_t dp_pref_ratio_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int ratio, rc;

	pr_debug("+++\n");

	ratio = secdp_debug_prefer_ratio_show();
	rc = sprintf(buf, "%d\n", ratio);

	return rc;
}

static ssize_t dp_pref_ratio_store(struct class *dev,
				struct class_attribute *attr, const char *buf, size_t size)
{
	int i, val[30] = {0, };

	if (secdp_check_store_args(buf, size)) {
		pr_err("args error!\n");
		goto exit;
	}

	pr_debug("+++, size(%d)\n", (int)size);

	get_options(buf, ARRAY_SIZE(val), val);
	for (i = 0; i < 16; i=i+4)
		pr_debug("%02x,%02x,%02x,%02x\n", val[i+1],val[i+2],val[i+3],val[i+4]);

	secdp_debug_prefer_ratio_store(val[1]);
exit:
	return size;
}

static CLASS_ATTR_RW(dp_pref_ratio);
#endif

enum {
	DEX = 0,
	DEX_VER,
	MONITOR_INFO,
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	DP_ERROR_INFO,
#endif
#ifdef CONFIG_SEC_FACTORY
	DP_SBU_SW_SEL,
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_ENG
	DP_FORCED_RES,
	DP_UNIT_TEST,
#endif
#ifdef SECDP_SELF_TEST
	DP_SELF_TEST,
	DP_EDID,
#endif
#ifdef SECDP_CALIBRATE_VXPX
	DP_VX_LVL,
	DP_PX_LVL,
	DP_PREF_SKIP,
	DP_PREF_RATIO,
#endif
};

static struct attribute *secdp_class_attrs[] = {
	[DEX]		= &class_attr_dex.attr,
	[DEX_VER]	= &class_attr_dex_ver.attr,
	[MONITOR_INFO]	= &class_attr_monitor_info.attr,
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	[DP_ERROR_INFO] = &class_attr_dp_error_info.attr,
#endif
#ifdef CONFIG_SEC_FACTORY
	[DP_SBU_SW_SEL]	= &class_attr_dp_sbu_sw_sel.attr,
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_ENG
	[DP_FORCED_RES]	= &class_attr_dp_forced_resolution.attr,
	[DP_UNIT_TEST]	= &class_attr_dp_unit_test.attr,
#endif
#ifdef SECDP_SELF_TEST
	[DP_SELF_TEST]	= &class_attr_dp_self_test.attr,
	[DP_EDID]	= &class_attr_dp_edid.attr,
#endif
#ifdef SECDP_CALIBRATE_VXPX
	[DP_VX_LVL]	= &class_attr_dp_vx_lvl.attr,
	[DP_PX_LVL]	= &class_attr_dp_px_lvl.attr,
	[DP_PREF_SKIP]	= &class_attr_dp_pref_skip.attr,
	[DP_PREF_RATIO]	= &class_attr_dp_pref_ratio.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(secdp_class);

struct secdp_sysfs* secdp_sysfs_init(void)
{
	struct class *dp_class;
	struct secdp_sysfs *sysfs;
	int rc = -1;

	dp_class = kzalloc(sizeof(*dp_class), GFP_KERNEL);
	if (!dp_class) {
		pr_err("fail to alloc sysfs->dp_class\n");
		goto err_exit;
	}

	dp_class->name = "dp_sec";
	dp_class->owner = THIS_MODULE;
	dp_class->class_groups = secdp_class_groups;

	rc = class_register(dp_class);
	if (rc < 0) {
		pr_err("couldn't register secdp sysfs class, rc: %d\n", rc);
		goto free_exit;
	}

	sysfs = kzalloc(sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs) {
		pr_err("fail to alloc sysfs\n");
		goto free_exit;
	}

	sysfs->dp_class = dp_class;

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_init(dp_class);
#endif

	pr_debug("success\n", rc);
	return sysfs;

free_exit:
	kzfree(dp_class);
err_exit:
	return NULL;
}

void secdp_sysfs_deinit(struct secdp_sysfs *sysfs)
{
	pr_debug("+++\n");

	if (sysfs) {
		if (sysfs->dp_class) {
			class_unregister(sysfs->dp_class);
			kzfree(sysfs->dp_class);
			sysfs->dp_class = NULL;
			pr_debug("freeing dp_class done\n");
		}

		kzfree(sysfs);
	}
}

struct secdp_sysfs *secdp_sysfs_get(struct device *dev, struct secdp_misc *sec)
{
	int rc = 0;
	struct secdp_sysfs_private *sysfs;
	struct secdp_sysfs *dp_sysfs;

	if (!dev || !sec) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	sysfs = devm_kzalloc(dev, sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs) {
		rc = -EINVAL;
		goto error;
	}

	sysfs->dev   = dev;
	sysfs->sec   = sec;
	dp_sysfs = &sysfs->dp_sysfs;

	g_secdp_sysfs = sysfs;

#ifdef SECDP_SELF_TEST
	INIT_DELAYED_WORK(&sec->self_test_reconnect_work, secdp_self_test_reconnect_work);
	INIT_DELAYED_WORK(&sec->self_test_hdcp_test_work, secdp_self_test_hdcp_test_work);
#endif
	return dp_sysfs;

error:
	return ERR_PTR(rc);
}

void secdp_sysfs_put(struct secdp_sysfs *dp_sysfs)
{
	struct secdp_sysfs_private *sysfs;

	if (!dp_sysfs)
		return;

	sysfs = container_of(dp_sysfs, struct secdp_sysfs_private, dp_sysfs);
	devm_kfree(sysfs->dev, sysfs);

	g_secdp_sysfs = NULL;
}
