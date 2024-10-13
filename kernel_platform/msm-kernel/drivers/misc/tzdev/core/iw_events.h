/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

typedef void (*iw_event_callback_t)(void *arg);

int tzdev_iw_events_register(iw_event_callback_t callback, void *arg);
void tzdev_iw_events_unregister(unsigned int nwd_event_id);
void tzdev_iw_events_raise(unsigned int swd_event_id);

int tzdev_iw_events_init(void);
void tzdev_iw_events_fini(void);
