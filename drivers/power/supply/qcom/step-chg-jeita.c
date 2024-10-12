/* Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "QCOM-STEPCHG: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_batterydata.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/pmic-voter.h>
#include "step-chg-jeita.h"
#include "smb5-lib.h"

#define MAX_STEP_CHG_ENTRIES	8
#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define JEITA_VOTER		"JEITA_VOTER"

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct range_data {
	u32 low_threshold;
	u32 high_threshold;
	u32 value;
};

struct step_chg_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fcc_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fv_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fv_cfg[MAX_STEP_CHG_ENTRIES];
};

struct step_chg_info {
	struct device		*dev;
	ktime_t			step_last_update_time;
	ktime_t			jeita_last_update_time;
	bool			step_chg_enable;
	bool			sw_jeita_enable;
	bool			config_is_read;
	bool			step_chg_cfg_valid;
	bool			sw_jeita_cfg_valid;
	bool			soc_based_step_chg;
	bool			batt_missing;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/03 start */
	bool			hs60_define_enable;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/03 end */
	/* HS50 add for SR-QL3095-01-67 Distinguish HS60, HS70 and HS50 charging by wenyaqi at 2020/08/03 start */
	int			distinguish_hs_projects;
	/* HS50 add for SR-QL3095-01-67 Distinguish HS60, HS70 and HS50 charging by wenyaqi at 2020/08/03 end */
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			step_index;
	int			get_config_retry_count;
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int temp_recharge_threshold_ma;
	#endif
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 end */

	struct step_chg_cfg	*step_chg_config;
	struct jeita_fcc_cfg	*jeita_fcc_config;
	struct jeita_fv_cfg	*jeita_fv_config;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct wakeup_source	*step_chg_ws;
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*main_psy;
	struct power_supply	*usb_psy;
	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;
	struct notifier_block	nb;
};

static struct step_chg_info *the_chip;

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */

#define BATT_HOT_DECIDEGREE_MAX			600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200

