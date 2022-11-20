/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDIO_CHANNEL_H__
#define __SDIO_CHANNEL_H__

int sprd_sdio_channel_open(void);
void sprd_sdio_channel_close(void);
int sprd_sdio_channel_tx(const char *buf, unsigned int len, unsigned int addr);
int sprd_sdio_channel_rx(char *buf, unsigned int len, unsigned int addr);

#endif
