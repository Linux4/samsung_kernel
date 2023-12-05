// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/sysfs.h>

#include "ufs-sec-sysfs.h"

#define get_vdi_member(member) ufs_sec_features.vdi->member

/* sec specific vendor sysfs nodes */
struct device *sec_ufs_cmd_dev;

/* SEC next WB : begin */
static void ufs_sec_wb_info_backup(struct ufs_sec_wb_info *backup)
{
	SEC_UFS_WB_INFO_BACKUP(enable_cnt);
	SEC_UFS_WB_INFO_BACKUP(disable_cnt);
	SEC_UFS_WB_INFO_BACKUP(amount_kb);
	SEC_UFS_WB_INFO_BACKUP(err_cnt);

	backup->state_ts = jiffies;
}

static ssize_t ufs_sec_wb_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ufs_sec_wb_info *wb_info_backup = ufs_sec_features.ufs_wb_backup;
	struct ufs_sec_wb_info *wb_info = ufs_sec_features.ufs_wb;
	long hours = 0;
	int len = 0;

	wb_info->state_ts = jiffies;
	hours = jiffies_to_msecs(wb_info->state_ts - wb_info_backup->state_ts) / 1000;	/* sec */
	hours = (hours + 60) / (60 * 60);	/* round up to hours */

	len = sprintf(buf, "\"TWCTRLCNT\":\"%llu\","
			"\"TWCTRLERRCNT\":\"%llu\","
			"\"TWDAILYMB\":\"%llu\","
			"\"TWTOTALMB\":\"%llu\","
			"\"TWhours\":\"%ld\"\n",
			(wb_info->enable_cnt + wb_info->disable_cnt),
			wb_info->err_cnt,    /* total error count */
			(wb_info->amount_kb >> 10),         /* WB write daily : MB */
			(wb_info_backup->amount_kb >> 10),    /* WB write total : MB */
			hours);

	ufs_sec_wb_info_backup(wb_info_backup);
	return len;
}
static DEVICE_ATTR(SEC_UFS_TW_info, 0444, ufs_sec_wb_info_show, NULL);
/* SEC next WB : end */

/* UFS info nodes : begin */
static ssize_t ufs_sec_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", get_vdi_member(unique_number));
}
static DEVICE_ATTR(un, 0440, ufs_sec_unique_number_show, NULL);

static ssize_t ufs_sec_lt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);
	if (!hba) {
		dev_err(dev, "skipping ufs lt read\n");
		get_vdi_member(lt) = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(&hba->sdev_ufs_device->sdev_gendev);
		ufs_sec_get_health_desc(hba);
		pm_runtime_put(&hba->sdev_ufs_device->sdev_gendev);
	} else {
		/* return previous LT value if not operational */
		dev_info(hba->dev, "ufshcd_state : %d, old LT: %01x\n",
				hba->ufshcd_state, get_vdi_member(lt));
	}
	return snprintf(buf, PAGE_SIZE, "%01x\n", get_vdi_member(lt));
}
static DEVICE_ATTR(lt, 0444, ufs_sec_lt_show, NULL);

static ssize_t ufs_sec_flt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);

	if (!hba) {
		dev_err(dev, "skipping ufs flt read\n");
		get_vdi_member(flt) = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(&hba->sdev_ufs_device->sdev_gendev);
		ufs_sec_get_health_desc(hba);
		pm_runtime_put(&hba->sdev_ufs_device->sdev_gendev);
	} else {
		/* return previous FLT value if not operational */
		dev_info(hba->dev, "ufshcd_state : %d, old FLT: %u\n",
				hba->ufshcd_state, get_vdi_member(flt));
	}
	return snprintf(buf, PAGE_SIZE, "%u\n", get_vdi_member(flt));
}
static DEVICE_ATTR(flt, 0444, ufs_sec_flt_show, NULL);

static ssize_t ufs_sec_lc_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", get_vdi_member(lc));
}

static ssize_t ufs_sec_lc_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	get_vdi_member(lc) = value;

	return count;
}
static DEVICE_ATTR(lc, 0664, ufs_sec_lc_info_show, ufs_sec_lc_info_store);

static ssize_t ufs_sec_hist_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return SEC_UFS_ERR_HIST_SUM(buf);
}

