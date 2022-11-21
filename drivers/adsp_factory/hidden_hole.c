/*
*  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dirent.h>
#include "adsp.h"

#define DATAREAD_TIMER_MS		200
#define DATAREAD_TIMER_MARGIN_MS	20
#define PREDEFINE_FILE_PATH	"/efs/FactoryApp/predefine"
#define WIN_TYPE_LEN	50
#define COEF_MAX	2
#define RETRY_MAX	3
#define OCTAS_START_INDEX	7
#define COEF_VERSION_LEN	20
#define COEF_VALUES_LEN		80
#define EFS_SAVE_NUMS		11

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

static int read_window_type(void)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	int iRet = 0;
	char window_type[10] = {0, };

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open("/sys/class/lcd/panel/window_type",
		O_RDONLY, 0440);
	if (IS_ERR(type_filp)) {
		iRet = PTR_ERR(type_filp);
		pr_err("[FACTORY] %s: open fail window_type:%d\n",
			__func__, iRet);
		goto err_open_exit;
	}

	iRet = vfs_read(type_filp, (char *)window_type,
		10 * sizeof(char), &type_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, iRet);
		iRet = -EIO;
		goto err_read_exit;
	}

	pr_info("%s - 0x%x, 0x%x, 0x%x", __func__,
		window_type[0], window_type[1], window_type[2]);
	iRet = (window_type[1] - '0') & 0x0f;

err_read_exit:
	filp_close(type_filp, current->files);
err_open_exit:
	set_fs(old_fs);

	return iRet;
}

struct related_light {
	int dgf;
	int r_coef;
	int g_coef;
	int b_coef;
	int c_coef;
	int cct_coef;
	int cct_off;
	int th_high;
	int th_low;
	int irisprox_th;
	int sum_crc;
};

enum {
	ID_UTYPE = 0,
	ID_BLACK = 1,
	ID_WHITE = 2,
	ID_GOLD = 3,
	ID_SILVER = 4,
	ID_GREEN = 5,
	ID_BLUE = 6,
	ID_PINKGOLD = 7,
	ID_DEFUALT,
} COLOR_ID_INDEX;

struct {
	int version;
	int octa_id;
	struct related_light rlight;
} light_coef_predefine_table[COEF_MAX] = {
	/* version, octa, dgf, r, g, b, c, ct, ct offset, th_h, th_l, sum_crc */
	{170427, ID_UTYPE,
		{2410, -35, -55, -275, 1000, 1198, 1418, 2100, 900, 700, 9361}},
	{170427, ID_BLACK,
		{2410, -35, -55, -275, 1000, 1198, 1418, 2100, 900, 700, 9361}},
};

static char *tmd90x_strtok_first_dot(char *str)
{
	static char *s;
	int i, len;

	if (str == NULL || *str == '\0')
		return NULL;

	s = str;
	len = (int)strlen(str);
	for (i = 0 ; i < len; i++) {
		if (s[i] == '.') {
			s[i] = '\0';
			return s;
		}
	}

	return s;
}

static int need_update_coef_efs(void)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	int iRet = 0, current_coef_version = 0;
	char coef_version[COEF_VERSION_LEN] = {0, };
	char *temp_version;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open("/efs/FactoryApp/hh_version", O_RDONLY, 0440);
	if (PTR_ERR(type_filp) == -ENOENT || PTR_ERR(type_filp) == -ENXIO) {
		pr_err("[FACTORY] %s : no version file\n", __func__);
		set_fs(old_fs);
		return true;
	} else if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(type_filp);
		pr_err("[FACTORY] %s: open fail version:%d\n", __func__, iRet);
		return iRet;
	}

	iRet = vfs_read(type_filp, (char *)coef_version,
		COEF_VERSION_LEN * sizeof(char), &type_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, iRet);
		iRet = -EIO;
	}

	filp_close(type_filp, current->files);
	set_fs(old_fs);

	temp_version = tmd90x_strtok_first_dot(coef_version);
	if (temp_version == NULL) {
		pr_err("[FACTORY] %s : Dot NULL.\n", __func__);
		return false;
	}

	iRet = kstrtoint(temp_version, 10, &current_coef_version);
	pr_info("[FACTORY] %s: %s,%s,%d\n",
		__func__, coef_version, temp_version, current_coef_version);

	if (iRet < 0) {
		pr_err("[FACTORY] %s : kstrtoint failed:%d\n", __func__, iRet);
		return iRet;
	}

	if (current_coef_version < light_coef_predefine_table[COEF_MAX-1].version) {
		pr_err("[FACTORY] %s : small:%d:%d", __func__, current_coef_version,
			light_coef_predefine_table[COEF_MAX-1].version);
		return true;
	}

	return false;
}

