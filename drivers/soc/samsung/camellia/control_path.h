/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_CONTROL_PATH_H__
#define __CAMELLIA_CONTROL_PATH_H__

#include "mailbox.h"

#define FLAG_CONTROL_BIT 0x80

#define REE_CONTROL_FLAG (FLAG_CONTROL_BIT | 0x1)

struct camellia_control_msg {
	struct camellia_msg_flag flag;
	u8 cmd;
	u8 sub_cmd;
} __attribute__((packed));

void camellia_dispatch_control_msg(struct camellia_control_msg *msg);

int camellia_control_path_init(void);
void camellia_control_path_deinit(void);

/* IOCTL commands */
#define CAMELLIA_CONTROL_IOCTL_MAGIC 'd'
#define CAMELLIA_CONTROL_IOCTL_SET_DAEMON _IOW(CAMELLIA_CONTROL_IOCTL_MAGIC, 40, pid_t)

#endif
