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

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
#include <linux/switch.h>
#endif /* CONFIG_ANDROID_SWITCH */

#if IS_ENABLED(CONFIG_SBU_SWITCH_CONTROL)
#include <linux/usb/typec/manager/if_cb_manager.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
#include <linux/combo_redriver/ps5169.h>
#endif
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
#include "../mediatek_v2/mtk_dp.h"
#endif
#include "sec_dp_mtk.h"
#include "sec_dp_sysfs.h"
#include "sec_dp_edid.h"
#include "sec_dp_api.h"
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include "displayport_bigdata.h"
#endif

//mtk extern functions
#ifdef FEATURE_MTK_DRM_DP_ENABLED
extern void mtk_dp_aux_swap_enable(bool enable);
extern void mtk_dp_set_pin_assign(u8 type);
extern void mtk_dp_SWInterruptSet(int bstatus);
extern void mtk_dp_register_sec_dp(struct sec_dp_dev *sec_dp);
extern struct mtk_dp *mtk_dp_get_dev(void);
#endif

static struct sec_dp_dev *g_sec_dp;

int sec_dp_log_level = 6;

struct sec_dp_dev *dp_get_dev(void)
{
	return g_sec_dp;
}

int dp_get_log_level(void)
{
	return sec_dp_log_level;
}

static struct drm_display_mode mode_vga[1] = {
	/* 1 - 640x480@60Hz 4:3 */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656,
		752, 800, 0, 480, 490, 492, 525, 0,
		DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, }
};

static void dp_set_fail_safe_mode(struct sec_dp_dev *dp)
{
	drm_mode_copy(&dp->sink_info.cur_mode, mode_vga);
	drm_mode_copy(&dp->sink_info.best_mode, mode_vga);
#ifdef FEATURE_DEX_SUPPORT
	drm_mode_copy(&dp->dex.best_mode, mode_vga);
#endif
}

static void dp_var_init(struct sec_dp_dev *dp)
{
	if (dp) {
		memset(&dp->sink_info, 0, sizeof(dp->sink_info));
		dp->sink_info.pref_mode.status = MODE_BAD;
	}
	dp_set_fail_safe_mode(dp);
}

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER

int redriver_tuning_table_map[DP_TUNE_MAX][3] = {
	{DP_TUNE_RBR_EQ0, LINKRATE_RBR, 0},
	{DP_TUNE_RBR_EQ1, LINKRATE_RBR, 1},
	{DP_TUNE_HBR_EQ0, LINKRATE_HBR, 0},
	{DP_TUNE_HBR_EQ1, LINKRATE_HBR, 1},
	{DP_TUNE_HBR2_EQ0, LINKRATE_HBR2, 0},
	{DP_TUNE_HBR2_EQ1, LINKRATE_HBR2, 1},
	{DP_TUNE_HBR3_EQ0, LINKRATE_HBR3, 0},
	{DP_TUNE_HBR3_EQ1, LINKRATE_HBR3, 1}
};

#define EQ0 0x20
#define EQ1 0x06
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)

static u8 ps5169_tune_table[DP_TUNE_MAX][PHY_SWING_MAX][PHY_PREEMPHASIS_MAX] = {
	//rbr_eq0
	{
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0}
	},
	//rbr_eq1
	{
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1}
	},
	//hbr_eq0
	{
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0}
	},
	//hbr_eq1
	{
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1}
	},
	//hbr2_eq0
	{
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0}
	},
	//hbr2_eq1
	{
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1}
	},
	//hbr3_eq0
	{
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0},
		{EQ0, EQ0, EQ0, EQ0}
	},
	//hbr3_eq1
	{
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1},
		{EQ1, EQ1, EQ1, EQ1}
	}
};

static void dp_redriver_config(bool enable, int lane)
{
	dp_info("%s en: %d, lane: %d\n", __func__, enable, lane);

	if (enable) {
		if (lane == 2)
			ps5169_config(DP2_LANE_USB_MODE, 1);
		else if (lane == 4)
			ps5169_config(DP_ONLY_MODE, 1);

		dp_info("redriver id1-0x%x:0x%x, id2-0x%x:0x%x\n",
			ps5169_i2c_read(Chip_ID1), ps5169_i2c_read(Chip_Rev1),
			ps5169_i2c_read(Chip_ID2), ps5169_i2c_read(Chip_Rev2));
	} else {
		//ps5169_config(CLEAR_STATE, 0);
	}
}
#endif

