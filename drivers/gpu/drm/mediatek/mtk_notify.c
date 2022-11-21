/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mtk_drm_drv.h"
#include "mtk_notify.h"

static struct class *notify_class;
static BLOCKING_NOTIFIER_HEAD(mtk_notifier_list);

static int create_notify_class(void)
{
	if (!notify_class) {
		notify_class = class_create(THIS_MODULE, "notify");
		if (IS_ERR(notify_class))
			return PTR_ERR(notify_class);
	}
	return 0;
}

int uevent_dev_register(struct mtk_uevent_dev *udev)
{
	int ret;

	if (!notify_class) {
		ret = create_notify_class();

		if (ret == 0)
			pr_info("create_notify_class susesess\n");
		else {
			pr_info("create_notify_class fail\n");
			return 0;
		}
	}
	udev->dev = device_create(notify_class, NULL,
			MKDEV(0, udev->index), NULL, udev->name);

	if (udev->dev) {
		pr_info("device create ok,index:0x%x\n", udev->index);
		ret = 0;
	} else {
		pr_info("device create fail,index:0x%x\n", udev->index);
		ret = -1;
	}

	dev_set_drvdata(udev->dev, udev);
	udev->state = 0;

	return ret;
}

int noti_uevent_user(struct mtk_uevent_dev *udev, int state)
{
	char *envp[3];
	char name_buf[120];
	char state_buf[120];

	if (udev == NULL)
		return -1;

	if (udev->state != state)
		udev->state = state;

	snprintf(name_buf, sizeof(name_buf), "%s", udev->name);
	envp[0] = name_buf;
	snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", udev->state);
	envp[1] = state_buf;
	envp[2] = NULL;
	pr_info("uevent name:%s ,state:%s\n", envp[0], envp[1]);

	kobject_uevent_env(&udev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

int noti_uevent_user_by_drm(struct drm_device *drm, int state)
{
	struct mtk_drm_private *priv;
	struct mtk_uevent_dev *udev;

	priv = drm->dev_private;
	udev = &priv->uevent_data;

	noti_uevent_user(udev, state);

	return 0;
}
int mtk_notifier_callback(struct notifier_block *p, unsigned long event, void *data)
{
	struct mtk_notifier *n = container_of(p, struct mtk_notifier, notifier);

	if (!n) {
		pr_info("%s: mtk_notifier is not initialized\n", __func__);
		return -EINVAL;
	}

	if (event == MTK_FPS_CHANGE) {
		int fps = *((unsigned int *)data);

		if (fps != n->fps) {
			n->fps = fps;
			pr_info("%s: FPS_CHANGED: %d\n", __func__, n->fps);
		} else {
			pr_info("%s: Ignore_FPS_CHANGE: %d\n", __func__, fps);
		}
	}
	return 0;
}

int mtk_notifier_activate(void)
{
	int ret = 0;
	struct mtk_notifier *n;

	n = kzalloc(sizeof(struct mtk_notifier), GFP_KERNEL);
	if (!n)
		return -ENOMEM;

	n->notifier.notifier_call = mtk_notifier_callback;

	ret = mtk_register_client(&n->notifier);
	if (ret)
		pr_info("unable to register mtk_notifier\n");

	return 0;
}

int mtk_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mtk_notifier_list, nb);
}
EXPORT_SYMBOL(mtk_register_client);

int mtk_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mtk_notifier_list, nb);
}
EXPORT_SYMBOL(mtk_unregister_client);

int mtk_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mtk_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(mtk_notifier_call_chain);
