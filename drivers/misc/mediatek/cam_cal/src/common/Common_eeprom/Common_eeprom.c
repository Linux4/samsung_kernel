/*
 * Driver for EEPROM
 *
 *
 */


#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "cam_cal.h"
#include "cam_cal_define.h"

#include "Common_eeprom.h"

/* #define EEPROMGETDLT_DEBUG */
#define EEPROM_DEBUG
#ifdef EEPROM_DEBUG
#define EEPROMDB printk
#else
#define EEPROMDB(x, ...)
#endif


static DEFINE_SPINLOCK(g_EEPROMLock);	/* for SMP */
#define EEPROM_I2C_BUSNUM 1

/*******************************************************************************
*
********************************************************************************/
#define EEPROM_ICS_REVISION 1	/* seanlin111208 */
/*******************************************************************************
*
********************************************************************************/
#define EEPROM_DRVNAME "CAM_CAL_DRV"
#define EEPROM_I2C_GROUP_ID 0

/*******************************************************************************
*
********************************************************************************/
/* #define FM50AF_EEPROM_I2C_ID 0x28 */
#define FM50AF_EEPROM_I2C_ID 0xA1


/*******************************************************************************/
/* define LSC data for M24C08F EEPROM on L10 project */
/********************************************************************************/
#define SampleNum 221
#define Read_NUMofEEPROM 2
#define Boundary_Address 256
#define EEPROM_Address_Offset 0xC
#define EEPROM_DYNAMIC_ALLOCATE_DEVNO 1


/*******************************************************************************
*
********************************************************************************/
static struct i2c_client *g_pstI2Cclient;

/* 81 is used for V4L driver */
static dev_t g_EEPROMdevno = MKDEV(EEPROM_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_pEEPROM_CharDrv;
/* static spinlock_t g_EEPROMLock; */
/* spin_lock(&g_EEPROMLock); */
/* spin_unlock(&g_EEPROMLock); */

static struct class *EEPROM_class;
static atomic_t g_EEPROMatomic;
/* static DEFINE_SPINLOCK(kdeeprom_drv_lock); */
/* spin_lock(&kdeeprom_drv_lock); */
/* spin_unlock(&kdeeprom_drv_lock); */


/*******************************************************************************
*
********************************************************************************/

/*******************************************************************************
*
********************************************************************************/
/* maximun read length is limited at "I2C_FIFO_SIZE" in I2c-mt6516.c which is 8 bytes */


/*******************************************************************************
*
********************************************************************************/
#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int EEPROM_Ioctl(struct inode *a_pstInode,
			struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
#else
static long EEPROM_Ioctl(struct file *file, unsigned int a_u4Command, unsigned long a_u4Param)
#endif
{
	int i4RetValue = 0;
	u8 *pBuff = NULL;
	stCAM_CAL_INFO_STRUCT *ptempbuf;

#ifdef EEPROMGETDLT_DEBUG
	struct timeval ktv1, ktv2;
	unsigned long TimeIntervalUS;
#endif

	EEPROMDB("[COMMON_EEPROM]1 In to IOCTL %x %x\n", _IOC_DIR(a_u4Command), _IOC_WRITE);
	EEPROMDB("[COMMON_EEPROM]2 In to IOCTL %x %x\n", CAM_CALIOC_G_READ, CAM_CALIOC_S_WRITE);


	if (_IOC_NONE == _IOC_DIR(a_u4Command)) {
		/*_IOC_NONE == _IOC_DIR(a_u4Command) */
	} else {
		pBuff = kmalloc(sizeof(stCAM_CAL_INFO_STRUCT), GFP_KERNEL);

		if (NULL == pBuff) {
			EEPROMDB("[S24EEPROM] ioctl allocate mem failed\n");
			return -ENOMEM;
		}

		if (_IOC_WRITE & _IOC_DIR(a_u4Command)) {
			if (copy_from_user((u8 *) pBuff, (u8 *) a_u4Param,
				sizeof(stCAM_CAL_INFO_STRUCT))) {	/* get input structure address */
				kfree(pBuff);
				EEPROMDB("[S24EEPROM] ioctl copy from user failed\n");
				return -EFAULT;
			}
		}
	}

	ptempbuf = (stCAM_CAL_INFO_STRUCT *) pBuff;

	EEPROMDB("[COMMON_EEPROM] In to IOCTL %x (0x%08x)\n", ptempbuf->u4Offset,
		 (ptempbuf->u4Offset & 0x000FFFFF));


	if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x00024C32) {
#if defined(GT24C32A_EEPROM)
		EEPROMDB("[GT24C32A] Jump to IOCTL\n");

		i4RetValue = GT24C32A_EEPROM_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[GT24C32A] Not defined in config\n");
#endif

	} else if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x000BCB32) {
#if defined(BRCB032GWZ_3)
		EEPROMDB("[BRCB032GW] Jump to IOCTL\n");

		i4RetValue = BRCB032GW_3_EEPROM_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[BRCB032GW] Not defined in config\n");
#endif

	} else if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x000BCC64) {
#if defined(BRCC064GWZ_3)
		EEPROMDB("[BRCC064GW] Jump to IOCTL\n");

		i4RetValue = BRCC064GWZ_3_EEPROM_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[BRCC064GW] Not defined in config\n");
#endif
	} else if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x000d9716) {
#if defined(DW9716_EEPROM)
		EEPROMDB("[DW9716EEPROM] Jump to IOCTL\n");

		i4RetValue = DW9716_EEPROM_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[DW9716EEPROM] Not defined in config\n");
#endif
	} else if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x00024C36) {
#if defined(AT24C36D)
		EEPROMDB("[AT24C36D] Jump to IOCTL\n");

		i4RetValue = AT24C36D_EEPROM_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[AT24C36D] Not defined in config\n");
#endif
	} else if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x0000C533) {
#if defined(ZC533_EEPROM)
		EEPROMDB("[ZC533_EEPROM] Jump to IOCTL\n");

		i4RetValue = ZC533_EEPROM_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[ZC533_EEPROM] Not defined in config\n");
#endif

	} else if ((ptempbuf->u4Offset & 0x000FFFFF) == 0x00008858) {
#if defined(OV8858_OTP)
		EEPROMDB("[OV8858_OTP] Jump to IOCTL\n");

		i4RetValue = OV8858_otp_Ioctl(file, a_u4Command, a_u4Param);
#else
		EEPROMDB("[OV8858_OTP] Not defined in config\n");
#endif

	} else {
		EEPROMDB("[COMMON_EEPROM] Masic number is wrong\n");
	}

	kfree(pBuff);

	return i4RetValue;
}