u8 (*dp_get_redriver_tuning_table(void))[PHY_SWING_MAX][PHY_PREEMPHASIS_MAX]
{
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
	return ps5169_tune_table;
#endif
}

static u8 (*dp_get_redriver_tuning_table_by_link_rate(u8 link_rate, int eq))[PHY_PREEMPHASIS_MAX]
{
	int idx = 0;
	u8 (*tuning_table)[PHY_SWING_MAX][PHY_PREEMPHASIS_MAX];

	for (idx = 0; idx  < DP_TUNE_MAX; idx++) {
		if (link_rate == redriver_tuning_table_map[idx][1] &&
				eq == redriver_tuning_table_map[idx][2])
			break;
	}

	tuning_table = dp_get_redriver_tuning_table();

	if (idx < DP_TUNE_MAX)
		return tuning_table[idx];

	//default HBR2
	if (eq == 0)
		return tuning_table[DP_TUNE_HBR2_EQ0];
	else
		return tuning_table[DP_TUNE_HBR2_EQ1];
}

/* bit0,1: swing, bit3,4: preemphasis */
static u8 mdrv_get_max_swing_preemp(u8 *levels)
{
	u8 max_swing = 0;
	u8 max_preemp = 0;

	for (int i = 0; i < 4; i++) {
		if (max_swing < (levels[i] & 0x3))
			max_swing = (levels[i] & 0x3);
		if (max_preemp < (levels[i] & 0xc))
			max_preemp = (levels[i] & 0xc);
	}

	return (max_swing | max_preemp);
}

/* bit0,1: swing, bit3,4: preemphasis of 4 lanes */
static void dp_redriver_notify_linkinfo(u8 *levels)
{
	struct sec_dp_dev *dp = dp_get_dev();
	u8 link_rate = dp->sink_info.link_rate;
	u8 (*tuning_table)[PHY_PREEMPHASIS_MAX];
	u8 eq0 = EQ0;
	u8 eq1 = EQ1;
	u8 v_level = 0;
	u8 p_level = 0;
	u8 max_level;

	max_level = mdrv_get_max_swing_preemp(levels);
	v_level = max_level & 0x3;
	p_level = (max_level >> 3) & 0x3;

	if (v_level >= PHY_SWING_MAX || p_level >= PHY_PREEMPHASIS_MAX) {
		dp_info("invalid redriver value v:%x, p:%x", v_level, p_level);
		return;
	}

	tuning_table = dp_get_redriver_tuning_table_by_link_rate(link_rate, 0);
	eq0 = tuning_table[v_level][p_level];

	tuning_table = dp_get_redriver_tuning_table_by_link_rate(link_rate, 1);
	eq1 = tuning_table[v_level][p_level];

	dp_info("link_rate:0x%x, v:%d, p:%d, eq0:0x%x, eq1:0x%x\n",
				link_rate, v_level, p_level, eq0, eq1);

#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
	ps5169_notify_dplink(eq0, eq1);
#endif
}
#endif //CONFIG_SEC_DISPLAYPORT_REDRIVER

// KHz
u32 dp_get_pixel_clock(u8 link_rate, u8 lane_count)
{
	u32 pixel_clock = 0;

	switch (link_rate) {
	case LINKRATE_RBR:
		pixel_clock = RBR_PIXEL_CLOCK_PER_LANE * (u32)lane_count;
		break;
	case LINKRATE_HBR:
		pixel_clock = HBR_PIXEL_CLOCK_PER_LANE * (u32)lane_count;
		break;
	case LINKRATE_HBR2:
		pixel_clock = HBR2_PIXEL_CLOCK_PER_LANE * (u32)lane_count;
		break;
	case LINKRATE_HBR3:
		pixel_clock = HBR3_PIXEL_CLOCK_PER_LANE * (u32)lane_count;
		break;
	default:
		pixel_clock = HBR2_PIXEL_CLOCK_PER_LANE * (u32)lane_count;
		break;
	}

	return pixel_clock;
}

static bool dp_devid_is_ps176(struct sec_dp_dev *dp)
{
	if (dp->sink_info.devid_str[0] == '1' &&
			dp->sink_info.devid_str[1] == '7' &&
			dp->sink_info.devid_str[2] == '6')
		return true;

	return false;
}

//max_pclk: KHz
static bool dp_mode_is_optimizable(struct drm_display_mode *mode, u32 max_pclk)
{
	int fps = drm_mode_vrefresh(mode);
	int resolution = mode->hdisplay * mode->vdisplay;

	if (fps > 110 && resolution >= (1920 * 1080) && max_pclk > 250000)
		return false;

	if (fps > 75 && resolution >= (2560 * 1440)  && max_pclk > 300000)
		return false;

	return true;
}

