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
#include <linux/slab.h>
#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"
#include "clkbuf-pmic.h"

struct match_pmic {
	char *name;
	struct clkbuf_hdlr *hdlr;
	int (*init)(struct clkbuf_dts *array, struct match_pmic *match);
};

static int read_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val,
			 u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_IS_NULL;

	if (!reg->mask)
		return -EREG_NO_MASK;

	*val = 0;

	if (IS_PMIC_HW(hw->hw_type)) {
		ret = regmap_read(hw->base.map, reg->ofs + ofs, val);
		(*val) = ((*val) & (reg->mask << reg->shift)) >> reg->shift;
	} else
		ret = -EREG_READ_FAIL;

	return ret;
}

static int write_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 val,
			  u32 ofs)
{
	int ret = 0;
	u32 mask;

	if (!reg)
		return -EREG_IS_NULL;

	if (!reg->mask)
		return -EREG_NO_MASK;

	if (IS_PMIC_HW(hw->hw_type)) {
		mask = reg->mask << reg->shift;
		val <<= reg->shift;
		ret = regmap_update_bits(hw->base.map, reg->ofs + ofs, mask,
					 val);
	} else
		ret = -EREG_WRITE_FAIL;

	return ret;
}

static int pmic_read(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val)
{
	return read_with_ofs(hw, reg, val, 0);
}

static int pmic_write(struct clkbuf_hw *hw, struct reg_t *reg, u32 val)
{
	return write_with_ofs(hw, reg, val, 0);
}

static int pmic_init_v1(struct clkbuf_dts *array, struct match_pmic *match)
{
	struct clkbuf_hdlr *hdlr = match->hdlr;
	struct plat_xodata *pd;
	struct xo_dts_cmd *xo_dts_cmd = array->init_dts_cmd.xo_dts_cmd;
	int ret = 0, i;
	int xo_id = array->xo_id;
	static DEFINE_SPINLOCK(lock);

	CLKBUF_DBG("array<%lx>,%s %d, id<%d>\n", (unsigned long)array, array->xo_name,
		   array->hw.hw_type, array->xo_id);

	pd = (struct plat_xodata *)(hdlr->data);
	pd->hw = array->hw;
	pd->lock = &lock;

	/* hook hdlr to array */
	array->hdlr = hdlr;

	/*if defined dts cmd, then do dts init flow*/
	for (i = 0; i < array->init_dts_cmd.num_xo_cmd; i++, xo_dts_cmd++) {
		if (xo_dts_cmd->xo_cmd) {
			if (!hdlr->ops->set_xo_cmd_hdlr) {
				CLKBUF_DBG("Error: set_xo_cmd_hdlr is null\n");
				break;
			}
			ret = hdlr->ops->set_xo_cmd_hdlr(pd, xo_dts_cmd->xo_cmd,
							 xo_id,
							 xo_dts_cmd->cmd_val,
							 array->perms);
			if (ret)
				CLKBUF_DBG("Error: %d\n", ret);
		}
	}

	return 0;
}

int set_xo_desense(void *data, int xo_id, u32 des)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0;
	spinlock_t *lock = pd->lock;

	spin_lock_irqsave(lock, flags);

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._de_sense;
	ret = pmic_write(&hw, &reg, des);

	spin_unlock_irqrestore(lock, flags);

	if (ret)
		CLKBUF_DBG("set xo desense failed, Error: %d\n", ret);

	return ret;
}

int get_xo_desense(void *data, int xo_id, u32 *out)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	int ret = 0;

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._de_sense;
	ret = pmic_read(&hw, &reg, out);

	if (ret)
		CLKBUF_DBG("clkbuf read desense failed, Error: %d\n", ret);

	return ret;
}

int set_xo_impedance(void *data, int xo_id, u32 imp)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0;
	spinlock_t *lock = pd->lock;

	spin_lock_irqsave(lock, flags);

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._impedance;
	ret = pmic_write(&hw, &reg, imp);

	spin_unlock_irqrestore(lock, flags);

	if (ret)
		CLKBUF_DBG("set xo en failed, Error: %d\n", ret);

	return ret;
}

