/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains umac mlme related CFG/INI Items.
 */

#ifndef __CFG_CMN_MLME_H
#define __CFG_CMN_MLME_H

/*
 * <ini>
 * max_chan_switch_ie_enable - Flag to enable max chan switch IE support
 * @Min: false
 * @Max: true
 * @Default: false
 *
 * For non_ap platform, this flag will be enabled at later point and for ap
 * platform this flag will be disabled
 *
 * Related: None
 *
 * Supported Feature: Max channel switch IE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MLME_MAX_CHAN_SWITCH_IE_ENABLE \
	CFG_INI_BOOL("max_chan_switch_ie_enable", \
	PLATFORM_VALUE(false, false), \
	"To enable max channel switch IE")

/*
 * <ini>
 * enable_11be - Ini to enable/disable 11BE feature capability
 * @Min: false
 * @Max: true
 * @Default: true
 *
 * This ini is used to enable/disable 11be feature capability.
 * 0 - Disable 11be feature
 * 1 - Enable 11be feature
 *
 * Related: None
 *
 * Supported Feature: 11be
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_11BE CFG_INI_UINT( \
	"enable_11be",\
	0, \
	1, \
	1, \
	CFG_VALUE_OR_DEFAULT, \
	"11be enable/disable support")

#ifdef WLAN_FEATURE_11BE
/*
 * <ini>
 * non_mlo_11be_ap_operation_enable - Flag to enable non MLO 802.11be AP
 * operation
 * @Min: false
 * @Max: true
 * @Default: false
 *
 * The 802.11be standard does not allow non-MLO 11be AP operation. For
 * development purposes, add an INI flag to enable/disable non-MLO 802.11be AP
 * operation. This INI will be disabled by default.
 *
 * Related: None
 *
 * Supported Feature: 802.11be protocol
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MLME_NON_MLO_11BE_AP_OPERATION_ENABLE \
	CFG_INI_BOOL("non_mlo_11be_ap_operation_enable", \
		     false, \
		     "Enable non MLO 11be AP operation")

#define CFG_MLME_11BE_ALL \
	CFG(CFG_MLME_NON_MLO_11BE_AP_OPERATION_ENABLE)
#else
#define CFG_MLME_11BE_ALL
#endif /* WLAN_FEATURE_11BE */

/*
 * <ini>
 * mlme_mlo_reconfig_reassoc_enable- Flag for non-AP MLD re-association
 * upon detecting ML Reconfig AP addition
 * @Min: false
 * @Max: true
 * @Default: false
 *
 * This flag when set to true enables re-association by non-AP MLD when
 * the non-AP MLD detects that the AP MLD it is associated with has
 * added a new AP using ML Reconfiguration.
 *
 * Related: None
 *
 * Supported Feature: 11be MLO Reconfig
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MLME_MLO_RECONFIG_REASSOC_ENABLE CFG_INI_UINT( \
	"mlme_mlo_reconfig_reassoc_enable",\
	0, 1, 0, \
	CFG_VALUE_OR_DEFAULT, \
	"MLO reconfig reassoc is supported by target")

#define CFG_CMN_MLME_ALL \
	CFG(CFG_MLME_MAX_CHAN_SWITCH_IE_ENABLE) \
	CFG(CFG_ENABLE_11BE) \
	CFG(CFG_MLME_MLO_RECONFIG_REASSOC_ENABLE) \
	CFG_MLME_11BE_ALL

#endif /* __CFG_CMN_MLME_H */
