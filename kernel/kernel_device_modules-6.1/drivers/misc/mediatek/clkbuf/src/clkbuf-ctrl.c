// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"
#include "clkbuf-pmic.h"
#include "srclken.h"

static bool _inited;

static struct clkbuf_dts *_array;
static void set_dts_array(struct clkbuf_dts *array)
{
	_array = array;
}
static struct clkbuf_dts *get_dts_array(void)
{
	return _array;
}

static struct clkbuf_dts *(*parse_sub_dts[])(struct clkbuf_dts *array,
					     struct device_node *clkbuf_node,
					     int nums) = {
	&parse_pmic_dts,
	&parse_srclken_dts,
	&parse_pmif_dts,
	NULL,
};

static int (*sub_init[])(struct clkbuf_dts *array, struct device *dev) = {
	&clkbuf_pmic_init,
	&clkbuf_srclken_init,
	&clkbuf_pmif_init,
	&clkbuf_platform_init,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	&clkbuf_debug_init,
#endif
	NULL,
};

static const struct of_device_id clkbuf_of_match[] = {
	{ .compatible = "mediatek,mt6878-clkbuf" },
	{ .compatible = "mediatek,mt6897-clkbuf" },
	{ .compatible = "mediatek,mt6985-clkbuf" },
	{ .compatible = "mediatek,mt6989-clkbuf" },
	{ .compatible = "mediatek,mt8792-clkbuf" },
	{ .compatible = "mediatek,mt8796-clkbuf" },
	{}
};

static const char *const xo_api_cmd[] = {
	[SET_XO_MODE] = "SET_XO_MODE",
	[SET_XO_EN_M] = "SET_XO_EN_M",
	[SET_XO_IMPEDANCE] = "SET_XO_IMPEDANCE",
	[SET_XO_DESENSE] = "SET_XO_DESENSE",
	[SET_XO_VOTER] = "SET_XO_VOTER",
};

static const char *const rc_api_cmd[] = {
	[RC_NONE_REQ] = "RC_NONE_REQ",
	[RC_FPM_REQ] = "RC_FPM_REQ",
	[RC_LPM_REQ] = "RC_LPM_REQ",
};

/* API usage example */
/* clkbuf_srclken_ctrl("RC_FPM_REQ", 6); */
/* clkbuf_srclken_ctrl("RC_FPM_REQ", 13); */
int clkbuf_srclken_ctrl(char *cmd, int sub_id)
{
	struct clkbuf_dts *array = get_dts_array();
	struct plat_rcdata *pd;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int ret = 0, nums = 0, i;
	unsigned int cmd_idx;
	int dts_api_perms = 0;

	if (!_inited || !array)
		return -ENODEV;

	nums = array->nums;

	for (cmd_idx = 1; cmd_idx < RC_MAX_REQ; cmd_idx <<=1) {
		if (!strcmp(rc_api_cmd[cmd_idx], cmd)) {
			/*loop all object---step1*/
			for (i = 0; i < nums; i++, array++) {
				hw = array->hw;
				hdlr = array->hdlr;
				/*filter RC object---step2*/
				if (IS_RC_HW(hw.hw_type) && (array->sub_id == sub_id)) {
					if (!hdlr->ops->srclken_subsys_ctrl) {
						CLKBUF_DBG("Error: srclken_subsys_ctrl is null\n");
						return ret;
					}
					dts_api_perms =	(array->perms & 0xffff0000) >> 16;
					pd = (struct plat_rcdata *)hdlr->data;
					ret = hdlr->ops->srclken_subsys_ctrl(
						pd, cmd_idx, array->sub_id,
						dts_api_perms);
					if (ret) {
						CLKBUF_DBG("Error: %d\n", ret);
						break;
					}
					return ret;
				}
			}
			CLKBUF_DBG("can not find RC HW and sub_id %d\n", sub_id);
			return -EINVAL;
		}
	}
	CLKBUF_DBG("can not find cmd %s\n", cmd);
	return -EINVAL;
}
EXPORT_SYMBOL(clkbuf_srclken_ctrl);

