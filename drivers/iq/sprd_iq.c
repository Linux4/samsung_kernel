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
#include <mach/board.h>
#include <asm/memory.h>
#include <linux/memblock.h>
#include <linux/devfreq.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <mach/hardware.h>


#define IQ_BUF_INIT                0x5A5A5A5A
#define IQ_BUF_WRITE_FINISHED      0x5A5A8181
#define IQ_BUF_READ_FINISHED       0x81815A5A
#define IQ_BUF_READING             0x81005a00

#define IQ_TRANSFER_SIZE (500*1024)

struct iq_header{
	volatile u32  WR_RD_FLAG;
	u32  data_addr;
	u32  data_len;
	u32  reserved;
};

struct sprd_iq {
	struct iq_header *head_1;
	struct iq_header *head_2;
	wait_queue_head_t wait;
};

static struct sprd_iq g_iq = {0};

static struct task_struct *iq_thread;

static uint iq_base;

static u32 iq_mode = 0;

module_param_named(iq_base, iq_base, uint, 0444);

extern ssize_t vser_iq_write(char *buf, size_t count);
extern void kernel_vser_register_callback(void *function);
extern phys_addr_t sprd_iq_addr(void);

static int __init early_mode(char *str)
{
	printk("early_mode \n");
	if(!memcmp(str, "iq", 2))
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
	while(length - send_num > 0) {
		vaddr = NULL;

		len =( length -send_num > IQ_TRANSFER_SIZE) ? IQ_TRANSFER_SIZE: (length -send_num);
		vaddr = __va(paddr+send_num);
		if(!vaddr) {
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
		if(ret < 0) {
			printk(KERN_ERR "sprd iq usb send error %d \n", ret);
			msleep(200);
		}
		else
		{
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

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {

		//printk(KERN_ERR "sprd_iq_thread flag1 = 0x%x, flag2 = 0x%x \n", p_iq->head_1->WR_RD_FLAG, p_iq->head_2->WR_RD_FLAG);
		//wait_event(p_iq->wait, p_iq->bstart);
		if(p_iq->head_1->WR_RD_FLAG == IQ_BUF_WRITE_FINISHED) {
			p_iq->head_1->WR_RD_FLAG = IQ_BUF_READING;
			sprd_iq_write(p_iq->head_1->data_addr, p_iq->head_1->data_len);
			//p_iq->head_1->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
		} else if(p_iq->head_2->WR_RD_FLAG == IQ_BUF_WRITE_FINISHED) {
			p_iq->head_2->WR_RD_FLAG = IQ_BUF_READING;
			sprd_iq_write(p_iq->head_2->data_addr, p_iq->head_2->data_len);
			//p_iq->head_2->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
		} else
			msleep(1000);
	}
	return 0;
}

static void sprd_iq_complete(char *buf,  int length)
{
	u32 vaddr = (u32)buf;
	pr_debug("sprd_iq_complete 0x%x, 0x%x \n", vaddr, length);
	if(vaddr + length == (u32)__va(g_iq.head_1->data_addr + g_iq.head_1->data_len))
		g_iq.head_1->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
	if(vaddr + length == (u32)__va(g_iq.head_2->data_addr + g_iq.head_2->data_len))
		g_iq.head_2->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
}

#ifdef CONFIG_SPRD_SCXX30_DMC_FREQ
static unsigned int sprd_iq_dmc_notify(struct devfreq_dbs *h, unsigned int state)
{
	if(state == DEVFREQ_PRE_CHANGE)
		return 1;
}


struct devfreq_dbs dmc_iq_notify = {
	.level = 0,
	.devfreq_notifier = sprd_iq_dmc_notify,
};
#endif

static long iq_mem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static int iq_mem_nocache_mmap(struct file *filp, struct vm_area_struct *vma)
{
    size_t size = vma->vm_end - vma->vm_start;

    printk(KERN_INFO "iq_mem_nocache_mmap\n");

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff,
                        vma->vm_end - vma->vm_start, vma->vm_page_prot))
    {
        printk(KERN_ERR "remap_pfn_range failed\n");
        return -EAGAIN;
    }

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


static int __init  sprd_iq_init(void)
{
    int ret;
    ret = misc_register(&iq_mem_dev);
    if (ret) {
        printk(KERN_ERR "cannot register iq_mmap_dev ret = (%d)\n",ret);
    }

	if(!in_iqmode())
		return -EINVAL;
	if (sprd_iq_addr() == 0xffffffff)
		return -ENOMEM;
#ifdef CONFIG_SPRD_SCXX30_DMC_FREQ
	devfreq_notifier_register(&dmc_iq_notify);
#endif
	init_waitqueue_head(&g_iq.wait);
	g_iq.head_1 = (struct iq_header*)__va(sprd_iq_addr());

	g_iq.head_2 = (struct iq_header*)__va(sprd_iq_addr()+SPRD_IQ_SIZE - sizeof(struct iq_header));

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
}

static void __exit sprd_iq_exit(void)
{
    misc_deregister(&iq_mem_dev);
}




module_init(sprd_iq_init);
module_exit(sprd_iq_exit);

MODULE_AUTHOR("zhongping tan");
MODULE_DESCRIPTION("SPRD IQ driver");
MODULE_LICENSE("GPL");
