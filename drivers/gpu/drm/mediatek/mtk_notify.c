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