static bool is_valid_hist_info(const char *buf, size_t count)
{
	int i;

	if (count != ERR_SUM_SIZE)
		return false;

	if (buf[0] != 'U' || buf[2] != 'I' || buf[4] != 'H' ||
	    buf[6] != 'L' || buf[8] != 'X' || buf[10] != 'Q' ||
	    buf[12] != 'R' || buf[14] != 'W' || buf[16] != 'F' ||
	    buf[18] != 'S' || buf[19] != 'M' || buf[21] != 'S' ||
	    buf[22] != 'H')
		return false;

	for (i = 1; i < ERR_SUM_SIZE; i += 2) {
		if (buf[i] - '0' < 0 || buf[i] - '0' >= 10)
			return false;
		/* increase index for "SM", "SH" */
		if (i == 17 || i == 20)
			i++;
	}

	return true;
}

static ssize_t ufs_sec_hist_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (!is_valid_hist_info(buf, count)) {
		pr_err("%s: %s, len(%lu)\n", __func__, buf, count);
		return -EINVAL;
	}

	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_cnt, UTP_err, buf[1]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UIC_err_cnt, UIC_err, buf[3]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(op_cnt, HW_RESET_cnt, buf[5]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(op_cnt, link_startup_cnt, buf[7]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(Fatal_err_cnt, LLE, buf[9]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_cnt, UTMR_query_task_cnt, buf[11]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_cnt, UTR_read_err, buf[13]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_cnt, UTR_write_err, buf[15]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(Fatal_err_cnt, DFE, buf[17]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(sense_cnt, scsi_medium_err, buf[20]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(sense_cnt, scsi_hw_err, buf[23]);

	return count;
}
static DEVICE_ATTR(hist, 0664, ufs_sec_hist_info_show, ufs_sec_hist_info_store);

static ssize_t ufs_sec_man_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);
	if (!hba) {
		dev_err(dev, "skipping ufs manid read\n");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%04x\n", hba->dev_info.wmanufacturerid);
}
static DEVICE_ATTR(man_id, 0444, ufs_sec_man_id_show, NULL);

static struct attribute *sec_ufs_info_attributes[] = {
	&dev_attr_un.attr,
	&dev_attr_lt.attr,
	&dev_attr_lc.attr,
	&dev_attr_man_id.attr,
	&dev_attr_flt.attr,
	&dev_attr_hist.attr,
	NULL
};

static struct attribute_group sec_ufs_info_attribute_group = {
	.attrs	= sec_ufs_info_attributes,
};

static void ufs_sec_create_info_sysfs(struct ufs_hba *hba)
{
	int ret = 0;

	ret = sysfs_create_group(&sec_ufs_cmd_dev->kobj,
			&sec_ufs_info_attribute_group);
	if (ret)
		dev_err(hba->dev, "%s: Failed to create sec_ufs_info sysfs group (err = %d)\n",
				__func__, ret);
}
/* UFS info nodes : end */

/* SEC error info : begin */
static ssize_t SEC_UFS_op_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(op_cnt, HW_RESET_cnt);
	SEC_UFS_ERR_INFO_BACKUP(op_cnt, link_startup_cnt);
	SEC_UFS_ERR_INFO_BACKUP(op_cnt, Hibern8_enter_cnt);
	SEC_UFS_ERR_INFO_BACKUP(op_cnt, Hibern8_exit_cnt);

	return count;
}

static ssize_t SEC_UFS_uic_cmd_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_TEST_MODE_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_GET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_SET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_PEER_GET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_PEER_SET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_POWERON_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_POWEROFF_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_ENABLE_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_RESET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_END_PT_RST_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_LINK_STARTUP_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_HIBER_ENTER_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_HIBER_EXIT_err);

	return count;
}

static ssize_t SEC_UFS_uic_err_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, PAERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DL_PA_INIT_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DL_NAC_RCVD_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DL_TC_REPLAY_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, NLERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, TLERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DMEERR_cnt);

	return count;
}

static ssize_t SEC_UFS_fatal_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, DFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, CFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, SBFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, CEFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, LLE);

	return count;
}

static ssize_t SEC_UFS_utp_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTMR_query_task_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTMR_abort_task_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_read_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_write_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_sync_cache_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_unmap_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_etc_err);

	return count;
}

static ssize_t SEC_UFS_query_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, NOP_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, R_Desc_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, W_Desc_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, R_Attr_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, W_Attr_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, R_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Set_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Clear_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Toggle_Flag_err);

	return count;
}

