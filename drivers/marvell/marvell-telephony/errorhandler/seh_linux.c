/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */
/*******************************************************************
*
*  FILE:	 seh_linux.c
*
*  DESCRIPTION: This file either serve as an entry point or a function
*                               for writing or reading to/from the Linux SEH
*                               Device Driver.
*
*
*  HISTORY:
*    April, 2008 - Rovin Yu
*
*
*******************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
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
#include <linux/wakelock.h>

#include <mach/irqs.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <asm/pgtable.h>

#include <mach/cputype.h>

#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>

#include "seh_linux.h"
#include "linux_driver_types.h"
#include "watchdog.h"
#include "pxa9xx_cp.h"

#ifdef CONFIG_PXA_RAMDUMP
#include <asm/ptrace.h> /*pt_regs*/
#include <mach/ramdump.h>
#include <mach/ramdump_defs.h>
#ifdef RAMDUMP_PHASE_1_1 /* comes from ramdump h-files above: backwards compatibility with older kernel */
#define SEH_RAMDUMP_ENABLED 
#endif
#endif
unsigned short seh_open_count = 0;
DEFINE_SPINLOCK(seh_init_lock);

struct workqueue_struct *seh_int_wq = NULL;
struct work_struct seh_int_request;
struct seh_dev *seh_dev = NULL;
static struct wake_lock seh_wakeup;
static bool bCpResetOnReq = false;
static int seh_open(struct inode *inode, struct file *filp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long seh_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int seh_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static void seh_dev_release(struct device *dev);
static int seh_remove(struct platform_device *dev);
static int seh_probe(struct platform_device *dev);
static ssize_t seh_read(struct file *filp, char *buf, size_t count,     loff_t *f_pos);
static unsigned int seh_poll(struct file *filp, poll_table *wait);
static int seh_mmap(struct file *file, struct vm_area_struct *vma);
static int seh_release(struct inode *inode, struct file *filp);

extern void cp_releasecp(void);
extern void cp_holdcp(void);
extern const struct cp_load_table_head *get_cp_load_table(void);


static const char* const seh_name = "seh";
static struct file_operations seh_fops = {
	.open		= seh_open,
	.read		= seh_read,
	.release	= seh_release,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl		= seh_ioctl,
#else
	.ioctl		= seh_ioctl,
#endif
	.poll		= seh_poll,
	.mmap		= seh_mmap,
	.owner		= THIS_MODULE
};

static struct platform_device seh_device = {
	.name			= "seh",
	.id			= 0,
	.dev			= {
		.release	= seh_dev_release,
	},
};

static struct platform_driver seh_driver = {
	.probe		= seh_probe,
	.remove		= seh_remove,
	.driver		= {
		.name	= "seh",
		.owner	= THIS_MODULE,
	},
};

static struct miscdevice seh_miscdev = {
	MISC_DYNAMIC_MINOR,
	"seh",
	&seh_fops,
};

static void EehSaveErrorInfo(EehErrorInfo* info);
#ifdef SEH_RAMDUMP_ENABLED
static int ramfile_mmap(struct file *file, struct vm_area_struct *vma);

static const struct file_operations ramfile_fops = {
	.owner = THIS_MODULE,
	.mmap  = ramfile_mmap,
};

static struct miscdevice ramfile_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "ramfile",
	.fops		= &ramfile_fops,
};

#endif /*SEH_RAMDUMP_ENABLED*/
//#define DEBUG_SEH_LINUX
#ifdef DEBUG_SEH_LINUX
#define DBGMSG(fmt, args ...)     printk("SEH: " fmt, ## args)
#define ERRMSG(fmt, args ...) printk(KERN_ERR "SEH:" fmt, ## args)
#define ENTER()                 printk("SEH: ENTER %s\n", __FUNCTION__)
#define LEAVE()                 printk("SEH: LEAVE %s\n", __FUNCTION__)
#define FUNC_EXIT()                     printk("SEH: EXIT %s\n", __FUNCTION__)
#define DPRINT(fmt, args ...) printk(KERN_INFO "SEH:" fmt, ## args)
#else
#define DBGMSG(fmt, args ...)     do {} while (0)
#define ERRMSG(fmt, args ...) printk(KERN_ERR "SEH:" fmt, ## args)
#define ENTER()         do {} while (0)
#define LEAVE()         do {} while (0)
#define FUNC_EXIT()     do {} while (0)
#define DPRINT(fmt, args ...) printk(KERN_INFO "SEH:" fmt, ## args)
#endif

