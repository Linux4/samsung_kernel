/*
    shm.h Created on: Aug 2, 2010, Jinhua Huang <jhhuang@marvell.com>

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

#ifndef SHM_H_
#define SHM_H_

#include <linux/skbuff.h>
#include "acipcd.h"

#define SHM_PACKET_PTR(b, n, sz) ((unsigned char *)b + n * sz)

enum shm_rb_type {
	shm_rb_main,
	shm_rb_psd,
	shm_rb_diag,
	shm_rb_total_cnt
};

struct shm_rbctl {
	/*
	 * type
	 */
	enum shm_rb_type rb_type;

	/*
	 * callback function
	 */
	struct shm_callback *cbs;

	/*
	 * private, owned by upper layer
	 */
	void *priv;

	/*
	 * key section pointer
	 */
	unsigned long     skctl_pa;
	struct shm_skctl *skctl_va;

	/*
	 * TX buffer
	 */
	unsigned long  tx_pa;
	void          *tx_va;
	int            tx_total_size;
	int            tx_skbuf_size;
	int            tx_skbuf_num;
	int            tx_skbuf_low_wm;

	/*
	 * RX buffer
	 */
	unsigned long  rx_pa;
	void          *rx_va;
	int            rx_total_size;
	int            rx_skbuf_size;
	int            rx_skbuf_num;
	int            rx_skbuf_low_wm;

	/*
	 * flow control flag
	 *
	 * I'm sure these two global variable will never enter race condition,
	 * so we needn't any exclusive lock mechanism to access them.
	 */
	bool is_ap_xmit_stopped;
	bool is_cp_xmit_stopped;

	/*
	 * statistics for ring buffer flow control interrupts
	 */
	unsigned long ap_stopped_num;
	unsigned long ap_resumed_num;
	unsigned long cp_stopped_num;
	unsigned long cp_resumed_num;
};

struct shm_callback {
	void (*peer_sync_cb) (struct shm_rbctl *);
	void (*packet_send_cb) (struct shm_rbctl *);
	void (*port_fc_cb) (struct shm_rbctl *);
	void (*rb_stop_cb) (struct shm_rbctl *);
	void (*rb_resume_cb) (struct shm_rbctl *);
};


/* share memory control block structure */
struct shm_skctl {
	/* up-link control block, AP write, CP read */
	volatile int ap_wptr;
	volatile int cp_rptr;
	volatile unsigned int ap_port_fc;

	/* down-link control block, CP write, AP read */
	volatile int ap_rptr;
	volatile int cp_wptr;
	volatile unsigned int cp_port_fc;

#define PMIC_MASTER_FLAG	0x4D415354
	/* PMIC SSP master status setting query */
	volatile unsigned int ap_pcm_master;
	volatile unsigned int cp_pcm_master;
	volatile unsigned int modem_ddrfreq;

	/* DIAG specific info */
	volatile unsigned int diag_header_ptr;
	volatile unsigned int diag_cp_db_ver;
	volatile unsigned int diag_ap_db_ver;
};

/* share memory socket header structure */
struct shm_skhdr {
	unsigned int address;	/* not used */
	int port;				/* queue port */
	unsigned int checksum;	/* not used */
	int length;				/* payload length */
};

/* PSD share memory socket header structure */
struct shm_psd_skhdr {
	unsigned short length;		/* payload length */
	unsigned short reserved;	/* not used */
};

/* DIAG share memory socket header structure */
struct direct_rb_skhdr {
	unsigned int length;		/* payload length */
};

extern struct shm_rbctl shm_rbctl[];

/* get the next tx socket buffer slot */
static inline int shm_get_next_tx_slot(struct shm_rbctl *rbctl, int slot)
{
	return slot + 1 == rbctl->tx_skbuf_num ? 0 : slot + 1;
}

/* get the next rx socket buffer slot */
static inline int shm_get_next_rx_slot(struct shm_rbctl *rbctl, int slot)
{
	return slot + 1 == rbctl->rx_skbuf_num ? 0 : slot + 1;
}

/* check if up-link free socket buffer available */
static inline bool shm_is_xmit_full(struct shm_rbctl *rbctl)
{
	return shm_get_next_tx_slot(rbctl,
				    rbctl->skctl_va->ap_wptr) ==
	    rbctl->skctl_va->cp_rptr;
}

/* check if down-link socket buffer data received */
static inline bool shm_is_recv_empty(struct shm_rbctl *rbctl)
{
	return rbctl->skctl_va->cp_wptr ==
	    rbctl->skctl_va->ap_rptr;
}

/* check if down-link socket buffer has enough free slot */
static inline bool shm_has_enough_free_rx_skbuf(struct shm_rbctl *rbctl)
{
	int free =
	    rbctl->skctl_va->ap_rptr -
	    rbctl->skctl_va->cp_wptr;
	if (free <= 0)
		free += rbctl->rx_skbuf_num;
	return (free > rbctl->rx_skbuf_low_wm);
}

