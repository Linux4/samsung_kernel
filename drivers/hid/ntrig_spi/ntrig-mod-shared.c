#include <linux/kernel.h>
#include <linux/module.h>
/* The following file has been added in kernel version 2.6.39 */
#include <linux/sched.h>

#include "ntrig-mod-shared.h"

/* Heartbeat support */
u8 hb_request_msg[NCP_HB_REQUEST_SIZE] = {
	NCP_START_BYTE, 0, 0, 0xfa, 0, 1, NCP_HB_GROUP, NCP_HB_CODE,
	0, 0, 0, 0, 0, 0,	/* return code + reserved */
	0, 0, 0, 0,		/* count */
	DEFAULT_HB_ENABLE,	/* enable HB */
	DEFAULT_HB_RATE,	/* HB rate */
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255
};
EXPORT_SYMBOL_GPL(hb_request_msg);

int is_heartbeat(u8 *data)
{
	return ((data[0] == NCP_START_BYTE) &&
		(data[NCP_GROUP_OFFSET] == NCP_HB_GROUP) &&
		(data[NCP_CODE_OFFSET] == NCP_HB_CODE));
}
EXPORT_SYMBOL_GPL(is_heartbeat);

int is_unsolicited_ncp(u8 *data)
{
	return ((data[0] == NCP_START_BYTE) &&
		(data[NCP_TYPE_OFFSET] == NCP_UNSOLICITED_TYPE));
}
EXPORT_SYMBOL_GPL(is_unsolicited_ncp);

u16 get_ncp_host_address(u8 *data)
{
	return data[1] + data[2] * 256;
}
EXPORT_SYMBOL_GPL(get_ncp_host_address);

u8 get_enable_heartbeat_param(void)
{
	return hb_request_msg[NCP_HB_REQUEST_ENABLE_OFFSET];
}
EXPORT_SYMBOL_GPL(get_enable_heartbeat_param);

static void update_heartbeat_msg_checksum(void)
{
	int i;
	u8 sum;
	for (i = 0, sum = 0; i < NCP_HB_REQUEST_SIZE - 1; i++)
		sum += hb_request_msg[i];
	hb_request_msg[NCP_HB_REQUEST_SIZE - 1] = 0 - sum;
}

void set_enable_heartbeat_param(u8 enable)
{
	hb_request_msg[NCP_HB_REQUEST_ENABLE_OFFSET] = enable;
	update_heartbeat_msg_checksum();

}
EXPORT_SYMBOL_GPL(set_enable_heartbeat_param);

u8 get_heartbeat_rate_param(void)
{
	return hb_request_msg[NCP_HB_REQUEST_HB_RATE_OFFSET];
}
EXPORT_SYMBOL_GPL(get_heartbeat_rate_param);

void set_heartbeat_rate_param(u8 rate)
{
	hb_request_msg[NCP_HB_REQUEST_HB_RATE_OFFSET] = rate;
	update_heartbeat_msg_checksum();
}
EXPORT_SYMBOL_GPL(set_heartbeat_rate_param);

/*************************
 *   NCP queue support   *
 *************************/

/** initial/max size (bytes) of the circular buffer we keep for ncp commands */
/* !!! the size must be a power of 2 (for kfifo_alloc) !!! */
#define NCP_FIFO_INITIAL_SIZE 4096
#define NCP_FIFO_MAX_SIZE (1024 * 128)
/** the maximum time to wait for a reply to an ncp command - 1 second. */
#define MAX_NCP_COMMAND_REPLY_TIMEOUT (HZ * 1)

