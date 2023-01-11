/*
 * b52isp.c
 *
 * Marvell B52 ISP driver, based on soc-isp framework
 *
 * Copyright:  (C) Copyright 2013 Marvell Technology Shanghai Ltd.
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
 */

#include <linux/device.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/mrvl-camera.h>
#include <media/b52-sensor.h>
#include <uapi/media/b52_api.h>
#include <linux/workqueue.h>
#ifdef CONFIG_DEVFREQ_GOV_THROUGHPUT
#include <linux/platform_data/camera.h>
#endif

#include "plat_cam.h"
#include "b52isp.h"
#include "b52-reg.h"

#define B52ISP_DRV_NAME		"ovt-isp-drv"
#define B52ISP_NAME		"ovt-isp"
#define B52ISP_IRQ_NAME		"ovt-isp-irq"
#define MS_PER_JIFFIES  10

#define PATH_PER_PIPE 3

static struct pm_qos_request ddrfreq_qos_req_min;

static void b52isp_tasklet(unsigned long data);

static void *meta_cpu;
static dma_addr_t meta_dma;
#define META_DATA_SIZE  0x81

static int trace = 2;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4)"
		"0 - mute"
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

#define mac_wport_mask(n)	\
	(((1 << B52AXI_PORT_W1) | (1 << B52AXI_PORT_W2))	\
		<< (B52AXI_PORT_CNT * n))
#define mac_rport_mask(n)	\
	((1 << B52AXI_PORT_R1) << (B52AXI_PORT_CNT * n))
static uint output_mask[9];
#if 0
static uint output_mask[9] = {
	mac_wport_mask(0) | mac_wport_mask(1) | mac_wport_mask(2),
	mac_wport_mask(0) | mac_wport_mask(1) | mac_wport_mask(2),
	mac_rport_mask(0) | mac_rport_mask(1) | mac_rport_mask(2),
	mac_wport_mask(0) | mac_wport_mask(1) | mac_wport_mask(2),
	mac_wport_mask(0) | mac_wport_mask(1) | mac_wport_mask(2),
	mac_rport_mask(0) | mac_rport_mask(1) | mac_rport_mask(2),
	mac_wport_mask(0) | mac_wport_mask(1) | mac_wport_mask(2),
	mac_wport_mask(0) | mac_wport_mask(1) | mac_wport_mask(2),
	mac_rport_mask(0) | mac_rport_mask(1) | mac_rport_mask(2),
};
#endif
module_param_array(output_mask, uint, NULL, 0644);
MODULE_PARM_DESC(output_mask,
		"Each of the array element is a mask used to specify which "
		"physical ports are allowed to connect to logical output. For "
		"example, output_mask[0] = 0xDB means WRITE#0/WRITE#1 on each "
		"MAC are allowed to connect to output#0.");

struct b52isp_hw_desc b52isp_hw_table[] = {
	[B52ISP_SINGLE] = {
		.idi_port	= 1,
		.idi_dump	= 1,
		.nr_pipe	= 1,
		.nr_axi		= 2,
		.hw_version	= 3,
	},
	[B52ISP_DOUBLE] = {
		.idi_port	= 2,
		.idi_dump	= 1,
		.nr_pipe	= 2,
		.nr_axi		= 3,
		.hw_version	= 1,
	},
	[B52ISP_LITE] = {
		.idi_port	= 1,
		.idi_dump	= 0,
		.nr_pipe	= 1,
		.nr_axi		= 1,
		.hw_version	= 2,
	},
};

static char b52isp_block_name[][32] = {
	[B52ISP_BLK_IDI]	= "b52blk-IDI",
	[B52ISP_BLK_PIPE1]	= "b52blk-pipeline#1",
	[B52ISP_BLK_DUMP1]	= "b52blk-datadump#1",
	[B52ISP_BLK_PIPE2]	= "b52blk-pipeline#2",
	[B52ISP_BLK_DUMP2]	= "b52blk-datadump#2",
	[B52ISP_BLK_AXI1]	= "b52blk-AXI-Master#1",
	[B52ISP_BLK_AXI2]	= "b52blk-AXI-Master#2",
	[B52ISP_BLK_AXI3]	= "b52blk-AXI-Master#3",
};

static char b52isp_ispsd_name[][32] = {
	[B52ISP_ISD_IDI1]	= B52_IDI1_NAME,
	[B52ISP_ISD_IDI2]	= B52_IDI2_NAME,
	[B52ISP_ISD_PIPE1]	= B52_PATH_YUV_1_NAME,
	[B52ISP_ISD_DUMP1]	= B52_PATH_RAW_1_NAME,
	[B52ISP_ISD_MS1]	= B52_PATH_M2M_1_NAME,
	[B52ISP_ISD_PIPE2]	= B52_PATH_YUV_2_NAME,
	[B52ISP_ISD_DUMP2]	= B52_PATH_RAW_2_NAME,
	[B52ISP_ISD_MS2]	= B52_PATH_M2M_2_NAME,
	[B52ISP_ISD_HS]		= "b52isd-HighSpeed",
	[B52ISP_ISD_HDR]	= "b52isd-HDRProcess",
	[B52ISP_ISD_3D]		= "b52isd-3DStereo",
	[B52ISP_ISD_A1W1]	= B52_OUTPUT_A_NAME,
	[B52ISP_ISD_A1W2]	= B52_OUTPUT_B_NAME,
	[B52ISP_ISD_A1R1]	= B52_INPUT_A_NAME,
	[B52ISP_ISD_A2W1]	= B52_OUTPUT_C_NAME,
	[B52ISP_ISD_A2W2]	= B52_OUTPUT_D_NAME,
	[B52ISP_ISD_A2R1]	= B52_INPUT_B_NAME,
	[B52ISP_ISD_A3W1]	= B52_OUTPUT_E_NAME,
	[B52ISP_ISD_A3W2]	= B52_OUTPUT_F_NAME,
	[B52ISP_ISD_A3R1]	= B52_INPUT_C_NAME,
};

struct b52isp_cmd_mapping_item {
	enum mcu_cmd_name	cmd_name;
	__s8			input_min_buf;
	__s8			output_min_buf;
};

#define b52_scmd_item(p, n, i, o)	\
	[(p)-B52ISP_ISD_PIPE1] = {(n), (i), (o)}
/* B52ISP single pipeline command mapping table */
static struct b52isp_cmd_mapping_item b52_scmd_table[PLAT_SRC_T_CNT]
					[TYPE_3A_CNT][B52ISP_SPATH_CNT] = {
	/* online use cases */
	[PLAT_SRC_T_SNSR] = {
		[TYPE_3A_UNLOCK] = {
			b52_scmd_item(B52ISP_ISD_PIPE1,	CMD_SET_FORMAT, -1, 0),
			b52_scmd_item(B52ISP_ISD_DUMP1,	CMD_RAW_DUMP, -1, 0),
			b52_scmd_item(B52ISP_ISD_MS1,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_PIPE2,	CMD_SET_FORMAT, -1, 0),
			b52_scmd_item(B52ISP_ISD_DUMP2,	CMD_RAW_DUMP, -1, 0),
			b52_scmd_item(B52ISP_ISD_MS2,	CMD_END, -1, -1),
		},
		[TYPE_3A_LOCKED] = {
			b52_scmd_item(B52ISP_ISD_PIPE1,	CMD_IMG_CAPTURE, -1, 0),
			b52_scmd_item(B52ISP_ISD_DUMP1,	CMD_IMG_CAPTURE, -1, 0),
			b52_scmd_item(B52ISP_ISD_MS1,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_PIPE2,	CMD_IMG_CAPTURE, -1, 0),
			b52_scmd_item(B52ISP_ISD_DUMP2,	CMD_IMG_CAPTURE, -1, 0),
			b52_scmd_item(B52ISP_ISD_MS2,	CMD_END, -1, -1),
		},
	},
	/* offline use cases */
	[PLAT_SRC_T_VDEV] = {
		[TYPE_3A_UNLOCK] = {
			b52_scmd_item(B52ISP_ISD_PIPE1,	CMD_SET_FORMAT, 1, 0),
			b52_scmd_item(B52ISP_ISD_DUMP1,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_MS1,	CMD_SET_FORMAT, 1, 0),
			/* FIXME: W/R for now, should be CMD_SET_FORMAT, 1, 0 */
			b52_scmd_item(B52ISP_ISD_PIPE2,	CMD_RAW_PROCESS, 1, 1),
			b52_scmd_item(B52ISP_ISD_DUMP2,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_MS2,	CMD_SET_FORMAT, 1, 0),
		},
		[TYPE_3A_LOCKED] = {
			b52_scmd_item(B52ISP_ISD_PIPE1,	CMD_RAW_PROCESS, 1, 1),
			b52_scmd_item(B52ISP_ISD_DUMP1,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_MS1,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_PIPE2,	CMD_RAW_PROCESS, 1, 1),
			b52_scmd_item(B52ISP_ISD_DUMP2,	CMD_END, -1, -1),
			b52_scmd_item(B52ISP_ISD_MS2,	CMD_END, -1, -1),
		},
	},
};

#define b52_dcmd_item(c, n, i, o)	\
	[(c)-TYPE_3D_COMBO] = {(n), (i), (o)}
/* B52ISP double pipeline command mapping table */
static struct b52isp_cmd_mapping_item b52_dcmd_table[PLAT_SRC_T_CNT]
					[TYPE_COMBO_CNT] = {
	/* online use cases */
	[PLAT_SRC_T_SNSR] = {
		b52_dcmd_item(TYPE_3D_COMBO,	CMD_SET_FORMAT,	-1, 0),
		b52_dcmd_item(TYPE_HS_COMBO,	CMD_SET_FORMAT,	-1, 0),
		b52_dcmd_item(TYPE_HDR_COMBO,	CMD_SET_FORMAT,	-1, 0),
	},
	/* offline use cases */
	[PLAT_SRC_T_SNSR] = {
		b52_dcmd_item(TYPE_3D_COMBO,	CMD_END,	2, 0),
		b52_dcmd_item(TYPE_HS_COMBO,	CMD_SET_FORMAT,	2, 0),
		b52_dcmd_item(TYPE_HDR_COMBO,	CMD_HDR_STILL,	2, 0),
	},
};

enum {
	ISP_CLK_AXI = 0,
	ISP_CLK_CORE,
	ISP_CLK_PIPE,
	ISP_CLK_AHB,
	ISP_CLK_END
};

static const char *clock_name[ISP_CLK_END];
/* FIXME this W/R for DE limitation, since AXI clk need enable before release reset*/

static struct isp_res_req b52idi_req[] = {
	{ISP_RESRC_MEM, 0,      0},
	{ISP_RESRC_IRQ},
	{ISP_RESRC_CLK, ISP_CLK_AXI},
	{ISP_RESRC_CLK, ISP_CLK_CORE},
	{ISP_RESRC_CLK, ISP_CLK_PIPE},
	{ISP_RESRC_CLK, ISP_CLK_AHB},
	{ISP_RESRC_END}
};

static struct isp_res_req b52pipe_req[] = {
	{ISP_RESRC_MEM, 0,      0},
	{ISP_RESRC_IRQ},
/*	{ISP_RESRC_CLK, ISP_CLK_PIPE},*/
	{ISP_RESRC_CLK, ISP_CLK_AHB},
	{ISP_RESRC_END}
};

static struct isp_res_req b52axi_req[] = {
	{ISP_RESRC_MEM, 0,      0},
/*	{ISP_RESRC_CLK, ISP_CLK_AXI},*/
	{ISP_RESRC_END}
};

static void __maybe_unused dump_mac_reg(void __iomem *mac_base)
{
	char buffer[0xD0];
	int i;
	for (i = 0; i < 0xD0; i++)
		buffer[i] = readb(mac_base + i);

	d_inf(4, "dump MAC registers from %p", mac_base);
	for (i = 0; i < 0xD0; i += 8) {
		d_inf(4, "[0x%02X..0x%02X] = %02X %02X %02X %02X     %02X %02X %02X %02X", i, i + 7,
			buffer[i + 0],
			buffer[i + 1],
			buffer[i + 2],
			buffer[i + 3],
			buffer[i + 4],
			buffer[i + 5],
			buffer[i + 6],
			buffer[i + 7]);
	}
}

void b52isp_set_ddr_qos(s32 value)
{
	pm_qos_update_request(&ddrfreq_qos_req_min, value);
}
EXPORT_SYMBOL(b52isp_set_ddr_qos);

static void b52isp_ddr_threshold_work(struct work_struct *work)
{
#ifdef CONFIG_DEVFREQ_GOV_THROUGHPUT
	unsigned long val;
	u32 threshold;
	struct b52isp *b52isp = container_of(work, struct b52isp, work);

	if (b52isp->ddr_threshold_up) {
		val = CAMFREQ_POSTCHANGE_UP;
		threshold = 30;
	} else {
		threshold = 0;
		val = CAMFREQ_POSTCHANGE_DOWN;
	}

	/* Need to call DDR threshold notifier in process context */
	srcu_notifier_call_chain(&b52isp->nh, val, &threshold);
#endif
}

void b52isp_set_ddr_threshold(struct work_struct *work, int up)
{
	unsigned long irq_flags;
	static DEFINE_SPINLOCK(lock);
	struct b52isp *b52isp = container_of(work, struct b52isp, work);

	spin_lock_irqsave(&lock, irq_flags);
	if (up == b52isp->ddr_threshold_up) {
		spin_unlock_irqrestore(&lock, irq_flags);
		return;
	}
	b52isp->ddr_threshold_up = up;
	spin_unlock_irqrestore(&lock, irq_flags);

	schedule_work(&b52isp->work);
}
EXPORT_SYMBOL(b52isp_set_ddr_threshold);

static int b52isp_attach_blk_isd(struct isp_subdev *isd, struct isp_block *blk)
{
	struct isp_dev_ptr *desc = devm_kzalloc(blk->dev, sizeof(*desc),
						GFP_KERNEL);

	if (desc == NULL)
		return -ENOMEM;
	if ((blk == NULL) || (isd == NULL))
		return -EINVAL;

	desc->ptr = blk;
	desc->type = ISP_GDEV_BLOCK;
	INIT_LIST_HEAD(&desc->hook);
	list_add_tail(&desc->hook, &isd->gdev_list);
	return 0;
}

int b52isp_detach_blk_isd(struct isp_subdev *isd, struct isp_block *blk)
{
	struct isp_dev_ptr *desc;

	list_for_each_entry(desc, &isd->gdev_list, hook) {
		if (blk == desc->ptr)
			goto find;
	}
	return -ENODEV;

find:
	list_del(&desc->hook);
	devm_kfree(blk->dev, desc);
	return 0;
}

static inline int b52isp_mfmt_to_pfmt(struct v4l2_pix_format *pf,
	struct v4l2_mbus_framefmt mf)
{
	pf->width = mf.width;
	pf->height = mf.height;

	switch (mf.code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		pf->pixelformat = V4L2_PIX_FMT_SBGGR8;
		pf->bytesperline = pf->width;
		break;
	case V4L2_MBUS_FMT_SGBRG8_1X8:
		pf->pixelformat = V4L2_PIX_FMT_SGBRG8;
		pf->bytesperline = pf->width;
		break;
	case V4L2_MBUS_FMT_SGRBG8_1X8:
		pf->pixelformat = V4L2_PIX_FMT_SGRBG8;
		pf->bytesperline = pf->width;
		break;
	case V4L2_MBUS_FMT_SRGGB8_1X8:
		pf->pixelformat = V4L2_PIX_FMT_SRGGB8;
		pf->bytesperline = pf->width;
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		pf->pixelformat = V4L2_PIX_FMT_SBGGR10;
		pf->bytesperline = pf->width * 10 / 8;
		break;
	case V4L2_MBUS_FMT_SGBRG10_1X10:
		pf->pixelformat = V4L2_PIX_FMT_SGBRG10;
		pf->bytesperline = pf->width * 10 / 8;
		break;
	case V4L2_MBUS_FMT_SGRBG10_1X10:
		pf->pixelformat = V4L2_PIX_FMT_SGRBG10;
		pf->bytesperline = pf->width * 10 / 8;
		break;
	case V4L2_MBUS_FMT_SRGGB10_1X10:
		pf->pixelformat = V4L2_PIX_FMT_SRGGB10;
		pf->bytesperline = pf->width * 10 / 8;
		break;
	default:
		pr_err("%s: not supported mbus code\n", __func__);
		return -EINVAL;
	}

	pf->sizeimage = pf->bytesperline * mf.height;
	pf->field = 0;
	pf->colorspace = mf.colorspace;

	return 0;
}

