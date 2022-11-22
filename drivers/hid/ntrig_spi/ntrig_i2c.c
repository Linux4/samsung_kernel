#include <linux/module.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/time.h>

#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"
#include "ntrig_low_msg.h"
#include "ntrig_i2c_low.h"
#include "ntrig-mod-shared.h"

/*max TX and RX should be equal to or less than 64 bytes
 *because of the size of each node in the queue in the bridge
 */

#define FLAGS_BYTES_SIZE		1
#define REPORT_ID_BYTES_SIZE		1
#define LENGTH_BYTES_SIZE		2
#define REGISTER_ADDRESS_BYTES_SIZE	2
#define COMMAND_BYTE_SIZE		4
/*TX*/
#define MAX_TX_FRAGMENT_SIZE		59
#define I2C_TX_BUF_SIZE		(MAX_TX_FRAGMENT_SIZE + \
				REGISTER_ADDRESS_BYTES_SIZE + \
				LENGTH_BYTES_SIZE + \
				REPORT_ID_BYTES_SIZE + \
				FLAGS_BYTES_SIZE + \
				COMMAND_BYTE_SIZE)

/*RX*/
#define MAX_REPORT_SIZE		27
#define MAX_RX_SIZE		(MAX_REPORT_SIZE + FLAGS_BYTES_SIZE)
#define I2C_RX_BUF_SIZE		(MAX_RX_SIZE + LENGTH_BYTES_SIZE + \
				REPORT_ID_BYTES_SIZE)
				/*size to read from the input register*/


/*  driver name */
#define DRIVER_NAME	"ntrig_i2c_mod"
/* the ID of the this I2C driver */
#define NTRIG_I2C_ID


/* max number of fingers in hybrid report */
#define MAX_NUM_OF_FINGERS_HYBRID 1

/* report ID of pen  */
#define REPORT_ID_PEN			0x01
/*  report ID of MT*/
#define REPORT_ID_MULTITOUCH		0x03
/* report ID of NCP request */
#define REPORT_NCP_REQUEST		0x05
/* report ID of NCP reply */
#define REPORT_NCP_REPLY		0x06
/* report ID of get feature reply */
#define GET_FEATURE_REPLY		0x0D

/** maximum size of an aggregated logical SPI message,
 *  16 (max fragments) * 122 (max message size) */
#define MAX_LOGICAL_MESSAGE_SIZE			1952

/** maximum size of I2C buffer for send/receive: in G2 = 256 bytes **/
#define MAX_I2C_TRANSFER_BUFFER_SIZE_G2		256
/** size (bytes) of	the	circular buffer	we keep	for	raw/ncp
 *  commands */
#define		RAW_RECEIVE_BUFFER_INITIAL_SIZE		4096
#define		RAW_RECEIVE_BUFFER_MAX_SIZE		(1024*120)

/** the maximum time to wait for a reply from raw(ncp/dfu) command. */
#define		MAX_I2C_RAW_COMMAND_REPLY_TIMEOUT	(HZ*1) /* 1 seconds */
/** max times to retry i2c transfer before exit with error**/
#define NTRIG_I2C_RETRY			  5

/** addresses of internal registers in the SPI to I2C bridge **/
/* hid descriptor register */
#define HID_DESC_REGISTER	0x01
/* report descriptor register*/
#define REPORT_DESC_REGISTER	0x02
/* input register*/
#define INPUT_REGISTER		0x03
/* command register*/
#define COMMAND_REGISTER	0x05
/* data register*/
#define DATA_REGISTER		0x06

/*******************  counters *******************/
/** counters names **/
/* device to host */
#define CNTR_NAME_MULTITOUCH	"channel multitouch"
#define CNTR_NAME_PEN		"channel pen"
#define CNTR_NAME_MAINT_REPLY	"channel maint reply"

/* host to device */
#define CNTR_NAME_MAINT		"channel maint"

#define CNTR_NAME_I2C_TRANS_FAIL "i2c transfer failures"
#define CNTR_NAME_PCKT_LOST	 "number of lost packet"

enum _i2c_cntrs_names {
	CNTR_MULTITOUCH = 0,
	CNTR_PEN,
	CNTR_MAINTREPLAY,
	CNTR_MAINT,
	CNTR_I2C_TRANS_FAIL,
	CNTR_PCKT_LOST,
	CNTR_NUMBER_OF_I2C_CNTRS
};

/*
 * the actual list of the counters.
 * Used for statistics
 */
static struct _ntrig_counter i2c_cntrs_list[CNTR_NUMBER_OF_I2C_CNTRS] = {
		{.name = CNTR_NAME_MULTITOUCH, .count = 0},
		{.name = CNTR_NAME_PEN, .count = 0},
		{.name = CNTR_NAME_MAINT_REPLY, .count = 0},
		{.name = CNTR_NAME_MAINT, .count = 0},
		{.name = CNTR_NAME_I2C_TRANS_FAIL, .count = 0},
		{.name = CNTR_NAME_PCKT_LOST, .count = 0},
};
/*
 * reset all counters to 0
 */
void i2c_reset_counters(void)
{
	int i;
	for (i = 0; i < CNTR_NUMBER_OF_I2C_CNTRS; ++i)
		i2c_cntrs_list[i].count = 0;
}

/*
 * return the array of ntrig_counter and it's length
 */
int i2c_get_counters(struct _ntrig_counter **counters_list_local,  int *length)
{
	*counters_list_local = i2c_cntrs_list;
	*length = CNTR_NUMBER_OF_I2C_CNTRS;
	return 0;
}
/******************* end counters *******************/

struct ntrig_i2c_msg {
	u16 length;
	u8 reportID;
	u8 data[MAX_RX_SIZE];
} __packed;

struct _ntrig_mt_finger {
	u8 fingerStatus;
	u16 fingerIndex;
	u16 x[2];
	u16 y[2];
	u16 dx;
	u16 dy;
	u32 vendorDefind;
} __packed;

struct _ntrig_mt_report {
	u8 reportID;
	u16 reportCount;
	struct _ntrig_mt_finger fingers[MAX_NUM_OF_FINGERS_HYBRID];
	u8 contactCount;
	u32 scanTime;
} __packed;

