/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Interface file between DECON and DSIM for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <uapi/linux/sched/types.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>
#include <linux/irq.h>
#include <drm/drm_edid.h>
#include <media/v4l2-subdev.h>
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS)
#include <soc/samsung/exynos-alt.h>
#endif

#include "decon.h"
#include "dsim.h"
#include "dpp.h"
#include "format.h"
//#include "../../../../soc/samsung/pwrcal/pwrcal.h"
//#include "../../../../soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"
#include "../../../../../kernel/irq/internals.h"
#include "./panels/exynos_panel_drv.h"
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS)
struct task_struct *devfreq_change_task
#endif

/* DECON irq handler for DSI interface */
static irqreturn_t decon_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	u32 irq_sts_reg;
	u32 ext_irq = 0;
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	ktime_t timestamp = ktime_get();
#endif

	if (IS_ERR_OR_NULL(decon)) {
		decon_err("%s decon has null pointer\n", __func__);
		BUG();
	}

	spin_lock(&decon->slock);
	if (IS_DECON_OFF_STATE(decon))
		goto irq_end;

	irq_sts_reg = decon_reg_get_interrupt_and_clear(decon->id, &ext_irq);
	decon_dbg("%s: irq_sts_reg = %x, ext_irq = %x\n", __func__,
			irq_sts_reg, ext_irq);

	if (irq_sts_reg & DPU_FRAME_START_INT_PEND) {
		/* VSYNC interrupt, accept it */
		decon->frame_cnt++;
		decon_dbg("Decon FrameStart(%d)\n", decon->frame_cnt);
		wake_up_interruptible_all(&decon->wait_vstatus);
		if (decon->state == DECON_STATE_TUI)
			decon_info("%s:%d TUI Frame Start\n", __func__, __LINE__);
	}

	if (irq_sts_reg & DPU_FRAME_DONE_INT_PEND) {
		DPU_EVENT_LOG(DPU_EVT_DECON_FRAMEDONE, &decon->sd, ktime_set(0, 0));
		DPU_DEBUG_DMA_BUF("frame_done\n");
		decon->hiber.frame_cnt++;
		decon_hiber_trig_reset(decon);
		if (decon->state == DECON_STATE_TUI)
			decon_info("%s:%d TUI Frame Done\n", __func__, __LINE__);
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
		decon->fsync.timestamp = timestamp;
		wake_up_interruptible_all(&decon->fsync.wait);
#endif
	}

	if (ext_irq & DPU_RESOURCE_CONFLICT_INT_PEND)
		DPU_EVENT_LOG(DPU_EVT_RSC_CONFLICT, &decon->sd, ktime_set(0, 0));

	if (ext_irq & DPU_TIME_OUT_INT_PEND) {
		decon_err("%s: DECON%d timeout irq occurs\n", __func__, decon->id);
#if defined(DPU_DUMP_BUFFER_IRQ)
		dpu_dump_afbc_info();
#endif
		BUG();
	}

irq_end:
	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS)
static int decon_devfreq_change_task(void *data)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);

		schedule();

		set_current_state(TASK_RUNNING);

		exynos_alt_call_chain();
	}

	return 0;
}
#endif

int decon_register_irq(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct platform_device *pdev;
	struct resource *res;
	int ret = 0;

	pdev = container_of(dev, struct platform_device, dev);

	/* 1: FRAME START */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, decon_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err("failed to install FRAME START irq\n");
		return ret;
	}

	/* 2: FRAME DONE */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	ret = devm_request_irq(dev, res->start, decon_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err("failed to install FRAME DONE irq\n");
		return ret;
	}

	/* 3: EXTRA: resource conflict, timeout and error irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	ret = devm_request_irq(dev, res->start, decon_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err("failed to install EXTRA irq\n");
		return ret;
	}

	/*
	 * If below IRQs are needed, please define irq number sequence
	 * like below
	 *
	 * DECON0
	 * 4: DIMMING_START
	 * 5: DIMMING_END
	 * 6: DQE_DIMMING_START
	 * 7: DQE_DIMMING_END
	 *
	 * DECON2
	 * 4: VSTATUS
	 */

	return ret;
}

int decon_get_clocks(struct decon_device *decon)
{
	return 0;
}

void decon_set_clocks(struct decon_device *decon)
{
}

void dpu_update_fps(struct decon_device *decon, u32 fps)
{
	if (fps == 0)
		return;

	if (decon->dt.out_type != DECON_OUT_DSI)
		return;

	decon_info("requested fps(%d), current fps(%d)\n",
			fps, decon->lcd_info->fps);
	v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			DSIM_IOC_SET_VIDEO_FPS, &fps);
	decon_info("applied fps(%d)\n", decon->lcd_info->fps);
}

int decon_get_out_sd(struct decon_device *decon)
{
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	int decon_error_cb(struct decon_device *decon,
			struct disp_check_cb_info *info);
	int decon_error_cb_by_fb_blank(struct decon_device *decon,
			struct disp_check_cb_info *info);

	struct disp_error_cb_info decon_error_cb_info = {
#if defined(CONFIG_SUPPORT_ERRFG_RECOVERY_BY_FB_BLANK)
		.error_cb = (disp_error_cb *)decon_error_cb_by_fb_blank,
#else
		.error_cb = (disp_error_cb *)decon_error_cb,
#endif
		.data = decon,
	};
#endif
	decon->out_sd[0] = decon->dsim_sd[decon->dt.out_idx[0]];
	if (IS_ERR_OR_NULL(decon->out_sd[0])) {
		decon_err("failed to get dsim%d sd\n", decon->dt.out_idx[0]);
		return -ENOMEM;
	}

	if (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) {
		decon->out_sd[1] = decon->dsim_sd[decon->dt.out_idx[1]];
		if (IS_ERR_OR_NULL(decon->out_sd[1])) {
			decon_err("failed to get 2nd dsim%d sd\n",
					decon->dt.out_idx[1]);
			return -ENOMEM;
		}
	}

	v4l2_subdev_call(decon->out_sd[0], core, ioctl, DSIM_IOC_GET_LCD_INFO, NULL);
	decon->lcd_info =
		(struct exynos_panel_info *)v4l2_get_subdev_hostdata(decon->out_sd[0]);
	if (IS_ERR_OR_NULL(decon->lcd_info)) {
		decon_err("failed to get lcd information\n");
		return -EINVAL;
	}

	decon_info("lcd_info: hfp %d hbp %d hsa %d vfp %d vbp %d vsa %d",
			decon->lcd_info->hfp, decon->lcd_info->hbp,
			decon->lcd_info->hsa, decon->lcd_info->vfp,
			decon->lcd_info->vbp, decon->lcd_info->vsa);
	decon_info("xres %d yres %d\n",
			decon->lcd_info->xres, decon->lcd_info->yres);

#ifdef CONFIG_EXYNOS_COMMON_PANEL
		v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				DSIM_IOC_SET_ERROR_CB, &decon_error_cb_info);