static bool is_batt_available(struct step_chg_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_bms_available(struct step_chg_info *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_usb_available(struct step_chg_info *chip)
{
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (!chip->usb_psy)
		return false;

	return true;
}

static int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		u32 max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		pr_err("Count %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	length = rc;
	per_tuple_length = sizeof(struct range_data) / sizeof(u32);
	if (length % per_tuple_length) {
		pr_err("%s length (%d) should be multiple of %d\n",
				prop_str, length, per_tuple_length);
		return -EINVAL;
	}
	tuples = length / per_tuple_length;

	if (tuples > MAX_STEP_CHG_ENTRIES) {
		pr_err("too many entries(%d), only %d allowed\n",
				tuples, MAX_STEP_CHG_ENTRIES);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str,
			(u32 *)ranges, length);
	if (rc) {
		pr_err("Read %s failed, rc=%d", prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges[i].low_threshold >
				ranges[i].high_threshold) {
			pr_err("%s thresholds should be in ascendant ranges\n",
						prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges[i - 1].high_threshold >
					ranges[i].low_threshold) {
				pr_err("%s thresholds should be in ascendant ranges\n",
							prop_str);
				rc = -EINVAL;
				goto clean;
			}
		}

		if (ranges[i].low_threshold > max_threshold)
			ranges[i].low_threshold = max_threshold;
		if (ranges[i].high_threshold > max_threshold)
			ranges[i].high_threshold = max_threshold;
		if (ranges[i].value > max_value)
			ranges[i].value = max_value;
	}

	return rc;
clean:
	memset(ranges, 0, tuples * sizeof(struct range_data));
	return rc;
}

/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 start */
#define FCC_FV_RANGE_HQ	4
/* HS60 add for HS60-5163 New board_id by wangzikang at 2019/12/23 start */
//#define PCB_MASK_HQ		0xF0
#define PCB_MASK_HQ		0xFF0
/* HS60 add for HS60-5163 New board_id by wangzikang at 2019/12/23 end */
#define PCB_SHIFT_HQ		4
enum {
	HQ_PCBA_ROW_ZQL1695 = 1,
	HQ_PCBA_PRC_IN_ID_ZQL1695,
	HQ_PCBA_LATAM_ZQL1695,
	/* HS60 add for HS60-5163 New board_id by wangzikang at 2019/12/23 start */
	HQ_PCBA_ROW_Q_ZQL1695 = 0x14,
	HQ_PCBA_PRC_IN_ID_Q_ZQL1695 ,
	HQ_PCBA_LATAM_Q_ZQL1695 ,
	HQ_PCBA_ROW_4000mAh_ZQL1695 = 0x1D,
	HQ_PCBA_PRC_IN_ID_4000mAh_ZQL1695,
	HQ_PCBA_LATAM_4000mAh_ZQL1695,
	/* HS60 add for HS60-5163 New board_id by wangzikang at 2019/12/23 end */
};
u32 hq_get_huaqin_pcba_config(void);
/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 end */

/* HS50 add for SR-QL3095-01-67 Distinguish HS60, HS70 and HS50 charging by wenyaqi at 2020/08/03 start */
enum {
	DETECT_HS60,
	DETECT_HS70,
	DETECT_HS50,
};
/* HS50 add for SR-QL3095-01-67 Distinguish HS60, HS70 and HS50 charging by wenyaqi at 2020/08/03 end */

/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 start */
extern struct smb_charger *chg_dev;
static int hq_switch_step_jeita_setting(struct step_chg_info *chip)
{
	struct device_node *batt_node, *profile_node;
	u32 max_fcc_ma;
	const __be32 *handle;
	int batt_id_ohms, rc;
	static bool is_first_flag = true;
	static bool is_dcp_old;

	struct range_data fcc_cfg_hs50_dcp[FCC_FV_RANGE_HQ] = {
		{0, 50, 400000},
		{51, 120, 1200000},
		{121, 450, 2000000},
		{451, 500, 2000000},
	};
	union power_supply_propval prop = {0, };

	handle = of_get_property(chip->dev->of_node,
			"qcom,battery-data", NULL);
	if (!handle) {
		pr_debug("ignore getting sw-jeita/step charging settings from profile\n");
		return 0;
	}

	batt_node = of_find_node_by_phandle(be32_to_cpup(handle));
	if (!batt_node) {
		pr_err("Get battery data node failed\n");
		return -EINVAL;
	}

	if (!is_bms_available(chip))
		return -ENODEV;

	power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &prop);
	batt_id_ohms = prop.intval;

	/* bms_psy has not yet read the batt_id */
	if (batt_id_ohms < 0)
		return -EBUSY;

	profile_node = of_batterydata_get_best_profile(batt_node,
					batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("Couldn't find profile\n");
		return -ENODATA;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
					&max_fcc_ma);
	if (rc < 0) {
		pr_err("max-fastchg-current-ma reading failed, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,distinguish-hs-projects",
			&chip->distinguish_hs_projects);
	if (!rc && (chip->distinguish_hs_projects < 0))
	{
		pr_err("qcom,distinguish-hs-projects is incorrect\n");
	}

	if (chip->distinguish_hs_projects == DETECT_HS50)
	{
		if (!chg_dev)
		{
			pr_err("line=%d: chg_dev is null\n", __LINE__);
			return -ENODATA;
		}

		if(is_first_flag)
		{
			is_dcp_old = !chg_dev->is_dcp;
			is_first_flag = false;
		}

		if (chg_dev->is_dcp == is_dcp_old)
		{
			pr_debug("chg_dev->is_dcp = is_dcp_old, do not config again!\n");
			return 1;
		}
		else
		{
			if(chg_dev->is_dcp)
			{
				chip->jeita_fcc_config->fcc_cfg[2].low_threshold = fcc_cfg_hs50_dcp[2].low_threshold;
				chip->jeita_fcc_config->fcc_cfg[2].high_threshold = fcc_cfg_hs50_dcp[2].high_threshold;
				chip->jeita_fcc_config->fcc_cfg[2].value = fcc_cfg_hs50_dcp[2].value;

				chip->jeita_fcc_config->fcc_cfg[3].low_threshold = fcc_cfg_hs50_dcp[3].low_threshold;
				chip->jeita_fcc_config->fcc_cfg[3].high_threshold = fcc_cfg_hs50_dcp[3].high_threshold;
				chip->jeita_fcc_config->fcc_cfg[3].value = fcc_cfg_hs50_dcp[3].value;
			}
			else
			{
				chip->sw_jeita_cfg_valid = true;
				rc = read_range_data_from_node(profile_node,
						"qcom,jeita-fcc-ranges",
						chip->jeita_fcc_config->fcc_cfg,
						BATT_HOT_DECIDEGREE_MAX, max_fcc_ma * 1000);
				if (rc < 0) {
					pr_debug("Read qcom,jeita-fcc-ranges failed from battery profile, rc=%d\n",
								rc);
					chip->sw_jeita_cfg_valid = false;
				}
			}
			is_dcp_old = chg_dev->is_dcp;
		}
		pr_debug("jeita_fcc_config->fcc_cfg[2]=%d,%d,%d,jeita_fcc_config->fcc_cfg[3]=%d,%d,%d\n",
			chip->jeita_fcc_config->fcc_cfg[2].low_threshold, chip->jeita_fcc_config->fcc_cfg[2].high_threshold,
			chip->jeita_fcc_config->fcc_cfg[2].value, chip->jeita_fcc_config->fcc_cfg[3].low_threshold,
			chip->jeita_fcc_config->fcc_cfg[3].high_threshold, chip->jeita_fcc_config->fcc_cfg[3].value);
	}
	return 1;

}
/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 end */

static int get_step_chg_jeita_setting_from_profile(struct step_chg_info *chip)
{
	struct device_node *batt_node, *profile_node;
	u32 max_fv_uv, max_fcc_ma;
	const char *batt_type_str;
	const __be32 *handle;
	int batt_id_ohms, rc;
	/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 start */
	u32 pcba_config = 0;
	struct range_data	fcc_cfg_global[FCC_FV_RANGE_HQ] = {
		{0, 50, 300000},
		{51, 120, 900000},
		/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
		{121, 450, 1200000},
		//Huaqin add for ZQL1693-50 by wangzikang at 2019/09/26 start
		{451, 500, 1200000},
		/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
		//{451, 550, 1000000},
		//Huaqin add for ZQL1693-50 by wangzikang at 2019/09/26 end
	};
	/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 end */
	union power_supply_propval prop = {0, };

	handle = of_get_property(chip->dev->of_node,
			"qcom,battery-data", NULL);
	if (!handle) {
		pr_debug("ignore getting sw-jeita/step charging settings from profile\n");
		return 0;
	}

	batt_node = of_find_node_by_phandle(be32_to_cpup(handle));
	if (!batt_node) {
		pr_err("Get battery data node failed\n");
		return -EINVAL;
	}

	if (!is_bms_available(chip))
		return -ENODEV;

	power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &prop);
	batt_id_ohms = prop.intval;

	/* bms_psy has not yet read the batt_id */
	if (batt_id_ohms < 0)
		return -EBUSY;

	profile_node = of_batterydata_get_best_profile(batt_node,
					batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("Couldn't find profile\n");
		return -ENODATA;
	}

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/03 start */
	chip->hs60_define_enable = of_property_read_bool(profile_node,
						 "qcom,hs60-define-enable");
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/03 end */

	rc = of_property_read_string(profile_node, "qcom,battery-type",
					&batt_type_str);
	if (rc < 0) {
		pr_err("battery type unavailable, rc:%d\n", rc);
		return rc;
	}
	pr_debug("battery: %s detected, getting sw-jeita/step charging settings\n",
					batt_type_str);

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
					&max_fv_uv);
	if (rc < 0) {
		pr_err("max-voltage_uv reading failed, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
					&max_fcc_ma);
	if (rc < 0) {
		pr_err("max-fastchg-current-ma reading failed, rc=%d\n", rc);
		return rc;
	}

	chip->soc_based_step_chg =
		of_property_read_bool(profile_node, "qcom,soc-based-step-chg");
	if (chip->soc_based_step_chg) {
		chip->step_chg_config->psy_prop = POWER_SUPPLY_PROP_CAPACITY,
		chip->step_chg_config->prop_name = "SOC";
		chip->step_chg_config->hysteresis = 0;
	}

	chip->step_chg_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,step-chg-ranges",
			chip->step_chg_config->fcc_cfg,
			chip->soc_based_step_chg ? 100 : max_fv_uv,
			max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,step-chg-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->step_chg_cfg_valid = false;
	}

	chip->sw_jeita_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fcc-ranges",
			chip->jeita_fcc_config->fcc_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,jeita-fcc-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->sw_jeita_cfg_valid = false;
	}

	/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 start */
	pcba_config = hq_get_huaqin_pcba_config();
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/03 start */
	pr_debug("[%s]line=%d: pcba_config=0x%x, match=%d, chip->hs60_define_enable=%d\n",
			__FUNCTION__, __LINE__, pcba_config, ((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ), chip->hs60_define_enable);

	if (chip->hs60_define_enable)
	{
		if ((((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_ZQL1695)
			/* HS60 add for HS60-5163 New board_id by wangzikang at 2019/12/23 start */
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_Q_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_Q_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_Q_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_4000mAh_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_4000mAh_ZQL1695)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_4000mAh_ZQL1695))
			/* HS60 add for HS60-5163 New board_id by wangzikang at 2019/12/23 start */
		{
			chip->jeita_fcc_config->fcc_cfg[2].low_threshold = fcc_cfg_global[2].low_threshold;
			chip->jeita_fcc_config->fcc_cfg[2].high_threshold = fcc_cfg_global[2].high_threshold;
			chip->jeita_fcc_config->fcc_cfg[2].value = fcc_cfg_global[2].value;

			chip->jeita_fcc_config->fcc_cfg[3].low_threshold = fcc_cfg_global[3].low_threshold;
			chip->jeita_fcc_config->fcc_cfg[3].high_threshold = fcc_cfg_global[3].high_threshold;
			chip->jeita_fcc_config->fcc_cfg[3].value = fcc_cfg_global[3].value;
		}
	}
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/03 end */
	/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 end */

	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 start */
	rc = hq_switch_step_jeita_setting(chip);
	if (rc <= 0)
		pr_err("hq_switch_step_jeita_setting fail\n");
	pr_debug("[%s]line=%d: chip->distinguish_hs_projects=%d\n",
		__FUNCTION__, __LINE__, chip->distinguish_hs_projects);
	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 end */

	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fv-ranges",
			chip->jeita_fv_config->fv_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fv_uv);
	if (rc < 0) {
		pr_debug("Read qcom,jeita-fv-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->sw_jeita_cfg_valid = false;
	}

	return rc;
}

