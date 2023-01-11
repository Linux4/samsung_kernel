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

#include <linux/relay.h>
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
#include <linux/pm_wakeup.h>
#include <linux/slab.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/cputype.h>

#include "seh_linux.h"
#include "watchdog.h"
#include "pxa_cp_load.h"
#include "common_regs.h"
#include "acipcd.h"

#ifdef CONFIG_PXA_RAMDUMP
#include <linux/ptrace.h>		/*pt_regs */
#include <linux/ramdump.h>
/* comes from ramdump h-files above:backwards compatibility with older kernel */
#ifdef RAMDUMP_PHASE_1_1
#define SEH_RAMDUMP_ENABLED
#endif
#endif

/* map ripc status register 0xd403_d000 , need hold lock_for_ripc when touch it*/
static void __iomem *ripc_hwlock;

unsigned short seh_open_count;
DEFINE_SPINLOCK(seh_init_lock);

static int ee_config_b_cp_reset = -1;

struct workqueue_struct *seh_int_wq;
struct work_struct seh_int_request;
struct seh_dev *seh_dev;
static struct wakeup_source seh_wakeup;
static bool bCpResetOnReq;
static int seh_open(struct inode *inode, struct file *filp);
static long seh_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int seh_remove(struct platform_device *dev);
static int seh_probe(struct platform_device *dev);
static ssize_t seh_read(struct file *filp, char *buf, size_t count,
			loff_t *f_pos);
static unsigned int seh_poll(struct file *filp, poll_table *wait);
static int seh_mmap(struct file *file, struct vm_area_struct *vma);
static int seh_release(struct inode *inode, struct file *filp);
static int seh_suspend(struct platform_device *pdev, pm_message_t state);
static int seh_resume(struct platform_device *pdev);

#ifdef CONFIG_COMPAT
static long compat_seh_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg);
#endif

static const char *const seh_name = "seh";
static const struct file_operations seh_fops = {
	.open		= seh_open,
	.read		= seh_read,
	.release	= seh_release,
	.unlocked_ioctl		= seh_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_seh_ioctl,
#endif
	.poll		= seh_poll,
	.mmap		= seh_mmap,
	.owner		= THIS_MODULE
};

static struct of_device_id seh_dt_ids[] = {
	{ .compatible = "marvell,seh", },
	{}
};

static struct platform_driver seh_driver = {
	.probe		= seh_probe,
	.remove		= seh_remove,
	.suspend	= seh_suspend,
	.resume		= seh_resume,
	.driver		= {
		.name	= "seh",
		.of_match_table = seh_dt_ids,
		.owner	= THIS_MODULE,
	},
};

static struct miscdevice seh_miscdev = {
	MISC_DYNAMIC_MINOR,
	"seh",
	&seh_fops,
};

static void EehSaveErrorInfo(EehErrorInfo *info);
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

#endif /*SEH_RAMDUMP_ENABLED */
/*#define DEBUG_SEH_LINUX*/
#ifdef DEBUG_SEH_LINUX
#define DBGMSG(fmt, args ...)     pr_debug("SEH: " fmt, ## args)
#define ERRMSG(fmt, args ...) pr_err("SEH:" fmt, ## args)
#define ENTER()                 pr_debug("SEH: ENTER %s\n", __func__)
#define LEAVE()                 pr_debug("SEH: LEAVE %s\n", __func__)
#define FUNC_EXIT()             pr_debug("SEH: EXIT %s\n", __func__)
#define DPRINT(fmt, args ...) pr_info("SEH:" fmt, ## args)
#else
#define DBGMSG(fmt, args ...)     do {} while (0)
#define ERRMSG(fmt, args ...) pr_err("SEH:" fmt, ## args)
#define ENTER()         do {} while (0)
#define LEAVE()         do {} while (0)
#define FUNC_EXIT()     do {} while (0)
#define DPRINT(fmt, args ...) pr_info("SEH:" fmt, ## args)
#endif

void reset_ripc_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&lock_for_ripc, flags);
	if (!mmp_hwlock_get_status()) {
		/* below code is weird, but need it. if reading ripc_hwlock return
		 * ture, it indicates that cp has held this lock, so we need free it. if
		 * reading  ripc_hwlock return false, it indicates that we aquire
		 * this lock, so we also need free it
		 */
		if (__raw_readl(ripc_hwlock)) {
			/* write "1" to this register will free the resource */
			__raw_writel(0x1, ripc_hwlock);
		} else {
			__raw_writel(0x1, ripc_hwlock);
		}
	}
	spin_unlock_irqrestore(&lock_for_ripc, flags);
	pr_info("%s: reset ripc lock\n", __func__);
}

