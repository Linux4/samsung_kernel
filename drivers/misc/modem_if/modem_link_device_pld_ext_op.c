/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <mach/mfp-pxa988-aruba.h>
#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_pld.h"

#if defined(CONFIG_CDMA_MODEM_CBP72) || defined(CONFIG_CDMA_MODEM_CBP82)
static void cbp_init_boot_map(struct pld_link_device *pld)
{
	struct memif_boot_map *bt_map = &pld->bt_map;

	bt_map->magic = (u32 *)pld->base;
	bt_map->buff = (u8 *)(pld->base + DP_BOOT_BUFF_OFFSET);
	bt_map->space = pld->size - 4;
}

static void cbp_init_dl_map(struct pld_link_device *pld)
{
	pld->dl_map.magic = (u32 *)pld->base;
	pld->dl_map.buff = (u8 *)(pld->base + DP_DLOAD_BUFF_OFFSET);
}

static void print_data(char *data, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (i && !(i % 16))
			mif_info("\n");
		mif_info("%02x ", *((unsigned char *)data + i));
	}
	mif_info("\n");
}

static int cbp_udl_wait_resp(struct pld_link_device *pld)
{
	int ret;
	int CPresq = 0;
	ret = wait_for_completion_timeout(&pld->udl_cmd_complete, 2*HZ/*UDL_TIMEOUT*/);
	if(!ret) {
		mif_info("ERR! No UDL_CMD_RESP!!!\n");
		return -EIO;
	}
	ret = _pld_read_register(pld->mbx2ap, &CPresq, 2);
	return CPresq;
}

static int cbp_xmit_binary(struct pld_link_device *pld,
			struct sk_buff *skb)
{
	struct spi_boot_frame *bf = (struct spi_boot_frame *)skb->data;
	u8 __iomem *buff = pld->bt_map.buff;
	int err, i = 0;

	if (bf->req) {
		if(bf->len)
			_pld_write_register(pld->dev[0]->txq.buff, bf->data, (bf->len > MAX_SEND_SIZE) ? MAX_SEND_SIZE : bf->len);
		_pld_write_register(pld->mbx2cp,&bf->req, 2);
	}

	if (bf->resp) {
		err = cbp_udl_wait_resp(pld);
		if (err < 0) {
			mif_err("ERR! cbp_udl_wait_resp fail (err %d)\n", err);
			goto exit;
		} else if (err == bf->resp) {
			err = skb->len;
		} else {
			mif_err("ERR! cbp_udl_wait_resp fail err(0x%4x), resp(0x%4x)\n", err, bf->resp);
			err = -EAGAIN;
		}
	}
exit:
	dev_kfree_skb_any(skb);
	return err;
}

static int cbp_xmit_DL_Magic(struct pld_link_device *pld)
{
	u8 __iomem *buff = pld->bt_map.buff;
	u32 magicCode = DP_MAGIC_DMDL;

	pld_control_cs_n(0);

	mif_info("[PLD_WRITE] addr = 0x%04x, data = 0x%04x\n", pld->magic_ap2cp, DP_MAGIC_DMDL);
	_pld_write_register(pld->magic_ap2cp, &magicCode, 4);

	return 0;
}

#endif

#if defined(CONFIG_CDMA_MODEM_MDM6600) || defined(CONFIG_GSM_MODEM_ESC6270)
enum qc_dload_tag {
	QC_DLOAD_TAG_NONE = 0,
	QC_DLOAD_TAG_BIN,
	QC_DLOAD_TAG_NV,
	QC_DLOAD_TAG_MAX
};

struct qc_dpram_boot_map {
	u8 __iomem *buff;
	u16 __iomem *frame_size;
	u16 __iomem *tag;
	u16 __iomem *count;
};

struct qc_dpram_udl_param {
	unsigned char *addr;
	unsigned int size;
	unsigned int count;
	unsigned int tag;
};

