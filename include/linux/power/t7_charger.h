/*
 * t7_charger.h - T7 USB/Adapter Charger Driver
 *
 * Copyright (c) 2012 Marvell Technology Ltd.
 * Yi Zhang<yizhang@marvell.com>
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
 *
 */

#ifndef __T7_CHARGER_H__
#define __T7_CHARGER_H__

struct t7_pdata {
	/* GPIO for control */
	int chg_int;	/* status pin0 */
	int chg_det;	/* status pin1 */
	int chg_en;	/* ENABLE pin */

	char **supplied_to;
	size_t num_supplicants;
};

#endif /* __T7_CHARGER_H__ */
