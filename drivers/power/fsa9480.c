/*
 * For TI 6111 musb ic
 *
 * Copyright (C) 2009 Samsung Electronics
 * Wonguk Jeong <wonguk.jeong@samsung.com>
 * Minkyu Kang <mk7.kang@samsung.com>
 *
 * Modified by Sumeet Pawnikar <sumeet.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <plat/microusbic.h>
#if defined(CONFIG_MACH_LT02)
#include <mach/mfp-pxa986-lt02.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#endif

#include <plat/vbus.h>

#include <linux/mfd/88pm80x.h>
//#include <linux/wakelock.h>
#include <plat/pm.h>
#include <linux/pm.h>
#include <mach/fsa9480.h>
#include <mach/irqs.h>

#include <linux/delay.h>
#include <linux/switch.h>

/*
#include "mv/mvUsbDevApi.h"
#include "mv/mvUsbCh9.h"
#include <mach/hardware.h>
#include <plat/vbus.h>
#include <plat/pxausb_comp.h>
#include <plat/pxa3xx_otg.h>
#include <plat/pxa_u2o.h>
#include "mvUsb.h"
*/

static BLOCKING_NOTIFIER_HEAD(usb_switch_notifier);

/* FSA9480 I2C registers */
#define FSA9480_REG_DEVID		0x01
#define FSA9480_REG_CTRL		0x02
#define FSA9480_REG_INT1		0x03
#define FSA9480_REG_INT2		0x04
#define FSA9480_REG_INT1_MASK		0x05
#define FSA9480_REG_INT2_MASK		0x06
#define FSA9480_REG_ADC			0x07
#define FSA9480_REG_TIMING1		0x08
#define FSA9480_REG_TIMING2		0x09
#define FSA9480_REG_DEV_T1		0x0A
#define FSA9480_REG_DEV_T2		0x0B
#define FSA9480_REG_BTN1		0x0C
#define FSA9480_REG_BTN2		0x0D
#define FSA9480_REG_CK			0x0E
#define FSA9480_REG_CK_INT1		0x0F
#define FSA9480_REG_CK_INT2		0x10
#define FSA9480_REG_CK_INTMASK1		0x11
#define FSA9480_REG_CK_INTMASK2		0x12
#define FSA9480_REG_MANSW1		0x13
#define FSA9480_REG_MANSW2		0x14
#define FSA9480_REG_DEV_T3		0x15
#define FSA9480_REG_RESET		0x1B
#define FSA9480_REG_VBUSINVALID		0x1D
#define FSA9480_REG_BCD_TIMER	0x20
#define FSA9480_REG_OCP_SETTING1	0x21
#define FSA9480_REG_OCP_2		0x22
#define FSA9480_REG_RSVDID3		0x3A

/* MANSW1 */
#define VAUDIO		0x90
#define UART		0x6C
#define AUDIO		0x48
#define DHOST		0x24
#define AUTO		0x00

/*TSU6721 MANSW1*/
/* Support LGT_DOCK (KSND) */
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
#define VAAUDIO_TSU6721			0x49
#else
#define VAAUDIO_TSU6721			0x4B
#endif
#define USB_HOST_TSU6721		0x27

/*FSA9485 MANSW1*/
#define VAUDIO_9485		0x93
#define AUDIO_9485		0x4B
#define DHOST_9485		0x27

/* MANSW2 */
#define MANSW2_JIG		(1 << 2)

/* Control */
#define SWITCH_OPEN            (1 << 4)
#define RAW_DATA               (1 << 3)
#define MANUAL_SWITCH          (1 << 2)
#define WAIT                   (1 << 1)
#define INT_MASK               (1 << 0)
#define CTRL_MASK              (SWITCH_OPEN | RAW_DATA | MANUAL_SWITCH | \
                                       WAIT | INT_MASK)
/* Device Type 1*/
#define DEV_USB_OTG		(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG		(1 << 5)
#define DEV_CAR_KIT		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_USB			(1 << 2)
#define DEV_AUDIO_2		(1 << 1)
#define DEV_AUDIO_1		(1 << 0)

#define FSA9480_DEV_T1_HOST_MASK	(DEV_USB_OTG)
#define FSA9480_DEV_T1_USB_MASK		(DEV_USB)
#define FSA9480_DEV_T1_UART_MASK	(DEV_UART)
#define FSA9480_DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_USB_CHG)
#define FSA9480_DEV_T1_AUDIO_MASK	(DEV_AUDIO_1 | DEV_AUDIO_2)
#define FSA9480_DEV_T1_OTG_MASK		(DEV_USB_OTG)
#define FSA9480_DEV_T1_CARKIT_MASK	(DEV_CAR_KIT)

/* Device Type 2*/
#define DEV_RESERVED		(1 << 7)
#define DEV_AV			(1 << 6)
#define DEV_TTY			(1 << 5)
#define DEV_PPD			(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define FSA9480_DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define FSA9480_DEV_T2_UART_MASK	(DEV_JIG_UART_OFF | DEV_JIG_UART_ON)
#define FSA9480_DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
					DEV_JIG_UART_OFF | DEV_JIG_UART_ON)
#define DEV_MHL					(DEV_AV)
#define FSA9480_DEV_T2_MHL_MASK         	(DEV_MHL)
#define FSA9480_DEV_T2_JIG_UARTOFF_MASK		(DEV_JIG_UART_OFF)
#define FSA9480_DEV_T2_JIG_UARTON_MASK		(DEV_JIG_UART_ON)
#define FSA9480_DEV_T2_JIG_USBOFF_MASK		(DEV_JIG_USB_OFF)
#define FSA9480_DEV_T2_JIG_USBON_MASK		(DEV_JIG_USB_ON)
#define FSA9480_DEV_T2_DESKDOCK_MASK		(DEV_AV)

/* Device Type 3 */
#define DEV_U200_CHG				(1 << 6)
#define DEV_APPLE_CHG				(1 << 5)
#define DEV_AV_VBUS				(1 << 4)
#define DEV_DCD_OUT_SDP				(1 << 2)
#define FSA9480_DEV_T3_DESKDOCK_VB_MASK		(DEV_AV_VBUS)
#define FSA9480_DEV_T3_U200CHG_MASK		(DEV_U200_CHG)
#define FSA9480_DEV_T3_NONSTD_SDP_MASK		(DEV_DCD_OUT_SDP)
#define FSA9480_DEV_T3_APPLECHG_MASK		(DEV_APPLE_CHG)

/* Reserved_ID 1 */
#define DEV_VBUSIN				(1 << 1)
#define FSA9480_DEV_RVDID1_VBUSIN_MASK		(DEV_VBUSIN)

/* Interrupt 2 */
#define RESERVED_ATTACH				(1 << 1)

/* Dock detect */
#define DETECT_REASON_NORMAL 0
#define DETECT_REASON_BOOT 1
#define DETECT_REASON_DOCKDETECT 2

//int audio_dock_detect = 0;
struct fsa9480_usbsw {
	struct i2c_client *client;
	struct fsa9480_platform_data *pdata;
	struct work_struct work;
	u8 dev1;
	u8 dev2;
	u8 dev3;
	u8 adc;
	u8 dev_rvd_id;
	int mansw;
	u8 id;
	struct pm_qos_request qos_idle;
	struct switch_dev dock_dev;
	struct mutex mutex;
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	int audio_dock_flag;
	int smart_dock_flag;
	int usb_dock_flag;
	int intr2;
	struct delayed_work	dock_work;
#endif
};

