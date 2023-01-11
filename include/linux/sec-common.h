/* include/linux/sec-common.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SEC_COMMON_H
#define SEC_COMMON_H

extern struct class *sec_class;

void sec_common_init(void);

void sec_common_init_post(void);

int get_board_id(void);

int get_recoverymode(void);

#if defined(CONFIG_SEC_DEBUG)
unsigned char pm88x_get_power_on_reason(void);
#endif

#endif
