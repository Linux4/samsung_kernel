/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SECDP_H
#define __SECDP_H

#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/secdp_logger.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>

#include "dp_hpd.h"
#include "dp_power.h"
#include "dp_panel.h"
#include "dp_catalog.h"

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
extern unsigned int lpcharge;
#endif

/*defined at kmodule/usb/typec/common/pdic_core.h*/
#define SAMSUNG_VENDOR_ID	0x04E8

#define DEXDOCK_PRODUCT_ID	0xA020	/* EE-MG950, DeX station */
#define HG950_PRODUCT_ID	0xA025	/* EE-HG950 */
#define MPA2_PRODUCT_ID		0xA027	/* EE-P5000 */
#define DEXPAD_PRODUCT_ID	0xA029	/* EE-M5100 */
#define DEXCABLE_PRODUCT_ID	0xA048	/* EE-I3100 */
#define MPA3_PRODUCT_ID		0xA056	/* EE-P3200 */

#define SECDP_ENUM_STR(x)	#x

#define SECDP_USB_CONCURRENCY
#define SECDP_USE_WAKELOCK
#define SECDP_MAX_HBR2
#define SECDP_OPTIMAL_LINK_RATE	 /* use optimum link_rate, not max link_rate */

/*#define SECDP_AUDIO_CTS*/
/*#define SECDP_HDCP_DISABLE*/
/*#define SECDP_EVENT_THREAD*/
/*#define SECDP_TEST_HDCP2P2_REAUTH*/
/*#define NOT_SUPPORT_DEX_RES_CHANGE*/

#define LEN_BRANCH_REV		3
#define	DPCD_BRANCH_HW_REV		0x509
#define	DPCD_BRANCH_SW_REV_MAJOR	0x50A
#define	DPCD_BRANCH_SW_REV_MINOR	0x50B

#define MAX_CNT_LINK_STATUS_UPDATE	4
#define MAX_CNT_HDCP_RETRY		10

/* MST: max resolution, max refresh rate, max pclk */
#define MST_MAX_COLS	3840
#define MST_MAX_ROWS	2160
#define MST_MAX_FPS	30
#define MST_MAX_PCLK	300000

#define PDIC_DP_NOTI_REG_DELAY		1000

#ifdef SECDP_EVENT_THREAD
struct secdp_event {
	struct dp_display_private *dp;
	u32 id;
};

#define SECDP_EVENT_Q_MAX 4

struct secdp_event_data {
	wait_queue_head_t event_q;
	u32 pndx;
	u32 gndx;
	struct secdp_event event_list[SECDP_EVENT_Q_MAX];
	spinlock_t event_lock;
};
#endif

/* displayport self test */
#if defined(CONFIG_SEC_DISPLAYPORT) && defined(CONFIG_SEC_DISPLAYPORT_ENG)
#define SECDP_SELF_TEST
#endif

#ifdef SECDP_SELF_TEST
#define ST_EDID_SIZE	256
#define ST_ARG_CNT	20

enum {
	ST_CLEAR_CMD,
	ST_LANE_CNT,
	ST_LINK_RATE,
	ST_CONNECTION_TEST,
	ST_HDCP_TEST,
	ST_EDID,
	ST_PREEM_TUN,
	ST_VOLTAGE_TUN,
	ST_MAX,
};

struct secdp_sef_test_item {
	char cmd_str[20];
	int arg[ST_ARG_CNT];
	int arg_cnt;
	char arg_str[100];
	bool enabled;
	void (*clear)(void);
};

int  secdp_self_test_status(int cmd);
void secdp_self_test_start_reconnect(void (*func)(void));
void secdp_self_test_start_hdcp_test(void (*func_on)(void),
	void (*func_off)(void));
u8  *secdp_self_test_get_edid(void);
void secdp_self_register_clear_func(int cmd, void (*func)(void));
int *secdp_self_test_get_arg(int cmd);
#endif/*SECDP_SELF_TEST*/

