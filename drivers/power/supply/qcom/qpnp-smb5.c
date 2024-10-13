/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/pmic-voter.h>
#include <linux/qpnp/qpnp-adc.h>
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_AFC)
#include <linux/afc/afc.h>
#endif
#endif
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
#if defined(CONFIG_TYPEC)
#include <linux/usb/typec/pdic_core.h>
#endif
#include "smb5-reg.h"
#include "smb5-lib.h"
#include "schgm-flash.h"

static struct smb_params smb5_pmi632_params = {
	.fcc			= {
		.name   = "fast charge current",
		.reg    = CHGR_FAST_CHARGE_CURRENT_CFG_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.fv			= {
		.name   = "float voltage",
		.reg    = CHGR_FLOAT_VOLTAGE_CFG_REG,
		.min_u  = 3600000,
		.max_u  = 4800000,
		.step_u = 10000,
	},
	.usb_icl		= {
		.name   = "usb input current limit",
		.reg    = USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.icl_max_stat		= {
		.name   = "dcdc icl max status",
		.reg    = ICL_MAX_STATUS_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.icl_stat		= {
		.name   = "input current limit status",
		.reg    = ICL_STATUS_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.otg_cl			= {
		.name	= "usb otg current limit",
		.reg	= DCDC_OTG_CURRENT_LIMIT_CFG_REG,
		.min_u	= 500000,
		.max_u	= 1000000,
		.step_u	= 250000,
	},
	.jeita_cc_comp_hot	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_HOT_REG,
		.min_u	= 0,
		.max_u	= 1575000,
		.step_u	= 25000,
	},
	.jeita_cc_comp_cold	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_COLD_REG,
		.min_u	= 0,
		.max_u	= 1575000,
		.step_u	= 25000,
	},
	.freq_switcher		= {
		.name	= "switching frequency",
		.reg	= DCDC_FSW_SEL_REG,
		.min_u	= 600,
		.max_u	= 1200,
		.step_u	= 400,
		.set_proc = smblib_set_chg_freq,
	},
	.aicl_5v_threshold		= {
		.name   = "AICL 5V threshold",
		.reg    = USBIN_5V_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 4700,
		.step_u = 100,
	},
	.aicl_cont_threshold		= {
		.name   = "AICL CONT threshold",
		.reg    = USBIN_CONT_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 8800,
		.step_u = 100,
		.get_proc = smblib_get_aicl_cont_threshold,
		.set_proc = smblib_set_aicl_cont_threshold,
	},
};

static struct smb_params smb5_pm855b_params = {
	.fcc			= {
		.name   = "fast charge current",
		.reg    = CHGR_FAST_CHARGE_CURRENT_CFG_REG,
		.min_u  = 0,
		.max_u  = 8000000,
		.step_u = 25000,
	},
	.fv			= {
		.name   = "float voltage",
		.reg    = CHGR_FLOAT_VOLTAGE_CFG_REG,
		.min_u  = 3600000,
		.max_u  = 4790000,
		.step_u = 10000,
	},
	.usb_icl		= {
		.name   = "usb input current limit",
		.reg    = USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u  = 0,
		.max_u  = 5000000,
		.step_u = 50000,
	},
	.icl_max_stat		= {
		.name   = "dcdc icl max status",
		.reg    = ICL_MAX_STATUS_REG,
		.min_u  = 0,
		.max_u  = 5000000,
		.step_u = 50000,
	},
	.icl_stat		= {
		.name   = "aicl icl status",
		.reg    = AICL_ICL_STATUS_REG,
		.min_u  = 0,
		.max_u  = 5000000,
		.step_u = 50000,
	},
	.otg_cl			= {
		.name	= "usb otg current limit",
		.reg	= DCDC_OTG_CURRENT_LIMIT_CFG_REG,
		.min_u	= 500000,
		.max_u	= 3000000,
		.step_u	= 500000,
	},
	.jeita_cc_comp_hot	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_HOT_REG,
		.min_u	= 0,
		.max_u	= 8000000,
		.step_u	= 25000,
		.set_proc = NULL,
	},
	.jeita_cc_comp_cold	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_COLD_REG,
		.min_u	= 0,
		.max_u	= 8000000,
		.step_u	= 25000,
		.set_proc = NULL,
	},
	.freq_switcher		= {
		.name	= "switching frequency",
		.reg	= DCDC_FSW_SEL_REG,
		.min_u	= 1200,
		.max_u	= 2400,
		.step_u	= 400,
		.set_proc = NULL,
	},
	.aicl_5v_threshold		= {
		.name   = "AICL 5V threshold",
		.reg    = USBIN_5V_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 4700,
		.step_u = 100,
	},
	.aicl_cont_threshold		= {
		.name   = "AICL CONT threshold",
		.reg    = USBIN_CONT_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 1180,
		.step_u = 100,
		.get_proc = smblib_get_aicl_cont_threshold,
		.set_proc = smblib_set_aicl_cont_threshold,
	},
};

struct smb_dt_props {
	int			usb_icl_ua;
	struct device_node	*revid_dev_node;
	enum float_options	float_option;
	int			chg_inhibit_thr_mv;
	bool			no_battery;
	bool			hvdcp_disable;
	int			auto_recharge_soc;
	int			auto_recharge_vbat_mv;
	int			wd_bark_time;
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;
	int			term_current_src;
	int			term_current_thresh_hi_ma;
	int			term_current_thresh_lo_ma;
};

struct smb5 {
	struct smb_charger	chg;
	struct dentry		*dfs_root;
	struct smb_dt_props	dt;
};

/* HS60 add for HS60-542 change debug_mask by wangzikang at 2019/09/16 start */
/* HS60 add for HS60-2939 change debug_mask by wangzikang at 2019/10/23 start */
//static int __debug_mask;
static int __debug_mask = PR_MISC;
/* HS60 add for HS60-2939 change debug_mask by wangzikang at 2019/10/23 end */
/* HS60 add for HS60-542 change debug_mask by wangzikang at 2019/09/16 end */
module_param_named(
	debug_mask, __debug_mask, int, 0600
);

static int __pd_disabled;
module_param_named(
	pd_disabled, __pd_disabled, int, 0600
);

static int __weak_chg_icl_ua = 500000;
module_param_named(
	weak_chg_icl_ua, __weak_chg_icl_ua, int, 0600
);

enum {
	USBIN_CURRENT,
	USBIN_VOLTAGE,
};

enum {
	BAT_THERM = 0,
	MISC_THERM,
	CONN_THERM,
	SMB_THERM,
};

#define PMI632_MAX_ICL_UA	3000000
static int smb5_chg_config_init(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct pmic_revid_data *pmic_rev_id;
	struct device_node *revid_dev_node;
	int rc = 0;

	revid_dev_node = of_parse_phandle(chip->chg.dev->of_node,
					  "qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR_OR_NULL(pmic_rev_id)) {
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		rc =  -EPROBE_DEFER;
		goto out;
	}

	switch (pmic_rev_id->pmic_subtype) {
	case PM855B_SUBTYPE:
		chip->chg.smb_version = PM855B_SUBTYPE;
		chg->param = smb5_pm855b_params;
		chg->name = "pm855b_charger";
		break;
	case PMI632_SUBTYPE:
		chip->chg.smb_version = PMI632_SUBTYPE;
		chg->wa_flags |= WEAK_ADAPTER_WA | USBIN_OV_WA |
					CHG_TERMINATION_WA;
		if (pmic_rev_id->rev4 >= 2)
			chg->wa_flags |= MOISTURE_PROTECTION_WA;
		chg->param = smb5_pmi632_params;
		chg->use_extcon = true;
		chg->name = "pmi632_charger";
		/* PMI632 does not support PD */
		chg->pd_not_supported = true;
		chg->hw_max_icl_ua =
			(chip->dt.usb_icl_ua > 0) ? chip->dt.usb_icl_ua
						: PMI632_MAX_ICL_UA;
		chg->chg_freq.freq_5V			= 600;
		chg->chg_freq.freq_6V_8V		= 800;
		chg->chg_freq.freq_9V			= 1050;
		chg->chg_freq.freq_removal		= 1050;
		chg->chg_freq.freq_below_otg_threshold	= 800;
		chg->chg_freq.freq_above_otg_threshold	= 800;
		break;
	default:
		pr_err("PMIC subtype %d not supported\n",
				pmic_rev_id->pmic_subtype);
		rc = -EINVAL;
	}

out:
	of_node_put(revid_dev_node);
	return rc;
}

#define PULL_NO_PULL	0
#define PULL_30K	30
#define PULL_100K	100
#define PULL_400K	400
static int get_valid_pullup(int pull_up)
{
	int pull;

	/* pull up can only be 0/30K/100K/400K) */
	switch (pull_up) {
	case PULL_NO_PULL:
		pull = INTERNAL_PULL_NO_PULL;
		break;
	case PULL_30K:
		pull = INTERNAL_PULL_30K_PULL;
		break;
	case PULL_100K:
		pull = INTERNAL_PULL_100K_PULL;
		break;
	case PULL_400K:
		pull = INTERNAL_PULL_400K_PULL;
		break;
	default:
		pull = INTERNAL_PULL_100K_PULL;
	}

	return pull;
}

#define INTERNAL_PULL_UP_MASK	0x3
static int smb5_configure_internal_pull(struct smb_charger *chg, int type,
					int pull)
{
	int rc;
	int shift = type * 2;
	u8 mask = INTERNAL_PULL_UP_MASK << shift;
	u8 val = pull << shift;

	rc = smblib_masked_write(chg, BATIF_ADC_INTERNAL_PULL_UP_REG,
				mask, val);
	if (rc < 0)
		dev_err(chg->dev,
			"Couldn't configure ADC pull-up reg rc=%d\n", rc);

	return rc;
}

/* Huaqin add for P200731-01593 Enable charging while TYPE-C is default mode by gaochao at 2020/08/10 start */
enum HS70_NA {
	HS70_HQ_PCBA_AT_T = 0x5,
	HS70_HQ_PCBA_Canada = 0x8,
};
/* Huaqin add for P200731-01593 Enable charging while TYPE-C is default mode by gaochao at 2020/08/10 end */