struct ntrig_i2c_mod_privdata {
	/** pointer back to the	i2c client */
	struct i2c_client *client;
	/** the	sensor id assigned to us by dispatcher */
	int sensor_id;
	/** gpio index for the irq line */
	unsigned irq_gpio;
	/** flags to use for requesting interrupt handler */
	int irq_flags;
	/** gpio index for power */
	int pwr_gpio;
	/** gpio index for output_enable, copied from spi platform data */
	int oe_gpio;
	/** true if	output_enable line is connected	to inverter,
	 *  copied from	spi	platform data */
	int oe_inverted;
	/** semaphore (mutex) for protecting the i2c transfer */
	struct semaphore i2c_lock;
	/** shared structure between this driver and dispatcher TODO
	 *  to be removed in new architecture */
	struct _ntrig_bus_device *ntrig_dispatcher;
	/** last pen button	code, for implementing pen event logic
	 *  (usbhid	compatibility) */
	int pen_last_btn;
	/** --------- WRITE/READ BULK/RAW NCP COMMANDS -------- */
	/** circular buffer	for	storing	incoming raw replies */
	struct kfifo raw_fifo;
	/** wait queue for implementing	read blocking (when	raw_fifo
	 *  is empty) */
	wait_queue_head_t raw_wait_queue;
	/** flag used when waiting for buffer to be	filled */
	int raw_data_arrived;
	/** scratch	buffer for discarding packets (when	circular
	 *  buffer is full) and for copying the old fifo to the
	 *  new one during fifo expansion */
	u8 scratch_buf[MAX_I2C_TRANSFER_BUFFER_SIZE_G2];
	/** buffer to send messages to I2C bus */
	uint8_t tx_buf[I2C_TX_BUF_SIZE];
	/** buffer for aggregating fragmented messages */
	u8 aggregation_buf[MAX_LOGICAL_MESSAGE_SIZE];
	/** size of aggregated message */
	int aggregation_size;
	/** number of logical message fragments left for complete message */
	int fragments_left;
	/**buffer to input register reports**/
	struct ntrig_i2c_msg input_report;
	/**buffer to pass to dispatcher**/
	struct mr_message_types_s mr[1];
	/*buffer to save reply of set feature request*/
	struct ntrig_i2c_msg get_feature_reply_buf;
	/*for check for packet lost*/
	u16 expected_mt_counter;
	/* need to send heartbeat request as an ACK to a heartbeat report */
	u8 send_hb_request;
	/* do we expect a heartbeat reply to a request that we sent? */
	u8 expecting_hb_reply;
};

static int ntrig_i2c_transfer(struct i2c_client *client,
				struct i2c_msg *msgs, int num_msgs)
{
	int ret;
	int i = 0;
	ntrig_dbg("inside %s, num of msgs: %d\n", __func__, num_msgs);

	do {
		ret = i2c_transfer(client->adapter, msgs, num_msgs);
		if (ret != num_msgs) {
			ntrig_err(
				"in %s, i2c_transfer failed, in try number %d\n"
				, __func__, i);
		}
	} while (ret != num_msgs && ++i < NTRIG_I2C_RETRY);

	if (ret != num_msgs) {
		ntrig_err("%s: i2c_transfer failure\n", __func__);
		return -1;
	}
	return 0;
}

/*
 * read buffer from internal register in the i2c client
 */
int ntrig_i2c_read(struct i2c_client *client, short reg, u8* buf, short len)
{
	int ret;

	struct i2c_msg msgs[2] = {
			{ client->addr, client->flags, 2, (u8 *)&reg },
			{ client->addr, client->flags | I2C_M_RD, len, buf}
	};

	ntrig_dbg(
		"in %s, register is %hd, length is %hd\n", __func__, reg, len);

	ret = ntrig_i2c_transfer(client, msgs, 2);

	ntrig_dbg("in %s, ret is %d\n", __func__, ret);

	return ret;
}

/*
 * fast read buffer from default register in the i2c client
 */
int ntrig_i2c_fast_read(struct i2c_client *client, u8* buf, short len)
{
	int ret = 0;
	struct i2c_msg msgs[1] = {
			{ client->addr, client->flags | I2C_M_RD, len, buf}
	};

	ntrig_dbg("in %s, length is %hd\n", __func__, len);

	ret = ntrig_i2c_transfer(client, msgs, 1);

	ntrig_dbg("in %s, ret is %d\n", __func__, ret);

	return ret;
}

/*
 * write buffer to internal register in the i2c client
 * the two first bytes of buf should contain the internal register to write into
 */
int ntrig_i2c_write(struct i2c_client *client, u8* buf, short len)
{
	int ret = 0;
	short reg = *((short *)&buf[0]);

	struct i2c_msg msgs[1] = {
			{ client->addr, client->flags, len, buf}
	};

	ntrig_dbg(
		"in %s, register is %hd, length is %hd\n", __func__, reg, len);

	ret = ntrig_i2c_transfer(client, msgs, 1);

	ntrig_dbg("in %s, ret is %d\n", __func__, ret);

	return ret;
}


struct i2c_client *g_i2c_client;
struct ntrig_i2c_mod_privdata *g_pdata;

/**
 * helper for spi_read_raw (below)
 * reads packet from fifo and store it in the buffer.
 * Packets are stored as 2 bytes length, followed by <length>
 * bytes for the message
 * Return the number of bytes copied to the buffer, or 0 if the
 * fifo was empty
 * No locking is performed in this function, it is caller
 * responsibility to protect the fifo if needed
 */
static int i2c_get_raw_data_from_fifo(struct kfifo *fifo,
					char *buf, size_t count)
{
	u16 data_len;
	int res;
	if (kfifo_len(fifo)) {
		/* we have data, can return immediately */
		res = kfifo_out(fifo, (unsigned char *)&data_len, sizeof(u16));
		ntrig_dbg(
			"%s: raw message size %d, count = %d\n",
				__func__, data_len, count);
		if (data_len > count)
			data_len = count;

		res = kfifo_out(fifo, buf, data_len);
	} else {
		data_len = 0;
	}
	ntrig_dbg("in %s, return data len is %hd\n", __func__, data_len);
	return data_len;
}

/**
 * Read NCP msg from RAW device.
 * On I2C, all responses come from the same device. We separate
 * them into HID and RAW based on some fields in the message
 * if success return number of bytes read and fill buffer. If
 * failed return <0 allocate at least MAX_SPI_RESPONSE_SIZE
 * bytes in the buffer
 * If there is no data in the buffer, it will block until data
 * is received, or until timeout is reached (500ms). Return -1
 * if no data arrived
 * Note: buf is kernel memory
 */
