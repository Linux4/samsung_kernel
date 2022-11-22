/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <soc/sprd/board.h>
#include <asm/memory.h>
#include <linux/memblock.h>
#include <linux/poll.h>
#include <linux/device.h>
#ifdef CONFIG_SPRD_SCXX30_DMC_FREQ
#include <linux/devfreq.h>
#endif
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <soc/sprd/hardware.h>

#define MAX_CHAR_NUM               128

#define IQ_BUF_INIT                0x5A5A5A5A
#define IQ_BUF_WRITE_FINISHED      0x5A5A8181
#define IQ_BUF_READ_FINISHED       0x81815A5A
#define IQ_BUF_READING             0x81005a00

#define IQ_BUF_OPEN                0x424F504E
#define IQ_BUF_LOCK                0x424C434B
#define DATA_AP_MOVE               0x4441504D
#define DATA_AP_MOVING             0x504D4441
#define DATA_CP_INJECT             0x44435049
#define DATA_AP_MOVE_FINISH        DATA_CP_INJECT
#define DATA_RESET                 0x44525354
#define MAX_PB_HEADER_SIZE		   0x100

#define IQ_TRANSFER_SIZE (500*1024)

#define SPRD_IQ_CLASS_NAME		"sprd_iq"

typedef enum {
	CMD_GET_IQ_BUF_INFO = 0x0,
	CMD_GET_IQ_PB_INFO,
	CMD_SET_IQ_CH_TYPE = 0x80,
	CMD_SET_IQ_WR_FINISHED,
	CMD_SET_IQ_RD_FINISHED,
	CMD_SET_IQ_MOVE_FINISHED,
} ioctl_cmd_t;

typedef enum {
	IQ_USB_MODE = 0,
	IQ_SLOG_MODE,
	PLAY_BACK_MODE,
} iq_mode_t;

iq_mode_t cur_iq_ch = IQ_USB_MODE;

struct iq_buf_info {
	unsigned base_offs;
	unsigned data_len;
};

struct iq_buf_info b_info;

struct iq_header{
	volatile u32  WR_RD_FLAG;
	u32  data_addr;
	u32  data_len;
	u32  reserved;
};

struct iq_pb_data_header {
	u32 data_status;
	u32 iqdata_offset;
	u32 iqdata_length;
	char iqdata_filename[MAX_CHAR_NUM];
};

struct sprd_iq {
	struct iq_header *head_1;
	struct iq_header *head_2;
	struct iq_pb_data_header *ipd_head;
	wait_queue_head_t wait;
};

static struct sprd_iq g_iq = {0};

static struct task_struct *iq_thread;

static uint iq_base;

static u32 iq_mode = 0;

static struct class *sprd_iq_class;

module_param_named(iq_base, iq_base, uint, 0444);

extern ssize_t vser_iq_write(char *buf, size_t count);
extern void kernel_vser_register_callback(void *function);
extern phys_addr_t sprd_iq_addr(void);

static int __init early_mode(char *str)
{
	printk("early_mode \n");
	if (!memcmp(str, "iq", 2))
		iq_mode = 1;

    return 0;
}

int in_iqmode(void)
{
	return (iq_mode == 1);
}

early_param("androidboot.mode", early_mode);


static ssize_t sprd_iq_write(u32 paddr, u32 length)
{
	void *vaddr;
	u32 len;
	u32 send_num = 0;
	ssize_t ret;
	printk(KERN_ERR "sprd_iq_write 0x%x, 0x%x \n", paddr, length);
	while (length - send_num > 0) {
		vaddr = NULL;

		len =( length -send_num > IQ_TRANSFER_SIZE) ? IQ_TRANSFER_SIZE: (length -send_num);
		vaddr = __va(paddr+send_num);
		if (!vaddr) {
			printk(KERN_ERR "sprd iq no memory  \n");
			msleep(10);
			continue;
		}
		//pr_debug("sprd_iq_write 0x%x, 0x%x \n", paddr+send_num, len);
		//printk("sprd_iq_write 0x%x, 0x%x  %x\n", paddr+send_num, (u32)vaddr, len);
#if 0	/* Don't use vserial */
		ret = vser_iq_write(vaddr, len);
#else
		ret = -1;
#endif
		if (ret < 0) {
			printk(KERN_ERR "sprd iq usb send error %d \n", ret);
			msleep(200);
		} else {
			send_num += ret;
			//printk(KERN_ERR "sprd iq send_num = 0x%x, ret = 0x%x \n", send_num, ret);
		}
	}
	return send_num;
}

