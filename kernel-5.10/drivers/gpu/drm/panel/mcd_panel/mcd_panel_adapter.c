// SPDX-License-Identifier: GPL-2.0-only
/*
 * MIPI-DSI based Samsung common panel driver.
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <drm/drm_panel.h>
#include <drm/drm_encoder.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>
#include <drm/drm_mipi_dsi.h>

#include "mcd_panel_adapter.h"
#include "mcd_panel_adapter_helper.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#undef OLD_DRM
#else
#define OLD_DRM
#endif

static int mcd_drm_parse_dt(void *_ctx, struct device_node *np)
{
	int ret = 0;
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;

	if (!np) {
		pr_err("%s node is null\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "MTK,timing,dsi-hs-clk", &ctx->spec.data_rate);
	dev_info(ctx->dev, "%s: data_rate: %d\n", __func__, ctx->spec.data_rate );

	ret = of_property_read_u32(np, "MTK,data_lane", &ctx->spec.data_lane_cnt);
	dev_info(ctx->dev, "%s: data_lane_cnt: %d\n", __func__, ctx->spec.data_lane_cnt);

	ret = of_property_read_u32_array(np, "MTK,size", ctx->spec.size_mm, 2);
	dev_info(ctx->dev, "%s: w: %d h: %d\n", __func__, ctx->spec.size_mm[0], ctx->spec.size_mm[1]);

	ret = of_property_read_u32(np, "MTK,multi_drop", &ctx->spec.multi_drop);
	dev_info(ctx->dev, "%s: multi_drop: %d\n", __func__, ctx->spec.multi_drop);

	return 0;
}

static void print_tx(u8 cmd_id, const u8 *cmd, int size)
{
	char data[256];
	int i, len;
	bool newline = false;

	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", cmd_id);
	for (i = 0; i < min_t(int, size, 256); i++) {
		if (newline)
			len += snprintf(data + len, ARRAY_SIZE(data) - len, "     ");
		newline = (!((i + 1) % 16) || (i + 1 == size)) ? true : false;
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X%s", cmd[i], newline ? "\n" : " ");
		if (newline) {
			pr_info("%s: %s", __func__, data);
			len = 0;
		}
	}
}

static void print_cmd_set(const struct cmd_set *cmd_set, int size)
{
	int i;

	for (i = 0; i < size; i++)
		print_tx(cmd_set[i].cmd_id,
				cmd_set[i].buf,
				cmd_set[i].size);
}

static inline unsigned char get_mipi_dsi_cmd_type(u8 cmd_id, int size)
{
	if (cmd_id == MIPI_DSI_WR_PPS_CMD)
		return MIPI_DSI_PICTURE_PARAMETER_SET;
	else if (cmd_id == MIPI_DSI_WR_DSC_CMD)
		return MIPI_DSI_COMPRESSION_MODE;

	if (size == 1)
		return MIPI_DSI_DCS_SHORT_WRITE;
	else if (size == 2)
		return MIPI_DSI_DCS_SHORT_WRITE_PARAM;

	return MIPI_DSI_DCS_LONG_WRITE;
}

/* Max command set			(MTK S/W define) */
#define MAX_CMDSET_NUM (MAX_TX_CMD_NUM)

/* Max paylaod size of SFR		(MTK H/W define) */
#define MAX_HW_TX_FIFO_SIZE (512)

/* Panel_drv's CMD_QUEUE MAX size	(MCD S/W define) */
#define MAX_CMD_SET_SIZE (MAX_PANEL_CMD_QUEUE)


