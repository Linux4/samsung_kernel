/*
 * Battery common header file for Marvell charger/battery driver
 *
 * Copyright (c) 2013 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __MRVL_BATTERY_COMMON_H
#define __MRVL_BATTERY_COMMON_H

inline struct power_supply *get_power_supply_by_name(char *name)
{
	if (!name)
		return NULL;
	else
		return power_supply_get_by_name(name);
}
/*
 * don't report error when charger node doesn't exit,
 * because the charger may be initialized ahead of fuel gauge;
   pr_err("%s: failed to "#function" psy (%s)\n", __func__, (name));	\
 */
#define psy_do_property(name, function, property, value)	\
{	\
	struct power_supply *psy;	\
	int ret;	\
	psy = get_power_supply_by_name((name));	\
	if (!psy) {	\
		value.intval = -EINVAL;	\
	      } else {	\
		      ret = psy->function##_property(psy, (property), &(value)); \
		      if (ret < 0) {	\
				pr_err("%s: failed to "#name" "#function" (%d=>%d)\n", __func__, (property), ret);\
			      value.intval = -EINVAL;	\
		      }	\
	      }	\
}

#endif