static int sprd_iq_thread(void *data)
{
	struct sprd_iq *p_iq = (struct sprd_iq*)data;
	struct sched_param param = {.sched_priority = 80};

	if (NULL == p_iq->head_1 || NULL == p_iq->head_2) {
		return -EPERM;
	}

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		if (PLAY_BACK_MODE == cur_iq_ch) {
			if (DATA_AP_MOVE == g_iq.ipd_head->data_status) {
				g_iq.ipd_head->data_status = DATA_AP_MOVING;
				wake_up_interruptible(&g_iq.wait);

			} else if (DATA_AP_MOVE == g_iq.ipd_head->data_status) {
				memset(__va(sprd_iq_addr()), 0, SPRD_IQ_SIZE);
				g_iq.head_1->WR_RD_FLAG = IQ_BUF_OPEN;
			}
		}

		if (p_iq->head_1->WR_RD_FLAG == IQ_BUF_WRITE_FINISHED) {
			p_iq->head_1->WR_RD_FLAG = IQ_BUF_READING;
			if (IQ_USB_MODE == cur_iq_ch) {
				sprd_iq_write(p_iq->head_1->data_addr, p_iq->head_1->data_len);

			} else if (IQ_SLOG_MODE == cur_iq_ch) {
				wake_up_interruptible(&g_iq.wait);

			}

		} else if (p_iq->head_2->WR_RD_FLAG == IQ_BUF_WRITE_FINISHED) {
			p_iq->head_2->WR_RD_FLAG = IQ_BUF_READING;
			if (IQ_USB_MODE == cur_iq_ch) {
				sprd_iq_write(p_iq->head_2->data_addr, p_iq->head_2->data_len);

			} else if (IQ_SLOG_MODE == cur_iq_ch) {
				wake_up_interruptible(&g_iq.wait);
			}

		} else {
			printk(KERN_INFO "iq buf is busy\n");
			msleep(1000);
		}
	}
	return 0;
}

static void sprd_iq_complete(char *buf,  int length)
{
	if (IQ_USB_MODE != cur_iq_ch)
		return;

	u32 vaddr = (u32)buf;
	pr_debug("sprd_iq_complete 0x%x, 0x%x \n", vaddr, length);
	if (vaddr + length == (u32)__va(g_iq.head_1->data_addr + g_iq.head_1->data_len))
		g_iq.head_1->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
	if (vaddr + length == (u32)__va(g_iq.head_2->data_addr + g_iq.head_2->data_len))
		g_iq.head_2->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
}

#ifdef CONFIG_SPRD_SCXX30_DMC_FREQ
static unsigned int sprd_iq_dmc_notify(struct devfreq_dbs *h, unsigned int state)
{
	if (state == DEVFREQ_PRE_CHANGE)
		return 1;
}

struct devfreq_dbs dmc_iq_notify = {
	.level = 0,
	.devfreq_notifier = sprd_iq_dmc_notify,
};
#endif

