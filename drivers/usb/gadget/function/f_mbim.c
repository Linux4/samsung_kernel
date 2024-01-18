#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/idr.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/hrtimer.h>
#include <linux/crc32.h>

#include <asm/byteorder.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>
#include <linux/usb/gadget.h>

#include <linux/mbim_queue.h>
#include "u_mbim.h"

#ifdef CONFIG_USB_SS_REMOTE_WAKEUP
#include "../dwc3/core.h"
#endif

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#define ETH_FRAME_LEN	1514

#define MAX_NOTIFY_REQUESTS (1)
#define MAX_BULKIN_REQUESTS	(8)
#define MAX_BULKOUT_REQUESTS	(4)

#define MBIM_BULK_BUFFER_SIZE	4096
#define USB_CDC_RESET		0x05

#define DEBUG			0
struct MBIM_MESSAGE_HEADER {
	unsigned int MessageType;
	// MSB:0, host -> function
	// MSB:1, host <- function
	unsigned int MessageLength; // Total Bytes
	unsigned int TransactionId;
};

#define MBIM_IOCTL_MAGIC		'o'
#define MBIM_GET_NTB_SIZE			_IOR(MBIM_IOCTL_MAGIC, 2, u32)
#define MBIM_GET_DATAGRAM_COUNT		_IOR(MBIM_IOCTL_MAGIC, 3, u16)

typedef struct _IOCTL_MBIM_GETCOMMAND_PARAM
{
	unsigned char Reserved[MBIM_BULK_BUFFER_SIZE];
}IOCTL_MBIM_GETCOMMAND_PARAM;

typedef struct _IOCTL_MBIM_SETRESPONSE_PARAM
{
	unsigned char Reserved[MBIM_BULK_BUFFER_SIZE];
}IOCTL_MBIM_SETRESPONSE_PARAM;

typedef struct _IOCTL_MBIM_CONNECT_INFO_PARAM
{
	int IsConnected;
}IOCTL_MBIM_CONNECT_INFO_PARAM;

#define IOCTL_MBIM_GETCOMMAND			_IOR( 'A', 4, IOCTL_MBIM_GETCOMMAND_PARAM )
#define IOCTL_MBIM_SETRESPONSE			_IOW( 'A', 5, IOCTL_MBIM_SETRESPONSE_PARAM )
#define IOCTL_MBIM_CONNECT_INFO			_IOW( 'A', 7, IOCTL_MBIM_CONNECT_INFO_PARAM )
#define IOCTL_MBIM_INFO_USBMODE			_IOR( 'A', 8, IOCTL_MBIM_GETCOMMAND_PARAM )

#define NR_MBIM_PORTS			1

#define MBIM_OS_STRING_ID   0xEE

#define MBIM_MINORS			4
#define GET_DEVICE_ID		0
#define GET_PORT_STATUS		1
#define SOFT_RESET			2

#define MBIM_MAX_TX_TIMEOUT_NSECS	3000000
#define MBIM_MIN_TX_TIMEOUT_NSECS	500000
#define MBIM_UL_SEND_TIME		1000000

#define MBIM_MAX_SEND_COUNT		64

static int major, minors;
static struct class *usb_gadget_class;
static DEFINE_IDA(mbim_ida);
static DEFINE_MUTEX(mbim_ida_lock);

struct ctrl_pkt {
	void		*buf;
	int			len;
	struct list_head	list;
};

/*-------------------------------------------------------------------------*/
enum mbim_notify_state {
	NCM_NOTIFY_NONE,
	NCM_NOTIFY_CONNECT,
	NCM_NOTIFY_SPEED,
	NCM_NOTIFY_RESPONSE_AVAILABLE,
};

struct mbim_dev {
	u8			ctrl_id, data_id;
	u8			data_alt_int;
	u16			ntb_max_datagrams;
	u32			ntb_input_size;
	spinlock_t		lock;
	struct mutex		lock_mbim_io;
	struct usb_gadget	*gadget;
	s8			interface;
	struct usb_ep		*in_ep, *out_ep, *notify_ep;
	//struct usb_request	*notify_req;
	atomic_t			notify_count;

	struct list_head	rx_reqs;
	struct list_head	rx_buffers;
	struct sk_buff_head	rx_sk_buffers;
	wait_queue_head_t	rx_wait;
	struct usb_request	*current_rx_req;
	size_t			current_rx_bytes;
	u8			*current_rx_buf;


	struct list_head	tx_reqs;
	struct list_head	tx_reqs_active;
	wait_queue_head_t	tx_wait;

	unsigned		tx_qlen;

	struct list_head	tx_notifyreqs;

	struct list_head	ul_shared_reqs;
	struct list_head	dl_shared_reqs;

	atomic_t		read_excl;
	atomic_t		write_excl;
	wait_queue_head_t	read_wq;
	wait_queue_head_t	write_wq;
	wait_queue_head_t	sync_wq;
	struct list_head	cpkt_req_q;
	struct list_head	cpkt_resp_q;

	bool			EnabledDevice;
	bool			mbim_ready;
	bool			mbim_online;

	spinlock_t		notify_req_lock;

	u8			reset_mbim;
	int			minor;
	struct usb_composite_dev *mbim_cdev;
	u8			mbim_cdev_open;
	wait_queue_head_t	wait;
	unsigned		q_len;
	char			*pnp_string;	/* We don't own memory! */
	struct usb_function	function;

	struct miscdevice	miscdev;
	struct net_device	*ndev;

	struct workqueue_struct	*mbim_dl;
	struct workqueue_struct	*mbim_ul;

	struct work_struct	start_xmit;
	struct hrtimer		tx_timer;
	struct hrtimer		rx_timer;
	bool en_timer;

	unsigned                dl_max_pkts_per_xfer;
	unsigned                ul_max_pkts_per_xfer;

	/* NCM requires fixed size bundles */
	bool			is_fixed;
	u32			fixed_out_len;
	u32			fixed_in_len;

	unsigned		qmult;

	bool			zlp;
	bool			occurred_timeout;

	spinlock_t		req_lock;	/* guard {rx,tx}_reqs */

	struct mbim_queue	*queue;
	struct mnet_data_info	*mnet_info;
	bool			wrap;
	bool			unwrap;
	int			func_flag;
	int			tx_skb_hold_count;
	/* pc wakeup gpio */
	int			host_wakeup_gpio;
	/* for debug */
	int			cnt_notify_int;
};

struct mbim_port{
	struct mbim_dev *mbim;
};


static void setup_rx_reqs(struct mbim_dev *dev);

static inline struct mbim_dev *func_to_mbim(struct usb_function *f)
{
	return container_of(f, struct mbim_dev, function);
}

/*-------------------------------------------------------------------------*/

/*
 * DESCRIPTORS ... most are static, but strings and (full) configuration
 * descriptors are built on demand.
 */

 /* holds our biggest descriptor */
#define USB_DESC_BUFSIZE		256
#define USB_DL_BUFSIZE			80 * 1024
#define USB_BUFSIZE			64 * 1024

#define NCM_STATUS_INTERVAL_MS		32
#define NCM_STATUS_BYTECOUNT		16	/* 8 byte header + data */

#define NDP_IN_DIVISOR		(0x1)

#define NTB_DEFAULT_IN_SIZE_IPA	(0x2000)
#define NTB_OUT_SIZE_IPA		(0x2000)

#define FORMATS_SUPPORTED	USB_CDC_NCM_NTB16_SUPPORTED

struct usb_cdc_ncm_ntb_parameters ntb_parameters = {
	.wLength = sizeof ntb_parameters,
	.bmNtbFormatsSupported = cpu_to_le16(FORMATS_SUPPORTED),
	.dwNtbInMaxSize = cpu_to_le32(NTB_DEFAULT_IN_SIZE),
	.wNdpInDivisor = cpu_to_le16(NDP_IN_DIVISOR),
	.wNdpInPayloadRemainder = cpu_to_le16(0),
	.wNdpInAlignment = cpu_to_le16(4),

	.dwNtbOutMaxSize = cpu_to_le32(NTB_OUT_SIZE),
	.wNdpOutDivisor = cpu_to_le16(32),
	.wNdpOutPayloadRemainder = cpu_to_le16(0),
	.wNdpOutAlignment = cpu_to_le16(4),
	.wNtbOutMaxDatagrams = 32,
};

struct usb_interface_assoc_descriptor mbim_iad_desc = {
	.bLength = sizeof mbim_iad_desc,
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount = 2,	/* control + data */
	.bFunctionClass = USB_CLASS_COMM,
	.bFunctionSubClass = USB_CDC_SUBCLASS_MBIM,
	.bFunctionProtocol = USB_CDC_PROTO_NONE,
	/* .iFunction =		DYNAMIC */
};

/* interface descriptor: */

struct usb_interface_descriptor mbim_control_intf = {
	.bLength = sizeof mbim_control_intf,
	.bDescriptorType = USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_COMM,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_MBIM,
	.bInterfaceProtocol = USB_CDC_PROTO_NONE,
	/* .iInterface = DYNAMIC */
};

struct usb_cdc_header_desc mbim_header_desc = {
	.bLength = sizeof mbim_header_desc,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_HEADER_TYPE,

	.bcdCDC = cpu_to_le16(0x0110),
};

struct usb_cdc_union_desc mbim_union_desc = {
	.bLength = sizeof(mbim_union_desc),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

struct usb_cdc_mbim_desc mbim_desc = {
	.bLength = sizeof mbim_desc,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_MBIM_TYPE,

	.bcdMBIMVersion = cpu_to_le16(0x0100),

	.wMaxControlMessage = cpu_to_le16(0x1000),
	.bNumberFilters = 0x20,
	.bMaxFilterSize = 0x80,
	.wMaxSegmentSize = cpu_to_le16(0xfe0),
	.bmNetworkCapabilities = 0x20,
};

struct usb_cdc_ext_mbb_desc {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubType;
	__le16 bcdMbbExtendedVersion;
	__u8 bMaxOutstandingCmdMsges;
	__le16 wMTU;
}__attribute__((packed));

struct usb_cdc_ext_mbb_desc ext_mbb_desc = {
	.bLength = sizeof ext_mbb_desc,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = 0x1c,
	/* Mbim expansion version for 5G should be set to version 2.0 */
	.bcdMbbExtendedVersion = cpu_to_le16(0x0200),
	.bMaxOutstandingCmdMsges = 64,
	.wMTU = 1500,
};

/* the default data interface has no endpoints ... */

struct usb_interface_descriptor mbim_data_nop_intf = {
	.bLength = sizeof mbim_data_nop_intf,
	.bDescriptorType = USB_DT_INTERFACE,

	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_CDC_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = USB_CDC_MBIM_PROTO_NTB,
	/* .iInterface = DYNAMIC */
};

/* ... but the "real" data interface has two bulk endpoints */

struct usb_interface_descriptor mbim_data_intf = {
	.bLength = sizeof mbim_data_intf,
	.bDescriptorType = USB_DT_INTERFACE,

	.bInterfaceNumber = 1,
	.bAlternateSetting = 1,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_CDC_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = USB_CDC_MBIM_PROTO_NTB,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */
#define LOG2_STATUS_INTERVAL_MSEC	8

struct usb_endpoint_descriptor fs_mbim_notify_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = 4 * cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval = 1,
};

struct usb_endpoint_descriptor fs_mbim_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

struct usb_endpoint_descriptor fs_mbim_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

struct usb_descriptor_header *mbim_fs_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* CDC MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &ext_mbb_desc,
	(struct usb_descriptor_header *) &fs_mbim_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &fs_mbim_in_desc,
	(struct usb_descriptor_header *) &fs_mbim_out_desc,
	NULL,
};

/* high speed support: */

struct usb_endpoint_descriptor hs_mbim_notify_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = 4 * cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval = LOG2_STATUS_INTERVAL_MSEC + 4,
};
struct usb_endpoint_descriptor hs_mbim_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
};

struct usb_endpoint_descriptor hs_mbim_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
};

struct usb_descriptor_header *mbim_hs_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* CDC MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &ext_mbb_desc,
	(struct usb_descriptor_header *) &hs_mbim_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &hs_mbim_in_desc,
	(struct usb_descriptor_header *) &hs_mbim_out_desc,
	NULL,
};


/* super speed support: */

struct usb_endpoint_descriptor ss_mbim_notify_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = 4 * cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval = USB_MS_TO_HS_INTERVAL(NCM_STATUS_INTERVAL_MS)
};

struct usb_ss_ep_comp_descriptor ss_mbim_notify_comp_desc = {
	.bLength = sizeof(ss_mbim_notify_comp_desc),
	.bDescriptorType = USB_DT_SS_ENDPOINT_COMP,

