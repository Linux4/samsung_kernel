// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: yiwen chiou<yiwen.chiou@mediatek.com
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-base-afe.h"

#include "mt6989-afe-cm.h"
#include "mt6989-afe-common.h"

static unsigned int cm0_rate;
static unsigned int cm1_rate;


void mt6989_set_cm_rate(int id, unsigned int rate)
{
	if (id == CM0)
		cm0_rate = rate;
	else
		cm1_rate = rate;
}
EXPORT_SYMBOL_GPL(mt6989_set_cm_rate);
static int mt6989_convert_cm_ch(unsigned int ch)
{
	return ch - 1;
}

static unsigned int calculate_cm_update(int rate, int ch)
{
	int update_val;

	pr_info("%s(), rate %d, channel %d\n",
		 __func__, rate, ch);

	update_val = 26000000 / rate / (ch / 2);
	update_val = update_val * 10 / 7;
	if (update_val > 100)
		update_val = 100;
	if (update_val < 7)
		update_val = 7;

	return (unsigned int)update_val;
}

int mt6989_set_cm(struct mtk_base_afe *afe, int id,
	       unsigned int update, bool swap, unsigned int ch)
{
	int cm_reg = 0;
	int cm_rate_mask = 0;
	int cm_update_mask = 0;
	int cm_swap_mask = 0;
	int cm_ch_mask = 0;
	int cm_rate_shift = 0;
	int cm_update_shift = 0;
	int cm_swap_shift = 0;
	int cm_ch_shift = 0;
	unsigned int rate = 0;
	unsigned int update_val = 0;

	pr_info("%s()-0, CM%d, rate %d, update %d, swap %d, ch %d\n",
		__func__, id, rate, update, swap, ch);

	switch (id) {
	case CM0:
		rate = cm0_rate;
		cm_reg = AFE_CM0_CON0;
		cm_rate_mask = AFE_CM0_1X_EN_SEL_FS_MASK;
		cm_rate_shift = AFE_CM0_1X_EN_SEL_FS_SFT;
		cm_update_mask = AFE_CM0_UPDATE_CNT_MASK;
		cm_update_shift = AFE_CM0_UPDATE_CNT_SFT;
		cm_swap_mask = AFE_CM0_BYTE_SWAP_MASK;
		cm_swap_shift = AFE_CM0_BYTE_SWAP_SFT;
		cm_ch_mask = AFE_CM0_CH_NUM_MASK;
		cm_ch_shift = AFE_CM0_CH_NUM_SFT;
		break;
	case CM1:
		rate = cm1_rate;
		cm_reg = AFE_CM1_CON0;
		cm_rate_mask = AFE_CM1_1X_EN_SEL_FS_MASK;
		cm_rate_shift = AFE_CM1_1X_EN_SEL_FS_SFT;
		cm_update_mask = AFE_CM1_UPDATE_CNT_MASK;
		cm_update_shift = AFE_CM1_UPDATE_CNT_SFT;
		cm_swap_mask = AFE_CM1_BYTE_SWAP_MASK;
		cm_swap_shift = AFE_CM1_BYTE_SWAP_SFT;
		cm_ch_mask = AFE_CM1_CH_NUM_MASK;
		cm_ch_shift = AFE_CM1_CH_NUM_SFT;
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id);
		return 0;
	}
	update_val = (update == 0x1)? calculate_cm_update(rate, (int)ch) : 0x64;
	/* update cnt */
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_update_mask, update_val, cm_update_shift);

	/* rate */
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_rate_mask, rate, cm_rate_shift);

	/* ch num */
	ch = mt6989_convert_cm_ch(ch);
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_ch_mask, ch, cm_ch_shift);

	/* swap */
	mtk_regmap_update_bits(afe->regmap, cm_reg,
			       cm_swap_mask, swap, cm_swap_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mt6989_set_cm);

int mt6989_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en)
{
	int cm_reg = 0;
	int cm_on_mask = 0;
	int cm_on_shift = 0;

	pr_info("%s, CM%d, en %d\n", __func__, id, en);

	switch (id) {
	case CM0:
		cm_reg = AFE_CM0_CON0;
		cm_on_mask = AFE_CM0_BYPASS_MODE_MASK;
		cm_on_shift = AFE_CM0_BYPASS_MODE_SFT;
		break;
	case CM1:
		cm_reg = AFE_CM1_CON0;
		cm_on_mask = AFE_CM1_BYPASS_MODE_MASK;
		cm_on_shift = AFE_CM1_BYPASS_MODE_SFT;
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id);
		return 0;
	}

	mtk_regmap_update_bits(afe->regmap, cm_reg, cm_on_mask,
			       en, cm_on_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mt6989_enable_cm_bypass);



int mt6989_enable_cm(struct mtk_base_afe *afe, int id, bool en)
{
	int cm_reg = 0;
	int cm_on_mask = 0;
	int cm_on_shift = 0;

	switch (id) {
	case CM0:
		cm_reg = AFE_CM1_CON0;
		cm_on_mask = AFE_CM0_ON_MASK;
		cm_on_shift = AFE_CM0_ON_SFT;
		break;
	case CM1:
		cm_reg = AFE_CM1_CON0;
		cm_on_mask = AFE_CM1_ON_MASK;
		cm_on_shift = AFE_CM1_ON_SFT;
		break;
	default:
		dev_info(afe->dev, "%s(), CM%d not found\n",
			 __func__, id);
		return 0;
	}
	mtk_regmap_update_bits(afe->regmap, cm_reg, cm_on_mask,
			       en, cm_on_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mt6989_enable_cm);

int mt6989_is_need_enable_cm(struct mtk_base_afe *afe, int id)
{
	int cm_reg = 0;
	int cm_on_mask = 0;
	int cm_on_shift = 0;
	unsigned int value = 0;

	switch (id) {
	case CM0:
		cm_reg = AFE_CM0_CON0;
		cm_on_mask = AFE_CM0_BYPASS_MODE_MASK_SFT;
		cm_on_shift = AFE_CM0_BYPASS_MODE_SFT;

		regmap_read(afe->regmap, cm_reg, &value);
		value &= cm_on_mask;
		value >>= cm_on_shift;

		pr_info("%s(), CM%d value %d\n", __func__, id, value);
		if (value != 0x1)
			return true;

		break;
	case CM1:
		cm_reg = AFE_CM1_CON0;
		cm_on_mask = AFE_CM1_BYPASS_MODE_MASK_SFT;
		cm_on_shift = AFE_CM1_BYPASS_MODE_SFT;

		regmap_read(afe->regmap, cm_reg, &value);
		value &= cm_on_mask;
		value >>= cm_on_shift;

		pr_info("%s(), CM%d value %d\n", __func__, id, value);
		if (value != 0x1)
			return true;

		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id);
		return 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mt6989_is_need_enable_cm);

MODULE_DESCRIPTION("Mediatek afe cm");
MODULE_AUTHOR("yiwen chiou<yiwen.chiou@mediatek.com>");
MODULE_LICENSE("GPL");

