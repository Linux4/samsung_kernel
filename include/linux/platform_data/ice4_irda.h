/*
 * Copyright (C) 2013 Samsung Electronics
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
 */

#ifndef _ICE4_IRDA_PLATFORM_DATA_H_
#define _ICE4_IRDA_PLATFORM_DATA_H_

struct ice4_irda_platform_data {
	int gpio_irda_irq;
	int gpio_creset;
	int gpio_fpga_rst_n;
	int gpio_cdone;
};

extern struct class *sec_class;

#endif /* _ICE4_IRDA_PLATFORM_DATA_H_ */
