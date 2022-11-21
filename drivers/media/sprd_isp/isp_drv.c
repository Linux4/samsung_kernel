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
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
//#include <mach/sci.h>
#include <soc/sprd/sci.h>
//#include <mach/hardware.h>
#ifndef CONFIG_64BIT
#include <soc/sprd/hardware.h>
#endif
#include <asm/cacheflush.h>
#include <video/sprd_isp.h>
#include "parse_hwinfo.h"
#include "compat_isp_drv.h"
#include "isp_drv.h"
#include "isp_reg.h"

#define ISP_MINOR MISC_DYNAMIC_MINOR

static struct isp_k_private *s_isp_private;

static int isp_wr_addr(struct isp_reg_bits *ptr);

static int32_t isp_block_buf_alloc(struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t buf_len = 0;

	buf_len = ISP_REG_BUF_SIZE + ISP_RAW_AE_BUF_SIZE + ISP_FRGB_GAMMA_BUF_SIZE + ISP_YUV_YGAMMA_BUF_SIZE + ISP_RAW_AWB_BUF_SIZE + ISP_YIQ_AEM_BUF_SIZE;

	isp_private->block_buf_addr = (unsigned long)vzalloc(buf_len);
	if (0 == isp_private->block_buf_addr) {
		ret = -1;
		printk("isp_block_buf_alloc: no memory error.\n");
	} else {
		isp_private->block_buf_len = buf_len;
		isp_private->reg_buf_addr = isp_private->block_buf_addr;
		isp_private->reg_buf_len = ISP_REG_BUF_SIZE;
		isp_private->raw_aem_buf_addr = isp_private->reg_buf_addr + isp_private->reg_buf_len;
		isp_private->raw_aem_buf_len = ISP_RAW_AE_BUF_SIZE;
		isp_private->full_gamma_buf_addr = isp_private->raw_aem_buf_addr + isp_private->raw_aem_buf_len;
		isp_private->full_gamma_buf_len = ISP_FRGB_GAMMA_BUF_SIZE;
		isp_private->yuv_ygamma_buf_addr = isp_private->full_gamma_buf_addr + isp_private->full_gamma_buf_len;
		isp_private->yuv_ygamma_buf_len = ISP_YUV_YGAMMA_BUF_SIZE;
		isp_private->raw_awbm_buf_addr = isp_private->yuv_ygamma_buf_addr + isp_private->yuv_ygamma_buf_len;
		isp_private->raw_awbm_buf_len = ISP_RAW_AWB_BUF_SIZE;
		isp_private->yiq_aem_buf_addr = isp_private->raw_awbm_buf_addr + isp_private->raw_awbm_buf_len;
		isp_private->yiq_aem_buf_len = ISP_YIQ_AEM_BUF_SIZE;
	}

	return ret;
}

static int32_t isp_block_buf_free(struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if ((0x00 != isp_private->block_buf_addr)
		&& (0x00 != isp_private->block_buf_len)) {
		vfree((void *)isp_private->block_buf_addr);
		isp_private->block_buf_addr = 0x00;
		isp_private->block_buf_len = 0x00;
		isp_private->reg_buf_addr = 0x00;
		isp_private->reg_buf_len = 0x00;
		isp_private->raw_aem_buf_addr = 0x00;
		isp_private->raw_aem_buf_len = 0x00;
		isp_private->full_gamma_buf_addr = 0x00;
		isp_private->full_gamma_buf_len = 0x00;
		isp_private->yuv_ygamma_buf_addr = 0x00;
		isp_private->yuv_ygamma_buf_len = 0x00;
		isp_private->raw_awbm_buf_addr = 0x00;
		isp_private->raw_awbm_buf_len = 0x00;
	}

	return ret;
}

static int32_t isp_yiq_antiflicker_buf_free(struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if ((0x00 != isp_private->yiq_antiflicker_buf_addr)
		&& (0x00 != isp_private->yiq_antiflicker_order)) {
		free_pages(isp_private->yiq_antiflicker_buf_addr, isp_private->yiq_antiflicker_order);
		isp_private->yiq_antiflicker_buf_addr = 0x00;
		isp_private->yiq_antiflicker_len = 0x00;
		isp_private->yiq_antiflicker_order = 0x00;
	}

	return ret;

}

static int32_t isp_yiq_antiflicker_buf_alloc(struct isp_k_private *isp_private, uint32_t len)
{
		int32_t ret = 0x00;
#ifndef CONFIG_64BIT
		uint32_t buf = 0x00;
		void *ptr = NULL;
#endif

		if (0x00 < len) {
			isp_private->yiq_antiflicker_len = len;
			isp_private->yiq_antiflicker_order = get_order(len);
			isp_private->yiq_antiflicker_buf_addr = (unsigned long)__get_free_pages(GFP_KERNEL | __GFP_COMP, isp_private->yiq_antiflicker_order);
			if (NULL == (void*)isp_private->yiq_antiflicker_buf_addr) {
				printk("isp_yiq_antiflicker_buf_alloc: memory error, addr:0x%lx, len:0x%x, order:0x%x.\n",
					isp_private->yiq_antiflicker_buf_addr,
					isp_private->yiq_antiflicker_len,
					isp_private->yiq_antiflicker_order);
				return -1;
			}
#ifndef CONFIG_64BIT
			ptr = (void*)isp_private->yiq_antiflicker_buf_addr;
			buf = virt_to_phys((volatile void *)isp_private->yiq_antiflicker_buf_addr);

			dmac_flush_range(ptr, ptr + len);
			outer_flush_range(__pa(ptr), __pa(ptr) + len);
#endif
		}

		return ret;

}

