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

#include <linux/i2c/ntrig_i2c.h>
//#include "ntrig_i2c.h"

#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"
#include "ntrig_low_msg.h"


#define DRIVER_NAME	"ntrig_i2c"

#define NTRIG_I2C_ID

#define I2C_TEST

/** maximum size of an aggregated logical SPI message,
 *  16 (max fragments) * 122 (max message size) */
#define MAX_LOGICAL_MESSAGE_SIZE			1952
/** maximum size of I2C buffer for send/receive: in G1 = 128 bytes **/
#define MAX_I2C_TRANSFER_BUFFER_SIZE_G1		128
/** maximum size of I2C buffer for send/receive: in G2 = 256 bytes **/
#define MAX_I2C_TRANSFER_BUFFER_SIZE_G2		256
/** size (bytes) of	the	circular buffer	we keep	for	raw/ncp
 *  commands */
#define		RAW_RECEIVE_BUFFER_INITIAL_SIZE		4096
#define		RAW_RECEIVE_BUFFER_MAX_SIZE			1024*120

/** the	maximum	time to	wait for a response	when sending
 *  command	to sensor. If sensor does not respond within this
 *  time host assumes it is	either in boot loader or not
 *  working */
/* HZ/2 = 500ms */
#define		MAX_I2C_COMMAND_REPLY_TIMEOUT		HZ/2
/** the maximum time to wait for a reply from raw(ncp/dfu)
 *  command.
 */
/* 1 seconds */
#define		MAX_I2C_RAW_COMMAND_REPLY_TIMEOUT	HZ*1

/** addresses of internal buffers in the SPI to I2C bridge **/
/* main buffer to write to it (I2C->SPI) */
#define I2C_BRIDGE_ADDRESS_WRITE_BUFFER		0x01
/* main buffer to read from it (SPI->I2C) */
#define I2C_BRIDGE_ADDRESS_READ_BUFFER		0x02
/* bridge status register */
#define I2C_BRIDGE_ADDRESS_STATUS_REGISTER	0x03
/* buffer that contains the length of incoming message */
#define I2C_BRIDGE_ADDRESS_LENGTH_BUFFER	0x04
/* bridge hardware ID buffer (version of HW in the bridge) */
#define I2C_BRIDGE_ADDRESS_HWID_BUFFER		0x05
/* bridge control register */
#define I2C_BRIDGE_ADDRESS_CONTROL_REGISTER	0x06
/* bridge spi bandwidth control register */
#define I2C_BRIDGE_ADDRESS_SPI_BANDWIDTH_CONTROL 0x07
/* water mark SPI->I2C control register*/
#define I2C_BRIDGE_ADDRESS_WATER_MARK_CONTROL	0x08

/* the bit in status byte in the bridge that tell if I2C->SPI buffer is ready */
#define WRITE_STATUS_BIT	3

/*The bit in control register that enable SPI port*/
#define ENABLE_SPI_PORT 	3

/** number of bytes from tracer/insight that should be dropped from packet */
#define BULK_DATA			5

/** counters names **/
/* device to host */
#define CNTR_NAME_MULTITOUCH	"channel multitouch"
#define CNTR_NAME_PEN		"channel pen"
#define CNTR_NAME_MAINT_REPLY	"channel maint reply"
#define CNTR_NAME_DEBUG_REPLY	"channel debug reply"

/* host to device */
#define CNTR_NAME_MAINT		"channel maint"
#define CNTR_NAME_DEBUG		"channel debug"

#define NTRIG_I2C_RETRY		5

/****************** for test ***************/
#ifdef I2C_TEST

int gpioValue;
short override_length=-1;
bool wrong_preamble=0;

bool prnt_lst_pckgs=0;
int expected_mt_counter=0;
int expected_pen_counter=0;
int cur_mt_counter=0;
int cur_pen_counter=0;

#endif
/********************* end for test **********/

/************** for sysfs files **************/
static struct i2c_client *tmp_i2c_client;
char *intBuf;
#define INTSIZE 10
int i2c_trns_fail=0;
/******************* end *********************/

/** filter pattern,	for	identifying	a valid	message	after the
 *  preamble (0xFFFFFFFF) */
static char FILTER_PATTERN[] = {0xA5, 0x5A, 0xE7, 0x7E};

struct _ntrig_low_i2c_msg {
	u8 internal_register;
	u8 pattern[4];
	struct _ntrig_low_msg msg;
} __attribute__((packed));

typedef enum _i2c_cntrs_names{
	CNTR_MULTITOUCH = 0,
	CNTR_PEN,
	CNTR_MAINTREPLY,
	CNTR_DEBUGREPLY,
	CNTR_MAINT,
	CNTR_DEBUG,
	CNTR_NUMBER_OF_I2C_CNTRS
}i2c_cntrs_names;

static struct _ntrig_counter i2c_cntrs_list[CNTR_NUMBER_OF_I2C_CNTRS] = 
{
	{.name = CNTR_NAME_MULTITOUCH, .count = 0},
	{.name = CNTR_NAME_PEN, .count = 0},
	{.name = CNTR_NAME_MAINT_REPLY, .count = 0},
	{.name = CNTR_NAME_DEBUG_REPLY, .count = 0},
	{.name = CNTR_NAME_MAINT, .count = 0},
	{.name = CNTR_NAME_DEBUG, .count = 0},
};

void i2c_reset_counters(void)
{
	int i;
	for(i=0; i<CNTR_NUMBER_OF_I2C_CNTRS; ++i)
	{
		i2c_cntrs_list[i].count = 0;
	}
}


/*
 * return the array of struct _ntrig_counter and it's length
 */

int i2c_get_counters(struct _ntrig_counter **counters_list_local,  int *length)
{
	*counters_list_local = i2c_cntrs_list;
	*length = CNTR_NUMBER_OF_I2C_CNTRS;
	return 0;
}

struct ntrig_i2c_privdata {
	
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
	//struct _ntrig_low_msg lmsg;
	/** struct for storing incoming messages from digitizer**/ 
	struct _ntrig_low_i2c_msg lmsg;
	/** semaphore (mutex) for protecting the i2c transfer */
	struct semaphore i2c_lock;
	/** shared structure between this driver and dispatcher TODO
	 *  to be removed in new architecture */
	struct _ntrig_bus_device *ntrig_dispatcher;
	/** message	for	sending	hid	reports	to dispatcher */
	struct mr_message_types_s report;
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
	/** buffer to recieve messages from I2C bus */
	uint8_t rx_buf[MAX_I2C_TRANSFER_BUFFER_SIZE_G2];
	/** buffer to send messages to I2C bus */
	uint8_t tx_buf[MAX_I2C_TRANSFER_BUFFER_SIZE_G2];
	/** length of current tx_buf **/
	u16 tx_buf_length;
	/** buffer for aggregating fragmented messages */
	u8 aggregation_buf[MAX_LOGICAL_MESSAGE_SIZE];
	/** size of aggregated message */
	int aggregation_size;
	/** number of logical message fragments left for complete message */
	int fragments_left;
	/** counter that count i2c_transfer failures, for debugging and statistics **/
	unsigned long i2c_transfer_failure;
};

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
static void i2c_set_output_enable(struct ntrig_i2c_privdata *pdata, bool enable)
{
	if(pdata->oe_gpio >= 0) {
		int val;
	
		if(pdata->oe_inverted) {
			val = enable ? 0 : 1;
		} else {
			val = enable ? 1 : 0;
		}
	
		gpio_set_value(pdata->oe_gpio, val);
	}
}

/**
 * return 1 if output_enable line is in enabled state, 0 if 
 * it is in disabled state. 
 * If the output_enable line is connected through an inverter, 
 * reverse the gpio returned values to take it into account 
 */
static bool i2c_get_output_enabled(struct ntrig_i2c_privdata *pdata)
{
	if(pdata->oe_gpio >= 0) {
		int val = gpio_get_value(pdata->oe_gpio);
		if(pdata->oe_inverted) {
			return val ? 0 : 1;
		} else {
			return val ? 1 : 0;
		}
	} else {
		/* no gpio available, assume as if it is always set */
		return 1;
	}
}