static void reset_fc_lock(void)
{
	if (cpu_is_pxa1908() ||
		cpu_is_pxa1936()) {
		int value;

		value = __raw_readl(APMU_FC_LOCK_STATUS);

		if (value & APMU_FC_LOCK_STATUS_CP_RD_STATUS) {
			/* CP got the lock */

			pr_info("%s: APMU_FC_LOCK_STATUS before: %08x\n",
				__func__, value);

			/* clear CP_RD_STATUS */
			value = __raw_readl(APMU_PMU_CC_CP);
			value |= APMU_PMU_CC_CP_CP_RD_ST_CLEAR;
			__raw_writel(value, APMU_PMU_CC_CP);
			value &= ~APMU_PMU_CC_CP_CP_RD_ST_CLEAR;
			__raw_writel(value, APMU_PMU_CC_CP);
			pr_info("%s: clear CP_RD_STATUS\n", __func__);

			pr_info("%s: APMU_FC_LOCK_STATUS after: %08x\n",
				__func__, __raw_readl(APMU_FC_LOCK_STATUS));
		}
	}
}

/*
 * The top part for SEH interrupt handler.
 */
irqreturn_t seh_int_handler_low(int irq, void *dev_id)
{
	u8 apmu_debug;
	pr_info("CP down !!!!\n");

	/*
	 * mask CP_CLK_OFF_ACK
	 */
	apmu_debug = __raw_readb(APMU_DEBUG);
	pr_info("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
	apmu_debug |= (APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
	__raw_writeb(apmu_debug, APMU_DEBUG);
	pr_info("%s: APMU_DEBUG after %02x\n", __func__,
	       __raw_readb(APMU_DEBUG));

	__pm_wakeup_event(&seh_wakeup, 10000);
	watchdog_deactive();
	reset_ripc_lock();
	reset_fc_lock();
	acipc_ap_block_cpuidle_axi(false);
	pr_info("%s: APMU_DEBUG 0x%08x\n", __func__,
	       __raw_readl(APMU_DEBUG));
	pr_info("%s: APMU_CORE_STATUS 0x%08x\n", __func__,
	       __raw_readl(APMU_CORE_STATUS));
	pr_info("%s: MPMU_CPSR 0x%08x\n", __func__,
	       __raw_readl(MPMU_CPSR));
	queue_work(seh_int_wq, &seh_int_request);
	return IRQ_HANDLED;
}

/*
 * The bottom part for SEH interrupt handler
 */
void seh_int_handler_high(struct work_struct *work)
{
	struct eeh_msg_list_struct *msg;
	if (seh_dev) {
		if (down_interruptible(&seh_dev->read_sem)) {
			ERRMSG("%s: fail to down semaphore\n", __func__);
			return;
		}

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		if (!msg) {
			up(&seh_dev->read_sem);
			DPRINT("malloc error\n");
			return;
		}
		msg->msg.msgId = EEH_WDT_INT_MSG;
		list_add_tail(&msg->msg_list, &seh_dev->msg_head);

		up(&seh_dev->read_sem);
		wake_up_interruptible(&seh_dev->readq);
	}
}

static void seh_change_msg_reset_state(void)
{
	struct eeh_msg_list_struct *m_curr, *m_next;
	list_for_each_entry_safe(m_curr, m_next, &seh_dev->msg_head, msg_list)
	{
		if (m_curr->msg.reset_cp) {
			m_curr->msg.reset_cp = 0;
			enable_irq(cp_watchdog->irq); /* match disable_irq */
		}
	}
}

int seh_api_ioctl_handler(unsigned long arg)
{
	EehApiParams params;
	EEH_STATUS status = EEH_SUCCESS;

	if (copy_from_user(&params, (EehApiParams *) arg, sizeof(EehApiParams)))
		return -EFAULT;

	DPRINT("seh_api_ioctl_handler %d\n ", params.eehApiId);

	switch (params.eehApiId) {	/* specific EEH API handler */

	case _EehInit:

		DBGMSG("Kernel Space EehInit Params:No params\n");

		enable_irq(cp_watchdog->irq);
		if (copy_to_user
		    (&((EehApiParams *) arg)->status, &status,
		     sizeof(unsigned int)))
			return -EFAULT;

		break;

	case _EehDeInit:

		DBGMSG("Kernel Space EehDeInit Params:No params\n");

		if (copy_to_user
		    (&((EehApiParams *) arg)->status, &status,
		     sizeof(unsigned int)))
			return -EFAULT;

		break;

	case _EehInsertComm2Reset:
	{
		EehInsertComm2ResetParam resetParams;

		DBGMSG("Kernel Space EehInsertComm2Reset Params: %x\n",
		       params.params);
		if (params.params != NULL) {
			if (copy_from_user
			    (&resetParams, params.params,
			     sizeof(EehInsertComm2ResetParam)))
				return -EFAULT;

			if (resetParams.AssertType == EEH_AP_ASSERT)
				disable_irq(cp_watchdog->irq);
		}
		cp_holdcp();
		reset_ripc_lock();
		reset_fc_lock();
		acipc_ap_block_cpuidle_axi(false);

		if (copy_to_user
		    (&((EehApiParams *) arg)->status, &status,
		     sizeof(unsigned int)))
			return -EFAULT;

		break;
	}
	case _EehChangeResetState:

		DBGMSG("Kernel Space EehChangeResetState Params:No params\n");

		if (down_interruptible(&seh_dev->read_sem))
			return -EFAULT;

		seh_change_msg_reset_state();

		up(&seh_dev->read_sem);

		if (copy_to_user
		    (&((EehApiParams *) arg)->status, &status,
		     sizeof(unsigned int)))
			return -EFAULT;

		break;
	case _EehReleaseCommFromReset:

		DBGMSG("Kernel Space EehReleaseCommFromReset"
		       " Params:No params\n");

		cp_releasecp();

		enable_irq(cp_watchdog->irq);
		if (copy_to_user
		    (&((EehApiParams *) arg)->status, &status,
		     sizeof(unsigned int)))
			return -EFAULT;

		break;

	case _EehGetCPLoadAddr:
	{
		EehGetCPLoadAddrParam param;

		DBGMSG("Kernel Space _EehGetCPLoadAddr\n");

		if (arbel_bin_phys_addr) {
			param.arbel_load_addr = arbel_bin_phys_addr;
			if (copy_to_user
			    (((EehApiParams *) arg)->params, &param,
			     sizeof(param)))
				return -EFAULT;
		} else {
			status = EEH_ERROR;
		}

		if (copy_to_user
		    (&((EehApiParams *) arg)->status, &status,
		     sizeof(unsigned int)))
			return -EFAULT;
	}

	break;

	default:
		ERRMSG("WRONG Api = %d (params.eehApiId)\n", params.eehApiId);
		return -EFAULT;
	}
	return 0;
}


int ripc_init(struct platform_device *pdev)
{
	struct resource *res;
	int ret = 0;

	/* iomap ripc register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "%s: no iomem defined\n", __func__);
		ret = -ENOMEM;
		goto out;
	}
	ripc_hwlock = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ripc_hwlock)) {
		dev_err(&pdev->dev, "ioremap ripc_hwlock failure\n");
		ret = (int)PTR_ERR(ripc_hwlock);
	}

out:
	return ret;
}

static int seh_suspend(struct platform_device *pdev, pm_message_t state)
{
	return watchdog_suspend();
}

static int seh_resume(struct platform_device *pdev)
{
	return watchdog_resume();
}

static int seh_probe(struct platform_device *dev)
{
	int ret;

	ENTER();

	ret = ripc_init(dev);
	if (ret < 0)
		return ret;

	if (cp_watchdog_probe(dev) < 0)
		return -ENOENT;

	/* map common register base address */
	if (!map_apmu_base_va()) {
		pr_err("error to ioremap APMU_BASE_ADDR\n");
		ret = -ENOMEM;
		goto remove_watchdog;
	}

	if (!map_mpmu_base_va()) {
		pr_err("error to ioremap MPMU_BASE_ADDR\n");
		ret = -ENOMEM;
		goto unmap_apmu;
	}

	seh_int_wq = create_workqueue("seh_rx_wq");
	if (!seh_int_wq) {
		ERRMSG("create workqueue error\n");
		ret = -ENOMEM;
		goto unmap_mpmu;
	}

	INIT_WORK(&seh_int_request, seh_int_handler_high);

	seh_dev = kzalloc(sizeof(struct seh_dev), GFP_KERNEL);
	if (seh_dev == NULL) {
		ERRMSG("seh_probe: unable to allocate memory\n");
		ret = -ENOMEM;
		goto free_workqueue;
	}

	INIT_LIST_HEAD(&seh_dev->msg_head);
	INIT_LIST_HEAD(&seh_dev->ast_head);
	init_waitqueue_head(&(seh_dev->readq));
	sema_init(&seh_dev->read_sem, 1);
	sema_init(&seh_dev->ast_sem, 1);
	seh_dev->dev = (struct device *)dev;

	ret = misc_register(&seh_miscdev);
	if (ret) {
		ERRMSG("seh_probe: failed to call misc_register\n");
		goto free_mem;
	}

	wakeup_source_init(&seh_wakeup, "seh_wakeups");

	ret = request_irq(cp_watchdog->irq, seh_int_handler_low,
		IRQF_DISABLED, seh_name, NULL);
	if (ret) {
		ERRMSG("seh_probe: cannot register the COMM WDT interrupt\n");
		goto dereg_misc;
	}

	watchdog_count_stop_fp = watchdog_count_stop;

	LEAVE();
	return 0;

dereg_misc:
	misc_deregister(&seh_miscdev);
	wakeup_source_trash(&seh_wakeup);
free_mem:
	kfree(seh_dev);
free_workqueue:
	destroy_workqueue(seh_int_wq);
unmap_mpmu:
	unmap_mpmu_base_va();
unmap_apmu:
	unmap_apmu_base_va();
remove_watchdog:
	cp_watchdog_remove(dev);

	LEAVE();

	return ret;
}

static int reboot_notifier_func(struct notifier_block *this,
	unsigned long code, void *cmd)
{
	pr_info("reboot notifier, hold CP and reset ripc\n");
	cp_holdcp();
	reset_ripc_lock();
	reset_fc_lock();
	return 0;
}

static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_notifier_func,
};

static int __init seh_init(void)
{
	int ret;

	ENTER();

	ret = platform_driver_register(&seh_driver);
	if (ret)
		ERRMSG("Cannot register SEH platform driver\n");
	else
		register_reboot_notifier(&reboot_notifier);

#ifdef SEH_RAMDUMP_ENABLED
	/* register RAMDUMP device */
	ret = misc_register(&ramfile_miscdev);
	if (ret)
		pr_err("%s can't register device, err=%d\n", __func__,
		       ret);
#endif /* SEH_RAMDUMP_ENABLED */
	LEAVE();

	return ret;
}

static void __exit seh_exit(void)
{
	ENTER();
	unregister_reboot_notifier(&reboot_notifier);
	platform_driver_unregister(&seh_driver);
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
	if (seh_open_count) {
		seh_open_count++;
		spin_unlock(&seh_init_lock);
		DPRINT("seh is opened by process id: %d(%s)\n",
		       current->tgid, current->comm);
		return 0;
	}

	seh_open_count = 1;

	spin_unlock(&seh_init_lock);

	LEAVE();
	DPRINT("seh is opened by process id: %d(%s)\n", current->tgid,
	       current->comm);
	return 0;
}

static ssize_t seh_read(struct file *filp, char *buf, size_t count,
			loff_t *f_pos)
{
	struct seh_dev *dev;
	struct eeh_msg_list_struct *msg;

	ENTER();

	dev = (struct seh_dev *)filp->private_data;
	if (dev == NULL)
		return -ERESTARTSYS;

	if (down_interruptible(&dev->read_sem))
		return -ERESTARTSYS;

	while (list_empty(&dev->msg_head)) {	/* nothing to read */
		up(&dev->read_sem);		/* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		DPRINT("%s reading: going to sleep\n", current->comm);
		if (wait_event_interruptible
		    (dev->readq, (!list_empty(&dev->msg_head))))
			/* signal: tell the fs layer to handle it */
			return -ERESTARTSYS;
		/* otherwise loop, but first reacquire the lock */
		if (down_interruptible(&dev->read_sem))
			return -ERESTARTSYS;
	}
	msg = list_first_entry(&dev->msg_head,
			struct eeh_msg_list_struct, msg_list);
	if (msg) { /* Message Queue is not NULL */
		/* ok, data is there, return something */
		count = min(count, sizeof(EehMsgStruct));
		if (copy_to_user(buf, (void *)&(msg->msg), count)) {
			up(&dev->read_sem);
			return -EFAULT;
		}
		DPRINT("%s read %li bytes, msgId: %d\n", current->comm,
		       (long)count, msg->msg.msgId);

		if (msg->msg.msgId == EEH_CP_SILENT_RESET_MSG)
			bCpResetOnReq = false;

		/* delete the message from list and free it */
		list_del(&msg->msg_list);
		kfree(msg);
		up(&dev->read_sem);
	} else {
		/* Message Queue is NULL */
		up(&dev->read_sem);
		return -EFAULT;
	}

	LEAVE();
	return count;
}

static struct eeh_ast_list_struct *assert_caused_crash(EehAppCrashParam *param)
{
	struct eeh_ast_list_struct *a_curr, *a_next;
	list_for_each_entry_safe(a_curr, a_next, &seh_dev->ast_head, ast_list)
	{
		if (!strcmp(param->processName, a_curr->param.processName)) {
			list_del_init(&a_curr->ast_list);
			return a_curr;
		}
	}

	return NULL;
}

static long seh_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct seh_dev *dev;

	if (_IOC_TYPE(cmd) != SEH_IOC_MAGIC) {
		ERRMSG("seh_ioctl: seh magic number is wrong!\n");
		return -ENOTTY;
	}

	dev = (struct seh_dev *)filp->private_data;
	if (dev == NULL)
		return -EFAULT;

	DBGMSG("seh_ioctl,cmd=0x%x\n", cmd);

	ret = 0;

	switch (cmd) {
	case SEH_IOCTL_API:
		ret = seh_api_ioctl_handler(arg);
		break;
	case SEH_IOCTL_TEST:
		{
			struct eeh_msg_list_struct *msg;
			DPRINT("SEH_IOCTL_TEST\n");
			if (down_interruptible(&seh_dev->read_sem))
				return -EFAULT;

			msg = kzalloc(sizeof(*msg), GFP_KERNEL);
			if (!msg) {
				up(&seh_dev->read_sem);
				DPRINT("malloc error\n");
				return -ENOMEM;
			}
			msg->msg.msgId = EEH_WDT_INT_MSG;
			list_add_tail(&msg->msg_list, &seh_dev->msg_head);

			up(&seh_dev->read_sem);
			wake_up_interruptible(&seh_dev->readq);
			break;
		}
	case SEH_IOCTL_APP_ASSERT:
		{
			EehAppAssertParam param;
			struct eeh_ast_list_struct *ast;

			if (copy_from_user
			    (&param, (EehAppAssertParam *) arg,
			     sizeof(EehAppAssertParam)))
				return -EFAULT;

			DPRINT("Receive SEH_IOCTL_APP_ASSERT:%s\n",
			       param.msgDesc);

			/* operation assert list */
			if (down_interruptible(&seh_dev->ast_sem))
				return -EFAULT;

			ast = kzalloc(sizeof(*ast), GFP_KERNEL);
			if (!ast) {
				up(&seh_dev->ast_sem);
				DPRINT("malloc error\n");
				return -ENOMEM;
			}
			strcpy(ast->param.msgDesc, param.msgDesc);
			strcpy(ast->param.processName, param.processName);
			list_add_tail(&ast->ast_list, &seh_dev->ast_head);

			up(&seh_dev->ast_sem);
			break;
		}
	case SEH_IOCTL_EMULATE_PANIC:
		panic("EEH emulated panic\n");
		break;
	case SEH_IOCTL_SET_ERROR_INFO:
		{
			EehErrorInfo param;
			if (copy_from_user
			    (&param, (EehErrorInfo *) arg,
			     sizeof(EehErrorInfo)))
				return -EFAULT;
			EehSaveErrorInfo(&param);
			break;
		}
	case SEH_IOCTL_CP_SILENT_RESET:
		{
			EehCpSilentResetParam param;
			if (copy_from_user
			    (&param, (EehCpSilentResetParam *) arg,
			     sizeof(EehCpSilentResetParam)))
				return -EFAULT;

			DPRINT("Receive SEH_IOCTL_CP_SILENT_RESET:%s\n",
			       param.msgDesc);

			ret = trigger_modem_crash(param.force, param.msgDesc);
			break;
		}
	case SEH_IOCTL_APP_CRASH:
		{
			EehAppCrashParam param;
			struct eeh_msg_list_struct *msg;
			struct eeh_ast_list_struct *ast;

			if (copy_from_user
			    (&param, (EehAppCrashParam *) arg,
			     sizeof(EehAppCrashParam)))
				return -EFAULT;

			DPRINT("Receive SEH_IOCTL_APP_CRASH:%s\n",
			       param.processName);

			if (down_interruptible(&seh_dev->read_sem))
				return -EFAULT;

			if (down_interruptible(&seh_dev->ast_sem)) {
				up(&seh_dev->read_sem);
				return -EFAULT;
			}

			msg = kzalloc(sizeof(*msg), GFP_KERNEL);
			if (!msg) {
				up(&seh_dev->ast_sem);
				up(&seh_dev->read_sem);
				DPRINT("malloc error\n");
				return -ENOMEM;
			}
			ast = assert_caused_crash(&param);
			if (ast) { /* Process crash is caused by assert */
				msg->msg.msgId = EEH_AP_ASSERT_MSG;
				strcpy(msg->msg.msgDesc, ast->param.msgDesc);
				kfree(ast);
			} else { /* Normal crash */
				msg->msg.msgId = EEH_AP_CRASH_MSG;
			}
			strcpy(msg->msg.processName, param.processName);
			msg->msg.reset_cp = param.reset_cp;
			list_add_tail(&msg->msg_list, &seh_dev->msg_head);

			/* For process which recovery need reset CP
			 * irq will be enabled after released CP,
			 * But for process which do not need reset CP
			 * no need to disable irq, otherwise, the
			 * irq enable/disable will be unmatched */
			if (param.reset_cp)
				disable_irq(cp_watchdog->irq);

			up(&seh_dev->ast_sem);
			up(&seh_dev->read_sem);
			wake_up_interruptible(&seh_dev->readq);

			break;
		}
	case SEH_IOCTL_SET_EECONFIG_B_CP_RESET:
		{
			ee_config_b_cp_reset = (int)arg;
			DPRINT("Recv SEH_IOCTL_SET_EECONFIG_B_CP_RESET: %d\n",
					ee_config_b_cp_reset);
			break;
		}
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
int compat_seh_api_ioctl_handler(struct file *filp, unsigned int cmd,
		unsigned long arg)
{

	struct EehApiParams32 __user *argp = (void __user *)arg;
	EehApiParams __user *buf;
	compat_uptr_t param_addr;
	void __user *param_ptr;
	int ret = 0;

	buf = compat_alloc_user_space(sizeof(*buf));
	if (!access_ok(VERIFY_WRITE, buf, sizeof(*buf))
	    || !access_ok(VERIFY_WRITE, argp, sizeof(*argp)))
		return -EFAULT;

	if (__copy_in_user(buf, argp, offsetof(struct EehApiParams32, params))
	    || __get_user(param_addr, &argp->params)
	    || __put_user(compat_ptr(param_addr), &buf->params))
		return -EFAULT;

	ret = seh_ioctl(filp, cmd, (unsigned long)buf);

	if (ret)
		return ret;

	if (__copy_in_user(argp, buf, offsetof(struct EehApiParams32, params))
	    || __get_user(param_ptr, &buf->params)
	    || __put_user((compat_uptr_t)(unsigned long)param_ptr,
	    &argp->params))
		return -EFAULT;

	return 0;
}

int compat_set_err_inf(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct EehErrorInfo32 __user *argp = (void __user *)arg;
	EehErrorInfo __user *buf;
	compat_uptr_t param_addr;

	buf = compat_alloc_user_space(sizeof(*buf));
	if (!access_ok(VERIFY_WRITE, buf, sizeof(*buf))
	    || !access_ok(VERIFY_WRITE, argp, sizeof(*argp)))
		return -EFAULT;

	if (__get_user(param_addr, &argp->str)
		|| __put_user(compat_ptr(param_addr), &buf->str)
		|| __get_user(param_addr, &argp->regs)
		|| __put_user(compat_ptr(param_addr), &buf->regs))
		return -EFAULT;

	return seh_ioctl(filp, cmd, (unsigned long)buf);
}

static long compat_seh_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case SEH_IOCTL_API:
		ret = compat_seh_api_ioctl_handler(filp, cmd, arg);
		break;
	case SEH_IOCTL_SET_ERROR_INFO:
		ret = compat_set_err_inf(filp, cmd, arg);
		break;
	default:
		ret = seh_ioctl(filp, cmd, arg);
		break;
	}

	return ret;
}
#endif

