/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#ifdef CONFIG_SPRD_MAILBOX
#include <soc/sprd/mailbox.h>
#else
#define SPRD_DEV_P2V(paddr)	(paddr)
#define SPRD_DEV_V2P(vaddr)	(vaddr)
#endif

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

struct sipc_core sipc_ap;
EXPORT_SYMBOL(sipc_ap);

/* if it's upon mailbox arch, overwrite the implementation*/
#ifdef CONFIG_SPRD_MAILBOX
#ifdef CONFIG_SPRD_MAILBOX_FIFO
static uint32_t sipc_rxirq_status(uint8_t id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	return mbox_core_fifo_full(info->core_id);
}

static void sipc_rxirq_clear(uint8_t id)
{

}

static void sipc_txirq_trigger(uint8_t id, u64 msg)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	mbox_raw_sent(info->core_id, msg);
}
#else
static uint32_t sipc_rxirq_status(uint8_t id)
{
	return 1;
}

#define DEFINE_SIPC_RXIRQ_CLEAR_FN(id)
static void sipc_rxirq_clear(uint8_t id)
{

}

static void sipc_txirq_trigger(uint8_t id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	mbox_raw_sent(info->core_id, 0);
}
#endif
#else
static uint32_t sipc_rxirq_status(uint8_t id)
{
	return 1;
}

static void sipc_rxirq_clear(uint8_t id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	__raw_writel(info->ap2cp_bit_clr,
		(volatile void __iomem *)info->cp2ap_int_ctrl);
}

static void sipc_txirq_trigger(uint8_t id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	__raw_writel(info->ap2cp_bit_trig,
	(volatile void __iomem *)info->ap2cp_int_ctrl);
}