static long iq_mem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk(KERN_INFO "enter iq_mem_ioctl cmd = 0x%x\n", cmd);

	if (NULL == g_iq.head_1 || NULL == g_iq.head_2)
		return -EPERM;

	switch (cmd) {
	case CMD_GET_IQ_BUF_INFO:
		if (IQ_BUF_READING == g_iq.head_1->WR_RD_FLAG) {
			b_info.base_offs = g_iq.head_1->data_addr - iq_base;
			b_info.data_len = g_iq.head_1->data_len;

		} else if (IQ_BUF_READING == g_iq.head_2->WR_RD_FLAG) {
			b_info.base_offs = g_iq.head_2->data_addr - iq_base;
			b_info.data_len = g_iq.head_2->data_len;
		}

		if (copy_to_user((void *)arg, (void *)&b_info, sizeof(struct iq_buf_info))) {
			printk(KERN_ERR "copy iq buf info to user space failed.\n");
			return -EFAULT;
		}

		break;

	case CMD_GET_IQ_PB_INFO:
		if (PLAY_BACK_MODE != cur_iq_ch) {
			printk(KERN_ERR "current mode is not playback mode\n");
			return -EINVAL;
		}

		if (copy_to_user((void *)arg,
				 (void *)g_iq.ipd_head,
				 sizeof(struct iq_pb_data_header))) {
			printk(KERN_ERR "copy iq playback data info to user space failed.\n");
			return -EFAULT;
		}

		break;

	case CMD_SET_IQ_CH_TYPE:
		if (arg < IQ_USB_MODE || arg > PLAY_BACK_MODE) {
			printk(KERN_ERR "iq ch type invalid\n");
			return -EINVAL;
		}

		if (IQ_SLOG_MODE == arg) {
			if (IQ_BUF_READING == g_iq.head_1->WR_RD_FLAG || IQ_BUF_READING == g_iq.head_2->WR_RD_FLAG) {
				wake_up_interruptible(&g_iq.wait);
			}

		} else if (PLAY_BACK_MODE == arg) {
			g_iq.ipd_head = (struct iq_header*)__va(sprd_iq_addr()
								+ sizeof(struct iq_header));
			g_iq.head_1->data_addr = sprd_iq_addr()
						 + MAX_PB_HEADER_SIZE;
			g_iq.head_1->WR_RD_FLAG = IQ_BUF_OPEN;
		}

		cur_iq_ch = arg;

		break;

	case CMD_SET_IQ_RD_FINISHED:
		if (IQ_BUF_READING == g_iq.head_1->WR_RD_FLAG) {
			g_iq.head_1->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
		} else if (IQ_BUF_READING == g_iq.head_2->WR_RD_FLAG) {
			g_iq.head_2->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
		}

		break;

	case CMD_SET_IQ_MOVE_FINISHED:
		if (PLAY_BACK_MODE != cur_iq_ch) {
			printk(KERN_ERR "current mode is not playback mode\n");
			return -EINVAL;
		}

		if (copy_from_user((void *)g_iq.ipd_head,
				   (void *)arg,
				   sizeof(struct iq_pb_data_header))) {
			printk(KERN_ERR "copy iq playback data info from user space failed.\n");
			return -EFAULT;
		}

		g_iq.head_1->data_len = g_iq.ipd_head->iqdata_length;

		if (DATA_AP_MOVE_FINISH == g_iq.ipd_head->data_status) {
			g_iq.head_1->WR_RD_FLAG = IQ_BUF_LOCK;
		}

		break;

	default:
		break;

	}

	return 0;
}

static unsigned int iq_mem_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned mask = 0;

	if (NULL == g_iq.head_1 || NULL == g_iq.head_2)
		return POLLERR;

	poll_wait(filp, &g_iq.wait, wait);

	if (IQ_SLOG_MODE == cur_iq_ch) {
		if (IQ_BUF_READING == g_iq.head_1->WR_RD_FLAG ||
		    IQ_BUF_READING == g_iq.head_2->WR_RD_FLAG) {
			mask |= (POLLIN | POLLRDNORM);
		}
	} else if (PLAY_BACK_MODE == cur_iq_ch) {
		if (DATA_AP_MOVING == g_iq.ipd_head->data_status) {
			printk(KERN_ERR "I/Q playback status DATA_AP_MOVING\n");
			mask |= (POLLIN | POLLRDNORM);
		}
	}

	return mask;
}

static int iq_mem_nocache_mmap(struct file *filp, struct vm_area_struct *vma)
{
    size_t size = vma->vm_end - vma->vm_start;
    printk(KERN_INFO "iq_mem_nocache_mmap\n");

#if 1
	if (iq_base == ~0x0) {
		printk(KERN_ERR "Invalid iq_base(0x%x).\n", iq_base);
		return -EAGAIN;
	}

	if (size <= SPRD_IQ_SIZE) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (remap_pfn_range(vma,vma->vm_start, iq_base>>PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
			printk(KERN_ERR "remap_pfn_range failed\n");
			return -EAGAIN;
		}

	} else {
		printk(KERN_ERR "map size to big, exceed maxsize(0x%xMB).\n", SPRD_IQ_SIZE/(1024*1024));
		return -EAGAIN;
	}
#else
	if (vma->vm_pgoff < iq_base || (vma->vm_pgoff + size - iq_base) > IQ_BUF_MAX_SIZE) {
		printk(KERN_ERR "mmap area(0x%x - 0x%x) is not support.\n", vma->vm_pgoff, vma->vm_pgoff + size);
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		printk(KERN_ERR "remap_pfn_range failed\n");
		return -EAGAIN;
	}

#endif

	printk(KERN_INFO "iq mmap %x,%x,%x\n", (unsigned int)PAGE_SHIFT,
	       (unsigned int)vma->vm_start,
	       (unsigned int)(vma->vm_end - vma->vm_start));
	return 0;
}

static int iq_mem_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "iq_mem_open called\n");
	return 0;
}

static int iq_mem_release (struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "iq_mem_release\n");
	return 0;
}

static const struct file_operations iq_mem_fops =
{
	.owner = THIS_MODULE,
	.poll = iq_mem_poll,
	.unlocked_ioctl =iq_mem_ioctl,
	.mmap  =iq_mem_nocache_mmap,
	.open =iq_mem_open,
	.release =iq_mem_release,
};

