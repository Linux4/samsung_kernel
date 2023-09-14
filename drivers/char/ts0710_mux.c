/* File: mux_driver.c
 *
 * Portions derived from rfcomm.c, original header as follows:
 *
 * Copyright (C) 2000, 2001  Axis Communications AB
 *
 * Author: Mats Friden <mats.friden@axis.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Exceptionally, Axis Communications AB grants discretionary and
 * conditional permissions for additional use of the text contained
 * in the company's release of the AXIS OpenBT Stack under the
 * provisions set forth hereunder.
 *
 * Provided that, if you use the AXIS OpenBT Stack with other files,
 * that do not implement functionality as specified in the Bluetooth
 * System specification, to produce an executable, this does not by
 * itself cause the resulting executable to be covered by the GNU
 * General Public License. Your use of that executable is in no way
 * restricted on account of using the AXIS OpenBT Stack code with it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the provisions of the GNU
 * General Public License.
 *
 */

/*
 * Copyright (C) 2002-2004  Motorola
 * Copyright (C) 2006 Harald Welte <laforge@openezx.org>
 *  07/28/2002  Initial version
 *  11/18/2002  Second version
 *  04/21/2004  Add GPRS PROC
 *  09/28/2008  Porting to kernel 2.6.21 by Spreadtrum
 *  11/08/2013  Add Multi-mux support
*/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/sprdmux.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sched/rt.h>
#include <linux/version.h> 
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include "ts0710.h"
#include "ts0710_mux.h"


#define TS0710MUX_GPRS_SESSION_MAX 3
#define TS0710MUX_MAJOR 250
#define TS0710MUX_MINOR_START 0
#define NR_MUXS 32

#define TS0710MUX_TIME_OUT 250	/* 2500ms, for BP UART hardware flow control AP UART  */

#define TS0710MUX_IO_DLCI_FC_ON 0x54F2
#define TS0710MUX_IO_DLCI_FC_OFF 0x54F3
#define TS0710MUX_IO_FC_ON 0x54F4
#define TS0710MUX_IO_FC_OFF 0x54F5

#define TS0710MUX_MAX_BUF_SIZE		(64*1024)
#define TS0710MUX_MAX_TOTAL_SIZE	(1024*1024)

#define TS0710MUX_SEND_BUF_OFFSET 10
#define TS0710MUX_SEND_BUF_SIZE (DEF_TS0710_MTU + TS0710MUX_SEND_BUF_OFFSET + 34)
#define TS0710MUX_RECV_BUF_SIZE TS0710MUX_SEND_BUF_SIZE

/*For BP UART problem Begin*/
#ifdef TS0710SEQ2
#define ACK_SPACE 0		/* 6 * 11(ACK frame size)  */
#else
#define ACK_SPACE 0		/* 6 * 7(ACK frame size)  */
#endif
/*For BP UART problem End*/

#define TS0710MUX_SERIAL_BUF_SIZE (DEF_TS0710_MTU + TS0710_MAX_HDR_SIZE + ACK_SPACE)	/* For BP UART problem: ACK_SPACE  */

#define TS0710MUX_MAX_TOTAL_FRAME_SIZE (DEF_TS0710_MTU + TS0710_MAX_HDR_SIZE + FLAG_SIZE)
#define TS0710MUX_MAX_CHARS_IN_BUF 65535
#define TS0710MUX_THROTTLE_THRESHOLD DEF_TS0710_MTU

#define TEST_PATTERN_SIZE 250

#define CMDTAG 0x55
#define DATATAG 0xAA

#define ACK 0x4F		/*For BP UART problem */

/*For BP UART problem Begin*/
#ifdef TS0710SEQ2
#define FIRST_BP_SEQ_OFFSET 0	/*offset from start flag */
#define SECOND_BP_SEQ_OFFSET 0	/*offset from start flag */
#define FIRST_AP_SEQ_OFFSET 0	/*offset from start flag */
#define SECOND_AP_SEQ_OFFSET 0	/*offset from start flag */
#define SLIDE_BP_SEQ_OFFSET 0	/*offset from start flag */
#define SEQ_FIELD_SIZE 0
#else
#define SLIDE_BP_SEQ_OFFSET  1	/*offset from start flag */
#define SEQ_FIELD_SIZE    0
#endif

#define ADDRESS_FIELD_OFFSET (1 + SEQ_FIELD_SIZE)	/*offset from start flag */
/*For BP UART problem End*/

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(v) (void)(v)
#endif

#define TS0710MUX_GPRS1_DLCI  3
#define TS0710MUX_GPRS2_DLCI  4
#define TS0710MUX_VT_DLCI         2

#define TS0710MUX_GPRS1_RECV_COUNT_IDX 0
#define TS0710MUX_GPRS1_SEND_COUNT_IDX 1
#define TS0710MUX_GPRS2_RECV_COUNT_IDX 2
#define TS0710MUX_GPRS2_SEND_COUNT_IDX 3
#define TS0710MUX_VT_RECV_COUNT_IDX 4
#define TS0710MUX_VT_SEND_COUNT_IDX 5

#define TS0710MUX_COUNT_MAX_IDX        5
#define TS0710MUX_COUNT_IDX_NUM (TS0710MUX_COUNT_MAX_IDX + 1)

#define SPRDMUX_MAX_NUM	SPRDMUX_ID_MAX

/* Bit number in flags of mux_send_struct */
#define RECV_RUNNING 0
#define MUX_MODE_MAX_LEN 11

#define MUX_STATE_NOT_READY 0
#define MUX_STATE_READY 1
#define MUX_STATE_CRASHED 3
#define MUX_STATE_RECOVERING 4

#define MUX_RIL_LINE_END 11

#define MUX_VETH_LINE_BEGIN 13
#define MUX_VETH_LINE_END 17
#define MUX_VETH_RINGBUFER_NUM 24

#define MUX_WATCH_INTERVAL	3 * HZ

/* Debug */
//#define TS0710DEBUG
//#define TS0710LOG

static __u8 line2dlci[NR_MUXS] =
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ,14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
typedef struct {
	__u8 cmdline;
	__u8 dataline;
} dlci_line;

static dlci_line dlci2line[] = {
	{0, 0},			/* DLCI 0 */
	{0, 0},			/* DLCI 1 */
	{1, 1},			/* DLCI 2 */
	{2, 2},			/* DLCI 3 */
	{3, 3},			/* DLCI 4 */
	{4, 4},			/* DLCI 5 */
	{5, 5},			/* DLCI 6 */
	{6, 6},			/* DLCI 7 */
	{7, 7},			/* DLCI 8 */
	{8, 8},			/* DLCI 9 */
	{9, 9},			/* DLCI 10 */
	{10, 10},		/* DLCI 11 */
	{11, 11},		/* DLCI 12 */
	{12, 12},		/* DLCI 13 */
	{13, 13},		/* DLCI 14 */
	{14, 14},		/* DLCI 15 */
	{15, 15},		/* DLCI 16 */
	{16, 16},		/* DLCI 17 */
	{17, 17},		/* DLCI 18 */
	{18, 18},		/* DLCI 19 */
	{19, 19},		/* DLCI 20 */
	{20, 20},		/* DLCI 21 */
	{21, 21},		/* DLCI 22 */
	{22, 22},		/* DLCI 23 */
	{23, 23},		/* DLCI 24 */
	{24, 24},		/* DLCI 25 */
	{25, 25},		/* DLCI 26 */
	{26, 26},		/* DLCI 27 */
	{27, 27},		/* DLCI 28 */
	{28, 28},		/* DLCI 29 */
	{29, 29},		/* DLCI 30 */
	{30, 30},		/* DLCI 31 */
	{31, 31},		/* DLCI 32 */
};

/* set line ring buffer num*/
static __u8 ringbuf_num[SPRDMUX_ID_MAX][NR_MUXS];

typedef struct {
	__u8 *buf;
	__u8 **frame;
	unsigned long flags;
	__u16 *length;
	__u8 need_notify;

	volatile __u8 dummy;	/* Allignment to 4*n bytes */
	struct mutex send_lock;
//	struct mutex send_data_lock; /*Todo*/
	__u32 read_index;
	__u32 write_index;

	wait_queue_head_t tx_wait;
} mux_send_struct;

struct mux_recv_packet_tag {
	__u8 *data;
	__u32 length;
	__u32 pos;
	struct mux_recv_packet_tag *next;
};
typedef struct mux_recv_packet_tag mux_recv_packet;

struct mux_recv_struct_tag {
	__u8 data[TS0710MUX_RECV_BUF_SIZE];
	__u32 length;
	__u32 total;
	__u32 pos;
	mux_recv_packet *mux_packet;
	struct mutex recv_lock;
	struct mutex recv_data_lock;
	wait_queue_head_t rx_wait;
};
typedef struct mux_recv_struct_tag mux_recv_struct;

struct notify {
	notify_func func;
	void *user_data;
};

struct sprd_mux {
	SPRDMUX_ID_E mux_id;
	int mux_exiting;
	mux_send_struct *mux_send_info[NR_MUXS];
	volatile __u8 mux_send_info_flags[NR_MUXS];
	mux_recv_struct *mux_recv_info[NR_MUXS];
	volatile __u8 mux_recv_info_flags[NR_MUXS];
	struct completion send_completion;

	struct task_struct *mux_recv_kthread;
	struct task_struct *mux_send_kthread;

	struct sprdmux *io_hal;
	ts0710_con *connection;
	volatile int cmux_mode;

	struct notify callback[NR_MUXS];
	struct mutex handshake_mutex;
	struct mutex open_mutex[NR_MUXS];
	volatile short int open_count[NR_MUXS];

	__u32 check_read_sum[NR_MUXS];
	__u32 check_write_sum[NR_MUXS];

	wait_queue_head_t handshake_ready;
	wait_queue_head_t modem_ready;
	struct task_struct *mux_recover_kthread;
	int mux_status;

/* receive_worker */
	unsigned char tbuf[TS0710MUX_MAX_BUF_SIZE];
	unsigned char *tbuf_ptr;
	unsigned char *start_flag;
	int framelen;
	/*For BP UART problem Begin */
	__u8 expect_seq;
	/*For BP UART problem End */
	struct timer_list watch_timer;

#ifdef TS0710DEBUG
	unsigned char debug_hex_buf[TS0710MUX_MAX_BUF_SIZE];
	unsigned char debug_str_buf[TS0710MUX_MAX_BUF_SIZE];
#endif

#ifdef TS0710LOG
	unsigned char debug_frame_buf[TS0710MUX_MAX_BUF_SIZE];
#endif

} ;

typedef struct mux_info_{
	SPRDMUX_ID_E mux_id;
	char mux_name[SPRDMUX_MAX_NAME_LEN];
	struct sprd_mux *handle;
	struct sprdmux mux;

} mux_info;

static mux_info sprd_mux_mgr[SPRDMUX_MAX_NUM] =
{
	[0] = {
			.mux_id = SPRDMUX_ID_SPI,
			.mux_name   = "spimux",
			.handle = NULL,
	},
	[1] = {
			.mux_id = SPRDMUX_ID_SDIO,
			.mux_name	= "sdiomux",
			.handle = NULL,
	},
};

static void mux_print_mux_mgr(void);
static void mux_mgr_init(mux_info *mux_mgr);
static int mux_receive_thread(void *data);
static void mux_display_recv_info(const char * tag, int mux_id, int line);
static void receive_worker(struct sprd_mux *self, int start);
static int mux_mode = 0;

#ifdef min
#undef min
#define min(a,b)    ( (a)<(b) ? (a):(b) )
#endif

static int send_ua(ts0710_con * ts0710, __u8 dlci);
static int send_dm(ts0710_con * ts0710, __u8 dlci);
static int send_sabm(ts0710_con * ts0710, __u8 dlci);
static int send_disc(ts0710_con * ts0710, __u8 dlci);
static void queue_uih(mux_send_struct * send_info, __u32 ring_index, __u16 len, ts0710_con * ts0710, __u8 dlci);
static int send_pn_msg(ts0710_con * ts0710, __u8 prior, __u32 frame_size, __u8 credit_flow, __u8 credits, __u8 dlci, __u8 cr);
static int send_nsc_msg(ts0710_con * ts0710, mcc_type cmd, __u8 cr);
static void set_uih_hdr(short_frame * uih_pkt, __u8 dlci, __u32 len, __u8 cr);
static void mux_sched_send(struct sprd_mux *self);
static int ts0710_ctrl_channel_status(ts0710_con * ts0710);
static __u32 crc_check(__u8 * data, __u32 length, __u8 check_sum);
static __u8 crc_calc(__u8 * data, __u32 length);
static void create_crctable(__u8 table[]);
static __u8 crctable[256];

/* mux recover */
static int mux_handshake(struct sprd_mux *self);
static void mux_stop(struct sprd_mux *self);
static int mux_restore_channel(ts0710_con * ts0710);
static void mux_wakeup_all_read(struct sprd_mux *self);
static void mux_wakeup_all_write(struct sprd_mux *self);
static void mux_wakeup_all_opening(struct sprd_mux *self);
static void mux_wakeup_all_closing(struct sprd_mux *self);
static void mux_wakeup_all_wait(struct sprd_mux *self);

static void mux_tidy_buff(struct sprd_mux *self);
static void mux_recover(struct sprd_mux *self);
static int mux_recover_thread(void *data);
static int mux_restore_channel(ts0710_con * ts0710);
static void mux_display_connection(ts0710_con * ts0710, const char *tag);

static void mux_set_ringbuf_num(void);
static int mux_line_state(SPRDMUX_ID_E mux_id, int line);
static mux_send_struct * mux_alloc_send_info(SPRDMUX_ID_E mux_id, int line);
int sprdmux_line_busy(SPRDMUX_ID_E mux_id, int line);
void sprdmux_set_line_notify(SPRDMUX_ID_E mux_id, int line, __u8 notify);
static void mux_free_send_info(mux_send_struct * send_info);
static void display_send_info(char * tag, SPRDMUX_ID_E mux_id, int line);

static void mux_init_timer(struct sprd_mux *self);
static void mux_start_timer(struct sprd_mux *self);
static void mux_wathch_check(unsigned long priv);
static void mux_stop_timer(struct sprd_mux *self);

#ifdef TS0710DEBUG

#define TS0710_DEBUG(fmt, arg...) printk(KERN_INFO "MUX:%s "fmt, __FUNCTION__ , ## arg)
#define MUX_TS0710_DEBUG(mux_id, fmt, arg...) \
do \
{if (mux_id >= 0 && mux_id < SPRDMUX_ID_MAX)\
printk(KERN_INFO "[%s]: %s "fmt,sprd_mux_mgr[mux_id].mux_name, __FUNCTION__ , ## arg); \
} while (0)
#else
#define TS0710_DEBUG(fmt...)
#define MUX_TS0710_DEBUG(mux_id, fmt, arg...)
#endif /* End #ifdef TS0710DEBUG */

#ifdef TS0710LOG
#define TS0710_PRINTK(fmt, arg...) printk(fmt, ## arg)
#else
#define TS0710_PRINTK(fmt, arg...) printk(fmt, ## arg)
#endif /* End #ifdef TS0710LOG */

#ifdef TS0710DEBUG
static void MUX_TS0710_DEBUGHEX(SPRDMUX_ID_E mux_id, __u8 * buf, int len)
{
	unsigned char *tbuf = NULL;
	int i = 0;
	int c = 0;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || !buf || len <= 0) {
		printk(KERN_ERR "MUX: Error %s Invalid Param\n", __FUNCTION__);
		return;
	}

	if (!sprd_mux_mgr[mux_id].handle) {
		printk(KERN_ERR "MUX[%d]: Error %s mux_id Handle is 0\n", mux_id,  __FUNCTION__);
		mux_print_mux_mgr();
		return;
	}

	tbuf = sprd_mux_mgr[mux_id].handle->debug_hex_buf;

	if (!tbuf) {
		printk(KERN_ERR "MUX[%d]: Error %s mux_id tbuf is NULL\n", mux_id,  __FUNCTION__);
		return;
	}

	for (i = 0; (i < len) && (c < (TS0710MUX_MAX_BUF_SIZE - 3)); i++) {
		sprintf(&tbuf[c], "%02x ", buf[i]);
		c += 3;
	}
	tbuf[c] = 0;

	MUX_TS0710_DEBUG(mux_id, "%s", tbuf);
}

static void MUX_TS0710_DEBUGSTR(SPRDMUX_ID_E mux_id, __u8 * buf, int len)
{
	unsigned char *tbuf = NULL;
	struct sprd_mux *self = NULL;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || !buf || len <= 0) {
		printk(KERN_ERR "MUX: Error %s Invalid Param\n", __FUNCTION__);
		return;
	}

	self = sprd_mux_mgr[mux_id].handle;
	if (!self) {
		printk(KERN_ERR "MUX[%d]: Error %s self is NULL\n", mux_id,  __FUNCTION__);
		mux_print_mux_mgr();
		return;
	}

	tbuf = self->debug_str_buf;

	if (!tbuf) {
		printk(KERN_ERR "MUX[%d]: Error %s mux_id tbuf is NULL\n", mux_id,  __FUNCTION__);
		return;
	}

	if (len > (TS0710MUX_MAX_BUF_SIZE - 1)) {
		len = (TS0710MUX_MAX_BUF_SIZE - 1);
	}

	memcpy(tbuf, buf, len);
	tbuf[len] = 0;

	/* 0x00 byte in the string pointed by tbuf may truncate the print result */
	MUX_TS0710_DEBUG(mux_id, "%s", tbuf);
}
#else
#define MUX_TS0710_DEBUGHEX(mux_id, buf, len)
#define MUX_TS0710_DEBUGSTR(mux_id, buf, len)
#endif /* End #ifdef TS0710DEBUG */

