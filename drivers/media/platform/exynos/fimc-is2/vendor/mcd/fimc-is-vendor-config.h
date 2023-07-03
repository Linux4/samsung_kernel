/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_VENDOR_CONFIG_H
#define FIMC_IS_VENDOR_CONFIG_H

#if defined(CONFIG_CAMERA_ON5X)
#include "fimc-is-vendor-config_on5x.h"
#elif defined(CONFIG_CAMERA_J3POP)
#include "fimc-is-vendor-config_j3pop.h"
#elif defined(CONFIG_CAMERA_GTESVELTE)
#include "fimc-is-vendor-config_gtesvelte.h"
#elif defined(CONFIG_CAMERA_XCOVER4)
#include "fimc-is-vendor-config_xcover4.h"
#elif defined(CONFIG_CAMERA_J3Y17)
#include "fimc-is-vendor-config_j3y17.h"
#elif defined(CONFIG_CAMERA_ON5XREFLTE)
#include "fimc-is-vendor-config_on5xreflte.h"
#elif defined(CONFIG_CAMERA_J2CORE)
#include "fimc-is-vendor-config_j2corelte.h"
#elif defined(CONFIG_CAMERA_J3TOP)
#include "fimc-is-vendor-config_j3top.h"
#elif defined(CONFIG_CAMERA_GTAXSWIFI)
#include "fimc-is-vendor-config_gtaxswifi.h"
#else
#include "fimc-is-vendor-config_javalte.h" /* Default */
#endif

#endif
