/*
 * isp.c
 *
 * Marvell DxO ISP - Top level module
 *	Based on omap3isp
 *
 * Copyright:  (C) Copyright 2011 Marvell International Ltd.
 *              Henry Zhao <xzhao10@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <asm/cacheflush.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <linux/pm_qos.h>

#include "isp.h"
#include "ispreg.h"
#include "ispdma.h"
#include "ispccic.h"

#define BIT2BYTE			8
#define MIPI_DDR_TO_BIT_CLK		2

#define MVISP_DUMMY_BUF_SIZE	(1920*1080*2)
#define ISP_STOP_TIMEOUT	msecs_to_jiffies(1000)

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
static struct pm_qos_request mvisp_qos_idle;
#endif

static irqreturn_t mvisp_ipc_isr(int irq, void *_isp)
{
	struct mvisp_device *isp = _isp;
	union agent_id id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_DXO_CORE,
	};
	struct isp_ispdma_device *ispdma = container_of(
					get_agent(isp->manager.hw, id),
					struct isp_ispdma_device, agent.hw);

	mod_setbit(ispdma, ISP_IRQSTAT, ~0);
	mv_ispdma_ipc_isr_handler(ispdma);

	return IRQ_HANDLED;
}

static int mvisp_reset(struct mvisp_device *isp)
{
	return 0;
}

static void mvisp_save_context(struct mvisp_device *isp,
	struct isp_reg_context *reg_list)
{
}

static void mvisp_restore_context(struct mvisp_device *isp,
	struct isp_reg_context *reg_list)
{
}

unsigned long mvisp_get_avail_clk_rate(unsigned long *avail_clk_rate,
		unsigned int num, unsigned long rate)
{
	int i;

	if (!avail_clk_rate && !rate)
		return 0;

	for (i = 0; i < num; i++) {
		if (rate <= avail_clk_rate[i])
			break;
	}

	if (i < num)
		return avail_clk_rate[i];

	return 0;
}

static void mvisp_resume_modules(struct mvisp_device *isp)
{
	mvisp_resume_module_pipeline(&isp->mvisp_ispdma.subdev.entity);
	mvisp_resume_module_pipeline(&isp->mvisp_ccic1.subdev.entity);
}
#endif

static int mvisp_reset(struct mvisp_device *isp)
{
	return 0;
}

static void
mvisp_save_context(struct mvisp_device *isp,
	struct isp_reg_context *reg_list)
{
}

static void
mvisp_restore_context(struct mvisp_device *isp,
	struct isp_reg_context *reg_list)
{
}
static int plat_get_slot(union agent_id id);

unsigned long mvisp_get_avail_clk_rate(unsigned long *avail_clk_rate,
		unsigned int num, unsigned long rate)
{
	int i;

	if (!avail_clk_rate && !rate)
		return 0;

	for (i = 0; i < num; i++) {
		if (rate <= avail_clk_rate[i])
			break;
	}

	if (i < num)
		return avail_clk_rate[i];

	printk(KERN_ERR "ERROR:the requested clock rate %luMHz\n", rate);
	return 0;
}

int mvisp_set_fclk_dphy(struct v4l2_subdev *sd,
		struct v4l2_sensor_csi_dphy *sensor_dphy)
{
	unsigned long min_clk_rate;
	struct mvisp_device *isp =
		(struct mvisp_device *) v4l2_get_subdev_hostdata(sd);

	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic = container_of(
					get_agent(isp->manager.hw, ccic_id),
					struct ccic_device, agent.hw);
	union agent_id dphy_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id	= AGENT_CCIC_DPHY,
	};
	struct ccic_dphy_t *dphy = container_of(
					get_agent(isp->manager.hw, dphy_id),
					struct ccic_dphy_t, agent.hw);

	if (!isp->sensor_connected || !sensor_dphy)
		return 0;

	dphy->dphy_set = sensor_dphy->sensor_set_dphy;
	memcpy(&dphy->dphy_desc, &sensor_dphy->dphy_desc,
			 sizeof(struct v4l2_sensor_csi_dphy));

	min_clk_rate = dphy->dphy_desc.clk_freq *
		 dphy->dphy_desc.nr_lane * MIPI_DDR_TO_BIT_CLK / BIT2BYTE / MHZ;

	min_clk_rate = mvisp_get_avail_clk_rate(ccic->pdata.avail_fclk_rate,
			ccic->pdata.avail_fclk_rate_num, min_clk_rate);
	if (!min_clk_rate)
		return -EINVAL;
	ccic->pdata.fclk_mhz = min_clk_rate;

/*isp func clock >= ccic func clok*/
	min_clk_rate = mvisp_get_avail_clk_rate(isp->pdata->isp_clk_rate,
			isp->pdata->isp_clk_rate_num, ccic->pdata.fclk_mhz);
	if (!min_clk_rate)
		return -EINVAL;
	isp->min_fclk_mhz = min_clk_rate;

	return 0;
}


	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic = container_of(
					get_agent(isp->manager.hw, ccic_id),
					struct ccic_device, agent.hw);
	union agent_id dphy_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id	= AGENT_CCIC_DPHY,
	};
	struct ccic_dphy_t *dphy = container_of(
					get_agent(isp->manager.hw, dphy_id),
					struct ccic_dphy_t, agent.hw);

	if (!isp->sensor_connected || !sensor_dphy)
		return 0;

	dphy->dphy_set = sensor_dphy->sensor_set_dphy;
	memcpy(&dphy->dphy_desc, &sensor_dphy->dphy_desc,
			 sizeof(struct v4l2_sensor_csi_dphy));

	min_clk_rate = dphy->dphy_desc.clk_freq *
		 dphy->dphy_desc.nr_lane * MIPI_DDR_TO_BIT_CLK / BIT2BYTE / MHZ;

	min_clk_rate = mvisp_get_avail_clk_rate(ccic->pdata.avail_fclk_rate,
			ccic->pdata.avail_fclk_rate_num, min_clk_rate);
	if (!min_clk_rate)
		return -EINVAL;
	ccic->pdata.fclk_mhz = min_clk_rate;

