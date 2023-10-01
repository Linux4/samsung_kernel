/*
 * awinic_dsp.c  aw87xxx pa module
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 * Author: Alex <zhaozhongbo@awinic.com>
 *
 */
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "aw87xxx.h"
#include "aw87xxx_monitor.h"
#include "awinic_dsp.h"

static const uint32_t PARAM_ID_INDEX_TABLE[][INDEX_PARAMS_ID_MAX] = {
	{
		AFE_PARAM_ID_AWDSP_RX_PARAMS,
		AFE_PARAM_ID_AWDSP_RX_SET_ENABLE,
		AFE_PARAM_ID_AWDSP_TX_SET_ENABLE,
		AFE_PARAM_ID_AWDSP_RX_VMAX_L,
		AFE_PARAM_ID_AWDSP_RX_CALI_CFG_L,
		AFE_PARAM_ID_AWDSP_RX_RE_L,
		AFE_PARAM_ID_AWDSP_RX_NOISE_L,
		AFE_PARAM_ID_AWDSP_RX_F0_L,
		AFE_PARAM_ID_AWDSP_RX_REAL_DATA_L,
		AFE_PARAM_ID_AWDSP_RX_MSG,
	},
	{
		AFE_PARAM_ID_AWDSP_RX_PARAMS,
		AFE_PARAM_ID_AWDSP_RX_SET_ENABLE,
		AFE_PARAM_ID_AWDSP_TX_SET_ENABLE,
		AFE_PARAM_ID_AWDSP_RX_VMAX_R,
		AFE_PARAM_ID_AWDSP_RX_CALI_CFG_R,
		AFE_PARAM_ID_AWDSP_RX_RE_R,
		AFE_PARAM_ID_AWDSP_RX_NOISE_R,
		AFE_PARAM_ID_AWDSP_RX_F0_R,
		AFE_PARAM_ID_AWDSP_RX_REAL_DATA_R,
		AFE_PARAM_ID_AWDSP_RX_MSG,
	},
};

static DEFINE_MUTEX(g_msg_dsp_lock);


#ifdef AW_MTK_OPEN_DSP_PLATFORM
extern int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, int32_t data_size);
extern int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer,
					int16_t size, int32_t *buf_len);
#else

static int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, int32_t data_size)
{
	return 0;
}

static int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer,
					int16_t size, int32_t *buf_len)
{
	return 0;
}
#endif

bool aw87xx_platform_init(void)
{
#ifdef AW_MTK_OPEN_DSP_PLATFORM
	return true;
#else
	return false;
#endif
}

static int aw_get_params_id_by_index(int index, int32_t *params_id,
				     int32_t channel)
{
	if (index >= INDEX_PARAMS_ID_MAX || channel > 1) {
		pr_err("%s: error: index is %d, channel %d\n",
		       __func__, index, channel);
		return -EINVAL;
	}
	*params_id = PARAM_ID_INDEX_TABLE[channel][index];
	return 0;
}

static int aw_mtk_write_data_to_dsp(int index,
				void *data, int data_size, int channel)
{
	int32_t param_id;
	int32_t *dsp_data = NULL;
	struct aw_dsp_msg_hdr *hdr = NULL;
	int ret;

	ret = aw_get_params_id_by_index(index, &param_id, channel);
	if (ret < 0)
		return ret;

	pr_debug("%s: param id = 0x%x", __func__, param_id);

	dsp_data = kzalloc(sizeof(struct aw_dsp_msg_hdr) + data_size,
			GFP_KERNEL);
	if (!dsp_data) {
		pr_err("%s: kzalloc dsp_msg error\n", __func__);
		return -ENOMEM;
	}

	hdr = (struct aw_dsp_msg_hdr *)dsp_data;
	hdr->type = DSP_MSG_TYPE_DATA;
	hdr->opcode_id = param_id;
	hdr->version = AWINIC_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_data) + sizeof(struct aw_dsp_msg_hdr),
			data, data_size);
	ret = mtk_spk_send_ipi_buf_to_dsp(dsp_data,
			sizeof(struct aw_dsp_msg_hdr) + data_size);
	if (ret < 0) {
		pr_err("%s:write data failed\n", __func__);
		kfree(dsp_data);
		dsp_data = NULL;
		return ret;
	}

	kfree(dsp_data);
	dsp_data = NULL;
	return 0;
}