/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
enum {
	HQ_PCBA_ROW_ZQL1695_SMB = 1,
	HQ_PCBA_PRC_IN_ID_ZQL1695_SMB,
	HQ_PCBA_LATAM_ZQL1695_SMB,
	HQ_PCBA_ROW_Q_ZQL1695_SMB = 0x14,
	HQ_PCBA_PRC_IN_ID_Q_ZQL1695_SMB ,
	HQ_PCBA_LATAM_Q_ZQL1695_SMB ,
	HQ_PCBA_ROW_4000mAh_ZQL1695_SMB = 0x1D,
	HQ_PCBA_PRC_IN_ID_4000mAh_ZQL1695_SMB,
	HQ_PCBA_LATAM_4000mAh_ZQL1695_SMB,
};
#define PCB_MASK_HQ		0xFF0
#define PCB_SHIFT_HQ		4
u32 hq_get_huaqin_pcba_config(void);
/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
#define MICRO_1P5A		1500000
#define MICRO_1PA		1000000
#define MICRO_P1A		100000
#define OTG_DEFAULT_DEGLITCH_TIME_MS	50
#define MIN_WD_BARK_TIME		16
#define DEFAULT_WD_BARK_TIME		64
#define BITE_WDOG_TIMEOUT_8S		0x3
#define BARK_WDOG_TIMEOUT_MASK		GENMASK(3, 2)
#define BARK_WDOG_TIMEOUT_SHIFT		2
static int smb5_parse_dt(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	int rc, byte_len;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
	int index = 0;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */

	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
	u32 pcba_config = 0;
	pcba_config = hq_get_huaqin_pcba_config();
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	chg->step_chg_enabled = of_property_read_bool(node,
				"qcom,step-charging-enable");

	chg->sw_jeita_enabled = of_property_read_bool(node,
				"qcom,sw-jeita-enable");

	rc = of_property_read_u32(node, "qcom,wd-bark-time-secs",
					&chip->dt.wd_bark_time);
	if (rc < 0 || chip->dt.wd_bark_time < MIN_WD_BARK_TIME)
		chip->dt.wd_bark_time = DEFAULT_WD_BARK_TIME;

	chip->dt.no_battery = of_property_read_bool(node,
						"qcom,batteryless-platform");

	rc = of_property_read_u32(node,
			"qcom,fcc-max-ua", &chip->dt.batt_profile_fcc_ua);
	if (rc < 0)
		chip->dt.batt_profile_fcc_ua = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,fv-max-uv", &chip->dt.batt_profile_fv_uv);
	if (rc < 0)
		chip->dt.batt_profile_fv_uv = -EINVAL;
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
	/*rc = of_property_read_u32(node,
				"qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
	if (rc < 0)
		chip->dt.usb_icl_ua = -EINVAL;
	*/
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
	rc = of_property_read_u32(node,
				"qcom,otg-cl-ua", &chg->otg_cl_ua);
	if (rc < 0)
		chg->otg_cl_ua = (chip->chg.smb_version == PMI632_SUBTYPE) ?
							MICRO_1PA : MICRO_1P5A;

	rc = of_property_read_u32(node, "qcom,chg-term-src",
			&chip->dt.term_current_src);
	if (rc < 0)
		chip->dt.term_current_src = ITERM_SRC_UNSPECIFIED;
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
	if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM439_PLATFORM &&
		((((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_4000mAh_ZQL1695_SMB)
		|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_4000mAh_ZQL1695_SMB)
		|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_4000mAh_ZQL1695_SMB)))
	{
		chip->dt.term_current_thresh_hi_ma = -180; //HS60 4000mAh
	}
	else
	{
		rc = of_property_read_u32(node, "qcom,chg-term-current-ma",
			&chip->dt.term_current_thresh_hi_ma);
	}
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
	if (chip->dt.term_current_src == ITERM_SRC_ADC)
		rc = of_property_read_u32(node, "qcom,chg-term-base-current-ma",
				&chip->dt.term_current_thresh_lo_ma);

	if (of_find_property(node, "qcom,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL)
			return -ENOMEM;

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}

		/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
		pr_debug("[%s]line=%d: thermal_levels=%d, distinguish_sdm439_sdm450_others=%d\n",
				__FUNCTION__, __LINE__, chg->thermal_levels, chg->distinguish_sdm439_sdm450_others);

		for (index = 0; index < chg->thermal_levels; index++)
		{
			pr_debug("[%s]line=%d: [before set] thermal_mitigation[%d]=%d\n",
				__FUNCTION__, __LINE__, index, chg->thermal_mitigation[index]);
		}

		if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_PLATFORM ||
			chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_HS50)  //HS70 or HS50
		{
			//HS70 thermal cooling device battery, should chg->thermal_mitigation[index] = setting value;
			//must first distinguish NA and Global by board id, pcba_config = hq_get_huaqin_pcba_config();

			for (index = 0; index < chg->thermal_levels; index++)
			{
				pr_debug("[%s]line=%d: [after set HS70] thermal_mitigation[%d]=%d\n",
					__FUNCTION__, __LINE__, index, chg->thermal_mitigation[index]);
			}
		}
		/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */
	}


	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	if (of_find_property(node, "qcom,typec-interface-protection", &byte_len)) {
		chg->typec_interface_protection = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->typec_interface_protection == NULL)
			return -ENOMEM;

		chg->typec_interface_protection_length = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,typec-interface-protection",
				chg->typec_interface_protection,
				chg->typec_interface_protection_length);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}

		pr_debug("[%s]line=%d: typec_interface_protection_length=%d\n",
				__FUNCTION__, __LINE__, chg->typec_interface_protection_length);

		for (index = 0; index < chg->typec_interface_protection_length; index++)
		{
			pr_debug("[%s]line=%d: typec_interface_protection[%d]=0x%x\n",
				__FUNCTION__, __LINE__, index, chg->typec_interface_protection[index]);
		}
	}
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	if (of_find_property(node, "qcom,usb-connector-thermal", &byte_len)) {
		chg->usb_connector_thermal = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->usb_connector_thermal == NULL)
			return -ENOMEM;

		chg->usb_connector_thermal_length = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,usb-connector-thermal",
				chg->usb_connector_thermal,
				chg->usb_connector_thermal_length);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}

		pr_debug("[%s]line=%d: usb_connector_thermal_length=%d\n",
				__FUNCTION__, __LINE__, chg->usb_connector_thermal_length);

		for (index = 0; index < chg->usb_connector_thermal_length; index++)
		{
			pr_debug("[%s]line=%d: usb_connector_thermal[%d]=0x%x\n",
				__FUNCTION__, __LINE__, index, chg->usb_connector_thermal[index]);
		}
	}
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */

	rc = of_property_read_u32(node, "qcom,float-option",
						&chip->dt.float_option);
	if (!rc && (chip->dt.float_option < 0 || chip->dt.float_option > 4)) {
		pr_err("qcom,float-option is out of range [0, 4]\n");
		return -EINVAL;
	}

	chip->dt.hvdcp_disable = of_property_read_bool(node,
						"qcom,hvdcp-disable");


	rc = of_property_read_u32(node, "qcom,chg-inhibit-threshold-mv",
				&chip->dt.chg_inhibit_thr_mv);
	if (!rc && (chip->dt.chg_inhibit_thr_mv < 0 ||
				chip->dt.chg_inhibit_thr_mv > 300)) {
		pr_err("qcom,chg-inhibit-threshold-mv is incorrect\n");
		return -EINVAL;
	}

	chip->dt.auto_recharge_soc = -EINVAL;
	rc = of_property_read_u32(node, "qcom,auto-recharge-soc",
				&chip->dt.auto_recharge_soc);
	if (!rc && (chip->dt.auto_recharge_soc < 0 ||
			chip->dt.auto_recharge_soc > 100)) {
		pr_err("qcom,auto-recharge-soc is incorrect\n");
		return -EINVAL;
	}
	chg->auto_recharge_soc = chip->dt.auto_recharge_soc;

	chip->dt.auto_recharge_vbat_mv = -EINVAL;
	rc = of_property_read_u32(node, "qcom,auto-recharge-vbat-mv",
				&chip->dt.auto_recharge_vbat_mv);
	if (!rc && (chip->dt.auto_recharge_vbat_mv < 0)) {
		pr_err("qcom,auto-recharge-vbat-mv is incorrect\n");
		return -EINVAL;
	}

	chg->dcp_icl_ua = chip->dt.usb_icl_ua;

	chg->suspend_input_on_debug_batt = of_property_read_bool(node,
					"qcom,suspend-input-on-debug-batt");

	rc = of_property_read_u32(node, "qcom,otg-deglitch-time-ms",
					&chg->otg_delay_ms);
	if (rc < 0)
		chg->otg_delay_ms = OTG_DEFAULT_DEGLITCH_TIME_MS;

	chg->hw_die_temp_mitigation = of_property_read_bool(node,
					"qcom,hw-die-temp-mitigation");

	chg->hw_connector_mitigation = of_property_read_bool(node,
					"qcom,hw-connector-mitigation");

	/* Huaqin add for HS60-63 Set cut-off voltage by gaochao at 2019/07/17 start */
	chg->hw_connector_mitigation = false;
	/* Huaqin add for HS60-63 Set cut-off voltage by gaochao at 2019/07/17 end */

	chg->connector_pull_up = -EINVAL;
	of_property_read_u32(node, "qcom,connector-internal-pull-kohm",
					&chg->connector_pull_up);

	/* Huaqin add for HS60-63 Set cut-off voltage by gaochao at 2019/07/17 start */
	chg->connector_pull_up = -EINVAL;
	/* Huaqin add for HS60-63 Set cut-off voltage by gaochao at 2019/07/17 end */

	chg->moisture_protection_enabled = of_property_read_bool(node,
					"qcom,moisture-protection-enable");

	chg->fcc_stepper_enable = of_property_read_bool(node,
					"qcom,fcc-stepping-enable");

	return 0;
}

static int smb5_get_adc_data(struct smb_charger *chg, int channel,
				union power_supply_propval *val)
{
	int rc = 0;
	struct qpnp_vadc_result result;
	u8 reg;

	if (!chg->vadc_dev) {
		if (of_find_property(chg->dev->of_node, "qcom,chg-vadc",
					NULL)) {
			chg->vadc_dev = qpnp_get_vadc(chg->dev, "chg");
			if (IS_ERR(chg->vadc_dev)) {
				rc = PTR_ERR(chg->vadc_dev);
				if (rc != -EPROBE_DEFER)
					pr_debug("Failed to find VADC node, rc=%d\n",
							rc);
				else
					chg->vadc_dev = NULL;

				return rc;
			}
		} else {
			return -ENODATA;
		}
	}

	if (IS_ERR(chg->vadc_dev))
		return PTR_ERR(chg->vadc_dev);

	mutex_lock(&chg->vadc_lock);

	switch (channel) {
	case USBIN_VOLTAGE:
		/* Store ADC channel config */
		rc = smblib_read(chg, BATIF_ADC_CHANNEL_EN_REG, &reg);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read ADC config rc=%d\n", rc);
			goto done;
		}

		/* Disable all ADC channels except IBAT channel */
		rc = smblib_write(chg, BATIF_ADC_CHANNEL_EN_REG,
				IBATT_CHANNEL_EN_BIT);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't write ADC config rc=%d\n", rc);
			goto done;
		}

		rc = qpnp_vadc_read(chg->vadc_dev, VADC_USB_IN_V_DIV_16_PM5,
				&result);
		if (rc < 0)
			pr_err("Failed to read USBIN_V over vadc, rc=%d\n", rc);
		else
			val->intval = result.physical;

		/* Restore ADC channel config */
		rc |= smblib_write(chg, BATIF_ADC_CHANNEL_EN_REG, reg);
		if (rc < 0)
			dev_err(chg->dev,
				"Couldn't write ADC config rc=%d\n", rc);

		break;
	case USBIN_CURRENT:
		rc = qpnp_vadc_read(chg->vadc_dev, VADC_USB_IN_I_PM5, &result);
		if (rc < 0) {
			pr_err("Failed to read USBIN_I over vadc, rc=%d\n", rc);
			goto done;
		}
		val->intval = result.physical;
		break;
	default:
		pr_debug("Invalid channel\n");
		rc = -EINVAL;
		break;
	}

done:
	mutex_unlock(&chg->vadc_lock);
	return rc;
}


/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property smb5_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PD_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_TYPEC_POWER_ROLE,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_PD_ACTIVE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_BOOST_CURRENT,
	POWER_SUPPLY_PROP_PE_START,
	POWER_SUPPLY_PROP_CTM_CURRENT_MAX,
	POWER_SUPPLY_PROP_HW_CURRENT_MAX,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_PR_SWAP,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_SDP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONNECTOR_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_HVDCP_OPTI_ALLOWED,
	POWER_SUPPLY_PROP_QC_OPTI_DISABLE,
	POWER_SUPPLY_PROP_MOISTURE_DETECTED,
};

