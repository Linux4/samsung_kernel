/*
 * extcon-sec-coreprime.h
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

#include <linux/extcon/extcon-sm5504.h>

extern void cutoff_cable_signal(struct extcon_sec *extsec_info);

static u32 sm5504_detect_cable_fn(struct extcon_sec *extsec_info, u32 phy_cable_idx)
{
	u32 idx;

	switch (phy_cable_idx) {
	case SM5504_CABLE_JIG_USB_OFF:
	case SM5504_CABLE_JIG_USB_ON:
		idx = CABLE_JIG_USB;
		break;
	case SM5504_CABLE_JIG_UART_OFF:
		idx = CABLE_JIG_UART;
		cutoff_cable_signal(extsec_info);
		break;
	case SM5504_CABLE_JIG_UART_ON:
		idx = CABLE_JIG_UART;
		cutoff_cable_signal(extsec_info);
		break;
	case SM5504_CABLE_CARKIT_T1:	/* LG USB Cable */
		extsec_info->extsec_set_path_fn("USB");
		extsec_info->manual_mode = true;
	case SM5504_CABLE_USB:
	case SM5504_CABLE_CDP:
		idx = CABLE_USB;
		break;
	case SM5504_CABLE_OTG:
		idx = CABLE_USB_HOST;
		break;
	case SM5504_CABLE_DCP:
	case SM5504_CABLE_OTHER_CHARGER:
		idx = CABLE_TA;
		break;
	case SM5504_CABLE_DESKTOP_DOCK:
		switch_set_state(&extsec_info->dock_dev, true);
		extsec_info->dock_mode = true;
		dev_info(extsec_info->dev, "Report Dock Attach");
		idx = CABLE_DESKTOP_DOCK;
		break;
	case SM5504_CABLE_UNKNOWN_VB:
		idx = CABLE_UNKNOWN_VB;
		break;
	case SM5504_CABLE_UNKNOWN:
	default:
		idx = CABLE_UNKNOWN;
	}

	return idx;
}

static int __init extcon_sec_set_board(struct extcon_sec *extsec_info)
{
	int board_id = get_board_id();
	extsec_info->pdata =
			kzalloc(sizeof(struct extcon_sec_platform_data),
								GFP_KERNEL);
	if (!extsec_info->pdata)
		return -ENOMEM;

	switch (board_id) {
	case 0 :
	default:
		SET_PHY_EXTCON(extsec_info, sm5504);
	}

	return 0;
}