static void get_config_work(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, get_config_work.work);
	int i, rc;

	chip->config_is_read = false;
	rc = get_step_chg_jeita_setting_from_profile(chip);

	if (rc < 0) {
		if (rc == -ENODEV || rc == -EBUSY) {
			if (chip->get_config_retry_count++
					< GET_CONFIG_RETRY_COUNT) {
				pr_debug("bms_psy is not ready, retry: %d\n",
						chip->get_config_retry_count);
				goto reschedule;
			}
		}
	}

	chip->config_is_read = true;

	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_debug("step-chg-cfg: %duV(SoC) ~ %duV(SoC), %duA\n",
			chip->step_chg_config->fcc_cfg[i].low_threshold,
			chip->step_chg_config->fcc_cfg[i].high_threshold,
			chip->step_chg_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_debug("jeita-fcc-cfg: %ddecidegree ~ %ddecidegre, %duA\n",
			chip->jeita_fcc_config->fcc_cfg[i].low_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].high_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].value);
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		pr_debug("jeita-fv-cfg: %ddecidegree ~ %ddecidegre, %duV\n",
			chip->jeita_fv_config->fv_cfg[i].low_threshold,
			chip->jeita_fv_config->fv_cfg[i].high_threshold,
			chip->jeita_fv_config->fv_cfg[i].value);

	return;