static u32 g_u4Opened;
/* #define */
/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
static int EEPROM_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	EEPROMDB("[COMMON_EEPROM] EEPROM_Open Client %p\n", g_pstI2Cclient);
	spin_lock(&g_EEPROMLock);
	if (g_u4Opened) {
		spin_unlock(&g_EEPROMLock);
		return -EBUSY;
	}
	g_u4Opened = 1;
	atomic_set(&g_EEPROMatomic, 0);

	spin_unlock(&g_EEPROMLock);

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int EEPROM_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_EEPROMLock);

	g_u4Opened = 0;

	atomic_set(&g_EEPROMatomic, 0);

	spin_unlock(&g_EEPROMLock);

	return 0;
}


static const struct file_operations g_stEEPROM_fops = {
	.owner = THIS_MODULE,
	.open = EEPROM_Open,
	.release = EEPROM_Release,
	/* .ioctl = EEPROM_Ioctl */
	.unlocked_ioctl = EEPROM_Ioctl
};

static int RegisterEEPROMCharDrv(void)
{
	struct device *EEPROM_device = NULL;

#if EEPROM_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_EEPROMdevno, 0, 1, EEPROM_DRVNAME)) {
		EEPROMDB("[S24EEPROM] Allocate device no failed\n");

		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_EEPROMdevno, 1, EEPROM_DRVNAME)) {
		EEPROMDB("[COMMON_EEPROM] Register device no failed\n");

		return -EAGAIN;
	}
#endif

	/* Allocate driver */
	g_pEEPROM_CharDrv = cdev_alloc();

	if (NULL == g_pEEPROM_CharDrv) {
		unregister_chrdev_region(g_EEPROMdevno, 1);

		EEPROMDB("[COMMON_EEPROM] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pEEPROM_CharDrv, &g_stEEPROM_fops);

	g_pEEPROM_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pEEPROM_CharDrv, g_EEPROMdevno, 1)) {
		EEPROMDB("[COMMON_EEPROM] Attatch file operation failed\n");

		unregister_chrdev_region(g_EEPROMdevno, 1);

		return -EAGAIN;
	}

	EEPROM_class = class_create(THIS_MODULE, "EEPROMdrv");
	if (IS_ERR(EEPROM_class)) {
		int ret = PTR_ERR(EEPROM_class);

		EEPROMDB("Unable to create class, err = %d\n", ret);
		return ret;
	}
	EEPROM_device = device_create(EEPROM_class, NULL, g_EEPROMdevno, NULL, EEPROM_DRVNAME);

	return 0;
}

#if 0
static void UnregisterEEPROMCharDrv(void)
{
	/* Release char driver */
	cdev_del(g_pEEPROM_CharDrv);

	unregister_chrdev_region(g_EEPROMdevno, 1);

	device_destroy(EEPROM_class, g_EEPROMdevno);
	class_destroy(EEPROM_class);
}
#endif

static int EEPROM_probe(struct platform_device *pdev)
{
	return 0;
}

static int EEPROM_remove(struct platform_device *pdev)
{
	return 0;
}

/* platform structure */
static struct platform_driver g_stEEPROM_Driver = {
	.probe = EEPROM_probe,
	.remove = EEPROM_remove,
	.driver = {
		   .name = EEPROM_DRVNAME,
		   .owner = THIS_MODULE,
		   }
};


static struct platform_device g_stEEPROM_Device = {
	.name = EEPROM_DRVNAME,
	.id = 0,
	.dev = {
		}
};

static int __init EEPROM_i2C_init(void)
{
	RegisterEEPROMCharDrv();

	if (platform_device_register(&g_stEEPROM_Device)) {
		EEPROMDB("failed to register S24EEPROM driver, 2nd time\n");
		return -ENODEV;
	}


	if (platform_driver_register(&g_stEEPROM_Driver)) {
		EEPROMDB("failed to register S24EEPROM driver\n");
		return -ENODEV;
	}



	return 0;
}

static void __exit EEPROM_i2C_exit(void)
{
	platform_driver_unregister(&g_stEEPROM_Driver);
}
module_init(EEPROM_i2C_init);
module_exit(EEPROM_i2C_exit);

MODULE_DESCRIPTION("EEPROM driver");
MODULE_AUTHOR("Sean Lin <Sean.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");
