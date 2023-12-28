// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics
 */
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

#include "cam_ois_define.h"
#include "cam_ois_drv.h"
#include "cam_ois.h"
#include "cam_ois_sysfs.h"
#include "cam_ois_common.h"

#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define CAM_OIS_DRV_NAME "CAM_OIS_DRV"
#define CAM_OIS_CLASS_NAME "CAM_OIS_CLS"
#define CAM_OIS_DEV_MAJOR_NUMBER 236

#define CAM_OIS_MAX_BUF_SIZE 65536/*For Safety, Can Be Adjusted */

#define CAM_OIS_I2C_DEV1_NAME CAM_OIS_DRV_NAME
#define CAM_OIS_I2C_DEV2_NAME "CAM_OIS_DEV2"

static dev_t g_devNum = MKDEV(CAM_OIS_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static unsigned int g_drvOpened;
static struct i2c_client *g_pstI2Cclients[OIS_I2C_DEV_IDX_MAX] = { NULL };
// static struct i2c_client *g_pstI2CclientG;
static struct device *ois_device;

static DEFINE_SPINLOCK(g_spinLock);	/*for SMP */

struct i2c_client *get_ois_i2c_client(enum OIS_I2C_DEV_IDX ois_i2c_ch)
{
	struct i2c_client *i2c_client = NULL;

	spin_lock(&g_spinLock);
	i2c_client = g_pstI2Cclients[ois_i2c_ch];
	spin_unlock(&g_spinLock);

	return i2c_client;
}

struct device *get_cam_ois_dev(void)
{
	return ois_device;
}

static int cam_ois_drv_open(struct inode *a_pstInode, struct file *a_pstFile)
{
	int ret = 0;

	LOG_INF(" - E");
	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		LOG_INF("Opened");
		ret = -EBUSY;
	} else {
		g_drvOpened = 1;
		spin_unlock(&g_spinLock);
	}
	LOG_DBG(" - X");
	return ret;
}

static int cam_ois_drv_release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_spinLock);
	g_drvOpened = 0;
	spin_unlock(&g_spinLock);

	return 0;
}

static long cam_ois_drv_ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		unsigned long a_u4Param)
{
	long ret = 0;

	switch (a_u4Command) {
	LOG_INF("%d", a_u4Command);
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long cam_ois_drv_ioctl_compat(struct file *a_pstFile, unsigned int a_u4Command,
			unsigned long a_u4Param)
{
	long ret = 0;

	ret = cam_ois_drv_ioctl(a_pstFile, a_u4Command, (unsigned long)compat_ptr(a_u4Param));

	return ret;
}
#endif

static const struct file_operations g_stCAM_OIS_fops1 = {
	.owner = THIS_MODULE,
	.open = cam_ois_drv_open,
	.release = cam_ois_drv_release,
	.unlocked_ioctl = cam_ois_drv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cam_ois_drv_ioctl_compat,
#endif
};

/***********************************************
 *
 ***********************************************/

#define CAM_OIS_DYNAMIC_ALLOCATE_DEVNO 1
static inline int cam_ois_chrdev_register(void)
{
	LOG_INF(" - E");

#if CAM_OIS_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_devNum, 0, 1, CAM_OIS_DRV_NAME)) {
#else
	if (register_chrdev_region(g_devNum, 1, CAM_OIS_DRV_NAME)) {
#endif
		LOG_DBG("Register device no failed");
		return -EAGAIN;
	}

	g_charDrv = cdev_alloc();

	if (g_charDrv == NULL) {
		unregister_chrdev_region(g_devNum, 1);
		LOG_DBG("Allocate mem for kobject failed");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &g_stCAM_OIS_fops1);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		LOG_INF("Attach file operation failed");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, CAM_OIS_CLASS_NAME);
	if (IS_ERR(g_drvClass)) {
		int ret = PTR_ERR(g_drvClass);

		LOG_INF("Unable to create class, err = %d", ret);
		return ret;
	}
	ois_device = device_create(g_drvClass, NULL, g_devNum, NULL, CAM_OIS_DRV_NAME);
	if (ois_device == NULL) {
		LOG_ERR("fail to create device %s", CAM_OIS_DRV_NAME);
		return -EIO;
	}

	ois_device->of_node = of_find_compatible_node(NULL, NULL, "mediatek,camera_ois_hw");
	if (ois_device->of_node == NULL) {
		LOG_ERR("fail to get camera_ois_hw node");
		return -EIO;
	}

	LOG_DBG(" - X");

	return 0;
}

static void cam_ois_chrdev_unregister(void)
{
	LOG_INF(" - E\n");
	/*Release char driver */
	cdev_del(g_charDrv);

	unregister_chrdev_region(g_devNum, 1);

	device_destroy(g_drvClass, g_devNum);

	class_destroy(g_drvClass);
	LOG_INF(" - X\n");
}

/**************************************************
 * cam_ois_i2c_probe
 **************************************************/
static int cam_ois_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	LOG_INF(" - E");
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclients[OIS_I2C_DEV_IDX_1] = client;

	// I2C device id is set by DT
	//g_pstI2Cclients[OIS_I2C_DEV_IDX_1]->addr = 0x1A;
	spin_unlock(&g_spinLock);

	ret = cam_ois_chrdev_register();
	if (ret)
		LOG_INF("register char device failed!\n");
	else
		LOG_INF("register char device success");

	LOG_INF("- X");
	return ret;
}

