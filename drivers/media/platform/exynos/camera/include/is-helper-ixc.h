/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef is_helper_ixc_H
#define is_helper_ixc_H

#include <linux/i2c.h>
#include <linux/i3c/device.h>
#include <linux/i3c/master.h>
#if defined(USE_CAMERA_HW_ERROR_DETECT) && defined(USE_CAMERA_ADAPTIVE_MIPI)
#include "is-vender.h"
#endif
//#define IS_VIRTUAL_MODULE

#ifdef IS_VIRTUAL_MODULE
#define ixc_info(fmt, ...) \
	printk(KERN_DEBUG "[^^]"fmt, ##__VA_ARGS__)
#else
#define ixc_info(fmt, ...)
#endif

/* FIXME: make it dynamic parsing */
#define IXC_NEXT  3
#define IXC_BYTE  2
#define IXC_DATA  1
#define IXC_ADDR  0

#define GET_IXC_ADDR16(wbuf, addr)			\
({							\
	wbuf[0] = ((addr) & 0xFF00) >> 8;		\
	wbuf[1] = ((addr) & 0xFF);			\
})

#define SET_I3C_TRANSFER_WRITE(xfers, len, wbuf)	\
({							\
	xfers[0].rnw = IXC_TRANSFER_WRITE;		\
	xfers[0].len = len;				\
	xfers[0].data.out = wbuf;			\
})

#define SET_I3C_TRANSFER_READ(xfers, len, val)		\
({							\
	xfers[1].rnw = IXC_TRANSFER_READ;		\
	xfers[1].len = len;				\
	xfers[1].data.in = val;				\
})

/* setfile I2C write option */
/* use 32bit setfile structure */
#define IXC_MODE_BASE	0xF0000000
#define IXC_MODE_BURST_ADDR	(1 + IXC_MODE_BASE)
#define IXC_MODE_BURST_DATA	(2 + IXC_MODE_BASE)
#define IXC_MODE_DELAY		(3 + IXC_MODE_BASE)

/* use 16bit setfile structure */
#define IXC_MODE_BASE_16BIT	0xF000
#define IXC_MODE_BURST_ADDR_16BIT	(1 + IXC_MODE_BASE_16BIT)
#define IXC_MODE_BURST_DATA_16BIT	(2 + IXC_MODE_BASE_16BIT)
#define IXC_MODE_BURST_DATA_8BIT	(3 + IXC_MODE_BASE_16BIT)
#define IXC_MODE_DELAY_16BIT		(4 + IXC_MODE_BASE_16BIT)

#define IXC_ARGS(...)	__VA_ARGS__
#define IXC_DELAY_US(delay_us)	IXC_ARGS((delay_us)/1000, (delay_us)%1000, IXC_MODE_DELAY_16BIT)
#define IXC_BURST_ADDR(address)	IXC_ARGS(address, 0, IXC_MODE_BURST_ADDR_16BIT)
#define IXC_BURST_DATA(data)	IXC_ARGS(data, 0, IXC_MODE_BURST_DATA_16BIT)
#define IXC_BURST_DATA_8BIT(data)	IXC_ARGS(data, 0, IXC_MODE_BURST_DATA_8BIT)

enum ixc_transfer_type {
	IXC_TRANSFER_WRITE,
	IXC_TRANSFER_READ,
};

enum ixc_transfer_size {
	IXC_WRITE_SIZE = 1,
	IXC_READ_SIZE,
};

struct is_ixc_ops {
	int (*addr8_read8)(void *client, u8 addr, u8 *val);
	int (*read8)(void *client, u16 addr, u8 *val);
	int (*read16)(void *client, u16 addr, u16 *val);
	int (*read8_size)(void *client, void *buf, u16 addr, size_t size);
	int (*addr8_write8)(void *client, u8 addr, u8 val);
	int (*write8)(void *client, u16 addr, u8 val);
	int (*write8_array)(void *client, u16 addr, u8 *val, u32 num);
	int (*write16)(void *client, u16 addr, u16 val);
	int (*write16_array)(void *client, u16 addr, u16 *val, u32 num);
	int (*write16_burst)(void *client, u16 addr, u16 *val, u32 num);
	int (*write8_sequential)(void *client, u16 addr, u8 *val, u16 num);
	int (*data_read16)(void *client, u16 *val);
	int (*data_write16)(void *client, u8 val_high, u8 val_low);
	int (*addr_data_write16)(void *client, u8 addr, u8 val_high, u8 val_low);
};

int is_i2c_transfer(struct i2c_adapter *adapter, struct i2c_msg *msg, u32 size);
struct is_ixc_ops *pablo_get_i2c(void);
struct is_ixc_ops *pablo_get_i3c(void);
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_i2c_check_adapter(struct i2c_client *client);
int pablo_kunit_i2c_fill_msg_addr8_read8(struct i2c_client *client,
	u8 addr, u8 *val, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_read8(struct i2c_client *client,
	u16 addr, u8 *val, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_read16(struct i2c_client *client,
	u16 addr, struct i2c_msg *msg, u8 *wbuf, u8 *rbuf);
int pablo_kunit_i2c_fill_msg_data_read16(struct i2c_client *client,
	struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_read8_size(void *buf, u8 addr, size_t size,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_addr8_write8(struct i2c_client *client,
	u8 addr, u8 val, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_write8(struct i2c_client *client,
	u16 addr, u8 val, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_write8_array(struct i2c_client *client,
	u16 addr, u8 *val, u32 num, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_write16(struct i2c_client *client,
	u16 addr, u16 val, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_data_write16(struct i2c_client *client,
	u8 val_h, u8 val_l, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_write16_array(struct i2c_client *client,
	u16 addr, u16 *arr_val, u32 num, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_write16_burst(struct i2c_client *client,
	u16 addr, u16 *arr_val, u32 num, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i2c_fill_msg_write8_sequential(struct i2c_client *client,
	u16 addr, u8 *val, u32 num, struct i2c_msg *msg, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_addr8_read8(u8 addr, u8 *val,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_read16(u16 addr,
	struct i3c_priv_xfer *xfers, u8 *wbuf, u8 *rbuf);
int pablo_kunit_i3c_fill_msg_data_read16(struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_addr8_write8(u8 addr, u8 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_write8(u16 addr, u8 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_write8_array(u16 addr, u8 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_write16(u16 addr, u16 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_data_write16(u8 val_h, u8 val_l,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_write16_array(u16 addr, u16 *arr_val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_write16_burst(u16 addr, u16 *arr_val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
int pablo_kunit_i3c_fill_msg_write8_sequential(u16 addr, u8 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf);
#endif
#endif