/** Initialize the ncp fifo structure */
int init_ncp_fifo(struct ntrig_ncp_fifo *ncp_fifo,
	unsigned int *fifo_full_counter)
{
	/* init variables relatedto raw read/write
	 * we use the spi_lock to protect the fifo, no additional lock needed */
	int err = kfifo_alloc(&ncp_fifo->fifo, NCP_FIFO_INITIAL_SIZE,
		GFP_KERNEL);
	if (err) {
		ntrig_err("%s: fail to alloc ncp fifo, err = %d\n", __func__,
			err);
		return err;
	}
	sema_init(&ncp_fifo->fifo_lock, 1);
	init_waitqueue_head(&ncp_fifo->wait_queue);
	ncp_fifo->data_arrived = 0;
	ncp_fifo->fifo_full_counter = fifo_full_counter;
	if (fifo_full_counter != NULL)
		*fifo_full_counter = 0;
	return 0;
}
EXPORT_SYMBOL_GPL(init_ncp_fifo);

/** Release ncp fifo resources */
void uninit_ncp_fifo(struct ntrig_ncp_fifo *ncp_fifo)
{
	kfifo_free(&ncp_fifo->fifo);
}
EXPORT_SYMBOL_GPL(uninit_ncp_fifo);

static void discard_ncp_fifo_bytes(struct ntrig_ncp_fifo *ncp_fifo, u16 size)
{
	int res;
	/* split large chunks to parts that fit into scratch_buf */
	while (size > 0) {
		int get_size = size;
		if (get_size > SCRATCH_BUFFER_SIZE)
			get_size = SCRATCH_BUFFER_SIZE;
		res = kfifo_out(&ncp_fifo->fifo, ncp_fifo->scratch_buf,
			get_size);
		size -= get_size;
	}
}

/* Discards a message from the fifo, to make space for another message.
 * Must be called with the fifo lock held.
 */
static void discard_ncp_message(struct ntrig_ncp_fifo *ncp_fifo)
{
	u16 size;
	int res;
	res = kfifo_out(&ncp_fifo->fifo, (u8 *)&size, sizeof(u16));
	discard_ncp_fifo_bytes(ncp_fifo, size);
}

/* Extends the fifo size to NCP_FIFO_MAX_SIZE. */
static void expand_ncp_fifo(struct ntrig_ncp_fifo *ncp_fifo)
{
	struct kfifo *old_fifo = &ncp_fifo->fifo;
	struct kfifo new_fifo;
	int buf_size, get_size, res;
	int err = kfifo_alloc(&new_fifo, NCP_FIFO_MAX_SIZE, GFP_KERNEL);
	if (err) {
		ntrig_err("%s: fail to allocate a larger ncp fifo, err = %d\n",
			__func__, err);
		return;
	}
	/* copy the contents of the old fifo to the new one */
	buf_size = sizeof(ncp_fifo->scratch_buf);
	while ((get_size = kfifo_len(old_fifo)) > 0) {
		if (get_size > buf_size)
			get_size = buf_size;
		res = kfifo_out(old_fifo, ncp_fifo->scratch_buf, get_size);
		kfifo_in(&new_fifo, ncp_fifo->scratch_buf, get_size);
	}
	kfifo_free(old_fifo);
	ncp_fifo->fifo = new_fifo;
	ntrig_dbg("%s: NCP fifo has been expanded to %u bytes\n", __func__,
		NCP_FIFO_MAX_SIZE);
}

/* Free at least 'size' bytes in the fifo by:
 * - extending the fifo if not already extended
 * - discarding old messages
 */
static void free_space_in_fifo(struct ntrig_ncp_fifo *ncp_fifo, int size)
{
	if (kfifo_size(&ncp_fifo->fifo) < NCP_FIFO_MAX_SIZE)
		expand_ncp_fifo(ncp_fifo);
	while (kfifo_avail(&ncp_fifo->fifo) < size) {
		if (ncp_fifo->fifo_full_counter != NULL)
			(*ncp_fifo->fifo_full_counter)++;
		ntrig_err(
			"%s: NCP receive buffer is full, discarding messages."
			"Buffer size=%d, available=%d\n",
			__func__, kfifo_size(&ncp_fifo->fifo),
			kfifo_avail(&ncp_fifo->fifo));
		discard_ncp_message(ncp_fifo);
	}
}