static u8 dp_get_optimal_linkrate(u8 max_linkrate, u8 lane_count)
{
	struct sec_dp_dev *dp = dp_get_dev();
	u8 new_linkrate = max_linkrate;
	u32 max_pclk = dp->sink_info.best_mode.clock; /* KHz */
	int linkrate[LINKRATE_COUNT] = {LINKRATE_RBR, LINKRATE_HBR, LINKRATE_HBR2, LINKRATE_HBR3};
	int link_count = ARRAY_SIZE(linkrate);

#ifndef FEATURE_SUPPORT_HBR3
	if (max_linkrate == LINKRATE_HBR3) {
		max_linkrate = LINKRATE_HBR2;
		new_linkrate = LINKRATE_HBR2;
		dp_info("HBR3 not support\n");
	}
#endif
	dp->sink_info.link_rate = max_linkrate;
	dp->sink_info.lane_count = lane_count;

	if (dp->sink_info.test_sink)
		return max_linkrate;

	if (max_linkrate == LINKRATE_RBR)
		return max_linkrate;

	for (int i = 0; i < link_count && linkrate[i] <= max_linkrate; i++) {
		u32 pclk = dp_get_pixel_clock(linkrate[i], lane_count);

		if (pclk >= max_pclk) {
			new_linkrate = linkrate[i];
			break;
		}
	}

	/* check if mode does not allow optimization. */
	if (dp_devid_is_ps176(dp) && new_linkrate == LINKRATE_HBR &&
			!dp_mode_is_optimizable(&dp->sink_info.best_mode, max_pclk)) {
		dp_info("not optimize link rate\n");

		return max_linkrate;
	}

	dp->sink_info.link_rate = new_linkrate;
	dp_info("link rate(max:0x%x, new:0x%x) lane count: %d, pclock:%u\n",
		max_linkrate, new_linkrate, lane_count, max_pclk);

	return new_linkrate;
}

void dp_set_branch_info(u8 dfp_type, u8 *ver, char *devid)
{
	struct sec_dp_dev *dp = dp_get_dev();
	static const char * const dfp[] = {"DP", "VGA", "HDMI", "Others"};

	if (dfp_type > 3)
		dfp_type = 3;

	dp_info("DFP type: %s\n", dfp[dfp_type]);
	dp_info("Branch revision: HW(0x%X), SW(0x%X, 0x%X)\n",
		ver[0], ver[1], ver[2]);
	dp_info("devid: %s\n", devid);

	dp->sink_info.sw_ver[0] = ver[1];
	dp->sink_info.sw_ver[1] = ver[2];

	strncpy(dp->sink_info.devid_str, devid, DPCD_DEVID_STR_SIZE);

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_ADAPTER_HWID, ver[0]);
	secdp_bigdata_save_item(BD_ADAPTER_FWVER, (ver[1] << 8) | ver[2]);
	secdp_bigdata_save_item(BD_ADAPTER_TYPE, dfp[dfp_type]);
#endif
}

/* u8 header[4], u8 data[27] */
static void dp_make_spd_infoframe(u32 dpcd_rev, u8 *header, u8 *data)
{
	header[0] = 0x0;
	header[1] = 0x83; //type SPD

	if (dpcd_rev < 0x14) {
		header[2] = 25; // db length
		header[3] = 0x12 << 2; //version

		strncpy(&data[0], "SEC.MCB", 7);
		strncpy(&data[8], "GALAXY", 7);
	} else {
		header[2] = 27; // SDP length
		header[3] = 0x13 << 2; //version
		data[0] = 0x1;
		data[1] = 25; //db length
		strncpy(&data[2], "SEC.MCB", 7);
		strncpy(&data[10], "GALAXY", 7);
	}
}

static int dp_get_aux_level(void)
{
	struct sec_dp_dev *dp = dp_get_dev();
	int level;

	if (dp->aux_level < 0x7 || dp->aux_level > 0xf)
		level = 0x9; //default 0x9;
	else
		level = dp->aux_level;

	dp_info("AUX level: 0x%x\n", level);

	return level;
}

#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
static struct switch_dev switch_secdp_hpd = {
	.name = "hdmi",
};
static struct switch_dev switch_secdp_msg = {
	.name = "secdp_msg",
};
static struct switch_dev switch_secdp_audio = {
	.name = "ch_hdmi_audio",
};