struct qc_dpram_udl_check {
	unsigned int total_size;
	unsigned int rest_size;
	unsigned int send_size;
	unsigned int copy_start;
	unsigned int copy_complete;
	unsigned int boot_complete;
};

static struct qc_dpram_boot_map qc_bt_map;
static struct qc_dpram_udl_param qc_udl_param;
static struct qc_dpram_udl_check qc_udl_check;

static void qc_dload_task(unsigned long data);

static void qc_init_boot_map(struct pld_link_device *pld)
{
	struct qc_dpram_boot_map *qbt_map = &qc_bt_map;
	struct modemlink_dpram_data *dpram = pld->dpram;

	qbt_map->buff = pld->dev[0]->txq.buff;
	qbt_map->frame_size = (u16 *)(pld->base + dpram->boot_size_offset);
	qbt_map->tag = (u16 *)(pld->base + dpram->boot_tag_offset);
	qbt_map->count = (u16 *)(pld->base + dpram->boot_count_offset);

	tasklet_init(&pld->dl_tsk, qc_dload_task, (unsigned long)pld);
}

static void qc_dload_map(struct pld_link_device *pld, u8 is_upload)
{
	struct qc_dpram_boot_map *qbt_map = &qc_bt_map;
	struct modemlink_dpram_data *dpram = pld->dpram;
	unsigned int upload_offset = 0;

	if (is_upload == 1)	{
		upload_offset = 0x1000;
		qbt_map->buff = pld->dev[0]->rxq.buff;
	}	else {
		upload_offset = 0;
		qbt_map->buff = pld->dev[0]->txq.buff;
	}

	qbt_map->frame_size = (u16 *)(pld->base +
			dpram->boot_size_offset + upload_offset);
	qbt_map->tag = (u16 *)(pld->base +
			dpram->boot_tag_offset + upload_offset);
	qbt_map->count = (u16 *)(pld->base +
			dpram->boot_count_offset + upload_offset);
}

static int qc_prepare_download(struct pld_link_device *pld)
{
	int retval = 0;
	int count = 0;

	qc_dload_map(pld, 0);

	while (1) {
		if (qc_udl_check.copy_start) {
			qc_udl_check.copy_start = 0;
			break;
		}

		usleep_range(10000, 11000);

		count++;
		if (count > 1000) {
			mif_err("ERR! count %d\n", count);
			return -1;
		}
	}

	return retval;
}

static void _qc_do_download(struct pld_link_device *pld,
			struct qc_dpram_udl_param *param)
{
	struct qc_dpram_boot_map *qbt_map = &qc_bt_map;

	if (param->size <= pld->dpram->max_boot_frame_size) {
		iowrite16(PLD_ADDR_MASK(&qbt_map->buff[0]),
					pld->address_buffer);
		memcpy(pld->base, param->addr, param->size);

		iowrite16(PLD_ADDR_MASK(&qbt_map->frame_size[0]),
					pld->address_buffer);
		iowrite16(param->size, pld->base);

		iowrite16(PLD_ADDR_MASK(&qbt_map->tag[0]),
					pld->address_buffer);
		iowrite16(param->tag, pld->base);

		iowrite16(PLD_ADDR_MASK(&qbt_map->count[0]),
					pld->address_buffer);
		iowrite16(param->count, pld->base);

		pld->send_intr(pld, 0xDB12);
	} else {
		mif_info("param->size %d\n", param->size);
	}
}

static int _qc_download(struct pld_link_device *pld, void *arg,
			enum qc_dload_tag tag)
{
	int retval = 0;
	int count = 0;
	int cnt_limit;
	unsigned char *img;
	struct qc_dpram_udl_param param;

	retval = copy_from_user((void *)&param, (void *)arg, sizeof(param));
	if (retval < 0) {
		mif_err("ERR! copy_from_user fail\n");
		return -1;
	}

	img = vmalloc(param.size);
	if (!img) {
		mif_err("ERR! vmalloc fail\n");
		return -1;
	}
	memset(img, 0, param.size);
	memcpy(img, param.addr, param.size);

