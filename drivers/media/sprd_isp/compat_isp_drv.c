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

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <video/sprd_isp.h>
#include "compat_isp_drv.h"


struct compat_isp_capability {
	uint32_t isp_id;
	uint32_t index;
	compat_uptr_t property_param;
};

struct compat_isp_io_param {
	uint32_t isp_id;
	uint32_t sub_block;
	uint32_t property;
	compat_uptr_t property_param;
};

struct compat_isp_reg_param {
	compat_ulong_t reg_param;
	uint32_t counts;
};

struct compat_isp_reg_bits {
	compat_ulong_t reg_addr;
	compat_ulong_t reg_value;
};


#define COMPAT_ISP_IO_CAPABILITY     _IOR(ISP_IO_MAGIC, 0, struct compat_isp_capability)
#define COMPAT_ISP_IO_READ           _IOR(ISP_IO_MAGIC, 2, struct compat_isp_reg_param)
#define COMPAT_ISP_IO_WRITE          _IOW(ISP_IO_MAGIC, 3, struct compat_isp_reg_param)
#define CAMPAT_ISP_IO_CFG_PARAM      _IOWR(ISP_IO_MAGIC, 7, struct compat_isp_io_param)
#define CAMPAT_ISP_REG_READ     	 _IOR(ISP_IO_MAGIC, 8, struct compat_isp_reg_bits)


static int compat_get_capability_param(
			struct compat_isp_capability __user *data32,
			struct isp_capability __user *data)
{
	int err = 0;
	uint32_t tmp;
	compat_uptr_t parm;

	err = get_user(tmp, &data32->isp_id);
	err |= put_user(tmp, &data->isp_id);
	err |= get_user(tmp, &data32->index);
	err |= put_user(tmp, &data->index);
	err |= get_user(parm, &data32->property_param);
	err |= put_user(parm, &data->property_param);

	return err;
}

static int compat_get_cfg_param(
			struct compat_isp_io_param __user *data32,
			struct isp_io_param __user *data)
{
	int err  =0;
	uint32_t tmp;
	compat_uptr_t parm;

	err = get_user(tmp, &data32->isp_id);
	err |= put_user(tmp, &data->isp_id);
	err |= get_user(tmp, &data32->sub_block);
	err |= put_user(tmp, &data->sub_block);
	err |= get_user(tmp, &data32->property);
	err |= put_user(tmp, &data->property);
	err |= get_user(parm, &data32->property_param);
	err |= put_user(parm, &data->property_param);

	return err;
}

static int compat_get_rw_param(
			struct compat_isp_reg_param __user *data32,
			struct isp_reg_param __user *data)
{
	int err = 0;
	compat_ulong_t addr;
	uint32_t cnt;

	err = get_user(addr, &data32->reg_param);
	err |= put_user(addr, &data->reg_param);
	err |= get_user(cnt, &data32->counts);
	err |= put_user(cnt, &data->counts);

	return err;
}

static int compat_get_rw_param_detail(
			struct isp_reg_param __user *data)

{
	int err = 0;
	int i;
	int cnt;
	compat_ulong_t addr;
	compat_ulong_t val;
	struct compat_isp_reg_bits __user *data32;
	struct isp_reg_bits __user *reg_data;
	struct isp_reg_bits __user *data_tmp;

	if (NULL == data) {
		printk("compat_get_rw_param_detail: data is NULL.\n");
		return -EFAULT;
	}

	cnt = data->counts;
	data32 = compat_ptr(data->reg_param);
	reg_data = (struct isp_reg_bits __user *)((unsigned long)data + sizeof(struct isp_reg_param));
	data->reg_param = reg_data;
	if (NULL == reg_data) {
		printk("compat_get_rw_param_detail: reg_data is NULL.\n");
		return -EFAULT;
	}

	data_tmp = reg_data;
	for (i = 0; i < cnt; i++) {
		err = get_user(addr, &data32->reg_addr);
		err |= put_user(addr, &data_tmp->reg_addr);
		err |= get_user(val, &data32->reg_value);
		err |= put_user(val, &data_tmp->reg_value);
		if (err) {
			printk("compat_get_rw_param_detail: failed to get register addr&val cnt %d err %d\n", cnt, err);
			break;
		}
		data32++;
		data_tmp++;
	}

	return err;
}

static int compat_put_read_param(
			struct compat_isp_reg_param __user *data32,
			struct isp_reg_param __user *data)

{
	int err = 0;
	int i;
	int cnt;
	compat_ulong_t val;
	struct compat_isp_reg_bits __user *reg_data32;
	struct isp_reg_bits __user *reg_data;

	cnt = data->counts;
	reg_data32 = (struct compat_isp_reg_bits __user *)data32->reg_param;
	reg_data = (struct isp_reg_bits __user *)data->reg_param;
	for (i = 0; i < cnt; i++) {
		err = get_user(val, &reg_data->reg_value);
		err |= put_user(val, &reg_data32->reg_value);
		if (err) {
			printk("compat_put_read_param: failed to get register val err %d\n", err);
			break;
		}
		reg_data32++;
		reg_data++;
	}

	return err;
}

