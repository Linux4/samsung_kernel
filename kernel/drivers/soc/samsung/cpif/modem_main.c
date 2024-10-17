// SPDX-License-Identifier: GPL-2.0
/* linux/drivers/modem/modem.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/if_arp.h>
#include <linux/device.h>
#include <linux/panic_notifier.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of_reserved_mem.h>
#include <uapi/linux/in.h>
#include <linux/inet.h>
#include <net/ipv6.h>

#if IS_ENABLED(CONFIG_LINK_DEVICE_SHMEM)
#include <soc/samsung/shm_ipc.h>
#include "mcu_ipc.h"
#endif

#include <soc/samsung/exynos-modem-ctrl.h>
#include "modem_prj.h"
#include "modem_variation.h"
#include "modem_utils.h"
#include "modem_ctrl.h"
#include "modem_toe_device.h"

#if IS_ENABLED(CONFIG_MODEM_IF_LEGACY_QOS)
#include "cpif_qos_info.h"
#endif

#define FMT_WAKE_TIME   (msecs_to_jiffies(300))
#define RAW_WAKE_TIME   (HZ*6)
#define NET_WAKE_TIME	(HZ/2)

static struct modem_shared *create_modem_shared_data(
				struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct modem_shared *msd;

	msd = devm_kzalloc(dev, sizeof(struct modem_shared), GFP_KERNEL);
	if (!msd)
		return NULL;

	/* initialize link device list */
	INIT_LIST_HEAD(&msd->link_dev_list);
	INIT_LIST_HEAD(&msd->activated_ndev_list);

	spin_lock_init(&msd->lock);
	spin_lock_init(&msd->active_list_lock);

	return msd;
}

static struct modem_ctl *create_modemctl_device(struct platform_device *pdev,
		struct modem_shared *msd)
{
	struct device *dev = &pdev->dev;
	struct modem_data *pdata = pdev->dev.platform_data;
	struct modem_ctl *modemctl;
	int ret;

	/* create modem control device */
	modemctl = devm_kzalloc(dev, sizeof(struct modem_ctl), GFP_KERNEL);
	if (!modemctl) {
		mif_err("%s: modemctl devm_kzalloc fail\n", pdata->name);
		mif_err("%s: xxx\n", pdata->name);
		return NULL;
	}

	modemctl->dev = dev;
	modemctl->name = pdata->name;
	modemctl->mdm_data = pdata;

	modemctl->msd = msd;

	modemctl->phone_state = STATE_OFFLINE;

	INIT_LIST_HEAD(&modemctl->modem_state_notify_list);
	spin_lock_init(&modemctl->lock);
	spin_lock_init(&modemctl->tx_timer_lock);
	init_completion(&modemctl->init_cmpl);
	init_completion(&modemctl->off_cmpl);

	/* init modemctl device for getting modemctl operations */
	ret = call_modem_init_func(modemctl, pdata);
	if (ret) {
		mif_err("%s: call_modem_init_func fail (err %d)\n",
			pdata->name, ret);
		mif_err("%s: xxx\n", pdata->name);
		devm_kfree(dev, modemctl);
		return NULL;
	}

	/* Register panic notifier_call*/
	modemctl->send_panic_nb.notifier_call = send_panic_to_cp_notifier;
	atomic_notifier_chain_register(&panic_notifier_list, &modemctl->send_panic_nb);

	mif_info("%s is created!!!\n", pdata->name);

	return modemctl;
}

