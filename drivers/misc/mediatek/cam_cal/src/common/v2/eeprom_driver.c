/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "eeprom_driver.h"
#include "eeprom_i2c_common_driver.h"
#include "cam_cal_list.h"

#include "cam_cal.h"
#include "kd_imgsensor_sysfs_adapter.h"

#define DEV_NODE_NAME_PREFIX "camera_eeprom"
#define DEV_NAME_FMT "camera_eeprom%u"
#define DEV_CLASS_NAME_FMT "camera_eepromdrv%u"
#define EEPROM_DEVICE_NNUMBER 255

static struct EEPROM_DRV ginst_drv[MAX_EEPROM_NUMBER];

static struct stCAM_CAL_LIST_STRUCT *get_list(u32 sensor_id)
{
	struct stCAM_CAL_LIST_STRUCT *plist;

	cam_cal_get_sensor_list(&plist);

	while (plist &&
	       (plist->sensorID != 0) &&
	       (plist->sensorID != sensor_id))
		plist++;

	return plist;
}

static unsigned int read_region(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned char *buf,
				unsigned int offset, unsigned int size)
{
	unsigned int ret;
	unsigned short dts_addr;
	struct stCAM_CAL_LIST_STRUCT *plist = get_list(pdata->sensor_info.sensor_id);
	unsigned int size_limit = (plist && plist->maxEepromSize > 0)
		? plist->maxEepromSize : DEFAULT_MAX_EEPROM_SIZE_8K;

	if (offset + size > size_limit) {
		pr_debug("Error! not support address >= 0x%x!!\n", size_limit);
		return 0;
	}

	if (plist && plist->readCamCalData) {
		pr_debug("i2c addr 0x%x\n", plist->slaveID);
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		dts_addr = pdata->pdrv->pi2c_client->addr;
		pdata->pdrv->pi2c_client->addr = (plist->slaveID >> 1);
		ret = plist->readCamCalData(pdata->pdrv->pi2c_client, pdata->sensor_info,
					    offset, buf, size);
		pdata->pdrv->pi2c_client->addr = dts_addr;
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	} else {
		pr_debug("no customized\n");
	// we don't use this
/*
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		ret = Common_read_region(pdata->pdrv->pi2c_client, pdata->sensor_info,
					 offset, buf, size);
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
*/
		return 0;
	}

	if (IMGSENSOR_SYSFS_UPDATE(buf, pdata->sensor_info.device_id,
		pdata->sensor_info.sensor_id, offset, size, ret) < 0)
		ret = 0;

	return ret;
}

static unsigned int write_region(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned char *buf,
				unsigned int offset, unsigned int size)
{
	unsigned int ret;
	unsigned short dts_addr;
	struct stCAM_CAL_LIST_STRUCT *plist = get_list(pdata->sensor_info.sensor_id);
	unsigned int size_limit = (plist && plist->maxEepromSize > 0)
		? plist->maxEepromSize : DEFAULT_MAX_EEPROM_SIZE_8K;

	if (offset + size > size_limit) {
		pr_debug("Error! not support address >= 0x%x!!\n", size_limit);
		return 0;
	}

	if (plist && plist->writeCamCalData) {
		pr_debug("i2c addr 0x%x\n", plist->slaveID);
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		dts_addr = pdata->pdrv->pi2c_client->addr;
		pdata->pdrv->pi2c_client->addr = (plist->slaveID >> 1);
		ret = plist->writeCamCalData(pdata->pdrv->pi2c_client, pdata->sensor_info,
					    offset, buf, size);
		pdata->pdrv->pi2c_client->addr = dts_addr;
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	} else {
		pr_debug("no customized\n");
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		ret = Common_write_region(pdata->pdrv->pi2c_client, pdata->sensor_info,
					 offset, buf, size);
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	}

	return ret;
}