static inline int b52isp_try_apply_cmd(struct b52isp_lpipe *pipe)
{
	struct b52isp_cmd *cmd;
	int i, ret = 0;
	unsigned long output_sel;

	if ((pipe == NULL) || (pipe->cur_cmd == NULL))
		return -EINVAL;
	cmd = pipe->cur_cmd;

	/*
	 * Convert output_map, which is logical, to output_sel, which is
	 * physical. This is because MCU FW only know physical output ports.
	 */
	output_sel = 0;
	for (i = 0; i < pipe->isd.subdev.entity.num_links; i++) {
		struct media_link *link = &pipe->isd.subdev.entity.links[i];
		struct isp_subdev *isd;
		struct b52isp_laxi *laxi;
		int bit;

		if (link->source != pipe->isd.pads + B52PAD_PIPE_OUT)
			continue;
		if ((link->flags & MEDIA_LNK_FL_ENABLED) == 0)
			continue;

		/* Find linked AXI */
		isd = v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(link->sink->entity));
		if (WARN_ON(isd == NULL))
			return -EPIPE;
		laxi = isd->drv_priv;
		if ((laxi->port < B52AXI_PORT_W1) ||
			(laxi->port > B52AXI_PORT_W2))
			continue;
		bit = laxi->mac * 2 + laxi->port;
		if (test_and_set_bit(bit, &output_sel))
			BUG_ON(1);
	}

	/* handle IMG_CAPTURE and HDR_STILL case */
	if (cmd->src_type == CMD_SRC_AXI) {
		struct isp_vnode *vnode = cmd->mem.vnode;
		/* FIXME: maybe need check laxi->stream here? */
		if ((vnode->format.type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ||
			(vnode->format.fmt.pix_mp.num_planes != 1)) {
			ret = -EINVAL;
			goto err_exit;
		}
		cmd->src_fmt.width = vnode->format.fmt.pix_mp.width;
		cmd->src_fmt.height = vnode->format.fmt.pix_mp.height;
		cmd->src_fmt.pixelformat = vnode->format.fmt.pix_mp.pixelformat;
		cmd->src_fmt.field = vnode->format.fmt.pix_mp.field;
		cmd->src_fmt.colorspace = vnode->format.fmt.pix_mp.colorspace;
		cmd->src_fmt.bytesperline
			= vnode->format.fmt.pix_mp.plane_fmt[0].bytesperline;
		cmd->src_fmt.sizeimage
			= vnode->format.fmt.pix_mp.plane_fmt[0].sizeimage;
	} else {
		struct v4l2_subdev *sensor = cmd->sensor;
		struct v4l2_subdev_format fmt = {
			.pad = 0,
			.which = V4L2_SUBDEV_FORMAT_ACTIVE,
		};
		ret = v4l2_subdev_call(sensor, pad, get_fmt, NULL, &fmt);
		if (ret < 0)
			goto err_exit;
		d_inf(4, "got sensor %s <w%d, h%d, c%X>",
			sensor->name, fmt.format.width,
			fmt.format.height, fmt.format.code);

		ret = b52isp_mfmt_to_pfmt(&cmd->src_fmt, fmt.format);
		if (ret < 0)
			goto err_exit;
	}

	if (cmd->cmd_name == CMD_SET_FORMAT ||
		cmd->cmd_name == CMD_CHG_FORMAT) {
		/* FIXME: if all enable port close, will also clean port sel to ISP*/
		if (cmd->enable_map == 0x0)
			output_sel = 0x0;
		if (output_sel == pipe->output_sel) {
			if (cmd->enable_map == pipe->enable_map)
				goto cmd_done;
			else {
				cmd->cmd_name = CMD_CHG_FORMAT;
				d_inf(4, "use CHANGE_FORMAT instead");
			}
		} else {
			cmd->cmd_name = CMD_SET_FORMAT;
		}
	}
	/* MCU command only recognize output select */
	cmd->output_map = output_sel;

	ret = b52_hdl_cmd(cmd);
	if (ret < 0) {
		d_inf(1, "MCU command apply failed: %d", ret);
		goto err_exit;
	}

cmd_done:
	pipe->output_sel = output_sel;
	pipe->enable_map = cmd->enable_map;

	/* FIXME set flags to 0, keep the sensor init once */
	cmd->flags &= ~BIT(CMD_FLAG_INIT);

	d_inf(4, "MCU command apply success: %s", pipe->isd.subdev.name);
	return 0;

err_exit:
	return ret;
}

/********************************* IDI block *********************************/
static int b52isp_idi_hw_open(struct isp_block *block)
{
	return 0;
}

static void b52isp_idi_hw_close(struct isp_block *block)
{
}
static int b52isp_idi_set_power(struct isp_block *block, int level)
{
	int ret = 0;

	if (level)
		ret = pm_runtime_get_sync(block->dev);
	else {
		b52isp_set_ddr_qos(PM_QOS_DEFAULT_VALUE);
		ret = pm_runtime_put(block->dev);
	}

	return ret;
}

static int b52isp_idi_set_clock(struct isp_block *block, int rate)
{
	struct clk *axi_clk = block->clock[0];
	struct clk *core_clk = block->clock[1];
	struct clk *pipe_clk = block->clock[2];

	if (rate) {
		clk_set_rate(axi_clk, 312000000);
		clk_set_rate(core_clk, 156000000);
		clk_set_rate(pipe_clk, 312000000);
	}

	return 0;
}

struct isp_block_ops b52isp_idi_hw_ops = {
	.open	= b52isp_idi_hw_open,
	.close	= b52isp_idi_hw_close,
	.set_power	= b52isp_idi_set_power,
	.set_clock  = b52isp_idi_set_clock,
};

int b52isp_idi_change_clock(struct isp_block *block,
				int w, int h, int fps)
{
	int sz = w * h * fps;
	int rate;
	struct clk *axi_clk = block->clock[0];
	struct clk *pipe_clk = block->clock[2];

	/* Need to refine the frequency */
	if (sz > 300000000)
		rate = 416000000;
	else if (sz > 180000000)
		rate = 312000000;
	else if (sz > 100000000)
		rate = 208000000;
	else
		rate = 156000000;

	clk_set_rate(axi_clk, rate);
	if (sz > 300000000)
		rate = 499000000;
	else if (sz > 180000000)
		rate = 312000000;
	else if (sz > 100000000)
		rate = 208000000;
	else
		rate = 156000000;
	clk_set_rate(pipe_clk, rate);
	b52_set_sccb_clock_rate(clk_get_rate(pipe_clk), 400000);

	d_inf(3, "isp axi clk %lu", clk_get_rate(axi_clk));
	d_inf(3, "isp pipe clk %lu", clk_get_rate(pipe_clk));

	return 0;
}
EXPORT_SYMBOL(b52isp_idi_change_clock);

/********************************* IDI subdev *********************************/
/* ioctl(subdev, IOCTL_XXX, arg) is handled by this one */
static long b52isp_idi_ioctl(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	return 0;
}

static const struct v4l2_subdev_core_ops b52isp_idi_core_ops = {
	.ioctl	= b52isp_idi_ioctl,
};

static int b52isp_idi_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	int ret = 0;
	return ret;
}

static int b52isp_idi_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int ret = 0;
	return ret;
}

static int b52isp_idi_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	return ret;
}

static int b52isp_idi_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	return ret;
}

static int b52isp_idi_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;
	sel->r = *rect;
	return 0;
}

static int b52isp_idi_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		goto exit;
	/* Really apply here if not using MCU */
	d_inf(4, "%s:pad[%d] crop(%d, %d)<>(%d, %d)", sd->name, sel->pad,
		sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	*rect = sel->r;
exit:
	return ret;
}

static const struct v4l2_subdev_pad_ops b52isp_idi_pad_ops = {
	.enum_mbus_code		= b52isp_idi_enum_mbus_code,
	.enum_frame_size	= b52isp_idi_enum_frame_size,
	.get_fmt		= b52isp_idi_get_format,
	.set_fmt		= b52isp_idi_set_format,
	.get_selection		= b52isp_idi_get_selection,
	.set_selection		= b52isp_idi_set_selection,
};

static const struct v4l2_subdev_ops b52isp_idi_subdev_ops = {
	.core	= &b52isp_idi_core_ops,
	.pad	= &b52isp_idi_pad_ops,
};

static int b52isp_idi_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations b52isp_idi_media_ops = {
	.link_setup = b52isp_idi_link_setup,
};

static int b52isp_idi_sd_open(struct isp_subdev *ispsd)
{
	return 0;
}

static void b52isp_idi_sd_close(struct isp_subdev *ispsd)
{
}

struct isp_subdev_ops b52isp_idi_sd_ops = {
	.open		= b52isp_idi_sd_open,
	.close		= b52isp_idi_sd_close,
};

static int b52isp_pwr_enable;
static int b52isp_idi_node_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct isp_block *blk = isp_sd2blk(isd);
	int ret;

	if (pm_runtime_suspended(blk->dev))
		b52isp_pwr_enable = 0;
	ret = isp_block_tune_power(blk, 1);
	b52_set_base_addr(blk->reg_base);
	return ret;
}

static int b52isp_idi_node_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct isp_block *blk = isp_sd2blk(isd);
	int ret;
	ret = isp_block_tune_power(blk, 0);
	b52_set_base_addr(blk->reg_base);
	b52isp_ctrl_reset_bp_val();
	return ret;
}

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops b52isp_idi_node_ops = {
	.open	= b52isp_idi_node_open,
	.close	= b52isp_idi_node_close,
};

static void b52isp_idi_remove(struct b52isp *b52isp)
{
	struct isp_block *blk = b52isp->blk[B52ISP_BLK_IDI];
	struct b52isp_idi *idi;
	int i;

	if (blk == NULL)
		return;
	idi = container_of(blk, struct b52isp_idi, block);

	for (i = 0; i < b52isp->hw_desc->idi_port; i++) {
		struct isp_subdev *isd = b52isp->isd[B52ISP_ISD_IDI1 + i];
		b52isp->isd[B52ISP_ISD_IDI1 + i] = NULL;
		b52isp_detach_blk_isd(isd, blk);
		plat_ispsd_unregister(isd);
		v4l2_ctrl_handler_free(&isd->ctrl_handler);
		devm_kfree(b52isp->dev, isd);
	}
	b52isp->blk[B52ISP_BLK_IDI] = NULL;
	devm_kfree(b52isp->dev, idi);
}

static int b52isp_idi_create(struct b52isp *b52isp)
{
	struct b52isp_idi *idi = devm_kzalloc(b52isp->dev, sizeof(*idi),
						GFP_KERNEL);
	struct isp_block *block;
	struct isp_subdev *ispsd;
	struct v4l2_subdev *sd;
	int ret, i;

	if (idi == NULL)
		return -ENOMEM;

	/* Add ISP Path Blocks */
	block = &idi->block;
	block->id.dev_type = PCAM_IP_B52ISP;
	block->id.dev_id = b52isp->dev->id;
	block->id.mod_id = B52ISP_BLK_IDI;
	block->dev = b52isp->dev;
	block->name = b52isp_block_name[B52ISP_BLK_IDI];
	block->req_list = b52idi_req;
	block->ops = &b52isp_idi_hw_ops;
	b52isp->blk[B52ISP_BLK_IDI] = block;
	idi->parent = b52isp;

	for (i = 0; i < b52isp->hw_desc->idi_port; i++) {
		ispsd = devm_kzalloc(b52isp->dev, sizeof(*ispsd), GFP_KERNEL);
		if (ispsd == NULL)
			return -ENOMEM;
		sd = &ispsd->subdev;
		ret = v4l2_ctrl_handler_init(&ispsd->ctrl_handler, 0);
		if (unlikely(ret < 0))
			return ret;
		sd->entity.ops = &b52isp_idi_media_ops;
		v4l2_subdev_init(sd, &b52isp_idi_subdev_ops);
		sd->internal_ops = &b52isp_idi_node_ops;
		sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		sd->ctrl_handler = &ispsd->ctrl_handler;
		strncpy(sd->name, b52isp_ispsd_name[B52ISP_ISD_IDI1 + i],
			sizeof(sd->name));
		ispsd->ops = &b52isp_idi_sd_ops;
		ispsd->drv_priv = idi;
		ispsd->pads[B52PAD_IDI_IN].flags = MEDIA_PAD_FL_SINK;
		ispsd->pads[B52PAD_IDI_PIPE1].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads[B52PAD_IDI_DUMP1].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads[B52PAD_IDI_PIPE2].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads[B52PAD_IDI_DUMP2].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads[B52PAD_IDI_BOTH].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads_cnt = B52_IDI_PAD_CNT;
		ispsd->single = 1;
		INIT_LIST_HEAD(&ispsd->gdev_list);
		ispsd->sd_code = SDCODE_B52ISP_IDI1 + i;

		ret = b52isp_attach_blk_isd(ispsd, block);
		if (ret < 0)
			return ret;
		ret = plat_ispsd_register(ispsd);
		if (ret < 0)
			return ret;
		b52isp->isd[B52ISP_ISD_IDI1 + i] = ispsd;
	}
	return 0;
}



/******************************* ISP Path Block *******************************/
static int b52isp_path_hw_open(struct isp_block *block)
{
	int ret;
	struct b52isp_ppipe *ppipe = NULL;
	int hw_version;
	if (pm_runtime_suspended(block->dev)) {
		d_inf(1, "ISP haven't been power on, it can't load firmware\n");
		return 0;
	} else if (b52isp_pwr_enable == 1)
		goto fw_done;
	else
		b52isp_pwr_enable = 1;
	ppipe = container_of(block,
			struct b52isp_ppipe, block);
	hw_version = ppipe->parent->hw_desc->hw_version;
	ret = b52_load_fw(block->dev, block->reg_base, 1,
					b52isp_pwr_enable, hw_version);
	if (ret < 0)
		return ret;

fw_done:
	d_inf(2, "MCU Initialization done");
	return 0;
}

int check_load_firmware(void)
{
	return b52isp_pwr_enable;
}
EXPORT_SYMBOL_GPL(check_load_firmware);

static void b52isp_path_hw_close(struct isp_block *block)
{
	b52_load_fw(block->dev, block->reg_base, 0, b52isp_pwr_enable, 0);
}

static int b52isp_path_s_clock(struct isp_block *block, int rate)
{
	return 0;
}

static int b52isp_path_hw_s_power(struct isp_block *block, int level)
{
	int ret = 0;

	if (level)
		ret = pm_runtime_get_sync(block->dev);
	else
		ret = pm_runtime_put(block->dev);

	return ret;
}

struct isp_block_ops b52isp_path_hw_ops = {
	.open	= b52isp_path_hw_open,
	.close	= b52isp_path_hw_close,
	.set_power	= b52isp_path_hw_s_power,
	.set_clock  = b52isp_path_s_clock,
};

