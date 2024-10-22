// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y) do { } while (0)
#endif
#define MT65XX_LED_MODE_CUST_LCM (4)

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_color.h"
#include "mtk_drm_mmp.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_gamma.h"
#include "mtk_dmdp_aal.h"

#undef pr_fmt
#define pr_fmt(fmt) "[disp_aal]" fmt

static struct drm_device *g_drm_dev;
static int mtk_aal_sof_irq_trigger(void *data);

enum AAL_UPDATE_HIST {
	UPDATE_NONE = 0,
	UPDATE_SINGLE,
	UPDATE_MULTIPLE
};
/* #define DRE3_IN_DISP_AAL */
/* HW specified */
#define AAL_DRE_HIST_START	(1152)
#define AAL_DRE_HIST_END	(4220)
#define AAL_DRE_GAIN_START	(4224)
#define AAL_DRE_GAIN_END	(6396)

#define AAL_SRAM_SOF 1
#define AAL_SRAM_EOF 0
#define aal_min(a, b)			(((a) < (b)) ? (a) : (b))

enum AAL_USER_CMD {
	INIT_REG = 0,
	SET_PARAM,
	FLIP_SRAM,
	BYPASS_AAL,
	SET_CLARITY_REG
};
#define AAL_IRQ_OF_END 0x2
#define AAL_IRQ_IF_END 0x1

static void disp_aal_on_start_of_frame(struct mtk_ddp_comp *comp);

static inline phys_addr_t mtk_aal_dre3_pa(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	return (aal_data->dre3_hw.dev) ? aal_data->dre3_hw.pa : comp->regs_pa;
}

static inline void __iomem *mtk_aal_dre3_va(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	return (aal_data->dre3_hw.dev) ? aal_data->dre3_hw.va : comp->regs;
}

static void mtk_aal_write_mask(void __iomem *address, u32 data, u32 mask)
{
	u32 value = data;

	if (mask != ~0) {
		value = readl(address);
		value &= ~mask;
		data &= mask;
		value |= data;
	}
	writel(value, address);
}

#define AALERR(fmt, arg...) pr_notice("[ERR]%s:" fmt, __func__, ##arg)

static bool debug_flow_log;
#define AALFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define AALAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_write_cmdq_log;
#define AALWC_LOG(fmt, arg...) do { \
	if (debug_write_cmdq_log) \
		pr_notice("[WC]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_irq_log;
#define AALIRQ_LOG(fmt, arg...) do { \
	if (debug_irq_log) \
		pr_notice("[IRQ]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_dump_clarity_regs;
#define AALCRT_LOG(fmt, arg...) do { \
	if (debug_dump_clarity_regs) \
		pr_notice("[Clarity Debug]%s:" fmt, __func__, ##arg); \
	} while (0)

/* config register which might have extra DRE3 aal hw */
static inline s32 basic_cmdq_write(struct cmdq_pkt *handle,
	struct mtk_ddp_comp *comp, u32 offset, u32 value, u32 mask)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		s32 result;
		phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

		result = cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + offset, value, mask);
		if (result) {
			AALERR("write reg fail, offset:%#x\n", offset);
			return result;
		}
		AALWC_LOG("write 0x%03x with 0x%08x (0x%08x)\n",
			offset, value, mask);
		if (aal_data->dre3_hw.dev)
			result = cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + offset, value, mask);
		return result;
	} else {
		return cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + offset, value, mask);
	}
}

#define LOG_INTERVAL_TH 200
#define LOG_BUFFER_SIZE 4
static char g_aal_log_buffer[256] = "";
static int g_aal_log_index;
struct timespec64 g_aal_log_prevtime = {0};

#define AAL_BYPASS_SW 0
#define AAL_BYPASS_HW 1
static int g_aal_bypass_mode;

static void disp_aal_set_interrupt(struct mtk_ddp_comp *comp,
	int enable, int _bypass, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	int bypass = _bypass;

	if (!aal_data->primary_data->aal_fo->mtk_aal_support) {
		AALIRQ_LOG("aal is not support\n");
		return;
	}

	if (aal_data->is_right_pipe)
		return;

	if (bypass < 0)
		bypass = atomic_read(&aal_data->primary_data->force_relay);
	if (enable &&
		(bypass != 1 || pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])) {
		/* Enable output frame end interrupt */
		if (handle)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_INTEN, AAL_IRQ_OF_END, ~0);
		else
			writel(AAL_IRQ_OF_END, comp->regs + DISP_AAL_INTEN);

		atomic_set(&aal_data->primary_data->irq_en, 1);
		AALIRQ_LOG("interrupt enabled\n");
	} else if (!enable) {
		if (handle) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_INTEN, 0x0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_INTSTA, 0x0, ~0);
		} else {
			writel(0x0, comp->regs + DISP_AAL_INTEN);
			writel(0x0, comp->regs + DISP_AAL_INTSTA);
		}
		atomic_set(&aal_data->primary_data->irq_en, 0);
		AALIRQ_LOG("interrupt disabled\n");
	}
}

static unsigned long timevaldiff(struct timespec64 *starttime,
	struct timespec64 *finishtime)
{
	unsigned long msec;

	msec = (finishtime->tv_sec-starttime->tv_sec)*1000;
	msec += (finishtime->tv_nsec-starttime->tv_nsec) / NSEC_PER_USEC / 1000;

	return msec;
}

static void disp_aal_notify_backlight_log(int bl_1024)
{
	struct timespec64 aal_time;
	unsigned long diff_mesc = 0;
	unsigned long tsec;
	unsigned long tusec;

	ktime_get_ts64(&aal_time);
	tsec = (unsigned long)aal_time.tv_sec % 100;
	tusec = (unsigned long)aal_time.tv_nsec / NSEC_PER_USEC / 1000;

	diff_mesc = timevaldiff(&g_aal_log_prevtime, &aal_time);
	if (!debug_api_log)
		return;
	pr_notice("time diff = %lu\n", diff_mesc);

	if (diff_mesc > LOG_INTERVAL_TH) {
		if (g_aal_log_index == 0) {
			pr_notice("%s: %d/1023\n", __func__, bl_1024);
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
					"%s, %d/1023 %03lu.%03lu", __func__,
					bl_1024, tsec, tusec);
			pr_notice("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	} else {
		if (g_aal_log_index == 0) {
			sprintf(g_aal_log_buffer,
					"%s %d/1023 %03lu.%03lu", __func__,
					bl_1024, tsec, tusec);
			g_aal_log_index += 1;
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
					"%s, %d/1023 %03lu.%03lu", __func__,
					bl_1024, tsec, tusec);
			g_aal_log_index += 1;
		}

		if ((g_aal_log_index >= LOG_BUFFER_SIZE) || (bl_1024 == 0)) {
			pr_notice("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	}

	memcpy(&g_aal_log_prevtime, &aal_time, sizeof(struct timespec64));
}

void disp_aal_refresh_by_kernel(struct mtk_disp_aal *aal_data, int need_lock)
{
	struct mtk_ddp_comp *comp = &aal_data->ddp_comp;
	bool delay_trig = atomic_read(&aal_data->primary_data->force_delay_check_trig);

	if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1) {
		if (need_lock)
			DDP_MUTEX_LOCK(&comp->mtk_crtc->lock, __func__, __LINE__);
		atomic_set(&aal_data->primary_data->force_enable_irq, 1);
		if (!atomic_read(&aal_data->primary_data->force_relay)) {
			atomic_set(&aal_data->primary_data->irq_en, 1);
			atomic_set(&aal_data->primary_data->should_stop, 0);
		}

		/*
		 * Backlight or Kernel API latency should be smallest
		 * only need to trigger when calling not from atomic
		 */
		mtk_crtc_check_trigger(comp->mtk_crtc, delay_trig, false);
		if (need_lock)
			DDP_MUTEX_UNLOCK(&comp->mtk_crtc->lock, __func__, __LINE__);
	}
}

void disp_aal_notify_backlight_changed(struct mtk_ddp_comp *comp,
		int trans_backlight, int panel_nits, int max_backlight, int need_lock)
{
	unsigned long flags;
	unsigned int service_flags;
	int prev_backlight;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;

	AALAPI_LOG("bl %d/%d nits %d\n", trans_backlight, max_backlight, panel_nits);
	disp_aal_notify_backlight_log(trans_backlight);
	//disp_aal_exit_idle(__func__, 1);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);
	// FIXME
	//max_backlight = disp_pwm_get_max_backlight(DISP_PWM0);
	AALAPI_LOG("connector_id = %d, max_backlight = %d\n", connector_id, max_backlight);

	if ((max_backlight != -1) && (trans_backlight > max_backlight))
		trans_backlight = max_backlight;

	prev_backlight = atomic_read(&aal_data->primary_data->backlight_notified);
	atomic_set(&aal_data->primary_data->backlight_notified, trans_backlight);
	CRTC_MMP_MARK(0, notify_backlight, trans_backlight, prev_backlight);

	service_flags = 0;
	if ((prev_backlight == 0) && (prev_backlight != trans_backlight))
		service_flags = AAL_SERVICE_FORCE_UPDATE;

	if (trans_backlight == 0) {
		aal_data->primary_data->backlight_set = trans_backlight;

		if (aal_data->primary_data->led_type != TYPE_ATOMIC)
			mtk_leds_brightness_set(connector_id, 0, 0, (0X1<<SET_BACKLIGHT_LEVEL));
		/* set backlight = 0 may be not from AAL, */
		/* we have to let AALService can turn on backlight */
		/* on phone resumption */
		service_flags = AAL_SERVICE_FORCE_UPDATE;
	} else if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 0 ||
		(atomic_read(&aal_data->primary_data->force_relay) == 1 &&
		!pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])) {
		/* AAL Service is not running */

		if (aal_data->primary_data->led_type != TYPE_ATOMIC)
			mtk_leds_brightness_set(connector_id, trans_backlight,
						0, (0X1<<SET_BACKLIGHT_LEVEL));
	}

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);
	aal_data->primary_data->hist.backlight = trans_backlight;
	aal_data->primary_data->hist.panel_nits = panel_nits;
	aal_data->primary_data->hist.serviceFlags |= service_flags;
	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);
	// always notify aal service for LED changed
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, need_lock);

	disp_aal_refresh_by_kernel(aal_data, need_lock);
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int led_brightness_changed_event_to_aal(struct notifier_block *nb, unsigned long event,
	void *v)
{
	int trans_level;
	struct led_conf_info *led_conf;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct pq_common_data *pq_data = NULL;
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	led_conf = (struct led_conf_info *)v;
	if (!led_conf) {
		DDPPR_ERR("%s: led_conf is NULL!\n", __func__);
		return -1;
	}
	crtc = get_crtc_from_connector(led_conf->connector_id, g_drm_dev);
	if (crtc == NULL) {
		led_conf->aal_enable = 0;
		DDPPR_ERR("%s: connector_id(%d) failed to get crtc!\n", __func__,
				led_conf->connector_id);
		return NOTIFY_DONE;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!(mtk_crtc->crtc_caps.crtc_ability & ABILITY_PQ) ||
			atomic_read(&mtk_crtc->pq_data->pipe_info_filled) != 1) {
		DDPINFO("%s, bl %d no need pq, connector_id:%d, crtc_id:%d\n", __func__,
				led_conf->cdev.brightness, led_conf->connector_id, drm_crtc_index(crtc));
		led_conf->aal_enable = 0;
		return NOTIFY_DONE;
	}

	pq_data = mtk_crtc->pq_data;
	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		led_conf->aal_enable = 0;
		DDPINFO("%s: connector_id: %d, crtc_id: %d, has no DISP_AAL comp!\n", __func__,
				led_conf->connector_id, drm_crtc_index(crtc));
		return NOTIFY_DONE;
	}

	switch (event) {
	case LED_BRIGHTNESS_CHANGED:
		trans_level = led_conf->cdev.brightness;

		if (led_conf->led_type == LED_TYPE_ATOMIC)
			break;

		aal_data = comp_to_aal(comp);
		if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS] &&
			(atomic_read(&aal_data->primary_data->force_relay) != 1)) {
			disp_aal_notify_backlight_changed(comp, trans_level, -1,
				led_conf->cdev.max_brightness, 1);
		} else {
			trans_level = (
				led_conf->max_hw_brightness
				* led_conf->cdev.brightness
				+ (led_conf->cdev.max_brightness / 2))
				/ led_conf->cdev.max_brightness;
			if (led_conf->cdev.brightness != 0 &&
				trans_level == 0)
				trans_level = 1;

			disp_aal_notify_backlight_changed(comp, trans_level, -1,
				led_conf->max_hw_brightness, 1);
		}

		AALAPI_LOG("brightness changed: %d(%d)\n",
			trans_level, led_conf->cdev.brightness);
		led_conf->aal_enable = 1;
		break;
	case LED_STATUS_SHUTDOWN:
		if (led_conf->led_type == LED_TYPE_ATOMIC)
			break;

		if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])
			disp_aal_notify_backlight_changed(comp, 0, -1,
				led_conf->cdev.max_brightness, 1);
		else
			disp_aal_notify_backlight_changed(comp, 0, -1,
				led_conf->max_hw_brightness, 1);
		break;
	case LED_TYPE_CHANGED:
		pr_info("[leds -> aal] led type changed: %d", led_conf->led_type);

		aal_data = comp_to_aal(comp);
		aal_data->primary_data->led_type = (unsigned int)led_conf->led_type;

		// force set aal_enable to 1 for ELVSS
		if (led_conf->led_type == LED_TYPE_ATOMIC)
			led_conf->aal_enable = 1;

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = led_brightness_changed_event_to_aal,
};
#endif

int mtk_aal_eventctl_bypass(struct mtk_ddp_comp *comp, int bypass)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	int ret = 0;

	AALFLOW_LOG("set bypass %d\n", bypass);
	if (g_aal_bypass_mode == AAL_BYPASS_HW) {
		atomic_set(&aal_data->primary_data->should_stop, 0);
		if (!bypass && atomic_read(&aal_data->primary_data->force_relay))
			ret = mtk_crtc_user_cmd_impl(&comp->mtk_crtc->base, comp, BYPASS_AAL, &bypass, true);
		if (bypass && !atomic_read(&aal_data->primary_data->force_relay))
			ret = mtk_crtc_user_cmd_impl(&comp->mtk_crtc->base, comp, BYPASS_AAL, &bypass, true);
		if (ret != 0) {
			AALFLOW_LOG("fail to unrelay, set flag first\n");
			atomic_set(&aal_data->primary_data->force_relay, bypass);
			mtk_dmdp_aal_bypass_flag(aal_data->comp_dmdp_aal, bypass);
		}
	} else
		atomic_set(&aal_data->primary_data->should_stop, bypass);
	return 0;
}

static int mtk_drm_ioctl_aal_eventctl_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	int ret = 0;
	unsigned int events = *(unsigned int *)data;
	int bypass = !!(events & AAL_EVENT_STOP);
	int enable = !!(events & AAL_EVENT_EN);
	int delay_trigger;

	AALFLOW_LOG("0x%x\n", events);
	delay_trigger = atomic_read(&aal_data->primary_data->force_delay_check_trig);
	if (enable)
		mtk_crtc_check_trigger(comp->mtk_crtc, delay_trigger, true);
	if (atomic_read(&aal_data->primary_data->force_enable_irq)) {
		enable = 1;
		bypass = 0;
	}
	atomic_set(&aal_data->primary_data->irq_en, enable);
	ret = mtk_aal_eventctl_bypass(comp, bypass);

	return ret;
}

int mtk_drm_ioctl_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_eventctl_impl(comp, data);
}

static void mtk_disp_aal_refresh_trigger(struct work_struct *work_item)
{
	struct work_struct_aal_data *work_data = container_of(work_item,
						struct work_struct_aal_data, task);
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	if (!work_data->data)
		return;
	comp  = (struct mtk_ddp_comp *)work_data->data;
	aal_data = comp_to_aal(work_data->data);

	AALFLOW_LOG("start\n");

	if (atomic_read(&aal_data->primary_data->force_delay_check_trig) == 1)
		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
	else
		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);
}

void disp_aal_flip_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	u32 hist_apb = 0, hist_int = 0, sram_cfg = 0, sram_mask = 0;
	u32 curve_apb = 0, curve_int = 0;
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_curve_sram = aal_data->data->aal_dre3_curve_sram;
	int dre30_write = atomic_read(&aal_data->primary_data->dre30_write);
	atomic_t *curve_sram_apb = &aal_data->force_curve_sram_apb;

	if (!aal_data->primary_data->aal_fo->mtk_dre30_support)
		return;

	if (aal_data->primary_data->sram_method != AAL_SRAM_SOF)
		return;

	if (atomic_read(&aal_data->dre_config) == 1 && !atomic_read(&aal_data->first_frame)) {
		AALFLOW_LOG("[SRAM] g_aal_dre_config not 0 in %s\n", caller);
		return;
	}

	atomic_set(&aal_data->dre_config, 1);
	if (atomic_cmpxchg(&aal_data->force_hist_apb, 0, 1) == 0) {
		hist_apb = 0;
		hist_int = 1;
	} else if (atomic_cmpxchg(&aal_data->force_hist_apb, 1, 0) == 1) {
		hist_apb = 1;
		hist_int = 0;
	} else
		AALERR("[SRAM] Error when get hist_apb in %s\n", caller);

	if (aal_dre3_curve_sram) {
		if (dre30_write) {
			if (atomic_cmpxchg(curve_sram_apb, 0, 1) == 0) {
				curve_apb = 0;
				curve_int = 1;
			} else if (atomic_cmpxchg(curve_sram_apb, 1, 0) == 1) {
				curve_apb = 1;
				curve_int = 0;
			} else
				AALERR("[SRAM] Error when get curve_apb in %s\n", caller);
		} else {
			if (atomic_read(curve_sram_apb) == 0) {
				curve_apb = 1;
				curve_int = 0;
			} else if (atomic_read(curve_sram_apb) == 1) {
				curve_apb = 0;
				curve_int = 1;
			} else
				AALERR("[SRAM] Error when get curve_apb in %s\n", caller);
		}
	}
	SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_HIST_SRAM_EN);
	SET_VAL_MASK(sram_cfg, sram_mask, hist_apb, REG_FORCE_HIST_SRAM_APB);
	SET_VAL_MASK(sram_cfg, sram_mask, hist_int, REG_FORCE_HIST_SRAM_INT);
	if (aal_dre3_curve_sram) {
		SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_CURVE_SRAM_EN);
		SET_VAL_MASK(sram_cfg, sram_mask, curve_apb, REG_FORCE_CURVE_SRAM_APB);
		SET_VAL_MASK(sram_cfg, sram_mask, curve_int, REG_FORCE_CURVE_SRAM_INT);
	}
	AALFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x comp_id[%d] in %s\n",
		hist_apb, hist_int, sram_cfg, comp->id, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_SRAM_CFG, sram_cfg, sram_mask);
}

static void mtk_aal_init(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("+ comd id :%d\n", comp->id);

	if (aal_data->primary_data->aal_fo->mtk_aal_support &&
		atomic_read(&aal_data->primary_data->force_relay) != 1) {
		AALFLOW_LOG("Enable AAL histogram\n");
		// Enable AAL histogram, engine
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x3 << 1, (0x3 << 1));
	} else {
		AALFLOW_LOG("Disable AAL histogram\n");
		// Disable AAL histogram, engine
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x0 << 1, (0x3 << 1));
	}

	atomic_set(&aal_data->hist_available, 0);
	atomic_set(&aal_data->dre20_hist_is_ready, 0);

	atomic_set(&aal_data->primary_data->sof_irq_available, 0);
	atomic_set(&aal_data->eof_irq, 0);
}