#if 0
static int32_t isp_b4awb_switch_buf(struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	/*first bypass b4awb*/
	REG_MWR(ISP_BINNING_PARAM, BIT_0, 0x1);

	/*switch buf*/
	if (isp_private->b4awb_buf[0].buf_flag == 1) {
		isp_private->b4awb_buf[1].buf_flag = 1;
		REG_WR(ISP_BINNING_MEM_ADDR, isp_private->b4awb_buf[1].buf_phys_addr);
		isp_private->b4awb_buf[0].buf_flag = 0;
	} else if (isp_private->b4awb_buf[1].buf_flag == 1) {
		isp_private->b4awb_buf[0].buf_flag = 1;
		REG_WR(ISP_BINNING_MEM_ADDR, isp_private->b4awb_buf[0].buf_phys_addr);
		isp_private->b4awb_buf[1].buf_flag = 0;
	}

	/*enable b4awb*/
	REG_MWR(ISP_BINNING_PARAM, BIT_0, 0x0);

	return ret;
}
#endif

static void isp_read_reg(struct isp_reg_bits *reg_bits_ptr, uint32_t counts)
{
	uint32_t i = 0;
	unsigned long reg_val = 0, reg_addr = 0;

	for (i = 0; i < counts; i++) {
		reg_addr = ISP_BASE_ADDR + reg_bits_ptr[i].reg_addr;
		if((reg_addr >= ISP_BASE_ADDR) && (reg_addr <= (ISP_BASE_ADDR + 0xFFFFFUL)))
		{
			reg_val = REG_RD(reg_addr);
			reg_bits_ptr[i].reg_value = reg_val;
		  	reg_bits_ptr[i].reg_addr = isp_phybase + reg_bits_ptr[i].reg_addr;
		} 
		else
		{
			 printk("isp_read_reg: This is not ISP reg address %d\n",reg_addr); 	   
		}
	}
}

static void isp_write_reg(struct isp_reg_bits *reg_bits_ptr, uint32_t counts)
{
	uint32_t i = 0;
	unsigned long reg_val = 0, reg_addr = 0;

	for (i = 0; i < counts; i++) {
		reg_addr = reg_bits_ptr[i].reg_addr + ISP_BASE_ADDR;
		reg_val = reg_bits_ptr[i].reg_value;
		if((reg_addr >= ISP_BASE_ADDR) && (reg_addr <= (ISP_BASE_ADDR + 0xFFFFFUL)))
		{
			REG_WR(reg_addr, reg_val);
		}
		else
		{
			printk("isp_write_reg: This is not ISP reg address %d\n",reg_addr);        
		}
	}
}

static int32_t isp_queue_init(struct isp_queue *queue)
{
	if (NULL == queue) {
		printk("isp_queue_init: queue is null error.\n");
		return -1;
	}

	memset(queue, 0x00, sizeof(*queue));
	queue->write = &queue->node[0];
	queue->read  = &queue->node[0];

	return 0;
}

static int32_t isp_queue_write(struct isp_queue *queue, struct isp_node *node)
{
	struct isp_node *ori_node = NULL;

	if (NULL == queue || NULL == node) {
		printk("isp_queue_write: queue or node is null error %p %p\n",
			queue, node);
		return -1;
	}

	ori_node = queue->write;

	*queue->write++ = *node;
	if (queue->write > &queue->node[ISP_QUEUE_LENGTH-1])
		queue->write = &queue->node[0];

	if (queue->write == queue->read)
		queue->write = ori_node;

	return 0;
}

static int32_t isp_queue_read(struct isp_queue *queue, struct isp_node *node)
{
	if (NULL == queue || NULL == node) {
		printk("isp_queue_read: queue or node is null error %p %p\n",
			queue, node);
		return -1;
	}

	if (queue->read != queue->write) {
		*node = *queue->read++;
		if (queue->read > &queue->node[ISP_QUEUE_LENGTH-1])
			queue->read = &queue->node[0];
	}

	return 0;
}