static int smb5_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_usb_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_prop_usb_online(chg, val);
		/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 start */
		#if !defined(HQ_FACTORY_BUILD)	//ss version
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0)
		{
			pr_err( "Couldn't read usb_present. rc=%d\n", rc);
			return rc;
		}
		if ( (chg->flash_active) && (chg->online_state_last == 1) && (pval.intval == 1))
		{
			val->intval = 1;
		}
		chg->online_state_last = val->intval ;
		#endif
		/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 end */
		if (!val->intval)
			break;

		if (((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) ||
		   (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB))
			&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
			val->intval = 0;
		else
			val->intval = 1;

		if (chg->real_charger_type == POWER_SUPPLY_TYPE_UNKNOWN)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_prop_usb_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		rc = smblib_get_prop_usb_voltage_max_design(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable, PD_VOTER);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = chg->real_charger_type;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
			val->intval = POWER_SUPPLY_TYPEC_NONE;
		else
			val->intval = chg->typec_mode;
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
			val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		else
			rc = smblib_get_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
			val->intval = 0;
		else
			rc = smblib_get_prop_typec_cc_orientation(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		val->intval = chg->pd_active;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_BOOST_CURRENT:
		val->intval = chg->boost_current_ua;
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		rc = smblib_get_prop_pd_in_hard_reset(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		val->intval = chg->system_suspend_supported;
		break;
	case POWER_SUPPLY_PROP_PE_START:
		rc = smblib_get_pe_start(chg, val);
		break;
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable, CTM_VOTER);
		break;
	case POWER_SUPPLY_PROP_HW_CURRENT_MAX:
		rc = smblib_get_charge_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PR_SWAP:
		rc = smblib_get_prop_pr_swap_in_progress(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		val->intval = chg->voltage_max_uv;
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		val->intval = chg->voltage_min_uv;
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable,
					      USB_PSY_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TYPE:
		val->intval = chg->connector_type;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_UNKNOWN;
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0)
			break;
		val->intval = pval.intval ? POWER_SUPPLY_SCOPE_DEVICE
				: chg->otg_present ? POWER_SUPPLY_SCOPE_SYSTEM
						: POWER_SUPPLY_SCOPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0 || !pval.intval) {
			val->intval = 0;
			return rc;
		}
		if (chg->smb_version == PMI632_SUBTYPE)
			rc = smb5_get_adc_data(chg, USBIN_CURRENT, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (chg->smb_version == PMI632_SUBTYPE)
			rc = smb5_get_adc_data(chg, USBIN_VOLTAGE, val);
		break;
	case POWER_SUPPLY_PROP_HVDCP_OPTI_ALLOWED:
		val->intval = !chg->flash_active;
		break;
	case POWER_SUPPLY_PROP_QC_OPTI_DISABLE:
		if (chg->hw_die_temp_mitigation)
			val->intval = POWER_SUPPLY_QC_THERMAL_BALANCE_DISABLE
					| POWER_SUPPLY_QC_INOV_THERMAL_DISABLE;
		if (chg->hw_connector_mitigation)
			val->intval |= POWER_SUPPLY_QC_CTM_DISABLE;
		break;
	case POWER_SUPPLY_PROP_MOISTURE_DETECTED:
		val->intval = chg->moisture_present;
		break;
	default:
		pr_err("get prop %d is not supported in usb\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb5_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		rc = smblib_set_prop_pd_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		rc = smblib_set_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		rc = smblib_set_prop_pd_active(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		rc = smblib_set_prop_pd_in_hard_reset(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		chg->system_suspend_supported = val->intval;
		break;
	case POWER_SUPPLY_PROP_BOOST_CURRENT:
		rc = smblib_set_prop_boost_current(chg, val);
		break;
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		rc = vote(chg->usb_icl_votable, CTM_VOTER,
						val->intval >= 0, val->intval);
		break;
	case POWER_SUPPLY_PROP_PR_SWAP:
		rc = smblib_set_prop_pr_swap_in_progress(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		rc = smblib_set_prop_pd_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		rc = smblib_set_prop_pd_voltage_min(chg, val);
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		rc = smblib_set_prop_sdp_current_max(chg, val);
		break;
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smb5_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		return 1;
	default:
		break;
	}

	return 0;
}

#if defined(CONFIG_TYPEC)
#define ROLE_REVERSAL_DELAY_MS		1500
static void qpnp_typec_role_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct smb_charger *chg = container_of(dwork,
				struct smb_charger, role_reversal_check);
	int rc;
	union power_supply_propval pval;

	if (!chg || !chg->port) {
		pr_err("%s: chg or chg->port is null! Return\n", __func__);
		return;
	}

	switch (chg->requested_port_type) {
	case TYPEC_PORT_UFP:
		if ((chg->partner == NULL) ||
			(chg->typec_power_role != TYPEC_SINK) ||
			(chg->typec_data_role != TYPEC_DEVICE)) {
			pr_info("%s: Role-swap fails! Back to DRP mode\n", __func__);
			pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
			rc = smblib_set_prop_typec_power_role(chg, &pval);
			if (rc < 0)
				pr_err("%s: Failed to set DRP mode rc=%d\n", __func__, rc);
		}
		break;
	case TYPEC_PORT_DFP:
		if ((chg->partner == NULL) ||
			(chg->typec_power_role != TYPEC_SOURCE) ||
			(chg->typec_data_role != TYPEC_HOST)) {
			pr_info("%s: Role-swap fails! Back to DRP mode\n", __func__);
			chg->typec_power_role_flag = true;
			pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
			rc = smblib_set_prop_typec_power_role(chg, &pval);
			if (rc < 0)
				pr_err("%s: Failed to set DRP mode rc=%d\n", __func__, rc);
		}
		break;
	default:
		pr_info("Already in DRP mode\n");
		break;
	}

	return;
}

static int usbpd_dr_set(const struct typec_capability *cap, enum typec_data_role role)
{
	pr_info("%s: Do nothing\n", __func__);
	return 0;
}

static int usbpd_pr_set(const struct typec_capability *cap, enum typec_role role)
{
	pr_info("%s: Do nothing\n", __func__);
	return 0;
}

static int usbpd_port_type_set(const struct typec_capability *cap, enum typec_port_type port_type)
{
	int rc = 0;
	union power_supply_propval pval;
	struct smb_charger *chg = container_of(cap, struct smb_charger, typec_cap);

	if (!chg) {
		pr_err("%s: chg is null\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: typec_power_role=%d, typec_data_role=%d, pwr_opmode=%d, port_type=%d\n",
		__func__,chg->typec_power_role, chg->typec_data_role, chg->pwr_opmode, port_type);

	if (chg->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
		pr_err("Already in mode transition skipping request\n");
		return 0;
	}

	switch (port_type) {
	case TYPEC_PORT_UFP:
		if (rc < 0)
			pr_err("%s: Failed to set UFP mode rc=%d\n", __func__, rc);
		else {
			chg->requested_port_type = TYPEC_PORT_UFP;
			chg->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		}
		break;
	case TYPEC_PORT_DFP:
		if (rc < 0)
			pr_err("%s: Failed to set DFP mode rc=%d\n", __func__, rc);
		else {
			chg->requested_port_type = TYPEC_PORT_DFP;
			chg->typec_try_state_change = TRY_ROLE_SWAP_TYPE;
		}
		break;
	case TYPEC_PORT_DRP:
		pr_err("%s: don't set TYPEC_PORT_DRP here\n", __func__);
		return 0;
		break;
	default:
		pr_info("%s: Invalid role (not DFP/UFP) %d\n", __func__, port_type);
		rc = -EINVAL;
	}

	pval.intval = POWER_SUPPLY_TYPEC_PR_NONE;
	rc = smblib_set_prop_typec_power_role(chg, &pval);
	/*
	 * schedule delayed work to check if device latched to the
	 * requested mode.
	 */
	if ((rc >= 0) && (chg->typec_try_state_change == TRY_ROLE_SWAP_TYPE)) {
		cancel_delayed_work_sync(&chg->role_reversal_check);
		schedule_delayed_work(&chg->role_reversal_check,
			msecs_to_jiffies(ROLE_REVERSAL_DELAY_MS));
	}

	/* wait until it takes effect */
	while (chg->typec_try_state_change != TRY_ROLE_SWAP_NONE)
		msleep(20);

	return rc;
}
#endif

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.properties = smb5_usb_props,
	.num_properties = ARRAY_SIZE(smb5_usb_props),
	.get_property = smb5_usb_get_prop,
	.set_property = smb5_usb_set_prop,
	.property_is_writeable = smb5_usb_prop_is_writeable,
};

static int smb5_init_usb_psy(struct smb5 *chip)
{
	struct power_supply_config usb_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_cfg.drv_data = chip;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

/********************************
 * USB PC_PORT PSY REGISTRATION *
 ********************************/
static enum power_supply_property smb5_usb_port_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static int smb5_usb_port_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	union power_supply_propval pval = {0, };
	#endif
	/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 end */
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_prop_usb_online(chg, val);
		/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 start */
		#if !defined(HQ_FACTORY_BUILD)	//ss version
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0)
		{
			pr_err( "Couldn't read usb_present. rc=%d\n", rc);
			return rc;
		}
		if ( (chg->flash_active) && (chg->online_state_last == 1) && (pval.intval == 1))
		{
			val->intval = 1;
		}
		chg->online_state_last = val->intval ;
		#endif
		/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 end */
		if (!val->intval)
			break;

		if (((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) ||
		   (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB))
			&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	default:
		pr_err_ratelimited("Get prop %d is not supported in pc_port\n",
				psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb5_usb_port_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int rc = 0;

	switch (psp) {
	default:
		pr_err_ratelimited("Set prop %d is not supported in pc_port\n",
				psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_port_psy_desc = {
	.name		= "pc_port",
	.type		= POWER_SUPPLY_TYPE_USB,
	.properties	= smb5_usb_port_props,
	.num_properties	= ARRAY_SIZE(smb5_usb_port_props),
	.get_property	= smb5_usb_port_get_prop,
	.set_property	= smb5_usb_port_set_prop,
};

static int smb5_init_usb_port_psy(struct smb5 *chip)
{
	struct power_supply_config usb_port_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_port_cfg.drv_data = chip;
	usb_port_cfg.of_node = chg->dev->of_node;
	chg->usb_port_psy = devm_power_supply_register(chg->dev,
						  &usb_port_psy_desc,
						  &usb_port_cfg);
	if (IS_ERR(chg->usb_port_psy)) {
		pr_err("Couldn't register USB pc_port power supply\n");
		return PTR_ERR(chg->usb_port_psy);
	}

	return 0;
}
/* HS60 add for SR-ZQL1871-01-299 OTG psy by wangzikang at 2019/10/29 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
static enum power_supply_property smb5_otg_props[] = {
	POWER_SUPPLY_PROP_TYPE,
};

static int smb5_otg_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_OTG;
		break;
	default:
		pr_err_ratelimited("Get prop %d is not supported in pc_port\n",
				psp);
		return -EINVAL;
	}

	return 0;
}

static int smb5_otg_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int rc = 0;

	switch (psp) {
	default:
		pr_err_ratelimited("Set prop %d is not supported in pc_port\n",
				psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct power_supply_desc otg_psy_desc = {
	.name		= "otg",
	.type		= POWER_SUPPLY_TYPE_USB_OTG,
	.properties	= smb5_otg_props,
	.num_properties	= ARRAY_SIZE(smb5_otg_props),
	.get_property	= smb5_otg_get_prop,
	.set_property	= smb5_otg_set_prop,
};

static int smb5_init_otg_psy(struct smb5 *chip)
{
	struct power_supply_config otg_cfg = {};
	struct smb_charger *chg = &chip->chg;

	otg_cfg.drv_data = chip;
	otg_cfg.of_node = chg->dev->of_node;
	chg->otg_psy = devm_power_supply_register(chg->dev,
						  &otg_psy_desc,
						  &otg_cfg);
	if (IS_ERR(chg->otg_psy)) {
		pr_err("Couldn't register USB pc_port power supply\n");
		return PTR_ERR(chg->otg_psy);
	}

	return 0;
}
#endif
/* HS60 add for SR-ZQL1871-01-299 OTG psy by wangzikang at 2019/10/29 end */

/*****************************
 * USB MAIN PSY REGISTRATION *
 *****************************/

static enum power_supply_property smb5_usb_main_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED,
	POWER_SUPPLY_PROP_FCC_DELTA,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_FLASH_ACTIVE,
	POWER_SUPPLY_PROP_FLASH_TRIGGER,
};

static int smb5_usb_main_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fv, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fcc,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_MAIN;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED:
		rc = smblib_get_prop_input_voltage_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_FCC_DELTA:
		rc = smblib_get_prop_fcc_delta(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_icl_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_ACTIVE:
		val->intval = chg->flash_active;
		break;
	case POWER_SUPPLY_PROP_FLASH_TRIGGER:
		rc = schgm_flash_get_vreg_ok(chg, &val->intval);
		break;
	default:
		pr_debug("get prop %d is not supported in usb-main\n", psp);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_AFC)
static void smblib_flash_active_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
		flash_active_work.work);

	if(chg->flash_active) {
		if(chg->afc_sts == AFC_9V)
				afc_set_voltage(SET_5V);
	} else {
		if (chg->afc_sts == AFC_5V)
			afc_set_voltage(SET_9V);
	}
}
#endif
#endif
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/

static int smb5_usb_main_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval = {0, };
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_set_charge_param(chg, &chg->param.fv, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_set_charge_param(chg, &chg->param.fcc, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_set_icl_current(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_ACTIVE:
		if ((chg->smb_version == PMI632_SUBTYPE)
				&& (chg->flash_active != val->intval)) {
			chg->flash_active = val->intval;

			rc = smblib_get_prop_usb_present(chg, &pval);
			if (rc < 0)
				pr_err("Failed to get USB preset status rc=%d\n",
						rc);
			if (pval.intval) {
				rc = smblib_force_vbus_voltage(chg,
					chg->flash_active ? FORCE_5V_BIT
								: IDLE_BIT);
				if (rc < 0)
					pr_err("Failed to force 5V\n");
				else
					chg->pulse_cnt = 0;

				/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
				#if !defined(HQ_FACTORY_BUILD)	//ss version
				#if defined(CONFIG_AFC)
				if(chg->flash_active) {
					cancel_delayed_work_sync(&chg->flash_active_work);
					schedule_delayed_work(&chg->flash_active_work,
								round_jiffies_relative(msecs_to_jiffies(0)));
				} else {
					cancel_delayed_work_sync(&chg->flash_active_work);
					schedule_delayed_work(&chg->flash_active_work,
								round_jiffies_relative(msecs_to_jiffies(5000)));
				}
				#endif
				#endif
				/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
			} else {
				/* USB absent & flash not-active - vote 100mA */
				vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
							true, SDP_100_MA);
			}

			pr_debug("flash active VBUS 5V restriction %s\n",
				chg->flash_active ? "applied" : "removed");

			/* Update userspace */
			if (chg->batt_psy)
				power_supply_changed(chg->batt_psy);
		}
		break;
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_main_psy_desc = {
	.name		= "main",
	.type		= POWER_SUPPLY_TYPE_MAIN,
	.properties	= smb5_usb_main_props,
	.num_properties	= ARRAY_SIZE(smb5_usb_main_props),
	.get_property	= smb5_usb_main_get_prop,
	.set_property	= smb5_usb_main_set_prop,
};

static int smb5_init_usb_main_psy(struct smb5 *chip)
{
	struct power_supply_config usb_main_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_main_cfg.drv_data = chip;
	usb_main_cfg.of_node = chg->dev->of_node;
	chg->usb_main_psy = devm_power_supply_register(chg->dev,
						  &usb_main_psy_desc,
						  &usb_main_cfg);
	if (IS_ERR(chg->usb_main_psy)) {
		pr_err("Couldn't register USB main power supply\n");
		return PTR_ERR(chg->usb_main_psy);
	}

	return 0;
}

/*************************
 * DC PSY REGISTRATION   *
 *************************/

static enum power_supply_property smb5_dc_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_REAL_TYPE,
};

static int smb5_dc_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = get_effective_result(chg->dc_suspend_votable);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_dc_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_prop_dc_online(chg, val);
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = POWER_SUPPLY_TYPE_WIPOWER;
		break;
	default:
		return -EINVAL;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}
	return 0;
}

static int smb5_dc_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = vote(chg->dc_suspend_votable, WBC_VOTER,
				(bool)val->intval, 0);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smb5_dc_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc dc_psy_desc = {
	.name = "dc",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = smb5_dc_props,
	.num_properties = ARRAY_SIZE(smb5_dc_props),
	.get_property = smb5_dc_get_prop,
	.set_property = smb5_dc_set_prop,
	.property_is_writeable = smb5_dc_prop_is_writeable,
};

static int smb5_init_dc_psy(struct smb5 *chip)
{
	struct power_supply_config dc_cfg = {};
	struct smb_charger *chg = &chip->chg;

	dc_cfg.drv_data = chip;
	dc_cfg.of_node = chg->dev->of_node;
	chg->dc_psy = devm_power_supply_register(chg->dev,
						  &dc_psy_desc,
						  &dc_cfg);
	if (IS_ERR(chg->dc_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->dc_psy);
	}

	return 0;
}

/*************************
 * BATT PSY REGISTRATION *
 *************************/
static enum power_supply_property smb5_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_SW_JEITA_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_PARALLEL_DISABLE,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_DIE_HEALTH,
	POWER_SUPPLY_PROP_RERUN_AICL,
	POWER_SUPPLY_PROP_DP_DM,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_RECHARGE_SOC,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE,
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	#endif
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 end */
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_STORE_MODE,
	#endif
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */
	/* HS60 add for SR-ZQL1695-01000000466 Provide sysFS node named /sys/class/power_supply/battery/online by gaochao at 2019/08/08 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_ONLINE,
	#endif
	/* HS60 add for SR-ZQL1695-01000000466 Provide sysFS node named /sys/class/power_supply/battery/online by gaochao at 2019/08/08 end */
	/* HS60 add for SR-ZQL1695-01000000455 Provide sysFS node named /sys/class/power_supply/battery/batt_current_event by gaochao at 2019/08/08 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_BATT_CURRENT_EVENT,
	#endif
	/* HS60 add for SR-ZQL1695-01000000455 Provide sysFS node named /sys/class/power_supply/battery/batt_current_event by gaochao at 2019/08/08 end */
	/* HS60 add for SR-ZQL1695-01000000460 Provide sysFS node named /sys/class/power_supply/battery/batt_misc_event by gaochao at 2019/08/11 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_BATT_MISC_EVENT,
	#endif
	/* HS60 add for SR-ZQL1695-01000000460 Provide sysFS node named /sys/class/power_supply/battery/batt_misc_event by gaochao at 2019/08/11 end */
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_RECHARGE_VBAT,
	#endif
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 end */
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW,
	//+ SS_charging, add battery_cycle node
	POWER_SUPPLY_PROP_BATTERY_CYCLE,
	//- SS_charging, add battery_cycle node
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
	#if defined(CONFIG_AFC)
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_AFC_RESULT,
	POWER_SUPPLY_PROP_HV_DISABLE,
	#endif
	#endif
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
};

