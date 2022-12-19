/* sec_ext.h
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

#ifndef SEC_EXT_H
#define SEC_EXT_H

/*
 * Param op.
 */
#ifdef CONFIG_SEC_PARAM
#define CM_OFFSET				CONFIG_CM_OFFSET
#define CM_OFFSET_LIMIT 8
enum
{
	PARAM_OFF = '0',
	PARAM_ON = '1',
};

extern int sec_set_param(unsigned long offset, char val);
extern int sec_set_param_str(unsigned long offset, const char *val, int size);
#else
#define sec_set_param(a,b)			{-1)
#endif /* CONFIG_SEC_PARAM */

#endif /* CONFIG_SEC_EXT */
