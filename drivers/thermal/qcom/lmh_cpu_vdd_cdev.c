// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/cpu_cooling.h>
#include <linux/idr.h>

#include <soc/qcom/qtee_shmbridge.h>
#include <asm/cacheflush.h>
#include <soc/qcom/scm.h>

#define LMH_CPU_VDD_MAX_LVL	1
#define LIMITS_CLUSTER_MIN_FREQ_OFFSET	0x3C0
#define LIMITS_DCVSH			0x10
#define LIMITS_NODE_DCVS		0x44435653
#define LIMITS_SUB_FN_THERMAL		0x54484D4C
#define LIMITS_ZEROC			0x4f534d5a
#define LIMITS_CLUSTER_0		0x6370302D

struct lmh_cpu_vdd_cdev {
	struct list_head node;
	int id;
	bool cpu_vdd_state;
	bool osm_vdd_cdev;
	void *min_freq_reg;
	struct thermal_cooling_device *cdev;
};

static DEFINE_IDA(lmh_cpu_vdd_ida);
static DEFINE_MUTEX(lmh_cpu_vdd_lock);
static LIST_HEAD(lmh_cpu_vdd_list);

static int limits_dcvs_write(uint32_t val)
{
	int ret;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;
	uint32_t payload_len;
	struct qtee_shm shm;

	payload_len = PAGE_ALIGN(5 * sizeof(uint32_t));
	if (!qtee_shmbridge_is_enabled()) {
		payload = kzalloc(payload_len, GFP_KERNEL);
		if (!payload)
			return -ENOMEM;
		desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	} else {
		ret = qtee_shmbridge_allocate_shm(
			payload_len, &shm);
		if (ret)
			return -ENOMEM;
		payload = shm.vaddr;
		desc_arg.args[0] = shm.paddr;
	}

	payload[0] = LIMITS_SUB_FN_THERMAL; /* algorithm */
	payload[1] = 0; /* unused sub-algorithm */
	payload[2] = LIMITS_ZEROC; //0x4f534d5a
	payload[3] = 1; /* number of values */
	payload[4] = val;

	desc_arg.args[1] = payload_len;
	desc_arg.args[2] = LIMITS_NODE_DCVS;
	desc_arg.args[3] = LIMITS_CLUSTER_0;
	desc_arg.args[4] = 0; /* version */
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL,
					SCM_VAL, SCM_VAL);

	dmac_flush_range(payload, (void *)payload + payload_len);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LIMITS_DCVSH), &desc_arg);
	if (ret < 0)
		pr_err("scm call failed. val:%d err:%d\n", val, ret);

	if (!qtee_shmbridge_is_enabled())
		kzfree(payload);
	else
		qtee_shmbridge_free_shm(&shm);

	return ret;
}

static int lmh_cpu_vdd_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	int ret;
	struct lmh_cpu_vdd_cdev *vdd_cdev = cdev->devdata;

	if (state > LMH_CPU_VDD_MAX_LVL)
		return -EINVAL;

	state = !!state;
	/* Check if the old cooling action is same as new cooling action */
	if (vdd_cdev->cpu_vdd_state == state)
		return 0;

	if (vdd_cdev->osm_vdd_cdev) {
		ret = limits_dcvs_write((u32)state);
		if (ret < 0)
			return ret;
	} else {
		writel_relaxed(state, vdd_cdev->min_freq_reg);
	}

	vdd_cdev->cpu_vdd_state = state;

	pr_debug("%s limits CPU VDD restriction for %s\n",
		state ? "Triggered" : "Cleared", vdd_cdev->cdev->type);

	return 0;
}

static int lmh_cpu_vdd_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct lmh_cpu_vdd_cdev *lmh_cpu_vdd_cdev = cdev->devdata;

	*state = (lmh_cpu_vdd_cdev->cpu_vdd_state) ?
			LMH_CPU_VDD_MAX_LVL : 0;

	return 0;
}

static int lmh_cpu_vdd_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = LMH_CPU_VDD_MAX_LVL;
	return 0;
}

static struct thermal_cooling_device_ops lmh_cpu_vdd_cooling_ops = {
	.get_max_state = lmh_cpu_vdd_get_max_state,
	.get_cur_state = lmh_cpu_vdd_get_cur_state,
	.set_cur_state = lmh_cpu_vdd_set_cur_state,
};