int get_xo_impedance(void *data, int xo_id, u32 *out)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	int ret = 0;

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._impedance;
	ret = pmic_read(&hw, &reg, out);

	if (ret)
		CLKBUF_DBG("clkbuf read init xo en_m failed, Error: %d\n", ret);

	return ret;
}

int get_xo_mode(void *data, int xo_id, u32 *out)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	int ret = 0;

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._xo_mode;
	ret = pmic_read(&hw, &reg, out);

	if (ret)
		CLKBUF_DBG("clkbuf read init xo en_m failed, Error: %d\n", ret);

	return ret;
}

int get_xo_en_m(void *data, int xo_id, u32 *out)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	int ret = 0;

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._xo_en;
	ret = pmic_read(&hw, &reg, out);

	if (ret)
		CLKBUF_DBG("clkbuf read init xo en_m failed\n");

	return ret;
}

int set_xo_mode(void *data, int xo_id, u32 mode)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct common_regs *com_regs;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0, mode_num;
	u32 out = 0;

	spinlock_t *lock = pd->lock;

	spin_lock_irqsave(lock, flags);

	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	mode_num = com_regs->mode_num;

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._xo_mode;
	ret = pmic_write(&hw, &reg, (mode <= mode_num) ? mode : 0);

	spin_unlock_irqrestore(lock, flags);
	/*SW MODE need to wait 400ms for output clk*/
	if (mode == 0) {
		get_xo_en_m(data, xo_id, &out);
		if (out == 1)
			udelay(400);
	}

	if (ret)
		CLKBUF_DBG("set xo en failed\n");

	return ret;
}

int set_xo_en_m(void *data, int xo_id, int onoff)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct xo_buf_t xo_buf;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0;
	u32 out = 0;

	spinlock_t *lock = pd->lock;

	spin_lock_irqsave(lock, flags);

	xo_buf = (pd->xo_buf_t)[xo_id];
	reg = xo_buf._xo_en;
	ret = pmic_write(&hw, &reg, (onoff == 1) ? 1 : 0);

	spin_unlock_irqrestore(lock, flags);
	/*SW MODE need to wait 400ms for output clk*/
	if (onoff == 1) {
		get_xo_mode(data, xo_id, &out);
		if (out == 0)
			udelay(400);
	}

	if (ret)
		CLKBUF_DBG("set xo en failed\n");

	return ret;
}


int get_xo_auxout(void *data, int xo_id, u32 *out, char *reg_name)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct xo_buf_t xo_buf;
	struct common_regs *com_regs;
	struct clkbuf_hw hw;
	struct reg_t reg_in, reg_out;
	unsigned long flags = 0;
	int auxout_sel, ret = 0;
	spinlock_t *lock = NULL;

	if (!pd)
		return 0;

	lock = pd->lock;

	xo_buf = (pd->xo_buf_t)[xo_id];
	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;

	reg_in = com_regs->_static_aux_sel;

	if (!strcmp(reg_name, "xo_en")) {
		reg_out = xo_buf._xo_en_auxout;
		auxout_sel = xo_buf.xo_en_auxout_sel;
	} else if (!strcmp(reg_name, "bblpm")) {
		reg_out = com_regs->_bblpm_auxout;
		auxout_sel = com_regs->bblpm_auxout_sel;
	} else {
		return -EREG_NOT_SUPPORT;
	}

	spin_lock_irqsave(lock, flags);

	ret = pmic_write(&hw, &reg_in, auxout_sel);
	if (ret)
		goto FAIL1;

	ret = pmic_read(&hw, &reg_out, out);
	if (ret)
		goto FAIL2;

	spin_unlock_irqrestore(lock, flags);
	return ret;

FAIL1:
	spin_unlock_irqrestore(lock, flags);
	CLKBUF_DBG("1st Error: %d\n", ret);
	return ret;
FAIL2:
	spin_unlock_irqrestore(lock, flags);
	CLKBUF_DBG("2nd Error: %d\n", ret);
	return ret;

}