int mtk_drm_cmdset_cleanup(struct mtk_panel *ctx)
{
	memset(&ctx->mtk_dsi_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	return 0;
}

int mtk_drm_cmdset_add(struct mtk_panel *ctx, struct mipi_dsi_device *dsi, u8 type, const u8 *data, size_t size)
{
	int index;
	struct mtk_ddic_dsi_msg *dsi_msg = &ctx->mtk_dsi_msg;

	if (data == NULL || size <= 0)
		return -EINVAL;

	/*
	 * Why we add "4".
	 * MTK DSI tx SFR(long write):
	 * First 4byte is reserved for  configuration.
	 * [Config] + [DataType] + [SizeMSB] + [SizeLSB]
	 */


	if (size + 4 >= MAX_HW_TX_FIFO_SIZE) {
		dev_err(ctx->dev, "%s fifo sfr size limit over. (size:%d max:%d) (type:0x%02x cmd:0x%02x)\n",
			__func__, size, MAX_HW_TX_FIFO_SIZE, type, data[0]);
		BUG();
	}

	index = dsi_msg->tx_cmd_num;

	dsi_msg->type[index] = type;
	dsi_msg->tx_len[index] = size;
	dsi_msg->tx_buf[index] = (void *)data;
	dsi_msg->tx_cmd_num++;
	dsi_msg->flags |= (dsi->mode_flags & MIPI_DSI_MODE_LPM) ? MIPI_DSI_MSG_USE_LPM : 0;

	dev_dbg(ctx->dev, "%s index: %d tx_cmd_num: %d flags:%x\n",
			__func__, index, dsi_msg->tx_cmd_num, dsi_msg->flags);

	return 0;
}

int mtk_drm_cmdset_flush(struct mtk_panel *ctx, bool wait_done)
{
	int ret;
	int set_lcm_blocking = 0;

	if (!ctx->mtk_dsi_msg.tx_cmd_num) {
		dev_err(ctx->dev, "there is no MIPI command to transfer\n");
		return -EINVAL;
	}

	/* in video mode, blocking => blocking_nowait (mutex early release) */
	if (wait_done && (ctx->dsi->mode_flags & MIPI_DSI_MODE_VIDEO))
		set_lcm_blocking = SET_LCM_BLOCKING_NOWAIT;
	else if (wait_done)
		set_lcm_blocking = SET_LCM_BLOCKING;

	ret = set_lcm_wrapper(&ctx->mtk_dsi_msg, set_lcm_blocking);
	if (ret < 0) {
		dev_err(ctx->dev, "%s failed to write %d\n", __func__, ret);
		return ret;
	}

	mtk_drm_cmdset_cleanup(ctx);

	return ret;
}

static int mtk_dsi_write(struct mtk_panel *ctx, struct mipi_dsi_device *dsi,
			u8 cmd_id, const u8 *buf, int len, bool wait_done)
{
	int ret;

	if (!dsi) {
		pr_err("%s: invalid dsi device\n", __func__);
		return -EINVAL;
	}

	if (!dsi->host) {
		dev_err(ctx->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	mtk_drm_cmdset_cleanup(ctx);

	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_tx(cmd_id, buf, len);

	ret = mtk_drm_cmdset_add(ctx, dsi, cmd_id, buf, len);
	if (ret < 0) {
		dev_err(&dsi->dev, "%s failed to write(add) %d\n", __func__, ret);
		return ret;
	}

	ret = mtk_drm_cmdset_flush(ctx, wait_done);
	if (ret < 0) {
		dev_err(&dsi->dev, "%s failed to write(flush) %d\n", __func__, ret);
		return ret;
	}

	/* w/a for mipi_write returned 0 when success, request fix to lsi */
	ret = len;

	if (ret != len)
		dev_warn(&dsi->dev, "%s req %d, written %d bytes\n", __func__, len, ret);

	return len;
}

/*
 * MTK doesn't stack all cmds in fifo at once to transfer burst TX.
 * MTK GCE transfers cmds "one by one" very fastly.
 *
 *	ex)
 *	1. copy cmd[0].buf -> fifo
 *	2. flush fifo
 *	3. copy cmd[1].buf -> fifo
 *	4. flush fifo
 *	.....
 *
 * So, We don't need to count "sum" of all cmd set.
 * But, We should check if "each cmds" size is bigger than fifo size or not.
 */

static int mtk_dsi_write_table(struct mtk_panel *ctx, struct mipi_dsi_device *dsi,
			const struct cmd_set *cmd, int size, u32 option)
{
	int ret;
	bool wait_done = (option & DSIM_OPTION_WAIT_TX_DONE);

	int total_size = 0;
	int i = 0;
	int from = 0;
	s64 elapsed_usec;
	struct timespec64 cur_ts, last_ts, delta_ts;

	if (!dsi) {
		pr_err("%s: invalid dsi device\n", __func__);
		return -EINVAL;
	}

	if (!dsi->host) {
		dev_err(&dsi->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: cmd is null\n", __func__);
		return -EINVAL;
	}

	if (size <= 0) {
		dev_err(&dsi->dev, "invalid cmd size %d\n", size);
		return -EINVAL;
	}

	if (size > MAX_CMD_SET_SIZE) {
		dev_err(&dsi->dev, "exceeded MAX_CMD_SET_SIZE(%d) (size:%d)\n",
				MAX_CMD_SET_SIZE, size);
		return -EINVAL;
	}

	/* 1. Cleanup Buffer */
	mtk_drm_cmdset_cleanup(ctx);

	ktime_get_ts64(&last_ts);

	/* 2. Queue command set Buffer */
	for (i = 0; i < size; i++) {
		if (cmd[i].buf == NULL) {
			dev_err(ctx->dev, "cmd[%d].buf is null\n", i);
			continue;
		}

		/* If FIFO has no space to stack cmd, then flush first */
		if (i - from >= (MAX_CMDSET_NUM - 1)) {

			/* 3. Flush Command Set Buffer */
			if (mtk_drm_cmdset_flush(ctx, wait_done)) {
				dev_err(ctx->dev, "failed to mtk_drm_cmdset_flush\n");
				ret = -EIO;
				goto error;
			}

			if (panel_cmd_log)
				print_cmd_set(&cmd[from], i - from);

			pr_debug("cmd_set:%d\n", i - from);

			from = i;
		}

		if (cmd[i].cmd_id == MIPI_DSI_WR_DSC_CMD) {
			ret = mtk_drm_cmdset_add(ctx, dsi, get_mipi_dsi_cmd_type(cmd[i].cmd_id, cmd[i].size),
					cmd[i].buf, cmd[i].size);
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_PPS_CMD) {
			ret = mtk_drm_cmdset_add(ctx, dsi, get_mipi_dsi_cmd_type(cmd[i].cmd_id, cmd[i].size),
					cmd[i].buf, cmd[i].size);
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_GEN_CMD) {
			ret = mtk_drm_cmdset_add(ctx, dsi, get_mipi_dsi_cmd_type(cmd[i].cmd_id, cmd[i].size),
					cmd[i].buf, cmd[i].size);
		} else {
			dev_err(ctx->dev, "invalid cmd_id %d\n", cmd[i].cmd_id);
			ret = -EINVAL;
			goto error;
		}

		if (ret) {
			dev_err(ctx->dev, "failed to mtk_drm_cmdset_add\n");
			goto error;
		}

		total_size += cmd[i].size;
	}

	/* 3. Flush Command Set Buffer */
	if (mtk_drm_cmdset_flush(ctx, wait_done)) {
		dev_err(ctx->dev, "failed to mtk_drm_cmdset_flush\n");
		ret = -EIO;
		goto error;
	}

	if (panel_cmd_log)
		print_cmd_set(&cmd[from], i - from);

	ktime_get_ts64(&cur_ts);
	delta_ts = timespec64_sub(cur_ts, last_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;

	pr_debug("done (cmd_set:%d size:%d elapsed %2lld.%03lld msec)\n",
			size, total_size,
			elapsed_usec / 1000, elapsed_usec % 1000);

	ret = total_size;
error:
	return ret;

}

static int mtk_dsi_write_gpara(struct mtk_panel *ctx, struct mipi_dsi_device *dsi, u8 addr, u32 offset, u32 option)
{
	u8 gpara[4] = { 0xB0, 0x00, };
	int gpara_len = 1;
	bool wait_tx_done = (option & DSIM_OPTION_WAIT_TX_DONE);

	if (!offset)
		return 0;

	if (option & DSIM_OPTION_2BYTE_GPARA)
		gpara[gpara_len++] = (offset >> 8) & 0xFF;

	gpara[gpara_len++] = offset & 0xFF;
	if (option & DSIM_OPTION_POINT_GPARA)
		gpara[gpara_len++] = addr;

	return mtk_dsi_write(ctx, dsi,
			get_mipi_dsi_cmd_type(MIPI_DSI_WR_GEN_CMD, gpara_len),
			gpara, gpara_len, wait_tx_done);
}

static int mcd_drm_mtk_dsi_write(void *_ctx, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option)
{
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	int ret;
	bool wait_tx_done = (option & DSIM_OPTION_WAIT_TX_DONE);

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: invalid cmd data\n", __func__);
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		dev_err(&dsi->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	ret = mtk_dsi_write_gpara(ctx, dsi, cmd[0], offset, option);
	if (ret < 0) {
		pr_err("%s: failed to write offset\n", __func__);
		return ret;
	}

	return mtk_dsi_write(ctx, dsi, get_mipi_dsi_cmd_type(cmd_id, size),
			cmd, size, wait_tx_done);
}

static int mcd_drm_mtk_dsi_write_table(void *_ctx, const struct cmd_set *cmd, int size, u32 option)
{
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;
	struct mipi_dsi_device *dsi;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: invalid cmd data\n", __func__);
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		dev_err(&dsi->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	return mtk_dsi_write_table(ctx, dsi, cmd, size, option);
}

struct mtk_ddic_dsi_msg rx_cmd_msg;
unsigned char rx_buf[RT_MAX_NUM * 2];

static int mtk_dsi_read(struct mipi_dsi_device *dsi, u8 cmd, u8 *buf, int len)
{
	int ret;
	struct mtk_ddic_dsi_msg *cmd_msg = &rx_cmd_msg;
	unsigned int i = 0, j = 0;
	unsigned long ret_dlen = 0;
	u8 tx[10] = {0};

	if (!dsi) {
		pr_err("%s: invalid dsi device\n", __func__);
		return -EINVAL;
	}

	if (!dsi->host) {
		dev_err(&dsi->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = MIPI_DSI_DCS_READ; /* fix? */
	tx[0] = cmd;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = rx_buf;
	memset(cmd_msg->rx_buf[0], 0, sizeof(rx_buf));
	cmd_msg->rx_len[0] = len;

	ret = read_lcm_wrapper(cmd_msg);

	if (ret < 0) {
		dev_err(&dsi->dev, "%s failed to write %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < (u16)cmd_msg->rx_cmd_num; i++) {
		ret_dlen = cmd_msg->rx_len[i];
		dev_info(&dsi->dev, "read lcm addr:0x%x--dlen:%d--cmd_idx:%d\n",
			*(char *)(cmd_msg->tx_buf[i]), ret_dlen, i);
		for (j = 0; j < ret_dlen; j++) {
			dev_info(&dsi->dev, "read lcm addr:0x%x--byte:%d,val:0x%x\n",
				*(char *)(cmd_msg->tx_buf[i]), j,
				*(char *)(cmd_msg->rx_buf[i] + j));

				buf[j] = *(char *)(cmd_msg->rx_buf[i] + j);
		}
	}

	ret = ret_dlen;

	if (ret != len)
		dev_warn(&dsi->dev, "%s req %d, written %d bytes\n", __func__, len, ret);

	return ret;
}

static int mcd_drm_mtk_dsi_read(void *_ctx, u8 addr, u32 offset, u8 *buf, int size, u32 option)
{
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	int ret;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!buf) {
		pr_err("%s: invalid read buffer\n", __func__);
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	ret = mtk_dsi_write_gpara(ctx, dsi, addr, offset, option | DSIM_OPTION_WAIT_TX_DONE);
	if (ret < 0) {
		pr_err("%s: failed to write offset\n", __func__);
		return ret;
	}

	return mtk_dsi_read(dsi, addr, buf, size);
}

static int mcd_drm_mtk_dsi_get_state(void *_ctx)
{
#if 0 /* Todo */
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		pr_err("%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);

	return (dsim->state == DSIM_STATE_SUSPEND) ?
        CTRL_INTERFACE_STATE_INACTIVE : CTRL_INTERFACE_STATE_ACTIVE;
#endif
	return CTRL_INTERFACE_STATE_ACTIVE;
}

static int mcd_drm_wait_work(void *_ctx, u32 work_idx, u32 timeout)
{
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;
	struct mcd_panel_wq *w;
	unsigned int count;
	int ret;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (work_idx >= MAX_MCD_PANEL_WQ) {
		pr_err("%s: invalid range %d\n", __func__, work_idx);
		return -EINVAL;
	}

	if (!timeout) {
		pr_err("%s: timeout must be greater than 0\n", __func__);
		return -EINVAL;
	}

	w = &ctx->wqs[work_idx];
	if (!w->wq) {
		pr_err("%s: invalid workqueue\n", __func__);
		return -EFAULT;
	}

	count = atomic_read(&w->count);
	if (!queue_delayed_work(w->wq, &w->dwork, msecs_to_jiffies(0))) {
		pr_err("%s failed to queueing work\n", __func__);
		return -EINVAL;
	}

	ret = wait_event_interruptible_timeout(w->wait,
		count < atomic_read(&w->count), msecs_to_jiffies(timeout));
	if (ret == 0) {
		pr_warn("%s timeout %d, %dms\n", __func__, ret, timeout);
		return -ETIMEDOUT;
	}

	return 0;
}

static int mcd_drm_wait_vsync(struct mtk_panel *ctx, u32 timeout)
{
	int ret;

	down_read(&ctx->mcd_drm_state_lock);
	if (ctx->mcd_drm_state != MCD_DRM_STATE_ENABLED) {
		pr_err("%s: panel drm state is invalid\n", __func__);
		ret = -EINVAL;
		goto dec_exit;
	}

	ret = mtk_drm_wait_one_vblank();
	if (ret < 0) {
		pr_err("%s: failed to wait vsync\n", __func__);
		ret = -EINVAL;
		goto dec_exit;
	}

dec_exit:
	up_read(&ctx->mcd_drm_state_lock);

	return ret;
}

static int mcd_drm_wait_frame_start(struct mtk_panel *ctx, u32 timeout)
{
	pr_info("%s ++\n", __func__);
	mtk_wait_frame_start(timeout);
	pr_info("%s --\n", __func__);

	return 0;
}

static inline int mcd_drm_wait_for_vsync(void *ctx, u32 timeout)
{
	return mcd_drm_wait_work(ctx, MCD_PANEL_WQ_VSYNC, timeout);
}

static inline int mcd_drm_wait_for_fsync(void *ctx, u32 timeout)
{
	return mcd_drm_wait_work(ctx, MCD_PANEL_WQ_FSYNC, timeout);
}

static inline int mcd_drm_set_bypass(void *ctx, bool on)
{
	return mtk_drm_set_frame_skip(on);
}

#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
static int mcd_drm_panel_set_osc(struct mtk_panel *ctx, u32 frequency)
{
	struct panel_clock_info info;
	int ret;

	if (!ctx) {
		pr_err("FREQ_HOP: ERR:%s: invalid param\n", __func__);
		return -EINVAL;
	}

	info.clock_id = CLOCK_ID_OSC;
	info.clock_rate = frequency;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev,
			req_set_clock, &info);
	if (ret < 0) {
		pr_err("%s: failed to set ffc\n", __func__);
		return ret;
	}

	return 0;
}

static int mcd_drm_panel_set_ffc(struct mtk_panel *ctx, u32 frequency)
{
	struct panel_clock_info info;
	int ret;

	if (!ctx) {
		pr_err("FREQ_HOP: ERR:%s: invalid param\n", __func__);
		return -EINVAL;
	}

	info.clock_id = CLOCK_ID_DSI;
	info.clock_rate = frequency;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev,
			req_set_clock, &info);
	if (ret < 0) {
		pr_err("%s: failed to set ffc\n", __func__);
		return ret;
	}

	return 0;
}

int mcd_ddp_update_dsi_freq(struct mtk_panel *ctx, unsigned int freq)
{
	int i;
	unsigned int data_rate = freq / 1000;

	for (i = 0; i < ctx->desc->num_modes; i++) {
		ctx->desc->modes[i].mtk_ext.dyn.pll_clk = data_rate / 2;
		ctx->desc->modes[i].mtk_ext.dyn.data_rate = data_rate;
		ctx->desc->modes[i].mtk_ext.dyn.switch_en = 1;
	}

	pr_info("%s data_rate: %d\n", __func__, data_rate);

	mtk_disp_mipi_ccci_callback(1, 0);

	return 0;
}

__visible_for_testing int mcd_drm_set_freq_hop(void *_ctx,
		struct freq_hop_param *param)
{
	struct mipi_dsi_device *dsi;
	struct mtk_panel *ctx = _ctx;
	int ret;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi) {
		pr_err("%s: invalid dsi\n", __func__);
		return -EINVAL;
	}

	ret = mcd_ddp_update_dsi_freq(ctx, param->dsi_freq);
	if (ret < 0) {
		pr_err("%s: failed to update dsi_freq(%d)\n",
				__func__, param->dsi_freq);
		return ret;
	}

	ret = mcd_drm_panel_set_ffc(ctx, param->dsi_freq);
	if (ret < 0) {
		pr_err("%s: failed to set ffc(%d)\n",
				__func__, param->dsi_freq);
		return ret;
	}

	ret = mcd_drm_panel_set_osc(ctx, param->osc_freq);
	if (ret < 0) {
		pr_err("%s: failed to set osc(%d)\n",
				__func__, param->osc_freq);
		return ret;
	}

	return 0;
}
#endif

__visible_for_testing int mcd_drm_set_lpdt(void *_ctx, bool on)
{
	struct mipi_dsi_device *dsi;
	struct mtk_panel *ctx = (struct mtk_panel *)_ctx;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi) {
		pr_err("%s: invalid dsi\n", __func__);
		return -EINVAL;
	}

	pr_info("%s %s\n", __func__, on ? "on" : "off");

	if (on)
		dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	else
		dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	return 0;
}

struct panel_adapter_funcs mcd_panel_adapter_funcs = {
	.read = mcd_drm_mtk_dsi_read,
	.write = mcd_drm_mtk_dsi_write,
	.write_table = mcd_drm_mtk_dsi_write_table,
	.get_state = mcd_drm_mtk_dsi_get_state,
	.parse_dt = mcd_drm_parse_dt,
	.wait_for_vsync = mcd_drm_wait_for_vsync,
	.wait_for_fsync = mcd_drm_wait_for_fsync,
	.set_bypass = mcd_drm_set_bypass,
//	.get_bypass = mcd_drm_get_bypass, //TODO
	.set_lpdt = mcd_drm_set_lpdt,
//	.dpu_register_dump = mcd_drm_dpu_register_dump,
//	.dpu_event_log_print = mcd_drm_dpu_event_log_print,
//	.set_commit_retry = mcd_drm_set_commit_retry,
//	.emergency_off = mcd_drm_emergency_off,
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	.set_freq_hop = mcd_drm_set_freq_hop,
#endif
};

static int mcd_panel_check_probe(struct mtk_panel *ctx)
{
	int ret = 0;

	if (!ctx || !ctx->mcd_panel_dev)
		return -ENODEV;

	mutex_lock(&ctx->probe_lock);
	if (ctx->mcd_panel_probed)
		goto out;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, probe);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel probe failed %d", __func__, ret);
		goto out;
	}
	ctx->mcd_panel_probed = true;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, get_ddi_props, &ctx->ddi_props);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel get ddi props failed %d", __func__, ret);
		goto out;
	}

out:
	mutex_unlock(&ctx->probe_lock);
	return ret;
}

static int mcd_panel_set_power(struct mtk_panel *ctx, int power)
{
	int ret;

	if (!ctx)
		return -EINVAL;

	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	if (power) {
		ret = call_mcd_panel_func(ctx->mcd_panel_dev, power_on);
		if (ret < 0) {
			dev_err(ctx->dev, "%s failed to power_on %d\n", __func__, ret);
			return ret;
		}
	} else {
		ret = call_mcd_panel_func(ctx->mcd_panel_dev, power_off);
		if (ret < 0) {
			dev_err(ctx->dev, "%s failed to power_off %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

void mcd_drm_probe_handler(struct work_struct *data)
{
	struct mcd_panel_wq *w = container_of(to_delayed_work(data), struct mcd_panel_wq, dwork);
	struct mtk_panel *ctx = container_of(w, struct mtk_panel, wqs[MCD_PANEL_WQ_PANEL_PROBE]);
	int ret;

	pr_info("%s +\n", __func__);

	ret = mcd_panel_check_probe(ctx);
	if (ret < 0)
		dev_err(ctx->dev, "%s mcd-panel not probed %d\n", __func__, ret);

	pr_info("%s -\n", __func__);
}

void mcd_drm_vblank_handler(struct work_struct *data)
{
	struct mcd_panel_wq *w =
		container_of(to_delayed_work(data), struct mcd_panel_wq, dwork);
	struct mtk_panel *ctx = wq_to_mtk_panel(w);
	int ret;

	ret = mcd_drm_wait_vsync(ctx, 0);
	if (ret) {
		pr_err("%s failed to wait vsync\n", __func__);
		return;
	}

	atomic_inc(&w->count);
	wake_up_interruptible_all(&w->wait);
}

void mcd_drm_fsync_handler(struct work_struct *data)
{
	struct mcd_panel_wq *w = container_of(to_delayed_work(data), struct mcd_panel_wq, dwork);
	struct mtk_panel *ctx = wq_to_mtk_panel(w);
	int ret;

	ret = mcd_drm_wait_frame_start(ctx, 1000); /* TODO time out 1000 ok? */
	if (ret) {
		pr_err("%s failed to wait fsync\n", __func__);
		return;
	}

	atomic_inc(&w->count);
	wake_up_interruptible_all(&w->wait);
}

void mcd_drm_dispon_handler(struct work_struct *data)
{
	struct mcd_panel_wq *w =
		container_of(to_delayed_work(data),
				struct mcd_panel_wq, dwork);
	struct mtk_panel *ctx = container_of(w,
			struct mtk_panel, wqs[MCD_PANEL_WQ_DISPON]);
	int ret;

	if (!ctx->ddi_props.support_avoid_sandstorm)
		mcd_drm_wait_frame_start(ctx, 1000); /* TODO time out 1000 ok? */

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, display_on);
	if (ret < 0)
		dev_err(ctx->dev, "%s failed to display_on %d\n", __func__, ret);
}

work_func_t mcd_panel_wq_fns[MAX_MCD_PANEL_WQ] = {
	[MCD_PANEL_WQ_VSYNC] = mcd_drm_vblank_handler,
	[MCD_PANEL_WQ_FSYNC] = mcd_drm_fsync_handler,
	[MCD_PANEL_WQ_DISPON] = mcd_drm_dispon_handler,
	[MCD_PANEL_WQ_PANEL_PROBE] = mcd_drm_probe_handler,
};

static char *mcd_panel_wq_names[MAX_MCD_PANEL_WQ] = {
	[MCD_PANEL_WQ_VSYNC] = "wq_vsync",
	[MCD_PANEL_WQ_FSYNC] = "wq_fsync",
	[MCD_PANEL_WQ_DISPON] = "wq_dispon",
	[MCD_PANEL_WQ_PANEL_PROBE] = "wq_panel_probe",
};

int mcd_panel_wq_exit(struct mtk_panel *ctx)
{
	struct mcd_panel_wq *w;
	int i;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_MCD_PANEL_WQ; i++) {
		if (!mcd_panel_wq_fns[i])
			continue;

		w = &ctx->wqs[i];
		if (w->wq) {
			destroy_workqueue(w->wq);
			w->wq = NULL;
		}
	}
	return 0;
}

int mcd_panel_wq_init(struct mtk_panel *ctx)
{
	struct mcd_panel_wq *w;
	int ret, i;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_MCD_PANEL_WQ; i++) {
		if (!mcd_panel_wq_fns[i])
			continue;

		w = &ctx->wqs[i];
		w->index = i;
		INIT_DELAYED_WORK(&w->dwork, mcd_panel_wq_fns[i]);
		w->wq = create_singlethread_workqueue(mcd_panel_wq_names[i]);
		if (!w->wq) {
			pr_err("%s failed to workqueue initialize %s\n", __func__, mcd_panel_wq_names[i]);
			ret = mcd_panel_wq_exit(ctx);
			if (ret < 0)
				pr_err("%s failed to wq_exit %d\n", __func__, ret);
			return -EINVAL;
		}
		w->name = mcd_panel_wq_names[i];
		init_waitqueue_head(&w->wait);
		atomic_set(&w->count, 0);
	}

	return 0;
}

/**************  MTK EXT **************/
static int mtk_panel_ext_set_power(struct drm_panel *panel, int power)
{
	struct mtk_panel *ctx = container_of(panel, struct mtk_panel, panel);

	pr_debug("%s: dev_name(%s) power(%d)\n",
			__func__, dev_name(ctx->dev), power);

	mcd_panel_set_power(ctx, power);

	return 0;
}

#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
static int mcd_panel_set_lock(struct mtk_panel *ctx, int lock)
{
	int ret;

	if (!ctx)
		return -EINVAL;

	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	if (lock) {
		ret = call_mcd_panel_func(ctx->mcd_panel_dev, lock);
		if (ret < 0) {
			dev_err(ctx->dev, "%s failed to power_on %d\n", __func__, ret);
			return ret;
		}
	} else {
		ret = call_mcd_panel_func(ctx->mcd_panel_dev, unlock);
		if (ret < 0) {
			dev_err(ctx->dev, "%s failed to power_off %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int mtk_panel_ext_set_panel_lock(struct drm_panel *panel, int lock)
{
	struct mtk_panel *ctx = container_of(panel, struct mtk_panel, panel);

	mcd_panel_set_lock(ctx, lock);

	return 0;
}
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
static struct drm_display_mode *get_mode_by_connector_id(
	struct drm_connector *connector, unsigned int id)
{
	struct drm_display_mode *mode;
	unsigned int i = 0;

	list_for_each_entry(mode, &connector->modes, head) {
		if (i == id)
			return mode;
		i++;
	}
	return NULL;
}

#ifdef OLD_DRM
static int mtk_panel_ext_param_set(struct drm_panel *panel,
		unsigned int id)
#else
static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int id)
#endif
{
	struct mtk_panel *ctx;
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct drm_display_mode *mode;
#ifdef OLD_DRM
	struct drm_connector *connector = panel->connector;
#endif
	int i;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	mode = get_mode_by_connector_id(connector, id);
	if (IS_ERR_OR_NULL(mode))
		return -EINVAL;

	for (i = 0; i< ctx->desc->num_modes; i++) {
		if (drm_mode_equal(mode, &ctx->desc->modes[i].mode)) {
			ext->params = &ctx->desc->modes[i].mtk_ext;
			return 0;
		}
	}

	return 1;
}

#if IS_ENABLED(CONFIG_USDM_PANEL_LPM)
static int mtk_panel_ext_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	ret = mcd_panel_check_probe(ctx);
	if (ret < 0)
		return ret;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, doze);
	if (ret < 0) {
		dev_err(ctx->dev, "%s failed to doze %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int mtk_panel_ext_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	ret = mcd_panel_check_probe(ctx);
	if (ret < 0)
		return ret;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, sleep_out);
	if (ret < 0) {
		dev_err(ctx->dev, "%s failed to sleep_out %d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif

#ifdef OLD_DRM
static int mtk_panel_mode_switch(struct drm_panel *panel,
		unsigned int cur_mode, unsigned int dst_mode,
		enum MTK_PANEL_MODE_SWITCH_STAGE stage)
#else
static int mtk_panel_mode_switch(struct drm_panel *panel,
		struct drm_connector *connector,
		unsigned int cur_mode, unsigned int dst_mode,
		enum MTK_PANEL_MODE_SWITCH_STAGE stage)
#endif
{
	int ret = 0;
	struct drm_display_mode *mode;
	struct mtk_panel *ctx;
	struct panel_display_modes *pdms;
	struct panel_display_mode *pdm;
#ifdef OLD_DRM
	struct drm_connector *connector = panel->connector;
#endif

	if (cur_mode == dst_mode)
		return ret;

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	mode = get_mode_by_connector_id(connector, dst_mode);
	if (IS_ERR_OR_NULL(mode))
		return -EINVAL;

	dev_dbg(ctx->dev, "%s +\n", __func__);

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, get_display_mode, &pdms);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel get_display_mode failed %d", __func__, ret);
		return ret;
	}

	pdm = mtk_panel_find_panel_mode(pdms, mode);
	if (pdm == NULL) {
		dev_err(ctx->dev, "%s: %dx%d@%d is not found",
				__func__, mode->hdisplay, mode->vdisplay,
				drm_mode_vrefresh(mode));
		return -EINVAL;
	}

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, set_display_mode, pdm);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel set_display_mode failed %d", __func__, ret);
		return ret;
	}

	dev_info(ctx->dev, "%s drm-mode:%s, panel-mode:%s\n",
			__func__, mode->name, pdm->name);

	dev_dbg(ctx->dev, "%s -\n", __func__);

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_power = mtk_panel_ext_set_power,
#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
	.set_panel_lock = mtk_panel_ext_set_panel_lock,
#endif
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mtk_panel_mode_switch,
#if IS_ENABLED(CONFIG_USDM_PANEL_LPM)
	.doze_enable = mtk_panel_ext_doze_enable,
	.doze_disable = mtk_panel_ext_doze_disable,
#endif
};
#endif /* CONFIG_MTK_PANEL_EXT */

/**************  DRM PANEL **************/
static int mcd_panel_disable(struct drm_panel *panel)
{
	struct mtk_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, sleep_in);
	if (ret < 0) {
		dev_err(ctx->dev, "%s failed to sleep_in %d\n", __func__, ret);
		return 0; /* return ret; */
	}
	ctx->enabled = false;

	down_write(&ctx->mcd_drm_state_lock);
	ctx->mcd_drm_state = MCD_DRM_STATE_DISABLED;
	up_write(&ctx->mcd_drm_state_lock);

	return 0;
}

static int mcd_panel_display_on(struct mtk_panel *ctx)
{
	if (!ctx)
		return -ENODEV;

	queue_delayed_work(ctx->wqs[MCD_PANEL_WQ_DISPON].wq,
			&ctx->wqs[MCD_PANEL_WQ_DISPON].dwork, msecs_to_jiffies(0));

	dev_info(ctx->dev, "%s MCD_PANEL_WQ_DISPON queued\n", __func__);

	return 0;
}

/*
 * How to MTK DSI enable
 * 1: ext->set_power
 * 2: DSI enable (LP11)
 * 3: panel_prepare (init-seq)
 * 4: DSI start (HS)(vdo_mode:stream run)
 * 5: panel_enable (display_on_wq)
 */

static int mcd_panel_enable(struct drm_panel *panel)
{
	struct mtk_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	if (ctx->enabled) {
		dev_info(ctx->dev, "%s : panel is already initialized\n", __func__);
		return 0;
	}

	ret = mcd_panel_check_probe(ctx);
	if (ret < 0)
		return ret;

	ctx->enabled = true;

	ret = mcd_panel_display_on(ctx);
	if (ret < 0)
		dev_err(ctx->dev, "%s failed to display_on %d\n", __func__, ret);

	return 0;
}

static int mcd_panel_unprepare(struct drm_panel *panel)
{
	struct mtk_panel *ctx;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	return 0;
}

static int mcd_panel_prepare(struct drm_panel *panel)
{
	struct mtk_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct mtk_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	ret = mcd_panel_check_probe(ctx);
	if (ret < 0)
		return ret;

	ctx->mcd_drm_state = MCD_DRM_STATE_ENABLED;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, sleep_out);
	if (ret < 0) {
		dev_err(ctx->dev, "%s failed to sleep_out %d\n", __func__, ret);
		return 0; /* return ret; */
	}

	return 0;
}

#ifdef OLD_DRM
int mcd_panel_get_modes(struct drm_panel *panel)
#else
int mcd_panel_get_modes(struct drm_panel *panel, struct drm_connector *conn)
#endif
{
	struct mtk_panel *ctx =
		container_of(panel, struct mtk_panel, panel);
	struct drm_display_mode *preferred_mode = NULL;
#ifdef OLD_DRM
	struct drm_connector *conn = panel->connector;
#endif
	int i;

	dev_info(ctx->dev, "%s +\n", __func__);

	for (i = 0; i < (int)ctx->desc->num_modes; i++) {
		const struct mtk_panel_mode *pmode = &ctx->desc->modes[i];
		struct drm_display_mode *mode;

		mode = drm_mode_duplicate(conn->dev, &pmode->mode);
		if (!mode) {
			dev_err(ctx->dev, "failed to add mode %s\n", mode->name);
			return -ENOMEM;
		}

		mode->type |= DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(conn, mode);

		dev_info(ctx->dev, "added display mode: %s\n", mode->name);

		if (!preferred_mode || (mode->type & DRM_MODE_TYPE_PREFERRED))
			preferred_mode = mode;
	}

	if (preferred_mode) {
		dev_info(ctx->dev, "preferred display mode: %s\n",
				preferred_mode->name);
		preferred_mode->type |= DRM_MODE_TYPE_PREFERRED;
		conn->display_info.width_mm = ctx->spec.size_mm[0];
		conn->display_info.height_mm = ctx->spec.size_mm[1];
	}

	dev_info(ctx->dev, "%s -\n", __func__);

	return i;
}
EXPORT_SYMBOL(mcd_panel_get_modes);

static const struct drm_panel_funcs mcd_panel_funcs = {
	.disable = mcd_panel_disable,
	.unprepare = mcd_panel_unprepare,
	.prepare = mcd_panel_prepare,
	.enable = mcd_panel_enable,
	.get_modes = mcd_panel_get_modes,
};

static int mcd_panel_probe(struct mtk_panel *ctx)
{
	struct platform_device *pdev;
	struct device_node *np;
	struct panel_adapter adapter = {
		.ctx = ctx,
		.fifo_size = MAX_HW_TX_FIFO_SIZE,
		.funcs = &mcd_panel_adapter_funcs,
	};
	int ret;

	if (!ctx)
		return -EINVAL;

	init_rwsem(&ctx->mcd_drm_state_lock);
	mutex_init(&ctx->probe_lock);
	ctx->mcd_drm_state = MCD_DRM_STATE_DISABLED;

	np = of_find_compatible_node(NULL, NULL, "samsung,panel-drv");
	if (!np) {
		dev_err(ctx->dev, "%s: compatible(\"samsung,panel-drv\") node not found\n", __func__);
		return -ENOENT;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		dev_err(ctx->dev, "%s: mcd-panel device not found\n", __func__);
		return -ENODEV;
	}

	ctx->mcd_panel_dev = (struct panel_device *)platform_get_drvdata(pdev);
	if (!ctx->mcd_panel_dev) {
		dev_err(ctx->dev, "%s: failed to get panel_device\n", __func__);
		return -ENODEV;
	}

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, attach_adapter, &adapter);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel call attach_adapter failed %d", __func__, ret);
		return ret;
	}

	ret = mcd_panel_wq_init(ctx);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel initialize workqueue failed %d", __func__, ret);
		return ret;
	}

	queue_delayed_work(ctx->wqs[MCD_PANEL_WQ_PANEL_PROBE].wq,
			&ctx->wqs[MCD_PANEL_WQ_PANEL_PROBE].dwork,
			msecs_to_jiffies(MCD_PANEL_PROBE_DELAY_MSEC));

	return 0;
}

