/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef _SCSC_RELEASE_H
#define _SCSC_RELEASE_H

#ifdef CONFIG_SOC_EXYNOS3830
#define SCSC_RELEASE_SOLUTION "mx152"
#elif defined(CONFIG_SOC_EXYNOS9630)
#define SCSC_RELEASE_SOLUTION "mx450"
#elif defined(CONFIG_SOC_EXYNOS9610)
#define SCSC_RELEASE_SOLUTION "mx250"
#else
#define SCSC_RELEASE_SOLUTION "wlbt"
#endif



#define SCSC_RELEASE_PRODUCT 10
#define SCSC_RELEASE_ITERATION 11
#define SCSC_RELEASE_CANDIDATE 1

#define SCSC_RELEASE_POINT 5

#endif


