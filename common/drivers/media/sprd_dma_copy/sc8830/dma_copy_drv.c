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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <linux/file.h>
#include <mach/dma.h>
#include <linux/sched.h>
#include <video/sprd_dma_copy_k.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "dma_copy_drv.h"

#define DMA_COPY_MINOR MISC_DYNAMIC_MINOR
#define DMA_COPY_USER_MAX 4
#define INVALID_USER_ID PID_MAX_DEFAULT

struct dm_copy_user {
	pid_t pid;
};

struct dm_copy_ctx {
	struct semaphore sem_dev_open;
	struct semaphore sem_copy;
	struct semaphore sem_copy_callback;
};

static struct dm_copy_user *g_dma_copy_user = NULL;
static struct dm_copy_ctx g_dma_copy_ctx;
static struct dm_copy_ctx *g_dma_copy_ctx_ptr = &g_dma_copy_ctx;

static struct dm_copy_user *dma_copy_get_user(pid_t user_pid)
{
	struct dm_copy_user *ret_user = NULL;
	int i;

	for (i = 0; i < DMA_COPY_USER_MAX; i ++) {
		if ((g_dma_copy_user + i)->pid == user_pid) {
			ret_user = g_dma_copy_user + i;
			break;
		}
	}

	if (ret_user == NULL) {
		for (i = 0; i < DMA_COPY_USER_MAX; i ++) {
			if ((g_dma_copy_user + i)->pid == INVALID_USER_ID) {
				ret_user = g_dma_copy_user + i;
				ret_user->pid = user_pid;
				break;
			}
		}
	}

	return ret_user;
}

int dma_copy_k_open(struct inode *node, struct file *file)
{
	struct dm_copy_user *p_user = NULL;
	int ret = 0;

	DMA_COPY_PRINT("dma_copy_k_open start");
	down(&(g_dma_copy_ctx_ptr->sem_dev_open));
	p_user = dma_copy_get_user(current->pid);
	if (NULL == p_user) {
		printk("dma_copy_k_open user cnt full  pid:%d. \n",current->pid);
		up(&(g_dma_copy_ctx_ptr->sem_dev_open));
		return -1;
	}
	file->private_data = p_user;
	up(&(g_dma_copy_ctx_ptr->sem_dev_open));
	DMA_COPY_PRINT("dma_copy_k_open end");

	return ret;
}

int dma_copy_k_release(struct inode *node, struct file *file)
{
	DMA_COPY_PRINT("dma_copy_k_release");

	((struct dm_copy_user *)(file->private_data))->pid = INVALID_USER_ID;

	return 0;
}

static void dma_copy_k_trim_callback(int chn, void *data)
{
	up(&(g_dma_copy_ctx_ptr->sem_copy_callback));
}

static int dma_copy_k_trim(uint32_t dst_addr, uint32_t src_addr,
			       uint32_t trim_w, uint32_t offset, uint32_t trim_size)
{
	int ret = 0, chn_id = 0;
	uint32_t data_width = 0, src_step = 0;
	struct sci_dma_cfg dma_cfg;

	chn_id = sci_dma_request("dma_copy_k_trim", FULL_DMA_CHN);
	if (chn_id < 0) {
		printk("dma_copy_k_trim: sci_dma_request fail.");
		return -1;
	}

	memset(&dma_cfg, 0x0, sizeof(dma_cfg));

	if ((trim_size & 0x3) == 0) {
		data_width = WORD_WIDTH;
		src_step = 4;
	} else {
		if ((trim_size & 0x1) == 0) {
			data_width = SHORT_WIDTH;
			src_step = 2;
		} else {
			data_width = BYTE_WIDTH;
			src_step = 1;
		}
	}

	dma_cfg.datawidth = data_width;
	dma_cfg.src_addr = src_addr;
	dma_cfg.des_addr = dst_addr;
	dma_cfg.src_step = src_step;
	dma_cfg.des_step = src_step;
	dma_cfg.fragmens_len = trim_w;
	dma_cfg.block_len = trim_w;
	dma_cfg.transcation_len = trim_size;
	dma_cfg.src_frag_step = offset;
	dma_cfg.dst_frag_step = 0;
	dma_cfg.req_mode = TRANS_REQ_MODE;

	ret = sci_dma_config(chn_id, &dma_cfg, 1, NULL);
	if (ret < 0) {
		printk("dma_copy_k_trim: sci_dma_config fail.");
		sci_dma_free(chn_id);
		return -1;
	}

	ret = sci_dma_register_irqhandle(chn_id, TRANS_DONE,
					dma_copy_k_trim_callback, NULL);
	if (ret < 0) {
		printk("dma_copy_k_trim: sci_dma_register_irqhandle fail.");
		sci_dma_free(chn_id);
		return -1;
	}

	sci_dma_start(chn_id, DMA_UID_SOFTWARE);

	down(&(g_dma_copy_ctx_ptr->sem_copy_callback));

	sci_dma_free(chn_id);

	return 0;
}

