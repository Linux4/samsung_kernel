/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drm_atomic_helper.h>
#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <video/mipi_display.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include "sprd_dpu.h"
#include "sprd_panel.h"
#include "dsi/sprd_dsi_api.h"
#include "sysfs/sysfs_display.h"
/*Tab A8 code for AX6300DEV-805 by suyurui at 2021/10/9 start*/
#include <linux/touchscreen_info.h>
/*Tab A8 code for AX6300DEV-805 by suyurui at 2021/10/9 end*/
/*Tab A8_T code for  P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
#include <linux/kthread.h>
#include <linux/sprd_dfs_drv.h>
#endif
/*Tab A8_T code for  P221008-02152 by zhawei at 2022/10/27 end*/

#define SPRD_MIPI_DSI_FMT_DSC 0xff
#define SPRD_OLED_DEFAULT_BRIGHTNESS 25
/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 start */
#define CUSTOM_CMD_PAYLOAD_SIZE 2
/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 end */

static DEFINE_MUTEX(panel_lock);

/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
int8_t g_is_ddr_loadon = 0;
int8_t g_needed_loadon_ddr = 0;
const char *ddr_lcdon = "lcdon";
const char *ddr_lcdoff = "lcdoff";
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/
const char *lcd_name;

/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 start */
const char *lcd_noise;
char noise_val;
static char is_init_code = 0;
/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 end */

/*HS03 code for SR-SL6215-01-82 by LiChao at 20210809 start*/
unsigned int tp_gesture = 0;
EXPORT_SYMBOL(tp_gesture);
/*HS03 code for SR-SL6215-01-82 by LiChao at 20210809 end*/
static int __init lcd_name_get(char *str)
{
	if (str != NULL)
		lcd_name = str;
	DRM_INFO("lcd name from uboot: %s\n", lcd_name);
	return 0;
}
__setup("lcd_name=", lcd_name_get);

/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 start */
static int __init lcd_noise_get(char *str)
{
	if (str != NULL){
		lcd_noise = str;
		sscanf(lcd_noise, "%x", &noise_val);
	}
	DRM_INFO("lcd noise from LK cmdline: %s, %x \n", lcd_noise, noise_val);
	return 0;
}
__setup("panel_noise=", lcd_noise_get);
/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 end */

/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
int get_scene_dfs_count(const char *scenario) {

	unsigned int scene_num = 0;
	char *name = 0;
	unsigned int freq = 0;
	unsigned int count = 0;
	unsigned int magic = 0;

	int i = 0;
	int err = 0;

	err = get_scene_num(&scene_num);
	for (i = 0; i < scene_num; i++) {
		err = get_scene_info(&name, &freq, &count, &magic, i);
		DRM_INFO("lcd scene i:%d,name:%s,scenario:%s\n",i,name,scenario);
		if (err == 0 && strcmp(name, scenario) == 0) {
			DRM_INFO("lcd scene i:%d,scenario:%s  count:%d break\n", i, name, count);
			break;
		}
	}
	return count;
}
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/
static inline struct sprd_panel *to_sprd_panel(struct drm_panel *panel)
{
	return container_of(panel, struct sprd_panel, base);
}

static int sprd_panel_send_cmds(struct mipi_dsi_device *dsi,
				const void *data, int size)
{
	struct sprd_panel *panel;
	const struct dsi_cmd_desc *cmds = data;
	u16 len;
	/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 start */
	u8 noise_payload[CUSTOM_CMD_PAYLOAD_SIZE] = {0xF6, 0x3E};
	/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 end */

	if ((cmds == NULL) || (dsi == NULL))
		return -EINVAL;

	panel = mipi_dsi_get_drvdata(dsi);

	while (size > 0) {
		len = (cmds->wc_h << 8) | cmds->wc_l;
		/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 start*/
		if(cmds->data_type == 0xFF){
			if ((tp_gesture != 0) && (panel->info.ddi_need_tp_reset != 0) && (panel->info.ddi_tp_reset_gpio > 0)){
				//TODO: HS03 Set tp reset pin 1
				if (gpio_is_valid(panel->info.ddi_tp_reset_gpio)){
					DRM_INFO("TP reset in init code\n");
					gpio_set_value(panel->info.ddi_tp_reset_gpio, 1);
				}
			}
		}
		/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 end*/

		/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 start */
		if((noise_val != 0) && (is_init_code == 1) && (cmds->payload[0] == 0xF6)) {
			DRM_INFO("LCM Set REG %x from %x to %x\n", cmds->payload[0], cmds->payload[1], noise_val);
			noise_payload[1] = noise_val;
			if (panel->info.use_dcs) {
				mipi_dsi_dcs_write_buffer(dsi, (const u8 *)noise_payload, len);
			} else {
				mipi_dsi_generic_write(dsi, (const u8 *)noise_payload, len);
			}
		} else {
			if (panel->info.use_dcs) {
				mipi_dsi_dcs_write_buffer(dsi, cmds->payload, len);
			} else {
				mipi_dsi_generic_write(dsi, cmds->payload, len);
			}
		}
		/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 end */
		if (cmds->wait)
			mdelay(cmds->wait);
		cmds = (const struct dsi_cmd_desc *)(cmds->payload + len);
		size -= (len + 4);
	}

	return 0;
}

static int sprd_panel_unprepare(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);
	struct gpio_timing *timing;
	int items, i;

	DRM_INFO("%s()\n", __func__);

	/*HS03 code for SR-SL6215-01-82 by LiChao at 20210809 start*/
	/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 start*/
	if (tp_gesture != 0 && !panel->esd_flag) {
	/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 end*/
		DRM_INFO("%s() Enter TP gesture mode(%d),keep power\n", __func__, tp_gesture);
		return 0;
	}
	/*HS03 code for SR-SL6215-01-82 by LiChao at 20210809 end*/
	/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 start*/
	if (panel->info.power_off_sequence == 1){
		DRM_INFO("HS03 Set lcd reset behind avdd\n");
		if (panel->info.reset_gpio) {
			items = panel->info.rst_off_seq.items;
			timing = panel->info.rst_off_seq.timing;
			for (i = 0; i < items; i++) {
				gpiod_direction_output(panel->info.reset_gpio,
							timing[i].level);
				mdelay(timing[i].delay);
			}
		}
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 start*/
	} else if (panel->info.reset_force_pull_low == 1) {
		if (panel->info.reset_gpio) {
			gpiod_direction_output(panel->info.reset_gpio, 0);
			mdelay(panel->info.reset_delay_vspn_ms);
		}
	}
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 end*/
	/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 end*/

	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 start */
	if (panel->info.avee_gpio) {
		gpiod_direction_output(panel->info.avee_gpio, 0);
		if (panel->info.power_vsn_off_delay) {
			mdelay(panel->info.power_vsn_off_delay);
		} else {
			mdelay(panel->info.power_gpio_delay);
		}
	}

	if (panel->info.avdd_gpio) {
		gpiod_direction_output(panel->info.avdd_gpio, 0);
		if (panel->info.power_vsp_off_delay) {
			mdelay(panel->info.power_vsp_off_delay);
		} else {
			mdelay(5);
		}
	}
	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 end */

	regulator_disable(panel->supply);

	return 0;
}

