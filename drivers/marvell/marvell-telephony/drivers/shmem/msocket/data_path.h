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

#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include "shm_share.h"
#include "shm.h"
#include "msocket.h"

// #define DEBUG_DP

#ifdef DEBUG_DP
#define DP_ENTER()               printk("%s enter\n", __func__)
#define DP_LEAVE()               printk("%s leave\n", __func__)
#define DP_PRINT(fmt, args...)   printk(fmt, ##args)
#else
#define DP_ENTER()
#define DP_LEAVE()
#define DP_PRINT(fmt, args...)
#endif
#define DP_ERROR(fmt, args...)   printk(KERN_ERR fmt, ##args)

enum data_path_type {
	dp_type_psd,
	dp_type_total_cnt
};

enum data_path_result {
	dp_success,

	dp_already_opened,
	dp_not_open,

	dp_tx_packet_too_large,
	dp_tx_link_failure,
	dp_tx_link_stopped,

	dp_rx_packet_error,
	dp_rx_packet_dropped,
	dp_rx_keep_pending,
};

/*
 * data channel callbacks
 *
 * Note: all these functions will be called in interrupt context,
 * so they cannot sleep, and should as quickly as possible
 */
struct data_path_callback {
	/*
	 * tx
	 *
	 * get_packet is called when we need to transmit a packet
	 *
	 * finish_tx is called when last packet transmit is finished,
	 * result is the transmit result
	 */
	struct sk_buff *(*get_packet) (void);
	void (*finish_tx) (enum data_path_result result);

	/*
	 * rx
	 *
	 * data_rx is called when we received a packet from remote
	 * side, the return value is rx result, we use this flag to
	 * tell low-level if this packet is need to keep pending
	 */
	enum data_path_result (*data_rx) (unsigned char *data,
					     unsigned int length);

	/*
	 * flow control
	 *
	 * tx_stop is called when low-level tx link is full, upper
	 * layer may need to drop packet after receiving this
	 *
	 * tx_resume is called when tx can be started again, upper
	 * layer may need to re-schedule tx (if it have some
	 * packets pending) after receiving this
	 *
	 * rx_stop is called when low-level rx link is full, upper
	 * layer need to re-schedule rx as soon as possible and
	 * begin to drop packets
	 *
	 */
	void (*tx_stop) (void);
	void (*tx_resume) (void);
	void (*rx_stop) (void);

	/*
	 * link status changed
	 *
	 * link_down is called when low-level link is broken
	 *
	 * link_up is called when low-level link is repaired
	 */
	void (*link_down) (void);
	void (*link_up) (void);
};

struct data_path {
	atomic_t state;

	enum data_path_type dp_type;

	struct shm_rbctl *rbctl;

	struct tasklet_struct tx_tl;
	struct tasklet_struct rx_tl;

	struct data_path_callback *cbs;
};

extern void data_path_broadcast_msg(int proc);

static inline void data_path_link_down(void)
{
	data_path_broadcast_msg(MsocketLinkdownProcId);
}

static inline void data_path_link_up(void)
{
	data_path_broadcast_msg(MsocketLinkupProcId);
}

static inline bool data_path_is_link_up(void)
{
	return msocket_is_synced;
}

static inline bool data_path_is_tx_stopped(struct data_path *dp)
{
	return dp->rbctl->is_ap_xmit_stopped;
}

static inline bool data_path_is_rx_stopped(struct data_path *dp)
{
	return dp->rbctl->is_cp_xmit_stopped;
}

extern int data_path_init(void);
extern void data_path_exit(void);

extern struct data_path *data_path_open(enum data_path_type dp_type,
					      struct data_path_callback *cbs);
extern void data_path_close(struct data_path *dp);

extern void data_path_schedule_tx(struct data_path *dp);
extern void data_path_schedule_rx(struct data_path *dp);

#endif /* DATA_CHANNEL_H_ */
