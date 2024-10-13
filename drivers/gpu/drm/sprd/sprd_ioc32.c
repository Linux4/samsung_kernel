// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <drm/drm_atomic_helper.h>
#include <uapi/drm/sprd_drm_gsp.h>
#include "sprd_drm_gsp.h"

#define DRM_IOCTL32_DEF(n, f)[DRM_##n] = {.fn = f, .name = #n}
#define BLOB_PROP_MAX_SIZE 		(1024 * 1024 * 4)
#define BLOB_PROP_CREATE_CMD 	0xBD
#define IOCTL_W_FLAG 			(BIT(30))

struct drm_gsp_cfg_user32 {
	bool async;
	u32 size;
	u32 num;
	bool split;
	u32 config;
};

struct drm_gsp_capability32 {
	u32 size;
	u32 cap;
};

static int sprd_gsp_trigger_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drm_gsp_cfg_user32 getparam32;
	struct drm_gsp_cfg_user getparam;

	if (copy_from_user(&getparam32, (void __user *)arg, sizeof(getparam32)))
		return -EFAULT;

	getparam.async = getparam32.async;
	getparam.size = getparam32.size;
	getparam.num = getparam32.num;
	getparam.split = getparam32.split;
	getparam.config = compat_ptr(getparam32.config);

	return drm_ioctl_kernel(file, sprd_gsp_trigger_ioctl, &getparam, 0);
}

static int sprd_gsp_get_capability_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drm_gsp_capability32 getparam32;
	struct drm_gsp_capability getparam;

	if (copy_from_user(&getparam32, (void __user *)arg, sizeof(getparam32)))
		return -EFAULT;

	getparam.size = getparam32.size;
	getparam.cap = compat_ptr(getparam32.cap);
	return drm_ioctl_kernel(file, sprd_gsp_get_capability_ioctl, &getparam, 0);
}

static struct {
	drm_ioctl_compat_t *fn;
	char *name;
} sprd_compat_ioctls[] = {
	DRM_IOCTL32_DEF(SPRD_GSP_GET_CAPABILITY, sprd_gsp_get_capability_compat_ioctl),
	DRM_IOCTL32_DEF(SPRD_GSP_TRIGGER, sprd_gsp_trigger_compat_ioctl),
};

long sprd_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	struct drm_file *file_priv = filp->private_data;
	drm_ioctl_compat_t *fn = NULL;
	int ret;
	unsigned int arg_size;
	struct drm_mode_create_blob *out_resp;
	char *kdata = NULL;
	u32 length;

	if ((cmd & IOCTL_W_FLAG) && (nr == BLOB_PROP_CREATE_CMD)) {
		arg_size = _IOC_SIZE(cmd);
		if (!arg_size) {
			DRM_ERROR("invalid arg size");
			return -EFAULT;
		}

		if (arg_size > BLOB_PROP_MAX_SIZE) {
			DRM_ERROR("reject to create prop over max size");
			return -EFAULT;
		}
		kdata = kzalloc((arg_size + 1), GFP_KERNEL);
		if (!kdata) {
			DRM_ERROR("get arg data from user space failed");
			return -EFAULT;
		}

		ret = copy_from_user(kdata, (void __user *)arg, arg_size);
		if (ret) {
			DRM_ERROR("get arg data from user space failed");
			kfree(kdata);
			return ret;
		}

		out_resp = (struct drm_mode_create_blob *)kdata;
		length = out_resp->length;
		if (length > BLOB_PROP_MAX_SIZE) {
			DRM_ERROR("reject to create prop over max size");
			kfree(kdata);
			return -EFAULT;
		}
		kfree(kdata);
	}

	if (nr < DRM_COMMAND_BASE)
		return drm_compat_ioctl(filp, cmd, arg);

	if (nr >= DRM_COMMAND_BASE + ARRAY_SIZE(sprd_compat_ioctls))
		return drm_ioctl(filp, cmd, arg);

	fn = sprd_compat_ioctls[nr - DRM_COMMAND_BASE].fn;
	if (!fn)
		return drm_ioctl(filp, cmd, arg);

	DRM_DEBUG("pid=%d, dev=0x%lx, auth=%d, %s\n", task_pid_nr(current),
			(long)old_encode_dev(file_priv->minor->kdev->devt),
			file_priv->authenticated,
			sprd_compat_ioctls[nr - DRM_COMMAND_BASE].name);
	ret = (*fn) (filp, cmd, arg);
	if (ret)
		DRM_DEBUG("ret = %d\n", ret);
	return ret;
}