static ssize_t SEC_UFS_err_sum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(op_cnt, op_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, UIC_cmd_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, UIC_err);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, Fatal_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTP_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Query_err);

	return count;
}

static ssize_t sense_err_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(sense_cnt, scsi_medium_err);
	SEC_UFS_ERR_INFO_BACKUP(sense_cnt, scsi_hw_err);

	return count;
}

SEC_UFS_DATA_ATTR_RW(SEC_UFS_op_cnt, "\"HWRESET\":\"%u\",\"LINKFAIL\":\"%u\""
		",\"H8ENTERFAIL\":\"%u\",\"H8EXITFAIL\":\"%u\"\n",
		get_err_member(op_cnt).HW_RESET_cnt,
		get_err_member(op_cnt).link_startup_cnt,
		get_err_member(op_cnt).Hibern8_enter_cnt,
		get_err_member(op_cnt).Hibern8_exit_cnt);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_uic_cmd_cnt, "\"TESTMODE\":\"%u\""
		",\"DME_GET\":\"%u\",\"DME_SET\":\"%u\",\"DME_PGET\":\"%u\""
		",\"DME_PSET\":\"%u\",\"PWRON\":\"%u\",\"PWROFF\":\"%u\""
		",\"DME_EN\":\"%u\",\"DME_RST\":\"%u\",\"EPRST\":\"%u\""
		",\"LINKSTARTUP\":\"%u\",\"H8ENTER\":\"%u\""
		",\"H8EXIT\":\"%u\"\n",
		get_err_member(UIC_cmd_cnt).DME_TEST_MODE_err,
		get_err_member(UIC_cmd_cnt).DME_GET_err,
		get_err_member(UIC_cmd_cnt).DME_SET_err,
		get_err_member(UIC_cmd_cnt).DME_PEER_GET_err,
		get_err_member(UIC_cmd_cnt).DME_PEER_SET_err,
		get_err_member(UIC_cmd_cnt).DME_POWERON_err,
		get_err_member(UIC_cmd_cnt).DME_POWEROFF_err,
		get_err_member(UIC_cmd_cnt).DME_ENABLE_err,
		get_err_member(UIC_cmd_cnt).DME_RESET_err,
		get_err_member(UIC_cmd_cnt).DME_END_PT_RST_err,
		get_err_member(UIC_cmd_cnt).DME_LINK_STARTUP_err,
		get_err_member(UIC_cmd_cnt).DME_HIBER_ENTER_err,
		get_err_member(UIC_cmd_cnt).DME_HIBER_EXIT_err);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_uic_err_cnt, "\"PAERR\":\"%u\""
		",\"DLPAINITERROR\":\"%u\",\"DLNAC\":\"%u\""
		",\"DLTCREPLAY\":\"%u\",\"NLERR\":\"%u\",\"TLERR\":\"%u\""
		",\"DMEERR\":\"%u\"\n",
		get_err_member(UIC_err_cnt).PAERR_cnt,
		get_err_member(UIC_err_cnt).DL_PA_INIT_ERR_cnt,
		get_err_member(UIC_err_cnt).DL_NAC_RCVD_ERR_cnt,
		get_err_member(UIC_err_cnt).DL_TC_REPLAY_ERR_cnt,
		get_err_member(UIC_err_cnt).NLERR_cnt,
		get_err_member(UIC_err_cnt).TLERR_cnt,
		get_err_member(UIC_err_cnt).DMEERR_cnt);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_fatal_cnt, "\"DFE\":\"%u\",\"CFE\":\"%u\""
		",\"SBFE\":\"%u\",\"CEFE\":\"%u\",\"LLE\":\"%u\"\n",
		get_err_member(Fatal_err_cnt).DFE,
		get_err_member(Fatal_err_cnt).CFE,
		get_err_member(Fatal_err_cnt).SBFE,
		get_err_member(Fatal_err_cnt).CEFE,
		get_err_member(Fatal_err_cnt).LLE);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_utp_cnt, "\"UTMRQTASK\":\"%u\""
		",\"UTMRATASK\":\"%u\",\"UTRR\":\"%u\",\"UTRW\":\"%u\""
		",\"UTRSYNCCACHE\":\"%u\",\"UTRUNMAP\":\"%u\""
		",\"UTRETC\":\"%u\"\n",
		get_err_member(UTP_cnt).UTMR_query_task_cnt,
		get_err_member(UTP_cnt).UTMR_abort_task_cnt,
		get_err_member(UTP_cnt).UTR_read_err,
		get_err_member(UTP_cnt).UTR_write_err,
		get_err_member(UTP_cnt).UTR_sync_cache_err,
		get_err_member(UTP_cnt).UTR_unmap_err,
		get_err_member(UTP_cnt).UTR_etc_err);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_query_cnt, "\"NOPERR\":\"%u\",\"R_DESC\":\"%u\""
		",\"W_DESC\":\"%u\",\"R_ATTR\":\"%u\",\"W_ATTR\":\"%u\""
		",\"R_FLAG\":\"%u\",\"S_FLAG\":\"%u\",\"C_FLAG\":\"%u\""
		",\"T_FLAG\":\"%u\"\n",
		get_err_member(Query_cnt).NOP_err,
		get_err_member(Query_cnt).R_Desc_err,
		get_err_member(Query_cnt).W_Desc_err,
		get_err_member(Query_cnt).R_Attr_err,
		get_err_member(Query_cnt).W_Attr_err,
		get_err_member(Query_cnt).R_Flag_err,
		get_err_member(Query_cnt).Set_Flag_err,
		get_err_member(Query_cnt).Clear_Flag_err,
		get_err_member(Query_cnt).Toggle_Flag_err);

