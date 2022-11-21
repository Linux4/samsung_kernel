// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Author Harry.Lee <Harry.Lee@mediatek.com>
 */

#include "mtk_notify.h"
#if defined(CONFIG_SMCDSD_PANEL)
#include "primary_display.h"
#endif

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

int mtk_notifier_callback(struct notifier_block *p, unsigned long event, void *data)
{
	struct mtk_notifier *n = container_of(p, struct mtk_notifier, notifier);

	if (!n) {
		pr_info("%s: mtk_notifier is not initialized\n", __func__);
		return -EINVAL;
	}

	if (event == MTK_FPS_CHANGE) {
		int fps = *((int *)data);

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

#if defined(CONFIG_MTK_VSYNC_PRINT)
static unsigned long mtk_ddp_get_tracing_mark(void)
{
	static unsigned long addr;

	if (addr == 0)
		addr = kallsyms_lookup_name("tracing_mark_write");

	return addr;
}

static void print_trace(const char *tag, int value)
{
	preempt_disable();
	event_trace_printk(mtk_ddp_get_tracing_mark(), "C|%d|%s|%d\n",
		0xFFFF0000, tag, value);
	preempt_enable();
}

static void vsync_tag_start(unsigned int index)
{
	char tag_name[30] = { '\0' };

	if (primary_display_is_video_mode())
		sprintf(tag_name,
			index ? "ExtDispRefresh" : "PrimDispRefresh");
	else
		sprintf(tag_name,
			index ? "ExtDispRefresh" : "PrimDispRefreshTE");
	print_trace(tag_name, 1);
}

static void vsync_tag_end(unsigned int index)
{
	char tag_name[30] = { '\0' };

	if (primary_display_is_video_mode())
		sprintf(tag_name, index ?
			"ExtDispRefresh" : "PrimDispRefresh");
	else
		sprintf(tag_name,
			index ? "ExtDispRefresh" : "PrimDispRefreshTE");
	print_trace(tag_name, 0);
}

void vsync_print_handler(enum DISP_MODULE_ENUM module,
		unsigned int reg_val)
{
	int index = 0;

	switch (module) {
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		if (!primary_display_is_video_mode())
			break;
		index = module - DISP_MODULE_RDMA0;
		/*Always process eof prior to sof*/
		if (reg_val & (1 << 1)) {
			vsync_tag_start(index);
			vsync_tag_end(index);
		}
		break;

	case DISP_MODULE_DSI0:
		if (primary_display_is_video_mode())
			break;

		index = module - DISP_MODULE_DSI0;

		if (reg_val & (1 << 2)) {
			vsync_tag_start(index);
			vsync_tag_end(index);
		}
		break;

	default:
		break;
	}
}
#endif