static void mtk_disp_aal_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!aal_data->is_right_pipe) {
			aal_data->overhead.comp_overhead = 8;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead += aal_data->overhead.comp_overhead;
			cfg->tile_overhead.left_in_width += aal_data->overhead.comp_overhead;
			/*copy from total overhead info*/
			aal_data->overhead.in_width = cfg->tile_overhead.left_in_width;
			aal_data->overhead.total_overhead = cfg->tile_overhead.left_overhead;
		} else {
			aal_data->overhead.comp_overhead = 8;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead += aal_data->overhead.comp_overhead;
			cfg->tile_overhead.right_in_width += aal_data->overhead.comp_overhead;
			/*copy from total overhead info*/
			aal_data->overhead.in_width = cfg->tile_overhead.right_in_width;
			aal_data->overhead.total_overhead = cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_aal_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	aal_data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		aal_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	aal_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static bool debug_bypass_alg_mode;
static void mtk_aal_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int val = 0, out_val = 0;
	unsigned int overhead_v = 0;
	int width = cfg->w, height = cfg->h;
	int out_width = cfg->w;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (comp->mtk_crtc->is_dual_pipe)
		aal_data->primary_data->isDualPQ = true;
	else
		aal_data->primary_data->isDualPQ = false;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		width = aal_data->overhead.in_width;
		out_width = width - aal_data->overhead.comp_overhead;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;

		out_width = width;
	}

	AALFLOW_LOG("(w,h)=(%d,%d)+, %d\n",
		width, height, aal_data->primary_data->get_size_available);

	aal_data->primary_data->size.height = height;
	aal_data->primary_data->size.width = cfg->w;
	aal_data->primary_data->dual_size.height = height;
	aal_data->primary_data->dual_size.width = cfg->w;
	aal_data->primary_data->size.isdualpipe = aal_data->primary_data->isDualPQ;
	aal_data->primary_data->dual_size.isdualpipe = aal_data->primary_data->isDualPQ;
	if (aal_data->primary_data->get_size_available == false) {
		aal_data->primary_data->get_size_available = true;
		wake_up_interruptible(&aal_data->primary_data->size_wq);
		AALFLOW_LOG("size available: (w,h)=(%d,%d)+\n", width, height);
	}

	if (!aal_data->set_partial_update) {
		val = (width << 16) | (height);
		out_val = (out_width << 16) | height;
	} else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : aal_data->tile_overhead_v.overhead_v;
		val = (width << 16) | (aal_data->roi_height + overhead_v * 2);
		out_val = (out_width << 16) | (aal_data->roi_height + overhead_v * 2);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_SIZE, val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_OUTPUT_SIZE, out_val, ~0);


	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (!aal_data->is_right_pipe) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_OFFSET, 0x0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				(aal_data->primary_data->size.width/2 - 1) << 13, ~0);
		} else {
			struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
				(aal1_data->overhead.comp_overhead << 16) | 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				((aal_data->primary_data->size.width / 2
				+ aal1_data->overhead.total_overhead - 1) << 13)
				| aal1_data->overhead.total_overhead, ~0);
		}

		aal_data->primary_data->dual_size.aaloverhead = aal_data->overhead.total_overhead;
	} else if (comp->mtk_crtc->is_dual_pipe) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
			(0 << 16) | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
			(aal_data->primary_data->size.width/2 - 1) << 13, ~0);

		aal_data->primary_data->dual_size.aaloverhead = 0;
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
			(0 << 16) | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
			(aal_data->primary_data->size.width - 1) << 13, ~0);

		aal_data->primary_data->size.aaloverhead = 0;
	}

	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, (0x1 << 8), (0x1 << 8));
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, (0x0 << 8), (0x1 << 8));
	else
		DDPINFO("Display AAL's bit is : %u\n", cfg->bpc);

	if (atomic_read(&aal_data->primary_data->force_relay) == 1) {
		// Set reply mode
		AALFLOW_LOG("g_aal_force_relay\n");
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 1, 1);
	} else {
		// Disable reply mode
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0, 1);
	}

	if (aal_data->primary_data->aal_fo->mtk_dre30_support
		&& aal_data->primary_data->disp_clarity_support) {
		phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + MDP_AAL_DRE_BILATERAL_STATUS_CTRL,
			0x3 << 1, 0x3 << 1);
	}

	mtk_aal_init(comp, cfg, handle);

	AALWC_LOG("AAL_CFG=0x%x  compid:%d\n",
		readl(comp->regs + DISP_AAL_CFG), comp->id);
}

static void disp_aal_wait_hist(struct mtk_ddp_comp *comp)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (aal_data->primary_data->isDualPQ) {
		struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

		if ((atomic_read(&aal_data->hist_available) == 0) ||
				(atomic_read(&aal1_data->hist_available) == 0)) {
			atomic_set(&aal_data->primary_data->hist_wait_dualpipe, 1);
			ret = wait_event_interruptible(aal_data->primary_data->hist_wq,
					(atomic_read(&aal_data->hist_available) == 1) &&
					(atomic_read(&aal1_data->hist_available) == 1) &&
					comp->mtk_crtc->enabled &&
					!atomic_read(&aal_data->primary_data->should_stop));
		}
		AALFLOW_LOG("aal0 and aal1 hist_available = 1, waken up, ret = %d\n", ret);
	} else if (atomic_read(&aal_data->hist_available) == 0) {
		atomic_set(&aal_data->primary_data->hist_wait_dualpipe, 0);
		AALFLOW_LOG("wait_event_interruptible\n");
		ret = wait_event_interruptible(aal_data->primary_data->hist_wq,
				atomic_read(&aal_data->hist_available) == 1 &&
				comp->mtk_crtc->enabled &&
				!atomic_read(&aal_data->primary_data->should_stop));
		AALFLOW_LOG("hist_available = 1, waken up, ret = %d\n", ret);
	} else
		AALFLOW_LOG("hist_available = 0\n");
}

static void disp_aal_dump_clarity_regs(struct mtk_disp_aal *aal_data, uint32_t pipe)
{
	if (debug_dump_clarity_regs && (pipe == 0)) {
		DDPMSG("%s(aal0): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			aal_data->primary_data->hist.aal0_clarity[0],
			aal_data->primary_data->hist.aal0_clarity[1],
			aal_data->primary_data->hist.aal0_clarity[2],
			aal_data->primary_data->hist.aal0_clarity[3],
			aal_data->primary_data->hist.aal0_clarity[4],
			aal_data->primary_data->hist.aal0_clarity[5],
			aal_data->primary_data->hist.aal0_clarity[6]);
		DDPMSG("%s(tdshp0): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			aal_data->primary_data->hist.tdshp0_clarity[0],
			aal_data->primary_data->hist.tdshp0_clarity[2],
			aal_data->primary_data->hist.tdshp0_clarity[4],
			aal_data->primary_data->hist.tdshp0_clarity[6],
			aal_data->primary_data->hist.tdshp0_clarity[8],
			aal_data->primary_data->hist.tdshp0_clarity[10],
			aal_data->primary_data->hist.tdshp0_clarity[11]);
	} else if (debug_dump_clarity_regs && (pipe == 1)) {
		DDPMSG("%s(aal1): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			aal_data->primary_data->hist.aal1_clarity[0],
			aal_data->primary_data->hist.aal1_clarity[1],
			aal_data->primary_data->hist.aal1_clarity[2],
			aal_data->primary_data->hist.aal1_clarity[3],
			aal_data->primary_data->hist.aal1_clarity[4],
			aal_data->primary_data->hist.aal1_clarity[5],
			aal_data->primary_data->hist.aal1_clarity[6]);
		DDPMSG("%s(tdshp2): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			aal_data->primary_data->hist.tdshp1_clarity[0],
			aal_data->primary_data->hist.tdshp1_clarity[2],
			aal_data->primary_data->hist.tdshp1_clarity[4],
			aal_data->primary_data->hist.tdshp1_clarity[6],
			aal_data->primary_data->hist.tdshp1_clarity[8],
			aal_data->primary_data->hist.tdshp1_clarity[10],
			aal_data->primary_data->hist.tdshp1_clarity[11]);
	}
}

static bool disp_aal_read_single_hist(struct mtk_ddp_comp *comp)
{
	bool read_success = true;
	int i;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *disp_tdshp = aal_data->comp_tdshp;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	if (atomic_read(&aal_data->eof_irq) == 0) {
		AALIRQ_LOG("no eof, skip\n");
		return false;
	}
	if (!aal_data->is_right_pipe) {
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal0_maxHist[i] = readl(comp->regs +
					DISP_AAL_STATUS_00 + (i << 2));
		}
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal0_yHist[i] = readl(comp->regs +
					DISP_Y_HISTOGRAM_00 + (i << 2));
		}
		if (aal_data->data->mdp_aal_ghist_support &&
			aal_data->primary_data->aal_fo->mtk_dre30_support) {
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 1;
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal0_maxHist[i] = readl(dre3_va +
						MDP_AAL_STATUS_00 + (i << 2));
			}
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal0_yHist[i] = readl(dre3_va +
						MDP_Y_HISTOGRAM_00 + (i << 2));
			}
		} else
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 0;
		read_success = disp_color_reg_get(aal_data->comp_color,
				DISP_COLOR_TWO_D_W1_RESULT,
				&aal_data->primary_data->hist.aal0_colorHist);

		// for Display Clarity
		if (aal_data->primary_data->disp_clarity_support) {
			for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.aal0_clarity[i] =
				readl(dre3_va + MDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
			}

			for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.tdshp0_clarity[i] =
				readl(disp_tdshp->regs + MDP_TDSHP_STATUS_00 + (i << 2));
			}

			disp_aal_dump_clarity_regs(aal_data, 0);
		}
	} else {
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal1_maxHist[i] = readl(comp->regs +
					DISP_AAL_STATUS_00 + (i << 2));
		}
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal1_yHist[i] = readl(comp->regs +
					DISP_Y_HISTOGRAM_00 + (i << 2));
		}
		if (aal_data->data->mdp_aal_ghist_support &&
			aal_data->primary_data->aal_fo->mtk_dre30_support) {
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 1;
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal1_maxHist[i] = readl(dre3_va +
						MDP_AAL_STATUS_00 + (i << 2));
			}
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal1_yHist[i] = readl(dre3_va +
						MDP_Y_HISTOGRAM_00 + (i << 2));
			}
		} else
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 0;
		read_success = disp_color_reg_get(aal_data->comp_color,
				DISP_COLOR_TWO_D_W1_RESULT,
				&aal_data->primary_data->hist.aal1_colorHist);

		// for Display Clarity
		if (aal_data->primary_data->disp_clarity_support) {
			for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.aal1_clarity[i] =
				readl(dre3_va + MDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
			}

			for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.tdshp1_clarity[i] =
				readl(disp_tdshp->regs + MDP_TDSHP_STATUS_00 + (i << 2));
			}

			disp_aal_dump_clarity_regs(aal_data, 1);
		}
	}
	return read_success;
}

static int disp_aal_copy_hist_to_user(struct mtk_ddp_comp *comp,
	struct DISP_AAL_HIST *hist)
{
	unsigned long flags;
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (hist == NULL) {
		AALERR("%s DstHist is NULL\n", __func__);
		return -1;
	}

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support
		&& aal_data->primary_data->dre30_enabled)
		memcpy(&aal_data->primary_data->dre30_hist_db,
				&aal_data->primary_data->dre30_hist,
				sizeof(aal_data->primary_data->dre30_hist));

	aal_data->primary_data->hist.panel_type = atomic_read(&aal_data->primary_data->panel_type);
	aal_data->primary_data->hist.essStrengthIndex = aal_data->primary_data->ess_level;
	aal_data->primary_data->hist.ess_enable = aal_data->primary_data->ess_en;
	aal_data->primary_data->hist.dre_enable = aal_data->primary_data->dre_en;
	aal_data->primary_data->hist.fps = aal_data->primary_data->fps;
	AALFLOW_LOG("%s fps:%d\n", __func__, aal_data->primary_data->fps);
	if (aal_data->primary_data->isDualPQ) {
		aal_data->primary_data->hist.pipeLineNum = 2;
		aal_data->primary_data->hist.srcWidth = aal_data->primary_data->dual_size.width;
		aal_data->primary_data->hist.srcHeight = aal_data->primary_data->dual_size.height;
	} else {
		aal_data->primary_data->hist.pipeLineNum = 1;
		aal_data->primary_data->hist.srcWidth = aal_data->primary_data->size.width;
		aal_data->primary_data->hist.srcHeight = aal_data->primary_data->size.height;
	}

	memcpy(&aal_data->primary_data->hist_db, &aal_data->primary_data->hist,
		sizeof(aal_data->primary_data->hist));

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support
		&& aal_data->primary_data->dre30_enabled)
		aal_data->primary_data->hist_db.dre30_hist =
			aal_data->primary_data->init_dre30.dre30_hist_addr;

	memcpy(hist, &aal_data->primary_data->hist_db, sizeof(aal_data->primary_data->hist_db));

	if (aal_data->primary_data->aal_fo->mtk_dre30_support
		&& aal_data->primary_data->dre30_enabled)
		ret = copy_to_user(AAL_U32_PTR(aal_data->primary_data->init_dre30.dre30_hist_addr),
			&aal_data->primary_data->dre30_hist_db,
			sizeof(aal_data->primary_data->dre30_hist_db));

	aal_data->primary_data->hist.serviceFlags = 0;
	atomic_set(&aal_data->hist_available, 0);
	atomic_set(&aal_data->dre20_hist_is_ready, 0);

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

		atomic_set(&aal1_data->hist_available, 0);
		atomic_set(&aal1_data->dre20_hist_is_ready, 0);
	}
	atomic_set(&aal_data->primary_data->force_enable_irq, 0);

	return ret;
}

void dump_hist(struct mtk_ddp_comp *comp, struct DISP_AAL_HIST *data)
{
	int i = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	pr_notice("aal0_maxHist:\n");
	for (i = 0; i < 3; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
			data->aal0_maxHist[i*10 + 0], data->aal0_maxHist[i*10 + 1],
			data->aal0_maxHist[i*10 + 2], data->aal0_maxHist[i*10 + 3],
			data->aal0_maxHist[i*10 + 4], data->aal0_maxHist[i*10 + 5],
			data->aal0_maxHist[i*10 + 6], data->aal0_maxHist[i*10 + 7],
			data->aal0_maxHist[i*10 + 9], data->aal0_maxHist[i*10 + 9]);
	}
	pr_notice("%d %d %d", data->aal0_maxHist[30], data->aal0_maxHist[31],
			data->aal0_maxHist[32]);

	pr_notice("aal0_yHist:\n");
	for (i = 0; i < 3; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
			data->aal0_yHist[i*10 + 0], data->aal0_yHist[i*10 + 1],
			data->aal0_yHist[i*10 + 2], data->aal0_yHist[i*10 + 3],
			data->aal0_yHist[i*10 + 4], data->aal0_yHist[i*10 + 5],
			data->aal0_yHist[i*10 + 6], data->aal0_yHist[i*10 + 7],
			data->aal0_yHist[i*10 + 9], data->aal0_yHist[i*10 + 9]);
	}
	pr_notice("%d %d %d", data->aal0_yHist[30], data->aal0_yHist[31],
			data->aal0_yHist[32]);
	if (aal_data->primary_data->isDualPQ) {
		pr_notice("aal1_maxHist:\n");
		for (i = 0; i < 3; i++) {
			pr_notice("%d %d %d %d %d %d %d %d %d %d",
				data->aal1_maxHist[i*10 + 0], data->aal1_maxHist[i*10 + 1],
				data->aal1_maxHist[i*10 + 2], data->aal1_maxHist[i*10 + 3],
				data->aal1_maxHist[i*10 + 4], data->aal1_maxHist[i*10 + 5],
				data->aal1_maxHist[i*10 + 6], data->aal1_maxHist[i*10 + 7],
				data->aal1_maxHist[i*10 + 9], data->aal1_maxHist[i*10 + 9]);
		}
		pr_notice("%d %d %d", data->aal1_maxHist[30], data->aal1_maxHist[31],
				data->aal1_maxHist[32]);
		pr_notice("aal1_yHist:\n");
		for (i = 0; i < 3; i++) {
			pr_notice("%d %d %d %d %d %d %d %d %d %d",
				data->aal1_yHist[i*10 + 0], data->aal1_yHist[i*10 + 1],
				data->aal1_yHist[i*10 + 2], data->aal1_yHist[i*10 + 3],
				data->aal1_yHist[i*10 + 4], data->aal1_yHist[i*10 + 5],
				data->aal1_yHist[i*10 + 6], data->aal1_yHist[i*10 + 7],
				data->aal1_yHist[i*10 + 9], data->aal1_yHist[i*10 + 9]);
		}
		pr_notice("%d %d %d", data->aal1_yHist[30], data->aal1_yHist[31],
			data->aal1_yHist[32]);
	}

	pr_notice("serviceFlags:%u, backlight: %d, colorHist: %d\n",
			data->serviceFlags, data->backlight, data->aal0_colorHist);
	pr_notice("requestPartial:%d, panel_type: %u\n",
			data->requestPartial, data->panel_type);
	pr_notice("essStrengthIndex:%d, ess_enable: %d, dre_enable: %d\n",
			data->essStrengthIndex, data->ess_enable,
			data->dre_enable);
}

void dump_base_voltage(struct DISP_PANEL_BASE_VOLTAGE *data)
{
	int i = 0;

	if (data->flag) {
		pr_notice("Anodeoffset:\n");
		for (i = 0; i < 23; i++)
			pr_notice("AnodeOffset[%d] = %d\n", i, data->AnodeOffset[i]);

		pr_notice("ELVSSoffset:\n");
		for (i = 0; i < 23; i++)
			pr_notice("ELVSSBase[%d] = %d\n", i, data->ELVSSBase[i]);
	} else
		pr_notice("invalid base voltage\n");
}

static bool debug_dump_aal_hist;
int mtk_drm_ioctl_aal_get_hist_impl(struct mtk_ddp_comp *comp, void *data)
{

	disp_aal_wait_hist(comp);
	if (disp_aal_copy_hist_to_user(comp, (struct DISP_AAL_HIST *) data) < 0)
		return -EFAULT;
	if (debug_dump_aal_hist)
		dump_hist(comp, data);
	return 0;
}

int mtk_drm_ioctl_aal_get_hist(struct drm_device *dev, void *data,
struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_get_hist_impl(comp, data);
}

static void disp_aal_dre3_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,
	const struct DISP_AAL_INITREG *init_regs)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);
	int dre_alg_mode = 1;

	AALFLOW_LOG("start, bitShift: %d  compId%d\n", aal_data->data->bitShift, comp->id);

	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_01,
		(init_regs->dre_blk_y_num << 5) | init_regs->dre_blk_x_num,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_02,
		(init_regs->dre_blk_height << (aal_data->data->bitShift)) |
		init_regs->dre_blk_width, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_04,
		(init_regs->dre_flat_length_slope << 13) |
		init_regs->dre_flat_length_th, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_00,
		(init_regs->dre_s_upper << 24) |
		(init_regs->dre_s_lower << 16) |
		(init_regs->dre_y_upper << 8) | init_regs->dre_y_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_01,
		(init_regs->dre_h_slope << 24) |
		(init_regs->dre_s_slope << 20) |
		(init_regs->dre_y_slope << 16) |
		(init_regs->dre_h_upper << 8) | init_regs->dre_h_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_ALPHA_BLEND_00,
		(init_regs->dre_y_alpha_shift_bit << 25) |
		(init_regs->dre_y_alpha_base << 16) |
		(init_regs->dre_x_alpha_shift_bit << 9) |
		init_regs->dre_x_alpha_base, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_05,
		init_regs->dre_blk_area, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_06,
		init_regs->dre_blk_area_min, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_07,
		(init_regs->height - 1) << (aal_data->data->bitShift), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_SRAM_CFG,
		init_regs->hist_bin_type, 0x1);
#if defined(DRE3_IN_DISP_AAL)
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DUAL_PIPE_INFO_00,
		(0 << 13) | 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DUAL_PIPE_INFO_01,
		((init_regs->dre_blk_x_num-1) << 13) |
		(init_regs->dre_blk_width-1), ~0);
