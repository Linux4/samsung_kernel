/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_USB_BOOST_H
#define _MTK_USB_BOOST_H

enum{
	TYPE_CPU_FREQ,
	TYPE_CPU_CORE,
	TYPE_DRAM_VCORE,
	TYPE_VCORE,
	_TYPE_MAXID
};

enum{
	ACT_HOLD,
	ACT_RELEASE,
	_ACT_MAXID
};

struct act_arg_obj {
	int arg1;
	int arg2;
	int arg3;
};

void usb_boost_set_para_and_arg(int id, int *para, int para_range,
	struct act_arg_obj *act_arg);

void usb_boost_by_id(int id);
void usb_boost(void);
int usb_boost_init(void);
void usb_audio_boost(bool enable);
int audio_core_hold(void);
int audio_core_release(void);
int audio_freq_hold(void);
int audio_freq_release(void);
void audio_boost_quirk_setting(int vid, int pid);
void audio_boost_default_setting(void);
void usb_boost_vcore_control(bool hold);
extern bool vcore_holding_by_others;


void register_usb_boost_act(int type_id, int action_id,
	int (*func)(struct act_arg_obj *arg));

/* #define USB_BOOST_DBG_ENABLE */
#define USB_BOOST_NOTICE(fmt, args...) \
	pr_notice("USB_BOOST, <%s(), %d> " fmt, __func__, __LINE__, ## args)

#ifdef USB_BOOST_DBG_ENABLE
#define USB_BOOST_DBG(fmt, args...) \
	pr_notice("USB_BOOST, <%s(), %d> " fmt, __func__, __LINE__, ## args)
#else
#define USB_BOOST_DBG(fmt, args...) do {} while (0)
#endif

#endif
