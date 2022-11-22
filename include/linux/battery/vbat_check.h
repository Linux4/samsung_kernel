/* include/linux/battery/vbat_check.h
 *
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VBAT_CHECK_H__
#define __VBAT_CHECK_H__

enum vbat_check_id {
	VBAT_CHECK_ID_DEFAULT = 0,
	VBAT_CHECK_ID_JIGON,
	VBAT_CHECK_ID_PMIC_ADC,
	VBAT_CHECK_ID_CHARGER,
};

struct vbat_check {
	const char *name;
	int id;
	int (*vbat_check_cb)(void);
	struct list_head entry;
};

#ifdef CONFIG_VBAT_CHECK
extern int register_vbat_check_cb(struct vbat_check *vcheck);
extern void unregister_vbat_check_cb(const char *name);
extern int vbat_check_cb_chain(char *vbat_check_name);
#else
int register_vbat_check_cb(struct vbat_check *vcheck) {}
int unregister_vbat_check_cb(const char *name) {}
int vbat_check_cb_chain(char *vbat_check_name) {return 1;}
#endif

#endif