static void dp_set_switch_poor_connect(void)
{
	dp_err("set poor connect switch event\n");
	switch_set_state(&switch_secdp_msg, 1);
	switch_set_state(&switch_secdp_msg, 0);
}

/*for HDMI_PLUGGED intent*/
static void dp_set_switch_hpd_state(int state)
{
	switch_set_state(&switch_secdp_hpd, state);
}

/* for audio, factory */
static void dp_set_switch_audio_state(int state)
{
	switch_set_state(&switch_secdp_audio, state);
	dp_info("set dp audio event %d\n", state);
}

#endif

#ifdef FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
#define DP_HAL_INIT_TIME	30/*sec*/
static void dp_hal_ready_wait(struct sec_dp_dev *dp)
{
	time64_t wait_time = ktime_get_boottime_seconds();

	dp_info("current time is %lld\n", wait_time);

	if (wait_time < DP_HAL_INIT_TIME) {
		wait_time = DP_HAL_INIT_TIME - wait_time;
		dp_info("wait for %lld\n", wait_time);

		wait_event_interruptible_timeout(dp->dp_ready_wait,
		    (dp->dp_ready_wait_state == DP_READY_YES), msecs_to_jiffies(wait_time * 1000));
	}
	dp->dp_ready_wait_state = DP_READY_YES;
}

#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE)
extern int usbpd_manager_get_side_check(void);
#endif
/* return 0 is CC1, 1 is CC2 */
static int dp_get_usb_direction(struct sec_dp_dev *dp)
{
	int dir = 0;

	if (GPIO_IS_VALID(dp->gpio_usb_dir))
		dir = gpio_get_value(dp->gpio_usb_dir);
#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE)
	else
		dir = usbpd_manager_get_side_check() == 1 ? 1 : 0;
#endif
	dp_info("Get direction From PDIC %s\n", !dir ? "CC1" : "CC2");

	return dir;
}

/* dir: 0 is CC1, 1 is CC2 */
void dp_aux_sel(struct sec_dp_dev *dp, int dir)
{
#if IS_ENABLED(CONFIG_SBU_SWITCH_CONTROL)
	usbpd_sbu_switch_control(!dir ? CLOSE_SBU_CC1_ACTIVE : CLOSE_SBU_CC2_ACTIVE);
#else
	if (GPIO_IS_VALID(dp->gpio_sw_sel))
		gpio_direction_output(dp->gpio_sw_sel, dir ? 1 : 0);
#endif
	/* require this delay to reach the stable AUX voltage */
	msleep(200);
}

/* dir: 0 is CC1, 1 is CC2 */
void dp_aux_onoff(struct sec_dp_dev *dp, bool on, int dir)
{
#if IS_ENABLED(CONFIG_SBU_SWITCH_CONTROL)
	if (on)
		dp_aux_sel(dp, dir);
	else
		usbpd_sbu_switch_control(OPEN_SBU);
#else
	if (on)
		dp_aux_sel(dp, dir);

	if (!GPIO_IS_VALID(dp->gpio_sw_oe)) {
		dp_info("gpio_sw_oe is not ready\n");
		return;
	}

	if (on == 1) //active low
		gpio_direction_output(dp->gpio_sw_oe, 0);
	else
		gpio_direction_output(dp->gpio_sw_oe, 1);
#endif
	dp_info("aux switch %s\n", on ? "enable" : "disable");

#ifdef FEATURE_MTK_DRM_DP_ENABLED
	mtk_dp_aux_swap_enable(!dir);
#endif
}

static void dp_mux_set(struct sec_dp_dev *dp, bool en)
{
	void __iomem *mux_addr;

	mux_addr = ioremap(0x10005600, 0x1000);
	if (mux_addr) {
		u32 val = readl(mux_addr);

#ifdef FEATURE_USB_DP_MUX_SET
		if (en) {
			val &= ~(1 << 12);
			if (dp->pdic_usb_dir)
				val |= (1 << 18);
			else
				val &= ~(1 << 18);

			if (dp->pdic_lane_count == 4)
				val |= (1 << 19);
			else
				val &= ~(1 << 19);
			writel(val, mux_addr);
		} else {
			val |= (1 << 12);
			writel(val, mux_addr);
		}
#endif
		dp_info("dp mux reg = 0x%X\n", val);
	}
}

