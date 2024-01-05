/*
 * include/linux/flip_cover.h
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _FLIP_COVER_H_
#define _FLIP_COVER_H_

#ifdef CONFIG_SEC_SYSFS
#define SEC_KEY_PATH	"sec_key"
#endif

struct flip_cover_pdata {
	int (*gpio_init)(void);
	bool wakeup;
	u8 active_low;
	u32 gpio;
	u32 code;
	u64 debounce;
	const char *name;
	void *data;
};

struct flip_cover_drvdata {
	struct flip_cover_pdata *pdata;
	struct input_dev *input;
	struct wake_lock wlock;
	struct delayed_work dwork;
	struct device *dev;
	struct mutex lock;
	struct list_head key_list_head;
	bool flip_cover_open;
	bool input_enabled;
	int prev_level;
	void *data;
};

#ifdef CONFIG_SEC_SYSFS
extern struct device *sec_dev_get_by_name(char *name);
#endif

#endif /* _FLIP_COVER_H_ */