static int32_t isp_set_clk(struct isp_k_file *file, enum isp_clk_sel clk_sel)
{
	int32_t ret = 0;
	char *parent = "clk_256m";
	struct clk *clk_parent = NULL;
	struct isp_k_private *isp_private = NULL;

	if (!file) {
		printk("isp_set_clk: file is null error.\n");
		return -1;
	}

	isp_private = file->isp_private;
	if (!isp_private) {
		printk("isp_set_clk: isp_private is null error.\n");
		return -1;
	}

	switch (clk_sel) {
	case ISP_CLK_480M:
		parent = "clk_usbpll";
		break;
	case ISP_CLK_384M:
		parent = "clk_384m";
		break;
	case ISP_CLK_312M:
		parent = "clk_312m";
		break;
	case ISP_CLK_256M:
		parent = "clk_256m";
		break;
	case ISP_CLK_128M:
		parent = "clk_128m";
		break;
	case ISP_CLK_48M:
		parent = "clk_48m";
		break;
	case ISP_CLK_76M8:
		parent = "clk_76p8m";
		break;
	case ISP_CLK_NONE:
		printk("isp_set_clk: close clock %d\n", (int)clk_get_rate(isp_private->clock));
		if (isp_private->clock) {
			clk_disable(isp_private->clock);
			clk_put(isp_private->clock);
			isp_private->clock = NULL;
		}
		return 0;
	default:
		parent = "clk_128m";
		break;
	}

	if (NULL == isp_private->clock) {
		isp_private->clock = parse_clk(isp_private->dn, "clk_isp");
		if (IS_ERR(isp_private->clock)) {
			printk("isp_set_clk: parse_clk error.\n");
			return -1;
		}
	} else {
		clk_disable(isp_private->clock);
	}

	clk_parent = clk_get(NULL, parent);
	if (IS_ERR(clk_parent)) {
		printk("isp_set_clk: clk_get error %d\n", (int)clk_parent);
		return -1;
	}

	ret = clk_set_parent(isp_private->clock, clk_parent);
	if(ret){
		printk("isp_set_clk: clk_set_parent error.\n");
	}

	ret = clk_enable(isp_private->clock);
	if (ret) {
		printk("isp_set_clk: clk_enable error.\n");
		return -1;
	}

	return ret;
}

static int32_t isp_module_rst(struct isp_k_file *file)
{
	int32_t ret = 0;
	struct isp_k_private *isp_private = NULL;

	if (!file) {
		printk("isp_module_rst: file is null error.\n");
		return -1;
	}

	isp_private = file->isp_private;
	if (!isp_private) {
		printk("isp_module_rst: isp_private is null error.\n");
		return -1;
	}

	if (0x00 != atomic_read(&isp_private->users)) {
		ret = isp_axi_bus_waiting();

		isp_clr_int();

		sci_glb_set(ISP_MODULE_RESET, ISP_RST_LOG_BIT);
		sci_glb_set(ISP_MODULE_RESET, ISP_RST_CFG_BIT);
		sci_glb_set(ISP_MODULE_RESET, ISP_RST_LOG_BIT);
		sci_glb_set(ISP_MODULE_RESET, ISP_RST_CFG_BIT);
		sci_glb_set(ISP_MODULE_RESET, ISP_RST_LOG_BIT);
		sci_glb_set(ISP_MODULE_RESET, ISP_RST_CFG_BIT);

		sci_glb_clr(ISP_MODULE_RESET, ISP_RST_CFG_BIT);
		sci_glb_clr(ISP_MODULE_RESET, ISP_RST_LOG_BIT);
	}

	return ret;
}

static int32_t isp_module_eb(struct isp_k_file *file)
{
	int32_t ret = 0;
	struct isp_k_private *isp_private = NULL;

	if (!file) {
		printk("isp_module_eb: file is null error.\n");
		return -1;
	}

	isp_private = file->isp_private;
	if (!isp_private) {
		printk("isp_module_eb: isp_private is null error.\n");
		return -1;
	}

	if (0x01 == atomic_inc_return(&isp_private->users)) {

		ret = clk_mm_i_eb(isp_private->dn, 1);
		if (unlikely(0 != ret)) {
			ret = -1;
			printk("isp_module_eb: clk_mm_i_eb error.\n");
		}

	#if defined(CONFIG_MACH_SP7720EA) || defined(CONFIG_MACH_SP7720EB) || defined(CONFIG_MACH_PIKEB_J1MINI_3G) || defined(CONFIG_MACH_PIKEB_J1_3G) || defined(CONFIG_MACH_SP7720EB_PRIME) || defined(CONFIG_MACH_J1MINI3G)
		ret = isp_set_clk(file, ISP_CLK_256M);
	#elif defined(CONFIG_ARCH_SCX35LT8)
		ret = isp_set_clk(file, ISP_CLK_480M);
	#else
		ret = isp_set_clk(file, ISP_CLK_312M);
	#endif
		if (unlikely(0 != ret)) {
			ret = -1;
			printk("isp_module_eb: isp_set_clk error.\n");
		}
	}

	return ret;
}

static int32_t isp_module_dis(struct isp_k_file *file)
{
	int32_t ret = 0;
	struct isp_k_private *isp_private = NULL;

	if (!file) {
		printk("isp_module_dis: file is null error.\n");
		return -1;
	}

	isp_private = file->isp_private;
	if (!isp_private) {
		printk("isp_module_dis: isp_private is null error.\n");
		return -1;
	}

	if (0x00 == atomic_dec_return(&isp_private->users)) {

		ret = isp_set_clk(file, ISP_CLK_NONE);
		if (unlikely(0 != ret)) {
			ret = -1;
			printk("isp_module_dis: isp_set_clk error.\n");
		}

		ret = clk_mm_i_eb(isp_private->dn, 0);
		if (unlikely(0 != ret)) {
			ret = -1;
			printk("isp_module_dis: close clk_mm_i error.\n");
		}
	}

	return ret;
}

