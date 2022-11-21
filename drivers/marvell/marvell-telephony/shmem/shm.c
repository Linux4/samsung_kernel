/*
    shm.c Created on: Aug 2, 2010, Jinhua Huang <jhhuang@marvell.com>

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

#include <linux/types.h>

#include "shm.h"
#include "acipcd.h"
#include "pxa9xx_cp.h"

#define SHM_SKBUF_SIZE 		2048	/* maximum packet size */
#define SHM_PSD_TX_SKBUF_SIZE 	2048	/* PSD tx maximum packet size */
#define SHM_PSD_RX_SKBUF_SIZE 	16384	/* PSD rx maximum packet size */

struct shm_rbctl shm_rbctl[shm_rb_total_cnt];

extern struct cp_load_table_head *get_cp_load_table(void);

static int shm_param_init(void)
{
	struct cp_load_table_head *ptable = get_cp_load_table();
	if (!ptable)
		return -1;

	/* main ring buffer */
	shm_rbctl[shm_rb_main].skctl_pa = ptable->dp.ACIPCOtherPortKeySectionBegin;

	shm_rbctl[shm_rb_main].tx_skbuf_size = SHM_SKBUF_SIZE;
	shm_rbctl[shm_rb_main].rx_skbuf_size = SHM_SKBUF_SIZE;

	shm_rbctl[shm_rb_main].tx_pa = ptable->dp.ACIPCOtherPortDlBegin;
	shm_rbctl[shm_rb_main].rx_pa = ptable->dp.ACIPCOtherPortUlBegin;

	shm_rbctl[shm_rb_main].tx_total_size =
	    ptable->dp.ACIPCOtherPortDlEnd - ptable->dp.ACIPCOtherPortDlBegin + 4;
	shm_rbctl[shm_rb_main].rx_total_size =
	    ptable->dp.ACIPCOtherPortUlEnd - ptable->dp.ACIPCOtherPortUlBegin + 4;

	shm_rbctl[shm_rb_main].tx_skbuf_num =
	    shm_rbctl[shm_rb_main].tx_total_size /
	    shm_rbctl[shm_rb_main].tx_skbuf_size;
	shm_rbctl[shm_rb_main].rx_skbuf_num =
	    shm_rbctl[shm_rb_main].rx_total_size /
	    shm_rbctl[shm_rb_main].rx_skbuf_size;

	shm_rbctl[shm_rb_main].tx_skbuf_low_wm =
	    (shm_rbctl[shm_rb_main].tx_skbuf_num + 1) / 4;
	shm_rbctl[shm_rb_main].rx_skbuf_low_wm =
	    (shm_rbctl[shm_rb_main].rx_skbuf_num + 1) / 4;

	/* psd dedicated ring buffer */
	shm_rbctl[shm_rb_psd].skctl_pa = ptable->dp.ACIPCPSKeySectionBegin;

	shm_rbctl[shm_rb_psd].tx_skbuf_size = SHM_PSD_TX_SKBUF_SIZE;
	shm_rbctl[shm_rb_psd].rx_skbuf_size = SHM_PSD_RX_SKBUF_SIZE;


	shm_rbctl[shm_rb_psd].tx_pa = ptable->dp.ACIPCPSDownlinkBegin;
	shm_rbctl[shm_rb_psd].rx_pa = ptable->dp.ACIPCPSUplinkBegin;

	shm_rbctl[shm_rb_psd].tx_total_size =
	    ptable->dp.ACIPCPSDownlinkEnd - ptable->dp.ACIPCPSDownlinkBegin + 4;
	shm_rbctl[shm_rb_psd].rx_total_size =
	    ptable->dp.ACIPCPSUplinkEnd - ptable->dp.ACIPCPSUplinkBegin + 4;

	shm_rbctl[shm_rb_psd].tx_skbuf_num =
	    shm_rbctl[shm_rb_psd].tx_total_size /
	    shm_rbctl[shm_rb_psd].tx_skbuf_size;
	shm_rbctl[shm_rb_psd].rx_skbuf_num =
	    shm_rbctl[shm_rb_psd].rx_total_size /
	    shm_rbctl[shm_rb_psd].rx_skbuf_size;

	shm_rbctl[shm_rb_psd].tx_skbuf_low_wm =
	    (shm_rbctl[shm_rb_psd].tx_skbuf_num + 1) / 4;
	shm_rbctl[shm_rb_psd].rx_skbuf_low_wm =
	    (shm_rbctl[shm_rb_psd].rx_skbuf_num + 1) / 4;


	/* diag ring buffer */
	shm_rbctl[shm_rb_diag].skctl_pa = ptable->dp.ACIPCDIAGKeySectionBegin;

	shm_rbctl[shm_rb_diag].tx_skbuf_size = SHM_SKBUF_SIZE;
	shm_rbctl[shm_rb_diag].rx_skbuf_size = SHM_SKBUF_SIZE;

	shm_rbctl[shm_rb_diag].tx_pa = ptable->dp.ACIPCDIAGPortUlBegin;
	shm_rbctl[shm_rb_diag].rx_pa = ptable->dp.ACIPCDIAGPortDlBegin;

	shm_rbctl[shm_rb_diag].tx_total_size =
	    ptable->dp.ACIPCDIAGPortUlEnd - ptable->dp.ACIPCDIAGPortUlBegin + 4;
	shm_rbctl[shm_rb_diag].rx_total_size =
	    ptable->dp.ACIPCDIAGPortDlEnd - ptable->dp.ACIPCDIAGPortDlBegin + 4;

	shm_rbctl[shm_rb_diag].tx_skbuf_num =
	    shm_rbctl[shm_rb_diag].tx_total_size /
	    shm_rbctl[shm_rb_diag].tx_skbuf_size;
	shm_rbctl[shm_rb_diag].rx_skbuf_num =
	    shm_rbctl[shm_rb_diag].rx_total_size /
	    shm_rbctl[shm_rb_diag].rx_skbuf_size;

	shm_rbctl[shm_rb_diag].tx_skbuf_low_wm =
	    (shm_rbctl[shm_rb_diag].tx_skbuf_num + 1) / 4;
	shm_rbctl[shm_rb_diag].rx_skbuf_low_wm =
	    (shm_rbctl[shm_rb_diag].rx_skbuf_num + 1) / 4;

	printk("ACIPCPSDownlinkBegin: 0x%08x, ACIPCPSDownlinkEnd: 0x%08x\n",
	       ptable->dp.ACIPCPSDownlinkBegin,
	       ptable->dp.ACIPCPSDownlinkEnd);
	printk("ACIPCPSUplinkBegin: 0x%08x, ACIPCPSUplinkEnd: 0x%08x\n",
	       ptable->dp.ACIPCPSUplinkBegin,
	       ptable->dp.ACIPCPSUplinkEnd);
	printk
	    ("ACIPCOtherPortDlBegin: 0x%08x, ACIPCOtherPortDlEnd: 0x%08x\n",
	     ptable->dp.ACIPCOtherPortDlBegin,
	     ptable->dp.ACIPCOtherPortDlEnd);
	printk
	    ("ACIPCOtherPortUlBegin: 0x%08x, ACIPCOtherPortUlEnd: 0x%08x\n",
	     ptable->dp.ACIPCOtherPortUlBegin,
	     ptable->dp.ACIPCOtherPortUlEnd);
	printk
	    ("ACIPCDIAGPortDlBegin: 0x%08x, ACIPCDIAGPortDlEnd: 0x%08x\n",
	     ptable->dp.ACIPCDIAGPortDlBegin,
	     ptable->dp.ACIPCDIAGPortDlEnd);
	printk
	    ("ACIPCDIAGPortUlBegin: 0x%08x, ACIPCDIAGPortUlEnd: 0x%08x\n",
	     ptable->dp.ACIPCDIAGPortUlBegin,
	     ptable->dp.ACIPCDIAGPortUlEnd);
	printk
	    ("ACIPCOtherPortKeySectionBegin: 0x%08x, ACIPCPSKeySectionBegin: 0x%08x \
	      ACIPCDIAGPortKeySectionBegin: 0x%08x \n",
	     ptable->dp.ACIPCOtherPortKeySectionBegin,
	     ptable->dp.ACIPCPSKeySectionBegin,
	     ptable->dp.ACIPCDIAGKeySectionBegin);

	return 0;
}

