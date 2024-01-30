// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author Harry.Lee <Harry.Lee@mediatek.com>
 */

#include "mtk_drm_drv.h"
#include "mtk_notify.h"

const char *power_mode_name[] = {
	"DISP_OFF",
	"DISP_ON",
	"DISP_DOZE_SUSPEND",
	"DISP_DOZE",
};


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

static int foo_notifier_event(int power_mode)
{

	if (power_mode == DISP_ON || power_mode == DISP_DOZE)	/* ON, DOZE */
		DDPINFO("EARLY: %s\n", __func__, power_mode_name[power_mode]);

	if (power_mode == DISP_OFF || power_mode == DISP_DOZE_SUSPEND)	/* OFF, DOZE_SUSPEND */
		DDPINFO("AFTER: %s\n", __func__, power_mode_name[power_mode]);

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

	if (event == MTK_POWER_MODE_CHANGE) {
		int power_mode = *((unsigned int *)data);

		if (power_mode != n->power_mode) {
			n->power_mode = power_mode;

			switch (power_mode) {
			case DISP_OFF:
				break;
			case DISP_ON:
				foo_notifier_event(power_mode);
				break;
			case DISP_DOZE:
				foo_notifier_event(power_mode);
				break;
			case DISP_DOZE_SUSPEND:
				break;
			}
			DDPINFO("%s : %s!!\n", __func__, power_mode_name[power_mode]);
		}
	}

	if (event == MTK_POWER_MODE_DONE) {
		int power_mode = *((unsigned int *)data);

		switch (power_mode) {
		case DISP_OFF:
			foo_notifier_event(power_mode);
			break;
		case DISP_ON:
			break;
		case DISP_DOZE:
			break;
		case DISP_DOZE_SUSPEND:
			foo_notifier_event(power_mode);
			break;
		}
		DDPINFO("%s: %s DONE\n", __func__, power_mode_name[power_mode]);
	}

	return 0;
}

int mtk_check_powermode(struct drm_atomic_state *state, int mode)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct mtk_crtc_state *old_state, *new_state;
	int i;
	int powerMode = DISP_NONE;
	int pre_powerMode = DISP_NONE;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		if (drm_crtc_index(crtc))
			continue;

		old_state = to_mtk_crtc_state(old_crtc_state);
		new_state = to_mtk_crtc_state(new_crtc_state);

		if (!new_crtc_state->active_changed &&
			old_state->prop_val[CRTC_PROP_DOZE_ACTIVE] ==
			new_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
			continue;

		powerMode = (new_state->prop_val[CRTC_PROP_DOZE_ACTIVE] << 1) |
			new_crtc_state->active;
		pre_powerMode = (new_state->prop_val[CRTC_PROP_DOZE_ACTIVE] << 1) |
			new_crtc_state->active;

		DDPINFO("%s : %s -> %s\n", __func__,
			power_mode_name[pre_powerMode], power_mode_name[powerMode]);

		if (powerMode != DISP_NONE)
			mtk_notifier_call_chain(mode, (void *)&powerMode);
	}

	return powerMode;
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