static struct miscdevice iq_mem_dev = {
	.minor   = MISC_DYNAMIC_MINOR,
	.name   = "iq_mem",
	.fops   = &iq_mem_fops,
};

ssize_t show_iq_param(struct class *class, struct class_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,"iq_base: 0x%x\ncur_iq_ch: 0x%x\n", iq_base, cur_iq_ch);
}

ssize_t show_iq_buf_status(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if (iq_base == ~0x0)
		return 0;

	ret += snprintf(buf + ret, PAGE_SIZE,
			"\n------- iq memory header -------\n");
	ret += snprintf(buf + ret, PAGE_SIZE,
			"\trw_flag: 0x%x\n\tdata_addr: 0x%x\n\tdata_len: 0x%x\n",
			g_iq.head_1->WR_RD_FLAG,
			g_iq.head_1->data_addr,
			g_iq.head_1->data_len);

	if (PLAY_BACK_MODE == cur_iq_ch) {
		ret += snprintf(buf + ret, PAGE_SIZE,
				"\n------- iq playback data header -------\n");
		ret += snprintf(buf + ret, PAGE_SIZE,
				"\tdata_status: 0x%x\n\tiqdata_offset: 0x%x\n\tiqdata_length: 0x%x\n\tfile: %s\n",
				g_iq.ipd_head->data_status,
				g_iq.ipd_head->iqdata_offset,
				g_iq.ipd_head->iqdata_length,
				g_iq.ipd_head->iqdata_filename);
	}

	return ret;
}

static CLASS_ATTR(iq_param, S_IRUGO, show_iq_param, NULL);
static CLASS_ATTR(iq_buf_status, S_IRUGO, show_iq_buf_status, NULL);

static int __init sprd_iq_init(void)
{
	int ret;

	if (!in_iqmode())
		return -EINVAL;
	if (sprd_iq_addr() == ~0x0)
		return -ENOMEM;

	ret = misc_register(&iq_mem_dev);
	if (ret) {
		printk(KERN_ERR "cannot register iq_mmap_dev ret = (%d)\n",ret);
		return ret;
	}

	sprd_iq_class = class_create(THIS_MODULE, SPRD_IQ_CLASS_NAME);
	if (IS_ERR(sprd_iq_class)) {
		printk(KERN_ERR "class create error(%s)\n", SPRD_IQ_CLASS_NAME);
		goto error1;
	}

	ret = class_create_file(sprd_iq_class, &class_attr_iq_param);
	if (ret) {
		printk(KERN_ERR "iq: couldn't create iq_param attribute\n");
		goto error2;
	}

	ret = class_create_file(sprd_iq_class, &class_attr_iq_buf_status);
	if (ret) {
		printk(KERN_ERR "iq: couldn't create iq_buf_status attribute\n");
		goto error2;
	}

#ifdef CONFIG_SPRD_SCXX30_DMC_FREQ
	devfreq_notifier_register(&dmc_iq_notify);
#endif
	init_waitqueue_head(&g_iq.wait);
	g_iq.head_1 = (struct iq_header*)__va(sprd_iq_addr());

	g_iq.head_2 = (struct iq_header*)__va(sprd_iq_addr() + SPRD_IQ_SIZE
					      - sizeof(struct iq_header));

	g_iq.head_1->WR_RD_FLAG = IQ_BUF_INIT;// IQ_BUF_READ_FINISHED;
	g_iq.head_2->WR_RD_FLAG = IQ_BUF_INIT;//IQ_BUF_READ_FINISHED;

	iq_base = sprd_iq_addr();
#if 0	/* Don't use vserial */
	kernel_vser_register_callback((void*)sprd_iq_complete);
#endif
	iq_thread = kthread_create(sprd_iq_thread, (void*)&g_iq, "iq_thread");
	if (IS_ERR(iq_thread)) {
		printk(KERN_ERR "mux_ipc_thread error!.\n");
		return PTR_ERR(iq_thread);
	}

	wake_up_process(iq_thread);
	return 0;

error2:
	class_destroy(sprd_iq_class);
error1:
	misc_deregister(&iq_mem_dev);
	return ret;
}

static void __exit sprd_iq_exit(void)
{
	class_destroy(sprd_iq_class);
	misc_deregister(&iq_mem_dev);
}

module_init(sprd_iq_init);
module_exit(sprd_iq_exit);

MODULE_AUTHOR("zhongping tan");
MODULE_DESCRIPTION("SPRD IQ driver");
MODULE_LICENSE("GPL");