void dp_hpd_changed(struct sec_dp_dev *dp, int hpd)
{
	static int current_hpd = DP_HPD_LOW;

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
	dp_logger_set_max_count(500);
#endif
	dp_info("%s current:%d, new:%d\n", __func__, current_hpd, hpd);

	if (hpd != DP_HPD_IRQ && hpd == current_hpd) {
		current_hpd = hpd;
		return;
	}

	current_hpd = hpd;

	if (hpd == DP_HPD_HIGH)
		dp_mux_set(dp, true);

	if (dp->funcs.redriver_config)
		dp->funcs.redriver_config((hpd == DP_HPD_HIGH), dp->pdic_lane_count);

	if (hpd == DP_HPD_HIGH)
		dp_var_init(dp);

#ifdef FEATURE_MTK_DRM_DP_ENABLED
	mtk_dp_SWInterruptSet(hpd);
#endif
}

static int dp_usb_typec_notification_proceed(struct sec_dp_dev *dp,
				PD_NOTI_TYPEDEF *usb_typec_info)
{
	dp_debug("%s: dump(0x%01x, 0x%01x, 0x%02x, 0x%04x, 0x%04x, 0x%04x)\n",
			__func__, usb_typec_info->src, usb_typec_info->dest, usb_typec_info->id,
			usb_typec_info->sub1, usb_typec_info->sub2, usb_typec_info->sub3);

	switch (usb_typec_info->id) {
	case PDIC_NOTIFY_ID_DP_CONNECT:
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_DETACH:
#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
			dp_logger_set_max_count(100);
#endif
			dp_info("PDIC_NOTIFY_ID_DP_CONNECT, %x\n", usb_typec_info->sub1);
			dp->pdic_link_conf = false;
			dp->pdic_pin_type = 0;
			dp->pdic_hpd = false;
			dp_hpd_changed(dp, DP_HPD_LOW);
			dp_aux_onoff(dp, 0, 0);
			dp_mux_set(dp, false);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_disconnection();
#endif
			break;
		case PDIC_NOTIFY_ATTACH:
			dp_info("PDIC_NOTIFY_ID_DP_CONNECT, %x\n", usb_typec_info->sub1);
			dp->sink_info.ven_id = usb_typec_info->sub2;
			dp->sink_info.prod_id = usb_typec_info->sub3;
			dp_info("VID:0x%04X, PID:0x%04X\n", dp->sink_info.ven_id, dp->sink_info.prod_id);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_connection();
			secdp_bigdata_save_item(BD_ADT_VID, dp->sink_info.ven_id);
			secdp_bigdata_save_item(BD_ADT_PID, dp->sink_info.prod_id);
#endif
			break;
		default:
			break;
		}

		break;

	case PDIC_NOTIFY_ID_DP_LINK_CONF:
		dp_info("PDIC_NOTIFY_ID_DP_LINK_CONF %x\n",
				usb_typec_info->sub1);
		dp->pdic_usb_dir = dp_get_usb_direction(dp);
		dp_aux_onoff(dp, 1, dp->pdic_usb_dir);
		dp->pdic_pin_type = 0;
		dp->pdic_lane_count = 4;
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_LINK_CONFIGURE, usb_typec_info->sub1 + 'A' - 1);
#endif
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_DP_PIN_C:
			dp->pdic_pin_type = DP_USB_PIN_ASSIGNMENT_C;
			break;
		case PDIC_NOTIFY_DP_PIN_D:
			dp->pdic_pin_type = DP_USB_PIN_ASSIGNMENT_D;
			dp->pdic_lane_count = 2;
			break;
		case PDIC_NOTIFY_DP_PIN_E:
			dp->pdic_pin_type = DP_USB_PIN_ASSIGNMENT_E;
			break;
		case PDIC_NOTIFY_DP_PIN_F:
			dp->pdic_pin_type = DP_USB_PIN_ASSIGNMENT_F;
			dp->pdic_lane_count = 2;
			break;
		default:
			dp_err("Unknown DP pin assignment %d\n", usb_typec_info->sub1);
			break;
		}

		if (dp->pdic_pin_type) {
#ifdef FEATURE_MTK_DRM_DP_ENABLED
			mtk_dp_set_pin_assign(dp->pdic_pin_type);
#endif
			dp->pdic_link_conf = true;
			if (dp->pdic_hpd)
				dp_hpd_changed(dp, DP_HPD_HIGH);
		}
		break;

	case PDIC_NOTIFY_ID_DP_HPD:
		dp_info("PDIC_NOTIFY_ID_DP_HPD, %x, %x\n",
				usb_typec_info->sub1, usb_typec_info->sub2);
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_IRQ:
			break;
		case PDIC_NOTIFY_LOW:
			dp->pdic_hpd = false;
			dp_hpd_changed(dp, DP_HPD_LOW);
			break;
		case PDIC_NOTIFY_HIGH:
			if (dp->pdic_hpd && usb_typec_info->sub2 == PDIC_NOTIFY_IRQ) {
				dp_hpd_changed(dp, DP_HPD_IRQ);
			} else {
				dp->pdic_hpd = true;
				if (dp->pdic_link_conf)
					dp_hpd_changed(dp, DP_HPD_HIGH);
			}
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}

	return 0;
}