static int sprd_panel_prepare(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);
	struct gpio_timing *timing;
	int items, i, ret;

	DRM_INFO("HS03 set panel power enter\n");

	/*Tab A8 code for SR-AX6300-01-441 by huangzhongjie at 20211129 start*/
	if (panel->info.reset_low_before_power_delay) {
		gpiod_direction_output(panel->info.reset_gpio, 0);
		mdelay(panel->info.reset_low_before_power_delay);
	}
	/*Tab A8 code for SR-AX6300-01-441 by huangzhongjie at 20211129 end*/

	ret = regulator_enable(panel->supply);
	if (ret < 0)
		DRM_ERROR("enable lcd regulator failed\n");
	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 start */
	/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 start*/
	/* HS03 code for SL6215TDEV-652 by gaoxue at 20221020 start */
	if (tp_gesture == 0 || panel->esd_flag || panel->info.power_vsp_out) {
	/* HS03 code for SL6215TDEV-652 by gaoxue at 20221020 end */
	/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 end*/
		if (panel->info.avdd_gpio) {
			gpiod_direction_output(panel->info.avdd_gpio, 1);
			if (panel->info.power_vsp_on_delay) {
				mdelay(panel->info.power_vsp_on_delay);
			} else {
				mdelay(panel->info.power_gpio_delay);
			}
		}

		if (panel->info.avee_gpio) {
			gpiod_direction_output(panel->info.avee_gpio, 1);
			if (panel->info.power_vsn_on_delay) {
				mdelay(panel->info.power_vsn_on_delay);
			} else {
				mdelay(5);
			}
		}
	}
	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 end */

	/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 start*/
	if (panel->info.reset_gpio) {
		items = panel->info.rst_on_seq.items;
		timing = panel->info.rst_on_seq.timing;
		for (i = 0; i < items; i++) {
			gpiod_direction_output(panel->info.reset_gpio,
						timing[i].level);
			/*Tab A8 code for AX6300DEV-1778 by suyurui at 2021/10/22 start*/
#if defined (CONFIG_TARGET_UMS512_1H10) || defined (CONFIG_TARGET_UMS512_25C10)
			if (i == 2) {
				if (tp_is_used == HXCHIPSET) {
					himax_resume_by_ddi();
				}
			}
#endif
			/*Tab A8 code for AX6300DEV-1778 by suyurui at 2021/10/22 end*/
			mdelay(timing[i].delay);

			if ((panel->info.ddi_need_tp_reset != 0) && (tp_gesture != 0) && (timing[i].level == 0)&& (panel->info.ddi_tp_reset_gpio > 0)){
				//TODO: Set TP reset pin 0
				if (gpio_is_valid(panel->info.ddi_tp_reset_gpio)){
					gpio_set_value(panel->info.ddi_tp_reset_gpio, 0);
				}
			}

		}
	}
	/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 end*/
	/*HS03 code for SL6215DEV-3850 by zhoulingyun at 20211216 start*/
	/*Tab A7 T618 code for SR-AX6189A-01-148 by suyurui at 2021/12/22 start*/
#if defined (CONFIG_TARGET_UMS9230_4H10) || defined (CONFIG_TARGET_UMS512_1H10) || defined (CONFIG_TARGET_UMS512_25C10)
	if (tp_is_used == NVT_TOUCH) {
		nvt_resume_for_earlier();
	}
#endif
	/*Tab A7 T618 code for SR-AX6189A-01-148 by suyurui at 2021/12/22 end*/
	/*HS03 code for SL6215DEV-3850 by zhoulingyun at 20211216 end*/

	DRM_INFO("HS03 set panel power exit\n");
	return 0;
}

void  sprd_panel_enter_doze(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("%s() enter\n", __func__);

	mutex_lock(&panel_lock);

	if (panel->esd_work_pending) {
		cancel_delayed_work_sync(&panel->esd_work);
		panel->esd_work_pending = false;
	}

	sprd_panel_send_cmds(panel->slave,
	       panel->info.cmds[CMD_CODE_DOZE_IN],
	       panel->info.cmds_len[CMD_CODE_DOZE_IN]);

	mutex_unlock(&panel_lock);
}

void  sprd_panel_exit_doze(struct drm_panel *p)
{
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("%s() enter\n", __func__);

	mutex_lock(&panel_lock);

	sprd_panel_send_cmds(panel->slave,
		panel->info.cmds[CMD_CODE_DOZE_OUT],
		panel->info.cmds_len[CMD_CODE_DOZE_OUT]);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	if (panel->info.esd_conf.esd_check_en) {
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
		schedule_delayed_work(&panel->esd_work,
				      msecs_to_jiffies(1000));
		panel->esd_work_pending = true;
	}

	mutex_unlock(&panel_lock);
}

static int sprd_panel_disable(struct drm_panel *p)
{
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
	int err = 0;
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("%s()\n", __func__);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	/*
	 * FIXME:
	 * The cancel work should be executed before DPU stop,
	 * otherwise the esd check will be failed if the DPU
	 * stopped in video mode and the DSI has not change to
	 * CMD mode yet. Since there is no VBLANK timing for
	 * LP cmd transmission.
	 */
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
	if (panel->esd_work_pending) {
		cancel_delayed_work_sync(&panel->esd_work);
		panel->esd_work_pending = false;
	}

	mutex_lock(&panel_lock);

	if (panel->backlight) {
		panel->backlight->props.power = FB_BLANK_POWERDOWN;
		panel->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(panel->backlight);
	}
	/*Tab A8 code for AX6300DEV-174 by fengzhigang at 20210907 start*/
	if (panel->info.deep_sleep_flag == 1) {
		/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 start*/
		if (tp_gesture == 0 && !panel->esd_flag) {
		/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 end*/
			sprd_panel_send_cmds(panel->slave,
			    		 panel->info.cmds[CMD_CODE_DEEP_SLEEP_IN],
			    		 panel->info.cmds_len[CMD_CODE_DEEP_SLEEP_IN]);
		} else {
			sprd_panel_send_cmds(panel->slave,
			    		 panel->info.cmds[CMD_CODE_SLEEP_IN],
			    		 panel->info.cmds_len[CMD_CODE_SLEEP_IN]);
		}
	} else {
		sprd_panel_send_cmds(panel->slave,
			    	 panel->info.cmds[CMD_CODE_SLEEP_IN],
			    	 panel->info.cmds_len[CMD_CODE_SLEEP_IN]);
	}
	/*Tab A8 code for AX6300DEV-174 by fengzhigang at 20210907 end*/
	panel->is_enabled = false;

/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
	//DRM_INFO("scene ddr_lcdoff num:%d \n", err);
	if (g_needed_loadon_ddr) {
		err = scene_dfs_request((char *)ddr_lcdoff);
		if (err) {
			DRM_ERROR("scene lcd off enter fail:%d\n", err);
		} else {
			DRM_INFO("scene lcd off enter success\n");
			scene_exit((char *)ddr_lcdon);
		}
	}
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/

	mutex_unlock(&panel_lock);

	return 0;
}

static int sprd_panel_enable(struct drm_panel *p)
{
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
	int err = 0;
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/
	struct sprd_panel *panel = to_sprd_panel(p);

	DRM_INFO("HS03 set init code enter\n");

	mutex_lock(&panel_lock);

/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
	DRM_INFO("scene g_needed_loadon_ddr :%d \n", g_needed_loadon_ddr);
	if (g_needed_loadon_ddr) {
		err = scene_dfs_request((char *)ddr_lcdon);
		if (err) {
			DRM_ERROR("scene lcd on enter fail:%d\n", err);
		} else {
			DRM_INFO("scene lcd on enter success\n");
			scene_exit((char *)ddr_lcdoff);
		}
	}
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/

	/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 start */
	is_init_code = 1;
	sprd_panel_send_cmds(panel->slave,
			     panel->info.cmds[CMD_CODE_INIT],
			     panel->info.cmds_len[CMD_CODE_INIT]);
	is_init_code = 0;
	/* HS03 code for SL6215DEV-3752 by LiChao at 20211127 end */

	if (panel->backlight) {
		panel->backlight->props.power = FB_BLANK_UNBLANK;
		panel->backlight->props.state &= ~BL_CORE_FBBLANK;
		backlight_update_status(panel->backlight);
	}
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	if (panel->info.esd_conf.esd_check_en) {
		schedule_delayed_work(&panel->esd_work,
				      msecs_to_jiffies(1000));
		panel->esd_work_pending = true;
		panel->esd_work_backup = false;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
	}

	panel->is_enabled = true;
	mutex_unlock(&panel_lock);
	DRM_INFO("HS03 set init code exit\n");

	return 0;
}