	/* the following 3 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
	.wBytesPerInterval = 4 * cpu_to_le16(NCM_STATUS_BYTECOUNT),
};

struct usb_endpoint_descriptor ss_mbim_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(1024),
};

struct usb_endpoint_descriptor ss_mbim_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(1024),
};

struct usb_ss_ep_comp_descriptor ss_mbim_bulk_comp_desc = {
	.bLength = sizeof(ss_mbim_bulk_comp_desc),
	.bDescriptorType = USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	 .bMaxBurst =		4,
	/* .bmAttributes =	0, */
};

struct usb_descriptor_header *mbim_ss_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* CDC NBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &ext_mbb_desc,
	(struct usb_descriptor_header *) &ss_mbim_notify_desc,
	(struct usb_descriptor_header *) &ss_mbim_notify_comp_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &ss_mbim_in_desc,
	(struct usb_descriptor_header *) &ss_mbim_bulk_comp_desc,
	(struct usb_descriptor_header *) &ss_mbim_out_desc,
	(struct usb_descriptor_header *) &ss_mbim_bulk_comp_desc,
	NULL,
};


/* string IDs are assigned dynamically */
#define F_MBIM_IDX			0

static struct usb_string mbim_string_defs[] = {
	[F_MBIM_IDX].s = "Samsung Mobile Broadband Adapter",
	{  } /* end of list */
};

static struct usb_gadget_strings mbim_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		mbim_string_defs,
};
static struct usb_gadget_strings *mbim_strings[] = {
	&mbim_string_table,
	NULL,
};

static struct mbim_dev *_mbim_dev;

/* maxpacket and other transfer characteristics vary by speed. */
static inline struct usb_endpoint_descriptor *ep_desc(struct usb_gadget *gadget,
	struct usb_endpoint_descriptor *fs,
	struct usb_endpoint_descriptor *hs,
	struct usb_endpoint_descriptor *ss)
{
	switch (gadget->speed) {
	case USB_SPEED_SUPER:
		return ss;
	case USB_SPEED_HIGH:
		return hs;
	default:
		return fs;
	}
}


static inline int mbim_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	}
	else {
		atomic_dec(excl);
		return -EBUSY;
	}
}

static inline void mbim_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static struct ctrl_pkt *mbim_alloc_ctrl_pkt(unsigned len, gfp_t flags)
{
	struct ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void mbim_free_ctrl_pkt(struct ctrl_pkt *pkt)
{
	if (pkt) {
		kfree(pkt->buf);
		kfree(pkt);
	}
}

static void mbim_clear_queues(struct mbim_dev	*dev)
{
	struct ctrl_pkt	*cpkt = NULL;
	struct list_head *act, *tmp;
	unsigned long flags;

	pr_info("usb: %s: mbim_clear_queues +++\n", __func__);

	spin_lock_irqsave(&dev->notify_req_lock, flags);
	list_for_each_safe(act, tmp, &dev->cpkt_req_q) {
		cpkt = list_entry(act, struct ctrl_pkt, list);
		list_del(&cpkt->list);
		mbim_free_ctrl_pkt(cpkt);
	}
	list_for_each_safe(act, tmp, &dev->cpkt_resp_q) {
		cpkt = list_entry(act, struct ctrl_pkt, list);
		list_del(&cpkt->list);
		mbim_free_ctrl_pkt(cpkt);
	}
	spin_unlock_irqrestore(&dev->notify_req_lock, flags);
	pr_info("usb: %s: mbim_clear_queues ---\n", __func__);
}

static void mbim_clear_data_queues(struct mbim_dev *dev)
{
	struct sk_buff		*skb;

	pr_info("usb: %s: mbim data queue clear\n", __func__);

	while(!mbim_queue_empty()) {
		skb = mbim_dequeue_tail();
		dev_kfree_skb_any(skb);
	}	
}

/*-------------------------------------------------------------------------*/

static struct usb_request *
mbim_req_alloc(struct usb_ep *ep, unsigned len, gfp_t gfp_flags)
{
	struct usb_request	*req;

	req = usb_ep_alloc_request(ep, gfp_flags);

	if (req != NULL) {
		req->length = 0;
		if ( len ) {
			req->buf = kmalloc(len, gfp_flags);
			if (req->buf == NULL) {
				usb_ep_free_request(ep, req);
				return NULL;
			}
		}
	}

	return req;
}

static void mbim_req_free(struct usb_ep *ep, struct usb_request *req)
{
	if (ep != NULL && req != NULL) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static struct sk_buff *mnet_wrap_ntb(struct mbim_dev *dev, struct sk_buff *skb, struct usb_request *req)
{
	struct mnet_data_info *mnet_info = dev->mnet_info;
	int		ncb_len = 0;
	__le16		*tmp;
	int		div;
	int		rem;
	int		pad;
	int		ndp_align;
	int		ndp_pad;
	unsigned	max_size = mnet_info->port.fixed_in_len;
	const struct ndp_parser_opts *opts = mnet_info->parser_opts;
	unsigned	crc_len = mnet_info->is_crc ? sizeof(uint32_t) : 0;
#if DEBUG
	pr_info("usb: %s: +++\n", __func__);
#endif
	/* If multi-packet is enabled, ncm header will be added directly
	 * in USB request buffer.
	 */
	div = le16_to_cpu(ntb_parameters.wNdpInDivisor);
	rem = le16_to_cpu(ntb_parameters.wNdpInPayloadRemainder);
	ndp_align = le16_to_cpu(ntb_parameters.wNdpInAlignment);

	ncb_len += opts->nth_size;
	ndp_pad = ALIGN(ncb_len, ndp_align) - ncb_len;
	ncb_len += ndp_pad;
	ncb_len += opts->ndp_size;
	ncb_len += 2 * 2 * opts->dgram_item_len * MBIM_MAX_DATAGRAM; /* Datagram entry */
	ncb_len += 2 * 2 * opts->dgram_item_len; /* Zero datagram entry */
	pad = ALIGN(ncb_len, div) + rem - ncb_len;
	ncb_len += pad;

	if (ncb_len + skb->len + crc_len > max_size) {
		printk(KERN_ERR"usb: %s Dropped skb skblen (%d) \n", __func__, skb->len);
		dev_kfree_skb_any(skb);
		return NULL;
	}

	tmp = (void *) req->buf;
	memset(tmp, 0, ncb_len);

	put_unaligned_le32(opts->nth_sign, tmp); /* dwSignature */
	tmp += 2;
	/* wHeaderLength */
	put_unaligned_le16(opts->nth_size, tmp++);
	/* wSequence */
	put_unaligned_le16(++mnet_info->nth_sequence, tmp++);
	put_ncm(&tmp, opts->block_length, skb->len + ncb_len); /* (d)wBlockLength */

	/* (d)wFpIndex */
	/* the first pointer is right after the NTH + align */
	put_ncm(&tmp, opts->ndp_index, opts->nth_size + ndp_pad);

	tmp = (void *)tmp + ndp_pad;

	/* NDP */
	put_unaligned_le32(opts->ndp_sign, tmp); /* dwSignature */
	tmp += 2;
	/* wLength */
	put_unaligned_le16(ncb_len - opts->nth_size - pad, tmp++);

	tmp += opts->reserved1;
	tmp += opts->next_ndp_index; /* skip reserved (d)wNextFpIndex */
	tmp += opts->reserved2;

	if (mnet_info->is_crc) {
		uint32_t crc;

		crc = ~crc32_le(~0,
				skb->data + ncb_len,
				skb->len - ncb_len);
		put_unaligned_le32(crc, skb->data + skb->len);
		skb_put(skb, crc_len);
	}

	/* (d)wDatagramIndex[0] */
	put_ncm(&tmp, opts->dgram_item_len, ncb_len);
	/* (d)wDatagramLength[0] */
	put_ncm(&tmp, opts->dgram_item_len, skb->len);
	/* (d)wDatagramIndex[1] and  (d)wDatagramLength[1] already zeroed */

	/* Incase of Higher MTU size we do not need to expand and zero off the remaining
	   In packet size to max_size . This saves bandwidth . e.g for 16K In size max mtu is 9k
	*/
	if (skb->len > MAX_TX_NONFIXED) {
		memset(skb_put(skb, max_size - skb->len),
		       0, max_size - skb->len);
#if DEBUG
		printk(KERN_DEBUG"usb:%s Expanding the buffer %d \n", __func__, skb->len);
#endif
	}

	memcpy(req->buf + ncb_len, skb->data, skb->len);
	req->length = skb->len + ncb_len + crc_len;

#if DEBUG
	pr_info("usb: %s: ---\n", __func__);
#endif
	return skb;

}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct mbim_dev	*dev = ep->driver_data;

	switch (req->status) {
	default:
		/* FALLTHROUGH */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		break;
	case 0:
		dev->ndev->stats.tx_bytes += req->length;
		dev->ndev->stats.tx_packets++;
#if DEBUG
		pr_info("usb: %s: transfer complete!! [req: %pK, size: %d]\n", __func__, req, req->length);
#endif
		break;
	}

#if DEBUG
	pr_info("usb: %s: transmit complete!! [%pK]\n", __func__, req);
#endif
	spin_lock(&dev->req_lock);
	/* Take the request struct off the active list and put it on the
	 * free list.
	 */
	list_del_init(&req->list);
	req->length = 0;

	list_add_tail(&req->list, &dev->tx_reqs);
	spin_unlock(&dev->req_lock);

	wake_up_interruptible(&dev->tx_wait);
}

static int  mnet_unwrap_ntb(struct mbim_dev *dev, struct usb_request *req, struct sk_buff_head *list)
{
	__le16		*tmp = (void *) req->buf;
	unsigned	index, index2;
	int		ndp_index;
	unsigned	dg_len, dg_len2;
	unsigned	ndp_len;
	struct sk_buff	*skb2;
	int		ret = -EINVAL;
	unsigned	max_size = le32_to_cpu(ntb_parameters.dwNtbOutMaxSize);
	const struct ndp_parser_opts *opts = dev->mnet_info->parser_opts;
	unsigned	crc_len = dev->mnet_info->is_crc ? sizeof(uint32_t) : 0;
	int		dgram_counter;
#if DEBUG
	pr_info("usb: %s: +++\n", __func__);
#endif
	/* dwSignature */
	if (get_unaligned_le32(tmp) != opts->nth_sign) {
		pr_err("usb: %s: Wrong NTH SIGN, skblen %d[%x : %x]\n", __func__, req->actual
			, get_unaligned_le32(tmp), opts->nth_sign);
		print_hex_dump(KERN_INFO, "HEAD:", DUMP_PREFIX_ADDRESS, 32, 1,
			       req->buf, 32, false);

		goto err;
	}
	tmp += 2;

	/* wHeaderLength */
	if (get_unaligned_le16(tmp++) != opts->nth_size) {
		pr_err("usb: %s: Wrong NTB headersize [%x : %x]", __func__
				, get_unaligned_le16(tmp++), opts->nth_size);
		goto err;
	}
	tmp++; /* skip wSequence */

	/* (d)wBlockLength */
	if (get_ncm(&tmp, opts->block_length) > max_size) {
		pr_err("usb: %s: OUT size exceeded\n", __func__);
		goto err;
	}

	ndp_index = get_ncm(&tmp, opts->ndp_index);

	/* Run through all the NDP's in the NTB */
	do {
		/* NCM 3.2 */
		if (((ndp_index % 4) != 0) &&
				(ndp_index < opts->nth_size)) {
			pr_err("usb: %s: Bad index: %#X\n", __func__, ndp_index);
			goto err;
		}

		/* walk through NDP */
		tmp = (void *)(req->buf + ndp_index);
		if (get_unaligned_le32(tmp) != opts->ndp_sign) {
			pr_err("usb: %s: Wrong NDP IPS SIGN [%x : %x]", __func__
				, get_unaligned_le32(tmp), opts->ndp_sign);
			goto err;
		}
		tmp += 2;

		ndp_len = get_unaligned_le16(tmp++);

		/*
		 * NCM 3.3.1
		 * entry is 2 items
		 * item size is 16/32 bits, opts->dgram_item_len * 2 bytes
		 * minimal: struct usb_cdc_ncm_ndpX + normal entry + zero entry
		 * Each entry is a dgram index and a dgram length.
		 */
		if ((ndp_len < opts->ndp_size
				+ 2 * 2 * (opts->dgram_item_len * 2))
				|| (ndp_len % opts->ndplen_align != 0)) {
			pr_err("usb: %s: Bad NDP length: %#X\n", __func__, ndp_len);
			goto err;
		}
		tmp += opts->reserved1;
		/* Check for another NDP (d)wNextNdpIndex */
		ndp_index = get_ncm(&tmp, opts->next_ndp_index);
		tmp += opts->reserved2;

		ndp_len -= opts->ndp_size;
		index2 = get_ncm(&tmp, opts->dgram_item_len);
		dg_len2 = get_ncm(&tmp, opts->dgram_item_len);
		dgram_counter = 0;

		do {
			index = index2;
			dg_len = dg_len2;
			if (dg_len < 14 + crc_len) { /* ethernet hdr + crc */
				pr_err("usb: %s: Bad dgram length: %#X\n", __func__, dg_len);
				goto err;
			}
			if (dev->mnet_info->is_crc) {
				uint32_t crc, crc2;

				crc = get_unaligned_le32(req->buf +
							 index + dg_len -
							 crc_len);
				crc2 = ~crc32_le(~0,
						 req->buf + index,
						 dg_len - crc_len);
				if (crc != crc2) {
					pr_err("usb: %s: Bad CRC\n", __func__);
					goto err;
				}
			}

			index2 = get_ncm(&tmp, opts->dgram_item_len);
			dg_len2 = get_ncm(&tmp, opts->dgram_item_len);

			/*
			 * Copy the data into a new skb.
			 * This ensures the truesize is correct
			 */
			skb2 = netdev_alloc_skb_ip_align(dev->ndev, dg_len - crc_len);
			if (skb2 == NULL) {
				pr_err("%s: alloc fail!!\n", __func__);
				goto err;
			}
			skb_put_data(skb2, req->buf + index,
				     dg_len - crc_len);

			skb_queue_tail(list, skb2);

			ndp_len -= 2 * (opts->dgram_item_len * 2);

			dgram_counter++;

			if (index2 == 0 || dg_len2 == 0)
				break;
		} while (ndp_len > 2 * (opts->dgram_item_len * 2));
	} while (ndp_index);
#if DEBUG
	pr_err("usb: %s: Parsed NTB with %d frames\n", __func__, dgram_counter);
	pr_info("usb: %s: ---\n", __func__);
#endif
	return 0;
err:
	pr_info("usb: %s: occurred err: %d\n", __func__, ret);
	return ret;
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct mbim_dev	*dev = ep->driver_data;
//	struct sk_buff	*skb = req->context;
	int			status = req->status;
	unsigned long		flags;

#if DEBUG
	pr_info("usb: %s: ############# rx_complete[%pK, req: %pK] \n", __func__, req);
#endif
	spin_lock_irqsave(&dev->lock, flags);

	list_del_init(&req->list);	/* Remode from Active List */

	switch (status) {

		/* normal completion */
	case 0:
		if (req->actual > 0) {

			// for ncm
			if (dev->unwrap) {
#if DEBUG
				pr_err("usb: %s: enter unwrap ntb\n", __func__);
#endif
//				skb->data = req->buf;
//				skb->len = req->actual;
				status = mnet_unwrap_ntb(dev, req, &dev->rx_sk_buffers);
				if (status) {
					pr_err("usb: %s: ncm unwrap error: %d\n", __func__, status);
					dev->ndev->stats.rx_errors++;
				}
			}
#if DEBUG
			pr_err("usb: %s: start rx_work ntb\n", __func__);
#endif
			list_add_tail(&req->list, &dev->rx_reqs);
		}
		else {
			list_add_tail(&req->list, &dev->rx_reqs);
		}
		break;

		/* software-driven interface shutdown */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		pr_info("usb: %s: rx shutdown, code %d\n", __func__, status);
		list_add_tail(&req->list, &dev->rx_reqs);
		break;

		/* for hardware automagic (such as pxa) */
	case -ECONNABORTED:		/* endpoint reset */
		pr_info("usb: rx %s reset\n", ep->name);
		list_add_tail(&req->list, &dev->rx_reqs);
		break;

		/* data overrun */
	case -EOVERFLOW:
		/* FALLTHROUGH */

	default:
		pr_info("usb: rx status %d\n", status);
		list_add_tail(&req->list, &dev->rx_reqs);
		break;
	}

	wake_up_interruptible(&dev->rx_wait);
	spin_unlock_irqrestore(&dev->lock, flags);
	if (status == 0)
		setup_rx_reqs(dev);
}

#if DEBUG
void dumpData(unsigned char *data, ssize_t size)
{

	ssize_t x, y;
	int i = 0;

	pr_info("usb: len=%ld", size);

	for (y = 0; y < size / 16; y++)
	{
		for (x = 0; x < 16; x++)
		{
			pr_info("%02X ", data[i]);
			i++;
		}
		pr_info("\n");
	}
	for (x = 0; x < size % 16; x++)
	{
		pr_info("%02X ", data[i]);
		i++;
	}
	pr_info("\n");
	pr_info("\n");
	return;

}
#endif

static void mbim_call_interrupt_enqueue(struct mbim_dev	*dev, void * buf, size_t len)
{
	struct usb_request		*req = NULL;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->notify_req_lock, flags);
	if(!list_empty(&dev->tx_notifyreqs))
	{
		req = container_of(dev->tx_notifyreqs.next,
			struct usb_request, list);
		list_del_init(&req->list);
	} else {
		spin_unlock_irqrestore(&dev->notify_req_lock, flags);
		printk("usb: %s : queue empty : dev->notify_count = %d \n",
					__func__, atomic_read(&dev->notify_count));
		atomic_inc(&dev->notify_count);
		goto exit;
	}
	spin_unlock_irqrestore(&dev->notify_req_lock, flags);

	if (dev->notify_ep->driver_data == NULL) {
		pr_err("usb: %s: ep is not enabled.. return error\n", __func__);
		spin_lock_irqsave(&dev->notify_req_lock, flags);
		list_add_tail(&req->list, &dev->tx_notifyreqs);
		spin_unlock_irqrestore(&dev->notify_req_lock, flags);
		goto exit;
	}

	req->length = len;
	memcpy(req->buf, buf, len);
#if DEBUG
	dumpData(buf, len);
#endif
	ret = usb_ep_queue(dev->notify_ep,
		req, GFP_ATOMIC);

	if (ret) {
		pr_info("usb: %s: notify_ep enqueue error %d\n", __func__, ret);
		spin_lock_irqsave(&dev->notify_req_lock, flags);
		list_add_tail(&req->list, &dev->tx_notifyreqs);
		spin_unlock_irqrestore(&dev->notify_req_lock, flags);
	}
exit:
	return;
}

