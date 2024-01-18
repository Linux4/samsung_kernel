#ifndef U_MBIM_H
#define U_MBIM_H

#include <linux/usb/composite.h>
#include "u_ether.h"

struct f_mbim_opts {
	struct usb_function_instance	func_inst;
	int								minor;
	char							*pnp_string;
	bool							pnp_string_allocated;
	unsigned						q_len;
	struct mutex					lock;
	int								refcnt;
};

struct ncm_header {
	u32 signature;
	u16 header_len;
	u16 sequence;
	u16 blk_len;
	u16 index;
	u32 dgram_sig;
	u16 dgram_header_len;
	u16 dgram_rev;
	u16 dgram_index0;
	u16 dgram_len0;
} __packed;

#define NCM_NTH_SIGNATURE		(0x484D434E)
#define NCM_NTH_LEN16			(0xC)
#define NCM_NTH_SEQUENCE		(0x0)
#define NCM_NTH_INDEX16			(0xC)
#define NCM_NDP_SIGNATURE		(0x304D434E)
#define NCM_NDP_LEN16			(0xB4)
#define NCM_NDP_REV			(0x0)

#define NTB_DEFAULT_IN_SIZE	16384 * 4
#define NTB_OUT_SIZE		16384

#define TX_MAX_NUM_DPE		32

/* Delay for the transmit to wait before sending an unfilled NTB frame. */
#define TX_TIMEOUT_NSECS	300000

#define MBIM_MAX_DATAGRAM        (42)

/*
 * Here are options for NCM Datagram Pointer table (NDP) parser.
 * There are 2 different formats: NDP16 and NDP32 in the spec (ch. 3),
 * in NDP16 offsets and sizes fields are 1 16bit word wide,
 * in NDP32 -- 2 16bit words wide. Also signatures are different.
 * To make the parser code the same, put the differences in the structure,
 * and switch pointers to the structures when the format is changed.
 */

struct ndp_parser_opts {
	u32		nth_sign;
	u32		ndp_sign;
	unsigned	nth_size;
	unsigned	ndp_size;
	unsigned	dpe_size;
	unsigned	ndplen_align;
	/* sizes in u16 units */
	unsigned	dgram_item_len; /* index or length */
	unsigned	block_length;
	unsigned	ndp_index;
	unsigned	reserved1;
	unsigned	reserved2;
	unsigned	next_ndp_index;
};

#define INIT_NDP16_OPTS {					\
		.nth_sign = USB_CDC_NCM_NTH16_SIGN,		\
		.ndp_sign = USB_CDC_MBIM_NDP16_IPS_SIGN,	\
		.nth_size = sizeof(struct usb_cdc_ncm_nth16),	\
		.ndp_size = sizeof(struct usb_cdc_ncm_ndp16),	\
		.dpe_size = sizeof(struct usb_cdc_ncm_dpe16),	\
		.ndplen_align = 4,				\
		.dgram_item_len = 1,				\
		.block_length = 1,				\
		.ndp_index = 1,					\
		.reserved1 = 0,					\
		.reserved2 = 0,					\
		.next_ndp_index = 1,				\
	}

#define INIT_NDP32_OPTS {					\
		.nth_sign = USB_CDC_NCM_NTH32_SIGN,		\
		.ndp_sign = USB_CDC_NCM_NDP32_NOCRC_SIGN,	\
		.nth_size = sizeof(struct usb_cdc_ncm_nth32),	\
		.ndp_size = sizeof(struct usb_cdc_ncm_ndp32),	\
		.dpe_size = sizeof(struct usb_cdc_ncm_dpe32),	\
		.ndplen_align = 8,				\
		.dgram_item_len = 2,				\
		.block_length = 2,				\
		.ndp_index = 2,					\
		.reserved1 = 1,					\
		.reserved2 = 2,					\
		.next_ndp_index = 2,				\
	}

static const struct ndp_parser_opts ndp16_opts = INIT_NDP16_OPTS;
static const struct ndp_parser_opts ndp32_opts = INIT_NDP32_OPTS;

struct mnet_data_info {
	const struct ndp_parser_opts	*parser_opts;
	bool				is_crc;

	/* For multi-frame NDP TX */
	struct sk_buff			*skb_tx_data;
	struct sk_buff			*skb_tx_ndp;
	u16				nth_sequence;
	u16				nth_wblocklength;
	u16				nth_wndpindex;

	u16				ndp_length;

	u16				ndp_dgram_count;
	bool				timer_force_tx;

	bool				timer_stopping;

	struct gether			port;
};

static inline void put_ncm(__le16 **p, unsigned size, unsigned val)
{
	switch (size) {
	case 1:
		put_unaligned_le16((u16)val, *p);
		break;
	case 2:
		put_unaligned_le32((u32)val, *p);

		break;
	default:
		BUG();
	}

	*p += size;
}

static inline unsigned get_ncm(__le16 **p, unsigned size)
{
	unsigned tmp;

	switch (size) {
	case 1:
		tmp = get_unaligned_le16(*p);
		break;
	case 2:
		tmp = get_unaligned_le32(*p);
		break;
	default:
		BUG();
	}

	*p += size;
	return tmp;
}

#endif /* U_MBIM_H */