#define ITERM_SCALING_FACTOR_PMI632	1525
#define ITERM_SCALING_FACTOR_PM855B	3050
static int smb5_get_prop_batt_iterm(struct smb_charger *chg,
		union power_supply_propval *val)
{
	int rc, temp, scaling_factor;
	u8 stat, buf[2];

	/*
	 * Currently, only ADC comparator-based termination is supported,
	 * hence read only the threshold corresponding to ADC source.
	 * Proceed only if CHGR_ITERM_USE_ANALOG_BIT is 0.
	 */
	rc = smblib_read(chg, CHGR_ENG_CHARGING_CFG_REG, &stat);
	if (rc < 0) {
		pr_err("Couldn't read CHGR_ENG_CHARGING_CFG_REG rc=%d\n", rc);
		return rc;
	}

	if (stat & CHGR_ITERM_USE_ANALOG_BIT) {
		val->intval = -EINVAL;
		return 0;
	}

	rc = smblib_batch_read(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG, buf, 2);

	if (rc < 0) {
		pr_err("Couldn't read CHGR_ADC_ITERM_UP_THD_MSB_REG rc=%d\n",
				rc);
		return rc;
	}

	temp = buf[1] | (buf[0] << 8);
	temp = sign_extend32(temp, 15);

	if (chg->smb_version == PMI632_SUBTYPE)
		scaling_factor = ITERM_SCALING_FACTOR_PMI632;
	else
		scaling_factor = ITERM_SCALING_FACTOR_PM855B;

	temp = div_s64(temp * scaling_factor, 10000);
	val->intval = temp;

	return rc;
}

static int smb5_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb_charger *chg = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblib_get_prop_batt_status(chg, val);
		/* HS60 add for P191025-06620 Charging popup is coming while camera takes a photo with flash light by gaochao at 2019/11/21 start */
		#if !defined(HQ_FACTORY_BUILD)	//ss version
		chg->previous_charger_status = val->intval;
		#endif
		/* HS60 add for P191025-06620 Charging popup is coming while camera takes a photo with flash light by gaochao at 2019/11/21 end */
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = smblib_get_prop_batt_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_batt_present(chg, val);
		break;
	/* HS60 add for SR-ZQL1695-01000000466 Provide sysFS node named /sys/class/power_supply/battery/online by gaochao at 2019/08/08 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_ONLINE:
		smblib_get_prop_batt_online_samsung(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01000000466 Provide sysFS node named /sys/class/power_supply/battery/online by gaochao at 2019/08/08 end */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblib_get_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		val->intval = !get_client_vote(chg->chg_disable_votable,
					      USER_VOTER);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smblib_get_prop_batt_charge_type(chg, val);
		break;
	/* HS60 add for SR-ZQL1695-01000000455 Provide sysFS node named /sys/class/power_supply/battery/batt_current_event by gaochao at 2019/08/08 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_BATT_CURRENT_EVENT:
		smblib_get_prop_batt_batt_current_event_samsung(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01000000455 Provide sysFS node named /sys/class/power_supply/battery/batt_current_event by gaochao at 2019/08/08 end */
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblib_get_prop_batt_capacity(chg, val);
		break;
	/* HS60 add for SR-ZQL1695-01000000460 Provide sysFS node named /sys/class/power_supply/battery/batt_misc_event by gaochao at 2019/08/11 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_BATT_MISC_EVENT:
		smblib_get_prop_batt_batt_misc_event_samsung(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01000000460 Provide sysFS node named /sys/class/power_supply/battery/batt_misc_event by gaochao at 2019/08/11 end */
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblib_get_prop_system_temp_level(chg, val);
		break;
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		smblib_get_prop_batt_store_mode_samsung(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		val->intval = chg->slate_mode;
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 end */
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		rc = smblib_get_prop_system_temp_level_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblib_get_prop_input_current_limited(chg, val);
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		val->intval = chg->step_chg_enabled;
		break;
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
		val->intval = chg->sw_jeita_enabled;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = get_client_vote(chg->fv_votable,
				BATT_PROFILE_VOTER);
		break;
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		val->intval = chg->auto_recharge_vbat_mv;
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 end */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CURRENT_NOW, val);
		if (!rc)
			val->intval *= (-1);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_client_vote(chg->fcc_votable,
					      BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		rc = smb5_get_prop_batt_iterm(chg, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* Huaqin add for ZQL1695-HQ000001 Fix the battery temperature as 25 degree by gaochao at 2019/07/11 start */
		rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_TEMP, val);
		//val->intval = 250;
		/* Huaqin add for ZQL1695-HQ000001 Fix the battery temperature as 25 degree by gaochao at 2019/07/11 end */
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		rc = smblib_get_prop_batt_charge_done(chg, val);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
		val->intval = get_client_vote(chg->pl_disable_votable,
					      USER_VOTER);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		if (chg->die_health == -EINVAL)
			rc = smblib_get_prop_die_health(chg, val);
		else
			val->intval = chg->die_health;
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		val->intval = chg->pulse_cnt;
		break;
	case POWER_SUPPLY_PROP_RERUN_AICL:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_COUNTER, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CYCLE_COUNT, val);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		val->intval = chg->auto_recharge_soc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_FULL, val);
		break;
	case POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE:
		val->intval = chg->fcc_stepper_enable;
		break;
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CURRENT_NOW, val);
		if (!rc)
			val->intval *= (-1);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		if(chg->batt_cycle != -EINVAL) {
			val->intval = chg->batt_cycle;
		} else {
			val->intval = 0;
		}
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	#if defined(CONFIG_AFC)
	case POWER_SUPPLY_PROP_HV_CHARGER_STATUS:
		val->intval = 0;
		if(chg->hv_disable)
			val->intval = 0;
		else {
			if((chg->real_charger_type == POWER_SUPPLY_TYPE_AFC)
				|| (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP)
				|| (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3))
				val->intval = 1;
		}
		break;
	case POWER_SUPPLY_PROP_AFC_RESULT:
		if (chg->afc_sts >= AFC_5V)
			val->intval = 1;
		else
			val->intval = 0;

		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		val->intval = chg->hv_disable;
		break;
	#endif
	#endif
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb5_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct smb_charger *chg = power_supply_get_drvdata(psy);
	bool enable;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblib_set_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblib_set_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		vote(chg->chg_disable_votable, USER_VOTER, !val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblib_set_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblib_set_prop_batt_capacity(chg, val);
		break;
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		smblib_set_prop_batt_store_mode_samsung(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		chg->slate_mode = val->intval;
		rc = smblib_set_prop_input_suspend(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 end */
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
		vote(chg->pl_disable_votable, USER_VOTER, (bool)val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->batt_profile_fv_uv = val->intval;
		vote(chg->fv_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
		rc = smblib_set_prop_rechg_vbat_thresh(chg, val);
		break;
	#endif
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 end */
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		enable = !!val->intval || chg->sw_jeita_enabled;
		rc = smblib_configure_wdog(chg, enable);
		if (rc == 0)
			chg->step_chg_enabled = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
		if (chg->sw_jeita_enabled != (!!val->intval)) {
			rc = smblib_disable_hw_jeita(chg, !!val->intval);
			enable = !!val->intval || chg->step_chg_enabled;
			rc |= smblib_configure_wdog(chg, enable);
			if (rc == 0)
				chg->sw_jeita_enabled = !!val->intval;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		chg->batt_profile_fcc_ua = val->intval;
		vote(chg->fcc_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as the device is active */
		if (!val->intval)
			break;
		if (chg->pl.psy)
			power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_SET_SHIP_MODE, val);
		rc = smblib_set_prop_ship_mode(chg, val);
		break;
	case POWER_SUPPLY_PROP_RERUN_AICL:
		rc = smblib_run_aicl(chg, RERUN_AICL);
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		if (!chg->flash_active)
			rc = smblib_dp_dm(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblib_set_prop_input_current_limited(chg, val);
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		chg->die_health = val->intval;
		power_supply_changed(chg->batt_psy);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		if (chg->smb_version == PMI632_SUBTYPE) {
			/* toggle charging to force recharge */
			vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
					true, 0);
			/* charge disable delay */
			msleep(50);
			vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
					false, 0);
		}
		break;
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	//+ SS_charging, add battery_cycle node
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
		if(val->intval >= 0) {
			chg->batt_cycle = val->intval;
			rc = power_supply_set_property(chg->bms_psy,
					POWER_SUPPLY_PROP_BATTERY_CYCLE, val);
			pr_err("BATTERY_CYCLE(%d)\n", chg->batt_cycle);
		}
		/* Checking Cycle and Update battery health */
		chg->battery_health = BATTERY_HEALTH_BAD;
		if (1500 >= (chg->batt_cycle % 10000))
			chg->battery_health  = BATTERY_HEALTH_AGED;
		if (1200 >= (chg->batt_cycle % 10000))
			chg->battery_health  = BATTERY_HEALTH_NORMAL;
		if (900 >= (chg->batt_cycle % 10000))
			chg->battery_health  = BATTERY_HEALTH_GOOD;
		pr_info("%s: update battery_health(%d)\n", __func__, chg->battery_health);
		break;
	//- SS_charging, add battery_cycle node
	#endif
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	#if defined(CONFIG_AFC)
	case POWER_SUPPLY_PROP_AFC_RESULT:
		is_afc_result(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		if (val->intval == HV_DISABLE) {
			chg->hv_disable = true;
			vote(chg->usb_icl_votable, SEC_BATTERY_DISABLE_HV_VOTER,
					true, DCP_CURRENT_UA);
		} else {
			chg->hv_disable = false;
			vote(chg->usb_icl_votable, SEC_BATTERY_DISABLE_HV_VOTER,
					false, 0);
		}
		break;
	#endif
	#endif
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int smb5_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CAPACITY:
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	#endif
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 end */
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
	#endif
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
	case POWER_SUPPLY_PROP_DP_DM:
	case POWER_SUPPLY_PROP_RERUN_AICL:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
	case POWER_SUPPLY_PROP_DIE_HEALTH:
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	case POWER_SUPPLY_PROP_RECHARGE_VBAT:
	#endif
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 end */
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	//+ SS_charging, add battery_cycle node
	case POWER_SUPPLY_PROP_BATTERY_CYCLE:
	//- SS_charging, add battery_cycle node
	#endif
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = smb5_batt_props,
	.num_properties = ARRAY_SIZE(smb5_batt_props),
	.get_property = smb5_batt_get_prop,
	.set_property = smb5_batt_set_prop,
	.property_is_writeable = smb5_batt_prop_is_writeable,
};

static int smb5_init_batt_psy(struct smb5 *chip)
{
	struct power_supply_config batt_cfg = {};
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}

/******************************
 * VBUS REGULATOR REGISTRATION *
 ******************************/

static struct regulator_ops smb5_vbus_reg_ops = {
	.enable = smblib_vbus_regulator_enable,
	.disable = smblib_vbus_regulator_disable,
	.is_enabled = smblib_vbus_regulator_is_enabled,
};

static int smb5_init_vbus_regulator(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	chg->vbus_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vbus_vreg),
				      GFP_KERNEL);
	if (!chg->vbus_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vbus_vreg->rdesc.owner = THIS_MODULE;
	chg->vbus_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vbus_vreg->rdesc.ops = &smb5_vbus_reg_ops;
	chg->vbus_vreg->rdesc.of_match = "qcom,smb5-vbus";
	chg->vbus_vreg->rdesc.name = "qcom,smb5-vbus";

	chg->vbus_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vbus_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vbus_vreg->rdev)) {
		rc = PTR_ERR(chg->vbus_vreg->rdev);
		chg->vbus_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VBUS regulator rc=%d\n", rc);
	}

	return rc;
}

/******************************
 * VCONN REGULATOR REGISTRATION *
 ******************************/

static struct regulator_ops smb5_vconn_reg_ops = {
	.enable = smblib_vconn_regulator_enable,
	.disable = smblib_vconn_regulator_disable,
	.is_enabled = smblib_vconn_regulator_is_enabled,
};

static int smb5_init_vconn_regulator(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	chg->vconn_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vconn_vreg),
				      GFP_KERNEL);
	if (!chg->vconn_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vconn_vreg->rdesc.owner = THIS_MODULE;
	chg->vconn_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vconn_vreg->rdesc.ops = &smb5_vconn_reg_ops;
	chg->vconn_vreg->rdesc.of_match = "qcom,smb5-vconn";
	chg->vconn_vreg->rdesc.name = "qcom,smb5-vconn";

	chg->vconn_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vconn_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vconn_vreg->rdev)) {
		rc = PTR_ERR(chg->vconn_vreg->rdev);
		chg->vconn_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VCONN regulator rc=%d\n", rc);
	}

	return rc;
}

/***************************
 * HARDWARE INITIALIZATION *
 ***************************/