static int sprd_panel_get_modes(struct drm_panel *p)
{
	struct drm_display_mode *mode;
	struct sprd_panel *panel = to_sprd_panel(p);
	struct device_node *np = panel->slave->dev.of_node;
	u32 surface_width = 0, surface_height = 0;
	int i, mode_count = 0;

	DRM_INFO("%s()\n", __func__);

	/*
	 * Only include timing0 for preferred mode. if it defines "native-mode"
	 * property in dts, whether lcd timing in dts is in order or reverse
	 * order. it can parse timing0 about func "of_get_drm_display_mode".
	 * so it all matches correctly timimg0 for perferred mode.
	 */
	mode = drm_mode_duplicate(p->drm, &panel->info.mode);
	if (!mode) {
		DRM_ERROR("failed to alloc mode %s\n", panel->info.mode.name);
		return 0;
	}
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(p->connector, mode);
	mode_count++;

	/*
	 * Don't include timing0 for default mode. if lcd timing in dts is in
	 * order, timing0 is the fist one. if lcd timing in dts is reserve
	 * order, timing0 is the last one.
	 */
	for (i = 0; i < panel->info.num_buildin_modes - 1; i++)	{
		mode = drm_mode_duplicate(p->drm,
			&(panel->info.buildin_modes[i]));
		if (!mode) {
			DRM_ERROR("failed to alloc mode %s\n",
				panel->info.buildin_modes[i].name);
			return 0;
		}
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_DEFAULT;
		drm_mode_probed_add(p->connector, mode);
		mode_count++;
	}

	of_property_read_u32(np, "sprd,surface-width", &surface_width);
	of_property_read_u32(np, "sprd,surface-height", &surface_height);
	if (surface_width && surface_height) {
		struct videomode vm = {};

		vm.hactive = surface_width;
		vm.vactive = surface_height;
		vm.pixelclock = surface_width * surface_height * 60;

		mode = drm_mode_create(p->drm);

		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_BUILTIN |
			DRM_MODE_TYPE_CRTC_C;
		mode->vrefresh = 60;
		drm_display_mode_from_videomode(&vm, mode);
		drm_mode_probed_add(p->connector, mode);
		mode_count++;
	}

	p->connector->display_info.width_mm = panel->info.mode.width_mm;
	p->connector->display_info.height_mm = panel->info.mode.height_mm;

	return mode_count;
}
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
static int dfs_lcdon_load_thread(void *data) {
	int err = 0;
	while(1) {
		mdelay(100);
		err = get_scene_dfs_count(ddr_lcdon);
		if (!err) {
			DRM_INFO("scene lcd on count num:%d\n",err);
			g_needed_loadon_ddr = 1;
			err = scene_dfs_request((char *)ddr_lcdon);
			if (err) {
				DRM_ERROR("scene lcd on enter fail:%d\n", err);
			} else {
				DRM_INFO("scene lcd on enter success\n");
				scene_exit((char *)ddr_lcdoff);
			}
			break;
		}
		break;
	}
	return 0;
}
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/

static const struct drm_panel_funcs sprd_panel_funcs = {
	.get_modes = sprd_panel_get_modes,
	.enable = sprd_panel_enable,
	.disable = sprd_panel_disable,
	.prepare = sprd_panel_prepare,
	.unprepare = sprd_panel_unprepare,
};

/* HS03 code for SL6215DEV-185 by LiChao at 20210824 start */
static int sprd_panel_esd_check(struct sprd_panel *panel)
{
	struct panel_info *info = &panel->info;
	struct sprd_dsi *dsi;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	u8 read_val[16] = {0x00};
	int i = 0;
	int j = 0;
	int last_rd_count = 0;

	mutex_lock(&panel_lock);

	if (!panel->base.connector ||
	    !panel->base.connector->encoder ||
	    !panel->base.connector->encoder->crtc ||
	    (panel->base.connector->encoder->crtc->state &&
	    !panel->base.connector->encoder->crtc->state->active)) {
		mutex_unlock(&panel_lock);
		DRM_INFO("skip esd during panel suspend\n");
		return 0;
	}

	dsi = container_of(panel->base.connector, struct sprd_dsi, connector);
	if (!dsi->ctx.is_inited) {
		DRM_WARN("dsi is not initialized, skip esd check\n");
		mutex_unlock(&panel_lock);
		return 0;
	}

	if (!panel->is_enabled) {
		DRM_INFO("panel is not enabled, skip esd check\n");
		mutex_unlock(&panel_lock);
		return 0;
	}

	/* FIXME: we should enable HS cmd tx here */
	for (i = 0; i < info->esd_conf.esd_check_reg_count; i++) {
		/*Tab A8 code for AX6300DEV-3466 by huangzhongjie at 20211126 start*/
		if (panel->info.esd_check_double_reg_mode == 1) {
			sprd_panel_send_cmds(panel->slave,
				panel->info.cmds[CMD_CODE_ESD_CHECK_ENABLE_CMD],
				panel->info.cmds_len[CMD_CODE_ESD_CHECK_ENABLE_CMD]);
			sprd_panel_send_cmds(panel->slave,
				panel->info.cmds[CMD_CODE_ESD_CHECK_READ_MASTER_CMD],
				panel->info.cmds_len[CMD_CODE_ESD_CHECK_READ_MASTER_CMD]);

			mipi_dsi_set_maximum_return_packet_size(panel->slave,
				info->esd_conf.val_len_array[i]);
			mipi_dsi_dcs_read(panel->slave, info->esd_conf.reg_seq[i],
				&read_val, info->esd_conf.val_len_array[i]);

			sprd_panel_send_cmds(panel->slave,
				panel->info.cmds[CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL],
				panel->info.cmds_len[CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL]);

			for (j = 0; j < info->esd_conf.val_len_array[i]; j++) {
				if (read_val[j] != info->esd_conf.val_seq[j + last_rd_count]) {
					DRM_ERROR("esd check failed, read master value 0x%02x != reg value 0x%02x\n",
						read_val[j], info->esd_conf.val_seq[j + last_rd_count]);
					mutex_unlock(&panel_lock);
					return -EINVAL;
				}
			}

			sprd_panel_send_cmds(panel->slave,
				panel->info.cmds[CMD_CODE_ESD_CHECK_READ_SLAVE_CMD],
				panel->info.cmds_len[CMD_CODE_ESD_CHECK_READ_SLAVE_CMD]);

			mipi_dsi_set_maximum_return_packet_size(panel->slave,
				info->esd_conf.val_len_array[i]);
			mipi_dsi_dcs_read(panel->slave, info->esd_conf.reg_seq[i],
				&read_val, info->esd_conf.val_len_array[i]);

			sprd_panel_send_cmds(panel->slave,
				panel->info.cmds[CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL],
				panel->info.cmds_len[CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL]);
			for (j = 0; j < info->esd_conf.val_len_array[i]; j++) {
				if (read_val[j] != info->esd_conf.val_seq[j + last_rd_count]) {
					DRM_ERROR("esd check failed, read slave value 0x%02x != reg value 0x%02x\n",
						read_val[j], info->esd_conf.val_seq[j + last_rd_count]);
					mutex_unlock(&panel_lock);
					return -EINVAL;
				}
			}
			last_rd_count += info->esd_conf.val_len_array[i];
		} else {
			mipi_dsi_set_maximum_return_packet_size(panel->slave,
				info->esd_conf.val_len_array[i]);
			mipi_dsi_dcs_read(panel->slave, info->esd_conf.reg_seq[i],
				&read_val, info->esd_conf.val_len_array[i]);
			for (j = 0; j < info->esd_conf.val_len_array[i]; j++) {
				if (read_val[j] != info->esd_conf.val_seq[j + last_rd_count]) {
					DRM_ERROR("esd check failed, read value 0x%02x != reg value 0x%02x\n",
						read_val[j], info->esd_conf.val_seq[j + last_rd_count]);
					mutex_unlock(&panel_lock);
					return -EINVAL;
				}
			}
			last_rd_count += info->esd_conf.val_len_array[i];
		}
		/*Tab A8 code for AX6300DEV-3466 by huangzhongjie at 20211126 end*/
	}
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

	mutex_unlock(&panel_lock);

	return 0;
}
/* HS03 code for SL6215DEV-185 by LiChao at 20210824 end */