int read_ee_config_b_cp_reset(void)
{
	return ee_config_b_cp_reset;
}
EXPORT_SYMBOL(read_ee_config_b_cp_reset);

static unsigned int seh_poll(struct file *filp, poll_table *wait)
{
	struct seh_dev *dev = filp->private_data;
	unsigned int mask = 0;

	ENTER();

	poll_wait(filp, &dev->readq, wait);

	if (!list_empty(&dev->msg_head))  /* read finished */
		mask |= POLLIN | POLLRDNORM;

	LEAVE();

	return mask;
}

/* device memory map method */
static void seh_vma_open(struct vm_area_struct *vma)
{
	DBGMSG("SEH OPEN 0x%lx -> 0x%lx (0x%lx)\n", vma->vm_start,
	       vma->vm_pgoff << PAGE_SHIFT, vma->vm_page_prot);
}

static void seh_vma_close(struct vm_area_struct *vma)
{
	DBGMSG("SEH CLOSE 0x%lx -> 0x%lx\n", vma->vm_start,
	       vma->vm_pgoff << PAGE_SHIFT);
}

static struct vm_operations_struct vm_ops = {
	.open = seh_vma_open,
	.close = seh_vma_close
};

/*
 * vma->vm_end, vma->vm_start: specify the user space process address range
 *                             assigned when mmap has been called;
 * vma->vm_pgoff: is the physical address supplied by user to mmap in the
 *                last argument (off)
 * However, mmap restricts the offset, so we pass this shifted 12 bits right.
 */
