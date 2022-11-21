/* decon_notify.c
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
#include <linux/fb.h>
#include <linux/export.h>
#include <linux/module.h>

#include "decon_notify.h"

static BLOCKING_NOTIFIER_HEAD(decon_notifier_list);

int decon_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&decon_notifier_list, nb);
}
EXPORT_SYMBOL(decon_register_notifier);

int decon_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&decon_notifier_list, nb);
}
EXPORT_SYMBOL(decon_unregister_notifier);

int decon_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&decon_notifier_list, val, v);
}
EXPORT_SYMBOL(decon_notifier_call_chain);

#define EVENT_LIST	\
_X(EVENT_NONE)	\
_X(EVENT_MODE_CHANGE)	\
_X(EVENT_SUSPEND)	\
_X(EVENT_RESUME)	\
_X(EVENT_MODE_DELETE)	\
_X(EVENT_FB_REGISTERED)	\
_X(EVENT_FB_UNREGISTERED)	\
_X(EVENT_GET_CONSOLE_MAP)	\
_X(EVENT_SET_CONSOLE_MAP)	\
_X(EVENT_BLANK)	\
_X(EVENT_NEW_MODELIST)	\
_X(EVENT_MODE_CHANGE_ALL)	\
_X(EVENT_CONBLANK)	\
_X(EVENT_GET_REQ)	\
_X(EVENT_FB_UNBIND)	\
_X(EVENT_REMAP_ALL_CONSOLE)	\
_X(EARLY_EVENT_BLANK)	\
_X(R_EARLY_EVENT_BLANK)	\

#define STATE_LIST	\
_X(BLANK_UNBLANK)	\
_X(BLANK_NORMAL)	\
_X(BLANK_VSYNC_SUSPEND)	\
_X(BLANK_HSYNC_SUSPEND)	\
_X(BLANK_POWERDOWN)	\

#define _X(a)	DECON_##a,
enum {	EVENT_LIST	EVENT_MAX	};
enum {	STATE_LIST	STATE_MAX	};
#undef _X

#define _X(a)	#a,
char *EVENT_NAME[] = { EVENT_LIST };
char *STATE_NAME[] = { STATE_LIST };
#undef _X

static ktime_t decon_ktime;

static int decon_notifier_event_time(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int blank = 0;

	switch (val) {
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	blank = *(int *)evdata->data;

	pr_info("decon: decon_notifier_event: blank_mode: %d, %02lx, - %s, %s, %lld\n", blank, val, STATE_NAME[blank], EVENT_NAME[val], ktime_to_ms(ktime_sub(ktime_get(), decon_ktime)));

	return NOTIFY_DONE;
}

static struct notifier_block decon_time_notifier = {
	.notifier_call = decon_notifier_event_time,
	.priority = -1,
};

static int decon_notifier_event(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int blank = 0;

	switch (val) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (evdata && evdata->info && evdata->info->node)
		return NOTIFY_DONE;

	if (val == FB_EARLY_EVENT_BLANK)
		decon_ktime = ktime_get();

	blank = *(int *)evdata->data;

	if (blank >= STATE_MAX || val >= EVENT_MAX) {
		pr_info("decon: invalid notifier info: %d, %02lx\n", blank, val);
		return NOTIFY_DONE;
	}

	if (val == FB_EARLY_EVENT_BLANK)
		pr_info("decon: decon_notifier_event: blank_mode: %d, %02lx, + %s, %s\n", blank, val, STATE_NAME[blank], EVENT_NAME[val]);
	else if (val == FB_EVENT_BLANK)
		pr_info("decon: decon_notifier_event: blank_mode: %d, %02lx, ~ %s, %s\n", blank, val, STATE_NAME[blank], EVENT_NAME[val]);

	decon_notifier_call_chain(val, v);

	return NOTIFY_DONE;
}

static struct notifier_block decon_fb_notifier = {
	.notifier_call = decon_notifier_event,
};

static void __exit decon_notifier_exit(void)
{
	decon_unregister_notifier(&decon_time_notifier);

	fb_unregister_client(&decon_fb_notifier);
}

static int __init decon_notifier_init(void)
{
	fb_register_client(&decon_fb_notifier);

	decon_register_notifier(&decon_time_notifier);

	return 0;
}

late_initcall(decon_notifier_init);
module_exit(decon_notifier_exit);

