/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_LOGSINK_H__
#define __CAMELLIA_LOGSINK_H__

#define LOG_OFFSET 0xF80000
#define LOG_SIZE 0x40000

int camellia_memlog_register(void);
void camellia_call_log_work(u8 event);
void camellia_write_log_file(void);

#endif
