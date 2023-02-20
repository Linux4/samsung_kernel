/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2017, 2019 The Linux Foundation. All rights reserved. */

#ifndef _CNSS_UTILS_H_
#define _CNSS_UTILS_H_

#include <linux/types.h>

/* feature macro to indicate the cnss_utils_get_wlan_unsafe_channel_sap
 * supported in cnss_utils
 */
#define CNSS_UTILS_VENDOR_UNSAFE_CHAN_API_SUPPORT

struct device;

enum cnss_utils_cc_src {
	CNSS_UTILS_SOURCE_CORE,
	CNSS_UTILS_SOURCE_11D,
	CNSS_UTILS_SOURCE_USER
};

#define CNSS_CH_AVOID_MAX_RANGE   4

/**
* struct cnss_ch_avoid_freq_type
* @start_freq: start freq (MHz)
* @end_freq: end freq (Mhz)
*/
struct cnss_ch_avoid_freq_type {
                uint32_t start_freq;
                uint32_t end_freq;
};

/**
* struct cnss_ch_avoid_ind_type
* @ch_avoid_range_cnt: count
* @avoid_freq_range: avoid freq range array
*/
struct cnss_ch_avoid_ind_type {
                uint32_t ch_avoid_range_cnt;
                struct cnss_ch_avoid_freq_type avoid_freq_range[CNSS_CH_AVOID_MAX_RANGE];
};

/**
* cnss_utils_get_wlan_unsafe_channel_sap() - Get vendor unsafe ch freq ranges
* @dev: device
* @ch_avoid_ranges: unsafe freq channel ranges
*  
* This APi returns vendor specific unsafe channel frequency ranges. Wlan SAP
* will avoid select the unsafe channels in the range. MSM platform will be 
* no-op. On Non MSM platform, customer can implement this API based on their 
* private interface with modem and return SAP avoid frequency range based on 
* modem state.
*
* Return: 0 for success
*         Non zero failure code for errors
*/

extern int cnss_utils_get_wlan_unsafe_channel_sap(struct device *dev,  
	struct cnss_ch_avoid_ind_type *ch_avoid_ranges);

extern int cnss_utils_set_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 ch_count);
extern int cnss_utils_get_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 *ch_count, u16 buf_len);
extern int cnss_utils_wlan_set_dfs_nol(struct device *dev,
				       const void *info, u16 info_len);
extern int cnss_utils_wlan_get_dfs_nol(struct device *dev,
				       void *info, u16 info_len);
extern int cnss_utils_get_driver_load_cnt(struct device *dev);
extern void cnss_utils_increment_driver_load_cnt(struct device *dev);
extern int cnss_utils_set_wlan_mac_address(const u8 *in, uint32_t len);
extern u8 *cnss_utils_get_wlan_mac_address(struct device *dev, uint32_t *num);
extern int cnss_utils_set_wlan_derived_mac_address(const u8 *in, uint32_t len);
extern u8 *cnss_utils_get_wlan_derived_mac_address(struct device *dev,
							uint32_t *num);
extern void cnss_utils_set_cc_source(struct device *dev,
				     enum cnss_utils_cc_src cc_source);
extern enum cnss_utils_cc_src cnss_utils_get_cc_source(struct device *dev);

#endif