/* HS03 code for SL6215DEV-185 by LiChao at 20210824 start */
static int sprd_panel_te_check(struct sprd_panel *panel)
{
	static int te_wq_inited;
	struct sprd_dpu *dpu;
	struct sprd_dsi *dsi;
	int ret;
	bool irq_occur = false;

	mutex_lock(&panel_lock);

	if (!panel->base.connector ||
	    !panel->base.connector->encoder ||
	    !panel->base.connector->encoder->crtc ||
	    (panel->base.connector->encoder->crtc->state &&
	    !panel->base.connector->encoder->crtc->state->active)) {
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
		mutex_unlock(&panel_lock);
		DRM_INFO("skip esd during panel suspend\n");
		return 0;
	}

	dsi = container_of(panel->base.connector, struct sprd_dsi, connector);
	if (!dsi->ctx.is_inited) {
		DRM_WARN("dsi is not initialized，skip esd check\n");
		/* Tab A8 code for AX6300U-83 by tangzhen at 20231019 start */
		mutex_unlock(&panel_lock);
		/* Tab A8 code for AX6300U-83 by tangzhen at 20231019 end */
		return 0;
	}

	if (!panel->is_enabled) {
		DRM_INFO("panel is not enabled, skip esd check\n");
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
		mutex_unlock(&panel_lock);
		return 0;
	}

	dpu = container_of(panel->base.connector->encoder->crtc,
		struct sprd_dpu, crtc);

	if (!te_wq_inited) {
		init_waitqueue_head(&dpu->ctx.te_wq);
		te_wq_inited = 1;
		dpu->ctx.evt_te = false;
		DRM_INFO("%s init te waitqueue\n", __func__);
	}

	/* DPU TE irq maybe enabled in kernel */
	if (!dpu->ctx.is_inited) {
		mutex_unlock(&panel_lock);
		return 0;
	}

	dpu->ctx.te_check_en = true;

	/* wait for TE interrupt */
	ret = wait_event_interruptible_timeout(dpu->ctx.te_wq,
		dpu->ctx.evt_te, msecs_to_jiffies(500));
	if (!ret) {
		/* double check TE interrupt through dpu_int_raw register */
		if (dpu->core && dpu->core->check_raw_int) {
			down(&dpu->ctx.refresh_lock);
			if (dpu->ctx.is_inited)
				irq_occur = dpu->core->check_raw_int(&dpu->ctx,
					DISPC_INT_TE_MASK);
			up(&dpu->ctx.refresh_lock);
			if (!irq_occur) {
				DRM_ERROR("TE esd timeout.\n");
				ret = -1;
			} else
				DRM_WARN("TE occur, but isr schedule delay\n");
		} else {
			DRM_ERROR("TE esd timeout.\n");
			ret = -1;
		}
	}

	dpu->ctx.te_check_en = false;
	dpu->ctx.evt_te = false;

	mutex_unlock(&panel_lock);

	return ret < 0 ? ret : 0;
}
/* HS03 code for SL6215DEV-185 by LiChao at 20210824 start */

/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
static int sprd_panel_mix_check(struct sprd_panel *panel)
{
	int ret;

	ret = sprd_panel_esd_check(panel);
	if (ret) {
		DRM_ERROR("mix check use read reg with error\n");
		return ret;
	}

	ret = sprd_panel_te_check(panel);
	if (ret) {
		DRM_ERROR("mix check use te signal with error\n");
		return ret;
	}

	return 0;
}
/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

static void sprd_panel_esd_work_func(struct work_struct *work)
{
	struct sprd_panel *panel = container_of(work, struct sprd_panel,
						esd_work.work);
	struct panel_info *info = &panel->info;
	int ret;

	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	if (info->esd_conf.esd_check_mode == ESD_MODE_REG_CHECK) {
		ret = sprd_panel_esd_check(panel);
	} else if (info->esd_conf.esd_check_mode == ESD_MODE_TE_CHECK) {
		ret = sprd_panel_te_check(panel);
/* HS03 code for SR-SL6215-01-1148 by wenghailong at 20220421 start */
#ifdef CONFIG_TARGET_UMS9230_4H10
		if (gcore_tp_esd_fail) {
			ret = -1;
		}
#endif
/* HS03 code for SR-SL6215-01-1148 by wenghailong at 20220421 end */
	} else if (info->esd_conf.esd_check_mode == ESD_MODE_MIX_CHECK) {
		ret = sprd_panel_mix_check(panel);
	} else {
		DRM_ERROR("unknown esd check mode:%d\n", info->esd_conf.esd_check_mode);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
		return;
	}

	if (ret && panel->base.connector && panel->base.connector->encoder) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;

		encoder = panel->base.connector->encoder;
		funcs = encoder->helper_private;
		panel->esd_work_pending = false;

		if (!encoder->crtc || (encoder->crtc->state &&
		    !encoder->crtc->state->active)) {
			DRM_INFO("skip esd recovery during panel suspend\n");
			/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
			panel->esd_work_backup = true;
			/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
			return;
		}
		DRM_INFO("====== esd recovery start ========\n");
		/*Tab A8 code for AX6300DEV-3529 by chenpengbo at 20211204 start*/
		panel->esd_flag = true;
		/*Tab A8 code for AX6300DEV-3529 by chenpengbo at 20211204 end*/
		funcs->disable(encoder);
		/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211110 start*/
		mdelay(100);
		/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211110 end*/
		funcs->enable(encoder);

		if (panel->oled_bdev) {
			DRM_INFO("====== recovery oled backlight ========\n");
			if (panel->oled_bdev->props.brightness) {
				sprd_oled_set_brightness(panel->oled_bdev);
			} else {
				panel->oled_bdev->props.brightness = SPRD_OLED_DEFAULT_BRIGHTNESS;
				sprd_oled_set_brightness(panel->oled_bdev);
			}
		}
		/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
		if (!panel->esd_work_pending && panel->is_enabled) {
			schedule_delayed_work(&panel->esd_work,
					msecs_to_jiffies(info->esd_conf.esd_check_period));
		}
		/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 start*/
		panel->esd_flag = false;
		/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 start*/
		DRM_INFO("======= esd recovery end =========\n");
	} else {
		schedule_delayed_work(&panel->esd_work,
			msecs_to_jiffies(info->esd_conf.esd_check_period));
	}
		/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
}

static int sprd_panel_gpio_request(struct device *dev,
			struct sprd_panel *panel)
{
	panel->info.avdd_gpio = devm_gpiod_get_optional(dev,
					"avdd", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(panel->info.avdd_gpio))
		DRM_WARN("can't get panel avdd gpio: %ld\n",
				 PTR_ERR(panel->info.avdd_gpio));

	panel->info.avee_gpio = devm_gpiod_get_optional(dev,
					"avee", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(panel->info.avee_gpio))
		DRM_WARN("can't get panel avee gpio: %ld\n",
				 PTR_ERR(panel->info.avee_gpio));

	panel->info.reset_gpio = devm_gpiod_get_optional(dev,
					"reset", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(panel->info.reset_gpio))
		DRM_WARN("can't get panel reset gpio: %ld\n",
				 PTR_ERR(panel->info.reset_gpio));

	return 0;
}

static int of_parse_reset_seq(struct device_node *np,
				struct panel_info *info)
{
	struct property *prop;
	int bytes, rc;
	u32 *p;

	prop = of_find_property(np, "sprd,reset-on-sequence", &bytes);
	if (!prop) {
		DRM_ERROR("sprd,reset-on-sequence property not found\n");
		return -EINVAL;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "sprd,reset-on-sequence",
					p, bytes / 4);
	if (rc) {
		DRM_ERROR("parse sprd,reset-on-sequence failed\n");
		kfree(p);
		return rc;
	}

