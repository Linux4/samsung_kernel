/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>


#include <linux/sipc.h>
#include <linux/sipc_priv.h>

#define SMSG_TXBUF_ADDR		(0)
#define SMSG_TXBUF_SIZE		(SZ_1K)
#define SMSG_RXBUF_ADDR		(SMSG_TXBUF_SIZE)
#define SMSG_RXBUF_SIZE		(SZ_1K)

#define SMSG_RINGHDR		(SMSG_TXBUF_SIZE + SMSG_RXBUF_SIZE)
#define SMSG_TXBUF_RDPTR	(SMSG_RINGHDR + 0)
#define SMSG_TXBUF_WRPTR	(SMSG_RINGHDR + 4)
#define SMSG_RXBUF_RDPTR	(SMSG_RINGHDR + 8)
#define SMSG_RXBUF_WRPTR	(SMSG_RINGHDR + 12)


#define DEFINE_SIPC_RXIRQ_STATUS_FN(id) \
static int sipc_rxirq_status##id(void) \
{ \
	return 1;\
}

#define DEFINE_SIPC_RXIRQ_CLEAR_FN(id) \
static void sipc_rxirq_clear##id(void) \
{ \
	struct sipc_child_node_info *info = &sipc_dev->pdata->info_table[id];\
	__raw_writel(info->ap2cp_bit_clr, (volatile void *)info->cp2ap_int_ctrl);\
}

#define DEFINE_SIPC_TXIRQ_TRIGGER_FN(id) \
static void sipc_txirq_trigger##id(void) \
{ \
	struct sipc_child_node_info *info = &sipc_dev->pdata->info_table[id];\
	__raw_writel(info->ap2cp_bit_trig, (volatile void *)info->ap2cp_int_ctrl);\
}


#define __GET_SIPC_FN_NAME(fn, id) fn##id

#define __GET_SIPC_FN(fn, id) \
({ \
	void* ret; \
	switch(id) { \
		case 0: \
			ret = __GET_SIPC_FN_NAME(fn, 0);\
			break; \
		case 1: \
			ret = __GET_SIPC_FN_NAME(fn, 1);\
			break; \
		case 2: \
			ret = __GET_SIPC_FN_NAME(fn, 2);\
			break; \
		default: \
			break; \
	} \
	ret; \
}) \

#define GET_SIPC_RXIRQ_STATUS_FN(id) __GET_SIPC_FN(sipc_rxirq_status, id)
#define GET_SIPC_RXIRQ_CLEAR_FN(id) __GET_SIPC_FN(sipc_rxirq_clear, id)
#define GET_SIPC_TXIRQ_TRIGGER_FN(id) __GET_SIPC_FN(sipc_txirq_trigger, id)

struct sipc_child_node_info {
	uint8_t dst;
	char * name;

	uint32_t cp_base;
	uint32_t cp_size;

	uint32_t ring_base;
	uint32_t ring_size;

	uint32_t smem_base;
	uint32_t smem_size;

	uint32_t ap2cp_int_ctrl;
	uint32_t cp2ap_int_ctrl;
	uint32_t ap2cp_bit_trig;
	uint32_t ap2cp_bit_clr;

	uint32_t irq;
};

struct sipc_init_data {
	int is_alloc;
	uint32_t chd_nr;
	uint32_t smem_base;
	uint32_t smem_size;
	struct sipc_child_node_info info_table[0];
};

struct sipc_device {
	int status;
	uint32_t inst_nr;
	struct sipc_init_data *pdata;
	struct smsg_ipc *smsg_inst;
};

struct sipc_device *sipc_dev;

DEFINE_SIPC_TXIRQ_TRIGGER_FN(0)
DEFINE_SIPC_TXIRQ_TRIGGER_FN(1)
DEFINE_SIPC_TXIRQ_TRIGGER_FN(2)

DEFINE_SIPC_RXIRQ_CLEAR_FN(0)
DEFINE_SIPC_RXIRQ_CLEAR_FN(1)
DEFINE_SIPC_RXIRQ_CLEAR_FN(2)

DEFINE_SIPC_RXIRQ_STATUS_FN(0)
DEFINE_SIPC_RXIRQ_STATUS_FN(1)
DEFINE_SIPC_RXIRQ_STATUS_FN(2)