/*isp func clock >= ccic func clok*/
	min_clk_rate = mvisp_get_avail_clk_rate(isp->pdata->isp_clk_rate,
			isp->pdata->isp_clk_rate_num, ccic->pdata.fclk_mhz);
	if (!min_clk_rate)
		return -EINVAL;
	isp->min_fclk_mhz = min_clk_rate;

>>>> ORIGINAL //JBP_DEV/Platform/System/Maple/JBP98x/kernel/kernel/drivers/media/video/mvisp/isp.c#1
	ccic_set_mclk_rate(ccic, 0);
	hw_tune_power(&dphy->agent.hw, 0);
	hw_tune_power(&ccic->agent.hw, 0);
	hw_tune_power(&ispdma->agent.hw, 0);
==== THEIRS //JBP_DEV/Platform/System/Maple/JBP98x/kernel/kernel/drivers/media/video/mvisp/isp.c#2
	return 0;
==== YOURS //GWANGHUILEE_CHN_GSM_JELLYBEANPLUS_DELOS-3G-CMCC-LINUX/android/kernel/drivers/media/video/mvisp/isp.c
	ccic_set_mclk_rate(ccic, 0);
	hw_tune_power(&dphy->agent.hw, 0);
	hw_tune_power(&ccic->agent.hw, 0);
	hw_tune_power(&ispdma->agent.hw, 0);

<<<<
}

static void mvisp_put_clocks(struct mvisp_device *isp)
{
}

static int mvisp_get_clocks(struct mvisp_device *isp,
		struct mvisp_platform_data *pdata)
{
	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic = container_of(
					get_agent(isp->manager.hw, ccic_id),
					struct ccic_device, agent.hw);

