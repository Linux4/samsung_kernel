/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DECON_BOARD_H__
#define __DECON_BOARD_H__

#include <linux/device.h>

extern unsigned int lcdtype;

void run_list(struct device *dev, const char *name);

#endif