#endif

	return 0;
}

int decon_get_pinctrl(struct decon_device *decon)
{
	int ret = 0;

	if ((decon->dt.out_type != DECON_OUT_DSI) ||
			(decon->dt.psr_mode == DECON_VIDEO_MODE) ||
			(decon->dt.trig_mode != DECON_HW_TRIG)) {
		decon_warn("decon%d doesn't need pinctrl\n", decon->id);
		return 0;
	}

	decon->res.pinctrl = devm_pinctrl_get(decon->dev);
	if (IS_ERR(decon->res.pinctrl)) {
		decon_err("failed to get decon-%d pinctrl\n", decon->id);
		ret = PTR_ERR(decon->res.pinctrl);
		decon->res.pinctrl = NULL;
		goto err;
	}

	decon->res.hw_te_on = pinctrl_lookup_state(decon->res.pinctrl, "hw_te_on");
	if (IS_ERR(decon->res.hw_te_on)) {
		decon_err("failed to get hw_te_on pin state\n");
		ret = PTR_ERR(decon->res.hw_te_on);
		decon->res.hw_te_on = NULL;
		goto err;
	}
	decon->res.hw_te_off = pinctrl_lookup_state(decon->res.pinctrl, "hw_te_off");
	if (IS_ERR(decon->res.hw_te_off)) {
		decon_err("failed to get hw_te_off pin state\n");
		ret = PTR_ERR(decon->res.hw_te_off);
		decon->res.hw_te_off = NULL;
		goto err;
	}

err:
	return ret;
}

static irqreturn_t decon_ext_irq_handler(int irq, void *dev_id)
{
	struct decon_device *decon = dev_id;
	struct decon_mode_info psr;
	ktime_t timestamp = ktime_get();

	decon_systrace(decon, 'C', "decon_te_signal", 1);
	DPU_EVENT_LOG(DPU_EVT_TE_INTERRUPT, &decon->sd, timestamp);

	spin_lock(&decon->slock);

	if (decon->dt.trig_mode == DECON_SW_TRIG) {
		decon_to_psr_info(decon, &psr);
		decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_ENABLE);
	}

	if (decon->hiber.enabled && decon->state == DECON_STATE_ON &&
			decon->dt.out_type == DECON_OUT_DSI) {
		if (decon_min_lock_cond(decon)) {
			if (list_empty(&decon->hiber.worker.work_list)) {
				atomic_inc(&decon->hiber.remaining_hiber);
				kthread_queue_work(&decon->hiber.worker, &decon->hiber.work);
			}
		}
	}

	decon_systrace(decon, 'C', "decon_te_signal", 0);
	decon->vsync.timestamp = timestamp;
	wake_up_interruptible_all(&decon->vsync.wait);

	spin_unlock(&decon->slock);
#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS)
	if (devfreq_change_task)
		wake_up_process(devfreq_change_task);
#endif

	return IRQ_HANDLED;
}

int decon_register_ext_irq(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct platform_device *pdev;
	int gpio = -EINVAL, gpio1 = -EINVAL;
	int ret = 0;

	pdev = container_of(dev, struct platform_device, dev);

	/* Get IRQ resource and register IRQ handler. */
	if (of_get_property(dev->of_node, "gpios", NULL) != NULL) {
		gpio = of_get_gpio(dev->of_node, 0);
		if (gpio < 0) {
			decon_err("failed to get proper gpio number\n");
			return -EINVAL;
		}

		gpio1 = of_get_gpio(dev->of_node, 1);
		if (gpio1 < 0)
			decon_info("This board doesn't support TE GPIO of 2nd LCD\n");
	} else {
		decon_err("failed to find gpio node from device tree\n");
		return -EINVAL;
	}

	decon->res.irq = gpio_to_irq(gpio);

	decon_info("%s: gpio(%d)\n", __func__, decon->res.irq);
	ret = devm_request_irq(dev, decon->res.irq, decon_ext_irq_handler,
			IRQF_TRIGGER_RISING, pdev->name, decon);

	decon->eint_status = 1;

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS)
	devfreq_change_task =
		kthread_create(decon_devfreq_change_task, NULL,
				"devfreq_change");
	if (IS_ERR(devfreq_change_task))
		return PTR_ERR(devfreq_change_task);
#endif

	return ret;
}

static ssize_t decon_show_vsync(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%llu\n",
			ktime_to_ns(decon->vsync.timestamp));
}
static DEVICE_ATTR(vsync, S_IRUGO, decon_show_vsync, NULL);

static int decon_vsync_thread(void *data)
{
	struct decon_device *decon = data;
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
	struct v4l2_event ev;
	int num_dsi = (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) ? 2 : 1;
	int i;
#endif
	struct timespec64 tv;

	while (!kthread_should_stop()) {
		ktime_t timestamp = decon->vsync.timestamp;
		int ret = wait_event_interruptible(decon->vsync.wait,
			(timestamp != decon->vsync.timestamp) &&
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
			(decon->vsync.panel_notify || decon->vsync.active));
#else
			decon->vsync.active);
#endif
		if (!ret) {
#if defined(CONFIG_EXYNOS_COMMON_PANEL)
			if (decon->vsync.panel_notify) {
#if 0
				ev.timestamp = ktime_to_timespec64(decon->vsync.timestamp);
#else
				tv = ktime_to_timespec64(decon->fsync.timestamp);
				ev.timestamp.tv_sec =  tv.tv_sec;
				ev.timestamp.tv_nsec =	tv.tv_nsec;
#endif
				ev.type = V4L2_EVENT_DECON_VSYNC;
				for (i = 0; i < num_dsi; i++) {
					v4l2_subdev_call(decon->out_sd[i], core, ioctl,
							DSIM_IOC_NOTIFY, (void *)&ev);
				}
			}

			if (decon->vsync.active)
#endif
				sysfs_notify(&decon->dev->kobj, NULL, "vsync");
		}
	}

	return 0;
}