reschedule:
	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

}

static int get_val(struct range_data *range, int hysteresis, int current_index,
		int threshold,
		int *new_index, int *val)
{
	int i;

	*new_index = -EINVAL;

	/*
	 * If the threshold is lesser than the minimum allowed range,
	 * return -ENODATA.
	 */
	if (threshold < range[0].low_threshold)
		return -ENODATA;

	/* First try to find the matching index without hysteresis */
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++) {
		if (!range[i].high_threshold && !range[i].low_threshold) {
			/* First invalid table entry; exit loop */
			break;
		}

		if (is_between(range[i].low_threshold,
			range[i].high_threshold, threshold)) {
			*new_index = i;
			*val = range[i].value;
			break;
		}
	}

	/*
	 * If nothing was found, the threshold exceeds the max range for sure
	 * as the other case where it is lesser than the min range is handled
	 * at the very beginning of this function. Therefore, clip it to the
	 * max allowed range value, which is the one corresponding to the last
	 * valid entry in the battery profile data array.
	 */
	if (*new_index == -EINVAL) {
		if (i == 0) {
			/* Battery profile data array is completely invalid */
			return -ENODATA;
		}

		*new_index = (i - 1);
		*val = range[*new_index].value;
	}

	/*
	 * If we don't have a current_index return this
	 * newfound value. There is no hysterisis from out of range
	 * to in range transition
	 */
	if (current_index == -EINVAL)
		return 0;

	/*
	 * Check for hysteresis if it in the neighbourhood
	 * of our current index.
	 */
	if (*new_index == current_index + 1) {
		if (threshold < range[*new_index].low_threshold + hysteresis) {
			/*
			 * Stay in the current index, threshold is not higher
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	} else if (*new_index == current_index - 1) {
		if (threshold > range[*new_index].high_threshold - hysteresis) {
			/*
			 * stay in the current index, threshold is not lower
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	}
	return 0;
}

static int handle_step_chg_config(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0;
	u64 elapsed_us;

	elapsed_us = ktime_us_delta(ktime_get(), chip->step_last_update_time);
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		goto reschedule;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED, &pval);
	if (rc < 0)
		chip->step_chg_enable = 0;
	else
		chip->step_chg_enable = pval.intval;

	if (!chip->step_chg_enable || !chip->step_chg_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto update_time;
	}

	rc = power_supply_get_property(chip->batt_psy,
			chip->step_chg_config->psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
			chip->step_chg_config->prop_name, rc);
		return rc;
	}

	rc = get_val(chip->step_chg_config->fcc_cfg,
			chip->step_chg_config->hysteresis,
			chip->step_index,
			pval.intval,
			&chip->step_index,
			&fcc_ua);
	if (rc < 0) {
		/* remove the vote if no step-based fcc is found */
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto update_time;
	}

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		return -EINVAL;

	vote(chip->fcc_votable, STEP_CHG_VOTER, true, fcc_ua);

	pr_debug("%s = %d Step-FCC = %duA\n",
		chip->step_chg_config->prop_name, pval.intval, fcc_ua);

update_time:
	chip->step_last_update_time = ktime_get();
	return 0;

reschedule:
	/* reschedule 1000uS after the remaining time */
	return (STEP_CHG_HYSTERISIS_DELAY_US - elapsed_us + 1000);
}