static int set_capid_pre1(void *data, int aac)
{
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	struct reg_t reg;
	int ret = 0;
	int val = 0;

	pd = (struct plat_xodata *)data;
	if (!pd || !pd->common_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = pd->common_regs->_aac_fpm_swen;

	if (aac) {
		ret |= pmic_read(&hw, &reg, &val);
		val &= ~0x1F;
		val |= aac;
		ret |= pmic_write(&hw, &reg, val);
		mdelay(1);
		val |= (1 << 5);
		ret |= pmic_write(&hw, &reg, val);
	} else {
		val &= ~(1 << 6);
		ret |= pmic_write(&hw, &reg, val);
		mdelay(1);
		val |= (1 << 6);
		ret |= pmic_write(&hw, &reg, val);
		mdelay(5);
	}

	if (ret)
		CLKBUF_DBG("set aac fpm swen failed(capid:%d)\n", aac);

	return ret;
}

static int set_capid_pre2(void *data, int capid)
{
	struct plat_xodata *pd;
	struct common_regs *com_regs;
	struct clkbuf_hw hw;
	struct reg_t reg;
	int ret = 0;
	int old_capid = 0, new_capid = 0;

	pd = (struct plat_xodata *)data;
	if (!pd)
		return -EREG_NOT_SUPPORT;

	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = com_regs->_cdac_fpm;

	ret |= pmic_read(&hw, &reg, &old_capid);
	ret |= pmic_write(&hw, &reg, capid);
	ret |= pmic_read(&hw, &reg, &new_capid);
	if (!ret)
		CLKBUF_DBG("set capid: 0x%x, old_capid: 0x%x, new_capid: 0x%x\n",
			   capid, old_capid, new_capid);
	else
		CLKBUF_ERR("set capid_pre2 failed\n");

	return ret;
}

static int get_aac(void *data, u32 *acc)
{
	int ret = 0;
	int val = 0;
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	struct reg_t reg;

	pd = (struct plat_xodata *)data;
	if (!pd || !pd->common_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = pd->common_regs->_static_aux_sel;

	ret |= pmic_read(&hw, &reg, &val);
	val &= ~ 0x7F;
	val |= 5;
	ret |= pmic_write(&hw, &reg, val);

	reg = pd->common_regs->_xo_en_auxout;
	ret |= pmic_read(&hw, &reg, acc);
	*acc &= 0x1F;
	if (!ret)
		CLKBUF_DBG("get aac 0x%x\n", *acc);
	else
		CLKBUF_ERR("get aac failed\n");

	return ret;
}

static int get_capid(void *data, u32 *capid)
{
	int ret = 0;
	struct plat_xodata *pd;
	struct common_regs *com_regs;
	struct clkbuf_hw hw;
	struct reg_t reg;

	pd = (struct plat_xodata *)data;
	if (!pd)
		return -EREG_NOT_SUPPORT;

	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = com_regs->_cdac_fpm;

	ret = pmic_read(&hw, &reg, capid);
	if (!ret)
		CLKBUF_DBG("get capid 0x%x\n", *capid);
	else
		CLKBUF_ERR("get capid failed\n");

	return ret;
}

static int set_heater(void *data, unsigned int level)
{
	int ret = 0;
	int val = 0;
	struct plat_xodata *pd;
	struct clkbuf_hw hw;
	struct reg_t reg;

	if (level > 3)
		return -1;

	pd = (struct plat_xodata *)data;
	if (!pd || !pd->common_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = pd->common_regs->_heater_sel;

	pmic_read(&hw, &reg, &val);

	val &= ~0x3;
	val |= level;

	ret = pmic_write(&hw, &reg, val);

	if (ret)
		CLKBUF_DBG("switch heater failed(0x%x)\n", level);

	return ret;
}

static int get_heater(void *data, u32 *level)
{
	int ret = 0;
	struct plat_xodata *pd;
	struct common_regs *com_regs;
	struct clkbuf_hw hw;
	struct reg_t reg;

	pd = (struct plat_xodata *)data;
	if (!pd)
		return -EREG_NOT_SUPPORT;

	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = com_regs->_heater_sel;

	ret = pmic_read(&hw, &reg, level);
	if (ret) {
		CLKBUF_DBG("get heat sel failed\n");
		return ret;
	}
	CLKBUF_DBG("get heat sel 0x%x\n", *level);

	return ret;
}

int __get_pmrcen(void *data, u32 *out)
{
	int ret = 0, tmp = 0;
	struct plat_xodata *pd;
	struct common_regs *com_regs;
	struct clkbuf_hw hw;
	struct reg_t reg;

	pd = (struct plat_xodata *)data;
	if (!pd)
		return 0;

	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	hw = pd->hw;
	reg = com_regs->_pmrc_en_l;

	ret = pmic_read(&hw, &reg, out);
	if (ret) {
		CLKBUF_DBG("read dcxo pmrc_en failed\n");
		return ret;
	}

	/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
	ret = read_with_ofs(&hw, &reg, &tmp, 1);
	if (ret) {
		CLKBUF_DBG("read dcxo pmrc_en ofs: 1 failed\n");
		return ret;
	}
	*out |= (tmp << 8);

	CLKBUF_DBG("dump pmrcen = %x", *out);
	return ret;
}

int get_xo_voter(void *data, int xo_id, u32 *out)
{
	u32 tmp = 0;
	int ret = 0;
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct xo_buf_t xo_buf;
	struct clkbuf_hw hw;
	struct reg_t reg;
	unsigned long flags = 0;
	spinlock_t *lock = pd->lock;

	xo_buf = (pd->xo_buf_t)[xo_id];
	hw = pd->hw;
	reg = xo_buf._rc_voter;

	spin_lock_irqsave(lock, flags);

	ret = pmic_read(&hw, &reg, out);
	if (ret)
		goto FAIL1;

	/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
	ret = read_with_ofs(&hw, &reg, &tmp, 1);
	if (ret)
		goto FAIL2;

	*out |= (tmp << 8);

	spin_unlock_irqrestore(lock, flags);

	CLKBUF_DBG("dump voter = %x", *out);
	return ret;

FAIL1:
	spin_unlock_irqrestore(lock, flags);
	CLKBUF_DBG("1st Error: %d\n", ret);
	return ret;
FAIL2:
	spin_unlock_irqrestore(lock, flags);
	CLKBUF_DBG("2nd Error: %d\n", ret);
	return ret;
}

int set_xo_voter(void *data, int xo_id, u32 vote)
{
	u32 spmi_mask = 0;
	int ret = 0;
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct xo_buf_t xo_buf;
	struct clkbuf_hw hw;
	struct common_regs *com_regs;
	struct reg_t reg;
	unsigned long flags = 0;
	spinlock_t *lock = pd->lock;

	xo_buf = (pd->xo_buf_t)[xo_id];
	hw = pd->hw;
	reg = xo_buf._rc_voter;
	com_regs = pd->common_regs;
	if (!com_regs)
		return -EREG_NOT_SUPPORT;

	spmi_mask = com_regs->spmi_mask;

	vote &= spmi_mask;

	spin_lock_irqsave(lock, flags);

	ret = pmic_write(&hw, &reg, vote);
	if (ret)
		goto FAIL1;

	vote >>= 8;

	/* if pmic is spmi type rw, read 1 more reg to fit 16bits */
	ret = write_with_ofs(&hw, &reg, vote, 1);
	if (ret)
		goto FAIL2;

	spin_unlock_irqrestore(lock, flags);
	return ret;

FAIL1:
	spin_unlock_irqrestore(lock, flags);
	CLKBUF_DBG("1st Error: %d\n", ret);
	return ret;
FAIL2:
	spin_unlock_irqrestore(lock, flags);
	CLKBUF_DBG("2nd Error: %d\n", ret);
	return ret;
}

static int __get_xo_cmd_hdlr_v1(void *data, int xo_id, char *buf, int len)
{
	u32 out;
	int ret = 0;

	/*****XO MODE****/
	ret = get_xo_mode(data, xo_id, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "xo_mode: <%2d> ", out);
	/****EN_M****/
	ret = get_xo_en_m(data, xo_id, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "xo_en_m: <%2d> ", out);
	/****AUXOUT****/
	ret = get_xo_auxout(data, xo_id, &out, "xo_en");
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "auxout: <%2d>\n", out);
	/****IMPEDANCE****/
	ret = get_xo_impedance(data, xo_id, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "imp: <0x%08x> ", out);
	/******DESENSE: makesure platform data reg exist*****/
	ret = get_xo_desense(data, xo_id, &out);
	if (ret)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"desense: not support ");
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
				"desense: <0x%08x> ", out);
	/****VOTER****/
	ret = get_xo_voter(data, xo_id, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "voter: <0x%04x>\n", out);

	return len;
}

