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

#ifndef _SHM_H_
#define _SHM_H_

#include <linux/skbuff.h>
#if 0
#include <linux/platform_data/mmp_ddr_tbl.h>
#endif
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/clk/mmpcpdvc.h>
#include "pxa_cp_load_ioctl.h"
#include "acipcd.h"
#include "amipcd.h"

#define SHM_PACKET_PTR(b, n, sz) ((unsigned char *)(b) + (n) * (sz))

enum shm_grp_type {
	shm_grp_cp,
	shm_grp_m3,
	shm_grp_total_cnt
};

enum shm_rb_type {
	shm_rb_main,
	shm_rb_psd,
	shm_rb_diag,
	shm_rb_m3,
	shm_rb_total_cnt
};

struct shm_rbctl {
	/*
	 * name
	 */
	const char *name;

	/*
	 * type
	 */
	enum shm_rb_type rb_type;

	/*
	 * group type
	 */
	enum shm_grp_type grp_type;

	/*
	 * callback function
	 */
	struct shm_callback *cbs;

	/*
	 * private, owned by upper layer
	 */
	void *priv;

	/*
	 * debugfs dir
	 */
	struct dentry *rbdir;

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
	bool           tx_cacheable;

	/*
	 * RX buffer
	 */
	unsigned long  rx_pa;
	void          *rx_va;
	int            rx_total_size;
	int            rx_skbuf_size;
	int            rx_skbuf_num;
	int            rx_skbuf_low_wm;
	bool           rx_cacheable;

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
	/*
	 * lock for virtual address mapping
	 */
	struct mutex	va_lock;
#ifdef CONFIG_SSIPC_SUPPORT
	unsigned int uuid_high;
	unsigned int uuid_low;
#endif	
};

struct shm_callback {
	void (*peer_sync_cb)(struct shm_rbctl *);
	void (*packet_send_cb)(struct shm_rbctl *);
	void (*port_fc_cb)(struct shm_rbctl *);
	void (*rb_stop_cb)(struct shm_rbctl *);
	void (*rb_resume_cb)(struct shm_rbctl *);
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

	volatile unsigned int reset_request;
	volatile unsigned int ap_pm_status_request;
	volatile unsigned int profile_number;

	/* dvc voltage table number */
	volatile unsigned int dvc_vol_tbl_num;
	volatile unsigned int dvc_vol_tbl[16];

#define VERSION_MAGIC_FLAG 0x56455253
#define VERSION_NUMBER_FLAG 0x1
	volatile unsigned int version_magic;
	volatile unsigned int version_number;

	volatile unsigned int dfc_dclk_num;
	volatile unsigned int dfc_dclk[16];

	/*L+G or G+L*/
	volatile unsigned int network_mode;

	/* uuid reserved for SSIPC solution */
	volatile unsigned int uuid_high;
	volatile unsigned int uuid_low;

	/* dvc voltage and frequency */
	volatile unsigned int cp_freq[MAX_CP_PPNUM];
	volatile unsigned int cp_vol[MAX_CP_PPNUM];
	volatile unsigned int msa_dvc_vol;
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
	return rbctl->skctl_va->cp_wptr == rbctl->skctl_va->ap_rptr;
}

/* check if down-link socket buffer has enough free slot */
static inline bool shm_has_enough_free_rx_skbuf(struct shm_rbctl *rbctl)
{
	int free = rbctl->skctl_va->ap_rptr - rbctl->skctl_va->cp_wptr;
	if (free <= 0)
		free += rbctl->rx_skbuf_num;
	return free > rbctl->rx_skbuf_low_wm;
}

/* get free rx skbuf num */
static inline int shm_free_rx_skbuf(struct shm_rbctl *rbctl)
{
	int free = rbctl->skctl_va->ap_rptr - rbctl->skctl_va->cp_wptr;
	if (free <= 0)
		free += rbctl->rx_skbuf_num;
	return free;
}

/* get free tx skbuf num */
static inline int shm_free_tx_skbuf(struct shm_rbctl *rbctl)
{
	int free = rbctl->skctl_va->cp_rptr - shm_get_next_tx_slot(rbctl,
				    rbctl->skctl_va->ap_wptr);
	if (free < 0)
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
	if (!rbctl)
		return 0;

	rbctl->cp_stopped_num++;
	rbctl->is_cp_xmit_stopped = true;

	if (rbctl->cbs && rbctl->cbs->rb_stop_cb)
		rbctl->cbs->rb_stop_cb(rbctl);

	return 0;
}

static inline u32 shm_rb_resume_cb(struct shm_rbctl *rbctl)
{
	if (!rbctl)
		return 0;

	rbctl->ap_resumed_num++;
	rbctl->is_ap_xmit_stopped = false;

	if (rbctl->cbs && rbctl->cbs->rb_resume_cb)
		rbctl->cbs->rb_resume_cb(rbctl);

	return 0;
}

