/* linux/drivers/modem/modem.c
 *
 * Copyright (C) 2010 Google, Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/if_arp.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_variation.h"
#include "modem_utils.h"

#include <linux/of_gpio.h>

#define FMT_WAKE_TIME   (HZ/2)
#define RFS_WAKE_TIME   (HZ*3)
#define RAW_WAKE_TIME   (HZ*6)

static struct modem_shared *create_modem_shared_data(void)
{
	struct modem_shared *msd;
	int size = MAX_MIF_BUFF_SIZE;

	msd = kzalloc(sizeof(struct modem_shared), GFP_KERNEL);
	if (!msd)
		return NULL;

	/* initialize tree of io devices */
	msd->iodevs_tree_chan = RB_ROOT;
	msd->iodevs_tree_fmt = RB_ROOT;

	msd->storage.cnt = 0;
	msd->storage.addr = kzalloc(MAX_MIF_BUFF_SIZE +
		(MAX_MIF_SEPA_SIZE * 2), GFP_KERNEL);
	if (!msd->storage.addr) {
		mif_err("IPC logger buff alloc failed!!\n");
		kfree(msd);
		return NULL;
	}

	/* initialize link device list */
	INIT_LIST_HEAD(&msd->link_dev_list);
	INIT_LIST_HEAD(&msd->activated_ndev_list);
	memset(msd->storage.addr, 0, size + (MAX_MIF_SEPA_SIZE * 2));
	memcpy(msd->storage.addr, MIF_SEPARATOR, strlen(MIF_SEPARATOR));
	msd->storage.addr += MAX_MIF_SEPA_SIZE;
	memcpy(msd->storage.addr, &size, MAX_MIF_SEPA_SIZE);
	msd->storage.addr += MAX_MIF_SEPA_SIZE;
	spin_lock_init(&msd->lock);

	return msd;
}

static struct modem_ctl *create_modemctl_device(struct platform_device *pdev,
		struct modem_shared *msd)
{
	int ret = 0;
	struct modem_data *pdata;
	struct modem_ctl *modemctl;
	struct device *dev = &pdev->dev;

	/* create modem control device */
	modemctl = kzalloc(sizeof(struct modem_ctl), GFP_KERNEL);
	if (!modemctl)
		return NULL;

	modemctl->msd = msd;
	modemctl->dev = dev;
	modemctl->phone_state = STATE_OFFLINE;

	pdata = pdev->dev.platform_data;
	modemctl->mdm_data = pdata;
	modemctl->name = pdata->name;

	/* init modemctl device for getting modemctl operations */
	ret = call_modem_init_func(modemctl, pdata);
	if (ret) {
		kfree(modemctl);
		return NULL;
	}

	mif_info("%s is created!!!\n", pdata->name);

	return modemctl;
}

static struct io_device *create_io_device(struct modem_io_t *io_t,
		struct modem_shared *msd, struct modem_ctl *modemctl,
		struct modem_data *pdata)
{
	int ret = 0;
	struct io_device *iod = NULL;

	iod = kzalloc(sizeof(struct io_device), GFP_KERNEL);
	if (!iod) {
		mif_err("iod == NULL\n");
		return NULL;
	}

	RB_CLEAR_NODE(&iod->node_chan);
	RB_CLEAR_NODE(&iod->node_fmt);

	iod->name = io_t->name;
	iod->id = io_t->id;
	iod->format = io_t->format;
	iod->io_typ = io_t->io_type;
	iod->link_types = io_t->links;
	iod->net_typ = pdata->modem_net;
	iod->use_handover = pdata->use_handover;
	iod->ipc_version = pdata->ipc_version;
	iod->attr = io_t->attr;
	iod->rxq_max = io_t->rxq_max;
	iod->multi_len = io_t->multi_len;
	atomic_set(&iod->opened, 0);

	/* link between io device and modem control */
	iod->mc = modemctl;
	if (iod->id == SIPC5_CH_ID_FMT_0) {
		modemctl->iod = iod;
		mif_info("FMT device = %s\n", iod->name);
	}
	if (iod->format == IPC_BOOT && iod->id == 0) {
		modemctl->bootd = iod;
		mif_info("Bood device = %s\n", iod->name);
	}

	/* link between io device and modem shared */
	iod->msd = msd;

	/* add iod to rb_tree */
	if (iod->format != IPC_RAW)
		insert_iod_with_format(msd, iod->format, iod);

	if (sipc5_is_not_reserved_channel(iod->id))
		insert_iod_with_channel(msd, iod->id, iod);

	/* register misc device or net device */
	ret = sipc5_init_io_device(iod);
	if (ret) {
		kfree(iod);
		mif_err("sipc5_init_io_device fail (%d)\n", ret);
		return NULL;
	}

	mif_debug("%s is created!!!\n", iod->name);
	return iod;
}