int decon_create_vsync_thread(struct decon_device *decon)
{
	int ret = 0;
	char name[16];

	if (decon->dt.out_type != DECON_OUT_DSI) {
		decon_info("vsync thread is only needed for DSI path\n");
		return 0;
	}

	ret = device_create_file(decon->dev, &dev_attr_vsync);
	if (ret) {
		decon_err("failed to create vsync file\n");
		return ret;
	}

	sprintf(name, "decon%d-vsync", decon->id);
	decon->vsync.thread = kthread_run(decon_vsync_thread, decon, name);
	if (IS_ERR_OR_NULL(decon->vsync.thread)) {
		decon_err("failed to run vsync thread\n");
		decon->vsync.thread = NULL;
		ret = PTR_ERR(decon->vsync.thread);
		goto err;
	}

	return 0;

err:
	device_remove_file(decon->dev, &dev_attr_vsync);
	return ret;
}

void decon_get_edid(struct decon_device *decon, struct decon_edid_data *edid_data)
{
	struct edid edid;
	const u8 edid_header[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

	decon_dbg("%s: edid size = %d, header size = %d\n", __func__,
			sizeof(struct edid), sizeof(edid_header));
	memset(&edid, 0, sizeof(struct edid));
	memcpy(&edid, edid_header, sizeof(edid_header));

	/*
	 * If you want to manipulate EDID information, use member variables
	 * of edid structure in here.
	 *
	 * ex) edid.width_cm = xx; edid.height_cm = yy
	 */

	memcpy(edid_data->edid_data, &edid, EDID_BLOCK_SIZE);
	edid_data->size = EDID_BLOCK_SIZE;
}

void decon_destroy_vsync_thread(struct decon_device *decon)
{
	device_remove_file(decon->dev, &dev_attr_vsync);

	if (decon->vsync.thread)
		kthread_stop(decon->vsync.thread);
}

#if defined(CONFIG_EXYNOS_COMMON_PANEL)
static int decon_fsync_thread(void *data)
{
	struct decon_device *decon = data;
	struct v4l2_event ev;
	ktime_t timestamp;
	int num_dsi = (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) ? 2 : 1;
	int i, ret;
	struct timespec64 tv;

	while (!kthread_should_stop()) {
		timestamp = decon->fsync.timestamp;
		ret = wait_event_interruptible(decon->fsync.wait,
				(timestamp != decon->fsync.timestamp) &&
				decon->fsync.active);
		if (!ret) {
#if 0
			ev.timestamp = ktime_to_timespec64(decon->fsync.timestamp);
#else
			tv = ktime_to_timespec64(decon->fsync.timestamp);
			ev.timestamp.tv_sec =  tv.tv_sec;
			ev.timestamp.tv_nsec =  tv.tv_nsec;
#endif
			ev.type = V4L2_EVENT_DECON_FRAME_DONE;
			for (i = 0; i < num_dsi; i++) {
				v4l2_subdev_call(decon->out_sd[i], core, ioctl,
						DSIM_IOC_NOTIFY, (void *)&ev);
				decon_dbg("%s notify frame done event\n", __func__);
			}
		}
	}

	return 0;
}

int decon_create_fsync_thread(struct decon_device *decon)
{
	char name[16];

	if (decon->dt.out_type != DECON_OUT_DSI) {
		decon_info("frame sync thread is only needed for DSI path\n");
		return 0;
	}

	sprintf(name, "decon%d-fsync", decon->id);
	decon->fsync.thread = kthread_run(decon_fsync_thread, decon, name);
	if (IS_ERR_OR_NULL(decon->fsync.thread)) {
		decon_err("failed to run fsync thread\n");
		decon->fsync.thread = NULL;
		return PTR_ERR(decon->fsync.thread);
	}

	return 0;
}

void decon_destroy_fsync_thread(struct decon_device *decon)
{
	if (decon->fsync.thread)
		kthread_stop(decon->fsync.thread);
}

int decon_error_cb(struct decon_device *decon,
		struct disp_check_cb_info *info)
{
	int ret = 0;
	int retry = 0;
	int status = 0;
	enum decon_state prev_state;

	if (decon == NULL || info == NULL) {
		decon_warn("%s invalid param\n", __func__);
		return -EINVAL;
	}

	decon_info("%s +\n", __func__);
	decon_bypass_on(decon);
	mutex_lock(&decon->lock);
	prev_state = decon->state;
	if (decon->state == DECON_STATE_OFF) {
		decon_warn("%s decon-%d already off state\n",
				__func__, decon->id);
		decon_bypass_off(decon);
		mutex_unlock(&decon->lock);
		return 0;
	}

	decon_hiber_block_exit(decon);
	kthread_flush_worker(&decon->up.worker);
	usleep_range(16900, 17000);
	if (prev_state == DECON_STATE_DOZE_SUSPEND) {
		ret = _decon_enable(decon, DECON_STATE_DOZE);
		if (ret < 0) {
			decon_err("decon-%d failed to set DECON_STATE_DOZE (ret %d)\n",
					decon->id, ret);
		}
	}

	while (++retry <= DISP_ERROR_CB_RETRY_CNT) {
		decon_warn("%s try recovery(%d times)\n",
				__func__, retry);
		if (IS_DECON_DOZE_STATE(decon)) {
			ret = _decon_disable(decon, DECON_STATE_OFF);
			if (ret < 0) {
				decon_err("decon-%d failed to set DECON_STATE_OFF (ret %d)\n",
						decon->id, ret);
			}

			status = disp_check_status(info);
			if (IS_DISP_CHECK_STATUS_DISCONNECTED(status)) {
				decon_info("%s ub disconnected(status 0x%x)\n",
						__func__, status);
				break;
			}

			ret = _decon_enable(decon, DECON_STATE_DOZE);
			if (ret < 0) {
				decon_err("decon-%d failed to set DECON_STATE_DOZE (ret %d)\n",
						decon->id, ret);
			}
		} else {
			ret = _decon_disable(decon, DECON_STATE_OFF);
			if (ret < 0) {
				decon_err("decon-%d failed to set DECON_STATE_OFF (ret %d)\n",
						decon->id, ret);
			}

			status = disp_check_status(info);
			if (IS_DISP_CHECK_STATUS_DISCONNECTED(status)) {
				decon_info("%s ub disconnected(status 0x%x)\n",
						__func__, status);
				break;
			}

			ret = _decon_enable(decon, DECON_STATE_ON);
			if (ret < 0) {
				decon_err("decon-%d failed to set DECON_STATE_ON (ret %d)\n",
						decon->id, ret);
			}
		}

		/*
		 * disp_check_status() will check only not-ok status
		 * if not go through and try update last regs
		 */
		status = disp_check_status(info);
		if (IS_DISP_CHECK_STATUS_NOK(status)) {
			decon_err("%s failed to recovery subdev(status 0x%x)\n",
					__func__, status);
			continue;
		}

		DPU_FULL_RECT(&decon->last_regs.up_region, decon->lcd_info);
		decon->last_regs.need_update = true;
		ret = decon_update_last_regs(decon, &decon->last_regs);
		if (ret < 0) {
			decon_err("%s failed to update last image(ret %d)\n",
					__func__, ret);
			continue;
		}
		decon_info("%s recovery successfully(retry %d times)\n",
				__func__, retry);
		break;
	}

	if (retry > DISP_ERROR_CB_RETRY_CNT) {
		decon_err("DECON:ERR:%s:failed to recover(retry %d times)\n",
				__func__, DISP_ERROR_CB_RETRY_CNT);
		decon_dump(decon, true);
#ifdef CONFIG_LOGGING_BIGDATA_BUG
		log_decon_bigdata(decon);
#endif
		BUG();
	}

	if (prev_state == DECON_STATE_DOZE_SUSPEND) {
		/* sleep for image update & display on */
		msleep(34);
		ret = _decon_disable(decon, DECON_STATE_DOZE_SUSPEND);
		if (ret < 0) {
			decon_err("decon-%d failed to set DECON_STATE_DOZE_SUSPEND (ret %d)\n",
					decon->id, ret);
		}
	}

	decon_bypass_off(decon);
	decon_hiber_unblock(decon);
	mutex_unlock(&decon->lock);
	decon_info("%s -\n", __func__);

	return ret;
}

int decon_error_cb_by_fb_blank(struct decon_device *decon,
		struct disp_check_cb_info *info)
{
	int ret = 0;
	struct fb_info *fb_info = registered_fb[0];

	if (decon == NULL || info == NULL) {
		decon_warn("%s invalid param\n", __func__);
		return -EINVAL;
	}

	decon_info("%s +\n", __func__);

	/* FB LOCK */
	lock_fb_info(fb_info);

	if (decon->state == DECON_STATE_OFF) {
		unlock_fb_info(fb_info);
		decon_info("%s skip. because decon state off\n", __func__);
		goto exit;
	}

	/* FBINFO_MISC_USEREVENT deprecated */
	/* fbmem: pull fbcon_update_vcs() out of fb_set_var() */
//	fb_info->flags |= FBINFO_MISC_USEREVENT;
	fb_blank(fb_info, FB_BLANK_POWERDOWN);
	fb_blank(fb_info, FB_BLANK_UNBLANK);
//	fb_info->flags &= ~FBINFO_MISC_USEREVENT;

	DPU_FULL_RECT(&decon->last_regs.up_region, decon->lcd_info);
	decon->last_regs.need_update = true;
	ret = decon_update_last_regs(decon, &decon->last_regs);
	if (ret < 0) {
		decon_err("%s failed to update last image(ret %d)\n",
				__func__, ret);
	}

	/* FB UNLOCK */
	unlock_fb_info(fb_info);

exit:
	decon_info("%s -\n", __func__);

	return ret;
}

#endif

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
#define ESD_RECOVERY_RETRY_CNT	5
static int decon_handle_esd(struct decon_device *decon)
{
	struct dsim_device *dsim;
	int ret = 0;
	int retry = 0;
	int status = 0;

	decon_info("%s +\n", __func__);

	if (decon == NULL) {
		decon_warn("%s invalid param\n", __func__);
		return -EINVAL;
	}

	decon_bypass_on(decon);
	dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
	dsim->esd_recovering = true;

	while (++retry <= ESD_RECOVERY_RETRY_CNT) {
		decon_warn("%s try recovery(%d times)\n",
				__func__, retry);
		ret = decon_update_pwr_state(decon, DISP_PWR_OFF);
		if (ret < 0) {
			decon_err("%s decon-%d failed to set subdev OFF state\n",
					__func__, decon->id);
			continue;
		}

		ret = decon_update_pwr_state(decon, DISP_PWR_NORMAL);
		if (ret < 0) {
			decon_err("%s decon-%d failed to set subdev ON state\n",
					__func__, decon->id);
			continue;
		}

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION_TEST)
		status = DSIM_ESD_OK;
#else
		status = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_READ_STATE, NULL);
#endif
		if (status != DSIM_ESD_OK) {
			decon_err("%s failed to recover subdev(status %d)\n",
					__func__, status);
			continue;
		}

		DPU_FULL_RECT(&decon->last_regs.up_region, decon->lcd_info);
		decon->last_regs.need_update = true;
		ret = decon_update_last_regs(decon, &decon->last_regs);
		if (ret < 0) {
			decon_err("%s failed to update last image(ret %d)\n",
					__func__, ret);
			continue;
		}
		decon_info("%s recovery successfully(retry %d times)\n",
				__func__, retry);
		break;
	}

	if (retry > ESD_RECOVERY_RETRY_CNT) {
		decon_err("DECON:ERR:%s:failed to recover(retry %d times)\n",
				__func__, ESD_RECOVERY_RETRY_CNT);
		decon_dump(decon, true);
		if (decon->dt.out_type == DECON_OUT_DSI)
			v4l2_subdev_call(decon->out_sd[0], core, ioctl,
					DSIM_IOC_DUMP, NULL);
		BUG();
	}

	dsim->esd_recovering = false;
	decon_bypass_off(decon);
	decon_info("%s -\n", __func__);

	return ret;
}