#ifdef TS0710LOG
static void MUX_TS0710_LOGSTR_FRAME(SPRDMUX_ID_E mux_id, __u8 send, __u8 * data, int len)
{
	unsigned char *tbuf = NULL;
	short_frame *short_pkt;
	long_frame *long_pkt;
	__u8 *uih_data_start;
	__u32 uih_len;
	__u8 dlci;
	int pos;
	struct sprd_mux *self = NULL;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || !data || len <= 0) {
		printk(KERN_ERR "MUX: Error %s Invalid Param\n", __FUNCTION__);
		return;
	}

	self = sprd_mux_mgr[mux_id].handle;
	if (!self) {
		printk(KERN_ERR "MUX[%d]: Error %s self is NULL\n", mux_id,  __FUNCTION__);
		return;
	}

	tbuf = self->debug_frame_buf;

	if (!tbuf) {
		printk(KERN_ERR "MUX[%d]: Error %s tbuf is NULL\n", mux_id,  __FUNCTION__);
		return;
	}

	if (len <= 0) {
		return;
	}

	pos = 0;
	if (send) {
		pos += sprintf(&tbuf[pos], "<");
		short_pkt = (short_frame *) (data + 1);
	} else {
		pos +=
		    sprintf(&tbuf[pos], ">%d ",
			    *(data + SLIDE_BP_SEQ_OFFSET));

#ifdef TS0710SEQ2
		pos +=
		    sprintf(&tbuf[pos], "%02x %02x %02x %02x ",
			    *(data + FIRST_BP_SEQ_OFFSET),
			    *(data + SECOND_BP_SEQ_OFFSET),
			    *(data + FIRST_AP_SEQ_OFFSET),
			    *(data + SECOND_AP_SEQ_OFFSET));
#endif

		short_pkt = (short_frame *) (data + ADDRESS_FIELD_OFFSET);
	}

	dlci = short_pkt->h.addr.server_chn << 1 | short_pkt->h.addr.d;
	switch (CLR_PF(short_pkt->h.control)) {
	case SABM:
		pos += sprintf(&tbuf[pos], "C SABM %d ::", dlci);
		break;
	case UA:
		pos += sprintf(&tbuf[pos], "C UA %d ::", dlci);
		break;
	case DM:
		pos += sprintf(&tbuf[pos], "C DM %d ::", dlci);
		break;
	case DISC:
		pos += sprintf(&tbuf[pos], "C DISC %d ::", dlci);
		break;

	case ACK:
		pos += sprintf(&tbuf[pos], "C ACK %d ", short_pkt->data[0]);

#ifdef TS0710SEQ2
		pos +=
		    sprintf(&tbuf[pos], "%02x %02x %02x %02x ",
			    short_pkt->data[1], short_pkt->data[2],
			    short_pkt->data[3], short_pkt->data[4]);
#endif

		pos += sprintf(&tbuf[pos], "::");
		break;

	case UIH:
		if (!dlci) {
			pos += sprintf(&tbuf[pos], "C MCC %d ::", dlci);
		} else {

			if ((short_pkt->h.length.ea) == 0) {
				long_pkt = (long_frame *) short_pkt;
				uih_len = GET_LONG_LENGTH(long_pkt->h.length);
				uih_data_start = long_pkt->h.data;
			} else {
				uih_len = short_pkt->h.length.len;
				uih_data_start = short_pkt->data;
			}
			switch (0) {
			case CMDTAG:
				pos +=
				    sprintf(&tbuf[pos], "I %d A %d ::", dlci,
					    uih_len);
				break;
			case DATATAG:
			default:
				pos +=
				    sprintf(&tbuf[pos], "I %d D %d ::", dlci,
					    uih_len);
				break;
			}

		}
		break;
	default:
		pos += sprintf(&tbuf[pos], "N!!! %d ::", dlci);
		break;
	}

	if (len > (TS0710MUX_MAX_BUF_SIZE - pos - 1)) {
		len = (TS0710MUX_MAX_BUF_SIZE - pos - 1);
	}

	memcpy(&tbuf[pos], data, len);
	pos += len;
	tbuf[pos] = 0;

	/* 0x00 byte in the string pointed by g_tbuf may truncate the print result */
	TS0710_PRINTK("[%s]: %s\n", sprd_mux_mgr[mux_id].mux_name, tbuf);
}
#else
#define MUX_TS0710_LOGSTR_FRAME(mux_id, send, data, len)
#endif

#ifdef TS0710SIG
#define my_for_each_task(p) \
        for ((p) = current; ((p) = (p)->next_task) != current; )

/* To be fixed */
static void TS0710_SIG2APLOGD(void)
{
	struct task_struct *p;
	static __u8 sig = 0;

	if (sig) {
		return;
	}

	read_lock(&tasklist_lock);
	my_for_each_task(p) {
		if (strncmp(p->comm, "aplogd", 6) == 0) {
			sig = 1;
			if (send_sig(SIGUSR2, p, 1) == 0) {
				TS0710_PRINTK
				    ("MUX: success to send SIGUSR2 to aplogd!\n");
			} else {
				TS0710_PRINTK
				    ("MUX: failure to send SIGUSR2 to aplogd!\n");
			}
			break;
		}
	}
	read_unlock(&tasklist_lock);

	if (!sig) {
		TS0710_PRINTK("MUX: not found aplogd!\n");
	}
}
#else
#define TS0710_SIG2APLOGD()
#endif

static int valid_dlci(__u8 dlci)
{
	if ((dlci < TS0710_MAX_CHN) && (dlci > 0))
		return 1;
	else
		return 0;
}

static int basic_write(ts0710_con * ts0710, __u8 * buf, int len)
{
	int res, send;
	struct sprd_mux *self = NULL;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	self = (struct sprd_mux *)ts0710->user_data;

	if (!self || !self->io_hal || !self->io_hal->io_write) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	buf[0] = TS0710_BASIC_FLAG;
	buf[len + 1] = TS0710_BASIC_FLAG;

	MUX_TS0710_LOGSTR_FRAME(self->mux_id, 1, buf, len + 2);
	MUX_TS0710_DEBUGHEX(self->mux_id, buf, len + 2);
	send = 0;

	while (send < len + 2) {
		res = self->io_hal->io_write(buf + send, len + 2 - send);
		if (res < 0)
			return -EIO;
		else if (res == 0)
			msleep(2);
		else
			send = send + res;
	}

	return len + 2;
}

/* Functions for the crc-check and calculation */

#define CRC_VALID 0xcf

static __u32 crc_check(__u8 * data, __u32 length, __u8 check_sum)
{
	__u8 fcs = 0xff;
#ifdef TS0710DEBUG
	__u32 len = length;
#endif
	length = length - 1;
	while (length--) {
		fcs = crctable[fcs ^ *data++];
	}
	fcs = crctable[fcs ^ check_sum];
	TS0710_DEBUG("fcs : %d\n", fcs);
	TS0710_DEBUG(" crc_check :len:%d check_sum:%d fcs : %d\n", len,
		     check_sum, fcs);
	if (fcs == (uint) 0xcf) {	/*CRC_VALID) */
		TS0710_DEBUG("crc_check: CRC check OK\n");
		return 0;
	} else {
		TS0710_PRINTK("MUX crc_check: CRC check failed\n");
		return 1;
	}
}

/* Calculates the checksum according to the ts0710 specification */

static __u8 crc_calc(__u8 * data, __u32 length)
{
	__u8 fcs = 0xff;

	while (length--) {
		fcs = crctable[fcs ^ *data++];
	}

	return 0xff - fcs;
}

/* Calulates a reversed CRC table for the FCS check */

static void create_crctable(__u8 table[])
{
	int i, j;

	__u8 data;
	__u8 code_word = (__u8) 0xe0;
	__u8 sr = (__u8) 0;

	for (j = 0; j < 256; j++) {
		data = (__u8) j;

		for (i = 0; i < 8; i++) {
			if ((data & 0x1) ^ (sr & 0x1)) {
				sr >>= 1;
				sr ^= code_word;
			} else {
				sr >>= 1;
			}

			data >>= 1;
			sr &= 0xff;
		}

		table[j] = sr;
		sr = 0;
	}
}

static void ts0710_reset_dlci(ts0710_con *ts0710, __u8 j)
{
	if (j >= TS0710_MAX_CHN)
		return;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	ts0710->dlci[j].state = DISCONNECTED;
	ts0710->dlci[j].flow_control = 0;
	ts0710->dlci[j].mtu = DEF_TS0710_MTU;
	ts0710->dlci[j].initiated = 0;
	ts0710->dlci[j].initiator = 0;
}

static void ts0710_reset_con(ts0710_con *ts0710)
{
	__u8 j;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	ts0710->initiator = 0;
	ts0710->mtu = DEF_TS0710_MTU + TS0710_MAX_HDR_SIZE;
	ts0710->be_testing = 0;
	ts0710->test_errs = 0;

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		ts0710_reset_dlci(ts0710, j);
	}
}

static void ts0710_init_waitqueue(ts0710_con *ts0710)
{
	__u8 j;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	init_waitqueue_head(&ts0710->test_wait);

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		init_waitqueue_head(&ts0710->dlci[j].open_wait);
		init_waitqueue_head(&ts0710->dlci[j].close_wait);
	}
}

static void ts0710_init(ts0710_con *ts0710)
{
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	ts0710_reset_con(ts0710);
}

static void ts0710_upon_disconnect(ts0710_con *ts0710)
{
	__u8 j;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		ts0710->dlci[j].state = DISCONNECTED;
		wake_up_all(&ts0710->dlci[j].open_wait);
		wake_up_interruptible(&ts0710->dlci[j].close_wait);
	}
	ts0710->be_testing = 0;
	wake_up_interruptible(&ts0710->test_wait);
	ts0710_reset_con(ts0710);
}

/* Sending packet functions */

/* Creates a UA packet and puts it at the beginning of the pkt pointer */

static int send_ua(ts0710_con * ts0710, __u8 dlci)
{
	__u8 buf[sizeof(short_frame) + FCS_SIZE + FLAG_SIZE];
	short_frame *ua;


	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (!ts0710->user_data) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "send_ua: Creating UA packet to DLCI %d\n", dlci);

	ua = (short_frame *) (buf + 1);
	ua->h.addr.ea = 1;
	ua->h.addr.cr = ((~(ts0710->initiator)) & 0x1);
	ua->h.addr.d = (dlci) & 0x1;
	ua->h.addr.server_chn = (dlci) >> 0x1;
	ua->h.control = SET_PF(UA);
	ua->h.length.ea = 1;
	ua->h.length.len = 0;
	ua->data[0] = crc_calc((__u8 *) ua, SHORT_CRC_CHECK);

	return basic_write(ts0710, buf, sizeof(short_frame) + FCS_SIZE);
}

/* Creates a DM packet and puts it at the beginning of the pkt pointer */

static int send_dm(ts0710_con * ts0710, __u8 dlci)
{
	__u8 buf[sizeof(short_frame) + FCS_SIZE + FLAG_SIZE];
	short_frame *dm;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (!ts0710->user_data) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "send_dm: Creating DM packet to DLCI %d\n", dlci);

	dm = (short_frame *) (buf + 1);
	dm->h.addr.ea = 1;
	dm->h.addr.cr = ((~(ts0710->initiator)) & 0x1);
	dm->h.addr.d = dlci & 0x1;
	dm->h.addr.server_chn = dlci >> 0x1;
	dm->h.control = SET_PF(DM);
	dm->h.length.ea = 1;
	dm->h.length.len = 0;
	dm->data[0] = crc_calc((__u8 *) dm, SHORT_CRC_CHECK);

	return basic_write(ts0710, buf, sizeof(short_frame) + FCS_SIZE);
}

static int send_sabm(ts0710_con * ts0710, __u8 dlci)
{
	__u8 buf[sizeof(short_frame) + FCS_SIZE + FLAG_SIZE];
	short_frame *sabm;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (!ts0710->user_data) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "send_sabm: Creating SABM packet to DLCI %d\n", dlci);

	sabm = (short_frame *) (buf + 1);
	sabm->h.addr.ea = 1;
	sabm->h.addr.cr = ((ts0710->initiator) & 0x1);
	sabm->h.addr.d = dlci & 0x1;
	sabm->h.addr.server_chn = dlci >> 0x1;
	sabm->h.control = SET_PF(SABM);
	sabm->h.length.ea = 1;
	sabm->h.length.len = 0;
	sabm->data[0] = crc_calc((__u8 *) sabm, SHORT_CRC_CHECK);

	return basic_write(ts0710, buf, sizeof(short_frame) + FCS_SIZE);
}

static int send_disc(ts0710_con * ts0710, __u8 dlci)
{
	__u8 buf[sizeof(short_frame) + FCS_SIZE + FLAG_SIZE];
	short_frame *disc;

	if (!ts0710)
	{
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (!ts0710->user_data)
	{
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "send_disc: Creating DISC packet to DLCI %d\n", dlci);

	disc = (short_frame *) (buf + 1);
	disc->h.addr.ea = 1;
	disc->h.addr.cr = ((ts0710->initiator) & 0x1);
	disc->h.addr.d = dlci & 0x1;
	disc->h.addr.server_chn = dlci >> 0x1;
	disc->h.control = SET_PF(DISC);
	disc->h.length.ea = 1;
	disc->h.length.len = 0;
	disc->data[0] = crc_calc((__u8 *) disc, SHORT_CRC_CHECK);

	return basic_write(ts0710, buf, sizeof(short_frame) + FCS_SIZE);
}

static void queue_uih(mux_send_struct * send_info, __u32 ring_index, __u16 len,
		      ts0710_con * ts0710, __u8 dlci)
{
	__u32 size;

	if (!ts0710 || !send_info) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	if (!ts0710->user_data) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	if (!send_info->frame) {
		printk(KERN_ERR "MUX: Error %s send_info->frame is NULL\n", __FUNCTION__);
		return;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id,"queue_uih: Creating UIH packet with %d bytes data to DLCI %d\n", len, dlci);

	if (len > SHORT_PAYLOAD_SIZE) {
		long_frame *l_pkt;

		size = sizeof(long_frame) + len + FCS_SIZE;
		l_pkt = (long_frame *) (send_info->frame[ring_index] - sizeof(long_frame));
		set_uih_hdr((void *)l_pkt, dlci, len, ts0710->initiator);
		l_pkt->data[len] = crc_calc((__u8 *) l_pkt, LONG_CRC_CHECK);
		send_info->frame[ring_index] = ((__u8 *) l_pkt) - 1;
	} else {
		short_frame *s_pkt;

		size = sizeof(short_frame) + len + FCS_SIZE;
		s_pkt = (short_frame *) (send_info->frame[ring_index] - sizeof(short_frame));
		set_uih_hdr((void *)s_pkt, dlci, len, ts0710->initiator);
		s_pkt->data[len] = crc_calc((__u8 *) s_pkt, SHORT_CRC_CHECK);
		send_info->frame[ring_index] = ((__u8 *) s_pkt) - 1;
	}

	if (!send_info->length) {
		printk(KERN_ERR "MUX: %s  pid[%d] send_info->length	NULL\n", __FUNCTION__, current->pid);
		return;
	}

	send_info->length[ring_index] = size;
}

/* Multiplexer command packets functions */

/* Turns on the ts0710 flow control */

static int ts0710_fcon_msg(ts0710_con * ts0710, __u8 cr)
{
	__u8 buf[30];
	mcc_short_frame *mcc_pkt;
	short_frame *uih_pkt;
	__u32 size;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	size = sizeof(short_frame) + sizeof(mcc_short_frame) + FCS_SIZE;
	uih_pkt = (short_frame *) (buf + 1);
	set_uih_hdr(uih_pkt, CTRL_CHAN, sizeof(mcc_short_frame), ts0710->initiator);
	uih_pkt->data[sizeof(mcc_short_frame)] = crc_calc((__u8 *) uih_pkt, SHORT_CRC_CHECK);
	mcc_pkt = (mcc_short_frame *) (uih_pkt->data);

	mcc_pkt->h.type.ea = EA;
	mcc_pkt->h.type.cr = cr;
	mcc_pkt->h.type.type = FCON;
	mcc_pkt->h.length.ea = EA;
	mcc_pkt->h.length.len = 0;

	return basic_write(ts0710, buf, size);
}

/* Turns off the ts0710 flow control */

static int ts0710_fcoff_msg(ts0710_con * ts0710, __u8 cr)
{
	__u8 buf[30];
	mcc_short_frame *mcc_pkt;
	short_frame *uih_pkt;
	__u32 size;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	size = (sizeof(short_frame) + sizeof(mcc_short_frame) + FCS_SIZE);
	uih_pkt = (short_frame *) (buf + 1);
	set_uih_hdr(uih_pkt, CTRL_CHAN, sizeof(mcc_short_frame), ts0710->initiator);
	uih_pkt->data[sizeof(mcc_short_frame)] = crc_calc((__u8 *) uih_pkt, SHORT_CRC_CHECK);
	mcc_pkt = (mcc_short_frame *) (uih_pkt->data);

	mcc_pkt->h.type.ea = 1;
	mcc_pkt->h.type.cr = cr;
	mcc_pkt->h.type.type = FCOFF;
	mcc_pkt->h.length.ea = 1;
	mcc_pkt->h.length.len = 0;

	return basic_write(ts0710, buf, size);
}

/* Sends an PN-messages and sets the not negotiable parameters to their
   default values in ts0710 */

static int send_pn_msg(ts0710_con * ts0710, __u8 prior, __u32 frame_size,
		       __u8 credit_flow, __u8 credits, __u8 dlci, __u8 cr)
{
	__u8 buf[30];
	pn_msg *pn_pkt;
	__u32 size;


	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (!ts0710->user_data) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id,
						"send_pn_msg: DLCI 0x%02x, prior:0x%02x, frame_size:%d, credit_flow:%x, credits:%d, cr:%x\n",
						dlci, prior, frame_size, credit_flow, credits, cr);

	size = sizeof(pn_msg);
	pn_pkt = (pn_msg *) (buf + 1);

	set_uih_hdr((void *)pn_pkt, CTRL_CHAN, size - (sizeof(short_frame) + FCS_SIZE), ts0710->initiator);
	pn_pkt->fcs = crc_calc((__u8 *) pn_pkt, SHORT_CRC_CHECK);

	pn_pkt->mcc_s_head.type.ea = 1;
	pn_pkt->mcc_s_head.type.cr = cr;
	pn_pkt->mcc_s_head.type.type = PN;
	pn_pkt->mcc_s_head.length.ea = 1;
	pn_pkt->mcc_s_head.length.len = 8;

	pn_pkt->res1 = 0;
	pn_pkt->res2 = 0;
	pn_pkt->dlci = dlci;
	pn_pkt->frame_type = 0;
	pn_pkt->credit_flow = credit_flow;
	pn_pkt->prior = prior;
	pn_pkt->ack_timer = 0;
	SET_PN_MSG_FRAME_SIZE(pn_pkt, frame_size);
	pn_pkt->credits = credits;
	pn_pkt->max_nbrof_retrans = 0;

	return basic_write(ts0710, buf, size);
}

/* Send a Not supported command - command, which needs 3 bytes */

static int send_nsc_msg(ts0710_con * ts0710, mcc_type cmd, __u8 cr)
{
	__u8 buf[30];
	nsc_msg *nsc_pkt;
	__u32 size;

	size = sizeof(nsc_msg);
	nsc_pkt = (nsc_msg *) (buf + 1);

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	set_uih_hdr((void *)nsc_pkt, CTRL_CHAN, sizeof(nsc_msg) - sizeof(short_frame) - FCS_SIZE, ts0710->initiator);

	nsc_pkt->fcs = crc_calc((__u8 *) nsc_pkt, SHORT_CRC_CHECK);

	nsc_pkt->mcc_s_head.type.ea = 1;
	nsc_pkt->mcc_s_head.type.cr = cr;
	nsc_pkt->mcc_s_head.type.type = NSC;
	nsc_pkt->mcc_s_head.length.ea = 1;
	nsc_pkt->mcc_s_head.length.len = 1;

	nsc_pkt->command_type.ea = 1;
	nsc_pkt->command_type.cr = cmd.cr;
	nsc_pkt->command_type.type = cmd.type;

	return basic_write(ts0710, buf, size);
}