static int i2c_read_raw(void *dev, char *buf, size_t count)
{
	struct i2c_client *client = dev;
	struct ntrig_i2c_mod_privdata *privdata =
		(struct ntrig_i2c_mod_privdata *)dev_get_drvdata(&client->dev);
	int err, data_len;

	ntrig_dbg("in %s\n", __func__);
	ntrig_dbg("in %s, before 1st down\n", __func__);
	/* use i2c_lock to protect the circular buffer*/
	err = down_interruptible(&privdata->i2c_lock);
	if (err != 0) {
		/* we were interrupted, cancel the request*/
		return -ERESTARTSYS;
	}
	ntrig_dbg("in %s, after 1st down\n", __func__);

	data_len = i2c_get_raw_data_from_fifo(&privdata->raw_fifo, buf, count);
	if (data_len > 0) {
		/* we got data, return immediately */
		up(&privdata->i2c_lock);
		ntrig_dbg("in %s, after 1st up\n", __func__);
		return data_len;
	}

	/* buffer is empty, we will need to wait.
		Release the lock before waiting */
	privdata->raw_data_arrived = 0;

	up(&privdata->i2c_lock);
	ntrig_dbg("in %s, after 2nd up\n", __func__);

	err = wait_event_interruptible_timeout(privdata->raw_wait_queue,
					(privdata->raw_data_arrived != 0),
					MAX_I2C_RAW_COMMAND_REPLY_TIMEOUT);
	if (err < 0) {
		/* we were interrupted */
		ntrig_dbg(
		    "in %s, we were interrupted (1st), exit\n", __func__);
		return -ERESTARTSYS;
	}

	ntrig_dbg("in %s, before 2nd down\n", __func__);
	/*get the lock again*/
	err = down_interruptible(&privdata->i2c_lock);
	if (err != 0) {
		/*we were interrupted, cancel the request */
		ntrig_dbg(
		    "in %s, we were interrupted (2nd), exit\n", __func__);
		return -ERESTARTSYS;
	}
	ntrig_dbg("in %s, after 2nd down\n", __func__);

	/* read from fifo again,
	   this time return 0 if there is no data (timeout) */
	data_len = i2c_get_raw_data_from_fifo(&privdata->raw_fifo, buf, count);

	up(&privdata->i2c_lock);

	ntrig_dbg("in %s, after 3rd up\n", __func__);

	ntrig_dbg("%s ended succesfully\n", __func__);
	ntrig_dbg("in %s, return data len is %d\n", __func__, data_len);
	return data_len;
}

/**
 * Write NCP msg to RAW device (on I2C it is the same as HID
 * device though we may use a different channel in the I2C
 * message). if success return number of bytes written >0 if
 * failed returns <0
 * the function returns immediately and does not wait for a
 * reply. Replies are buffered and obtained using i2c_read_raw,
 * which blocks if there are no replies waiting.
 */
static int i2c_write_raw(void *dev, const char *buf, short msg_len)
{
	struct i2c_client *client = dev;
	struct ntrig_i2c_mod_privdata *privdata =
		(struct ntrig_i2c_mod_privdata *)dev_get_drvdata(&client->dev);

	int err;
	/*command of set report:
	low byte:
	 reserved    report_type     report ID
	    00	 11		0101		==0x35
	high byte:
	 reserved      opcode
	   0000	  0011		==0x3
	*/
	u8 setReportCommand[2] = {0x35, 0x3};
	short commandRegister = COMMAND_REGISTER;
	short dataRegister = DATA_REGISTER;
	u8 command[4];
	u8 reportID = REPORT_NCP_REQUEST;

	u8 left_frag = 1;
	u8 flags = 0;
	u8 *pLocationInBuf = (u8 *)buf;
	u8 *pLocationInTx = privdata->tx_buf;
	short toSendLen = 0;
	short lengthStillToSend = msg_len;
	short packetLength = 0;
	ntrig_dbg("inside %s\n", __func__);

	if (msg_len <= 0) {
		ntrig_err("%s: empty message\n", __func__);
		return 0;
	}

	ntrig_dbg("%s:  msg_len=%hd\n", __func__, msg_len);

	memcpy(command, &commandRegister, sizeof(short));
	memcpy(command+sizeof(short), setReportCommand,
						sizeof(setReportCommand));


	if (msg_len > MAX_TX_FRAGMENT_SIZE) {
		left_frag = (msg_len / MAX_TX_FRAGMENT_SIZE) +
				((msg_len % MAX_TX_FRAGMENT_SIZE == 0) ? 0 : 1);
	}
	ntrig_dbg("%s: initial left_frag: 0x%02X\n", __func__, left_frag);

	ntrig_dbg("in %s, before down\n", __func__);
	/*critical section: i2c bus*/
	err = down_interruptible(&privdata->i2c_lock);
	if (err != 0) {
		/* we were interrupted, cancel the request*/
		return -ERESTARTSYS;
	}
	ntrig_dbg("in %s, after down\n", __func__);

	do {
		left_frag--;
		flags = 0x80 + left_frag;
		toSendLen = (lengthStillToSend < MAX_TX_FRAGMENT_SIZE) ?
				lengthStillToSend : MAX_TX_FRAGMENT_SIZE;

		ntrig_dbg("%s: left_frag: 0x%02X\n", __func__, left_frag);
		ntrig_dbg("%s: toSendLen: %hd\n", __func__, toSendLen);
		ntrig_dbg(
			"%s: lengthStillToSend: %hd\n",
			__func__, lengthStillToSend);

		/*we subtruct the 2 bytes of address register length and 4 bytes of the command */
		packetLength = I2C_TX_BUF_SIZE - 6;
		/* adding command content & address into pLocationInTx */
		memcpy(pLocationInTx,command,sizeof(command));
		pLocationInTx += sizeof(command);
		/*copy the internal register bytes*/
		memcpy(pLocationInTx, &dataRegister, sizeof(short));
		pLocationInTx += sizeof(short);
		/*copy the length bytes*/
		memcpy(pLocationInTx, &packetLength, sizeof(short));
		pLocationInTx += sizeof(short);
		/*copy the reportID of NCP request*/
		memcpy(pLocationInTx, &reportID, sizeof(u8));
		pLocationInTx += sizeof(u8);

		memcpy(pLocationInTx, &flags, sizeof(u8));
		pLocationInTx += sizeof(u8);
		/*copy the actual buffer*/
		memcpy(pLocationInTx, pLocationInBuf, toSendLen);
		err = ntrig_i2c_write(privdata->client,
					privdata->tx_buf, I2C_TX_BUF_SIZE);
		if (err) {
			ntrig_err("%s: i2c write failure %d\n",
							__func__, err);
			goto exit_err;
		}
		memset(privdata->tx_buf, 0xFF, sizeof(privdata->tx_buf));

		pLocationInTx = privdata->tx_buf;
		pLocationInBuf += toSendLen;
		lengthStillToSend -= toSendLen;
	} while (left_frag != 0);

	i2c_cntrs_list[CNTR_MAINT].count++;

	/* normal finish */
	up(&privdata->i2c_lock);
	ntrig_dbg("in %s, after up\n", __func__);

	ntrig_dbg("%s ended succesfully\n", __func__);
	/* done */
	return msg_len;
exit_err:
	up(&privdata->i2c_lock);
	ntrig_dbg("in %s, after up (exit_err)\n", __func__);
	return 0;
}

