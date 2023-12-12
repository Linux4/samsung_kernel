/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
/*Tab A9 code for AX6739A-1679 by wenghailong at 20230704 start*/
#ifndef _EXTCON_MTK_USB_H_
#define _EXTCON_MTK_USB_H_
#define EARPHONE_PLUGOUT_STATE      0
#define EARPHONE_PLUGIN_STATE       1
/*Tab A9 code for AX6739A-1679 by wenghailong at 20230704 end*/
struct mtk_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;
	struct usb_role_switch *role_sw;
	unsigned int c_role; /* current data role */
	struct workqueue_struct *extcon_wq;
	struct regulator *vbus;
	unsigned int vbus_vol;
	unsigned int vbus_cur;
	bool vbus_on;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	struct delayed_work wq_psy;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc_dev;
	struct notifier_block tcpc_nb;
#endif
	bool bypss_typec_sink;
	/* id gpio */
	struct gpio_desc *id_gpiod;
	int id_irq;
	struct delayed_work wq_detcable;
};

struct usb_role_info {
	struct mtk_extcon_info *extcon;
	struct delayed_work dwork;
	unsigned int d_role; /* desire data role */
};

enum {
	DUAL_PROP_MODE_UFP = 0,
	DUAL_PROP_MODE_DFP,
	DUAL_PROP_MODE_NONE,
};

enum {
	DUAL_PROP_PR_SRC = 0,
	DUAL_PROP_PR_SNK,
	DUAL_PROP_PR_NONE,
};

/*Tab A9 code for AX6739A-1679 by wenghailong at 20230704 start*/
extern int g_tp_detect_typec_flag;
extern bool g_tp_typec_earphone_in;
extern bool g_tp_round_earphone_in;
extern int earphone_notifier_register(struct notifier_block *nb);
extern int earphone_notifier_unregister(struct notifier_block *nb);
extern int earphone_notifier_call_chain(unsigned long val, void *v);
/*Tab A9 code for AX6739A-1679 by wenghailong at 20230704 end*/
#endif    // _EXTCON_MTK_USB_H_