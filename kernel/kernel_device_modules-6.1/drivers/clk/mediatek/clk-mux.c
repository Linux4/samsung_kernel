// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/sched/clock.h>
#include <linux/clk-provider.h>

#include "clk-mtk.h"
#include "clk-mux.h"

static bool is_registered;
static const struct dfs_ops *mux_dfs_ops;

static inline struct mtk_clk_mux *to_mtk_clk_mux(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_mux, hw);
}

static inline struct mtk_clk_user *to_mtk_clk_user(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_user, hw);
}

void mtk_clk_mux_register_callback(const struct dfs_ops *ops)
{
	mux_dfs_ops = ops;
}
EXPORT_SYMBOL_GPL(mtk_clk_mux_register_callback);

static int mtk_clk_mux_enable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = BIT(mux->data->gate_shift);

	return regmap_update_bits(mux->regmap, mux->data->mux_ofs,
			mask, ~mask);
}

static void mtk_clk_mux_disable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = BIT(mux->data->gate_shift);

	regmap_update_bits(mux->regmap, mux->data->mux_ofs, mask, mask);
}

static int mtk_clk_mux_enable_setclr(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	regmap_write(mux->regmap, mux->data->clr_ofs,
		BIT(mux->data->gate_shift));

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return 0;
}

static void mtk_clk_mux_disable_setclr(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);

	regmap_write(mux->regmap, mux->data->set_ofs,
			BIT(mux->data->gate_shift));
}

static int mtk_clk_mux_is_enabled(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;

	if (!is_registered)
		return 0;

	regmap_read(mux->regmap, mux->data->mux_ofs, &val);

	return (val & BIT(mux->data->gate_shift)) == 0;
}

static int mtk_clk_hwv_mux_is_enabled(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;

	if (!is_registered)
		return 0;

	regmap_read(mux->hwv_regmap, mux->data->hwv_set_ofs, &val);

	return (val & BIT(mux->data->gate_shift)) != 0;
}

static int mtk_clk_hwv_mux_is_done(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;

	regmap_read(mux->hwv_regmap, mux->data->hwv_sta_ofs, &val);

	return (val & BIT(mux->data->gate_shift)) != 0;
}