static void decon_esd_process(int esd, struct decon_device *decon)
{
	int ret;

	switch (esd) {
	case DSIM_ESD_CHECK_ERROR:
		decon_err("%s, It is not ESD, \
			but DDI is abnormal state(%d)\n", __func__, esd);
		break;
	case DSIM_ESD_OK:
		decon_dbg("%s, DDI has normal state(%d)\n", __func__, esd);
		break;
	case DSIM_ESD_ERROR:
		decon_err("%s, ESD is detected(%d)\n", __func__, esd);
		ret = decon_handle_esd(decon);
		if (ret)
			decon_err("%s, failed to recover ESD\n", __func__);
		break;
	default:
		decon_err("%s, Not supported value(%d)\n", __func__, esd);
		break;
	}
}

static int decon_esd_thread(void *data)
{
	struct decon_device *decon = data;
	struct dsim_device *dsim = NULL;
	int esd = 0;

	while (!kthread_should_stop()) {
		/* Loop for ESD detection */
		if (decon->state == DECON_STATE_OFF) {
			/* go to sleep when decon is not ready */
			decon_info("%s, Sleep \n", __func__);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			set_current_state(TASK_RUNNING);
			continue;
		}

		decon_hiber_block_exit(decon);
		if (decon->state == DECON_STATE_ON) {
			mutex_lock(&decon->esd.lock);

			dsim = container_of(decon->out_sd[0],
					struct dsim_device, sd);

			decon_dbg("%s, Try to check ESD\n", __func__);

			esd = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_READ_STATE, NULL);
			decon_esd_process(esd, decon);

			mutex_unlock(&decon->esd.lock);
		}
		decon_hiber_unblock(decon);
		/* sleep ESD_SLEEP_TIME second when decon is not state on
		 * and after read DDI state
		 */
		decon_dbg("%s, Sleep %d second\n", __func__, ESD_SLEEP_TIME);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(ESD_SLEEP_TIME * HZ);
	}
	/* when if kthread_should_stop() return true */
	return 0;
}