static int attach_devices(struct io_device *iod, enum modem_link tx_link)
{
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	/* find link type for this io device */
	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld)) {
			/* The count 1 bits of iod->link_types is count
			 * of link devices of this iod.
			 * If use one link device,
			 * or, 2+ link devices and this link is tx_link,
			 * set iod's link device with ld
			 */
			if ((countbits(iod->link_types) <= 1) ||
					(tx_link == ld->link_type)) {
				mif_debug("set %s->%s\n", iod->name, ld->name);
				set_current_link(iod, ld);
			}
		}
	}

	/* if use rx dynamic switch, set tx_link at modem_io_t of
	 * board-*-modems.c
	 */
	if (!get_current_link(iod)) {
		mif_err("%s->link == NULL\n", iod->name);
		BUG();
	}

	switch (iod->format) {
	case IPC_FMT:
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
		iod->waketime = FMT_WAKE_TIME;
		break;

	case IPC_RAW:
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
		iod->waketime = RAW_WAKE_TIME;
		break;

	case IPC_RFS:
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
		iod->waketime = RFS_WAKE_TIME;
		break;

	case IPC_MULTI_RAW:
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
		iod->waketime = RAW_WAKE_TIME;
		break;

	case IPC_BOOT:
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
		iod->waketime = RAW_WAKE_TIME;
		break;

	case IPC_RAW_NCM:
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
		break;

	default:
		break;
	}

	return 0;
}

#ifdef CONFIG_OF
static int parse_dt_common_pdata(struct device_node *np,
		struct modem_data *pdata)
{
	u32 val;

	mif_dt_read_string(np, "mif,name", pdata->name);

	mif_dt_read_enum(np, "mif,modem_net", pdata->modem_net);
	mif_dt_read_enum(np, "mif,modem_type", pdata->modem_type);
	mif_dt_read_enum(np, "mif,link_types", pdata->link_types);
	mif_dt_read_bool(np, "mif,use_handover", pdata->use_handover);
	mif_dt_read_enum(np, "mif,ipc_version", pdata->ipc_version);
	mif_dt_read_u32(np, "mif,num_iodevs", pdata->num_iodevs);

	mif_dt_read_u32(np, "mif,max_link_channel", pdata->max_link_channel);
	mif_dt_read_u32(np, "mif,max_acm_channel", pdata->max_acm_channel);

	/* Optional DT, ignore it if property not exist. */
	pdata->max_tx_qlen = of_property_read_u32(np, "mif,max_tx_qlen", &val)
				? 0 : val;
	return 0;
}

static int parse_dt_iodevs_pdata(struct device_node *np,
		struct modem_data *pdata)
{
	struct device_node *child = NULL;
	struct modem_io_t *iod = NULL;
	int idx = 0;
	u32 val;
	int ret;

	pdata->iodevs = kzalloc(sizeof(struct modem_io_t) * pdata->num_iodevs,
			GFP_KERNEL);
	if (!pdata->iodevs) {
		mif_err("iodevs: failed to alloc memory\n");
		return -ENOMEM;
	}

	for_each_child_of_node(np, child) {
		iod = &pdata->iodevs[idx];

		mif_dt_read_string(child, "iod,name", iod->name);
		mif_info("%s\n", iod->name);
		mif_dt_read_u32(child, "iod,id", iod->id);
		mif_dt_read_enum(child, "iod,format", iod->format);
		mif_dt_read_enum(child, "iod,io_type", iod->io_type);
		mif_dt_read_u32(child, "iod,links", iod->links);
		if (countbits(iod->links) > 1) {
			mif_dt_read_enum(child, "iod,tx_link", iod->tx_link);
			mif_info("tx_link: %d\n", iod->tx_link);
		}
		mif_dt_read_u32(child, "iod,attr", iod->attr);
		ret = of_property_read_u32(child, "iod,rxq_max", &val);
		iod->rxq_max = ret < 0 ? 0 : val;
		ret = of_property_read_u32(child, "iod,multi_len", &val);
		iod->multi_len = ret < 0 ? SIPC_MULTIFMT_LEN : val;
		idx++;
	}

	return 0;
}

static struct modem_data *modem_if_parse_dt_pdata(struct device *dev)
{
	struct modem_data *pdata;
	struct device_node *iodevs = NULL;