/* HS60 add for ZQL1693-16 Decrease threshold to decrease recharging time by gaochao at 2019/08/30 start */
//#define JEITA_SUSPEND_HYST_UV		50000
#define JEITA_SUSPEND_HYST_UV		10000
/* HS60 add for ZQL1693-16 Decrease threshold to decrease recharging time by gaochao at 2019/08/30 end */
/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
#define CUSTOM_BAT_HIGH_TEMP_THRESHOLD_DOWM		450
#define CUSTOM_BAT_HIGH_TEMP_THRESHOLD_RESUME		440
#define CUSTOM_BAT_HIGH_TEMP_STOP_CHARGE_VOL		4100000
#define CUSTOM_BAT_HIGH_TEMP_RESUME_CHARGE_VOL		3900000
#define CUSTOM_BAT_RECHARGE_THRESHOLD				4130
/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 start*/
#define CUSTOM_BAT_RECHARGE_THRESHOLD_SDI			4030
/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 end*/
static void handle_high_temp_charge(struct step_chg_info *chip, int fcc_ua)
{
	int rc = 0;
	int battery_temperature = 0;
	int battery_voltage = 0;
	int recharge_threshold = 4330;
	union power_supply_propval pval = {0, };

	/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 start*/
	union power_supply_propval prop = {0, };
	int custom_recharge_threshold = 4330;
	int batt_id_ohms = 200000;

	rc = power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &prop);
	if (rc == 0)
	{
		batt_id_ohms = prop.intval;
	}
	else
	{
		pr_err("Cannot get Batt_ID! rc = %d. \n",rc);
	}

	/* HS50 add for SR-QL3095-01-67 Distinguish HS60, HS70 and HS50 charging by wenyaqi at 2020/08/03 start */
	if (chip->distinguish_hs_projects == DETECT_HS50)
	{
		custom_recharge_threshold = CUSTOM_BAT_RECHARGE_THRESHOLD;
		pr_debug("Threshold  =  %d.\n", custom_recharge_threshold);
	}
	else
	{
		if ((!chip->hs60_define_enable) && (batt_id_ohms > 170000))
		{
			custom_recharge_threshold = CUSTOM_BAT_RECHARGE_THRESHOLD_SDI;
			pr_debug("SDI Threshold  =  %d.\n", custom_recharge_threshold);
		}
		else
		{
			custom_recharge_threshold = CUSTOM_BAT_RECHARGE_THRESHOLD;
			pr_debug("Threshold  =  %d.\n", custom_recharge_threshold);
		}
	}
	/* HS50 add for SR-QL3095-01-67 Distinguish HS60, HS70 and HS50 charging by wenyaqi at 2020/08/03 end */
	/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 end*/

	if (chip)
	{
		rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		if (rc < 0)
		{
			pr_err("Failed to get battery temperature, rc=%d\n", rc);
		}
		battery_temperature = pval.intval;

		rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0)
		{
			pr_err("Failed to get battery voltage, rc=%d\n", rc);
		}
		battery_voltage = pval.intval;

		rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_RECHARGE_VBAT, &pval);
		if (rc < 0) {
				pr_err("Failed to get recharge voltage property on batt_psy, rc=%d\n" , rc);
				pval.intval = 4330;
		}
		recharge_threshold = pval.intval;

		if (battery_temperature >= CUSTOM_BAT_HIGH_TEMP_THRESHOLD_DOWM)
		{
			pr_debug("[%s]line=%d, Come into 45~50 degrees Celsius! The threshold is %d mV\n",
				__FUNCTION__, __LINE__, recharge_threshold);
			/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 start*/
			if (recharge_threshold != custom_recharge_threshold)			//set recharge voltage once
			/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 end*/
			{
				chip->temp_recharge_threshold_ma = recharge_threshold;		//restore recharge voltage
				/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 start*/
				pval.intval = custom_recharge_threshold;					//set recharge voltage
				/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 end*/

				pr_debug("[%s]line=%d, Set recharge_threshold to %d mV\n",
					__FUNCTION__, __LINE__, pval.intval);

				rc = power_supply_set_property(chip->batt_psy, POWER_SUPPLY_PROP_RECHARGE_VBAT, &pval);
				if (rc < 0)
				{
					pr_err("Failed to set recharge voltage property on batt_psy, rc=%d\n" , rc);
				}
			}

		}
		else if (battery_temperature < CUSTOM_BAT_HIGH_TEMP_THRESHOLD_RESUME)
		{
			pr_debug("[%s]line=%d, Back to normal temperature, temp_recharge_threshold_ma=%d mV\n",
				__FUNCTION__, __LINE__, chip->temp_recharge_threshold_ma);
			/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 start*/
			if (recharge_threshold == custom_recharge_threshold)			//recover recharge voltage
			/*HS70 add for HS70-5059 Optimize the charging experience of SDI at temperature 45~50 by wangzikang at 2020/03/31 end*/
			{
				pval.intval = chip->temp_recharge_threshold_ma;
				pr_info("[%s]line=%d, Set back the recharge threshold, temp_recharge_threshold_ma=%d mV\n",
					__FUNCTION__, __LINE__, chip->temp_recharge_threshold_ma);

				rc = power_supply_set_property(chip->batt_psy, POWER_SUPPLY_PROP_RECHARGE_VBAT, &pval);
				if (rc < 0)
				{
					pr_err("Failed to set recharge voltage property on batt_psy, rc=%d\n" , rc);
				}
			}
		}
		else
		{
			pr_debug("[%s]line=%d, Keep recharge status, recharge_threshold=%d mV\n",
				__FUNCTION__, __LINE__, recharge_threshold);
		}

		pr_debug("[%s]line=%d, fcc_ua=%d, bat_temp=%d, bat_vol=%d, hs60_define_enable=%d, recharge_threshold=%d mV, temp_recharge_threshold_ma=%d mV\n",
				__FUNCTION__, __LINE__, fcc_ua, battery_temperature, battery_voltage, chip->hs60_define_enable, recharge_threshold, chip->temp_recharge_threshold_ma);
	}
	else
	{
		pr_err("[%s]line=%d: chip is null\n", __FUNCTION__, __LINE__);
	}
}
#endif
/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 end */

