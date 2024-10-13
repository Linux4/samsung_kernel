/* drivers/battery/vbat_check.c
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/battery/vbat_check.h>

static LIST_HEAD(vbat_check_cb_list);
static DEFINE_MUTEX(vbat_check_list_lock);

int register_vbat_check_cb(struct vbat_check *vbat_check)
{
	if (!vbat_check)
		return -EINVAL;

	mutex_lock(&vbat_check_list_lock);
	list_add(&vbat_check->entry, &vbat_check_cb_list);
	mutex_unlock(&vbat_check_list_lock);

	pr_info("%s: %s is registered\n", __func__, vbat_check->name);

	return 0;
}
EXPORT_SYMBOL_GPL(register_vbat_check_cb);

void unregister_vbat_check_cb(const char *name)
{
	struct vbat_check *vbat_check, *tmp_vbat_check;

	mutex_lock(&vbat_check_list_lock);

	list_for_each_entry_safe(vbat_check, tmp_vbat_check,
				&vbat_check_cb_list, entry) {
		if (!strcmp(vbat_check->name, name)) {
			list_del(&vbat_check->entry);
			kfree(vbat_check);
		}
	}

	mutex_unlock(&vbat_check_list_lock);
}
EXPORT_SYMBOL_GPL(unregister_vbat_check_cb);

int vbat_check_cb_chain(char *vbat_check_name)
{
	struct vbat_check *vbat_check;
	int ret;

	mutex_lock(&vbat_check_list_lock);
	list_for_each_entry(vbat_check, &vbat_check_cb_list, entry) {
		if (!strcmp(vbat_check_name, vbat_check->name)) {
			ret = vbat_check->vbat_check_cb();
			goto out_chain;
		}
	}

	ret = EINVAL;

	pr_err("%s: no matched vbat check id found\n", __func__);
out_chain:
	mutex_unlock(&vbat_check_list_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(vbat_check_cb_chain);