static struct io_device *create_io_device(struct platform_device *pdev,
		struct modem_io_t *io_t, struct modem_shared *msd,
		struct modem_ctl *modemctl, struct modem_data *pdata)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct io_device *iod;

	iod = devm_kzalloc(dev, sizeof(struct io_device), GFP_KERNEL);
	if (!iod) {
		mif_err("iod == NULL\n");
		return NULL;
	}

	INIT_LIST_HEAD(&iod->list);

	iod->name = io_t->name;
	iod->ch = io_t->ch;
	iod->format = io_t->format;
	iod->io_typ = io_t->io_type;
	iod->link_type = io_t->link_type;
	iod->attrs = io_t->attrs;
	atomic_set(&iod->opened, 0);
	spin_lock_init(&iod->info_id_lock);
	spin_lock_init(&iod->clat_lock);

	/* link between io device and modem control */
	iod->mc = modemctl;

	switch (pdata->protocol) {
	case PROTOCOL_SIPC:
		iod->max_tx_size = io_t->ul_buffer_size;
		if (is_fmt_iod(iod) && iod->ch == SIPC5_CH_ID_FMT_0)
			modemctl->iod = iod;
		break;
	case PROTOCOL_SIT:
		if (is_fmt_iod(iod) && iod->ch == EXYNOS_CH_ID_FMT_0)
			modemctl->iod = iod;
		break;
	default:
		mif_err("protocol error\n");
		return NULL;
	}

	if (is_boot_iod(iod)) {
		modemctl->bootd = iod;
		mif_info("BOOT device = %s\n", iod->name);
	}

	/* link between io device and modem shared */
	iod->msd = msd;
	insert_iod_with_channel(msd, iod->ch, iod);

	/* register misc device or net device */
	ret = sipc5_init_io_device(iod, pdata);
	if (ret) {
		mif_err("sipc5_init_io_device fail (%d)\n", ret);

		if (iod->sk_multi_q)
			kvfree(iod->sk_multi_q);
		devm_kfree(dev, iod);

		return NULL;
	}

	mif_info("%s created. attrs:0x%08x\n", iod->name, iod->attrs);
	return iod;
}

static int attach_devices(struct io_device *iod, struct device *dev)
{
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	/* find link type for this io device */
	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld)) {
			mif_debug("set %s->%s\n", iod->name, ld->name);
			set_current_link(iod, ld);

			if (iod->io_typ == IODEV_NET && iod->ndev) {
				struct vnet *vnet;

				vnet = netdev_priv(iod->ndev);
				vnet->hiprio_ack_only = ld->hiprio_ack_only;
			}
		}
	}

	ld = get_current_link(iod);
	if (!ld) {
		mif_err("%s->link == NULL\n", iod->name);
		BUG();
	}

	iod->ws = cpif_wake_lock_register(dev, iod->name);
	if (iod->ws == NULL) {
		mif_err("%s: wakeup_source_register fail\n", iod->name);
		return -EINVAL;
	}

	iod->waketime = 0;

	/* Some iods act like IODEV_NET but its io_typ is not IODEV_NET */
#if IS_ENABLED(CONFIG_LINK_DEVICE_WITH_SBD_ARCH)
	if (ld->sbd_ipc &&
	    iod->ch >= SIPC_CH_ID_PDP_0 && iod->ch <= SIPC_CH_ID_LOOPBACK2)
		iod->waketime = NET_WAKE_TIME;
#else
	if (iod->ch >= EXYNOS_CH_ID_PDP_0 && iod->ch <= EXYNOS_CH_ID_EMBMS_1)
		iod->waketime = NET_WAKE_TIME;
#endif

	if (is_fmt_iod(iod))
		iod->waketime = FMT_WAKE_TIME;
	else if (is_boot_iod(iod) || is_dump_iod(iod))
		iod->waketime = RAW_WAKE_TIME;
	else if (is_ps_iod(iod))
		iod->waketime = NET_WAKE_TIME;

	return 0;
}

static int parse_dt_common_pdata(struct device_node *np,
				 struct modem_data *pdata)
{
	mif_dt_read_string(np, "mif,name", pdata->name);

	mif_dt_read_enum(np, "mif,modem_type", pdata->modem_type);
	mif_dt_read_u32_noerr(np, "mif,protocol", pdata->protocol);
	mif_info("protocol:%d\n", pdata->protocol);

	mif_dt_read_u32(np, "mif,link_type", pdata->link_type);
	mif_dt_read_string(np, "mif,link_name", pdata->link_name);
	mif_dt_read_u32(np, "mif,link_attrs", pdata->link_attrs);
	mif_dt_read_u32(np, "mif,interrupt_types", pdata->interrupt_types);

	mif_dt_read_u32_noerr(np, "mif,capability_check", pdata->capability_check);
	mif_info("capability_check:%d\n", pdata->capability_check);

	return 0;
}

static int parse_dt_mbox_pdata(struct device *dev, struct device_node *np,
					struct modem_data *pdata)
{
	struct modem_mbox *mbox;
	int ret = 0;

	if ((pdata->link_type != LINKDEV_SHMEM) &&
			(pdata->link_type != LINKDEV_PCIE)) {
		mif_err("mbox: link type error:0x%08x\n", pdata->link_type);
		return ret;
	}

