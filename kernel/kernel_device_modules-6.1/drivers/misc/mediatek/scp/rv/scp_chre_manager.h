/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __SCP_CHRE_MANAGER_H__
#define __SCP_CHRE_MANAGER_H__

void scp_chre_manager_init(void);
void scp_chre_manager_exit(void);

#define SCP_CHRE_MAGIC 0x67728269
#define SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM 0x8000

/* ipi msg */
struct scp_chre_ipi_msg {
	uint32_t magic;
	uint32_t size;
};

/* payload for exchanging CHRE data*/
struct scp_chre_manager_payload {
	struct scp_chre_ipi_msg msg;
	uint64_t ptr;
};

enum SCP_CHRE_STAT {
	SCP_CHRE_UNINIT = 0,
	SCP_CHRE_STOP = 1,
	SCP_CHRE_START = 2,
};

/* State sync structure */
struct scp_chre_manager_stat_info {
	long ret_user_stat_addr;
};

/* IOCTL */
#define IOCTL_NR_MASK ((1U << _IOC_NRBITS)-1)
#define SCP_CHRE_MANAGER_STAT_UNINIT _IOW('a', 0, unsigned int)
#define SCP_CHRE_MANAGER_STAT_STOP _IOW('a', 1, unsigned int)
#define SCP_CHRE_MANAGER_STAT_START _IOW('a', 2, unsigned int)

#endif
