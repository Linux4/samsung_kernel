// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SEC DP Sysfs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/string.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
#include "../mediatek_v2/mtk_dp.h"
#endif

#include "sec_dp_api.h"
#include "sec_dp_sysfs.h"
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include "displayport_bigdata.h"
#endif

extern int sec_dp_log_level;

#ifdef FEATURE_MANAGE_HMD_LIST
/*
 * assume that 1 HMD device has name(14),vid(4),pid(4) each, then
 * max 32 HMD devices(name,vid,pid) need 806 bytes including TAG, NUM, comba
 */
#define MAX_DEX_STORE_LEN	1024
static int dp_update_hmd_list(struct sec_dp_dev *dp, const char *buf, size_t size)
{
	int ret = 0;
	char str[MAX_DEX_STORE_LEN] = {0,};
	char *p, *tok;
	u32 num_hmd = 0;
	int j = 0;
	u32 val;

	mutex_lock(&dp->hmd_lock);

	memcpy(str, buf, size);
	p = str;

	tok = strsep(&p, ",");
	if (strncmp(DEX_TAG_HMD, tok, strlen(DEX_TAG_HMD))) {
		dp_debug("not HMD tag %s\n", tok);
		ret = -EINVAL;
		goto not_tag_exit;
	}

	tok = strsep(&p, ",");
	if (tok == NULL || *tok == 0xa/*LF*/) {
		ret = -EPERM;
		goto exit;
	}
	ret = kstrtouint(tok, 10, &num_hmd);
	if (ret || num_hmd > MAX_NUM_HMD) {
		dp_err("invalid list num %d\n", num_hmd);
		num_hmd = 0;
		ret = -EPERM;
		goto exit;
	}

	for (j = 0; j < num_hmd; j++) {
		/* monitor name */
		tok = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		strscpy(dp->hmd_list[j].monitor_name, tok, MON_NAME_LEN);

		/* VID */
		tok  = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		if (kstrtouint(tok, 16, &val)) {
			ret = -EPERM;
			break;
		}
		dp->hmd_list[j].ven_id = val;

		/* PID */
		tok  = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		if (kstrtouint(tok, 16, &val)) {
			ret = -EPERM;
			break;
		}
		dp->hmd_list[j].prod_id = val;

		dp_info("HMD%02d: %s, 0x%04x, 0x%04x\n", j,
				dp->hmd_list[j].monitor_name,
				dp->hmd_list[j].ven_id,
				dp->hmd_list[j].prod_id);
	}

exit:
	/* clear rest */
	for (; j < MAX_NUM_HMD; j++) {
		dp->hmd_list[j].monitor_name[0] = '\0';
		dp->hmd_list[j].ven_id = 0;
		dp->hmd_list[j].prod_id = 0;
	}

not_tag_exit:
	mutex_unlock(&dp->hmd_lock);

	return ret;
}
#endif

#define KEY_PROV_TAG "hdcpkey:"
static int dp_check_hdcpkey_log(const char *buf, size_t size)
{
	int tag_len = strlen(KEY_PROV_TAG);

	if (strncmp(buf, KEY_PROV_TAG, tag_len))
		return size;

	dp_info("%s\n", buf + tag_len);

	return 0;
}

#ifdef FEATURE_DEX_SUPPORT
static ssize_t dex_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct sec_dp_dev *dp = dp_get_dev();
	int ret = 0;
	enum dex_state cur_state = dp->dex.cur_state;

	dp_info("dex state:%d, setting: %d, pdic state:%d\n",
			cur_state, dp->dex.ui_setting, dp->pdic_hpd);

	if (!dp->pdic_hpd)
		cur_state = DEX_OFF;

	ret = scnprintf(buf, 8, "%d\n", cur_state);

	if (dp->dex.cur_state == DEX_RECONNECTING)
		dp->dex.cur_state = dp->dex.ui_setting;

	return ret;
}

static ssize_t dex_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct sec_dp_dev *dp = dp_get_dev();
	int val = 0;
	u32 dex_run = 0;
	u32 dex_setting = 0;
	bool need_reconnect = false;