static irqreturn_t isp_isr(int irq, void *dev_id)
{
	int32_t ret = 0;
	unsigned long flag = 0;
	struct isp_node node;
	struct isp_k_file *fd = NULL;
	struct isp_drv_private *drv_private = NULL;
	struct timespec ts;

	if (!dev_id) {
		printk("isp_isr: dev_id is null error.\n");
		return IRQ_NONE;
	}

	fd = (struct isp_k_file *)dev_id;
	drv_private = (struct isp_drv_private *)&fd->drv_private;

	if (!drv_private) {
		printk("isp_isr: drv_private is null error.\n");
		return IRQ_NONE;
	}

	spin_lock_irqsave(&drv_private->isr_lock, flag);
	memset(&node, 0x00, sizeof(node));
	ret = isp_get_int_num(&node);

	/*B4AWB INT*/
	/*if (node.irq_val1 & BIT_18) {
		isp_b4awb_switch_buf(fd->isp_private);
	}*/

	ktime_get_ts(&ts);
	node.time.sec = ts.tv_sec;
	node.time.usec =  ts.tv_nsec / NSEC_PER_USEC;
	isp_queue_write((struct isp_queue *)&drv_private->queue, (struct isp_node *)&node);
	spin_unlock_irqrestore(&drv_private->isr_lock, flag);

	up(&drv_private->isr_done_lock);

	if (ret) {
		return IRQ_NONE;
	} else {
		return IRQ_HANDLED;
	}
}

static int32_t isp_register_irq(struct isp_k_file *file)
{
	int32_t ret = 0;

	ret = request_irq(ISP_IRQ, isp_isr, IRQF_SHARED, "ISP", (void *)file);

	return ret;
}

static void isp_unregister_irq(struct isp_k_file *file)
{
	free_irq (ISP_IRQ, (void *)file);
}

static int isp_open (struct inode *node, struct file *file)
{
	int ret = 0;
	struct isp_k_private *isp_private = s_isp_private;//platform_get_drvdata(rot_get_platform_device())
	struct isp_k_file *fd = NULL;

	if (!file) {
		ret = -EINVAL;
		printk("isp_open: file is null error.\n");
		return ret;
	}

	if (!isp_private) {
		ret = -EFAULT;
		printk("isp_open: isp_private is null, error.\n");
		return ret;
	}

	down(&isp_private->device_lock);

	fd = vzalloc(sizeof(*fd));
	if (!fd) {
		ret = -ENOMEM;
		up(&isp_private->device_lock);
		printk("isp_open: no memory for fd, error.\n");
		return ret;
	}

	fd->isp_private = isp_private;

	spin_lock_init(&fd->drv_private.isr_lock);
	sema_init(&fd->drv_private.isr_done_lock, 0);
	ret = isp_queue_init(&(fd->drv_private.queue));
	if (unlikely(0 != ret)) {
		ret = -EFAULT;
		vfree(fd);
		up(&isp_private->device_lock);
		printk("isp_open: isp_queue_init error.\n");
		return ret;
	}

	ret = isp_module_eb(fd);
	if (unlikely(0 != ret)) {
		ret = -EIO;
		printk("isp_open: isp_module_eb error.\n");
		goto open_exit;
	}

	ret = isp_module_rst(fd);
	if (unlikely(0 != ret)) {
		ret = -EIO;
		isp_module_dis(fd);
		printk("isp_open: isp_module_rst error.\n");
		goto open_exit;
	}

	ret = isp_register_irq(fd);
	if (unlikely(0 != ret)) {
		ret = -EIO;
		isp_module_dis(fd);
		printk("isp_open: isp_register_irq error.\n");
		goto open_exit;
	}

	file->private_data = fd;

	printk("isp_open: success.\n");

	return ret;

open_exit:
	vfree(fd);
	fd = NULL;

	file->private_data = NULL;

	up(&isp_private->device_lock);

	return ret;
}

static int isp_release (struct inode *node, struct file *file)
{
	int ret = 0;
	struct isp_k_private *isp_private = NULL;
	struct isp_k_file *fd = NULL;

	if (!file) {
		ret = -EINVAL;
		printk("isp_release: file is null error.\n");
		return ret;
	}

	fd = file->private_data;
	if (!fd) {
		ret = -EFAULT;
		printk("isp_release: fd is null error.\n");
		return ret;
	}

	isp_private = fd->isp_private;
	if (!isp_private) {
		printk("isp_release: isp_private is null error.\n");
		goto fd_free;
	}

	down(&isp_private->ioctl_lock);

	isp_unregister_irq(fd);

	ret = isp_module_dis(fd);

	up(&isp_private->ioctl_lock);

fd_free:
	vfree(fd);
	fd = NULL;

	file->private_data = NULL;

	if (isp_private)
		up(&isp_private->device_lock);

	printk("isp_release: success.\n");

	return 0;
}