int check_crc_table(int index)
{
	struct related_light *light;

	light = &light_coef_predefine_table[index].rlight;

	return ((light->dgf + light->r_coef + light->g_coef + light->b_coef +
		light->c_coef + light->cct_coef + light->cct_off +
		light->th_high + light->th_low + light->irisprox_th)
		== light->sum_crc) ? true:false;
}

int make_coef_efs(int index)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	int iRet = 0;
	char predefine_value_path[WIN_TYPE_LEN+1];
	char light_values[COEF_VALUES_LEN] = {0, };

	snprintf(predefine_value_path, WIN_TYPE_LEN,
		"/efs/FactoryApp/predefine%d",
		light_coef_predefine_table[index].octa_id);

	pr_info("[FACTORY] %s: path:%s\n", __func__,
		predefine_value_path);

	sprintf(light_values, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		light_coef_predefine_table[index].rlight.dgf,
		light_coef_predefine_table[index].rlight.r_coef,
		light_coef_predefine_table[index].rlight.g_coef,
		light_coef_predefine_table[index].rlight.b_coef,
		light_coef_predefine_table[index].rlight.c_coef,
		light_coef_predefine_table[index].rlight.cct_coef,
		light_coef_predefine_table[index].rlight.cct_off,
		light_coef_predefine_table[index].rlight.th_high,
		light_coef_predefine_table[index].rlight.th_low,
		light_coef_predefine_table[index].rlight.irisprox_th,
		light_coef_predefine_table[index].rlight.sum_crc);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open(predefine_value_path,
		O_TRUNC | O_RDWR | O_CREAT, 0660);

	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(type_filp);
		pr_err("[FACTORY] %s: open fail predefine_value_path:%d\n",
			__func__, iRet);
		return iRet;
	}

	iRet = vfs_write(type_filp,
		(char *)light_values,
		COEF_VALUES_LEN * sizeof(char), &type_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd write %d\n", __func__, iRet);
		iRet = -EIO;
	}

	filp_close(type_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

static void tmd490x_itoa(char *buf, int v)
{
	int mod[10];
	int i;

	for (i = 0; i < 3; i++) {
		mod[i] = (v % 10);
		v = v / 10;
		if (v == 0)
			break;
	}

	if (i == 3)
		i--;

	if (i >= 1)
		*buf = (char) ('a' + mod[0]);
	else
		*buf = (char) ('0' + mod[0]);

	buf++;
	*buf = '\0';
}

int update_coef_version(void)
{
	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	char version[COEF_VERSION_LEN] = {0,};
	char tmp[5] = {0,};
	int i = 0, iRet = 0;

	sprintf(version, "%d.",
		light_coef_predefine_table[COEF_MAX - 1].version);

	for (i = 0 ; i < COEF_MAX ; i++) {
		if (check_crc_table(i)) {
			tmd490x_itoa(tmp, light_coef_predefine_table[i].octa_id);
			strncat(version, tmp, 1);
			pr_err("[FACTORY] %s: version:%s\n", __func__, version);
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open("/efs/FactoryApp/hh_version",
		O_TRUNC | O_RDWR | O_CREAT, 0660);
	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(type_filp);
		pr_err("[FACTORY] %s: open fail version:%d\n", __func__, iRet);
		return iRet;
	}

	iRet = vfs_write(type_filp, (char *)version,
		COEF_VERSION_LEN * sizeof(char), &type_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd write fail:%d\n", __func__, iRet);
		iRet = -EIO;
	}

	filp_close(type_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

void hidden_hole_data_read(struct adsp_data *data)
{
	struct file *hole_filp = NULL;
	struct hidden_hole_data rlight;
	char light_values[COEF_VALUES_LEN] = {0, };
	struct msg_data_hidden_hole message;
	struct msg_data chown_msg;
	mm_segment_t old_fs;
	int iRet = 0, win_type = 0, i;
	char predefine_value_path[WIN_TYPE_LEN+1];

	if (need_update_coef_efs()) {
		for (i = 0 ; i < COEF_MAX ; i++) {
			if (!check_crc_table(i)) {
				pr_err("[FACTORY] %s: CRC check fail (%d)\n",
					__func__, i);
				iRet = -1;
			}
		}

		if (iRet == 0) {
			for (i = 0 ; i < COEF_MAX ; i++) {
			iRet = make_coef_efs(i);
			if (iRet < 0)
				pr_err("[FACTORY] %s: NUCE fail:%d\n",
				__func__, i);
		}
		update_coef_version();
		} else {
			pr_err("[FACTORY] %s: can't not update/make coef_efs\n",
				__func__);
		}
		chown_msg.sensor_type = ADSP_FACTORY_LIGHT;
		adsp_unicast(&chown_msg, sizeof(chown_msg),
			NETLINK_MESSAGE_HIDDEN_HOLE_CHANGE_OWNER, 0, 0);
		msleep(DATAREAD_TIMER_MS + DATAREAD_TIMER_MARGIN_MS);
	}

	win_type = read_window_type();
	if (win_type < 0 || win_type >= COEF_MAX) {
		pr_err("[FACTORY] %s: win_type is invalid value\n", __func__);
		win_type = ID_UTYPE;
	}

	snprintf(predefine_value_path, WIN_TYPE_LEN,
		"/efs/FactoryApp/predefine%d", win_type);

	pr_info("[FACTORY] %s: win_type:%d, %s\n",
		__func__, win_type, predefine_value_path);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	hole_filp = filp_open(predefine_value_path, O_RDONLY, 0440);
	if (IS_ERR(hole_filp)) {
		iRet = PTR_ERR(hole_filp);
		pr_err("[FACTORY] %s - Can't open hidden hole file:%d\n",
			__func__, iRet);
		set_fs(old_fs);
		goto err_exit;
	}

	iRet = vfs_read(hole_filp, (char *)&light_values,
		COEF_VALUES_LEN * sizeof(char), &hole_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, iRet);
		filp_close(hole_filp, current->files);
		set_fs(old_fs);
		goto err_exit;
	}

	filp_close(hole_filp, current->files);
	set_fs(old_fs);

	iRet = sscanf(light_values, "%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d",
		&rlight.d_factor, &rlight.r_coef, &rlight.g_coef,
		&rlight.b_coef, &rlight.c_coef, &rlight.ct_coef,
		&rlight.ct_offset, &rlight.th_high, &rlight.th_low,
		&rlight.irisprox_th, &rlight.sum_crc);
	if (iRet != EFS_SAVE_NUMS) {
		pr_err("[FACTORY] %s: sscanf fail:%d\n", __func__, iRet);
		goto err_exit;
	}

	message.sensor_type = ADSP_FACTORY_LIGHT;
	message.d_factor = rlight.d_factor;
	message.r_coef = rlight.r_coef;
	message.g_coef = rlight.g_coef;
	message.b_coef = rlight.b_coef;
	message.c_coef = rlight.c_coef;
	message.ct_coef = rlight.ct_coef;
	message.ct_offset = rlight.ct_offset;
	message.th_high = rlight.th_high;
	message.th_low = rlight.th_low;
	message.irisprox_th = rlight.irisprox_th;
	pr_info("[FACTORY] %s: message:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		__func__, message.d_factor, message.r_coef, message.g_coef,
		message.b_coef, message.c_coef, message.ct_coef,
		message.ct_offset, message.th_high, message.th_low,
		message.irisprox_th);

	msleep(DATAREAD_TIMER_MS + DATAREAD_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_HIDDEN_HOLE_WRITE_DATA, 0, 0);

err_exit:
	return;
}

static int tmd490x_hh_check_crc(void)
{
	struct file *hole_filp = NULL;
	struct hidden_hole_data message;
	char light_values[COEF_VALUES_LEN] = {0, };
	mm_segment_t old_fs;
	int i, sum, iRet = 0;
	char predefine_value_path[WIN_TYPE_LEN+1];
	char efs_version[COEF_VERSION_LEN] = {0, };

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	hole_filp = filp_open("/efs/FactoryApp/hh_version", O_RDONLY, 0440);
	if (IS_ERR(hole_filp)) {
		pr_info("[FACTORY] %s: open fail\n", __func__);
		iRet = PTR_ERR(hole_filp);
		goto crc_err_open_ver;
	}

	iRet = vfs_read(hole_filp, (char *)&efs_version,
		COEF_VERSION_LEN * sizeof(char), &hole_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, iRet);
		goto crc_err_read_ver;
	}

	pr_info("[FACTORY] %s: efs_version:%s\n", __func__, efs_version);

	filp_close(hole_filp, current->files);
	set_fs(old_fs);

	i = OCTAS_START_INDEX;
	while (efs_version[i] >= '0' && efs_version[i] <= 'f') {
		if (efs_version[i] >= 'a')
			snprintf(predefine_value_path, WIN_TYPE_LEN,
				"/efs/FactoryApp/predefine%d",
				efs_version[i] - 'a' + 10);
		else
			snprintf(predefine_value_path, WIN_TYPE_LEN,
				"/efs/FactoryApp/predefine%d",
				efs_version[i] - '0');
		pr_info("[FACTORY] %s: path:%s\n",
			__func__, predefine_value_path);

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		hole_filp = filp_open(predefine_value_path, O_RDONLY, 0440);
		if (IS_ERR(hole_filp)) {
			set_fs(old_fs);
			iRet = PTR_ERR(hole_filp);
			pr_err("%s - Can't open hidden hole file:%d\n",
				__func__, iRet);
			goto crc_err_open;
		}

		iRet = vfs_read(hole_filp, (char *)light_values,
			COEF_VALUES_LEN * sizeof(char), &hole_filp->f_pos);
		if (iRet < 0) {
			pr_err("[FACTORY] %s: fd read fail:%d\n",
				__func__, iRet);
			goto crc_err_read;
		}

		filp_close(hole_filp, current->files);
		set_fs(old_fs);

		iRet = sscanf(light_values, "%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d",
			&message.d_factor, &message.r_coef, &message.g_coef,
			&message.b_coef, &message.c_coef, &message.ct_coef,
			&message.ct_offset, &message.th_high, &message.th_low,
			&message.irisprox_th, &message.sum_crc);
		if (iRet != EFS_SAVE_NUMS) {
			pr_err("[FACTORY] %s: sscanf fail:%d\n",
				__func__, iRet);
			goto crc_err_sum;
		}

		sum = message.d_factor + message.r_coef + message.g_coef +
			message.b_coef + message.c_coef + message.ct_coef +
			message.ct_offset + message.th_high + message.th_low +
			message.irisprox_th;
		if (sum != message.sum_crc) {
			pr_err("[FACTORY] %s: CRC error %d:%d\n",
				__func__, sum, message.sum_crc);
			iRet = -1;
			goto crc_err_sum;
		}
		i++;
	}

	return iRet;
crc_err_read_ver:
crc_err_read:
	filp_close(hole_filp, current->files);
crc_err_open_ver:
crc_err_open:
	set_fs(old_fs);
crc_err_sum:
	return iRet;
}

static ssize_t tmd490x_hh_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	struct file *type_filp = NULL;
	mm_segment_t old_fs;
	int iRet = 0;
	char *temp_char;

	temp_char = (char *)&buf[1];
	pr_info("[FACTORY] %s: buf:%s:\n", __func__, buf);
	pr_info("[FACTORY] %s: temp_char:%s:\n", __func__, temp_char);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open("/efs/FactoryApp/hh_version",
		O_TRUNC | O_RDWR | O_CREAT, 0660);
	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(type_filp);
		pr_err("[FACTORY] %s: open fail version:%d\n", __func__, iRet);
		return size;
	}

	iRet = vfs_write(type_filp, (char *)temp_char,
		COEF_VERSION_LEN * sizeof(char), &type_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd write fail:%d\n", __func__, iRet);
		iRet = -EIO;
	}

	filp_close(type_filp, current->files);
	set_fs(old_fs);

	return size;

}
static ssize_t tmd490x_hh_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct file *ver_filp = NULL;
	mm_segment_t old_fs;
	char efs_version[COEF_VERSION_LEN] = {0, };
	char table_version[COEF_VERSION_LEN] = {0, };
	char tmp[5] = {0,};
	int i = 0, iRet = 0;

	iRet = tmd490x_hh_check_crc();
	if (iRet < 0) {
		pr_err("[FACTORY] %s: CRC check error:%d\n", __func__, iRet);
		goto err_check_crc;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ver_filp = filp_open("/efs/FactoryApp/hh_version", O_RDONLY, 0440);
	if (IS_ERR(ver_filp)) {
		pr_err("[FACTORY] %s: open fail\n", __func__);
		goto err_open_fail;
	}

	iRet = vfs_read(ver_filp, (char *)&efs_version,
		COEF_VERSION_LEN * sizeof(char), &ver_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, iRet);
		goto err_fail;
	}

	filp_close(ver_filp, current->files);
	set_fs(old_fs);

	sprintf(table_version, "%d.",
		light_coef_predefine_table[COEF_MAX - 1].version);

	for (i = 0 ; i < COEF_MAX ; i++) {
		tmd490x_itoa(tmp, light_coef_predefine_table[i].octa_id);
		strncat(table_version, tmp, 1);
		pr_err("[FACTORY] %s: version:%s\n", __func__, table_version);
	}

	pr_info("[FACTORY] %s: ver:%s:%s\n",
		__func__, efs_version, table_version);

	return snprintf(buf, PAGE_SIZE, "P%s,P%s\n",
		efs_version, table_version);
