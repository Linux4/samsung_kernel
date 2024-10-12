/*
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __SM5504_H__
#define __SM5504_H__

#define MUIC_DEV_NAME_SM5504	"muic-sm5504"

/* SM5504 MUIC  registers */
enum {
	SM5504_MUIC_REG_DEV_ID		= 0x01,
	SM5504_MUIC_REG_CTRL		= 0x02,
	SM5504_MUIC_REG_INT1		= 0x03,
	SM5504_MUIC_REG_INT2		= 0x04,
	SM5504_MUIC_REG_INT_MASK1	= 0x05,
	SM5504_MUIC_REG_INT_MASK2	= 0x06,
	SM5504_MUIC_REG_ADC			= 0x07,
	SM5504_MUIC_REG_DEV_TYPE1	= 0x0A,
	SM5504_MUIC_REG_DEV_TYPE2	= 0x0B,
	SM5504_MUIC_REG_MAN_SW1		= 0x13,
	SM5504_MUIC_REG_MAN_SW2		= 0x14,
	SM5504_MUIC_REG_RESET		= 0x1B,
	SM5504_MUIC_REG_CHG			= 0x24,
	SM5504_MUIC_REG_END,
};

/* CONTROL : REG_CTRL */
#define SM5504_ADC_EN			(1 << 7)
#define SM5504_USBCHDEN			(1 << 6)
#define SM5504_CHGTYP			(1 << 5)
#define SM5504_SWITCH_OPEN		(1 << 4)
#define SM5504_MANUAL_SWITCH	(1 << 2)
#define SM5504_MASK_INT			(1 << 0)
#define SM5504_CTRL_INIT		0xE4

struct sm5504_muic_data {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex 		muic_mutex;
	struct wake_lock	muic_wake_lock;

	int irq_gpio;
	int keyboard_state;
	int adc_rescan_count;
};

#endif /* __SM5504_H__ */
