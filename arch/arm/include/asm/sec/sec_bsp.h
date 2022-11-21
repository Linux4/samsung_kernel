/* arch/arm/include/asm/sec/sec_bsp.h
 *
 * Copyright (C) 2014 Samsung Electronics
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

#ifndef __SEC_BSP_H__
#define __SEC_BSP_H__

#ifdef CONFIG_SEC_BSP
extern void sec_boot_stat_add(const char * c);
#else
static inline void sec_boot_stat_add(const char * c) { }
#endif

#endif /* __SEC_BSP_H__ */