	/* FIXME: actually, we should use get"CCIC_FUNC", and get_rate */
	if (isp->cpu_type == MV_MMP3) {
		ccic->pdata.fclk_mhz = 400;
>>>> ORIGINAL //JBP_DEV/Platform/System/Maple/JBP98x/kernel/kernel/drivers/media/video/mvisp/isp.c#1
	else
		ccic->pdata.fclk_mhz = 312;
==== THEIRS //JBP_DEV/Platform/System/Maple/JBP98x/kernel/kernel/drivers/media/video/mvisp/isp.c#2
		ccic->pdata.mclk_mhz = 26;
	} else {
/* The new sensor dphy setting will not take effect
 * immediately. Since the ccic function will affect the
 * mclk, which we can not change the related register on
 * the fly. So the setting is applied next time. The first
 * time will set the clock to 416M.
 * */
		ccic->pdata.fclk_mhz = 416;
		ccic->pdata.mclk_mhz = 24;

	ccic->pdata.avail_fclk_rate_num  = pdata->ccic_clk_rate_num;
	memcpy(ccic->pdata.avail_fclk_rate, pdata->ccic_clk_rate,
			sizeof(unsigned long) * pdata->ccic_clk_rate_num);
	}
==== YOURS //GWANGHUILEE_CHN_GSM_JELLYBEANPLUS_DELOS-3G-CMCC-LINUX/android/kernel/drivers/media/video/mvisp/isp.c
	else
/* The new sensor dphy setting will not take effect
 * immediately. Since the ccic function will affect the
 * mclk, which we can not change the related register on
 * the fly. So the setting is applied next time. The first
 * time will set the clock to 416M.
 * */
		ccic->pdata.fclk_mhz = 416;

	ccic->pdata.avail_fclk_rate_num  = pdata->ccic_clk_rate_num;
	memcpy(ccic->pdata.avail_fclk_rate, pdata->ccic_clk_rate,
			sizeof(unsigned long) * pdata->ccic_clk_rate_num);

<<<<
	return 0;
}


static void mvisp_put_sensor_resource(struct mvisp_device *isp)
{
}

struct mvisp_device *mvisp_get(struct mvisp_device *isp)
{
	struct mvisp_device *__isp = isp;
	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic = container_of(
					get_agent(isp->manager.hw, ccic_id),
					struct ccic_device, agent.hw);
	union agent_id dphy_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id	= AGENT_CCIC_DPHY,
	};
	struct ccic_dphy_t *dphy = container_of(
					get_agent(isp->manager.hw, dphy_id),
					struct ccic_dphy_t, agent.hw);
	union agent_id dxo_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_DXO_CORE,
	};
	struct isp_ispdma_device *ispdma = container_of(
					get_agent(isp->manager.hw, dxo_id),
					struct isp_ispdma_device, agent.hw);
	int ret;

	if (isp == NULL)
		return NULL;

	mutex_lock(&isp->mvisp_mutex);
	if (isp->ref_count > 0)
		goto out;

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	pm_qos_update_request(&mvisp_qos_idle,
			PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
#endif
	if (!isp->pdata->init_pin || isp->pdata->init_pin(isp->manager.dev, 1))
		goto out;

	if (isp->isp_pwr_ctrl == NULL || isp->isp_pwr_ctrl(1) < 0) {
		__isp = NULL;
		goto out;
	}

	ret = hw_tune_power(&ispdma->agent.hw, ~0);
	ret |= hw_tune_power(&ccic->agent.hw, ~0);
	ret |= hw_tune_power(&dphy->agent.hw, ~0);
	ccic_set_mclk_rate(ccic, 1);

	if (ret < 0) {
		__isp = NULL;
		goto out_power_off;
	}

	/* We don't want to restore context before saving it! */
	if (isp->has_context == true)
		mvisp_restore_context(isp, NULL);
	else
		isp->has_context = true;

out_power_off:
	if (__isp == NULL && isp->isp_pwr_ctrl != NULL)
		isp->isp_pwr_ctrl(0);

out:
	if (__isp != NULL)
		isp->ref_count++;
	mutex_unlock(&isp->mvisp_mutex);

	return __isp;
}