#else
	if (comp->mtk_crtc->is_dual_pipe) {
		if (!aal_data->is_right_pipe) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + MDP_AAL_TILE_00,
				(0x0 << 23) | (0x1 << 22) |
				(0x1 << 21) | (0x1 << 20) |
				(init_regs->dre0_blk_num_x_end << 15) |
				(init_regs->dre0_blk_num_x_start << 10) |
				(init_regs->blk_num_y_end << 5) |
				init_regs->blk_num_y_start, ~0);

			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + MDP_AAL_TILE_01,
				(init_regs->dre0_blk_cnt_x_end << (aal_data->data->bitShift)) |
				init_regs->dre0_blk_cnt_x_start, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				(init_regs->dre0_act_win_x_end << (aal_data->data->bitShift)) |
				init_regs->dre0_act_win_x_start, ~0);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + MDP_AAL_TILE_00,
				(0x1 << 23) | (0x0 << 22) |
				(0x1 << 21) | (0x1 << 20) |
				(init_regs->dre1_blk_num_x_end << 15) |
				(init_regs->dre1_blk_num_x_start << 10) |
				(init_regs->blk_num_y_end << 5) |
				init_regs->blk_num_y_start, ~0);

			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + MDP_AAL_TILE_01,
				(init_regs->dre1_blk_cnt_x_end << (aal_data->data->bitShift)) |
				init_regs->dre1_blk_cnt_x_start << 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				(init_regs->dre1_act_win_x_end << (aal_data->data->bitShift)) |
				init_regs->dre1_act_win_x_start, ~0);
		}
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + MDP_AAL_TILE_00,
			(0x1 << 21) | (0x1 << 20) |
			(init_regs->blk_num_x_end << 15) |
			(init_regs->blk_num_x_start << 10) |
			(init_regs->blk_num_y_end << 5) |
			init_regs->blk_num_y_start, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + MDP_AAL_TILE_01,
			(init_regs->blk_cnt_x_end << (aal_data->data->bitShift)) |
			init_regs->blk_cnt_x_start << 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
			(init_regs->act_win_x_end << (aal_data->data->bitShift)) |
			init_regs->act_win_x_start, ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + MDP_AAL_TILE_02,
		(init_regs->blk_cnt_y_end << (aal_data->data->bitShift)) |
		init_regs->blk_cnt_y_start, ~0);

	mtk_dmdp_aal_primary_data_update(aal_data->comp_dmdp_aal, init_regs);
#endif
	/* Change to Local DRE version */
	if (debug_bypass_alg_mode)
		dre_alg_mode = 0;
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_CFG_MAIN,
		(0x0 << 3) | (dre_alg_mode << 4), (1 << 3) | (1 << 4));

	atomic_or(0x1, &aal_data->primary_data->change_to_dre30);
}

#define CABC_GAINLMT(v0, v1, v2) (((v2) << 20) | ((v1) << 10) | (v0))

static int disp_aal_write_init_regs(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle)
{
	int ret = -EFAULT;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1) {
		struct DISP_AAL_INITREG *init_regs = &aal_data->primary_data->init_regs;

		int i, j = 0;
		int *gain;

		gain = init_regs->cabc_gainlmt;
		basic_cmdq_write(handle, comp, DISP_AAL_DRE_MAPPING_00,
			(init_regs->dre_map_bypass << 4), 1 << 4);

		for (i = 0; i <= 10; i++) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_CABC_GAINLMT_TBL(i),
				CABC_GAINLMT(gain[j], gain[j + 1], gain[j + 2]),
				~0);
			j += 3;
		}

		if (aal_data->primary_data->aal_fo->mtk_dre30_support)
			disp_aal_dre3_config(comp, handle, init_regs);

		AALFLOW_LOG("init done\n");
		ret = 0;
	}

	return ret;
}

#define PRINT_INIT_REG(x1) pr_notice("[INIT]%s=0x%x\n", #x1, data->x1)
void dump_init_reg(struct DISP_AAL_INITREG *data)
{
	PRINT_INIT_REG(dre_s_lower);
	PRINT_INIT_REG(dre_s_upper);
	PRINT_INIT_REG(dre_y_lower);
	PRINT_INIT_REG(dre_y_upper);
	PRINT_INIT_REG(dre_h_lower);
	PRINT_INIT_REG(dre_h_upper);
	PRINT_INIT_REG(dre_h_slope);
	PRINT_INIT_REG(dre_s_slope);
	PRINT_INIT_REG(dre_y_slope);
	PRINT_INIT_REG(dre_x_alpha_base);
	PRINT_INIT_REG(dre_x_alpha_shift_bit);
	PRINT_INIT_REG(dre_y_alpha_base);
	PRINT_INIT_REG(dre_y_alpha_shift_bit);
	PRINT_INIT_REG(dre_blk_x_num);
	PRINT_INIT_REG(dre_blk_y_num);
	PRINT_INIT_REG(dre_blk_height);
	PRINT_INIT_REG(dre_blk_width);
	PRINT_INIT_REG(dre_blk_area);
	PRINT_INIT_REG(dre_blk_area_min);
	PRINT_INIT_REG(hist_bin_type);
	PRINT_INIT_REG(dre_flat_length_slope);
	PRINT_INIT_REG(dre_flat_length_th);
	PRINT_INIT_REG(blk_num_x_start);
	PRINT_INIT_REG(blk_num_x_end);
	PRINT_INIT_REG(dre0_blk_num_x_start);
	PRINT_INIT_REG(dre0_blk_num_x_end);
	PRINT_INIT_REG(dre1_blk_num_x_start);
	PRINT_INIT_REG(dre1_blk_num_x_end);
	PRINT_INIT_REG(blk_cnt_x_start);
	PRINT_INIT_REG(blk_cnt_x_end);
	PRINT_INIT_REG(blk_num_y_start);
	PRINT_INIT_REG(blk_num_y_end);
	PRINT_INIT_REG(blk_cnt_y_start);
	PRINT_INIT_REG(blk_cnt_y_end);
	PRINT_INIT_REG(dre0_blk_cnt_x_start);
	PRINT_INIT_REG(dre0_blk_cnt_x_end);
	PRINT_INIT_REG(dre1_blk_cnt_x_start);
	PRINT_INIT_REG(dre1_blk_cnt_x_end);
	PRINT_INIT_REG(act_win_x_start);
	PRINT_INIT_REG(act_win_x_end);
	PRINT_INIT_REG(dre0_act_win_x_start);
	PRINT_INIT_REG(dre0_act_win_x_end);
	PRINT_INIT_REG(dre1_act_win_x_start);
	PRINT_INIT_REG(dre1_act_win_x_end);
}

static bool debug_dump_init_reg = true;
static int disp_aal_set_init_reg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct DISP_AAL_INITREG *user_regs)
{
	int ret = -EFAULT;
	struct DISP_AAL_INITREG *init_regs;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (!aal_data->primary_data->aal_fo->mtk_aal_support)
		return ret;

	init_regs = &aal_data->primary_data->init_regs;

	memcpy(init_regs, user_regs, sizeof(*init_regs));
	if (debug_dump_init_reg)
		dump_init_reg(init_regs);

	atomic_set(&aal_data->primary_data->is_init_regs_valid, 1);

	AALFLOW_LOG("Set init reg: %lu\n", sizeof(*init_regs));
	AALFLOW_LOG("init_reg.dre_map_bypass:%d\n", init_regs->dre_map_bypass);
	ret = disp_aal_write_init_regs(comp, handle);
	if (comp->mtk_crtc->is_dual_pipe)
		ret = disp_aal_write_init_regs(aal_data->companion, handle);

	AALFLOW_LOG("ret = %d\n", ret);

	return ret;
}

int mtk_drm_ioctl_aal_init_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, INIT_REG, data);
}

int mtk_drm_ioctl_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_init_reg_impl(comp, data);
}

#define DRE_REG_2(v0, off0, v1, off1) (((v1) << (off1)) | \
	((v0) << (off0)))
#define DRE_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) | (v1 << (off1)) | ((v0) << (off0)))

static int disp_aal_write_dre3_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	if (atomic_read(&aal_data->primary_data->change_to_dre30) == 0x3) {
		if (copy_from_user(&aal_data->primary_data->dre30_gain_db,
			      AAL_U32_PTR(param->dre30_gain),
			      sizeof(aal_data->primary_data->dre30_gain_db)) == 0) {

			spin_lock_irqsave(&aal_data->primary_data->dre3_gain_lock, flags);
			memcpy(&aal_data->primary_data->dre30_gain,
				&aal_data->primary_data->dre30_gain_db,
				sizeof(aal_data->primary_data->dre30_gain));
			spin_unlock_irqrestore(&aal_data->primary_data->dre3_gain_lock, flags);
		}
	}

	return 0;
}

static int disp_aal_write_dre_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	const int *gain;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	gain = param->DREGainFltStatus;
	if (aal_data->primary_data->ess20_spect_param.flag & 0x3)
		CRTC_MMP_MARK(0, aal_ess20_curve, comp->id, 0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(0),
	    DRE_REG_2(gain[0], 0, gain[1], 14), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(1),
		DRE_REG_2(gain[2], 0, gain[3], 13), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(2),
		DRE_REG_2(gain[4], 0, gain[5], 12), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(3),
		DRE_REG_2(gain[6], 0, gain[7], 11), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(4),
		DRE_REG_2(gain[8], 0, gain[9], 11), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(5),
		DRE_REG_2(gain[10], 0, gain[11], 11), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(6),
		DRE_REG_3(gain[12], 0, gain[13], 11, gain[14], 22), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(7),
		DRE_REG_3(gain[15], 0, gain[16], 10, gain[17], 20), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(8),
		DRE_REG_3(gain[18], 0, gain[19], 10, gain[20], 20), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(9),
		DRE_REG_3(gain[21], 0, gain[22], 9, gain[23], 18), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(10),
		DRE_REG_3(gain[24], 0, gain[25], 9, gain[26], 18), ~0);
	/* Write dre curve to different register */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(11),
	    DRE_REG_2(gain[27], 0, gain[28], 9), ~0);

	return 0;
}

static int disp_aal_write_cabc_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	int i;
	const int *gain;

	AALFLOW_LOG("\n");
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_CABC_00,
		1 << 31, 1 << 31);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_CABC_02,
		param->cabc_fltgain_force, 0x3ff);

	gain = param->cabc_gainlmt;
	for (i = 0; i <= 10; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CABC_GAINLMT_TBL(i),
			CABC_GAINLMT(gain[0], gain[1], gain[2]), ~0);
		gain += 3;
	}

	return 0;
}

static int disp_aal_write_param_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

// From mt6885, on DRE3.5+ESS mode, ESS function was
// controlled by DREGainFltStatus, not cabc_gainlmt, so need to
// set DREGainFltStatus to hw whether DRE3.5 or 2.5
	disp_aal_write_dre_to_reg(comp, handle, param);
	if (aal_data->primary_data->aal_fo->mtk_dre30_support
		&& aal_data->primary_data->dre30_enabled) {
		disp_aal_write_dre3_to_reg(comp, handle, param);
		disp_aal_write_cabc_to_reg(comp, handle, param);
	} else {
#ifndef NOT_SUPPORT_CABC_HW
		disp_aal_write_cabc_to_reg(comp, handle, param);
#endif
	}

	return 0;
}

void dump_param(const struct DISP_AAL_PARAM *param)
{
	int i = 0;

	pr_notice("DREGainFltStatus: ");
	for (i = 0; i < 2; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
		param->DREGainFltStatus[i*10 + 0],
		param->DREGainFltStatus[i*10 + 1],
		param->DREGainFltStatus[i*10 + 2],
		param->DREGainFltStatus[i*10 + 3],
		param->DREGainFltStatus[i*10 + 4],
		param->DREGainFltStatus[i*10 + 5],
		param->DREGainFltStatus[i*10 + 6],
		param->DREGainFltStatus[i*10 + 7],
		param->DREGainFltStatus[i*10 + 8],
		param->DREGainFltStatus[i*10 + 9]);
	}
	pr_notice("%d %d %d %d %d %d %d %d %d",
		param->DREGainFltStatus[20], param->DREGainFltStatus[21],
		param->DREGainFltStatus[22], param->DREGainFltStatus[23],
		param->DREGainFltStatus[24], param->DREGainFltStatus[25],
		param->DREGainFltStatus[26], param->DREGainFltStatus[27],
		param->DREGainFltStatus[28]);

	pr_notice("cabc_gainlmt: ");
	for (i = 0; i < 3; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
		param->cabc_gainlmt[i*10 + 0],
		param->cabc_gainlmt[i*10 + 1],
		param->cabc_gainlmt[i*10 + 2],
		param->cabc_gainlmt[i*10 + 3],
		param->cabc_gainlmt[i*10 + 4],
		param->cabc_gainlmt[i*10 + 5],
		param->cabc_gainlmt[i*10 + 6],
		param->cabc_gainlmt[i*10 + 7],
		param->cabc_gainlmt[i*10 + 8],
		param->cabc_gainlmt[i*10 + 9]);
	}
	pr_notice("%d %d %d",
		param->cabc_gainlmt[30], param->cabc_gainlmt[31],
		param->cabc_gainlmt[32]);

	pr_notice("cabc_fltgain_force: %d, FinalBacklight: %d",
		param->cabc_fltgain_force, param->FinalBacklight);
	pr_notice("allowPartial: %d, refreshLatency: %d",
		param->allowPartial, param->refreshLatency);
}

static bool debug_dump_input_param;
int disp_aal_set_param(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		struct DISP_AAL_PARAM *param)
{
	int ret = -EFAULT;
	u64 time_use = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (debug_dump_input_param)
		dump_param(&aal_data->primary_data->aal_param);
	//For 120Hz rotation issue
	ktime_get_ts64(&aal_data->primary_data->end);
	time_use = (aal_data->primary_data->end.tv_sec
		- aal_data->primary_data->start.tv_sec) * 1000000
		+ (aal_data->primary_data->end.tv_nsec
		- aal_data->primary_data->start.tv_nsec) / NSEC_PER_USEC;
	//pr_notice("set_param time_use is %lu us\n",time_use);
	// tbd. to be fixd
	if (time_use < 260) {
		// Workaround for 120hz rotation,do not let
		//aal command too fast,else it will merged with
		//DISP commmand and caused trigger loop clear EOF
		//before config loop.The DSI EOF has 100 us later then
		//RDMA EOF,and the worst DISP config time is 153us,
		//so if intervel less than 260 should delay
		usleep_range(260-time_use, 270-time_use);
	}

	ret = disp_aal_write_param_to_reg(comp, handle, &aal_data->primary_data->aal_param);
	if (comp->mtk_crtc->is_dual_pipe) {
		ret = disp_aal_write_param_to_reg(aal_data->companion, handle,
							&aal_data->primary_data->aal_param);
	}
	atomic_set(&aal_data->primary_data->dre30_write, 1);

	return ret;
}

#define PRINT_AAL_REG(x1, x2, x3, x4) \
	pr_notice("[2]0x%x=0x%x 0x%x=0x%x 0x%x=0x%x 0x%x=0x%x\n", \
		x1, readl(comp->regs + x1), x2, readl(comp->regs + x2), \
		x3, readl(comp->regs + x3), x4, readl(comp->regs + x4))

#define PRINT_AAL3_REG(x1, x2, x3, x4) \
	pr_notice("[3]0x%x=0x%x 0x%x=0x%x 0x%x=0x%x 0x%x=0x%x\n", \
		x1, readl(dre3_va + x1), x2, readl(dre3_va + x2), \
		x3, readl(dre3_va + x3), x4, readl(dre3_va + x4))
bool dump_reg(struct mtk_ddp_comp *comp, bool locked)
{
	unsigned long flags = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool dump_success = false;

	if (locked || spin_trylock_irqsave(&aal_data->primary_data->clock_lock, flags)) {
		if (atomic_read(&aal_data->is_clock_on)) {
			PRINT_AAL_REG(0x0, 0x8, 0x10, 0x20);
			PRINT_AAL_REG(0x30, 0xFC, 0x160, 0x200);
			PRINT_AAL_REG(0x204, 0x20C, 0x3B4, 0x45C);
			PRINT_AAL_REG(0x460, 0x464, 0x468, 0x4D8);
			PRINT_AAL_REG(0x4DC, 0x500, 0x224, 0x504);
			if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
				void __iomem *dre3_va = mtk_aal_dre3_va(comp);

				PRINT_AAL3_REG(0x0, 0x8, 0x10, 0x20);
				PRINT_AAL3_REG(0x30, 0x34, 0x38, 0xC4);
				PRINT_AAL3_REG(0xC8, 0xF4, 0xF8, 0x200);
				PRINT_AAL3_REG(0x204, 0x45C, 0x460, 0x464);
				PRINT_AAL3_REG(0x468, 0x46C, 0x470, 0x474);
				PRINT_AAL3_REG(0x478, 0x480, 0x484, 0x488);
				PRINT_AAL3_REG(0x48C, 0x490, 0x494, 0x498);
				PRINT_AAL3_REG(0x49C, 0x4B4, 0x4B8, 0x4BC);
				PRINT_AAL3_REG(0x4D4, 0x4EC, 0x4F0, 0x53C);
			}
			dump_success = true;
		} else
			AALIRQ_LOG("clock is not enabled\n");
		if (!locked)
			spin_unlock_irqrestore(&aal_data->primary_data->clock_lock, flags);
	} else
		AALIRQ_LOG("clock lock is locked\n");
	return dump_success;
}

int mtk_drm_ioctl_aal_set_ess20_spect_param_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	unsigned int flag = 0;
	struct DISP_AAL_ESS20_SPECT_PARAM *param = (struct DISP_AAL_ESS20_SPECT_PARAM *) data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;

	output_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);

	memcpy(&aal_data->primary_data->ess20_spect_param, param, sizeof(*param));
	AALAPI_LOG("[aal_kernel]ELVSSPN = %d, flag = %d\n",
		aal_data->primary_data->ess20_spect_param.ELVSSPN,
		aal_data->primary_data->ess20_spect_param.flag);
	if (aal_data->primary_data->ess20_spect_param.flag & (1 << ENABLE_DYN_ELVSS)) {
		AALAPI_LOG("[aal_kernel]enable dyn elvss, connector_id = %d, flag = %d\n",
				connector_id, aal_data->primary_data->ess20_spect_param.flag);
		flag = 1 << ENABLE_DYN_ELVSS;
		mtk_leds_brightness_set(connector_id, 0, 0, flag);
	}
	return ret;
}

int mtk_drm_ioctl_aal_set_ess20_spect_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_set_ess20_spect_param_impl(comp, data);
}

static bool debug_skip_set_param;
int mtk_drm_ioctl_aal_set_param_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	int prev_backlight = 0;
	int prev_elvsspn = 0;
	struct DISP_AAL_PARAM *param = (struct DISP_AAL_PARAM *) data;
	bool delay_refresh = false;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;

	if (debug_skip_set_param) {
		pr_notice("skip_set_param for debug\n");
		return ret;
	}
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);
	/* Not need to protect g_aal_param, */
	/* since only AALService can set AAL parameters. */
	memcpy(&aal_data->primary_data->aal_param, param, sizeof(*param));

	prev_backlight = aal_data->primary_data->backlight_set;
	prev_elvsspn = aal_data->primary_data->elvsspn_set;
	aal_data->primary_data->backlight_set = aal_data->primary_data->aal_param.FinalBacklight;
	aal_data->primary_data->elvsspn_set = aal_data->primary_data->ess20_spect_param.ELVSSPN;

	mutex_lock(&aal_data->primary_data->sram_lock);
	ret = mtk_crtc_user_cmd(crtc, comp, SET_PARAM, data);
	mutex_unlock(&aal_data->primary_data->sram_lock);

	atomic_set(&aal_data->primary_data->allowPartial,
		aal_data->primary_data->aal_param.allowPartial);

	if (atomic_read(&aal_data->primary_data->backlight_notified) == 0)
		aal_data->primary_data->backlight_set = 0;

	if ((prev_backlight == aal_data->primary_data->backlight_set) ||
		(aal_data->primary_data->led_type == TYPE_ATOMIC))
		aal_data->primary_data->ess20_spect_param.flag &= (~(1 << SET_BACKLIGHT_LEVEL));
	else
		aal_data->primary_data->ess20_spect_param.flag |= (1 << SET_BACKLIGHT_LEVEL);

	if (prev_elvsspn == aal_data->primary_data->elvsspn_set)
		aal_data->primary_data->ess20_spect_param.flag &= (~(1 << SET_ELVSS_PN));
	if (pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (aal_data->primary_data->aal_param.silky_bright_flag == 0) {
			AALAPI_LOG("connector_id:%d, bl:%d, silky_bright_flag:%d, ELVSSPN:%u, flag:%u\n",
				connector_id, aal_data->primary_data->backlight_set,
				aal_data->primary_data->aal_param.silky_bright_flag,
				aal_data->primary_data->ess20_spect_param.ELVSSPN,
				aal_data->primary_data->ess20_spect_param.flag);

			mtk_leds_brightness_set(connector_id,
					aal_data->primary_data->backlight_set,
					aal_data->primary_data->ess20_spect_param.ELVSSPN,
					aal_data->primary_data->ess20_spect_param.flag);
		}
	} else if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS]) {
		//if (pre_bl != cur_bl)
		AALAPI_LOG("gian:%u, backlight:%d, ELVSSPN:%u, flag:%u\n",
			aal_data->primary_data->aal_param.silky_bright_gain[0],
			aal_data->primary_data->backlight_set,
			aal_data->primary_data->ess20_spect_param.ELVSSPN,
			aal_data->primary_data->ess20_spect_param.flag);

		mtk_trans_gain_to_gamma(aal_data->comp_gamma,
			&aal_data->primary_data->aal_param.silky_bright_gain[0],
			aal_data->primary_data->backlight_set,
			(void *)&aal_data->primary_data->ess20_spect_param);
	} else {
		AALAPI_LOG("connector_id:%d, pre_bl:%d, bl:%d, pn:%u, flag:%u\n",
			connector_id, prev_backlight, aal_data->primary_data->backlight_set,
			aal_data->primary_data->ess20_spect_param.ELVSSPN,
			aal_data->primary_data->ess20_spect_param.flag);
		mtk_leds_brightness_set(connector_id, aal_data->primary_data->backlight_set,
					aal_data->primary_data->ess20_spect_param.ELVSSPN,
					aal_data->primary_data->ess20_spect_param.flag);
	}

	AALFLOW_LOG("delay refresh: %d\n", aal_data->primary_data->aal_param.refreshLatency);
	if (aal_data->primary_data->aal_param.refreshLatency == 33)
		delay_refresh = true;
	mtk_crtc_check_trigger(comp->mtk_crtc, delay_refresh, true);
	return ret;
}