static int sipc_create(struct sipc_device *sipc)
{
	struct sipc_init_data *pdata = sipc->pdata;
	struct smsg_ipc *inst;
	struct sipc_child_node_info *info;
	uint32_t base, num;
	int ret, i;

	if (!pdata) {
		return -ENODEV;
	}

	num = pdata->chd_nr;
	inst = sipc->smsg_inst;
	info = pdata->info_table;

	for (i = 0; i < num; i++) {
		base = (uint32_t)ioremap((uint32_t)info[i].ring_base, info[i].ring_size);
		pr_info("sipc:[%d] after ioremap vbase=0x%x, pbase=0x%x, size=0x%x\n",
			i, base, info[i].ring_base, info[i].ring_size);

		inst[i].txbuf_size = SMSG_TXBUF_SIZE / sizeof(struct smsg);
		inst[i].txbuf_addr = base + SMSG_TXBUF_ADDR;
		inst[i].txbuf_rdptr = base + SMSG_TXBUF_RDPTR;
		inst[i].txbuf_wrptr = base + SMSG_TXBUF_WRPTR;

		inst[i].rxbuf_size = SMSG_RXBUF_SIZE / sizeof(struct smsg);
		inst[i].rxbuf_addr = base + SMSG_RXBUF_ADDR;
		inst[i].rxbuf_rdptr = base + SMSG_RXBUF_RDPTR;
		inst[i].rxbuf_wrptr = base + SMSG_RXBUF_WRPTR;

		ret = smsg_ipc_create(inst[i].dst, &inst[i]);

		pr_info("sipc:[%d] created\n", i);
		if (ret) {
			break;
		}
	}
	return ret;
}


static int sipc_parse_dt(struct sipc_init_data **init, struct device *dev)
{
	struct device_node *np = dev->of_node, *nchd;
	struct sipc_init_data *pdata;
	struct sipc_child_node_info *info;
	struct resource res;
	uint32_t data, nr;
	int ret, i;

	nr = of_get_child_count(np);
	if (nr == 0 || nr >= SIPC_ID_NR) {
		return (!nr ? -ENODEV : -EINVAL);
	}
	pdata = kzalloc(sizeof(struct sipc_init_data) + nr*sizeof(struct sipc_child_node_info), GFP_KERNEL);
	if (!pdata) {
		pr_err("sipc: failed to alloc mem for pdata\n");
		return -ENOMEM;
	}
	pdata->is_alloc = 1;

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		goto error;
	}
	pdata->chd_nr = nr;
	pdata->smem_base = (uint32_t)res.start;
	pdata->smem_size = (uint32_t)(res.end - res.start + 1);
	pr_info("sipc: chd_nr=%d smem_base=0x%x, smem_size=0x%x\n", pdata->chd_nr, pdata->smem_base, pdata->smem_size);

	i = 0;
	info = pdata->info_table;

	for_each_child_of_node(np, nchd) {
		ret = of_property_read_string(nchd, "sprd,name", (const char **)&info[i].name);
		if (ret) {
			goto error;
		}
		pr_info("sipc:[%d] name=%s\n", i, info[i].name);

		ret = of_property_read_u32(nchd, "sprd,dst", &data);
		if (ret) {
			goto error;
		}
		info[i].dst = (uint8_t)data;
		pr_info("sipc:[%d] dst=%u\n", i, info[i].dst);

		/* get ipi base addr */
		ret = of_property_read_u32(nchd, "sprd,ap2cp", &info[i].ap2cp_int_ctrl);
		if (ret) {
			goto error;
		}

		ret = of_property_read_u32(nchd, "sprd,cp2ap", &info[i].cp2ap_int_ctrl);
		if (ret) {
			goto error;
		}
		pr_info("sipc:[%d] ap2cp_int_ctrl=0x%x, cp2ap_int_ctrl=0x%x\n",
			i, info[i].ap2cp_int_ctrl, info[i].cp2ap_int_ctrl);

		ret = of_property_read_u32(nchd, "sprd,trig", &info[i].ap2cp_bit_trig);
		if (ret) {
			goto error;
		}

		ret = of_property_read_u32(nchd, "sprd,clr", &info[i].ap2cp_bit_clr);
		if (ret) {
			goto error;
		}
		pr_info("sipc:[%d] ap2cp_bit_trig=0x%x, ap2cp_bit_clr=0x%x\n",
			i, info[i].ap2cp_bit_trig, info[i].ap2cp_bit_clr);

		/* get cp base addr */
		ret = of_address_to_resource(nchd, 0, &res);
		if (ret) {
			goto error;
		}
		info[i].cp_base = (uint32_t)res.start;
		info[i].cp_size = (uint32_t)(res.end - res.start + 1);
		pr_info("sipc:[%d] cp_base=0x%x, cp_size=0x%x\n",
			i, info[i].cp_base, info[i].cp_size);

		/* get smem base addr */
		ret = of_address_to_resource(nchd, 1, &res);
		if (ret) {
			goto error;
		}
		info[i].smem_base = (uint32_t)res.start;
		info[i].smem_size = (uint32_t)(res.end - res.start + 1);
		pr_info("sipc:[%d] smem_base=0x%x, smem_size=0x%x\n", i, info[i].smem_base, info[i].smem_size);

		/* get ring base addr */
		ret = of_address_to_resource(nchd, 2, &res);
		if (ret) {
			goto error;
		}
		info[i].ring_base = (uint32_t)res.start;
		info[i].ring_size = (uint32_t)(res.end - res.start + 1);
		pr_info("sipc:[%d] ring_base=0x%x, ring_size=0x%x\n", i, info[i].ring_base, info[i].ring_size);

		/* get irq */
		info[i].irq = irq_of_parse_and_map(nchd, 0);
		if (!info[i].irq) {
			ret = -EINVAL;
			goto error;
		}
		pr_info("sipc:[%d] irq=%d\n", i, info[i].irq);

		i++;
	}
	*init = pdata;
	return 0;
