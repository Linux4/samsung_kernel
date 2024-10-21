/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#ifndef CLKBUF_COMMON_H
#define CLKBUF_COMMON_H

#include <linux/compiler_types.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#define EXTRACT_REG_VAL(val, mask, shift) (((val) >> (shift)) & (mask))

#define DEFINE_ATTR_RO(_name)                                                  \
	static struct kobj_attribute _name##_attr = {                          \
		.attr =                                                        \
			{                                                      \
				.name = #_name,                                \
				.mode = 0444,                                  \
			},                                                     \
		.show = _name##_show,                                          \
	}

#define DEFINE_ATTR_RW(_name)                                                  \
	static struct kobj_attribute _name##_attr = {                          \
		.attr =                                                        \
			{                                                      \
				.name = #_name,                                \
				.mode = 0644,                                  \
			},                                                     \
		.show = _name##_show,                                          \
		.store = _name##_store,                                        \
	}

#define DEFINE_ATTR_WO(_name)                                                  \
	static struct kobj_attribute _name##_attr = {                          \
		.attr =                                                        \
			{                                                      \
				.name = #_name,                                \
				.mode = 0222,                                  \
			},                                                     \
		.store = _name##_store,                                        \
	}

#define __ATTR_OF(_name) (&_name##_attr.attr)

#endif /* CLKBUF_COMMON_H */