/**
 * build an NCP/DFU command. The command code will determine how to build
 * the SPI header. The command data will be copied after the header 
 * Buffer must be large enough to hold maximum possible raw 
 * command (560 bytes for DFU) 
 */
void build_i2c_ncp_dfu_cmd(u8 cmd, const char* buf, short msg_len, char* out_buf)
{
	int i;
	struct _ntrig_low_i2c_msg* msg = (struct _ntrig_low_i2c_msg*)out_buf;
	char* data;
	u32 sum;
	msg->internal_register = I2C_BRIDGE_ADDRESS_WRITE_BUFFER;
	/* set the filter pattern */
	for(i=0; i<4; i++) {
		msg->pattern[i] = FILTER_PATTERN[i];
	}
	msg->msg.type = LOWMSG_TYPE_COMMAND;
	msg->msg.flags = 0;
	
	/* fill specific requests */
	ntrig_dbg("%s:  request 0x%X\n", __FUNCTION__, cmd);
	switch(cmd) {
	case LOWMSG_REQUEST_DEBUG_AGENT:
		data = &msg->msg.data[0];
		/* ignore BULK_DATA prefix - should not be sent */		
		msg_len -= BULK_DATA;
		buf += BULK_DATA;
		msg->msg.length = msg_len + offsetof(struct _ntrig_low_msg, data);
		msg->msg.channel = LOWMSG_CHANNEL_DEBUG;
		msg->msg.function = LOWMSG_FUNCTION_DEBUG_TYPE_A;
		memcpy(data, buf, msg_len);
		data += msg_len;
		// checksum
		for (i = 0, sum = 0; i < msg->msg.length; i++)
			sum += (u32)((unsigned char *)&msg->msg)[i];
		memcpy(data, &sum, sizeof(sum));
		break;
	/*case SPI_ENABLED_COMMAND:
		// arbitrary SPI command, see header file defining this constant 
		data = &msg->msg.data[0];
		msg->msg.length = msg_len - SPI_ENABLED_COMMAND_HEADER_LENGTH + offsetof(struct _ntrig_low_msg, data);
		msg->msg.channel = buf[1];
		msg->msg.function = buf[2];
		memcpy(data, &buf[SPI_ENABLED_COMMAND_HEADER_LENGTH], msg_len);
		break;*/		
	case LOWMSG_REQUEST_NCP_DFU:
		/* a standard NCP/DFU command. Copy buffer as is */
		data = &msg->msg.data[0];
		msg->msg.length = msg_len + offsetof(struct _ntrig_low_msg, data);
		msg->msg.channel = LOWMSG_CHANNEL_MAINT;
		msg->msg.function = LOWMSG_FUNCTION_NCP;
		memcpy(data, buf, msg_len);
		break;
	default:
		ntrig_err("%s: unknown request %d\n", __FUNCTION__, cmd);
	}
}

static int ntrig_i2c_write_message(struct ntrig_i2c_privdata* privdata)
{
	int ret;
	int i;
	struct i2c_client *client = privdata->client;
	u8 buf = I2C_BRIDGE_ADDRESS_STATUS_REGISTER;
	u8 status;
	struct i2c_msg msgs[2] = {
		{ client->addr, client->flags, 1, &buf },//write the internal address of status register
	        { client->addr, client->flags | I2C_M_RD, 1, &status }//erad the status register
	};
	ntrig_dbg("inside %s\n", __FUNCTION__);
	ntrig_dbg("%s is calling i2c_transfer to get status register\n", __FUNCTION__);
	for(i=0;i<NTRIG_I2C_RETRY;++i)
	{
		ret = i2c_transfer(client->adapter, msgs, 2);
		if(ret==2)
		{
			break;
		}
		else
		{
			privdata->i2c_transfer_failure++;
			i2c_trns_fail++;
		}
	}
	if(ret != 2)
	{
		ntrig_err("%s: i2c_transfer failure\n", __FUNCTION__);
		return -1;
	}
	
	ntrig_dbg("in %s,  status register is %hd\n", __FUNCTION__,status);
	
	if((status >> WRITE_STATUS_BIT) & 1) // I2C->SPI buffer is not ready
	{
		ntrig_dbg("in %s,  write status bit is 1, sleeping for 2 ms\n", __FUNCTION__);
		msleep(2);//sleep 2 ms, the time that take the bridge to empty the buffer
		
		ntrig_dbg("%s is calling i2c_transfer to get status register, 2nd try\n", __FUNCTION__);
		for(i=0;i<NTRIG_I2C_RETRY;++i)
		{
			ret = i2c_transfer(client->adapter, msgs, 2);
			if(ret==2)
			{
				break;
			}
			else
			{
				privdata->i2c_transfer_failure++;
				i2c_trns_fail++;
			}
		}
		if(ret != 2)
		{
			ntrig_err("%s: i2c_transfer failure\n", __FUNCTION__);
			return -1;
		}
	
		ntrig_dbg("in %s,  status registeris %hd\n", __FUNCTION__,status);
	
		if((status >> WRITE_STATUS_BIT) & 1) //try again
		{
			ntrig_err("%s: I2C->SPI buffer is not ready\n", __FUNCTION__);
			return -1;
		}
	}
	
	
	ntrig_dbg("in %s,  writing buffer to I2C\n", __FUNCTION__);
	ntrig_dbg("buffer is  %s, in length %d \n",privdata->tx_buf,privdata->tx_buf_length);
	for(i=0;i<NTRIG_I2C_RETRY;++i)
	{
		ret = i2c_master_send(client,privdata->tx_buf,privdata->tx_buf_length);
		if(ret >= 0)
		{
			break;
		}
		else
		{
			privdata->i2c_transfer_failure++;
			i2c_trns_fail++;
		}
	}
	if(ret < 0)
	{
		ntrig_err("%s: i2c master send failure\n", __FUNCTION__);
		return -1;
	}
	ntrig_dbg("in %s, ended successfully\n", __FUNCTION__);
	return 0;
}

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
static int i2c_get_raw_data_from_fifo(struct kfifo* fifo, char* buf, size_t count)
{
	u16 data_len;

	if(kfifo_len(fifo)) {
		/* we have data, can return immediately */
		kfifo_out(fifo, (unsigned char*)&data_len, sizeof(u16));
		ntrig_dbg("%s: raw message size %d, count = %d\n", __FUNCTION__, data_len, count);
		if(data_len > count) {
			data_len = count;
		}
		kfifo_out(fifo, buf, data_len);
	} else {
		data_len = 0;
	}
	ntrig_dbg("in %s, return data len is %hd\n", __FUNCTION__,data_len);
	return data_len;
}

/** 
 * Read NCP msg from RAW device. 
 * On I2C, all responses come from the same device. We separate 
 * them into HID and RAW based on some fields in the message 
 * if success return number of bytes read and fill buffer. If
 * failed return <0 //Allocate at least MAX_SPI_RESPONSE_SIZE 
 * bytes in the buffer 
 * If there is no data in the buffer, it will block until data 
 * is received, or until timeout is reached (500ms). Return -1 
 * if no data arrived 
 * Note: buf is kernel memory 
 */
