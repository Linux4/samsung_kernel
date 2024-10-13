/*
 * MELFAS MIP4 Touchkey
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4_sec.c : Command functions (Optional)
 *
 */

#include "mip4.h"

#if MIP_USE_CMD

static unsigned char IsFWDownloading = 0;

/**
* Touch Key FW Update status
*/

static ssize_t tc300k_fw_update_status_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct mip4_tk_info *info = dev_get_drvdata(dev);
	int ret;

	switch (IsFWDownloading) {
	case 1:
		ret = sprintf(buf, "%s\n", "DOWNLOADING");
		break;
	case 2:
		ret = sprintf(buf, "%s\n", "FAIL");
		break;
	case 3:
		ret = sprintf(buf, "%s\n", "PASS");
		break;
	default:
		dev_err(&info->client->dev, "%s: invalid status\n", __func__);
		ret = 0;
		goto out;
	}

	dev_info(&info->client->dev, "%s: %#x\n", __func__, IsFWDownloading);


out:
	return ret;
}


/**
* Touch Key FW Update
*/
static ssize_t mip_sys_fw_update(struct device *dev,
				   struct device_attribute *devattr,
				   const char *buf, size_t count)
{
	struct mip4_tk_info *info = dev_get_drvdata(dev);
	int ret;
	u8 fw_path;
	
	IsFWDownloading = 1;

	switch (*buf) {
	case 's':
	case 'S':
		if(mip4_tk_fw_update_from_storage(info, FW_PATH_EXTERNAL, true)){
		//if(mip4_tk_fw_update_from_kernel(info)){
			IsFWDownloading = 2;
			return -EINVAL;
		}
		break;

	case 'i':
	case 'I':
		if(mip4_tk_fw_update_from_kernel(info)){
			IsFWDownloading = 2;
			return -EINVAL;
		}		
		break;

	default:
		dev_err(&info->client->dev, "%s: invalid parameter %c\n.", __func__,
			*buf);
		return -EINVAL;
	}

	dev_dbg(&info->client->dev, "%s - cmd[%s] fw update success\n", __func__, buf);
	
	IsFWDownloading = 3;
	return count;
}


/**
* Command : Get firmware version from file
*/

static ssize_t mip_sys_get_fw_ver_bin(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mip4_tk_info *info = dev_get_drvdata(dev);
	int ret;
	u8 ver_file[MIP_FW_MAX_SECT_NUM * 2];
	
	if(mip4_tk_get_fw_version_from_bin(info, ver_file)){
		return -EINVAL;
	}
	
	ret = sprintf(buf, "%02X.%02X/%02X.%02X/%02X.%02X/%02X.%02X\n", ver_file[0], ver_file[1], ver_file[2], ver_file[3], ver_file[4], ver_file[5], ver_file[6], ver_file[7]);
	dev_info(&info->client->dev, "%s: %s\n", __func__, buf);
	return ret;
}


/**
* Command : Get firmware version from chip
*/

static ssize_t mip_sys_get_fw_ver_ic(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mip4_tk_info *info = dev_get_drvdata(dev);
	int ret;
	u8 rbuf[64];

	if(mip4_tk_get_fw_version(info, rbuf)){
		return sprintf(buf, "Failed\n");
		//return -EINVAL;
	}

	ret = sprintf(buf, "%02X.%02X/%02X.%02X/%02X.%02X/%02X.%02X\n", rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

	dev_info(&info->client->dev, "%s: %s\n", __func__, buf);
	return ret;
}

#if 0
/**
* Sysfs print key intensity
*/
static ssize_t mip_sys_intensity_key(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip4_tk_info *info = dev_get_drvdata(dev);
	int key_offset = info->node_x * info->node_y;
	int key_idx = -1;
	int retry = 3;

	if (!strcmp(attr->attr.name, "touchkey_recent")){
		key_idx = 0;
	}
	else if (!strcmp(attr->attr.name, "touchkey_back")){
		key_idx = 1;
	}
	else {
		dev_err(&info->client->dev, "%s [ERROR] Invalid attribute\n", __func__);		
		goto ERROR;
	}

	while(retry--){
		if(info->test_busy == false){
			break;
		}
		msleep(10);
	}
	if(retry <= 0){
		dev_err(&info->client->dev, "%s [ERROR] skip\n", __func__);
		goto EXIT;
	}

	if(mip_get_image(info, MIP_IMG_TYPE_INTENSITY)){
		dev_err(&info->client->dev, "%s [ERROR] mip_get_image\n", __func__);
		goto ERROR;
	}	

EXIT:
	dev_dbg(&info->client->dev, "%s - %s [%d]\n", __func__, attr->attr.name, info->image_buf[key_offset + key_idx]);

	return snprintf(buf, PAGE_SIZE, "%d", info->image_buf[key_offset + key_idx]);

ERROR:	
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	
	return snprintf(buf, PAGE_SIZE, "NG");
}
#endif
//static DEVICE_ATTR(touchkey_recent, S_IRUGO, mip_sys_intensity_key, NULL);
//static DEVICE_ATTR(touchkey_back, S_IRUGO, mip_sys_intensity_key, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
		   mip_sys_get_fw_ver_ic, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP,
		   mip_sys_get_fw_ver_bin, NULL);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, mip_sys_fw_update);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO,
		   tc300k_fw_update_status_show, NULL);

/**
* Sysfs print key threshold
*/
static ssize_t mip_sys_threshold_key(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip4_tk_info *info = dev_get_drvdata(dev);
	u8 wbuf[4];
	u8 rbuf[4];
	int threshold;

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CONTACT_THD_KEY;
	if(mip4_tk_i2c_read(info, wbuf, 2, rbuf, 2)){
		goto ERROR;
	}

	threshold = (rbuf[1] << 8) | rbuf[0];

	dev_dbg(&info->client->dev, "%s - threshold [%d]\n", __func__, threshold);
	return snprintf(buf, PAGE_SIZE, "%d", threshold);

ERROR:	
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return snprintf(buf, PAGE_SIZE, "NG");	
}
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, mip_sys_threshold_key, NULL);

/**
* Sysfs - touchkey attr info
*/
static struct attribute *mip_cmd_key_attr[] = {
//	&dev_attr_touchkey_recent.attr,
//	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	NULL,
};

/**
* Sysfs - touchkey attr group info
*/
static const struct attribute_group mip_cmd_key_attr_group = {
	.attrs = mip_cmd_key_attr,
};

extern struct class *sec_class;

/**
* Create sysfs command functions
*/
int mip4_tk_sysfs_cmd_create(struct mip4_tk_info *info)
{
	struct i2c_client *client = info->client;
	int i = 0;

	//create sec class
	/*
	info->cmd_class = class_create(THIS_MODULE, "sec");
	info->cmd_dev = device_create(info->cmd_class, NULL, 0, info, "touchkey");
	*/
	info->cmd_dev = device_create(sec_class, NULL, 0, info, "sec_touchkey");
	if (IS_ERR(info->cmd_dev)) {
		dev_err(&client->dev, "Failed to create fac tsp temp dev\n");
		info->cmd_dev = NULL;
		return -ENODEV;		
	}
	
	if (sysfs_create_group(&info->cmd_dev->kobj, &mip_cmd_key_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}
	
	return 0;
}

/**
* Remove sysfs command functions
*/
void mip4_tk_sysfs_cmd_remove(struct mip4_tk_info *info)
{
	sysfs_remove_group(&info->client->dev.kobj, &mip_cmd_key_attr_group);

	return;
}

#endif