/* monitor aspect ratio */
enum mon_aspect_ratio_t {
	MON_RATIO_NA = -1,
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

static inline char *secdp_aspect_ratio_to_string(enum mon_aspect_ratio_t ratio)
{
	switch (ratio) {
	case MON_RATIO_3_2:     return DP_ENUM_STR(MON_RATIO_3_2);
	case MON_RATIO_4_3:     return DP_ENUM_STR(MON_RATIO_4_3);
	case MON_RATIO_5_3:     return DP_ENUM_STR(MON_RATIO_5_3);
	case MON_RATIO_5_4:     return DP_ENUM_STR(MON_RATIO_5_4);
	case MON_RATIO_10P5_9:  return DP_ENUM_STR(MON_RATIO_10P5_9);
	case MON_RATIO_11_10:   return DP_ENUM_STR(MON_RATIO_11_10);
	case MON_RATIO_16_9:    return DP_ENUM_STR(MON_RATIO_16_9);
	case MON_RATIO_16_10:   return DP_ENUM_STR(MON_RATIO_16_10);
	case MON_RATIO_21_9:    return DP_ENUM_STR(MON_RATIO_21_9);
	case MON_RATIO_21_10:   return DP_ENUM_STR(MON_RATIO_21_10);
	case MON_RATIO_32_9:    return DP_ENUM_STR(MON_RATIO_32_9);
	case MON_RATIO_32_10:   return DP_ENUM_STR(MON_RATIO_32_10);
	case MON_RATIO_NA:      return DP_ENUM_STR(MON_RATIO_NA);
	default:                return "unknown";
	}
}

/* adapter type : SST or MST */
enum secdp_adapter_t {
	SECDP_ADT_UNKNOWN = -1,
	SECDP_ADT_SST = 10,
	SECDP_ADT_MST = 11,
};

/* dex supported resolutions */
enum dex_support_res_t {
	DEX_RES_NOT_SUPPORT = 0,
	DEX_RES_1600X900,  /* HD+ */
	DEX_RES_1920X1080, /* FHD */
	DEX_RES_1920X1200, /* WUXGA */
	DEX_RES_2560X1080, /* UW-UXGA */
	DEX_RES_2560X1440, /* QHD */
	DEX_RES_2560X1600, /* WQXGA */
	DEX_RES_3440X1440, /* UW-QHD */
	DEX_RES_END,
};

static inline char *secdp_dex_res_to_string(int res)
{
	switch (res) {
	case DEX_RES_NOT_SUPPORT:
		return DP_ENUM_STR(DEX_RES_NOT_SUPPORT);
	case DEX_RES_1600X900:
		return DP_ENUM_STR(DEX_RES_1600X900);
	case DEX_RES_1920X1080:
		return DP_ENUM_STR(DEX_RES_1920X1080);
	case DEX_RES_1920X1200:
		return DP_ENUM_STR(DEX_RES_1920X1200);
	case DEX_RES_2560X1080:
		return DP_ENUM_STR(DEX_RES_2560X1080);
	case DEX_RES_2560X1440:
		return DP_ENUM_STR(DEX_RES_2560X1440);
	case DEX_RES_2560X1600:
		return DP_ENUM_STR(DEX_RES_2560X1600);
	case DEX_RES_3440X1440:
		return DP_ENUM_STR(DEX_RES_3440X1440);
	default:
		return "unknown";
	}
}

#define DEX_RES_DFT	DEX_RES_1920X1080   /* DeX default resolution */
#define DEX_DFT_COL	1920
#define DEX_DFT_ROW	1080
#define DEX_RES_MAX	DEX_RES_3440X1440   /* DeX max resolution */
#define DEX_MAX_COL	3440
#define DEX_MAX_ROW	1440
#define DEX_FPS_MIN	50                  /* DeX min refresh rate */
#define DEX_FPS_MAX	60                  /* DeX max refresh rate */
#define MIRROR_REFRESH_MIN	24

enum DEX_STATUS {
	DEX_DISABLED = 0,
	DEX_ENABLED,
	DEX_DURING_MODE_CHANGE,
};

/** redriver devices */
enum secdp_redrv_dev {
	SECDP_REDRV_NONE = 0,
	SECDP_REDRV_PTN36502,	/* don't need AUX_SEL control */
	SECDP_REDRV_PS5169,	/* need AUX_SEL control */
};

static inline char *secdp_redrv_to_string(int res)
{
	switch (res) {
	case SECDP_REDRV_NONE:
		return DP_ENUM_STR(SECDP_REDRV_NONE);
	case SECDP_REDRV_PTN36502:
		return DP_ENUM_STR(SECDP_REDRV_PTN36502);
	case SECDP_REDRV_PS5169:
		return DP_ENUM_STR(SECDP_REDRV_PS5169);
	default:
		return "unknown";
	}
}

enum secdp_hw_ver_t {
	DP_HW_LEGACY,
	DP_HW_V123_HBR2_HBR3,
	DP_HW_V123_HBR_RBR,
	DP_HW_MAX,
};

static inline char *secdp_hw_to_string(int hw)
{
	switch (hw) {
	case DP_HW_LEGACY:
		return DP_ENUM_STR(DP_HW_LEGACY);
	case DP_HW_V123_HBR2_HBR3:
		return DP_ENUM_STR(DP_HW_V123_HBR2_HBR3);
	case DP_HW_V123_HBR_RBR:
		return DP_ENUM_STR(DP_HW_V123_HBR_RBR);
	default:
		return "unknown";
	}
}

enum secdp_phy_param_t {
	DP_PARAM_VX,	/* voltage swing */
	DP_PARAM_PX,	/* pre-emphasis */
	DP_PARAM_MAX,
};

static inline char *secdp_phy_type_to_string(int param)
{
	switch (param) {
	case DP_PARAM_VX:
		return DP_ENUM_STR(DP_PARAM_VX);
	case DP_PARAM_PX:
		return DP_ENUM_STR(DP_PARAM_PX);
	default:
		return "unknown";
	}
}

#define EV_USBPD_ATTENTION	BIT(13)

static inline char *secdp_ev_event_to_string(int event)
{
	switch (event) {
	case EV_USBPD_ATTENTION:
		return DP_ENUM_STR(EV_USBPD_ATTENTION);
	default:
		return "unknown";
	}
}

struct secdp_attention_node {
	PD_NOTI_TYPEDEF noti;
	struct list_head list;
};

struct secdp_adapter {
	uint ven_id;
	uint prod_id;
	char fw_ver[10];   /* firmware ver, 0:h/w, 1:s/w major, 2:s/w minor */

