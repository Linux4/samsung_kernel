/*
 *  linux/drivers/media/video/cmmb/if228.c - cmmb if228 driver
 *
 *  Based on linux/drivers/media/video/cafe_ccic.c
 *
 *  Copyright:	(C) Copyright 2008 Marvell International Ltd.
 *              Yu Xu <yuxu@marvell.com>
 *
 * A register read/writer wrapper for Innofidei CMMB chip.
 * Currently works with the IF228 basing on SPI bus.
 *
 * Copyright 2006 One Laptop Per Child Association, Inc.
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * Written by Jonathan Corbet, corbet@lwn.net.
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-chip-ident.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>

#include <linux/vmalloc.h>
#include <linux/platform_device.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/cacheflush.h>

#include <linux/clk.h>

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/spi/cmmb.h>
#include <linux/gpio.h>
#include <media/if228.h>
/* #include <asm/arch/hardware.h> */
/* #include <asm/arch/littleton.h> */

/*#define CONFIG_CMMB_DEBUG*/
#ifdef CONFIG_CMMB_DEBUG
#define CMMB_ERR(fmt, x...) pr_err(fmt, ##x)
#define CMMB_INFO(fmt, x...) pr_info(fmt, ##x)
#define CMMB_DEBUG(fmt, x...) pr_devel(fmt, ##x)
#else
#define CMMB_ERR(fmt, x...)
#define CMMB_INFO(fmt, x...)
#define CMMB_DEBUG(fmt, x...)
#endif

#define DRIVER_NAME "cmmb_if"

/*
 * Parameters.
 */
MODULE_AUTHOR("Yu Xu");
MODULE_DESCRIPTION("CMMB Demodulator IF228 driver");
MODULE_LICENSE("GPL v2");
MODULE_SUPPORTED_DEVICE("Video");

/*
 * Internal DMA buffer management.  Since the controller cannot do S/G I/O,
 * we must have physically contiguous buffers to bring frames into.
 * These parameters control how many buffers we use, whether we
 * allocate them at load time (better chance of success, but nails down
 * memory) or when somebody tries to use the cmmbera (riskier), and,
 * for load-time allocation, how big they should be.
 *
 * The controller can cycle through three buffers.  We could use
 * more by flipping pointers around, but it probably makes little
 * sense.
 */

#define MAX_DMA_BUFS 3
#define CMMB_NO		(1-1)
#define CMMB_YES	1
static int alloc_bufs_at_read = CMMB_NO;
module_param(alloc_bufs_at_read, bool, 0444);
MODULE_PARM_DESC(alloc_bufs_at_read,
		 "Non-zero value causes DMA buffers to be allocated when the "
		 "video capture device is read, rather than at module load "
		 "time.  This saves memory, but decreases the chances of "
		 "successfully getting those buffers.");

static int dma_buf_size = 65536;
module_param(dma_buf_size, uint, 0444);
MODULE_PARM_DESC(dma_buf_size,
		 "The size of the allocated DMA buffers.  If actual operating "
		 "parameters require larger buffers, an attempt to reallocate "
		 "will be made.");

static int min_buffers = 1;
module_param(min_buffers, uint, 0644);
MODULE_PARM_DESC(min_buffers,
		 "The minimum number of streaming I/O buffers we are willing "
		 "to work with.");

static int max_buffers = 10;
module_param(max_buffers, uint, 0644);
MODULE_PARM_DESC(max_buffers,
		 "The maximum number of streaming I/O buffers an application "
		 "will be allowed to allocate.  These buffers are big and live "
		 "in vmalloc space.");

/*
 * A description of one of our devices.
 * Locking: controlled by s_mutex.  Certain fields, however, require
 *	    the dev_lock spinlock; they are marked as such by comments.
 *	    dev_lock is also required for access to device registers.
 */
struct cmmb_dev {
	unsigned long flags;	/* Buffer status, mainly (dev_lock) */
	int users;		/* How many open FDs */
	struct file *owner;	/* Who has data access (v4l2) */

	/*
	 * Subsystem structures.
	 */
	int irq;
	struct video_device v4ldev;
	struct v4l2_buffer v4lbuf;

	struct list_head dev_list;	/* link to other devices */

	/* DMA buffers */
	int mapcount;
	unsigned int dma_buf_size;	/* allocated size */
	int order;		/* Internal buffer addresses */

	void *dma_bufs;		/* Internal buffer addresses */
	dma_addr_t dma_handles;	/* Buffer bus addresses */