static int i2c_read_raw(void *dev, char *buf, size_t count)
{
	struct i2c_client* client = dev;
	struct ntrig_i2c_privdata* privdata = (struct ntrig_i2c_privdata*)dev_get_drvdata(&client->dev);
	int err, data_len;
	
	ntrig_dbg("in %s\n", __FUNCTION__);
	// use i2c_lock to protect the circular buffer 
	ntrig_dbg("in %s, before 1st down\n", __FUNCTION__);
	err = down_interruptible(&privdata->i2c_lock);
	
	if(err != 0) {
		// we were interrupted, cancel the request 
		return -ERESTARTSYS;
	}
	ntrig_dbg("in %s, after 1st down\n", __FUNCTION__);
	
	data_len = i2c_get_raw_data_from_fifo(&privdata->raw_fifo, buf, count);
	if(data_len > 0) {
		/* we got data, return immediately */	
		up(&privdata->i2c_lock);
		ntrig_dbg("in %s, after 1st up\n", __FUNCTION__);
		return data_len; 
	}

	/* buffer is empty, we will need to wait. Release the lock before waiting */
	privdata->raw_data_arrived = 0;
	
	up(&privdata->i2c_lock);
	ntrig_dbg("in %s, after 2nd up\n", __FUNCTION__);
	
	err = wait_event_interruptible_timeout(privdata->raw_wait_queue, (privdata->raw_data_arrived != 0), 
										   MAX_I2C_RAW_COMMAND_REPLY_TIMEOUT);
	if(err < 0) {
		/* we were interrupted */
		ntrig_err("in %s, we were interrupted (1st), exit\n", __FUNCTION__);
		return -ERESTARTSYS;
	}


	ntrig_dbg("in %s, before 2nd down\n", __FUNCTION__);
	//get the lock again 
	err = down_interruptible(&privdata->i2c_lock);
	if(err != 0) {
		// we were interrupted, cancel the request 
		ntrig_err("in %s, we were interrupted (2nd), exit\n", __FUNCTION__);
		return -ERESTARTSYS;
	}
	ntrig_dbg("in %s, after 2nd down\n", __FUNCTION__);

	/* read from fifo again, this time return 0 if there is no data (timeout) */
	data_len = i2c_get_raw_data_from_fifo(&privdata->raw_fifo, buf, count);

	up(&privdata->i2c_lock);
	
	ntrig_dbg("in %s, after 3rd up\n", __FUNCTION__);
	
	ntrig_dbg("%s ended succesfully\n", __FUNCTION__);
	ntrig_dbg("in %s, return data len is %d\n", __FUNCTION__,data_len);
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
	struct ntrig_i2c_privdata* privdata = (struct ntrig_i2c_privdata*)dev_get_drvdata(&client->dev);
	u8 request;
	int err, len;
	char intReg[2];
	
	ntrig_dbg("inside %s\n", __FUNCTION__);
	
	if(msg_len <= 0) {
		ntrig_err("%s: empty message\n", __FUNCTION__);
		return 0;
	}

	
	request = *buf;

	ntrig_dbg("%s: request=%d, msg_len=%d\n", __FUNCTION__, request, msg_len);
	
	/*********************************/
	ntrig_dbg("in %s, before down\n", __FUNCTION__);
	//critical section: i2c bus
	err = down_interruptible(&privdata->i2c_lock);
	if(err != 0) {
		// we were interrupted, cancel the request 
		return -ERESTARTSYS;
	}
	ntrig_dbg("in %s, after down\n", __FUNCTION__);
	
	switch(request) {
	//case SPI_ENABLED_COMMAND:
	case LOWMSG_REQUEST_DEBUG_AGENT:
	case LOWMSG_REQUEST_NCP_DFU:
	{
		struct _ntrig_low_i2c_msg* txbuf = (struct _ntrig_low_i2c_msg*)(privdata->tx_buf);
		build_i2c_ncp_dfu_cmd(request, buf, msg_len, (char*)txbuf);
		privdata->tx_buf_length = txbuf->msg.length + 1 + 4;//message length + 1 intenral register +  4 pattern
		switch(txbuf->msg.channel)
		{
		case LOWMSG_CHANNEL_DEBUG : 
			i2c_cntrs_list[CNTR_DEBUG].count++;
			break;
		case LOWMSG_CHANNEL_MAINT :
			i2c_cntrs_list[CNTR_MAINT].count++;
			break;
		}
		len = MAX_I2C_TRANSFER_BUFFER_SIZE_G2;
		/*for(k=0;k<10;++k)
			ntrig_dbg("%s, 0x%X\n",__FUNCTION__,buf[k]);
		 */
		if (request == LOWMSG_REQUEST_NCP_DFU && buf[6]==0x07 && buf[7]==0x01) {
			intReg[0]=I2C_BRIDGE_ADDRESS_CONTROL_REGISTER;
			intReg[1]=0x8A;
			ntrig_dbg("in %s, goto bootloader, disabling MISO\n", __FUNCTION__);
			i2c_master_send(tmp_i2c_client,intReg,2);//disable MISO 
			err = ntrig_i2c_write_message(privdata);
			msleep(1000);
			intReg[0]=I2C_BRIDGE_ADDRESS_CONTROL_REGISTER;
			intReg[1]=0x9A;	
			ntrig_dbg("in %s, goto bootloader, enabling MISO\n", __FUNCTION__);
			i2c_master_send(tmp_i2c_client,intReg,2);//enable MISO
		}
		else
		{
			err = ntrig_i2c_write_message(privdata);
		}
		
		if(err) {
			ntrig_err("%s: i2c write failure %d\n", __FUNCTION__, err);
			goto exit_err;
		}
		/* clear the txbuf so we don't send this command again by mistake */
		memset(txbuf, 0xFF, len);
		break;
	}
	default:
		ntrig_err("%s: unsupported command %d\n", __FUNCTION__, request);
		err = -1;
		goto exit_err;
	}

	
	/* normal finish */
	up(&privdata->i2c_lock);
	ntrig_dbg("in %s, after up\n", __FUNCTION__);
	
	ntrig_dbg("%s ended succesfully\n", __FUNCTION__);
	/* done */
	return msg_len;
exit_err:
	up(&privdata->i2c_lock);
	ntrig_dbg("in %s, after up (exit_err)\n", __FUNCTION__);
	return err;
}

/**
 * helper for managing the raw receive buffer. Discards a 
 * message from the buffer 
 * Must be called with i2c_lock held 
 */
static void i2c_discard_raw_message(struct ntrig_i2c_privdata* privdata)
{
	u16 size;

	kfifo_out(&privdata->raw_fifo, (unsigned char*)&size, sizeof(u16));
	while(size > 0) {
		int getsize = size;
		if(getsize > MAX_I2C_TRANSFER_BUFFER_SIZE_G2) {
			getsize = MAX_I2C_TRANSFER_BUFFER_SIZE_G2;
		}
		kfifo_out(&privdata->raw_fifo, &privdata->scratch_buf[0], getsize);
		size -= getsize;
	}
}

/**
 * called to extend the raw fifo size when the first debug agent reply is
 * received. The debug agent requires a significantly larger fifo.
 */
static void i2c_expand_raw_fifo(struct ntrig_i2c_privdata* privdata)
{
	struct kfifo new_fifo;
	int buf_size, get_size;
	int err = kfifo_alloc(&new_fifo, RAW_RECEIVE_BUFFER_MAX_SIZE, GFP_KERNEL);
	if(err) {
		ntrig_err("%s: fail to allocate a larger raw_fifo, err = %d\n", __FUNCTION__, err);
		return;
	}
	/* copy the contents of the old fifo to the new one */
	buf_size = sizeof(privdata->scratch_buf);
	while ((get_size = kfifo_len(&privdata->raw_fifo)) > 0) {
		if (get_size > buf_size)
			get_size = buf_size;
		kfifo_out(&privdata->raw_fifo, &privdata->scratch_buf[0], get_size);
		kfifo_in(&new_fifo, &privdata->scratch_buf[0], get_size);
	}
	kfifo_free(&privdata->raw_fifo);
	privdata->raw_fifo = new_fifo;
}

/****************************************************************************************/
/**
 * called when we have a complete message received from the I2C 
 * layer 
 */