struct ic_vendor {
	u8 id;
	char *part_num;
};

static struct ic_vendor muic_list[] = {
	{0x01, "SM5502"},
	{0x12, "TSU6721"},
};

static struct fsa9480_usbsw *chip;
static struct wakeup_source JIGConnect_suspend_wake;
static struct wakeup_source USB_suspend_wake;


#ifdef CONFIG_VIDEO_MHL_V1
/*for MHL cable insertion*/
static int isMHLconnected = 0;
#endif

static int isProbe = 0;
static int audio_state = 0;

extern struct class *sec_class;
extern int jack_is_detected;

int usb_switch_register_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&usb_switch_notifier, nb);
}

EXPORT_SYMBOL_GPL(usb_switch_register_notify);

/**
 * usb_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * usb_register_notify() must have been previously called for this function
 * to work properly.
 */
int usb_switch_unregister_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&usb_switch_notifier, nb);
}

EXPORT_SYMBOL_GPL(usb_switch_unregister_notify);

static ssize_t adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct fsa9480_usbsw *usbsw = chip;
	u8 adc_value[] = "1C";
	u8 adc_fail = 0;

	if (usbsw->dev2 & FSA9480_DEV_T2_JIG_MASK) {
		printk("adc_show JIG_UART_OFF\n");
		return sprintf(buf, "%s\n", adc_value);
	} else {
		printk("adc_show no detect\n");
		return sprintf(buf, "%d\n", adc_fail);
	}
}

static DEVICE_ATTR(adc, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH /*0665 */ ,
		   adc_show, NULL);

static int fsa9480_write_reg(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;
	u8 buf[2];
	struct i2c_msg msg[1];

	buf[0] = reg;
	buf[1] = data;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	printk("[FSA9480] fsa9480_write_reg   reg[0x%2x] data[0x%2x]\n", buf[0],
	       buf[1]);

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret != 1) {
		printk("\n [FSA9480] i2c Write Failed (ret=%d) \n", ret);
		return -1;
	}
	//printk("[FSA9480] i2c Write success reg[0x%2x] data[0x%2x] ret[%d]\n", buf[0], buf[1], ret);

	return ret;
}

static int fsa9480_read_reg(struct i2c_client *client, u8 reg, u8 * data)
{
	int ret = 0;
	u8 buf[1];
	struct i2c_msg msg[2];

	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		printk("[FSA9480] i2c Read Failed reg:0x%x(ret=%d)\n", reg,
		       ret);
		return -1;
	}
	*data = buf[0];

	printk("[FSA9480] i2c Read success reg:0x%x[0x%x]\n", reg, buf[0]);
	return 0;
}

static void fsa9480_set_vaudio(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;

	u8 mansw1;
	u8 value;
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	u8 intmask2;
#endif

	if (!audio_state) {
		if (usbsw->id == muic_list[1].id) {
			/* TSU6721 Chip Default : 365K --> Audio ON */
			fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);
			fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			fsa9480_read_reg(client, FSA9480_REG_INT2_MASK, &intmask2);
			if (jack_is_detected) {
				mansw1 &= AUTO;
				value |= MANUAL_SWITCH;
			}
			else {
				mansw1 |= VAAUDIO_TSU6721;
				value &= ~MANUAL_SWITCH;
			}
			intmask2 |= (1 << 2);
#else
			mansw1 &= AUTO;
			value |= MANUAL_SWITCH;
#endif

			fsa9480_write_reg(client, FSA9480_REG_MANSW1, mansw1);
			fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, intmask2);
#endif
		} else {
			/* SM5502 */
			fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);
			fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);

			mansw1 |= VAAUDIO_TSU6721;
			value &= ~MANUAL_SWITCH;

			fsa9480_write_reg(client, FSA9480_REG_MANSW1, mansw1);
			fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
		}
		audio_state = 1;
	}
}

static void fsa9480_disable_vaudio(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;

	u8 mansw1;
	u8 value;

	if (audio_state) {
		if (usbsw->id == muic_list[1].id) {
			/* TSU6721 Chip Default : 365K --> Audio ON
			 * So, Manually Open the Switch
			*/
			fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);
			fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			if (jack_is_detected) {
				mansw1 = 0x01;
				value &= ~MANUAL_SWITCH;
			}
			else {
				mansw1 &= AUTO;
				value |= MANUAL_SWITCH;
			}
#else
			mansw1 = 0x01;
			value &= ~MANUAL_SWITCH;
#endif
			fsa9480_write_reg(client, FSA9480_REG_MANSW1, mansw1);
			fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
		} else {
			/* SM5502 */
			fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);
			fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);

			mansw1 &= AUTO;
			value |= MANUAL_SWITCH;

			fsa9480_write_reg(client, FSA9480_REG_MANSW1, mansw1);
			fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
		}
		audio_state = 0;
	}
}

void fsa9480_dock_audiopath_ctrl(int on)
{
	if( on ){
		fsa9480_set_vaudio();
	} else {
		fsa9480_disable_vaudio();
	}
}
EXPORT_SYMBOL_GPL(fsa9480_dock_audiopath_ctrl);


void fsa9480_set_usbhost(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 mansw1;
	u8 value;
	fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);
	fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);
	mansw1 |= USB_HOST_TSU6721;
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	value &= ~(MANUAL_SWITCH | RAW_DATA);
#else
	value &= ~MANUAL_SWITCH;
#endif
	fsa9480_write_reg(client, FSA9480_REG_MANSW1, mansw1);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
	switch_set_state(&usbsw->dock_dev, 1);
}

void fsa9480_disable_usbhost(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 mansw1;
	u8 value;
	fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);
	fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);
	mansw1 &= AUTO;
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	value |= (MANUAL_SWITCH | RAW_DATA);
#else
	value |= MANUAL_SWITCH;
#endif
	fsa9480_write_reg(client, FSA9480_REG_MANSW1, mansw1);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
	switch_set_state(&usbsw->dock_dev, 0);
}

static void fsa9480_read_adc_value(void)
{
	u8 adc = 0;
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;

	fsa9480_read_reg(client, FSA9480_REG_ADC, &adc);

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	if (adc == 0x1A || adc == 0x11){
		usbsw->audio_dock_flag = 1;
	}
	else if (adc == 0x10 || adc == 0x12){
		usbsw->usb_dock_flag = 1;
	}
#endif
	printk("[FSA9480] %s: adc is 0x%x\n", __func__, adc);
}

#ifdef CONFIG_VIDEO_MHL_V1
static void DisableFSA9480Interrupts(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	printk("DisableFSA9480Interrupts-2\n");

	fsa9480_write_reg(client, FSA9480_REG_INT1_MASK, 0xFF);
	fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0x1F);
}

static void EnableFSA9480Interrupts(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 intr, intr2;

	printk("EnableFSA9480Interrupts\n");

	/*clear interrupts */
	fsa9480_read_reg(client, FSA9480_REG_INT1, &intr);
	fsa9480_read_reg(client, FSA9480_REG_INT2, &intr2);

	fsa9480_write_reg(client, FSA9480_REG_INT1_MASK, 0x00);
	fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0x00);
}
#endif