	bool ss_genuine;
	bool ss_legacy;
	enum dex_support_res_t dex_type;
};

#define MON_NAME_LEN	14	/* monitor name length, max 13 chars + null */

#define MAX_NUM_HMD	32
#define DEX_TAG_HMD	"HMD"

struct secdp_sink_dev {
	uint ven_id;	/* vendor id from PDIC */
	uint prod_id;	/* product id from PDIC */
	char monitor_name[MON_NAME_LEN];	/* from EDID */
};

struct secdp_pdic_noti {
	struct delayed_work   reg_work;
	struct notifier_block nb;
	bool registered;
	bool reset;   /* true if PDIC or SSUSB get reset after DP connection */
};

struct secdp_prefer {
	enum mon_aspect_ratio_t	ratio;

	bool exist;   /* true if preferred resolution */
	int  hdisp;   /* horizontal pixel of preferred resolution */
	int  vdisp;   /* vertical pixel of preferred resolution */
	int  refresh; /* refresh rate of preferred resolution */
};

struct secdp_dex {
	struct class *sysfs_class;
	enum dex_support_res_t	res;	/* dex supported resolution */

	enum DEX_STATUS prev; /* previously known as "dex_now" */
	enum DEX_STATUS curr; /* previously known as "dex_en" */
	int  setting_ui;      /* "dex_set", true if setting has Dex mode */

	bool ignore_prefer_ratio; /* true if prefer ratio does not match to dex ratio */
	bool adapter_check_skip;

	/*
	 * 2 if resolution is changed during dex mode change.
	 * And once dex framework reads the dex_node_stauts using dex node,
	 * it's assigned to same value with curr.
	 */
	enum DEX_STATUS status; /* previously known as "dex_node_status" */

	bool reconnecting; /* true if dex is under reconnecting */
};

struct secdp_display_timing {
	u32  active_h;
	u32  active_v;
	u32  refresh_rate;
	bool interlaced;
	int  clock;    /* pixel clock, refer to "struct drm_display_mode" */
	enum dex_support_res_t dex_res;    /* dex supported resolution */
	enum mon_aspect_ratio_t mon_ratio; /* monitor aspect ratio */
	int  supported;                    /* for unit test */
	u64  total;
};

struct secdp_mst {
	bool exist;	/* true if MST receiver, false otherwise(SST) */
	int  hpd_count;	/* avoid successive hpd low/high/low/.. from MST */
};

struct secdp_hmd {
	struct secdp_sink_dev list[MAX_NUM_HMD]; /* supported HMD dev list */
	struct mutex lock;
	bool exist;	/* true if connected sink is known HMD device */
};

struct secdp_hdcp {
	struct delayed_work start_work;
	int retry;	/* count if dp link is unstable during hdcp */
};

struct secdp_hpd {
	struct delayed_work noti_work;
	bool noti_deferred;
	atomic_t val;	/* 1 if hpd high, 0 if hpd low" */
};

struct secdp_debug {
	bool prefer_check_skip;
};

struct secdp_misc {
	struct delayed_work link_status_work;
	struct delayed_work link_backoff_work;
	bool backoff_start;
	struct delayed_work poor_discon_work;

	bool cable_connected; /* previously known as "cable_connected_phy" */
	bool link_conf;       /* previously known as "sec_link_conf" */
	struct secdp_hpd hpd;

	struct secdp_adapter adapter;
	struct secdp_pdic_noti pdic_noti;

	struct secdp_display_timing prf_timing; /* preferred timing */
	struct secdp_display_timing mrr_timing; /* max "mirror" timing */
	struct secdp_display_timing dex_timing; /* max "dex" timing */

	struct secdp_prefer prefer;
	struct secdp_hdcp hdcp;
	struct secdp_debug debug;
	struct secdp_sysfs *sysfs;
	struct secdp_dex dex;
	struct secdp_mst mst;
	struct secdp_hmd hmd;