static int handle_jeita(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, fv_uv = 0;
	u64 elapsed_us;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_SW_JEITA_ENABLED, &pval);
	if (rc < 0)
		chip->sw_jeita_enable = 0;
	else
		chip->sw_jeita_enable = pval.intval;

	if (!chip->sw_jeita_enable || !chip->sw_jeita_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		if (chip->usb_icl_votable)
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		return 0;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->jeita_last_update_time);
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		goto reschedule;

	rc = power_supply_get_property(chip->batt_psy,
			chip->jeita_fcc_config->psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				chip->jeita_fcc_config->prop_name, rc);
		return rc;
	}

	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/07/31 start */
	rc = hq_switch_step_jeita_setting(chip);
	if (rc <= 0)
		pr_err("hq_switch_step_jeita_setting fail\n");
	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/07/31 end */

	rc = get_val(chip->jeita_fcc_config->fcc_cfg,
			chip->jeita_fcc_config->hysteresis,
			chip->jeita_fcc_index,
			pval.intval,
			&chip->jeita_fcc_index,
			&fcc_ua);
	if (rc < 0)
		fcc_ua = 0;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	vote(chip->fcc_votable, JEITA_VOTER, fcc_ua ? true : false, fcc_ua);

	rc = get_val(chip->jeita_fv_config->fv_cfg,
			chip->jeita_fv_config->hysteresis,
			chip->jeita_fv_index,
			pval.intval,
			&chip->jeita_fv_index,
			&fv_uv);
	if (rc < 0)
		fv_uv = 0;

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto update_time;

	if (!chip->usb_icl_votable)
		chip->usb_icl_votable = find_votable("USB_ICL");

	if (!chip->usb_icl_votable)
		goto set_jeita_fv;

	/*
	 * If JEITA float voltage is same as max-vfloat of battery then
	 * skip any further VBAT specific checks.
	 */
	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
	if (rc || (pval.intval == fv_uv)) {
		vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		goto set_jeita_fv;
	}

	/*
	 * Suspend USB input path if battery voltage is above
	 * JEITA VFLOAT threshold.
	 */
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	 /*
	if (fv_uv > 0) {
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!rc && (pval.intval > fv_uv))
			vote(chip->usb_icl_votable, JEITA_VOTER, true, 0);
		else if (pval.intval < (fv_uv - JEITA_SUSPEND_HYST_UV))
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
	}
	*/
	#else
	if (fv_uv > 0) {
		rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!rc && (pval.intval > fv_uv))
			vote(chip->usb_icl_votable, JEITA_VOTER, true, 0);
		else if (pval.intval < (fv_uv - JEITA_SUSPEND_HYST_UV))
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
	}
	#endif
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 end */

set_jeita_fv:
	vote(chip->fv_votable, JEITA_VOTER, fv_uv ? true : false, fv_uv);

	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	handle_high_temp_charge(chip, fcc_ua);
	#endif
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 end */

update_time:
	chip->jeita_last_update_time = ktime_get();

	if (!chip->main_psy)
		chip->main_psy = power_supply_get_by_name("main");
	if (chip->main_psy)
		power_supply_changed(chip->main_psy);

	return 0;

reschedule:
	/* reschedule 1000uS after the remaining time */
	return (STEP_CHG_HYSTERISIS_DELAY_US - elapsed_us + 1000);
}

static int handle_battery_insertion(struct step_chg_info *chip)
{
	int rc;
	union power_supply_propval pval = {0, };

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0) {
		pr_err("Get battery present status failed, rc=%d\n", rc);
		return rc;
	}

	if (chip->batt_missing != (!pval.intval)) {
		chip->batt_missing = !pval.intval;
		pr_debug("battery %s detected\n",
				chip->batt_missing ? "removal" : "insertion");
		if (chip->batt_missing) {
			chip->step_chg_cfg_valid = false;
			chip->sw_jeita_cfg_valid = false;
			chip->get_config_retry_count = 0;
		} else {
			/*
			 * Get config for the new inserted battery, delay
			 * to make sure BMS has read out the batt_id.
			 */
			schedule_delayed_work(&chip->get_config_work,
				msecs_to_jiffies(WAIT_BATT_ID_READY_MS));
		}
	}

	return rc;
}

/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
extern struct smb_charger *smbchg_dev;

#define BAT_TEMPERATURE_NEGATIVE_0	0
#define BAT_TEMPERATURE_POSITIVE_3		30
#define BAT_TEMPERATURE_POSITIVE_47	470
#define BAT_TEMPERATURE_POSITIVE_50	500

#define CUSTOM_JEITA_HARD		1

#define JEITA_HARD_THRESHOLDS_LEN		2

