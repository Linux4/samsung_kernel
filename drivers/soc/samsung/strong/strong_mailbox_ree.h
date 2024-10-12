/*
 * strong_mailbox_ree.h - Samsung Strong Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __STRONG_MAILBOX_REE_H__
#define __STRONG_MAILBOX_REE_H__

#include <linux/miscdevice.h>
#include "strong_mailbox_sfr.h"

typedef struct {
	void *(*mem_allocator)(ssize_t len);
	int (*msg_receiver)(void *buf, unsigned int len, unsigned int status);
	void (*msg_mlog_write)(void);
	uint8_t mem_allocator_flag;
	uint8_t msg_receiver_flag;
	uint8_t msg_mlog_write_flag;
} camellia_func_t;

/* debug define */
enum MB_DBG_INDEX {
	MB_DBG_CNT, MB_DBG_R0, MB_DBG_R1, MB_DBG_R2, MB_DBG_R3,
	MB_DBG_IP, MB_DBG_ST, MB_DBG_LR, MB_DBG_PC, MB_DBG_XPSR,
	MB_DBG_MSP, MB_DBG_PSP, MB_DBG_CFSR, MB_DBG_FAR,
	MB_DBG_MSG, MB_DBG_MAX_INDEX = (STR_MAX_MB_DATA_LEN / 4 - 1)
};
#define MB_DBG_MSG_MAX_SIZE ((MB_DBG_MAX_INDEX - MB_DBG_MSG) * 4)

/*****************************************************************************/
/* Function prototype							     */
/*****************************************************************************/
uint32_t exynos_strong_mailbox_map_sfr(void);
uint32_t mailbox_receive_and_callback(void);
uint32_t strong_enable(void);
uint32_t strong_disable(void);
uint32_t mailbox_cmd(uint32_t mbox_cmd);

/* Export symbols */
void mailbox_register_allocator(void *(*fp)(ssize_t));
void mailbox_register_message_receiver(int (*fp)(void *, unsigned int, unsigned int));
void mailbox_register_message_mlog_writer(void (*fp)(void));

uint32_t mailbox_open(void);
uint32_t mailbox_close(void);

void mailbox_fault_and_callback(bool sfrdump, unsigned int *val, char *str);

int exynos_ssp_notify_ap_sleep(void);
int exynos_ssp_notify_ap_wakeup(void);

#endif /* __STRONG_MAILBOX_REE_H__ */