#ifdef FEATURE_MANAGE_HMD_LIST
	int ret;

	if (size >= MAX_DEX_STORE_LEN) {
		dp_err("invalid input size %lu\n", size);
		return -EINVAL;
	}

	ret = dp_update_hmd_list(dp, buf, size);
	if (ret == 0) /* HMD list update success */
		return size;
	else if (ret != -EINVAL) /* tried to update HMD list but error*/
		return ret;
#endif

	if (!dp_check_hdcpkey_log(buf, size))
		return size;

#ifdef FEATURE_HIGHER_RESOLUTION
	if (!dp_higher_resolution(dp, buf, size))
		return size;
#endif
	if (kstrtouint(buf, 10, &val)) {
		dp_err("invalid input %s\n", buf);
		return -EINVAL;
	}

	if (val != 0x00 && val != 0x01 && val != 0x10 && val != 0x11) {
		dp_err("invalid input 0x%X\n", val);
		return -EINVAL;
	}

	dex_setting = (val & 0xF0) >> 4;
	dex_run = (val & 0x0F);

	dp_info("dex state:%d, setting:%d->%d, run:%d, hpd:%d\n",
			dp->dex.cur_state, dp->dex.ui_setting, dex_setting,
			dex_run, dp->pdic_hpd);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (dp->dp_ready_wait_state != DP_READY_YES) {
		dp_info("dp_ready_wait wakeup\n");
		dp->dp_ready_wait_state = DP_READY_YES;
		wake_up_interruptible(&dp->dp_ready_wait);
	}
#endif
#ifdef FEATURE_MANAGE_HMD_LIST
	if (dp->is_hmd_dev) {
		dp_info("HMD dev\n");
		dp->dex.ui_setting = dex_setting;
		return size;
	}
#endif
	/* if dex setting is changed, then check if reconnecting is needed */
	if (dex_setting != dp->dex.ui_setting) {
		need_reconnect = true;

		if (dex_setting == DEX_ON) {
			/* check if cur mode is the same as best dex mode */
			if (drm_mode_match(&dp->sink_info.cur_mode,
					&dp->dex.best_mode, DRM_MODE_MATCH_TIMINGS))
				need_reconnect = false;
		} else if (dex_setting == DEX_OFF) {
			/* check if cur mode is the same as best mode */
			if (drm_mode_match(&dp->sink_info.cur_mode,
					&dp->sink_info.best_mode, DRM_MODE_MATCH_TIMINGS))
				need_reconnect = false;
		}
	}

	dp->dex.ui_setting = dex_setting;

	/* reconnect if setting was mirroring(0) and dex is running(1), */
	if (dp->pdic_hpd && need_reconnect) {
		dp_info("reconnect to %s mode\n", dp->dex.ui_setting ? "dex":"mirroring");
		dp->dex.cur_state = DEX_RECONNECTING;
		dp_hpd_changed(dp, DP_HPD_LOW);
		msleep(500);
		if (dp->pdic_cable_state == PDIC_NOTIFY_ATTACH && dp->pdic_hpd != 0) {
			dp_info("retry connecting\n");
			dp_hpd_changed(dp, DP_HPD_HIGH);
		}
	}

	dp_info("dex exit: state:%d, setting:%d\n",
			dp->dex.cur_state, dp->dex.ui_setting);

	return size;
}
static CLASS_ATTR_RW(dex);

static ssize_t dex_ver_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct sec_dp_dev *dp = dp_get_dev();

	ret = scnprintf(buf, 8, "%02X%02X\n", dp->sink_info.sw_ver[0],
			dp->sink_info.sw_ver[1]);

	return ret;
}
static CLASS_ATTR_RO(dex_ver);

static ssize_t monitor_info_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct sec_dp_dev *dp = dp_get_dev();

	ret = scnprintf(buf, PAGE_SIZE, "%s,0x%x,0x%x\n",
			dp->sink_info.edid_manufacturer,
			dp->sink_info.edid_product,
			dp->sink_info.edid_serial);

	return ret;
}
static CLASS_ATTR_RO(monitor_info);
#endif /* FEATURE_DEX_SUPPORT */

