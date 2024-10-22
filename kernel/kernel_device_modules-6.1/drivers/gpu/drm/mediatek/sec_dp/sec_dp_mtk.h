// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SEC Displayport
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef _SEC_DP_MTK_H_
#define _SEC_DP_MTK_H_

//include
#include <linux/version.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>

#include <drm/drm_modes.h>
#include <drm/drm_device.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
#include <drm/drm_dp_helper.h>
#else
#include <drm/display/drm_dp_helper.h>
#endif
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
#include "../mediatek_v2/mtk_dp_common.h"
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
#include "dp_logger.h"
#endif

//feature
#define FEATURE_MTK_DRM_DP_ENABLED
#define FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
//#define FEATURE_USB_DP_MUX_SET
#define FEATURE_DEX_SUPPORT
//#define FEATURE_MANAGE_HMD_LIST
//#define FEATURE_DSC_SUPPORT
//#define FEATURE_SUPPORT_HBR3

//macro
#define GPIO_IS_VALID(x) (gpio_is_valid(x) && x)

//type
#ifndef FEATURE_MTK_DRM_DP_ENABLED
enum dp_usb_pin_assign_type {
	DP_USB_PIN_ASSIGNMENT_C = 4,
	DP_USB_PIN_ASSIGNMENT_D = 8,
	DP_USB_PIN_ASSIGNMENT_E = 16,
	DP_USB_PIN_ASSIGNMENT_F = 32,
	DP_USB_PIN_ASSIGNMENT_MAX_NUM,
};
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
enum redriver_tuning_table_index {
	DP_TUNE_RBR_EQ0,
	DP_TUNE_RBR_EQ1,
	DP_TUNE_HBR_EQ0,
	DP_TUNE_HBR_EQ1,
	DP_TUNE_HBR2_EQ0,
	DP_TUNE_HBR2_EQ1,
	DP_TUNE_HBR3_EQ0,
	DP_TUNE_HBR3_EQ1,
	DP_TUNE_MAX
};

enum dp_pre_emphasis_type {
	PHY_PREEMPHASIS0,	/* 0   db */
	PHY_PREEMPHASIS1,	/* 3.5 db */
	PHY_PREEMPHASIS2,	/* 6.0 db */
	PHY_PREEMPHASIS3,	/* 9.5 db */
	PHY_PREEMPHASIS_MAX,
};

enum dp_voltage_type {
	PHY_SWING0,	/* 0.4 v */
	PHY_SWING1,	/* 0.6 v */
	PHY_SWING2,	/* 0.8 v */
	PHY_SWING3,	/* 1.2 v, optional */
	PHY_SWING_MAX,
};
#endif

#define LINKRATE_RBR 0x6
#define LINKRATE_HBR 0xA
#define LINKRATE_HBR2 0x14
#define LINKRATE_HBR3 0x1E
#define LINKRATE_COUNT 4

#define RBR_PIXEL_CLOCK_PER_LANE 48600 /* KHz */
#define HBR_PIXEL_CLOCK_PER_LANE 85500 /* KHz */
#define HBR2_PIXEL_CLOCK_PER_LANE 171000 /* KHz */
#define HBR3_PIXEL_CLOCK_PER_LANE 256500 /* KHz */

#ifdef FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
enum dp_hpd_event_type {
	DP_HPD_LOW = 0x2,
	DP_HPD_HIGH = 0x4,
	DP_HPD_IRQ = 0x8,
};
enum dp_wait_state {
	DP_READY_NO,
	DP_READY_YES,
};
struct pdic_event {
	struct list_head list;
	PD_NOTI_TYPEDEF event;
};
#endif

/* monitor aspect ratio */
enum mon_aspect_ratio_t {
	MON_RATIO_NA,
	MON_RATIO_3_2,
	MON_RATIO_4_3,
	MON_RATIO_5_3,
	MON_RATIO_5_4,
	MON_RATIO_10P5_9,
	MON_RATIO_11_10,
	MON_RATIO_16_9,
	MON_RATIO_16_10,
	MON_RATIO_21_9,
	MON_RATIO_21_10,
	MON_RATIO_32_9,
	MON_RATIO_32_10,
};

#ifdef FEATURE_DEX_SUPPORT
enum dex_state {
	DEX_OFF,
	DEX_ON,
	DEX_RECONNECTING,
};

enum dex_support_type {
	DEX_NOT_SUPPORT = 0,
	DEX_FHD_SUPPORT,
	DEX_WQHD_SUPPORT,
	DEX_UHD_SUPPORT
};

struct dex_support_modes {
	u32  active_h;
	u32  active_v;
	enum mon_aspect_ratio_t mon_ratio;
	enum dex_support_type support_type;
};

struct dex_data {
	enum dex_state ui_setting;
	enum dex_state ui_run;
	enum dex_state cur_state;
	enum dex_support_type adapter_type;
	struct drm_display_mode best_mode;
	bool skip_adapter_check;
};
#endif