static int __set_xo_cmd_hdlr_v1(void *data, int cmd, int xo_id, u32 input,
				int perms)
{
	int ret = 0;

	/*handle premission from dts*/
	perms = perms & 0xffff;

	CLKBUF_DBG("cmd: %x, perms: %x\n", cmd, perms);
	switch (cmd & perms) {
	case SET_XO_MODE: // = 0x0001,
		ret = set_xo_mode(data, xo_id, input);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_XO_EN_M: // = 0x0002,
		ret = set_xo_en_m(data, xo_id, input);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_XO_IMPEDANCE: // = 0x0004,
		ret = set_xo_impedance(data, xo_id, input);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_XO_DESENSE: // = 0x0008,
		ret = set_xo_desense(data, xo_id, input);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_XO_VOTER: // = 0x0010,
		ret = set_xo_voter(data, xo_id, input);
		if (ret)
			goto WRITE_FAIL;
		break;

	default:
		ret = -EUSER_INPUT_INVALID;
		goto WRITE_FAIL;
	}
	return ret;
WRITE_FAIL:
	return ret;
}

static int __get_pmic_common_hdlr(void *data, char *buf, int len)
{
	u32 out = 0;
	int ret = 0;

	/****AAC****/
	ret = get_aac(data, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "aac: <%d>\n", out);

	/****CAPID****/
	ret = get_capid(data, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "capid: <%d>\n", out);

	/****HEATER****/
	ret = get_heater(data, &out);
	if (ret)
		return len;
	len += snprintf(buf + len, PAGE_SIZE - len, "heater: <%d>\n", out);

	return len;
}

