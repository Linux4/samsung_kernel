/**
 * nxp_ptn5150.c - Spreadtrum Type-C Glue layer
 *
 * Copyright (c) 2015 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Xiaogang Liang <xiaogang.liang@spreadtrum.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <linux/mfd/typec.h>
#include <linux/i2c/nxp_ptn5150.h>
#include <soc/sprd/regulator.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>

enum nxp_ptn5150_regs {
	PTN5150_REG_VERSION = 0x01,
	PTN5150_REG_CONTROL = 0x02,
	PTN5150_REG_DETINTS = 0x03,
	PTN5150_REG_CCSTATUS = 0x04,
	PTN5150_REG_CON_DET = 0x30,
	PTN5150_REG_VCONSTATS = 0x31,
	PTN5150_REG_IRQMAST = 0x32,
	PTN5150_REG_IRQSTATUS = 0x33,
};
enum ptn5150_port_status {
	PORT_STATUS_UFP_UNATTACHED = UFP_DFP_STANDBY,
	PORT_STATUS_UFP_ATTACHED = UFP_DETECTED,
	PORT_STATUS_UFP_LOCK = 0xFF,	/* Module debug state machine */
	PORT_STATUS_DFP_UNATTACHED = UFP_DFP_STANDBY,
	PORT_STATUS_DFP_WAIT,	/* Module debug state machine */
	PORT_STATUS_DFP_ATTACHED = DFP_DETECTED,
	PORT_STATUS_POWER_CABLE = 0xFF,
	PORT_STATUS_AUDIO_CABLE = 0xFF,	/* Not support yet */
	PORT_STATUS_DISABLED = 0xFF,	/* Module debug state machine */
};
enum ptn5150_port_type {
	PORT_TYPE_UFP = 1,
	PORT_TYPE_DFP,
	PORT_TYPE_DRP,
};

struct nxp_ptn5150_params_data {
	int irq;
	enum ptn5150_port_type port_type;
};

struct nxp_ptn5150_data {
	struct i2c_client *client;
	struct work_struct isr_work;
	struct nxp_ptn5150_params_data *platform_data;
	enum typec_port_status port_status;
	bool attached;
};
static BLOCKING_NOTIFIER_HEAD(typec_chain_head);

static struct i2c_client *this_client;

static int ptn5150_i2c_rxdata(char *rxdata, int length)
{
	int ret = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = rxdata,
		 },
		{
		 .addr = this_client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxdata,
		 },
	};

	if (i2c_transfer(this_client->adapter, msgs, 2) != 2) {
		ret = -EIO;
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	}

	return ret;
}

static int ptn5150_i2c_txdata(char *txdata, int length)
{
	int ret = 0;

	struct i2c_msg msg[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txdata,
		 },
	};

	if (i2c_transfer(this_client->adapter, msg, 1) != 1) {
		ret = -EIO;
		pr_err("%s i2c write error: %d\n", __func__, ret);
	}

	return ret;
}

/***********************************************************************************************
*Name	:	    ptn5150_write_reg
*
*Input	:               addr -- address
*                            para -- parameter
*
*Output	:
*
*function	:	   write register of ptn5150
*
***********************************************************************************************/
static int ptn5150_write_reg(u8 addr, u8 para)
{
	u8 buf[3];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;
	ret = ptn5150_i2c_txdata(buf, 2);
	if (ret < 0) {
		pr_err("write reg failed! %#x ret: %d\n", buf[0], ret);
		return -1;
	}

	return 0;
}

/***********************************************************************************************
*Name	:	ptn5150_read_reg
*
*Input	:	addr
*                         pdata
*
*Output	:
*
*function	:	read register of ptn5150
*
***********************************************************************************************/
static int ptn5150_read_reg(u8 addr, u8 *pdata)
{
	int ret = 0;
	u8 buf[2] = { addr, 0 };

	struct i2c_msg msgs[] = {
		{
		 .addr = this_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = this_client->addr,
		 .flags = I2C_M_RD,
		 .len = 1,
		 .buf = buf + 1,
		 },
	};

	if (i2c_transfer(this_client->adapter, msgs, 2) != 2) {
		ret = -EIO;
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	}

	*pdata = buf[1];
	return ret;
}