static int smb5_configure_typec(struct smb_charger *chg)
{
	int rc;
	u8 val = 0;

	rc = smblib_read(chg, LEGACY_CABLE_STATUS_REG, &val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read Legacy status rc=%d\n", rc);
		return rc;
	}
	/*
	 * If Legacy cable is detected re-trigger Legacy detection
	 * by disabling/enabling typeC mode.
	 */
	if (val & TYPEC_LEGACY_CABLE_STATUS_BIT) {
		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_DISABLE_CMD_BIT, TYPEC_DISABLE_CMD_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable TYPEC rc=%d\n", rc);
			return rc;
		}

		/* delay before enabling typeC */
		msleep(500);

		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_DISABLE_CMD_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable TYPEC rc=%d\n", rc);
			return rc;
		}
	}

	/* disable apsd */
	rc = smblib_configure_hvdcp_apsd(chg, false);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't disable APSD rc=%d\n", rc);
		return rc;
	}

	/* HS60 add for HQ000001 While power on VBUS, start BC1.2 by gaochao at 2020/01/14 start */
	rc = smblib_masked_write(chg, TYPE_C_CFG_REG,
		BC1P2_START_ON_CC_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable BC1.2 rc=%d\n", rc);
		return rc;
	}
	/* HS60 add for HQ000001 While power on VBUS, start BC1.2 by gaochao at 2020/01/14 end */

	rc = smblib_write(chg, TYPE_C_INTERRUPT_EN_CFG_1_REG,
				TYPEC_CCOUT_DETACH_INT_EN_BIT |
				TYPEC_CCOUT_ATTACH_INT_EN_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	/* enable try.snk and clear force sink for DRP mode */
	rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				EN_TRY_SNK_BIT | EN_SNK_ONLY_BIT,
				EN_TRY_SNK_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't enable try.snk rc=%d\n", rc);
		return rc;
	}

	/* Keep VCONN in h/w controlled mode for PMI632 */
	if (chg->smb_version != PMI632_SUBTYPE) {
		/* configure VCONN for software control */
		rc = smblib_masked_write(chg, TYPE_C_VCONN_CONTROL_REG,
				 VCONN_EN_SRC_BIT | VCONN_EN_VALUE_BIT,
				 VCONN_EN_SRC_BIT);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure VCONN for SW control rc=%d\n",
				rc);
			return rc;
		}
	}

	/* Disable TypeC and RID change source interrupts */
	rc = smblib_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int smb5_configure_micro_usb(struct smb_charger *chg)
{
	int rc;

	rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					MICRO_USB_STATE_CHANGE_INT_EN_BIT,
					MICRO_USB_STATE_CHANGE_INT_EN_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	if (chg->moisture_protection_enabled &&
				(chg->wa_flags & MOISTURE_PROTECTION_WA)) {
		/* Enable moisture detection interrupt */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
				TYPEC_WATER_DETECTION_INT_EN_BIT,
				TYPEC_WATER_DETECTION_INT_EN_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable moisture detection interrupt rc=%d\n",
				rc);
			return rc;
		}

		/* Enable uUSB factory mode */
		rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT,
					EN_MICRO_USB_FACTORY_MODE_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable uUSB factory mode c=%d\n",
				rc);
			return rc;
		}

		/* Disable periodic monitoring of CC_ID pin */
		rc = smblib_write(chg, TYPEC_U_USB_WATER_PROTECTION_CFG_REG, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable periodic monitoring of CC_ID rc=%d\n",
				rc);
			return rc;
		}
	}

	return rc;
}

static int smb5_configure_mitigation(struct smb_charger *chg)
{
	int rc;
	u8 chan = 0, src_cfg = 0;

	if (!chg->hw_die_temp_mitigation && !chg->hw_connector_mitigation)
		return 0;

	if (chg->hw_die_temp_mitigation) {
		chan = DIE_TEMP_CHANNEL_EN_BIT;
		src_cfg = THERMREG_DIE_ADC_SRC_EN_BIT
			| THERMREG_DIE_CMP_SRC_EN_BIT;
	}

	if (chg->hw_connector_mitigation) {
		chan |= CONN_THM_CHANNEL_EN_BIT;
		src_cfg |= THERMREG_CONNECTOR_ADC_SRC_EN_BIT;
	}

	rc = smblib_masked_write(chg, MISC_THERMREG_SRC_CFG_REG,
			THERMREG_SW_ICL_ADJUST_BIT | THERMREG_DIE_ADC_SRC_EN_BIT
			| THERMREG_DIE_CMP_SRC_EN_BIT
			| THERMREG_CONNECTOR_ADC_SRC_EN_BIT, src_cfg);
	if (rc < 0) {
		dev_err(chg->dev,
				"Couldn't configure THERM_SRC reg rc=%d\n", rc);
		return rc;
	};

	rc = smblib_masked_write(chg, BATIF_ADC_CHANNEL_EN_REG,
			CONN_THM_CHANNEL_EN_BIT | DIE_TEMP_CHANNEL_EN_BIT,
			chan);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable ADC channel rc=%d\n", rc);
		return rc;
	}

	return 0;
}

#define RAW_TERM_CURR(conv_factor, scaled_ma)	\
				div_s64((int64_t)scaled_ma * 10000, conv_factor)
#define ITERM_LIMITS_PMI632_MA	5000
#define ITERM_LIMITS_PM855B_MA	10000
static int smb5_configure_iterm_thresholds_adc(struct smb5 *chip)
{
	u8 *buf;
	int rc = 0;
	int raw_hi_thresh, raw_lo_thresh, max_limit_ma, scaling_factor;
	struct smb_charger *chg = &chip->chg;

	if (chip->chg.smb_version == PMI632_SUBTYPE) {
		scaling_factor = ITERM_SCALING_FACTOR_PMI632;
		max_limit_ma = ITERM_LIMITS_PMI632_MA;
	} else {
		scaling_factor = ITERM_SCALING_FACTOR_PM855B;
		max_limit_ma = ITERM_LIMITS_PM855B_MA;
	}

	if (chip->dt.term_current_thresh_hi_ma < (-1 * max_limit_ma)
		|| chip->dt.term_current_thresh_hi_ma > max_limit_ma
		|| chip->dt.term_current_thresh_lo_ma < (-1 * max_limit_ma)
		|| chip->dt.term_current_thresh_lo_ma > max_limit_ma) {
		dev_err(chg->dev, "ITERM threshold out of range rc=%d\n", rc);
		return -EINVAL;
	}

	/*
	 * Conversion:
	 *	raw (A) = (scaled_mA * (10000) / conv_factor)
	 * Note: raw needs to be converted to big-endian format.
	 */

	if (chip->dt.term_current_thresh_hi_ma) {
		raw_hi_thresh = RAW_TERM_CURR(scaling_factor,
					chip->dt.term_current_thresh_hi_ma);
		raw_hi_thresh = sign_extend32(raw_hi_thresh, 15);
		buf = (u8 *)&raw_hi_thresh;
		raw_hi_thresh = buf[1] | (buf[0] << 8);

		rc = smblib_batch_write(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG,
				(u8 *)&raw_hi_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold HIGH rc=%d\n",
					rc);
			return rc;
		}
	}

	if (chip->dt.term_current_thresh_lo_ma) {
		raw_lo_thresh = RAW_TERM_CURR(scaling_factor,
					chip->dt.term_current_thresh_lo_ma);
		raw_lo_thresh = sign_extend32(raw_lo_thresh, 15);
		buf = (u8 *)&raw_lo_thresh;
		raw_lo_thresh = buf[1] | (buf[0] << 8);

		rc = smblib_batch_write(chg, CHGR_ADC_ITERM_LO_THD_MSB_REG,
				(u8 *)&raw_lo_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold LOW rc=%d\n",
					rc);
			return rc;
		}
	}

	return rc;
}