static int dma_copy_k_yuv_trim(DMA_COPY_CFG_T * param_ptr)
{
	int32_t ret = 0;
	uint32_t src_addr = 0, dst_addr = 0, len =0, trim_w = 0, offset = 0;
	uint32_t src_offset_rec_pixel = 0, src_rec_pixel = 0;
	if (DMA_COPY_YUV420 == param_ptr->format) {
		src_offset_rec_pixel = param_ptr->src_rec.y * param_ptr->src_size.w;
		src_rec_pixel = param_ptr->src_rec.w * param_ptr->src_rec.h;

		/*dma copy trim Y*/
		dst_addr = param_ptr->dst_addr.y_addr;
		src_addr = param_ptr->src_addr.y_addr +
				src_offset_rec_pixel + param_ptr->src_rec.x;
		trim_w = param_ptr->src_rec.w;
		offset = param_ptr->src_size.w - param_ptr->src_rec.w;
		len = src_rec_pixel;
		ret = dma_copy_k_trim(dst_addr, src_addr, trim_w, offset, len);
		if (ret) {
			printk("dma_copy_k_start: trim y fail. \n");
			return -1;
		}
		/*dma copy trim UV*/
		dst_addr = param_ptr->dst_addr.uv_addr;
		src_addr = param_ptr->src_addr.uv_addr +
				((src_offset_rec_pixel >> 1) + param_ptr->src_rec.x);
		trim_w = param_ptr->src_rec.w;
		offset = param_ptr->src_size.w - param_ptr->src_rec.w;
		len = src_rec_pixel >> 1;
		ret = dma_copy_k_trim(dst_addr, src_addr, trim_w, offset, len);
		if (ret) {
			printk("dma_copy_k_start: trim uv fail. \n");
			return -1;
		}
	} else {
		ret = -1;
	}
	return ret;
}

static int dma_copy_k_yuv(DMA_COPY_CFG_T * param_ptr)
{
	int32_t ret = 0;
	uint32_t src_addr = 0, dst_addr = 0, len =0;

	/*dma copy Y*/
	src_addr = param_ptr->src_addr.y_addr;
	dst_addr = param_ptr->dst_addr.y_addr;
	len = param_ptr->src_size.w * param_ptr->src_size.h;
	ret = sci_dma_memcpy(dst_addr, src_addr, len);
	if (ret) {
		printk("dma_copy_k_yuv: y fail. \n");
		return -1;
	}
	/*dma copy UV*/
	src_addr = param_ptr->src_addr.uv_addr;
	dst_addr = param_ptr->dst_addr.uv_addr;
	if (DMA_COPY_YUV420 == param_ptr->format) {
		len = (param_ptr->src_size.w * param_ptr->src_size.h) >> 1;
	} else {
		len = param_ptr->src_size.w * param_ptr->src_size.h;/*YUV422*/
	}
	ret = sci_dma_memcpy(dst_addr, src_addr, len);
	if (ret) {
		printk("dma_copy_k_yuv: uv fail. \n");
		return -1;
	}
	return 0;
}

static int dma_copy_k_start(DMA_COPY_CFG_T * param_ptr)
{
	int32_t ret = 0;

	DMA_COPY_PRINT("dma_copy_k_start format:%d, src_size:%d %d, src_rect:%d %d %d %d,src_addr:%x %x, dst_addr=%x %x\n",
		param_ptr->format, param_ptr->src_size.w, param_ptr->src_size.h,
		param_ptr->src_rec.x, param_ptr->src_rec.y, param_ptr->src_rec.w, param_ptr->src_rec.h,
		param_ptr->src_addr.y_addr, param_ptr->src_addr.uv_addr,
		param_ptr->dst_addr.y_addr, param_ptr->dst_addr.uv_addr);

	if (DMA_COPY_YUV400 <= param_ptr->format ||
		(param_ptr->src_rec.x & 0x01) || (param_ptr->src_rec.y & 0x01) ||
		(param_ptr->src_rec.w & 0x01) || (param_ptr->src_rec.h & 0x01) ||
		(param_ptr->src_size.w & 0x01) || (param_ptr->src_size.h & 0x01) ||
		0 == param_ptr->src_addr.y_addr || 0 == param_ptr->src_addr.uv_addr ||
		0 == param_ptr->dst_addr.y_addr || 0 == param_ptr->dst_addr.uv_addr ||
		0 == param_ptr->src_rec.w || 0 == param_ptr->src_rec.h ||
		0 == param_ptr->src_size.w|| 0 == param_ptr->src_size.h ||
		(param_ptr->src_rec.x + param_ptr->src_rec.w > param_ptr->src_size.w) ||
		(param_ptr->src_rec.y + param_ptr->src_rec.h > param_ptr->src_size.h)) {
		printk("dma_copy_k_start: param is error. \n");
		return -1;
	}

	if ((0 == param_ptr->src_rec.x) && (0 == param_ptr->src_rec.y)
		&& (param_ptr->src_rec.w == param_ptr->src_size.w)
		&&(param_ptr->src_rec.h == param_ptr->src_size.h)) {
		ret = dma_copy_k_yuv(param_ptr);
	} else {
		ret = dma_copy_k_yuv_trim(param_ptr);
	}

	return ret;
}