#endif
static int sipc_create(struct sipc_device *sipc)
{
	struct sipc_init_data *pdata = sipc->pdata;
	struct smsg_ipc *inst;
	struct sipc_child_node_info *info;
	void __iomem *base;
	uint32_t num;
	int ret, i, j = 0;

	if (!pdata)
		return -ENODEV;

	num = pdata->chd_nr;
	inst = sipc->smsg_inst;
	info = pdata->info_table;

	for (i = 0; i < num; i++) {
		if (j < pdata->newchd_nr && info[i].is_new) {
			base = (void __iomem *)shmem_ram_vmap_nocache((uint32_t)
						       info[i].ring_base,
						       info[i].ring_size);

			if(base == NULL){
				pr_info("sipc chd%d ioremap return 0\n", i);
				return ENOMEM;
			}
			pr_info("sipc:[tag%d] after ioremap vbase=0x%p, pbase=0x%x, size=0x%x\n",
				j, base, info[i].ring_base, info[i].ring_size);
			inst[j].id = sipc_ap.sipc_tag_ids;
			sipc_ap.sipc_tags[sipc_ap.sipc_tag_ids] = &info[i];
			sipc_ap.sipc_tag_ids++;
			inst[j].txbuf_size = SMSG_TXBUF_SIZE / sizeof(struct smsg);
			inst[j].txbuf_addr = (uintptr_t)base + SMSG_TXBUF_ADDR;
			inst[j].txbuf_rdptr = (uintptr_t)base + SMSG_TXBUF_RDPTR;
			inst[j].txbuf_wrptr = (uintptr_t)base + SMSG_TXBUF_WRPTR;

			inst[j].rxbuf_size = SMSG_RXBUF_SIZE / sizeof(struct smsg);
			inst[j].rxbuf_addr = (uintptr_t)base + SMSG_RXBUF_ADDR;
			inst[j].rxbuf_rdptr = (uintptr_t)base + SMSG_RXBUF_RDPTR;
			inst[j].rxbuf_wrptr = (uintptr_t)base + SMSG_RXBUF_WRPTR;

			ret = smsg_ipc_create(inst[j].dst, &inst[j]);

			pr_info("sipc:[tag%d] created, dst = %d\n", j, inst[j].dst);
			j++;
			if (ret)
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
#ifndef CONFIG_SPRD_MAILBOX
	uint32_t int_ctrl_phy[2];
#endif
	uint32_t data, nr;
	int ret, i;

	nr = of_get_child_count(np);
	if (nr == 0 || nr >= SIPC_ID_NR)
		return (!nr ? -ENODEV : -EINVAL);
	pdata = devm_kzalloc(dev,
			     sizeof(struct sipc_init_data) +
			     nr*sizeof(struct sipc_child_node_info),
			     GFP_KERNEL);
	if (!pdata) {
		pr_err("sipc: failed to alloc mem for pdata\n");
		return -ENOMEM;
	}

	pdata->is_alloc = 1;
	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		goto error;
	pdata->chd_nr = nr;
#ifdef CONFIG_SPRD_SIPC_MULTI_SMEM
	pdata->name = (char *)res.name;
#else
	pdata->name = NULL;
#endif
	pdata->smem_base = (uint32_t)res.start;
	pdata->smem_size = (uint32_t)(res.end - res.start + 1);
	pr_info("sipc: name=%s, chd_nr=%d smem_base=0x%x, smem_size=0x%x\n",
		pdata->name, pdata->chd_nr, pdata->smem_base, pdata->smem_size);

	i = 0;
	info = pdata->info_table;

	for_each_child_of_node(np, nchd) {
		ret = of_property_read_string(nchd, "sprd,name",
					      (const char **)&info[i].name);
		if (ret)
			goto error;
		pr_info("sipc:[%d] name=%s\n", i, info[i].name);

		ret = of_property_read_u32(nchd, "sprd,dst", &data);
		if (ret)
			goto error;
		info[i].dst = (uint8_t)data;
		pr_info("sipc:[tag%d] dst=%u\n", i, info[i].dst);
		if (smsg_ipcs[info[i].dst] == NULL) {
			pdata->newchd_nr++;
			info[i].is_new = 1;
		} else {
			info[i].is_new = 0;
		}

#ifndef CONFIG_SPRD_MAILBOX
		/* get ipi base addr */
		ret = of_property_read_u32(nchd,
					   "sprd,ap2cp",
					   &int_ctrl_phy[0]);
		if (ret)
			goto error;
		info[i].ap2cp_int_ctrl = SPRD_DEV_P2V(int_ctrl_phy[0]);

		ret = of_property_read_u32(nchd,
					   "sprd,cp2ap",
					   &int_ctrl_phy[1]);
		if (ret)
			goto error;
		info[i].cp2ap_int_ctrl = SPRD_DEV_P2V(int_ctrl_phy[1]);

		pr_info("sipc:[tag%d] ap2cp_int_ctrl=0x%lx, ap2cp_ctrl_phy=0x%x, cp2ap_int_ctrl=0x%lx, cp2ap_ctrl_phy=0x%x\n",
			i, info[i].ap2cp_int_ctrl,
			int_ctrl_phy[0],
			info[i].cp2ap_int_ctrl,
			int_ctrl_phy[1]);

		ret = of_property_read_u32(nchd,
					   "sprd,trig",
					   &info[i].ap2cp_bit_trig);
		if (ret)
			goto error;

		ret = of_property_read_u32(nchd,
					   "sprd,clr",
					   &info[i].ap2cp_bit_clr);
		if (ret)
			goto error;
		pr_info("sipc:[%d] ap2cp_bit_trig=0x%x, ap2cp_bit_clr=0x%x\n",
			i, info[i].ap2cp_bit_trig, info[i].ap2cp_bit_clr);
#endif
		if (strcmp(info[i].name, "sipc-pmsys") != 0) {
			/* get cp base addr */
			ret = of_address_to_resource(nchd, 0, &res);
			if (ret)
				goto error;
			info[i].cp_base = (uint32_t)res.start;
			info[i].cp_size = (uint32_t)(res.end - res.start + 1);
			pr_info("sipc:[tag%d] cp_base=0x%x, cp_size=0x%x\n",
				i, info[i].cp_base, info[i].cp_size);

			/* get smem base addr */
			ret = of_address_to_resource(nchd, 1, &res);
			if (ret)
				goto error;
			info[i].smem_base = (uint32_t)res.start;
			info[i].smem_size = (uint32_t)(res.end - res.start + 1);
			pr_info("sipc:[tag%d] smem_base=0x%x, smem_size=0x%x\n",
				i, info[i].smem_base, info[i].smem_size);

			/* get ring base addr */
			ret = of_address_to_resource(nchd, 2, &res);
			if (ret)
				goto error;
			info[i].ring_base = (uint32_t)res.start;
			info[i].ring_size = (uint32_t)(res.end - res.start + 1);
			pr_info("sipc:[tag%d] ring_base=0x%x, ring_size=0x%x\n",
				i, info[i].ring_base, info[i].ring_size);
		} else {
#ifdef CONFIG_SPRD_SIPC_MULTI_SMEM
			/* get ring base addr */
			ret = of_address_to_resource(nchd, 0, &res);
			if (ret)
				goto error;
			info[i].smem_base = (uint32_t)res.start;
			info[i].smem_size = (uint32_t)(res.end - res.start + 1);
			pr_info("sipc:[tag%d] pmsys smem_base=0x%x, smem_size=0x%x\n", i, info[i].ring_base, info[i].ring_size);

			/* get ring base addr */
			ret = of_address_to_resource(nchd, 1, &res);
#else
			ret = of_address_to_resource(nchd, 0, &res);
#endif
			if (ret)
				goto error;

			info[i].ring_base = (uint32_t)res.start;
			info[i].ring_size = (uint32_t)(res.end - res.start + 1);
			pr_info("sipc:[tag%d] pmsys ring_base=0x%x, ring_size=0x%x\n",
				i, info[i].ring_base, info[i].ring_size);
		}

#ifdef CONFIG_SPRD_MAILBOX
		ret = of_property_read_u32(nchd,
					   "mailbox,core",
					   &info[i].core_id);
		if (ret)
			goto error;
		pr_info("sipc:[tag%d] core_id=%u\n", i, info[i].core_id);
#else
		/* get irq */
		info[i].irq = irq_of_parse_and_map(nchd, 0);
		if (!info[i].irq) {
			ret = -EINVAL;
			goto error;
		}
		pr_info("sipc:[tag%d] irq=%d\n", i, info[i].irq);
#endif
		i++;
	}
	*init = pdata;
	return 0;
error:
	devm_kfree(dev, pdata);
	return ret;
}

static void sipc_destroy_pdata(struct sipc_init_data **ppdata, struct device *dev)
{
	struct sipc_init_data *pdata = *ppdata;

	if (pdata && pdata->is_alloc)
		devm_kfree(dev, pdata);;
}

static int sipc_probe(struct platform_device *pdev)
{
	struct sipc_init_data *pdata = pdev->dev.platform_data;
	struct sipc_device *sipc;
	struct sipc_child_node_info *info;
	struct smsg_ipc *smsg;
	uint32_t num, len, sid;
	int ret, i, j = 0;

	if (!pdata && pdev->dev.of_node) {
		ret = sipc_parse_dt(&pdata, &pdev->dev);
		if (ret) {
			pr_err("sipc: failed to parse dt, ret=%d\n", ret);
			return -ENODEV;
		}
	}

	sipc = devm_kzalloc(&pdev->dev, sizeof(struct sipc_device), GFP_KERNEL);
	if (!sipc) {
		sipc_destroy_pdata(&pdata, &pdev->dev);
		return -ENOMEM;
	}

	num = pdata->chd_nr;
	smsg = devm_kzalloc(&pdev->dev,
			    pdata->newchd_nr * sizeof(struct smsg_ipc),
			    GFP_KERNEL);
	if (!smsg) {
		sipc_destroy_pdata(&pdata, &pdev->dev);
		devm_kfree(&pdev->dev, sipc);
		return -ENOMEM;
	}
	pr_info("sipc: tag count = %d\n", num);
	info = pdata->info_table;
	for (i = 0; i < num; i++) {
		if (j < pdata->newchd_nr && info[i].is_new) {
			smsg[j].name = info[i].name;
			smsg[j].dst = info[i].dst;
#ifdef CONFIG_SPRD_MAILBOX
			smsg[j].core_id = info[i].core_id;
#else
			smsg[j].irq = info[i].irq;
#endif
			smsg[j].rxirq_status = sipc_rxirq_status;
			smsg[j].rxirq_clear = sipc_rxirq_clear;
			smsg[j].txirq_trigger = sipc_txirq_trigger;

#ifdef CONFIG_SPRD_MAILBOX
			pr_info("sipc:[tag%d] smsg name=%s, dst=%u, core_id=%d\n",
				j, smsg[j].name, smsg[j].dst, smsg[j].core_id);
#else
			pr_info("sipc:[tag%d] smsg name=%s, dst=%u, irq=%d\n",
				j, smsg[j].name, smsg[j].dst, smsg[j].irq);
#endif
			j++;
		}
	}

	sipc->pdata = pdata;
	sipc->smsg_inst = smsg;

	if (pdata->name != NULL) {
		len = strlen(pdata->name);
		strncpy((char *)&sid, pdata->name + 4, 1);
		sid = (sid & 0xff) - '0';
		if (len != 5 || strncmp(pdata->name, "sipc", 4) != 0 || sid >= SIPC_DEV_NR) {
			printk(KERN_ERR "sipc_probe: Failed to get valid sipc device name, len=%d, sid=%d\n", len, sid);
			sipc_destroy_pdata(&pdata, &pdev->dev);
			devm_kfree(&pdev->dev, sipc);
			devm_kfree(&pdev->dev, smsg);
			return -ENODEV;
		}
	} else {
		sid = 0;
	}
	sipc_ap.sipc_dev[sid] = sipc;

	pr_info("sipc: smem_init smem_base=0x%x, smem_size=0x%x\n",
		pdata->smem_base, pdata->smem_size);
#ifdef CONFIG_SPRD_SIPC_MULTI_SMEM
	if (strcmp(pdata->name, "sipc0") == 0) {
		smem_init_phead(pdata->smem_base);
	}
#else
	smem_init_phead(pdata->smem_base);
#endif
	smem_init(pdata->smem_base, pdata->smem_size);
	smsg_suspend_init();

	sipc_create(sipc);

	platform_set_drvdata(pdev, sipc);
	return 0;
}

static int sipc_remove(struct platform_device *pdev)
{
	struct sipc_device *sipc = platform_get_drvdata(pdev);

	sipc_destroy_pdata(&sipc->pdata, &pdev->dev);
	devm_kfree(&pdev->dev, sipc->smsg_inst);
	devm_kfree(&pdev->dev, sipc);
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

subsys_initcall_sync(sipc_init);
module_exit(sipc_exit);

MODULE_AUTHOR("Qiu Yi");
MODULE_DESCRIPTION("SIPC module driver");
MODULE_LICENSE("GPL");