	mbox = (struct modem_mbox *)devm_kzalloc(dev, sizeof(struct modem_mbox), GFP_KERNEL);
	if (!mbox) {
		mif_err("mbox: failed to alloc memory\n");
		return -ENOMEM;
	}
	pdata->mbx = mbox;

	mif_dt_read_u32(np, "mif,int_ap2cp_msg", mbox->int_ap2cp_msg);
	mif_dt_read_u32(np, "mif,int_ap2cp_wakeup", mbox->int_ap2cp_wakeup);
	mif_dt_read_u32(np, "mif,int_ap2cp_status", mbox->int_ap2cp_status);
	mif_dt_read_u32(np, "mif,int_ap2cp_active", mbox->int_ap2cp_active);
#if IS_ENABLED(CONFIG_CP_LLC)
	mif_dt_read_u32(np, "mif,int_ap2cp_llc_status", mbox->int_ap2cp_llc_status);
#endif
#if IS_ENABLED(CONFIG_CP_RESET_NOTI)
	mif_dt_read_u32(np, "mif,int_ap2cp_cp_reset_noti", mbox->int_ap2cp_cp_reset_noti);
#endif
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	mif_dt_read_u32(np, "mif,int_ap2cp_pcie_link_ack", mbox->int_ap2cp_pcie_link_ack);
#endif
	mif_dt_read_u32(np, "mif,int_ap2cp_uart_noti", mbox->int_ap2cp_uart_noti);

	mif_dt_read_u32(np, "mif,irq_cp2ap_msg", mbox->irq_cp2ap_msg);
	mif_dt_read_u32(np, "mif,irq_cp2ap_status", mbox->irq_cp2ap_status);
	mif_dt_read_u32(np, "mif,irq_cp2ap_active", mbox->irq_cp2ap_active);
#if IS_ENABLED(CONFIG_CP_LLC)
	mif_dt_read_u32(np, "mif,irq_cp2ap_llc_status", mbox->irq_cp2ap_llc_status);
#endif
	mif_dt_read_u32(np, "mif,irq_cp2ap_wakelock", mbox->irq_cp2ap_wakelock);
	mif_dt_read_u32(np, "mif,irq_cp2ap_ratmode", mbox->irq_cp2ap_rat_mode);

	return ret;
}

static int parse_dt_ipc_region_pdata(struct device *dev, struct device_node *np,
					struct modem_data *pdata)
{
	int ret = 0;

	/* legacy buffer (fmt, raw) setting */
	mif_dt_read_u32(np, "legacy_fmt_head_tail_offset",
			pdata->legacy_fmt_head_tail_offset);
	mif_dt_read_u32(np, "legacy_fmt_buffer_offset", pdata->legacy_fmt_buffer_offset);
	mif_dt_read_u32(np, "legacy_fmt_txq_size", pdata->legacy_fmt_txq_size);
	mif_dt_read_u32(np, "legacy_fmt_rxq_size", pdata->legacy_fmt_rxq_size);
	mif_dt_read_u32(np, "legacy_raw_head_tail_offset",
			pdata->legacy_raw_head_tail_offset);
	mif_dt_read_u32(np, "legacy_raw_buffer_offset", pdata->legacy_raw_buffer_offset);
	mif_dt_read_u32(np, "legacy_raw_txq_size", pdata->legacy_raw_txq_size);
	mif_dt_read_u32(np, "legacy_raw_rxq_size", pdata->legacy_raw_rxq_size);
	mif_dt_read_u32(np, "legacy_raw_rx_buffer_cached",
			pdata->legacy_raw_rx_buffer_cached);

	mif_dt_read_u32_noerr(np, "offset_ap_version", pdata->offset_ap_version);
	mif_dt_read_u32_noerr(np, "offset_cp_version", pdata->offset_cp_version);
	mif_dt_read_u32_noerr(np, "offset_cmsg_offset", pdata->offset_cmsg_offset);
	mif_dt_read_u32_noerr(np, "offset_srinfo_offset", pdata->offset_srinfo_offset);
	mif_dt_read_u32_noerr(np, "offset_clk_table_offset", pdata->offset_clk_table_offset);
	mif_dt_read_u32_noerr(np, "offset_buff_desc_offset", pdata->offset_buff_desc_offset);
	mif_dt_read_u32_noerr(np, "offset_capability_offset", pdata->offset_capability_offset);

#if IS_ENABLED(CONFIG_MODEM_IF_LEGACY_QOS)
	/* legacy priority queue setting */
	mif_dt_read_u32(np, "legacy_raw_qos_head_tail_offset",
			pdata->legacy_raw_qos_head_tail_offset);
	mif_dt_read_u32(np, "legacy_raw_qos_buffer_offset", pdata->legacy_raw_qos_buffer_offset);
	mif_dt_read_u32(np, "legacy_raw_qos_txq_size", pdata->legacy_raw_qos_txq_size);
	mif_dt_read_u32(np, "legacy_raw_qos_rxq_size", pdata->legacy_raw_qos_rxq_size);
#endif

