/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/samsung/sec_class.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ktime.h>

#define DUMP_SET_COUNT			10
#define REGULATOR_TYPE_COUNT		2
#define REGULATOR_COUNT			20
#define REGISTER_COUNT			10

#define PON_BASE			0x800
#define PON_INT_RT_STS			(PON_BASE + 0x10)
#define PON_INT_LATCHED_STS		(PON_BASE + 0x18)
#define PON_INT_PENDING_STS		(PON_BASE + 0x19)

#define SMPS_BASE			0x1400
#define SMPS_CTRL_STATUS1		(SMPS_BASE + 0x08)
#define SMPS_CTRL_INT_RT_STS		(SMPS_BASE + 0x10)
#define SMPS_CTRL_INT_LATCHED_CLR	(SMPS_BASE + 0x14)
#define SMPS_CTRL_FOLLOW_HWEN		(SMPS_BASE + 0x47)

#define LDO_BASE			0x4000
#define LDO_STATUS1			(LDO_BASE + 0x8)
#define LDO_STATUS3			(LDO_BASE + 0xA)
#define LDO_STATUS4			(LDO_BASE + 0xB)
#define LDO_INT_RT_STS			(LDO_BASE + 0x10)
#define LDO_LATCHED_CLR			(LDO_BASE + 0x14)
#define LDO_FOLLOW_HWEN			(LDO_BASE + 0x47)
#define LDO_PBS_VOTE_CTL		(LDO_BASE + 0x49)

/* Can store pmic information up to 10 times */
#define BUF_CNT			10
#define BUF_LEN			1000
#define MAX_LEN			4096	// sysfs max file size: 4K

#define MS_DIVIDER		1000
#define NS_DIVIDER		1000000000

enum spmi_type {
	SPMI_0 = 0,
	SPMI_1,
	SPMI_4 = 4,
	SPMI_5,
};

extern struct regmap *get_globl_regmap(int usid);

/* for enable/disable manual reset, from retail group's request */
extern void do_keyboard_notifier(int onoff);

static struct device *sec_ap_pmic_dev;

struct sec_pm_debug_data {
	unsigned int pm0_smps_count;
	unsigned int pm0_ldo_count;
	unsigned int pm1_smps_count;
	unsigned int pm1_ldo_count;
};

struct pmDump {
	int pon[REGISTER_COUNT];			// 10
	int smps[REGULATOR_COUNT][REGISTER_COUNT];	// 20, 10
	int ldo[REGULATOR_COUNT][REGISTER_COUNT];	// 20, 10
};

struct secPmDump {
	int init_flag;					// Check initializing
	int front, rear;				// timestamp: ######.######
	struct pmDump pmic[REGULATOR_TYPE_COUNT];	// 2: PMIC A(0,1), C(4,5)
};

/*
 * struct sec_pm_debug_info: This will keep pmic register information
 * buf: keep each ldo status
 * full_buf: Keep last 10 ldo state
 */
struct sec_pm_debug_info {
	struct device *dev;
	struct sec_pm_debug_data *pdata;
	char name[PLATFORM_NAME_SIZE];
	struct device_node *np;

	/* Each module's register count */
	int pon_reg_cnt, smps_reg_cnt, ldo_reg_cnt;

	/* This value shows the next buffer index to be stored */
	int index;

	char full_buf[MAX_LEN];

	/* raw data */
	struct secPmDump set[DUMP_SET_COUNT];		// Store last 10 state
};
struct sec_pm_debug_info *global_info;

enum sec_pm_debug_type {
	TYPE_SPMI,
	NR_TYPE_SEC_PM_DEBUG
};

static const struct platform_device_id sec_pm_debug_id[] = {
	{"sec-pm-debug", TYPE_SPMI},
	{},
};

static const struct of_device_id sec_pm_debug_match[] = {
	{.compatible = "samsung,sec-pm-debug",
		.data = &sec_pm_debug_id[TYPE_SPMI]},
	{},
};