	qc_udl_check.total_size = param.size;
	qc_udl_check.rest_size = param.size;
	qc_udl_check.send_size = 0;
	qc_udl_check.copy_complete = 0;

	qc_udl_param.addr = img;
	qc_udl_param.size = pld->dpram->max_boot_frame_size;
	if (tag == QC_DLOAD_TAG_NV)
		qc_udl_param.count = 1;
	else
		qc_udl_param.count = param.count;
	qc_udl_param.tag = tag;

	if (qc_udl_check.rest_size < pld->dpram->max_boot_frame_size)
		qc_udl_param.size = qc_udl_check.rest_size;

	/* Download image (binary or NV) */
	_qc_do_download(pld, &qc_udl_param);

	/* Wait for completion
	*/
	if (tag == QC_DLOAD_TAG_NV)
		cnt_limit = 200;
	else
		cnt_limit = 1000;

	while (1) {
		if (qc_udl_check.copy_complete) {
			qc_udl_check.copy_complete = 0;
			retval = 0;
			break;
		}

		usleep_range(10000, 11000);

		count++;
		if (count > cnt_limit) {
			mif_err("ERR! count %d\n", count);
			retval = -1;
			break;
		}
	}

	vfree(img);

	return retval;
}

static int qc_download_bin(struct pld_link_device *pld, void *arg)
{
	return _qc_download(pld, arg, QC_DLOAD_TAG_BIN);
}

static int qc_download_nv(struct pld_link_device *pld, void *arg)
{
	return _qc_download(pld, arg, QC_DLOAD_TAG_NV);
}

static void qc_dload_task(unsigned long data)
{
	struct pld_link_device *pld = (struct pld_link_device *)data;

	qc_udl_check.send_size += qc_udl_param.size;
	qc_udl_check.rest_size -= qc_udl_param.size;

	qc_udl_param.addr += qc_udl_param.size;

	if (qc_udl_check.send_size >= qc_udl_check.total_size) {
		qc_udl_check.copy_complete = 1;
		qc_udl_param.tag = 0;
		return;
	}

	if (qc_udl_check.rest_size < pld->dpram->max_boot_frame_size)
		qc_udl_param.size = qc_udl_check.rest_size;

	qc_udl_param.count += 1;

	_qc_do_download(pld, &qc_udl_param);
}

static void qc_dload_cmd_handler(struct pld_link_device *pld, u16 cmd)
{
	switch (cmd) {
	case 0x1234:
		qc_udl_check.copy_start = 1;
		break;

	case 0xDBAB:
		tasklet_schedule(&pld->dl_tsk);
		break;

	case 0xABCD:
		mif_info("[%s] booting Start\n", pld->ld.name);
		qc_udl_check.boot_complete = 1;
		break;

	default:
		mif_err("ERR! unknown command 0x%04X\n", cmd);
	}
}

static int qc_boot_start(struct pld_link_device *pld)
{
	u16 mask = 0;
	int count = 0;

	/* Send interrupt -> '0x4567' */
	mask = 0x4567;
	pld->send_intr(pld, mask);

	while (1) {
		if (qc_udl_check.boot_complete) {
			qc_udl_check.boot_complete = 0;
			break;
		}

		usleep_range(10000, 11000);

		count++;
		if (count > 200) {
			mif_err("ERR! count %d\n", count);
			return -1;
		}
	}

	return 0;
}

static int qc_boot_post_process(struct pld_link_device *pld)
{
	int count = 0;

	while (1) {
		if (pld->boot_start_complete) {
			pld->boot_start_complete = 0;
			break;
		}

		usleep_range(10000, 11000);

		count++;
		if (count > 200) {
			mif_err("ERR! count %d\n", count);
			return -1;
		}
	}

	return 0;
}