static void dp_pdic_event_proceed_work(struct work_struct *work)
{
	struct sec_dp_dev *dp = dp_get_dev();
	struct pdic_event *data;

	if (dp->dp_ready_wait_state != DP_READY_YES)
		dp_hal_ready_wait(dp);

	while (!list_empty(&dp->list_pd)) {
		PD_NOTI_TYPEDEF pdic_evt;

		mutex_lock(&dp->pdic_lock);
		data = list_first_entry(&dp->list_pd, typeof(*data), list);
		memcpy(&pdic_evt, &data->event, sizeof(pdic_evt));
		list_del(&data->list);
		kfree(data);
		mutex_unlock(&dp->pdic_lock);
		dp_usb_typec_notification_proceed(dp, &pdic_evt);
		dp_debug("pdic queue done\n");
	}
}

static void dp_pdic_queue_flush(struct sec_dp_dev *dp)
{
	struct pdic_event *data, *next;

	if (list_empty(&dp->list_pd))
		return;

	dp_info("delete hpd event from pdic queue\n");

	mutex_lock(&dp->pdic_lock);
	list_for_each_entry_safe(data, next, &dp->list_pd, list) {
		if (data->event.id == PDIC_NOTIFY_ID_DP_HPD) {
			list_del(&data->list);
			kfree(data);
		}
	}
	mutex_unlock(&dp->pdic_lock);
}

static int dp_usb_typec_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct sec_dp_dev *dp = dp_get_dev();
	PD_NOTI_TYPEDEF usb_typec_info = *(PD_NOTI_TYPEDEF *)data;
	struct pdic_event *pd_data;

	if (usb_typec_info.dest != PDIC_NOTIFY_DEV_DP)
		return 0;

	dp_debug("PDIC action(%ld) dump(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
		action, usb_typec_info.src, usb_typec_info.dest, usb_typec_info.id,
		usb_typec_info.sub1, usb_typec_info.sub2, usb_typec_info.sub3);

	pd_data = kzalloc(sizeof(struct pdic_event), GFP_KERNEL);
	if (!pd_data) {
		dp_err("kzalloc error for pdic event\n");
		return -ENOMEM;
	}

	switch (usb_typec_info.id) {
	case PDIC_NOTIFY_ID_DP_CONNECT:
		dp->pdic_cable_state = usb_typec_info.sub1;
		dp_info("queued CONNECT: %x\n", usb_typec_info.sub1);
		break;
	case PDIC_NOTIFY_ID_DP_LINK_CONF:
		dp_info("queued LINK_CONF: %x\n", usb_typec_info.sub1);
		break;
	case PDIC_NOTIFY_ID_DP_HPD:
		dp_info("queued HPD: %x, %x\n", usb_typec_info.sub1, usb_typec_info.sub2);
		break;
	}

	/* if disconnect or hpd low event come, then flush queue */
	if (((usb_typec_info.id == PDIC_NOTIFY_ID_DP_CONNECT &&
			usb_typec_info.sub1 == PDIC_NOTIFY_DETACH) ||
			(usb_typec_info.id == PDIC_NOTIFY_ID_DP_HPD &&
			usb_typec_info.sub1 == PDIC_NOTIFY_LOW))) {
		dp_pdic_queue_flush(dp);
	};

	memcpy(&pd_data->event, &usb_typec_info, sizeof(usb_typec_info));

	mutex_lock(&dp->pdic_lock);
	list_add_tail(&pd_data->list, &dp->list_pd);
	mutex_unlock(&dp->pdic_lock);

	queue_delayed_work(dp->dp_wq, &dp->pdic_event_proceed_work, 0);

	return 0;
}