static int sec_ap_pmic_parse_dt(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info = platform_get_drvdata(pdev);
	struct sec_pm_debug_data *pdata;

	if (!info || !pdev->dev.of_node)
		return -ENODEV;
	info->np = pdev->dev.of_node;

	pdata = devm_kzalloc(info->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	if (of_property_read_u32(info->np, "pm0-smps-count", &pdata->pm0_smps_count)) {
		dev_err(info->dev, "failed to get pm0_smps_count\n");
		return -EINVAL;
	}
	if (of_property_read_u32(info->np, "pm0-ldo-count", &pdata->pm0_ldo_count)) {
		dev_err(info->dev, "failed to get pm0_ldo_count\n");
		return -EINVAL;
	}

	if (of_property_read_u32(info->np, "pm1-smps-count", &pdata->pm1_smps_count)) {
		dev_err(info->dev, "failed to get pm1_smps_count\n");
		return -EINVAL;
	}
	if (of_property_read_u32(info->np, "pm1-ldo-count", &pdata->pm1_ldo_count)) {
		dev_err(info->dev, "failed to get pm1_ldo_count\n");
		return -EINVAL;
	}

	info->pdata = pdata;

	pr_info("%s: %d, %d, %d, %d\n", __func__, info->pdata->pm0_smps_count, info->pdata->pm0_ldo_count,
					info->pdata->pm1_smps_count, info->pdata->pm1_ldo_count);

	return 0;
}

static int pwrsrc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	sec_get_pwrsrc(buf);
	seq_printf(m, buf);

	return 0;
}

static int pwrsrc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pwrsrc_show, NULL);
}

static const struct file_operations proc_pwrsrc_operation = {
	.open		= pwrsrc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static ssize_t manual_reset_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = qpnp_get_s2_reset_onoff();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", ret);
}

static ssize_t manual_reset_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff = 0;

	if (kstrtoint(buf, 10, &onoff))
		return -EINVAL;

	pr_info("%s: onoff(%d)\n", __func__, onoff);

	do_keyboard_notifier(onoff);
	qpnp_control_s2_reset_onoff(onoff);

	return len;
}
static DEVICE_ATTR_RW(manual_reset);


static ssize_t chg_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = qpnp_pon_check_chg_det();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", ret);
}
static DEVICE_ATTR_RO(chg_det);

static ssize_t off_reason_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", qpnp_pon_get_off_reason());
}
static DEVICE_ATTR_RO(off_reason);

static int set_pon_raw_data(struct regmap *map)
{
	int rc, i = 0;

	rc = regmap_read(map, PON_INT_RT_STS, &(global_info->set[global_info->index].pmic[0].pon[i++]));
	rc = regmap_read(map, PON_INT_LATCHED_STS, &(global_info->set[global_info->index].pmic[0].pon[i++]));
	rc = regmap_read(map, PON_INT_PENDING_STS, &(global_info->set[global_info->index].pmic[0].pon[i++]));

	return i;
}

static int set_smps_raw_data(struct regmap *map, unsigned int reg, int spmi_type, int smps_index)
{
	int rc, type, i = 0;

	type = spmi_type / 4;

	rc = regmap_read(map, SMPS_CTRL_STATUS1 + reg, &(global_info->set[global_info->index].pmic[type].smps[smps_index][i++]));
	rc = regmap_read(map, SMPS_CTRL_INT_RT_STS + reg, &(global_info->set[global_info->index].pmic[type].smps[smps_index][i++]));
	rc = regmap_read(map, SMPS_CTRL_INT_LATCHED_CLR + reg, &(global_info->set[global_info->index].pmic[type].smps[smps_index][i++]));
	rc = regmap_read(map, SMPS_CTRL_FOLLOW_HWEN + reg, &(global_info->set[global_info->index].pmic[type].smps[smps_index][i++]));

	return i;
}

