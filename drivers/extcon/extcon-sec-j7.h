/*
 * extcon-sec-j7.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 * Author: Seonggyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/*
 * Do not include this file directly
 */

#include <linux/extcon/extcon-s2mm001b.h>

extern void cutoff_cable_signal(struct extcon_sec *extsec_info);

static u32 s2mm001b_detect_cable_fn(struct extcon_sec *extsec_info, u32 phy_cable_idx)
{
	u32 idx;

	switch (phy_cable_idx) {
	case S2MM001B_CABLE_JIG_USB_OFF:
	case S2MM001B_CABLE_JIG_USB_ON:
		idx = CABLE_JIG_USB;
		break;
	case S2MM001B_CABLE_JIG_UART_OFF:
		idx = CABLE_JIG_UART;
		cutoff_cable_signal(extsec_info);
		break;
	case S2MM001B_CABLE_JIG_UART_ON:
		idx = CABLE_JIG_UART;
		cutoff_cable_signal(extsec_info);
		break;
	case S2MM001B_CABLE_CARKIT_T2:		/* LG Cable */
		extsec_info->extsec_set_path_fn("USB");
		extsec_info->manual_mode = true;
	case S2MM001B_CABLE_USB:
	case S2MM001B_CABLE_CDP:
		idx = CABLE_USB;
		break;
	case S2MM001B_CABLE_PPD:
		idx = CABLE_PPD;
		break;
	case S2MM001B_CABLE_OTG:
		extsec_info->extsec_set_path_fn("OTG");
		extsec_info->manual_mode = true;
		idx = CABLE_USB_HOST;
		break;
	case S2MM001B_CABLE_DCP:
	case S2MM001B_CABLE_APPLE_CHG:
	case S2MM001B_CABLE_U200:
		idx = CABLE_TA;
		break;
	case S2MM001B_CABLE_UNKNOWN_VB:
		idx = CABLE_UNKNOWN_VB;
		break;
	default:
		idx = CABLE_UNKNOWN;
	}

	return idx;
}

static __init int extcon_sec_set_board(struct extcon_sec *extsec_info)
{
	extsec_info->pdata =
			kzalloc(sizeof(struct extcon_sec_platform_data),
								GFP_KERNEL);
	if (!extsec_info->pdata)
		return -ENOMEM;

	switch (get_board_id()) {
	case 0:
	default:
		SET_PHY_EXTCON(extsec_info, s2mm001b);
	}

	return 0;
}