static void fsa9480_id_open(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;

	pr_alert("fsa9480 id_open\n");
	fsa9480_write_reg(client, FSA9480_REG_RESET, 0x01);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, 0x1E);
}

void fsa9480_set_switch(const char *buf)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 value = 0;
	unsigned int path = 0;

	fsa9480_read_reg(client, FSA9480_REG_CTRL, &value);

	if (!strncmp(buf, "VAUDIO", 6)) {
		if (usbsw->id == 0)
			path = VAUDIO_9485;
		else
			path = VAUDIO;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "UART", 4)) {
		path = UART;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "AUDIO", 5)) {
		if (usbsw->id == 0)
			path = AUDIO_9485;
		else
			path = AUDIO;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "DHOST", 5)) {
		path = DHOST;
		value &= ~MANUAL_SWITCH;
	} else if (!strncmp(buf, "AUTO", 4)) {
		path = AUTO;
		value |= MANUAL_SWITCH;
	} else {
		printk(KERN_ERR "Wrong command\n");
		return;
	}

	usbsw->mansw = path;
	fsa9480_write_reg(client, FSA9480_REG_MANSW1, path);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, value);
}

EXPORT_SYMBOL_GPL(fsa9480_set_switch);

void fsa9480_chgpump_ctrl(int enable)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 ctrl_reg;

	if (usbsw->id != muic_list[0].id)
		return;

	/* If enabled is true
	 * Control Register(0x02) : RAW DATA(BIT3) = 0
	 * Reserved_ID Register(0x3A) : CHGPUMP_nEN(BIT0) = 0
	 * If enable is false
	 * Control Register(0x02) : RAW DATA(BIT3) = 1
	 * Reserved_ID Register(0x3A) : CHGPUMP_nEN(BIT0) = 1
	 */
	fsa9480_read_reg(client, FSA9480_REG_CTRL, &ctrl_reg);

	enable ? (ctrl_reg &= ~RAW_DATA) : (ctrl_reg |= RAW_DATA);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, ctrl_reg);

	if( enable ){
		fsa9480_write_reg(client, FSA9480_REG_RSVDID3, 0x00);
		printk(KERN_INFO "FSA9480 CHG Pump Enabled\n");
	} else {
		fsa9480_write_reg(client, FSA9480_REG_RSVDID3, 0x01);
		printk(KERN_INFO "FSA9480 CHG Pump Disabled\n");
	}


}
EXPORT_SYMBOL_GPL(fsa9480_chgpump_ctrl);

ssize_t fsa9480_get_switch(char *buf)
{
	struct fsa9480_usbsw *usbsw = chip;
	struct i2c_client *client = usbsw->client;
	u8 value;

	fsa9480_read_reg(client, FSA9480_REG_MANSW1, &value);

	if (value == VAUDIO)
		return sprintf(buf, "VAUDIO\n");
	else if (value == UART)
		return sprintf(buf, "UART\n");
	else if (value == AUDIO)
		return sprintf(buf, "AUDIO\n");
	else if (value == DHOST)
		return sprintf(buf, "DHOST\n");
	else if (value == AUTO)
		return sprintf(buf, "AUTO\n");
	else
		return sprintf(buf, "%x", value);
}

EXPORT_SYMBOL_GPL(fsa9480_get_switch);

#ifdef CONFIG_VIDEO_MHL_V1
void FSA9480_EnableIntrruptByMHL(bool _bDo)
{
	struct fsa9480_platform_data *pdata = chip->pdata;
	struct i2c_client *client = chip->client;
	char buf[16];

	if (true == _bDo) {
		fsa9480_write_reg(client, FSA9480_REG_CTRL, 0x1E);
		EnableFSA9480Interrupts();
	} else {
		DisableFSA9480Interrupts();
	}

	fsa9480_get_switch(buf);
	printk("[%s] fsa switch status = %s\n", __func__, buf);
}

/*MHL call this function to change VAUDIO path*/
void FSA9480_CheckAndHookAudioDock(void)
{
	struct fsa9480_platform_data *pdata = chip->pdata;
	struct i2c_client *client = chip->client;

	printk("[FSA9480] %s: FSA9485 VAUDIO\n", __func__);

	isMHLconnected = 0;
 
	if (pdata->mhl_cb)
		pdata->mhl_cb(FSA9480_DETACHED);

	EnableFSA9480Interrupts();

	if (chip->id == 0)
		chip->mansw = VAUDIO_9485;
	else
		chip->mansw = VAUDIO;

	/*make ID change report */
	fsa9480_write_reg(client, FSA9480_REG_CTRL, 0x16);

	if (pdata->deskdock_cb)
		pdata->deskdock_cb(FSA9480_ATTACHED);

}

EXPORT_SYBMOL_GPL(FSA9480_CheckAndHookAudioDock);
#endif

static ssize_t fsa9480_show_status(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	u8 devid, ctrl, adc, dev1, dev2, intr;
	u8 intmask1, intmask2, time1, time2, mansw1;

	fsa9480_read_reg(client, FSA9480_REG_DEVID, &devid);
	fsa9480_read_reg(client, FSA9480_REG_CTRL, &ctrl);
	fsa9480_read_reg(client, FSA9480_REG_ADC, &adc);
	fsa9480_read_reg(client, FSA9480_REG_INT1_MASK, &intmask1);
	fsa9480_read_reg(client, FSA9480_REG_INT2_MASK, &intmask2);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T1, &dev1);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T2, &dev2);
	fsa9480_read_reg(client, FSA9480_REG_TIMING1, &time1);
	fsa9480_read_reg(client, FSA9480_REG_TIMING2, &time2);
	fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);

	fsa9480_read_reg(client, FSA9480_REG_INT1, &intr);
	intr &= 0xFF;

	return sprintf(buf, "Device ID(%02x), CTRL(%02x)\n"
		       "ADC(%02x), DEV_T1(%02x), DEV_T2(%02x)\n"
		       "INT(%04x), INTMASK(%02x, %02x)\n"
		       "TIMING(%02x, %02x), MANSW1(%02x)\n",
		       devid, ctrl, adc, dev1, dev2, intr,
		       intmask1, intmask2, time1, time2, mansw1);
}

static ssize_t fsa9480_show_manualsw(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return fsa9480_get_switch(buf);

}

static ssize_t fsa9480_set_manualsw(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	fsa9480_set_switch(buf);
	return count;
}

static ssize_t fsa9480_set_syssleep(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fsa9480_usbsw *usbsw = chip;

	if (!strncmp(buf, "1", 1)) {
		pm_qos_update_request(&usbsw->qos_idle,
				      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
		__pm_relax(&JIGConnect_suspend_wake);
	}
	return count;
}

ssize_t usb_state_show_attrs(struct device * dev,
			     struct device_attribute * attr, char *buf)
{
	//FIXME: This will be implemented
	return 0;
}

static ssize_t usb_sel_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "PDA");
}

static DEVICE_ATTR(status, S_IRUGO, fsa9480_show_status, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWUSR,
		   fsa9480_show_manualsw, fsa9480_set_manualsw) ;
static DEVICE_ATTR(syssleep, S_IWUSR, NULL, fsa9480_set_syssleep);
static DEVICE_ATTR(usb_state, S_IRUGO, usb_state_show_attrs, NULL);
static DEVICE_ATTR(usb_sel, S_IRUGO, usb_sel_show_attrs, NULL);