	/* control message offset setting (optional)*/
	mif_dt_read_u32_noerr(np, "cmsg_offset", pdata->cmsg_offset);
	/* srinfo settings */
	mif_dt_read_u32(np, "srinfo_offset", pdata->srinfo_offset);
	mif_dt_read_u32(np, "srinfo_size", pdata->srinfo_size);
	/* clk_table offset (optional)*/
	mif_dt_read_u32_noerr(np, "clk_table_offset", pdata->clk_table_offset);
	/* offset setting for new SIT buffer descriptors (optional) */
	mif_dt_read_u32_noerr(np, "buff_desc_offset", pdata->buff_desc_offset);

	/* offset setting for capability */
	if (pdata->capability_check)
		mif_dt_read_u32(np, "capability_offset", pdata->capability_offset);

	of_property_read_u32_array(np, "ap2cp_msg", pdata->ap2cp_msg, 2);
	of_property_read_u32_array(np, "cp2ap_msg", pdata->cp2ap_msg, 2);
	of_property_read_u32_array(np, "cp2ap_united_status", pdata->cp2ap_united_status, 2);
	of_property_read_u32_array(np, "ap2cp_united_status", pdata->ap2cp_united_status, 2);
#if IS_ENABLED(CONFIG_CP_LLC)
	of_property_read_u32_array(np, "ap2cp_llc_status", pdata->ap2cp_llc_status, 2);
	of_property_read_u32_array(np, "cp2ap_llc_status", pdata->cp2ap_llc_status, 2);
#endif
	of_property_read_u32_array(np, "ap2cp_btl_size", pdata->ap2cp_btl_size, 2);
	of_property_read_u32_array(np, "ap2cp_kerneltime", pdata->ap2cp_kerneltime, 2);
	of_property_read_u32_array(np, "ap2cp_kerneltime_sec", pdata->ap2cp_kerneltime_sec, 2);
	of_property_read_u32_array(np, "ap2cp_kerneltime_usec", pdata->ap2cp_kerneltime_usec, 2);