	/* Locks */
	struct mutex s_mutex;	/* Access to this structure */
	spinlock_t dev_lock;	/* Access to device */

	/* Misc */
	wait_queue_head_t iowait;	/* Waiting on frame data */
	struct spi_device *spi;
	unsigned int module_hw_status;
};

/*
 * Low-level register I/O.
 */
/*All the below cmmb_spi_xxx functions use
 * the struct cmmb_dev,instead of struct spi_device.
 * The purposes are
 *  1.make the interface simple
 *  2.easy to add other features in the further if need
 *
 * As the same as cmmb_v4l_xxx,
 * if the cmmb_spi_xxx return 0, it is OK, other is not OK
 */
#define CMMB_SPI_MAX_SIZE 4096
#define CMMB_CMD_MAX_SIZE 5
static void cmmb_spi_cs_assert(struct cmmb_dev *cmmb)
{

	struct cmmb_platform_data *pdata;

	pdata = cmmb->spi->dev.platform_data;
	if (!pdata || !pdata->cs_assert)
		return;
	pdata->cs_assert();

}

static void cmmb_spi_cs_deassert(struct cmmb_dev *cmmb)
{
	struct cmmb_platform_data *pdata;

	pdata = cmmb->spi->dev.platform_data;
	if (!pdata || !pdata->cs_deassert)
		return;
	pdata->cs_deassert();

}

static int cmmb_spi_write(struct cmmb_dev *cmmb, const u8 * buffer, int length)
{
	struct spi_device *spi = cmmb->spi;
	int offset = 0;
	int temp_size = 0;
	int ret;

	while (length) {
		temp_size =
		    (length >= CMMB_SPI_MAX_SIZE) ? CMMB_SPI_MAX_SIZE : length;

		ret = spi_write(spi, (const u8 *)buffer + offset, temp_size);
		if (ret) {
			CMMB_ERR("%s: spi_write to cmmb device error!",
				 __func__);
			break;
		}

		offset += temp_size;
		length -= temp_size;
	}
	return length;
}

static int cmmb_spi_read(struct cmmb_dev *cmmb, void *buffer, int length)
{
	struct spi_device *spi = cmmb->spi;
	int offset = 0;
	int temp_size = 0;
	int ret;

	while (length) {
		temp_size =
		    (length >= CMMB_SPI_MAX_SIZE) ? CMMB_SPI_MAX_SIZE : length;

		ret = spi_read(spi, (u8 *) buffer + offset, (size_t) temp_size);
		if (ret) {
			CMMB_ERR("%s: spi_read from cmmb device error!",
				 __func__);
			break;
		}
		offset += temp_size;
		length -= temp_size;
	}
	return length;
}

static int cmmb_spi_user_write(struct cmmb_dev *cmmb,
			       void *user_buf, int length)
{
	struct spi_device *spi = cmmb->spi;
	int offset = 0;
	int temp_size = 0;
	int ret;

	while (length) {
		temp_size =
		    (length >= CMMB_SPI_MAX_SIZE) ? CMMB_SPI_MAX_SIZE : length;

		ret = copy_from_user((u8 *) cmmb->dma_bufs,
				     (unsigned char *)user_buf + offset,
				     temp_size);
		if (ret) {
			CMMB_ERR("%s: copy_from_user error!", __func__);
			break;
		}

		ret = spi_write(spi, (const u8 *)cmmb->dma_bufs, temp_size);
		if (ret) {
			CMMB_ERR("%s: spi_write to cmmb device error!",
				 __func__);
			break;
		}

		offset += temp_size;
		length -= temp_size;
	}
	return length;
}

static int cmmb_spi_user_read(struct cmmb_dev *cmmb, void *user_buf, int length)
{
	struct spi_device *spi = cmmb->spi;
	int offset = 0;
	int temp_size = 0;
	int ret;

	while (length) {
		temp_size =
		    (length >= CMMB_SPI_MAX_SIZE) ? CMMB_SPI_MAX_SIZE : length;

		ret = spi_read(spi, (u8 *) cmmb->dma_bufs, (size_t) temp_size);
		if (ret) {
			CMMB_ERR("%s: spi_read from cmmb device error!",
				 __func__);
			break;
		}

		ret = copy_to_user((unsigned char *)user_buf + offset,
				   (u8 *) cmmb->dma_bufs, temp_size);
		if (ret) {
			CMMB_ERR("%s: copy_to_user error!", __func__);
			break;
		}

		offset += temp_size;
		length -= temp_size;
	}
	return length;
}