static int eeprom_open(struct inode *a_inode, struct file *a_file)
{
	struct EEPROM_DRV_FD_DATA *pdata;
	struct EEPROM_DRV *pdrv;

	pr_debug("open\n");

	pdata = kmalloc(sizeof(struct EEPROM_DRV_FD_DATA), GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

	pdrv = container_of(a_inode->i_cdev, struct EEPROM_DRV, cdev);

	pdata->pdrv = pdrv;
	pdata->sensor_info.sensor_id = 0;

	a_file->private_data = pdata;

	return 0;
}

static int eeprom_release(struct inode *a_inode, struct file *a_file)
{
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;

	pr_debug("release\n");

	kfree(pdata);

	return 0;
}

static ssize_t eeprom_read(struct file *a_file, char __user *user_buffer,
			   size_t size, loff_t *offset)
{
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;
	u8 *kbuf = kmalloc(size, GFP_KERNEL);

	pr_debug("[%s] read %lu %llu\n", __func__, size, *offset);

	if (kbuf == NULL)
		return -ENOMEM;

	if (read_region(pdata, kbuf, *offset, size) != size ||
	    copy_to_user(user_buffer, kbuf, size)) {
		kfree(kbuf);
		return -EFAULT;
	}

	*offset += size;
	kfree(kbuf);
	return size;
}

static ssize_t eeprom_write(struct file *a_file, const char __user *user_buffer,
			    size_t size, loff_t *offset)
{
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;
	u8 *kbuf = kmalloc(size, GFP_KERNEL);

	pr_debug("write %lu %llu\n", size, *offset);

	if (kbuf == NULL)
		return -ENOMEM;

	if (copy_from_user(kbuf, user_buffer, size) ||
	    write_region(pdata, kbuf, *offset, size) != size) {
		kfree(kbuf);
		return -EFAULT;
	}

	*offset += size;
	kfree(kbuf);
	return size;
}

static loff_t eeprom_seek(struct file *a_file, loff_t offset, int whence)
{
#define MAX_LENGTH 16192 /*MAX 16k bytes*/
	loff_t new_pos = 0;

	switch (whence) {
	case 0: /* SEEK_SET: */
		new_pos = offset;
		break;
	case 1: /* SEEK_CUR: */
		new_pos = a_file->f_pos + offset;
		break;
	case 2: /* SEEK_END: */
		new_pos = MAX_LENGTH + offset;
		break;
	default:
		return -EINVAL;
	}

	if (new_pos < 0)
		return -EINVAL;

	a_file->f_pos = new_pos;

	return new_pos;
}

int eeprom_ioctl_get_command_data(enum CAM_CAL_COMMAND command, unsigned int device_id, unsigned int sensor_id)
{
	int info = -1;
	struct stCAM_CAL_LIST_STRUCT *cam_cal_list = NULL;
	const struct imgsensor_vendor_rom_addr *rom_addr = NULL;

	rom_addr = IMGSENSOR_SYSGET_ROM_ADDR_BY_ID(device_id, sensor_id);

	if (rom_addr == NULL) {
		pr_err("[%s] rom_addr is NULL", __func__);
		return -EFAULT;
	}

	switch (command) {
	case CAM_CAL_COMMAND_EEPROM_LIMIT_SIZE:
		cam_cal_list = get_list(sensor_id);
		info = cam_cal_list->maxEepromSize;
		break;
	case CAM_CAL_COMMAND_CAL_SIZE:
		info = rom_addr->rom_max_cal_size;
		break;
	case CAM_CAL_COMMAND_CONVERTED_CAL_SIZE:
		info = rom_addr->rom_converted_max_cal_size;
		break;
	case CAM_CAL_COMMAND_AWB_ADDR:
		info = rom_addr->rom_awb_cal_data_start_addr;
		break;
	case CAM_CAL_COMMAND_CONVERTED_AWB_ADDR:
		if (rom_addr->converted_cal_addr == NULL)
			return -EINVAL;
		info = rom_addr->converted_cal_addr->rom_awb_cal_data_start_addr;
		break;
	case CAM_CAL_COMMAND_LSC_ADDR:
		info = rom_addr->rom_shading_cal_data_start_addr;
		break;
	case CAM_CAL_COMMAND_CONVERTED_LSC_ADDR:
		if (rom_addr->converted_cal_addr == NULL)
			return -EINVAL;
		info = rom_addr->converted_cal_addr->rom_shading_cal_data_start_addr;
		break;
	case CAM_CAL_COMMAND_MODULE_INFO_ADDR:
		info = rom_addr->rom_header_main_module_info_start_addr;
		break;
	case CAM_CAL_COMMAND_BAYERFORMAT:
	case CAM_CAL_COMMAND_MEMTYPE:
		pr_err("[%s] Not used anymore", __func__);
		info = -EINVAL;
		break;
	case CAM_CAL_COMMAND_NONE:
	default:
		pr_err("No such command %d\n", command);
		info = -EINVAL;
	}

	return info;
}

long eeprom_ioctl_control_command(void *pBuff)
{
	struct CAM_CAL_SENSOR_INFO sensor_info;
	u32 info = 0;
	int ret = -1;

	sensor_info.sensor_id = ((struct CAM_CAL_SENSOR_INFO *)pBuff)->sensor_id;
	sensor_info.device_id = ((struct CAM_CAL_SENSOR_INFO *)pBuff)->device_id;
	sensor_info.command = ((struct CAM_CAL_SENSOR_INFO *)pBuff)->command;

	ret = eeprom_ioctl_get_command_data(sensor_info.command, sensor_info.device_id, sensor_info.sensor_id);
	if (ret < 0)
		return ret;

	info = (u32)ret;

	sensor_info.device_id = IMGSENSOR_SENSOR_IDX_MAP(sensor_info.device_id);

	pr_info("[%s] sensor id = 0x%x, device id = 0x%x, info: 0x%x\n",
			__func__, sensor_info.sensor_id, sensor_info.device_id, info);
	copy_to_user((void __user *)((struct CAM_CAL_SENSOR_INFO *)pBuff)->info,
				(void *)&info, sizeof(info));
	return 0;
}

#ifdef CONFIG_COMPAT
long compat_eeprom_ioctl_control_command(void *pBuff)
{
	struct COMPAT_CAM_CAL_SENSOR_INFO sensor_info;
	compat_uint_t info = 0;
	int ret = -1;

	sensor_info.sensor_id = ((struct COMPAT_CAM_CAL_SENSOR_INFO *)pBuff)->sensor_id;
	sensor_info.device_id = ((struct COMPAT_CAM_CAL_SENSOR_INFO *)pBuff)->device_id;
	sensor_info.command = ((struct COMPAT_CAM_CAL_SENSOR_INFO *)pBuff)->command;

	ret = eeprom_ioctl_get_command_data(sensor_info.command, sensor_info.device_id, sensor_info.sensor_id);
	if (ret < 0)
		return ret;

	info = (compat_uint_t)ret;

	sensor_info.device_id = IMGSENSOR_SENSOR_IDX_MAP(sensor_info.device_id);

	copy_to_user(compat_ptr(((struct COMPAT_CAM_CAL_SENSOR_INFO *)pBuff)->info),
				&info, sizeof(info));

	return 0;
}
#endif

static long eeprom_ioctl(struct file *a_file, unsigned int a_cmd,
			 unsigned long a_param)
{
	void *pBuff = NULL;
	long ret = 0;
	struct EEPROM_DRV_FD_DATA *pdata =
		(struct EEPROM_DRV_FD_DATA *) a_file->private_data;
	u32 ioctl_size = _IOC_SIZE(a_cmd);

	pr_debug("ioctl\n");

	if (_IOC_DIR(a_cmd) == _IOC_NONE) {
		pr_err("[%s] Command == _IOC_NONE", __func__);
		return -EFAULT;
	}

	pBuff = kmalloc(ioctl_size, GFP_KERNEL);
	if (pBuff == NULL)
		return -ENOMEM;

	memset(pBuff, 0, ioctl_size);

	if (((_IOC_WRITE | _IOC_READ) & _IOC_DIR(a_cmd)) &&
	    copy_from_user(pBuff,
			   (void *)a_param,
			   _IOC_SIZE(a_cmd))) {

		kfree(pBuff);
		pr_err("ioctl copy from user failed\n");
		return -EFAULT;
	}

	switch (a_cmd) {
	case CAM_CALIOC_S_SENSOR_INFO:
		pdata->sensor_info.sensor_id =
			((struct CAM_CAL_SENSOR_INFO *)pBuff)->sensor_id;
		pdata->sensor_info.device_id =
			((struct CAM_CAL_SENSOR_INFO *)pBuff)->device_id;

		pr_debug("sensor id = 0x%x, device id = 0x%x \n",
		       pdata->sensor_info.sensor_id, pdata->sensor_info.device_id);
		break;
	case CAM_CALIOC_G_SENSOR_INFO:
		ret = eeprom_ioctl_control_command(pBuff);
		if (ret < 0)
			pr_err("[%s] Failed to get data", __func__);
		break;
	default:
		pr_err("No such command %d\n", a_cmd);
		ret = -EINVAL;
	}

	kfree(pBuff);
	return ret;
}

#ifdef CONFIG_COMPAT
static long eeprom_compat_ioctl(struct file *a_file, unsigned int a_cmd,
				unsigned long a_param)
{
	void *pBuff = NULL;
	long ret = -1;
	struct COMPAT_EEPROM_DRV_FD_DATA *pdata =
		(struct COMPAT_EEPROM_DRV_FD_DATA *) a_file->private_data;
	u32 ioctl_size = _IOC_SIZE(a_cmd);

	if (_IOC_DIR(a_cmd) == _IOC_NONE) {
		pr_err("[%s] Command == _IOC_NONE", __func__);
		return -EFAULT;
	}

	pBuff = kmalloc(ioctl_size, GFP_KERNEL);
	if (pBuff == NULL)
		return -ENOMEM;

	memset(pBuff, 0, ioctl_size);

	if (((_IOC_WRITE | _IOC_READ) & _IOC_DIR(a_cmd)) &&
		copy_from_user(pBuff,
			   (void *)a_param,
			   _IOC_SIZE(a_cmd))) {

		kfree(pBuff);
		pr_err("ioctl copy from user failed\n");
		return -EFAULT;
	}

	switch (a_cmd) {
	case COMPAT_CAM_CALIOC_S_SENSOR_INFO:
		pdata->sensor_info.sensor_id =
			((struct COMPAT_CAM_CAL_SENSOR_INFO *)pBuff)->sensor_id;
		pdata->sensor_info.device_id =
			((struct COMPAT_CAM_CAL_SENSOR_INFO *)pBuff)->device_id;

		pr_debug("sensor id = 0x%x, device id = 0x%x\n",
			   pdata->sensor_info.sensor_id, pdata->sensor_info.device_id);
		break;
	case COMPAT_CAM_CALIOC_G_SENSOR_INFO:
		ret = compat_eeprom_ioctl_control_command(pBuff);
		if (ret < 0)
			pr_err("[%s] Failed to get data", __func__);
		break;
	default:
		pr_err("No such command %u\n", a_cmd);
		ret = -EINVAL;
	}

	kfree(pBuff);
	return ret;
}
#endif

static const struct file_operations geeprom_file_operations = {
	.owner = THIS_MODULE,
	.open = eeprom_open,
	.read = eeprom_read,
	.write = eeprom_write,
	.llseek = eeprom_seek,
	.release = eeprom_release,
	.unlocked_ioctl = eeprom_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = eeprom_compat_ioctl
#endif
};

static inline int retrieve_index(struct i2c_client *client,
				 unsigned int *index)
{
	const char *node_name = client->dev.of_node->name;
	const size_t prefix_len = strlen(DEV_NODE_NAME_PREFIX);

	if (strncmp(node_name, DEV_NODE_NAME_PREFIX, prefix_len) == 0 &&
	    kstrtouint(node_name + prefix_len, 10, index) == 0) {
		pr_debug("index = %u\n", *index);
		return 0;
	}

	pr_err("invalid node name format\n");
	*index = 0;
	return -EINVAL;
}

static inline int eeprom_driver_register(struct i2c_client *client,
					 unsigned int index)
{
	int ret = 0;
	struct EEPROM_DRV *pinst;
	char device_drv_name[DEV_NAME_STR_LEN_MAX] = { 0 };
	char class_drv_name[DEV_NAME_STR_LEN_MAX] = { 0 };

	if (index >= MAX_EEPROM_NUMBER) {
		pr_err("node index out of bound\n");
		return -EINVAL;
	}

	ret = snprintf(device_drv_name, DEV_NAME_STR_LEN_MAX - 1,
		DEV_NAME_FMT, index);
	if (ret < 0) {
		pr_info(
		"[eeprom]%s error, ret = %d", __func__, ret);
		return -EFAULT;
	}
	ret = snprintf(class_drv_name, DEV_NAME_STR_LEN_MAX - 1,
		DEV_CLASS_NAME_FMT, index);
	if (ret < 0) {
		pr_info(
		"[eeprom]%s error, ret = %d", __func__, ret);
		return -EFAULT;
	}

	ret = 0;
	pinst = &ginst_drv[index];
	pinst->dev_no = MKDEV(EEPROM_DEVICE_NNUMBER, index);

	if (alloc_chrdev_region(&pinst->dev_no, 0, 1, device_drv_name)) {
		pr_err("Allocate device no failed\n");
		return -EAGAIN;
	}

	/* Attatch file operation. */
	cdev_init(&pinst->cdev, &geeprom_file_operations);

	/* Add to system */
	if (cdev_add(&pinst->cdev, pinst->dev_no, 1)) {
		pr_err("Attatch file operation failed\n");
		unregister_chrdev_region(pinst->dev_no, 1);
		return -EAGAIN;
	}

	memcpy(pinst->class_name, class_drv_name, DEV_NAME_STR_LEN_MAX);
	pinst->pclass = class_create(THIS_MODULE, pinst->class_name);
	if (IS_ERR(pinst->pclass)) {
		ret = PTR_ERR(pinst->pclass);

		pr_err("Unable to create class, err = %d\n", ret);
		return ret;
	}

	device_create(pinst->pclass, NULL, pinst->dev_no, NULL,
		      device_drv_name);

	pinst->pi2c_client = client;
	mutex_init(&pinst->eeprom_mutex);

	return ret;
}

static inline int eeprom_driver_unregister(unsigned int index)
{
	struct EEPROM_DRV *pinst = NULL;

	if (index >= MAX_EEPROM_NUMBER) {
		pr_err("node index out of bound\n");
		return -EINVAL;
	}

	pinst = &ginst_drv[index];

	/* Release char driver */
	unregister_chrdev_region(pinst->dev_no, 1);

	device_destroy(pinst->pclass, pinst->dev_no);
	class_destroy(pinst->pclass);

	return 0;
}

static int eeprom_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	unsigned int index = 0;

	pr_debug("probe start name: %s\n", client->dev.of_node->name);

	if (retrieve_index(client, &index) < 0)
		return -EINVAL;
	else
		return eeprom_driver_register(client, index);
}

static int eeprom_remove(struct i2c_client *client)
{
	unsigned int index = 0;

	pr_debug("remove name: %s\n", client->dev.of_node->name);

	if (retrieve_index(client, &index) < 0)
		return -EINVAL;
	else
		return eeprom_driver_unregister(index);
}

static const struct of_device_id eeprom_of_match[] = {
	{ .compatible = "mediatek,camera_eeprom", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, eeprom_of_match);

static struct i2c_driver eeprom_i2c_init = {
	.driver = {
		.name   = "mediatek,camera_eeprom",
		.of_match_table = of_match_ptr(eeprom_of_match),
	},
	.probe      = eeprom_probe,
	.remove     = eeprom_remove,
};

module_i2c_driver(eeprom_i2c_init);

MODULE_DESCRIPTION("camera eeprom driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");