/* daily err sum */
SEC_UFS_DATA_ATTR_RW(SEC_UFS_err_sum, "\"OPERR\":\"%u\",\"UICCMD\":\"%u\""
		",\"UICERR\":\"%u\",\"FATALERR\":\"%u\",\"UTPERR\":\"%u\""
		",\"QUERYERR\":\"%u\"\n",
		get_err_member(op_cnt).op_err,
		get_err_member(UIC_cmd_cnt).UIC_cmd_err,
		get_err_member(UIC_err_cnt).UIC_err,
		get_err_member(Fatal_err_cnt).Fatal_err,
		get_err_member(UTP_cnt).UTP_err,
		get_err_member(Query_cnt).Query_err);

SEC_UFS_DATA_ATTR_RW(sense_err_count, "\"MEDIUM\":\"%u\",\"HWERR\":\"%u\"\n",
		get_err_member(sense_cnt).scsi_medium_err,
		get_err_member(sense_cnt).scsi_hw_err);

SEC_UFS_DATA_ATTR_RO(sense_err_logging, "\"LBA0\":\"%lx\",\"LBA1\":\"%lx\""
		",\"LBA2\":\"%lx\",\"LBA3\":\"%lx\",\"LBA4\":\"%lx\""
		",\"LBA5\":\"%lx\",\"LBA6\":\"%lx\",\"LBA7\":\"%lx\""
		",\"LBA8\":\"%lx\",\"LBA9\":\"%lx\""
		",\"REGIONMAP\":\"%016llx\"\n",
		get_err_member(sense_err_log).issue_LBA_list[0],
		get_err_member(sense_err_log).issue_LBA_list[1],
		get_err_member(sense_err_log).issue_LBA_list[2],
		get_err_member(sense_err_log).issue_LBA_list[3],
		get_err_member(sense_err_log).issue_LBA_list[4],
		get_err_member(sense_err_log).issue_LBA_list[5],
		get_err_member(sense_err_log).issue_LBA_list[6],
		get_err_member(sense_err_log).issue_LBA_list[7],
		get_err_member(sense_err_log).issue_LBA_list[8],
		get_err_member(sense_err_log).issue_LBA_list[9],
		get_err_member(sense_err_log).issue_region_map);

/* accumulated err sum */
SEC_UFS_DATA_ATTR_RO(SEC_UFS_err_summary,
		"OPERR : %u, UICCMD : %u, UICERR : %u, FATALERR : %u"
		", UTPERR : %u, QUERYERR : %u\n"
		"MEDIUM : %u, HWERR : %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(op_cnt, op_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_cmd_cnt, UIC_cmd_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_err_cnt, UIC_err),
		SEC_UFS_ERR_INFO_GET_VALUE(Fatal_err_cnt, Fatal_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UTP_cnt, UTP_err),
		SEC_UFS_ERR_INFO_GET_VALUE(Query_cnt, Query_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_cnt, scsi_medium_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_cnt, scsi_hw_err));