int mtk_drm_ioctl_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}

	return mtk_drm_ioctl_aal_set_param_impl(comp, data);
}

static unsigned int disp_aal_read_clear_irq(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;

	/* Check current irq status */
	intsta = readl(comp->regs + DISP_AAL_INTSTA);
	writel(intsta & ~0x3, comp->regs + DISP_AAL_INTSTA);
	AALIRQ_LOG("AAL Module compID:%d\n", comp->id);
	return intsta;
}

static bool debug_skip_dre3_irq;
static bool debug_dump_reg_irq;
static int dump_blk_x = -1;
static int dump_blk_y = -1;

#define AAL_DRE_BLK_NUM			(16)
#define AAL_BLK_MAX_ALLOWED_NUM		(128)
#define AAL_DRE3_POINT_NUM		(17)
#define AAL_DRE_GAIN_POINT16_START	(512)

#define DRE_POLL_SLEEP_TIME_US	(10)
#define DRE_MAX_POLL_TIME_US	(1000)

static inline bool disp_aal_reg_poll(struct mtk_ddp_comp *comp,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	bool return_value = false;
	unsigned int reg_value = 0;
	unsigned int polling_time = 0;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	do {
		reg_value = readl(dre3_va + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(DRE_POLL_SLEEP_TIME_US);
		polling_time += DRE_POLL_SLEEP_TIME_US;
	} while (polling_time < DRE_MAX_POLL_TIME_US);

	return return_value;
}

static bool disp_aal_read_dre3(struct mtk_ddp_comp *comp,
	const int dre_blk_x_num, const int dre_blk_y_num)
{
	int hist_offset;
	int arry_offset = 0;
	unsigned int read_value;
	int dump_start = -1;
	u32 dump_table[6] = {0};
	int i = 0, j = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_auto_inc = aal_data->data->aal_dre3_auto_inc;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Read Global histogram for ESS */
	//if (disp_aal_read_single_hist(comp) != true)
		//return false;
	if (atomic_read(&aal_data->eof_irq) == 0) {
		AALIRQ_LOG("no eof, skip\n");
		return false;
	}
	atomic_set(&aal_data->eof_irq, 0);
	AALIRQ_LOG("start\n");
	if (dump_blk_x >= 0 && dump_blk_x < 16
		&& dump_blk_y >= 0 && dump_blk_y < 8)
		dump_start = 6 * (dump_blk_x + dump_blk_y * 16);

	/* Read Local histogram for DRE 3 */
	hist_offset = aal_data->data->aal_dre_hist_start;
	writel(hist_offset, dre3_va + DISP_AAL_SRAM_RW_IF_2);
	for (; hist_offset <= aal_data->data->aal_dre_hist_end;
			hist_offset += 4) {
		if (!aal_dre3_auto_inc)
			writel(hist_offset, dre3_va + DISP_AAL_SRAM_RW_IF_2);
		read_value = readl(dre3_va + DISP_AAL_SRAM_RW_IF_3);

		if (arry_offset >= AAL_DRE30_HIST_REGISTER_NUM)
			return false;
		if (dump_start >= 0 && arry_offset >= dump_start
			&& arry_offset < (dump_start + 6))
			dump_table[arry_offset-dump_start] = read_value;
		if (aal_data->is_right_pipe) {
			aal_data->primary_data->dre30_hist.aal1_dre_hist[arry_offset++] =
						read_value;
		} else {
			aal_data->primary_data->dre30_hist.aal0_dre_hist[arry_offset++] =
						read_value;
		}
	}

	if (!aal_data->is_right_pipe) {
		for (i = 0; i < 8; i++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe0[i] = readl(dre3_va +
				MDP_AAL_DUAL_PIPE00 + (i << 2));
			}
		for (j = 0; j < 8; j++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe0[j+i] = readl(dre3_va +
				MDP_AAL_DUAL_PIPE08 + (j << 2));
		}
	} else {
		for (i = 0; i < 8; i++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe1[i] = readl(dre3_va +
				MDP_AAL_DUAL_PIPE00 + (i << 2));
		}
		for (j = 0; j < 8; j++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe1[j+i] = readl(dre3_va +
				MDP_AAL_DUAL_PIPE08 + (j << 2));
		}
	}

	if (dump_start >= 0)
		pr_notice("[DRE3][HIST][%d-%d] %08x %08x %08x %08x %08x %08x\n",
			dump_blk_x, dump_blk_y,
			dump_table[0], dump_table[1], dump_table[2],
			dump_table[3], dump_table[4], dump_table[5]);

	return true;
}
static bool disp_aal_write_dre3_v1(struct mtk_ddp_comp *comp)
{
	int gain_offset;
	int arry_offset = 0;
	unsigned int write_value;

	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Write Local Gain Curve for DRE 3 */
	AALIRQ_LOG("start\n");
	writel(aal_data->data->aal_dre_gain_start,
		dre3_va + DISP_AAL_SRAM_RW_IF_0);
	if (disp_aal_reg_poll(comp, DISP_AAL_SRAM_STATUS,
		(0x1 << 16), (0x1 << 16)) != true) {
		AALERR("DISP_AAL_SRAM_STATUS ERROR\n");
		return false;
	}
	for (gain_offset = aal_data->data->aal_dre_gain_start;
		gain_offset <= aal_data->data->aal_dre_gain_end;
			gain_offset += 4) {
		if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		write_value = aal_data->primary_data->dre30_gain.dre30_gain[arry_offset++];
		writel(gain_offset, dre3_va + DISP_AAL_SRAM_RW_IF_0);
		writel(write_value, dre3_va + DISP_AAL_SRAM_RW_IF_1);
	}
	return true;
}

static bool disp_aal_write_dre3_v2(struct mtk_ddp_comp *comp)
{
	int gain_offset;
	int arry_offset = 0;
	unsigned int write_value;

	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Write Local Gain Curve for DRE 3 */
	AALIRQ_LOG("start\n");
	if ((atomic_read(&aal_data->primary_data->dre30_write) != 1) &&
		(atomic_read(&aal_data->dre_hw_prepare) == 0)) {
		AALIRQ_LOG("no need to write dre3\n");
		return true;
	}
	writel(aal_data->data->aal_dre_gain_start, dre3_va + MDP_AAL_CURVE_SRAM_WADDR);
	for (gain_offset = aal_data->data->aal_dre_gain_start;
		gain_offset <= aal_data->data->aal_dre_gain_end;
			gain_offset += 4) {
		if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		write_value = aal_data->primary_data->dre30_gain.dre30_gain[arry_offset++];
		//writel(gain_offset, dre3_va + MDP_AAL_CURVE_SRAM_WADDR);
		writel(write_value, dre3_va + MDP_AAL_CURVE_SRAM_WDATA);
	}
	return true;
}

static bool disp_aal_write_dre3(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool ret = false;

	if (aal_data->data->aal_dre3_curve_sram)
		ret = disp_aal_write_dre3_v2(comp);
	else
		ret = disp_aal_write_dre3_v1(comp);

	return ret;
}

static void disp_aal_update_dre3_sram(struct mtk_ddp_comp *comp,
	 bool check_sram)
{
	bool result = false;
	unsigned long flags;
	int dre_blk_x_num, dre_blk_y_num;
	unsigned int read_value;
	int hist_apb = 0, hist_int = 0;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALIRQ_LOG("[g_aal_first_frame = %d\n", atomic_read(&aal_data->first_frame));
	AALIRQ_LOG("[g_aal0_hist_available = %d\n",
			atomic_read(&aal_data->hist_available));
	AALIRQ_LOG("[g_aal_eof_irq = %d\n", atomic_read(&aal_data->eof_irq));

	// reset g_aal_eof_irq to 0 when first frame,
	// to avoid timing issue
	if (atomic_read(&aal_data->first_frame) == 1)
		atomic_set(&aal_data->eof_irq, 0);

	CRTC_MMP_EVENT_START(0, aal_dre30_rw, comp->id, 0);
	if (check_sram) {
		read_value = readl(dre3_va + DISP_AAL_SRAM_CFG);
		hist_apb = (read_value >> 5) & 0x1;
		hist_int = (read_value >> 6) & 0x1;
		AALIRQ_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in (SOF) compID:%d\n",
			hist_apb, hist_int, read_value, comp->id);
		if (hist_int != atomic_read(&aal_data->force_hist_apb))
			AALIRQ_LOG("dre3: SRAM config %d != %d config?\n",
				hist_int, atomic_read(&aal_data->force_hist_apb));
	}
	read_value = readl(dre3_va + DISP_AAL_DRE_BLOCK_INFO_01);
	dre_blk_x_num = aal_min(AAL_DRE_BLK_NUM, read_value & 0x1F);
	dre_blk_y_num = aal_min(AAL_BLK_MAX_ALLOWED_NUM/dre_blk_x_num,
		    (read_value >> 5) & 0x1F);
	if (spin_trylock_irqsave(&aal_data->primary_data->hist_lock, flags)) {
		result = disp_aal_read_dre3(comp,
			dre_blk_x_num, dre_blk_y_num);
		if (result) {
			aal_data->primary_data->dre30_hist.dre_blk_x_num = dre_blk_x_num;
			aal_data->primary_data->dre30_hist.dre_blk_y_num = dre_blk_y_num;

			atomic_set(&aal_data->hist_available, 1);
		//	if (atomic_read(&aal_data->primary_data->hist_wait_dualpipe) == 1)
		//		atomic_set(&g_aal1_hist_available, 1);
		}
		spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);
		if (result) {
			AALIRQ_LOG("wake_up_interruptible\n");
			wake_up_interruptible(&aal_data->primary_data->hist_wq);
		} else {
			AALIRQ_LOG("result fail\n");
		}
	} else {
		AALIRQ_LOG("comp %d hist not retrieved\n", comp->id);
		CRTC_MMP_MARK(0, aal_dre30_rw, comp->id, 0xEE);
	}

	CRTC_MMP_MARK(0, aal_dre30_rw, comp->id, 1);

	if (spin_trylock_irqsave(&aal_data->primary_data->dre3_gain_lock, flags)) {
		/* Write DRE 3.0 gain */
		if (!atomic_read(&aal_data->first_frame))
			disp_aal_write_dre3(comp);

		spin_unlock_irqrestore(&aal_data->primary_data->dre3_gain_lock, flags);
	} else {
		AALERR("comp %d dre30 gain not set\n", comp->id);
		CRTC_MMP_MARK(0, aal_dre30_rw, comp->id, 0x1EE);
	}
	CRTC_MMP_EVENT_END(0, aal_dre30_rw, comp->id, 2);
}

static void disp_aal_dre3_irq_handle(struct mtk_ddp_comp *comp)
{
	u32 hist_apb = 0, hist_int = 0, sram_cfg = 0, sram_mask = 0;
	u32 curve_apb = 0, curve_int = 0;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_curve_sram = aal_data->data->aal_dre3_curve_sram;
	int dre30_write = atomic_read(&aal_data->primary_data->dre30_write);
	atomic_t *force_curve_sram_apb = &aal_data->force_curve_sram_apb;

	if (atomic_read(&aal_data->primary_data->change_to_dre30) != 0x3)
		return;

	if (debug_dump_reg_irq)
		debug_dump_reg_irq = !dump_reg(comp, true);

	if (debug_skip_dre3_irq) {
		pr_notice("skip dre3 irq for debug\n");
		return;
	}

	if (aal_data->primary_data->sram_method == AAL_SRAM_EOF &&
		atomic_read(&aal_data->primary_data->dre_halt) == 0) {
		if (atomic_cmpxchg(&aal_data->force_hist_apb, 0, 1) == 0) {
			hist_apb = 0;
			hist_int = 1;
		} else if (atomic_cmpxchg(&aal_data->force_hist_apb, 1, 0) == 1) {
			hist_apb = 1;
			hist_int = 0;
		} else {
			AALERR("Error when get hist_apb irq_handler\n");
			return;
		}

		if (aal_dre3_curve_sram) {
			if (dre30_write) {
				if (atomic_cmpxchg(force_curve_sram_apb, 0, 1) == 0) {
					curve_apb = 0;
					curve_int = 1;
				} else if (atomic_cmpxchg(force_curve_sram_apb, 1, 0) == 1) {
					curve_apb = 1;
					curve_int = 0;
				} else
					AALERR("[SRAM] Error when get curve_apb\n");
			} else {
				if (atomic_read(force_curve_sram_apb) == 0) {
					curve_apb = 1;
					curve_int = 0;
				} else if (atomic_read(force_curve_sram_apb) == 1) {
					curve_apb = 0;
					curve_int = 1;
				} else
					AALERR("[SRAM] Error when get curve_apb\n");
			}
		}
		AALIRQ_LOG("[SRAM] %s hist_apb (%d) hist_int (%d) in(EOF) comp:%d\n",
			__func__, hist_apb, hist_int, comp->id);
		SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_HIST_SRAM_EN);
		SET_VAL_MASK(sram_cfg, sram_mask, hist_apb, REG_FORCE_HIST_SRAM_APB);
		SET_VAL_MASK(sram_cfg, sram_mask, hist_int, REG_FORCE_HIST_SRAM_INT);
		if (aal_dre3_curve_sram) {
			SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_CURVE_SRAM_EN);
			SET_VAL_MASK(sram_cfg, sram_mask, curve_apb, REG_FORCE_CURVE_SRAM_APB);
			SET_VAL_MASK(sram_cfg, sram_mask, curve_int, REG_FORCE_CURVE_SRAM_INT);
		}
		mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG, sram_cfg, sram_mask);
		atomic_set(&aal_data->primary_data->dre_halt, 1);
		disp_aal_update_dre3_sram(comp, false);
		atomic_set(&aal_data->primary_data->dre_halt, 0);
	} else if (aal_data->primary_data->sram_method == AAL_SRAM_SOF) {
		if (mtk_drm_is_idle(&(comp->mtk_crtc->base))) {
			AALIRQ_LOG("[SRAM] when idle, operate SRAM in (EOF) comp id:%d\n",
					comp->id);
			disp_aal_update_dre3_sram(comp, false);
		}
	}
}

static void disp_aal_set_init_dre30(struct mtk_ddp_comp *comp,
		struct DISP_DRE30_INIT *user_regs)
{
	struct DISP_DRE30_INIT *init_dre3;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	init_dre3 = &aal_data->primary_data->init_dre30;

	memcpy(init_dre3, user_regs, sizeof(*init_dre3));
	/* Modify DRE3.0 config flag */
	atomic_or(0x2, &aal_data->primary_data->change_to_dre30);
}

static void ddp_aal_dre3_write_curve_full(struct mtk_ddp_comp *comp)
{
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);
	uint32_t reg_value = 0, reg_mask = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_curve_sram = aal_data->data->aal_dre3_curve_sram;

	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_EN);
	SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_HIST_SRAM_APB);
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_INT);
	if (aal_dre3_curve_sram) {
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_EN);
		SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_CURVE_SRAM_APB);
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_INT);
	}
	mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG, reg_value, reg_mask);
	disp_aal_write_dre3(comp);

	reg_value = 0;
	reg_mask = 0;
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_EN);
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_APB);
	SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_HIST_SRAM_INT);
	if (aal_dre3_curve_sram) {
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_EN);
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_APB);
		SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_CURVE_SRAM_INT);
	}
	mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG, reg_value, reg_mask);
	disp_aal_write_dre3(comp);

	atomic_set(&aal_data->force_hist_apb, 0);
	if (aal_dre3_curve_sram)
		atomic_set(&aal_data->force_curve_sram_apb, 0);
}

static bool write_block(struct mtk_disp_aal *aal_data, const unsigned int *dre3_gain,
	const int block_x, const int block_y, const int dre_blk_x_num, int check)
{
	bool return_value = false;
	uint32_t block_offset = 4 * (block_y * dre_blk_x_num + block_x);
	uint32_t value;

	do {
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[0] & 0xff) |
			((dre3_gain[1] & 0xff) << 8) |
			((dre3_gain[2] & 0xff) << 16) |
			((dre3_gain[3] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x TS: 0x%08llx\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset],
			       arch_timer_read_counter());
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[4] & 0xff) |
			((dre3_gain[5] & 0xff) << 8) |
			((dre3_gain[6] & 0xff) << 16) |
			((dre3_gain[7] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x TS: 0x%08llx\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset],
			       arch_timer_read_counter());
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[8] & 0xff) |
			((dre3_gain[9] & 0xff) << 8) |
			((dre3_gain[10] & 0xff) << 16) |
			((dre3_gain[11] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x TS: 0x%08llx\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset],
			       arch_timer_read_counter());
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[12] & 0xff) |
			((dre3_gain[13] & 0xff) << 8) |
			((dre3_gain[14] & 0xff) << 16) |
			((dre3_gain[15] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x TS: 0x%08llx\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset],
			       arch_timer_read_counter());
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		return_value = true;
	} while (0);

	return return_value;
}

static bool write_curve16(struct mtk_disp_aal *aal_data, const unsigned int *dre3_gain,
	const int dre_blk_x_num, const int dre_blk_y_num, int check)
{
	int32_t blk_x, blk_y;
	const int32_t blk_num_max = dre_blk_x_num * dre_blk_y_num;
	unsigned int write_value = 0x0;
	uint32_t bit_shift = 0;
	uint32_t block_offset = AAL_DRE_GAIN_POINT16_START;

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			write_value |=
				((dre3_gain[16] & 0xff) << (8*bit_shift));
			bit_shift++;

			if (bit_shift >= 4) {
				if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
					return false;
				if (check &&
				    write_value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
					DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x TS: 0x%08llx\n",
					       __func__, __LINE__, dre_blk_x_num, blk_x, blk_y, block_offset,
					       write_value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset],
					       arch_timer_read_counter());
					return false;
				}
				aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] =
					write_value;

				write_value = 0x0;
				bit_shift = 0;
			}
		}
	}

	if ((blk_num_max>>2)<<2 != blk_num_max) {
		/* configure last curve */
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		if (check && write_value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x TS: 0x%08llx\n",
			       __func__, __LINE__, dre_blk_x_num, blk_x, blk_y, block_offset,
			       write_value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset],
			       arch_timer_read_counter());
			return false;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset] = write_value;
	}

	return true;
}

