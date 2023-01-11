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

#ifdef CONFIG_SOC_CAMERA_SR200
#define SR200_CCIC_PORT 1
extern struct soc_camera_desc soc_camera_desc_0;
#endif

#endif
