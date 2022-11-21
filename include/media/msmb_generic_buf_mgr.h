/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MEDIA_MSMB_GENERIC_BUF_MGR_H__
#define __MEDIA_MSMB_GENERIC_BUF_MGR_H__

#include <uapi/media/msmb_generic_buf_mgr.h>
#include <linux/compat.h>

struct v4l2_subdev *msm_buf_mngr_get_subdev(void);

#ifdef CONFIG_COMPAT


#define VIDIOC_MSM_BUF_MNGR_GET_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 33, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_PUT_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 34, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_BUF_DONE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 35, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_FLUSH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 39, struct msm_buf_mngr_info32_t)

#endif

#endif