static void mbim_do_notify_response_avaliable(struct mbim_dev	*dev)
{
	struct usb_cdc_notification	event;

	event.bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
		| USB_RECIP_INTERFACE;
	event.bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
	event.wValue = cpu_to_le16(0);
	event.wIndex = cpu_to_le16(dev->ctrl_id);
	event.wLength = cpu_to_le16(0);

	mbim_call_interrupt_enqueue(
		dev, 
		&event,
		sizeof(struct usb_cdc_notification)
	);
}

static void mbim_do_notify_connect(struct mbim_dev	*dev)
{
	struct usb_cdc_notification	event;

	event.bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
		| USB_RECIP_INTERFACE;
	event.bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
	event.wValue = cpu_to_le16(1);
	event.wIndex = cpu_to_le16(dev->ctrl_id);
	event.wLength = cpu_to_le16(0);

	mbim_call_interrupt_enqueue(
		dev,
		&event,
		sizeof(struct usb_cdc_notification)
	);
}

static void mbim_do_notify_disconnect(struct mbim_dev	*dev)
{
	struct usb_cdc_notification	event;

	event.bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
		| USB_RECIP_INTERFACE;
	event.bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
	event.wValue = cpu_to_le16(0);
	event.wIndex = cpu_to_le16(dev->ctrl_id);
	event.wLength = cpu_to_le16(0);

	mbim_call_interrupt_enqueue(
		dev,
		&event,
		sizeof(struct usb_cdc_notification)
	);
}

static int
fmbim_send_cpkt_response(struct mbim_dev *gr, struct ctrl_pkt *cpkt)
{
	struct mbim_dev	*dev = gr;
	unsigned long	flags;

	spin_lock_irqsave(&dev->notify_req_lock, flags);
	list_add_tail(&cpkt->list, &dev->cpkt_resp_q);
	spin_unlock_irqrestore(&dev->notify_req_lock, flags);

//	dev->notify_state = NCM_NOTIFY_RESPONSE_AVAILABLE;
	mbim_do_notify_response_avaliable(dev);

	return 0;
}

/*-------------------------------------------------------------------------*/

static int mnet_open(struct net_device *ndev)
{
	struct mbim_port *port = netdev_priv(ndev);
	struct mbim_dev	*dev = port->mbim;

	pr_info("usb: %s: +++\n", __func__);

	dev->tx_qlen = 0;
	netif_wake_queue(ndev);

	pr_info("usb: %s: ---\n", __func__);
	return 0;
}

static int mnet_close(struct net_device *ndev)
{
	pr_info("usb: %s: +++\n", __func__);
	netif_stop_queue(ndev);
	pr_info("usb: %s: ---\n", __func__);
	return 0;
}

int get_current_length(__le16 *tmp)
{
	return get_unaligned_le16(tmp + 10) + get_unaligned_le16(tmp + 11);
}

void mbim_add_datagram(__le16 *tmp, int length, int holdcnt)
{
	int tmp_val;
	__le16 *tmp_addr;
	u32 prev_index, prev_len;

	tmp += 4;
	tmp_val = get_unaligned_le16(tmp);		// wBlockLength
	put_unaligned_le16(tmp_val + length, tmp);
	tmp_val = get_unaligned_le16(tmp);

	tmp_addr = tmp + 6;

	tmp_addr += ((holdcnt - 1) * 2);

	prev_index = get_unaligned_le16(tmp_addr);
	tmp_addr += 1;
	prev_len = get_unaligned_le16(tmp_addr);
	tmp_addr += 1;
#if DEBUG
	pr_info("usb: %s: prev_index: %08X, prev_len: %d\n", __func__, prev_index, prev_len);
#endif
	// calc ips# header
	put_unaligned_le16(prev_index + prev_len, tmp_addr);
	tmp_addr += 1;
	put_unaligned_le16(length, tmp_addr);
#if DEBUG
	pr_info("usb: %s: next_index: %08X, next_len: %d\n", __func__, prev_index + prev_len, length);
#endif
}

static enum hrtimer_restart tx_timeout(struct hrtimer *data)
{
	struct mbim_dev *dev = container_of(data, struct mbim_dev, tx_timer);

#if DEBUG
	printk("usb: %s : timer timeout \n",__func__);
#endif

	dev->occurred_timeout = 1;
	wake_up(&dev->queue->dl_wait);

	return HRTIMER_NORESTART;
}
static enum hrtimer_restart rx_timeout(struct hrtimer *data)
{
	struct mbim_dev *dev = container_of(data, struct mbim_dev, rx_timer);
	struct sk_buff		*skb;
	int ret;
	int		cnt = 0;

#if DEBUG
	pr_info("usb: %s: +++\n", __func__);
#endif
	if (dev->EnabledDevice == false)
		return HRTIMER_NORESTART;

	if (unlikely(skb_queue_empty(&dev->rx_sk_buffers)))
		goto retry;