void shm_rb_data_init(struct shm_rbctl *rbctl)
{
	rbctl->is_ap_xmit_stopped = false;
	rbctl->is_cp_xmit_stopped = false;

	rbctl->ap_stopped_num = 0;
	rbctl->ap_resumed_num = 0;
	rbctl->cp_stopped_num = 0;
	rbctl->cp_resumed_num = 0;

	rbctl->skctl_va->ap_wptr = 0;
	rbctl->skctl_va->cp_rptr = 0;
	rbctl->skctl_va->ap_port_fc = 0;
	rbctl->skctl_va->ap_rptr = 0;
	rbctl->skctl_va->cp_wptr = 0;
	rbctl->skctl_va->cp_port_fc = 0;
	rbctl->skctl_va->ap_pcm_master = PMIC_MASTER_FLAG;
	rbctl->skctl_va->diag_ap_db_ver = 0;
	rbctl->skctl_va->diag_cp_db_ver = 0;
	rbctl->skctl_va->diag_header_ptr = 0;
}

static inline void shm_rb_dump(struct shm_rbctl *rbctl)
{
	printk("rb_type %d:\n", rbctl - shm_rbctl);
	printk("\tskctl_pa: 0x%08lx, skctl_va: 0x%p\n",
	       rbctl->skctl_pa, rbctl->skctl_va);

	printk("\ttx_pa: 0x%08lx, tx_va: 0x%p\n", rbctl->tx_pa, rbctl->tx_va);
	printk("\ttx_total_size: 0x%08x, tx_skbuf_size: 0x%08x\n",
	       rbctl->tx_total_size, rbctl->tx_skbuf_size);
	printk("\ttx_skbuf_num: 0x%08x, tx_skbuf_low_wm: 0x%08x\n",
	       rbctl->tx_skbuf_num, rbctl->tx_skbuf_low_wm);

	printk("\trx_pa: 0x%08lx, rx_va: 0x%p\n", rbctl->rx_pa, rbctl->rx_va);
	printk("\trx_total_size: 0x%08x, rx_skbuf_size: 0x%08x\n",
	       rbctl->rx_total_size, rbctl->rx_skbuf_size);
	printk("\trx_skbuf_num: 0x%08x, rx_skbuf_low_wm: 0x%08x\n",
	       rbctl->rx_skbuf_num, rbctl->rx_skbuf_low_wm);
}