void disp_aal_dre3_check_reset_to_linear(struct mtk_ddp_comp *comp)
{
	const int dre_blk_x_num = 8;
	const int dre_blk_y_num = 16;
	unsigned long flags;
	int blk_x, blk_y, curve_point;
	unsigned int dre3_gain[AAL_DRE3_POINT_NUM];
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("start\n");
	for (curve_point = 0; curve_point < AAL_DRE3_POINT_NUM;
		curve_point++) {
		/* assign initial gain curve */
		dre3_gain[curve_point] = aal_min(255, 16 * curve_point);
	}

	spin_lock_irqsave(&aal_data->primary_data->dre3_gain_lock, flags);
	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			/* write each block dre curve */
			if (!write_block(aal_data, dre3_gain, blk_x, blk_y, dre_blk_x_num, 1))
				goto _return;
		}
	}
	/* write each block dre curve last point */
	write_curve16(aal_data, dre3_gain, dre_blk_x_num, dre_blk_y_num, 1);
_return:
	spin_unlock_irqrestore(&aal_data->primary_data->dre3_gain_lock, flags);
}


static void disp_aal_dre3_init(struct mtk_ddp_comp *comp)
{
	const int dre_blk_x_num = 8;
	const int dre_blk_y_num = 16;
	unsigned long flags;
	int blk_x, blk_y, curve_point;
	unsigned int dre3_gain[AAL_DRE3_POINT_NUM];
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("start\n");
	for (curve_point = 0; curve_point < AAL_DRE3_POINT_NUM;
		curve_point++) {
		/* assign initial gain curve */
		dre3_gain[curve_point] = aal_min(255, 16 * curve_point);
	}

	spin_lock_irqsave(&aal_data->primary_data->dre3_gain_lock, flags);
	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			/* write each block dre curve */
			write_block(aal_data, dre3_gain, blk_x, blk_y, dre_blk_x_num, 0);
		}
	}
	/* write each block dre curve last point */
	write_curve16(aal_data, dre3_gain, dre_blk_x_num, dre_blk_y_num, 0);

	ddp_aal_dre3_write_curve_full(comp);
	spin_unlock_irqrestore(&aal_data->primary_data->dre3_gain_lock, flags);
}

static void disp_aal_single_pipe_hist_update(struct mtk_ddp_comp *comp, unsigned int status)
{
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool read_success = false;

	CRTC_MMP_EVENT_START(0, aal_dre20_rh, comp->id, 0);
	/* Only process end of frame state */
	if ((status & AAL_IRQ_OF_END) == 0x0) {
		AALERR("break comp %u status 0x%x\n", comp->id, status);
		CRTC_MMP_EVENT_END(0, aal_dre20_rh, comp->id, 1);
		return;
	}

	CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 2);
	if (spin_trylock_irqsave(&aal_data->primary_data->hist_lock, flags)) {
		read_success = disp_aal_read_single_hist(comp);
		if (read_success)
			atomic_set(&aal_data->eof_irq, 0);
		spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

		if (read_success == true) {
			atomic_set(&aal_data->dre20_hist_is_ready, 1);
			AALIRQ_LOG("DDP_COMPONENT_AAL0 read_success = %d\n",
					read_success);

			if (atomic_read(&aal_data->first_frame) == 1) {
				atomic_set(&aal_data->first_frame, 0);
				aal_data->primary_data->refresh_task.data = (void *)comp;
				queue_work(aal_data->primary_data->refresh_wq,
					&aal_data->primary_data->refresh_task.task);
			}
		}
	} else {
		AALIRQ_LOG("comp %d hist not retrieved\n", comp->id);
		CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 0xEE);
	}
	CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 3);
	if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 0)
		disp_aal_set_interrupt(comp, 0, -1, NULL);
	CRTC_MMP_EVENT_END(0, aal_dre20_rh, comp->id, 4);
}

int mtk_drm_ioctl_aal_init_dre30_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		AALFLOW_LOG("\n");
		disp_aal_set_init_dre30(comp, (struct DISP_DRE30_INIT *) data);
	} else {
		AALFLOW_LOG("DRE30 not support\n");
	}

	return 0;
}

int mtk_drm_ioctl_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_init_dre30_impl(comp, data);
}

static int disp_aal_wait_size(struct mtk_disp_aal *aal_data, unsigned long timeout)
{
	int ret = 0;

	if (aal_data->primary_data->get_size_available == false) {
		ret = wait_event_interruptible(aal_data->primary_data->size_wq,
		aal_data->primary_data->get_size_available == true);
		pr_notice("size_available = 1, Waken up, ret = %d\n",
			ret);
	} else {
		/* If g_aal_get_size_available is already set, */
		/* means AALService was delayed */
		pr_notice("size_available = 0\n");
	}
	return ret;
}

int mtk_drm_ioctl_aal_get_size_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct DISP_AAL_DISPLAY_SIZE *dst =
		(struct DISP_AAL_DISPLAY_SIZE *)data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	disp_aal_wait_size(aal_data, 60);

	if (comp == NULL || comp->mtk_crtc == NULL) {
		AALERR("%s null pointer!\n", __func__);
		return -1;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		memcpy(dst, &aal_data->primary_data->dual_size,
				sizeof(aal_data->primary_data->dual_size));
	else
		memcpy(dst, &aal_data->primary_data->size, sizeof(aal_data->primary_data->size));

	return 0;
}

int mtk_drm_ioctl_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_get_size_impl(comp, data);
}

int mtk_drm_ioctl_clarity_set_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	AALCRT_LOG(": enter set clarity regs\n");

	return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, SET_CLARITY_REG, data);
}

int mtk_drm_ioctl_clarity_set_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_clarity_set_reg_impl(comp, data);
}

int mtk_drm_ioctl_aal_set_trigger_state_impl(struct mtk_ddp_comp *comp, void *data)
{
	unsigned long flags;
	unsigned int dre3EnState;
	struct DISP_AAL_TRIG_STATE *trigger_state = (struct DISP_AAL_TRIG_STATE *)data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_disp_aal_primary *pdata = aal_data->primary_data;

	dre3EnState = trigger_state->dre3_en_state;

	AALFLOW_LOG("dre3EnState: 0x%x, trigger_state: %d, ali: %d, aliThres: %d\n",
			dre3EnState,
			trigger_state->dre_frm_trigger,
			trigger_state->curAli, trigger_state->aliThreshold);

	if (dre3EnState & 0x2) {
		AALFLOW_LOG("dre change to open!\n");

		spin_lock_irqsave(&pdata->hist_lock, flags);
		if ((pdata->aal_fo->mtk_dre30_support)
			&& (!pdata->dre30_enabled)) {
			pdata->prv_dre30_enabled = pdata->dre30_enabled;
			pdata->dre30_enabled = true;
		}
		spin_unlock_irqrestore(&pdata->hist_lock, flags);

		if (!pdata->prv_dre30_enabled && pdata->dre30_enabled) {
			// need flip sram to get local histogram
			mtk_crtc_user_cmd(aal_data->crtc, comp, FLIP_SRAM, NULL);
			pdata->prv_dre30_enabled = pdata->dre30_enabled;
		}

		trigger_state->dre3_krn_flag = pdata->dre30_enabled ? 1 : 0;
		return 0;
	}

	if (dre3EnState & 0x1) {
		spin_lock_irqsave(&pdata->hist_lock, flags);
		if (trigger_state->dre_frm_trigger == 0) {
			if ((pdata->aal_fo->mtk_dre30_support)
				&& pdata->dre30_enabled) {
				pdata->dre30_enabled = false;
#if IS_ENABLED(CONFIG_MTK_DISP_DEBUG)
				disp_aal_dre3_check_reset_to_linear(comp);
#endif
				AALFLOW_LOG("dre change to close!\n");
			}
		}

		trigger_state->dre3_krn_flag = pdata->dre30_enabled ? 1 : 0;
		spin_unlock_irqrestore(&pdata->hist_lock, flags);
	}

	return 0;
}

int mtk_drm_ioctl_aal_set_trigger_state(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}
	return mtk_drm_ioctl_aal_set_trigger_state_impl(comp, data);
}

int mtk_drm_ioctl_aal_get_base_voltage(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct DISP_PANEL_BASE_VOLTAGE *dst_baseVoltage = (struct DISP_PANEL_BASE_VOLTAGE *)data;
	struct DISP_PANEL_BASE_VOLTAGE src_baseVoltage;
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);
	if (!output_comp) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EFAULT;
	}

	AALFLOW_LOG("get base_voltage\n");

	/* DSI_SEND_DDIC_CMD */
	if (output_comp) {
		ret = mtk_ddp_comp_io_cmd(output_comp, NULL,
			DSI_READ_ELVSS_BASE_VOLTAGE, &src_baseVoltage);
		if (ret < 0)
			DDPPR_ERR("%s:read elvss base voltage failed\n", __func__);
		else {
			memcpy(dst_baseVoltage, &src_baseVoltage, sizeof(struct DISP_PANEL_BASE_VOLTAGE));
			if (debug_dump_aal_hist)
				dump_base_voltage(&src_baseVoltage);
		}
	}
	return ret;
}