static int set_ldo_raw_data(struct regmap *map, unsigned int reg, int spmi_type, int ldo_index)
{
	int rc, type, i = 0;

	type = spmi_type / 4;

	rc = regmap_read(map, LDO_STATUS1 + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));
	rc = regmap_read(map, LDO_STATUS3 + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));
	rc = regmap_read(map, LDO_STATUS4 + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));
	rc = regmap_read(map, LDO_INT_RT_STS + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));
	rc = regmap_read(map, LDO_LATCHED_CLR + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));
	rc = regmap_read(map, LDO_FOLLOW_HWEN + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));
	rc = regmap_read(map, LDO_PBS_VOTE_CTL + reg, &(global_info->set[global_info->index].pmic[type].ldo[ldo_index][i++]));

	return i;
}

char temp_buf[BUF_LEN] = {0,};
/* Just print pon and each regulator's status1 data */
static void print_brief_ap_pmic_info()
{
	int i, q_index = global_info->index - 1;
	char *buf_ptr = temp_buf;

	memset(temp_buf, 0, sizeof(temp_buf));

	/* TIME */
	buf_ptr += sprintf(buf_ptr, "DPM :TIME: %d.%06d, q_index(%d)\n", 
			global_info->set[q_index].front, global_info->set[q_index].rear, q_index);

	/* PON */
	buf_ptr += sprintf(buf_ptr, "DPM0: PON: ");
	for(i = 0; i < global_info->pon_reg_cnt; i++)
		buf_ptr += sprintf(buf_ptr, "%02x", global_info->set[q_index].pmic[0].pon[i]);
	buf_ptr += sprintf(buf_ptr, "\n");

	/** Regulator **/
	/* PM0 */
	for(i = 0; i < global_info->pdata->pm0_smps_count; i++) {
		if (i % 10 == 0)
			buf_ptr += sprintf(buf_ptr, "DPM0: S%02d: ", i + 1);
		buf_ptr += sprintf(buf_ptr, "%02x ", global_info->set[q_index].pmic[0].smps[i][0]);

		if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm0_smps_count - 1))
			buf_ptr += sprintf(buf_ptr, "\n");
	}
	for(i = 0; i < global_info->pdata->pm0_ldo_count; i++) {
		if (i % 10 == 0)
			buf_ptr += sprintf(buf_ptr, "DPM0: L%02d: ", i + 1);

		buf_ptr += sprintf(buf_ptr, "%02x ", global_info->set[q_index].pmic[0].ldo[i][0]);

		if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm0_ldo_count - 1))
			buf_ptr += sprintf(buf_ptr, "\n");
	}

	/* PM1 */
	for(i = 0; i < global_info->pdata->pm1_smps_count; i++) {
		if (i % 10 == 0)
			buf_ptr += sprintf(buf_ptr, "DPM1: S%02d: ", i + 1);

		buf_ptr += sprintf(buf_ptr, "%02x ", global_info->set[q_index].pmic[1].smps[i][0]);

		if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm1_smps_count - 1))
			buf_ptr += sprintf(buf_ptr, "\n");
	}
	for(i = 0; i < global_info->pdata->pm1_ldo_count; i++) {
		if (i % 10 == 0)
			buf_ptr += sprintf(buf_ptr, "DPM1: L%02d: ", i + 1);

		buf_ptr += sprintf(buf_ptr, "%02x ", global_info->set[q_index].pmic[1].ldo[i][0]);

		if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm1_ldo_count - 1))
			buf_ptr += sprintf(buf_ptr, "\n");
	}

	pr_info("%s\n", temp_buf);
}