	info->rst_on_seq.items = bytes / 8;
	info->rst_on_seq.timing = (struct gpio_timing *)p;

	prop = of_find_property(np, "sprd,reset-off-sequence", &bytes);
	if (!prop) {
		DRM_ERROR("sprd,reset-off-sequence property not found\n");
		return -EINVAL;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "sprd,reset-off-sequence",
					p, bytes / 4);
	if (rc) {
		DRM_ERROR("parse sprd,reset-off-sequence failed\n");
		kfree(p);
		return rc;
	}

	info->rst_off_seq.items = bytes / 8;
	info->rst_off_seq.timing = (struct gpio_timing *)p;

	return 0;
}

static int of_parse_buildin_modes(struct panel_info *info,
	struct device_node *lcd_node)
{
	int i, rc, num_timings;
	struct device_node *timings_np;


	timings_np = of_get_child_by_name(lcd_node, "display-timings");
	if (!timings_np) {
		DRM_ERROR("%s: can not find display-timings node\n",
			lcd_node->name);
		return -ENODEV;
	}

	num_timings = of_get_child_count(timings_np);
	if (num_timings == 0) {
		/* should never happen, as entry was already found above */
		DRM_ERROR("%s: no timings specified\n", lcd_node->name);
		goto done;
	}

	info->buildin_modes = kzalloc(sizeof(struct drm_display_mode) *
				num_timings, GFP_KERNEL);

	for (i = 0; i < num_timings; i++) {
		rc = of_get_drm_display_mode(lcd_node,
			&info->buildin_modes[i], NULL, i);
		if (rc) {
			DRM_ERROR("get display timing failed\n");
			goto entryfail;
		}

		info->buildin_modes[i].width_mm = info->mode.width_mm;
		info->buildin_modes[i].height_mm = info->mode.height_mm;
		info->buildin_modes[i].vrefresh = info->mode.vrefresh;
	}
	info->num_buildin_modes = num_timings;
	DRM_INFO("info->num_buildin_modes = %d\n", num_timings);
	goto done;

entryfail:
	kfree(info->buildin_modes);
done:
	of_node_put(timings_np);

	return 0;
}