static int cmmb_spi_power_on_chk(struct cmmb_dev *cmmb)
{
	u8 byte;

	byte = 0x01;
	cmmb_spi_cs_assert(cmmb);
	cmmb_spi_write(cmmb, (const u8 *)&byte, sizeof(byte));
	cmmb_spi_cs_deassert(cmmb);

	cmmb_spi_cs_assert(cmmb);
	cmmb_spi_read(cmmb, (u8 *)&byte, sizeof(byte));
	cmmb_spi_cs_deassert(cmmb);

	if (byte == 0x02)	/*sucess */
		return 0;
	else			/*fail */
		return 1;
}

/* ---------------------------------------------------------------------*/
/*
 * We keep a simple list of known devices to search at open time.
 */
static LIST_HEAD(cmmb_dev_list);
static DEFINE_MUTEX(cmmb_dev_list_lock);

static void cmmb_add_dev(struct cmmb_dev *cmmb)
{
	mutex_lock(&cmmb_dev_list_lock);
	list_add_tail(&cmmb->dev_list, &cmmb_dev_list);
	mutex_unlock(&cmmb_dev_list_lock);
}

static void cmmb_remove_dev(struct cmmb_dev *cmmb)
{
	mutex_lock(&cmmb_dev_list_lock);
	list_del(&cmmb->dev_list);
	mutex_unlock(&cmmb_dev_list_lock);
}

static struct cmmb_dev *cmmb_find_dev(int minor)
{
	struct cmmb_dev *cmmb;

	mutex_lock(&cmmb_dev_list_lock);
	list_for_each_entry(cmmb, &cmmb_dev_list, dev_list) {
		if (cmmb->v4ldev.minor == minor)
			goto done;
	}
	cmmb = NULL;
done:
	mutex_unlock(&cmmb_dev_list_lock);
	return cmmb;
}

static struct cmmb_dev *cmmb_find_by_spi(struct spi_device *spi)
{
	struct cmmb_dev *cmmb;

	mutex_lock(&cmmb_dev_list_lock);
	list_for_each_entry(cmmb, &cmmb_dev_list, dev_list) {
		if (cmmb->spi == spi)
			goto done;
	}
	cmmb = NULL;
done:
	mutex_unlock(&cmmb_dev_list_lock);
	return cmmb;
}

/* -------------------------------------------------------------------- */
/*
 * DMA buffer management.  These functions need s_mutex held.
 */

/* FIXME: this is inefficient as hell, since dma_alloc_coherent just
 * does a get_free_pages() call, and we waste a good chunk of an orderN
 * allocation.  Should try to allocate the whole set in one chunk.
 */
static int cmmb_alloc_dma_bufs(struct cmmb_dev *cmmb)
{
	cmmb->dma_buf_size = dma_buf_size;

	cmmb->order = get_order(cmmb->dma_buf_size);
	cmmb->dma_bufs = (unsigned long *)
		__get_free_pages(GFP_KERNEL | GFP_DMA, cmmb->order);
	if (cmmb->dma_bufs == NULL) {
		CMMB_ERR("Failed to allocate DMA buffer\n");
		return -ENOMEM;
	}
	cmmb->dma_handles = __pa(cmmb->dma_bufs);

	/* For debug, remove eventually */
	memset(cmmb->dma_bufs, 0xcc, cmmb->dma_buf_size);

	return 0;
}

static void cmmb_free_dma_bufs(struct cmmb_dev *cmmb)
{
	free_pages((unsigned long)cmmb->dma_bufs, cmmb->order);
	cmmb->dma_bufs = NULL;
}

/* ----------------------------------------------------------------------- */
/*
 * Here starts the V4L2 interface code.
 */

static int cmmb_vidioc_g_ctrl(struct file *filp, void *priv,
			      struct v4l2_control *ctrl)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int ret;
	unsigned char addr = 0;
	unsigned char data = 0;

	mutex_lock(&cmmb->s_mutex);
	if (ctrl->id == SPI_CID_R_ONE_BYTE) {
		cmmb_spi_cs_assert(cmmb);
		ret = cmmb_spi_read(cmmb, (u8 *)&data, sizeof(data));
		cmmb_spi_cs_deassert(cmmb);
	} else {
		CMMB_ERR("%s: This control ID doesn't support get\n", __func__);
		ret = -EINVAL;
	}
	ctrl->value = CMMB_IOCTL(addr, data);
	mutex_unlock(&cmmb->s_mutex);
	return ret;
}