static long dma_copy_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	down(&(g_dma_copy_ctx_ptr->sem_copy));
	{
		int ret = 0;
		DMA_COPY_CFG_T params;

		ret = copy_from_user(&params, (DMA_COPY_CFG_T *) arg, sizeof(DMA_COPY_CFG_T));
		if (0 == ret) {
			if (dma_copy_k_start(&params)) {
				printk("dma_copy_k_ioctl fail. \n");
				ret = -EFAULT;
			}
		}
		up(&(g_dma_copy_ctx_ptr->sem_copy));
		return ret;
	}
}

static struct file_operations dma_copy_fops = {
	.owner = THIS_MODULE,
	.open = dma_copy_k_open,
	.unlocked_ioctl = dma_copy_k_ioctl,
	.release = dma_copy_k_release,
};

static struct miscdevice dma_copy_dev = {
	.minor = DMA_COPY_MINOR,
	.name = "sprd_dma_copy",
	.fops = &dma_copy_fops,
};

int dma_copy_k_probe(struct platform_device *pdev)
{
	int ret, i;
	struct dm_copy_user *p_user;
	printk(KERN_ALERT "dma_copy_k_probe called\n");

	ret = misc_register(&dma_copy_dev);
	if (ret) {
		printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			DMA_COPY_MINOR, ret);
		return ret;
	}

	g_dma_copy_user = kzalloc(DMA_COPY_USER_MAX * sizeof(struct dm_copy_user), GFP_KERNEL);
	if (NULL == g_dma_copy_user) {
		printk("dm_copy_user, no mem");
		return -1;
	}

	p_user = g_dma_copy_user;
	for (i = 0; i < DMA_COPY_USER_MAX; i++) {
		p_user->pid = INVALID_USER_ID;
		p_user ++;
	}

	printk(KERN_ALERT " dma_copy_k_probe Success\n");
	return 0;
}

static int dma_copy_k_remove(struct platform_device *dev)
{
	printk(KERN_INFO "dma_copy_k_remove called !\n");
	if (g_dma_copy_user) {
		kfree(g_dma_copy_user);
	}
	misc_deregister(&dma_copy_dev);
	printk(KERN_INFO "dma_copy_k_remove Success !\n");
	return 0;
}

const struct of_device_id of_match_dma_copy[] = {
	{ .compatible = "sprd,sprd_dma_copy", },
	{ },
};

static struct platform_driver dma_copy_driver = {
	.probe = dma_copy_k_probe,
	.remove = dma_copy_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_dma_copy",
		.of_match_table = of_match_ptr(of_match_dma_copy),
		},
};

int __init dma_copy_k_init(void)
{
	printk(KERN_INFO "dma_copy_k_init called !\n");
	if (platform_driver_register(&dma_copy_driver) != 0) {
		printk("platform device register Failed \n");
		return -1;
	}
	sema_init(&(g_dma_copy_ctx_ptr->sem_dev_open), 1);
	sema_init(&(g_dma_copy_ctx_ptr->sem_copy), 1);
	sema_init(&(g_dma_copy_ctx_ptr->sem_copy_callback), 0);
	return 0;
}

void dma_copy_k_exit(void)
{
	printk(KERN_INFO "dma_copy_k_exit called !\n");
	platform_driver_unregister(&dma_copy_driver);
}

module_init(dma_copy_k_init);
module_exit(dma_copy_k_exit);

MODULE_DESCRIPTION("DMA Copy Driver");
MODULE_LICENSE("GPL");