static ssize_t dp_sbu_sw_sel_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct sec_dp_dev *dp = dp_get_dev();
	int val[3] = {0,};
	int aux_sw_sel, aux_sw_oe;

	if (strnchr(buf, size, '-')) {
		dp_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 3, val);

	aux_sw_sel = val[1];
	aux_sw_oe = val[2];
	/* oe: 0 is on, 1 is off, sel: 0 is CC1, 1 is CC2 */
	dp_info("sbu_sw_sel(%d), sbu_sw_oe(%d)\n", aux_sw_sel, aux_sw_oe);

	if ((aux_sw_sel == 0 || aux_sw_sel == 1) && (aux_sw_oe == 0 || aux_sw_oe == 1))
		dp_aux_onoff(dp, !aux_sw_oe, aux_sw_sel);
	else
		dp_err("invalid aux switch parameter\n");

	return size;
}
static CLASS_ATTR_WO(dp_sbu_sw_sel);

#ifdef CONFIG_SEC_DISPLAYPORT_DBG
static ssize_t edid_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct sec_dp_dev *dp = dp_get_dev();
	u8 *edid_buf = dp->edid_buf;
	u32 blk_cnt;
	int i;
	ssize_t size = 0;

	blk_cnt = edid_buf[0x7e] + 1;

	if (dp->test_edid_size == 0)
		return sprintf(buf, "Test EDID not ready\n");

	if (blk_cnt < 1 || blk_cnt > 4)
		return sprintf(buf, "invalid edid data\n");

	for (i = 0; i <= blk_cnt * 128; i++) {
		size += sprintf(buf + size, " %02X", edid_buf[i]);
		if (i % 16 == 15)
			size += sprintf(buf + size, "\n");
		else if (i % 8 == 7)
			size += sprintf(buf + size, " ");
	}

	return size;
}

static ssize_t edid_test_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct sec_dp_dev *dp = dp_get_dev();
	u8 *edid_buf = dp->edid_buf;
	int buf_size = sizeof(dp->edid_buf);
	int i;
	int edid_idx = 0, hex_cnt = 0, buf_idx = 0;
	u8 hex = 0;
	u8 temp;
	int max_size = (buf_size * 6); /* including ',' ' ' and prefix ', 0xFF' */


	dp->test_edid_size = 0;
	memset(edid_buf, 0, buf_size);

	for (i = 0; i < size && i < max_size; i++) {
		temp = *(buf + buf_idx++);
		/* value is separated by comma, space or line feed*/
		if (temp == ',' || temp == ' ' || temp == '\x0A') {
			if (hex_cnt != 0)
				edid_buf[edid_idx++] = hex;
			hex = 0;
			hex_cnt = 0;
		} else if (hex_cnt == 0 && temp == '0') {
			hex_cnt++;
			continue;
		} else if (temp == 'x' || temp == 'X') {
			hex_cnt = 0;
			hex = 0;
			continue;
		} else if (!temp || temp == '\0') { /* EOL */
			dp_info("parse end. edid cnt: %d\n", edid_idx);
			break;
		} else if (temp >= '0' && temp <= '9') {
			hex = (hex << 4) + (temp - '0');
			hex_cnt++;
		} else if (temp >= 'a' && temp <= 'f') {
			hex = (hex << 4) + (temp - 'a') + 0xa;
			hex_cnt++;
		} else if (temp >= 'A' && temp <= 'F') {
			hex = (hex << 4) + (temp - 'A') + 0xa;
			hex_cnt++;
		} else {
			dp_info("invalid value %c, %d\n", temp, hex_cnt);
			return size;
		}

		if (hex_cnt > 2 || edid_idx > buf_size + 1) {
			dp_info("wrong input. %d, %d, [%c]\n", hex_cnt, edid_idx, temp);
			return size;
		}
	}

	if (hex_cnt > 0)
		edid_buf[edid_idx] = hex;

	if (edid_idx > 0 && edid_idx % 128 == 0)
		dp->test_edid_size = edid_idx;

	dp_info("edid size = %d\n", edid_idx);

	return size;
}
static CLASS_ATTR_RW(edid_test);

static ssize_t dp_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int size;

	size = snprintf(buf, PAGE_SIZE, "0: log level\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "1: HDCP enable at connect\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "add test\n");

	return size;
}