static long isp_ioctl( struct file *file, unsigned int cmd, unsigned long param)
{
	long ret = 0;
	struct isp_irq irq_param;
	struct isp_node node;
	struct isp_reg_param reg_param = {0, 0};
	struct isp_reg_bits *reg_bits_ptr = NULL;
	struct isp_k_private *isp_private = NULL;
	struct isp_drv_private *drv_private = NULL;
	struct isp_k_file *fd = NULL;

	if (!file) {
		ret = -EINVAL;
		printk("isp_ioctl: file is null error.\n");
		return ret;
	}

	fd = file->private_data;
	if (!fd) {
		ret = - EFAULT;
		printk("isp_ioctl: private_data is null error.\n");
		return ret;
	}

	isp_private = fd->isp_private;
	if (!isp_private) {
		ret = -EFAULT;
		printk("isp_ioctl: isp_private is null error.\n");
		return ret;
	}

	drv_private = &fd->drv_private;

	switch (cmd) {
	case ISP_IO_IRQ:
	{
		ret = down_interruptible(&fd->drv_private.isr_done_lock);
		if (ret) {
			memset(&irq_param, 0, sizeof(irq_param));
			irq_param.ret_val = ret;
			ret = copy_to_user ((void *)param, (void *)&irq_param, sizeof(irq_param));
			if ( 0 != ret) {
				printk("isp_ioctl: irq: copy_to_user error ret = %d\n", (uint32_t)ret);
			}
			ret = -ERESTARTSYS;
			return ret;
		}

		ret = isp_queue_read(&drv_private->queue, &node);
		if (0 != ret) {
			ret = -EFAULT;
			printk("isp_ioctl: isp_queue_read error, ret = 0x%x\n", (uint32_t)ret);
			return ret;
		}

		memset(&irq_param, 0, sizeof(irq_param));
		irq_param.irq_val0 = node.irq_val0;
		irq_param.irq_val1 = node.irq_val1;
		irq_param.irq_val2 = node.irq_val2;
		irq_param.irq_val3 = node.irq_val3;
		irq_param.reserved = node.reserved;
		irq_param.time = node.time;
		ret = copy_to_user ((void *)param, (void *)&irq_param, sizeof(irq_param));
		if (0 != ret) {
			ret = -EFAULT;
			printk("isp_k: ioctl irq: copy_to_user error, ret = 0x%x", (uint32_t)ret);
		}
		break;
	}

	case ISP_IO_READ:
	{
		uint32_t buf_size = 0;

		down(&isp_private->ioctl_lock);

		ret = copy_from_user((void *)&reg_param, (void *)param, sizeof(struct isp_reg_param));
		if ( 0 != ret) {
			ret = -EFAULT;
			printk("isp_ioctl: read copy_from_user 0 error, ret = 0x%x\n", (uint32_t)ret);
			goto io_read_exit;
		}

		buf_size = reg_param.counts * sizeof(struct isp_reg_bits);
		if (buf_size > isp_private->reg_buf_len) {
			ret = -EFAULT;
			printk("isp_ioctl: read buf len error.\n");
			goto io_read_exit;
		}

		reg_bits_ptr = (struct isp_reg_bits *)isp_private->reg_buf_addr;
		ret = copy_from_user((void *)reg_bits_ptr, (void *)reg_param.reg_param, buf_size);
		if ( 0 != ret) {
			ret = -EFAULT;
			printk("isp_ioctl: read copy_from_user 1 error, ret = 0x%x\n", (uint32_t)ret);
			goto io_read_exit;
		}

		isp_read_reg(reg_bits_ptr, reg_param.counts);

		ret = copy_to_user((void *)reg_param.reg_param, (void *)reg_bits_ptr, buf_size);
		if ( 0 != ret) {
			ret = -EFAULT;
			printk("isp_ioctl: read copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
			goto io_read_exit;
		}

		io_read_exit:
		if (reg_bits_ptr) {
			memset((void *)isp_private->reg_buf_addr, 0x00, buf_size);
			reg_bits_ptr = NULL;
		}

		up(&isp_private->ioctl_lock);

		break;
	}

	case ISP_IO_WRITE:
	{
		uint32_t buf_size = 0;

		down(&isp_private->ioctl_lock);

		ret = copy_from_user((void *)&reg_param, (void *)param, sizeof(struct isp_reg_param));
		if ( 0 != ret) {
			printk("isp_ioctl: write copy_from_user 0 error, ret = 0x%x\n", (uint32_t)ret);
			ret = -EFAULT;
			goto io_write_exit;
		}

		buf_size = reg_param.counts * sizeof(struct isp_reg_bits);
		if (buf_size > isp_private->reg_buf_len) {
			ret = -EFAULT;
			printk("isp_ioctl: write buf len error.\n");
			goto io_write_exit;
		}

		reg_bits_ptr = (struct isp_reg_bits *)isp_private->reg_buf_addr;
		ret = copy_from_user((void *)reg_bits_ptr, (void *)reg_param.reg_param, buf_size);
		if ( 0 != ret) {
			ret = -EFAULT;
			printk("isp_ioctl: write copy_from_user 1 error, ret = 0x%x\n", (uint32_t)ret);
			goto io_write_exit;
		}

		isp_write_reg(reg_bits_ptr, reg_param.counts);

		io_write_exit:
		if (reg_bits_ptr) {
			memset((void *)isp_private->reg_buf_addr, 0x00, buf_size);
			reg_bits_ptr = NULL;
		}

		up(&isp_private->ioctl_lock);

		break;
	}

	case ISP_IO_RST:
	{
		down(&isp_private->ioctl_lock);

		ret = isp_module_rst(fd);
		if (ret) {
			ret = -EFAULT;
			printk("isp_ioctl: restet error.\n");
		}

		isp_private->is_vst_ivst_update = 1;
		isp_private->full_gamma_buf_id = 0;
		isp_private->yuv_ygamma_buf_id = 0;

		up(&isp_private->ioctl_lock);

		break;
	}

	case ISP_IO_STOP:
	{
		unsigned long flag = 0;
		struct isp_node node;

		down(&isp_private->ioctl_lock);

		isp_en_irq(ISP_INT_CLEAR_MODE);

		spin_lock_irqsave(&drv_private->isr_lock,flag);
		memset(&node, 0x00, sizeof(node));
		node.reserved = ISP_INT_EVT_STOP;
		isp_queue_write((struct isp_queue *)&drv_private->queue, (struct isp_node *)&node);
		spin_unlock_irqrestore(&drv_private->isr_lock, flag);

		up(&fd->drv_private.isr_done_lock);

		up(&isp_private->ioctl_lock);

		break;
	}

	case ISP_IO_INT:
	{
		struct isp_interrupt int_param;

		down(&isp_private->ioctl_lock);

		ret = copy_from_user((void *)&int_param, (void *)param, sizeof(int_param));
		if (ret) {
			ret = -EFAULT;
			printk("isp_ioctl: int copy_from_user error, ret = %d\n", (uint32_t)ret);
		}

		if (0 == ret)
			isp_en_irq(int_param.int_mode);

		up(&isp_private->ioctl_lock);

		break;
	}

	case ISP_IO_CFG_PARAM:
		down(&isp_private->ioctl_lock);
		ret = isp_cfg_param((void *)param, isp_private);
		up(&isp_private->ioctl_lock);
		break;

	case ISP_IO_CAPABILITY:
		ret = isp_capability((void *)param);
		break;

	case ISP_REG_READ:
	{
		int ret;
		int num;
		int ISP_REG_NUM = 20467;

		struct isp_reg_bits *ptr = (struct isp_reg_bits *)vmalloc(ISP_REG_NUM * sizeof(struct isp_reg_bits));

		if (NULL == ptr) {
			printk("isp_ioctl:REG_READ: kmalloc error\n");
			return -ENOMEM;
		}
		memset(ptr, 0, ISP_REG_NUM * sizeof(struct isp_reg_bits));

		num = isp_wr_addr(ptr);

		isp_read_reg(ptr, num);

		ret = copy_to_user((void *)param, (void *)ptr, ISP_REG_NUM * sizeof(struct isp_reg_bits));
		if ( 0 != ret) {
			printk("isp_ioctl: REG_READ: copy_to_user error ret = %d\n", (uint32_t)ret);
			vfree(ptr);
			return ret;
			}
		vfree(ptr);
		break;
	}

	default:
		printk("isp_ioctl: cmd is unsupported, cmd = %x\n", (int32_t)cmd);
		return -EFAULT;
	}

	return ret;
}

static struct file_operations isp_fops = {
	.owner = THIS_MODULE,
	.open = isp_open,
	.unlocked_ioctl = isp_ioctl,
	.compat_ioctl = compat_isp_ioctl,
	.release = isp_release,
};

static struct miscdevice isp_dev = {
	.minor = ISP_MINOR,
	.name = "sprd_isp",
	.fops = &isp_fops,
};

static int isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct isp_k_private *isp_private = NULL;

	isp_private = devm_kzalloc(&pdev->dev, sizeof(*isp_private), GFP_KERNEL);
	if (!isp_private) {
		printk("isp_probe: isp_private is null error.\n");
		return -ENOMEM;
	}

	atomic_set(&isp_private->users, 0);
	isp_private->dn = pdev->dev.of_node;
	isp_private->clock = NULL;
	sema_init(&isp_private->device_lock, 1);
	sema_init(&isp_private->ioctl_lock, 1);
	ret = isp_block_buf_alloc(isp_private);
	if (ret) {
		ret = -ENOMEM;
		devm_kfree(&pdev->dev, isp_private);
		printk("isp_probe: no memory for isp_private, error.\n");
		return ret;
	}

	platform_set_drvdata(pdev, isp_private);

	s_isp_private = isp_private;

	ret = isp_yiq_antiflicker_buf_alloc(isp_private, ISP_YIQ_ANTIFLICKER_SIZE);
	if (ret) {
		ret = -ENOMEM;
		isp_block_buf_free(isp_private);
		devm_kfree(&pdev->dev, isp_private);
		platform_set_drvdata(pdev, NULL);
		printk("isp_probe: no memory for isp lsc buf alloc, error.\n");
		return ret;
	}

	ret = misc_register(&isp_dev);
	if (ret) {
		ret = -EACCES;
		isp_yiq_antiflicker_buf_free(isp_private);
		isp_block_buf_free(isp_private);
		devm_kfree(&pdev->dev, isp_private);
		platform_set_drvdata(pdev, NULL);
		printk("isp_probe: misc_register error.\n");
	}

	parse_baseaddress(pdev->dev.of_node);
	printk("isp_probe: success.\n");

	return ret;
}