struct mtk_panel_desc *
mcd_panel_get_mtk_panel_desc(struct mtk_panel *ctx)
{
	struct panel_display_modes *pdms;
	struct mtk_panel_desc *desc;
	struct mipi_dsi_device *dsi;
	int ret;

	if (!ctx)
		return ERR_PTR(-EINVAL);

	dsi = ctx->dsi;
	if (!dsi)
		return ERR_PTR(-EINVAL);

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, get_display_mode, &pdms);
	if (ret < 0) {
		dev_err(ctx->dev, "%s: mcd_panel get_display_mode failed %d", __func__, ret);
		return ERR_PTR(ret);
	}

	/* create mtk_panel_desc from mcd-panel driver */
	desc = mtk_panel_desc_create_from_panel_display_modes(ctx, pdms);

	/* MTK setting */
	dsi->lanes = ctx->spec.data_lane_cnt;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_EOT_PACKET;

	if (pdms[0].modes[0]->panel_video_mode)
		dsi->mode_flags |= (MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST);

	dsi->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;

	return desc;
}

static int mcd_panel_adapter_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct mtk_panel *ctx;
	int ret = 0;

	ctx = devm_kzalloc(dev, sizeof(struct mtk_panel), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dsi = dsi;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;

	ret = mcd_panel_probe(ctx);
	if (ret < 0) {
		dev_err(dev, "%s: mcd_panel_probe failed %d", __func__, ret);
		return ret;
	}

	ctx->desc = mcd_panel_get_mtk_panel_desc(ctx);
	if (IS_ERR_OR_NULL(ctx->desc)) {
		dev_err(ctx->dev, "%s: failed to get mtk_panel_desc (ret:%ld)",
				__func__, PTR_ERR(ctx->desc));
		return PTR_ERR(ctx->desc);
	}

#ifdef OLD_DRM
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &mcd_panel_funcs;
#else
	drm_panel_init(&ctx->panel, dev,
			&mcd_panel_funcs, DRM_MODE_CONNECTOR_DSI);
#endif
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "%s: mipi_dsi_attach failed %d", __func__, ret);
		return ret;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ctx->desc->modes[0].mtk_ext, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	dev_info(ctx->dev, "mcd common panel driver has been probed.");

	return ret;
}

static int mcd_panel_adapter_remove(struct mipi_dsi_device *dsi)
{
	struct mtk_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	mtk_panel_desc_destroy(ctx, (struct mtk_panel_desc *)ctx->desc);

	return 0;
}

static const struct of_device_id mcd_panel_adatper_of_match[] = {
	{ .compatible = "samsung,mcd-panel-samsung-drv" },
	{ }
};
MODULE_DEVICE_TABLE(of, mcd_panel_adatper_of_match);

static struct mipi_dsi_driver mcd_panel_adapter_driver = {
	.probe = mcd_panel_adapter_probe,
	.remove = mcd_panel_adapter_remove,
	.driver = {
		.name = "mcd-panel-samsung-drv",
		.of_match_table = mcd_panel_adatper_of_match,
	},
};
module_mipi_dsi_driver(mcd_panel_adapter_driver);

MODULE_DESCRIPTION("MIPI-DSI based Samsung common panel driver");
MODULE_LICENSE("GPL");