void mvisp_put(struct mvisp_device *isp)
{
	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic = container_of(
					get_agent(isp->manager.hw, ccic_id),
					struct ccic_device, agent.hw);
	union agent_id dphy_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id	= AGENT_CCIC_DPHY,
	};
	struct ccic_dphy_t *dphy = container_of(
					get_agent(isp->manager.hw, dphy_id),
					struct ccic_dphy_t, agent.hw);
	union agent_id dxo_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_DXO_CORE,
	};
	struct isp_ispdma_device *ispdma = container_of(
					get_agent(isp->manager.hw, dxo_id),
					struct isp_ispdma_device, agent.hw);

	if (isp == NULL)
		return;

	mutex_lock(&isp->mvisp_mutex);
	BUG_ON(isp->ref_count == 0);
	if (--isp->ref_count == 0) {
		mvisp_save_context(isp, NULL);
		ccic_set_mclk_rate(ccic, 0);
		hw_tune_power(&dphy->agent.hw, 0);
		hw_tune_power(&ccic->agent.hw, 0);
		hw_tune_power(&ispdma->agent.hw, 0);
		if (isp->isp_pwr_ctrl != NULL)
			isp->isp_pwr_ctrl(0);

		if (isp->pdata->init_pin)
			isp->pdata->init_pin(isp->manager.dev, 0);
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
		pm_qos_update_request(&mvisp_qos_idle,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
#endif
	}
	mutex_unlock(&isp->mvisp_mutex);
}

/* --------------------------------------------------------------------------
 * Platform device driver
 */


#if 0

/*
 * Power management support.
 *
 * As the ISP can't properly handle an input video stream interruption on a non
 * frame boundary, the ISP pipelines need to be stopped before sensors get
 * suspended. However, as suspending the sensors can require a running clock,
 * which can be provided by the ISP, the ISP can't be completely suspended
 * before the sensor.
 *
 * To solve this problem power management support is split into prepare/complete
 * and suspend/resume operations. The pipelines are stopped in prepare() and the
 * ISP clocks get disabled in suspend(). Similarly, the clocks are reenabled in
 * resume(), and the the pipelines are restarted in complete().
 *
 * TODO: PM dependencies between the ISP and sensors are not modeled explicitly
 * yet.
 */
static int mvisp_pm_prepare(struct device *dev)
{
	struct mvisp_device *isp = dev_get_drvdata(dev);
	int reset;

	WARN_ON(mutex_is_locked(&isp->mvisp_mutex));

	if (isp->ref_count == 0)
		return 0;

	reset = mvisp_suspend_modules(isp);
	mvisp_disable_interrupts(isp);
	mvisp_save_context(isp, NULL);
	if (reset)
		mvisp_reset(isp);

	return 0;
}

static int mvisp_pm_suspend(struct device *dev)
{
	struct mvisp_device *isp = dev_get_drvdata(dev);

	WARN_ON(mutex_is_locked(&isp->mvisp_mutex));

	if (isp->ref_count)
		mvisp_disable_clocks(isp);

	return 0;
}

static int mvisp_pm_resume(struct device *dev)
{
	struct mvisp_device *isp = dev_get_drvdata(dev);

	if (isp->ref_count == 0)
		return 0;

	return mvisp_enable_clocks(isp);
}

static void mvisp_pm_complete(struct device *dev)
{
	struct mvisp_device *isp = dev_get_drvdata(dev);

	if (isp->ref_count == 0)
		return;

	mvisp_restore_context(isp, NULL);
	mvisp_enable_interrupts(isp);
	mvisp_resume_modules(isp);
}

#else

#define mvisp_pm_prepare	NULL
#define mvisp_pm_suspend	NULL
#define mvisp_pm_resume	NULL
#define mvisp_pm_complete	NULL

#endif /* CONFIG_PM */

static void mvisp_unregister_entities(struct mvisp_device *isp)
{
	v4l2_device_unregister(&isp->manager.v4l2_dev);
	media_device_unregister(&isp->manager.media_dev);
}

static struct v4l2_subdev *
mvisp_register_subdev_group(struct mvisp_device *isp,
		     struct mvisp_subdev_i2c_board_info *board_info)
{
	struct v4l2_subdev *sd = NULL;

	if (board_info->board_info == NULL)
		return NULL;

	for (; board_info->board_info; ++board_info) {
		struct v4l2_subdev *subdev;
		struct i2c_adapter *adapter;

		adapter = i2c_get_adapter(board_info->i2c_adapter_id);
		if (adapter == NULL) {
			dev_err(isp->manager.dev,
				"%s: Unable to get I2C adapter %d for"
				"device %s\n", __func__,
				board_info->i2c_adapter_id,
				board_info->board_info->type);
			continue;
		}

		subdev = v4l2_i2c_new_subdev_board(&isp->manager.v4l2_dev,
				adapter, board_info->board_info, NULL);
		if (subdev == NULL) {
			dev_err(isp->manager.dev, "%s: Unable to register subdev %s\n",
				__func__, board_info->board_info->type);
			continue;
		} else {
			sd = subdev;
			break;
		}
	}