static int aw_mtk_read_data_from_dsp(int index,
			void *data, int data_size, int channel)
{
	int ret;
	int32_t param_id;
	struct aw_dsp_msg_hdr hdr;

	ret = aw_get_params_id_by_index(index, &param_id, channel);
	if (ret < 0)
		return ret;

	pr_debug("%s: param id = 0x%x", __func__, param_id);
	hdr.type = DSP_MSG_TYPE_CMD;
	hdr.opcode_id = param_id;
	hdr.version = AWINIC_DSP_MSG_HDR_VER;

	mutex_lock(&g_msg_dsp_lock);

	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr, sizeof(struct aw_dsp_msg_hdr));
	if (ret < 0) {
		pr_err("%s:send cmd failed\n", __func__);
		goto dsp_msg_failed;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp(data, data_size, &data_size);
	if (ret < 0) {
		pr_err("%s:get data failed\n", __func__);
		goto dsp_msg_failed;
	}
	mutex_unlock(&g_msg_dsp_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_msg_dsp_lock);
	return ret;
}

static int aw_mtk_read_msg_from_dsp(int inline_id,
			void *data, int data_size, int32_t channel)
{
	struct aw_dsp_msg_hdr hdr[2];
	int ret;

	hdr[0].type = DSP_MSG_TYPE_DATA;
	hdr[0].opcode_id = AFE_PARAM_ID_AWDSP_RX_MSG;
	hdr[0].version = AWINIC_DSP_MSG_HDR_VER;
	hdr[1].type = DSP_MSG_TYPE_CMD;
	hdr[1].opcode_id = inline_id;
	hdr[1].version = AWINIC_DSP_MSG_HDR_VER;

	mutex_lock(&g_msg_dsp_lock);
	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr,
			2 * sizeof(struct aw_dsp_msg_hdr));
	if (ret < 0) {
		pr_err("%s:send cmd failed\n", __func__);
		goto dsp_msg_failed;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp(data, data_size, &data_size);
	if (ret < 0) {
		pr_err("%s:get data failed\n", __func__);
		goto dsp_msg_failed;
	}

	mutex_unlock(&g_msg_dsp_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_msg_dsp_lock);
	return ret;
}


int aw_get_vmax_from_dsp(uint32_t *vmax, int32_t channel)
{
	int ret;

	ret = aw_mtk_read_data_from_dsp(INDEX_PARAMS_ID_RX_VMAX,
			(void *)vmax, sizeof(uint32_t), channel);
	if (ret < 0) {
		pr_err("%s: get vmax failed\n", __func__);
		return ret;
	}

	return 0;
}

int aw_set_vmax_to_dsp(uint32_t vmax, int32_t channel)
{
	int ret;

	ret =  aw_mtk_write_data_to_dsp(INDEX_PARAMS_ID_RX_VMAX,
			&vmax, sizeof(uint32_t), channel);
	if (ret < 0) {
		pr_err("%s : set vmax failed\n", __func__);
		return ret;
	}

	return 0;
}

int aw_get_active_flag(uint32_t *active_flag, int32_t channel)
{
	int ret;

	ret = aw_mtk_read_msg_from_dsp(INLINE_PARAM_ID_ACTIVE_FLAG,
			(void *)active_flag, 2 * sizeof(uint32_t), channel);
	if (ret < 0) {
		pr_err("%s : get active flag failed\n", __func__);
		return ret;
	}

	return 0;
}





