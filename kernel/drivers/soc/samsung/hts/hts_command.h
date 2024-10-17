/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#define SIGNATURE_ARGUMENT_START        (0x535448)
#define SIGNATURE_ARGUMENT_END          (0x5ACC33A5)

#define IOCTL_MAGIC			'H'
#define IOCTL_MODIFY_TICK		_IO(IOCTL_MAGIC, 0x10)
#define IOCTL_START_TICK		_IO(IOCTL_MAGIC, 0x11)
#define IOCTL_STOP_TICK			_IO(IOCTL_MAGIC, 0x12)

#define IOCTL_CGROUP_ADD		_IO(IOCTL_MAGIC, 0x20)
#define IOCTL_CGROUP_CLEAR		_IO(IOCTL_MAGIC, 0x22)

#define IOCTL_CONFIGURE_EVENT		_IO(IOCTL_MAGIC, 0x30)
#define IOCTL_CLEAR_EVENT		_IO(IOCTL_MAGIC, 0x31)

#define IOCTL_SET_LOG_MASK		_IO(IOCTL_MAGIC, 0x40)
#define IOCTL_SET_REF_MASK		_IO(IOCTL_MAGIC, 0x41)

#define IOCTL_MASK_ADD			_IO(IOCTL_MAGIC, 0x50)
#define IOCTL_MASK_CLEAR		_IO(IOCTL_MAGIC, 0x51)

#define IOCTL_PREDEFINED_SET		_IO(IOCTL_MAGIC, 0x60)
#define IOCTL_PREDEFINED_ADD		_IO(IOCTL_MAGIC, 0x61)
#define IOCTL_PREDEFINED_CLEAR		_IO(IOCTL_MAGIC, 0x62)

#define IOCTL_SET_CORE_THRE		_IO(IOCTL_MAGIC, 0x70)
#define IOCTL_SET_TOTAL_THRE		_IO(IOCTL_MAGIC, 0x71)
