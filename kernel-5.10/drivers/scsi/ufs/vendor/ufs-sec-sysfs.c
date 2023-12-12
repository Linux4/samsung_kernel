// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include "ufs-sec-sysfs.h"
#include <linux/sysfs.h>

#define get_vdi_member(member) ufs_sec_features.vdi->member

/* sec specific vendor sysfs nodes */
static struct device *sec_ufs_cmd_dev;

static ssize_t ufs_sec_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", get_vdi_member(unique_number));
}
static DEVICE_ATTR(un, 0440, ufs_sec_unique_number_show, NULL);

static ssize_t ufs_sec_man_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = get_vdi_member(hba);

	if (!hba) {
		dev_err(dev, "skipping ufs manid read\n");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%04x\n", hba->dev_info.wmanufacturerid);
}
static DEVICE_ATTR(man_id, 0444, ufs_sec_man_id_show, NULL);

static ssize_t ufs_sec_lt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = get_vdi_member(hba);

	if (!hba) {
		dev_err(dev, "skipping ufs lt read\n");
		get_vdi_member(lifetime) = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(hba->dev);
		ufs_sec_get_health_desc(hba);
		pm_runtime_put(hba->dev);
	} else {
		/* return previous LT value if not operational */
		dev_info(hba->dev, "ufshcd_state : %d, old LT: %01x\n",
				hba->ufshcd_state, get_vdi_member(lifetime));
	}

	return snprintf(buf, PAGE_SIZE, "%01x\n", get_vdi_member(lifetime));
}
static DEVICE_ATTR(lt, 0444, ufs_sec_lt_show, NULL);

/* SEC error info : begin */
SEC_UFS_DATA_ATTR_RO(SEC_UFS_op_cnt, "\"HWRESET\":\"%u\",\"LINKFAIL\":\"%u\",\"H8ENTERFAIL\":\"%u\""
		",\"H8EXITFAIL\":\"%u\"\n",
		get_err_member(op_count).HW_RESET_count,
		get_err_member(op_count).link_startup_count,
		get_err_member(op_count).Hibern8_enter_count,
		get_err_member(op_count).Hibern8_exit_count);

SEC_UFS_DATA_ATTR_RO(SEC_UFS_uic_cmd_cnt, "\"TESTMODE\":\"%u\",\"DME_GET\":\"%u\",\"DME_SET\":\"%u\""
		",\"DME_PGET\":\"%u\",\"DME_PSET\":\"%u\",\"PWRON\":\"%u\",\"PWROFF\":\"%u\""
		",\"DME_EN\":\"%u\",\"DME_RST\":\"%u\",\"EPRST\":\"%u\",\"LINKSTARTUP\":\"%u\""
		",\"H8ENTER\":\"%u\",\"H8EXIT\":\"%u\"\n",
		get_err_member(UIC_cmd_count).DME_TEST_MODE_err,
		get_err_member(UIC_cmd_count).DME_GET_err,
		get_err_member(UIC_cmd_count).DME_SET_err,
		get_err_member(UIC_cmd_count).DME_PEER_GET_err,
		get_err_member(UIC_cmd_count).DME_PEER_SET_err,
		get_err_member(UIC_cmd_count).DME_POWERON_err,
		get_err_member(UIC_cmd_count).DME_POWEROFF_err,
		get_err_member(UIC_cmd_count).DME_ENABLE_err,
		get_err_member(UIC_cmd_count).DME_RESET_err,
		get_err_member(UIC_cmd_count).DME_END_PT_RST_err,
		get_err_member(UIC_cmd_count).DME_LINK_STARTUP_err,
		get_err_member(UIC_cmd_count).DME_HIBER_ENTER_err,
		get_err_member(UIC_cmd_count).DME_HIBER_EXIT_err);

SEC_UFS_DATA_ATTR_RO(SEC_UFS_uic_err_cnt, "\"PAERR\":\"%u\",\"DLPAINITERROR\":\"%u\",\"DLNAC\":\"%u\""
		",\"DLTCREPLAY\":\"%u\",\"NLERR\":\"%u\",\"TLERR\":\"%u\",\"DMEERR\":\"%u\"\n",
		get_err_member(UIC_err_count).PA_ERR_cnt,
		get_err_member(UIC_err_count).DL_PA_INIT_ERROR_cnt,
		get_err_member(UIC_err_count).DL_NAC_RECEIVED_ERROR_cnt,
		get_err_member(UIC_err_count).DL_TC_REPLAY_ERROR_cnt,
		get_err_member(UIC_err_count).NL_ERROR_cnt,
		get_err_member(UIC_err_count).TL_ERROR_cnt,
		get_err_member(UIC_err_count).DME_ERROR_cnt);

SEC_UFS_DATA_ATTR_RO(SEC_UFS_fatal_cnt, "\"DFE\":\"%u\",\"CFE\":\"%u\",\"SBFE\":\"%u\""
		",\"CEFE\":\"%u\",\"LLE\":\"%u\"\n",
		get_err_member(Fatal_err_count).DFE,
		get_err_member(Fatal_err_count).CFE,
		get_err_member(Fatal_err_count).SBFE,
		get_err_member(Fatal_err_count).CEFE,
		get_err_member(Fatal_err_count).LLE);