static int ts0710_msc_msg(ts0710_con * ts0710, __u8 value, __u8 cr, __u8 dlci)
{
	__u8 buf[30];
	msc_msg *msc_pkt;
	__u32 size;

	size = sizeof(msc_msg);
	msc_pkt = (msc_msg *) (buf + 1);

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	set_uih_hdr((void *)msc_pkt, CTRL_CHAN, sizeof(msc_msg) - sizeof(short_frame) - FCS_SIZE, ts0710->initiator);

	msc_pkt->fcs = crc_calc((__u8 *) msc_pkt, SHORT_CRC_CHECK);

	msc_pkt->mcc_s_head.type.ea = 1;
	msc_pkt->mcc_s_head.type.cr = cr;
	msc_pkt->mcc_s_head.type.type = MSC;
	msc_pkt->mcc_s_head.length.ea = 1;
	msc_pkt->mcc_s_head.length.len = 2;

	msc_pkt->dlci.ea = 1;
	msc_pkt->dlci.cr = 1;
	msc_pkt->dlci.d = dlci & 1;
	msc_pkt->dlci.server_chn = (dlci >> 1) & 0x1f;

	msc_pkt->v24_sigs = value;

	return basic_write(ts0710, buf, size);
}

static int ts0710_test_msg(ts0710_con * ts0710, __u8 * test_pattern, __u32 len,
			   __u8 cr, __u8 * f_buf)
{
	__u32 size;

	if (!ts0710 || !test_pattern || !f_buf)
	{
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (len > SHORT_PAYLOAD_SIZE) {
		long_frame *uih_pkt;
		mcc_long_frame *mcc_pkt;

		size = (sizeof(long_frame) + sizeof(mcc_long_frame) + len + FCS_SIZE);
		uih_pkt = (long_frame *) (f_buf + 1);

		set_uih_hdr((short_frame *) uih_pkt, CTRL_CHAN, len + sizeof(mcc_long_frame), ts0710->initiator);
		uih_pkt->data[GET_LONG_LENGTH(uih_pkt->h.length)] = crc_calc((__u8 *) uih_pkt, LONG_CRC_CHECK);
		mcc_pkt = (mcc_long_frame *) uih_pkt->data;

		mcc_pkt->h.type.ea = EA;
		/* cr tells whether it is a commmand (1) or a response (0) */
		mcc_pkt->h.type.cr = cr;
		mcc_pkt->h.type.type = TEST;
		SET_LONG_LENGTH(mcc_pkt->h.length, len);
		memcpy(mcc_pkt->value, test_pattern, len);
	} else if (len > (SHORT_PAYLOAD_SIZE - sizeof(mcc_short_frame))) {
		long_frame *uih_pkt;
		mcc_short_frame *mcc_pkt;

		/* Create long uih packet and short mcc packet */
		size =
		    (sizeof(long_frame) + sizeof(mcc_short_frame) + len +
		     FCS_SIZE);
		uih_pkt = (long_frame *) (f_buf + 1);

		set_uih_hdr((short_frame *) uih_pkt, CTRL_CHAN, len + sizeof(mcc_short_frame), ts0710->initiator);
		uih_pkt->data[GET_LONG_LENGTH(uih_pkt->h.length)] = crc_calc((__u8 *) uih_pkt, LONG_CRC_CHECK);
		mcc_pkt = (mcc_short_frame *) uih_pkt->data;

		mcc_pkt->h.type.ea = EA;
		mcc_pkt->h.type.cr = cr;
		mcc_pkt->h.type.type = TEST;
		mcc_pkt->h.length.ea = EA;
		mcc_pkt->h.length.len = len;
		memcpy(mcc_pkt->value, test_pattern, len);
	} else {
		short_frame *uih_pkt;
		mcc_short_frame *mcc_pkt;

		size = (sizeof(short_frame) + sizeof(mcc_short_frame) + len + FCS_SIZE);
		uih_pkt = (short_frame *) (f_buf + 1);

		set_uih_hdr((void *)uih_pkt, CTRL_CHAN, len
			    + sizeof(mcc_short_frame), ts0710->initiator);
		uih_pkt->data[uih_pkt->h.length.len] =
		    crc_calc((__u8 *) uih_pkt, SHORT_CRC_CHECK);
		mcc_pkt = (mcc_short_frame *) uih_pkt->data;

		mcc_pkt->h.type.ea = EA;
		mcc_pkt->h.type.cr = cr;
		mcc_pkt->h.type.type = TEST;
		mcc_pkt->h.length.ea = EA;
		mcc_pkt->h.length.len = len;
		memcpy(mcc_pkt->value, test_pattern, len);

	}
	return basic_write(ts0710, f_buf, size);
}

static void set_uih_hdr(short_frame * uih_pkt, __u8 dlci, __u32 len, __u8 cr)
{
	uih_pkt->h.addr.ea = 1;
	uih_pkt->h.addr.cr = cr;
	uih_pkt->h.addr.d = dlci & 0x1;
	uih_pkt->h.addr.server_chn = dlci >> 1;
	uih_pkt->h.control = CLR_PF(UIH);

	if (len > SHORT_PAYLOAD_SIZE) {
		SET_LONG_LENGTH(((long_frame *) uih_pkt)->h.length, len);
	} else {
		uih_pkt->h.length.ea = 1;
		uih_pkt->h.length.len = len;
	}
}

/* Parses a multiplexer control channel packet */

static void process_mcc(__u8 * data, __u32 len, ts0710_con * ts0710, int longpkt)
{
	__u8 *tbuf = NULL;
	mcc_short_frame *mcc_short_pkt;
	struct sprd_mux *self = NULL;
	int j;

	TS0710_DEBUG("process_mcc\n");

	if (!ts0710 || !data) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	self = (struct sprd_mux *)ts0710->user_data;
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	if (longpkt) {
		mcc_short_pkt = (mcc_short_frame *) (((long_frame *) data)->data);
	} else {
		mcc_short_pkt = (mcc_short_frame *) (((short_frame *) data)->data);
	}

	switch (mcc_short_pkt->h.type.type) {
	case TEST:
		if (mcc_short_pkt->h.type.cr == MCC_RSP) {
			MUX_TS0710_DEBUG(self->mux_id, "Received test command response\n");

			if (ts0710->be_testing) {
				if ((mcc_short_pkt->h.length.ea) == 0) {
					mcc_long_frame *mcc_long_pkt;
					mcc_long_pkt = (mcc_long_frame *) mcc_short_pkt;
					if (GET_LONG_LENGTH
					    (mcc_long_pkt->h.length) !=
					    TEST_PATTERN_SIZE) {
						ts0710->test_errs = TEST_PATTERN_SIZE;
							MUX_TS0710_DEBUG(self->mux_id, "Err: received test pattern is %d bytes long, not expected %d\n", GET_LONG_LENGTH(mcc_long_pkt->h.length), TEST_PATTERN_SIZE);
					} else {
						ts0710->test_errs = 0;
						for (j = 0; j < TEST_PATTERN_SIZE; j++) {
							if (mcc_long_pkt->value[j] != (j & 0xFF)) {
								(ts0710->test_errs)++;
							}
						}
					}

				} else {

#if TEST_PATTERN_SIZE < 128
					if (mcc_short_pkt->h.length.len != TEST_PATTERN_SIZE) {
#endif

						ts0710->test_errs = TEST_PATTERN_SIZE;
							MUX_TS0710_DEBUG(self->mux_id, "Err: received test pattern is %d bytes long, not expected %d\n", mcc_short_pkt->h.length.len, TEST_PATTERN_SIZE);

#if TEST_PATTERN_SIZE < 128
					} else {
						ts0710->test_errs = 0;
						for (j = 0; j < TEST_PATTERN_SIZE; j++) {
							if (mcc_short_pkt->value[j] != (j & 0xFF)) {
								(ts0710->test_errs)++;
							}
						}
					}
#endif

				}

				ts0710->be_testing = 0;	/* Clear the flag */
				wake_up_interruptible(&ts0710->test_wait);
			} else {
				MUX_TS0710_DEBUG(self->mux_id, "Err: shouldn't or late to get test cmd response\n");
			}
		} else {
			tbuf = (__u8 *) kmalloc(len + 32, GFP_ATOMIC);
			if (!tbuf) {
				break;
			}

			if ((mcc_short_pkt->h.length.ea) == 0) {
				mcc_long_frame *mcc_long_pkt;
				mcc_long_pkt = (mcc_long_frame *) mcc_short_pkt;
				ts0710_test_msg(ts0710, mcc_long_pkt->value,
						GET_LONG_LENGTH(mcc_long_pkt->h.
								length),
						MCC_RSP, tbuf);
			} else {
				ts0710_test_msg(ts0710, mcc_short_pkt->value,
						mcc_short_pkt->h.length.len,
						MCC_RSP, tbuf);
			}

			kfree(tbuf);
		}
		break;

	case FCON:		/*Flow control on command */
		MUX_TS0710_DEBUG(self->mux_id, "MUX Received Flow control(all channels) on command\n");
		if (mcc_short_pkt->h.type.cr == MCC_CMD) {
			ts0710->dlci[0].state = CONNECTED;
			ts0710_fcon_msg(ts0710, MCC_RSP);
			mux_sched_send(self);
		}
		break;

	case FCOFF:		/*Flow control off command */
		MUX_TS0710_DEBUG(self->mux_id, "MUX Received Flow control(all channels) off command\n");
		if (mcc_short_pkt->h.type.cr == MCC_CMD) {
			for (j = 0; j < TS0710_MAX_CHN; j++) {
				ts0710->dlci[j].state = FLOW_STOPPED;
			}
			ts0710_fcoff_msg(ts0710, MCC_RSP);
		}
		break;

	case MSC:		/*Modem status command */
		{
			__u8 dlci;
			__u8 v24_sigs;

			dlci = (mcc_short_pkt->value[0]) >> 2;
			v24_sigs = mcc_short_pkt->value[1];

			if ((ts0710->dlci[dlci].state != CONNECTED)
			    && (ts0710->dlci[dlci].state != FLOW_STOPPED)) {
			        MUX_TS0710_DEBUG(self->mux_id, "Received Modem status dlci = %d\n",dlci);
				send_dm(ts0710, dlci);
				break;
			}
			if (mcc_short_pkt->h.type.cr == MCC_CMD) {
				MUX_TS0710_DEBUG(self->mux_id, "Received Modem status command\n");
				if (v24_sigs & 2) {
					if (ts0710->dlci[dlci].state ==
					    CONNECTED) {
						MUX_TS0710_DEBUG(self->mux_id, "MUX Received Flow off on dlci %d\n",
						     dlci);
						ts0710->dlci[dlci].state =
						    FLOW_STOPPED;
					}
				} else {
					if (ts0710->dlci[dlci].state ==
					    FLOW_STOPPED) {
						ts0710->dlci[dlci].state =
						    CONNECTED;
						MUX_TS0710_DEBUG(self->mux_id, "MUX Received Flow on on dlci %d\n",
						     dlci);
						mux_sched_send(self);
					}
				}

				ts0710_msc_msg(ts0710, v24_sigs, MCC_RSP, dlci);

			} else {
				MUX_TS0710_DEBUG(self->mux_id, "Received Modem status response\n");

				if (v24_sigs & 2) {
					MUX_TS0710_DEBUG(self->mux_id, "Flow stop accepted\n");
				}
			}
			break;
		}

	case PN:		/*DLC parameter negotiation */
		{
			__u8 dlci;
			__u16 frame_size;
			pn_msg *pn_pkt;

			pn_pkt = (pn_msg *) data;
			dlci = pn_pkt->dlci;
			frame_size = GET_PN_MSG_FRAME_SIZE(pn_pkt);
			MUX_TS0710_DEBUG(self->mux_id, "Received DLC parameter negotiation, PN\n");
			if (pn_pkt->mcc_s_head.type.cr == MCC_CMD) {
				MUX_TS0710_DEBUG(self->mux_id, "received PN command with:\n");
				MUX_TS0710_DEBUG(self->mux_id, "Frame size:%d\n", frame_size);

				frame_size =
				    min(frame_size, ts0710->dlci[dlci].mtu);
				send_pn_msg(ts0710, pn_pkt->prior, frame_size,
					    0, 0, dlci, MCC_RSP);
				ts0710->dlci[dlci].mtu = frame_size;
				MUX_TS0710_DEBUG(self->mux_id, "process_mcc : mtu set to %d\n",
					     ts0710->dlci[dlci].mtu);
			} else {
				MUX_TS0710_DEBUG(self->mux_id, "received PN response with:\n");
				MUX_TS0710_DEBUG(self->mux_id, "Frame size:%d\n", frame_size);

				frame_size =
				    min(frame_size, ts0710->dlci[dlci].mtu);
				ts0710->dlci[dlci].mtu = frame_size;

				MUX_TS0710_DEBUG(self->mux_id, 
				    "process_mcc : mtu set on dlci:%d to %d\n",
				     dlci, ts0710->dlci[dlci].mtu);

				if (ts0710->dlci[dlci].state == NEGOTIATING) {
					ts0710->dlci[dlci].state = CONNECTING;
					wake_up_all(&ts0710->dlci[dlci].open_wait);
				}
			}
			break;
		}

	case NSC:		/*Non supported command resonse */
		MUX_TS0710_DEBUG(self->mux_id, "MUX Received Non supported command response\n");
		break;

	default:		/*Non supported command received */
		MUX_TS0710_DEBUG(self->mux_id, "MUX Received a non supported command\n");
		send_nsc_msg(ts0710, mcc_short_pkt->h.type, MCC_RSP);
		break;
	}
}

static mux_recv_packet *get_mux_recv_packet(__u32 size)
{
	mux_recv_packet *recv_packet;

	recv_packet = (mux_recv_packet *) kmalloc(sizeof(mux_recv_packet), GFP_ATOMIC);
	if (!recv_packet) {
		return 0;
	}

	recv_packet->data = (__u8 *) kmalloc(size, GFP_ATOMIC);
	if (!(recv_packet->data)) {
		kfree(recv_packet);
		return 0;
	}
	recv_packet->length = 0;
	recv_packet->next = 0;
	recv_packet->pos = 0;

	return recv_packet;
}

static void free_mux_recv_packet(mux_recv_packet * recv_packet)
{
	if (!recv_packet) {
		return;
	}

	if (recv_packet->data) {
		kfree(recv_packet->data);
	}
	kfree(recv_packet);
}

static void free_mux_recv_struct(mux_recv_struct * recv_info)
{
	mux_recv_packet *recv_packet1, *recv_packet2;

	if (!recv_info) {
		return;
	}

	recv_packet1 = recv_info->mux_packet;
	while (recv_packet1) {
		recv_packet2 = recv_packet1->next;
		free_mux_recv_packet(recv_packet1);
		recv_packet1 = recv_packet2;
	}

	kfree(recv_info);
}

static int ts0710_recv_data(ts0710_con * ts0710, char *data, int len)
{
	short_frame *short_pkt;
	long_frame *long_pkt;
	__u8 *uih_data_start;
	__u32 uih_len;
	__u8 dlci;
	__u8 be_connecting;
	struct sprd_mux *self = NULL;

	if (!ts0710 || !data) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	self = (struct sprd_mux *)ts0710->user_data;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	short_pkt = (short_frame *) data;

	dlci = short_pkt->h.addr.server_chn << 1 | short_pkt->h.addr.d;
	MUX_TS0710_DEBUG(self->mux_id, "MUX ts0710_recv_data dlci = %d\n",dlci);
	switch (CLR_PF(short_pkt->h.control)) {
	case SABM:
		MUX_TS0710_DEBUG(self->mux_id, "SABM-packet received\n");

		if (!dlci) {
			MUX_TS0710_DEBUG(self->mux_id, "server channel == 0\n");
			ts0710->dlci[0].state = CONNECTED;

			MUX_TS0710_DEBUG(self->mux_id, "sending back UA - control channel\n");
			send_ua(ts0710, dlci);
			wake_up_all(&ts0710->dlci[0].open_wait);

		} else if (valid_dlci(dlci)) {

			MUX_TS0710_DEBUG(self->mux_id, "Incomming connect on channel %d\n", dlci);

			MUX_TS0710_DEBUG(self->mux_id, "sending UA, dlci %d\n", dlci);
			send_ua(ts0710, dlci);

			ts0710->dlci[dlci].state = CONNECTED;
			wake_up_all(&ts0710->dlci[dlci].open_wait);

		} else {
			MUX_TS0710_DEBUG(self->mux_id, "invalid dlci %d, sending DM\n", dlci);
			send_dm(ts0710, dlci);
		}

		break;

	case UA:
		printk(KERN_INFO "MUX: id[%d] dlci = %d UA packet received\n", self->mux_id, dlci);

		if (!dlci) {
			MUX_TS0710_DEBUG(self->mux_id, "server channel == 0|dlci[0].state=%d\n",
				     ts0710->dlci[0].state);

			if (ts0710->dlci[0].state == CONNECTING) {
				ts0710->dlci[0].state = CONNECTED;
				wake_up_all(&ts0710->dlci[0].open_wait);
			} else if (ts0710->dlci[0].state == DISCONNECTING) {
				ts0710_upon_disconnect(ts0710);
			} else {
				MUX_TS0710_DEBUG(self->mux_id, " Something wrong receiving UA packet\n");
			}
		} else if (valid_dlci(dlci)) {
			MUX_TS0710_DEBUG(self->mux_id, "Incomming UA on channel %d|dlci[%d].state=%d\n",
			     dlci, dlci, ts0710->dlci[dlci].state);

			if (ts0710->dlci[dlci].state == CONNECTING) {
				ts0710->dlci[dlci].state = CONNECTED;
				wake_up_all(&ts0710->dlci[dlci].open_wait);
			} else if (ts0710->dlci[dlci].state == DISCONNECTING) {
				ts0710->dlci[dlci].state = DISCONNECTED;
				wake_up_all(&ts0710->dlci[dlci].open_wait);
				wake_up_interruptible(&ts0710->dlci[dlci].close_wait);
				ts0710_reset_dlci(ts0710, dlci);
			} else {
				/* Should NOT be here */
				ts0710->dlci[dlci].state = DISCONNECTED;
				wake_up_all(&ts0710->dlci[dlci].open_wait);
				wake_up_interruptible(&ts0710->dlci[dlci].close_wait);
				ts0710_reset_dlci(ts0710, dlci);

				printk(KERN_ERR "MUX: id[%d] dlci[%d] wrong receiving UA packet state = %d\n", self->mux_id, dlci, ts0710->dlci[dlci].state);
			}
		} else {
			MUX_TS0710_DEBUG(self->mux_id, "invalid dlci %d\n", dlci);
		}

		break;

	case DM:
		MUX_TS0710_DEBUG(self->mux_id, "DM packet received\n");

		if (!dlci) {
			MUX_TS0710_DEBUG(self->mux_id, "server channel == 0\n");

			if (ts0710->dlci[0].state == CONNECTING) {
				be_connecting = 1;
			} else {
				be_connecting = 0;
			}
			ts0710_upon_disconnect(ts0710);
			if (be_connecting) {
				ts0710->dlci[0].state = REJECTED;
			}
		} else if (valid_dlci(dlci)) {
			MUX_TS0710_DEBUG(self->mux_id, "Incomming DM on channel %d\n", dlci);

			if (ts0710->dlci[dlci].state == CONNECTING) {
				ts0710->dlci[dlci].state = REJECTED;
			} else {
				ts0710->dlci[dlci].state = DISCONNECTED;
			}
			wake_up_all(&ts0710->dlci[dlci].open_wait);
			wake_up_interruptible(&ts0710->dlci[dlci].close_wait);
			ts0710_reset_dlci(ts0710, dlci);
		} else {
			MUX_TS0710_DEBUG(self->mux_id, "invalid dlci %d\n", dlci);
		}

		break;
	case DISC:
		MUX_TS0710_DEBUG(self->mux_id, "DISC packet received\n");

		if (!dlci) {
			MUX_TS0710_DEBUG(self->mux_id, "server channel == 0\n");

			send_ua(ts0710, dlci);
			MUX_TS0710_DEBUG(self->mux_id, "DISC, sending back UA\n");

			ts0710_upon_disconnect(ts0710);
		} else if (valid_dlci(dlci)) {
			MUX_TS0710_DEBUG(self->mux_id, "Incomming DISC on channel %d\n", dlci);

			send_ua(ts0710, dlci);
			MUX_TS0710_DEBUG(self->mux_id, "DISC, sending back UA\n");

			ts0710->dlci[dlci].state = DISCONNECTED;
			wake_up_all(&ts0710->dlci[dlci].open_wait);
			wake_up_interruptible(&ts0710->dlci[dlci].close_wait);
			ts0710_reset_dlci(ts0710, dlci);
		} else {
			MUX_TS0710_DEBUG(self->mux_id, "invalid dlci %d\n", dlci);
		}

		break;

	case UIH:
		MUX_TS0710_DEBUG(self->mux_id, "UIH packet received\n");

		if ((dlci >= TS0710_MAX_CHN)) {
			MUX_TS0710_DEBUG(self->mux_id, "invalid dlci %d\n", dlci);
			send_dm(ts0710, dlci);
			break;
		}

		if (GET_PF(short_pkt->h.control)) {
			MUX_TS0710_DEBUG(self->mux_id, "Error UIH packet with P/F set, discard it!\n");
			break;
		}

		if ((ts0710->dlci[dlci].state != CONNECTED)
		    && (ts0710->dlci[dlci].state != FLOW_STOPPED)) {
			MUX_TS0710_DEBUG(self->mux_id, "Error DLCI %d not connected, discard it!\n", dlci);
			send_dm(ts0710, dlci);
			break;
		}

		if ((short_pkt->h.length.ea) == 0) {
			MUX_TS0710_DEBUG(self->mux_id, "Long UIH packet received\n");
			long_pkt = (long_frame *) data;
			uih_len = GET_LONG_LENGTH(long_pkt->h.length);
			uih_data_start = long_pkt->h.data;
			MUX_TS0710_DEBUG(self->mux_id, "long packet length %d\n", uih_len);

		} else {
			MUX_TS0710_DEBUG(self->mux_id, "Short UIH pkt received\n");
			uih_len = short_pkt->h.length.len;
			uih_data_start = short_pkt->data;

		}

		if (dlci == 0) {
			MUX_TS0710_DEBUG(self->mux_id, "UIH on serv_channel 0\n");
			process_mcc(data, len, ts0710,
				    !(short_pkt->h.length.ea));
		} else if (valid_dlci(dlci)) {
			/* do line dispatch */
			__u8 line;
			mux_recv_struct *recv_info;

			mux_recv_packet *recv_packet, *recv_packet2;
			MUX_TS0710_DEBUG(self->mux_id, "UIH on channel %d\n", dlci);

			if (uih_len > ts0710->dlci[dlci].mtu) {
				MUX_TS0710_DEBUG(self->mux_id, "MUX Error:  DLCI:%d, uih_len:%d is bigger than mtu:%d, discard data!\n",
				     dlci, uih_len, ts0710->dlci[dlci].mtu);
				break;
			}

			line = dlci2line[dlci].dataline;	/* we see data and commmand as same */

			if (self->callback[line].func) {
				MUX_TS0710_DEBUG(self->mux_id, "MUX: callback on mux%d", line);
				if (!(*self->callback[line].func)(line, SPRDMUX_EVENT_COMPLETE_READ, uih_data_start, uih_len, self->callback[line].user_data)) {
					break;
				}
			}

			if ((!self->open_count[line])) {
				MUX_TS0710_DEBUG(self->mux_id, "MUX: No application waiting for, discard it! /dev/mux%d\n", line);
			} else {	/* Begin processing received data */
				if ((!self->mux_recv_info_flags[line])
					|| (!self->mux_recv_info[line])) {
					MUX_TS0710_DEBUG(self->mux_id, "MUX Error: No mux_recv_info, discard it! /dev/mux%d\n", line);
					break;
				}

				recv_info = self->mux_recv_info[line];
				if(!recv_info) {
					MUX_TS0710_DEBUG(self->mux_id, "recv_data : recv_info is null\n");
					break;
				}

				if (recv_info->total > TS0710MUX_MAX_TOTAL_SIZE) {
					MUX_TS0710_DEBUG(self->mux_id, KERN_INFO "MUX : discard data for line:%d, recv_info->total = %d!\n", line, recv_info->total);
					break;
				}

				if (line <= MUX_RIL_LINE_END) {
					/* Just display AT line */
					printk(KERN_ERR "MUX: id[%d] line[%d] received %d data\n", self->mux_id, line, uih_len);
				}

				mutex_lock(&recv_info->recv_data_lock);

				if (recv_info->total != 0 || recv_info->length != 0) { /* add new data to tail */
					/* recv_info is already linked into mux_recv_queue */
					recv_packet = get_mux_recv_packet(uih_len);
					if (!recv_packet) {
						MUX_TS0710_DEBUG(self->mux_id, "MUX: no memory\n");
						mutex_unlock(&recv_info->recv_data_lock);
						break;
					}

					memcpy(recv_packet->data, uih_data_start, uih_len);
					recv_packet->length = uih_len;
					recv_info->total += uih_len;
					self->check_read_sum[line] += uih_len;

					recv_packet->next = NULL;

					if (!(recv_info->mux_packet)) {
						recv_info->mux_packet = recv_packet;
					} else {
						recv_packet2 = recv_info->mux_packet;
						while (recv_packet2->next) {
							recv_packet2 = recv_packet2->next;
						}
						recv_packet2->next = recv_packet;
					}	/* End if( !(recv_info->mux_packet) ) */
				} else {
					if (uih_len > TS0710MUX_RECV_BUF_SIZE) {
						MUX_TS0710_DEBUG(self->mux_id, "MUX Error:  line:%d, uih_len == %d is too big\n", line, uih_len);
						uih_len = TS0710MUX_RECV_BUF_SIZE;
					}
					memcpy(recv_info->data, uih_data_start, uih_len);
					recv_info->length = uih_len;
					recv_info->total = uih_len;
					recv_info->pos = 0;
					self->check_read_sum[line] += uih_len;
					wake_up_interruptible(&recv_info->rx_wait);
				}

				mutex_unlock(&recv_info->recv_data_lock);
			}	/* End processing received data */
		} else {
			MUX_TS0710_DEBUG(self->mux_id, "invalid dlci %d\n", dlci);
		}

		break;

	default:
		MUX_TS0710_DEBUG(self->mux_id, "illegal packet\n");
		break;
	}
	return 0;
}

/* Close ts0710 channel */
static void ts0710_close_channel(ts0710_con *ts0710, __u8 dlci)
{
	int try;
	unsigned long t;
	struct sprd_mux *self = NULL;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	self = (struct sprd_mux *)ts0710->user_data;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "ts0710_disc_command on channel %d\n", dlci);

	if ((ts0710->dlci[dlci].state == DISCONNECTED)
	    || (ts0710->dlci[dlci].state == REJECTED)) {
		return;
	} else if (ts0710->dlci[dlci].state == DISCONNECTING) {
		/* Reentry */
		return;
	} else {
		ts0710->dlci[dlci].state = DISCONNECTING;
		if (self->mux_status != MUX_STATE_READY) {
			printk(KERN_ERR "MUX: Error %s cp is Not OK\n", __FUNCTION__);
			ts0710->dlci[dlci].state = DISCONNECTED;
			return;
		}

		try = 1;
		while (try) {
			--try;
			t = jiffies;
			send_disc(ts0710, dlci);
			interruptible_sleep_on_timeout(&ts0710->dlci[dlci].close_wait, 10 * TS0710MUX_TIME_OUT);

			if (ts0710->dlci[dlci].state == DISCONNECTED) {
				break;
			} else if (ts0710->dlci[dlci].state == CONNECTED || ts0710->dlci[dlci].state == CONNECTING) {
				/* Todo:Should NOT be here */
				printk(KERN_ERR "MUX id[%d] DLCI:%d opening occured!\n", self->mux_id, dlci);
				break;
			}
			else if (signal_pending(current)) {
				MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "MUX DLCI %d Send DISC got signal!\n", dlci);
				break;
			} else if ((jiffies - t) >= TS0710MUX_TIME_OUT) {
				MUX_TS0710_DEBUG(((struct sprd_mux *)ts0710->user_data)->mux_id, "MUX DLCI %d Send DISC timeout!\n", dlci);
				break;
			}
		}
		if (ts0710->dlci[dlci].state != DISCONNECTED) {
			if (dlci == 0) {	/* Control Channel */
				ts0710_upon_disconnect(ts0710);
			} else {	/* Other Channel */
				ts0710->dlci[dlci].state = DISCONNECTED;
				wake_up_interruptible(&ts0710->dlci[dlci].close_wait);
				ts0710_reset_dlci(ts0710, dlci);
			}
		}
	}
}