	pdata = kzalloc(sizeof(struct modem_data), GFP_KERNEL);
	if (!pdata) {
		mif_err("modem_data: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}

	if (parse_dt_common_pdata(dev->of_node, pdata)) {
		mif_err("DT error: failed to parse common\n");
		goto error;
	}

	iodevs = of_get_child_by_name(dev->of_node, "iodevs");
	if (!iodevs) {
		mif_err("DT error: failed to get child node\n");
		goto error;
	}

	if (parse_dt_iodevs_pdata(iodevs, pdata)) {
		mif_err("DT error: failed to parse iodevs\n");
		goto error;
	}

	dev->platform_data = pdata;
	mif_info("DT parse complete!\n");
	return pdata;

error:
	if (pdata) {
		if (pdata->iodevs)
			kfree(pdata->iodevs);
		kfree(pdata);
	}
	return ERR_PTR(-EINVAL);
}

static const struct of_device_id sec_modem_match[] = {
	{ .compatible = "sec_modem,modem_pdata", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_modem_match);
#else
static struct modem_data *modem_if_parse_dt_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}
#endif

static int __devinit modem_probe(struct platform_device *pdev)
{
	int i;
	struct modem_data *pdata = pdev->dev.platform_data;
	struct modem_shared *msd = NULL;
	struct modem_ctl *modemctl = NULL;
	struct io_device **iod = NULL;
	struct link_device *ld;

	mif_info("%s\n", pdev->name);
	if (pdev->dev.of_node) {
		pdata = modem_if_parse_dt_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			mif_err("MIF DT parse error!\n");
			return PTR_ERR(pdata);
		}
	} else {
		if (!pdata) {
			mif_err("Non-DT, incorrect pdata!\n");
			return -EINVAL;
		}
	}

	msd = create_modem_shared_data();
	if (!msd) {
		mif_err("msd == NULL\n");
		goto err_free_modemctl;
	}

	modemctl = create_modemctl_device(pdev, msd);
	if (!modemctl) {
		mif_err("modemctl == NULL\n");
		goto err_free_modemctl;
	}

	/* create link device */
	/* support multi-link device */
	for (i = 0; i < LINKDEV_MAX; i++) {
		/* find matching link type */
		if (pdata->link_types & LINKTYPE(i)) {
			ld = call_link_init_func(pdev, i);
			if (!ld)
				goto err_free_modemctl;

			mif_err("link created: %s\n", ld->name);
			ld->link_type = i;
			ld->mc = modemctl;
			ld->msd = msd;
			list_add(&ld->list, &msd->link_dev_list);
		}
	}

	iod = kzalloc(sizeof(struct io_device *) * pdata->num_iodevs,
			GFP_KERNEL);
	if (!iod) {
		mif_err("iod_p memory alloc fail\n");
		goto err_free_modemctl;
	}

	/* create io deivces and connect to modemctl device */
	for (i = 0; i < pdata->num_iodevs; i++) {
		iod[i] = create_io_device(&pdata->iodevs[i], msd, modemctl,
				pdata);
		if (!iod[i]) {
			mif_err("iod[%d] == NULL\n", i);
			goto err_free_modemctl;
		}

		attach_devices(iod[i], pdata->iodevs[i].tx_link);
	}
	kfree(iod);

	platform_set_drvdata(pdev, modemctl);

	mif_info("Complete!!!\n");

	return 0;

err_free_modemctl:
	if (iod != NULL) {
		for (i = 0; i < pdata->num_iodevs; i++)
			if (iod[i] != NULL)
				kfree(iod[i]);
		kfree(iod);
	}

	if (modemctl != NULL)
		kfree(modemctl);

	if (msd != NULL)
		kfree(msd);

	return -ENOMEM;
}

static void modem_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct modem_ctl *mc = dev_get_drvdata(dev);
	mc->ops.modem_off(mc);
	mc->phone_state = STATE_OFFLINE;
}

static int modem_suspend(struct device *pdev)
{
#ifndef CONFIG_LINK_DEVICE_HSIC
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->gpio_pda_active && mc->gpio_cp_reset
				&& gpio_get_value(mc->gpio_cp_reset))
		gpio_set_value(mc->gpio_pda_active, 0);
#endif

	return 0;
}

static int modem_resume(struct device *pdev)
{
#ifndef CONFIG_LINK_DEVICE_HSIC
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->gpio_pda_active && mc->gpio_cp_reset
				&& gpio_get_value(mc->gpio_cp_reset))
		gpio_set_value(mc->gpio_pda_active, 1);
#endif
	return 0;
}

static int modem_suspend_noirq(struct device *pdev)
{
#ifdef CONFIG_LINK_DEVICE_HSIC
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->gpio_pda_active && mc->gpio_cp_reset
				&& gpio_get_value(mc->gpio_cp_reset))
		gpio_set_value(mc->gpio_pda_active, 0);
#endif
	return 0;
}

static int modem_resume_noirq(struct device *pdev)
{
#ifdef CONFIG_LINK_DEVICE_HSIC
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->gpio_pda_active && mc->gpio_cp_reset
				&& gpio_get_value(mc->gpio_cp_reset))
		gpio_set_value(mc->gpio_pda_active, 1);
#endif
	return 0;
}

static const struct dev_pm_ops modem_pm_ops = {
	.suspend    = modem_suspend,
	.resume     = modem_resume,
	.suspend_noirq = modem_suspend_noirq,
	.resume_noirq = modem_resume_noirq,
};

static struct platform_driver modem_driver = {
	.probe = modem_probe,
	.shutdown = modem_shutdown,
	.driver = {
		.name = "mif_sipc5",
		.pm   = &modem_pm_ops,
		.suppress_bind_attrs = true,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sec_modem_match),
#endif
	},
};

static int __init modem_init(void)
{
	return platform_driver_register(&modem_driver);
}

late_initcall(modem_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Interface Driver");