static ssize_t dp_test_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct sec_dp_dev *dp = dp_get_dev();
	u32 val[8] = {0,};

	if (strnchr(buf, size, '-')) {
		dp_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	if (!strncmp(buf, "uevent_enable", 13)) {
		dp_enable_uevent(1);
		dp_info("debug uevent enabled\n");
		return size;
	} else if (!strncmp(buf, "uevent_disable", 14)) {
		dp_enable_uevent(0);
		dp_info("debug uevent disabled\n");
		return size;
	}

	get_options(buf, 7, (int *)val);
	dp_info("%s %d, %d, %d\n", __func__, val[0], val[1], val[2]);

	switch (val[1]) {
	case 0:
		sec_dp_log_level = val[2];
		dp_info("dp log level = %d\n", val[2]);
		break;
	case 1:
		dp->hdcp_enable_connect = !!val[2];
		dp_info("hdcp enable at connection feature %s\n",
				dp->hdcp_enable_connect ? "set" : "unset" );
		break;
	default:
		break;
	}

	return size;
}
static CLASS_ATTR_RW(dp_test);

extern void mtk_dp_phy_print(void);
extern struct DPTX_PHY_PARAMETER *mtk_dp_get_phy_param(void);
static ssize_t phy_tune_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct DPTX_PHY_PARAMETER *phy_params = mtk_dp_get_phy_param();
	int size = 0;
	int i;
	char *phy_names[10] = {
		"L0P0", "L0P1", "L0P2", "L0P3", "L1P0",
		"L1P1", "L1P2", "L2P0", "L2P1", "L3P0"};
	u32 dts_data[6] = {0x0, };

	mtk_dp_phy_print();

	size = snprintf(buf, size, "PHY INFO:\n");
	for (i = 0; i < DPTX_PHY_LEVEL_COUNT; i++) {
		size += snprintf(buf + size, PAGE_SIZE - size,
			"[%d](%s):C0 = 0x%02x, CP1 = 0x%02x\n",
			i, phy_names[i], phy_params[i].C0, phy_params[i].CP1);

		dts_data[i/4] |= (phy_params[i].C0 << (8*(i%4)));
		dts_data[i/4 + 3] |= (phy_params[i].CP1 << (8*(i%4)));
	}

	size += snprintf(buf + size, PAGE_SIZE - size, "\n");

	for (i = 0; i < 6; i++)
		size += snprintf(buf + size, PAGE_SIZE - size, "0x%08x ", dts_data[i]);

	size += snprintf(buf + size, PAGE_SIZE - size,
			"\n\n usage) echo [index],[C0],[CP1] > phy_tune\n");

	return size;
}

static ssize_t phy_tune_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct DPTX_PHY_PARAMETER *phy_params = mtk_dp_get_phy_param();
	u32 val[8] = {0,};

	if (strnchr(buf, size, '-')) {
		dp_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 4, (int *)val);
	dp_info("%s %d, %d, 0x%X, 0x%X\n", __func__, val[0], val[1], val[2], val[3]);

	if (val[0] != 3)
		return size;

	if (val[1] < 0 || val[1] >= 10)
		return size;

	phy_params[val[1]].C0 = (u8)val[2];
	phy_params[val[1]].CP1 = (u8)val[3];

	return size;
}
static CLASS_ATTR_RW(phy_tune);

static ssize_t aux_tune_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct sec_dp_dev *dp = dp_get_dev();
	int size = 0;

	size = snprintf(buf, PAGE_SIZE, "Current level: 0x%x\n", dp->aux_level);

	size += snprintf(buf + size, PAGE_SIZE - size,
			"\n( 0x7=200mv, 0x8=300mV, 0x9=400mV, 0xa=500mv, 0xb=600mv, ~ 0xf)\n");
	size += snprintf(buf + size, PAGE_SIZE - size,
			" usage) echo 0x7 > aux_tune\n");

	return size;
}