static int ts0710_open_channel(ts0710_con *ts0710, __u8 dlci)
{
	int try;
	int retval;
	struct sprd_mux *self = NULL;

	retval = -ENODEV;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return retval;
	}

	self = (struct sprd_mux *)ts0710->user_data;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return retval;
	}

	TS0710_PRINTK("MUX %s id[%d] DLCI:%d state = %d dlci[0]'state = %d\n", __FUNCTION__, self->mux_id, dlci, ts0710->dlci[dlci].state, ts0710->dlci[0].state);

	if (dlci == 0) {	/* control channel */
		if ((ts0710->dlci[0].state == CONNECTED)
		    || (ts0710->dlci[0].state == FLOW_STOPPED)) {
			return 0;
		} else if (ts0710->dlci[0].state == CONNECTING) {
			/* Reentry */
			printk(KERN_INFO "MUX: id[%d] DLCI: 0, reentry to open DLCI 0, pid: %d, %s !\n",self->mux_id, current->pid, current->comm);
			try = 1;/* No try*/
			while (try) {
				--try;
				wait_event_interruptible(ts0710->dlci[0].open_wait, ts0710->dlci[0].state != CONNECTING || self->mux_status == MUX_STATE_CRASHED);
				TS0710_PRINTK("MUX: id[%d] DLCI:%d after open wait state = %d try = %d", self->mux_id, dlci, ts0710->dlci[dlci].state, try);

				if (self->mux_status == MUX_STATE_CRASHED) {
					printk(KERN_INFO "MUX: id[%d] Error DLCI[%d] cp is crashed!\n", self->mux_id, dlci);
				//Need check
					return -ENODEV;
				}

				if ((ts0710->dlci[0].state == CONNECTED)
				    || (ts0710->dlci[0].state == FLOW_STOPPED)) {
					retval = 0;
					break;
				} else if (ts0710->dlci[0].state == REJECTED) {
					printk(KERN_ERR "MUX: id[%d] Error DLCI[%d] REJECTED\n", self->mux_id, dlci);
					retval = -EREJECTED;
					break;
				} else if (ts0710->dlci[0].state == DISCONNECTED) {
					printk(KERN_ERR "MUX: id[%d] Error DLCI[%d] DISCONNECTED\n", self->mux_id, dlci);
					retval = -EAGAIN;
					break;
				} else if (signal_pending(current)) {
					printk(KERN_ERR "MUX: id[%d] DLCI:%d Wait for connecting got signal!\n", self->mux_id, dlci);
					retval = -EAGAIN;
					break;

				} else if (ts0710->dlci[0].state == CONNECTING) {
					printk(KERN_ERR "MUX: id[%d] DLCI:%d error out 6\n", self->mux_id, dlci);
					break;
				}
			}

			if (ts0710->dlci[0].state == CONNECTING) {
				ts0710->dlci[0].state = DISCONNECTED;
				retval = -EAGAIN;
			}
		} else if ((ts0710->dlci[0].state != DISCONNECTED)
			   && (ts0710->dlci[0].state != REJECTED)) {
			printk(KERN_ERR "MUX: id[%d] DLCI:%d state is invalid!\n", self->mux_id, dlci);
			return retval;
		} else {
			ts0710->initiator = 1;
			ts0710->dlci[0].state = CONNECTING;
			ts0710->dlci[0].initiator = 1;
			try = 1;/* No try*/
			while (try) {

				--try;
				send_sabm(ts0710, 0);
				wait_event_interruptible(ts0710->dlci[0].open_wait, ts0710->dlci[0].state != CONNECTING || self->mux_status == MUX_STATE_CRASHED);

				TS0710_PRINTK("MUX: id[%d] DLCI:%d after open wait state = %d try = %d", self->mux_id, dlci, ts0710->dlci[dlci].state, try);

				if (self->mux_status == MUX_STATE_CRASHED) {
					printk(KERN_INFO "MUX: id[%d] Error DLCI[%d] cp is crashed!\n", self->mux_id, dlci);
				//Need check
					return -ENODEV;
				}

				if ((ts0710->dlci[0].state == CONNECTED)
				    || (ts0710->dlci[0].state == FLOW_STOPPED)) {
					retval = 0;
					break;
				} else if (ts0710->dlci[0].state == REJECTED) {
					printk(KERN_ERR "MUX: id[%d] DLCI:%d Send SABM got rejected!\n",self->mux_id, dlci);
					retval = -EREJECTED;
					break;
				} else if (signal_pending(current)) {
					printk(KERN_ERR "MUX: id[%d] DLCI:%d Send SABM got signal!\n", self->mux_id, dlci);
					retval = -EAGAIN;
					break;
				}
			}

			if (ts0710->dlci[0].state == CONNECTING) {
				ts0710->dlci[0].state = DISCONNECTED;
				retval = -EAGAIN;
			}
			wake_up_all(&ts0710->dlci[0].open_wait);
		}
	} else {	/* other channel */
		if ((ts0710->dlci[0].state != CONNECTED)
		    && (ts0710->dlci[0].state != FLOW_STOPPED)) {
			return retval;
		} else if ((ts0710->dlci[dlci].state == CONNECTED)
			   || (ts0710->dlci[dlci].state == FLOW_STOPPED)) {
			return 0;
		} else if ((ts0710->dlci[dlci].state == NEGOTIATING)
			   || (ts0710->dlci[dlci].state == CONNECTING)) {
			/* Reentry */
			try = 1;/* No try*/
			while (try) {

				--try;
				wait_event_interruptible(ts0710->dlci[dlci].open_wait, (ts0710->dlci[dlci].state != CONNECTING || ts0710->dlci[dlci].state!= NEGOTIATING || self->mux_status == MUX_STATE_CRASHED));
				TS0710_PRINTK("MUX: id[%d] DLCI:%d after open wait state = %d try = %d", self->mux_id, dlci, ts0710->dlci[dlci].state, try);

				if (self->mux_status == MUX_STATE_CRASHED) {
					printk(KERN_INFO "MUX: id[%d] Error DLCI[%d] cp is crashed!\n", self->mux_id, dlci);
				//Need check
					return -ENODEV;
				}

				if ((ts0710->dlci[dlci].state == CONNECTED)
				    || (ts0710->dlci[dlci].state == FLOW_STOPPED)) {
					retval = 0;
					break;
				} else if (ts0710->dlci[dlci].state == REJECTED) {
					retval = -EREJECTED;
					printk(KERN_ERR "MUX: id[%d] DLCI:%d Send SABM got rejected!\n",self->mux_id, dlci);
					break;
				} else if (ts0710->dlci[dlci].state == DISCONNECTED) {
					printk(KERN_ERR "MUX: id[%d] Error DLCI[%d] DISCONNECTED\n", self->mux_id, dlci);
					break;
				} else if (signal_pending(current)) {
					printk(KERN_ERR "MUX: id[%d] DLCI:%d Wait for connecting got signal!\n", self->mux_id, dlci);
					retval = -EAGAIN;
					break;
				} else if ((ts0710->dlci[dlci].state == NEGOTIATING)
					|| (ts0710->dlci[dlci].state == CONNECTING)) {
					printk(KERN_ERR "MUX: id[%d] DLCI:%d CONNECTING or NEGOTIATING\n", self->mux_id, dlci);
					break;
				}
			}

			if ((ts0710->dlci[dlci].state == NEGOTIATING)
			    || (ts0710->dlci[dlci].state == CONNECTING)) {
				ts0710->dlci[dlci].state = DISCONNECTED;
				retval = -EAGAIN;
			}
		} else if ((ts0710->dlci[dlci].state != DISCONNECTED)
			   && (ts0710->dlci[dlci].state != REJECTED)) {
			return retval;
		} else {
			ts0710->dlci[dlci].state = CONNECTING;
			ts0710->dlci[dlci].initiator = 1;
			if (ts0710->dlci[dlci].state == CONNECTING) {
				try = 1;
				while (try) {

					--try;
					send_sabm(ts0710, dlci);
					wait_event_interruptible(ts0710->dlci[dlci].open_wait,ts0710->dlci[dlci].state != CONNECTING || self->mux_status == MUX_STATE_CRASHED);

					TS0710_PRINTK("MUX: id[%d] DLCI:%d after open wait state = %d try = %d", self->mux_id, dlci, ts0710->dlci[dlci].state, try);

					if (self->mux_status == MUX_STATE_CRASHED) {
						printk(KERN_INFO "MUX: id[%d] Error DLCI[%d] cp is crashed!\n", self->mux_id, dlci);
						//Need check
						return -ENODEV;
					}

					if ((ts0710->dlci[dlci].state == CONNECTED)
					    || (ts0710->dlci[dlci].state ==FLOW_STOPPED)) {
						retval = 0;
						break;
					} else if (ts0710->dlci[dlci].state == DISCONNECTED || ts0710->dlci[dlci].state == DISCONNECTING) {
						/* Todo:Should NOT be here */
						printk(KERN_ERR "MUX id[%d] DLCI:%d opening occured!\n", self->mux_id, dlci);
						retval = -EAGAIN;
						break;
					} else if (ts0710->dlci[dlci].state == REJECTED) {
						printk(KERN_ERR "MUX: id[%d] DLCI:%d Send SABM got rejected!\n", self->mux_id, dlci);
						retval = -EREJECTED;
						break;
					} else if (signal_pending(current)) {
						printk(KERN_ERR "MUX: id[%d] DLCI:%d Send SABM got signal!\n", self->mux_id, dlci);
						retval = -EAGAIN;
						break;
					}
				}
			}

			if ((ts0710->dlci[dlci].state == NEGOTIATING)
			    || (ts0710->dlci[dlci].state == CONNECTING)) {
				ts0710->dlci[dlci].state = DISCONNECTED;
				retval = -EAGAIN;
			}
			wake_up_all(&ts0710->dlci[dlci].open_wait);
		}
	}
	return retval;
}

