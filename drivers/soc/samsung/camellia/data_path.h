/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_DATA_PATH_H__
#define __CAMELLIA_DATA_PATH_H__

#include "mailbox.h"

#define FLAG_DATA_BIT 0x00

#define REE_DATA_FLAG (FLAG_DATA_BIT | 0x1)

struct camellia_data_msg_header {
	u32 source;
	u32 destination;
	u32 length;
} __attribute__((packed));

struct camellia_data_msg {
	struct camellia_msg_flag flag;
	struct camellia_data_msg_header header;
	u8 payload[];
} __attribute__((packed));

void camellia_dispatch_data_msg(struct camellia_data_msg *msg);

size_t camellia_send_data_msg(unsigned int id, struct camellia_data_msg *msg);
int camellia_read_data_msg_timeout(unsigned int id, char __user *buffer,
								   size_t cnt, uint64_t timeout);
unsigned int camellia_poll_wait_data_msg(unsigned int id,
		struct file *file,
		struct poll_table_struct *poll_tab);
int camellia_is_data_msg_queue_empty(unsigned int id);
int camellia_data_peer_create(unsigned int id);
void camellia_data_peer_destroy(unsigned int id);

int camellia_data_path_init(void);
void camellia_data_path_deinit(void);

#endif
