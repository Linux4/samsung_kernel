/* arch/arm/include/asm/sec/sec_log.h
 *
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
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


#ifndef __SEC_LOG_H__
#define __SEC_LOG_H__

extern int sec_log_buf_nocache_enable;

#ifdef CONFIG_SEC_LOG
void sec_debug_panic_message(int en);
#else /* CONFIG_SEC_LOG */
static inline void sec_debug_panic_message(en) { }
#endif /* CONFIG_SEC_LOG */

#endif /*__SEC_LOG_H__ */
