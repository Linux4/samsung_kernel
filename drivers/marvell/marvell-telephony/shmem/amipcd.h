/*
    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _AMIPCD_H_
#define _AMIPCD_H_

#include <linux/types.h>
#include <linux/version.h>
#include <linux/ratelimit.h>

#include <linux/pxa9xx_amipc.h>

/* some arbitrary definitions */
#define PACKET_SENT		0x11
#define PEER_SYNC		0x5

#define TX_RESUME		0x9
#define TX_STOP			0x81

/*
 * the following amipc_notify_* inline functions are used to generate
 * interrupt from AP to M3.
 */
/* generate peer sync interrupt */
static inline void amipc_notify_peer_sync(void)
{
	if (amipc_datasend(AMIPC_SHM_PACKET_NOTIFY, PEER_SYNC,
		0, 0) != AMIPC_RC_OK)
		pr_err_ratelimited("%s: notify m3 failed\n", __func__);
}

/* notify m3 that new packet available in the socket buffer */
static inline void amipc_notify_packet_sent(void)
{
	if (amipc_datasend(AMIPC_SHM_PACKET_NOTIFY, PACKET_SENT,
		0, 0) != AMIPC_RC_OK)
		pr_err_ratelimited("%s: notify m3 failed\n", __func__);
}

/* notify m3 that m3 can continue transmission */
static inline void amipc_notify_m3_tx_resume(void)
{
	pr_warn("MSOCK: amipc_notify_m3_tx_resume!!!\n");
	if (amipc_datasend(AMIPC_RINGBUF_FC, TX_RESUME,
		0, 0) != AMIPC_RC_OK)
		pr_err_ratelimited("%s: notify m3 failed\n", __func__);
}

/*notify m3 that ap transmission is stopped, please resume me later */
static inline void amipc_notify_ap_tx_stopped(void)
{
	pr_warn("MSOCK: amipc_notify_ap_tx_stopped!!!\n");
	if (amipc_datasend(AMIPC_RINGBUF_FC, TX_STOP,
		0, 0) != AMIPC_RC_OK)
		pr_err_ratelimited("%s: notify m3 failed\n", __func__);
}

extern int amipc_init(void);
extern void amipc_exit(void);
extern struct wakeup_source amipc_wakeup;

#endif /* _AMIPCD_H_ */
