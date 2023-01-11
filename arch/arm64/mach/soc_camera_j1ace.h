/*
 *  Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */
#ifndef __SOC_CAMERA_DKB_H__
#define __SOC_CAMERA_DKB_H__

#include <media/soc_camera.h>
#include <media/mrvl-camera.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_SOC_CAMERA_DB8221A
#define DB8221A_CCIC_PORT 1
extern struct soc_camera_desc soc_camera_desc_1;
#endif

#ifdef CONFIG_SOC_CAMERA_S5K4ECGX
#define S5K4ECGX_CCIC_PORT 0
extern struct soc_camera_desc soc_camera_desc_0;
#endif

#endif
