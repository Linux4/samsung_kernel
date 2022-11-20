/*
 * vbus_notifier.c
 * Samsung Mobile Vbus Notifier
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/notifier.h>
#include <linux/sec_class.h>
#include <linux/vbus_notifier.h>

#define SET_VBUS_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
	} while (0)

#define DESTROY_VBUS_NOTIFIER_BLOCK(nb)			\
		SET_VBUS_NOTIFIER_BLOCK(nb, NULL, -1)

static struct vbus_notifier_struct vbus_notifier;
struct blocking_notifier_head vbus_notifier_head =
	BLOCKING_NOTIFIER_INIT(vbus_notifier_head);
static DEFINE_MUTEX(vbus_mutex);

int vbus_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			vbus_notifier_device_t listener)
{
	int ret = 0;

	pr_info("%s: listener=%d register\n", __func__, listener);

	SET_VBUS_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&vbus_notifier_head, nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
				__func__, ret);

	mutex_lock(&vbus_mutex);
	if (vbus_notifier.vbus_type != STATUS_VBUS_UNKNOWN)
		nb->notifier_call(nb, vbus_notifier.cmd, &(vbus_notifier.vbus_type));
	mutex_unlock(&vbus_mutex);

	return ret;
}

int vbus_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&vbus_notifier_head, nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
				__func__, ret);
	DESTROY_VBUS_NOTIFIER_BLOCK(nb);

	return ret;
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_WORK)
static void vbus_notifier_notify_work(struct work_struct *data)
{
	struct vbus_notifier_event_work *event_work = container_of(data,
			struct vbus_notifier_event_work, vbus_work);
	int ret = 0;

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, event_work->cmd,
			event_work->vbus_type);

	ret = blocking_notifier_call_chain(&vbus_notifier_head,
			event_work->cmd, &(event_work->vbus_type));

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	kfree(event_work);
}

static void vbus_notifier_notify_event(void)
{
	struct vbus_notifier_event_work *event_work;

	event_work = kmalloc(sizeof(struct vbus_notifier_event_work),
			GFP_KERNEL);
	if (!event_work)
		return;

	event_work->cmd = vbus_notifier.cmd;
	event_work->vbus_type = vbus_notifier.vbus_type;
	INIT_WORK(&event_work->vbus_work, vbus_notifier_notify_work);
	queue_work(vbus_notifier.vbus_noti_wq, &event_work->vbus_work);
}
#else
static int vbus_notifier_notify(void)
{
	int ret = 0;

	pr_info("%s: CMD=%d, DATA=%d\n", __func__, vbus_notifier.cmd,
			vbus_notifier.vbus_type);

	ret = blocking_notifier_call_chain(&vbus_notifier_head,
			vbus_notifier.cmd, &(vbus_notifier.vbus_type));

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}
#endif

void vbus_notifier_handle(vbus_status_t new_dev)
{
	pr_info("%s: (%d)->(%d)\n", __func__, vbus_notifier.vbus_type, new_dev);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_WORK)
	if (!vbus_notifier.vbus_noti_wq)
		vbus_notifier.vbus_noti_wq =
			create_singlethread_workqueue("vbus_notifier");
#endif

	if (vbus_notifier.vbus_type == new_dev)
		return;

	mutex_lock(&vbus_mutex);
	if (new_dev == STATUS_VBUS_HIGH)
		vbus_notifier.cmd = VBUS_NOTIFY_CMD_RISING;
	else if (new_dev == STATUS_VBUS_LOW)
		vbus_notifier.cmd = VBUS_NOTIFY_CMD_FALLING;

	vbus_notifier.vbus_type = new_dev;

	/* vbus attach broadcast */
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER_WORK)
	vbus_notifier_notify_event();
#else
	vbus_notifier_notify();
#endif
	mutex_unlock(&vbus_mutex);
}