static int seh_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* check if addr is normal memory */
	if (pfn_valid(pa)) {
		if (remap_pfn_range(vma, vma->vm_start, pa,
				       size, vma->vm_page_prot)) {
			ERRMSG("remap page range failed\n");
			return -ENXIO;
		}
	} else if (io_remap_pfn_range(vma, vma->vm_start, pa,
			       size, vma->vm_page_prot)) {
		ERRMSG("remap page range failed\n");
		return -ENXIO;
	}
	vma->vm_ops = &vm_ops;
	seh_vma_open(vma);
	return 0;
}

static int seh_release(struct inode *inode, struct file *filp)
{
	ENTER();

	spin_lock(&seh_init_lock);
	--seh_open_count;
	spin_unlock(&seh_init_lock);
	DPRINT("seh is closed by process id: %d(%s)\n", current->tgid,
	       current->comm);

	return 0;
}

static int seh_remove(struct platform_device *dev)
{
	struct eeh_msg_list_struct *m_curr, *m_next;
	struct eeh_ast_list_struct *a_curr, *a_next;

	ENTER();

	free_irq(cp_watchdog->irq, NULL);
	misc_deregister(&seh_miscdev);
	list_for_each_entry_safe(a_curr, a_next, &seh_dev->ast_head, ast_list)
	{
		list_del(&a_curr->ast_list);
		kfree(a_curr);
	}
	list_for_each_entry_safe(m_curr, m_next, &seh_dev->msg_head, msg_list)
	{
		list_del(&m_curr->msg_list);
		kfree(m_curr);
	}
	kfree(seh_dev);
	destroy_workqueue(seh_int_wq);
	wakeup_source_trash(&seh_wakeup);
	cp_watchdog_remove(dev);

	/* unmap common register */
	unmap_apmu_base_va();
	unmap_mpmu_base_va();

	LEAVE();
	return 0;
}

