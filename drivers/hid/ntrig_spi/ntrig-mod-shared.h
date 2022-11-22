/*
 *  HID driver for N-Trig touchscreens
 *
 *  Copyright (c) 2012 N-TRIG
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef _NTRIG_MOD_SHARED_H
#define _NTRIG_MOD_SHARED_H

#include <linux/kfifo.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>

#include "typedef-ntrig.h"
#include "ntrig-common.h"

/* Driver version */
#define NTRIG_DRIVER_MODULE_VERSION "-3"

/* Heartbeat support */
#define FIRST_HB_MESSAGE 0x01
#define NCP_HOST_ADDRESS_OFFSET 1
#define NCP_TYPE_OFFSET 5
#define NCP_GROUP_OFFSET 6
#define NCP_CODE_OFFSET 7

#define NCP_HB_REQUEST_SIZE 250
#define NCP_HB_REQUEST_COUNT_OFFSET 14
#define NCP_HB_REQUEST_ENABLE_OFFSET 18
#define NCP_HB_REQUEST_HB_RATE_OFFSET 19
#define NCP_HB_REQUEST_RESERVED_OFFSET 20

#define NCP_HB_REPORT_RATE_OFFSET 65
#define NCP_HB_REPORT_ENABLE_OFFSET 68

#define NCP_START_BYTE 0x7e
#define NCP_UNSOLICITED_TYPE 0x82
#define NCP_HB_GROUP 0x20
#define NCP_HB_CODE 0x1

#define DEFAULT_HB_ENABLE 1
#define DEFAULT_HB_RATE 10

extern u8 hb_request_msg[NCP_HB_REQUEST_SIZE];
/* check if the given sensor message is an HB message */
int is_heartbeat(u8 *data);
/* check if the given sensor message is an unsolicited NCP message */
int is_unsolicited_ncp(u8 *data);
/* get the host address from an ncp message */
u16 get_ncp_host_address(u8 *data);
/* get/set the parameters of the HB request */
u8 get_enable_heartbeat_param(void);
void set_enable_heartbeat_param(u8 enable);
u8 get_heartbeat_rate_param(void);
void set_heartbeat_rate_param(u8 rate);

/*************************
 *   NCP queue support   *
 *************************/

#define SCRATCH_BUFFER_SIZE 264

struct ntrig_ncp_fifo {
	/** circular buffer for storing incoming raw replies */
	struct kfifo fifo;
	/** semaphore (mutex) for protecting fifo access */
	struct semaphore fifo_lock;
	/** wait queue for implementing read blocking (when raw_fifo is empty) */
	wait_queue_head_t wait_queue;
	/** flag used when waiting for buffer to be filled */
	int data_arrived;
	/** scratch buffer for discarding packets (when circular buffer is full) and
	 * for copying the old fifo to the new one during fifo expansion */
	u8 scratch_buf[SCRATCH_BUFFER_SIZE];
	/** pointer to bus driver counter of discarded messages due to full fifo;
	 * may be NULL. */
	unsigned int *fifo_full_counter;
};

/* Initialize the ncp fifo */
int init_ncp_fifo(struct ntrig_ncp_fifo *fifo, unsigned int *fifo_full_counter);

/* Release ncp fifo resources */
void uninit_ncp_fifo(struct ntrig_ncp_fifo *fifo);

/* Push ncp message to fifo, freeing space if needed. */
void enqueue_ncp_message(struct ntrig_ncp_fifo *ncp_fifo, const u8 *data,
	u16 size);

/**
 * Read ncp message. On success, return number of bytes read and fill buffer.
 * On failure, return a negative value.
 * Caller should allocate at least MAX_SPI_RESPONSE_SIZE bytes in the buffer.
 * If there is no data in the fifo, the function blocks until data is received,
 * or until timeout is reached (1 sec).
 * Note: buf is kernel memory.
 */
int read_ncp_message(struct ntrig_ncp_fifo *ncp_fifo, u8 *buf, size_t count);

#endif /* _NTRIG_MOD_SHARED_H */