static int mtk_clk_hwv_mux_enable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0, val2 = 0;
	bool is_done = false;
	int i = 0;

	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);
	regmap_write(mux->hwv_regmap, mux->data->hwv_set_ofs,
			BIT(mux->data->gate_shift));

	while (!mtk_clk_hwv_mux_is_enabled(hw)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (1) {
		if (!is_done)
			regmap_read(mux->hwv_regmap, mux->data->hwv_sta_ofs, &val);

		if (((val & BIT(mux->data->gate_shift)) != 0))
			is_done = true;

		if (is_done) {
			regmap_read(mux->regmap, mux->data->mux_ofs, &val2);
			if ((val2 & BIT(mux->data->gate_shift)) == 0)
				break;
		}

		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;

		i++;
	}

	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return 0;

hwv_done_fail:
	regmap_read(mux->regmap, mux->data->mux_ofs, &val);
	regmap_read(mux->hwv_regmap, mux->data->hwv_sta_ofs, &val2);
	pr_err("%s mux enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_prepare_fail:
	regmap_read(mux->regmap, mux->data->hwv_sta_ofs, &val);
	pr_err("%s mux prepare timeout(%x)\n", clk_hw_get_name(hw), val);

	mtk_clk_notify(mux->regmap, mux->hwv_regmap, NULL,
			mux->data->mux_ofs, (mux->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			mux->data->gate_shift, CLK_EVT_HWV_CG_TIMEOUT);
	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return -EBUSY;
}

static void mtk_clk_hwv_mux_disable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val;
	int i = 0;

	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);

	regmap_write(mux->hwv_regmap, mux->data->hwv_clr_ofs,
			BIT(mux->data->gate_shift));

	while (mtk_clk_hwv_mux_is_enabled(hw)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_clk_hwv_mux_is_done(hw)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return;

hwv_done_fail:
	regmap_read(mux->regmap, mux->data->mux_ofs, &val);
	pr_err("%s mux disable timeout(%d ns)(%x)\n", clk_hw_get_name(hw),
			i * MTK_WAIT_HWV_DONE_US, val);
hwv_prepare_fail:
	pr_err("%s mux unprepare timeout(%d ns)\n", clk_hw_get_name(hw),
			i * MTK_WAIT_HWV_PREPARE_US);

	mtk_clk_notify(mux->regmap, mux->hwv_regmap, clk_hw_get_name(hw),
			mux->data->mux_ofs, (mux->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			mux->data->gate_shift, CLK_EVT_HWV_CG_TIMEOUT);
	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);
}

static int mtk_clk_ipi_mux_enable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	struct ipi_callbacks *cb;
	u32 val = 0;
	int ret = 0;

	cb = mtk_clk_get_ipi_cb();

	if (cb && cb->clk_enable) {
		ret = cb->clk_enable(mux->data->ipi_shift);
		if (ret) {
			regmap_read(mux->regmap, mux->data->mux_ofs, &val);
			pr_err("Failed to send enable ipi to VCP %s: 0x%x(%d)\n",
					clk_hw_get_name(hw), val, ret);
			goto err;
		}
	}

	return 0;
err:
	mtk_clk_notify(mux->regmap, mux->hwv_regmap, clk_hw_get_name(hw),
			mux->data->mux_ofs, (mux->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			mux->data->gate_shift, CLK_EVT_IPI_CG_TIMEOUT);
	return ret;
}

static void mtk_clk_ipi_mux_disable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	struct ipi_callbacks *cb;
	u32 val = 0;
	int ret = 0;

	cb = mtk_clk_get_ipi_cb();

	if (cb && cb->clk_disable) {
		ret = cb->clk_disable(mux->data->ipi_shift);
		if (ret) {
			regmap_read(mux->regmap, mux->data->mux_ofs, &val);
			pr_err("Failed to send disable ipi to VCP %s: 0x%x(%d)\n",
					clk_hw_get_name(hw), val, ret);
			goto err;
		}
	}

	return;

err:
	mtk_clk_notify(mux->regmap, mux->hwv_regmap, clk_hw_get_name(hw),
			mux->data->mux_ofs, (mux->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			mux->data->gate_shift, CLK_EVT_IPI_CG_TIMEOUT);
}

static u8 mtk_clk_mux_get_parent(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = GENMASK(mux->data->mux_width - 1, 0);
	u32 val = 0;

	regmap_read(mux->regmap, mux->data->mux_ofs, &val);
	val = (val >> mux->data->mux_shift) & mask;

	return val;
}

static int __mtk_clk_mux_set_parent_lock(struct clk_hw *hw, u8 index, bool setclr, bool upd)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	struct clk_hw *qs_hw;
	u32 mask = GENMASK(mux->data->mux_width - 1, 0);
	u32 val = 0, orig = 0;
	unsigned long flags = 0;
	bool qs_pll_need_off = false;
	int ret = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	if (setclr) {
		regmap_read(mux->regmap, mux->data->mux_ofs, &orig);

		val = (orig & ~(mask << mux->data->mux_shift))
				| (index << mux->data->mux_shift);

		if (val != orig) {
			if ((mux->flags & CLK_ENABLE_QUICK_SWITCH) == CLK_ENABLE_QUICK_SWITCH) {
				val = mux->data->qs_shift;

				qs_hw = clk_hw_get_parent_by_index(hw, val);
				if (!qs_hw) {
					pr_err("qs_hw is null, index = %d\n", val);
					ret = -EFAULT;
					goto null_pointer_error;
				}
				qs_hw = clk_hw_get_parent(qs_hw);
				if (!qs_hw) {
					pr_err("qs_hw is null\n");
					ret = -EFAULT;
					goto null_pointer_error;
				}

				if (!mtk_hwv_pll_is_on(qs_hw)) {
					/* mainpll2 enable for quick switch */
					mtk_hwv_pll_on(qs_hw);
					qs_pll_need_off = true;
				}
			}

			if ((mux->flags & CLK_ENABLE_MERGE_CONTROL) == CLK_ENABLE_MERGE_CONTROL) {
				ret = sp_merge_clk_control(mux, index, mask);
			} else {
				regmap_write(mux->regmap, mux->data->clr_ofs,
						mask << mux->data->mux_shift);
				regmap_write(mux->regmap, mux->data->set_ofs,
						index << mux->data->mux_shift);
			}
		}

		if (upd)
			if ((mux->flags & CLK_ENABLE_MERGE_CONTROL) != CLK_ENABLE_MERGE_CONTROL)
				regmap_write(mux->regmap, mux->data->upd_ofs,
						BIT(mux->data->upd_shift));

		if (qs_pll_need_off && (mux->flags & CLK_ENABLE_QUICK_SWITCH)
				== CLK_ENABLE_QUICK_SWITCH) {
			/* mainpll2 disable for quick switch */
			mtk_hwv_pll_off(qs_hw);
		}
	} else
		regmap_update_bits(mux->regmap, mux->data->mux_ofs,
			mask << mux->data->mux_shift,
			index << mux->data->mux_shift);

	if (mux->data->chk_ofs) {
		regmap_read(mux->regmap, mux->data->chk_ofs, &val);
		if (val & mux->data->chk_shift)
			mtk_clk_notify(mux->regmap, NULL, clk_hw_get_name(hw),
				mux->data->chk_ofs, 0,
				mux->data->chk_shift, CLK_EVT_SET_PARENT_ERR);
	}

null_pointer_error:
	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return ret;
}

