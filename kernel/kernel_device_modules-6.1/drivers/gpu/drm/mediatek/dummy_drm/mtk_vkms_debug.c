// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_vkms_drv.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <mt-plat/mrdump.h>
#endif

static bool is_buffer_init;
static char *debug_buffer;
#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *mtkfb_dbgfs;
#endif
static struct drm_device *drm_dev;
int dummy_bitmap_enable = 1;

static void process_dbg_opt(const char *opt)
{
	DDPINFO("display_debug cmd %s\n", opt);

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPPR_ERR("%s, invalid drm dev,opt:%s\n", __func__, opt);
		return;
	}

	if (strncmp(opt, "helper", 6) == 0) {

	} else if (strncmp(opt, "dummy_bitmap:", 7) == 0) {
		if (strncmp(opt + 13, "1", 1) == 0)
			dummy_bitmap_enable = 1;
		else
			dummy_bitmap_enable = 0;
	}
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DDPINFO("[mtkfb_dbg] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}
static int debug_get_info(unsigned char *stringbuf, int buf_len)
{
	int n = 0;
	struct mtk_drm_private *private;

	if (IS_ERR_OR_NULL(drm_dev)) {
		DDPPR_ERR("%s:%d, drm_dev is NULL\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	if (IS_ERR_OR_NULL(drm_dev->dev_private)) {
		DDPPR_ERR("%s:%d, drm_dev->dev_private is NULL\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	private = drm_dev->dev_private;

	stringbuf[n++] = 0;
	return n;
}

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	int debug_bufmax;
	static int n;

	if (*ppos != 0 || !is_buffer_init)
		goto out;

	if (!debug_buffer) {
		debug_buffer = vmalloc(sizeof(char) * DEBUG_BUFFER_SIZE);
		if (!debug_buffer)
			return -ENOMEM;

		memset(debug_buffer, 0, sizeof(char) * DEBUG_BUFFER_SIZE);
	}

	debug_bufmax = DEBUG_BUFFER_SIZE - 1;
	n = debug_get_info(debug_buffer, debug_bufmax);

out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512];

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;

	process_dbg_cmd(cmd_buffer);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read, .write = debug_write, .open = debug_open,
};

void disp_dbg_probe(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)

	mtkfb_dbgfs = debugfs_create_file("mtkfb", S_IFREG | 0440, NULL,
					  NULL, &debug_fops);
	if (!mtkfb_dbgfs) {
		DDPPR_ERR("[%s %d]DEBUG failed to create mtkfb in /proc/disp_ddp\n",
			__func__, __LINE__);
		goto out;
	}
	drm_mmp_init();
#endif
out:
	return;

}
void disp_dbg_init(struct drm_device *dev)
{
	if (IS_ERR_OR_NULL(dev))
		DDPMSG("%s, disp debug init with invalid dev\n", __func__);
	else
		DDPMSG("%s, disp debug init\n", __func__);
	drm_dev = dev;
}

void disp_dbg_deinit(void)
{
	if (debug_buffer)
		vfree(debug_buffer);
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(mtkfb_dbgfs);
#endif
}

