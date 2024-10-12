/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
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
#define PFX "CAM_OIS"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
//#include "cam_cal.h"
//#include "cam_cal_define.h"
#include "i2c-mtk.h"
#include "cam_ois_ctrl.h"
//#include "eeprom_i2c_common_driver.h"
//#include "imgsensor_sysfs.h"
//#include "kd_imgsensor_sysfs_adapter.h"
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif


#define CAM_OIS_DRV_NAME "CAM_OIS_DRV"
#define CAM_OIS_DEV_MAJOR_NUMBER 236

#define CAM_OIS_MAX_BUF_SIZE 65536/*For Safety, Can Be Adjusted */

#define CAM_OIS_I2C_DEV1_NAME CAM_OIS_DRV_NAME
#define CAM_OIS_I2C_DEV2_NAME "CAM_OIS_DEV2"

static dev_t g_devNum = MKDEV(CAM_OIS_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static unsigned int g_drvOpened;
static struct i2c_client *g_pstI2Cclients[OIS_I2C_DEV_IDX_MAX] = { NULL };
static struct i2c_client *g_pstI2CclientG;

static DEFINE_SPINLOCK(g_spinLock);	/*for SMP */


//static unsigned int g_lastDevID;

/* add for linux-4.4 */
#ifndef I2C_WR_FLAG
#define I2C_WR_FLAG		(0x1000)
#define I2C_MASK_FLAG	(0x00ff)
#endif

#define OIS_I2C_MSG_SIZE_WRITE 128
#define OIS_I2C_MSG_SIZE_READ 2
#ifndef OIS_I2C_READ_MSG_LENGTH_MAX
#define OIS_I2C_READ_MSG_LENGTH_MAX 32
#endif

#define OIS_I2C_SPEED 400 //400k

void set_global_ois_i2c_client(struct i2c_client *client)
{
	spin_lock(&g_spinLock);
	g_pstI2CclientG = client;
	spin_unlock(&g_spinLock);
}

struct i2c_client *get_ois_i2c_client(enum OIS_I2C_DEV_IDX ois_i2c_ch)
{
	struct i2c_client *i2c_client = NULL;

	spin_lock(&g_spinLock);
	i2c_client = g_pstI2Cclients[ois_i2c_ch];
	spin_unlock(&g_spinLock);

	return i2c_client;
}

int ois_cam_i2c_read(u16 addr, u32 length, u8 *read_buf, int speed)
{
	int i4RetValue = 0;
	char puReadCmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	struct i2c_msg msg[OIS_I2C_MSG_SIZE_READ];

	if (g_pstI2CclientG == NULL) {
		pr_err("g_pstI2CclientG is NULL\n");
		return -1;
	}

	if (length > OIS_I2C_READ_MSG_LENGTH_MAX) {
		pr_debug("exceed one transition %d bytes limitation\n",
			 OIS_I2C_READ_MSG_LENGTH_MAX);
		return -1;
	}
	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr =
		g_pstI2CclientG->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_spinLock);

	msg[0].addr = g_pstI2CclientG->addr;
	msg[0].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;

	msg[1].addr = g_pstI2CclientG->addr;
	msg[1].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = read_buf;

	if ((speed > 0) && (speed <= 1000))
		speed = speed * 1000;
	else
		speed = OIS_I2C_SPEED * 1000;

	i4RetValue = mtk_i2c_transfer(g_pstI2CclientG->adapter, msg,
				OIS_I2C_MSG_SIZE_READ, 0, speed);

	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = g_pstI2CclientG->addr & I2C_MASK_FLAG;
	spin_unlock(&g_spinLock);

	if (i4RetValue != OIS_I2C_MSG_SIZE_READ) {
		pr_err("I2C read data failed!!\n");
		return -1;
	}
	return 0;
}