static int smb5_configure_iterm_thresholds(struct smb5 *chip)
{
	int rc = 0;

	switch (chip->dt.term_current_src) {
	case ITERM_SRC_ADC:
		rc = smb5_configure_iterm_thresholds_adc(chip);
		break;
	default:
		break;
	}

	return rc;
}
/* HS70 add for HS70-736 Decrease threshold of AICL to get more ICL from TA by gaochao at 2019/11/19 start */
#define CUSTOM_DEFAULT_AICL_5V_THRESHOLD_MV  4000
/* HS70 add for HS70-736 Decrease threshold of AICL to get more ICL from TA by gaochao at 2019/11/19 end */
static int smb5_init_hw(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc, type = 0;
	u8 val = 0;
	/* Huaqin add for P200731-01593 Enable charging while TYPE-C is default mode by gaochao at 2020/08/10 start */
	u32 pcba_config = 0;
	pcba_config = hq_get_huaqin_pcba_config();
	/* Huaqin add for P200731-01593 Enable charging while TYPE-C is default mode by gaochao at 2020/08/10 end */

	if (chip->dt.no_battery)
		chg->fake_capacity = 50;

	if (chip->dt.batt_profile_fcc_ua < 0)
		smblib_get_charge_param(chg, &chg->param.fcc,
				&chg->batt_profile_fcc_ua);

	if (chip->dt.batt_profile_fv_uv < 0)
		smblib_get_charge_param(chg, &chg->param.fv,
				&chg->batt_profile_fv_uv);

	smblib_get_charge_param(chg, &chg->param.usb_icl,
				&chg->default_icl_ua);
	smblib_get_charge_param(chg, &chg->param.aicl_5v_threshold,
				&chg->default_aicl_5v_threshold_mv);

	/* HS50 add for HS50-186 re-enable hw jeita by wenyaqi at 2020/9/2 start */
	rc = smblib_masked_write(chg, JEITA_EN_CFG_REG, 0x10, 0x10);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't re-enable hw jeita rc=%d\n", rc);
	}
	/* HS50 add for HS50-186 re-enable hw jeita by wenyaqi at 2020/9/2 end */

 	/* HS70 add for HS70-736 Decrease threshold of AICL to get more ICL from TA by gaochao at 2019/11/19 start */
	if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_PLATFORM ||
		chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_HS50)  //HS70 or HS50
 	{
  		chg->default_aicl_5v_threshold_mv = CUSTOM_DEFAULT_AICL_5V_THRESHOLD_MV;
 	}
 	/* HS70 add for HS70-736 Decrease threshold of AICL to get more ICL from TA by gaochao at 2019/11/19 end */

	chg->aicl_5v_threshold_mv = chg->default_aicl_5v_threshold_mv;

 	/* HS70 add for HS70-736 Decrease threshold of AICL to get more ICL from TA by gaochao at 2019/11/19 start */
	if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_PLATFORM ||
		chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_HS50)  //HS70 or HS50
 	{
  		smblib_set_charge_param(chg, &chg->param.aicl_5v_threshold, chg->aicl_5v_threshold_mv);
 	}
 	/* HS70 add for HS70-736 Decrease threshold of AICL to get more ICL from TA by gaochao at 2019/11/19 end */

	smblib_get_charge_param(chg, &chg->param.aicl_cont_threshold,
				&chg->default_aicl_cont_threshold_mv);
	chg->aicl_cont_threshold_mv = chg->default_aicl_cont_threshold_mv;

	/* Use SW based VBUS control, disable HW autonomous mode */
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
	if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM439_PLATFORM)	//HS60
	{
		/* HS60 add for HS60-162 Disable HVDCP by gaochao at 2019/08/07 start */
		/*
		rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
			HVDCP_AUTH_ALG_EN_CFG_BIT | HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT,
			HVDCP_AUTH_ALG_EN_CFG_BIT);
		*/
		rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG, HVDCP_EN_BIT, 0);
		/* HS60 add for HS60-162 Disable HVDCP by gaochao at 2019/08/07 end */
	}
	else		//HS70 or HS50
	{
		rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
			HVDCP_AUTH_ALG_EN_CFG_BIT | HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT,
			HVDCP_AUTH_ALG_EN_CFG_BIT);
	}
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure HVDCP rc=%d\n", rc);
		return rc;
	}

	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	#if defined(CONFIG_AFC)
       rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG, HVDCP_EN_BIT, 0);
       if (rc < 0) {
               dev_err(chg->dev, "Couldn't config HVDCP rc=%d\n", rc);
               return rc;
       }
	#endif
	#endif
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
	/*
	 * PMI632 can have the connector type defined by a dedicated register
	 * TYPEC_MICRO_USB_MODE_REG or by a common TYPEC_U_USB_CFG_REG.
	 */
	if (chg->smb_version == PMI632_SUBTYPE) {
		rc = smblib_read(chg, TYPEC_MICRO_USB_MODE_REG, &val);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't read USB mode rc=%d\n", rc);
			return rc;
		}
		type = !!(val & MICRO_USB_MODE_ONLY_BIT);
	}

	/*
	 * If TYPEC_MICRO_USB_MODE_REG is not set and for all non-PMI632
	 * check the connector type using TYPEC_U_USB_CFG_REG.
	 */
	if (!type) {
		rc = smblib_read(chg, TYPEC_U_USB_CFG_REG, &val);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't read U_USB config rc=%d\n",
					rc);
			return rc;
		}

		type = !!(val & EN_MICRO_USB_MODE_BIT);
	}

	pr_debug("Connector type=%s\n", type ? "Micro USB" : "TypeC");

	if (type) {
		chg->connector_type = POWER_SUPPLY_CONNECTOR_MICRO_USB;
		rc = smb5_configure_micro_usb(chg);
	} else {
		chg->connector_type = POWER_SUPPLY_CONNECTOR_TYPEC;
		rc = smb5_configure_typec(chg);
	}
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure TypeC/micro-USB mode rc=%d\n", rc);
		return rc;
	}

	/*
	 * PMI632 based hw init:
	 * - Enable STAT pin function on SMB_EN
	 * - Rerun APSD to ensure proper charger detection if device
	 *   boots with charger connected.
	 * - Initialize flash module for PMI632
	 */
	if (chg->smb_version == PMI632_SUBTYPE) {
		rc = smblib_masked_write(chg, MISC_SMB_EN_CMD_REG,
					EN_STAT_CMD_BIT, EN_STAT_CMD_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure SMB_EN rc=%d\n",
					rc);
			return rc;
		}

		schgm_flash_init(chg);
		smblib_rerun_apsd_if_required(chg);
	}

	/* clear the ICL override if it is set */
	rc = smblib_icl_override(chg, false);
	if (rc < 0) {
		pr_err("Couldn't disable ICL override rc=%d\n", rc);
		return rc;
	}

	/* set OTG current limit */
	rc = smblib_set_charge_param(chg, &chg->param.otg_cl, chg->otg_cl_ua);
	if (rc < 0) {
		pr_err("Couldn't set otg current limit rc=%d\n", rc);
		return rc;
	}

	/* configure temperature mitigation */
	rc = smb5_configure_mitigation(chg);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure mitigation rc=%d\n", rc);
		return rc;
	}

	/* vote 0mA on usb_icl for non battery platforms */
	vote(chg->usb_icl_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->dc_suspend_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->fcc_votable, HW_LIMIT_VOTER,
		chip->dt.batt_profile_fcc_ua > 0, chip->dt.batt_profile_fcc_ua);
	vote(chg->fv_votable, HW_LIMIT_VOTER,
		chip->dt.batt_profile_fv_uv > 0, chip->dt.batt_profile_fv_uv);
	vote(chg->fcc_votable,
		BATT_PROFILE_VOTER, chg->batt_profile_fcc_ua > 0,
		chg->batt_profile_fcc_ua);
	vote(chg->fv_votable,
		BATT_PROFILE_VOTER, chg->batt_profile_fv_uv > 0,
		chg->batt_profile_fv_uv);

	/* Some h/w limit maximum supported ICL */
	vote(chg->usb_icl_votable, HW_LIMIT_VOTER,
			chg->hw_max_icl_ua > 0, chg->hw_max_icl_ua);

	/*
	 * AICL configuration:
	 * AICL ADC disable
	 */
	if (chg->smb_version != PMI632_SUBTYPE) {
		rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
				USBIN_AICL_ADC_EN_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't config AICL rc=%d\n", rc);
			return rc;
		}
	}
	/*Huaqin add for HS60-2106 by wangzikang at 2019/10/01 start*/
	else
	{
		rc = smblib_write(chg, USBIN_AICL_OPTIONS_CFG_REG,0xc7);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't config AICL rc=%d\n", rc);
			return rc;
		}

		rc = smblib_write(chg, USBIN_LOAD_CFG_REG,0x27);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't config LOAD_CFG rc=%d\n", rc);
			return rc;
		}
	}
	/*Huaqin add for HS60-2106 by wangzikang at 2019/10/01 end*/

	/* enable the charging path */
	rc = vote(chg->chg_disable_votable, DEFAULT_VOTER, false, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable charging rc=%d\n", rc);
		return rc;
	}

	/* configure VBUS for software control */
	rc = smblib_masked_write(chg, DCDC_OTG_CFG_REG, OTG_EN_SRC_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure VBUS for SW control rc=%d\n", rc);
		return rc;
	}

	/*
	 * configure the one time watchdong periodic interval and
	 * disable "watchdog bite disable charging".
	 */
	val = (ilog2(chip->dt.wd_bark_time / 16) << BARK_WDOG_TIMEOUT_SHIFT)
			& BARK_WDOG_TIMEOUT_MASK;
	val |= BITE_WDOG_TIMEOUT_8S;
	rc = smblib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			BITE_WDOG_DISABLE_CHARGING_CFG_BIT |
			BARK_WDOG_TIMEOUT_MASK | BITE_WDOG_TIMEOUT_MASK,
			val);
	if (rc < 0) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}

	/* set termination current threshold values */
	rc = smb5_configure_iterm_thresholds(chip);
	if (rc < 0) {
		pr_err("Couldn't configure ITERM thresholds rc=%d\n",
				rc);
		return rc;
	}

	/* configure float charger options */
	switch (chip->dt.float_option) {
	case FLOAT_DCP:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, 0);
		break;
	case FLOAT_SDP:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, FORCE_FLOAT_SDP_CFG_BIT);
		break;
	case DISABLE_CHARGING:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, FLOAT_DIS_CHGING_CFG_BIT);
		break;
	case SUSPEND_INPUT:
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK, SUSPEND_FLOAT_CFG_BIT);
		break;
	default:
		rc = 0;
		break;
	}

	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure float charger options rc=%d\n",
			rc);
		return rc;
	}

	rc = smblib_read(chg, USBIN_OPTIONS_2_CFG_REG, &chg->float_cfg);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read float charger options rc=%d\n",
			rc);
		return rc;
	}

	switch (chip->dt.chg_inhibit_thr_mv) {
	case 50:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_50MV);
		break;
	case 100:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_100MV);
		break;
	case 200:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_200MV);
		break;
	case 300:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_300MV);
		break;
	case 0:
		rc = smblib_masked_write(chg, CHGR_CFG2_REG,
				CHARGER_INHIBIT_BIT, 0);
	default:
		break;
	}

	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure charge inhibit threshold rc=%d\n",
			rc);
		return rc;
	}

	rc = smblib_masked_write(chg, CHGR_CFG2_REG, RECHG_MASK,
				(chip->dt.auto_recharge_vbat_mv != -EINVAL) ?
				VBAT_BASED_RECHG_BIT : 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure VBAT-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge VBAT threshold */
	if (chip->dt.auto_recharge_vbat_mv != -EINVAL) {
		u32 temp = VBAT_TO_VRAW_ADC(chip->dt.auto_recharge_vbat_mv);

		temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
		rc = smblib_batch_write(chg,
			CHGR_ADC_RECHARGE_THRESHOLD_MSB_REG, (u8 *)&temp, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ADC_RECHARGE_THRESHOLD REG rc=%d\n",
				rc);
			return rc;
		}
		/* Program the sample count for VBAT based recharge to 3 */
		rc = smblib_masked_write(chg, CHGR_NO_SAMPLE_TERM_RCHG_CFG_REG,
					NO_OF_SAMPLE_FOR_RCHG,
					2 << NO_OF_SAMPLE_FOR_RCHG_SHIFT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHGR_NO_SAMPLE_FOR_TERM_RCHG_CFG rc=%d\n",
				rc);
			return rc;
		}
	}
	/* HS60 add for P191031-07911 by wangzikang at 2019/11/08 start */
	rc = smblib_masked_write(chg, CHGR_CFG2_REG, RECHG_MASK,
				(chip->dt.auto_recharge_soc != -EINVAL) ?
				//SOC_BASED_RECHG_BIT : VBAT_BASED_RECHG_BIT);
				VBAT_BASED_RECHG_BIT: VBAT_BASED_RECHG_BIT);
	/* HS60 add for P191031-07911 by wangzikang at 2019/11/08 end */
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure SOC-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge threshold */
	if (chip->dt.auto_recharge_soc != -EINVAL) {
		rc = smblib_write(chg, CHARGE_RCHG_SOC_THRESHOLD_CFG_REG,
				(chip->dt.auto_recharge_soc * 255) / 100);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHG_RCHG_SOC_REG rc=%d\n",
				rc);
			return rc;
		}
		/* Program the sample count for SOC based recharge to 1 */
		rc = smblib_masked_write(chg, CHGR_NO_SAMPLE_TERM_RCHG_CFG_REG,
						NO_OF_SAMPLE_FOR_RCHG, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHGR_NO_SAMPLE_FOR_TERM_RCHG_CFG rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chg->sw_jeita_enabled) {
		rc = smblib_disable_hw_jeita(chg, true);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't set hw jeita rc=%d\n", rc);
			return rc;
		}
	}

	rc = smblib_configure_wdog(chg,
			chg->step_chg_enabled || chg->sw_jeita_enabled);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure watchdog rc=%d\n",
				rc);
		return rc;
	}

	if (chg->connector_pull_up != -EINVAL) {
		rc = smb5_configure_internal_pull(chg, CONN_THERM,
				get_valid_pullup(chg->connector_pull_up));
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure CONN_THERM pull-up rc=%d\n",
				rc);
			return rc;
		}
	}

	/*Huaqin add for HQ00001 enable charging while Rp-Rp on both CC Pins by qianyingdong at 2020/06/02 start*/
	rc = smblib_write(chg, 0x154A, 0x17);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set 0x154A rc=%d\n", rc);
		return rc;
	}
	/*Huaqin add for HQ00001 enable charging while Rp-Rp on both CC Pins by qianyingdong at 2020/06/02 end*/
	/* Huaqin add for P200731-01593 Enable charging while TYPE-C is default mode by gaochao at 2020/08/10 start */
	if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_PLATFORM)		//HS70
	{
		if ((((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HS70_HQ_PCBA_AT_T)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HS70_HQ_PCBA_Canada))
		{
			pr_info("[%s]line=%d: ignore A11 NA\n", __FUNCTION__, __LINE__);
		}
		else
		{
			rc = smblib_write(chg, 0x1342, 0x1);
			if (rc < 0) {
				dev_err(chg->dev, "Couldn't config 0x1342 to 0x1 rc=%d\n", rc);
			}
		}
	}
	else
	{
		/*Huaqin add for Enable Charge while TypeC mode detected as DEFAULT by qianyingdong at 2020/07/14 start*/
		rc = smblib_write(chg, 0x1342, 0x1);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't config 0x1342 to 0x1 rc=%d\n", rc);
		}
		/*Huaqin add for Enable Charge while TypeC mode detected as DEFAULT by qianyingdong at 2020/07/14 end*/
	}
	/* Huaqin add for P200731-01593 Enable charging while TYPE-C is default mode by gaochao at 2020/08/10 end */
	return rc;
}

static int smb5_post_init(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval;
	int rc;

	/*
	 * In case the usb path is suspended, we would have missed disabling
	 * the icl change interrupt because the interrupt could have been
	 * not requested
	 */
	rerun_election(chg->usb_icl_votable);

	/* configure power role for dual-role */
	pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
	rc = smblib_set_prop_typec_power_role(chg, &pval);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure DRP role rc=%d\n",
				rc);
		return rc;
	}

	rerun_election(chg->usb_irq_enable_votable);

	return 0;
}

/****************************
 * DETERMINE INITIAL STATUS *
 ****************************/

static int smb5_determine_initial_status(struct smb5 *chip)
{
	struct smb_irq_data irq_data = {chip, "determine-initial-status"};
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval val;
	int rc;

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		return rc;
	}
	chg->early_usb_attach = val.intval;

	if (chg->bms_psy)
		smblib_suspend_on_debug_battery(chg);

	usb_plugin_irq_handler(0, &irq_data);
	typec_attach_detach_irq_handler(0, &irq_data);
	typec_state_change_irq_handler(0, &irq_data);
	usb_source_change_irq_handler(0, &irq_data);
	chg_state_change_irq_handler(0, &irq_data);
	icl_change_irq_handler(0, &irq_data);
	batt_temp_changed_irq_handler(0, &irq_data);
	wdog_bark_irq_handler(0, &irq_data);
	typec_or_rid_detection_change_irq_handler(0, &irq_data);

	return 0;
}

/**************************
 * INTERRUPT REGISTRATION *
 **************************/