static int cmmb_vidioc_s_ctrl(struct file *filp, void *priv,
			      struct v4l2_control *ctrl)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int ret;
	/* unsigned char addr = CMMB_IOCTL_ADDR(ctrl->value); */
	unsigned char data = CMMB_IOCTL_DATA(ctrl->value);

	CMMB_DEBUG("enter cmmb_vidioc_s_ctrl\n");
	mutex_lock(&cmmb->s_mutex);

	if (ctrl->id == SPI_CID_W_ONE_BYTE) {
		cmmb_spi_cs_assert(cmmb);
		ret = cmmb_spi_write(cmmb, (const u8 *)&data, sizeof(data));
		cmmb_spi_cs_deassert(cmmb);
	} else {
		CMMB_ERR("%s: This control ID doesn't support set\n", __func__);
		ret = -EINVAL;
	}

	mutex_unlock(&cmmb->s_mutex);
	return ret;
}

static int cmmb_vidioc_s_ext_ctrl(struct file *filp, void *priv,
				  struct v4l2_ext_controls *ctrl)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int ret = 0;
	int rlen;
	unsigned char *buffer;
	int len;
	unsigned int addr;
	unsigned int clear_bits;
	unsigned char spi_cmd[CMMB_CMD_MAX_SIZE];

	CMMB_DEBUG("enter cmmb_vidioc_s_ext_ctrl\n");

	buffer = (unsigned char *)ctrl->controls[0].value;
	len = ctrl->controls[1].value;
	addr = ctrl->controls[2].value;
	rlen = len;

	mutex_lock(&cmmb->s_mutex);

	switch (ctrl->controls[0].id) {
	case SPI_CID_W_BYTE_TYPE2:
		CMMB_DEBUG("SPI_CID_W_BYTE_TYPE2,len:%x\n", len);
		cmmb_spi_cs_assert(cmmb);

		spi_cmd[0] = WRITE_AHBM2;
		spi_cmd[1] = (addr >> 24) & 0xff;
		spi_cmd[2] = (addr >> 16) & 0xff;
		spi_cmd[3] = (addr >> 8) & 0xff;
		spi_cmd[4] = (addr >> 0) & 0xff;
		cmmb_spi_write(cmmb, (const u8 *)spi_cmd, 5);

		rlen = cmmb_spi_user_write(cmmb, (unsigned char *)buffer, len);

		cmmb_spi_cs_deassert(cmmb);
		break;

	case SPI_CID_W_WORD_TYPE2:
		CMMB_ERR("SPI_CID_W_WORD_BYTE2 not supported\n");
		break;

	case SPI_CID_W_BYTES_TYPE3:
		CMMB_DEBUG("SPI_CID_W_BYTES_TYPE3,len:%x\n", len);
		cmmb_spi_cs_assert(cmmb);

		spi_cmd[0] = WRITE_AHBM1;
		spi_cmd[1] = (addr >> 24) & 0xff;
		spi_cmd[2] = (addr >> 16) & 0xff;
		spi_cmd[3] = (addr >> 8) & 0xff;
		spi_cmd[4] = (addr >> 0) & 0xff;
		cmmb_spi_write(cmmb, (const u8 *)spi_cmd, 5);

		rlen = cmmb_spi_user_write(cmmb, (unsigned char *)buffer, len);

		cmmb_spi_cs_deassert(cmmb);

		break;

	case SPI_CID_W_BYTES_TYPE4:
		CMMB_DEBUG("SPI_CID_W_BYTES_TYPE4,len:%x\n", len);
		cmmb_spi_cs_assert(cmmb);
		spi_cmd[0] = ctrl->controls[2].value;
		cmmb_spi_write(cmmb, (const u8 *)spi_cmd, 1);
		cmmb_spi_cs_deassert(cmmb);

		/*usleep(100); */
		cmmb_spi_cs_assert(cmmb);
		rlen = cmmb_spi_user_write(cmmb, (unsigned char *)buffer, len);
		cmmb_spi_cs_deassert(cmmb);

		break;
	case CLEAR_CMMB_MODULE_STATUS:
		ret = copy_from_user((u8 *) &clear_bits,
			(unsigned char *)buffer,
			sizeof(cmmb->module_hw_status));
		if (ret) {
			/* set rlen for below checking ret */
			rlen = 1;
		} else {
			rlen = 0;
			cmmb->module_hw_status &= ~clear_bits;
		}
		break;
	default:
		CMMB_ERR("%s: id not support\n", __func__);
		break;

	}

	mutex_unlock(&cmmb->s_mutex);

	ret = rlen ? 1 : 0;
	if (ret == 1)
		CMMB_ERR("s_ctrl error\n");
	return ret;

}