int ois_cam_i2c_write(u8 *write_buf, u16 length, u16 write_per_cycle, int speed)
{
	struct i2c_msg msg[OIS_I2C_MSG_SIZE_WRITE];
	u8 *pdata = write_buf;
	u8 *pend  = write_buf + length;
	int i   = 0;
	int ret = 0;
	int i4RetValue = 0;

	if (g_pstI2CclientG == NULL) {
		pr_err("g_pstI2CclientG is NULL\n");
		return -1;
	}

	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr =
		g_pstI2CclientG->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_spinLock);

	while (pdata < pend && i < OIS_I2C_MSG_SIZE_WRITE) {
		msg[i].addr  = g_pstI2CclientG->addr;
		msg[i].flags = 0;
		msg[i].len   = write_per_cycle;
		msg[i].buf   = pdata;

		i++;
		pdata += write_per_cycle;
	}

	if ((speed > 0) && (speed <= 1000))
		speed = speed * 1000;
	else
		speed = OIS_I2C_SPEED * 1000;

	i4RetValue = mtk_i2c_transfer(g_pstI2CclientG->adapter, msg, i, 0, speed);

	if (i4RetValue != i) {
		pr_err("I2C read data failed!!\n");
		ret = -1;
	}

	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = g_pstI2CclientG->addr & I2C_MASK_FLAG;
	spin_unlock(&g_spinLock);

	return ret;
}

/**************************************************
 * cam_ois_i2c_probe
 **************************************************/
static int cam_ois_i2c_probe
	(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclients[OIS_I2C_DEV_IDX_1] = client;

	// I2C device id is set by DT
	//g_pstI2Cclients[OIS_I2C_DEV_IDX_1]->addr = 0x1A;
	spin_unlock(&g_spinLock);

	return 0;
}

/**********************************************
 * CAMERA_OIS_i2c_remove
 **********************************************/
static int cam_ois_i2c_remove(struct i2c_client *client)
{
	return 0;
}

/***********************************************
 * cam_ois_i2c_probe2
 ***********************************************/
static int cam_ois_i2c_probe2
	(struct i2c_client *client, const struct i2c_device_id *id)
{
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclients[OIS_I2C_DEV_IDX_2] = client;

	// I2C device id is set by DT
	//g_pstI2Cclients[OIS_I2C_DEV_IDX_2]->addr = 0x54;
	spin_unlock(&g_spinLock);

	return 0;
}

/********************************************************
 * CAMERA OIS i2c_remove2
 ********************************************************/
static int cam_ois_i2c_remove2(struct i2c_client *client)
{
	return 0;
}

/*************************************************************
 * I2C related variable
 *************************************************************/


static const struct i2c_device_id
	cam_ois_i2c_id[] = { {CAM_OIS_DRV_NAME, 0}, {} };
static const struct i2c_device_id
	cam_ois_i2c_id2[] = { {CAM_OIS_I2C_DEV2_NAME, 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id cam_ois_i2c_of_ids[] = {
	{.compatible = "mediatek,camera_main_ois",},
	{}
};
#endif

struct i2c_driver cam_ois_i2c_driver = {
	.probe = cam_ois_i2c_probe,
	.remove = cam_ois_i2c_remove,
	.driver = {
		   .name = CAM_OIS_DRV_NAME,
		   .owner = THIS_MODULE,

#ifdef CONFIG_OF
		   .of_match_table = cam_ois_i2c_of_ids,
#endif
		   },
	.id_table = cam_ois_i2c_id,
};

/*********************************************************
 * I2C Driver structure for Sub
 *********************************************************/
#ifdef CONFIG_OF
static const struct of_device_id cam_ois_i2c_driver2_of_ids[] = {
	{.compatible = "mediatek,camera_sub_ois",},
	{}
};
#endif

struct i2c_driver cam_ois_i2c_driver2 = {
	.probe = cam_ois_i2c_probe2,
	.remove = cam_ois_i2c_remove2,
	.driver = {
		   .name = CAM_OIS_I2C_DEV2_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = cam_ois_i2c_driver2_of_ids,
#endif
		   },
	.id_table = cam_ois_i2c_id2,
};


/*******************************************************
 * cam_ois_probe
 *******************************************************/
static int cam_ois_probe(struct platform_device *pdev)
{
	i2c_add_driver(&cam_ois_i2c_driver2);
	return i2c_add_driver(&cam_ois_i2c_driver);
}

/*******************************************************
 * cam_ois_remove()
 *******************************************************/
static int cam_ois_remove(struct platform_device *pdev)
{
	i2c_del_driver(&cam_ois_i2c_driver);
	i2c_del_driver(&cam_ois_i2c_driver2);
	return 0;
}

/******************************************************
 *
 ******************************************************/
static struct platform_device g_plat_dev = {
	.name = CAM_OIS_DRV_NAME,
	.id = 0,
	.dev = {
		}
};


static struct platform_driver g_camera_ois_driver = {
	.probe = cam_ois_probe,
	.remove = cam_ois_remove,
	.driver = {
		   .name = CAM_OIS_DRV_NAME,
		   .owner = THIS_MODULE,
		}
};

static int cam_ois_drv_open(struct inode *a_pstInode, struct file *a_pstFile)
{
	int ret = 0;

	pr_debug("%s start\n", __func__);
	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		pr_debug("Opened, return -EBUSY\n");
		ret = -EBUSY;
	} else {
		g_drvOpened = 1;
		spin_unlock(&g_spinLock);
	}

	return ret;
}

static int cam_ois_drv_release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_spinLock);
	g_drvOpened = 0;
	spin_unlock(&g_spinLock);

	return 0;
}

