/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef PABLO_KUNIT_TEST_IXC_H
#define PABLO_KUNIT_TEST_IXC_H

static inline int pkt_ixc_addr8_read8(void *client, u8 addr, u8 *val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_read8(void *client, u16 addr, u8 *val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_read16(void *client, u16 addr, u16 *val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_read8_size(void *client, void *buf, u16 addr, size_t size)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_addr8_write8(void *client, u8 addr, u8 val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_write8(void *client, u16 addr, u8 val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_write8_array(void *client, u16 addr, u8 *val, u32 num)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_write16(void *client, u16 addr, u16 val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_write16_array(void *client, u16 addr, u16 *val, u32 num)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_write16_burst(void *client, u16 addr, u16 *val, u32 num)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_write8_sequential(void *client, u16 addr, u8 *val, u16 num)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_data_read16(void *client, u16 *val)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_data_write16(void *client, u8 val_h, u8 val_l)
{
	/* Do nothing */
	return 0;
}

static inline int pkt_ixc_addr_data_write16(void *client, u8 addr, u8 val_h, u8 val_l)
{
	/* Do nothing */
	return 0;
}

#endif /* PABLO_KUNIT_TEST_IXC_H */