static int cmmb_vidioc_g_ext_ctrl(struct file *filp, void *priv,
				  struct v4l2_ext_controls *ctrl)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int ret = 1;
	unsigned char *buffer;
	int len;
	unsigned int addr;
	unsigned char spi_cmd[CMMB_CMD_MAX_SIZE];
	int rlen;

	buffer = (unsigned char *)ctrl->controls[0].value;
	len = ctrl->controls[1].value;
	addr = ctrl->controls[2].value;
	rlen = len;

	CMMB_DEBUG("enter cmmb_vidioc_g_ext_ctrl\n");
	mutex_lock(&cmmb->s_mutex);

	switch (ctrl->controls[0].id) {
	case SPI_CID_R_BYTES:
		CMMB_DEBUG("SPI_CID_R_BYTES, len: 0x%x\n", len);
		if (len <= dma_buf_size) {
			/*avoid to over read */
			cmmb_spi_cs_assert(cmmb);
			rlen = cmmb_spi_read(cmmb, cmmb->dma_bufs, len);
			cmmb_spi_cs_deassert(cmmb);
		} else {
			CMMB_ERR("the data read length is too large\n");
		}
		break;

	case SPI_CID_R_BYTE_TYPE2:
		CMMB_DEBUG("SPI_CID_R_BYTE_TYPE2, len: 0x%x\n", len);

		cmmb_spi_cs_assert(cmmb);

		spi_cmd[0] = READ_AHBM2;
		spi_cmd[1] = (addr >> 24) & 0xff;
		spi_cmd[2] = (addr >> 16) & 0xff;
		spi_cmd[3] = (addr >> 8) & 0xff;
		spi_cmd[4] = (addr >> 0) & 0xff;
		cmmb_spi_write(cmmb, (const u8 *)spi_cmd, 5);

		/* the first byte should be droped due to the if228 issue */
		rlen = cmmb_spi_read(cmmb, (u8 *) cmmb->dma_bufs, 1);

		rlen |= cmmb_spi_user_read(cmmb, (unsigned char *)buffer, len);

		cmmb_spi_cs_deassert(cmmb);

		CMMB_DEBUG("%x %x %x %x %x\n", *(u8 *) cmmb->dma_bufs,
			   *((u8 *) cmmb->dma_bufs + 1),
			   *((u8 *) cmmb->dma_bufs + 2),
			   *((u8 *) cmmb->dma_bufs + 3),
			   *((u8 *) cmmb->dma_bufs + 4));

		break;

	case SPI_CID_R_WORD_TYPE2:
		CMMB_ERR("SPI_CID_R_WORD_TYPE2 not supported.\n");
		break;

	case SPI_CID_R_BYTES_TYPE3:
		CMMB_DEBUG("SPI_CID_R_BYTES_TYPE3, len: 0x%x\n", len);
		cmmb_spi_cs_assert(cmmb);

		spi_cmd[0] = READ_AHBM1;
		spi_cmd[1] = (addr >> 24) & 0xff;
		spi_cmd[2] = (addr >> 16) & 0xff;
		spi_cmd[3] = (addr >> 8) & 0xff;
		spi_cmd[4] = (addr >> 0) & 0xff;
		cmmb_spi_write(cmmb, (const u8 *)spi_cmd, 5);

		/* the first byte should be droped due to the if228 issue */
		rlen = cmmb_spi_read(cmmb, (u8 *) cmmb->dma_bufs, 1);

		rlen |= cmmb_spi_user_read(cmmb, (unsigned char *)buffer, len);

		cmmb_spi_cs_deassert(cmmb);

		CMMB_DEBUG("%x %x %x %x %x\n", *(u8 *) cmmb->dma_bufs,
			   *((u8 *) cmmb->dma_bufs + 1),
			   *((u8 *) cmmb->dma_bufs + 2),
			   *((u8 *) cmmb->dma_bufs + 3),
			   *((u8 *) cmmb->dma_bufs + 4));

		break;

	case SPI_CID_R_BYTES_TYPE4:
		CMMB_DEBUG("SPI_CID_R_BYTES_TYPE4, len: 0x%x\n", len);
		cmmb_spi_cs_assert(cmmb);
		spi_cmd[0] = ctrl->controls[2].value;
		cmmb_spi_write(cmmb, (const u8 *)spi_cmd, 1);
		cmmb_spi_cs_deassert(cmmb);

		/*usleep(100); */
		cmmb_spi_cs_assert(cmmb);
		rlen = cmmb_spi_user_read(cmmb, (unsigned char *)buffer, len);
		cmmb_spi_cs_deassert(cmmb);

		break;

	case CHECK_CMMB_MODULE_STATUS:
		rlen = copy_to_user((unsigned char *)buffer,
				   (u8 *) &cmmb->module_hw_status,
				   sizeof(cmmb->module_hw_status));
		break;

	default:
		CMMB_ERR("%s: id not support\n", __func__);
		break;

	}

	mutex_unlock(&cmmb->s_mutex);

	ret = rlen ? 1 : 0;
	if (ret == 1)
		CMMB_ERR("cmmb_vidioc_g_ext_ctrl error\n");
	return ret;
}