#define CUSTOM_CONFIG_ONCE_STOP_CHARGING		0
#define CUSTOM_CONFIG_ONCE_RESUME_CHARGING		1

int smblib_update_jeita(struct smb_charger *chg, u32 *thresholds, int type);
static void set_hard_jeita_dynamically(struct step_chg_info *chip)
{
	int rc = 0;
	static int config_once = CUSTOM_CONFIG_ONCE_STOP_CHARGING;
	union power_supply_propval bat_temperature = {0, };
	//union power_supply_propval bat_charging_status = {0, };
	union power_supply_propval bat_health_status = {0, };
	u32 jeita_hard_thresholds_2_50_degree[JEITA_HARD_THRESHOLDS_LEN] = {0x32F2, 0xDBF};		// 3    50
	u32 jeita_hard_thresholds_neg_1_50_degree[JEITA_HARD_THRESHOLDS_LEN] = {0x36B9, 0xDBF};	// 0    50
	u32 jeita_hard_thresholds_neg_1_47_degree[JEITA_HARD_THRESHOLDS_LEN] = {0x36B9, 0xEF9};	// 0    47

	if (!chip || !smbchg_dev)
	{
		pr_err("line=%d: chip is null\n", __LINE__);
		return;
	}

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &bat_temperature);
	if (rc < 0)
	{
		pr_err("Get battery teperature status failed, rc=%d\n", rc);
	}

	/*
	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &bat_charging_status);
	if (rc < 0)
	{
		pr_err("Get battery charging status failed, rc=%d\n", rc);
	}
	*/

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_HEALTH, &bat_health_status);
	if (rc < 0)
	{
		pr_err("Get battery health status failed, rc=%d\n", rc);
	}

	pr_debug("[%s]line=%d: bat_temperature=%d, bat_health_status=%d, config_once=%d\n",
			__FUNCTION__, __LINE__, bat_temperature.intval, bat_health_status.intval, config_once);

	if ((bat_temperature.intval <= BAT_TEMPERATURE_NEGATIVE_0) /* && (bat_charging_status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) */
			&& (bat_health_status.intval == POWER_SUPPLY_HEALTH_COLD))
	{
		/* stop charging when battery temperature below 0 degree */
		if (config_once == CUSTOM_CONFIG_ONCE_STOP_CHARGING)
		{
			pr_debug("[%s]line=%d: set hard jeita[3, 50]\n", __FUNCTION__, __LINE__);
			rc = smblib_update_jeita(smbchg_dev, jeita_hard_thresholds_2_50_degree, CUSTOM_JEITA_HARD);			// 3   50
			if (rc < 0)
			{
				pr_err("line=%d: Couldn't configure Jeita cold threshold rc=%d\n", __LINE__, rc);
			}

			config_once = CUSTOM_CONFIG_ONCE_RESUME_CHARGING;
		}

	}
	else if ((bat_temperature.intval >= BAT_TEMPERATURE_POSITIVE_3) /* && (bat_charging_status.intval == POWER_SUPPLY_STATUS_CHARGING) */
			&& (bat_health_status.intval == POWER_SUPPLY_HEALTH_COOL))
	{
		/* resume charging when battery temperature above 3 degree */
		if (config_once == CUSTOM_CONFIG_ONCE_RESUME_CHARGING)
		{
			pr_debug("[%s]line=%d: set hard jeita[0, 50]\n", __FUNCTION__, __LINE__);
			rc = smblib_update_jeita(smbchg_dev, jeita_hard_thresholds_neg_1_50_degree, CUSTOM_JEITA_HARD);		// 0    50
			if (rc < 0)
			{
				pr_err("line=%d: Couldn't configure Jeita cold threshold rc=%d\n", __LINE__, rc);
			}

			config_once = CUSTOM_CONFIG_ONCE_STOP_CHARGING;
		}

	}
	else if ((bat_temperature.intval >= BAT_TEMPERATURE_POSITIVE_50) /* && (bat_charging_status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) */
			&& ((bat_health_status.intval == POWER_SUPPLY_HEALTH_OVERHEAT) || (bat_health_status.intval == POWER_SUPPLY_HEALTH_HOT)))
	{
		/* stop charging when battery temperature above 50 degree */
		if (config_once == CUSTOM_CONFIG_ONCE_STOP_CHARGING)
		{
			pr_debug("[%s]line=%d: set hard jeita[0, 47]\n", __FUNCTION__, __LINE__);
			rc = smblib_update_jeita(smbchg_dev, jeita_hard_thresholds_neg_1_47_degree, CUSTOM_JEITA_HARD);		// 0    47
			if (rc < 0)
			{
				pr_err("line=%d: Couldn't configure Jeita cold threshold rc=%d\n", __LINE__, rc);
			}

			config_once = CUSTOM_CONFIG_ONCE_RESUME_CHARGING;
		}

	}
	else if ((bat_temperature.intval <= BAT_TEMPERATURE_POSITIVE_47) /* && (bat_charging_status.intval == POWER_SUPPLY_STATUS_CHARGING) */
			&& (bat_health_status.intval == POWER_SUPPLY_HEALTH_WARM))
	{
		/* resume charging when battery temperature below 47 degree */
		if (config_once == CUSTOM_CONFIG_ONCE_RESUME_CHARGING)
		{
			pr_debug("[%s]line=%d: set hard jeita[0, 50]\n", __FUNCTION__, __LINE__);
			rc = smblib_update_jeita(smbchg_dev, jeita_hard_thresholds_neg_1_50_degree, CUSTOM_JEITA_HARD);		// 0    50
			if (rc < 0)
			{
				pr_err("line=%d: Couldn't configure Jeita cold threshold rc=%d\n", __LINE__, rc);
			}

			config_once = CUSTOM_CONFIG_ONCE_STOP_CHARGING;
		}

	}
	else
	{
		pr_debug("[%s]line=%d: set nothing\n", __FUNCTION__, __LINE__);
	}
}
#endif
/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 end */

