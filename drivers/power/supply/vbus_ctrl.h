/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020.
 */
/*HS03s for SR-AL5625-01-250 by wenyaqi at 20210426 start*/
#include <linux/kernel.h>

#ifndef __VBUS_CTRL__
#define __VBUS_CTRL__

#define CHG_ALERT_HOT_TEMP  70	//70degC
#define CHG_ALERT_WARM_TEMP 60	//60degC
#define CHG_ALERT_HOT_STATE  1	//>70degC
#define CHG_ALERT_WARM_STATE 2	//<60degC
#define USB_TERMAL_DETECT_TIMER 30000

enum {
	VBUS_CTRL_LOW,	/* disconnect VBUS to GND */
	VBUS_CTRL_HIGH,	/* connect VBUS to GND */
};

struct vbus_ctrl_dev {
	struct device *dev;
	int state;
	int vbus_ctrl_gpio;
	struct pinctrl *pinctrl;
	struct delayed_work vbus_ctrl_state_work;
};

extern int mtktsusb_get_hw_temp(void);

#endif
/*HS03s for SR-AL5625-01-250 by wenyaqi at 20210426 end*/