static int mtk_clk_mux_set_parent_lock(struct clk_hw *hw, u8 index)
{
	return __mtk_clk_mux_set_parent_lock(hw, index, false, false);
}

static int mtk_clk_mux_set_parent_setclr_lock(struct clk_hw *hw, u8 index)
{
	return __mtk_clk_mux_set_parent_lock(hw, index, true, false);
}

static int mtk_clk_mux_set_parent_setclr_upd_lock(struct clk_hw *hw, u8 index)
{
	return __mtk_clk_mux_set_parent_lock(hw, index, true, true);
}

static int mtk_clk_hwv_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask;
	u32 opp_mask;
	u32 val = 0, val2 = 0, orig = 0, renew = 0;
	int i = 0;

	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);

	mask = GENMASK(6, 0) << mux->data->mux_shift;
	opp_mask = GENMASK(index, 0) << mux->data->mux_shift;

	regmap_read(mux->hwv_regmap, mux->data->hwv_set_ofs, &orig);

	val = orig & mask;
	val ^= opp_mask;

	if (val > opp_mask)
		regmap_write(mux->hwv_regmap, mux->data->hwv_clr_ofs, val);
	else
		regmap_write(mux->hwv_regmap, mux->data->hwv_set_ofs, val);

	while (1) {
		regmap_read(mux->hwv_regmap, mux->data->hwv_set_ofs, &renew);
		if ((renew & opp_mask) == opp_mask)
			break;
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (1) {
		regmap_read(mux->hwv_regmap, mux->data->hwv_sta_ofs, &renew);

		if ((renew & val) == val)
			break;

		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;

		i++;
	}

	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return 0;

hwv_done_fail:
	regmap_read(mux->regmap, mux->data->mux_ofs, &val);
	regmap_read(mux->hwv_regmap, mux->data->hwv_sta_ofs, &val2);
	pr_err("%s mux enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_prepare_fail:
	regmap_read(mux->regmap, mux->data->hwv_sta_ofs, &val);
	pr_err("%s mux prepare timeout(%x)\n", clk_hw_get_name(hw), val);
	mtk_clk_notify(mux->regmap, mux->hwv_regmap, clk_hw_get_name(hw),
			mux->data->mux_ofs, (mux->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			mux->data->gate_shift, CLK_EVT_SET_PARENT_TIMEOUT);
	if (mux->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return -EBUSY;
}

static int mtk_clk_mux_determine_rate_closest(struct clk_hw *hw, struct clk_rate_request *req)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	int parent_index = -1;
	unsigned long now = 0;

	now = clk_hw_get_rate(hw);
	parent_index = clk_hw_get_parent_index(hw);
	if (parent_index >= 0) {
		req->best_parent_hw = clk_hw_get_parent_by_index(hw, parent_index);
		if (!req->best_parent_hw)
			return -EINVAL;

		if ((mux->flags & MUX_ROUND_CLOSEST) == MUX_ROUND_CLOSEST) {
			req->best_parent_rate = clk_hw_get_rate(req->best_parent_hw);
			__clk_mux_determine_rate_closest(hw, req);

			if (abs(now - req->rate) > abs(req->best_parent_rate - req->rate))
				return req->best_parent_rate;
		}
	}

	return now;
}

static int mtk_clk_mux_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	int idx, parent_index = -1;
	int ret = 0;

	if (mux_dfs_ops != NULL && mux_dfs_ops->get_opp != NULL)
		parent_index = mux_dfs_ops->get_opp(clk_hw_get_name(hw));

	if (parent_index >= 0) {
		ret = mtk_clk_hwv_mux_set_parent(hw, parent_index);
		if (!ret) {
			idx = mtk_clk_mux_get_parent(hw);
			req->best_parent_hw = clk_hw_get_parent_by_index(hw, parent_index);
			if (!req->best_parent_hw)
				return -EINVAL;

			req->best_parent_rate = clk_hw_get_rate(req->best_parent_hw);
			ret = clk_hw_set_parent(hw, req->best_parent_hw);
			if (ret)
				pr_err("mux set parent error(%d)\n", ret);
		}

		return ret;
	} else
		return parent_index;

	return 0;
}

const struct clk_ops mtk_mux_ops = {
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_lock,
};
EXPORT_SYMBOL_GPL(mtk_mux_ops);


const struct clk_ops mtk_mux_clr_set_ops = {
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_setclr_lock,
};
EXPORT_SYMBOL_GPL(mtk_mux_clr_set_ops);

const struct clk_ops mtk_mux_clr_set_upd_ops = {
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_setclr_upd_lock,
};
EXPORT_SYMBOL_GPL(mtk_mux_clr_set_upd_ops);

const struct clk_ops mtk_mux_gate_ops = {
	.enable = mtk_clk_mux_enable,
	.disable = mtk_clk_mux_disable,
	.is_enabled = mtk_clk_mux_is_enabled,
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_lock,
};
EXPORT_SYMBOL_GPL(mtk_mux_gate_ops);

const struct clk_ops mtk_mux_gate_clr_set_upd_2_ops = {
	.enable = mtk_clk_mux_enable_setclr,
	.disable = mtk_clk_mux_disable_setclr,
	.is_enabled = mtk_clk_mux_is_enabled,
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_setclr_upd_lock,
	.determine_rate = mtk_clk_mux_determine_rate_closest,
};
EXPORT_SYMBOL_GPL(mtk_mux_gate_clr_set_upd_2_ops);

const struct clk_ops mtk_mux_gate_clr_set_upd_ops = {
	.enable = mtk_clk_mux_enable_setclr,
	.disable = mtk_clk_mux_disable_setclr,
	.is_enabled = mtk_clk_mux_is_enabled,
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_setclr_upd_lock,
};
EXPORT_SYMBOL_GPL(mtk_mux_gate_clr_set_upd_ops);

const struct clk_ops mtk_hwv_mux_ops = {
	.enable = mtk_clk_hwv_mux_enable,
	.disable = mtk_clk_hwv_mux_disable,
	.is_enabled = mtk_clk_mux_is_enabled,
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_setclr_upd_lock,
};
EXPORT_SYMBOL_GPL(mtk_hwv_mux_ops);

const struct clk_ops mtk_hwv_dfs_mux_dummy_ops = {
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_hwv_mux_set_parent,
	.determine_rate = mtk_clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(mtk_hwv_dfs_mux_dummy_ops);

const struct clk_ops mtk_hwv_dfs_mux_ops = {
	.enable = mtk_clk_hwv_mux_enable,
	.disable = mtk_clk_hwv_mux_disable,
	.is_enabled = mtk_clk_mux_is_enabled,
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_hwv_mux_set_parent,
	.determine_rate = mtk_clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(mtk_hwv_dfs_mux_ops);

const struct clk_ops mtk_ipi_mux_ops = {
	.prepare = mtk_clk_ipi_mux_enable,
	.unprepare = mtk_clk_ipi_mux_disable,
	.enable = mtk_clk_hwv_mux_enable,
	.disable = mtk_clk_hwv_mux_disable,
	.is_enabled = mtk_clk_mux_is_enabled,
	.get_parent = mtk_clk_mux_get_parent,
	.set_parent = mtk_clk_mux_set_parent_setclr_upd_lock,
};
EXPORT_SYMBOL_GPL(mtk_ipi_mux_ops);

static struct clk *mtk_clk_register_mux(const struct mtk_mux *mux,
				 struct regmap *regmap,
				 struct regmap *hw_voter_regmap,
				 spinlock_t *lock)
{
	struct mtk_clk_mux *clk_mux;
	struct clk_init_data init = {};
	struct clk *clk;

	clk_mux = kzalloc(sizeof(*clk_mux), GFP_KERNEL);
	if (!clk_mux)
		return ERR_PTR(-ENOMEM);

	init.name = mux->name;
	init.flags = mux->flags;
	init.parent_names = mux->parent_names;
	init.num_parents = mux->num_parents;
	if (mux->flags & CLK_USE_HW_VOTER) {
		if (hw_voter_regmap)
			init.ops = mux->ops;
		else
			init.ops = mux->dma_ops;
	} else
		init.ops = mux->ops;

	clk_mux->regmap = regmap;
	clk_mux->hwv_regmap = hw_voter_regmap;
	clk_mux->data = mux;
	clk_mux->lock = lock;
	clk_mux->flags = mux->flags;
	clk_mux->hw.init = &init;

	clk = clk_register(NULL, &clk_mux->hw);
	if (IS_ERR(clk)) {
		kfree(clk_mux);
		return clk;
	}

	return clk;
}

int mtk_clk_register_muxes(const struct mtk_mux *muxes,
			   int num, struct device_node *node,
			   spinlock_t *lock,
			   struct clk_onecell_data *clk_data)
{
	struct regmap *regmap, *hw_voter_regmap, *hwv_mult_regmap = NULL;
	struct clk *clk;
	int i;

	is_registered = false;

	regmap = syscon_node_to_regmap(node);
	if (IS_ERR(regmap)) {
		pr_err("Cannot find regmap for %pOF: %ld\n", node,
		       PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	hw_voter_regmap = syscon_regmap_lookup_by_phandle(node, "hw-voter-regmap");
	if (IS_ERR(hw_voter_regmap))
		hw_voter_regmap = NULL;

	for (i = 0; i < num; i++) {
		const struct mtk_mux *mux = &muxes[i];

		if (IS_ERR_OR_NULL(clk_data->clks[mux->id])) {
			if (mux->hwv_comp) {
				hwv_mult_regmap = syscon_regmap_lookup_by_phandle(node,
						mux->hwv_comp);
				if (IS_ERR(hwv_mult_regmap))
					hwv_mult_regmap = NULL;
				hw_voter_regmap = hwv_mult_regmap;
			}

			clk = mtk_clk_register_mux(mux, regmap, hw_voter_regmap, lock);

			if (IS_ERR(clk)) {
				pr_err("Failed to register clk %s: %ld\n",
				       mux->name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[mux->id] = clk;
		}
	}

	is_registered = true;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_clk_register_muxes);

static int mtk_clk_mux_user_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	struct mtk_clk_user *user = to_mtk_clk_user(hw);
	struct clk_hw *hw_t = user->target_hw;

	if (mux_dfs_ops != NULL && mux_dfs_ops->set_opp != NULL) {
		mux_dfs_ops->set_opp(clk_hw_get_name(hw), req->rate);
		req->rate = clk_hw_round_rate(hw_t, req->rate);
	}

	return 0;
}

static unsigned long mtk_clk_mux_user_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	if (mux_dfs_ops != NULL && mux_dfs_ops->get_rate != NULL)
		return mux_dfs_ops->get_rate(clk_hw_get_name(hw));

	return 0;
}

const struct clk_ops mtk_mux_user_ops = {
	.determine_rate = mtk_clk_mux_user_determine_rate,
	.recalc_rate = mtk_clk_mux_user_recalc_rate,
};
EXPORT_SYMBOL_GPL(mtk_mux_user_ops);

static struct clk *mtk_clk_mux_register_user(const struct mtk_mux_user *user,
				struct clk_hw *target_hw,
				spinlock_t *lock)
{
	struct mtk_clk_user *clk_user;
	struct clk_init_data init = {};
	struct clk *clk;

	clk_user = kzalloc(sizeof(*clk_user), GFP_KERNEL);
	if (!clk_user)
		return ERR_PTR(-ENOMEM);

	init.name = user->name;
	init.flags = user->flags;
	init.parent_names = NULL;
	init.num_parents = 0;
	init.ops = user->ops;

	clk_user->lock = lock;
	clk_user->flags = user->flags;
	clk_user->hw.init = &init;
	clk_user->target_hw = target_hw;

	clk = clk_register(NULL, &clk_user->hw);
	if (IS_ERR(clk)) {
		kfree(clk_user);
		return clk;
	}

	return clk;
}

int mtk_clk_mux_register_user_clks(const struct mtk_mux_user *clks, int num,
		spinlock_t *lock, struct clk_onecell_data *clk_data, struct device *dev)
{
	struct clk *clk_t;
	struct clk *clk;
	struct clk_hw *hw;
	int i;

	for (i = 0; i < num; i++) {
		const struct mtk_mux_user *user = &clks[i];

		if (IS_ERR_OR_NULL(clk_data->clks[user->id])) {
			clk_t = devm_clk_get(dev, user->target_name);
			if (IS_ERR(clk_t)) {
				pr_err("Failed to find clk target %s: %ld\n",
						user->name, PTR_ERR(clk_t));
				continue;
			}

			hw = __clk_get_hw(clk_t);

			clk = mtk_clk_mux_register_user(user, hw, lock);

			if (IS_ERR(clk)) {
				pr_err("Failed to register clk %s: %ld\n",
						user->name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[user->id] = clk;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_clk_mux_register_user_clks);

MODULE_LICENSE("GPL");
