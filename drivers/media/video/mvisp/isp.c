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

#define MVISP_DUMMY_BUF_SIZE	(1920*1080*2)
#define ISP_STOP_TIMEOUT	msecs_to_jiffies(1000)

#ifdef CONFIG_CPU_PXA988
static struct pm_qos_request mvisp_qos_idle;
#endif

/*
 * mvisp_enable_interrupts - Enable ISP interrupts.
 */
static void mvisp_enable_interrupts(struct mvisp_device *isp)
{
	mvisp_reg_writel(isp, 0x1, ISP_IOMEM_ISPDMA, ISP_IRQMASK);
}

/*
 * mvisp_disable_interrupts - Disable ISP interrupts.
 */
static void mvisp_disable_interrupts(struct mvisp_device *isp)
{
	mvisp_reg_writel(isp, 0x0, ISP_IOMEM_ISPDMA, ISP_IRQMASK);
}


static void mvisp_power_settings(struct mvisp_device *isp, int idle)
{
}


static irqreturn_t mvisp_ipc_isr(int irq, void *_isp)
{
	struct mvisp_device *isp = _isp;
	u32 irqstatus;

	irqstatus = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISP_IRQSTAT);
	mvisp_reg_writel(isp, irqstatus,
			ISP_IOMEM_ISPDMA, ISP_IRQSTAT);
	mv_ispdma_ipc_isr_handler(&isp->mvisp_ispdma);

	return IRQ_HANDLED;
}

static irqreturn_t pxa_ccic_isr_1(int irq, void *_isp)
{

	struct mvisp_device *isp = _isp;
	u32 irqstatus;

	irqstatus = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);
	mvisp_reg_writel(isp, irqstatus,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);
	pxa_ccic_dma_isr_handler(&isp->mvisp_ccic1, irqstatus);

	return IRQ_HANDLED;
}


static irqreturn_t mvisp_dma_isr(int irq, void *_isp)
{

	struct mvisp_device *isp = _isp;
	u32 irqstatus;

	irqstatus = mvisp_reg_readl(isp,
		ISP_IOMEM_ISPDMA, ISPDMA_IRQSTAT);

	mvisp_reg_writel(isp, irqstatus,
		ISP_IOMEM_ISPDMA, ISPDMA_IRQSTAT);
	mv_ispdma_dma_isr_handler(&isp->mvisp_ispdma, irqstatus);

	return IRQ_HANDLED;
}

static int mvisp_pipeline_pm_use_count(struct media_entity *entity)
{
	struct media_entity_graph graph;
	int use = 0;

	media_entity_graph_walk_start(&graph, entity);

	while ((entity = media_entity_graph_walk_next(&graph))) {
		if (media_entity_type(entity) == MEDIA_ENT_T_DEVNODE)
			use += entity->use_count;
	}

	return use;
}

