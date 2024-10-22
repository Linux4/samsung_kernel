// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/interconnect.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include "adsp_platform_driver.h"
#include "adsp_qos.h"

#define SCENE_MASK(scene)           (1ULL << (scene))
#define SCENE_STA_SET(status, scene) ((status) |= SCENE_MASK(scene))
#define SCENE_STA_CLR(status, scene) ((status) &= ~(SCENE_MASK(scene)))
#define BW_MAX_MBPS                 (500)

static struct adsp_qos_control qos_ctrl;

struct adsp_qos_scene_info qos_scene_table[] = {
	[ADSP_QOS_SCENE_PHONECALL] = { .name = "adsp-qos-scene-phone-call"},
};

static bool is_scene_enable(enum adsp_qos_scene scene)
{
	if (scene >= ADSP_QOS_SCENE_NUM) {
		pr_info("%s scene %d is not a valid scene\n", __func__, scene);
		return false;
	}
	return !!(qos_ctrl.adsp_qos_scene_enable & SCENE_MASK(scene));
}

static bool is_scene_requested_bw(enum adsp_qos_scene scene)
{
	/* this function should be used in a critical section */
	if (scene >= ADSP_QOS_SCENE_NUM) {
		pr_info("%s scene %d is not a valid scene\n", __func__, scene);
		return false;
	}
	return !!(qos_ctrl.adsp_qos_scene_status & SCENE_MASK(scene));
}

int adsp_qos_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg_hrt");
	if (unlikely(!res)) {
		pr_info("%s get resource IFRBUS_AO fail.\n", __func__);
		return 0;
	}

	qos_ctrl.cfg_hrt = devm_ioremap(dev, res->start,
					resource_size(res));
	if (unlikely(!qos_ctrl.cfg_hrt)) {
		pr_warn("%s get ioremap IFRBUS_AO fail: 0x%llx\n",
			__func__, res->start);
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "hrt-ctrl-bits",
				   &qos_ctrl.hrt_bits);
	if (ret) {
		pr_warn("%s get hrt_bits fail:%s.\n", __func__, "hrt-ctrl-bits");
		return -ENODEV;
	}
	/* emi hrt setting */
	writel(qos_ctrl.hrt_bits, qos_ctrl.cfg_hrt);

	qos_ctrl.icc_hrt_path = of_icc_get(dev, "icc-hrt-bw");
	if (IS_ERR_OR_NULL(qos_ctrl.icc_hrt_path)) {
		pr_warn("%s get icc_hrt_path fail:%s.\n", __func__, "icc-hrt-bw");
		return -ENODEV;
	}

	qos_ctrl.adsp_qos_scene_enable = 0;
	qos_ctrl.adsp_qos_scene_status = 0;
	qos_ctrl.cur_bw_mbps = 0;

	mutex_init(&qos_ctrl.lock);

	return 0;
}
EXPORT_SYMBOL(adsp_qos_probe);

void adsp_set_scene_bw(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	enum adsp_qos_scene sc;
	char *sc_name;
	u32 scene_info[2];

	for (sc = 0; sc < ADSP_QOS_SCENE_NUM; sc++) {
		sc_name = qos_scene_table[sc].name;
		ret = of_property_read_u32_array(dev->of_node, sc_name, scene_info, 2);
		if (ret) {
			pr_info("%s cannot find bw for %s\n", __func__, sc_name);
			continue;
		}

		if (scene_info[0]) {
			SCENE_STA_SET(qos_ctrl.adsp_qos_scene_enable, sc);
			qos_scene_table[sc].bw_mbps = scene_info[1];
		} else
			SCENE_STA_CLR(qos_ctrl.adsp_qos_scene_enable, sc);
	}

	pr_info("%s scene enable %llx\n", __func__, qos_ctrl.adsp_qos_scene_enable);
	for (sc = 0; sc < ADSP_QOS_SCENE_NUM; sc++) {
		sc_name = qos_scene_table[sc].name;
		pr_info("%s scene:%s bw:%d\n", __func__, sc_name, qos_scene_table[sc].bw_mbps);
	}
}
EXPORT_SYMBOL(adsp_set_scene_bw);

void set_adsp_icc_bw(uint32_t bw_mbps)
{
	//pr_info("%s %d Mbps\n", __func__, bw_mbps);
	icc_set_bw(qos_ctrl.icc_hrt_path, MBps_to_icc(bw_mbps), 0);
}
EXPORT_SYMBOL(set_adsp_icc_bw);

void clear_adsp_icc_bw(void)
{
	icc_set_bw(qos_ctrl.icc_hrt_path, 0, 0);
}
EXPORT_SYMBOL(clear_adsp_icc_bw);

int adsp_icc_bw_req(uint16_t scene, uint16_t set)
{
	int ret = 0;
	uint16_t bw_mbps = 0;

	if (scene >= ADSP_QOS_SCENE_NUM) {
		pr_info("%s %d is not a valid scene\n", __func__, scene);
		return -EINVAL;
	}

	if (!is_scene_enable(scene)) {
		pr_info("%s scene %d is not enabled, scene_enable: %llx\n",
			__func__, scene, qos_ctrl.adsp_qos_scene_enable);
		return -EINVAL;
	}

	bw_mbps = qos_scene_table[scene].bw_mbps;
	if (bw_mbps > BW_MAX_MBPS) {
		pr_info("%s bw_mbps %d over %d\n", __func__, bw_mbps, BW_MAX_MBPS);
		return -EINVAL;
	}

	pr_debug("%s receive qos req scene:%d set:%d bw:%d\n",
		 __func__, scene, set, bw_mbps);

	mutex_lock(&qos_ctrl.lock);
	if (set) {
		if (is_scene_requested_bw(scene)) {
			pr_info("%s scene %d has requested bw, scene_status: 0x%llx\n",
				__func__, scene, qos_ctrl.adsp_qos_scene_status);
			ret = -EFAULT;
			goto EXIT;
		}
		SCENE_STA_SET(qos_ctrl.adsp_qos_scene_status, scene);
		qos_ctrl.cur_bw_mbps += bw_mbps;
	} else {
		if (!is_scene_requested_bw(scene)) {
			pr_info("%s scene %d has not requested bw, scene_status: 0x%llx\n",
				__func__, scene, qos_ctrl.adsp_qos_scene_status);
			ret = -EFAULT;
			goto EXIT;
		}
		SCENE_STA_CLR(qos_ctrl.adsp_qos_scene_status, scene);
		qos_ctrl.cur_bw_mbps -= bw_mbps;
	}

	if (qos_ctrl.cur_bw_mbps > BW_MAX_MBPS) {
		pr_info("%s cur_bw_mbps (%d) > MAX_MBPS (%d), set adsp icc bw as MAX_MBPS\n",
			__func__, qos_ctrl.cur_bw_mbps, BW_MAX_MBPS);
		set_adsp_icc_bw(BW_MAX_MBPS);
	} else
		set_adsp_icc_bw(qos_ctrl.cur_bw_mbps);
EXIT:
	mutex_unlock(&qos_ctrl.lock);

	return ret;
}
EXPORT_SYMBOL(adsp_icc_bw_req);