static int of_parse_oled_cmds(struct sprd_oled *oled,
		const void *data, int size)
{
	const struct dsi_cmd_desc *cmds = data;
	struct dsi_cmd_desc *p;
	u16 len;
	int i, total;

	if (cmds == NULL)
		return -EINVAL;

	/*
	 * TODO:
	 * Currently, we only support the same length cmds
	 * for oled brightness level. So we take the first
	 * cmd payload length as all.
	 */
	len = (cmds->wc_h << 8) | cmds->wc_l;
	total =  size / (len + 4);

	p = (struct dsi_cmd_desc *)kzalloc(size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	memcpy(p, cmds, size);
	for (i = 0; i < total; i++) {
		oled->cmds[i] = p;
		p = (struct dsi_cmd_desc *)(p->payload + len);
	}

	oled->cmds_total = total;
	oled->cmd_len = len + 4;

	return 0;
}

int sprd_oled_set_brightness(struct backlight_device *bdev)
{
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
	struct task_struct *tsk_st ;
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/

	int brightness;
	struct sprd_oled *oled = bl_get_data(bdev);
	struct sprd_panel *panel = oled->panel;

	mutex_lock(&panel_lock);
	if (!panel->is_enabled) {
		mutex_unlock(&panel_lock);
		DRM_WARN("oled panel has been powered off\n");
		return -ENXIO;
	}

	brightness = bdev->props.brightness;

	DRM_INFO("%s brightness: %d\n", __func__, brightness);

	sprd_panel_send_cmds(panel->slave,
			     panel->info.cmds[CMD_OLED_REG_LOCK],
			     panel->info.cmds_len[CMD_OLED_REG_LOCK]);

	/*Tab A8 code for SR-AX6300-01-1079 by hehaoran at 20211001 start*/
	if (brightness == 0) {
		sprd_panel_send_cmds(panel->slave,
				     panel->info.cmds[CMD_CODE_PWM_DIMING_OFF],
			             panel->info.cmds_len[CMD_CODE_PWM_DIMING_OFF]);
	}
	/*Tab A8 code for SR-AX6300-01-1079 by hehaoran at 20211001 end*/

	if (oled->cmds_total == 1) {
		if (oled->cmds[0]->wc_l == 3) {
			oled->cmds[0]->payload[1] = brightness >> 8;
			oled->cmds[0]->payload[2] = brightness & 0xFF;
		} else
			oled->cmds[0]->payload[1] = brightness;

		sprd_panel_send_cmds(panel->slave,
			     oled->cmds[0],
			     oled->cmd_len);
	} else
		sprd_panel_send_cmds(panel->slave,
			     oled->cmds[brightness],
			     oled->cmd_len);

	sprd_panel_send_cmds(panel->slave,
			     panel->info.cmds[CMD_OLED_REG_UNLOCK],
			     panel->info.cmds_len[CMD_OLED_REG_UNLOCK]);

/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 start*/
#ifdef CONFIG_TARGET_UMS512_1H10
	if (!g_is_ddr_loadon) {
		// struct task_struct *tsk_st =
		tsk_st = kthread_run(dfs_lcdon_load_thread, NULL,"panel_lcdon_freq_load");
		if (IS_ERR(tsk_st)) {
			PTR_ERR(tsk_st);
			DRM_ERROR("create load lcdon freq thread failed\n");
		}
		g_is_ddr_loadon++;
	}
#endif
/*Tab A8_T code for P221008-02152 by zhawei at 2022/10/27 end*/

	mutex_unlock(&panel_lock);

	return 0;
}

static const struct backlight_ops sprd_oled_backlight_ops = {
	.update_status = sprd_oled_set_brightness,
};

static int sprd_oled_backlight_init(struct sprd_panel *panel)
{
	struct sprd_oled *oled;
	struct device_node *oled_node;
	struct panel_info *info = &panel->info;
	const void *p;
	int bytes, rc;
	u32 temp;

	oled_node = of_get_child_by_name(info->of_node,
				"oled-backlight");
	if (!oled_node)
		return 0;

	oled = devm_kzalloc(&panel->dev,
			sizeof(struct sprd_oled), GFP_KERNEL);
	if (!oled)
		return -ENOMEM;

	oled->bdev = devm_backlight_device_register(&panel->dev,
			"sprd_backlight", &panel->dev, oled,
			&sprd_oled_backlight_ops, NULL);
	if (IS_ERR(oled->bdev)) {
		DRM_ERROR("failed to register oled backlight ops\n");
		return PTR_ERR(oled->bdev);
	}

	panel->oled_bdev = oled->bdev;
	p = of_get_property(oled_node, "brightness-levels", &bytes);
	if (p) {
		info->cmds[CMD_OLED_BRIGHTNESS] = p;
		info->cmds_len[CMD_OLED_BRIGHTNESS] = bytes;
	} else
		DRM_ERROR("can't find brightness-levels property\n");

	p = of_get_property(oled_node, "sprd,reg-lock", &bytes);
	if (p) {
		info->cmds[CMD_OLED_REG_LOCK] = p;
		info->cmds_len[CMD_OLED_REG_LOCK] = bytes;
	} else
		DRM_INFO("can't find sprd,reg-lock property\n");

	p = of_get_property(oled_node, "sprd,reg-unlock", &bytes);
	if (p) {
		info->cmds[CMD_OLED_REG_UNLOCK] = p;
		info->cmds_len[CMD_OLED_REG_UNLOCK] = bytes;
	} else
		DRM_INFO("can't find sprd,reg-unlock property\n");

	rc = of_property_read_u32(oled_node, "default-brightness-level", &temp);
	if (!rc)
		oled->bdev->props.brightness = temp;
	else
		oled->bdev->props.brightness = 25;

	rc = of_property_read_u32(oled_node, "sprd,max-level", &temp);
	if (!rc)
		oled->max_level = temp;
	else
		oled->max_level = 255;

	oled->bdev->props.max_brightness = oled->max_level;
	oled->panel = panel;
	of_parse_oled_cmds(oled,
			panel->info.cmds[CMD_OLED_BRIGHTNESS],
			panel->info.cmds_len[CMD_OLED_BRIGHTNESS]);

	DRM_INFO("%s() ok\n", __func__);

	return 0;
}

static int sprd_i2c_set_brightness(struct backlight_device *bdev)
{
	aw99703_set_brightness(bdev->props.brightness);
	return 0;
}

static const struct backlight_ops sprd_i2c_backlight_ops = {
	.update_status = sprd_i2c_set_brightness,
	.options = 2048,
};

static int sprd_i2c_backlight_init(struct sprd_panel *panel)
{
	struct backlight_properties props;
	struct backlight_device *bl;
	struct panel_info *info = &panel->info;
	struct device_node *backlight_node;
	int rc, temp;

	backlight_node = of_get_child_by_name(info->of_node,
			"i2c-backlight");
	if (!backlight_node)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	rc = of_property_read_u32(backlight_node,
		"default-brightness-level", &temp);
	if (!rc)
		props.brightness = temp;
	else
		props.brightness = 614;

	rc = of_property_read_u32(backlight_node, "sprd,max-level", &temp);
	if (!rc)
		props.max_brightness = temp;
	else
		props.max_brightness = 2047;

	bl = devm_backlight_device_register(&panel->dev,
			"sprd_backlight", &panel->dev, NULL,
			&sprd_i2c_backlight_ops, &props);
	if (IS_ERR(bl)) {
		DRM_ERROR("failed to register i2c backlight device");
		return PTR_ERR(bl);
	}

	return 0;
}

int sprd_panel_parse_lcddtb(struct device_node *lcd_node,
	struct sprd_panel *panel)
{
	u32 val;
	struct panel_info *info = &panel->info;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	int bytes = 0;
	int rc = 0;
	int i = 0;
	int ret = 0;
	const void *p;
	const char *str;

	if (!lcd_node) {
		DRM_ERROR("Lcd node from dtb is Null\n");
		return -ENODEV;
	}
	info->of_node = lcd_node;

	rc = of_property_read_u32(lcd_node, "sprd,dsi-work-mode", &val);
	if (!rc) {
		if (val == SPRD_DSI_MODE_CMD)
			info->mode_flags = 0;
		else if (val == SPRD_DSI_MODE_VIDEO_BURST)
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					   MIPI_DSI_MODE_VIDEO_BURST;
		else if (val == SPRD_DSI_MODE_VIDEO_SYNC_PULSE)
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					   MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
		else if (val == SPRD_DSI_MODE_VIDEO_SYNC_EVENT)
			info->mode_flags = MIPI_DSI_MODE_VIDEO;
	} else {
		DRM_ERROR("dsi work mode is not found! use video mode\n");
		info->mode_flags = MIPI_DSI_MODE_VIDEO |
				   MIPI_DSI_MODE_VIDEO_BURST;
	}

	if (of_property_read_bool(lcd_node, "sprd,dsi-non-continuous-clock"))
		info->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;

	rc = of_property_read_u32(lcd_node, "sprd,dsi-lane-number", &val);
	if (!rc)
		info->lanes = val;
	else
		info->lanes = 4;

	rc = of_property_read_string(lcd_node, "sprd,dsi-color-format", &str);
	if (rc)
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(str, "rgb888"))
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(str, "rgb666"))
		info->format = MIPI_DSI_FMT_RGB666;
	else if (!strcmp(str, "rgb666_packed"))
		info->format = MIPI_DSI_FMT_RGB666_PACKED;
	else if (!strcmp(str, "rgb565"))
		info->format = MIPI_DSI_FMT_RGB565;
	else if (!strcmp(str, "dsc"))
		info->format = SPRD_MIPI_DSI_FMT_DSC;
	else
		DRM_ERROR("dsi-color-format (%s) is not supported\n", str);

	rc = of_property_read_u32(lcd_node, "sprd,width-mm", &val);
	if (!rc)
		info->mode.width_mm = val;
	else
		info->mode.width_mm = 68;

	rc = of_property_read_u32(lcd_node, "sprd,height-mm", &val);
	if (!rc)
		info->mode.height_mm = val;
	else
		info->mode.height_mm = 121;

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-enable", &val);
	if (!rc) {
		info->esd_conf.esd_check_en = val;
	}

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-mode", &val);
	if (!rc) {
		info->esd_conf.esd_check_mode = val;
	} else {
		info->esd_conf.esd_check_mode = 1;
	}

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-period", &val);
	if (!rc) {
		info->esd_conf.esd_check_period = val;
	} else {
		info->esd_conf.esd_check_period = 1000;
	}

	rc = of_property_read_u32(lcd_node, "sprd,esd-check-reg-count", &val);
	if (!rc) {
		info->esd_conf.esd_check_reg_count = val;
	} else {
		info->esd_conf.esd_check_reg_count = 1;
	}

	/*Tab A8 code for by huangzhongjie at 20211126 start*/
	rc = of_property_read_u32(lcd_node, "sprd,esd-check-double-reg-mode", &val);
	if (!rc) {
		info->esd_check_double_reg_mode = val;
	} else {
		info->esd_check_double_reg_mode = 0;
	}

	if (panel->info.esd_check_double_reg_mode) {
		p = of_get_property(lcd_node, "sprd,esd-check-enable-cmd", &bytes);
		if (p) {
			info->cmds[CMD_CODE_ESD_CHECK_ENABLE_CMD] = p;
			info->cmds_len[CMD_CODE_ESD_CHECK_ENABLE_CMD] = bytes;
		} else {
			DRM_ERROR("can't find sprd,esd-check-enable-cmd property\n");
		}

		p = of_get_property(lcd_node, "sprd,esd-check-read-master-cmd", &bytes);
		if (p) {
			info->cmds[CMD_CODE_ESD_CHECK_READ_MASTER_CMD] = p;
			info->cmds_len[CMD_CODE_ESD_CHECK_READ_MASTER_CMD] = bytes;
		} else {
			DRM_ERROR("can't find sprd,esd-check-read-master-cmd property\n");
		}

		p = of_get_property(lcd_node, "sprd,esd-check-read-salve-cmd", &bytes);
		if (p) {
			info->cmds[CMD_CODE_ESD_CHECK_READ_SLAVE_CMD] = p;
			info->cmds_len[CMD_CODE_ESD_CHECK_READ_SLAVE_CMD] = bytes;
		} else {
			DRM_ERROR("can't find sprd,esd-check-read-salve-cmd property\n");
		}

		p = of_get_property(lcd_node, "sprd,esd-check-return-to-normal-cmd", &bytes);
		if (p) {
			info->cmds[CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL] = p;
			info->cmds_len[CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL] = bytes;
		} else {
			DRM_ERROR("can't find sprd,esd-check-return-to-normal-cmd property\n");
		}
	}
	/*Tab A8 code for by huangzhongjie at 20211126 end*/

	/* HS03 code for SR-SL6215-01-124 by LiChao at 20210907 start */
	if (info->esd_conf.esd_check_reg_count) {
		info->esd_conf.reg_seq = kzalloc(((info->esd_conf.esd_check_reg_count + 1) * 4), GFP_KERNEL);
		rc = of_property_read_u8_array(lcd_node, "sprd,esd-check-register",
				info->esd_conf.reg_seq, info->esd_conf.esd_check_reg_count);
		if (rc) {
			ret = -EINVAL;
		}

		info->esd_conf.val_len_array = kzalloc(((info->esd_conf.esd_check_reg_count + 1) * 4),
                                                        GFP_KERNEL);
		rc = of_property_read_u32_array(lcd_node, "sprd,esd-check-value-len",
				info->esd_conf.val_len_array, info->esd_conf.esd_check_reg_count);
		if (rc) {
			ret = -EINVAL;
		}
		for (i = 0; i < info->esd_conf.esd_check_reg_count; i++) {
			info->esd_conf.total_esd_val_count += info->esd_conf.val_len_array[i];
		}

		info->esd_conf.val_seq = kzalloc(((info->esd_conf.total_esd_val_count + 1) * 4), GFP_KERNEL);
		rc = of_property_read_u8_array(lcd_node, "sprd,esd-check-value",
				info->esd_conf.val_seq, info->esd_conf.total_esd_val_count);
		if (rc) {
			ret = -EINVAL;
		}

		if (ret) {
			info->esd_conf.reg_seq[0] = 0x0a;
			info->esd_conf.val_seq[0] = 0x9c;
			info->esd_conf.val_len_array[0] = 1;
			info->esd_conf.total_esd_val_count = 1;
		}
	}
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
	/* HS03 code for SR-SL6215-01-124 by LiChao at 20210907 end */

	/*Tab A8 code for SR-AX6300-01-441 by huangzhongjie at 20211129 start*/
	rc = of_property_read_u32(lcd_node, "sprd,reset-low-before-power-delay", &val);
	if (!rc) {
		info->reset_low_before_power_delay = val;
	} else {
		info->reset_low_before_power_delay = 0;
	}
	/*Tab A8 code for SR-AX6300-01-441 by huangzhongjie at 20211129 end*/

	/* HS03 code for SL6215TDEV-652 by gaoxue at 20221020 start */
	rc = of_property_read_u32(lcd_node, "sprd,power-vsp-out", &val);
	if (!rc) {
		info->power_vsp_out = val;
		DRM_INFO("HS03 get power_vsp_out%d\n", info->power_vsp_out);
	} else {
		info->power_vsp_out = 0;
	}
	/* HS03 code for SL6215TDEV-652 by gaoxue at 20221020 end */

	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 start */
	rc = of_property_read_u32(lcd_node, "sprd,power-vsp-on-delay", &val);
	if (!rc) {
		info->power_vsp_on_delay = val;
	} else {
		info->power_vsp_on_delay = 0;
	}

	rc = of_property_read_u32(lcd_node, "sprd,power-vsn-on-delay", &val);
	if (!rc) {
		info->power_vsn_on_delay = val;
	} else {
		info->power_vsn_on_delay = 0;
	}

	rc = of_property_read_u32(lcd_node, "sprd,power-vsp-off-delay", &val);
	if (!rc) {
		info->power_vsp_off_delay = val;
	} else {
		info->power_vsp_off_delay = 0;
	}

	rc = of_property_read_u32(lcd_node, "sprd,power-vsn-off-delay", &val);
	if (!rc) {
		info->power_vsn_off_delay = val;
	} else {
		info->power_vsn_off_delay = 0;
	}
	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 end */

	rc = of_property_read_u32(lcd_node, "sprd,power-gpio-delay", &val);
	if (!rc)
		info->power_gpio_delay = val;
	else
		info->power_gpio_delay = 5;

	/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 start*/
	rc = of_property_read_u32(lcd_node, "sprd,power-off-sequence", &val);
	if (!rc){
		info->power_off_sequence = val;
		DRM_INFO("HS03 get power_off_sequence %d\n", info->power_off_sequence);
	} else {
		info->power_off_sequence = 0;
	}

	rc = of_property_read_u32(lcd_node, "sprd,ddi-need-tp-reset", &val);
	if (!rc){
		info->ddi_need_tp_reset = val;
		DRM_INFO("HS03 get ddi_need_tp_reset %d\n", info->ddi_need_tp_reset);
	} else {
		info->ddi_need_tp_reset = 0;
	}

	rc = of_property_read_u32(lcd_node, "sprd,ddi-tp-reset-gpio", &val);
	if (!rc){
		info->ddi_tp_reset_gpio = val;
		DRM_INFO("HS03 get ddi-tp-reset-gpio %d\n", info->ddi_need_tp_reset);
	} else {
		info->ddi_tp_reset_gpio = 0;
	}
	/*HS03 code for SR-SL6215-01-118 by LiChao at 20210830 end*/

	if (of_property_read_bool(lcd_node, "sprd,use-dcs-write"))
		info->use_dcs = true;
	else
		info->use_dcs = false;

	rc = of_parse_reset_seq(lcd_node, info);
	if (rc)
		DRM_ERROR("parse lcd reset sequence failed\n");

	p = of_get_property(lcd_node, "sprd,initial-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_INIT] = p;
		info->cmds_len[CMD_CODE_INIT] = bytes;
	} else
		DRM_ERROR("can't find sprd,initial-command property\n");

	p = of_get_property(lcd_node, "sprd,sleep-in-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_IN] = p;
		info->cmds_len[CMD_CODE_SLEEP_IN] = bytes;
	} else
		DRM_ERROR("can't find sprd,sleep-in-command property\n");

	/*Tab A8 code for SR-AX6300-01-1079 by hehaoran at 20211001 start*/
	p = of_get_property(lcd_node, "sprd,pwm-diming-off-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_PWM_DIMING_OFF] = p;
		info->cmds_len[CMD_CODE_PWM_DIMING_OFF] = bytes;
	} else {
		DRM_ERROR("can't find sprd,pwm-diming-off-command property\n");
        }
	/*Tab A8 code for SR-AX6300-01-1079 by hehaoran at 20211001 end*/

	/*Tab A8 code for AX6300DEV-174 by fengzhigang at 20210907 start*/
	p = of_get_property(lcd_node, "sprd,deep-sleep-in-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_DEEP_SLEEP_IN] = p;
		info->cmds_len[CMD_CODE_DEEP_SLEEP_IN] = bytes;
	} else
		DRM_ERROR("can't find sprd,deep-sleep-in-command property\n");
	rc = of_property_read_u32(lcd_node, "sprd,deep-sleep-in", &val);
	if (!rc) {
		info->deep_sleep_flag = val;
	} else {
		info->deep_sleep_flag = 0;
		DRM_ERROR("can't find sprd,deep-sleep-in property\n");
	}
	/*Tab A8 code for AX6300DEV-174 by fengzhigang at 20210907 end*/
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 start*/
	rc = of_property_read_u32(panel->info.of_node, "sprd,reset-delay-vspn-ms", &val);
	if (!rc) {
		panel->info.reset_delay_vspn_ms = val;
	} else {
		panel->info.reset_delay_vspn_ms = 5;
		DRM_ERROR("can't find sprd,reset-delay-vspn-ms property\n");
	}
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 end*/
	p = of_get_property(lcd_node, "sprd,sleep-out-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_OUT] = p;
		info->cmds_len[CMD_CODE_SLEEP_OUT] = bytes;
	} else
		DRM_ERROR("can't find sprd,sleep-out-command property\n");

	p = of_get_property(lcd_node, "sprd,doze-in-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_DOZE_IN] = p;
		info->cmds_len[CMD_CODE_DOZE_IN] = bytes;
	} else
		DRM_INFO("can't find sprd,doze-in-command property\n");

	p = of_get_property(lcd_node, "sprd,doze-out-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_DOZE_OUT] = p;
		info->cmds_len[CMD_CODE_DOZE_OUT] = bytes;
	} else
		DRM_INFO("can't find sprd,doze-out-command property\n");

	rc = of_get_drm_display_mode(lcd_node, &info->mode, 0,
				     OF_USE_NATIVE_MODE);
	if (rc) {
		DRM_ERROR("get display timing failed\n");
		return rc;
	}

	info->mode.vrefresh = drm_mode_vrefresh(&info->mode);
	of_parse_buildin_modes(info, lcd_node);

	return 0;
}