	struct completion dp_off_comp;
	struct completion dp_discon_comp;
	bool dp_disconnecting;

	struct mutex notify_lock;
	struct mutex attention_lock;
	struct mutex notifier_lock;
	atomic_t noti_status;

	struct notifier_block reboot_nb;
	bool reboot; /* true if rebooted or shutdown */

#ifdef SECDP_USE_WAKELOCK
	struct wakeup_source *ws;
#endif
#ifdef SECDP_EVENT_THREAD
	/* for PDIC event handler */
	struct secdp_event_data dp_event;
	struct task_struct *ev_thread;
	struct workqueue_struct *workq;
	struct list_head attention_head;
#endif
#ifdef SECDP_SELF_TEST
	struct delayed_work self_test_reconnect_work;
	struct delayed_work self_test_hdcp_test_work;

	void (*self_test_reconnect_callback)(void);
	void (*self_test_hdcp_on_callback)(void);
	void (*self_test_hdcp_off_callback)(void);

	u8 self_test_edid[ST_EDID_SIZE];
#endif
};

bool secdp_check_if_lpm_mode(void);
int  secdp_send_deferred_hpd_noti(void);
bool secdp_get_clk_status(enum dp_pm_type type);

int  secdp_pdic_noti_register_ex(struct secdp_misc *sec, bool retry);
bool secdp_phy_reset_check(void);
bool secdp_get_power_status(void);
bool secdp_get_cable_status(void);
bool secdp_get_hpd_irq_status(void);
int  secdp_get_hpd_status(void);
bool secdp_get_poor_connection_status(void);
bool secdp_get_link_train_status(void);
struct dp_panel *secdp_get_panel_info(void);
struct drm_connector *secdp_get_connector(void);

void secdp_redriver_onoff(bool enable, int lane);
void secdp_redriver_linkinfo(u32 rate, u8 v_level, u8 p_level);

int  secdp_is_mst_receiver(void);

int  secdp_power_request_gpios(struct dp_power *dp_power);
void secdp_config_gpios_factory(int aux_sel, bool out_en);
enum dp_hpd_plug_orientation secdp_get_plug_orientation(void);
bool secdp_get_reboot_status(void);

bool secdp_check_hmd_dev(const char *name_to_search);
int  secdp_store_hmd_dev(char *buf, size_t len, int num);

void secdp_timing_init(void);
void secdp_reconnect(void);
bool secdp_check_reconnect(void);
bool secdp_check_dex_mode(void);

void secdp_clear_link_status_cnt(struct dp_link *dp_link);
void secdp_reset_link_status(struct dp_link *dp_link);
bool secdp_check_link_stable(struct dp_link *dp_link);
void secdp_link_backoff_start(void);
void secdp_link_backoff_stop(void);
bool secdp_dex_adapter_skip_show(void);
void secdp_dex_adapter_skip_store(bool skip);

bool secdp_panel_hdr_supported(void);

#if defined(CONFIG_SEC_DISPLAYPORT_ENG)
enum secdp_hw_preshoot_t {
	DP_HW_PRESHOOT_0,
	DP_HW_PRESHOOT_1,
	DP_HW_PRESHOOT_MAX,
};

static inline char *secdp_preshoot_to_string(int hw)
{
	switch (hw) {
	case DP_HW_PRESHOOT_0:
		return DP_ENUM_STR(DP_HW_PRESHOOT_0);
	case DP_HW_PRESHOOT_1:
		return DP_ENUM_STR(DP_HW_PRESHOOT_1);
	default:
		return "unknown";
	}
}

int  secdp_show_hmd_dev(char *buf);

/* preshoot adjustment */
int  secdp_catalog_preshoot_show(char *buf);
void secdp_catalog_preshoot_store(char *buf);

/* voltage swing, pre-emphasis */
int  secdp_parse_vxpx_show(enum secdp_hw_ver_t hw,
				enum secdp_phy_param_t vxpx, char *buf);
int  secdp_parse_vxpx_store(enum secdp_hw_ver_t hw,
				enum secdp_phy_param_t vxpx, char *buf);
int  secdp_show_phy_param(char *buf);

/* AUX configuration */
int  secdp_aux_cfg_show(char *buf);
int  secdp_aux_cfg_store(char *buf);

/* preferred resolution debug */
int  secdp_debug_prefer_skip_show(void);
void secdp_debug_prefer_skip_store(bool skip);
int  secdp_debug_prefer_ratio_show(void);
void secdp_debug_prefer_ratio_store(int ratio);
int  secdp_show_link_param(char *buf);
#endif/*CONFIG_SEC_DISPLAYPORT_ENG*/

#endif/*__SECDP_H*/