int decon_create_esd_thread(struct decon_device *decon)
{
	int ret = 0;
	char name[16];

	if (decon->dt.out_type != DECON_OUT_DSI) {
		decon_info("esd thread is only needed for DSI path\n");
		return 0;
	}

	sprintf(name, "decon%d-esd", decon->id);
	decon->esd.thread = kthread_run(decon_esd_thread, decon, name);
	if (IS_ERR_OR_NULL(decon->esd.thread)) {
		decon_err("failed to run esd thread\n");
		decon->esd.thread = NULL;
		ret = PTR_ERR(decon->esd.thread);
	}

	return ret;
}

void decon_destroy_esd_thread(struct decon_device *decon)
{
	if (decon->esd.thread)
		kthread_stop(decon->esd.thread);
}
#endif /* CONFIG_EXYNOS_READ_ESD_SOLUTION */

/*
 * Variable Descriptions
 *   dsc_en : comp_mode (0=No Comp, 1=DSC, 2=MIC, 3=LEGO)
 *   dsc_width : min_window_update_width depending on compression mode
 *   dsc_height : min_window_update_height depending on compression mode
 */
static ssize_t decon_show_psr_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct exynos_panel_info *lcd_info = decon->lcd_info;
	int i;
	char *p = buf;
	struct lcd_mres_info *mres_info = &lcd_info->mres;
	int len;

	len = sprintf(p, "%d\n", decon->dt.psr_mode);
	len += sprintf(p + len, "%d\n", mres_info->number);
	for (i = 0; i < mres_info->number; i++) {
		if (mres_info->res_info[i].dsc_en)
			len += sprintf(p + len, "%d\n%d\n%d\n%d\n%d\n",
				mres_info->res_info[i].width,
				mres_info->res_info[i].height,
				mres_info->res_info[i].dsc_width,
				mres_info->res_info[i].dsc_height,
				mres_info->res_info[i].dsc_en);
		else
			len += sprintf(p + len, "%d\n%d\n%d\n%d\n%d\n",
				mres_info->res_info[i].width,
				mres_info->res_info[i].height,
				MIN_WIN_BLOCK_WIDTH,
				MIN_WIN_BLOCK_HEIGHT,
				mres_info->res_info[i].dsc_en);
	}
	return len;
}
static DEVICE_ATTR(psr_info, S_IRUGO, decon_show_psr_info, NULL);

int decon_create_psr_info(struct decon_device *decon)
{
	int ret = 0;

	/* It's enought to make a file for PSR information */
	if (decon->id != 0)
		return 0;

	ret = device_create_file(decon->dev, &dev_attr_psr_info);
	if (ret) {
		decon_err("failed to create psr info file\n");
		return ret;
	}

	return ret;
}

void decon_destroy_psr_info(struct decon_device *decon)
{
	device_remove_file(decon->dev, &dev_attr_psr_info);
}

static ssize_t decon_show_hiber_exit(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	char *p = buf;
	int len = 0;

	decon_dbg("%s +\n", __func__);

	if (!decon->hiber.early_wakeup_enable)
		return len;

	decon->hiber.early_wakeup_cnt++;
	kthread_queue_work(&decon->hiber.exit_worker, &decon->hiber.exit_work);
	len = sprintf(p, "%d\n", decon->hiber.early_wakeup_cnt);

	decon_dbg("%s -\n", __func__);
	return len;
}
static DEVICE_ATTR(hiber_exit, S_IRUGO, decon_show_hiber_exit, NULL);

/* Framebuffer interface related callback functions */
static u32 fb_visual(u32 bits_per_pixel, unsigned short palette_sz)
{
	switch (bits_per_pixel) {
	case 32:
	case 24:
	case 16:
	case 12:
		return FB_VISUAL_TRUECOLOR;
	case 8:
		if (palette_sz >= 256)
			return FB_VISUAL_PSEUDOCOLOR;
		else
			return FB_VISUAL_TRUECOLOR;
	case 1:
		return FB_VISUAL_MONO01;
	default:
		return FB_VISUAL_PSEUDOCOLOR;
	}
}

static inline u32 fb_linelength(u32 xres_virtual, u32 bits_per_pixel)
{
	return (xres_virtual * bits_per_pixel) / 8;
}

static u16 fb_panstep(u32 res, u32 res_virtual)
{
	return res_virtual > res ? 1 : 0;
}

int decon_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct decon_window_regs win_regs;
	int win_no = win->idx;

	if ((!IS_DECON_HIBER_STATE(decon) && IS_DECON_OFF_STATE(decon)) ||
			decon->state == DECON_STATE_INIT)
		return 0;

	memset(&win_regs, 0, sizeof(struct decon_window_regs));

	decon_hiber_block_exit(decon);

	decon_reg_wait_update_done_timeout(decon->id, SHADOW_UPDATE_TIMEOUT);
	info->fix.visual = fb_visual(var->bits_per_pixel, 0);

	info->fix.line_length = fb_linelength(var->xres_virtual,
			var->bits_per_pixel);
	info->fix.xpanstep = fb_panstep(var->xres, var->xres_virtual);
	info->fix.ypanstep = fb_panstep(var->yres, var->yres_virtual);

	win_regs.wincon |= wincon(win_no);
	win_regs.start_pos = win_start_pos(0, 0);
	win_regs.end_pos = win_end_pos(0, 0, var->xres, var->yres);
	win_regs.pixel_count = (var->xres * var->yres);
	win_regs.whole_w = var->xoffset + var->xres;
	win_regs.whole_h = var->yoffset + var->yres;
	win_regs.offset_x = var->xoffset;
	win_regs.offset_y = var->yoffset;
	win_regs.ch = decon->dt.dft_ch;
	decon_reg_set_window_control(decon->id, win_no, &win_regs, false);

	decon_hiber_unblock(decon);
	return 0;
}
EXPORT_SYMBOL(decon_set_par);