static void i2c_process_message(struct ntrig_i2c_privdata* privdata)
{
	struct _ntrig_low_msg* msg = &privdata->lmsg.msg;
	struct mr_message_types_s* mr = &privdata->report;
#ifdef I2C_TEST
	int i;
#endif
	ntrig_dbg("%s: message type %d\n", __FUNCTION__, msg->type);
	ntrig_dbg("%s: channel=%d function=%d\n", __FUNCTION__, msg->channel, msg->function);
	//printk("length is %d\n",lll);
	
	if(msg->flags & 0x80) {
		/* bit 7 set in flags means a 256 byte packet arrived, this is a G4 sensor.
		   Switch our SPI message size so next transfers will be more efficient. */
		//privdata->spi_msg_size = SPI_MESSAGE_SIZE_G4;
	}
	switch(msg->channel) {
	case LOWMSG_CHANNEL_MULTITOUCH_TRACKED:
	case LOWMSG_CHANNEL_MULTITOUCH:
		if(msg->function == LOWMSG_FUNCTION_MT_REPORT) {
			/* fill in multi-touch report and send to dispatcher */
			u8 contactCount;
			struct _ntrig_low_mt_report* mtr = (struct _ntrig_low_mt_report*)&msg->data[0];
			struct _ntrig_low_mt_finger* fingers;
			int i;
			/*****       for test ******/
#ifdef I2C_TEST
			if(prnt_lst_pckgs)
			{
				memcpy(&cur_mt_counter,&(msg->data[msg->length-12]),4);
				ntrig_dbg("cur_mt_counter is %d\n",cur_mt_counter);
				ntrig_dbg("expected_mt_counter is %d\n",expected_mt_counter);
				if(cur_mt_counter==expected_mt_counter)
				{
					//printk("report counter OK\n");
					expected_mt_counter++;
				}
				else
				{
					printk("we lost multy touch packet(s) numbers:\n");
					for(i=expected_mt_counter;i<cur_mt_counter;i++)
					{
						printk("%hd\n",i);
					}
					expected_mt_counter=cur_mt_counter+1;
				}
			}
			else
			{
				expected_mt_counter++;
			}
#endif
			/*****      end for test ******/
			if(msg->flags & 0x80) {          
				/* 256 byte report always sends 10 fingers (G4) */
				contactCount = mtr->g4fingers.contactCount;
				fingers = &mtr->g4fingers.fingers[0];
				if(contactCount > MAX_MT_FINGERS_G4) {
					ntrig_err("%s: invalid g4 mt report, too many fingers: %d\n", __FUNCTION__, contactCount);
					return;
				}
			} else {
				/* 128 byte report, 6 fingers (G3.x) */
				contactCount = mtr->g3fingers.contactCount;
				fingers = &mtr->g3fingers.fingers[0];
				if(contactCount > MAX_MT_FINGERS_G3) {
					ntrig_err("%s: invalid g3 mt report, too many fingers: %d\n", __FUNCTION__, contactCount);
					return;
				}
			}
			ntrig_dbg("%s: finger count=%x vendor defined = %u\n", __FUNCTION__, contactCount, fingers[0].vendorDefined);
			mr->type = MSG_FINGER_PARSE;
			mr->msg.fingers_event.sensor_id = privdata->sensor_id;
			mr->msg.fingers_event.frame_index = mtr->reportCount;
			mr->msg.fingers_event.num_of_fingers = contactCount;
			for(i=0; i<contactCount; i++) {
				u32 vendor = fingers[i].vendorDefined;
				
				mr->msg.fingers_event.finger_array[i].x_coord = fingers[i].x;
				mr->msg.fingers_event.finger_array[i].y_coord = fingers[i].y;
				mr->msg.fingers_event.finger_array[i].dx = fingers[i].dx;
				mr->msg.fingers_event.finger_array[i].dy = fingers[i].dy;
				mr->msg.fingers_event.finger_array[i].track_id = fingers[i].fingerIndex;
				mr->msg.fingers_event.finger_array[i].removed = !(fingers[i].flags & 0x01);//tip switch
				mr->msg.fingers_event.finger_array[i].generic = !(fingers[i].flags & 0x01);//same as removed == tip switch
				mr->msg.fingers_event.finger_array[i].palm = (fingers[i].flags & 0x04) >> 2;//touch valid
			}
			i2c_cntrs_list[CNTR_MULTITOUCH].count++;
			/* call the dispatcher to deliver the message */			
			WriteHIDNTRIG(mr);
		} else {
			ntrig_err("%s: invalid mt report, function=%d\n", __FUNCTION__, msg->function);
		}
		break;
	case LOWMSG_CHANNEL_PEN:
		if(msg->function == LOWMSG_FUNCTION_PEN_REPORT) {
			/* fill in pen report and send to dispatcher */
			struct _ntrig_low_pen_report* pr = (struct _ntrig_low_pen_report*)&msg->data[0];
			/***** for test ******/
#ifdef I2C_TEST
			if(prnt_lst_pckgs)
			{
				memcpy(&cur_pen_counter,&(msg->data[msg->length-12]),4);
				ntrig_dbg("cur_pen_counter is %d\n",cur_pen_counter);
				ntrig_dbg("expected_pen_counter is %d\n",expected_pen_counter);
				if(cur_pen_counter==expected_pen_counter)
				{
					//printk("report counter OK\n");
					expected_pen_counter++;
				}
				else
				{
					printk("we lost pen packet(s) numbers:\n");
					for(i=expected_pen_counter;i<cur_pen_counter;i++)
					{
						printk("%hd\n",i);
					}
					expected_pen_counter=cur_pen_counter+1;
				}
			}
			else
			{
				expected_pen_counter++;
			}
#endif
			/*****      end for test ******/
			mr->type = MSG_PEN_EVENTS;
			mr->msg.pen_event.sensor_id = privdata->sensor_id;
			mr->msg.pen_event.x_coord = pr->x;
			mr->msg.pen_event.y_coord = pr->y;
			mr->msg.pen_event.pressure = pr->pressure;
			mr->msg.pen_event.btn_code = pr->flags;
			mr->msg.pen_event.battery_status = pr->battery_status;
			i2c_cntrs_list[CNTR_PEN].count++;
			/* call the dispatcher to deliver the message */
			WriteHIDNTRIG(mr);
		} else {
			ntrig_err("%s: invalid pen report, function=%d\n", __FUNCTION__, msg->function);
		}
		break;
	case LOWMSG_CHANNEL_DEBUG_REPLY:
	{
		/* reply to the debug agent - extend raw fifo if not already extended.
		 * we assume we are inside i2c_lock */
		if (kfifo_size(&privdata->raw_fifo) < RAW_RECEIVE_BUFFER_MAX_SIZE)
		{
			i2c_cntrs_list[CNTR_DEBUGREPLY].count++;
			i2c_expand_raw_fifo(privdata);
		}
	}
	/* fall through */
	case LOWMSG_CHANNEL_MAINT_REPLY:
	{
		/* reply to a raw/ncp message (mostly used for dfu) */
		/* copy the payload (after function) to a circular buffer where it can be retrieved later as a fifo.
		   we assume we are inside i2c_lock */
		u16 size = msg->length - offsetof(struct _ntrig_low_msg, data);
		u8 *data = &msg->data[0];
		int avail;
		ntrig_dbg("%s: received ncp reply, size=%d\n", __FUNCTION__, size);
		if((size + sizeof(u16)) > kfifo_size(&privdata->raw_fifo)) {
			ntrig_err("%s: packet too large to put in buffer (size=%d, max=%d), discarding\n",
				__FUNCTION__, size, kfifo_size(&privdata->raw_fifo));
			break; 
		}
		/* handle fragmented logical messages - flags=number of fragments left */
		if (msg->channel == LOWMSG_CHANNEL_DEBUG_REPLY)
		{
			u8 flags = msg->flags;
			if ((flags > 0) || (privdata->fragments_left > 0))	/* logical message fragment */
			{
				int message_error = 0;
				if ((privdata->fragments_left > 0) && (privdata->fragments_left != flags + 1))
				{
					ntrig_err("%s: logical message fragmentation corruption - previous number of left fragments=%d, current=%d, discarding\n", 
						__FUNCTION__, privdata->fragments_left, flags);
					message_error = 1;
				}
				else if (privdata->aggregation_size + size > MAX_LOGICAL_MESSAGE_SIZE)
				{
					ntrig_err("%s: logical message too large to put in aggregation buffer (size=%d, max=%d), discarding\n", 
						__FUNCTION__, privdata->aggregation_size + size, MAX_LOGICAL_MESSAGE_SIZE);
					message_error = 1;
				}
				if (message_error)
				{
					/* discard logical message */
					privdata->aggregation_size = 0;
					privdata->fragments_left = 0;
					break;
				}
				memcpy(&privdata->aggregation_buf[privdata->aggregation_size], &msg->data[0], size);
				privdata->aggregation_size += size;
				privdata->fragments_left = flags;
				if (flags > 0)	/* more fragments to come */
				{
					ntrig_dbg("%s: fragmented logical message, waiting for complete message\n", __FUNCTION__);
					break;
				}
				/* last fragment received */
				data = privdata->aggregation_buf;
				size = privdata->aggregation_size;
				privdata->aggregation_size = 0;
			}
		}
		/* if fifo can't hold our messages, discard messages from it until it has room */
		avail = kfifo_avail(&privdata->raw_fifo);
		if(avail < size) {
			ntrig_err("%s: raw receive buffer is full, discarding messages\n", __FUNCTION__);
			do {
				i2c_discard_raw_message(privdata);
				avail = kfifo_avail(&privdata->raw_fifo);
			} while(avail < size);
		}
		/* we got here, there is enough room in the buffer, insert the message */
		kfifo_in(&privdata->raw_fifo, (unsigned char*)&size, sizeof(u16));
		kfifo_in(&privdata->raw_fifo, data, size);

		/* wake up any threads blocked on the buffer */
		privdata->raw_data_arrived = 1;
		i2c_cntrs_list[CNTR_MAINTREPLY].count++;
		wake_up(&privdata->raw_wait_queue);
		break;
	}
	}
}