err_fail:
	filp_close(ver_filp, current->files);
err_open_fail:
	set_fs(old_fs);
err_check_crc:
	pr_info("[FACTORY] %s: fail\n", __func__);
	return snprintf(buf, PAGE_SIZE, "0,0\n");
}

static ssize_t tmd490x_hh_write_all_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct file *type_filp = NULL;
	struct hidden_hole_data message;
	mm_segment_t old_fs;
	char predefine_value_path[WIN_TYPE_LEN+1];
	int iRet = 0, octa_id = 0;
	char light_values[COEF_VALUES_LEN] = {0, };

	iRet = sscanf(buf, "%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d,%10d",
		&octa_id, &message.d_factor, &message.r_coef, &message.g_coef,
		&message.b_coef, &message.c_coef, &message.ct_coef,
		&message.ct_offset, &message.th_high, &message.th_low,
		&message.irisprox_th, &message.sum_crc);
	if (iRet != EFS_SAVE_NUMS + 1) {
		pr_err("[FACTORY] %s: sscanf fail:%d\n", __func__, iRet);
		return size;
	}

	pr_info("[FACTORY] %s:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", __func__,
		octa_id, message.d_factor, message.r_coef, message.g_coef,
		message.b_coef, message.c_coef, message.ct_coef,
		message.ct_offset, message.th_high, message.th_low,
		message.irisprox_th, message.sum_crc);

	sprintf(light_values, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		message.d_factor, message.r_coef, message.g_coef,
		message.b_coef, message.c_coef, message.ct_coef,
		message.ct_offset, message.th_high, message.th_low,
		message.irisprox_th, message.sum_crc);

	snprintf(predefine_value_path, WIN_TYPE_LEN,
		"/efs/FactoryApp/predefine%d", octa_id);

	pr_info("[FACTORY] %s: path:%s\n", __func__, predefine_value_path);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	type_filp = filp_open(predefine_value_path,
		O_TRUNC | O_RDWR | O_CREAT, 0660);
	if (IS_ERR(type_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(type_filp);
		pr_err("[FACTORY] %s: open fail predefine_value_path:%d\n",
			__func__, iRet);
		return size;
	}

	iRet = vfs_write(type_filp, (char *)light_values,
		COEF_VALUES_LEN * sizeof(char), &type_filp->f_pos);
	if (iRet < 0) {
		pr_err("[FACTORY] %s: fd write:%d\n", __func__, iRet);
		iRet = -EIO;
	}

	filp_close(type_filp, current->files);
	set_fs(old_fs);

	return size;
}

