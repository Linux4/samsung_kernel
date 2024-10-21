/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK USB Logger Driver
 * *
 * Copyright (c) 2023 MediaTek Inc.
 */


#ifndef _U_LOGGER_H__
#define _U_LOGGER_H__

#include <linux/usb.h>

/* logger->cmd bit defined */
#define LOGGER_DUMPER_ENABLE    (0x01 << 0)
#define LOGGER_DUMPER_DEBUG     (0x01 << 1)
#define LOGGER_DUMPER_ENQ       (0x01 << 2)
#define LOGGER_DUMPER_DEQ       (0x01 << 3)

/* logger->sub_cmd bit defined */
#define LOGGER_TYPE_MASK        (0x0f)
#define LOGGER_TYPE_CTRL        (0x01 << 0)
#define LOGGER_TYPE_ISOC        (0x01 << 1)
#define LOGGER_TYPE_BULK        (0x01 << 2)
#define LOGGER_TYPE_INTR        (0x01 << 3)

struct u_logger {
	struct device *dev;
	struct proc_dir_entry *root;
	u32 cmd;
	u32 sub_cmd;
};

struct mtk_urb {
	void *addr;
	void *transfer_buffer;
	int transfer_buffer_length;
	int actual_length;
	struct usb_ctrlrequest *setup_pkt;
	const struct usb_endpoint_descriptor *desc;
};

int raw_str_length(struct u_logger *logger, struct mtk_urb *urb);
void raw_str_fill(struct u_logger *logger, char *data, struct mtk_urb *urb);
int info_str_length(struct u_logger *logger, struct mtk_urb *urb);
void info_str_fill(struct u_logger *logger, char *info, struct mtk_urb *urb);
char *xfer_type_name(int type);

#define check_cmd(cmd, bit) (cmd & bit)
#define dumper_dbg(logger, fmt, args...) do { \
		if (check_cmd(logger->cmd, LOGGER_DUMPER_DEBUG)) \
			dev_info(logger->dev, "%s: "fmt, __func__, ##args); \
	} while(0)

#define is_dumper_enable(logger) check_cmd(logger->cmd, LOGGER_DUMPER_ENABLE)
#define is_trace_enqueue(logger) check_cmd(logger->cmd, LOGGER_DUMPER_ENQ)
#define is_trace_dequeue(logger) check_cmd(logger->cmd, LOGGER_DUMPER_DEQ)

#endif /* _U_LOGGER_H__ */