static int ts0710_exec_test_cmd(ts0710_con *ts0710)
{
	__u8 *f_buf;		/* Frame buffer */
	__u8 *d_buf;		/* Data buffer */
	int retval = -EFAULT;
	int j;
	unsigned long t;
	struct sprd_mux *self = NULL;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return retval;
	}

	self = (struct sprd_mux *)ts0710->user_data;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return retval;
	}

	MUX_TS0710_DEBUG(self->mux_id, "ts0710_exec_test_cmd\n");

	if (ts0710->be_testing) {
		/* Reentry */
		t = jiffies;
		interruptible_sleep_on_timeout(&ts0710->test_wait,
					       3 * TS0710MUX_TIME_OUT);
		if (ts0710->be_testing == 0) {
			if (ts0710->test_errs == 0) {
				retval = 0;
			} else {
				retval = -EFAULT;
			}
		} else if (signal_pending(current)) {
			MUX_TS0710_DEBUG(self->mux_id, "Wait for Test_cmd response got signal!\n");
			retval = -EAGAIN;
		} else if ((jiffies - t) >= 3 * TS0710MUX_TIME_OUT) {
			MUX_TS0710_DEBUG(self->mux_id, "Wait for Test_cmd response timeout!\n");
			retval = -EFAULT;
		}
	} else {
		ts0710->be_testing = 1;	/* Set the flag */

		f_buf = (__u8 *) kmalloc(TEST_PATTERN_SIZE + 32, GFP_KERNEL);
		d_buf = (__u8 *) kmalloc(TEST_PATTERN_SIZE + 32, GFP_KERNEL);
		if ((!f_buf) || (!d_buf)) {
			if (f_buf) {
				kfree(f_buf);
			}
			if (d_buf) {
				kfree(d_buf);
			}

			ts0710->be_testing = 0;	/* Clear the flag */
			ts0710->test_errs = TEST_PATTERN_SIZE;
			wake_up_interruptible(&ts0710->test_wait);
			return -ENOMEM;
		}

		for (j = 0; j < TEST_PATTERN_SIZE; j++) {
			d_buf[j] = j & 0xFF;
		}

		t = jiffies;
		ts0710_test_msg(ts0710, d_buf, TEST_PATTERN_SIZE, MCC_CMD,
				f_buf);
		interruptible_sleep_on_timeout(&ts0710->test_wait,
					       2 * TS0710MUX_TIME_OUT);
		if (ts0710->be_testing == 0) {
			if (ts0710->test_errs == 0) {
				retval = 0;
			} else {
				retval = -EFAULT;
			}
		} else if (signal_pending(current)) {
			MUX_TS0710_DEBUG(self->mux_id, "Send Test_cmd got signal!\n");
			retval = -EAGAIN;
		} else if ((jiffies - t) >= 2 * TS0710MUX_TIME_OUT) {
			MUX_TS0710_DEBUG(self->mux_id, "Send Test_cmd timeout!\n");
			ts0710->test_errs = TEST_PATTERN_SIZE;
			retval = -EFAULT;
		}

		ts0710->be_testing = 0;	/* Clear the flag */
		wake_up_interruptible(&ts0710->test_wait);

		/* Release buffer */
		if (f_buf) {
			kfree(f_buf);
		}
		if (d_buf) {
			kfree(d_buf);
		}
	}

	return retval;
}

static void mux_sched_send(struct sprd_mux *self)
{
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	complete(&self->send_completion);
}

/*
 * Returns 1 if found, 0 otherwise. needle must be null-terminated.
 * strstr might not work because WebBox sends garbage before the first OKread
 *
 */
int findInBuf(unsigned char *buf, int len, char *needle)
{
	int i;
	int needleMatchedPos = 0;

	if (needle[0] == '\0') {
		return 1;
	}

	for (i = 0; i < len; i++) {
		if (needle[needleMatchedPos] == buf[i]) {
			needleMatchedPos++;
			if (needle[needleMatchedPos] == '\0') {
				/* Entire needle was found */
				return 1;
			}
		} else {
			needleMatchedPos = 0;
		}
	}
	return 0;
}


/****************************
 * TTY driver routines
*****************************/
void ts0710_mux_close(int mux_id, int line)
{
	__u8 dlci;
	__u8 cmdline;
	__u8 dataline;
	ts0710_con *ts0710 = NULL;
	struct sprd_mux *self = NULL;

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] is invalid\n", __FUNCTION__, mux_id, line);
		return;
	}

	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] self is NULL\n", __FUNCTION__, mux_id, line);
		return;
	}

	ts0710 = self ->connection;
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] ts0710 is NULL\n", __FUNCTION__, mux_id, line);
		return;
	}

	if (self->open_count[line] > 0) {
		self->open_count[line]--;
	}

	// The close here and the open in mux_recover_thread may conflict, so wait
	if (self->mux_status == MUX_STATE_RECOVERING) {
		int rval = 0;

		printk(KERN_INFO "MUX: mux[%d] line[%d] pid[%d] wait close \n", mux_id, line, current->pid);

		rval = wait_event_interruptible_timeout(self->modem_ready, self->mux_status == MUX_STATE_READY, TS0710MUX_TIME_OUT);

		if (rval < 0) {
			printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] close wait interrupted!\n", __FUNCTION__, mux_id, line, current->pid);
		
		} else if (rval == 0) {
			printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] timeout!\n", __FUNCTION__, mux_id, line, current->pid);
		
		}
	}

	printk(KERN_INFO "MUX: mux[%d] line[%d] pid[%d] closed!\n", mux_id, line, current->pid);

	dlci = line2dlci[line];
	cmdline = dlci2line[dlci].cmdline;
	dataline = dlci2line[dlci].dataline;

	if ((self->open_count[cmdline] == 0) && (self->open_count[dataline] == 0)) {
		//close current channel first
		ts0710_close_channel(ts0710, dlci);
	}

	if (self->open_count[line] == 0) {
		if ((self->mux_send_info_flags[line])
			&& (self->mux_send_info[line])
			) {
			self->mux_send_info_flags[line] = 0;
			mux_free_send_info(self->mux_send_info[line]);
			self->mux_send_info[line] = 0;
			MUX_TS0710_DEBUG(self->mux_id, "Free mux_send_info for /dev/mux%d\n", line);
		}

		if ((self->mux_recv_info_flags[line])
			&& (self->mux_recv_info[line])
			&& (self->mux_recv_info[line]->total == 0)) {
			self->mux_recv_info_flags[line] = 0;
			free_mux_recv_struct(self->mux_recv_info[line]);
			self->mux_recv_info[line] = 0;
			MUX_TS0710_DEBUG(self->mux_id, "Free mux_recv_info for /dev/mux%d\n", line);
		}
	}
}

int ts0710_mux_read(int mux_id, int line, const unsigned char *buf, int count, int timeout)
{
	__u8 dlci;
	mux_recv_struct *recv_info;
	struct sprd_mux *self = NULL;
	ts0710_con *ts0710 = NULL;
	int rval = 0;
	mux_recv_packet *recv_packet;
	unsigned char *buf_pos;
	int cp_cnt = 0;

	MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] count  = %d, timeout = %d\n", line, current->pid, count, timeout);

	if (count <= 0) {
		return 0;
	}

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] is invalid\n", __FUNCTION__, mux_id, line);
		return -EINVAL;
	}

	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] self is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	ts0710 = self ->connection;
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] ts0710 is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	dlci = line2dlci[line];
	if (ts0710->dlci[0].state == FLOW_STOPPED) {
		MUX_TS0710_DEBUG(self->mux_id, "Flow stopped on all channels, returning zero /dev/mux%d\n", line);
		return 0;
	} else if (ts0710->dlci[dlci].state == FLOW_STOPPED) {
		MUX_TS0710_DEBUG(self->mux_id, "Flow stopped, returning zero /dev/mux%d\n", line);
		return 0;
	} else if (ts0710->dlci[dlci].state == CONNECTED) {
		if (!(self->mux_send_info_flags[line])) {
			TS0710_PRINTK("MUX Error: mux_write: mux_send_info_flags[%d] == 0\n", line);
			return -ENODEV;
		}
		recv_info = self->mux_recv_info[line];
		if (!recv_info) {
			TS0710_PRINTK("MUX Error: mux_write: mux_send_info[%d] == 0\n", line);
			return -ENODEV;
		}

		if (timeout == 0) {
			if (!mutex_trylock(&recv_info->recv_lock)) {
				printk(KERN_INFO "MUX: %s mux[%d] line[%d] pid[%d] is busy!\n", __FUNCTION__, mux_id, line, current->pid);
				return -EBUSY;
			}

			if (recv_info->total == 0) {
				printk(KERN_INFO "MUX: %s mux[%d] line[%d] pid[%d] is empty!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&recv_info->recv_lock);
				return -EBUSY;
			}
		} else if (timeout < 0){
			mutex_lock(&recv_info->recv_lock);
			MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] read wait\n", line, current->pid);
			rval = wait_event_interruptible(recv_info->rx_wait, recv_info->total != 0 && self->mux_status == MUX_STATE_READY);
			if (rval < 0) {
				printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] read wait interrupted!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&recv_info->recv_lock);
				return -EINTR;
			}
		}else {
			mutex_lock(&recv_info->recv_lock);
			rval = wait_event_interruptible_timeout(recv_info->rx_wait, recv_info->total != 0 && self->mux_status == MUX_STATE_READY, timeout);
			if (rval < 0) {
				printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] write wait interrupted!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&recv_info->recv_lock);
				return -EINTR;

			} else if (rval == 0) {
				printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] timeout!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&recv_info->recv_lock);
				return -ETIME;

			}
		}

		mutex_lock(&recv_info->recv_data_lock);

		buf_pos = (unsigned char *)buf;

		MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] length  = %d total = %d\n", line, current->pid, recv_info->length, recv_info->total);

		//copy data of recv_info before one of the packet
		if (recv_info->length != 0) {

			cp_cnt = min(count, recv_info->length);

			if ((uint32_t)buf > TASK_SIZE) {
				memcpy(buf_pos, recv_info->data + recv_info->pos, cp_cnt);
			} else {
				if(copy_to_user(buf_pos, recv_info->data + recv_info->pos, cp_cnt)) {
					mutex_unlock(&recv_info->recv_data_lock);
					mutex_unlock(&recv_info->recv_lock);
					return -EFAULT;
				}
			}

			buf_pos += cp_cnt;
			count -= cp_cnt;
			recv_info->length -= cp_cnt;
			recv_info->total -= cp_cnt;

			self->check_read_sum[line] -= cp_cnt;

			if (recv_info->length == 0) {
				recv_info->pos = 0;
			} else {
				recv_info->pos += cp_cnt;
			}
		}

		while(count > 0 && recv_info->total != 0) {
			if ((recv_packet = recv_info->mux_packet)) {
				cp_cnt = min(count, recv_packet->length);

				if ((uint32_t)buf > TASK_SIZE) {
					memcpy(buf_pos, recv_packet->data + recv_packet->pos, cp_cnt);
				} else {
					if(copy_to_user(buf_pos, recv_packet->data + recv_packet->pos, cp_cnt)) {
						mutex_unlock(&recv_info->recv_data_lock);
						mutex_unlock(&recv_info->recv_lock);
						printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] total is error!\n", __FUNCTION__, mux_id, line, current->pid);
						return -EFAULT;
					}
				}

				buf_pos += cp_cnt;
				count -= cp_cnt;
				recv_packet->length -= cp_cnt;
				recv_info->total -= cp_cnt;
				self->check_read_sum[line] -= cp_cnt;

				if (recv_packet->length == 0) {
					recv_packet->pos = 0;
					recv_info->mux_packet = recv_packet->next;
					free_mux_recv_packet(recv_packet);
				} else {
					recv_packet->pos += cp_cnt;
				}
			} else {
				if (recv_info->total != 0) {
					printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] total is incorrect!\n", __FUNCTION__, mux_id, line, current->pid);
					break;
				}
			}
		}
		mutex_unlock(&recv_info->recv_data_lock);
		mutex_unlock(&recv_info->recv_lock);

		MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] read = %d\n", line, current->pid, buf_pos - buf);

		return buf_pos - buf;
	}
	return 0;
}

int ts0710_mux_poll_wait(int mux_id, int line, struct file *filp, poll_table *wait)
{
	__u8 dlci;
	struct sprd_mux *self = NULL;
	ts0710_con *ts0710 = NULL;
	mux_send_struct *send_info;
	mux_recv_struct *recv_info;
	unsigned int mask = 0;

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] is invalid\n", __FUNCTION__, mux_id, line);
		return -EINVAL;
	}

	self = sprd_mux_mgr[mux_id].handle;
	if (!self) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] self is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	ts0710 = self ->connection;
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] ts0710 is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	dlci = line2dlci[line];
	if (ts0710->dlci[dlci].state != CONNECTED) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] state is %d\n", __FUNCTION__, mux_id, line, ts0710->dlci[dlci].state);
		return -ENODEV;
	}

	send_info = self->mux_send_info[line];
	recv_info = self->mux_recv_info[line];

	poll_wait(filp, &send_info->tx_wait, wait);
	poll_wait(filp, &recv_info->rx_wait, wait);

	if (recv_info->total != 0 && self->mux_status == MUX_STATE_READY) {
		mask |= POLLIN | POLLRDNORM;
	}

	if (mux_line_state(mux_id, line) == 0 && self->mux_status == MUX_STATE_READY) {
		mask |= POLLOUT | POLLWRNORM;
	}

	return mask;

}

int ts0710_mux_write(int mux_id, int line, const unsigned char *buf, int count, int timeout)
{
	__u8 dlci;
	mux_send_struct *send_info;
	__u8 *d_buf;
	__u16 c;
	struct sprd_mux *self = NULL;
	ts0710_con *ts0710 = NULL;
	int rval = 0;
	__u32 ring_index;

	MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] count  = %d, timeout = %d\n", line, current->pid, count, timeout);

	if (count <= 0) {
		return 0;
	}

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] is invalid\n", __FUNCTION__, mux_id, line);
		return -EINVAL;
	}

	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] self is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	ts0710 = self ->connection;
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] ts0710 is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	dlci = line2dlci[line];
	if (ts0710->dlci[0].state == FLOW_STOPPED) {
		MUX_TS0710_DEBUG(self->mux_id, "Flow stopped on all channels, returning zero /dev/mux%d\n", line);
		return 0;
	} else if (ts0710->dlci[dlci].state == FLOW_STOPPED) {
		MUX_TS0710_DEBUG(self->mux_id, "Flow stopped, returning zero /dev/mux%d\n", line);
		return 0;
	} else if (ts0710->dlci[dlci].state == CONNECTED) {
		if (!(self->mux_send_info_flags[line])) {
			TS0710_PRINTK("MUX Error: mux_write: mux_send_info_flags[%d] == 0\n", line);
			return -ENODEV;
		}
		send_info = self->mux_send_info[line];
		if (!send_info) {
			TS0710_PRINTK("MUX Error: mux_write: mux_send_info[%d] == 0\n", line);
			return -ENODEV;
		}

		c = min(count, (ts0710->dlci[dlci].mtu - 1));

		if (c <= 0) {
			return 0;
		}

		if (timeout == 0) {
			if (!mutex_trylock(&send_info->send_lock)) {
				printk(KERN_INFO "MUX: %s mux[%d] line[%d] pid[%d] is busy!\n", __FUNCTION__, mux_id, line, current->pid);
				return -EBUSY;
			}

			if (mux_line_state(mux_id, line)) {
				printk(KERN_INFO "MUX: %s mux[%d] line[%d] pid[%d] is filled!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&send_info->send_lock);
				return -EBUSY;
			}

			if (self->mux_status != MUX_STATE_READY)
			{
				printk(KERN_INFO "MUX: %s mux[%d] line[%d] pid[%d] mux is Not ready!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&send_info->send_lock);
				return -EBUSY;
			}
		} else if (timeout < 0){
				mutex_lock(&send_info->send_lock);

				MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] wait write\n", line, current->pid);

				rval = wait_event_interruptible(send_info->tx_wait, mux_line_state(mux_id, line) == 0 && self->mux_status == MUX_STATE_READY);
				if (rval < 0) {
					printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] write wait interrupted!\n", __FUNCTION__, mux_id, line, current->pid);
					mutex_unlock(&send_info->send_lock);
					return -EINTR;
				}
		}else {
			mutex_lock(&send_info->send_lock);
			rval = wait_event_interruptible_timeout(send_info->tx_wait, mux_line_state(mux_id, line) == 0 && self->mux_status == MUX_STATE_READY, timeout);
			if (rval < 0) {
				printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] write wait interrupted!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&send_info->send_lock);
				rval = -EINTR;
			} else if (rval == 0) {
				printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] timeout!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&send_info->send_lock);
				return -ETIME;
			}
		}

		ring_index = send_info->write_index % ringbuf_num[mux_id][line];

		d_buf = send_info->buf + (ring_index * TS0710MUX_SEND_BUF_SIZE) + TS0710MUX_SEND_BUF_OFFSET;