#ifdef SEH_RAMDUMP_ENABLED
#include <linux/swap.h>		/*nr_free_buffer_pages and similiar */
#include <linux/vmstat.h>	/*global_page_state */

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
	void *pbuf;

	/* Check we don't exhaust all system memory to prevent crash before EEH
	   is done with saving logs. Use the total free for now */

	unsigned int avail_mem = global_page_state(NR_FREE_PAGES) * PAGE_SIZE;
	pr_err("ramfile_mmap(0x%lx), available 0x%x\n", usize,
	       avail_mem);
	if (avail_mem < RAMFILE_LOW_WATERMARK) {
		pr_err("Rejected\n");
		return -ENOMEM;
	}

	/* Note: kmalloc allocates physically continous memory.
	   vmalloc would allocate potentially physically discontinuous memory.
	   The advantage of vmalloc is that it would be able to allocate more
	   memory when physical memory available is fragmented */
	pbuf = kmalloc(usize, GFP_KERNEL);
#ifdef RAMFILE_DEBUG
	pr_info("ramfile_mmap(0x%lx): ka=%lx ua=%lx\n",
		(unsigned long)usize, (unsigned long)pbuf,
		(unsigned long)vma->vm_start);
#endif
	if (!pbuf)
		return -ENOMEM;

	/* Allocated. Map this to user space and let it fill in the data.
	   We do not want to waste a whole page for the ramfile_desc header,
	   so we map all the buffer to user space, which should reserved the
	   header area.
	   We will fill the header and link it into the ramdump when user
	   space is done and calls unmap. This way user mistake corrupting
	   the header will not compromise the kernel operation. */
	/* needed during unmap/close */
	vma->vm_pgoff = __phys_to_pfn(__virt_to_phys((unsigned long)pbuf));

	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret_val = remap_pfn_range(vma, (unsigned long)vma->vm_start,
				  vma->vm_pgoff, usize, vma->vm_page_prot);
	if (ret_val < 0) {
		kfree(pbuf);
		return ret_val;
	}
	vma->vm_ops = &ramfile_vm_ops;
	return 0;
}