static struct attribute *sec_ufs_error_attributes[] = {
	&dev_attr_SEC_UFS_op_cnt.attr,
	&dev_attr_SEC_UFS_uic_cmd_cnt.attr,
	&dev_attr_SEC_UFS_uic_err_cnt.attr,
	&dev_attr_SEC_UFS_fatal_cnt.attr,
	&dev_attr_SEC_UFS_utp_cnt.attr,
	&dev_attr_SEC_UFS_query_cnt.attr,
	&dev_attr_SEC_UFS_err_sum.attr,
	&dev_attr_sense_err_count.attr,
	&dev_attr_sense_err_logging.attr,
	&dev_attr_SEC_UFS_err_summary.attr,
	&dev_attr_SEC_UFS_TW_info.attr,
	NULL
};

static struct attribute_group sec_ufs_error_attribute_group = {
	.attrs	= sec_ufs_error_attributes,
};

static void ufs_sec_create_err_info_sysfs(struct ufs_hba *hba)
{
	struct device *shost_dev = &(hba->host->shost_dev);
	int ret = 0;

	ret = sysfs_create_group(&shost_dev->kobj,
			&sec_ufs_error_attribute_group);
	if (ret)
		dev_err(hba->dev, "%s: cannot create sec err sysfs err: %d\n",
				__func__, ret);

}
/* SEC error info : end */

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
/* SEC cmd log : begin */
static ssize_t ufs_sec_cmd_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log =
		ufs_sec_features.ufs_cmd_log;
	struct ufs_sec_cmd_log_entry *entry = NULL;
	int i = (ufs_cmd_log->pos + UFS_SEC_CMD_LOGGING_MAX
			- UFS_SEC_CMD_LOGNODE_MAX);
	int idx = 0;
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"%02s: %10s: %2s %3s %4s %9s %6s %16s\n",
			"No", "log string", "lu", "tag",
			"c_id", "lba", "length", "time");

	for (idx = 0; idx < UFS_SEC_CMD_LOGNODE_MAX; idx++, i++) {
		i %= UFS_SEC_CMD_LOGGING_MAX;
		entry = &ufs_cmd_log->entries[i];
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%2d: %10s: %2d %3d 0x%02x %9u %6d %16llu\n",
				idx,
				entry->str, entry->lun, entry->tag,
				entry->cmd_id, entry->lba,
				entry->transfer_len, entry->tstamp);
	}

	return len;
}
static DEVICE_ATTR(cmd_log, 0440, ufs_sec_cmd_log_show, NULL);

static struct attribute *sec_ufs_cmd_log_attributes[] = {
	&dev_attr_cmd_log.attr,
	NULL
};

static struct attribute_group sec_ufs_cmd_log_attribute_group = {
	.attrs	= sec_ufs_cmd_log_attributes,
};

static void ufs_sec_create_cmd_log_sysfs(struct ufs_hba *hba)
{
	int ret = 0;

	ret = sysfs_create_group(&sec_ufs_cmd_dev->kobj,
			&sec_ufs_cmd_log_attribute_group);
	if (ret)
		dev_err(hba->dev, "%s: Failed to create sec_ufs_cmd_log sysfs "
				"group (err = %d)\n",
				__func__, ret);

}
/* SEC cmd log : end */
#endif

static int ufs_sec_create_sysfs_dev(struct ufs_hba *hba)
{
	/* sec specific vendor sysfs nodes */
	if (!sec_ufs_cmd_dev)
		sec_ufs_cmd_dev = sec_device_create(hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev)) {
		pr_err("Fail to create sysfs dev\n");
		return -ENODEV;
	}

	return 0;
}

void ufs_sec_add_sysfs_nodes(struct ufs_hba *hba)
{
	if (ufs_sec_is_err_cnt_allowed())
		ufs_sec_create_err_info_sysfs(hba);

	if (!ufs_sec_create_sysfs_dev(hba)) {
		/* sec specific vendor sysfs nodes */
		ufs_sec_create_info_sysfs(hba);

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		if (ufs_sec_is_cmd_log_allowed())
			ufs_sec_create_cmd_log_sysfs(hba);
#endif
	}
}

void ufs_sec_remove_sysfs_nodes(struct ufs_hba *hba)
{
	if (sec_ufs_cmd_dev)
		sysfs_remove_group(&sec_ufs_cmd_dev->kobj,
				&sec_ufs_info_attribute_group);
}