long compat_isp_ioctl(struct file *file, unsigned int cmd, unsigned long param)
{
	long ret;
	int err = 0;
	void __user *up = compat_ptr(param);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case ISP_IO_IRQ:
	case ISP_IO_RST:
	case ISP_IO_STOP:
	case ISP_IO_INT:
		ret = file->f_op->unlocked_ioctl(file, cmd, (unsigned long)up);
		break;

	case COMPAT_ISP_IO_CAPABILITY:
	{
		struct compat_isp_capability __user *data32;
		struct isp_capability __user *data;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data) {
			printk("compat_isp_ioctl: failed to compat_alloc_user_space.\n");
			return -EFAULT;
		}

		err = compat_get_capability_param(data32, data);
		if (err) {
			printk("compat_isp_ioctl: failed to get capability param.\n");
			return err;
		}

		ret = file->f_op->unlocked_ioctl(file, ISP_IO_CAPABILITY, (unsigned long)data);
		break;
	}

	case COMPAT_ISP_IO_READ:
	{
		struct compat_isp_reg_param __user *data32;
		struct isp_reg_param __user *data;
		uint32_t cnt;

		data32 = compat_ptr(param);
		err = get_user(cnt, &data32->counts);

		data = compat_alloc_user_space(sizeof(struct isp_reg_param) + cnt*sizeof(struct isp_reg_bits));
		if (NULL == data) {
			printk("compat_isp_ioctl: failed to compat_alloc_user_space.\n");
			return -EFAULT;
		}

		err = compat_get_rw_param(data32, data);
		if (err) {
			printk("compat_isp_ioctl: failed to get isp_reg_param.\n");
			return err;
		}

		err = compat_get_rw_param_detail(data);
		if (err) {
			printk("compat_isp_ioctl: failed to get isp_reg_param detail.\n");
			return err;
		}

		ret = file->f_op->unlocked_ioctl(file, ISP_IO_READ, (unsigned long)data);
		err = compat_put_read_param(data32, data);
		ret = ret ? ret : err;
		break;
	}

	case CAMPAT_ISP_REG_READ:
	{
		struct compat_isp_reg_bits __user *data32;
		struct isp_reg_bits __user *data;
		unsigned long reg_cnt = 20467;
		compat_ulong_t addr;
		compat_ulong_t val;
		int i;
		int err = 0;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(reg_cnt * sizeof(struct isp_reg_bits));
		if (NULL == data) {
			printk("compat_isp_ioctl: CAMPAT_ISP_REG_READ:failed to compat_alloc_user_space.\n");
			return -EFAULT;
		}

		ret = file->f_op->unlocked_ioctl(file, ISP_REG_READ, (unsigned long)data);
		for (i = 0; i < reg_cnt; i++) {
			err = get_user(val, &data->reg_value);
			err |= put_user(val, &data32->reg_value);
			if (err) {
				printk("compat_put_read_param: CAMPAT_ISP_REG_READ:failed to get reg_value%d\n", err);
				break;
			}

			err = get_user(val, &data->reg_addr);
			err |= put_user(val, &data32->reg_addr);
			if (err) {
				printk("compat_put_read_param: CAMPAT_ISP_REG_READ:failed to get reg_addr%d\n", err);
				break;
			}

			data32++;
			data++;
		}
		ret = ret ? ret : err;
		break;
	}

	case COMPAT_ISP_IO_WRITE:
	{
		struct compat_isp_reg_param __user *data32;
		struct isp_reg_param __user *data;
		uint32_t cnt;

		data32 = compat_ptr(param);
		err = get_user(cnt, &data32->counts);
		if (err) {
			printk("compat_isp_ioctl: failed to get register count.\n");
			return err;
		}

		data = compat_alloc_user_space(sizeof(struct isp_reg_param) + cnt*sizeof(struct isp_reg_bits));
		if (NULL == data) {
			printk("compat_isp_ioctl: failed to compat_alloc_user_space.\n");
			return -EFAULT;
		}

		err = compat_get_rw_param(data32, data);
		if (err) {
			printk("compat_isp_ioctl: failed to get isp_reg_param.\n");
			return err;
		}

		err = compat_get_rw_param_detail(data);
		if (err) {
			printk("compat_isp_ioctl: failed to get isp_reg_param detail.\n");
			return err;
		}

		ret = file->f_op->unlocked_ioctl(file, ISP_IO_WRITE, (unsigned long)data);
		break;
	}

	case CAMPAT_ISP_IO_CFG_PARAM:
	{
		struct compat_isp_io_param __user *data32;
		struct isp_io_param __user *data;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data) {
			printk("compat_isp_ioctl: failed to compat_alloc_user_space.\n");
			return -EFAULT;
		}

		err = compat_get_cfg_param(data32, data);
		if (err) {
			printk("compat_isp_ioctl: failed to get cfg param.\n");
			return err;
		}

		ret = file->f_op->unlocked_ioctl(file, ISP_IO_CFG_PARAM, (unsigned long)data);
		break;
	}

	default:
		printk("compat_isp_ioctl, don't support cmd 0x%x", cmd);
		break;
	}

exit:
	return ret;
}

