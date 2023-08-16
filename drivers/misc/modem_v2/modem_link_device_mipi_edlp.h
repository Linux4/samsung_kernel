/*
 * Copyright (C) 2012 Samsung Electronics.
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

#ifndef __MODEM_LINK_DEVICE_MIPI_EDLP_H__
#define __MODEM_LINK_DEVICE_MIPI_EDLP_H__

#define HSI_SEMAPHORE_COUNT	0

#define HSI_CHANNEL_TX_STATE_UNAVAIL	(1 << 0)
#define HSI_CHANNEL_TX_STATE_WRITING	(1 << 1)
#define HSI_CHANNEL_RX_STATE_UNAVAIL	(1 << 0)
#define HSI_CHANNEL_RX_STATE_READING	(1 << 1)

#define HSI_WRITE_DONE_TIMEOUT	(HZ)
#define HSI_READ_DONE_TIMEOUT	(HZ)
#define HSI_ACK_DONE_TIMEOUT	(HZ / 2)
#define HSI_ACWAKE_DOWN_TIMEOUT	(HZ)

#define HSI_TX_MODE_SYNC	0
#define HSI_TX_MODE_ASYNC	1

#define HSI_CONTROL_CHANNEL	0
#define HSI_FLASHLESS_CHANNEL	0
#define HSI_CP_RAMDUMP_CHANNEL	0
#define HSI_RAW_NET_A_CHANNEL	1
#define HSI_RAW_NET_B_CHANNEL	2
#define HSI_RAW_NET_C_CHANNEL	3
#define HSI_RAW_NET_D_CHANNEL	4
#define HSI_MULTI_FMT_CHANNEL	5
#define HSI_MODEM_DEBUG_CHANNEL	6
#define HSI_EXTRA_CHANNEL	7
#define HSI_NUM_OF_USE_CHANNELS	8

#define HSI_RX_0_CHAN_MAX_PDU	(32 * 1024)
#define HSI_RX_IPDATA_CHAN_PDU	(8 * 1024)
#define HSI_RX_FORMAT_CHAN_PDU	(4 * 1024 + 256)
#define HSI_RX_CPDEBUG_CHAN_PDU	(8 * 1024)
#define HSI_RX_EXTRA_CHAN_PDU	(128 * 1024 + 256)
#define HSI_TX_NETCTR_CHAN_PDU	(6 * 1024 + 512)

#define DUMP_PACKET_SIZE	4097 /* (16K + 4 ) / 4 length, word unit */
#define DUMP_ERR_INFO_SIZE	39 /* 150 bytes + 4 length , word unit */

#define HSI_EDLP_INVALID_CHANNEL	0xFF
#define HSI_EDLP_MSG_ECHO_CHECK_PATTERN	0xACAFFE
#define HSI_EDLP_MSG_NOP_CHECK_PATTERN	0xE7E7EC

#define HSI_EDLP_PDU_MAX_HEADER_COUNT	100
#define HSI_EDLP_PDU_SIGNATURE	0xF9A80000
#define HSI_EDLP_PDU_FIRST_ADDR	0x40 /* 64B */

#define EDLP_PACKET_MAX	256 /* can be varied */
#define EDLP_SIGN_SIZE	4
#define EDLP_PACKET_INFO_SIZE	8
#define HSI_BYTE_ALIGN	16
#define HSI_NET_PAD_FOR_CP 16

#define HSI_EDLP_SIGN_MASK 0xFFFF0000
#define HSI_EDLP_SIZE_MASK 0x0003FFFF
#define HSI_EDLP_SIZE(x)	((x) & HSI_EDLP_SIZE_MASK)
#define HSI_EDLP_N_FLD_MASK 0x80000000
#define HSI_EDLP_P_FLD_MASK 0x60000000
#define HSI_EDLP_CHECK_LAST(x) ((x) & HSI_EDLP_N_FLD_MASK ? 0 : 1)

#define get_edlp_align_size(x)	ALIGN((x), HSI_BYTE_ALIGN)
#define get_edlp_header_size(x)			\
	ALIGN(EDLP_SIGN_SIZE + EDLP_PACKET_INFO_SIZE * (x), HSI_BYTE_ALIGN)
#define set_as_middle(size)	((size) | HSI_EDLP_N_FLD_MASK)
#define set_as_last(size)	(size)

#define create_hi_prio_wq(name) \
	alloc_workqueue("hsi_%s_h_wq", \
			WQ_HIGHPRI | WQ_UNBOUND | WQ_RESCUER, 1, (name))