/* Print last 5 ap pmic state (full) */
static void print_all_ap_pmic_info(int event, s64 timestamp)
{
	int i, j, k, q_index, cnt;
	char *buf_ptr;

	if (event == 3 || event == 4)	/* 3(Suspend), 4(Resume) */
		print_brief_ap_pmic_info();
	else if (event == 1 || event == 2) {	/* 1(ap_pmic_info1: first 5 state), 2(ap_pmic_info2: last 5 state), Ramdump/Dumpstate(Call 1, 2) */
		cnt = BUF_CNT/2;

		if (event == 1)
			q_index = (global_info->index) % BUF_CNT;
		else if (event == 2)
			q_index = (((global_info->index) % BUF_CNT) + 5) % BUF_CNT;

		memset(global_info->full_buf, 0, sizeof(global_info->full_buf));
		for (k = 0; k < cnt; k++) {
			// TEST: Remove this checking
			// Check whether raw data is initialized or not
			if (!global_info->set[q_index].init_flag) {
				q_index = (q_index + 1) % BUF_CNT;
				continue;
			}

			memset(temp_buf, 0, sizeof(temp_buf));
			buf_ptr = temp_buf;

			buf_ptr += sprintf(buf_ptr, "DPM :TIME: %d.%06d, q_index(%d)\n", 
					global_info->set[q_index].front, global_info->set[q_index].rear, q_index);

			/*** Store current ap pmic status (pon, smps, ldo) ***/
			/* PON */
			buf_ptr += sprintf(buf_ptr, "DPM0: PON: ");
			for(i = 0; i < global_info->pon_reg_cnt; i++)
				buf_ptr += sprintf(buf_ptr, "%02x", global_info->set[q_index].pmic[0].pon[i]);
			buf_ptr += sprintf(buf_ptr, "\n");

			/** Regulator **/
			/* PM0 */
			for(i = 0; i < global_info->pdata->pm0_smps_count; i++) {
				if (i % 10 == 0)
					buf_ptr += sprintf(buf_ptr, "DPM0: S%02d: ", i + 1);

				for(j = 0; j < global_info->smps_reg_cnt; j++)
					buf_ptr += sprintf(buf_ptr, "%02x", global_info->set[q_index].pmic[0].smps[i][j]);
				buf_ptr += sprintf(buf_ptr, " ");

				if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm0_smps_count - 1))
					buf_ptr += sprintf(buf_ptr, "\n");
			}

			for(i = 0; i < global_info->pdata->pm0_ldo_count; i++) {
				if (i % 10 == 0)
					buf_ptr += sprintf(buf_ptr, "DPM0: L%02d: ", i + 1);

				for(j = 0; j < global_info->ldo_reg_cnt; j++)
					buf_ptr += sprintf(buf_ptr, "%02x", global_info->set[q_index].pmic[0].ldo[i][j]);
				buf_ptr += sprintf(buf_ptr, " ");

				if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm0_ldo_count - 1))
					buf_ptr += sprintf(buf_ptr, "\n");
			}

			/* PM1 */
			for(i = 0; i < global_info->pdata->pm1_smps_count; i++) {
				if (i % 10 == 0)
					buf_ptr += sprintf(buf_ptr, "DPM1: S%02d: ", i + 1);

				for(j = 0; j < global_info->smps_reg_cnt; j++)
					buf_ptr += sprintf(buf_ptr, "%02x", global_info->set[q_index].pmic[1].smps[i][j]);
				buf_ptr += sprintf(buf_ptr, " ");

				if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm1_smps_count - 1))
					buf_ptr += sprintf(buf_ptr, "\n");
			}
			for(i = 0; i < global_info->pdata->pm1_ldo_count; i++) {
				if (i % 10 == 0)
					buf_ptr += sprintf(buf_ptr, "DPM1: L%02d: ", i + 1);

				for(j = 0; j < global_info->ldo_reg_cnt; j++)
					buf_ptr += sprintf(buf_ptr, "%02x", global_info->set[q_index].pmic[1].ldo[i][j]);
				buf_ptr += sprintf(buf_ptr, " ");

				if (((i != 0) && (((i+1) % 10) == 0)) || (i == global_info->pdata->pm1_ldo_count - 1))
					buf_ptr += sprintf(buf_ptr, "\n");
			}

			pr_info("%s\n", temp_buf);
			strcat(global_info->full_buf, temp_buf);

			q_index = (q_index + 1) % BUF_CNT;
		}
	}
}