static struct attribute *fsa9480_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_switch.attr,
	&dev_attr_syssleep.attr,
	NULL
};

static const struct attribute_group fsa9480_group = {
	.attrs = fsa9480_attributes,
};

static irqreturn_t fsa9480_irq_handler(int irq, void *data)
{
	struct fsa9480_usbsw *usbsw = data;
	schedule_work(&usbsw->work);

	return IRQ_HANDLED;
}

#if 0
/* SW RESET for TI USB:To fix no USB recog problem after jig attach&detach*/
static void TI_SWreset(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;

	printk("[FSA9480] TI_SWreset ...Start\n");
	disable_irq(client->irq);

	/*Hold SCL&SDA Low more than 30ms */
	gpio_direction_output(mfp_to_gpio(GPIO050_GPIO_50), 0);
	gpio_direction_output(mfp_to_gpio(GPIO049_GPIO_49), 0);
	msleep(31);
	/*Make SCL&SDA High again */
	gpio_direction_output(mfp_to_gpio(GPIO050_GPIO_50), 1);
	gpio_direction_output(mfp_to_gpio(GPIO049_GPIO_49), 1);
	/*Should I make this input setting? Not Sure */
	//gpio_direction_input(mfp_to_gpio(MFP_PIN_GPIO64));
	//gpio_direction_input(mfp_to_gpio(MFP_PIN_GPIO66));

	/*Write SOME Init register value again */
	fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0x20);
	fsa9480_write_reg(client, FSA9480_REG_CTRL, 0x1E);

	enable_irq(client->irq);
	printk("[FSA9480] TI_SWreset ...Done\n");
}
#endif

 /* microUSB switch IC : SM5502 - Silicon Mitus */
