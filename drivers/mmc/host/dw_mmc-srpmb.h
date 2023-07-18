/*
 * Secure RPMB header for Exynos MMC RPMB
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _MMC_SRPMB_H
#define _MMC_SRPMB_H

#define GET_WRITE_COUNTER			1
#define WRITE_DATA				2
#define READ_DATA				3

#define AUTHEN_KEY_PROGRAM_RES			0x0100
#define AUTHEN_KEY_PROGRAM_REQ			0x0001
#define RESULT_READ_REQ				0x0005
#define RPMB_END_ADDRESS			0x4000

#define RPMB_PACKET_SIZE			512
#define RPMB_REQRES				510
#define RPMB_RESULT				508

#define WRITE_COUNTER_DATA_LEN_ERROR		0x601
#define WRITE_COUNTER_SECURITY_OUT_ERROR	0x602
#define WRITE_COUNTER_SECURITY_IN_ERROR		0x603
#define WRITE_DATA_LEN_ERROR			0x604
#define WRITE_DATA_SECURITY_OUT_ERROR		0x605
#define WRITE_DATA_RESULT_SECURITY_OUT_ERROR	0x606
#define WRITE_DATA_SECURITY_IN_ERROR		0x607
#define READ_LEN_ERROR				0x608
#define READ_DATA_SECURITY_OUT_ERROR		0x609
#define READ_DATA_SECURITY_IN_ERROR		0x60A
#define RPMB_INVALID_COMMAND			0x60B
#define RPMB_FAIL_SUSPEND_STATUS		0x60C

#define RPMB_IN_PROGRESS			0xDCDC
#define RPMB_PASSED				0xBABA

#define IS_INCLUDE_RPMB_DEVICE			"0:0:0:1"

#define ON					1
#define OFF					0

#define RPMB_BUF_MAX_SIZE			32 * 1024
#define RELIABLE_WRITE_REQ_SET			(1 << 31)

struct _mmc_rpmb_ctx {
	struct device *dev;
	int irq;
	void *wsm_virtaddr;
	dma_addr_t wsm_phyaddr;
	struct workqueue_struct *srpmb_queue;
	struct work_struct work;
	struct wakeup_source wakesrc;
	spinlock_t lock;
	struct notifier_block pm_notifier;
};

struct _mmc_rpmb_req {
	uint32_t cmd;
	volatile uint32_t status_flag;
	uint32_t type;
	uint32_t data_len;
	uint32_t inlen;
	uint32_t outlen;
	uint8_t rpmb_data[0];
};

struct rpmb_packet {
	u16	request;
	u16	result;
	u16	count;
	u16	address;
	u32	write_counter;
	u8	nonce[16];
	u8	data[256];
	u8	Key_MAC[32];
	u8	stuff[196];
};

struct mmc_rpmb_data {
	struct device dev;
	struct cdev chrdev;
	int id;
	unsigned int part_index;
	struct mmc_blk_data *md;
	struct list_head node;
};

struct mmc_blk_data {
	struct device *parent;
	struct gendisk *disk;
	struct mmc_queue queue;
	struct list_head part;
	struct list_head rpmbs;

	unsigned int flags;
#define MMC_BLK_CMD23   	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR  	(1 << 1)	/* MMC Reliable write support */

	unsigned int usage;
	unsigned int read_only;
	unsigned int part_type;
	unsigned int reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD      BIT(3)
#define MMC_BLK_CQE_RECOVERY    BIT(4)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with dev_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int area_type;

	/* debugfs files (only in main mmc_blk_data) */
	struct dentry *status_dentry;
	struct dentry *ext_csd_dentry;
};

struct mmc_blk_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
	struct mmc_rpmb_data *rpmb;
};

#endif