static int __set_pmic_common_hdlr(void *data, int cmd, int arg, int perms)
{
	int ret = 0;

	/*cancel premission handler*/

	CLKBUF_DBG("cmd: %x, arg: %x\n", cmd, arg);
	switch (cmd) {
	case SET_CAPID_PRE_1: // = 0x1000,
		ret = set_capid_pre1(data, arg);
		if (ret)
			goto WRITE_FAIL;
		break;
	case SET_CAPID_PRE_2: // = 0x2000,
		ret = set_capid_pre2(data, arg);
		if (ret)
			goto WRITE_FAIL;
		break;
	case SET_CAPID: // = 0x4000,
		ret |= set_capid_pre1(data, 0);
		ret |= set_capid_pre2(data, arg);
		if (ret)
			goto WRITE_FAIL;
		break;
	case SET_HEATER: // = 0x8000,
		ret = set_heater(data, arg);
		if (ret)
			goto WRITE_FAIL;
		break;
	default:
		goto WRITE_FAIL;
	}
	return ret;
WRITE_FAIL:
	return cmd;
}

int __dump_pmic_debug_regs(void *data, char *buf, int len)
{
	struct plat_xodata *pd = (struct plat_xodata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct reg_t *reg_p = NULL;
	int ret = 0;
	int i;
	u32 out;

	reg_p = pd->debug_regs;
	if (!reg_p) {
		CLKBUF_DBG("read rep_p failed\n");
		return len;
	}

	for (i = 0; strcmp((*reg_p).name, "NULL"); i++, reg_p++) {
		//spin lock
		ret = pmic_read(&hw, reg_p, &out);
		//unlock
		if (ret) {
			CLKBUF_DBG("read debug_regs[%d] failed\n", i);
			break;
		}

		if (!buf)
			CLKBUF_DBG(
				"PMIC DBG reg: %s Addr: 0x%08x Val: 0x%08x\n",
				(*reg_p).name, (*reg_p).ofs, out);
		else
			len += snprintf(
				buf + len, PAGE_SIZE - len,
				"PMIC DBG reg: %s Addr: 0x%08x Val: 0x%08x\n",
				(*reg_p).name, (*reg_p).ofs, out);
	}

	/****BBLPM AUXOUT****/
	ret = get_xo_auxout(pd, 0, &out, "bblpm");
	if (ret)
		return len;
	if (!buf)
		CLKBUF_DBG("bblpm auxout: <%2d>\n", out);
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
				"bblpm auxout: <%2d>\n", out);

	return len;
}