static const struct file_operations g_stCAM_OIS_fops1 = {
	.owner = THIS_MODULE,
	.open = cam_ois_drv_open,
	.release = cam_ois_drv_release,
};

/***********************************************
 *
 ***********************************************/

#define CAM_OIS_DYNAMIC_ALLOCATE_DEVNO 1
static inline int cam_ois_chrdev_register(void)
{
	struct device *device = NULL;

	pr_debug("%s Start\n", __func__);

#if CAM_OIS_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_devNum, 0, 1, CAM_OIS_DRV_NAME)) {
		pr_debug("Allocate device no failed\n");
		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_devNum, 1, CAM_OIS_DRV_NAME)) {
		pr_debug("Register device no failed\n");
		return -EAGAIN;
	}
#endif

	g_charDrv = cdev_alloc();

	if (g_charDrv == NULL) {
		unregister_chrdev_region(g_devNum, 1);
		pr_debug("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &g_stCAM_OIS_fops1);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		pr_debug("Attach file operation failed\n");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, "CAM_OISdrv1");
	if (IS_ERR(g_drvClass)) {
		int ret = PTR_ERR(g_drvClass);

		pr_debug("Unable to create class, err = %d\n", ret);
		return ret;
	}
	device = device_create(g_drvClass, NULL, g_devNum, NULL,
		CAM_OIS_DRV_NAME);
	pr_debug("%s End\n", __func__);

	return 0;
}

static void cam_ois_chrdev_unregister(void)
{
	/*Release char driver */

	class_destroy(g_drvClass);

	device_destroy(g_drvClass, g_devNum);

	cdev_del(g_charDrv);

	unregister_chrdev_region(g_devNum, 1);
}

/***********************************************
 *
 ***********************************************/

static int __init cam_ois_drv_init(void)
{
	pr_debug("%s - E!\n", __func__);

	if (platform_driver_register(&g_camera_ois_driver)) {
		pr_err("failed to register camera OIS driver i2C main\n");
		return -ENODEV;
	}

	if (platform_device_register(&g_plat_dev)) {
		pr_err("failed to register camera OIS device");
		return -ENODEV;
	}

	cam_ois_chrdev_register();

	pr_debug("%s - X!\n", __func__);
	return 0;
}

static void __exit cam_ois_drv_exit(void)
{

	platform_device_unregister(&g_plat_dev);
	platform_driver_unregister(&g_camera_ois_driver);

	cam_ois_chrdev_unregister();
}
module_init(cam_ois_drv_init);
module_exit(cam_ois_drv_exit);

MODULE_DESCRIPTION("Camera OIS Driver");
MODULE_LICENSE("GPL");
