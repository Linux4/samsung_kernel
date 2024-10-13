/* sound/soc/samsung/abox/abox_synchronized_ipc_ti.c
 *
 * ALSA SoC Audio Layer - Samsung Abox synchronized IPC driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "abox.h"
#include "abox_dma.h"
//#include <sound/tas_smart_amp_v2.h>

#define TIMEOUT_MS		130
#define DEBUG_SYNCHRONIZED_IPC

#ifdef DEBUG_SYNCHRONIZED_IPC
#define ipc_dbg(format, args...)	\
pr_info("[SYNC_IPC] %s: " format "\n", __func__, ## args)
#else
#define ipc_dbg(format, args...)
#endif /* DEBUG_SYNCHRONIZED_IPC */

#define ipc_err(format, args...)	\
pr_err("[SYNC_IPC] %s: " format "\n", __func__, ## args)

#define REALTIME_GEAR_ID	0x7007

static DECLARE_WAIT_QUEUE_HEAD(wq_read);
static DECLARE_WAIT_QUEUE_HEAD(wq_write);

struct abox_dma_data *data;

bool abox_ipc_read_avail;
bool abox_ipc_write_avail;

#define SMARTPA_ABOX_ERROR	0xF0F0F0F0
#define TI_SMARTPA_VENDOR_ID	1234

#define TAS_PAYLOAD_SIZE	14
#define TAS_SET_PARAM		0
#define TAS_GET_PARAM		1

struct ti_smartpa_data {
	uint32_t payload[TAS_PAYLOAD_SIZE];
};
struct ti_smartpa_data *ti_smartpa_rd_data;
struct ti_smartpa_data ti_smartpa_rd_data_tmp;
struct ti_smartpa_data *ti_smartpa_wr_data;
struct ti_smartpa_data ti_smartpa_wr_data_tmp;

int ti_smartpa_read(void *prm_data, int offset, int size)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	ipc_dbg("");

	abox_request_power(data->dev_abox, REALTIME_GEAR_ID, "tas25xx");

	ti_smartpa_rd_data = (struct ti_smartpa_data *)prm_data;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = TI_SMARTPA_VENDOR_ID;
	erap_msg->param.raw.params[1] = TAS_GET_PARAM;
	erap_msg->param.raw.params[2] = offset;
	erap_msg->param.raw.params[3] = size;
	abox_ipc_read_avail = false;

	if(!data) {
		pr_err("[TI-SmartPA] %s: data is NULL", __func__);
		ret = -1;
		goto release;
	}

	ret = abox_request_ipc(data->dev_abox, IPC_ERAP,
				&msg, sizeof(msg), 0, 0);
	if (ret) {
		pr_err("[TI-SmartPA] %s: abox_request_ipc is failed: %d\n",
			__func__, ret);
		ret = -1;
		goto release;
	}

	ret = wait_event_timeout(wq_read,
		abox_ipc_read_avail != false, msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("[TI-SmartPA] %s: wait_event timeout\n", __func__);
		ret = -ETIMEDOUT;
	} else if (ret < 0) {
		pr_err("[TI-SmartPA] %s: wait_event err(%d)\n", __func__, ret);
		ret = -1;
	} else {
		memcpy(&ti_smartpa_rd_data->payload[0],
			&ti_smartpa_rd_data_tmp.payload[0], size);
		ret = 0;
	}

release:
	abox_release_power(data->dev_abox, REALTIME_GEAR_ID, "tas25xx");

	return ret;
}
EXPORT_SYMBOL_GPL(ti_smartpa_read);

int ti_smartpa_write(void *prm_data, int offset, int size)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	ipc_dbg("");

	abox_request_power(data->dev_abox, REALTIME_GEAR_ID, "tas25xx");

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = TI_SMARTPA_VENDOR_ID;
	erap_msg->param.raw.params[1] = TAS_SET_PARAM;
	erap_msg->param.raw.params[2] = offset;
	erap_msg->param.raw.params[3] = size;

	memcpy(&erap_msg->param.raw.params[4], prm_data,
		min((sizeof(uint32_t) * size), sizeof(erap_msg->param.raw)));
	abox_ipc_write_avail = false;

	if(!data) {
		pr_err("[TI-SmartPA] %s: data is NULL", __func__);
		ret = -1;
		goto release;
	}

	ret = abox_request_ipc(data->dev_abox, IPC_ERAP,
			&msg, sizeof(msg), 0, 0);
	if (ret) {
		pr_err("[TI-SmartPA] %s: abox_request_ipc is failed: %d\n", __func__, ret);
		ret = -1;
		goto release;
	}

	ret = wait_event_timeout(wq_write,
		abox_ipc_write_avail != false, msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("[TI-SmartPA] %s: wait_event timeout\n", __func__);
		ret = -ETIMEDOUT;
	} else
		ret = 0;