	return sd;
}

int mvisp_connect_sensor_entities(struct mvisp_device *isp)
{
	struct media_entity *input;
	unsigned int flags;
	unsigned int pad;
	int ret = 0;
	struct map_pipeline *pipeline;
	struct mvisp_v4l2_subdevs_group *subdev_group;
	union agent_id id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic;

	if ((isp->sensor == NULL) || isp->sensor->host_priv == NULL)
		return -EINVAL;

	subdev_group =
		(struct mvisp_v4l2_subdevs_group *)isp->sensor->host_priv;
	v4l2_set_subdev_hostdata(isp->sensor, isp);

	switch (subdev_group->if_type) {
	case ISP_INTERFACE_CCIC_1:
	case ISP_INTERFACE_PARALLEL_0:
		id.dev_id = 0;
		break;
	case ISP_INTERFACE_CCIC_2:
	case ISP_INTERFACE_PARALLEL_1:
		id.dev_id = 1;
		break;
	default:
		dev_err(isp->manager.dev, "%s: invalid interface type %u\n",
			   __func__, subdev_group->if_type);
		return -EINVAL;
	}
	ccic = container_of(get_agent(isp->manager.hw, id),
				struct ccic_device, agent.hw);
	input = &ccic->agent.subdev.entity;
	pad = CCIC_PADI_SNSR;
	flags = 0;

	ret = media_entity_create_link(&isp->sensor->entity, 0,
		input, pad, flags);
	if (ret < 0)
		return ret;

	media_entity_call(&isp->sensor->entity, link_setup,
		&isp->sensor->entity.pads[0], &input->pads[pad], flags);
	media_entity_call(input, link_setup,
		&isp->sensor->entity.pads[0], &input->pads[pad], flags);

	id.mod_type = MAP_AGENT_NORMAL;
	id.mod_id = AGENT_CCIC_DPHY;
	input = &(container_of(get_agent(isp->manager.hw, id),
				struct map_agent, hw)->subdev.entity);
	ret = media_entity_create_link(&isp->sensor->entity, 0,
					input, DPHY_PADI, 0);
	if (ret < 0)
		return ret;

	list_for_each_entry(pipeline, &isp->manager.pipeline_pool, hook) {
		pipeline->def_src = &isp->sensor->entity;
	}

	return ret;
}

int sensor_set_clock(struct hw_agent *agent, int rate)
{
	/* FIXME: hard coding here, sensor mclk are not necessarily connected
	 * to CCIC_0 */
	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	union agent_id dphy_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_DPHY,
	};
	struct ccic_device *ccic = container_of(
					get_agent(agent->mngr, ccic_id),
					struct ccic_device, agent.hw);
	struct ccic_dphy_t *dphy = container_of(
					get_agent(agent->mngr, dphy_id),
					struct ccic_dphy_t, agent.hw);
	int ret;
	/* Yes, it's hard to believe, sensor clock is actually provided by
	 * ccic-core */
	ret = hw_tune_power(&ccic->agent.hw, rate);
	ret |= hw_tune_power(&dphy->agent.hw, rate);
	return ret;
}
EXPORT_SYMBOL(sensor_set_clock);

static int mvisp_detect_sensor(struct mvisp_device *isp)
{
	struct mvisp_platform_data *pdata = isp->pdata;
	struct mvisp_v4l2_subdevs_group *subdev_group;
	int ret = -EINVAL;
	struct v4l2_subdev *sensor;

	/* Register external entities */
	for (subdev_group = pdata->subdev_group;
			subdev_group->i2c_board_info; ++subdev_group) {
		/* Register the sensor devices */
#ifdef CONFIG_VIDEO_MVISP_SENSOR
		if (strcmp(subdev_group->name, "sensor group") == 0) {
#endif
			sensor = mvisp_register_subdev_group(isp,
						subdev_group->i2c_board_info);
			if (sensor == NULL)
				return ret;

			sensor->host_priv = subdev_group;
			isp->sensor = sensor;
			isp->sensor_connected = true;
			ret = 0;
#ifdef CONFIG_VIDEO_MVISP_SENSOR
		} else {/* Register the other devices */
			mvisp_register_subdev_group(isp,
					subdev_group->i2c_board_info);
		}
#endif
	}
	return ret;
}