#define APMU_DEBUG_CP_HALT        (1 << 0)
#define APMU_DEBUG_CP_CLK_OFF_ACK (1 << 3)
/*
 * The top part for SEH interrupt handler.
 */
irqreturn_t seh_int_handler_low(int irq, void *dev_id)
{
	u8 apmu_debug;
	printk("CP down !!!!\n");

	/*
	 * mask CP_CLK_OFF_ACK
	 */
	apmu_debug = __raw_readb(APMU_DEBUG);
	printk("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
	apmu_debug |= (APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
	__raw_writeb(apmu_debug, APMU_DEBUG);
	printk("%s: APMU_DEBUG after %02x\n", __func__, __raw_readb(APMU_DEBUG));

	wake_lock_timeout(&seh_wakeup, HZ * 10);
	watchDogDeactive();
	printk("%s: APMU_DEBUG 0x%08x\n", __func__, __raw_readl(APMU_DEBUG));
	printk("%s: APMU_CORE_STATUS 0x%08x\n", __func__, __raw_readl(APMU_CORE_STATUS));
	printk("%s: MPMU_CPSR 0x%08x\n", __func__, __raw_readl(MPMU_CPSR));
	queue_work(seh_int_wq, &seh_int_request);
	return IRQ_HANDLED;
}
/*
 * The bottom part for SEH interrupt handler
 */
void seh_int_handler_high(struct work_struct *work)
{

	if (seh_dev)
	{
		if (down_interruptible(&seh_dev->read_sem))
		{
			ERRMSG("%s: fail to down semaphore\n", __FUNCTION__);
			return;
		}

		seh_dev->msg.msgId = EEH_WDT_INT_MSG;
		up(&seh_dev->read_sem);
		wake_up_interruptible(&seh_dev->readq);
	}

}

int seh_api_ioctl_handler(unsigned long arg)
{
	EehApiParams params;
	EEH_STATUS status = EEH_SUCCESS;

	if (copy_from_user(&params, (EehApiParams*)arg, sizeof(EehApiParams)))
		return -EFAULT;

	DPRINT("seh_api_ioctl_handler %d\n ", params.eehApiId);

	switch (params.eehApiId) // specific EEH API handler
	{

	case _EehInit:


		DBGMSG("Kernel Space EehInit Params:No params\n");

		enable_irq(IRQ_COMM_WDT_ID);
		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;


		break;

	case _EehDeInit:

		DBGMSG("Kernel Space EehDeInit Params:No params\n");

		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;

		break;

	case _EehInsertComm2Reset:
	{
    	EehInsertComm2ResetParam resetParams;

		DBGMSG("Kernel Space EehInsertComm2Reset Params: %x \n",params.params);
		if(params.params != NULL)
		{
			if (copy_from_user(&resetParams, params.params,sizeof(EehInsertComm2ResetParam)))
				return -EFAULT;

			if(resetParams.AssertType == EEH_AP_ASSERT)
			{
				disable_irq(IRQ_COMM_WDT_ID);
			}
		}
		cp_holdcp();

		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;

		break;
	}
	case _EehReleaseCommFromReset:

		DBGMSG("Kernel Space EehReleaseCommFromReset Params:No params\n");

		cp_releasecp();

		enable_irq(IRQ_COMM_WDT_ID);
		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;

		break;

	case _EehDisableCPUFreq:
	{
		DBGMSG("Kernel Space _EehDisableCPUFreq Params: No params\n");

		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;
	}

	break;

	case _EehEnableCPUFreq:
	{
		DBGMSG("Kernel Space _EehEnableCPUFreq Params: No params\n");

		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;
	}

	break;

	case _EehGetCPLoadAddr:
	{
		const struct cp_load_table_head *hdr = get_cp_load_table();
		EehGetCPLoadAddrParam param;

		DBGMSG("Kernel Space _EehGetCPLoadAddr\n");

		if(hdr)
		{
			param.arbel_load_addr = hdr->imageBegin - OFFSET_IN_ARBEL_IMAGE;
			if (copy_to_user(((EehApiParams*)arg)->params, &param, sizeof(param)))
				return -EFAULT;
		}
		else
		{
			status = EEH_ERROR;
		}

		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;
	}

	break;
	case _EehGetModemChipType:
	{
		EehGetModemChipTypeParam param;

		DBGMSG("Kernel Space _EehGetModemChipType\n");

		if(cpu_is_pxa988())
			param.modemChipType = EEH_MODEM_CHIP_TYPE_PXA988;
		else if(cpu_is_pxa986())
			param.modemChipType = EEH_MODEM_CHIP_TYPE_PXA986;
		else
			param.modemChipType = EEH_MODEM_CHIP_TYPE_UNKNOWN;

		if (copy_to_user(((EehApiParams*)arg)->params, &param, sizeof(param)))
			return -EFAULT;

		if (copy_to_user(&( (EehApiParams*)arg)->status, &status, sizeof(unsigned int)))
			return -EFAULT;
	}

	break;

	default:
		ERRMSG("WRONG Api = %d (params.eehApiId)\n", params.eehApiId);
		return -EFAULT;
	}
	return 0;
}

static int seh_probe(struct platform_device *dev)
{
	int ret;

	ENTER();

	seh_int_wq = create_workqueue("seh_rx_wq");

	INIT_WORK(&seh_int_request, seh_int_handler_high);

	seh_dev = (struct seh_dev*)kzalloc(sizeof(struct seh_dev), GFP_KERNEL);
	if (seh_dev == NULL)
	{
		ERRMSG("seh_probe: unable to allocate memory\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&(seh_dev->readq));
	sema_init(&seh_dev->read_sem, 1);
	seh_dev->dev = (struct device *)dev;

	ret = misc_register(&seh_miscdev);
	if (ret)
		ERRMSG("seh_probe: failed to call misc_register\n");
	wake_lock_init(&seh_wakeup, WAKE_LOCK_SUSPEND, "seh_wakeups");

	ret = request_irq(IRQ_COMM_WDT_ID, seh_int_handler_low, IRQF_DISABLED,
			     seh_name, NULL);
	if (ret)
	{
		ERRMSG("seh_probe: cannot register the COMM WDT interrupt\n");
		return ret;
	}
	LEAVE();

	return ret;
}

static int __init seh_init(void)
{
	int ret;

	ENTER();

	ret = platform_device_register(&seh_device);
	if (!ret)
	{
		ret = platform_driver_register(&seh_driver);
		if (ret)
		{
			ERRMSG("Cannot register SEH platform driver\n");
			platform_device_unregister(&seh_device);
		}
	}
	else
	{
		ERRMSG("Cannot register SEH platform device\n");
	}

#ifdef SEH_RAMDUMP_ENABLED
	/* register RAMDUMP device */
	ret = misc_register(&ramfile_miscdev);
	if (ret)
		printk(KERN_ERR "%s can't register device, err=%d\n", __func__, ret);
#endif /* SEH_RAMDUMP_ENABLED */
	LEAVE();

	return ret;
}

static void __exit seh_exit(void)
{
	ENTER();

	platform_driver_unregister(&seh_driver);
	platform_device_unregister(&seh_device);
#ifdef SEH_RAMDUMP_ENABLED
	misc_deregister(&ramfile_miscdev);
#endif /* SEH_RAMDUMP_ENABLED */

	LEAVE();

}

static int seh_open(struct inode *inode, struct file *filp)
{

	ENTER();

	/*  Save the device pointer */
	if (seh_dev)
		filp->private_data = seh_dev;
	else
		return -ERESTARTSYS;
	/*
	 * Only to prevent kernel preemption.
	 */
	spin_lock(&seh_init_lock);


	/* if seh driver is opened already. Just increase the count */
	if (seh_open_count)
	{
		seh_open_count++;
		spin_unlock(&seh_init_lock);
		DPRINT("seh is opened by process id: %d(\"%s\")\n", current->tgid,current->comm);
		return 0;
	}


	seh_open_count = 1;

	spin_unlock(&seh_init_lock);

	LEAVE();
	DPRINT("seh is opened by process id: %d(\"%s\")\n", current->tgid,current->comm);
	return 0;
}

static ssize_t seh_read(struct file *filp, char *buf, size_t count,
			loff_t *f_pos)
{
	struct seh_dev *dev;

	ENTER();

	dev = (struct seh_dev *)filp->private_data;
	if (dev == NULL)
		return -ERESTARTSYS;

	if (down_interruptible(&dev->read_sem))
		return -ERESTARTSYS;

	while (dev->msg.msgId == EEH_INVALID_MSG)
	{                               /* nothing to read */
		up(&dev->read_sem);     /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		DPRINT("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->readq, (dev->msg.msgId != EEH_INVALID_MSG)))
			return -ERESTARTSYS;  /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (down_interruptible(&dev->read_sem))
			return -ERESTARTSYS;
	}

	/* ok, data is there, return something */
	count = min(count, sizeof(EehMsgStruct));
	if (copy_to_user(buf, (void*)&(dev->msg), count))
	{
		up(&dev->read_sem);
		return -EFAULT;
	}

	DPRINT("\"%s\" did read %li bytes, msgId: %d\n", current->comm, (long)count, dev->msg.msgId);

	if(dev->msg.msgId == EEH_CP_SILENT_RESET_MSG)
		bCpResetOnReq = false;
	/* reset the msg info */
	memset(&(dev->msg), 0, sizeof(EehMsgStruct));
	up(&dev->read_sem);

	LEAVE();
	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long seh_ioctl(struct file *filp,
		     unsigned int cmd, unsigned long arg)
#else
static int seh_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
#endif

{
	int ret;
	struct seh_dev *dev;

	if (_IOC_TYPE(cmd) != SEH_IOC_MAGIC)
	{
		ERRMSG("seh_ioctl: seh magic number is wrong!\n");
		return -ENOTTY;
	}

	dev = (struct seh_dev *)filp->private_data;
	if (dev == NULL)
		return -EFAULT;

	DBGMSG("seh_ioctl,cmd=0x%x\n", cmd);

	ret = 0;

	switch (cmd)
	{
	case SEH_IOCTL_API:
		ret = seh_api_ioctl_handler(arg);
		break;
	case SEH_IOCTL_TEST:
		DPRINT("SEH_IOCTL_TEST\n");
		if (down_interruptible(&seh_dev->read_sem))
			return -EFAULT;

		seh_dev->msg.msgId = EEH_WDT_INT_MSG;
		up(&seh_dev->read_sem);
		wake_up_interruptible(&seh_dev->readq);
		break;
		case SEH_IOCTL_APP_ASSERT:
		{
			EehAppAssertParam param;

			if (copy_from_user(&param, (EehAppAssertParam*)arg, sizeof(EehAppAssertParam)))
        		return -EFAULT;

			DPRINT("Receive SEH_IOCTL_APP_ASSERT:%s\n", param.msgDesc);

			if (down_interruptible(&seh_dev->read_sem))
				return -EFAULT;

			disable_irq(IRQ_COMM_WDT_ID);
			seh_dev->msg.msgId = EEH_AP_ASSERT_MSG;
			strcpy(seh_dev->msg.msgDesc, param.msgDesc);
			strcpy(seh_dev->msg.processName, param.processName);
			up(&seh_dev->read_sem);
			wake_up_interruptible(&seh_dev->readq);

			break;
		}
		case SEH_IOCTL_EMULATE_PANIC:
			panic("EEH emulated panic\n");
			break;
		case SEH_IOCTL_SET_ERROR_INFO:
		{
			EehErrorInfo param;
			if (copy_from_user(&param, (EehErrorInfo*)arg, sizeof(EehErrorInfo)))
        			return -EFAULT;
			EehSaveErrorInfo(&param);
			break;
		}
		case SEH_IOCTL_CP_SILENT_RESET:
		{
			EehCpSilentResetParam param;
			if (copy_from_user(&param, (EehCpSilentResetParam*)arg, sizeof(EehCpSilentResetParam)))
				return -EFAULT;

			DPRINT("Receive SEH_IOCTL_CP_SILENT_RESET:%s\n", param.msgDesc);

			if (down_interruptible(&seh_dev->read_sem))
				return -EFAULT;

			if(bCpResetOnReq)
			{
				printk("CP already in the process of reset on requested\n");
				up(&seh_dev->read_sem);
				return -EFAULT;
			}
			bCpResetOnReq = true;
			disable_irq(IRQ_COMM_WDT_ID);
			cp_holdcp();
			seh_dev->msg.msgId = EEH_CP_SILENT_RESET_MSG;
			strcpy(seh_dev->msg.msgDesc, param.msgDesc);
			seh_dev->msg.force = param.force;
			up(&seh_dev->read_sem);
			wake_up_interruptible(&seh_dev->readq);

			break;
		}
		case SEH_IOCTL_APP_CRASH:
		{
			EehAppCrashParam param;
			if (copy_from_user(&param, (EehAppCrashParam*)arg, sizeof(EehAppCrashParam)))
				return -EFAULT;

			if (down_interruptible(&seh_dev->read_sem))
				return -EFAULT;

			DPRINT("Receive SEH_IOCTL_APP_CRASH:%s\n", param.processName);

			disable_irq(IRQ_COMM_WDT_ID);
			seh_dev->msg.msgId = EEH_AP_CRASH_MSG;
			strcpy(seh_dev->msg.processName, param.processName);
			up(&seh_dev->read_sem);
			wake_up_interruptible(&seh_dev->readq);

			break;
		}
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static unsigned int seh_poll(struct file *filp, poll_table *wait)
{
	struct seh_dev *dev = filp->private_data;
	unsigned int mask = 0;

	ENTER();

	poll_wait(filp, &dev->readq, wait);

	if (dev->msg.msgId != EEH_INVALID_MSG)  /* read finished */
		mask |= POLLIN | POLLRDNORM;

	LEAVE();

	return mask;
}

/* device memory map method */
static void seh_vma_open(struct vm_area_struct *vma)
{
	DBGMSG("SEH OPEN 0x%lx -> 0x%lx (0x%lx)\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT, vma->vm_page_prot);
}
static void seh_vma_close(struct vm_area_struct *vma)
{
	DBGMSG("SEH CLOSE 0x%lx -> 0x%lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

static struct vm_operations_struct vm_ops =
{
	.open	= seh_vma_open,
	.close	= seh_vma_close
};

/*
   vma->vm_end, vma->vm_start : specify the user space process address range assigned when mmap has been called;
   vma->vm_pgoff - is the physical address supplied by user to mmap in the last argument (off)
	     However, mmap restricts the offset, so we pass this shifted 12 bits right.
 */
static int seh_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_RESERVED | VM_IO);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma,
			       vma->vm_start,
			       pa, /* physical page index */
			       size,
			       vma->vm_page_prot))
	{
		ERRMSG("remap page range failed\n");
		return -ENXIO;
	}
	vma->vm_ops = &vm_ops;
	seh_vma_open(vma);
	return(0);
}

static int seh_release(struct inode *inode, struct file *filp)
{
	ENTER();

	spin_lock(&seh_init_lock);
	--seh_open_count;
	spin_unlock(&seh_init_lock);
	DPRINT("seh is closed by process id: %d(\"%s\")\n", current->tgid,current->comm);
		
	return 0;
}


static void seh_dev_release(struct device *dev)
{
	return;
}

static int seh_remove(struct platform_device *dev)
{
	ENTER();

	free_irq(IRQ_COMM_WDT_ID, NULL);
	misc_deregister(&seh_miscdev);
	kfree(seh_dev);
	destroy_workqueue(seh_int_wq);
	wake_lock_destroy(&seh_wakeup);
	LEAVE();
	return 0;
}


#ifdef SEH_RAMDUMP_ENABLED
#include <linux/swap.h> /*nr_free_buffer_pages and similiar*/
#include <linux/vmstat.h> /*global_page_state*/

/*#define RAMFILE_DEBUG*/
static void ramfile_vma_close(struct vm_area_struct *vma);
static struct vm_operations_struct ramfile_vm_ops = {
	.close = ramfile_vma_close
};
#define RAMFILE_LOW_WATERMARK 0x100000
/* NOTE: the size requested by user already accounts for ramfile header */
static int ramfile_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret_val = 0;
	unsigned long usize = vma->vm_end - vma->vm_start;
	void* pbuf;

	/* Check we don't exhaust all system memory to prevent crash before EEH
		is done with saving logs. Use the total free for now */
	
	unsigned int avail_mem = global_page_state(NR_FREE_PAGES)*PAGE_SIZE;
	printk(KERN_ERR "ramfile_mmap(0x%x), available 0x%x\n", usize, avail_mem);
	if (avail_mem < RAMFILE_LOW_WATERMARK) {
		printk(KERN_ERR "Rejected\n");
		return -ENOMEM;
	}	
	
	/* Note: kmalloc allocates physically continous memory.
	vmalloc would allocate potentially physically discontinuous memory.
	The advantage of vmalloc is that it would be able to allocate more 
	memory when physical memory available is fragmented */
	pbuf = kmalloc(usize, GFP_KERNEL);
#ifdef RAMFILE_DEBUG	
	printk(KERN_ERR "ramfile_mmap(0x%x): ka=%.8x ua=%.8x\n", usize, pbuf, (unsigned int)vma->vm_start);
#endif	
	if (!pbuf)
		return -ENOMEM;

	/* Allocated. Map this to user space and let it fill in the data.
		We do not want to waste a whole page for the ramfile_desc header,
		so we map all the buffer to user space, which should reserved the
		header area.
		We will fill the header and link it into the ramdump when user
		space is done and calls unmap. This way user mistake corrupting
		the header will not compromise the kernel operation.*/
	vma->vm_pgoff = __phys_to_pfn(__virt_to_phys((unsigned)pbuf)); /* needed during unmap/close */
		
	vma->vm_flags |= (VM_RESERVED|VM_IO);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret_val = remap_pfn_range(vma, (unsigned int)vma->vm_start,
				vma->vm_pgoff, usize, vma->vm_page_prot);
	if (ret_val < 0) {
		kfree(pbuf);
		return ret_val;
	}
	vma->vm_ops = &ramfile_vm_ops;
	return(0);
}
static void ramfile_vma_close(struct vm_area_struct *vma)
{
	struct ramfile_desc *prf;
	unsigned long usize = vma->vm_end - vma->vm_start;
					
	/* Fill in the ramfile desc (header) */
	prf = (struct ramfile_desc *)__phys_to_virt(__pfn_to_phys(vma->vm_pgoff));
	prf->payload_size = usize;
	prf->flags = RAMFILE_PHYCONT;
	memset((void*)&prf->reserved[0], 0, sizeof(prf->reserved));
	ramdump_attach_ramfile(prf);
#ifdef RAMFILE_DEBUG	
	printk(KERN_ERR "ramfile close 0x%x - linked into RDC\n", (unsigned)prf);
#endif
}

/* EehSaveErrorInfo: save the error ID/string into ramdump */
static void EehSaveErrorInfo(EehErrorInfo* info)
{
	char str[RAMDUMP_ERR_STR_LEN];
	char *s = 0;
	struct pt_regs regs;
	struct pt_regs *p = 0;
	unsigned err;
	err = (info->err == ERR_EEH_CP) ? RAMDUMP_ERR_EEH_CP : RAMDUMP_ERR_EEH_AP;

	if (info->str && !copy_from_user(str, info->str, sizeof(str))) {
		if ((s=strchr(str, '\n'))!=0) *s=0;
		else str[sizeof(str)-1]=0;
		s = &str;
	}
	if (info->regs && !copy_from_user(&regs, (struct pt_regs *)info->regs, sizeof(regs)))
		p = &regs;
	ramdump_save_dynamic_context(s, (int)err, NULL, p);
	printk(KERN_ERR "SEH saved error info: %.8x (%s)\n", err, s);
}

#else
static void EehSaveErrorInfo(EehErrorInfo* info) {}
#endif

module_init(seh_init);
module_exit(seh_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell System Error Handler.");