static void status_change_work(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, status_change_work.work);
	int rc = 0;
	int reschedule_us;
	int reschedule_jeita_work_us = 0;
	int reschedule_step_work_us = 0;
	union power_supply_propval prop = {0, };

	if (!is_batt_available(chip))
		goto exit_work;

	handle_battery_insertion(chip);

	/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	set_hard_jeita_dynamically(chip);
	#endif
	/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 end */

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc > 0)
		reschedule_jeita_work_us = rc;
	else if (rc < 0)
		pr_err("Couldn't handle sw jeita rc = %d\n", rc);

	rc = handle_step_chg_config(chip);
	if (rc > 0)
		reschedule_step_work_us = rc;
	if (rc < 0)
		pr_err("Couldn't handle step rc = %d\n", rc);

	/* Remove stale votes on USB removal */
	if (is_usb_available(chip)) {
		prop.intval = 0;
		power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &prop);
		if (!prop.intval) {
			if (chip->usb_icl_votable)
				vote(chip->usb_icl_votable, JEITA_VOTER,
						false, 0);
		}
	}

	reschedule_us = min(reschedule_jeita_work_us, reschedule_step_work_us);
	if (reschedule_us == 0)
		goto exit_work;
	else
		schedule_delayed_work(&chip->status_change_work,
				usecs_to_jiffies(reschedule_us));
	return;

exit_work:
	__pm_relax(chip->step_chg_ws);
}

static int step_chg_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct step_chg_info *chip = container_of(nb, struct step_chg_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
			|| (strcmp(psy->desc->name, "usb") == 0)) {
		__pm_stay_awake(chip->step_chg_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	if ((strcmp(psy->desc->name, "bms") == 0)) {
		if (chip->bms_psy == NULL)
			chip->bms_psy = psy;
		if (!chip->config_is_read)
			schedule_delayed_work(&chip->get_config_work, 0);
	}

	return NOTIFY_OK;
}

static int step_chg_register_notifier(struct step_chg_info *chip)
{
	int rc;

	chip->nb.notifier_call = step_chg_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

int qcom_step_chg_init(struct device *dev,
		bool step_chg_enable, bool sw_jeita_enable)
{
	int rc;
	struct step_chg_info *chip;

	if (the_chip) {
		pr_err("Already initialized\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->step_chg_ws = wakeup_source_register("qcom-step-chg");
	if (!chip->step_chg_ws)
		return -EINVAL;

	chip->dev = dev;
	chip->step_chg_enable = step_chg_enable;
	chip->sw_jeita_enable = sw_jeita_enable;
	chip->step_index = -EINVAL;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	chip->temp_recharge_threshold_ma = 4330;
	#endif
	/* HS60 add for ZQL1693-56 Optimize the charging experience at temperature 45~50 by gaochao at 2019/11/14 end */

	chip->step_chg_config = devm_kzalloc(dev,
			sizeof(struct step_chg_cfg), GFP_KERNEL);
	if (!chip->step_chg_config)
		return -ENOMEM;

	chip->step_chg_config->psy_prop = POWER_SUPPLY_PROP_VOLTAGE_NOW;
	chip->step_chg_config->prop_name = "VBATT";
	chip->step_chg_config->hysteresis = 100000;

	chip->jeita_fcc_config = devm_kzalloc(dev,
			sizeof(struct jeita_fcc_cfg), GFP_KERNEL);
	chip->jeita_fv_config = devm_kzalloc(dev,
			sizeof(struct jeita_fv_cfg), GFP_KERNEL);
	if (!chip->jeita_fcc_config || !chip->jeita_fv_config)
		return -ENOMEM;

	chip->jeita_fcc_config->psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fcc_config->prop_name = "BATT_TEMP";
	chip->jeita_fcc_config->hysteresis = 10;
	chip->jeita_fv_config->psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fv_config->prop_name = "BATT_TEMP";
	chip->jeita_fv_config->hysteresis = 10;

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->get_config_work, get_config_work);

	rc = step_chg_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

	the_chip = chip;

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->step_chg_ws);
	return rc;
}

void qcom_step_chg_deinit(void)
{
	struct step_chg_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	cancel_delayed_work_sync(&chip->get_config_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->step_chg_ws);
	the_chip = NULL;
}