int register_typec_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&typec_chain_head, nb);
}

EXPORT_SYMBOL_GPL(register_typec_notifier);

int unregister_typec_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&typec_chain_head, nb);
}

EXPORT_SYMBOL_GPL(unregister_typec_notifier);

static int typec_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&typec_chain_head, val, NULL)
		== NOTIFY_BAD) ? -EINVAL : 0;
}

static void typec_isr_work(struct work_struct *w)
{
	u8 cabletype, port_type;
	struct nxp_ptn5150_data *nxp_ptn5150_data =
	    container_of(w, struct nxp_ptn5150_data, isr_work);
	port_type = nxp_ptn5150_data->platform_data->port_type;
	if (!nxp_ptn5150_data) {

		pr_info("?ptn5150 nxp_ptn5150_data is null\n");
		return;
	}
	if (!nxp_ptn5150_data->platform_data) {
		pr_info("?ptn5150  platform_data is null\n");
		return;
	}
	if (!nxp_ptn5150_data->attached) {
		pr_info("ptn5150 unattached!\n");
		if (PORT_TYPE_UFP == port_type) {
			typec_notifier_call_chain(TYPEC_STATUS_DEV_DISCONN);
		} else if (PORT_TYPE_DFP == port_type) {
			typec_notifier_call_chain(TYPEC_STATUS_HOST_DISCONN);
		}
		return;
	}

	cabletype = nxp_ptn5150_data->port_status;
	switch (cabletype) {

	case PORT_STATUS_UFP_ATTACHED:

		typec_notifier_call_chain(TYPEC_STATUS_DEV_CONN);
		break;

	case PORT_STATUS_DFP_ATTACHED:

		typec_notifier_call_chain(TYPEC_STATUS_HOST_CONN);
		break;

	default:
		pr_info("Type-C port type %x\n", cabletype);
		break;

	}
}

static int ptn5150_efuse_get_params(unsigned int *param)
{
}

/*TODO: read params from EFUSE*/
static int ptn5150_set_params(struct nxp_ptn5150_data *nxp_ptn5150_typec)
{
}