static void dumpAlgAALClarityRegOutput(struct DISP_CLARITY_REG clarityHWReg)
{
	AALCRT_LOG("bilateral_impulse_noise_en[%d] dre_bilateral_detect_en[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_impulse_noise_en,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_detect_en);

	AALCRT_LOG("have_bilateral_filter[%d] dre_output_mode[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.have_bilateral_filter,
		clarityHWReg.mdp_aal_clarity_regs.dre_output_mode);

	AALCRT_LOG("bilateral_range_flt_slope[%d] dre_bilateral_activate_blending_A[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_range_flt_slope,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_A);

	AALCRT_LOG("dre_bilateral_activate_blending_B[%d] dre_bilateral_activate_blending_C[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_B,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_C);

	AALCRT_LOG("dre_bilateral_activate_blending_D[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_D);
	AALCRT_LOG("dre_bilateral_activate_blending_wgt_gain[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_wgt_gain);

	AALCRT_LOG("dre_bilateral_blending_en[%d] dre_bilateral_blending_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_en,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_wgt);

	AALCRT_LOG("dre_bilateral_blending_wgt_mode[%d] dre_bilateral_size_blending_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_wgt_mode,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_size_blending_wgt);

	AALCRT_LOG("bilateral_custom_range_flt1 [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_4);

	AALCRT_LOG("bilateral_custom_range_flt1 [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_4);

	AALCRT_LOG("bilateral_custom_range_flt1 [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_4);

	AALCRT_LOG("bilateral_custom_range_flt2 [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_4);

	AALCRT_LOG("bilateral_custom_range_flt2 [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_4);

	AALCRT_LOG("bilateral_custom_range_flt2 [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_4);

	AALCRT_LOG("bilateral_contrary_blending_wgt[%d] bilateral_custom_range_flt_gain[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_contrary_blending_wgt,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt_gain);

	AALCRT_LOG("bilateral_custom_range_flt_slope[%d] bilateral_range_flt_gain[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt_slope,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_range_flt_gain);

	AALCRT_LOG("bilateral_size_blending_wgt[%d] bilateral_contrary_blending_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_size_blending_wgt,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_contrary_blending_out_wgt);

	AALCRT_LOG("bilateral_custom_range_flt1_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_out_wgt);
	AALCRT_LOG("bilateral_custom_range_flt2_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_out_wgt);

	AALCRT_LOG("bilateral_range_flt_out_wgt[%d] bilateral_size_blending_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_range_flt_out_wgt,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_size_blending_out_wgt);

	AALCRT_LOG("dre_bilateral_blending_region_protection_en[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_region_protection_en);
	AALCRT_LOG("dre_bilateral_region_protection_activate_A[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_A);

	AALCRT_LOG("dre_bilateral_region_protection_activate_B[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_B);
	AALCRT_LOG("dre_bilateral_region_protection_activate_C[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_C);

	AALCRT_LOG("dre_bilateral_region_protection_activate_D[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_D);
	AALCRT_LOG("dre_bilateral_region_protection_input_shift_bit[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_input_shift_bit);
}

static void dumpAlgTDSHPRegOutput(struct DISP_CLARITY_REG clarityHWReg)
{
	AALCRT_LOG("tdshp_gain_high[%d] tdshp_gain_mid[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.tdshp_gain_high,
		clarityHWReg.disp_tdshp_clarity_regs.tdshp_gain_mid);

	AALCRT_LOG("mid_coef_v_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_4);

	AALCRT_LOG("mid_coef_v_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_4);

	AALCRT_LOG("mid_coef_v_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_4);

	AALCRT_LOG("mid_coef_h_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_4);

	AALCRT_LOG("mid_coef_h_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_4);

	AALCRT_LOG("mid_coef_h_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_v_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_v_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_v_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_h_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_h_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_h_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_rd_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_rd_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_rd_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_ld_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_ld_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_ld_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_4);

	AALCRT_LOG("mid_negative_offset[%d] mid_positive_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.mid_negative_offset,
		clarityHWReg.disp_tdshp_clarity_regs.mid_positive_offset);
	AALCRT_LOG("high_negative_offset[%d] high_positive_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_negative_offset,
		clarityHWReg.disp_tdshp_clarity_regs.high_positive_offset);

	AALCRT_LOG("D_active_parameter_N_gain[%d] D_active_parameter_N_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_N_offset);
	AALCRT_LOG("D_active_parameter_P_gain[%d] D_active_parameter_P_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_P_offset);

	AALCRT_LOG("High_active_parameter_N_gain[%d] High_active_parameter_N_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_N_offset);
	AALCRT_LOG("High_active_parameter_P_gain[%d] High_active_parameter_P_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_P_offset);

	AALCRT_LOG("L_active_parameter_N_gain[%d] L_active_parameter_N_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_N_offset);
	AALCRT_LOG("L_active_parameter_P_gain[%d] L_active_parameter_P_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_P_offset);

	AALCRT_LOG("Mid_active_parameter_N_gain[%d] Mid_active_parameter_N_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_N_offset);
	AALCRT_LOG("Mid_active_parameter_P_gain[%d] Mid_active_parameter_P_offset[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_P_offset);

	AALCRT_LOG("SIZE_PARA_BIG_HUGE[%d] SIZE_PARA_MEDIUM_BIG[%d] SIZE_PARA_SMALL_MEDIUM[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARA_BIG_HUGE,
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARA_MEDIUM_BIG,
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARA_SMALL_MEDIUM);

	AALCRT_LOG("high_auto_adaptive_weight_HUGE[%d] high_size_adaptive_weight_HUGE[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_HUGE,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_HUGE);
	AALCRT_LOG("Mid_auto_adaptive_weight_HUGE[%d] Mid_size_adaptive_weight_HUGE[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_HUGE,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_HUGE);

	AALCRT_LOG("high_auto_adaptive_weight_BIG[%d] high_size_adaptive_weight_BIG[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_BIG,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_BIG);
	AALCRT_LOG("Mid_auto_adaptive_weight_BIG[%d] Mid_size_adaptive_weight_BIG[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_BIG,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_BIG);

	AALCRT_LOG("high_auto_adaptive_weight_MEDIUM[%d]high_size_adaptive_weight_MEDIUM[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_MEDIUM,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_MEDIUM);
	AALCRT_LOG("Mid_auto_adaptive_weight_MEDIUM[%d] Mid_size_adaptive_weight_MEDIUM[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_MEDIUM,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_MEDIUM);

	AALCRT_LOG("high_auto_adaptive_weight_SMALL[%d] high_size_adaptive_weight_SMALL[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_SMALL,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_SMALL);
	AALCRT_LOG("Mid_auto_adaptive_weight_SMALL[%d] Mid_size_adaptive_weight_SMALL[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_SMALL,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_SMALL);

	AALCRT_LOG("FILTER_HIST_EN[%d] FREQ_EXTRACT_ENHANCE[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.FILTER_HIST_EN,
		clarityHWReg.disp_tdshp_clarity_regs.FREQ_EXTRACT_ENHANCE);
	AALCRT_LOG("freq_D_weighting[%d] freq_H_weighting[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.freq_D_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_H_weighting);

	AALCRT_LOG("freq_L_weighting[%d] freq_M_weighting[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.freq_L_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_M_weighting);
	AALCRT_LOG("freq_D_final_weighting[%d] freq_L_final_weighting[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.freq_D_final_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_L_final_weighting);

	AALCRT_LOG("freq_M_final_weighting[%d] freq_WH_final_weighting[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.freq_M_final_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_WH_final_weighting);
	AALCRT_LOG("SIZE_PARAMETER[%d] chroma_high_gain[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARAMETER,
		clarityHWReg.disp_tdshp_clarity_regs.chroma_high_gain);

	AALCRT_LOG("chroma_high_index[%d] chroma_low_gain[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.chroma_high_index,
		clarityHWReg.disp_tdshp_clarity_regs.chroma_low_gain);
	AALCRT_LOG("chroma_low_index[%d] luma_high_gain[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.chroma_low_index,
		clarityHWReg.disp_tdshp_clarity_regs.luma_high_gain);

	AALCRT_LOG("luma_high_index[%d] luma_low_gain[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.luma_high_index,
		clarityHWReg.disp_tdshp_clarity_regs.luma_low_gain);
	AALCRT_LOG("luma_low_index[%d] Chroma_adaptive_mode[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.luma_low_index,
		clarityHWReg.disp_tdshp_clarity_regs.Chroma_adaptive_mode);

	AALCRT_LOG("Chroma_shift[%d] Luma_adaptive_mode[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Chroma_shift,
		clarityHWReg.disp_tdshp_clarity_regs.Luma_adaptive_mode);
	AALCRT_LOG("Luma_shift[%d] class_0_positive_gain[%d] class_0_negative_gain[%d]\n",
		clarityHWReg.disp_tdshp_clarity_regs.Luma_shift,
		clarityHWReg.disp_tdshp_clarity_regs.class_0_positive_gain,
		clarityHWReg.disp_tdshp_clarity_regs.class_0_negative_gain);
}

static int mtk_disp_clarity_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_CLARITY_REG *clarity_regs)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *comp_tdshp = aal_data->comp_tdshp;
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

	if (clarity_regs == NULL)
		return -1;

	// aal clarity set registers
	CRTC_MMP_MARK(0, clarity_set_regs, comp->id, 1);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATEAL,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_impulse_noise_en << 9 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_detect_en << 8 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_slope << 4 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_flt_en << 1 |
		clarity_regs->mdp_aal_clarity_regs.have_bilateral_filter << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DISP_AAL_CFG_MAIN,
		clarity_regs->mdp_aal_clarity_regs.dre_output_mode, 0x1 << 5);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_Blending_00,
		(clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_D << 27 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_C << 23 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_B << 19 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_A << 15 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_wgt_gain << 11 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_wgt_mode << 9 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_wgt << 4 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_en << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_Blending_01,
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_size_blending_wgt << 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT1_00,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT1_01,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT1_02,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT2_00,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT2_01,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT2_02,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_FLT_CONFIG,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_size_blending_wgt << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_contrary_blending_wgt << 10 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt_slope << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt_gain << 3 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_gain << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_FREQ_BLENDING,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_out_wgt << 20 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_out_wgt << 15 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_out_wgt << 10 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_size_blending_out_wgt << 5 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_contrary_blending_out_wgt << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_REGION_PROTECTION,
	(clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_input_shift_bit << 25 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_D << 21 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_C << 13 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_B << 5 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_A << 1 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_region_protection_en << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA,
		(clarity_regs->disp_tdshp_clarity_regs.mid_negative_offset << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_positive_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_negative_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_positive_offset << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_D,
		(clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_H,
		(clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_L,
		(clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_M,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_SIZE_PARA,
		(clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_SMALL_MEDIUM << 0 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_MEDIUM_BIG << 6 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_BIG_HUGE << 12), 0x3FFFF);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_HUGE,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_HUGE << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_HUGE << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_HUGE << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_HUGE << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_BIG,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_BIG << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_BIG << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_BIG << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_BIG << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_MEDIUM,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_MEDIUM << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_MEDIUM << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_MEDIUM << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_MEDIUM << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_SMALL,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_SMALL << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_SMALL << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_SMALL << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_SMALL << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_CFG,
		(clarity_regs->disp_tdshp_clarity_regs.FREQ_EXTRACT_ENHANCE << 12 |
		clarity_regs->disp_tdshp_clarity_regs.FILTER_HIST_EN << 16),
		((0x1 << 16) | (0x1 << 12)));

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_FREQUENCY_WEIGHTING,
		(clarity_regs->disp_tdshp_clarity_regs.freq_M_weighting << 0 |
		clarity_regs->disp_tdshp_clarity_regs.freq_H_weighting << 4 |
		clarity_regs->disp_tdshp_clarity_regs.freq_D_weighting << 8 |
		clarity_regs->disp_tdshp_clarity_regs.freq_L_weighting << 12), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + MDP_TDSHP_FREQUENCY_WEIGHTING_FINAL,
		(clarity_regs->disp_tdshp_clarity_regs.freq_M_final_weighting << 0 |
		clarity_regs->disp_tdshp_clarity_regs.freq_D_final_weighting << 5 |
		clarity_regs->disp_tdshp_clarity_regs.freq_L_final_weighting << 10 |
		clarity_regs->disp_tdshp_clarity_regs.freq_WH_final_weighting << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + LUMA_CHROMA_PARAMETER,
		(clarity_regs->disp_tdshp_clarity_regs.luma_low_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.luma_low_index << 3 |
		clarity_regs->disp_tdshp_clarity_regs.luma_high_index << 8 |
		clarity_regs->disp_tdshp_clarity_regs.luma_high_gain << 13 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_low_gain << 16 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_low_index << 19 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_high_index << 24 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_high_gain << 29), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH,
		(clarity_regs->disp_tdshp_clarity_regs.Luma_adaptive_mode << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Chroma_adaptive_mode << 1 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARAMETER << 2 |
		clarity_regs->disp_tdshp_clarity_regs.Luma_shift << 12 |
		clarity_regs->disp_tdshp_clarity_regs.Chroma_shift << 15),
		(0x7 << 15) | (0x7 << 12) | (0x1F << 2) | (1 << 1) | (1 << 0));

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + CLASS_0_2_GAIN,
		(clarity_regs->disp_tdshp_clarity_regs.class_0_positive_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.class_0_negative_gain << 5),
		0x3FF);

	CRTC_MMP_MARK(0, clarity_set_regs, comp->id, 2);

	return ret;
}

static void mtk_aal_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		int dre_alg_mode = 0;
		phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

		if (atomic_read(&aal_data->primary_data->change_to_dre30) & 0x1)
			dre_alg_mode = 1;
		if (debug_bypass_alg_mode)
			dre_alg_mode = 0;
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + DISP_AAL_CFG_MAIN,
			(0x0 << 3) | (dre_alg_mode << 4), (1 << 3) | (1 << 4));
	}
	AALFLOW_LOG("\n");
	basic_cmdq_write(handle, comp, DISP_AAL_EN, 0x1, ~0);

	// for Display Clarity
	if (aal_data->primary_data->disp_clarity_regs != NULL) {
		mutex_lock(&aal_data->primary_data->clarity_lock);
		if (mtk_disp_clarity_set_reg(comp, handle,
			aal_data->primary_data->disp_clarity_regs) < 0)
			DDPMSG("%s: clarity_set_reg failed\n", __func__);
		mutex_unlock(&aal_data->primary_data->clarity_lock);
	}
}

static void mtk_aal_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	basic_cmdq_write(handle, comp, DISP_AAL_EN, 0x0, ~0);
}

static void mtk_aal_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("bypass: %d\n", bypass);
	atomic_set(&aal_data->primary_data->force_relay, bypass);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_CFG, bypass, 0x1);
	disp_aal_set_interrupt(comp, !bypass, -1, handle);

	if (bypass == 0) // Enable AAL Histogram
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x6, 0x7);

}

static int mtk_aal_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("cmd: %d\n", cmd);
	switch (cmd) {
	case INIT_REG:
		if (disp_aal_set_init_reg(comp, handle,
			(struct DISP_AAL_INITREG *) data) < 0) {
			AALERR("INIT_REG: fail\n");
			return -EFAULT;
		}
		break;
	case SET_PARAM:
		if (disp_aal_set_param(comp, handle,
			(struct DISP_AAL_PARAM *) data) < 0) {
			AALERR("SET_PARAM: fail\n");
			return -EFAULT;
		}
		break;
	case FLIP_SRAM:
		disp_aal_flip_sram(comp, handle, __func__);
		if (comp->mtk_crtc->is_dual_pipe)
			disp_aal_flip_sram(aal_data->companion, handle, __func__);
		break;
	case BYPASS_AAL:
	{
		int *value = data;

		mtk_aal_bypass(comp, *value, handle);
		if (aal_data->primary_data->aal_fo->mtk_dre30_support)
			mtk_dmdp_aal_bypass(aal_data->comp_dmdp_aal, *value, handle);

		if (comp->mtk_crtc->is_dual_pipe) {
			mtk_aal_bypass(aal_data->companion, *value, handle);
			if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
				struct mtk_disp_aal *aal_companion_data = comp_to_aal(aal_data->companion);

				mtk_dmdp_aal_bypass(aal_companion_data->comp_dmdp_aal, *value, handle);
			}
		}
	}
		break;
	case SET_CLARITY_REG:
	{
		if (!aal_data->primary_data->disp_clarity_regs) {
			aal_data->primary_data->disp_clarity_regs
				= kmalloc(sizeof(struct DISP_CLARITY_REG), GFP_KERNEL);
			if (aal_data->primary_data->disp_clarity_regs == NULL) {
				DDPMSG("%s: no memory\n", __func__);
				return -EFAULT;
			}
		}

		if (data == NULL)
			return -EFAULT;

		mutex_lock(&aal_data->primary_data->clarity_lock);
		memcpy(aal_data->primary_data->disp_clarity_regs, (struct DISP_CLARITY_REG *)data,
			sizeof(struct DISP_CLARITY_REG));

		dumpAlgAALClarityRegOutput(*aal_data->primary_data->disp_clarity_regs);
		dumpAlgTDSHPRegOutput(*aal_data->primary_data->disp_clarity_regs);

		CRTC_MMP_EVENT_START(0, clarity_set_regs, 0, 0);
		if (mtk_disp_clarity_set_reg(comp, handle,
			aal_data->primary_data->disp_clarity_regs) < 0) {
			DDPMSG("[Pipe0] %s: clarity_set_reg failed\n", __func__);
			mutex_unlock(&aal_data->primary_data->clarity_lock);

			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			if (mtk_disp_clarity_set_reg(aal_data->companion, handle,
					aal_data->primary_data->disp_clarity_regs) < 0) {
				DDPMSG("[Pipe1] %s: clarity_set_reg failed\n", __func__);
				mutex_unlock(&aal_data->primary_data->clarity_lock);

				return -EFAULT;
			}
		}
		CRTC_MMP_EVENT_END(0, clarity_set_regs, 0, 3);
		mutex_unlock(&aal_data->primary_data->clarity_lock);

		if (atomic_read(&aal_data->primary_data->force_delay_check_trig) == 1)
			mtk_crtc_check_trigger(comp->mtk_crtc, true, false);
		else
			mtk_crtc_check_trigger(comp->mtk_crtc, false, false);
	}
		break;
	default:
		AALERR("error cmd: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void ddp_aal_dre3_backup(struct mtk_ddp_comp *comp)
{
#if defined(DRE3_IN_DISP_AAL)
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	aal_data->primary_data->backup.DRE_BLOCK_INFO_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_00);
	aal_data->primary_data->backup.DRE_BLOCK_INFO_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_01);
	aal_data->primary_data->backup.DRE_BLOCK_INFO_02 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_02);
	aal_data->primary_data->backup.DRE_BLOCK_INFO_04 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_04);
	aal_data->primary_data->backup.DRE_CHROMA_HIST_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_00);
	aal_data->primary_data->backup.DRE_CHROMA_HIST_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_01);
	aal_data->primary_data->backup.DRE_ALPHA_BLEND_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_ALPHA_BLEND_00);
	aal_data->primary_data->backup.DRE_BLOCK_INFO_05 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_05);
	aal_data->primary_data->backup.DRE_BLOCK_INFO_06 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_06);
	aal_data->primary_data->backup.DRE_BLOCK_INFO_07 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_07);
	aal_data->primary_data->backup.SRAM_CFG =
		readl(aal_data->dre3_hw.va + DISP_AAL_SRAM_CFG);
	aal_data->primary_data->backup.DUAL_PIPE_INFO_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_00);
	aal_data->primary_data->backup.DUAL_PIPE_INFO_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_01);
#endif
}

static void ddp_aal_dre_backup(struct mtk_ddp_comp *comp)
{
	int i;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	aal_data->primary_data->backup.DRE_MAPPING =
		readl(comp->regs + DISP_AAL_DRE_MAPPING_00);

	for (i = 0; i < DRE_FLT_NUM; i++)
		aal_data->primary_data->backup.DRE_FLT_FORCE[i] =
			readl(comp->regs + DISP_AAL_DRE_FLT_FORCE(i));

}

static void ddp_aal_cabc_backup(struct mtk_ddp_comp *comp)
{
#ifndef NOT_SUPPORT_CABC_HW
	int i;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	aal_data->primary_data->backup.CABC_00 = readl(comp->regs + DISP_AAL_CABC_00);
	aal_data->primary_data->backup.CABC_02 = readl(comp->regs + DISP_AAL_CABC_02);

	for (i = 0; i < CABC_GAINLMT_NUM; i++)
		aal_data->primary_data->backup.CABC_GAINLMT[i] =
		    readl(comp->regs + DISP_AAL_CABC_GAINLMT_TBL(i));
#endif	/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_cfg_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	aal_data->primary_data->backup.AAL_CFG = readl(comp->regs + DISP_AAL_CFG);
	if (!aal_data->is_right_pipe)
		aal_data->primary_data->backup.AAL_INTEN = readl(comp->regs + DISP_AAL_INTEN);
}

static void ddp_aal_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	ddp_aal_cabc_backup(comp);
	ddp_aal_dre_backup(comp);
	ddp_aal_dre3_backup(comp);
	ddp_aal_cfg_backup(comp);
	atomic_set(&aal_data->primary_data->initialed, 1);
}

static void ddp_aal_dre3_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

#if defined(DRE3_IN_DISP_AAL)
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_00,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_00 & (0x1FFF << 13), 0x1FFF << 13);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_01,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_01, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_02,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_02, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_04,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_04 & (0x3FF << 13), 0x3FF << 13);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_00,
		aal_data->primary_data->backup.DRE_CHROMA_HIST_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_01,
		aal_data->primary_data->backup.DRE_CHROMA_HIST_01 & 0xFFFF, 0xFFFF);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_ALPHA_BLEND_00,
		aal_data->primary_data->backup.DRE_ALPHA_BLEND_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_05,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_05, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_06,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_06, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_07,
		aal_data->primary_data->backup.DRE_BLOCK_INFO_07, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_SRAM_CFG,
		aal_data->primary_data->backup.SRAM_CFG, 0x1);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_00,
		aal_data->primary_data->backup.DUAL_PIPE_INFO_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_01,
		aal_data->primary_data->backup.DUAL_PIPE_INFO_01, ~0);
#endif

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		unsigned long flags;

		spin_lock_irqsave(&aal_data->primary_data->dre3_gain_lock, flags);
		ddp_aal_dre3_write_curve_full(comp);
		spin_unlock_irqrestore(&aal_data->primary_data->dre3_gain_lock, flags);
	}
}

static void ddp_aal_dre_restore(struct mtk_ddp_comp *comp)
{
	int i;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	writel(aal_data->primary_data->backup.DRE_MAPPING,
		comp->regs + DISP_AAL_DRE_MAPPING_00);

	for (i = 0; i < DRE_FLT_NUM; i++)
		writel(aal_data->primary_data->backup.DRE_FLT_FORCE[i],
			comp->regs + DISP_AAL_DRE_FLT_FORCE(i));
}

static void ddp_aal_cabc_restore(struct mtk_ddp_comp *comp)
{
#ifndef NOT_SUPPORT_CABC_HW
	int i;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	writel(aal_data->primary_data->backup.CABC_00, comp->regs + DISP_AAL_CABC_00);
	writel(aal_data->primary_data->backup.CABC_02, comp->regs + DISP_AAL_CABC_02);

	for (i = 0; i < CABC_GAINLMT_NUM; i++)
		writel(aal_data->primary_data->backup.CABC_GAINLMT[i],
			comp->regs + DISP_AAL_CABC_GAINLMT_TBL(i));
#endif	/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_cfg_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	writel(aal_data->primary_data->backup.AAL_CFG, comp->regs + DISP_AAL_CFG);
	if (!aal_data->is_right_pipe)
		writel(aal_data->primary_data->backup.AAL_INTEN, comp->regs + DISP_AAL_INTEN);
}

static void ddp_aal_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (atomic_read(&aal_data->primary_data->initialed) != 1)
		return;

	AALFLOW_LOG("\n");
	ddp_aal_cabc_restore(comp);
	ddp_aal_dre_restore(comp);
	ddp_aal_dre3_restore(comp);
	ddp_aal_cfg_restore(comp);
}

static bool debug_skip_first_br;
static void mtk_aal_prepare(struct mtk_ddp_comp *comp)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool first_restore = (atomic_read(&aal_data->is_clock_on) == 0);

	mtk_ddp_comp_clk_prepare(comp);

	atomic_set(&aal_data->is_clock_on, 1);
	atomic_set(&aal_data->first_frame, 1);

	AALFLOW_LOG("[aal_data, g_aal_data] addr[%lx, %lx] val[%d, %d]\n",
			(unsigned long)&aal_data->is_clock_on,
			(unsigned long)&aal_data->is_clock_on,
			atomic_read(&aal_data->is_clock_on),
			atomic_read(&aal_data->is_clock_on));

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		if (aal_data->dre3_hw.clk) {
			ret = clk_prepare(aal_data->dre3_hw.clk);
			if (ret < 0)
				DDPPR_ERR("failed to prepare dre3_hw.clk\n");
		}
	}
	AALFLOW_LOG("first_restore %d, debug_skip_first_br %d\n",
			first_restore, debug_skip_first_br);
	if (!first_restore && !debug_skip_first_br)
		return;
	atomic_set(&aal_data->dre_hw_prepare, 1);
	/* Bypass shadow register and read shadow register */
	if (aal_data->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, AAL_BYPASS_SHADOW,
			DISP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);

	ddp_aal_restore(comp);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		if (atomic_cmpxchg(&aal_data->dre_hw_init, 0, 1) == 0)
			disp_aal_dre3_init(comp);
	}
	atomic_set(&aal_data->dre_hw_prepare, 0);
}

static void mtk_aal_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	unsigned long flags;
	bool first_backup = (atomic_read(&aal_data->is_clock_on) == 1);

	AALFLOW_LOG("\n");
	spin_lock_irqsave(&aal_data->primary_data->clock_lock, flags);
	atomic_set(&aal_data->is_clock_on, 0);

	spin_unlock_irqrestore(&aal_data->primary_data->clock_lock, flags);
	if (first_backup || debug_skip_first_br)
		ddp_aal_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		if (aal_data->dre3_hw.clk)
			clk_unprepare(aal_data->dre3_hw.clk);
	}
}

static void mtk_aal_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	atomic_set(&aal_data->hist_available, 0);
	atomic_set(&aal_data->dre20_hist_is_ready, 0);
	atomic_set(&aal_data->eof_irq, 0);
	atomic_set(&aal_data->first_frame, 1);
	atomic_set(&aal_data->force_curve_sram_apb, 0);
	atomic_set(&aal_data->force_hist_apb, 0);
	atomic_set(&aal_data->dre_hw_init, 0);
	atomic_set(&aal_data->dre_hw_prepare, 0);
	atomic_set(&aal_data->dre_config, 0);
	if (!aal_data->is_right_pipe) {
		aal_data->comp_tdshp = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DISP_TDSHP, 0);
		aal_data->comp_gamma = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DISP_GAMMA, 0);
		aal_data->comp_color = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DISP_COLOR, 0);
		aal_data->comp_dmdp_aal = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DMDP_AAL, 0);
	} else {
		aal_data->comp_tdshp = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DISP_TDSHP, 0);
		aal_data->comp_gamma = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DISP_GAMMA, 0);
		aal_data->comp_color = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DISP_COLOR, 0);
		aal_data->comp_dmdp_aal = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DMDP_AAL, 0);
	}
}

static void mtk_aal_primary_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_disp_aal *companion_aal_data = comp_to_aal(aal_data->companion);
	char thread_name[20] = {0};
	struct sched_param param = {.sched_priority = 85 };
	struct cpumask mask;
	int len = 0;

	aal_data->crtc = &comp->mtk_crtc->base;

	if (aal_data->is_right_pipe) {
		kfree(aal_data->primary_data);
		aal_data->primary_data = NULL;
		aal_data->primary_data = companion_aal_data->primary_data;
		return;
	}

	init_waitqueue_head(&aal_data->primary_data->hist_wq);
	init_waitqueue_head(&aal_data->primary_data->sof_irq_wq);
	init_waitqueue_head(&aal_data->primary_data->size_wq);

	spin_lock_init(&aal_data->primary_data->clock_lock);
	spin_lock_init(&aal_data->primary_data->hist_lock);
	spin_lock_init(&aal_data->primary_data->dre3_gain_lock);

	mutex_init(&aal_data->primary_data->sram_lock);
	mutex_init(&aal_data->primary_data->clarity_lock);

	atomic_set(&(aal_data->primary_data->hist_wait_dualpipe), 0);
	atomic_set(&(aal_data->primary_data->sof_irq_available), 0);
	atomic_set(&(aal_data->primary_data->is_init_regs_valid), 0);
	atomic_set(&(aal_data->primary_data->backlight_notified), 0);
	atomic_set(&(aal_data->primary_data->initialed), 0);
	atomic_set(&(aal_data->primary_data->allowPartial), 0);
	atomic_set(&(aal_data->primary_data->force_enable_irq), 0);
	atomic_set(&(aal_data->primary_data->force_relay), 0);
	atomic_set(&(aal_data->primary_data->should_stop), 0);
	atomic_set(&(aal_data->primary_data->dre30_write), 0);
	atomic_set(&(aal_data->primary_data->irq_en), 0);
	atomic_set(&(aal_data->primary_data->force_delay_check_trig), 0);
	atomic_set(&(aal_data->primary_data->dre_halt), 0);
	atomic_set(&(aal_data->primary_data->change_to_dre30), 0);
	atomic_set(&(aal_data->primary_data->panel_type), 0);

	memset(&(aal_data->primary_data->start), 0,
		sizeof(aal_data->primary_data->start));
	memset(&(aal_data->primary_data->end), 0,
		sizeof(aal_data->primary_data->end));
	memset(&(aal_data->primary_data->hist), 0,
		sizeof(aal_data->primary_data->hist));
	aal_data->primary_data->hist.backlight = -1;
	aal_data->primary_data->hist.essStrengthIndex = ESS_LEVEL_BY_CUSTOM_LIB;
	aal_data->primary_data->hist.ess_enable = ESS_EN_BY_CUSTOM_LIB;
	aal_data->primary_data->hist.dre_enable = DRE_EN_BY_CUSTOM_LIB;

	memset(&(aal_data->primary_data->hist_db), 0,
		sizeof(aal_data->primary_data->hist_db));
	memset(&(aal_data->primary_data->init_dre30), 0,
		sizeof(aal_data->primary_data->init_dre30));
	memset(&(aal_data->primary_data->dre30_gain), 0,
		sizeof(aal_data->primary_data->dre30_gain));
	memset(&(aal_data->primary_data->dre30_gain_db), 0,
		sizeof(aal_data->primary_data->dre30_gain_db));
	memset(&(aal_data->primary_data->dre30_hist), 0,
		sizeof(aal_data->primary_data->dre30_hist));
	memset(&(aal_data->primary_data->dre30_hist_db), 0,
		sizeof(aal_data->primary_data->dre30_hist_db));
	memset(&(aal_data->primary_data->size), 0,
		sizeof(aal_data->primary_data->size));
	memset(&(aal_data->primary_data->dual_size), 0,
		sizeof(aal_data->primary_data->dual_size));
	memset(&(aal_data->primary_data->aal_param), 0,
		sizeof(aal_data->primary_data->aal_param));

	aal_data->primary_data->ess20_spect_param.ClarityGain = 0;
	aal_data->primary_data->ess20_spect_param.ELVSSPN = 0;
	aal_data->primary_data->ess20_spect_param.flag = 0x01<<SET_BACKLIGHT_LEVEL;

	aal_data->primary_data->sof_irq_event_task = NULL;
	aal_data->primary_data->flip_wq = NULL;
	aal_data->primary_data->refresh_wq = NULL;
	aal_data->primary_data->disp_clarity_regs = NULL;

	aal_data->primary_data->backlight_set = 0;
	aal_data->primary_data->sram_method = AAL_SRAM_SOF;
	aal_data->primary_data->get_size_available = 0;
	aal_data->primary_data->ess_level = 0;
	aal_data->primary_data->dre_en = DRE_EN_BY_CUSTOM_LIB;
	aal_data->primary_data->ess_en = ESS_EN_BY_CUSTOM_LIB;
	aal_data->primary_data->ess_level_cmd_id = 0;
	aal_data->primary_data->dre_en_cmd_id = 0;
	aal_data->primary_data->ess_en_cmd_id = 0;
	aal_data->primary_data->isDualPQ = 0;
	aal_data->primary_data->led_type = TYPE_FILE;

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (comp->id == DDP_COMPONENT_AAL0)
		mtk_leds_register_notifier(&leds_init_notifier);
#endif

	aal_data->primary_data->refresh_wq = create_singlethread_workqueue("aal_refresh_trigger");
	INIT_WORK(&aal_data->primary_data->refresh_task.task, mtk_disp_aal_refresh_trigger);

	// start thread for aal sof
	len = sprintf(thread_name, "aal_sof_%d", comp->id);
	if (len < 0)
		strcpy(thread_name, "aal_sof_0");
	aal_data->primary_data->sof_irq_event_task = kthread_create(mtk_aal_sof_irq_trigger,
						comp, thread_name);

	cpumask_setall(&mask);
	cpumask_clear_cpu(0, &mask);
	set_cpus_allowed_ptr(aal_data->primary_data->sof_irq_event_task, &mask);
	if (sched_setscheduler(aal_data->primary_data->sof_irq_event_task, SCHED_RR, &param))
		pr_notice("aal_sof_irq_event_task setschedule fail");

	wake_up_process(aal_data->primary_data->sof_irq_event_task);
}

void mtk_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct drm_display_mode *mode;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	mode = mtk_crtc_get_display_mode_by_comp(__func__, &mtk_crtc->base, comp, false);
	if ((mode != NULL) && (aal_data != NULL)) {
		aal_data->primary_data->fps = drm_mode_vrefresh(mode);
		DDPMSG("%s: first config set fps: %d\n", __func__, aal_data->primary_data->fps);
	}
	mtk_aal_config(comp, cfg, handle);
}

int mtk_aal_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	uint32_t force_delay_trigger;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	switch (cmd) {
	case FRAME_DIRTY:
	{
		if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1)
			disp_aal_set_interrupt(comp, 1, -1, handle);
	}
		break;
	case FORCE_TRIG_CTL:
	{
		force_delay_trigger = *(uint32_t *)params;
		atomic_set(&aal_data->primary_data->force_delay_check_trig, force_delay_trigger);
	}
		break;
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_aal *data = comp_to_aal(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_aal *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_aal(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		mtk_aal_data_init(comp);
		mtk_aal_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion) {
			mtk_aal_data_init(data->companion);
			mtk_aal_primary_data_init(data->companion);
		}
	}
		break;
	case NOTIFY_MODE_SWITCH:
	{
		struct mtk_modeswitch_param *modeswitch_param = (struct mtk_modeswitch_param *)params;

		aal_data->primary_data->fps = modeswitch_param->fps;
		AALFLOW_LOG("AAL_FPS_CHG fps:%d\n", aal_data->primary_data->fps);
	}
		break;
	case IRQ_LEVEL_ALL:
	case IRQ_LEVEL_NORMAL:
	{
		if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1)
			disp_aal_set_interrupt(comp, 1, -1, handle);
	}
		break;
	case IRQ_LEVEL_IDLE:
	{
		disp_aal_set_interrupt(comp, 0, -1, handle);
	}
		break;
	default:
		break;
	}
	AALFLOW_LOG("end\n");
	return 0;
}

static int mtl_aal_cfg_clarity_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (!aal_data->primary_data->disp_clarity_regs) {
		aal_data->primary_data->disp_clarity_regs =
				kmalloc(sizeof(struct DISP_CLARITY_REG), GFP_KERNEL);
		if (aal_data->primary_data->disp_clarity_regs == NULL) {
			DDPMSG("%s: no memory\n", __func__);
			return -EFAULT;
		}
	}

	if (data == NULL)
		return -EFAULT;

	mutex_lock(&aal_data->primary_data->clarity_lock);
	memcpy(aal_data->primary_data->disp_clarity_regs, (struct DISP_CLARITY_REG *)data,
		sizeof(struct DISP_CLARITY_REG));

	dumpAlgAALClarityRegOutput(*aal_data->primary_data->disp_clarity_regs);
	dumpAlgTDSHPRegOutput(*aal_data->primary_data->disp_clarity_regs);

	CRTC_MMP_EVENT_START(0, clarity_set_regs, 0, 0);
	if (mtk_disp_clarity_set_reg(comp, handle, aal_data->primary_data->disp_clarity_regs) < 0) {
		DDPMSG("[Pipe0] %s: clarity_set_reg failed\n", __func__);
		mutex_unlock(&aal_data->primary_data->clarity_lock);

		return -EFAULT;
	}
	if (comp->mtk_crtc->is_dual_pipe && aal_data->companion) {
		if (mtk_disp_clarity_set_reg(aal_data->companion, handle,
				aal_data->primary_data->disp_clarity_regs) < 0) {
			DDPMSG("[Pipe1] %s: clarity_set_reg failed\n", __func__);
			mutex_unlock(&aal_data->primary_data->clarity_lock);

			return -EFAULT;
		}
	}
	CRTC_MMP_EVENT_END(0, clarity_set_regs, 0, 3);
	mutex_unlock(&aal_data->primary_data->clarity_lock);
	return 0;
}

static int mtk_aal_cfg_set_param(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	int prev_backlight = 0;
	struct DISP_AAL_PARAM *param = (struct DISP_AAL_PARAM *) data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;
	int prev_elvsspn = 0;

	if (debug_skip_set_param) {
		pr_notice("skip_set_param for debug\n");
		return ret;
	}
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);
	/* Not need to protect g_aal_param, */
	/* since only AALService can set AAL parameters. */
	memcpy(&aal_data->primary_data->aal_param, param, sizeof(*param));

	prev_backlight = aal_data->primary_data->backlight_set;
	aal_data->primary_data->backlight_set = aal_data->primary_data->aal_param.FinalBacklight;
	prev_elvsspn = aal_data->primary_data->elvsspn_set;
	aal_data->primary_data->elvsspn_set = aal_data->primary_data->ess20_spect_param.ELVSSPN;

	mutex_lock(&aal_data->primary_data->sram_lock);
	ret = disp_aal_set_param(comp, handle, data);
	if (ret < 0)
		AALERR("SET_PARAM: fail\n");
	mutex_unlock(&aal_data->primary_data->sram_lock);

	atomic_set(&aal_data->primary_data->allowPartial,
				aal_data->primary_data->aal_param.allowPartial);

	if (atomic_read(&aal_data->primary_data->backlight_notified) == 0)
		aal_data->primary_data->backlight_set = 0;

	if ((prev_backlight == aal_data->primary_data->backlight_set) ||
			(aal_data->primary_data->led_type == TYPE_ATOMIC))
		aal_data->primary_data->ess20_spect_param.flag &= (~(1 << SET_BACKLIGHT_LEVEL));
	else
		aal_data->primary_data->ess20_spect_param.flag |= (1 << SET_BACKLIGHT_LEVEL);
	if (prev_elvsspn == aal_data->primary_data->elvsspn_set)
		aal_data->primary_data->ess20_spect_param.flag &= (~(1 << SET_ELVSS_PN));

	if (pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (aal_data->primary_data->aal_param.silky_bright_flag == 0) {
			AALAPI_LOG("connector_id:%d, bl:%d, silky_bright_flag:%d, ELVSSPN:%u, flag:%u\n",
				connector_id, aal_data->primary_data->backlight_set,
				aal_data->primary_data->aal_param.silky_bright_flag,
				aal_data->primary_data->ess20_spect_param.ELVSSPN,
				aal_data->primary_data->ess20_spect_param.flag);

			if(! aal_data->primary_data->ess20_spect_param.flag)
				mtk_leds_brightness_set(connector_id,
					aal_data->primary_data->backlight_set,
					aal_data->primary_data->ess20_spect_param.ELVSSPN,
					aal_data->primary_data->ess20_spect_param.flag);
		}
	} else if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS]) {
		//if (pre_bl != cur_bl)
		AALAPI_LOG("gian:%u, backlight:%d, ELVSSPN:%u, flag:%u\n",
			aal_data->primary_data->aal_param.silky_bright_gain[0],
			aal_data->primary_data->backlight_set,
			aal_data->primary_data->ess20_spect_param.ELVSSPN,
			aal_data->primary_data->ess20_spect_param.flag);

		mtk_cfg_trans_gain_to_gamma(mtk_crtc, handle,
			&aal_data->primary_data->aal_param,
			aal_data->primary_data->backlight_set,
			(void *)&aal_data->primary_data->ess20_spect_param);
	} else {
		AALAPI_LOG("connector_id:%d, pre_bl:%d, bl:%d, pn:%u, flag:%u\n",
			connector_id, prev_backlight, aal_data->primary_data->backlight_set,
			aal_data->primary_data->ess20_spect_param.ELVSSPN,
			aal_data->primary_data->ess20_spect_param.flag);

		if (aal_data->primary_data->ess20_spect_param.flag)
			mtk_leds_brightness_set(connector_id, aal_data->primary_data->backlight_set,
					aal_data->primary_data->ess20_spect_param.ELVSSPN,
					aal_data->primary_data->ess20_spect_param.flag);
	}

	return ret;
}

static int mtk_aal_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	/* TYPE2 user_cmd alone */
	case PQ_AAL_INIT_REG:
		ret = disp_aal_set_init_reg(comp, handle, (struct DISP_AAL_INITREG *) data);
		break;
	case PQ_AAL_CLARITY_SET_REG:
		ret = mtl_aal_cfg_clarity_set_reg(comp, handle, data, data_size);
		break;
	/* TYPE3 combine use_cmd & others*/
	case PQ_AAL_SET_PARAM:
		/* TODO move silky gamma to gamma */
		ret = mtk_aal_cfg_set_param(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_aal_pq_ioctl_transact(struct mtk_ddp_comp *comp,
		      unsigned int cmd, void *params, unsigned int size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_AAL_EVENTCTL:
		ret = mtk_drm_ioctl_aal_eventctl_impl(comp, params);
		break;
	case PQ_AAL_INIT_DRE30:
		ret = mtk_drm_ioctl_aal_init_dre30_impl(comp, params);
		break;
	case PQ_AAL_GET_HIST:
		ret = mtk_drm_ioctl_aal_get_hist_impl(comp, params);
		break;
	case PQ_AAL_GET_SIZE:
		ret = mtk_drm_ioctl_aal_get_size_impl(comp, params);
		break;
	case PQ_AAL_SET_ESS20_SPECT_PARAM:
		ret = mtk_drm_ioctl_aal_set_ess20_spect_param_impl(comp, params);
		break;
	case PQ_AAL_INIT_REG:
		ret = mtk_drm_ioctl_aal_init_reg_impl(comp, params);
		break;
	case PQ_AAL_CLARITY_SET_REG:
		ret = mtk_drm_ioctl_clarity_set_reg_impl(comp, params);
		break;
	case PQ_AAL_SET_PARAM:
		ret = mtk_drm_ioctl_aal_set_param_impl(comp, params);
		break;
	case PQ_AAL_SET_TRIGGER_STATE:
		ret = mtk_drm_ioctl_aal_set_trigger_state_impl(comp, params);
		break;
	case PQ_AAL_GET_BASE_VOLTAGE:
		ret = mtk_drm_ioctl_aal_get_base_voltage(comp, params);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_disp_aal_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s set partial update, height:%d, enable:%d\n",
			mtk_dump_comp_str(comp), partial_roi.height, enable);

	aal_data->set_partial_update = enable;
	aal_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : aal_data->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (aal_data->set_partial_update) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_SIZE, aal_data->roi_height + overhead_v * 2, 0x0FFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_SIZE, aal_data->roi_height + overhead_v * 2, 0x0FFF);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_SIZE, full_height, 0x0FFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_SIZE, full_height, 0x0FFF);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_aal_funcs = {
	.config = mtk_aal_config,
	.first_cfg = mtk_aal_first_cfg,
	.start = mtk_aal_start,
	.stop = mtk_aal_stop,
	.bypass = mtk_aal_bypass,
	.user_cmd = mtk_aal_user_cmd,
	.io_cmd = mtk_aal_io_cmd,
	.prepare = mtk_aal_prepare,
	.unprepare = mtk_aal_unprepare,
	.config_overhead = mtk_disp_aal_config_overhead,
	.config_overhead_v = mtk_disp_aal_config_overhead_v,
	.pq_frame_config = mtk_aal_pq_frame_config,
	.pq_ioctl_transact = mtk_aal_pq_ioctl_transact,
	.mutex_sof_irq = disp_aal_on_start_of_frame,
	.partial_update = mtk_disp_aal_set_partial_update,
};