	while (likely(skb = skb_dequeue(&dev->rx_sk_buffers))) {

		dev->ndev->stats.rx_packets++;
		dev->ndev->stats.rx_bytes += skb->len;

		ret = mbim_send_skb_to_cpif(skb);
		if (ret)
			dev->ndev->stats.rx_errors++;

		if (dev->EnabledDevice == false)
			return HRTIMER_NORESTART;
		if (cnt++ >= MBIM_MAX_SEND_COUNT)
			break;
	}

retry:
	hrtimer_start(&dev->rx_timer, ktime_set(0, MBIM_UL_SEND_TIME), HRTIMER_MODE_REL);
#if DEBUG
	pr_info("usb: %s: ---\n", __func__);
#endif
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_USB_SS_REMOTE_WAKEUP
int mbim_resume_cmpl;
#endif
static void mnet_start_xmit(struct work_struct *work)
{
	struct mbim_dev *dev = container_of(work, struct mbim_dev, start_xmit);
	unsigned long		flags;
	struct usb_request	*req;
	struct sk_buff		*skb;
	int			ret;
	int			added_offset = 0;
	static unsigned long	tx_timeout = MBIM_MIN_TX_TIMEOUT_NSECS;
	static int cnt;
#if DEBUG
	pr_info("usb: %s: +++\n", __func__);
#endif
	if (dev->EnabledDevice == false) {
		pr_err("usb: %s: did not enabled device (%d)\n", __func__, __LINE__);

		goto done;
	}
#ifdef CONFIG_USB_SS_REMOTE_WAKEUP
	if (!mbim_resume_cmpl) {
		if (cnt < 1)
			pr_info("%s: system resume not complete yet (%d)\n", __func__, __LINE__);
		cnt++;
		goto err;
	}
	if (cnt)
		pr_info("%s: system resume send data (%d) cnt = %d\n", __func__, __LINE__, cnt);
	cnt = 0;
#endif

	spin_lock_irqsave(&dev->req_lock, flags);
	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->req_lock, flags);

#if DEBUG
		pr_err("usb: %s: tx requeset queue empty\n", __func__);
#endif
		goto err;
	}
	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);
	spin_unlock_irqrestore(&dev->req_lock, flags);

	tx_timeout = dev->occurred_timeout ? MBIM_MIN_TX_TIMEOUT_NSECS : MBIM_MAX_TX_TIMEOUT_NSECS;
	dev->occurred_timeout = 0;
	hrtimer_start(&dev->tx_timer, ktime_set(0, tx_timeout), HRTIMER_MODE_REL);

	spin_lock_irqsave(&dev->lock, flags);
	/* Check if there is any available write buffers */
	if (likely(mbim_queue_empty())) {
		/* Turn interrupts back on before sleeping. */
		spin_unlock_irqrestore(&dev->lock, flags);

		/* Sleep until a write buffer is available */
		wait_event_interruptible(dev->queue->dl_wait,
			(likely(!mbim_queue_empty()) || !dev->EnabledDevice));

		spin_lock_irqsave(&dev->lock, flags);
	}
	if (dev->EnabledDevice == false) {
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_err("usb: %s: did not enabled device (%d)\n", __func__, __LINE__);

		spin_lock_irqsave(&dev->req_lock, flags);
		list_add_tail(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
		return;
	}

	skb = mbim_dequeue_tail();
	spin_unlock_irqrestore(&dev->lock, flags);

#if DEBUG
	pr_info("usb: mbim!!! %s: %pK, size = %d, req=%pK \n", __func__, skb, skb->len, req);
#endif
	if (dev->wrap && dev->EnabledDevice) {
		spin_lock_irqsave(&dev->lock, flags);
		skb = mnet_wrap_ntb(dev, skb, req);
		spin_unlock_irqrestore(&dev->lock, flags);
		if (!skb) {
			spin_lock_irqsave(&dev->req_lock, flags);
			list_add_tail(&req->list, &dev->tx_reqs);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			dev->ndev->stats.tx_dropped++;
			goto err;
		}

		dev->tx_skb_hold_count = 0;
	}

	do {
#if DEBUG
		pr_info("usb: %s: start gather skb packet\n", __func__);
#endif

		if (skb == NULL) {
			if (likely(mbim_queue_empty())) {				
				/* Sleep until a write buffer is available */
				wait_event_interruptible(dev->queue->dl_wait,
					(likely(!mbim_queue_empty()) || dev->occurred_timeout
					|| !dev->EnabledDevice));
			}
			if (dev->EnabledDevice == false) {
				spin_unlock_irqrestore(&dev->lock, flags);
				pr_err("usb: %s: did not enabled device .. so exit (%d)\n", __func__, __LINE__);
				spin_lock_irqsave(&dev->req_lock, flags);
				list_add_tail(&req->list, &dev->tx_reqs);
				spin_unlock_irqrestore(&dev->req_lock, flags);
				hrtimer_cancel(&dev->tx_timer);
				return;
			} else if (dev->occurred_timeout) {
				break;
			}

			skb = mbim_dequeue_tail();
			dev->func_flag = NCM_ADD_DGRAM;

			/* Copy received IP data from SKB */
			memcpy(req->buf + req->length + added_offset, skb->data, skb->len);
			/* Increment req length by skb data length */
			req->length = req->length + skb->len + added_offset;
			req->context = NULL;
		}
#if DEBUG
		pr_info("usb: %s: req: %pK, req->len: %d, skb: %pK\n", __func__, req, req->length, skb);
#endif
		spin_lock_irqsave(&dev->req_lock, flags);

		if (dev->func_flag == NCM_ADD_DGRAM) {
			mbim_add_datagram((__le16 *)(req->buf), skb->len, dev->tx_skb_hold_count);
		} else {
			req->length = get_current_length((__le16 *)(req->buf));
		}

		dev_kfree_skb_any(skb);
		skb = NULL;
		dev->tx_skb_hold_count++;
		spin_unlock_irqrestore(&dev->req_lock, flags);

#if DEBUG
		pr_info("usb: %s: complete gather one skb packet [%d]\n", __func__, dev->tx_skb_hold_count);
#endif
	} while(dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer && !dev->occurred_timeout);

	hrtimer_cancel(&dev->tx_timer);
	req->complete = tx_complete;
	/* Check if we need to send a zero length packet. */
	if (req->length > USB_DL_BUFSIZE)
		/* They will be more TX requests so no yet. */
		req->zero = 0;
	else
		/* If the data amount is not a multiple of the
		 * maxpacket size then send a zero length packet.
		 */
		req->zero = ((req->length % dev->in_ep->maxpacket) == 0);

	/* We've disconnected or reset so free the req and buffer */
	if (dev->reset_mbim) {
		spin_lock_irqsave(&dev->req_lock, flags);
		list_add_tail(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
		goto err;
	}

	spin_lock_irqsave(&dev->req_lock, flags);
	list_add(&req->list, &dev->tx_reqs_active);
	spin_unlock_irqrestore(&dev->req_lock, flags);

	ret = usb_ep_queue(dev->in_ep, req, GFP_ATOMIC);
	if (ret) {
		pr_err("usb: %s: ep_queue fail (%d)\n", __func__, ret);
		spin_lock_irqsave(&dev->req_lock, flags);
		list_del(&req->list);
		list_add_tail(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
		if (dev->EnabledDevice == false) {
			pr_err("usb: %s: did not enabled device .. so exit (%d)\n", __func__, __LINE__);
			goto done;
		}
		goto err;
	}

	dev->func_flag = NCM_ADD_HEADER;

err:
	queue_work_on(6, dev->mbim_dl, &dev->start_xmit);
done:
#if DEBUG
	pr_info("usb: %s: ---\n", __func__);
#endif
	return;
}

static netdev_tx_t mnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct mbim_port *port = netdev_priv(ndev);
	struct mbim_dev	*dev = port->mbim;

	pr_info("usb: %s: +++\n", __func__);

	if (dev->EnabledDevice == false)
		return -EINVAL;

	return NETDEV_TX_OK;
}
static int mbim_open(struct inode *inode, struct file *fd)
{
	struct mbim_dev	*dev = _mbim_dev;
	unsigned long		flags;
	int			ret = -EBUSY;

	pr_info("usb: %s: +++\n", __func__);

	if (!dev) {
		pr_err("usb: %s: mbim_dev is NULL!\n", __func__);
		return -ENODEV;
	}

	fd->private_data = _mbim_dev;

	spin_lock_irqsave(&dev->lock, flags);

	if (!dev->mbim_cdev_open)
	{
		dev->mbim_cdev_open++;
	}
	else
		dev->mbim_cdev_open++;

	ret = 0;
	spin_unlock_irqrestore(&dev->lock, flags);

	pr_info("usb: %s: ---\n", __func__);
	return ret;
}

static int
mbim_close(struct inode *inode, struct file *fd)
{
	struct mbim_dev	*dev = fd->private_data;
	unsigned long		flags;

	pr_info("usb: %s: +++\n", __func__);
	spin_lock_irqsave(&dev->lock, flags);
	dev->mbim_cdev_open--;
	fd->private_data = NULL;

	//dev->mbim_status &= ~SELECTED;

	spin_unlock_irqrestore(&dev->lock, flags);
	pr_info("usb: %s: ---\n", __func__);

	return 0;
}

/* This function must be called with interrupts turned off. */
static void
setup_rx_reqs(struct mbim_dev *dev)
{
	struct usb_request              *req;
	unsigned long	flags;

#if DEBUG
	pr_info("usb: %s: +++\n", __func__);
#endif
	spin_lock_irqsave(&dev->lock, flags);
	while (likely(!list_empty(&dev->rx_reqs))) {
		int error;

		if (!dev->EnabledDevice ) {
			pr_err("usb: %s mbim disabled (%d)\n",__func__, dev->EnabledDevice);
			break;
		}
		req = container_of(dev->rx_reqs.next,
			struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		/* The USB Host sends us whatever amount of data it wants to
		 * so we always set the length field to the full USB_BUFSIZE.
		 * If the amount of data is more than the read() caller asked
		 * for it will be stored in the request buffer until it is
		 * asked for by read().
		 */
		req->length = USB_BUFSIZE;
		req->complete = rx_complete;

		/* here, we unlock, and only unlock, to avoid deadlock. */
		//spin_unlock(&dev->lock);

#if DEBUG
		pr_info("%s: usb_ep_queue: [%p, %p]\n", __func__, req->context, req);
#endif
		error = usb_ep_queue(dev->out_ep, req, GFP_ATOMIC);
		//spin_lock(&dev->lock);
		if (error) {
			pr_info("usb: %s: rx submit --> %d\n", __func__, error);
			spin_lock_irqsave(&dev->lock, flags);
			list_add_tail(&req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		spin_lock_irqsave(&dev->lock, flags);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
#if DEBUG
	pr_info("usb: %s: ---\n", __func__);
#endif
}

static ssize_t
mbim_read(struct file *fd, char __user *buf, size_t len, loff_t *ptr)
{
	struct mbim_dev		*dev = fd->private_data;
	unsigned long			flags;
	size_t				size;
	size_t				bytes_copied;
	struct usb_request		*req;
	/* This is a pointer to the current USB rx request. */
	struct usb_request		*current_rx_req;
	/* This is the number of bytes in the current rx buffer. */
	size_t				current_rx_bytes;
	/* This is a pointer to the current rx buffer. */
	u8				*current_rx_buf;

	if (len == 0)
		return -EINVAL;

	if(dev->EnabledDevice == false)
		return -EINVAL;

	mutex_lock(&dev->lock_mbim_io);
	spin_lock_irqsave(&dev->lock, flags);

	dev->reset_mbim = 0;

	bytes_copied = 0;
	current_rx_req = dev->current_rx_req;
	current_rx_bytes = dev->current_rx_bytes;
	current_rx_buf = dev->current_rx_buf;
	dev->current_rx_req = NULL;
	dev->current_rx_bytes = 0;
	dev->current_rx_buf = NULL;

	/* Check if there is any data in the read buffers. Please note that
	 * current_rx_bytes is the number of bytes in the current rx buffer.
	 * If it is zero then check if there are any other rx_buffers that
	 * are on the completed list. We are only out of data if all rx
	 * buffers are empty.
	 */
	if ((current_rx_bytes == 0) &&
		(likely(list_empty(&dev->rx_buffers)))) {
		/* Turn interrupts back on before sleeping. */
		spin_unlock_irqrestore(&dev->lock, flags);

		/*
		 * If no data is available check if this is a NON-Blocking
		 * call or not.
		 */
		if (fd->f_flags & (O_NONBLOCK | O_NDELAY)) {
			mutex_unlock(&dev->lock_mbim_io);
			return -EAGAIN;
		}

		/* Sleep until data is available */
		wait_event_interruptible(dev->rx_wait,
			(likely(!list_empty(&dev->rx_buffers))));
		spin_lock_irqsave(&dev->lock, flags);
	}

	/* We have data to return then copy it to the caller's buffer.*/
	while ((current_rx_bytes || likely(!list_empty(&dev->rx_buffers)))
		&& len) {
		if (current_rx_bytes == 0) {
			req = container_of(dev->rx_buffers.next,
				struct usb_request, list);
//			list_del_init(&req->list);

			if (req->actual && req->buf) {
				current_rx_req = req;
				current_rx_bytes = req->actual;
				current_rx_buf = req->buf;
			}
			else {
//				list_add(&req->list, &dev->rx_reqs);
				continue;
			}
		}

		/* Don't leave irqs off while doing memory copies */
		spin_unlock_irqrestore(&dev->lock, flags);

		if (len > current_rx_bytes)
			size = current_rx_bytes;
		else
			size = len;

		size -= copy_to_user(buf, current_rx_buf, size);
		bytes_copied += size;
		len -= size;
		buf += size;

		spin_lock_irqsave(&dev->lock, flags);

		/* We've disconnected or reset so return. */
		if (dev->reset_mbim) {
//			list_add(&current_rx_req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->lock, flags);
			mutex_unlock(&dev->lock_mbim_io);
			return -EAGAIN;
		}

		/* If we not returning all the data left in this RX request
		 * buffer then adjust the amount of data left in the buffer.
		 * Othewise if we are done with this RX request buffer then
		 * requeue it to get any incoming data from the USB host.
		 */
		if (size < current_rx_bytes) {
			current_rx_bytes -= size;
			current_rx_buf += size;
		}
		else {
//			list_add(&current_rx_req->list, &dev->rx_reqs);
			current_rx_bytes = 0;
			current_rx_buf = NULL;
			current_rx_req = NULL;
		}
	}

	dev->current_rx_req = current_rx_req;
	dev->current_rx_bytes = current_rx_bytes;
	dev->current_rx_buf = current_rx_buf;

	spin_unlock_irqrestore(&dev->lock, flags);
	mutex_unlock(&dev->lock_mbim_io);

	if (bytes_copied)
	{
		//pr_info("############################################### exit read() = %d\n", bytes_copied);
		return bytes_copied;
	}
	else
	{
		pr_info("usb: ############################################### exit read() error\n");
		return -EAGAIN;
	}
}

static ssize_t
mbim_write(struct file *fd, const char __user *buf, size_t len, loff_t *ptr)
{
	struct mbim_dev	*dev = fd->private_data;
	unsigned long		flags;
	size_t			size;	/* Amount of data in a TX request. */
	size_t			bytes_copied = 0;
	struct usb_request	*req;

	if (dev->EnabledDevice == false)
		return -EINVAL;

	if (len == 0)
		return -EINVAL;

	mutex_lock(&dev->lock_mbim_io);
	spin_lock_irqsave(&dev->lock, flags);

	dev->reset_mbim = 0;

	/* Check if there is any available write buffers */
	if (likely(list_empty(&dev->tx_reqs))) {
		/* Turn interrupts back on before sleeping. */
		spin_unlock_irqrestore(&dev->lock, flags);

		/*
		 * If write buffers are available check if this is
		 * a NON-Blocking call or not.
		 */
		if (fd->f_flags & (O_NONBLOCK | O_NDELAY)) {
			mutex_unlock(&dev->lock_mbim_io);
			return -EAGAIN;
		}

		/* Sleep until a write buffer is available */
		wait_event_interruptible(dev->tx_wait,
			(likely(!list_empty(&dev->tx_reqs))));
		spin_lock_irqsave(&dev->lock, flags);
	}

	if (!dev->mbim_online) {
		/* usb disconnect */
		spin_unlock_irqrestore(&dev->lock, flags);
		mutex_unlock(&dev->lock_mbim_io);
		return -EPIPE;
	}

	while (likely(!list_empty(&dev->tx_reqs)) && len) {

		if (len > USB_BUFSIZE)
			size = USB_BUFSIZE;
		else
			size = len;

		req = container_of(dev->tx_reqs.next, struct usb_request,
			list);
		list_del_init(&req->list);

		req->complete = tx_complete;
		req->length = size;

		/* Check if we need to send a zero length packet. */
		if (len > size)
			/* They will be more TX requests so no yet. */
			req->zero = 0;
		else
			/* If the data amount is not a multiple of the
			 * maxpacket size then send a zero length packet.
			 */
			req->zero = ((len % dev->in_ep->maxpacket) == 0);

		/* Don't leave irqs off while doing memory copies */
		spin_unlock_irqrestore(&dev->lock, flags);

		if (copy_from_user(req->buf, buf, size)) {
			list_add_tail(&req->list, &dev->tx_reqs);
			mutex_unlock(&dev->lock_mbim_io);
			return bytes_copied;
		}

		bytes_copied += size;
		len -= size;
		buf += size;

		spin_lock_irqsave(&dev->lock, flags);

		/* We've disconnected or reset so free the req and buffer */
		if (dev->reset_mbim) {
			list_add_tail(&req->list, &dev->tx_reqs);
			spin_unlock_irqrestore(&dev->lock, flags);
			mutex_unlock(&dev->lock_mbim_io);
			return -EAGAIN;
		}

		if (usb_ep_queue(dev->in_ep, req, GFP_ATOMIC)) {
			list_add_tail(&req->list, &dev->tx_reqs);
			spin_unlock_irqrestore(&dev->lock, flags);
			mutex_unlock(&dev->lock_mbim_io);
			return -EAGAIN;
		}

		list_add_tail(&req->list, &dev->tx_reqs_active);

	}

	spin_unlock_irqrestore(&dev->lock, flags);
	mutex_unlock(&dev->lock_mbim_io);

	if (bytes_copied)
		return bytes_copied;
	else
		return -EAGAIN;
}

static int get_usbmode_info (struct mbim_dev *dev, unsigned long arg)
{
	struct usb_function *f;
	struct usb_composite_dev *cdev = dev->mbim_cdev;
	struct usb_configuration *configuration;
	char usbmode[100] = {0,};
	int length;

	if (!dev->mbim_ready) {
		printk("usb: %s dev->mbim_ready is false\n",__func__);
		return -1;
	}

	/* possible query menu */
	/* MBIM only			*/
	/* MTP+MBIM			*/
	/* MBIM+DM			*/
	/* gnss and acm is skipped from the return value */
	list_for_each_entry(configuration, &cdev->configs, list) {
		list_for_each_entry(f, &configuration->functions, list) {
			if (f != NULL) {
				if (!strcmp(f->name, "acm") ||	!strcmp(f->name, "gnss")) {
					printk("usb: %s : %s is skip \n",__func__, f->name);
				} else {
					if (strlen(usbmode))
						strcat(usbmode,",");
					strcat(usbmode, f->name);
				}
			}
		}
	}
	length = strlen(usbmode);
	printk("usb: %s : usb_mode = %s : length = %d \n",__func__, usbmode, length);

	if (copy_to_user((IOCTL_MBIM_GETCOMMAND_PARAM *)arg, usbmode, length)) {
		return -1;
	}

	return length;
}

static long
mbim_ioctl(struct file *fd, unsigned int code, unsigned long arg)
{
	struct mbim_dev	*dev = fd->private_data;
	unsigned long		flags;
	int			status = 0;
	struct ctrl_pkt *cpkt = NULL;
	int ret = 0;
	IOCTL_MBIM_CONNECT_INFO_PARAM ConnectInfo;
	//IOCTL_MBIM_SETRESPONSE_PARAM UnsolicitedEventInfo;
	struct MBIM_MESSAGE_HEADER mbim_message;

	/* handle ioctls */
	pr_info("usb: mbim_ioctl ++\n");

	//spin_lock_irqsave(&dev->lock, flags);

	/* wait device connected */
	if (!dev->EnabledDevice) {
		ret = wait_event_interruptible(dev->sync_wq,
			dev->EnabledDevice);

		if (ret < 0 ) {
			pr_err("usb: %s Waiting failed (%d)\n",__func__, __LINE__);
			return -1;
		}
	}

	switch (code) {
		case IOCTL_MBIM_CONNECT_INFO:
			pr_info("usb: [IOCTL_MBIM_CONNECT_INFO]\n");
			ret = copy_from_user(&ConnectInfo, (IOCTL_MBIM_CONNECT_INFO_PARAM *)arg, sizeof(IOCTL_MBIM_CONNECT_INFO_PARAM));
			if (ret) {
				pr_info("usb: copy_from_user failed err:%d\n", ret);
				return ret;
			}
			if (ConnectInfo.IsConnected)
			{
				mbim_do_notify_connect(dev);
			}
			else
			{
				mbim_do_notify_disconnect(dev);
			}
			//dev->notify_state = NCM_NOTIFY_CONNECT;
			//mbim_do_notify(dev);

			break;
		case IOCTL_MBIM_GETCOMMAND:
			//pr_info("[IOCTL_MBIM_GETCOMMAND]\n");

			if (mbim_lock(&dev->read_excl)) {
				pr_info("usb: Previous reading is not finished yet\n");
				mdelay(500);
				return -1;
			}

			while (list_empty(&dev->cpkt_req_q)) {
				//pr_info("Requests list is empty. Wait.\n");
				ret = wait_event_interruptible(dev->read_wq,
					(!list_empty(&dev->cpkt_req_q) || !dev->EnabledDevice));
				if (ret < 0) {
					pr_info("usb: Waiting failed : ret = %d (%d)\n", ret, __LINE__);
					mbim_unlock(&dev->read_excl);
					return ret;
				}
				if (dev->EnabledDevice == false) {
					mbim_unlock(&dev->read_excl);
					pr_err("usb: %s: did not enabled device (%d)\n", __func__, __LINE__);
					return -1;
				}
			}
			spin_lock_irqsave(&dev->notify_req_lock, flags);
			cpkt = list_first_entry(&dev->cpkt_req_q, struct ctrl_pkt,
				list);

			//pr_info("Received request packet\n");

			if (cpkt->len > MBIM_BULK_BUFFER_SIZE) {
				spin_unlock_irqrestore(&dev->notify_req_lock, flags);
				mbim_unlock(&dev->read_excl);
				pr_info("usb: cpkt size too big:%d > buf size:%d\n",
					cpkt->len, MBIM_BULK_BUFFER_SIZE);
				return -1;
			}

			//pr_info("cpkt size:%d\n", cpkt->len);

			list_del(&cpkt->list);
			spin_unlock_irqrestore(&dev->notify_req_lock, flags);
			mbim_unlock(&dev->read_excl);

			mbim_message.MessageType = *((unsigned int *)cpkt->buf);
			mbim_message.MessageLength = *((unsigned int *)cpkt->buf+1);
			mbim_message.TransactionId =  *((unsigned int *)cpkt->buf+2);
			pr_info("usb: %s: cpkt->buf: %s (cpkt->len : %d) (TYPE=0x%x /ID:%d)\n", __func__,
				cpkt->buf, cpkt->len, mbim_message.MessageType, mbim_message.TransactionId);
			if (copy_to_user((IOCTL_MBIM_GETCOMMAND_PARAM *)arg, cpkt->buf, cpkt->len))
			{
				mbim_free_ctrl_pkt(cpkt);
				return -1;
			}

			status = cpkt->len;
			mbim_free_ctrl_pkt(cpkt);
			break;

		case IOCTL_MBIM_INFO_USBMODE:
			pr_info("usb: [IOCTL_MBIM_INFO_USBMODE]\n");
			status = get_usbmode_info(dev, arg);
			break;

		case IOCTL_MBIM_SETRESPONSE:
			pr_info("usb: [IOCTL_MBIM_SETRESPONSE]\n");

			if (mbim_lock(&dev->write_excl)) {
				pr_info("usb: Previous writing not finished yet\n");
				return -EBUSY;
			}

			cpkt = mbim_alloc_ctrl_pkt(MBIM_BULK_BUFFER_SIZE, GFP_KERNEL);
			if (IS_ERR(cpkt)) {
				pr_err("usb: failed to allocate ctrl pkt\n");
				mbim_unlock(&dev->write_excl);
				return -ENOMEM;
			}

			ret = copy_from_user(cpkt->buf, (IOCTL_MBIM_SETRESPONSE_PARAM *)arg, MBIM_BULK_BUFFER_SIZE);
			if (ret) {
				pr_err("usb: copy_from_user failed err:%d\n", ret);
				mbim_free_ctrl_pkt(cpkt);
				mbim_unlock(&dev->write_excl);
				return -1;
			}
			mbim_message.MessageType = *((unsigned int *)cpkt->buf);
			mbim_message.MessageLength = *((unsigned int *)cpkt->buf+1);
			mbim_message.TransactionId =  *((unsigned int *)cpkt->buf+2);
			/* prevent kernel panic / slub : Poison overwritten*/
			pr_info("usb: [IOCTL_MBIM_SETRESPONSE] : (TYPE=0x%x/ID:%d, len=%d)\n",
				mbim_message.MessageType, mbim_message.TransactionId, mbim_message.MessageLength);

			cpkt->len = MBIM_BULK_BUFFER_SIZE;
			status = cpkt->len;
			fmbim_send_cpkt_response(dev, cpkt);
			mbim_unlock(&dev->write_excl);
			break;

	default:
		/* could not handle ioctl */
		status = -ENOTTY;
	}

	//spin_unlock_irqrestore(&dev->lock, flags);

	return status;
}

/* used after endpoint configuration */
static const struct file_operations mbim_fops = {
	.owner = THIS_MODULE,
	.open = mbim_open,
	.read = mbim_read,
	.write = mbim_write,
//	.fsync = mbim_fsync,
//	.poll = mbim_poll,
	.unlocked_ioctl = mbim_ioctl,
	.release = mbim_close,
};

static struct miscdevice mbim_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_mbim",
	.fops = &mbim_fops,
};

static const struct net_device_ops mnet_device = {
	.ndo_open = mnet_open,
	.ndo_stop = mnet_close,
	.ndo_start_xmit = mnet_xmit,
};

void mnet_setup(struct net_device *ndev)
{
	ndev->netdev_ops = &mnet_device;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->addr_len = 0;
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}
/*-------------------------------------------------------------------------*/
static void mbim_reset_interface(struct mbim_dev *dev)
{
	if (dev->interface < 0)
		return;

	pr_info("usb: %s: +++\n", __func__);

	if (dev->in_ep->desc)
		usb_ep_disable(dev->in_ep);

	if (dev->out_ep->desc)
		usb_ep_disable(dev->out_ep);

	if (dev->notify_ep->desc)
		usb_ep_disable(dev->notify_ep);

	dev->in_ep->desc = NULL;
	dev->out_ep->desc = NULL;
	dev->notify_ep->desc = NULL;
	dev->interface = -1;
	/* connection gone */
	atomic_set(&dev->notify_count, 0);
	pr_info("usb: %s: dev->notify_count=%d ---\n", __func__, atomic_read(&dev->notify_count));
}

/*-------------------------------------------------------------------------*/

static bool gmbim_req_match(struct usb_function *f,
	const struct usb_ctrlrequest *ctrl,
	bool config0)
{
	struct mbim_dev	*dev = func_to_mbim(f);
	u16			w_index = le16_to_cpu(ctrl->wIndex);

	//pr_info("gmbim_req_match++\n");
	//pr_info("ctrl reqtype:0x%02x, req:0x%02x, value:0x%04x, index:0x%04x, length:0x%04x\n",
	//	ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	if (config0)
		return false;

	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE ||
		(ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS)
		return false;

	return w_index == dev->interface;
}

struct mbim_ntb_input_size {
	u32	ntb_input_size;
	u16	ntb_max_datagrams;
	u16	reserved;
};

static void mbim_set_ntb_input_size_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* now for SET_NTB_INPUT_SIZE only */
	unsigned		in_size = 0;
	struct usb_function	*f = req->context;
	struct mbim_dev *dev = func_to_mbim(f);
	struct mbim_ntb_input_size *ntb = NULL;

	//pr_info("mbim_set_ntb_input_size_complete++\n");

	req->context = NULL;
	if (req->status || req->actual != req->length) {
		pr_info("usb: Bad control-OUT transfer\n");
		goto invalid;
	}

	if (req->length == 4) {
		in_size = get_unaligned_le32(req->buf);
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
			in_size > le32_to_cpu(ntb_parameters.dwNtbInMaxSize)) {
			pr_info("usb: Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
	}
	else if (req->length == 8) {
		ntb = (struct mbim_ntb_input_size *)req->buf;
		in_size = get_unaligned_le32(&(ntb->ntb_input_size));
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
			in_size > le32_to_cpu(ntb_parameters.dwNtbInMaxSize)) {
			pr_info("usb: Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
		dev->ntb_max_datagrams =
			get_unaligned_le16(&(ntb->ntb_max_datagrams));
	}
	else {
		pr_info("usb: Illegal NTB length %d\n", in_size);
		goto invalid;
	}

	//pr_info("Set NTB INPUT SIZE %d\n", in_size);

	dev->ntb_input_size = in_size;
	return;

invalid:
	usb_ep_set_halt(ep);

	return;
}

static void mbim_send_encapsulated_command_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* now for SET_NTB_INPUT_SIZE only */
	//unsigned		in_size = 0;
	struct usb_function	*f = req->context;
	struct mbim_dev *dev = func_to_mbim(f);
	struct ctrl_pkt		*cpkt = NULL;
	int			len = req->actual;
	unsigned long		flags;

	//pr_info("mbim_send_encapsulated_command_complete++\n");

	req->context = NULL;
	if (req->status || req->actual != req->length) {
		pr_info("usb: Bad control-OUT transfer\n");
		goto invalid;
	}

	cpkt = mbim_alloc_ctrl_pkt(len, GFP_ATOMIC);
	if (IS_ERR(cpkt)) {
		pr_info("usb: Unable to allocate ctrl pkt\n");
		return;
	}

	//pr_info("Add to cpkt_req_q packet with len = %d\n", len);
	memcpy(cpkt->buf, req->buf, len);

	spin_lock_irqsave(&dev->notify_req_lock, flags);

	if (!dev->mbim_cdev_open) {
		pr_info("usb: mbim file handler %pK is not open (%d)", dev, dev->mbim_cdev_open);
		spin_unlock_irqrestore(&dev->notify_req_lock, flags);
		mbim_free_ctrl_pkt(cpkt);
		return;
	}

	list_add_tail(&cpkt->list, &dev->cpkt_req_q);
	spin_unlock_irqrestore(&dev->notify_req_lock, flags);

	/* wakeup read thread */
	//pr_info("Wake up read queue");
	wake_up(&dev->read_wq);

	return;

invalid:
	usb_ep_set_halt(ep);

	return;
}

#if 0
#define USB_CDC_SEND_ENCAPSULATED_COMMAND	0x00
#define USB_CDC_GET_ENCAPSULATED_RESPONSE	0x01
#define USB_CDC_GET_NTB_PARAMETERS		0x80
#define USB_CDC_GET_NTB_FORMAT			0x83
#define USB_CDC_SET_NTB_FORMAT			0x84
#define USB_CDC_GET_NTB_INPUT_SIZE		0x85
#define USB_CDC_SET_NTB_INPUT_SIZE		0x86
#define USB_CDC_GET_MAX_DATAGRAM_SIZE	0x87
#define USB_CDC_SET_MAX_DATAGRAM_SIZE	0x88
#define USB_CDC_RESET		0x05
#endif
/*
 * The setup() callback implements all the ep0 functionality that's not
 * handled lower down.
 */
static int mbim_func_setup(struct usb_function *f,
	const struct usb_ctrlrequest *ctrl)
{
	struct MBIM_MESSAGE_HEADER	* pResp = NULL;
	struct mbim_dev *dev = func_to_mbim(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
//	u8			*buf = req->buf;
	int			value = -EOPNOTSUPP;
	struct ctrl_pkt		*cpkt = NULL;
	u16			wIndex = le16_to_cpu(ctrl->wIndex);
	u16			wValue = le16_to_cpu(ctrl->wValue);
	u16			wLength = le16_to_cpu(ctrl->wLength);

	struct MBIM_MESSAGE_HEADER mbim_message;

	//pr_info("mbim_func_setup++\n");
	//pr_info("ctrl reqtype:0x%02x, req:0x%02x, value:0x%04x, index:0x%04x, length:0x%04x\n",
	//	ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	switch (ctrl->bRequestType&USB_TYPE_MASK) {
	case USB_TYPE_CLASS:

		if (dev->mbim_cdev_open == 0)
		{
			goto unknown;
		}

		switch (ctrl->bRequest) {
		case USB_CDC_GET_NTB_PARAMETERS:
			//pr_info("[USB_CDC_GET_NTB_PARAMETERS]\n");
			value = wLength > sizeof ntb_parameters ?
				sizeof ntb_parameters : wLength;
			memcpy(req->buf, &ntb_parameters, value);
			//pr_info("copy ntb_parameters\n");
			break;

		case USB_CDC_GET_NTB_INPUT_SIZE:
			//pr_info("[USB_CDC_GET_NTB_INPUT_SIZE]\n");
			goto unknown;

		case USB_CDC_SET_NTB_INPUT_SIZE:
			//pr_info("[USB_CDC_SET_NTB_INPUT_SIZE]\n");
			req->complete = mbim_set_ntb_input_size_complete;
			req->length = wLength;
			req->context = f;
			value = req->length;
			break;

		case USB_CDC_SEND_ENCAPSULATED_COMMAND:
			//pr_info("[USB_CDC_SEND_ENCAPSULATED_COMMAND]\n");
			req->complete = mbim_send_encapsulated_command_complete;
			req->length = wLength;
			req->context = f;
			value = req->length;
			break;

		case USB_CDC_GET_ENCAPSULATED_RESPONSE:

			spin_lock(&dev->notify_req_lock);
			if (list_empty(&dev->cpkt_resp_q)) {
				pr_info("usb: ctrl resp queue empty\n");
				spin_unlock(&dev->notify_req_lock);
				break;
			}

			cpkt = list_first_entry(&dev->cpkt_resp_q,
				struct ctrl_pkt, list);
			list_del(&cpkt->list);
			spin_unlock(&dev->notify_req_lock);

			pResp = (struct MBIM_MESSAGE_HEADER	* )cpkt->buf;

			value = min_t(unsigned, wLength, pResp->MessageLength);
			memcpy(req->buf, cpkt->buf, value);

			mbim_message.MessageType = *((unsigned int *)cpkt->buf);
			mbim_message.MessageLength = *((unsigned int *)cpkt->buf+1);
			mbim_message.TransactionId =  *((unsigned int *)cpkt->buf+2);

			if (mbim_message.TransactionId)
				dev->cnt_notify_int++;

			pr_info("usb: [USB_CDC_GET_ENCAPSULATED_RESPONSE] :TYPE=0x%x/ID:%d/len=%d, cnt_notify_int=%d\n",
				mbim_message.MessageType, mbim_message.TransactionId,
				mbim_message.MessageLength, dev->cnt_notify_int);

			mbim_free_ctrl_pkt(cpkt);
			//pr_info("processed, len = %d\n", value);

			break;

		case USB_CDC_RESET:
			//pr_info("[USB_CDC_RESET]\n");
			value = 0;
			break;


		default:
			goto unknown;
		}
		break;

	default:
	unknown:
		VDBG(dev,
			"usb: unknown ctrl req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			wValue, wIndex, wLength);
		break;
	}
	/* host either stalls (value < 0) or reports success */
	if (value >= 0) {
		req->length = value;
		req->zero = value < wLength;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			ERROR(dev, "usb: %s:%d Error!\n", __func__, __LINE__);
			req->status = 0;
		}
	}
	return value;
}

#ifdef CONFIG_USB_SS_REMOTE_WAKEUP
void mbim_suspend(void)
{
	pr_info("%s : mbim_resume_cmpl clear\n", __func__);
	mbim_resume_cmpl = 0;
}

void mbim_resume(void)
{
	pr_info("%s : mbim_resule_cmpl set\n", __func__);
	mbim_resume_cmpl = 1;
}
#endif

static void mbim_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct mbim_dev *dev = req->context;
	unsigned long flags;
	//struct usb_cdc_notification	*event = req->buf;

	switch (req->status) {
	case 0:
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		atomic_set(&dev->notify_count, 0);
		pr_info("usb: %s: req_shutdown (%pK) \n",__func__, req);
		break;
	default:
		break;
	}
//	mbim_do_notify(dev);
	spin_lock_irqsave(&dev->notify_req_lock, flags);
	list_add_tail(&req->list, &dev->tx_notifyreqs);
	spin_unlock_irqrestore(&dev->notify_req_lock, flags);

	if (req->status == 0 ) {
		if (atomic_read(&dev->notify_count) > 0) {
			printk("usb: %s : pending response .. try signal to pc (%d) \n",
							__func__, atomic_read(&dev->notify_count));
			atomic_dec(&dev->notify_count);
			mbim_do_notify_response_avaliable(dev);
		}
	}

}

static inline void mnet_reset_values(struct mbim_dev *dev)
{
	struct mnet_data_info	*m_info = dev->mnet_info;

	m_info->is_crc = false;
	m_info->nth_wndpindex = 0xc;

	m_info->parser_opts = &ndp16_opts;
	m_info->port.cdc_filter = DEFAULT_FILTER;
	/* doesn't make sense for ncm, fixed size used */
	m_info->port.header_len = 0;
	m_info->port.fixed_out_len = le32_to_cpu(ntb_parameters.dwNtbOutMaxSize);
	m_info->port.fixed_in_len = NTB_DEFAULT_IN_SIZE;
}

static int mbim_func_bind(struct usb_configuration *c,
	struct usb_function *f)
{
	struct usb_gadget *gadget = c->cdev->gadget;
#ifdef CONFIG_USB_SS_REMOTE_WAKEUP
	struct dwc3 *dwc = container_of(gadget, struct dwc3, gadget);
#endif
	struct mbim_dev *dev = func_to_mbim(f);
	struct usb_composite_dev *cdev = c->cdev;
	struct usb_ep *notify_ep = NULL;
	struct usb_ep *in_ep = NULL;
	struct usb_ep *out_ep = NULL;
	struct usb_request *req;
	struct mbim_port *mnet_port;
//	struct sk_buff *skb;
	int id;
	int ret;
	u32 i;
	int status = 0;

	pr_info("usb: mbim_func_bind++\n");

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;

	dev->ctrl_id = id;
	mbim_iad_desc.bFirstInterface = id;
	mbim_control_intf.bInterfaceNumber = id;
	mbim_union_desc.bMasterInterface0 = id;

	/* maybe allocate device-global string IDs */
	if (mbim_string_defs[0].id == 0) {
		/* control interface label */
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		mbim_string_defs[F_MBIM_IDX].id = status;
		mbim_control_intf.iInterface = status;
	}

#ifdef CONFIG_USB_SS_REMOTE_WAKEUP
/* FUNCTION_WAKE Interface field: This field identifies the first interface         */
/* in the function that caused the device to perform a remote wake operation. */
		dwc->interface_number = id;
		pr_info("%s: interface_number:%d\n", __func__, dwc->interface_number);
#endif

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->data_id = id;

	mbim_data_nop_intf.bInterfaceNumber = id;
	mbim_data_intf.bInterfaceNumber = id;
	mbim_union_desc.bSlaveInterface0 = id;

	/* finish hookup to lower layer ... */
	dev->gadget = gadget;

	/* all we really need is bulk IN/OUT */
	in_ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_in_desc);
	if (!in_ep)
	{
		pr_info("usb: usb_ep_autoconfig error, in_ep\n");
		return -ENODEV;
	}

	out_ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_out_desc);
	if (!out_ep)
	{
		pr_info("usb: usb_ep_autoconfig error, out_ep\n");
		return -ENODEV;
	}

	notify_ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_notify_desc);
	if (!notify_ep)
	{
		pr_info("usb: usb_ep_autoconfig error, notify_ep\n");
		return -ENODEV;
	}

	dev->in_ep = in_ep;
	dev->out_ep = out_ep;
	dev->notify_ep = notify_ep;

	dev->EnabledDevice = false;

	hs_mbim_in_desc.bEndpointAddress = fs_mbim_in_desc.bEndpointAddress;
	hs_mbim_out_desc.bEndpointAddress = fs_mbim_out_desc.bEndpointAddress;
	hs_mbim_notify_desc.bEndpointAddress =
		fs_mbim_notify_desc.bEndpointAddress;

	ss_mbim_in_desc.bEndpointAddress = fs_mbim_in_desc.bEndpointAddress;
	ss_mbim_out_desc.bEndpointAddress = fs_mbim_out_desc.bEndpointAddress;
	ss_mbim_notify_desc.bEndpointAddress =
		fs_mbim_notify_desc.bEndpointAddress;

	ret = usb_assign_descriptors(f, mbim_fs_function, mbim_hs_function,
		mbim_ss_function, NULL);

	if (ret)
		return ret;

	ret = -ENOMEM;

	for (i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
		req = mbim_req_alloc(dev->notify_ep, USB_BUFSIZE, GFP_KERNEL);
		if (!req)
			goto fail_notify_reqs;
		req->context = dev;
		req->complete = mbim_notify_complete;
		list_add(&req->list, &dev->tx_notifyreqs);
	}

	for (i = 0; i < MAX_BULKIN_REQUESTS; i++) {
		req = mbim_req_alloc(dev->in_ep, USB_DL_BUFSIZE, GFP_KERNEL);
		if (!req)
			goto fail_tx_reqs;
		dev->func_flag = NCM_ADD_HEADER;
		list_add(&req->list, &dev->tx_reqs);
		pr_info("usb: %s: pre DL req: %pK, req->buf: %pK\n", __func__, req, &req->buf);
	}

	for (i = 0; i < MAX_BULKOUT_REQUESTS; i++) {
		req = mbim_req_alloc(dev->out_ep, USB_BUFSIZE, GFP_KERNEL);
		//skb = __netdev_alloc_skb(dev->ndev, SZ_2K, GFP_KERNEL);
		if (!req)
			goto fail_rx_reqs;
		pr_info("usb: %s: pre alloc skb pointer: %pK\n", __func__, req);
		//skb->data = req->buf;
		//req->context = skb;
		list_add_tail(&req->list, &dev->rx_reqs);
	}

	dev->ndev = alloc_netdev(sizeof(struct mbim_port), "mbim_net%d", NET_NAME_UNKNOWN, mnet_setup);
	if (!dev->ndev)
		pr_err("usb: %s: alloc_netdev fail\n", __func__);

	snprintf(dev->ndev->name, sizeof(dev->ndev->name), "%s%%d", "mbim");

	ret = register_netdev(dev->ndev);
	if (ret) {
		pr_err("usb: %s: ERR! register_netdev fail\n", __func__);
	}

	mnet_port = netdev_priv(dev->ndev);
	mnet_port->mbim = dev;

	dev->mbim_dl  = create_singlethread_workqueue("mbim_dl");
	if (!dev->mbim_dl) {
		pr_err("usb: %s: Unable to create workqueue: mbim_wq\n", __func__);
		ret = -ENOMEM;
		goto fail_rx_reqs;
	}

	dev->mbim_ul = create_singlethread_workqueue("mbim_ul");
	if (!dev->mbim_ul) {
		pr_err("usb: %s: Unable to create workqueue: mbim_wq\n", __func__);
		destroy_workqueue(dev->mbim_dl);
		ret = -ENOMEM;
		goto fail_rx_reqs;
	}

	INIT_WORK(&dev->start_xmit, mnet_start_xmit);
	hrtimer_init(&dev->tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dev->tx_timer.function = tx_timeout;
	hrtimer_init(&dev->rx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dev->rx_timer.function = rx_timeout;

	if (dev->wrap & dev->unwrap)
		mnet_reset_values(dev);

	INIT_LIST_HEAD(&dev->cpkt_req_q);
	INIT_LIST_HEAD(&dev->cpkt_resp_q);

	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	dev->mbim_ready = true;
	dev->mbim_cdev = cdev;

	pr_info("usb: mbim_func_bind-- OK\n");

	return 0;
#if 0
fail_cdev_add:
	device_destroy(usb_gadget_class, devt);
#endif

fail_rx_reqs:
	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next, struct usb_request, list);
		list_del(&req->list);
		mbim_req_free(dev->out_ep, req);
	}

fail_tx_reqs:
	while (!list_empty(&dev->tx_reqs)) {
		req = container_of(dev->tx_reqs.next, struct usb_request, list);
		list_del(&req->list);
		mbim_req_free(dev->in_ep, req);
	}

fail_notify_reqs:
	while (!list_empty(&dev->tx_notifyreqs)) {
		req = container_of(dev->tx_notifyreqs.next, struct usb_request, list);
		list_del(&req->list);
		mbim_req_free(dev->notify_ep, req);
	}

	return ret;

}
static int mbim_func_set_alt(struct usb_function *f,
	unsigned intf, unsigned alt)
{
	struct mbim_dev *dev = func_to_mbim(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret = -ENOTSUPP;

	pr_info("usb: %s intf %d alt %d\n", __func__, intf, alt);
	/* Control interface has only altsetting 0 */
	if (intf == dev->ctrl_id) {
		if (alt != 0)
			goto fail;

		if (dev->notify_ep->driver_data) {
			pr_info("usb: %s CONTROL_INTERFACE\n", __func__);
			usb_ep_disable(dev->notify_ep);
		}

		if (!(dev->notify_ep->desc)) {
			DBG(cdev, "init ncm ctrl %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, dev->notify_ep)) {
				dev->notify_ep->desc = NULL;
				pr_err("usb: %s :Failed configuring notify ep %s: err %d\n",
						__func__, dev->notify_ep->name, ret);
				goto fail;
			}
		}
		ret = usb_ep_enable(dev->notify_ep);
		if (ret) {
			pr_err("usb: %s ep#%s enable failed, err#%d\n",
						__func__, dev->notify_ep->name, ret);
			goto fail1;
		}
		dev->notify_ep->driver_data = dev;

	/* Data interface has two altsettings, 0 and 1 */
	} else if (intf == dev->data_id) {
		if (alt > 1)
			goto fail;
		dev->interface = intf;
		atomic_set(&dev->queue->open_excl, 1);

		if (alt == 0) {
			if (dev->in_ep->driver_data) {
				DBG(cdev, "reset ncm\n");
				pr_info("usb: %s reset mbim (alt = 0)\n", __func__);
				if (dev->in_ep->desc)
					usb_ep_disable(dev->in_ep);

				if (dev->out_ep->desc)
					usb_ep_disable(dev->out_ep);

				dev->in_ep->desc = NULL;
				dev->out_ep->desc = NULL;

				/* connection gone */
				atomic_set(&dev->notify_count, 0);
				pr_info("usb: %s: dev->notify_count=%d ---\n",
						__func__, atomic_read(&dev->notify_count));
			}
			/* if data interface released .. we must clear EnableDevice flag */
			dev->EnabledDevice = false;
			wake_up(&dev->sync_wq);
			pr_info("usb: %s : interface down : (%d) \n",
							__func__, dev->cnt_notify_int);
		}

		if (alt == 1) {
			pr_info("Alt set 1, initialize ports\n");

			if (!dev->in_ep->desc || !dev->out_ep->desc) {
				DBG(cdev, "init mbim\n");
				if (config_ep_by_speed(cdev->gadget, f,
						       dev->in_ep) ||
				    config_ep_by_speed(cdev->gadget, f,
						       dev->out_ep)) {
					dev->in_ep->desc = NULL;
					dev->out_ep->desc = NULL;
					goto fail;
				}
			}
			mbim_clear_data_queues(dev);
			dev->mbim_online = true;
			ret = usb_ep_enable(dev->in_ep);
			if (ret != 0) {
				DBG(dev, "in ep enable %s --> %d\n", dev->in_ep->name, ret);
				goto fail1;
			}
			dev->in_ep->driver_data = dev;
			ret = usb_ep_enable(dev->out_ep);
			if (ret != 0) {
				DBG(dev, "out ep enable %s --> %d\n", dev->out_ep->name, ret);
				goto fail1;
			}
			dev->out_ep->driver_data = dev;

			dev->EnabledDevice = true;
			setup_rx_reqs(dev);
			queue_work_on(6, dev->mbim_dl, &dev->start_xmit);
			hrtimer_start(&dev->rx_timer, ktime_set(0, MBIM_UL_SEND_TIME), HRTIMER_MODE_REL);
			atomic_set(&dev->queue->open_excl, 1);
			wake_up(&dev->sync_wq);
			dev->cnt_notify_int = 0;
		}

	} else
		goto fail;
	mbim_resume_cmpl = 1;
	return 0;
fail1:
	if (dev->in_ep->driver_data)
		usb_ep_disable(dev->in_ep);
	if (dev->out_ep->driver_data)
		usb_ep_disable(dev->out_ep);
	if (dev->notify_ep->driver_data)
		usb_ep_disable(dev->notify_ep);
	dev->in_ep->desc = NULL;
	dev->out_ep->desc = NULL;
	dev->notify_ep->desc = NULL;
fail:
	pr_info("usb: %s intf %d alt %d fail return\n", __func__, intf, alt);
	return -EINVAL;
}

static int mbim_func_get_alt(struct usb_function *f,
	unsigned intf)
{
	struct mbim_dev *dev = func_to_mbim(f);
	int ret = -ENOTSUPP;

	//pr_info("mbim_func_get_alt++\n");
	if (intf == 0)
		return 0;
	if (intf == 1)
		return dev->data_alt_int;
	return ret;
}

static void mbim_func_disable(struct usb_function *f)
{
	struct mbim_dev *dev = func_to_mbim(f);

	pr_info("usb: %s +++\n", __func__);

	dev->EnabledDevice = false;
	dev->mbim_online = false;

	mbim_clear_queues(dev);

	mbim_reset_interface(dev);

	/* break queue wait logic */
	wake_up(&dev->queue->dl_wait);
	wake_up(&dev->read_wq);
	wake_up(&dev->sync_wq);

	hrtimer_cancel(&dev->rx_timer);
	pr_info("usb: %s ----\n", __func__);
}

static inline struct f_mbim_opts
*to_f_mbim_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_mbim_opts,
		func_inst.group);
}

