/*
 * linux/drivers/video/fbdev/exynos/mafpc/mafpc_drv.c
 *
 * Source file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "../panel_debug.h"
#include "../panel_drv.h"
#include "mafpc_drv.h"

#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
#include <soc/samsung/exynos/exynos-lpd.h>
#endif

static int abc_fops_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct miscdevice *miscdev = file->private_data;
	struct mafpc_device *mafpc = container_of(miscdev, struct mafpc_device, miscdev);

	file->private_data = mafpc;
	panel_info("was called\n");

	return ret;
}


static ssize_t write_comp_image(struct mafpc_device *mafpc, const char __user *buf, size_t count)
{
	bool scale_factor = false;
	int offset = MAFPC_HEADER_SIZE;

	if ((mafpc == NULL) || (buf == NULL)) {
		panel_err("invalid param\n");
		return -EINVAL;
	}

	if ((!mafpc->comp_img_buf) || (!mafpc->scale_buf)) {
		panel_err("invalid buffer");
		return -EINVAL;
	}

	if (count == (MAFPC_HEADER_SIZE + mafpc->ctrl_cmd_len + mafpc->comp_img_len)) {
		panel_info("Write size: %ld, without scale factor\n", count);
	} else if (count == (MAFPC_HEADER_SIZE + mafpc->ctrl_cmd_len + mafpc->comp_img_len + mafpc->scale_len)) {
		panel_info("Write size: %ld, with scale factor\n", count);
		scale_factor = true;
	} else {
		panel_err("invalid count: %ld\n", count);
		return -EINVAL;
	}

	if (copy_from_user(mafpc->ctrl_cmd, buf + offset, mafpc->ctrl_cmd_len)) {
		panel_err("failed to get user's header\n");
		goto exit_comp_write;
	}
	print_hex_dump(KERN_ERR, "COMP_IMG", DUMP_PREFIX_ADDRESS, 32, 4, mafpc->ctrl_cmd, mafpc->ctrl_cmd_len, false);

	offset += mafpc->ctrl_cmd_len;
	if (copy_from_user(mafpc->comp_img_buf, buf + offset, mafpc->comp_img_len)) {
		panel_err("failed to get comp img\n");
		goto exit_comp_write;
	}

	if (scale_factor) {
		offset += mafpc->comp_img_len;
		if (copy_from_user(mafpc->scale_buf, buf + offset, mafpc->scale_len)) {
			panel_err("failed to get comp img\n");
			goto exit_comp_write;
		}
		print_hex_dump(KERN_ERR, "SCALE_FACTOR", DUMP_PREFIX_ADDRESS, 32, 4,
			mafpc->scale_buf, mafpc->scale_len, false);
	}
	mafpc->written |= MAFPC_UPDATED_FROM_SVC;

exit_comp_write:
	return count;
}


#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)

#define MAX_LUT_TABLE		(256 * 4 * 3)

/* header(1byte) + lut_type(1byte) + reserved + reserved
 * lut_type => 0,  grey ^ coeff [0]: 0, [1] : 1
 * lut_type => 1,  grey/255 ^ coeff  [0] : 0, [255] : 1
 */