static int cmmb_vidioc_try_fmt_cap(struct file *filp, void *priv,
				   struct v4l2_format *fmt)
{
	return 0;
}

/* The function is used for fmt_check in v4l2-ioctl.c */
static int cmmb_vidioc_g_fmt_cap(struct file *filp, void *priv,
				 struct v4l2_format *fmt)
{
	return 0;
}

static int cmmb_vidioc_querybuf(struct file *filp, void *priv,
				struct v4l2_buffer *buf)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int ret = -EINVAL;

	mutex_lock(&cmmb->s_mutex);
	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		goto out;

	cmmb->v4lbuf.length = dma_buf_size;
	cmmb->v4lbuf.index = 0;
	cmmb->v4lbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cmmb->v4lbuf.field = V4L2_FIELD_NONE;
	cmmb->v4lbuf.memory = V4L2_MEMORY_MMAP;
	cmmb->v4lbuf.m.offset = 0;

	*buf = cmmb->v4lbuf;

	ret = 0;
out:
	mutex_unlock(&cmmb->s_mutex);
	return ret;
}

static void cmmb_v4l_vm_open(struct vm_area_struct *vma)
{
	struct cmmb_dev *cmmb = vma->vm_private_data;
	/*
	 * Locking: done under mmap_sem, so we don't need to
	 * go back to the cmmb lock here.
	 */
	cmmb->mapcount++;
}

static void cmmb_v4l_vm_close(struct vm_area_struct *vma)
{
	struct cmmb_dev *cmmb = vma->vm_private_data;

	mutex_lock(&cmmb->s_mutex);
	cmmb->mapcount--;
	/* Docs say we should stop I/O too... */
	if (cmmb->mapcount == 0)
		cmmb->v4lbuf.flags &= ~V4L2_BUF_FLAG_MAPPED;
	mutex_unlock(&cmmb->s_mutex);
}

static struct vm_operations_struct cmmb_v4l_vm_ops = {
	.open = cmmb_v4l_vm_open,
	.close = cmmb_v4l_vm_close
};

static int cmmb_v4l_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int ret = -EINVAL;

	/* if (! (vma->vm_flags & VM_WRITE) || ! (vma->vm_flags & VM_SHARED)) */
	/* if (! (vma->vm_flags & VM_SHARED)) */
	/*      return -EINVAL; */
	/*
	 * Find the buffer they are looking for.
	 */
	mutex_lock(&cmmb->s_mutex);

	ret =
	    remap_pfn_range(vma, vma->vm_start,
			    cmmb->dma_handles >> PAGE_SHIFT,
			    vma->vm_end - vma->vm_start,
			    pgprot_noncached(vma->vm_page_prot));
	if (ret)
		goto out;
	vma->vm_flags |= VM_DONTEXPAND;
	vma->vm_private_data = cmmb;
	vma->vm_ops = &cmmb_v4l_vm_ops;
	cmmb->v4lbuf.flags |= V4L2_BUF_FLAG_MAPPED;
	cmmb_v4l_vm_open(vma);
	ret = 0;
out:
	mutex_unlock(&cmmb->s_mutex);
	return ret;
}

static int cmmb_v4l_open(struct file *filp)
{
	struct cmmb_dev *cmmb;
	struct cmmb_platform_data *pdata;

	cmmb = cmmb_find_dev(video_devdata(filp)->minor);
	if (cmmb == NULL)
		return -ENODEV;

	pdata = cmmb->spi->dev.platform_data;

	filp->private_data = cmmb;

	clear_bit(0, &cmmb->flags);

	mutex_lock(&cmmb->s_mutex);
	(cmmb->users)++;
	if ((cmmb->users == 1) && (pdata->power_on)) {
		pdata->power_on();
		cmmb->module_hw_status &= ~CMMB_MODULE_STATUS_POWER_OFF;
	}
	mutex_unlock(&cmmb->s_mutex);

	return 0;
}