#ifdef FEATURE_MANAGE_HMD_LIST
#define MAX_NUM_HMD	32
#define DEX_TAG_HMD	"HMD"

enum dex_hmd_type {
	DEX_HMD_MON = 0,	/* monitor name field */
	DEX_HMD_VID,		/* vid field */
	DEX_HMD_PID,		/* pid field */
	DEX_HMD_FIELD_MAX,
};
#endif

#define DPCD_DEVID_STR_SIZE 6
#define MON_NAME_LEN	14	/* monitor name */
struct dp_sink_dev {
	u32 ven_id;		/* vendor id from PDIC */
	u32 prod_id;		/* product id from PDIC */
	char monitor_name[MON_NAME_LEN]; /* max 14 bytes, from EDID */
	u8 sw_ver[2];		/* sw revision */
	char devid_str[DPCD_DEVID_STR_SIZE + 1];

	u8 link_rate;
	u8 lane_count;
	u32 max_pix_clk; /*KHz: max bandwidth of current link */

	bool test_sink;

	u8 edid_manufacturer[4];
	u32 edid_product;
	u32 edid_serial;

	int audio_channels;
	int audio_bit_rates;
	int audio_sample_rates;

	struct drm_display_mode cur_mode;
	struct drm_display_mode best_mode;
	struct drm_display_mode pref_mode;
};

struct sec_dp_funcs {
	u8 (*get_optimal_linkrate)(u8 max_linkrate, u8 lane_count);
	void (*set_branch_info)(u8 dfp_type, u8 *sw_ver, char *devid);

	void (*logger_print)(const char *fmt, ...);
	void (*make_spd_infoframe)(u32 dpcd_rev, u8 *_header, u8 *_data);
	int (*get_aux_level)(void);

	//redriver enable
	void (*redriver_config)(bool enable, int lane);
	//Adjust the swing and pre-emphasis
	void (*redriver_notify_linkinfo)(u8 *levels);
	//uevent
	void (*set_switch_poor_connect)(void);
	void (*set_switch_hpd_state)(int state);
	void (*set_switch_audio_state)(int state);
	//edid
	void (*parse_edid)(struct edid *edid);
	struct drm_display_mode *(*get_best_mode)(void);
	void (*print_all_modes)(struct list_head *modes);
	void (*link_training_postprocess)(u8 link_rate, u8 link_count);
	void (*validate_modes)(void);
	u32 (*reduce_audio_capa)(u32 caps);
};

struct sec_dp_dev {
	struct sec_dp_funcs funcs;

	int gpio_sw_oe;
	int gpio_sw_sel;
	int gpio_usb_dir;

	struct dp_sink_dev sink_info;
	bool prefer_found;
	int max_width;
	int max_height;
	u32 aux_level;
	bool hdcp_enable_connect;

#ifdef FEATURE_DEX_SUPPORT
	struct dex_data dex;
#endif

#ifdef FEATURE_MANAGE_HMD_LIST
	struct dp_sink_dev hmd_list[MAX_NUM_HMD];  /*list of supported HMD device*/
	struct mutex hmd_lock;
	bool is_hmd_dev;
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct workqueue_struct *dp_wq;
	struct delayed_work notifier_register_work;
	struct notifier_block dp_typec_nb;
	int notifier_registered;
	int pdic_pin_type;
	int pdic_lane_count;
	bool pdic_link_conf;
	bool pdic_hpd;
	int pdic_cable_state;
	int pdic_usb_dir;
#ifdef FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
	struct list_head list_pd;
	struct delayed_work pdic_event_proceed_work;
	struct mutex pdic_lock;
	enum dp_wait_state dp_ready_wait_state;
	wait_queue_head_t dp_ready_wait;
#endif
#endif
	int test_edid_size;
	u8 edid_buf[512];
};

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER

#define dp_err(fmt, ...)						\
	do {								\
		if (dp_get_log_level() > 4) {				\
			pr_info("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#define dp_info(fmt, ...)						\
	do {								\
		if (dp_get_log_level() > 5) {				\
			pr_info("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#define dp_debug(fmt, ...)						\
	do {								\
		if (dp_get_log_level() > 6)	{			\
			pr_info("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#define dp_hex_dump(prefix, buf, size)				\
	do {							\
		print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 16, 1,	\
					buf, size, false);	\
		dp_logger_hex_dump(buf, prefix, size);	\
	} while (0)

#else /* CONFIG_SEC_DISPLAYPORT_LOGGER */

#define dp_info pr_info
#define dp_debug pr_debug
#define dp_hex_dump(prefix, buf, size)	\
	print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 16, 1,	\
					buf, size, false)

#endif /* CONFIG_SEC_DISPLAYPORT_LOGGER */

struct sec_dp_dev *dp_get_dev(void);
void dp_aux_sel(struct sec_dp_dev *dp, int dir);
void dp_aux_onoff(struct sec_dp_dev *dp, bool on, int dir);
void dp_hpd_changed(struct sec_dp_dev *dp, int hpd);
int dp_get_log_level(void);

#endif //_SEC_DP_MTK_H_