static ssize_t write_lut_table(struct mafpc_device *mafpc, const char __user *buf, size_t count)
{
	int ret;
	unsigned int header;
	void *lut_buf;
	struct comp_mem_info *comp_mem;

	if ((mafpc == NULL) || (buf == NULL)) {
		panel_err("invalid param\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;
	panel_info("lut buf size: %d, count: %d\n", comp_mem->lut_size, count);
	if (comp_mem->lut_size != count) {
		panel_err("invalid lut size: %d\n");
		return -EINVAL;
	}

	lut_buf = (void *)phys_to_virt(comp_mem->lut_base);
	if (lut_buf == NULL) {
		panel_err("null lut buf\n");
		return -EINVAL;
	}

	ret = copy_from_user(&header, buf, LPD_COMP_LUT_HEADER_SZ);
	if (ret) {
		panel_err("failed to get user's header\n");
		return ret;
	}
	panel_info("header : %c, lut_type: %d\n", header, (header >> 8) & 0xff);

	ret = copy_from_user(lut_buf, buf, count);
	if (ret) {
		panel_err("failed to get user's header\n");
		return ret;
	}

	return count;

}
#endif


static ssize_t abc_fops_write(struct file *file, const char __user *buf,  size_t count, loff_t *ppos)
{
	int ret = 0;
	char header[MAFPC_HEADER_SIZE];
	struct mafpc_device *mafpc = file->private_data;

	ret = copy_from_user(header, buf, MAFPC_HEADER_SIZE);
	if (ret) {
		panel_err("failed to get user's header\n");
		return ret;
	}

	panel_info("write cnt: %d, header : %c\n", count, header[MAFPC_DATA_IDENTIFIER]);

	switch (header[MAFPC_DATA_IDENTIFIER]) {
	case 'M':
		ret = write_comp_image(mafpc, buf, count);
		if (ret < 0) {
			panel_err("failed to write compensation image\n");
			return 0;
		}
		break;
#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
	case 'L':
		ret = write_lut_table(mafpc, buf, count);
		if (ret < 0) {
			panel_err("failed to write lut table\n");
			return 0;
		}
		break;
#endif
	default:
		panel_err("known identifier: %d\n", header[MAFPC_DATA_IDENTIFIER]);
		return 0;
	}

	return count;

}

static int mafpc_instant_on(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	mafpc->req_update = true;
	panel_mutex_lock(&mafpc->mafpc_lock);

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		goto exit_write;
	}

	if (panel_get_cur_state(panel) == PANEL_STATE_ALPM) {
		panel_err("gct not supported on LPM\n");
		goto exit_write;
	}

	/*
	 * mAFPC Instant_on consists of two stage.
	 * 1. Send compensation image for mAFPC to DDI, as soon as transmitted the instant_on ioctl.
	 * 2. Send instant_on command to DDI, after frame done
	 */
	panel_info("++ PANEL_MAFPC_IMG_SEQ ++\n");
	ret = panel_do_seqtbl_by_name(panel, PANEL_MAFPC_IMG_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
		goto exit_write;
	}

	mafpc->written |= MAFPC_UPDATED_TO_DEV;
	mafpc->req_instant_on = true;

exit_write:
	panel_info("-- PANEL_MAFPC_IMG_SEQ -- \n");
	panel_mutex_unlock(&mafpc->mafpc_lock);
	mafpc->req_update = false;
	return ret;
}

static int mafpc_instant_off(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	panel_mutex_lock(&mafpc->mafpc_lock);

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_err("panel is not active\n");
		goto err;
	}

	if (panel_get_cur_state(panel) == PANEL_STATE_ALPM) {
		panel_err("gct not supported on LPM\n");
		goto err;
	}

	ret = panel_do_seqtbl_by_name(panel, PANEL_MAFPC_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
	}

err:
	panel_mutex_unlock(&mafpc->mafpc_lock);
	return ret;
}

static long abc_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct mafpc_device *mafpc = file->private_data;
	switch (cmd) {
	case IOCTL_MAFPC_ON:
		panel_info("mAFPC on\n");
		mafpc->enable = true;
		break;
	case IOCTL_MAFPC_ON_INSTANT:
		panel_info("mAFPC on instantly On\n");
		mafpc->enable = true;
		ret = mafpc_instant_on(mafpc);
		if (ret)
			panel_info("failed to instant on\n");
		break;
	case IOCTL_MAFPC_OFF:
		panel_info("mAFPC off\n");
		mafpc->enable = false;
		break;
	case IOCTL_MAFPC_OFF_INSTANT:
		panel_info("mAFPC off instantly\n");
		mafpc->enable = false;
		ret = mafpc_instant_off(mafpc);
		if (ret)
			panel_info("failed to instant off\n");
		break;
	default:
		panel_info("Invalid Command\n");
		break;
	}

	return ret;
}