/**
 * helper for managing the raw receive buffer. Discards a
 * message from the buffer
 * Must be called with i2c_lock held
 */
static void i2c_discard_raw_message(struct ntrig_i2c_mod_privdata *privdata)
{
	u16 size;
	int res;
	res = kfifo_out(&privdata->raw_fifo,
					(unsigned char *)&size, sizeof(u16));
	while (size > 0) {
		int getsize = size;
		if (getsize > MAX_I2C_TRANSFER_BUFFER_SIZE_G2)
			getsize = MAX_I2C_TRANSFER_BUFFER_SIZE_G2;

		res = kfifo_out(&privdata->raw_fifo, &privdata->scratch_buf[0],
								getsize);
		size -= getsize;
	}
}

/**
 * called to extend the raw fifo size when the first debug agent reply is
 * received. The debug agent requires a significantly larger fifo.
 */
static void i2c_expand_raw_fifo(struct ntrig_i2c_mod_privdata *privdata)
{
	struct kfifo new_fifo;
	int buf_size, get_size, res;
	int err = kfifo_alloc(&new_fifo, RAW_RECEIVE_BUFFER_MAX_SIZE,
								GFP_KERNEL);
	if (err) {
		ntrig_dbg("%s: fail to allocate a larger raw_fifo, err = %d\n",
							__func__, err);
		return;
	}
	/* copy the contents of the old fifo to the new one */
	buf_size = sizeof(privdata->scratch_buf);
	while ((get_size = kfifo_len(&privdata->raw_fifo)) > 0) {
		if (get_size > buf_size)
			get_size = buf_size;
		res = kfifo_out(&privdata->raw_fifo, &privdata->scratch_buf[0],
								get_size);
		kfifo_in(&new_fifo, &privdata->scratch_buf[0], get_size);
	}
	kfifo_free(&privdata->raw_fifo);
	privdata->raw_fifo = new_fifo;
}

/*
 * verify that current counter is equal to he expected
 * if not, print all the counters that we missed
 */
void checkForLostPackets(u16 cur, u16 *expected)
{
	if ((*expected) != cur) {
		u16 nLost = cur - (*expected);
		ntrig_err(
			"we lost %hu packet(s), current: %hu, expected: %hu\n",
						nLost, cur, (*expected));
		i2c_cntrs_list[CNTR_PCKT_LOST].count += nLost;/*for statistics*/
		(*expected) = cur+1;
	} else {
		(*expected)++;
	}
}

static int send_heartbeat_request(struct ntrig_i2c_mod_privdata *privdata)
{
	int ret;
	/* We expect a solicited reply from the FW if "Enable HB" is 0 */
	privdata->expecting_hb_reply = (get_enable_heartbeat_param() == 0);
	ntrig_dbg("%s - enable HB=%d\n", __func__,
		get_enable_heartbeat_param());
	ret = i2c_write_raw(privdata->client, hb_request_msg,
		NCP_HB_REQUEST_SIZE);
	if (ret != NCP_HB_REQUEST_SIZE) {
		privdata->expecting_hb_reply = 0;
		ntrig_err(
			"%s: Failed to ACK unsolicited heartbeat message, "
			"ret=%d\n", __func__, ret);
		return 1;
	}
	return 0;
}

/****************************************************************************************/
/**
 * called when we have a complete message received from the I2C
 * layer
 */
