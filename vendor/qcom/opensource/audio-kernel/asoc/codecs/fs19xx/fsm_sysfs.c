/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-01-20 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_SYSFS)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>

static uint8_t g_reg_addr;

#ifdef CONFIG_FSM_SYSCAL
static ssize_t fsm_re25_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	fsm_config_t *cfg = fsm_get_config();
	struct fsadsp_cmd_re25 cmd_re25;
	struct fsm_dev *fsm_dev;
	int re25, index;
	int ret;

	fsm_dev = dev_get_drvdata(dev);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null\n");
		return -EINVAL;
	}

	cfg->force_calib = true;
	fsm_set_calib_mode();
	fsm_delay_ms(2500);

	memset(&cmd_re25, 0, sizeof(struct fsadsp_cmd_re25));
	ret = fsm_afe_save_re25(&cmd_re25);
	cfg->force_calib = false;
	if (ret) {
		pr_err("save re25 failed:%d", ret);
		return ret;
	}

	index = fsm_get_index_by_position(fsm_dev->pos_mask);
	if (index < 0) {
		pr_addr(err, "Failed to get index: %d\n", index);
		return -EINVAL;
	}

	re25 = cmd_re25.cal_data[index].re25;
	return scnprintf(buf, PAGE_SIZE, "%d\n", re25);
}

static ssize_t fsm_f0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int payload[FSM_CALIB_PAYLOAD_SIZE];
	struct fsm_dev *fsm_dev;
	struct fsm_afe afe;
	int index, f0;
	int ret;

	fsm_dev = dev_get_drvdata(dev);
	if (fsm_dev == NULL) {
		pr_err("fsm_dev is null\n");
		return -EINVAL;
	}

	// fsm_set_calib_mode();
	// fsm_delay_ms(5000);
	afe.module_id = AFE_MODULE_ID_FSADSP_RX;
	afe.port_id = fsm_afe_get_rx_port();
	afe.param_id  = CAPI_V2_PARAM_FSADSP_CALIB;
	afe.op_set = false;
	ret = fsm_afe_send_apr(&afe, payload, sizeof(payload));
	if (ret) {
		pr_err("send apr failed:%d", ret);
		return ret;
	}

	index = fsm_get_index_by_position(fsm_dev->pos_mask);
	if (index < 0) {
		pr_addr(err, "Failed to get index: %d\n", index);
		return -EINVAL;
	}

	f0 = payload[3+6*index];
	return scnprintf(buf, PAGE_SIZE, "%d\n", f0);
}
#endif

static ssize_t fsm_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	fsm_version_t version;
	struct preset_file *pfile;
	int dev_count;
	int len = 0;

	fsm_get_version(&version);
	len  = scnprintf(buf + len, PAGE_SIZE, "version: %s\n",
			version.code_version);
	len += scnprintf(buf + len, PAGE_SIZE, "branch : %s\n",
			version.git_branch);
	len += scnprintf(buf + len, PAGE_SIZE, "commit : %s\n",
			version.git_commit);
	len += scnprintf(buf + len, PAGE_SIZE, "date   : %s\n",
			version.code_date);
	pfile = fsm_get_presets();
	dev_count = (pfile ? pfile->hdr.ndev : 0);
	len += scnprintf(buf + len, PAGE_SIZE, "device : [%d, %d]\n",
			dev_count, fsm_dev_count());

	return len;
}

static ssize_t fsm_debug_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t len)
{
	fsm_config_t *cfg = fsm_get_config();
	int value = simple_strtoul(buf, NULL, 0);

	if (cfg) {
		cfg->i2c_debug = !!value;
	}
	pr_info("i2c debug: %s", (cfg->i2c_debug ? "ON" : "OFF"));

	return len;
}

static ssize_t dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	uint16_t val;
	ssize_t len;
	uint8_t reg;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	for (len = 0, reg = 0; reg <= 0xCF; reg++) {
		ret = fsm_reg_read(fsm_dev, reg, &val);
		if (ret)
			return ret;
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%02X:%04X%c", reg, val,
				(reg & 0x7) == 0x7 ? '\n' : ' ');
	}
	buf[len - 1] = '\n';

	return len;
}

static ssize_t reg_rw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	uint16_t val;
	ssize_t len;
	int ret;

	if (fsm_dev == NULL)
		return -EINVAL;

	ret = fsm_reg_read(fsm_dev, g_reg_addr, &val);
	if (ret)
		return ret;

	len = snprintf(buf, PAGE_SIZE, "%02x:%04x\n", g_reg_addr, val);

	return len;
}

static ssize_t reg_rw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);
	int data;
	int ret;

	g_reg_addr = 0x00;

	if (fsm_dev == NULL)
		return -EINVAL;

	if (count < 2 || count > 9)
		return -EINVAL;

	ret = kstrtoint(buf, 0, &data);
	if (ret)
		return ret;

	if (count < 6) { // "0xXX"
		g_reg_addr = data & 0xFF;
		return count;
	}

	g_reg_addr = data >> 16;
	ret = fsm_reg_write(fsm_dev, g_reg_addr, data & 0xFFFF);
	if (ret)
		return ret;

	return count;
}

#ifdef CONFIG_FSM_SYSCAL
static DEVICE_ATTR(fsm_re25, S_IRUGO, fsm_re25_show, NULL);
static DEVICE_ATTR(fsm_f0, S_IRUGO, fsm_f0_show, NULL);
#endif
static DEVICE_ATTR(fsm_info, S_IRUGO, fsm_info_show, NULL);
static DEVICE_ATTR(fsm_debug, S_IWUSR, NULL, fsm_debug_store);
static DEVICE_ATTR_RO(dump);
static DEVICE_ATTR_RW(reg_rw);

static struct attribute *fs19xx_sysfs_attrs[] = {
#ifdef CONFIG_FSM_SYSCAL
	&dev_attr_fsm_re25.attr,
	&dev_attr_fsm_f0.attr,
#endif
	&dev_attr_fsm_info.attr,
	&dev_attr_fsm_debug.attr,
	&dev_attr_dump.attr,
	&dev_attr_reg_rw.attr,
	NULL,
};

static struct attribute *fs183x_sysfs_attrs[] = {
	&dev_attr_fsm_info.attr,
	&dev_attr_fsm_debug.attr,
	&dev_attr_dump.attr,
	&dev_attr_reg_rw.attr,
	NULL,
};

ATTRIBUTE_GROUPS(fs19xx_sysfs);
ATTRIBUTE_GROUPS(fs183x_sysfs);

int fsm_sysfs_init(struct device *dev)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);

	if (fsm_dev == NULL)
		return -EINVAL;

	if (fsm_dev->is19xx)
		return sysfs_create_group(&dev->kobj, &fs19xx_sysfs_group);
	else
		return sysfs_create_group(&dev->kobj, &fs183x_sysfs_group);
}

void fsm_sysfs_deinit(struct device *dev)
{
	struct fsm_dev *fsm_dev = dev_get_drvdata(dev);

	if (fsm_dev == NULL)
		return;

	if (fsm_dev->is19xx)
		sysfs_remove_group(&dev->kobj, &fs19xx_sysfs_group);
	else
		sysfs_remove_group(&dev->kobj, &fs183x_sysfs_group);
}
#endif
