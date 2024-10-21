// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "scp_feature_define.h"
#include "scp_helper.h"
#include "scp.h"
#include "scp_chre_manager.h"

/* scp mbox/ipi related */
#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "scp_ipi.h"

/* scp chre message buffer for IPI slots */
uint32_t scp_chre_ackdata[2];
uint32_t scp_chre_msgdata[2];
int scp_chre_ack2scp[2];

/* scp chre manager payload */
uint64_t scp_chre_payload_to_addr;
uint64_t scp_chre_payload_to_size;
uint64_t scp_chre_payload_from_addr;
uint64_t scp_chre_payload_from_size;

/* scp chre manager for scp status check */
static unsigned int scp_chre_stat_flag = SCP_CHRE_UNINIT;
static DECLARE_WAIT_QUEUE_HEAD(scp_chre_stat_queue);

/*
 * IPI for chre
 * @param id:   IPI id
 * @param prdata: callback function parameter
 * @param data:  IPI data
 * @param len: IPI data length
 */
static int scp_chre_ack_handler(unsigned int id, void *prdata, void *data,
				unsigned int len)
{
	return 0;
}

char __user *chre_buf;
static int scp_chre_ipi_handler(unsigned int id, void *prdata, void *data,
				unsigned int len)
{
	struct scp_chre_manager_payload chre_message;
	struct scp_chre_ipi_msg msg = *(struct scp_chre_ipi_msg *)data;

	if (msg.size > SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM) {
		pr_err("[SCP] %s: msg size exceeds maximum\n", __func__);
		scp_chre_ack2scp[0] = IPI_NO_MEMORY;
		scp_chre_ack2scp[1] = -1;
		return -EFAULT;
	}

	if (chre_buf == NULL) {
		pr_debug("[SCP] chre user hasn't wait ipi, try to send again\n");
		scp_chre_ack2scp[0] = IPI_PIN_BUSY;
		scp_chre_ack2scp[1] = -1;
		return -EFAULT;
	}

	chre_message.ptr = scp_chre_payload_from_addr;
	/* copy payload to user */
	if (copy_to_user(chre_buf, (void *)chre_message.ptr, msg.size)) {
		pr_err("[SCP] cher payload copy to user failed\n");
		return -EFAULT;
	}

	scp_chre_ack2scp[0] = IPI_ACTION_DONE;
	scp_chre_ack2scp[1] = msg.size;
	/* reset user buf address */
	chre_buf = NULL;

	return 0;
}
/* CHRE sysfs operations */
static ssize_t scp_chre_manager_read(struct file *filp,
		char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;

	if (count <= 0 || count > SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM) {
		pr_err("[SCP] %s: wrong size(%zd)\n", __func__, count);
		return -1;
	}

	chre_buf = buf;
	scp_chre_ack2scp[0] = 0;
	scp_chre_ack2scp[1] = -1;
	ret = mtk_ipi_recv_reply(&scp_ipidev, IPI_IN_SCP_HOST_CHRE,
			&scp_chre_ack2scp, PIN_IN_SIZE_SCP_HOST_CHRE);
	if (ret == IPI_FAKE_SIGNAL)
		scp_chre_ack2scp[1] = 0;

	return scp_chre_ack2scp[1];
}

static ssize_t scp_chre_manager_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos)
{
	int ret;
	struct scp_chre_ipi_msg msg;
	struct scp_chre_manager_payload chre_message;

	if (count <= 0 || count > (SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM + sizeof(msg))) {
		pr_err("[SCP] %s: wrong size(%zd)\n", __func__, count);
		return -EFAULT;
	}

	if (copy_from_user(&msg, buf, sizeof(msg))) {
		pr_err("[SCP] msg copy from user failed\n");
		return -EFAULT;
	}

	if (msg.magic != SCP_CHRE_MAGIC &&
		msg.size > SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM) {
		pr_err("[SCP] magic/size check fail\n");
		return -EFAULT;
	}

	chre_message.ptr = scp_chre_payload_to_addr;

	if (copy_from_user((void *)chre_message.ptr, (char *)(buf+8), msg.size)) {
		pr_err("[SCP] payload copy from user failed\n");
		return -EFAULT;
	}

	ret = mtk_ipi_send_compl(
			&scp_ipidev,
			IPI_OUT_HOST_SCP_CHRE,
			IPI_SEND_WAIT,		//blocking mode
			&msg,
			PIN_OUT_SIZE_HOST_SCP_CHRE,
			500);

	if (ret != IPI_ACTION_DONE)
		pr_notice("[SCP] %s: ipi failed, ret = %d\n", __func__, ret);
	else
		pr_debug("[SCP] %s: ipi ack done(%u)\n", __func__, scp_chre_ackdata[0]);

	return scp_chre_ackdata[1];
}

