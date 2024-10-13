/*
 * Copyright (C) 2015-2019, Samsung Electronics.
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

#ifndef __EXYNOS_MODEM_CTRL_H__
#define __EXYNOS_MODEM_CTRL_H__

#include <linux/skbuff.h>

#define MODEM_CTRL_UART_AP 0
#define MODEM_CTRL_UART_CP 1

#if defined(CONFIG_EXYNOS_MODEM_IF)
extern int modem_force_crash_exit_ext(void);
extern int modem_send_panic_noti_ext(void);
#else
static inline int modem_force_crash_exit_ext(void) { return 0; }
static inline int modem_send_panic_noti_ext(void) { return 0; }
#endif

#if defined(CONFIG_CP_ZEROCOPY) || defined(CONFIG_CP_PKTPROC_V2)
extern bool __skb_free_head_cp_zerocopy(struct sk_buff *skb);

/**
 @brief		skb_free_head_cp_zerocopy
 @param skb	the pointer to skbuff
 @return bool	return true if skb->data in zerocopy buffer,
		not to do a rest of skb_free_head process.
*/
static inline bool skb_free_head_cp_zerocopy(struct sk_buff *skb)
{
	return __skb_free_head_cp_zerocopy(skb);
}
#else
static inline bool skb_free_head_cp_zerocopy(struct sk_buff *skb) {
	return false;
}
#endif

#if defined(CONFIG_CP_UART_NOTI)
extern void send_uart_noti_to_modem(int val);
#else
static inline void send_uart_noti_to_modem(int val) { return; }
#endif

#endif /* __EXYNOS_MODEM_CTRL_H__ */