static int lmh_cpu_vdd_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct lmh_cpu_vdd_cdev *lmh_cpu_vdd_cdev;
	struct device_node *dn = pdev->dev.of_node;
	uint32_t min_reg;
	char cdev_name[THERMAL_NAME_LENGTH] = "";
	const __be32 *addr;

	lmh_cpu_vdd_cdev = devm_kzalloc(&pdev->dev, sizeof(*lmh_cpu_vdd_cdev),
					GFP_KERNEL);
	if (!lmh_cpu_vdd_cdev)
		return -ENOMEM;

	lmh_cpu_vdd_cdev->osm_vdd_cdev = of_property_read_bool(dn,
					"qcom,osm-llm-cpu-vdd-cdev");

	if (!lmh_cpu_vdd_cdev->osm_vdd_cdev) {
		addr = of_get_address(dn, 0, NULL, NULL);
		if (!addr) {
			dev_err(&pdev->dev,
				"Property llm-base-addr not found\n");
			return -EINVAL;
		}

		min_reg = be32_to_cpu(addr[0]) + LIMITS_CLUSTER_MIN_FREQ_OFFSET;

		lmh_cpu_vdd_cdev->min_freq_reg = devm_ioremap(&pdev->dev,
								min_reg, 0x4);
		if (!lmh_cpu_vdd_cdev->min_freq_reg) {
			dev_err(&pdev->dev,
				"lmh cpu vdd register remap failed\n");
			return -ENOMEM;
		}
	}

	mutex_lock(&lmh_cpu_vdd_lock);
	ret = ida_simple_get(&lmh_cpu_vdd_ida, 0, 0, GFP_KERNEL);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to create ida\n");
		goto unlock_exit;
	}
	lmh_cpu_vdd_cdev->id = ret;
	ret = 0;

	snprintf(cdev_name, THERMAL_NAME_LENGTH, "lmh-cpu-vdd%d",
			lmh_cpu_vdd_cdev->id);

	lmh_cpu_vdd_cdev->cdev = thermal_of_cooling_device_register(
					dn,
					cdev_name,
					lmh_cpu_vdd_cdev,
					&lmh_cpu_vdd_cooling_ops);
	if (IS_ERR(lmh_cpu_vdd_cdev->cdev)) {
		ret = PTR_ERR(lmh_cpu_vdd_cdev->cdev);
		dev_err(&pdev->dev, "Cooling register failed for %s, ret:%d\n",
			cdev_name, ret);
		lmh_cpu_vdd_cdev->cdev = NULL;
		goto remove_ida;
	}
	list_add(&lmh_cpu_vdd_cdev->node, &lmh_cpu_vdd_list);
	mutex_unlock(&lmh_cpu_vdd_lock);

	pr_debug("Cooling device [%s] registered.\n", cdev_name);

	return ret;

remove_ida:
	ida_simple_remove(&lmh_cpu_vdd_ida, lmh_cpu_vdd_cdev->id);

unlock_exit:
	mutex_unlock(&lmh_cpu_vdd_lock);

	return ret;
}

static int lmh_cpu_vdd_remove(struct platform_device *pdev)
{
	struct lmh_cpu_vdd_cdev *lmh_cpu_vdd, *c_next;

	mutex_lock(&lmh_cpu_vdd_lock);
	list_for_each_entry_safe(lmh_cpu_vdd, c_next,
			&lmh_cpu_vdd_list, node) {
		if (lmh_cpu_vdd->cdev) {
			thermal_cooling_device_unregister(
				lmh_cpu_vdd->cdev);
			lmh_cpu_vdd->cdev = NULL;
		}
		ida_simple_remove(&lmh_cpu_vdd_ida, lmh_cpu_vdd->id);
		list_del(&lmh_cpu_vdd->node);
	}
	mutex_unlock(&lmh_cpu_vdd_lock);

	return 0;
}
static const struct of_device_id lmh_cpu_vdd_match[] = {
	{ .compatible = "qcom,lmh-cpu-vdd", },
	{},
};

static struct platform_driver lmh_cpu_vdd_driver = {
	.probe		= lmh_cpu_vdd_probe,
	.remove         = lmh_cpu_vdd_remove,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = lmh_cpu_vdd_match,
	},
};
builtin_platform_driver(lmh_cpu_vdd_driver);
