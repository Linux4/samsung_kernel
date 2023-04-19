// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_misc.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/compat.h>
#include <linux/miscdevice.h>
#include <sound/samsung/vts.h>
#include <sound/exynos/sounddev_vts.h>
#include <linux/pm_runtime.h>

#include "vts.h"
#include "vts_misc.h"
#include "vts_dbg.h"

static struct device *vts_misc_vts_dev;
static struct vts_data *p_vts_misc_vts_data;

static int vts_fio_open(struct inode *inode, struct file *file)
{
	file->private_data = p_vts_misc_vts_data;
	return 0;
}

static long vts_fio_common_ioctl(struct file *file,
		unsigned int cmd, int __user *_arg)
{
	struct vts_data *data = (struct vts_data *)file->private_data;
	struct platform_device *pdev;
	struct device *dev;
	int arg;
	struct vts_ipc_msg ipc_msg;
	u32 values[3] = {0, 0, 0};
	struct vts_model_bin_info sm_info;

	if (!data || (((cmd >> 8) & 0xff) != 'V'))
		return -ENOTTY;
	pdev = data->pdev;
	dev = &pdev->dev;

	switch (cmd) {
	case VTSDRV_MISC_IOCTL_LOAD_SOUND_MODEL:
		if (copy_from_user(&sm_info, (struct vts_model_bin_info __user *)_arg,
					sizeof(struct vts_model_bin_info))) {
			vts_dev_err(dev, "%s: LOAD_SOUND_MODEL failed", __func__);
			return -EFAULT;
		}

		if (sm_info.actual_sz < 0 || sm_info.actual_sz > sm_info.max_sz)
			return -EINVAL;
		if (sm_info.actual_sz > VTS_MODEL_BIN_MAXSZ)
			return -EINVAL;
		memcpy(data->sm_data, data->dmab_model.area, sm_info.actual_sz);
		data->sm_loaded = true;
		data->sm_info.actual_sz = sm_info.actual_sz;
		data->sm_info.max_sz = sm_info.max_sz;
		data->sm_info.offset = sm_info.offset;
		vts_dev_info(dev, "%s: LOAD_SOUND_MODEL actual_sz=%d, max_sz=%d, offset=0x%x",
				__func__, data->sm_info.actual_sz, data->sm_info.max_sz, data->sm_info.offset);
		break;
	case VTSDRV_MISC_IOCTL_READ_GOOGLE_VERSION:
		if (get_user(arg, _arg))
			return -EFAULT;

		if (data->google_version == 0) {
			vts_dev_info(dev, "get_sync : Read Google Version");
			pm_runtime_get_sync(dev);
			vts_start_runtime_resume(dev, 0);
			if (pm_runtime_active(dev))
				pm_runtime_put(dev);
		}
		vts_dev_info(dev, "Google Version : %d", data->google_version);
		put_user(data->google_version, (int __user *)_arg);
		break;
	case VTSDRV_MISC_IOCTL_READ_EVENT_TYPE:
		if (get_user(arg, _arg))
			return -EFAULT;

		put_user(data->poll_event_type, (int __user *)_arg);
		data->poll_event_type = EVENT_NONE;
		break;
	case VTSDRV_MISC_IOCTL_WRITE_EXIT_POLLING:
		if (get_user(arg, _arg))
			return -EFAULT;

		data->poll_event_type |= EVENT_STOP_POLLING|EVENT_READY;
		wake_up(&data->poll_wait_queue);
		break;
	case VTSDRV_MISC_IOCTL_SET_PARAM:
		if (copy_from_user(&ipc_msg, (struct vts_ipc_msg __user *)_arg,
					sizeof(struct vts_ipc_msg))) {
			vts_dev_err(dev, "%s: SET_PARAM failed", __func__);
			return -EFAULT;
		}
		values[0] = ipc_msg.values[0];
		values[1] = ipc_msg.values[1];
		values[2] = ipc_msg.values[2];
		vts_dev_info(dev, "%s: SET_PARAM msg: %d, value: 0x%x, 0x%x, 0x%x",
				__func__, ipc_msg.msg, values[0], values[1], values[2]);
		if (ipc_msg.msg < VTS_IRQ_AP_IPC_RECEIVED || ipc_msg.msg > VTS_IRQ_AP_COMMAND) {
			vts_dev_err(dev, "%s: ipc message %d is out of range", __func__, ipc_msg.msg);
			return -EFAULT;
		}

		if (vts_start_ipc_transaction(dev, data, ipc_msg.msg, &values, 0, 1) < 0)
			vts_dev_err(dev, "%s: SET_PARAM ipc transaction failed\n", __func__);

		break;
	case VTSDRV_MISC_IOCTL_READ_GOOGLE_UUID:
		vts_dev_info(dev, "Google UUID: %s", data->google_uuid);
		if (copy_to_user((void __user *)_arg, data->google_uuid, 40))
			vts_dev_err(dev, "%s: READ GOOGLE UUID failed", __func__);

		break;
	default:
		pr_err("VTS unknown ioctl = 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static long vts_fio_ioctl(struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return vts_fio_common_ioctl(file, cmd, (int __user *)_arg);
}

#ifdef CONFIG_COMPAT
static long vts_fio_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return vts_fio_common_ioctl(file, cmd, compat_ptr(_arg));
}
#endif /* CONFIG_COMPAT */

static int vts_fio_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vts_data *data = (struct vts_data *)file->private_data;

	return dma_mmap_wc(&data->pdev->dev, vma, data->dmab_model.area,
			   data->dmab_model.addr, data->dmab_model.bytes);
}

static unsigned int vts_fio_poll(struct file *file, poll_table *wait)
{
	struct vts_data *data = (struct vts_data *)file->private_data;
	int ret = 0;

	poll_wait(file, &data->poll_wait_queue, wait);

	if (data->poll_event_type & EVENT_READY) {
		data->poll_event_type &= ~EVENT_READY;
		ret = POLLIN | POLLRDNORM;
	}

	return ret;
}

static const struct file_operations vts_fio_fops = {
	.owner			= THIS_MODULE,
	.open			= vts_fio_open,
	.release		= NULL,
	.unlocked_ioctl		= vts_fio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= vts_fio_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap			= vts_fio_mmap,
	.poll			= vts_fio_poll,
};

static struct miscdevice vts_fio_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "vts_fio_dev",
	.fops =     &vts_fio_fops
};

int vts_misc_register(
	struct device *dev_vts)
{
	struct vts_data *data = dev_get_drvdata(dev_vts);
	int ret = 0;

	vts_misc_vts_dev = dev_vts;
	p_vts_misc_vts_data = data;

	ret = misc_register(&vts_fio_miscdev);
	if (ret)
		vts_dev_warn(dev_vts, "Failed to create device for sound model download\n");
	else
		vts_dev_info(dev_vts, "misc_register ok");

	return ret;
}
