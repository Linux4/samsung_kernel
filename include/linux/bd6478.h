/* include/linux/bd6478.h
 *
 * Copyright (C) 2015 Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef _LINUX_BD6478_H
#define _LINUX_BD6478_H

#define MAX_TIMEOUT		10000

struct bd6478_pdata {
	u32 duty;
	u32 period;
	u32 max_timeout;
	u32 pwm_id;
};

#endif