//		printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] 	ring_index = %d, num = %d\n", __FUNCTION__, mux_id, line, current->pid, ring_index, ringbuf_num[mux_id][line]);

		if ((uint32_t)buf > TASK_SIZE) {
			memcpy(&d_buf[0], buf, c);
		} else {
			if(copy_from_user(&d_buf[0], buf, c)) {
				printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] pid[%d] failed to copy from user!\n", __FUNCTION__, mux_id, line, current->pid);
				mutex_unlock(&send_info->send_lock);
				return -EFAULT;
			}
		}

		MUX_TS0710_DEBUG(self->mux_id, "Prepare to send %d bytes from /dev/mux%d", c, line);
		MUX_TS0710_DEBUGHEX(self->mux_id, d_buf, c);

		if (!send_info->frame) {
			printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] send_info->frame  NULL\n", __FUNCTION__, mux_id, line, current->pid);
			return 0;
		}

		send_info->frame[ring_index] = d_buf;

		queue_uih(send_info, ring_index, c, ts0710, dlci);

		if (line <= MUX_RIL_LINE_END) {
		    printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] write  = %d\n", __FUNCTION__, mux_id, line, current->pid, c);
		}

		send_info->write_index++;

		mutex_unlock(&send_info->send_lock);

		mux_sched_send(self);

//		printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] write  = %d\n", __FUNCTION__, mux_id, line, current->pid, c);

		MUX_TS0710_DEBUG(mux_id, "line[%d] pid[%d] wait write\n", line, current->pid);

		return c;
	} else {
		printk(KERN_ERR "MUX: %s mux[%d] line[%d] pid[%d] not connected\n", __FUNCTION__, mux_id, line, current->pid);
		return -EDISCONNECTED;
	}
}

int ts0710_mux_mux_ioctl(int mux_id, int line, unsigned int cmd, unsigned long arg)
{
	__u8 dlci;
	struct sprd_mux *self = NULL;
	ts0710_con *ts0710 = NULL;

	UNUSED_PARAM(arg);

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] is invalid\n", __FUNCTION__, mux_id, line);
		return -EINVAL;
	}

	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] self is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	ts0710 = self ->connection;
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s mux[%d] line[%d] ts0710 is NULL\n", __FUNCTION__, mux_id, line);
		return -ENODEV;
	}

	dlci = line2dlci[line];
	switch (cmd) {
	case TS0710MUX_IO_MSC_HANGUP:
		if (ts0710_msc_msg(ts0710, EA | RTR | DV, MCC_CMD, dlci) < 0) {
			return -EAGAIN;
		} else {
			return 0;
		}

	case TS0710MUX_IO_TEST_CMD:
		return ts0710_exec_test_cmd(ts0710);

	default:
		break;
	}
	return -ENOIOCTLCMD;
}

int ts0710_mux_open(int mux_id, int line)
{
	int retval;
	__u8 dlci;
	__u8 cmdline;
	__u8 dataline;
	mux_send_struct *send_info;
	mux_recv_struct *recv_info;
	struct sprd_mux *self = NULL;
	ts0710_con *ts0710 = NULL;

	retval = -ENODEV;

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s line[%d] is wrong\n", __FUNCTION__, line);
		goto out;
	}

	self = sprd_mux_mgr[mux_id].handle;

	mutex_lock(&self->open_mutex[line]);

	ts0710 = self->connection;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		goto out;
	}

	self->open_count[line]++;
	dlci = line2dlci[line];

	printk(KERN_INFO "MUX: %s id = %d, dlci = %d, pid = %d\n", __FUNCTION__, self->mux_id, dlci, current->pid);

	retval = wait_event_interruptible(self->modem_ready, self->mux_status == MUX_STATE_READY);
	if (retval < 0) {
		printk(KERN_WARNING "MUX: %s mux[%d] line[%d] pid[%d] ready wait interrupted!\n", __FUNCTION__, self->mux_id, line, current->pid);
		self->open_count[line]--;
		mutex_unlock(&self->open_mutex[line]);
		retval = -EINTR;
		goto out;
	}

	if (self->cmux_mode == 0) {
		mutex_lock(&self->handshake_mutex);
		printk(KERN_INFO "MUX: id = %d, dlci = %d mode = %d\n", self->mux_id, dlci, self->cmux_mode);
		if (self->cmux_mode == 0) {
			if (mux_handshake(self) != 0) {
					mutex_unlock(&self->handshake_mutex);
					self->open_count[line]--;
					mutex_unlock(&self->open_mutex[line]);
					printk(KERN_WARNING "\n wrong modem state !!!!\n");
					retval = -ENODEV;
					goto out;
			}
			self->cmux_mode = 1;
			wake_up(&self->handshake_ready);

			/*	 Open server channel 0 first */
			if ((retval = ts0710_open_channel(ts0710, 0)) != 0) {
				printk(KERN_ERR "MUX: Error Can't connect server channel 0!\n");
				ts0710_init(ts0710);
				self->open_count[line]--;
				mutex_unlock(&self->handshake_mutex);
				mutex_unlock(&self->open_mutex[line]);
				goto out;
			}
		}
		mutex_unlock(&self->handshake_mutex);
	}

	if ((retval = ts0710_ctrl_channel_status(self->connection)) != 0) {
		ts0710_init(ts0710);
		self->open_count[line]--;
		mutex_unlock(&self->open_mutex[line]);
		goto out;
	}

	/* Allocate memory first. As soon as connection has been established, MUX may receive */
	if (self->mux_send_info_flags[line] == 0) {
		send_info = mux_alloc_send_info(mux_id, line);
		if (!send_info) {
			retval = -ENOMEM;
			self->open_count[line]--;
			mutex_unlock(&self->open_mutex[line]);
			goto out;
		}

		send_info->read_index = 0;
		send_info->write_index = 0;
		send_info->flags = 0;
		mutex_init(&send_info->send_lock);
		init_waitqueue_head(&send_info->tx_wait);

		self->mux_send_info[line] = send_info;
		self->mux_send_info_flags[line] = 1;

		MUX_TS0710_DEBUG(self->mux_id, "Allocate mux_send_info for /dev/mux%d\n", line);
	}

	if (self->mux_recv_info_flags[line] == 0) {
		recv_info = (mux_recv_struct *) kmalloc(sizeof(mux_recv_struct), GFP_KERNEL);
		if (!recv_info) {
			self->mux_send_info_flags[line] = 0;
			mux_free_send_info(self->mux_send_info[line]);
			self->mux_send_info[line] = 0;

			MUX_TS0710_DEBUG(self->mux_id, "Free mux_send_info for /dev/mux%d\n", line);
			retval = -ENOMEM;

			self->open_count[line]--;
			mutex_unlock(&self->open_mutex[line]);
			goto out;
		}
		recv_info->length = 0;
		recv_info->total = 0;
		recv_info->mux_packet = 0;
		recv_info->pos = 0;
		mutex_init(&recv_info->recv_lock);
		mutex_init(&recv_info->recv_data_lock);
		init_waitqueue_head(&recv_info->rx_wait);

		self->mux_recv_info[line] = recv_info;
		self->mux_recv_info_flags[line] = 1;

		MUX_TS0710_DEBUG(self->mux_id, "Allocate mux_recv_info for /dev/mux%d\n", line);
	}

	/* Now establish DLCI connection */
	cmdline = dlci2line[dlci].cmdline;
	dataline = dlci2line[dlci].dataline;

	MUX_TS0710_DEBUG(self->mux_id, "dlci = %d cmdline = %d dataline = %d open_count[cmdline] = %d open_count[dataline] = %d\n", dlci, cmdline, dataline, self->open_count[cmdline], self->open_count[dataline]);

	if ((self->open_count[cmdline] > 0) || (self->open_count[dataline] > 0)) {
		if ((retval = ts0710_open_channel(ts0710, dlci)) != 0) {
			TS0710_PRINTK("MUX: Can't connected channel %d!\n", dlci);
			ts0710_reset_dlci(ts0710, dlci);

			self->mux_send_info_flags[line] = 0;
			mux_free_send_info(self->mux_send_info[line]);
			self->mux_send_info[line] = 0;

			MUX_TS0710_DEBUG(self->mux_id, "Free mux_send_info for /dev/mux%d\n", line);

			self->mux_recv_info_flags[line] = 0;
			free_mux_recv_struct(self->mux_recv_info[line]);
			self->mux_recv_info[line] = 0;
			MUX_TS0710_DEBUG(self->mux_id, "Free mux_recv_info for /dev/mux%d\n", line);

			self->open_count[line]--;
			mutex_unlock(&self->open_mutex[line]);
			goto out;
		}
	}

	mutex_unlock(&self->open_mutex[line]);
	retval = 0;

	out:
		printk(KERN_INFO "MUX: id = %d, dlci = %d, retval = %d\n", self->mux_id, dlci, retval);

		return retval;
}

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO) || unlikely(policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

/*For BP UART problem Begin*/
#ifdef TS0710SEQ2
static int send_ack(ts0710_con * ts0710, __u8 seq_num, __u8 bp_seq1,
		    __u8 bp_seq2)
#else
static int send_ack(ts0710_con * ts0710, __u8 seq_num)
#endif
{
	return 0;
}

static int mux_set_thread_pro(int pro)
{
	int ret;
	struct sched_param s;

	/* just for this write, set us real-time */
	if (!task_has_rt_policy(current)) {
		struct cred *new = prepare_creds();
		if(!new)
			return -ENOMEM;
		cap_raise(new->cap_effective, CAP_SYS_NICE);
		commit_creds(new);
		s.sched_priority = MAX_RT_PRIO - pro;
		ret = sched_setscheduler(current, SCHED_RR, &s);
		if (ret != 0)
			printk(KERN_WARNING "MUX: set priority failed!\n");
	}
	return 0;
}

/*For BP UART problem End*/

static void receive_worker(struct sprd_mux *self, int start)
{
	int count;
	unsigned char *search, *to, *from;
	short_frame *short_pkt;
	long_frame *long_pkt;
	/*For BP UART problem Begin */
	__u32 crc_error;
	__u8 *uih_data_start;
	__u32 uih_len;
	/*For BP UART problem End */
	int i;

	if (!self || !self->io_hal || !self->io_hal->io_read) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	if (start) {
		memset(self->tbuf, 0, TS0710MUX_MAX_BUF_SIZE);
		self->tbuf_ptr = self->tbuf;
		self->start_flag = 0;
	}

	for (i = MUX_VETH_LINE_BEGIN; i <= MUX_VETH_LINE_END; i++) {
		if (self->callback[i].func) {
			(*self->callback[i].func)(i, SPRDMUX_EVENT_READ_IND, NULL, 0, self->callback[i].user_data);
		}
	}

	count = self->io_hal->io_read(self->tbuf_ptr, TS0710MUX_MAX_BUF_SIZE - (self->tbuf_ptr - self->tbuf));
	if (count <= 0 || self->cmux_mode == 0) {
		return;
	}

	self->tbuf_ptr += count;

	if ((self->start_flag != 0) && (self->framelen != -1)) {
		if ((self->tbuf_ptr - self->start_flag) < self->framelen) {
			return;
		}
	}

	search = &self->tbuf[0];
	while (1) {
		if (self->start_flag == 0) {	/* Frame Start Flag not found */
			self->framelen = -1;
			while (search < self->tbuf_ptr) {
				if (*search == TS0710_BASIC_FLAG) {

					self->start_flag = search;
					break;
				}
#ifdef TS0710LOG
				else {
					TS0710_PRINTK(">S %02x %c\n", *search, *search);
				}
#endif
				search++;
			}

			if (self->start_flag == 0) {
				self->tbuf_ptr = &self->tbuf[0];
				break;
			}
		} else {	/* Frame Start Flag found */
			/* 1 start flag + 1 address + 1 control + 1 or 2 length + lengths data + 1 FCS + 1 end flag */
			/* For BP UART problem 1 start flag + 1 seq_num + 1 address + ...... */
			/*if( (framelen == -1) && ((tbuf_ptr - start_flag) > TS0710_MAX_HDR_SIZE) ) */
			if ((self->framelen == -1) && ((self->tbuf_ptr - self->start_flag) > (TS0710_MAX_HDR_SIZE + SEQ_FIELD_SIZE))) {	/*For BP UART problem */
				/*short_pkt = (short_frame *) (start_flag + 1); */
				short_pkt = (short_frame *) (self->start_flag + ADDRESS_FIELD_OFFSET);	/*For BP UART problem */
				if (short_pkt->h.length.ea == 1) {	/* short frame */
					/*framelen = TS0710_MAX_HDR_SIZE + short_pkt->h.length.len + 1; */
					self->framelen = TS0710_MAX_HDR_SIZE + short_pkt->h.length.len + 1 + SEQ_FIELD_SIZE;	/*For BP UART problem */
				} else {	/* long frame */
					/*long_pkt = (long_frame *) (start_flag + 1); */
					long_pkt = (long_frame *) (self->start_flag + ADDRESS_FIELD_OFFSET);	/*For BP UART problem */
					/*framelen = TS0710_MAX_HDR_SIZE + GET_LONG_LENGTH( long_pkt->h.length ) + 2; */
					self->framelen = TS0710_MAX_HDR_SIZE + GET_LONG_LENGTH(long_pkt->h.length) + 2 + SEQ_FIELD_SIZE;	/*For BP UART problem */
				}

				/*if( framelen > TS0710MUX_MAX_TOTAL_FRAME_SIZE ) { */
				if (self->framelen > (TS0710MUX_MAX_TOTAL_FRAME_SIZE + SEQ_FIELD_SIZE)) {	/*For BP UART problem */
					MUX_TS0710_LOGSTR_FRAME(self->mux_id, 0, self->start_flag, (self->tbuf_ptr - self->start_flag));
					TS0710_PRINTK("MUX Error: %s: frame length:%d is bigger than Max total frame size:%d\n",
		 /*__FUNCTION__, framelen, TS0710MUX_MAX_TOTAL_FRAME_SIZE);*/
					     __FUNCTION__, self->framelen, (TS0710MUX_MAX_TOTAL_FRAME_SIZE + SEQ_FIELD_SIZE));	/*For BP UART problem */
					search = self->start_flag + 1;
					self->start_flag = 0;
					self->framelen = -1;
					continue;
				}
			}

			if ((self->framelen != -1)
			    && ((self->tbuf_ptr - self->start_flag) >= self->framelen)) {
				if (*(self->start_flag + self->framelen - 1) == TS0710_BASIC_FLAG) {	/* OK, We got one frame */

					/*For BP UART problem Begin */

					MUX_TS0710_LOGSTR_FRAME(self->mux_id, 0, self->start_flag, self->framelen);
					MUX_TS0710_DEBUGHEX(self->mux_id, self->start_flag, self->framelen);
					short_pkt = (short_frame *) (self->start_flag + ADDRESS_FIELD_OFFSET);
					if ((short_pkt->h.length.ea) == 0) {
						long_pkt = (long_frame *) (self->start_flag + ADDRESS_FIELD_OFFSET);
						uih_len = GET_LONG_LENGTH(long_pkt->h.length);
						uih_data_start = long_pkt->h.data;

						crc_error = crc_check((__u8*) (self->start_flag + SLIDE_BP_SEQ_OFFSET), LONG_CRC_CHECK + 1, *(uih_data_start + uih_len));
					} else {
						uih_len = short_pkt->h.length.len;
						uih_data_start = short_pkt->data;

						crc_error = crc_check((__u8*) (self->start_flag + SLIDE_BP_SEQ_OFFSET), SHORT_CRC_CHECK + 1, *(uih_data_start + uih_len));
					}

					if (!crc_error) {
						if (1) 	/*expect_seq == *(start_flag + SLIDE_BP_SEQ_OFFSET)*/
						{
							self->expect_seq = *(self->start_flag + SLIDE_BP_SEQ_OFFSET);
							self->expect_seq++;
							if (self->expect_seq >= 4){
								self->expect_seq = 0;
							}
#ifdef TS0710SEQ2
							send_ack(self->connection, self->expect_seq, *(self->start_flag + FIRST_BP_SEQ_OFFSET), *(self->start_flag + SECOND_BP_SEQ_OFFSET));
#else
							send_ack(self->connection, self->expect_seq);
#endif

							ts0710_recv_data(self->connection, self->start_flag + ADDRESS_FIELD_OFFSET, self->framelen - 2 - SEQ_FIELD_SIZE);
						} else {

#ifdef TS0710DEBUG
							if (*(self->start_flag + SLIDE_BP_SEQ_OFFSET) != 0x9F) {
#endif

								TS0710_PRINTK("MUX sequence number %d is not expected %d, discard data!\n", *(self->start_flag + SLIDE_BP_SEQ_OFFSET), self->expect_seq);

#ifdef TS0710SEQ2
								send_ack(self->connection, self->expect_seq, *(self->start_flag + FIRST_BP_SEQ_OFFSET), *(self->start_flag + SECOND_BP_SEQ_OFFSET));
#else
								send_ack(self->connection, self->expect_seq);
#endif

#ifdef TS0710DEBUG
							} else {
								*(uih_data_start + uih_len) = 0;
								TS0710_PRINTK("MUX bp log: %s\n", uih_data_start);
							}
#endif

						}
					} else {	/* crc_error */
						search = self->start_flag + 1;
						self->start_flag = 0;
						self->framelen = -1;
						continue;
					}	/*End if(!crc_error) */

					search = self->start_flag + self->framelen;
				} else {
					MUX_TS0710_LOGSTR_FRAME(self->mux_id, 0, self->start_flag, self->framelen);
					MUX_TS0710_DEBUGHEX(self->mux_id, self->start_flag, self->framelen);
					TS0710_PRINTK("MUX: Lost synchronization!\n");
					search = self->start_flag + 1;
				}

				self->start_flag = 0;
				self->framelen = -1;
				continue;
			}

			if (self->start_flag != &self->tbuf[0]) {

				to = self->tbuf;
				from = self->start_flag;
				count = self->tbuf_ptr - self->start_flag;
				while (count--) {
					*to++ = *from++;
				}

				self->tbuf_ptr -= (self->start_flag - self->tbuf);
				self->start_flag = self->tbuf;

			}
			break;
		}		/* End Frame Start Flag found */
	}			/* End while(1) */
}

static int mux_receive_thread(void *data)
{
	int start = 1;
	struct sprd_mux *self = data;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	printk(KERN_ERR "MUX: id = %d %s entered\n", self->mux_id, __FUNCTION__);

	mux_set_thread_pro(95);
	while (!kthread_should_stop()) {
		if (self->mux_exiting == 1) {
			self->mux_exiting = 2;
			return 0;
		}

		wait_event(self->handshake_ready, self->cmux_mode == 1);

		receive_worker(self, start);
		start = 0;
	}
	return 0;
}

static int mux_send_thread(void *private_)
{
	ts0710_con *ts0710;
	__u8 j;
	mux_send_struct *send_info;
	__u8 dlci;
	struct sprd_mux *self;
	__u32 ring_index;
	__u32 num;

	self = (struct sprd_mux *)(private_);

	if (!self) {
		printk(KERN_ERR "MUX: Error %s Self is NULL\n", __FUNCTION__);
		return 0;
	}

	printk(KERN_ERR "MUX: id = %d %s entered\n", self->mux_id, __FUNCTION__);

	ts0710 = self->connection;
	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return 0;
	}

	if (ts0710->dlci[0].state == FLOW_STOPPED) {
		MUX_TS0710_DEBUG(self->mux_id, "Flow stopped on all channels\n");
		return 0;
	}

	mux_set_thread_pro(80);

	while (!kthread_should_stop()) {

		wait_for_completion_interruptible(&self->send_completion);
		for (j = 0; j < NR_MUXS; j++) {
			wait_event(self->modem_ready, self->mux_status == MUX_STATE_READY);

			if (!(self->mux_send_info_flags[j])) {
				continue;
			}

			send_info = self->mux_send_info[j];
			if (!send_info) {
				continue;
			}

			if (send_info->write_index == send_info->read_index) {
				continue;
			}

			dlci = line2dlci[j];
			if (ts0710->dlci[dlci].state == FLOW_STOPPED) {
				MUX_TS0710_DEBUG(self->mux_id, "Flow stopped on channel DLCI: %d\n", dlci);
				continue;
			} else if (ts0710->dlci[dlci].state != CONNECTED) {
				MUX_TS0710_DEBUG(self->mux_id, "DLCI %d not connected\n", dlci);
				send_info->read_index = send_info->write_index;
				wake_up_interruptible(&send_info->tx_wait);
				continue;
			}

			while(send_info->write_index != send_info->read_index) {
				ring_index = send_info->read_index % ringbuf_num[self->mux_id][j];
				MUX_TS0710_DEBUG(self->mux_id, "Send queued UIH for /dev/mux%d ring_index = %d, len = %d", j,ring_index,  send_info->length[ring_index]);

				if (send_info->length[ring_index] <= TS0710MUX_SERIAL_BUF_SIZE) {
					if (basic_write(ts0710, (__u8 *) send_info->frame[ring_index], send_info->length[ring_index]) <= 0 ) {
						printk("[%s]: %s /dev/mux%d index = %d basic_write fail!\n", sprd_mux_mgr[self->mux_id].mux_name, __FUNCTION__, j, ring_index);
						break;
					}
				} else {
					printk("[%s]: %s /dev/mux%d send length is exceed %d\n", sprd_mux_mgr[self->mux_id].mux_name, __FUNCTION__, j, send_info->length[ring_index]);
				}
				send_info->length[ring_index] = 0;
				send_info->read_index++;
				num = send_info->write_index - send_info->read_index;
				if (send_info->need_notify || 0 == num) {
					if (num <= ringbuf_num[self->mux_id][j]/2) {
						send_info->need_notify = false;
//						printk("[%s]: %s /dev/mux%d num = %d, cnt = %u\n", sprd_mux_mgr[self->mux_id].mux_name, __FUNCTION__, j, num, cnt++);
						if (self->callback[j].func) {
							(*self->callback[j].func)(j, SPRDMUX_EVENT_COMPLETE_WRITE, (__u8 *)send_info->frame, 0, self->callback[j].user_data);
						}
					}
				}
			}

			wake_up_interruptible(&send_info->tx_wait);
		}			/* End for() loop */

	}
	return 0;
}