static int cmmb_v4l_release(struct file *filp)
{
	struct cmmb_dev *cmmb = filp->private_data;
	struct cmmb_platform_data *pdata;

	mutex_lock(&cmmb->s_mutex);
	(cmmb->users)--;
	pdata = cmmb->spi->dev.platform_data;
	if ((cmmb->users == 0) && (pdata->power_off)) {
		pdata->power_off();
		cmmb->module_hw_status |= CMMB_MODULE_STATUS_POWER_OFF;
	}
	mutex_unlock(&cmmb->s_mutex);

	return 0;
}

static unsigned int cmmb_v4l_poll(struct file *filp,
				  struct poll_table_struct *pt)
{
	struct cmmb_dev *cmmb = filp->private_data;
	int frame = 0;
	int data_ready = 0;

	poll_wait(filp, &cmmb->iowait, pt);

	spin_lock(&cmmb->dev_lock);

	data_ready = test_and_clear_bit(frame, &cmmb->flags);

	spin_unlock(&cmmb->dev_lock);

	if (data_ready)
		return POLLIN | POLLRDNORM;
	else
		return 0;
}

static void cmmb_v4l_dev_release(struct video_device *vd)
{
	return;
}

/*
 * This template device holds all of those v4l2 methods; we
 * clone it for specific real devices.
 */

static const struct v4l2_file_operations cmmb_v4l_fops = {
	.owner = THIS_MODULE,
	.open = cmmb_v4l_open,
	.release = cmmb_v4l_release,
	.poll = cmmb_v4l_poll,
	.mmap = cmmb_v4l_mmap,
	.ioctl = video_ioctl2,
};

static const struct v4l2_ioctl_ops cmmb_v4l_ioctl_ops = {
	.vidioc_try_fmt_vid_cap = cmmb_vidioc_try_fmt_cap,
	.vidioc_querybuf = cmmb_vidioc_querybuf,
	.vidioc_g_ctrl = cmmb_vidioc_g_ctrl,
	.vidioc_s_ctrl = cmmb_vidioc_s_ctrl,
	.vidioc_g_ext_ctrls = cmmb_vidioc_g_ext_ctrl,
	.vidioc_s_ext_ctrls = cmmb_vidioc_s_ext_ctrl,
	.vidioc_g_fmt_vid_cap = cmmb_vidioc_g_fmt_cap,
};

static struct video_device cmmb_v4l_template = {
	.name = "cmmb-module",
	.minor = -1,		/* Get one dynamically */
	.tvnorms = V4L2_STD_NTSC_M,
	/* .current_norm = V4L2_STD_NTSC_M, *//* make mplayer happy */

	.fops = &cmmb_v4l_fops,
	.ioctl_ops = &cmmb_v4l_ioctl_ops,
	.release = cmmb_v4l_dev_release,
};

/* ---------------------------------------------------------------------- */
/*
 * Interrupt handler stuff
 */

static irqreturn_t cmmb_irq(int irq, void *data)
{
	struct cmmb_dev *cmmb = data;
	int frame = 0;

	spin_lock(&cmmb->dev_lock);
	/*
	 * Basic frame housekeeping.
	 */
	if (test_bit(frame, &cmmb->flags) && printk_ratelimit()) {
		/* CMMB_INFO("Frame overrun on %d, frames lost\n", frame); */
		;
	}
	set_bit(frame, &cmmb->flags);

	spin_unlock(&cmmb->dev_lock);

	wake_up_interruptible(&cmmb->iowait);

	return IRQ_HANDLED;
}

static int __devinit cmmb_probe(struct spi_device *spi)
{
	struct cmmb_platform_data *pdata;
	struct cmmb_dev *cmmb;
	int ret;

	pdata = spi->dev.platform_data;
	if (!pdata || !pdata->power_on)
		return -ENODEV;
	pdata->power_on();

	/*
	 * bits_per_word cannot be configured in platform data
	 */
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret < 0)
		goto out;

	/*
	 * Start putting together one of our big cmmb device structures.
	 */
	ret = -ENOMEM;
	cmmb = kzalloc(sizeof(struct cmmb_dev), GFP_KERNEL);
	if (cmmb == NULL)
		goto out;

	cmmb->spi = spi;

#if 1				/*spi test */
	msleep(100);
	if (0 != cmmb_spi_power_on_chk(cmmb)) {
		/*return 0 is OK */
		CMMB_INFO("CMMB IF208  Demodulator can't be detected\n");
		if (pdata->power_off) {
			pdata->power_off();
			cmmb->module_hw_status |= CMMB_MODULE_STATUS_POWER_OFF;
		}
		ret = -EINVAL;
		goto out_free;
	}