static long scp_chre_manager_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct scp_chre_manager_stat_info pkt;

	memset(&pkt, 0, sizeof(pkt));
	if (copy_from_user((void *)&pkt, (const void __user *)arg, sizeof(pkt))) {
		pr_err("[SCP] get chre stat pkt fail\n");
		return -EFAULT;
	}

	if (pkt.ret_user_stat_addr == 0) {
		pr_err("[SCP] get chre stat addr fail\n");
		return -EFAULT;
	}

	/* once user state != kernel state, return stat and sync to user immediately
	 * Or wait notification broadcast event.
	 */
	switch (cmd) {
	case SCP_CHRE_MANAGER_STAT_UNINIT:
	case SCP_CHRE_MANAGER_STAT_STOP:
	case SCP_CHRE_MANAGER_STAT_START:
		wait_event_interruptible(scp_chre_stat_queue,
			scp_chre_stat_flag != (cmd & IOCTL_NR_MASK));
		break;
	default:
		pr_err("[SCP] Unknown state, but still update new state to chre\n");
		break;
	}

	if (copy_to_user((void __user *)pkt.ret_user_stat_addr,
			&scp_chre_stat_flag, sizeof(unsigned int))) {
		pr_err("[SCP] update chre state fail\n");
		return -EFAULT;
	}

	pr_debug("%s: scp_chre_stat_flag: %d\n", __func__, scp_chre_stat_flag);

	return 0;
}

static const struct file_operations scp_chre_manager_fops = {
	.owner		= THIS_MODULE,
	.read           = scp_chre_manager_read,
	.write          = scp_chre_manager_write,
	.unlocked_ioctl = scp_chre_manager_ioctl,
	.compat_ioctl   = scp_chre_manager_ioctl,
};

static struct miscdevice scp_chre_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scp_chre_manager",
	.fops = &scp_chre_manager_fops
};

static int scp_chre_recover_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY:
		scp_chre_stat_flag = SCP_CHRE_START;
		break;
	case SCP_EVENT_STOP:
		scp_chre_stat_flag = SCP_CHRE_STOP;
		break;
	}

	wake_up_interruptible(&scp_chre_stat_queue);

	return NOTIFY_DONE;
}

static struct notifier_block scp_chre_recover_notifier = {
	.notifier_call = scp_chre_recover_event,
};

static int scp_chre_channel_init(void)
{
	int ret;

	scp_chre_payload_to_addr = (uint64_t)scp_get_reserve_mem_virt(SCP_CHRE_TO_MEM_ID);
	scp_chre_payload_to_size = (uint64_t)scp_get_reserve_mem_size(SCP_CHRE_TO_MEM_ID);
	scp_chre_payload_from_addr = (uint64_t)scp_get_reserve_mem_virt(SCP_CHRE_FROM_MEM_ID);
	scp_chre_payload_from_size = (uint64_t)scp_get_reserve_mem_size(SCP_CHRE_FROM_MEM_ID);

	if (scp_chre_payload_to_addr == 0 || scp_chre_payload_to_size == 0
	|| scp_chre_payload_from_addr == 0 || scp_chre_payload_from_size == 0) {
		pr_err("[SCP] chre memory reserve fail\n");
		return -1;
	}

	/* synchronization for send IPI post callback sequence */
	ret = mtk_ipi_register(
			&scp_ipidev,
			IPI_OUT_HOST_SCP_CHRE,
			NULL,
			(void *)scp_chre_ack_handler,
			(void *)&scp_chre_ackdata);
	if (ret) {
		pr_err("[SCP] IPI_OUT_HOST_SCP_CHRE register failed %d\n", ret);
		return -1;
	}

	/* receive IPI handler */
	ret = mtk_ipi_register(
			&scp_ipidev,
			IPI_IN_SCP_HOST_CHRE,
			(void *)scp_chre_ipi_handler,
			NULL,
			(void *)&scp_chre_msgdata);
	if (ret) {
		pr_err("[SCP] IPI_IN_SCP_HOST_CHRE register failed %d\n", ret);
		return -1;
	}

	return 0;
}

void scp_chre_manager_init(void)
{
	int ret;

	if (scp_chre_channel_init()) {
		pr_notice("[SCP] skip chre manager init\n");
		return;
	}

	scp_A_register_notify(&scp_chre_recover_notifier);

	ret = misc_register(&scp_chre_device);
	if (unlikely(ret != 0))
		pr_err("[SCP] chre misc register failed(%d)\n", ret);
}

void scp_chre_manager_exit(void)
{
	misc_deregister(&scp_chre_device);
}

