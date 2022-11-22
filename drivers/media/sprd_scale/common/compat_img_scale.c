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
#include <video/sprd_scale_k.h>
#include "img_scale.h"
#include "compat_img_scale.h"

struct compat_scale_frame_param_t {
	struct scale_size_t input_size;
	struct scale_rect_t input_rect;
	enum scale_fmt_e input_format;
	struct scale_addr_t input_addr;
	struct scale_endian_sel_t input_endian;
	struct scale_size_t output_size;
	enum scale_fmt_e output_format;
	struct scale_addr_t output_addr;
	struct scale_endian_sel_t output_endian;
	enum scale_mode_e scale_mode;
	uint32_t slice_height;
	compat_caddr_t coeff_addr;
};

#define COMPAT_SCALE_IO_START     _IOW(SCALE_IO_MAGIC, 0, struct compat_scale_frame_param_t)

static long compat_get_scale_frame_param(
			struct scale_frame_param_t *kp,
			struct compat_scale_frame_param_t __user *up)
{
	compat_caddr_t tmp;

	if (!access_ok(VERIFY_READ, up, sizeof(struct compat_scale_frame_param_t)) ||
		copy_from_user(&kp->input_size, &up->input_size, sizeof(struct scale_size_t)) ||
		copy_from_user(&kp->input_rect, &up->input_rect, sizeof(struct scale_rect_t)) ||
		get_user(kp->input_format, &up->input_format) ||
		copy_from_user(&kp->input_addr, &up->input_addr, sizeof(struct scale_addr_t)) ||
		copy_from_user(&kp->input_endian, &up->input_endian, sizeof(struct scale_endian_sel_t)) ||
		copy_from_user(&kp->output_size, &up->output_size, sizeof(struct scale_size_t)) ||
		get_user(kp->output_format, &up->output_format) ||
		copy_from_user(&kp->output_addr, &up->output_addr, sizeof(struct scale_addr_t)) ||
		copy_from_user(&kp->output_endian, &up->output_endian, sizeof(struct scale_endian_sel_t)) ||
		get_user(kp->scale_mode, &up->scale_mode) ||
		get_user(kp->slice_height, &up->slice_height) ||
		get_user(tmp, &up->coeff_addr))
			return -EFAULT;
	kp->coeff_addr = compat_ptr(tmp);
	return 0;
}

long compat_scale_k_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct scale_frame_param_t karg;
	void __user *up = compat_ptr(arg);
	int compatible_arg = 1;
	long err = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_SCALE_IO_START:
		err = compat_get_scale_frame_param(&karg, up);
		cmd = SCALE_IO_START;
		compatible_arg = 0;
		break;

	default:
		break;
	}
	if (err)
		return err;

	if (compatible_arg)
		err = filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)up);
	else {
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);
		err = filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)&karg);
		set_fs(old_fs);
	}
	return err;
}