static void qc_start_handler(struct pld_link_device *pld)
{
	/*
	 * INT_MASK_VALID | INT_MASK_CMD | INT_MASK_CP_AIRPLANE_BOOT |
	 * INT_MASK_CP_AP_ANDROID | INT_MASK_CMD_INIT_END
	 */
	u16 mask = (0x0080 | 0x0040 | 0x1000 | 0x0100 | 0x0002);

	pld->boot_start_complete = 1;

	/* Send INIT_END code to CP */
	mif_info("send 0x%04X (INIT_END)\n", mask);

	pld->send_intr(pld, mask);
}

static void qc_crash_log(struct pld_link_device *pld)
{
	struct link_device *ld = &pld->ld;
	static unsigned char buf[151];
	u8 __iomem *data = NULL;

	data = pld->get_rx_buff(pld, IPC_FMT);
	memcpy(buf, data, (sizeof(buf) - 1));

	mif_info("PHONE ERR MSG\t| %s Crash\n", ld->mdm_data->name);
	mif_info("PHONE ERR MSG\t| %s\n", buf);
}

static int _qc_data_upload(struct pld_link_device *pld,
			struct qc_dpram_udl_param *param)
{
	struct qc_dpram_boot_map *qbt_map = &qc_bt_map;
	int retval = 0;
	u16 intval = 0;
	int count = 0;

	while (1) {
		if (!gpio_get_value(pld->gpio_ipc_int2ap)) {
			intval = pld->recv_intr(pld);
			if (intval == 0xDBAB) {
				break;
			} else {
				mif_err("intr 0x%08x\n", intval);
				return -1;
			}
		}

		usleep_range(1000, 2000);

		count++;
		if (count > 200) {
			mif_err("<%s:%d>\n", __func__, __LINE__);
			return -1;
		}
	}

	iowrite16(PLD_ADDR_MASK(&qbt_map->frame_size[0]),
				pld->address_buffer);
	param->size = ioread16(pld->base);

	iowrite16(PLD_ADDR_MASK(&qbt_map->tag[0]),
				pld->address_buffer);
	param->tag = ioread16(pld->base);

	iowrite16(PLD_ADDR_MASK(&qbt_map->count[0]),
				pld->address_buffer);
	param->count = ioread16(pld->base);

	iowrite16(PLD_ADDR_MASK(&qbt_map->buff[0]),
				pld->address_buffer);
	memcpy(param->addr, pld->base, param->size);

	pld->send_intr(pld, 0xDB12);

	return retval;
}

static int qc_uload_step1(struct pld_link_device *pld)
{
	int retval = 0;
	int count = 0;
	u16 intval = 0;
	u16 mask = 0;

	qc_dload_map(pld, 1);

	mif_info("+---------------------------------------------+\n");
	mif_info("|            UPLOAD PHONE SDRAM               |\n");
	mif_info("+---------------------------------------------+\n");

	while (1) {
		if (!gpio_get_value(pld->gpio_ipc_int2ap)) {
			intval = pld->recv_intr(pld);
			mif_info("intr 0x%04x\n", intval);
			if (intval == 0x1234) {
				break;
			} else {
				mif_info("ERR! invalid intr\n");
				return -1;
			}
		}

		usleep_range(1000, 2000);

		count++;
		if (count > 200) {
			intval = pld->recv_intr(pld);
			mif_info("count %d, intr 0x%04x\n", count, intval);
			if (intval == 0x1234)
				break;
			return -1;
		}
	}

	mask = 0xDEAD;
	pld->send_intr(pld, mask);

	return retval;
}