static struct smb_irq_info smb5_irqs[] = {
	/* CHARGER IRQs */
	[CHGR_ERROR_IRQ] = {
		.name		= "chgr-error",
		.handler	= default_irq_handler,
	},
	[CHG_STATE_CHANGE_IRQ] = {
		.name		= "chg-state-change",
		.handler	= chg_state_change_irq_handler,
		.wake		= true,
	},
	[STEP_CHG_STATE_CHANGE_IRQ] = {
		.name		= "step-chg-state-change",
	},
	[STEP_CHG_SOC_UPDATE_FAIL_IRQ] = {
		.name		= "step-chg-soc-update-fail",
	},
	[STEP_CHG_SOC_UPDATE_REQ_IRQ] = {
		.name		= "step-chg-soc-update-req",
	},
	[FG_FVCAL_QUALIFIED_IRQ] = {
		.name		= "fg-fvcal-qualified",
	},
	[VPH_ALARM_IRQ] = {
		.name		= "vph-alarm",
	},
	[VPH_DROP_PRECHG_IRQ] = {
		.name		= "vph-drop-prechg",
	},
	/* DCDC IRQs */
	[OTG_FAIL_IRQ] = {
		.name		= "otg-fail",
		.handler	= default_irq_handler,
	},
	[OTG_OC_DISABLE_SW_IRQ] = {
		.name		= "otg-oc-disable-sw",
	},
	[OTG_OC_HICCUP_IRQ] = {
		.name		= "otg-oc-hiccup",
	},
	[BSM_ACTIVE_IRQ] = {
		.name		= "bsm-active",
	},
	[HIGH_DUTY_CYCLE_IRQ] = {
		.name		= "high-duty-cycle",
		.handler	= high_duty_cycle_irq_handler,
		.wake		= true,
	},
	[INPUT_CURRENT_LIMITING_IRQ] = {
		.name		= "input-current-limiting",
		.handler	= default_irq_handler,
	},
	[CONCURRENT_MODE_DISABLE_IRQ] = {
		.name		= "concurrent-mode-disable",
	},
	[SWITCHER_POWER_OK_IRQ] = {
		.name		= "switcher-power-ok",
		.handler	= switcher_power_ok_irq_handler,
	},
	/* BATTERY IRQs */
	[BAT_TEMP_IRQ] = {
		.name		= "bat-temp",
		.handler	= batt_temp_changed_irq_handler,
		.wake		= true,
	},
	[ALL_CHNL_CONV_DONE_IRQ] = {
		.name		= "all-chnl-conv-done",
	},
	[BAT_OV_IRQ] = {
		.name		= "bat-ov",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BAT_LOW_IRQ] = {
		.name		= "bat-low",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BAT_THERM_OR_ID_MISSING_IRQ] = {
		.name		= "bat-therm-or-id-missing",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BAT_TERMINAL_MISSING_IRQ] = {
		.name		= "bat-terminal-missing",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BUCK_OC_IRQ] = {
		.name		= "buck-oc",
	},
	[VPH_OV_IRQ] = {
		.name		= "vph-ov",
	},
	/* USB INPUT IRQs */
	[USBIN_COLLAPSE_IRQ] = {
		.name		= "usbin-collapse",
		.handler	= default_irq_handler,
	},
	[USBIN_VASHDN_IRQ] = {
		.name		= "usbin-vashdn",
		.handler	= default_irq_handler,
	},
	[USBIN_UV_IRQ] = {
		.name		= "usbin-uv",
		.handler	= usbin_uv_irq_handler,
		.wake		= true,
		.storm_data	= {true, 3000, 5},
	},
	[USBIN_OV_IRQ] = {
		.name		= "usbin-ov",
		.handler	= usbin_ov_irq_handler,
	},
	[USBIN_PLUGIN_IRQ] = {
		.name		= "usbin-plugin",
		.handler	= usb_plugin_irq_handler,
		.wake           = true,
	},
	[USBIN_REVI_CHANGE_IRQ] = {
		.name		= "usbin-revi-change",
	},
	[USBIN_SRC_CHANGE_IRQ] = {
		.name		= "usbin-src-change",
		.handler	= usb_source_change_irq_handler,
		.wake           = true,
	},
	[USBIN_ICL_CHANGE_IRQ] = {
		.name		= "usbin-icl-change",
		.handler	= icl_change_irq_handler,
		.wake           = true,
	},
	/* DC INPUT IRQs */
	[DCIN_VASHDN_IRQ] = {
		.name		= "dcin-vashdn",
	},
	[DCIN_UV_IRQ] = {
		.name		= "dcin-uv",
		.handler	= default_irq_handler,
	},
	[DCIN_OV_IRQ] = {
		.name		= "dcin-ov",
		.handler	= default_irq_handler,
	},
	[DCIN_PLUGIN_IRQ] = {
		.name		= "dcin-plugin",
		.handler	= dc_plugin_irq_handler,
		.wake           = true,
	},
	[DCIN_REVI_IRQ] = {
		.name		= "dcin-revi",
	},
	[DCIN_PON_IRQ] = {
		.name		= "dcin-pon",
		.handler	= default_irq_handler,
	},
	[DCIN_EN_IRQ] = {
		.name		= "dcin-en",
		.handler	= default_irq_handler,
	},
	/* TYPEC IRQs */
	[TYPEC_OR_RID_DETECTION_CHANGE_IRQ] = {
		.name		= "typec-or-rid-detect-change",
		.handler	= typec_or_rid_detection_change_irq_handler,
		.wake           = true,
	},
	[TYPEC_VPD_DETECT_IRQ] = {
		.name		= "typec-vpd-detect",
	},
	[TYPEC_CC_STATE_CHANGE_IRQ] = {
		.name		= "typec-cc-state-change",
		.handler	= typec_state_change_irq_handler,
		.wake           = true,
	},
	[TYPEC_VCONN_OC_IRQ] = {
		.name		= "typec-vconn-oc",
		.handler	= default_irq_handler,
	},
	[TYPEC_VBUS_CHANGE_IRQ] = {
		.name		= "typec-vbus-change",
	},
	[TYPEC_ATTACH_DETACH_IRQ] = {
		.name		= "typec-attach-detach",
		.handler	= typec_attach_detach_irq_handler,
		.wake		= true,
	},
	[TYPEC_LEGACY_CABLE_DETECT_IRQ] = {
		.name		= "typec-legacy-cable-detect",
		.handler	= default_irq_handler,
	},
	[TYPEC_TRY_SNK_SRC_DETECT_IRQ] = {
		.name		= "typec-try-snk-src-detect",
	},
	/* MISCELLANEOUS IRQs */
	[WDOG_SNARL_IRQ] = {
		.name		= "wdog-snarl",
	},
	[WDOG_BARK_IRQ] = {
		.name		= "wdog-bark",
		.handler	= wdog_bark_irq_handler,
		.wake		= true,
	},
	[AICL_FAIL_IRQ] = {
		.name		= "aicl-fail",
	},
	[AICL_DONE_IRQ] = {
		.name		= "aicl-done",
		.handler	= default_irq_handler,
	},
	[SMB_EN_IRQ] = {
		.name		= "smb-en",
	},
	[IMP_TRIGGER_IRQ] = {
		.name		= "imp-trigger",
	},
	[TEMP_CHANGE_IRQ] = {
		.name		= "temp-change",
	},
	[TEMP_CHANGE_SMB_IRQ] = {
		.name		= "temp-change-smb",
	},
	/* FLASH */
	[VREG_OK_IRQ] = {
		.name		= "vreg-ok",
	},
	[ILIM_S2_IRQ] = {
		.name		= "ilim2-s2",
		.handler	= schgm_flash_ilim2_irq_handler,
	},
	[ILIM_S1_IRQ] = {
		.name		= "ilim1-s1",
	},
	[VOUT_DOWN_IRQ] = {
		.name		= "vout-down",
	},
	[VOUT_UP_IRQ] = {
		.name		= "vout-up",
	},
	[FLASH_STATE_CHANGE_IRQ] = {
		.name		= "flash-state-change",
		.handler	= schgm_flash_state_change_irq_handler,
	},
	[TORCH_REQ_IRQ] = {
		.name		= "torch-req",
	},
	[FLASH_EN_IRQ] = {
		.name		= "flash-en",
	},
};

static int smb5_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb5_irqs); i++) {
		if (strcmp(smb5_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb5_request_interrupt(struct smb5 *chip,
				struct device_node *node, const char *irq_name)
{
	struct smb_charger *chg = &chip->chg;
	int rc, irq, irq_index;
	struct smb_irq_data *irq_data;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smb5_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smb5_irqs[irq_index].handler)
		return 0;

	irq_data = devm_kzalloc(chg->dev, sizeof(*irq_data), GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	irq_data->parent_data = chip;
	irq_data->name = irq_name;
	irq_data->storm_data = smb5_irqs[irq_index].storm_data;
	mutex_init(&irq_data->storm_data.storm_lock);

	rc = devm_request_threaded_irq(chg->dev, irq, NULL,
					smb5_irqs[irq_index].handler,
					IRQF_ONESHOT, irq_name, irq_data);
	if (rc < 0) {
		pr_err("Couldn't request irq %d\n", irq);
		return rc;
	}

	smb5_irqs[irq_index].irq = irq;
	smb5_irqs[irq_index].irq_data = irq_data;
	if (smb5_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smb5_request_interrupts(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = smb5_request_interrupt(chip, child, name);
			if (rc < 0)
				return rc;
		}
	}
	if (chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq)
		chg->usb_icl_change_irq_enabled = true;

	return rc;
}

static void smb5_free_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb5_irqs); i++) {
		if (smb5_irqs[i].irq > 0) {
			if (smb5_irqs[i].wake)
				disable_irq_wake(smb5_irqs[i].irq);

			devm_free_irq(chg->dev, smb5_irqs[i].irq,
						smb5_irqs[i].irq_data);
		}
	}
}

static void smb5_disable_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb5_irqs); i++) {
		if (smb5_irqs[i].irq > 0)
			disable_irq(smb5_irqs[i].irq);
	}
}

#if defined(CONFIG_DEBUG_FS)

static int force_batt_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->batt_psy);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_batt_psy_update_ops, NULL,
			force_batt_psy_update_write, "0x%02llx\n");

static int force_usb_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->usb_psy);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_usb_psy_update_ops, NULL,
			force_usb_psy_update_write, "0x%02llx\n");

static int force_dc_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->dc_psy);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_dc_psy_update_ops, NULL,
			force_dc_psy_update_write, "0x%02llx\n");

static void smb5_create_debugfs(struct smb5 *chip)
{
	struct dentry *file;

	chip->dfs_root = debugfs_create_dir("charger", NULL);
	if (IS_ERR_OR_NULL(chip->dfs_root)) {
		pr_err("Couldn't create charger debugfs rc=%ld\n",
			(long)chip->dfs_root);
		return;
	}

	file = debugfs_create_file("force_batt_psy_update", 0600,
			    chip->dfs_root, chip, &force_batt_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_batt_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_usb_psy_update", 0600,
			    chip->dfs_root, chip, &force_usb_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_usb_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_dc_psy_update", 0600,
			    chip->dfs_root, chip, &force_dc_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_dc_psy_update file rc=%ld\n",
			(long)file);
}

#else

static void smb5_create_debugfs(struct smb5 *chip)
{}

#endif

static int smb5_show_charger_status(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval val;
	int usb_present, batt_present, batt_health, batt_charge_type;
	int rc;

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		return rc;
	}
	usb_present = val.intval;

	rc = smblib_get_prop_batt_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt present rc=%d\n", rc);
		return rc;
	}
	batt_present = val.intval;

	rc = smblib_get_prop_batt_health(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt health rc=%d\n", rc);
		val.intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	batt_health = val.intval;

	rc = smblib_get_prop_batt_charge_type(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return rc;
	}
	batt_charge_type = val.intval;

	pr_info("SMB5 status - usb:present=%d type=%d batt:present = %d health = %d charge = %d\n",
		usb_present, chg->real_charger_type,
		batt_present, batt_health, batt_charge_type);
	return rc;
}

/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
#define MATCH_TYPE_C_PROTECTION	0
#define MATCH_USB_THERMAL			1

void load_driver_via_board_id(struct smb_charger *chg, int length, u32 board_id, int match_id)
{
	int index = 0;
	int *p_array = NULL;
	int *enable = NULL;

	if (chg && (length > 0))
	{
		if (match_id == MATCH_TYPE_C_PROTECTION)
		{
			enable = &chg->typec_interface_protection_enable;
			p_array = chg->typec_interface_protection;
		}
		else if (match_id == MATCH_USB_THERMAL)
		{
			enable = &chg->usb_connector_thermal_enable;
			p_array = chg->usb_connector_thermal;
		}
		else
		{
			pr_info("[ss][%s]line=%d: reserved do nothing\n", __FUNCTION__, __LINE__);
		}

		*enable = false;
		for (index = 0; index < length; index++)
		{
			if (p_array[index] == board_id)
			{
				*enable = true;

				pr_info("[ss][%s]line=%d: enable=%d, p_array[%d]=0x%x, board_id=0x%x, match_id=%d\n",
						__FUNCTION__, __LINE__, *enable, index, p_array[index], board_id, match_id);

				return;
			}
		}

		/*
		if (match_id == MATCH_TYPE_C_PROTECTION)
		{
			chg->typec_interface_protection_enable = false;

			for (index = 0; index < length; index++)
			{
				if (chg->typec_interface_protection[index] == board_id)
				{
					chg->typec_interface_protection_enable = true;

					printk("[ss][%s]line=%d: typec_interface_protection_enable=%d, board_id=0x%x, match_id=%d\n",
							__FUNCTION__, __LINE__, chg->typec_interface_protection_enable, board_id, match_id);

					return;
				}
			}
		}
		else if (match_id == MATCH_USB_THERMAL)
		{
			chg->usb_connector_thermal_enable = false;

			for (index = 0; index < length; index++)
			{
				if (chg->usb_connector_thermal[index] == board_id)
				{
					chg->usb_connector_thermal_enable = true;

					printk("[ss][%s]line=%d: usb_connector_thermal_enable=%d, board_id=0x%x, match_id=%d\n",
							__FUNCTION__, __LINE__, chg->usb_connector_thermal_enable, board_id, match_id);

					return;
				}
			}
		}
		else
		{
			printk("[ss][%s]line=%d: reserved do nothing\n", __FUNCTION__, __LINE__);
		}
		*/
	}
	else
	{
		pr_err("[ss][%s]line=%d: chg is null\n", __FUNCTION__, __LINE__);
	}
}
/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */

/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 start */
#if defined(CUSTOM_VREG_L8_2P95)
#define VREG_L8_VTG_MIN_UV                  2900000
#define VREG_L8_VTG_MAX_UV                  2900000
int request_VREG_L8(struct smb_charger *chg, bool enable)
{
	int rc = 0;
	/* fetch the DPDM regulator */
	if (!chg->VREG_L8_2P95 && of_get_property(chg->dev->of_node,
				"VREG_L8_2P95-supply", NULL))
	{
		chg->VREG_L8_2P95 = devm_regulator_get(chg->dev, "VREG_L8_2P95");
		if (IS_ERR(chg->VREG_L8_2P95))
		{
			rc = PTR_ERR(chg->VREG_L8_2P95);
			pr_err("Couldn't get VREG_L8_2P95 regulator rc=%d\n", rc);
			chg->VREG_L8_2P95 = NULL;

			return rc;
		}
	}
	/* set VREG_L8_2P95 */
	if (enable)
	{
		if (chg->VREG_L8_2P95 && !regulator_is_enabled(chg->VREG_L8_2P95))
		{
			pr_info("enabling VREG_L8_2P95 regulator\n");

			/* set output voltage */
			rc = regulator_set_voltage(chg->VREG_L8_2P95, VREG_L8_VTG_MIN_UV, VREG_L8_VTG_MAX_UV);
			if (rc)
			{
				pr_err("Couldn't enable VREG_L8_2P95 regulator output voltage failed rc=%d\n", rc);
			}

			/*set output current */
			/*
			rc = regulator_set_load(chg->VREG_L8_2P95, int load_uA);
			if (rc)
			{
				pr_err("Couldn't enable VREG_L8_2P95 regulator output current failed rc=%d\n", rc);
			}
			*/

			/* enable VREG_L8_2P95 */
			rc = regulator_enable(chg->VREG_L8_2P95);
			if (rc < 0)
			{
				pr_err("Couldn't enable VREG_L8_2P95 regulator rc=%d\n", rc);
			}
		}
	}
	else
	{
		if (chg->VREG_L8_2P95 && regulator_is_enabled(chg->VREG_L8_2P95))
		{
			pr_info("disabling VREG_L8_2P95 regulator\n");

			/* disable VREG_L8_2P95 */
			rc = regulator_disable(chg->VREG_L8_2P95);
			if (rc < 0)
			{
				pr_err("Couldn't disable VREG_L8_2P95 regulator rc=%d\n", rc);
			}
		}
	}

	return rc;
}
#endif
/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 end */

/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 start */
#define HQ_PCBA_CONFIG_SMEM_ITEM_KERNEL         135
static u32  huaqin_pcba_config = 0xffffffff;

void *smem_get_entry(unsigned int id, unsigned int *size, unsigned int to_proc, unsigned int flags);

u32 hq_board_pcba_config(void)
{
	u32 *pcba_config_addr = NULL;
	u32 smem_pcba_config_size = 0;

	pcba_config_addr = (u32 *)smem_get_entry(HQ_PCBA_CONFIG_SMEM_ITEM_KERNEL, &smem_pcba_config_size, 0, 0);
	if (pcba_config_addr)
	{
		huaqin_pcba_config = *pcba_config_addr;

		pr_debug("[ss]line=%d: board_pcba_config: smem_pcba_config_size=%d, pcba_config=0x%x\n",
				__LINE__, smem_pcba_config_size, huaqin_pcba_config);
		return 0;
	}
	else
	{
		pr_err("[ss]line=%d: board_pcba_config: reading the smem pcba config address fail\n", __LINE__);
		return 1;
	}
}