static int mvisp_pipeline_pm_power_one(struct media_entity *entity, int change)
{
	struct v4l2_subdev *subdev;
	int ret;

	subdev = media_entity_type(entity) == MEDIA_ENT_T_V4L2_SUBDEV
	       ? media_entity_to_v4l2_subdev(entity) : NULL;

	if (entity->use_count == 0 && change > 0 && subdev != NULL) {
		ret = v4l2_subdev_call(subdev, core, s_power, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
	}

	entity->use_count += change;
	WARN_ON(entity->use_count < 0);

	if (entity->use_count == 0 && change < 0 && subdev != NULL)
		v4l2_subdev_call(subdev, core, s_power, 0);

	return 0;
}

static int mvisp_pipeline_pm_power(struct media_entity *entity, int change)
{
	struct media_entity_graph graph;
	struct media_entity *first = entity;
	int ret = 0;

	if (!change)
		return 0;

	media_entity_graph_walk_start(&graph, entity);
	while (!ret && (entity = media_entity_graph_walk_next(&graph)))
		if (media_entity_type(entity) != MEDIA_ENT_T_DEVNODE)
			ret = mvisp_pipeline_pm_power_one(entity, change);

	if (!ret)
		return 0;

	media_entity_graph_walk_start(&graph, first);
	while ((first = media_entity_graph_walk_next(&graph))
	       && first != entity)
		if (media_entity_type(first) != MEDIA_ENT_T_DEVNODE)
			mvisp_pipeline_pm_power_one(first, -change);

	return ret;
}

int mvisp_pipeline_pm_use(struct media_entity *entity, int use)
{
	int change = use ? 1 : -1;
	int ret;

	mutex_lock(&entity->parent->graph_mutex);

	/* Apply use count to node. */
	entity->use_count += change;
	WARN_ON(entity->use_count < 0);

	/* Apply power change to connected non-nodes. */
	ret = mvisp_pipeline_pm_power(entity, change);

	mutex_unlock(&entity->parent->graph_mutex);

	return ret;
}

static int mvisp_pipeline_link_notify(struct media_pad *source,
				    struct media_pad *sink, u32 flags)
{
	int source_use = mvisp_pipeline_pm_use_count(source->entity);
	int sink_use = mvisp_pipeline_pm_use_count(sink->entity);
	int ret;

	if (!(flags & MEDIA_LNK_FL_ENABLED)) {
		/* Powering off entities is assumed to never fail. */
		mvisp_pipeline_pm_power(source->entity, -sink_use);
		mvisp_pipeline_pm_power(sink->entity, -source_use);
		return 0;
	}

	ret = mvisp_pipeline_pm_power(source->entity, sink_use);
	if (ret < 0)
		return ret;

	ret = mvisp_pipeline_pm_power(sink->entity, source_use);
	if (ret < 0)
		mvisp_pipeline_pm_power(source->entity, -sink_use);

	return ret;
}

static int mvisp_pipeline_enable(struct isp_pipeline *pipe,
			       enum isp_pipeline_stream_state mode)
{
	struct media_entity *entity;
	struct media_pad *pad;
	struct v4l2_subdev *subdev;
	int ret = 0, cnt;

	for (cnt = 0; cnt < FAR_END_MAX_NUM; cnt++) {
		if (pipe->output[cnt] == NULL)
			continue;

		if (pipe->video_state[cnt] == mode)
			continue;
		pipe->video_state[cnt] = mode;

		entity = &pipe->output[cnt]->video.entity;
		while (1) {
			pad = &entity->pads[0];
			if (!(pad->flags & MEDIA_PAD_FL_SINK))
				break;

			pad = media_entity_remote_source(pad);
			if (pad == NULL ||
			    media_entity_type(pad->entity)
			    != MEDIA_ENT_T_V4L2_SUBDEV)
				break;

			entity = pad->entity;
			subdev = media_entity_to_v4l2_subdev(entity);

			ret = v4l2_subdev_call(subdev, video, s_stream, mode);
			if (ret < 0 && ret != -ENOIOCTLCMD)
				break;
		}
	}

	return ret;
}

static int mvisp_pipeline_disable(struct isp_pipeline *pipe,
			       enum isp_pipeline_stream_state mode)
{
	struct media_entity *entity;
	struct media_pad *pad;
	struct v4l2_subdev *subdev;
	int failure = 0;
	int cnt;

	for (cnt = 0; cnt < FAR_END_MAX_NUM; cnt++) {
		if (pipe->output[cnt] == NULL)
			continue;

		if (pipe->video_state[cnt] == mode)
			continue;
		pipe->video_state[cnt] = mode;

		entity = &pipe->output[cnt]->video.entity;
		while (1) {
			pad = &entity->pads[0];
			if (!(pad->flags & MEDIA_PAD_FL_SINK))
				break;

			pad = media_entity_remote_source(pad);
			if (pad == NULL ||
				media_entity_type(pad->entity)
					!= MEDIA_ENT_T_V4L2_SUBDEV)
				break;

			entity = pad->entity;
			subdev = media_entity_to_v4l2_subdev(entity);

			if (failure == 0)
				failure = v4l2_subdev_call(subdev, video,
						s_stream, mode);
			else
				v4l2_subdev_call(subdev, video, s_stream, mode);
		}
	}

	return failure;
}

int mvisp_pipeline_set_stream(struct isp_pipeline *pipe,
				 enum isp_pipeline_stream_state state)
{
	int ret;

	if (state == ISP_PIPELINE_STREAM_STOPPED)
		ret = mvisp_pipeline_disable(pipe, state);
	else
		ret = mvisp_pipeline_enable(pipe, state);

	pipe->stream_state = state;

	return ret;
}

#if 0
static void mvisp_pipeline_resume(struct isp_pipeline *pipe)
{

	int singleshot = (pipe->stream_state == ISP_PIPELINE_STREAM_SINGLESHOT);
	int cnt;

	mvisp_video_resume(pipe->output, !singleshot);
	if (singleshot)
		mvisp_video_resume(pipe->input, 0);
		mvisp_pipeline_enable(pipe, pipe->stream_state);
}

static void mvisp_pipeline_suspend(struct isp_pipeline *pipe)
{
	mvisp_pipeline_disable(pipe);
}

static bool mvisp_pipeline_is_last(struct media_entity *me)
{
	struct isp_pipeline *pipe;
	struct media_pad *pad;
	int cnt;

	if (!me->pipe)
		return false;
	pipe = to_isp_pipeline(me);
	if (pipe->stream_state == ISP_PIPELINE_STREAM_STOPPED)
		return false;

	for (cnt = 0; cnt < FAR_END_MAX_NUM; cnt++) {
		if (pipe->output[cnt] == NULL)
			continue;
		pad = media_entity_remote_source(&pipe->output[cnt]->pad);
		if (pad->entity == me)
			return true;
	}

	return false;
}

static void mvisp_suspend_module_pipeline(struct media_entity *me)
{
	if (mvisp_pipeline_is_last(me))
		mvisp_pipeline_suspend(to_isp_pipeline(me));
}

static void mvisp_resume_module_pipeline(struct media_entity *me)
{
	if (mvisp_pipeline_is_last(me))
		mvisp_pipeline_resume(to_isp_pipeline(me));
}

static int mvisp_suspend_modules(struct mvisp_device *isp)
{
	/*unsigned long timeout;*/

	mvisp_suspend_module_pipeline(&isp->mvisp_ispdma.subdev.entity);
	mvisp_suspend_module_pipeline(&isp->mvisp_ccic1.subdev.entity);

#if 0
	timeout = jiffies + mvisp_STOP_TIMEOUT;
	while (isp_pipeline_wait(&isp->mvisp_ispdma) {
		if (time_after(jiffies, timeout)) {
			dev_info(isp->dev, "can't stop modules.\n");
			return 1;
		}
		msleep(20);
	}
#endif
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

static int mvisp_enable_clocks(struct mvisp_device *isp)
{
	int clk_i;
	int ret = 0;

	for (clk_i = 0; clk_i < isp->isp_clknum +
			isp->ccic_clknum; clk_i++) {
		if (isp->clock[clk_i] != NULL) {
			ret = clk_enable(isp->clock[clk_i]);
			if (ret) {
				dev_err(isp->dev, "clk_enable failed\n");
				goto out_clk_enable;
			}
		}
	}

	pxa_ccic_ctrl_pixclk(isp, 1);
	return 0;

out_clk_enable:
	for (--clk_i; clk_i >= 0; clk_i--) {
		if (isp->clock[clk_i] != NULL)
			clk_disable(isp->clock[clk_i]);
	}

	return ret;
}

static void mvisp_disable_clocks(struct mvisp_device *isp)
{
	int clk_i;

	pxa_ccic_ctrl_pixclk(isp, 0);

	for (clk_i = 0; clk_i < isp->isp_clknum +
			isp->ccic_clknum; clk_i++) {
		if (isp->clock[clk_i] != NULL)
			clk_disable(isp->clock[clk_i]);
	}
}

static void mvisp_put_clocks(struct mvisp_device *isp)
{
	int clk_i;

	for (clk_i = 0; clk_i < isp->isp_clknum +
			isp->ccic_clknum; clk_i++) {
		if (isp->clock[clk_i] != NULL) {
			clk_put(isp->clock[clk_i]);
			isp->clock[clk_i] = NULL;
		}
	}
}

static int mvisp_get_clocks(struct mvisp_device *isp,
		struct mvisp_platform_data *pdata)
{
	struct clk *clk;
	int clk_i;
	int ret = 0;
	unsigned int isp_clknum = isp->isp_clknum;
	unsigned int ccic_clknum = isp->ccic_clknum;


	for (clk_i = 0; clk_i < isp_clknum + ccic_clknum; clk_i++)
		isp->clock[clk_i] = NULL;

	for (clk_i = 0; clk_i < isp_clknum + ccic_clknum; clk_i++) {
		clk = clk_get(isp->dev, pdata->clkname[clk_i]);
		if (IS_ERR(clk)) {
			dev_err(isp->dev, "clk_get %s failed\n",
					pdata->clkname[clk_i]);
			ret = PTR_ERR(clk);
			goto out_get_clk;
		}
		isp->clock[clk_i] = clk;
	}

	return 0;

out_get_clk:
	for (--clk_i; clk_i >= 0; clk_i--) {
		clk_put(isp->clock[clk_i]);
		isp->clock[clk_i] = NULL;
	}

	return ret;
}


static void mvisp_put_sensor_resource(struct mvisp_device *isp)
{
	int i;

	isp->mmio_base[CCIC_ISP_IOMEM_1] = NULL;

	for (i = 0; i < isp->ccic_clknum; i++) {
		if (isp->clock[isp->isp_clknum + i] != NULL) {
			clk_put(isp->clock[isp->isp_clknum + i]);
			isp->clock[isp->isp_clknum + i] = NULL;
		}
	}
}

struct mvisp_device *mvisp_get(struct mvisp_device *isp)
{
	struct mvisp_device *__isp = isp;

	if (isp == NULL)
		return NULL;

	mutex_lock(&isp->mvisp_mutex);
	if (isp->ref_count > 0)
		goto out;

#ifdef CONFIG_CPU_PXA988
	pm_qos_update_request(&mvisp_qos_idle,
			PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
#endif
	if (isp->pdata->init_pin)
		isp->pdata->init_pin(isp->dev, 1);

	if (isp->isp_pwr_ctrl == NULL || isp->isp_pwr_ctrl(1) < 0) {
		__isp = NULL;
		goto out;
	}

	if (mvisp_enable_clocks(isp) < 0) {
		__isp = NULL;
		goto out_power_off;
	}

	/* We don't want to restore context before saving it! */
	if (isp->has_context == true)
		mvisp_restore_context(isp, NULL);
	else
		isp->has_context = true;

	mvisp_enable_interrupts(isp);

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
	if (isp == NULL)
		return;

	mutex_lock(&isp->mvisp_mutex);
	BUG_ON(isp->ref_count == 0);
	if (--isp->ref_count == 0) {
		mvisp_disable_interrupts(isp);
		mvisp_save_context(isp, NULL);
		mvisp_disable_clocks(isp);
		if (isp->isp_pwr_ctrl != NULL)
			isp->isp_pwr_ctrl(0);

		if (isp->pdata->init_pin)
			isp->pdata->init_pin(isp->dev, 0);
#ifdef CONFIG_CPU_PXA988
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
	mv_ispdma_unregister_entities(&isp->mvisp_ispdma);
	pxa_ccic_unregister_entities(&isp->mvisp_ccic1);

	v4l2_device_unregister(&isp->v4l2_dev);
	media_device_unregister(&isp->media_dev);
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
			dev_err(isp->dev,
				"%s: Unable to get I2C adapter %d for"
				"device %s\n", __func__,
				board_info->i2c_adapter_id,
				board_info->board_info->type);
			continue;
		}

		subdev = v4l2_i2c_new_subdev_board(&isp->v4l2_dev, adapter,
				board_info->board_info, NULL);
		if (subdev == NULL) {
			dev_err(isp->dev, "%s: Unable to register subdev %s\n",
				__func__, board_info->board_info->type);
			continue;
		} else {
			sd = subdev;
			break;
		}
	}

	return sd;
}


static int mvisp_register_ispdev(struct mvisp_device *isp)
{
	int ret = 0;
	isp->media_dev.dev = isp->dev;

	strlcpy(isp->media_dev.model, "DXO ISP",
		sizeof(isp->media_dev.model));
	isp->media_dev.link_notify = mvisp_pipeline_link_notify;
	ret = media_device_register(&isp->media_dev);
	if (ret < 0) {
		dev_err(isp->dev, "%s: Media device registration failed (%d)\n",
			__func__, ret);
		return ret;
	}

	isp->v4l2_dev.mdev = &isp->media_dev;
	ret = v4l2_device_register(isp->dev, &isp->v4l2_dev);
	if (ret < 0) {
		dev_err(isp->dev, "%s: V4L2 device registration failed (%d)\n",
			__func__, ret);

		media_device_unregister(&isp->media_dev);
	}

	return ret;
}


int mvisp_connect_sensor_entities(struct mvisp_device *isp)
{
	struct media_entity *input;
	unsigned int flags;
	unsigned int pad;
	int ret = 0;
	struct mvisp_v4l2_subdevs_group *subdev_group;

	if ((isp->sensor == NULL) || isp->sensor->host_priv == NULL)
		return -EINVAL;

	subdev_group =
		(struct mvisp_v4l2_subdevs_group *)isp->sensor->host_priv;

	switch (subdev_group->if_type) {
	case ISP_INTERFACE_CCIC_1:
		pxa_ccic_set_sensor_type(&isp->mvisp_ccic1,
				isp->sensor_type);
		input = &isp->mvisp_ccic1.subdev.entity;
		pad = CCIC_PAD_SINK;
		flags = 0;
		break;
	case ISP_INTERFACE_CCIC_2:
		pxa_ccic_set_sensor_type(&isp->mvisp_ccic2,
				isp->sensor_type);
		input = &isp->mvisp_ccic2.subdev.entity;
		pad = CCIC_PAD_SINK;
		flags = 0;
		break;
	case ISP_INTERFACE_PARALLEL_0:
	case ISP_INTERFACE_PARALLEL_1:
	default:
		dev_err(isp->dev, "%s: invalid interface type %u\n",
			   __func__, subdev_group->if_type);
		return -EINVAL;
	}

	ret = media_entity_create_link(&isp->sensor->entity, 0,
		input, pad, flags);
	if (ret < 0)
		return ret;

	media_entity_call(&isp->sensor->entity, link_setup,
		&isp->sensor->entity.pads[0], &input->pads[pad], flags);
	media_entity_call(input, link_setup,
		&isp->sensor->entity.pads[0], &input->pads[pad], flags);

	return ret;
}


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
#ifdef VIDEO_MVISP_SENSOR
		if (strcmp(subdev_group->name, "sensor group") == 0) {
#endif
			sensor = mvisp_register_subdev_group(isp,
						subdev_group->i2c_board_info);
			if (sensor == NULL)
				return ret;

			sensor->host_priv = subdev_group;

			/* For unsupported sensor, ignore it*/
			if (strncmp(sensor->entity.name, "ov882", 5) == 0) {
				isp->sensor_type = SENSOR_OV882X;
				isp->sensor = sensor;
				ret = 0;
			} else if (strncmp(sensor->entity.name,
						"ov5647", 6) == 0) {
				isp->sensor_type = SENSOR_OV5647;
				isp->sensor = sensor;
				ret = 0;
			}else if (strncmp(sensor->entity.name,
						 "lsi3h5", 6) == 0) {
				isp->sensor_type = SENSOR_LSI3H5;
				isp->sensor = sensor;
				ret = 0;
			}
#ifdef VIDEO_MVISP_SENSOR
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

	/* Register internal entities */
	ret = mv_ispdma_register_entities(&isp->mvisp_ispdma, &isp->v4l2_dev);
	if (ret < 0)
		goto done;

	if (isp->sensor_connected == true) {
		ret = pxa_ccic_register_entities(
			&isp->mvisp_ccic1, &isp->v4l2_dev);
		if (ret < 0)
			goto done;
	}

	ret = v4l2_device_register_subdev_nodes(&isp->v4l2_dev);
done:
	if (ret < 0) {
		mv_ispdma_unregister_entities(&isp->mvisp_ispdma);
		pxa_ccic_unregister_entities(&isp->mvisp_ccic1);
	}

	return ret;
}

static void mvisp_cleanup_modules(struct mvisp_device *isp)
{
	mv_ispdma_cleanup(isp);
}

static int mvisp_initialize_modules(struct mvisp_device *isp)
{
	int ret = 0;

	ret = mv_ispdma_init(isp);
	if (ret < 0) {
		dev_err(isp->dev, "DXO IPC initialization failed\n");
		goto error_ispdma;
	}

	if (isp->sensor_connected == true) {
		ret = pxa_ccic_init(isp);
		if (ret < 0) {
			dev_err(isp->dev, "PXA CCIC initialization failed\n");
			goto error_ispccic;
		}

		/* Connect the submodules. */
		ret = media_entity_create_link(
				&isp->mvisp_ccic1.subdev.entity,
				CCIC_PAD_ISP_SRC,
				&isp->mvisp_ispdma.subdev.entity,
				ISPDMA_PAD_SINK, 0);
		if (ret < 0)
			goto error_link;
	}

	return 0;

error_link:
	pxa_ccic_cleanup(isp);
error_ispccic:
	mv_ispdma_cleanup(isp);
error_ispdma:
	return ret;
}

static int mvisp_remove(struct platform_device *pdev)
{
	struct mvisp_device *isp = platform_get_drvdata(pdev);
	int i;

	mvisp_unregister_entities(isp);
	mvisp_cleanup_modules(isp);

	free_irq(isp->irq_ipc, isp);
	free_irq(isp->irq_dma, isp);
	if (isp->sensor_connected == true)
		free_irq(isp->irq_ccic1, isp);

	mvisp_put_clocks(isp);

	for (i = 0; i < CCIC_ISP_IOMEM_1; i++) {
		if (isp->mmio_base[i]) {
			iounmap(isp->mmio_base[i]);
			isp->mmio_base[i] = NULL;
		}

		if (isp->mmio_base_phys[i]) {
			release_mem_region(isp->mmio_base_phys[i],
					   isp->mmio_size[i]);
			isp->mmio_base_phys[i] = 0;
		}
	}

	if (isp->dummy_pages != NULL) {
		__free_pages(isp->dummy_pages, isp->dummy_order);
		isp->dummy_pages = NULL;
	}

	kfree(isp);

	return 0;
}

static int mvisp_map_mem_resource(struct platform_device *pdev,
				struct mvisp_device *isp)
{
	struct resource *mem;
	int cnt;

	for (cnt = 0; cnt < CCIC_ISP_IOMEM_1; cnt++) {
		/* request the mem region for the camera registers */
		mem = platform_get_resource(pdev, IORESOURCE_MEM, cnt);
		if (!mem) {
			dev_err(isp->dev, "no mem resource? cnt %d\n", cnt);
			return -ENODEV;
		}
		if (!request_mem_region(mem->start,
				resource_size(mem), pdev->name)) {
			dev_err(isp->dev,
				"cannot reserve camera register I/O region\n");
			return -ENODEV;
		}
		isp->mmio_base_phys[cnt] = mem->start;
		isp->mmio_size[cnt] = resource_size(mem);
		/* map the region */
		isp->mmio_base[cnt] =
			ioremap_nocache(isp->mmio_base_phys[cnt],
							isp->mmio_size[cnt]);
		if (!isp->mmio_base[cnt]) {
			dev_err(isp->dev,
				"cannot map camera register I/O region %d\n"
				, cnt);
			return -ENODEV;
		}
	}

	/* ccic address are statically mapped. */
	isp->mmio_base_phys[CCIC_ISP_IOMEM_1] = 0;
	isp->mmio_size[CCIC_ISP_IOMEM_1] = 0;
	/* map the region */
	isp->mmio_base[CCIC_ISP_IOMEM_1] =
		(void __iomem *)CCIC1_VIRT_BASE;

	return 0;
}

static int mvisp_prepare_dummy(struct mvisp_device *isp)
{
	isp->dummy_vaddr = NULL;
	isp->dummy_paddr = 0;
	isp->dummy_pages = NULL;
	isp->dummy_order = 0;

	if (isp->ccic_dummy_ena || isp->ispdma_dummy_ena) {
		isp->dummy_order = get_order(PAGE_ALIGN(MVISP_DUMMY_BUF_SIZE));
		isp->dummy_pages = alloc_pages(
				GFP_KERNEL, isp->dummy_order);
		if (isp->dummy_pages != NULL) {
			isp->dummy_vaddr =
				(void *)page_address(isp->dummy_pages);
			if (isp->dummy_vaddr != NULL)
				isp->dummy_paddr = __pa(isp->dummy_vaddr);
		}
	}

	return 0;
}

static int mvisp_probe(struct platform_device *pdev)
{
	struct mvisp_platform_data *pdata = pdev->dev.platform_data;
	const struct platform_device_id *pid = platform_get_device_id(pdev);
	struct mvisp_device *isp;
	int ret;
	int i;

	if (pdata == NULL)
		return -EINVAL;

	isp = kzalloc(sizeof(*isp) + sizeof(struct clk *) *
			(pdata->isp_clknum + pdata->ccic_clknum), GFP_KERNEL);
	if (!isp) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}
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

	isp->dev = &pdev->dev;
	isp->pdata = pdata;
	isp->ref_count = 0;
	isp->has_context = false;
	isp->ccic_dummy_ena = pdata->ccic_dummy_ena;
	isp->ispdma_dummy_ena = pdata->ispdma_dummy_ena;
	isp->isp_pwr_ctrl = pdata->isp_pwr_ctrl;
	isp->isp_clknum = pdata->isp_clknum;
	isp->ccic_clknum = pdata->ccic_clknum;
	isp->cpu_type = pid->driver_data;

	platform_set_drvdata(pdev, isp);

	ret = mvisp_register_ispdev(isp);
	if (ret < 0)
		return ret;

	/* Clocks */
	ret = mvisp_map_mem_resource(pdev, isp);
	if (ret < 0)
		goto error;

	ret = mvisp_get_clocks(isp, pdata);
	if (ret < 0)
		goto error;

	if (mvisp_get(isp) == NULL)
		goto error;

	ret = mvisp_detect_sensor(isp);
	if (ret < 0)
		isp->sensor_connected = false;
	else
		isp->sensor_connected = true;

	ret = mvisp_reset(isp);
	if (ret < 0)
		goto error_isp;


	/* Interrupt */
	isp->irq_dma = platform_get_irq(pdev, 0);
	if (isp->irq_dma <= 0) {
		dev_err(isp->dev, "No DMA IRQ resource\n");
		ret = -ENODEV;
		goto error_isp;
	}
	if (request_irq(isp->irq_dma,
			mvisp_dma_isr, IRQF_DISABLED,
			"mv_ispdmairq", isp)) {
		dev_err(isp->dev, "Unable to request DMA IRQ\n");
		ret = -EINVAL;
		goto error_isp;
	}

	isp->irq_ipc = platform_get_irq(pdev, 1);
	if (isp->irq_ipc <= 0) {
		dev_err(isp->dev, "No IPC IRQ resource\n");
		ret = -ENODEV;
		goto error_irq_dma;
	}
	if (request_irq(isp->irq_ipc,
			mvisp_ipc_isr, IRQF_DISABLED,
			"mv_ispirq", isp)) {
		dev_err(isp->dev, "Unable to request IPC IRQ\n");
		ret = -EINVAL;
		goto error_irq_dma;
	}

	if (isp->sensor_connected == true) {
		isp->irq_ccic1 = platform_get_irq(pdev, 2);
		if (isp->irq_ccic1 <= 0) {
			dev_err(isp->dev, "No CCIC IRQ resource\n");
			ret = -ENODEV;
			goto error_irq_ipc;
		}
		if (request_irq(isp->irq_ccic1,
				pxa_ccic_isr_1, IRQF_DISABLED|IRQF_SHARED,
				"pxa_ccicirq", isp)) {
			dev_err(isp->dev, "Unable to request CCIC IRQ\n");
			ret = -EINVAL;
			goto error_irq_ipc;
		}
	}

	/* Entities */
	ret = mvisp_initialize_modules(isp);
	if (ret < 0)
		goto error_irq_ccic;

	ret = mvisp_register_entities(isp);
	if (ret < 0)
		goto error_modules;

	if (isp->sensor_connected == true) {
		ret = mvisp_connect_sensor_entities(isp);
		if (ret < 0)
			goto error_modules;

		mvisp_prepare_dummy(isp);
	}

	mvisp_power_settings(isp, 1);
	mvisp_put(isp);

	if (isp->sensor_connected == false)
		mvisp_put_sensor_resource(isp);

	return 0;

error_modules:
	mvisp_cleanup_modules(isp);
error_irq_ccic:
	if (isp->sensor_connected == true)
		free_irq(isp->irq_ccic1, isp);
error_irq_ipc:
	free_irq(isp->irq_ipc, isp);
error_irq_dma:
	free_irq(isp->irq_dma, isp);
error_isp:
	mvisp_put(isp);
error:
	mvisp_put_clocks(isp);

	for (i = 0; i < CCIC_ISP_IOMEM_1; i++) {
		if (isp->mmio_base[i]) {
			iounmap(isp->mmio_base[i]);
			isp->mmio_base[i] = NULL;
		}

		if (isp->mmio_base_phys[i]) {
			release_mem_region(isp->mmio_base_phys[i],
					   isp->mmio_size[i]);
			isp->mmio_base_phys[i] = 0;
		}
	}

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
#ifdef CONFIG_CPU_PXA988
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
#ifdef CONFIG_CPU_PXA988
	pm_qos_remove_request(&mvisp_qos_idle);
#endif
}

module_init(mvisp_init);
module_exit(mvisp_cleanup);

MODULE_AUTHOR("Marvell Technology Ltd.");
MODULE_DESCRIPTION("DxO ISP driver");
MODULE_LICENSE("GPL");
