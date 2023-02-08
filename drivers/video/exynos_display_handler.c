/* exynos_display_handler.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <asm/bug.h>

#include "exynos_display_handler.h"

static DEFINE_MUTEX(display_lock);
static LIST_HEAD(display_chain_list);
static BLOCKING_NOTIFIER_HEAD(exynos_display_notifier_list);

struct display_list {
	struct list_head	list;
	int			level;
	void			*data;
	int			(*func)(void *data, unsigned long event);
};

int register_display_handler(int (*func)(void *data, unsigned long event), int level, void *data)
{
	struct display_list *handler;
	struct display_list *tmp_handler = NULL;
	struct list_head *pos;
	int order;

	handler = kzalloc(sizeof(*handler), GFP_KERNEL);
	if (!handler) {
		pr_err("fail to allocate %s\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&display_lock);

	order = level;

	list_for_each(pos, &display_chain_list) {
		tmp_handler = list_entry(pos, struct display_list, list);
		if (tmp_handler->level > order)
			break;
		else if (tmp_handler->level == order) {
			pr_err("level(%d) is alraedy existed\n", order);
			order++;
		}
	}

	handler->level = order;
	handler->func = func;
	handler->data = data;
	list_add_tail(&handler->list, pos);

	mutex_unlock(&display_lock);

	return 0;
}
EXPORT_SYMBOL(register_display_handler);

void unregister_display_handler(int (*func)(void *data, unsigned long event))
{
	struct display_list *handler;

	mutex_lock(&display_lock);

	list_for_each_entry(handler, &display_chain_list, list) {
		if (handler->func == func) {
			list_del(&handler->list);
			kfree(handler);
			handler = NULL;
			break;
		}
	}

	mutex_unlock(&display_lock);
}
EXPORT_SYMBOL(unregister_display_handler);

#if 0
int register_display_resume(int (*func)(void *data), int level, void *data)
{
	struct display_list *handler;
	struct display_list *tmp_handler = NULL;
	struct list_head *pos;
	int order;

	handler = kzalloc(sizeof(*handler), GFP_KERNEL);
	if (!handler) {
		pr_err("fail to allocate %s\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&display_lock);

	order = level;

	list_for_each(pos, &display_resume_list) {
		tmp_handler = list_entry(pos, struct display_list, list);
		if (tmp_handler->level > order)
			break;
		else if (tmp_handler->level == order) {
			pr_err("level(%d) is alraedy existed\n", order);
			order++;
		}
	}

	handler->level = order;
	handler->func = func;
	handler->data = data;
	list_add_tail(&handler->list, pos);
	mutex_unlock(&display_lock);

	return 0;
}
EXPORT_SYMBOL(register_display_resume);

void unregister_display_resume(int (*func)(void *data))
{
	struct display_list *handler;

	mutex_lock(&display_lock);
	list_for_each_entry(handler, &display_resume_list, list) {
		if (handler->func == func) {
			list_del(&handler->list);
			kfree(handler);
			handler = NULL;
			break;
		}
	}
	mutex_unlock(&display_lock);
}
EXPORT_SYMBOL(unregister_display_resume);
#endif

int register_exynos_display_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&exynos_display_notifier_list, nb);
}
EXPORT_SYMBOL(register_exynos_display_notifier);

int unregister_exynos_display_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&exynos_display_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_exynos_display_notifier);

int exynos_display_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&exynos_display_notifier_list, val, v);
}
EXPORT_SYMBOL(exynos_display_notifier_call_chain);

static int exynos_display_notifier_event(struct notifier_block *this,
	unsigned long event, void *data)
{
	struct display_list *handler;

	switch (event) {
	case FB_BLANK_POWERDOWN:
		mutex_lock(&display_lock);
		list_for_each_entry(handler, &display_chain_list, list) {
			if (handler == NULL)
				return NOTIFY_BAD;

			if (handler->func == NULL)
				return NOTIFY_BAD;

			handler->func(handler->data, event);
		}
		mutex_unlock(&display_lock);
		return NOTIFY_OK;
	case FB_BLANK_UNBLANK:
		mutex_lock(&display_lock);
		list_for_each_entry_reverse(handler, &display_chain_list, list) {
			if (handler == NULL)
				return NOTIFY_BAD;

			if (handler->func == NULL)
				return NOTIFY_BAD;

			handler->func(handler->data, event);
		}
		mutex_unlock(&display_lock);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block exynos_display_notifier = {
	.notifier_call = exynos_display_notifier_event,
};

static void __exit exynos_display_notifier_exit(void)
{
	unregister_exynos_display_notifier(&exynos_display_notifier);

	return;
}

static int __init exynos_display_notifier_init(void)
{
	register_exynos_display_notifier(&exynos_display_notifier);

	return 0;
}

late_initcall(exynos_display_notifier_init);
module_exit(exynos_display_notifier_exit);