bool isValid(struct _ntrig_low_msg *lmsg)
{
	if(lmsg->length > MAX_I2C_TRANSFER_BUFFER_SIZE_G2 || lmsg->length < 2)
		return false;
	return true;
}

bool isValidPremable(struct _ntrig_low_i2c_msg *lmsg)
{
	int i;
	/***** for test ****/
#ifdef I2C_TEST
	if(wrong_preamble)
	{
		ntrig_dbg("using wrong preamble\n");
		lmsg->pattern[0]=0;
	}
	else
	{
		ntrig_dbg("using real preamble\n");
	}
#endif
	/***** end for test ***/
	for(i=0;i<4;++i)
	{
		ntrig_dbg("lmsg->pattern %d is 0x%X\n",i, lmsg->pattern[i]);
		if(lmsg->pattern[i] != FILTER_PATTERN[i])
			return false;
	}
	return true;
}
/*
// main buffer to write to it (I2C->SPI) 
#define I2C_BRIDGE_ADDRESS_WRITE_BUFFER		0x01
// main buffer to read from it (SPI->I2C)
#define I2C_BRIDGE_ADDRESS_READ_BUFFER		0x02
// bridge status register 
#define I2C_BRIDGE_ADDRESS_STATUS_REGISTER	0x03
// buffer that contains the length of incoming message 
#define I2C_BRIDGE_ADDRESS_LENGTH_BUFFER	0x04
// bridge hardware ID buffer (version of HW in the bridge) 
#define I2C_BRIDGE_ADDRESS_HWID_BUFFER		0x05
// bridge control register 
#define I2C_BRIDGE_ADDRESS_CONTROL_REGISTER	0x06*/
static int ntrig_i2c_read_message(struct ntrig_i2c_privdata* privdata)
{
	int ret;
	int i;
	struct i2c_client *client = privdata->client;
	u8 buf = I2C_BRIDGE_ADDRESS_LENGTH_BUFFER;
	u8 length;
	
	struct i2c_msg msgs[2] = {
		{ client->addr, client->flags, 1, &buf },//write the internal address of buffer where the length is stored
	        { client->addr, client->flags | I2C_M_RD, 1, &length }//read the length
	};
	
	ntrig_dbg("inside %s\n", __FUNCTION__);
	
	for(i=0;i<NTRIG_I2C_RETRY;++i)
	{
		ret = i2c_transfer(client->adapter, msgs, 2);
		if(ret==2)
		{
			break;
		}
		else
		{
			privdata->i2c_transfer_failure++;
			i2c_trns_fail++;
		}
	}
	if(ret != 2)
	{
		ntrig_err("%s: i2c_transfer failure\n", __FUNCTION__);
		return -1;
	}
	ntrig_dbg("in %s, length to read is %hd\n", __FUNCTION__,length);
	
	buf=I2C_BRIDGE_ADDRESS_READ_BUFFER;
	msgs[0].buf=&buf;
	/***** for test *****/
#ifdef I2C_TEST
	if(override_length<0)
	{
		ntrig_dbg("using real length\n");
#endif
		msgs[1].len=length+4;//length + 4 bytes of premable
#ifdef I2C_TEST
	}
	else
	{
		ntrig_dbg("using override length\n");
		msgs[1].len=override_length;
	}
#endif
	/***** end for test *****/
	msgs[1].buf=((char*)&privdata->lmsg)+1;
	for(i=0;i<NTRIG_I2C_RETRY;++i)
	{
		ret = i2c_transfer(client->adapter, msgs, 2);
		if(ret==2)
		{
			break;
		}
		else
		{
			privdata->i2c_transfer_failure++;
			i2c_trns_fail++;
		}
	}
	if(ret != 2)
	{		
		ntrig_err("%s: i2c_transfer failure\n", __FUNCTION__);
		return -1;
	}
	ntrig_dbg("%s ended successfully\n", __FUNCTION__);
	return 0;
}
/*********************************************************************************************/

/**
 * interrupt handler, invoked when we have data waiting from 
 * sensor 
 * Note this function is registered as a threaded irq, so 
 * executed in a separate thread and we can sleep here (the I2C 
 * transfers, for example, can sleep) 
 */
static irqreturn_t i2c_irq(int irq, void *dev_id)
{
	struct ntrig_i2c_privdata* privdata = dev_id;
	ntrig_dbg("in %s\n", __FUNCTION__);
	if(!i2c_get_output_enabled(privdata)) {
		/* output_enable is low, meaning the sensor will not be able to access the I2C bus,
		   no point in trying any I2C transfer, so end here to avoid extra noise on the I2C bus.
		   Wait a bit to avoid loading the CPU too much */
		msleep(100);
		ntrig_dbg("%s: exit, output enabled is false\n", __FUNCTION__);
		return IRQ_HANDLED;
	}
	
	// repeat, until there is no more data 
	 do
	 {	
		ntrig_dbg("%s: gpio line is low (active interrupt)\n", __FUNCTION__);

		
		ntrig_dbg("in %s, before down\n", __FUNCTION__);
		//critical section: i2c transfer 
		down(&privdata->i2c_lock);
		ntrig_dbg("in %s, after down\n", __FUNCTION__);

		if(0 == ntrig_i2c_read_message(privdata))
		{
			ntrig_dbg("i2c read message success\n");
			if(isValidPremable(&privdata->lmsg))
			{
				ntrig_dbg("valid premable, go to proccess data\n");
				i2c_process_message(privdata);
			}
			else
			{
				ntrig_err("%s: I2C not valid premable!\n", __FUNCTION__);
			}
		}
		//critial section end 
		up(&privdata->i2c_lock);
		ntrig_dbg("in %s, after up\n", __FUNCTION__);
	}
	while(gpio_get_value(privdata->irq_gpio)==0);//while the interrupt line is low
	
	ntrig_dbg("%s ended\n", __FUNCTION__);
	return IRQ_HANDLED;
}
				

/**
 * registers the device to the dispatcher driver
 */
