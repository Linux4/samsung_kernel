/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/ioctl.h>
#include <linux/types.h>

#define MTP_MAX_PACKET_LEN_FROM_APP 16

#define	MTP_ACM_ENABLE		0
#define	MTP_ONLY_ENABLE		1
#define	MTP_DISABLE		2
#define	MTP_CLEAR_HALT		3
#define	MTP_WRITE_INT_DATA	4
#define SET_MTP_USER_PID	5
#define GET_SETUP_DATA		6
#define SET_SETUP_DATA		7
#define SEND_RESET_ACK		8
#define SET_ZLP_DATA		9
#define GET_HIGH_FULL_SPEED	10

#define MTP_VBUS_DISABLE 12
#define SIG_SETUP		44

/*PIMA15740-2000 spec*/
#define USB_PTPREQUEST_CANCELIO   0x64    /* Cancel request */
#define USB_PTPREQUEST_GETEVENT   0x65    /* Get extened event data */
#define USB_PTPREQUEST_RESET      0x66    /* Reset Device */
#define USB_PTPREQUEST_GETSTATUS  0x67    /* Get Device Status */
#define USB_PTPREQUEST_CANCELIO_SIZE 6
#define USB_PTPREQUEST_GETSTATUS_SIZE 12

int mtp_enable(void);
void mtp_function_enable(int enable);

struct usb_mtp_ctrlrequest {
	struct usb_ctrlrequest	setup;
};


struct usb_container_header {
	uint32_t  Length;/* the valid size, in BYTES, of the container  */
	uint16_t   Type;/* Container type */
	uint16_t   Code;/* Operation code, response code, or Event code */
	uint32_t  TransactionID;/* host generated number */
};

struct read_send_info {
	int	Fd;/* Media File fd */
	uint64_t Length;/* the valid size, in BYTES, of the container  */
	uint16_t Code;/* Operation code, response code, or Event code */
	uint32_t TransactionID;/* host generated number */
};


extern struct usb_function_instance *alloc_inst_mtp_ptp(bool mtp_config);
extern struct usb_function *function_alloc_mtp_ptp(
			struct usb_function_instance *fi, bool mtp_config);

struct mtp_file_range {
    /* file descriptor for file to transfer */
	int         fd;
    /* offset in file for start of transfer */
	loff_t      offset;
    /* number of bytes to transfer */
	int64_t     length;
    /* MTP command ID for data header,
     * used only for MTP_SEND_FILE_WITH_HEADER
     */
	uint16_t    command;
    /* MTP transaction ID for data header,
     * used only for MTP_SEND_FILE_WITH_HEADER
     */
	uint32_t    transaction_id;
};

struct mtp_event {
    /* size of the event */
	size_t      length;
    /* event data to send */
	void        *data;
};

/* Sends the specified file range to the host */
#define MTP_SEND_FILE              _IOW('M', 0, struct mtp_file_range)
/* Receives data from the host and writes it to a file.
 * The file is created if it does not exist.
 */
#define MTP_RECEIVE_FILE           _IOW('M', 1, struct mtp_file_range)
/* Sends an event to the host via the interrupt endpoint */
#define MTP_SEND_EVENT             _IOW('M', 3, struct mtp_event)
/* Sends the specified file range to the host,
 * with a 12 byte MTP data packet header at the beginning.
 */
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
#define SEND_FILE_WITH_HEADER 11
#else
#define MTP_SEND_FILE_WITH_HEADER  _IOW('M', 4, struct mtp_file_range)
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef __KERNEL__
#ifdef CONFIG_COMPAT
struct __compat_mtp_event {
	compat_size_t   length;
	compat_caddr_t  data;
};

#endif
#endif

#define COMPAT_MTP_SEND_EVENT   _IOW('M', 3, struct __compat_mtp_event)