static void dp_notifier_register_work(struct work_struct *work)
{
	struct sec_dp_dev *dp = dp_get_dev();

#ifdef FEATURE_MTK_DRM_DP_ENABLED
	mtk_dp_register_sec_dp(dp);
#endif

	if (!dp->notifier_registered) {
		dp->notifier_registered = 1;
		dp_info("notifier registered\n");
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		manager_notifier_register(&dp->dp_typec_nb,
			dp_usb_typec_notification, MANAGER_NOTIFY_PDIC_DP);
#endif
	}
}
#endif //FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
static void dp_parse_redriver(struct sec_dp_dev *dp, struct device_node *np)
{
#define NODE_COUNT (PHY_SWING_MAX * PHY_PREEMPHASIS_MAX)
	struct device_node *redriver_np;
	u8 val[NODE_COUNT];
	u8 (*tuning_table)[PHY_SWING_MAX][PHY_PREEMPHASIS_MAX];
	int ret;
	const char *node_name[DP_TUNE_MAX] = {
		"rbr-eq0",
		"rbr-eq1",
		"hbr-eq0",
		"hbr-eq1",
		"hbr2-eq0",
		"hbr2-eq1",
		"hbr3-eq0",
		"hbr3-eq1"
	};

	if (IS_ERR_OR_NULL(np))
		return;

	redriver_np = of_parse_phandle(np, "redriver_node", 0);
	if (!redriver_np) {
		dp_info("redriver not found\n");
		return;
	}

	for (int i = 0; i < DP_TUNE_MAX; i++) {
		ret = of_property_read_u8_array(redriver_np, node_name[i], val, NODE_COUNT);
		if (ret) {
			dp_err("Failed to read %s\n", node_name[i]);
			break;
		}
		dp_hex_dump("DP redriver: ", val, NODE_COUNT);
		tuning_table = dp_get_redriver_tuning_table();
		memcpy(tuning_table[i], val, NODE_COUNT);
	}
}
#endif

static int dp_parse_dt(struct sec_dp_dev *dp, struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		dp_err("no device tree information\n");
		return -EINVAL;
	}

#if !IS_ENABLED(CONFIG_SBU_SWITCH_CONTROL)
	dp->gpio_sw_oe = of_get_named_gpio(np, "dp,aux_sw_oe", 0);
	if (GPIO_IS_VALID(dp->gpio_sw_oe)) {
		dp_info("dp_aux_sw_oe ok\n");
		gpio_direction_output(dp->gpio_sw_oe, 1);
	} else {
		if (-EPROBE_DEFER != dp->gpio_sw_oe)
			dp_err("dp_aux_sw_oe nok(%d)\n", dp->gpio_sw_oe);
		return dp->gpio_sw_oe;
	}

	dp->gpio_sw_sel = of_get_named_gpio(np, "dp,sbu_sw_sel", 0);
	if (GPIO_IS_VALID(dp->gpio_sw_sel))
		dp_info("dp_sbu_sw_sel ok\n");
	else {
		if (-EPROBE_DEFER != dp->gpio_sw_sel)
			dp_err("dp_sbu_sw_sel nok(%d)\n", dp->gpio_sw_sel);
		return dp->gpio_sw_sel;
	}
#endif
	dp->gpio_usb_dir = of_get_named_gpio(np, "dp,usb_cc_dir", 0);
	if (GPIO_IS_VALID(dp->gpio_usb_dir))
		dp_info("dp_usb_cc_dir ok\n");
	else
		dp_err("(OPTION)none dp_usb_cc_dir (%d)\n", dp->gpio_usb_dir);

	//aux tuning level
	if (of_property_read_u32(np, "dp,aux_tuning_level", &dp->aux_level)) {
		dp_err("set aux level to default 0x9\n");
		dp->aux_level = 0x9;
	} else {
		dp_info("aux tuning level: 0x%x\n", dp->aux_level);
	}

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
	dp_parse_redriver(dp, np);
#endif

	dp_info("parse dt done\n");

	return 0;
}