static int register_to_dispatcher(struct i2c_client *client)
{
	struct ntrig_i2c_privdata* privdata = (struct ntrig_i2c_privdata*)dev_get_drvdata(&client->dev);
	struct _ntrig_dev_ncp_func ncp_func;
	int ret, flags;
	
	struct _ntrig_bus_device *nd;
	
	if(DTRG_NO_ERROR != allocate_device(&privdata->ntrig_dispatcher)){
		dev_err(&client->dev, "cannot allocate N-Trig dispatcher\n");
		return DTRG_FAILED;
	}
	
	//* register device in the dispatcher. We register twice - once for HID and once for RAW (ncp/bulk)
	ncp_func.dev = client;
	ncp_func.read = i2c_read_raw;
	ncp_func.write = i2c_write_raw;
	ncp_func.read_counters = i2c_get_counters;
	ncp_func.reset_counters = i2c_reset_counters;
	
	privdata->sensor_id = RegNtrigDispatcher(TYPE_BUS_I2C, "i2c", &ncp_func);
	if (privdata->sensor_id == DTRG_FAILED) {
		ntrig_err("%s: Cannot register device to dispatcher\n", __FUNCTION__);
		return DTRG_FAILED;
	}
	
	// register to	receive	interrupts when	sensor has data 
	flags = privdata->irq_flags;
	if(flags == 0) {
		// default flags 
		flags = IRQF_ONESHOT | IRQF_TRIGGER_LOW;
	}
	// get the irq 
	ntrig_dbg("%s: requesting irq %d\n", __FUNCTION__, client->irq);
	ret = request_threaded_irq(client->irq, NULL, i2c_irq,\
	 	flags, DRIVER_NAME, privdata);
	if (ret) {
		dev_err(&client->dev, "%s: request_irq(%d) failed\n",
			__func__, privdata->irq_gpio);
		printk("request irq  %d failed\n",privdata->irq_gpio);
			
		return ret;
	}

	
	// fill some default values for sensor area
	 //  TODO should be retrieved from sensor, currently	not
	 //  supported in SPI 
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
	create_multi_touch (nd, privdata->sensor_id);

	ntrig_dbg("End of %s\n", __FUNCTION__);
	return DTRG_NO_ERROR;
}

/**
 * enable or disable the power line connected to our 
 * sensor. 
 * When enabled, sensor power is up .
 * When disabled, sensor power is down. 
 * Assumes gpio line was prepared before (request, set 
 * direction) 
 */
static void i2c_set_pwr(struct ntrig_i2c_privdata *pdata, bool enable)
{
	if (pdata->pwr_gpio >=0) // power gpio is present
	{
		if (enable)
		{
			gpio_set_value(pdata->pwr_gpio, 1);
			msleep(500);
			i2c_set_output_enable(pdata, enable);
		}
		else
		{
			gpio_set_value(pdata->pwr_gpio, 0);
			i2c_set_output_enable(pdata, enable);
		}
	}
}

static int init_i2c_pwr_gpio(struct ntrig_i2c_privdata *pdata)
{
	if (pdata->pwr_gpio >=0) // power gpio is present
	{
		// set the pwr gpio line to turn on the sensor
		int pwr_gpio = pdata->pwr_gpio;
		int err = gpio_request(pwr_gpio, "ntrig_i2c_pwr");
		if(err) {
			ntrig_err("%s: fail to request gpio for pwr(%d), err=%d\n", __FUNCTION__, pwr_gpio, err);
			/* continue anyway... */
		}	
		err = gpio_direction_output(pwr_gpio, 0); // low
		if(err) {
			ntrig_err("%s: fail to change pwr\n", __FUNCTION__);
			return err;
		}
		msleep(50);
		gpio_set_value(pwr_gpio, 1); // high
		msleep(50);
	}
	return 0;
}


static int ntrig_i2c_probe(struct i2c_client *client, const struct i2c_device_id *dev_id)
{
	struct ntrig_i2c_privdata *pdata;
	int err, low, high, gpio_index;
	int irq_gpio;
	struct ntrig_i2c_platform_data* platdata = client->dev.platform_data;
	printk("in %s\n", __FUNCTION__);
	ntrig_dbg("%s: output_enable gpio is %d\n", __FUNCTION__, platdata->oe_gpio);
	pdata = kzalloc(sizeof(struct ntrig_i2c_privdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(&client->dev, "%s: no memory\n", __func__);
		return -ENOMEM;
	}	
	/*********** initialize variables for sysfs files ********************/

	tmp_i2c_client = client;
	intBuf = kzalloc(INTSIZE+1, GFP_KERNEL);
	if(intBuf == NULL)
	{
		printk(KERN_INFO "in %s:\n, intBuf is null", __FUNCTION__);
		kfree(pdata);
		return -1;
	}
	memset(intBuf,0,INTSIZE+1);

	/************************  end  ************************************/
	pdata->client = client;
	pdata->aggregation_size = 0;
	pdata->fragments_left = 0;
	pdata->oe_gpio = platdata->oe_gpio;
	pdata->oe_inverted = platdata->oe_inverted;
	pdata->irq_gpio = irq_to_gpio(client->irq);
	pdata->irq_flags = platdata->irq_flags;
	//pdata->test_kobj = ntrig_create_spi_test_file();
	
	pdata->aggregation_size = 0;
	pdata->fragments_left = 0;
	pdata->i2c_transfer_failure=0;
	dev_set_drvdata(&client->dev, pdata);
	
	sema_init(&pdata->i2c_lock, 1);
	/*
	we have to check if there are fields in the client structure that we have to set manually
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	err = spi_setup(spi);
	if (err < 0) {
		ntrig_err("%s: spi_setup failure\n", __FUNCTION__);
		return err;
	}
	
	/* init variables relatedto raw read/write
	   we use the i2c_lock to protect the fifo, no additional lock is needed */
	err = kfifo_alloc(&pdata->raw_fifo, RAW_RECEIVE_BUFFER_INITIAL_SIZE, GFP_KERNEL);
	if(err) {
		ntrig_err("%s: fail to alloc raw_fifo, err = %d\n", __FUNCTION__, err);
		return err;
	}
	init_waitqueue_head(&pdata->raw_wait_queue);

	
	//set the output_enable gpio line to allow sensor to work 
	gpio_index = pdata->oe_gpio;
	if(gpio_index >= 0) {
		err = gpio_request(gpio_index, "ntrig_i2c_output_enable");
		if(err) {
			ntrig_err("%s: fail to request gpio for output_enable(%d), err=%d\n", __FUNCTION__, gpio_index, err);
			// continue anyway...
		}
		low = pdata->oe_inverted ? 1 : 0;
		high = pdata->oe_inverted ? 0 : 1;
		err = gpio_direction_output(gpio_index, low);
		if(err) {
			ntrig_err("%s: fail to change output_enable\n", __FUNCTION__);
			return err;
		}
		msleep(50);
		gpio_set_value(gpio_index, high);
		msleep(50);
	}
	
	/* register the IRQ GPIO line. The actual interrupt is requested in register_to_dispatcher */
	irq_gpio = irq_to_gpio(client->irq);
	/*** for test ****/
#ifdef I2C_TEST
	gpioValue=irq_gpio;
	printk("gpio num is %d\n", gpioValue);
#endif
	/*** end for test ****/
	err = gpio_request(irq_gpio, "ntrig_i2c_irq");
	if(err) {
		printk("%s: fail to request gpio for irq(%d), err=%d\n", __FUNCTION__, irq_gpio, err);		
		/* continue anyway... */
	}

	/* register with the dispatcher, this will also create input event files */
	err = register_to_dispatcher(client);
	if(err) {
		printk("%s: fail to register to dispatcher, err = %d\n", __FUNCTION__, err);
		return err;
	}


	pdata->pwr_gpio = platdata->pwr_gpio;
	init_i2c_pwr_gpio(pdata);

	i2c_reset_counters();
	
	/* enable in I2C->SPI bridge SPI, SPI Rx, interrupt to host*/
	/* set packet size in bridge */
	intBuf[0]=I2C_BRIDGE_ADDRESS_CONTROL_REGISTER;
	intBuf[1]=0x98; //for G4	
	//intBuf[1]=0x9A;//for G3.x
	i2c_master_send(tmp_i2c_client,intBuf,2);
	printk("in %s, ended successfully\n", __FUNCTION__);
	/* success */
	return 0;
}

static int ntrig_i2c_remove(struct i2c_client *client)
{
	struct ntrig_i2c_privdata *pdata = dev_get_drvdata(&client->dev);
	if (!pdata)
		return -EINVAL;
	/* TODO we use a hard-coded bus id - need to change it in order to support multiple sensors connected over SPI bus */
	UnregNtrigDispatcher(pdata->ntrig_dispatcher, pdata->sensor_id, TYPE_BUS_I2C, "i2c");	
	
	kfree(pdata);
	
	if(intBuf!=NULL)
	{
		kfree(intBuf);
	}
	
	return 0;
}

static int ntrig_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct ntrig_i2c_privdata* privdata = (struct ntrig_i2c_privdata*)dev_get_drvdata(&client->dev);
	ntrig_dbg("in %s\n", __FUNCTION__);	
	i2c_set_pwr(privdata, false);
	return 0;
}