static struct clkbuf_operation clkbuf_ops_v1 = {
	.get_pmrcen = __get_pmrcen,
	.dump_pmic_debug_regs = __dump_pmic_debug_regs,
	.get_xo_cmd_hdlr = __get_xo_cmd_hdlr_v1,
	.set_xo_cmd_hdlr = __set_xo_cmd_hdlr_v1,
};

/* tablet ops */
static struct clkbuf_operation clkbuf_ops_v2 = {
	.get_pmrcen = __get_pmrcen,
	.dump_pmic_debug_regs = __dump_pmic_debug_regs,
	.get_xo_cmd_hdlr = __get_xo_cmd_hdlr_v1,
	.set_xo_cmd_hdlr = __set_xo_cmd_hdlr_v1,
	.set_pmic_common_hdlr = __set_pmic_common_hdlr,
	.get_pmic_common_hdlr = __get_pmic_common_hdlr,
};

static struct clkbuf_hdlr pmic_hdlr_v1 = {
	.ops = &clkbuf_ops_v1,
	.data = &mt6685_data,
};

/* tablet hdlr */
static struct clkbuf_hdlr pmic_hdlr_v2 = {
	.ops = &clkbuf_ops_v2,
	.data = &mt6685_tb_data,
};

static struct match_pmic mt6685_match_pmic = {
	.name = "mediatek,mt6685-clkbuf",
	.hdlr = &pmic_hdlr_v1,
	.init = &pmic_init_v1,
};

static struct match_pmic mt6685_tb_match_pmic = {
	.name = "mediatek,mt6685-tb-clkbuf",
	.hdlr = &pmic_hdlr_v2,
	.init = &pmic_init_v1,
};

static struct match_pmic *matches_pmic[] = {
	&mt6685_match_pmic,
	&mt6685_tb_match_pmic,
	NULL,
};

int count_pmic_node(struct device_node *clkbuf_node)
{
	struct device_node *pmic_node, *xo_buf;
	int num_xo = 0;
	/*check clkbuf controller*/
	pmic_node = of_parse_phandle(clkbuf_node, "pmic", 0);

	if (!pmic_node) {
		CLKBUF_DBG("find pmic_node failed, not support DCXO\n");
		return 0;
	}
	/*count xo numbers*/
	for_each_child_of_node(pmic_node, xo_buf) {
		num_xo++;
	}

	CLKBUF_DBG("xo support numbers: %d", num_xo);
	return num_xo;
}

static int parsing_dts_xo_cmds(struct device_node *xo_buf,
			       struct clkbuf_dts *array)
{
	/*****parsing init dts cmd****/
	int num_args = 2, offset = 0, cmd_size = 0, num_xo_cmd = 0, i;
	struct xo_dts_cmd *xo_dts_cmd;

	if (!of_get_property(xo_buf, "xo-cmd", &num_xo_cmd))
		array->init_dts_cmd.num_xo_cmd = 0;