static void fsa9480_detect_dev_sm(struct fsa9480_usbsw *usbsw, int intrs)
{
	u8 val1, val2, val3, adc, rvd_id;
	u8 intr, intr2;
	int dev_classifi = 0;

	struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;

	intr = intrs & 0xFF;
	intr2 = (intrs & 0xFF00) >> 8;

	fsa9480_read_reg(client, FSA9480_REG_DEV_T1, &val1);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T2, &val2);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T3, &val3);
	fsa9480_read_reg(client, FSA9480_REG_ADC, &adc);
	fsa9480_read_reg(client, FSA9480_REG_VBUSINVALID, &rvd_id);

	/* Unusual Cases */
	if ((intr == 0x02) && (isProbe == 1)) {
		fsa9480_id_open();
		return;
	}

	/* Attached */
	if (intr & (1 << 0)) {
		if (val1 & FSA9480_DEV_T1_USB_MASK
		    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			dev_classifi = CABLE_TYPE1_USB_MUIC;
			printk(KERN_INFO "[FSA9480] USB ATTACHED*****\n");
			__pm_stay_awake(&USB_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
		}
		if (val1 & FSA9480_DEV_T1_CHARGER_MASK
		    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			dev_classifi = CABLE_TYPE1_TA_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(DCP/CDP) ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_OTG_MASK) {
			dev_classifi = CABLE_TYPE1_OTG_MUIC;
			printk(KERN_INFO "[FSA9480] OTG ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_CARKIT_MASK) {
			dev_classifi = CABLE_TYPE1_CARKIT_T1OR2_MUIC;
			printk(KERN_INFO "[FSA9480] CARKIT ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_UARTOFF_MASK) {
			if (rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
				dev_classifi = CABLE_TYPE2_JIG_UART_OFF_VB_MUIC;
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTOFF_VB ATTACHED*****\n");
			} else {
				dev_classifi = CABLE_TYPE2_JIG_UART_OFF_MUIC;
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTOFF ATTACHED*****\n");
			}
		}
		if (val2 & FSA9480_DEV_T2_JIG_UARTON_MASK) {
			if (rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
				dev_classifi = CABLE_TYPE2_JIG_UART_ON_VB_MUIC;
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTON_VB ATTACHED*****\n");
			} else {
				dev_classifi = CABLE_TYPE2_JIG_UART_ON_MUIC;
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTON ATTACHED*****\n");
			}
		}
		if (val2 & FSA9480_DEV_T2_JIG_USBOFF_MASK
		    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_USB_OFF_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_OFF ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_USBON_MASK
		    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_USB_ON_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_ON ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_MASK) {
			printk("[FSA9480] AP WakeLock for FactoryTest *****\n");
			__pm_stay_awake(&JIGConnect_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
		}
		if (val2 & FSA9480_DEV_T2_DESKDOCK_MASK) {
			/* Check device3 register for Dock+VBUS */
			if (val3 & FSA9480_DEV_T3_DESKDOCK_VB_MASK
			    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
				dev_classifi = CABLE_TYPE3_DESKDOCK_VB_MUIC;
				printk(KERN_INFO
				       "[FSA9480] DESKDOCK+VBUS ATTACHED*****\n");
			} else {
				dev_classifi = CABLE_TYPE2_DESKDOCK_MUIC;
				printk(KERN_INFO
				       "[FSA9480] DESKDOCK ATTACHED*****\n");
			}
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 1);
			if (jack_is_detected) {
				fsa9480_disable_vaudio();
			} else {
				fsa9480_set_vaudio();
			}
		}
		if (val3 & FSA9480_DEV_T3_U200CHG_MASK
		    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			dev_classifi = CABLE_TYPE3_U200CHG_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(U200 CHG) ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_NONSTD_SDP_MASK
		    && rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			dev_classifi = CABLE_TYPE3_NONSTD_SDP_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(NON-STANDARD SDP) ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_UART_MASK ||
		    val2 & FSA9480_DEV_T2_UART_MASK) {
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_ATTACHED);
		}
		/* for Charger driver */
		if (pdata->charger_cb)
			pdata->charger_cb(dev_classifi);
		blocking_notifier_call_chain(&usb_switch_notifier, dev_classifi,
					     NULL);
	} else if (intr & (1 << 1)) {	/* DETACH */
		if (usbsw->dev1 & FSA9480_DEV_T1_USB_MASK
		    && usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			printk(KERN_INFO "[FSA9480] USB DETACHED*****\n");
			__pm_relax(&USB_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_CHARGER_MASK
		    && usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			printk(KERN_INFO
			       "[FSA9480] TA(DCP/CDP) DETACHED*****\n");
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_OTG_MASK) {
			printk(KERN_INFO "[FSA9480] OTG DETACHED*****\n");
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_CARKIT_MASK) {
			printk(KERN_INFO "[FSA9480] CARKIT DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_UARTOFF_MASK) {
			if (usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTOFF+VBUS DETACHED*****\n");
			} else {
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTOFF DETACHED*****\n");
			}
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_UARTON_MASK) {
			if (usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTON_VB DETACHED*****\n");
			} else {
				printk(KERN_INFO
				       "[FSA9480] JIG_UARTON DETACHED*****\n");
			}
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_USBOFF_MASK
		    && usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_OFF DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_USBON_MASK
		    && usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_ON DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_MASK) {
			printk("[FSA9480] AP WakeLock Release *****\n");
			__pm_relax(&JIGConnect_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_DESKDOCK_MASK) {
			/* Check device3 register for Dock+VBUS */
			if (usbsw->dev3 & FSA9480_DEV_T3_DESKDOCK_VB_MASK
			    && usbsw->dev_rvd_id) {
				printk(KERN_INFO
				       "[FSA9480] DESKDOCK+VBUS DETTACHED*****\n");
			} else {
				printk(KERN_INFO
				       "[FSA9480] DESKDOCK DETACHED*****\n");
			}
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 0);
			fsa9480_disable_vaudio();
		}
		if (usbsw->dev3 & FSA9480_DEV_T3_U200CHG_MASK
		    && usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			printk(KERN_INFO
			       "[FSA9480] TA(U200_CHG) DETTACHED*****\n");
		}

		if (usbsw->dev3 & FSA9480_DEV_T3_NONSTD_SDP_MASK
		    && usbsw->dev_rvd_id & FSA9480_DEV_RVDID1_VBUSIN_MASK) {
			printk(KERN_INFO
			       "[FSA9480] TA(NON-STANDARD SDP) DETACHED*****\n");
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_UART_MASK ||
		    usbsw->dev2 & FSA9480_DEV_T2_UART_MASK) {
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_DETACHED);
		}
		/* for Charger driver */
		if (pdata->charger_cb)
			pdata->charger_cb(CABLE_TYPE_NONE_MUIC);
		blocking_notifier_call_chain(&usb_switch_notifier,
					     CABLE_TYPE_NONE_MUIC, NULL);
	}

	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
	usbsw->dev3 = val3;
	usbsw->adc = adc;
	usbsw->dev_rvd_id = rvd_id;
	chip->dev1 = val1;
	chip->dev2 = val2;
	chip->dev3 = val3;
	chip->adc = adc;
	chip->dev_rvd_id = rvd_id;
}

static void fsa9480_detect_dev_ti(struct fsa9480_usbsw *usbsw, int intrs, int reason)
{
	u8 val1, val2, val3, adc;
	u8 intr1, intr2;
	int dev_classifi = 0;

	struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;

	/* Add delay for Tablet 2A Charger */
	usleep_range(9000, 10000);

	fsa9480_read_reg(client, FSA9480_REG_DEV_T1, &val1);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T2, &val2);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T3, &val3);
	fsa9480_read_reg(client, FSA9480_REG_ADC, &adc);

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	fsa9480_read_adc_value();
	if (((usbsw->audio_dock_flag ||  usbsw->usb_dock_flag) && usbsw->intr2 & 0x02) ||
		(usbsw->audio_dock_flag && reason)) {
		intrs = 0x01;
		val1 = 0;
		val2 = DEV_AV;
		val3 = 0;
	}

	if (reason == DETECT_REASON_BOOT &&
		(usbsw->audio_dock_flag || usbsw->usb_dock_flag)) {
		schedule_delayed_work(&usbsw->dock_work, msecs_to_jiffies(30000));
	}
	else if (reason == DETECT_REASON_DOCKDETECT &&
		(usbsw->audio_dock_flag || usbsw->usb_dock_flag)) {
		intrs = 0x01;
		val1 = 0;
		val2 = DEV_AV;
		val3 = 0;
	}
	usbsw->audio_dock_flag = 0;
	usbsw->usb_dock_flag = 0;
#endif

	intr1 = intrs & 0xFF;
	intr2 = (intrs & 0xFF00) >> 8;

	/* Unusual Cases */
	if ((intr1 == 0x02) && (isProbe == 1)) {
		fsa9480_id_open();
		return;
	}

	/* Attached */
	if (intr1 & (1 << 0)) {
		if (val1 & FSA9480_DEV_T1_USB_MASK) {
			dev_classifi = CABLE_TYPE1_USB_MUIC;
			printk(KERN_INFO "[FSA9480] USB ATTACHED*****\n");
			__pm_stay_awake(&USB_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
		}
		if (val1 & FSA9480_DEV_T1_CHARGER_MASK) {
			dev_classifi = CABLE_TYPE1_TA_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(DCP/CDP) ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_OTG_MASK) {
			dev_classifi = CABLE_TYPE1_OTG_MUIC;
			printk(KERN_INFO "[FSA9480] OTG ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_CARKIT_MASK) {
			dev_classifi = CABLE_TYPE1_CARKIT_T1OR2_MUIC;
			printk(KERN_INFO "[FSA9480] CARKIT ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_UARTOFF_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_UART_OFF_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_UARTOFF ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_UARTON_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_UART_ON_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_UARTON ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_USBOFF_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_USB_OFF_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_OFF ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_USBON_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_USB_ON_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_ON ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_MASK) {
			printk("[FSA9480] AP WakeLock for FactoryTest *****\n");
			__pm_stay_awake(&JIGConnect_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
		}
		if (val2 & FSA9480_DEV_T2_DESKDOCK_MASK) {
			dev_classifi = CABLE_TYPE2_DESKDOCK_MUIC;
			printk(KERN_INFO "[FSA9480] DESKDOCK ATTACHED*****\n");
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 1);
			if (jack_is_detected) {
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
				audio_state = 1;
#endif
				fsa9480_disable_vaudio();
			}
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			else
				fsa9480_set_vaudio();
#endif
		}
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
		if (val2 == DEV_RESERVED) {
			printk("[FSA9480] USB DOCK ATTACHED*****\n");
			dev_classifi = CABLE_TYPE_AUDIODOCK_MUIC;
			fsa9480_set_usbhost();
		}
#endif
		if (val3 & FSA9480_DEV_T3_DESKDOCK_VB_MASK) {
			dev_classifi = CABLE_TYPE3_DESKDOCK_VB_MUIC;
			printk(KERN_INFO
			       "[FSA9480] DESKDOCK+VBUS ATTACHED*****\n");
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 1);
			if (jack_is_detected)
				fsa9480_disable_vaudio();
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			else
				fsa9480_set_vaudio();
#endif
		}
		if (adc == 0x10) {
			dev_classifi = CABLE_TYPE2_DESKDOCK_MUIC;
			/* Only Support Charging */
			printk(KERN_INFO
			       "[FSA9480] SmartDock ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_U200CHG_MASK) {
			dev_classifi = CABLE_TYPE3_U200CHG_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(U200 CHG) ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_NONSTD_SDP_MASK) {
			dev_classifi = CABLE_TYPE3_NONSTD_SDP_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(NON-STANDARD SDP) ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_APPLECHG_MASK) {
			dev_classifi = CABLE_TYPE3_APPLECHG_MUIC;
			printk(KERN_INFO "[FSA9480] TA(APPLE) ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_UART_MASK ||
		    val2 & FSA9480_DEV_T2_UART_MASK) {
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_ATTACHED);
		}
		/* for Charger driver */
		if (pdata->charger_cb)
			pdata->charger_cb(dev_classifi);
		blocking_notifier_call_chain(&usb_switch_notifier, dev_classifi,
					     NULL);
	} else if (intr1 & (1 << 1)) {	/* DETACH */
		if (usbsw->dev1 & FSA9480_DEV_T1_USB_MASK) {
			printk(KERN_INFO "[FSA9480] USB DETACHED*****\n");
			__pm_relax(&USB_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_CHARGER_MASK) {
			printk(KERN_INFO
			       "[FSA9480] TA(DCP/CDP) DETACHED*****\n");
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_OTG_MASK) {
			printk(KERN_INFO "[FSA9480] OTG DETACHED*****\n");
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_CARKIT_MASK) {
			printk(KERN_INFO "[FSA9480] CARKIT DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_UARTOFF_MASK) {
			printk(KERN_INFO
			       "[FSA9480] JIG_UARTOFF DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_UARTON_MASK) {
			printk(KERN_INFO
			       "[FSA9480] JIG_UARTON DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_USBOFF_MASK) {
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_OFF DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_USBON_MASK) {
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_ON DETACHED*****\n");
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_JIG_MASK) {
			printk("[FSA9480] AP WakeLock Release *****\n");
			__pm_relax(&JIGConnect_suspend_wake);
			pm_qos_update_request(&usbsw->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
		}
		if (usbsw->dev2 & FSA9480_DEV_T2_DESKDOCK_MASK) {
			printk(KERN_INFO "[FSA9480] DESKDOCK DETACHED*****\n");
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 0);
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			if (jack_is_detected)
				fsa9480_set_vaudio();
			else
				fsa9480_disable_vaudio();
#else
			if (jack_is_detected)
				fsa9480_set_vaudio();
#endif
		}
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
		fsa9480_read_adc_value();
		if ((!(usbsw->dev2 & FSA9480_DEV_T2_DESKDOCK_MASK)) &&
			(usbsw->audio_dock_flag ||  usbsw->usb_dock_flag || adc == 0x1f)) {
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 0);
			if (jack_is_detected)
				fsa9480_set_vaudio();
			else
				fsa9480_disable_vaudio();
		}

		if (val2 == DEV_RESERVED) {
			printk("[FSA9480] USB DOCK DETTACHED*****\n");
			fsa9480_disable_usbhost();
		}
#endif
		if (usbsw->dev3 & FSA9480_DEV_T3_DESKDOCK_VB_MASK) {
			printk(KERN_INFO
			       "[FSA9480] DESKDOCK+VBUS DETTACHED*****\n");
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 0);
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
			fsa9480_disable_vaudio();
#else
			if (jack_is_detected)
				fsa9480_set_vaudio();
#endif
		}
		if (usbsw->adc == 0x10) {
			dev_classifi = CABLE_TYPE2_DESKDOCK_MUIC;
			/* Only Support Charging */
			printk(KERN_INFO
			       "[FSA9480] SmartDock Detached*****\n");
		}
		if (usbsw->dev3 & FSA9480_DEV_T3_U200CHG_MASK) {
			printk(KERN_INFO
			       "[FSA9480] TA(U200_CHG) DETTACHED*****\n");
		}
		if (usbsw->dev3 & FSA9480_DEV_T3_NONSTD_SDP_MASK) {
			printk(KERN_INFO
			       "[FSA9480] TA(NON-STANDARD SDP) DETACHED*****\n");
		}

		if (usbsw->dev3 & FSA9480_DEV_T3_APPLECHG_MASK) {
			printk(KERN_INFO "[FSA9480] TA(APPLE) DETACHED*****\n");
		}
		if (usbsw->dev1 & FSA9480_DEV_T1_UART_MASK ||
		    usbsw->dev2 & FSA9480_DEV_T2_UART_MASK) {
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_DETACHED);
		}
		/* for Charger driver */
		if (pdata->charger_cb)
			pdata->charger_cb(CABLE_TYPE_NONE_MUIC);
		blocking_notifier_call_chain(&usb_switch_notifier,
					     CABLE_TYPE_NONE_MUIC, NULL);
	}

	/* Bug Case WorkAround */
	if (!intr1 && !intr2) {
		if (!val1 && !val2 && !val3) {
			printk(KERN_INFO "[FSA9480](BUG Case)Accessory DETACHED*****\n");
			/* for Charger driver */
			if (pdata->charger_cb)
				pdata->charger_cb(CABLE_TYPE_NONE_MUIC);
			blocking_notifier_call_chain(&usb_switch_notifier,
						     CABLE_TYPE_NONE_MUIC, NULL);
		}
	}		

	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
	usbsw->dev3 = val3;
	usbsw->adc = adc;
	chip->dev1 = val1;
	chip->dev2 = val2;
	chip->dev3 = val3;
	chip->adc = adc;
}

static void fsa9480_detect_dev_inq(struct fsa9480_usbsw *usbsw, int intrs)
{
	u8 val1, val2, val3, adc;
	u8 intr1, intr2;
	int dev_classifi = 0;

	struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;

	intr1 = intrs & 0xFF;
	intr2 = (intrs & 0xFF00) >> 8;

	/* Add delay for Tablet 2A Charger */
	usleep_range(9000, 10000);

	fsa9480_read_reg(client, FSA9480_REG_DEV_T1, &val1);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T2, &val2);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T3, &val3);
	fsa9480_read_reg(client, FSA9480_REG_ADC, &adc);

	/* Attached */
	if (intr1 & (1 << 0)) {
		if (val1 & FSA9480_DEV_T1_USB_MASK) {
			dev_classifi = CABLE_TYPE1_USB_MUIC;
			printk(KERN_INFO "[FSA9480] USB ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_CHARGER_MASK) {
			dev_classifi = CABLE_TYPE1_TA_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(DCP/CDP) ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_OTG_MASK) {
			dev_classifi = CABLE_TYPE1_OTG_MUIC;
			printk(KERN_INFO "[FSA9480] OTG ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_CARKIT_MASK) {
			dev_classifi = CABLE_TYPE1_CARKIT_T1OR2_MUIC;
			printk(KERN_INFO "[FSA9480] CARKIT ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_UARTOFF_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_UART_OFF_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_UARTOFF ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_UARTON_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_UART_ON_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_UARTON ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_USBOFF_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_USB_OFF_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_OFF ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_JIG_USBON_MASK) {
			dev_classifi = CABLE_TYPE2_JIG_USB_ON_MUIC;
			printk(KERN_INFO
			       "[FSA9480] JIG_USB_ON ATTACHED*****\n");
		}
		if (val2 & FSA9480_DEV_T2_DESKDOCK_MASK) {
			dev_classifi = CABLE_TYPE2_DESKDOCK_MUIC;
			printk(KERN_INFO "[FSA9480] DESKDOCK ATTACHED*****\n");
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 1);
			if (jack_is_detected) {
				fsa9480_disable_vaudio();
			} else {
				fsa9480_set_vaudio();
			}
		}
		if (val3 & FSA9480_DEV_T3_DESKDOCK_VB_MASK) {
			dev_classifi = CABLE_TYPE3_DESKDOCK_VB_MUIC;
			printk(KERN_INFO
			       "[FSA9480] DESKDOCK+VBUS ATTACHED*****\n");
			/* Dock */
			switch_set_state(&usbsw->dock_dev, 1);
			if (jack_is_detected) {
				fsa9480_disable_vaudio();
			} else {
				fsa9480_set_vaudio();
			}
		}
		if (adc == 0x10) {
			dev_classifi = CABLE_TYPE2_DESKDOCK_MUIC;
			/* Only Support Charging */
			printk(KERN_INFO
			       "[FSA9480] SmartDock ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_U200CHG_MASK) {
			dev_classifi = CABLE_TYPE3_U200CHG_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(U200 CHG) ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_NONSTD_SDP_MASK) {
			dev_classifi = CABLE_TYPE3_NONSTD_SDP_MUIC;
			printk(KERN_INFO
			       "[FSA9480] TA(NON-STANDARD SDP) ATTACHED*****\n");
		}
		if (val3 & FSA9480_DEV_T3_APPLECHG_MASK) {
			dev_classifi = CABLE_TYPE3_APPLECHG_MUIC;
			printk(KERN_INFO "[FSA9480] TA(APPLE) ATTACHED*****\n");
		}
		if (val1 & FSA9480_DEV_T1_UART_MASK ||
		    val2 & FSA9480_DEV_T2_UART_MASK) {
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_ATTACHED);
		}
		/* for Charger driver */
		if (pdata->charger_cb)
			pdata->charger_cb(dev_classifi);
		blocking_notifier_call_chain(&usb_switch_notifier, dev_classifi,
					     NULL);
	}

	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
	usbsw->dev3 = val3;
	usbsw->adc = adc;
}


void muic_attached_accessory_inquire(void)
{
	struct fsa9480_usbsw *usbsw = chip;

	printk(KERN_INFO "[FSA9480] %s\n", __func__);
	fsa9480_detect_dev_inq(usbsw, 1);
}
EXPORT_SYMBOL_GPL(muic_attached_accessory_inquire);

int get_real_usbic_state(void)
{
	struct fsa9480_usbsw *usbsw = chip;
	int ret = MICROUSBIC_NO_DEVICE;
	u8 val1 = 0;
	u8 val2 = 0;

	/* read real usb ic state
	   val1 = chip->dev1;
	   val2 = chip->dev2;
	 */
	struct i2c_client *client = usbsw->client;
	fsa9480_read_reg(client, FSA9480_REG_DEV_T1, &val1);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T2, &val2);

	if (val1 & FSA9480_DEV_T1_USB_MASK)
		ret = MICROUSBIC_USB_CABLE;
	else if (val1 & FSA9480_DEV_T1_CHARGER_MASK)
		ret = MICROUSBIC_USB_CHARGER;
	else if (val1 & FSA9480_DEV_T1_UART_MASK)
		ret = MICROUSBIC_USB_CHARGER;
	else if (val1 & FSA9480_DEV_T1_HOST_MASK)
		ret = MICROUSBIC_HOST;

	if (ret == MICROUSBIC_NO_DEVICE) {
		if (val2 & DEV_JIG_USB_ON)
			ret = MICROUSBIC_JIG_USB_ON;
		else if (val2 & FSA9480_DEV_T2_MHL_MASK)
			ret = MICROUSBIC_MHL_CHARGER;
	}

	return ret;
}

static void fsa9480_work_cb(struct work_struct *work)
{
	u8 intr, intr2;
	int intrs = 0;

	struct fsa9480_usbsw *usbsw =
	    container_of(work, struct fsa9480_usbsw, work);
	struct i2c_client *client = usbsw->client;

	mutex_lock(&usbsw->mutex);

	/* Read and Clear Interrupt1/2 */
	fsa9480_read_reg(client, FSA9480_REG_INT1, &intr);
	fsa9480_read_reg(client, FSA9480_REG_INT2, &intr2);

	intrs |= (intr2 << 8) | intr;

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	usbsw->intr2 = intr2;
#endif

	if (usbsw->id == muic_list[0].id)
		fsa9480_detect_dev_sm(usbsw, intrs);
	else if (usbsw->id == muic_list[1].id)
		fsa9480_detect_dev_ti(usbsw, intrs,DETECT_REASON_NORMAL);

	mutex_unlock(&usbsw->mutex);
}

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
static void fsa9480_muic_dock_detect(struct work_struct *work)
{
	struct fsa9480_usbsw *info =
		container_of(work, struct fsa9480_usbsw, dock_work.work);

	fsa9480_detect_dev_ti(info, 1, DETECT_REASON_DOCKDETECT);
}
#endif

static int fsa9480_irq_init(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret = 0, irq = -1;
	u8 intr1, intr2, ocp2;
	u8 mansw1;
	unsigned int ctrl = CTRL_MASK;

	/* Read and Clear INTERRUPT1,2 REGS */
	fsa9480_read_reg(client, FSA9480_REG_INT1, &intr1);
	fsa9480_read_reg(client, FSA9480_REG_INT2, &intr2);

	if (usbsw->id == muic_list[0].id)
		/* INTMASK1 : Mask Nothing */
		ret = fsa9480_write_reg(client, FSA9480_REG_INT1_MASK, 0x00);
	else if (usbsw->id == muic_list[1].id)
		/* INTMASK1 : Mask OCP, OVP_OCP_OTP_DIS */
		ret = fsa9480_write_reg(client, FSA9480_REG_INT1_MASK, 0xC0);
	if (ret < 0)
		return ret;

	if (usbsw->id == muic_list[0].id)
		/* INTMASK2 Mask VBUSOUT ON/OFF Interrupt */
		ret = fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0x81);
	else if (usbsw->id == muic_list[1].id) {
		/* INTMASK2 Mask VBUSOUT ON/OFF,  Interrupt */
		ret = fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0xA1);

		/* Change OCP Level */
		fsa9480_read_reg(client, FSA9480_REG_OCP_2, &ocp2);
		ocp2 |= 0b11;
		fsa9480_write_reg(client, FSA9480_REG_OCP_2, ocp2);	
	}
	if (ret < 0)
		return ret;

	fsa9480_read_reg(client, FSA9480_REG_MANSW1, &mansw1);
	usbsw->mansw = mansw1;

	/* Unmask Interrupt */
	ctrl &= ~INT_MASK;

	/* Manual Switching Mode */
	if (usbsw->mansw)
		ctrl &= ~MANUAL_SWITCH;

	/* CONTROL REG */
	fsa9480_write_reg(client, FSA9480_REG_CTRL, ctrl);

	INIT_WORK(&usbsw->work, fsa9480_work_cb);

	ret = gpio_request(mfp_to_gpio(GPIO_IRQ(client->irq)), "fsa9480 irq");
	if (ret) {
		dev_err(&client->dev, "fsa9480: Unable to get gpio %d\n",
			client->irq);
		goto gpio_out;
	}
	gpio_direction_input(mfp_to_gpio(GPIO_IRQ(client->irq)));

	ret = request_irq(client->irq, fsa9480_irq_handler, IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW, "fsa9480 micro USB", usbsw);	/*2. Low level detection */
	if (ret) {
		dev_err(&client->dev, "fsa9480: Unable to get IRQ %d\n", irq);
		goto out;
	}

	return 0;
gpio_out:
	gpio_free(mfp_to_gpio(GPIO_IRQ(client->irq)));
out:
	return ret;
}

static int __devinit fsa9480_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
#if defined (VBUS_DETECT)
	struct fsa9480_platform_data *pdata = client->dev.platform_data;
#endif
	struct fsa9480_usbsw *usbsw;
	struct device *switch_dev;

	int i, ret = 0;
#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	u8 ocp_setting;
#endif

	printk("[FSA9480] PROBE ......\n");

	isProbe = 1;

	/* For AT Command FactoryTest */
	wakeup_source_init(&JIGConnect_suspend_wake, "JIGConnect_suspend_wake");
	wakeup_source_init(&USB_suspend_wake, "USB_suspend_wake");

	usbsw = kzalloc(sizeof(struct fsa9480_usbsw), GFP_KERNEL);
	if (!usbsw) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	usbsw->client = client;
	usbsw->pdata = client->dev.platform_data;

	chip = usbsw;

	i2c_set_clientdata(client, usbsw);

	mutex_init(&usbsw->mutex);

	/* DeskTop Dock  */
	usbsw->dock_dev.name = "dock";
	ret = switch_dev_register(&usbsw->dock_dev);
	if (ret < 0)
		printk("dock_dev_register error !!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (device_create_file(switch_dev, &dev_attr_adc) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_adc.attr.name);
	if (device_create_file(switch_dev, &dev_attr_usb_state) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_usb_state.attr.name);
	if (device_create_file(switch_dev, &dev_attr_usb_sel) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_usb_sel.attr.name);
	dev_set_drvdata(switch_dev, usbsw);

	ret = sysfs_create_group(&client->dev.kobj, &fsa9480_group);
	if (ret) {
		dev_err(&client->dev,
			"[FSA9480] Creating fsa9480 attribute group failed");
		goto fsa9480_probe_fail2;
	}

	fsa9480_read_reg(client, FSA9480_REG_DEVID, &usbsw->id);
	for (i = 0; i < ARRAY_SIZE(muic_list); i++) {
		if (usbsw->id == muic_list[i].id)
			printk(KERN_INFO "[FSA9480] PartNum : %s\n",
			       muic_list[i].part_num);
	}

	ret = fsa9480_irq_init(usbsw);
	if (ret)
		goto fsa9480_probe_fail;

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
		// bcd time out for 3.6s -->TI
		fsa9480_read_reg(client, FSA9480_REG_BCD_TIMER, &ocp_setting);
		ocp_setting &= 0xc7;	/* 1100 0111 */
		ocp_setting |= 0x28;
		fsa9480_write_reg(client, FSA9480_REG_BCD_TIMER, ocp_setting);
		/*set timing1 to 300ms */
		fsa9480_write_reg(client, FSA9480_REG_TIMING1, 0x4);
		fsa9480_write_reg(client, FSA9480_REG_INT2_MASK, 0xA1);
#else
		/*set timing1 to 100ms */
		fsa9480_write_reg(client, FSA9480_REG_TIMING1, 0x01);
#endif

	if (chip->pdata->reset_cb)
		chip->pdata->reset_cb();

	chip->pdata->id_open_cb = fsa9480_id_open;

#if defined (VBUS_DETECT)
	vbus = kzalloc(sizeof(struct pm860x_vbus_info), GFP_KERNEL);
	if (!vbus) {
		ret = -ENOMEM;
		goto out_mem;
	}
	dev_set_drvdata(&client->dev, vbus);

	vbus->res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	if (!vbus->res) {
		ret = -ENOMEM;
		goto out_mem2;
	}
	vbus->res->start = pdata->vbus->reg_base;
	vbus->res->end = pdata->vbus->reg_end;

	memset(&info, 0, sizeof(struct pxa_vbus_info));
	info.dev = &client->dev;
	info.res = vbus->res;

	pxa_vbus_init(&info);

	data =
	    VBUS_A_VALID | VBUS_A_SESSION_VALID | VBUS_B_SESSION_VALID |
	    VBUS_B_SESSION_END | VBUS_ID;
	pxa_unmask_vbus(data);

#endif
	usbsw->qos_idle.name = "Jig driver";
	pm_qos_add_request(&usbsw->qos_idle, PM_QOS_CPUIDLE_BLOCK,
			   PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	INIT_DELAYED_WORK(&usbsw->dock_work, fsa9480_muic_dock_detect);
#endif

	/* device detection */
	printk(KERN_INFO "[FSA9480] First Detection\n");
	if (usbsw->id == muic_list[0].id)
		fsa9480_detect_dev_sm(usbsw, 1);
	else if (usbsw->id == muic_list[1].id)
		fsa9480_detect_dev_ti(usbsw, 1, DETECT_REASON_BOOT);

	isProbe = 0;
	printk("[FSA9480] PROBE Done.\n");

	return 0;

#if defined (VBUS_DETECT)
out_mem:
	return ret;
out_mem2:
	kfree(vbus);
#endif

fsa9480_probe_fail2:
	if (client->irq)
		free_irq(client->irq, NULL);
fsa9480_probe_fail:
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static int __devexit fsa9480_remove(struct i2c_client *client)
{
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);
	if (client->irq)
		free_irq(client->irq, NULL);
	i2c_set_clientdata(client, NULL);

	pm_qos_remove_request(&usbsw->qos_idle);

#if defined(CONFIG_MACH_LT02LGT) || defined(USE_LGT_DOCK)
	cancel_delayed_work(&usbsw->dock_work);
#endif

	sysfs_remove_group(&client->dev.kobj, &fsa9480_group);
	kfree(usbsw);
	return 0;
}

static int fsa9480_suspend(struct i2c_client *client)
{
#if 0
	gpio_direction_output(mfp_to_gpio(GPIO050_GPIO_50), 1);	// set_value
	gpio_direction_output(mfp_to_gpio(GPIO049_GPIO_49), 1);

	printk("[FSA9480] fsa9480_suspend  enable_irq_wake...\n");
	enable_irq_wake(client->irq);
#endif
	return 0;
}

#ifdef CONFIG_PM
static int fsa9480_resume(struct i2c_client *client)
{
#if 0				/*Totoro: No need to read at resume */
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);
	u8 intr;
	u8 val1, val2;

	/* for hibernation */
	fsa9480_read_reg(client, FSA9480_REG_DEV_T1, &val1);
	fsa9480_read_reg(client, FSA9480_REG_DEV_T2, &val2);

	if (val1 || val2)
		intr = 1 << 0;
	else
		intr = 1 << 1;

	/* device detection */
	fsa9480_detect_dev(usbsw, intr);
#endif
#if 0
	printk("[FSA9480] fsa9480_resume  disable_irq_wake...\n");
	disable_irq_wake(client->irq);
#endif
	return 0;
}
#else
#define fsa9480_resume         NULL
#endif

static const struct i2c_device_id fsa9480_id[] = {
	{"fsa9480", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, fsa9480_id);

static struct i2c_driver fsa9480_i2c_driver = {
	.driver = {
		   .name = "fsa9480",
		   },
	.probe = fsa9480_probe,
	.remove = __devexit_p(fsa9480_remove),
	.suspend = fsa9480_suspend,
	.resume = fsa9480_resume,
	.id_table = fsa9480_id,
};

static int __init fsa9480_init(void)
{
	printk("[FSA9480] fsa9480_init\n");
	return i2c_add_driver(&fsa9480_i2c_driver);
}

module_init(fsa9480_init);

#ifdef CONFIG_CHARGER_DETECT_BOOT
charger_module_init(fsa9480_init);
#endif

static void __exit fsa9480_exit(void)
{
	i2c_del_driver(&fsa9480_i2c_driver);
}

module_exit(fsa9480_exit);

MODULE_AUTHOR("Wonguk.Jeong <wonguk.jeong@samsung.com>");
MODULE_DESCRIPTION("FSA9480 USB Switch driver");
MODULE_LICENSE("GPL");