/* get free rx skbuf num */
static inline int shm_free_rx_skbuf(struct shm_rbctl *rbctl)
{
	int free =
	    rbctl->skctl_va->ap_rptr -
	    rbctl->skctl_va->cp_wptr;
	if (free <= 0)
		free += rbctl->rx_skbuf_num;
	return free;
}

/* get free tx skbuf num */
static inline int shm_free_tx_skbuf(struct shm_rbctl *rbctl)
{
	int free =
	    rbctl->skctl_va->cp_rptr -
	    rbctl->skctl_va->ap_wptr;
	if (free <= 0)
		free += rbctl->tx_skbuf_num;
	return free;
}

/* check if cp pmic is in master mode */
static inline bool shm_is_cp_pmic_master(struct shm_rbctl *rbctl)
{
	return rbctl->skctl_va->cp_pcm_master == PMIC_MASTER_FLAG;
}

/* callback wrapper for acipcd */
static inline u32 shm_peer_sync_cb(struct shm_rbctl *rbctl)
{
	if (rbctl && rbctl->cbs && rbctl->cbs->peer_sync_cb)
		rbctl->cbs->peer_sync_cb(rbctl);

	return 0;
}

static inline u32 shm_packet_send_cb(struct shm_rbctl *rbctl)
{
	if (rbctl && rbctl->cbs && rbctl->cbs->packet_send_cb)
		rbctl->cbs->packet_send_cb(rbctl);

	return 0;
}

static inline u32 shm_port_fc_cb(struct shm_rbctl *rbctl)
{
	if (rbctl && rbctl->cbs && rbctl->cbs->port_fc_cb)
		rbctl->cbs->port_fc_cb(rbctl);

	return 0;
}

static inline u32 shm_rb_stop_cb(struct shm_rbctl *rbctl)
{
	if (!rbctl) return 0;

	rbctl->cp_stopped_num++;
	rbctl->is_cp_xmit_stopped = true;

	if (rbctl->cbs && rbctl->cbs->rb_stop_cb)
		rbctl->cbs->rb_stop_cb(rbctl);

	return 0;
}

static inline u32 shm_rb_resume_cb(struct shm_rbctl *rbctl)
{
	if (!rbctl) return 0;

	rbctl->ap_resumed_num++;
	rbctl->is_ap_xmit_stopped = false;

	if (rbctl->cbs && rbctl->cbs->rb_resume_cb)
		rbctl->cbs->rb_resume_cb(rbctl);

	return 0;
}

/* acipc event wrapper */
static inline void shm_notify_peer_sync(struct shm_rbctl *rbctl)
{
	if(rbctl && rbctl->rb_type == shm_rb_main)
		acipc_notify_peer_sync();
}

static inline void shm_notify_packet_sent(struct shm_rbctl *rbctl)
{
	if(rbctl && rbctl->rb_type == shm_rb_main)
		acipc_notify_packet_sent();
	else if (rbctl && rbctl->rb_type == shm_rb_psd)
		acipc_notify_psd_packet_sent();
	else if (rbctl && rbctl->rb_type == shm_rb_diag)
		acipc_notify_diag_packet_sent();
}

static inline void shm_notify_port_fc(struct shm_rbctl *rbctl)
{
	if(rbctl && rbctl->rb_type == shm_rb_main)
		acipc_notify_port_fc();
}

static inline void shm_notify_cp_tx_resume(struct shm_rbctl *rbctl)
{
	if (!rbctl) return;

	rbctl->cp_resumed_num++;
	rbctl->is_cp_xmit_stopped = false;

	if(rbctl->rb_type == shm_rb_main)
		acipc_notify_cp_tx_resume();
	else if (rbctl->rb_type == shm_rb_psd)
		acipc_notify_cp_psd_tx_resume();
}

static inline void shm_notify_ap_tx_stopped(struct shm_rbctl *rbctl)
{
	if (!rbctl) return;

	rbctl->ap_stopped_num++;
	rbctl->is_ap_xmit_stopped = true;

	if (rbctl->rb_type == shm_rb_main)
		acipc_notify_ap_tx_stopped();
	else if (rbctl->rb_type == shm_rb_psd)
		acipc_notify_ap_psd_tx_stopped();
}

/*
 * functions exported by this module
 */
/* init & exit */
extern int shm_init(void);
extern void shm_exit(void);
extern void shm_rb_data_init(struct shm_rbctl *rbctl);

/* open & close */
extern struct shm_rbctl *shm_open(enum shm_rb_type rb_type,
	struct shm_callback *cb, void *priv);
extern void shm_close(struct shm_rbctl *rbctl);

/* xmit and recv */
extern void shm_xmit(struct shm_rbctl *rbctl, struct sk_buff *skb);
extern struct sk_buff *shm_recv(struct shm_rbctl *rbctl);

#endif /* SHM_H_ */