static int mvisp_register_entities(struct mvisp_device *isp)
{
	int ret = 0;
	union agent_id dxo_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_DXO_CORE,
	};
	struct isp_ispdma_device *ispdma = container_of(
					get_agent(isp->manager.hw, dxo_id),
					struct isp_ispdma_device, agent.hw);

	/* Register internal entities */
	ret = mv_ispdma_register_entities(ispdma, &isp->manager.v4l2_dev);
	if (ret < 0)
		goto done;

done:
	return ret;
}

static void mvisp_cleanup_modules(struct mvisp_device *isp)
{
	union agent_id dxo_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_DXO_CORE,
	};
	struct isp_ispdma_device *ispdma = container_of(
					get_agent(isp->manager.hw, dxo_id),
					struct isp_ispdma_device, agent.hw);

	mv_ispdma_cleanup(ispdma);
}

static int mvisp_initialize_modules(struct mvisp_device *isp)
{
	int ret = 0;
	union agent_id dxo_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id		= 0,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id		= AGENT_DXO_CORE,
	};
	struct isp_ispdma_device *ispdma = container_of(
					get_agent(isp->manager.hw, dxo_id),
					struct isp_ispdma_device, agent.hw);

	union agent_id dmac_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id		= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id		= AGENT_DXO_DMAC,
	};
	struct isp_dma_mod *dmac = container_of(
					get_agent(isp->manager.hw, dmac_id),
					struct isp_dma_mod, agent.hw);

	union agent_id dmad_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id		= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id		= AGENT_DXO_DMAD,
	};
	struct isp_dma_mod *dmad = container_of(
					get_agent(isp->manager.hw, dmad_id),
					struct isp_dma_mod, agent.hw);

	union agent_id dmai_id = {
		.dev_type	= PCAM_IP_DXO,
		.dev_id		= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id		= AGENT_DXO_DMAI,
	};
	struct isp_dma_mod *dmai = container_of(
					get_agent(isp->manager.hw, dmai_id),
					struct isp_dma_mod, agent.hw);

	union agent_id ccic_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id	= 0,
		.mod_type	= MAP_AGENT_DMA_OUT,
		.mod_id	= AGENT_CCIC_CORE,
	};
	struct ccic_device *ccic = container_of(\
					get_agent(isp->manager.hw, ccic_id),
					struct ccic_device, agent.hw);

	union agent_id dphy_id = {
		.dev_type	= PCAM_IP_CCIC,
		.dev_id		= 0,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id		= AGENT_CCIC_DPHY,
	};
	struct ccic_dphy_t *dphy = container_of(
					get_agent(isp->manager.hw, dphy_id),
					struct ccic_dphy_t, agent.hw);
	struct map_link_dscr pxa988_links[] = {
		{
			.src_id		= &dphy_id,
			.src_pad	= DPHY_PADO_CCIC,
			.dst_id		= &ccic_id,
			.dst_pad	= CCIC_PADI_DPHY,
			.flags		= 0,
		},
		{
			.src_id		= &ccic_id,
			.src_pad	= CCIC_PADO_DXO,
			.dst_id		= &dxo_id,
			.dst_pad	= ISPDMA_PAD_SINK,
			.flags		= 0,
		},
	};
	struct map_pipeline *pipeline;

	/* establish the liasion between agents, suppose don't need this in the
	 * final version */
	ispdma->isp = ccic->isp = dphy->isp = isp;
	dmac->core = dmad->core = dmai->core = ispdma;

	/* Finally, create all the file nodes for each subdev */
	ret = map_manager_attach_agents(&isp->manager);
	if (ret < 0)
		return ret;
	list_for_each_entry(pipeline, &isp->manager.pipeline_pool, hook) {
		struct map_vnode *vnode = agent_get_video(&dmai->agent);
		pipeline->def_src = &vnode->vdev.entity;
	}
	return map_create_links(&isp->manager,
				pxa988_links, ARRAY_SIZE(pxa988_links));
}