static ssize_t tmd490x_hh_write_all_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet = 0;
	int i;

	for (i = 0; i < COEF_MAX ; i++) {
		if (!check_crc_table(i)) {
			pr_err("[FACTORY] %s: CRC check fail (%d)\n",
				__func__, i);

			return snprintf(buf, PAGE_SIZE, "%s\n", "FALSE");
		}
	}

	for (i = 0; i < COEF_MAX ; i++) {
		iRet = make_coef_efs(i);
		if (iRet < 0)
			pr_err("[FACTORY] %s: make_coef_efs fail:%d\n",
			__func__, i);
	}

	iRet = tmd490x_hh_check_crc();
	pr_info("[FACTORY] %s: success to write all data:%d\n", __func__, iRet);

	if (iRet < 0)
		return snprintf(buf, PAGE_SIZE, "%s\n", "FALSE");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "TRUE");
}

static ssize_t tmd490x_hh_is_exist_efs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct file *hole_filp = NULL;
	mm_segment_t old_fs;
	char predefine_value_path[WIN_TYPE_LEN+1];
	int retry_cnt = 0, win_type;
	bool is_exist = false;

	win_type = read_window_type();
	if (win_type < 0 || win_type >= COEF_MAX) {
		pr_err("[FACTORY] %s: win_type is invalid value\n", __func__);
		win_type = ID_UTYPE;
	}		

	snprintf(predefine_value_path, WIN_TYPE_LEN,
		"/efs/FactoryApp/predefine%d", win_type);

	pr_info("[FACTORY] %s: win:%d:%s\n",
		__func__, win_type, predefine_value_path);
	old_fs = get_fs();
	set_fs(KERNEL_DS);