/* API usage example */
/* clkbuf_xo_ctrl("SET_XO_MODE", 12, 0); */
/* clkbuf_xo_ctrl("SET_XO_EN_M", 11, 1); */
int clkbuf_xo_ctrl(char *cmd, int xo_id, u32 input)
{
	struct clkbuf_dts *array = get_dts_array();
	struct clkbuf_hdlr *hdlr;
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	int dts_api_perms;
	int ret = 0, nums = 0, i;
	unsigned int cmd_idx;

	if (!_inited || !array)
		return -ENODEV;

	nums = array->nums;

	for (cmd_idx = 1; cmd_idx < MAX_XO_CMD; cmd_idx <<=1) {
		if (!strcmp(xo_api_cmd[cmd_idx], cmd)) {
			/*loop all object---step1*/
			for (i = 0; i < nums; i++, array++) {
				hw = array->hw;
				hdlr = array->hdlr;
				/*filter PMIC object---step2*/
				if ((IS_PMIC_HW(hw.hw_type)) && (array->xo_id == xo_id)) {
					if (!hdlr->ops->set_xo_cmd_hdlr) {
						CLKBUF_DBG("Error: set_xo_cmd_hdlr is null\n");
						return ret;
					}
					pd = (struct plat_xodata *)hdlr->data;
					dts_api_perms =	(array->perms & 0xffff0000) >> 16;
					ret = hdlr->ops->set_xo_cmd_hdlr(
						pd, cmd_idx, xo_id, input,
						dts_api_perms);
					if (ret) {
						CLKBUF_DBG("Error: %d\n", ret);
						break;
					}
					return ret;
				}
			}
			CLKBUF_DBG("can not find PMIC HW and sub_id %d\n", xo_id);
			return -EINVAL;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(clkbuf_xo_ctrl);

static struct clkbuf_dts *parse_dt(struct platform_device *pdev)
{
	struct device_node *clkbuf_node;
	struct clkbuf_dts *array = NULL, *head;
	const struct of_device_id *match;
	int size = 0, nums = 0;
	struct clkbuf_dts *(**parse_dts_call)(
		struct clkbuf_dts *, struct device_node *, int) = parse_sub_dts;

	clkbuf_node = pdev->dev.of_node;
	match = of_match_node(pdev->dev.driver->of_match_table, clkbuf_node);

	nums += count_pmic_node(clkbuf_node);

	nums += count_src_node(clkbuf_node);

	nums += count_pmif_node(clkbuf_node);

	size = sizeof(*array) * (nums + 1);
	array = kzalloc(size, GFP_KERNEL);

	if (!array) {
		CLKBUF_DBG("-ENOMEM\n");
		return NULL;
	}

	head = array;

	while (*parse_dts_call != NULL) {
		array = (*parse_dts_call)(array, clkbuf_node, nums);
		parse_dts_call++;
	}

	set_dts_array(head);

	return head;
}

static int clkbuf_probe(struct platform_device *pdev)
{
	struct clkbuf_dts *array;
	struct device *dev = &pdev->dev;
	int nums = 0, i;

	int (**init_call)(struct clkbuf_dts *, struct device *) = sub_init;

	CLKBUF_DBG("\n");

	array = parse_dt(pdev);
	if (!array)
		return -ENOMEM;

	while (*init_call != NULL) {
		(*init_call)(array, dev);
		init_call++;
	}

	nums = array->nums;
	//make sure array is complete
	for (i = 0; i < nums; i++, array++) {
		struct clkbuf_hdlr *hdlr = array->hdlr;

		if (!hdlr) {
			CLKBUF_DBG("array<%lx>, index: %d, type:%d, nums:%d, perms:<%x>\n",
				(unsigned long)array, i, array->hw.hw_type, array->nums, array->perms);

			return -1;
		}
	}
	/* set _inited is the last step */
	mb();
	_inited = true;

	return 0;
}

#define DUMP_DONE 1
#define WAIT_TO_DUMP 0
static int dump_all(struct device *dev)
{
	struct clkbuf_dts *array = get_dts_array();
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	struct plat_xodata *xopd;
	struct plat_rcdata *rcpd;
	int pmic_dump = WAIT_TO_DUMP, rc_dump = WAIT_TO_DUMP, pmif_dump = WAIT_TO_DUMP;
	int nums, i, ret = 0;
	char *buf = NULL;
	u32 out = 0;

	buf = vmalloc(CLKBUF_STATUS_INFO_SIZE);
	if (!buf)
		return -ENOMEM;

	if (!array) {
		vfree(buf);
		return -ENODEV;
	}

	nums = array->nums;

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;
		hw = array->hw;

		switch (hw.hw_type) {
		case PMIC:
			if ((pmic_dump == DUMP_DONE) | !(hdlr->ops->get_pmrcen))
				break;
			xopd = (struct plat_xodata *)hdlr->data;
			ret = hdlr->ops->get_pmrcen(xopd, &out);
			if (ret)
				break;
			pmic_dump = DUMP_DONE;
			break;
		case SRCLKEN_STA:
		case SRCLKEN_CFG:
			if ((rc_dump == DUMP_DONE) | !(hdlr->ops->dump_srclken_trace))
				break;
			rcpd = (struct plat_rcdata *)hdlr->data;
			hdlr->ops->dump_srclken_trace(rcpd, buf, 0);
			CLKBUF_DBG("%s\n", buf);
			rc_dump = DUMP_DONE;
			break;
		case PMIF_M:
		case PMIF_P:
			if ((pmif_dump == DUMP_DONE) | !(hdlr->ops->spmi_dump_pmif_record))
				break;
			hdlr->ops->spmi_dump_pmif_record();
			pmif_dump = DUMP_DONE;
			break;
		default:
			break;
		}
	}

	vfree(buf);

	return 0;
}

static int clk_buf_dev_pm_suspend(struct device *dev)
{
	return dump_all(dev);
}

static int clk_buf_dev_pm_resume(struct device *dev)
{
	return dump_all(dev);
}

const struct dev_pm_ops clk_buf_suspend_ops = {
	.suspend_noirq = clk_buf_dev_pm_suspend,
	.resume_noirq = clk_buf_dev_pm_resume,
};

static struct platform_driver clkbuf_driver = {
	.probe = clkbuf_probe,
	.driver = {
			.name = "clkbuf",
			.owner = THIS_MODULE,
			.of_match_table = clkbuf_of_match,
			.pm = &clk_buf_suspend_ops,
	},
};
module_platform_driver(clkbuf_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek CLKBUF CTRL Driver");
MODULE_AUTHOR("Kuan-Hsin Lee <kuan-hsin.lee@mediatek.com>");
MODULE_SOFTDEP("pre: mt6685-core");