static inline int __plat_get_slot(union agent_id id)
{
	switch (id.dev_type) {
	case PCAM_IP_DXO:
		switch (id.mod_id) {
		case AGENT_DXO_CORE:
			return 0;
		case AGENT_DXO_DMAI:
			return 1;
		case AGENT_DXO_DMAD:
			return 2;
		case AGENT_DXO_DMAC:
			return 3;
		}
		break;
	case PCAM_IP_CCIC:
		switch (id.mod_id) {
		case AGENT_CCIC_CORE:
			return AGENT_DXO_END * PCAM_DEV_DXO_END
				+ id.dev_id * AGENT_CCIC_END + 0;
		case AGENT_CCIC_DPHY:
			return AGENT_DXO_END * PCAM_DEV_DXO_END
				+ id.dev_id * AGENT_CCIC_END + 1;
		}
		break;
	case PCAM_IP_SENSOR:
		return AGENT_DXO_END * PCAM_DEV_DXO_END
			+ AGENT_CCIC_END * PCAM_DEV_CCIC_END
			+ id.mod_id;
	}
	return -EINVAL;
}

static int plat_get_slot(union agent_id id)
{
	return __plat_get_slot(id);
}

static struct hw_manager	pcam_manager = {
	.resrc_pool	= LIST_HEAD_INIT(pcam_manager.resrc_pool),
	.get_slot	= &plat_get_slot,
};

int plat_agent_register(struct hw_agent *agent)
{
	return hw_agent_register(agent, &pcam_manager);
}
EXPORT_SYMBOL(plat_agent_register);

int plat_resrc_register(struct device *dev, struct resource *res,
	const char *name, union agent_id mask,
	int res_id, void *handle, void *priv)
{
	if (hw_res_register(dev, res, &pcam_manager, name, mask, res_id,
				handle, priv) == NULL)
		return -ENOMEM;
	else
		return 0;
}
EXPORT_SYMBOL(plat_resrc_register);

static int mvisp_remove(struct platform_device *pdev)
{
	struct mvisp_device *isp = platform_get_drvdata(pdev);

	mvisp_unregister_entities(isp);
	mvisp_cleanup_modules(isp);

	free_irq(isp->irq_ipc, isp);

	mvisp_put_clocks(isp);

	if (isp->dummy_pages != NULL) {
		__free_pages(isp->dummy_pages, isp->dummy_order);
		isp->dummy_pages = NULL;
	}

	kfree(isp);
	hw_manager_clean(&pcam_manager);

	return 0;
}

