/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_hrvc_worker.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_HRVC_WORKER_H
#define __S5P_MFC_HRVC_WORKER_H __FILE__

#include "s5p_mfc_common.h"
#include <linux/kthread.h>

#define HRVC_CMD	0x0000
#define HRVC_INT	0x0001
#define LIB_CODE_SIZE	0xd3000

int hrvc_load_libfw(void);
int hrvc_load_ldfw(void);

void hrvc_libfw_on(void);
int hrvc_ldfw_on(void);

void hrvc_worker_queue(int mode, int is_drm);

int hrvc_worker_initialize(void);
void hrvc_worker_finalize(void);

#endif /* __S5P_MFC_HRVC_WORKER_H  */
