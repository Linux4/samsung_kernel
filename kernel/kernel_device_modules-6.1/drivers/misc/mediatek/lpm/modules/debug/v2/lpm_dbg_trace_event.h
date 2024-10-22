/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __LPM_DBG_TRACE_EVENT_H__
#define __LPM_DBG_TRACE_EVENT_H__

#include <lpm_dbg_common_v2.h>

int lpm_trace_event_init(struct subsys_req *plat_subsys_req, unsigned int plat_subsys_req_num);
void lpm_trace_event_deinit(void);

#endif /* __LPM_DBG_TRACE_EVENT_H__ */