static int qc_uload_step2(struct pld_link_device *pld, void *arg)
{
	int retval = 0;
	struct qc_dpram_udl_param param;

	retval = copy_from_user((void *)&param, (void *)arg, sizeof(param));
	if (retval < 0) {
		mif_err("ERR! copy_from_user fail (err %d)\n", retval);
		return -1;
	}

	retval = _qc_data_upload(pld, &param);
	if (retval < 0) {
		mif_err("ERR! _qc_data_upload fail (err %d)\n", retval);
		return -1;
	}

	if (!(param.count % 500))
		mif_info("param->count = %d\n", param.count);

	if (param.tag == 4) {
		enable_irq(pld->irq);
		mif_info("param->tag = %d\n", param.tag);
	}

	retval = copy_to_user((unsigned long *)arg, &param, sizeof(param));
	if (retval < 0) {
		mif_err("ERR! copy_to_user fail (err %d)\n", retval);
		return -1;
	}

	return retval;
}

static int qc_ioctl(struct pld_link_device *pld, struct io_device *iod,
		unsigned int cmd, unsigned long arg)
{
	struct link_device *ld = &pld->ld;
	int err = 0;

	switch (cmd) {
	case IOCTL_DPRAM_PHONE_POWON:
		err = qc_prepare_download(pld);
		if (err < 0)
			mif_info("%s: ERR! prepare_download fail\n", ld->name);
		break;

	case IOCTL_DPRAM_PHONEIMG_LOAD:
		err = qc_download_bin(pld, (void *)arg);
		if (err < 0)
			mif_info("%s: ERR! download_bin fail\n", ld->name);
		break;

	case IOCTL_DPRAM_NVDATA_LOAD:
		err = qc_download_nv(pld, (void *)arg);
		if (err < 0)
			mif_info("%s: ERR! download_nv fail\n", ld->name);
		break;

	case IOCTL_DPRAM_PHONE_BOOTSTART:
		err = qc_boot_start(pld);
		if (err < 0) {
			mif_info("%s: ERR! boot_start fail\n", ld->name);
			break;
		}

		err = qc_boot_post_process(pld);
		if (err < 0)
			mif_info("%s: ERR! boot_post_process fail\n", ld->name);

		break;

	case IOCTL_DPRAM_PHONE_UPLOAD_STEP1:
		disable_irq_nosync(pld->irq);
		err = qc_uload_step1(pld);
		if (err < 0) {
			enable_irq(pld->irq);
			mif_info("%s: ERR! upload_step1 fail\n", ld->name);
		}
		break;

	case IOCTL_DPRAM_PHONE_UPLOAD_STEP2:
		err = qc_uload_step2(pld, (void *)arg);
		if (err < 0) {
			enable_irq(pld->irq);
			mif_info("%s: ERR! upload_step2 fail\n", ld->name);
		}
		break;

	default:
		mif_err("%s: ERR! invalid cmd 0x%08X\n", ld->name, cmd);
		err = -EINVAL;
		break;
	}

	return err;
}
#endif

static struct pld_ext_op ext_op_set[] = {
#if defined(CONFIG_CDMA_MODEM_MDM6600)
	[QC_MDM6600] = {
		.exist = 1,
		.init_boot_map = qc_init_boot_map,
		.dload_cmd_handler = qc_dload_cmd_handler,
		.cp_start_handler = qc_start_handler,
		.crash_log = qc_crash_log,
		.ioctl = qc_ioctl,
	},
#endif
#if defined(CONFIG_GSM_MODEM_ESC6270)
	[QC_ESC6270] = {
		.exist = 1,
		.init_boot_map = qc_init_boot_map,
		.dload_cmd_handler = qc_dload_cmd_handler,
		.cp_start_handler = qc_start_handler,
		.crash_log = qc_crash_log,
		.ioctl = qc_ioctl,
	},
#endif
#ifdef CONFIG_CDMA_MODEM_CBP82
	[VIA_CBP82] = {
		.exist = 1,
		.init_boot_map = cbp_init_boot_map,
		.init_dl_map = cbp_init_dl_map,
		.xmit_binary = cbp_xmit_binary,
		.setdl_magic = cbp_xmit_DL_Magic,
	},
#endif

};

struct pld_ext_op *pld_get_ext_op(enum modem_t modem)
{
	if (ext_op_set[modem].exist)
		return &ext_op_set[modem];
	else
		return NULL;
}
