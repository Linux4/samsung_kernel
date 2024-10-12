/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_MAILBOX_H__
#define __CAMELLIA_MAILBOX_H__

struct camellia_msg_flag {
	u8 value;
} __attribute__((packed));

int camellia_mailbox_init(void);
void camellia_mailbox_deinit(void);
int camellia_cache_cnt(bool loop_search);
void *camellia_cache_get_buf(void *);
#endif