static int shm_rb_init(struct shm_rbctl *rbctl)
{
	rbctl->rb_type = rbctl - shm_rbctl;

	/* map to non-cache virtual address */
	rbctl->skctl_va =
	    ioremap_nocache(rbctl->skctl_pa, sizeof(struct shm_skctl));
	if (!rbctl->skctl_va)
		goto exit1;

	rbctl->tx_va = ioremap_nocache(rbctl->tx_pa, rbctl->tx_total_size);
	if (!rbctl->tx_va)
		goto exit2;

	rbctl->rx_va = ioremap_nocache(rbctl->rx_pa, rbctl->rx_total_size);

	if (!rbctl->rx_va)
		goto exit3;

	shm_rb_data_init(rbctl);
	shm_rb_dump(rbctl);

	return 0;

exit3:
	iounmap(rbctl->tx_va);
exit2:
	iounmap(rbctl->skctl_va);
exit1:
	return -1;
}

static int shm_rb_exit(struct shm_rbctl *rbctl)
{
	/* release memory */
	iounmap(rbctl->skctl_va);
	iounmap(rbctl->tx_va);
	iounmap(rbctl->rx_va);

	return 0;
}

/* shm_init */
int shm_init(void)
{
	int ret;
	struct shm_rbctl *rbctl;
	struct shm_rbctl *rbctl2;
	struct shm_rbctl *rbctl_end = shm_rbctl + shm_rb_total_cnt;


	ret = shm_param_init();
	if (ret < 0)
		return ret;

	for (rbctl = shm_rbctl; rbctl != rbctl_end; ++rbctl) {
		ret = shm_rb_init(rbctl);
		if (ret < 0)
			break;
	}

	if (ret < 0) {
		for (rbctl2 = shm_rbctl; rbctl2 != rbctl; ++rbctl2)
			shm_rb_exit(rbctl2);
	}

	return ret;
}

