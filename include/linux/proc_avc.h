/* linux/include/linux/proc_avc.h
 *
 * Copyright (C) 2015 Samsung Electronics Co, Ltd.
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


#ifndef __PROC_AVC_H__
#define __PROC_AVC_H__

#ifdef CONFIG_PROC_AVC

extern int __init sec_avc_log_init(void);
extern void sec_avc_log(char *fmt, ...);

#else /* CONFIG_PROC_AVC */

static inline int __init sec_avc_log_init(void) { return 0; }
static inline void sec_avc_log(char *fmt, ...) { }

#endif /* CONFIG_PROC_AVC */

#endif /* __PROC_AVC_H__ */