ssize_t mux_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	return seq_read(file, buf, size, ppos);
}

static int mux_proc_show(struct seq_file *seq, void *v)
{
	char mux_buf[MUX_MODE_MAX_LEN + 1];

	memset(mux_buf, 0, sizeof(mux_buf));
	snprintf(mux_buf, MUX_MODE_MAX_LEN, "%d\n", mux_mode);
	seq_puts(seq, mux_buf);

	return 0;
}
static int mux_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mux_proc_show, inode->i_private);
}

static ssize_t mux_proc_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char mux_buf[len + 1];
	int val;

	memset(mux_buf, 0, len + 1);
	if (len > 0) {
		if (copy_from_user(mux_buf, buf, len)) {
		   printk(KERN_ERR "MUX: Error %s failed to copy from user!\n", __FUNCTION__);
		   return -EFAULT;

		}

		val = simple_strtoul(mux_buf, NULL, 10);
		mux_mode = val;
	}

	return len;
}

static int mux_create_proc(void)
{
	struct proc_dir_entry *mux_entry;
	static const struct file_operations mux_fops = {
		.owner = THIS_MODULE,
		.open = mux_proc_open,
		.read = mux_proc_read,
		.write = mux_proc_write,
		.llseek = seq_lseek,
		.release = single_release,
	};

	mux_entry = proc_create("mux_mode", 0666, NULL, &mux_fops);
	if (!mux_entry) {
		printk(KERN_ERR "MUX: Error Can not create mux proc entry\n");
		return -ENOMEM;
	}

	return 0;
}

static void mux_set_ringbuf_num(void)
{
	int mux_id;
	int line;

	for(mux_id = SPRDMUX_ID_SPI; mux_id < SPRDMUX_ID_MAX; mux_id++) {
		for(line = 0; line < NR_MUXS; line++) {
			if (mux_id == SPRDMUX_ID_SDIO && (line >= MUX_VETH_LINE_BEGIN && line <=MUX_VETH_LINE_END)) {
				ringbuf_num[mux_id][line] = MUX_VETH_RINGBUFER_NUM;
			} else {
				/* Default */
				ringbuf_num[mux_id][line] = 1;
			}
		}
	}
}

static int mux_line_state(SPRDMUX_ID_E mux_id, int line)
{
	struct sprd_mux *self = NULL;
	mux_send_struct *send_info;

	self = sprd_mux_mgr[mux_id].handle;

	send_info = self->mux_send_info[line];

	if (!send_info) {
		printk(KERN_ERR "MUX Error: %s: mux_send_info[%d][%d] == 0\n", __FUNCTION__, mux_id, line);
		return 1;
	}

	if (send_info->write_index - send_info->read_index >= ringbuf_num[mux_id][line]) {
		/* No idle ring buffer */
		return 1;
	}

	return 0;
}

int sprdmux_line_busy(SPRDMUX_ID_E mux_id, int line)
{
	return mux_line_state(mux_id, line);
}

void sprdmux_set_line_notify(SPRDMUX_ID_E mux_id, int line, __u8 notify)

{
	struct sprd_mux *self = NULL;
	mux_send_struct *send_info;

	self = sprd_mux_mgr[mux_id].handle;

	send_info = self->mux_send_info[line];

	if (!send_info) {
		printk(KERN_ERR "MUX Error: %s: mux_send_info[%d][%d] == 0\n", __FUNCTION__, mux_id, line);
		return;
	}

	send_info->need_notify = notify;
}

static mux_send_struct * mux_alloc_send_info(SPRDMUX_ID_E mux_id, int line)
{
	mux_send_struct *send_info_ptr = NULL;

	send_info_ptr = (mux_send_struct *) kzalloc(sizeof(mux_send_struct), GFP_KERNEL);
	if (!send_info_ptr) {
		return NULL;
	}

	send_info_ptr->buf = (__u8 *)vmalloc(sizeof(__u8) * TS0710MUX_SEND_BUF_SIZE * ringbuf_num[mux_id][line]);
	if (!send_info_ptr->buf) {
		kfree(send_info_ptr);
		return NULL;
	}

	send_info_ptr->length = (__u16 *)kzalloc(sizeof(__u16) * ringbuf_num[mux_id][line], GFP_KERNEL);
	if (!send_info_ptr->length) {
		vfree(send_info_ptr->buf);
		kfree(send_info_ptr);
		return NULL;
	}

	send_info_ptr->frame = (__u8 **)kzalloc(sizeof(__u8 *) * ringbuf_num[mux_id][line], GFP_KERNEL);

	if (!send_info_ptr->frame) {
		kfree(send_info_ptr->length);
		vfree(send_info_ptr->buf);
		kfree(send_info_ptr);
		return NULL;
	}

	return send_info_ptr;
}

static void mux_free_send_info(mux_send_struct * send_info)
{
	if (!send_info) {
		return;
	}

	if (send_info->frame) {
		kfree(send_info->frame);
		send_info->frame = NULL;
	}

	if (send_info->length) {
		kfree(send_info->length);
		send_info->length = NULL;
	}

	if (send_info->buf) {
		vfree(send_info->buf);
		send_info->buf = NULL;

	}

	kfree(send_info);
	send_info = NULL;
}

static void mux_remove_proc(void)
{
	remove_proc_entry("mux_mode", NULL);	/* remove /proc/mux_mode */
}

int ts0710_mux_init(void)
{
	create_crctable(crctable);

//	mux_mgr_init(sprd_mux_mgr);

	if (mux_create_proc()) {
		printk(KERN_ERR "MUX: Error %s create mux proc interface failed!\n", __FUNCTION__);
	}

	mux_set_ringbuf_num();
	return 0;
}

void ts0710_mux_exit(void)
{
	mux_remove_proc();
	mux_mgr_init(sprd_mux_mgr);
}

int ts0710_mux_create(int mux_id)
{
	struct sprd_mux *self = NULL;
	int j, rval = 0;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX) {
		printk(KERN_ERR "MUX: Error %sInvalid Param\n",__FUNCTION__);
		rval = -EINVAL;
		goto err_iomux;
	}

	self = (struct sprd_mux *)kzalloc(sizeof(struct sprd_mux), GFP_KERNEL);

	if (!self) {
		printk(KERN_ERR "MUX: Error %s no memory for self\n",__FUNCTION__);
		rval = -ENOMEM;
		goto err_sprd_mux;
	}

	self->mux_id = mux_id;

	self->connection = (ts0710_con *)kzalloc(sizeof(ts0710_con), GFP_KERNEL);

	if (!self->connection) {
		printk(KERN_ERR "MUX: Error %s no memory for connection\n",__FUNCTION__);
		rval = -ENOMEM;
		goto err_conn;
	}

	self->connection->user_data = (void *)self;

	ts0710_init(self->connection);

	ts0710_init_waitqueue(self->connection);

	mutex_init(&self->handshake_mutex);

	for (j = 0; j < NR_MUXS; j++) {
		self->mux_send_info_flags[j] = 0;
		self->mux_send_info[j] = 0;
		self->mux_recv_info_flags[j] = 0;
		self->mux_recv_info[j] = 0;
		self->open_count[j] = 0;

		mutex_init(&self->open_mutex[j]);
	}

	self->framelen = -1;
	self->expect_seq = 0;
	self->mux_status = MUX_STATE_NOT_READY;

	init_completion(&self->send_completion);
	init_waitqueue_head(&self->handshake_ready);
	init_waitqueue_head(&self->modem_ready);

	/*create mux_send thread*/
	self->mux_send_kthread = kthread_create(mux_send_thread, self, "mux_send");
	if(IS_ERR(self->mux_send_kthread)) {
		printk(KERN_ERR "MUX: Error %s Unable to create mux_send thread err = %ld\n", __FUNCTION__, PTR_ERR(self->mux_send_kthread));
		rval = PTR_ERR(self->mux_send_kthread);
		goto err_send_thread;
	}

	wake_up_process(self->mux_send_kthread);

	/*create receive thread*/
	self->mux_recv_kthread = kthread_create(mux_receive_thread, self, "mux_receive");
	if(IS_ERR(self->mux_recv_kthread)) {
		printk(KERN_ERR "MUX: Error %s Unable to create mux_receive thread err = %ld\n", __FUNCTION__, PTR_ERR(self->mux_recv_kthread));
		rval = PTR_ERR(self->mux_recv_kthread);
		goto err_recv_thread;
	}
	wake_up_process(self->mux_recv_kthread);

	self->io_hal = &sprd_mux_mgr[mux_id].mux;
	sprd_mux_mgr[mux_id].handle = self;

	return rval;

err_recv_thread:
	kthread_stop(self->mux_send_kthread);
	self->mux_send_kthread = NULL;
err_send_thread:
	kfree(self->connection);
err_conn:
	kfree(self);
err_sprd_mux:
err_iomux:
	printk(KERN_ERR "MUX: Error %s Create Failed \n",__FUNCTION__);

	return rval;
}




void ts0710_mux_destory(int mux_id)
{
	int j;
	struct sprd_mux *self = NULL;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX) {
		printk(KERN_ERR "MUX: Error %sInvalid Param\n",__FUNCTION__);
		return;
	}

	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s Invalid Param self = %x\n", __FUNCTION__, (int)self);
		return;
	}

	for (j = 0; j < NR_MUXS; j++) {
		if (self->mux_send_info_flags[j]) {
			mux_free_send_info(self->mux_send_info[j]);
			self->mux_send_info_flags[j] = 0;
		}

		if ((self->mux_recv_info_flags[j]) && (self->mux_recv_info[j])) {
			free_mux_recv_struct(self->mux_recv_info[j]);
		}
		self->mux_recv_info_flags[j] = 0;
		self->mux_recv_info[j] = 0;
	}
	self->mux_exiting = 1;

	ts0710_close_channel(self->connection, 0);
	TS0710_SIG2APLOGD();
	self->cmux_mode = 0;

	if (!self->io_hal || !self->io_hal->io_stop) {
		self->io_hal->io_stop(SPRDMUX_READ);
	}

	while (self->mux_exiting == 1) {
		msleep(10);
	}

	if(self->mux_recv_kthread) {
		kthread_stop(self->mux_recv_kthread);
		self->mux_recv_kthread = NULL;
	}

	if(self->mux_send_kthread) {
		kthread_stop(self->mux_send_kthread);
		self->mux_send_kthread = NULL;
	}

	if (self->connection) {
		kfree(self->connection);
	}

	if (self->mux_recover_kthread) {
		kthread_stop(self->mux_recover_kthread);
		self->mux_recover_kthread = NULL;
	}
	kfree(self);
}

static void mux_print_mux_mgr(void)
{
	int j = 0;

	printk(KERN_INFO "No 	MUX_ID		Name		Handle\n");

	for (j = 0; j < SPRDMUX_MAX_NUM; j++) {
		printk(KERN_INFO "%d		%d			%s		%d\n", j, sprd_mux_mgr[j].mux_id,
				sprd_mux_mgr[j].mux_name, (uint32_t)sprd_mux_mgr[j].handle);
	}
}
static void mux_mgr_init(mux_info *mux_mgr)
{
	int j = 0;
	mux_print_mux_mgr();

	if (mux_mgr) {
		for (j = 0; j < SPRDMUX_MAX_NUM; j++) {
			mux_mgr[j].handle = NULL;
		}
	}
}

int sprdmux_register(struct sprdmux *mux)
{
	if (!mux || mux->id < 0 || mux->id >= SPRDMUX_ID_MAX ) {
		printk(KERN_ERR "MUX: %s Invalid Param\n", __FUNCTION__);
		return -EINVAL;
	}

	memcpy(&sprd_mux_mgr[mux->id].mux, mux, sizeof(struct sprdmux));

	printk(KERN_INFO "ts0710MUX[%d] 0x%x, 0x%x 0x%x\n", mux->id, (unsigned int)mux->io_write, (unsigned int)mux->io_read, (unsigned int)mux->io_stop);

	ts0710_mux_create(mux->id);

	return 0;
}

void sprdmux_unregister(SPRDMUX_ID_E mux_id)
{
	if (mux_id >= SPRDMUX_ID_MAX || mux_id < 0) {
		printk(KERN_ERR "MUX: Error %s Invalid Param mux_id = %d\n", __FUNCTION__, mux_id);
		return;
	}

	memset(&sprd_mux_mgr[mux_id].mux, 0, sizeof(struct sprdmux));
	ts0710_mux_destory(mux_id);
}

int sprdmux_open(SPRDMUX_ID_E mux_id, int index)
{
	int rval = 0;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || index < 0 || index >= NR_MUXS) {
		printk(KERN_ERR "MUX: Error %s Invalid Param mux_id = %d, index = %d\n", __FUNCTION__, mux_id, index);
		return -EINVAL;
	}

	rval = ts0710_mux_status(mux_id);

	if (rval != 0) {
		printk(KERN_ERR "MUX: Error %s [%d][%d] mux_status is Not OK\n", __FUNCTION__, mux_id, index);
		return rval;
	}

	return ts0710_mux_open(mux_id, index);
}

void sprdmux_close(SPRDMUX_ID_E mux_id, int index)
{
	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || index < 0 || index >= NR_MUXS) {
		printk(KERN_ERR "MUX: Error %s Invalid Param mux_id = %d, index = %d\n", __FUNCTION__, mux_id, index);
		return;
	}

	if (ts0710_mux_status(mux_id) != 0) {
		printk(KERN_ERR "MUX: Error %s [%d][%d] mux_status is Not OK\n", __FUNCTION__, mux_id, index);
		return;
	}

	ts0710_mux_close(mux_id, index);
}

int sprdmux_write(SPRDMUX_ID_E mux_id, int index, const unsigned char *buf, int count)
{
	int rval = 0;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || index < 0 || index >= NR_MUXS) {
		printk(KERN_ERR "MUX: Error %s Invalid Param mux_id = %d, index = %d\n", __FUNCTION__, mux_id, index);
		return -EINVAL;
	}

	rval = ts0710_mux_status(mux_id);

	if (rval != 0) {
		printk(KERN_ERR "MUX: Error %s [%d][%d] mux_status is Not OK\n", __FUNCTION__, mux_id, index);
		return rval;
	}

	return ts0710_mux_write(mux_id, index, buf, count, 0);
}

int sprdmux_register_notify_callback(SPRDMUX_ID_E mux_id, struct sprdmux_notify *notify)
{
	struct sprd_mux *self = NULL;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX || !notify || notify->index >= NR_MUXS) {
		printk(KERN_ERR "MUX: Error %s Invalid Param\n", __FUNCTION__);
		return -EINVAL;
	}

	self = sprd_mux_mgr[mux_id].handle;

	if (self) {
		if (!self->callback[notify->index].func) {
			self->callback[notify->index].func = notify->func;
			self->callback[notify->index].user_data = notify->user_data;
		}

		return 0;
	}

	printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
	return -ENODEV;
}