retry_open_efs:
	hole_filp = filp_open(predefine_value_path, O_RDONLY, 0440);

	if (IS_ERR(hole_filp)) {
		pr_err("[FACTORY] %s: open fail fail:%d\n",
			__func__, IS_ERR(hole_filp));
		if (retry_cnt < RETRY_MAX) {
			retry_cnt++;
			goto retry_open_efs;
		} else
			is_exist = false;
	} else {
		filp_close(hole_filp, current->files);
		is_exist = true;
	}

	set_fs(old_fs);

	pr_info("[FACTORY] %s: is_exist:%d, retry :%d\n",
		__func__, is_exist, retry_cnt);

	if (is_exist)
		return snprintf(buf, PAGE_SIZE, "%s\n", "TRUE");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "FALSE");
}

static ssize_t tmd490x_hh_ext_prox_th_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int threshold = 0;
	int win_type;

	if (kstrtoint(buf, 10, &threshold)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return size;
	}

	win_type = read_window_type();
	if (win_type < 0 || win_type >= COEF_MAX) {
		pr_err("[FACTORY] %s: win_type read fail\n", __func__);
		return size;
	}

	pr_info("[FACTORY] %s: win_type:%d, thd:%d\n",
		__func__, win_type, threshold);
	light_coef_predefine_table[win_type].rlight.irisprox_th = threshold;

	return size;
}