static int mtk_disp_aal_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	if (!g_drm_dev)
		g_drm_dev = drm_dev;
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_aal_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_aal_component_ops = {
	.bind	= mtk_disp_aal_bind,
	.unbind = mtk_disp_aal_unbind,
};

void mtk_aal_dump(struct mtk_ddp_comp *comp)
{
	void __iomem  *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, 0x200, 0x10);
}

void mtk_aal_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem  *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp),
			comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x580; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (aal_data->primary_data->isDualPQ && aal_data->companion) {
		baddr = aal_data->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(aal_data->companion),
				aal_data->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(aal_data->companion));
		for (k = 0; k <= 0x580; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(aal_data->companion));
	}
}

void disp_aal_on_end_of_frame(struct mtk_ddp_comp *comp, unsigned int status)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	//For 120Hz rotation issue
	ktime_get_ts64(&aal_data->primary_data->start);

	atomic_set(&aal_data->eof_irq, 1);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support
			&& aal_data->primary_data->dre30_enabled) {
		disp_aal_read_single_hist(comp);
		disp_aal_dre3_irq_handle(comp);
	} else
		disp_aal_single_pipe_hist_update(comp, status);

	AALIRQ_LOG("[SRAM] clean dre_config in (EOF)  comp->id = %d dre_en %d\n", comp->id,
		aal_data->primary_data->dre30_enabled);

	atomic_set(&aal_data->dre_config, 0);
}

static void disp_aal_wait_sof_irq(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	int ret = 0;
	int pm_ret = 0;
	int aal_lock = 0;
	int retry = 5;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (atomic_read(&aal_data->primary_data->sof_irq_available) == 0) {
		AALFLOW_LOG("wait_event_interruptible\n");
		ret = wait_event_interruptible(aal_data->primary_data->sof_irq_wq,
				atomic_read(&aal_data->primary_data->sof_irq_available) == 1);
		AALFLOW_LOG("sof_irq_available = 1, waken up, ret = %d\n", ret);
	} else {
		AALFLOW_LOG("sof_irq_available = 0\n");
		return;
	}

	CRTC_MMP_EVENT_START(0, aal_sof_thread, 0, 0);

	AALIRQ_LOG("[SRAM] g_aal_dre_config(%d) in SOF\n",
			atomic_read(&aal_data->dre_config));
	pm_ret = mtk_vidle_pq_power_get(__func__);
	mutex_lock(&aal_data->primary_data->sram_lock);
	spin_lock_irqsave(&aal_data->primary_data->clock_lock, flags);
	if (atomic_read(&aal_data->is_clock_on) != 1)
		AALIRQ_LOG("clock is off\n");
	else
		disp_aal_update_dre3_sram(comp, true);
	spin_unlock_irqrestore(&aal_data->primary_data->clock_lock,
			flags);
	if (!aal_data->primary_data->isDualPQ)
		mutex_unlock(&aal_data->primary_data->sram_lock);

	CRTC_MMP_MARK(0, aal_sof_thread, 0, 1);

	if (aal_data->primary_data->isDualPQ) {
		struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

		AALIRQ_LOG("[SRAM] g_aal1_dre_config(%d) in SOF\n",
			atomic_read(&aal1_data->dre_config));
		while ((!aal_lock) && (retry != 0)) {
			aal_lock = spin_trylock_irqsave(&aal1_data->primary_data->clock_lock,
							flags);
			if (!aal_lock) {
				usleep_range(500, 1000);
				retry--;
			}
		}
		if (aal_lock) {
			if (atomic_read(&aal1_data->is_clock_on) != 1)
				AALIRQ_LOG("aal1 clock is off\n");
			else
				disp_aal_update_dre3_sram(aal_data->companion, true);

			spin_unlock_irqrestore(&aal1_data->primary_data->clock_lock,
				flags);
		}
		mutex_unlock(&aal1_data->primary_data->sram_lock);
		CRTC_MMP_MARK(0, aal_sof_thread, 0, 2);

		if (atomic_read(&aal_data->first_frame) == 1 &&
			atomic_read(&aal1_data->first_frame) == 1 &&
			aal_data->primary_data->dre30_enabled) {
			mtk_crtc_user_cmd(aal_data->crtc, comp, FLIP_SRAM, NULL);
			mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
			atomic_set(&aal_data->first_frame, 0);
			atomic_set(&aal1_data->first_frame, 0);
			if (!pm_ret)
				mtk_vidle_pq_power_put(__func__);
			CRTC_MMP_EVENT_END(0, aal_sof_thread, 0, 3);
			return;
		}
	} else {
		if (atomic_read(&aal_data->first_frame) == 1 &&
			aal_data->primary_data->dre30_enabled) {
			AALIRQ_LOG("aal_refresh_task\n");
			mtk_crtc_user_cmd(aal_data->crtc, comp, FLIP_SRAM, NULL);
			mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
			atomic_set(&aal_data->first_frame, 0);
			if (!pm_ret)
				mtk_vidle_pq_power_put(__func__);
			CRTC_MMP_EVENT_END(0, aal_sof_thread, 0, 4);
			return;
		}
	}

	if (atomic_read(&aal_data->primary_data->irq_en) == 1) {
		if (!atomic_read(&aal_data->first_frame) && aal_data->primary_data->dre30_enabled)
			mtk_crtc_user_cmd(aal_data->crtc, comp, FLIP_SRAM, NULL);
	}

	if (atomic_read(&aal_data->primary_data->dre30_write) == 1) {
		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
		atomic_set(&aal_data->primary_data->dre30_write, 0);
	}
	if (!pm_ret)
		mtk_vidle_pq_power_put(__func__);
	CRTC_MMP_EVENT_END(0, aal_sof_thread, 0, 5);
}

static void disp_aal_on_start_of_frame(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	if (atomic_read(&aal_data->primary_data->irq_en) == 0 ||
			atomic_read(&aal_data->primary_data->should_stop)) {
		atomic_set(&aal_data->primary_data->eof_irq_en, 0);
		AALIRQ_LOG("%s, skip irq\n", __func__);
		return;
	}
	atomic_set(&aal_data->primary_data->eof_irq_en, 1);

	if (!aal_data->primary_data->aal_fo->mtk_dre30_support
		|| !aal_data->primary_data->dre30_enabled) {


		if (aal_data->primary_data->isDualPQ) {
			struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

			if (atomic_read(&aal_data->dre20_hist_is_ready) &&
				atomic_read(&aal1_data->dre20_hist_is_ready)) {
				atomic_set(&aal_data->hist_available, 1);
				atomic_set(&aal1_data->hist_available, 1);
				wake_up_interruptible(&aal_data->primary_data->hist_wq);
			}
		} else {
			if (atomic_read(&aal_data->dre20_hist_is_ready)) {
				atomic_set(&aal_data->hist_available, 1);
				//if (atomic_read(&g_aal_hist_wait_dualpipe) == 1)
				//	atomic_set(&g_aal1_hist_available, 1);
				wake_up_interruptible(&aal_data->primary_data->hist_wq);
			}
		}
		return;
	}

	if (atomic_read(&aal_data->primary_data->force_relay) == 1 &&
		!pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])
		return;
	if (atomic_read(&aal_data->primary_data->change_to_dre30) != 0x3)
		return;
	if (aal_data->primary_data->sram_method != AAL_SRAM_SOF)
		return;

	if (!atomic_read(&aal_data->primary_data->sof_irq_available)) {
		atomic_set(&aal_data->primary_data->sof_irq_available, 1);
		wake_up_interruptible(&aal_data->primary_data->sof_irq_wq);
	}
}