	/* Status Bit Info */
	mif_dt_read_u32(np, "sbi_lte_active_mask", pdata->sbi_lte_active_mask);
	mif_dt_read_u32(np, "sbi_lte_active_pos", pdata->sbi_lte_active_pos);
	mif_dt_read_u32(np, "sbi_cp_status_mask", pdata->sbi_cp_status_mask);
	mif_dt_read_u32(np, "sbi_cp_status_pos", pdata->sbi_cp_status_pos);
	mif_dt_read_u32(np, "sbi_cp_rat_mode_mask", pdata->sbi_cp2ap_rat_mode_mask);
	mif_dt_read_u32(np, "sbi_cp_rat_mode_pos", pdata->sbi_cp2ap_rat_mode_pos);
	mif_dt_read_u32(np, "sbi_cp2ap_wakelock_mask", pdata->sbi_cp2ap_wakelock_mask);
	mif_dt_read_u32(np, "sbi_cp2ap_wakelock_pos", pdata->sbi_cp2ap_wakelock_pos);
	mif_dt_read_u32(np, "sbi_pda_active_mask", pdata->sbi_pda_active_mask);
	mif_dt_read_u32(np, "sbi_pda_active_pos", pdata->sbi_pda_active_pos);
	mif_dt_read_u32(np, "sbi_tx_flowctl_mask", pdata->sbi_tx_flowctl_mask);
	mif_dt_read_u32(np, "sbi_tx_flowctl_pos", pdata->sbi_tx_flowctl_pos);
	mif_dt_read_u32(np, "sbi_ap_status_mask", pdata->sbi_ap_status_mask);
	mif_dt_read_u32(np, "sbi_ap_status_pos", pdata->sbi_ap_status_pos);
	mif_dt_read_u32(np, "sbi_crash_type_mask", pdata->sbi_crash_type_mask);
	mif_dt_read_u32(np, "sbi_crash_type_pos", pdata->sbi_crash_type_pos);
	mif_dt_read_u32(np, "sbi_uart_noti_mask", pdata->sbi_uart_noti_mask);
	mif_dt_read_u32(np, "sbi_uart_noti_pos", pdata->sbi_uart_noti_pos);
	mif_dt_read_u32(np, "sbi_ds_det_mask", pdata->sbi_ds_det_mask);
	mif_dt_read_u32(np, "sbi_ds_det_pos", pdata->sbi_ds_det_pos);
#if IS_ENABLED(CONFIG_CP_LLC)
	mif_dt_read_u32(np, "sbi_ap_llc_request_mask", pdata->sbi_ap_llc_request_mask);
	mif_dt_read_u32(np, "sbi_ap_llc_request_pos", pdata->sbi_ap_llc_request_pos);
	mif_dt_read_u32(np, "sbi_ap_llc_target_mask", pdata->sbi_ap_llc_target_mask);
	mif_dt_read_u32(np, "sbi_ap_llc_target_pos", pdata->sbi_ap_llc_target_pos);
	mif_dt_read_u32(np, "sbi_ap_llc_way_mask", pdata->sbi_ap_llc_way_mask);
	mif_dt_read_u32(np, "sbi_ap_llc_way_pos", pdata->sbi_ap_llc_way_pos);
	mif_dt_read_u32(np, "sbi_ap_llc_alloc_mask", pdata->sbi_ap_llc_alloc_mask);
	mif_dt_read_u32(np, "sbi_ap_llc_alloc_pos", pdata->sbi_ap_llc_alloc_pos);
	mif_dt_read_u32(np, "sbi_ap_llc_return_mask", pdata->sbi_ap_llc_return_mask);
	mif_dt_read_u32(np, "sbi_ap_llc_return_pos", pdata->sbi_ap_llc_return_pos);
	mif_dt_read_u32(np, "sbi_cp_llc_request_mask", pdata->sbi_cp_llc_request_mask);
	mif_dt_read_u32(np, "sbi_cp_llc_request_pos", pdata->sbi_cp_llc_request_pos);
	mif_dt_read_u32(np, "sbi_cp_llc_target_mask", pdata->sbi_cp_llc_target_mask);
	mif_dt_read_u32(np, "sbi_cp_llc_target_pos", pdata->sbi_cp_llc_target_pos);
	mif_dt_read_u32(np, "sbi_cp_llc_way_mask", pdata->sbi_cp_llc_way_mask);
	mif_dt_read_u32(np, "sbi_cp_llc_way_pos", pdata->sbi_cp_llc_way_pos);
	mif_dt_read_u32(np, "sbi_cp_llc_alloc_mask", pdata->sbi_cp_llc_alloc_mask);
	mif_dt_read_u32(np, "sbi_cp_llc_alloc_pos", pdata->sbi_cp_llc_alloc_pos);
	mif_dt_read_u32(np, "sbi_cp_llc_return_mask", pdata->sbi_cp_llc_return_mask);
	mif_dt_read_u32(np, "sbi_cp_llc_return_pos", pdata->sbi_cp_llc_return_pos);
#endif

	mif_dt_read_u32_noerr(np, "sbi_ap2cp_kerneltime_sec_mask",
			pdata->sbi_ap2cp_kerneltime_sec_mask);
	mif_dt_read_u32_noerr(np, "sbi_ap2cp_kerneltime_sec_pos",
			pdata->sbi_ap2cp_kerneltime_sec_pos);
	mif_dt_read_u32_noerr(np, "sbi_ap2cp_kerneltime_usec_mask",
			pdata->sbi_ap2cp_kerneltime_usec_mask);
	mif_dt_read_u32_noerr(np, "sbi_ap2cp_kerneltime_usec_pos",
			pdata->sbi_ap2cp_kerneltime_usec_pos);

	/* Check pktproc use 36bit addr */
	mif_dt_read_u32(np, "pktproc_use_36bit_addr",
			pdata->pktproc_use_36bit_addr);

	return ret;
}

static int parse_dt_iodevs_pdata(struct device *dev, struct device_node *np,
				 struct modem_data *pdata)
{
	struct device_node *child = NULL;
#if IS_ENABLED(CONFIG_CH_EXTENSION)
#define DUMMY_IOD_NAME	"umts_dummy"