release:
	abox_release_power(data->dev_abox, REALTIME_GEAR_ID, "tas25xx");

	return ret;
}
EXPORT_SYMBOL_GPL(ti_smartpa_write);

static irqreturn_t abox_synchronized_ipc_ti_handler(int irq,
					void *dev_id, ABOX_IPC_MSG *msg)
{
	struct IPC_ERAP_MSG *erap_msg = &msg->msg.erap;
	unsigned int res_id = erap_msg->param.raw.params[0];
	unsigned int is_read = erap_msg->param.raw.params[1];
	unsigned int offset = erap_msg->param.raw.params[2];
	unsigned int size = erap_msg->param.raw.params[3];
	irqreturn_t ret = IRQ_HANDLED;

	ipc_dbg("irq = %d, res_id=0x%x, is_read=%u offset=%d, size=%u, value0=%x",
		irq, res_id, is_read, offset, size, erap_msg->param.raw.params[4]);

	switch (irq) {
	case IPC_ERAP:
		switch (erap_msg->msgtype) {
		case REALTIME_EXTRA:
			switch (res_id) {
			case TI_SMARTPA_VENDOR_ID:
				if (erap_msg->param.raw.params[1] == TAS_GET_PARAM) {
					ipc_dbg("irq = %d, res_id=0x%x, is_read=%u offset=%u, size=%u, value0=0x%x",
						irq, res_id, is_read, offset, size, erap_msg->param.raw.params[4]);
					memcpy(&ti_smartpa_rd_data_tmp.payload[0], &erap_msg->param.raw.params[4],
						min(sizeof(struct ti_smartpa_data), sizeof(erap_msg->param.raw)));
					abox_ipc_read_avail = true;
					if (waitqueue_active(&wq_read))
						wake_up(&wq_read);
				} else if (erap_msg->param.raw.params[1] == TAS_SET_PARAM) {
					abox_ipc_write_avail = true;
					if (waitqueue_active(&wq_write))
						wake_up(&wq_write);
				} else {
					pr_err("[TI-SmartPA] %s: Invalid callback, %d", __func__,
						erap_msg->param.raw.params[1]);
				}
				ret = IRQ_HANDLED;
			break;
			default:
				ipc_err("unknown res_id(%d)", res_id);
				ret = IRQ_NONE;
			break;
			}
		break;
		default:
			ipc_err("unknown message type(%d)", erap_msg->msgtype);
			ret = IRQ_NONE;
		break;
		}
	break;
	default:
		ipc_err("unknown command");
		ret = IRQ_NONE;
	break;
	}

	return ret;
}

static int samsung_abox_synchronized_ipc_ti_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_abox;
	struct platform_device *pdev_abox;

	ipc_dbg(" ");

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "[SYNC_IPC] Failed to allocate memory\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, data);

	np_abox = of_parse_phandle(np, "abox", 0);
	if (!np_abox) {
		dev_err(dev, "[SYNC_IPC] Failed to get abox device node\n");
		return -EPROBE_DEFER;
	}

	pdev_abox = of_find_device_by_node(np_abox);
	data->dev_abox = &pdev_abox->dev;
	if (!data->dev_abox) {
		dev_err(dev, "[SYNC_IPC] Failed to get abox platform device\n");
		return -EPROBE_DEFER;
	}
	data->abox_data = platform_get_drvdata(pdev_abox);

	abox_register_ipc_handler(data->dev_abox, IPC_ERAP,
				abox_synchronized_ipc_ti_handler, pdev);

	return 0;
}

static int samsung_abox_synchronized_ipc_ti_remove(struct platform_device *pdev)
{
	ipc_dbg(" ");

	return 0;
}

static const struct of_device_id samsung_abox_synchronized_ipc_ti_match[] = {
	{
		.compatible = "samsung,abox-synchronized-ipc-ti",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_synchronized_ipc_ti_match);

static struct platform_driver samsung_abox_synchronized_ipc_ti_driver = {
	.probe  = samsung_abox_synchronized_ipc_ti_probe,
	.remove = samsung_abox_synchronized_ipc_ti_remove,
	.driver = {
		.name = "samsung-abox-synchronized-ipc-ti",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_synchronized_ipc_ti_match),
	},
};
module_platform_driver(samsung_abox_synchronized_ipc_ti_driver);

/* Module information */
MODULE_DESCRIPTION("Samsung ASoC A-Box Synchronized IPC TI Driver");
MODULE_ALIAS("platform:samsung-abox-synchronized-ipc-ti");
MODULE_LICENSE("GPL");
