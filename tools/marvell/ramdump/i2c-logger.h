/*------------------------------------------------------------
(C) Copyright [2006-2008] Marvell International Ltd.
All Rights Reserved
------------------------------------------------------------*/

#ifndef _I2C_LOGGER_H_
#define _I2C_LOGGER_H_

struct mandatory_field {
	u8 type;
	u8 slave_addr;
	u8 slave_reg;
	u8 data;
	u8 len;
	u8 tv_msec1;
	u16 tv_msec2;
};

struct optional_field1 {
	u8 type;
	u8 ibmr;
	u16 isr;
	u32 icr;
};

struct optional_field2 {
	u8 type;
	u8 spare;
	u16 errn;
	u32 entry;
};

struct type {
	u8 value;
	u8 dummy[7];
};

union i2c_log_msg {
	struct type type;
	struct mandatory_field mandatory;
	struct optional_field1 optional_1;
	struct optional_field2 optional_2;
};

enum {
	MANDATORY_MSG = 1,
	OPTIONAL_MSG1,
	OPTIONAL_MSG2,
};

#define CREATE_MANDATORY_TYPE(err, i2c_id, type)	\
				((err << 4) | (i2c_id << 2) | type)
#define CREATE_OPTIONAL1_TYPE(isr)	\
				((((isr >> 16) & 0x0f) << 2) | (OPTIONAL_MSG1))
#define GET_ADAPNR(type)	((type & 0xf) >> 2)
#define GET_ERR(type)		(type >> 4)
#define GET_TYPE(type)		(type & 0x3)
#define GET_TIME(msec1, msec2)	((msec1 << 16) | msec2)
#define GET_ISR(type, isr)	((((u32)type & 0xfc) << 16) | (isr))

#define I2C_LOGGER_MSG_COUNT 200

struct i2c_log_buffer {
	u32 sighex;
	u8 sigtext[8];
	u32 buf_size;
	union i2c_log_msg msg_buf[I2C_LOGGER_MSG_COUNT];
	u16 buf_head;
};

#define MANDATORY_FIELD_FORMAT	\
	"i2c: time_%d, adap_%d, slave_%02x, reg_%02x, %s , data_%02x, len_%d\n"

#define MANDATORY_FIELD_DATA(mandatory)	\
			GET_TIME(mandatory->tv_msec1,\
				 mandatory->tv_msec2),\
			GET_ADAPNR(mandatory->type),\
			(mandatory->slave_addr >> 1),\
			mandatory->slave_reg, \
			(mandatory->slave_addr & 0x1) ? "R" : "W", \
			mandatory->data,\
			mandatory->len

#define OPTIONAL_MSG1_FIELD_FORMAT \
				"\ticr=%08x, isr=%08x, ibmr=%x, "

#define OPTIONAL_MSG1_FIELD_DATA(optional_1) \
				optional_1->icr, \
				GET_ISR(optional_1->type, optional_1->isr), \
				optional_1->ibmr

#define OPTIONAL_MSG2_FIELD_FORMAT \
				"entry/err=%d/%d\n"

#define OPTIONAL_MSG2_FIELD_DATA(optional_2) \
				optional_2->entry, optional_2->errn

enum err_id {
	ERR_NO_ERROR,
	ERR_BUS_ERROR,
	ERR_SPURIOUS_IRQ,
	ERR_NULL_BUFFER,
	ERR_NO_ANSWER,
	ERR_ALD,
	ERR_ALD_BED,
	ERR_ALD_RETRY,
	ERR_ALD_ADDR,
	ERR_ALD_CONT,
	ERR_TIMEOUT,
	ERR_UNKNOWN,
};


struct i2c_err {
	char *str;
	int id;
};

const struct i2c_err i2c_err[] = {
	{"OK", ERR_NO_ERROR},
	{"Bus ERROR i2c_pxa_slave_txempty", ERR_BUS_ERROR},
	{"ERROR: XFER timeout - started but never finished", ERR_TIMEOUT},
	{"wrong ISR_BED & ISR_ALD", ERR_ALD_BED},
	{"INFO but not error\n i2c: Arbitration Loss Detected, go with RETRY", ERR_ALD_RETRY},
	{"ERROR: ADDR Arbitration Loss with MasterSTOP", ERR_ALD_ADDR},
	{"INFO but not error\n i2c: Arbitration Loss Detected, but transaction continued", ERR_ALD_CONT},
	{"Bus ERROR or no answer from slave-device", ERR_NO_ANSWER},
	{"NULL buffer for TX", ERR_NULL_BUFFER},
	{"wrong ISR_ALD", ERR_ALD},
	{"Bus ERROR on Data stage", ERR_BUS_ERROR},
	{"NULL buffer for RX", ERR_NULL_BUFFER},
	{"spurious SLAVE irq (ISR_SAD=1)", ERR_SPURIOUS_IRQ},
	{"spurious SLAVE irq (ISR_SSD=1)", ERR_SPURIOUS_IRQ},
	{"spurious SLAVE irq", ERR_SPURIOUS_IRQ},
	{"spurious irq", ERR_SPURIOUS_IRQ},
	{"Unknown error", ERR_UNKNOWN},
};

#endif