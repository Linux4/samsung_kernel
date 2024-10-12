/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#ifndef __CONTEXTHUB_H_
#define __CONTEXTHUB_H_

#if IS_ENABLED(CONFIG_NANOHUB)
/* interface to maintain power during accessing chub */
extern void contexthub_power_lock(void);
extern void contexthub_power_unlock(void);
#else
static inline void contexthub_power_lock(void) {}
static inline void contexthub_power_unlock(void) {}
#endif

#endif /* __CONTEXTHUB_H_ */