static int mvisp_probe(struct platform_device *pdev)
{
	struct mvisp_platform_data *pdata = pdev->dev.platform_data;
	const struct platform_device_id *pid = platform_get_device_id(pdev);
	struct mvisp_device *isp;
	int ret;

	/* by this time, suppose all agents are registered */

	if (pdata == NULL)
		return -EINVAL;

	isp = kzalloc(sizeof(*isp) + sizeof(struct clk *) *
			(pdata->isp_clknum + pdata->ccic_clknum), GFP_KERNEL);
	if (!isp) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	isp->manager.hw = &pcam_manager;
	isp->manager.hw->drv_priv = &isp->manager;

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	strcpy(isp->mcd_root_codec.mcd.name, "dxo_codec");
	isp->mcd_root_codec.mcd.nr_entity = MCD_ENTITY_END;
	ret = mcd_init(&isp->mcd_root_codec.mcd);
	if (ret < 0)
		return ret;

	printk(KERN_INFO "cam: Marvell Camera Debug interface created in " \
		"debugfs/%s\n", isp->mcd_root_codec.mcd.name);

	strcpy(isp->mcd_root_display.mcd.name, "dxo_display");
	isp->mcd_root_display.mcd.nr_entity = MCD_ENTITY_END;
	ret = mcd_init(&isp->mcd_root_display.mcd);
	if (ret < 0)
		return ret;

	printk(KERN_INFO "cam: Marvell Camera Debug interface created in " \
		"debugfs/%s\n", isp->mcd_root_display.mcd.name);
#endif
	mutex_init(&isp->mvisp_mutex);

	isp->manager.dev = &pdev->dev;
	isp->pdata = pdata;
	isp->ref_count = 0;
	isp->has_context = false;
	isp->ccic_dummy_ena = pdata->ccic_dummy_ena;
	isp->ispdma_dummy_ena = pdata->ispdma_dummy_ena;
	isp->isp_pwr_ctrl = pdata->isp_pwr_ctrl;
	isp->cpu_type = pid->driver_data;

	platform_set_drvdata(pdev, isp);

	isp->manager.name = "MrvlCamMngr";
	isp->manager.dev = &pdev->dev;
	isp->manager.plat_priv = isp;
	ret = map_manager_init(&isp->manager);
	if (ret < 0)
		return ret;

	/* Clocks */
	ret = mvisp_get_clocks(isp, pdata);
	if (ret < 0)
		goto error;

	if (mvisp_get(isp) == NULL)
		goto error;

	mvisp_detect_sensor(isp);

	ret = mvisp_reset(isp);
	if (ret < 0)
		goto error_isp;

	/* Interrupt */
	isp->irq_ipc = platform_get_irq(pdev, 0);
	if (isp->irq_ipc <= 0) {
		dev_err(isp->manager.dev, "No IPC IRQ resource\n");
		ret = -ENODEV;
		goto error_irq_dma;
	}
	if (request_irq(isp->irq_ipc,
			mvisp_ipc_isr, IRQF_DISABLED,
			"mv_ispirq", isp)) {
		dev_err(isp->manager.dev, "Unable to request IPC IRQ\n");
		ret = -EINVAL;
		goto error_irq_dma;
	}

	/* Entities */
	ret = mvisp_initialize_modules(isp);
	if (ret < 0)
		goto error_irq_dma;

	ret = mvisp_register_entities(isp);
	if (ret < 0)
		goto error_modules;

	if (isp->sensor_connected == true) {
		ret = mvisp_connect_sensor_entities(isp);
		if (ret < 0)
			goto error_modules;
	}

	mvisp_put(isp);

	if (isp->sensor_connected == false)
		mvisp_put_sensor_resource(isp);

	return 0;

error_modules:
	mvisp_cleanup_modules(isp);
error_irq_dma:
	free_irq(isp->irq_ipc, isp);
error_isp:
	mvisp_put(isp);
error:
	mvisp_put_clocks(isp);

	platform_set_drvdata(pdev, NULL);
	kfree(isp);
	return ret;
}

static const struct dev_pm_ops mvisp_pm_ops = {
	.prepare = mvisp_pm_prepare,
	.suspend = mvisp_pm_suspend,
	.resume = mvisp_pm_resume,
	.complete = mvisp_pm_complete,
};

static const struct platform_device_id mvisp_id_table[] = {
	{"mmp3-mvisp",   MV_MMP3},
	{"pxa988-mvisp", MV_PXA988},
	{ },
};
MODULE_DEVICE_TABLE(platform, i2c_pxa_id_table);

static struct platform_driver mvisp_driver = {
	.probe = mvisp_probe,
	.remove = mvisp_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "mvisp",
		.pm	= &mvisp_pm_ops,
	},
	.id_table = mvisp_id_table,
};

/*
 * mvisp_init - ISP module initialization.
 */
static int __init mvisp_init(void)
{
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	mvisp_qos_idle.name = "DxOISP";
	pm_qos_add_request(&mvisp_qos_idle, PM_QOS_CPUIDLE_BLOCK,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
#endif
	return platform_driver_register(&mvisp_driver);
}

/*
 * mvisp_cleanup - ISP module cleanup.
 */
static void __exit mvisp_cleanup(void)
{
	platform_driver_unregister(&mvisp_driver);
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	pm_qos_remove_request(&mvisp_qos_idle);
#endif
}

module_init(mvisp_init);
module_exit(mvisp_cleanup);

MODULE_AUTHOR("Marvell Technology Ltd.");
MODULE_DESCRIPTION("DxO ISP driver");
MODULE_LICENSE("GPL");