static int isp_remove(struct platform_device *dev)
{
	struct isp_k_private *isp_private = NULL;

	isp_private = platform_get_drvdata(dev);

	if (isp_private) {
		isp_yiq_antiflicker_buf_free(isp_private);
		isp_block_buf_free(isp_private);
		devm_kfree(&dev->dev, isp_private);
		platform_set_drvdata(dev, NULL);
	} else {
		printk("isp_remove: isp_private is null error.\n");
	}

	misc_deregister(&isp_dev);

	printk("isp_remove: success.\n");

	return 0;
}

static const struct of_device_id of_match_table_isp[] = {
	{ .compatible = "sprd,sprd_isp", },
	{ },
};

static struct platform_driver isp_driver = {
	.probe = isp_probe,
	.remove = isp_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_isp",
		.of_match_table = of_match_ptr(of_match_table_isp),
	},
};

static int __init isp_init(void)
{
	printk ("isp_init: success.\n");

	if (platform_driver_register(&isp_driver)) {
		printk ("isp_init: platform_driver_register error.\n");
		return -1;
	}

	return 0;
}

static void __exit isp_exit(void)
{
	printk ("isp_exit: success.\n");

	platform_driver_unregister(&isp_driver);
}

static int isp_wr_addr(struct isp_reg_bits *ptr)
{
		int i;
		int sum = 0;
		struct isp_reg_bits *ptr_reg = ptr;

		if(NULL == ptr){
			printk("isp_wr_addr: ptr\n");
			return -1;
		}


		ptr_reg -> reg_addr = 0x0000; /* Interrupt part*/
		sum++;

		for (i = 0; i < 17; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0014 + i * 0x4;
			sum++;
		}

		for (i = 0; i < 4; i++)    /*Fetch*/
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0100 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0114 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 10; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0120 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 4; i++)  /* store*/
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0200 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 10; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0214 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 12; i++) /* dispatch */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0300 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* arbiter */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0400 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 4; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0414 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* axi */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0500 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0514 + i * 0x4;
			sum++;
		}

		ptr_reg++;	/* raw sizer */
		ptr_reg -> reg_addr = 0x0600;
		sum++;

		for(i = 0; i < 45; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x0614 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 3; i++)  /* common */
		{
			ptr_reg++;
            ptr_reg -> reg_addr = 0x0700 + i * 0x4;
			sum++;
        }

		for(i = 0; i < 16; i++)
        {
            ptr_reg++;
            ptr_reg -> reg_addr = 0x0714 + i * 0x4;
			sum++;
        }

		for(i = 0; i < 2; i++) /* global gain */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1000 + i * 0x14;
			sum++;
		}

		ptr_reg++;  /* BLC */
		ptr_reg -> reg_addr = 0x1100;
		sum++;

		for(i = 0; i < 3; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1114 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* RGBG */
		ptr_reg -> reg_addr = 0x1200;
		sum++;

		for(i = 0; i < 3; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1214 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* PWD */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1300 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 11; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1314 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* NLC */
		ptr_reg -> reg_addr = 0x1400;
		sum++;

		for(i = 0; (ptr_reg->reg_addr) < 0x14b0; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1414 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* lens shading calibration */
		ptr_reg -> reg_addr = 0x1500;
		sum++;

		for(i = 0; i < 15; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1514 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* binning */
		ptr_reg -> reg_addr = 0x1600;
		sum++;

		for(i = 0; i < 3; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1614 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* AWBM */
		ptr_reg -> reg_addr = 0x1700;
		sum++;

		for(i = 0; i < 34; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1714 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x17A0 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* AWBC */
		ptr_reg -> reg_addr = 0x1800;
		sum++;

		for(i = 0; i < 14; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1814 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* AEM */
		ptr_reg -> reg_addr = 0x1900;
		sum++;

		for(i = 0; i < 4; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1914 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* BPC */
		ptr_reg -> reg_addr = 0x1A00;
		sum++;

		for(i = 0; i < 14; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1A14 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* GRGB */
		ptr_reg -> reg_addr = 0x1B00;
		sum++;

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1B14 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* BDN */
		ptr_reg -> reg_addr = 0x1C00;
		sum++;

		for(i = 0; i < 101; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1C14 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 7; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1DA8 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* RGBG2 */
		ptr_reg -> reg_addr = 0x1E00;
		sum++;

		for(i = 0; i < 3; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1E14 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* 1d lnc */
		ptr_reg -> reg_addr = 0x1F00;
		sum++;

		for(i = 0; i < 19; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1F14 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* NLM VST IVST */
		ptr_reg -> reg_addr = 0x2014;
		sum++;

		ptr_reg++;
		ptr_reg -> reg_addr = 0x2214;
		sum++;

		for(i = 0; i < 3; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x2100 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 35; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x2114 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* CFA: clolor filter array */
		ptr_reg -> reg_addr = 0x3000;
		sum++;

		for(i = 0; i < 7; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3014 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* CMC */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3100 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 5; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3114 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 5; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x312C + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* GAMMA */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3300 + i * 0x14;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* Color matrix correction for 8 bits */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3500 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 16; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3514 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* CT: Color transformation */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3600 + i * 0x4;
			sum++;
		}

		ptr_reg++;
		ptr_reg -> reg_addr = 0x3614;
		sum++;

		for(i = 0; i < 2; i++)  /* CCE: clolor conversion enhancement */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3700 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 13; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3714 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* HSV */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3800 + i * 0x14;
			sum++;
		}

		ptr_reg++; /* Radial CSC */
		ptr_reg -> reg_addr = 0x3900;
		sum++;

		for(i = 0; i < 14; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3914 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* ISP_PRECNRNEW */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3A00 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 4; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3A14 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* ISP_PSTRZ */
		ptr_reg -> reg_addr = 0x3B00;
		sum++;

		for(i = 0; i < 9; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3B14 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* AFM: auto focus monitor */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3C00 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 105; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x3C14 + i * 0x4;
			sum++;
		}

		ptr_reg++;  /* YIQ AEM */
		ptr_reg -> reg_addr = 0x4100;
		sum++;

		for(i = 0; i < 10; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x4114 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* ANTI FLICKER */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x4200 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 4; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x4214 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* YIQ AFM */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x4300 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 158; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x4314 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* Pre-CDN */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5000 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 19; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5014 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* Pre-Filter */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5100 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5114 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* Brightness */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5300 + i * 0x14;
			sum++;
		}

		for(i = 0; i < 2; i++)  /* Contrast */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5400 + i * 0x14;
			sum++;
		}

		ptr_reg++; /* HIST: histogram */
		ptr_reg -> reg_addr = 0x5500;
		sum++;

		for(i = 0; i < 5; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5514 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* HIST2 */
		ptr_reg -> reg_addr = 0x5600;
		sum++;

		for(i = 0; i < 9; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5614 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* AUTOCONT: auto contrat adjustment */
		ptr_reg -> reg_addr = 0x5700;
		sum++;

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5714 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* cdn */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5800 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 18; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5814 + i * 0x4;
			sum++;
		}

		ptr_reg++; /* edge */
		ptr_reg -> reg_addr = 0x5900;
		sum++;

		for(i = 0; i < 12; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5914 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* emboss */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5A00 + i * 0x14;
			sum++;
		}

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5A00 + i * 0x14;
			sum++;
		}

		ptr_reg++; /* CSS */
		ptr_reg -> reg_addr = 0x5B00;
		sum++;

		for(i = 0; i < 10; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5B14 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++) /* csa */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5C00 + i * 0x14;
			sum++;
		}

		for(i = 0; i < 2; i++) /* hua */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5D00 + i * 0x14;
			sum++;
		}

		ptr_reg++; /* post-cdn */
		ptr_reg -> reg_addr = 0x5E00;
		sum++;

		for(i = 0; i < 29; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x5E14 + i * 0x4;
			sum++;
		}

		ptr_reg++;
		ptr_reg -> reg_addr = 0x5F14;
		sum++;

		for(i = 0; i < 2; i++) /* ygamma */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x6000 + i * 0x14;
			sum++;
		}

		for(i = 0; i < 2; i++) /* ydelay */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x6100 + i * 0x14;
			sum++;
		}

		for(i = 0; i < 2; i++) /* iircnr */
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x6400 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 16; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x6414 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 2; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x2000 + i * 0x54;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x153fc; i++) // isp v memory1: awbm; isp v memory2:aem
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x10000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x163fc; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x16000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x17b60; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x17000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x185a0; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x18000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x1b200; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x19000 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 3; i++)
		{
			int addr_n;
			int addr_base = 0x1c000 + i * 0x1000;
			for(addr_n = 0; (ptr_reg->reg_addr) < (addr_base + 0x200); addr_n++)
			{
				ptr_reg++;
				ptr_reg -> reg_addr = addr_base + addr_n * 0x4;
				sum++;
			}
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x20b60; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x1f000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x215a0; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x21000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x24200; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x22000 + i * 0x4;
			sum++;
		}

		for(i = 0; i < 3; i++)
		{
			int addr_n;
			int addr_base = 0x25000 + i * 0x1000;
			for(addr_n = 0; (ptr_reg->reg_addr) < (addr_base + 0x200); addr_n++)
			{
				ptr_reg++;
				ptr_reg -> reg_addr = addr_base + addr_n * 0x4;
				sum++;
			}
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x2b2fc; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x28000 + i * 0x4;
			sum++;
		}

		for(i = 0; (ptr_reg->reg_addr) < 0x2e2fc; i++)
		{
			ptr_reg++;
			ptr_reg -> reg_addr = 0x2C000 + i * 0x4;
			sum++;
		}

		return sum;
}

module_init(isp_init);
module_exit(isp_exit);
MODULE_DESCRIPTION("Isp Driver");
MODULE_LICENSE("GPL");
