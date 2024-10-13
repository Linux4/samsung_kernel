/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_DUMP_H__
#define __CAMELLIA_DUMP_H__

void camellia_call_coredump_work(u8 event);
int camellia_coredump_memlog_register(void);
int camellia_dump_init(void);
void camellia_dump_deinit(void);

#endif