static ssize_t abc_fops_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	void *image_buf;
	struct comp_mem_info *comp_mem;
	struct mafpc_device *mafpc = file->private_data;

	panel_info("count:%d\n", count);

	if (mafpc == NULL) {
		panel_err("null mafpc\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;
	if (comp_mem->reserved == false) {
		panel_err("does not supprot read ops\n");
		return 0;
	}

	if (count >= comp_mem->image_size) {
		panel_err("wrong buffer size\n");
		return 0;
	}

	image_buf = (void *)phys_to_virt(comp_mem->image_base);
	if (image_buf == NULL) {
		panel_err("failed to get virtual address\n");
		return 0;
	}

	print_hex_dump(KERN_ERR, "image_base", DUMP_PREFIX_ADDRESS, 32, 4, image_buf, 32, false);

	ret = copy_to_user(buf, image_buf, count);
	if (ret) {
		panel_err("failed to copy to user\n");
		return ret;
	}

	return count;

}

static int abc_fops_release(struct inode *inode, struct file *file)
{
	panel_info("was called\n");

	return 0;
}

static const struct file_operations abc_drv_fops = {
	.owner = THIS_MODULE,
	.open = abc_fops_open,
	.write = abc_fops_write,
	.read = abc_fops_read,
	.compat_ioctl = abc_fops_ioctl,
	.unlocked_ioctl = abc_fops_ioctl,
	.release = abc_fops_release,
};

static int mafpc_v4l2_probe(struct mafpc_device *mafpc, void *arg)
{
	int ret = 0;
	struct mafpc_info *info = (struct mafpc_info *)arg;

	if (unlikely(!info)) {
		panel_err("got null mafpc info\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_img) {
		panel_err("MCD:ABC: Can't found abc image\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_crc) {
		panel_err("MCD:ABC: Can't found abc's crc value\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_scale_factor) {
		panel_err("MCD:ABC: Can't found abc's scale factor\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	if (!info->abc_scale_map_tbl) {
		panel_err("MCD:ABC: Can't found abc's scale br map\n");
		ret = -EINVAL;
		goto err_v4l2_probe;
	}

	mafpc->comp_img_buf = info->abc_img;
	mafpc->comp_img_len = info->abc_img_len;
	panel_info("MCD:ABC: ABC Image Size : %d\n", mafpc->comp_img_len);

	mafpc->scale_buf = info->abc_scale_factor;
	mafpc->scale_len = info->abc_scale_factor_len;
	panel_info("MCD:ABC: ABC's Scale Factor Size : %d\n", mafpc->scale_len);

	mafpc->comp_crc_buf = info->abc_crc;
	mafpc->comp_crc_len = info->abc_crc_len;

	mafpc->scale_map_br_tbl = info->abc_scale_map_tbl;
	mafpc->scale_map_br_tbl_len = info->abc_scale_map_tbl_len;

	mafpc->ctrl_cmd_len = info->abc_ctrl_cmd_len;
	return ret;

err_v4l2_probe:
	return ret;
}

static int mafpc_v4l2_frame_done(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	ret = panel_do_seqtbl_by_name(panel, PANEL_MAFPC_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
	}

	mafpc->req_instant_on = false;

	return ret;
}

static long mafpc_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct mafpc_device *mafpc = container_of(sd, struct mafpc_device, sd);

	switch (cmd) {
	case V4L2_IOCTL_PROBE_ABC:
		panel_info("MCD:ABC: v4l2_ioctl: PROBE_ABC\n");
		ret = mafpc_v4l2_probe(mafpc, arg);
		break;

	case V4L2_IOCTL_MAFPC_GET_INFO:
		v4l2_set_subdev_hostdata(sd, mafpc);
		break;

	case V4L2_IOCTL_MAFPC_PANEL_INIT:
		panel_info("MCD:ABC: v4l2_ioctl: V4L2_IOCTL_MAFPC_GET_INIT\n");
		mafpc->written |= MAFPC_UPDATED_TO_DEV;
		break;

	case V4L2_IOCTL_MAFPC_PANEL_EXIT:
		panel_info("MCD:ABC: v4l2_ioctl: V4L2_IOCTL_MAFPC_GET_EXIT\n");
		mafpc->written &= ~(MAFPC_UPDATED_TO_DEV);
		break;

	case V4L2_IOCL_MAFPC_FRAME_DONE:
		if (mafpc->req_instant_on) {
			panel_info("MCD:ABC: v4l2_ioctl: V4L2_IOCL_MAFPC_FRAME_DONE\n");
			ret = mafpc_v4l2_frame_done(mafpc);
		}
		break;
	default:
		panel_err("invalid cmd\n");
	}

	return ret;
}

static const struct v4l2_subdev_core_ops mafpc_v4l2_sd_core_ops = {
	.ioctl = mafpc_core_ioctl,
};

static const struct v4l2_subdev_ops mafpc_subdev_ops = {
	.core = &mafpc_v4l2_sd_core_ops,
};

static void mafpc_init_v4l2_subdev(struct mafpc_device *mafpc)
{
	struct v4l2_subdev *sd = &mafpc->sd;

	v4l2_subdev_init(sd, &mafpc_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = 0;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", MAFPC_V4L2_DEV_NAME, mafpc->id);
	v4l2_set_subdevdata(sd, mafpc);
}

struct mafpc_device *mafpc_device_create(void)
{
	struct mafpc_device *mafpc;

	mafpc = kzalloc(sizeof(struct mafpc_device), GFP_KERNEL);
	if (!mafpc)
		return NULL;

	return mafpc;
}
EXPORT_SYMBOL(mafpc_device_create);

int mafpc_device_init(struct mafpc_device *mafpc)
{
	if (!mafpc)
		return -EINVAL;

	mafpc_init_v4l2_subdev(mafpc);

	return 0;
}
EXPORT_SYMBOL(mafpc_device_init);

struct mafpc_device *get_mafpc_device(struct panel_device *panel)
{
	struct mafpc_device *mafpc;
	int ret;

	if (!panel)
		return NULL;

	if (panel->mafpc_sd == NULL) {
		panel_err("mafpc_sd is null\n");
		return NULL;
	}

	/* check if mafpc v4l2 subdev initialized */
	if (!panel->mafpc_sd->ops) {
		panel_err("mafpc v4l2 subdev is not initlaized\n");
		return NULL;
	}

	ret = v4l2_subdev_call(panel->mafpc_sd, core, ioctl,
		V4L2_IOCTL_MAFPC_GET_INFO, NULL);
	if (ret < 0) {
		panel_err("failed to mafpc V4L2_IOCTL_MAFPC_GET_INFO\n");
		return NULL;
	}

	mafpc = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return NULL;
	}

	return mafpc;
}
EXPORT_SYMBOL(get_mafpc_device);


#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)

int parse_lpd_rmem(struct mafpc_device *mafpc, struct device *dev)
{
	struct device_node *np;
	struct device_node *node;
	struct reserved_mem *rmem;
	struct comp_mem_info *comp_mem;
	unsigned int canvas_size;
	phys_addr_t canvas_base;

	if ((mafpc == NULL) || (dev == NULL)) {
		panel_err("invalid param");
		return -EINVAL;
	}

	node = dev->of_node;
	if (node == NULL) {
		panel_err("null node\n");
		return -EINVAL;
	}

	np = of_parse_phandle(node, "memory-region", 0);
	if (np == NULL) {
		panel_err("failed to parse reserved mem for burnin\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(np);
	if (rmem == NULL) {
		panel_err("failed to get reserved mem for burnin\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;

	comp_mem->image_size = LPD_COMP_IMAGE_SIZE;
	canvas_size = LPD_COMP_CANVAS_SIZE;
	comp_mem->lut_size = LPD_COMP_LUT_SIZE;

	comp_mem->image_base = rmem->base + rmem->size - LPD_MAX_COMP_MEMORY;
	canvas_base = comp_mem->image_base + comp_mem->image_size;
	comp_mem->lut_base = canvas_base + canvas_size;

	comp_mem->reserved = true;

	panel_info("LPD DRAM is reserved at addr %x total size %x\n", rmem->base, rmem->size);

	panel_info("image base: %x, size: %x, lut base: %x, size: %x, total rmem: %x\n",
		comp_mem->image_base, comp_mem->image_size, comp_mem->lut_base, comp_mem->lut_size, rmem->size);

	return 0;
}

#endif


static int mafpc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct mafpc_device *mafpc;

	mafpc = mafpc_device_create();
	if (!mafpc) {
		panel_err("failed to allocate mafpc device.\n");
		return -ENOMEM;
	}

	mafpc->dev = dev;
	mafpc->miscdev.minor = MISC_DYNAMIC_MINOR;
	mafpc->miscdev.fops = &abc_drv_fops;
	mafpc->miscdev.name = MAFPC_DEV_NAME;
	mafpc->req_instant_on = false;
	mafpc->id = 0;

	platform_set_drvdata(pdev, mafpc);

	ret = mafpc_device_init(mafpc);
	if (ret < 0) {
		panel_err("failed to initialize panel device\n");
		return ret;
	}

	ret = misc_register(&mafpc->miscdev);
	if (ret < 0) {
		panel_err("failed to register mafpc drv\n");
		return ret;
	}

	mafpc->comp_mem.reserved = false;

#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
	ret = parse_lpd_rmem(mafpc, dev);
	if (ret) {
		panel_err("failed to parse lpd mem\n");
		return ret;
	}
#endif

	panel_info("MCD:ABC: probed done\n");

	return 0;
}


static const struct of_device_id mafpc_drv_of_match_table[] = {
	{ .compatible = "samsung,panel-mafpc", },
	{},
};

MODULE_DEVICE_TABLE(of, mafpc_drv_of_match_table);

struct platform_driver mafpc_driver = {
	.probe = mafpc_probe,
	.driver = {
		.name = MAFPC_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mafpc_drv_of_match_table),
	}
};

MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_LICENSE("GPL");