int decon_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;

	var->xres_virtual = max(var->xres_virtual, var->xres);
	var->yres_virtual = max(var->yres_virtual, var->yres);

	if (!decon_validate_x_alignment(decon, 0, var->xres,
			var->bits_per_pixel))
		return -EINVAL;

	/* always ensure these are zero, for drop through cases below */
	var->transp.offset = 0;
	var->transp.length = 0;

	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		var->red.offset		= 4;
		var->green.offset	= 2;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 3;
		var->blue.length	= 2;
		var->transp.offset	= 7;
		var->transp.length	= 1;
		break;

	case 19:
		/* 666 with one bit alpha/transparency */
		var->transp.offset	= 18;
		var->transp.length	= 1;
	case 18:
		var->bits_per_pixel	= 32;

		/* 666 format */
		var->red.offset		= 12;
		var->green.offset	= 6;
		var->blue.offset	= 0;
		var->red.length		= 6;
		var->green.length	= 6;
		var->blue.length	= 6;
		break;

	case 16:
		/* 16 bpp, 565 format */
		var->red.offset		= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		break;

	case 32:
	case 28:
	case 25:
		var->transp.length	= var->bits_per_pixel - 24;
		var->transp.offset	= 24;
		/* drop through */
	case 24:
		/* our 24bpp is unpacked, so 32bpp */
		var->bits_per_pixel	= 32;
		var->red.offset		= 16;
		var->red.length		= 8;
		var->green.offset	= 8;
		var->green.length	= 8;
		var->blue.offset	= 0;
		var->blue.length	= 8;
		break;

	default:
		decon_err("invalid bpp %d\n", var->bits_per_pixel);
		return -EINVAL;
	}

	decon_dbg("xres:%d, yres:%d, v_xres:%d, v_yres:%d, bpp:%d\n",
			var->xres, var->yres, var->xres_virtual,
			var->yres_virtual, var->bits_per_pixel);

	return 0;
}

static inline unsigned int chan_to_field(unsigned int chan,
					 struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	unsigned int val;

	printk("@@@@ %s\n", __func__);
	decon_dbg("%s: win %d: %d => rgb=%d/%d/%d\n", __func__, win->idx,
			regno, red, green, blue);

	if (IS_DECON_OFF_STATE(decon))
		return 0;

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseudo-palette */

		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val  = chan_to_field(red,   &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue,  &info->var.blue);

			pal[regno] = val;
		}
		break;
	default:
		return 1;	/* unknown type */
	}

	return 0;
}
EXPORT_SYMBOL(decon_setcolreg);

int decon_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct decon_win *win = info->par;
	struct decon_device *decon = win->decon;
	struct v4l2_subdev *sd = NULL;
	struct decon_win_config config;
	int ret = 0;
	struct decon_mode_info psr;
	int dpp_id = decon->dt.dft_ch;
	struct dpp_config dpp_config;
	unsigned long aclk_khz;

	if (decon->dt.out_type != DECON_OUT_DSI) {
		decon_warn("%s: decon%d unspported on out_type(%d)\n",
				__func__, decon->id, decon->dt.out_type);
		return 0;
	}

	if ((!IS_DECON_HIBER_STATE(decon) && IS_DECON_OFF_STATE(decon)) ||
			decon->state == DECON_STATE_INIT) {
		decon_warn("%s: decon%d state(%d), UNBLANK missed\n",
				__func__, decon->id, decon->state);
		return 0;
	}

	decon_dbg("%s: [%d %d %d %d %d %d]\n", __func__,
			var->xoffset, var->yoffset,
			var->xres, var->yres,
			var->xres_virtual, var->yres_virtual);

	memset(&config, 0, sizeof(struct decon_win_config));
	switch (var->bits_per_pixel) {
	case 16:
		config.format = DECON_PIXEL_FORMAT_RGB_565;
		break;
	case 24:
	case 32:
		config.format = DECON_PIXEL_FORMAT_RGBA_8888; /* DECON_PIXEL_FORMAT_BGRA_8888 */
		break;
	default:
		decon_err("%s: Not supported bpp %d\n", __func__,
				var->bits_per_pixel);
		return -EINVAL;
	}

	config.dpp_parm.addr[0] = info->fix.smem_start;
	config.src.x =  var->xoffset;
	config.src.y =  var->yoffset;
	config.src.w = var->xres;
	config.src.h = var->yres;
	config.src.f_w = var->xres_virtual;
	config.src.f_h = var->yres_virtual;
	config.dst.w = config.src.w;
	config.dst.h = config.src.h;
	config.dst.f_w = decon->lcd_info->xres;
	config.dst.f_h = decon->lcd_info->yres;
	config.state = DECON_WIN_STATE_BUFFER;

	if (decon_check_limitation(decon, decon->dt.dft_win, &config) < 0)
		return -EINVAL;

	#if defined(CONFIG_EXYNOS_BTS)
	    decon->bts.ops->bts_acquire_bw(decon);
	#endif

	decon_hiber_block_exit(decon);

	decon_to_psr_info(decon, &psr);

	/*
	 * info->var is old parameters and var is new requested parameters.
	 * var must be copied to info->var before decon_set_par function
	 * is called.
	 *
	 * If not, old parameters are set to window configuration
	 * and new parameters are set to DMA and DPP configuration.
	 */
	memcpy(&info->var, var, sizeof(struct fb_var_screeninfo));
	decon_set_par(info);

	set_bit(dpp_id, &decon->cur_using_dpp);
	set_bit(dpp_id, &decon->prev_used_dpp);
	sd = decon->dpp_sd[dpp_id];

	aclk_khz = v4l2_subdev_call(decon->out_sd[0], core, ioctl,
			EXYNOS_DPU_GET_ACLK, NULL) / 1000U;

	memcpy(&dpp_config.config, &config, sizeof(struct decon_win_config));
	dpp_config.rcv_num = aclk_khz;

	if (v4l2_subdev_call(sd, core, ioctl, DPP_WIN_CONFIG, &dpp_config)) {
		decon_err("%s: Failed to config DPP-%d\n", __func__, win->dpp_id);
		decon_reg_win_enable_and_update(decon->id, decon->dt.dft_win, false);
		clear_bit(dpp_id, &decon->cur_using_dpp);
		set_bit(dpp_id, &decon->dpp_err_stat);
		goto err;
	}

	decon_reg_all_win_shadow_update_req(decon->id);

	decon_reg_start(decon->id, &psr);