static int sprd_panel_parse_dt(struct device_node *np, struct sprd_panel *panel)
{
	struct device_node *lcd_node;
	int rc;
	const char *str;
	char lcd_path[60];

	rc = of_property_read_string(np, "sprd,force-attached", &str);
	if (!rc)
		lcd_name = str;

	sprintf(lcd_path, "/lcds/%s", lcd_name);
	lcd_node = of_find_node_by_path(lcd_path);
	if (!lcd_node) {
		DRM_ERROR("%pOF: could not find %s node\n", np, lcd_name);
		return -ENODEV;
	}
	rc = sprd_panel_parse_lcddtb(lcd_node, panel);
	if (rc)
		return rc;

	return 0;
}

static int sprd_panel_device_create(struct device *parent,
				    struct sprd_panel *panel)
{
	panel->dev.class = display_class;
	panel->dev.parent = parent;
	panel->dev.of_node = panel->info.of_node;
	dev_set_name(&panel->dev, "panel0");
	dev_set_drvdata(&panel->dev, panel);

	return device_register(&panel->dev);
}

static int sprd_panel_panic_event(struct notifier_block *self, unsigned long val, void *reason)
{
	struct sprd_panel *panel = container_of(self, struct sprd_panel, panic_nb);

	/*HS03 code for  P221121-04171 by zhawei at 2022/11/23 start*/
	if (!panel->is_enabled) {
		DRM_ERROR("panel is not enabled, skip panic event.\n");
		return 0;
	}
	/*HS03 code for  P221121-04171 by zhawei at 2022/11/23 end*/

	mutex_lock(&panel_lock);
	sprd_panel_send_cmds(panel->slave,
			     panel->info.cmds[CMD_CODE_SLEEP_IN],
			     panel->info.cmds_len[CMD_CODE_SLEEP_IN]);
	mutex_unlock(&panel_lock);

	DRM_ERROR("%s() disable panel because panic event\n", __func__);

	return 0;
}