static void mux_display_recv_info(const char * tag, int mux_id, int line)
{
	struct sprd_mux *self = NULL;
	mux_recv_struct *recv_info;
	int sum = 0;
	mux_recv_packet *recv_packet;

	if (mux_id < 0 || mux_id >= SPRDMUX_ID_MAX) {
		printk(KERN_ERR "MUX: Error %s %sInvalid Param mux_id = %d\n", __FUNCTION__, tag, mux_id);
		return;
	}

	if ((line < 0) || (line >= NR_MUXS)) {
		printk(KERN_ERR "MUX: Error %s %sline[%d] is wrong\n", __FUNCTION__, tag, line);
		return;

	}

	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX[%d]: Error %s %s self is NULL\n", mux_id,  __FUNCTION__, tag);
		mux_print_mux_mgr();
		return;
	}

	recv_info = self->mux_recv_info[line];

	if (recv_info == NULL){
		printk(KERN_ERR "MUX: Error %s %s mux_recv_info is NULL\n", __FUNCTION__, tag);
		return;
	}

	sum += recv_info->length;

	while((recv_packet = recv_info->mux_packet) != NULL) {
		sum += recv_packet->length;
		recv_packet = recv_packet->next;
	}

	if (sum != recv_info->total || sum != self->check_read_sum[line]) {
		printk(KERN_ERR "MUX: Error %s %s mux_id = %d, line = %d, total = %d, sum = %d check = %d len = %d\n", __FUNCTION__, tag, mux_id, line, recv_info->total, sum, self->check_read_sum[line], recv_info->length);
		while((recv_packet = recv_info->mux_packet) != NULL) {
			sum += recv_packet->length;
			recv_packet = recv_packet->next;
			printk(KERN_ERR "MUX: Error %s %s recv_packet->length = %d\n", __FUNCTION__, tag, recv_packet->length);
		}
	}
}

int ts0710_mux_status(int mux_id)
{
	if (mux_id >= SPRDMUX_ID_MAX || mux_id < 0) {
		printk(KERN_ERR "MUX: Error %s [%d] Invalid Param\n", __FUNCTION__, mux_id);
		return -EINVAL;
	}

	if (sprd_mux_mgr[mux_id].handle && sprd_mux_mgr[mux_id].mux_id == mux_id
		&& sprd_mux_mgr[mux_id].mux.io_read != NULL && sprd_mux_mgr[mux_id].mux.io_write!= NULL && sprd_mux_mgr[mux_id].mux.io_stop!= NULL) {

		return 0;
	}

	printk(KERN_ERR "MUX: Error %s  [%d] status is Not OK\n", __FUNCTION__, mux_id);

	return -ENODEV;
}

static int mux_handshake(struct sprd_mux *self)
{
	char buffer[256];
	char *buff = buffer;
	int count, i = 0;

	MUX_TS0710_DEBUG(self->mux_id, "cmux_mode = %d\n", self->cmux_mode);

	if (self->cmux_mode == 1) {
		printk(KERN_ERR "MUX: %s mux_mode is incorrect!\n", __FUNCTION__);
		return 0;
	}

	memset(buffer, 0, 256);
	printk(KERN_INFO "\n cmux say hello >, id %d \n", self->mux_id);

	if (mux_mode == 1) {
		count = self->io_hal->io_write("AT+SMMSWAP=0\r", strlen("AT+SMMSWAP=0\r"));
	} else {
		count = self->io_hal->io_write("AT\r", strlen("AT\r"));
	}

	if (count < 0) {
		printk(KERN_INFO "\n MUX: id = %d,cmux write stoped for crash\n", self->mux_id);
		return -1;
	}

	/*wait for response "OK \r" */
	printk(KERN_INFO "\n MUX: id = %d,cmux receiving\n", self->mux_id);
	mux_init_timer(self);
	while (1) {
		mux_start_timer(self);
		count = self->io_hal->io_read(buff, sizeof(buffer) - (buff - buffer));
		mux_stop_timer(self);
		printk(KERN_INFO "MUX: id = %d,ts mux received %d chars\n", self->mux_id, count);
		if(count > 0) {
			buff += count;
			if (findInBuf(buffer, 256, "OK")) {
				break;
			} else if (findInBuf(buffer, 256, "ERROR")) {
				printk(KERN_INFO "\n MUX: id = %d,wrong modem state !!!!\n", self->mux_id);
				break;
			}
		} else if (count == 0){
		        msleep(200);
			continue;
		} else {
			if (self->mux_status == MUX_STATE_CRASHED) {
				printk(KERN_INFO "\n MUX: id = %d,cmux read stoped for crash\n", self->mux_id);
				return -1;
			}
			msleep(2000);
			if (mux_mode == 1) {
				count = self->io_hal->io_write("AT+SMMSWAP=0\r", strlen("AT+SMMSWAP=0\r"));
			} else {
				count = self->io_hal->io_write("AT\r", strlen("AT\r"));
			}
			if (count < 0 && self->mux_status == MUX_STATE_CRASHED) {
				printk(KERN_INFO "\n MUX: id = %d,cmux read stoped for crash\n", self->mux_id);
				return -1;
			}
			i++;
		}
		if (i > 5) {
			printk(KERN_WARNING "\n MUX: id = %d,wrong modem state !!!!\n", self->mux_id);
			return -1;
		}
	}

	count = self->io_hal->io_write("at+cmux=0\r", strlen("at+cmux=0\r"));
	if (count < 0) {
		printk(KERN_INFO "\n MUX: id = %d,cmux write stoped\n", self->mux_id);
		return -1;
	}

	count = self->io_hal->io_read(buffer, sizeof(buffer));
	if (count < 0) {
		printk(KERN_INFO "\n MUX: id = %d,cmux read stoped\n", self->mux_id);
		return -1;
	}

	printk(KERN_INFO "MUX: id = %d, handshake OK\n", self->mux_id);

	return 0;
}

void  mux_ipc_enable(__u8 is_enable)
{
	int j;
	struct sprd_mux *self;

	printk(KERN_INFO "MUX: %s entered enable = %d\n", __FUNCTION__, is_enable);

	for (j = 0; j <sizeof(sprd_mux_mgr)/sizeof(sprd_mux_mgr[0]); j++) {
		if (sprd_mux_mgr[j].handle) {
			self = sprd_mux_mgr[j].handle;

			MUX_TS0710_DEBUG(self->mux_id, "enable = %d old state = %d\n", is_enable, self->mux_status);

			if (is_enable) {
				if (self->mux_status == MUX_STATE_NOT_READY) {
					self->mux_status = MUX_STATE_READY;
					wake_up_all(&self->modem_ready);
				} else if (self->mux_status == MUX_STATE_CRASHED) {
					mux_recover(self);
				} else {
					printk(KERN_ERR "MUX: %s wrong enable state \n", __FUNCTION__);
				}
			} else {
				if (self->mux_status == MUX_STATE_NOT_READY) {
					;//TODO:
				} else {
					mux_stop(self);
				}
			}
		}
	}
}

static void mux_wakeup_all_read(struct sprd_mux *self)
{
	int j;
	mux_recv_struct *recv_info;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return ;
	}

	for (j = 0; j < NR_MUXS; j++) {
		if (!(self->mux_recv_info_flags[j])) {
			continue;
		}

		recv_info = self->mux_recv_info[j];
		if (!recv_info) {
			continue;
		}

		wake_up_interruptible(&recv_info->rx_wait);
	}
}

static void mux_wakeup_all_write(struct sprd_mux *self)
{
	int j;
	mux_send_struct *send_info;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return ;
	}

	for (j = 0; j < NR_MUXS; j++) {
		if (!(self->mux_send_info_flags[j])) {
			continue;
		}

		send_info = self->mux_send_info[j];
		if (!send_info) {
			continue;
		}

		wake_up_interruptible(&send_info->tx_wait);
	}

}

static void mux_wakeup_all_opening(struct sprd_mux *self)
{
	int j;
	ts0710_con *ts0710;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return ;
	}

	ts0710 = self->connection;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		if (ts0710->dlci[j].state == CONNECTING) {
			wake_up_interruptible(&ts0710->dlci[j].open_wait);
		}
	}
}

static void mux_wakeup_all_closing(struct sprd_mux *self)
{
	int j;
	ts0710_con *ts0710;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return ;
	}

	ts0710 = self->connection;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		if (ts0710->dlci[j].state == DISCONNECTING) {
			wake_up_interruptible(&ts0710->dlci[j].close_wait);
		}
	}
}

static void mux_wakeup_all_wait(struct sprd_mux *self)
{
	mux_wakeup_all_read(self);
	mux_wakeup_all_write(self);
	mux_wakeup_all_opening(self);
	mux_wakeup_all_closing(self);

	if (self && self->connection) {
		wake_up_interruptible(&self->connection->test_wait);
	}
}

static void mux_tidy_buff(struct sprd_mux *self)
{
	int j;
	int k;
	mux_send_struct *send_info;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return ;
	}

	for (j = 0; j < NR_MUXS; j++) {
		if (!(self->mux_send_info_flags[j])) {
			continue;
		}

		send_info = self->mux_send_info[j];
		if (!send_info) {
			continue;
		}

		for (k = 0; k < ringbuf_num[self->mux_id][j]; k++) {
			send_info->length[k] = 0;
		}

		send_info->read_index = 0;
		send_info->write_index = 0;
	}

	memset(self->tbuf, 0, TS0710MUX_MAX_BUF_SIZE);
	self->tbuf_ptr = self->tbuf;
	self->start_flag = 0;
	self->framelen = -1;

	return;
}

static void mux_recover(struct sprd_mux *self)
{
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	MUX_TS0710_DEBUG(self->mux_id, "entered\n");

	self->mux_status = MUX_STATE_RECOVERING;

	if (self->mux_recover_kthread) {
		kthread_stop(self->mux_recover_kthread);
		schedule();
		self->mux_recover_kthread = NULL;
	}

	self->mux_recover_kthread = kthread_create(mux_recover_thread, self, "mux_recover");

	if(IS_ERR(self->mux_recover_kthread)) {
		printk(KERN_ERR "MUX: Error %s Unable to create mux_recover_kthread thread err = %ld\n", __FUNCTION__, PTR_ERR(self->mux_recover_kthread));

	}

	mux_tidy_buff(self);

	wake_up_process(self->mux_recover_kthread);
}

static int mux_recover_thread(void *data)
{
	struct sprd_mux *self = data;

	if (!self) {
		printk(KERN_ERR "MUX: %s self is NULL\n", __FUNCTION__);
		return 0;
	}

	printk(KERN_ERR "MUX: id[%d] %s entered\n", self->mux_id, __FUNCTION__);

	if (self->cmux_mode == 0) {
		mutex_lock(&self->handshake_mutex);
		if (mux_handshake(self) != 0) {
			printk(KERN_ERR "MUX: id[%d] %s handshake fail\n", self->mux_id, __FUNCTION__);
			mutex_unlock(&self->handshake_mutex);
			self->mux_recover_kthread = NULL;
			return 0;
		}

		self->cmux_mode = 1;
		wake_up(&self->handshake_ready);
		mutex_unlock(&self->handshake_mutex);
		while(!kthread_should_stop()) {
			 if (mux_restore_channel(self->connection) == 0) {
				self->mux_status = MUX_STATE_READY;

				wake_up_all(&self->modem_ready);
				mux_wakeup_all_read(self);
				mux_wakeup_all_write(self);
				self->mux_recover_kthread = NULL;
				printk(KERN_ERR "MUX: id[%d] %s recover successed\n", self->mux_id, __FUNCTION__);
				break;
			 } else {
				if (self->mux_status == MUX_STATE_CRASHED) {
					//another CP disable occured
					printk(KERN_ERR "MUX: id[%d] %s out anoter disable occured\n", self->mux_id, __FUNCTION__);
					break;
				}
				printk(KERN_ERR "MUX: id[%d] %s recover failed retry\n", self->mux_id, __FUNCTION__);
				msleep(500);
			 }
		}
	}

	printk(KERN_ERR "MUX: id[%d] %s out\n", self->mux_id, __FUNCTION__);

	return 0;
}

static void mux_stop(struct sprd_mux *self)
{
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	MUX_TS0710_DEBUG(self->mux_id, "entered\n");

	self->mux_status = MUX_STATE_CRASHED;
	self->cmux_mode = 0;
	self->io_hal->io_stop(SPRDMUX_ALL);

//	mux_wakeup_all_read(self);
//	mux_wakeup_all_write(self);
	mux_wakeup_all_opening(self);
	mux_wakeup_all_closing(self);

	if (self->connection) {
		wake_up_interruptible(&self->connection->test_wait);
	}

	if (self->mux_recover_kthread) {
		kthread_stop(self->mux_recover_kthread);
		schedule();
		self->mux_recover_kthread = NULL;
	}

	return;
}

static void mux_wakeup_channel(ts0710_con * ts0710)
{
	__u8 j;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return;
	}

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		if (ts0710->dlci[j].state == CONNECTING) {
			wake_up_interruptible(&ts0710->dlci[j].open_wait);
		}
	}

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		if (ts0710->dlci[j].state == DISCONNECTING) {
			wake_up_interruptible(&ts0710->dlci[j].close_wait);
		}
	}
}

static int mux_restore_channel(ts0710_con * ts0710)
{
	__u8 j;
	__u8 line;
	struct sprd_mux *self = NULL;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return -1;
	}

	self = (struct sprd_mux *)ts0710->user_data;
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return -1;
	}

	mux_display_connection(ts0710, "before restore");

	if (ts0710_ctrl_channel_status(self->connection) != 0) {
		printk(KERN_ERR "MUX: Error %s ctrl channel status is Not OK\n", __FUNCTION__);
		return -1;
	}

	for (j = 0; j < TS0710_MAX_CHN; j++) {
		line = dlci2line[j].dataline;
		if (ts0710->dlci[j].state == CONNECTING ||
			ts0710->dlci[j].state == NEGOTIATING||
			ts0710->dlci[j].state == CONNECTED ||
			self->open_count[line] != 0) {
			ts0710->dlci[j].state = DISCONNECTED;
			if (ts0710_open_channel(ts0710, j) != 0) {
				printk(KERN_ERR "MUX: Error %s failed\n", __FUNCTION__);
				return -1;
			}
		}
	}

	mux_display_connection(ts0710, "after restore");

	return 0;
}

static void mux_display_connection(ts0710_con * ts0710, const char *tag)
{
#ifdef TS0710DEBUG
		__u8 j;
		struct sprd_mux *self = NULL;

		if (!ts0710) {
			printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
			return;
		}

		self = (struct sprd_mux *)ts0710->user_data;
		if (!self) {
			printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
			return;
		}

		for (j = 0; j < TS0710_MAX_CHN; j++) {
			printk(KERN_ERR "MUX: %s id[%d] dlci[%d]'s state = %d\n", tag, self->mux_id, j, ts0710->dlci[j].state);
		}
#endif
	return;
}

static void mux_init_timer(struct sprd_mux *self)
{
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	init_timer(&self->watch_timer);
}

static void mux_start_timer(struct sprd_mux *self)
{
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	self->watch_timer.expires = jiffies + MUX_WATCH_INTERVAL;
	self->watch_timer.data = (unsigned long)self;
	self->watch_timer.function = mux_wathch_check;

	add_timer(&self->watch_timer);
}

static void mux_wathch_check(unsigned long priv)
{
	struct sprd_mux *self = (struct sprd_mux *) priv;

	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	printk(KERN_ERR "MUX: %s called\n", __FUNCTION__);

	if (self->cmux_mode == 0) {
		self->io_hal->io_stop(self->mux_id);
	}
}

static void mux_stop_timer(struct sprd_mux *self)
{
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return;
	}

	del_timer(&self->watch_timer);
}

static int ts0710_ctrl_channel_status(ts0710_con * ts0710)
{
	int retval = -ENODEV;
	struct sprd_mux *self = NULL;

	if (!ts0710) {
		printk(KERN_ERR "MUX: Error %s ts0710 is NULL\n", __FUNCTION__);
		return retval;
	}

	self = (struct sprd_mux *)ts0710->user_data;
	if (!self) {
		printk(KERN_ERR "MUX: Error %s self is NULL\n", __FUNCTION__);
		return retval;
	}

	if (ts0710->dlci[0].state != CONNECTED) {
		mutex_lock(&self->handshake_mutex);
		if (ts0710->dlci[0].state != CONNECTED) {
			if ((retval = ts0710_open_channel(ts0710, 0)) != 0) {
				printk(KERN_ERR "MUX: Error Can't connect server channel 0!\n");
				ts0710_init(ts0710);
				mutex_unlock(&self->handshake_mutex);
				return retval;
			}
		}
		mutex_unlock(&self->handshake_mutex);
	}

	return 0;
}

static void display_send_info(char * tag, SPRDMUX_ID_E mux_id, int line)
{
	struct sprd_mux *self = NULL;
	mux_send_struct *send_info;
	int j;
	self = sprd_mux_mgr[mux_id].handle;

	if (!self) {
		printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] pid[%d] self  NULL\n", tag, __FUNCTION__, mux_id, line, current->pid);
		return;
	}

	send_info = self->mux_send_info[line];

	if (!send_info) {
		printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] pid[%d] send_info  NULL\n", tag, __FUNCTION__, mux_id, line, current->pid);
		return;
	}

	if (!send_info->frame) {
		printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] pid[%d] send_info->frame  NULL\n", tag, __FUNCTION__, mux_id, line, current->pid);
		return;
	}

	if (!send_info->buf) {
		printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] pid[%d] send_info->buf  NULL\n", tag, __FUNCTION__, mux_id, line, current->pid);
		return;
	}

	if (!send_info->length) {
		printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] pid[%d] send_info->length  NULL\n", tag, __FUNCTION__, mux_id, line, current->pid);
		return;
	}

//	printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] buf = 0x%x, frame = 0x%x, length = 0x%x\n", tag, __FUNCTION__, mux_id, line, send_info->buf, send_info->frame, send_info->length);

	for (j = 0; j < ringbuf_num[mux_id][line]; j++) {
//		printk(KERN_ERR "MUX: %s, %s mux[%d] line[%d] index[%d] send_info->length[%d] = %u send_info->frame[%d] = 0x%u\n", tag, __FUNCTION__, mux_id, line, j,  send_info->length[j], j,send_info->frame[j]);
	}
}



EXPORT_SYMBOL(sprdmux_register);
EXPORT_SYMBOL(sprdmux_unregister);
EXPORT_SYMBOL(sprdmux_open);
EXPORT_SYMBOL(sprdmux_close);
EXPORT_SYMBOL(sprdmux_write);
EXPORT_SYMBOL(sprdmux_register_notify_callback);
EXPORT_SYMBOL(sprdmux_line_busy);
EXPORT_SYMBOL(sprdmux_set_line_notify);

EXPORT_SYMBOL(ts0710_mux_create);
EXPORT_SYMBOL(ts0710_mux_destory);
EXPORT_SYMBOL(ts0710_mux_init);
EXPORT_SYMBOL(ts0710_mux_exit);
EXPORT_SYMBOL(ts0710_mux_status);
EXPORT_SYMBOL(ts0710_mux_open);
EXPORT_SYMBOL(ts0710_mux_close);
EXPORT_SYMBOL(ts0710_mux_write);
EXPORT_SYMBOL(ts0710_mux_read);
EXPORT_SYMBOL(ts0710_mux_poll_wait);
EXPORT_SYMBOL(ts0710_mux_mux_ioctl);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Welte <laforge@openezx.org>");
MODULE_DESCRIPTION("GSM TS 07.10 Multiplexer");

