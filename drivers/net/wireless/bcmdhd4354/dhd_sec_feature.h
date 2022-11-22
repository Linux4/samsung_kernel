/*
 * Customer HW 4 dependant file
 *
 * Copyright (C) 1999-2013, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_sec_feature.h$
 */

#ifndef _dhd_sec_feature_h_
#define _dhd_sec_feature_h_

/* PROJECTS */
#if defined(CONFIG_SEC_K_PROJECT)
#undef OOB_INTR_ONLY
#undef HW_OOB
#define SDIO_ISR_THREAD
#endif /* CONFIG_SEC_K_PROJECT */

#if defined(CONFIG_MACH_VIENNA) || defined(CONFIG_MACH_V2)
#define SUPPORT_MULTIPLE_CHIPS

#undef USE_CID_CHECK
#define READ_MACADDR
#endif /* CONFIG_MACH_VIENNA || CONFIG_MACH_V2 */

#if defined(CONFIG_MACH_LT03EUR) || defined(CONFIG_MACH_LT03SKT) ||\
    defined(CONFIG_MACH_LT03KTT) || defined(CONFIG_MACH_LT03LGT)
#undef USE_CID_CHECK
#define READ_MACADDR
#endif	/* CONFIG_MACH_LT03EUR || CONFIG_MACH_LT03SKT || CONFIG_MACH_LT03KTT ||
	 * CONFIG_MACH_LT03LGT
	 */

/* REGION CODE */
#ifndef CONFIG_WLAN_REGION_CODE
#define CONFIG_WLAN_REGION_CODE 100
#endif /* CONFIG_WLAN_REGION_CODE */

#if (CONFIG_WLAN_REGION_CODE >= 100) && (CONFIG_WLAN_REGION_CODE < 200) /* EUR */
#if (CONFIG_WLAN_REGION_CODE == 101) /* EUR ORG */
/* GAN LITE NAT KEEPALIVE FILTER */
#define GAN_LITE_NAT_KEEPALIVE_FILTER
#endif /* CONFIG_WLAN_REGION_CODE == 101 */
#endif /* CONFIG_WLAN_REGION_CODE >= 100 && CONFIG_WLAN_REGION_CODE < 200 */

#if (CONFIG_WLAN_REGION_CODE >= 200) && (CONFIG_WLAN_REGION_CODE < 300) /* KOR */
#undef USE_INITIAL_2G_SCAN
#ifndef ROAM_ENABLE
#define ROAM_ENABLE
#endif /* ROAM_ENABLE */
#ifndef ROAM_API
#define ROAM_API
#endif /* ROAM_API */
#ifndef ROAM_CHANNEL_CACHE
#define ROAM_CHANNEL_CACHE
#endif /* ROAM_CHANNEL_CACHE */
#ifndef OKC_SUPPORT
#define OKC_SUPPORT
#endif /* OKC_SUPPORT */

#ifndef ROAM_AP_ENV_DETECTION
#define ROAM_AP_ENV_DETECTION
#endif /* ROAM_AP_ENV_DETECTION */

#undef WRITE_MACADDR
#ifndef READ_MACADDR
#define READ_MACADDR
#endif /* READ_MACADDR */

#if (CONFIG_WLAN_REGION_CODE == 201) /* SKT */
#ifdef CONFIG_MACH_UNIVERSAL5410
/* Make CPU core clock 300MHz & assign dpc thread workqueue to CPU1 */
#define FIX_CPU_MIN_CLOCK
#endif /* CONFIG_MACH_UNIVERSAL5410 */
#endif /* CONFIG_WLAN_REGION_CODE == 201 */

#if (CONFIG_WLAN_REGION_CODE == 202) /* KTT */
#define VLAN_MODE_OFF
#define CUSTOM_KEEP_ALIVE_SETTING	30000
#define FULL_ROAMING_SCAN_PERIOD_60_SEC

#ifdef CONFIG_MACH_UNIVERSAL5410
/* Make CPU core clock 300MHz & assign dpc thread workqueue to CPU1 */
#define FIX_CPU_MIN_CLOCK
#endif /* CONFIG_MACH_UNIVERSAL5410 */
#endif /* CONFIG_WLAN_REGION_CODE == 202 */

#if (CONFIG_WLAN_REGION_CODE == 203) /* LGT */
#ifdef CONFIG_MACH_UNIVERSAL5410
/* Make CPU core clock 300MHz & assign dpc thread workqueue to CPU1 */
#define FIX_CPU_MIN_CLOCK
#define FIX_BUS_MIN_CLOCK
#endif /* CONFIG_MACH_UNIVERSAL5410 */
#endif /* CONFIG_WLAN_REGION_CODE == 203 */
#endif /* CONFIG_WLAN_REGION_CODE >= 200 && CONFIG_WLAN_REGION_CODE < 300 */

#if (CONFIG_WLAN_REGION_CODE >= 300) && (CONFIG_WLAN_REGION_CODE < 400) /* CHN */
#define BCMWAPI_WPI
#define BCMWAPI_WAI
#endif /* CONFIG_WLAN_REGION_CODE >= 300 && CONFIG_WLAN_REGION_CODE < 400 */

#if (CONFIG_WLAN_REGION_CODE == 402) /* TMO */
#undef CUSTOM_SUSPEND_BCN_LI_DTIM
#define CUSTOM_SUSPEND_BCN_LI_DTIM      3
#endif /* CONFIG_WLAN_REGION_CODE == 402 */

#if !defined(READ_MACADDR) && !defined(WRITE_MACADDR) && !defined(RDWR_KORICS_MACADDR) \
	&& !defined(RDWR_MACADDR)
#define GET_MAC_FROM_OTP
#define SHOW_NVRAM_TYPE
#endif /* !READ_MACADDR && !WRITE_MACADDR && !RDWR_KORICS_MACADDR && !RDWR_MACADDR */

#define WRITE_WLANINFO

#endif /* _dhd_sec_feature_h_ */