static ssize_t tmd490x_hh_ext_prox_th_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int win_type;

	win_type = read_window_type();
	if (win_type < 0 || win_type >= COEF_MAX) {
		pr_err("[FACTORY] %s: win_type is invalid value\n", __func__);
		win_type = ID_UTYPE;
	}

	pr_info("[FACTORY] %s: win_type:%d, thd:%d\n", __func__, win_type,
		light_coef_predefine_table[win_type].rlight.irisprox_th);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		light_coef_predefine_table[win_type].rlight.irisprox_th);
}

static DEVICE_ATTR(hh_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd490x_hh_ver_show, tmd490x_hh_ver_store);
static DEVICE_ATTR(hh_write_all_data, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd490x_hh_write_all_data_show, tmd490x_hh_write_all_data_store);
static DEVICE_ATTR(hh_is_exist_efs, S_IRUGO,
	tmd490x_hh_is_exist_efs_show, NULL);
static DEVICE_ATTR(hh_ext_prox_th, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd490x_hh_ext_prox_th_show, tmd490x_hh_ext_prox_th_store);

static struct device_attribute *hh_hole_attrs[] = {
	&dev_attr_hh_ver,
	&dev_attr_hh_write_all_data,
	&dev_attr_hh_is_exist_efs,
	&dev_attr_hh_ext_prox_th,
	NULL,
};

static int __init hh_hole_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_HH_HOLE, hh_hole_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit hh_hole_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_HH_HOLE);

	pr_info("[FACTORY] %s\n", __func__);
}
module_init(hh_hole_factory_init);
module_exit(hh_hole_factory_exit);
