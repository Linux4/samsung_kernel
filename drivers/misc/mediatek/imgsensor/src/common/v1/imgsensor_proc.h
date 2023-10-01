/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_PROC_H__
#define __IMGSENSOR_PROC_H__

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "imgsensor_common.h"

#define PROC_CAMERA_INFO "driver/camera_info"
#define PROC_SENSOR_STAT "driver/imgsensor_status_info"

#define IMGSENSOR_STATUS_INFO_LENGTH 128
#define camera_info_size 4096

/**[AX3565] lisizhou add for cam info 2020/12/04 start **/
#define CAM_MODULE_INFO_CONFIG 1
/**[AX3565] lisizhou add for cam info 2020/12/04 end **/

extern char mtk_ccm_name[camera_info_size];
extern struct IMGSENSOR *pgimgsensor;

enum IMGSENSOR_RETURN imgsensor_proc_init(void);
#endif

