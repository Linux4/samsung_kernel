/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MET_LOG_EMI_H
#define _MET_LOG_EMI_H

#if CFG_SUPPORT_MET_LOG
struct MET_LOG_EMI_CTRL {
	uint8_t initialized;
	uint32_t project_id;
	struct workqueue_struct *wq;
	struct work_struct work;
	void *priv;
	uint32_t start_addr;
	uint32_t end_addr;
	uint32_t offset;
	uint32_t emi_size;
	uint32_t rp; /* read pointer */
	uint32_t irp; /* internal read pointer, used by driver */
	uint32_t wp; /* write pointer */
	uint32_t checksum; /* 1: new MET data cover un-read MET data */
	uint8_t *buffer;
};

struct MET_LOG_HEADER {
	uint32_t rp; /* read pointer */
	uint32_t wp; /* write pointer */
	uint32_t checksum; /* 1: new MET data cover un-read MET data */
};

uint32_t met_log_emi_init(struct ADAPTER *ad);
uint32_t met_log_emi_deinit(struct ADAPTER *ad);
#endif /* CFG_SUPPORT_MET_LOG */

#endif /* _MET_LOG_EMI_H */