static int ntrig_i2c_resume(struct i2c_client *client)
{
	struct ntrig_i2c_privdata* privdata = (struct ntrig_i2c_privdata*)dev_get_drvdata(&client->dev);
	ntrig_dbg("in %s\n", __FUNCTION__);
	i2c_set_pwr(privdata, true);
	return 0;
}



static struct i2c_device_id ntrig_i2c_idtable[] = {
	{ DRIVER_NAME, NTRIG_I2C_ID },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ntrig_i2c_idtable);

static struct i2c_driver ntrig_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = ntrig_i2c_idtable,
	.suspend = ntrig_i2c_suspend,
	.resume = ntrig_i2c_resume,
	.probe		= ntrig_i2c_probe,
	.remove		= __devexit_p(ntrig_i2c_remove),	
};

/***************************************** for test ***************************/
#ifdef I2C_TEST
static void print_client_info(struct i2c_client *client)
{
	if(client==NULL)
	{
		printk(KERN_INFO "tmp_i2c_client is NULL!!!\n");
		return;
	}
	printk(KERN_INFO "client is not NULL, OK\n");
	printk(KERN_INFO "client name: %s\n",tmp_i2c_client->name );
	printk(KERN_INFO "client irq %d \n",tmp_i2c_client->irq );
	printk(KERN_INFO "client address %hd \n",tmp_i2c_client->addr);
	printk(KERN_INFO "client flags %hd \n",tmp_i2c_client->flags);
}
#endif
/********************** end for test ***************************************/


/************** sysfs files ***********************/ 
////for write to all store functions use "echo -n"

static ssize_t address_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "got address: %hd:\n", tmp_i2c_client->addr);
	*buf=((char)tmp_i2c_client->addr) - '0';
	return 1;
}
static ssize_t address_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	tmp_i2c_client->addr=((short)*buf) - '0';
	printk(KERN_INFO "address that stored: %hd:\n", tmp_i2c_client->addr);
	return count;
}

static ssize_t ntrig_dummy_i2c_read_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int count = 32;
	//return i2c_master_recv(tmp_i2c_client,buf,count);
	int ret;
	
	u8 int_buf = I2C_BRIDGE_ADDRESS_READ_BUFFER;
	u8 length=0;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, count, buf}//read the buffer
	};
	
	
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "length before %hd \n",length);
	
	
	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge SPI->I2C: 0x%X \n",*buf);
	
	return count;
}


static ssize_t ntrig_dummy_i2c_write_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int i;
	
	
	if(tmp_i2c_client==NULL)
	{
		printk(KERN_INFO "tmp_i2c_client is NULL!!!\n");
		return 0;
	}
	
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "buf: %s\n", buf);
	printk(KERN_INFO "count: %d\n", count);
	
	
	intBuf[0]=I2C_BRIDGE_ADDRESS_WRITE_BUFFER;
	intBuf[1]=0xA5;
	intBuf[2]=0x5A;
	intBuf[3]=0xE7;
	intBuf[4]=0x7E;
	
	memcpy(intBuf+5,buf,count);
	printk(KERN_INFO "internal address: 0x%X\n",intBuf[0]);
	for(i=5;i<count+5;i++)
	{
	//	printk(KERN_INFO "buf[%d]: 0x%X\n",i, buf[i]);
		intBuf[i]=intBuf[i]-'0';
		printk(KERN_INFO "intBuf[%d]: 0x%X\n",i, intBuf[i]);
	}
	
	
	
	//tmp_i2c_client->addr=address;
	i2c_master_send(tmp_i2c_client,intBuf,count+5);
	printk(KERN_INFO "ntrig_dummy_i2c bridge I2C->SPI: 0x%X \n",*intBuf);
	
	return count;
	
}

static ssize_t status_reg_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	//int gpio_show = -1;
	u8 int_buf = I2C_BRIDGE_ADDRESS_STATUS_REGISTER;
	//char a[1]={0xab};
	//memcpy(buf,a,1);
	
	//printk(KERN_INFO "ntrig_dummy_i2c gpio num is %d \n",gpioValue);
	//gpio_show = gpio_get_value(gpioValue);
	//printk(KERN_INFO "ntrig_dummy_i2c gpio value is %d \n",gpio_show);

	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg status: 0x%X \n",*buf);
	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg status: 0x%X \n",*buf);
	
	return 1;
}

static ssize_t status_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	printk(KERN_INFO "status register store not implemented \n");
	return count;
}

static ssize_t length_reg_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	
	u8 int_buf = I2C_BRIDGE_ADDRESS_LENGTH_BUFFER;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	
	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg length: 0x%X \n",*buf);
	
	return 1;
}

static ssize_t length_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	printk(KERN_INFO "length register store not implemented \n");
	return count;
}


static ssize_t hardware_id_reg_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	
	u8 int_buf = I2C_BRIDGE_ADDRESS_HWID_BUFFER;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	
	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg hardware ID: 0x%X \n",*buf);
	
	return 1;

}

static ssize_t hardware_id_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	printk(KERN_INFO "hardware ID register store not implemented \n");
	return count;
}

static ssize_t control_reg_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	
	u8 int_buf = I2C_BRIDGE_ADDRESS_CONTROL_REGISTER;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	
	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg control show: 0x%X \n",*buf);
	
	return 1;
}

//for write to this function use "echo -n"
static ssize_t control_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int i;
	u8 byte = 0;
	u8 tmp[8] = {0};
		

	
	intBuf[0]=I2C_BRIDGE_ADDRESS_CONTROL_REGISTER;
	memcpy(tmp,buf,count);
	for(i=0;i<count;i++)
	{		
		printk(KERN_INFO "tmp 0x%X\n",tmp[i]);
		tmp[i] = tmp[i] - '0';
		printk(KERN_INFO "tmp 0x%X\n",tmp[i]);
	}
	for(i=0;i<count;i++)
	{
		//intBuf[i]=intBuf[i]-'0';
		printk(KERN_INFO "loop %d, buf[%d]: 0x%X\n",i,i,tmp[i]);
		printk(KERN_INFO "loop %d, byte: 0x%X\n",i,byte);
		byte = byte | ((tmp[i] /*- '0'*/) << (count - 1 - i));
	}
	printk(KERN_INFO "byte: 0x%X\n",byte);
	intBuf[1]=byte;
	//tmp_i2c_client->addr=address;
	i2c_master_send(tmp_i2c_client,intBuf,2);
	
	//ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg control store: 0x%X \n",*buf);
	
	return count;
}

static ssize_t spi_bandwidth_control_reg_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	
	u8 int_buf = I2C_BRIDGE_ADDRESS_SPI_BANDWIDTH_CONTROL;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	

	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg spi control show: 0x%X \n",*buf);
	
	return 1;
}

static ssize_t spi_bandwidth_control_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	intBuf[0] = I2C_BRIDGE_ADDRESS_SPI_BANDWIDTH_CONTROL;
	memcpy(intBuf+1,buf,1);
	intBuf[1]=intBuf[1]-'0';
	
	//tmp_i2c_client->addr=address;
	i2c_master_send(tmp_i2c_client,intBuf,count+1);

	printk(KERN_INFO "ntrig_dummy_i2c bridge reg spi control store: 0x%X \n",*buf);
	
	return count;
}

static ssize_t water_mark_control_reg_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	
	u8 int_buf = I2C_BRIDGE_ADDRESS_WATER_MARK_CONTROL;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	

	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg copntrol water mark show: 0x%X \n",*buf);
	
	return 1;
	
}

static ssize_t water_mark_control_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	intBuf[0] = I2C_BRIDGE_ADDRESS_WATER_MARK_CONTROL;
	memcpy(intBuf+1,buf,1);
	intBuf[1]=intBuf[1]-'0';
	
	//tmp_i2c_client->addr=address;
	i2c_master_send(tmp_i2c_client,intBuf,count+1);

	
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg copntrol water mark store: 0x%X \n",*buf);
	
	return count;
}

