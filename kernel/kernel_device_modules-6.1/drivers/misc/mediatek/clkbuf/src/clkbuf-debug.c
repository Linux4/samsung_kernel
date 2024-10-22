// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "clkbuf-debug.h"
#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"
#include "srclken.h"
#include "clkbuf-pmif.h"
#include "clkbuf-pmic.h"

static struct clkbuf_dts *_array;
static void set_dts_array(struct clkbuf_dts *array)
{
	_array = array;
}
static struct clkbuf_dts *get_dts_array(void)
{
	return _array;
}

static ssize_t dump_pmic_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	struct clkbuf_hdlr *hdlr;
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	int nums, i, len = 0;

	if (!array)
		return sprintf(buf, "array is null\n");

	nums = array->nums;

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;

		hw = array->hw;

		if (IS_PMIC_HW(hw.hw_type)) {
			if (!hdlr->ops->dump_pmic_debug_regs)
				return sprintf(
					buf,
					"Error: dump_pmic_debug_regs is null\n");

			pd = (struct plat_xodata *)hdlr->data;
			len += hdlr->ops->dump_pmic_debug_regs(pd, buf, len);
			break;
		}
	}
	if (!IS_PMIC_HW(hw.hw_type))
		return sprintf(buf, "failed\n");

	return len;
}

static ssize_t dump_srclken_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	struct plat_rcdata *pd;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int len = 0, nums = 0, i;

	if (!array)
		return sprintf(buf, "array is null\n");

	nums = array->nums;

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;

		hw = array->hw;
		/*only need one RC obj, no need loop all obj*/
		if (IS_RC_HW(hw.hw_type)) {
			if (!hdlr->ops->dump_srclken_status)
				return sprintf(
					buf,
					"Error: dump_srclken_status is null\n");

			pd = (struct plat_rcdata *)hdlr->data;
			len = hdlr->ops->dump_srclken_status(pd, buf);
			break;
		}
	}
	if (!IS_RC_HW(hw.hw_type))
		return sprintf(buf, "array is invalid for support srclken\n");

	return len;
}

static ssize_t dump_srclken_trace_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	struct plat_rcdata *pd;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int len = 0, nums = 0, dump_max = 1, i;

	if (!array)
		return sprintf(buf, "array is null\n");

	nums = array->nums;

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;

		hw = array->hw;
		/*only need one RC obj, no need loop all obj*/
		if (IS_RC_HW(hw.hw_type)) {
			if (!hdlr->ops->dump_srclken_trace)
				return sprintf(
					buf,
					"Error: dump_srclken_trace is null\n");

			pd = (struct plat_rcdata *)hdlr->data;
			len = hdlr->ops->dump_srclken_trace(
				pd, buf, dump_max);
			break;
		}
	}
	if (!IS_RC_HW(hw.hw_type))
		return sprintf(buf, "array is invalid for support srclken\n");

	return len;
}

static ssize_t subsys_req_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	struct plat_rcdata *pd;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int i;
	int len = 0, nums = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "/***RC CMD usage:***/\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "RC_NONE_REQ = 0x0001\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "RC_FPM_REQ = 0x0002\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "RC_LPM_REQ = 0x0004\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "RC_BBLPM_REQ = 0x0008\n");
	len += snprintf(buf + len, PAGE_SIZE - len,
			"echo \"[CMD(hex)] [sub_id(dec)]\" > subsys_req\n");
	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"ex. echo \"0x0002 6\" >  /sys/kernel/clkbuf/subsys_req\n");
	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"===================================================\n");

	if (!array)
		return sprintf(buf, "array is null\n");

	nums = array->nums;

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;

		hw = array->hw;
		if (!IS_RC_HW(hw.hw_type))
			continue;

		if (!hdlr->ops->get_rc_MXX_req_sta)
			return sprintf(buf,
				       "Error: get_rc_MXX_req_sta is null\n");

		pd = (struct plat_rcdata *)hdlr->data;
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"support subsys:<%s>, sub_id:<%2d>, mapping xo:<%s>\n",
			array->subsys_name, array->sub_id, array->xo_name);

		len += hdlr->ops->get_rc_MXX_req_sta(pd, array->sub_id,
						     buf + len);

		len += hdlr->ops->get_rc_MXX_cfg(pd, array->sub_id,
						     buf + len);
	}

	return len;
}

