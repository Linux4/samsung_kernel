/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */


#define CONFIG_SPECTRA_CAMERA 1
#define CONFIG_SPECTRA_ISP    1
#define CONFIG_SPECTRA_SENSOR 1
#define CONFIG_SPECTRA_ICP    1
#define CONFIG_SPECTRA_JPEG   1
#define CONFIG_SAMSUNG_SBI   1

#if defined(CONFIG_SEC_O1Q_PROJECT) || defined(CONFIG_SEC_T2Q_PROJECT)\
	|| defined(CONFIG_SEC_Q2Q_PROJECT) || defined(CONFIG_SEC_R9Q_PROJECT) || defined(CONFIG_SEC_V2Q_PROJECT)
#define CONFIG_SAMSUNG_SBI_QOS_TUNE
#endif