/*
 * Event explanation
 * 3(Suspend), 4(Resume) -> Store all information, but just print LDO_STATUS1, LDO_EN_CTL
 * 0(Ramdump), 1(ap_pmic_info1: first 5 state), 2(ap_pmic_info2: last 5 state): panic, reading sysfs, dumpstate(*#9900#) -> Store and print all information
 */
char* get_ap_pmic_info(int event)
{
	struct regmap *map;
	int i;
	s64 timestamp = ktime_to_ns(ktime_get());

	if (event != 2) {
		/* Store all time, pon, smps, ldo information whenever this function is called */
		/* time */
		global_info->set[global_info->index].init_flag = 1;
		global_info->set[global_info->index].front = timestamp / NS_DIVIDER;
		global_info->set[global_info->index].rear = (timestamp % NS_DIVIDER) / MS_DIVIDER;

		/* pon */
		map = get_globl_regmap(SPMI_0);
		global_info->pon_reg_cnt = set_pon_raw_data(map);

		/* smps, ldo */
		map = get_globl_regmap(SPMI_1);
		for(i = 0; i < global_info->pdata->pm0_smps_count; i++)
			global_info->smps_reg_cnt = set_smps_raw_data(map, i * 0x100, SPMI_1, i);
		for(i = 0; i < global_info->pdata->pm0_ldo_count; i++)
			global_info->ldo_reg_cnt = set_ldo_raw_data(map, i * 0x100, SPMI_1, i);

		map = get_globl_regmap(SPMI_5);
		for(i = 0; i < global_info->pdata->pm1_smps_count; i++)
			global_info->smps_reg_cnt = set_smps_raw_data(map, i * 0x100, SPMI_5, i);
		for(i = 0; i < global_info->pdata->pm1_ldo_count; i++)
			global_info->ldo_reg_cnt = set_ldo_raw_data(map, i * 0x100, SPMI_5, i);

		/* index value should be the next index to be stored */
		global_info->index = (global_info->index + 1) % BUF_CNT;
	}

	print_all_ap_pmic_info(event, timestamp);

	return global_info->full_buf;
}
EXPORT_SYMBOL_GPL(get_ap_pmic_info);

/* 
 * check_ap_pmic_info_diff - Check difference between suspend/resume
 * 
 * This function will print the details of difference
 * Last saved buf index value ((global_info->index - 1 + BUF_CNT) % BUF_CNT)
 * It should be compared with ((global_info->index - 2 + BUF_CNT) % BUF_CNT)
 */
void check_ap_pmic_info_diff(void)
{
	int i, j, diff = 0;
	int r_idx = (global_info->index - 1 + BUF_CNT) % BUF_CNT;	// resume : last index
	int s_idx = ((global_info->index - 2 + BUF_CNT) % BUF_CNT);	// suspend: last index - 1

	/* PON */
	for(i = 0; i < global_info->pon_reg_cnt; i++) {
		if(global_info->set[r_idx].pmic[0].pon[i] != global_info->set[s_idx].pmic[0].pon[i]) {
			diff = 1;
			goto diff;
		}
	}

	/** Regulator **/
	/* PM0 */
	for(i = 0; i < global_info->pdata->pm0_smps_count; i++) {
		for(j = 0; j < global_info->smps_reg_cnt; j++) {
			if(global_info->set[r_idx].pmic[0].smps[i][j] != global_info->set[s_idx].pmic[0].smps[i][j]) {
				diff = 1;
				goto diff;
			}
		}
	}

	for(i = 0; i < global_info->pdata->pm0_ldo_count; i++) {
		for(j = 0; j < global_info->ldo_reg_cnt; j++) {
			if(global_info->set[r_idx].pmic[0].ldo[i][j] != global_info->set[s_idx].pmic[0].ldo[i][j]) {
				diff = 1;
				goto diff;
			}
		}
	}

	/* PM1 */
	for(i = 0; i < global_info->pdata->pm1_smps_count; i++) {
		for(j = 0; j < global_info->smps_reg_cnt; j++) {
			if(global_info->set[r_idx].pmic[1].smps[i][j] != global_info->set[s_idx].pmic[1].smps[i][j]) {
				diff = 1;
				goto diff;
			}
		}
	}
	for(i = 0; i < global_info->pdata->pm1_ldo_count; i++) {
		for(j = 0; j < global_info->ldo_reg_cnt; j++) {
			if(global_info->set[r_idx].pmic[1].ldo[i][j] != global_info->set[s_idx].pmic[1].ldo[i][j]) {
				diff = 1;
				goto diff;
			}
		}
	}

diff:
	if (diff == 1)
		pr_info("Please check the diff!\n");
}
EXPORT_SYMBOL_GPL(check_ap_pmic_info_diff);

