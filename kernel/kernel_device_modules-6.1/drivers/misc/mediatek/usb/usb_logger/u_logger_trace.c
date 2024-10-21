// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Dumper Driver
 * *
 * Copyright (c) 2023 MediaTek Inc.
 */


#define CREATE_TRACE_POINTS
#include "u_logger_trace.h"

void u_logger_dbg_trace(struct device *dev, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	trace_u_logger_log(dev, &vaf);
	va_end(args);
}

/* Print in Hex format, 1 byte data contains in 2 characters */
#define DATA_STR_SIZE(urb)	(urb->transfer_buffer_length * 2 + 1)
#define ACTUAL_DATA_STR_SIZE(urb) (urb->actual_length * 2 + 1)

#define EMPTY_STR_SIZE	(5)
#define SETUP_PKT_SIZE	(30)
#define INFO_STR_SIZE(urb)	(SETUP_PKT_SIZE + DATA_STR_SIZE(urb))

static void fill_str_msb(char *data, int data_length, char *str, int str_length)
{
	int i, transferred = 0;

	for (i = 0; i < data_length; i++)
		transferred += snprintf(str + transferred, str_length - transferred, "%02x", data[i]);

	*(str + transferred) = '\0';
}

int info_str_length(struct u_logger *logger, struct mtk_urb *urb)
{
	bool is_ctrl = (usb_endpoint_type(urb->desc) == USB_ENDPOINT_XFER_CONTROL);

	/* only ep0 has essentail info in enqueue phase */
	return (is_ctrl && urb->setup_pkt) ? INFO_STR_SIZE(urb) : EMPTY_STR_SIZE;
}

void info_str_fill(struct u_logger *logger, char *info_str, struct mtk_urb *urb)
{
	int total = info_str_length(logger, urb), transferred = 0;
	struct usb_ctrlrequest *setup = urb->setup_pkt;
	//char *data = (char *)urb->transfer_buffer;

	if (total == EMPTY_STR_SIZE) {
		*info_str = '\0';
		return;
	}

	dumper_dbg(logger, "%s urb:%p buf:%p len:%d str_len:%d\n", __func__,
		urb->addr, urb->transfer_buffer, urb->transfer_buffer_length, total);

	/* setup packet */
	transferred += snprintf(info_str, total, "[%02x %02x %04x %04x %04x] ",
		setup->bRequestType, setup->bRequest,
		le16_to_cpu(setup->wValue), le16_to_cpu(setup->wIndex),
		le16_to_cpu(setup->wLength));

	/* data packet */
	//fill_str_msb(data, urb->transfer_buffer_length, info_str + transferred, total - transferred);
}

int raw_str_length(struct u_logger *logger, struct mtk_urb *urb)
{
	if (!urb->actual_length)
		return EMPTY_STR_SIZE;

	return ACTUAL_DATA_STR_SIZE(urb);
}

void raw_str_fill(struct u_logger *logger, char *data_str, struct mtk_urb *urb)
{
	int total = raw_str_length(logger, urb);

	if (total == EMPTY_STR_SIZE) {
		*data_str = '\0';
		return;
	}

	dumper_dbg(logger, "%s urb:%p buf:%p len:%d str_len:%d\n", __func__,
		urb->addr, urb->transfer_buffer, urb->actual_length, total);

	memset(data_str, '0', total);
	fill_str_msb((char *)urb->transfer_buffer, urb->actual_length, data_str, total);
}

/* do not modify xfer name, unless you modify parsing tool as well */
char *xfer_type_name(int type)
{
	if (type == USB_ENDPOINT_XFER_CONTROL)
		return "ctrl";
	else if (type == USB_ENDPOINT_XFER_ISOC)
		return "isoc";
	else if (type == USB_ENDPOINT_XFER_BULK)
		return "bulk";
	else if (type == USB_ENDPOINT_XFER_INT)
		return "intr";
	else
		return "unknown";
}