error:
	kfree(pdata);
	return ret;
}

static void sipc_destroy_pdata(struct sipc_init_data **ppdata)
{
	struct sipc_init_data *pdata = ppdata;
	if (pdata && pdata->is_alloc) {
		kfree(pdata);
	}
	return;
}

static int sipc_probe(struct platform_device *pdev)
{
	struct sipc_init_data *pdata = pdev->dev.platform_data;
	struct sipc_device *sipc;
	struct sipc_child_node_info *info;
	struct smsg_ipc *smsg;
	uint32_t num;
	int ret, i;

	pr_info("sipc:[0] define status=0x%x, clear=0x%x, trigger=0x%x\n",
		(uint32_t)sipc_rxirq_status0, (uint32_t)sipc_rxirq_clear0, (uint32_t)sipc_txirq_trigger0);

	pr_info("sipc:[1] define status=0x%x, clear=0x%x, trigger=0x%x\n",
		(uint32_t)sipc_rxirq_status1, (uint32_t)sipc_rxirq_clear1, (uint32_t)sipc_txirq_trigger1);

	pr_info("sipc:[2] define status=0x%x, clear=0x%x, trigger=0x%x\n",
		(uint32_t)sipc_rxirq_status2, (uint32_t)sipc_rxirq_clear2, (uint32_t)sipc_txirq_trigger2);

	if (!pdata && pdev->dev.of_node) {
		ret = sipc_parse_dt(&pdata, &pdev->dev);
		if (ret) {
			pr_err("sipc: failed to parse dt, ret=%d\n", ret);
			return -ENODEV;
		}
	}

	if (!pdata) {
		pr_err("sipc: pdata is NULL!!\n");
		return -ENOMEM;
	}

	sipc = kzalloc(sizeof(struct sipc_device), GFP_KERNEL);
	if (!sipc) {
		sipc_destroy_pdata(&pdata);
		return -ENOMEM;
	}

	num = pdata->chd_nr;
	smsg = kzalloc(num * sizeof(struct smsg_ipc), GFP_KERNEL);
	if (!smsg)
	{
		sipc_destroy_pdata(&pdata);
		kfree(sipc);
		return -ENOMEM;
	}
	pr_info("sipc: num=%d\n", num);
	info = pdata->info_table;
	for (i = 0; i < num; i++) {
		smsg[i].name = info[i].name;
		smsg[i].dst = info[i].dst;
		smsg[i].irq = info[i].irq;
		smsg[i].rxirq_status = GET_SIPC_RXIRQ_STATUS_FN(i);
		smsg[i].rxirq_clear = GET_SIPC_RXIRQ_CLEAR_FN(i);
		smsg[i].txirq_trigger = GET_SIPC_TXIRQ_TRIGGER_FN(i);

		pr_info("sipc: [%d] fn status=0x%x, clear=0x%x, trigger=0x%x\n",
			i, (uint32_t)smsg[i].rxirq_status, (uint32_t)smsg[i].rxirq_clear,
			(uint32_t)smsg[i].txirq_trigger);
		pr_info("sipc:[%d] smsg name=%s, dst=%u, irq=%d\n",
			i, smsg[i].name, smsg[i].dst, smsg[i].irq);
	}

	sipc->pdata = pdata;
	sipc->smsg_inst = smsg;
	sipc_dev = sipc;

	smsg_suspend_init();

	sipc_create(sipc);

	pr_info("sipc: smem_base=0x%x, smem_size=0x%x\n", pdata->smem_base, pdata->smem_size);
	smem_init(pdata->smem_base, pdata->smem_size);

	platform_set_drvdata(pdev, sipc);
	return 0;
}

static int sipc_remove(struct platform_device *pdev)
{
	struct sipc_device *sipc = platform_get_drvdata(pdev);

	sipc_destroy_pdata(&sipc->pdata);
	kfree(sipc->smsg_inst);
	kfree(sipc);
	return 0;
}


static const struct of_device_id sipc_match_table[] = {
        {.compatible = "sprd,sipc", },
        { },
};

static struct platform_driver sipc_driver = {
        .driver = {
                .owner = THIS_MODULE,
                .name = "sipc",
                .of_match_table = sipc_match_table,
        },
        .probe = sipc_probe,
        .remove = sipc_remove,
};

static int __init sipc_init(void)
{
	return platform_driver_register(&sipc_driver);
}

static void __exit sipc_exit(void)
{
        platform_driver_unregister(&sipc_driver);
}

arch_initcall(sipc_init);
module_exit(sipc_exit);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC module driver");
MODULE_LICENSE("GPL");
