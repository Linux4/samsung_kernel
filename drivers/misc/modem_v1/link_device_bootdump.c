/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

static inline int check_udl_space(struct mem_link_device *mld,
				  struct mem_ipc_device *dev,
				  unsigned int qsize, unsigned int in,
				  unsigned int out, unsigned int count)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int space;

	if (!circ_valid(qsize, in, out)) {
		mif_err("%s: ERR! Invalid %s_TXQ{qsize:%d in:%d out:%d}\n",
			ld->name, dev->name, qsize, in, out);
		return -EIO;
	}

	space = circ_get_space(qsize, in, out);
	if (unlikely(space < count)) {
		mif_err("%s: NOSPC in %s_TXQ{qsize:%d in:%d "
			"out:%d space:%d count:%d}\n", ld->name,
			dev->name, qsize, in, out, space, count);
		return -ENOSPC;
	}

	return 0;
}

/**
@brief		copy UDL data in an skb to a circular TXQ

Enqueues a frame in @b @@skb to the @b @@dev TXQ if there is enough space in the
TXQ, then releases @b @@skb.

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance
@param skb	the pointer to an sk_buff instance

@retval "> 0"	the size of the frame written in the TXQ
@retval "< 0"	an error code (-EBUSY, -ENOSPC, or -EIO)
*/
static inline int udl_write(struct mem_link_device *mld,
			    struct mem_ipc_device *dev, struct sk_buff *skb)
{
	unsigned int count = skb->len;
	char *src = skb->data;
	char *dst = get_txq_buff(dev);
	unsigned int qsize = get_txq_buff_size(dev);
	unsigned int in = get_txq_head(dev);
	unsigned int out = get_txq_tail(dev);
	int space;

	space = check_udl_space(mld, dev, qsize, in, out, count);
	if (unlikely(space < 0))
		return space;

	barrier();

	circ_write(dst, src, qsize, in, count);

	barrier();

	set_txq_head(dev, circ_new_ptr(qsize, in, count));

	/* Commit the item before incrementing the head */
	smp_mb();

	return count;
}

/**
@brief		transmit a BOOT/DUMP data packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
*/
int xmit_udl(struct mem_link_device *mld, struct io_device *iod,
		    enum sipc_ch_id ch, struct sk_buff *skb)
{
	int ret;
	struct mem_ipc_device *dev = mld->dev[IPC_RAW];
	int count = skb->len;
	int tried = 0;

	while (1) {
		ret = udl_write(mld, dev, skb);
		if (ret == count)
			break;

		if (ret != -ENOSPC)
			goto exit;

		tried++;
		if (tried >= 20)
			goto exit;

		if (in_interrupt())
			mdelay(50);
		else
			msleep(50);
	}

	dev_kfree_skb_any(skb);

exit:
	return ret;
}

/**
@brief		function for the @b xmit_boot method in a link_device instance

Copies a CP bootloader binary in a user space to the BOOT region for CP

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param arg	the pointer to a modem_firmware instance

@retval "= 0"	if NO error
@retval "< 0"	an error code
*/
int mem_xmit_boot(struct link_device *ld, struct io_device *iod,
		     unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	void __iomem *dst;
	void __user *src;
	int err;
	struct modem_firmware mf;

	/**
	 * Get the information about the boot image
	 */
	memset(&mf, 0, sizeof(struct modem_firmware));

	err = copy_from_user(&mf, (const void __user *)arg, sizeof(mf));
	if (err) {
		mif_err("%s: ERR! INFO copy_from_user fail\n", ld->name);
		return -EFAULT;
	}

	/**
	 * Check the size of the boot image
	 */
	if (mf.size > mld->boot_size) {
		mif_err("%s: ERR! Invalid BOOT size %d\n", ld->name, mf.size);
		return -EINVAL;
	}
	mif_err("%s: BOOT size = %d bytes\n", ld->name, mf.size);

	/**
	 * Copy the boot image to the BOOT region
	 */
	memset(mld->boot_base, 0, mld->boot_size);

	dst = (void __iomem *)mld->boot_base;
	src = (void __user *)mf.binary;
	err = copy_from_user(dst, src, mf.size);
	if (err) {
		mif_err("%s: ERR! BOOT copy_from_user fail\n", ld->name);
		return err;
	}

	return 0;
}

/**
@brief		function for the @b dload_start method in a link_device instance

Set all flags and environments for CP binary download

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
int mem_start_download(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	reset_ipc_map(mld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_BOOT))
		sbd_deactivate(&mld->sbd_link_dev);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_BOOT_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	if (mld->dpram_magic) {
		unsigned int magic;

		set_magic(mld, MEM_BOOT_MAGIC);
		magic = get_magic(mld);
		if (magic != MEM_BOOT_MAGIC) {
			mif_err("%s: ERR! magic 0x%08X != BOOT_MAGIC 0x%08X\n",
				ld->name, magic, MEM_BOOT_MAGIC);
			return -EFAULT;
		}
		mif_err("%s: magic == 0x%08X\n", ld->name, magic);
	}

	return 0;
}

/**
@brief		function for the @b firm_update method in a link_device instance

Updates download information for each CP binary image by copying download
information for a CP binary image from a user space to a local buffer in a
mem_link_device instance.

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param arg	the pointer to a std_dload_info instance
*/
int mem_update_firm_info(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	int ret;

	ret = copy_from_user(&mld->img_info, (void __user *)arg,
			     sizeof(struct std_dload_info));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	return 0;
}

/**
@brief		function for the @b dump_start method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
int mem_start_upload(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP))
		sbd_deactivate(&mld->sbd_link_dev);

	reset_ipc_map(mld);

	if (mld->attrs & LINK_ATTR(LINK_ATTR_DUMP_ALIGNED))
		ld->aligned = true;
	else
		ld->aligned = false;

	if (mld->dpram_magic) {
		unsigned int magic;

		set_magic(mld, MEM_DUMP_MAGIC);
		magic = get_magic(mld);
		if (magic != MEM_DUMP_MAGIC) {
			mif_err("%s: ERR! magic 0x%08X != DUMP_MAGIC 0x%08X\n",
				ld->name, magic, MEM_DUMP_MAGIC);
			return -EFAULT;
		}
		mif_err("%s: magic == 0x%08X\n", ld->name, magic);
	}

	return 0;
}