#endif

	mutex_init(&cmmb->s_mutex);
	mutex_lock(&cmmb->s_mutex);

	spin_lock_init(&cmmb->dev_lock);
	INIT_LIST_HEAD(&cmmb->dev_list);
	init_waitqueue_head(&cmmb->iowait);

	ret = request_irq(spi->irq, cmmb_irq, IRQF_TRIGGER_FALLING,
			  "CMMB Demodulator", cmmb);
	if (ret) {
		CMMB_INFO("CMMB request irq failed.\n");
		goto out_mutex;
	}

	/*
	 * Get the v4l2 setup done.
	 */
	memcpy(&cmmb->v4ldev, &cmmb_v4l_template, sizeof(cmmb_v4l_template));
	cmmb->v4ldev.debug = 0;
	ret = video_register_device(&cmmb->v4ldev, VFL_TYPE_GRABBER, -1);
	if (ret)
		goto out_mutex;
	/*
	 * If so requested, try to get our DMA buffers now.
	 */
	if (!alloc_bufs_at_read) {
		if (cmmb_alloc_dma_bufs(cmmb))
			CMMB_INFO("Unable to alloc DMA buffers at load"
				  " will try again later.");
	}

	mutex_unlock(&cmmb->s_mutex);
	cmmb_add_dev(cmmb);

	if (pdata->power_off) {
		pdata->power_off();
		cmmb->module_hw_status |= CMMB_MODULE_STATUS_POWER_OFF;
	}

	CMMB_INFO("CMMB Demodulator detected\n");

	return ret;
out_mutex:
	mutex_unlock(&cmmb->s_mutex);
out_free:
	kzfree(cmmb);
out:
	if (pdata->power_off)
		pdata->power_off();

	return ret;
}

static int cmmb_remove(struct spi_device *spi)
{
	struct cmmb_dev *cmmb = cmmb_find_by_spi(spi);

	if (cmmb == NULL) {
		CMMB_INFO("cmmb_remove on unknown spi %p\n", spi);
		return -ENODEV;
	}

	mutex_lock(&cmmb->s_mutex);
	if (cmmb->users > 0)
		CMMB_INFO("Removing a device with users!\n");

	cmmb_remove_dev(cmmb);

	if (!alloc_bufs_at_read)
		cmmb_free_dma_bufs(cmmb);

	video_unregister_device(&cmmb->v4ldev);

	free_irq(spi->irq, cmmb);

	mutex_unlock(&cmmb->s_mutex);

	if (cmmb)
		kzfree(cmmb);

	return 0;
}

static int cmmb_suspend(struct spi_device *spi, pm_message_t mesg)
{
	struct cmmb_dev *cmmb = cmmb_find_by_spi(spi);
	struct cmmb_platform_data *pdata;

	if (cmmb == NULL) {
		CMMB_INFO("cmmb_suspend on unknown spi %p\n", spi);
		return -ENODEV;
	}

	if (!(cmmb->module_hw_status & CMMB_MODULE_STATUS_POWER_OFF)) {
		pdata = cmmb->spi->dev.platform_data;
		if ((pdata) && (pdata->power_off))
			pdata->power_off();
		cmmb->module_hw_status |= CMMB_MODULE_STATUS_POWER_OFF;
	}
	cmmb->module_hw_status |= CMMB_MODULE_STATUS_NEED_REINIT;

	return 0;
}
static int cmmb_resume(struct spi_device *spi)
{
	struct cmmb_dev *cmmb = cmmb_find_by_spi(spi);
	struct cmmb_platform_data *pdata;

	if (cmmb == NULL) {
		CMMB_INFO("cmmb_resume on unknown spi %p\n", spi);
		return -ENODEV;
	}
	pdata = cmmb->spi->dev.platform_data;

	return 0;
}
static struct spi_driver cmmb_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = cmmb_probe,
	.remove = __devexit_p(cmmb_remove),
	.suspend = cmmb_suspend,
	.resume =  cmmb_resume,
};

/*
 * Module initialization
 */

static int __init cmmb_module_init(void)
{
	CMMB_INFO("CMMB Demodulator driver, at your service\n");

	return spi_register_driver(&cmmb_driver);
}

static void __exit cmmb_module_exit(void)
{
	spi_unregister_driver(&cmmb_driver);
}

module_init(cmmb_module_init);
module_exit(cmmb_module_exit);