static ssize_t ap_pmic_info1_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_ap_pmic_info(1));

}
static DEVICE_ATTR_RO(ap_pmic_info1);

static ssize_t ap_pmic_info2_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_ap_pmic_info(2));

}
static DEVICE_ATTR_RO(ap_pmic_info2);

static int sec_ap_pmic_panic_handler(struct notifier_block *nb,
		unsigned long action, void *data)
{
	get_ap_pmic_info(1);
	get_ap_pmic_info(2);

	return 0;
}

static struct attribute *sec_ap_pmic_attributes[] = {
	&dev_attr_chg_det.attr,
	&dev_attr_manual_reset.attr,
	&dev_attr_off_reason.attr,
	&dev_attr_ap_pmic_info1.attr,
	&dev_attr_ap_pmic_info2.attr,
	NULL,
};

static struct attribute_group sec_ap_pmic_attr_group = {
	.attrs = sec_ap_pmic_attributes,
};

static struct notifier_block sec_ap_pmic_panic_notifier = {
	.notifier_call = sec_ap_pmic_panic_handler,
};

static int sec_ap_pmic_probe(struct platform_device *pdev)
{
	int ret;
	struct sec_pm_debug_info *info;
	struct proc_dir_entry *entry;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;
	ret = sec_ap_pmic_parse_dt(pdev);

	global_info = info;
	global_info->index = 0;
	memset(global_info->set, 0, sizeof(global_info->set));
	sec_ap_pmic_dev = sec_device_create(NULL, "ap_pmic");

	if (unlikely(IS_ERR(sec_ap_pmic_dev))) {
		pr_err("%s: Failed to create ap_pmic device\n", __func__);
		ret = PTR_ERR(sec_ap_pmic_dev);
		goto err_device_create;
	}

	ret = sysfs_create_group(&sec_ap_pmic_dev->kobj,
				&sec_ap_pmic_attr_group);
	if (ret < 0) {
		pr_err("%s: Failed to create sysfs group\n", __func__);
		goto err_device_create;
	}

	/* pmic pon logging for eRR.p */
	entry = proc_create("pwrsrc", 0444, NULL, &proc_pwrsrc_operation);
	if (unlikely(!entry)) {
		ret =  -ENOMEM;
		goto err_device_create;
	}
	
	/* Register panic notifier*/
	atomic_notifier_chain_register(&panic_notifier_list, &sec_ap_pmic_panic_notifier);

	dev_info(info->dev, "%s successfully probed.\n", info->name);

	return 0;

err_device_create:
	return ret;
}

static int sec_ap_pmic_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver sec_ap_pmic_driver = {
	.driver = {
		.name = "sec-pm-debug",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_debug_match),
	},
	.probe = sec_ap_pmic_probe,
	.remove = sec_ap_pmic_remove,
};
module_platform_driver(sec_ap_pmic_driver);

MODULE_DESCRIPTION("sec_ap_pmic driver");
MODULE_AUTHOR("Jiman Cho <jiman85.cho@samsung.com");
MODULE_LICENSE("GPL");