/****************************** ISP Path Subdev ******************************/
static int b52isp_path_set_profile(struct isp_subdev *isd)
{
	struct media_pad *r_pad = media_entity_remote_pad(
					isd->pads + B52PAD_PIPE_IN);
	struct b52isp_lpipe *pipe = isd->drv_priv;
	struct b52isp_cmd *cur_cmd;
	struct isp_subdev *src;
	int ret = 0;

	WARN_ON(list_empty(&isd->gdev_list));
	mutex_lock(&pipe->state_lock);

	if (pipe->cur_cmd) {
		/* If already has a snapshot, clean the old command */
		devm_kfree(isd->build->dev, pipe->cur_cmd);
		pipe->cur_cmd = NULL;
	}

	pipe->cur_cmd = devm_kzalloc(isd->build->dev,
					sizeof(*pipe->cur_cmd), GFP_KERNEL);
	if (pipe->cur_cmd == NULL) {
		ret = -ENOMEM;
		goto exit;
	}
	cur_cmd = pipe->cur_cmd;

	INIT_LIST_HEAD(&pipe->cur_cmd->hook);

	if (unlikely(WARN_ON(r_pad == NULL))) {
		ret = -EPERM;
		goto exit;
	}

	/* get pre-scaler */
	src = v4l2_get_subdev_hostdata(
		media_entity_to_v4l2_subdev(r_pad->entity));
	if (unlikely(WARN_ON(src == NULL))) {
		ret = -EPIPE;
		goto exit;
	}

	/* setup source and pre-scaler */
	switch (src->sd_code) {
	case SDCODE_B52ISP_IDI1:
	case SDCODE_B52ISP_IDI2:
		cur_cmd->src_type = CMD_SRC_SNSR;
		cur_cmd->pre_crop = src->crop_pad[r_pad->index];
		r_pad = media_entity_remote_pad(src->pads + B52PAD_IDI_IN);
		if (unlikely(WARN_ON(r_pad == NULL))) {
			ret = -EPIPE;
			goto exit;
		}
		/* get sensor */
		r_pad = media_entity_remote_pad(&r_pad->entity->pads[0]);
		if (unlikely(WARN_ON(r_pad == NULL))) {
			ret = -EPIPE;
			goto exit;
		}
		if (unlikely(WARN_ON(media_entity_type(r_pad->entity)
					!= MEDIA_ENT_T_V4L2_SUBDEV))) {
			ret = -EPIPE;
			goto exit;
		}
		cur_cmd->sensor =
			media_entity_to_v4l2_subdev(r_pad->entity);
		d_inf(4, "sensor is %s", cur_cmd->sensor->name);
		break;

	case SDCODE_B52ISP_A1R1:
	case SDCODE_B52ISP_A2R1:
	case SDCODE_B52ISP_A3R1:
		cur_cmd->src_type = CMD_SRC_AXI;
		cur_cmd->pre_crop = src->crop_pad[B52PAD_AXI_OUT];
		break;
	}

	/* setup pipeline */
	switch (pipe->isd.sd_code) {
	case SDCODE_B52ISP_PIPE1:
		cur_cmd->path = B52ISP_ISD_PIPE1;
		break;
	case SDCODE_B52ISP_DUMP1:
		cur_cmd->path = B52ISP_ISD_DUMP1;
		break;
	case SDCODE_B52ISP_MS1:
		pipe->cur_cmd->path = B52ISP_ISD_MS1;
		break;
	case SDCODE_B52ISP_PIPE2:
		cur_cmd->path = B52ISP_ISD_PIPE2;
		break;
	case SDCODE_B52ISP_DUMP2:
		cur_cmd->path = B52ISP_ISD_DUMP1;
		break;
	case SDCODE_B52ISP_MS2:
		pipe->cur_cmd->path = B52ISP_ISD_MS2;
		break;
	case SDCODE_B52ISP_HS:
		cur_cmd->path = B52ISP_ISD_HS;
		break;
	case SDCODE_B52ISP_HDR:
		cur_cmd->path = B52ISP_ISD_HDR;
		break;
	case SDCODE_B52ISP_3D:
		cur_cmd->path = B52ISP_ISD_3D;
		break;
	default:
		d_inf(1, "path id <%d> not recongnized", pipe->isd.sd_code);
		ret = -ENODEV;
		goto exit;
	}

	/* setup command name */
	switch (isd->sd_code) {
	case SDCODE_B52ISP_PIPE1:
	case SDCODE_B52ISP_PIPE2:
		switch (src->sd_code) {
		case SDCODE_B52ISP_IDI1:
		case SDCODE_B52ISP_IDI2:
			cur_cmd->cmd_name = CMD_SET_FORMAT;
			cur_cmd->flags = BIT(CMD_FLAG_INIT);
			if (pipe->meta_dma) {
				cur_cmd->meta_dma = pipe->meta_dma;
				cur_cmd->flags |= BIT(CMD_FLAG_META_DATA);
			}
			break;
		case SDCODE_B52ISP_A1R1:
		case SDCODE_B52ISP_A2R1:
		case SDCODE_B52ISP_A3R1:
			cur_cmd->cmd_name = CMD_RAW_PROCESS;
			break;
		}
		break;
	case SDCODE_B52ISP_DUMP1:
	case SDCODE_B52ISP_DUMP2:
		if (src->sd_code != SDCODE_B52ISP_IDI1 &&
			src->sd_code != SDCODE_B52ISP_IDI2) {
			ret = -EINVAL;
			goto exit;
		}
		cur_cmd->cmd_name = CMD_RAW_DUMP;
		/* TODO: set sensor ID here */
		break;
	case SDCODE_B52ISP_MS1:
	case SDCODE_B52ISP_MS2:
		switch (src->sd_code) {
		case SDCODE_B52ISP_A1R1:
		case SDCODE_B52ISP_A2R1:
		case SDCODE_B52ISP_A3R1:
			pipe->cur_cmd->cmd_name = CMD_SET_FORMAT;
			pipe->cur_cmd->flags |= BIT(CMD_FLAG_MS);
			if (pipe->meta_dma) {
				cur_cmd->meta_dma = pipe->meta_dma;
				cur_cmd->flags |= BIT(CMD_FLAG_META_DATA);
			}
			break;
		default:
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SDCODE_B52ISP_HS:
		cur_cmd->cmd_name = CMD_SET_FORMAT;
		break;
	case SDCODE_B52ISP_3D:
		cur_cmd->cmd_name = CMD_SET_FORMAT;
		break;
	case SDCODE_B52ISP_HDR:
		switch (src->sd_code) {
		case SDCODE_B52ISP_IDI1:
		case SDCODE_B52ISP_IDI2:
			cur_cmd->cmd_name = CMD_SET_FORMAT;
			break;
		case SDCODE_B52ISP_A1R1:
		case SDCODE_B52ISP_A2R1:
		case SDCODE_B52ISP_A3R1:
			cur_cmd->cmd_name = CMD_HDR_STILL;
			break;
		}
		break;
	}

	cur_cmd->post_crop = isd->crop_pad[B52PAD_PIPE_OUT];

exit:
	mutex_unlock(&pipe->state_lock);
	return ret;
}

static int b52isp_config_af_win(struct isp_subdev *isd,
		struct v4l2_rect *r)
{
	int id;
	struct b52isp_lpipe *pipe = isd->drv_priv;
	struct b52isp_ctrls *ctrls = &pipe->ctrls;
	struct isp_block *blk = isp_sd2blk(isd);

	if (!r || !ctrls || !blk) {
		pr_err("%s: paramter is NULL\n", __func__);
		return -EINVAL;
	}

	ctrls->af_win = *r;

	switch (blk->id.mod_id) {
	case B52ISP_BLK_PIPE1:
		id = 0;
		break;
	case B52ISP_BLK_PIPE2:
		id = 1;
		break;
	default:
		pr_err("%s: id error\n", __func__);
		return -EINVAL;
	}

	b52_set_focus_win(r, id);
	return 0;
}

static int b52isp_config_metering_mode(struct isp_subdev *isd,
		struct b52isp_expo_metering *metering)
{
	struct b52isp_lpipe *pipe = isd->drv_priv;
	struct b52isp_ctrls *ctrls = &pipe->ctrls;

	if (!metering || !ctrls) {
		pr_err("%s: paramter is NULL\n", __func__);
		return -EINVAL;
	}
	if (metering->mode >= NR_METERING_MODE) {
		pr_err("%s: mode(%d) is error\n", __func__, metering->mode);
		return -EINVAL;
	}

	ctrls->metering_mode[metering->mode] = *metering;

	return 0;
}

static int b52isp_config_metering_roi(struct isp_subdev *isd,
		struct b52isp_win *r)
{
	struct b52isp_lpipe *pipe = isd->drv_priv;
	struct b52isp_ctrls *ctrls = &pipe->ctrls;

	if (!r || !ctrls) {
		pr_err("%s: paramter is NULL\n", __func__);
		return -EINVAL;
	}

	ctrls->metering_roi = *r;

	return 0;
}

static int b52isp_path_rw_ctdata(struct isp_subdev *isd, int write,
					struct b52_data_node *node)
{
	struct isp_block *blk = isp_sd2blk(isd);
	int ret;
	void    *tmpbuf = NULL;

	if (WARN_ON(node == NULL) ||
		WARN_ON(node->size == 0))
		return -EINVAL;

	/*
	 * TODO: actually the right thing to do here is scan for each physical
	 * device, and apply C&T data accordingly. This is important for:
	 * HDR:		P1 = P2 = sensor
	 * High speed:	P1 + P2 = sensor
	 * 3D stereo:	P1 = sensorL, P2 = sensorR
	 */

	tmpbuf = devm_kzalloc(isd->build->dev, node->size, GFP_KERNEL);
	if (tmpbuf == NULL) {
		d_inf(1, "%s not enough memery, try later",
			isd->subdev.name);
		return -EAGAIN;
	}
	if (copy_from_user(tmpbuf, node->buffer, node->size)) {
		ret = -EAGAIN;
		goto err;
	}

	if (blk == NULL) {
		d_inf(1, "%s not mapped to physical device yet, try later",
			isd->subdev.name);
		ret = -EAGAIN;
		goto err;
	}

	switch (blk->id.mod_id) {
	case B52ISP_BLK_PIPE1:
		ret = b52_rw_pipe_ctdata(1, write, tmpbuf,
					node->size / sizeof(struct b52_regval));
		break;
	case B52ISP_BLK_PIPE2:
		ret = b52_rw_pipe_ctdata(2, write, tmpbuf,
					node->size / sizeof(struct b52_regval));
		break;
	default:
		d_inf(1, "download C&T data not allowed on \"%s\"",
			isd->subdev.name);
		ret = -EACCES;
		goto err;
	}
	if (copy_to_user(node->buffer, tmpbuf, node->size))
		ret = -EAGAIN;
err:
	devm_kfree(isd->build->dev, tmpbuf);
	return ret;
}
static int b52isp_config_awb_gain(struct isp_subdev *isd,
					struct b52isp_awb_gain *awb_gain)
{
	int id = 0;
	struct isp_block *blk = isp_sd2blk(isd);

	if (!blk || !awb_gain) {
		pr_err("%s: paramter is NULL\n", __func__);
		return -EINVAL;
	}

	if (blk->id.mod_id == B52ISP_BLK_PIPE1)
		id = 0;
	else if (blk->id.mod_id == B52ISP_BLK_PIPE2)
		id = 1;
	else {
		pr_err("%s: wrong mod id\n", __func__);
		return -EINVAL;
	}

	return b52isp_rw_awb_gain(awb_gain, id);
}

static int b52isp_config_memory_sensor(struct isp_subdev *isd,
					struct memory_sensor *arg)
{
	int ret = 0;
	struct b52isp_lpipe *pipe = isd->drv_priv;
	pipe->cur_cmd->memory_sensor_data = memory_sensor_match(arg->name);
	if (pipe->cur_cmd->memory_sensor_data == NULL) {
		pr_err("memory sensor can't match the real sensor\n");
		ret = -EINVAL;
	}
	return ret;
}

static int b52isp_config_adv_dns(struct isp_subdev *isd,
		struct b52isp_adv_dns *dns)
{
	int ret = 0;
	struct b52isp_lpipe *pipe = isd->drv_priv;

	if (dns->type >= ADV_DNS_MAX) {
		pr_err("Not have such type: %d\n", dns->type);
		ret = -EINVAL;
	}

	pipe->cur_cmd->adv_dns.type = dns->type;
	pipe->cur_cmd->adv_dns.Y_times = dns->times;
	pipe->cur_cmd->adv_dns.UV_times = dns->times;
	return ret;
}

static int b52isp_set_path_arg(struct isp_subdev *isd,
		struct b52isp_path_arg *arg)
{
	struct b52isp_lpipe *lpipe = isd->drv_priv;

	lpipe->path_arg = *arg;
	d_inf(3, "%s: 3A_lock:%d, nr_bracket:%d, expo_2/1:0x%X, expo_3/1:0x%X, linear_yuv:%d",
		isd->subdev.name, arg->aeag, arg->nr_frame,
		arg->ratio_1_2, arg->ratio_1_3, arg->linear_yuv);
	return 0;
}

static int b52isp_anti_shake(struct isp_subdev *isd,
		struct b52isp_anti_shake_arg *arg)
{
	return b52_cmd_anti_shake(arg->block_size, arg->enable);
}

static int b52isp_brightness(struct isp_subdev *isd,
	struct b52isp_brightness *arg)
{
	switch (arg->op) {
	case TYPE_READ:
		arg->ygain_h = b52_readb(0x65b07);
		arg->ygain_l = b52_readb(0x65b08);
		arg->yoffset_h = b52_readb(0x65b11);
		arg->yoffset_l = b52_readb(0x65b12);
		arg->uv_matrix_00_h = b52_readb(0x65b09);
		arg->uv_matrix_00_l = b52_readb(0x65b0a);
		arg->uv_matrix_01_h = b52_readb(0x65b0b);
		arg->uv_matrix_01_l = b52_readb(0x65b0c);
		arg->uv_matrix_10_h = b52_readb(0x65b0d);
		arg->uv_matrix_10_l = b52_readb(0x65b0e);
		arg->uv_matrix_11_h = b52_readb(0x65b0f);
		arg->uv_matrix_11_l = b52_readb(0x65b10);
		arg->hgain = b52_readb(0x65b19);
		break;
	case TYPE_WRITE:
		b52_writeb(0x65b07, arg->ygain_h);
		b52_writeb(0x65b08, arg->ygain_l);
		b52_writeb(0x65b09, arg->uv_matrix_00_h);
		b52_writeb(0x65b0a, arg->uv_matrix_00_l);
		b52_writeb(0x65b0b, arg->uv_matrix_01_h);
		b52_writeb(0x65b0c, arg->uv_matrix_01_l);
		b52_writeb(0x65b0d, arg->uv_matrix_10_h);
		b52_writeb(0x65b0e, arg->uv_matrix_10_l);
		b52_writeb(0x65b0f, arg->uv_matrix_11_h);
		b52_writeb(0x65b10, arg->uv_matrix_11_l);
		b52_writeb(0x65b19, arg->hgain);
		break;
	}
	return 0;
}

static int b52isp_saturation(struct isp_subdev *isd,
	struct b52isp_saturation *arg)
{
	u32 base;
	int i = 0;
	struct isp_block *blk = isp_sd2blk(isd);

	if (!arg)
		return 0;

	if (blk->id.mod_id == B52ISP_BLK_PIPE1)
		base = FW_P1_REG_BASE;
	else if (blk->id.mod_id == B52ISP_BLK_PIPE2) {
		base = FW_P2_REG_BASE;
		pr_err("Current ISP FW can't support this on p2\n");
		return -EINVAL;
	}

	/* 0x333f3~0x333f8 */
	while (REG_FW_UV_MIN_SATURATON + i <= REG_FW_UV_MAX_SATURATON) {
		b52_writeb(base + REG_FW_UV_MIN_SATURATON + i, arg->sat_arr[i]);
		i++;
	}

	return 0;
}

static int b52isp_sharpness(struct isp_subdev *isd,
	struct b52isp_sharpness *arg)
{
	u32 base;
	int i = 0;
	struct isp_block *blk = isp_sd2blk(isd);

	if (!arg)
		return 0;

	if (blk->id.mod_id == B52ISP_BLK_PIPE1)
		base = FW_P1_REG_BASE;
	else if (blk->id.mod_id == B52ISP_BLK_PIPE2) {
		base = FW_P2_REG_BASE;
		pr_err("Current ISP FW can't support this on p2\n");
		return -EINVAL;
	}

	/* 0x331ea~0x331ef */
	while (REG_FW_MIN_SHARPNESS + i <= REG_FW_MAX_SHARPNESS) {
		b52_writeb(FW_P1_REG_BASE + REG_FW_MIN_SHARPNESS + i, arg->sha_arr[i]);
		i++;
	}

	return 0;
}

/* ioctl(subdev, IOCTL_XXX, arg) is handled by this one */
static long b52isp_path_ioctl(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	int ret = 0;
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);

	switch (cmd) {
	case VIDIOC_PRIVATE_B52ISP_TOPOLOGY_SNAPSHOT:
		ret = b52isp_path_set_profile(isd);
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_AF_WINDONW:
		ret = b52isp_config_af_win(isd, (struct v4l2_rect *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_EXPO_METERING_MODE:
		ret = b52isp_config_metering_mode(isd,
				(struct b52isp_expo_metering *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_EXPO_METERING_ROI:
		ret = b52isp_config_metering_roi(isd,
				(struct b52isp_win *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_DOWNLOAD_CTDATA:
		ret = b52isp_path_rw_ctdata(isd, 1,
				(struct b52_data_node *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_UPLOAD_CTDATA:
		ret = b52isp_path_rw_ctdata(isd, 0,
				(struct b52_data_node *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_AWB_GAIN:
		ret = b52isp_config_awb_gain(isd,
				(struct b52isp_awb_gain *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_MEMORY_SENSOR:
		ret = b52isp_config_memory_sensor(isd,
				(struct memory_sensor *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_ADV_DNS:
		ret = b52isp_config_adv_dns(isd,
				(struct b52isp_adv_dns *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_SET_PATH_ARG:
		ret = b52isp_set_path_arg(isd, (struct b52isp_path_arg *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_ANTI_SHAKE:
		ret = b52isp_anti_shake(isd, (struct b52isp_anti_shake_arg *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_BRIGHTNESS:
		ret = b52isp_brightness(isd, (struct b52isp_brightness *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_SATURATION:
		ret = b52isp_saturation(isd, (struct b52isp_saturation *)arg);
		break;
	case VIDIOC_PRIVATE_B52ISP_SHARPNESS:
		ret = b52isp_sharpness(isd, (struct b52isp_sharpness *)arg);
		break;
	default:
		d_inf(1, "unknown ioctl '%c', dir=%d, #%d (0x%08x)\n",
			_IOC_TYPE(cmd), _IOC_DIR(cmd), _IOC_NR(cmd), cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
/*FIXME: need to refine return val*/
static int b52_usercopy(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	char	sbuf[128];
	void    *mbuf = NULL;
	void	*parg = arg;
	long	err  = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	if (_IOC_DIR(cmd) != _IOC_NONE) {
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			unsigned int n = _IOC_SIZE(cmd);

			if (copy_from_user(parg, (void __user *)arg, n))
				goto out;

			/* zero out anything we don't copy from userspace */
			if (n < _IOC_SIZE(cmd))
				memset((u8 *)parg + n, 0, _IOC_SIZE(cmd) - n);
		} else {
			/* read-only ioctl */
			memset(parg, 0, _IOC_SIZE(cmd));
		}
	}

	/* Handles IOCTL */
	err = v4l2_subdev_call(sd, core, ioctl, cmd, parg);
	if (err == -ENOIOCTLCMD)
		err = -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ) {
		unsigned int n = _IOC_SIZE(cmd);
		if (copy_to_user((void __user *)arg, parg, n))
			goto out;
	}
out:
	kfree(mbuf);
	return err;
}

struct b52_data_node32 {
	__u32	size;
	compat_caddr_t buffer;
};

struct b52isp_profile32 {
	unsigned int profile_id;
	compat_caddr_t arg;
};

#define VIDIOC_PRIVATE_B52ISP_TOPOLOGY_SNAPSHOT32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 0, struct b52isp_profile32)
#define VIDIOC_PRIVATE_B52ISP_DOWNLOAD_CTDATA32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct b52_data_node32)
#define VIDIOC_PRIVATE_B52ISP_UPLOAD_CTDATA32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct b52_data_node32)

static int get_b52isp_profile32(struct b52isp_profile *kp,
		struct b52isp_profile32 __user *up)
{
	u32 tmp;
	if (!access_ok(VERIFY_READ, up, sizeof(struct b52isp_profile32)) ||
			get_user(tmp, &up->arg) ||
			get_user(kp->profile_id, &up->profile_id))
		return -EFAULT;

	kp->arg = compat_ptr(tmp);

	return 0;
}

static int get_rw_ctdata32(struct b52_data_node *kp,
		struct b52_data_node32 __user *up)
{
	u32 tmp;
	if (!access_ok(VERIFY_READ, up, sizeof(struct b52_data_node32)) ||
			get_user(tmp, &up->buffer) ||
			get_user(kp->size, &up->size))
		return -EFAULT;

	kp->buffer = compat_ptr(tmp);

	return 0;
}

static long b52isp_compat_ioctl32(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	int ret = 0;
	union {
		struct b52isp_profile pf;
		struct b52_data_node nd;
	} karg;
	int compatible_arg = 1;

	switch (cmd) {
	case VIDIOC_PRIVATE_B52ISP_TOPOLOGY_SNAPSHOT32:
		cmd = VIDIOC_PRIVATE_B52ISP_TOPOLOGY_SNAPSHOT;
		get_b52isp_profile32(&karg.pf, arg);
		compatible_arg = 0;
		break;
	case VIDIOC_PRIVATE_B52ISP_DOWNLOAD_CTDATA32:
		cmd = VIDIOC_PRIVATE_B52ISP_DOWNLOAD_CTDATA;
		get_rw_ctdata32(&karg.nd, arg);
		compatible_arg = 0;
		break;
	case VIDIOC_PRIVATE_B52ISP_UPLOAD_CTDATA32:
		cmd = VIDIOC_PRIVATE_B52ISP_UPLOAD_CTDATA;
		get_rw_ctdata32(&karg.nd, arg);
		compatible_arg = 0;
		break;
	case VIDIOC_PRIVATE_B52ISP_CONFIG_AF_WINDONW:
	case VIDIOC_PRIVATE_B52ISP_CONFIG_EXPO_METERING_MODE:
	case VIDIOC_PRIVATE_B52ISP_CONFIG_EXPO_METERING_ROI:
	case VIDIOC_PRIVATE_B52ISP_CONFIG_AWB_GAIN:
	case VIDIOC_PRIVATE_B52ISP_CONFIG_MEMORY_SENSOR:
	case VIDIOC_PRIVATE_B52ISP_CONFIG_ADV_DNS:
	case VIDIOC_PRIVATE_B52ISP_SET_PATH_ARG:
	case VIDIOC_PRIVATE_B52ISP_ANTI_SHAKE:
	case VIDIOC_PRIVATE_B52ISP_BRIGHTNESS:
	case VIDIOC_PRIVATE_B52ISP_SATURATION:
	case VIDIOC_PRIVATE_B52ISP_SHARPNESS:
		break;
	default:
		d_inf(1, "unknown compat ioctl '%c', dir=%d, #%d (0x%08x)\n",
			_IOC_TYPE(cmd), _IOC_DIR(cmd), _IOC_NR(cmd), cmd);
		return -ENXIO;
	}

	if (compatible_arg)
		ret = b52_usercopy(sd, cmd, arg);
	else {
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);
		ret = b52_usercopy(sd, cmd, (void *)&karg);
		set_fs(old_fs);
	}

	return ret;
}
#endif

static const struct v4l2_subdev_core_ops b52isp_path_core_ops = {
	.ioctl	= b52isp_path_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = b52isp_compat_ioctl32,
#endif
};

static int b52isp_path_set_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static const struct v4l2_subdev_video_ops b52isp_path_video_ops = {
	.s_stream	= b52isp_path_set_stream,
};

static int b52isp_path_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	int ret = 0;
	return ret;
}

static int b52isp_path_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int ret = 0;
	return ret;
}

static int b52isp_path_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_mbus_framefmt *pad_fmt;

	if (fmt->pad >= isd->pads_cnt)
		return -EINVAL;
	pad_fmt = isd->fmt_pad + fmt->pad;
	fmt->format = *pad_fmt;
	return 0;
}

static int b52isp_path_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_mbus_framefmt *pad_fmt;
	int ret = 0;

	if (fmt->pad >= isd->pads_cnt)
		return -EINVAL;
	pad_fmt = isd->fmt_pad + fmt->pad;

	if (fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		goto exit;
	/* Really apply here if not using MCU */
	d_inf(4, "%s:pad[%d] apply format<w%d, h%d, c%X>", sd->name, fmt->pad,
		fmt->format.width, fmt->format.height, fmt->format.code);
	*pad_fmt = fmt->format;
exit:
	return ret;
}

static int b52isp_path_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;
	sel->r = *rect;
	return 0;
}

static int b52isp_path_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		goto exit;
	/* Really apply here if not using MCU */
	d_inf(4, "%s:pad[%d] crop(%d, %d)<>(%d, %d)", sd->name, sel->pad,
		sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	*rect = sel->r;
exit:
	return ret;
}

static const struct v4l2_subdev_pad_ops b52isp_path_pad_ops = {
	.enum_mbus_code		= b52isp_path_enum_mbus_code,
	.enum_frame_size	= b52isp_path_enum_frame_size,
	.get_fmt		= b52isp_path_get_format,
	.set_fmt		= b52isp_path_set_format,
	.get_selection		= b52isp_path_get_selection,
	.set_selection		= b52isp_path_set_selection,
};

static const struct v4l2_subdev_ops b52isp_path_subdev_ops = {
	.core	= &b52isp_path_core_ops,
	.video	= &b52isp_path_video_ops,
	.pad	= &b52isp_path_pad_ops,
};

static int b52isp_path_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct isp_subdev *isd;
	struct b52isp_lpipe *lpipe;
	struct b52isp_ppipe *ppipe = NULL, *ppipe2 = NULL;
	struct b52isp *b52isp;
	int ret = 0;

	if (unlikely(WARN_ON(sd == NULL)))
		return -EPERM;
	isd = v4l2_get_subdev_hostdata(sd);
	if (unlikely(WARN_ON(isd == NULL)))
		return -EPERM;
	lpipe = isd->drv_priv;
	b52isp = lpipe->parent;
	WARN_ON(atomic_read(&lpipe->link_ref) < 0);

	if ((flags & MEDIA_LNK_FL_ENABLED) == 0)
		goto link_off;

	/* already mappend? */
	if (atomic_inc_return(&lpipe->link_ref) > 1)
		return 0;

	switch (lpipe->isd.sd_code) {
	/*
	 * TODO: For Now, use static mapping between physc_dev and logic_dev,
	 * maybe change to dynamic in future?
	 */
	case SDCODE_B52ISP_PIPE1:
		/* By default, driver assume CMD SET_FORMAT is used */
		lpipe->path_arg.aeag = TYPE_3A_UNLOCK;
		lpipe->path_arg.nr_frame = 0;
		ppipe = container_of(b52isp->blk[B52ISP_BLK_PIPE1],
			struct b52isp_ppipe, block);
		goto grab_single;
	case SDCODE_B52ISP_DUMP1:
		/* By default, driver assume CMD RAW_DUMP is used */
		lpipe->path_arg.aeag = TYPE_3A_UNLOCK;
		lpipe->path_arg.nr_frame = 0;
		ppipe = container_of(b52isp->blk[B52ISP_BLK_DUMP1],
			struct b52isp_ppipe, block);
		goto grab_single;
	case SDCODE_B52ISP_MS1:
		ppipe = container_of(b52isp->blk[B52ISP_BLK_PIPE1],
			struct b52isp_ppipe, block);
		goto grab_single;
	case SDCODE_B52ISP_PIPE2:
		/* By default, driver assume CMD SET_FORMAT is used */
		lpipe->path_arg.aeag = TYPE_3A_UNLOCK;
		lpipe->path_arg.nr_frame = 0;
		ppipe = container_of(b52isp->blk[B52ISP_BLK_PIPE2],
			struct b52isp_ppipe, block);
		goto grab_single;
	case SDCODE_B52ISP_DUMP2:
		/* By default, driver assume CMD RAW_DUMP is used */
		lpipe->path_arg.aeag = TYPE_3A_UNLOCK;
		lpipe->path_arg.nr_frame = 0;
		ppipe = container_of(b52isp->blk[B52ISP_BLK_DUMP2],
			struct b52isp_ppipe, block);
			goto grab_single;
	case SDCODE_B52ISP_MS2:
		ppipe = container_of(b52isp->blk[B52ISP_BLK_PIPE2],
			struct b52isp_ppipe, block);
grab_single:
		mutex_lock(&ppipe->map_lock);
		if (ppipe->isd == NULL) {
			ppipe->isd = &lpipe->isd;
			ret = b52isp_attach_blk_isd(&lpipe->isd, &ppipe->block);
			if (unlikely(ret < 0))
				goto fail_single;
			mutex_unlock(&ppipe->map_lock);
			isp_block_tune_power(&ppipe->block, 1);
			break;
		}
		ret = -EBUSY;
fail_single:
		/* physical pipeline occupied, abort */
		mutex_unlock(&ppipe->map_lock);
		return ret;
	/* For combined use case, grab both physical pipeline */
	case SDCODE_B52ISP_HS:
	case SDCODE_B52ISP_HDR:
	case SDCODE_B52ISP_3D:
		ppipe = container_of(b52isp->blk[B52ISP_BLK_PIPE1],
			struct b52isp_ppipe, block);
		ppipe2 = container_of(b52isp->blk[B52ISP_BLK_PIPE2],
			struct b52isp_ppipe, block);
		mutex_lock(&ppipe->map_lock);
		mutex_lock(&ppipe2->map_lock);
		if (ppipe->isd == NULL && ppipe2->isd == NULL) {
			ppipe->isd = &lpipe->isd;
			ppipe2->isd = &lpipe->isd;
			ret = b52isp_attach_blk_isd(&lpipe->isd, &ppipe->block);
			if (unlikely(ret < 0))
				goto fail_double;
			ret = b52isp_attach_blk_isd(&lpipe->isd,
							&ppipe2->block);
			if (unlikely(ret < 0)) {
				b52isp_detach_blk_isd(&lpipe->isd,
							&ppipe->block);
				goto fail_double;
			isp_block_tune_power(&ppipe->block, 1);
			isp_block_tune_power(&ppipe2->block, 1);
			}
			mutex_unlock(&ppipe->map_lock);
			break;
		}
		ret = -EBUSY;
fail_double:
		mutex_unlock(&ppipe->map_lock);
		mutex_unlock(&ppipe2->map_lock);
		return ret;
	default:
		return -ENODEV;
	}
	/* alloc meta data internal buffer */
	if (lpipe->meta_cpu == NULL) {
		lpipe->meta_size = b52_get_metadata_len(B52ISP_ISD_PIPE1);
		lpipe->meta_cpu = meta_cpu;
		lpipe->meta_dma = meta_dma;
		if ((lpipe->meta_cpu == NULL) ||
			(lpipe->meta_size >= META_DATA_SIZE)) {
			WARN_ON(1);
			return -ENOMEM;
		}

		d_inf(4, "alloc meta data for %s with %d bytes, VA@%p, PA@%X",
			lpipe->isd.subdev.name, lpipe->meta_size,
			lpipe->meta_cpu, (u32)lpipe->meta_dma);
		memset(lpipe->meta_cpu, 0xCD, lpipe->meta_size + 1);
	}
	if (lpipe->pinfo_buf == NULL) {
		lpipe->pinfo_size = PIPE_INFO_SIZE;
		lpipe->pinfo_buf = devm_kzalloc(b52isp->dev,
				lpipe->pinfo_size, GFP_KERNEL);
		WARN_ON(lpipe->pinfo_buf == NULL);
	}
	if (ppipe2)
		d_inf(4, "%s mapped to [%s | %s]", lpipe->isd.subdev.name,
			ppipe->block.name, ppipe2->block.name);
	else
		d_inf(4, "%s mapped to %s", lpipe->isd.subdev.name,
			ppipe->block.name);
	lpipe->output_sel = 0;
	/* trigger sensor init event here */
	return ret;

link_off:
	if (atomic_dec_return(&lpipe->link_ref) > 0)
		return 0;
	do {
		struct isp_dev_ptr *item = list_first_entry(
				&lpipe->isd.gdev_list,
				struct isp_dev_ptr, hook);
		ppipe = container_of(item->ptr, struct b52isp_ppipe, block);
		if (lpipe->meta_cpu)
			lpipe->meta_cpu = NULL;

		if (lpipe->pinfo_buf) {
			devm_kfree(b52isp->dev, lpipe->pinfo_buf);
			lpipe->pinfo_buf = NULL;
		}

		isp_block_tune_power(&ppipe->block, 0);
		mutex_lock(&ppipe->map_lock);
		ppipe->isd = NULL;
		b52isp_detach_blk_isd(&lpipe->isd, &ppipe->block);
		mutex_unlock(&ppipe->map_lock);
	} while (!list_empty(&lpipe->isd.gdev_list));
	d_inf(4, "%s and %s ummapped",
		lpipe->isd.subdev.name, ppipe->block.name);
	return 0;
}

static const struct media_entity_operations b52isp_path_media_ops = {
	.link_setup = b52isp_path_link_setup,
};

static int b52isp_path_sd_open(struct isp_subdev *ispsd)
{
	return 0;
}

static void b52isp_path_sd_close(struct isp_subdev *ispsd)
{
}

struct isp_subdev_ops b52isp_path_sd_ops = {
	.open		= b52isp_path_sd_open,
	.close		= b52isp_path_sd_close,
};

static void b52isp_path_remove(struct b52isp *b52isp)
{
	int i;

	for (i = B52ISP_BLK_PIPE1; i <= B52ISP_BLK_DUMP2; i++) {
		struct isp_block *blk = b52isp->blk[i];
		b52isp->blk[i] = NULL;
		if (blk)
			devm_kfree(b52isp->dev,
				container_of(blk, struct b52isp_ppipe, block));
	}

	for (i = B52ISP_ISD_PIPE1; i <= B52ISP_ISD_3D; i++) {
		struct isp_subdev *isd = b52isp->isd[i];
		struct b52isp_lpipe *pipe = container_of(isd,
						struct b52isp_lpipe, isd);
		b52isp->isd[i] = NULL;
		if (isd) {
			mutex_lock(&pipe->state_lock);
			if (pipe->cur_cmd)
				devm_kfree(isd->build->dev, pipe->cur_cmd);
			mutex_unlock(&pipe->state_lock);
			plat_ispsd_unregister(isd);
			v4l2_ctrl_handler_free(&isd->ctrl_handler);
			devm_kfree(b52isp->dev,
				container_of(isd, struct b52isp_lpipe, isd));
		}
	}
}

static int b52isp_path_create(struct b52isp *b52isp)
{
	struct isp_block *block;
	struct isp_subdev *ispsd;
	struct v4l2_subdev *sd;
	int ret, i, pipe_cnt;

	for (i = 0; i < b52isp->hw_desc->nr_pipe * 2; i++) {
		struct b52isp_ppipe *pipe = devm_kzalloc(b52isp->dev,
			sizeof(*pipe), GFP_KERNEL);
		if (pipe == NULL)
			return -ENOMEM;
		/* Add ISP pipeline Blocks */
		block = &pipe->block;
		block->id.dev_type = PCAM_IP_B52ISP;
		block->id.dev_id = b52isp->dev->id;
		block->id.mod_id = B52ISP_BLK_PIPE1 + i;
		block->dev = b52isp->dev;
		block->name = b52isp_block_name[B52ISP_BLK_PIPE1 + i];
		block->req_list = b52pipe_req;
		block->ops = &b52isp_path_hw_ops;
		mutex_init(&pipe->map_lock);
		pipe->parent = b52isp;
		b52isp->blk[B52ISP_BLK_PIPE1 + i] = block;
	}
	if (b52isp->hw_desc->nr_pipe == 1)
		pipe_cnt = PATH_PER_PIPE;
	else
		pipe_cnt = b52isp->hw_desc->nr_pipe * 3 + 3;
	for (i = 0; i < pipe_cnt; i++) {
		/* Add ISP logical pipeline isp-subdev */
		struct b52isp_lpipe *pipe = devm_kzalloc(b52isp->dev,
			sizeof(*pipe), GFP_KERNEL);
		if (pipe == NULL)
			return -ENOMEM;
		ispsd = &pipe->isd;
		sd = &ispsd->subdev;
		ret = b52isp_init_ctrls(&pipe->ctrls);
		if (unlikely(ret < 0))
			return ret;
		sd->entity.ops = &b52isp_path_media_ops;
		v4l2_subdev_init(sd, &b52isp_path_subdev_ops);
		sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		sd->ctrl_handler = &pipe->ctrls.ctrl_handler;
		strncpy(sd->name, b52isp_ispsd_name[B52ISP_ISD_PIPE1 + i],
			sizeof(sd->name));
		ispsd->ops = &b52isp_path_sd_ops;
		ispsd->drv_priv = pipe;
		ispsd->pads[0].flags = MEDIA_PAD_FL_SINK;
		ispsd->pads[1].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads_cnt = 2;
		ispsd->single = 1;
		INIT_LIST_HEAD(&ispsd->gdev_list);
		ispsd->sd_code = SDCODE_B52ISP_PIPE1 + i;
		switch (i+B52ISP_ISD_PIPE1) {
		case B52ISP_ISD_PIPE1:
		case B52ISP_ISD_DUMP1:
		/*
		 * Just attach physcal pipeline to logical pipeline to help
		 * init phys_pipe. After the registeration, they'll be detached
		 */
			if (b52isp->blk[B52ISP_BLK_PIPE1 + i]) {
				ret = b52isp_attach_blk_isd(ispsd,
					b52isp->blk[B52ISP_BLK_PIPE1 + i]);
				if (unlikely(ret < 0))
					return ret;
			}
			ret = plat_ispsd_register(ispsd);
			if (ret < 0)
				return ret;
			b52isp->isd[B52ISP_ISD_PIPE1 + i] = ispsd;
			pipe->parent = b52isp;
			mutex_init(&pipe->state_lock);

			/* Now detach! */
			if (b52isp->blk[B52ISP_BLK_PIPE1 + i])
				b52isp_detach_blk_isd(ispsd,
					b52isp->blk[B52ISP_BLK_PIPE1 + i]);
		/*
		 * From this point, physical pipeline and logical pipeline is
		 * indepedent to each other. Before using any logical pipeline,
		 * it MUST find a physical pipeline and attach to it
		 */
			break;
		case B52ISP_ISD_PIPE2:
		case B52ISP_ISD_DUMP2:
		/*
		* Just attach physcal pipeline to logical pipeline to help
		* init phys_pipe. After the registeration, they'll be detached
		*/
			if (b52isp->blk[B52ISP_BLK_PIPE1 + i - 1]) {
				ret = b52isp_attach_blk_isd(ispsd,
					b52isp->blk[B52ISP_BLK_PIPE1 + i - 1]);
				if (unlikely(ret < 0))
					return ret;
			}
			ret = plat_ispsd_register(ispsd);
			if (ret < 0)
				return ret;
			b52isp->isd[B52ISP_ISD_PIPE1 + i] = ispsd;
			pipe->parent = b52isp;
			mutex_init(&pipe->state_lock);

			/* Now detach! */
			if ((b52isp->blk[B52ISP_BLK_PIPE1 + i - 1]))
				b52isp_detach_blk_isd(ispsd,
					b52isp->blk[B52ISP_BLK_PIPE1 + i - 1]);
		/*
		* From this point, physical pipeline and logical pipeline is
		* indepedent to each other. Before using any logical pipeline,
		* it MUST find a physical pipeline and attach to it
		*/
			break;
		default:
			ret = plat_ispsd_register(ispsd);
			if (ret < 0)
				return ret;
			b52isp->isd[B52ISP_ISD_PIPE1 + i] = ispsd;
			pipe->parent = b52isp;
			mutex_init(&pipe->state_lock);
			break;
		}
		/* For single pipeline ISP, don't create virtual pipes */
		if ((b52isp->hw_desc->nr_pipe < 2) && (i >= B52ISP_ISD_PIPE2))
			break;
	}
	return 0;
}

/****************************** AXI Master Block ******************************/

static int b52isp_axi_hw_open(struct isp_block *block)
{
	return 0;
}

static void b52isp_axi_hw_close(struct isp_block *block)
{

}

static int b52isp_axi_set_clock(struct isp_block *block, int rate)
{
	return 0;
}

struct isp_block_ops b52isp_axi_hw_ops = {
	.open	= b52isp_axi_hw_open,
	.close	= b52isp_axi_hw_close,
	.set_clock = b52isp_axi_set_clock,
};

/***************************** AXI Master Subdev *****************************/
/* ioctl(subdev, IOCTL_XXX, arg) is handled by this one */
static long b52isp_axi_ioctl(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	return 0;
}

static const struct v4l2_subdev_core_ops b52isp_axi_core_ops = {
	.ioctl	= b52isp_axi_ioctl,
};

static int b52isp_axi_set_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static const struct v4l2_subdev_video_ops b52isp_axi_video_ops = {
	.s_stream	= b52isp_axi_set_stream,
};

static int b52isp_axi_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	int ret = 0;
	return ret;
}

static int b52isp_axi_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int ret = 0;
	return ret;
}

static int b52isp_axi_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_mbus_framefmt *pad_fmt;

	if (fmt->pad >= isd->pads_cnt)
		return -EINVAL;
	pad_fmt = isd->fmt_pad + fmt->pad;
	fmt->format = *pad_fmt;
	return 0;
}

static int b52isp_axi_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_mbus_framefmt *pad_fmt;
	int ret = 0;

	if (fmt->pad >= isd->pads_cnt)
		return -EINVAL;
	pad_fmt = isd->fmt_pad + fmt->pad;

	if (fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		goto exit;
	/* Really apply here if not using MCU */
	d_inf(4, "%s:pad[%d] apply format<w%d, h%d, c%X>", sd->name, fmt->pad,
		fmt->format.width, fmt->format.height, fmt->format.code);
	*pad_fmt = fmt->format;
exit:
	return ret;
}

static int b52isp_axi_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;
	sel->r = *rect;
	return 0;
}

static int b52isp_axi_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		goto exit;
	/* Really apply here if not using MCU */
	d_inf(4, "%s:pad[%d] crop(%d, %d)<>(%d, %d)", sd->name, sel->pad,
		sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	*rect = sel->r;
exit:
	return ret;
}

static const struct v4l2_subdev_pad_ops b52isp_axi_pad_ops = {
	.enum_mbus_code		= b52isp_axi_enum_mbus_code,
	.enum_frame_size	= b52isp_axi_enum_frame_size,
	.get_fmt		= b52isp_axi_get_format,
	.set_fmt		= b52isp_axi_set_format,
	.set_selection		= b52isp_axi_set_selection,
	.get_selection		= b52isp_axi_get_selection,
};

static const struct v4l2_subdev_ops b52isp_axi_subdev_ops = {
	.core	= &b52isp_axi_core_ops,
	.video	= &b52isp_axi_video_ops,
	.pad	= &b52isp_axi_pad_ops,
};

int b52_fill_mmu_chnl(struct plat_cam *pcam,
		struct isp_videobuf *buf, int num_planes)
{
	int ret = 0;
	struct plat_vnode *pvnode = NULL;

	pvnode = container_of(buf->vb.vb2_queue, struct plat_vnode, vnode.vq);
	if (pvnode->fill_mmu_chnl)
		ret = pvnode->fill_mmu_chnl(pcam, &buf->vb, num_planes);

	return ret;
}
EXPORT_SYMBOL(b52_fill_mmu_chnl);

static inline int b52_fill_buf(struct isp_videobuf *buf,
		struct plat_cam *pcam, int num_planes, int mac_id, int port)
{
	int i;
	int ret = 0;
	dma_addr_t dmad[VIDEO_MAX_PLANES];

	if (buf == NULL) {
		d_inf(4, "%s: buffer is NULL", __func__);
		return -EINVAL;
	}

	ret = b52_fill_mmu_chnl(pcam, buf, num_planes);
	if (ret < 0) {
		d_inf(1, "Failed to fill mmu addr");
		return ret;
	}

	for (i = 0; i < num_planes; i++)
		dmad[i] = (dma_addr_t)(buf->ch_info[i].daddr);

	ret = b52_update_mac_addr(dmad, 0, num_planes, mac_id, port);
	if (ret < 0) {
		d_inf(1, "Failed to update mac addr");
		return ret;
	}

	return ret;
}

static inline int b52isp_update_metadata(struct isp_subdev *isd,
	struct isp_vnode *vnode, struct isp_videobuf *buffer)
{
	int i;
	u32 type;
	u16 size;
	u16 *src;
	u16 *dst;
	/*
	 * FIXME: hard code to assume META plane comes right after
	 * image plane, we should check the plane signature to identify
	 * the plane usage.
	 */
	int meta_pid = vnode->format.fmt.pix_mp.num_planes;
	struct media_pad *pad_pipe =
		media_entity_remote_pad(isd->pads + B52PAD_AXI_IN);
	struct isp_subdev *lpipe_isd = v4l2_get_subdev_hostdata(
		media_entity_to_v4l2_subdev(pad_pipe->entity));
	struct b52isp_lpipe *lpipe = container_of(lpipe_isd,
		struct b52isp_lpipe, isd);

	if ((buffer->vb.v4l2_buf.type ==
		 V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ||
		(buffer->vb.num_planes <= meta_pid))
		return -EINVAL;

	if (buffer->va[meta_pid] == NULL ||
		lpipe->meta_cpu == NULL ||
		lpipe->pinfo_buf == NULL)
		return -EINVAL;

	type = buffer->vb.v4l2_planes[meta_pid].reserved[0];
	if (type == V4L2_PLANE_SIGNATURE_PIPELINE_INFO)
		b52_read_pipeline_info(0, lpipe->pinfo_buf);

	dst = buffer->va[meta_pid];
	if (type == V4L2_PLANE_SIGNATURE_PIPELINE_INFO) {
		src = (u16 *)lpipe->pinfo_buf;
		size = lpipe->pinfo_size;
	} else if (type == V4L2_PLANE_SIGNATURE_PIPELINE_META) {
		src = (u16 *)lpipe->meta_cpu;
		size = lpipe->meta_size;

		if (((u8 *)lpipe->meta_cpu)[size] != 0xCD)
			d_inf(1, "%s:%d meta data cross the requested size",
				isd->subdev.name, ((u8 *)lpipe->meta_cpu)[size]);
	} else
		return -EINVAL;

	if (size > buffer->vb.v4l2_planes[meta_pid].length) {
		d_inf(2, "%s buffer length error: req %x, got %x",
			  isd->subdev.name, size,
			  buffer->vb.v4l2_planes[meta_pid].length);
		return -EINVAL;
	}

	if (type == V4L2_PLANE_SIGNATURE_PIPELINE_INFO)
		for (i = 0; i < size; i += 2)
			*dst++ = *src++;
	else
		for (i = 0; i < size; i += 2)
			*dst++ = swab16(*src++);

	return 0;
}

static int b52isp_mac_handler(struct isp_subdev *isd, unsigned long event)
{
	u8 irqstatus = event & 0xFFFF;
	u8 port = (event >> PORT_ID_SHIFT) & 0xF;
	u8 mac_id = (event >> MAC_ID_SHIFT) & 0xF;
	struct isp_videobuf *buffer;
	struct isp_vnode *vnode = ispsd_get_video(isd);
	struct b52isp_laxi *laxi = isd->drv_priv;
	struct isp_build *build = container_of(isd->subdev.entity.parent,
						struct isp_build, media_dev);
	struct plat_cam *pcam = build->plat_priv;
	int num_planes = vnode->format.fmt.pix_mp.num_planes;
	struct isp_block *blk = isp_sd2blk(&laxi->isd);
	struct b52isp_paxi *paxi = container_of(blk, struct b52isp_paxi, blk);
	int ret = 0;

	BUG_ON(vnode == NULL);

	switch (port) {
	case B52AXI_PORT_R1:
		port = 2;
		break;
	case B52AXI_PORT_W1:
		port = 0;
		break;
	case B52AXI_PORT_W2:
		port = 1;
		break;
	default:
		BUG_ON(1);
		break;
	}

	if (irqstatus & VIRT_IRQ_FIFO)
		laxi->overflow = 1;
	if (laxi->overflow)
		irqstatus &= ~VIRT_IRQ_START;

	if (laxi->dma_state != B52DMA_ACTIVE)
		goto check_sof_irq;
	else
		goto check_eof_irq;

check_sof_irq:
	if (irqstatus & VIRT_IRQ_START) {
		laxi->dma_state = B52DMA_ACTIVE;
		irqstatus &= ~VIRT_IRQ_START;

		buffer = isp_vnode_find_busy_buffer(vnode, 1);
		if (!buffer) {
			buffer = isp_vnode_get_idle_buffer(vnode);
			isp_vnode_put_busy_buffer(vnode, buffer);
		}

		if (buffer && laxi->stream)
			b52_fill_buf(buffer, pcam, num_planes, mac_id, port);

		d_inf(4, "%s receive start", isd->subdev.name);
	}

	if ((laxi->dma_state == B52DMA_ACTIVE) &&
		(irqstatus & (VIRT_IRQ_FIFO | VIRT_IRQ_DONE)))
		goto check_eof_irq;

check_drop:
	if (irqstatus & VIRT_IRQ_DROP) {
		struct plat_vnode *pvnode = container_of(vnode, struct plat_vnode, vnode);
		if (laxi->stream)
			laxi->dma_state = B52DMA_IDLE;
		irqstatus &= ~VIRT_IRQ_DROP;

		if (laxi->overflow) {
			if (laxi->drop_cnt++ < 1)
				goto check_eof_irq;

			laxi->drop_cnt = 0;
			laxi->overflow = 0;

			if (pvnode->reset_mmu_chnl) {
				pvnode->reset_mmu_chnl(pcam, vnode, num_planes);
				d_inf(3, "%s reset", isd->subdev.name);
			}
			b52_clear_overflow_flag(laxi->mac, laxi->port);
		}

		buffer = isp_vnode_find_busy_buffer(vnode, 0);
		if (!buffer) {
			buffer = isp_vnode_get_idle_buffer(vnode);
			isp_vnode_put_busy_buffer(vnode, buffer);
		} else if (laxi->stream)
			d_inf(3, "%s: buffer in the BusyQ when drop! idle:%d, busy:%d",
				  vnode->vdev.name,
				  vnode->idle_buf_cnt, vnode->busy_buf_cnt);
		if (buffer && laxi->stream)
			b52_fill_buf(buffer, pcam, num_planes, mac_id, port);
		else {
			if (laxi->dma_state == B52DMA_DROP)
				laxi->dma_state = B52DMA_HW_NO_STREAM;
			else
				laxi->dma_state = B52DMA_DROP;
		}
		d_inf(3, "%s receive drop frame", isd->subdev.name);
	}

check_eof_irq:
	if (irqstatus & VIRT_IRQ_FIFO) {
		laxi->dma_state = B52DMA_IDLE;
		irqstatus &= ~VIRT_IRQ_FIFO;

		d_inf(3, "%s receive FIFO flow error", isd->subdev.name);
	}

	if (irqstatus & VIRT_IRQ_DONE) {
		irqstatus &= ~VIRT_IRQ_DONE;
		if (laxi->dma_state != B52DMA_ACTIVE)
			goto recheck;
		laxi->dma_state = B52DMA_IDLE;

		if ((paxi->r_type == B52AXI_REVENT_MEMSENSOR) &&
			(port == B52AXI_PORT_R1)) {
			buffer = isp_vnode_find_busy_buffer(vnode, 0);
			if (buffer && laxi->stream)
				b52_fill_buf(buffer, pcam, num_planes, mac_id, port);
			d_inf(4, "%s f2f kick read port", isd->subdev.name);
			goto recheck;
		}

		buffer = isp_vnode_get_busy_buffer(vnode);
		if (!buffer) {
			d_inf(3, "%s: done busy buffer is NULL", isd->subdev.name);
			goto recheck;
		}

		b52isp_update_metadata(isd, vnode, buffer);
		isp_vnode_export_buffer(buffer);

		d_inf(4, "%s receive done", isd->subdev.name);
	}

recheck:
	if (laxi->dma_state != B52DMA_ACTIVE) {
		if (irqstatus & (VIRT_IRQ_START | VIRT_IRQ_DROP))
			goto check_sof_irq;
		if (irqstatus & (VIRT_IRQ_FIFO | VIRT_IRQ_DONE))
			d_inf(1, "-----------unmatched EOF IRQ: %X", irqstatus);
	} else {
		if (irqstatus & (VIRT_IRQ_FIFO | VIRT_IRQ_DONE))
			goto check_eof_irq;
		if (irqstatus & (VIRT_IRQ_START | VIRT_IRQ_DROP)) {
			d_inf(1, "-----------unmatched SOF IRQ: %X", irqstatus);
			if (irqstatus & VIRT_IRQ_DROP)
				goto check_drop;
		}
	}

	return ret;
}

static int b52isp_mac_irq_event(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct isp_subdev *isd = data;
	return b52isp_mac_handler(isd, event);
}

static struct notifier_block b52isp_mac_irq_nb = {
	.notifier_call = b52isp_mac_irq_event,
};

/*
 * Select a physc port for a logic dev
 */
static int b52_map_axi_port(struct b52isp_laxi *laxi, int map,
			struct plat_topology *topo)
{
	struct b52isp *b52 = laxi->parent;
	struct isp_block *blk;
	struct b52isp_paxi *paxi = NULL;
	int ret = 0, fit_bit, ok_bit;
	__u32 idle_map, tag;

	WARN_ON(atomic_read(&laxi->map_cnt) < 0);

	if (map == 0)
		goto unmap;

	/* already mappend? */
	if (atomic_inc_return(&laxi->map_cnt) > 1)
		return 0;

	/* Actually map logic dev to physc dev */
	mutex_lock(&b52->mac_map_lock);
	tag = plat_get_src_tag(topo);
	idle_map = output_mask[laxi->isd.sd_code - SDCODE_B52ISP_A1W1]
		& (~b52->mac_map);
	fit_bit = ok_bit = -1;

	while (idle_map) {
		unsigned long cur_mask = idle_map - (idle_map & (idle_map - 1));
		__u32 bit = find_first_bit(&cur_mask, BITS_PER_LONG);
		__u32 src;

		blk = b52->blk[B52ISP_BLK_AXI1 + bit / B52AXI_PORT_CNT];
		paxi = container_of(blk, struct b52isp_paxi, blk);
		if ((bit % B52AXI_PORT_CNT) == B52AXI_PORT_R1) {
			WARN_ON(paxi->isd[B52AXI_PORT_R1]);
			fit_bit = ok_bit = bit;
			break;
		}

		src = paxi->src_tag[B52AXI_PORT_W1] |
			paxi->src_tag[B52AXI_PORT_W2];
		if (src == 0) {
			if (ok_bit < 0)
				ok_bit = bit;
		} else if (src == tag) {
			fit_bit = bit;
			break;
		}
		idle_map -= cur_mask;
	}

	if (fit_bit < 0)
		fit_bit = ok_bit;
	if (fit_bit < 0) {
		/* Can't find a physical port to map the logical port */
		ret = -EBUSY;
		goto fail;
	}

	if (WARN_ON(test_and_set_bit(fit_bit, &b52->mac_map))) {
		ret = -EBUSY;
		goto fail;
	}
	WARN_ON(paxi->isd[fit_bit % B52AXI_PORT_CNT]);
	blk = b52->blk[B52ISP_BLK_AXI1 + fit_bit / B52AXI_PORT_CNT];
	paxi = container_of(blk, struct b52isp_paxi, blk);
	paxi->isd[fit_bit % B52AXI_PORT_CNT] = &laxi->isd;
	paxi->src_tag[fit_bit % B52AXI_PORT_CNT] = tag;
	ret = b52isp_attach_blk_isd(&laxi->isd, &paxi->blk);
	if (unlikely(ret < 0))
		goto fail;
	laxi->mac = fit_bit / B52AXI_PORT_CNT;
	laxi->port = fit_bit % B52AXI_PORT_CNT;

	mutex_unlock(&b52->mac_map_lock);

	d_inf(3, "%s couple to MAC%d, port%d", laxi->isd.subdev.name,
		laxi->mac + 1, laxi->port + 1);
	return 0;

fail:
	/* failed to map to physical dev, abort */
	mutex_unlock(&b52->mac_map_lock);
	atomic_dec(&laxi->map_cnt);
	return ret;

unmap:
	if (atomic_dec_return(&laxi->map_cnt) > 0)
		return 0;
	paxi = container_of(isp_sd2blk(&laxi->isd), struct b52isp_paxi, blk);
	fit_bit = laxi->mac * B52AXI_PORT_CNT + laxi->port;

	mutex_lock(&b52->mac_map_lock);
	paxi->isd[laxi->port] = NULL;
	paxi->src_tag[laxi->port] = 0;
	paxi->event = 0;
	laxi->mac = -1;
	laxi->port = -1;
	b52isp_detach_blk_isd(&laxi->isd, &paxi->blk);
	clear_bit(fit_bit, &b52->mac_map);
	mutex_unlock(&b52->mac_map_lock);

	d_inf(3, "%s decouple from MAC%d, port%d", laxi->isd.subdev.name,
		laxi->mac + 1, laxi->port + 1);
	return 0;
}

static int b52_enable_axi_port(struct b52isp_laxi *laxi, int enable,
			struct isp_vnode *vnode)
{
	int ret;
	int cnt = 0;
	struct plat_vnode *pvnode = container_of(vnode, struct plat_vnode, vnode);
	struct isp_build *build = container_of(laxi->isd.subdev.entity.parent,
						 struct isp_build, media_dev);
	struct plat_cam *pcam = build->plat_priv;
	struct b52isp_paxi *paxi = container_of(isp_sd2blk(&laxi->isd),
						 struct b52isp_paxi, blk);

	WARN_ON(atomic_read(&laxi->en_cnt) < 0);

	if (!pvnode || !pcam || !paxi) {
		d_inf(1, "%s %d, parameter error\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (enable == 0)
		goto disable;

	if (atomic_inc_return(&laxi->en_cnt) > 1)
		return 0;

	ret = isp_block_tune_power(&paxi->blk, 1);
	if (unlikely(ret < 0))
		goto pwr_err;

	if (pvnode->alloc_mmu_chnl) {
		ret = pvnode->alloc_mmu_chnl(pcam, PCAM_IP_B52ISP,
					paxi->blk.id.mod_id, laxi->port,
					vnode->format.fmt.pix_mp.num_planes,
					&pvnode->mmu_ch_dsc);
		if (unlikely(ret < 0))
			goto mmu_err;
	}

	ret = b52_ctrl_mac_irq(laxi->mac, laxi->port, 1);
	if (unlikely(ret < 0))
		goto mac_err;

	return 0;

mac_err:
	if (pvnode->free_mmu_chnl)
		pvnode->free_mmu_chnl(pcam, &pvnode->mmu_ch_dsc);
mmu_err:
	isp_block_tune_power(&paxi->blk, 0);
pwr_err:
	atomic_dec(&laxi->en_cnt);
	return ret;

disable:
	if (atomic_dec_return(&laxi->en_cnt) > 0)
		return 0;

	b52_clear_mac_rdy_bit(laxi->mac, laxi->port);
	while (laxi->dma_state != B52DMA_HW_NO_STREAM) {
		usleep_range(1000, 1500);
		if (cnt++ > 500) {
			d_inf(1, "%s err wait HW_NO_STREAM",
				  laxi->isd.subdev.name);
			break;
		}
	}

	b52_ctrl_mac_irq(laxi->mac, laxi->port, 0);
	if (pvnode->free_mmu_chnl)
		pvnode->free_mmu_chnl(pcam, &pvnode->mmu_ch_dsc);
	isp_block_tune_power(&paxi->blk, 0);

	laxi->drop_cnt = 0;
	laxi->overflow = 0;

	return 0;
}

static int b52isp_export_cmd_buffer(struct b52isp_cmd *cmd)
{
	struct isp_videobuf *ivb;
	int i;
	if (cmd->src_type == CMD_SRC_AXI) {
		ivb = isp_vnode_get_busy_buffer(cmd->mem.vnode);
		while (ivb) {
			isp_vnode_export_buffer(ivb);
			ivb = isp_vnode_get_busy_buffer(cmd->mem.vnode);
		}
	}

	for (i = 0; i < B52_OUTPUT_PER_PIPELINE; i++) {
		if (cmd->output[i].vnode == NULL)
			continue;
		ivb = isp_vnode_get_busy_buffer(cmd->output[i].vnode);
		while (ivb) {
			isp_vnode_export_buffer(ivb);
			ivb = isp_vnode_get_busy_buffer(cmd->output[i].vnode);
		}
	}
	return 0;
}

#define SETUP_WITH_DEFAULT_FORMAT	\
do {	\
	if (lpipe->output_sel !=	\
		lpipe->cur_cmd->output_map) {	\
		for (i = 0; i < B52_OUTPUT_PER_PIPELINE; i++) {	\
			if (!test_bit(i, &lpipe->cur_cmd->output_map))	\
				continue;	\
			lpipe->cur_cmd->output[i].vnode = topo->dst[i];	\
			lpipe->cur_cmd->output[i].pix_mp =	\
				vnode->format.fmt.pix_mp;	\
		}	\
	}	\
} while (0)

static int b52isp_laxi_stream_handler(struct b52isp_laxi *laxi,
		struct plat_vnode *pvnode, int stream)
{
	int ret = 0, port, out_id, sid;
	unsigned long irq_flags;
	static int mm_stream;
	struct isp_subdev *pipe;
	struct isp_build *build = container_of(laxi->isd.subdev.entity.parent,
						struct isp_build, media_dev);
	struct isp_vnode *vnode = &pvnode->vnode;
	struct plat_cam *pcam = build->plat_priv;
	int num_planes = vnode->format.fmt.pix_mp.num_planes;
	struct b52isp_paxi *paxi;
	struct plat_topology *topo;
	struct b52isp_lpipe *lpipe;

	d_inf(3, "%s: handling the stream %d event from %s",
		laxi->isd.subdev.name, stream, vnode->vdev.name);

	if (laxi->stream == stream) {
		d_inf(3, "%s: already stream %d\n", vnode->vdev.name, stream);
		return 0;
	}

	topo = pvnode->plat_topo;
	pipe = topo->path;
	if (WARN_ON(pipe == NULL))
		return -ENODEV;
	for (out_id = 0; out_id < MAX_OUTPUT_PER_PIPELINE; out_id++)
		if (&pvnode->vnode == topo->dst[out_id])
			break;
	lpipe = pipe->drv_priv;

	laxi->stream = stream;

	if (!stream)
		goto stream_off;

	/* Map / Unmap Logical AXI and Physical AXI */
	if (laxi == topo->scalar_a->drv_priv) {
		ret = b52_map_axi_port(laxi, 1, topo);
		if (unlikely(WARN_ON(ret < 0)))
			return ret;
	} else {
		sid = 0;
		while (topo->scalar_b[sid]) {
			ret = b52_map_axi_port(topo->scalar_b[sid]->drv_priv,
						1, topo);
			if (unlikely(WARN_ON(ret < 0)))
				goto unmap_scalar_b;
			sid++;
		}
	}

	/* Stream on must happen after map LAXI and PAXI */
	if (laxi->stream) {
		int mac_id, i, nr_out;
		struct isp_videobuf *isp_vb;
		struct isp_block *blk = isp_sd2blk(&laxi->isd);

		paxi = container_of(blk, struct b52isp_paxi, blk);
		mac_id = laxi->mac;
		port = laxi->port;

		ret = b52_enable_axi_port(laxi, laxi->stream, vnode);
		if (unlikely(WARN_ON(ret < 0)))
			return ret;

		mutex_lock(&lpipe->state_lock);
		if (vnode->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			set_bit(out_id, &lpipe->cur_cmd->enable_map);

		if ((lpipe->cur_cmd->cmd_name != CMD_RAW_PROCESS) &&
			!(lpipe->cur_cmd->flags & BIT(CMD_FLAG_MS))) {
			struct v4l2_subdev *sd = lpipe->cur_cmd->sensor;
			struct media_pad *csi_pad = media_entity_remote_pad(sd->entity.pads);
			struct v4l2_subdev *csi_sd = media_entity_to_v4l2_subdev(csi_pad->entity);
			struct b52_sensor *sensor = to_b52_sensor(sd);

			if ((lpipe->cur_cmd->cmd_name == CMD_SET_FORMAT) ||
					(lpipe->cur_cmd->cmd_name == CMD_RAW_DUMP))
				b52_sensor_call(sensor, init);
			v4l2_subdev_call(csi_sd, video, s_stream, 1);
			v4l2_subdev_call(sd, video, s_stream, 1);
		}
		/*
		 * This is just a W/R. Eventually, all command will be decided
		 * by driver topology and path atgument, just before send MCU
		 * commands
		 */
		switch (lpipe->cur_cmd->cmd_name) {
		case CMD_SET_FORMAT:
			if (lpipe->path_arg.aeag == TYPE_3A_LOCKED)
				lpipe->cur_cmd->cmd_name = CMD_IMG_CAPTURE;
			break;
		default:
			break;
		}

		/*
		 * For Multiple output pipelines, no need to worry about
		 * concurrent stream on, because all vdev stream on must hold
		 * pipeline state lock first, so only one vdev thread will
		 * reach this switch case
		 */
		switch (lpipe->cur_cmd->cmd_name) {
		case CMD_RAW_DUMP:
		case CMD_CHG_FORMAT:
		case CMD_SET_FORMAT:
			lpipe->cur_cmd->nr_mac = paxi->parent->hw_desc->nr_axi;
			lpipe->cur_cmd->output_map = topo->dst_map;
			if (vnode->buf_type ==
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

				/*
				 * FIXME: Firmware issue require all output port
				 * to be configured with a default format, even
				 * if it's not streamed on
				 */
				SETUP_WITH_DEFAULT_FORMAT;

				lpipe->cur_cmd->output[out_id].vnode = vnode;
				lpipe->cur_cmd->output[out_id].no_zoom = pvnode->no_zoom;
				lpipe->cur_cmd->output[out_id].pix_mp =
					vnode->format.fmt.pix_mp;
				laxi->dma_state = B52DMA_HW_NO_STREAM;
				lpipe->cur_cmd->flags |=
					BIT(CMD_FLAG_LOCK_AEAG);

				if (mm_stream == 1) {
					paxi->r_type = B52AXI_REVENT_MEMSENSOR;
					ret = b52isp_try_apply_cmd(lpipe);
					if (ret < 0)
						goto unlock;
				} else if (lpipe->cur_cmd->flags
							& BIT(CMD_FLAG_MS))
					ret = 0;
				else {
					paxi->r_type =
						B52AXI_REVENT_RAWPROCCESS;
					ret = b52isp_try_apply_cmd(lpipe);
					if (ret < 0)
						goto unlock;
				}
			} else if (vnode->buf_type ==
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
				isp_vb = isp_vnode_get_idle_buffer(vnode);
				BUG_ON(isp_vb == NULL);
				lpipe->cur_cmd->mem.axi_id = mac_id;
				lpipe->cur_cmd->mem.buf[0] = isp_vb;
				lpipe->cur_cmd->mem.vnode = vnode;
				if (lpipe->cur_cmd->memory_sensor_data ==
									NULL) {
					d_inf(1, "can't match sensor\n");
					d_inf(1, "OV13850 as default sensor\n");
					lpipe->cur_cmd->memory_sensor_data =
					memory_sensor_match("ovt,ov13850");
				}
				if (lpipe->cur_cmd->flags & BIT(CMD_FLAG_MS)) {
					paxi->r_type = B52AXI_REVENT_MEMSENSOR;
					b52isp_set_ddr_qos(528000);
					ret = b52isp_try_apply_cmd(lpipe);
					if (ret < 0)
						goto unlock;
					ret = b52_fill_buf(isp_vb, pcam,
						num_planes, mac_id, 2);
					if (ret < 0)
						goto unlock;
					ret = isp_vnode_put_busy_buffer(vnode,
									isp_vb);
					if (unlikely(WARN_ON(ret < 0)))
						goto unlock;
					mm_stream = 1;
				}
			}
			break;
		case CMD_IMG_CAPTURE:
			lpipe->cur_cmd->output_map = topo->dst_map;
			for (i = 0; i < lpipe->path_arg.nr_frame; i++) {
				isp_vb = isp_vnode_get_idle_buffer(vnode);
				if (isp_vb == NULL) {
					d_inf(1, "%s: capture buf not ready %d > %d",
						lpipe->isd.subdev.name,
						lpipe->path_arg.nr_frame, i);
					goto unlock;
				}
				ret = isp_vnode_put_busy_buffer(vnode, isp_vb);
				if (unlikely(WARN_ON(ret < 0)))
					goto unlock;
				lpipe->cur_cmd->output[out_id].vnode = vnode;
				lpipe->cur_cmd->output[out_id].pix_mp =
					vnode->format.fmt.pix_mp;
				lpipe->cur_cmd->output[out_id].buf[i] = isp_vb;
			}

			if (pvnode->fill_mmu_chnl) {
				lpipe->cur_cmd->priv = (void *)pcam;
				isp_vb = lpipe->cur_cmd->output[out_id].buf[0];
				ret = pvnode->fill_mmu_chnl(pcam, &isp_vb->vb, num_planes);
				if (ret < 0) {
					d_inf(1, "%s: failed to config MMU channel",
						  vnode->vdev.name);
					goto unlock;
				}
			} else
				lpipe->cur_cmd->priv = NULL;

			lpipe->cur_cmd->output[out_id].nr_buffer =
							lpipe->path_arg.nr_frame;
			lpipe->cur_cmd->b_ratio_1 = lpipe->path_arg.ratio_1_2;
			lpipe->cur_cmd->b_ratio_2 = lpipe->path_arg.ratio_1_3;
			if (lpipe->path_arg.linear_yuv)
				lpipe->cur_cmd->flags |=
					BIT(CMD_FLAG_LINEAR_YUV);
			lpipe->cur_cmd->flags |= BIT(CMD_FLAG_LOCK_AEAG);
			ret = b52isp_try_apply_cmd(lpipe);
			lpipe->cur_cmd->flags &= ~BIT(CMD_FLAG_LINEAR_YUV);
			if (!ret)
			b52isp_export_cmd_buffer(lpipe->cur_cmd);
			goto unlock;
		case CMD_HDR_STILL:
		case CMD_RAW_PROCESS:
			for (i = 0; i < vnode->hw_min_buf; i++) {
				isp_vb = isp_vnode_get_idle_buffer(vnode);
				BUG_ON(isp_vb == NULL);
				ret = isp_vnode_put_busy_buffer(vnode, isp_vb);
				if (unlikely(WARN_ON(ret < 0)))
					goto unlock;
				if (!pvnode->fill_mmu_chnl)
					continue;
				ret = pvnode->fill_mmu_chnl(pcam, &isp_vb->vb,
					num_planes);
				if (ret < 0) {
					d_inf(1, "%s: failed to config MMU channel",
						vnode->vdev.name);
					goto unlock;
				}
			}
			nr_out = find_first_bit(&lpipe->cur_cmd->output_map,
					BITS_PER_LONG) + 1;
			if (nr_out >= BITS_PER_LONG)
				nr_out = 0;
			if (vnode->buf_type ==
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				lpipe->cur_cmd->output[nr_out].vnode = vnode;
				lpipe->cur_cmd->output[nr_out].pix_mp =
					vnode->format.fmt.pix_mp;
				lpipe->cur_cmd->output[nr_out].buf[0] =
					isp_vnode_find_busy_buffer(vnode, 0);
				lpipe->cur_cmd->output[nr_out].nr_buffer = 1;
				lpipe->cur_cmd->output_map =
					(lpipe->cur_cmd->output_map << 1) + 1;
			} else {
				lpipe->cur_cmd->mem.vnode = vnode;
				lpipe->cur_cmd->mem.axi_id = mac_id;
				for (i = 0;; i++) {
					isp_vb = isp_vnode_find_busy_buffer(
						vnode, i);
					if (isp_vb == NULL)
						break;
					lpipe->cur_cmd->mem.buf[i] = isp_vb;
				}
			}

			/* Check if all output and input are ready */
			if (lpipe->cur_cmd->mem.vnode == NULL) {
				d_inf(3, "%s: input not ready",
					topo->path->subdev.name);
				ret = 0;
				goto unlock;
			}

			for (i = 0; topo->dst[i]; i++) {
				if (topo->dst[i]->state < ISP_VNODE_ST_WORK)
					continue;
				if (topo->dst[i] == vnode)
					continue;
				nr_out--;
			}
			/*
			 * If number of stream_on output and number of buffer
			 * ready outputs not match, refuse to send command
			 */
			if (nr_out) {
				d_inf(3, "%s: at least one output not ready",
					topo->path->subdev.name);
				ret = 0;
				goto unlock;
			}
			ret = b52isp_try_apply_cmd(lpipe);
			if (ret < 0)
				goto unlock;
			b52isp_export_cmd_buffer(lpipe->cur_cmd);
			lpipe->cur_cmd->mem.vnode = NULL;
			memset(lpipe->cur_cmd->output, 0,
				sizeof(lpipe->cur_cmd->output));
			lpipe->cur_cmd->output_map = 0;

			goto unlock;
		default:
			d_inf(1, "TODO: add stream on support of %s in command %d",
				laxi->isd.subdev.name,
				lpipe->cur_cmd->cmd_name);
			break;
		}

		/* Begin to accept H/W IRQ */
		local_irq_save(irq_flags);
		switch (port) {
		case B52AXI_PORT_W1:
			ret = atomic_notifier_chain_register(
					&paxi->irq_w1.head, &b52isp_mac_irq_nb);
			break;
		case B52AXI_PORT_W2:
			ret = atomic_notifier_chain_register(
					&paxi->irq_w2.head, &b52isp_mac_irq_nb);
			break;
		case B52AXI_PORT_R1:
			ret = atomic_notifier_chain_register(
					&paxi->irq_r1.head, &b52isp_mac_irq_nb);
			break;
		default:
			WARN_ON(1);
			break;
		}
		b52_clear_overflow_flag(laxi->mac, laxi->port);
		local_irq_restore(irq_flags);
unlock:
		mutex_unlock(&lpipe->state_lock);
	}

	if (ret < 0)
		goto disable_axi;

	return ret;

stream_off:
	/* stream off must happen before unmap LAXI and PAXI */
	if (!laxi->stream) {
		int mac_id;
		struct isp_block *blk = isp_sd2blk(&laxi->isd);

		if (blk == NULL)
			return 0;
		paxi = container_of(blk, struct b52isp_paxi, blk);
		mac_id = laxi->mac;
		port = laxi->port;

		if (vnode->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			clear_bit(out_id, &lpipe->cur_cmd->enable_map);

		switch (lpipe->cur_cmd->cmd_name) {
		case CMD_TEST:
		case CMD_RAW_DUMP:
		case CMD_IMG_CAPTURE:
		case CMD_RAW_PROCESS:
		case CMD_HDR_STILL:
			break;
		case CMD_CHG_FORMAT:
		case CMD_SET_FORMAT:
			mutex_lock(&lpipe->state_lock);
			if (!lpipe->cur_cmd->enable_map)
				lpipe->cur_cmd->cmd_name = CMD_SET_FORMAT;
			/* set the stream off flag for isp cmd*/
			lpipe->cur_cmd->flags |= BIT(CMD_FLAG_STREAM_OFF);
			ret = b52isp_try_apply_cmd(lpipe);
			if (lpipe->cur_cmd->cmd_name == CMD_SET_FORMAT)
				laxi->dma_state = B52DMA_HW_NO_STREAM;

			lpipe->cur_cmd->flags &= ~BIT(CMD_FLAG_STREAM_OFF);
			if (ret < 0) {
				d_inf(1, "apply change cmd failed on port:%d\n",
					out_id);
				goto stream_data_off;
			}
stream_data_off:
			mutex_unlock(&lpipe->state_lock);
			break;
		default:
			d_inf(1, "TODO: add stream off support of %s in command %d",
				laxi->isd.subdev.name,
				lpipe->cur_cmd->cmd_name);
			break;
		}

disable_axi:
		b52_enable_axi_port(laxi, 0, vnode);
		b52isp_set_ddr_threshold(&laxi->parent->work, 0);

		/* stream off sensor and csi */
		if ((lpipe->cur_cmd->cmd_name != CMD_RAW_PROCESS) &&
			!(lpipe->cur_cmd->flags & BIT(CMD_FLAG_MS))) {
			struct v4l2_subdev *sd = lpipe->cur_cmd->sensor;
			struct media_pad *csi_pad = media_entity_remote_pad(sd->entity.pads);
			struct v4l2_subdev *csi_sd = media_entity_to_v4l2_subdev(csi_pad->entity);

			v4l2_subdev_call(sd, video, s_stream, 0);
			v4l2_subdev_call(csi_sd, video, s_stream, 0);
		}

		if (laxi->stream && ret < 0)
			goto unmap_scalar;

		switch (lpipe->cur_cmd->cmd_name) {
		case CMD_IMG_CAPTURE:
		case CMD_RAW_PROCESS:
		case CMD_HDR_STILL:
			break;
		default:
			/* Stop listening to IRQ */
			switch (port) {
			case B52AXI_PORT_W1:
				ret = atomic_notifier_chain_unregister(
					&paxi->irq_w1.head, &b52isp_mac_irq_nb);
				WARN_ON(ret < 0);
				break;
			case B52AXI_PORT_W2:
				ret = atomic_notifier_chain_unregister(
					&paxi->irq_w2.head, &b52isp_mac_irq_nb);
				WARN_ON(ret < 0);
				break;
			case B52AXI_PORT_R1:
				ret = atomic_notifier_chain_unregister(
					&paxi->irq_r1.head, &b52isp_mac_irq_nb);
				WARN_ON(ret < 0);
				mm_stream = 0;
				break;
			default:
				WARN_ON(1);
			}
			break;
		}

	}

unmap_scalar:
	/* Unmap Logical AXI and Physical AXI */
	if (laxi == topo->scalar_a->drv_priv)
		ret |= b52_map_axi_port(laxi, 0, topo);
	else {
		/* If all output mapped, find the last post scalar */
		sid = 0;
		while (topo->scalar_b[sid])
			sid++;
unmap_scalar_b:
		for (sid--; sid >= 0; sid--)
			ret |= b52_map_axi_port(topo->scalar_b[sid]->drv_priv,
					 0, topo);
	}

	return ret;
}

static int b52isp_laxi_close_handler(struct b52isp_laxi *laxi,
					struct plat_vnode *pvnode)
{
	struct b52isp_lpipe *pipe = pvnode->plat_topo->path->drv_priv;

	mutex_lock(&pipe->state_lock);

	BUG_ON(atomic_read(&pipe->cmd_ref) == 0);
	if (atomic_dec_return(&pipe->cmd_ref) == 0) {
		kfree(pipe->plat_topo);
		pipe->plat_topo = NULL;
	}
	pvnode->plat_topo = NULL;

	mutex_unlock(&pipe->state_lock);

	d_inf(3, "close %s", pvnode->vnode.vdev.name);
	return 0;
}

static int b52isp_laxi_open_handler(struct b52isp_laxi *laxi,
					struct plat_vnode *pvnode)
{
	struct b52isp_lpipe *pipe = NULL;
	struct b52isp_cmd_mapping_item *item;
	struct isp_vnode *vnode = &pvnode->vnode;
	struct plat_topology topo;
	int ret;

	ret = plat_vnode_get_topology(pvnode, &topo);
	if (ret < 0)
		return ret;
	pipe = topo.path->drv_priv;

	mutex_lock(&pipe->state_lock);

	if (atomic_inc_return(&pipe->cmd_ref) == 1) {
		pipe->plat_topo = kzalloc(sizeof(topo), GFP_KERNEL);
		if (unlikely(WARN_ON(pipe->plat_topo == NULL))) {
			ret = -ENOMEM;
			goto err_alloc;
		}
		/* continue to setup pipeline outputs */
		plat_topo_get_output(&topo);
		*pipe->plat_topo = topo;
	} else
		BUG_ON(pipe->plat_topo->path != topo.path);

	pvnode->plat_topo = pipe->plat_topo;

	WARN_ON(pipe->cur_cmd == NULL);
	/* setup MCU command for vdev part */
	if (pipe->isd.sd_code < B52ISP_ISD_HS) {
		item = &b52_scmd_table[topo.src_type][pipe->path_arg.aeag]
				[pipe->isd.sd_code - SDCODE_B52ISP_PIPE1];
		if (vnode->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			/* output vdev */
			if (item->cmd_name == CMD_IMG_CAPTURE)
				vnode->hw_min_buf = pipe->path_arg.nr_frame;
			else
				vnode->hw_min_buf = item->output_min_buf;
		} else {
			/* input vdev */
			vnode->hw_min_buf = item->input_min_buf;
		}
	} else {
		item = &b52_dcmd_table[topo.src_type][pipe->path_arg.combo];
		if (vnode->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			/* output vdev */
			vnode->hw_min_buf = item->output_min_buf;
		} else {
			/* input vdev */
			vnode->hw_min_buf = item->input_min_buf;
		}
	}

	mutex_unlock(&pipe->state_lock);

	d_inf(3, "open %s (path:%s, hw_min_buf:%d)", vnode->vdev.name,
		topo.path->subdev.name, vnode->hw_min_buf);

	return 0;

err_alloc:
	atomic_dec(&pipe->cmd_ref);
	mutex_unlock(&pipe->state_lock);
	return ret;
}

static int b52isp_video_event(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct plat_vnode *pvnode = container_of(data,
					struct plat_vnode, vnode);
	struct b52isp_laxi *laxi = pvnode->vnode.notifier.priv;
	int ret = 0;

	switch (event) {
	case VDEV_NOTIFY_STM_ON:
		ret = b52isp_laxi_stream_handler(laxi, pvnode, 1);
		break;
	case VDEV_NOTIFY_STM_OFF:
		ret = b52isp_laxi_stream_handler(laxi, pvnode, 0);
		break;
	case VDEV_NOTIFY_OPEN:
		ret = b52isp_laxi_open_handler(laxi, pvnode);
		break;
	case VDEV_NOTIFY_CLOSE:
		ret = b52isp_laxi_close_handler(laxi, pvnode);
		break;
	case VDEV_NOTIFY_S_FMT:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct notifier_block b52isp_video_nb = {
	.notifier_call = b52isp_video_event,
};

static int b52isp_axi_connect_video(struct isp_subdev *isd,
					struct isp_vnode *vnode)
{
	struct b52isp_laxi *laxi = isd->drv_priv;
	int ret;

	ret = blocking_notifier_chain_register(&vnode->notifier.head,
			&b52isp_video_nb);
	if (ret < 0)
		return ret;

	vnode->notifier.priv = laxi;

	return 0;
}

static int b52isp_axi_disconnect_video(struct isp_subdev *isd,
					struct isp_vnode *vnode)
{
	int ret;

	ret = blocking_notifier_chain_unregister(&vnode->notifier.head,
			&b52isp_video_nb);
	if (ret < 0)
		return ret;

	return 0;
}

static int b52isp_axi_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct isp_subdev *isd;
	int port;
	/* TODO: MUST assign physical device to logical device!!! */
	if (sd == NULL)
		return -ENODEV;
	isd = v4l2_get_subdev_hostdata(sd);
	if (isd == NULL)
		return -ENODEV;

	switch (isd->sd_code) {
	case SDCODE_B52ISP_A1W1:
	case SDCODE_B52ISP_A2W1:
	case SDCODE_B52ISP_A3W1:
		port = B52AXI_PORT_W1;
		break;
	case SDCODE_B52ISP_A1W2:
	case SDCODE_B52ISP_A2W2:
	case SDCODE_B52ISP_A3W2:
		port = B52AXI_PORT_W2;
		break;
	case SDCODE_B52ISP_A1R1:
	case SDCODE_B52ISP_A2R1:
	case SDCODE_B52ISP_A3R1:
		port = B52AXI_PORT_R1;
		break;
	default:
		BUG_ON(1);
	}

	if (me_to_vnode(remote->entity)) {
		struct isp_vnode *vnode = me_to_vnode(remote->entity);
		/* connect to vdev */
		if (flags & MEDIA_LNK_FL_ENABLED)
			b52isp_axi_connect_video(isd, vnode);
		else
			b52isp_axi_disconnect_video(isd, vnode);
	} else {
		/* connect to pipe */
	}
	return 0;
}

static const struct media_entity_operations b52isp_axi_media_ops = {
	.link_setup = b52isp_axi_link_setup,
};

static int b52isp_axi_sd_open(struct isp_subdev *isd)
{
	return 0;
}

static void b52isp_axi_sd_close(struct isp_subdev *isd)
{
}

struct isp_subdev_ops b52isp_axi_sd_ops = {
	.open		= b52isp_axi_sd_open,
	.close		= b52isp_axi_sd_close,
};

static void b52isp_axi_remove(struct b52isp *b52isp)
{
	struct isp_build *build = NULL;
	int i;

	if (b52isp->isd[B52ISP_ISD_A1W1])
		build = b52isp->isd[B52ISP_ISD_A1W1]->build;

	for (i = 0; i < b52isp->hw_desc->nr_axi; i++) {
		struct isp_block *blk = b52isp->blk[B52ISP_BLK_AXI1 + i];
		struct b52isp_paxi *axi;

		axi = container_of(blk, struct b52isp_paxi, blk);
		tasklet_kill(&axi->tasklet);

		b52isp_detach_blk_isd(b52isp->isd[B52ISP_ISD_A1W1 + 3 * i + 0],
					blk);
		b52isp_detach_blk_isd(b52isp->isd[B52ISP_ISD_A1W1 + 3 * i + 1],
					blk);
		b52isp_detach_blk_isd(b52isp->isd[B52ISP_ISD_A1W1 + 3 * i + 2],
					blk);
		devm_kfree(b52isp->dev,
			container_of(blk, struct b52isp_paxi, blk));
		b52isp->blk[B52ISP_BLK_AXI1 + i] = NULL;
	}
	for (i = 0; i < b52isp->hw_desc->nr_axi * 3; i++) {
		struct isp_subdev *isd = b52isp->isd[B52ISP_ISD_A1W1 + i];
		plat_ispsd_unregister(isd);
		v4l2_ctrl_handler_free(&isd->ctrl_handler);
		devm_kfree(b52isp->dev,
			container_of(isd, struct b52isp_laxi, isd));
		b52isp->blk[B52ISP_ISD_A1W1 + i] = NULL;
	}
}

static int b52isp_axi_create(struct b52isp *b52isp)
{
	struct isp_build *build;
	int i, ret = 0;

	/*
	 * B52ISP AXI master is the tricky part: physically, there is only 2/3
	 * AXI master, but each AXI master has 2 write port and 1 read port.
	 * So at logical level, we expose each R/W port as a logical device,
	 * the mapping between p_dev and l_dev is 1:3. The more, the mapping
	 * relationship is dynamic, because we need to carefully select which
	 * AXI ports to use so as to balance bandwidth requirement between AXI
	 * masters and minimize power.
	 */

	for (i = 0; i < b52isp->hw_desc->nr_axi; i++) {
		struct b52isp_paxi *axi =
			devm_kzalloc(b52isp->dev, sizeof(*axi), GFP_KERNEL);
		struct isp_block *block;

		if (unlikely(axi == NULL))
			return -ENOMEM;
		block = &axi->blk;

		block->id.dev_type = PCAM_IP_B52ISP;
		block->id.dev_id = b52isp->dev->id;
		block->id.mod_id = B52ISP_BLK_AXI1 + i;
		block->dev = b52isp->dev;
		block->name = b52isp_block_name[B52ISP_BLK_AXI1 + i];
		block->req_list = b52axi_req;
		block->ops = &b52isp_axi_hw_ops;
		b52isp->blk[B52ISP_BLK_AXI1 + i] = block;
		axi->parent = b52isp;
	}
	/* isp-subdev common part setup */
	for (i = 0; i < b52isp->hw_desc->nr_axi * 3; i++) {
		struct b52isp_laxi *axi =
			devm_kzalloc(b52isp->dev, sizeof(*axi), GFP_KERNEL);
		struct isp_subdev *ispsd;
		struct v4l2_subdev *sd;

		if (axi == NULL)
			return -ENOMEM;
		/* Add ISP AXI Master isp-subdev */
		ispsd = &axi->isd;
		sd = &ispsd->subdev;
		ret = v4l2_ctrl_handler_init(&ispsd->ctrl_handler, 16);
		if (unlikely(ret < 0))
			return ret;
		sd->entity.ops = &b52isp_axi_media_ops;
		v4l2_subdev_init(sd, &b52isp_axi_subdev_ops);
		sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		sd->ctrl_handler = &ispsd->ctrl_handler;
		strncpy(sd->name, b52isp_ispsd_name[B52ISP_ISD_A1W1 + i],
			sizeof(sd->name));
		ispsd->ops = &b52isp_axi_sd_ops;
		ispsd->pads[0].flags = MEDIA_PAD_FL_SINK;
		ispsd->pads[1].flags = MEDIA_PAD_FL_SOURCE;
		ispsd->pads_cnt = 2;
		ispsd->single = 1;
		INIT_LIST_HEAD(&ispsd->gdev_list);
		ispsd->sd_code = SDCODE_B52ISP_A1W1 + i;
		ispsd->drv_priv = axi;
		b52isp->isd[B52ISP_ISD_A1W1 + i] = ispsd;
		axi->parent = b52isp;
		axi->dma_state = B52DMA_HW_NO_STREAM;
		axi->drop_cnt = 0;
		axi->mac = -1;
		axi->port = -1;
	}
	for (i = 0; i < b52isp->hw_desc->nr_axi; i++) {
		struct isp_subdev *ispsd;

		ispsd = b52isp->isd[B52ISP_ISD_A1W1 + 3 * i + 0];
		ispsd->sd_type = ISD_TYPE_DMA_OUT;

		ret = plat_ispsd_register(ispsd);
		if (ret < 0)
			return ret;

		ispsd = b52isp->isd[B52ISP_ISD_A1W1 + 3 * i + 1];
		ispsd->sd_type = ISD_TYPE_DMA_OUT;
		ret = plat_ispsd_register(ispsd);
		if (ret < 0)
			return ret;

		ispsd = b52isp->isd[B52ISP_ISD_A1W1 + 3 * i + 2];
		ispsd->sd_type = ISD_TYPE_DMA_IN;
		ret = b52isp_attach_blk_isd(ispsd,
				b52isp->blk[B52ISP_BLK_AXI1 + i]);
		if (unlikely(ret < 0))
			return ret;
		ret = plat_ispsd_register(ispsd);
		if (ret < 0)
			return ret;
		ret = b52isp_detach_blk_isd(ispsd,
				b52isp->blk[B52ISP_BLK_AXI1 + i]);
		if (unlikely(ret < 0))
			return ret;
		build = ispsd->build;
	}
	/* after register to platform camera manager, can use SoCISP functions*/
	for (i = 0; i < b52isp->hw_desc->nr_axi; i++) {
		struct isp_block *blk = b52isp->blk[B52ISP_BLK_AXI1 + i];
		struct b52isp_paxi *axi;

		if (blk == NULL)
			continue;
		axi = container_of(blk, struct b52isp_paxi, blk);
		/* register irq events used to broadcase irq to ports */
		ATOMIC_INIT_NOTIFIER_HEAD(&axi->irq_w1.head);
		ATOMIC_INIT_NOTIFIER_HEAD(&axi->irq_w2.head);
		ATOMIC_INIT_NOTIFIER_HEAD(&axi->irq_r1.head);
		axi->id = i;
		spin_lock_init(&axi->lock);
		tasklet_init(&axi->tasklet, b52isp_tasklet, (unsigned long)axi);
	}
	return 0;
}

/***************************** The B52ISP subdev *****************************/
static void b52isp_cleanup(struct b52isp *b52isp)
{
	b52isp_axi_remove(b52isp);
	b52isp_path_remove(b52isp);
	b52isp_idi_remove(b52isp);
}

#define b52_add_link(isp, src, spad, dst, dpad) \
do { \
	ret = media_entity_create_link( \
		&(isp)->isd[(src)]->subdev.entity, (spad), \
		&(isp)->isd[(dst)]->subdev.entity, (dpad), 0); \
	if (ret < 0) {\
		d_inf(1, "failed to create link[%s:%d]==>[%s:%d]", \
			(isp)->isd[(src)]->subdev.entity.name, (spad), \
			(isp)->isd[(dst)]->subdev.entity.name, (dpad)); \
		goto exit_err; \
	} \
} while (0)

static int b52isp_setup(struct b52isp *b52isp)
{
	int i, ret;

	b52isp->mac_map = 0;
	mutex_init(&b52isp->mac_map_lock);

	ret = b52isp_idi_create(b52isp);
	if (unlikely(ret < 0))
		goto exit_err;
	/* Number of pipeline depends on ISP version */
	ret = b52isp_path_create(b52isp);
	if (unlikely(ret < 0))
		goto exit_err;

	/* Number of AXI master depends on ISP version */
	ret = b52isp_axi_create(b52isp);
	if (unlikely(ret < 0))
		goto exit_err;

	/* First, the single pipeline links */
	for (i = 0; i < b52isp->hw_desc->idi_port; i++) {
		b52_add_link(b52isp, B52ISP_ISD_IDI1 + i, B52PAD_IDI_PIPE1,
			B52ISP_ISD_PIPE1, B52PAD_PIPE_IN);
		if (b52isp->hw_desc->idi_dump == 0)
			continue;
		b52_add_link(b52isp, B52ISP_ISD_IDI1 + i, B52PAD_IDI_DUMP1,
			B52ISP_ISD_DUMP1, B52PAD_PIPE_IN);
	}

	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_PIPE1, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_MS1, B52PAD_MS_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS1, B52PAD_MS_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS1, B52PAD_MS_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);

	if (b52isp->hw_desc->nr_axi < 2)
		return 0;

	/* for two axi links */
	b52_add_link(b52isp, B52ISP_ISD_IDI1, B52PAD_IDI_DUMP1,
			B52ISP_ISD_DUMP1, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_PIPE1, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_MS1, B52PAD_MS_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS1, B52PAD_MS_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS1, B52PAD_MS_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);

	if (b52isp->hw_desc->nr_pipe < 2)
		return 0;
	/* Then, the double pipeline links */
	for (i = 0; i < b52isp->hw_desc->idi_port; i++) {
		b52_add_link(b52isp, B52ISP_ISD_IDI1 + i, B52PAD_IDI_PIPE2,
			B52ISP_ISD_PIPE2, B52PAD_PIPE_IN);
		if (b52isp->hw_desc->idi_dump == 0)
			continue;
		b52_add_link(b52isp, B52ISP_ISD_IDI1 + i, B52PAD_IDI_DUMP2,
			B52ISP_ISD_DUMP2, B52PAD_PIPE_IN);
	}

	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_PIPE1, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_PIPE2, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_PIPE2, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_PIPE2, B52PAD_PIPE_IN);

	b52_add_link(b52isp, B52ISP_ISD_PIPE1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP1, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);

	b52_add_link(b52isp, B52ISP_ISD_PIPE2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_PIPE2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);

	b52_add_link(b52isp, B52ISP_ISD_DUMP2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_DUMP2, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);

	/* Last, the logical pipeline links */
	b52_add_link(b52isp, B52ISP_ISD_IDI1, B52PAD_IDI_BOTH,
			B52ISP_ISD_HS, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_IDI1, B52PAD_IDI_BOTH,
			B52ISP_ISD_HDR, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_IDI1, B52PAD_IDI_BOTH,
			B52ISP_ISD_3D, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_IDI2, B52PAD_IDI_BOTH,
			B52ISP_ISD_HS, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_IDI2, B52PAD_IDI_BOTH,
			B52ISP_ISD_HDR, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_IDI2, B52PAD_IDI_BOTH,
			B52ISP_ISD_3D, B52PAD_PIPE_IN);

	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_HS, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_HS, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_HS, B52PAD_PIPE_IN);

	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_HDR, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_HDR, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_HDR, B52PAD_PIPE_IN);

	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_3D, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_3D, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_3D, B52PAD_PIPE_IN);

	b52_add_link(b52isp, B52ISP_ISD_HS, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HS, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HS, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HS, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HS, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HS, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);

	b52_add_link(b52isp, B52ISP_ISD_HDR, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HDR, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HDR, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HDR, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HDR, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_HDR, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);

	b52_add_link(b52isp, B52ISP_ISD_3D, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_3D, B52PAD_PIPE_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_3D, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_3D, B52PAD_PIPE_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_3D, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_3D, B52PAD_PIPE_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);


	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_MS1, B52PAD_PIPE_IN);
	b52_add_link(b52isp, B52ISP_ISD_A1R1, B52PAD_AXI_OUT,
			B52ISP_ISD_MS2, B52PAD_MS_IN);
	b52_add_link(b52isp, B52ISP_ISD_A2R1, B52PAD_AXI_OUT,
			B52ISP_ISD_MS2, B52PAD_MS_IN);
	b52_add_link(b52isp, B52ISP_ISD_A3R1, B52PAD_AXI_OUT,
			B52ISP_ISD_MS2, B52PAD_MS_IN);

	b52_add_link(b52isp, B52ISP_ISD_MS1, B52PAD_MS_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS1, B52PAD_MS_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);

	b52_add_link(b52isp, B52ISP_ISD_MS2, B52PAD_MS_OUT,
			B52ISP_ISD_A1W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS2, B52PAD_MS_OUT,
			B52ISP_ISD_A1W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS2, B52PAD_MS_OUT,
			B52ISP_ISD_A2W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS2, B52PAD_MS_OUT,
			B52ISP_ISD_A2W2, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS2, B52PAD_MS_OUT,
			B52ISP_ISD_A3W1, B52PAD_AXI_IN);
	b52_add_link(b52isp, B52ISP_ISD_MS2, B52PAD_MS_OUT,
			B52ISP_ISD_A3W2, B52PAD_AXI_IN);
	return 0;

exit_err:
	b52isp_cleanup(b52isp);
	return ret;
}

/***************************** The B52ISP IP Core *****************************/
static void b52isp_tasklet(unsigned long data)
{
	int ret;
	u32 event;
	unsigned long msg;
	unsigned long irq_flags;
	struct b52isp_paxi *paxi = (struct b52isp_paxi *)data;

	spin_lock_irqsave(&paxi->lock, irq_flags);
	event = paxi->event;
	paxi->event = 0;
	spin_unlock_irqrestore(&paxi->lock, irq_flags);

	if (event & 0x0F) {
		msg = (paxi->id << MAC_ID_SHIFT) |
			(B52AXI_PORT_W1 << PORT_ID_SHIFT) |
			((event >> 0) & 0xF);
		ret = atomic_notifier_call_chain(&paxi->irq_w1.head,
					msg, paxi->isd[B52AXI_PORT_W1]);
		if (ret < 0)
			d_inf(3, "notifer error for w1: %d", ret);
	}

	if (event & 0xF0) {
		msg = (paxi->id << MAC_ID_SHIFT) |
			(B52AXI_PORT_W2 << PORT_ID_SHIFT) |
			((event >> 4) & 0xF);
		ret = atomic_notifier_call_chain(&paxi->irq_w2.head,
					msg, paxi->isd[B52AXI_PORT_W2]);
		if (ret < 0)
			d_inf(3, "notifer error for w2: %d", ret);
		}

	if (event & 0xF00) {
		msg = (paxi->id << MAC_ID_SHIFT) |
			(B52AXI_PORT_R1 << PORT_ID_SHIFT) |
			((event >> 8) & 0xF);
		ret = atomic_notifier_call_chain(&paxi->irq_r1.head,
					msg, paxi->isd[B52AXI_PORT_R1]);
		if (ret < 0)
			d_inf(3, "notifer error for r1: %d", ret);
	}
}


static irqreturn_t b52isp_irq_handler(int irq, void *data)
{
	int i;
	u32 event[3];
	struct b52isp *b52isp = data;
	struct b52isp_paxi *paxi;

	b52_ack_xlate_irq(event, b52isp->hw_desc->nr_axi, &b52isp->work);

	for (i = 0; i < b52isp->hw_desc->nr_axi; i++) {
		if (event[i] == 0)
			continue;

		paxi = container_of(b52isp->blk[B52ISP_BLK_AXI1 + i],
							struct b52isp_paxi, blk);

		spin_lock(&paxi->lock);
		if (paxi->event & event[i])
			d_inf(2, "mac bit already set,%x:%x", paxi->event, event[i]);
		paxi->event |= event[i];
		spin_unlock(&paxi->lock);

		tasklet_hi_schedule(&paxi->tasklet);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id b52isp_dt_match[] = {
	{
		.compatible = "ovt,single-pipeline ISP",
		.data = b52isp_hw_table + B52ISP_SINGLE,
	},
	{
		.compatible = "ovt,double-pipeline ISP",
		.data = b52isp_hw_table + B52ISP_DOUBLE,
	},
	{
		.compatible = "ovt,single-pipeline ISP(Lite)",
		.data = b52isp_hw_table + B52ISP_LITE,
	},
	{},
};
MODULE_DEVICE_TABLE(of, b52isp_dt_match);

static int b52isp_probe(struct platform_device *pdev)
{
	int count;
	struct b52isp *b52isp;
	const struct of_device_id *of_id =
				of_match_device(b52isp_dt_match, &pdev->dev);
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	struct resource clk;
	struct block_id pdev_mask = {
		.dev_type = PCAM_IP_B52ISP,
		.dev_id = pdev->dev.id,
		.mod_type = 0xFF,
		.mod_id = 0xFF,
	};
	int ret, i, nr_axi;

	b52isp = devm_kzalloc(&pdev->dev, sizeof(struct b52isp),
				GFP_KERNEL);
	if (unlikely(b52isp == NULL)) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	meta_cpu = dmam_alloc_coherent(&pdev->dev, META_DATA_SIZE,
				&meta_dma, GFP_KERNEL);
	if (unlikely(meta_cpu == NULL))
		dev_err(&pdev->dev, "could not allocate meta buffer\n");

	if (unlikely(of_id == NULL)) {
		dev_err(&pdev->dev, "failed to find matched device\n");
		return -ENODEV;
	}
	b52isp->hw_desc = of_id->data;
	dev_info(&pdev->dev, "Probe OVT ISP with %d pipeline, %d AXI Master\n",
		b52isp->hw_desc->nr_pipe, b52isp->hw_desc->nr_axi);

	for (i = 0; i < b52isp->hw_desc->nr_axi * 3; i += 3) {
		for (nr_axi = 0; nr_axi < b52isp->hw_desc->nr_axi; nr_axi++) {
			output_mask[i + 0] |= mac_wport_mask(nr_axi);
			output_mask[i + 1] |= mac_wport_mask(nr_axi);
			output_mask[i + 2] |= mac_rport_mask(nr_axi);
		}
	}

	platform_set_drvdata(pdev, b52isp);
	b52isp->dev = &pdev->dev;

	/* register all platform resource to map manager */
	for (i = 0;; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL)
			break;
		ret = plat_resrc_register(&pdev->dev, res, "b52isp-reg",
			pdev_mask, i, NULL, NULL);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed register mem resource %s",
				res->name);
			return ret;
		}
	}

	/* get irqs */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource");
		return -ENXIO;
	}
	ret = plat_resrc_register(&pdev->dev, res, B52ISP_IRQ_NAME,
				pdev_mask, 0,
				/* irq handler */
				&b52isp_irq_handler,
				/* irq context*/
				b52isp);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed register irq resource %s",
			res->name);
		return ret;
	}

	/* get clock(s) */
	count = of_property_count_strings(np, "clock-names");
	if (count < 1 || count > ISP_CLK_END) {
		pr_err("%s: clock count error %d\n", __func__, count);
		return -EINVAL;
	}
/* the clocks order in ISP_CLK_END need align in DTS */
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "clock-names",
					    i, &clock_name[i]);
		if (ret) {
			pr_err("%s: unable to get clock %d\n", __func__, count);
			return -ENODEV;
		}
	}

	clk.flags = ISP_RESRC_CLK;
	for (i = 0; i < ISP_CLK_END; i++) {
		clk.name = clock_name[i];
		ret = plat_resrc_register(&pdev->dev, &clk, NULL, pdev_mask,
						i, NULL, NULL);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed register clock resource %s",
				res->name);
			return ret;
		}
	}

	ddrfreq_qos_req_min.name = B52ISP_DRV_NAME;
	pm_qos_add_request(&ddrfreq_qos_req_min,
				PM_QOS_DDR_DEVFREQ_MIN,
				PM_QOS_DEFAULT_VALUE);

	ret = b52isp_setup(b52isp);
	if (unlikely(ret < 0)) {
		dev_err(&pdev->dev, "failed to break down %s into isp-subdev\n",
			B52ISP_NAME);
		return ret;
	}

	srcu_init_notifier_head(&b52isp->nh);
	INIT_WORK(&b52isp->work, b52isp_ddr_threshold_work);
#ifdef CONFIG_DEVFREQ_GOV_THROUGHPUT
	camfeq_register_dev_notifier(&b52isp->nh);
#endif

	pm_runtime_enable(b52isp->dev);

	return 0;
}

static int b52isp_remove(struct platform_device *pdev)
{
	struct b52isp *b52isp = platform_get_drvdata(pdev);

	cancel_work_sync(&b52isp->work);
	pm_runtime_disable(b52isp->dev);
	dmam_free_coherent(b52isp->dev, META_DATA_SIZE, meta_cpu, meta_dma);
	devm_kfree(b52isp->dev, b52isp);
	return 0;
}

struct platform_driver b52isp_driver = {
	.driver = {
		.name	= B52ISP_DRV_NAME,
		.of_match_table = of_match_ptr(b52isp_dt_match),
	},
	.probe	= b52isp_probe,
	.remove	= b52isp_remove,
};

module_platform_driver(b52isp_driver);

MODULE_AUTHOR("Jiaquan Su <jqsu@marvell.com>");
MODULE_DESCRIPTION("Marvell B52 ISP driver, based on soc-isp framework");
MODULE_LICENSE("GPL");