err:
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

	if (decon_reg_wait_update_done_and_mask(decon->id, &psr, SHADOW_UPDATE_TIMEOUT)
			< 0)
		decon_err("%s: wait_for_update_timeout\n", __func__);

	decon_hiber_unblock(decon);
	return ret;
}
EXPORT_SYMBOL(decon_pan_display);

int decon_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	int ret;
	struct decon_win *win = info->par;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
#if defined(CONFIG_FB_TEST)
	ret = dma_buf_mmap(win->fb_buf_data.dma_buf, vma, 0);
#else
	ret = dma_buf_mmap(win->dma_buf_data[0].dma_buf, vma, 0);
#endif

	return ret;
}
EXPORT_SYMBOL(decon_mmap);

int decon_exit_hiber(struct decon_device *decon)
{
	int ret = 0;
	struct decon_param p;
	struct decon_mode_info psr;
	enum decon_state prev_state = decon->state;

	DPU_EVENT_START();

	if (!decon->hiber.enabled)
		return 0;

	decon_hiber_block(decon);

	if (atomic_read(&decon->hiber.remaining_hiber))
		kthread_flush_worker(&decon->hiber.worker);

	mutex_lock(&decon->hiber.lock);

	if (decon->state != DECON_STATE_HIBER)
		goto err;

	decon_dbg("enable decon-%d\n", decon->id);

	ret = decon_set_out_sd_state(decon, DECON_STATE_ON);
	if (ret < 0) {
		decon_err("%s decon-%d failed to set subdev EXIT_ULPS state\n",
				__func__, decon->id);
	}

	decon_to_init_param(decon, &p);
	decon_reg_init(decon->id, decon->dt.out_idx[0], &p);

	/*
	 * After hibernation exit, If panel is partial size, DECON and DSIM
	 * are also set as same partial size.
	 */
	if (!is_full(&decon->win_up.prev_up_region, decon->lcd_info))
		dpu_set_win_update_partial_size(decon, &decon->win_up.prev_up_region);

	if (!decon->id && !decon->eint_status) {
		struct irq_desc *desc = irq_to_desc(decon->res.irq);
		/* Pending IRQ clear */
		if ((!IS_ERR_OR_NULL(desc)) && (desc->irq_data.chip->irq_ack)) {
			desc->irq_data.chip->irq_ack(&desc->irq_data);
			desc->istate &= ~IRQS_PENDING;
		}
		enable_irq(decon->res.irq);
		decon->eint_status = 1;
	}

	decon->state = DECON_STATE_ON;
	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 1);

	decon_hiber_trig_reset(decon);

	decon_dbg("decon-%d %s - (state:%d -> %d)\n",
			decon->id, __func__, prev_state, decon->state);
	decon->hiber.exit_cnt++;
	DPU_EVENT_LOG(DPU_EVT_EXIT_HIBER, &decon->sd, start);
	decon_hiber_finish(decon);

err:
	decon_hiber_unblock(decon);
	mutex_unlock(&decon->hiber.lock);

	return ret;
}

int decon_enter_hiber(struct decon_device *decon)
{
	int ret = 0;
	struct decon_mode_info psr;
	enum decon_state prev_state = decon->state;

	DPU_EVENT_START();

	if (!decon->hiber.enabled)
		return 0;

	mutex_lock(&decon->hiber.lock);

	if (decon_is_enter_shutdown(decon))
		goto err2;

	if (decon_is_hiber_blocked(decon))
		goto err2;

	decon_hiber_block(decon);
	if (decon->state != DECON_STATE_ON)
		goto err;

	decon_dbg("decon-%d %s +\n", decon->id, __func__);
	decon_hiber_trig_reset(decon);

	if (atomic_read(&decon->up.remaining_frame))
		kthread_flush_worker(&decon->up.worker);

	decon_to_psr_info(decon, &psr);
	decon_reg_set_int(decon->id, &psr, 0);

	if (!decon->id && (decon->vsync.irq_refcount <= 0) &&
			decon->eint_status) {
		disable_irq(decon->res.irq);
		decon->eint_status = 0;
	}

	ret = decon_reg_stop(decon->id, decon->dt.out_idx[0], &psr, true,
			decon->lcd_info->fps);
	if (ret < 0)
		decon_dump(decon, true);

	/* DMA protection disable must be happen on dpp domain is alive */
	if (decon->dt.out_type != DECON_OUT_WB) {
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
		decon_set_protected_content(decon, NULL);
#endif
		decon->cur_using_dpp = 0;
		decon_dpp_stop(decon, false);
	}

#if IS_ENABLED(CONFIG_EXYNOS_BTS)
	decon->bts.ops->bts_release_bw(decon);
#endif

	ret = decon_set_out_sd_state(decon, DECON_STATE_HIBER);
	if (ret < 0)
		decon_err("%s decon-%d failed to set subdev ENTER_ULPS state\n",
				__func__, decon->id);

	decon->state = DECON_STATE_HIBER;

	decon->hiber.enter_cnt++;
	DPU_EVENT_LOG(DPU_EVT_ENTER_HIBER, &decon->sd, start);
	decon_hiber_start(decon);

err:
	decon_hiber_unblock(decon);
err2:
	mutex_unlock(&decon->hiber.lock);

	decon_dbg("decon-%d %s - (state:%d -> %d)\n",
			decon->id, __func__, prev_state, decon->state);

	return ret;
}

int decon_hiber_block_exit(struct decon_device *decon)
{
	int ret = 0;

	if (!decon || !decon->hiber.enabled)
		return 0;

	decon_hiber_block(decon);
	ret = decon_exit_hiber(decon);

	return ret;
}

static void decon_hiber_handler(struct kthread_work *work)
{
	struct decon_hiber *hiber =
		container_of(work, struct decon_hiber, work);
	struct decon_device *decon =
		container_of(hiber, struct decon_device, hiber);

	if (!decon || !decon->hiber.enabled) {
		atomic_dec(&decon->hiber.remaining_hiber);
		return;
	}

	if (decon_hiber_enter_cond(decon))
		decon_enter_hiber(decon);

	atomic_dec(&decon->hiber.remaining_hiber);
}

static void decon_exit_hiber_handler(struct kthread_work *work)
{
	struct decon_hiber *hiber =
		container_of(work, struct decon_hiber, exit_work);
	struct decon_device *decon =
		container_of(hiber, struct decon_device, hiber);

	if (!decon || !decon->hiber.enabled)
		return;

	decon_dbg("%s +\n", __func__);
	decon_exit_hiber(decon);
	decon_dbg("%s -\n", __func__);
}

