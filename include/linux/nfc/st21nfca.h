/*
 * Copyright (C) 2011 Stollmann E+V GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define ST21NFCA_MAGIC	0xEA

/*
 * ST21NFCA power control via ioctl
 * ST21NFCA_GET_WAKEUP :  poll gpio-level for Wakeup pin
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(>1): power on with firmware download enabled
 */
#define ST21NFCA_GET_WAKEUP	_IOR(ST21NFCA_MAGIC, 0x01, unsigned int)

struct st21nfca_i2c_platform_data {
	unsigned int irq_gpio;
	unsigned int ena_gpio;
	unsigned int polarity_mode;
};