static void i2c_process_message(struct ntrig_i2c_mod_privdata *privdata)
{
	u8 reportID = privdata->input_report.reportID;
	static u8 reportStart;
	static u8 fingersPassed;
	static u16 numOfFingers;
	u16 contactCountToSend = 0;
	int i;
	int btn;
	u8 *tmp;
	u16 length = privdata->input_report.length;
	if (length == 0) {/*we get 2 bytes of 0 after reset, ignore*/
		ntrig_dbg("in %s, report length is 0, ignore\n", __func__);
	}
	/*struct timeval tv;
	do_gettimeofday(&tv);
	ntrig_dbg("time: %ld.%ld\n",tv.tv_sec,tv.tv_usec);*/
	switch (reportID) {
	case REPORT_ID_MULTITOUCH:
	{
		struct _ntrig_mt_report *mtReport =
		  (struct _ntrig_mt_report *)&(privdata->input_report.reportID);
		ntrig_dbg("size _ntrig_mt_report %d\n",
					sizeof(struct _ntrig_mt_report));
		ntrig_dbg("size input %d\n", sizeof(privdata->input_report));
		tmp = (u8 *)mtReport;
		/*we expected first fragment
		  report will be with contact count > 0 */
		if (reportStart == 0 && mtReport->contactCount == 0) {
			ntrig_err(
				"ERROR: reportStart==0 && contactCount==0, we"
				" probably missed the first report fragment\n");
			/*for statistics,
				it could be more than 1 but we can't know*/
			i2c_cntrs_list[CNTR_PCKT_LOST].count += 1;
			break;
		}
		/*we expected all fragment reports except the first
			to be with contact count == 0 */
		if (reportStart == 1 && mtReport->contactCount > 0) {
			ntrig_err(
				"ERROR: reportStart == 1 && contactCount > 0,"
				"we lost one or more report fragment(s)\n");
			/*for statistics, it could be more than 1
						but we can't know */
			i2c_cntrs_list[CNTR_PCKT_LOST].count += 1;
			fingersPassed = 0;
		}

		if (reportStart == 0) {/* this is the first report fargment*/
			reportStart = 1;
			checkForLostPackets(mtReport->reportCount,
				&(privdata->expected_mt_counter));
		}

		if (mtReport->contactCount > 0)
			numOfFingers = mtReport->contactCount;

		contactCountToSend =
		(numOfFingers - fingersPassed >= MAX_NUM_OF_FINGERS_HYBRID) ?
		MAX_NUM_OF_FINGERS_HYBRID : numOfFingers - fingersPassed;

		privdata->mr->type = MSG_FINGER_PARSE;
		privdata->mr->msg.fingers_event.sensor_id =
							privdata->sensor_id;
		privdata->mr->msg.fingers_event.frame_index =
							mtReport->reportCount;
		privdata->mr->msg.fingers_event.num_of_fingers =
							contactCountToSend;

		for (i = 0; i < contactCountToSend; i++) {
			privdata->mr->msg.fingers_event.finger_array[i].x_coord
						= mtReport->fingers[i].x[0];
			privdata->mr->msg.fingers_event.finger_array[i].y_coord
						= mtReport->fingers[i].y[0];
			privdata->mr->msg.fingers_event.finger_array[i].dx
						= mtReport->fingers[i].dx;
			privdata->mr->msg.fingers_event.finger_array[i].dy
						= mtReport->fingers[i].dy;
			privdata->mr->msg.fingers_event.finger_array[i].track_id
					= mtReport->fingers[i].fingerIndex;
			privdata->mr->msg.fingers_event.finger_array[i].removed
			   = !(mtReport->fingers[i].fingerStatus & 0x01);
			privdata->mr->msg.fingers_event.finger_array[i].generic
				= !(mtReport->fingers[i].fingerStatus & 0x01);
			privdata->mr->msg.fingers_event.finger_array[i].palm
			  = !((mtReport->fingers[i].fingerStatus & 0x04) >> 2);
		}

		i2c_cntrs_list[CNTR_MULTITOUCH].count++;/*for statistics*/

		/* call the dispatcher to deliver the message */

		fingersPassed += contactCountToSend;

		ntrig_dbg(
			"numOfFingers: %hd, contactCountToSend: "
			"%hd, fingersPassed: %hd\n",
			numOfFingers, contactCountToSend, fingersPassed);

		WriteHIDNTRIG(privdata->mr);

		if (fingersPassed == numOfFingers) {
			reportStart = 0;
			numOfFingers = 0;
			fingersPassed = 0;
		}

		break;
	}
	case REPORT_ID_PEN:
	{
		/* fill in pen report and send to dispatcher */
		struct _ntrig_low_pen_report *pr =
			(struct _ntrig_low_pen_report *)
			&(privdata->input_report.reportID);
		ntrig_dbg(
			"pen report, x=%hd, y=%hd, pressure=%hd,"
			" flags=%02X, battery status=%02X\n",
			pr->x, pr->y, pr->pressure, pr->flags
			, pr->battery_status);
		privdata->mr->type = MSG_PEN_EVENTS;
		privdata->mr->msg.pen_event.sensor_id = privdata->sensor_id;
		privdata->mr->msg.pen_event.x_coord = pr->x;
		privdata->mr->msg.pen_event.y_coord = pr->y;
		privdata->mr->msg.pen_event.pressure = pr->pressure;
		privdata->mr->msg.pen_event.btn_code = pr->flags;
		privdata->mr->msg.pen_event.btn_removed
						= privdata->pen_last_btn;
		privdata->mr->msg.pen_event.battery_status = pr->battery_status;
		privdata->pen_last_btn = btn;
		i2c_cntrs_list[CNTR_PEN].count++;
		/* call the dispatcher to deliver the message */
		WriteHIDNTRIG(privdata->mr);

		break;
	}
	case REPORT_NCP_REPLY:
	{
		u16 size;
		u8 *data;
		u8 flags;
		int avail, i;
		i2c_cntrs_list[CNTR_MAINTREPLAY].count++;
		if (kfifo_size(&privdata->raw_fifo)
						< RAW_RECEIVE_BUFFER_MAX_SIZE) {
			i2c_expand_raw_fifo(privdata);
		}
		/* reply to a raw/ncp message (mostly used for dfu) */
		/* copy the payload (after function) to a circular buffer
		where it can be retrieved later as a fifo.
		   we assume we are inside i2c_lock */
		size = privdata->input_report.length
				- offsetof(struct ntrig_i2c_msg, data) - 1;
		data = privdata->input_report.data;
		ntrig_dbg("%s: received ncp reply, size=%d\n",
							__func__, size);
		if ((size + sizeof(u16)) > kfifo_size(&privdata->raw_fifo)) {
			ntrig_dbg("%s: packet too large to put in buffer"
				"(size=%d, max=%d), discarding\n",
			__func__, size, kfifo_size(&privdata->raw_fifo));
			break;
		}
		/*handle fragmented logical messages - flags
			=number of fragments left
		*/

		for (i = 0; i < size; i++) {
			ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ALL, "%02x\n",
				data[i]);
		}

		flags = (data[0] & ~0x80);/*ignore msb always 1 in G4*/
		data++;/*skip the flags byte*/

		/* logical message fragment*/
		if ((flags > 0) || (privdata->fragments_left > 0)) {
			int message_error = 0;
			if ((privdata->fragments_left > 0)
				&& (privdata->fragments_left != flags + 1)) {
				ntrig_err(
					"%s: logical message fragmentation"
					" corruption - previous number of left"
				     " fragments=%d, current=%d, discarding\n",
				__func__, privdata->fragments_left, flags);
				message_error = 1;
			} else if (privdata->aggregation_size + size
				> MAX_LOGICAL_MESSAGE_SIZE) {
				ntrig_err(
				   "%s: logical message too large to put in"
				   " aggregation buffer (size=%d, max=%d),"
				   " discarding\n",
				   __func__,
				   privdata->aggregation_size + size,
				   MAX_LOGICAL_MESSAGE_SIZE);
				message_error = 1;
			}
			if (message_error) {
				/* discard logical message*/
				privdata->aggregation_size = 0;
				privdata->fragments_left = 0;
				break;
			}
			memcpy(
			&privdata->aggregation_buf[privdata->aggregation_size],
								data, size);
			privdata->aggregation_size += size;
			privdata->fragments_left = flags;
			if (flags > 0) {	/* more fragments to come*/
				ntrig_dbg(
					"%s: fragmented logical message,"
					" waiting for complete message\n",
					__func__);
				break;
			}
			/* last fragment received */
			data = privdata->aggregation_buf;
			size = privdata->aggregation_size;
			privdata->aggregation_size = 0;
		}

		/* Handle heartbeat message:
		 * Unsolicited (type==0x82): discard message and send back an
		 *   ACK (HB request).
		 * Solicited (type==0x81): discard if we're expecting a reply to
		 *   our own request (with "enable HB==0"), otherwise pass to
		 *   user space.
		 */
		if (is_heartbeat(data)) {
			if (is_unsolicited_ncp(data)) {
				ntrig_dbg("%s - unsolicited HB report\n",
					__func__);
				privdata->send_hb_request = 1;
				break;
			} else if (privdata->expecting_hb_reply) {
				ntrig_dbg(
					"%s - discarding solicited HB report\n",
					__func__);
				privdata->expecting_hb_reply = 0;
				break;
			} else
				ntrig_dbg("%s - received solicited HB report\n",
					__func__);
		}

		/* if fifo can't hold our messages,
		discard messages from it until it has room */
		avail = kfifo_avail(&privdata->raw_fifo);
		if (avail < size) {
			ntrig_err(
				"%s: raw receive buffer is full,"
				" discarding messages\n", __func__);
			do {
				i2c_discard_raw_message(privdata);
				avail = kfifo_avail(&privdata->raw_fifo);
			} while (avail < size);
		}
		/* we got here, there is enough room in the buffer,
			insert the message*/
		kfifo_in(&privdata->raw_fifo,
					(unsigned char *)&size, sizeof(u16));
		kfifo_in(&privdata->raw_fifo, data, size);

		/* wake up any threads blocked on the buffer */
		privdata->raw_data_arrived = 1;
		i2c_cntrs_list[CNTR_MAINTREPLAY].count++;
		wake_up(&privdata->raw_wait_queue);
		break;
	}

	case GET_FEATURE_REPLY:
	{
		memcpy(&privdata->get_feature_reply_buf,
			&privdata->input_report,
			privdata->input_report.length);
		break;
	}
	default:
		ntrig_err("Not recognized report ID 0x%02X\n", reportID);
	}
}

