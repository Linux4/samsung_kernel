/*
 * drivers/gpu/ion/compat_ion.c
 *
 * Copyright (C) 2013 Google, Inc.
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

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/ion.h>
#include <video/ion_sprd.h>
#include "sprd_fence.h"
#include "compat_sprd_ion.h"

/* See kernel/include/video/ion_sprd.h for the definition of these structs */
struct compat_ion_phys_data {
	compat_int_t fd_buffer;
	compat_ulong_t phys;
	compat_size_t size;
};

struct compat_ion_msync_data {
	compat_int_t fd_buffer;
	compat_uptr_t vaddr;
	compat_uptr_t paddr;
	compat_size_t size;
};

struct compat_ion_mmu_data {
	compat_int_t master_id;
	compat_int_t fd_buffer;
	compat_ulong_t iova_addr;
	compat_size_t iova_size;
};

static int compat_get_ion_phys_data(
			struct compat_ion_phys_data __user *data32,
			struct ion_phys_data __user *data)
{
	compat_int_t i;
	int err;

	err = get_user(i, &data32->fd_buffer);
	err |= put_user(i, &data->fd_buffer);

	return err;
};

static int compat_put_ion_phys_data(
			struct compat_ion_phys_data __user *data32,
			struct ion_phys_data __user *data)
{
	compat_ulong_t ul;
	compat_size_t s;
	int err;

	err = get_user(ul, &data->phys);
	err |= put_user(ul, &data32->phys);
	err |= get_user(s, &data->size);
	err |= put_user(s, &data32->size);

	return err;
};

static int compat_get_ion_msync_data(
			struct compat_ion_msync_data __user *data32,
			struct ion_msync_data __user *data)
{
	compat_int_t i;
	compat_uptr_t up;
	compat_size_t s;
	int err;

	err = get_user(i, &data32->fd_buffer);
	err |= put_user(i, &data->fd_buffer);
	err |= get_user(up, &data32->vaddr);
	err |= put_user(up, &data->vaddr);
	err |= get_user(up, &data32->paddr);
	err |= put_user(up, &data->paddr);
	err |= get_user(s, &data32->size);
	err |= put_user(s, &data->size);

	return err;
};

static int compat_get_ion_mmu_data(
			struct compat_ion_mmu_data __user *data32,
			struct ion_mmu_data __user *data)
{
	compat_int_t i;
	compat_ulong_t ul;
	compat_size_t s;
	int err;

	err = get_user(i, &data32->master_id);
	err |= put_user(i, &data->master_id);
	err |= get_user(i, &data32->fd_buffer);
	err |= put_user(i, &data->fd_buffer);
	err |= get_user(ul, &data32->iova_addr);
	err |= put_user(ul, &data->iova_addr);
	err |= get_user(s, &data32->iova_size);
	err |= put_user(s, &data->iova_size);

	return err;
};

static int compat_put_ion_mmu_data(
			struct compat_ion_mmu_data __user *data32,
			struct ion_mmu_data __user *data)
{
	compat_int_t i;
	compat_ulong_t ul;
	compat_size_t s;
	int err;

	err = get_user(i, &data->master_id);
	err |= put_user(i, &data32->master_id);
	err |= get_user(i, &data->fd_buffer);
	err |= put_user(i, &data32->fd_buffer);
	err |= get_user(ul, &data->iova_addr);
	err |= put_user(ul, &data32->iova_addr);
	err |= get_user(s, &data->iova_size);
	err |= put_user(s, &data32->iova_size);

	return err;
};

long compat_sprd_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
#if 0
	struct ion_client *client = filp->private_data;
	struct ion_device *dev = client->dev;

	if (!filp->f_op || !dev->custom_ioctl)
		return -ENOTTY;
#endif
	long ret;

	pr_debug("%s, cmd: %u", __FUNCTION__, cmd);
	switch (cmd) {
	case ION_SPRD_CUSTOM_PHYS:
	{
		struct compat_ion_phys_data __user *data32;
		struct ion_phys_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_ion_phys_data(data32, data);
		if (err)
			return err;
#if 0
		ret = dev->custom_ioctl(client, ION_SPRD_CUSTOM_PHYS,
						(unsigned long)data);
#else
		ret = sprd_ion_ioctl(filp, ION_SPRD_CUSTOM_PHYS, (unsigned long)data);
#endif
		err = compat_put_ion_phys_data(data32, data);
		return ret ? ret : err;
	}
	case ION_SPRD_CUSTOM_MSYNC:
	{
		struct compat_ion_msync_data __user *data32;
		struct ion_msync_data __user *data;
		int err;
	
		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
	
		err = compat_get_ion_msync_data(data32, data);
		if (err)
			return err;
#if 0
		ret = dev->custom_ioctl(client, ION_SPRD_CUSTOM_MSYNC,
						(unsigned long)data);
#else
		ret = sprd_ion_ioctl(filp, ION_SPRD_CUSTOM_MSYNC, (unsigned long)data);
#endif
		return ret;
	}
#if defined(CONFIG_SPRD_IOMMU)
	case ION_SPRD_CUSTOM_MAP:
	{
		struct compat_ion_mmu_data __user *data32;
		struct ion_mmu_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_ion_mmu_data(data32, data);
		if (err)
			return err;
#if 0
		ret = dev->custom_ioctl(client, ION_SPRD_CUSTOM_GSP_MAP,
						(unsigned long)data);
#else
		ret = sprd_ion_ioctl(filp, ION_SPRD_CUSTOM_MAP, (unsigned long)data);
#endif
		err = compat_put_ion_mmu_data(data32, data);
		return ret ? ret : err;
	}
	case ION_SPRD_CUSTOM_UNMAP:
	{
		struct compat_ion_mmu_data __user *data32;
		struct ion_mmu_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_ion_mmu_data(data32, data);
		if (err)
			return err;
#if 0
		ret = dev->custom_ioctl(client, ION_SPRD_CUSTOM_GSP_UNMAP,
						(unsigned long)data);
#else
		ret = sprd_ion_ioctl(filp, ION_SPRD_CUSTOM_UNMAP, (unsigned long)data);
#endif
		err = compat_put_ion_mmu_data(data32, data);
		return ret ? ret : err;
	}
#endif
	case ION_SPRD_CUSTOM_FENCE_CREATE:
	{
		int ret = -1;
		struct ion_fence_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}

		ret = sprd_fence_build(&data);
		if (ret != 0) {
			pr_err("sprd_fence_build failed\n");
			return -EFAULT;
		}

		if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
			sprd_fence_destroy(&data);
			pr_err("copy_to_user fence failed\n");
			return -EFAULT;
		}

		break;
    }
	case ION_SPRD_CUSTOM_FENCE_SIGNAL:
	{
		struct ion_fence_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}

		sprd_fence_signal(&data);

		break;
	}
	case ION_SPRD_CUSTOM_FENCE_DUP:
	{
		break;

	}
	default:
		return -ENOIOCTLCMD;
	}
}