SEC_UFS_DATA_ATTR_RO(SEC_UFS_utp_cnt, "\"UTMRQTASK\":\"%u\",\"UTMRATASK\":\"%u\",\"UTRR\":\"%u\""
		",\"UTRW\":\"%u\",\"UTRSYNCCACHE\":\"%u\",\"UTRUNMAP\":\"%u\",\"UTRETC\":\"%u\"\n",
		get_err_member(UTP_count).UTMR_query_task_count,
		get_err_member(UTP_count).UTMR_abort_task_count,
		get_err_member(UTP_count).UTR_read_err,
		get_err_member(UTP_count).UTR_write_err,
		get_err_member(UTP_count).UTR_sync_cache_err,
		get_err_member(UTP_count).UTR_unmap_err,
		get_err_member(UTP_count).UTR_etc_err);

SEC_UFS_DATA_ATTR_RO(SEC_UFS_query_cnt, "\"NOPERR\":\"%u\",\"R_DESC\":\"%u\",\"W_DESC\":\"%u\""
		",\"R_ATTR\":\"%u\",\"W_ATTR\":\"%u\",\"R_FLAG\":\"%u\",\"S_FLAG\":\"%u\""
		",\"C_FLAG\":\"%u\",\"T_FLAG\":\"%u\"\n",
		get_err_member(Query_count).NOP_err,
		get_err_member(Query_count).R_Desc_err,
		get_err_member(Query_count).W_Desc_err,
		get_err_member(Query_count).R_Attr_err,
		get_err_member(Query_count).W_Attr_err,
		get_err_member(Query_count).R_Flag_err,
		get_err_member(Query_count).Set_Flag_err,
		get_err_member(Query_count).Clear_Flag_err,
		get_err_member(Query_count).Toggle_Flag_err);

SEC_UFS_DATA_ATTR_RO(SEC_UFS_sense_cnt, "\"MEDIUM\":\"%u\",\"HWERR\":\"%u\"\n",
		get_err_member(Sense_count).scsi_medium_err,
		get_err_member(Sense_count).scsi_hw_err);

/* accumulated err sum */
SEC_UFS_DATA_ATTR_RO(SEC_UFS_err_summary,
		"OPERR : %u, UICCMD : %u, UICERR : %u, FATALERR : %u, UTPERR : %u, QUERYERR : %u\n"
		"MEDIUM : %u, HWERR : %u\n",
		get_err_member(op_count).op_err,
		get_err_member(UIC_cmd_count).UIC_cmd_err,
		get_err_member(UIC_err_count).UIC_err,
		get_err_member(Fatal_err_count).Fatal_err,
		get_err_member(UTP_count).UTP_err,
		get_err_member(Query_count).Query_err,
		get_err_member(Sense_count).scsi_medium_err,
		get_err_member(Sense_count).scsi_hw_err);
/* SEC error info : end */

static struct attribute *sec_ufs_info_attributes[] = {
	&dev_attr_un.attr,
	&dev_attr_lt.attr,
	&dev_attr_man_id.attr,
	NULL
};

static struct attribute *sec_ufs_error_attributes[] = {
	&dev_attr_SEC_UFS_op_cnt.attr,
	&dev_attr_SEC_UFS_uic_cmd_cnt.attr,
	&dev_attr_SEC_UFS_uic_err_cnt.attr,
	&dev_attr_SEC_UFS_fatal_cnt.attr,
	&dev_attr_SEC_UFS_utp_cnt.attr,
	&dev_attr_SEC_UFS_query_cnt.attr,
	&dev_attr_SEC_UFS_sense_cnt.attr,
	&dev_attr_SEC_UFS_err_summary.attr,
	NULL
};

static struct attribute_group sec_ufs_info_attribute_group = {
	.attrs	= sec_ufs_info_attributes,
};

static struct attribute_group sec_ufs_error_attribute_group = {
	.attrs	= sec_ufs_error_attributes,
};

static void ufs_sec_create_info_sysfs(struct ufs_hba *hba)
{
	int ret = 0;

	/* sec specific vendor sysfs nodes */
	if (!sec_ufs_cmd_dev)
		sec_ufs_cmd_dev = sec_device_create(hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev))
		pr_err("Fail to create sysfs dev\n");
	else {
		ret = sysfs_create_group(&sec_ufs_cmd_dev->kobj,
				&sec_ufs_info_attribute_group);
		if (ret)
			dev_err(hba->dev, "%s: Failed to create sec_ufs_info sysfs group, %d\n",
					__func__, ret);
	}
}

static void ufs_sec_create_err_info_sysfs(struct ufs_hba *hba)
{
	int ret = 0;
	struct device *dev = &(hba->host->shost_dev);

	/* scsi_host sysfs nodes */
	ret = sysfs_create_group(&dev->kobj, &sec_ufs_error_attribute_group);
	if (ret)
		pr_err("cannot create sec error sysfs group err: %d\n", ret);
}

static int ufs_sec_create_sysfs_dev(struct ufs_hba *hba)
{
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
	if (!ufs_sec_create_sysfs_dev(hba))
		ufs_sec_create_info_sysfs(hba);

	if (ufs_sec_is_err_cnt_allowed())
		ufs_sec_create_err_info_sysfs(hba);
}

void ufs_sec_remove_sysfs_nodes(struct ufs_hba *hba)
{
	if (sec_ufs_cmd_dev)
		sysfs_remove_group(&sec_ufs_cmd_dev->kobj,
				&sec_ufs_info_attribute_group);
}
MODULE_LICENSE("GPL v2");