	struct modem_io_t *dummy_p_iod = NULL;
#endif

	for_each_child_of_node(np, child) {
		struct modem_io_t *p_iod = NULL;
		struct modem_io_t *iod;
		unsigned int ch_count = 0;
		char *name;

		do {
			iod = devm_kzalloc(dev, sizeof(struct modem_io_t), GFP_KERNEL);
			if (!iod) {
				mif_err("failed to alloc iodev\n");
				return -ENOMEM;
			}

			if (!p_iod) {
				mif_dt_read_string(child, "iod,name", name);
				mif_dt_read_u32(child, "iod,ch", iod->ch);
				mif_dt_read_enum(child, "iod,format", iod->format);
				mif_dt_read_enum(child, "iod,io_type", iod->io_type);
				mif_dt_read_u32(child, "iod,link_type", iod->link_type);
				mif_dt_read_u32(child, "iod,attrs", iod->attrs);

				if (pdata->protocol == PROTOCOL_SIPC)
					mif_dt_read_u32_noerr(child, "iod,max_tx_size",
							      iod->ul_buffer_size);

				if (iod->attrs & IO_ATTR_SBD_IPC) {
					mif_dt_read_u32(child, "iod,ul_num_buffers",
							iod->ul_num_buffers);
					mif_dt_read_u32(child, "iod,ul_buffer_size",
							iod->ul_buffer_size);
					mif_dt_read_u32(child, "iod,dl_num_buffers",
							iod->dl_num_buffers);
					mif_dt_read_u32(child, "iod,dl_buffer_size",
							iod->dl_buffer_size);
				}

				if (iod->attrs & IO_ATTR_OPTION_REGION)
					mif_dt_read_string(child, "iod,option_region",
							   iod->option_region);

				if (iod->attrs & IO_ATTR_MULTI_CH)
					mif_dt_read_u32(child, "iod,ch_count", iod->ch_count);

				p_iod = iod;
			} else {
				memcpy(iod, p_iod, sizeof(struct modem_io_t));
			}

			if (ch_count < p_iod->ch_count) {
				snprintf(iod->name, sizeof(iod->name), "%s%d", name, ch_count);
				iod->ch = p_iod->ch + ch_count;
			} else {
				snprintf(iod->name, sizeof(iod->name), "%s", name);
			}

			pdata->iodevs[pdata->num_iodevs] = iod;
			pdata->num_iodevs++;

#if IS_ENABLED(CONFIG_CH_EXTENSION)
			if (iod->ch == IOD_CH_EX_ID_PDP_DUMMY ||
			    !strcmp(iod->name, DUMMY_IOD_NAME)) {
				mif_err("dummy iodev is not allowed. ch:%u name:%s\n",
					iod->ch, iod->name);
				return -EINVAL;
			}

			/* Every iods with IODEV_NET should have the same declaration */
			if (!dummy_p_iod && iod->io_type == IODEV_NET)
				dummy_p_iod = p_iod;
#endif
		} while (++ch_count < p_iod->ch_count);
	}

#if IS_ENABLED(CONFIG_CH_EXTENSION)
	if (dummy_p_iod) {
		struct modem_io_t *iod;

		iod = devm_kzalloc(dev, sizeof(*iod), GFP_KERNEL);
		if (!iod) {
			mif_err("failed to alloc dummy iodev\n");
			return -ENOMEM;
		}
		memcpy(iod, dummy_p_iod, sizeof(*iod));

		snprintf(iod->name, sizeof(iod->name), DUMMY_IOD_NAME);
		iod->ch = IOD_CH_EX_ID_PDP_DUMMY;

		pdata->iodevs[pdata->num_iodevs] = iod;
		pdata->num_iodevs++;
	}
#endif

	mif_info("num_iodevs:%d\n", pdata->num_iodevs);

	return 0;
}

static struct modem_data *modem_if_parse_dt_pdata(struct device *dev)
{
	struct modem_data *pdata;
	struct device_node *iodevs_node = NULL;

