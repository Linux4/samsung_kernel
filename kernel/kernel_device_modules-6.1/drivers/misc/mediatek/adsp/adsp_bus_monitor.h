/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_BUS_MONITOR_H__
#define __ADSP_BUS_MONITOR_H__

/* bus monitor enum */
enum bus_monitor_stage {
	STATE_OFF = 0x0,
	STATE_RUN = 0x1,
	STATE_EMI_TIMEOUT = 0x2,
	STATE_INFRA_TIMEOUT = 0x4,
};

/* bus monitor debug info (20 bytes) */
struct bus_monitor_debug_info {
	u32 state;
	u32 second;
	u32 mini_second;
	u32 interrupt;
	u32 intenable;
};

/* bus monitor methods, be care all of them need adsp clk on */
void adsp_bus_monitor_dump(void);
#endif