void shm_exit(void)
{
	struct shm_rbctl *rbctl;
	struct shm_rbctl *rbctl_end = shm_rbctl + shm_rb_total_cnt;

	for (rbctl = shm_rbctl; rbctl != rbctl_end; ++rbctl)
		shm_rb_exit(rbctl);
}

struct shm_rbctl *shm_open(enum shm_rb_type rb_type,
	struct shm_callback *cb, void *priv)
{
	if (rb_type >= shm_rb_total_cnt || rb_type < 0) {
		printk("%s: incorrect type %d\n", __func__, rb_type);
		return NULL;
	}

	if (!cb) {
		printk("%s: callback is NULL\n", __func__);
		return NULL;
	}

	shm_rbctl[rb_type].cbs = cb;
	shm_rbctl[rb_type].priv = priv;

	return &shm_rbctl[rb_type];
}

void shm_close(struct shm_rbctl *rbctl)
{
	if (!rbctl) {
		return;
	}

	rbctl->cbs = NULL;
	rbctl->priv = NULL;
}

/* write packet to share memory socket buffer */
void shm_xmit(struct shm_rbctl *rbctl, struct sk_buff *skb)
{
	struct shm_skctl *skctl = rbctl->skctl_va;

	/*
	 * we always write to the next slot !?
	 * thinking the situation of the first slot in the first accessing
	 */
	int slot = shm_get_next_tx_slot(rbctl, skctl->ap_wptr);

	if (!skb) {
		printk(KERN_ERR "shm_xmit skb is null..\n");
		return;
	}
	memcpy(SHM_PACKET_PTR(rbctl->tx_va, slot, rbctl->tx_skbuf_size),
	       skb->data, skb->len);
	skctl->ap_wptr = slot;	/* advance pointer index */
}

/* read packet from share memory socket buffer */
struct sk_buff *shm_recv(struct shm_rbctl *rbctl)
{
	struct shm_skctl *skctl = rbctl->skctl_va;
	enum shm_rb_type rb_type = rbctl - shm_rbctl;

	/* yes, we always read from the next slot either */
	int slot = shm_get_next_rx_slot(rbctl, skctl->ap_rptr);

	/* get the total packet size first for memory allocate */
	unsigned char *hdr = SHM_PACKET_PTR(rbctl->rx_va, slot,
					       rbctl->rx_skbuf_size);
	int count = 0;
	struct sk_buff *skb = NULL;

	if (rb_type == shm_rb_main) {
		struct shm_skhdr *skhdr = (struct shm_skhdr*)hdr;
		count = skhdr->length + sizeof(*skhdr);

		if (skhdr->length <= 0) {
			printk(KERN_EMERG
					"MSOCK: shm_recv: slot = %d, skhdr->length = %d\n", slot,
					skhdr->length);
			goto error_length;
		}
	}
	else if (rb_type == shm_rb_psd) {
		struct shm_psd_skhdr *skhdr = (struct shm_psd_skhdr*)hdr;
		count = skhdr->length + sizeof(*skhdr);
		printk(KERN_WARNING
				"MSOCK: shm_recv: calling in psd ring buffer!!!\n");
	}

	if (count > rbctl->rx_skbuf_size) {
		printk(KERN_EMERG
		       "MSOCK: shm_recv: slot = %d, count = %d\n", slot,
		       count);
		goto error_length;
	}

	skb = alloc_skb(count, GFP_ATOMIC);
	if (!skb)
		return NULL;

	/* write all the packet data including header to sk_buff */
	memcpy(skb_put(skb, count), hdr, count);

error_length:
	/* advance reader pointer */
	skctl->ap_rptr = slot;

	return skb;
}