static ssize_t aux_tune_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct sec_dp_dev *dp = dp_get_dev();
	u32 val[2] = {0,};

	if (strnchr(buf, size, '-')) {
		dp_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 2, (int *)val);

	if (val[1] < 0 || val[1] > 0xf)
		return size;

	dp->aux_level = val[1];
	dp_info("new aux level: 0x%x\n", dp->aux_level);

	return size;
}
static CLASS_ATTR_RW(aux_tune);

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
static ssize_t redriver_tune_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int size = 0;
	u8 (*tuning_table)[PHY_SWING_MAX][PHY_PREEMPHASIS_MAX];
	const char *tune_name[DP_TUNE_MAX] = {
		"RBR-eq0",
		"RBR-eq1",
		"HBR-eq0",
		"HBR-eq1",
		"HBR2-eq0",
		"HBR2-eq1",
		"HBR3-eq0",
		"HBR3-eq1"
	};
	int i, j;

	tuning_table = dp_get_redriver_tuning_table();
	for (i = 0; i < DP_TUNE_MAX; i++) {
		dp_info("[%d]%s\n", i, tune_name[i]);
		size += snprintf(buf + size, PAGE_SIZE, "[%d]%s\n", i, tune_name[i]);
		for (j = 0; j < PHY_SWING_MAX; j++) {
			dp_info(" swing%d: 0x%02x 0x%02x 0x%02x 0x%02x\n", j,
				tuning_table[i][j][0], tuning_table[i][j][1],
				tuning_table[i][j][2], tuning_table[i][j][3]);
			size += snprintf(buf + size, PAGE_SIZE,
					" swing[%d]: 0x%02x 0x%02x 0x%02x 0x%02x\n", j,
					tuning_table[i][j][0], tuning_table[i][j][1],
					tuning_table[i][j][2], tuning_table[i][j][3]);
		}
	}

	size += snprintf(buf + size, PAGE_SIZE,
			"\necho 'linkrate,swing,val1,val2,val3,val4' > redriver_tune\n");
	size += snprintf(buf + size, PAGE_SIZE,
			" ex) hbr2-eq0 swing2 => echo '4,2,0x6,0x6,0x6,0x6' > redriver_tune\n");

	return size;
}

static ssize_t redriver_tune_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	u32 val[8] = {0,};
	u8 (*tuning_table)[PHY_SWING_MAX][PHY_PREEMPHASIS_MAX];

	if (strnchr(buf, size, '-')) {
		dp_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 7, (int *)val);

	dp_info("%s %d, %d, %d\n", __func__, val[0], val[1], val[2]);
	if (val[0] != 6)
		return size;

	if (val[1] >= DP_TUNE_MAX || val[2] >= PHY_SWING_MAX)
		return size;

	tuning_table = dp_get_redriver_tuning_table();

	tuning_table[val[1]][val[2]][0] = val[3];
	tuning_table[val[1]][val[2]][1] = val[4];
	tuning_table[val[1]][val[2]][2] = val[5];
	tuning_table[val[1]][val[2]][3] = val[6];

	return size;
}
static CLASS_ATTR_RW(redriver_tune);
#endif
#endif //CONFIG_SEC_DISPLAYPORT_DBG

int dp_create_sysfs(void)
{
	int ret = 0;
	struct class *dp_class;

	dp_class = class_create(THIS_MODULE, "dp_sec");
	if (IS_ERR(dp_class)) {
		dp_err("failed to creat dp_class\n");
		return PTR_ERR(dp_class);
	}
#ifdef FEATURE_DEX_SUPPORT
	ret = class_create_file(dp_class, &class_attr_dex);
	if (ret)
		dp_err("failed to create dp_dex\n");
	ret = class_create_file(dp_class, &class_attr_dex_ver);
	if (ret)
		dp_err("failed to create dp_dex_ver\n");
	ret = class_create_file(dp_class, &class_attr_monitor_info);
	if (ret)
		dp_err("failed to create dp_monitor_info\n");
#endif
	ret = class_create_file(dp_class, &class_attr_dp_sbu_sw_sel);
	if (ret)
		dp_err("failed to create dp_sbu_sw_sel\n");

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_init(dp_class);
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_DBG
	ret = class_create_file(dp_class, &class_attr_edid_test);
	if (ret)
		dp_err("failed to create attr_edid\n");
	ret = class_create_file(dp_class, &class_attr_phy_tune);
	if (ret)
		dp_err("failed to create phy_tune\n");
	ret = class_create_file(dp_class, &class_attr_aux_tune);
	if (ret)
		dp_err("failed to create aux_tune\n");
#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
	ret = class_create_file(dp_class, &class_attr_redriver_tune);
	if (ret)
		dp_err("failed to create redriver_tune\n");
#endif
	ret = class_create_file(dp_class, &class_attr_dp_test);
	if (ret)
		dp_err("failed to create dp_test\n");
#endif

	dp_info("dp sysfs create done\n");

	return ret;
}