/**********************************************
 * CAMERA_OIS_i2c_remove
 **********************************************/
static int cam_ois_i2c_remove(struct i2c_client *client)
{
	cam_ois_chrdev_unregister();
	return 0;
}

/***********************************************
 * cam_ois_i2c_probe2
 ***********************************************/
static int cam_ois_i2c_probe2(struct i2c_client *client, const struct i2c_device_id *id)
{
	LOG_INF(" - E");
	/* get sensor i2c client */
	spin_lock(&g_spinLock);
	g_pstI2Cclients[OIS_I2C_DEV_IDX_2] = client;

	// I2C device id is set by DT
	//g_pstI2Cclients[OIS_I2C_DEV_IDX_2]->addr = 0x54;
	spin_unlock(&g_spinLock);
	LOG_INF(" - X");
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
	int ret = 0;

	LOG_INF("- E");
	ret = i2c_add_driver(&cam_ois_i2c_driver2);
	if (ret)
		LOG_INF("fail to add i2c driver 2");

	LOG_INF("I2C Client2 0x%x", g_pstI2Cclients[OIS_I2C_DEV_IDX_2]);

	LOG_INF(" add i2c - E");
	ret = i2c_add_driver(&cam_ois_i2c_driver);
	if (ret)
		LOG_INF("fail to add i2c driver 0");
	LOG_INF(" add i2c - X");

	LOG_INF("I2C Client1 0x%x", g_pstI2Cclients[OIS_I2C_DEV_IDX_1]);

	ret = cam_ois_prove_init();
	if (ret)
		LOG_INF("fail to set ois mcu info");

	LOG_INF("- X");
	return ret;
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
static struct platform_device g_cam_ois_device = {
	.name = CAM_OIS_DRV_NAME,
	.id = 0,
	.dev = {
	}
};

#ifdef CONFIG_OF
static const struct of_device_id g_cam_of_device_id[] = {
	{.compatible = "mediatek,camera_ois_hw",},
	{}
};
#endif

static struct platform_driver g_cam_ois_driver = {
	.probe = cam_ois_probe,
	.remove = cam_ois_remove,
	.driver = {
		.name = CAM_OIS_DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = g_cam_of_device_id,
#endif
	}
};


/***********************************************
 *
 ***********************************************/

int __init cam_ois_drv_init(void)
{
	LOG_INF(" - E");

	if (platform_device_register(&g_cam_ois_device)) {
		LOG_ERR("failed to register camera OIS device");
		return -ENODEV;
	}

	if (platform_driver_register(&g_cam_ois_driver)) {
		LOG_ERR("failed to register camera OIS driver i2C main\n");
		return -ENODEV;
	}

	// cam_ois_chrdev_register();
	if (cam_ois_sysfs_init()) {
		LOG_ERR("fail to init cam ois sysfs");
		return -ENODEV;
	}
	LOG_DBG(" - X");
	return 0;
}

void __exit cam_ois_drv_exit(void)
{
	cam_ois_destroy_sysfs();
	platform_driver_unregister(&g_cam_ois_driver);
	platform_device_unregister(&g_cam_ois_device);
	// cam_ois_chrdev_unregister();
}
// #if	1
// late_initcall(cam_ois_drv_init);
// #else
// module_init(cam_ois_drv_init);
// #endif
// module_exit(cam_ois_drv_exit);

MODULE_DESCRIPTION("Samsung Camera OIS Module Driver for MTK");
MODULE_LICENSE("GPL v2");
