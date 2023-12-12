/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TZ_KMEMLEAK_H__
#define __TZ_KMEMLEAK_H__

#if IS_MODULE(CONFIG_TZDEV)
int tz_kmemleak_init(void);
#endif

#endif /* __TZ_KMEMLEAK_H__ */