/* If needed, send an HB request - which is delayed until the end of the bus
 * transfer and until spi_lock is released.
 */
static void handle_delayed_hb_reply(struct ntrig_i2c_mod_privdata *privdata)
{
	if (privdata->send_hb_request) {
		privdata->send_hb_request = 0;	/* avoid recursive calls */
		if (send_heartbeat_request(privdata))	/* failed to send */
			privdata->send_hb_request = 1;
	}
}

/**
 * interrupt handler, invoked when we have data waiting from
 * sensor
 * Note this function is registered as a threaded irq, so
 * executed in a separate thread and we can sleep here (the I2C
 * transfers, for example, can sleep)
 */
static irqreturn_t i2c_irq(int irq, void *dev_id)
{
	struct ntrig_i2c_mod_privdata *privdata = dev_id;
	ntrig_dbg("in %s\n", __func__);

	/* repeat, until there is no more data */
	do {
		ntrig_dbg("%s: gpio line is low (active interrupt)\n",
								__func__);
		ntrig_dbg("in %s, before down\n", __func__);
		/*critical section: i2c transfer*/
		down(&privdata->i2c_lock);
		ntrig_dbg("in %s, after down\n", __func__);

		if (0 == ntrig_i2c_fast_read(privdata->client,
			(u8 *)&privdata->input_report, I2C_RX_BUF_SIZE)) {
			i2c_process_message(privdata);
		}

		/*critial section end*/
		up(&privdata->i2c_lock);
		ntrig_dbg("in %s, after up\n", __func__);
	} while (gpio_get_value(privdata->irq_gpio) == 0);
	/*while the interrupt line is low*/
	handle_delayed_hb_reply(privdata);

	ntrig_dbg("%s endded\n", __func__);
	return IRQ_HANDLED;
}

/**
 * registers the device to the dispatcher driver
 */
static int register_to_dispatcher(struct i2c_client *client)
{
	struct ntrig_i2c_mod_privdata *privdata =
		(struct ntrig_i2c_mod_privdata *)dev_get_drvdata(&client->dev);
	struct _ntrig_dev_ncp_func ncp_func;
	int ret;

	struct _ntrig_bus_device *nd;

	if (DTRG_NO_ERROR != allocate_device(&privdata->ntrig_dispatcher)) {
		dev_err(&client->dev, "cannot allocate N-Trig dispatcher\n");
		return DTRG_FAILED;
	}

	/* register device in the dispatcher*/
	ncp_func.dev = client;
	ncp_func.read = i2c_read_raw;
	ncp_func.write = i2c_write_raw;
	ncp_func.read_counters = i2c_get_counters;
	ncp_func.reset_counters = i2c_reset_counters;

	privdata->sensor_id = RegNtrigDispatcher(TYPE_BUS_I2C, "i2c",
		&ncp_func);
	if (privdata->sensor_id == DTRG_FAILED) {
		ntrig_dbg("%s: Cannot register device to dispatcher\n",
								__func__);
		return DTRG_FAILED;
	}

	/* fill some default values for sensor area
	  TODO should be retrieved from sensor, currently	not
	  supported in SPI*/
	nd = privdata->ntrig_dispatcher;
	nd->logical_min_x = 0;
	nd->logical_max_x = 9600;
	nd->logical_min_y = 0;
	nd->logical_max_y = 7200;
	nd->pressure_min = 1;
	nd->pressure_max = 255;
	nd->is_touch_set = 1;
	nd->touch_width = 2;
#ifndef MT_REPORT_TYPE_B
	create_single_touch(nd, privdata->sensor_id);
#endif
	create_multi_touch(nd, privdata->sensor_id);

	ntrig_dbg("End of %s\n", __func__);
	return DTRG_NO_ERROR;
}


/************** sysfs files ***********************/
void printBuf(u8 *buf, int count, char *name)
{
	int i;
	printk(KERN_DEBUG "%s:\n", name);
	for (i = 0; i < count-1; i++)
		printk(KERN_DEBUG "0x%02X, ", (u8)buf[i]);
	printk(KERN_DEBUG "0x%02X\n", (u8)buf[i]);
}
unsigned int g_length = 500;
static ssize_t length_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	memcpy(buf, &g_length, sizeof(int));
	return sizeof(int);
}

static ssize_t length_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int i;
	ntrig_dbg("in %s, count is %d\n", __func__, count);
	for (i = 0; i < count; i++)
		ntrig_dbg("%02x\n", buf[i]);
	g_length = *((int *)buf);
	ntrig_dbg("in %s store length: %d\n", __func__, g_length);
	return count;
}

static ssize_t address_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	memcpy(buf, &g_i2c_client->addr, sizeof(short));
	return sizeof(short);
}

static ssize_t address_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int i;
	ntrig_dbg("in %s, count is %d\n", __func__, count);
	for (i = 0; i < count; i++)
		ntrig_dbg("%02x\n", buf[i]);
	g_i2c_client->addr = *((short *)buf);
	ntrig_dbg("in %s store address: %hd\n",
					__func__, g_i2c_client->addr);
	return count;
}

static ssize_t hid_register_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	short count = 30;
	int ret;
	ntrig_dbg("in %s, before read\n", __func__);
	ret = ntrig_i2c_read(g_i2c_client,
				(short)HID_DESC_REGISTER, (u8 *)buf, count);
	ntrig_dbg("in %s, after read\n", __func__);
	printBuf(buf, count, "I2C HID register");

	return count;
}

static ssize_t report_register_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	short count = g_length;
	int ret;
	ntrig_dbg("in %s, length is %d\n", __func__, count);
	ret = ntrig_i2c_read(g_i2c_client,
				(short)REPORT_DESC_REGISTER, (u8 *)buf, count);
	printBuf(buf, count, "I2C report register");

	return count;
}

static ssize_t input_register_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	short count = I2C_RX_BUF_SIZE;
	int ret;
	ret = ntrig_i2c_fast_read(g_i2c_client, (u8 *)buf, count);
	printBuf(buf, count, "I2C input register");

	return count;
}

static ssize_t command_register_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	short count = 2;
	int ret;
	ret = ntrig_i2c_read(g_i2c_client,
				(short)COMMAND_REGISTER, (u8 *)buf, count);
	printBuf(buf, count, "I2C command register");

	return count;
}

