/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __PLATFORM_MIF_QOS_API_H__
#define __PLATFORM_MIF_QOS_API_H__
#include "platform_mif.h"
#if IS_ENABLED(CONFIG_SCSC_QOS)
int platform_mif_qos_init(struct platform_mif *platform);
#else
void platform_mif_qos_init(struct platform_mif *platform);
#endif
#endif //__PLATFORM_MIF_QOS_API_H__