u32 hq_get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 end */

/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 start */
void hq_set_VREG_L8_via_pcba_config(struct smb_charger *chg)
{
	if (!chg)
	{
		pr_err("[ss]line=%d: ichg is null\n", __LINE__);
		return;
	}

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	if (chg->typec_interface_protection_enable)
	{
		request_VREG_L8(chg, true);
	}

	pr_debug("[ss]line=%d: request_VREG_L8, typec_interface_protection_enable=%d\n",
			__LINE__, chg->typec_interface_protection_enable);
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */
}
/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 end */

/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 start */
void hq_vbus_control_gpio_init(struct smb_charger *chg, struct platform_device *pdev)
{
	int rc = 0;

	chg->vbus_control_gpio = of_get_named_gpio(pdev->dev.of_node, "draw-VBUS-GPIO59", 0);
	rc = gpio_request(chg->vbus_control_gpio, "draw-VBUS-GPIO59");
	if (!rc)
	{
		if ((int)chg->vbus_control_gpio >= 0)
		{
			chg->vbus_control_gpio_enable = true;
		}
		pr_debug("line=%d: Success to request draw-VBUS-GPIO59=%d\n",
				__LINE__, (int)chg->vbus_control_gpio);
	}
	else
	{
		chg->vbus_control_gpio_enable = false;
		/* HS70 add for HS70-250 Set usb thermal by gaochao at 2019/10/31 start */
		pr_err("line=%d: failed to request draw-VBUS-GPIO59, rc=%d\n", __LINE__, rc);
		/* HS70 add for HS70-250 Set usb thermal by gaochao at 2019/10/31 end */
	}

	if (!rc)
	{
		rc = gpio_direction_output(chg->vbus_control_gpio, DRAW_VBUS_GPIO59_LOW);
		if (rc)
		{
			pr_err("line=%d: failed to pull low vbus_control_gpio\n", __LINE__);
		}
		else
		{
			pr_debug("line=%d: Pull low vbus_control_gpio\n", __LINE__);
		}
	}

	rc = gpio_get_value(chg->vbus_control_gpio);

	pr_debug("line=%d: init vbus_control_gpio=%d, vbus_control_gpio_enable=%d\n",
			__LINE__, rc, chg->vbus_control_gpio_enable);
}

void hq_vbus_control_gpio_init_config(struct smb_charger *chg, struct platform_device *pdev)
{
	if (!chg || !pdev)
	{
		pr_err("[ss]line=%d: ichg or pdev is null\n", __LINE__);
		return;
	}

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	if (chg->usb_connector_thermal_enable)
	{
		hq_vbus_control_gpio_init(chg, pdev);
	}

	pr_debug("[ss]line=%d: hq_vbus_control_gpio_init, usb_connector_thermal_enable=%d\n",
			__LINE__, chg->usb_connector_thermal_enable);
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */
}
/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 end */

/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 start */
struct smb_charger *chg_dev = NULL;
/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 end */
/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
struct smb_charger *smbchg_dev = NULL;
#if defined(CONFIG_BATT_CISD)
extern void batt_cisd_init(struct smb_charger *chip);
extern int batt_create_attrs(struct device *dev);
#endif
#endif
/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 end */

static int smb5_probe(struct platform_device *pdev)
{
	struct smb5 *chip;
	struct smb_charger *chg;
	int rc = 0;
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
	u32 pcba_config = 0;
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chg = &chip->chg;
	chg->dev = &pdev->dev;
	chg->debug_mask = &__debug_mask;
	chg->pd_disabled = &__pd_disabled;
	chg->weak_chg_icl_ua = &__weak_chg_icl_ua;
	chg->mode = PARALLEL_MASTER;
	chg->irq_info = smb5_irqs;
	chg->die_health = -EINVAL;
	chg->otg_present = false;
	mutex_init(&chg->vadc_lock);

	/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	smbchg_dev = chg;
	pr_debug("[%s]line=%d: init smbchg_dev\n", __FUNCTION__, __LINE__);
	#endif
	/* HS70 add for HS71-21 Optimize the stop and resume charging of battery temperature by gaochao at 2019/11/29 end */

	chg->regmap = dev_get_regmap(chg->dev->parent, NULL);
	if (!chg->regmap) {
		pr_err("parent regmap is missing\n");
		return -EINVAL;
	}

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	chg->vbus_control_gpio_enable = false;

	rc = of_property_read_u32(chg->dev->of_node, "qcom,distinguish-sdm439-sdm450-others",
			&chg->distinguish_sdm439_sdm450_others);
	if (!rc && (chg->distinguish_sdm439_sdm450_others < 0))
	{
		pr_err("qcom,distinguish-sdm439-sdm450-others is incorrect\n");
	}

	pr_info("[%s]line=%d: distinguish_sdm439_sdm450_others=%d, vbus_control_gpio_enable=%d\n",
			__FUNCTION__, __LINE__, chg->distinguish_sdm439_sdm450_others, chg->vbus_control_gpio_enable);
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */

	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 start */
	chg->is_dcp = false;
	chg_dev = chg;
	pr_debug("[%s]line=%d: init chg_dev\n", __FUNCTION__, __LINE__);
	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 end */

	/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 start */
	hq_board_pcba_config();
	/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 end */
	/* HS70 add for HS70-919 Set ICL as 1650mA and 2000mA in different versions by qianyingdong at 2019/11/21 start */
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
	pcba_config = hq_get_huaqin_pcba_config();
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
	if (chg->distinguish_sdm439_sdm450_others == DETECT_SDM439_PLATFORM)  //HS60
	{
		/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 start */
		if ((((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_Q_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_Q_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_Q_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_ROW_4000mAh_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_PRC_IN_ID_4000mAh_ZQL1695_SMB)
			|| (((pcba_config & PCB_MASK_HQ) >> PCB_SHIFT_HQ) == HQ_PCBA_LATAM_4000mAh_ZQL1695_SMB))
		{
			//ZQL1695 & ZQL1635
			chip->dt.usb_icl_ua = 1050000;
		}
		else //ZQL1693 & ZQL1690
		{
			/* HS60 add for ZQL1693-60 Set ICL as 1650mA instead of QC default 3A by gaochao at 2019/11/07 start */
			rc = of_property_read_u32(chg->dev->of_node, "qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
			if (rc < 0)
				chip->dt.usb_icl_ua = -EINVAL;
			/* HS60 add for ZQL1693-60 Set ICL as 1650mA instead of QC default 3A by gaochao at 2019/11/07 end */
		}
	}
	/*HS60 added for HS60-5473 Optimize the charge expierience for HS60 with 4000mAh battery by wangzikang at 2020/04/07 end */
	else if(chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_PLATFORM ||
			chg->distinguish_sdm439_sdm450_others == DETECT_SDM450_HS50)  //HS70 or HS50
	{
		rc = of_property_read_u32(chg->dev->of_node, "qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
		if (rc < 0)
			chip->dt.usb_icl_ua = -EINVAL;
	}
	else
	{
		pr_err("qcom,distinguish-sdm439-sdm450-others is incorrect\n");
	}
	/* HS70 add for HS70-919 Set ICL as 1650mA and 2000mA in different versions by qianyingdong at 2019/11/21 end */

	rc = smb5_chg_config_init(chip);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't setup chg_config rc=%d\n", rc);
		return rc;
	}

	rc = smb5_parse_dt(chip);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	load_driver_via_board_id(chg, chg->typec_interface_protection_length, hq_get_huaqin_pcba_config(), MATCH_TYPE_C_PROTECTION);
	load_driver_via_board_id(chg, chg->usb_connector_thermal_length, hq_get_huaqin_pcba_config(), MATCH_USB_THERMAL);
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */

	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
	#if defined(CUSTOM_VREG_L8_2P95)
	hq_set_VREG_L8_via_pcba_config(chg);
	#endif

	hq_vbus_control_gpio_init_config(chg, pdev);
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */

	rc = smblib_init(chg);
	if (rc < 0) {
		pr_err("Smblib_init failed rc=%d\n", rc);
		return rc;
	}

	/* set driver data before resources request it */
	platform_set_drvdata(pdev, chip);

	rc = smb5_init_vbus_regulator(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize vbus regulator rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smb5_init_vconn_regulator(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize vconn regulator rc=%d\n",
				rc);
		goto cleanup;
	}

	/* extcon registration */
	chg->extcon = devm_extcon_dev_allocate(chg->dev, smblib_extcon_cable);
	if (IS_ERR(chg->extcon)) {
		rc = PTR_ERR(chg->extcon);
		dev_err(chg->dev, "failed to allocate extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	rc = devm_extcon_dev_register(chg->dev, chg->extcon);
	if (rc < 0) {
		dev_err(chg->dev, "failed to register extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	rc = smb5_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto cleanup;
	}

	if (chg->smb_version == PM855B_SUBTYPE) {
		rc = smb5_init_dc_psy(chip);
		if (rc < 0) {
			pr_err("Couldn't initialize dc psy rc=%d\n", rc);
			goto cleanup;
		}
	}

	rc = smb5_init_usb_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_usb_main_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb main psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_usb_port_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb pc_port psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_batt_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}
	/* HS60 add for SR-ZQL1871-01-299 OTG psy by wangzikang at 2019/10/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	rc = smb5_init_otg_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize otg psy rc=%d\n", rc);
		goto cleanup;
	}
	#endif
	/* HS60 add for SR-ZQL1871-01-299 OTG psy by wangzikang at 2019/10/29 end */
	rc = smb5_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smb5_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_post_init(chip);
	if (rc < 0) {
		pr_err("Failed in post init rc=%d\n", rc);
		goto free_irq;
	}

	smb5_create_debugfs(chip);

	rc = smb5_show_charger_status(chip);
	if (rc < 0) {
		pr_err("Failed in getting charger status rc=%d\n", rc);
		goto free_irq;
	}
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	#if defined(CONFIG_AFC)
	if(get_afc_mode())
		chg->hv_disable = HV_DISABLE;
	else
		chg->hv_disable = HV_ENABLE;

	INIT_DELAYED_WORK(&chg->flash_active_work, smblib_flash_active_work);
	#endif
	#endif
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/

	device_init_wakeup(chg->dev, true);

/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_BATT_CISD)
	batt_cisd_init(chg);

	if (!IS_ERR(chg->batt_psy)) {
		batt_create_attrs(&chg->batt_psy->dev);
	} else {
		pr_info("%s : batt-psy not available\n", __func__);
	}
#endif
#endif
/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */

#if defined(CONFIG_TYPEC)
	chg->typec_cap.revision = USB_TYPEC_REV_1_2;
	chg->typec_cap.pd_revision = 0x000;
	chg->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	chg->typec_cap.pr_set = usbpd_pr_set;
	chg->typec_cap.dr_set = usbpd_dr_set;
	chg->typec_cap.port_type_set = usbpd_port_type_set;

	chg->typec_cap.type = TYPEC_PORT_DRP;

	chg->typec_power_role = TYPEC_SINK;
	chg->typec_data_role = TYPEC_DEVICE;
	chg->typec_try_state_change = TRY_ROLE_SWAP_NONE;
	chg->pwr_opmode = TYPEC_PWR_MODE_USB;
	INIT_DELAYED_WORK(&chg->role_reversal_check,
				qpnp_typec_role_check_work);

	chg->port = typec_register_port(chg->dev, &chg->typec_cap);
	if (IS_ERR(chg->port))
		pr_err("%s: unable to register typec_register_port\n", __func__);
	else
		pr_info("%s: success typec_register_port\n", __func__);
	chg->partner = NULL;
	init_completion(&chg->typec_reverse_completion);
#endif
	pr_info("QPNP SMB5 probed successfully\n");

	return rc;

free_irq:
	smb5_free_interrupts(chg);
cleanup:
	smblib_deinit(chg);
	platform_set_drvdata(pdev, NULL);

	return rc;
}

static int smb5_remove(struct platform_device *pdev)
{
	struct smb5 *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	/* force enable APSD */
	smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				BC1P2_SRC_DETECT_BIT, BC1P2_SRC_DETECT_BIT);

	smb5_free_interrupts(chg);
	smblib_deinit(chg);
	platform_set_drvdata(pdev, NULL);
#if defined(CONFIG_TYPEC)
	typec_unregister_port(chg->port);
#endif
	return 0;
}

static void smb5_shutdown(struct platform_device *pdev)
{
	struct smb5 *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	/* disable all interrupts */
	smb5_disable_interrupts(chg);

	/* configure power role for UFP */
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC)
		smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_POWER_ROLE_CMD_MASK, EN_SNK_ONLY_BIT);

	/* force HVDCP to 5V */
	smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT, 0);
	smblib_write(chg, CMD_HVDCP_2_REG, FORCE_5V_BIT);

	/* force enable APSD */
	smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				BC1P2_SRC_DETECT_BIT, BC1P2_SRC_DETECT_BIT);
/* HS50 add for P201023-04559 re-connect vbus when shutdown with afc TA by wenyaqi at 2020/10/28 start */
#if defined(CONFIG_AFC)
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_AFC)
	{
		if (chg->vbus_control_gpio_enable)
		{
			ss_vbus_control_gpio_set(chg, DRAW_VBUS_GPIO59_HIGH);
			ss_vbus_control_gpio_set(chg, DRAW_VBUS_GPIO59_LOW);
		}
	}
#endif
/* HS50 add for P201023-04559 re-connect vbus when shutdown with afc TA by wenyaqi at 2020/10/28 end */
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-smb5", },
	{ },
};

static struct platform_driver smb5_driver = {
	.driver		= {
		.name		= "qcom,qpnp-smb5",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= smb5_probe,
	.remove		= smb5_remove,
	.shutdown	= smb5_shutdown,
};
module_platform_driver(smb5_driver);

MODULE_DESCRIPTION("QPNP SMB5 Charger Driver");
MODULE_LICENSE("GPL v2");