static ssize_t subsys_req_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t count)
{
	struct clkbuf_dts *array = get_dts_array();
	struct plat_rcdata *pd;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int cmd = 0x0;
	int sub_id = 0, ret = 0, nums = 0, i;

	if (!array)
		return -ENODEV;

	nums = array->nums;

	if (sscanf(buf, "%x %d", &cmd, &sub_id) == 2) {
		CLKBUF_DBG("subsys_req, input cmd: %x, sub_id: %d\n", cmd, sub_id);

		for (i = 0; i < nums; i++, array++) {
			hdlr = array->hdlr;
			hw = array->hw;

			if (!IS_RC_HW(hw.hw_type))
				continue;

			if (array->sub_id == sub_id) {
				if (!hdlr->ops->srclken_subsys_ctrl)
					return -EINVAL;

				pd = (struct plat_rcdata *)hdlr->data; //need IS_RC_HW b4
				ret = hdlr->ops->srclken_subsys_ctrl(
					pd, cmd, array->sub_id, array->perms);
				if (ret) {
					CLKBUF_DBG("Error: %d\n", ret);
					break;
				}

				return count;
			}
		}
	}

	CLKBUF_DBG("can not find sub_id %d\n", sub_id);
	return count;
}

static ssize_t xo_status_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	struct clkbuf_hdlr *
		hdlr; //should get right hdlr , if pasing srclken would get wrong
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	u32 out = 0;
	int ret = 0, len = 0, nums = 0, xo_id = 0, i;
	/*count last xo element for printing pmrcen*/
	int count = 0;

	if (!array)
		return sprintf(buf, "array is null\n");

	nums = array->nums;

	len += snprintf(buf + len, PAGE_SIZE - len, "/***XO CMD usage:***/\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "SET_XO_MODE = 0x0001\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "SET_XO_EN_M = 0x0002\n");
	len += snprintf(buf + len, PAGE_SIZE - len,
			"SET_XO_IMPEDANCE = 0x0004\n");
	len += snprintf(buf + len, PAGE_SIZE - len,
			"SET_XO_DESENSE = 0x0008\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "SET_XO_VOTER = 0x0010\n");
	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"echo \"[CMD(hex)] [xo_id(dec)] [val(hex)]\" > xo_status\n");
	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"ex. echo \"0x0002 3 1\" > /sys/kernel/clkbuf/xo_status\n");
	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"===================================================\n");

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;

		hw = array->hw;
		if (hw.hw_type != PMIC)
			continue;

		if ((!hdlr->ops->get_xo_cmd_hdlr) || (!hdlr->ops->get_pmrcen))
			return sprintf(buf, "Error: call back is null\n");

		pd = (struct plat_xodata *)hdlr->data; //could be NULL if rc node
		xo_id = array->xo_id;

		len += snprintf(buf + len, PAGE_SIZE - len,
				"xo_id: <%2d> xo_name: <%10s> ", xo_id,
				array->xo_name);
		len = hdlr->ops->get_xo_cmd_hdlr(pd, xo_id, buf, len);

		/****PMRC_EN****/
		count++;
		if (count == array->num_xo) {
			ret = hdlr->ops->get_pmrcen(pd, &out);
			if (ret)
				return len;
			len += snprintf(
				buf + len, PAGE_SIZE - len,
				"---------------------------------------------------\n");
			len += snprintf(buf + len, PAGE_SIZE - len,
					"pmrcen: <0x%08x>\n", out);
		}
	}
	return len;
}

static ssize_t xo_status_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	struct clkbuf_dts *array = get_dts_array();
	struct clkbuf_hdlr *hdlr;
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	unsigned int cmd;
	int ret = 0, nums = 0, xo_id = 0, i;
	u32 input = 0;

	if (!array)
		return -ENODEV;

	nums = array->nums;

	if (sscanf(buf, "%x %d %x", &cmd, &xo_id, &input) == 3) {
		CLKBUF_DBG("user input cmd: %x , xo_id: %d, val(hex): %x\n",
			   cmd, xo_id, input);
		/*loop all object---step1*/
		for (i = 0; i < nums; i++, array++) {
			hw = array->hw;
			/*filter PMIC element---step2*/
			if ((IS_PMIC_HW(hw.hw_type)) &&
			    (array->xo_id == xo_id)) {

				hdlr = array->hdlr;
				if (!hdlr->ops->set_xo_cmd_hdlr)
					return -EINVAL;

				pd = (struct plat_xodata *)hdlr
					     ->data; //need (hw.hw_type == PMIC) b4

				ret = hdlr->ops->set_xo_cmd_hdlr(
					pd, cmd, xo_id, input, array->perms);
				if (ret)
					CLKBUF_DBG("Error: %d\n", ret);
				break;
			}
		}
		return count;
	}
	return -EINVAL;
}