static ssize_t command_register_store(struct kobject *kobj,
						struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int i;
	short ret;
	u8 tx[I2C_TX_BUF_SIZE];
	short commandRegister = COMMAND_REGISTER;
	memcpy(tx, &commandRegister, sizeof(short));
	memcpy(tx + sizeof(short), buf, count);
	ntrig_dbg("in %s\n", __func__);
	for (i = 0; i < count + sizeof(short); i++)
		ntrig_dbg("%02x\n", tx[i]);
	ret = ntrig_i2c_write(g_i2c_client, tx, count + sizeof(short));
	if (ret != 0)
		ntrig_err("in %s, ret is %d\n", __func__, ret);
	return count;
}

static ssize_t data_register_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	short count = I2C_RX_BUF_SIZE;
	int ret;

	ret = ntrig_i2c_read(g_i2c_client,
					(short)DATA_REGISTER, (u8 *)buf, count);
	printBuf(buf, count, "I2C data register");

	return count;
}

static ssize_t data_register_store(struct kobject *kobj,
						struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret, i;
	u8 tx[I2C_TX_BUF_SIZE];
	short dataRegister = DATA_REGISTER;
	memcpy(tx, &dataRegister, sizeof(short));
	memcpy(tx + sizeof(short), buf, count);
	ntrig_dbg("in %s\n", __func__);
	for (i = 0; i < count + sizeof(short); i++)
		ntrig_dbg("%02x\n", tx[i]);

	ret = ntrig_i2c_write(g_i2c_client, tx, count + sizeof(short));
	if (ret != 0)
		ntrig_err("in %s, ret is %d\n", __func__, ret);
	return count;
}

static ssize_t get_feature_reply_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	u8 *tmp = (u8 *)&g_pdata->get_feature_reply_buf;
	short count = g_pdata->get_feature_reply_buf.length;
	int i;
	ntrig_dbg("in %s, count is %d\n", __func__, count);
	for (i = 0; i < count; i++)
		ntrig_dbg("%02x\n", tmp[i]);
	memcpy(buf, tmp, count);
	return count;
}

static ssize_t enable_hb_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	/* data is not written into actual file on file system, but rather
	 * saves it in a memory buffer */
	u8 enable = buf[0] - '0';
	ntrig_dbg("inside %s - enable=%d\n", __func__, enable);
	if (count > 0) {
		set_enable_heartbeat_param(enable);
		send_heartbeat_request(g_pdata);
	}
	return count;
}
static ssize_t enable_hb_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf)
{
	ntrig_dbg("inside %s\n", __func__);
	buf[0] = get_enable_heartbeat_param() + '0'; /* cast to char */
	buf[1] = 0; /* end of string */
	return 2; /* number of written chars */
}

static struct kobj_attribute int_length =
		__ATTR(length, S_IRUGO | S_IWUGO, length_show, length_store);
static struct kobj_attribute int_address =
		__ATTR(address, S_IRUGO | S_IWUGO, address_show, address_store);
static struct kobj_attribute hid_register =
		__ATTR(hid_register, S_IRUGO, hid_register_show, NULL);
static struct kobj_attribute report_register =
		__ATTR(report_register, S_IRUGO , report_register_show, NULL);
static struct kobj_attribute input_register =
		__ATTR(input_register, S_IRUGO , input_register_show, NULL);
static struct kobj_attribute command_register =
		__ATTR(command_register, S_IRUGO | S_IWUGO,
			command_register_show, command_register_store);
static struct kobj_attribute data_register =
		__ATTR(data_register, S_IRUGO | S_IWUGO,
			data_register_show, data_register_store);
static struct kobj_attribute get_feature_reply =
		__ATTR(get_feature, S_IRUGO, get_feature_reply_show, NULL);
static struct kobj_attribute enable_hb =
	__ATTR(enable_hb, S_IRUGO | S_IWUGO, enable_hb_show, enable_hb_store);

static struct attribute *attrs[] = {
		&int_length.attr,
		&int_address.attr,
		&hid_register.attr,
		&report_register.attr,
		&input_register.attr,
		&command_register.attr,
		&data_register.attr,
		&get_feature_reply.attr,
		&enable_hb.attr,
		NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
		.attrs = attrs
};

static struct kobject *dispatcher_kobj;

/********************************* end sysfs files ***************************/

/**
 * enable or disable the output_enable line connected to our
 * sensor.
 * When enabled, sensor can access the SPI bus
 * When disabled, sensor cannot access the SPI bus.
 * Note that if the output_enable line is connected to an
 * inverter, we reverse the values for enable/disable
 * (high=disable, low=enable)
 * Assumes gpio line was prepared before (request, set
 * direction)
 */
static void i2c_set_output_enable(struct ntrig_i2c_mod_privdata *pdata,
								bool enable)
{
	if (pdata->oe_gpio >= 0) {
		int val;

		if (pdata->oe_inverted)
			val = enable ? 0 : 1;
		else
			val = enable ? 1 : 0;

		gpio_set_value(pdata->oe_gpio, val);
	}
}

/**
 * return 1 if output_enable line is in enabled state, 0 if
 * it is in disabled state.
 * If the output_enable line is connected through an inverter,
 * reverse the gpio returned values to take it into account
 */
static bool i2c_get_output_enabled(struct ntrig_i2c_mod_privdata *pdata)
{
	if (pdata->oe_gpio >= 0) {
		int val = gpio_get_value(pdata->oe_gpio);
		if (pdata->oe_inverted)
			return val ? 0 : 1;
		else
			return val ? 1 : 0;
	} else {
		/* no gpio available, assume as if it is always set */
		return 1;
	}
}

/**
 * enable or disable the power line connected to our
 * sensor.
 * When enabled, sensor power is up .
 * When disabled, sensor power is down.
 * Assumes gpio line was prepared before (request, set
 * direction)
 */
static void i2c_set_pwr(struct ntrig_i2c_mod_privdata *pdata, bool enable)
{
	if (pdata->pwr_gpio >= 0) { /* power gpio is present*/
		if (enable) {
			gpio_set_value(pdata->pwr_gpio, 1);
			msleep(500);
			i2c_set_output_enable(pdata, enable);
		} else {
			i2c_set_output_enable(pdata, enable);
			msleep(10);
			gpio_set_value(pdata->pwr_gpio, 0);
		}
	}
}