/* acipc event wrapper */
static inline void shm_notify_peer_sync(struct shm_rbctl *rbctl)
{
	if (!rbctl)
		return;

	if (rbctl->rb_type == shm_rb_main)
		acipc_notify_peer_sync();
	else if (rbctl->rb_type == shm_rb_m3)
		amipc_notify_peer_sync();
}

static inline void shm_notify_packet_sent(struct shm_rbctl *rbctl)
{
	if (!rbctl)
		return;

	switch (rbctl->rb_type) {
	case shm_rb_main:
		acipc_notify_packet_sent();
		break;
	case shm_rb_psd:
		acipc_notify_psd_packet_sent();
		break;
	case shm_rb_diag:
		acipc_notify_diag_packet_sent();
		break;
	case shm_rb_m3:
		amipc_notify_packet_sent();
		break;
	default:
		break;
	}
}

static inline void shm_notify_port_fc(struct shm_rbctl *rbctl)
{
	if (rbctl && rbctl->rb_type == shm_rb_main)
		acipc_notify_port_fc();
}

static inline void shm_notify_cp_tx_resume(struct shm_rbctl *rbctl)
{
	if (!rbctl)
		return;

	rbctl->cp_resumed_num++;
	rbctl->is_cp_xmit_stopped = false;

	if (rbctl->rb_type == shm_rb_main)
		acipc_notify_cp_tx_resume();
	else if (rbctl->rb_type == shm_rb_psd)
		acipc_notify_cp_psd_tx_resume();
	else if (rbctl->rb_type == shm_rb_m3)
		amipc_notify_m3_tx_resume();
}

static inline void shm_notify_ap_tx_stopped(struct shm_rbctl *rbctl)
{
	if (!rbctl)
		return;

	rbctl->ap_stopped_num++;
	rbctl->is_ap_xmit_stopped = true;

	if (rbctl->rb_type == shm_rb_main)
		acipc_notify_ap_tx_stopped();
	else if (rbctl->rb_type == shm_rb_psd)
		acipc_notify_ap_psd_tx_stopped();
	else if (rbctl->rb_type == shm_rb_m3)
		amipc_notify_ap_tx_stopped();
}

static inline void shm_flush_dcache(struct shm_rbctl *rbctl,
	void *addr, size_t size)
{
	if (rbctl && rbctl->tx_cacheable) {
		dma_addr_t dma = 0;
#ifdef CONFIG_ARM
		dma = virt_to_dma(NULL, addr);
#elif defined CONFIG_ARM64
		dma = phys_to_dma(NULL, virt_to_phys(addr));
#else
#error unsupported arch
#endif
		dma_sync_single_for_device(NULL, dma,
			size, DMA_TO_DEVICE);

	}
}

static inline void shm_invalidate_dcache(struct shm_rbctl *rbctl,
	void *addr, size_t size)
{
	if (rbctl && rbctl->rx_cacheable) {
		dma_addr_t dma = 0;
#ifdef CONFIG_ARM
		dma = virt_to_dma(NULL, addr);
#elif defined CONFIG_ARM64
		dma = phys_to_dma(NULL, virt_to_phys(addr));
#else
#error unsupported arch
#endif
		dma_sync_single_for_device(NULL, dma,
			size, DMA_FROM_DEVICE);
	}
}

/*
 * functions exported by this module
 */
/* init & exit */
extern int tel_shm_init(enum shm_grp_type grp_type,
	const void *data);
extern void tel_shm_exit(enum shm_grp_type grp_type);
extern int shm_debugfs_init(void);
extern void shm_debugfs_exit(void);
extern void shm_rb_data_init(struct shm_rbctl *rbctl);
extern void shm_lock_init(void);
extern int shm_free_rx_skbuf_safe(struct shm_rbctl *rbctl);
extern int shm_free_tx_skbuf_safe(struct shm_rbctl *rbctl);

/* open & close */
extern struct shm_rbctl *shm_open(enum shm_rb_type rb_type,
				  struct shm_callback *cb, void *priv);
extern void shm_close(struct shm_rbctl *rbctl);

/* xmit and recv */
extern void shm_xmit(struct shm_rbctl *rbctl, struct sk_buff *skb);
extern struct sk_buff *shm_recv(struct shm_rbctl *rbctl);
#if 0
extern int get_voltage_table_for_cp(unsigned int *dvc_voltage_tbl,
				unsigned int *level_num);
extern void get_dfc_tables(void __iomem *ciu_base, struct pxa1928_ddr_opt **tbl, int *len);
#endif

#endif /* _SHM_H_ */