static irqreturn_t ptn5150_interrupt(int irq, void *dev_id)
{
	struct nxp_ptn5150_data *ptn5150_typec =
	    (struct nxp_ptn5150_data *)dev_id;
	u8 status;

	if (!ptn5150_read_reg(PTN5150_REG_DETINTS, &status) && ptn5150_typec) {
		pr_debug("ptn5150 status reg:0x %x \n", status);
		/* Detach Interrup    Auto-clear after I2C read */
		if (status & DETACHED_INTERRUPT) {
			ptn5150_typec->attached = 0;

		} /* Attached Interrup    Auto-clear after I2C read */
		else if (status & ATTACHED_INTERRUPT) {
			ptn5150_typec->attached = 1;
		}
		if (!ptn5150_read_reg(PTN5150_REG_CCSTATUS, &status)) {
			ptn5150_typec->port_status = status;

		}

		schedule_work(&ptn5150_typec->isr_work);
	} else {
		pr_err("ptn5150 read reg or get dev id error addr:0x %x\n",
		       ptn5150_typec);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static struct nxp_ptn5150_params_data *nxp_ptn5150_dt(struct device *dev)
{
	struct nxp_ptn5150_params_data *pdata;
	struct device_node *np = dev->of_node;
	u8 ret = 0;
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Could not allocate struct nxp_ptn5150_dt");
		goto fail;
	}
	ret = of_property_read_u8(np, "port-type", (u8 *) &pdata->port_type);
	if (ret) {
		dev_err(dev, "fail to get port type\n");
		goto fail;
	}
	if (pdata->port_type < 1 || pdata->port_type > 3)
		pdata->port_type = PORT_TYPE_UFP;
	pdata->irq = irq_of_parse_and_map(np, 0);

	return pdata;
      fail:
	kfree(pdata);
	return NULL;
}
#endif

static int nxp_ptn5150_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct nxp_ptn5150_params_data *ptn5150_params_data =
	    client->dev.platform_data;
	struct device *dev = &client->dev;
	struct nxp_ptn5150_data *ptn5150_typec;
	u8 port_type = 0;
	int err = 0;
	pr_info("==nxp_ptn5150_probe=\n");
#ifdef CONFIG_OF
	if (client->dev.of_node && !ptn5150_params_data) {
		ptn5150_params_data = nxp_ptn5150_dt(&client->dev);
		if (ptn5150_params_data) {
			client->dev.platform_data = ptn5150_params_data;
		} else {
			err = -ENOMEM;
			goto exit_alloc_platform_data_failed;
		}
	}
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	ptn5150_typec =
	    devm_kzalloc(dev, sizeof(*ptn5150_params_data), GFP_KERNEL);
	if (!ptn5150_typec) {
		dev_err(dev, "%s not enough memory\n", __func__);
		return -ENOMEM;
	}

	ptn5150_typec->platform_data = ptn5150_params_data;
	this_client = client;
	ptn5150_typec->client = client;
	ptn5150_set_params(ptn5150_typec);
	i2c_set_clientdata(client, ptn5150_typec);
	client->irq = ptn5150_params_data->irq;
	err = request_irq(client->irq, ptn5150_interrupt,
			  IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name,
			  ptn5150_typec);
	if (err < 0) {
		dev_err(&client->dev, "?ptn5150_probe: request irq failed %d\n",
			err);
	}
	INIT_WORK(&ptn5150_typec->isr_work, typec_isr_work);
	port_type = ptn5150_typec->platform_data->port_type;
	switch (port_type) {
	case PORT_TYPE_DRP:
		err = ptn5150_write_reg(PTN5150_REG_CONTROL, DRP_MODE);
		if (err < 0) {

			pr_err("?ptn5150 write chip  reg error DRP_MODE \n");
		}
		break;
	case PORT_TYPE_DFP:
		err = ptn5150_write_reg(PTN5150_REG_CONTROL, DFP_MODE);
		if (err < 0) {

			pr_err("?ptn5150 write chip  reg error DFP_MODE \n");
		}
		break;
	case PORT_TYPE_UFP:
		err = ptn5150_write_reg(PTN5150_REG_CONTROL, UFP_MODE);
		if (err < 0) {

			pr_err("?ptn5150 write chip  reg error UFP_MODE \n");
		}
	default:

		pr_err("?ptn5150 dts data error port_type unknow \n");
		break;
	}
	enable_irq(client->irq);

      exit_check_functionality_failed:
	ptn5150_typec = NULL;
	i2c_set_clientdata(client, ptn5150_typec);
      exit_alloc_platform_data_failed:
	return err;
	return 0;
}

static int nxp_ptn5150_remove(struct i2c_client *client)
{
	struct nxp_ptn5150_data *ptn_5150_client = i2c_get_clientdata(client);

	pr_info("==nxp_ptn5150_remove=\n");

	free_irq(client->irq, ptn_5150_client);

	kfree(ptn_5150_client);
	ptn_5150_client = NULL;
	i2c_set_clientdata(client, ptn_5150_client);

	return 0;
}

static const struct i2c_device_id ptn5150_id[] = {
	{PTN5150_NAME, 0}, {}
};

MODULE_DEVICE_TABLE(i2c, ptn5150_id);

static const struct of_device_id nxp_of_match[] = {
	{.compatible = "nxp,ptn5150",},
	{}
};

MODULE_DEVICE_TABLE(of, nxp_of_match);
static struct i2c_driver ptn5150_driver = {
	.probe = nxp_ptn5150_probe,
	.remove = nxp_ptn5150_remove,
	.id_table = ptn5150_id,
	.driver = {
		   .name = PTN5150_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = nxp_of_match,
		   },
};

static int __init nxp_ptn5150_init(void)
{
	return i2c_add_driver(&ptn5150_driver);
}

static void __exit nxp_ptn5150_exit(void)
{
	i2c_del_driver(&ptn5150_driver);
}

module_init(nxp_ptn5150_init);
module_exit(nxp_ptn5150_exit);

MODULE_AUTHOR("<xiaogang.liang@spreadtrum.com>");
MODULE_DESCRIPTION("nxp ptn5150 typec driver");
MODULE_LICENSE("GPL");
