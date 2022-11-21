/*
 * include/linux/mfd/rt5511/registers.h -- Registers definitions of RT5511
 *
 * Richtek RT5511 Codec driver header file of registers definitions
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#ifndef __MFD_RT5511_CORE_H__
#define __MFD_RT5511_CORE_H__

#include <linux/i2c.h>

#define RT5511_DRV_VER		"1.1.9_A"

enum rt5511_type {
	RT5511 = 0,
};

#define RT5511_NUM_EQ 40

typedef void (*rt5511_poweron_cb)(void *data);

struct rt5511 {
	struct mutex io_lock;
	struct mutex irq_lock;

	enum rt5511_type type;

	int revision;
	int cust_id;

	struct device *dev;
	int (*read_dev)(struct rt5511 *rt5511, u8 reg,
			int bytes, void *dest);
	int (*write_dev)(struct rt5511 *rt5511, u8 reg,
			 int bytes, const void *src);

	void *control_data;

	/* Used over suspend/resume */
	bool suspended;

	int num_supplies;
	struct regulator_bulk_data *supplies;
	void *poweron_cb_priv_data;
	rt5511_poweron_cb poweron_cb;

};

/* Device I/O API */
/* modify reg addr to 8 bits later */
int rt5511_reg_read(struct rt5511 *rt5511, u16 reg, u32 *pval, int size);
int rt5511_reg_write(struct rt5511 *rt5511, u16 reg, u32 val, int size);

int rt5511_set_bits(struct rt5511 *rt5511, u16 reg,
		    u8 mask, u8 val);
int rt5511_bits_on(struct rt5511 *rt5511, u16 reg, u8 mask);
int rt5511_bits_off(struct rt5511 *rt5511, u16 reg, u8 mask);

/* n bytes read write */
int rt5511_bulk_read(struct rt5511 *rt5511, u16 reg,
		     int count, u8 *buf);
int rt5511_bulk_write(struct rt5511 *rt5511, u16 reg,
		      int count, const u8 *buf);

struct i2c_client *rt5511_get_i2c_client(struct rt5511 *rt5511);



void rt5511_set_private_power_on_event(struct rt5511 *rt5511,
				       rt5511_poweron_cb cb,
				       void *priv_data);

#ifdef CONFIG_PM
int rt5511_mfd_do_suspend(struct rt5511 *rt5511);
int rt5511_mfd_do_resume(struct rt5511 *rt5511);
#endif

#endif
