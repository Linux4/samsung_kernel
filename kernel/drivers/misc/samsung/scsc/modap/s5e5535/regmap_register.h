/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "mif_reg.h"

const u32 pmureg_register_offset[] = {
	WLBT_STAT,
	WLBT_DEBUG,
	WLBT_CONFIGURATION,
	WLBT_STATUS,
	WLBT_STATES,
	WLBT_OPTION,
	WLBT_CTRL_NS,
	WLBT_CTRL_S,
	WLBT_OUT,
	WLBT_IN,
	WLBT_INT_IN,
	WLBT_INT_EN,
	WLBT_INT_TYPE,
	WLBT_INT_DIR,
	SYSTEM_OUT,
	VGPIO_TX_MONITOR2,
};

const char *pmureg_register_name[] = {
	"WLBT_STAT",
	"WLBT_DEBUG",
	"WLBT_CONFIGURATION",
	"WLBT_STATUS",
	"WLBT_STATES",
	"WLBT_OPTION",
	"WLBT_CTRL_NS",
	"WLBT_CTRL_S",
	"WLBT_OUT",
	"WLBT_IN",
	"WLBT_INT_IN",
	"WLBT_INT_EN",
	"WLBT_INT_TYPE",
	"WLBT_INT_DIR",
	"SYSTEM_OUT",
	"VGPIO_TX_MONITOR2",
};
const u32 boot_cfg_register_offset[] = {
};

const char *boot_cfg_register_name[] = {
};