static void ramfile_vma_close(struct vm_area_struct *vma)
{
	struct ramfile_desc *prf;
	unsigned long usize = vma->vm_end - vma->vm_start;

	/* Fill in the ramfile desc (header) */
	prf =
	    (struct ramfile_desc *)__phys_to_virt(__pfn_to_phys(vma->vm_pgoff));
	prf->payload_size = usize;
	prf->flags = RAMFILE_PHYCONT;
	memset((void *)&prf->reserved[0], 0, sizeof(prf->reserved));
	ramdump_attach_ramfile(prf);
#ifdef RAMFILE_DEBUG
	pr_err("ramfile close 0x%lx - linked into RDC\n",
		(unsigned long)prf);
#endif
}

/* EehSaveErrorInfo: save the error ID/string into ramdump */
static void EehSaveErrorInfo(EehErrorInfo *info)
{
	char str[RAMDUMP_ERR_STR_LEN];
	char *s = 0;
	struct pt_regs regs;
	struct pt_regs *p = 0;
	unsigned err;
	err =
	    (info->err == ERR_EEH_CP) ? RAMDUMP_ERR_EEH_CP : RAMDUMP_ERR_EEH_AP;

	if (info->str && !copy_from_user(str, info->str, sizeof(str))) {
		s = strchr(str, '\n');
		if (s)
			*s = 0;
		else
			str[sizeof(str) - 1] = 0;
		s = str;
	}
	if (info->regs
	    && !copy_from_user(&regs, (struct pt_regs *)info->regs,
			       sizeof(regs)))
		p = &regs;
	ramdump_save_dynamic_context(s, (int)err, NULL, p);
	pr_err("SEH saved error info: %.8x (%s)\n", err, s);
}

#else
static void EehSaveErrorInfo(EehErrorInfo *info)
{
}
#endif

int trigger_modem_crash(int force_reset, const char *disc)
{
	struct eeh_msg_list_struct *msg;

	if (seh_dev == NULL)
		return -EFAULT;

	if (down_interruptible(&seh_dev->read_sem))
		return -EFAULT;

	if (bCpResetOnReq) {
		pr_info("CP already in the process of reset\n");
		up(&seh_dev->read_sem);
		return -EFAULT;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		up(&seh_dev->read_sem);
		DPRINT("malloc error\n");
		return -ENOMEM;
	}
	bCpResetOnReq = true;
	disable_irq(cp_watchdog->irq);
	msg->msg.msgId = EEH_CP_SILENT_RESET_MSG;
	strcpy(msg->msg.msgDesc, disc);
	msg->msg.force = force_reset;
	list_add_tail(&msg->msg_list, &seh_dev->msg_head);
	up(&seh_dev->read_sem);
	wake_up_interruptible(&seh_dev->readq);
	return 0;
}
EXPORT_SYMBOL(trigger_modem_crash);

module_init(seh_init);
module_exit(seh_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell System Error Handler.");