int decon_register_hiber_work(struct decon_device *decon)
{
	struct sched_param param;
	int ret = 0;

	decon->hiber.enabled = false;
	if (!IS_ENABLED(CONFIG_EXYNOS_HIBERNATION)) {
		decon_info("display doesn't support hibernation mode\n");
		return 0;
	}

	mutex_init(&decon->hiber.lock);

	atomic_set(&decon->hiber.trig_cnt, 0);
	atomic_set(&decon->hiber.block_cnt, 0);

	kthread_init_worker(&decon->hiber.worker);
	decon->hiber.thread = kthread_run(kthread_worker_fn,
			&decon->hiber.worker, "decon_hiber");
	if (IS_ERR(decon->hiber.thread)) {
		decon->hiber.thread = NULL;
		decon_err("failed to run hibernation thread\n");
		return PTR_ERR(decon->hiber.thread);
	}
	param.sched_priority = 20;
	sched_setscheduler_nocheck(decon->hiber.thread, SCHED_FIFO, &param);
	kthread_init_work(&decon->hiber.work, decon_hiber_handler);

	decon->hiber.hiber_enter_cnt = DECON_ENTER_HIBER_CNT;
	decon->hiber.enabled = true;
	decon_info("display supports hibernation mode\n");

	/* register asynchronous hibernation early wakeup feature */
	decon->hiber.early_wakeup_enable = false;
	if (!IS_ENABLED(CONFIG_EXYNOS_HIBERNATION_EARLY_WAKEUP) || (decon->id)) {
		decon_info("hibernation early wakeup is disabled\n");
		return 0;
	}

	/* initialize exit hibernation thread */
	kthread_init_worker(&decon->hiber.exit_worker);
	decon->hiber.exit_thread = kthread_run(kthread_worker_fn,
			&decon->hiber.exit_worker, "decon_exit_hiber");
	if (IS_ERR(decon->hiber.exit_thread)) {
		decon->hiber.exit_thread = NULL;
		decon_err("failed to run exit hibernation thread\n");
		return PTR_ERR(decon->hiber.exit_thread);
	}
	param.sched_priority = 20;
	sched_setscheduler_nocheck(decon->hiber.exit_thread, SCHED_FIFO, &param);
	kthread_init_work(&decon->hiber.exit_work, decon_exit_hiber_handler);
	decon->hiber.early_wakeup_cnt = 0;

	ret = device_create_file(decon->dev, &dev_attr_hiber_exit);
	if (ret) {
		decon_err("failed to create hiber exit file\n");
		return ret;
	}
	decon->hiber.early_wakeup_enable = true;
	decon_info("hibernation early wakeup is enabled\n");

	return 0;
}

void decon_init_low_persistence_mode(struct decon_device *decon)
{
	decon->low_persistence = false;

	if (!IS_ENABLED(CONFIG_EXYNOS_LOW_PERSISTENCE)) {
		decon_info("display doesn't support low persistence mode\n");
		return;
	}

	decon->low_persistence = true;
	decon_info("display supports low persistence mode\n");
}

void dpu_init_freq_hop(struct decon_device *decon)
{
#if !defined(CONFIG_SOC_EXYNOS9820_EVT0)
	if (IS_ENABLED(CONFIG_EXYNOS_FREQ_HOP)) {
		decon->freq_hop.enabled = true;
		decon->freq_hop.target_m = decon->lcd_info->dphy_pms.m;
		decon->freq_hop.request_m = decon->lcd_info->dphy_pms.m;
		decon->freq_hop.target_k = decon->lcd_info->dphy_pms.k;
		decon->freq_hop.request_k = decon->lcd_info->dphy_pms.k;
	} else {
		decon->freq_hop.enabled = false;
	}
#endif
}

void dpu_update_freq_hop(struct decon_device *decon)
{
#if !defined(CONFIG_SOC_EXYNOS9820_EVT0)
	if (!decon->freq_hop.enabled)
		return;

	decon->freq_hop.target_m = decon->freq_hop.request_m;
	decon->freq_hop.target_k = decon->freq_hop.request_k;
#endif
}

void dpu_set_freq_hop(struct decon_device *decon, bool en)
{
#if !defined(CONFIG_SOC_EXYNOS9820_EVT0)
	struct stdphy_pms *pms;
	struct decon_freq_hop freq_hop;
	u32 target_m = decon->freq_hop.target_m;
	u32 target_k = decon->freq_hop.target_k;

	if ((decon->dt.out_type != DECON_OUT_DSI) || (!decon->freq_hop.enabled))
		return;

	pms = &decon->lcd_info->dphy_pms;
	if ((pms->m != target_m) || (pms->k != target_k)) {
		if (en) {
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			/* wakeup PLL if sleeping... */
			decon_reg_set_pll_wakeup(decon->id, true);
			decon_reg_set_pll_sleep(decon->id, false);
#endif

			v4l2_subdev_call(decon->out_sd[0], core, ioctl,
					DSIM_IOC_SET_FREQ_HOP, &decon->freq_hop);

		} else {
			pms->m = decon->freq_hop.target_m;
			pms->k = decon->freq_hop.target_k;
			freq_hop.target_m = 0;
			decon_info("%s: pmsk[%d %d %d %d]\n", __func__,
					pms->p, pms->m, pms->s, pms->k);
			v4l2_subdev_call(decon->out_sd[0], core, ioctl,
					DSIM_IOC_SET_FREQ_HOP, &freq_hop);
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			decon_reg_set_pll_sleep(decon->id, true);
#endif
		}
	}
#endif
}

int dpu_hw_recovery_process(struct decon_device *decon)
{
	int err = 0;
	u32 dsi_idx = decon->dt.out_idx[0];
	u32 fps = decon->lcd_info->fps;
	struct decon_mode_info psr;

	decon_info("decon%d %s +\n", decon->id, __func__);

	if (decon->dt.out_type != DECON_OUT_DSI) {
		decon_info("decon%d is not DSI path..exit\n", decon->id);
		err = -EINVAL;
		goto out;
	}

	/* dsim & dphy reset and recover */
	v4l2_subdev_call(decon->out_sd[0], core, ioctl,
				DSIM_IOC_RECOVERY_PROC, NULL);

	/* decon instant off */
	decon_to_psr_info(decon, &psr);
	err = decon_reg_stop_inst(decon->id, dsi_idx, &psr, fps);
	if (err < 0)
		decon_err("%s, failed to instant_stop\n", __func__);

out:
	decon_info("decon%d %s -\n", decon->id, __func__);

	return err;
}
