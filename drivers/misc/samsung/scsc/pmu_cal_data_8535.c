/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
/*pmucal guide version : v18.5*/
#include "pmu_cal.h"

struct pmucal_data WLBT_INIT_SEQ[] = {
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CTRL_S, 3, 0x1 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_OPTION, 3, 0x1 },
	{ WLBT_PMUCAL_DELAY, 1, 0, 0, 0, 0x3 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, V_PWREN, 31, 0x12 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, V_PWREN, 31, 0x13 },
	{ WLBT_PMUCAL_DELAY, 1, 0, 0, 0, 0x3E8 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CONFIGURATION, 0, 0x1 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_STATUS, 0, 0x1 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_IN, 4, 0x1 },
	{ WLBT_PMUCAL_CLEAR, 1, 1, WLBT_CTRL_NS, 8, 8 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, WLBT_CTRL_NS, 7, 7 },
};

struct pmucal_data WLBT_RESET_ASSERT[] = {
	{ WLBT_PMUCAL_CLEAR, 1, 1, WLBT_CTRL_NS, 7, 7 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CONFIGURATION, 0, 0x0 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_STATUS, 0, 0x0 },
};

struct pmucal_data WLBT_RESET_RELEASE[] = {
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_OPTION, 3, 0x1 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, V_PWREN, 31, 0x12 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, V_PWREN, 31, 0x13 },
	{ WLBT_PMUCAL_DELAY, 1, 0, 0, 0, 0x3E8 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CONFIGURATION, 0, 0x1 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_STATUS, 0, 0x1 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_IN, 4, 0x1 },
	{ WLBT_PMUCAL_CLEAR, 1, 1, WLBT_CTRL_NS, 8, 8 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, WLBT_CTRL_NS, 7, 7 },
};

struct pmucal pmucal_wlbt = {
	WLBT_INIT_SEQ, WLBT_RESET_ASSERT, WLBT_RESET_RELEASE, 11, 3, 9,
};