static ssize_t pmif_status_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	struct plat_pmifdata *pd;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int len = 0, nums = 0, i;

	if (!array)
		return sprintf(buf, "array is null\n");

	nums = array->nums;

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;

		hw = array->hw;
		/*only need one PMIF obj, no need loop all obj*/
		if (IS_PMIF_HW(hw.hw_type)) {
			if (!hdlr->ops->dump_pmif_status)
				return sprintf(
					buf,
					"Error: dump_pmif_status is null\n");

			pd = (struct plat_pmifdata *)hdlr->data;
			len += hdlr->ops->dump_pmif_status(pd, buf + len);
			break;
		}
	}
	if (!IS_PMIF_HW(hw.hw_type))
		return sprintf(buf, "array is invalid for support pmif\n");

	return len;
}

static ssize_t pmif_status_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t count)
{
	struct clkbuf_dts *array = get_dts_array();
	struct clkbuf_hdlr *hdlr;
	struct plat_pmifdata *pd;
	struct clkbuf_hw hw;
	unsigned int cmd;
	int ret = 0, nums = 0, i;
	u32 input = 0, pmif_id = 0;

	if (!array)
		return -ENODEV;

	nums = array->nums;

	if (sscanf(buf, "%x %x %x", &cmd, &input, &pmif_id) == 3) {
		CLKBUF_DBG("user input cmd: %x, val(hex): %x, pmif_id(hex): %x\n",
			cmd, input, pmif_id);
		/*loop all object---step1*/
		for (i = 0; i < nums; i++, array++) {
			hw = array->hw;
			/*filter PMIF object---step2*/
			if ((IS_PMIF_HW(hw.hw_type)) && (array->pmif_id == pmif_id)) {
				hdlr = array->hdlr;
				if (!hdlr->ops->set_pmif_inf)
					return -EINVAL;

				pd = (struct plat_pmifdata *)hdlr
					     ->data; //need (hw.hw_type == PMIF) b4
				ret = hdlr->ops->set_pmif_inf(
					pd, cmd, pmif_id, input);
				if (ret)
					CLKBUF_DBG("Error: %d\n", ret);
				break;
			}
		}
		return count;
	}
	return -EINVAL;
}

static ssize_t dump_clkbuf_dts_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	struct clkbuf_dts *array = get_dts_array();
	int nums = 0, len = 0, i;

	nums = array->nums;

	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"%16s, %16s, %13s, %11s, %2s, ",
		"array", "hdlr", "xo_name", "rc_sub_name", "HW");

	len += snprintf(
		buf + len, PAGE_SIZE - len,
		"%5s, %6s, %7s, %8s\n",
		"xo_id", "sub_id", "pmif_id", "perms");

	for (i = 0; i < nums; i++, array++) {
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"%16lx, %16lx, %13s, %11s, %2d, ",
			(unsigned long)array, (unsigned long)array->hdlr,
			array->xo_name, array->subsys_name, array->hw.hw_type);
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"%5d, %6d, %7d, %8x\n",
			array->xo_id, array->sub_id, array->pmif_id, array->perms);

	}
	return len;
}

DEFINE_ATTR_RW(xo_status);
DEFINE_ATTR_RW(pmif_status);
DEFINE_ATTR_RW(subsys_req);
DEFINE_ATTR_RO(dump_pmic);
DEFINE_ATTR_RO(dump_srclken);
DEFINE_ATTR_RO(dump_srclken_trace);
DEFINE_ATTR_RO(dump_clkbuf_dts);

static struct attribute *clkbuf_attrs[] = {
	__ATTR_OF(xo_status),
	__ATTR_OF(pmif_status),
	__ATTR_OF(subsys_req),
	__ATTR_OF(dump_pmic),
	__ATTR_OF(dump_srclken),
	__ATTR_OF(dump_srclken_trace),
	__ATTR_OF(dump_clkbuf_dts),
	NULL,
};

static struct attribute_group clkbuf_attr_group = {
	.name = "clkbuf",
	.attrs = clkbuf_attrs,
};

int clkbuf_debug_init(struct clkbuf_dts *array, struct device *dev)
{
	int ret = 0;

	CLKBUF_DBG("\n");
	set_dts_array(array);

	/* create /sys/kernel/clkbuf/xxx */
	ret = sysfs_create_group(kernel_kobj, &clkbuf_attr_group);
	if (ret) {
		CLKBUF_DBG("FAILED TO CREATE /sys/kernel/clkbuf (%d)\n", ret);
		return ret;
	}

	CLKBUF_DBG("\n");
	return 0;
}