static struct notifier_block sprd_panel_panic_event_nb = {
	.notifier_call = sprd_panel_panic_event,
	.priority = 128,
};

static int sprd_panel_probe(struct mipi_dsi_device *slave)
{
	int ret;
	struct sprd_panel *panel;
	struct device_node *bl_node;

	panel = devm_kzalloc(&slave->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	bl_node = of_parse_phandle(slave->dev.of_node,
					"sprd,backlight", 0);
	if (bl_node) {
		panel->backlight = of_find_backlight_by_node(bl_node);
		of_node_put(bl_node);

		if (panel->backlight) {
			panel->backlight->props.state &= ~BL_CORE_FBBLANK;
			panel->backlight->props.power = FB_BLANK_UNBLANK;
			backlight_update_status(panel->backlight);
		} else {
			DRM_WARN("backlight is not ready, panel probe deferred\n");
			return -EPROBE_DEFER;
		}
	} else
		DRM_WARN("backlight node not found\n");

	panel->supply = devm_regulator_get(&slave->dev, "power");
	if (IS_ERR(panel->supply)) {
		if (PTR_ERR(panel->supply) == -EPROBE_DEFER)
			DRM_ERROR("regulator driver not initialized, probe deffer\n");
		else
			DRM_ERROR("can't get regulator: %ld\n", PTR_ERR(panel->supply));

		return PTR_ERR(panel->supply);
	}

	INIT_DELAYED_WORK(&panel->esd_work, sprd_panel_esd_work_func);

	ret = sprd_panel_parse_dt(slave->dev.of_node, panel);
	if (ret) {
		DRM_ERROR("parse panel info failed\n");
		return ret;
	}

	ret = sprd_panel_gpio_request(&slave->dev, panel);
	if (ret) {
		DRM_WARN("gpio is not ready, panel probe deferred\n");
		return -EPROBE_DEFER;
	}

	ret = sprd_panel_device_create(&slave->dev, panel);
	if (ret) {
		DRM_ERROR("panel device create failed\n");
		return ret;
	}

	ret = sprd_oled_backlight_init(panel);
	if (ret) {
		DRM_ERROR("oled backlight init failed\n");
		return ret;
	}

	ret = sprd_i2c_backlight_init(panel);
	if (ret) {
		DRM_ERROR("i2c backlight init failed\n");
		return ret;
	}

	panel->base.dev = &panel->dev;
	panel->base.funcs = &sprd_panel_funcs;
	drm_panel_init(&panel->base);

	ret = drm_panel_add(&panel->base);
	if (ret) {
		DRM_ERROR("drm_panel_add() failed\n");
		return ret;
	}

	slave->lanes = panel->info.lanes;
	slave->format = panel->info.format;
	slave->mode_flags = panel->info.mode_flags;

	ret = mipi_dsi_attach(slave);
	if (ret) {
		DRM_ERROR("failed to attach dsi panel to host\n");
		drm_panel_remove(&panel->base);
		return ret;
	}
	panel->slave = slave;

	sprd_panel_sysfs_init(&panel->dev);
	mipi_dsi_set_drvdata(slave, panel);

	/*
	 * FIXME:
	 * The esd check work should not be scheduled in probe
	 * function. It should be scheduled in the enable()
	 * callback function. But the dsi encoder will not call
	 * drm_panel_enable() the first time in encoder_enable().
	 */
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	if (panel->info.esd_conf.esd_check_en) {
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
		schedule_delayed_work(&panel->esd_work,
				      msecs_to_jiffies(2000));
		panel->esd_work_pending = true;
	}

	panel->is_enabled = true;

	panel->panic_nb = sprd_panel_panic_event_nb;
	atomic_notifier_chain_register(&panic_notifier_list, &panel->panic_nb);

	DRM_INFO("panel driver probe success\n");

	return 0;
}

static int sprd_panel_remove(struct mipi_dsi_device *slave)
{
	struct sprd_panel *panel = mipi_dsi_get_drvdata(slave);
	int ret;

	DRM_INFO("%s()\n", __func__);

	sprd_panel_disable(&panel->base);
	sprd_panel_unprepare(&panel->base);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	kfree(panel->info.esd_conf.val_len_array);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
	ret = mipi_dsi_detach(slave);
	if (ret < 0)
		DRM_ERROR("failed to detach from DSI host: %d\n", ret);

	drm_panel_detach(&panel->base);
	drm_panel_remove(&panel->base);

	return 0;
}

/*Tab A8 code for AX6300DEV-1875 by hehaoran at 20211018 start*/
/**
*Name：sprd_panel_shutdown
*Author：hehaoran
*Date�?021.10.18
*Param：struct mipi_dsi_device *slave
*Return：NA
*Purpose：change tp_gesture make vsp and vsn  async off
*/
static void sprd_panel_shutdown(struct mipi_dsi_device *slave)
{
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 start*/
	struct sprd_panel *panel = mipi_dsi_get_drvdata(slave);
	int rc = 0;
	u32 val = 0;

	DRM_INFO("%s()\n", __func__);
	/*Tab A8 code for P211028-05262 by fengzhigang at 20211107 start*/
	panel->info.deep_sleep_flag = 0;
	/*Tab A8 code for P211028-05262 by fengzhigang at 20211107 end*/
	tp_gesture = 0;

	rc = of_property_read_u32(panel->info.of_node, "sprd,reset-force-pull-low", &val);
	if (!rc) {
		panel->info.reset_force_pull_low = val;
	} else {
		panel->info.reset_force_pull_low = 0;
		DRM_ERROR("can't find sprd,reset-force-pull-low  property\n");
	}
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 end*/
}
/*Tab A8 code for AX6300DEV-1875 by hehaoran at 20211018 end*/

static const struct of_device_id panel_of_match[] = {
	{ .compatible = "sprd,generic-mipi-panel", },
	{ }
};
MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver sprd_panel_driver = {
	.driver = {
		.name = "sprd-mipi-panel-drv",
		.of_match_table = panel_of_match,
	},
	.probe = sprd_panel_probe,
	.remove = sprd_panel_remove,
	/*Tab A8 code for AX6300DEV-1875 by hehaoran at 20211018 start*/
	.shutdown = sprd_panel_shutdown,
	/*Tab A8 code for AX6300DEV-1875 by hehaoran at 20211018 end*/
};
module_mipi_dsi_driver(sprd_panel_driver);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD MIPI DSI Panel Driver");
MODULE_LICENSE("GPL v2");
