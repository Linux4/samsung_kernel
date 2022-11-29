/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__

#define MAX_STEP_CHG_ENTRIES	8

struct step_chg_jeita_param {
	u32			psy_prop;
	char			*prop_name;
	int			rise_hys;
	int			fall_hys;
	bool			use_bms;
};

struct range_data {
	int low_threshold;
	int high_threshold;
	u32 value;
};

int qcom_step_chg_init(struct device *dev,
		bool step_chg_enable, bool sw_jeita_enable, bool jeita_arb_en);
void qcom_step_chg_deinit(void);
#endif /* __STEP_CHG_H__ */