#ifdef I2C_TEST
static ssize_t over_length_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "override length is : %hd:\n", override_length);
	*buf=((char)override_length) - '0';
	print_client_info(tmp_i2c_client);
	return 1;
}

static ssize_t over_length_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int i;
	int start=0;
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	if(buf[0]=='-')
	{
		start=1;
	}
	override_length = 0;
	for(i=start;i<count;i++)
	{
		override_length=(override_length*10)+(buf[i] - '0');
	}
	if(start==1)
	{
		override_length=override_length*-1;
	}
	printk(KERN_INFO "override length that stored: %hd:\n", override_length);
	return count;
}


static ssize_t wrong_prem_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "wrong_preamble is : %hd:\n", wrong_preamble);
	*buf=((char)wrong_preamble) - '0';
	return 1;
}

static ssize_t wrong_prem_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	wrong_preamble=buf[0]-'0';
	printk(KERN_INFO "wrong_preamble that stored: %hd:\n", wrong_preamble);
	return count;
}
static ssize_t re_register_irq_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	
	int err;
	
	struct ntrig_i2c_privdata* privdata = (struct ntrig_i2c_privdata*)dev_get_drvdata(&tmp_i2c_client->dev);
	int ret, flags;
	int irq_gpio = irq_to_gpio(tmp_i2c_client->irq);
	
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	
	err = gpio_request(irq_gpio, "ntrig_i2c_irq");
	if(err) {
		printk("%s: fail to request gpio for irq(%d), err=%d\n", __FUNCTION__, irq_gpio, err);		
		/* continue anyway... */
	}
	flags = IRQF_ONESHOT | IRQF_TRIGGER_LOW;
	printk("%s: requesting irq %d\n", __FUNCTION__, tmp_i2c_client->irq);
	ret = request_threaded_irq(tmp_i2c_client->irq, NULL, i2c_irq,\
	 	flags, DRIVER_NAME, privdata);
	if (ret) {	
		printk("request irq  %d failed\n",privdata->irq_gpio);
	}
	return 1;
}

static ssize_t re_register_irq_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int irq_gpio = irq_to_gpio(tmp_i2c_client->irq);
	
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	
	gpio_free(irq_gpio);
	//free_irq(tmp_i2c_client->irq,NULL);
	return 1;
}
#endif

static ssize_t db_status_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	int ret;
	
	u8 int_buf = 0x09;
	
	struct i2c_msg msgs[2] = {
		{ tmp_i2c_client->addr, tmp_i2c_client->flags, 1, &int_buf },
	        { tmp_i2c_client->addr, tmp_i2c_client->flags | I2C_M_RD, 1, buf }
	};
	printk("in debug status show\n");
	
	ret = i2c_transfer(tmp_i2c_client->adapter, msgs, 2);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg DB status show: 0x%X \n",*buf);
	printk(KERN_INFO "ntrig_dummy_i2c bridge reg DB status show: 0x%X \n",(buf[0]-'0'));
	
	return 1;
}

#ifdef I2C_TEST
static ssize_t print_lost_packages_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "prnt_lst_pckgs is : %hd:\n", prnt_lst_pckgs);
	*buf=((char)prnt_lst_pckgs) - '0';
	return 1;
}

static ssize_t print_lost_packages_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	prnt_lst_pckgs=buf[0]-'0';
	printk(KERN_INFO "prnt_lst_pckgs that stored: %hd:\n", prnt_lst_pckgs);
	return count;
}
#endif


static ssize_t i2c_trans_fail_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	printk(KERN_INFO "in %s:\n", __FUNCTION__);
	printk(KERN_INFO "i2c_trans_fail is : %d:\n", i2c_trns_fail);
	*buf=((char)i2c_trns_fail) - '0';
	return 1;
}

static struct kobj_attribute to_from_spi = 
	__ATTR(to_from_spi, S_IRUGO | S_IWUGO, ntrig_dummy_i2c_read_show, ntrig_dummy_i2c_write_store);
static struct kobj_attribute status_reg = 
	__ATTR(status_reg, S_IRUGO | S_IWUGO, status_reg_show, status_reg_store);
static struct kobj_attribute length_reg = 
	__ATTR(length_reg, S_IRUGO | S_IWUGO, length_reg_show, length_reg_store);
static struct kobj_attribute hardware_id_reg = 
	__ATTR(hardware_id_reg, S_IRUGO | S_IWUGO, hardware_id_reg_show, hardware_id_reg_store);
static struct kobj_attribute control_reg = 
	__ATTR(control_reg, S_IRUGO | S_IWUGO, control_reg_show, control_reg_store);
static struct kobj_attribute spi_bandwidth_control_reg = 
	__ATTR(spi_bandwidth_control_reg, S_IRUGO | S_IWUGO, spi_bandwidth_control_reg_show, spi_bandwidth_control_reg_store);
static struct kobj_attribute water_mark_control_reg = 
	__ATTR(water_mark_control_reg, S_IRUGO | S_IWUGO, water_mark_control_reg_show, water_mark_control_reg_store);
static struct kobj_attribute slave_address = 
	__ATTR(slave_address, S_IRUGO | S_IWUGO, address_show, address_store);
static struct kobj_attribute db_status = 
	__ATTR(db_status, S_IRUGO, db_status_show, NULL);
static struct kobj_attribute i2c_trans_fail = 
	__ATTR(i2c_trans_fail, S_IRUGO, i2c_trans_fail_show, NULL);
#ifdef I2C_TEST
static struct kobj_attribute over_length = 
	__ATTR(over_length, S_IRUGO | S_IWUGO, over_length_show, over_length_store);
static struct kobj_attribute wrong_prem = 
	__ATTR(wrong_prem, S_IRUGO | S_IWUGO, wrong_prem_show, wrong_prem_store);
static struct kobj_attribute register_irq = 
	__ATTR(register_irq, S_IRUGO | S_IWUGO, re_register_irq_show, re_register_irq_store);
static struct kobj_attribute print_lost_packages = 
	__ATTR(print_lost_packages, S_IRUGO | S_IWUGO, print_lost_packages_show, print_lost_packages_store);
#endif


static struct attribute *attrs[] = {
	&to_from_spi.attr,
	&status_reg.attr,
	&length_reg.attr,
	&hardware_id_reg.attr,
	&control_reg.attr,
	&spi_bandwidth_control_reg.attr,
	&water_mark_control_reg.attr,
	&slave_address.attr,
	&db_status.attr,
	&i2c_trans_fail.attr,
#ifdef I2C_TEST
	&over_length.attr,
	&wrong_prem.attr,
	&register_irq.attr,
	&print_lost_packages.attr,
#endif
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs
};

static struct kobject *dispatcher_kobj;

/********************************* end sysfs files ****************************/


static int __init ntrig_i2c_init(void)
{
	/**************** create sysfs files *******************/
	int retval;
	dispatcher_kobj = kobject_create_and_add("n-trig_i2c", NULL);
	if (!dispatcher_kobj) {
		ntrig_err( "inside %s\n failed to create dispatcher_kobj", __FUNCTION__);
		return -ENOMEM;
	}
	// Create the files associated with this kobject 
	retval = sysfs_create_group(dispatcher_kobj, &attr_group);
	if (retval) {
		ntrig_err( "inside %s\n failed to create sysfs_group", __FUNCTION__);
		kobject_put(dispatcher_kobj);
	}
	/********************* end sysfs files *************************/
	printk("in %s\n", __FUNCTION__);
	
	return i2c_add_driver(&ntrig_i2c_driver);
}

static void __exit ntrig_i2c_exit(void)
{
	sysfs_remove_group(dispatcher_kobj, &attr_group);		
	kobject_put(dispatcher_kobj);
	dispatcher_kobj = NULL;
	i2c_del_driver(&ntrig_i2c_driver);
}

module_init(ntrig_i2c_init);
module_exit(ntrig_i2c_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("N-Trig I2C driver");