/* Push NCP message to fifo, freeing space if needed. */
void enqueue_ncp_message(struct ntrig_ncp_fifo *ncp_fifo, const u8 *data,
	u16 size)
{
	struct kfifo *fifo = &ncp_fifo->fifo;
	int needed_size = size + sizeof(u16);
	down(&ncp_fifo->fifo_lock);
	/* if fifo can't hold our message, free space in it */
	if (kfifo_avail(fifo) < needed_size)
		free_space_in_fifo(ncp_fifo, needed_size);
	/* insert the message */
	kfifo_in(fifo, (unsigned char *)&size, sizeof(u16));
	kfifo_in(fifo, data, size);
	/* wake up any threads blocked on the buffer */
	ncp_fifo->data_arrived = 1;
	wake_up(&ncp_fifo->wait_queue);
	up(&ncp_fifo->fifo_lock);
}
EXPORT_SYMBOL_GPL(enqueue_ncp_message);

/* Helper for read_ncp_message (below).
 * Reads a message from the fifo and stores it in the buffer. Messages are
 * stored as: 2 bytes length, followed by <length> bytes for the message.
 * Returns the number of bytes copied to the buffer, or 0 if the fifo was empty.
 * No locking is performed in this function, it is caller responsibility to
 * protect the fifo if needed.
 */
static int get_ncp_message_from_fifo(struct ntrig_ncp_fifo *ncp_fifo, char *buf,
	size_t count)
{
	u16 data_len = 0, discard_len = 0;
	int res;
	struct kfifo *fifo = &ncp_fifo->fifo;
	if (kfifo_len(fifo)) {	/* we have data */
		res = kfifo_out(fifo, (u8 *)&data_len, sizeof(u16));
		ntrig_dbg("%s: ncp message size %d, count = %d\n", __func__,
			data_len, count);
		if (data_len > count) {
			/* message is larger than caller's buffer - fill the
			 * buffer and discard the rest of the message to keep
			 * in sync with the (size, data) pairs in the fifo.
			 */
			discard_len = data_len - count;
			data_len = count;
		}
		res = kfifo_out(fifo, buf, data_len);
		if (discard_len)
			discard_ncp_fifo_bytes(ncp_fifo, discard_len);
	}
	return data_len;
}

/* Read NCP message. On success, return number of bytes read and fill buffer.
 * On failure, return a negative value.
 * Caller should allocate a buffer of sufficient size.
 * If there is no data in the fifo, the function blocks until data is received,
 * or until timeout is reached (1 sec).
 * Note: buf is kernel memory
 */
int read_ncp_message(struct ntrig_ncp_fifo *ncp_fifo, u8 *buf, size_t count)
{
	int err, data_len;

	err = down_interruptible(&ncp_fifo->fifo_lock);
	if (err != 0) /* we were interrupted, cancel the request */
		return -ERESTARTSYS;

	data_len = get_ncp_message_from_fifo(ncp_fifo, buf, count);
	if (data_len > 0) {
		/* we got data, return immediately */
		up(&ncp_fifo->fifo_lock);
		return data_len;
	}

	/* fifo is empty, wait for a message to arrive. Release the lock before
	 * waiting */
	ncp_fifo->data_arrived = 0;
	up(&ncp_fifo->fifo_lock);
	err = wait_event_interruptible_timeout(ncp_fifo->wait_queue,
		(ncp_fifo->data_arrived != 0), MAX_NCP_COMMAND_REPLY_TIMEOUT);
	if (err < 0) /* we were interrupted */
		return -ERESTARTSYS;

	/* get the lock again */
	err = down_interruptible(&ncp_fifo->fifo_lock);
	if (err != 0) /* we were interrupted, cancel the request */
		return -ERESTARTSYS;

	/* read from fifo again, this time return 0 if there is no data
	 * (timeout) */
	data_len = get_ncp_message_from_fifo(ncp_fifo, buf, count);
	up(&ncp_fifo->fifo_lock);
	return data_len;
}
EXPORT_SYMBOL_GPL(read_ncp_message);