	pdata = devm_kzalloc(dev, sizeof(struct modem_data), GFP_KERNEL);
	if (!pdata) {
		mif_err("modem_data: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}

	if (parse_dt_common_pdata(dev->of_node, pdata)) {
		mif_err("DT error: failed to parse common\n");
		goto error;
	}

	if (parse_dt_mbox_pdata(dev, dev->of_node, pdata)) {
		mif_err("DT error: failed to parse mbox\n");
		goto error;
	}

	if (parse_dt_ipc_region_pdata(dev, dev->of_node, pdata)) {
		mif_err("DT error: failed to parse control messages\n");
		goto error;
	}

	iodevs_node = of_get_child_by_name(dev->of_node, "iodevs");
	if (!iodevs_node) {
		mif_err("DT error: failed to get child node\n");
		goto error;
	}

	if (parse_dt_iodevs_pdata(dev, iodevs_node, pdata)) {
		mif_err("DT error: failed to parse iodevs\n");
		goto error;
	}

	dev->platform_data = pdata;
	mif_info("DT parse complete!\n");

	return pdata;

error:
	if (pdata) {
		unsigned int id;

		for (id = 0; id < ARRAY_SIZE(pdata->iodevs); id++) {
			if (pdata->iodevs[id])
				devm_kfree(dev, pdata->iodevs[id]);
		}

		devm_kfree(dev, pdata);
	}
	return ERR_PTR(-EINVAL);
}

static const struct of_device_id cpif_dt_match[] = {
	{ .compatible = "samsung,exynos-cp", },
	{},
};
MODULE_DEVICE_TABLE(of, cpif_dt_match);

static ssize_t do_cp_crash_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);
	int type, ret;

	/* Currently, the type is not useful */
	ret = kstrtoint(buf, 0, &type);
	if (ret)
		return -EINVAL;

	mif_info("do_cp_crash type:%d\n", type);
	modem_force_crash_exit(mc);

	return count;
}

static ssize_t modem_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", cp_state_str(mc->phone_state));
}

static DEVICE_ATTR_WO(do_cp_crash);
static DEVICE_ATTR_RO(modem_state);