static int mtk_aal_sof_irq_trigger(void *data)
{
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	while (!kthread_should_stop()) {
		disp_aal_wait_sof_irq(comp);
		atomic_set(&aal_data->primary_data->sof_irq_available, 0);
	}

	return 0;
}

static irqreturn_t mtk_disp_aal_irq_handler(int irq, void *dev_id)
{
	unsigned int status0 = 0, status1 = 0;
	struct mtk_disp_aal *aal = dev_id;
	struct mtk_ddp_comp *comp = &aal->ddp_comp;
	struct mtk_ddp_comp *comp1 = aal->companion;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	irqreturn_t ret = IRQ_NONE;

	if (IS_ERR_OR_NULL(aal))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get("aal_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}
	mtk_crtc = aal->ddp_comp.mtk_crtc;
	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is NULL\n", __func__);
		ret = IRQ_NONE;
		goto out;
	}

	status0 = disp_aal_read_clear_irq(comp);
	if (mtk_crtc->is_dual_pipe && comp1)
		status1 = disp_aal_read_clear_irq(comp1);

	AALIRQ_LOG("irq, val:0x%x,0x%x\n", status0, status1);

	if (atomic_read(&aal->primary_data->eof_irq_en) == 0) {
		AALIRQ_LOG("%s, skip irq\n", __func__);
		ret = IRQ_HANDLED;
		goto out;
	}

	if (atomic_read(&aal->primary_data->force_relay) == 1) {
		AALIRQ_LOG("aal is force_relay\n");
		disp_aal_set_interrupt(comp, 0, -1, NULL);
		ret = IRQ_HANDLED;
		goto out;
	}

	DRM_MMP_MARK(IRQ, irq, status0);
	DRM_MMP_MARK(aal0, status0, status1);

	disp_aal_on_end_of_frame(comp, status0);
	if (mtk_crtc->is_dual_pipe && comp1)
		disp_aal_on_end_of_frame(comp1, status1);

	DRM_MMP_MARK(aal0, status0, 1);

	ret = IRQ_HANDLED;
out:
	mtk_drm_top_clk_isr_put("aal_irq");

	return ret;
}

static int mtk_disp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_aal *priv;
	enum mtk_ddp_comp_id comp_id;
	struct device_node *tdshp_node;
	int ret, irq;
	struct device_node *dre3_dev_node;
	struct platform_device *dre3_pdev;
	struct resource dre3_res;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		AALERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_AAL);
	if ((int)comp_id < 0) {
		AALERR("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	atomic_set(&priv->is_clock_on, 0);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		AALERR("Failed to get irq %d\n", ret);
		goto error_primary;
	}

	priv->primary_data->aal_fo = devm_kzalloc(dev, sizeof(*priv->primary_data->aal_fo),
							GFP_KERNEL);
	if (priv->primary_data->aal_fo == NULL) {
		ret = -ENOMEM;
		AALERR("Failed to alloc aal_fo %d\n", ret);
		goto error_primary;
	}
	// for Display Clarity
	tdshp_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_tdshp0");
	if (!of_property_read_u32(tdshp_node, "mtk-tdshp-clarity-support",
		&priv->primary_data->tdshp_clarity_support)) {
		DDPMSG("disp_tdshp: mtk_tdshp_clarity_support = %d\n",
			priv->primary_data->tdshp_clarity_support);
	}

	if (of_property_read_u32(dev->of_node, "mtk-aal-support",
		&priv->primary_data->aal_fo->mtk_aal_support)) {
		AALERR("comp_id: %d, mtk_aal_support = %d\n",
			comp_id, priv->primary_data->aal_fo->mtk_aal_support);
		priv->primary_data->aal_fo->mtk_aal_support = 0;
	}

	if (of_property_read_u32(dev->of_node, "mtk-dre30-support",
		&priv->primary_data->aal_fo->mtk_dre30_support)) {
		AALERR("comp_id: %d, mtk_dre30_support = %d\n",
				comp_id, priv->primary_data->aal_fo->mtk_dre30_support);
		priv->primary_data->aal_fo->mtk_dre30_support = 0;
	} else {
		if (priv->primary_data->aal_fo->mtk_dre30_support) {
			if (of_property_read_u32(dev->of_node, "aal-dre3-en",
					&priv->primary_data->dre30_en)) {
				priv->primary_data->dre30_enabled = true;
			} else
				priv->primary_data->dre30_enabled =
					(priv->primary_data->dre30_en == 1) ? true : false;

			// for Display Clarity
			if (!of_property_read_u32(dev->of_node, "mtk-aal-clarity-support",
					&priv->primary_data->aal_clarity_support))
				DDPMSG("mtk_aal_clarity_support = %d\n",
						priv->primary_data->aal_clarity_support);

			if ((priv->primary_data->aal_clarity_support == 1)
					&& (priv->primary_data->tdshp_clarity_support == 1)) {
				priv->primary_data->disp_clarity_support = 1;
				DDPMSG("%s: display clarity support = %d\n",
					__func__, priv->primary_data->disp_clarity_support);
			}
		}
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_aal_funcs);
	if (ret) {
		AALERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);


	ret = devm_request_irq(dev, irq, mtk_disp_aal_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	do {
		const char *clock_name;

		if (!priv->primary_data->aal_fo->mtk_dre30_support) {
			pr_notice("[debug] dre30 is not support\n");
			break;
		}
		dre3_dev_node = of_parse_phandle(
			pdev->dev.of_node, "aal-dre3", 0);
		if (dre3_dev_node)
			pr_notice("found dre3 aal node, it's another hw\n");
		else
			break;
		dre3_pdev = of_find_device_by_node(dre3_dev_node);
		if (dre3_pdev)
			pr_notice("found dre3 aal device, it's another hw\n");
		else
			break;
		of_node_put(dre3_dev_node);
		priv->dre3_hw.dev = &dre3_pdev->dev;
		priv->dre3_hw.va = of_iomap(dre3_pdev->dev.of_node, 0);
		if (!priv->dre3_hw.va) {
			pr_notice("cannot found allocate dre3 va!\n");
			break;
		}
		ret = of_address_to_resource(
			dre3_pdev->dev.of_node, 0, &dre3_res);
		if (ret) {
			pr_notice("cannot found allocate dre3 resource!\n");
			break;
		}
		priv->dre3_hw.pa = dre3_res.start;

		if (of_property_read_string(dre3_dev_node, "clock-names", &clock_name))
			priv->dre3_hw.clk = of_clk_get_by_name(dre3_dev_node, clock_name);

		if (IS_ERR(priv->dre3_hw.clk)) {
			pr_notice("fail @ dre3 clock. name:%s\n",
				"DRE3_AAL0");
			break;
		}
		pr_notice("dre3 dev:%p va:%p pa:%pa", priv->dre3_hw.dev,
			priv->dre3_hw.va, &priv->dre3_hw.pa);
	} while (0);

	ret = component_add(dev, &mtk_disp_aal_component_ops);
	if (ret) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	AALFLOW_LOG("-\n");
error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);
	return ret;
}

static int mtk_disp_aal_remove(struct platform_device *pdev)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_aal_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (priv->ddp_comp.id == DDP_COMPONENT_AAL0)
		mtk_leds_unregister_notifier(&leds_init_notifier);
#endif

	return 0;
}

static const struct mtk_disp_aal_data mt6885_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.aal_dre_hist_start = 1152,
	.aal_dre_hist_end   = 4220,
	.aal_dre_gain_start = 4224,
	.aal_dre_gain_end   = 6396,
	.bitShift = 13,
};

static const struct mtk_disp_aal_data mt6873_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6853_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6833_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6983_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6895_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6879_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6855_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6985_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6897_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6886_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6989_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.mdp_aal_ghist_support = true,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6878_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.mdp_aal_ghist_support = false,
	.bitShift = 16,
};

static const struct of_device_id mtk_disp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-disp-aal",
	  .data = &mt6885_aal_driver_data},
	{ .compatible = "mediatek,mt6873-disp-aal",
	  .data = &mt6873_aal_driver_data},
	{ .compatible = "mediatek,mt6853-disp-aal",
	  .data = &mt6853_aal_driver_data},
	{ .compatible = "mediatek,mt6833-disp-aal",
	  .data = &mt6833_aal_driver_data},
	{ .compatible = "mediatek,mt6983-disp-aal",
	  .data = &mt6983_aal_driver_data},
	{ .compatible = "mediatek,mt6895-disp-aal",
	  .data = &mt6895_aal_driver_data},
	{ .compatible = "mediatek,mt6879-disp-aal",
	  .data = &mt6879_aal_driver_data},
	{ .compatible = "mediatek,mt6855-disp-aal",
	  .data = &mt6855_aal_driver_data},
	{ .compatible = "mediatek,mt6985-disp-aal",
	  .data = &mt6985_aal_driver_data},
	{ .compatible = "mediatek,mt6886-disp-aal",
	  .data = &mt6886_aal_driver_data},
	{ .compatible = "mediatek,mt6835-disp-aal",
	  .data = &mt6835_aal_driver_data},
	{ .compatible = "mediatek,mt6897-disp-aal",
	  .data = &mt6897_aal_driver_data},
	{ .compatible = "mediatek,mt6989-disp-aal",
	  .data = &mt6989_aal_driver_data},
	{ .compatible = "mediatek,mt6878-disp-aal",
	  .data = &mt6878_aal_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_aal_driver_dt_match);

struct platform_driver mtk_disp_aal_driver = {
	.probe		= mtk_disp_aal_probe,
	.remove		= mtk_disp_aal_remove,
	.driver		= {
		.name	= "mediatek-disp-aal",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_aal_driver_dt_match,
	},
};

/* Legacy AAL_SUPPORT_KERNEL_API */
void disp_aal_set_lcm_type(unsigned int panel_type)
{
/*
	unsigned long flags;

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	atomic_set(&g_aal_panel_type, panel_type);
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	AALAPI_LOG("panel_type = %d\n", panel_type);
*/
}

#define AAL_CONTROL_CMD(ID, CONTROL) (ID << 16 | CONTROL)
void disp_aal_set_ess_level(struct mtk_ddp_comp *comp, int level)
{
	unsigned long flags;
	int level_command = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	aal_data->primary_data->ess_level_cmd_id += 1;
	aal_data->primary_data->ess_level_cmd_id = aal_data->primary_data->ess_level_cmd_id % 64;
	level_command = AAL_CONTROL_CMD(aal_data->primary_data->ess_level_cmd_id, level);

	aal_data->primary_data->ess_level = level_command;

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	disp_aal_refresh_by_kernel(aal_data, 1);
	AALAPI_LOG("level = %d (cmd = 0x%x)\n", level, level_command);
}

void disp_aal_set_ess_en(struct mtk_ddp_comp *comp, int enable)
{
	unsigned long flags;
	int enable_command = 0;
	int level_command = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	aal_data->primary_data->ess_en_cmd_id += 1;
	aal_data->primary_data->ess_en_cmd_id = aal_data->primary_data->ess_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(aal_data->primary_data->ess_en_cmd_id, enable);

	aal_data->primary_data->ess_en = enable_command;

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	disp_aal_refresh_by_kernel(aal_data, 1);
	AALAPI_LOG("en = %d (cmd = 0x%x) level = 0x%08x (cmd = 0x%x)\n",
		enable, enable_command, ESS_LEVEL_BY_CUSTOM_LIB, level_command);
}

void disp_aal_set_dre_en(struct mtk_ddp_comp *comp, int enable)
{
	unsigned long flags;
	int enable_command = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	aal_data->primary_data->dre_en_cmd_id += 1;
	aal_data->primary_data->dre_en_cmd_id = aal_data->primary_data->dre_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(aal_data->primary_data->dre_en_cmd_id, enable);

	aal_data->primary_data->dre_en = enable_command;

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	disp_aal_refresh_by_kernel(aal_data, 1);
	AALAPI_LOG("en = %d (cmd = 0x%x)\n", enable, enable_command);
}

void disp_aal_debug(struct drm_crtc *crtc, const char *opt)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;
	int gain_offset;
	int arry_offset;
	unsigned int gain_arry[4];
	unsigned int *gain_pr;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return;
	}
	aal_data = comp_to_aal(comp);

	pr_notice("[debug]: %s\n", opt);
	if (strncmp(opt, "setparam:", 9) == 0) {
		debug_skip_set_param = strncmp(opt + 9, "skip", 4) == 0;
		pr_notice("[debug] skip_set_param=%d\n",
			debug_skip_set_param);
	} else if (strncmp(opt, "dre3irq:", 8) == 0) {
		debug_skip_dre3_irq = strncmp(opt + 8, "skip", 4) == 0;
		pr_notice("[debug] skip_dre3_irq=%d\n",
			debug_skip_dre3_irq);
	} else if (strncmp(opt, "dre3algmode:", 12) == 0) {
		debug_bypass_alg_mode = strncmp(opt + 12, "bypass", 6) == 0;
		pr_notice("[debug] bypass_alg_mode=%d\n",
			debug_bypass_alg_mode);
	} else if (strncmp(opt, "dumpregirq", 10) == 0) {
		debug_dump_reg_irq = true;
		pr_notice("[debug] debug_dump_reg_irq=%d\n",
			debug_dump_reg_irq);
	} else if (strncmp(opt, "dumpdre3hist:", 13) == 0) {
		if (sscanf(opt + 13, "%d,%d\n",
			&dump_blk_x, &dump_blk_y) == 2)
			pr_notice("[debug] dump_blk_x=%d dump_blk_y=%d\n",
				dump_blk_x, dump_blk_y);
		else
			pr_notice("[debug] dump_blk parse fail\n");
	} else if (strncmp(opt, "dumpdre3gain", 12) == 0) {
		arry_offset = 0;
		gain_pr = aal_data->primary_data->dre30_gain.dre30_gain;
		for (gain_offset = aal_data->data->aal_dre_gain_start;
			gain_offset <= aal_data->data->aal_dre_gain_end;
				gain_offset += 16, arry_offset += 4) {
			if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
				break;
			if (arry_offset + 4 <= AAL_DRE30_GAIN_REGISTER_NUM) {
				memcpy(gain_arry, gain_pr + arry_offset, sizeof(unsigned int) * 4);
			} else {
				memset(gain_arry, 0, sizeof(gain_arry));
				memcpy(gain_arry, gain_pr + arry_offset,
					sizeof(unsigned int) * (arry_offset + 5 - AAL_DRE30_GAIN_REGISTER_NUM));
			}
			DDPMSG("[debug] dre30_gain 0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n",
					arry_offset, gain_arry[0], gain_arry[1], gain_arry[2], gain_arry[3]);
		}
	} else if (strncmp(opt, "first_br:", 9) == 0) {
		debug_skip_first_br = strncmp(opt + 9, "skip", 4) == 0;
		pr_notice("[debug] skip_first_br=%d\n",
			debug_skip_first_br);
	} else if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[debug] debug_flow_log=%d\n",
			debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[debug] debug_api_log=%d\n",
			debug_api_log);
	} else if (strncmp(opt, "write_cmdq_log:", 15) == 0) {
		debug_write_cmdq_log = strncmp(opt + 15, "1", 1) == 0;
		pr_notice("[debug] debug_write_cmdq_log=%d\n",
			debug_write_cmdq_log);
	} else if (strncmp(opt, "irq_log:", 8) == 0) {
		debug_irq_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[debug] debug_irq_log=%d\n",
			debug_irq_log);
	} else if (strncmp(opt, "bypass_mode:", 12) == 0) {
		g_aal_bypass_mode = strncmp(opt + 12, "1", 1) == 0;
		pr_notice("[debug] g_aal_bypass_mode=%d\n",
			g_aal_bypass_mode);
	} else if (strncmp(opt, "dump_aal_hist:", 14) == 0) {
		debug_dump_aal_hist = strncmp(opt + 14, "1", 1) == 0;
		pr_notice("[debug] debug_dump_aal_hist=%d\n",
			debug_dump_aal_hist);
	} else if (strncmp(opt, "dump_input_param:", 17) == 0) {
		debug_dump_input_param = strncmp(opt + 17, "1", 1) == 0;
		pr_notice("[debug] debug_dump_input_param=%d\n",
			debug_dump_input_param);
	} else if (strncmp(opt, "set_ess_level:", 14) == 0) {
		int debug_ess_level;

		if (sscanf(opt + 14, "%d", &debug_ess_level) == 1) {
			pr_notice("[debug] ess_level=%d\n", debug_ess_level);
			disp_aal_set_ess_level(comp, debug_ess_level);
		} else
			pr_notice("[debug] set_ess_level failed\n");
	} else if (strncmp(opt, "set_ess_en:", 11) == 0) {
		bool debug_ess_en;

		debug_ess_en = !strncmp(opt + 11, "1", 1);
		pr_notice("[debug] debug_ess_en=%d\n", debug_ess_en);
		disp_aal_set_ess_en(comp, debug_ess_en);
	} else if (strncmp(opt, "set_dre_en:", 11) == 0) {
		bool debug_dre_en;

		debug_dre_en = !strncmp(opt + 11, "1", 1);
		pr_notice("[debug] debug_dre_en=%d\n", debug_dre_en);
		disp_aal_set_dre_en(comp, debug_dre_en);
	} else if (strncmp(opt, "aal_sram_method:", 16) == 0) {
		bool aal_align_eof;

		if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
			aal_align_eof = !strncmp(opt + 11, "0", 1);
			aal_data->primary_data->sram_method = aal_align_eof ?
						AAL_SRAM_EOF : AAL_SRAM_SOF;
			pr_notice("[debug] aal_sram_method=%d\n",
					aal_data->primary_data->sram_method);
		} else {
			pr_notice("[debug] dre30 is not support\n");
		}
	} else if (strncmp(opt, "dump_clarity_regs:", 18) == 0) {
		debug_dump_clarity_regs = strncmp(opt + 18, "1", 1) == 0;
		pr_notice("[debug] debug_dump_clarity_regs=%d\n",
			debug_dump_clarity_regs);
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		pr_notice("[debug] skip_set_param=%d\n",
			debug_skip_set_param);
		pr_notice("[debug] skip_dre3_irq=%d\n",
			debug_skip_dre3_irq);
		pr_notice("[debug] bypass_alg_mode=%d\n",
			debug_bypass_alg_mode);
		pr_notice("[debug] debug_dump_reg_irq=%d\n",
			debug_dump_reg_irq);
		pr_notice("[debug] dump_blk_x=%d dump_blk_y=%d\n",
			dump_blk_x, dump_blk_y);
		pr_notice("[debug] skip_first_br=%d\n",
			debug_skip_first_br);
		pr_notice("[debug] debug_flow_log=%d\n",
			debug_flow_log);
		pr_notice("[debug] debug_api_log=%d\n",
			debug_api_log);
		pr_notice("[debug] debug_write_cmdq_log=%d\n",
			debug_write_cmdq_log);
		pr_notice("[debug] debug_irq_log=%d\n",
			debug_irq_log);
		pr_notice("[debug] debug_dump_aal_hist=%d\n",
			debug_dump_aal_hist);
		pr_notice("[debug] debug_dump_input_param=%d\n",
			debug_dump_input_param);
		pr_notice("[debug] debug_ess_level=%d\n", aal_data->primary_data->ess_level);
		pr_notice("[debug] debug_ess_en=%d\n", aal_data->primary_data->ess_en);
		pr_notice("[debug] debug_dre_en=%d\n", aal_data->primary_data->dre_en);
		pr_notice("[debug] debug_dump_clarity_regs=%d\n", debug_dump_clarity_regs);
	}
}

void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return;
	}

	aal_data = comp_to_aal(comp);
	if (atomic_read(&aal_data->primary_data->force_relay) == bypass)
		return;

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_AAL, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

unsigned int disp_aal_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	aal_data = comp_to_aal(comp);

	return atomic_read(&aal_data->primary_data->force_relay);
}