static void dp_function_register(struct sec_dp_dev *dp)
{
	dp->funcs.get_optimal_linkrate = dp_get_optimal_linkrate;
	dp->funcs.set_branch_info = dp_set_branch_info;
	dp->funcs.parse_edid = dp_parse_edid;
	dp->funcs.get_best_mode = dp_get_best_mode;
	dp->funcs.print_all_modes = dp_print_all_modes;
	dp->funcs.link_training_postprocess = dp_link_training_postprocess;
	dp->funcs.validate_modes = dp_validate_modes;
	dp->funcs.make_spd_infoframe = dp_make_spd_infoframe;
	dp->funcs.reduce_audio_capa = dp_reduce_audio_capa;
	dp->funcs.get_aux_level = dp_get_aux_level;

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
	dp->funcs.logger_print = dp_logger_print;
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
#if IS_ENABLED(CONFIG_COMBO_REDRIVER_PS5169)
	dp->funcs.redriver_config = dp_redriver_config;
	dp->funcs.redriver_notify_linkinfo = dp_redriver_notify_linkinfo;
#endif
#endif
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
	dp->funcs.set_switch_poor_connect = dp_set_switch_poor_connect;
	dp->funcs.set_switch_hpd_state = dp_set_switch_hpd_state;
	dp->funcs.set_switch_audio_state = dp_set_switch_audio_state;
#endif

}

static int _sec_dp_mtk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sec_dp_dev *dp;
	int ret = 0;

	dp_debug("%s(%d)\n", __func__, __LINE__);

	if (g_sec_dp) {
		dp_err("already initialized\n");
		return -EINVAL;
	}

	dp = devm_kmalloc(dev, sizeof(struct sec_dp_dev), GFP_KERNEL | __GFP_ZERO);
	if (!dp) {
		dp_err("failed to allocate sec_dp_dev\n");
		return -ENOMEM;
	}
	g_sec_dp = dp;

	dp_parse_dt(dp, pdev);

#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
	ret = switch_dev_register(&switch_secdp_msg);
	if (ret)
		dp_err("Failed to register dp msg switch\n");

	ret = switch_dev_register(&switch_secdp_hpd);
	if (ret)
		dp_err("Failed to register dp hpd switch\n");

	ret = switch_dev_register(&switch_secdp_audio);
	if (ret)
		dp_err("Failed to register dp audio switch\n");
#endif

	dp_function_register(dp);

	dp->dp_wq = create_singlethread_workqueue("sec_dp_mtk");
	if (!dp->dp_wq) {
		dp_err("create wq failed.\n");
		return -ENOMEM;
	}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) || IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#ifdef FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
	init_waitqueue_head(&dp->dp_ready_wait);
	INIT_LIST_HEAD(&dp->list_pd);
	INIT_DELAYED_WORK(&dp->pdic_event_proceed_work,
			dp_pdic_event_proceed_work);
	mutex_init(&dp->pdic_lock);
#endif
	INIT_DELAYED_WORK(&dp->notifier_register_work,
			dp_notifier_register_work);
	queue_delayed_work(dp->dp_wq, &dp->notifier_register_work,
			msecs_to_jiffies(10000));
#endif

	ret = dp_create_sysfs();

#ifdef FEATURE_MTK_DRM_DP_ENABLED
	mtk_dp_register_sec_dp(dp);
#endif
	dp->hdcp_enable_connect = false;

	dp_info("%s(%d) done\n", __func__, __LINE__);

	return ret;
}

static int sec_dp_mtk_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = _sec_dp_mtk_probe(pdev);

	return 0;
}

static void sec_dp_mtk_deinit(void)
{
	struct sec_dp_dev *dp = dp_get_dev();

	if (!dp)
		return;

	if (dp->dp_wq)
		destroy_workqueue(dp->dp_wq);

#ifdef FEATURE_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
	mutex_destroy(&dp->pdic_lock);
#endif
}

static int sec_dp_mtk_remove(struct platform_device *pdev)
{
	sec_dp_mtk_deinit();

	return 0;
}

static const struct of_device_id sec_dp_mtk_platform_match_table[] = {
	{
		.compatible = "sec,dp_mtk_platform",
	},
	{},
};

static struct platform_driver sec_dp_mtk_platform_driver = {
	.driver = {
		.name = "sec_dp_mtk_platform",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sec_dp_mtk_platform_match_table,
#endif
	},
	.probe =  sec_dp_mtk_probe,
	.remove = sec_dp_mtk_remove,
};

static int __init sec_dp_mtk_init(void)
{
	int ret = 0;

	dp_info("%s\n", __func__);
	dp_logger_init();
	ret = platform_driver_register(&sec_dp_mtk_platform_driver);

	return ret;
}

static void __exit sec_dp_mtk_exit(void)
{
	platform_driver_unregister(&sec_dp_mtk_platform_driver);
}

module_init(sec_dp_mtk_init);
module_exit(sec_dp_mtk_exit);

MODULE_DESCRIPTION("Samsung Displayport driver for MTK");
MODULE_LICENSE("GPL");