static struct attribute *modem_attrs[] = {
	&dev_attr_do_cp_crash.attr,
	&dev_attr_modem_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(modem);

static int cpif_cdev_alloc_region(struct modem_data *pdata, struct modem_shared *msd)
{
	int ret;

	ret = alloc_chrdev_region(&msd->cdev_major, 0, pdata->num_iodevs, "cpif");
	if (ret < 0) {
		mif_err("alloc_chrdev_region() failed:%d\n", ret);
		return ret;
	}

	msd->cdev_class = class_create(THIS_MODULE, "cpif");
	if (IS_ERR(msd->cdev_class)) {
		mif_err("class_create() failed:%ld\n", PTR_ERR(msd->cdev_class));
		ret = -ENOMEM;
		unregister_chrdev_region(MAJOR(msd->cdev_major), pdata->num_iodevs);
		return ret;
	}

	return 0;
}

static int cpif_probe(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct modem_data *pdata = dev->platform_data;
	struct modem_shared *msd;
	struct modem_ctl *modemctl;
	struct io_device **iod;
	size_t size;
	struct link_device *ld;
	int err;

#if IS_ENABLED(CONFIG_CPIF_MEMORY_LOGGER)
	cpif_memlog_init(pdev);
#endif

	mif_info("Exynos CP interface driver %s\n", get_cpif_driver_version());
	mif_info("%s: +++ (%s)\n", pdev->name, CONFIG_OPTION_REGION);

	err = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
	if (err) {
		mif_err("dma_set_mask_and_coherent() error:%d\n", err);
		goto fail;
	}

	if (dev->of_node) {
		pdata = modem_if_parse_dt_pdata(dev);
		if (IS_ERR(pdata)) {
			mif_err("MIF DT parse error!\n");
			err = PTR_ERR(pdata);
			goto fail;
		}
	} else {
		if (!pdata) {
			mif_err("Non-DT, incorrect pdata!\n");
			err = -EINVAL;
			goto fail;
		}
	}

	msd = create_modem_shared_data(pdev);
	if (!msd) {
		mif_err("%s: msd == NULL\n", pdata->name);
		err = -ENOMEM;
		goto fail;
	}

	modemctl = create_modemctl_device(pdev, msd);
	if (!modemctl) {
		mif_err("%s: modemctl == NULL\n", pdata->name);
		devm_kfree(dev, msd);
		err = -ENOMEM;
		goto fail;
	}

	if (toe_dev_create(pdev)) {
		mif_err("%s: toe dev not created\n", pdata->name);
		goto free_mc;
	}

	/* create link device */
	ld = call_link_init_func(pdev, pdata->link_type);
	if (!ld)
		goto free_mc;

	mif_info("%s: %s link created\n", pdata->name, ld->name);

	ld->mc = modemctl;
	ld->msd = msd;
	list_add(&ld->list, &msd->link_dev_list);

	/* char device */
	err = cpif_cdev_alloc_region(pdata, msd);
	if (err) {
		mif_err("cpif_cdev_alloc_region() err:%d\n", err);
		goto free_mc;
	}

	/* create io deivces and connect to modemctl device */
	size = sizeof(struct io_device *) * pdata->num_iodevs;
	iod = kzalloc(size, GFP_KERNEL);
	if (!iod) {
		mif_err("kzalloc() err\n");
		goto free_chrdev;
	}

	for (i = 0; i < pdata->num_iodevs; i++) {
		if (pdata->iodevs[i]->attrs & IO_ATTR_OPTION_REGION &&
		    strcmp(pdata->iodevs[i]->option_region, CONFIG_OPTION_REGION))
			continue;

		iod[i] = create_io_device(pdev, pdata->iodevs[i], msd,
					  modemctl, pdata);
		if (!iod[i]) {
			mif_err("%s: iod[%d] == NULL\n", pdata->name, i);
			goto free_iod;
		}

		/* Basically, iods of IPC_FMT and IPC_BOOT will receive the state */
		if (is_fmt_iod(iod[i]) || is_boot_iod(iod[i]) ||
		    iod[i]->attrs & IO_ATTR_STATE_RESET_NOTI)
			list_add_tail(&iod[i]->list, &modemctl->modem_state_notify_list);

		attach_devices(iod[i], dev);
	}

	cp_btl_create(&pdata->btl, dev);

	platform_set_drvdata(pdev, modemctl);

	kfree(iod);

	if (sysfs_create_groups(&dev->kobj, modem_groups))
		mif_err("failed to create modem groups node\n");

#if IS_ENABLED(CONFIG_MODEM_IF_LEGACY_QOS)
	err = cpif_qos_init_list();
	if (err < 0)
		mif_err("failed to initialize hiprio list(%d)\n", err);
#endif

#if IS_ENABLED(CONFIG_CPIF_VENDOR_HOOK)
	err = hook_init();
	if (err)
		mif_err("failed to register vendor hook\n");
#endif

	mif_info("%s: done ---\n", pdev->name);
	return 0;

free_iod:
	for (i = 0; i < pdata->num_iodevs; i++) {
		if (iod[i]) {
			sipc5_deinit_io_device(iod[i]);
			devm_kfree(dev, iod[i]);
		}
	}
	kfree(iod);

free_chrdev:
	class_destroy(msd->cdev_class);
	unregister_chrdev_region(MAJOR(msd->cdev_major), pdata->num_iodevs);

free_mc:
	if (modemctl)
		devm_kfree(dev, modemctl);

	if (msd)
		devm_kfree(dev, msd);

	err = -ENOMEM;

fail:
	mif_err("%s: xxx\n", pdev->name);

	panic("CP interface driver probe failed\n");
	return err;
}

static void cpif_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct modem_ctl *mc = dev_get_drvdata(dev);

	if (mc->ops.power_shutdown)
		mc->ops.power_shutdown(mc);

	mc->phone_state = STATE_OFFLINE;

	mif_err("%s\n", mc->name);
}

static int modem_suspend(struct device *pdev)
{
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->ops.suspend)
		mc->ops.suspend(mc);

	set_wakeup_packet_log(true);

	return 0;
}

static int modem_resume(struct device *pdev)
{
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	set_wakeup_packet_log(false);

	if (mc->ops.resume)
		mc->ops.resume(mc);

	return 0;
}

static const struct dev_pm_ops cpif_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(modem_suspend, modem_resume)
};

static struct platform_driver cpif_driver = {
	.probe = cpif_probe,
	.shutdown = cpif_shutdown,
	.driver = {
		.name = "cp_interface",
		.owner = THIS_MODULE,
		.pm = &cpif_pm_ops,
		.suppress_bind_attrs = true,
		.of_match_table = cpif_dt_match,
	},
};

module_platform_driver(cpif_driver);

MODULE_DESCRIPTION("Exynos CP interface Driver");
MODULE_LICENSE("GPL");