static int init_i2c_pwr_gpio(struct ntrig_i2c_mod_privdata *pdata)
{
	if (pdata->pwr_gpio >= 0) {/* power gpio is present*/
		/* set the pwr gpio line to turn on the sensor*/
		int pwr_gpio = pdata->pwr_gpio;
		int err = gpio_request(pwr_gpio, "ntrig_i2c_pwr");

		if (err) {
			printk(KERN_DEBUG
				"%s: fail to request gpio for pwr(%d), err=%d\n",
					__func__, pwr_gpio, err);
			/* continue anyway... */
		}
		err = gpio_direction_output(pwr_gpio, 0); /* low */
		if (err) {
			printk(KERN_DEBUG "%s: fail to change pwr\n", __func__);
			return err;
		}
		msleep(50);
		gpio_set_value(pwr_gpio, 1); /* high */
		msleep(50);
	}
	return 0;
}


static int ntrig_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	i2c_set_pwr(g_pdata, false);
	return 0;
}

static int ntrig_i2c_resume(struct i2c_client *client)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	i2c_set_pwr(g_pdata, true);
	return 0;
}

static int __init ntrig_i2c_mod_init(void)
{
	struct ntrig_i2c_mod_privdata *pdata;
	int err, irq_gpio, flags;
	int low, high, gpio_index;

	/* create sysfs files */
	int retval;
	dispatcher_kobj = kobject_create_and_add("ntrig_i2c", NULL);
	if (!dispatcher_kobj) {
		ntrig_dbg("inside %s\n failed to create dispatcher_kobj",
								__func__);
		return -ENOMEM;
	}
	/* Create the files associated with this kobject */
	retval = sysfs_create_group(dispatcher_kobj, &attr_group);
	if (retval) {
		printk(KERN_DEBUG "inside %s\n failed to create sysfs_group",
								__func__);
		kobject_put(dispatcher_kobj);
	}
	/* end sysfs files */

	printk(KERN_DEBUG "in %s\n", __func__);

	struct i2c_device_data *i2c_data = get_ntrig_i2c_device_data();
	g_i2c_client = i2c_data->m_i2c_client;

	struct ntrig_i2c_platform_data *platdata = &i2c_data->m_platform_data;

	if (g_i2c_client == NULL) {
		printk(KERN_DEBUG "inside %s, g_i2c_client is NULL!!!\n ",
			__func__);
		return -1;
	}

	pdata = kzalloc(sizeof(struct ntrig_i2c_mod_privdata), GFP_KERNEL);
	g_pdata = pdata;
	if (pdata == NULL) {
		printk(KERN_DEBUG "%s: no memory to allocate pdata\n",
			__func__);
		return -ENOMEM;
	}

	pdata->client = g_i2c_client;
	pdata->aggregation_size = 0;
	pdata->fragments_left = 0;
	pdata->irq_gpio = irq_to_gpio(g_i2c_client->irq);
	pdata->expected_mt_counter = 0;
	pdata->send_hb_request = 0;
	/* Set the "Enable HB" value in the heartbeat request */
	set_enable_heartbeat_param(DEFAULT_HB_ENABLE);
	sema_init(&pdata->i2c_lock, 1);

	/* init variables related to raw read/write
	 we use the i2c_lock to protect the fifo, no additional lock is needed
	 */
	err = kfifo_alloc(&pdata->raw_fifo, RAW_RECEIVE_BUFFER_INITIAL_SIZE,
								GFP_KERNEL);
	if (err) {
		printk(KERN_DEBUG "%s: fail to alloc raw_fifo, err = %d\n",
							__func__, err);
		return err;
	}
	init_waitqueue_head(&pdata->raw_wait_queue);

	/* register the IRQ GPIO line.*/
	irq_gpio = irq_to_gpio(g_i2c_client->irq);

	err = gpio_request(irq_gpio, "ntrig_i2c_irq");
	if (err) {
		printk(KERN_DEBUG
			"%s: fail to request gpio for irq(%d), err=%d\n",
			__func__, irq_gpio, err);
	}

	flags = IRQF_ONESHOT | IRQF_TRIGGER_LOW;
	/* get the irq */
	printk(KERN_DEBUG "%s: requesting irq %d\n",
		__func__, g_i2c_client->irq);
	err = request_threaded_irq(g_i2c_client->irq, NULL, i2c_irq,\
			flags, DRIVER_NAME, pdata);
	if (err) {
		printk(KERN_DEBUG "request irq  %d failed\n",
			g_i2c_client->irq);
	}
	
	pdata->oe_gpio = platdata->oe_gpio;
	pdata->oe_inverted = platdata->oe_inverted;

	/*set the output_enable gpio line to allow sensor to work*/
	gpio_index = pdata->oe_gpio;
	if (gpio_index >= 0) {
		err = gpio_request(gpio_index, "ntrig_i2c_output_enable");
		if (err) {
			printk(KERN_DEBUG
				"%s: fail to request gpio for output_enable(%d), err=%d\n",
					__func__, gpio_index, err);
			/* continue anyway...*/
		}
		low = pdata->oe_inverted ? 1 : 0;
		high = pdata->oe_inverted ? 0 : 1;
		err = gpio_direction_output(gpio_index, low);
	
		if (err) {
			printk(KERN_DEBUG "%s: fail to change output_enable\n",
				__func__);
			return err;
		}
		msleep(50);
		gpio_set_value(gpio_index, high);
		msleep(50);
	}
       
	pdata->pwr_gpio = platdata->pwr_gpio;
	init_i2c_pwr_gpio(pdata);

#ifdef CONFIG_PM
	ntrig_i2c_register_pwr_mgmt_callbacks(ntrig_i2c_suspend,
		ntrig_i2c_resume);
#endif
	
	dev_set_drvdata(&g_i2c_client->dev, pdata);

	/* register with the dispatcher,
	   this will also create input event files
	*/
	err = register_to_dispatcher(g_i2c_client);
	if (err) {
		printk(KERN_DEBUG
			"%s: fail to register to dispatcher, err = %d\n",
							__func__, err);
		return err;
	}

	i2c_reset_counters();

	printk(KERN_DEBUG "in %s, endded successfully\n", __func__);

	return 0;
}

static void __exit ntrig_i2c_mod_exit(void)
{
	sysfs_remove_group(dispatcher_kobj, &attr_group);
	kobject_put(dispatcher_kobj);
	dispatcher_kobj = NULL;
	free_irq(g_i2c_client->irq, g_pdata);
	gpio_free(irq_to_gpio(g_i2c_client->irq));
	if (g_pdata) {
		/* TODO we use a hard-coded bus id - need to change it in order
			to support multiple sensors connected over SPI bus */
		UnregNtrigDispatcher(g_pdata->ntrig_dispatcher,
				g_pdata->sensor_id, TYPE_BUS_I2C, "i2c");

		kfree(g_pdata);
	}
	printk(KERN_DEBUG "in %s\n", __func__);
}

module_init(ntrig_i2c_mod_init);
module_exit(ntrig_i2c_mod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("N-Trig I2C module driver");