	if (num_xo_cmd != 0) {
		array->init_dts_cmd.num_xo_cmd = num_xo_cmd;
		cmd_size = sizeof(*xo_dts_cmd) * (num_xo_cmd + 1);
		array->init_dts_cmd.xo_dts_cmd = kzalloc(cmd_size, GFP_KERNEL);

		if (!array->init_dts_cmd.xo_dts_cmd) {
			CLKBUF_DBG("-ENOMEM\n");
			return -ENOMEM;
		}

		xo_dts_cmd = array->init_dts_cmd.xo_dts_cmd;

		/*save cmd_sets to xo_dts_cmd*/
		for (i = 0; i < num_xo_cmd; i++, xo_dts_cmd++) {
			offset = i * num_args;
			if (of_property_read_u32_index(xo_buf, "xo-cmd", offset,
						       &xo_dts_cmd->xo_cmd))
				break;
			if (of_property_read_u32_index(xo_buf, "xo-cmd",
						       offset + 1,
						       &xo_dts_cmd->cmd_val))
				break;
		}
	}
	return 0;
}

struct clkbuf_dts *parse_pmic_dts(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums)
{
	struct device_node *pmic_node, *xo_buf;
	struct platform_device *pmic_dev;
	struct regmap *pmic_base;
	unsigned int num_xo = 0;

	pmic_node = of_parse_phandle(clkbuf_node, "pmic", 0);

	if (!pmic_node) {
		CLKBUF_DBG("find pmic_node failed, not support DCXO\n");
		return NULL;
	}
	/*count xo numbers*/
	for_each_child_of_node(pmic_node, xo_buf) {
		num_xo++;
	}

	pmic_dev = of_find_device_by_node(pmic_node);
	pmic_base = dev_get_regmap(pmic_dev->dev.parent, NULL);

	/*start parsing pmic dcxo dts*/
	for_each_child_of_node(pmic_node, xo_buf) {
		/* default for optional field */
		const char *comp = NULL;
		int xo_id, perms = 0xffff; /*API need 0xffffffff*/

		if (parsing_dts_xo_cmds(xo_buf, array)) {
			CLKBUF_DBG("-ENOMEM\n");
			return array;
		}
		of_property_read_u32(xo_buf, "xo-id", &xo_id);
		of_property_read_u32(xo_buf, "perms", &perms);
		of_property_read_string(pmic_node, "compatible", &comp);

		array->nums = nums;
		array->num_xo = num_xo;
		array->comp = (char *)comp;
		array->xo_name = (char *)xo_buf->name;
		array->hw.hw_type = PMIC;
		array->xo_id = xo_id;
		array->perms = perms;
		array->hw.base.map = pmic_base;
		array++;
	}

	return array;
}

int clkbuf_pmic_init(struct clkbuf_dts *array, struct device *dev)
{
	struct match_pmic **match_pmic = matches_pmic;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int i;
	int nums = array->nums;
	int num_xo = 0;

	CLKBUF_DBG("\n");

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;
		/*each init function should make sure array->hw is not null*/
		hw = array->hw;
		/*only need one PMIC obj, no need loop all obj*/
		if (IS_PMIC_HW(hw.hw_type)) {
			num_xo = array->num_xo;
			break;
		}
	}

	if (!IS_PMIC_HW(array->hw.hw_type)) {
		CLKBUF_DBG("no dts pmic HW!\n");
		return -1;
	}

	/* find match by compatible */
	for (; (*match_pmic) != NULL; match_pmic++) {
		char *comp = (*match_pmic)->name;
		char *target =
			array->comp;

		if (strcmp(comp, target) == 0)
			break;
	}

	if (*match_pmic == NULL) {
		CLKBUF_DBG("no match pmic compatible!\n");
		return -1;
	}

	/* init flow: prepare xo obj to specific array element*/
	for (i = 0; i < num_xo; i++, array++) {
		char *src = array->comp;
		char *plat_target = (*match_pmic)->name;

		if (strcmp(src, plat_target) == 0)
			(*match_pmic)->init(array, *match_pmic);
	}

	CLKBUF_DBG("\n");
	return 0;
}