static void mbim_attr_release(struct config_item *item)
{
	struct f_mbim_opts *opts = to_f_mbim_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations mbim_item_ops = {
	.release = mbim_attr_release,
};

static ssize_t f_mbim_opts_pnp_string_show(struct config_item *item,
	char *page)
{
	struct f_mbim_opts *opts = to_f_mbim_opts(item);
	int result = 0;

	mutex_lock(&opts->lock);
	if (!opts->pnp_string)
		goto unlock;

	result = strlcpy(page, opts->pnp_string, PAGE_SIZE);
	if (result >= PAGE_SIZE) {
		result = PAGE_SIZE;
	}
	else if (page[result - 1] != '\n' && result + 1 < PAGE_SIZE) {
		page[result++] = '\n';
		page[result] = '\0';
	}

unlock:
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_mbim_opts_pnp_string_store(struct config_item *item,
	const char *page, size_t len)
{
	struct f_mbim_opts *opts = to_f_mbim_opts(item);
	char *new_pnp;
	int result;

	mutex_lock(&opts->lock);

	new_pnp = kstrndup(page, len, GFP_KERNEL);
	if (!new_pnp) {
		result = -ENOMEM;
		goto unlock;
	}

	if (opts->pnp_string_allocated)
		kfree(opts->pnp_string);

	opts->pnp_string_allocated = true;
	opts->pnp_string = new_pnp;
	result = len;
unlock:
	mutex_unlock(&opts->lock);

	return result;
}

CONFIGFS_ATTR(f_mbim_opts_, pnp_string);

static ssize_t f_mbim_opts_q_len_show(struct config_item *item,
	char *page)
{
	struct f_mbim_opts *opts = to_f_mbim_opts(item);
	int result;

	mutex_lock(&opts->lock);
	result = sprintf(page, "%d\n", opts->q_len);
	mutex_unlock(&opts->lock);

	return result;
}

static ssize_t f_mbim_opts_q_len_store(struct config_item *item,
	const char *page, size_t len)
{
	struct f_mbim_opts *opts = to_f_mbim_opts(item);
	int ret;
	u16 num;

	mutex_lock(&opts->lock);
	if (opts->refcnt) {
		ret = -EBUSY;
		goto end;
	}

	ret = kstrtou16(page, 0, &num);
	if (ret)
		goto end;

	opts->q_len = (unsigned)num;
	ret = len;
end:
	mutex_unlock(&opts->lock);
	return ret;
}

CONFIGFS_ATTR(f_mbim_opts_, q_len);

static struct configfs_attribute *mbim_attrs[] = {
	&f_mbim_opts_attr_pnp_string,
	&f_mbim_opts_attr_q_len,
	NULL,
};

static struct config_item_type mbim_func_type = {
	.ct_item_ops = &mbim_item_ops,
	.ct_attrs = mbim_attrs,
	.ct_owner = THIS_MODULE,
};

static inline int gmbim_get_minor(void)
{
	int ret;

	ret = ida_simple_get(&mbim_ida, 0, 0, GFP_KERNEL);
	if (ret >= MBIM_MINORS) {
		ida_simple_remove(&mbim_ida, ret);
		ret = -ENODEV;
	}

	return ret;
}

static inline void gmbim_put_minor(int minor)
{
	ida_simple_remove(&mbim_ida, minor);
}

static int gmbim_setup(int);
static void gmbim_cleanup(void);

static void gmbim_free_inst(struct usb_function_instance *f)
{
	struct f_mbim_opts *opts;

	opts = container_of(f, struct f_mbim_opts, func_inst);

	mutex_lock(&mbim_ida_lock);

	gmbim_put_minor(opts->minor);
	if (ida_is_empty(&mbim_ida))
		gmbim_cleanup();

	mutex_unlock(&mbim_ida_lock);

	misc_deregister(&mbim_device);

	if (opts->pnp_string_allocated)
		kfree(opts->pnp_string);
	kfree(opts);
}

static void gmbim_free(struct usb_function *f)
{
	struct mbim_dev *dev = func_to_mbim(f);
	struct f_mbim_opts *opts;

	opts = container_of(f->fi, struct f_mbim_opts, func_inst);
	kfree(dev);
	mutex_lock(&opts->lock);
	--opts->refcnt;
	mutex_unlock(&opts->lock);
}

static void mbim_func_unbind(struct usb_configuration *c,
	struct usb_function *f)
{
	struct mbim_dev	*dev;
	struct usb_request	*req;
	unsigned long	flags;

	pr_info("usb: mbim_func_unbind++\n");

	mbim_string_defs[0].id = 0;
	dev = func_to_mbim(f);

	device_destroy(usb_gadget_class, MKDEV(major, dev->minor));
	unregister_netdev(dev->ndev);
	free_netdev(dev->ndev);

	/* we must already have been disconnected ... no i/o may be active */
	WARN_ON(!list_empty(&dev->tx_reqs_active));

	//kfree(dev->notify_req->buf);
	//usb_ep_free_request(dev->notify_ep, dev->notify_req);

	/* Free all memory for this driver. */

	dev->mbim_ready = false;
	wake_up(&dev->sync_wq);

	spin_lock_irqsave(&dev->notify_req_lock, flags);
	while (!list_empty(&dev->tx_notifyreqs)) {
		req = container_of(dev->tx_notifyreqs.next, struct usb_request,
			list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->notify_req_lock, flags);

		mbim_req_free(dev->notify_ep, req);
		spin_lock_irqsave(&dev->notify_req_lock, flags);
	}
	spin_unlock_irqrestore(&dev->notify_req_lock, flags);

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&dev->tx_reqs)) {
		req = container_of(dev->tx_reqs.next, struct usb_request,
			list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		mbim_req_free(dev->in_ep, req);
		spin_lock_irqsave(&dev->lock, flags);
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (dev->current_rx_req != NULL)
		mbim_req_free(dev->out_ep, dev->current_rx_req);

	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next,
			struct usb_request, list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		mbim_req_free(dev->out_ep, req);
		spin_lock_irqsave(&dev->lock, flags);
	}
	while (!list_empty(&dev->rx_buffers)) {
		req = container_of(dev->rx_buffers.next,
			struct usb_request, list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->lock, flags);

		mbim_req_free(dev->out_ep, req);
		spin_lock_irqsave(&dev->lock, flags);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	mbim_clear_data_queues(dev);

	usb_free_all_descriptors(f);
	destroy_workqueue(dev->mbim_dl);
	destroy_workqueue(dev->mbim_ul);
	hrtimer_cancel(&dev->rx_timer);
	dev->mbim_ready = false;

	pr_info("usb: mbim_func_unbind--\n");
}

static struct usb_function_instance *gmbim_alloc_inst(void)
{
	struct f_mbim_opts *opts;
	struct usb_function_instance *ret;
	int status = 0;

	pr_info("usb: %s: +++\n", __func__);
	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = gmbim_free_inst;
	ret = &opts->func_inst;

	mutex_lock(&mbim_ida_lock);

	if (ida_is_empty(&mbim_ida)) {
		status = gmbim_setup(MBIM_MINORS);
		if (status) {
			ret = ERR_PTR(status);
			kfree(opts);
			goto unlock;
		}
	}

	opts->minor = gmbim_get_minor();
	if (opts->minor < 0) {
		ret = ERR_PTR(opts->minor);
		kfree(opts);
		if (ida_is_empty(&mbim_ida))
			gmbim_cleanup();
		goto unlock;
	}
	config_group_init_type_name(&opts->func_inst.group, "",
		&mbim_func_type);

	status = misc_register(&mbim_device);
	if (status)
		pr_err("usb: %s: mbim driver failed to register\n", __func__);

	pr_info("usb: %s: ---\n", __func__);
unlock:
	mutex_unlock(&mbim_ida_lock);
	return ret;
}

#ifdef CONFIG_OF
static int mbim_parse_dt(struct mbim_dev	*dev)
{
	struct device_node *np = NULL;
	int gpio = 0;	

	np = of_find_compatible_node(NULL, NULL, "samsung,usb-mbim");
	if (!np) {
		printk("usb: %s : mbim dt find fails\n", __func__);
		goto err;
	}

	gpio = of_get_named_gpio(np, "gpio_mbim_host_wakeup", 0);
	if (!gpio_is_valid(gpio)) {		
		pr_err("usb: %s: gpio_mbim_host_wakeup: Invalied gpio pins\n", __func__);
		goto err;
	} else
		dev->host_wakeup_gpio = gpio;
	return 0;
err:
	dev->host_wakeup_gpio = -1;
	return -1;
}
#endif

static struct usb_function *gmbim_alloc(struct usb_function_instance *fi)
{
	struct mbim_dev	*dev;
	struct f_mbim_opts	*opts;
#if CONFIG_OF
	int ret;
	int gpio = -1;
#endif 
	pr_info("usb: %s: +++\n", __func__);
	opts = container_of(fi, struct f_mbim_opts, func_inst);

	mutex_lock(&opts->lock);
	if (opts->minor >= minors) {
		mutex_unlock(&opts->lock);
		return ERR_PTR(-ENOENT);
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		mutex_unlock(&opts->lock);
		return ERR_PTR(-ENOMEM);
	}

	dev->mnet_info = kzalloc(sizeof(struct mnet_data_info), GFP_KERNEL);
	if (!dev->mnet_info) {
		pr_err("usb: %s: alloc mnet_info fail\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
#ifdef CONFIG_OF
	/* Get gpio info from DT */
	ret = mbim_parse_dt(dev);
	if (ret) {
		pr_err("usb: %s: we can't get gpio info\n", __func__);
	} else {
		gpio = dev->host_wakeup_gpio;
		if (gpio_is_valid(gpio)) {
			ret = gpio_request(gpio, "mbim_pc_waekup");
			if (ret) {
				pr_err("usb: failed to gpio request %d\n", gpio);
			}
			gpio_direction_output(gpio, 1);
			pr_info("usb: %s : gpio = %d \n", __func__, gpio);
		}
	}
#endif	
	++opts->refcnt;
	dev->minor = opts->minor;
	dev->pnp_string = opts->pnp_string;
	dev->q_len = opts->q_len;
	mutex_unlock(&opts->lock);

	dev->function.name = "mbim";
	dev->function.bind = mbim_func_bind;
	dev->function.setup = mbim_func_setup;
	dev->function.unbind = mbim_func_unbind;
	dev->function.set_alt = mbim_func_set_alt;
	dev->function.get_alt = mbim_func_get_alt;
	dev->function.disable = mbim_func_disable;
	dev->function.req_match = gmbim_req_match;
	dev->function.free_func = gmbim_free;
	dev->function.strings = mbim_strings;

	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);
	INIT_LIST_HEAD(&dev->rx_buffers);
	INIT_LIST_HEAD(&dev->tx_reqs_active);
	INIT_LIST_HEAD(&dev->ul_shared_reqs);
	INIT_LIST_HEAD(&dev->dl_shared_reqs);

	INIT_LIST_HEAD(&dev->tx_notifyreqs);

	skb_queue_head_init(&dev->rx_sk_buffers);

	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	spin_lock_init(&dev->notify_req_lock);
	mutex_init(&dev->lock_mbim_io);
	init_waitqueue_head(&dev->rx_wait);
	init_waitqueue_head(&dev->tx_wait);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	init_waitqueue_head(&dev->sync_wq);

	dev->interface = -1;
	dev->mbim_cdev_open = 0;
	//	dev->mbim_status = NOT_ERROR;
	dev->current_rx_req = NULL;
	dev->current_rx_bytes = 0;
	dev->current_rx_buf = NULL;

	dev->mbim_cdev_open = 0;
	dev->queue = mbim_queue_init();
	mbim_resume_cmpl = 1;
#if 0
	if (gpio_is_valid(gpio)) {
		register_mbim_gpio_info(gpio);
	}
#endif

	dev->wrap = 1;
	dev->unwrap = 1;
	dev->dl_max_pkts_per_xfer = MBIM_MAX_DATAGRAM;
	dev->ul_max_pkts_per_xfer = 1;

	_mbim_dev = dev;

	pr_info("usb: %s: ---\n", __func__);
	return &dev->function;
}

DECLARE_USB_FUNCTION_INIT(mbim, gmbim_alloc_inst, gmbim_alloc);
MODULE_LICENSE("GPL");

static int gmbim_setup(int count)
{
	int status;
	dev_t devt;

	usb_gadget_class = class_create(THIS_MODULE, "usb_mbim_gadget");
	if (IS_ERR(usb_gadget_class)) {
		status = PTR_ERR(usb_gadget_class);
		usb_gadget_class = NULL;
		pr_err("unable to create usb_gadget class %d\n", status);
		return status;
	}

	status = alloc_chrdev_region(&devt, 0, count, "USB mbim gadget");
	if (status) {
		pr_err("alloc_chrdev_region %d\n", status);
		class_destroy(usb_gadget_class);
		usb_gadget_class = NULL;
		return status;
	}

	major = MAJOR(devt);
	minors = count;

	return status;
}

static void gmbim_cleanup(void)
{
	if (major) {
		unregister_chrdev_region(MKDEV(major, 0), minors);
		major = minors = 0;
	}

	class_destroy(usb_gadget_class);
	usb_gadget_class = NULL;
}