#define create_norm_prio_wq(name) \
	alloc_workqueue("hsi_%s_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1, (name))

enum {
	HSI_EDLP_MSG_BREAK = 0x0,
	HSI_EDLP_MSG_ECHO,
	HSI_EDLP_MSG_NOP = 0x4,
	HSI_EDLP_MSG_CONF_CH,
	HSI_EDLP_MSG_OPEN_CONN = 0x7,
	HSI_EDLP_MSG_CLOSE_CONN = 0xA,
	HSI_EDLP_MSG_ACK,
	HSI_EDLP_MSG_NAK,
	HSI_EDLP_MSG_CREDITS = 0xF,
	HSI_EDLP_MSG_INVALID = 0xFF,
};

struct if_hsi_edlp_pdu_header {
	u32 start_address;
	u32 flag_n_size;
} __packed;

struct if_hsi_edlp_pdu {
	u32 signature;
	struct if_hsi_edlp_pdu_header header[HSI_EDLP_PDU_MAX_HEADER_COUNT];
} __packed;

inline int if_hsi_edlp_check_sign(u32 *sign)
{
	return ((*sign & HSI_EDLP_SIGN_MASK) == HSI_EDLP_PDU_SIGNATURE) ? 1 : 0;
}

struct hsi_channel_info {
	char *name;
	int wq_priority;
	int rx_pdu_sz;
	int tx_pdu_sz;
	int use_credits;
	int is_dma_ch;
};

struct if_hsi_channel {
	unsigned int channel_id;
	unsigned int use_credits;
	atomic_t credits;
	int is_dma_ch;

	void *tx_data;
	int tx_pdu_size;
	unsigned int tx_count;
	void *rx_data;
	int rx_pdu_size;
	unsigned int rx_count;
	unsigned int packet_size;

	u32 *edlp_tx_pdu;
	u16 edlp_tx_seq_num;

	struct semaphore write_done_sem;
	struct semaphore conf_ch_done_sem;
	struct semaphore open_conn_done_sem;
	struct semaphore close_conn_done_sem;
	struct semaphore credits_done_sem;

	atomic_t opened;

	struct workqueue_struct *tx_wq;
	struct sk_buff_head tx_q;
	struct work_struct tx_w;

	/* reference to link device */
	struct link_device *ld;

	wait_queue_head_t credit_waitq;
};

struct mipi_link_device {
	struct link_device ld;

	/* mipi specific link data */
	struct if_hsi_channel hsi_channels[HSI_MAX_CHANNELS];

	struct delayed_work start_work;

	struct wake_lock wlock;
	struct timer_list hsi_acwake_down_timer;

	/* maybe -list of io devices for the link device to use
	 * to find where to send incoming packets to */
	struct list_head list_of_io_devices;

	/* hsi control data */
	unsigned int is_dma_capable;
	struct hsi_client *client;
	struct device *controller;

	atomic_t acwake;
	spinlock_t acwake_lock;
	spinlock_t credit_lock;
};
/* converts from struct link_device* to struct xxx_link_device* */
#define to_mipi_link_device(linkdev) \
			container_of(linkdev, struct mipi_link_device, ld)


enum {
	HSI_INIT_MODE_NORMAL,
	HSI_INIT_MODE_FLASHLESS_BOOT,
	HSI_INIT_MODE_CP_RAMDUMP,
	HSI_INIT_MODE_FLASHLESS_BOOT_EBL,
};

/*
 * HSI transfer complete callback prototype
 */
typedef void (*xfer_complete_cb) (struct hsi_msg *pdu);

static int if_hsi_set_wakeline(struct mipi_link_device *mipi_ld,
			unsigned int state);
static int if_hsi_open_channel(struct mipi_link_device *mipi_ld,
			unsigned int ch_num);
static int if_hsi_init_handshake(struct mipi_link_device *mipi_ld, int mode);
static struct io_device *if_hsi_get_iod_with_hsi_channel(
			struct mipi_link_device *mipi_ld, unsigned ch_num);
static u32 if_hsi_create_cmd(u32 cmd_type, int ch, u32 param);
static int if_hsi_send_command(struct mipi_link_device *mipi_ld,
			u32 cmd_type, int ch, u32 param);
static int if_hsi_edlp_send(struct mipi_link_device *mipi_ld,
			int ch, void *data, int len);
static void *if_hsi_buffer_alloc(struct mipi_link_device *mipi_ld,
			struct hsi_msg *msg, unsigned int size);
static void if_hsi_buffer_free(struct mipi_link_device *mipi_ld,
			struct hsi_msg *msg);
static void if_hsi_msg_destruct(struct hsi_msg *msg);
static void if_hsi_write_done(struct hsi_msg *msg);
static int if_hsi_write(struct mipi_link_device *mipi_ld, unsigned int ch_num,
			u32 *data, unsigned int size, int tx_mode);
static int if_hsi_read_async(struct mipi_link_device *mipi_ld,
			unsigned int ch_num, unsigned int size);
static void if_hsi_port_event(struct hsi_client *cl, unsigned long event);

#define MIPI_LOG_TAG "mipi_link: "

#define mipi_err(fmt, ...) \
	pr_err(MIPI_LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define mipi_debug(fmt, ...) \
	pr_debug(MIPI_LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define mipi_info(fmt, ...) \
	pr_info(MIPI_LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)

#endif
