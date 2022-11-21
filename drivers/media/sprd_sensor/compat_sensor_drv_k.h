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
#ifndef _COMPAT_IMG_SCALE_H_
#define _COMPAT_IMG_SCALE_H_

#if IS_ENABLED(CONFIG_COMPAT)

long compat_sensor_k_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#else

#define compat_sensor_k_ioctl  NULL

#endif /* CONFIG_COMPAT */
#endif
