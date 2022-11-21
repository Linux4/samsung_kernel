/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/aio.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <mach/irqs.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include "diag.h"
#include <linux/cdev.h>
#include "shm_share.h"
#include <linux/skbuff.h>
#include "msocket.h"
#include <linux/delay.h>


static int diagstub_open(struct inode *inode, struct file *filp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long diagstub_ioctl(struct file *filp,
			  unsigned int cmd, unsigned long arg);
#else
static int diagstub_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
#endif

int diagstub_major = 0;
int diagstub_minor = 0;
struct cdev *diagdev_devices;
static struct class *diagdev_class;
static struct file_operations diagstub_fops = {
	.open	= diagstub_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl	= diagstub_ioctl,
#else
	.ioctl	= diagstub_ioctl,
#endif
	.owner	= THIS_MODULE
};

extern int diagsockfd;
extern BOOL diagDiscAcked;
extern BOOL diagConnAcked;
extern BOOL diagHostUsbConn;
int gRcvdCpPkt = 0;
int gRcvdCpLongPkt = 0;
int gUsbSentCpPkt = 0;
int gUsbSentCpPktError = 0;


#if     1
#define DPRINT(fmt, args ...)     printf(fmt, ## args)
#define DBGMSG(fmt, args ...)     printk("Diag: " fmt, ## args)
#define ENTER()                 printk("Diag: ENTER %s\n", __FUNCTION__)
#define LEAVE()                 printk("Diag: LEAVE %s\n", __FUNCTION__)
#define FUNC_EXIT()                     printk("Diag: EXIT %s\n", __FUNCTION__)
#define ASSERT(a)  if (!(a)) { \
		while (1) { \
			printk("ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __FUNCTION__, __LINE__); \
		} \
}
#else
#define DPRINT(fmt, args ...)     do {} while (0)
#define DBGMSG(fmt, args ...)     do {} while (0)
#define ENTER()         do {} while (0)
#define LEAVE()         do {} while (0)
#define FUNC_EXIT()     do {} while (0)
#define ASSERT(a)  if (!(a)) { \
		while (1) { \
			printk("ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __FUNCTION__, __LINE__); \
		} \
}
#endif

static int diagstub_open(struct inode *inode, struct file *filp)
{
	ENTER();

	LEAVE();
	return 0;

}
void diagUsbConnectNotify(void)
{
	UINT32 dataId = DiagExtIfConnectedMsg;
	UINT8 datamsg[sizeof(DIAGHDR) + sizeof(UINT32)];
	DIAGHDR *pHeader;
	int retrycunt = 3;

	diagHostUsbConn = TRUE;
	diagDiscAcked = FALSE;
	/* If we have alreadly received Conn Ack, we can ignore it */
	if (diagConnAcked)
		return;

	pHeader = (DIAGHDR *)datamsg;
	pHeader->packetlen = sizeof(UINT32);
	pHeader->seqNo = 0;
	pHeader->msgType = dataId;
	*(UINT32 *)(datamsg+sizeof(DIAGHDR)) = dataId;
	while (retrycunt > 0)
	{
		if (!diagConnAcked)
		{
			diagTxRawData(datamsg, sizeof(datamsg));
			retrycunt--;
		}
		else
		{
			break;
		}
	}

//	if(diagsockfd < 0)
//		diagsockfd = msocket(DIAG_PORT);
}

void diagUsbDisConnectNotify(void)
{
	UINT32 dataId = DiagExtIfDisconnectedMsg;
	int retrycunt = 3;
	UINT8 datamsg[sizeof(DIAGHDR) + sizeof(UINT32)];
	DIAGHDR *pHeader;

	pHeader = (DIAGHDR *)datamsg;
	pHeader->packetlen = sizeof(UINT32);
	pHeader->seqNo = 0;
	pHeader->msgType = dataId;
	*(UINT32 *)(datamsg+sizeof(DIAGHDR)) = dataId;
	diagHostUsbConn = FALSE;
	diagConnAcked = FALSE;

	while (retrycunt > 0)
	{
		if (!diagDiscAcked)
		{
			diagTxRawData(datamsg, sizeof(datamsg));
			retrycunt--;
		}
		else
		{
			break;
		}
	}
//	mclose(DIAG_PORT);
//	diagsockfd = -1;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long diagstub_ioctl(struct file *filp,
			  unsigned int cmd, unsigned long arg)
#else
static int diagstub_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg)
#endif
{
	switch (cmd)
	{
	case DIAGSTUB_USBSTART:
		InitDiagChannel();
		break;
	case DIAGSTUB_USBINIT:
		InitUSBChannel();
		break;
	}

	return 0;
}
static void diagdev_setup_cdev(struct cdev *dev, int index)
{
	int err, devno = MKDEV(diagstub_major, diagstub_minor + index);

	ENTER();

	cdev_init(dev, &diagstub_fops);
	dev->owner = THIS_MODULE;
	dev->ops = &diagstub_fops;
	err = cdev_add(dev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding diagstub%d", err, index);

	LEAVE();
}

void diagdev_cleanup_module(void)
{
	dev_t devno = MKDEV(diagstub_major, diagstub_minor);

	ENTER();

	DeInitDiagChannel();

	/* Get rid of our char dev entries */
	if (diagdev_devices)
	{
		{
			cdev_del(diagdev_devices);
			device_destroy(diagdev_class,
				       MKDEV(diagstub_major, diagstub_minor));
		}
		kfree(diagdev_devices);
	}

	class_destroy(diagdev_class);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);



	LEAVE();

}

static int __init diagstub_init(void)
{
	dev_t dev = 0;
	int result;
	char name[256];

	ENTER();
	if (diagstub_major)
	{
		dev = MKDEV(diagstub_major, diagstub_minor);
		result = register_chrdev_region(dev, 1, "diagdatastub");
	}
	else
	{
		result = alloc_chrdev_region(&dev, diagstub_minor, 1, "diagdatastub");
		diagstub_major = MAJOR(dev);
	}
	if (result < 0)
	{
		printk(KERN_WARNING "diagstub: can't get major %d\n", diagstub_minor);
		return result;
	}

	diagdev_devices = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!diagdev_devices)
	{
		result = -ENOMEM;
		goto fail;
	}
	memset(diagdev_devices, 0, sizeof(struct cdev));

	/* Initialize each device. */
	diagdev_class = class_create(THIS_MODULE, "diagdatastub");
	sprintf(name, "%s", "diagdatastub");
	device_create(diagdev_class, NULL,
		      MKDEV(diagstub_major, diagstub_minor), NULL, name);

	diagdev_setup_cdev(diagdev_devices, 0);

	//Move InitDiagChannel to ioctl:DIAGSTUB_USBSTART
	//InitDiagChannel();

	LEAVE();
	return 0;
 fail:
	diagdev_cleanup_module();

	return result;
}

static void __exit diagstub_exit(void)
{
	ENTER();
	diagdev_cleanup_module();
	LEAVE();
}


module_init(diagstub_init);
module_exit(diagstub_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell Diag stub.");


EXPORT_SYMBOL(diagSendUsbData);
EXPORT_SYMBOL(diagRegisterRxCallBack);
EXPORT_SYMBOL(diagUnregisterRxCallBack);
EXPORT_SYMBOL(diagUsbConnectNotify);
EXPORT_SYMBOL(diagUsbDisConnectNotify);

EXPORT_SYMBOL(gRcvdCpPkt);
EXPORT_SYMBOL(gRcvdCpLongPkt);
EXPORT_SYMBOL(gUsbSentCpPkt);
EXPORT_SYMBOL(gUsbSentCpPktError);




